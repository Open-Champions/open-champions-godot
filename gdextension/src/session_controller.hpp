#pragma once
#include "champions/frame_clock.hpp"
#include "champions/combat_play.hpp"
#include "champions/combat_play_effects.hpp"
#include "champions/level_play.hpp"
#include "champions/native_save.hpp"
#include "champions/world_geometry.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <memory>

// Godot is presentation only: callers submit integer command bits, never
// host-float positions, velocities or hit points. Elapsed time only enters
// the non-authoritative host accumulator, which submits integer commands.
class ChampionsSessionController : public godot::RefCounted {
  GDCLASS(ChampionsSessionController, godot::RefCounted)
  std::unique_ptr<champions::simulation::rta::native_save::Scene> control_;
  std::unique_ptr<champions::simulation::rta::combat_play::Session> combat_;
  // Full fixture only: native-fx-2 EffectSimulation in every ready tick. Declared
  // after combat_ so it is destroyed first; always dropped before combat_ changes.
  std::unique_ptr<champions::simulation::rta::combat_play::Effects> effects_;
  std::shared_ptr<const champions::rta::fx::skills::RecipeConstants> fx_constants_;
  godot::String fx_constants_source_;
  godot::Ref<godot::RefCounted> fx_bridge_;
  void drop_effects();
  void make_effects();
  std::unique_ptr<champions::simulation::rta::level_play::Session> level_;
  champions::simulation::rta::session_control::Setup combat_setup_;
  champions::simulation::rta::session_control::Observation active_observation() const;
  champions::simulation::rta::session_control::Observation active_advance(uint32_t);
  godot::Dictionary level_observation() const;
  champions::simulation::rta::session_control::Setup active_setup() const;
  godot::Dictionary combat_observation() const;
  champions::presentation::rta::FrameClock clock_;
  champions::simulation::rta::session_control::Observation current_;
  godot::Vector3 previous_position_, current_position_;
  double previous_facing_ = 0, current_facing_ = 0;
  void
  cache_state(const champions::simulation::rta::session_control::Observation &,
              bool snap);
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
  godot::Dictionary write_save_file(const godot::String &, bool direct);
  godot::Dictionary observation() const;
  void snap();
  godot::Dictionary present_observation(
      const champions::simulation::rta::session_control::Observation &) const;

protected:
  static void _bind_methods();

public:
  ChampionsSessionController();
  ~ChampionsSessionController() override;
  godot::Dictionary advance(int64_t command_mask);
  godot::Dictionary advance_time(int64_t elapsed_usec, int64_t command_mask);
  godot::Dictionary get_presentation() const;
  // Collapse presentation history at authoritative discontinuities.
  godot::Dictionary snap_presentation();
  godot::Dictionary start_play_session(bool combat_fixture = false, bool level_fixture = false, bool animated_fixture = false,
                                      bool full_fixture = false);
  // Full combat fixture (combat_play::Fixture::Full): pad buttons and Holy Strike.
  godot::Dictionary combat_attack(bool held);
  godot::Dictionary combat_release_attack();
  godot::Dictionary combat_block(bool held);
  godot::Dictionary combat_cast_holy_strike();
  // fx_skill_recipes.py --json (user's) or the labelled synthetic fixture.
  godot::Dictionary set_effect_recipe_constants_json(const godot::String &json, const godot::String &source);
  // A ChampionsFxBridge whose world the Full session's EffectSimulation draws
  // into (null: headless). Presentation only; seeds and HP do not depend on it.
  godot::Dictionary attach_effect_bridge(godot::Object *bridge);
  // Object draw slots for one rendered frame (HolyStrike_draw glows).
  godot::Dictionary draw_effect_slots();
  godot::Dictionary level_gate_open();
  godot::Dictionary level_gate_confirm(const godot::String &);
  godot::Dictionary level_request(const godot::String &);
  godot::Dictionary combat_hit_event(int64_t slot, int64_t mask);
  godot::Dictionary get_play_capabilities() const;
  godot::Dictionary reset_session();
  godot::Dictionary set_recovery_test(bool enabled);
  godot::Dictionary set_facing_test(bool enabled);
  godot::Dictionary set_animation_cache_test(bool enabled);
  godot::Dictionary animation_cache_command(int64_t kind, int64_t clip);
  godot::Dictionary set_paused(bool paused);
  godot::Dictionary release_input();
  godot::Dictionary stop_session();
  godot::Dictionary get_observation() const;
  godot::Dictionary get_terrain() const;
  godot::Dictionary save_file(const godot::String &path);
  godot::Dictionary save_snapshot_file(const godot::String &path);
  godot::Dictionary prepare_save_file(const godot::String &path);
  godot::Dictionary load_terrain_file(const godot::String &path);
  godot::Dictionary prepare_terrain_file(const godot::String &path);
  godot::Dictionary commit_terrain_file(int64_t token);
  bool cancel_terrain_file(int64_t token);
};
