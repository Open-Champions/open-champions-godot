#include "champions/combat_runtime.hpp"
#include "champions/arena_monsters.hpp"
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
    services.water_disturb = [](const Vec3 &, Raw, Raw) { return Raw(0); };
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
bool refused_self(C::World &w, C::Address cloud) {
  try {
    w.run_actor(cloud);
  } catch (const C::Unsupported &e) {
    return std::string(e.what()).find("DiseaseCloud damage_apply on itself") !=
           std::string::npos;
  }
  return false;
}
void host() {
  {
    // The victim and owner sit one cell column below the cloud, so the box walk
    // meets them before the cloud, which is in its own box and refuses.
    Host h;
    auto victim = h.creature({from_int(-5), 0, 0});
    auto owner = h.creature({from_int(-5), 0, 0});
    h.w->write(C::FRAME, 4, 60);
    h.w->write(0x41b1e4, 4, 0x41b000);
    auto cloud = h.w->spawn_disease_cloud({0, 0, 0}, 3, owner);
    h.w->write(cloud + 0x44, 4, 0x12345678);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->write(cloud + 0x60, 4, 20);
    h.w->clear_history();
    check(refused_self(*h.w, cloud), "the cloud's damage_apply on itself is refused");
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
    check(refused_self(*h.w, cloud), "the second tick refuses at the cloud again");
    check(h.w->attached_spells(victim).size() == 1, "a second hit does not allocate");
    check(!has_event(*h.w, "creature_addSpell"), "no second disease allocation");
    (void)pools;
  }
  {
    Host h;
    auto owner = h.creature({from_int(-5), 0, 0});
    h.w->write(owner + 12, 4, h.w->read(owner + 12) | 0x40);
    auto victim = h.creature({from_int(-5), 0, 0});
    h.w->write(victim + 12, 4, h.w->read(victim + 12) | 0x40);
    h.w->write(C::FRAME, 4, 60);
    auto cloud = h.w->spawn_disease_cloud({0, 0, 0}, 1, owner);
    h.w->write(cloud + 0x58, 4, 10);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->clear_history();
    check(refused_self(*h.w, cloud), "the cloud itself is not a player and is refused");
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
    // Slot 1 with 3 reaches DiseaseCloud_dtor 0x3cd480: only the symbol is
    // in the research (runtime.tsv), so ending the cloud is refused.
    std::string refusal;
    try {
      h.w->run_actor(cloud);
    } catch (const C::Unsupported &e) {
      refusal = e.what();
    }
    check(refusal.find("DiseaseCloud_dtor 0x3cd480") != std::string::npos,
          "the cloud's end-frame destructor is refused: '" + refusal + "'");
    check(!has_event(*h.w, "destroy"), "the cloud is not destroyed by a guessed body");
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
    // The round-3 constructor research now covers the complete chain.
    Host h;
    Vec3 pos{0x3f800000, 0x40000000, 0x40400000};
    auto chunk = h.w->spawn_gut_chunk(pos, 2, 0);
    check(h.w->contains(chunk) && h.w->read(chunk + 0x48) == 0x50f3b8,
          "GutChunk builds the PhProp chain and installs its table");
    check(h.w->read(chunk + 0x190) == ONE,
          "GutChunk's rigid body has the executed inverse mass");
    std::string refusal;
    try { h.w->run_actor(chunk); }
    catch (const arena_monsters::Unsupported &e) { refusal = e.what(); }
    check(refusal.find("model_getHullPoints returned null") != std::string::npos,
          "the absent loaded model hull stays refused");
  }
  {
    Host h;
    auto chunk = h.w->spawn_gut_chunk({0, 0, 0}, 2, 0);
    std::vector<std::uint8_t> vif(0x40 + 64 * 24);
    vif[0x25] = 4;
    for (unsigned i = 0; i < 24; ++i)
      vif[0x40 + i] = std::uint8_t(i - 12);
    h.w->bind_prop_model(chunk, {vif.data(), vif.size()});
    const auto model = h.w->read(chunk + 0x4c);
    check(model != 0xA10000 && h.w->read(C::Address(model) + 0x25, 1) == 4 &&
              h.w->read(C::Address(model) + 0x40, 1) == std::uint8_t(-12),
          "the PhProp model pointer resolves an owned signed VIF hull");
    // This authored stationary case does not need the unbound horizontal
    // terrain sweep service. The hull loop and remaining tick still execute.
    h.w->write(chunk + 0x1bc, 4, 0);
    h.w->write(chunk + 0x1c0, 4, 0);
    h.ground = ONE;
    h.w->run_actor(chunk);
    check(h.w->read(chunk + 0x228) == ONE,
          "the completed prop tick records the supplied terrain height");
    vif[0x25] = 0;
    bool refused = false;
    try { h.w->bind_prop_model(chunk, {vif.data(), vif.size()}); }
    catch (const champions::FormatError &) { refused = true; }
    check(refused && h.w->read(C::Address(model) + 0x25, 1) == 4,
          "a missing disc hull refuses without replacing the bound model");
    check(h.w->read(chunk + 0x4c) == model,
          "invalid hull leaves the owned destination selected");
    vif[0x25] = 4;
    bool truncated = false;
    try { h.w->bind_prop_model(chunk, {vif.data(), 0x40 + 23}); }
    catch (const champions::FormatError &) { truncated = true; }
    check(truncated && h.w->read(chunk + 0x4c) == model,
          "a truncated hull refuses without replacing the owned destination");
    auto other = h.creature({0, 0, 0});
    bool wrong_actor = false;
    try { h.w->bind_prop_model(other, {vif.data(), vif.size()}); }
    catch (const C::Unsupported &) { wrong_actor = true; }
    check(wrong_actor, "an unrelated actor cannot bind a prop model");
    constexpr C::Address foreign = 0xA10000;
    h.w->write(foreign, 4, 0x12345678);
    h.w->write(chunk + 0x4c, 4, foreign);
    bool wrong_pointer = false;
    try { h.w->bind_prop_model(chunk, {vif.data(), vif.size()}); }
    catch (const C::Unsupported &) { wrong_pointer = true; }
    check(wrong_pointer && h.w->read(foreign) == 0x12345678,
          "a modified model destination is refused before writing");
  }
  {
    Host h;
    h.w->write(0x4f7120, 4, 1); // Mode B is selected by the caller.
    h.w->begin_arena_intro();
    check(h.w->read(0x5038c8, 2) == 180 && h.w->read(0x503890) == 1,
          "wave intro begins with the observed lock and 180 visits");
    for (unsigned visit = 0; visit < 180; ++visit) {
      h.w->arena_intro_visit();
      check(h.w->read(0x5038c8, 2) == 179 - visit &&
                h.w->read(0x503890) == 1 &&
                h.w->read(0x41b4fc) == 1 && h.w->read(0x41b500) == 1,
            "each positive intro visit preserves all three locks");
    }
    h.w->arena_intro_visit();
    check(h.w->read(0x5038c8, 2) == 0xffff &&
              h.w->read(0x503890) == 0 &&
              h.w->read(0x41b4fc) == 0 && h.w->read(0x41b500) == 0,
          "visit 181 releases the creature state gate");
  }
  {
    world_queries::LevelData data;
    data.tg_w = data.tg_h = 1;
    data.grid = {{0}};
    world_queries::HeightField field;
    field.w = field.h = 1;
    field.samples = {0};
    data.heightfields.push_back(field);
    data.elements.push_back(world_queries::make_element(
        0, -100, -100, 100, 100, 0, 0, 0, 0, 0, 32767, 0));
    world_queries::Level terrain(data);
    auto services = C::with_terrain({}, terrain);
    check(services.raycast({0, 0, from_int(5)}, {from_int(20), 0, 0},
                           3, ONE) == MINUS_ONE,
          "selected terrain gives the clear-line ray answer, not a zero stub");
    check(services.height_below({0, 0, from_int(5)}, from_int(4)) == 0,
          "sight and movement services share the same selected terrain");
  }
  {
    Host h;
    auto target = h.creature({from_int(8), 0, 0});
    h.w->write(target + 0x2e0, 2, 0x20);
    h.w->write(target + 0x24, 4, 0x1000);
    h.w->write(0x1000, 4, 0x2000);
    // StrongStancePhantom_ctor 0x1618d4 calls Creature_ctor 0x109d68 with the
    // target's class definition. The research Creature_ctor body
    // (ref_arena_monsters.creature_ctor) calls GameObject_ctor with that
    // definition, which the combat World does not compose; refuse there.
    std::string refusal;
    try {
      h.w->spawn_phantom(target, "knockdown1_a.anm", 4, 1);
    } catch (const C::Unsupported &e) {
      refusal = e.what();
    }
    check(refusal.find("Creature_ctor 0x109d68") != std::string::npos,
          "the phantom's Creature_ctor is refused: '" + refusal + "'");
    check(!has_event(*h.w, "obj_runEveryFrame"), "no half-built phantom is scheduled");
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
    // Kind 1 on the ground (result 1): fx_spawn 5 builds Fire objects that
    // draw the shared seed (research-fx effects.md row 5); not executed.
    bool fire = false;
    try {
      hit_kind(1);
    } catch (const C::Unsupported &e) {
      fire = std::string(e.what()).find("fx_spawn id 5") != std::string::npos;
    }
    check(fire, "a kind-1 ground hit refuses at fx_spawn 5");
    check(!has_event(*h.w, "presentation:fx_spawn"), "effect 5 is not noted as presentation");
    {
      // Kind 1 into a creature (result 2, no ground): no effect 5, but the
      // (draw & 3) + 3 ParticleProp blocks; ParticleProp_ctor 0x1549e8 is a
      // Prop (GameObject_ctor, one shared draw) whose body is not read.
      Host t;
      auto shooter = t.creature({from_int(2000), 0, 0});
      C::Spawn enemy;
      enemy.team = 1; // the shooter's slot 20 must call it an enemy
      t.w->spawn_skelight(enemy);
      C::Missile m;
      m.kind = 1;
      m.position = {0, 0, from_int(36)};
      m.velocity = {0, 0, 0};
      auto arrow = t.w->spawn_missile(shooter, m);
      t.w->clear_history();
      std::string refusal;
      try {
        t.w->run_actor(arrow);
      } catch (const C::Unsupported &e) {
        refusal = e.what();
      }
      check(refusal.find("ParticleProp_ctor") != std::string::npos,
            "a kind-1 creature hit refuses at ParticleProp_ctor: '" + refusal + "'");
      check(!has_event(*t.w, "presentation:fx_spawn"), "a creature hit has no effect 5");
    }
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
    // Clear of the player. An overlap sets the move result and the ending runs.
    t.position = {from_int(400), 0, from_int(36)};
    auto item = h.w->spawn_thrown(p, t);
    h.w->write(C::SHARED_SEED, 4, 1);
    h.w->clear_history();
    h.w->run_actor(item);
    check(h.w->contains(item), "a potion in flight stays");
    check(count_event(*h.w, "presentation:particle_spawn") == 3,
          "kind 1 emits three flames");
    h.w->clear_history();
    h.w->block_sparks(p, 1);
    check(count_event(*h.w, "model_getAttachPoint") == 2,
          "silent sparks with no samples return after the hands");
    check(!has_event(*h.w, "presentation:particle_spawn"),
          "a sample count of 0 spawns nothing");
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
  // (role, attacker, attack, flags, descriptor slot 0) per damage_apply call.
  std::vector<std::pair<std::string, std::vector<Raw>>> damage;
  std::optional<Raw> seed;
  std::vector<Raw> spell, swarm;
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
    } else if (tag == "DAMAGE") {
      std::string role, t;
      ss >> role;
      std::vector<Raw> row;
      while (ss >> t)
        row.push_back(hexword(t));
      cur.damage.emplace_back(role, row);
    } else if (tag == "SEED") {
      std::string t;
      ss >> t;
      cur.seed = hexword(t);
    } else if (tag == "SPELL" || tag == "SWARM") {
      std::string t;
      auto &out = tag == "SPELL" ? cur.spell : cur.swarm;
      while (ss >> t)
        out.push_back(hexword(t));
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
  if (c.name == "disease-attach") {
    Host h;
    auto victim = h.creature({0, 0, 0});
    h.w->write(victim + 0x1c, 4, 0x41c00000);  // 24.0
    h.w->write(victim + 0x68, 4, 0x3e800000);  // disease resistance 0.25
    h.w->write(C::FRAME, 4, 77);
    h.w->write(C::SHARED_SEED, 4, 0x2468ace1);
    h.w->write(0x41b1e4, 4, 0x41b000);
    h.w->clear_history();
    auto block = h.w->attach_disease(victim, 0);
    check(block != 0, "disease-attach block");
    check(c.seed && h.w->read(C::SHARED_SEED) == *c.seed, "disease-attach shared seed");
    const unsigned offs[] = {0x10, 0x14, 0x1a0, 0x1a4, 0x1a8, 0x1ac};
    check(c.spell.size() == 6, "disease-attach spell words");
    for (unsigned k = 0; k < 6; ++k)
      check(h.w->read(block + offs[k]) == c.spell[k],
            "disease-attach word +" + std::to_string(offs[k]));
    check(c.swarm.size() == 96, "disease-attach swarm width");
    for (unsigned k = 0; k < 96; ++k)
      check(h.w->read(block + 0x1c + 4 * k) == c.swarm[k],
            "disease-attach swarm word " + std::to_string(k));
    check(h.w->attached_spells(victim).size() == 1, "disease-attach is linked");
    return;
  }
  if (c.name == "cloud-tick") {
    // The victim sits one cell column below the cloud, so the grid walk meets
    // it first, as the research box callback lists it first. The cloud is in
    // its own box (GameObject_ctor links it into grid A, radius 24).
    Host h;
    auto owner = far_owner(h);
    auto victim = h.creature({from_int(-5), 0x40000000, 0});
    h.w->write(C::FRAME, 4, 60);
    auto cloud = h.w->spawn_disease_cloud({0x3f800000, 0x40000000, 0x40400000}, 1, owner);
    h.w->write(cloud + 0x44, 4, 0x12345678);
    h.w->write(cloud + 0x54, 4, 1000);
    h.w->write(cloud + 0x58, 4, 10);
    h.w->write(cloud + 0x5c, 4, 0x41200000);
    h.w->write(cloud + 0x60, 4, 20);
    h.w->clear_history();
    std::string refusal;
    try {
      h.w->run_actor(cloud);
    } catch (const C::Unsupported &e) {
      refusal = e.what();
    }
    check(c.radius && h.w->read(cloud + 0x5c) == *c.radius, "cloud-tick radius word");
    expect_particles(*h.w, c);
    std::vector<const C::Event *> hits;
    for (auto &e : h.w->events())
      if (e.name == "damage_apply")
        hits.push_back(&e);
    // Every research damage_apply on a creature is matched; the one on the
    // cloud itself is where the native World refuses (slot 2 of 0x5186f0).
    std::size_t matched = 0;
    for (auto &row : c.damage) {
      if (row.first == "self") {
        check(refusal.find("DiseaseCloud damage_apply on itself") != std::string::npos,
              "cloud-tick: the cloud's own damage_apply is refused: '" + refusal + "'");
        break;
      }
      check(matched < hits.size(), "cloud-tick damage call " + std::to_string(matched));
      auto &e = *hits[matched++];
      check(e.args.size() >= 4 && Raw(e.args[0]) == victim && Raw(e.args[1]) == row.second.at(0) &&
                Raw(e.args[2]) == row.second.at(1) && Raw(e.args[3]) == row.second.at(2),
            "cloud-tick damage arguments");
    }
    check(matched == hits.size(), "cloud-tick damage count");
    auto spells = h.w->attached_spells(victim);
    check(spells.size() == 1, "cloud-tick: the creature took the 0x26 disease entry");
    return;
  }
  int kind = 0;
  bool hit = true;
  if (c.name == "arrow5") {
    kind = 5;
    hit = false;
  } else if (c.name == "arrow1-flight") {
    kind = 1;
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
  if (c.name == "arrow1") {
    // The research hit calls fx_spawn 5 (3 or 4 Fire objects drawing the
    // shared seed, research-fx effects.md section 6) and ParticleProp_ctor;
    // neither body is executed, so the native World refuses at fx_spawn 5.
    check(c.fx.size() == 1 && c.fx[0] == 5, "arrow1: the research hit spawns effect 5");
    std::string refusal;
    try {
      h.w->run_actor(arrow);
    } catch (const C::Unsupported &e) {
      refusal = e.what();
    }
    check(refusal.find("fx_spawn id 5") != std::string::npos,
          "arrow1: the hit refuses at fx_spawn 5: '" + refusal + "'");
    check(!has_event(*h.w, "presentation:fx_spawn") && !has_event(*h.w, "ParticleProp_ctor"),
          "arrow1: no silent effect 5 or prop");
    return;
  }
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
