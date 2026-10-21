#include "champions/combat_play.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
// world_findFreeSpot in the Full composition: the exact replay of the
// research-generated vectors, and the idle Full fight that used to stop on it.
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
namespace {
int checks = 0;
void check(bool b, const std::string &s) {
  ++checks;
  if (!b)
    throw std::runtime_error(s);
}
struct Case {
  std::string name;
  Raw seed = 0;
  std::vector<std::array<Raw, 6>> objects; // x y z radius height flags
  std::array<Raw, 4> terrain{};            // BX HY HI LO
  std::array<Raw, 6> call{};               // cx cy cz spread radius any
  std::array<Raw, 5> out{};                // px py pz ok seed
};
template <std::size_t N> std::array<Raw, N> words(std::istringstream &in) {
  std::array<Raw, N> v{};
  for (auto &x : v) {
    std::string t;
    in >> t;
    x = Raw(std::stoul(t, nullptr, 16));
  }
  return v;
}
std::vector<Case> load(const char *path) {
  std::ifstream f(path);
  check(bool(f), "vector file opens");
  std::vector<Case> out;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag == "CASE") {
      out.emplace_back();
      in >> out.back().name;
      continue;
    }
    check(!out.empty(), "record before CASE");
    auto &c = out.back();
    if (tag == "SEED") c.seed = words<1>(in)[0];
    else if (tag == "OBJ") c.objects.push_back(words<6>(in));
    else if (tag == "TERRAIN") c.terrain = words<4>(in);
    else if (tag == "CALL") c.call = words<6>(in);
    else if (tag == "OUT") c.out = words<5>(in);
    else check(false, "unknown record " + tag);
  }
  return out;
}
C::Services host(const std::array<Raw, 4> &t) {
  auto s = C::flat_fixture_services();
  s.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
  s.animation_advance = [](Raw, Raw) { return Raw(0); };
  // The fixture's two-rule terrain (generate_combat_free_spot_vectors.py).
  s.terrain_blocked = [t](const Vec3 &p, Raw) { return lt(p[0], t[0]) ? ONE : Raw(0); };
  s.terrain_height = [t](const Vec3 &p, Raw) { return lt(p[1], t[1]) ? t[2] : t[3]; };
  return s;
}
void replay(const Case &c) {
  C::World w(host(c.terrain), 1, C::Composition::Full);
  check(w.read(0x4039b8) == 0, c.name + ": exclude mask is the reference's 0");
  for (const auto &o : c.objects) {
    C::Spawn s;
    s.position = {o[0], o[1], o[2]};
    auto a = w.spawn_skelight(s);
    w.write(a + 0x1c, 4, o[3]);
    w.write(a + 0x20, 4, o[4]);
    if (o[5] & 1)
      w.write(a + 12, 4, w.read(a + 12) | 1);
    check((w.read(a + 12) & 0x20001) == (o[5] & 0x20001), c.name + ": object flags");
  }
  w.write(C::SHARED_SEED, 4, c.seed);
  auto r = w.find_free_spot({c.call[0], c.call[1], c.call[2]}, c.call[3], c.call[4], c.call[5]);
  for (unsigned k = 0; k < 3; ++k)
    check(r.first[k] == c.out[k], c.name + ": point word " + std::to_string(k));
  check(Raw(r.second) == c.out[3], c.name + ": ok");
  check(Raw(w.read(C::SHARED_SEED)) == c.out[4], c.name + ": shared seed after the search");
}
void missing_terrain_refuses() {
  auto s = C::flat_fixture_services();
  C::World w(s, 1, C::Composition::Full);
  bool refused = false;
  try {
    w.find_free_spot({0, 0, 0}, 72, 12, 0);
  } catch (const C::Unsupported &) {
    refused = true;
  }
  check(refused, "a Full world without the terrain services refuses the search");
}
// The idle Full fight: the Skelight at x = 80 never moves, so its
// Creature_moveTo stuck recovery calls world_findFreeSpot(pos, 72, 12, 0) once
// the stand reaches 300 clock ticks, in the 311th ready tick. It used to stop
// there with 310 ready ticks done.
void idle_full_fight() {
  combat_play::Session s(session_control::Controller().setup().initial_terrain,
                         combat_play::Fixture::Full);
  s.advance(0);
  int searched_at = -1;
  for (int t = 0; t < 400; ++t) {
    auto o = s.advance(0);
    check(!o.failed, "idle Full fight tick " + std::to_string(t + 1) + ": " + o.error);
    for (const auto &e : s.tick_events())
      if (e.name == "findFreeSpot" && searched_at < 0) {
        searched_at = int(o.ready_ticks);
        check(e.args.size() == 10 && e.args[0] == from_int(80) && e.args[1] == 0 &&
                  e.args[3] == 72 && e.args[4] == 12 && e.args[5] == 0 && e.args[9] == 1,
              "the stuck Skelight searches from its own position, spread 72, radius 12");
        const auto a = s.actors().at(1);
        check(a.position[0] == Raw(e.args[6]) && a.position[1] == Raw(e.args[7]) &&
                  a.position[2] == Raw(e.args[8]),
              "a found spot moves the Skelight there");
      }
  }
  check(searched_at == 311, "the stuck recovery searches in ready tick 311");
}
// Attack, then walk (research t3/research-combat-walk, difftest_combat_walk
// walk.once and walk.mash.12, seeds 20973 and 7). Player_run state 1 ends 40
// ticks after the swing starts with the stick past 0.8, and the neutral state
// walks on. A press every 12 ticks chains the swings and never hands back. A
// press is a held edge, as a pad makes it (press_attack(true) for one tick).
void attack_then_walk() {
  namespace SC = session_control;
  // The fixture's two Skelights chase and kill a player that stays near them
  // within about 150 ticks (party wipe), so both runs stop at 120.
  auto run = [](bool mash, std::vector<Raw> &ys, std::vector<Raw> &states,
                std::vector<Raw> &starts) {
    const int ticks = 120;
    combat_play::Session s(SC::Controller().setup().initial_terrain,
                           combat_play::Fixture::Full);
    s.advance(0);
    for (int t = 0; t < ticks; ++t) {
      const bool press = mash ? (t >= 10 && (t - 10) % 12 == 0) : t == 10;
      if (press)
        s.press_attack(true);
      else
        s.release_attack();
      auto o = s.advance(SC::POS_Y);
      check(!o.failed, "attack-then-walk tick " + std::to_string(t) + ": " + o.error);
      const auto p = s.world().target(s.actors().at(0).handle);
      ys.push_back(Raw(s.world().read(p + 20)));
      states.push_back(Raw(s.world().read(p + 0x58c)));
      starts.push_back(Raw(s.world().read(p + 0x448)));
    }
  };
  std::vector<Raw> ys, states, starts;
  run(false, ys, states, starts);
  check(states[10] == 1, "a held press starts a swing");
  int end = -1;
  for (int t = 11; t < int(states.size()) && end < 0; ++t)
    if (states[t - 1] == 1 && states[t] == 2)
      end = t;
  check(end == 50, "state 1 ends 40 ticks after the swing starts (walk.once)");
  for (int t = 11; t < end; ++t)
    check(ys[t] == ys[10], "no step during the swing");
  // The stick walks the fixture's player along -y (camera yaw 0).
  for (int t = end + 1; t < int(ys.size()); ++t)
    check(lt(ys[t], ys[t - 1]), "the player walks on after the swing");
  check(lt(add(ys.back(), from_int(100)), ys[end]), "and covers ground");
  std::vector<Raw> mys, mstates, mstarts;
  run(true, mys, mstates, mstarts);
  int restarts = 0;
  for (int t = 10; t < int(mstates.size()); ++t) {
    check(mstates[t] == 1, "a press every 12 ticks keeps state 1 (walk.mash.12)");
    check(mys[t] == mys[10], "no step while the swings chain");
    restarts += t > 10 && mstarts[t] != mstarts[t - 1];
  }
  check(restarts >= 4, "the presses chain the next swings");
}
// A walking player beside the armed Skelight at x = 80: walking presses it,
// its Creature_moveTo stuck recovery searches world_findFreeSpot(pos, 72, 12,
// 0), and the fight runs on without a refusal.
void walk_beside_skelight() {
  namespace SC = session_control;
  combat_play::Session s(SC::Controller().setup().initial_terrain,
                         combat_play::Fixture::Full);
  s.advance(0);
  int searches = 0;
  Raw y_before = 0, x_before = 0;
  bool moved_last = false;
  for (int t = 0; t < 1500; ++t) {
    auto o = s.advance(((t / 90) % 2 == 0) ? SC::POS_X : SC::NEG_X);
    check(!o.failed, "walking fight tick " + std::to_string(t + 1) + ": " + o.error);
    for (const auto &e : s.tick_events())
      if (e.name == "findFreeSpot") {
        ++searches;
        check(e.args[3] == 72 && e.args[4] == 12 && e.args[5] == 0,
              "Creature_moveTo's search arguments");
      }
    const auto a = s.actors().at(0);
    moved_last = a.position[0] != x_before || a.position[1] != y_before;
    x_before = a.position[0];
    y_before = a.position[1];
  }
  check(searches > 0, "the pressed Skelight searched a free spot");
  check(moved_last, "the player still walks at the end");
  std::cout << "walking fight: 1500 ready ticks, " << searches << " free-spot searches\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2) {
      auto cases = load(argv[1]);
      check(cases.size() == 11, "eleven reference cases");
      for (const auto &c : cases)
        replay(c);
      std::cout << cases.size() << " free spot cases, " << checks << " checks\n";
      return 0;
    }
    missing_terrain_refuses();
    idle_full_fight();
    attack_then_walk();
    walk_beside_skelight();
    std::cout << "free spot host checks " << checks << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
