// Godot adapter for one WORLD's water tiles and moving pieces (native
// champions::real_world). Reads only the explicitly supplied WORLD/objects
// bytes; writes only an explicitly named session copy.
#include "champions/level_flow.hpp"
#include "champions/model_texture_bind.hpp"
#include "champions/real_world.hpp"
#include "champions/texture.hpp"
#include "champions/water_draw.hpp"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
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
namespace WD = champions::presentation::rta::water_draw;

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
Ref<ArrayMesh> water_mesh(const std::array<WD::DrawSurface, 2> &surfaces,
                         PackedFloat32Array &fixes, Array &described) {
  Ref<ArrayMesh> mesh;
  mesh.instantiate();
  for (int set = 0; set < 2; ++set) {
    const auto &surface = surfaces[static_cast<std::size_t>(set)];
    if (surface.positions.empty())
      continue;
    PackedVector3Array positions;
    PackedVector2Array uvs;
    PackedColorArray colors;
    PackedInt32Array indices;
    positions.resize(static_cast<std::int64_t>(surface.positions.size()));
    uvs.resize(static_cast<std::int64_t>(surface.uv_texels.size()));
    colors.resize(static_cast<std::int64_t>(surface.positions.size()));
    indices.resize(static_cast<std::int64_t>(surface.indices.size()));
    const auto channels = WD::colour_channels(surface.colour);
    const Color tint(channels[0] / 128.f, channels[1] / 128.f,
                     channels[2] / 128.f, 1.f);
    for (std::size_t i = 0; i < surface.positions.size(); ++i) {
      const auto &p = surface.positions[i];
      positions.set(static_cast<std::int64_t>(i), to_godot(p[0], p[1], p[2]));
      const auto &uv = surface.uv_texels[i];
      uvs.set(static_cast<std::int64_t>(i), Vector2(uv[0], uv[1]));
      colors.set(static_cast<std::int64_t>(i), tint);
    }
    for (std::size_t i = 0; i < surface.indices.size(); ++i)
      indices.set(static_cast<std::int64_t>(i),
                  static_cast<std::int32_t>(surface.indices[i]));
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = positions;
    arrays[Mesh::ARRAY_TEX_UV] = uvs;
    arrays[Mesh::ARRAY_COLOR] = colors;
    arrays[Mesh::ARRAY_INDEX] = indices;
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    fixes.push_back(surface.fix);
    Dictionary info;
    info["fix"] = surface.fix;
    info["colour"] = std::int64_t(surface.colour);
    info["layer"] = surface.layer;
    info["triangles"] = surface.triangles;
    info["second_set"] = set == 1;
    info["additive"] = surface.fix < 0.f;
    // The same colour, unclamped, for a shader uniform: a vertex colour
    // channel saturates at 1.0 and the stored colour can reach 255/128.
    info["tint"] = Vector3(channels[0] / 128.f, channels[1] / 128.f,
                           channels[2] / 128.f);
    described.push_back(info);
  }
  return mesh;
}
RW::cw::Vec3 raw_position(const PackedInt64Array &p) {
  if (p.size() != 3)
    throw std::runtime_error("A raw position needs three words.");
  return {std::uint32_t(p[0]), std::uint32_t(p[1]), std::uint32_t(p[2])};
}
// world_objectFactory matches customVars as a case-insensitive full class name
// and that constructor is levelSettings_parse. Other classes are not that parser.
bool custom_vars(std::string_view cls) {
  constexpr char name[] = "customvars";
  if (cls.size() != sizeof(name) - 1)
    return false;
  for (std::size_t i = 0; i < cls.size(); ++i) {
    const auto c = cls[i];
    const auto lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    if (lower != name[i])
      return false;
  }
  return true;
}
} // namespace

class ChampionsRealWorld : public RefCounted {
  GDCLASS(ChampionsRealWorld, RefCounted)
  std::vector<std::uint8_t> bytes_;
  std::unique_ptr<WG::Geometry> geometry_;
  std::unique_ptr<RW::WaterImage> water_;
  std::unique_ptr<RW::PieceScene> pieces_;
  std::unique_ptr<WD::Field> waves_;
  // The player's wading on this water (player_wading.hpp). Declared after the
  // image it borrows, so it is destroyed first.
  std::unique_ptr<RW::Wader> wader_;
  std::uint32_t level_flags_ = 0;
  Ref<ArrayMesh> wave_mesh_;
  PackedFloat32Array wave_fixes_;
  Array wave_surfaces_;

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("open", "world_path", "objects"),
                         &ChampionsRealWorld::open);
    ClassDB::bind_method(D_METHOD("water_test", "position_raw"),
                         &ChampionsRealWorld::water_test);
    ClassDB::bind_method(
        D_METHOD("water_disturb", "strength_raw", "radius_raw", "position_raw"),
        &ChampionsRealWorld::water_disturb);
    ClassDB::bind_method(D_METHOD("wade_step", "frame", "position_raw", "moving"),
                         &ChampionsRealWorld::wade_step);
    ClassDB::bind_method(D_METHOD("set_level_flags", "flags"),
                         &ChampionsRealWorld::set_level_flags);
    ClassDB::bind_method(D_METHOD("set_door", "piece", "open"),
                         &ChampionsRealWorld::set_door);
    ClassDB::bind_method(D_METHOD("piece_draws"),
                         &ChampionsRealWorld::piece_draws);
    ClassDB::bind_method(D_METHOD("water_sync", "frame", "frozen"),
                         &ChampionsRealWorld::water_sync);
    ClassDB::bind_method(D_METHOD("water_texture", "bytes"),
                         &ChampionsRealWorld::water_texture);
    ClassDB::bind_static_method("ChampionsRealWorld",
                                D_METHOD("model_texture", "vif", "tex"),
                                &ChampionsRealWorld::model_texture);
    ClassDB::bind_static_method("ChampionsRealWorld",
                                D_METHOD("decode_texture", "bytes"),
                                &ChampionsRealWorld::decode_texture);
    ClassDB::bind_static_method("ChampionsRealWorld",
                                D_METHOD("companion_texture_name", "vif_name"),
                                &ChampionsRealWorld::companion_texture_name);
  }

public:
  Dictionary open(const String &world_path, const PackedByteArray &objects) {
    Dictionary out;
    out["ok"] = false;
    try {
      wader_.reset();
      level_flags_ = 0;
      water_.reset();
      pieces_.reset();
      waves_.reset();
      wave_mesh_.unref();
      wave_fixes_.clear();
      wave_surfaces_.clear();
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
      if (!geometry_->water.empty()) {
        water_ = std::make_unique<RW::WaterImage>(bytes_, *geometry_);
        std::vector<WD::TileSource> sources;
        sources.reserve(geometry_->water.size());
        for (const auto &w : geometry_->water) {
          const auto &c = geometry_->chunks.at(w.chunk);
          WD::TileSource source;
          source.chunk = w.chunk;
          source.neighbours = w.neighbours;
          source.mask = w.mask;
          source.second_set = w.second_set;
          source.bounds = c.bounds_bits;
          source.origin = {c.translation[0], c.translation[1]};
          source.chunk_z = c.translation[2];
          source.flag_0x200 = (c.flags & 0x200u) != 0;
          sources.push_back(source);
        }
        waves_ = std::make_unique<WD::Field>(std::move(sources));
        wader_ = std::make_unique<RW::Wader>(
            *water_, [this](std::uint32_t strength, std::uint32_t radius,
                            const RW::cw::Vec3 &point) {
              return disturb_all(strength, radius, point);
            });
        auto *image = water_.get();
        waves_->set_chunk_query(
            [image](std::uint32_t radius, const std::array<std::uint32_t, 3> &p) {
              return image->disturb_chunks(radius, {p[0], p[1], p[2]});
            });
      }
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
      if (waves_) {
        // customVars lines after reset and setup (research section 7 step 2).
        // Whether the factory runs before setup was not executed.
        std::vector<std::string> lines;
        for (const auto &record : records) {
          if (!custom_vars(record.cls))
            continue;
          for (const auto &prop : record.props)
            lines.push_back(prop);
        }
        const auto parsed = WD::replacing(lines);
        waves_->set_appearance(parsed.appearance);
        out["water_lines"] = std::int64_t(parsed.applied);
        out["water_lines_refused"] = std::int64_t(parsed.refused);
        out["water_negative_fix"] =
            parsed.appearance.fix[0] < 0 || parsed.appearance.fix[1] < 0;
      }
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
  // One water_disturb: the service counts the chunks. The service grid pointer
  // stays 0, so it writes nothing; the drawn field holds the grids and takes
  // the same call through the same query.
  std::uint32_t disturb_all(std::uint32_t strength, std::uint32_t radius,
                            const RW::cw::Vec3 &point) {
    const auto count = water_->disturb(strength, radius, point);
    if (count != 0 && waves_)
      waves_->disturb(strength, radius, point);
    return count;
  }
  int64_t water_disturb(int64_t strength, int64_t radius,
                        const PackedInt64Array &position) {
    if (!water_)
      return 0;
    try {
      return static_cast<std::int64_t>(disturb_all(
          std::uint32_t(strength), std::uint32_t(radius), raw_position(position)));
    } catch (const std::exception &) {
      return -1;
    }
  }
  // One Player_move water step for the scene's player (Wader's contract: the
  // control walk supplies frame, feet and a moved-this-tick flag).
  Dictionary wade_step(int64_t frame, const PackedInt64Array &position, bool moving) {
    Dictionary out;
    out["ok"] = false;
    try {
      if (frame < 0 || frame > 0xffffffffll)
        throw std::runtime_error("A wading frame does not fit an unsigned word.");
      if (wader_) {
        wader_->step(std::uint32_t(frame), level_flags_, raw_position(position), moving);
        const auto &t = wader_->totals();
        out["in_water"] = wader_->in_water();
        out["steps"] = std::int64_t(t.steps);
        out["ticks_in_water"] = std::int64_t(t.ticks_in_water);
        out["entries"] = std::int64_t(t.entries);
        out["exits"] = std::int64_t(t.exits);
        out["draws"] = std::int64_t(t.draws);
        out["hurts"] = std::int64_t(t.hurts);
        out["disturbs"] = std::int64_t(t.disturbs);
        out["touched"] = std::int64_t(t.touched);
        out["splashes"] = std::int64_t(t.splashes);
      } else {
        out["in_water"] = false;
      }
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
  // Level flag word 0x44fe08 from the level table row (level-lights JSON).
  Dictionary set_level_flags(int64_t flags) {
    Dictionary out;
    out["ok"] = false;
    if (flags < 0 || flags > 0xffffffffll) {
      out["error"] = String("A level flag word does not fit an unsigned word.");
      return out;
    }
    level_flags_ = static_cast<std::uint32_t>(flags);
    if (!waves_) {
      out["ok"] = true;
      return out;
    }
    try {
      waves_->set_level_flags(static_cast<std::uint32_t>(flags));
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
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
  Dictionary water_sync(int64_t frame, bool frozen) {
    Dictionary out;
    out["ok"] = false;
    out["dirty"] = false;
    out["layer"] = WD::LAYER;
    try {
      if (frame < 0 || frame > 0xffffffffll)
        throw std::runtime_error("A water frame does not fit an unsigned word.");
      if (!waves_) {
        out["ok"] = true;
        out["tiles"] = 0;
        out["live"] = 0;
        return out;
      }
      waves_->on_tick(static_cast<std::uint32_t>(frame), frozen);
      out["dirty"] = waves_->dirty();
      if (waves_->dirty() || wave_mesh_.is_null()) {
        wave_fixes_.clear();
        wave_surfaces_.clear();
        wave_mesh_ = water_mesh(waves_->surfaces(), wave_fixes_, wave_surfaces_);
        waves_->clear_dirty();
      }
      std::int64_t triangles = 0;
      for (int i = 0; i < wave_surfaces_.size(); ++i)
        triangles += static_cast<std::int64_t>(
            Dictionary(wave_surfaces_[i]).get("triangles", 0));
      out["mesh"] = wave_mesh_;
      out["fixes"] = wave_fixes_;
      out["surfaces"] = wave_surfaces_;
      out["triangles"] = triangles;
      out["tiles"] = static_cast<std::int64_t>(waves_->tile_count());
      out["live"] = static_cast<std::int64_t>(waves_->live_blocks());
      out["level_flags"] = static_cast<std::int64_t>(waves_->level_flags());
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
  Dictionary water_texture(const PackedByteArray &bytes) {
    Dictionary out;
    out["ok"] = false;
    try {
      if (bytes.is_empty())
        throw std::runtime_error("The water texture is empty.");
      const auto image = champions::decodeRtaTexture(
          {bytes.ptr(), static_cast<std::size_t>(bytes.size())});
      PackedByteArray rgba;
      rgba.resize(static_cast<std::int64_t>(image.rgba.size()));
      if (!image.rgba.empty())
        std::memcpy(rgba.ptrw(), image.rgba.data(), image.rgba.size());
      Ref<Image> picture;
      picture.instantiate();
      picture->set_data(static_cast<int>(image.width), static_cast<int>(image.height),
                        false, Image::FORMAT_RGBA8, rgba);
      out["image"] = picture;
      out["width"] = static_cast<std::int64_t>(image.width);
      out["height"] = static_cast<std::int64_t>(image.height);
      out["ok"] = true;
    } catch (const std::exception &) {
      out["error"] = String("water texture refused");
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
      out["fix2"] = int64_t(bound.fix2);
      out["two_pass"] = bound.two_pass;
      out["equation"] = int64_t(bound.mode.equation);
      out["alpha_fix"] = int64_t(bound.mode.fix);
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
        auto code = [](B blend) {
          return blend == B::On ? 1 : blend == B::Off ? 0 : -1;
        };
        mesh["blend"] = code(m.blend);
        mesh["environment"] = m.environment;
        mesh["environment_blend"] = code(m.environment_blend);
        meshes.push_back(mesh);
      }
      out["meshes"] = meshes;
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
  // Owned RGBA of one explicitly supplied lump texture, including the level
  // environment image. The caller chooses the bytes; no lump name is inferred.
  static Dictionary decode_texture(const PackedByteArray &bytes) {
    Dictionary out;
    out["ok"] = false;
    try {
      const auto image = champions::decodeRtaTexture(
          {bytes.ptr(), std::size_t(bytes.size())});
      out["width"] = int64_t(image.width);
      out["height"] = int64_t(image.height);
      PackedByteArray rgba;
      rgba.resize(std::int64_t(image.rgba.size()));
      if (!image.rgba.empty())
        std::memcpy(rgba.ptrw(), image.rgba.data(), image.rgba.size());
      out["rgba"] = rgba;
      out["ok"] = true;
    } catch (const std::exception &e) {
      out["error"] = String::utf8(e.what());
    }
    return out;
  }
};

void register_real_world() { GDREGISTER_CLASS(ChampionsRealWorld); }
