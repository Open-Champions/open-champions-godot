#include "champions/combat_runtime.hpp"
#include "champions/fx_trig.hpp"
#include "champions/own_machines.hpp"
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <set>
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
  // A creature object to carry the round-5 bodies that are called directly
  // (bank fade and spring, isDemonNear, the volley run): the authored Skelight
  // placement, the same pool block and layout the class spawns use.
  C::Address carrier(Raw y = 0) {
    C::Spawn s;
    s.position[1] = y;
    s.hp = 0x42f00000;
    return w->spawn_skelight(s);
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
bool has_event(const C::World &w, const std::string &name, std::uint64_t first) {
  for (const auto &e : w.events())
    if (e.name == name && !e.args.empty() && e.args[0] == first)
      return true;
  return false;
}
void player(Host &h) {
  C::Spawn s;
  s.team = 0; // the fixture players' team
  h.w->spawn_player(0, s);
}
C::Address place(Host &h, const char *cls, int x, int y) {
  C::Spawn s;
  s.position[0] = from_int(Raw(x));
  s.position[1] = from_int(Raw(y));
  s.facing = 0xc000;
  s.hp = 0x42f00000;
  return h.w->spawn_class(cls, s);
}
// The AntQueen, Skeleton and Soul constructors, set-states and run prologues
// in the World (research-combat-ctors e9777eb; combat_ctors.cpp), with the
// round-5 switch bodies for states 0 and 1.
void live_fight() {
  {
    // The Round-1 composition has none of their call-out services.
    C::World round1(Host().services, 1);
    for (const char *cls : {"AntQueen", "Skeleton", "Soul"})
      check(threw_message([&] { round1.spawn_class(cls, C::Spawn{}); }, "needs the Full composition"),
            std::string(cls) + " needs the Full composition");
  }
  {
    Host h;
    player(h);
    h.w->clear_history();
    auto q = place(h, "AntQueen", 0, 100);
    auto def = Raw(h.w->read(q + 0x24));
    check(h.w->read(q + 0x48) == 0x511078 && h.w->slot_function(q, 0) == 0x3c3c78 &&
              h.w->slot_function(q, 1) == 0x3c3c10 && h.w->slot_function(q, 2) == 0x2378fc &&
              h.w->slot_function(q, 4) == 0x236f78 && h.w->slot_function(q, 27) == 0x237918 &&
              h.w->slot_function(q, 23) == 0x10ded0,
          "AntQueen table: the documented overrides over the base");
    check(h.w->read(q + 0x232, 2) == 0 && h.w->read(q + 0xf4) == 0,
          "AntQueen_ctor stores state 0 without its entry (+0xf4 stays 0)");
    check(h.w->read(q + 0xb4, 1) == 0x13 && h.w->read(q + 0x1c) == 0x42340000 &&
              h.w->read(q + 0xb8) == 0x430c0000 && h.w->read(q + 0x28, 1) == 0x37 &&
              h.w->read(q + 0x5c) == 0x3ecccccc && h.w->read(q + 0x3d4) == 180 &&
              (h.w->read(q + 0xa8, 8) & 0x8020001000ull) == 0x8020001000ull &&
              !(h.w->read(q + 0xa8, 8) >> 63) && h.w->read(q + 0x220, 8) == UINT64_MAX,
          "AntQueen_ctor class words");
    check(h.w->read(q + 0x3d8) ==
              Raw(h.w->read(q + 0x300) + 180 + (h.w->read(q + 0x44) >> 16) % 180),
          "AntQueen_ctor's one own draw sets +0x3d8 = clock + 180 + d % 180");
    check(has_event(*h.w, "anim_push", q + 0x114) && has_event(*h.w, "Creature_init", q),
          "AntQueen_ctor pushes idle_a and calls Creature_init");
    int entries = 0;
    for (const auto &e : h.w->events())
      entries += e.name == "creature_setDefEntryParams" && !e.args.empty() && e.args[0] == q;
    check(entries == 5 && h.w->read(def + 0x7c) != 0, "five event-time entries");
    auto s = place(h, "Skeleton", 60, 100);
    check(h.w->read(s + 0x48) == 0x5175f8 && h.w->slot_function(s, 20) == 0x2a7e04 &&
              h.w->slot_function(s, 2) == 0x2a774c && h.w->slot_function(s, 12) == 0x2a7688,
          "Skeleton table");
    check(h.w->read(s + 0x50) == s + 0x3d8 && h.w->read(s + 0x3d8) == 0x50cda0,
          "Skeleton's embedded WeaponShield holder");
    check(h.w->read(s + 0x3d4, 1) == 0 && h.w->read(s + 0x3c0) == 0x447a0000 &&
              h.w->read(s + 0x3d0) < 5,
          "type name Skeleton matches no prefix: drawn kind, no side, HomeRange 1000");
    check(h.w->read(s + 0x232, 2) == 0 && h.w->read(s + 0xf4) == h.w->read(s + 0x300) + 120,
          "Skeleton_ctor calls Skeleton_setState(0): +0xf4 = clock + 120");
    auto holder = s + 0x3d8;
    auto bits = h.w->read(holder + 4, 1);
    check(bool(bits & 1) == bool(h.w->read(holder + 8, 1)) &&
              bool(bits & 2) == bool(h.w->read(holder + 0x98, 1)),
          "the executed holder slot 1 stores the hand parts and presence bits");
    auto soul = place(h, "Soul", -60, 100);
    check(h.w->read(soul + 0x48) == 0x517740 && h.w->slot_function(soul, 16) == 0x2ab298,
          "Soul table with Soul_getModel");
    check(h.w->read(soul + 0x310, 1) == 0 && h.w->read(soul + 0x312, 2) == 0xffff &&
              h.w->read(soul + 0x232, 2) == 0 && h.w->read(soul + 0xf4) == 0 &&
              (h.w->read(soul + 0xa8, 8) >> 63),
          "Soul_ctor: permanent charm on team 0, state 0 without its draw, active");
    // Research-combat-ctors-2 (combat-ctors-2.md sections 5 and 6).
    auto soul_hp = h.w->read(soul + 0x228), soul_seed = h.w->read(soul + 0x44);
    h.w->take_damage(soul, q, 0, from_int(5));
    check(h.w->read(soul + 0x228) == soul_hp && h.w->read(soul + 0x3b0, 2) != 0x7fff &&
              h.w->read(soul + 0x44) == soul_seed,
          "Soul_takeDamage ignores an attacker that is not a Demon");
    h.w->write(C::FRAME, 4, 77);
    h.w->take_damage(s, q, 0, from_int(5));
    check(h.w->read(s + 0x3b0, 2) == 0x7fff && h.w->read(s + 0x304) == 77,
          "Skeleton_takeDamage is Creature_takeDamage");
    h.w->clear_history();
    h.w->destroy(q);
    bool gone = true;
    for (auto a : h.w->actors())
      gone = gone && a != q;
    check(gone && has_event(*h.w, "sub_202500", q + 0x320) &&
              has_event(*h.w, "AnimState_dtor", q + 0x114),
          "AntQueen_dtor runs Creature_dtor and frees the block");
    h.w->clear_history();
    h.w->destroy(s);
    gone = true;
    for (auto a : h.w->actors())
      gone = gone && a != s;
    check(gone, "Skeleton_dtor frees the block");
  }
  {
    // The first sighting now enters the executed state-3 chase. This host
    // fixture supplies no terrain free-spot service, which is its boundary.
    Host h;
    player(h);
    auto s = place(h, "Skeleton", 60, 100);
    std::string error;
    int ticks = 0;
    try {
      for (; ticks < 400; ++ticks)
        h.w->tick();
    } catch (const std::exception &e) {
      error = e.what();
    }
    check(h.w->read(s + 0x232, 2) == 3 &&
              error.find("world_findFreeSpot terrain service missing") != std::string::npos,
          "the Skeleton enters its chase before the absent host terrain service: " + error);
  }
  {
    // AntQueen sees the player and enters state 3 (walk tracks, +0x3c4 =
    // 0xabe0); AntQueen_run now runs every state (antqueen-run.md).
    Host h;
    player(h);
    auto q = place(h, "AntQueen", 0, 100);
    std::string error;
    std::set<Raw> states;
    try {
      for (int t = 0; t < 400; ++t) {
        h.w->tick();
        states.insert(Raw(h.w->read(q + 0x232, 2)));
      }
    } catch (const std::exception &e) {
      error = e.what();
    }
    check(error.empty() && states.count(3) && states.count(8),
          "AntQueen chases in state 3 and takes off in state 8: " + error);
  }
  {
    // Far from every activity centre: the Skeleton and the Soul return before
    // their switch; the AntQueen does not test activity and leaves state 0.
    Host h;
    player(h);
    auto q = place(h, "AntQueen", 2000, 2000);
    auto s = place(h, "Skeleton", -2000, 2000);
    auto soul = place(h, "Soul", 2000, -2000);
    for (int t = 0; t < 130; ++t)
      h.w->tick();
    check(h.w->read(q + 0x232, 2) == 1, "an inactive AntQueen still runs her switch");
    check(h.w->read(s + 0x232, 2) == 0 && h.w->read(soul + 0x232, 2) == 0 &&
              !(h.w->read(soul + 0xa8, 8) >> 63),
          "an inactive Skeleton or Soul returns before its switch");
  }
  {
    // The Soul alone wanders in states 0 and 1; the demon test runs on the
    // frames where (g_simFrame ^ serial) & 15 is 0.
    Host h;
    player(h);
    auto soul = place(h, "Soul", -60, 100);
    int demon = 0, ticks = 0;
    for (; ticks < 300; ++ticks) {
      h.w->clear_history();
      h.w->tick();
      for (const auto &e : h.w->events())
        demon += e.name == "Soul_isDemonNear";
      check(h.w->read(soul + 0x232, 2) <= 1, "the Soul stays in states 0 and 1");
    }
    check(demon == 300 / 16 + 1, "a demon test every 16 frames: " + std::to_string(demon));
  }
  {
    // On airhub every Soul request becomes 0x22 before the filter.
    Host h;
    const std::string level = "AirHub";
    for (unsigned k = 0; k <= level.size(); ++k)
      h.w->write(0x41b721 + k, 1, k < level.size() ? Raw(std::uint8_t(level[k])) : 0);
    auto soul = place(h, "Soul", 0, 0);
    check(h.w->read(soul + 0x232, 2) == 0x22 &&
              h.w->read(soul + 0xf4) == h.w->read(soul + 0x300) + 300 &&
              (h.w->read(soul + 0xc) & 9) == 9,
          "a Soul on airhub enters 0x22 from its constructor");
  }
}
// The armed shield's run (0x2e8428) steps its clock on every player tick and
// ends through the base Spell destructor once the duration passes.
void shield_lifetime() {
  Host h;
  C::Spawn s;
  auto player = h.w->spawn_player(0, s);
  C::SkillSetup holy;
  holy.stats[0] = {1, 100, 0};
  holy.stats[1] = {1, 1, 0};
  h.w->give_skill(16, holy);
  auto spell = h.w->arm_fixture_holy_shield(player);
  check(h.w->read(spell + 0x20) == from_int(1), "the duration is statistic 1 in seconds");
  int destroyed_at = -1, runs = 0;
  for (int t = 0; t < 70 && destroyed_at < 0; ++t) {
    h.w->clear_history();
    h.w->run_actor(player);
    for (const auto &e : h.w->events()) {
      runs += e.name == "particleGroup_setMatrix";
      if (e.name == "destroy_spell" && !e.args.empty() && Raw(e.args[0]) == spell)
        destroyed_at = t;
    }
  }
  check(runs == 60 && destroyed_at == 60, "a 1 s shield runs 60 times and ends on the 61st call: " +
                                              std::to_string(runs) + ", " + std::to_string(destroyed_at));
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
    auto a = h.carrier();
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
      // A demon needs its own class table (its slot-0 getter is planted
      // below): Gnaw objects share VT_GNAW, the others the Skelight table.
      return std::string(cls) == "Gnaw" ? h.w->spawn_class(cls, s) : h.w->spawn_skelight(s);
    };
    {
      Host h;
      auto soul = place(h, "Skelight", 0);
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
    // A list entry is (demon << 1) | flag 8. Demons are Gnaws whose class
    // table's slot-0 getter is Demon_typeinfo; the others are Skelights.
    for (const auto &row : c.demonlists) {
      Host h;
      auto soul = place(h, "Skelight", 0);
      for (std::size_t k = 1; k < row.size(); ++k) {
        bool demon = row[k] & 2;
        auto obj = place(h, demon ? "Gnaw" : "Skelight", from_int(Raw(k * 10)));
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
    // Soul_run's phase gate, replayed on the decision module (the live World
    // Soul is checked in live_fight): (state, g_simFrame, byte +0x230, demon
    // calls).
    for (const auto &row : c.soulphase) {
      own_machines::View v;
      v.frame = row.at(1);
      v.serial = row.at(2);
      auto tick = own_machines::step("Soul", row.at(0), v);
      Raw demon = 0;
      for (const auto &call : tick.calls)
        demon += call == "demon";
      check(demon == row.at(3), "soul phase state " + std::to_string(row[0]) + " frame " +
                                    std::to_string(row[1]) + " byte " + std::to_string(row[2]));
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
    auto obj = h.carrier();
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
    else {
      live_fight();
      shield_lifetime();
    }
    std::cout << checks << " round-5 checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
