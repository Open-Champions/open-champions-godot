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
#include <map>
#include <memory>
#include <stdexcept>

using namespace godot;
namespace {
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
      const auto native_surfaces =
          champions::presentation::rta::level_texture::bind_world(geometry,
                                                                  textures);
      if (native_surfaces.empty() || native_surfaces.size() > 4096)
        throw std::runtime_error(
            "The level has no bounded supported textured ground.");
      std::map<const void *, std::int64_t> image_ids;
      Array images, surfaces;
      std::size_t triangles = 0;
      for (const auto &s : native_surfaces) {
        if (s.positions.size() != s.uv.size() || s.positions.size() % 3 ||
            s.positions.size() / 3 > 1000000 - triangles)
          throw std::runtime_error(
              "The level exceeds the preview triangle limit.");
        triangles += s.positions.size() / 3;
        auto found = image_ids.find(s.image.get());
        if (found == image_ids.end()) {
          Dictionary image;
          image["width"] = int(s.image->width);
          image["height"] = int(s.image->height);
          PackedByteArray rgba;
          if (rgba.resize(std::int64_t(s.image->rgba.size())) != OK)
            throw std::bad_alloc();
          for (std::size_t i = 0; i < s.image->rgba.size(); ++i)
            rgba.set(std::int64_t(i), s.image->rgba[i]);
          image["rgba"] = rgba;
          const std::int64_t id = images.size();
          images.push_back(image);
          found = image_ids.emplace(s.image.get(), id).first;
        }
        PackedVector3Array positions;
        PackedVector2Array uv;
        for (std::size_t i = 0; i < s.positions.size(); ++i) {
          const auto &p = s.positions[i];
          const auto &t = s.uv[i];
          positions.push_back(Vector3(p[0], p[1], p[2]));
          uv.push_back(Vector2(t[0], t[1]));
        }
        Dictionary surface;
        surface["positions"] = positions;
        surface["uv"] = uv;
        surface["image"] = found->second;
        surface["cell"] = int(s.cell);
        surface["chunk"] = std::int64_t(s.chunk);
        surface["batch"] = std::int64_t(s.batch);
        surfaces.push_back(surface);
      }
      out["images"] = images;
      out["surfaces"] = surfaces;
      out["triangles"] = std::int64_t(triangles);
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
};
void initialize(ModuleInitializationLevel level) {
  if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
    GDREGISTER_CLASS(ChampionsLevelTexturePreview);
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
