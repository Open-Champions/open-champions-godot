// Counts one readable WORLD/TEX pair and writes generic cache files.
// Prints counts only. Names, stems and payload bytes stay off stdout.
#include "champions/archive.hpp"
#include "champions/level_texture.hpp"
#include "champions/texture.hpp"
#include "champions/world_geometry.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using champions::Bytes;
using champions::decodeRtaTexture;
using champions::decodeRtaTexture32;
using champions::hasExtension;
using champions::payload;
using champions::readGob;
using champions::readLmp;

namespace {
std::vector<std::uint8_t> read_file(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  in.seekg(0, std::ios::end);
  const auto n = in.tellg();
  if (n < 0)
    return {};
  in.seekg(0);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));
  if (!bytes.empty())
    in.read(reinterpret_cast<char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!in)
    return {};
  return bytes;
}

void write_file(const fs::path &path, const std::uint8_t *data, std::size_t n) {
  std::ofstream out(path, std::ios::binary);
  if (!out)
    throw std::runtime_error("cache file");
  out.write(reinterpret_cast<const char *>(data),
            static_cast<std::streamsize>(n));
  if (!out)
    throw std::runtime_error("cache file");
}

void write_u32(std::ostream &out, std::uint32_t value) {
  const unsigned char bytes[4] = {
      static_cast<unsigned char>(value),
      static_cast<unsigned char>(value >> 8),
      static_cast<unsigned char>(value >> 16),
      static_cast<unsigned char>(value >> 24)};
  out.write(reinterpret_cast<const char *>(bytes), 4);
}

std::string stem_key(const std::string &name) {
  const auto slash = name.find_last_of("/\\");
  std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
  const auto dot = base.find_last_of('.');
  if (dot != std::string::npos)
    base.resize(dot);
  for (char &c : base) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  return base;
}

std::uint16_t u16_at(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  return static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << 8));
}

std::uint32_t u32_at(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  return static_cast<std::uint32_t>(bytes[at]) |
         (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
}

struct EtexCount {
  int lumps = 0;
  int decoded = 0;
  int failed = 0;
  int width = 0;
  int height = 0;
  bool saved = false;
};

struct Pair {
  std::vector<std::uint8_t> world;
  std::vector<std::uint8_t> tex;
};

void note_etex(const std::vector<std::uint8_t> &bytes, EtexCount &etex,
               const fs::path &out_dir) {
  ++etex.lumps;
  try {
    const auto image = decodeRtaTexture32(Bytes{bytes.data(), bytes.size()});
    ++etex.decoded;
    if (!etex.saved) {
      std::ofstream out(out_dir / "env.rgba", std::ios::binary);
      if (!out)
        return;
      write_u32(out, image.width);
      write_u32(out, image.height);
      out.write(reinterpret_cast<const char *>(image.rgba.data()),
                static_cast<std::streamsize>(image.rgba.size()));
      if (!out)
        return;
      etex.width = static_cast<int>(image.width);
      etex.height = static_cast<int>(image.height);
      etex.saved = true;
    }
  } catch (const std::exception &) {
    ++etex.failed;
  }
}

void add_world(std::map<std::string, Pair> &pairs, const std::string &key,
              const std::vector<std::uint8_t> &bytes) {
  if (key.empty())
    return;
  auto &pair = pairs[key];
  if (pair.world.empty())
    pair.world = bytes;
}

void harvest(const std::vector<std::uint8_t> &container, int depth,
             std::map<std::string, Pair> &pairs, EtexCount &etex,
             const fs::path &out_dir, int &lmp_fail, bool collect,
             const std::string &container_stem, const std::string &gob_stem,
             int &world_members) {
  std::vector<champions::Entry> entries;
  try {
    entries = readLmp(Bytes{container.data(), container.size()});
  } catch (const std::exception &) {
    ++lmp_fail;
    return;
  }
  if (entries.size() == 1 && hasExtension(entries[0].name, ".etex")) {
    const auto body = payload(Bytes{container.data(), container.size()}, entries[0]);
    note_etex(std::vector<std::uint8_t>(body.data, body.data + body.size), etex,
              out_dir);
  }
  for (const auto &entry : entries) {
    const auto body = payload(Bytes{container.data(), container.size()}, entry);
    std::vector<std::uint8_t> bytes(body.data, body.data + body.size);
    if (depth > 0 && hasExtension(entry.name, ".lmp")) {
      harvest(bytes, depth - 1, pairs, etex, out_dir, lmp_fail, collect,
              stem_key(entry.name), gob_stem, world_members);
      continue;
    }
    if (!collect ||
        (!hasExtension(entry.name, ".world") && !hasExtension(entry.name, ".tex")))
      continue;
    auto &pair = pairs[stem_key(entry.name)];
    if (hasExtension(entry.name, ".world")) {
      ++world_members;
      add_world(pairs, stem_key(entry.name), bytes);
      add_world(pairs, container_stem, bytes);
      add_world(pairs, gob_stem, bytes);
    } else if (pair.tex.empty()) {
      pair.tex = std::move(bytes);
    }
  }
}

// WORLD header +0x6c (the automap tile texture) is "a 0x40-byte header +
// pixel data" whose pixel format was not decoded (research-textures 391cf6b
// docs/formats/world.md, header +0x6c: U). Nothing here decodes it; the
// count line reports atlas=undocumented.
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: level-draw-measure <disc-root> <out-dir>\n";
    return 2;
  }
  const fs::path root = argv[1];
  const fs::path out_dir = argv[2];
  std::error_code error;
  fs::create_directories(out_dir, error);
  if (error)
    return 1;

  std::map<std::string, int> extensions;
  std::vector<fs::path> gob_paths;
  std::map<std::string, fs::path> tex_paths;
  int files = 0;
  int gob_files = 0;
  int world_members = 0;
  int tex_matched = 0;
  int gob_fail = 0;
  int lmp_fail = 0;
  int pairs_seen = 0;
  int pairs_refused = 0;
  int pairs_empty = 0;
  int pairs_over_cap = 0;
  EtexCount etex;
  bool have_level = false;
  bool have_env = false;
  int surfaces = 0;
  int triangles = 0;
  int env = 0;
  int alpha2 = 0;
  int alpha1 = 0;
  int alpha9 = 0;
  int alpha48 = 0;
  int layer0 = 0;
  int layer4 = 0;
  int dynamic = 0;
  int constant = 0;
  int cutout = 0;
  int automap_ok = 0;
  int automap_w = 0;
  int automap_h = 0;
  int automap_cells = 0;
  int automap_quads = 0;
  int omitted_models = 0;
  int omitted_emitters = 0;
  int omitted_lights = 0;

  for (fs::recursive_directory_iterator it(root, error), end; it != end && !error;
       it.increment(error)) {
    if (!it->is_regular_file())
      continue;
    ++files;
    const auto name = it->path().filename().string();
    const auto dot = name.find_last_of('.');
    const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
    std::string key = ext;
    for (char &c : key) {
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c - 'A' + 'a');
    }
    ++extensions[key];
    if (hasExtension(name, ".tex")) {
      auto &slot = tex_paths[stem_key(name)];
      if (slot.empty())
        slot = it->path();
    }
    if (!hasExtension(name, ".gob"))
      continue;
    ++gob_files;
    gob_paths.push_back(it->path());
  }
  if (error)
    return 1;

  for (const auto &gob_path : gob_paths) {
    const auto gob = read_file(gob_path);
    if (gob.empty()) {
      ++gob_fail;
      continue;
    }
    std::vector<champions::Entry> entries;
    try {
      entries = readGob(Bytes{gob.data(), gob.size()});
    } catch (const std::exception &) {
      ++gob_fail;
      continue;
    }
    std::map<std::string, Pair> pairs;
    for (const auto &entry : entries) {
      const auto body = payload(Bytes{gob.data(), gob.size()}, entry);
      std::vector<std::uint8_t> bytes(body.data, body.data + body.size);
      if (hasExtension(entry.name, ".lmp")) {
        harvest(bytes, 1, pairs, etex, out_dir, lmp_fail, !have_env,
                stem_key(entry.name), stem_key(gob_path.filename().string()),
                world_members);
        continue;
      }
      if (have_env ||
          (!hasExtension(entry.name, ".world") && !hasExtension(entry.name, ".tex")))
        continue;
      auto &pair = pairs[stem_key(entry.name)];
      if (hasExtension(entry.name, ".world")) {
        ++world_members;
        add_world(pairs, stem_key(entry.name), bytes);
        add_world(pairs, stem_key(gob_path.filename().string()), bytes);
      } else if (pair.tex.empty()) {
        pair.tex = std::move(bytes);
      }
    }
    if (have_env)
      continue;
    for (auto &item : pairs) {
      if (item.second.world.empty() || !item.second.tex.empty())
        continue;
      const auto found = tex_paths.find(item.first);
      if (found == tex_paths.end())
        continue;
      item.second.tex = read_file(found->second);
      if (!item.second.tex.empty())
        ++tex_matched;
    }
    for (auto &item : pairs) {
      if (item.second.world.empty() || item.second.tex.empty())
        continue;
      ++pairs_seen;
      try {
        const auto geometry =
            champions::presentation::rta::world_geometry::decode(item.second.world);
        champions::presentation::rta::level_texture::File textures(
            std::vector<std::uint8_t>(item.second.tex));
        const auto bound =
            champions::presentation::rta::level_texture::bind_world(
                geometry, textures, item.second.world);
        if (bound.empty()) {
          ++pairs_empty;
          continue;
        }
        if (bound.size() > 4096) {
          ++pairs_over_cap;
          continue;
        }
        int env_count = 0;
        for (const auto &surface : bound)
          if (surface.material.environment)
            ++env_count;
        // Keep the first readable level, then replace it with the first that
        // has an environment-mapped surface.
        if (have_level && env_count == 0)
          continue;
        write_file(out_dir / "world.bin", item.second.world.data(),
                   item.second.world.size());
        write_file(out_dir / "tex.bin", item.second.tex.data(),
                   item.second.tex.size());
        surfaces = static_cast<int>(bound.size());
        triangles = 0;
        env = 0;
        alpha2 = 0;
        alpha1 = 0;
        alpha9 = 0;
        alpha48 = 0;
        layer0 = 0;
        layer4 = 0;
        dynamic = 0;
        constant = 0;
        cutout = 0;
        omitted_models = static_cast<int>(geometry.omitted_models);
        omitted_emitters = static_cast<int>(geometry.omitted_emitters);
        omitted_lights = static_cast<int>(geometry.omitted_lights);
        for (const auto &surface : bound) {
          triangles += static_cast<int>(surface.positions.size() / 3);
          if (surface.material.environment)
            ++env;
          if (surface.material.alpha1 == 2)
            ++alpha2;
          else if (surface.material.alpha1 == 1)
            ++alpha1;
          else if (surface.material.alpha1 == 9)
            ++alpha9;
          else if (surface.material.alpha1 == 0x48)
            ++alpha48;
          if (surface.material.sort_layer == 4)
            ++layer4;
          else
            ++layer0;
          if (surface.material.dynamic_light)
            ++dynamic;
          else
            ++constant;
          if (surface.material.cutout())
            ++cutout;
        }
        try {
          const auto grid =
              champions::presentation::rta::level_texture::read_automap_grid(
                  item.second.world);
          automap_ok = 1;
          automap_w = grid.width;
          automap_h = grid.height;
          automap_cells = static_cast<int>(grid.cells.size());
          if (grid.width >= 2 && grid.height >= 2)
            automap_quads = (grid.width - 1) * (grid.height - 1);
        } catch (const std::exception &) {
          automap_ok = 0;
          automap_w = 0;
          automap_h = 0;
          automap_cells = 0;
          automap_quads = 0;
        }
        have_level = true;
        have_env = env_count > 0;
        if (have_env)
          break;
      } catch (const std::exception &) {
        ++pairs_refused;
      }
    }
  }

  std::cout << "files=" << files << " gob_files=" << gob_files
            << " gob_fail=" << gob_fail << " lmp_fail=" << lmp_fail
            << " world_members=" << world_members
            << " tex_matched=" << tex_matched << " pairs_seen=" << pairs_seen
            << " pairs_refused=" << pairs_refused
            << " pairs_empty=" << pairs_empty
            << " pairs_over_cap=" << pairs_over_cap
            << " saved=" << (have_level ? 1 : 0)
            << " saved_env=" << (have_env ? 1 : 0)
            << " surfaces=" << surfaces << " triangles=" << triangles
            << " env=" << env << " alpha2=" << alpha2 << " alpha1=" << alpha1
            << " alpha9=" << alpha9 << " alpha48=" << alpha48
            << " layer0=" << layer0 << " layer4=" << layer4
            << " dynamic=" << dynamic << " constant=" << constant
            << " cutout=" << cutout << " automap_ok=" << automap_ok
            << " automap_w=" << automap_w << " automap_h=" << automap_h
            << " automap_cells=" << automap_cells
            << " automap_quads=" << automap_quads << " atlas=undocumented"
            << " etex_lumps=" << etex.lumps << " etex_decoded=" << etex.decoded
            << " etex_failed=" << etex.failed << " etex_w=" << etex.width
            << " etex_h=" << etex.height << " omitted_models=" << omitted_models
            << " omitted_emitters=" << omitted_emitters
            << " omitted_lights=" << omitted_lights << "\n";
  std::cout << "extensions";
  for (const auto &item : extensions)
    std::cout << " " << (item.first.empty() ? "none" : item.first) << "="
              << item.second;
  std::cout << "\n";
  return have_level ? 0 : 1;
}
