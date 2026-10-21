// Counts-only quest pass over an explicitly selected, read-only RTA directory.
// Prints aggregate counts for the quest commands and the campaign events.
// Never prints paths, level names, entry-point names or script text.
#include "champions/archive.hpp"
#include "champions/script_commands.hpp"
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
  sc::Counts total;
  q::Events events;
  std::uint64_t levels_with_tex = 0, scripts_found = 0, scripts_loaded = 0;
  std::uint64_t scripts_no_level_entry = 0, scripts_load_refused = 0;
  std::uint64_t levels_unreadable = 0;
  for (const auto &file : files) {
    if (extension_of(file.first) != "gob")
      continue;
    const auto stem = stem_of(file.first);
    if (!files.count(stem + ".tex"))
      continue;
    ++levels_with_tex;
    auto gob = read_file(file.second);
    if (gob.empty()) {
      ++levels_unreadable;
      continue;
    }
    std::vector<champions::Entry> top;
    try {
      top = champions::readGob({gob.data(), gob.size()});
    } catch (const std::exception &) {
      ++levels_unreadable;
      continue;
    }
    const auto *lump = find_entry(top, stem + ".lmp");
    if (!lump)
      continue;
    champions::Bytes lump_bytes;
    try {
      lump_bytes = champions::payload({gob.data(), gob.size()}, *lump);
    } catch (const std::exception &) {
      ++levels_unreadable;
      continue;
    }
    std::vector<champions::Entry> inner;
    try {
      inner = champions::readLmp(lump_bytes);
    } catch (const std::exception &) {
      ++levels_unreadable;
      continue;
    }
    const auto *script = find_entry(inner, "script.scr");
    if (!script)
      continue;
    ++scripts_found;
    champions::Bytes script_bytes;
    try {
      script_bytes = champions::payload(lump_bytes, *script);
    } catch (const std::exception &) {
      ++scripts_load_refused;
      continue;
    }
    sc::Host host;
    champions::simulation::rta::amx::ByteVector image(script_bytes.data, script_bytes.data + script_bytes.size);
    if (!host.load(image)) {
      ++scripts_load_refused;
      continue;
    }
    ++scripts_loaded;
    if (const auto *objects = find_entry(inner, "objects.ob")) {
      try {
        const auto bytes = champions::payload(lump_bytes, *objects);
        host.adopt_objects(bytes.data, bytes.size);
      } catch (const std::exception &) {
      }
    }
    host.variables().memory.word(NUM_PLAYERS, 1u);
    sc::Stop stop = sc::Stop::Fault;
    try {
      stop = host.begin_level();
      drain(host);
    } catch (const std::exception &) {
    }
    if (stop == sc::Stop::Missing)
      ++scripts_no_level_entry;
    add(total, host.counts());
    add_events(events, host.campaign().events());
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
  std::cout << "act_resets " << events.act_resets << '\n';
  std::cout << "flag39_removed " << events.flag39_removed << '\n';
  std::cout << "exp_grants " << events.exp_grants << '\n';
  return 0;
}
