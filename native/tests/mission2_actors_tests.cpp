// Authored fixtures only. Expected call sequences are transcribed from the
// independently executed ref_mission2_enemies.py at ffff9a7, not from native
// output. Loaded services are explicit synthetic answers, never retail data.
#include "champions/mission2_actors.hpp"
#include "champions/combat_runtime.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace R = champions::simulation::rta;
namespace M = R::mission2_actors;
namespace C = R::combat_runtime;
namespace {
unsigned checks = 0;
void check(bool ok, const char *message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void refused(F fn) {
  bool caught = false;
  try { fn(); } catch (const M::Unsupported &) { caught = true; }
  check(caught, "required refusal");
}
void put(std::vector<std::uint8_t> &v, std::size_t p, std::uint32_t n, unsigned width = 4) {
  for (unsigned i = 0; i < width; ++i) v.at(p + i) = std::uint8_t(n >> (8 * i));
}
std::vector<std::uint8_t> lump(unsigned count) {
  const auto payload = 4 + count * 12 + count * 16;
  std::vector<std::uint8_t> v(payload + count, 0);
  put(v, 0, count);
  for (unsigned i = 0; i < count; ++i) {
    auto name = "fixture" + std::to_string(i);
    put(v, 4 + i * 12, 4 + count * 12 + i * 16);
    put(v, 8 + i * 12, payload + i);
    put(v, 12 + i * 12, 1);
    std::copy(name.begin(), name.end(), v.begin() + 4 + count * 12 + i * 16);
    v[payload + i] = std::uint8_t(i + 1);
  }
  return v;
}
std::shared_ptr<R::arena_modes::Executable> executable() {
  // One wholly synthetic ELF32 segment for three definition tables and
  // authored names. These bytes are constructed here, never extracted.
  constexpr unsigned base = 0x4ff000, file = 0x100, size = 0x4000;
  std::vector<std::uint8_t> v(file + size, 0);
  v[0] = 0x7f; v[1] = 'E'; v[2] = 'L'; v[3] = 'F'; v[4] = 1; v[5] = 1;
  put(v, 0x1c, 0x34); put(v, 0x2a, 0x20, 2); put(v, 0x2c, 1, 2);
  put(v, 0x34, 1); put(v, 0x38, file); put(v, 0x3c, base); put(v, 0x44, size);
  unsigned next = base + 0x2000;
  auto text = [&](const std::string &s) {
    const auto at = next;
    for (char c : s) v[file + next++ - base] = std::uint8_t(c);
    ++next;
    return at;
  };
  for (auto k : {M::Kind::Cutter, M::Kind::Droid, M::Kind::IronGuardian}) {
    const auto &p = M::profile(k);
    const auto offset = file + p.definition - base;
    put(v, offset, text("fixture" + std::to_string(p.entries) + ".lmp"));
    for (unsigned i = 0; i < p.entries; ++i)
      put(v, offset + 8 + i * 8, text("fixture" + std::to_string(i)));
  }
  auto e = std::make_shared<R::arena_modes::Executable>();
  check(e->load(std::move(v)), "synthetic executable maps");
  return e;
}
struct Fixture {
  std::shared_ptr<R::WorldAuthority> authority = std::make_shared<R::WorldAuthority>();
  champions::resources::Manager resources;
  std::shared_ptr<R::arena_modes::Executable> exe = executable();
  std::vector<M::Call> calls;
  std::vector<unsigned> event_ids;
  R::Raw events = 0;
  bool active = true, deny_filter = false;
  std::function<R::Raw(const M::Call &)> extra;
  M::Runtime runtime;
  Fixture() : runtime(*authority, resources, *exe, services()) {
    for (auto count : {26u,10u,34u}) {
      auto data = lump(count);
      resources.load_lmp("fixture" + std::to_string(count) + ".lmp", {data.data(), data.size()});
    }
  }
  M::Services services() {
    M::Services s;
    s.observe = [this](const M::Call &c) { calls.push_back(c); };
    s.initialize = [this](std::uint32_t, M::Kind, const std::vector<unsigned> &ids) { event_ids = ids; };
    s.call = [this](const M::Call &c) -> R::Raw {
      if (extra) return extra(c);
      if (c.function == 0x10c0f0) return active;
      if (c.function == 0x1162b0) return events;
      if (c.function == 0x10a9c0) return !deny_filter;
      return 0; // EXPLICIT synthetic lower-service answer, like the oracle.
    };
    return s;
  }
  M::Actor &actor(M::Kind k, unsigned seed = 0) {
    auto *o = authority->objects->new_object(M::profile(k).size, {0,0,0});
    check(o != nullptr, "synthetic actor block");
    authority->register_object(*o);
    authority->register_factory(o->addr);
    authority->memory.store(o->addr + 0x228, 4, R::from_int(100));
    authority->memory.store(o->addr + 0x22c, 4, R::from_int(100));
    authority->memory.store(o->addr + 0x24, 4, M::profile(k).definition);
    authority->memory.store(o->addr + 0x44, 4, seed);
    authority->memory.store(o->addr + 0x0c, 4, 0x20); // supplied Creature base flag
    authority->memory.store(o->addr + 0x310, 1, 0xff); // authored hostile team (-1)
    return runtime.adopt(k, *o, 123);
  }
  void store(unsigned a, unsigned v, unsigned width = 4) { authority->memory.store(a, width, v); }
  unsigned read(unsigned a, unsigned width = 4) { return unsigned(authority->memory.load(a, width)); }
  std::vector<unsigned> functions() {
    std::vector<unsigned> result;
    for (auto &c : calls) result.push_back(c.function);
    return result;
  }
};
void constructors() {
  for (auto seed : {20973u,7u})
    for (auto k : {M::Kind::Cutter,M::Kind::Hammer,M::Kind::Droid,M::Kind::IronGuardian}) {
      Fixture f;
      f.store(R::creature_hp::G_SIM_FRAME, 1234);
      auto &a = f.actor(k, seed);
      const auto at = a.object->addr;
      check(f.read(at + 0x44) == seed, "class adoption consumes no shared/own draw");
      check(f.read(at + 0x228) == R::from_int(100), "base stats not rerolled");
      check(f.read(at + 0x48) == M::profile(k).vtable, "class vtable");
      check(f.read(at + 0xb4, 1) == 4 && f.read(at + 0x232, 2) == 0, "kind and initial state");
      check(f.read(at + 0xec) == 1234 && f.read(at + 0xf4) == 1534, "frame-based fields");
      const auto &entries = a.object->class_def->entries;
      check(entries.size() == M::profile(k).entries, "runtime definition count");
      for (unsigned i = 0; i < entries.size(); ++i)
        check(entries[i].second == f.read(M::profile(k).definition + 8 * (i + 1) + 4), "resolved slot identity");
      if (k == M::Kind::Cutter || k == M::Kind::Hammer) {
        check(f.event_ids == std::vector<unsigned>({3,4,5,12,13,14,15,22,6,7,8,16,17,18}), "Clockwork event IDs");
        check(f.read(at + 0x3c8, 1) == (k == M::Kind::Hammer), "Clockwork subkind");
        check(f.read(at + 0x50) == at + 0x3d0, "owned holder identity");
        check(f.read(at + 0x1c) == (k == M::Kind::Cutter ? 0x41b00000u : 0x41e00000u), "Clockwork radius");
        if (k == M::Kind::Cutter) check(f.read(at + 0xdc) == 0x3f666666, "cutter scale");
      } else if (k == M::Kind::Droid) {
        check(f.event_ids == std::vector<unsigned>({3,9,10}), "Droid event IDs");
        check(f.read(at + 0x300) == 1234 && f.read(at + 0x3cc) == 4834, "Droid clock and expiry");
      } else {
        check(f.event_ids == std::vector<unsigned>({3,4,5,6,13,7,8}), "guardian event IDs");
        check(f.read(at + 0x3c0) == 0x453b8000 && f.read(at + 0x3c4) == 3000 &&
              f.read(at + 0x3d4) == 0, "guardian ranges and budget counter");
      }
      refused([&] { f.runtime.adopt(k, *a.object, 0); });
    }
  check(M::classify(0x24e124, "clockworkcutter") == M::Kind::Cutter, "factory cutter identity");
  check(M::classify(0x24e124, "clockworkhammer") == M::Kind::Hammer, "factory hammer identity");
  check(!M::classify(0x24e124, "ClockworkUnknown"), "unknown Clockwork variant refused");
  check(!M::classify(0x27b840, "IronGuardian"), "constructor and name must agree");
}
void balance_rows() {
  // ffff9a7 BALANCE / balance_factors, computed by its independent EE
  // arithmetic. In particular 100 * raw(0.01) is 0x3f7fffff, not 1.0.
  struct Row { const char *name; unsigned hash; std::array<unsigned,5> percentages, factors; };
  const Row rows[] = {
      {"clockworkcutter",0x39a4,{100,200,110,100,129},{0x3f7fffff,0x3fffffff,0x3f8ccccc,0x3f7fffff,0x3fa51eb8}},
      {"clockworkhammer",0x6d9b,{120,250,139,100,150},{0x3f999999,0x401fffff,0x3fb1eb84,0x3f7fffff,0x3fbfffff}},
      {"Droid",0x3475,{60,100,350,100,100},{0x3f199999,0x3f7fffff,0x405fffff,0x3f7fffff,0x3f7fffff}},
      {"IronGuardian",0x0cec,{1800,550,400,500,1800},{0x418fffff,0x40afffff,0x407fffff,0x409fffff,0x418fffff}}};
  std::vector<std::uint8_t> data(2 + 4 * 12, 0);
  put(data, 0, 4, 2);
  for (unsigned i = 0; i < 4; ++i) {
    put(data, 2 + i * 12, rows[i].hash, 2);
    for (unsigned j = 0; j < 5; ++j) put(data, 4 + i * 12 + j * 2, rows[i].percentages[j], 2);
  }
  for (const auto &r : rows)
    check(R::campaign_actors::balance_lookup(data.data(), data.size(), r.name) == r.factors,
          "four independent balance-factor rows");
  check(R::campaign_actors::balance_lookup(data.data(), data.size(), "dRoId") == rows[2].factors,
        "balance identity ignores ASCII case");
}
std::vector<unsigned> expected(M::Kind k, unsigned state) {
  // ref_mission2_enemies.TICK_PREFIX / tick_calls, exact immutable oracle.
  std::vector<unsigned> v = k == M::Kind::Cutter || k == M::Kind::Hammer ?
      std::vector<unsigned>{0x110d30,0x112948,0x10c0f0,0x118058,0x1162b0,0x115eb8,0x10ded0} :
      std::vector<unsigned>{0x3c2738,0x110d30,0x3c2738,0x112948,0x10c0f0,0x1162b0,0x115eb8,0x10ded0};
  const bool cw = k == M::Kind::Cutter || k == M::Kind::Hammer;
  if (state == 0) { v.push_back(0x10e658); v.push_back(0x10eed8); }
  if (state == 3) v.push_back(cw ? 0x24f90c : k == M::Kind::Droid ? 0x27c0a0 : 0x27b550);
  if (state == 6) {
    if (cw) { v.push_back(0x1187e0); v.push_back(0x119938); }
    else if (k == M::Kind::IronGuardian) v.push_back(0x119b58);
    v.push_back(cw ? 0x24f90c : k == M::Kind::Droid ? 0x27c0a0 : 0x27b550);
  }
  if (state == 20) {
    if (cw) v.push_back(0x113c48);
    if (k != M::Kind::Droid) v.push_back(0x1108a0);
  }
  if (cw) v.push_back(0x18f3d0);
  if (k == M::Kind::Hammer) { v.push_back(0x1509d0); v.push_back(0x1509d0); }
  return v;
}
void ticks_and_damage() {
  unsigned calls = 0;
  for (auto k : {M::Kind::Cutter,M::Kind::Hammer,M::Kind::Droid,M::Kind::IronGuardian})
    for (auto state : {0u,3u,6u,20u}) {
      Fixture f;
      auto &a = f.actor(k);
      const auto at = a.object->addr;
      f.store(at + 0x232, state, 2);
      f.events = state == 6 ? 1 : 0;
      f.runtime.tick(at);
      check(f.functions() == expected(k, state), "16 original synthetic tick sequences");
      calls += unsigned(f.calls.size());
      if (state == 6 && k != M::Kind::Droid) {
        const auto fn = k == M::Kind::IronGuardian ? 0x119b58u : 0x119938u;
        auto hit = std::find_if(f.calls.begin(), f.calls.end(), [fn](const M::Call &c) { return c.function == fn; });
        const auto flags = k == M::Kind::Cutter ? 0x40008u : k == M::Kind::Hammer ? 1u : 8u;
        check(hit != f.calls.end() && hit->args == std::vector<unsigned>{flags}, "three measured hit flags");
      }
      f.runtime.take_damage(at, 0, 8, R::from_int(25));
      check(f.read(at + 0x228) == R::from_int(75) && f.read(at + 0xc8) == R::from_int(25) &&
            f.read(at + 0x2dc) == 8, "inherited 100 -> 75 damage probe");
      refused([&] { f.runtime.take_damage(at, 0xdeadbeef, 8, R::from_int(1)); });
    }
  check(calls == 162, "audited tick aggregate 162 calls");
}
void death_and_explosion() {
  for (auto k : {M::Kind::Cutter,M::Kind::Hammer,M::Kind::Droid,M::Kind::IronGuardian}) {
    Fixture f;
    // Seed 7's first own draw is divisible by three; like the zero-result
    // oracle this follows the immediate eight-part Clockwork route.
    auto &a = f.actor(k, 7);
    const auto at = a.object->addr;
    f.runtime.set_state(at, 20);
    std::vector<unsigned> expected_death{0x10a9c0};
    if (k == M::Kind::Cutter || k == M::Kind::Hammer) {
      expected_death.push_back(0x1509d0);
      expected_death.insert(expected_death.end(), 8, 0x24fd8c);
    }
    if (k != M::Kind::Droid) expected_death.push_back(0x10f2c8);
    if (k != M::Kind::IronGuardian) expected_death.push_back(0x1509d0);
    expected_death.insert(expected_death.end(), {0x16cfe0,0x10beb8});
    check(f.functions() == expected_death, "four audited death routes");
    for (const auto &c : f.calls) if (c.function == 0x10f2c8)
      check(c.floats == std::vector<unsigned>({0x42100000,0x40000000,0x3fcccccc,0x3f199999,0x3fb33333,0x3e800000}), "six raw loot floats");
    check(a.death_reported && f.read(at + 0x232, 2) == 20, "death stores state");
    const auto n = f.calls.size();
    f.runtime.set_state(at, 20);
    check(n == f.calls.size(), "death is not replayed");
    refused([&] { f.runtime.set_state(at, 9); });
  }
  Fixture f;
  auto &a = f.actor(M::Kind::Droid);
  const auto at = a.object->addr;
  f.store(at + 0x232, 0x17, 2); f.store(at + 0x11a, 0x80, 2); f.store(at + 0xd2, 123, 2);
  f.runtime.tick(at);
  std::vector<unsigned> explode{0x3c2738,0x110d30,0x3c2738,0x112948,0x10c0f0,0x1162b0,0x115eb8,0x10ded0,
      0x1ef270,0x213ea0,0x1f3d00,0x17365c,0x214230,0x161270,0x1509d0,0x19f110,0x14fdb8,0x201bd8,0x14f200};
  for (unsigned i = 0; i < 10; ++i) { explode.push_back(0x14fdb8); explode.push_back(0x155a0c); }
  check(f.functions() == explode, "original Droid explosion dispatch order");
  for (const auto &c : f.calls) if (c.function == 0x161270)
    check(c.args == std::vector<unsigned>({123,0x40840,0,0}) &&
          c.floats == std::vector<unsigned>({0x42700000,0x43100000}), "measured Droid blast inputs");
  check((f.read(at + 0xc) & 0xa) == 0xa, "exploded object flags");
}
void refusals_still_process_damage() {
  Fixture f;
  auto &a = f.actor(M::Kind::Cutter);
  const auto at = a.object->addr;
  f.runtime.contain_refusals(true);
  unsigned damage_calls = 0;
  f.extra = [&](const M::Call &c) -> unsigned {
    if (c.function == 0x110d30) ++damage_calls;
    if (c.function == 0x10c0f0) return 1;
    if (c.function == 0x118058) throw M::Unsupported("synthetic missing holder answer");
    return 0;
  };
  f.runtime.tick(at); f.runtime.tick(at);
  check(a.ai_refusal.has_value() && damage_calls == 2 && f.runtime.refusals().size() == 1,
        "AI refusal is contained without freezing damage");
  unsigned drops = 0;
  f.extra = [&](const M::Call &c) -> unsigned {
    if (c.function == 0x10a9c0) return 1;
    if (c.function == 0x10f2c8) { ++drops; throw M::Unsupported("synthetic missing loot answer"); }
    return 0;
  };
  f.runtime.set_state(at, 20); f.runtime.set_state(at, 20);
  check(drops == 1 && a.death_refusal && !a.death_reported, "incomplete death is labelled and never double-rolls");
}
void lifetime_boundaries() {
  Fixture invalid_state;
  auto &invalid_droid = invalid_state.actor(M::Kind::Droid);
  const auto invalid_at = invalid_droid.object->addr;
  invalid_state.store(invalid_at + 0x232, 6, 2);
  invalid_state.store(invalid_at + 0x11a, 8, 2);
  invalid_state.events = 1;
  invalid_state.runtime.tick(invalid_at);
  check(invalid_state.calls.back().function == 0x27c0a0 &&
        invalid_state.calls.back().args == std::vector<unsigned>{0},
        "Droid's unlisted state always requests reset despite an active clip");
  check(std::none_of(invalid_state.calls.begin(), invalid_state.calls.end(), [](const M::Call &c) {
          return c.function == 0x119938 || c.function == 0x119b58;
        }), "Droid state 6 never dispatches a melee hit");
  Fixture f;
  auto &droid = f.actor(M::Kind::Droid);
  const auto at = droid.object->addr;
  f.active = false;
  f.runtime.tick(at);
  check(f.functions() == std::vector<unsigned>({0x3c2738,0x110d30,0x3c2738,0x112948,0x10c0f0}),
        "inactive Droid stops before animation and AI");
  f.active = true;
  f.store(at + 0x300, 3600);
  f.runtime.tick(at); // expiry is strictly deadline < clock
  f.store(at + 0x300, 3601);
  refused([&] { f.runtime.tick(at); });
  check(f.read(at + 0x232, 2) == 0, "unexecuted fuse entry cannot fabricate an explosion state");
  Fixture guardian;
  auto &boss = guardian.actor(M::Kind::IronGuardian);
  guardian.store(R::creature_hp::G_SIM_FRAME, 1);
  refused([&] { guardian.runtime.tick(boss.object->addr); });
  guardian.store(boss.object->addr + 0x3d4, 50);
  guardian.runtime.tick(boss.object->addr); // exhausted budget never attempts a spawn
  check(guardian.read(boss.object->addr + 0x3d4) == 50, "no invented Droid beyond the documented budget");
}
void shared_world_damage() {
  for (auto k : {M::Kind::Cutter,M::Kind::Hammer,M::Kind::IronGuardian}) {
    Fixture f;
    auto services = C::flat_fixture_services();
    C::World w(f.authority, services, 0, C::Composition::Full);
    auto &actor = f.actor(k);
    const auto at = actor.object->addr;
    const auto &p = M::profile(k);
    w.define_creature_table(p.vtable, {{4,p.run},{27,p.set_state}});
    w.adopt_creature(at);
    check(w.slot_function(at, 2) == 0x116638, "shared World uses inherited takeDamage slot");
    C::Spawn player;
    player.position = {R::from_int(40),0,0};
    player.team = 0;
    const auto victim = w.spawn_player(0, player);
    w.relink(at + C::TARGET, victim);
    w.take_damage(at, victim, 8, R::from_int(25));
    check(f.read(at + 0x228) == R::from_int(75), "shared live-attacker damage reaches the class bytes");
    check(w.target(at + C::LAST_ATTACKER) == victim, "live attacker handle shares the World");
    f.store(at + 0x232, 6, 2); f.store(at + 0x11a, 8, 2);
    f.store(at + 0x38, 0, 2);
    f.extra = [&](const M::Call &c) -> unsigned {
      if (c.function == 0x10c0f0 || c.function == 0x1162b0) return 1;
      if (c.function == 0x119938 || c.function == 0x119b58) {
        C::Arc arc;
        arc.near = 0; arc.far = R::from_int(k == M::Kind::IronGuardian ? 84 : 36);
        arc.facing = 0; arc.angle = k == M::Kind::IronGuardian ? 10000 : 8000;
        arc.minimum = 25; arc.maximum = 25; arc.attack = 100000; arc.flags = c.args.at(0);
        return c.function == 0x119938 ? w.hit_target_in_arc(at, arc) : w.hit_all_in_arc(at, arc);
      }
      return 0;
    };
    // Players accumulate damage at +0xc8; processDamage applies it to the
    // session record, not the Creature object's +0x228 word. State 6 is an
    // authored fixture avoiding a separate Strong Stance reaction service.
    w.write(victim + 0x58c, 4, 6);
    const auto before = R::Raw(w.read(C::PLAYERS + 0xac));
    f.runtime.tick(at);
    if (!R::lt(0, R::Raw(w.read(victim + 0xc8)))) {
      std::cerr << "synthetic hit kind=" << int(k) << " target=" << w.target(at + C::TARGET)
                << " victim=" << victim << " state=" << f.read(at + 0x232, 2) << '\n';
      for (const auto &e : w.events()) {
        std::cerr << e.name;
        for (auto arg : e.args) std::cerr << ' ' << arg;
        std::cerr << '\n';
      }
    }
    check(R::lt(0, R::Raw(w.read(victim + 0xc8))), "synthetic hit reaches Player_takeDamage");
    w.process_damage(victim);
    check(R::lt(R::Raw(w.read(C::PLAYERS + 0xac)), before), "synthetic class hit deals actual damage through shared combat");
  }
  Fixture f;
  C::World w(f.authority, C::flat_fixture_services(), 0, C::Composition::Full);
  auto &droid = f.actor(M::Kind::Droid);
  const auto at = droid.object->addr;
  w.define_creature_table(0x515da8, {{4,0x27ba78},{27,0x27c0a0}});
  w.adopt_creature(at);
  C::Spawn player;
  player.position = {R::from_int(40),0,0}; player.team = 0;
  const auto victim = w.spawn_player(0, player);
  f.store(at + 0x232, 0x17, 2); f.store(at + 0x11a, 0x80, 2); f.store(at + 0xd2, 25, 2);
  f.extra = [&](const M::Call &c) -> unsigned {
    if (c.function == 0x10c0f0) return 1;
    if (c.function == 0x161270)
      w.damage_in_radius(at, {0,0,0}, c.args.at(0), c.args.at(1), c.args.at(2), c.args.at(3),
                         c.floats.at(0), c.floats.at(1));
    return 0;
  };
  w.write(victim + 0x58c, 4, 6);
  const auto before = R::Raw(w.read(C::PLAYERS + 0xac));
  f.runtime.tick(at);
  check(R::lt(0, R::Raw(w.read(victim + 0xc8))), "Droid blast reaches Player_takeDamage");
  w.process_damage(victim);
  check(R::lt(R::Raw(w.read(C::PLAYERS + 0xac)), before), "synthetic Droid blast deals actual shared-World damage");
}
} // namespace
int main() {
  try {
    constructors(); balance_rows(); ticks_and_damage(); death_and_explosion(); refusals_still_process_damage(); lifetime_boundaries(); shared_world_damage();
    std::cout << "mission2 actors: " << checks << " checks, 16 ticks/162 calls, 3 hit dispatches, 4 deaths, 1 explosion PASS\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
