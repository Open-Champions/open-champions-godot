#include "champions/blood1_actors.hpp"
#include <array>
#include <utility>

namespace champions::simulation::rta::blood1_actors {
namespace {
constexpr std::array<const char *, 22> KOBOLD_NAMES{{
    "kobold.vif", "kobold.tex", "attack1.anm", "attack2.anm",
    "attack3.anm", "attack4.anm", "attack5.anm", "death1.anm",
    "death2.anm", "death3.anm", "death4.anm", "evade.anm",
    "evade_lft.anm", "evade_ryt.anm", "hitreact.anm",
    "hitreact2.anm", "knockdown.anm", "idle1.anm", "idle2.anm",
    "run.anm", "spell1.anm", "walk.anm"}};
constexpr std::array<const char *, 18> WRAITH_NAMES{{
    "warwraith.vif", "warwraith.tex", "ataklong.anm",
    "atakswd1.anm", "atakswd2.anm", "death.anm",
    "deathtouch.anm", "fly.anm", "forward.anm",
    "hitreaction.anm", "idle_axcythe.anm", "idleweap.anm",
    "cythe.vif", "cythe.tex", "dhaxe.vif", "dhaxe.tex",
    "dhsword1.vif", "dhsword1.tex"}};
constexpr std::array<std::uint32_t, 11> KOBOLD_EVENTS{{3,4,5,6,7,21,17,8,9,10,11}};
constexpr std::array<std::uint32_t, 4> WRAITH_EVENTS{{3,4,5,7}};
constexpr LootArgs KOBOLD_LOOT{0x42100000,0x40000000,0x3fcccccc,
                                0x3e999999,0x3fb33333,0x3df5c28f};
constexpr LootArgs WRAITH_LOOT{0x42100000,0x40000000,0x40333333,
                                0,0x40400000,0x3e800000};
template <std::size_t N>
std::shared_ptr<objects::ClassDef> definition(const char *lump,
                                               const std::array<const char *, N> &names) {
  return std::make_shared<objects::ClassDef>(lump,
      std::vector<std::string>(names.begin(), names.end()));
}
template <typename F> void required(const F &fn, const char *method) {
  if (!fn) throw Unsupported(std::string(method) + ": Unknown service");
}
const char *type_name(Kind k) {
  switch (k) {
  case Kind::Kobold: return "Kobold";
  case Kind::KoboldTough: return "Koboldtough";
  case Kind::KoboldShaman: return "koboldshaman";
  case Kind::WarWraith: return "WarWraith";
  }
  throw Unsupported("constructor: Unknown class");
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
  return {ONE, ONE, ONE, ONE, ONE}; // Creature_ctor absent-row fallback.
}

Raw Actor::health() const { return Raw(hp.mem.load(object->addr + 0x228, 4)); }
bool Actor::dead() const { return !lt(ZERO, health()); }

Runtime::Runtime(objects::World &world, resources::Manager &resources, Services services)
    : world_(world), resources_(resources), services_(std::move(services)),
      kobold_def_(definition("kobold.lmp", KOBOLD_NAMES)),
      wraith_def_(definition("wwraith.lmp", WRAITH_NAMES)) {
  if (world_.on_run || world_.res_find)
    throw Unsupported("constructor: World already has actor or resource dispatch");
  world_.res_find = [this](const std::string &lump, const std::string &entry) {
    return resolve(lump, entry);
  };
  world_.on_run = [this](objects::GameObject &o) { tick(o.addr); };
}
Runtime::~Runtime() { world_.on_run = {}; world_.res_find = {}; }

std::uint32_t Runtime::resolve(const std::string &lump, const std::string &entry) {
  auto handle = resources_.find(lump, entry);
  if (!handle || !resources_.bytes(*handle))
    throw Unsupported("GameObject_ctor/res_find: named resource unavailable");
  handles_.push_back(*handle);
  return std::uint32_t(handles_.size()); // Native opaque ID, not a game pointer.
}

Actor &Runtime::construct(Kind kind, const objects::Vec3 &pos,
                          std::uint32_t angle, objects::Props props, Raw initial_hp) {
  return construct_impl(kind, pos, angle, std::move(props), initial_hp, {});
}
Actor &Runtime::construct_impl(Kind kind, const objects::Vec3 &pos,
                               std::uint32_t angle, objects::Props props, Raw initial_hp,
                               const std::function<void(Actor &)> &base_stats) {
  if (props && !props->empty())
    throw Unsupported("Creature_ctor/properties: complete base effects Unknown");
  required(services_.model_init, "constructor/model setup");
  required(services_.animation_init, "constructor/animation setup");
  required(services_.event_setup, "constructor/event setup");
  required(services_.creature_init, "constructor/Creature_init");
  if (kind == Kind::WarWraith)
    required(services_.holder_set_part, "WarWraith_ctor/Weapon1 holder");
  auto def = kind == Kind::WarWraith ? wraith_def_ : kobold_def_;
  for (const auto &entry : def->entries) {
    auto handle = resources_.find(def->lump, entry.first);
    if (!handle || !resources_.bytes(*handle))
      throw Unsupported("constructor: named definition resource unavailable");
  }
  auto *o = world_.new_object(kind == Kind::WarWraith ? 0x468 : 0x3d0,
                              pos, def, std::move(props));
  if (!o) throw Unsupported("GameObject_ctor: object pool exhausted");
  try {
    o->angle = angle;
    o->class_name = type_name(kind);
    o->type_name = *o->class_name;
    std::unique_ptr<Actor> actor;
    switch (kind) {
    case Kind::Kobold: actor = std::make_unique<Kobold>(o); break;
    case Kind::KoboldTough: actor = std::make_unique<KoboldTough>(o); break;
    case Kind::KoboldShaman: actor = std::make_unique<KoboldShaman>(o); break;
    case Kind::WarWraith: actor = std::make_unique<WarWraith>(o); break;
    }
    auto &m = actor->hp.mem;
    m.store(o->addr + 0x228, 4, initial_hp);
    m.store(o->addr + 0x22c, 4, initial_hp);
    m.store(o->addr + 0x10, 4, pos[0]);
    m.store(o->addr + 0x14, 4, pos[1]);
    if (base_stats) base_stats(*actor);
    o->vtable = kind == Kind::WarWraith ? 0x517f80 : 0x515ef0;
    o->radius = kind == Kind::Kobold ? 0x41400000 :
                kind == Kind::KoboldTough ? 0x41a00000 : 0x41900000;
    m.store(o->addr + 0x48, 4, o->vtable);
    m.store(o->addr + 0x1c, 4, o->radius);
    if (kind == Kind::WarWraith) {
      actor->weapon_part = ((o->rand() % 3) * 2) + 13;
      m.store(o->addr + 0x3c8, 1, 100);
      m.store(o->addr + 0xb4, 1, 0x10);
      m.store(o->addr + 0xb8, 4, 0x430c0000); // 140
      m.store(o->addr + 0x3c0, 4, 0x44160000); // 600
      m.store(o->addr + 0x3d0, 4, 0x50cf40);
      m.store(o->addr + 0x50, 4, o->addr + 0x3d0);
      services_.holder_set_part(o->addr + 0x3d0, actor->weapon_part);
    } else {
      actor->variant = kind == Kind::KoboldTough ? 1 :
                       kind == Kind::KoboldShaman ? 2 : 0;
      o->draw_half_width = 0x1e;
      m.store(o->addr + 0x28, 1, 0x1e);
      m.store(o->addr + 0xb4, 1, 9);
      m.store(o->addr + 0x3cc, 1, actor->variant);
      m.store(o->addr + 0xe8, 4, actor->variant);
      if (kind != Kind::Kobold)
        m.store(o->addr + 0xdc, 4, kind == Kind::KoboldTough ? 0x3fc00000 : 0x3f99999a);
    }
    services_.model_init(o->addr);
    services_.animation_init(o->addr);
    world_.run_every_60(*o);
    if (kind == Kind::WarWraith) {
      for (auto event : WRAITH_EVENTS) services_.event_setup(o->addr, event);
    } else {
      for (auto event : KOBOLD_EVENTS) services_.event_setup(o->addr, event);
    }
    services_.creature_init(o->addr);
    actor->hp.callbacks["handle_clear"] = [this](creature_hp::Model &, const std::vector<std::uint32_t> &a) {
      required(services_.handle_clear, "Creature_takeDamage/handle_clear");
      services_.handle_clear(a.at(0));
    };
    actor->hp.callbacks["handle_link"] = [this](creature_hp::Model &, const std::vector<std::uint32_t> &a) {
      required(services_.handle_link, "Creature_takeDamage/handle_link");
      services_.handle_link(a.at(0), a.at(1));
    };
    auto *result = actor.get();
    actors_.emplace(o->addr, std::move(actor));
    return *result;
  } catch (...) {
    world_.destroy(*o);
    throw;
  }
}

Actor &Runtime::construct_balanced(Kind kind, const objects::Vec3 &pos,
                                   std::uint32_t angle, objects::Props props,
                                   const std::uint8_t *balance, std::size_t balance_size,
                                   const combat::Tables &tables,
                                   const combat::World &combat_world,
                                   std::int64_t rank) {
  const std::string name = type_name(kind);
  const auto pct = balance_lookup(balance, balance_size, name);
  return construct_impl(kind, pos, angle, std::move(props),
      kind == Kind::WarWraith ? 0x40800000 : 0x41200000,
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

void Runtime::set_state(std::uint32_t address, std::uint32_t state) {
  auto &a = at(address);
  if (state != 0x14)
    throw Unsupported("class setState: non-death transition Unknown");
  if (a.death_reported) return;
  if (a.kind == Kind::WarWraith)
    required(services_.loot_generate, "loot_generateDrops");
  else
    required(services_.drop_loot, "Creature_dropLoot");
  required(services_.death_remainder, "class setState/death remainder");
  if (a.kind == Kind::WarWraith)
    required(services_.holder_drop, "WarWraith_setState/holder part drop");
  a.state = state;
  a.hp.mem.store(address + 0x232, 2, state);
  if (a.kind == Kind::WarWraith) {
    services_.holder_drop(address + 0x3d0);
    services_.loot_generate(address, WRAITH_LOOT);
  } else {
    services_.drop_loot(address, KOBOLD_LOOT);
  }
  services_.death_remainder(address);
  a.death_reported = true;
}

void Runtime::tick(std::uint32_t address) {
  auto &a = at(address);
  required(services_.process_damage, "Creature_processDamage");
  required(services_.creature_run, "Creature_run");
  required(services_.tick_input, "Creature_updateActive");
  services_.process_damage(address);
  services_.creature_run(address);
  const auto in = services_.tick_input(address);
  if (!in.active) return;
  required(services_.animation_step, "Creature_stepAnim");
  required(services_.knockback, "Creature_applyKnockback");
  if (a.kind == Kind::WarWraith) {
    required(services_.holder_update, "WarWraith_run/holder update");
    required(services_.turn_target, "Creature_turnToTarget");
    services_.holder_update(address + 0x3d0);
    services_.animation_step(address);
    if (a.state != 0x14) {
      required(services_.pick_target, "Creature_pickTarget");
      services_.pick_target(address);
    }
    services_.knockback(address);
    services_.turn_target(address);
  } else {
    required(services_.pick_target, "Creature_pickTarget");
    required(services_.pick_ally, "Kobold_run/pickAlly");
    services_.animation_step(address);
    services_.knockback(address);
    services_.pick_target(address);
    services_.pick_ally(address);
  }
  switch (a.state) {
  case 0:
    if (in.has_target || in.away_home)
      throw Unsupported("class run/state0: target or home transition Unknown");
    required(services_.can_see_target, "class run/state0 sight check");
    services_.can_see_target(address);
    if (a.kind != Kind::WarWraith) {
      required(services_.away_home_probe, "Kobold_run/state0 home check");
      services_.away_home_probe(address);
    }
    break;
  case 3:
    if (in.has_target)
      throw Unsupported("class run/state3: target decision Unknown");
    required(services_.state_change, "class run/state3 transition");
    services_.state_change(address);
    break;
  case 6:
  case 7:
    if (a.state == 7 && in.has_target)
      throw Unsupported("class run/state7 targeted attack or drain Unknown");
    if (a.kind != Kind::WarWraith) {
      required(services_.settle, "Kobold_run/settle");
      services_.settle(address);
      if (in.has_target && in.hit_event) {
        required(services_.hit_target_in_arc, "Creature_hitTargetInArc");
        required(services_.turn_target, "Creature_turnToTarget");
        required(services_.state_change, "Kobold_run/targeted setState");
        services_.hit_target_in_arc(address,
            a.kind == Kind::KoboldTough ? 0x440008 : 8);
        services_.turn_target(address);
        services_.state_change(address);
        return;
      }
    } else if (in.has_target && in.hit_event) {
      required(services_.holder_descriptor, "WarWraith_run/holder descriptor");
      required(services_.hit_target_at_reach, "Creature_hitTargetAtReach");
      required(services_.grid_update, "WarWraith_run/obj_updateGridCell");
      required(services_.state_change, "WarWraith_run/targeted setState");
      services_.holder_descriptor(address + 0x3d0);
      services_.hit_target_at_reach(address, 8);
      services_.turn_target(address);
      services_.grid_update(address);
      services_.state_change(address);
      return;
    }
    if (in.has_target || in.animation_over)
      throw Unsupported("class run/attack follow-up Unknown");
    required(services_.state_change, "class run/attack no-target transition");
    services_.state_change(address);
    break;
  case 0x14:
    if (a.kind == Kind::WarWraith) {
      if (a.hp.mem.load(address + 0x3c8, 1) != 100)
        throw Unsupported("WarWraith_run/dead fade and particle path Unknown");
      required(services_.delete_object, "WarWraith_run/fade deletion");
      services_.delete_object(address);
    } else {
      required(services_.move_by, "Kobold_run/dead move");
      required(services_.dead_update, "Kobold_run/deadUpdate");
      services_.move_by(address);
      services_.dead_update(address);
    }
    break;
  default:
    throw Unsupported("class run: state or AI branch Unknown");
  }
}

void Runtime::take_damage(std::uint32_t address, std::uint32_t attacker,
                          std::uint32_t flags, Raw amount) {
  auto &a = at(address);
  // This isolated HP model has no World-backed attacker bytes. The committed
  // blood1 probe covers only the null-attacker branch.
  if (attacker)
    throw Unsupported("Creature_takeDamage: nonzero attacker Unknown");
  a.hp.mem.store(creature_hp::G_SIM_FRAME, 4, std::uint32_t(world_.frame));
  a.hp.take_damage(address, attacker, flags, amount);
}
} // namespace champions::simulation::rta::blood1_actors
