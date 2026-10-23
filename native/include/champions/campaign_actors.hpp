#pragma once

#include "champions/creature_hp.hpp"
#include "champions/combat_stats.hpp"
#include "champions/objects.hpp"
#include "champions/resource_manager.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>

// Bounded RTA US v2.00 class ports. One objects::World block is the actor's
// grid and run-list identity. Missing Creature/animation service answers refuse
// at the method that needs them; no captured heap address is used as a handle.
namespace champions::simulation::rta::campaign_actors {
enum class Kind { WarBoar, WarBoarSmall, Minotaur };
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
struct LootArgs {
  Raw z, throw_speed, gold_scale, gold_chance, quality_row, item_chance;
  Raw filter = 0, force = 0;
};
struct EventParams {
  std::uint32_t entry;
  std::array<std::uint32_t, 4> masks;
  std::array<Raw, 4> times;
};
// monsters.bal rows are supplied from the selected game data. The five signed
// percentages use the original single-precision 0.01 multiplication.
std::array<Raw, 5> balance_lookup(const std::uint8_t *, std::size_t,
                                  const std::string &type_name);
struct TickInput {
  bool active = false, has_target = false, away_home = false;
  bool hit_event = false, next_event = false, animation_over = false;
  // Frame at which state 0 was entered. The class sets +0xf4 to this age plus
  // the fixed 180 on entry; the boar constructor alone does not establish it.
  std::optional<std::int64_t> state_entry_frame;
};
struct Services {
  // Creature_run, updateActive, stepAnim, knockback, pickTarget and the
  // remaining class AI questions are supplied by the caller. Empty handlers
  // are Unknown and refuse only when their branch is reached.
  std::function<TickInput(std::uint32_t)> tick_input;
  std::function<void(std::uint32_t)> process_damage;
  std::function<void(std::uint32_t)> creature_run;
  std::function<void(std::uint32_t)> animation_step;
  std::function<void(std::uint32_t)> knockback;
  std::function<void(std::uint32_t)> pick_target;
  std::function<void(std::uint32_t)> dead_update;
  std::function<void(std::uint32_t, std::uint32_t)> hit_all_in_arc;
  // The class call sites are known, but model masks, relocated animation
  // state, terrain and hand-part writes have no complete original result.
  // Native resource ID for definition slot 0, then variant.
  std::function<std::uint64_t(std::uint32_t, std::uint32_t)> model_hide_mask;
  std::function<std::uint32_t(std::uint32_t, std::uint32_t)> minotaur_variant;
  std::function<std::uint64_t(std::uint32_t)> minotaur_base_flags;
  std::function<void(std::uint32_t, bool)> animation_init;
  std::function<void(std::uint32_t)> minotaur_initial_state;
  std::function<void(std::uint32_t, const EventParams &)> set_def_entry_params;
  std::function<void(std::uint32_t)> creature_init;
  std::function<void(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t)> weapon_shield;
  // Creature_dropLoot answer for a rank-0, untinted creature only: eligibility
  // plus the one ordinary generator call. The runtime refuses rank 1/2, tint
  // and unknown rank before calling, so the service never sees them.
  std::function<void(std::uint32_t, const LootArgs &)> creature_drop_loot;
  std::function<void(std::uint32_t)> on_death;
  std::function<void(std::uint32_t)> minotaur_death_remainder;
  std::function<void(std::uint32_t)> death_animation;
  std::function<bool(std::uint32_t, std::uint32_t)> filter_state;
  std::function<void(std::uint32_t, std::uint32_t)> animation_blend;
  std::function<void(std::uint32_t)> handle_clear;
  std::function<void(std::uint32_t, std::uint32_t)> handle_link;
};
struct Actor {
  Kind kind;
  objects::GameObject *object;
  std::uint32_t state = 0, last_attack = 0, hit_frame = 0;
  std::uint32_t forward_offset = 0, scale = 0, turn_rate = 0, kind_byte = 0;
  std::uint32_t small_marker = 0;
  std::uint64_t hidden_parts = 0;
  std::uint64_t creature_flags = 0;
  std::optional<std::int64_t> supplied_rank;
  creature_hp::Model hp;
  bool death_reported = false;
  combat::MonsterStats stats;
  Actor(Kind, objects::GameObject *);
  virtual ~Actor() = default;
  Raw health() const;
  bool dead() const;
};
struct WarBoar : Actor {
  explicit WarBoar(objects::GameObject *o, bool small = false)
      : Actor(small ? Kind::WarBoarSmall : Kind::WarBoar, o) {}
};
struct WarBoarSmall : WarBoar {
  explicit WarBoarSmall(objects::GameObject *o) : WarBoar(o, true) {}
};
struct Minotaur : Actor {
  explicit Minotaur(objects::GameObject *o) : Actor(Kind::Minotaur, o) {}
};
class Runtime {
  objects::World &world_;
  resources::Manager &resources_;
  Services services_;
  std::shared_ptr<objects::ClassDef> boar_def_, minotaur_def_;
  std::map<std::uint32_t, std::unique_ptr<Actor>> actors_;
  std::vector<resources::Handle> handles_;
  std::uint32_t resolve(const std::string &, const std::string &);
  Actor &construct_impl(Kind, const objects::Vec3 &, std::uint32_t,
                        objects::Props, Raw,
                        const std::function<void(Actor &)> &);
  void tick_boar(Actor &, const TickInput &);
  void set_state(Actor &, std::uint32_t);
public:
  Runtime(objects::World &, resources::Manager &, Services);
  ~Runtime();
  // Supplied HP is a bounded synthetic/base-service answer. Use the balanced
  // overload when the selected level's tables and rank are available.
  // boss_rank is the caller's creature_bossRank answer; absent means Unknown
  // and death loot refuses.
  Actor &construct(Kind, const objects::Vec3 &, std::uint32_t angle,
                   objects::Props, Raw initial_hp,
                   std::optional<std::int64_t> boss_rank = std::nullopt);
  Actor &construct_balanced(Kind, const objects::Vec3 &, std::uint32_t angle,
                            objects::Props, const std::uint8_t *balance,
                            std::size_t balance_size, const combat::Tables &,
                            const combat::World &, std::int64_t rank);
  Actor &at(std::uint32_t);
  void tick(std::uint32_t);
  void set_state(std::uint32_t, std::uint32_t);
  void take_damage(std::uint32_t, std::uint32_t attacker,
                   std::uint32_t flags, Raw amount);
};
} // namespace champions::simulation::rta::campaign_actors
