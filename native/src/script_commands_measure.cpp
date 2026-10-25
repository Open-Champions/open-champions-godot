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
#include <optional>
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

  total.callee_refusals += one.callee_refusals;

  total.objects_built += one.objects_built;
  total.objects_unbuilt += one.objects_unbuilt;
  total.objects_not_created += one.objects_not_created;
  total.objects_pool_refused += one.objects_pool_refused;
  total.objects_unclassified += one.objects_unclassified;
  total.load_draws_unmodelled += one.load_draws_unmodelled;
  total.item_load_gate_refusals += one.item_load_gate_refusals;
  total.item_load_draws_inferred += one.item_load_draws_inferred;
  for (const auto &kv : one.unbuilt_constructors)
    total.unbuilt_constructors[kv.first] += kv.second;
  for (const auto &kv : one.build_refusals)
    total.build_refusals[kv.first] += kv.second;
  total.callee_refusals += one.callee_refusals;
  for (std::size_t i = 0; i < sc::COMMANDS; ++i) {
    total.executed[i] += one.executed[i];
    total.refused[i] += one.refused[i];
    total.stopped[i] += one.stopped[i];
    total.inferred[i] += one.inferred[i];

  }
}

void drain(sc::Host &host) {
  if (!host.waiting() || host.faulted())
    return;
  std::set<std::string> seen;
  // A measurement cap, not original behaviour: the original keeps ticking.
  // It bounds a counting wait whose key never repeats.
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
  // --bare: the host as the play session left it before docs/script-session.md
  // (no executable, no seed, no player count). Otherwise each level runs as the
  // play session runs it: one player, difficulty 1, the session seed fixture
  // below with SeedPolicy::OfflineSession, and the directory's SLUS_209.73 for
  // the object factory when there is one.
  bool bare = false;
  // Leave one part of the session binding out, to attribute count changes.
  bool no_seed = false, no_players = false, no_objects = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--game" && i + 1 < argc)
      game = argv[++i];
    else if (arg == "--bare")
      bare = true;
    else if (arg == "--no-seed")
      no_seed = true;
    else if (arg == "--no-players")
      no_players = true;
    else if (arg == "--no-objects")
      no_objects = true;
    else if (!arg.empty() && arg[0] != '-')
      root = arg;
    else {
      std::cerr << "usage: champions-script-commands-measure --game rta [--bare] DIR\n";
      return 2;
    }
  }
  if (bare && (no_seed || no_players || no_objects)) {
    std::cerr << "--bare already leaves every part out\n";
    return 2;
  }
  if (game != "rta" || root.empty() || !fs::is_directory(root)) {
    std::cerr << "usage: champions-script-commands-measure --game rta [--bare] DIR\n";
    return 2;
  }
  std::map<std::string, fs::path> files;
  scan(root, 0, files);
  // The play session's default shared seed (session_control::Config), as a
  // fixture: an offline session's seed is a clock reading.
  constexpr std::uint32_t SESSION_SEED = 17;
  std::shared_ptr<champions::simulation::rta::arena_modes::Executable> exe;
  if (!bare && !no_objects && files.count("slus_209.73")) {
    auto bytes = read_file(files["slus_209.73"]);
    auto next = std::make_shared<champions::simulation::rta::arena_modes::Executable>();
    if (next->load(std::move(bytes)))
      exe = std::move(next);
  }
  // The string table: every .uni entry of GTEXT.LMP (docs/STRINGS.md), and the
  // help chime's sound group, byte +0x3c of fx.lmp/butn_message.anm.
  std::vector<std::vector<std::uint8_t>> gtext;
  std::optional<std::uint8_t> chime;
  const auto lump_entries = [&](const std::string &file) {
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
    const auto found = files.find(file);
    if (found == files.end())
      return out;
    const auto bytes = read_file(found->second);
    try {
      const champions::Bytes b{bytes.data(), bytes.size()};
      for (const auto &entry : champions::readLmp(b)) {
        const auto p = champions::payload(b, entry);
        out.emplace_back(lower(entry.name), std::vector<std::uint8_t>(p.data, p.data + p.size));
      }
    } catch (const std::exception &) {
    }
    return out;
  };
  for (auto &entry : lump_entries("gtext.lmp"))
    if (champions::simulation::rta::strings::is_uni_name(entry.first))
      gtext.push_back(std::move(entry.second));
  for (const auto &entry : lump_entries("fx.lmp"))
    if (entry.first == champions::simulation::rta::strings::HELP_CHIME_ENTRY && entry.second.size() > 0x3c)
      chime = entry.second[0x3c];
  std::map<std::string, std::uint64_t> text_refusals;
  std::uint64_t level_tables = 0;
  std::vector<std::uint8_t> balance;
  if (exe && files.count("balance.lmp")) {
    auto archive = read_file(files["balance.lmp"]);
    try {
      const auto rows = champions::readLmp({archive.data(), archive.size()});
      if (const auto *entry = find_entry(rows, "monsters.bal")) {
        const auto bytes = champions::payload({archive.data(), archive.size()}, *entry);
        balance.assign(bytes.data, bytes.data + bytes.size);
      }
    } catch (const std::exception &) {
      balance.clear();
    }
  }
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
    champions::simulation::rta::SharedRandom seed(SESSION_SEED);
    if (!bare) {
      if (!no_seed)
        host.bind_seed(&seed, sc::SeedPolicy::OfflineSession);
      if (!no_players)
        host.start_single_player();
      if (exe)
        host.set_executable(sc::executable_view(exe));
      if (exe && !balance.empty())
        try { host.set_level_stats(stem, balance.data(), balance.size()); }
        catch (const std::exception &) {}
      for (const auto &entry : inner)
        if (extension_of(entry.name) == "world") {
          try {
            const auto bytes = champions::payload(lump_bytes, entry);
            host.set_level_world(bytes.data, bytes.size);
          } catch (const std::exception &) {}
          break;
        }
      host.advance_first_level_items(stem);
    }
    // The text sources are not session state; bare reproduces the earlier host.
    for (const auto &table : gtext)
      if (!bare)
        host.register_text(table);
    // A level lump's .uni joins the table while the lump is loaded (1.1).
    for (const auto &entry : inner)
      if (!bare && champions::simulation::rta::strings::is_uni_name(entry.name)) {
        try {
          const auto bytes = champions::payload(lump_bytes, entry);
          if (host.register_text(std::vector<std::uint8_t>(bytes.data, bytes.data + bytes.size)))
            ++level_tables;
        } catch (const std::exception &) {
        }
      }
    if (chime && !bare)
      host.set_help_chime(*chime);
    // Camera_setMode writes 0 only while this machine's inventory, shop,
    // dialog camera or a cutscene is up (research-ui docs/monsters-2.md 7.1
    // M6); none runs in this pass, so the local camera is in mode 1.
    if (!bare)
      host.hud().inputs.camera_mode = 1;
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
    for (const auto &[reason, n] : host.counts().text_refusals)
      text_refusals[reason] += n;
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

  std::cout << "callee_refusals " << total.callee_refusals << '\n';

  std::cout << "mode " << (bare ? "bare" : "session") << (no_seed ? " no-seed" : "")
            << (no_players ? " no-players" : "") << (no_objects ? " no-objects" : "") << '\n';
  std::cout << "executable_used " << (exe ? 1 : 0) << '\n';
  std::cout << "objects_built " << total.objects_built << '\n';
  std::cout << "objects_unbuilt " << total.objects_unbuilt << '\n';
  std::cout << "objects_not_created " << total.objects_not_created << '\n';
  std::cout << "objects_pool_refused " << total.objects_pool_refused << '\n';
  std::cout << "objects_unclassified " << total.objects_unclassified << '\n';
  std::cout << "load_draws_unmodelled " << total.load_draws_unmodelled << '\n';
  std::cout << "item_load_gate_refusals " << total.item_load_gate_refusals << '\n';
  std::cout << "item_load_draws_inferred " << total.item_load_draws_inferred << '\n';
  std::cout << "unbuilt_constructors " << total.unbuilt_constructors.size() << '\n';
  for (const auto &kv : total.build_refusals)
    std::cout << "build_refused " << kv.first << ' ' << kv.second << '\n';

  std::cout << "text_tables " << gtext.size() << " level_text_tables " << level_tables << " help_chime "
            << (chime ? 1 : 0) << '\n';
  for (const auto &[reason, n] : text_refusals)
    std::cout << "text_refusal " << reason << ' ' << n << '\n';
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    std::cout << "executed " << sc::row(i).name << ' ' << total.executed[i] << '\n';
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    std::cout << "refused " << sc::row(i).name << ' ' << total.refused[i] << '\n';
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (total.inferred[i])
      std::cout << "inferred " << sc::row(i).name << ' ' << total.inferred[i] << '\n';
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    std::cout << "stopped " << sc::row(i).name << ' ' << total.stopped[i] << '\n';

  return 0;
}
