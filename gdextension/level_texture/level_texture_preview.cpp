#include "champions/level_texture.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

using namespace godot;
void register_real_world();
void register_level_texture_stream();
// Automap quads of the WORLD header +0x80 grid; shared with the streamed
// display (level_texture_stream.cpp).
Dictionary level_automap_dictionary(const std::vector<std::uint8_t> &world_bytes) {
  Dictionary automap;
  automap["ok"] = false;
  try {
    const auto grid =
        champions::presentation::rta::level_texture::read_automap_grid(
            world_bytes);
    Array quads;
    if (grid.width >= 2 && grid.height >= 2) {
      for (int row = 0; row + 1 < grid.height; ++row) {
        for (int col = 0; col + 1 < grid.width; ++col) {
          const auto at = [&](int c, int r) {
            return grid.cells[static_cast<std::size_t>(r * grid.width + c)];
          };
          const auto quad =
              champions::presentation::rta::level_texture::automap_quad(
                  at(col, row), at(col, row + 1), at(col + 1, row),
                  at(col + 1, row + 1));
          PackedInt32Array uv, alpha;
          for (const auto &corner : quad) {
            uv.push_back(corner.u);
            uv.push_back(corner.v);
            alpha.push_back(corner.alpha);
          }
          Dictionary one;
          one["column"] = col;
          one["row"] = row;
          one["uv"] = uv;
          one["alpha"] = alpha;
          quads.push_back(one);
        }
      }
    }
    automap["ok"] = true;
    automap["origin_x"] = int(grid.origin_x);
    automap["origin_y"] = int(grid.origin_y);
    automap["width"] = int(grid.width);
    automap["height"] = int(grid.height);
    automap["cells"] = std::int64_t(grid.cells.size());
    automap["quads"] = quads;
  } catch (const std::exception &error) {
    automap["error"] = String::utf8(error.what());
  }
  return automap;
}
namespace {
// Display budgets. The largest RTA level measured (counts only, 2026-10-20)
// has 47,277 batches in 5,071 consecutive (chunk, material) runs, 1.83 M
// triangles, 5.48 M vertices and 369 MB of distinct RGBA texels.
constexpr std::size_t MAX_SURFACES = 16384;      // after merging runs
constexpr std::size_t MAX_TRIANGLES = 4000000;   // the WORLD decoder cap
constexpr std::size_t MAX_RGBA_BYTES = 1024u * 1024u * 1024u;
bool same_material(const champions::presentation::rta::level_texture::Surface &a,
                   const champions::presentation::rta::level_texture::Surface &b) {
  const auto &x = a.material, &y = b.material;
  return a.chunk == b.chunk && a.image == b.image && x.alpha1 == y.alpha1 &&
         x.test1 == y.test1 && x.blend_enabled == y.blend_enabled &&
         x.light_constant == y.light_constant &&
         x.dynamic_light == y.dynamic_light && x.sort_layer == y.sort_layer &&
         x.environment == y.environment;
}
std::vector<std::uint8_t> read_file(const String &path, std::uint64_t limit) {
  Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
  if (f.is_null())
    throw std::runtime_error(
        "The selected WORLD or TEX file could not be opened.");
  const auto size = f->get_length();
  if (size > limit)
    throw std::runtime_error(
        "The selected WORLD or TEX file exceeds the preview limit.");
  auto b = f->get_buffer(size);
  if (std::uint64_t(b.size()) != size)
    throw std::runtime_error("The selected file changed while being read.");
  if (!b.size())
    return {};
  return {b.ptr(), b.ptr() + b.size()};
}
class ChampionsLevelTexturePreview : public Object {
  GDCLASS(ChampionsLevelTexturePreview, Object)
protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("load_world_tex", "world_path", "tex_path"),
                         &ChampionsLevelTexturePreview::load_world_tex);
    ClassDB::bind_method(D_METHOD("environment_rows", "camera", "frame",
                                  "level_options", "quarter"),
                         &ChampionsLevelTexturePreview::environment_rows);
    ClassDB::bind_method(D_METHOD("world_constant_channel", "base", "added"),
                         &ChampionsLevelTexturePreview::world_constant_channel);
    ClassDB::bind_method(D_METHOD("environment_uv", "normal", "row4", "row5",
                                  "sx", "sy"),
                         &ChampionsLevelTexturePreview::environment_uv);
  }

public:
  Dictionary load_world_tex(const String &world_path,
                            const String &tex_path) const {
    Dictionary out;
    out["ok"] = false;
    try {
      auto world_bytes = read_file(world_path, 64u * 1024u * 1024u);
      auto tex_bytes = read_file(tex_path, 512u * 1024u * 1024u);
      const auto geometry =
          champions::presentation::rta::world_geometry::decode(world_bytes);
      const champions::presentation::rta::level_texture::File textures(
          std::move(tex_bytes));
      auto bound =
          champions::presentation::rta::level_texture::bind_world(geometry,
                                                                  textures, world_bytes);
      // Consecutive batches of one chunk with the same image and material
      // become one surface, in source order, so draw order is unchanged.
      std::vector<champions::presentation::rta::level_texture::Surface>
          native_surfaces;
      std::size_t batches = bound.size();
      for (auto &s : bound) {
        if (!native_surfaces.empty() && same_material(native_surfaces.back(), s)) {
          auto &into = native_surfaces.back();
          into.positions.insert(into.positions.end(), s.positions.begin(),
                                s.positions.end());
          into.uv.insert(into.uv.end(), s.uv.begin(), s.uv.end());
        } else
          native_surfaces.push_back(std::move(s));
      }
      bound.clear();
      if (native_surfaces.empty() || native_surfaces.size() > MAX_SURFACES)
        throw std::runtime_error(
            "The level has no bounded supported textured ground.");
      std::size_t rgba_bytes = 0, vertices = 0;
      std::map<const void *, std::int64_t> image_ids;
      Array images, surfaces;
      std::size_t triangles = 0;
      for (const auto &s : native_surfaces) {
        if (s.positions.size() != s.uv.size() || s.positions.size() % 3 ||
            s.positions.size() / 3 > MAX_TRIANGLES - triangles)
          throw std::runtime_error(
              "The level exceeds the preview triangle limit.");
        triangles += s.positions.size() / 3;
        vertices += s.positions.size();
        auto found = image_ids.find(s.image.get());
        if (found == image_ids.end()) {
          if (s.image->rgba.size() > MAX_RGBA_BYTES - rgba_bytes)
            throw std::runtime_error(
                "The level exceeds the preview texture memory limit.");
          rgba_bytes += s.image->rgba.size();
          Dictionary image;
          image["width"] = int(s.image->width);
          image["height"] = int(s.image->height);
          PackedByteArray rgba;
          if (rgba.resize(std::int64_t(s.image->rgba.size())) != OK)
            throw std::bad_alloc();
          if (!s.image->rgba.empty())
            std::memcpy(rgba.ptrw(), s.image->rgba.data(), s.image->rgba.size());
          image["rgba"] = rgba;
          const std::int64_t id = images.size();
          images.push_back(image);
          found = image_ids.emplace(s.image.get(), id).first;
        }
        PackedVector3Array positions;
        PackedVector2Array uv;
        if (positions.resize(std::int64_t(s.positions.size())) != OK ||
            uv.resize(std::int64_t(s.uv.size())) != OK)
          throw std::bad_alloc();
        Vector3 *pw = positions.ptrw();
        Vector2 *uw = uv.ptrw();
        for (std::size_t i = 0; i < s.positions.size(); ++i) {
          const auto &p = s.positions[i];
          const auto &t = s.uv[i];
          pw[i] = Vector3(p[0], p[1], p[2]);
          uw[i] = Vector2(t[0], t[1]);
        }
        Dictionary surface;
        surface["positions"] = positions;
        surface["uv"] = uv;
        surface["image"] = found->second;
        surface["cell"] = int(s.cell);
        surface["chunk"] = std::int64_t(s.chunk);
        surface["batch"] = std::int64_t(s.batch);
        surface["alpha1"] = int(s.material.alpha1);
        surface["test1"] = std::int64_t(s.material.test1);
        surface["blend_enabled"] = s.material.blend_enabled;
        surface["environment"] = s.material.environment;
        surface["sort_layer"] = int(s.material.sort_layer);
        surface["dynamic_light"] = s.material.dynamic_light;
        surface["light_constant"] = int(s.material.light_constant);
        surface["alpha2_set"] = s.material.alpha2_set;
        surface["alpha2"] = int(s.material.alpha2);
        surface["flags_after"] = std::int64_t(s.material.flags_after);
        surface["draw_order"] = s.draw_order;
        surface["quarter"] = s.quarter ? int(*s.quarter) : -1;
        PackedVector3Array normals;
        for (const auto &normal : s.stored_normals)
          normals.push_back(Vector3(normal[0], normal[1], normal[2]));
        surface["stored_normals"] = normals;
        surfaces.push_back(surface);
      }
      const Dictionary automap = level_automap_dictionary(world_bytes);
      out["images"] = images;
      out["automap"] = automap;
      out["surfaces"] = surfaces;
      out["triangles"] = std::int64_t(triangles);
      out["batches"] = std::int64_t(batches);
      out["vertices"] = std::int64_t(vertices);
      out["rgba_bytes"] = std::int64_t(rgba_bytes);
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }

  Dictionary environment_rows(const PackedFloat64Array &camera, int64_t frame,
                              int64_t level_options, int64_t quarter) const {
    Dictionary out;
    out["ok"] = false;
    try {
      if (camera.size() < 9)
        throw std::runtime_error("environment rows need 9 camera components");
      std::array<double, 9> values{};
      for (int i = 0; i < 9; ++i)
        values[static_cast<std::size_t>(i)] = camera[i];
      const auto axis =
          champions::presentation::rta::level_texture::axis_xy(values);
      const std::optional<int> turn =
          quarter < 0 ? std::nullopt : std::optional<int>(static_cast<int>(quarter));
      const auto rows =
          champions::presentation::rta::level_texture::env_rows(
              axis, static_cast<std::uint32_t>(frame),
              static_cast<std::uint32_t>(level_options), turn);
      PackedFloat64Array row4, row5;
      for (double value : rows.first)
        row4.push_back(value);
      for (double value : rows.second)
        row5.push_back(value);
      out["row4"] = row4;
      out["row5"] = row5;
      out["ok"] = true;
    } catch (const std::exception &error) {
      out["error"] = String::utf8(error.what());
    }
    return out;
  }

  int64_t world_constant_channel(int64_t base, int64_t added) const {
    return champions::presentation::rta::level_texture::world_constant_channel(
        static_cast<int>(base), static_cast<int>(added));
  }

  Dictionary environment_uv(const PackedFloat64Array &normal,
                            const PackedFloat64Array &row4,
                            const PackedFloat64Array &row5, double sx,
                            double sy) const {
    Dictionary out;
    out["ok"] = false;
    try {
      if (normal.size() < 3 || row4.size() < 4 || row5.size() < 4)
        throw std::runtime_error("environment uv is missing a row or normal");
      std::array<double, 3> n{normal[0], normal[1], normal[2]};
      std::array<double, 4> a{row4[0], row4[1], row4[2], row4[3]};
      std::array<double, 4> b{row5[0], row5[1], row5[2], row5[3]};
      const auto uv =
          champions::presentation::rta::level_texture::env_uv(n, a, b, sx, sy);
      out["u"] = uv[0];
      out["v"] = uv[1];
      out["ok"] = true;
    } catch (const std::exception &error) {
      out["error"] = String::utf8(error.what());
    }
    return out;
  }
};
void initialize(ModuleInitializationLevel level) {
  if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
  {
    GDREGISTER_CLASS(ChampionsLevelTexturePreview);
    register_real_world();
    register_level_texture_stream();
  }
}
void terminate(ModuleInitializationLevel) {}
} // namespace
extern "C" GDExtensionBool GDE_EXPORT
champions_level_texture_init(GDExtensionInterfaceGetProcAddress address,
                             GDExtensionClassLibraryPtr library,
                             GDExtensionInitialization *initialization) {
  GDExtensionBinding::InitObject init(address, library, initialization);
  init.register_initializer(initialize);
  init.register_terminator(terminate);
  init.set_minimum_library_initialization_level(
      MODULE_INITIALIZATION_LEVEL_SCENE);
  return init.init();
}
