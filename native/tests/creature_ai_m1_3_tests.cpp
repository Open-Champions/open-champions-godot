// Synthetic tests for creature_ai_m1_3. Every expectation cites
// t3/research-campaign-combat fbb2e1d: "C:<n>" docs/creature-ai-m1-3.md,
// "M2:<n>" docs/monsters-2.md, "MOD:<n>" tools/models_creature_ai_m1_3.py,
// "DT:<n>" tools/difftest_creature_ai_m1_3.py. argv[1] is the vector file
// written by tools/generate_creature_ai_m1_3_vectors.py (research models on
// the research's synthetic grid; no game data).
#include "champions/creature_ai_m1_3.hpp"
#include "champions/core_state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace champions::simulation::rta;
namespace A = creature_ai_m1_3;
using A::Class;

namespace {
int checks = 0;
void check(bool ok, const std::string &label) {
  ++checks;
  if (!ok) throw std::runtime_error(label);
}
template <class F> bool refuses(F fn) {
  try { fn(); } catch (const A::Unsupported &) { return true; }
  return false;
}
Raw f32(float v) { Raw r; std::memcpy(&r, &v, 4); return r; }
// Event arguments are value x ARG_SCALE (C:198-246 compares floats to four decimals).
std::int64_t U(double v) { return std::llround(v * double(A::ARG_SCALE)); }
A::Event E(const std::string &name, const std::vector<double> &args) {
  A::Event e{name, {}};
  for (auto v : args) e.args.push_back(U(v));
  return e;
}
double arg(const A::Event &e, std::size_t i) { return double(e.args.at(i)) / double(A::ARG_SCALE); }
A::Answer answers(std::vector<std::int64_t> list) {
  auto at = std::make_shared<std::size_t>(0);
  return [list = std::move(list), at](const A::Event &) -> std::int64_t {
    const auto i = (*at)++;
    return i < list.size() ? list[i] : 0;
  };
}
// Answers by query name (first match per call); unnamed queries read 0.
A::Answer by_name(std::map<std::string, std::vector<std::int64_t>> m) {
  auto seen = std::make_shared<std::map<std::string, std::size_t>>();
  return [m = std::move(m), seen](const A::Event &e) -> std::int64_t {
    const auto it = m.find(e.name);
    if (it == m.end()) return 0;
    const auto i = (*seen)[e.name]++;
    return i < it->second.size() ? it->second[i] : it->second.back();
  };
}
// see(near, far, arc) answered by its (near, far, arc) key.
A::Answer see_map(std::map<std::tuple<double, double, double>, std::int64_t> s,
                  std::map<std::string, std::int64_t> other = {}) {
  return [s = std::move(s), other = std::move(other)](const A::Event &e) -> std::int64_t {
    if (e.name == "see") {
      const auto it = s.find({arg(e, 0), arg(e, 1), arg(e, 3)});
      return it == s.end() ? 0 : it->second;
    }
    const auto it = other.find(e.name);
    return it == other.end() ? 0 : it->second;
  };
}
A::Context base(std::uint32_t state, bool target = true) {
  A::Context c;
  c.state = state;
  c.target = target;
  c.dmg_low = 11; c.dmg_high = 22; c.rating = 33;  // REF:73 DMG
  // The inputs the probe held fixed, stated explicitly (CAI13-1, CAI13-2):
  // a dry Froglock trident, Clockwork object flags clear.
  c.holder_hand1 = 0x12;
  c.water = A::FroglockWater{};
  c.object_flags = 0;
  return c;
}
std::vector<std::uint32_t> req(const A::Tick &t) { return t.requests(); }
unsigned count(const A::Tick &t, const std::string &name) {
  unsigned n = 0;
  for (const auto &e : t.events) n += e.name == name;
  return n;
}
// A seed whose first `n` own draws satisfy pred (DT:289-296 lucky seeds).
std::uint32_t seed_where(const std::function<bool(const std::vector<std::uint32_t> &)> &pred, unsigned n) {
  for (std::uint32_t s = 1;; ++s) {
    Lcg g(s);
    std::vector<std::uint32_t> d;
    for (unsigned i = 0; i < n; ++i) d.push_back(g.draw());
    if (pred(d)) return s;
  }
}
const std::map<std::string, Class> NAMES = {
  {"Kobold", Class::Kobold}, {"WarWraith", Class::WarWraith}, {"Orc", Class::Orc},
  {"Goblin", Class::Goblin}, {"Ogre", Class::Ogre}, {"Warrior", Class::Warrior},
  {"Clockwork", Class::Clockwork}, {"Droid", Class::Droid}, {"IronGuardian", Class::IronGuardian},
  {"Froglock", Class::Froglock}, {"Amphian", Class::Amphian}, {"SeaMonster", Class::SeaMonster}};
bool walked(Class c, std::uint32_t s) {
  // MOD:153 Kobold ALL excludes 9 (and stops before 0x18); MOD:1114 Goblin
  // and MOD:1141 Froglock exclude 7 and 8; others walk 0..0x18 (MOD:1090).
  if (s > 0x18) return false;
  if (c == Class::Kobold) return s != 9 && s < 0x18;
  if (c == Class::Goblin || c == Class::Froglock) return s != 7 && s != 8;
  return true;
}
std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream in(s);
  while (std::getline(in, cur, d)) out.push_back(cur);
  if (!s.empty() && s.back() == d) out.push_back("");
  return out;
}

// --- Equivalence with the research models on their synthetic grid (C:198-246).
void vectors(const char *path) {
  std::ifstream in(path);
  check(bool(in), "vector file opens");
  std::string line;
  unsigned rows = 0;
  std::map<std::string, unsigned> per;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto f = split(line, '\t');
    check(f.size() == 19, "vector row has 19 fields");
    const auto cls = NAMES.at(f[0]);
    A::Context c;
    c.state = std::stoul(f[1]);
    c.target = f[2] == "1";
    c.ev = std::stoul(f[3]);
    c.anim = std::stoul(f[4]);
    c.clock = std::stoll(f[5]);
    c.deadline = std::stoll(f[6]);
    if (f[7] != "-")
      for (const auto &kv : split(f[7], ',')) {
        const auto eq = kv.find('=');
        c.fields[std::stoul(kv.substr(0, eq))] = std::stoll(kv.substr(eq + 1));
      }
    c.hp = std::stoul(f[8], nullptr, 16);
    c.max_hp = std::stoul(f[9], nullptr, 16);
    c.alive_players = std::stoul(f[10]);
    c.ally = f[11] == "1";
    c.holder_hand1 = std::stoul(f[12]);
    c.base_run_ends = f[13] != "0";
    c.active = f[14] != "0";
    c.seed = std::stoul(f[15], nullptr, 16);
    c.dmg_low = 11; c.dmg_high = 22; c.rating = 33;
    c.water = A::FroglockWater{};  // MOD:1141-1142: the walk was dry
    c.object_flags = 0;            // and started with object flags clear
    std::vector<std::int64_t> used;
    if (f[16] != "-") for (const auto &a : split(f[16], ',')) used.push_back(std::stoll(a));
    std::vector<A::Event> want;
    if (f[17] != "-")
      for (const auto &e : split(f[17], ';')) {
        const auto bar = e.find('|');
        A::Event ev{e.substr(0, bar), {}};
        const auto rest = e.substr(bar + 1);
        if (!rest.empty()) for (const auto &a : split(rest, ',')) ev.args.push_back(U(std::strtod(a.c_str(), nullptr)));
        want.push_back(ev);
      }
    const auto got = A::tick(cls, c, answers(used));
    if (!(got.events == want) || got.seed != std::stoul(f[18], nullptr, 16)) {
      std::cerr << "vector mismatch: " << line << "\n got:";
      for (const auto &e : got.events) {
        std::cerr << ' ' << e.name;
        for (auto a : e.args) std::cerr << ',' << double(a) / double(A::ARG_SCALE);
      }
      std::cerr << " seed " << std::hex << got.seed << std::dec << "\n";
      check(false, "decision vector matches research model");
    }
    ++checks;
    ++rows;
    ++per[f[0]];
  }
  check(rows == 24206, "all 24,206 vector paths compared");
  check(per.size() == 12, "vectors cover all twelve classes");
}

// --- C:66-73: both tick exits, every walked state of every class.
void exits() {
  for (const auto &n : NAMES)
    for (std::uint32_t s = 0; s <= 0x18; ++s) {
      if (!walked(n.second, s)) continue;
      auto c = base(s);
      c.base_run_ends = true;
      auto t = A::tick(n.second, c, answers({1, 1, 1}));
      check(t.events.size() == 2 && t.events[1].name == "baseRun", n.first + " Creature_run exit");
      c.base_run_ends = false;
      c.active = false;
      t = A::tick(n.second, c, answers({1, 1, 1}));
      check(t.events.size() == 3 && t.events[2].name == "updateActive", n.first + " updateActive exit");
    }
}

// --- C:73-82 head order and knock-back scales; C:86-95 slot picks.
void head_and_slots() {
  const std::map<Class, float> knock = {{Class::Kobold, 1.0f}, {Class::WarWraith, 0.0f},
      {Class::Orc, 0.4f}, {Class::Goblin, 0.5f}, {Class::Ogre, 0.1f}, {Class::Warrior, 0.4f},
      {Class::Clockwork, 0.4f}, {Class::Droid, 1.0f}, {Class::IronGuardian, 0.0f},
      {Class::Froglock, 0.5f}, {Class::Amphian, 0.5f}, {Class::SeaMonster, 0.2f}};
  for (const auto &n : NAMES) {
    const auto &i = A::info(n.second);
    check(i.knockback == U(knock.at(n.second)), n.first + " knock-back scale (C:77-80)");
    check(i.slot23 == 0x10ded0 && i.slot24 == 0x10e2c0 && i.slot25 == 0x10e658,
          n.first + " base pick/ally/sight slots (C:89-90)");
    check(i.slot2 == (n.second == Class::Orc ? 0x2a2eac : 0x116638), n.first + " slot 2 (C:91)");
    check(i.slot21 == (n.second == Class::Kobold ? 0x280434 : 0x119400), n.first + " slot 21 (C:92)");
    check(i.slot20 == (n.second == Class::WarWraith ? 0x2c1bc0 : 0x119328), n.first + " slot 20 (C:93)");
    check(i.home_range == (n.second == Class::WarWraith ? 600u : n.second == Class::IronGuardian ? 3000u : 1000u),
          n.first + " HomeRange (C:103-106, M2:6202, M2:1355)");
    check(A::class_of_constructor(i.constructor) == n.second, n.first + " constructor (C:41-56)");
    const auto t = A::tick(n.second, base(0x10), answers({1}));
    if (n.second == Class::WarWraith) {
      check(t.events[3].name == "stepAnim" && t.events[4].name == "pickTarget" &&
            t.events[5].name == "knockback", "WarWraith picks before knock-back (C:80-81)");
    } else {
      check(t.events[3].name == "stepAnim" && t.events[4].name == "knockback" &&
            t.events[4].args.size() == 1 && t.events[4].args[0] == U(knock.at(n.second)) &&
            t.events[5].name == "pickTarget" && t.events[5].args == std::vector<std::int64_t>{0},
            n.first + " stepAnim, knock-back, pick order (C:73-77)");
    }
    const bool ally = n.second == Class::Kobold;
    check((t.events[6].name == "pickAlly") == ally, n.first + " slot 24 every tick only for Kobold (C:81-82)");
  }
  auto shaman = base(0x10);
  shaman.fields[0x3d4] = 5;
  check(A::tick(Class::Goblin, shaman, answers({1})).has("pickAlly"), "goblin shaman picks an ally (C:81-82)");
  auto dead = base(0x14);
  check(!A::tick(Class::WarWraith, dead).has("pickTarget"), "dead WarWraith skips the pick (C:80-81)");
}

// --- Thresholds: ally counts, box counts, deadlines, clock-compared fields.
void thresholds() {
  // C:258: tough Kobold alliesNearTarget(24) > 2: 2 stays, 3 goes.
  auto tough = base(3); tough.fields[0x3cc] = 1;
  check(req(A::tick(Class::Kobold, tough, answers({2}))).empty(), "tough Kobold 2 allies stays");
  check(req(A::tick(Class::Kobold, tough, answers({3}))) == std::vector<std::uint32_t>{5}, "tough Kobold 3 allies goes");
  // C:309-310: Orc soldier/leaders > 0 (1 goes), recruit > 1 (1 stays, 2 goes).
  for (std::int64_t v : {1, 2, 3}) {
    auto o = base(3); o.fields[0x478] = v;
    check(req(A::tick(Class::Orc, o, answers({0, 0, 1}))) == std::vector<std::uint32_t>{0x17}, "Orc soldier/leader 1 ally -> 0x17");
    check(req(A::tick(Class::Orc, o, answers({0, 0, 0}))).empty(), "Orc soldier/leader 0 allies stays");
  }
  auto recruit = base(3); recruit.fields[0x478] = 0;
  recruit.seed = seed_where([](auto d) { return d[0] % 30 != 0; }, 1);
  check(req(A::tick(Class::Orc, recruit, answers({0, 0, 1}))).empty(), "Orc recruit 1 ally stays");
  check(req(A::tick(Class::Orc, recruit, answers({0, 0, 2}))) == std::vector<std::uint32_t>{0x17}, "Orc recruit 2 allies -> 0x17");
  // C:311-312 / C:350: own draw % 30 == 0 and box count < 3 (2 goes, 3 stays).
  recruit.seed = seed_where([](auto d) { return d[0] % 30 == 0; }, 1);
  check(req(A::tick(Class::Orc, recruit, answers({0, 0, 0, 2}))) == std::vector<std::uint32_t>{4}, "Orc box 2 -> rear approach");
  check(req(A::tick(Class::Orc, recruit, answers({0, 0, 0, 3}))).empty(), "Orc box 3 stays");
  auto gob = base(3); gob.fields[0x3d4] = 0;
  gob.seed = recruit.seed;
  check(req(A::tick(Class::Goblin, gob, answers({0, 0, 2}))) == std::vector<std::uint32_t>{4}, "Goblin box 2 -> 4");
  check(req(A::tick(Class::Goblin, gob, answers({0, 0, 3}))).empty(), "Goblin box 3 stays");
  // C:333-334: Orc recruit 0x17 allies < 2 (1 goes, 2 stays); deadline 101 keeps it.
  auto r17 = base(0x17); r17.fields[0x478] = 0; r17.deadline = 101;
  check(req(A::tick(Class::Orc, r17, answers({0, 1}))) == std::vector<std::uint32_t>{3}, "Orc recruit 0x17 1 ally -> 3");
  check(req(A::tick(Class::Orc, r17, answers({0, 2}))).empty(), "Orc recruit 0x17 2 allies stays");
  // C:379: Warrior allies >= 1 (0 stays, 1 goes) with pickTarget(1).
  const auto w0 = A::tick(Class::Warrior, base(3), see_map({{{0, 120, 8000}, 1}}, {{"alliesNearTarget", 0}}));
  const auto w1 = A::tick(Class::Warrior, base(3), see_map({{{0, 120, 8000}, 1}}, {{"alliesNearTarget", 1}}));
  check(req(w0).empty() && req(w1) == std::vector<std::uint32_t>{5} &&
        w1.events[w1.events.size() - 2] == E("pickTarget", {1}), "Warrior ally threshold and pickTarget(1)");
  // C:208 deadline +0xf4 against clock 100: strictly greater.
  for (std::int64_t dl : {99, 100, 101}) {
    auto k = base(0); k.deadline = dl;
    check(req(A::tick(Class::Kobold, k, answers({0, 0}))) == (dl < 100 ? std::vector<std::uint32_t>{1} : std::vector<std::uint32_t>{}),
          "Kobold idle deadline " + std::to_string(dl));
  }
  // C:284-285 WarWraith +0x3cc: 99 -> drain 7; 100 and 101 -> 6.
  for (std::int64_t v : {99, 100, 101}) {
    auto ww = base(3); ww.fields[0x3cc] = v;
    check(req(A::tick(Class::WarWraith, ww, answers({1}))) == std::vector<std::uint32_t>{v == 99 ? 7u : 6u},
          "WarWraith drain clock " + std::to_string(v));
  }
  // C:329-330 Orc shaman +0x47c; C:354-355 goblin shaman +0x3d8.
  for (std::int64_t v : {99, 100, 101}) {
    auto sh = base(0x17); sh.fields[0x478] = 5; sh.fields[0x47c] = v;
    check((req(A::tick(Class::Orc, sh)) == std::vector<std::uint32_t>{3}) == (v == 99), "Orc shaman 0x17 clock " + std::to_string(v));
    auto gs = base(0x17); gs.fields[0x3d4] = 5; gs.fields[0x3d8] = v;
    check((req(A::tick(Class::Goblin, gs)) == std::vector<std::uint32_t>{3}) == (v == 99), "goblin shaman 0x17 clock " + std::to_string(v));
  }
  // C:416-417 Droid self-destruct +0x3cc < clock (99 goes; 100/101 do not).
  for (std::int64_t v : {99, 100, 101}) {
    auto d = base(0); d.fields[0x3cc] = v;
    const auto t = A::tick(Class::Droid, d);
    check((!req(t).empty() && req(t)[0] == 0x17) == (v == 99), "Droid fuse clock " + std::to_string(v));
  }
  auto fused = base(0x17); fused.fields[0x3cc] = 0;
  check(req(A::tick(Class::Droid, fused)).empty(), "Droid in 0x17 is not re-fused");
  // C:426-429 IronGuardian: counter +1 > 120 (119 stays, 120/121 go); +0x3cc < clock gates cone 240.
  for (std::int64_t v : {119, 120, 121}) {
    auto g = base(0); g.fields[0x3cc] = v;
    check((req(A::tick(Class::IronGuardian, g, answers({1}))) == std::vector<std::uint32_t>{3}) == (v >= 120),
          "IronGuardian counter " + std::to_string(v));
  }
  for (std::int64_t v : {99, 100, 101}) {
    auto g = base(3); g.fields[0x3cc] = v;
    const auto t = A::tick(Class::IronGuardian, g);
    bool asked = false;
    for (const auto &e : t.events) asked |= e.name == "cone" && e.args[0] == U(240);
    check(asked == (v == 99), "IronGuardian 10 clock " + std::to_string(v));
  }
}

// --- C:159-185 hitAtReach (DT:547-583 rows, attacker at the origin, facing 0).
void reach() {
  struct Row { float reach, x, y, z, own_y; std::uint32_t flags; bool hit; };
  const Row rows[] = {
    {36, 95.9f, 0, 0, 0, 0, true}, {36, 96.0f, 0, 0, 0, 0, false},
    {36, -23.9f, 0, 0, 0, 0, true}, {36, -24.0f, 0, 0, 0, 0, false},
    {36, 36, 0, 59.9f, 0, 0, true}, {36, 36, 0, 60.0f, 0, 0, false},
    {60, 119.9f, 0, 0, 0, 0, true}, {60, 120.0f, 0, 0, 0, 0, false},
    {36, 36, 0, 0, 0, 8, false}, {36, 36, 0, 0, 0, 0x40, true},
    {36, 36, 59.9f, 0, 0, 0, true}, {36, 36, 60.1f, 0, 0, 0, false},
    {36, 36, -59.9f, 0, 0, 0, true}, {36, 36, -60.1f, 0, 0, 0, false},
    {36, 78.0f, 42.0f, 0, 0, 0, true}, {36, 78.5f, 42.5f, 0, 0, 0, false},
    {36, 70.6f, 34.6f, 34.6f, 0, 0, true}, {36, 70.7f, 34.7f, 34.7f, 0, 0, false},
    {36, 36, 69.9f, 0, 10, 0, true}, {36, 36, 70.1f, 0, 10, 0, false},
    {36, 36, -49.9f, 0, 10, 0, true}, {36, 36, -50.1f, 0, 10, 0, false},
    {36, 36, 10.0f, 0, 10, 0, true},
    // C:171-172: exactly 60.0 along +0x14 hits, along -0x14 misses (Executed;
    // the cause is Inferred, C:176-179, and no offset is added here).
    {36, 36, 60.0f, 0, 0, 0, true}, {36, 36, -60.0f, 0, 0, 0, false},
  };
  for (const auto &r : rows) {
    A::ReachProbe p;
    p.attacker = {0, f32(r.own_y), 0};
    p.target = {f32(r.x), f32(r.y), f32(r.z)};
    p.reach = f32(r.reach);
    p.target_flags = r.flags;
    check(A::hit_at_reach(p) == r.hit, "hitAtReach row x=" + std::to_string(r.x) + " y=" + std::to_string(r.y) +
          " z=" + std::to_string(r.z));
  }
}

// --- C:459-465 Amphian block below half; C:135-146 no other HP decision.
void hit_points() {
  const auto block = seed_where([](auto d) { return (d[0] & 3) == 0; }, 1);
  const auto noblock = seed_where([](auto d) { return (d[0] & 3) != 0; }, 1);
  struct P { float hp, top; bool below; };
  for (const auto &p : {P{10, 100, true}, P{49, 100, true}, P{50, 100, false}, P{51, 100, false},
                        P{99, 200, true}, P{100, 200, false}, P{101, 200, false}}) {
    auto c = base(5);
    c.hp = f32(p.hp); c.max_hp = f32(p.top);
    c.seed = block;
    const auto t = A::tick(Class::Amphian, c, answers({1}));
    check((req(t) == std::vector<std::uint32_t>{0x17}) == p.below, "Amphian block at " + std::to_string(p.hp) + "/" + std::to_string(p.top));
    check(t.draws == 1u, "Amphian draw count");
    c.seed = noblock;
    const auto u = A::tick(Class::Amphian, c, answers({1}));
    check(u.draws == (p.below ? 2u : 1u), "the block draw is made only below half");
    check(req(u).size() == 1 && req(u)[0] >= 6 && req(u)[0] <= 8, "otherwise 6 + draw % 3");
  }
  // Same answers and seed at HP 10/49/50/51 of 100 and 100/100: only Amphian 5 differs.
  for (const auto &n : NAMES)
    for (std::uint32_t s = 0; s <= 0x18; ++s) {
      if (!walked(n.second, s)) continue;
      for (std::int64_t fill : {0, 1}) {
        auto c = base(s);
        c.ev = 1; c.anim = 8; c.seed = block;
        const auto full = A::tick(n.second, c, answers(std::vector<std::int64_t>(16, fill)));
        for (float hp : {10.f, 49.f, 50.f, 51.f}) {
          c.hp = f32(hp);
          const auto t = A::tick(n.second, c, answers(std::vector<std::int64_t>(16, fill)));
          const bool same = t.events == full.events && t.seed == full.seed;
          const bool amphian5 = n.second == Class::Amphian && s == 5 && fill && hp < 50;
          check(same != amphian5, n.first + " HP-independent decision state " + std::to_string(s));
        }
      }
    }
}

// --- C:279-298, M2:6128, M2:6277-6279: WarWraith signed fade.
void fade() {
  for (std::int64_t f : {98, 99, 100, 101, 127, 128, 150, 255}) {
    const std::int32_t s8 = f >= 128 ? std::int32_t(f) - 256 : std::int32_t(f);
    for (std::uint32_t state : {0u, 3u, 0x14u}) {
      auto c = base(state);
      c.fields[0x3c8] = f;
      c.fields[0x3cc] = 500;
      const auto t = A::tick(Class::WarWraith, c);
      std::size_t k = 0;
      while (k < t.events.size() && t.events[k].name != "knockback") ++k;
      const bool head_turn = k + 1 < t.events.size() && t.events[k + 1].name == "turn";
      check(head_turn == (s8 >= 99), "WarWraith head turn at fade " + std::to_string(f));
      if (state == 0x14) {
        check(t.has("objDelete") == (s8 >= 100), "WarWraith delete at fade " + std::to_string(f));
        check((t.draws >= 4) == (s8 < 100), "WarWraith particle path at fade " + std::to_string(f));
      }
    }
  }
}

// --- C:329-332, M2:5492-5494: Orc shaman 0x17 long-range sight -> extra turn.
void orc_shaman_sight() {
  auto c = base(0x17); c.fields[0x478] = 5; c.fields[0x47c] = 100;
  const auto t = A::tick(Class::Orc, c, see_map({{{0, 6000, 0x7fff}, 1}}));
  check(count(t, "turn") == 2 && req(t).empty(), "shaman far sight adds a turn");
  bool asked = false;
  for (const auto &e : t.events) asked |= e == E("see", {0, 6000, 0, 0x7fff, 0});
  check(asked, "shaman asks see(0, 6000, 0x7fff)");
  check(count(A::tick(Class::Orc, c), "turn") == 1, "no far sight: one turn");
  check(req(A::tick(Class::Orc, c, see_map({{{0, 30, 6000}, 1}}))) == std::vector<std::uint32_t>{7}, "shaman in reach -> 7");
}

// --- C:432-435, M2:1392: IronGuardian 7/8 turns only with exactly 1 player alive.
void guardian_turn() {
  for (std::uint32_t s : {7u, 8u})
    for (std::uint32_t alive : {0u, 1u, 2u, 3u, 4u}) {
      auto c = base(s); c.alive_players = alive; c.ev = 1; c.anim = 8;
      const auto t = A::tick(Class::IronGuardian, c);
      check(t.has("turn") == (alive == 1), "IronGuardian turn alive=" + std::to_string(alive));
      check(t.events.back() == E("hitAll", {0, 84, s == 7 ? 16384.0 : -16384.0, 0x4000, 11, 22, 33, 0x80008}),
            "IronGuardian side swing facing");
    }
}

// --- C:218-221 rare own-generator branches (first / second draw).
void rare() {
  const auto zero = [](unsigned n) { return seed_where([n](auto d) { for (auto x : d) if (x % 60) return false; return true; }, n); };
  const auto none = seed_where([](auto d) { return d[0] % 60 && d[0] % 30 && d[0] % 5 && d[0] % 3 && (d[0] & 3) && (d[0] & 1); }, 1);
  // Ogre 5 out of reach: outside 156 and draw % 60 == 0 -> 3 (C:367-368).
  auto og = base(5); og.seed = zero(1);
  check(req(A::tick(Class::Ogre, og, see_map({}, {{"notRooted", 1}}))) == std::vector<std::uint32_t>{3}, "Ogre 1-in-60 leave");
  og.seed = none;
  check(req(A::tick(Class::Ogre, og, see_map({}, {{"notRooted", 1}, {"alliesNearTarget", 1}}))).empty(), "Ogre stays");
  // Ogre 5 in reach: last not 8/9 and draw % 3 == 0 -> 8 (C:366).
  og.seed = zero(1);
  check(req(A::tick(Class::Ogre, og, see_map({{{0, 60, 2000}, 1}}))) == std::vector<std::uint32_t>{8}, "Ogre crush on draw % 3");
  og.fields[0x460] = 8;
  check(req(A::tick(Class::Ogre, og, see_map({{{0, 60, 2000}, 1}}))) == std::vector<std::uint32_t>{6}, "Ogre after 8 swings 6");
  // Warrior 5 not rooted: draw % 60 == 0 and not (near and allies) -> 3 (C:383-384).
  auto wr = base(5); wr.seed = zero(1); wr.deadline = 200;
  check(req(A::tick(Class::Warrior, wr, see_map({}, {{"notRooted", 1}}))) == std::vector<std::uint32_t>{3}, "Warrior 1-in-60 leave");
  // Orc 6 melee ev&1: draw % 5 == 0 -> 3 (C:320-321).
  auto oc = base(6); oc.fields[0x478] = 1; oc.ev = 1; oc.anim = 8; oc.deadline = 50; oc.seed = zero(1);
  check(req(A::tick(Class::Orc, oc)) == std::vector<std::uint32_t>{3}, "Orc 1-in-5 break-off");
  // Goblin 3 shield block: hasShield, targetAttacking and draw % 3 == 0 -> 0x11 (C:347-348).
  auto gb = base(3); gb.fields[0x3d4] = 1; gb.seed = zero(1);
  check(req(A::tick(Class::Goblin, gb, see_map({{{0, 30, 6000}, 1}}, {{"hasShield", 1}, {"targetAttacking", 1}}))) ==
        std::vector<std::uint32_t>{0x11}, "Goblin shield block");
  // Clockwork 5 after 6: draw & 3 == 0 -> 8 (C:400-401).
  auto cw = base(5); cw.fields[0x3cc] = 6; cw.seed = zero(1);
  check(req(A::tick(Class::Clockwork, cw, see_map({{{0, 36, 8000}, 1}}))) == std::vector<std::uint32_t>{8}, "Clockwork spin");
  // SeaMonster 5: draw & 1 == 0 -> 7, else draw % 3 == 0 -> 9 (C:475-476).
  auto sm = base(5); sm.seed = zero(1);
  check(req(A::tick(Class::SeaMonster, sm, answers({1}))) == std::vector<std::uint32_t>{7}, "SeaMonster 7 on even draw");
  sm.seed = seed_where([](auto d) { return (d[0] & 1) && d[1] % 3 == 0; }, 2);
  check(req(A::tick(Class::SeaMonster, sm, answers({1}))) == std::vector<std::uint32_t>{9}, "SeaMonster crush on second draw");
  // Warrior 6 combo 1 chain: even own draw -> 7 (C:389).
  auto ch = base(6); ch.fields[0x474] = 1; ch.ev = 2; ch.anim = 8; ch.seed = zero(1);
  check(req(A::tick(Class::Warrior, ch, see_map({{{0, 36, 8000}, 1}}))) == std::vector<std::uint32_t>{7}, "Warrior chain on even draw");
}

// --- C:115-127 filter and no-case states; C:131-154 entries and death calls.
void set_states() {
  for (const auto &n : NAMES) {
    const auto c = n.second;
    A::SetContext s; s.target = true; s.root = 30;
    check(A::set_state(c, s, 3).landed == 5, n.first + " root, target: 3 -> 5");
    check(A::set_state(c, s, 6).landed == (c == Class::Droid ? 0u : 6u), n.first + " root keeps 6 (Droid stores 0)");
    s.target = false;
    check(A::set_state(c, s, 3).landed == 0, n.first + " root, no target: 3 -> 0");
    s = {}; s.target = true; s.stun = 30;
    check(A::set_state(c, s, 3).landed == 0x13, n.first + " stun -> 0x13");
    s = {}; s.target = true; s.cooldown = 30;
    check(A::set_state(c, s, 3).landed == 0x10, n.first + " cooldown, target -> 0x10");
    s.target = false;
    const auto cl = A::set_state(c, s, 3);
    check(cl.landed == 3 && std::count(cl.cleared.begin(), cl.cleared.end(), std::pair<std::uint32_t, std::int64_t>{0x318, 0}),
          n.first + " cooldown cleared without a target");
    s = {}; s.target = true; s.cooldown = 30; s.root = 9; s.stun = 9;
    const auto d = A::set_state(c, s, 0x14);
    check(d.landed == 0x14 && d.cleared.size() == 4, n.first + " death clears the timers");
    // C:131-132: entering 0x10 stores +0x318 = 60, except SeaMonster. The
    // executed entry table (MOD:1164-1196) has no WarWraith 0x10 row, so its
    // entry stays unknown rather than assumed.
    const auto f = A::set_state(c, {}, 0x10);
    const bool writes60 = f.writes.size() == 1 && f.writes[0].offset == 0x318 && f.writes[0].base == 60;
    if (c == Class::WarWraith)
      check(!f.entry_known && f.writes.empty() && f.landed == 0x10, "WarWraith 0x10 entry not executed");
    else
      check(f.entry_known && writes60 == (c != Class::SeaMonster) &&
            (c != Class::SeaMonster || f.unwritten == std::vector<std::uint32_t>{0x318}),
            n.first + " flee cooldown (SeaMonster none)");
  }
  // C:124-127 no-case states store 0.
  const std::map<Class, std::vector<std::uint32_t>> nocase = {
    {Class::Kobold, {4, 0xa, 0x11}}, {Class::Ogre, {4, 0xa, 0x11, 0x17}}, {Class::Warrior, {4, 0x11}},
    {Class::Clockwork, {4, 0xa, 0x11, 0x17}}, {Class::Droid, {4, 6, 0xa, 0x11}},
    {Class::IronGuardian, {4, 0x11, 0x17}}, {Class::Amphian, {4, 0xa, 0x11}}};
  for (const auto &n : nocase)
    for (auto st : n.second) check(A::set_state(n.first, {}, st).landed == 0, "no-case state stores 0");
  // C:151-153: Droid 0x14/0x17 call neither dropLoot nor loot_generateDrops.
  for (std::uint32_t st : {0x14u, 0x17u}) {
    const auto r = A::set_state(Class::Droid, {}, st);
    check(r.calls == std::vector<std::string>{"onDeath"} &&
          r.absent == std::vector<std::string>{"dropLoot", "lootGenerate"}, "Droid has no loot call");
  }
  check(A::set_state(Class::WarWraith, {}, 0x14).calls == std::vector<std::string>{"lootGenerate", "onDeath"},
        "WarWraith calls loot_generateDrops (C:151-152)");
  check(A::set_state(Class::Goblin, {}, 0x14).calls == std::vector<std::string>{"dropLoot"}, "Goblin death call");
  // Entry timers (C:274-275, C:299-300, C:335-336): ranges and draw sources.
  std::uint32_t next = 1234;
  const auto draw = [&next] { return next; };
  const auto k1 = A::set_state(Class::Kobold, {}, 1).writes.at(0);
  check(k1.offset == 0xf4 && k1.resolve(1000, draw) == 1000 + 120 + 1234 % 120, "Kobold 1 deadline");
  const auto w7 = A::set_state(Class::WarWraith, {}, 7).writes.at(0);
  check(w7.offset == 0x3cc && w7.resolve(1000, draw) == 1000 + 300 + 1234 % 120, "WarWraith drain clock");
  const auto o6 = A::set_state(Class::Orc, {}, 6).writes.at(0);
  check(o6.resolve(1000, draw) == 10 + 1234 % 10, "Orc stand counter 10 + r%10");
  const auto o11 = A::set_state(Class::Orc, {}, 0x11).writes;
  check(o11.at(0).kind == A::EntryWrite::Clock && o11.at(0).resolve(1000, draw) == 1120 &&
        o11.at(1).offset == 0xa8 && o11.at(1).base == 0x200, "Orc block clock + 120 and cflag 0x200");
  A::SetContext global; global.global_refuses = true;
  check(A::set_state(Class::Ogre, global, 3).refused, "global filter refusal");
  A::SetContext wander; wander.x231 = 1;
  check(A::set_state(Class::Ogre, wander, 1).refused && !A::set_state(Class::Ogre, wander, 3).refused, "+0x231 refuses 1");
}

// --- C:28-60 level records and key counts.
void records() {
  unsigned total = 0;
  std::map<std::string, unsigned> creatures;
  for (const auto &r : A::level_creatures()) { total += r.count; creatures[r.level] += r.count; }
  check(total == 807, "807 creature/hazard records (C:34)");
  check(A::level_record_totals().size() == 11 && A::level_record_totals().at("blood1") == 363 &&
        A::level_record_totals().at("islc2") == 255, "level record totals (C:43-56)");
  for (const auto &l : A::level_record_totals()) check(creatures.at(l.first) <= l.second, "creature records within level totals");
  check(A::factory_pick("blood1", "koboldshaman").constructor == 0x27f0c0, "kobold shaman uses the Kobold class");
  check(A::factory_pick("islc2", "Crab").constructor == 0x2547fc && !A::class_of_constructor(0x2547fc),
        "critters are not one of the twelve classes (C:58-59)");
  check(refuses([] { A::factory_pick("blood1", "Ogre"); }), "unplaced type refused");
  const auto &k = A::key_counts();
  check(k.size() == 4 && k[0].count == 1 && std::string(k[1].type) == "SeaMonster" && k[1].count == 2 &&
        k[2].with.size() == 2 && k[3].without.size() == 2, "property-key counts (C:35-39)");
}

// --- Unknowns stay refused (C:271-272, C:341-342, C:451).
void unknowns() {
  check(refuses([] { A::tick(Class::Kobold, base(9)); }), "Kobold 9 body refused");
  check(refuses([] { A::set_state(Class::Kobold, {}, 9); }), "Kobold 9 entry refused");
  for (std::uint32_t s : {7u, 8u}) {
    check(refuses([s] { A::tick(Class::Goblin, base(s)); }), "Goblin 7/8 refused");
    check(refuses([s] { A::tick(Class::Froglock, base(s)); }), "Froglock 7/8 refused");
  }
  check(refuses([] { A::tick(Class::Ogre, base(0x19)); }), "unwalked state refused");
  // A class run requests these states; the request is executed, the body is not.
  auto archer = base(3); archer.fields[0x3d4] = 4;
  check(req(A::tick(Class::Goblin, archer, see_map({{{0, 400, 6000}, 1}}))).at(0) == 7, "Goblin archer requests 7");
}

// --- A few state-machine paths spelled out (C:253-479).
void machines() {
  auto ogre = base(0);
  check(req(A::tick(Class::Ogre, ogre, answers({1}))) == std::vector<std::uint32_t>{1}, "Ogre sight -> 1 (C:363)");
  check(req(A::tick(Class::WarWraith, base(1), answers({1}))) == std::vector<std::uint32_t>{0}, "WarWraith 1 sight -> 0 (C:283)");
  auto k1 = base(1); k1.deadline = 50;
  const auto kt = A::tick(Class::Kobold, k1, answers({0}));
  check(req(kt) == std::vector<std::uint32_t>{0} && !kt.has("moveToDest"), "Kobold 1 expiry does not move (C:257)");
  auto o2 = base(2); o2.fields[0x478] = 4;
  check(req(A::tick(Class::Orc, o2, answers({1}))) == std::vector<std::uint32_t>{3}, "Orc archer forced sight in 2 -> 3 (C:307)");
  o2.fields[0x478] = 1;
  check(req(A::tick(Class::Orc, o2, answers({1}))) == std::vector<std::uint32_t>{0}, "Orc plain sight in 2 -> 0 (C:307)");
  auto o4 = base(4); o4.fields[0x478] = 0;
  const auto t4 = A::tick(Class::Orc, o4, see_map({{{0, 30, 6000}, 1}}));
  check(req(t4) == std::vector<std::uint32_t>{6} && t4.events.back().name == "moveToTarget" && t4.draws == 1,
        "Orc 4: 6 then a draw and moveToTarget in the same tick (C:316-317)");
  auto o6 = base(6); o6.fields[0x478] = 1; o6.anim = 0; o6.deadline = 1;
  check(req(A::tick(Class::Orc, o6)) == std::vector<std::uint32_t>{3}, "Orc 6 counter 1 ends after the clip (C:326-327)");
  o6.anim = 8;
  check(req(A::tick(Class::Orc, o6)).empty(), "Orc 6 counter 1 not counted while the clip plays");
  auto g3 = base(3); g3.fields[0x3d4] = 4;
  const auto gt = A::tick(Class::Goblin, g3);
  check(count(gt, "moveToTarget") == 2, "Goblin archer 3 also runs the melee block (C:345-346)");
  auto c9 = base(9, false);
  const auto ct = A::tick(Class::Clockwork, c9, answers({0, 0, 0}));
  check(req(ct) == std::vector<std::uint32_t>{0, 3}, "Clockwork 9 without target still runs exit tests (C:405)");
  check(req(A::tick(Class::Clockwork, base(0x13), answers({0}))).empty(), "Clockwork 0x13 tests isNotStunned (C:409)");
  check(req(A::tick(Class::Amphian, base(0x13), answers({1}))) == std::vector<std::uint32_t>{3}, "Amphian 0x13 (C:468)");
  auto drain = base(7); drain.ev = 1; drain.anim = 8;
  const auto dt = A::tick(Class::WarWraith, drain);
  check(dt.has("skill+0x5c") && dt.events[dt.events.size() - 2] == E("hitAtReach", {36, 11, 22, 33, 0x404}),
        "WarWraith drain hitAtReach(36, 0x404) (C:290-291)");
  auto ogre7 = base(7); ogre7.ev = 1; ogre7.anim = 8;
  check(A::tick(Class::Ogre, ogre7).events.back() == E("hitAll", {0, 60, 0, 6000, 1, 5, 33, 0x100001}),
        "Ogre shield-bash literal damage 1-5 (C:370)");
  auto ks = base(8, false); ks.fields[0x3cc] = 2; ks.ev = 1; ks.anim = 8;
  check(A::tick(Class::Kobold, ks, by_name({{"wounded", {1}}})).has("addSpell"), "Kobold shaman heals without a target (C:270)");
  auto frog = base(3); frog.holder_hand1 = 0x10;
  check(req(A::tick(Class::Froglock, frog, see_map({{{0, 100, 0x7fff}, 1}}))) == std::vector<std::uint32_t>{0x10},
        "Froglock blowgun retreats close (C:448)");
  auto droid = base(0x17); droid.anim = 0x80; droid.fields[0x3cc] = 3600;
  const auto dr = A::tick(Class::Droid, droid);
  check(dr.has("damageInRadius") && dr.has("objDelete") && count(dr, "poolNew") == 11, "Droid fuse ends (C:420-422)");
  auto hammer = base(0); hammer.fields[0x3c8] = 1; hammer.seed = seed_where([](auto d) { return (d[0] & 1) && (d[4] & 1); }, 5);
  check(A::tick(Class::Clockwork, hammer).draws >= 5, "hammer steam puffs draw every tick (C:410-411)");
}

using V = std::vector<std::uint32_t>;
// --- CAI13-1: Froglock water (C:447-448; M2:4143-4160, Observed, never walked).
void froglock_water() {
  const auto in_reach = see_map({{{0, 400, 6000}, 1}});
  auto blow = base(3); blow.holder_hand1 = 0x10;
  check(req(A::tick(Class::Froglock, blow, in_reach)) == V{7}, "dry blowgun in reach -> 7 (C:447-448)");
  blow.water = A::FroglockWater{true, 1, 0, 0};
  check(req(A::tick(Class::Froglock, blow, in_reach)) == V{8}, "blowgun standing in water -> 8 (M2:4158-4159)");
  auto five = blow; five.state = 5;
  check(req(A::tick(Class::Froglock, five, in_reach)) == V{7}, "state 5 has no water branch (M2:4161-4162)");
  // Helper 0x265754 (M2:4143-4145): entered water, or water ahead just ended.
  const A::FroglockWater entered{true, 0, 0, 0}, ended{false, 0, 0, 1};
  const A::FroglockWater still_in{true, 1, 0, 0}, ahead_on{false, 0, 1, 1};
  for (std::uint32_t hand : {0x12u, 0x10u}) {
    for (const auto &w : {entered, ended}) {
      auto c = base(3); c.holder_hand1 = hand; c.water = w;
      const auto t = A::tick(Class::Froglock, c);
      check(req(t) == V{3} && t.events[t.events.size() - 2].name == "moveToTarget",
            "moving Froglock re-requests 3 after moveToTarget (M2:4157, M2:4160)");
      auto near = c;
      const auto u = A::tick(Class::Froglock, near, see_map({{{0, 40, 0x7fff}, 1}, {{0, 400, 0x7fff}, 1}}));
      check(req(u).empty() && u.has("turn"), "turning Froglock makes no water re-request");
    }
    for (const auto &w : {still_in, ahead_on}) {
      auto c = base(3); c.holder_hand1 = hand; c.water = w;
      check(req(A::tick(Class::Froglock, c)).empty(), "no water change: no re-request");
    }
  }
  auto wander = base(1); wander.water = entered;
  const auto tw = A::tick(Class::Froglock, wander);
  check(req(tw) == V{1} && tw.events[tw.events.size() - 2].name == "moveToDest",
        "state 1 re-requests 1 after moveToDest (M2:4152-4153)");
  check(req(A::tick(Class::Froglock, wander, answers({1}))) == V{0}, "state 1 sight -> 0 without the helper");
  auto flee = base(0x10); flee.water = ended;
  const auto tf = A::tick(Class::Froglock, flee, answers({1}));
  check(req(tf) == V{0x10} && !tf.has("attackReady") && !tf.has("flee"), "0x10 re-requests 0x10 first (M2:4178-4179)");
  flee.water = still_in;
  check(req(A::tick(Class::Froglock, flee, answers({1}))) == V{0}, "0x10 without a change: attack ready -> 0");
  auto missing = base(0); missing.water.reset();
  check(refuses([&] { A::tick(Class::Froglock, missing); }), "Froglock without a water state refuses");
  for (std::uint32_t bad : {2u, 255u}) {
    auto b = base(0); b.water = A::FroglockWater{false, bad, 0, 0};
    check(refuses([&] { A::tick(Class::Froglock, b); }), "water byte outside 0/1 refuses");
    b.water = A::FroglockWater{false, 0, 0, bad};
    check(refuses([&] { A::tick(Class::Froglock, b); }), "water byte outside 0/1 refuses");
  }
}

// --- CAI13-2: Clockwork object flag 2 (M2:3711-3716, M2:3721-3723, MOD:738-742).
void clockwork_flag2() {
  auto h = base(0); h.fields[0x3c8] = 1;
  h.seed = seed_where([](auto d) { return (d[0] & 1) && (d[1] & 1); }, 2);
  const auto clear = A::tick(Class::Clockwork, h);
  check(clear.draws >= 8, "flag 2 clear: both odd puffs draw a unit-ball velocity");
  h.object_flags = 2;
  check(A::tick(Class::Clockwork, h).draws == 2, "flag 2 set: the puff draws stay, the velocities do not");
  auto dead = base(0x14); dead.fields[0x3c8] = 1; dead.seed = h.seed;
  dead.ev = 4; dead.object_flags = 0;
  const auto burst = A::tick(Class::Clockwork, dead);
  check(burst.has("damageInRadius") && burst.draws == 3, "burst tick: radius damage draw and two puff draws");
  dead.object_flags = 0x4003;  // a later dead tick: the burst set flag 2 (M2:3716)
  const auto later = A::tick(Class::Clockwork, dead);
  check(!later.has("damageInRadius") && !later.has("poolNew") && later.draws == 2,
        "after the burst: no second burst, no puff velocities");
  auto cutter = base(0x14); cutter.ev = 4; cutter.object_flags = 2;
  check(!A::tick(Class::Clockwork, cutter).has("damageInRadius"), "cutter wreck with flag 2 does not burst");
  auto missing = base(0); missing.object_flags.reset();
  check(refuses([&] { A::tick(Class::Clockwork, missing); }), "Clockwork without object flags refuses");
}

// --- CAI13-3: unexecuted setState landings refuse (C:124, DT:401-461).
void unexecuted_landings() {
  const std::vector<std::pair<Class, std::uint32_t>> refused = {
    {Class::WarWraith, 8}, {Class::Droid, 9}, {Class::Goblin, 7}, {Class::Goblin, 8},
    {Class::SeaMonster, 7}, {Class::SeaMonster, 8}, {Class::SeaMonster, 9}, {Class::SeaMonster, 0x17},
    {Class::SeaMonster, 0x18}, {Class::Orc, 7}, {Class::Warrior, 9}, {Class::Warrior, 10}};
  for (const auto &r : refused) {
    A::SetContext s; s.target = true;
    check(refuses([&] { A::set_state(r.first, s, r.second); }), "unexecuted landing refused");
  }
  // Filter landings without an entry row keep the executed landing only.
  A::SetContext s; s.target = true;
  const auto ww3 = A::set_state(Class::WarWraith, s, 3);
  check(ww3.landed == 3 && !ww3.entry_known && ww3.writes.empty(), "WarWraith 3: filter landing, entry unknown");
  const auto sm6 = A::set_state(Class::SeaMonster, s, 6);
  check(sm6.landed == 6 && !sm6.entry_known, "SeaMonster 6: filter landing (DT:445), entry unknown");
  // A redirect onto an executed landing is accepted: Droid 1 has no row, but a
  // root timer turns it into 5 (DT:443). Root keeps 6-15, so 9 stays refused.
  A::SetContext rooted; rooted.target = true; rooted.root = 30;
  check(refuses([] { A::set_state(Class::Droid, {}, 1); }), "Droid 1 has no executed landing");
  check(A::set_state(Class::Droid, rooted, 1).landed == 5, "rooted Droid 1 lands the filter's 5");
  check(refuses([&] { A::set_state(Class::Droid, rooted, 9); }), "root keeps Droid 9, still unexecuted");
  const auto cw9 = A::set_state(Class::Clockwork, s, 9);
  check(cw9.landed == 9 && cw9.entry_known && cw9.writes.size() == 2, "Clockwork 9 entry (MOD:1184)");
  const auto ig9 = A::set_state(Class::IronGuardian, s, 9);
  check(ig9.landed == 9 && ig9.writes.size() == 1 && ig9.writes[0].offset == 0x3c8 && ig9.writes[0].base == 0,
        "IronGuardian 9 entry (MOD:1190)");
  // Every row ran with the class variant field at 0 (DT:410, DT:455).
  A::SetContext v; v.target = true; v.variant = 1;
  for (auto c : {Class::Kobold, Class::Orc, Class::Goblin, Class::Clockwork})
    check(refuses([&] { A::set_state(c, v, 0); }), "setState with a nonzero variant refused");
  check(!A::set_state(Class::Ogre, v, 0).refused, "classes without a variant field are not gated");
  v.variant = 0;
  check(A::set_state(Class::Kobold, v, 8).writes.at(0).base == 8, "Kobold 8 entry at variant 0 (MOD:1165)");
}

// --- CAI13-5: variant values outside the walked set refuse.
void variants() {
  const auto bad = [](Class c, std::uint32_t off, std::int64_t value) {
    auto k = base(0); k.fields[off] = value;
    return refuses([&] { A::tick(c, k); });
  };
  check(bad(Class::Kobold, 0x3cc, 3) && !bad(Class::Kobold, 0x3cc, 2), "Kobold +0x3cc 0-2 (MOD:152)");
  check(bad(Class::Orc, 0x478, 6) && bad(Class::Orc, 0x478, -1) && !bad(Class::Orc, 0x478, 5), "Orc +0x478 0-5 (MOD:1111)");
  check(bad(Class::Goblin, 0x3d4, 6) && !bad(Class::Goblin, 0x3d4, 5), "Goblin +0x3d4 0-5 (MOD:1115)");
  check(bad(Class::Clockwork, 0x3c8, 2) && !bad(Class::Clockwork, 0x3c8, 1), "Clockwork +0x3c8 0/1 (MOD:1125-1127)");
  for (std::uint32_t hand : {0u, 0x11u, 0x13u}) {
    auto f = base(0); f.holder_hand1 = hand;
    check(refuses([&] { A::tick(Class::Froglock, f); }), "Froglock holder other than 0x12/0x10 (MOD:1142)");
  }
}

// --- Kernel hooks used by the live runtimes.
void hooks() {
  check(A::droid_fuse_due(0, 99, 100) && !A::droid_fuse_due(0, 100, 100) && !A::droid_fuse_due(0, 101, 100),
        "Droid fuse +0x3cc < clock (C:416-417)");
  check(!A::droid_fuse_due(0x14, 0, 100) && !A::droid_fuse_due(0x17, 0, 100), "no fuse in 0x14/0x17");
  const auto k = A::tick(Class::Kobold, base(0));
  check(k.body == 7 && k.events[k.body].name == "sight", "Kobold body starts after pickAlly");
  auto ww = base(0); ww.fields[0x3c8] = 100;
  const auto w = A::tick(Class::WarWraith, ww);
  check(w.events[w.body - 1].name == "turn" && w.events[w.body].name == "sight", "WarWraith body starts after the fade turn");
  auto ex = base(3); ex.active = false;
  const auto e = A::tick(Class::Ogre, ex);
  check(e.body == e.events.size(), "an early exit has an empty body");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc < 2) throw std::runtime_error("usage: creature-ai-m1-3-tests <vectors.tsv>");
    exits();
    head_and_slots();
    thresholds();
    reach();
    hit_points();
    fade();
    orc_shaman_sight();
    guardian_turn();
    rare();
    set_states();
    records();
    unknowns();
    machines();
    froglock_water();
    clockwork_flag2();
    unexecuted_landings();
    variants();
    hooks();
    vectors(argv[1]);
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << "\n";
    return 1;
  }
  std::cout << "creature_ai_m1_3: " << checks << " checks passed\n";
  return 0;
}
