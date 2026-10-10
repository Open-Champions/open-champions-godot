#pragma once
#include "champions/combat_damage.hpp"
#include "champions/creature_hp.hpp"
#include <memory>
#include <string>
#include <vector>

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
class Adapter {
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
  std::size_t operation_depth() const noexcept;
  std::size_t raw_callback_depth() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace champions::simulation::rta::combat_creature
