#pragma once
// The level-script host with the dialog attached (docs/DIALOG.md "Level-script
// host"), on an authored script, an authored objects.ob and an authored .dbin.
#include "champions/amx.hpp"
#include "champions/dialog.hpp"
#include "champions/script_commands.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace dialog_host_tests {
namespace a = champions::simulation::rta::amx;
namespace sc = champions::simulation::rta::script_commands;
namespace dl = champions::simulation::rta::dialog;
using Check = void (*)(bool, const std::string &);

struct Call {
  std::string native;
  std::vector<a::Cell> args;
};
struct Public {
  std::string name;
  std::vector<Call> calls;
  // The value the entry returns; with use_last the last call's answer.
  a::Cell value = 0;
  bool use_last = false;
};
struct Script {
  a::ByteVector image;
  std::vector<std::pair<a::Address, std::string>> strings;
};

inline Script build(const std::vector<Public> &publics, const std::vector<std::pair<a::Address, std::string>> &strings) {
  std::vector<std::string> natives;
  auto native_index = [&](const std::string &name) {
    auto found = std::find(natives.begin(), natives.end(), name);
    if (found == natives.end()) {
      natives.push_back(name);
      return a::Cell(natives.size() - 1);
    }
    return a::Cell(found - natives.begin());
  };
  std::vector<a::AsmLine> program;
  auto ins = [&](const std::string &name, std::vector<a::AsmValue> args = {}) {
    program.push_back(a::AsmInstruction{name, std::move(args), {}, {}});
  };
  for (const auto &p : publics) {
    program.push_back(a::Label{p.name + ":"});
    ins("PROC");
    for (const auto &call : p.calls) {
      for (auto it = call.args.rbegin(); it != call.args.rend(); ++it)
        ins("PUSH.C", {*it});
      ins("PUSH.C", {a::Cell(4 * call.args.size())});
      ins("SYSREQ.C", {native_index(call.native)});
      ins("STACK", {a::Cell(4 * (call.args.size() + 1))});
    }
    if (!p.use_last)
      ins("CONST.pri", {p.value});
    ins("RETN");
  }
  const auto code = a::assemble(program);
  a::ImageOptions options;
  options.data.resize(16384);
  options.natives = natives;
  options.stack_cells = 512;
  for (const auto &p : publics)
    options.publics.emplace_back(p.name, a::Cell(code.labels.at(p.name)));
  return Script{a::build_image(code.cells, options), strings};
}

// objects.ob (level_flow::parse_objects): count, flags, string table offset;
// 20-byte record headers then property offsets ending in 0.
struct Record {
  std::string cls;
  std::uint16_t angle = 0;
  std::array<std::uint32_t, 3> pos{};
  std::vector<std::string> props;
};
inline std::vector<std::uint8_t> objects_ob(const std::vector<Record> &records) {
  std::vector<std::uint8_t> table{0}; // offset 0 is the terminator value
  auto intern = [&](const std::string &s) {
    const auto at = static_cast<std::uint32_t>(table.size());
    table.insert(table.end(), s.begin(), s.end());
    table.push_back(0);
    return at;
  };
  std::vector<std::uint8_t> body;
  auto put = [](std::vector<std::uint8_t> &v, std::uint32_t x, int n) {
    for (int i = 0; i < n; ++i)
      v.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
  };
  for (const auto &r : records) {
    std::vector<std::uint8_t> rec;
    put(rec, intern(r.cls), 4);
    const auto size = 20 + 4 * (r.props.size() + 1);
    put(rec, static_cast<std::uint32_t>(size), 2);
    put(rec, r.angle, 2);
    for (auto w : r.pos)
      put(rec, w, 4);
    for (const auto &p : r.props)
      put(rec, intern(p), 4);
    put(rec, 0, 4);
    body.insert(body.end(), rec.begin(), rec.end());
  }
  std::vector<std::uint8_t> out;
  put(out, static_cast<std::uint32_t>(records.size()), 2);
  put(out, 0, 2);
  put(out, static_cast<std::uint32_t>(8 + body.size()), 4);
  out.insert(out.end(), body.begin(), body.end());
  out.insert(out.end(), table.begin(), table.end());
  return out;
}

inline dl::Wide wide(const std::string &s) { return dl::Wide(s.begin(), s.end()); }

struct World {
  std::map<std::string, dl::Wide> texts;
  sc::DialogServices services() const {
    sc::DialogServices s;
    const auto *t = &texts;
    s.text = [t](const std::string &key) {
      const auto found = t->find(key);
      if (found == t->end())
        throw dl::Refused("synthetic text key missing");
      return found->second;
    };
    s.layout_height = [](const dl::Wide &) { return std::int64_t(dl::LINE); };
    s.shown = [](const dl::Wide &text, std::int64_t, std::int64_t budget, int) {
      return budget < 0 ? std::int64_t(text.size()) : std::min<std::int64_t>(budget, std::int64_t(text.size()));
    };
    return s;
  }
};

// Addresses of the script strings.
constexpr a::Address S_NPC = 400, S_STEP = 600, S_NEXT = 800, S_POST = 1000, S_POSTED = 1200, S_ITEM = 1400,
                     S_TITLE = 1600, S_BODY = 1800;

inline int command_index(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::strcmp(sc::row(i).name, name) == 0)
      return int(i);
  return -1;
}

inline bool load(sc::Host &host, const Script &script, const std::vector<std::uint8_t> &objects) {
  if (!host.load(script.image))
    return false;
  for (const auto &[at, s] : script.strings)
    host.machine()->set_string(at, s, false);
  host.variables().memory.word(0x4d8ae0u, 1u); // one player record for the quest commands
  host.variables().memory.word(0x4f719cu, 1u); // difficulty 1
  host.run_prestart();
  host.adopt_objects(objects.data(), objects.size());
  return true;
}

// Ticks the run task and the Script task, pressing confirm every fourth tick,
// until the conversation and the camera are done.
inline int run_to_close(sc::Host &host, int limit = 4000) {
  int t = 0;
  for (; t < limit && (host.dialog().is_open() || host.dialog().cam_move != 0); ++t) {
    sc::DialogInputs in;
    in.pad = t % 4 == 3 ? 0x40u : 0u;
    host.dialog_tick(in);
    host.on_ready_tick();
  }
  return t;
}

inline void run(Check check, std::string &where) {
  where = "host";
  const std::vector<std::pair<a::Address, std::string>> strings = {
      {S_NPC, "npc"}, {S_STEP, "step"}, {S_NEXT, "nextlvl"}, {S_POST, "post"},
      {S_POSTED, "posted"}, {S_ITEM, "anitem"}, {S_TITLE, "Title"}, {S_BODY, "Body"}};
  const std::vector<Public> publics = {
      {"talk", {{"startDialog", {S_NPC, 1, 0}}}},
      {"nc_zero", {}, 0},
      {"gate_no", {}, 0},
      {"act1", {{"setv", {S_STEP, 7}}, {"addQuest", {S_TITLE, S_BODY}}, {"activateMissionMenu", {}}}},
      {"act_end", {{"newWorldPost", {S_NEXT}}, {"activateStore", {3}}, {"postAction", {S_POST}},
                   {"setMissionAvailable", {1, 1}}}},
      {"post", {{"setv", {S_POSTED, 1}}}},
      {"c_item", {{"playerHasItem", {S_ITEM}}}, 0, true},
  };
  const auto script = build(publics, strings);
  World world;
  for (const char *k : {"t0", "t1", "t2", "c0a", "c0b", "c1", "c2", "nm", "Affirmation", "dialogMore"})
    world.texts[k] = wide(std::string(k) + " text");
  // node 0: c0a (act1, -> 1) and c0b (hidden by gate_no)
  // node 1: condition nc_zero (0): not a start; c1 (act_end, end)
  // node 2: condition nc_missing (no such entry: true): the start; c2 (act1, -> 0)
  std::vector<dl::Node> nodes(3);
  nodes[0].text = "t0";
  nodes[0].voice = "v0";
  nodes[0].choices = {{"c0a", "", "act1", 1}, {"c0b", "gate_no", "", -1}};
  nodes[1].condition = "nc_zero";
  nodes[1].text = "t1";
  nodes[1].choices = {{"c1", "", "act_end", -1}};
  nodes[2].condition = "nc_missing";
  nodes[2].text = "t2";
  nodes[2].choices = {{"c2", "", "act1", 0}};
  for (auto &n : nodes)
    n.raw_choices = std::int32_t(n.choices.size());
  sc::DialogLevel level;
  level.level = "synth";
  level.files["npc.dbin"] = dl::build_dbin(nodes);
  level.stream["v0.vag"] = {0x800, 0x1000};
  const auto objects = objects_ob({{"NPC", 0, {0, 0, 0}, {"name=npc", "showName=\"nm\"", "dialogScript=talk", "dialogOffset=0,0,0,0"}},
                                   {"TestLocation", 0, {0x43480000u, 0, 0}, {"name=far", "showName=nm", "dialogScript=talk"}}});
  // showName keeps its quotes; text_lookup drops them (docs/data-tables.md 6).
  auto services = world.services();
  const auto plain = services.text;
  services.text = [plain](const std::string &key) {
    std::string k;
    for (char c : key)
      if (c != '"')
        k.push_back(c);
    return plain(k);
  };

  // ---- without the dialog the built-ins stay refused ----
  {
    sc::Host host;
    check(load(host, script, objects), "host: script loads");
    host.run_start();
    check(host.run("talk") == sc::Stop::Complete, "host: talk entry runs without the dialog");
    const auto i = command_index("startDialog");
    check(i >= 0 && host.counts().refused[std::size_t(i)] == 1 && host.counts().executed[std::size_t(i)] == 0,
          "host: startDialog refused when no dialog is attached");
  }

  // ---- one whole conversation ----
  sc::Host host;
  check(load(host, script, objects), "host: script loads");
  host.attach_dialog(level, services);
  host.run_start();
  const auto talkers = host.talkers();
  check(talkers.size() == 1, "host: the NPC talks, the TestLocation class is not documented");
  const std::array<champions::simulation::rta::Raw, 3> front{0x41f00000u, 0, 0}; // 30, 0, 0
  const std::array<champions::simulation::rta::Raw, 3> behind{0xc1f00000u, 0, 0};
  const std::array<champions::simulation::rta::Raw, 3> far{0x43480000u, 0x43480000u, 0};
  check(host.use(0, far) == "no candidate", "host: nothing in the use box");
  check(host.use(0, behind) == "behind", "host: Creature_interact's facing test");
  check(host.dialog().mutex == 0, "host: behind leaves the menu mutex free");
  check(host.use(0, front) == "script", "host: the use button runs dialogScript");
  const auto &d = host.dialog();
  check(d.is_open() && d.node && *d.node == 2, "host: a missing condition entry counts as true (node 2 starts)");
  check(d.mutex == 1 && d.hud_suppressed == 1 && d.swoop == 1 && d.cam_move == 1,
        "host: open holds the mutex, suppresses the HUD and starts the camera move");
  check(host.use(0, front) == "dialog open", "host: no interaction while a dialog is open");
  check(d.visible.size() == 1, "host: node 2 offers its one choice");
  const int ticks = run_to_close(host);
  check(!d.is_open() && d.cam_move == 0 && ticks < 4000, "host: the conversation closes through the swoop out");
  // The same conversation with the host's own g_uiFadeIn stepped once per displayed frame
  // (docs/ui-text-gaps.md 1): the fade-gated leave message waits for exactly
  // 1.0. Wave 24's conversation camera can then take additional swoop-out
  // ticks before the end message closes the dialog.
  {
    sc::Host fading;
    check(load(fading, script, objects), "fade host: script loads");
    fading.attach_dialog(level, services);
    fading.run_start();
    check(fading.use(0, front) == "script", "fade host: the use button runs dialogScript");
    int t = 0, steps = -1, steps_at_leave = -1, steps_at_end = -1, steps_when_one = -1;
    for (; t < 4000 && (fading.dialog().is_open() || fading.dialog().cam_move != 0); ++t) {
      sc::DialogInputs in;
      in.pad = t % 4 == 3 ? 0x40u : 0u;
      in.fade = fading.ui_ramps().fade[0];
      const int steps_before = steps;
      if (steps_when_one < 0 && steps >= 0 && in.fade == 0x3f800000u)
        steps_when_one = steps; // the first tick that reads exactly 1.0 after the restart
      fading.dialog_tick(in);
      fading.on_ready_tick();
      if (steps_at_leave < 0 && steps >= 0 && fading.dialog().end_sent != 0)
        steps_at_leave = steps_before; // fade-gated MSG_LEAVE ran on this tick
      if (steps_at_end < 0 && steps >= 0 && !fading.dialog().is_open())
        steps_at_end = steps_before; // the camera's final message ended it
      if (steps < 0 && fading.dialog().closing != 0)
        steps = 0; // ui_restartFadeIn ran inside this tick
      fading.ui_frame_step();
      if (steps >= 0)
        ++steps;
    }
    check(!fading.dialog().is_open() && t < 4000, "fade host: the conversation still closes");
    check(t > ticks, "fade host: message 3 comes later than with a fade held at 1.0");
    check(steps_when_one == 9, "fade host: the fade reads exactly 1.0 after the ninth step and not before");
    check(steps_at_leave >= 9 && steps_at_leave <= 10,
          "fade host: the leave message follows the first tick that reads 1.0");
    check(steps_at_end >= steps_at_leave,
          "fade host: the camera's end message follows the fade-gated leave");
  }
  const auto &c = host.dialog_counts();
  check(c.start_messages == 1 && c.opens == 1 && c.ends == 1, "host: one message 0, one open, one end");
  check(c.choices_taken == 3 && c.nodes_shown == 3, "host: three choices taken over nodes 2, 0, 1");
  check(c.missing_entries == 1, "host: the missing condition entry is counted");
  check(d.mutex == 0 && d.hud_suppressed == 0, "host: message 3 frees the mutex and the HUD");
  check(c.deferred_shops == 1, "host: activateStore inside the dialog opens the shop at the end");
  check(c.post_actions == 1 && c.pending_levels == 1, "host: the post action ran and newWorldPost queued the level");
  check(host.variables().memory.cstr(0x41b6d0) == "nextlvl" && host.variables().memory.read(0x41b720) == 0xff,
        "host: pending level 0x41b6d0 with 0xff at 0x41b720");
  check(host.level().pending.name == "nextlvl", "host: level flow has the request");
  const auto &campaign = host.campaign();
  std::int64_t step = -1, posted = -1;
  for (const auto &v : campaign.vars()) {
    if (v.name == "step") step = v.value;
    if (v.name == "posted") posted = v.value;
  }
  check(step == 7 && posted == 1, "host: the action's setv and the post action's setv");
  check(campaign.record(0).quest_count == 1, "host: addQuest from a choice action");
  check((campaign.record(0).missions[std::size_t(champions::simulation::rta::quests::mission_index(1, 1))] & 1) == 1,
        "host: setMissionAvailable from a choice action");
  check(campaign.events().menu_opens == 0, "host: activateMissionMenu is refused while a dialog is open");
  // The silent line: v0 has a .vag, the others have none.
  check(c.voice_lines == 1 && c.silent_lines == 2, "host: one voiced line, two silent lines");

  // ---- the conversation camera (docs/spec/dialog-gaps.md 2) ----
  // The swoop takes 47 to 182 Camera_lookAt calls each way (the arrival is one call
  // after the progress reaches the end), so the conversation cannot end in one tick.
  check(c.camera_views == 1 && c.camera_steps >= 2 * 47, "host: the camera swoops in and out in 47 or more steps each");
  check(host.dialog_camera_view() && host.dialog_camera_view()->progress == champions::simulation::rta::ZERO,
        "host: the swoop out ends at progress 0");
  check(host.dialog_camera_pose() != nullptr, "host: the camera pose was published");
  {
    // The speaker has no dialogOffset: the sideways offset would be a stack word nobody
    // initialised (section 10), so the opening is refused and the mutex given back.
    const auto bare = objects_ob({{"NPC", 0, {0, 0, 0}, {"name=npc", "showName=\"nm\"", "dialogScript=talk"}}});
    sc::Host h;
    check(load(h, script, bare), "host: script loads");
    h.attach_dialog(level, services);
    h.run_start();
    h.use(0, front);
    check(!h.dialog().is_open() && h.dialog().mutex == 0 && h.dialog_camera_view() == nullptr,
          "host: no dialogOffset: the opening is refused, nothing is invented");
    bool reasoned = false;
    for (const auto &[reason, n] : h.dialog_counts().refused)
      reasoned |= reason.find("uninitialised stack slot") != std::string::npos && n >= 1;
    check(reasoned, "host: the refusal names the stack slot");
  }
  {
    // dialog_loadBox: no box.tex word in "intro" unless an earlier level left one.
    auto intro = level;
    intro.level = "intro";
    intro.files["npc.dbin"] = dl::build_dbin(nodes);
    sc::Host h;
    check(load(h, script, objects), "host: script loads");
    h.attach_dialog(intro, services);
    check(h.dialog_box_word() == 0, "host: intro writes no box word");
    h.run_start();
    h.use(0, front);
    check(!h.dialog().is_open(), "host: a dialog in intro with word 0 is refused, not drawn");
    intro.previous_box = true;
    sc::Host h2;
    check(load(h2, script, objects), "host: script loads");
    h2.attach_dialog(intro, services);
    h2.run_start();
    check(h2.dialog_box_word() != 0 && h2.use(0, front) == "script" && h2.dialog().is_open(),
          "host: intro keeps the previous level's box word");
    auto modeb = level;
    modeb.pvp_mode_b = true;
    sc::Host h3;
    check(load(h3, script, objects), "host: script loads");
    h3.attach_dialog(modeb, services);
    check(h3.dialog_box_word() == 0, "host: arena mode B writes no box word");
  }
  check(host.dialog_cursor_glyph() == 0x7e && host.dialog_cursor_glyph(0) == 0x7b, "host: the cursor glyph bytes");
  {
    sc::Host h;
    check(load(h, script, objects), "host: script loads");
    h.attach_dialog(level, services);
    h.run_start();
    check(h.dialog_gate("prop_draw") == "draw" && h.dialog_gate("compass", {{"compass_on", 1}}) == "go",
          "host: nothing is gated with no dialog open");
    h.use(0, front);
    check(h.dialog().is_open() && h.dialog_gate("prop_draw") == "skip" && h.dialog_gate("phprop_draw") == "skip" &&
              h.dialog_gate("compass", {{"compass_on", 1}}) == "skip" && h.dialog_gate("player_marker") == "skip",
          "host: props, the compass and the player marker are not drawn while a dialog is open");
  }

  // ---- item condition and refusals ----
  {
    auto refused_nodes = nodes;
    refused_nodes[2].choices = {{"c2", "c_item", "", -1}};
    auto lvl = level;
    lvl.files["npc.dbin"] = dl::build_dbin(refused_nodes);
    sc::Host h;
    check(load(h, script, objects), "host: script loads");
    h.attach_dialog(lvl, services);
    h.run_start();
    check(h.use(0, front) == "script", "host: dialogScript runs");
    check(h.dialog().is_open() && h.dialog().mutex != 0,
          "host: an item condition runs and the dialog opens");
    check(h.dialog_counts().refused.empty(), "host: the item condition is not refused");
  }
  {
    std::vector<dl::Node> wide_node(1);
    wide_node[0].text = "t0";
    for (int i = 0; i < 11; ++i)
      wide_node[0].choices.push_back({"c0a", "", "", -1});
    wide_node[0].raw_choices = 11;
    auto lvl = level;
    lvl.files["npc.dbin"] = dl::build_dbin(wide_node);
    sc::Host h;
    check(load(h, script, objects), "host: script loads");
    h.attach_dialog(lvl, services);
    h.run_start();
    h.use(0, front);
    check(!h.dialog().is_open() && h.dialog().mutex == 0,
          "host: 11 visible choices are refused instead of overrunning 0x8e2d20");
  }
  {
    // showName without dialogScript: Creature_interact takes the mutex and runs
    // no script (docs/dialog.md 2.2); the next use finds it busy.
    const auto mute = objects_ob({{"NPC", 0, {0, 0, 0}, {"name=mute", "showName=nm"}}});
    sc::Host h;
    check(load(h, script, mute), "host: script loads");
    h.attach_dialog(level, services);
    h.run_start();
    check(h.talkers().empty() && h.interactables().size() == 1, "host: a showName-only NPC is interactable, not a talker");
    check(h.use(0, front) == "no script" && h.dialog().mutex == 1, "host: no dialogScript keeps the mutex held");
    check(h.use(0, front) == "busy", "host: the held mutex makes the next use busy");
    check(h.dialog_leave() && h.dialog().mutex == 0 && h.dialog_counts().client_mutex_releases == 1,
          "host: the client frees the held mutex");
  }
  {
    // No text table: refused.
    sc::Host h;
    check(load(h, script, objects), "host: script loads");
    auto none = services;
    none.text = nullptr;
    h.attach_dialog(level, none);
    h.run_start();
    h.use(0, front);
    check(!h.dialog().is_open(), "host: no text table refuses the conversation");
  }
}
} // namespace dialog_host_tests

inline void run_host_tests(dialog_host_tests::Check check, std::string &where) { dialog_host_tests::run(check, where); }
