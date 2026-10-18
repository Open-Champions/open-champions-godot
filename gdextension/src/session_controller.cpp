#include "session_controller.hpp"
#include "champions/terrain_file.hpp"
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
  Dictionary terrain, observation;
  String name;
  int64_t token = 0;
};

void ChampionsSessionController::_bind_methods() {
  ClassDB::bind_method(D_METHOD("start_play_session"),
                       &ChampionsSessionController::start_play_session);
  ClassDB::bind_method(D_METHOD("get_play_capabilities"),
                       &ChampionsSessionController::get_play_capabilities);
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
  ClassDB::bind_method(D_METHOD("commit_terrain_file", "token"),
                       &ChampionsSessionController::commit_terrain_file);
  ClassDB::bind_method(D_METHOD("cancel_terrain_file", "token"),
                       &ChampionsSessionController::cancel_terrain_file);
}
ChampionsSessionController::ChampionsSessionController()
    : control_(std::make_unique<rta::native_save::Scene>(scene_config())),
      terrain_(present_terrain(control_->setup().initial_terrain.data)) {
  prepare_meshes(terrain_);
  cache_state(control_->observation(), true);
}
ChampionsSessionController::~ChampionsSessionController() = default;

Dictionary ChampionsSessionController::observation() const {
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
    cache_state(control_->advance(static_cast<uint32_t>(mask)), true);
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
void ChampionsSessionController::snap_presentation() {
  cache_state(control_->observation(), true);
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
        cache_state(control_->advance(static_cast<uint32_t>(mask)), false);
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
    auto setup = control_->setup();
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
    control_ = std::move(next);
    pending_.reset();
    snap_presentation();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_facing_test(bool enabled) {
  try {
    auto setup = control_->setup();
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
    control_ = std::move(next);
    pending_.reset();
    snap_presentation();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_animation_cache_test(bool enabled) {
  try {
    auto setup = control_->setup();
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
    control_ = std::move(next);
    pending_.reset();
    snap_presentation();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::animation_cache_command(int64_t kind,
                                                               int64_t clip) {
  try {
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
  result["history_window"] = int64_t(rta::play_session::DEFAULT_WINDOW);
  Array formats;
  formats.push_back("play-continuation-v1");
  result["save_formats"] = formats;
  Array unsupported;
  unsupported.push_back("recovery");
  unsupported.push_back("facing");
  unsupported.push_back("animation_cache");
  result["unsupported_optional_diagnostics"] = unsupported;
  return result;
}
Dictionary ChampionsSessionController::start_play_session() {
  try {
    auto setup = control_->setup();
    if (setup.config.recovery || setup.config.facing ||
        setup.config.animation_cache)
      throw std::runtime_error("Play sessions do not support recovery, facing "
                               "or animation-cache diagnostics.");
    setup.config.movement = rta::session_control::MovementPolicy::DryCreature;
    setup.config.max_ready_ticks =
        rta::session_control::MAX_CREATURE_READY_TICKS;
    setup.config.play_window = rta::play_session::DEFAULT_WINDOW;
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
    pending_.reset();
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
    pending_.reset();
    control_->reset();
    snap_presentation();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_paused(bool value) {
  try {
    control_->set_paused(value);
    snap_presentation();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::release_input() {
  try {
    control_->release_input();
    snap_presentation();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::stop_session() {
  try {
    control_->stop();
    snap_presentation();
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
Dictionary ChampionsSessionController::present_geometry(
    const champions::presentation::rta::world_geometry::Geometry &geometry) {
  Array surfaces;
  std::size_t triangles = 0;
  for (const auto &chunk : geometry.chunks) {
    if (!chunk.cell || !chunk.packet_index)
      continue;
    const auto &packet = geometry.packets.at(*chunk.packet_index);
    for (std::size_t b = 0; b < packet.batches.size(); ++b) {
      const auto &batch = packet.batches[b];
      if (batch.triangles.empty())
        continue;
      if (surfaces.size() >= 4096 ||
          batch.triangles.size() > 1000000 - triangles)
        throw std::runtime_error(
            "This WORLD exceeds the scene's static mesh limit.");
      triangles += batch.triangles.size();
      const auto &vertices = chunk.world_vertices.at(b);
      PackedVector3Array positions;
      for (const auto &triangle : batch.triangles)
        for (const auto index : triangle) {
          const auto &p = vertices.at(index);
          for (const auto coordinate : p)
            if (!std::isfinite(coordinate) ||
                std::abs(coordinate) > std::numeric_limits<float>::max())
              throw std::runtime_error(
                  "This WORLD exceeds the display coordinate range.");
          positions.push_back(Vector3(p[0], p[1], p[2]));
        }
      Dictionary surface;
      surface["positions"] = positions;
      surface["chunk"] = int64_t(chunk.index);
      surface["batch"] = int64_t(b);
      surfaces.push_back(surface);
    }
  }
  if (surfaces.is_empty())
    throw std::runtime_error(
        "This WORLD has no visible supported static ground.");
  Dictionary out;
  out["ok"] = true;
  out["mode"] = "static_world";
  out["surfaces"] = surfaces;
  out["triangles"] = int64_t(triangles);
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
    const rta::world_queries::Vec3 origin{};
    if (rta::world_queries::terrain_height_at_point(query, origin) ==
            rta::world_queries::NO_FLOOR ||
        rta::world_queries::terrain_height_below(
            query, rta::session_control::RADIUS, origin) ==
            rta::world_queries::NO_FLOOR)
      throw std::runtime_error(
          "This WORLD has no supported ground at the current starting point.");
    auto prepared = std::make_unique<PendingTerrain>();
    const Dictionary terrain =
        illustration ? present_terrain(data) : present_geometry(geometry);
    prepared->terrain = terrain;
    prepare_meshes(prepared->terrain);
    const auto current_config = control_->setup().config;
    // Preserve the historical scene policy when recovery is off. An active
    // opt-in descriptor must survive terrain replacement, including its cap.
    prepared->control = std::make_unique<rta::native_save::Scene>(
        (current_config.recovery || current_config.facing ||
         current_config.play_window)
            ? current_config
            : scene_config(),
        data);
    prepared->name = path.get_file();
    const Dictionary initial_observation =
        present_observation(prepared->control->observation());
    prepared->observation = initial_observation;
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
Dictionary ChampionsSessionController::commit_terrain_file(int64_t token) {
  try {
    if (!pending_ || pending_->token != token)
      throw std::runtime_error("The prepared terrain is no longer available.");
    // All potentially failing preparation, including the returned owned values,
    // completes before publication. The remaining swaps do not run simulation.
    Dictionary result = pending_->observation.duplicate(true);
    auto restored = pending_->control->observation();
    const Vector3 position(display_float(restored.position[0]),
                           display_float(restored.position[1]),
                           display_float(restored.position[2]));
    const double facing =
        double(restored.facing & 0xffffu) * (6.283185307179586 / 65536.0);
    control_.swap(pending_->control);
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
    const auto bytes =
        current_.play_window
            ? control_->save_continuation()
            : (direct ? control_->save_indexed_snapshot() : control_->save());
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
