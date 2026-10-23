#include "champions/combat_runtime.hpp"
#include "champions/arena_monsters.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
// Round-4 host checks. Exact numeric vectors are combat-runtime-4-exact.
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
namespace {
int checks = 0;
constexpr Raw P10 = 0x41200000, P20 = 0x41a00000, P30 = 0x41f00000, V40 = 0x42200000,
              GRAV = 0x410fffff, FAR = 0x44fa0000;
constexpr C::Address LEVEL_TEXT = 0x44fe18, INNPIT = 0x535ed8;
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
  int points = 0;
  Vec3 sample{0x3f800000, 0x40000000, 0x40400000};
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
    services.sample_point_count = [this](Raw) { return Raw(points); };
    services.sample_point = [this](Raw, C::Address, const Vec3 &, Raw, Raw, Raw) {
      return sample;
    };
    services.resource = [](const C::ResourceQuery &) { return Raw(0xA10000); };
    w = std::make_unique<C::World>(services, 1, C::Composition::Full);
  }
  C::Address player_at(const Vec3 &pos, unsigned slot = 0) {
    C::Spawn s;
    s.position = pos;
    auto p = w->spawn_player(slot, s);
    w->write(p + 0x10, 4, pos[0]);
    w->write(p + 0x14, 4, pos[1]);
    w->write(p + 0x18, 4, pos[2]);
    return p;
  }
};
void plant(C::World &w, C::Address a, const char *s) {
  unsigned i = 0;
  for (; s[i]; ++i)
    w.write(a + i, 1, static_cast<unsigned char>(s[i]));
  w.write(a + i, 1, 0);
}
int count_event(const C::World &w, const std::string &name) {
  int n = 0;
  for (auto &e : w.events())
    if (e.name == name)
      ++n;
  return n;
}
bool has_event(const C::World &w, const std::string &name) {
  return count_event(w, name) != 0;
}
const C::Event *find_event(const C::World &w, const std::string &name) {
  for (auto &e : w.events())
    if (e.name == name)
      return &e;
  return nullptr;
}
bool refused(const std::function<void()> &fn, const std::string &part) {
  try {
    fn();
  } catch (const C::Unsupported &e) {
    return std::string(e.what()).find(part) != std::string::npos;
  }
  return false;
}
C::Address potion(Host &h, int kind, Raw vz, Raw expiry) {
  auto owner = h.player_at({FAR, 0, 0});
  C::Thrown t;
  t.kind = kind;
  t.position = {P10, P20, P30};
  t.velocity = {0, 0, vz};
  auto obj = h.w->spawn_thrown(owner, t);
  h.w->write(C::FRAME, 4, 100);
  h.w->write(obj + 0x10, 4, P10);
  h.w->write(obj + 0x14, 4, P20);
  h.w->write(obj + 0x18, 4, P30);
  h.w->write(obj + 0x1b0, 4, 0);
  h.w->write(obj + 0x1b4, 4, 0);
  h.w->write(obj + 0x1b8, 4, vz);
  h.w->write(obj + 0x1cc, 2, 0);
  h.w->write(obj + 0xec, 4, expiry);
  h.w->write(C::SHARED_SEED, 4, 1);
  h.w->clear_history();
  return obj;
}
void host() {
  {
    Host h;
    auto obj = potion(h, 0, 0, 1000);
    h.w->run_actor(obj);
    check(h.w->contains(obj), "kind 0 still flies");
  }
  {
    Host h;
    auto obj = potion(h, 1, V40, 180);
    h.w->retain_round2_refusals();
    check(refused([&] { h.w->run_actor(obj); }, "thrown potion"),
          "the round-2 corpus still refuses a potion");
  }
  {
    Host live, splash;
    live.ground = splash.ground = 0x42c80000;
    auto a = potion(live, 1, GRAV, 1000);
    // The kind-1 ending builds five LooseFire objects (potion-ctors.md); the
    // stone sound and the fire sound come before them.
    live.w->run_actor(a);
    auto b = potion(splash, 1, GRAV, 1000);
    splash.w->write(b + 0xcc, 1, 1);
    splash.w->write(C::SHARED_SEED, 4, 1);
    splash.w->clear_history();
    splash.w->run_actor(b);
    auto sounds = [](const C::World &w) { return count_event(w, "presentation:sound_playOnObject"); };
    check(sounds(*live.w) == sounds(*splash.w) && sounds(*live.w) >= 2,
          "a ground hit sounds whether or not the splash byte is set");
    auto loose = [](const C::World &w) {
      int n = 0;
      for (auto x : w.actors())
        n += w.read(x + 0x48) == 0x50e2b8;
      return n;
    };
    check(loose(*live.w) == 5 && loose(*splash.w) == 5, "kind 1 builds five LooseFire objects");
    check(count_event(*live.w, "fx_spawn") == 5, "each LooseFire runs fx_spawn id 5");
  }
  {
    Host hit, far;
    hit.ground = sub(P30, 0x3f800000);
    far.ground = add(P30, 0x40800000);
    plant(*hit.w, LEVEL_TEXT, "town");
    plant(*hit.w, INNPIT, "innpit");
    plant(*far.w, LEVEL_TEXT, "town");
    plant(*far.w, INNPIT, "innpit");
    auto a = potion(hit, 3, GRAV, 0);
    hit.w->run_actor(a);
    auto damage = find_event(*hit.w, "world_damageInRadius");
    check(damage && damage->args.size() >= 10 && damage->args[4] == 0x32 &&
              damage->args[5] == 0x40840 && damage->args[8] == 0x42200000 &&
              damage->args[9] == 0x43340000,
          "a blast outside innpit records the radius damage");
    check(count_event(*hit.w, "presentation:decal_add") == 1, "ground one unit away gets a decal");
    auto b = potion(far, 3, GRAV, 0);
    far.w->run_actor(b);
    check(!has_event(*far.w, "presentation:decal_add"), "ground exactly 4 away gets no decal");
    check(has_event(*far.w, "world_damageInRadius"), "the height gate does not skip the damage");
  }
  {
    Host h;
    plant(*h.w, LEVEL_TEXT, "InNpIt");
    plant(*h.w, INNPIT, "innpit");
    auto obj = potion(h, 3, GRAV, 0);
    h.w->run_actor(obj);
    check(!has_event(*h.w, "world_damageInRadius"), "innpit skips the blast damage");
    check(has_event(*h.w, "LightEffect_ctor"), "the blast still builds its light");
  }
  {
    Host h;
    h.points = 0;
    auto player = h.player_at({P10, P20, P30});
    C::SkillSetup setup;
    setup.stats[0] = {1, 1, 0};
    auto spell = h.w->attach_holy_shield(player, h.w->give_skill(16, setup), 1);
    check(refused([&] { h.w->holy_shield_absorb(spell); }, "sample count of 0"),
          "a shield with no samples stays refused");
  }
  {
    // skills-35-68 section 54: SpellHolyShield is 0x28 bytes on table
    // 0x519ce0 (slot 0 getter 0x3cf124, slot 1 0x3cf0c4, slot 2 0x2e8428).
    Host h;
    auto player = h.player_at({0, 0, 0});
    C::SkillSetup setup;
    setup.stats[0] = {1, 1, 0};
    auto skill = h.w->give_skill(16, setup);
    h.w->clear_history();
    auto spell = h.w->attach_holy_shield(player, skill, 1);
    auto pool = find_event(*h.w, "pool_new");
    check(pool && !pool->args.empty() && pool->args[0] == 0x28, "the Holy Shield block is 0x28 bytes");
    check(h.w->read(spell + 0x14) == 0x519ce0, "the shield uses table 0x519ce0");
    check(h.w->read(0x519ce0 + 4) == 0x3cf124 && h.w->read(0x519ce0 + 12) == 0x3cf0c4 &&
              h.w->read(0x519ce0 + 20) == 0x2e8428,
          "table 0x519ce0 slots 0, 1 and 2");
    check(h.w->read(0x519ce0 + 28) == 0 && h.w->read(0x519ce0 + 36) == 0,
          "slots 3 and 4 (base, address not documented) stay 0");
  }
  {
    Host h;
    h.points = 4;
    auto player = h.player_at({0, 0, 0});
    h.w->write(player + 0x58c, 4, 0);
    auto rec = C::PLAYERS;
    h.w->write(rec + 0xc0, 4, 0x42c80000);
    C::SkillSetup setup;
    setup.stats[0] = {1, 100, 0};
    h.w->attach_holy_shield(player, h.w->give_skill(16, setup), 1);
    h.w->take_damage(player, 0, 0, P10);
    auto frac = mul(from_int(100), 0x3c23d70a);
    auto left = sub(0x42c80000, mul(P10, frac));
    check(h.w->read(rec + 0xc0) == left, "Holy Shield pays its fraction from mana");
  }
  {
    Host h;
    auto player = h.player_at({0, 0, 0});
    h.w->clear_history();
    h.w->block_sparks(player, 0);
    check(count_event(*h.w, "presentation:particle_spawn") == 50, "silent 0 keeps the 50 sparks");
    h.w->retain_round2_refusals();
    check(refused([&] { h.w->block_sparks(player, 2); }, "silent"),
          "the corpus still refuses silent sparks");
  }
  {
    Host h;
    auto player = h.player_at({0, 0, 0});
    h.w->write(C::PLAYERS + 0x4b08, 2, 2);
    h.w->write(C::PLAYERS + 0x4b0a, 2, 1);
    h.w->write(C::PLAYERS + 0x4b0c, 2, 0xffff);
    C::Missile m;
    m.position = {0, 0, 0x42100000};
    auto arrow = h.w->spawn_missile(0, m);
    check(refused([&] { h.w->run_actor(arrow); }, "Dodge"),
          "a nonzero Dodge rank stays refused");
    (void)player;
  }
  {
    Host h;
    h.ground = 0x42c80000;
    auto owner = h.player_at({FAR, 0, 0});
    C::Missile m;
    m.kind = 1;
    m.position = {0, 0, 0x40400000};
    auto arrow = h.w->spawn_missile(owner, m);
    h.w->clear_history();
    check(refused([&] { h.w->run_actor(arrow); }, "fx_spawn id 5"),
          "Fire construction from fx_spawn 5 stays refused");
    check(!has_event(*h.w, "presentation:fx_spawn"), "kind 1 does not note fx_spawn 5");
  }
  {
    Host h;
    auto chunk = h.w->spawn_gut_chunk({0, 0, 0}, 1, 0);
    check(h.w->contains(chunk) && h.w->slot_function(chunk, 4) == 0x1edb18,
          "the GutChunk constructor installs its executed run slot");
    std::string failure;
    try { h.w->run_actor(chunk); }
    catch (const arena_monsters::Unsupported &e) { failure = e.what(); }
    check(failure.find("model_getHullPoints returned null") != std::string::npos,
          "the composed run stops at its missing loaded hull");
  }
  std::cout << checks << " round-4 host checks passed\n";
}
struct Exact {
  std::string name;
  std::optional<Raw> vz, spin, expiry, cooldown, result, seed;
  std::vector<std::vector<Raw>> particles, fires, decals, places, lights;
  std::vector<Raw> gas, ring;
};
Raw hexword(const std::string &s) { return Raw(std::stoul(s, nullptr, 16)); }
std::vector<Raw> words(std::istringstream &ss) {
  std::vector<Raw> out;
  std::string w;
  while (ss >> w)
    out.push_back(hexword(w));
  return out;
}
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
    } else if (tag == "VZ" || tag == "SPIN" || tag == "EXPIRY" || tag == "COOLDOWN" ||
               tag == "RESULT" || tag == "SEED") {
      auto value = words(ss).at(0);
      if (tag == "VZ")
        cur.vz = value;
      else if (tag == "SPIN")
        cur.spin = value;
      else if (tag == "EXPIRY")
        cur.expiry = value;
      else if (tag == "COOLDOWN")
        cur.cooldown = value;
      else if (tag == "RESULT")
        cur.result = value;
      else
        cur.seed = value;
    } else if (tag == "PARTICLE")
      cur.particles.push_back(words(ss));
    else if (tag == "FIRE")
      cur.fires.push_back(words(ss));
    else if (tag == "DECAL")
      cur.decals.push_back(words(ss));
    else if (tag == "GAS")
      cur.gas = words(ss);
    else if (tag == "RING")
      cur.ring = words(ss);
    else if (tag == "LPLACE")
      cur.places.push_back(words(ss));
    else if (tag == "LIGHT")
      cur.lights.push_back(words(ss));
    else
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
  for (std::size_t i = 0; i < got.size() && i < want.size(); ++i) {
    check(got[i].size() == want[i].size(), what + " width");
    for (std::size_t k = 0; k < got[i].size() && k < want[i].size(); ++k)
      check(got[i][k] == want[i][k], what + " " + std::to_string(i) + " word " + std::to_string(k));
  }
}
std::vector<const C::Event *> named(const C::World &w, const std::string &name) {
  std::vector<const C::Event *> out;
  for (auto &e : w.events())
    if (e.name == name)
      out.push_back(&e);
  return out;
}
void replay(const Exact &c) {
  if (c.name.rfind("potion-", 0) == 0) {
    int kind = c.name == "potion-gas" ? 2 : c.name == "potion-blast" ? 3 : 1;
    Host h;
    if (c.name == "potion-fire")
      h.ground = 0x40800000;
    if (c.name == "potion-blast") {
      h.ground = P30;
      plant(*h.w, LEVEL_TEXT, "InNpIt");
      plant(*h.w, INNPIT, "innpit");
    }
    auto obj = potion(h, kind, c.name == "potion-flight" ? V40 : GRAV,
                      c.name == "potion-flight" ? 180 : 0);
    if (!c.fires.empty() || !c.gas.empty()) {
      // The research reached LooseFire_ctor 0x202b70 / poisonGasSpell_ctor
      // 0x202de8 as stubbed call-outs (ref_combat_round4 at 95c17ae). The
      // World now runs both constructors (research-combat-ctors 98c081a;
      // potion_ctors.txt.gz replays them), and LooseFire_ctor draws the shared
      // seed 11 to 18 times, so the vector is compared up to and including the
      // first constructor's arguments: the flight particles, the first fire's
      // place and owner, or the gas cloud's words.
      const auto frame = Raw(h.w->read(C::FRAME));
      auto thrower = h.w->target(obj + 0x1c8);
      h.w->run_actor(obj);
      std::vector<std::vector<Raw>> particles;
      for (auto e : named(*h.w, "presentation:particle_spawn")) {
        if (e->args.at(0) == 0x870548)
          continue; // a Fire's own particle (none in this tick)
        particles.push_back(span(*e, 0, 8));
      }
      expect_rows(particles, c.particles, c.name + " particle");
      if (!c.fires.empty()) {
        std::vector<C::Address> loose;
        for (auto a : h.w->actors())
          if (h.w->read(a + 0x48) == 0x50e2b8)
            loose.push_back(a);
        check(loose.size() == 5, c.name + " five LooseFire objects");
        check(!loose.empty() && h.w->read(loose[0] + 0x10) == c.fires[0][0] &&
                  h.w->read(loose[0] + 0x14) == c.fires[0][1] &&
                  h.w->read(loose[0] + 0x18) == c.fires[0][2] &&
                  h.w->target(loose[0] + 0x4c) == thrower &&
                  h.w->read(loose[0] + 0x50) == frame + 120,
              c.name + " the first LooseFire at the recorded place, owned by the thrower");
        check(named(*h.w, "presentation:decal_add").size() == 5, c.name + " five decals");
      } else {
        C::Address gas = 0;
        for (auto a : h.w->actors())
          if (h.w->read(a + 0x48) == 0x50d500)
            gas = a;
        check(gas && h.w->read(gas + 0x50) == c.gas[0] && h.w->read(gas + 0x54) == c.gas[1] &&
                  h.w->read(gas + 0x58) == c.gas[2] && h.w->read(gas + 0x5c) == frame + c.gas[3] &&
                  h.w->read(gas + 0x60) == c.gas[4] && h.w->read(gas + 0x68) == c.gas[5] &&
                  h.w->read(gas + 0x6c) == c.gas[6] && h.w->target(gas + 0x4c) == thrower,
              c.name + " the gas cloud carries the recorded arguments");
      }
      return;
    }
    h.w->run_actor(obj);
    if (c.vz)
      check(h.w->read(obj + 0x1b8) == *c.vz, c.name + " vz");
    if (c.spin)
      check(h.w->read(obj + 0x1cc, 2) == *c.spin, c.name + " spin");
    if (c.expiry)
      check(h.w->read(obj + 0xec) == *c.expiry, c.name + " expiry");
    std::vector<std::vector<Raw>> particles, fires, decals;
    for (auto e : named(*h.w, "presentation:particle_spawn"))
      particles.push_back(span(*e, 0, 8));
    for (auto e : named(*h.w, "LooseFire_ctor"))
      fires.push_back(span(*e, 1, 3));
    for (auto e : named(*h.w, "presentation:decal_add"))
      decals.push_back(span(*e, 0, 7));
    expect_rows(particles, c.particles, c.name + " particle");
    expect_rows(fires, c.fires, c.name + " fire");
    expect_rows(decals, c.decals, c.name + " decal");
    if (!c.gas.empty()) {
      auto gas = named(*h.w, "poisonGasSpell_ctor");
      check(gas.size() == 1, c.name + " gas");
      auto got = span(*gas[0], 1, 6);
      got.push_back(Raw(gas[0]->args.at(8)));
      check(got == c.gas, c.name + " gas words");
    }
    if (!c.ring.empty()) {
      auto ring = named(*h.w, "presentation:fx_ringBurst");
      check(ring.size() == 1 && span(*ring[0], 0, 4) == c.ring, c.name + " ring");
    }
    auto lights = named(*h.w, "LightEffect_ctor");
    std::vector<std::vector<Raw>> places, colors;
    for (auto e : lights) {
      places.push_back(span(*e, 1, 3));
      colors.push_back(span(*e, 4, 12));
    }
    expect_rows(places, c.places, c.name + " light place");
    expect_rows(colors, c.lights, c.name + " light");
    return;
  }
  if (c.name.rfind("shield-", 0) == 0) {
    Host h;
    h.points = 4;
    auto player = h.player_at({P10, P20, P30});
    C::SkillSetup setup;
    setup.stats[0] = {1, c.name == "shield-due" ? 100 : 0, 0};
    auto spell = h.w->attach_holy_shield(player, h.w->give_skill(16, setup), 1);
    if (c.name == "shield-held")
      h.w->write(spell + 0x1c, 4, 0x3f800000);
    h.w->write(C::SHARED_SEED, 4, 1);
    h.w->clear_history();
    auto got = h.w->holy_shield_absorb(spell);
    if (c.result)
      check(got == *c.result, c.name + " result");
    if (c.cooldown)
      check(h.w->read(spell + 0x1c) == *c.cooldown, c.name + " cooldown");
    std::vector<std::vector<Raw>> particles;
    for (auto e : named(*h.w, "presentation:particle_spawn"))
      particles.push_back(span(*e, 0, 8));
    expect_rows(particles, c.particles, c.name + " particle");
    auto lights = named(*h.w, "LightEffect_ctor");
    std::vector<std::vector<Raw>> places, colors;
    for (auto e : lights) {
      places.push_back(span(*e, 1, 3));
      colors.push_back(span(*e, 4, 12));
    }
    expect_rows(places, c.places, c.name + " light place");
    expect_rows(colors, c.lights, c.name + " light");
    return;
  }
  if (c.name.rfind("spark-", 0) == 0) {
    Host h;
    h.points = c.name == "spark-silent1" ? 2 : 1;
    h.sample = {0x40800000, 0x40a00000, 0x40c00000};
    auto player = h.player_at({0x3f800000, 0x40000000, 0x40400000});
    h.w->write(C::SHARED_SEED, 4, 1);
    h.w->clear_history();
    Raw silent = c.name == "spark-silent1" ? 1 : c.name == "spark-silent2" ? 2 : 3;
    h.w->block_sparks(player, silent);
    if (c.seed)
      check(h.w->read(C::SHARED_SEED) == *c.seed, c.name + " seed");
    std::vector<std::vector<Raw>> particles;
    for (auto e : named(*h.w, "presentation:particle_spawn"))
      particles.push_back(span(*e, 0, 8));
    expect_rows(particles, c.particles, c.name + " particle");
    return;
  }
  throw Mismatch("unknown case " + c.name);
}
void vectors(const char *path) {
  auto cases = load_vectors(path);
  check(cases.size() == 9, "vector file has the nine round-4 cases");
  for (auto &c : cases)
    replay(c);
  std::cout << checks << " exact round-4 vector checks passed\n";
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
