// Replays native/tests/vectors/potion_ctors.txt.gz (tools/generate_potion_ctors_vectors.py,
// research ref_potion_ctors at e9777eb) through the combat World: LooseFire_ctor with
// fx_spawn id 5 and Fire_ctor, Fire_run, LooseFire_run, LooseFire_onTouch and
// poisonGasSpell_ctor. The words each body stores, the shared seed and its draw count
// are compared; addresses and handle words are not. Research-combat-ctors-2
// (5b92837) adds the three destructors (combat-ctors-2.md section 6), the fire
// entry of a touch (fire-touch.md; the damage_apply rows themselves are
// fire-touch-exact) and poisonGasSpell_run (combat round 6).
#include "champions/combat_runtime.hpp"
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
namespace {
int checks = 0, failures = 0;
void check(bool b, const std::string &s) {
  ++checks;
  if (!b && ++failures < 40)
    std::cerr << "FAIL " << s << "\n";
}
struct Host {
  C::Services services;
  std::unique_ptr<C::World> w;
  Host() {
    services.dialog_is_open = [] { return false; };
    services.jumpgate_travel_state = [] { return Raw(0); };
    services.raycast = [](const Vec3 &, const Vec3 &, Raw, Raw) { return MINUS_ONE; };
    services.slide = [](const Vec3 &p, const Vec3 &d, Raw, Raw) {
      C::Slide out;
      for (unsigned k = 0; k < 3; ++k)
        out.position[k] = add(p[k], d[k]);
      return out;
    };
    services.height_below = [](const Vec3 &, Raw) { return Raw(0); };
    services.water_test = [](const Vec3 &, Raw) { return Raw(0); };
    services.water_disturb = [](const Vec3 &, Raw, Raw) { return Raw(0); };
    services.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
    services.animation_advance = [](Raw, Raw) { return Raw(0); };
    services.animation_root = [](C::Address) { return std::array<Raw, 2>{0, 0}; };
    services.skill_stat = [](const std::string &, Raw, Raw) { return Raw(0); };
    services.camera_yaw = [](Raw) { return Raw(0); };
    services.camera_flag = [](Raw) { return Raw(0); };
    services.attach_point = [](C::Address, Raw, const Vec3 &pos, Raw, Raw) { return pos; };
    services.sample_point_count = [](Raw) { return Raw(0); };
    services.resource = [](const C::ResourceQuery &) { return Raw(0); };
    services.hide_mask = [](Raw, Raw) { return UINT64_MAX; };
    w = std::make_unique<C::World>(services, 1, C::Composition::Full);
  }
  C::Address player(unsigned slot, Raw x) {
    C::Spawn s;
    s.team = 0;
    s.position = {x, 0, 0};
    return w->spawn_player(slot, s);
  }
  std::vector<C::Address> of_table(Raw table) const {
    std::vector<C::Address> out;
    for (auto a : w->actors())
      if (w->read(a + 0x48) == table)
        out.push_back(a);
    return out;
  }
};
std::vector<Raw> words(std::istringstream &in) {
  std::vector<Raw> out;
  std::string t;
  while (in >> t)
    out.push_back(Raw(std::stoul(t, nullptr, 16)));
  return out;
}
const C::Event *last(const C::World &w, const std::string &name) {
  const C::Event *out = nullptr;
  for (const auto &e : w.events())
    if (e.name == name)
      out = &e;
  return out;
}
bool alive(const C::World &w, C::Address a) {
  for (auto x : w.actors())
    if (x == a)
      return true;
  return false;
}
std::string refusal(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const C::Unsupported &e) {
    return e.what();
  }
  return "";
}
struct Case {
  std::string name, kind;
  std::vector<Raw> in, obj, seed, after, glow, particle;
  std::vector<std::vector<Raw>> fires;
  std::string out;
  int asked = -1;
  bool deleted = false;
};
void loosefire(const Case &c) {
  Host h;
  // in: seed frame x y z owner
  C::Address owner = c.in[5] ? h.player(0, from_int(Raw(-2000))) : 0;
  h.w->write(C::FRAME, 4, c.in[1]);
  h.w->write(C::SHARED_SEED, 4, c.in[0]);
  h.w->clear_history();
  auto obj = h.w->spawn_loose_fire({c.in[2], c.in[3], c.in[4]}, owner);
  std::vector<Raw> got;
  for (unsigned off : {0x0c, 0x1c, 0x20, 0x44, 0x48, 0x50})
    got.push_back(Raw(h.w->read(obj + off)));
  got.push_back(Raw(h.w->read(obj + 0x28, 1)));
  got.push_back(Raw(h.w->read(obj + 0x29, 1)));
  check(got == c.obj, c.name + " LooseFire words");
  check((h.w->target(obj + 0x4c) == owner), c.name + " owner handle");
  auto fires = h.of_table(0x50f208);
  check(fires.size() == c.fires.size(), c.name + " fire count " + std::to_string(fires.size()));
  for (std::size_t k = 0; k < fires.size() && k < c.fires.size(); ++k) {
    auto f = fires[k];
    std::vector<Raw> fw{Raw(h.w->read(f + 0x10)), Raw(h.w->read(f + 0x14)), Raw(h.w->read(f + 0x18)),
                        Raw(h.w->read(f + 0x44)), Raw(h.w->read(f + 0x0c)), Raw(h.w->read(f + 0x28, 1)),
                        Raw(h.w->read(f + 0x48)), Raw(h.w->read(f + 0x4c, 2))};
    check(fw == c.fires[k], c.name + " fire " + std::to_string(k));
  }
  int draws = 0;
  for (const auto &e : h.w->events())
    draws += e.name == "shared_draw";
  check(Raw(h.w->read(C::SHARED_SEED)) == c.seed[0] && Raw(draws) == c.seed[1],
        c.name + " shared seed and draws " + std::to_string(draws));
  // Every fire and the LooseFire run every frame.
  for (auto f : fires)
    check(h.w->read(f + 4) != 0, c.name + " fire on a run list");
}
void fire_run(const Case &c) {
  Host h;
  h.w->write(C::FRAME, 4, 0);
  h.w->write(C::SHARED_SEED, 4, 1);
  auto lf = h.w->spawn_loose_fire({0, 0, 0}, 0);
  (void)lf;
  auto fire = h.of_table(0x50f208).at(0);
  // in: life frame own x y z
  h.w->write(fire + 0x4c, 2, c.in[0]);
  h.w->write(C::FRAME, 4, c.in[1]);
  h.w->write(fire + 0x44, 4, c.in[2]);
  for (unsigned k = 0; k < 3; ++k)
    h.w->write(fire + 0x10 + 4 * k, 4, c.in[3 + k]);
  auto seed = h.w->read(C::SHARED_SEED);
  h.w->clear_history();
  auto why = refusal([&] { h.w->run_actor(fire); });
  auto glow = last(*h.w, "presentation:glow_add");
  auto part = last(*h.w, "presentation:particle_spawn");
  check(glow && std::vector<Raw>(glow->args.begin(), glow->args.end()) == c.glow, c.name + " glow");
  check(part && std::vector<Raw>(part->args.begin(), part->args.end()) == c.particle, c.name + " particle");
  check(Raw(h.w->read(fire + 0x4c, 2)) == c.after[0] && Raw(h.w->read(fire + 0x44)) == c.after[1],
        c.name + " life and own seed after");
  check(h.w->read(C::SHARED_SEED) == seed, c.name + " no shared draw");
  // Slot 1 with 3 runs Fire_dtor 0x3c5050: GameObject_dtor and pool_free.
  check(why.empty() && c.deleted == !alive(*h.w, fire) &&
            c.deleted == bool(last(*h.w, "destroy")),
        c.name + " deletion through slot 1: '" + why + "'");
}
void loosefire_run(const Case &c) {
  Host h;
  h.w->write(C::FRAME, 4, 0);
  auto lf = h.w->spawn_loose_fire({0, 0, 0}, 0);
  h.w->write(lf + 0x50, 4, c.in[0]);
  h.w->write(C::FRAME, 4, c.in[1]);
  auto why = refusal([&] { h.w->run_actor(lf); });
  // LooseFire_dtor 0x3c6778: the owner handle cleared, GameObject_dtor, free.
  check(why.empty() && bool(c.in[2]) == !alive(*h.w, lf),
        c.name + " LooseFire_run end test: '" + why + "'");
}
void touch(const Case &c) {
  Host h;
  // in: frame owner_kind other_kind difficulty enemy
  h.w->write(C::FRAME, 4, 0);
  C::Address owner = c.in[1] ? h.player(0, from_int(Raw(-2000))) : 0;
  auto lf = h.w->spawn_loose_fire({0, 0, 0}, owner);
  C::Address other = 0;
  if (c.in[2] == 2)
    other = h.player(owner ? 1 : 0, from_int(2000));
  else if (c.in[2] == 1) {
    C::Spawn s;
    s.position = {from_int(1000), 0, 0};
    other = h.w->spawn_skelight(s);
  } else
    other = h.of_table(0x50f208).at(0);
  h.w->write(0x4f719c, 4, c.in[3]);
  h.w->write(C::FRAME, 4, c.in[0]);
  h.w->clear_history();
  auto seed = Raw(h.w->read(C::SHARED_SEED));
  auto why = refusal([&] { h.w->touch(lf, other); });
  std::istringstream out(c.out);
  std::string tag;
  out >> tag;
  auto v = words(out);
  if (tag == "NONE") {
    check(why.empty() && !last(*h.w, "damage_apply") && !last(*h.w, "LooseFire_onTouch"),
          c.name + " no damage: '" + why + "'");
  } else if (tag == "ENTRY") {
    // The entry (0x15, v, v, 0) through the real damage_apply (fire-touch.md
    // section 2): it always hits, and the burn +0x340 is stored as 2 - 2 x the
    // fire resistance (0 here). A touch makes 7 shared draws, 8 or 9 with a
    // player thrower (its stun roll, and a critical roll with a bonus).
    check(v[0] == 0x15 && v[1] == v[2] && v[3] == 0, c.name + " entry shape");
    auto note = last(*h.w, "LooseFire_onTouch");
    check(note && note->args.size() == 4 && note->args[2] == 0x15 && note->args[3] == v[1],
          c.name + " fire entry value");
    auto apply = last(*h.w, "damage_apply");
    check(why.empty() && apply && Raw(apply->args[0]) == other &&
              Raw(apply->args[1]) == (v[6] ? owner : lf) && Raw(apply->args[2]) == v[4] &&
              Raw(apply->args[3]) == v[5],
          c.name + " damage_apply with the fire entry: '" + why + "'");
    int draws = 0;
    for (Raw s = seed; s != Raw(h.w->read(C::SHARED_SEED)) && draws < 64; ++draws)
      s = Raw(std::uint64_t(s) * 0x19660d + 0x3c6ef35f);
    check(h.w->read(other + 0x340) == 0x40000000 && draws >= 7 && draws <= 9,
          c.name + " the burn " + std::to_string(h.w->read(other + 0x340)) + " and the draws: " +
              std::to_string(draws));
  } else {
    auto note = last(*h.w, "damage_apply");
    check(note && note->args.size() == 4 && Raw(note->args[0]) == other &&
              Raw(note->args[1]) == (v[2] ? owner : lf) && Raw(note->args[2]) == v[0] &&
              Raw(note->args[3]) == v[1],
          c.name + " empty-descriptor damage_apply arguments");
  }
}
void gas(const Case &c) {
  Host h;
  // in: seed frame x y z life radius word owner damage
  C::Address owner = c.in[8] ? h.player(0, from_int(Raw(-2000))) : 0;
  h.w->write(C::FRAME, 4, c.in[1]);
  h.w->write(C::SHARED_SEED, 4, c.in[0]);
  h.w->clear_history();
  auto obj = h.w->spawn_poison_gas({c.in[2], c.in[3], c.in[4]}, c.in[5], c.in[6], c.in[7], owner, c.in[9]);
  std::vector<Raw> got;
  for (unsigned off : {0x0c, 0x1c, 0x20, 0x44, 0x48, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c})
    got.push_back(Raw(h.w->read(obj + off)));
  check(got == c.obj, c.name + " poisonGasSpell words");
  check(h.w->target(obj + 0x4c) == owner, c.name + " owner handle");
  int draws = 0;
  for (const auto &e : h.w->events())
    draws += e.name == "shared_draw";
  check(Raw(h.w->read(C::SHARED_SEED)) == c.seed[0] && Raw(draws) == c.seed[1], c.name + " one shared draw");
  // poisonGasSpell_run 0x1fd0f0 (combat round 6, combat_runtime_round6.cpp):
  // it runs until its end frame +0x5c, then slot 1 with 3 runs
  // poisonGasSpell_dtor 0x3c7878. A radius of 0x7fffffff asks for 2^31 / 10
  // particles a frame, which the World's history budget refuses.
  h.w->write(C::FRAME, 4, c.in[1] + 1);
  std::string why;
  try {
    h.w->run_actor(obj);
  } catch (const std::exception &e) {
    why = e.what();
  }
  if (c.in[6] == 0x7fffffff) {
    check(why.find("budget") != std::string::npos, c.name + " a huge radius: '" + why + "'");
    return;
  }
  check(why.empty() && alive(*h.w, obj) == (c.in[5] > 1), c.name + " the gas run: '" + why + "'");
  if (!alive(*h.w, obj))
    return;
  h.w->write(C::FRAME, 4, Raw(h.w->read(obj + 0x5c)));
  why = refusal([&] { h.w->run_actor(obj); });
  check(why.empty() && !alive(*h.w, obj), c.name + " the cloud ends through its destructor: '" + why + "'");
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: potion_ctors_tests vectors.txt\n";
    return 2;
  }
  std::ifstream f(argv[1]);
  std::vector<Case> cases;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    std::string tag;
    ss >> tag;
    if (tag == "CASE") {
      cases.push_back({});
      ss >> cases.back().name;
      continue;
    }
    auto &c = cases.back();
    if (tag == "LOOSEFIRE" || tag == "FIRERUN" || tag == "LFRUN" || tag == "TOUCH" || tag == "GAS") {
      c.kind = tag;
      c.in = words(ss);
    } else if (tag == "OBJ")
      c.obj = words(ss);
    else if (tag == "FIRE")
      c.fires.push_back(words(ss));
    else if (tag == "SEED")
      c.seed = words(ss);
    else if (tag == "GLOW")
      c.glow = words(ss);
    else if (tag == "PARTICLE")
      c.particle = words(ss);
    else if (tag == "DELETE")
      c.deleted = true;
    else if (tag == "AFTER")
      c.after = words(ss);
    else if (tag == "ASKED")
      ss >> c.asked;
    else if (tag == "OUT")
      std::getline(ss >> std::ws, c.out);
  }
  for (const auto &c : cases) {
    try {
      if (c.kind == "LOOSEFIRE")
        loosefire(c);
      else if (c.kind == "FIRERUN")
        fire_run(c);
      else if (c.kind == "LFRUN")
        loosefire_run(c);
      else if (c.kind == "TOUCH")
        touch(c);
      else
        gas(c);
    } catch (const std::exception &e) {
      check(false, c.name + " threw " + e.what());
    }
  }
  std::cout << cases.size() << " potion cases, " << checks << " checks, " << failures << " failures\n";
  return failures || cases.empty() ? 1 : 0;
}
