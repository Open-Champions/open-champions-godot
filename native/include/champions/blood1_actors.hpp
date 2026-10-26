#pragma once

#include "champions/combat_stats.hpp"
#include "champions/creature_hp.hpp"
#include "champions/objects.hpp"
#include "champions/resource_manager.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>

// Isolated RTA US v2.00 blood1 actors. The interface mirrors campaign_actors:
// one World object per actor, caller-owned lower services, no scene binding.
namespace champions::simulation::rta::blood1_actors {
enum class Kind { Kobold, KoboldTough, KoboldShaman, WarWraith };
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
struct LootArgs { Raw z, throw_speed, gold_scale, gold_chance, quality_row, item_chance; };
struct TickInput {
  bool active = false, has_target = false, hit_event = false;
  std::optional<bool> animation_over;
  bool away_home = false;
  // Caller answers the class +0xf4 deadline after its state-0 setup.
  std::optional<std::int64_t> idle_deadline;
  std::optional<objects::Vec3> target_position;
};
struct Services {
  std::function<void(std::uint32_t)> model_init, animation_init, creature_init;
  std::function<void(std::uint32_t, std::uint32_t)> event_setup;
  std::function<void(std::uint32_t, std::uint32_t)> holder_set_part;
  std::function<TickInput(std::uint32_t)> tick_input;
  std::function<void(std::uint32_t)> process_damage, creature_run;
  std::function<void(std::uint32_t)> animation_step, knockback, pick_target, pick_ally;
  std::function<void(std::uint32_t)> holder_update, holder_descriptor;
  std::function<void(std::uint32_t)> turn_target, grid_update, settle, dead_update;
  std::function<void(std::uint32_t)> can_see_target, away_home_probe, move_by;
  // True means Creature_filterState refused the death request. The caller
  // supplies its global PvP context; the runtime applies the documented
  // state-20 timer clearing only after an accepted answer.
  std::function<bool(std::uint32_t, std::uint32_t)> filter_state;
  // Tick probes prove a setState request, not its resulting AI state.
  std::function<void(std::uint32_t, std::uint32_t)> state_change;
  std::function<void(std::uint32_t, std::uint32_t)> hit_target_in_arc;
  std::function<void(std::uint32_t, std::uint32_t)> hit_target_at_reach;
  std::function<void(std::uint32_t)> holder_drop;
  std::function<void(std::uint32_t, const LootArgs &)> drop_loot;
  std::function<void(std::uint32_t, const LootArgs &)> loot_generate;
  std::function<void(std::uint32_t)> death_remainder, delete_object;
  std::function<void(std::uint32_t)> handle_clear;
  std::function<void(std::uint32_t, std::uint32_t)> handle_link;
};
std::array<Raw, 5> balance_lookup(const std::uint8_t *, std::size_t,
                                  const std::string &type_name);
struct Actor {
  Kind kind;
  objects::GameObject *object;
  std::uint64_t object_serial;
  creature_hp::Model hp;
  combat::MonsterStats stats;
  std::uint32_t state = 0, variant = 0, weapon_part = 0;
  bool death_reported = false;
  Actor(Kind k, objects::GameObject *o)
      : kind(k), object(o), object_serial(o->serial) {}
  virtual ~Actor() = default;
  Raw health() const;
  bool dead() const;
};
struct Kobold : Actor {
  explicit Kobold(objects::GameObject *o, Kind k = Kind::Kobold) : Actor(k, o) {}
};
struct KoboldTough : Kobold {
  explicit KoboldTough(objects::GameObject *o) : Kobold(o, Kind::KoboldTough) {}
};
struct KoboldShaman : Kobold {
  explicit KoboldShaman(objects::GameObject *o) : Kobold(o, Kind::KoboldShaman) {}
};
struct WarWraith : Actor {
  explicit WarWraith(objects::GameObject *o) : Actor(Kind::WarWraith, o) {}
};
class Runtime {
  objects::World &world_;
  resources::Manager &resources_;
  Services services_;
  std::shared_ptr<objects::ClassDef> kobold_def_, wraith_def_;
  std::map<std::uint32_t, std::unique_ptr<Actor>> actors_;
  std::vector<resources::Handle> handles_;
  void retire_stale(std::uint32_t);
  std::uint32_t resolve(const std::string &, const std::string &);
  Actor &construct_impl(Kind, const objects::Vec3 &, std::uint32_t,
                        objects::Props, Raw, const std::function<void(Actor &)> &);
public:
  Runtime(objects::World &, resources::Manager &, Services);
  ~Runtime();
  Actor &construct(Kind, const objects::Vec3 &, std::uint32_t angle,
                   objects::Props, Raw initial_hp);
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
} // namespace champions::simulation::rta::blood1_actors
