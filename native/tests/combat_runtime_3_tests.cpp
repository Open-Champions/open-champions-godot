#include "champions/combat_runtime.hpp"
#include "champions/core_state.hpp"
#include <array>
#include <optional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
// Round-3 host checks. Exact numeric vectors are combat-runtime-3-exact.
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
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
    services.raycast = [](const Vec3 &, const Vec3 &, Raw, Raw) {
      return MINUS_ONE;
    };
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
    services.attach_point = [](C::Address, Raw, const Vec3 &pos, Raw, Raw) {
      return pos;
    };
    services.sample_point_count = [](Raw) { return Raw(0); };
    services.resource = [](const C::ResourceQuery &) { return Raw(0xA10000); };
    w = std::make_unique<C::World>(services, 1, C::Composition::Full);
  }
  C::Address creature(const Vec3 &pos) {
    C::Spawn s;
    s.position = pos;
    return w->spawn_skelight(s);
  }
  bool event_is(const std::string &name, const C::Event &e) const {
    return e.name == name;
  }
};
bool has_event(const C::World &w, const std::string &name) {
  for (auto &e : w.events())
    if (e.name == name)
      return true;
  return false;
}
int count_event(const C::World &w, const std::string &name) {
  int n = 0;
  for (auto &e : w.events())
    if (e.name == name)
      ++n;
  return n;
}
const C::Event *find_event(const C::World &w, const std::string &name) {
  for (auto &e : w.events())
    if (e.name == name)
      return &e;
  return nullptr;
}
void host() {
  {
    Host h;
    auto victim = h.creature({0, 0, 0});
    auto owner = h.creature({0, 0, 0});
    h.w->write(C::FRAME, 4, 60);
    h.w->write(0x41b1e4, 4, 0x41b000);
    auto cloud = h.w->spawn_disease_cloud({0, 0, 0}, 3, owner);
    h.w->write(cloud + 0x44, 4, 0x12345678);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->write(cloud + 0x60, 4, 20);
    h.w->clear_history();
    h.w->run_actor(cloud);
    check(h.w->read(cloud + 0x5c) == add(0x41200000, HALF), "cloud radius grows by 1/2");
    check(count_event(*h.w, "presentation:particle_spawn") == 12,
          "four particles on each of three streams");
    auto hit = find_event(*h.w, "damage_apply");
    check(hit && hit->args.size() >= 4 && hit->args[0] == victim && hit->args[1] == 0 &&
              hit->args[2] == 100000 && hit->args[3] == 0x1000040,
          "cloud damage has no attacker");
    auto spells = h.w->attached_spells(victim);
    check(spells.size() == 1, "DiseaseEffect attached");
    auto spell = spells[0];
    check(h.w->read(spell + 0x1a4) == 5, "no resistance lasts five seconds");
    check(h.w->read(spell + 0x1a8) == 0, "disease level is 0");
    check(h.w->read(spell + 0x1ac) == 0x41b000, "disease skill word");
    check(h.w->read(spell + 0x14) == 0x518db0, "disease spell table");
    check(h.w->read(spell + 8) == 0, "spell base skill word is null");
    auto pools = count_event(*h.w, "pool_new");
    h.w->clear_history();
    h.w->run_actor(cloud);
    check(h.w->attached_spells(victim).size() == 1, "a second hit does not allocate");
    check(!has_event(*h.w, "creature_addSpell"), "no second disease allocation");
    (void)pools;
  }
  {
    Host h;
    auto owner = h.creature({0, 0, 0});
    h.w->write(owner + 12, 4, h.w->read(owner + 12) | 0x40);
    auto victim = h.creature({0, 0, 0});
    h.w->write(victim + 12, 4, h.w->read(victim + 12) | 0x40);
    h.w->write(C::FRAME, 4, 60);
    auto cloud = h.w->spawn_disease_cloud({0, 0, 0}, 1, owner);
    h.w->write(cloud + 0x58, 4, 10);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->clear_history();
    h.w->run_actor(cloud);
    check(!has_event(*h.w, "damage_apply"), "two players are skipped");
  }
  {
    Host h;
    auto victim = h.creature({0, 0, 0});
    h.w->write(C::FRAME, 4, 60);
    auto cloud = h.w->spawn_disease_cloud({0, 0, 0}, 1, 0);
    h.w->write(cloud + 0x58, 4, 10);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->clear_history();
    h.w->run_actor(cloud);
    check(!has_event(*h.w, "damage_apply"), "an empty owner handle deals nothing");
    (void)victim;
  }
  {
    Host h;
    auto owner = h.creature({0, 0, 0});
    h.w->write(C::FRAME, 4, 60);
    auto cloud = h.w->spawn_disease_cloud({0, 0, 0}, 1, owner);
    h.w->write(cloud + 0x54, 4, 60);
    h.w->run_actor(cloud);
    check(!h.w->contains(cloud), "the cloud destroys itself at its end frame");
  }
  {
    Host h;
    auto victim = h.creature({0, 0, 0});
    h.w->write(victim + 0x68, 4, 0x3f800000);
    h.w->clear_history();
    auto made = h.w->attach_disease(victim, 0);
    check(made != 0, "the pool still returns the effect");
    check(has_event(*h.w, "creature_addSpell"), "addSpell runs with the block");
    check(h.w->read(made + 0x1a4) == 0, "full resistance is zero seconds");
  }
  {
    Host h;
    h.w->write(0x44fdc0, 4, 0);
    h.w->write(0x4f86a0 + 0x58, 4, 0x111);
    h.w->write(0x4f86a0 + 0x60, 4, 0x222);
    Vec3 pos{0x3f800000, 0x40000000, 0x40400000};
    auto chunk = h.w->spawn_gut_chunk(pos, 2, 0);
    check(h.w->read(chunk + 0x248) == 1, "gore below 2 sets the blood bit");
    check(h.w->read(chunk + 0x48) == 0x50f3b8, "GutChunk table");
    check(h.w->read(chunk + 0x25c) == 2 && h.w->read(chunk + 0x260, 1) == 0,
          "GutChunk timer and flag");
    check(h.w->read(chunk + 0x250) == pos[0], "PhProp position is copied");
    auto call = find_event(*h.w, "PhProp_ctor");
    check(call && call->args.size() > 6 && call->args[6] == 0x111, "name at +0x58");
    h.w->write(0x44fdc0, 4, 2);
    auto chunk2 = h.w->spawn_gut_chunk({0, 0, 0}, 1, 2);
    check(h.w->read(chunk2 + 0x248) == 2, "gore at 2 keeps the bits");
    auto call2 = find_event(*h.w, "PhProp_ctor");
    // The second call is the later one; scan from the end.
    const C::Event *last = nullptr;
    for (auto &e : h.w->events())
      if (e.name == "PhProp_ctor")
        last = &e;
    check(last && last->args.size() > 6 && last->args[6] == 0x222, "bit 1 selects +0x60");
    (void)call2;
  }
  {
    Host h;
    auto target = h.creature({from_int(8), 0, 0});
    h.w->write(target + 0x2e0, 2, 0x20);
    h.w->write(target + 0x24, 4, 0x1000);
    h.w->write(0x1000, 4, 0x2000);
    auto phantom = h.w->spawn_phantom(target, "knockdown1_a.anm", 4, 1);
    check(h.w->read(phantom + 0x3cc) == 0x43700000 && h.w->read(phantom + 0x3d0) == 0x43700000,
          "flag 1 lasts 240");
    check(h.w->read(phantom + 0x3d4) == 1, "phantom flag");
    check(h.w->read(phantom + 0x1c) == 0x41c00000, "phantom radius 24");
    check(h.w->read(phantom + 0x48) == 0x50f448, "phantom table");
    check(h.w->read(phantom + 0x2e0, 2) == 0x20, "facing word copied");
    check(h.w->target(phantom + 0x3c8) == target, "target linked");
    auto brief = h.w->spawn_phantom(target, "knockdown1_a.anm", 0, 0);
    check(h.w->read(brief + 0x3cc) == 0x42960000, "flag 0 lasts 75");
    h.w->write(phantom + 0x3cc, 4, 0x3f800000);
    h.w->clear_history();
    h.w->run_actor(phantom);
    check(h.w->read(phantom + 0x3cc) == 0 || !h.w->contains(phantom), "timer 1 deletes");
    check(has_event(*h.w, "obj_delete"), "obj_delete at the end");
    h.w->write(brief + 0x3cc, 4, 0x40000000);
    h.w->clear_history();
    h.w->run_actor(brief);
    check(h.w->contains(brief) && h.w->read(brief + 0x3cc) == 0x3f800000,
          "timer 2 becomes 1 and stays");
    check(!has_event(*h.w, "obj_delete"), "no delete while the timer remains");
  }
  {
    Host h;
    auto owner = h.creature({0, 0, 0});
    auto hit_kind = [&](int kind) {
      h.ground = from_int(100);
      C::Missile m;
      m.kind = kind;
      m.position = {0x3f800000, 0x40000000, 0x40400000};
      m.velocity = {0, 0, 0};
      m.impact = 0x55aa;
      auto arrow = h.w->spawn_missile(owner, m);
      h.w->write(arrow + 0x44, 4, 0x1234);
      h.w->clear_history();
      h.w->run_actor(arrow);
      return arrow;
    };
    auto miss = [&](int kind) {
      h.ground = 0;
      C::Missile m;
      m.kind = kind;
      m.position = {0, 0, from_int(36)};
      m.velocity = {0, 0, 0};
      auto arrow = h.w->spawn_missile(owner, m);
      h.w->clear_history();
      h.w->run_actor(arrow);
      return arrow;
    };
    auto stayed = miss(1);
    check(h.w->contains(stayed), "a miss does not destroy the arrow");
    check(has_event(*h.w, "presentation:light_relink"), "a miss still relinks the light");
    check(!has_event(*h.w, "destroy"), "a miss leaves the expiry");
    auto kind2 = hit_kind(2);
    check(!h.w->contains(kind2), "kind 2 hit destroys the arrow");
    check(has_event(*h.w, "presentation:fx_ringBurst"), "kind 2 ring");
    int relink = -1, destroyed = -1, n = 0;
    for (auto &e : h.w->events()) {
      if (e.name == "presentation:light_relink" && relink < 0)
        relink = n;
      if (e.name == "destroy" && destroyed < 0)
        destroyed = n;
      ++n;
    }
    check(relink >= 0 && destroyed > relink, "destroy follows the light relink");
    h.w->clear_history();
    auto kind5 = hit_kind(5);
    check(!h.w->contains(kind5), "kind 5 hit destroys the arrow");
    check(count_event(*h.w, "presentation:particle_spawn") == 32, "kind 5 emits 32");
    auto fx = find_event(*h.w, "presentation:fx_spawn");
    check(fx && !fx->args.empty() && fx->args[0] == 10, "kind 5 spawns effect 10");
    check(!has_event(*h.w, "presentation:fx_ringBurst"), "kind 5 has no radius burst");
    auto kind1 = hit_kind(1);
    check(!h.w->contains(kind1), "kind 1 hit destroys the arrow");
    auto fx1 = find_event(*h.w, "presentation:fx_spawn");
    check(fx1 && !fx1->args.empty() && fx1->args[0] == 5, "kind 1 spawns effect 5");
    check(count_event(*h.w, "ParticleProp_ctor") >= 3 &&
              count_event(*h.w, "ParticleProp_ctor") <= 6,
          "kind 1 builds 3 to 6 props");
    check(count_event(*h.w, "presentation:particle_spawn") == 48, "kind 1 emits 24 pairs");
    auto kind6 = hit_kind(6);
    check(!h.w->contains(kind6), "kind 6 hit destroys the arrow");
    check(count_event(*h.w, "presentation:particle_spawn") == 100, "kind 6 emits 100");
    check(has_event(*h.w, "player_getPassiveLevel") == false, "a creature owner has no passive level");
    C::Spawn sp;
    sp.position = {0, 0, 0};
    auto player = h.w->spawn_player(0, sp);
    h.ground = from_int(100);
    C::Missile bow;
    bow.kind = 6;
    bow.position = {0, 0, from_int(36)};
    bow.velocity = {0, 0, 0};
    auto owned = h.w->spawn_missile(player, bow);
    h.w->clear_history();
    h.w->run_actor(owned);
    check(has_event(*h.w, "player_getPassiveLevel"), "kind 6 asks the player's passive level");
    h.ground = from_int(100);
    C::Missile stone;
    stone.kind = 0;
    stone.position = {0, 0, from_int(3)};
    stone.velocity = {0, 0, 0};
    auto rock = h.w->spawn_missile(owner, stone);
    h.w->clear_history();
    h.w->run_actor(rock);
    check(!has_event(*h.w, "presentation:light_relink"), "kinds 0 and 3 keep the stone hit");
    check(has_event(*h.w, "presentation:sound_playOnObject"), "stone hit still plays");
  }
  {
    Host h;
    auto p = [&] {
      C::Spawn s;
      s.position = {0, 0, 0};
      return h.w->spawn_player(0, s);
    }();
    C::Thrown t;
    t.kind = 1;
    t.position = {0, 0, from_int(36)};
    auto item = h.w->spawn_thrown(p, t);
    bool refused = false;
    try {
      h.w->run_actor(item);
    } catch (const C::Unsupported &e) {
      refused = std::string(e.what()).find("potion") != std::string::npos;
    }
    check(refused, "thrown potions stay refused");
    bool silent = false;
    try {
      h.w->block_sparks(p, 1);
    } catch (const C::Unsupported &e) {
      silent = std::string(e.what()).find("silent") != std::string::npos;
    }
    check(silent, "silent block sparks stay refused");
    auto victim = h.creature({0, 0, 0});
    h.w->attach_disease(victim, 0);
    bool skill = false;
    try {
      h.w->run_actor(victim);
    } catch (const C::Unsupported &e) {
      skill = std::string(e.what()).find("SpellDiseaseEffect_run") != std::string::npos;
    }
    check(skill, "the disease spell run stays refused");
    (void)item;
  }
  {
    Host h;
    auto owner = h.creature({0, 0, 0});
    h.w->write(C::FRAME, 4, 60);
    auto cloud = h.w->spawn_disease_cloud({0x3f800000, 0x40000000, 0x40400000}, 1, owner);
    h.w->write(cloud + 0x44, 4, 0x12345678);
    h.w->write(cloud + 0x58, 4, 48);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->write(cloud + 0x54, 4, 1000);
    // Keep the frame off a damage tick so the object seed is only the streams.
    h.w->write(C::FRAME, 4, 61);
    h.w->clear_history();
    h.w->run_actor(cloud);
    check(h.w->read(cloud + 0x5c) == 0x41280000, "exact radius 10.5");
    const C::Event *first = nullptr;
    int n = 0;
    for (auto &e : h.w->events())
      if (e.name == "presentation:particle_spawn") {
        if (!first)
          first = &e;
        ++n;
      }
    check(n == 12, "exact stream count");
    check(first && first->args.size() >= 7 && first->args[0] == 0x8e5320 &&
              first->args[1] == 0x3df3f800 && first->args[2] == 0x41054f00 &&
              first->args[3] == 0xc08ca120 && first->args[4] == 0xc01dec00 &&
              first->args[5] == 0x3e69a000 && first->args[6] == 0x4093ff00,
          "exact first disease particle");
    (void)owner;
  }
  std::cout << checks << " native combat runtime round-3 checks passed\n";
}

struct Exact {
  std::string name;
  std::optional<Raw> radius, expiry;
  std::vector<std::array<Raw, 7>> particles;
  std::vector<Raw> fx, samples;
  std::optional<std::array<Raw, 4>> ring, light;
};
Raw hexword(const std::string &s) {
  return Raw(std::stoul(s, nullptr, 16));
}
std::vector<Exact> load_vectors(const char *path) {
  std::ifstream in(path);
  if (!in)
    throw Mismatch("cannot open vectors");
  std::vector<Exact> out;
  Exact cur;
  std::string line;
  auto flush = [&] {
    if (!cur.name.empty())
      out.push_back(cur);
    cur = {};
  };
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    std::string tag;
    ss >> tag;
    if (tag == "CASE") {
      flush();
      ss >> cur.name;
    } else if (tag == "RADIUS") {
      std::string w;
      ss >> w;
      cur.radius = hexword(w);
    } else if (tag == "EXPIRY") {
      std::string w;
      ss >> w;
      cur.expiry = hexword(w);
    } else if (tag == "P") {
      std::array<Raw, 7> row{};
      for (auto &w : row) {
        std::string t;
        ss >> t;
        w = hexword(t);
      }
      cur.particles.push_back(row);
    } else if (tag == "FX" || tag == "SAMPLE") {
      std::string w;
      ss >> w;
      (tag == "FX" ? cur.fx : cur.samples).push_back(hexword(w));
    } else if (tag == "RING" || tag == "LIGHT") {
      std::array<Raw, 4> row{};
      for (auto &w : row) {
        std::string t;
        ss >> t;
        w = hexword(t);
      }
      (tag == "RING" ? cur.ring : cur.light) = row;
    } else
      throw Mismatch("unknown vector tag " + tag);
  }
  flush();
  return out;
}
std::vector<std::array<Raw, 7>> spawned(const C::World &w) {
  std::vector<std::array<Raw, 7>> rows;
  for (auto &e : w.events()) {
    if (e.name != "presentation:particle_spawn" || e.args.size() < 7)
      continue;
    std::array<Raw, 7> row{};
    for (unsigned i = 0; i < 7; ++i)
      row[i] = Raw(e.args[i]);
    rows.push_back(row);
  }
  return rows;
}
void expect_particles(const C::World &w, const Exact &c) {
  auto got = spawned(w);
  check(got.size() == c.particles.size(), c.name + " particle count");
  for (std::size_t i = 0; i < got.size() && i < c.particles.size(); ++i)
    check(got[i] == c.particles[i], c.name + " particle " + std::to_string(i));
}
C::Address far_owner(Host &h) {
  return h.creature({from_int(2000), 0, 0});
}
void replay(const Exact &c) {
  if (c.name == "cloud") {
    Host h;
    auto owner = far_owner(h);
    h.w->write(C::FRAME, 4, 61);
    auto cloud = h.w->spawn_disease_cloud({0x3f800000, 0x40000000, 0x40400000}, 1, owner);
    h.w->write(cloud + 0x44, 4, 0x12345678);
    h.w->write(cloud + 0x58, 4, 48);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->write(cloud + 0x54, 4, 1000);
    h.w->clear_history();
    h.w->run_actor(cloud);
    check(c.radius && h.w->read(cloud + 0x5c) == *c.radius, "cloud radius word");
    expect_particles(*h.w, c);
    return;
  }
  int kind = 0;
  bool hit = true;
  if (c.name == "arrow5") {
    kind = 5;
    hit = false;
  } else if (c.name == "arrow1")
    kind = 1;
  else if (c.name == "arrow2")
    kind = 2;
  else if (c.name == "arrow6")
    kind = 6;
  else
    throw Mismatch("unknown case " + c.name);
  Host h;
  auto owner = far_owner(h);
  h.ground = hit ? from_int(100) : 0;
  h.w->write(C::FRAME, 4, 60);
  C::Missile m;
  m.kind = kind;
  m.position = {0x3f800000, 0x40000000, 0x40400000};
  m.velocity = {0, 0, 0};
  auto arrow = h.w->spawn_missile(owner, m);
  h.w->write(arrow + 0x44, 4, 0x1234);
  h.w->write(arrow + 0xd8, 4, 40);
  h.w->write(arrow + 0xec, 4, 1000);
  h.w->clear_history();
  h.w->run_actor(arrow);
  if (c.expiry)
    check(h.w->read(arrow + 0xec) == *c.expiry, c.name + " expiry");
  expect_particles(*h.w, c);
  for (auto id : c.fx) {
    auto e = find_event(*h.w, "presentation:fx_spawn");
    check(e && !e->args.empty() && e->args[0] == id, c.name + " fx");
  }
  for (auto sample : c.samples) {
    auto e = find_event(*h.w, "presentation:sound_playOnObject");
    check(e && !e->args.empty() && e->args[0] == sample, c.name + " sample");
  }
  if (c.ring) {
    auto e = find_event(*h.w, "presentation:fx_ringBurst");
    check(e && e->args.size() >= 4 && e->args[0] == (*c.ring)[0] &&
              e->args[1] == (*c.ring)[1] && e->args[2] == (*c.ring)[2] &&
              e->args[3] == (*c.ring)[3],
          c.name + " ring");
  }
  if (c.light) {
    auto e = find_event(*h.w, "LightEffect_ctor");
    check(e && e->args.size() >= 8 && e->args[4] == (*c.light)[0] &&
              e->args[5] == (*c.light)[1] && e->args[6] == (*c.light)[2] &&
              e->args[7] == (*c.light)[3],
          c.name + " light");
  }
  check(hit ? !h.w->contains(arrow) : h.w->contains(arrow), c.name + " lifetime");
}
void vectors(const char *path) {
  auto cases = load_vectors(path);
  check(!cases.empty(), "vector file has cases");
  for (auto &c : cases)
    replay(c);
  std::cout << checks << " exact round-3 vector checks passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2)
      vectors(argv[1]);
    else
      host();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
