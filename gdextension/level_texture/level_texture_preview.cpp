#include "champions/level_texture.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>

using namespace godot;
void register_real_world();
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
        surfaces.push_back(surface);
      }
      out["images"] = images;
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
};
void initialize(ModuleInitializationLevel level) {
  if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
  {
    GDREGISTER_CLASS(ChampionsLevelTexturePreview);
    register_real_world();
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
