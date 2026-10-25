#pragma once
#include "champions/hub_services.hpp"
#include "champions/frame_clock.hpp"
#include "champions/arena_executable.hpp"
#include "champions/campaign_fight.hpp"
#include "champions/combat_play.hpp"
#include "champions/session_combat_runtime_adapter.hpp"
#include "champions/core_state.hpp"
#include "champions/combat_play_effects.hpp"
#include "champions/level_play.hpp"
#include "champions/native_save.hpp"
#include "champions/play_fixture_save.hpp"
#include "champions/player_state.hpp"
#include "champions/level_flow.hpp"
#include "champions/world_geometry.hpp"
#include "champions/player_services.hpp"
#include "champions/real_world.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <array>
#include <godot_cpp/variant/array.hpp>
#include <cstdint>
#include <memory>
#include <map>
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
  // The ordinary play session owns the one native address authority. Its
  // combat adapter is prepared for registered level-factory objects, but
  // gameplay ticks still require the separately ported production services.
  std::shared_ptr<champions::simulation::rta::WorldAuthority> world_authority_;
  std::unique_ptr<champions::simulation::rta::combat_runtime::World> world_runtime_;
  std::unique_ptr<champions::simulation::rta::combat_runtime::SessionAdapter> world_adapter_;
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
  std::unique_ptr<champions::simulation::rta::world_queries::Level> player_service_level_;
  std::unique_ptr<champions::real_world::WaterImage> player_water_image_;
  std::unique_ptr<champions::simulation::rta::player_services::Bundle> player_services_;
  // The first real enemy fight (native/include/champions/campaign_fight.hpp):
  // the session Player and the level factory's WarBoar-family actors in the one
  // shared World. Declared before the host so the host's objects are released
  // first (a host reset must precede reset_fight()).
  std::unique_ptr<champions::simulation::rta::campaign_fight::Fight> fight_;
  // Opt-in level script. Absent until a caller passes a script image. Counts
  // stay off the observation dictionary.
  std::unique_ptr<champions::simulation::rta::script_commands::Host> scripts_;
  std::string fight_level_;
  void reset_fight();
  void start_fight(const champions::simulation::rta::session_control::Observation &);
  void apply_fight(champions::simulation::rta::session_control::Observation &) const;
  godot::Dictionary fight_observation() const;
public:
  // DIAGNOSTIC, not an original routine (scripted runs only).
  godot::Dictionary fight_place_player(const godot::Vector3 &position, int64_t facing);
  // Save/load: put the saved player position and health back (not an original routine).
  godot::Dictionary fight_restore_player(const godot::Vector3 &position, int64_t facing, int64_t hp_raw);
private:
  // Parsed records belong to this level's script image and are replaced with it.
  std::vector<champions::simulation::rta::level_flow::Record> script_level_objects_;
  struct LiveTrigger {
    std::size_t record = 0;
    champions::simulation::rta::level_flow::Trigger body;
    bool refused = false;
    LiveTrigger(std::size_t i, champions::simulation::rta::level_flow::Trigger t, bool r)
        : record(i), body(std::move(t)), refused(r) {}
  };
  std::vector<LiveTrigger> live_triggers_;
  std::array<std::vector<std::size_t>, 15> trigger_lists_;
  unsigned trigger_list_cursor_ = 0;
  std::size_t trigger_frame_ = 0;
  std::string trigger_refusal_;
  std::size_t trigger_samples_ = 0, trigger_dispatches_ = 0,
              trigger_missing_ = 0;
  std::size_t trigger_queued_dispatches_ = 0;
  std::size_t trigger_queued_record_ = 0;
  std::string trigger_queued_destination_;
  std::string trigger_level_name_;
  std::map<std::string, std::vector<champions::simulation::rta::level_flow::TriggerState>> trigger_states_;
  void run_level_triggers();
  std::map<std::size_t, champions::simulation::rta::hub_services::Savepoint> hub_savepoints_;
  // Presentation word retained when an opt-in level script host is replaced.
  std::uint32_t script_fade_ = 0;
  bool ui_frame_driven_ = false;
  void remember_script_fade();
  std::optional<champions::simulation::rta::player_state::Tables> player_tables_;
  std::optional<champions::simulation::rta::player_state::Record> player_;
  // Presentation identity: advances whenever a session player record is replaced.
  std::uint64_t player_record_revision_ = 0;
  void bind_player_to_scripts(champions::simulation::rta::script_commands::Host &,
                              const champions::simulation::rta::player_state::Record &);
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
  // dialog_loadBox's word 0x4fe538 survives from one level to the next (docs/spec/dialog-gaps.md 4).
  bool dialog_box_loaded_ = false;
  uint32_t dialog_gate(uint32_t mask) const;
  // Text commands (session_controller_text.cpp, docs/STRINGS.md): the game
  // folder's GTEXT.LMP tables, the level lump's tables, the help chime group.
  std::vector<std::vector<std::uint8_t>> text_tables_, level_text_;
  std::optional<std::uint8_t> help_chime_;
  bool help_paused_ = false;
  static void bind_text_methods();
  static void bind_hub_methods();
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
  std::string start_previous_level_, start_entrance_;
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
  godot::Dictionary combat_select_disease_bolt(int64_t rank = 1);
  godot::Dictionary combat_press_disease_bolt(bool held = true);
  godot::Dictionary combat_release_disease_bolt();
  godot::Dictionary combat_spawn_creature(int64_t own_class);
  godot::Dictionary combat_spawn_mounted_horseman(int64_t variant);
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
                                        const godot::Dictionary &dialog = godot::Dictionary(),
                                        std::int64_t music_volume_bits = -1,
                                        const godot::String &level_name = godot::String(),
                                        const godot::PackedByteArray &balance = godot::PackedByteArray(),
                                        const godot::PackedByteArray &world = godot::PackedByteArray(),
                                        const godot::Dictionary &resource_lumps = godot::Dictionary());
  godot::Array combat_resource_lump_names() const;
  // The use button for slot 0, taken at the next ready tick (Player_interact).
  godot::Dictionary dialog_use();
  godot::Array hub_service_targets() const;
  godot::Dictionary hub_service_use(int64_t index, bool card_busy);
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
  // The conversation camera (docs/spec/dialog-gaps.md 2) and the outcome of one dialog_isOpen site.
  godot::Dictionary dialog_camera() const;
  godot::Dictionary dialog_site_gate(const godot::String &site, const godot::Dictionary &conditions) const;
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
  godot::Dictionary script_level_request() const;
  godot::Dictionary script_mission_menu_state() const;
  godot::Dictionary script_mission_menu_tick(int64_t pad_bits, int64_t fade);
  godot::Array script_exit_targets() const;
  godot::Dictionary inspect_exit_records(const godot::PackedByteArray &) const;
  godot::Dictionary script_trigger_status() const;
  godot::Array script_player_markers() const;
  godot::Dictionary script_run_exit(int64_t index);
  // Every .uni table of GTEXT.LMP (registered first) and the sound group of
  // fx.lmp/butn_message.anm (byte +0x3c; -1 when absent). Applies to every
  // later level script, and to the current one.
  godot::Dictionary set_text_sources(const godot::Array &tables, int64_t help_chime_group);
  // The level lump's .uni tables, registered after GTEXT's for the next script.
  godot::Dictionary set_level_text(const godot::Array &tables);
  // The HUD text state the five commands wrote, for drawing.
  godot::Dictionary text_hud() const;
  // Session step 13 (docs/UI_TEXT_GAPS.md): once per displayed frame, after the
  // draw. Returns the g_uiPulse / g_uiFadeIn the frame drew with (raw floats),
  // then steps them. Until a presentation calls set_ui_frame_driver(true), every
  // ready tick counts as one displayed frame (the documented one tick per frame).
  godot::Dictionary ui_frame();
  godot::Dictionary set_ui_frame_driver(bool driven);
  // The visible units of a text key without advancing the miss ring.
  godot::PackedInt32Array text_key_units(const godot::String &key) const;
  godot::Dictionary text_lookup(const godot::String &key) const;
  // help_task for one displayed frame; confirm is the authored confirm input.
  // An open box pauses the offline session until it closes.
  godot::Dictionary help_frame(bool confirm);
  godot::Dictionary set_player_character(const godot::Dictionary &character);
  godot::Dictionary save_player_companion(const godot::String &path) const;
  godot::Dictionary load_player_companion(const godot::String &path);
  godot::Dictionary save_campaign_bytes();
  godot::Dictionary load_campaign_bytes(const godot::PackedByteArray &bytes);
  godot::Dictionary reset_session();
  godot::Dictionary set_recovery_test(bool enabled);
  godot::Dictionary set_facing_test(bool enabled);
  godot::Dictionary set_animation_cache_test(bool enabled);
  godot::Dictionary animation_cache_command(int64_t kind, int64_t clip);
  godot::Dictionary set_paused(bool paused);
  godot::Dictionary release_input();
  godot::Dictionary stop_session();
  godot::Dictionary get_observation() const;
  godot::Dictionary probe_full_walk_weight_test();
  godot::Dictionary get_terrain() const;
  godot::Dictionary save_file(const godot::String &path);
  godot::Dictionary save_snapshot_file(const godot::String &path);
  godot::Dictionary prepare_save_file(const godot::String &path, bool reuse_loaded_level = false);
  godot::Dictionary load_terrain_file(const godot::String &path);
  void set_player_camera(int64_t slot, int64_t yaw, const godot::Variant &flag,
                         const godot::Vector3 &position,
                         const godot::PackedVector3Array &side_normals);
  champions::simulation::rta::player_services::Bundle *player_services() noexcept {
    return player_services_.get();
  }
  godot::Dictionary prepare_terrain_file(const godot::String &path, bool build_display = true);
  // Level-spawn story start for the next terrain prepared from exactly `path`:
  // that level's objects.ob bytes (level_flow::story_start_ground). Empty
  // bytes or another path keep the historical origin start and check.
  // level_name is the level's own name, used only to look it up in the
  // mode-A arena table read by load_arena_tables.
  godot::Dictionary set_terrain_start_objects(const godot::String &path,
                                              const godot::PackedByteArray &objects,
                                              const godot::String &level_name = godot::String(),
                                              const godot::String &previous = godot::String(),
                                              const godot::String &entrance = godot::String());
  // The arena name tables from the owner's executable, read at run time
  // (arena_modes::read_names). Kept in memory only; the result is counts.
  godot::Dictionary load_arena_tables(const godot::String &executable_path);
  godot::Dictionary commit_terrain_file(int64_t token);
  bool cancel_terrain_file(int64_t token);
};
