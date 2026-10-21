// Counts-only level-script pass over an explicitly selected, read-only RTA
// directory. Prints aggregate counts and the 191 published command names.
// Never prints paths, level names, entry-point names or script text.
#include "champions/archive.hpp"
#include "champions/script_commands.hpp"
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
namespace sc = champions::simulation::rta::script_commands;

namespace {
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;
constexpr int MAX_DEPTH = 3;
constexpr std::size_t MAX_FILES = 4096;

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
  total.refusal_stops += one.refusal_stops;
  total.objects_registered += one.objects_registered;
  total.objects_refused += one.objects_refused;
  total.stub_refusals += one.stub_refusals;
  for (std::size_t i = 0; i < sc::COMMANDS; ++i) {
    total.executed[i] += one.executed[i];
    total.refused[i] += one.refused[i];
  }
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
      std::cerr << "usage: champions-script-commands-measure --game rta DIR\n";
      return 2;
    }
  }
  if (game != "rta" || root.empty() || !fs::is_directory(root)) {
    std::cerr << "usage: champions-script-commands-measure --game rta DIR\n";
    return 2;
  }
  std::map<std::string, fs::path> files;
  scan(root, 0, files);
  sc::Counts total;
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
    sc::Stop stop = sc::Stop::Fault;
    try {
      // Loader order: preStart, the objects, then start (docs/SCRIPT_COMMANDS.md).
      host.run_prestart();
      if (const auto *objects = find_entry(inner, "objects.ob")) {
        try {
          const auto bytes = champions::payload(lump_bytes, *objects);
          host.adopt_objects(bytes.data, bytes.size);
        } catch (const std::exception &) {
          // adopt_objects counts its own refusal.
        }
      }
      stop = host.run_start();
      drain(host);
    } catch (const std::exception &) {
      // The host counts faults itself. A throw past that is still a fault.
    }
    if (stop == sc::Stop::Missing)
      ++scripts_no_level_entry;
    add(total, host.counts());
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
  std::cout << "scripts_stopped_on_refusal " << total.refusal_stops << '\n';
  std::cout << "objects_registered " << total.objects_registered << '\n';
  std::cout << "objects_refused " << total.objects_refused << '\n';
  std::cout << "stub_refusals " << total.stub_refusals << '\n';
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    std::cout << "executed " << sc::row(i).name << ' ' << total.executed[i] << '\n';
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    std::cout << "refused " << sc::row(i).name << ' ' << total.refused[i] << '\n';
  return 0;
}
