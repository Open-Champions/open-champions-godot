#pragma once
#include "champions/frame_clock.hpp"
#include "champions/arena_executable.hpp"
#include "champions/combat_play.hpp"
#include "champions/core_state.hpp"
#include "champions/combat_play_effects.hpp"
#include "champions/level_play.hpp"
#include "champions/native_save.hpp"
#include "champions/play_fixture_save.hpp"
#include "champions/player_state.hpp"
#include "champions/world_geometry.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <array>
#include <godot_cpp/variant/array.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace champions::simulation::rta::script_commands {
class Host;
}
// Godot is presentation only: callers submit integer command bits, never
// host-float positions, velocities or hit points. Elapsed time only enters
// the non-authoritative host accumulator, which submits integer commands.
class ChampionsSessionController : public godot::RefCounted {
  GDCLASS(ChampionsSessionController, godot::RefCounted)
  std::unique_ptr<champions::simulation::rta::native_save::Scene> control_;
  // Combat and level fixtures behind the journaled fixture-continuation facades
  // (play_fixture_save.hpp); the Full fixture's EffectSimulation lives in the
  // combat scene and runs in every ready tick.
  std::unique_ptr<champions::simulation::rta::fixture_save::CombatScene> combat_;
  std::shared_ptr<const champions::rta::fx::skills::RecipeConstants> fx_constants_;
  champions::simulation::rta::fixture_save::ConstantsIdentity fx_constants_identity_;
  godot::String fx_constants_source_;
  godot::Ref<godot::RefCounted> fx_bridge_;
  void attach_effects();
  std::optional<champions::simulation::rta::fixture_save::Bytes> loaded_presentation_;
  void apply_loaded_presentation();
  godot::Dictionary prepare_fixture_save(const godot::Ref<godot::FileAccess> &file);
  const champions::simulation::rta::combat_play::Effects *effects() const {
    return combat_ ? combat_->effects() : nullptr;
  }
  std::unique_ptr<champions::simulation::rta::fixture_save::LevelScene> level_;
  // Opt-in level script. Absent until a caller passes a script image. Counts
  // stay off the observation dictionary.
  std::unique_ptr<champions::simulation::rta::script_commands::Host> scripts_;
  std::optional<champions::simulation::rta::player_state::Tables> player_tables_;
  std::optional<champions::simulation::rta::player_state::Record> player_;
  void bind_player_to_scripts(champions::simulation::rta::script_commands::Host &);
  champions::simulation::rta::player_state::Record observed_player() const;
  void pump_scripts(std::size_t ready_ticks_before);
  // The level script's draws and the play session's World are one stream
  // (script_commands::SeedPolicy::OfflineSession, research
  // t3/research-script-session 6841b8a docs/script-session.md section 1): it
  // starts at the play session's configured shared seed when the script
  // attaches (the port's stand-in for the offline session's clock seed), the
  // load's draws come first, and play starts its World from where they left
  // it. script_world_ is the World's state at that hand-over; a World that
  // draws on its own, or a session that replaces it, unpairs them, and getRand
  // is refused from then on.
  std::unique_ptr<champions::simulation::rta::SharedRandom> script_seed_;
  std::optional<std::uint32_t> script_world_;
  bool script_seed_unpaired_ = false;
  void unpair_script_seed();
  void unpair_script_seed_if_paired();
  // The owner's executable, kept by load_arena_tables: the level script's
  // object factory reads its strings (script_commands::Host::set_executable).
  std::shared_ptr<const champions::simulation::rta::arena_modes::Executable> executable_;
  struct DialogData;
  std::shared_ptr<DialogData> dialog_data_;
  bool dialog_use_pending_ = false;
  int64_t dialog_talk_pending_ = -1;
  uint32_t dialog_pad_ = 0;
  // queue_count, status, position of voice stream 1 (dialog_voice_state).
  std::optional<std::array<int64_t, 3>> dialog_voice_;
  godot::String dialog_last_use_;
  uint32_t dialog_gate(uint32_t mask) const;
  // Text commands (session_controller_text.cpp, docs/STRINGS.md): the game
  // folder's GTEXT.LMP tables, the level lump's tables, the help chime group.
  std::vector<std::vector<std::uint8_t>> text_tables_, level_text_;
  std::optional<std::uint8_t> help_chime_;
  bool help_paused_ = false;
  static void bind_text_methods();
  void apply_text(champions::simulation::rta::script_commands::Host &);
  void note_help_box();
  void release_help_pause();
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
  godot::String start_objects_path_;
  std::vector<uint8_t> start_objects_;
  std::string start_level_name_;
  std::vector<std::string> arena_names_a_;
  godot::Dictionary terrain_;
  godot::String terrain_name_ = "Practice ground";
  static godot::Dictionary
  present_terrain(const champions::simulation::rta::world_queries::LevelData &);
  static godot::Dictionary present_geometry(
      const champions::presentation::rta::world_geometry::Geometry &, bool build_display = true);
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
  godot::Dictionary combat_spawn_creature(int64_t own_class);
  godot::Dictionary combat_drop_potion(int64_t kind);
  static godot::PackedStringArray own_creature_classes();
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
  // Level script for the loaded level. Empty bytes are refused and leave the
  // session observation unchanged. A loaded script runs its level entry now;
  // later ready ticks resume deferred calls.
  // carry_campaign: the new host starts from the previous host's quest and
  // mission campaign image (quests::Campaign::save), as the binder record
  // and quest variables persist across a level change. Off by default, so
  // each level's script counts stay independent of load order.
  // dialog (docs/DIALOG.md), when not empty, attaches the NPC dialog machine
  // before the level entry runs: level (String), dbin (Dictionary of entry
  // name -> PackedByteArray), stream_directory (ntsc_dialog.bin bytes),
  // vat_size (int), text (gtext.uni bytes) and font (gamefontmed.fnt bytes).
  godot::Dictionary enable_level_script(const godot::PackedByteArray &script,
                                        const godot::PackedByteArray &objects,
                                        bool carry_campaign = false,
                                        const godot::Dictionary &dialog = godot::Dictionary());
  // The use button for slot 0, taken at the next ready tick (Player_interact).
  godot::Dictionary dialog_use();
  // Pad bits pressed this frame for the run task (0x1000 up, 0x4000 down,
  // 0x40 confirm), taken at the next ready tick.
  godot::Dictionary dialog_press(int64_t pad_bits);
  // dialog_drawTask for one displayed frame: the box and glyph draws.
  godot::Dictionary dialog_frame(int64_t text_option);
  // Diagnostic, not an original routine: Creature_interact on talker `index`
  // from 30 units in front of it, at the next ready tick.
  godot::Dictionary dialog_talk_to(int64_t index);
  // Not in the original: leave a conversation a refused script stalled.
  godot::Dictionary dialog_leave();
  godot::Dictionary dialog_status() const;
  // Calls the dialog made outside its own state since the last take: the
  // voice (stream_play, stream_stop, clear_hold on stream 1), the shop, the
  // freeze and the camera words, for the presentation.
  godot::Array dialog_events();
  // What the run task reads from voice stream 1 (queue_count, status,
  // position) from the presentation's native audio; used from the next
  // ready tick. Without it every line reads as having no audio entry.
  godot::Dictionary dialog_voice_state(const godot::Dictionary &voice);
  godot::Dictionary disable_level_script();
  godot::Dictionary script_command_counts() const;
  // Every .uni table of GTEXT.LMP (registered first) and the sound group of
  // fx.lmp/butn_message.anm (byte +0x3c; -1 when absent). Applies to every
  // later level script, and to the current one.
  godot::Dictionary set_text_sources(const godot::Array &tables, int64_t help_chime_group);
  // The level lump's .uni tables, registered after GTEXT's for the next script.
  godot::Dictionary set_level_text(const godot::Array &tables);
  // The HUD text state the five commands wrote, for drawing.
  godot::Dictionary text_hud() const;
  // help_task for one displayed frame; confirm is the authored confirm input.
  // An open box pauses the offline session until it closes.
  godot::Dictionary help_frame(bool confirm);
  godot::Dictionary set_player_character(const godot::Dictionary &character);
  godot::Dictionary save_player_companion(const godot::String &path) const;
  godot::Dictionary load_player_companion(const godot::String &path);
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
  godot::Dictionary prepare_terrain_file(const godot::String &path, bool build_display = true);
  // Level-spawn story start for the next terrain prepared from exactly `path`:
  // that level's objects.ob bytes (level_flow::story_start_ground). Empty
  // bytes or another path keep the historical origin start and check.
  // level_name is the level's own name, used only to look it up in the
  // mode-A arena table read by load_arena_tables.
  godot::Dictionary set_terrain_start_objects(const godot::String &path,
                                              const godot::PackedByteArray &objects,
                                              const godot::String &level_name = godot::String());
  // The arena name tables from the owner's executable, read at run time
  // (arena_modes::read_names). Kept in memory only; the result is counts.
  godot::Dictionary load_arena_tables(const godot::String &executable_path);
  godot::Dictionary commit_terrain_file(int64_t token);
  bool cancel_terrain_file(int64_t token);
};
