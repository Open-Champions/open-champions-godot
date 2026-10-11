#pragma once
#include "champions/session_control.hpp"
#include "champions/world_geometry.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>

// Godot is presentation only: callers submit integer command bits, never
// host-float positions, velocities, hit points or elapsed simulation time.
class ChampionsSessionController : public godot::RefCounted {
  GDCLASS(ChampionsSessionController, godot::RefCounted)
  std::unique_ptr<champions::simulation::rta::session_control::Controller>
      control_;
  struct PendingTerrain;
  std::unique_ptr<PendingTerrain> pending_;
  int64_t next_candidate_ = 1;
  godot::Dictionary terrain_;
  godot::String terrain_name_ = "Practice ground";
  static godot::Dictionary
  present_terrain(const champions::simulation::rta::world_queries::LevelData &);
  static godot::Dictionary present_geometry(
      const champions::presentation::rta::world_geometry::Geometry &);
  static void prepare_meshes(godot::Dictionary &);
  godot::Dictionary observation() const;
  godot::Dictionary present_observation(
      const champions::simulation::rta::session_control::Observation &) const;

protected:
  static void _bind_methods();

public:
  ChampionsSessionController();
  ~ChampionsSessionController() override;
  godot::Dictionary advance(int64_t command_mask);
  godot::Dictionary reset_session();
  godot::Dictionary set_paused(bool paused);
  godot::Dictionary release_input();
  godot::Dictionary stop_session();
  godot::Dictionary get_observation() const;
  godot::Dictionary get_terrain() const;
  godot::Dictionary load_terrain_file(const godot::String &path);
  godot::Dictionary prepare_terrain_file(const godot::String &path);
  godot::Dictionary commit_terrain_file(int64_t token);
  bool cancel_terrain_file(int64_t token);
};
