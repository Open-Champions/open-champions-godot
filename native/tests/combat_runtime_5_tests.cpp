#include "champions/combat_runtime.hpp"
#include "champions/fx_trig.hpp"
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
// Round-5 host checks and the exact vector replay.
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
namespace T = champions::rta::fx::trig;
namespace {
int checks = 0;
struct Mismatch : std::logic_error {
  using std::logic_error::logic_error;
};
void check(bool b, const std::string &s) {
  ++checks;
  if (!b)
    throw Mismatch(s);
}
struct Host {
  C::Services services;
  std::unique_ptr<C::World> w;
  Raw ground = 0;
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
    services.height_below = [this](const Vec3 &, Raw) { return ground; };
    services.water_test = [](const Vec3 &, Raw) { return Raw(0); };
    services.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
    services.animation_advance = [](Raw, Raw) { return Raw(0); };
    services.animation_root = [](C::Address) { return std::array<Raw, 2>{0, 0}; };
    services.skill_stat = [](const std::string &, Raw, Raw) { return Raw(0); };
    services.camera_yaw = [](Raw) { return Raw(0); };
    services.camera_flag = [](Raw) { return Raw(0); };
    services.attach_point = [](C::Address, Raw, const Vec3 &pos, Raw, Raw) { return pos; };
    services.sample_point_count = [](Raw) { return Raw(0); };
    services.resource = [](const C::ResourceQuery &) { return Raw(0); };
    w = std::make_unique<C::World>(services, 1, C::Composition::Full);
  }
  C::Address creature(const char *cls, Raw y = 0) {
    C::Spawn s;
    s.position[1] = y;
    s.hp = 0x42f00000;
    return w->spawn_class(cls, s);
  }
};
int count_event(const C::World &w, const std::string &name) {
  int n = 0;
  for (const auto &e : w.events())
    if (e.name == name)
      ++n;
  return n;
}
bool threw_message(const std::function<void()> &fn, const std::string &part) {
  try {
    fn();
  } catch (const std::exception &e) {
    return std::string(e.what()).find(part) != std::string::npos;
  }
  return false;
}
void live_fight() {
  Host h;
  auto queen = h.creature("AntQueen");
  auto skeleton = h.creature("Skeleton", from_int(40));
  auto soul = h.creature("Soul", from_int(-40));
  for (int t = 0; t < 20; ++t) {
    h.w->write(C::FRAME, 4, static_cast<Raw>(t));
    h.w->run_actor(queen);
    h.w->run_actor(skeleton);
    h.w->run_actor(soul);
  }
  check(h.w->read(queen + 0x232, 2) == 0, "AntQueen stays in state 0 beside the fight");
  check(h.w->read(skeleton + 0x232, 2) == 0, "Skeleton stays in state 0 beside the fight");
  check(h.w->read(soul + 0x232, 2) == 0, "Soul stays in state 0 beside the fight");

  Host phase;
  auto s = phase.creature("Soul");
  phase.w->write(C::FRAME, 4, 9);
  phase.w->write(s + 0x230, 1, 0);
  phase.w->write(s + 0x44, 4, 9);
  phase.w->clear_history();
  phase.w->run_actor(s);
  check(count_event(*phase.w, "Soul_isDemonNear") == 0,
        "frame 9 with phase byte 0 does not call the demon test");
  phase.w->write(s + 0x230, 1, 9);
  phase.w->clear_history();
  phase.w->run_actor(s);
  check(count_event(*phase.w, "Soul_isDemonNear") == 1,
        "frame 9 with phase byte 9 calls the demon test");
  check(count_event(*phase.w, "world_collectObjectsInBox") == 1, "the demon box is collected");

  Host gate;
  auto far_q = gate.creature("AntQueen", from_int(10000));
  auto defin = gate.w->read(far_q + 0x24);
  gate.w->write(defin + 0xb4, 4, 0x11);
  gate.w->write(far_q + 0x118, 1, 1);
  gate.w->write(far_q + 0x11c, 4, 0x11);
  gate.w->write(far_q + 0xf4, 4, 1000);
  gate.w->run_actor(far_q);
  check(gate.w->read(far_q + 0x128) == 0xbd088888u, "an inactive AntQueen still fades");
  check(gate.w->read(far_q + 0x232, 2) == 0, "the fade does not leave state 0");
  auto far_s = gate.creature("Skeleton", from_int(10000));
  gate.w->write(far_s + 0x300, 4, 5);
  gate.w->write(far_s + 0xf4, 4, 0);
  gate.w->write(far_s + 0x3d0, 4, 4);
  gate.w->run_actor(far_s);
  check(gate.w->read(far_s + 0x232, 2) == 0, "an inactive Skeleton does not take the expired transition");
  auto far_soul = gate.creature("Soul", from_int(10000));
  gate.w->write(C::FRAME, 4, 9);
  gate.w->write(far_soul + 0x230, 1, 9);
  gate.w->clear_history();
  gate.w->run_actor(far_soul);
  check(count_event(*gate.w, "Soul_isDemonNear") == 0, "an inactive Soul does not test for a demon");

  Host bow;
  auto sk = bow.creature("Skeleton");
  bow.w->write(sk + 0x3d0, 4, 4);
  bow.w->run_actor(sk);
  check(bow.w->read(sk + 0x232, 2) == 0, "a bow skeleton with nothing to see stays in state 0");
}
Raw hexword(const std::string &s) { return Raw(std::stoul(s, nullptr, 16)); }
std::vector<Raw> words_of(std::istringstream &ss) {
  std::vector<Raw> out;
  std::string w;
  while (ss >> w)
    out.push_back(hexword(w));
  return out;
}
struct Exact {
  std::string name;
  bool refuse = false;
  std::vector<std::pair<Raw, Raw>> hits, faces;
  std::vector<Raw> gibhead, gibangle, bank, fade, out, volley, shieldset, demonbox;
  std::vector<std::vector<Raw>> demonlists, soulphase;
  Raw gibseed = 0, result = 0, age = 0;
  bool end = false, has_age = false, has_result = false;
  Raw end_reason = 0;
  std::vector<std::vector<Raw>> particles;
};
std::vector<Exact> load_vectors(const char *path) {
  std::ifstream in(path);
  if (!in)
    throw Mismatch(std::string("cannot read ") + path);
  std::vector<Exact> out;
  Exact cur;
  auto flush = [&] {
    if (!cur.name.empty())
      out.push_back(cur);
    cur = {};
  };
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    std::string tag;
    ss >> tag;
    if (tag == "CASE") {
      flush();
      ss >> cur.name;
    } else if (tag == "HIT") {
      auto row = words_of(ss);
      cur.hits.push_back({row.at(0), row.at(1)});
    } else if (tag == "FACE") {
      auto row = words_of(ss);
      cur.faces.push_back({row.at(0), row.at(1)});
    } else if (tag == "GIBHEAD")
      cur.gibhead = words_of(ss);
    else if (tag == "GIBSEED")
      cur.gibseed = words_of(ss).at(0);
    else if (tag == "GIBANGLE")
      cur.gibangle = words_of(ss);
    else if (tag == "BANK")
      cur.bank = words_of(ss);
    else if (tag == "FADE")
      cur.fade = words_of(ss);
    else if (tag == "OUT")
      cur.out = words_of(ss);
    else if (tag == "REFUSE")
      cur.refuse = true;
    else if (tag == "RESULT") {
      cur.result = words_of(ss).at(0);
      cur.has_result = true;
    } else if (tag == "PARTICLE")
      cur.particles.push_back(words_of(ss));
    else if (tag == "VOLLEY")
      cur.volley = words_of(ss);
    else if (tag == "SHIELDSET")
      cur.shieldset = words_of(ss);
    else if (tag == "DEMONBOX")
      cur.demonbox = words_of(ss);
    else if (tag == "DEMONLIST")
      cur.demonlists.push_back(words_of(ss));
    else if (tag == "SOULPHASE")
      cur.soulphase.push_back(words_of(ss));
    else if (tag == "AGE") {
      cur.age = words_of(ss).at(0);
      cur.has_age = true;
    } else if (tag == "END") {
      cur.end = true;
      cur.end_reason = words_of(ss).at(0);
    } else
      throw Mismatch("unknown vector tag " + tag);
  }
  flush();
  return out;
}
std::vector<Raw> span(const C::Event &e, unsigned from, unsigned count) {
  std::vector<Raw> out;
  for (unsigned i = 0; i < count; ++i)
    out.push_back(Raw(e.args.at(from + i)));
  return out;
}
void expect_rows(const std::vector<std::vector<Raw>> &got, const std::vector<std::vector<Raw>> &want,
                 const std::string &what) {
  check(got.size() == want.size(), what + " count " + std::to_string(got.size()));
  for (std::size_t i = 0; i < got.size() && i < want.size(); ++i)
    for (std::size_t k = 0; k < want[i].size(); ++k)
      check(got[i].at(k) == want[i][k], what + " " + std::to_string(i) + " word " + std::to_string(k));
}
void plant_tracks(C::World &w, C::Address a, Raw count, const std::vector<Raw> &clips, Raw clip_a,
                  Raw clip_b, Raw keep) {
  auto defin = w.read(a + 0x24);
  for (unsigned i = 0; i < 4; ++i)
    w.write(defin + (i == 0 ? 0xb4 : i == 1 ? 0x44 : i == 2 ? 0xbc : 0x4c), 4, clips[i]);
  w.write(a + 0x118, 1, count & 0xff);
  w.write(a + 0x11c, 4, clip_a);
  w.write(a + 0x13c, 4, clip_b);
  w.write(a + 0x124, 4, keep);
  w.write(a + 0x144, 4, keep);
}
void replay(const Exact &c) {
  if (c.name == "angles") {
    for (auto [hit, want] : c.hits)
      check(T::process_damage_hit_angle(hit) == want, "hit angle");
    for (auto [facing, want] : c.faces)
      check(T::process_damage_facing_angle(facing) == want, "facing angle");
    check(T::process_damage_gib_angle(c.gibhead.at(0), c.gibhead.at(1), c.gibhead.at(2),
                                      c.gibhead.at(3)) == c.gibhead.at(4),
          "gib heading");
    std::uint32_t seed = 0x12345678, hit = 0x100;
    std::vector<Raw> angles;
    for (auto row : {std::array<std::uint32_t, 3>{2, 16000, 0xe0c0},
                     std::array<std::uint32_t, 3>{4, 40000, 0xb1e0}})
      for (std::uint32_t chunk = 0; chunk < row[0]; ++chunk) {
        for (int draw = 0; draw < 2; ++draw)
          seed = seed * 0x19660du + 0x3c6ef35fu;
        seed = seed * 0x19660du + 0x3c6ef35fu;
        auto heading = T::process_damage_gib_angle(hit, seed >> 16, row[1], row[2]);
        angles.push_back(heading);
        angles.push_back(heading);
      }
    check(seed == c.gibseed, "gib seed");
    check(angles == c.gibangle, "gib angles");
    return;
  }
  if (!c.bank.empty() || !c.fade.empty()) {
    Host h;
    auto a = h.creature("AntQueen");
    if (!c.bank.empty()) {
      h.w->write(a + 0x110, 2, c.bank.at(0));
      h.w->write(a + 0x3e0, 4, c.bank.at(8));
      h.w->write(a + 0x3dc, 4, c.bank.at(9));
      plant_tracks(*h.w, a, c.bank.at(1), {c.bank.at(2), c.bank.at(3), c.bank.at(4), c.bank.at(5)},
                   c.bank.at(6), c.bank.at(7), c.bank.at(10));
      h.w->update_bank(a);
      check(h.w->read(a + 0x3e0) == c.out.at(0), c.name + " vel");
      check(h.w->read(a + 0x3dc) == c.out.at(1), c.name + " pos");
      check(h.w->read(a + 0x3e4, 2) == c.out.at(2), c.name + " angle");
      check(h.w->read(a + 0x124) == c.out.at(3), c.name + " weight 0");
      check(h.w->read(a + 0x144) == c.out.at(4), c.name + " weight 1");
    } else {
      plant_tracks(*h.w, a, c.fade.at(0), {c.fade.at(1), c.fade.at(2), c.fade.at(3), c.fade.at(4)},
                   c.fade.at(5), c.fade.at(6), 0);
      h.w->fade_bank_tracks(a);
      check(h.w->read(a + 0x128) == c.out.at(0), c.name + " rate 0");
      check(h.w->read(a + 0x148) == c.out.at(1), c.name + " rate 1");
      check(h.w->read(a + 0x124) == c.out.at(2), c.name + " weight untouched");
    }
    return;
  }
  if (c.refuse) {
    Host h;
    C::Spawn s;
    auto player = h.w->spawn_player(0, s);
    C::SkillSetup holy;
    holy.stats[0] = {1, 100, 0};
    h.w->give_skill(16, holy);
    auto spell = h.w->arm_fixture_holy_shield(player);
    auto model = h.w->read(h.w->read(player + 0x24) + 12);
    h.w->write(model + 0x64, 4, 0);
    std::string got;
    try {
      h.w->holy_shield_sample(spell);
      got = "returned";
    } catch (const std::exception &e) {
      got = e.what();
    }
    check(got.find("sample count of 0") != std::string::npos, "a sample count of 0 is refused: " + got);
    return;
  }
  if (!c.shieldset.empty()) {
    // facing, position, own seed, point count, points (x y z word), joint
    // count, then 12 halfwords per joint.
    const auto &v = c.shieldset;
    Host h;
    C::Spawn s;
    auto player = h.w->spawn_player(0, s);
    C::SkillSetup holy;
    holy.stats[0] = {1, 100, 0};
    h.w->give_skill(16, holy);
    auto spell = h.w->arm_fixture_holy_shield(player);
    auto model = h.w->read(h.w->read(player + 0x24) + 12);
    auto pose = h.w->read(player + 0x114);
    h.w->write(player + 0x38, 2, v.at(0));
    for (unsigned k = 0; k < 3; ++k)
      h.w->write(player + 0x10 + 4 * k, 4, v.at(1 + k));
    h.w->write(spell + 0x10, 4, v.at(4));
    h.w->write(spell + 0x1c, 4, 0x3f800000);
    h.w->write(spell + 0x24, 4, 7);
    h.w->write(spell, 1, 1);
    auto points = v.at(5);
    h.w->write(model + 0x64, 4, points);
    std::size_t at = 6;
    for (Raw i = 0; i < points; ++i)
      for (unsigned col = 0; col < 4; ++col)
        h.w->write(model + 0x80 + 8 * i + 2 * col, 2, v.at(at++));
    auto joints = v.at(at++);
    h.w->write(pose + 8, 2, joints);
    for (Raw j = 0; j < joints; ++j)
      for (unsigned k = 0; k < 12; ++k)
        h.w->write(pose + 0x10 + 24 * j + 2 * k, 2, v.at(at++));
    check(at == v.size(), "shield set-up words");
    h.w->clear_history();
    check(h.w->holy_shield_sample(spell) == c.result, c.name + " result");
    std::vector<std::vector<Raw>> particles;
    for (const auto &e : h.w->events())
      if (e.name == "presentation:particle_spawn")
        particles.push_back(span(e, 0, 8));
    expect_rows(particles, c.particles, c.name + " particle");
    check(particles.size() == 0xf1, c.name + " has 241 particles");
    return;
  }
  if (!c.demonbox.empty()) {
    const auto &v = c.demonbox;
    auto place = [&](Host &h, const char *cls, Raw dx) {
      C::Spawn s;
      s.position = {add(v.at(0), dx), v.at(1), v.at(2)};
      s.hp = 0x42f00000;
      return h.w->spawn_class(cls, s);
    };
    {
      Host h;
      auto soul = place(h, "Soul", 0);
      h.w->clear_history();
      check(h.w->is_demon_near(soul) == 0, "no demon in an empty box");
      bool seen = false;
      for (const auto &e : h.w->events())
        if (e.name == "world_collectObjectsInBox") {
          seen = true;
          check(span(e, 0, 7) == std::vector<Raw>(v.begin() + 3, v.end()), "demon box arguments");
        }
      check(seen, "the demon box is collected");
    }
    // A list entry is (demon << 1) | flag 8. Demons are Critters whose class
    // table's slot-0 getter is Demon_typeinfo; the others are Skelights.
    for (const auto &row : c.demonlists) {
      Host h;
      auto soul = place(h, "Soul", 0);
      for (std::size_t k = 1; k < row.size(); ++k) {
        bool demon = row[k] & 2;
        auto obj = place(h, demon ? "Critter" : "Skelight", from_int(Raw(k * 10)));
        if (demon)
          h.w->write(h.w->read(obj + 0x48) + 4, 4, 0x3ca81c);
        if (row[k] & 1)
          h.w->write(obj + 0xc, 4, h.w->read(obj + 0xc) | 8);
      }
      check(Raw(h.w->is_demon_near(soul)) == row.at(0), "demon list answer");
    }
    return;
  }
  if (!c.soulphase.empty()) {
    for (const auto &row : c.soulphase) {
      Host h;
      auto soul = h.creature("Soul");
      h.w->write(soul + 0x232, 2, row.at(0));
      h.w->write(C::FRAME, 4, row.at(1));
      h.w->write(soul + 0x230, 1, row.at(2));
      h.w->clear_history();
      h.w->run_actor(soul);
      check(Raw(count_event(*h.w, "Soul_isDemonNear")) == row.at(3),
            "soul phase state " + std::to_string(row[0]) + " frame " + std::to_string(row[1]) +
                " byte " + std::to_string(row[2]));
    }
    return;
  }
  if (c.has_result) {
    Host h;
    C::Spawn s;
    s.position = {0x41200000, 0x41a00000, 0x41f00000};
    auto player = h.w->spawn_player(0, s);
    C::SkillSetup holy;
    holy.stats[0] = {1, 100, 0};
    h.w->give_skill(16, holy);
    auto spell = h.w->arm_fixture_holy_shield(player);
    auto model = h.w->read(h.w->read(player + 0x24) + 12);
    h.w->write(player + 0x10, 4, 0x41200000);
    h.w->write(player + 0x14, 4, 0x41a00000);
    h.w->write(player + 0x18, 4, 0x41f00000);
    h.w->write(player + 0x38, 2, 0x1000);
    h.w->write(model + 0x80, 2, 16);
    h.w->write(spell + 0x10, 4, 1);
    h.w->write(spell + 0x1c, 4, 0x3f800000);
    h.w->write(spell + 0x24, 4, 7);
    h.w->write(spell, 1, 1);
    h.w->clear_history();
    auto got = h.w->holy_shield_sample(spell);
    check(got == c.result, "holy shield sample result");
    std::vector<std::vector<Raw>> particles;
    for (const auto &e : h.w->events())
      if (e.name == "presentation:particle_spawn")
        particles.push_back(span(e, 0, 8));
    expect_rows(particles, c.particles, "holy shield particle");
    check(particles.size() == 0xf1, "241 holy shield particles");
    return;
  }
  if (!c.volley.empty()) {
    Host h;
    h.ground = c.volley.at(3);
    auto obj = h.creature("Critter");
    h.w->write(obj + 0x60, 4, c.volley.at(0));
    h.w->write(obj + 0x6c, 4, c.volley.at(1));
    h.w->write(obj + 0x44, 4, c.volley.at(2));
    h.w->write(obj + 0x74, 4, 0xab);
    h.w->write(obj + 0x10, 4, 0x41000000);
    h.w->write(obj + 0x14, 4, 0x41800000);
    h.w->write(obj + 0x18, 4, 0x41f00000);
    h.w->write(obj + 0x54, 4, 0);
    h.w->write(obj + 0x58, 4, 0);
    h.w->write(obj + 0x5c, 4, 0);
    h.w->clear_history();
    h.w->multifire_run(obj);
    if (c.has_age)
      check(h.w->read(obj + 0x6c) == c.age, c.name + " age");
    int ends = 0;
    Raw reason = 0;
    std::vector<std::vector<Raw>> particles;
    for (const auto &e : h.w->events()) {
      if (e.name == "MultiFireProjectile_end") {
        ++ends;
        reason = Raw(e.args.at(1));
      } else if (e.name == "presentation:particle_spawn")
        particles.push_back(span(e, 0, 8));
    }
    check(c.end == (ends == 1), c.name + " end call");
    if (c.end)
      check(reason == c.end_reason, c.name + " end reason");
    expect_rows(particles, c.particles, c.name + " particle");
    return;
  }
  throw Mismatch("unknown case " + c.name);
}
void vectors(const char *path) {
  auto cases = load_vectors(path);
  check(cases.size() == 18, "vector file has the eighteen round-5 cases");
  for (const auto &c : cases)
    replay(c);
}
} // namespace
int main(int argc, char **argv) {
  try {
    set_fpu();
    if (argc == 2)
      vectors(argv[1]);
    else
      live_fight();
    std::cout << checks << " round-5 checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
