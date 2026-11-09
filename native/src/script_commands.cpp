#include "champions/script_commands.hpp"
#include "champions/objects.hpp"
#include "champions/quests_bridge.hpp"
#include "champions/session.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
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
    // generateMonster reads the found object type before spawning. Every
    // ScriptGenerator record is built (Host::build_generator), so only an
    // unbuilt one can hide behind a stub (touches_stub).
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
// showBust and showCompass are executed setters behind text_lookup; research
// d4987fd executed the table, wstr_format and their HUD state
// (docs/strings.md), which the host runs through champions-strings with the
// game's own table (register_text). selectSetCFlag
// and the guarded trigger(portal,0) request are partial rows routed here.
// selectSetCFlag's bit-0 path reaches
// creature_startBossMusic, whose name selector is executed and whose stop and
// queue bodies are inferred from research-audio 6b08886 (counted in
// Counts::inferred). setCFlag takes the same path through the quest route.
bool routed_command(const char *name) {
  static const char *names[] = {
      "setMonsterVal",     "selectSetMonsterVal", "setMonsterRange",    "setCreatureParams",
      "initScriptSetParams", "hudResetFadeScreen", "hudFadeScreen",     "charmMonster",
      "followPlayer",      "startMusic",          "endMusic",           "selectSetCFlag",
      "setInfo",           "addHelpMessage",      "addHelpMessageF",    "showBust",
      "showCompass",       "newWorld",            "newWorldAlways",    "simpleNewWorld",
      "newWorldParameter", "setEntrance", "trigger",
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

// The dialog built-ins (docs/DIALOG.md). With a dialog attached their binder
// bodies run; their seams are answered by script_dialog.cpp.
bool dialog_command(const char *name) {
  static const char *names[] = {"startDialog", "dialogRunning", "forceMutexRelease", "activateStore",
                                "newWorldPost", "changeSwoop", "startWithSwoopDone", "trigger"};
  for (const char *each : names)
    if (std::strcmp(name, each) == 0)
      return true;
  return false;
}
bool text_command(const std::string &name) {
  return name == "setInfo" || name == "addHelpMessage" || name == "addHelpMessageF" ||
         name == "showBust" || name == "showCompass";
}
// The resource word the host answers res_find("fx.lmp", "butn_message.anm")
// with: a host handle for the chime sequence, never a binder address.
constexpr std::uint32_t CHIME_SEQUENCE = 1;
// Binder words the HUD gate reads (docs/strings.md 3.1, 3.4) and the HUD words
// binder handlers read back (helpMessageActive reads 0x41b928).
constexpr std::uint32_t LOCAL_MENU_WORD = 0x41b69d, LOADSCREEN_WORD = 0x506094,
                        MENU_OPEN_WORD = 0x41b4f8, NETWORK_WORD = 0x402791;
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

Host::Host() : Host(nullptr) {}
Host::Host(std::shared_ptr<WorldAuthority> authority)
    : authority_(std::move(authority)),
      variables_(authority_ ? std::make_unique<builtins::Variables>(authority_)
                           : std::make_unique<builtins::Variables>()),
      campaign_(std::make_unique<quests::Campaign>()),
      effects_(std::make_unique<champions::rta::fx::sim::EffectWorld>()),
      audio_(std::make_unique<champions::audio::Playback>()),
      script_fx_(std::make_unique<script_effects::Effects>(variables_->memory)),
      text_(std::make_unique<strings::TextTable>()), hud_(std::make_unique<strings::Hud>()),
      text_commands_(std::make_unique<strings::Commands>(*text_, *hud_)),
      sound_(std::make_unique<strings::SoundSeqs>(&variables_->world->handles)) {
  // No camera object is installed until the caller supplies its mode.
  hud_->inputs.camera_mode.reset();
  sound_->limits = strings::disc_group_limits();
  text_commands_->find_object = [this](const std::string &name)
      -> std::optional<std::array<std::uint32_t, 3>> {
    const auto *obj = variables_->find_first(name);
    if (!obj)
      return std::nullopt;
    auto &memory = variables_->memory;
    return std::array<std::uint32_t, 3>{memory.word(obj->addr + 0x10), memory.word(obj->addr + 0x14),
                                        memory.word(obj->addr + 0x18)};
  };
  equipment_loading_ = std::make_unique<loading_equipment::Runtime>(variables_->memory);
  install();
  bind_campaign();
  items_ = std::make_unique<script_items::Service>(*variables_);
  items_->delegates["script_runByName"] = [this](const std::vector<std::uint64_t> &args) {
    const auto entry = variables_->memory.cstr(std::uint32_t(args.at(0)));
    const auto stop = run(entry);
    if (stop != Stop::Complete && stop != Stop::Missing)
      throw script_items::Refused("item notification '" + entry + "' did not complete (stop " +
                                  std::to_string(static_cast<unsigned>(stop)) + ")");
    return std::uint64_t(0);
  };
  variables_->stub_creature = [this](const objects::GameObject &obj) {
    const auto info = stub_info_.find(obj.addr);
    return stubs_.count(obj.addr) != 0 && info != stub_info_.end() &&
           info->second.creature == level_factory::Creature::Yes;
  };
  // The executable image's g_difficultyIdx is 1; the mission rows are 1..5
  // and row 0 lies on the skill pairs clearAllSkills fills (research-items
  // 49bc578 docs/script-items.md 3). game_scanCm1Vars(0)'s word is -1 until
  // game_set4f7110 runs (section 6).
  variables_->memory.word(script_items::G_DIFFICULTY, static_cast<std::uint32_t>(script_items::DIFFICULTY_IMAGE));
  campaign_->set_difficulty(script_items::DIFFICULTY_IMAGE);
  variables_->memory.word(script_items::G_CM1_WORD, static_cast<std::uint32_t>(script_items::CM1_IMAGE));
  // The executable image starts g_hudFade at zero (research-fade 197f52a).
  variables_->memory.word(script_effects::G_FADE, 0);
}
void Host::mark_built(std::uint32_t address) {
  built_.insert(address);
  if (authority_) authority_->register_factory(address);
}
Host::~Host() {
  // Remove pairs from host-owned heads before the external objects or heads
  // are destroyed; Variables unregisters its own object nodes during teardown.
  for (auto &entry : target_cells_)
    variables_->world->handles.clear(*entry.second);
  // Variables unregisters external objects and their handle nodes. Their
  // host-owned list heads must still exist while that teardown runs.
  machine_.reset();
  variables_.reset();
}

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
void Host::mirror() { quests::mirror_from_variables(*campaign_, *variables_, player_currents_known_); }

namespace {
// Routes that read current HP/mana (record +0xac/+0xc0), copy the whole record
// with its unknown currents, or write a current field. Unknown stays distinct
// from a dead player or an empty mana pool. Sources, all research-menus
// docs/script.md (built-in table, rows by name) and docs/player.md:
//   checkPlayerAlive 0x1470ac  count of players with HP > 0 only when which == 0;
//                              with which != 0 it reads no HP (script.md:688)
//   healPlayer 0x1468ac, giveFullHealth 0x146950, planarSteel 0x1466d4
//                              write +0xac from +0xb0
//   zeroPlayerMana 0x142450, refillPlayerMana 0x142494   write +0xc0
//   givePlayerExp 0x146278     player_giveExp refills HP and mana on a level
//   savePlayer 0x141724, restorePlayer 0x141744   whole-record copy
//   playGotItemSound 0x1464e8  reads +0xac only with two or more players
//                              (script.md row 61 and P7); one player never does
// giveFullHealth, zeroPlayerMana, refillPlayerMana and givePlayerExp read no
// unknown field themselves (pure writes; givePlayerExp refills only on a level
// gained). They are still refused: the host does not track a written current
// as known, so a later read would otherwise see a half-known pair. This is a
// conservative choice, not a documented read.
bool needs_player_current(const char *name, std::int32_t players, bool which_zero) {
  if (std::strcmp(name, "checkPlayerAlive") == 0) return which_zero;
  for (const char *route : {"healPlayer", "giveFullHealth",
                            "zeroPlayerMana", "refillPlayerMana", "givePlayerExp",
                            "planarSteel", "savePlayer", "restorePlayer"})
    if (std::strcmp(name, route) == 0) return true;
  return players >= 2 && std::strcmp(name, "playGotItemSound") == 0;
}
}

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
void Host::refuse(std::size_t index, amx::Machine &machine, const char *reason) {
  counts_.refused[index] += 1;
  last_refusal_ = std::string("refused ") + kRows[index].name;
  if (reason) last_refusal_ += std::string(": ") + reason;
  if (mission1_) {
    counts_.stopped[index] += 1;
    throw RefusedResult(std::string("Mission 1 unported effect: ") + kRows[index].name);
  }
  if (complete_commands_ || result_used(machine)) {
    counts_.stopped[index] += 1;
    throw RefusedResult(last_refusal_);
  }
}

// Reads the name argument as the binder does (the cell at params + 4). An
// address the machine refuses makes the binder use a remembered string slot,
// which the host does not track; that counts as touching a stub.
bool Host::touches_stub(std::size_t index, amx::Machine &machine) {
  if (selected_form(kRows[index].name))
    return stubs_.count(variables_->memory.word(SELECTED_WORD)) != 0;
  const auto *read = stub_read(kRows[index].name);
  if (!read || (stubs_.empty() && built_.empty()))
    return false;
  const auto address = machine.cell(machine.params + 4);
  const std::string command = kRows[index].name;
  if (machine.get_addr(address) != amx::ERR_NONE) {
    counts_.stub_refusal_reasons[command + ": name argument is not a resolvable address"] += 1;
    return true;
  }
  const bool hide_show = std::strcmp(kRows[index].name, "hideMonster") == 0 ||
                         std::strcmp(kRows[index].name, "showMonster") == 0;
  const bool generate = std::strcmp(kRows[index].name, "generateMonster") == 0;
  for (const auto *obj : variables_->find(machine.get_string(address), read->limit)) {
    if (stubs_.count(obj->addr)) {
      const auto info = stub_info_.find(obj->addr);
      // hideMonster / showMonster need only the creature flag of the class
      // (docs/spec/hide-monster.md 3): a stub whose factory row decided it
      // runs. generateMonster acts only on a ScriptGenerator, and every one
      // of those is built unless its constructor refused.
      if (hide_show && info != stub_info_.end() && info->second.creature != level_factory::Creature::Unknown)
        continue;
      if (generate && info != stub_info_.end() && info->second.constructor != SCRIPT_GENERATOR_CTOR)
        continue;
      char at[32];
      std::snprintf(at, sizeof at, "0x%06x", info == stub_info_.end() ? 0u : info->second.constructor);
      counts_.stub_refusal_reasons[command + ": " + (info == stub_info_.end() ? "stub without a factory row" :
          info->second.load_property_refused ? std::string("hide/wait load property, constructor ") + at :
          info->second.creature == level_factory::Creature::Unknown ? std::string("undecided row, constructor ") + at
                                                                    : std::string("constructor ") + at)] += 1;
      return true;
    }
    // A built creature is readable only when this level supplied the balance
    // rows and monster level used by Creature_setStats.
    if (read->hit_points && built_.count(obj->addr) && !stats_built_.count(obj->addr))
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
    const bool dialog_cmd = dialog_command(kRows[i].name);
    out.emplace(kRows[i].name, [this, i, function, documented, sync_seed, needs_tables, entrance, cflag, dialog_cmd](
                                   amx::Machine &machine, const std::vector<amx::Cell> &params)
                                   -> std::optional<amx::Address> {
      if (mission1_) {
        // Only the addressed static calls in the audited handoff are admitted.
        // An ignored return never licenses skipping an unknown side effect.
        if (machine.cip >= 8 && mission1_->sites.count(std::uint32_t(machine.cip - 8)))
          ++mission1_sites_reached_[std::uint32_t(machine.cip - 8)];
        if (!mission1_->allows(machine, kRows[i].name, params) ||
            std::string(kRows[i].name) == "trigger" ||
            std::string(kRows[i].name) == "autoPropSetState" ||
            std::string(kRows[i].name) == "cutScene" ||
            std::string(kRows[i].name) == "callScript") {
          ++counts_.refused[i];
          ++counts_.stopped[i];
          throw RefusedResult("Mission 1: untraced script call or effect body");
        }
      }
      // The dialog built-ins run only while dialog is attached; item and quest
      // handlers retain their own routing and refusal rules.
      const bool dialog_route = dialog_cmd && dialog_attached_;
      // Mission-2 research 047adfe leaves newWorld(0)'s destination
      // Unknown. Address zero can contain a readable string in an AMX
      // image; that is not evidence for the null call's runtime target.
      // Do not dispatch it, nor the inferred boss-music path, in this mode.
      if (complete_commands_ &&
          ((std::strcmp(kRows[i].name, "newWorld") == 0 && machine.cell(machine.params + 4) == 0) ||
           (inferred_route(kRows[i].name) &&
            machine.cell(machine.params + (cflag ? 8 : 4)) == 0))) {
        counts_.callee_refusals += 1;
        refuse(i, machine, cflag || selected_form(kRows[i].name)
                              ? "boss-music playback is inferred"
                              : "null destination is Unknown in mission-2 research");
        return 0;
      }
      if (!player_currents_known_ &&
          needs_player_current(kRows[i].name,
                               static_cast<std::int32_t>(variables_->memory.word(script_items::G_NUM_PLAYERS)),
                               std::strcmp(kRows[i].name, "checkPlayerAlive") == 0 &&
                                   machine.cell(machine.params + 4) == 0)) {
        counts_.unknown_player_current_refusals += 1;
        refuse(i, machine);
        return 0;
      }
      if (!documented && !dialog_route && !quests::route_undocumented(kRows[i].name, *campaign_) &&
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
        // Attached trigger preflight refuses the command itself, even when
        // the script discards its result. No later entry effects may run.
        if (dialog_attached_ && std::strcmp(kRows[i].name, "trigger") == 0) {
          counts_.refused[i] += 1;
          counts_.stopped[i] += 1;
          throw RefusedResult("refused trigger");
        }
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
      // With a dialog attached, the words the binder and the machine share
      // are copied in before and back after every command.
      struct DialogSync {
        Host &host;
        bool on, was;
        DialogSync(Host &h, bool attached, bool command) : host(h), on(attached), was(h.in_dialog_command_) {
          if (on) {
            host.dialog_push();
            host.in_dialog_command_ = was || command;
          }
        }
        ~DialogSync() {
          if (on) {
            host.in_dialog_command_ = was;
            host.dialog_pull();
          }
        }
      } dialog_sync(*this, dialog_attached_, dialog_route);
      // research-mission-unlocks@950b2538 docs/spec/mission-unlocks.md:88-113.
      // Execute the supplied original action through the production binder; only
      // the documented (mission, on) write is admitted in this bounded dialog.
      if (hub_action_running_ && std::strcmp(kRows[i].name,"setMissionAvailable")==0 &&
          (hub_action_mission_==0 || machine.cell(machine.params+4)!=hub_action_mission_ ||
           machine.cell(machine.params+8)!=1)) {
        counts_.executed[i]-=1;
        counts_.refused[i]+=1;
        counts_.stopped[i]+=1;
        throw RefusedResult("refused hub availability write outside documented offer");
      }
      // research-hub-npcs@fbbfe65 docs/hub-services.md:112-142: a live hub /
      // hub2 dialog action executes only setMissionAvailable, addQuest, setv,
      // changeSwoop and activateStore, each as its researched effect (no item,
      // gold or other write). Anything else stops the action before it runs.
      if (hub_effect_) {
        const std::string native = kRows[i].name;
        const auto arg = [&](int k) { return machine.cell(machine.params + 4 + 4 * k); };
        const auto &effect = *hub_effect_;
        auto &used = hub_effect_used_;
        std::string why;
        if (!hub_dialog::action_native(native))
          why = native + " is outside the five researched dialog-action natives";
        else if (native == "setMissionAvailable") {
          if (!effect.avail || arg(0) != effect.avail || arg(1) != 1 || used.avail++)
            why = "availability write differs from the researched effect";
        } else if (native == "addQuest") {
          if (!effect.quest || used.quest++)
            why = "quest add differs from the researched effect";
        } else if (native == "changeSwoop") {
          if (!effect.store || arg(0) != 0 || used.swoop++)
            why = "swoop write differs from the researched effect";
        } else if (native == "activateStore") {
          if (!effect.store || arg(0) != 0 || used.store++)
            why = "store request differs from the researched effect";
        } else if (native == "setv") {
          const auto address = arg(0);
          const bool named = machine.get_addr(address) == amx::ERR_NONE;
          const auto lower = [](std::string t) {
            for (auto &c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return t;
          };
          if (effect.var == hub_dialog::Var::None || !hub_services_ || !named || arg(1) != 1 ||
              lower(machine.get_string(address)) != lower(hub_services_->variable(effect.var)) || used.var++)
            why = "variable write differs from the researched effect";
        }
        if (!why.empty()) {
          counts_.executed[i] -= 1;
          counts_.refused[i] += 1;
          counts_.stopped[i] += 1;
          dialog_counts_.hub_service_refused[why] += 1;
          throw RefusedResult("refused hub dialog action: " + why);
        }
      }
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

// A routed command whose callee research did not execute for this state, or
// a text command the port refuses. Checked before the binder runs, so a
// refused command stores nothing.
bool Host::callee_unknown(std::size_t index, amx::Machine &machine) {
  const std::string name = kRows[index].name;
  auto &memory = variables_->memory;
  const auto creature = [&](const objects::GameObject *obj) {
    return obj && (memory.u32(obj->addr + 0xc) & 0x20u) != 0;
  };
  if (text_command(name)) {
    const auto reason = text_refusal(name, machine);
    if (reason)
      counts_.text_refusals[*reason] += 1;
    return reason.has_value();
  }
  if (name == "trigger" && dialog_attached_) {
    // The binder calls slot 3 on every named object (limit 100). Only the
    // documented Creature_trigger classes have a dialog route. Check the
    // whole set before calling any slot: an unknown target has no answer.
    const auto found = named(machine, 100);
    if (!found)
      return true;
    for (const auto *obj : *found)
      if (!dialog_trigger_supported(obj->addr) &&
          !(hub_ring(obj->addr) && machine.cell(machine.params + 8) == 0))
        return true;
    return false;
  }
  if (name == "givePlayerRandom") {
    // The stack's +0x00..+0x0f after ten failed generations are Unknown in
    // item-drops.md section 6. Refuse before its first random draw.
    return true;
  }
  if (name == "charmMonster" || name == "followPlayer") {
    // Creature_setCharm reaches handle_clear of +0x234 past these gates.
    // Research d4987fd executed it on a live target (docs/strings.md 5): it
    // unlinks this creature's nodes and their partners only. The host runs
    // it on its own handle pool, so a live word must be one of its cells.
    const auto found = named(machine, 0x200);
    if (!found)
      return true;
    for (const auto *obj : *found) {
      if (!creature(obj) || memory.s16(obj->addr + 0x312) == -1 || (memory.u32(obj->addr + 0xc) & 0x40u))
        continue;
      if (memory.u32(obj->addr + 0x234) != 0 && !target_cell(obj->addr, false))
        return true;
    }
    return false;
  }
  return false;
}

std::optional<Host::LevelLoadGlobals> Host::level_load_globals() const {
  if (!stats_ready_) return std::nullopt;
  const auto difficulty = static_cast<std::int32_t>(variables_->memory.u32(script_items::G_DIFFICULTY));
  // monster_level_ was computed for difficulty 1 (set_level_stats).
  if (difficulty != 1) return std::nullopt;
  return LevelLoadGlobals{monster_level_, difficulty};
}
void Host::bind_monster_level(std::uint32_t raw) {
  variables_->memory.word(0x4f714c, raw);
  monster_level_bound_ = true;
}
void Host::bind_level_name(const std::string &) {
  // The allocation size of 0x41b721 is not pinned; copying any caller string
  // there risks overwriting unrelated emulated memory.
  throw script_items::Refused("level-name global capacity is unknown");
}
void Host::set_music_volume(std::uint32_t bits) { audio_->set_music_volume(bits); }
void Host::set_fade_level(std::uint32_t level) { variables_->memory.word(script_effects::G_FADE, level); }
std::uint32_t Host::fade_level() const { return variables_->memory.word(script_effects::G_FADE); }

// The string arguments the binder will copy (params + 4 + 4k). A refused
// address would make the binder reuse a remembered string slot, which the
// pre-check cannot see, so it refuses.
namespace {
std::optional<std::string> arg_string(amx::Machine &machine, unsigned k) {
  const auto address = machine.cell(machine.params + 4 + 4 * static_cast<amx::Cell>(k));
  if (machine.get_addr(address) != amx::ERR_NONE)
    return std::nullopt;
  return machine.get_string(address);
}
std::uint32_t arg_word(amx::Machine &machine, unsigned k) {
  return static_cast<std::uint32_t>(machine.cell(machine.params + 4 + 4 * static_cast<amx::Cell>(k)));
}
} // namespace

// The gate inputs the binder's memory holds: a dialog sets g_hudSuppressed,
// a local menu, the load screen and the menu flag are binder words.
void Host::refresh_hud_inputs() {
  auto &memory = variables_->memory;
  auto &in = hud_->inputs;
  in.suppressed = memory.s8(strings::G_HUD_SUPPRESSED) != 0;
  in.local_menu = memory.s8(LOCAL_MENU_WORD) != 0;
  in.loadscreen = memory.s8(LOADSCREEN_WORD) != 0;
  in.menu_open = memory.u32(MENU_OPEN_WORD) != 0;
  in.network = memory.s8(NETWORK_WORD) != 0;
}

// The words binder handlers read back: helpMessageActive reads the open flag.
void Host::mirror_hud() {
  auto &memory = variables_->memory;
  memory.w8(strings::G_HELP_OPEN, hud_->help_open ? 1 : 0);
  memory.w32(strings::G_HELP_CLOSE_DELAY, static_cast<std::uint32_t>(hud_->help_close_delay));
  memory.w32(strings::G_HELP_TITLE, hud_->help_title);
  memory.w32(strings::G_HELP_BODY, hud_->help_body);
  memory.w8(strings::G_BUST_SHOWN, hud_->bust_shown);
  memory.w8(strings::G_COMPASS_SHOWN, hud_->compass_shown);
  memory.w16(strings::G_MSG_FRAMES, hud_->msg_frames);
  memory.w8(strings::G_MSG_FORCE, hud_->msg_force);
}

std::optional<std::string> Host::text_refusal(const std::string &name, amx::Machine &machine) const {
  if (!has_text())
    return std::string("no string table");
  const auto unreadable = std::string("string argument not readable");
  if (name == "setInfo") {
    const auto key = arg_string(machine, 0);
    if (!key)
      return unreadable;
    return text_commands_->check_set_info(*key);
  }
  if (name == "addHelpMessage" || name == "addHelpMessageF") {
    if (!chime_group_)
      return std::string("no help chime resource");
    if (name == "addHelpMessage")
      return arg_string(machine, 0) && arg_string(machine, 1) ? std::nullopt : std::optional<std::string>(unreadable);
    const auto key1 = arg_string(machine, 0), key2 = arg_string(machine, 2);
    if (!key1 || !key2)
      return unreadable;
    return text_commands_->check_help_f(*key1, arg_word(machine, 1), *key2, arg_word(machine, 3));
  }
  if (name == "showBust") {
    const auto texture = arg_string(machine, 0), key = arg_string(machine, 1);
    if (!texture || !key)
      return unreadable;
    return text_commands_->check_bust(*texture, *key);
  }
  if (name == "showCompass") {
    const auto object = arg_string(machine, 0), key = arg_string(machine, 1);
    if (!object || !key)
      return unreadable;
    // The object's position words are the binder's; a stub has them too.
    return text_commands_->check_compass(*object, *key);
  }
  return std::nullopt;
}

bool Host::register_text(const std::vector<std::uint8_t> &uni) {
  try {
    text_->register_table(uni, text_->next_base());
    return true;
  } catch (const strings::Refused &) {
    return false;
  }
}

void Host::set_help_chime(std::uint8_t group) {
  chime_group_ = group;
  sound_->groups[CHIME_SEQUENCE] = group;
}

Cell *Host::target_cell(std::uint32_t creature, bool create) {
  const auto word = variables_->memory.u32(creature + 0x234);
  auto found = target_cells_.find(creature);
  if (found != target_cells_.end()) {
    auto *cell = found->second.get();
    // The word is the first node of the cell's list (or 0 for an empty one).
    const auto *first = cell->next;
    const auto expect = first && first->kind == LinkKind::Node
                            ? HandlePool::NODE_BASE +
                                  HandlePool::NODE_SIZE *
                                      static_cast<std::uint32_t>(static_cast<const Node *>(first)->index)
                            : 0u;
    if (!create && expect != word)
      return nullptr;
    return cell;
  }
  if (!create)
    return nullptr;
  return target_cells_.emplace(creature, std::make_unique<Cell>()).first->second.get();
}

std::uint32_t Host::link_target(std::uint32_t creature, std::uint32_t target) {
  auto *obj = variables_->obj_at(target);
  if (!obj)
    throw amx::HostFault("link_target: no such target object");
  auto *cell = target_cell(creature, true);
  auto *node = variables_->world->handles.link(*cell, obj->heads[0], obj);
  const auto word = HandlePool::NODE_BASE + HandlePool::NODE_SIZE * static_cast<std::uint32_t>(node->index);
  variables_->memory.w32(creature + 0x234, word);
  return word;
}

strings::HelpFrame Host::help_task(bool confirm) {
  refresh_hud_inputs();
  const bool was_paused = hud_->paused;
  const auto frame = hud_->help_task(confirm);
  auto &memory = variables_->memory;
  // help_task stores g_paused and 0x44fa80 while the box is up offline, and
  // clears them on close (docs/strings.md 3.3).
  if (hud_->paused != was_paused || frame.closed) {
    memory.w32(PAUSED_WORD, hud_->paused ? 1u : 0u);
    memory.w8(strings::G_HELP_PAUSE_BYTE, hud_->help_pause_byte ? 1 : 0);
  } else if (hud_->paused) {
    memory.w32(PAUSED_WORD, 1u);
    memory.w8(strings::G_HELP_PAUSE_BYTE, 1);
  }
  if (frame.closed)
    memory.w32(strings::G_MENU_INPUT_LOCKOUT, static_cast<std::uint32_t>(hud_->menu_input_lockout));
  mirror_hud();
  if (frame.run_post_action && machine_ && !faulted_) {
    machine_->budget = session::SCRIPT_BUDGET;
    try {
      variables_->run_post_action();
    } catch (const std::exception &error) {
      remember(error);
    }
    classify();
  }
  return frame;
}

std::uint32_t Host::scene(const std::string &name,
                          const std::vector<builtins::Variables::ServiceValue> &args) {
  auto &fx = *script_fx_;
  auto &memory = variables_->memory;
  if (name == "net_closeLocalMenus") {
    // The researched callee has no offline effect. Its network path needs
    // menu/vehicle services this host does not provide.
    if (memory.read(0x402791))
      throw amx::HostFault("network menu close is not available in this offline host");
    return 0;
  }
  if (name == "hud_setFade") {
    if (args.empty())
      throw amx::HostFault("hud_setFade argument count");
    fx.set_fade(word_of(args[0]));
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
    const auto creature = word_of(args[0]);
    const auto flags = memory.u32(creature + 0xc);
    const bool stores = memory.s16(creature + 0x312) != -1 && (flags & 0x20u) && !(flags & 0x40u);
    Cell *cell = nullptr;
    if (stores && memory.u32(creature + 0x234) != 0) {
      cell = target_cell(creature, false);
      if (!cell)
        throw amx::HostFault("Creature_setCharm target word is not a host handle cell");
    }
    fx.set_charm(creature, word_of(args[1]), word_of(args[2]));
    // handle_clear of the live target (research d4987fd docs/strings.md 5,
    // executed): this creature's nodes and their partners go; the target and
    // every other handle to it stay.
    if (cell)
      variables_->world->handles.clear(*cell);
    return 0;
  }
  if (name == "text_lookup") {
    if (args.empty())
      throw amx::HostFault("text_lookup argument count");
    return text_->lookup(string_of(args[0])).address;
  }
  if (name == "hud_showMessage") {
    if (args.size() < 4)
      throw amx::HostFault("hud_showMessage argument count");
    refresh_hud_inputs();
    hud_->show_message(text_commands_->resolve(word_of(args[0])), word_of(args[1]),
                       static_cast<std::int32_t>(word_of(args[2])), word_of(args[3]));
    mirror_hud();
    return 0;
  }
  if (name == "wstr_format") {
    // addHelpMessageF: the looked-up text is the format, f12 its integer.
    if (args.size() < 10)
      throw amx::HostFault("wstr_format argument count");
    const auto at = word_of(args[1]);
    std::array<std::uint32_t, 8> fargs{};
    for (std::size_t i = 0; i < 8; ++i)
      fargs[i] = word_of(args[2 + i]);
    const strings::CStr no_c = [](std::uint32_t) -> std::vector<std::uint8_t> {
      throw amx::HostFault("%s in a help text dereferences a float argument");
    };
    const strings::WStr no_w = [](std::uint32_t) -> strings::Units {
      throw amx::HostFault("%u in a help text dereferences a float argument");
    };
    const auto out = strings::wstr_format(
        [this, at](std::size_t k) { return text_->unit_at(at + static_cast<std::uint32_t>(2 * k)); },
        fargs, no_c, no_w);
    formatted_.push_back(strings::visible(out.written));
    return 0;
  }
  if (name == "wstr_copy") {
    if (args.size() < 2 || formatted_.empty())
      throw amx::HostFault("wstr_copy without a formatted text");
    const auto dst = word_of(args[0]);
    auto text = std::move(formatted_.front());
    formatted_.pop_front();
    if (dst == strings::HELP_F_TITLE && text.size() <= strings::HELP_F_TITLE_LIMIT)
      hud_->help_f_title = std::move(text);
    else if (dst == strings::HELP_F_BODY && text.size() <= strings::HELP_F_BODY_LIMIT)
      hud_->help_f_body = std::move(text);
    else
      throw amx::HostFault("wstr_copy to an unmodelled or overflowing buffer");
    return 0;
  }
  if (name == "hud_showHelpMessage") {
    if (args.size() < 3)
      throw amx::HostFault("hud_showHelpMessage argument count");
    if (!chime_group_)
      throw amx::HostFault("hud_showHelpMessage without the chime resource");
    refresh_hud_inputs();
    hud_->events.clear();
    const bool opened = hud_->show_help(static_cast<std::int32_t>(word_of(args[0])), word_of(args[1]),
                                        word_of(args[2]));
    if (opened) {
      // res_find(fx.lmp, butn_message.anm), sound_playOnObject(seq, 0, 1.0),
      // then sound_setPlaysWhileFrozen(handle): +0x3b of the chosen slot.
      sound_->frozen = 0;
      const auto played = sound_->play_on_object(CHIME_SEQUENCE, nullptr, 0x3f800000u);
      if (played.slot)
        sound_->slots[*played.slot].frozen = 1;
      // ui_startPulse(0): g_uiPulse[0] = 2.0 (research-ui docs/mainloop.md),
      // a presentation value the caller decays per displayed frame.
      ui_pulse_starts_ += 1;
      ui_ramps_.pulse[0] = ui_text_gaps::uiStartPulse();
    }
    mirror_hud();
    return 0;
  }
  if (name == "hud_setBust") {
    if (args.size() < 4)
      throw amx::HostFault("hud_setBust argument count");
    refresh_hud_inputs();
    hud_->set_bust(string_of(args[0]), text_commands_->resolve(word_of(args[1])), word_of(args[2]),
                   word_of(args[3]));
    mirror_hud();
    return 0;
  }
  if (name == "hud_setCompassTarget") {
    if (args.size() < 2)
      throw amx::HostFault("hud_setCompassTarget argument count");
    refresh_hud_inputs();
    hud_->set_compass(vec_of(args[0]), text_commands_->resolve(word_of(args[1])));
    mirror_hud();
    return 0;
  }
  if (name == "virtual") {
    if (args.size() < 3)
      throw amx::HostFault("virtual argument count");
    if (word_of(args[0]) == 3u) {
      const auto *portal = variables_->find_first("portal");
      const auto *object = variables_->obj_at(word_of(args[1]));
      // The named portal, or the hub ring record (research-hub-npcs@fbbfe65
      // docs/hub-services.md:395-401): the request is counted, not rendered.
      if (!object || word_of(args[2]) != 0u || (portal != object && !hub_ring(object->addr)))
        throw amx::HostFault("unsupported portal virtual request");
      ++portal_trigger_requests_;
      return 0;
    }
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
  seed_gate_observed_ = false;
  stats_built_.clear();
}

level_flow::ChangeResult Host::change_level(const level_flow::ChangeOptions &options) {
  mirror();
  auto result = quests::change_level(*campaign_, flow_, options, {});
  quests::mirror_to_variables(*campaign_, *variables_, player_currents_known_);
  return result;
}
bool Host::tick_mission_menu(int button, const quests::MenuInputs &inputs) {
  mirror();
  const bool launch = campaign_->tick_menu(button, inputs);
  const auto &menu = campaign_->menu();
  // State 0 stores pause 1. The negative closing step stores 0
  // (mission-unlocks.md:156-158, ref_mission_lifecycle.py state 4).
  if (menu.paused)
    variables_->memory.word(PAUSED_WORD, 1u);
  else
    variables_->memory.word(PAUSED_WORD, 0u);
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
  // Same-process load keeps the live run task. A host that has never
  // registered one stays unregistered: the image does not carry the registry.
  // research-mission-unlocks@e5c6176 docs/spec/mission-unlocks.md:252-260.
  const bool run = campaign_->menu_run_registered();
  auto next = quests::Campaign::load(bytes);
  next.retain_menu_run(run);
  *campaign_ = std::move(next);
  quests::restore_variables(*campaign_, *variables_, player_currents_known_);
}

bool Host::load(const amx::ByteVector &image) {
  mission1_.reset();
  mission1_sites_reached_.clear();
  hub_offers_.reset();
  hub_offer_start_=-1;
  hub_opening_=false;
  hub_action_running_=false;
  hub_action_mission_=0;
  hub_services_.reset();
  hub_services_start_=-1;
  hub_services_mismatch_.clear();
  hub_effect_.reset();
  machine_.reset();
  variables_->script_machine.reset();
  level_started_ = false;
  loading_deferred_complete_ = false;
  waiting_ = false;
  faulted_ = false;
  refusal_stopped_ = false;
  last_refusal_.clear();
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
    mission1_ = mission1_scripts::Contract::inspect(*machine);
    machine_ = std::move(machine);
    variables_->script_machine = machine_;
    return true;
  } catch (const std::exception &error) {
    if (mission1_ || std::string(error.what()).find("Mission 1") == 0) last_fault_ = error.what();
    mission1_.reset();
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
    if (mission1_) {
      if (!exe_) throw std::runtime_error("Mission 1 census needs the selected executable");
      mission1_->validate_objects(records, *exe_);
    }
    if (exe_)
      return adopt_classified(records);
    record_addresses_.assign(records.size(), 0);
    for (std::size_t index = 0; index < records.size(); ++index) {
      const auto &record = records[index];
      auto props = std::make_shared<std::vector<std::string>>(record.props);
      // No factory class: nothing says what the record is. Not registered.
      if (record.cls.empty()) {
        counts_.objects_refused += 1;
        continue;
      }
      const auto obj = variables_->new_object({}, 0, record.position, record.angle, record.cls, props);
      record_addresses_[index] = obj->addr;
      stubs_.insert(obj->addr);
      counts_.objects_registered += 1;
      counts_.objects_unclassified += 1;
      counts_.load_draws_unmodelled += 1;
    }
    return true;
  } catch (const std::exception &error) {
    if (mission1_) last_fault_ = std::string(error.what()).find("Mission 1") == 0 ? error.what() : "Mission 1 object adoption refused";
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
  if (mission1_) {
    const auto offset = mission1_->offset_of(name);
    if (offset) return run_mission1_entry(*offset);
    remember(RefusedResult("Mission 1: entry body outside addressed handoff"));
    return Stop::Refused;
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
Stop Host::run_prestart() {
  if (mission1_) {
    const auto at = mission1_->offset_of("preStart");
    return at ? run_mission1_entry(*at) : Stop::Missing;
  }
  return enter_level("preStart");
}

// start, then the load's post action (game_loadLevel 0x1e60b8), then one
// script_runDeferred outside any tick (mainloop_runSession 0x322e6c;
// docs/mainloop.md step 7). That run promotes what start queued, so it runs
// on the first ready tick.
Stop Host::run_start() {
  loading_deferred_complete_ = false;
  const auto refused_before = counts_.refused;
  if (mission1_) {
    if (!mission1_->objects_validated) {
      count_refusal("Mission 1 object census or links not validated");
      return Stop::Refused;
    }
    const auto at = mission1_->offset_of("start");
    return at ? run_mission1_entry(*at) : Stop::Missing;
  }
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
    // One returned pass, including back-to-front promotion, is sufficient.
    // research-loading-readiness@9dd49046:docs/loading-readiness.md:11-37.
    loading_deferred_complete_ = !faulted_ && !refusal_stopped_ &&
                                 counts_.refused == refused_before;
  } catch (const std::exception &error) {
    remember(error);
  }
  return classify();
}

// All addresses/conditions below: research-loading-readiness@9dd49046,
// docs/loading-readiness.md (line ranges beside each operation).
Host::LoadingReadiness Host::loading_readiness(const std::string &destination,
                                                bool report_arrival_limits) {
  LoadingReadiness out;
  out.deferred_complete = loading_deferred_complete_ && !faulted_ && !refusal_stopped_;
  if (!out.deferred_complete)
    out.refusals.push_back("Deferred loading pass absent, faulted or refused commands.");
  const auto &m = variables_->memory;
  // 91-95,140-152: explicit offline setup AND the live mode byte equals zero.
  // Sparse memory's default zero alone cannot establish a session's mode.
  if (loading_offline_session_ && m.read(0x402791) == 0) {
    out.peers_ready = true;
    out.save_gate_ready = true; // gate bypass only; no persistence claim
  } else {
    out.refusals.push_back("Peer readiness unavailable: session not explicitly offline.");
    out.refusals.push_back("Network save gate unavailable: card service unbound.");
  }
  // 117-138: exact identifiers come from the selected executable, never literals.
  if (!exe_ || !exe_->string || destination.empty()) {
    out.refusals.push_back("Store applicability unavailable: executable or destination absent.");
  } else {
    try {
      bool match = false;
      for (const auto address : {0x554678u, 0x554680u, 0x554688u, 0x554698u}) {
        const auto identifier = exe_->string(address);
        if (identifier.empty()) throw std::runtime_error("empty stock identifier");
        match = match || destination == identifier;
      }
      if (!match) out.stores_complete = true;
      else out.refusals.push_back("Applicable transition stock generation/RNG unavailable.");
    } catch (const std::exception &) {
      out.refusals.push_back("Store applicability unavailable: identifier read failed.");
    }
  }
  // 41-53: only explicitly published runtime fields may establish null players
  // or empty equipment. Default sparse zeros and executable initial bytes cannot.
  const auto published = [&m](std::uint32_t address, std::size_t size) {
    return m.published(address,size);
  };
  bool known = published(0x4d8ae0, 4), pending = false;
  // The host's supported session is single-player only; no guessed record bound.
  if (known && loading_offline_session_ && m.word(0x4d8ae0) == 1) {
    const auto slot = 0x4dcec8u;
    known = published(slot, 4);
    if (known && m.word(slot)) {
      const auto player = m.word(slot);
      known = published(player + 0x3c8, 4);
      // This host only publishes record 0, never arbitrary unchecked offsets.
      known = known && m.word(player + 0x3c8) == 0;
      for (unsigned i = 0; known && i < 17; ++i) {
        const auto at = 0x4d8af0u + 0x43f0u + i * 4;
        known = published(at, 4);
        if (!known || !m.word(at)) continue;
        const auto item = m.word(at);
        known = published(item, 4);
        if (!known) break;
        const auto definition = m.word(item);
        known = definition && published(definition + 0x1c, 1);
        if (!known) break;
        const auto category = m.read(definition + 0x1c);
        // 48-50: slot 3's selected-definition skip precedes category dispatch.
        // Its specialized selection is unbound, so refuse rather than guess.
        if (i == 3) { known = false; break; } // selection remains unverified (r2:68-73)
        if (category >= 5 && category <= 8) { pending = true; continue; }
        // 52-53: environment guard requires both observed runtime globals.
        if (category == 0 || category == 2 || category == 3) {
          if (published(0x41b69d, 1) && published(0x44fe08, 4) &&
              m.read(0x41b69d) == 0 && (m.word(0x44fe08) & 0x10000000u)) continue;
        }
        // d6f035fa:docs/loading-arrival-r2.md:75-97: specialized scanner routes.
        // Slot 3 selection above remains unknown; other routes require actual
        // formatter output and published environment/definition fields.
        const auto cache = loading_equipment::Runtime::cache(i, 0);
        if (cache && published(0x4f85b0,4) && m.word(0x4f85b0) != 0 &&
            published(definition+0x30,4) && m.word(definition+0x30) != 0 &&
            equipment_loading_->format) {
          try {
            auto identity = equipment_loading_->format(definition);
            if (identity.size() < 4) throw amx::HostFault("equipment identifier too short");
            identity.resize(identity.size()-4);
            pending = !equipment_loading_->lookup(*cache, identity).ready || pending;
            continue;
          } catch (const std::exception &) { known = false; break; }
        }
        // 9dd49046:docs/loading-readiness.md:55-79: generic lookup still unbound.
        known = false;
      }
    }
  } else known = false;
  if (known) out.equipment_pending = pending;
  else out.refusals.push_back("Equipment readiness unavailable: runtime player/slot/model publication or specialized I/O unbound.");
  // 366b65a:docs/loading-arrival-r4.md:184-207. Naming these limits does not
  // clear deferred_complete or publish equipment. The live route ordinal is
  // not selected here, so the observed prefix is not applied.
  if (report_arrival_limits) {
    out.refusals.push_back("normal deferred census");
    out.refusals.push_back("route state");
    out.refusals.push_back("object load");
    out.refusals.push_back("resource bootstrap");
    out.refusals.push_back("cutscene runtime");
  }
  return out;
}

// The Script task runs on every simulated tick (docs/script.md 2.4 and 2.6),
// also with an empty queue: it still flips the halves.
void Host::on_ready_tick() {
  // HUD Run (priority 50) runs before Script (125) in the tick: hud_tickMessage
  // and the bust/compass countdowns (docs/strings.md 3.2, 3.4; the task table
  // of docs/mainloop.md).
  refresh_hud_inputs();
  hud_->tick_message();
  hud_->tick_widgets();
  mirror_hud();
  if (mission1_) {
    // No continuous original/native script run is in the evidence. Deferred
    // work can be inspected via the explicitly addressed entry API only.
    counts_.build_refusals.emplace("Mission 1 continuous script execution untraced", 1);
    return;
  }
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

Stop Host::run_mission1_entry(std::uint32_t offset) {
  if (!mission1_ || !machine_ || !mission1_->entries.count(offset)) return Stop::Refused;
  if (!mission1_->objects_validated &&
      !(mission1_->level == mission1_scripts::Level::Strife && offset == 0x568))
    return Stop::Refused;
  for (const auto &sw : mission1_->switches)
    if (sw.action_entry == offset && variables_->quest_get(sw.persistent) != 0)
      return Stop::Refused; // the paired key is !getv, not a repeatable increment
  if (faulted_ && !refusal_stopped_) return Stop::Fault;
  // A new, explicit diagnostic invocation may follow a bounded refusal. This
  // does not resume a stopped script or drain its deferred/post-action queue.
  faulted_ = refusal_stopped_ = false;
  last_fault_.clear();
  // The handoff proves selected cells, not the intervening entry bodies.
  // Apply only those cells through the existing binder, never execute unknown
  // bytecode between two documented call sites.
  for (const auto &sw : mission1_->switches) {
    if (offset == sw.key_entry) {
      auto stop = mission1_site(sw.key_entry + 0x2c);
      if (stop == Stop::Complete) last_value_ = !last_value_;
      return stop;
    }
    if (offset == sw.action_entry) {
      // INC at action+0x1c addresses local slot zero (audited switch_effects).
      machine_->set_cell(0, std::uint32_t(machine_->cell(0)) + 1u);
      auto stop = mission1_site(sw.action_entry + 0x48);
      return stop == Stop::Complete ? mission1_site(sw.action_entry + 0x74) : stop;
    }
  }
  if (mission1_->level == mission1_scripts::Level::Drundar && offset == 0xa3c) {
    // research-strife-drundar@3216e7b0 docs/strife-drundar.md:214-235,
    // tools/difftest_strife_drundar.py:415-461: getv -> NOT -> JZER.
    // Nonzero saved values branch to 0xb10 without effects. Zero falls
    // through; refuse the unsupported sequence before any mutating effect.
    auto stop = mission1_site(0xa68);
    if (stop != Stop::Complete) return stop;
    if (last_value_ != 0) return Stop::Complete;
    remember(RefusedResult("Mission 1: unsupported door sequence; cutscene completion unestablished"));
    return Stop::Refused;
  }
  remember(RefusedResult("Mission 1: entry body outside addressed effects"));
  return Stop::Refused;
}

bool Host::restore_mission1_counter() {
  if (!mission1_ || !machine_ || mission1_->level != mission1_scripts::Level::Strife) return false;
  machine_->set_cell(0x14c, std::uint32_t(variables_->quest_get(mission1_->counter_key)));
  ++mission1_sites_reached_[0x5e8];
  return true;
}
std::int32_t Host::mission1_local_count() const {
  if (!mission1_ || !machine_) throw std::runtime_error("No Mission 1 addressed contract");
  return machine_->cell(mission1_->level == mission1_scripts::Level::Strife ? 0x14c : 0);
}

Stop Host::mission1_site(std::uint32_t offset) {
  if (!mission1_ || !machine_ || !mission1_->objects_validated || !mission1_->sites.count(offset))
    return Stop::Refused;
  if (faulted_ && !refusal_stopped_) return Stop::Fault;
  const auto &site = mission1_->sites.at(offset);
  if (site.arguments.empty()) return Stop::Refused; // needs the original VM's computed counter
  faulted_ = refusal_stopped_ = false;
  last_fault_.clear();
  auto &m = *machine_;
  // Binder scratch is temporary. Global script/campaign effects survive; VM
  // stack, heap and continuation do not become a fabricated script resume.
  struct Frame {
    amx::Machine &m;
    amx::Address hea, stk, frm, cip, params;
    explicit Frame(amx::Machine &v) : m(v), hea(v.hea), stk(v.stk), frm(v.frm), cip(v.cip), params(v.params) {}
    ~Frame() { m.hea=hea; m.stk=stk; m.frm=frm; m.cip=cip; m.params=params; }
  } frame(m);
  try {
    const auto allocation = m.allot(site.arguments.size()+1);
    if (allocation.first != amx::ERR_NONE || !allocation.second) throw amx::HostFault("Addressed binder scratch refused");
    const auto at = *allocation.second;
    m.set_cell(at, site.arguments.size()*4);
    for (std::size_t i=0; i<site.arguments.size(); ++i) m.set_cell(at+4+4*i, std::uint32_t(site.arguments[i]));
    m.cip = offset+8;
    m.budget = session::SCRIPT_BUDGET;
    const auto names = m.native_names();
    const auto found = std::find(names.begin(), names.end(), site.command);
    if (found == names.end()) throw amx::HostFault("Addressed native missing");
    const auto result = m.call_native(found-names.begin(), at);
    if (result.first) throw amx::HostFault("Addressed native returned VM error");
    last_value_ = amx::s32(result.second);
    return Stop::Complete;
  } catch (const std::exception &e) {
    remember(e);
    return stopped();
  }
}
Stop Host::restore_mission1_addressed() {
  if (!mission1_ || !mission1_->objects_validated) return Stop::Refused;
  if (faulted_ && !refusal_stopped_) return Stop::Fault;
  if (mission1_->level == mission1_scripts::Level::Strife) {
    restore_mission1_counter();
    const std::uint32_t sites[4][4] = {{0x6b0,0x6dc,0x708,0x734}, {0x794,0x7c0,0x7ec,0x818},
                                      {0x878,0x8a4,0x8d0,0x8fc}, {0x95c,0x988,0x9b4,0x9e0}};
    for (unsigned group=0; group<4; ++group) {
      const bool flag = variables_->quest_get(mission1_->stage_keys[group]) != 0;
      if (group == 0 ? !flag : flag) for (const auto at : sites[group]) {
        const auto stop = mission1_site(at);
        if (stop != Stop::Complete) return stop;
      }
    }
  } else {
    if (variables_->quest_get(mission1_->door_flag)) {
      auto stop = mission1_site(0x374);
      if (stop != Stop::Complete) return stop;
      stop = mission1_site(0x3a8);
      if (stop != Stop::Complete) return stop;
    }
    for (const auto &site : mission1_->restore_keys) if (variables_->quest_get(site.second)) {
      const auto stop = mission1_site(site.first);
      if (stop != Stop::Complete) return stop;
    }
  }
  return Stop::Complete;
}

Stop Host::mission1_count_gate() {
  if (!mission1_ || !mission1_->objects_validated || mission1_->level != mission1_scripts::Level::Drundar)
    return Stop::Refused;
  // EQ.C.pri at 0x870, literal eight. No >= or inferred restore count.
  if (mission1_local_count() != 8) return Stop::Refused;
  auto stop = mission1_site(0x89c);
  return stop == Stop::Complete ? mission1_site(0x8dc) : stop;
}
Stop Host::mission1_bridge_request(unsigned index) {
  if (!mission1_ || mission1_->level != mission1_scripts::Level::Strife || index >= 3) return Stop::Refused;
  const std::uint32_t at[] = {0x1c90,0x1df0,0x1ed4};
  return mission1_site(at[index]);
}

Stop Host::run(const std::string &entry) {
  const auto stop = enter(entry, false);
  if (stop != Stop::Complete)
    return stop;
  note_queue();
  return waiting_ ? Stop::Wait : Stop::Complete;
}

Stop Host::run_object_entry(std::size_t record_index, const std::string &entry,
                            unsigned active_player) {
  if (active_player >= 4 || record_index >= record_addresses_.size() ||
      record_addresses_[record_index] == 0)
    return Stop::Refused;
  auto &memory = variables_->memory;
  const auto previous = memory.word(SELECTED_WORD);
  const auto previous_player = memory.word(0x4d8ae4);
  memory.word(SELECTED_WORD, record_addresses_[record_index]);
  memory.word(0x4d8ae4, active_player);
  try {
    const auto result = run(entry);
    memory.word(SELECTED_WORD, previous);
    memory.word(0x4d8ae4, previous_player);
    return result;
  } catch (...) {
    memory.word(SELECTED_WORD, previous);
    memory.word(0x4d8ae4, previous_player);
    throw;
  }
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
