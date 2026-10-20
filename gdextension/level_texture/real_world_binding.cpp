// Godot adapter for one WORLD's water tiles and moving pieces (native
// champions::real_world). Reads only the explicitly supplied WORLD/objects
// bytes; writes only an explicitly named session copy.
#include "champions/level_flow.hpp"
#include "champions/model_texture_bind.hpp"
#include "champions/real_world.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <cstring>
#include <memory>
#include <stdexcept>

using namespace godot;
namespace RW = champions::real_world;
namespace WG = champions::presentation::rta::world_geometry;
namespace LF = champions::simulation::rta::level_flow;

namespace {
std::vector<std::uint8_t> read_world(const String &path) {
  Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
  if (f.is_null())
    throw std::runtime_error("The WORLD file could not be opened.");
  const auto size = f->get_length();
  if (size > 64u * 1024u * 1024u)
    throw std::runtime_error("The WORLD file exceeds 64 MiB.");
  const auto b = f->get_buffer(size);
  if (std::uint64_t(b.size()) != size)
    throw std::runtime_error("The WORLD file changed while being read.");
  return size ? std::vector<std::uint8_t>(b.ptr(), b.ptr() + b.size())
              : std::vector<std::uint8_t>{};
}
float raw_float(std::uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
// Native Z-up (x, y, z) to Godot Y-up (x, z, -y).
Vector3 to_godot(double x, double y, double z) {
  return Vector3(real_t(x), real_t(z), real_t(-y));
}
RW::cw::Vec3 raw_position(const PackedInt64Array &p) {
  if (p.size() != 3)
    throw std::runtime_error("A raw position needs three words.");
  return {std::uint32_t(p[0]), std::uint32_t(p[1]), std::uint32_t(p[2])};
}
} // namespace

class ChampionsRealWorld : public RefCounted {
  GDCLASS(ChampionsRealWorld, RefCounted)
  std::vector<std::uint8_t> bytes_;
  std::unique_ptr<WG::Geometry> geometry_;
  std::unique_ptr<RW::WaterImage> water_;
  std::unique_ptr<RW::PieceScene> pieces_;

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("open", "world_path", "objects"),
                         &ChampionsRealWorld::open);
    ClassDB::bind_method(D_METHOD("water_test", "position_raw"),
                         &ChampionsRealWorld::water_test);
    ClassDB::bind_method(
        D_METHOD("water_disturb", "strength_raw", "radius_raw", "position_raw"),
        &ChampionsRealWorld::water_disturb);
    ClassDB::bind_method(D_METHOD("set_door", "piece", "open"),
                         &ChampionsRealWorld::set_door);
    ClassDB::bind_method(D_METHOD("piece_draws"),
                         &ChampionsRealWorld::piece_draws);
    ClassDB::bind_static_method("ChampionsRealWorld",
                                D_METHOD("model_texture", "vif", "tex"),
                                &ChampionsRealWorld::model_texture);
    ClassDB::bind_static_method("ChampionsRealWorld",
                                D_METHOD("companion_texture_name", "vif_name"),
                                &ChampionsRealWorld::companion_texture_name);
  }

public:
  Dictionary open(const String &world_path, const PackedByteArray &objects) {
    Dictionary out;
    out["ok"] = false;
    try {
      water_.reset();
      pieces_.reset();
      bytes_ = read_world(world_path);
      geometry_ = std::make_unique<WG::Geometry>(WG::decode(bytes_));
      Array water;
      for (const auto &w : geometry_->water) {
        const auto &c = geometry_->chunks.at(w.chunk);
        Dictionary tile;
        tile["chunk"] = std::int64_t(w.chunk);
        tile["min"] = to_godot(raw_float(c.bounds_bits[0]),
                               raw_float(c.bounds_bits[1]),
                               raw_float(c.bounds_bits[2]));
        tile["max"] = to_godot(raw_float(c.bounds_bits[3]),
                               raw_float(c.bounds_bits[4]),
                               raw_float(c.bounds_bits[5]));
        // Drawn at z = chunk translation z / 4 (level-flow section 12).
        tile["drawn_height"] = double(c.translation[2]) * 0.25;
        tile["test_height"] = double(raw_float(c.bounds_bits[2]));
        tile["second_set"] = w.second_set;
        water.push_back(tile);
      }
      out["water"] = water;
      if (!geometry_->water.empty())
        water_ = std::make_unique<RW::WaterImage>(bytes_, *geometry_);
      std::vector<LF::Record> records;
      if (objects.size())
        records = LF::parse_objects(
            {objects.ptr(), std::size_t(objects.size())});
      Array pieces;
      if (!geometry_->pieces.empty()) {
        pieces_ = std::make_unique<RW::PieceScene>(bytes_, *geometry_, records);
        for (std::size_t i = 0; i < pieces_->size(); ++i) {
          const auto &m = geometry_->pieces[i];
          Dictionary piece;
          PackedInt64Array chunks;
          for (std::int32_t k = 0; k < m.count; ++k)
            chunks.push_back(m.first_chunk + k);
          piece["chunks"] = chunks;
          piece["claimed"] = pieces_->claimer(i) >= 0;
          piece["collision"] = pieces_->collision(i);
          pieces.push_back(piece);
        }
        out["claimed"] = std::int64_t(pieces_->claimed());
        out["started_open"] = std::int64_t(pieces_->started_open());
      }
      out["pieces"] = pieces;
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
  Dictionary water_test(const PackedInt64Array &position) {
    Dictionary out;
    out["ok"] = false;
    try {
      out["in_water"] = false;
      if (water_) {
        // Entering water uses water_test(12) (level-flow section 12).
        const auto t = water_->test(0x41400000u, raw_position(position));
        out["in_water"] = t.in_water;
        out["height_raw"] = std::int64_t(t.height);
        out["height"] = double(raw_float(t.height));
      }
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
  int64_t water_disturb(int64_t strength, int64_t radius,
                        const PackedInt64Array &position) {
    if (!water_)
      return 0;
    try {
      return water_->disturb(std::uint32_t(strength), std::uint32_t(radius),
                             raw_position(position));
    } catch (const std::exception &) {
      return -1;
    }
  }
  bool set_door(int64_t piece, bool open) {
    if (!pieces_ || piece < 0)
      return false;
    try {
      pieces_->set_door(std::size_t(piece), open);
      return true;
    } catch (const std::exception &) {
      return false;
    }
  }
  Array piece_draws() const {
    Array out;
    if (!pieces_)
      return out;
    for (const auto &d : pieces_->draws()) {
      const auto &a = d.affine;
      // P A P^T with P: native (x, y, z) -> Godot (x, z, -y).
      // Columns: P A e_x, P A e_z, -P A e_y.
      const Basis basis(Vector3(real_t(a[0]), real_t(a[8]), real_t(-a[4])),
                        Vector3(real_t(a[2]), real_t(a[10]), real_t(-a[6])),
                        Vector3(real_t(-a[1]), real_t(-a[9]), real_t(a[5])));
      Dictionary draw;
      draw["piece"] = d.piece;
      draw["chunk"] = std::int64_t(d.chunk);
      draw["transform"] =
          Transform3D(basis, to_godot(a[3], a[7], a[11]));
      draw["collision"] = pieces_->collision(std::size_t(d.piece));
      out.push_back(draw);
    }
    return out;
  }
  static String companion_texture_name(const String &name) {
    return String::utf8(champions::model_texture::companion_texture_name(
                            name.utf8().get_data())
                            .c_str());
  }
  // Texture and per-corner UVs of one explicitly chosen model and its
  // same-base texture (docs/formats/model-texture.md).
  static Dictionary model_texture(const PackedByteArray &vif,
                                  const PackedByteArray &tex) {
    Dictionary out;
    out["ok"] = false;
    try {
      const auto bound = champions::model_texture::bind(
          {vif.ptr(), std::size_t(vif.size())},
          {tex.ptr(), std::size_t(tex.size())});
      out["width"] = int64_t(bound.texture.width);
      out["height"] = int64_t(bound.texture.height);
      PackedByteArray rgba;
      rgba.resize(std::int64_t(bound.texture.rgba.size()));
      if (!bound.texture.rgba.empty())
        std::memcpy(rgba.ptrw(), bound.texture.rgba.data(),
                    bound.texture.rgba.size());
      out["rgba"] = rgba;
      out["alpha1"] = int64_t(bound.alpha1);
      Array meshes;
      for (const auto &m : bound.meshes) {
        Dictionary mesh;
        mesh["index"] = int64_t(m.index);
        mesh["visible_by_default"] = m.visible_by_default;
        PackedVector2Array uv;
        uv.resize(std::int64_t(m.uv.size()));
        for (std::size_t i = 0; i < m.uv.size(); ++i)
          uv.set(std::int64_t(i), Vector2(real_t(m.uv[i][0]), real_t(m.uv[i][1])));
        mesh["uv"] = uv;
        using B = champions::model_texture::Blend;
        mesh["blend"] = m.blend == B::On ? 1 : m.blend == B::Off ? 0 : -1;
        meshes.push_back(mesh);
      }
      out["meshes"] = meshes;
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
};

void register_real_world() { GDREGISTER_CLASS(ChampionsRealWorld); }
