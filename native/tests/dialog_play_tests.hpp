#pragma once
// Authored script/objects/dialog integration regressions. Evidence:
// research-dialog 334578d docs/dialog.md 2.2, 5.1-5.2, 6.1 and 7.
#include "dialog_host_tests.hpp"

namespace dialog_play_tests {
using namespace dialog_host_tests;
namespace rta = champions::simulation::rta;

inline std::vector<std::uint8_t> uni(const World &world) {
  std::vector<std::uint8_t> bytes;
  for (const auto &[key, text] : world.texts) {
    bytes.insert(bytes.end(), key.begin(), key.end());
    bytes.push_back(0);
    if (bytes.size() & 1u) bytes.push_back(0);
    for (const auto c : text) {
      bytes.push_back(static_cast<std::uint8_t>(c));
      bytes.push_back(static_cast<std::uint8_t>(c >> 8));
    }
    bytes.insert(bytes.end(), {0, 0});
  }
  bytes.push_back(0);
  return bytes;
}

inline void run(Check check, std::string &where) {
  where = "scripted dialog";
  constexpr a::Address S_TEXTURE = 2000, S_PICK = 2200, S_ABSENT = 2400;
  const auto script = build({
      {"start", {{"trigger", {S_NPC, 123}}}},
      {"talk", {{"startDialog", {S_NPC, 0, 0}}}},
      {"again", {{"trigger", {S_NPC, -1}}}},
      {"absent", {{"trigger", {S_ABSENT, 0}}}},
      {"refuse_then_mutate", {{"trigger", {S_NPC, 123}}, {"setv", {S_PICK, 1}}}},
      {"hidden", {}, 0},
      {"pick_a", {{"setv", {S_PICK, 1}}}},
      {"pick_b", {{"setv", {S_PICK, 2}}, {"showBust", {S_TEXTURE, S_BODY, 1, 10}},
                    {"showCompass", {S_NPC, S_BODY}}, {"postAction", {S_POST}}}},
      {"post", {{"showBust", {S_TEXTURE, S_BODY, 3, 10}}}},
  }, {{S_NPC, "npc"}, {S_TEXTURE, "synthetic.tex"}, {S_BODY, "body"},
      {S_POST, "post"}, {S_PICK, "pick"}, {S_ABSENT, "absent"}});
  World world;
  for (const auto *key : {"nm", "body", "next", "a", "b", "hide", "Affirmation", "dialogMore"})
    world.texts[key] = wide(std::string("Authored ") + key);
  dl::Node first;
  first.text = "body";
  first.choices = {{"hide", "hidden", "", -1}, {"a", "", "pick_a", 1}, {"b", "", "pick_b", 1}};
  first.raw_choices = 3;
  dl::Node next;
  next.text = "next";
  sc::DialogLevel level;
  level.level = "synthetic";
  level.files["npc.dbin"] = dl::build_dbin({first, next});
  const Record npc{"NPC", 0, {}, {"name=npc", "showName=nm", "dialogScript=talk"}};
  const auto objects = objects_ob({npc});
  sc::Host host;
  check(load(host, script, objects), "scripted: loads authored inputs");
  check(host.register_text(uni(world)), "scripted: registers authored HUD text");
  host.hud().inputs.camera_mode = 1; // explicit local follow camera for the HUD setter gate
  host.attach_dialog(level, world.services());
  host.run_start();
  check(host.dialog().is_open() && host.dialog_counts().triggers.at("script") == 1,
        "scripted: level start trigger opens the conversation without a use or facing test");
  check(host.variables().memory.read(rta::strings::G_HUD_SUPPRESSED) == 1 && host.hud().inputs.suppressed,
        "scripted: the HUD and binder see suppression immediately");
  check(host.dialog().visible == std::vector<int>({1, 2}), "scripted: condition hides only its choice");
  host.run("again");
  check(host.dialog_counts().triggers.at("busy") == 1 && host.dialog_counts().opens == 1,
        "scripted: a second trigger respects the mutex");
  for (int i = 0; i < 200 && !(host.dialog().reveal_done && host.dialog().at_end); ++i)
    host.dialog_tick({});
  check(host.dialog().reveal_done && host.dialog().at_end, "scripted: body reaches the choice prompt");
  const auto draw = host.dialog_draw(rta::ONE, 1, 0x7e, 0);
  const auto count = [&](const std::string &kind) {
    return std::count_if(draw.begin(), draw.end(), [&](const auto &e) { return e.kind == kind; });
  };
  check(count("sprite") == 1 && count("wide_centered") == 1 && count("ascii") == 1,
        "scripted: one box, speaker name and cursor; the NPC machine draws no portrait");
  sc::DialogInputs input;
  input.pad = dl::DOWN_BITS;
  host.dialog_tick(input);
  check(host.dialog().cursor == 0 && host.dialog().cursor_target == 1 && host.dialog().blend != rta::ONE,
        "scripted: offline down starts the documented cursor blend");
  input.pad = 0x40;
  host.dialog_tick(input);
  check(host.dialog_counts().choices_taken == 0, "scripted: confirm during the cursor blend takes no choice");
  for (int i = 0; i < 9; ++i) host.dialog_tick({});
  check(host.dialog().cursor == 1 && host.dialog().blend == rta::ONE, "scripted: cursor arrives on the second visible choice");
  host.dialog_tick(input);
  check(host.dialog().node == 1 && host.dialog_counts().choices_taken == 1, "scripted: selected action advances to its next node");
  bool picked_b = false;
  for (const auto &v : host.campaign().vars()) picked_b |= v.name == "pick" && v.value == 2;
  check(picked_b, "scripted: the selected choice's action reached campaign state");
  check(host.hud().bust_shown == 0 && host.hud().compass_shown == 0,
        "scripted: choice actions cannot show a HUD bust or compass during a dialog");
  check(run_to_close(host) < 4000 && !host.dialog().is_open(), "scripted: next node without choices closes through message 3");
  check(host.dialog_counts().post_actions == 1 && host.hud().bust_shown != 0 && host.hud().bust_value == 3,
        "scripted: post action can show the bust after message 3 clears suppression");
  check(host.variables().memory.read(rta::strings::G_HUD_SUPPRESSED) == 0 && !host.hud().inputs.suppressed,
        "scripted: end releases the shared HUD gate");
  check(host.run("absent") == sc::Stop::Complete, "scripted: trigger with no targets is a documented no-op");

  // A mixed target set must be refused before the first NPC is triggered.
  auto unknown = npc;
  unknown.cls = "UndocumentedActor";
  sc::Host refused;
  check(load(refused, script, objects_ob({npc, unknown})), "scripted: loads mixed targets");
  refused.attach_dialog(level, world.services());
  check(refused.run("refuse_then_mutate") == sc::Stop::Refused && refused.campaign().vars().empty(),
        "scripted: statement trigger stops before a later campaign mutation");
  check(!refused.dialog().is_open() && refused.dialog_counts().triggers.empty() &&
            refused.counts().refused[std::size_t(command_index("trigger"))] == 1,
        "scripted: unknown slot 3 refuses the whole command before any dialog effect");
  check(refused.dialog().mutex == 0 && !refused.hud().inputs.suppressed,
        "scripted: refused trigger leaves mutex and HUD usable");

  for (const auto &level_name : {std::string(), std::string("HaTe3")}) {
    sc::Host special;
    check(load(special, script, objects), "override: loads authored inputs");
    auto special_level = level;
    special_level.level = level_name;
    special.attach_dialog(special_level, world.services());
    check(special.run("refuse_then_mutate") == sc::Stop::Refused && special.campaign().vars().empty(),
          "override: statement trigger stops before a later campaign mutation");
    check(!special.dialog().is_open() && special.dialog_counts().triggers.empty(),
          "override: unknown level or NPC animation override domain stays refused");
  }
  sc::Host cutscene;
  check(load(cutscene, script, objects), "cutscene: loads authored inputs");
  cutscene.attach_dialog(level, world.services());
  cutscene.variables().memory.byte(0x44fcd0, 1);
  check(cutscene.run("refuse_then_mutate") == sc::Stop::Refused && cutscene.campaign().vars().empty(),
        "cutscene: statement trigger stops before a later campaign mutation");
  check(!cutscene.dialog().is_open() && cutscene.dialog_counts().triggers.empty(),
        "override: NPC cutscene mesh path is refused, not replaced with a dialog");

  sc::Host voiced;
  check(load(voiced, script, objects), "voice: loads authored inputs");
  voiced.attach_dialog(level, world.services());
  voiced.run_start();
  for (int i = 0; i < 200 && !(voiced.dialog().reveal_done && voiced.dialog().at_end); ++i)
    voiced.dialog_tick({});
  const auto playing = voiced.dialog_draw(rta::ONE, 0, 0x7e, 1);
  const auto silent = voiced.dialog_draw(rta::ONE, 0, 0x7e, 0);
  const auto has_cursor = [](const auto &ops) {
    return std::any_of(ops.begin(), ops.end(), [](const auto &e) { return e.kind == "ascii"; });
  };
  check(!has_cursor(playing) && has_cursor(silent),
        "voice: text-off prompt waits for the supplied playing status to clear");
  check(std::none_of(playing.begin(), playing.end(), [](const auto &e) { return e.kind == "wide_centered"; }),
        "voice: text-off draws no speaker name");
}
} // namespace dialog_play_tests
