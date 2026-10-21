// Counts-only quest pass over an explicitly selected, read-only RTA directory.
// Prints aggregate counts for the quest commands and the campaign events.
// Never prints paths, level names, entry-point names or script text.
#include "champions/archive.hpp"
#include "champions/quests_bridge.hpp"
#include "champions/script_commands.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace sc = champions::simulation::rta::script_commands;
namespace q = champions::simulation::rta::quests;

namespace {
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;
constexpr int MAX_DEPTH = 3;
constexpr std::size_t MAX_FILES = 4096;
// Binder word g_numPlayers. The measure plants one player so addQuest has a
// record to write. An unset word is 0 and the call would not enter its loop.
// This is a fixture, not a claimed boot value.
constexpr std::uint32_t NUM_PLAYERS = 0x4d8ae0u;

// Tables read from the owner's executable at run time, never copied. Addresses
// and layouts are research t3/research-quests 7efd3ce (difftest_quests.py,
// docs/quests.md section 1): g_missionTable 13 rows of 0x34 (story +0x08, evil
// +0x0c, medal +0x10, bonus +0x14, flag words +0x20/+0x24/+0x28),
// g_actLevelTables (one pointer per act to rows of a name pointer and a
// saved-state size, 8 bytes), the per-act counts and g_actCount, and the 81
// experience thresholds. ELF_SHA is difftest_quests.ELF_SHA (US v2.00).
constexpr const char *ELF_SHA = "292f6f48e3609c0026def2307d76c222cd5b389303d53b89b0e107d7b1dcd675";
constexpr std::uint32_t MISSION_TABLE = 0x4fe540u, MISSION_STRIDE = 0x34u, MISSION_ROWS = 13u;
constexpr std::uint32_t ACT_LISTS = 0x4f8e48u, ACT_COUNTS = 0x4f8e10u, ACT_COUNT = 0x4f8e7cu;
constexpr std::uint32_t XP_TABLE = 0x41b558u;
// MISSION_AT is mission row 1 (+0x4bbf + 13): the rows are difficulties 1..5.
constexpr std::uint32_t PLAYERS_AT = 0x4d8af0u, QUEST_COUNT = 0x43d0u, MISSION_AT = 0x4bccu, MEDAL_AT = 0x4c0du;
constexpr std::uint32_t EVIL_AT = 0x4c1au, DECISION_AT = 0x4c1bu, QUEST_VARS = 0x86da80u;

// FIPS 180-4 SHA-256, used only to confirm the executable's revision.
std::string sha256(const std::vector<std::uint8_t> &data) {
  static const std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  auto msg = data;
  const std::uint64_t bits = std::uint64_t(data.size()) * 8u;
  msg.push_back(0x80);
  while (msg.size() % 64 != 56)
    msg.push_back(0);
  for (int i = 7; i >= 0; --i)
    msg.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
  auto rot = [](std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
  for (std::size_t at = 0; at < msg.size(); at += 64) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = std::uint32_t(msg[at + 4 * i]) << 24 | std::uint32_t(msg[at + 4 * i + 1]) << 16 |
             std::uint32_t(msg[at + 4 * i + 2]) << 8 | std::uint32_t(msg[at + 4 * i + 3]);
    for (int i = 16; i < 64; ++i) {
      const auto s0 = rot(w[i - 15], 7) ^ rot(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const auto s1 = rot(w[i - 2], 17) ^ rot(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t v[8];
    std::copy(h, h + 8, v);
    for (int i = 0; i < 64; ++i) {
      const auto s1 = rot(v[4], 6) ^ rot(v[4], 11) ^ rot(v[4], 25);
      const auto ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
      const auto t1 = v[7] + s1 + ch + k[i] + w[i];
      const auto s0 = rot(v[0], 2) ^ rot(v[0], 13) ^ rot(v[0], 22);
      const auto maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
      const auto t2 = s0 + maj;
      for (int j = 7; j > 0; --j)
        v[j] = v[j - 1];
      v[4] += t1;
      v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; ++i)
      h[i] += v[i];
  }
  static const char digits[] = "0123456789abcdef";
  std::string out;
  for (auto word : h)
    for (int shift = 28; shift >= 0; shift -= 4)
      out.push_back(digits[(word >> shift) & 15u]);
  return out;
}

// A 32-bit little-endian ELF image addressed by virtual address through its
// PT_LOAD segments (the standard ELF layout). Bytes past a segment's file size
// read as zero. Unmapped addresses are refused.
struct Image {
  struct Segment {
    std::uint32_t vaddr, offset, filesz, memsz;
  };
  std::vector<std::uint8_t> bytes;
  std::vector<Segment> segments;
  static std::uint32_t le32(const std::vector<std::uint8_t> &b, std::size_t at) {
    if (at + 4 > b.size())
      throw std::runtime_error("ELF header is truncated");
    return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 | std::uint32_t(b[at + 2]) << 16 |
           std::uint32_t(b[at + 3]) << 24;
  }
  static std::uint32_t le16(const std::vector<std::uint8_t> &b, std::size_t at) {
    if (at + 2 > b.size())
      throw std::runtime_error("ELF header is truncated");
    return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8;
  }
  explicit Image(std::vector<std::uint8_t> data) : bytes(std::move(data)) {
    if (bytes.size() < 0x34 || bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 1 || bytes[5] != 1)
      throw std::runtime_error("not a 32-bit little-endian ELF");
    const auto phoff = le32(bytes, 0x1c), phentsize = le16(bytes, 0x2a), phnum = le16(bytes, 0x2c);
    for (std::uint32_t i = 0; i < phnum; ++i) {
      const std::size_t at = std::size_t(phoff) + std::size_t(i) * phentsize;
      if (le32(bytes, at) != 1)
        continue;
      Segment seg{le32(bytes, at + 8), le32(bytes, at + 4), le32(bytes, at + 16), le32(bytes, at + 20)};
      if (std::uint64_t(seg.offset) + seg.filesz > bytes.size())
        throw std::runtime_error("ELF segment past the file");
      segments.push_back(seg);
    }
  }
  std::uint8_t u8(std::uint32_t addr) const {
    for (const auto &seg : segments) {
      if (addr >= seg.vaddr && addr - seg.vaddr < seg.memsz) {
        const auto rel = addr - seg.vaddr;
        return rel < seg.filesz ? bytes[std::size_t(seg.offset) + rel] : 0;
      }
    }
    throw std::runtime_error("address outside the executable's segments");
  }
  std::uint32_t u32(std::uint32_t addr) const {
    return std::uint32_t(u8(addr)) | std::uint32_t(u8(addr + 1)) << 8 | std::uint32_t(u8(addr + 2)) << 16 |
           std::uint32_t(u8(addr + 3)) << 24;
  }
  std::int32_t s32(std::uint32_t addr) const { return static_cast<std::int32_t>(u32(addr)); }
  std::string cstr(std::uint32_t addr, std::size_t limit) const {
    std::string out;
    for (std::size_t i = 0; i < limit; ++i) {
      const auto c = u8(addr + static_cast<std::uint32_t>(i));
      if (!c)
        return out;
      out.push_back(static_cast<char>(c));
    }
    throw std::runtime_error("table string has no terminator");
  }
};
struct Tables {
  std::vector<q::MissionRow> rows;
  std::vector<std::vector<std::string>> acts;
  champions::simulation::rta::level_flow::Acts flow_acts;
  std::array<std::int32_t, q::XP_LEVELS> xp{};
};
Tables read_tables(const Image &elf) {
  Tables out;
  for (std::uint32_t i = 0; i < MISSION_ROWS; ++i) {
    const auto base = MISSION_TABLE + i * MISSION_STRIDE;
    auto text = [&](std::uint32_t off) {
      const auto ptr = elf.u32(base + off);
      return ptr ? elf.cstr(ptr, 48) : std::string();
    };
    out.rows.push_back({text(0x08), text(0x0c), text(0x10), text(0x14), elf.s32(base + 0x20),
                        elf.s32(base + 0x24), elf.s32(base + 0x28)});
  }
  const auto acts = elf.s32(ACT_COUNT);
  if (acts < 0 || acts > 64)
    throw std::runtime_error("act count outside 0..64");
  for (std::int32_t act = 0; act < acts; ++act) {
    const auto list = elf.u32(ACT_LISTS + 4u * std::uint32_t(act));
    const auto count = elf.s32(ACT_COUNTS + 4u * std::uint32_t(act));
    if (count < 0 || count > 256)
      throw std::runtime_error("act row count outside 0..256");
    std::vector<std::string> names;
    champions::simulation::rta::level_flow::Act rows;
    for (std::int32_t k = 0; k < count; ++k) {
      const auto at = list + 8u * std::uint32_t(k);
      const auto np = elf.u32(at);
      const auto name = np ? elf.cstr(np, 32) : std::string();
      names.push_back(name);
      rows.push_back({name, elf.u32(at + 4)});
    }
    out.acts.push_back(names);
    out.flow_acts.push_back(rows);
  }
  for (int i = 0; i < q::XP_LEVELS; ++i)
    out.xp[static_cast<std::size_t>(i)] = elf.s32(XP_TABLE + 4u * std::uint32_t(i));
  return out;
}

const char *quest_commands[] = {
    "addQuest", "removeQuest", "getv", "setv", "addv", "setMissionAvailable",
    "getMissionAvailable", "grantMission", "getMissionComplete", "setEvil", "getEvil",
    "setMadeDecision", "getMadeDecision", "grantGold", "grantSilver", "grantBronze",
    "getMedal", "setPlayerFlag", "getPlayerFlag", "givePlayerGold", "givePlayerExp",
    "activateMissionMenu", "setCFlag", "countQuestItems", "playerHasItem",
    "onePlayerHasItem", "removePlayerItem", "removeAllItems", "givePlayerItem",
    "givePlayerRandom"};

std::string lower(std::string text) {
  for (auto &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
std::string stem_of(const std::string &name) {
  const auto dot = name.rfind('.');
  return lower(dot == std::string::npos ? name : name.substr(0, dot));
}
std::string extension_of(const std::string &name) {
  const auto dot = name.rfind('.');
  return dot == std::string::npos ? std::string() : lower(name.substr(dot + 1));
}
std::vector<std::uint8_t> read_file(const fs::path &path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > MAX_FILE)
    return {};
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (size && !in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size)))
    return {};
  return bytes;
}
void scan(const fs::path &dir, int depth, std::map<std::string, fs::path> &files) {
  if (depth > MAX_DEPTH || files.size() >= MAX_FILES)
    return;
  std::error_code ec;
  for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (it->is_symlink(ec))
      continue;
    if (it->is_directory(ec))
      scan(it->path(), depth + 1, files);
    else if (it->is_regular_file(ec) && files.size() < MAX_FILES)
      files[lower(it->path().filename().string())] = it->path();
  }
}
const champions::Entry *find_entry(const std::vector<champions::Entry> &entries, const std::string &name) {
  for (const auto &entry : entries)
    if (lower(entry.name) == name)
      return &entry;
  return nullptr;
}
void add(sc::Counts &total, const sc::Counts &one) {
  total.started += one.started;
  total.reached_wait += one.reached_wait;
  total.reached_complete += one.reached_complete;
  total.faults += one.faults;
  total.missing_entries += one.missing_entries;
  total.objects_registered += one.objects_registered;
  total.objects_refused += one.objects_refused;
  for (std::size_t i = 0; i < sc::COMMANDS; ++i) {
    total.executed[i] += one.executed[i];
    total.refused[i] += one.refused[i];
  }
}
void add_events(q::Events &total, const q::Events &one) {
  total.menu_opens += one.menu_opens;
  total.cflag_bit0 += one.cflag_bit0;
  total.cflag_bit26 += one.cflag_bit26;
  total.cflag_other += one.cflag_other;
  total.boss_music += one.boss_music;
  total.act_resets += one.act_resets;
  total.flag39_removed += one.flag39_removed;
  total.exp_grants += one.exp_grants;
}
void drain(sc::Host &host) {
  if (!host.waiting() || host.faulted())
    return;
  std::set<std::string> seen;
  // The same measurement cap as champions-script-commands-measure (not
  // original behaviour); the key is Host::queue_key.
  const auto limit = static_cast<std::uint64_t>(host.public_count()) + 2u;
  for (std::uint64_t step = 0; step < limit && host.waiting() && !host.faulted(); ++step) {
    if (!seen.insert(host.queue_key()).second)
      return;
    host.on_ready_tick();
  }
}
int command_index(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::strcmp(sc::row(i).name, name) == 0)
      return static_cast<int>(i);
  return -1;
}
struct Level {
  std::string stem;
  champions::simulation::rta::amx::ByteVector script;
  std::vector<std::uint8_t> objects;
};
struct Effects {
  std::uint64_t quest_lines = 0, available_bits = 0, complete_bits = 0, medals = 0, evil_bits = 0,
                decision_bits = 0, vars = 0, vars_nonzero = 0;
};
// Record 0's campaign fields after a run, from the binder (difftest_quests
// offsets). The run is offline with one planted player.
void add_effects(Effects &total, sc::Host &host) {
  const auto &m = host.variables().memory;
  total.quest_lines += static_cast<std::uint64_t>(std::max(0, m.s32(PLAYERS_AT + QUEST_COUNT)));
  for (std::uint32_t i = 0; i < 65; ++i) {
    const auto byte = m.read(PLAYERS_AT + MISSION_AT + i);
    total.available_bits += byte & 1u;
    total.complete_bits += (byte >> 1) & 1u;
  }
  for (std::uint32_t i = 0; i < 13; ++i)
    total.medals += m.read(PLAYERS_AT + MEDAL_AT + i) ? 1u : 0u;
  for (int bit = 0; bit < 8; ++bit) {
    total.evil_bits += (m.read(PLAYERS_AT + EVIL_AT) >> bit) & 1u;
    total.decision_bits += (m.read(PLAYERS_AT + DECISION_AT) >> bit) & 1u;
  }
  auto at = QUEST_VARS;
  for (int guard = 0; guard < 100000 && m.read(at); ++guard) {
    const auto name = m.cstr(at);
    total.vars += 1;
    total.vars_nonzero += m.s8(at + static_cast<std::uint32_t>(name.size()) + 1u) ? 1u : 0u;
    at += static_cast<std::uint32_t>(name.size() + 2);
  }
}
void run(sc::Host &host) {
  try {
    host.run_prestart();
    host.run_start();
    drain(host);
  } catch (const std::exception &) {
  }
}
} // namespace

int main(int argc, char **argv) {
  std::string game;
  std::string root;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--game" && i + 1 < argc)
      game = argv[++i];
    else if (!arg.empty() && arg[0] != '-')
      root = arg;
    else {
      std::cerr << "usage: champions-quests-measure --game rta DIR\n";
      return 2;
    }
  }
  if (game != "rta" || root.empty() || !fs::is_directory(root)) {
    std::cerr << "usage: champions-quests-measure --game rta DIR\n";
    return 2;
  }
  std::map<std::string, fs::path> files;
  scan(root, 0, files);
  std::vector<Level> levels;
  std::set<std::string> level_stems;
  std::uint64_t levels_with_tex = 0, scripts_found = 0, scripts_load_refused = 0, levels_unreadable = 0;
  for (const auto &file : files) {
    if (extension_of(file.first) != "gob")
      continue;
    const auto stem = stem_of(file.first);
    if (!files.count(stem + ".tex"))
      continue;
    ++levels_with_tex;
    level_stems.insert(stem);
    auto gob = read_file(file.second);
    if (gob.empty()) {
      ++levels_unreadable;
      continue;
    }
    try {
      const auto top = champions::readGob({gob.data(), gob.size()});
      const auto *lump = find_entry(top, stem + ".lmp");
      if (!lump)
        continue;
      const auto lump_bytes = champions::payload({gob.data(), gob.size()}, *lump);
      const auto inner = champions::readLmp(lump_bytes);
      const auto *script = find_entry(inner, "script.scr");
      if (!script)
        continue;
      ++scripts_found;
      Level level;
      level.stem = stem;
      try {
        const auto bytes = champions::payload(lump_bytes, *script);
        level.script.assign(bytes.data, bytes.data + bytes.size);
      } catch (const std::exception &) {
        ++scripts_load_refused;
        continue;
      }
      if (const auto *objects = find_entry(inner, "objects.ob")) {
        try {
          const auto bytes = champions::payload(lump_bytes, *objects);
          level.objects.assign(bytes.data, bytes.data + bytes.size);
        } catch (const std::exception &) {
        }
      }
      levels.push_back(std::move(level));
    } catch (const std::exception &) {
      ++levels_unreadable;
    }
  }

  // Pass 1: each level in a fresh host, one planted player.
  sc::Counts total;
  q::Events events;
  Effects effects;
  std::uint64_t scripts_loaded = 0, scripts_no_level_entry = 0;
  for (const auto &level : levels) {
    sc::Host host;
    if (!host.load(level.script)) {
      ++scripts_load_refused;
      continue;
    }
    ++scripts_loaded;
    host.variables().memory.word(NUM_PLAYERS, 1u);
    sc::Stop stop = sc::Stop::Fault;
    try {
      // The loader's order (docs/SCRIPT_COMMANDS.md): preStart, the
      // objects.ob records, then start.
      host.run_prestart();
      if (!level.objects.empty())
        host.adopt_objects(level.objects.data(), level.objects.size());
      stop = host.run_start();
      drain(host);
    } catch (const std::exception &) {
    }
    if (stop == sc::Stop::Missing)
      ++scripts_no_level_entry;
    add(total, host.counts());
    add_events(events, host.campaign().events());
    add_effects(effects, host);
  }
  std::cout << "levels_with_tex " << levels_with_tex << '\n';
  std::cout << "levels_unreadable " << levels_unreadable << '\n';
  std::cout << "scripts_found " << scripts_found << '\n';
  std::cout << "scripts_loaded " << scripts_loaded << '\n';
  std::cout << "scripts_started " << total.started << '\n';
  std::cout << "scripts_no_level_entry " << scripts_no_level_entry << '\n';
  std::cout << "scripts_load_refused " << scripts_load_refused << '\n';
  std::cout << "scripts_reached_wait " << total.reached_wait << '\n';
  std::cout << "scripts_reached_complete " << total.reached_complete << '\n';
  std::cout << "script_faults " << total.faults << '\n';
  std::cout << "objects_registered " << total.objects_registered << '\n';
  std::cout << "objects_refused " << total.objects_refused << '\n';
  std::cout << "players_fixture 1\n";
  for (const char *name : quest_commands) {
    const auto index = command_index(name);
    const auto executed = index < 0 ? 0 : total.executed[static_cast<std::size_t>(index)];
    const auto refused = index < 0 ? 0 : total.refused[static_cast<std::size_t>(index)];
    std::cout << "executed " << name << ' ' << executed << '\n';
    std::cout << "refused " << name << ' ' << refused << '\n';
  }
  std::cout << "menu_opens " << events.menu_opens << '\n';
  std::cout << "cflag_bit0 " << events.cflag_bit0 << '\n';
  std::cout << "cflag_bit26 " << events.cflag_bit26 << '\n';
  std::cout << "cflag_other " << events.cflag_other << '\n';
  std::cout << "boss_music " << events.boss_music << '\n';
  std::cout << "exp_grants " << events.exp_grants << '\n';
  std::cout << "after_run quest_lines " << effects.quest_lines << '\n';
  std::cout << "after_run mission_available_bits " << effects.available_bits << '\n';
  std::cout << "after_run mission_complete_bits " << effects.complete_bits << '\n';
  std::cout << "after_run medals " << effects.medals << '\n';
  std::cout << "after_run evil_bits " << effects.evil_bits << '\n';
  std::cout << "after_run decision_bits " << effects.decision_bits << '\n';
  std::cout << "after_run quest_vars " << effects.vars << '\n';
  std::cout << "after_run quest_vars_nonzero " << effects.vars_nonzero << '\n';

  // Pass 2: the owner's executable, when the folder has it. Tables are read
  // at run time and only counted.
  const auto exe = files.find("slus_209.73");
  if (exe == files.end()) {
    std::cout << "executable absent\n";
    return 0;
  }
  const auto exe_bytes = read_file(exe->second);
  if (sha256(exe_bytes) != ELF_SHA) {
    std::cout << "executable revision_mismatch\n";
    return 0;
  }
  Tables tables;
  try {
    tables = read_tables(Image(exe_bytes));
  } catch (const std::exception &) {
    std::cout << "executable tables_unreadable\n";
    return 0;
  }
  std::cout << "executable us_v2_00 1\n";
  auto on_disc = [&](const std::string &name) {
    return !name.empty() && level_stems.count(lower(name)) != 0;
  };
  std::uint64_t pointers = 0, pointers_on_disc = 0, side_rows_closed = 0;
  for (const auto &row : tables.rows) {
    for (const auto *name : {&row.story, &row.evil, &row.medal, &row.bonus}) {
      if (name->empty())
        continue;
      ++pointers;
      pointers_on_disc += on_disc(*name) ? 1u : 0u;
    }
    if (!row.flag20 && !row.flag24 && !row.flag28 && row.medal.empty())
      ++side_rows_closed;
  }
  std::cout << "tables mission_rows " << tables.rows.size() << '\n';
  std::cout << "tables mission_level_pointers " << pointers << '\n';
  std::cout << "tables mission_levels_on_disc " << pointers_on_disc << '\n';
  std::cout << "tables rows_without_side_columns " << side_rows_closed << '\n';
  std::uint64_t act_rows = 0, act_empty = 0, act_rows_on_disc = 0;
  for (const auto &act : tables.acts) {
    for (const auto &name : act) {
      ++act_rows;
      act_empty += name.empty() ? 1u : 0u;
      act_rows_on_disc += on_disc(name) ? 1u : 0u;
    }
  }
  std::uint64_t stems_in_act = 0, stems_outside = 0;
  for (const auto &stem : level_stems)
    (q::find_act(stem, -1, tables.acts) >= 0 ? stems_in_act : stems_outside) += 1;
  std::uint64_t xp_rising = 0;
  for (int i = 1; i < q::XP_LEVELS; ++i)
    xp_rising += tables.xp[static_cast<std::size_t>(i)] >= tables.xp[static_cast<std::size_t>(i - 1)] ? 1u : 0u;
  std::cout << "tables acts " << tables.acts.size() << '\n';
  std::cout << "tables act_rows " << act_rows << '\n';
  std::cout << "tables act_rows_empty " << act_empty << '\n';
  std::cout << "tables act_rows_on_disc " << act_rows_on_disc << '\n';
  std::cout << "tables levels_in_an_act " << stems_in_act << '\n';
  std::cout << "tables levels_in_no_act " << stems_outside << '\n';
  std::cout << "tables xp_steps_not_falling " << xp_rising << " of " << (q::XP_LEVELS - 1) << '\n';
  std::cout << "tables hub_names_on_disc " << (on_disc(q::HUB_GOOD) ? 1 : 0) + (on_disc(q::HUB_EVIL) ? 1 : 0)
            << " of 2\n";

  // Pass 3: the real mission menu over the real rows. Every mission available
  // and complete with a gold medal, then each row and column is chosen with
  // cross and the fade done; evil off and on. Counts launches and whether the
  // launched name is a level on the disc.
  std::uint64_t launches = 0, launches_on_disc = 0, refused_cross = 0;
  for (int evil = 0; evil < 2; ++evil) {
    for (int row = 0; row < q::MENU_ROWS; ++row) {
      for (int col = 0; col < 3; ++col) {
        q::Campaign campaign;
        campaign.set_num_players(1);
        campaign.set_difficulty(q::IMAGE_DIFFICULTY);
        campaign.set_rows(tables.rows);
        for (int mission = 0; mission < q::MISSIONS; ++mission) {
          campaign.mission_set_available(mission, 1);
          campaign.mission_set_complete(mission, -1);
          campaign.grant_medal(mission, 3, [](const std::string &) {});
        }
        if (evil)
          campaign.set_evil_command(1);
        campaign.open_menu(0);
        while (campaign.menu().state != 2)
          campaign.tick_menu(0);
        campaign.menu().row = row;
        campaign.menu().col = col;
        campaign.tick_menu(q::BTN_CROSS);
        if (campaign.menu().state != 5) {
          ++refused_cross;
          continue;
        }
        q::MenuInputs done;
        done.fade = 128;
        bool launched = false;
        for (int tick = 0; tick < 8 && !launched; ++tick)
          launched = campaign.tick_menu(0, done);
        if (!launched)
          continue;
        ++launches;
        launches_on_disc += on_disc(campaign.menu().pending) ? 1u : 0u;
      }
    }
  }
  std::cout << "menu launches " << launches << '\n';
  std::cout << "menu launches_on_disc " << launches_on_disc << '\n';
  std::cout << "menu cross_refused " << refused_cross << '\n';

  // Pass 4: one campaign carried through every level in directory order
  // (not a play-through order) with the real act tables. Each level loads the
  // saved campaign, changes level through level flow, runs, and saves again.
  q::Campaign start;
  start.set_num_players(1);
  start.seed_new_character(0);
  start.set_rows(tables.rows);
  start.set_acts(tables.acts);
  start.set_xp_table(tables.xp);
  auto carried = start.save();
  int act = -1;
  std::uint64_t changes = 0, resets = 0, cleared_lines = 0, round_trips = 0, chain_faults = 0, ran = 0;
  for (const auto &level : levels) {
    sc::Host host;
    if (!host.load(level.script))
      continue;
    if (!level.objects.empty())
      host.adopt_objects(level.objects.data(), level.objects.size());
    try {
      host.load_campaign(carried);
      host.level().acts = tables.flow_acts;
      host.level().act = act;
      host.level().pending.request(level.stem);
      const auto before = static_cast<std::uint64_t>(std::max(0, host.campaign().record(0).quest_count));
      const auto change = host.change_level({});
      changes += change.act_changed ? 1u : 0u;
      if (change.reset_act) {
        ++resets;
        cleared_lines += before;
      }
      run(host);
      ++ran;
      chain_faults += host.counts().faults;
      carried = host.save_campaign();
      round_trips += q::Campaign::load(carried).save() == carried ? 1u : 0u;
      act = host.level().act;
    } catch (const std::exception &) {
      ++chain_faults;
    }
  }
  const auto last = q::Campaign::load(carried);
  std::uint64_t vars_at_end = last.vars().size(), available_at_end = 0, complete_at_end = 0;
  for (std::size_t i = 0; i < last.record(0).missions.size(); ++i) {
    available_at_end += last.record(0).missions[i] & 1u;
    complete_at_end += (last.record(0).missions[i] >> 1) & 1u;
  }
  std::cout << "chain levels_run " << ran << '\n';
  std::cout << "chain act_changes " << changes << '\n';
  std::cout << "chain act_resets " << resets << '\n';
  std::cout << "chain quest_lines_cleared " << cleared_lines << '\n';
  std::cout << "chain save_round_trips " << round_trips << '\n';
  std::cout << "chain faults " << chain_faults << '\n';
  std::cout << "chain end quest_lines " << last.record(0).quest_count << '\n';
  std::cout << "chain end quest_vars " << vars_at_end << '\n';
  std::cout << "chain end mission_available_bits " << available_at_end << '\n';
  std::cout << "chain end mission_complete_bits " << complete_at_end << '\n';
  return 0;
}
