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
      result[i] = combat::fmul(combat::itof(u16(at + 2 + 2 * i)), 0x3c23d70a);
    return result;
  }
  // Observed Creature_ctor fallback for a type absent from monsters.bal.
  return {ONE, ONE, ONE, ONE, ONE};
}

Actor::Actor(Kind k, objects::GameObject *o, creature_hp::Memory *shared)
    : kind(k), object(o), object_serial(o->serial),
      hp_owned_(shared ? std::make_unique<creature_hp::Model>(*shared)
                       : std::make_unique<creature_hp::Model>()),
      hp(*hp_owned_) {}
Raw Actor::health() const { return Raw(hp.mem.load(object->addr + 0x228, 4)); }
bool Actor::dead() const { return !lt(ZERO, health()); }

Runtime::Runtime(objects::World &world, resources::Manager &resources, Services services,
                 creature_hp::Memory *shared)
    : world_(world), resources_(resources), services_(std::move(services)),
      boar_def_(definition("warboar.lmp", BOAR_NAMES)), shared_(shared) {
  // A shared-memory Runtime is dispatched and resolved by its owner (the
  // combat World calls tick(address); entries resolve in adopt()).
  if (shared_) return;
  // This bounded runtime owns dispatch for the supplied isolated World.
  if (world_.on_run || world_.res_find)
    throw Unsupported("constructor: World already has actor or resource dispatch");
  world_.res_find = [this](const std::string &lump, const std::string &entry) {
    return resolve(lump, entry);
  };
  world_.on_run = [this](objects::GameObject &o) { tick(o.addr); };
}
Runtime::~Runtime() {
  if (shared_) return;
  world_.on_run = {};
  world_.res_find = {};
}
void Runtime::bind_minotaur_definition(std::string lump, const std::vector<std::string> &entries) {
  if (lump.empty() || entries.size()!=26)
    throw Unsupported("Minotaur definition: archive and 26 entries required");
  for (const auto &name:entries)
    if (name.empty()) throw Unsupported("Minotaur definition: empty entry");
  if (!actors_.empty()) throw Unsupported("Minotaur definition: bind before construction");
  minotaur_def_=std::make_shared<objects::ClassDef>(std::move(lump),entries);
}

bool Runtime::owns_live_object(const objects::GameObject &object) const {
  const auto found = world_.objects.find(object.addr);
  return found != world_.objects.end() && found->second == &object && object.alive;
}


Actor &Runtime::register_actor(std::unique_ptr<Actor> actor) {
  retire_stale();
  if (!owns_live_object(*actor->object) || actor->object->serial != actor->object_serial)
    throw Unsupported("actor object is not live in this World");
  const auto address = actor->object->addr;
  const auto inserted = actors_.emplace(address, std::move(actor));
  if (!inserted.second)
    throw Unsupported("address already has an actor");
  return *inserted.first->second;
}

std::optional<std::pair<std::string, std::string>> Runtime::resource_name(std::uint32_t token) const {
  if (!token || token > names_.size()) return std::nullopt;
  return names_[token - 1];
}
std::optional<champions::Bytes> Runtime::resource_bytes(std::uint32_t token) {
  if (!token || token > handles_.size()) return std::nullopt;
  return resources_.bytes(handles_[token - 1]);
}

std::uint32_t Runtime::resolve(const std::string &lump, const std::string &entry) {
  auto handle = resources_.find(lump, entry);
  if (!handle || !resources_.bytes(*handle))
    throw Unsupported("GameObject_ctor/res_find: named resource unavailable");
  handles_.push_back(*handle);
  names_.emplace_back(lump, entry);
  // Native opaque IDs are scoped to this Runtime; they are never game pointers.
  return std::uint32_t(handles_.size());
}

Actor &Runtime::construct(Kind kind, const objects::Vec3 &pos,
                          std::uint32_t angle, objects::Props props, Raw initial_hp,
                          std::optional<std::int64_t> boss_rank) {
  return construct_impl(kind, pos, angle, std::move(props), initial_hp,
                        [boss_rank](Actor &a) { a.supplied_rank = boss_rank; });
}

Actor &Runtime::construct_impl(Kind kind, const objects::Vec3 &pos,
                              std::uint32_t angle, objects::Props props,
                              Raw initial_hp,
                              const std::function<void(Actor &)> &base_stats) {
  retire_stale();
  if (props && !props->empty())
    throw Unsupported("Creature_ctor/properties: complete base effects Unknown");
  required(services_.model_hide_mask, "constructor/model_buildHideMask");
  required(services_.animation_init, "constructor/anim_clear_push");
  required(services_.set_def_entry_params, "constructor/creature_setDefEntryParams");
  required(services_.creature_init, "constructor/Creature_init");
  if (kind == Kind::Minotaur) {
    required(services_.weapon_shield, "Minotaur_ctor/WeaponShield");
    required(services_.minotaur_variant, "Minotaur_ctor/hide variant Unknown");
    required(services_.minotaur_base_flags, "Minotaur_ctor/base creature flags Unknown");
    required(services_.minotaur_initial_state, "Minotaur_ctor/setState(0)");
  }
  auto def = kind == Kind::Minotaur ? minotaur_def_ : boar_def_;
  if (!def) throw Unsupported("Minotaur definition: runtime bindings missing");
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
      // Creature_ctor 0x109d68: constructor clock/deadline are both zero;
      // it does not enter idle through WarBoar_setState(0).
      actor->hp.mem.store(o->addr + 0x300, 4, 0);
      actor->hp.mem.store(o->addr + 0xf4, 4, 0);
      o->radius = 0x41f00000;
      o->draw_half_width = 40;
    } else {
      o->radius = 0x41c80000; // Minotaur_ctor: 25
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
      actor->creature_flags = services_.minotaur_base_flags(o->addr) |
                              UINT64_C(0x200040000);
      actor->turn_rate = 0x3f999999; // 1.2
      actor->kind_byte = 0xb;
      actor->hp.mem.store(o->addr + 0xb8, 4, 0);
      actor->hp.mem.store(o->addr + 0xb0, 4, actor->turn_rate);
      actor->hp.mem.store(o->addr + 0xb4, 1, actor->kind_byte);
      actor->hp.mem.store(o->addr + 0xf4, 4, std::uint32_t(world_.frame) + 300u);
      actor->hp.mem.store(o->addr + 0xa8, 8, actor->creature_flags);
      actor->hp.mem.store(o->addr + 0x46c, 4, 0);
      services_.weapon_shield(o->addr + 0x3c8, 1, 0x15, 0x20);
      services_.weapon_shield(o->addr + 0x3c8, 2, 0x19, 0x20);
    }
    if (kind != Kind::Minotaur) {
      actor->hp.mem.store(o->addr + 0xb0, 4, actor->turn_rate);
      actor->hp.mem.store(o->addr + 0xb4, 1, actor->kind_byte);
      actor->hp.mem.store(o->addr + 0xa8, 8, actor->creature_flags);
    }
    const auto own_draw = kind == Kind::Minotaur ? o->rand() : 0u;
    if (kind == Kind::Minotaur)
      actor->hp.mem.store(o->addr + 0xe8, 4, 1u << (own_draw & 1u));
    const auto variant = kind == Kind::Minotaur
        ? services_.minotaur_variant(o->addr, own_draw) : 0u;
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
    return register_actor(std::move(actor));
  } catch (...) {
    // An unavailable downstream model/animation/terrain service must not
    // leave a live grid object with no actor dispatch record.
    if (owns_live_object(*o)) world_.destroy(*o);
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
        a.supplied_rank = rank;
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

void Runtime::retire_stale() const {
  for (auto entry = actors_.begin(); entry != actors_.end();) {
    const auto live = world_.objects.find(entry->first);
    if (live == world_.objects.end() || live->second != entry->second->object ||
        live->second->serial != entry->second->object_serial || !live->second->alive)
      entry = actors_.erase(entry);
    else
      ++entry;
  }
}

Actor *Runtime::find(std::uint32_t address) {
  retire_stale();
  auto it = actors_.find(address);
  return it == actors_.end() ? nullptr : it->second.get();
}

std::uint32_t Runtime::definition_entry(Kind kind, unsigned slot) {
  auto def = kind == Kind::Minotaur ? minotaur_def_ : boar_def_;
  if (!def) throw Unsupported("Minotaur definition: runtime bindings missing");
  if (!slot || slot > def->entries.size())
    throw Unsupported("definition slot outside the documented table");
  return def->entries[slot - 1].second;
}

Actor &Runtime::adopt(Kind kind, objects::GameObject &object, std::uint32_t angle,
                      std::optional<std::int64_t> boss_rank) {
  retire_stale();
  if (!shared_) throw Unsupported("adopt: needs a shared-memory Runtime");
  if (!owns_live_object(object))
    throw Unsupported("adopt: object is not live in this World");
  if (kind == Kind::Minotaur) {
    required(services_.minotaur_construct,"Minotaur constructor");
    if (!minotaur_def_) throw Unsupported("Minotaur definition: runtime bindings missing");
    if (actors_.count(object.addr)) throw Unsupported("adopt: address already has an actor");
    auto def=minotaur_def_;
    for (const auto &entry:def->entries) {
      auto handle=resources_.find(def->lump,entry.first);
      if (!handle || !resources_.bytes(*handle))
        throw Unsupported("Minotaur constructor: definition resource unavailable");
    }
    if (def->generation!=world_.lump_generation) {
      for (auto &entry:def->entries) entry.second=resolve(def->lump,entry.first);
      def->generation=world_.lump_generation;
    }
    const auto at=object.addr;
    for (unsigned k=0;k<26;++k)
      shared_->store(0x501200+12+8*k,4,def->entries[k].second);
    object.class_def=def;
    object.class_name="Minotaur";
    object.angle=std::uint16_t(angle);
    object.vtable=0x5165f0;
    object.radius=0x41c80000;
    auto actor=std::make_unique<Minotaur>(&object,shared_);
    actor->supplied_rank=boss_rank;
    services_.minotaur_construct(at,object.type_name.empty()?"Minotaur":object.type_name);
    object.seed=std::uint32_t(shared_->load(at+0x44,4));
    actor->state=std::uint32_t(shared_->load(at+0x232,2));
    actor->hidden_parts=shared_->load(at+0x220,8);
    actor->creature_flags=shared_->load(at+0xa8,8);
    return register_actor(std::move(actor));
  }
  required(services_.model_hide_mask, "constructor/model_buildHideMask");
  required(services_.animation_init, "constructor/anim_clear_push");
  required(services_.set_def_entry_params, "constructor/creature_setDefEntryParams");
  required(services_.creature_init, "constructor/Creature_init");
  if (actors_.count(object.addr))
    throw Unsupported("adopt: address already has an actor");
  auto def = boar_def_;
  // Preflight: every named entry must exist before any state is touched.
  for (const auto &entry : def->entries) {
    auto h = resources_.find(def->lump, entry.first);
    if (!h || !resources_.bytes(*h))
      throw Unsupported("constructor: named definition resource unavailable");
  }
  if (def->generation != world_.lump_generation) {
    for (auto &entry : def->entries) entry.second = resolve(def->lump, entry.first);
    def->generation = world_.lump_generation;
  }
  object.class_def = def;
  object.angle = angle;
  object.class_name = kind == Kind::WarBoarSmall ? "WarBoarSmall" : "WarBoar";
  object.type_name = *object.class_name;
  std::unique_ptr<Actor> actor;
  if (kind == Kind::WarBoarSmall) actor = std::make_unique<WarBoarSmall>(&object, shared_);
  else actor = std::make_unique<WarBoar>(&object, false, shared_);
  auto &mem = actor->hp.mem;
  const auto at = object.addr;
  actor->supplied_rank = boss_rank;
  // WarBoar_ctor 0x2ba03c, after Creature_ctor (research-campaign-combat
  // handoff round 2): the same stores construct() makes, over the base
  // constructor's bytes.
  object.vtable = 0x517cf0;
  object.radius = 0x41f00000;
  object.draw_half_width = 40;
  actor->creature_flags = UINT64_C(0x200040000);
  actor->turn_rate = ONE;
  actor->kind_byte = 3;
  actor->forward_offset = kind == Kind::WarBoarSmall ? 0xc1200000 : 0xc1700000;
  actor->scale = kind == Kind::WarBoarSmall ? 0x3f199999 : 0x3f400000;
  actor->small_marker = kind == Kind::WarBoarSmall ? 1 : 0;
  mem.store(at + 0x48, 4, 0x517cf0);
  mem.store(at + 0x1c, 4, object.radius);
  mem.store(at + 0x28, 1, 40);
  mem.store(at + 0xb8, 4, 0);
  mem.store(at + 0xb0, 4, ONE);
  mem.store(at + 0xb4, 1, 3);
  mem.store(at + 0xa8, 8, mem.load(at + 0xa8, 8) | UINT64_C(0x200040000));
  mem.store(at + 0x5c, 4, 0xbe4ccccc);
  mem.store(at + 0xe0, 4, actor->forward_offset);
  mem.store(at + 0xdc, 4, actor->scale);
  mem.store(at + 0x3d0, 1, actor->small_marker);
  mem.store(at + 0x232, 2, 0);
  mem.store(at + 0xec, 4, 0);
  mem.store(at + 0xe8, 4, 0);
  mem.store(at + 0x220, 8, 0);
  actor->hp.callbacks["handle_clear"] = [this](creature_hp::Model &, const std::vector<std::uint32_t> &a) {
    required(services_.handle_clear, "Creature_takeDamage/handle_clear");
    services_.handle_clear(a.at(0));
  };
  actor->hp.callbacks["handle_link"] = [this](creature_hp::Model &, const std::vector<std::uint32_t> &a) {
    required(services_.handle_link, "Creature_takeDamage/handle_link");
    services_.handle_link(a.at(0), a.at(1));
  };
  // Definition words: slot k's resolved ID lives at +8k+4 (research: class
  // definition 0x502f80; model_buildHideMask reads slot 1, idle is slot 16).
  for (unsigned k = 1; k <= def->entries.size(); ++k)
    mem.store(0x502f80 + 8 * k + 4, 4, def->entries[k - 1].second);
  actor->hidden_parts = services_.model_hide_mask(def->entries.at(0).second, 0);
  mem.store(at + 0x220, 8, actor->hidden_parts);
  services_.animation_init(at, true);
  world_.run_every_60(object);
  for (const auto &p : BOAR_EVENTS) services_.set_def_entry_params(at, p);
  services_.creature_init(at);
  return register_actor(std::move(actor));
}

Actor &Runtime::at(std::uint32_t address) {
  auto *actor = find(address);
  if (!actor)
    throw Unsupported("actor address is not live in this World");
  return *actor;
}

void Runtime::set_state(Actor &a, std::uint32_t state) {
  if (shared_ && a.kind==Kind::Minotaur) {
    required(services_.minotaur_set_state,"Minotaur_setState");
    services_.minotaur_set_state(a.object->addr,state);
    a.state=std::uint32_t(shared_->load(a.object->addr+0x232,2));
    a.death_reported=a.state==20;
    return;
  }
  a.state = std::uint32_t(a.hp.mem.load(a.object->addr + 0x232, 2));
  if (state == 0x14 && a.death_reported) return;
  if (a.kind != Kind::Minotaur && state != 0x14) {
    required(services_.filter_state, "WarBoar_setState/Creature_filterState");
    const auto filtered = services_.filter_state(a.object->addr, state);
    if (!filtered) return;
    state = *filtered;
  }
  if (state == 0x14) {
    // Only the ordinary call is modeled (docs/data-tables.md 2.2): rank 1/2
    // extra calls, the tint multipliers and MiniBoss suppression are not.
    if (!a.supplied_rank)
      throw Unsupported("Creature_dropLoot/creature_bossRank Unknown");
    if (*a.supplied_rank != 0)
      throw Unsupported("Creature_dropLoot/rank 1-2 extra calls Unknown");
    if (a.hp.mem.load(a.object->addr + 0xa0, 1) != 0)
      throw Unsupported("Creature_dropLoot/tint gold and quality scaling Unknown");
    if (a.kind != Kind::Minotaur) {
      required(services_.filter_state, "WarBoar_setState/Creature_filterState");
      required(services_.death_animation, "WarBoar_setState/death animation Unknown");
      const auto filtered = services_.filter_state(a.object->addr, state);
      if (!filtered) return;
      if (*filtered != state) { enter_boar_state(a, *filtered); return; }
    }
    required(services_.creature_drop_loot, "Creature_dropLoot/eligibility Unknown");
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
    services_.creature_drop_loot(a.object->addr, args);
    if (a.kind == Kind::Minotaur) {
      services_.minotaur_death_remainder(a.object->addr);
    } else {
      // Observed boar death-animation choice: one own-generator draw. A
      // shared-memory actor's generator is the object's +0x44 word.
      own_draw(a);
      services_.death_animation(a.object->addr);
      services_.on_death(a.object->addr);
    }
    a.death_reported = true;
    return;
  }
  if (a.kind == Kind::Minotaur)
    throw Unsupported("Minotaur_setState: state service Unknown");
  enter_boar_state(a, state);
}

void Runtime::tick(std::uint32_t address) {
  auto &a = at(address);
  if (a.kind == Kind::Minotaur) {
    if (a.ai_refusal) return;
    required(services_.minotaur_run,"Minotaur_run");
    try { services_.minotaur_run(address); }
    catch (const std::exception &e) {
      if (!contain_) throw;
      if (!a.ai_refusal) { a.ai_refusal=e.what(); refusal_counts_[e.what()]+=1; }
    }
    a.state=std::uint32_t(a.hp.mem.load(address+0x232,2));
    a.death_reported=a.state==20;
    return;
  }
  required(services_.process_damage, "Creature_processDamage");
  required(services_.creature_run, "Creature_run");
  services_.process_damage(address);
  if (services_.creature_run(address)) return;
  if (a.dead() && !a.death_reported) { set_state(a, 0x14); return; }
  if (a.ai_refusal) return;
  if (!contain_) { tick_ai(a, address); return; }
  try {
    tick_ai(a, address);
  } catch (const Unsupported &refusal) {
    a.ai_refusal = refusal.what();
    refusal_counts_[refusal.what()] += 1;
  }
}

std::int64_t Runtime::clock() const {
  // A shared-memory Runtime reads the combat World's simulation frame word.
  return shared_ ? std::int64_t(shared_->load(creature_hp::G_SIM_FRAME, 4)) : world_.frame;
}

void Runtime::tick_ai(Actor &a, std::uint32_t address) {
  required(services_.update_active, "Creature_updateActive");
  if (!services_.update_active(address)) return;
  required(services_.animation_step, "Creature_stepAnim");
  required(services_.knockback, "Creature_applyKnockback");
  required(services_.pick_target, "Creature_pickTarget");
  const auto events = services_.animation_step(address);
  services_.knockback(address);
  services_.pick_target(address);
  a.state = std::uint32_t(a.hp.mem.load(address + 0x232, 2));
  tick_boar(a, events);
}

void Runtime::set_state(std::uint32_t address, std::uint32_t state) {
  set_state(at(address), state);
}

void Runtime::take_damage(std::uint32_t address, std::uint32_t attacker,
                          std::uint32_t flags, Raw amount) {
  auto &a = at(address);
  if (attacker != 0)
    throw Unsupported("Creature_takeDamage: nonzero attacker World binding Unknown");
  a.hp.mem.store(creature_hp::G_SIM_FRAME, 4, std::uint32_t(clock()));
  a.hp.take_damage(address, attacker, flags, amount);
}
} // namespace champions::simulation::rta::campaign_actors
