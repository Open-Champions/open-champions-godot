#include "champions/combat_runtime.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
// Round-6 host checks and the exact vector replay (research faa7033).
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
constexpr Raw GAS_RUN = 0x1fd0f0, CORPSE_RUN = 0x2ce4a4, SMOKE_RUN = 0x31c898,
              PUSTULE_RUN = 0x30bde0, VOLLEY_RUN = 0x2da2bc;
constexpr C::Address G_CULL = 0x4039c1, G_FOCUS = 0x44f048, G_LEVEL_FLAGS = 0x44fe08,
                     G_ARENA_COUNTER = 0x5038c8, HO_VT = 0x51ae58, PS_VT = 0x51ae28;
struct Sample {
  Raw index, facing;
  Vec3 pos;
};
struct Host {
  C::Services services;
  std::unique_ptr<C::World> w;
  Raw ground = 0, samples = 0, entry = 0, group = 0;
  Vec3 sample{};
  std::vector<Sample> sampled;
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
    services.height_at_point = [this](const Vec3 &) { return ground; };
    services.water_test = [](const Vec3 &, Raw) { return Raw(0); };
    services.water_disturb = [](const Vec3 &, Raw, Raw) { return Raw(0); };
    services.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
    services.animation_advance = [](Raw, Raw) { return Raw(0); };
    services.animation_root = [](C::Address) { return std::array<Raw, 2>{0, 0}; };
    services.skill_stat = [](const std::string &, Raw, Raw) { return Raw(0); };
    services.camera_yaw = [](Raw) { return Raw(0); };
    services.camera_flag = [](Raw) { return Raw(0); };
    services.attach_point = [](C::Address, Raw, const Vec3 &pos, Raw, Raw) { return pos; };
    services.sample_point_count = [this](Raw) { return samples; };
    services.sample_point = [this](Raw, C::Address, const Vec3 &pos, Raw facing, Raw index,
                                   Raw) {
      sampled.push_back({index, facing, pos});
      return sample;
    };
    services.resource = [this](const C::ResourceQuery &) { return entry; };
    services.particle_group = [this] { return group; };
    w = std::make_unique<C::World>(services, 1, C::Composition::Full);
  }
};
int count_event(const C::World &w, const std::string &name) {
  int n = 0;
  for (const auto &e : w.events())
    n += e.name == name;
  return n;
}
std::string refusal(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const C::Unsupported &e) {
    return e.what();
  }
  return "";
}
std::vector<Raw> span(const C::Event &e, unsigned from, unsigned count) {
  std::vector<Raw> out;
  for (unsigned i = 0; i < count; ++i)
    out.push_back(Raw(e.args.at(from + i)));
  return out;
}

// ---- host checks ---------------------------------------------------------------
// The object runs are reached through slot 4 of a table that holds them; slot 1
// of the fixture table is empty, so the end refuses after the call is recorded.
void dispatch_and_refusals() {
  Host h;
  h.w->write(C::FRAME, 4, 7);
  auto gas = h.w->spawn_effect_fixture(GAS_RUN, 0x80, {0, 0, 0});
  h.w->write(gas + 0x5c, 4, 100);
  h.w->write(gas + 0x60, 4, 20);
  h.w->write(gas + 0x64, 4, from_int(4));
  h.w->clear_history();
  h.w->run_actor(gas);
  check(count_event(*h.w, "presentation:particle_spawn") == 2, "a gas cloud runs through slot 4");
  check(h.w->read(gas + 0x64) == 0x40900000u, "the gas radius grows by one half");
  h.w->write(C::FRAME, 4, 100);
  h.w->clear_history();
  auto why = refusal([&] { h.w->run_actor(gas); });
  check(why.find("poisonGasSpell") != std::string::npos &&
            why.find("not in the research") != std::string::npos,
        "the gas end refuses at its unmodelled destructor: " + why);
  check(count_event(*h.w, "delete_self") == 1, "the slot-1 call is recorded before the refusal");

  // The round-5 run now reaches the round-6 end: reason 0 at the age limit.
  Host v;
  auto bolt = v.w->spawn_effect_fixture(VOLLEY_RUN, 0x80, {0, 0, 0});
  v.w->write(bolt + 0x6c, 4, 600);
  v.w->write(bolt + 0x74, 4, 0xab);
  v.w->clear_history();
  v.w->run_actor(bolt);
  check(count_event(*v.w, "MultiFireProjectile_end") == 1, "the run records the end reason");
  check(count_event(*v.w, "presentation:trail_release") == 1, "the end releases the trail");
  check(count_event(*v.w, "obj_delete") == 1, "the end deletes the bolt");
  check(v.w->read(bolt + 0x60) == 1, "the end stores 1 at +0x60");
  v.w->clear_history();
  v.w->run_actor(bolt);
  check(v.w->events().size() == 1, "an ended bolt's run returns at once");

  // A spell end without a modelled destructor refuses.
  Host s;
  C::Spawn sp;
  sp.hp = 0x42f00000;
  auto creature = s.w->spawn_class("Skelight", sp);
  s.w->write(C::FRAME, 4, 50);
  auto horn = s.w->hornomen_ctor(1, creature, 9);
  check(s.w->read(horn + 0x14) == HO_VT, "the horn ctor stores table 0x51ae58");
  check(s.w->read(horn + 0x18) == 650, "the horn ends 600 frames later");
  s.w->write(C::FRAME, 4, 650);
  why = refusal([&] { s.w->hornomen_run(horn, {0, 0, 0}); });
  check(why.find("SpellHornOmenEffect") != std::string::npos, "the horn end refuses: " + why);
  auto planar = s.w->planarsteel_ctor(1, creature, 9);
  check(s.w->read(planar + 0x14) == PS_VT, "the planar ctor stores table 0x51ae28");
  s.w->write(C::FRAME, 4, 650 + 240);
  why = refusal([&] { s.w->planarsteel_run(planar, {0, 0, 0}); });
  check(why.find("SpellPlanarSteelEffect") != std::string::npos, "the planar end refuses: " + why);

  // The two spell tables are stored, not filled.
  check(s.w->read(HO_VT + 12) == 0 && s.w->read(PS_VT + 12) == 0,
        "no spell table word is planted");
}

// ---- vectors -------------------------------------------------------------------
Raw hexword(const std::string &s) { return Raw(std::stoul(s, nullptr, 16)); }
std::vector<Raw> words_of(std::istringstream &ss) {
  std::vector<Raw> out;
  std::string w;
  while (ss >> w)
    out.push_back(hexword(w));
  return out;
}
struct Row {
  std::string a, b;
  std::vector<Raw> v;
};
struct Exact {
  std::string name, entry;
  std::vector<Raw> env, owner, skill, rec, creature, cw0, ctor, box;
  std::vector<std::vector<Raw>> obj, victims, particles, clouds, sounds, soundobjs, fx, sprintf_,
      trails, glows, samples, ow, cw, near;
  std::vector<std::pair<std::string, Raw>> enemies;
  std::vector<std::string> order, calls;
  std::vector<Row> damage;
  bool refuse = false, has_seed = false, has_deleted = false, has_nearview = false;
  Raw seed = 0, deleted = 0, reason = 0, nearview = 0;
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
    } else if (tag == "ENTRY")
      ss >> cur.entry;
    else if (tag == "ENV")
      cur.env = words_of(ss);
    else if (tag == "OBJ")
      cur.obj.push_back(words_of(ss));
    else if (tag == "OWNER")
      cur.owner = words_of(ss);
    else if (tag == "VICTIM")
      cur.victims.push_back(words_of(ss));
    else if (tag == "ENEMY") {
      std::string who, value;
      ss >> who >> value;
      cur.enemies.push_back({who, hexword(value)});
    } else if (tag == "ORDER") {
      std::string t;
      while (ss >> t)
        cur.order.push_back(t);
    } else if (tag == "SKILL")
      cur.skill = words_of(ss);
    else if (tag == "REC")
      cur.rec = words_of(ss);
    else if (tag == "CREATURE")
      cur.creature = words_of(ss);
    else if (tag == "CW0")
      cur.cw0 = words_of(ss);
    else if (tag == "CTOR")
      cur.ctor = words_of(ss);
    else if (tag == "REASON")
      cur.reason = words_of(ss).at(0);
    else if (tag == "P")
      cur.particles.push_back(words_of(ss));
    else if (tag == "DMG") {
      Row r;
      ss >> r.a >> r.b;
      r.v = words_of(ss);
      cur.damage.push_back(r);
    } else if (tag == "BOX")
      cur.box = words_of(ss);
    else if (tag == "CLOUD")
      cur.clouds.push_back(words_of(ss));
    else if (tag == "SOUND")
      cur.sounds.push_back(words_of(ss));
    else if (tag == "SOUNDOBJ")
      cur.soundobjs.push_back(words_of(ss));
    else if (tag == "FX")
      cur.fx.push_back(words_of(ss));
    else if (tag == "SPRINTF")
      cur.sprintf_.push_back(words_of(ss));
    else if (tag == "TRAIL")
      cur.trails.push_back(words_of(ss));
    else if (tag == "GLOW")
      cur.glows.push_back(words_of(ss));
    else if (tag == "SAMPLE")
      cur.samples.push_back(words_of(ss));
    else if (tag == "NEAR")
      cur.near.push_back(words_of(ss));
    else if (tag == "NEARVIEW") {
      cur.nearview = words_of(ss).at(0);
      cur.has_nearview = true;
    } else if (tag == "CALL") {
      std::string n;
      ss >> n;
      cur.calls.push_back(n);
    } else if (tag == "REFUSE")
      cur.refuse = true;
    else if (tag == "OW")
      cur.ow.push_back(words_of(ss));
    else if (tag == "CW")
      cur.cw.push_back(words_of(ss));
    else if (tag == "SEED") {
      cur.seed = words_of(ss).at(0);
      cur.has_seed = true;
    } else if (tag == "DELETED") {
      cur.deleted = words_of(ss).at(0);
      cur.has_deleted = true;
    } else
      throw Mismatch("unknown vector tag " + tag);
  }
  flush();
  return out;
}
void expect_rows(const std::vector<std::vector<Raw>> &got,
                 const std::vector<std::vector<Raw>> &want, const std::string &what) {
  check(got.size() == want.size(), what + " count " + std::to_string(got.size()) + " want " +
                                       std::to_string(want.size()));
  for (std::size_t i = 0; i < got.size() && i < want.size(); ++i) {
    check(got[i].size() >= want[i].size(), what + " row width");
    for (std::size_t k = 0; k < want[i].size(); ++k)
      check(got[i][k] == want[i][k], what + " " + std::to_string(i) + " word " +
                                         std::to_string(k));
  }
}
Vec3 vec_of(const std::vector<Raw> &v, std::size_t at) { return {v.at(at), v.at(at + 1), v.at(at + 2)}; }
const char *TAG_NAMES[] = {"obj", "owner", "v1", "v2", "v3", "v4"};

void replay(const Exact &c) {
  Host h;
  auto &w = *h.w;
  const auto &e = c.env;
  // ENV: frame cull focusx focusy level ground samples sample(3) entry group
  //      counter seed pos(3)
  h.ground = e.at(5);
  h.samples = e.at(6);
  h.sample = vec_of(e, 7);
  h.entry = e.at(10);
  h.group = e.at(11);
  auto pos = vec_of(e, 14);
  std::map<C::Address, std::string> tags;
  std::map<std::string, C::Address> by_tag;
  auto name = [&](C::Address a) {
    auto t = tags.find(a);
    return t == tags.end() ? std::string("?") : t->second;
  };
  auto remember = [&](const std::string &t, C::Address a) {
    tags[a] = t;
    by_tag[t] = a;
  };
  int players = 0;
  auto spawn = [&](Raw kind, const Vec3 &at) {
    C::Spawn s;
    s.position = at;
    s.hp = 0x42f00000;
    if (kind == 1)
      return w.spawn_player(unsigned(players++), s);
    return w.spawn_class("Skelight", s);
  };
  C::Address owner = 0;
  if (!c.owner.empty()) {
    owner = spawn(c.owner.at(0), vec_of(c.owner, 1));
    if (c.owner.at(0) == 2)
      w.write(owner + 0x310, 1, c.owner.at(4));
    remember("owner", owner);
  }
  w.write(C::FRAME, 4, e.at(0));
  w.write(G_CULL, 1, e.at(1));
  w.write(G_FOCUS, 4, e.at(2));
  w.write(G_FOCUS + 4, 4, e.at(3));
  w.write(G_LEVEL_FLAGS, 4, e.at(4));
  w.write(G_ARENA_COUNTER, 2, e.at(12));

  const bool spell = c.entry == "arena" || c.entry == "horn" || c.entry == "planar";
  const bool ctor = c.entry == "hornctor" || c.entry == "planarctor";
  C::Address obj = 0, creature = 0;
  if (spell || ctor) {
    C::Spawn s;
    if (!c.creature.empty())
      s.position = vec_of(c.creature, 0);
    s.hp = 0x42f00000;
    creature = w.spawn_class("Skelight", s);
    if (!c.creature.empty())
      w.write(creature + 0x38, 2, c.creature.at(3));
    if (!c.cw0.empty())
      w.write(creature + c.cw0.at(0), c.cw0.at(1), c.cw0.at(2));
    remember("creature", creature);
  }
  if (spell) {
    obj = w.spell_block();
    w.write(obj + 0xc, 4, creature);
    w.write(obj + 0x14, 4, c.entry == "horn" ? HO_VT : c.entry == "planar" ? PS_VT : 0);
  } else if (!ctor) {
    static const std::map<std::string, std::pair<Raw, Raw>> runs = {
        {"gas", {GAS_RUN, 0x80}},       {"corpse", {CORPSE_RUN, 0x80}},
        {"hit", {VOLLEY_RUN, 0x80}},    {"end", {VOLLEY_RUN, 0x80}},
        {"smoke", {SMOKE_RUN, 0x4a0}},  {"pustule", {PUSTULE_RUN, 0x180}}};
    auto r = runs.at(c.entry);
    bool linked = owner && c.entry != "smoke";
    obj = w.spawn_effect_fixture(r.first, r.second, pos, linked ? owner : 0);
  }
  if (obj) {
    remember("obj", obj);
    for (const auto &o : c.obj)
      w.write(obj + o.at(0), o.at(1), o.at(2));
  }
  for (const auto &v : c.victims) {
    auto at = vec_of(v, 2);
    for (unsigned k = 0; k < 3; ++k)
      at[k] = add(pos[k], at[k]);
    auto a = spawn(v.at(1), at);
    w.write(a + 0xc, 4, w.read(a + 0xc) | v.at(5));
    w.write(a + 0xa8, 4, w.read(a + 0xa8) | v.at(6));
    if (v.at(1) == 2)
      w.write(a + 0x310, 1, v.at(7));
    remember(TAG_NAMES[v.at(0)], a);
  }
  for (const auto &[who, value] : c.enemies)
    check(Raw(w.enemy(owner, by_tag.at(who))) == value,
          c.name + " fixture enemy relation for " + who);
  if (!c.skill.empty()) {
    C::SkillSetup setup;
    setup.stats[0] = {1, std::int32_t(c.skill.at(0)), 0};
    setup.stats[2] = {1, std::int32_t(c.skill.at(1)), 0};
    w.write(obj + 0x70, 4, w.give_skill(16, setup));
  }
  if (!c.rec.empty()) {
    w.write(C::PLAYERS + 0xd0, 4, c.rec.at(0));
    w.write(C::PLAYERS + 0xd4, 4, c.rec.at(1));
  }
  if (!c.order.empty()) {
    std::vector<std::string> got;
    for (auto a : w.collect_box(vec_of(c.box, 0), vec_of(c.box, 3), int(c.box.at(6))))
      got.push_back(name(a));
    std::string seen;
    for (const auto &g : got)
      seen += " " + g;
    check(got == c.order, c.name + " fixture box order:" + seen);
  }
  if (ctor)
    w.write(C::SHARED_SEED, 4, e.at(13));
  w.clear_history();
  h.sampled.clear();

  std::string why;
  try {
    if (c.entry == "gas")
      w.poison_gas_run(obj);
    else if (c.entry == "corpse")
      w.disease_corpse_run(obj);
    else if (c.entry == "hit")
      w.multifire_on_hit(obj, by_tag.at("v1"));
    else if (c.entry == "end")
      w.multifire_end(obj, c.reason);
    else if (c.entry == "smoke")
      w.smoke_gen_run(obj);
    else if (c.entry == "pustule")
      w.pustule_run(obj);
    else if (c.entry == "arena")
      w.arena_run(obj, {from_int(7), from_int(8), from_int(9)});
    else if (c.entry == "horn")
      w.hornomen_run(obj, {from_int(1), from_int(2), from_int(3)});
    else if (c.entry == "planar")
      w.planarsteel_run(obj, pos);
    else if (c.entry == "hornctor") {
      obj = w.hornomen_ctor(c.ctor.at(0), creature, c.ctor.at(1));
      remember("obj", obj);
    } else if (c.entry == "planarctor") {
      obj = w.planarsteel_ctor(c.ctor.at(0), creature, c.ctor.at(1));
      remember("obj", obj);
    } else
      throw Mismatch("unknown entry " + c.entry);
  } catch (const C::Unsupported &x) {
    why = x.what();
  }
  check(c.refuse == !why.empty(), c.name + " refusal: " + why);

  // Damage effects inside damage_apply are the round-2 bodies; only what the
  // round-6 method itself does is compared, so rows are cut at the first
  // damage_apply and sound/effect rows of a hit start at its end body.
  std::vector<std::vector<Raw>> particles, clouds, sounds, soundobjs, fx, sprintf_, trails, glows,
      near, box;
  std::vector<Row> damage;
  bool damaged = false, ending = c.entry != "hit";
  int deleted = 0, steps = 0;
  for (const auto &ev : w.events()) {
    if (ev.name == "round6_damage") {
      damaged = true;
      Row r{name(Raw(ev.args.at(0))), name(Raw(ev.args.at(1))), span(ev, 2, 8)};
      damage.push_back(r);
    } else if (ev.name == "presentation:trail_release") {
      ending = true;
      trails.push_back(span(ev, 0, 1));
    } else if (ev.name == "presentation:particle_spawn" && !damaged)
      particles.push_back(span(ev, 0, 8));
    else if (ev.name == "world_collectObjectsInBox" && box.empty())
      box.push_back(span(ev, 0, 7));
    else if (ev.name == "DiseaseCloud_ctor") {
      auto row = span(ev, 1, 5);
      row[4] = row[4] ? 1 : 0;
      clouds.push_back(row);
    } else if (ev.name == "presentation:sound_playAtPos" && !damaged)
      sounds.push_back(span(ev, 0, 5));
    else if (ev.name == "presentation:sound_playOnObject" && ending)
      soundobjs.push_back({Raw(ev.args.at(0)), Raw(ev.args.at(2))});
    else if (ev.name == "presentation:fx_spawn" && ending)
      fx.push_back(span(ev, 0, 4));
    else if (ev.name == "libc_sprintf")
      sprintf_.push_back(span(ev, 0, 2));
    else if (ev.name == "presentation:glow_add")
      glows.push_back(span(ev, 0, 12));
    else if (ev.name == "isNearView")
      near.push_back(span(ev, 1, 2));
    else if (ev.name == "obj_delete" && Raw(ev.args.at(0)) == obj)
      ++deleted;
    else if (ev.name == "SmokeGen_stepParticles")
      ++steps;
  }
  expect_rows(particles, c.particles, c.name + " particle");
  expect_rows(clouds, c.clouds, c.name + " cloud");
  expect_rows(sounds, c.sounds, c.name + " sound");
  expect_rows(soundobjs, c.soundobjs, c.name + " object sound");
  expect_rows(fx, c.fx, c.name + " effect");
  expect_rows(sprintf_, c.sprintf_, c.name + " printed name");
  expect_rows(trails, c.trails, c.name + " trail");
  expect_rows(glows, c.glows, c.name + " glow");
  if (c.box.empty())
    check(box.empty(), c.name + " collects no box");
  else
    expect_rows(box, {c.box}, c.name + " box");
  auto by_victim = [](const Row &a, const Row &b) { return a.a < b.a; };
  auto want_damage = c.damage;
  std::stable_sort(damage.begin(), damage.end(), by_victim);
  std::stable_sort(want_damage.begin(), want_damage.end(), by_victim);
  if (damage.size() != want_damage.size())
    for (const auto &ev : w.events()) {
      std::cerr << ev.name;
      for (auto a : ev.args)
        std::cerr << ' ' << std::hex << Raw(a) << std::dec;
      std::cerr << '\n';
    }
  check(damage.size() == want_damage.size(), c.name + " damage count " + std::to_string(damage.size()));
  for (std::size_t i = 0; i < damage.size() && i < want_damage.size(); ++i) {
    check(damage[i].a == want_damage[i].a && damage[i].b == want_damage[i].b,
          c.name + " damage victim " + damage[i].a + " by " + damage[i].b);
    check(damage[i].v == want_damage[i].v, c.name + " damage words " + damage[i].a);
  }
  std::vector<std::vector<Raw>> samples;
  for (const auto &s : h.sampled)
    samples.push_back({s.index, s.facing, s.pos[0], s.pos[1], s.pos[2]});
  expect_rows(samples, c.samples, c.name + " sample point");
  if (c.has_nearview) {
    check(near.size() == 1, c.name + " one near-view test");
    check(near.at(0).at(0) == c.near.at(0).at(0), c.name + " near-view radius");
    check(near.at(0).at(1) == c.nearview, c.name + " near-view answer");
  }
  check(steps == int(c.calls.size()), c.name + " particle steps");
  if (c.has_deleted)
    check(Raw(deleted) == c.deleted, c.name + " obj_delete");
  for (const auto &o : c.ow)
    check(w.read(obj + o.at(0), o.at(1)) == o.at(2),
          c.name + " word +" + std::to_string(o.at(0)));
  for (const auto &o : c.cw)
    check(w.read(creature + o.at(0), o.at(1)) == o.at(2),
          c.name + " creature word +" + std::to_string(o.at(0)));
  if (c.has_seed)
    check(w.read(C::SHARED_SEED) == c.seed, c.name + " shared seed");
}
void vectors(const char *path) {
  auto cases = load_vectors(path);
  check(cases.size() == 46, "vector file has the forty-six round-6 cases");
  for (const auto &c : cases) {
    try {
      replay(c);
    } catch (const Mismatch &) {
      throw;
    } catch (const std::exception &x) {
      throw Mismatch(c.name + ": " + x.what());
    }
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    set_fpu();
    if (argc == 2)
      vectors(argv[1]);
    else
      dispatch_and_refusals();
    std::cout << checks << " round-6 checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
