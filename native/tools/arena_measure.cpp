// Counts-only arena task on the three cold arena indices.
// Reads an archive directory and does not write it. Prints no archive names,
// positions, or property strings. Not a CTest.
#include "champions/arena_modes.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/level_flow.hpp"
#include "champions/objects.hpp"
#include "champions/terrain_file.hpp"
#include "champions/world_queries.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::arena_modes;
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
    in.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
  return bool(in);
}
std::uint32_t u32(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 | std::uint32_t(b[at + 2]) << 16 |
         std::uint32_t(b[at + 3]) << 24;
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
bool sequential_lumps(const std::vector<std::uint8_t> &data, std::vector<Lump> &out) {
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
    out.push_back({std::move(name),
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
                   std::vector<std::uint8_t>(blob.begin() + data_at, blob.begin() + data_at + size)});
  }
  return true;
}

struct Row {
  unsigned index = 0;
  unsigned starts = 0;
  unsigned alternates = 0;
  unsigned count_byte = 0;
  int entry0_is_start = 0;
  int wave = 0;
  int lives = 0;
  int intro_ticks = 0;
  int latched = 0;
  int wave_searches = 0;
  int wave_ok = 0;
  int wave_retry = 0;
  int wave_floor = 0;
  int offline_div0 = 0;
  int offline_search = 0;
  int offline_facing0 = 0;
  int offline_floor = 0;
  int failed = 0;
};

void one(unsigned index, const std::vector<std::uint8_t> &world_bytes,
         const std::vector<std::uint8_t> *objects_bytes, Row &row) {
  row.index = index;
  world_queries::Level level(terrain_file::decode(world_bytes));
  std::vector<Record> records;
  if (objects_bytes)
    records = parse_objects({objects_bytes->data(), objects_bytes->size()});
  Match match;
  SpawnPoint start{};
  bool saw_start = false;
  for (const auto &record : records) {
    const SpawnPoint point{record.position[0], record.position[1], record.position[2], record.angle};
    if (record.cls == "ArenaPlayerStart") {
      ++row.starts;
      start = point;
      saw_start = true;
      match.register_spawn(0, point);
    } else if (record.cls == "ArenaPlayerAlternate") {
      ++row.alternates;
      match.register_spawn(-1, point);
    }
  }
  row.count_byte = match.spawn_count;
  if (saw_start && !match.points.empty() && match.points[0].x == start.x &&
      match.points[0].y == start.y && match.points[0].z == start.z &&
      match.points[0].angle == start.angle)
    row.entry0_is_start = 1;
  match.mode_a = true;
  match.mode_b = true;
  match.num_players = 1;
  match.active_count = 1;
  match.init();
  match.begin();
  while (s16(match.intro) > 0 && row.intro_ticks < 400) {
    match.tick();
    ++row.intro_ticks;
  }
  if (s16(match.intro) == 0) {
    row.latched = match.tick().intro_latch ? 1 : 0;
  }
  row.wave = s16(match.wave);
  row.lives = s16(match.slots[0].lives);
  auto blocked = [&](Raw radius, const Vec3 &pos) {
    return world_queries::terrain_test_flag1(level, radius, pos);
  };
  auto height = [&](Raw radius, const Vec3 &pos) {
    return world_queries::terrain_height(level, radius, pos);
  };
  objects::World world(objects::CallbackPolicy::ReferenceBase);
  world.shared = SharedRandom(0);
  auto search = [&](const Vec3 &centre, int spread, int radius) {
    ++row.wave_searches;
    if (spread == 0xf0)
      ++row.wave_retry;
    const auto found = world.find_free_spot(centre, spread, radius, false, blocked, height);
    if (found.second)
      ++row.wave_ok;
    return Spot{found.first, found.second};
  };
  if (!match.points.empty()) {
    const int visible = match.spawn_count == 0 ? 1 : match.spawn_count;
    const auto placed = pick_spawn(1, 0, match.points.data(), visible, match.seed, true, false, false,
                                   Vec3{}, match.active_count, search, Vec3{});
    if (player_ctor_z(placed.pos, PLACEMENT_RADIUS, height) != world_queries::NO_FLOOR)
      row.wave_floor = 1;
  } else {
    row.failed = 1;
  }
  objects::World offline_world(objects::CallbackPolicy::ReferenceBase);
  offline_world.shared = SharedRandom(0);
  auto offline_search = [&](const Vec3 &centre, int spread, int radius) {
    ++row.offline_search;
    const auto found = offline_world.find_free_spot(centre, spread, radius, false, blocked, height);
    return Spot{found.first, found.second};
  };
  try {
    const Vec3 origin{};
    const auto placed = pick_spawn(0, 0, match.points.data(), match.spawn_count, match.seed, true, false,
                                   false, origin, match.active_count, offline_search, origin);
    row.offline_facing0 = placed.facing == 0 ? 1 : 0;
    if (player_ctor_z(placed.pos, PLACEMENT_RADIUS, height) != world_queries::NO_FLOOR)
      row.offline_floor = 1;
  } catch (const DivideByZero &) {
    row.offline_div0 = 1;
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: champions-arena-measure <archive-directory>\n";
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
  for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
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
  unsigned paired = 0, read_failures = 0, lump_failures = 0, ran = 0;
  const unsigned want[] = {69, 70, 71};
  for (const auto &path : gobs) {
    const auto stem = path.stem().string();
    if (!tex.count(ascii_fold(stem, true)))
      continue;
    const auto index = paired++;
    if (index != want[0] && index != want[1] && index != want[2])
      continue;
    Row row;
    std::vector<std::uint8_t> file;
    if (!read_file(path, file)) {
      ++read_failures;
      std::cout << "row " << index << " failed 1\n";
      continue;
    }
    std::vector<Lump> lumps;
    if (!sequential_lumps(file, lumps)) {
      ++lump_failures;
      std::cout << "row " << index << " failed 1\n";
      continue;
    }
    const auto want_lump = ascii_fold(stem, false) + ".lmp";
    const Lump *level_lump = nullptr;
    for (const auto &lump : lumps)
      if (ascii_fold(lump.name, false) == want_lump) {
        level_lump = &lump;
        break;
      }
    if (!level_lump) {
      ++lump_failures;
      std::cout << "row " << index << " failed 1\n";
      continue;
    }
    std::vector<Lump> entries;
    if (!lmp_entries(level_lump->bytes, entries)) {
      ++lump_failures;
      std::cout << "row " << index << " failed 1\n";
      continue;
    }
    std::vector<std::string> names;
    for (const auto &entry : entries)
      names.push_back(entry.name);
    const auto world_name = first_entry_with_extension(names, "world");
    const Lump *world_lump = nullptr;
    const Lump *objects_lump = nullptr;
    for (const auto &entry : entries) {
      if (world_name && !world_lump && entry.name == *world_name)
        world_lump = &entry;
      if (!objects_lump && entry.name == "objects.ob")
        objects_lump = &entry;
    }
    if (!world_lump) {
      ++lump_failures;
      std::cout << "row " << index << " failed 1\n";
      continue;
    }
    try {
      one(index, world_lump->bytes, objects_lump ? &objects_lump->bytes : nullptr, row);
    } catch (const std::exception &) {
      row.failed = 1;
    }
    ++ran;
    std::cout << "row " << row.index << " starts " << row.starts << " alts " << row.alternates
              << " count " << row.count_byte << " entry0_start " << row.entry0_is_start
              << " wave " << row.wave << " lives " << row.lives
              << " intro " << row.intro_ticks << " latched " << row.latched << " searches "
              << row.wave_searches << " ok " << row.wave_ok << " retry " << row.wave_retry
              << " floor " << row.wave_floor << " offline_div0 " << row.offline_div0
              << " offline_search " << row.offline_search << " offline_facing0 " << row.offline_facing0
              << " offline_floor " << row.offline_floor << " failed " << row.failed << '\n';
  }
  std::cout << "paired " << paired << "\nran " << ran << "\nread_failures " << read_failures
            << "\nlump_failures " << lump_failures << '\n';
  return ran == 3 && read_failures == 0 && lump_failures == 0 ? 0 : 1;
}
