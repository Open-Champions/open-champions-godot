#pragma once
#include "champions/creature_anim/runtime.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/creature_water.hpp"
#include "champions/world_queries.hpp"
#include <map>
#include <optional>

namespace champions::simulation::rta::player_services {
using Address = combat_runtime::Address;
using Vec3 = ::champions::simulation::rta::Vec3;
using Plane = combat_runtime::Plane;

// The host supplies its current camera values for the local machine. All
// vectors and planes are in native world coordinates and raw32 units. A remote
// slot needs its own snapshot; it never inherits the local camera.
struct Camera {
  Raw yaw = 0;
  std::optional<Raw> flag; // net_getCamFlag needs an observed live mode
  Vec3 position{};
  std::array<Vec3, 4> side_normals{};
};
// Raw bounds and the water flag from the loaded WORLD chunk table. The index
// is only a stable source-order label; no presentation vertices are used.
struct WaterChunk {
  std::array<Raw, 3> minimum{}, maximum{};
};
// cda95dc: these are distinct live owners, not properties of a model clip.
struct AnimationFields {
  std::int8_t weapon_class;
  std::int32_t action_state;
};

// A service bundle borrows the loaded Level and optional runtime authorities.
// Its callbacks must not outlive this object or any borrowed owner.
class Bundle {
  world_queries::Level &level_;
  std::map<Raw, Camera> cameras_;
  Raw local_slot_ = 0;
  creature_anim::Runtime *animation_ = nullptr;
  std::map<Address, creature_anim::StateId> actors_;
  std::map<Address, creature_anim::Motion> motion_;
  std::function<creature_anim::ClipId(Address, Raw)> clips_;
  std::function<std::uint64_t(Address, unsigned)> player_read_;
  creature_water::Service *water_ = nullptr;
  creature_water::Address water_world_ = 0;
  bool water_grids_bound_ = false;
  bool water_chunks_bound_ = false;
  std::vector<WaterChunk> water_chunks_;
  bool touches_water(const Vec3 &, Raw, bool padded) const;
public:
  explicit Bundle(world_queries::Level &level) : level_(level) {}
  Bundle(const Bundle &) = delete;
  Bundle &operator=(const Bundle &) = delete;
  void set_camera(Raw slot, Camera camera);
  // Updates only the net_getCamFlag word of an already bound snapshot: offline
  // it is "the camera is not in follow mode" (movement-camera.md 7.2, mode != 1).
  void set_camera_flag(Raw slot, Raw flag);
  void set_local_slot(Raw slot) { local_slot_ = slot; }
  // The resolver maps the model-definition resource pointer to a registered
  // clip. research-player-model 7f13899 requires slot order from the selected
  // model's animation lump; duplicate names must not collapse table slots.
  void bind_animation(creature_anim::Runtime &runtime,
                      std::function<creature_anim::ClipId(Address, Raw)> clips);
  void bind_actor(Address actor, creature_anim::StateId state);
  // Supply the shared World's read method when its actor authority is ready.
  // Values are read on every call, so equip and action transitions are visible.
  void bind_player_reader(std::function<std::uint64_t(Address, unsigned)> read);
  AnimationFields animation_fields(Address actor) const;
  // Opt in only when the owner supplies runtime ripple grids for wet chunks.
  void bind_water(creature_water::Service &service,
                  creature_water::Address world, bool runtime_grids_bound = false);
  void bind_water_chunks(std::vector<WaterChunk> chunks);
  // Other production callbacks (dialog, resources, etc.) may be supplied in
  // base. Never pass flat_fixture_services as base for a real level.
  combat_runtime::Services services(combat_runtime::Services base = {});
};
} // namespace champions::simulation::rta::player_services
