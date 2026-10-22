// Host checks for the five text commands and the live charm target
// (docs/STRINGS.md). Included by script_commands_tests.cpp after its helpers.
// The table is synthetic; values come from research d4987fd docs/strings.md.
#pragma once

namespace text_host {
namespace st = champions::simulation::rta::strings;

std::vector<std::uint8_t> uni(const std::vector<std::pair<std::string, std::string>> &pairs) {
  std::vector<std::uint8_t> out;
  for (const auto &[key, text] : pairs) {
    out.insert(out.end(), key.begin(), key.end());
    out.push_back(0);
    if (out.size() & 1)
      out.push_back(0);
    for (const char c : text) {
      out.push_back(static_cast<std::uint8_t>(c));
      out.push_back(0);
    }
    out.push_back(0);
    out.push_back(0);
  }
  out.push_back(0);
  out.push_back(0);
  return out;
}
st::Units wide(const std::string &text) { return st::Units(text.begin(), text.end()); }

void prepare(sc::Host &host) {
  check(host.register_text(uni({{"Hello", "Welcome"},
                                {"Hello", "second copy"},
                                {"Title", "Gold"},
                                {"Body", "Find the key"},
                                {"Count", "Kills: %d left"},
                                {"Tail", "ends in %d"},
                                {"Long", std::string(120, 'x')},
                                {"Cap", "Captain"}})),
        "a synthetic table registers");
  host.set_help_chime(3);
  host.hud().inputs.camera_mode = 1;
}

void test_text_commands() {
  auto built = build_calls({{"start",
                             {{"setInfo", {2000}},
                              {"showBust", {2100, 2200, 7, -1}},
                              {"showCompass", {2300, 2200}},
                              {"addHelpMessage", {2400, 2500}},
                              {"addHelpMessageF", {2400, 12, 2600, 3}},
                              {"setInfo", {2700}}}},
                            {"post", {{"setInfo", {2400}}}}},
                           {{2000, "\"Hello\""}, {2100, "bust_face.tex"}, {2200, "Cap"},
                            {2300, "ogre"}, {2400, "Title"}, {2500, "Body"}, {2600, "Count"},
                            {2700, "Missing_key_of_some_length"}});
  sc::Host host;
  prepare(host);
  host.add_object("ogre", 0x20, {0x41200000u, 0x41a00000u, 0}, 0);
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "text script completes");
  for (const char *name : {"setInfo", "showBust", "showCompass", "addHelpMessage", "addHelpMessageF"})
    check(host.counts().executed[index_of(name)] >= 1 && host.counts().refused[index_of(name)] == 0,
          std::string("text command executed: ") + name);
  const auto &hud = host.hud();
  // The last setInfo missed: NF: plus the first 16 key bytes, 180 ticks.
  check(hud.message == wide("NF:Missing_key_of_s"), "a missing key shows NF: and 16 key bytes");
  check(hud.msg_frames == 180, "setInfo sets 180 ticks");
  check(hud.bust_shown == 5 && hud.bust_text == wide("Captain") && hud.bust_texture == "bust_face.tex" &&
            hud.bust_value == 7 && hud.bust_max == 0xffffffffu,
        "showBust stores the lease, texture, text, value and maximum");
  check(hud.compass_shown == 5 && hud.compass_label == wide("Captain") &&
            hud.compass_target == std::array<std::uint32_t, 3>{0x41200000u, 0x41a00000u, 0},
        "showCompass stores the lease, label and the object's position");
  // addHelpMessageF replaced addHelpMessage's box: one slot, the last call wins.
  check(hud.help_open && hud.help_title == st::HELP_F_TITLE && hud.help_body == st::HELP_F_BODY,
        "the help box points at the F buffers");
  check(hud.help_f_title == wide("Gold") && hud.help_f_body == wide("Kills: 3 left"),
        "addHelpMessageF formats each text with its integer");
  check(hud.help_close_delay == 90, "a box opens with a 90-frame close delay");
  check(host.variables().memory.u8(st::G_HELP_OPEN) == 1, "the open flag is mirrored for helpMessageActive");
  // Two chimes: the last free slot each time (31, then 30), no seed read.
  check(host.sound().slots[31].tracks == 1 && host.sound().slots[30].tracks == 1 &&
            host.sound().slots[29].tracks == 0 && host.sound().slots[31].frozen == 1,
        "each chime takes the highest free slot and plays while frozen");
  check(host.ui_pulse_starts() == 2, "each opening starts the UI pulse");
  check(host.ui_ramps().pulse[0] == 0x40000000u, "ui_startPulse(0) stores 2.0 in g_uiPulse[0]");
  host.ui_frame_step();
  check(host.ui_ramps().pulse[0] == champions::ui_text_gaps::pulseRamp(champions::ui_text_gaps::TWO, 1)[0],
        "the pulse decays by 0.8 of its excess per displayed frame");
  // HUD Run: the bust is a 5-tick lease, the message counts down per tick.
  for (int tick = 0; tick < 5; ++tick)
    host.on_ready_tick();
  check(hud.bust_shown == 0 && hud.compass_shown == 0, "the bust and the compass go after 5 ticks");
  check(hud.msg_frames == 175, "the message loses one per ready tick");
  // help_task: the offline game pauses until a confirm after 90 frames.
  host.variables().set_post_action("post");
  int frames = 0;
  st::HelpFrame frame;
  do {
    frame = host.help_task(true);
    ++frames;
    if (!frame.closed)
      check(frame.paused && host.variables().memory.u32(0x41b4f4) == 1, "an open box pauses the game");
  } while (!frame.closed && frames < 200);
  check(frames == 90, "the confirm is taken on the 90th frame");
  check(!hud.help_open && host.variables().memory.u32(0x41b4f4) == 0 &&
            host.variables().memory.u32(st::G_MENU_INPUT_LOCKOUT) == 60,
        "closing clears the pause and sets the 60-frame lockout");
  check(hud.message == wide("Gold"), "the post action runs after the confirm");
}

void test_text_refusals() {
  auto built = build_calls({{"start",
                             {{"setInfo", {2000}},
                              {"addHelpMessageF", {2100, 1, 2100, 2}},
                              {"showCompass", {2200, 2300}},
                              {"showBust", {2400, 2300, 1, -1}}}}},
                           {{2000, "Long"}, {2100, "Tail"}, {2200, "nobody"}, {2300, "Cap"},
                            {2400, "bust.tex"}});
  sc::Host host;
  prepare(host);
  host.hud().inputs.camera_mode.reset();
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "discarded text refusals do not stop the entry");
  for (const char *name : {"setInfo", "addHelpMessageF", "showCompass", "showBust"})
    check(host.counts().refused[index_of(name)] == 1 && host.counts().executed[index_of(name)] == 0,
          std::string("text refusal: ") + name);
  const auto &reasons = host.counts().text_refusals;
  check(reasons.count("setInfo text over 99 units") == 1, "a message over 99 units is refused");
  check(reasons.count("addHelpMessageF: format reads past its terminator") == 1,
        "a help text ending in a conversion is refused");
  check(reasons.count("showCompass of a missing object reads through a null pointer") == 1,
        "a compass on a missing object is refused");
  check(reasons.count("showBust without a local camera") == 1, "no camera refuses the bust");
  check(host.hud().message.empty() && !host.hud().help_open && host.hud().bust_shown == 0 &&
            host.text().nf_index() == 0,
        "a refused text command changes nothing, not even the miss ring");
}

// Research d4987fd docs/strings.md 5: Creature_setCharm's handle_clear of a
// live target unlinks this creature's node and its partner only.
void test_charm_live_target() {
  auto built = build_calls({{"start", {{"charmMonster", {2000}}}}}, {{2000, "imp"}});
  sc::Host host;
  host.add_object("imp", 0x20, {}, 0);
  host.add_object("wolf", 0x20, {}, 0);
  host.add_object("hero", 0x20, {}, 0);
  auto *imp = host.variables().find_first("imp");
  auto *wolf = host.variables().find_first("wolf");
  auto *hero = host.variables().find_first("hero");
  check(imp && wolf && hero, "charm objects exist");
  if (!imp || !wolf || !hero)
    return;
  host.link_target(imp->addr, hero->addr);
  host.link_target(wolf->addr, hero->addr);
  const auto free_before = host.variables().world->handles.count_free();
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "charm on a live target completes");
  auto &memory = host.variables().memory;
  check(host.counts().executed[index_of("charmMonster")] == 1 && host.counts().callee_refusals == 0,
        "charmMonster runs on a live target");
  check(memory.u32(imp->addr + 0x234) == 0 && memory.u16(imp->addr + 0x312) == 0xffff,
        "the target word is 0 and the charm stored");
  check(memory.u32(wolf->addr + 0x234) != 0, "another creature's target word is untouched");
  const auto held = champions::simulation::rta::HandlePool::items(hero->heads[0]);
  check(held.empty(), "the back list of the target holds partner nodes only");
  std::size_t back = 0;
  for (auto *node = hero->heads[0].next; node; node = node->next)
    ++back;
  check(back == 1, "only the charmed creature's partner left the target's back list");
  check(host.variables().world->handles.count_free() == free_before + 2, "both nodes return to the free list");
}
} // namespace text_host
