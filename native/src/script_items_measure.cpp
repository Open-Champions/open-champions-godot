// Counts the item, gold, skill and generator script call sites on a disc, as
// research-items 49bc578 tools/items_census.py does: every .GOB under DIR,
// every container lump in it, every script.scr in those. Counts only: no
// level, item, class or string is printed, only command names and numbers.
#include "champions/amx.hpp"
#include "champions/archive.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace amx = champions::simulation::rta::amx;

namespace {
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;
constexpr int MAX_DEPTH = 4;
constexpr amx::Cell OP_SYSREQ_C = 0x7b; // docs/script.md section 1
const char *const COMMANDS[] = {
    "playerHasItem", "onePlayerHasItem", "countQuestItems", "removePlayerItem", "removeAllItems",
    "removeAllWeapons", "givePlayerItem", "giveOnePlayerItem", "givePlayerRandom", "dropItem",
    "selectDropItem", "givePlayerGold", "takePlayerGold", "checkPlayerGold", "checkPlayerMinGold",
    "clearAllSkills", "generateMonster"};

std::string lower(std::string text) {
  for (auto &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
std::vector<std::uint8_t> read_file(const fs::path &path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > MAX_FILE)
    return {};
  std::ifstream in(path, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (!in || (size && !in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))))
    return {};
  return bytes;
}
void scan(const fs::path &dir, int depth, std::vector<fs::path> &gobs) {
  if (depth > MAX_DEPTH)
    return;
  std::error_code ec;
  std::vector<fs::path> here;
  for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (it->is_symlink(ec))
      continue;
    if (it->is_directory(ec))
      scan(it->path(), depth + 1, gobs);
    else if (it->is_regular_file(ec) && lower(it->path().extension().string()) == ".gob")
      here.push_back(it->path());
  }
  std::sort(here.begin(), here.end());
  gobs.insert(gobs.end(), here.begin(), here.end());
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2 || !fs::is_directory(argv[1])) {
    std::cerr << "usage: champions-script-items-measure DIR\n";
    return 2;
  }
  std::vector<fs::path> gobs;
  scan(argv[1], 0, gobs);
  std::map<std::string, std::uint64_t> calls, scripts;
  std::uint64_t gob_count = 0, gob_refused = 0, script_count = 0, script_refused = 0, all_calls = 0;
  for (const auto &path : gobs) {
    ++gob_count;
    const auto data = read_file(path);
    std::vector<champions::Entry> top;
    try {
      top = champions::readGob({data.data(), data.size()});
    } catch (const std::exception &) {
      ++gob_refused;
      continue;
    }
    for (const auto &lump_entry : top) {
      champions::Bytes lump;
      std::vector<champions::Entry> inner;
      try {
        lump = champions::payload({data.data(), data.size()}, lump_entry);
        inner = champions::readLmp(lump);
      } catch (const std::exception &) {
        continue; // a lump that is not a container
      }
      for (const auto &entry : inner) {
        if (entry.name != "script.scr")
          continue;
        ++script_count;
        amx::ByteVector image;
        try {
          const auto bytes = champions::payload(lump, entry);
          image.assign(bytes.data, bytes.data + bytes.size);
        } catch (const std::exception &) {
          ++script_refused;
          continue;
        }
        amx::Machine machine(image);
        if (machine.load() != amx::ERR_NONE || !machine.decoded) {
          ++script_refused;
          continue;
        }
        const auto names = machine.native_names();
        std::set<std::string> seen;
        const auto &code = *machine.decoded;
        for (auto it = code.begin(); it != code.end(); ++it) {
          if (it->second.opcode != OP_SYSREQ_C)
            continue;
          const auto *index = std::get_if<amx::Cell>(&it->second.operand);
          if (!index || *index < 0 || static_cast<std::size_t>(*index) >= names.size())
            continue;
          ++all_calls;
          const auto &name = names[static_cast<std::size_t>(*index)];
          bool wanted = false;
          for (const char *command : COMMANDS)
            wanted = wanted || name == command;
          if (!wanted)
            continue;
          calls[name] += 1;
          seen.insert(name);
        }
        for (const auto &name : seen)
          scripts[name] += 1;
      }
    }
  }
  std::cout << "gobs " << gob_count << '\n';
  std::cout << "gobs_refused " << gob_refused << '\n';
  std::cout << "scripts " << script_count << '\n';
  std::cout << "scripts_refused " << script_refused << '\n';
  std::cout << "native_calls " << all_calls << '\n';
  for (const char *command : COMMANDS)
    std::cout << "calls " << command << ' ' << calls[command] << " scripts " << scripts[command] << '\n';
  const auto sum = [&](std::initializer_list<const char *> names) {
    std::uint64_t total = 0;
    for (const char *name : names)
      total += calls[name];
    return total;
  };
  std::cout << "has_count_calls " << sum({"playerHasItem", "onePlayerHasItem", "countQuestItems"}) << '\n';
  std::cout << "remove_calls " << sum({"removePlayerItem"}) << '\n';
  std::cout << "give_calls " << sum({"givePlayerItem", "giveOnePlayerItem"}) << '\n';
  std::cout << "generate_monster_calls " << sum({"generateMonster"}) << '\n';
  return 0;
}
