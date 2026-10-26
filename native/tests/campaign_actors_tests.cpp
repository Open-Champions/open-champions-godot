#include "champions/campaign_actors.hpp"
#include "champions/item_generation.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace C = R::campaign_actors;
namespace O = R::objects;
namespace I = R::items;
namespace {
void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
void put(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i != 4; ++i) b.at(at + i) = std::uint8_t(v >> (i * 8));
}
std::vector<std::uint8_t> lump(const std::vector<std::string> &names) {
  std::size_t chars = 0;
  for (const auto &n : names) chars += n.size() + 1;
  const auto table_end = 4 + names.size() * 12;
  std::vector<std::uint8_t> b(table_end + chars + names.size(), 0);
  put(b, 0, std::uint32_t(names.size()));
  std::size_t name_at = table_end, data_at = table_end + chars;
  for (std::size_t i = 0; i < names.size(); ++i) {
    put(b, 4 + i * 12, std::uint32_t(name_at));
    put(b, 8 + i * 12, std::uint32_t(data_at));
    put(b, 12 + i * 12, 1);
    for (char c : names[i]) b[name_at++] = std::uint8_t(c);
    ++name_at;
    b[data_at++] = std::uint8_t(i + 1); // authored synthetic payload
  }
  return b;
}
template <class F> void refuses(F fn, const char *why) {
  bool caught = false;
  try { fn(); } catch (const C::Unsupported &) { caught = true; }
  check(caught, why);
}
std::pair<std::vector<std::string>, std::uint64_t>
seeded_loot(const C::LootArgs &args, std::uint32_t seed) {
  I::Tables tables;
  tables.quality_rows.assign(12, {0x3e800000, 0x3f000000, 0x3e800000});
  auto def = [&](const std::string &name) {
    auto d = std::make_shared<I::Definition>();
    d->name = name;
    d->index = std::int32_t(tables.defs.size());
    d->addr = I::A_DEFS + 64 * std::uint32_t(d->index);
    d->category = 2;
    d->count = 1;
    tables.defs.push_back(d);
  };
  def("arrows"); def("recall");
  tables.arrows = "arrows"; tables.recall = "recall";
  for (int i = 0; i < 12; ++i) {
    auto mana = "p_m" + std::to_string(i);
    auto heal = "p_h" + std::to_string(i);
    def(mana); def(heal);
    tables.mana_potions.push_back(mana);
    tables.heal_potions.push_back(heal);
  }
  I::Game game;
  game.num_players = 1; game.difficulty = 1;
  game.level = R::from_int(22);
  I::Rng rng(seed);
  I::World world;
  I::loot_generate_drops(tables, game, rng, world,
      args.gold_scale, args.gold_chance, args.quality_row, args.item_chance);
  std::vector<std::string> kinds;
  for (const auto &drop : world.drops) kinds.push_back(drop.kind);
  return {kinds, rng.draws};
}

// Synthetic service vectors mirror research-boar-ai 3f81c89's original-code
// branch probes. The services are deliberately scripted, not a live tick.
void ai_vectors(champions::resources::Manager &resources) {
  unsigned cases = 0;
  for (const unsigned seed : {20973u, 7u}) {
    O::World world(O::CallbackPolicy::ReferenceBase);
    C::Actor *actor = nullptr;
    std::vector<std::string> calls;
    std::vector<C::AnimationRequest> animations;
    R::Raw events = 0, flags = 0, last_hit_flags = 0;
    unsigned target_n = 0, enemy_n = 0, target_yes = 0, enemy_yes = 0;
    bool active = true, stopped = false, visible = false, away = false;
    C::Services s;
    s.model_hide_mask = [](auto, auto) { return UINT64_C(0); };
    s.animation_init = [](auto, auto) {};
    s.set_def_entry_params = [](auto, const auto &) {};
    s.creature_init = [](auto) {};
    s.process_damage = [&](auto) { calls.emplace_back("damage"); };
    s.creature_run = [&](auto) { calls.emplace_back("run"); return stopped; };
    s.update_active = [&](auto) { calls.emplace_back("active"); return active; };
    s.animation_step = [&](auto) { calls.emplace_back("anim"); return events; };
    s.knockback = [&](auto) { calls.emplace_back("knockback"); };
    s.pick_target = [&](auto) { calls.emplace_back("target"); };
    s.can_see_target = [&](auto, auto force) { check(force == 0, "force 0"); return visible; };
    s.away_from_home = [&](auto) { return away; };
    s.clear_path = [&](auto) { calls.emplace_back("clear"); };
    s.pick_wander_dest = [&](auto) { calls.emplace_back("wander"); };
    s.move_to_dest = [&](auto) { calls.emplace_back("dest"); };
    s.move_to_target = [&](auto) { calls.emplace_back("chase"); };
    s.turn_to_target = [&](auto) { calls.emplace_back("turn"); };
    s.flee_from_target = [&](auto, auto kind) { check(kind == 0, "flee kind 0"); calls.emplace_back("flee"); };
    s.move_zero = [&](auto) { calls.emplace_back("move0"); };
    s.hit_all_in_arc = [&](auto, auto f) { last_hit_flags = f; calls.emplace_back("hit"); };
    s.filter_state = [](auto, auto requested) -> std::optional<std::uint32_t> { return requested; };
    s.animation_start = [&](auto, const C::AnimationRequest &a) { animations.push_back(a); };
    s.target_in_arc = [&](auto, auto f, auto arc, auto near, auto far) {
      check(std::int16_t(f) == 0 || std::int16_t(f) == 8000 || std::int16_t(f) == -8000,
            "class cone facing");
      check(arc == 3000 || arc == 8000 || arc == 0x7fff, "class arc");
      check((near == 0 && (far == 0x41c80000 || far == 0x41a00000 || far == 0x41200000)) ||
            (near == 0x41c80000 && far == 0x42c80000), "class range");
      return ++target_n == target_yes;
    };
    s.enemy_in_cone = [&](auto, auto, auto arc, auto near, auto far) {
      check((arc == 3000 || arc == 8000) && near == 0 && far == 0x41c80000, "alternative enemy cone");
      return ++enemy_n == enemy_yes;
    };
    C::Runtime runtime(world, resources, s);
    actor = &runtime.construct(C::Kind::WarBoar, {}, 0, {}, R::from_int(100));
    const auto a = actor->object->addr;
    auto &m = actor->hp.mem;
    const auto reset = [&](unsigned state, unsigned own_seed, bool target = true) {
      m.store(a + 0x232, 2, state); actor->state = state;
      m.store(a + 0x234, 4, target ? 0x1000 : 0); // synthetic handle presence
      m.store(a + 0x11a, 2, 8); m.store(a + 0x3c8, 4, 0);
      m.store(a + 0x3cc, 4, 6); m.store(a + 0x314, 2, 0);
      m.store(a + 0x318, 2, 0); m.store(a + 0x31c, 2, 0);
      m.store(a + 0x300, 4, 0); m.store(a + 0xf4, 4, 0);
      actor->object->seed = own_seed;
      target_n = enemy_n = target_yes = enemy_yes = 0;
      calls.clear(); animations.clear(); events = flags = 0;
      visible = away = stopped = false; active = true;
    };
    const auto draw = [](unsigned &value) { value = value * 0x19660du + 0x3c6ef35fu; return value >> 16; };
    for (unsigned state : {0u,1u}) for (bool seen : {false,true}) {
      reset(state,seed); visible = seen; m.store(a + 0xf4,4,180);
      runtime.tick(a);
      check(actor->state == (seen ? (state == 0 ? 3u : 0u) : state), "idle and wander sight transitions differ");
      if (state == 1 && !seen) check(calls.back() == "dest", "unseen wandering continues to destination");
      ++cases;
    }
    for (const auto clock : {180u,181u}) {
      reset(0,seed); m.store(a + 0x300,4,clock); m.store(a + 0xf4,4,180);
      runtime.tick(a); check(actor->state == (clock == 180 ? 0u : 1u), "strict idle expiry"); ++cases;
    }
    reset(0,seed); away = true; runtime.tick(a);
    check(actor->state == 1, "away from home enters wander before deadline"); ++cases;
    std::mt19937 samples(seed);
    for (unsigned state : {3u, 5u})
      for (unsigned ordinal = 1; ordinal <= (state == 3 ? 8u : 6u); ++ordinal)
        for (unsigned last : {6u, 8u}) for (unsigned trial = 0; trial < 12; ++trial) {
          const auto own_seed = samples();
          reset(state, own_seed); m.store(a + 0x3cc, 4, last);
          const auto target_tests = state == 3 ? 5u : 3u;
          if (ordinal <= target_tests) target_yes = ordinal;
          else enemy_yes = ordinal - target_tests;
          unsigned expected = state, model = own_seed;
          if (state == 3) {
            if (ordinal == 1 || ordinal == 6) {
              const unsigned choices[]{8, 9, 6, 6, 6}; expected = choices[draw(model) % 5];
            } else if (ordinal == 2) expected = 10;
            else if (ordinal == 3 || ordinal == 7) expected = 8;
            else if (ordinal == 4 || ordinal == 8) expected = 9;
          } else {
            const unsigned choices[]{6, 8, 9, 6, 8, 9}; expected = choices[ordinal - 1];
            if (ordinal == 1 && last == 6 && draw(model) % 3 == 0) {
              const unsigned combo[]{10, 8, 9}; expected = combo[draw(model) % 3];
            }
          }
          const auto shared = world.shared.state;
          runtime.tick(a);
          check(actor->state == expected && actor->object->seed == model && world.shared.state == shared,
                "research choice and exact own/shared RNG consumption");
          if (ordinal == 5 && state == 3) check(calls.back() == "turn", "turn after all attack queries");
          ++cases;
        }
    for (unsigned state = 6; state <= 10; ++state) for (unsigned small : {0u, 1u})
      for (unsigned ev = 0; ev < 4; ++ev) {
        reset(state, seed); events = ev; m.store(a + 0x3d0, 1, small);
        m.store(a + 0x11a, 2, 0); // original probe's finished-animation default
        auto model = seed;
        if ((ev & 1) && (state == 7 || (state == 10 && !small)))
          flags = draw(model) % 3 == 0 ? 0x80008 : 0x40008;
        else flags = 8;
        runtime.tick(a);
        check(actor->state == 3 && m.load(a + 0x3c8, 4) == (ev & 1) && actor->object->seed == model,
              "attack completion, hit latch and draws");
        check(std::count(calls.begin(), calls.end(), "hit") == int(bool(ev & 1)) &&
              (!(ev & 1) || last_hit_flags == flags), "attack event flags");
        ++cases;
      }
    reset(6, seed); events = 1;
    runtime.tick(a); runtime.tick(a);
    check(std::count(calls.begin(), calls.end(), "hit") == 2, "latch does not suppress repeated supplied events"); ++cases;
    for (unsigned state = 6; state <= 10; ++state) for (unsigned ev : {0u,1u}) {
      reset(state,seed); events = ev; target_yes = 1;
      runtime.tick(a);
      check(std::count(calls.begin(),calls.end(),"turn") == int(state == 7 || state == 10 || (state == 6 && !ev)),
            "only state 6's turning is gated by the hit latch"); ++cases;
    }
    for (unsigned playing : {0u, 8u}) {
      reset(6, seed); events = 2; m.store(a + 0x11a, 2, playing); target_yes = playing ? 2 : 1;
      runtime.tick(a); check(actor->state == 5, "continuation marker even at animation end"); ++cases;
    }
    for (unsigned old : {0u,3u,5u,6u,7u,10u,15u,16u,18u,19u})
      for (unsigned requested : {0u,1u,3u,5u,6u,7u,8u,9u,10u,18u,19u}) {
        reset(old, seed); m.store(a + 0x300, 4, 900); m.store(a + 0x3c8, 4, 99);
        runtime.set_state(a, requested);
        const bool rejected = requested == 18 && old >= 6 && old <= 15;
        check(actor->state == (rejected ? old : requested) && animations.size() == (rejected ? 0u : 1u),
              "requested versus accepted hit reaction");
        if (requested == 0) check(m.load(a + 0xf4, 4) == 1080, "idle deadline is creature clock plus 180");
        if (requested >= 6 && requested <= 10)
        {
          check(m.load(a + 0x3c8, 4) == 0 && m.load(a + 0x3cc, 4) == requested, "attack entry words");
          const unsigned offsets[]{0x24,0x54,0x2c,0x3c,0x34};
          const R::Raw animation_flags[]{8,0x808,0x4008,0x4008,0x808};
          const auto &r = animations.front();
          check(r.definition_offset == offsets[requested-6] && r.flags == animation_flags[requested-6] &&
                r.blend_in == 0x3e999999 && r.blend_out == 0x3e999999 && !r.sync,
                "executed attack entry animation ABI");
        }
        ++cases;
      }
    for (unsigned state : {3u,5u}) for (unsigned rooted : {0u,1u}) {
      reset(state,seed,false); m.store(a + 0x314,2,rooted);
      runtime.tick(a);
      check(actor->state == (state == 3 ? 0u : rooted ? 5u : 3u), "target loss and rooted combo fallback"); ++cases;
    }
    for (unsigned state : {16u,18u,19u}) for (unsigned ready : {0u,1u}) {
      reset(state, seed);
      m.store(a + 0x318, 2, ready ? 0 : 1); m.store(a + 0x31c, 2, ready ? 0 : 1);
      m.store(a + 0x11a, 2, ready ? 0 : 0x10);
      runtime.tick(a);
      check(actor->state == (ready ? (state == 16 ? 0u : 3u) : state), "recovery state");
      if (!ready && state == 16) check(calls.back() == "flee", "cooldown drives retreat");
      ++cases;
    }
    reset(0, seed); runtime.tick(a);
    check(calls == std::vector<std::string>({"damage","run","active","anim","knockback","target"}), "tick prefix order"); ++cases;
    reset(0, seed); stopped = true; runtime.tick(a);
    check(calls.size() == 2, "base run early return"); ++cases;
    reset(0, seed); active = false; runtime.tick(a);
    check(calls.size() == 3, "inactive early return"); ++cases;
  }
  std::cout << "warboar synthetic AI cases=" << cases << " seeds=2\n";
}
}
int main() {
  // Definition slot order is the round-5 reference model. No retail data.
  const std::vector<std::string> boar{
      "warboar.vif", "warboar.tex", "warboar_b.tex", "atak_mid.anm",
      "atak_left_close.anm", "atak_front.anm", "atak_right_close.anm",
      "backup.anm", "charge.anm", "chargetak.anm", "death1.anm",
      "death2.anm", "hit2front.anm", "hit2left.anm", "hit2right.anm",
      "idle1.anm", "roar.anm", "walk.anm", "walk_trn_lft.anm",
      "walk_trn_ryt.anm"};
  const std::vector<std::string> minotaur{
      "minotaur.vif", "minotaur.tex", "atak_180.anm", "atak_back.anm",
      "atak_f1.anm", "atak_f2.anm", "atak_f3.anm", "atak_left.anm",
      "atak_right.anm", "block.anm", "death.anm", "death2.anm",
      "gore.anm", "hitreaction.anm", "idle1.anm", "idle2.anm",
      "run.anm", "taunt.anm", "taunt2.anm", "walk.anm", "axe.vif",
      "axe.tex", "shield.vif", "shield.tex", "shield2.vif", "shield2.tex"};
  champions::resources::Manager resources;
  auto b = lump(boar), m = lump(minotaur);
  resources.load_lmp("warboar.lmp", {b.data(), b.size()});
  resources.load_lmp("minotaur.lmp", {m.data(), m.size()});
  ai_vectors(resources);
  std::vector<std::uint8_t> balance(2 + 3 * 12, 0);
  put(balance, 0, 3);
  const std::array<std::uint16_t, 3> hashes{{0xfe90, 0x270c, 0xec70}};
  for (std::size_t row = 0; row < hashes.size(); ++row) {
    const auto at = 2 + row * 12;
    balance[at] = std::uint8_t(hashes[row]);
    balance[at + 1] = std::uint8_t(hashes[row] >> 8);
    for (unsigned i = 0; i < 5; ++i) {
      balance[at + 2 + i * 2] = 100;
      balance[at + 3 + i * 2] = 0;
    }
  }
  const auto factors = C::balance_lookup(balance.data(), balance.size(), "WarBoarSmall");
  check(factors[0] == R::mul(R::from_int(100), 0x3c23d70a),
        "round-2 balance percent f32");
  check(C::balance_lookup(balance.data(), balance.size(), "AbsentType") ==
        std::array<R::Raw,5>{R::ONE,R::ONE,R::ONE,R::ONE,R::ONE},
        "Creature_ctor missing balance row defaults to five 1.0 factors");
  refuses([&] { C::balance_lookup(balance.data(), balance.size() - 1, "WarBoar"); },
          "truncated balance refusal");
  O::World world(O::CallbackPolicy::ReferenceBase);
  world.shared.state = 20973;
  std::vector<std::string> calls;
  std::vector<C::LootArgs> drops;
  R::Raw event_bits = 0;
  bool sight = false, away = false;
  C::Services services;
  services.process_damage = [&](std::uint32_t) { calls.emplace_back("damage"); };
  services.creature_run = [&](std::uint32_t) { calls.emplace_back("run"); return false; };
  services.update_active = [](std::uint32_t) { return true; };
  services.animation_step = [&](std::uint32_t) { calls.emplace_back("anim"); return event_bits; };
  services.knockback = [&](std::uint32_t) { calls.emplace_back("knockback"); };
  services.pick_target = [&](std::uint32_t) { calls.emplace_back("target"); };
  services.dead_update = [&](std::uint32_t, R::Raw) { calls.emplace_back("dead"); };
  services.can_see_target = [&](std::uint32_t, R::Raw force) {
    check(force == 0, "WarBoar sight force 0"); return sight;
  };
  services.away_from_home = [&](std::uint32_t) { return away; };
  services.clear_path = [](std::uint32_t) {};
  services.pick_wander_dest = [](std::uint32_t) {};
  services.move_zero = [](std::uint32_t) {};
  services.target_in_arc = [](std::uint32_t, R::Raw, R::Raw, R::Raw, R::Raw) { return false; };
  services.enemy_in_cone = services.target_in_arc;
  services.hit_all_in_arc = [&](std::uint32_t, std::uint32_t flags) {
    check(flags == 8, "round-2 boar attack flags"); calls.emplace_back("hit");
  };
  services.model_hide_mask = [&](std::uint32_t model, std::uint32_t variant) {
    check(model != 0, "r5 hide mask receives resolved model slot");
    check(variant == 0 || variant == 1, "bounded hide variant");
    calls.emplace_back("mask");
    return UINT64_C(0x1234); // synthetic service result, not a game value
  };
  services.minotaur_variant = [&](std::uint32_t, std::uint32_t) {
    calls.emplace_back("variant"); return 1u; // r5 stub answer
  };
  services.minotaur_base_flags = [](std::uint32_t) {
    return UINT64_C(0x80); // synthetic base Creature_ctor answer
  };
  services.animation_init = [&](std::uint32_t, bool push) {
    calls.emplace_back(push ? "animPush" : "animClear");
  };
  std::vector<C::EventParams> events;
  std::vector<std::array<std::uint32_t, 4>> hands;
  services.set_def_entry_params = [&](std::uint32_t, const C::EventParams &p) {
    events.push_back(p);
  };
  services.creature_init = [&](std::uint32_t) { calls.emplace_back("init"); };
  services.weapon_shield = [&](std::uint32_t holder, std::uint32_t hand,
                               std::uint32_t part, std::uint32_t size) {
    check((hand == 1 && part == 0x15) || (hand == 2 && part == 0x19),
          "r5 Minotaur hands");
    check(size == 0x20, "r5 Minotaur holder size");
    hands.push_back({holder, hand, part, size});
    calls.emplace_back("hand");
  };
  services.minotaur_initial_state = [&](std::uint32_t) { calls.emplace_back("state0"); };
  services.creature_drop_loot = [&](std::uint32_t, const C::LootArgs &a) {
    // Synthetic answer for the rank-0 untinted ordinary call only; the runtime
    // must refuse every other case before reaching it.
    calls.emplace_back("dropLoot");
    calls.emplace_back("generateDrops"); drops.push_back(a);
  };
  services.on_death = [&](std::uint32_t) { calls.emplace_back("onDeath"); };
  services.death_animation = [&](std::uint32_t) { calls.emplace_back("deathAnim"); };
  services.minotaur_death_remainder = [&](std::uint32_t) {
    calls.emplace_back("minoDeathUnknown");
  };
  services.filter_state = [&](std::uint32_t, std::uint32_t requested) -> std::optional<std::uint32_t> {
    calls.emplace_back("filter"); return requested;
  };
  services.animation_start = [&](std::uint32_t, const C::AnimationRequest &) {
    calls.emplace_back("blend");
  };
  {
    O::World rejected(O::CallbackPolicy::ReferenceBase);
    rejected.shared.state = 7;
    auto unavailable = services;
    unavailable.model_hide_mask = [](std::uint32_t, std::uint32_t) -> std::uint64_t {
      throw C::Unsupported("model_buildHideMask: result Unknown");
    };
    C::Runtime failed(rejected, resources, unavailable);
    refuses([&] {
      failed.construct(C::Kind::WarBoar, {0, 0, 0}, 0, {}, R::from_int(100));
    }, "unknown model service refusal");
    check(rejected.objects.empty(), "constructor refusal leaves no live World actor");
  }
  {
    O::World rejected(O::CallbackPolicy::ReferenceBase);
    auto unavailable = services;
    unavailable.minotaur_base_flags = {};
    C::Runtime failed(rejected, resources, unavailable);
    refuses([&] { failed.construct(C::Kind::Minotaur, {0, 0, 0}, 0, {},
                                    R::from_int(100)); },
            "missing base creature flags refuse Minotaur construction");
    check(rejected.objects.empty(), "Minotaur base refusal consumes no World block");
  }
  {
    O::World recycled(O::CallbackPolicy::ReferenceBase);
    auto lifecycle_services = services;
    lifecycle_services.set_def_entry_params = [](std::uint32_t, const C::EventParams &) {};
    std::vector<std::uint32_t> ticks;
    lifecycle_services.process_damage = [&](std::uint32_t address) {
      ticks.push_back(address);
    };
    lifecycle_services.creature_run = [](std::uint32_t) { return 1; };
    C::Runtime runtime(recycled, resources, lifecycle_services);
    auto &original = runtime.construct(C::Kind::WarBoar, {}, 0, {}, R::from_int(100));
    auto *original_object = original.object;
    const auto address = original_object->addr;
    recycled.destroy(*original_object);
    auto &replacement = runtime.construct(C::Kind::WarBoarSmall, {}, 0, {}, R::from_int(80));
    check(replacement.object->addr == address && replacement.object != original_object,
          "reconstruction reuses the pool address with a new object identity");
    check(runtime.find(address) == &replacement && &runtime.at(address) == &replacement,
          "reconstructed actor remains accessible through the runtime");
    check(runtime.actors().size() == 1 && replacement.kind == C::Kind::WarBoarSmall &&
          replacement.health() == R::from_int(80), "replacement owns its new actor state");
    runtime.tick(address);
    check(ticks == std::vector<std::uint32_t>({address}), "replacement dispatches its tick");
    recycled.destroy(*replacement.object);
    check(runtime.actors().empty() && runtime.find(address) == nullptr,
          "runtime retires destroyed actors on access");
    refuses([&] { runtime.at(address); }, "destroyed actor lookup refuses");
  }
  C::Runtime actors(world, resources, services);
  refuses([&] {
    actors.construct(C::Kind::WarBoar, {0, 0, 0}, 0,
        std::make_shared<std::vector<std::string>>(1, "hide=1"),
        R::from_int(100));
  }, "Creature_ctor property effects Unknown refusal");
  auto &large = actors.construct(C::Kind::WarBoar, {0, 0, 0}, 0, {}, R::from_int(100), 0);
  auto &small = actors.construct(C::Kind::WarBoarSmall, {R::from_int(2), 0, 0}, 0, {}, R::from_int(80), 0);
  auto &mino = actors.construct(C::Kind::Minotaur, {R::from_int(4), 0, 0}, 0, {}, R::from_int(200), 0);
  check(dynamic_cast<C::WarBoar *>(&large) &&
        dynamic_cast<C::WarBoarSmall *>(&small) &&
        dynamic_cast<C::Minotaur *>(&mino),
        "native actor class identities");
  check(hands == std::vector<std::array<std::uint32_t, 4>>({
        {mino.object->addr + 0x3c8, 1, 0x15, 0x20},
        {mino.object->addr + 0x3c8, 2, 0x19, 0x20}}),
        "r5 Minotaur holder and ordered hand setup");
  check(large.object->vtable == 0x517cf0 && mino.object->vtable == 0x5165f0,
        "round-7 vtable");
  check(large.object->heads[2].next && large.object->heads[1].next &&
        mino.object->heads[2].next && mino.object->heads[1].next,
        "round-7 same World grid/run links");
  check(large.object->radius == 0x41f00000 && small.object->radius == 0x41f00000,
        "r2 boar radius");
  check(mino.object->radius == 0x41c80000 &&
        mino.hp.mem.load(mino.object->addr + 0x1c, 4) == 0x41c80000 &&
        mino.hp.mem.load(mino.object->addr + 0xb8, 4) == 0 &&
        mino.hp.mem.load(mino.object->addr + 0xb0, 4) == 0x3f999999 &&
        mino.hp.mem.load(mino.object->addr + 0xb4, 1) == 0xb &&
        mino.hp.mem.load(mino.object->addr + 0xf4, 4) == 300 &&
        mino.hp.mem.load(mino.object->addr + 0xa8, 8) == UINT64_C(0x200040080) &&
        mino.hp.mem.load(mino.object->addr + 0x46c, 4) == 0,
        "Minotaur_ctor documented radius, speed, turn, kind, age, flags and charge fields");
  check(large.forward_offset == 0xc1700000 && small.forward_offset == 0xc1200000 &&
        large.scale == 0x3f400000 && small.scale == 0x3f199999 && small.small_marker == 1,
        "round-2 boar variants");
  check(large.object->class_def == small.object->class_def &&
        large.object->class_def->entries.size() == 20 &&
        mino.object->class_def->entries.size() == 26,
        "round-5 definition cache and 46 names");
  for (const auto &entry : large.object->class_def->entries)
    check(entry.second != 0, "round-7 boar named res_find result");
  for (const auto &entry : mino.object->class_def->entries)
    check(entry.second != 0, "round-7 Minotaur named res_find result");
  check(world.objects.at(large.object->addr) == large.object &&
        world.objects.at(mino.object->addr) == mino.object,
        "round-7 one World identity");
  check(large.health() == R::from_int(100) && mino.health() == R::from_int(200),
        "HP offset 0x228");
  check(large.hp.mem.load(large.object->addr + 0x48, 4) == 0x517cf0 &&
        large.hp.mem.load(large.object->addr + 0xe0, 4) == 0xc1700000 &&
        small.hp.mem.load(small.object->addr + 0x3d0, 1) == 1 &&
        mino.hp.mem.load(mino.object->addr + 0x48, 4) == 0x5165f0,
        "r2/r7 actor memory offsets");
  check(events.size() == 22 && events[0].entry == 4 &&
        events[0].masks == std::array<std::uint32_t,4>{1,2,0,0} &&
        events[0].times == std::array<R::Raw,4>{0x3e999999,0x3f333333,0,0} &&
        events[4].entry == 10 && events[4].times[1] == 0x3f666666 &&
        events[6].entry == 12 && events[6].masks[0] == 0x84 &&
        events[14].entry == 5 && events[15].entry == 6 &&
        events[15].times[1] == 0x3f199999 &&
        events[21].entry == 4 && events[21].times[0] == 0x3f199999,
        "r5 entry event arguments and order");
  check(events[3].entry == 7 && events[3].masks[1] == 2 && events[3].times[1] == 0x3f000000 &&
        events[10].entry == 7 && events[10].times[1] == 0x3f000000,
        "WBAI-01: state 9 continuation is 0.5 seconds for both variants");
  check(mino.hidden_parts == 0x1234, "Unknown mask comes only from service");
  // First constructor is linked into run60 slot 1. The scheduler invokes
  // the same address held by the grid and HP storage.
  world.frame = 1;
  calls.clear(); world.run_objects();
  check(calls == std::vector<std::string>({"damage", "run", "anim", "knockback", "target"}),
        "round-2 bounded tick order");
  check(large.hp.mem.load(large.object->addr + 0xf4, 4) == 0,
        "constructor deadline is zero, without an idle-entry grace period");
  for (const std::int64_t entry : {0, 100}) {
    large.hp.mem.store(large.object->addr + 0x300, 4, entry);
    actors.set_state(large.object->addr, 0);
    large.hp.mem.store(large.object->addr + 0x300, 4, entry + 180);
    actors.tick(large.object->addr);
    check(large.state == 0, "state0 stays through the 180th frame after entry");
    large.hp.mem.store(large.object->addr + 0x300, 4, entry + 181);
    actors.tick(large.object->addr);
    check(large.state == 1, "strict deadline expiry enters wander");
  }
  world.frame = 1;
  actors.set_state(large.object->addr, 0);
  sight = true; actors.tick(large.object->addr);
  check(large.state == 3, "documented force-0 sight activates chase");
  sight = false; actors.tick(large.object->addr);
  check(large.state == 0, "chase with no target returns to idle");
  actors.set_state(large.object->addr, 6);
  large.hp.mem.store(large.object->addr + 0x11a, 2, 8);
  check(large.state == 6 && large.hit_frame == 0 && large.last_attack == 6 &&
        large.hp.mem.load(large.object->addr + 0x232, 2) == 6 &&
        large.hp.mem.load(large.object->addr + 0x3c8, 4) == 0 &&
        large.hp.mem.load(large.object->addr + 0x3cc, 4) == 6,
        "r2 attack state initialization");
  calls.clear(); event_bits = 1;
  actors.tick(large.object->addr);
  check(large.hit_frame == 1 && calls.back() == "hit" &&
        large.hp.mem.load(large.object->addr + 0x3c8, 4) == 1,
        "r2 attack hit event");
  actors.tick(large.object->addr);
  check(large.hit_frame == 1 && std::count(calls.begin(), calls.end(), "hit") == 2,
        "supplied hit events are not suppressed by the turning latch");
  event_bits = 0;
  const auto hp_before_attacker = large.health();
  const auto hit_angle_before = large.hp.mem.load(large.object->addr + 0x2e0, 2);
  const auto owner_before = large.hp.mem.load(large.object->addr + 0x2e4, 4);
  refuses([&] { actors.take_damage(large.object->addr, small.object->addr, 1,
                                   R::from_int(5)); },
          "nonzero attacker requires shared World HP binding");
  check(large.health() == hp_before_attacker &&
        large.hp.mem.load(large.object->addr + 0x2e0, 2) == hit_angle_before &&
        large.hp.mem.load(large.object->addr + 0x2e4, 4) == owner_before,
        "refusal does not fabricate hit angle or owner attribution");
  actors.take_damage(large.object->addr, 0, 0x42000, R::ZERO);
  check(large.health() == R::from_int(100) && !large.dead(),
        "r7 zero-damage miss leaves HP unchanged");
  actors.take_damage(large.object->addr, 0, 1, R::from_int(101));
  check(large.dead() && large.health() == R::from_int(-1), "Creature_takeDamage HP subtraction");
  // creature-tick spec 12.1 with attacker 0: frame stamp, flag OR, pending
  // damage; the hit angle and owner handle are written only for an attacker.
  check(large.hp.mem.load(large.object->addr + 0x304, 4) == world.frame &&
        (large.hp.mem.load(large.object->addr + 0x2dc, 4) & 1) == 1 &&
        large.hp.mem.load(large.object->addr + 0xc8, 4) == R::from_int(101) &&
        large.hp.mem.load(large.object->addr + 0x2e0, 2) == hit_angle_before &&
        large.hp.mem.load(large.object->addr + 0x2e4, 4) == owner_before,
        "slot-2 attacker-0 effects match spec 12.1");
  const auto before_death_seed = large.object->seed;
  calls.clear();
  actors.tick(large.object->addr);
  check(large.state == 0x14 && large.death_reported && drops.size() == 1,
        "next-tick boar death and one loot call");
  check(large.hp.mem.load(large.object->addr + 0x232, 2) == 0x14,
        "r8 boar death state offset");
  check(calls == std::vector<std::string>({"damage", "run", "filter", "dropLoot",
                                         "generateDrops", "deathAnim", "onDeath"}) &&
        large.object->seed == before_death_seed * 0x19660dU + 0x3c6ef35fU,
        "r2/r8 death order and one own animation draw");
  check(drops[0].gold_chance == 0 && drops[0].z == 0x42100000 &&
        drops[0].gold_scale == 0x40333333 &&
        drops[0].quality_row == 0x40400000, "round-3/8 zero Gold boar call");
  actors.tick(large.object->addr);
  check(drops.size() == 1, "death loot once");
  actors.take_damage(mino.object->addr, 0, 1, R::from_int(201));
  refuses([&] { actors.tick(mino.object->addr); }, "Minotaur run Unknown refusal");
  actors.set_state(mino.object->addr, 0x14);
  check(mino.dead() && mino.state == 0x14 && drops.size() == 2 &&
        drops[1].gold_chance == 0x3f199999 &&
        mino.hp.mem.load(mino.object->addr + 0x232, 2) == 0x14,
        "round-4 Minotaur Gold-capable death");
  check(seeded_loot(drops[0], 20973) ==
            std::make_pair(std::vector<std::string>{"consumable"}, UINT64_C(10)) &&
        seeded_loot(drops[0], 7) ==
            std::make_pair(std::vector<std::string>{}, UINT64_C(7)),
        "r3 boar fresh-seed generator kinds and draws");
  check(seeded_loot(drops[1], 20973).first == std::vector<std::string>{"gold"} &&
        seeded_loot(drops[1], 7).first.empty(),
        "r4 Minotaur fresh-seed Gold request, not credited Gold");
  calls.clear(); actors.tick(small.object->addr);
  check(calls == std::vector<std::string>({"damage", "run", "anim", "knockback", "target"}),
        "WarBoarSmall shares r2 bounded active tick");
  actors.take_damage(small.object->addr, 0, 1, R::from_int(81));
  check(small.health() == R::from_int(-1), "WarBoarSmall slot-2 HP write");
  actors.tick(small.object->addr);
  check(small.dead() && small.death_reported && drops.size() == 3 &&
        drops.back().gold_chance == 0,
        "WarBoarSmall next-tick death uses boar zero-Gold call");
  const auto cached_model = large.object->class_def->entries.front().second;
  world.lump_generation = 2;
  auto &reload = actors.construct(C::Kind::WarBoar,
      {R::from_int(6), 0, 0}, 0, {}, R::from_int(90));
  check(reload.object->class_def == large.object->class_def &&
        reload.object->class_def->generation == 2 &&
        reload.object->class_def->entries.front().second != cached_model,
        "r5 named resource re-resolution on lump generation change");
  // The second deterministic seed is a separate constructor stream control.
  O::World seed7(O::CallbackPolicy::ReferenceBase);
  seed7.shared.state = 7;
  champions::resources::Manager resources7;
  resources7.load_lmp("warboar.lmp", {b.data(), b.size()});
  resources7.load_lmp("minotaur.lmp", {m.data(), m.size()});
  C::Runtime actors7(seed7, resources7, services);
  auto &on7 = actors7.construct(C::Kind::WarBoar, {0, 0, 0}, 0, {}, R::from_int(100));
  check(on7.object->seed != large.object->seed, "seed 7 own stream differs");
  seed7.frame = 7;
  auto &mino7 = actors7.construct(C::Kind::Minotaur,
      {R::from_int(4), 0, 0}, 0, {}, R::from_int(200));
  check(mino7.hp.mem.load(mino7.object->addr + 0xf4, 4) == 307 &&
        mino7.hp.mem.load(mino7.object->addr + 0xe8, 4) ==
            (1u << ((mino7.object->seed >> 16) & 1u)),
        "seed-7 Minotaur frame threshold and own-draw hidden part");
  R::combat::Tables tables;
  R::combat::World combat_world;
  combat_world.num_players = 1;
  combat_world.monster_level = R::from_int(22);
  combat_world.level_name = U"valor";
  auto &balanced = actors7.construct_balanced(C::Kind::WarBoarSmall,
      {R::from_int(10), 0, 0}, 0, {}, balance.data(), balance.size(),
      tables, combat_world, 0);
  check(balanced.stats.hp && balanced.health() == *balanced.stats.hp &&
        balanced.stats.max_hp && balanced.health() == *balanced.stats.max_hp,
        "Creature_setStats uses own draw and balance row");
  check(balanced.hp.mem.load(balanced.object->addr + 0xe4, 2) ==
            std::uint16_t(balanced.stats.armor) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd0, 2) ==
            std::uint16_t(balanced.stats.dmg_low) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd2, 2) ==
            std::uint16_t(balanced.stats.dmg_high) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd4, 2) ==
            std::uint16_t(balanced.stats.attack) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd8, 4) ==
            balanced.stats.exp_value,
        "Creature_setStats documented field offsets");
  // Idle timer under the second reference seed's World.
  on7.hp.mem.store(on7.object->addr + 0x300, 4, 7);
  actors7.set_state(on7.object->addr, 0);
  on7.hp.mem.store(on7.object->addr + 0x300, 4, 187);
  actors7.tick(on7.object->addr);
  check(on7.state == 0, "seed-7 idle equality");
  on7.hp.mem.store(on7.object->addr + 0x300, 4, 188);
  actors7.tick(on7.object->addr);
  check(on7.state == 1, "seed-7 strict idle expiry");
  seed7.frame = 7;
  // Death loot: only the ordinary rank-0, untinted call is modeled.
  for (const auto rank : {1, 2}) {
    auto &ranked = actors7.construct_balanced(C::Kind::WarBoar,
        {R::from_int(20 + rank), 0, 0}, 0, {}, balance.data(), balance.size(),
        tables, combat_world, rank);
    const auto drop_count = drops.size();
    calls.clear();
    refuses([&] { actors7.set_state(ranked.object->addr, 0x14); },
            "rank 1/2 Creature_dropLoot extra calls refuse");
    check(ranked.state == 0 && !ranked.death_reported && drops.size() == drop_count &&
          calls.empty() && ranked.hp.mem.load(ranked.object->addr + 0x232, 2) == 0,
          "ranked loot refusal leaves state, seed-bound services and loot untouched");
  }
  {
    auto &unknown_rank = actors7.construct(C::Kind::WarBoar,
        {R::from_int(30), 0, 0}, 0, {}, R::from_int(100));
    calls.clear();
    refuses([&] { actors7.set_state(unknown_rank.object->addr, 0x14); },
            "unknown creature_bossRank refuses death loot");
    check(unknown_rank.state == 0 && calls.empty(), "unknown rank leaves actor untouched");
    auto &tinted = actors7.construct(C::Kind::WarBoar,
        {R::from_int(31), 0, 0}, 0, {}, R::from_int(100), 0);
    tinted.hp.mem.store(tinted.object->addr + 0xa0, 1, 6);
    calls.clear();
    refuses([&] { actors7.set_state(tinted.object->addr, 0x14); },
            "tint preset scaling refuses death loot");
    check(tinted.state == 0 && calls.empty(), "tint refusal leaves actor untouched");
    const auto before = drops.size();
    auto &plain = actors7.construct_balanced(C::Kind::WarBoar,
        {R::from_int(32), 0, 0}, 0, {}, balance.data(), balance.size(),
        tables, combat_world, 0);
    actors7.set_state(plain.object->addr, 0x14);
    check(drops.size() == before + 1 &&
          seeded_loot(drops.back(), 7) ==
              std::make_pair(std::vector<std::string>{}, UINT64_C(7)) &&
          seeded_loot(drops.back(), 20973) ==
              std::make_pair(std::vector<std::string>{"consumable"}, UINT64_C(10)),
          "rank-0 balanced boar death makes the one pinned ordinary call");
  }
  // Minotaur_ctor (docs/monsters-2.md 4871-4882) against an independent model,
  // fresh World per reference seed. Shared draw -> object seed (16-bit), the
  // base setStats draw (balanced path only), then the class's own draw.
  const auto lcg = [](std::uint32_t x) { return x * 0x19660dU + 0x3c6ef35fU; };
  for (const std::uint32_t seed : {20973u, 7u})
    for (const bool balanced : {false, true}) {
      O::World fresh(O::CallbackPolicy::ReferenceBase);
      fresh.shared.state = seed;
      fresh.frame = 40 + seed % 11;
      champions::resources::Manager res;
      res.load_lmp("warboar.lmp", {b.data(), b.size()});
      res.load_lmp("minotaur.lmp", {m.data(), m.size()});
      auto svc = services;
      std::vector<std::uint32_t> variant_draws;
      std::vector<std::string> order;
      svc.minotaur_variant = [&](std::uint32_t, std::uint32_t draw) {
        variant_draws.push_back(draw); order.emplace_back("variant"); return draw & 1u;
      };
      svc.weapon_shield = [&](std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {
        order.emplace_back("hand");
      };
      svc.minotaur_initial_state = [&](std::uint32_t) { order.emplace_back("state0"); };
      svc.creature_init = [&](std::uint32_t) { order.emplace_back("init"); };
      svc.minotaur_base_flags = [](std::uint32_t) { return UINT64_C(0x1); };
      C::Runtime rt(fresh, res, svc);
      auto &mi = balanced
          ? rt.construct_balanced(C::Kind::Minotaur, {0, 0, 0}, 0, {}, balance.data(),
                                  balance.size(), tables, combat_world, 0)
          : rt.construct(C::Kind::Minotaur, {0, 0, 0}, 0, {}, R::from_int(200), 0);
      auto word = lcg(seed) >> 16;                 // GameObject seed
      if (balanced) word = lcg(word);              // setStats consumes one draw
      word = lcg(word);                            // class draw
      const auto own = word >> 16;
      const auto at = mi.object->addr;
      check(variant_draws == std::vector<std::uint32_t>{own} &&
            mi.object->seed == word,
            "Minotaur ctor makes exactly one own draw and hands it to the hide variant");
      check(mi.object->radius == 0x41c80000 && mi.hp.mem.load(at + 0x1c, 4) == 0x41c80000 &&
            mi.hp.mem.load(at + 0xb8, 4) == 0 &&
            mi.hp.mem.load(at + 0xb0, 4) == 0x3f999999 &&
            mi.hp.mem.load(at + 0xb4, 1) == 0xb &&
            mi.hp.mem.load(at + 0xf4, 4) == std::uint32_t(fresh.frame + 300) &&
            mi.hp.mem.load(at + 0xa8, 8) == (UINT64_C(0x1) | UINT64_C(0x200040000)) &&
            mi.hp.mem.load(at + 0x46c, 4) == 0 &&
            mi.hp.mem.load(at + 0xe8, 4) == (1u << (own & 1u)) &&
            mi.hp.mem.load(at + 0x48, 4) == 0x5165f0,
            "Minotaur_ctor radius 25, speed 0, turn 1.2, kind 0xb, age+300, flags, +0x46c, own-draw word");
      check(order == std::vector<std::string>({"hand", "hand", "variant", "state0", "init"}),
            "Minotaur ctor hand, own draw/hide mask, setState(0), init order");
    }
  return 0;
}
