#include "champions/script_commands.hpp"
#include "champions/quests_bridge.hpp"
#include "champions/session.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace champions::simulation::rta::script_commands {
namespace {
const Row kRows[COMMANDS] = {
#include "script_commands_table.inc"
};
// Binder deferred words (script_builtins.cpp).
constexpr std::uint32_t DEFER_COUNT = 0x86da78, DEFER_QUEUED = 0x86da7c,
                         DEFER_FLIP = 0x4f8690;
constexpr std::uint32_t SEED_WORD = 0x404464, ENTRANCE_WORD = 0x4f8468;
// missionMenu_run (research t3/research-quests docs/quests.md section 4):
// g_paused, the pending level name and its orientation byte.
constexpr std::uint32_t PAUSED_WORD = 0x41b4f4, PENDING_WORD = 0x41b6d0, ORIENT_WORD = 0x41b720;

// Documented commands whose binder path reads, on the objects its name
// argument finds, the creature flag 0x20, hit points at +0x228 or the type
// name (script_builtins.cpp creature_dispatch and menu_dispatch; research
// 9649e6b tools/ref_natives.py n_<name>). limit is the binder's lookup limit;
// setHitPoints looks up the first object only, which is limit 2.
struct StubRead {
  const char *name;
  std::int64_t limit;
};
const StubRead kStubReads[] = {
    {"increaseEnemyRank", 0x200}, {"unRootMonster", 0x200}, {"stopFollowingPlayer", 0x200},
    {"friendly", 0x200},          {"unFriendly", 0x200},    {"hideMonster", 0x200},
    {"showMonster", 0x200},       {"getHitPoints", 0x200},  {"setHitPoints", 2},
    {"countLiveObjects", 0x80},   {"showOnMap", 100},
    // setCFlag tests the creature flag 0x20 on each object it finds
    // (research-quests 7efd3ce tools/ref_natives.py n_setCFlag).
    {"setCFlag", 0x200},
};
const StubRead *stub_read(const char *name) {
  for (const auto &read : kStubReads)
    if (std::strcmp(read.name, name) == 0)
      return &read;
  return nullptr;
}

// Opcode semantics: research 9649e6b docs/script.md section 1 (the opcode
// table). After a native returns, PRI holds its answer. The answer counts as
// discarded only when straight-line code sets PRI without reading it before
// anything reads PRI or transfers control; everything else is a use.
bool keeps_pri(amx::Cell op) {
  switch (op) {
  case 0x02: case 0x04: case 0x06: case 0x08: // LOAD.alt LOAD.S.alt LREF.alt LREF.S.alt
  case 0x0c: case 0x0e:                       // CONST.alt ADDR.alt
  case 0x10: case 0x12: case 0x14: case 0x16: // STOR.alt STOR.S.alt SREF.alt SREF.S.alt
  case 0x25: case 0x27: case 0x28: case 0x29: // PUSH.alt PUSH.C PUSH PUSH.S
  case 0x2b: case 0x2c: case 0x2d:            // POP.alt STACK HEAP
  case 0x5a: case 0x5b: case 0x5c:            // ZERO.alt ZERO ZERO.S
  case 0x7c: case 0x7d: case 0x7e: case 0x7f: // FILE LINE SYMBOL SRANGE
    return true;
  default:
    return false;
  }
}
bool sets_pri(amx::Cell op) {
  switch (op) {
  case 0x01: case 0x03: case 0x05: case 0x07: // LOAD.pri LOAD.S.pri LREF.pri LREF.S.pri
  case 0x0b: case 0x0d: case 0x1f:            // CONST.pri ADDR.pri LCTRL
  case 0x21: case 0x2a: case 0x59:            // MOVE.pri POP.pri ZERO.pri
  case 0x7b:                                  // SYSREQ.C: PRI = the next native's answer
    return true;
  default:
    return false;
  }
}
// The machine stores the address after the SYSREQ in cip before it calls a
// native (amx_execute.cpp), so the scan starts there.
bool result_used(const amx::Machine &machine) {
  const auto code = machine.decoded;
  if (!code)
    return true;
  amx::Address at = machine.cip;
  for (int step = 0; step < 64; ++step) {
    const auto found = code->find(at);
    if (found == code->end())
      return true;
    const auto op = found->second.opcode;
    if (sets_pri(op))
      return false;
    if (!keeps_pri(op))
      return true;
    at = found->second.next;
  }
  return true;
}

std::uint32_t word_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *word = std::get_if<std::uint32_t>(&value))
    return *word;
  throw amx::HostFault("scene value is not a word");
}
std::array<std::uint32_t, 3> vec_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *vec = std::get_if<std::array<std::uint32_t, 3>>(&value))
    return *vec;
  throw amx::HostFault("scene value is not a vector");
}
bool campaign_command(const char *name) {
  static const char *names[] = {
      "addQuest", "removeQuest", "getv", "setv", "addv", "setMissionAvailable",
      "getMissionAvailable", "grantMission", "getMissionComplete", "setEvil", "getEvil",
      "setMadeDecision", "getMadeDecision", "grantGold", "grantSilver", "grantBronze",
      "getMedal", "setPlayerFlag", "getPlayerFlag", "givePlayerGold", "givePlayerExp",
      "activateMissionMenu", "setCFlag"};
  for (const char *each : names)
    if (std::strcmp(name, each) == 0)
      return true;
  return false;
}
} // namespace

const Row &row(std::size_t index) {
  if (index >= COMMANDS)
    throw std::out_of_range("script command index");
  return kRows[index];
}

Host::Host()
    : variables_(std::make_unique<builtins::Variables>()),
      campaign_(std::make_unique<quests::Campaign>()),
      effects_(std::make_unique<champions::rta::fx::sim::EffectWorld>()) {
  install();
  bind_campaign();
}
Host::~Host() = default;

void Host::bind_campaign() {
  auto scene = variables_->scene_service;
  variables_->scene_service = [this, scene](const std::string &name,
                                            const std::vector<builtins::Variables::ServiceValue> &args)
      -> std::uint32_t {
    if (name == "creature_startBossMusic") {
      campaign_->note_boss_music();
      return 0;
    }
    // This host has no dialog machine. The open proceeds when that call is 0.
    if (name == "dialog_isOpen")
      return 0;
    if (name == "missionMenu_open") {
      const auto slot = args.empty() ? 0u : word_of(args[0]);
      campaign_->open_menu(static_cast<int>(slot));
      return 0;
    }
    if (scene)
      return scene(name, args);
    throw amx::HostFault("undocumented scene service");
  };
  variables_->player_service = [this](const std::string &name,
                                      const std::vector<std::uint32_t> &args) -> std::uint32_t {
    if (name == "player_giveExp" && campaign_->has_xp_table() && args.size() >= 2) {
      // The record may have changed since the last quest command.
      mirror();
      campaign_->give_exp_to(static_cast<int>(args[0]), static_cast<std::int32_t>(args[1]));
      quests::write_exp(*campaign_, *variables_, static_cast<int>(args[0]));
      return 0;
    }
    throw amx::HostFault(name + " callback required");
  };
}
void Host::mirror() { quests::mirror_from_variables(*campaign_, *variables_); }

void Host::install() {
  const auto known = variables_->natives();
  for (std::size_t i = 0; i < COMMANDS; ++i) {
    if (kRows[i].index != static_cast<int>(i) || !kRows[i].name)
      throw std::logic_error("script command table is not in binder order");
    if (known.find(kRows[i].name) == known.end())
      throw std::logic_error("script command is missing from the binder");
  }
  variables_->local_player_slot = []() { return amx::Cell(0); };
  variables_->run_reward_script = [this](const std::string &name) {
    variables_->run_script(name);
  };
  variables_->scene_service = [this](const std::string &name,
                                     const std::vector<builtins::Variables::ServiceValue> &args) -> std::uint32_t {
    if (name == "obj_moveOffGrid" || name == "obj_updateGridCell") {
      if (args.empty())
        throw amx::HostFault("grid call needs an object");
      auto *obj = variables_->obj_at(word_of(args[0]));
      if (!obj)
        throw amx::HostFault("grid call has no object");
      if (name == "obj_moveOffGrid")
        variables_->world->move_off_grid(*obj);
      else
        variables_->world->update_grid_cell(*obj);
      return 0;
    }
    if (name == "particle_spawn") {
      if (args.size() < 4)
        throw amx::HostFault("particle_spawn argument count");
      Spawn spawn;
      spawn.definition = word_of(args[0]);
      spawn.pos = vec_of(args[1]);
      spawn.vel = vec_of(args[2]);
      spawn.backdate = word_of(args[3]);
      spawns_.push_back(spawn);
      effects_->particle_spawn(spawn.definition, spawn.pos, spawn.vel, spawn.backdate);
      return 0;
    }
    throw amx::HostFault("undocumented scene service");
  };
}

// A refusal has no answer. When the script goes on to use the answer, the
// entry stops here instead of running on an invented 0.
void Host::refuse(std::size_t index, amx::Machine &machine) {
  counts_.refused[index] += 1;
  if (result_used(machine))
    throw RefusedResult(std::string("refused ") + kRows[index].name);
}

// Reads the name argument as the binder does (the cell at params + 4). An
// address the machine refuses makes the binder use a remembered string slot,
// which the host does not track; that counts as touching a stub.
bool Host::touches_stub(std::size_t index, amx::Machine &machine) const {
  const auto *read = stub_read(kRows[index].name);
  if (!read || stubs_.empty())
    return false;
  const auto address = machine.cell(machine.params + 4);
  if (machine.get_addr(address) != amx::ERR_NONE)
    return true;
  for (const auto *obj : variables_->find(machine.get_string(address), read->limit))
    if (stubs_.count(obj->addr))
      return true;
  return false;
}

amx::NativeMap Host::command_map() {
  const auto inner = variables_->natives();
  amx::NativeMap out;
  for (std::size_t i = 0; i < COMMANDS; ++i) {
    const auto found = inner.find(kRows[i].name);
    const auto function = found->second;
    const bool documented = kRows[i].effect == Effect::Documented;
    const bool sync_seed = std::strcmp(kRows[i].name, "getRand") == 0;
    const bool entrance = std::strcmp(kRows[i].name, "setEntrance") == 0;
    const bool cflag = std::strcmp(kRows[i].name, "setCFlag") == 0;
    out.emplace(kRows[i].name, [this, i, function, documented, sync_seed, entrance, cflag](
                                   amx::Machine &machine, const std::vector<amx::Cell> &params)
                                   -> std::optional<amx::Address> {
      // Undocumented commands refuse, except the quest commands whose bodies
      // are in the quest and mission model (QUESTS.md).
      if (!documented && !quests::route_undocumented(kRows[i].name, *campaign_)) {
        refuse(i, machine);
        return 0;
      }
      if (sync_seed && seed_refused()) {
        refuse(i, machine);
        return 0;
      }
      if (touches_stub(i, machine)) {
        counts_.stub_refusals += 1;
        refuse(i, machine);
        return 0;
      }
      counts_.executed[i] += 1;
      if (cflag) {
        try {
          campaign_->note_cflag(static_cast<int>(machine.cell(machine.params + 8)),
                                machine.cell(machine.params + 12) != 0);
        } catch (const std::exception &) {
          campaign_->note_cflag(-1, false);
        }
      }
      if (sync_seed)
        sync_seed_in();
      auto result = function(machine, params);
      if (sync_seed)
        sync_seed_out();
      if (entrance)
        apply_entrance();
      if (campaign_command(kRows[i].name))
        mirror();
      return result;
    });
  }
  return out;
}

// getRand must advance the session seed (research 9649e6b
// docs/script-composition.md, "The seam"); a host-private generator would
// desynchronise combat and FX. Without a bound seed the draw is refused. An
// adopted stub was never constructed, and each constructed object draws once
// from the shared seed during the objects.ob load (docs/issues/arena-entry.md
// 1.4), so after a stub the bound seed's state is not the original's either.
bool Host::seed_refused() const { return !external_seed_ || !stubs_.empty(); }
void Host::sync_seed_in() { variables_->memory.word(SEED_WORD, external_seed_->state); }
void Host::sync_seed_out() { external_seed_->state = variables_->memory.word(SEED_WORD); }
void Host::apply_entrance() { flow_.entrance = variables_->memory.cstr(ENTRANCE_WORD); }
void Host::bind_seed(SharedRandom *next) { external_seed_ = next; }
level_flow::ChangeResult Host::change_level(const level_flow::ChangeOptions &options) {
  mirror();
  auto result = quests::change_level(*campaign_, flow_, options, {});
  quests::mirror_to_variables(*campaign_, *variables_);
  return result;
}
bool Host::tick_mission_menu(int button, const quests::MenuInputs &inputs) {
  mirror();
  const bool launch = campaign_->tick_menu(button, inputs);
  const auto &menu = campaign_->menu();
  if (menu.paused)
    variables_->memory.word(PAUSED_WORD, 1u);
  if (launch) {
    variables_->memory.string(PENDING_WORD, menu.pending);
    variables_->memory.byte(ORIENT_WORD, static_cast<std::uint8_t>(menu.orient));
    flow_.pending.request(menu.pending, level_flow::ORIENT_NONE);
  }
  return launch;
}
std::vector<std::uint8_t> Host::save_campaign() {
  mirror();
  return campaign_->save();
}
void Host::load_campaign(const std::vector<std::uint8_t> &bytes) {
  auto next = quests::Campaign::load(bytes);
  *campaign_ = std::move(next);
  quests::restore_variables(*campaign_, *variables_);
}

bool Host::load(const amx::ByteVector &image) {
  machine_.reset();
  variables_->script_machine.reset();
  level_started_ = false;
  waiting_ = false;
  faulted_ = false;
  refusal_stopped_ = false;
  counted_wait_ = false;
  counted_complete_ = false;
  last_value_ = 0;
  if (image.empty() || image.size() > session::MAX_SCRIPT_BYTES)
    return false;
  try {
    auto machine = std::make_shared<amx::Machine>(image, command_map());
    if (machine->load() != amx::ERR_NONE)
      return false;
    for (const auto &binding : machine->natives()) {
      if (binding.kind != amx::BindingKind::Bound || !binding.function)
        return false;
    }
    machine_ = std::move(machine);
    variables_->script_machine = machine_;
    return true;
  } catch (const std::exception &) {
    machine_.reset();
    variables_->script_machine.reset();
    return false;
  }
}

bool Host::adopt_objects(const std::uint8_t *data, std::size_t size) {
  if (!data || !size) {
    counts_.objects_refused += 1;
    return false;
  }
  try {
    const auto records = level_flow::parse_objects(champions::Bytes{data, size});
    for (const auto &record : records) {
      auto props = std::make_shared<std::vector<std::string>>(record.props);
      // No factory class: nothing says what the record is. Not registered.
      if (record.cls.empty()) {
        counts_.objects_refused += 1;
        continue;
      }
      const auto obj = variables_->new_object({}, 0, record.position, record.angle, record.cls, props);
      stubs_.insert(obj->addr);
      counts_.objects_registered += 1;
    }
    return true;
  } catch (const std::exception &) {
    counts_.objects_refused += 1;
    return false;
  }
}

void Host::add_object(const std::string &name, std::uint32_t flags,
                      const std::array<std::uint32_t, 3> &pos, std::uint32_t angle) {
  auto props = std::make_shared<std::vector<std::string>>();
  props->push_back("name=" + name);
  variables_->new_object({}, flags, pos, angle, "10GameObject", props);
  counts_.objects_registered += 1;
}

void Host::note_fault() {
  counts_.faults += 1;
  faulted_ = true;
  waiting_ = false;
}
void Host::remember(const std::exception &error) {
  if (last_fault_.empty())
    last_fault_ = error.what();
  if (dynamic_cast<const RefusedResult *>(&error)) {
    counts_.refusal_stops += 1;
    refusal_stopped_ = true;
    faulted_ = true;
    waiting_ = false;
    return;
  }
  note_fault();
}
Stop Host::stopped() const { return refusal_stopped_ ? Stop::Refused : Stop::Fault; }
void Host::note_queue() {
  const auto state = queue();
  if (state.count == 0 && state.queued == 0) {
    waiting_ = false;
    if (level_started_ && !faulted_ && !counted_complete_) {
      counts_.reached_complete += 1;
      counted_complete_ = true;
    }
    return;
  }
  waiting_ = true;
  if (!counted_wait_) {
    counts_.reached_wait += 1;
    counted_wait_ = true;
  }
}
Stop Host::classify() {
  if (faulted_)
    return stopped();
  note_queue();
  if (waiting_)
    return Stop::Wait;
  if (level_started_ && counted_complete_)
    return Stop::Complete;
  return Stop::Missing;
}

Stop Host::enter(const std::string &name, bool optional) {
  if (!machine_ || faulted_) {
    if (!faulted_)
      note_fault();
    return stopped();
  }
  const auto found = machine_->find_public(name);
  if (found.first != amx::ERR_NONE) {
    if (!optional)
      counts_.missing_entries += 1;
    return Stop::Missing;
  }
  machine_->budget = session::SCRIPT_BUDGET;
  try {
    const auto result = machine_->execute(found.second, {}, 0);
    last_value_ = static_cast<amx::Cell>(result.second.value_or(0));
    if (result.first != amx::ERR_NONE) {
      note_fault();
      return Stop::Fault;
    }
  } catch (const std::exception &error) {
    remember(error);
    return stopped();
  }
  return Stop::Complete;
}

Stop Host::enter_level(const char *name) {
  if (!machine_ || faulted_) {
    if (!faulted_)
      note_fault();
    return stopped();
  }
  if (machine_->find_public(name).first != amx::ERR_NONE)
    return Stop::Missing;
  if (!level_started_) {
    counts_.started += 1;
    level_started_ = true;
  }
  return enter(name, false);
}

// level_loadScriptAndObjects runs preStart, loads the objects, then runs start
// (research 9649e6b docs/script.md 2.1; docs/issues/arena-entry.md 1.4).
Stop Host::run_prestart() { return enter_level("preStart"); }

// start, then the load's post action (game_loadLevel 0x1e60b8), then one
// script_runDeferred outside any tick (mainloop_runSession 0x322e6c;
// docs/mainloop.md step 7). That run promotes what start queued, so it runs
// on the first ready tick.
Stop Host::run_start() {
  const auto stop = enter_level("start");
  if (stop == Stop::Fault || stop == Stop::Refused)
    return stop;
  if (!level_started_)
    return Stop::Missing;
  try {
    machine_->budget = session::SCRIPT_BUDGET;
    variables_->run_post_action();
    machine_->budget = session::SCRIPT_BUDGET;
    variables_->run_deferred();
  } catch (const std::exception &error) {
    remember(error);
  }
  return classify();
}

// The Script task runs on every simulated tick (docs/script.md 2.4 and 2.6),
// also with an empty queue: it still flips the halves.
void Host::on_ready_tick() {
  if (faulted_ || !machine_)
    return;
  machine_->budget = session::SCRIPT_BUDGET;
  try {
    variables_->run_deferred();
  } catch (const std::exception &error) {
    remember(error);
    return;
  }
  classify();
}

Stop Host::run(const std::string &entry) {
  const auto stop = enter(entry, false);
  if (stop != Stop::Complete)
    return stop;
  note_queue();
  return waiting_ ? Stop::Wait : Stop::Complete;
}

Queue Host::queue() const {
  Queue state;
  if (!variables_)
    return state;
  state.count = variables_->memory.word(DEFER_COUNT);
  state.queued = variables_->memory.word(DEFER_QUEUED);
  state.flip = variables_->memory.word(DEFER_FLIP);
  return state;
}
std::string Host::queue_key() const {
  constexpr std::uint32_t names = 0x86b278;
  constexpr std::size_t bytes = 2u * 64u * 0x50u;
  auto raw = variables_->memory.read(names, bytes);
  const auto state = queue();
  for (const auto word : {state.count, state.queued}) {
    for (int shift = 0; shift < 32; shift += 8)
      raw.push_back(static_cast<std::uint8_t>(word >> shift));
  }
  return std::string(raw.begin(), raw.end());
}
std::uint32_t Host::public_count() const {
  return machine_ ? static_cast<std::uint32_t>(machine_->num_publics) : 0;
}
} // namespace champions::simulation::rta::script_commands
