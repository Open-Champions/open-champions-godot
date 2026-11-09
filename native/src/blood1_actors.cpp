#include "champions/blood1_actors.hpp"
#include "champions/creature_ai_m1_3.hpp"
#include <algorithm>
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
  if (world_.on_run || world_.on_destroy || world_.res_find)
    throw Unsupported("constructor: World already has actor or resource dispatch");
  world_.res_find = [this](const std::string &lump, const std::string &entry) {
    return resolve(lump, entry);
  };
  world_.on_run = [this](objects::GameObject &o) { tick(o.addr); };
  world_.on_destroy = [this](objects::GameObject &o, std::int64_t flags) {
    const auto address = o.addr;
    const auto serial = o.serial;
    world_.destroy(o, flags);
    if (!(flags & 1)) return;
    auto it = actors_.find(address);
    if (it != actors_.end() && it->second->object_serial == serial)
      actors_.erase(it);
  };
}
Runtime::~Runtime() {
  world_.on_run = {};
  world_.on_destroy = {};
  world_.res_find = {};
}

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
    retire_stale(o->addr);
    if (actors_.count(o->addr))
      throw Unsupported("GameObject_ctor: live actor address collision");
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
    auto inserted = actors_.emplace(o->addr, std::move(actor));
    if (!inserted.second)
      throw Unsupported("GameObject_ctor: actor insertion failed");
    return *inserted.first->second;
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

void Runtime::retire_stale(std::uint32_t address) {
  auto it = actors_.find(address);
  if (it == actors_.end()) return;
  const auto current = world_.objects.find(address);
  const auto *old = it->second->object;
  if (!old->alive || old->serial != it->second->object_serial ||
      current == world_.objects.end() || current->second != old ||
      current->second->serial != it->second->object_serial)
    actors_.erase(it);
}

Actor &Runtime::at(std::uint32_t address) {
  retire_stale(address);
  auto it = actors_.find(address);
  if (it == actors_.end())
    throw Unsupported("actor address is not live in this World");
  return *it->second;
}

namespace ai = creature_ai_m1_3;
void Runtime::enter(Actor &a, std::uint32_t requested) {
  const auto address = a.object->addr;
  // Only Kobold setState(8) (+0x3c8 = 8, MOD:1165; research-campaign-combat
  // fbb2e1d), executed from a live state-3 rig (DT:410). WarWraith 8 has no
  // executed row: the kernel refuses it (CAI13-3).
  if (a.kind == Kind::WarWraith && requested == 8) {
    try { ai::set_state(ai::Class::WarWraith, {}, requested); }
    catch (const ai::Unsupported &e) { throw Unsupported(std::string("class setState: ") + e.what()); }
  }
  if (a.kind == Kind::WarWraith || requested != 8)
    throw Unsupported("class setState: non-death transition Unknown");
  if (a.death_reported || a.state == 0x14)
    throw Unsupported("class setState: request on a dead actor Unknown");
  auto &m = a.hp.mem;
  ai::SetContext sc;
  sc.target = m.load(address + 0x234, 4) != 0;
  sc.root = std::uint16_t(m.load(address + 0x314, 2));
  sc.cooldown = std::uint16_t(m.load(address + 0x318, 2));
  sc.stun = std::uint16_t(m.load(address + 0x31c, 2));
  sc.x31a = std::uint16_t(m.load(address + 0x31a, 2));
  sc.x231 = std::int8_t(m.load(address + 0x231, 1));
  sc.variant = a.variant;  // tough 1 / shaman 2 never ran setState (DT:410)
  ai::SetResult r;
  try { r = ai::set_state(ai::Class::Kobold, sc, requested); }
  catch (const ai::Unsupported &e) { throw Unsupported(std::string("class setState: ") + e.what()); }
  required(services_.filter_state, "Creature_filterState/request");
  if (services_.filter_state(address, requested)) return;  // global refusal
  if (r.refused || r.filtered != requested)
    throw Unsupported("class setState: filterState redirect of a non-death request Unknown");
  for (const auto &c : r.cleared) m.store(address + c.first, 2, std::uint64_t(c.second));
  for (const auto &w : r.writes)  // +0x3c8 s32 (M2:4978)
    m.store(address + w.offset, 4, std::uint32_t(w.resolve(std::int64_t(world_.frame), [&a] {
      return std::uint32_t(a.object->rand());
    })));
  a.state = r.landed;
  m.store(address + 0x232, 2, r.landed);
  // The cast clip and the rest of Kobold_setState were not compared.
  throw Unsupported("Kobold setState(8): entry stored; clip start and remaining setState calls Unknown");
}

void Runtime::set_state(std::uint32_t address, std::uint32_t state) {
  auto &a = at(address);
  if (state != 0x14) { enter(a, state); return; }
  if (a.death_reported) return;
  if (a.kind == Kind::WarWraith)
    required(services_.loot_generate, "loot_generateDrops");
  else
    required(services_.drop_loot, "Creature_dropLoot");
  required(services_.death_remainder, "class setState/death remainder");
  if (a.kind == Kind::WarWraith)
    required(services_.holder_drop, "WarWraith_setState/holder part drop");
  required(services_.filter_state, "Creature_filterState/death request");
  if (services_.filter_state(address, state)) return;
  // For an accepted state-20 request, Creature_filterState clears these four
  // timers before derived setState writes or drop calls.
  for (auto off : {0x31cu, 0x318u, 0x314u, 0x31au})
    a.hp.mem.store(address + off, 2, 0);
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
  if (a.state == 0) {
    if (in.has_target || (a.kind != Kind::WarWraith && in.away_home))
      throw Unsupported("class run/state0: target or home transition Unknown");
    if (!in.idle_deadline)
      throw Unsupported("class run/state0: idle deadline answer Unknown");
  }
  if (a.state == 3 && in.has_target)
    throw Unsupported("class run/state3: target decision Unknown");
  if ((a.state == 6 || a.state == 7) && !in.animation_over)
    throw Unsupported("class run/attack animation completion answer Unknown");
  // Targeted state 7 (C:267-268 Kobold, C:289-292 WarWraith drain; Executed in
  // every variant, MOD:109-117, MOD:194-204): its inputs and services are
  // required before any effect.
  if (a.state == 7 && in.has_target) {
    if (a.kind == Kind::WarWraith) {
      required(services_.cold_weapons, "WarWraith_run/drain ColdWeapons skill value");
      required(services_.drain_at_reach, "WarWraith_run/drain hitTargetAtReach");
    } else if (!in.second_event) {
      throw Unsupported("Kobold_run/state7: animation event bit 1 answer Unknown");
    }
  }
  if (a.state != 0 && a.state != 3 && a.state != 6 && a.state != 7 &&
      a.state != 0x14)
    throw Unsupported("class run: state or AI branch Unknown");
  const bool wraith = a.kind == Kind::WarWraith;
  if (wraith && (a.state == 6 || a.state == 7) && in.has_target && !in.target_position)
    throw Unsupported("WarWraith_run/targeted attack: target position Unknown");
  auto fade = wraith ? std::uint32_t(a.hp.mem.load(address + 0x3c8, 1)) : 0u;
  if (wraith && fade > 100)
    throw Unsupported("WarWraith_run: fade outside documented range");
  if (wraith && a.state == 0x14 && fade != 100)
    throw Unsupported("WarWraith_run/dead fade and particle path Unknown");
  required(services_.animation_step, "Creature_stepAnim");
  required(services_.knockback, "Creature_applyKnockback");
  if (wraith) {
    required(services_.holder_update, "WarWraith_run/holder update");
    services_.holder_update(address + 0x3d0);
    services_.animation_step(address);
    if (a.state != 0x14) {
      required(services_.pick_target, "Creature_pickTarget");
      services_.pick_target(address);
    }
    services_.knockback(address);
    auto flags = a.hp.mem.load(address + 0xa8, 8);
    flags = fade >= 21 ? flags | UINT64_C(0x54) : flags & ~UINT64_C(0x54);
    a.hp.mem.store(address + 0xa8, 8, flags);
    if (fade >= 99)
      a.object->flags |= 1u;
    else if (a.state != 0x14)
      a.object->flags &= ~1u;
    a.hp.mem.store(address + 0xc, 4, a.object->flags);
    if (fade >= 99) {
      required(services_.turn_target, "WarWraith_run/fade turn");
      services_.turn_target(address);
    }
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
    if (wraith)
      a.hp.mem.store(address + 0x3c8, 1, std::min(fade + 1u, 100u));
    required(services_.can_see_target, "class run/state0 sight check");
    services_.can_see_target(address);
    if (!wraith) {
      required(services_.away_home_probe, "Kobold_run/state0 home check");
      services_.away_home_probe(address);
    }
    if (world_.frame > *in.idle_deadline) {
      required(services_.state_change, "class run/state0 idle expiry");
      services_.state_change(address, 1);
    }
    break;
  case 3:
    if (wraith)
      a.hp.mem.store(address + 0x3c8, 1,
                     fade > 60 ? fade - 1 : fade < 60 ? fade + 1 : fade);
    required(services_.state_change, "class run/state3 transition");
    services_.state_change(address, 0);
    break;
  case 6:
  case 7:
    if (wraith)
      a.hp.mem.store(address + 0x3c8, 1, fade > 2 ? fade - 2 : 0);
    if (a.state == 7 && in.has_target) {
      if (!wraith) {
        required(services_.settle, "Kobold_run/settle");
        services_.settle(address);
      }
      targeted_attack7(a, in, fade);
      break;
    }
    if (!wraith) {
      required(services_.settle, "Kobold_run/settle");
      services_.settle(address);
      if (in.has_target && in.hit_event) {
        required(services_.hit_target_in_arc, "Creature_hitTargetInArc");
        services_.hit_target_in_arc(address,
            a.kind == Kind::KoboldTough ? 0x440008 : 8);
      }
      if (in.has_target) {
        required(services_.turn_target, "Creature_turnToTarget");
        services_.turn_target(address);
      }
    } else if (in.has_target) {
      if (in.hit_event) {
        required(services_.holder_descriptor, "WarWraith_run/holder descriptor");
        required(services_.hit_target_at_reach, "Creature_hitTargetAtReach");
        services_.holder_descriptor(address + 0x3d0);
        services_.hit_target_at_reach(address, 8);
      }
      required(services_.grid_update, "WarWraith_run/obj_updateGridCell");
      required(services_.turn_target, "WarWraith_run/targeted turn");
      services_.turn_target(address);
      auto &height = a.object->pos[2];
      const auto target_height = (*in.target_position)[2];
      const auto step = div(from_int(1), from_int(6));
      if (lt(height, target_height))
        height = add(height, step);
      else if (lt(target_height, height))
        height = sub(height, step);
      a.hp.mem.store(address + 0x18, 4, height);
      services_.grid_update(address);
    }
    if (*in.animation_over) {
      required(services_.state_change, "class run/attack completion");
      services_.state_change(address, wraith ? 3 : 5);
    }
    break;
  case 0x14:
    if (wraith) {
      required(services_.delete_object, "WarWraith_run/fade deletion");
      services_.delete_object(address);
    } else {
      required(services_.move_by, "Kobold_run/dead move");
      required(services_.dead_update, "Kobold_run/deadUpdate");
      services_.move_by(address);
      services_.dead_update(address);
    }
    break;
  }
}

void Runtime::targeted_attack7(Actor &a, const TickInput &in, std::uint32_t fade) {
  const auto address = a.object->addr;
  const bool wraith = a.kind == Kind::WarWraith;
  required(services_.turn_target, "Creature_turnToTarget");
  required(services_.state_change, "class run/attack completion");
  if (wraith) required(services_.grid_update, "WarWraith_run/obj_updateGridCell");
  else required(services_.hit_target_in_arc, "Creature_hitTargetInArc");
  // The state body is creature_ai_m1_3's (research-campaign-combat fbb2e1d);
  // the runtime already ran the head, so only events from Tick::body on are
  // dispatched, in order.
  ai::Context c;
  c.state = 7;
  c.target = true;
  c.ev = (in.hit_event ? 1u : 0u) | (in.second_event.value_or(false) ? 2u : 0u);
  c.anim = *in.animation_over ? 0u : 8u;
  c.fields[0x3cc] = a.variant;
  c.fields[0x3c8] = fade;
  const auto answer = [&](const ai::Event &e) -> std::int64_t {
    if (e.name != "hitAtReach") return 0;
    // M2:6162-6165: ColdWeapons value, then hitTargetAtReach(36, 0x404).
    const auto cold = services_.cold_weapons(address);
    const auto hit = services_.drain_at_reach(address, 0x404, cold);
    // M2:6165-6167 (Observed): a landed drain heals by max HP / 15. The
    // research did not compare that write (C:291-292) and gives the factor
    // only approximately, so it is refused rather than skipped.
    if (hit) throw Unsupported("WarWraith drain landed: max HP / 15 heal not compared");
    return 0;
  };
  ai::Tick t;
  try { t = ai::tick(wraith ? ai::Class::WarWraith : ai::Class::Kobold, c, answer); }
  catch (const ai::Unsupported &e) { throw Unsupported(std::string("class run/state7: ") + e.what()); }
  for (std::size_t i = t.body; i < t.events.size(); ++i) {
    const auto &e = t.events[i];
    if (e.name == "hitInArc") {
      services_.hit_target_in_arc(address, std::uint32_t(e.args.at(7) / ai::ARG_SCALE));
    } else if (e.name == "turn") {
      services_.turn_target(address);
      if (wraith) {  // M2:6149-6150: the z-follow runs in both attack states
        auto &height = a.object->pos[2];
        const auto target_height = (*in.target_position)[2];
        const auto step = div(from_int(1), from_int(6));
        if (lt(height, target_height)) height = add(height, step);
        else if (lt(target_height, height)) height = sub(height, step);
        a.hp.mem.store(address + 0x18, 4, height);
        services_.grid_update(address);
      }
    } else if (e.name == "set") {
      services_.state_change(address, std::uint32_t(e.args.at(0) / ai::ARG_SCALE));
    } else if (e.name != "skill+0x5c" && e.name != "hitAtReach") {
      throw Unsupported("class run/state7: kernel event " + e.name + " has no runtime service");
    }
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
