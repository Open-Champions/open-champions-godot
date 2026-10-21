#include "champions/script_commands.hpp"
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
} // namespace

const Row &row(std::size_t index) {
  if (index >= COMMANDS)
    throw std::out_of_range("script command index");
  return kRows[index];
}

Host::Host()
    : variables_(std::make_unique<builtins::Variables>()),
      effects_(std::make_unique<champions::rta::fx::sim::EffectWorld>()) {
  install();
}
Host::~Host() = default;

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

amx::NativeMap Host::command_map() {
  const auto inner = variables_->natives();
  amx::NativeMap out;
  for (std::size_t i = 0; i < COMMANDS; ++i) {
    if (kRows[i].effect != Effect::Documented) {
      out.emplace(kRows[i].name, [this, i](amx::Machine &, const std::vector<amx::Cell> &)
                                     -> std::optional<amx::Address> {
        counts_.refused[i] += 1;
        return 0;
      });
      continue;
    }
    const auto found = inner.find(kRows[i].name);
    const auto function = found->second;
    const bool sync_seed = std::strcmp(kRows[i].name, "getRand") == 0;
    const bool entrance = std::strcmp(kRows[i].name, "setEntrance") == 0;
    out.emplace(kRows[i].name, [this, i, function, sync_seed, entrance](
                                   amx::Machine &machine, const std::vector<amx::Cell> &params)
                                   -> std::optional<amx::Address> {
      counts_.executed[i] += 1;
      if (sync_seed)
        sync_seed_in();
      auto result = function(machine, params);
      if (sync_seed)
        sync_seed_out();
      if (entrance)
        apply_entrance();
      return result;
    });
  }
  return out;
}

SharedRandom &Host::seed() {
  return external_seed_ ? *external_seed_ : variables_->world->shared;
}
void Host::sync_seed_in() { variables_->memory.word(SEED_WORD, seed().state); }
void Host::sync_seed_out() { seed().state = variables_->memory.word(SEED_WORD); }
void Host::apply_entrance() { flow_.entrance = variables_->memory.cstr(ENTRANCE_WORD); }
void Host::bind_seed(SharedRandom *next) { external_seed_ = next; }

bool Host::load(const amx::ByteVector &image) {
  machine_.reset();
  variables_->script_machine.reset();
  level_started_ = false;
  waiting_ = false;
  faulted_ = false;
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
      const std::string type = record.cls.empty() ? "10GameObject" : record.cls;
      variables_->new_object({}, 0, record.position, record.angle, type, props);
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
  note_fault();
}
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
    return Stop::Fault;
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
    return Stop::Fault;
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
    return Stop::Fault;
  }
  return Stop::Complete;
}

Stop Host::enter_level(const char *name) {
  if (!machine_ || faulted_) {
    if (!faulted_)
      note_fault();
    return Stop::Fault;
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
  if (stop == Stop::Fault)
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
  if (stop == Stop::Fault || stop == Stop::Missing)
    return stop;
  note_queue();
  if (faulted_)
    return Stop::Fault;
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
