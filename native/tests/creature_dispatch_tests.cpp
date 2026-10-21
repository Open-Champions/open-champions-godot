#include "champions/creature_anim/dispatch.hpp"
#include "creature_dispatch_fixture.hpp"
#include <iostream>
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::creature_anim;
unsigned checks = 0;
void check(bool v, const char *message) {
  ++checks;
  if (!v)
    throw std::runtime_error(message);
}
bool same_ops(const std::vector<AnimOp> &a, const std::vector<AnimOp> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].kind != b[i].kind || a[i].args != b[i].args)
      return false;
  return true;
}
template <class F> void refuses(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const Unsupported &) {
    caught = true;
  }
  check(caught, message);
}
int main() try {
  unsigned root = 0, steered = 0, special = 0, none = 0;
  for (const auto &[name, t] : dispatch_tables()) {
    if (t.group == "root")
      ++root;
    else if (t.group == "steered")
      ++steered;
    else if (t.group == "special") {
      ++special;
      dispatch_table(name);
    } else {
      ++none;
      const auto class_name = name;
      refuses([&] { dispatch_table(class_name); }, "no-dispatch class admitted");
    }
  }
  check(root == 47 && steered == 10 && special == 10 && none == 6,
        "coverage census");
  refuses([] { select_dispatch("Agony", 0xdead); }, "unknown state admitted");
  refuses([] { select_dispatch("missing", 0); }, "unknown class admitted");
  Choice bad;
  bad.condition = Condition{false, 0x2dc, 0x380000};
  auto knock = select_dispatch("Agony", 0x12, Choice{0, Condition{false, 0x2dc, 0x80000}});
  auto masked = select_dispatch("Agony", 0x12, bad);
  check(masked.stored == 18 && same_ops(masked.ops, knock.ops) &&
            agony_hit_entry(0x380000) == 0xd,
        "agony knockback mask");
  for (Raw bits : {0x80000u, 0x100000u, 0x200000u, 0x380000u, 0xffffffffu}) {
    Choice c;
    c.condition = Condition{false, 0x2dc, bits};
    check(same_ops(select_dispatch("Agony", 0x12, c).ops, knock.ops), "knockback bit");
  }
  auto hit = select_dispatch("Agony", 0x12, Choice{0, Condition{false, 0x2dc, 0x40000}});
  Choice either;
  either.condition = Condition{false, 0x2dc, 0x40001};
  check(same_ops(select_dispatch("Agony", 0x12, either).ops, hit.ops) &&
            agony_hit_entry(0x40001) == 9,
        "hit bit under the knockback mask");
  check(same_ops(select_dispatch("Agony", 0x12, Choice{0, Condition{false, 0x2dc, 1}}).ops,
                select_dispatch("Agony", 0x12).ops),
        "other damage bits stay on the baseline clip");
  check(clockwork_idle_entry(0) == 0xa && clockwork_idle_entry(3) == 40 &&
            clockwork_idle_entry(1 | 2) == 40 && !clockwork_idle_entry(2 | 4),
        "clockwork form index");
  auto both = combined_dispatch(
      "FemaleDarkElf", 0x12, {{0x2dc, 0x80000}, {0x3c8, 1}});
  check(both.stored == 0x12 && both.ops.size() == 3 &&
            both.ops[0].kind == "anim_start" && both.ops[0].args[0] == 4,
        "combined field winner");
  check(combined_dispatch("Horsemen", 0x14, {{0x3c8, 1}, {0x470, 3}}).ops.empty() &&
            combined_dispatch("Horsemen", 0x14, {{0x3c8, 1}, {0x470, 3}}).stored ==
                0,
        "horsemen form stores idle");
  check(combined_dispatch("VampireLord", 0x11, {{0x3c8, 1}, {0x3cc, 1}}).ops[0].args[0] ==
            11,
        "vampire form");
  check(combined_dispatch("Clockwork", 0x14, {{0xc, 2}, {0x3c8, 4}}).ops[0].args[0] ==
            46,
        "clockwork death form");
  refuses(
      [] { combined_dispatch("Agony", 0x12, {{0x2dc, 1}, {0x3c8, 1}}); },
      "unrecorded combined pair admitted");
  Runtime r(8, {}, 3);
  auto s = r.add_state();
  std::vector<ClipId> clips;
  for (unsigned i = 0; i < 112; ++i)
    clips.push_back(r.add_clip(dispatch_fixture::clip_bytes(120)));
  DispatchContext c{r, s, [&](Raw e) { return clips.at(e); }, {}};
  auto death = select_dispatch("Agony", 0x14);
  refuses([&] { apply_dispatch(c, death); },
          "missing virtual handler admitted");
  check(r.state(s).count == 0, "refused death mutated state");
  std::vector<AnimOp> calls;
  c.external = [&](const auto &o) { calls.push_back(o); };
  apply_dispatch(c, death);
  check(calls.size() == 1 && calls[0].args[0] == 28, "death virtual call lost");
  check((r.track(r.state(s).tracks[0]).flags & REPORT_END) != 0,
        "death must hold end");
  r.anim_clear(s);
  install_dispatch_events(c, "Agony");
  apply_dispatch(c, select_dispatch("Agony", 0));
  apply_dispatch(c, select_dispatch("Agony", 6));
  for (unsigned tick = 1; tick <= 37; ++tick) {
    auto v = r.advance(ONE, s, false);
    check((v.events & 1) == (tick == 37 ? 1 : 0), "Agony hit timing");
    if (tick == 37) {
      auto e = dispatch_events("Agony", 6, v.events);
      check(e.size() == 1 && e[0].kind == "hit_test" &&
                e[0].consumer == "Creature_hitTargetInArc",
            "hit emission");
    }
  }
  Choice combined;
  combined.random = 1;
  combined.condition = Condition{false, 0x2dc, 0x40000};
  refuses([&] { select_dispatch("Agony", 0x12, combined); },
          "unknown combined random/field accepted");
  r.anim_clear(s);
  std::vector<int> counts;
  c.external = [&](const auto &) { counts.push_back(r.state(s).count); };
  apply_dispatch(c, select_dispatch("Bubo", 0x14));
  check(counts == std::vector<int>({0, 1}),
        "holder/start/death handoff ordering");
  Selection invalid{{{"clear", {}}, {"unknown", {}}}, 0};
  auto before = r.state(s).count;
  refuses([&] { apply_dispatch(c, invalid); }, "unknown operation accepted");
  check(r.state(s).count == before,
        "unknown operation erased valid state prefix");
  auto corpse = dispatch_events("Agony", 0x14, -128);
  check(corpse.size() == 1 && corpse[0].kind == "corpse" &&
            corpse[0].word == 0xffffff80,
        "corpse signed event word");
  check(dispatch_events("Agony", 0x14, 0).size() == 1,
        "corpse zero word handoff");
  check(dispatch_transition_probe("Agony", 6, 0) == std::vector<Raw>{3},
        "attack exit probe");
  refuses([] { dispatch_transition_probe("Agony", 6, 3); },
          "unobserved transition predicate admitted");
  auto spider = dispatch_events("SpiderQueen", 0x14, 8);
  bool shake = false;
  for (auto &e : spider)
    shake |= e.consumer == "camera_shake";
  check(shake, "uninstalled consumer lost");
  install_dispatch_events(c, "Goblin", "26a624/v1");
  refuses([&] { install_dispatch_events(c, "Goblin", "unknown"); },
          "unknown event variant admitted");
  for (Raw state : {7, 99})
    refuses([&] { admit_player_state(state); },
            "unknown player state admitted");
  for (Raw state : {6, 9, 14})
    admit_player_state(state);
  check(player_entry(7, 0xa4) == 111, "player row indexing");
  refuses([] { player_combo_variant(9, 1, 1, 0); },
          "unrecorded model9 class1 combo admitted");
  refuses([] { player_combo_variant(11, 3, 1, 0); },
          "unrecorded model11 class3 combo admitted");
  PlayerInput p;
  p.weapon_class = 4;
  refuses([&] { player_action("warcry", p, {}); },
          "unwritten warcry name admitted");
  refuses([&] { player_action("first_swing", p, {}); },
          "bow first melee admitted");
  p.weapon_class = 2;
  p.model = 2;
  check(player_action("death", p, {}).ops[0].args[0] == 1,
        "excluded death model");
  p.model = 0;
  check(player_action("death", p, {}).ops[0].args[0] == 2, "heavy death entry");
  p.draw = 0xafc7;
  check(player_action("block_reaction", p, {}).ops[0].args[1] == 0x3e4ccccc,
        "block threshold low");
  p.draw++;
  check(player_action("block_reaction", p, {}).ops[0].args[1] == 0x3dcccccc,
        "block threshold high");
  std::string name;
  p.relative_hit = -0x2000;
  player_action("knockdown", p, [&](const auto &n) {
    name = n;
    return clips[0];
  });
  check(name == "knockdown2_c.anm", "knockdown signed boundary");
  refuses(
      [&] { player_action("ground_pound", p, [](const auto &) { return 0; }); },
      "missing named clip admitted");
  r.anim_clear(s);
  auto &t = r.track(r.state(s).tracks[0]);
  t.rate = 0x3c000000;
  t.time = from_int(9);
  reset_player_idle(c, {}, {});
  check(t.rate == 0x3c000000 && t.time == from_int(9) && t.flags == 0x1205,
        "idle reset erased stale fields");
  r.clip(clips[player_entry(0, 0x4c)]).events[3] = {ONE, 128};
  install_player_events(c);
  check(r.clip(clips[player_entry(0, 0x4c)]).events[3].mask == 128,
        "player constructor erased unwritten event slots");
  check(r.clip(clips[player_entry(7, 0x4c)]).events[0].mask == 0,
        "dual wield incorrectly initialized events");
  bool active = false;
  unsigned draws = 0;
  auto find = [&](const std::string &n) {
    name = n;
    return clips[100];
  };
  check(player_fidget(c, ZERO, active, 6, find,
                      [&] {
                        ++draws;
                        return 0;
                      }),
        "fidget start");
  check(name == "ohw_idled.anm" && draws == 1, "fidget class6/draw");
  check(!player_fidget(c, ONE, active, 6, find,
                       [&] {
                         ++draws;
                         return 0;
                       }) &&
            !active && draws == 1,
        "fidget stop draw ordering");
  check(r.track(r.state(s).tracks[4]).life_rate == 0xbdaaaaaa, "fidget fade");
  check(player_events(1, 15).empty() && player_events(0, 15).empty() &&
            player_events(5, 15).empty(),
        "player attack releases invented from clip events");
  check(player_events(10, 15).size() == 5 &&
            player_events(10, 15, true).size() == 1,
        "ground pound copy gating");
  check(player_events(11, 5).size() == 1, "combined footsteps duplicated");
  check(!player_marker_exit(0, 0) && !player_marker_exit(11, 0) &&
            !player_marker_exit(5, 0),
        "conditional marker exit guessed");
  check(player_marker_exit(3, 0) == 2 && !player_marker_exit(3, 8),
        "stagger marker exit");
  check(player_events(6, 1).empty() && !player_marker_exit(6, 0) &&
            !player_marker_exit(14, 0xff),
        "skill states invent no events");
  std::vector<std::string> looked;
  auto skill = [&](const std::string &n) {
    looked.push_back(n);
    return clips[0];
  };
  PlayerInput skill_input;
  skill_input.weapon_class = 0;
  skill_input.object_flags = 0x40;
  looked.clear();
  auto cyclone = player_action("cyclone", skill_input, skill);
  check(cyclone.stored == 6 && cyclone.ops.size() == 1 &&
            cyclone.ops[0].kind == "named_push" && cyclone.ops[0].args[1] == 0x23 &&
            looked[0] == "spel_bw_cyclone.anm" && looked[1] == "cyclone1.anm",
        "cyclone push");
  r.anim_clear(s);
  apply_dispatch(c, cyclone);
  check(r.track(r.state(s).tracks[0]).weight == ONE &&
            r.track(r.state(s).tracks[0]).flags == 0x23,
        "cyclone weight");
  looked.clear();
  auto bash = player_action("bash", skill_input, skill);
  check(bash.stored == 6 && bash.ops[0].kind == "named_blend" &&
            bash.ops[0].args[1] == 0x3dcccccc && bash.ops[0].args[3] == 0x812 &&
            looked[0] == "bash.anm",
        "bash blend");
  check(player_action("shield_bash", skill_input, skill).ops[0].args[3] == 0x812,
        "shield bash blend");
  looked.clear();
  auto ancestral = player_action("ancestral_call", skill_input, skill);
  check(ancestral.stored == 6 && ancestral.ops[0].kind == "named_blend" &&
            looked[0] == "spel_bw_ancestral.anm" && looked[1] == "ohw_warcry.anm" &&
            ancestral.ops[0].args[3] == 0x22,
        "ancestral call stores 6 after the blend");
  check(player_action("roar", skill_input, skill).ops[0].args[3] == 0x22 &&
            player_action("battle_cry", skill_input, skill).ops[0].args[3] == 0x22 &&
            player_action("blood_lust", skill_input, skill).ops[0].args[3] == 0x822,
        "class 0 shouts");
  skill_input.weapon_class = 4;
  refuses([&] { player_action("ancestral_call", skill_input, skill); },
          "weapon class 4 war cry admitted");
  skill_input.weapon_class = 6;
  refuses([&] { player_action("roar", skill_input, skill); },
          "weapon class 6 shout admitted");
  // CAI-7: the seven state-6 constructors ran only on a weapon-class-0 player
  // whose object flag word was 0x40 (player-animation-dispatch.md section 6).
  for (const char *action : {"cyclone", "bash", "shield_bash", "ancestral_call",
                             "roar", "battle_cry", "blood_lust"}) {
    for (Raw cls : {4u, 6u}) {
      skill_input.weapon_class = cls;
      skill_input.object_flags = 0x40;
      refuses([&] { player_action(action, skill_input, skill); },
              "state-6 constructor admitted for an unexecuted weapon class");
    }
    skill_input.weapon_class = 0;
    for (Raw flags : {0u, 0x60u, 0x41u}) {
      skill_input.object_flags = flags;
      refuses([&] { player_action(action, skill_input, skill); },
              "state-6 constructor admitted for unexecuted object flags");
    }
  }
  skill_input.weapon_class = 0;
  // Executed inputs only (ref_skill_anim.charge_state: 0x40 and 0); the
  // generated skill vectors in creature_dispatch.json.gz cover the rest.
  skill_input.object_flags = 0x40;
  auto charged = player_action("charge", skill_input, skill);
  check(charged.stored == 9 && charged.ops.empty(), "charge with flag 0x40");
  skill_input.object_flags = 0;
  check(player_action("charge", skill_input, skill).stored == 0,
        "charge with clear flags");
  skill_input.object_flags = 0x40;
  looked.clear();
  auto shadow = player_action("shadow_step", skill_input, skill);
  check(shadow.stored == 0xe && shadow.object_flags == 0x45u &&
            shadow.ops[0].kind == "named_push" && shadow.ops[0].args[1] == 0x4 &&
            looked[0] == "spel_de_shadowstep.anm",
        "shadow step push");
  r.anim_clear(s);
  apply_dispatch(c, shadow);
  check(r.track(r.state(s).tracks[0]).weight == 0x3f800000,
        "shadow step weight");
  skill_input.object_flags = 0x45;
  auto ended = player_action("shadow_step_end", skill_input, skill);
  check(ended.stored == 2 && ended.object_flags == 0x40u && ended.ops.empty(),
        "shadow step destructor");
  skill_input.object_flags = 0x41;
  refuses([&] { player_action("shadow_step", skill_input, skill); },
          "unrecorded shadow step flags admitted");
  refuses([&] { player_action("state_7", skill_input, skill); },
          "state 7 action admitted");
  std::cout << "PASS " << checks << " dispatch API checks\n";
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
