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
// there with 310 ready ticks done. The Soul of slot 6 wanders without root
// motion too, so its own recovery searches once from its place.
void idle_full_fight() {
  combat_play::Session s(session_control::Controller().setup().initial_terrain,
                         combat_play::Fixture::Full);
  s.advance(0);
  int searched_at = -1, other_searches = 0;
  for (int t = 0; t < 400; ++t) {
    auto o = s.advance(0);
    check(!o.failed, "idle Full fight tick " + std::to_string(t + 1) + ": " + o.error);
    for (const auto &e : s.tick_events())
      if (e.name == "findFreeSpot" && e.args.size() == 10 && e.args[0] != from_int(80))
        ++other_searches; // the Soul's own stuck recovery (slot 6)
      else if (e.name == "findFreeSpot" && searched_at < 0) {
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
  check(other_searches == 1, "the Soul's stand reaches 300 clock ticks once in 400");
}
// Round 4 (audit C3-2): the World no longer answers model_getMeshJoint and
// resref_get with stand-ins. The Full fixture supplies no loaded mesh record
// or resource reference, so the first Skeleton death refuses at the call.
// Counted here: how many Full-fight scenarios stop on those refusals, and the
// ready tick of the stop.
void loaded_data_refusals() {
  combat_play::Session s(session_control::Controller().setup().initial_terrain,
                         combat_play::Fixture::Full);
  s.advance(0);
  for (int t = 0; t < 5; ++t)
    check(!s.advance(0).failed, "the Full fight runs before the kill");
  C::Address skeleton = 0, player = 0;
  for (const auto &a : s.actors()) {
    if (a.slot == 5) skeleton = s.world().target(a.handle);
    if (a.slot == 0) player = s.world().target(a.handle);
  }
  check(skeleton && player, "slot 0 and slot 5 are present");
  s.world().take_damage(skeleton, player, 0, from_int(100000));
  std::string error;
  int refusals = 0, stop = -1;
  for (int t = 0; t < 400 && error.empty(); ++t) {
    auto o = s.advance(0);
    if (o.failed) {
      error = o.error;
      stop = int(o.ready_ticks) + 1; // the tick that failed
      ++refusals;
    }
  }
  std::cout << "Skeleton killed after ready tick 5: " << refusals << " refusal(s), refused in ready tick " << stop
            << ": " << error << '\n';
  check(refusals == 1 && (error.find("model_getMeshJoint") != std::string::npos ||
                          error.find("resref_get") != std::string::npos),
        "a killed Skeleton refuses at a loaded-data service: '" + error + "'");
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
    loaded_data_refusals();
    std::cout << "free spot host checks " << checks << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
