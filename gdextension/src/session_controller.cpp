#include "session_controller.hpp"
#include "champions/script_commands.hpp"
#include "champions/terrain_file.hpp"
#include "champions/level_flow.hpp"
#include "champions/arena_executable.hpp"
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
#include <limits>
#include <optional>
#include <stdexcept>
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
  ClassDB::bind_method(D_METHOD("combat_spawn_creature", "own_class"), &ChampionsSessionController::combat_spawn_creature);
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
  ClassDB::bind_method(D_METHOD("enable_level_script", "script", "objects", "carry_campaign", "level_name", "balance", "world"),
                       &ChampionsSessionController::enable_level_script, DEFVAL(false),
                       DEFVAL(String()), DEFVAL(PackedByteArray()), DEFVAL(PackedByteArray()));
  ClassDB::bind_method(D_METHOD("disable_level_script"),
                       &ChampionsSessionController::disable_level_script);
  ClassDB::bind_method(D_METHOD("script_command_counts"),
                       &ChampionsSessionController::script_command_counts);
  ClassDB::bind_method(D_METHOD("save_file", "path"),
                       &ChampionsSessionController::save_file);
  ClassDB::bind_method(D_METHOD("save_snapshot_file", "path"),
                       &ChampionsSessionController::save_snapshot_file);
  ClassDB::bind_method(D_METHOD("prepare_save_file", "path"),
                       &ChampionsSessionController::prepare_save_file);
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
  ClassDB::bind_method(D_METHOD("get_terrain"),
                       &ChampionsSessionController::get_terrain);
  ClassDB::bind_method(D_METHOD("load_terrain_file", "path"),
                       &ChampionsSessionController::load_terrain_file);
  ClassDB::bind_method(D_METHOD("prepare_terrain_file", "path"),
                       &ChampionsSessionController::prepare_terrain_file);
  ClassDB::bind_method(D_METHOD("set_terrain_start_objects", "path", "objects", "level_name"),
                       &ChampionsSessionController::set_terrain_start_objects, DEFVAL(String()));
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
  return result;
}
Dictionary ChampionsSessionController::get_observation() const {
  try {
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::advance(int64_t mask) {
  try {
    if (mask < 0 || mask > rta::session_control::COMMAND_MASK)
      throw std::runtime_error("Unknown control command.");
    const auto before = active_observation().ready_ticks;
    cache_state(active_advance(static_cast<uint32_t>(mask)), true);
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
        auto state = active_advance(static_cast<uint32_t>(mask));
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
void ChampionsSessionController::pump_scripts(std::size_t before) {
  if (!scripts_)
    return;
  // The control session's World makes no shared draw in its ticks
  // (session_control.cpp); if it ever does, the two streams have split.
  if (script_world_ && control_ && !combat_ && !level_ &&
      control_->shared_seed() != *script_world_)
    unpair_script_seed();
  for (auto tick = before; tick < current_.ready_ticks; ++tick)
    scripts_->on_ready_tick();
}
Dictionary ChampionsSessionController::enable_level_script(const PackedByteArray &script,
                                                          const PackedByteArray &objects,
                                                          bool carry_campaign,
                                                          const String &level_name,
                                                          const PackedByteArray &balance,
                                                          const PackedByteArray &world) {
  try {
    if (script.is_empty())
      throw std::runtime_error("Level script image is empty.");
    auto next = std::make_unique<rta::script_commands::Host>();
    rta::amx::ByteVector image(static_cast<std::size_t>(script.size()));
    std::memcpy(image.data(), script.ptr(), image.size());
    if (!next->load(image))
      throw std::runtime_error("Level script image was refused.");
    // A single-player game (docs/script-session.md section 2); a carried
    // campaign below brings its own values.
    next->start_single_player();
    const bool carried = carry_campaign && scripts_;
    if (carried)
      next->load_campaign(scripts_->save_campaign());
    // A fresh stream from the play session's configured shared seed; the
    // fixtures keep their own journaled seeds, so it is bound only while the
    // control session is the one that will play.
    auto seed = std::make_unique<rta::SharedRandom>(active_setup().config.shared_seed);
    if (!combat_ && !level_)
      next->bind_seed(seed.get(), rta::script_commands::SeedPolicy::OfflineSession);
    if (executable_)
      next->set_executable(rta::script_commands::executable_view(executable_));
    if (executable_ && !level_name.is_empty() && !balance.is_empty())
      next->set_level_stats(std::string(level_name.utf8().get_data()), balance.ptr(),
                            static_cast<std::size_t>(balance.size()));
    if (!world.is_empty())
      next->set_level_world(world.ptr(), static_cast<std::size_t>(world.size()));
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
    next->run_start();
    scripts_ = std::move(next);
    script_seed_ = (!combat_ && !level_) ? std::move(seed) : nullptr;
    script_world_.reset();
    script_seed_unpaired_ = false;
    Dictionary result;
    result["ok"] = true;
    result["campaign_carried"] = carried;
    result["executable"] = bool(executable_);
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::disable_level_script() {
  scripts_.reset();
  script_seed_.reset();
  script_world_.reset();
  Dictionary result;
  result["ok"] = true;
  return result;
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

  result["objects_built"] = static_cast<int64_t>(counts.objects_built);
  result["objects_unbuilt"] = static_cast<int64_t>(counts.objects_unbuilt);
  result["objects_not_created"] = static_cast<int64_t>(counts.objects_not_created);
  result["objects_pool_refused"] = static_cast<int64_t>(counts.objects_pool_refused);
  result["objects_unclassified"] = static_cast<int64_t>(counts.objects_unclassified);
  result["load_draws_unmodelled"] = static_cast<int64_t>(counts.load_draws_unmodelled);
  result["item_load_gate_refusals"] = static_cast<int64_t>(counts.item_load_gate_refusals);
  result["item_load_draws_inferred"] = static_cast<int64_t>(counts.item_load_draws_inferred);
  result["unbuilt_constructors"] = static_cast<int64_t>(counts.unbuilt_constructors.size());
  Dictionary build_refusals;
  for (const auto &kv : counts.build_refusals)
    build_refusals[String::utf8(kv.first.c_str())] = static_cast<int64_t>(kv.second);
  result["build_refusals"] = build_refusals;
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
      level_ = std::move(next); combat_.reset(); control_.reset(); pending_.reset();
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
      combat_setup_ = setup;
      combat_ = std::move(next);
      attach_effects();
      level_.reset();
      control_.reset(); // The combat World is the sole actor owner.
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
    // All allocations/validation precede publication. Terrain and visual owners
    // remain untouched. The remaining native moves and scalar stores cannot
    // fail.
    control_.swap(next);
    combat_.reset();
    level_.reset();
    pending_.reset();
    if (pair) {
      script_seed_->state = control_->shared_seed();
      script_world_ = script_seed_->state;
    }
    current_ = std::move(state);
    previous_position_ = current_position_ = position;
    previous_facing_ = current_facing_ = facing;
    clock_.reset();
    return result;
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::reset_session() {
  try {
    scripts_.reset();
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
    scripts_.reset();
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
  if (data.elements.size() != 1 || data.elements.front().flags != 0)
    throw std::runtime_error(
        "This scene currently displays one plain terrain surface at a time.");
  const auto &only = data.elements.front();
  for (const auto coordinate :
       {only.min_x, only.min_y, only.max_x, only.max_y, only.x, only.y, only.z})
    if (coordinate < -1048576 || coordinate > 1048576)
      throw std::runtime_error(
          "This terrain exceeds the scene display's coordinate range.");
  if (data.grid.empty() || data.grid.size() > 16384)
    throw std::runtime_error(
        "This terrain grid cannot be displayed in the scene yet.");
  for (const auto &cell : data.grid) {
    if (cell.empty() || std::any_of(cell.begin(), cell.end(), [](int64_t ref) {
          return ref != 0 && ref != -1;
        }))
      throw std::runtime_error("This terrain has partial surface coverage that "
                               "the scene cannot display yet.");
  }
  if (only.min_x < double(data.tg_x0) * 160 ||
      only.min_y < double(data.tg_y0) * 160 ||
      only.max_x >= (double(data.tg_x0) + data.tg_w) * 160 ||
      only.max_y >= (double(data.tg_y0) + data.tg_h) * 160)
    throw std::runtime_error(
        "This terrain surface extends beyond its query grid.");
  rta::ScopedFpu fixed{rta::Fpu{rta::Fpu::Model::Ps2, false, false}};
  rta::world_queries::Level query(data);
  Array surfaces;
  std::size_t tiles = 0;
  for (const auto &e : data.elements) {
    if (e.flags & 2)
      continue;
    if (e.flags != 0 || e.cos != 32767 || e.sin != 0)
      throw std::runtime_error(
          "This scene currently displays plain, unrotated terrain only.");
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
    const champions::presentation::rta::world_geometry::Geometry &geometry) {
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
          positions.set(at++, Vector3(p[0], p[1], p[2]));
        }
    }
    batches += std::size_t(chunk_batches);
    Dictionary surface;
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
ChampionsSessionController::prepare_terrain_file(const String &path) {
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
      auto story = rta::level_flow::story_start_ground({}, records, query);
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
        illustration ? present_terrain(data) : present_geometry(geometry);
    prepared->terrain = terrain;
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
    const String &path, const PackedByteArray &objects, const String &level_name) {
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
    arena_names_a_ = std::move(names.names_a);
    // Kept for level scripts' object factory (docs/SCRIPT_COMMANDS.md); the
    // bytes stay with the owner and nothing read is copied into the tree.
    executable_ = std::move(exe);
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
      combat_ = std::move(pending_->combat);
      level_ = std::move(pending_->level);
      control_.reset();
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
    control_.swap(pending_->control);
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
Dictionary ChampionsSessionController::prepare_save_file(const String &path) {
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
    // Saves contain collision state, not selected WORLD rendering bytes.
    // Restore an explicitly labelled illustration or refuse unsupported
    // display.
    const Dictionary terrain = present_terrain(setup.initial_terrain.data);
    prepared->terrain = terrain;
    prepare_meshes(prepared->terrain);
    prepared->name = "Restored collision ground";
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
    else if (a.slot)
      kind = combat_->animated() ? "Agony animation / synthetic combat carrier" : "Skelight";
    value["kind"] = kind;
    if (combat_->full() && a.present) {
      const auto address = combat_->world().target(a.handle);
      value["facing"] = int64_t(combat_->world().read(address + 0x38, 2));
      value["action_state"] = int64_t(combat_->world().read(address + 0x58c));
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
Dictionary ChampionsSessionController::combat_spawn_creature(int64_t own_class) {
  try {
    if (!combat_ || !combat_->full()) throw std::runtime_error("Own-machine creatures need the Full combat fixture.");
    if (own_class < 0 || own_class >= int64_t(rta::combat_play::OWN_CLASS_COUNT))
      throw std::runtime_error("Unknown own-machine class.");
    combat_->spawn_creature(unsigned(own_class));
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

rta::session_control::Observation ChampionsSessionController::active_observation() const {
  if (combat_) return combat_->observation();
  if (level_) return level_->observation();
  return control_->observation();
}
rta::session_control::Observation ChampionsSessionController::active_advance(uint32_t mask) {
  const auto before=active_observation().ready_ticks;
  loaded_presentation_.reset();
  auto next=combat_ ? combat_->advance(mask) : (level_ ? level_->advance(mask) : control_->advance(mask));
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
