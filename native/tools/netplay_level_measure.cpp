// Counts-only lockstep measure. Reads a private disc and prints no names,
// paths, coordinates or file bytes. Not a CTest.
#include "champions/archive.hpp"
#include "netplay_level_lmp.hpp"
#include "champions/level_flow.hpp"
#include "champions/netplay.hpp"
#include "champions/terrain_file.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace N = R::netplay;
namespace W = R::world_queries;
namespace F = R::level_flow;
namespace fs = std::filesystem;
using Raw = R::Raw;

namespace {
constexpr std::uint64_t FILE_CAP = 128ull * 1024ull * 1024ull;
constexpr Raw PLAYER_RADIUS = 0x41800000u;
constexpr Raw CREATURE_RADIUS = 0x41400000u;
constexpr int SCALES[] = {24, 32, 40, 48, 64, 80, 96, 128};

R::Raw raw_int(int value) {
  return R::from_int(static_cast<std::uint32_t>(value));
}

bool ascii_eq(const std::string &name, const char *want) {
  std::size_t n = 0;
  while (want[n])
    ++n;
  if (name.size() != n)
    return false;
  for (std::size_t i = 0; i < n; ++i) {
    unsigned char a = static_cast<unsigned char>(name[i]);
    unsigned char b = static_cast<unsigned char>(want[i]);
    if (a >= 'A' && a <= 'Z')
      a = static_cast<unsigned char>(a + 32);
    if (b >= 'A' && b <= 'Z')
      b = static_cast<unsigned char>(b + 32);
    if (a != b)
      return false;
  }
  return true;
}

bool ext_is(const fs::path &path, const char *want) {
  return ascii_eq(path.extension().string(), want);
}

bool read_file(const fs::path &path, std::vector<std::uint8_t> &out) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > FILE_CAP)
    return false;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  out.resize(static_cast<std::size_t>(size));
  if (!out.empty())
    in.read(reinterpret_cast<char *>(out.data()),
            static_cast<std::streamsize>(out.size()));
  return bool(in);
}

// Same walk as the netplay CTest: full deflection past pad_apply's radial
// dead zone from frame 16, reversing every 60 frames, follow camera (flag
// clear). No attack: these runs carry no creature.
N::Sample script(int slot, std::uint32_t frame) {
  N::Sample sample;
  sample.camera_flag = false;
  const bool early = frame < 16;
  const bool outward = ((frame / 60) % 2) == 0;
  const std::uint8_t go = early ? 128 : (outward ? 255 : 0);
  const std::uint8_t back = early ? 128 : (outward ? 0 : 255);
  if (slot < 2)
    sample.sticks[0] = slot == 0 ? go : back;
  else
    sample.sticks[1] = slot == 2 ? go : back;
  return sample;
}

void drive(N::Session &session, std::size_t ticks) {
  unsigned stall = 0;
  std::size_t reported = 0;
  while (session.sim_ticks() < ticks || !session.aligned()) {
    const auto before = session.sim_ticks();
    std::vector<N::Sample> samples;
    const auto frame = static_cast<std::uint32_t>(session.render_steps());
    for (int slot = 0; slot < session.peers(); ++slot)
      samples.push_back(script(slot, frame));
    session.step(samples);
    if (session.sim_ticks() == before) {
      if (++stall >= 240)
        throw std::runtime_error("lockstep stalled");
    } else {
      stall = 0;
    }
    if (session.sim_ticks() >= reported + 1000) {
      reported = session.sim_ticks();
      std::cerr << "peers=" << session.peers()
                << " sim_ticks=" << session.sim_ticks() << "\n";
    }
  }
}

bool standable(W::Level &level, Raw radius, Raw x, Raw y, Raw &z) {
  W::Vec3 probe{x, y, 0};
  const auto height = W::terrain_height(level, radius, probe);
  if (height == W::NO_FLOOR)
    return false;
  probe[2] = height;
  if (W::terrain_height_below(level, radius, probe) == W::NO_FLOOR)
    return false;
  z = height;
  return true;
}

bool used_xy(const std::vector<std::array<Raw, 2>> &used, Raw x, Raw y) {
  for (const auto &point : used)
    if (point[0] == x && point[1] == y)
      return true;
  return false;
}

std::optional<N::Placement> place_four(W::Level &level,
                                       const F::StoryGround &story) {
  if (!story.ground)
    return std::nullopt;
  Raw z = 0;
  if (!standable(level, PLAYER_RADIUS, story.position[0], story.position[1], z))
    return std::nullopt;
  N::Placement placement;
  placement.player_present[0] = true;
  placement.players[0] = {story.position[0], story.position[1], z};
  std::vector<std::array<Raw, 2>> used{{story.position[0], story.position[1]}};
  const int dirs[8][2] = {{1, 0},  {-1, 0},  {0, 1},  {0, -1},
                          {1, 1},  {-1, 1},  {1, -1}, {-1, -1}};
  int filled = 1;
  for (int scale : SCALES) {
    for (const auto &dir : dirs) {
      if (filled >= 4)
        break;
      const Raw x = R::add(story.position[0], raw_int(dir[0] * scale));
      const Raw y = R::add(story.position[1], raw_int(dir[1] * scale));
      if (used_xy(used, x, y))
        continue;
      Raw height = 0;
      if (!standable(level, PLAYER_RADIUS, x, y, height))
        continue;
      placement.player_present[std::size_t(filled)] = true;
      placement.players[std::size_t(filled)] = {x, y, height};
      used.push_back({x, y});
      ++filled;
    }
  }
  if (filled < 4)
    return std::nullopt;
  placement.creature_present = false;
  for (int scale : SCALES) {
    const Raw x = R::add(story.position[0], raw_int(scale));
    const Raw y = R::add(story.position[1], raw_int(scale));
    if (used_xy(used, x, y))
      continue;
    Raw height = 0;
    if (!standable(level, CREATURE_RADIUS, x, y, height))
      continue;
    placement.creature = {x, y, height};
    placement.creature_present = true;
    break;
  }
  return placement;
}

N::Placement for_peers(N::Placement placement, int peers) {
  for (int slot = 0; slot < 4; ++slot)
    placement.player_present[std::size_t(slot)] = slot < peers;
  return placement;
}

struct Proof {
  int peers = 0;
  std::size_t ticks = 0;
  std::size_t trace_bytes = 0;
  int mismatches = 0;
  int save_load = 0;
  int synced = 0;
  std::size_t moved_ticks = 0;
};

// Ticks on which some player position word (trace words 6..17) changed.
std::size_t moved_ticks(const std::vector<std::uint8_t> &trace) {
  const std::size_t ticks = trace.size() / N::TRACE_RECORD;
  std::size_t moved = 0;
  for (std::size_t tick = 1; tick < ticks; ++tick) {
    const auto *now = trace.data() + tick * N::TRACE_RECORD + 24;
    if (!std::equal(now, now + 48, now - N::TRACE_RECORD))
      ++moved;
  }
  return moved;
}

Proof prove(const W::Snapshot &snapshot, const N::Placement &placement,
            int peers, std::size_t ticks) {
  Proof proof;
  proof.peers = peers;
  auto subset = for_peers(placement, peers);
  // These players walk the whole run. A walking player beside an armed
  // Skelight reaches the unported world_findFreeSpot, so no creature.
  subset.creature_present = false;
  N::Session live(peers, snapshot, subset);
  drive(live, ticks / 2);
  const auto saved = live.save();
  auto resumed = N::Session::load(saved, snapshot);
  const auto at_save = live.trace_bytes(0);
  if (resumed.trace_bytes(0) != at_save)
    ++proof.mismatches;
  else
    proof.save_load = 1;
  drive(live, ticks);
  drive(resumed, ticks);
  const auto done = live.trace_bytes(0);
  proof.ticks = live.sim_ticks();
  proof.trace_bytes = done.size();
  proof.moved_ticks = moved_ticks(done);
  if (resumed.trace_bytes(0) != done)
    ++proof.mismatches;
  for (int slot = 0; slot < peers; ++slot) {
    if (live.trace_bytes(slot) != done)
      ++proof.mismatches;
    if (resumed.trace_bytes(slot) != done)
      ++proof.mismatches;
  }
  proof.synced = live.synced() && resumed.synced() ? 1 : 0;
  if (!proof.synced)
    ++proof.mismatches;
  return proof;
}

int probe_ms(const W::Snapshot &snapshot, const N::Placement &placement) {
  const auto start = std::chrono::steady_clock::now();
  N::Session session(2, snapshot, for_peers(placement, 2));
  drive(session, 20);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  return int(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                 .count());
}

struct Candidate {
  W::LevelData data;
  N::Placement placement;
  int elements = 0;
  int probe = 0;
  bool ok = false;
};

bool better(const Candidate &best, int elements, int probe) {
  if (!best.ok)
    return true;
  const bool small = elements <= 2000;
  const bool best_small = best.elements <= 2000;
  if (small != best_small)
    return small;
  return probe < best.probe;
}

// argv[1], else the private-validation copy beside the worktrees.
fs::path disc_root(int argc, char **argv) {
  std::vector<fs::path> candidates;
  if (argc > 1)
    candidates.emplace_back(argv[1]);
  candidates.emplace_back("../../private-validation/rta-disc");
  candidates.emplace_back("../../../private-validation/rta-disc");
  for (const auto &path : candidates) {
    std::error_code ec;
    if (fs::is_directory(path, ec))
      return path;
  }
  return {};
}
} // namespace

int main(int argc, char **argv) {
  const auto started = std::chrono::steady_clock::now();
  unsigned disc_files = 0, archives_seen = 0, worlds_decoded = 0,
           oversized = 0, placed = 0;
  champions::netplay_level::LmpRefusals lmp_refused;
  const auto root = disc_root(argc, argv);
  if (root.empty()) {
    std::cout << "disc_files=0\n";
    return 2;
  }
  R::ScopedFpu fixed{R::Fpu{R::Fpu::Model::Ps2, false, false}};
  Candidate best;
  std::error_code walk_ec;
  for (fs::recursive_directory_iterator
           it(root, fs::directory_options::skip_permission_denied, walk_ec),
       end;
       !walk_ec && it != end; it.increment(walk_ec)) {
    std::error_code ec;
    if (!it->is_regular_file(ec))
      continue;
    ++disc_files;
    if (!ext_is(it->path(), ".gob"))
      continue;
    std::error_code size_ec;
    if (fs::file_size(it->path(), size_ec) > FILE_CAP) {
      ++oversized;
      continue;
    }
    std::vector<std::uint8_t> file;
    if (!read_file(it->path(), file))
      continue;
    std::vector<champions::Entry> entries;
    try {
      entries = F::gob_entries({file.data(), file.size()});
    } catch (const std::exception &) {
      continue;
    }
    ++archives_seen;
    for (const auto &entry : entries) {
      if (!champions::hasExtension(entry.name, ".lmp"))
        continue;
      if (std::uint64_t(entry.offset) + entry.size > file.size())
        continue;
      std::vector<std::uint8_t> world, objects;
      if (!champions::netplay_level::lmp_world(file.data() + entry.offset,
                                                entry.size, world, objects,
                                                lmp_refused))
        continue;
      W::LevelData decoded;
      try {
        decoded = R::terrain_file::decode(world);
      } catch (const std::exception &) {
        continue;
      }
      ++worlds_decoded;
      const int elements = int(decoded.elements.size());
      if (elements > 8000 || elements == 0)
        continue;
      try {
        W::Level level(decoded);
        std::vector<F::Record> records;
        if (!objects.empty())
          records = F::parse_objects({objects.data(), objects.size()});
        const auto story = F::story_start_ground({}, records, level);
        auto placement = place_four(level, story);
        if (!placement)
          continue;
        ++placed;
        W::Snapshot snapshot{decoded, {}};
        int ms = 0;
        try {
          ms = probe_ms(snapshot, *placement);
        } catch (const std::exception &) {
          continue;
        }
        if (!better(best, elements, ms))
          continue;
        best.data = std::move(decoded);
        best.placement = *placement;
        best.elements = elements;
        best.probe = ms;
        best.ok = true;
      } catch (const std::exception &) {
        continue;
      }
    }
  }
  std::vector<Proof> proofs;
  std::string failure;
  if (best.ok) {
    W::Snapshot snapshot{best.data, {}};
    try {
      // Full combat retains 4 list walks plus one spell iterator per player
      // and refuses at 65536. 3 peers refuse after 9362 and 4 after 8192,
      // so those proofs stop under the cap. 2 peers reach 10000.
      const std::pair<int, std::size_t> runs[] = {{2, 10000}, {3, 9000}, {4, 8000}};
      for (const auto &run : runs)
        proofs.push_back(prove(snapshot, best.placement, run.first, run.second));
    } catch (const std::exception &error) {
      const std::string text = error.what();
      failure = text.find('/') == std::string::npos ? text : "level refused";
    }
  }
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - started)
                           .count();
  if (proofs.empty()) {
    std::cout << "disc_files=" << disc_files
              << " archives_seen=" << archives_seen
              << " worlds_decoded=" << worlds_decoded
              << " oversized=" << oversized << " placed=" << placed
              << " lmp_refused=" << lmp_refused.count
              << " elements=" << best.elements << " peers=0 sim_ticks=0"
              << " trace_bytes=0 mismatches=0 save_load=0 synced=0 seconds="
              << seconds;
    if (!failure.empty())
      std::cout << " fail=" << failure;
    std::cout << "\n";
    return 1;
  }
  bool pass = failure.empty() && proofs.size() == 3;
  for (const auto &proof : proofs) {
    std::cout << "disc_files=" << disc_files
              << " archives_seen=" << archives_seen
              << " worlds_decoded=" << worlds_decoded
              << " lmp_refused=" << lmp_refused.count
              << " elements=" << best.elements << " peers=" << proof.peers
              << " sim_ticks=" << proof.ticks
              << " trace_bytes=" << proof.trace_bytes
              << " mismatches=" << proof.mismatches
              << " save_load=" << proof.save_load
              << " synced=" << proof.synced
              << " moved_ticks=" << proof.moved_ticks << "\n";
    const std::size_t need = proof.peers == 2 ? 10000 : proof.peers == 3 ? 9000 : 8000;
    if (proof.ticks < need || proof.trace_bytes < need * N::TRACE_RECORD ||
        proof.moved_ticks == 0 ||
        proof.mismatches != 0 || proof.save_load != 1 || proof.synced != 1)
      pass = false;
  }
  std::cout << "seconds=" << seconds;
  if (!failure.empty())
    std::cout << " fail=" << failure;
  std::cout << "\n";
  return pass ? 0 : 1;
}
