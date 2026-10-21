// Streamed WORLD/TEX display source. open() keeps the WORLD geometry and the
// still-compressed TEX file; cell_view() decodes one texture cell and batches
// it for drawing. The scene keeps the 3 x 3 cells around the camera focus, as
// the original does (research docs/level-flow.md 13.1; docs/LEVEL_TEXTURES.md).
// Display only: nothing here reaches the simulation.
#include "champions/level_texture.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

using namespace godot;
namespace LT = champions::presentation::rta::level_texture;
namespace WG = champions::presentation::rta::world_geometry;
Dictionary level_automap_dictionary(const std::vector<std::uint8_t> &world_bytes);

namespace {
std::vector<std::uint8_t> read_file(const String &path, std::uint64_t limit) {
  Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
  if (f.is_null())
    throw std::runtime_error("The selected WORLD or TEX file could not be opened.");
  const auto size = f->get_length();
  if (size > limit)
    throw std::runtime_error("The selected WORLD or TEX file exceeds the preview limit.");
  auto bytes = f->get_buffer(size);
  if (std::uint64_t(bytes.size()) != size)
    throw std::runtime_error("The selected file changed while being read.");
  if (!bytes.size())
    return {};
  return {bytes.ptr(), bytes.ptr() + bytes.size()};
}

// Blend modes whose result depends on what is already in the frame
// (level_texture_material.gd: mix, add, premul and the automap mode).
bool order_dependent(const LT::Surface::Material &m) {
  return m.environment ||
         (m.blend_enabled && (m.alpha1 == 1 || m.alpha1 == 9 ||
                              m.alpha1 == 0x44 || m.alpha1 == 0x48));
}
// Chunk flag 0x20: a moving piece (docs/formats/world.md chunk flags; D: exactly
// the pieces' chunks). Its batches keep their own node so a pose can move them.
bool moving_piece(const LT::Surface &s) {
  return (s.material.flags_after & 0x20u) != 0;
}
// The full loader's run rule (level_texture_preview.cpp same_material).
bool same_run(const LT::Surface &a, const LT::Surface &b) {
  const auto &x = a.material, &y = b.material;
  return a.chunk == b.chunk && a.image == b.image && x.alpha1 == y.alpha1 &&
         x.test1 == y.test1 && x.blend_enabled == y.blend_enabled &&
         x.light_constant == y.light_constant &&
         x.dynamic_light == y.dynamic_light && x.sort_layer == y.sort_layer &&
         x.environment == y.environment;
}
using MaterialKey =
    std::tuple<std::uint8_t, std::uint32_t, bool, std::int16_t, bool, std::uint8_t>;
MaterialKey material_key(const LT::Surface::Material &m) {
  return {m.alpha1, m.test1, m.blend_enabled, m.light_constant, m.dynamic_light,
          m.sort_layer};
}
bool uv_inside(const LT::Surface &s) {
  for (const auto &uv : s.uv)
    if (!(uv[0] >= 0.0 && uv[0] <= 1.0 && uv[1] >= 0.0 && uv[1] <= 1.0))
      return false;
  return true;
}

struct Group {
  const LT::Image *image = nullptr;
  std::size_t atlas = SIZE_MAX; // index into the cell's atlas pages
  LT::Surface::Material material;
  std::optional<std::uint8_t> quarter;
  bool ordered = false, moving = false;
  std::size_t chunk = 0, batch = 0;
  int draw_order = 0;
  std::vector<const LT::Surface *> parts;
};

// Shelf packing of whole images with a one-texel clamped border, so bilinear
// filtering at a UV edge reads the same texels as clamp-to-edge on the image.
constexpr int ATLAS_LIMIT = 4096;
constexpr int BORDER = 1;
struct Atlas {
  int width = 0, height = 0;
  std::vector<std::uint8_t> rgba;
  std::map<const LT::Image *, std::array<int, 2>> at;
};
std::vector<Atlas> pack(std::vector<const LT::Image *> images) {
  std::sort(images.begin(), images.end(), [](auto a, auto b) {
    return std::tie(b->height, b->width) < std::tie(a->height, a->width);
  });
  std::vector<Atlas> pages;
  struct Shelf { int y, h, x; };
  std::vector<std::vector<Shelf>> shelves;
  for (const auto *image : images) {
    const int w = image->width + 2 * BORDER, h = image->height + 2 * BORDER;
    bool placed = false;
    for (std::size_t p = 0; p < pages.size() && !placed; ++p) {
      for (auto &shelf : shelves[p])
        if (h <= shelf.h && shelf.x + w <= ATLAS_LIMIT) {
          pages[p].at[image] = {shelf.x, shelf.y};
          shelf.x += w;
          pages[p].width = std::max(pages[p].width, shelf.x);
          placed = true;
          break;
        }
      if (placed)
        break;
      const int y = shelves[p].empty() ? 0 : shelves[p].back().y + shelves[p].back().h;
      if (y + h <= ATLAS_LIMIT) {
        shelves[p].push_back({y, h, w});
        pages[p].at[image] = {0, y};
        pages[p].width = std::max(pages[p].width, w);
        pages[p].height = y + h;
        placed = true;
      }
    }
    if (!placed) {
      pages.emplace_back();
      shelves.push_back({{0, h, w}});
      pages.back().at[image] = {0, 0};
      pages.back().width = w;
      pages.back().height = h;
    }
  }
  for (auto &page : pages) {
    page.rgba.assign(std::size_t(page.width) * std::size_t(page.height) * 4, 0);
    for (const auto &[image, origin] : page.at) {
      const int iw = image->width, ih = image->height;
      for (int y = -BORDER; y < ih + BORDER; ++y)
        for (int x = -BORDER; x < iw + BORDER; ++x) {
          const int sx = std::clamp(x, 0, iw - 1), sy = std::clamp(y, 0, ih - 1);
          const std::size_t to =
              (std::size_t(origin[1] + BORDER + y) * std::size_t(page.width) +
               std::size_t(origin[0] + BORDER + x)) * 4;
          std::memcpy(page.rgba.data() + to,
                      image->rgba.data() + (std::size_t(sy) * iw + sx) * 4, 4);
        }
    }
  }
  return pages;
}
} // namespace

class ChampionsLevelTextureStream : public RefCounted {
  GDCLASS(ChampionsLevelTextureStream, RefCounted)
  std::vector<std::uint8_t> world_bytes_;
  WG::Geometry geometry_;
  std::unique_ptr<LT::File> textures_;
  double cell_size_ = 960.0;
  std::set<std::int32_t> cells_;

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("open", "world_path", "tex_path"),
                         &ChampionsLevelTextureStream::open);
    ClassDB::bind_method(D_METHOD("focus_cell", "x", "y"),
                         &ChampionsLevelTextureStream::focus_cell);
    ClassDB::bind_method(D_METHOD("window", "cell_id"),
                         &ChampionsLevelTextureStream::window);
    ClassDB::bind_method(D_METHOD("window_order", "cells"),
                         &ChampionsLevelTextureStream::window_order);
    ClassDB::bind_method(D_METHOD("cell_view", "cell_id", "light_base"),
                         &ChampionsLevelTextureStream::cell_view);
  }

public:
  Dictionary open(const String &world_path, const String &tex_path) {
    Dictionary out;
    out["ok"] = false;
    textures_.reset();
    cells_.clear();
    try {
      auto world_bytes = read_file(world_path, 64u * 1024u * 1024u);
      auto tex_bytes = read_file(tex_path, 512u * 1024u * 1024u);
      auto geometry = WG::decode(world_bytes);
      auto textures = std::make_unique<LT::File>(std::move(tex_bytes));
      const double cell_size = LT::texture_cell_size(geometry);
      std::int64_t triangles = 0, batches = 0;
      std::set<std::int32_t> cells;
      for (const auto &chunk : geometry.chunks) {
        if (!chunk.cell || !chunk.packet_index)
          continue;
        cells.insert(chunk.cell);
        for (const auto &batch : geometry.packets.at(*chunk.packet_index).batches)
          if (!batch.triangles.empty()) {
            ++batches;
            triangles += std::int64_t(batch.triangles.size());
          }
      }
      if (cells.empty())
        throw std::runtime_error("The level has no bounded supported textured ground.");
      out["automap"] = level_automap_dictionary(world_bytes);
      world_bytes_ = std::move(world_bytes);
      geometry_ = std::move(geometry);
      textures_ = std::move(textures);
      cell_size_ = cell_size;
      cells_ = std::move(cells);
      out["cell_size"] = cell_size;
      out["cells"] = std::int64_t(cells_.size());
      out["triangles"] = triangles;
      out["batches"] = batches;
      out["ok"] = true;
    } catch (const std::exception &error) {
      textures_.reset();
      out["error"] = String::utf8(error.what());
    }
    return out;
  }

  // Native Z-up position; -1 when nothing is open.
  int64_t focus_cell(double x, double y) const {
    if (!textures_)
      return -1;
    try {
      return LT::cell_id_of(x, y, cell_size_);
    } catch (const std::exception &) {
      return -1;
    }
  }

  // The 3 x 3 window around a focus cell, limited to cells that hold drawn
  // chunks, nearest the focus cell first (ties in window order). The read
  // order metric beyond "nearest first" is not documented.
  PackedInt32Array window(int64_t cell_id) const {
    PackedInt32Array out;
    if (!textures_)
      return out;
    std::vector<std::pair<int, int>> near;
    for (const int cell : LT::cells_around(int(cell_id)))
      if (cells_.count(cell)) {
        const int dr = cell / 100 - int(cell_id) / 100;
        const int dc = cell % 100 - int(cell_id) % 100;
        near.push_back({dr * dr + dc * dc, cell});
      }
    std::stable_sort(near.begin(), near.end(),
                     [](auto a, auto b) { return a.first < b.first; });
    for (const auto &item : near)
      out.push_back(item.second);
    return out;
  }

  // {chunk, batch, draw order} triples over the resident cells.
  PackedInt64Array window_order(const PackedInt32Array &cells) const {
    PackedInt64Array out;
    if (!textures_)
      return out;
    try {
      std::vector<std::int32_t> list(cells.ptr(), cells.ptr() + cells.size());
      for (const auto &slot : LT::draw_slots(geometry_, world_bytes_, &list)) {
        out.push_back(std::int64_t(slot.chunk));
        out.push_back(std::int64_t(slot.batch));
        out.push_back(slot.draw_order);
      }
    } catch (const std::exception &) {
      out.clear();
    }
    return out;
  }

  // One cell's drawing groups. Order-independent batches (opaque, darken and
  // cutout modes, no environment pass, no moving piece, UVs inside the image)
  // share atlas pages and become one group per material and page. The others
  // keep the full loader's consecutive runs and carry their chunk and first
  // batch, so the scene can order them across the resident window.
  Dictionary cell_view(int64_t cell_id, int64_t light_base) const {
    Dictionary out;
    out["ok"] = false;
    if (!textures_) {
      out["error"] = String("texture stream is not open");
      return out;
    }
    try {
      const auto surfaces = LT::bind_cells(
          geometry_, *textures_, world_bytes_, {std::int32_t(cell_id)});
      std::vector<Group> runs;
      std::vector<const LT::Surface *> shared;
      bool extend = false;
      for (const auto &s : surfaces) {
        const bool ordered = order_dependent(s.material);
        const bool moving = moving_piece(s);
        if (!ordered && !moving && uv_inside(s)) {
          shared.push_back(&s);
          extend = false;
          continue;
        }
        if (extend && same_run(*runs.back().parts.back(), s)) {
          runs.back().parts.push_back(&s);
          continue;
        }
        Group g;
        g.image = s.image.get();
        g.material = s.material;
        g.quarter = s.quarter;
        g.ordered = ordered;
        g.moving = moving;
        g.chunk = s.chunk;
        g.batch = s.batch;
        g.draw_order = s.draw_order;
        g.parts.push_back(&s);
        runs.push_back(std::move(g));
        extend = true;
      }
      std::vector<const LT::Image *> images;
      for (const auto *s : shared)
        if (std::find(images.begin(), images.end(), s->image.get()) == images.end())
          images.push_back(s->image.get());
      const auto pages = pack(images);
      std::map<std::pair<MaterialKey, std::size_t>, std::size_t> by_page;
      std::vector<Group> batched;
      for (const auto *s : shared) {
        std::size_t page = 0;
        while (!pages[page].at.count(s->image.get()))
          ++page;
        const auto key = std::make_pair(material_key(s->material), page);
        auto found = by_page.find(key);
        if (found == by_page.end()) {
          found = by_page.emplace(key, batched.size()).first;
          Group g;
          g.atlas = page;
          g.material = s->material;
          g.draw_order = s->draw_order;
          batched.push_back(std::move(g));
        }
        batched[found->second].parts.push_back(s);
      }
      Array out_images, out_groups;
      std::map<const void *, std::int64_t> image_ids;
      auto image_id = [&](const void *key, int width, int height,
                          const std::vector<std::uint8_t> &rgba) {
        auto found = image_ids.find(key);
        if (found != image_ids.end())
          return found->second;
        Dictionary image;
        image["width"] = width;
        image["height"] = height;
        PackedByteArray bytes;
        if (bytes.resize(std::int64_t(rgba.size())) != OK)
          throw std::bad_alloc();
        if (!rgba.empty())
          std::memcpy(bytes.ptrw(), rgba.data(), rgba.size());
        image["rgba"] = bytes;
        const std::int64_t id = out_images.size();
        out_images.push_back(image);
        image_ids.emplace(key, id);
        return id;
      };
      std::int64_t triangles = 0;
      auto emit = [&](const Group &g) {
        std::int64_t count = 0;
        for (const auto *s : g.parts)
          count += std::int64_t(s->positions.size());
        PackedVector3Array positions;
        PackedVector2Array uv;
        if (positions.resize(count) != OK || uv.resize(count) != OK)
          throw std::bad_alloc();
        Vector3 *pw = positions.ptrw();
        Vector2 *uw = uv.ptrw();
        PackedFloat32Array custom;
        const bool env = g.material.environment;
        if (env && custom.resize(count * 4) != OK)
          throw std::bad_alloc();
        const Atlas *page = g.atlas == SIZE_MAX ? nullptr : &pages[g.atlas];
        std::int64_t at = 0;
        for (const auto *s : g.parts) {
          std::array<int, 2> origin{0, 0};
          if (page)
            origin = page->at.at(s->image.get());
          for (std::size_t i = 0; i < s->positions.size(); ++i, ++at) {
            const auto &p = s->positions[i];
            // Session display mapping (x, z, -y), as level_texture_terrain.gd.
            pw[at] = Vector3(real_t(float(p[0])), real_t(float(p[2])),
                             real_t(-float(p[1])));
            const auto &t = s->uv[i];
            if (page)
              uw[at] = Vector2(
                  float((origin[0] + BORDER + t[0] * s->image->width) / page->width),
                  float((origin[1] + BORDER + t[1] * s->image->height) / page->height));
            else
              uw[at] = Vector2(real_t(t[0]), real_t(t[1]));
            if (env) {
              const bool has = i < s->stored_normals.size();
              for (int c = 0; c < 3; ++c)
                custom.ptrw()[at * 4 + c] =
                    has ? float(s->stored_normals[i][std::size_t(c)]) : 0.f;
              custom.ptrw()[at * 4 + 3] = 0.f;
            }
          }
        }
        Dictionary d;
        d["positions"] = positions;
        d["uv"] = uv;
        // A negative light_base leaves constant-light groups uncoloured: the
        // scene applies the loaded level's base level later (TEX-3,
        // level_texture_terrain.gd set_constant_light).
        if (!g.material.dynamic_light && light_base >= 0) {
          const int channel =
              LT::world_constant_channel(int(light_base), int(g.material.light_constant));
          PackedColorArray colors;
          if (colors.resize(count) != OK)
            throw std::bad_alloc();
          colors.fill(Color(channel / 255.0f, channel / 255.0f, channel / 255.0f, 1.0f));
          d["colors"] = colors;
        }
        if (env)
          d["custom0"] = custom;
        d["image"] = page ? image_id(page, page->width, page->height, page->rgba)
                          : image_id(g.image, g.image->width, g.image->height,
                                     g.image->rgba);
        d["alpha1"] = int(g.material.alpha1);
        d["test1"] = std::int64_t(g.material.test1);
        d["blend_enabled"] = g.material.blend_enabled;
        d["environment"] = env;
        d["sort_layer"] = int(g.material.sort_layer);
        d["dynamic_light"] = g.material.dynamic_light;
        d["light_constant"] = int(g.material.light_constant);
        d["quarter"] = g.quarter ? int(*g.quarter) : -1;
        d["draw_order"] = g.draw_order;
        d["atlas"] = page != nullptr;
        if (!page) {
          d["chunk"] = std::int64_t(g.chunk);
          d["batch"] = std::int64_t(g.batch);
          d["moving"] = g.moving;
        }
        d["triangles"] = count / 3;
        triangles += count / 3;
        out_groups.push_back(d);
      };
      for (const auto &g : batched)
        emit(g);
      for (const auto &g : runs)
        emit(g);
      out["images"] = out_images;
      out["groups"] = out_groups;
      out["triangles"] = triangles;
      out["batches"] = std::int64_t(surfaces.size());
      out["cell"] = cell_id;
      out["ok"] = true;
    } catch (const std::exception &error) {
      out["error"] = String::utf8(error.what());
    }
    return out;
  }
};

void register_level_texture_stream() {
  GDREGISTER_CLASS(ChampionsLevelTextureStream);
}
