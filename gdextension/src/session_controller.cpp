#include "session_controller.hpp"
#include "champions/terrain_file.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <godot_cpp/classes/array_mesh.hpp>
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
  std::unique_ptr<rta::session_control::Controller> control;
  Dictionary terrain, observation;
  String name;
  int64_t token = 0;
};

void ChampionsSessionController::_bind_methods() {
  ClassDB::bind_method(D_METHOD("advance", "command_mask"),
                       &ChampionsSessionController::advance);
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
    : control_(
          std::make_unique<rta::session_control::Controller>(scene_config())),
      terrain_(present_terrain(control_->terrain_data())) {
  prepare_meshes(terrain_);
}
ChampionsSessionController::~ChampionsSessionController() = default;

Dictionary ChampionsSessionController::observation() const {
  return present_observation(control_->observation());
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
  result["hp_raw"] = int64_t(o.hp);
  result["hp"] = display_float(o.hp);
  result["vz_raw"] = int64_t(o.vz);
  result["radius_raw"] = int64_t(o.radius);
  result["radius"] = display_float(o.radius);
  result["frame"] = o.frame;
  result["submissions"] = int64_t(o.submissions);
  result["ready_ticks"] = int64_t(o.ready_ticks);
  result["damage_events"] = int64_t(o.damage_events);
  result["paused"] = o.paused;
  result["stopped"] = o.stopped;
  result["failed"] = o.failed;
  result["terrain_name"] = terrain_name_;
  result["terrain_note"] = terrain_.get("note", "");
  result["terrain_mode"] = terrain_.get("mode", "practice");
  // Observe the active native configuration without a second full snapshot.
  result["movement_policy"] =
      o.movement == rta::session_control::MovementPolicy::DryCreature
          ? "dry_creature"
          : "isolated";
  result["ready_tick_limit"] = int64_t(o.ready_tick_limit);
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
    return present_observation(control_->advance(static_cast<uint32_t>(mask)));
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::reset_session() {
  try {
    pending_.reset();
    control_->reset();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::set_paused(bool value) {
  try {
    control_->set_paused(value);
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::release_input() {
  try {
    control_->release_input();
    return observation();
  } catch (const std::exception &e) {
    return failure(e);
  }
}
Dictionary ChampionsSessionController::stop_session() {
  try {
    control_->stop();
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
    prepared->control = std::make_unique<rta::session_control::Controller>(
        scene_config(), data);
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
    control_.swap(pending_->control);
    // In pinned godot-cpp 4.5, Dictionary move-assignment reconstructs its
    // storage without releasing the previous value. Do not std::swap it.
    // Lvalue copy-assignment releases the old owner and retains the prepared
    // dictionary without allocating or constructing any mesh after publication.
    terrain_ = pending_->terrain;
    std::swap(terrain_name_, pending_->name);
    pending_.reset();
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
