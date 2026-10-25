// Counts-only acceptance measurement of the merged native modules against an
// explicitly selected, read-only RTA directory. Prints aggregate TSV counts and
// grouped refusal reasons only: never file names, paths, bytes or decoded values.
// Input files are opened read-only and never modified.
#include "champions/animation.hpp"
#include "champions/animation_decoder.hpp"
#include "champions/archive.hpp"
#include "champions/fx.hpp"
#include "champions/fx_skills.hpp"
#include "champions/inventory_icon_assoc.hpp"
#include "champions/inventory_icons.hpp"
#include "champions/level_flow.hpp"
#include "champions/level_texture.hpp"
#include "champions/model.hpp"
#include "champions/model_texture_bind.hpp"
#include "champions/native_audio.hpp"
#include "champions/real_world.hpp"
#include "champions/skinning.hpp"
#include "champions/texture.hpp"
#include "champions/terrain_file.hpp"
#include "champions/ui_font.hpp"
#include "champions/ui_layout.hpp"
#include "champions/world_geometry.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace lt = champions::presentation::rta::level_texture;
namespace wg = champions::presentation::rta::world_geometry;
namespace lf = champions::simulation::rta::level_flow;
namespace ad = champions::simulation::rta::animation_decoder;

namespace {

using Blob = std::vector<std::uint8_t>;
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;

std::string lower(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string extension(const std::string &name) {
  const auto dot = name.rfind('.');
  return dot == std::string::npos ? std::string() : lower(name.substr(dot + 1));
}
std::string stem(const std::string &name) {
  const auto dot = name.rfind('.');
  return lower(dot == std::string::npos ? name : name.substr(0, dot));
}

// Reasons are native error messages. Strip anything after '(' or ':' so a
// message that quotes a resource name cannot carry it into the report.
std::string reason(const std::exception &e) {
  std::string s = e.what();
  const auto cut = s.find_first_of("(:");
  if (cut != std::string::npos)
    s = s.substr(0, cut);
  while (!s.empty() && s.back() == ' ')
    s.pop_back();
  for (auto &c : s)
    if (c == '\t' || c == '\n' || c == '\r')
      c = ' ';
  return s.empty() ? std::string("unnamed refusal") : s;
}

struct Tally {
  std::map<std::string, std::uint64_t> counts;
  std::map<std::string, std::map<std::string, std::uint64_t>> reasons;
  void add(const std::string &key, std::uint64_t n = 1) { counts[key] += n; }
  void refuse(const std::string &module, const std::exception &e) {
    counts[module + ".refused"] += 1;
    reasons[module][reason(e)] += 1;
  }
};

std::optional<Blob> read_file(const fs::path &path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > MAX_FILE)
    return std::nullopt;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return std::nullopt;
  Blob bytes(static_cast<std::size_t>(size));
  if (size && !in.read(reinterpret_cast<char *>(bytes.data()),
                       static_cast<std::streamsize>(size)))
    return std::nullopt;
  return bytes;
}

champions::Bytes view(const Blob &b) { return {b.data(), b.size()}; }
Blob copy(champions::Bytes b) { return Blob(b.data, b.data + b.size); }

struct Child {
  std::string name;
  champions::Bytes bytes;
};
std::vector<Child> children(champions::Bytes bytes, const std::string &ext) {
  std::vector<Child> out;
  const auto entries =
      ext == "gob" ? champions::readGob(bytes) : champions::readLmp(bytes);
  for (const auto &e : entries)
    out.push_back({e.name, champions::payload(bytes, e)});
  return out;
}

// Every non-container payload, recursing GOB/LMP to depth 2 like the validator.
void flatten(champions::Bytes bytes, const std::string &ext, int depth,
             std::vector<Child> &out, Tally &t) {
  std::vector<Child> list;
  try {
    list = children(bytes, ext);
    t.add("containers.read");
  } catch (const std::exception &e) {
    t.refuse("containers", e);
    return;
  }
  for (auto &c : list) {
    const auto x = extension(c.name);
    if ((x == "gob" || x == "lmp") && depth < 2)
      flatten(c.bytes, x, depth + 1, out, t);
    else
      out.push_back(c);
  }
}

const Child *find(const std::vector<Child> &list, const std::string &name) {
  for (const auto &c : list)
    if (lower(c.name) == lower(name))
      return &c;
  return nullptr;
}

struct Disc {
  std::map<std::string, fs::path> files; // lower-case file name -> path
  std::vector<fs::path> ordered;
};

Disc scan(const fs::path &root) {
  Disc d;
  for (auto it = fs::recursive_directory_iterator(root); it != fs::end(it);
       ++it) {
    if (it->is_symlink() || !it->is_regular_file())
      continue;
    d.ordered.push_back(it->path());
    d.files.emplace(lower(it->path().filename().string()), it->path());
  }
  std::sort(d.ordered.begin(), d.ordered.end());
  return d;
}

// --- level textures + WORLD pairing ----------------------------------------
// Water tiles, moving pieces and the scene's static-mesh budget, counts only.
void measure_world_parts(const Blob &world, const wg::Geometry &g,
                         const std::vector<lf::Record> &records, Tally &t) {
  std::uint64_t surfaces = 0, triangles = 0;
  for (const auto &c : g.chunks) {
    if (!c.cell || !c.packet_index)
      continue;
    for (const auto &b : g.packets[*c.packet_index].batches)
      if (!b.triangles.empty()) {
        ++surfaces;
        triangles += b.triangles.size();
      }
  }
  t.add("scene_mesh.surfaces", surfaces);
  t.add("scene_mesh.triangles", triangles);
  if (surfaces >= 4096 || triangles > 1000000)
    t.add(surfaces >= 4096 ? "scene_mesh.levels_over_4096_surfaces"
                           : "scene_mesh.levels_over_1M_triangles");
  t.add("scene_mesh.max_surfaces_seen", 0);
  if (surfaces > t.counts["scene_mesh.max_surfaces_seen"])
    t.counts["scene_mesh.max_surfaces_seen"] = surfaces;
  if (triangles > t.counts["scene_mesh.max_triangles_seen"])
    t.counts["scene_mesh.max_triangles_seen"] = triangles;
  if (!g.water.empty()) {
    t.add("water.levels");
    t.add("water.tiles", g.water.size());
    std::uint64_t second = 0, cells = 0, centre_hits = 0;
    try {
      champions::real_world::WaterImage image(world, g);
      for (const auto &w : g.water) {
        second += w.second_set;
        for (unsigned r = 0; r < 32; ++r)
          for (unsigned c = 0; c < 32; ++c)
            cells += w.cell(r, c);
        const auto &c = g.chunks[w.chunk];
        const champions::simulation::rta::Vec3 centre{
            champions::simulation::rta::from_int(std::uint32_t(c.translation[0] / 4)),
            champions::simulation::rta::from_int(std::uint32_t(c.translation[1] / 4)),
            c.bounds_bits[2]};
        centre_hits += image.test(champions::simulation::rta::from_int(12), centre).in_water;
      }
      t.add("water.service_images");
      t.add("water.tile_centres_in_water", centre_hits);
    } catch (const std::exception &e) {
      t.refuse("water.service_image", e);
    }
    t.add("water.tiles_second_set", second);
    t.add("water.mask_cells_set", cells);
  }
  if (!g.pieces.empty()) {
    t.add("pieces.levels");
    t.add("pieces.total", g.pieces.size());
    std::uint64_t drawn = 0;
    for (const auto &m : g.pieces)
      for (std::int32_t k = 0; k < m.count; ++k)
        drawn += g.chunks[std::size_t(m.first_chunk + k)].cell != 0;
    t.add("pieces.drawn_chunks", drawn);
    // Extent of each drawn piece chunk's rest-pose vertices (size classes).
    for (const auto &m : g.pieces)
      for (std::int32_t k = 0; k < m.count; ++k) {
        const auto &c = g.chunks[std::size_t(m.first_chunk + k)];
        if (!c.cell || !c.packet_index)
          continue;
        double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
        for (const auto &batch : c.world_vertices)
          for (const auto &v : batch)
            for (int a = 0; a < 3; ++a) {
              lo[a] = std::min(lo[a], v[a]);
              hi[a] = std::max(hi[a], v[a]);
            }
        const double ext = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
        t.add(ext < 4 ? "pieces.drawn_extent_lt4" : ext < 32 ? "pieces.drawn_extent_lt32"
              : ext < 128 ? "pieces.drawn_extent_lt128" : "pieces.drawn_extent_ge128");
      }
    try {
      champions::real_world::PieceScene scene(world, g, records);
      t.add("pieces.claimed", scene.claimed());
      t.add("pieces.unclaimed", scene.size() - scene.claimed());
      t.add("pieces.claim_attempts", scene.claim_attempts());
      t.add("pieces.started_open", scene.started_open());
      std::uint64_t solid = 0;
      for (std::size_t i = 0; i < scene.size(); ++i)
        solid += scene.collision(i);
      t.add("pieces.collision_on_after_load", solid);
    } catch (const std::exception &e) {
      t.refuse("pieces.scene", e);
    }
  }
}

void measure_levels(const Disc &disc, Tally &t) {
  std::vector<std::string> names;
  for (const auto &[n, p] : disc.files)
    names.push_back(n);
  for (const auto &[name, path] : disc.files) {
    if (extension(name) != "tex")
      continue;
    t.add("level_tex.files");
    auto bytes = read_file(path);
    if (!bytes) {
      t.add("level_tex.unreadable");
      continue;
    }
    std::optional<lt::File> file;
    try {
      file.emplace(*bytes);
      t.add("level_tex.files_parsed");
    } catch (const std::exception &e) {
      t.refuse("level_tex.file", e);
      continue;
    }
    bool all = true;
    for (const auto &entry : file->directory()) {
      t.add("level_tex.cells");
      try {
        const auto cell = file->load_cell(entry.id);
        t.add("level_tex.cells_loaded");
        for (std::size_t i = 0; i < cell.textures().size(); ++i) {
          t.add("level_tex.textures");
          try {
            const auto image = cell.decode(i);
            t.add("level_tex.textures_decoded");
            t.add("level_tex.decoded_pixels",
                  std::uint64_t(image.width) * image.height);
          } catch (const std::exception &e) {
            all = false;
            t.refuse("level_tex.texture", e);
          }
        }
      } catch (const std::exception &e) {
        all = false;
        t.refuse("level_tex.cell", e);
      }
    }
    if (all)
      t.add("level_tex.files_fully_decoded");

    // WORLD from <stem>.gob -> <stem>.lmp (the documented <LEVEL>.TEX name).
    const auto gob = disc.files.find(stem(name) + ".gob");
    if (gob == disc.files.end()) {
      t.add("level.no_matching_gob");
      continue;
    }
    auto gob_bytes = read_file(gob->second);
    if (!gob_bytes)
      continue;
    t.add("level.gobs");
    try {
      const auto top = children(view(*gob_bytes), "gob");
      if (const auto *env = find(top, "env.lmp")) {
        t.add("level.env_lump");
        try {
          const auto members = children(env->bytes, "lmp");
          t.add("level.env_members", members.size());
          const Child *etex = nullptr;
          for (const auto &member : members)
            if (extension(member.name) == "etex") {
              etex = &member;
              break;
            }
          if (!etex)
            t.add("level.env_no_etex");
          else {
            std::string member = etex->name;
            for (auto &ch : member)
              ch = char(std::tolower(static_cast<unsigned char>(ch)));
            t.add(member == "envtex.etex" ? "level.env_member_envtex"
                                          : "level.env_member_other");
            try {
              const auto image = champions::decodeRtaTexture(etex->bytes);
              t.add("level.env_decoded");
              t.add(image.width == 128 && image.height == 128
                        ? "level.env_128"
                        : "level.env_other_size");
            } catch (const std::exception &e) {
              t.refuse("level.env", e);
            }
          }
        } catch (const std::exception &e) {
          t.refuse("level.env", e);
        }
      } else
        t.add("level.env_absent");
      const auto runtime = lf::gob_entries(view(*gob_bytes));
      t.add(runtime.size() == top.size() ? "level.gob_runtime_walk_same_count"
                                         : "level.gob_runtime_walk_diff_count");
      const auto *lump = find(top, stem(name) + ".lmp");
      if (!lump) {
        t.add("level.no_level_lump");
        continue;
      }
      const auto level = children(lump->bytes, "lmp");
      // Manifest steps resolvable from disc listing + GOB + level lump alone.
      lf::ManifestInput in;
      in.base = stem(name);
      for (const auto &c : top)
        in.gob_lumps.push_back(c.name);
      for (const auto &c : level)
        in.level_entries.push_back(c.name);
      in.disc_files = names;
      try {
        const auto manifest = lf::level_manifest(in);
        t.add("level_manifest.resolved");
        t.add("level_manifest.resources", manifest.size());
      } catch (const lf::MissingResource &e) {
        t.refuse("level_manifest", e);
      } catch (const std::exception &e) {
        t.refuse("level_manifest", e);
      }
      std::vector<lf::Record> level_records;
      if (const auto *objects = find(level, "objects.ob")) {
        try {
          const auto records = lf::parse_objects(objects->bytes);
          level_records = records;
          t.add("level_objects.parsed");
          t.add("level_objects.records", records.size());
        } catch (const std::exception &e) {
          t.refuse("level_objects", e);
        }
      }
      std::vector<std::string> level_names;
      for (const auto &c : level)
        level_names.push_back(c.name);
      const auto world = lf::first_entry_with_extension(level_names, "world");
      if (!world) {
        t.add("level.no_world");
        continue;
      }
      const auto world_bytes = copy(find(level, *world)->bytes);
      t.add("level_world.inputs");
      try {
        champions::simulation::rta::terrain_file::decode(world_bytes);
        t.add("level_world.terrain_decoded");
      } catch (const std::exception &e) {
        t.refuse("level_world.terrain", e);
      }
      try {
        const auto geometry = wg::decode(world_bytes);
        t.add("level_world.geometry_decoded");
        measure_world_parts(world_bytes, geometry, level_records, t);
        try {
          const auto surfaces = lt::bind_world(geometry, *file, world_bytes);
          t.add("level_pair.bound");
          t.add("level_pair.surfaces", surfaces.size());
          std::uint64_t dynamic = 0, cutout = 0, env = 0;
          for (const auto &s : surfaces) {
            dynamic += s.material.dynamic_light;
            cutout += s.material.cutout();
            env += s.material.environment;
          }
          t.add("level_pair.surfaces_dynamic_light", dynamic);
          t.add("level_pair.surfaces_cutout", cutout);
          t.add("level_pair.surfaces_environment", env);
          // Display budget inputs: distinct images, RGBA bytes, vertices and
          // (chunk, material) groups a merged scene would draw.
          std::map<const void *, std::uint64_t> images;
          std::map<std::pair<std::uint32_t, std::uint64_t>, int> groups;
          std::uint64_t vertices = 0, chunks_drawn = 0;
          std::uint32_t last_chunk = UINT32_MAX;
          for (const auto &s : surfaces) {
            images[s.image.get()] = s.image->rgba.size();
            vertices += s.positions.size();
            groups[{s.chunk, (std::uint64_t(s.material.alpha1) << 40) ^
                                 (std::uint64_t(s.material.test1) << 8) ^
                                 (std::uint64_t(s.material.blend_enabled) << 1) ^
                                 std::uint64_t(s.material.sort_layer) << 4 ^
                                 std::uint64_t(s.material.dynamic_light)}] = 1;
            if (s.chunk != last_chunk) {
              ++chunks_drawn;
              last_chunk = s.chunk;
            }
          }
          std::uint64_t rgba = 0;
          for (const auto &i : images)
            rgba += i.second;
          auto peak = [&](const std::string &k, std::uint64_t v) {
            if (v > t.counts[k])
              t.counts[k] = v;
          };
          peak("level_pair.max_level_images", images.size());
          peak("level_pair.max_level_rgba_bytes", rgba);
          peak("level_pair.max_level_vertices", vertices);
          peak("level_pair.max_level_surfaces", surfaces.size());
          peak("level_pair.max_level_chunk_groups", groups.size());
          peak("level_pair.max_level_drawn_chunks", chunks_drawn);
          t.add("level_pair.rgba_bytes", rgba);
        } catch (const std::exception &e) {
          t.refuse("level_pair", e);
        }
      } catch (const std::exception &e) {
        t.refuse("level_world.geometry", e);
      }
    } catch (const std::exception &e) {
      t.refuse("level.gob", e);
    }
  }
}

// --- inventory icons ---------------------------------------------------------
void measure_icons(const Disc &disc, Tally &t) {
  for (const auto &[name, path] : disc.files) {
    if (extension(name) != "hdr")
      continue;
    t.add("icons.hdr_files");
    const auto dat = disc.files.find(stem(name) + ".dat");
    auto hdr = read_file(path);
    if (dat == disc.files.end() || !hdr) {
      t.add("icons.hdr_without_dat");
      continue;
    }
    auto dat_bytes = read_file(dat->second);
    if (!dat_bytes) {
      t.add("icons.dat_unreadable_or_over_cap");
      continue;
    }
    try {
      const auto index =
          champions::inventory_icons::parse_index(hdr->data(), hdr->size());
      t.add("icons.indexes_parsed");
      t.add("icons.groups", index.size());
      for (const auto &group : index)
        for (const auto &entry : group.entries) {
          t.add("icons.pictures");
          try {
            const auto range =
                champions::inventory_icons::payload_range(entry,
                                                          dat_bytes->size());
            const auto pic = champions::inventory_icons::decode(
                dat_bytes->data() + range.offset, range.size);
            t.add("icons.pictures_decoded");
            if (pic.width == entry.width && pic.height == entry.height)
              t.add("icons.pictures_index_size_match");
          } catch (const std::exception &e) {
            t.refuse("icons.picture", e);
          }
        }
    } catch (const std::exception &e) {
      t.refuse("icons.index", e);
    }
  }
}

// --- audio -------------------------------------------------------------------
void decode_bank(const Blob &dir, const Blob &vat, Tally &t) {
  const auto before = t.counts["audio.vat_entry.refused"];
  try {
    const auto entries = champions::audio::parse_vat_directory(
        {dir.data(), dir.size()}, vat.size());
    t.add("audio.vat_directories_parsed");
    for (const auto &e : entries) {
      t.add("audio.vat_entries");
      try {
        const auto pcm =
            champions::audio::decode_vat_entry({vat.data(), vat.size()}, e);
        t.add("audio.vat_entries_decoded");
        t.add("audio.vat_samples", pcm.left.size());
      } catch (const std::exception &x) {
        t.refuse("audio.vat_entry", x);
      }
    }
  } catch (const std::exception &x) {
    t.refuse("audio.vat_directory", x);
  }
  if (t.counts["audio.vat_entry.refused"] != before)
    t.add("audio.vat_files_with_refused_entries");
}

void measure_audio(const Disc &disc, const std::vector<Child> &payloads,
                   Tally &t) {
  for (const auto &[name, path] : disc.files) {
    if (extension(name) != "vat")
      continue;
    t.add("audio.vat_files");
    auto vat = read_file(path);
    if (!vat)
      continue;
    // Directory: ntsc_dialog.bin inside <stem>.gob/<stem>.lmp or <stem>.lmp.
    std::optional<Blob> dir;
    for (const auto &holder : {stem(name) + ".gob", stem(name) + ".lmp"}) {
      const auto it = disc.files.find(holder);
      if (it == disc.files.end() || dir)
        continue;
      auto bytes = read_file(it->second);
      if (!bytes)
        continue;
      try {
        std::vector<Child> list;
        Tally scratch;
        flatten(view(*bytes), extension(holder), 0, list, scratch);
        if (const auto *c = find(list, "ntsc_dialog.bin"))
          dir = copy(c->bytes);
        else if (const auto *d = find(list, "dialog.bin"))
          dir = copy(d->bytes);
      } catch (const std::exception &) {
      }
    }
    if (!dir) {
      t.add("audio.vat_without_directory");
      continue;
    }
    t.add("audio.vat_with_directory");
    decode_bank(*dir, *vat, t);
  }
  auto vag = [&](champions::Bytes b, const char *key) {
    t.add(std::string(key) + ".inputs");
    try {
      const auto pcm = champions::audio::decode_vag({b.data, b.size});
      t.add(std::string(key) + ".decoded");
      t.add(std::string(key) + ".samples", pcm.left.size());
    } catch (const std::exception &e) {
      t.refuse(key, e);
    }
  };
  for (const auto &[name, path] : disc.files)
    if (extension(name) == "vag" || extension(name) == "va1")
      if (auto b = read_file(path))
        vag(view(*b), "audio.loose_vag");
  for (const auto &c : payloads)
    if (extension(c.name) == "vag")
      vag(c.bytes, "audio.nested_vag");
}

// --- fonts -------------------------------------------------------------------
void measure_fonts(const std::vector<Child> &payloads, Tally &t) {
  for (const auto &c : payloads) {
    if (extension(c.name) != "fnt")
      continue;
    t.add("fonts.inputs");
    try {
      const auto font = champions::readRtaUiFont(c.bytes);
      t.add("fonts.read");
      t.add("fonts.glyphs", font.glyphs.size());
      try {
        champions::decodeRtaTexture({font.texture.data(), font.texture.size()});
        t.add("fonts.page_decoded");
      } catch (const std::exception &e) {
        t.refuse("fonts.page", e);
      }
    } catch (const std::exception &e) {
      t.refuse("fonts.read", e);
    }
    try {
      const auto metrics = champions::ui_layout::fontMetrics(c.bytes);
      t.add("fonts.metrics");
      t.add("fonts.kerns", metrics.kerns.size());
    } catch (const std::exception &e) {
      t.refuse("fonts.metrics", e);
    }
  }
}

// --- animation decoder + model/clip skinning --------------------------------
void measure_animation(const fs::path &root, Tally &t) {
  for (const auto &path : scan(root).ordered) {
    const auto x = extension(path.filename().string());
    if (x != "lmp" && x != "gob")
      continue;
    auto bytes = read_file(path);
    if (!bytes)
      continue;
    // Group by immediate container so skinning tries only same-lump pairs.
    std::vector<std::pair<std::string, champions::Bytes>> lumps;
    try {
      if (x == "lmp")
        lumps.push_back({"", view(*bytes)});
      else
        for (const auto &c : children(view(*bytes), "gob"))
          if (extension(c.name) == "lmp")
            lumps.push_back({c.name, c.bytes});
    } catch (const std::exception &) {
      continue;
    }
    for (const auto &[unused, lump] : lumps) {
      std::vector<Child> list;
      try {
        list = children(lump, "lmp");
      } catch (const std::exception &) {
        continue;
      }
      std::vector<champions::RtaModel> models;
      std::vector<champions::RtaAnimationKeys> clips;
      for (const auto &c : list) {
        const auto cx = extension(c.name);
        if (cx == "anm") {
          t.add("anim_decoder.clips");
          try {
            const auto clip = ad::parse_clip(copy(c.bytes));
            t.add("anim_decoder.parsed");
            try {
              ad::decode_clip(clip, champions::simulation::rta::ZERO);
              t.add("anim_decoder.decoded_t0");
            } catch (const std::exception &e) {
              t.refuse("anim_decoder.decode", e);
            }
          } catch (const std::exception &e) {
            t.refuse("anim_decoder.parse", e);
          }
          try {
            clips.push_back(champions::decodeRtaAnimationKeys(c.bytes));
          } catch (const std::exception &e) {
            t.refuse("anim_keys", e);
          }
        } else if (cx == "vif") {
          try {
            models.push_back(champions::decodeRtaModel(c.bytes));
          } catch (const std::exception &e) {
            t.refuse("model", e);
          }
          // Same-base texture in the same lump (model-texture.md).
          t.add("model_texture.models");
          const auto *tex = find(list, champions::model_texture::companion_texture_name(c.name));
          if (!tex) {
            t.add("model_texture.no_companion");
          } else {
            try {
              const auto bound = champions::model_texture::bind(c.bytes, tex->bytes);
              t.add("model_texture.bound");
              t.add("model_texture.alpha1_" + std::to_string(bound.alpha1));
              t.add(bound.two_pass ? "model_texture.two_pass"
                                   : "model_texture.one_pass");
              t.add(bound.mode.equation == 0
                        ? "model_texture.alpha_undocumented"
                        : "model_texture.alpha_documented");
              if (bound.two_pass)
                t.add(bound.mode.equation == 0
                          ? "model_texture.two_pass_undocumented"
                          : "model_texture.two_pass_documented");
              t.add("model_texture.equation_" +
                    std::to_string(bound.mode.equation));
              t.add("model_texture.fix2_" + std::to_string(bound.fix2));
              // Refusal counts are always printed, zero included.
              t.add("model_texture.fix2_above_0x80",
                    bound.fix2_above_documented ? 1 : 0);
              for (const auto &m : bound.meshes) {
                using B = champions::model_texture::Blend;
                t.add(m.blend == B::On ? "model_texture.meshes_blend_on"
                      : m.blend == B::Off ? "model_texture.meshes_blend_off"
                      : m.blend == B::Mixed ? "model_texture.meshes_blend_mixed"
                                            : "model_texture.meshes_blend_unstated");
                t.add(m.environment_tag ? "model_texture.meshes_env_tag"
                                        : "model_texture.meshes_no_env_tag");
                if (!m.environment)
                  continue;
                t.add("model_texture.meshes_env_drawn");
                t.add(m.environment_blend == B::On
                          ? "model_texture.meshes_env_blend_on"
                      : m.environment_blend == B::Off
                          ? "model_texture.meshes_env_blend_off"
                      : m.environment_blend == B::Mixed
                          ? "model_texture.meshes_env_blend_mixed"
                          : "model_texture.meshes_env_blend_unstated");
              }
              if (bound.two_pass_mesh_without_env_tag)
                t.add("model_texture.two_pass_mesh_without_env_tag");
              t.add("model_texture.env_tag_without_two_pass",
                    bound.env_tag_without_two_pass ? 1 : 0);
            } catch (const std::exception &e) {
              t.refuse("model_texture", e);
            }
          }
        }
      }
      if (models.empty() || clips.empty())
        continue;
      t.add("pairing.lumps_with_models_and_clips");
      for (const auto &model : models) {
        t.add("pairing.models_considered");
        if (model.attachment_slot != -1) {
          t.add("pairing.models_attachment_skipped");
          continue;
        }
        bool any = false;
        for (const auto &clip : clips) {
          if (clip.skeleton.joint_mask != 0) {
            t.add("pairing.partial_clip_skipped");
            continue;
          }
          t.add("pairing.pairs_tried");
          try {
            const auto pose = champions::sampleRtaAnimationPose(clip, 0.0);
            const auto tf =
                champions::buildRtaAnimationTransforms(clip.skeleton, pose);
            champions::skinRtaModelPositions(model, tf);
            t.add("pairing.pairs_skinned");
            any = true;
          } catch (const std::exception &e) {
            t.refuse("pairing.pair", e);
          }
        }
        if (any)
          t.add("pairing.models_with_skinnable_clip");
      }
    }
  }
}

// --- FX run-time JSON ---------------------------------------------------------
std::optional<std::string> read_text(const char *path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(in), {});
}

void measure_fx(const char *defs, const char *recipes, Tally &t) {
  if (defs) {
    if (auto text = read_text(defs)) {
      std::map<std::uint32_t, champions::rta::fx::Definition> out;
      std::string error;
      if (champions::rta::fx::load_definition_json(*text, out, error)) {
        t.add("fx_defs.loaded");
        t.add("fx_defs.definitions", out.size());
      } else {
        t.counts["fx_defs.refused"] += 1;
        t.reasons["fx_defs"][reason(std::runtime_error(error))] += 1;
      }
    }
  }
  if (recipes) {
    if (auto text = read_text(recipes)) {
      try {
        const auto r = champions::rta::fx::skills::RecipeConstants::from_json(*text);
        t.add("fx_recipes.loaded");
        t.add("fx_recipes.calls", r.calls().size());
        t.add("fx_recipes.particle_definitions", r.particle_definitions.size());
      } catch (const std::exception &e) {
        t.refuse("fx_recipes", e);
      }
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  const char *dir = nullptr, *defs = nullptr, *recipes = nullptr;
  bool rta = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--game" && i + 1 < argc)
      rta = std::string(argv[++i]) == "rta";
    else if (a == "--fx-defs" && i + 1 < argc)
      defs = argv[++i];
    else if (a == "--fx-recipes" && i + 1 < argc)
      recipes = argv[++i];
    else if (!dir)
      dir = argv[i];
    else
      dir = nullptr, i = argc;
  }
  if (!rta || !dir) {
    std::cerr << "usage: champions-real-data-measure --game rta <directory> "
                 "[--fx-defs particle.json] [--fx-recipes recipes.json]\n";
    return 2;
  }
  Tally t;
  try {
    const auto disc = scan(dir);
    if (disc.files.empty()) {
      std::cerr << "no files\n";
      return 1;
    }
    std::vector<Child> payloads;
    std::vector<Blob> keep;
    for (const auto &path : disc.ordered) {
      const auto x = extension(path.filename().string());
      if (x != "gob" && x != "lmp")
        continue;
      if (auto b = read_file(path)) {
        keep.push_back(std::move(*b));
        flatten(view(keep.back()), x, 0, payloads, t);
      }
    }
    t.add("payloads.nested", payloads.size());
    measure_levels(disc, t);
    measure_icons(disc, t);
    measure_audio(disc, payloads, t);
    measure_fonts(payloads, t);
    measure_animation(dir, t);
    measure_fx(defs, recipes, t);
  } catch (const std::exception &e) {
    std::cerr << "measurement failed: " << reason(e) << "\n";
    return 1;
  }
  std::cout << "metric\tcount\n";
  for (const auto &[k, v] : t.counts)
    std::cout << k << '\t' << v << '\n';
  for (const auto &[module, reasons] : t.reasons)
    for (const auto &[r, n] : reasons)
      std::cout << "refusal\t" << module << '\t' << n << '\t' << r << '\n';
  return 0;
}
