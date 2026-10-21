#include "champions/script_commands.hpp"
#include "champions/objects.hpp"
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
// g_scriptSelectedObj (research 9649e6b tools/ref_natives.py G_SELECTED_OBJ,
// docs/script.md line 481): the object the select* commands act on.
constexpr std::uint32_t SELECTED_WORD = 0x4039f8;
// Stream 2's queue count: g_streams 0x876c10 + 2 * 0x1c4 + 448 (research
// 52984db docs/script-effects.md endMusic; ref_script_effects.stream_count_addr).
constexpr std::uint32_t STREAM2_COUNT = 0x877158;
// missionMenu_run (research t3/research-quests docs/quests.md section 4):
// g_paused, the pending level name and its orientation byte.
constexpr std::uint32_t PAUSED_WORD = 0x41b4f4, PENDING_WORD = 0x41b6d0, ORIENT_WORD = 0x41b720;

// Documented commands whose binder path reads, on the objects its name
// argument finds, the creature flag 0x20, hit points at +0x228 or the type
// name (script_builtins.cpp creature_dispatch and menu_dispatch; research
// 9649e6b tools/ref_natives.py n_<name>). limit is the binder's lookup limit;
// setHitPoints looks up the first object only, which is limit 2.
// hit_points: the path also reads hit points at +0x228 (getHitPoints).
struct StubRead {
  const char *name;
  std::int64_t limit;
  bool hit_points;
};
const StubRead kStubReads[] = {
    {"increaseEnemyRank", 0x200, false}, {"unRootMonster", 0x200, false},
    {"stopFollowingPlayer", 0x200, false}, {"friendly", 0x200, false},
    {"unFriendly", 0x200, false},          {"hideMonster", 0x200, false},
    {"showMonster", 0x200, false},         {"getHitPoints", 0x200, true},
    {"setHitPoints", 2, false},            {"countLiveObjects", 0x80, false},
    {"showOnMap", 100, false},
    // setCFlag tests the creature flag 0x20 on each object it finds
    // (research-quests 7efd3ce tools/ref_natives.py n_setCFlag).

    {"setCFlag", 0x200, false},
    // Executed callee routes also read the creature flag.
    {"setMonsterVal", 0x200, false}, {"setMonsterRange", 0x200, false},
    {"setCreatureParams", 0x200, false}, {"charmMonster", 0x200, false},
    {"followPlayer", 0x200, false},
    // generateMonster reads the found object type before spawning.
    {"generateMonster", 0x200, false},

};
// The select* forms act on g_scriptSelectedObj and test the same flag.
bool selected_form(const char *name) {
  return std::strcmp(name, "selectSetMonsterVal") == 0 || std::strcmp(name, "initScriptSetParams") == 0 ||
         std::strcmp(name, "selectSetCFlag") == 0;
}
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
const std::string &string_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *text = std::get_if<std::string>(&value))
    return *text;
  throw amx::HostFault("scene value is not a string");
}
std::array<std::uint32_t, 3> vec_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *vec = std::get_if<std::array<std::uint32_t, 3>>(&value))
    return *vec;
  throw amx::HostFault("scene value is not a vector");
}
// Commands whose callee body research executed (52984db
// tools/ref_script_effects.py COMMANDS, status executed) and that need no
// callee research did not enter. setInfo, addHelpMessage, addHelpMessageF,
// showBust and showCompass are executed setters behind text_lookup, whose
// string table the host does not have, so they stay refused. selectSetCFlag
// is the one partial row routed: its bit-0 path reaches
// creature_startBossMusic, whose name selector is executed and whose stop and
// queue bodies are inferred from research-audio 6b08886 (counted in
// Counts::inferred). setCFlag takes the same path through the quest route.
bool routed_command(const char *name) {
  static const char *names[] = {
      "setMonsterVal",     "selectSetMonsterVal", "setMonsterRange",    "setCreatureParams",
      "initScriptSetParams", "hudResetFadeScreen", "hudFadeScreen",     "charmMonster",
      "followPlayer",      "startMusic",          "endMusic",           "selectSetCFlag",
  };
  for (const char *item : names)
    if (std::strcmp(item, name) == 0)
      return true;
  return false;
}
// Rows whose bit-0 route runs a body research inferred rather than executed.
bool inferred_route(const char *name) {
  return std::strcmp(name, "setCFlag") == 0 || std::strcmp(name, "selectSetCFlag") == 0;
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
      effects_(std::make_unique<champions::rta::fx::sim::EffectWorld>()),
      audio_(std::make_unique<champions::audio::Playback>()),
      script_fx_(std::make_unique<script_effects::Effects>(variables_->memory)) {
  install();
  bind_campaign();
  items_ = std::make_unique<script_items::Service>(*variables_);
  // The executable image's g_difficultyIdx is 1; the mission rows are 1..5
  // and row 0 lies on the skill pairs clearAllSkills fills (research-items
  // 49bc578 docs/script-items.md 3). game_scanCm1Vars(0)'s word is -1 until
  // game_set4f7110 runs (section 6).
  variables_->memory.word(script_items::G_DIFFICULTY, static_cast<std::uint32_t>(script_items::DIFFICULTY_IMAGE));
  campaign_->set_difficulty(script_items::DIFFICULTY_IMAGE);
  variables_->memory.word(script_items::G_CM1_WORD, static_cast<std::uint32_t>(script_items::CM1_IMAGE));
}
Host::~Host() = default;

void Host::bind_campaign() {
  auto scene = variables_->scene_service;
  variables_->scene_service = [this, scene](const std::string &name,
                                            const std::vector<builtins::Variables::ServiceValue> &args)
      -> std::uint32_t {
    if (name == "creature_startBossMusic")
      campaign_->note_boss_music();
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
  // These answers reach only handlers that discard them (research 52984db
  // docs/script-effects.md: charmMonster, followPlayer, startMusic and
  // hudFadeScreen return 0 or hud_stepFade's own value).
  script_fx_->discarded = {"handle_clear", "ui_drawRect", "audio_setChannelVolume",
                           "stream_playLevel"};
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
    return scene(name, args);
  };
}

// A refusal has no answer. When the script goes on to use the answer, the
// entry stops here instead of running on an invented 0.
void Host::refuse(std::size_t index, amx::Machine &machine) {
  counts_.refused[index] += 1;
  if (result_used(machine)) {
    counts_.stopped[index] += 1;
    throw RefusedResult(std::string("refused ") + kRows[index].name);
  }
}

// Reads the name argument as the binder does (the cell at params + 4). An
// address the machine refuses makes the binder use a remembered string slot,
// which the host does not track; that counts as touching a stub.
bool Host::touches_stub(std::size_t index, amx::Machine &machine) const {
  if (selected_form(kRows[index].name))
    return stubs_.count(variables_->memory.word(SELECTED_WORD)) != 0;
  const auto *read = stub_read(kRows[index].name);
  if (!read || (stubs_.empty() && built_.empty()))
    return false;
  const auto address = machine.cell(machine.params + 4);
  if (machine.get_addr(address) != amx::ERR_NONE)
    return true;
  for (const auto *obj : variables_->find(machine.get_string(address), read->limit)) {
    if (stubs_.count(obj->addr))
      return true;
    // A built creature's hit points come from Creature_setStats, which is not
    // ported (docs/script-session.md section 5).
    if (read->hit_points && built_.count(obj->addr))
      return true;
  }
  return false;
}

amx::NativeMap Host::command_map() {
  const auto inner = variables_->natives();
  amx::NativeMap out;
  for (std::size_t i = 0; i < COMMANDS; ++i) {
    const auto found = inner.find(kRows[i].name);
    const auto function = found->second;
    const bool documented = kRows[i].effect == Effect::Documented || routed_command(kRows[i].name);
    const bool sync_seed = std::strcmp(kRows[i].name, "getRand") == 0 || script_items::draws_seed(kRows[i].name);
    const bool needs_tables = script_items::draws_seed(kRows[i].name);

    const bool entrance = std::strcmp(kRows[i].name, "setEntrance") == 0;
    const bool cflag = std::strcmp(kRows[i].name, "setCFlag") == 0;
    out.emplace(kRows[i].name, [this, i, function, documented, sync_seed, needs_tables, entrance, cflag](
                                   amx::Machine &machine, const std::vector<amx::Cell> &params)
                                   -> std::optional<amx::Address> {
      // Undocumented commands refuse, except the quest commands whose bodies
      // are in the quest and mission model (QUESTS.md).
      // ... and the item commands whose callees are in script_items.
      if (!documented && !quests::route_undocumented(kRows[i].name, *campaign_) &&
          !script_items::routes(kRows[i].name)) {
        refuse(i, machine);
        return 0;
      }
      if (needs_tables && !items_->has_tables()) {
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
      if (callee_unknown(i, machine)) {
        counts_.callee_refusals += 1;
        refuse(i, machine);
        return 0;
      }
      counts_.executed[i] += 1;
      // Only bit 0 reaches creature_startBossMusic (the selector's which
      // argument: params + 8 for setCFlag, + 4 for selectSetCFlag).
      if (inferred_route(kRows[i].name) &&
          machine.cell(machine.params + (std::strcmp(kRows[i].name, "setCFlag") == 0 ? 8 : 4)) == 0)
        counts_.inferred[i] += 1;
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
      std::optional<amx::Address> result;
      try {
        result = function(machine, params);
      } catch (const script_items::Refused &error) {
        // Refused part-way: what ran stays, and the entry stops here.
        if (sync_seed)
          sync_seed_out();
        counts_.executed[i] -= 1;
        counts_.refused[i] += 1;
        throw RefusedResult(std::string("refused ") + kRows[i].name + ": " + error.what());
      }
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
// 1.4), so after a stub the bound seed's state is not the original's either:
// with SeedPolicy::Exact that refuses. Nothing reseeds the shared seed at a
// level start; an offline session seeds it once from the cycle counter
// (research-script-session 6841b8a, docs/script-session.md section 1), so with
// SeedPolicy::OfflineSession the session's generator answers and the missing
// draws are only counted.
bool Host::seed_refused() const {
  if (!external_seed_)
    return true;
  if (seed_policy_ == SeedPolicy::OfflineSession)
    return false;
  return !stubs_.empty() || counts_.load_draws_unmodelled != 0;
}
void Host::sync_seed_in() { variables_->memory.word(SEED_WORD, external_seed_->state); }
void Host::sync_seed_out() { external_seed_->state = variables_->memory.word(SEED_WORD); }
void Host::apply_entrance() { flow_.entrance = variables_->memory.cstr(ENTRANCE_WORD); }

// The objects the binder's name argument finds. nullopt when the address is
// one the machine refuses: the binder would then reuse a remembered string
// slot the host does not track.
std::optional<std::vector<const objects::GameObject *>> Host::named(amx::Machine &machine,
                                                                   std::int64_t limit) const {
  const auto address = machine.cell(machine.params + 4);
  if (machine.get_addr(address) != amx::ERR_NONE)
    return std::nullopt;
  std::vector<const objects::GameObject *> out;
  for (const auto *obj : variables_->find(machine.get_string(address), limit))
    out.push_back(obj);
  return out;
}

std::string Host::boss_name(const objects::GameObject &obj) const {
  const auto type_at = variables_->memory.u32(obj.addr + 0x34);
  if (type_at == 0)
    return {};
  std::string object_name;
  if (auto found = objects::props_find(obj.props, "name"))
    object_name = *found;
  return champions::audio::Playback::boss_music_name(variables_->memory.cstr(type_at), object_name)
      .value_or(std::string());
}

// A routed command whose callee research did not execute for this state.
// Checked before the binder runs, so a refused command stores nothing.
bool Host::callee_unknown(std::size_t index, amx::Machine &machine) const {
  const std::string name = kRows[index].name;
  auto &memory = variables_->memory;
  const auto creature = [&](const objects::GameObject *obj) {
    return obj && (memory.u32(obj->addr + 0xc) & 0x20u) != 0;
  };
  if (name == "charmMonster" || name == "followPlayer") {
    // Creature_setCharm reaches handle_clear of +0x234 past these gates;
    // a live handle is research's Unknown (docs/script-effects.md charmMonster).
    const auto found = named(machine, 0x200);
    if (!found)
      return true;
    for (const auto *obj : *found) {
      if (!creature(obj) || memory.s16(obj->addr + 0x312) == -1 || (memory.u32(obj->addr + 0xc) & 0x40u))
        continue;
      if (memory.u32(obj->addr + 0x234) != 0)
        return true;
    }
    return false;
  }
  if (name == "hudFadeScreen")
    // hud_stepFade adds to g_hudFade, which is kept between fades; this host
    // knows it only after hudResetFadeScreen has set it.
    return !fade_known_;
  return false;
}

void Host::set_music_volume(std::uint32_t bits) { audio_->set_music_volume(bits); }

std::uint32_t Host::scene(const std::string &name,
                          const std::vector<builtins::Variables::ServiceValue> &args) {
  auto &fx = *script_fx_;
  auto &memory = variables_->memory;
  if (name == "hud_setFade") {
    if (args.empty())
      throw amx::HostFault("hud_setFade argument count");
    fx.set_fade(word_of(args[0]));
    fade_known_ = true;
    return 0;
  }
  if (name == "hud_stepFade") {
    if (args.empty())
      throw amx::HostFault("hud_stepFade argument count");
    return fx.step_fade(word_of(args[0]));
  }
  if (name == "Creature_scaleStats") {
    if (args.size() < 6)
      throw amx::HostFault("Creature_scaleStats argument count");
    std::uint32_t scales[5];
    for (int i = 0; i < 5; ++i)
      scales[i] = word_of(args[static_cast<std::size_t>(i + 1)]);
    return fx.scale_stats(word_of(args[0]), scales);
  }
  if (name == "Creature_setCharm") {
    if (args.size() < 3)
      throw amx::HostFault("Creature_setCharm argument count");
    if (memory.u32(word_of(args[0]) + 0x234) != 0 && memory.s16(word_of(args[0]) + 0x312) != -1)
      throw amx::HostFault("Creature_setCharm reached a live target handle");
    fx.set_charm(word_of(args[0]), word_of(args[1]), word_of(args[2]));
    return 0;
  }
  if (name == "virtual") {
    if (args.size() < 3)
      throw amx::HostFault("virtual argument count");
    if (word_of(args[0]) != 34u)
      throw amx::HostFault("undocumented virtual slot");
    fx.slot34_set_home_range(word_of(args[1]), word_of(args[2]));
    return 0;
  }
  if (name == "music_playLevel") {
    if (args.size() < 2)
      throw amx::HostFault("music_playLevel argument count");
    const auto &title = string_of(args[0]);
    const auto flags = word_of(args[1]);
    // The gate and the channel word read g_musicVolume and the duck scalar;
    // both are the host Playback's.
    memory.w32(script_effects::G_MUSIC_VOLUME, audio_->music_volume());
    memory.w32(script_effects::G_MUSIC_SCALAR, audio_->duck_current());
    // The name pointer only travels to stream_playLevel, which the host runs
    // as Playback::music_play_level by name. The word is a host handle, not
    // a binder address.
    const auto found = music_titles_.find(title);
    const auto handle = found != music_titles_.end()
                            ? found->second
                            : music_titles_.emplace(title, static_cast<std::uint32_t>(music_titles_.size())).first->second;
    const auto result = fx.music_play_level(handle, flags);
    if (fx.channel_volume) {
      music_channel_ = *fx.channel_volume;
      audio_->music_play_level(title, flags);
    }
    return result;
  }
  if (name == "stream_queueStop") {
    if (args.empty())
      throw amx::HostFault("stream_queueStop argument count");
    const auto stream = word_of(args[0]);
    if (stream != 2)
      throw amx::HostFault("stream_queueStop on a stream other than 2");
    // Stream 2's queue is the host Playback's; its count is the word the
    // executed body reads.
    memory.w32(STREAM2_COUNT, static_cast<std::uint32_t>(audio_->queued_count(2)));
    const auto before = memory.u32(STREAM2_COUNT);
    const auto result = fx.queue_stop(stream);
    if (before < 8)
      audio_->queue_stop(2);
    return result;
  }
  if (name == "creature_startBossMusic") {
    if (args.empty())
      throw amx::HostFault("creature_startBossMusic argument count");
    auto *obj = variables_->obj_at(word_of(args[0]));
    if (!obj)
      throw amx::HostFault("creature_startBossMusic has no object");
    // creature_bossMusicName: a null type name at +0x34 or no row is no
    // name, and then nothing else is called (docs/script-effects.md setCFlag).
    const auto type_at = memory.u32(obj->addr + 0x34);
    if (type_at == 0)
      return 0;
    std::string object_name;
    if (auto found = objects::props_find(obj->props, "name"))
      object_name = *found;
    audio_->play_boss_music(memory.cstr(type_at), object_name);
    return 0;
  }
  throw amx::HostFault("undocumented scene service");
}


void Host::bind_seed(SharedRandom *next, SeedPolicy policy) {
  external_seed_ = next;
  seed_policy_ = policy;
}

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
    if (exe_)
      return adopt_classified(records);
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
      counts_.objects_unclassified += 1;
      counts_.load_draws_unmodelled += 1;
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
// The state a deferred wait can count in: the deferred name table and its two
// counters (without the flip), the script's global variables (the data
// segment below the heap), the quest variables and the post-action name
// (g_postAction, research 9649e6b tools/ref_natives.py G_POST_ACTION).
// docs/script.md 2.4: a waiting entry "counts in a global variable and
// defers itself again".
std::string Host::queue_key() const {
  constexpr std::uint32_t names = 0x86b278;
  constexpr std::size_t bytes = 2u * 64u * 0x50u;
  constexpr std::uint32_t post_action = 0x86fa80;
  auto raw = variables_->memory.read(names, bytes);
  auto put = [&raw](std::uint32_t word) {
    for (int shift = 0; shift < 32; shift += 8)
      raw.push_back(static_cast<std::uint8_t>(word >> shift));
  };
  const auto state = queue();
  put(state.count);
  put(state.queued);
  if (machine_ && machine_->hea0 > machine_->dat)
    for (amx::Address at = 0; at < machine_->hea0 - machine_->dat; at += 4)
      put(static_cast<std::uint32_t>(machine_->cell(at)));
  for (const auto &var : variables_->quest_vars()) {
    raw.insert(raw.end(), var.first.begin(), var.first.end());
    raw.push_back(0);
    put(static_cast<std::uint32_t>(var.second));
  }
  const auto action = variables_->memory.cstr(post_action);
  raw.insert(raw.end(), action.begin(), action.end());
  return std::string(raw.begin(), raw.end());
}
std::uint32_t Host::public_count() const {
  return machine_ ? static_cast<std::uint32_t>(machine_->num_publics) : 0;
}
} // namespace champions::simulation::rta::script_commands
