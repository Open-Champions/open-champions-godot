#pragma once
#include "champions/combat_damage.hpp"
#include "champions/creature_hp.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::detail {
struct Checkpoint;
struct CheckpointAccess;
} // namespace champions::simulation::rta::detail
namespace champions::simulation::rta::combat_creature {
// Authored composition, not an object allocator or a shared object-World pool.
enum class Projection { CreatureFields, None };
enum class Dispatch { BaseCreature, Custom };
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : Error {
  using Error::Error;
};
struct Limits {
  std::size_t entries = 1024, raw_regions = 1024, depth = 64;
  creature_hp::Limits memory{};
};
struct Binding {
  std::uint32_t address = 0;
  std::uint64_t generation = 0;
  std::uint32_t extent = 0;
  Projection projection = Projection::CreatureFields;
  Dispatch dispatch = Dispatch::BaseCreature;
  std::string label;
};
struct RawRegion {
  std::uint32_t address = 0, size = 0;
  std::string label;
};
struct CallbackBinding {
  std::string name, label;
};
// Owned observations: all pointer fields in value records are cleared and their
// identities are represented by optional zero-based indices in these tables.
struct ObjSnapshot {
  combat::Obj value;
  std::optional<std::size_t> owner, status_source, last_attacker;
  std::vector<std::optional<std::size_t>> spells;
};
struct SpellSnapshot {
  combat::Spell value;
  std::optional<std::size_t> owner;
};
struct ItemSnapshot {
  combat::Item value;
  std::optional<std::size_t> definition;
};
struct PlayerSnapshot {
  combat::PlayerRec value;
  std::optional<std::size_t> object, second_weapon;
  std::vector<std::optional<std::size_t>> bag2, equip, weapon_sets;
};
struct WorldSnapshot {
  combat::Seed seed, cosmetic;
  std::int64_t frame = 0, pvp_a = 0, cutscene = 0, numbers_option = 0,
               network_mode = 0;
  Raw monster_level = 0;
  std::vector<Raw> focus;
  std::vector<std::int64_t> music;
  std::int64_t music_index = 0, num_players = 0, medal_mission = 0,
               difficulty = 0, load_cheat = 0;
  std::vector<std::int64_t> alive;
  combat::Text level_name;
  std::vector<combat::Event> events;
  std::vector<PlayerSnapshot> players;
  std::vector<std::optional<std::size_t>> owned_spells;
  // stat, stat_for, adjust, pool, enemy, take_damage.
  std::array<bool, 6> callbacks{};
  std::optional<combat::SharedRandomSnapshot> shared_random;
};
struct ModelSnapshot {
  std::map<std::uint32_t, std::uint8_t> backing;
  std::vector<creature_hp::Write> stores, writes;
  std::vector<creature_hp::Call> calls;
  std::vector<creature_hp::Event> events;
  std::uint64_t draws = 0;
  creature_hp::CallbackPolicy policy =
      creature_hp::CallbackPolicy::RequireHandlers;
  std::vector<std::pair<std::string, bool>> callbacks;
  bool overlay_present = false;
};
struct ProjectionSnapshot {
  std::uint32_t flags = 0;
  std::array<Raw, 3> position{};
  std::uint64_t cflags = 0;
  Raw hp = 0;
  std::array<Raw, 5> status{};
};
struct CallbackSnapshot {
  std::vector<ProjectionSnapshot> expected;
  std::int64_t frame = 0;
};
struct Snapshot {
  Limits limits;
  std::size_t operation_depth = 0, raw_callback_depth = 0;
  std::vector<CallbackSnapshot> callback_frames;
  std::vector<Binding> bindings;
  std::vector<RawRegion> regions;
  std::vector<CallbackBinding> callbacks;
  std::vector<ObjSnapshot> objects;
  std::vector<SpellSnapshot> spells;
  std::vector<ItemSnapshot> items;
  std::vector<combat::ItemDef> definitions;
  WorldSnapshot world;
  ModelSnapshot model;
};
class Adapter {
  friend struct detail::CheckpointAccess;
  void checkpoint(detail::Checkpoint &);

public:
  using HandleCallback =
      std::function<void(Adapter &, const std::vector<std::uint32_t> &)>;
  using DamageCallback = std::function<void(Adapter &, combat::Obj &, Raw,
                                            combat::Obj *, std::uint32_t)>;
  explicit Adapter(std::uint32_t seed = 0, std::uint32_t cosmetic = 0,
                   creature_hp::CallbackPolicy policy =
                       creature_hp::CallbackPolicy::RequireHandlers,
                   Limits limits = {});
  ~Adapter();
  Adapter(const Adapter &) = delete;
  Adapter &operator=(const Adapter &) = delete;
  Adapter(Adapter &&) = delete;
  Adapter &operator=(Adapter &&) = delete;
  // Stable owned records; pointer members keep their supplied identities. The
  // caller must retain external pointees. No removal/address reuse is provided.
  // CreatureFields reserves 0x3b2 bytes. None reserves the explicit extent.
  combat::Obj &create(std::uint32_t address, combat::Obj value = {},
                      Projection projection = Projection::CreatureFields,
                      Dispatch dispatch = Dispatch::BaseCreature,
                      std::uint32_t extent = 1);
  combat::Obj *lookup(std::uint32_t address) noexcept;
  const combat::Obj *lookup(std::uint32_t address) const noexcept;
  Binding binding(const combat::Obj &) const;
  std::vector<Binding> bindings() const;
  void reserve_raw(std::uint32_t address, std::uint32_t size,
                   const std::string &label);
  std::vector<RawRegion> raw_regions() const;
  void bind_custom(combat::Obj &, const std::string &label, DamageCallback);
  void set_handle_callback(const std::string &name, const std::string &label,
                           HandleCallback);
  // Explicit handle-backing/observation hooks. No pointer field is implicitly
  // serialized. Before runs after scalar projection; after runs after a normal
  // HP return. Both run under raw-memory authority, including nested calls.
  void set_before_base(const std::string &label, DamageCallback);
  void set_after_base(const std::string &label, DamageCallback);
  std::vector<CallbackBinding> callback_bindings() const;
  // Trusted synchronous access. Do not replace World.take_damage_fn, Model
  // callbacks/policy, or destroy the Adapter during an operation. Mutable
  // callback state belongs in owned memory or stable reference captures.
  // Use Adapter entry points for nested damage; calling combat/Model methods
  // directly bypasses adapter authority and operation-depth guards. Combat's
  // public containers and callback resources remain caller-managed.
  combat::World &world() noexcept;
  const combat::World &world() const noexcept;
  creature_hp::Model &model() noexcept;
  const creature_hp::Model &model() const noexcept;
  // Setup synchronization: project exact flags32,pos3,cflags64,hp,status5 plus
  // frame32, or publish those fields from memory. Other bytes and pointer
  // fields remain explicit backing. Missing bytes publish as zero.
  void project();
  void publish();
  void take_damage(combat::Obj &, Raw amount, combat::Obj *attacker,
                   std::uint32_t flags);
  int damage(combat::Obj &, combat::Obj *attacker, Raw minimum, Raw maximum,
             std::int64_t attack, std::uint32_t flags,
             const combat::Descriptor *descriptor = nullptr,
             std::int64_t no_crit = 0);
  Snapshot snapshot() const;
  std::size_t operation_depth() const noexcept;
  std::size_t raw_callback_depth() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace champions::simulation::rta::combat_creature
