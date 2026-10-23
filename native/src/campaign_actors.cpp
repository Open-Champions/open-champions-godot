#include "champions/campaign_actors.hpp"
#include <array>
#include <utility>

namespace champions::simulation::rta::campaign_actors {
namespace {
constexpr std::array<const char *, 20> BOAR_NAMES{{
    "warboar.vif", "warboar.tex", "warboar_b.tex", "atak_mid.anm",
    "atak_left_close.anm", "atak_front.anm", "atak_right_close.anm",
    "backup.anm", "charge.anm", "chargetak.anm", "death1.anm",
    "death2.anm", "hit2front.anm", "hit2left.anm", "hit2right.anm",
    "idle1.anm", "roar.anm", "walk.anm", "walk_trn_lft.anm",
    "walk_trn_ryt.anm"}};
constexpr std::array<const char *, 26> MINOTAUR_NAMES{{
    "minotaur.vif", "minotaur.tex", "atak_180.anm", "atak_back.anm",
    "atak_f1.anm", "atak_f2.anm", "atak_f3.anm", "atak_left.anm",
    "atak_right.anm", "block.anm", "death.anm", "death2.anm",
    "gore.anm", "hitreaction.anm", "idle1.anm", "idle2.anm",
    "run.anm", "taunt.anm", "taunt2.anm", "walk.anm", "axe.vif",
    "axe.tex", "shield.vif", "shield.tex", "shield2.vif", "shield2.tex"}};
// ref_campaign_combat_r5.EVENT_ARGUMENTS: call-site parameters, independent
// of archive contents or the still-unknown relocated clip pointers.
constexpr std::array<EventParams, 7> BOAR_EVENTS{{
    {4, {1,2,0,0}, {0x3e999999,0x3f333333,0,0}},
    {5, {1,2,0,0}, {0x3e999999,0x3f000000,0,0}},
    {6, {1,2,0,0}, {0x3e999999,0x3f333333,0,0}},
    {7, {1,2,0,0}, {0x3e999999,0x3f000000,0,0}},
    {10,{1,2,0,0}, {0x3f000000,0x3f666666,0,0}},
    {11,{0x84,0,0,0}, {ONE,0,0,0}},
    {12,{0x84,0,0,0}, {ONE,0,0,0}},
}};
constexpr std::array<EventParams, 8> MINOTAUR_EVENTS{{
    {5,{1,0,0,0}, {0x3e4ccccc,0,0,0}},
    {6,{1,1,0,0}, {0x3e4ccccc,0x3f199999,0,0}},
    {7,{1,0,0,0}, {0x3ecccccc,0,0,0}},
    {8,{1,4,0,0}, {0x3e4ccccc,0x3eb851eb,0,0}},
    {9,{1,0,0,0}, {0x3e4ccccc,0,0,0}},
    {13,{1,0,0,0}, {0x3e4ccccc,0,0,0}},
    {3,{1,0,0,0}, {0x3f199999,0,0,0}},
    {4,{1,0,0,0}, {0x3f199999,0,0,0}},
}};
template <std::size_t N>
std::shared_ptr<objects::ClassDef> definition(const char *lump,
                                               const std::array<const char *, N> &names) {
  std::vector<std::string> strings;
  strings.reserve(N);
  for (auto name : names) strings.emplace_back(name);
  return std::make_shared<objects::ClassDef>(lump, strings);
}
template <typename F> void required(const F &fn, const char *method) {
  if (!fn) throw Unsupported(std::string(method) + ": Unknown service");
}
std::uint16_t balance_hash(const std::string &name) {
  std::uint32_t h = 0;
  for (unsigned char c : name) {
    int v = c >= 128 ? int(c) - 256 : int(c);
    if (c >= 'A' && c <= 'Z') v += 32;
    h = ((h + v) & 0xffffu) * 0x401u & 0xffffu;
    h ^= h >> 6;
  }
  h = h * 9u & 0xffffu;
  return std::uint16_t(((h >> 11) ^ h) * 0x8001u);
}
} // namespace

std::array<Raw, 5> balance_lookup(const std::uint8_t *data, std::size_t size,
                                  const std::string &name) {
  if (!data || size < 2) throw Unsupported("balance_lookup: monsters.bal missing");
  const auto u16 = [&](std::size_t at) {
    return std::uint16_t(std::uint16_t(data[at]) | (std::uint16_t(data[at + 1]) << 8));
  };
  const auto count = u16(0);
  if (count > (size - 2) / 12)
    throw Unsupported("balance_lookup: truncated monsters.bal");
  for (std::size_t row = 0; row < count; ++row) {
    const auto at = 2 + 12 * row;
    if (u16(at) != balance_hash(name)) continue;
    std::array<Raw, 5> result{};
    for (unsigned i = 0; i < 5; ++i)
      result[i] = mul(from_int(std::int16_t(u16(at + 2 + 2 * i))), 0x3c23d70a);
    return result;
  }
  // Observed Creature_ctor fallback for a type absent from monsters.bal.
  return {ONE, ONE, ONE, ONE, ONE};
}

Actor::Actor(Kind k, objects::GameObject *o) : kind(k), object(o) {}
Raw Actor::health() const { return Raw(hp.mem.load(object->addr + 0x228, 4)); }
bool Actor::dead() const { return !lt(ZERO, health()); }

Runtime::Runtime(objects::World &world, resources::Manager &resources, Services services)
    : world_(world), resources_(resources), services_(std::move(services)),
      boar_def_(definition("warboar.lmp", BOAR_NAMES)),
      minotaur_def_(definition("minotaur.lmp", MINOTAUR_NAMES)) {
  // This bounded runtime owns dispatch for the supplied isolated World.
  if (world_.on_run || world_.res_find)
    throw Unsupported("constructor: World already has actor or resource dispatch");
  world_.res_find = [this](const std::string &lump, const std::string &entry) {
    return resolve(lump, entry);
  };
  world_.on_run = [this](objects::GameObject &o) { tick(o.addr); };
}
Runtime::~Runtime() {
  world_.on_run = {};
  world_.res_find = {};
}

std::uint32_t Runtime::resolve(const std::string &lump, const std::string &entry) {
  auto handle = resources_.find(lump, entry);
  if (!handle || !resources_.bytes(*handle))
    throw Unsupported("GameObject_ctor/res_find: named resource unavailable");
  handles_.push_back(*handle);
  // Native opaque IDs are scoped to this Runtime; they are never game pointers.
  return std::uint32_t(handles_.size());
}

Actor &Runtime::construct(Kind kind, const objects::Vec3 &pos,
                          std::uint32_t angle, objects::Props props, Raw initial_hp) {
  return construct_impl(kind, pos, angle, std::move(props), initial_hp, {});
}

Actor &Runtime::construct_impl(Kind kind, const objects::Vec3 &pos,
                              std::uint32_t angle, objects::Props props,
                              Raw initial_hp,
                              const std::function<void(Actor &)> &base_stats) {
  if (props && !props->empty())
    throw Unsupported("Creature_ctor/properties: complete base effects Unknown");
  required(services_.model_hide_mask, "constructor/model_buildHideMask");
  required(services_.animation_init, "constructor/anim_clear_push");
  required(services_.set_def_entry_params, "constructor/creature_setDefEntryParams");
  required(services_.creature_init, "constructor/Creature_init");
  if (kind == Kind::Minotaur) {
    required(services_.weapon_shield, "Minotaur_ctor/WeaponShield");
    required(services_.minotaur_variant, "Minotaur_ctor/hide variant Unknown");
    required(services_.minotaur_initial_state, "Minotaur_ctor/setState(0)");
  }
  auto def = kind == Kind::Minotaur ? minotaur_def_ : boar_def_;
  // Preflight avoids consuming a pool block for a missing entry. The actual
  // GameObject_ctor still resolves by name, in definition order, on generation
  // change. Resource decoding/animation upload is a separate Unknown service.
  for (const auto &entry : def->entries) {
    auto h = resources_.find(def->lump, entry.first);
    if (!h || !resources_.bytes(*h))
      throw Unsupported("constructor: named definition resource unavailable");
  }
  auto *o = world_.new_object(kind == Kind::Minotaur ? 0x470 : 0x3d8,
                              pos, def, std::move(props));
  if (!o) throw Unsupported("GameObject_ctor: object pool exhausted");
  try {
    o->angle = angle;
    o->class_name = kind == Kind::Minotaur ? "Minotaur" :
                    kind == Kind::WarBoarSmall ? "WarBoarSmall" : "WarBoar";
    o->type_name = *o->class_name;
    std::unique_ptr<Actor> actor;
    if (kind == Kind::WarBoarSmall)
      actor = std::make_unique<WarBoarSmall>(o);
    else if (kind == Kind::WarBoar)
      actor = std::make_unique<WarBoar>(o);
    else
      actor = std::make_unique<Minotaur>(o);
    // Creature_ctor follows GameObject_ctor and performs its own HP roll before
    // the class constructor's Minotaur variant draw.
    actor->hp.mem.store(o->addr + 0x228, 4, initial_hp);
    actor->hp.mem.store(o->addr + 0x22c, 4, initial_hp);
    actor->hp.mem.store(o->addr + 0x10, 4, pos[0]);
    actor->hp.mem.store(o->addr + 0x14, 4, pos[1]);
    if (base_stats) base_stats(*actor);
    o->vtable = kind == Kind::Minotaur ? 0x5165f0 : 0x517cf0;
    if (kind != Kind::Minotaur) {
      o->radius = 0x41f00000;
      o->draw_half_width = 40;
    }
    actor->hp.mem.store(o->addr + 0x48, 4, o->vtable);
    actor->hp.mem.store(o->addr + 0x1c, 4, o->radius);
    actor->hp.mem.store(o->addr + 0x28, 1, o->draw_half_width);
    if (kind != Kind::Minotaur) {
      actor->creature_flags = UINT64_C(0x200040000);
      actor->turn_rate = ONE;
      actor->kind_byte = 3;
      actor->forward_offset = kind == Kind::WarBoarSmall ? 0xc1200000 : 0xc1700000;
      actor->scale = kind == Kind::WarBoarSmall ? 0x3f199999 : 0x3f400000;
      actor->small_marker = kind == Kind::WarBoarSmall ? 1 : 0;
      actor->hp.mem.store(o->addr + 0xb8, 4, 0);
      actor->hp.mem.store(o->addr + 0xb0, 4, ONE);
      actor->hp.mem.store(o->addr + 0x5c, 4, 0xbe4ccccc);
      actor->hp.mem.store(o->addr + 0xe0, 4, actor->forward_offset);
      actor->hp.mem.store(o->addr + 0xdc, 4, actor->scale);
      actor->hp.mem.store(o->addr + 0x3d0, 1, actor->small_marker);
      actor->hp.mem.store(o->addr + 0xec, 4, 0);
      actor->hp.mem.store(o->addr + 0xe8, 4, 0);
      actor->hp.mem.store(o->addr + 0x220, 8, 0);
    } else {
      services_.weapon_shield(o->addr + 0x3c8, 1, 0x15, 0x20);
      services_.weapon_shield(o->addr + 0x3c8, 2, 0x19, 0x20);
    }
    if (kind != Kind::Minotaur) {
      actor->hp.mem.store(o->addr + 0xb0, 4, actor->turn_rate);
      actor->hp.mem.store(o->addr + 0xb4, 1, actor->kind_byte);
      actor->hp.mem.store(o->addr + 0xa8, 8, actor->creature_flags);
    }
    const auto variant = kind == Kind::Minotaur
        ? services_.minotaur_variant(o->addr, o->rand()) : 0u;
    actor->hidden_parts = services_.model_hide_mask(def->entries.at(0).second, variant);
    services_.animation_init(o->addr, kind != Kind::Minotaur);
    if (kind == Kind::Minotaur)
      services_.minotaur_initial_state(o->addr);
    actor->hp.callbacks["handle_clear"] = [this](creature_hp::Model &, const std::vector<std::uint32_t> &a) {
      required(services_.handle_clear, "Creature_takeDamage/handle_clear");
      services_.handle_clear(a.at(0));
    };
    actor->hp.callbacks["handle_link"] = [this](creature_hp::Model &, const std::vector<std::uint32_t> &a) {
      required(services_.handle_link, "Creature_takeDamage/handle_link");
      services_.handle_link(a.at(0), a.at(1));
    };
    world_.run_every_60(*o);
    if (kind == Kind::Minotaur) {
      for (const auto &p : MINOTAUR_EVENTS)
        services_.set_def_entry_params(o->addr, p);
    } else {
      for (const auto &p : BOAR_EVENTS)
        services_.set_def_entry_params(o->addr, p);
    }
    services_.creature_init(o->addr);
    const auto address = o->addr;
    auto *result = actor.get();
    actors_.emplace(address, std::move(actor));
    return *result;
  } catch (...) {
    // An unavailable downstream model/animation/terrain service must not
    // leave a live grid object with no actor dispatch record.
    world_.destroy(*o);
    throw;
  }
}

Actor &Runtime::construct_balanced(Kind kind, const objects::Vec3 &pos,
                                   std::uint32_t angle, objects::Props props,
                                   const std::uint8_t *balance,
                                   std::size_t balance_size,
                                   const combat::Tables &tables,
                                   const combat::World &combat_world,
                                   std::int64_t rank) {
  const std::string name = kind == Kind::Minotaur ? "Minotaur" :
                           kind == Kind::WarBoarSmall ? "WarBoarSmall" : "WarBoar";
  const auto pct = balance_lookup(balance, balance_size, name);
  // The class call site supplies 10.0 to Creature_ctor. For a non-null type,
  // Creature_setStats replaces that placeholder before class-owned draws.
  return construct_impl(kind, pos, angle, std::move(props), 0x41200000,
      [&](Actor &a) {
        a.stats = combat::monster_stats(tables, combat_world, pct, rank,
            std::u32string(name.begin(), name.end()), true, a.object->rand());
        if (!a.stats.hp || !a.stats.max_hp)
          throw Unsupported("Creature_setStats: HP result missing");
        const auto at = a.object->addr;
        a.hp.mem.store(at + 0x228, 4, *a.stats.hp);
        a.hp.mem.store(at + 0x22c, 4, *a.stats.max_hp);
        a.hp.mem.store(at + 0xe4, 2, std::uint16_t(a.stats.armor));
        a.hp.mem.store(at + 0xd0, 2, std::uint16_t(a.stats.dmg_low));
        a.hp.mem.store(at + 0xd2, 2, std::uint16_t(a.stats.dmg_high));
        a.hp.mem.store(at + 0xd4, 2, std::uint16_t(a.stats.attack));
        a.hp.mem.store(at + 0xd8, 4, a.stats.exp_value);
      });
}

Actor &Runtime::at(std::uint32_t address) {
  auto it = actors_.find(address);
  if (it == actors_.end() || !it->second->object->alive)
    throw Unsupported("actor address is not live in this World");
  return *it->second;
}

void Runtime::set_state(Actor &a, std::uint32_t state) {
  if (state == 0x14 && a.death_reported) return;
  if (state == 0x14) {
    if (a.kind != Kind::Minotaur) {
      required(services_.filter_state, "WarBoar_setState/Creature_filterState Unknown");
      required(services_.death_animation, "WarBoar_setState/death animation Unknown");
      if (!services_.filter_state(a.object->addr, state)) return;
    }
    required(services_.loot_eligible, "Creature_dropLoot/eligibility Unknown");
    required(services_.loot_generate, "loot_generateDrops service Unknown");
    if (a.kind == Kind::Minotaur)
      required(services_.minotaur_death_remainder,
               "Minotaur_setState/death remainder Unknown");
    else
      required(services_.on_death, "Creature_onDeath service Unknown");
    const LootArgs args = a.kind == Kind::Minotaur
        ? LootArgs{0x42100000, 0x40000000, 0x3fcccccc, 0x3f199999, 0x3fb33333, 0x3e800000}
        : LootArgs{0x42100000, 0x40000000, 0x40333333, ZERO, 0x40400000, 0x3e800000};
    a.state = state;
    a.hp.mem.store(a.object->addr + 0x232, 2, state);
    if (services_.loot_eligible(a.object->addr))
      services_.loot_generate(a.object->addr, args);
    if (a.kind == Kind::Minotaur) {
      services_.minotaur_death_remainder(a.object->addr);
    } else {
      a.object->rand(); // observed boar death-animation choice
      services_.death_animation(a.object->addr);
      services_.on_death(a.object->addr);
    }
    a.death_reported = true;
    return;
  }
  if (a.kind == Kind::Minotaur)
    throw Unsupported("Minotaur_setState: state service Unknown");
  if (a.kind != Kind::Minotaur && (state < 6 || state > 10))
    throw Unsupported("WarBoar_setState: unexecuted state Unknown");
  if (a.kind != Kind::Minotaur && state >= 6 && state <= 10) {
    required(services_.filter_state, "WarBoar_setState/Creature_filterState Unknown");
    required(services_.animation_blend, "WarBoar_setState/animation blend Unknown");
    if (!services_.filter_state(a.object->addr, state)) return;
    a.hit_frame = 0;
    a.last_attack = state;
    a.hp.mem.store(a.object->addr + 0x3c8, 4, 0);
    a.hp.mem.store(a.object->addr + 0x3cc, 4, state);
    services_.animation_blend(a.object->addr, state);
  }
  a.state = state;
  a.hp.mem.store(a.object->addr + 0x232, 2, state);
}

void Runtime::tick_boar(Actor &a, const TickInput &in) {
  switch (a.state) {
  case 0:
    if (in.has_target || in.away_home)
      throw Unsupported("WarBoar_run/state0: sight or home transition Unknown");
    return;
  case 3:
    throw Unsupported("WarBoar_run/state3: transition target Unknown");
  case 6:
    if (in.hit_event && !a.hit_frame) {
      required(services_.hit_all_in_arc, "WarBoar_run/state6 hitAllEnemiesInArc");
      services_.hit_all_in_arc(a.object->addr, 8);
      a.hit_frame = 1;
      a.hp.mem.store(a.object->addr + 0x3c8, 4, 1);
    }
    if (in.next_event || in.animation_over)
      throw Unsupported("WarBoar_run/state6: target-dependent next state Unknown");
    return;
  case 0x14:
    required(services_.dead_update, "WarBoar_run/deadUpdate");
    services_.dead_update(a.object->addr);
    return;
  default:
    throw Unsupported("WarBoar_run: state service Unknown");
  }
}

void Runtime::tick(std::uint32_t address) {
  auto &a = at(address);
  if (a.kind == Kind::Minotaur)
    throw Unsupported("Minotaur_run: Unknown in campaign-combat rounds 2-8");
  required(services_.process_damage, "Creature_processDamage");
  required(services_.creature_run, "Creature_run");
  required(services_.tick_input, "class run input");
  services_.process_damage(address);
  services_.creature_run(address);
  if (a.dead() && !a.death_reported) { set_state(a, 0x14); return; }
  const auto in = services_.tick_input(address);
  if (!in.active) return;
  required(services_.animation_step, "Creature_stepAnim");
  required(services_.knockback, "Creature_applyKnockback");
  required(services_.pick_target, "Creature_pickTarget");
  services_.animation_step(address);
  services_.knockback(address);
  services_.pick_target(address);
  tick_boar(a, in);
}

void Runtime::set_state(std::uint32_t address, std::uint32_t state) {
  set_state(at(address), state);
}

void Runtime::take_damage(std::uint32_t address, std::uint32_t attacker,
                          std::uint32_t flags, Raw amount) {
  auto &a = at(address);
  a.hp.mem.store(creature_hp::G_SIM_FRAME, 4, std::uint32_t(world_.frame));
  a.hp.take_damage(address, attacker, flags, amount);
}
} // namespace champions::simulation::rta::campaign_actors
