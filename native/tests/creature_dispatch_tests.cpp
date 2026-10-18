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
  unsigned root = 0, steered = 0, other = 0;
  for (const auto &[name, t] : dispatch_tables()) {
    if (t.group == "root")
      ++root;
    else if (t.group == "steered")
      ++steered;
    else {
      ++other;
      const auto class_name = name;
      refuses([&] { dispatch_table(class_name); }, "partial class admitted");
    }
  }
  check(root == 47 && steered == 10 && other == 16, "coverage census");
  refuses([] { select_dispatch("Agony", 0xdead); }, "unknown state admitted");
  refuses([] { select_dispatch("missing", 0); }, "unknown class admitted");
  Choice bad;
  bad.condition = Condition{false, 0x2dc, 0x380000};
  refuses([&] { select_dispatch("Agony", 0x12, bad); },
          "unrecorded combined damage admitted");
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
  for (Raw state : {6, 7, 9, 14, 99})
    refuses([&] { admit_player_state(state); },
            "unknown player state admitted");
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
  std::cout << "PASS " << checks << " dispatch API checks\n";
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
