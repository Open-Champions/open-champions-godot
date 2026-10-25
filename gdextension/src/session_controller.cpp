#include "session_controller.hpp"
#include "champions/ui_text_gaps.hpp"
#include "champions/native_audio.hpp"
#include "champions/script_commands.hpp"
#include "champions/ui_layout.hpp"
#include "champions/ui_text_layout.hpp"
#include "champions/terrain_file.hpp"
#include "champions/level_flow.hpp"
#include "champions/arena_executable.hpp"
#include "champions/hud_extras.hpp"
#include "fx_bridge.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace godot;
namespace rta = champions::simulation::rta;
namespace {
rta::session_control::Config scene_config() {
  rta::session_control::Config config;
  config.movement = rta::session_control::MovementPolicy::DryCreature;
  config.max_ready_ticks = rta::session_control::MAX_CREATURE_READY_TICKS;
  return config;
}
const char *combat_policy(const rta::fixture_save::CombatScene &s) {
  return s.full() ? rta::combat_play::FULL_POLICY : s.animated() ? rta::combat_play::ANIMATED_POLICY : rta::combat_play::POLICY;
}
float display_float(rta::Raw raw) {
  float value;
  static_assert(sizeof(value) == sizeof(raw));
  std::memcpy(&value, &raw, sizeof(raw));
  if (!std::isfinite(value))
    throw std::runtime_error("The scene cannot display this numeric value.");
  return value;
}
Dictionary failure(const std::exception &error) {
  Dictionary out;
  out["ok"] = false;
  out["error"] = String::utf8(error.what());
  return out;
}
// US v2.00 mission table, research-story-route and docs/QUESTS.md. Read the
// owner's executable at run time; no retail table or text enters the project.
std::vector<rta::quests::MissionRow> mission_rows(const rta::arena_modes::Executable &exe) {
  constexpr std::uint32_t table = 0x4fe540u, stride = 0x34u;
  std::vector<rta::quests::MissionRow> rows;
  rows.reserve(13);
  for (std::uint32_t i = 0; i < 13; ++i) {
    const auto base = table + i * stride;
    auto name = [&](std::uint32_t offset) {
      const auto ptr = exe.word(base + offset);
      return ptr ? exe.string(ptr) : std::string();
    };
    rows.push_back({name(0x08), name(0x0c), name(0x10), name(0x14),
                    std::int32_t(exe.word(base + 0x20)), std::int32_t(exe.word(base + 0x24)),
                    std::int32_t(exe.word(base + 0x28))});
  }
  if (rows.front().story != "blood1")
    throw std::runtime_error("The selected executable does not establish the first story mission.");
  return rows;
}
void quad(PackedVector3Array &v, double x0, double y0, double x1, double y1,
          double z) {
  for (const Vector3 p :
       {Vector3(x0, y0, z), Vector3(x1, y0, z), Vector3(x1, y1, z),
        Vector3(x0, y0, z), Vector3(x1, y1, z), Vector3(x0, y1, z)})
    v.push_back(p);
}
} // namespace
struct ChampionsSessionController::PendingTerrain {
  std::unique_ptr<rta::native_save::Scene> control;
  std::unique_ptr<rta::world_queries::Level> player_service_level;
  std::unique_ptr<champions::real_world::WaterImage> player_water_image;
  std::unique_ptr<rta::player_services::Bundle> player_services;
  // A fixture continuation (play_fixture_save.hpp) instead of a dry scene.
  std::unique_ptr<rta::fixture_save::CombatScene> combat;
  std::unique_ptr<rta::fixture_save::LevelScene> level;
  std::optional<rta::fixture_save::Bytes> presentation;
  rta::session_control::Setup setup;
  Dictionary terrain, observation;
  String name;
  int64_t token = 0;
};

void ChampionsSessionController::_bind_methods() {
  ADD_SIGNAL(MethodInfo("play_audio_tick", PropertyInfo(Variant::DICTIONARY, "packet")));
  ADD_SIGNAL(MethodInfo("combat_visual_tick", PropertyInfo(Variant::DICTIONARY, "packet")));
  ClassDB::bind_method(D_METHOD("start_play_session", "combat_fixture", "level_fixture", "animated_fixture", "full_fixture"),
                       &ChampionsSessionController::start_play_session, DEFVAL(false), DEFVAL(false), DEFVAL(false), DEFVAL(false));
  ClassDB::bind_method(D_METHOD("combat_attack", "held"), &ChampionsSessionController::combat_attack, DEFVAL(true));
  ClassDB::bind_method(D_METHOD("combat_release_attack"), &ChampionsSessionController::combat_release_attack);
  ClassDB::bind_method(D_METHOD("combat_block", "held"), &ChampionsSessionController::combat_block);
  ClassDB::bind_method(D_METHOD("combat_cast_holy_strike"), &ChampionsSessionController::combat_cast_holy_strike);
  ClassDB::bind_method(D_METHOD("combat_select_disease_bolt", "rank"), &ChampionsSessionController::combat_select_disease_bolt, DEFVAL(1));
  ClassDB::bind_method(D_METHOD("combat_press_disease_bolt", "held"), &ChampionsSessionController::combat_press_disease_bolt, DEFVAL(true));
  ClassDB::bind_method(D_METHOD("combat_release_disease_bolt"), &ChampionsSessionController::combat_release_disease_bolt);
  ClassDB::bind_method(D_METHOD("combat_spawn_creature", "own_class"), &ChampionsSessionController::combat_spawn_creature);
  ClassDB::bind_method(D_METHOD("combat_spawn_mounted_horseman", "variant"), &ChampionsSessionController::combat_spawn_mounted_horseman);
  ClassDB::bind_method(D_METHOD("combat_drop_potion", "kind"), &ChampionsSessionController::combat_drop_potion);
  ClassDB::bind_static_method("ChampionsSessionController", D_METHOD("own_creature_classes"), &ChampionsSessionController::own_creature_classes);
  ClassDB::bind_method(D_METHOD("set_effect_recipe_constants_json", "json", "source"),
                       &ChampionsSessionController::set_effect_recipe_constants_json);
  ClassDB::bind_method(D_METHOD("attach_effect_bridge", "bridge"), &ChampionsSessionController::attach_effect_bridge);
  ClassDB::bind_method(D_METHOD("draw_effect_slots"), &ChampionsSessionController::draw_effect_slots);
  ClassDB::bind_method(D_METHOD("level_gate_open"), &ChampionsSessionController::level_gate_open);
  ClassDB::bind_method(D_METHOD("level_gate_confirm", "destination"), &ChampionsSessionController::level_gate_confirm);
  ClassDB::bind_method(D_METHOD("level_request", "destination"), &ChampionsSessionController::level_request);
  ClassDB::bind_method(D_METHOD("combat_hit_event", "slot", "mask"),
                      &ChampionsSessionController::combat_hit_event);
  ClassDB::bind_method(D_METHOD("get_play_capabilities"),
                       &ChampionsSessionController::get_play_capabilities);
  ClassDB::bind_method(D_METHOD("enable_level_script", "script", "objects", "carry_campaign", "dialog", "music_volume_bits", "level_name", "balance", "world", "resource_lumps"),
                       &ChampionsSessionController::enable_level_script, DEFVAL(false), DEFVAL(Dictionary()), DEFVAL(int64_t(-1)),
                       DEFVAL(String()), DEFVAL(PackedByteArray()), DEFVAL(PackedByteArray()), DEFVAL(Dictionary()));
  ClassDB::bind_method(D_METHOD("combat_resource_lump_names"),
                       &ChampionsSessionController::combat_resource_lump_names);
  ClassDB::bind_method(D_METHOD("dialog_use"), &ChampionsSessionController::dialog_use);
  ClassDB::bind_method(D_METHOD("dialog_press", "pad_bits"), &ChampionsSessionController::dialog_press);
  ClassDB::bind_method(D_METHOD("dialog_frame", "text_option"), &ChampionsSessionController::dialog_frame, DEFVAL(1));
  ClassDB::bind_method(D_METHOD("dialog_leave"), &ChampionsSessionController::dialog_leave);
  ClassDB::bind_method(D_METHOD("dialog_talk_to", "index"), &ChampionsSessionController::dialog_talk_to);
  ClassDB::bind_method(D_METHOD("dialog_status"), &ChampionsSessionController::dialog_status);
  ClassDB::bind_method(D_METHOD("dialog_camera"), &ChampionsSessionController::dialog_camera);
  ClassDB::bind_method(D_METHOD("dialog_gate", "site", "conditions"), &ChampionsSessionController::dialog_site_gate,
                       DEFVAL(Dictionary()));
  ClassDB::bind_method(D_METHOD("dialog_events"), &ChampionsSessionController::dialog_events);
  ClassDB::bind_method(D_METHOD("dialog_voice_state", "voice"), &ChampionsSessionController::dialog_voice_state);
  ClassDB::bind_method(D_METHOD("fight_restore_player", "position", "facing", "hp_raw"),
                       &ChampionsSessionController::fight_restore_player);
  // DIAGNOSTIC ONLY (unchecked placement; scripted test runs and the .fight load).
  ClassDB::bind_method(D_METHOD("fight_place_player", "position", "facing"),
                       &ChampionsSessionController::fight_place_player);
  ClassDB::bind_method(D_METHOD("disable_level_script"),
                       &ChampionsSessionController::disable_level_script);
  ClassDB::bind_method(D_METHOD("script_command_counts"),
                       &ChampionsSessionController::script_command_counts);
  ClassDB::bind_method(D_METHOD("script_level_request"), &ChampionsSessionController::script_level_request);
  ClassDB::bind_method(D_METHOD("script_mission_menu_state"), &ChampionsSessionController::script_mission_menu_state);
  ClassDB::bind_method(D_METHOD("script_mission_menu_tick", "pad_bits", "fade"),
                       &ChampionsSessionController::script_mission_menu_tick);
  ClassDB::bind_method(D_METHOD("script_exit_targets"), &ChampionsSessionController::script_exit_targets);
  ClassDB::bind_method(D_METHOD("inspect_exit_records", "objects"), &ChampionsSessionController::inspect_exit_records);
  ClassDB::bind_method(D_METHOD("script_trigger_status"), &ChampionsSessionController::script_trigger_status);
  ClassDB::bind_method(D_METHOD("script_player_markers"), &ChampionsSessionController::script_player_markers);
  ClassDB::bind_method(D_METHOD("script_run_exit", "index"), &ChampionsSessionController::script_run_exit);
  bind_text_methods();
  bind_hub_methods();
  ClassDB::bind_method(D_METHOD("set_player_character", "character"),
                       &ChampionsSessionController::set_player_character);
  ClassDB::bind_method(D_METHOD("save_player_companion", "path"),
                       &ChampionsSessionController::save_player_companion);
  ClassDB::bind_method(D_METHOD("load_player_companion", "path"),
                       &ChampionsSessionController::load_player_companion);
  ClassDB::bind_method(D_METHOD("save_campaign_bytes"),
                       &ChampionsSessionController::save_campaign_bytes);
  ClassDB::bind_method(D_METHOD("load_campaign_bytes", "bytes"),
                       &ChampionsSessionController::load_campaign_bytes);
  ClassDB::bind_method(D_METHOD("save_file", "path"),
                       &ChampionsSessionController::save_file);
  ClassDB::bind_method(D_METHOD("save_snapshot_file", "path"),
                       &ChampionsSessionController::save_snapshot_file);
  ClassDB::bind_method(D_METHOD("prepare_save_file", "path", "reuse_loaded_level"),
                       &ChampionsSessionController::prepare_save_file, DEFVAL(false));
  ClassDB::bind_method(D_METHOD("advance_time", "elapsed_usec", "command_mask"),
                       &ChampionsSessionController::advance_time);
  ClassDB::bind_method(D_METHOD("get_presentation"),
                       &ChampionsSessionController::get_presentation);
  ClassDB::bind_method(D_METHOD("snap_presentation"),
                       &ChampionsSessionController::snap_presentation);
  ClassDB::bind_method(D_METHOD("advance", "command_mask"),
                       &ChampionsSessionController::advance);
  ClassDB::bind_method(D_METHOD("set_animation_cache_test", "enabled"),
                       &ChampionsSessionController::set_animation_cache_test);
  ClassDB::bind_method(D_METHOD("animation_cache_command", "kind", "clip"),
                       &ChampionsSessionController::animation_cache_command);
  ClassDB::bind_method(D_METHOD("set_facing_test", "enabled"),
                       &ChampionsSessionController::set_facing_test);
  ClassDB::bind_method(D_METHOD("set_recovery_test", "enabled"),
                       &ChampionsSessionController::set_recovery_test);
  ClassDB::bind_method(D_METHOD("reset_session"),
                       &ChampionsSessionController::reset_session);
  ClassDB::bind_method(D_METHOD("set_paused", "paused"),
                       &ChampionsSessionController::set_paused);
  ClassDB::bind_method(D_METHOD("release_input"),
                       &ChampionsSessionController::release_input);
  ClassDB::bind_method(D_METHOD("stop_session"),
                       &ChampionsSessionController::stop_session);
  ClassDB::bind_method(D_METHOD("get_observation"),
                       &ChampionsSessionController::get_observation);
  ClassDB::bind_method(D_METHOD("probe_full_walk_weight_test"),
                       &ChampionsSessionController::probe_full_walk_weight_test);
  ClassDB::bind_method(D_METHOD("get_terrain"),
                       &ChampionsSessionController::get_terrain);
  ClassDB::bind_method(D_METHOD("load_terrain_file", "path"),
                       &ChampionsSessionController::load_terrain_file);
  ClassDB::bind_method(D_METHOD("set_player_camera", "slot", "yaw", "flag", "position", "side_normals"),
                       &ChampionsSessionController::set_player_camera);
  ClassDB::bind_method(D_METHOD("prepare_terrain_file", "path", "build_display"),
                       &ChampionsSessionController::prepare_terrain_file, DEFVAL(true));
  ClassDB::bind_method(D_METHOD("set_terrain_start_objects", "path", "objects", "level_name", "previous", "entrance"),
                       &ChampionsSessionController::set_terrain_start_objects,
                       DEFVAL(String()), DEFVAL(String()), DEFVAL(String()));
  ClassDB::bind_method(D_METHOD("load_arena_tables", "executable_path"),
                       &ChampionsSessionController::load_arena_tables);
  ClassDB::bind_method(D_METHOD("commit_terrain_file", "token"),
                       &ChampionsSessionController::commit_terrain_file);
  ClassDB::bind_method(D_METHOD("cancel_terrain_file", "token"),
                       &ChampionsSessionController::cancel_terrain_file);
}
ChampionsSessionController::ChampionsSessionController()
    : control_(std::make_unique<rta::native_save::Scene>(scene_config())),
      terrain_(present_terrain(active_setup().initial_terrain.data)) {
  prepare_meshes(terrain_);
  cache_state(active_observation(), true);
}
ChampionsSessionController::~ChampionsSessionController() = default;

Dictionary ChampionsSessionController::observation() const {
  if (combat_) return combat_observation();
  if (level_) return level_observation();
  auto result = present_observation(current_);
  result["save_history_pending"] = control_->history_pending();
  if (fight_) {
    const auto fight = fight_observation();
    for (const auto &key : fight.keys()) result[key] = fight[key];
    if (fight_->player_ready()) result["movement_policy"] = "shared-world player (Player_run / Player_move)";
  }
  return result;
}
Dictionary ChampionsSessionController::present_observation(
    const rta::session_control::Observation &o) const {
  Dictionary result;
  PackedInt64Array raw;
  for (const auto value : o.position)
    raw.push_back(value);
  result["ok"] = !o.failed;
  result["error"] = String::utf8(o.error.c_str());
  result["position_raw"] = raw;
  result["position"] =
      Vector3(display_float(o.position[0]), display_float(o.position[1]),
              display_float(o.position[2]));
  result["facing_raw"] = int64_t(o.facing);
  result["facing_enabled"] = o.facing_enabled;
  result["facing_policy"] =
      o.facing_enabled ? rta::session_control::FACING_POLICY : "disabled";
  result["turn_rate_raw"] = int64_t(o.turn_rate);
  result["angular_velocity_raw"] = int64_t(o.angular_velocity);
  result["turn_slow"] = int64_t(o.turn_slow);
  result["turn_mode"] = int64_t(o.turn_mode);
  result["turn_result"] =
      o.turn_result ? Variant(int64_t(*o.turn_result)) : Variant();
  result["animation_cache_enabled"] = o.animation_cache_enabled;
  result["animation_cache_policy"] =
      o.animation_cache_enabled
          ? rta::session_control::CONTROL_ANIM_CACHE_POLICY
          : "disabled";
  result["animation_track"] = int64_t(o.animation_track);
  result["animation_event"] =
      o.animation_event ? Variant(int64_t(*o.animation_event)) : Variant();
  result["animation_time_raw"] =
      o.animation_decoded ? Variant(int64_t(o.animation_decoded->time))
                          : Variant();
  result["animation_sample_track"] =
      o.animation_decoded ? int64_t(o.animation_decoded->track) : 0;
  Array joints;
  if (o.animation_decoded)
    for (const auto &joint : o.animation_decoded->joints) {
      Dictionary value;
      PackedInt64Array pos, quat;
      for (auto v : joint.position)
        pos.push_back(v);
      for (auto v : joint.quaternion)
        quat.push_back(v);
      value["position_raw"] = pos;
      value["quaternion_raw"] = quat;
      joints.push_back(value);
    }
  result["animation_joints_raw"] = joints;
  result["hp_raw"] = int64_t(o.hp);
  result["hp"] = display_float(o.hp);
  result["vz_raw"] = int64_t(o.vz);
  result["radius_raw"] = int64_t(o.radius);
  result["radius"] = display_float(o.radius);
  result["recovery_enabled"] = o.recovery_enabled;
  result["recovery_policy"] =
      o.recovery_enabled ? rta::session_recovery::POLICY : "disabled";
  result["max_hp_raw"] = int64_t(o.max_health);
  result["max_hp"] = display_float(o.max_health);
  result["max_mana_raw"] = int64_t(o.max_mana);
  result["max_mana"] = display_float(o.max_mana);
  result["mana_raw"] = int64_t(o.mana);
  result["mana"] = display_float(o.mana);
  result["pending_health_raw"] = int64_t(o.pending_health);
  result["pending_health"] = display_float(o.pending_health);
  result["pending_mana_raw"] = int64_t(o.pending_mana);
  result["pending_mana"] = display_float(o.pending_mana);
  result["frame"] = o.frame;
  result["submissions"] = int64_t(o.submissions);
  result["ready_ticks"] = int64_t(o.ready_ticks);
  result["damage_events"] = int64_t(o.damage_events);
  result["paused"] = o.paused;
  result["stopped"] = o.stopped;
  result["failed"] = o.failed;
  result["save_history_pending"] = false;
  result["terrain_name"] = terrain_name_;
  result["terrain_note"] = terrain_.get("note", "");
  result["terrain_mode"] = terrain_.get("mode", "practice");
  // Observe the active native configuration without a second full snapshot.
  result["movement_policy"] =
      o.movement == rta::session_control::MovementPolicy::DryCreature
          ? "dry_creature"
          : "isolated";
  result["ready_tick_limit"] = int64_t(o.ready_tick_limit);
  result["session_mode"] = o.play_window ? "play" : "diagnostic";
  result["history_policy"] = o.play_window ? "windowed" : "complete";
  result["history_complete"] = !o.history.through_ready;
  result["history_window"] = int64_t(o.play_window);
  result["history_retired_through_ready"] = int64_t(o.history.through_ready);
  result["history_retirements"] = int64_t(o.history.retirements);
  PackedInt64Array retained, capacity, retired;
  for (auto n : o.retained.records)
    retained.push_back(int64_t(n));
  for (auto n : o.retained.capacity)
    capacity.push_back(int64_t(n));
  for (auto n : o.history.retired)
    retired.push_back(int64_t(n));
  result["history_retained_counts"] = retained;
  result["history_retained_capacity"] = capacity;
  result["history_retired_counts"] = retired;
  if (player_) {
    const auto p = observed_player();
    result["player_ready"] = true;
    result["player_record_revision"] = static_cast<int64_t>(player_record_revision_);
    // The character record is not linked to the simulated player: the original
    // mirrors record HP into the object inside Player_run (research-menus
    // docs/player.md), and native play has no such mirror. So hp, max_hp, mana
    // and max_mana above stay the simulation's own values, which are the ones
    // the simulation uses and the HUD draws. The record's values are reported
    // separately for menus and saves, never drawn as bars.
    result["player_record_currents_known"] = p.current_known;
    result["name"] = String::utf8(p.name.c_str());
    result["player_class"] = p.player_class;
    result["player_model"] = p.model;
    result["player_imported"] = p.imported;
    result["player_model_index"] = p.model;
    result["level"] = p.level;
    result["acknowledged_level"] = p.acknowledged;
    result["experience"] = p.experience;
    result["gold"] = p.gold;
    result["record_max_hp_raw"] = int64_t(p.max_hp);
    result["record_max_hp"] = display_float(p.max_hp);
    result["record_max_mana_raw"] = int64_t(p.max_mana);
    result["record_max_mana"] = display_float(p.max_mana);
    if (player_tables_ && p.acknowledged >= 1 && p.acknowledged <= 80) {
      result["experience_base"] = player_tables_->experience[std::size_t(p.acknowledged - 1)];
      result["experience_next"] = player_tables_->experience[std::size_t(p.acknowledged)];
    }
    if (p.current_known) {
      result["record_hp_raw"] = int64_t(p.hp);
      result["record_hp"] = display_float(p.hp);
      result["record_mana_raw"] = int64_t(p.mana);
      result["record_mana"] = display_float(p.mana);
    }
  } else {
    result["player_ready"] = false;
  }
  return result;
}
Dictionary ChampionsSessionController::get_observation() const {
  try {
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::probe_full_walk_weight_test() {
  try {
    if (!combat_ || !combat_->full())
      throw std::runtime_error("Walk-cell probe requires the synthetic Full fixture.");
    const auto player = combat_->actors().at(0);
    const auto address = combat_->world().target(player.handle);
    const auto expected = combat_->world().read(address + 0x144);
    const auto observed = combat_->probe_walk_life_zero([&] {
      const Array actors = combat_observation()["combat_actors"];
      const Dictionary row = actors[0];
      return rta::Raw(int64_t(row["locomotion_walk_weight_raw"]));
    });
    Dictionary out;
    out["ok"] = true;
    out["expected_weight_raw"] = int64_t(expected);
    out["observed_weight_raw"] = int64_t(observed);
    return out;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::advance(int64_t mask) {
  try {
    if (mask < 0 || mask > rta::session_control::COMMAND_MASK)
      throw std::runtime_error("Unknown control command.");
    const auto before = active_observation().ready_ticks;
    cache_state(active_advance(dialog_gate(static_cast<uint32_t>(mask))), true);
    pump_scripts(before);
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
void ChampionsSessionController::cache_state(
    const rta::session_control::Observation &o, bool snap) {
  previous_position_ = current_position_;
  previous_facing_ = current_facing_;
  current_position_ =
      Vector3(display_float(o.position[0]), display_float(o.position[1]),
              display_float(o.position[2]));
  // Native +x is facing zero; native +y maps to Godot -z.
  current_facing_ = double(o.facing & 0xffffu) * (6.283185307179586 / 65536.0);
  current_ = o;
  if (snap || o.paused || o.stopped || o.failed) {
    previous_position_ = current_position_;
    previous_facing_ = current_facing_;
    clock_.reset();
  }
}
void ChampionsSessionController::snap() {
  cache_state(active_observation(), true);
}
// Bound: the observation (display_float, World reads) can throw; nothing may cross
// the GDExtension boundary (binding audit after review finding 1).
Dictionary ChampionsSessionController::snap_presentation() {
  try {
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::advance_time(int64_t usec,
                                                    int64_t mask) {
  try {
    if (mask < 0 || mask > rta::session_control::COMMAND_MASK)
      throw std::runtime_error("Unknown control command.");
    if (current_.paused || current_.stopped || current_.failed) {
      clock_.reset();
    } else {
      clock_.advance(usec, [&]() {
        const auto generation = level_ ? level_->flow().generation : 0;
        const auto before = active_observation().ready_ticks;
        auto state = active_advance(dialog_gate(static_cast<uint32_t>(mask)));
        cache_state(state, level_ && generation != level_->flow().generation);
        pump_scripts(before);
        return !current_.paused && !current_.stopped && !current_.failed;
      });
    }
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::get_presentation() const {
  Dictionary out;
  out["previous_position"] = previous_position_;
  out["current_position"] = current_position_;
  out["previous_facing"] = previous_facing_;
  out["current_facing"] = current_facing_;
  out["alpha"] = clock_.alpha();
  return out;
}
Dictionary ChampionsSessionController::set_recovery_test(bool enabled) {
  try {
    if (combat_ || level_) throw std::runtime_error("Fixture refuses optional diagnostics.");
    auto setup = active_setup();
    if (enabled) {
      setup.config.recovery.emplace();
      setup.config.max_ready_ticks =
          rta::session_control::MAX_RECOVERY_READY_TICKS;
    } else {
      setup.config.recovery.reset();
      setup.config.max_ready_ticks =
          setup.config.movement ==
                  rta::session_control::MovementPolicy::DryCreature
              ? rta::session_control::MAX_CREATURE_READY_TICKS
              : rta::session_control::MAX_READY_TICKS;
    }
    if (setup.config.facing)
      setup.config.max_ready_ticks =
          rta::session_control::MAX_FACING_READY_TICKS;
    if (setup.config.animation_cache)
      setup.config.max_ready_ticks = rta::session_control::MAX_ANIM_READY_TICKS;
    auto next = std::make_unique<rta::native_save::Scene>(
        setup.config,
        setup.supplied_terrain ? std::optional<rta::world_queries::LevelData>(
                                     setup.initial_terrain.data)
                               : std::nullopt,
        setup.initial_terrain.limits);
    unpair_script_seed_if_paired();
    control_ = std::move(next);
    pending_.reset();
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_facing_test(bool enabled) {
  try {
    if (combat_ || level_) throw std::runtime_error("Fixture refuses optional diagnostics.");
    auto setup = active_setup();
    if (enabled) {
      setup.config.facing.emplace();
      // Authored demonstration rate, not a retail class statistic.
      setup.config.facing->initial.rate = 0x3f800000;
      setup.config.facing->mode = 0;
      setup.config.max_ready_ticks =
          rta::session_control::MAX_FACING_READY_TICKS;
    } else {
      setup.config.facing.reset();
      setup.config.max_ready_ticks =
          setup.config.movement ==
                  rta::session_control::MovementPolicy::DryCreature
              ? rta::session_control::MAX_CREATURE_READY_TICKS
              : rta::session_control::MAX_READY_TICKS;
    }
    if (setup.config.recovery)
      setup.config.max_ready_ticks =
          rta::session_control::MAX_RECOVERY_READY_TICKS;
    if (setup.config.animation_cache)
      setup.config.max_ready_ticks = rta::session_control::MAX_ANIM_READY_TICKS;
    auto next = std::make_unique<rta::native_save::Scene>(
        setup.config,
        setup.supplied_terrain ? std::optional<rta::world_queries::LevelData>(
                                     setup.initial_terrain.data)
                               : std::nullopt,
        setup.initial_terrain.limits);
    unpair_script_seed_if_paired();
    control_ = std::move(next);
    pending_.reset();
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_animation_cache_test(bool enabled) {
  try {
    if (combat_ || level_) throw std::runtime_error("Fixture refuses optional diagnostics.");
    auto setup = active_setup();
    if (enabled)
      setup.config.animation_cache =
          rta::session_control::diagnostic_animation_cache();
    else
      setup.config.animation_cache.reset();
    setup.config.max_ready_ticks =
        setup.config.animation_cache
            ? rta::session_control::MAX_ANIM_READY_TICKS
        : (setup.config.recovery || setup.config.facing)
            ? rta::session_control::MAX_FACING_READY_TICKS
        : setup.config.movement ==
                rta::session_control::MovementPolicy::DryCreature
            ? rta::session_control::MAX_CREATURE_READY_TICKS
            : rta::session_control::MAX_READY_TICKS;
    auto next = std::make_unique<rta::native_save::Scene>(
        setup.config,
        setup.supplied_terrain ? std::optional<rta::world_queries::LevelData>(
                                     setup.initial_terrain.data)
                               : std::nullopt,
        setup.initial_terrain.limits);
    unpair_script_seed_if_paired();
    control_ = std::move(next);
    pending_.reset();
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::animation_cache_command(int64_t kind,
                                                               int64_t clip) {
  try {
    if (combat_ || level_) throw std::runtime_error("Fixture refuses animation-cache commands.");
    if (kind < 0 || kind > 2 || clip < 1 || clip > 4)
      throw std::runtime_error("Unknown animation diagnostic command or clip.");
    const auto id = control_->queue_animation(
        rta::session_control::AnimationCommandKind(kind), uint64_t(clip));
    auto result = observation();
    result["animation_command_id"] = int64_t(id);
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::get_play_capabilities() const {
  Dictionary result;
  result["available"] = !current_.recovery_enabled &&
                        !current_.facing_enabled &&
                        !current_.animation_cache_enabled;
  result["policy"] = rta::play_session::POLICY;
  result["history_policy"] = "windowed";
  result["history_window"] = int64_t((combat_ || level_) ? 0 : rta::play_session::DEFAULT_WINDOW);
  result["terrain_elements"] = int64_t(active_setup().initial_terrain.data.elements.size());
  Array formats;
  if (!combat_ && !level_) formats.push_back("play-continuation-v1");
  else {
    // Review finding 2: the combat and level fixtures save as fixture continuations.
    Dictionary fixture;
    fixture["id"] = rta::fixture_save::FORMAT;
    fixture["label"] = "Fixture continuation (combat / level state)";
    fixture["method"] = "save_file";
    formats.push_back(fixture);
  }
  result["combat_fixture_available"] = true;
  result["full_fixture_available"] = bool(fx_constants_);
  result["full_policy"] = rta::combat_play::FULL_POLICY;
  result["effect_constants"] = fx_constants_source_;
  result["animated_fixture_available"] = true;
  result["level_fixture_available"] = true;
  if (level_) { result["policy"] = rta::level_play::POLICY; result["history_policy"] = "per-level-fixture-state"; }
  result["combat_policy"] = rta::combat_play::POLICY;
  if (combat_) { result["policy"] = combat_policy(*combat_); result["history_policy"] = "latest-tick"; }
  result["save_formats"] = formats;
  Array unsupported;
  unsupported.push_back("recovery");
  unsupported.push_back("facing");
  unsupported.push_back("animation_cache");
  result["unsupported_optional_diagnostics"] = unsupported;
  result["level_script_available"] = true;
  result["level_script_enabled"] = bool(scripts_);
  result["level_script_policy"] = rta::script_commands::POLICY;
  return result;
}
// The Script task runs once per simulated frame, also while paused (research
// 9649e6b docs/script.md 2.4 and 2.6). A submission simulates 0 or 1 ready
// ticks, so the task follows the ready-tick count, not the submissions.
void ChampionsSessionController::unpair_script_seed() {
  if (scripts_ && script_seed_)
    scripts_->bind_seed(nullptr);
  if (script_seed_)
    script_seed_unpaired_ = true;
  script_world_.reset();
  script_seed_.reset();
}
// A session that replaces the paired World leaves the script's stream behind.
void ChampionsSessionController::unpair_script_seed_if_paired() {
  if (script_world_)
    unpair_script_seed();
}
void ChampionsSessionController::remember_script_fade() {
  if (scripts_)
    script_fade_ = scripts_->fade_level();
}
void ChampionsSessionController::pump_scripts(std::size_t before) {
  if (!scripts_)
    return;
  // The control session's World makes no shared draw in its ticks
  // (session_control.cpp); if it ever does, the two streams have split.
  if (script_world_ && control_ && !combat_ && !level_ &&
      control_->shared_seed() != *script_world_)
    unpair_script_seed();
  for (auto tick = before; tick < current_.ready_ticks; ++tick) {
    if (scripts_->dialog_attached()) {
      // Priority 10 runObjects (Player_interact), then the dialog run task
      // at 110, before the Script task at 125 (research docs/script.md 2.6).
      if (dialog_talk_pending_ >= 0) {
        const auto talkers = scripts_->talkers();
        const auto index = std::size_t(dialog_talk_pending_);
        dialog_talk_pending_ = -1;
        if (index < talkers.size())
          dialog_last_use_ = String::utf8(
              scripts_->interact(talkers[index], 0, scripts_->in_front_of(talkers[index])).c_str());
      }
      if (dialog_use_pending_) {
        dialog_use_pending_ = false;
        std::array<rta::Raw, 3> at{current_.position[0], current_.position[1], current_.position[2]};
        dialog_last_use_ = String::utf8(scripts_->use(0, at).c_str());
      }
    }
    run_level_triggers(); // the 15-list follows the every-frame player walk
    if (scripts_->dialog_attached()) {
      rta::script_commands::DialogInputs in;
      in.pad = dialog_pad_;
      dialog_pad_ = 0;
      in.fade = scripts_->ui_ramps().fade[0]; // g_uiFadeIn[0] gates message 3 (docs/ui-text-gaps.md 1)
      if (dialog_voice_) {
        in.voice.queue_count = (*dialog_voice_)[0];
        in.voice.status = (*dialog_voice_)[1];
        in.voice.position = (*dialog_voice_)[2];
      }
      scripts_->dialog_tick(in);
    }
    scripts_->on_ready_tick();
    note_help_box();
    if (!ui_frame_driven_)
      scripts_->ui_frame_step();
  }
}
void ChampionsSessionController::run_level_triggers() {
  if (live_triggers_.empty()) return;
  // obj_runEvery15 steps its cursor before linking; runObjects walks the
  // selected list in reverse link order at priority 10.
  const auto bucket = (++trigger_frame_) % 15;
  const auto &list = trigger_lists_[bucket];
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    auto &live = live_triggers_[*it];
    if (live.refused) continue;
    ++trigger_samples_;
    const rta::level_flow::TriggerPlayer player{0, current_.position};
    live.body.run_exact({player}, false, nullptr, {},
                        [this, &live](const std::string &name, unsigned slot) {
      ++trigger_dispatches_;
      const Dictionary before = script_level_request();
      const auto stop = scripts_->run_object_entry(live.record, name, slot);
      const Dictionary after = script_level_request();
      if (stop == rta::script_commands::Stop::Complete && name == live.body.script &&
          bool(after.get("queued", false)) &&
          (!bool(before.get("queued", false)) || before.get("name", String()) != after.get("name", String()))) {
        ++trigger_queued_dispatches_;
        trigger_queued_record_ = live.record;
        trigger_queued_destination_ = String(after.get("name", String())).utf8().get_data();
      }
      if (stop == rta::script_commands::Stop::Missing) {
        ++trigger_missing_;
        return -1;
      }
      if (stop == rta::script_commands::Stop::Refused ||
          stop == rta::script_commands::Stop::Fault)
        throw std::runtime_error("Automatic Trigger script was refused.");
      return scripts_->last_value();
    });
  }
}
// Player_run takes no input while a dialog is open and Player_move zeroes the
// horizontal velocity (research docs/dialog.md 6.1).
uint32_t ChampionsSessionController::dialog_gate(uint32_t mask) const {
  return scripts_ && (scripts_->dialog().is_open() || scripts_->hud().help_open) ? 0u : mask;
}
namespace {
std::vector<std::uint8_t> raw_bytes(const PackedByteArray &bytes) {
  return std::vector<std::uint8_t>(bytes.ptr(), bytes.ptr() + bytes.size());
}
} // namespace
// The text table and gamefontmed.fnt the dialog services read.
struct ChampionsSessionController::DialogData {
  std::optional<rta::dialog::TextTable> text;
  std::optional<champions::ui_layout::FontMetrics> font;
  std::vector<std::uint8_t> font_bytes;
};

void ChampionsSessionController::bind_player_to_scripts(rta::script_commands::Host &host,
    const rta::player_state::Record &p) {
  if (!player_tables_) return;
  host.set_player_currents_known(p.current_known);
  auto &v = host.variables();
  auto &m = v.memory;
  const auto rec = v.player_record(0);
  m.string(rec, p.name);
  m.word(rec + 0x50, static_cast<std::uint32_t>(p.player_class));
  m.word(rec + 0x54, static_cast<std::uint32_t>(p.model));
  m.byte(rec + 0x58, static_cast<std::uint8_t>(p.acknowledged));
  m.byte(rec + 0x59, static_cast<std::uint8_t>(p.level));
  m.word(rec + 0x5c, static_cast<std::uint32_t>(p.experience));
  for (std::size_t n = 0; n < 4; ++n) {
    m.word(rec + 0x78 + std::uint32_t(n * 4), static_cast<std::uint32_t>(p.base_stats[n]));
    m.word(rec + 0x90 + std::uint32_t(n * 4), static_cast<std::uint32_t>(p.effective_stats[n]));
  }
  if (p.current_known) {
    m.word(rec + 0xac, p.hp);
    m.word(rec + 0xc0, p.mana);
  }
  m.word(rec + 0xb0, p.max_hp);
  m.word(rec + 0xc4, p.max_mana);
  m.word(rec + 0xe4, static_cast<std::uint32_t>(p.gold));
  std::array<std::int32_t, rta::quests::XP_LEVELS> thresholds{};
  std::copy(player_tables_->experience.begin(), player_tables_->experience.end(), thresholds.begin());
  host.campaign().set_xp_table(thresholds);
  auto campaign = host.campaign().record(0);
  campaign.level = p.level; campaign.exp = p.experience; campaign.gold = p.gold;
  if (p.current_known) campaign.hp = display_float(p.hp);
  campaign.max_hp = display_float(p.max_hp);
  if (p.current_known) campaign.mana = display_float(p.mana);
  campaign.max_mana = display_float(p.max_mana);
  host.campaign().set_record(0, campaign);
}

rta::player_state::Record ChampionsSessionController::observed_player() const {
  auto p = *player_;
  if (!scripts_) return p;
  const auto &m = scripts_->variables().memory;
  const auto rec = scripts_->variables().player_record(0);
  p.acknowledged = static_cast<std::int8_t>(m.read(rec + 0x58));
  p.level = static_cast<std::int8_t>(m.read(rec + 0x59));
  p.experience = static_cast<std::int32_t>(m.word(rec + 0x5c));
  p.gold = static_cast<std::int32_t>(m.word(rec + 0xe4));
  for (std::size_t n = 0; n < 4; ++n) {
    p.base_stats[n] = static_cast<std::int32_t>(m.word(rec + 0x78 + std::uint32_t(n * 4)));
    p.effective_stats[n] = static_cast<std::int32_t>(m.word(rec + 0x90 + std::uint32_t(n * 4)));
  }
  p.max_hp = m.word(rec + 0xb0);
  p.max_mana = m.word(rec + 0xc4);
  if (p.current_known) {
    p.hp = m.word(rec + 0xac);
    p.mana = m.word(rec + 0xc0);
  }
  return p;
}

Dictionary ChampionsSessionController::set_player_character(const Dictionary &character) {
  try {
    if (!player_tables_) throw std::runtime_error("Select the RTA US v2.00 executable for player formulas and XP thresholds.");
    const String name = character.get("name", "");
    const std::string named = name.utf8().get_data();
    const auto cls = static_cast<std::int32_t>(int64_t(character.get("class_index", -1)));
    const bool imported = bool(character.get("imported", false));
    const Array stats = character.get(imported ? "base_stats" : "stats", Array());
    if (stats.size() != 4 || (!imported && !bool(character.get("base_known", false))))
      throw std::runtime_error("Player start statistics are not known from the selected executable.");
    std::array<std::int32_t, 4> base{};
    for (int n = 0; n < 4; ++n) base[std::size_t(n)] = static_cast<std::int32_t>(int64_t(stats[n]));
    rta::player_state::Record next;
    if (imported) {
      const Array effective = character.get("effective_stats", Array());
      if (effective.size() != 4)
        throw std::runtime_error("Imported player record lacks effective statistics +0x90.");
      next.name = named; next.player_class = cls;
      next.model = static_cast<std::int32_t>(int64_t(character.get("model_index", -1)));
      next.base_stats = base;
      for (int n = 0; n < 4; ++n)
        next.effective_stats[std::size_t(n)] = static_cast<std::int32_t>(int64_t(effective[n]));
      next.level = static_cast<std::int32_t>(int64_t(character.get("level", 0)));
      next.acknowledged = static_cast<std::int32_t>(int64_t(character.get("acknowledged", 0)));
      next.experience = static_cast<std::int32_t>(int64_t(character.get("experience", -1)));
      next.gold = static_cast<std::int32_t>(int64_t(character.get("gold", -1)));
      for (const char *key : {"hp_raw", "max_hp_raw", "mana_raw", "max_mana_raw"})
        if (!character.has(key)) throw std::runtime_error("Imported player record lacks a documented current or maximum field.");
      next.hp = static_cast<rta::Raw>(int64_t(character["hp_raw"]));
      next.max_hp = static_cast<rta::Raw>(int64_t(character["max_hp_raw"]));
      next.mana = static_cast<rta::Raw>(int64_t(character["mana_raw"]));
      next.max_mana = static_cast<rta::Raw>(int64_t(character["max_mana_raw"]));
      next = rta::player_state::import(std::move(next), *player_tables_);
    } else {
      if (!bool(character.get("ready", false))) throw std::runtime_error("Character creation has not finished.");
      next = rta::player_state::create(named, cls,
          static_cast<std::int32_t>(int64_t(character.get("model", -1))), base, *player_tables_);
      // Creation has no live Player recalc or record-to-object current mirror.
      // Keep current-dependent script commands refused until that event exists.
    }
    player_ = std::move(next);
    ++player_record_revision_;
    if (scripts_) bind_player_to_scripts(*scripts_, *player_);
    Dictionary result = observation(); result["ok"] = true;
    return result;
  } catch (const std::exception &e) { return failure(e); }
}

Dictionary ChampionsSessionController::save_player_companion(const String &path) const {
  const String dest = path + String(".player"), temp = dest + String(".tmp");
  bool owns_temporary = false;
  try {
    if (!player_) {
      if (FileAccess::file_exists(dest) && DirAccess::remove_absolute(dest) != OK)
        throw std::runtime_error("An old player companion could not be removed.");
      Dictionary result; result["ok"] = true; result["saved"] = false; return result;
    }
    if (!player_tables_) throw std::runtime_error("The selected executable is unavailable for the player save.");
    const auto bytes = rta::player_state::save(observed_player(), *player_tables_);
    if (FileAccess::file_exists(temp)) throw std::runtime_error("A player save temporary file already exists.");
    Ref<FileAccess> file = FileAccess::open(temp, FileAccess::WRITE);
    if (file.is_null()) throw std::runtime_error("The player companion could not be created.");
    owns_temporary = true;
    PackedByteArray packed; packed.resize(bytes.size());
    std::copy(bytes.begin(), bytes.end(), packed.ptrw());
    file->store_buffer(packed); file->flush();
    const auto error = file->get_error(); file->close();
    if (error != OK || DirAccess::rename_absolute(temp, dest) != OK)
      throw std::runtime_error("The player companion could not be written.");
    owns_temporary = false;
    Dictionary result; result["ok"] = true; result["saved"] = true; return result;
  } catch (const std::exception &e) {
    if (owns_temporary) DirAccess::remove_absolute(temp);
    return failure(e);
  }
}

Dictionary ChampionsSessionController::load_player_companion(const String &path) {
  try {
    const String dest = path + String(".player");
    if (!FileAccess::file_exists(dest)) {
      player_.reset();
      ++player_record_revision_;
      if (scripts_) scripts_->set_player_currents_known(true); // no created record is held any more
      Dictionary result = observation(); result["ok"] = true; result["loaded"] = false; return result;
    }
    if (!player_tables_) throw std::runtime_error("Select the executable before loading a saved player record.");
    Ref<FileAccess> file = FileAccess::open(dest, FileAccess::READ);
    if (file.is_null() || file->get_length() > 256)
      throw std::runtime_error("The player companion cannot be read or is too large.");
    const auto packed = file->get_buffer(file->get_length());
    const std::vector<std::uint8_t> bytes(packed.ptr(), packed.ptr() + packed.size());
    auto next = rta::player_state::load(bytes, *player_tables_);
    player_ = std::move(next);
    ++player_record_revision_;
    if (scripts_) bind_player_to_scripts(*scripts_, *player_);
    Dictionary result = observation(); result["ok"] = true; result["loaded"] = true; return result;
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::save_campaign_bytes() {
  try {
    if (!scripts_) throw std::runtime_error("A level script is required to save campaign state.");
    const auto bytes = scripts_->save_campaign();
    PackedByteArray packed;
    packed.resize(bytes.size());
    std::copy(bytes.begin(), bytes.end(), packed.ptrw());
    Dictionary result; result["ok"] = true; result["bytes"] = packed; return result;
  } catch (const std::exception &e) { return failure(e); }
}

Dictionary ChampionsSessionController::load_campaign_bytes(const PackedByteArray &packed) {
  try {
    if (!scripts_) throw std::runtime_error("Load this level's script before campaign state.");
    if (packed.is_empty() || packed.size() > 1024 * 1024)
      throw std::runtime_error("Campaign save is empty or exceeds 1 MiB.");
    const std::vector<std::uint8_t> bytes(packed.ptr(), packed.ptr() + packed.size());
    scripts_->load_campaign(bytes);
    Dictionary result = observation(); result["ok"] = true; return result;
  } catch (const std::exception &e) { return failure(e); }
}
Array ChampionsSessionController::combat_resource_lump_names() const {
  Array names;
  if (!executable_) return names;
  for (const auto definition : {0x502f80u, 0x501200u}) {
    std::uint32_t address = 0;
    for (unsigned i = 0; i < 4; ++i)
      address |= std::uint32_t(executable_->byte(definition + i)) << (i * 8);
    const auto name = executable_->string(address);
    if (!name.empty()) names.push_back(String::utf8(name.c_str()));
  }
  if (player_) {
    // The session player's class animation LMP (Player_ctor's model definition).
    const auto lump = rta::campaign_fight::Fight::player_animation_lump(*executable_, unsigned(player_->model));
    if (!lump.empty()) names.push_back(String::utf8(lump.c_str()));
  }
  return names;
}

Dictionary ChampionsSessionController::enable_level_script(const PackedByteArray &script,
                                                          const PackedByteArray &objects,
                                                          bool carry_campaign,
                                                          const Dictionary &dialog,
                                                          int64_t music_volume_bits,
                                                          const String &level_name,
                                                          const PackedByteArray &balance,
                                                          const PackedByteArray &world,
                                                          const Dictionary &resource_lumps) {
  try {
    if (script.is_empty())
      throw std::runtime_error("Level script image is empty.");
    // A pending post action belongs to the destination's script load. This
    // bounded host does not carry it, so keep the current level intact.
    if (carry_campaign && scripts_ &&
        !scripts_->variables().memory.cstr(0x86fa80).empty())
      throw std::runtime_error("Level change with a pending post action is unsupported.");
    if (music_volume_bits < -1 || music_volume_bits > int64_t(std::numeric_limits<std::uint32_t>::max()))
      throw std::runtime_error("Music volume is not a 32-bit pattern.");
    auto next_authority = (!combat_ && !level_)
        ? std::make_shared<rta::WorldAuthority>() : nullptr;
    // A real level with the owner's executable, the level's terrain and the
    // audited player bundle gets the shared-World fight; anything else keeps
    // the prepared (unticked) World of the earlier sessions.
    const bool want_fight = next_authority && executable_ && player_services_ && !world.is_empty();
    std::unique_ptr<rta::campaign_fight::Fight> next_fight;
    if (want_fight)
      next_fight = std::make_unique<rta::campaign_fight::Fight>(
          next_authority, executable_, *player_services_, [this]() {
            return scripts_ && dialog_status().get("open", false);
          });
    auto next_runtime = (next_authority && !want_fight)
        ? std::make_unique<rta::combat_runtime::World>(next_authority,
                                                       rta::combat_runtime::Services{})
        : nullptr;
    auto next_adapter = next_runtime
        ? std::make_unique<rta::combat_runtime::SessionAdapter>(*next_runtime)
        : nullptr;
    auto next = next_authority
        ? std::make_unique<rta::script_commands::Host>(next_authority)
        : std::make_unique<rta::script_commands::Host>();
    apply_text(*next);
    rta::amx::ByteVector image(static_cast<std::size_t>(script.size()));
    std::memcpy(image.data(), script.ptr(), image.size());
    if (!next->load(image))
      throw std::runtime_error("Level script image was refused.");
    // The same options word feeds the native mixer and the script's private
    // playback model. Without an options file, use the documented boot default.
    next->set_music_volume(music_volume_bits < 0
                               ? champions::audio::boot_music_volume(std::nullopt)
                               : std::uint32_t(music_volume_bits));
    next->set_fade_level(scripts_ ? scripts_->fade_level() : script_fade_);
    const bool carried = carry_campaign && scripts_;
    const auto live_player = player_ && carried
        ? std::optional<rta::player_state::Record>(observed_player()) : player_;
    if (live_player) next->set_player_currents_known(live_player->current_known);
    // A single-player game (docs/script-session.md section 2); a carried
    // campaign below brings its own values.
    next->start_single_player();
    bool campaign_image_restored = false;
    if (carried) {
      const auto campaign_image = scripts_->save_campaign();
      next->load_campaign(campaign_image);
      campaign_image_restored = next->save_campaign() == campaign_image;
    }
    if (live_player)
      bind_player_to_scripts(*next, *live_player);
    // A fresh stream from the play session's configured shared seed; the
    // fixtures keep their own journaled seeds, so it is bound only while the
    // control session is the one that will play.
    auto seed = std::make_unique<rta::SharedRandom>(active_setup().config.shared_seed);
    if (!combat_ && !level_)
      next->bind_seed(seed.get(), rta::script_commands::SeedPolicy::OfflineSession);
    if (executable_)
      next->set_executable(rta::script_commands::executable_view(executable_));
    if (executable_)
      next->campaign().set_rows(mission_rows(*executable_));
    for (const auto &name_value : resource_lumps.keys()) {
      const String name = name_value;
      const PackedByteArray image = resource_lumps[name];
      if (image.is_empty() || image.size() > 64 * 1024 * 1024) {
        next->count_refusal("Selected resource archive unreadable");
        continue;
      }
      try {
        next->load_resource_lmp(name.utf8().get_data(), {image.ptr(), std::size_t(image.size())});
      } catch (const std::exception &) {
        next->count_refusal("Selected resource archive malformed");
      }
    }
    if (next_fight) next_fight->install(*next);
    // Malformed statistics or WORLD are refused and counted; the script still
    // attaches without them.
    if (executable_ && !level_name.is_empty() && !balance.is_empty()) {
      try {
        next->set_level_stats(std::string(level_name.utf8().get_data()), balance.ptr(),
                              static_cast<std::size_t>(balance.size()));
      } catch (const std::exception &) {
        next->count_refusal("Level statistics refused");
      }
    }
    if (!world.is_empty()) {
      try {
        next->set_level_world(world.ptr(), static_cast<std::size_t>(world.size()));
      } catch (const std::exception &) {
        next->count_refusal("Level WORLD refused");
      }
    }
    // The original checks hud.lmp residency. This attach path does not know
    // it, so the host records the inferred draw count without moving the seed.
    next->advance_first_level_items(std::string(level_name.utf8().get_data()));
    // Loader order (docs/SCRIPT_COMMANDS.md): preStart, the objects, start.
    next->run_prestart();
    if (!objects.is_empty()) {
      std::vector<std::uint8_t> raw(static_cast<std::size_t>(objects.size()));
      std::memcpy(raw.data(), objects.ptr(), raw.size());
      next->adopt_objects(raw.data(), raw.size());
    }
    std::shared_ptr<DialogData> data;
    if (!dialog.is_empty()) {
      data = std::make_shared<DialogData>();
      rta::script_commands::DialogLevel level;
      level.level = String(dialog.get("level", String())).utf8().get_data();
      // dialog_loadBox 0x22f910 (docs/spec/dialog-gaps.md 4): whether box.tex was found, whether
      // arena mode B is on, and whether an earlier level left the box word.
      level.box_available = bool(dialog.get("box_available", true));
      level.pvp_mode_b = bool(dialog.get("pvp_mode_b", false));
      level.previous_box = dialog_box_loaded_;
      const Dictionary files = dialog.get("dbin", Dictionary());
      const Array keys = files.keys();
      for (int64_t i = 0; i < keys.size(); ++i)
        level.files[String(keys[i]).utf8().get_data()] = raw_bytes(PackedByteArray(files[keys[i]]));
      const PackedByteArray directory = dialog.get("stream_directory", PackedByteArray());
      const int64_t vat_size = dialog.get("vat_size", int64_t(0));
      if (!directory.is_empty() && vat_size > 0) {
        const auto dir = raw_bytes(directory);
        for (const auto &entry : champions::audio::parse_vat_directory({dir.data(), dir.size()}, std::size_t(vat_size)))
          level.stream[entry.name] = {entry.offset, entry.length};
      }
      const PackedByteArray text = dialog.get("text", PackedByteArray());
      if (!text.is_empty()) {
        const auto bytes = raw_bytes(text);
        data->text = rta::dialog::TextTable::parse(bytes.data(), bytes.size());
      }
      const PackedByteArray font = dialog.get("font", PackedByteArray());
      if (!font.is_empty()) {
        data->font_bytes = raw_bytes(font);
        data->font = champions::ui_layout::fontMetrics({data->font_bytes.data(), data->font_bytes.size()});
      }
      rta::script_commands::DialogServices services;
      if (data->text)
        services.text = [data](const std::string &key) { return data->text->lookup(key); };
      if (data->font) {
        namespace T = champions::ui_text_layout;
        namespace D = rta::dialog;
        services.layout_height = [data](const D::Wide &t) -> std::int64_t {
          return T::drawWrapped(*data->font, D::BODY_X, 0, t, D::BODY_W, 0, 0, D::LINE).height;
        };
        services.shown = [data](const D::Wide &t, std::int64_t y, std::int64_t budget, int bottom) -> std::int64_t {
          const auto w = T::drawWrapped(*data->font, D::BODY_X, int(y), t, D::BODY_W, D::BODY_TOP, bottom, D::LINE,
                                        true, 0, budget < 0 ? std::size_t(-1) : std::size_t(budget));
          return std::int64_t(w.shown.value_or(0));
        };
      }
      next->attach_dialog(std::move(level), std::move(services));
      dialog_box_loaded_ = next->dialog_box_word() != 0;
    }
    next->run_start();
    std::vector<rta::level_flow::Record> next_objects;
    if (!objects.is_empty()) {
      const auto raw = raw_bytes(objects);
      next_objects = rta::level_flow::parse_objects({raw.data(), raw.size()});
    }
    release_help_pause();
    dialog_data_ = data;
    dialog_voice_.reset();
    dialog_last_use_ = String();
    dialog_use_pending_ = false;
    dialog_talk_pending_ = -1;
    dialog_pad_ = 0;
    // The previous host is released first, then its fight.
    scripts_ = std::move(next);
    fight_ = std::move(next_fight);
    fight_level_ = level_name.utf8().get_data();
    world_adapter_ = std::move(next_adapter);
    world_runtime_ = std::move(next_runtime);
    world_authority_ = std::move(next_authority);
    script_level_objects_ = std::move(next_objects);
    if (carried && !trigger_level_name_.empty()) {
      auto &saved = trigger_states_[trigger_level_name_];
      saved.clear();
      for (const auto &live : live_triggers_) saved.push_back(live.body.save());
    }
    live_triggers_.clear();
    for (auto &list : trigger_lists_) list.clear();
    trigger_list_cursor_ = 0;
    trigger_frame_ = 0;
    trigger_refusal_.clear();
    trigger_samples_ = trigger_dispatches_ = trigger_missing_ = 0;
    trigger_queued_dispatches_ = 0;
    trigger_queued_destination_.clear();
    const bool has_params = std::any_of(script_level_objects_.begin(), script_level_objects_.end(),
        [](const auto &r) { return r.cls == "TriggerParams"; });
    const bool has_unmodelled_slow = std::any_of(script_level_objects_.begin(), script_level_objects_.end(),
        [](const auto &r) { return r.cls == "Shooter" || r.cls == "FallPlatform"; });
    for (std::size_t i = 0; i < script_level_objects_.size(); ++i) {
      const auto &record = script_level_objects_[i];
      // FloorSwitch_ctor also joins the 15-list (research monsters-2).
      // Its construction consumes a cursor position in record order.
      if (record.cls == "FloorSwitch") {
        trigger_list_cursor_ = (trigger_list_cursor_ + 1) % 15;
        continue;
      }
      if (record.cls != "Trigger") continue;
      const auto integer = [&](const char *name) {
        const auto value = record.prop(name);
        if (!value) return 0;
        std::size_t consumed = 0;
        const auto parsed = std::stoll(*value, &consumed, 10);
        if (parsed < INT32_MIN || parsed > INT32_MAX)
          throw std::runtime_error("Trigger dimension outside signed32.");
        return int(parsed);
      };
      const auto script = record.prop("t").value_or("");
      const bool refused = has_params || has_unmodelled_slow || script.empty();
      if (refused) trigger_refusal_ =
          "TriggerParams linking, another slow-list constructor, or missing script is unported.";
      trigger_list_cursor_ = (trigger_list_cursor_ + 1) % 15;
      live_triggers_.emplace_back(i, rta::level_flow::Trigger(
          record.position[0], record.position[1], integer("w"), integer("h"), script), refused);
      const auto saved = trigger_states_.find(start_level_name_);
      if (carried && saved != trigger_states_.end() &&
          live_triggers_.size() <= saved->second.size())
        live_triggers_.back().body.restore(saved->second[live_triggers_.size() - 1]);
      trigger_lists_[trigger_list_cursor_].push_back(live_triggers_.size() - 1);
    }
    trigger_level_name_ = start_level_name_;
    hub_savepoints_.clear();
    script_fade_ = scripts_->fade_level();
    if (carried && live_player) player_ = *live_player;
    script_seed_ = (!combat_ && !level_) ? std::move(seed) : nullptr;
    script_world_.reset();
    script_seed_unpaired_ = false;
    note_help_box();
    Dictionary result;
    result["ok"] = true;
    result["campaign_carried"] = carried;
    result["campaign_image_restored"] = campaign_image_restored;
    result["executable"] = bool(executable_);
    result["music_volume"] = int64_t(scripts_->audio().music_volume());
    result["fade_level"] = int64_t(script_fade_);
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
void ChampionsSessionController::reset_fight() { fight_.reset(); }
void ChampionsSessionController::start_fight(const rta::session_control::Observation &state) {
  if (!fight_ || fight_->started()) return;
  // The session's player is the ported Player in the shared World. The dry
  // scene stays only as the ready-tick clock and save spine.
  rta::campaign_fight::Fight::PlayerStart start;
  start.position = state.position;
  start.facing = state.facing & 0xffffu;
  start.level_name = fight_level_;
  if (player_) {
    const auto p = observed_player();
    start.model_index = unsigned(p.model);
    start.hp = p.current_known ? p.hp : p.max_hp;
  } else {
    start.hp = state.hp;
  }
  fight_->start(start);
}
Dictionary ChampionsSessionController::fight_restore_player(const Vector3 &position, int64_t facing, int64_t hp_raw) {
  // A load replaced the scene before play started: the fight begins from the
  // loaded scene, then the saved player is put back.
  if (fight_ && !fight_->started() && control_) start_fight(control_->observation());
  // Refuse a malformed companion before writing anything (docs/NATIVE_SAVE.md,
  // ".fight companion"): finite position, 16-bit facing, 0 < hp <= max hp.
  // The position is not checked against the level's bounds (no documented
  // bounds contract is bound here).
  auto refuse = [](const char *why) {
    Dictionary bad;
    bad["ok"] = false;
    bad["error"] = why;
    return bad;
  };
  if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
    return refuse("saved position is not finite");
  if (facing < 0 || facing > 0xffff) return refuse("saved facing is outside 16 bits");
  if (!fight_ || !fight_->player_ready()) return refuse("no shared-World player");
  if (hp_raw < 0 || hp_raw > 0xffffffffll) return refuse("saved hit points are outside 32 bits");
  {
    const auto raw = static_cast<rta::Raw>(hp_raw);
    float hp = 0, max_hp = 0;
    const auto max_raw = fight_->player().max_hp;
    std::memcpy(&hp, &raw, sizeof(hp));
    std::memcpy(&max_hp, &max_raw, sizeof(max_hp));
    if (!std::isfinite(hp) || !(hp > 0.0f) || !(hp <= max_hp))
      return refuse("saved hit points are outside (0, max]");
  }
  Dictionary out = fight_place_player(position, facing);
  if (!bool(out.get("ok", false))) return out;
  const auto error = fight_->restore_health(rta::Raw(hp_raw & 0xffffffffll));
  out["ok"] = error.empty();
  if (!error.empty()) out["error"] = String::utf8(error.c_str());
  auto o = control_ ? control_->observation() : current_;
  apply_fight(o);
  cache_state(o, true);
  return out;
}
// DIAGNOSTIC ONLY, not an original routine: writes an unchecked position and
// facing (no terrain or bounds test). Used by scripted test runs and, through
// fight_restore_player, the .fight save load. Ordinary play never calls it.
Dictionary ChampionsSessionController::fight_place_player(const Vector3 &position, int64_t facing) {
  Dictionary out;
  out["ok"] = false;
  if (!fight_ || !fight_->player_ready()) {
    out["error"] = "no shared-World player";
    return out;
  }
  auto bits = [](double value) {
    const float narrowed = static_cast<float>(value);
    rta::Raw raw;
    std::memcpy(&raw, &narrowed, sizeof(raw));
    return raw;
  };
  const auto error = fight_->place_player({bits(position.x), bits(position.y), bits(position.z)},
                                          rta::Raw(facing & 0xffff));
  out["ok"] = error.empty();
  if (!error.empty()) out["error"] = String::utf8(error.c_str());
  return out;
}
Dictionary ChampionsSessionController::disable_level_script() {
  remember_script_fade();
  release_help_pause();
  scripts_.reset();
  reset_fight();
  script_level_objects_.clear();
  live_triggers_.clear();
  for (auto &list : trigger_lists_) list.clear();
  trigger_refusal_.clear();
  trigger_samples_ = trigger_dispatches_ = trigger_missing_ = 0;
  trigger_queued_dispatches_ = 0;
  trigger_queued_destination_.clear();
  trigger_states_.clear();
  trigger_level_name_.clear();
  hub_savepoints_.clear();
  script_seed_.reset();
  script_world_.reset();
  dialog_data_.reset();
  Dictionary result;
  result["ok"] = true;
  return result;
}
namespace {
Dictionary dialog_count_dict(const rta::script_commands::DialogCounts &c) {
  Dictionary d;
  d["uses"] = int64_t(c.uses);
  d["use_no_candidate"] = int64_t(c.use_no_candidate);
  d["use_ambiguous"] = int64_t(c.use_ambiguous);
  d["use_blocked_open"] = int64_t(c.use_blocked_open);
  d["start_messages"] = int64_t(c.start_messages);
  d["opens"] = int64_t(c.opens);
  d["open_gate_refused"] = int64_t(c.open_gate_refused);
  d["nodes_shown"] = int64_t(c.nodes_shown);
  d["choices_taken"] = int64_t(c.choices_taken);
  d["scripts_run"] = int64_t(c.scripts_run);
  d["missing_entries"] = int64_t(c.missing_entries);
  d["ends"] = int64_t(c.ends);
  d["pending_levels"] = int64_t(c.pending_levels);
  d["post_actions"] = int64_t(c.post_actions);
  d["deferred_shops"] = int64_t(c.deferred_shops);
  d["shop_requests"] = int64_t(c.shop_requests);
  d["stalls"] = int64_t(c.stalls);
  d["client_leaves"] = int64_t(c.client_leaves);
  d["client_mutex_releases"] = int64_t(c.client_mutex_releases);
  d["ticks_open"] = int64_t(c.ticks_open);
  d["voice_lines"] = int64_t(c.voice_lines);
  d["silent_lines"] = int64_t(c.silent_lines);
  d["anm_reads"] = int64_t(c.anm_reads);
  d["camera_views"] = int64_t(c.camera_views);
  d["camera_steps"] = int64_t(c.camera_steps);
  d["no_object_talker_records"] = int64_t(c.no_object_talker_records);
  int64_t refused = 0;
  for (const auto &entry : c.refused)
    refused += int64_t(entry.second);
  d["refused"] = refused;
  // Refusals by reason (the reasons never carry level, object or text names).
  Dictionary reasons;
  for (const auto &entry : c.refused)
    reasons[String::utf8(entry.first.c_str())] = int64_t(entry.second);
  d["refused_reasons"] = reasons;
  Dictionary interact;
  for (const auto &entry : c.interact)
    interact[String::utf8(entry.first.c_str())] = int64_t(entry.second);
  d["interact"] = interact;
  Dictionary mouth;
  for (const auto &entry : c.mouth)
    mouth[String::utf8(entry.first.c_str())] = int64_t(entry.second);
  d["mouth"] = mouth;
  return d;
}
} // namespace
Dictionary ChampionsSessionController::dialog_use() {
  Dictionary out;
  out["ok"] = bool(scripts_ && scripts_->dialog_attached() && !scripts_->hud().help_open);
  if (scripts_ && fight_level_ == "hub2") {
    out["ok"] = false;
    dialog_last_use_ = "Refused: alternate-hub object-to-dialog choice effects are not audited.";
    out["error"] = dialog_last_use_;
  }
  if (out["ok"])
    dialog_use_pending_ = true;
  return out;
}
Array ChampionsSessionController::dialog_events() {
  Array out;
  if (!scripts_ || !scripts_->dialog_attached())
    return out;
  for (const auto &e : scripts_->take_dialog_events()) {
    Dictionary item;
    item["kind"] = String::utf8(e.kind.c_str());
    Array args;
    for (const auto &a : e.args) {
      if (const auto *i = std::get_if<std::int64_t>(&a))
        args.push_back(*i);
      else if (const auto *t = std::get_if<std::string>(&a))
        args.push_back(String::utf8(t->c_str()));
      else
        args.push_back(Variant());
    }
    item["args"] = args;
    out.push_back(item);
  }
  return out;
}
namespace {
double raw_float(rta::Raw bits) {
  float value;
  static_assert(sizeof(value) == sizeof(bits));
  std::memcpy(&value, &bits, sizeof(value));
  return double(value);
}
} // namespace
// The conversation camera: the words Camera_setupDialogView wrote and the pose of the
// last Camera_apply (docs/spec/dialog-gaps.md 2). Positions are the console's floats.
Dictionary ChampionsSessionController::dialog_camera() const {
  Dictionary out;
  const bool attached = scripts_ && scripts_->dialog_attached();
  out["ok"] = attached;
  if (!attached)
    return out;
  const auto *view = scripts_->dialog_camera_view();
  const auto *pose = scripts_->dialog_camera_pose();
  out["view"] = view != nullptr;
  out["pose"] = pose != nullptr;
  if (view) {
    out["progress"] = raw_float(view->progress);
    out["turn_factor"] = raw_float(view->k);
    out["target"] = Vector3(float(raw_float(view->target[0])), float(raw_float(view->target[1])),
                            float(raw_float(view->target[2])));
    out["target_yaw"] = int64_t(view->target_yaw);
    out["target_pitch"] = int64_t(view->target_pitch);
  }
  if (pose) {
    out["position"] = Vector3(float(raw_float(pose->pos[0])), float(raw_float(pose->pos[1])),
                              float(raw_float(pose->pos[2])));
    out["yaw"] = int64_t(pose->yaw);
    out["pitch"] = int64_t(pose->pitch);
    out["forward_z"] = raw_float(pose->matrix[8]);
  }
  out["moving"] = scripts_->dialog().cam_move != 0;
  out["open"] = scripts_->dialog().is_open();
  return out;
}
// The outcome of one dialog_isOpen site with the other conditions it reads (integers).
Dictionary ChampionsSessionController::dialog_site_gate(const String &site, const Dictionary &conditions) const {
  Dictionary out;
  out["ok"] = false;
  if (!scripts_ || !scripts_->dialog_attached()) {
    out["error"] = "No dialog is attached to the level script.";
    return out;
  }
  try {
    rta::dialog_gaps::Conditions c;
    const Array keys = conditions.keys();
    for (int i = 0; i < keys.size(); ++i)
      c[String(keys[i]).utf8().get_data()] = int64_t(conditions[keys[i]]);
    out["outcome"] = String::utf8(scripts_->dialog_gate(site.utf8().get_data(), c).c_str());
    out["ok"] = true;
  } catch (const std::exception &e) {
    out["error"] = String::utf8(e.what());
  }
  return out;
}
Dictionary ChampionsSessionController::dialog_voice_state(const Dictionary &voice) {
  Dictionary out;
  // held_ready stays 1: a held level entry is resident at once here
  // (docs/DIALOG.md "Voice").
  dialog_voice_ = std::array<int64_t, 3>{int64_t(voice.get("queue_count", int64_t(0))),
                                         int64_t(voice.get("status", int64_t(0))),
                                         int64_t(voice.get("position", int64_t(0)))};
  out["ok"] = true;
  return out;
}
Dictionary ChampionsSessionController::dialog_talk_to(int64_t index) {
  Dictionary out;
  out["ok"] = bool(scripts_ && scripts_->dialog_attached() && !scripts_->hud().help_open) && index >= 0 &&
              std::size_t(index) < scripts_->talkers().size();
  if (scripts_ && fight_level_ == "hub2") {
    out["ok"] = false;
    dialog_last_use_ = "Refused: alternate-hub object-to-dialog choice effects are not audited.";
    out["error"] = dialog_last_use_;
  }
  if (out["ok"])
    dialog_talk_pending_ = index;
  return out;
}
Dictionary ChampionsSessionController::dialog_press(int64_t bits) {
  Dictionary out;
  // Only the three documented pad words (docs/dialog.md 8).
  const uint32_t allowed = rta::dialog::UP_BITS | rta::dialog::DOWN_BITS | 0x40u;
  out["ok"] = bool(scripts_ && scripts_->dialog_attached() && !scripts_->hud().help_open) && bits >= 0 && (uint32_t(bits) & ~allowed) == 0;
  if (out["ok"])
    dialog_pad_ |= uint32_t(bits);
  return out;
}
Dictionary ChampionsSessionController::dialog_leave() {
  Dictionary out;
  out["ok"] = bool(scripts_ && scripts_->dialog_leave());
  return out;
}
Dictionary ChampionsSessionController::dialog_status() const {
  Dictionary out;
  const bool attached = scripts_ && scripts_->dialog_attached();
  out["ok"] = attached;
  if (!attached)
    return out;
  const auto &d = scripts_->dialog();
  out["open"] = d.is_open();
  out["camera_moving"] = d.cam_move != 0;
  out["stalled"] = scripts_->dialog_stalled();
  out["stall_reason"] = String::utf8(scripts_->dialog_stall_reason().c_str());
  out["talkers"] = int64_t(scripts_->talkers().size());
  out["interactables"] = int64_t(scripts_->interactables().size());
  out["mutex_held"] = d.mutex != 0;
  out["last_use"] = dialog_last_use_;
  out["counts"] = dialog_count_dict(scripts_->dialog_counts());
  out["has_font"] = bool(dialog_data_ && dialog_data_->font);
  out["has_text"] = bool(dialog_data_ && dialog_data_->text);
  return out;
}
// dialog_drawTask's calls turned into the box sprite and gamefontmed glyphs
// (research docs/dialog.md 7; layout from champions/ui_text_layout.hpp).
Dictionary ChampionsSessionController::dialog_frame(int64_t text_option) {
  Dictionary out;
  out["ok"] = false;
  try {
    if (!scripts_ || !scripts_->dialog_attached())
      throw std::runtime_error("No dialog is attached to the level script.");
    namespace T = champions::ui_text_layout;
    namespace D = rta::dialog;
    const auto &d = scripts_->dialog();
    out["open"] = d.is_open();
    out["stalled"] = scripts_->dialog_stalled();
    out["stall_reason"] = String::utf8(scripts_->dialog_stall_reason().c_str());
    // The host's displayed-frame ramp supplies g_uiFadeIn[0]. The observed layout
    // supplies the cursor and "more" glyph (docs/UI_TEXT_GAPS.md).
    const auto ops = scripts_->dialog_draw(scripts_->ui_ramps().fade[0], int(text_option),
                                           int(scripts_->dialog_cursor_glyph()), 0);
    Array sprites, glyphs, refusals;
    const auto *font = dialog_data_ && dialog_data_->font ? &*dialog_data_->font : nullptr;
    if (!ops.empty() && !font)
      refusals.push_back("gamefontmed.fnt was not supplied: no dialog text drawn");
    std::uint32_t colour = 0x80808080u;
    std::int64_t budget = -1;
    std::array<std::int64_t, 4> clip{0, 0, 640, 448};
    const auto num = [](const D::Arg &a) { return std::get<std::int64_t>(a); };
    const auto lookup = [&](const std::string &key) -> D::Wide {
      if (!dialog_data_ || !dialog_data_->text)
        throw D::Refused("no text table");
      return dialog_data_->text->lookup(key);
    };
    std::int64_t cursor_quads = 0;
    const auto emit = [&](int x, int y, const D::Wide &units, std::size_t count) -> std::size_t {
      if (!font)
        return 0;
      std::size_t made = 0;
      T::TextState state;
      state.setColour(colour);
      for (const auto &g : T::glyphRun(*font, x, y, units, count, &state).glyphs) {
        Dictionary item;
        item["x_half"] = g.x_half;
        item["y"] = g.y;
        item["source_x"] = g.source.x;
        item["source_y"] = g.source.y;
        item["source_width"] = g.source.width;
        item["source_height"] = g.source.height;
        item["tint"] = int64_t(g.tint);
        item["clip"] = Rect2i(int(clip[0]), int(clip[1]), int(clip[2] - clip[0]), int(clip[3] - clip[1]));
        glyphs.push_back(item);
        ++made;
      }
      return made;
    };
    for (const auto &e : ops) {
      const auto &a = e.args;
      if (e.kind == "text_begin") {
        colour = std::uint32_t(num(a[1]));
      } else if (e.kind == "colour") {
        colour = std::uint32_t(num(a[0]));
      } else if (e.kind == "budget") {
        budget = num(a[0]);
      } else if (e.kind == "clip") {
        clip = {num(a[0]), num(a[1]), num(a[2]), num(a[3])};
      } else if (e.kind == "wrapped" && font) {
        const auto &key = std::get<std::string>(a[2]);
        const auto units = key == "body" ? d.text : lookup(key);
        const auto w = T::drawWrapped(*font, int(num(a[0])), int(num(a[1])), units, int(num(a[3])), int(num(a[4])),
                                      int(num(a[5])), int(num(a[6])), true, 0,
                                      budget < 0 ? std::size_t(-1) : std::size_t(budget));
        for (const auto &run : w.runs)
          emit(run.x, run.y, D::Wide(units.begin() + std::ptrdiff_t(run.start), units.end()), run.count);
      } else if (e.kind == "wide") {
        const auto units = lookup(std::get<std::string>(a[2]));
        emit(int(num(a[0])), int(num(a[1])), units, std::size_t(num(a[3])));
      } else if (e.kind == "wide_centered" && font) {
        const auto &v = std::get<std::vector<std::int64_t>>(a[2]);
        const D::Wide units(v.begin(), v.end());
        emit(T::centeredXWide(*font, int(num(a[0])), units), int(num(a[1])), units, std::size_t(num(a[3])));
      } else if (e.kind == "ascii") {
        // text_drawAscii of "%c" with the glyph byte: one character at (x, y).
        const auto code = std::uint16_t(num(a[2]) & 0xff);
        if (code != 0)
          cursor_quads += std::int64_t(emit(int(num(a[0])), int(num(a[1])), D::Wide{code}, 1));
      } else if (e.kind == "sprite") {
        Dictionary box;
        box["x"] = num(a[0]);
        box["y"] = num(a[1]);
        box["layer"] = num(a[2]);
        box["tint"] = num(a[3]);
        box["width"] = D::BOX_TEXELS_W;
        box["height"] = D::BOX_TEXELS_H;
        sprites.push_back(box);
      } else if (e.kind == "fit" || e.kind == "ascii_centered") {
        refusals.push_back("remote player name: no remote players in this host");
      }
    }
    out["sprites"] = sprites;
    out["glyphs"] = glyphs;
    out["cursor_glyph_quads"] = cursor_quads;
    out["refusals"] = refusals;
    out["ok"] = true;
  } catch (const std::exception &e) {
    out["error"] = String::utf8(e.what());
  }
  return out;
}
Dictionary ChampionsSessionController::script_command_counts() const {
  Dictionary result;
  if (!scripts_) {
    result["ok"] = false;
    return result;
  }
  const auto &counts = scripts_->counts();
  result["ok"] = true;
  result["started"] = static_cast<int64_t>(counts.started);
  result["reached_wait"] = static_cast<int64_t>(counts.reached_wait);
  result["reached_complete"] = static_cast<int64_t>(counts.reached_complete);
  result["faults"] = static_cast<int64_t>(counts.faults);
  result["missing_entries"] = static_cast<int64_t>(counts.missing_entries);
  result["refusal_stops"] = static_cast<int64_t>(counts.refusal_stops);
  result["objects_registered"] = static_cast<int64_t>(counts.objects_registered);
  result["objects_refused"] = static_cast<int64_t>(counts.objects_refused);
  result["stub_refusals"] = static_cast<int64_t>(counts.stub_refusals);
  result["callee_refusals"] = static_cast<int64_t>(counts.callee_refusals);
  result["unknown_player_current_refusals"] = static_cast<int64_t>(counts.unknown_player_current_refusals);

  result["objects_built"] = static_cast<int64_t>(counts.objects_built);
  result["objects_unbuilt"] = static_cast<int64_t>(counts.objects_unbuilt);
  result["objects_not_created"] = static_cast<int64_t>(counts.objects_not_created);
  result["objects_pool_refused"] = static_cast<int64_t>(counts.objects_pool_refused);
  result["objects_unclassified"] = static_cast<int64_t>(counts.objects_unclassified);
  result["load_draws_unmodelled"] = static_cast<int64_t>(counts.load_draws_unmodelled);
  result["item_load_gate_refusals"] = static_cast<int64_t>(counts.item_load_gate_refusals);
  result["item_load_draws_inferred"] = static_cast<int64_t>(counts.item_load_draws_inferred);
  result["unbuilt_constructors"] = static_cast<int64_t>(counts.unbuilt_constructors.size());
  // Both WarBoar names share the factory constructor. An unbuilt record is a
  // stub, not a live combat actor.
  const auto boars = counts.unbuilt_constructors.find(0x2ba03c);
  result["warboar_unbuilt"] = static_cast<int64_t>(
      boars == counts.unbuilt_constructors.end() ? 0 : boars->second);
  // Report the Minotaur constructor separately. This gives no evidence of a
  // Gold drop or credit.
  const auto minotaurs = counts.unbuilt_constructors.find(0x288fdc);
  result["minotaur_unbuilt"] = static_cast<int64_t>(
      minotaurs == counts.unbuilt_constructors.end() ? 0 : minotaurs->second);
  Dictionary build_refusals;
  for (const auto &kv : counts.build_refusals)
    build_refusals[String::utf8(kv.first.c_str())] = static_cast<int64_t>(kv.second);
  result["build_refusals"] = build_refusals;
  Dictionary stub_reasons;
  for (const auto &kv : counts.stub_refusal_reasons)
    stub_reasons[String::utf8(kv.first.c_str())] = static_cast<int64_t>(kv.second);
  result["stub_refusal_reasons"] = stub_reasons;
  result["seed_bound"] = bool(script_seed_);
  result["seed_paired"] = script_world_.has_value();
  result["seed_unpaired"] = script_seed_unpaired_;
  Dictionary executed, refused, inferred;

  for (std::size_t i = 0; i < rta::script_commands::COMMANDS; ++i) {
    const auto name = String::utf8(rta::script_commands::row(i).name);
    if (counts.executed[i])
      executed[name] = static_cast<int64_t>(counts.executed[i]);
    if (counts.refused[i])
      refused[name] = static_cast<int64_t>(counts.refused[i]);
    if (counts.inferred[i])
      inferred[name] = static_cast<int64_t>(counts.inferred[i]);
  }
  result["executed"] = executed;
  result["refused"] = refused;
  result["inferred"] = inferred;
  // The quest and mission campaign the host's commands wrote (QUESTS.md).
  const auto &campaign = scripts_->campaign();
  Dictionary quest;
  quest["num_players"] = int64_t(campaign.num_players());
  int64_t lines = 0, available = 0, complete = 0;
  for (int slot = 0; slot < campaign.num_players(); ++slot) {
    const auto &record = campaign.record(slot);
    lines += record.quest_count;
    for (const auto mission : record.missions) {
      available += mission & 1;
      complete += (mission >> 1) & 1;
    }
  }
  quest["quest_lines"] = lines;
  quest["mission_available_bits"] = available;
  quest["mission_complete_bits"] = complete;
  int64_t nonzero = 0;
  for (const auto &var : campaign.vars())
    nonzero += var.value != 0;
  quest["vars"] = int64_t(campaign.vars().size());
  quest["vars_nonzero"] = nonzero;
  const auto &events = campaign.events();
  quest["menu_opens"] = int64_t(events.menu_opens);
  quest["menu_launches"] = int64_t(events.menu_launches);
  quest["cflag_bit0"] = int64_t(events.cflag_bit0);
  quest["cflag_bit26"] = int64_t(events.cflag_bit26);
  quest["boss_music"] = int64_t(events.boss_music);
  quest["exp_grants"] = int64_t(events.exp_grants);
  result["campaign"] = quest;
  if (scripts_->dialog_attached())
    result["dialog"] = dialog_count_dict(scripts_->dialog_counts());
  return result;
}
namespace {
bool is_exit_record(const std::string &name) {
  return name == "Trigger" || name == "PushTrigger" || name == "JumpGate" ||
         name == "Portal" || name == "PortalHurt";
}
bool unsupported_exit_conditions(const rta::level_flow::Record &record,
                                 const std::vector<rta::level_flow::Record> &objects) {
  if (record.cls == "PushTrigger")
    return record.prop("shop") || record.prop("offOnly") || record.prop("once");
  if (record.cls != "Trigger") return false;
  for (const char *name : {"onOnly", "offOnly", "timeout", "needScriptHit"})
    if (record.prop(name)) return true;
  // Linking TriggerParams requires the original rectangle search and re-arm
  // state. Refuse conservatively when this level contains one.
  for (const auto &other : objects)
    if (other.cls == "TriggerParams") return true;
  return false;
}
}
Dictionary ChampionsSessionController::script_level_request() const {
  Dictionary result;
  result["ok"] = bool(scripts_);
  if (!scripts_) return result;
  // newWorld writes the binder's next-world word; dialog end and mission menu
  // also mirror into level_flow::Pending. Both use the researched 0x41b6d0 word.
  std::string name = scripts_->level().pending.name;
  if (name.empty()) name = scripts_->variables().memory.cstr(0x41b6d0);
  result["queued"] = !name.empty();
  result["name"] = String::utf8(name.c_str());
  result["orientation"] = int64_t(scripts_->variables().memory.read(0x41b720));
  result["entrance"] = String::utf8(scripts_->level().entrance.c_str());
  result["post_action_pending"] = !scripts_->variables().memory.cstr(0x86fa80).empty();
  return result;
}
Dictionary ChampionsSessionController::script_mission_menu_state() const {
  Dictionary out;
  out["ok"] = bool(scripts_ && executable_ && scripts_->campaign().rows().size() == 13);
  if (!bool(out["ok"])) return out;
  try {
    const auto &campaign = scripts_->campaign();
    const auto &menu = campaign.menu();
    out["open"] = campaign.menu_open() && menu.pending.empty() &&
                  !(menu.state == 4 && menu.counter <= 0);
    out["state"] = int64_t(menu.state);
    out["row"] = int64_t(menu.row);
    out["column"] = int64_t(menu.col);
    out["counter"] = int64_t(menu.counter);
    out["mark"] = int64_t(menu.mark);
    out["evil"] = bool(campaign.evil());
    out["network"] = campaign.network();
    out["chooser"] = int64_t(campaign.chooser());
    out["first_story_available"] = bool(campaign.mission_available(1));
    out["pending"] = String::utf8(menu.pending.c_str());
    Array rows;
    constexpr std::uint32_t table = 0x4fe540u, stride = 0x34u;
    for (std::uint32_t i = 0; i < 12; ++i) {
      const auto base = table + i * stride;
      const auto name = [&](std::uint32_t offset) {
        const auto ptr = executable_->word(base + offset);
        return ptr ? executable_->string(ptr) : std::string();
      };
      const auto &info = campaign.rows()[i];
      const int mission = int(i) + 1;
      const bool available = campaign.mission_available(mission) != 0;
      const bool complete = campaign.mission_complete(mission) != 0;
      const auto medal_byte = campaign.record(0).medals[mission];
      const int medal = medal_byte < 128 ? int(medal_byte) : int(medal_byte) - 256;
      Dictionary row;
      row["key"] = String::utf8(name(0x00).c_str());
      row["evil_key"] = String::utf8(name(0x04).c_str());
      row["image"] = String::utf8(name(0x18).c_str());
      row["evil_image"] = String::utf8(name(0x1c).c_str());
      row["x"] = int64_t(std::int32_t(executable_->word(base + 0x2c)));
      row["y"] = int64_t(std::int32_t(executable_->word(base + 0x30)));
      row["available"] = available;
      row["draw_present"] = i == 0 || available;
      row["complete"] = complete;
      row["medal"] = int64_t(medal);
      row["medal_column"] = bool(rta::quests::helper_value(6, info, available, complete, medal));
      row["bonus_column"] = bool(rta::quests::helper_value(10, info, available, complete, medal));
      rows.push_back(row);
    }
    out["rows"] = rows;
  } catch (const std::exception &e) { return failure(e); }
  return out;
}
Dictionary ChampionsSessionController::script_mission_menu_tick(int64_t pad_bits, int64_t fade) {
  try {
    if (!scripts_ || scripts_->campaign().rows().size() != 13 || !scripts_->campaign().menu_open())
      throw std::runtime_error("No documented mission menu is open.");
    constexpr std::uint32_t allowed = rta::quests::BTN_CROSS | rta::quests::BTN_CIRCLE |
                                      rta::quests::BTN_SELECT | rta::quests::BTN_UP |
                                      rta::quests::BTN_DOWN | rta::quests::BTN_LEFT |
                                      rta::quests::BTN_RIGHT | 0x200000u | 0x400000u;
    if (pad_bits < 0 || (std::uint64_t(pad_bits) & ~std::uint64_t(allowed)) != 0 || fade < 0 || fade > 128)
      throw std::runtime_error("Mission menu input is outside the documented pad and fade range.");
    rta::quests::MenuInputs inputs;
    inputs.fade = int(fade);
    const bool launched = scripts_->tick_mission_menu(int(pad_bits), inputs);
    auto out = script_mission_menu_state();
    out["launched"] = launched;
    return out;
  } catch (const std::exception &e) { return failure(e); }
}
Array ChampionsSessionController::script_exit_targets() const {
  Array result;
  if (!scripts_) return result;
  for (std::size_t i = 0; i < script_level_objects_.size(); ++i) {
    const auto &record = script_level_objects_[i];
    const auto &kind = record.cls;
    if (!is_exit_record(kind)) continue;
    Dictionary item;
    item["index"] = int64_t(i);
    item["kind"] = String::utf8(kind.c_str());
    item["position"] = Vector3(display_float(record.position[0]), display_float(record.position[1]),
                                 display_float(record.position[2]));
    if (kind == "Trigger") {
      const auto live = std::find_if(live_triggers_.begin(), live_triggers_.end(),
          [i](const auto &t) { return t.record == i; });
      item["walk_in"] = live != live_triggers_.end() && !live->refused;
      if (live != live_triggers_.end() && live->refused)
        item["walk_in_refusal"] = String::utf8(trigger_refusal_.c_str());
      if (live != live_triggers_.end()) {
        item["min"] = Vector3(display_float(live->body.min[0]), display_float(live->body.min[1]), 0);
        item["max"] = Vector3(display_float(live->body.max[0]), display_float(live->body.max[1]), 0);
      }
    }
    // A Portal is an arrival marker; JumpGate's script is a prompt predicate.
    // Neither is a direct script action. The host exposes them without claiming
    // an undocumented interaction or gate destination.
    item["script_action"] = !unsupported_exit_conditions(record, script_level_objects_) &&
        (kind == "Trigger" ? record.prop("t").has_value()
         : kind == "PushTrigger" ? record.prop("action").has_value() : false);
    result.push_back(item);
  }
  return result;
}
Dictionary ChampionsSessionController::inspect_exit_records(const PackedByteArray &objects) const {
  Dictionary result;
  result["ok"] = false;
  try {
    const auto raw = raw_bytes(objects);
    const auto records = rta::level_flow::parse_objects({raw.data(), raw.size()});
    int64_t triggers = 0, pushes = 0, gates = 0, portals = 0;
    for (const auto &record : records) {
      triggers += record.cls == "Trigger";
      pushes += record.cls == "PushTrigger";
      gates += record.cls == "JumpGate";
      portals += record.cls == "Portal" || record.cls == "PortalHurt";
    }
    result["ok"] = true;
    result["triggers"] = triggers;
    result["pushes"] = pushes;
    result["gates"] = gates;
    result["portals"] = portals;
  } catch (const std::exception &e) {
    result["error"] = String::utf8(e.what());
  }
  return result;
}
Dictionary ChampionsSessionController::script_trigger_status() const {
  Dictionary result;
  result["samples"] = int64_t(trigger_samples_);
  result["dispatches"] = int64_t(trigger_dispatches_);
  result["missing"] = int64_t(trigger_missing_);
  result["queued_dispatches"] = int64_t(trigger_queued_dispatches_);
  result["queued_record"] = int64_t(trigger_queued_record_);
  result["queued_destination"] = String::utf8(trigger_queued_destination_.c_str());
  result["refusal"] = String::utf8(trigger_refusal_.c_str());
  return result;
}
Array ChampionsSessionController::script_player_markers() const {
  Array result;
  if (!scripts_) return result;
  for (std::size_t i = 0; i < script_level_objects_.size(); ++i) {
    const auto &record = script_level_objects_[i];
    if (record.cls != "Player1") continue;
    Dictionary item;
    item["index"] = int64_t(i);
    PackedStringArray properties;
    for (const auto &property : record.props)
      properties.push_back(String::utf8(property.c_str()));
    item["properties"] = properties;
    result.push_back(item);
  }
  return result;
}
Dictionary ChampionsSessionController::script_run_exit(int64_t index) {
  Dictionary result;
  result["ok"] = false;
  try {
    if (!scripts_ || index < 0 || std::size_t(index) >= script_level_objects_.size())
      throw std::runtime_error("No level exit at that index.");
    if (fight_level_ == "hub2")
      throw std::runtime_error("Alternate-hub trigger entry effects are not audited; no action was run.");
    const auto &record = script_level_objects_[std::size_t(index)];
    const auto &kind = record.cls;
    const auto portal_before = scripts_->portal_trigger_requests();
    if (kind != "Trigger" && kind != "PushTrigger")
      throw std::runtime_error("Portal arrival and JumpGate prompt have no direct script action in this host.");
    if (unsupported_exit_conditions(record, script_level_objects_))
      throw std::runtime_error("This exit has conditions or lifetime state the host cannot evaluate.");
    if (kind == "PushTrigger") {
      // The object table preserves duplicate properties in their source order.
      // PushTrigger checks every key before running every action in that order.
      for (const auto &property : record.props) {
        if (property.rfind("key=", 0) != 0) continue;
        const auto stop = scripts_->run_object_entry(std::size_t(index), property.substr(4));
        if (stop != rta::script_commands::Stop::Complete)
          throw std::runtime_error("Exit key script was refused.");
        if (scripts_->last_value() == 0)
          throw std::runtime_error("Exit key script has not unlocked this action.");
      }
    }
    bool ran = false;
    const std::string prefix = kind == "Trigger" ? "t=" : "action=";
    for (const auto &property : record.props) {
      if (property.rfind(prefix, 0) != 0 || property.size() == prefix.size()) continue;
      ran = true;
      const auto stop = scripts_->run_object_entry(std::size_t(index), property.substr(prefix.size()));
      if (stop == rta::script_commands::Stop::Missing || stop == rta::script_commands::Stop::Fault ||
          stop == rta::script_commands::Stop::Refused)
        throw std::runtime_error("Exit script stopped or was refused.");
      if (kind == "Trigger") break;
    }
    if (!ran) throw std::runtime_error("This exit has no documented script action.");
    result["ok"] = true;
    result["queued"] = bool(script_level_request().get("queued", false));
    result["portal_requests"] = int64_t(scripts_->portal_trigger_requests() - portal_before);
  } catch (const std::exception &e) {
    result["error"] = String::utf8(e.what());
    if (scripts_ && !scripts_->last_fault().empty())
      result["script_fault"] = String::utf8(scripts_->last_fault().c_str());
  }
  return result;
}
Dictionary ChampionsSessionController::start_play_session(bool combat_fixture, bool level_fixture, bool animated_fixture,
                                                          bool full_fixture) {
  try {
    loaded_presentation_.reset();
    auto setup = active_setup();
    if (setup.config.recovery || setup.config.facing ||
        setup.config.animation_cache)
      throw std::runtime_error("Play sessions do not support recovery, facing "
                               "or animation-cache diagnostics.");
    if (animated_fixture && (!combat_fixture || level_fixture)) throw std::runtime_error("Animated fixture requires the combat fixture exclusively.");
    if (combat_fixture && level_fixture) throw std::runtime_error("Fixture policies are mutually exclusive.");
    if (full_fixture && (!combat_fixture || animated_fixture || level_fixture))
      throw std::runtime_error("Full combat fixture requires the combat fixture exclusively.");
    if (full_fixture && !fx_constants_)
      throw std::runtime_error("Full combat fixture requires effect recipe constants (fx_skill_recipes JSON).");
    if (level_fixture) {
      auto next = std::make_unique<rta::fixture_save::LevelScene>();
      auto state = next->observation();
      auto terrain = present_terrain(next->terrain().data);
      prepare_meshes(terrain);
      setup.initial_terrain = next->terrain(); setup.supplied_terrain = true;
      combat_setup_ = setup;
      // The fixture's seed is its own journaled one: a level script's
      // draws no longer share the session's stream.
      unpair_script_seed();
      reset_fight();
      world_adapter_.reset(); world_runtime_.reset(); world_authority_.reset();
      level_ = std::move(next); combat_.reset(); control_.reset(); pending_.reset();
      player_services_.reset(); player_water_image_.reset(); player_service_level_.reset();
      terrain_ = terrain; terrain_name_ = "Synthetic gate levels aa / aab";
      cache_state(state, true); clock_.reset();
      return observation();
    }
    if (combat_fixture) {
      const auto which = full_fixture ? rta::combat_play::Fixture::Full
                         : animated_fixture ? rta::combat_play::Fixture::Animated : rta::combat_play::Fixture::Combat;
      auto next = full_fixture ? std::make_unique<rta::fixture_save::CombatScene>(
                                     setup.initial_terrain, which, fx_constants_, fx_constants_identity_)
                               : std::make_unique<rta::fixture_save::CombatScene>(setup.initial_terrain, which);
      auto state = next->observation();
      unpair_script_seed();
      reset_fight();
      world_adapter_.reset(); world_runtime_.reset(); world_authority_.reset();
      combat_setup_ = setup;
      combat_ = std::move(next);
      attach_effects();
      level_.reset();
      control_.reset(); // The combat World is the sole actor owner.
      player_services_.reset(); player_water_image_.reset(); player_service_level_.reset();
      pending_.reset();
      cache_state(state, true);
      return observation();
    }
    setup.config.movement = rta::session_control::MovementPolicy::DryCreature;
    setup.config.max_ready_ticks =
        rta::session_control::MAX_CREATURE_READY_TICKS;
    setup.config.play_window = rta::play_session::DEFAULT_WINDOW;
    // An attached level script's load drew first: the World continues that
    // stream and creates the player after it, as the original builds the
    // players after the level's objects and its start entry
    // (docs/issues/arena-entry.md 1.4).
    const bool pair = scripts_ && script_seed_;
    if (pair)
      setup.config.shared_seed = script_seed_->state;
    auto next = std::make_unique<rta::native_save::Scene>(
        setup.config,
        setup.supplied_terrain ? std::optional<rta::world_queries::LevelData>(
                                     setup.initial_terrain.data)
                               : std::nullopt,
        setup.initial_terrain.limits);
    next->release_input();
    auto state = next->observation();
    Dictionary result = present_observation(state);
    const Vector3 position(display_float(state.position[0]),
                           display_float(state.position[1]),
                           display_float(state.position[2]));
    const double facing =
        double(state.facing & 0xffffu) * (6.283185307179586 / 65536.0);
    auto new_authority = world_authority_ ? nullptr : std::make_shared<rta::WorldAuthority>();
    auto new_runtime = new_authority
        ? std::make_unique<rta::combat_runtime::World>(new_authority,
                                                       rta::combat_runtime::Services{})
        : nullptr;
    auto new_adapter = new_runtime
        ? std::make_unique<rta::combat_runtime::SessionAdapter>(*new_runtime)
        : nullptr;
    // All allocations/validation precede publication. Terrain and visual owners
    // remain untouched. The remaining native moves and scalar stores cannot
    // fail.
    control_.swap(next);
    if (new_authority) {
      world_authority_ = std::move(new_authority);
      world_runtime_ = std::move(new_runtime);
      world_adapter_ = std::move(new_adapter);
    }
    combat_.reset();
    level_.reset();
    pending_.reset();
    if (pair) {
      script_seed_->state = control_->shared_seed();
      script_world_ = script_seed_->state;
    }
    start_fight(state);
    current_ = std::move(state);
    previous_position_ = current_position_ = position;
    previous_facing_ = current_facing_ = facing;
    clock_.reset();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::reset_session() {
  try {
    remember_script_fade();
    scripts_.reset();
    reset_fight();
    world_adapter_.reset(); world_runtime_.reset(); world_authority_.reset();
    help_paused_ = false;
    loaded_presentation_.reset();
    pending_.reset();
    if (combat_) {
      const auto which = combat_->fixture();
      auto next = which == rta::combat_play::Fixture::Full
                      ? std::make_unique<rta::fixture_save::CombatScene>(combat_setup_.initial_terrain, which,
                                                                          fx_constants_, fx_constants_identity_)
                      : std::make_unique<rta::fixture_save::CombatScene>(combat_setup_.initial_terrain, which);
      combat_.swap(next);
      attach_effects();
    } else if (level_) {
      auto next = std::make_unique<rta::fixture_save::LevelScene>(); level_.swap(next);
    } else control_->reset();
    if (!combat_ && !level_) {
      world_authority_ = std::make_shared<rta::WorldAuthority>();
      world_runtime_ = std::make_unique<rta::combat_runtime::World>(
          world_authority_, rta::combat_runtime::Services{});
      world_adapter_ = std::make_unique<rta::combat_runtime::SessionAdapter>(*world_runtime_);
    }
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_paused(bool value) {
  try {
    if (combat_) combat_->set_paused(value); else if (level_) level_->set_paused(value); else control_->set_paused(value);
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::release_input() {
  try {
    if (combat_) combat_->release_input(); else if (level_) level_->release_input(); else control_->release_input();
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::stop_session() {
  try {
    remember_script_fade();
    scripts_.reset();
    reset_fight();
    world_adapter_.reset(); world_runtime_.reset(); world_authority_.reset();
    help_paused_ = false;
    if (combat_) combat_->stop(); else if (level_) level_->stop(); else control_->stop();
    snap();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::get_terrain() const {
  return terrain_.duplicate(true);
}

Dictionary ChampionsSessionController::present_terrain(
    const rta::world_queries::LevelData &data) {
  // This is an explicit, bounded visual subset of supplied terrain, not an
  // original world renderer. Tiles show the sample heights used by the kernel.
  // Never invent rotated geometry, masked infinite walls or piece materials.
  if (data.elements.size() > 256)
    throw std::runtime_error(
        "This terrain has too many surfaces for the scene preview.");
  if (data.grid.empty() || data.grid.size() > 16384)
    throw std::runtime_error(
        "This terrain grid cannot be displayed in the scene yet.");
  rta::ScopedFpu fixed{rta::Fpu{rta::Fpu::Model::Ps2, false, false}};
  rta::world_queries::Level query(data);
  Array surfaces;
  std::size_t tiles = 0;
  for (std::size_t element_index = 0; element_index < data.elements.size(); ++element_index) {
    const auto &e = data.elements[element_index];
    if (e.flags & 2)
      continue;
    for (const auto coordinate :
         {e.min_x, e.min_y, e.max_x, e.max_y, e.x, e.y, e.z})
      if (coordinate < -1048576 || coordinate > 1048576)
        throw std::runtime_error(
            "This terrain exceeds the scene display's coordinate range.");
    if (e.flags != 0 || e.cos != 32767 || e.sin != 0)
      throw std::runtime_error(
          "This scene currently displays plain, unrotated terrain only.");
    if (e.min_x < double(data.tg_x0) * 160 ||
        e.min_y < double(data.tg_y0) * 160 ||
        e.max_x >= (double(data.tg_x0) + data.tg_w) * 160 ||
        e.max_y >= (double(data.tg_y0) + data.tg_h) * 160)
      throw std::runtime_error(
          "This terrain surface extends beyond its query grid.");
    if (e.min_x >= e.max_x || e.min_y >= e.max_y)
      throw std::runtime_error("This terrain surface has no visible area.");
    // The preview draws the whole element rectangle. Refuse holes: every grid
    // cell touched by that rectangle must list this element for collision.
    const auto col0 = (e.min_x - data.tg_x0 * 160) / 160;
    const auto col1 = (e.max_x - data.tg_x0 * 160) / 160;
    const auto row0 = (e.min_y - data.tg_y0 * 160) / 160;
    const auto row1 = (e.max_y - data.tg_y0 * 160) / 160;
    for (auto row = row0; row <= row1; ++row)
      for (auto col = col0; col <= col1; ++col) {
        const auto &cell = data.grid[std::size_t(row * data.tg_w + col)];
        if (std::find(cell.begin(), cell.end(), int64_t(element_index)) == cell.end())
          throw std::runtime_error("This terrain has partial surface coverage that "
                                   "the scene cannot display yet.");
      }
    const auto index =
        e.hf < 0 ? int64_t(data.heightfields.size()) + e.hf : e.hf;
    const auto &hf = data.heightfields.at(static_cast<std::size_t>(index));
    if (hf.samples.empty())
      continue;
    for (const auto sample : hf.samples)
      if ((uint16_t(sample) & 15) != 0)
        throw std::runtime_error("This terrain uses special collision samples "
                                 "that the scene cannot display yet.");
    const bool flat =
        std::all_of(hf.samples.begin(), hf.samples.end(),
                    [&](auto h) { return h == hf.samples.front(); });
    if (!flat)
      throw std::runtime_error("This scene currently displays constant-height "
                               "terrain surfaces only.");
    const int64_t height = int64_t(hf.samples.front()) / 16;
    if (hf.lo != height || hf.hi != height || hf.w < 3 || hf.h < 3)
      throw std::runtime_error("This terrain's height bounds are outside the "
                               "scene's flat-surface subset.");
    if (e.min_x < double(e.x) + double(hf.x0) * 4 ||
        e.min_y < double(e.y) + double(hf.y0) * 4 ||
        e.max_x > double(e.x) + (double(hf.x0) + hf.w - 1) * 4 ||
        e.max_y > double(e.y) + (double(hf.y0) + hf.h - 1) * 4)
      throw std::runtime_error(
          "This terrain surface extends beyond its height samples.");
    // Cross-check the displayed plane against the exact same native query
    // kernel, on an owned copy. No query stamp or gameplay state escapes.
    const auto raw_z = rta::from_int(uint32_t(height + e.z));
    for (const auto x : {e.min_x + 4, (e.min_x + e.max_x) / 2, e.max_x - 4})
      for (const auto y : {e.min_y + 4, (e.min_y + e.max_y) / 2, e.max_y - 4}) {
        const rta::world_queries::Vec3 p{rta::from_int(uint32_t(x)),
                                         rta::from_int(uint32_t(y)), raw_z};
        if (rta::world_queries::terrain_height_at_point(query, p) != raw_z ||
            rta::world_queries::terrain_height_below(
                query, rta::session_control::RADIUS, p) != raw_z)
          throw std::runtime_error("This terrain's sampled height does not "
                                   "match the supported scene display.");
      }
    tiles += 1;
    if (tiles > 65536)
      throw std::runtime_error(
          "This terrain exceeds the scene's visible tile limit.");
    PackedVector3Array vertices;
    auto tile = [&](int64_t x, int64_t y, int64_t w, int64_t h) {
      // This authored flat fallback has already passed the exact native query
      // checks above. Coordinates below are presentation-only.
      const double x0 = std::max<double>(e.min_x, e.x + x * 4.0);
      const double y0 = std::max<double>(e.min_y, e.y + y * 4.0);
      const double x1 = std::min<double>(e.max_x, e.x + (x + w) * 4.0);
      const double y1 = std::min<double>(e.max_y, e.y + (y + h) * 4.0);
      if (x1 > x0 && y1 > y0)
        quad(vertices, x0, y0, x1, y1, display_float(raw_z));
    };
    tile(hf.x0, hf.y0, hf.w, hf.h);
    if (vertices.size()) {
      Dictionary surface;
      surface["positions"] = vertices;
      surface["flat"] = flat;
      surfaces.push_back(surface);
    }
  }
  if (surfaces.is_empty())
    throw std::runtime_error(
        "The selected terrain has no supported visible ground.");
  Dictionary result;
  result["ok"] = true;
  result["surfaces"] = surfaces;
  result["mode"] = "practice";
  result["note"] =
      String::utf8("Practice ground · collision-plane illustration");
  return result;
}
void ChampionsSessionController::prepare_meshes(Dictionary &terrain) {
  Array surfaces = terrain["surfaces"];
  for (int64_t i = 0; i < surfaces.size(); ++i) {
    Dictionary surface = surfaces[i];
    const PackedVector3Array source = surface["positions"];
    if (source.is_empty() || source.size() % 3)
      throw std::runtime_error("The ground has incomplete triangles.");
    PackedVector3Array mapped;
    mapped.resize(source.size());
    for (int64_t j = 0; j < source.size(); ++j) {
      const auto p = source[j];
      if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::runtime_error(
            "The ground has a position outside the display range.");
      mapped.set(j, Vector3(p.x, p.z, -p.y));
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = mapped;
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    if (mesh->get_surface_count() != 1)
      throw std::runtime_error("The ground mesh could not be prepared.");
    surface["mesh"] = mesh;
    surfaces[i] = surface;
  }
  terrain["surfaces"] = surfaces;
}
// The neutral session display has one material, so every batch of a chunk
// shares one surface (submission order kept). The caps match the textured
// level loader's budgets; the measured peaks and memory are in
// docs/REAL_DATA.md (wave 12). Each triangle costs 36 bytes of positions plus
// its ArrayMesh copy.
constexpr std::size_t MAX_STATIC_SURFACES = 16384;
constexpr std::size_t MAX_STATIC_TRIANGLES = 4000000;
Dictionary ChampionsSessionController::present_geometry(
    const champions::presentation::rta::world_geometry::Geometry &geometry,
    bool build_display) {
  Array surfaces;
  std::size_t triangles = 0, batches = 0;
  for (const auto &chunk : geometry.chunks) {
    if (!chunk.cell || !chunk.packet_index)
      continue;
    const auto &packet = geometry.packets.at(*chunk.packet_index);
    std::size_t chunk_triangles = 0;
    for (const auto &batch : packet.batches)
      chunk_triangles += batch.triangles.size();
    if (!chunk_triangles)
      continue;
    if (std::size_t(surfaces.size()) >= MAX_STATIC_SURFACES ||
        chunk_triangles > MAX_STATIC_TRIANGLES - triangles)
      throw std::runtime_error(
          "This WORLD exceeds the scene's static mesh limit.");
    triangles += chunk_triangles;
    PackedVector3Array positions;
    if (build_display)
      positions.resize(int64_t(chunk_triangles * 3));
    int64_t at = 0;
    int64_t first_batch = -1, chunk_batches = 0;
    for (std::size_t b = 0; b < packet.batches.size(); ++b) {
      const auto &batch = packet.batches[b];
      if (batch.triangles.empty())
        continue;
      if (first_batch < 0)
        first_batch = int64_t(b);
      ++chunk_batches;
      const auto &vertices = chunk.world_vertices.at(b);
      for (const auto &triangle : batch.triangles)
        for (const auto index : triangle) {
          const auto &p = vertices.at(index);
          for (const auto coordinate : p)
            if (!std::isfinite(coordinate) ||
                std::abs(coordinate) > std::numeric_limits<float>::max())
              throw std::runtime_error(
                  "This WORLD exceeds the display coordinate range.");
          if (build_display)
            positions.set(at, Vector3(p[0], p[1], p[2]));
          ++at;
        }
    }
    batches += std::size_t(chunk_batches);
    Dictionary surface;
    // A streamed display needs the same validation and summary counts, but
    // never publishes this neutral mesh. Keep the authority in the Scene.
    if (build_display)
      surface["positions"] = positions;
    surface["chunk"] = int64_t(chunk.index);
    surface["batch"] = first_batch;
    surface["batches"] = chunk_batches;
    surfaces.push_back(surface);
  }
  if (surfaces.is_empty())
    throw std::runtime_error(
        "This WORLD has no visible supported static ground.");
  Dictionary out;
  out["ok"] = true;
  out["mode"] = "static_world";
  out["surfaces"] = surfaces;
  out["triangles"] = int64_t(triangles);
  out["batches"] = int64_t(batches);
  out["omitted_models"] = int64_t(geometry.omitted_models);
  out["omitted_lights"] = int64_t(geometry.omitted_lights);
  out["omitted_emitters"] = int64_t(geometry.omitted_emitters);
  out["note"] = String::utf8("Static WORLD ground · neutral display · models, "
                             "lights and effects omitted");
  return out;
}
Dictionary
ChampionsSessionController::prepare_terrain_file(const String &path,
                                                 bool build_display) {
  try {
    if (next_candidate_ == std::numeric_limits<int64_t>::max())
      throw std::runtime_error(
          "Restart the scene before loading more terrain.");
    const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null())
      throw std::runtime_error("The terrain file could not be opened.");
    const auto size = file->get_length();
    if (size > 64u * 1024u * 1024u)
      throw std::runtime_error("The terrain file exceeds 64 MiB.");
    const auto bytes = file->get_buffer(size);
    if (uint64_t(bytes.size()) != size)
      throw std::runtime_error("The terrain file changed while being read.");
    std::vector<uint8_t> input;
    if (bytes.size())
      input.assign(bytes.ptr(), bytes.ptr() + bytes.size());
    // Both projections consume the identical owned bytes. Geometry decoding
    // rejects water/moving pieces before any legacy illustration is considered.
    // The legacy authored collision-only fixture has exactly one all-zero
    // placeholder chunk. This is an explicitly labelled illustration path,
    // never a fallback after a real mesh fails decoding.
    auto word = [&](std::size_t at) -> uint32_t {
      if (at + 4 > input.size())
        return UINT32_MAX;
      return uint32_t(input[at]) | (uint32_t(input[at + 1]) << 8) |
             (uint32_t(input[at + 2]) << 16) | (uint32_t(input[at + 3]) << 24);
    };
    const bool illustration =
        input.size() >= 0xc0 && word(0) == 1 && word(0x24) == 0x84 &&
        word(0x50) == 0 &&
        std::all_of(input.begin() + 0x84, input.begin() + 0xc0,
                    [](uint8_t value) { return value == 0; });
    champions::presentation::rta::world_geometry::Geometry geometry;
    if (!illustration)
      geometry = champions::presentation::rta::world_geometry::decode(input);
    const auto data = rta::terrain_file::decode(input);
    rta::ScopedFpu fixed{rta::Fpu{rta::Fpu::Model::Ps2, false, false}};
    rta::world_queries::Level query(data);
    // With the level's objects.ob supplied for this exact path, the start is
    // level-spawn's cold story start (docs/LEVEL_FLOW.md section 17): marker
    // identity search, then player_ctor_z at radius 12. When a marker took,
    // that xy is the marker before Player1_ctor's findFreeSpot (pre_search)
    // and the original facing is unknown, so no facing is published.
    // Otherwise the historical origin start and its two floor probes are
    // unchanged.
    std::optional<rta::world_queries::Vec3> start;
    Dictionary start_info;
    if (!start_objects_path_.is_empty() && start_objects_path_ == path) {
      const auto records = rta::level_flow::parse_objects(
          {start_objects_.data(), start_objects_.size()});
      rta::level_flow::StoryGround story;
      if (start_previous_level_.empty() && start_entrance_.empty()) {
        story = rta::level_flow::story_start_ground({}, records, query);
      } else {
        rta::level_flow::StartEntry entry;
        entry.prev = start_previous_level_;
        entry.entrance = start_entrance_;
        rta::level_flow::Placement placement;
        placement.players = 1;
        story = rta::level_flow::entry_start_ground({}, records, entry, placement, query);
      }
      // A level the story snap cannot start is loaded in arena mode A only
      // when the owner's executable lists it in the mode-A table (0x5037b8,
      // research-levels 13fafa4 section 18.1: those archives are reached
      // through that table). That is the menu's mode-A load; the start is
      // arena_mode_a_start_ground, before the search and with no facing.
      bool arena_a = false;
      if (!story.ground &&
          rta::arena_modes::mode_a_index(arena_names_a_, start_level_name_) >= 0) {
        story = rta::level_flow::arena_mode_a_start_ground(records, query);
        arena_a = story.ground;
      }
      if (!story.ground)
        throw std::runtime_error(
            arena_names_a_.empty()
                ? "This WORLD has no ground at its story start (radius 12 snap). "
                  "A mode-A arena start needs the executable's arena tables."
                : "This WORLD has no ground at its story start (radius 12 snap) "
                  "and is not a mode-A arena level with an ArenaPlayerStart.");
      if (arena_a)
        start_info["arena_mode"] = "A";
      start = rta::world_queries::Vec3{story.position[0], story.position[1],
                                       story.object_z};
      start_info["marker_taken"] = story.marker_taken;
      start_info["rank"] = int64_t(story.rank);
      start_info["pre_search"] = story.pre_search;
      if (story.facing)
        start_info["facing"] = int64_t(*story.facing);
      start_info["records"] = int64_t(records.size());
    } else {
      const rta::world_queries::Vec3 origin{};
      if (rta::world_queries::terrain_height_at_point(query, origin) ==
              rta::world_queries::NO_FLOOR ||
          rta::world_queries::terrain_height_below(
              query, rta::session_control::RADIUS, origin) ==
              rta::world_queries::NO_FLOOR)
        throw std::runtime_error(
            "This WORLD has no supported ground at the current starting point.");
    }
    auto prepared = std::make_unique<PendingTerrain>();
    const Dictionary terrain =
        illustration ? present_terrain(data) : present_geometry(geometry, build_display);
    prepared->terrain = terrain;
    if (build_display || illustration)
      prepare_meshes(prepared->terrain);
    const auto current_config = active_setup().config;
    // Preserve the historical scene policy when recovery is off. An active
    // opt-in descriptor must survive terrain replacement, including its cap.
    auto next_config = (current_config.recovery || current_config.facing ||
                        current_config.play_window)
                           ? current_config
                           : scene_config();
    next_config.start = start;
    prepared->control =
        std::make_unique<rta::native_save::Scene>(next_config, data);
    prepared->player_service_level = std::make_unique<rta::world_queries::Level>(data);
    prepared->player_services = std::make_unique<rta::player_services::Bundle>(*prepared->player_service_level);
    if (!illustration) {
      prepared->player_water_image = std::make_unique<champions::real_world::WaterImage>(input, geometry);
      prepared->player_services->bind_water(prepared->player_water_image->service(),
                                           prepared->player_water_image->world());
    }
    prepared->name = path.get_file();
    const Dictionary initial_observation =
        present_observation(prepared->control->observation());
    prepared->observation = initial_observation;
    prepared->observation["terrain_name"] = prepared->name;
    prepared->observation["terrain_note"] = prepared->terrain["note"];
    prepared->observation["terrain_mode"] = prepared->terrain["mode"];
    if (start) {
      start_info["position"] = Vector3(display_float((*start)[0]),
                                       display_float((*start)[1]),
                                       display_float((*start)[2]));
      prepared->terrain["start"] = start_info;
    }
    prepared->token = next_candidate_;
    Dictionary result;
    result["ok"] = true;
    result["token"] = prepared->token;
    result["terrain"] = prepared->terrain.duplicate(true);
    pending_.swap(prepared);
    ++next_candidate_;
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_terrain_start_objects(
    const String &path, const PackedByteArray &objects, const String &level_name,
    const String &previous, const String &entrance) {
  Dictionary result;
  if (objects.size() > 16 * 1024 * 1024) {
    result["ok"] = false;
    result["error"] = "The objects table exceeds 16 MiB.";
    return result;
  }
  start_objects_path_ = objects.is_empty() ? String() : path;
  start_objects_.clear();
  if (!objects.is_empty())
    start_objects_.assign(objects.ptr(), objects.ptr() + objects.size());
  start_level_name_ = objects.is_empty() ? std::string() : std::string(level_name.utf8().get_data());
  start_previous_level_ = objects.is_empty() ? std::string() : std::string(previous.utf8().get_data());
  start_entrance_ = objects.is_empty() ? std::string() : std::string(entrance.utf8().get_data());
  result["ok"] = true;
  return result;
}
Dictionary ChampionsSessionController::load_arena_tables(const String &executable_path) {
  try {
    arena_names_a_.clear();
    executable_.reset();
    const Ref<FileAccess> file = FileAccess::open(executable_path, FileAccess::READ);
    if (file.is_null())
      throw std::runtime_error("The executable could not be opened.");
    const auto size = file->get_length();
    if (size > 64u * 1024u * 1024u)
      throw std::runtime_error("The executable exceeds 64 MiB.");
    const auto bytes = file->get_buffer(size);
    std::vector<uint8_t> input(bytes.ptr(), bytes.ptr() + bytes.size());
    auto exe = std::make_shared<rta::arena_modes::Executable>();
    if (!exe->load(std::move(input)))
      throw std::runtime_error("The executable is not an ELF32 image.");
    auto names = rta::arena_modes::read_names(*exe);
    auto player_tables = rta::player_state::read_tables(*exe);
    arena_names_a_ = std::move(names.names_a);
    // Kept for level scripts' object factory (docs/SCRIPT_COMMANDS.md); the
    // bytes stay with the owner and nothing read is copied into the tree.
    executable_ = std::move(exe);
    player_tables_ = std::move(player_tables);
    Dictionary result;
    result["ok"] = true;
    result["names_a"] = int64_t(arena_names_a_.size());
    result["names_b"] = int64_t(names.names_b.size());
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::commit_terrain_file(int64_t token) {
  try {
    if (!pending_ || pending_->token != token)
      throw std::runtime_error("The prepared terrain is no longer available.");
    if (pending_->combat || pending_->level) {
      // Validated at preparation (journal replay, witness, presentation block).
      unpair_script_seed();
      reset_fight();
      combat_ = std::move(pending_->combat);
      level_ = std::move(pending_->level);
      control_.reset();
      player_services_.reset();
      player_water_image_.reset();
      player_service_level_.reset();
      combat_setup_ = pending_->setup;
      terrain_ = pending_->terrain;
      std::swap(terrain_name_, pending_->name);
      loaded_presentation_ = std::move(pending_->presentation);
      pending_.reset();
      apply_loaded_presentation();
      attach_effects();
      cache_state(active_observation(), true);
      clock_.reset();
      Dictionary result = observation();
      result["presentation_restored"] = loaded_presentation_.has_value() && fx_bridge_.is_valid();
      return result;
    }
    // All potentially failing preparation, including the returned owned values,
    // completes before publication. The remaining swaps do not run simulation.
    Dictionary result = pending_->observation.duplicate(true);
    auto restored = pending_->control->observation();
    const Vector3 position(display_float(restored.position[0]),
                           display_float(restored.position[1]),
                           display_float(restored.position[2]));
    const double facing =
        double(restored.facing & 0xffffu) * (6.283185307179586 / 65536.0);
    unpair_script_seed_if_paired();
    // The fight borrows the player bundle. Reloading the terrain the fight was
    // built on (a save of the same level) keeps it and its bundle; any other
    // terrain replaces the bundle and so ends the fight.
    const bool keep_fight = fight_ && pending_->name == terrain_name_;
    if (!keep_fight) reset_fight();
    control_.swap(pending_->control);
    if (!keep_fight) {
      player_services_ = std::move(pending_->player_services);
      player_water_image_ = std::move(pending_->player_water_image);
      player_service_level_ = std::move(pending_->player_service_level);
    }
    combat_.reset();
    level_.reset();
    // In pinned godot-cpp 4.5, Dictionary move-assignment reconstructs its
    // storage without releasing the previous value. Do not std::swap it.
    // Lvalue copy-assignment releases the old owner and retains the prepared
    // dictionary without allocating or constructing any mesh after publication.
    terrain_ = pending_->terrain;
    std::swap(terrain_name_, pending_->name);
    pending_.reset();
    // No observation allocation or numeric validation after publication.
    // A loaded state starts a fresh interpolation pair with no elapsed debt.
    current_ = std::move(restored);
    previous_position_ = current_position_ = position;
    previous_facing_ = current_facing_ = facing;
    clock_.reset();
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
bool ChampionsSessionController::cancel_terrain_file(int64_t token) {
  if (!pending_ || pending_->token != token)
    return false;
  pending_.reset();
  return true;
}
Dictionary ChampionsSessionController::load_terrain_file(const String &path) {
  const auto prepared = prepare_terrain_file(path);
  if (!bool(prepared.get("ok", false)))
    return prepared;
  return commit_terrain_file(int64_t(prepared["token"]));
}
void ChampionsSessionController::set_player_camera(
    int64_t slot, int64_t yaw, const Variant &flag, const Vector3 &position,
    const PackedVector3Array &side_normals) {
  if (!player_services_) return; // no real level is resident yet
  if (slot < 0 || slot > 3 || side_normals.size() != 4)
    throw std::runtime_error("Player camera needs one slot and four side normals.");
  auto bits = [](double value) -> rta::Raw {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
      throw std::runtime_error("Player camera has a nonfinite component.");
    const float narrowed = static_cast<float>(value);
    rta::Raw raw;
    std::memcpy(&raw, &narrowed, sizeof(raw));
    return raw;
  };
  auto vector = [&](const Vector3 &v) -> rta::Vec3 {
    return {bits(v.x), bits(v.y), bits(v.z)};
  };
  rta::player_services::Camera snapshot;
  snapshot.yaw = rta::Raw(yaw & 0xffff);
  if (flag.get_type() == Variant::BOOL)
    snapshot.flag = bool(flag) ? 1 : 0;
  else if (flag.get_type() != Variant::NIL)
    throw std::runtime_error("Player camera mode needs a boolean or null.");
  snapshot.position = vector(position);
  for (int i = 0; i < 4; ++i)
    snapshot.side_normals[unsigned(i)] = vector(side_normals[i]);
  player_services_->set_local_slot(rta::Raw(slot));
  player_services_->set_camera(rta::Raw(slot), std::move(snapshot));
}

Dictionary ChampionsSessionController::save_file(const String &path) {
  return write_save_file(path, false);
}
Dictionary ChampionsSessionController::save_snapshot_file(const String &path) {
  return write_save_file(path, true);
}
Dictionary ChampionsSessionController::write_save_file(const String &path,
                                                       bool direct) {
  const String temporary = path + String(".ocnsave.tmp");
  bool owns_temporary = false;
  try {
    rta::native_save::Bytes bytes;
    if (level_) {
      bytes = level_->save();
    } else if (combat_) {
      // Fixture continuation v1; with a drawing bridge the exact per-machine
      // effect state travels in its presentation block.
      std::optional<rta::fixture_save::Bytes> presentation;
      auto *bridge = Object::cast_to<ChampionsFxBridge>(fx_bridge_.ptr());
      if (effects() && effects()->world() && bridge && bridge->world() == effects()->world())
        presentation = bridge->state_bytes();
      bytes = combat_->save(presentation ? &*presentation : nullptr);
    } else {
      (void)direct;
      bytes = current_.play_window ? control_->save_continuation()
                                   : (direct ? control_->save_indexed_snapshot() : control_->save());
    }
    if (FileAccess::file_exists(temporary))
      throw std::runtime_error(
          "A temporary save already exists; choose another file.");
    Ref<FileAccess> file = FileAccess::open(temporary, FileAccess::WRITE);
    if (file.is_null())
      throw std::runtime_error("The save file could not be created.");
    owns_temporary = true;
    PackedByteArray buffer;
    buffer.resize(bytes.size());
    std::copy(bytes.begin(), bytes.end(), buffer.ptrw());
    file->store_buffer(buffer);
    file->flush();
    const auto error = file->get_error();
    file->close();
    if (error != OK)
      throw std::runtime_error("The save file could not be written.");
    if (DirAccess::rename_absolute(temporary, path) != OK)
      throw std::runtime_error("The save file could not be replaced.");
    owns_temporary = false;
    Dictionary result;
    result["ok"] = true;
    return result;
  } catch (const std::exception &e) {
    if (owns_temporary)
      DirAccess::remove_absolute(temporary);
    return failure(e);
  }
}
Dictionary ChampionsSessionController::prepare_save_file(const String &path, bool reuse_loaded_level) {
  try {
    if (next_candidate_ == std::numeric_limits<int64_t>::max())
      throw std::runtime_error("Restart the scene before loading more saves.");
    const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null())
      throw std::runtime_error("The save file could not be opened.");
    const auto size = file->get_length();
    if (size > rta::native_save::MAX_BYTES)
      throw std::runtime_error("The save file exceeds 32 MiB.");
    {
      const PackedByteArray head = file->get_buffer(8);
      file->seek(0);
      if (rta::fixture_save::is_fixture_save(rta::fixture_save::Bytes(head.ptr(), head.ptr() + head.size())))
        return prepare_fixture_save(file);
    }
    class FileSource final : public rta::native_save::Source {
      Ref<FileAccess> file_;

    public:
      explicit FileSource(Ref<FileAccess> f) : file_(f) {}
      std::size_t size() const override { return file_->get_length(); }
      rta::native_save::Bytes read(std::size_t at,
                                   std::size_t length) const override {
        if (at > size() || length > size() - at)
          throw std::runtime_error("Save file range changed.");
        file_->seek(at);
        const auto bytes = file_->get_buffer(length);
        if (std::size_t(bytes.size()) != length)
          throw std::runtime_error("Save file changed while being read.");
        if (!length)
          return {};
        return {bytes.ptr(), bytes.ptr() + bytes.size()};
      }
    };
    auto prepared = std::make_unique<PendingTerrain>();
    prepared->control = rta::native_save::Scene::load_source(
        std::make_shared<FileSource>(file), "rta");
    const auto setup = prepared->control->setup();
    if (setup.config.movement !=
        rta::session_control::MovementPolicy::DryCreature)
      throw std::runtime_error("This scene requires a DryCreature save.");
    // The native continuation contains collision data, not the level's
    // rendering bytes. The real-data host may first reload and verify the
    // selected level's WORLD/TEX files, then reuse that prepared display.
    if (reuse_loaded_level) {
      if (!control_ || String(terrain_.get("mode", "")) != "static_world")
        throw std::runtime_error("Load and verify the saved level's own data first.");
      rta::native_save::Bytes saved_collision;
      rta::native_save::Bytes displayed_collision;
      rta::native_save::terrain_codec::encode_terrain(saved_collision,
                                                     setup.initial_terrain);
      rta::native_save::terrain_codec::encode_terrain(displayed_collision,
                                                     control_->setup().initial_terrain);
      if (saved_collision != displayed_collision)
        throw std::runtime_error("The displayed level's collision differs from the save.");
      prepared->terrain = terrain_;
      prepared->name = terrain_name_;
    } else {
      prepared->terrain = present_terrain(setup.initial_terrain.data);
      prepare_meshes(prepared->terrain);
      prepared->name = "Restored collision ground";
    }
    const Dictionary restored =
        present_observation(prepared->control->observation());
    prepared->observation = restored;
    prepared->observation["save_history_pending"] =
        prepared->control->history_pending();
    prepared->observation["terrain_name"] = prepared->name;
    prepared->observation["terrain_note"] = prepared->terrain["note"];
    prepared->observation["terrain_mode"] = prepared->terrain["mode"];
    prepared->token = next_candidate_;
    Dictionary result;
    result["ok"] = true;
    result["token"] = prepared->token;
    result["terrain"] = prepared->terrain.duplicate(true);
    pending_.swap(prepared);
    ++next_candidate_;
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}

rta::session_control::Setup ChampionsSessionController::active_setup() const {
  return (combat_ || level_) ? combat_setup_ : control_->setup();
}
Dictionary ChampionsSessionController::combat_observation() const {
  auto out = present_observation(current_);
  out["session_mode"] = "play";
  out["combat_enabled"] = true;
  out["animated_enabled"] = combat_->animated();
  out["full_enabled"] = combat_->full();
  out["combat_policy"] = combat_policy(*combat_);
  out["footstep_hook"] = "typed-ready-event; ClipRole audio bridge";
  out["movement_policy"] = combat_->full() ? "stick: executed Player_move (Full composition)" : "unsupported";
  out["history_policy"] = "latest-tick";
  out["history_complete"] = false;
  out["combat_shared_seed"] = int64_t(combat_->shared_seed());
  Array actors;
  for (const auto &a : combat_->actors()) {
    Dictionary value;
    value["slot"] = a.slot;
    value["handle"] = int64_t(a.handle);
    String kind = "Player";
    if (a.slot == 3)
      kind = String::utf8(combat_->own_creature_class().c_str());
    else if (a.slot == 4)
      kind = "AntQueen";
    else if (a.slot == 5)
      kind = "Skeleton";
    else if (a.slot == 6)
      kind = "Soul";
    else if (a.slot == 7)
      kind = "Horsemen";
    else if (a.slot == 8)
      kind = "Nightmare";
    else if (a.slot)
      kind = combat_->animated() ? "Agony animation / synthetic combat carrier" : "Skelight";
    value["kind"] = kind;
    if (combat_->full() && a.present) {
      const auto address = combat_->world().target(a.handle);
      value["facing"] = int64_t(combat_->world().read(address + 0x38, 2));
      value["action_state"] = int64_t(combat_->world().read(address + 0x58c));
      if (a.slot == 0) {
        // These are the executed Player_move mode-0 cells, not a position
        // delta inferred by presentation. Other play policies have no such
        // cells and must not advertise original player locomotion.
        value["locomotion_speed_raw"] = int64_t(combat_->world().read(address + 0x468));
        value["locomotion_blend_raw"] = int64_t(combat_->world().read(address + 0x46c));
        value["locomotion_walk_weight_raw"] = int64_t(combat_->world().read(address + 0x144));
        value["locomotion_run1_weight_raw"] = int64_t(combat_->world().read(address + 0x164));
        value["locomotion_run2_weight_raw"] = int64_t(combat_->world().read(address + 0x184));
        const auto slot = combat_->world().read(address + 0x3c8);
        if (slot < 4) {
          const auto record = 0x4d8af0u + static_cast<std::uint32_t>(slot) * 0x4c30u;
          value["player_model_index"] = int64_t(combat_->world().read(record + 0x54));
          value["weapon_animation_class"] = int64_t(static_cast<std::int8_t>(
              combat_->world().read(record + 0x4a86, 1)));
        }
      }
    }
    value["present"] = a.present;
    value["dead"] = a.dead;
    value["hp_raw"] = int64_t(a.hp);
    value["hp"] = display_float(a.hp);
    value["state"] = int64_t(a.state);
    value["position"] = Vector3(display_float(a.position[0]), display_float(a.position[1]), display_float(a.position[2]));
    if(combat_->animated()) {
      const auto &frame=combat_->animation_frame(a.slot);
      PackedVector3Array vertices;
      constexpr double d=rta::creature_anim::POSITION_DENOMINATOR;
      for(const auto &v:frame.vertices) vertices.push_back(Vector3(v[0]/d,v[2]/d,-v[1]/d));
      value["skinned_vertices"] = vertices;
      value["animation_tick"] = int64_t(frame.tick);
      value["animation_flags"] = int64_t(frame.flags);
      Array events;
      for(const auto &event:frame.events) {
        Dictionary item;
        item["kind"]=String::utf8(event.kind.c_str());
        item["consumer"]=String::utf8(event.consumer.c_str());
        item["mask"]=event.mask; item["word"]=int64_t(event.word);
        events.push_back(item);
      }
      value["animation_events"] = events;
    }
    actors.push_back(value);
  }
  out["combat_actors"] = actors;
  if (combat_->full()) {
    out["combat_sim_frame"] = int64_t(combat_->world().read(rta::combat_runtime::FRAME));
    const auto rec = rta::combat_runtime::PLAYERS;
    out["combat_spell_selected"] = combat_->world().read(rec + 0x4440 + 0x20, 2) == 4;
    out["combat_mana_raw"] = int64_t(combat_->world().read(rec + 0xc0));
    out["combat_max_mana_raw"] = int64_t(combat_->world().read(rec + 0xc4));
    out["combat_mana"] = display_float(rta::Raw(combat_->world().read(rec + 0xc0)));
    out["combat_max_mana"] = display_float(rta::Raw(combat_->world().read(rec + 0xc4)));
    out["effect_constants"] = fx_constants_source_;
    if (const auto *fx_sim = effects()) {
      const auto &c = fx_sim->counts();
      Dictionary fx;
      fx["ticks"] = int64_t(c.ticks);
      fx["particle_spawns"] = int64_t(c.particle_spawns);
      fx["fx_spawns"] = int64_t(c.fx_spawns);
      fx["decals"] = int64_t(c.decals);
      fx["glows"] = int64_t(c.glows);
      fx["class_runs"] = int64_t(c.class_runs);
      fx["game_damage"] = int64_t(c.game_damage);
      fx["game_queries"] = int64_t(c.game_queries);
      fx["drawing"] = fx_sim->world() != nullptr;
      Dictionary unrouted;
      for (const auto &entry : c.unrouted) unrouted[String::utf8(entry.first.c_str())] = int64_t(entry.second);
      fx["unrouted"] = unrouted;
      // Per-machine presentation refusals (review finding 5): particles of
      // definitions missing from the user's JSON, by address. Gameplay (seeds,
      // HP, ok) never depends on them, so they are reported, not failed.
      Dictionary missing;
      Array refusals;
      if (const auto *w = fx_sim->world())
        for (const auto &m : w->missing_by_definition) {
          missing[String("0x") + String::num_int64(m.first, 16)] = int64_t(m.second);
          refusals.push_back(String("particle definition 0x") + String::num_int64(m.first, 16) +
                             " missing from the user's JSON (" + String::num_int64(m.second) + " spawns)");
        }
      fx["missing_definitions"] = missing;
      fx["presentation_refusals"] = refusals;
      out["effects"] = fx;
    }
    Array names;
    for (const auto &e : combat_->tick_events()) names.push_back(String::utf8(e.name.c_str()));
    out["combat_tick_events"] = names;
  }
  return out;
}
Dictionary ChampionsSessionController::combat_hit_event(int64_t slot, int64_t mask) {
  try {
    if (!combat_ || slot < 0 || slot > 2 || mask < 0 || mask > 1)
      throw std::runtime_error("Combat fixture requires actor slot 0..2 and animation mask 0x1.");
    combat_->hit_event(unsigned(slot), rta::Raw(mask));
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}

// The Full scene draws into the attached bridge's world (journaled: the save
// records when the effect half had a world, see play_fixture_save.hpp).
void ChampionsSessionController::attach_effects() {
  if (!combat_ || !combat_->full()) return;
  auto *bridge = Object::cast_to<ChampionsFxBridge>(fx_bridge_.ptr());
  combat_->attach(bridge ? bridge->world() : nullptr);
}
Dictionary ChampionsSessionController::combat_attack(bool held) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Attack needs the Full combat fixture.");
    combat_->press_attack(held);
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_release_attack() {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Attack needs the Full combat fixture.");
    combat_->release_attack();
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_block(bool held) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Block needs the Full combat fixture.");
    combat_->set_block(held);
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_cast_holy_strike() {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Holy Strike needs the Full combat fixture.");
    combat_->cast_holy_strike();
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_select_disease_bolt(int64_t rank) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Disease Bolt selection needs the Full combat fixture.");
    if (rank < 1 || rank > 20) throw std::runtime_error("Disease Bolt rank needs a probed player rank 1 to 20.");
    combat_->select_disease_bolt(unsigned(rank));
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_press_disease_bolt(bool held) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Disease Bolt input needs the Full combat fixture.");
    combat_->press_disease_bolt(held);
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_release_disease_bolt() {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Disease Bolt input needs the Full combat fixture.");
    combat_->release_disease_bolt();
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_spawn_creature(int64_t own_class) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Own-machine creatures need the Full combat fixture.");
    if (own_class < 0 || own_class >= int64_t(rta::combat_play::OWN_CLASS_COUNT))
      throw std::runtime_error("Unknown own-machine class.");
    combat_->spawn_creature(unsigned(own_class));
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_spawn_mounted_horseman(int64_t variant) {
  try {
    if (!combat_ || !combat_->full())
      throw std::runtime_error("Mounted Horsemen need the Full combat fixture.");
    if (variant < 0 || variant > 3)
      throw std::runtime_error("Horsemen variant is outside 0 to 3.");
    combat_->spawn_mounted_horseman(unsigned(variant));
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::combat_drop_potion(int64_t kind) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Potions need the Full combat fixture.");
    if (kind < 1 || kind > 3) throw std::runtime_error("Potion kinds 1 to 3 only.");
    combat_->drop_potion(unsigned(kind));
    return observation();
  } catch (const std::exception &e) { return failure(e); }
}
PackedStringArray ChampionsSessionController::own_creature_classes() {
  PackedStringArray out;
  for (const auto *name : rta::combat_play::OWN_CLASSES) out.push_back(name);
  return out;
}
Dictionary ChampionsSessionController::set_effect_recipe_constants_json(const String &json, const String &source) {
  try {
    // Validated against the pinned recipe table before anything changes. A live
    // Full session keeps the constants it started with until reset/new play.
    auto next = std::make_shared<const champions::rta::fx::skills::RecipeConstants>(
        champions::rta::fx::skills::RecipeConstants::from_json(std::string(json.utf8().get_data())));
    fx_constants_ = std::move(next);
    fx_constants_identity_ = rta::fixture_save::identify(std::string(json.utf8().get_data()));
    fx_constants_source_ = source;
    Dictionary out;
    out["ok"] = true;
    out["source"] = source;
    return out;
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::attach_effect_bridge(Object *object) {
  try {
    auto *bridge = Object::cast_to<ChampionsFxBridge>(object);
    if (object && !bridge) throw std::runtime_error("attach_effect_bridge needs a ChampionsFxBridge or null.");
    fx_bridge_ = Ref<RefCounted>(bridge);
    apply_loaded_presentation();
    attach_effects();
    Dictionary out;
    out["ok"] = true;
    out["drawing"] = bridge != nullptr;
    return out;
  } catch (const std::exception &e) { return failure(e); }
}
Dictionary ChampionsSessionController::draw_effect_slots() {
  try {
    if (effects()) combat_->draw_slots();
    Dictionary out;
    out["ok"] = true;
    out["glows"] = int64_t(effects() ? effects()->counts().glows : 0);
    return out;
  } catch (const std::exception &e) { return failure(e); }
}

void ChampionsSessionController::apply_fight(rta::session_control::Observation &o) const {
  const auto p = fight_->player();
  if (!p.present) return;
  o.position = p.position;
  o.facing = p.facing & 0xffffu;
  o.facing_enabled = true;
  o.hp = p.hp;
  if (p.max_hp) o.max_health = p.max_hp;
}
Dictionary ChampionsSessionController::fight_observation() const {
  Dictionary out;
  if (!fight_) return out;
  out["campaign_fight"] = true;
  out["fight_started"] = fight_->started();
  out["fight_player_ready"] = fight_->player_ready();
  out["fight_failed"] = fight_->failed();
  out["fight_error"] = String::utf8(fight_->error().c_str());
  const auto &n = fight_->counts();
  Dictionary counts;
  counts["ticks"] = int64_t(n.ticks);
  counts["actors_built"] = int64_t(n.actors_built);
  counts["attack_edges"] = int64_t(n.attack_edges);
  counts["player_swings"] = int64_t(n.player_swings);
  counts["player_hits"] = int64_t(n.player_hits);
  counts["damage_events"] = int64_t(n.damage_events);
  counts["deaths"] = int64_t(n.deaths);
  counts["loot_calls"] = int64_t(n.loot_calls);
  counts["loot_items"] = int64_t(n.loot_items);
  counts["loot_shared_draws"] = int64_t(n.loot_shared_draws);
  counts["xp_awards"] = int64_t(n.xp_awards);
  counts["xp_gained"] = int64_t(n.xp_gained);
  counts["levels_gained"] = int64_t(n.levels_gained);
  counts["factory_runs_skipped"] = int64_t(n.factory_runs_skipped);
  out["fight_counts"] = counts;
  Dictionary refusals;
  for (const auto &entry : fight_->refusals()) refusals[String::utf8(entry.first.c_str())] = int64_t(entry.second);
  out["fight_refusals"] = refusals;
  Array enemies;
  int64_t handle = 0;
  for (const auto &e : fight_->enemies()) {
    Dictionary value;
    value["slot"] = int64_t(1 + handle);
    value["handle"] = int64_t(e.address);
    value["kind"] = e.small ? "WarBoarSmall" : "WarBoar";
    value["present"] = true;
    value["dead"] = e.dead;
    value["ai_frozen"] = e.ai_frozen;
    value["hp_raw"] = int64_t(e.hp);
    value["hp"] = display_float(e.hp);
    value["max_hp"] = display_float(e.max_hp);
    value["state"] = int64_t(e.state);
    value["facing"] = int64_t(e.facing);
    value["position"] = Vector3(display_float(e.position[0]), display_float(e.position[1]), display_float(e.position[2]));
    enemies.push_back(value);
    ++handle;
  }
  out["fight_enemies"] = enemies;
  const auto player = fight_->player();
  if (player.present) {
    out["fight_player_action_state"] = int64_t(player.action_state);
    out["fight_player_weapon_class"] = int64_t(player.weapon_class);
    // Host stand-in: the research does not document the route's starting
    // equipment, so the class-5 (unarmed) record is a labelled fallback.
    out["fight_starting_equipment"] = "refused_unknown";
    out["fight_player_weapon_class_source"] = "unarmed_fallback_standin";
    out["gold"] = int64_t(player.gold);
    // The experience record in the shared bytes (kill XP and level-up).
    out["fight_player_level"] = int64_t(player.level);
    out["fight_player_experience"] = int64_t(player.experience);
    out["fight_player_skill_points"] = int64_t(player.skill_points);
    out["fight_player_stat_points"] = int64_t(player.stat_points);
    out["fight_player_mana"] = display_float(player.mana);
    out["fight_player_max_mana"] = display_float(player.max_mana);
  }
  Array awards;
  for (const auto &a : fight_->awards()) {
    Dictionary value;
    value["slot"] = int64_t(a.slot);
    value["amount"] = int64_t(a.amount);
    value["levels_gained"] = int64_t(a.levels_gained);
    value["level_before"] = int64_t(a.level_before);
    value["level_after"] = int64_t(a.level_after);
    value["experience_before"] = int64_t(a.experience_before);
    value["experience_after"] = int64_t(a.experience_after);
    awards.push_back(value);
  }
  out["fight_awards"] = awards;
  // Item records the loot roll generated; no ground object exists for them.
  Array drops;
  for (const auto &g : fight_->ground_drops()) {
    Dictionary value;
    value["kind"] = String::utf8(g.kind.c_str());
    value["category"] = int64_t(g.category);
    value["count"] = int64_t(g.count);
    value["spawned"] = g.spawned;
    drops.push_back(value);
  }
  out["fight_ground_drops"] = drops;
  return out;
}
rta::session_control::Observation ChampionsSessionController::active_observation() const {
  if (combat_) return combat_->observation();
  if (level_) return level_->observation();
  auto o = control_->observation();
  // The shared-World player is what the scene presents (a stopped fight keeps
  // its last player state); the dry scene supplies only the clock fields.
  if (fight_ && fight_->player_ready()) apply_fight(o);
  return o;
}
rta::session_control::Observation ChampionsSessionController::active_advance(uint32_t mask) {
  const auto before=active_observation().ready_ticks;
  loaded_presentation_.reset();
  // The shared-World fight, when established, owns the player: the dry scene
  // only keeps the ready-tick clock, so it receives no movement or damage.
  const bool fighting = !combat_ && !level_ && fight_ && fight_->started() && fight_->player_ready() && !fight_->failed();
  // With a fight established the dry scene never receives the J/H diagnostic
  // damage bit (it is the player's attack button); a failed fight leaves the
  // stand-in with movement only.
  const uint32_t dry_mask = fight_ && fight_->player_ready() ? (fighting ? 0u : (mask & ~uint32_t(rta::session_control::DAMAGE))) : mask;
  auto next=combat_ ? combat_->advance(mask) : (level_ ? level_->advance(mask) : control_->advance(dry_mask));
  if (fighting && !next.failed && next.ready_ticks>before) fight_->tick(mask);
  if (!combat_ && !level_ && fight_ && fight_->player_ready()) apply_fight(next);
  if(!next.failed && next.ready_ticks>before) {
    if(combat_) {
      Dictionary visual; Array events;
      visual["tick"]=int64_t(next.ready_ticks);
      for(const auto &e:combat_->visual_events()) {
        Dictionary event;
        event["kind"]=e.death ? "death" : "hit";
        event["slot"]=int64_t(e.slot);
        event["position"]=Vector3(display_float(e.position[0]),display_float(e.position[1]),display_float(e.position[2]));
        events.push_back(event);
      }
      visual["events"]=events;
      emit_signal("combat_visual_tick",visual);
    }
    if(combat_ || level_ || next.play_window) {
      Dictionary packet; Array events;
      packet["tick"]=int64_t(next.ready_ticks);
      packet["level_key"]=level_ ? String::utf8(level_->flow().level.c_str())+":"+String::num_int64(level_->flow().generation) : String();
      if(combat_ && combat_->animated()) for(const auto &actor:combat_->actors()) {
        const auto &frame=combat_->animation_frame(actor.slot);
        // Dead/removed actors may retain their last pose: never replay its events.
        if(frame.tick!=next.ready_ticks) continue;
        for(const auto &event:frame.events) {
          Dictionary item;
          item["mask"]=event.mask;
          // This closed fixture selects attack clips for creatures and idle for
          // the player. Never infer ClipRole from the bitmask (0x4 is ambiguous).
          item["clip_role"]=actor.slot ? 0 : 2;
          item["slot"]=actor.slot;
          item["position"]=Vector3(display_float(actor.position[0]),display_float(actor.position[1]),display_float(actor.position[2]));
          events.push_back(item);
        }
      }
      packet["events"]=events;
      // Exactly one notification per successful ready tick, also in catch-up.
      // Presentation receives values only; it cannot modify World or its RNG.
      emit_signal("play_audio_tick",packet);
    }
  }
  return next;
}

Dictionary ChampionsSessionController::level_observation() const {
  auto out = present_observation(current_);
  auto f = level_->flow();
  out["session_mode"] = "play"; out["level_enabled"] = true;
  out["level_policy"] = rta::level_play::POLICY;
  out["movement_policy"] = "unsupported";
  out["history_policy"] = "per-level-fixture-state"; out["history_complete"] = false;
  out["level_name"] = String::utf8(f.level.c_str()); out["level_generation"] = f.generation;
  out["gate_travel_ticks"] = f.travel_ticks; out["gate_contact"] = f.gate_contact;
  out["gate_menu_open"] = f.menu_open; out["level_trigger_inside"] = f.trigger_inside;
  out["level_restored"] = f.restored; out["level_saved_count"] = int64_t(f.saved_levels);
  out["level_restored_bytes"] = int64_t(f.restored_bytes);
  out["gate_marker"] = Vector3(display_float(f.marker[0]), display_float(f.marker[1]), display_float(f.marker[2]));
  PackedInt64Array spawn; for(auto v:f.resolved_spawn) spawn.push_back(v);
  out["level_spawn_raw"] = spawn;
  return out;
}
Dictionary ChampionsSessionController::level_gate_open() {
  try { if(!level_) throw std::runtime_error("Level fixture not selected.");
    level_->open_gate(); snap(); return observation();
  } catch(const std::exception &e) {return failure(e);}
}
Dictionary ChampionsSessionController::level_gate_confirm(const String &name) {
  try { if(!level_) throw std::runtime_error("Level fixture not selected.");
    level_->confirm_gate(name.utf8().get_data()); snap(); return observation();
  } catch(const std::exception &e) {return failure(e);}
}
Dictionary ChampionsSessionController::level_request(const String &name) {
  try { if(!level_) throw std::runtime_error("Level fixture not selected.");
    level_->request_level(name.utf8().get_data()); return observation();
  } catch(const std::exception &e) {return failure(e);}
}

// A loaded Full continuation's presentation block goes to the attached bridge at
// commit, and again to a bridge attached before the next ready tick (the play
// scene rebuilds its FX node after a load).
void ChampionsSessionController::apply_loaded_presentation() {
  if (!loaded_presentation_ || !combat_ || !combat_->full()) return;
  if (auto *bridge = Object::cast_to<ChampionsFxBridge>(fx_bridge_.ptr())) bridge->restore_state(*loaded_presentation_);
}
Dictionary ChampionsSessionController::prepare_fixture_save(const Ref<FileAccess> &file) {
  const auto size = file->get_length();
  const PackedByteArray data = file->get_buffer(int64_t(size));
  if (std::uint64_t(data.size()) != size) throw std::runtime_error("Save file changed while being read.");
  const rta::fixture_save::Bytes bytes(data.ptr(), data.ptr() + data.size());
  auto prepared = std::make_unique<PendingTerrain>();
  prepared->setup = active_setup();
  if (bytes.size() > 24 && bytes[24] == 3) {
    prepared->level = rta::fixture_save::LevelScene::load(bytes, "rta");
    prepared->setup.initial_terrain = prepared->level->terrain();
    prepared->name = "Synthetic gate levels aa / aab";
  } else {
    const bool full = bytes.size() > 24 && bytes[24] == 2;
    if (full && !fx_constants_)
      throw std::runtime_error("Load the effect recipe constants this Full combat save was made with first.");
    auto loaded = full ? rta::fixture_save::CombatScene::load(bytes, "rta", fx_constants_, fx_constants_identity_)
                       : rta::fixture_save::CombatScene::load(bytes, "rta");
    if (loaded.presentation) ChampionsFxBridge::validate_state(*loaded.presentation);
    prepared->setup.initial_terrain = loaded.scene->terrain();
    prepared->combat = std::move(loaded.scene);
    prepared->presentation = std::move(loaded.presentation);
    prepared->name = "Restored combat fixture ground";
  }
  prepared->setup.supplied_terrain = true;
  prepared->terrain = present_terrain(prepared->setup.initial_terrain.data);
  prepare_meshes(prepared->terrain);
  prepared->token = next_candidate_;
  Dictionary result;
  result["ok"] = true;
  result["token"] = prepared->token;
  result["terrain"] = prepared->terrain.duplicate(true);
  result["format"] = rta::fixture_save::FORMAT;
  pending_.swap(prepared);
  ++next_candidate_;
  return result;
}
