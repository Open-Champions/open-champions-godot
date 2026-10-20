// Counts-only check of story_start_ground on a paired archive directory.
// Reads the directory and does not write it. Prints no archive names, positions,
// or property strings. Not a CTest: the private disc stays outside the suite.
#include "champions/core_arithmetic.hpp"
#include "champions/level_flow.hpp"
#include "champions/objects.hpp"
#include "champions/terrain_file.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::level_flow;
namespace fs = std::filesystem;

std::string ascii_fold(std::string s, bool upper) {
  for (auto &c : s) {
    if (upper && c >= 'a' && c <= 'z')
      c = char(c - 'a' + 'A');
    if (!upper && c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  }
  return s;
}
bool read_file(const fs::path &path, std::vector<std::uint8_t> &out) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  in.seekg(0, std::ios::end);
  const auto n = in.tellg();
  if (n < 0)
    return false;
  in.seekg(0);
  out.resize(static_cast<std::size_t>(n));
  if (!out.empty())
    in.read(reinterpret_cast<char *>(out.data()),
            static_cast<std::streamsize>(out.size()));
  return bool(in);
}
std::uint32_t u32(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 |
         std::uint32_t(b[at + 2]) << 16 | std::uint32_t(b[at + 3]) << 24;
}
std::uint16_t u16(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint16_t(b[at] | (b[at + 1] << 8));
}
std::string cstr(const std::vector<std::uint8_t> &b, std::size_t at) {
  if (at >= b.size())
    return {};
  auto end = at;
  while (end < b.size() && b[end])
    ++end;
  return std::string(reinterpret_cast<const char *>(b.data() + at), end - at);
}
struct Lump {
  std::string name;
  std::vector<std::uint8_t> bytes;
};
// Sequential payloads from 0x800. Directory offsets are ignored, and the
// gaydark lump rename is not applied: the census matches the lump's own name.
bool sequential_lumps(const std::vector<std::uint8_t> &data,
                      std::vector<Lump> &out) {
  if (data.size() < 0x800)
    return false;
  std::size_t pos = 0x800;
  for (std::size_t i = 0; i + 0x28 <= 0x800; i += 0x28) {
    if (!data[i])
      return true;
    std::size_t end = i;
    while (end < i + 32 && data[end])
      ++end;
    std::string name(reinterpret_cast<const char *>(data.data() + i), end - i);
    const auto size = u32(data, i + 0x24);
    if (pos > data.size() || size > data.size() - pos)
      return false;
    out.push_back(
        {std::move(name),
         std::vector<std::uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(pos),
                                   data.begin() + static_cast<std::ptrdiff_t>(pos + size))});
    const auto padded = std::uint64_t(pos) + size + 0x7ff;
    if (padded < pos)
      return false;
    pos = static_cast<std::size_t>(padded & ~std::uint64_t(0x7ff));
  }
  return false;
}
bool lmp_entries(const std::vector<std::uint8_t> &blob, std::vector<Lump> &out) {
  if (blob.size() < 4)
    return false;
  const unsigned count = u16(blob, 0);
  if (4ull + std::uint64_t(count) * 12 > blob.size())
    return false;
  for (unsigned i = 0; i < count; ++i) {
    const auto rec = 4 + i * 12;
    const auto name_at = u32(blob, rec);
    const auto data_at = u32(blob, rec + 4);
    const auto size = u32(blob, rec + 8);
    if (std::uint64_t(data_at) + size > blob.size())
      return false;
    out.push_back({cstr(blob, name_at),
                   std::vector<std::uint8_t>(
                       blob.begin() + data_at, blob.begin() + data_at + size)});
  }
  return true;
}
enum class Census { Skip, Bad, Open };
Census chunk_census(const std::vector<std::uint8_t> &world) {
  if (world.size() < 0x54)
    return Census::Bad;
  const auto chunks = u32(world, 0);
  const auto table = u32(world, 0x24);
  if (u32(world, 0x50) != 0)
    return Census::Skip;
  for (std::uint32_t i = 0; i < chunks; ++i) {
    const auto at = std::uint64_t(table) + std::uint64_t(i) * 0x3c + 0x24;
    if (at + 4 > world.size())
      return Census::Bad;
    const auto flags = u32(world, static_cast<std::size_t>(at));
    if ((flags & 0x80) || (flags & 0x20))
      return Census::Skip;
  }
  return Census::Open;
}
const Vec3 kOrigin{};
Raw radius4() { return 0x40800000u; }
bool floor_at(world_queries::Level &level, Raw radius, const Vec3 &pos) {
  return world_queries::terrain_height(level, radius, pos) !=
         world_queries::NO_FLOOR;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: champions-level-spawn-measure <archive-directory>\n";
    return 2;
  }
  const fs::path root(argv[1]);
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    std::cerr << "archive directory is not readable\n";
    return 1;
  }
  ScopedFpu ps2{Fpu{Fpu::Model::Ps2}};
  std::set<std::string> tex;
  std::vector<fs::path> gobs;
  for (fs::directory_iterator it(root, ec), end; !ec && it != end;
       it.increment(ec)) {
    if (!it->is_regular_file())
      continue;
    const auto name = it->path().filename().string();
    const auto dot = name.rfind('.');
    if (dot == std::string::npos)
      continue;
    const auto ext = ascii_fold(name.substr(dot), true);
    const auto stem = name.substr(0, dot);
    if (ext == ".TEX")
      tex.insert(ascii_fold(stem, true));
    else if (ext == ".GOB")
      gobs.push_back(it->path());
  }
  if (ec) {
    std::cerr << "archive directory is not readable\n";
    return 1;
  }
  std::sort(gobs.begin(), gobs.end());
  unsigned paired = 0, read_failures = 0, lump_failures = 0, decode_failures = 0,
           geometry_bad = 0, geometry_skipped = 0, origin_test = 0,
           story_ground = 0, origin_r4_floor = 0, origin_r12_floor = 0,
           marker_floor = 0, search_floor = 0, search_cases = 0, arena_n = 0,
           arena_floor = 0, scenario_miss = 0, measure_failures = 0;
  objects::World world(objects::CallbackPolicy::ReferenceBase);
  for (const auto &path : gobs) {
    const auto stem = path.stem().string();
    if (!tex.count(ascii_fold(stem, true)))
      continue;
    ++paired;
    std::vector<std::uint8_t> file;
    if (!read_file(path, file)) {
      ++read_failures;
      continue;
    }
    std::vector<Lump> lumps;
    if (!sequential_lumps(file, lumps)) {
      ++lump_failures;
      continue;
    }
    const auto want = ascii_fold(stem, false) + ".lmp";
    const Lump *level_lump = nullptr;
    for (const auto &lump : lumps)
      if (ascii_fold(lump.name, false) == want) {
        level_lump = &lump;
        break;
      }
    if (!level_lump)
      continue;
    std::vector<Lump> entries;
    if (!lmp_entries(level_lump->bytes, entries)) {
      ++lump_failures;
      continue;
    }
    std::vector<std::string> names;
    names.reserve(entries.size());
    for (const auto &e : entries)
      names.push_back(e.name);
    const auto world_name = first_entry_with_extension(names, "world");
    if (!world_name)
      continue;
    const Lump *world_lump = nullptr;
    const Lump *objects_lump = nullptr;
    for (const auto &e : entries) {
      if (!world_lump && e.name == *world_name)
        world_lump = &e;
      if (!objects_lump && e.name == "objects.ob")
        objects_lump = &e;
    }
    if (!world_lump)
      continue;
    const auto census = chunk_census(world_lump->bytes);
    if (census == Census::Bad) {
      ++geometry_bad;
      continue;
    }
    if (census == Census::Skip) {
      ++geometry_skipped;
      continue;
    }
    world_queries::Level level;
    try {
      level = world_queries::Level(terrain_file::decode(world_lump->bytes));
    } catch (const std::exception &) {
      ++decode_failures;
      continue;
    }
    if (world_queries::terrain_height_below(level, radius4(), kOrigin) !=
        world_queries::NO_FLOOR)
      continue;
    const auto index = origin_test++;
    try {
      std::vector<Record> records;
      if (objects_lump)
        records = parse_objects(
            {objects_lump->bytes.data(), objects_lump->bytes.size()});
      SpawnState marker_state;
      run_markers(
          marker_state, records, "", "", false,
          [](const Vec3 &centre, Raw, Raw, bool) { return centre; },
          []() -> std::uint16_t { return 0; }, false, false, 1);
      const Vec3 marker = marker_state.pos[0];
      const int r4 = floor_at(level, radius4(), kOrigin) ? 1 : 0;
      const int r12 = floor_at(level, PLACEMENT_RADIUS, kOrigin) ? 1 : 0;
      const int marked = floor_at(level, PLACEMENT_RADIUS, marker) ? 1 : 0;
      const auto story = story_start_ground({}, records, level);
      auto blocked = [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_test_flag1(level, radius, pos);
      };
      auto height = [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_height(level, radius, pos);
      };
      int found_floor = 0;
      for (Raw seed : {Raw{0}, Raw{20973}}) {
        world.shared = SharedRandom(seed);
        const auto found =
            world.find_free_spot(marker, 36, 24, false, blocked, height);
        if (floor_at(level, PLACEMENT_RADIUS, found.first))
          ++found_floor;
      }
      const Record *arena = nullptr;
      for (const auto &record : records)
        if (record.cls == "ArenaPlayerStart") {
          arena = &record;
          break;
        }
      int arena_here = 0, arena_ok = 0;
      if (arena) {
        arena_here = 1;
        world.shared = SharedRandom(20973);
        const auto placed = arena_initial_place(
            arena->position, 0, 1, false,
            [&](const Vec3 &centre, int spread, int radius, bool any) {
              return world.find_free_spot(centre, spread, radius, any, blocked,
                                          height);
            },
            false, arena->angle);
        arena_ok = floor_at(level, PLACEMENT_RADIUS, placed.pos) ? 1 : 0;
      }
      std::set<std::string> froms, names;
      bool portal = false;
      for (const auto &record : records) {
        auto cls = ascii_fold(record.cls, false);
        if (cls == "player1") {
          if (auto from = record.prop("from"))
            froms.insert(*from);
          if (auto name = record.prop("name"))
            names.insert(*name);
        }
        if (cls == "portal" || cls == "portalhurt")
          portal = true;
      }
      std::vector<std::tuple<std::string, std::string, bool, bool>> plans;
      plans.emplace_back("", "", false, false);
      for (const auto &from : froms)
        plans.emplace_back(from, "", false, false);
      for (const auto &name : names)
        plans.emplace_back("", name, false, false);
      plans.emplace_back("", "", true, false);
      if (portal)
        plans.emplace_back("", "", false, true);
      bool missed = false;
      for (const auto &plan : plans) {
        SpawnState spawn;
        run_markers(
            spawn, records, std::get<0>(plan), std::get<1>(plan),
            std::get<2>(plan),
            [](const Vec3 &centre, Raw, Raw, bool) { return centre; },
            []() -> std::uint16_t { return 0; }, false, std::get<3>(plan), 1);
        if (!floor_at(level, PLACEMENT_RADIUS, spawn.pos[0]))
          missed = true;
      }
      origin_r4_floor += unsigned(r4);
      origin_r12_floor += unsigned(r12);
      marker_floor += unsigned(marked);
      story_ground += story.ground ? 1 : 0;
      search_cases += 2;
      search_floor += unsigned(found_floor);
      arena_n += unsigned(arena_here);
      arena_floor += unsigned(arena_ok);
      scenario_miss += missed ? 1 : 0;
      std::cout << "level " << index << ' ' << (story.ground ? 1 : 0) << '\n';
    } catch (const std::exception &) {
      ++measure_failures;
      std::cout << "level " << index << " 0\n";
    }
  }
  std::cout << "paired " << paired << '\n'
            << "origin_test " << origin_test << '\n'
            << "story_ground " << story_ground << '\n'
            << "origin_r4_floor " << origin_r4_floor << '\n'
            << "origin_r12_floor " << origin_r12_floor << '\n'
            << "marker_r12_floor " << marker_floor << '\n'
            << "search_snap " << search_floor << '/' << search_cases << '\n'
            << "arena " << arena_n << " floor " << arena_floor << '\n'
            << "scenario_misses " << scenario_miss << '\n'
            << "read_failures " << read_failures << '\n'
            << "lump_failures " << lump_failures << '\n'
            << "decode_failures " << decode_failures << '\n'
            << "geometry_bad " << geometry_bad << '\n'
            << "geometry_skipped " << geometry_skipped << '\n'
            << "measure_failures " << measure_failures << '\n';
  return measure_failures || read_failures || lump_failures || decode_failures ||
                 geometry_bad
             ? 1
             : 0;
}
