// Private story-route acceptance over an explicitly selected, read-only RTA
// directory. Loads the shipped level scripts at run time, checks that every
// entry story_route lists is a public of its level's script, and replays the
// research's executed outcomes through the client's script host with the
// story-route gate on. Prints counts and case numbers only: never paths,
// script text, quest or help keys. Level and entry identifiers are the
// research's (t3/research-story-route-r3 3c7987c docs/story-route.md 6-8).
#include "champions/archive.hpp"
#include "champions/mission_menu_table.hpp"
#include "champions/script_commands.hpp"
#include "champions/story_route.hpp"
#include "champions/strings.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace sc = champions::simulation::rta::script_commands;
namespace q = champions::simulation::rta::quests;
namespace route = champions::simulation::rta::story_route;

namespace {
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;
constexpr std::uint32_t PENDING = 0x41b6d0, ORIENT = 0x41b720, ENTRANCE = 0x4f8468, POST = 0x86fa80;
// The research's sentinels (docs/story-route.md:381-385, :565-569).
constexpr std::uint8_t MISSION_SENTINEL = 0xA0, ORIENT_SENTINEL = 0x5A;
const char *const PENDING_SENTINEL = "PENDING-SENTINEL";
const char *const ENTRANCE_SENTINEL = "ENTRANCE-SENTINEL";

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
void scan(const fs::path &dir, int depth, std::map<std::string, fs::path> &files) {
  if (depth > 3 || files.size() >= 4096)
    return;
  std::error_code ec;
  for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (it->is_symlink(ec))
      continue;
    if (it->is_directory(ec))
      scan(it->path(), depth + 1, files);
    else if (it->is_regular_file(ec))
      files[lower(it->path().filename().string())] = it->path();
  }
}
const champions::Entry *find_entry(const std::vector<champions::Entry> &entries, const std::string &name) {
  for (const auto &entry : entries)
    if (lower(entry.name) == name)
      return &entry;
  return nullptr;
}

struct Level {
  std::vector<std::uint8_t> script;
  std::vector<std::vector<std::uint8_t>> text;
};
struct Disc {
  std::map<std::string, fs::path> files;
  std::vector<std::vector<std::uint8_t>> gtext;
  std::optional<std::uint8_t> chime;
  std::map<std::string, Level> levels;

  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> lump(const std::string &file) {
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
      out.clear();
    }
    return out;
  }
  const Level *level(const std::string &stem) {
    const auto cached = levels.find(stem);
    if (cached != levels.end())
      return cached->second.script.empty() ? nullptr : &cached->second;
    Level out;
    const auto gob_path = files.find(stem + ".gob");
    if (gob_path != files.end()) {
      const auto gob = read_file(gob_path->second);
      try {
        const auto top = champions::readGob({gob.data(), gob.size()});
        if (const auto *lmp = find_entry(top, stem + ".lmp")) {
          const auto lump_bytes = champions::payload({gob.data(), gob.size()}, *lmp);
          const auto inner = champions::readLmp(lump_bytes);
          if (const auto *script = find_entry(inner, "script.scr")) {
            const auto p = champions::payload(lump_bytes, *script);
            out.script.assign(p.data, p.data + p.size);
          }
          for (const auto &entry : inner)
            if (champions::simulation::rta::strings::is_uni_name(entry.name)) {
              const auto p = champions::payload(lump_bytes, entry);
              out.text.emplace_back(p.data, p.data + p.size);
            }
        }
      } catch (const std::exception &) {
        out = {};
      }
    }
    const auto &stored = levels.emplace(stem, std::move(out)).first->second;
    return stored.script.empty() ? nullptr : &stored;
  }
};

struct Prepared {
  std::unique_ptr<sc::Host> host;
  bool ok = false;
};
struct Plant {
  std::map<std::string, int> vars;
  bool evil = false;
  int offered = 0; // a mission whose record-0 byte is planted available (0xA1)
};

Prepared prepare(Disc &disc, const std::string &level, const Plant &plant = {}) {
  Prepared out;
  out.host = std::make_unique<sc::Host>();
  auto &host = *out.host;
  const auto *data = disc.level(level);
  if (!data)
    return out;
  host.start_single_player();
  for (const auto &table : disc.gtext)
    host.register_text(table);
  for (const auto &table : data->text)
    host.register_text(table);
  if (disc.chime)
    host.set_help_chime(*disc.chime);
  host.hud().inputs.camera_mode = 1;
  if (!host.load(champions::simulation::rta::amx::ByteVector(data->script.begin(), data->script.end())))
    return out;
  host.require_complete_commands();
  host.set_entry_gate(route::gate_for(level));
  auto campaign = q::Campaign::load(host.save_campaign());
  for (int slot = 0; slot < q::PLAYERS; ++slot)
    for (int mission = 3; mission <= 6; ++mission)
      campaign.record(slot).missions[std::size_t(q::mission_index(q::IMAGE_DIFFICULTY, mission))] = MISSION_SENTINEL;
  if (plant.offered)
    campaign.record(0).missions[std::size_t(q::mission_index(q::IMAGE_DIFFICULTY, plant.offered))] =
        MISSION_SENTINEL | 1u;
  if (plant.evil)
    campaign.record(0).evil = static_cast<std::uint8_t>(1u << q::IMAGE_DIFFICULTY);
  for (const auto &[name, value] : plant.vars)
    campaign.var_set(name, value);
  host.load_campaign(campaign.save());
  auto &memory = host.variables().memory;
  memory.string(PENDING, PENDING_SENTINEL);
  memory.string(ENTRANCE, ENTRANCE_SENTINEL);
  memory.byte(ORIENT, ORIENT_SENTINEL);
  out.ok = true;
  return out;
}

std::uint8_t byte(const sc::Host &host, int slot, int mission) {
  return host.campaign().record(slot).missions[std::size_t(q::mission_index(q::IMAGE_DIFFICULTY, mission))];
}
// Mission m on four records equals r0, rest; every other mission 3..6 stays at the sentinel.
bool missions(const sc::Host &host, int mission, std::uint8_t r0, std::uint8_t rest) {
  if (mission && (byte(host, 0, mission) != r0 || byte(host, 1, mission) != rest ||
                  byte(host, 2, mission) != rest || byte(host, 3, mission) != rest))
    return false;
  for (int other = 3; other <= 6; ++other)
    if (other != mission)
      for (int slot = 0; slot < q::PLAYERS; ++slot)
        if (byte(host, slot, other) != MISSION_SENTINEL)
          return false;
  return true;
}
bool untouched(const sc::Host &host) { return missions(host, 0, 0, 0); }
std::optional<int> var(sc::Host &host, const std::string &name) {
  for (const auto &[key, value] : host.variables().quest_vars())
    if (key == lower(name))
      return int(value);
  return std::nullopt;
}
int quests(const sc::Host &host) { return host.campaign().record(0).quest_count; }
std::string pending(sc::Host &host) { return host.variables().memory.cstr(PENDING); }
std::string entrance(sc::Host &host) { return host.variables().memory.cstr(ENTRANCE); }
int orient(sc::Host &host) { return host.variables().memory.read(ORIENT); }
bool ran(sc::Stop stop) { return stop == sc::Stop::Complete || stop == sc::Stop::Wait; }
// The entry ran up to a client command that stays refused (not an entry refusal).
bool stops_at(sc::Host &h, sc::Stop stop, const std::string &command) {
  return stop == sc::Stop::Refused && h.counts().entry_refusals == 0 && h.last_refusal() == "refused " + command;
}
std::size_t command(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::string(sc::row(i).name) == name)
      return i;
  return 0;
}

struct Tally {
  bool verbose = false;
  std::map<std::string, std::pair<int, int>> groups; // group -> passed, cases
  std::vector<std::string> failed;
  void add(const std::string &group, bool ok) {
    auto &g = groups[group];
    g.second += 1;
    if (ok)
      g.first += 1;
    else
      failed.push_back(group + "#" + std::to_string(g.second));
    if (!ok && verbose)
      std::cout << "  ^ " << group << "#" << g.second << '\n';
  }
};
} // namespace

int main(int argc, char **argv) {
  const bool verbose = argc == 3 && std::string(argv[1]) == "--verbose";
  if ((argc != 2 && !verbose) || !fs::is_directory(argv[argc - 1])) {
    std::cerr << "usage: champions-story-route-probe [--verbose] DIR\n";
    return 2;
  }
  Disc disc;
  scan(argv[argc - 1], 0, disc.files);
  for (auto &entry : disc.lump("gtext.lmp"))
    if (champions::simulation::rta::strings::is_uni_name(entry.first))
      disc.gtext.push_back(std::move(entry.second));
  for (const auto &entry : disc.lump("fx.lmp"))
    if (entry.first == champions::simulation::rta::strings::HELP_CHIME_ENTRY && entry.second.size() > 0x3c)
      disc.chime = entry.second[0x3c];
  Tally tally;
  tally.verbose = verbose;

  // Menu table and acts, read from the owner's executable at run time.
  if (disc.files.count("slus_209.73")) {
    champions::simulation::rta::arena_modes::Executable exe;
    if (exe.load(read_file(disc.files["slus_209.73"]))) {
      try {
        const auto rows = q::read_mission_rows(exe);
        const auto acts = q::read_act_tables(exe);
        const auto entries = route::menu_entries(rows, acts.names);
        int evil = 0;
        for (const auto &entry : entries)
          evil += !entry.evil.empty();
        tally.add("menu-table", entries.size() == 12 && evil == 3);
        // The table names of missions 3-5 are the levels sections 7-8 start in.
        tally.add("menu-table", entries[2].story == "islc1" && entries[3].story == "wat1" &&
                                    entries[4].story == "tormenta");
        for (int mission = 3; mission <= 5; ++mission)
          tally.add("menu-table", route::story_level_mission(entries[std::size_t(mission - 1)].story) == mission);
      } catch (const std::exception &) {
        tally.add("menu-table", false);
      }
    } else {
      tally.add("menu-table", false);
    }
  } else {
    tally.add("menu-table", false);
  }

  // Every listed entry is a public of its level's shipped script.
  int levels_loaded = 0;
  for (const auto &entry : route::observed_entries()) {
    auto prepared = prepare(disc, entry.level);
    tally.add("entries-present", prepared.ok && prepared.host->machine() &&
                                     prepared.host->machine()->find_public(entry.name).first ==
                                         champions::simulation::rta::amx::ERR_NONE);
  }
  for (const auto &entry : route::refused_entries()) {
    auto prepared = prepare(disc, entry.level);
    tally.add("refused-present", prepared.ok && prepared.host->machine() &&
                                     prepared.host->machine()->find_public(entry.name).first ==
                                         champions::simulation::rta::amx::ERR_NONE);
  }
  for (const auto &level : disc.levels)
    levels_loaded += !level.second.script.empty();

  // --verbose prints host state for a failed case to the terminal only.
  // It can name script entries and commands; keep it out of tracked logs.
  auto with = [&](const std::string &level, const Plant &plant, const std::function<bool(sc::Host &)> &fn) {
    auto prepared = prepare(disc, level, plant);
    if (!prepared.ok)
      return false;
    bool ok = false;
    try {
      ok = fn(*prepared.host);
    } catch (const std::exception &error) {
      if (verbose)
        std::cout << "  exception " << error.what() << '\n';
    }
    if (!ok && verbose) {
      auto &h = *prepared.host;
      std::cout << "  detail refusal[" << h.last_refusal() << "] fault[" << h.last_fault() << "] entry_refusals "
                << h.counts().entry_refusals << " value " << h.last_value() << " quests " << quests(h)
                << " pending[" << pending(h) << "] entrance[" << entrance(h) << "] orient " << orient(h);
      for (int m = 3; m <= 6; ++m)
        std::cout << " m" << m << '=' << int(byte(h, 0, m)) << '/' << int(byte(h, 1, m));
      std::cout << '\n';
    }
    return ok;
  };
  auto returns = [&](const std::string &level, const std::string &entry, const Plant &plant, int expected) {
    return with(level, plant, [&](sc::Host &h) {
      return h.run(entry) == sc::Stop::Complete && h.last_value() == expected && untouched(h);
    });
  };
  auto travel = [&](const std::string &level, const std::string &entry, const std::string &to,
                    const std::string &door) {
    return with(level, {}, [&](sc::Host &h) {
      return ran(h.run(entry)) && pending(h) == to && orient(h) == 0xFF && entrance(h) == door && untouched(h);
    });
  };

  // ---- Mission 3 (section 7) ----
  tally.add("m3-start", with("islc1", {}, [](sc::Host &h) {
              return h.run_start() == sc::Stop::Refused && h.last_refusal().rfind("refused cutScene", 0) == 0 &&
                     missions(h, 3, 0xA1, 0xA0) && quests(h) == 0 && pending(h) == PENDING_SENTINEL &&
                     var(h, "startedMission3") == 1 && var(h, "currentMission") == 3;
            }));
  tally.add("m3-start", with("islc1", {{{"sawislc", 1}}, false, 0}, [](sc::Host &h) {
              return ran(h.run_start()) && missions(h, 3, 0xA1, 0xA0) && var(h, "sawislc") == 1;
            }));
  tally.add("m3-start", with("islcave1", {}, [](sc::Host &h) {
              return ran(h.run_start()) && missions(h, 3, 0xA1, 0xA0) && quests(h) == 0 &&
                     var(h, "startedMission3") == 1;
            }));
  tally.add("m3-dialog", returns("islc1", "if4257", {}, 1));
  tally.add("m3-dialog", returns("islc1", "if4257", {{{"wa_metKerriel", 1}}, false, 0}, 0));
  tally.add("m3-dialog", returns("islc1", "ifa305", {}, 0));
  tally.add("m3-dialog", returns("islc1", "ifa305", {{{"wa_metKerriel", 1}}, false, 0}, 1));
  tally.add("m3-dialog", with("islc1", {}, [](sc::Host &h) {
              return ran(h.run("setvdea6")) && var(h, "wa_metKerriel") == 1 && quests(h) == 1 && untouched(h);
            }));
  for (const auto &[entry, to] : std::vector<std::pair<std::string, std::string>>{
           {"lostHope", "islsm2a"}, {"solitary", "islsm1"}, {"regret", "islsm3a"}, {"gnome", "islgnome"},
           {"kerriel", "islc1"}})
    tally.add("m3-travel", travel("islc1", entry, to, "boat"));
  for (const auto &[entry, post] : std::vector<std::pair<std::string, std::string>>{
           {"postActifb1b", "solitary"}, {"postActi6592", "lostHope"}, {"postActi928c", "regret"},
           {"postActifd9a", "gnome"}})
    tally.add("m3-racer", with("islc1", {}, [&, entry = entry, post = post](sc::Host &h) {
                return ran(h.run(entry)) && h.variables().memory.cstr(POST) == post && untouched(h);
              }));
  const std::vector<std::string> doors = {"one", "two", "three"};
  for (int i = 0; i < 3; ++i) {
    const auto entry = "entrance" + std::to_string(i + 1);
    for (int state = 0; state < 4; ++state) {
      // absent/absent, started 1 + finished absent, started 1 + finished 0, started 1 + finished 1.
      Plant plant;
      if (state >= 1)
        plant.vars["startedQuest"] = 1;
      if (state >= 2)
        plant.vars["finishedQuest"] = state == 3 ? 1 : 0;
      const int kills = state == 1 || state == 2 ? 8 : 0;
      tally.add("m3-gnome", with("islgnome", plant, [&](sc::Host &h) {
                  const auto stop = h.run(entry);
                  const auto k = command("killMonster");
                  const auto calls = h.counts().executed[k] + h.counts().refused[k];
                  if (kills == 0)
                    return ran(stop) && calls == 0 && pending(h) == "islc2" && entrance(h) == doors[std::size_t(i)];
                  return calls >= 1 && calls <= std::uint64_t(kills) && untouched(h);
                }));
    }
  }
  tally.add("m3-bless", returns("islc2", "iffbe4", {}, 0));
  tally.add("m3-bless", returns("islc2", "if9be1", {{{"wa_gotBlessed", 1}}, false, 0}, 1));
  tally.add("m3-bless", returns("islc2", "iffa37", {}, 1));
  tally.add("m3-bless", returns("islc2", "iffa37", {{{"wa_gotBlessed", 1}}, false, 0}, 0));
  // The grant matches; the client then stops at the completion sound
  // (soundSequence stays refused without a bound sound resource).
  tally.add("m3-complete", with("islc2", {{}, false, 3}, [](sc::Host &h) {
              const auto stop = h.run("callScri8a40");
              return (ran(stop) || stops_at(h, stop, "soundSequence")) && missions(h, 3, 0xA3, 0xA2) &&
                     var(h, "wa_gotBlessed") == 1;
            }));
  tally.add("m3-complete", with("islcave1", {}, [](sc::Host &h) {
              return ran(h.run("callScri7351")) && missions(h, 3, 0xA2, 0xA2) && quests(h) == 0;
            }));
  for (const auto *entry : {"setMissi5f9f", "setMissidce3", "setMissie8e9", "setMissic4"}) {
    const bool quest = std::string(entry) == "setMissie8e9" || std::string(entry) == "setMissic4";
    // Section 7 does not mention store calls on these; two stop at the
    // client's refused activateStore after the availability write.
    tally.add("m4-offer", with("hub", {}, [&](sc::Host &h) {
                const auto stop = h.run(entry);
                return (ran(stop) || stops_at(h, stop, "activateStore")) && missions(h, 4, 0xA1, 0xA0) &&
                       quests(h) == (quest ? 1 : 0);
              }));
  }

  // ---- Mission 4 (section 8) ----
  std::string good_title, evil_title;
  for (const bool evil : {false, true})
    tally.add("m4-start", with("wat1", {{}, evil, 0}, [&](sc::Host &h) {
                const bool ok = h.run_start() == sc::Stop::Refused &&
                                h.last_refusal().rfind("refused cutScene", 0) == 0 && missions(h, 4, 0xA1, 0xA0) &&
                                quests(h) == 1 && var(h, "startedMission4") == 1 && var(h, "currentMission") == 4 &&
                                pending(h) == PENDING_SENTINEL;
                (evil ? evil_title : good_title) = q::field_text(h.campaign().record(0).quests[0].title);
                return ok;
              }));
  tally.add("m4-start", !good_title.empty() && good_title != evil_title);
  tally.add("m4-start", with("wat1", {{{"sawWat1", 1}}, false, 0}, [](sc::Host &h) {
              return ran(h.run_start()) && missions(h, 4, 0xA1, 0xA0) && quests(h) == 1;
            }));
  // game_getEvil returns the masked value, 1 << difficulty: 2 at the image
  // difficulty 1 (research ref_natives.py:1191-1194); research ran difficulty 0.
  tally.add("m4-keys", returns("wat1", "return10c8", {{}, true, 0}, 1 << q::IMAGE_DIFFICULTY));
  tally.add("m4-keys", returns("wat1", "return10c8", {}, 0));
  tally.add("m4-keys", returns("wat1", "return6198", {}, 1));
  tally.add("m4-keys", returns("wat1", "return6198", {{}, true, 0}, 0));
  for (const auto &[entry, to] : std::vector<std::pair<std::string, std::string>>{
           {"newWorld57c0", "wate2c"}, {"newWorld9711", "wate2b"}, {"newWorld7517", "wate2a"},
           {"newWorldced", "watg2c"}, {"newWorldf0d", "watg2a"}, {"newWorldd7df", "watg2b"}})
    tally.add("m4-travel", travel("wat1", entry, to, ENTRANCE_SENTINEL));
  tally.add("m4-water1", with("water1", {}, [](sc::Host &h) {
              return ran(h.run_start()) && missions(h, 4, 0xA1, 0xA0) && quests(h) == 0;
            }));
  tally.add("m4-water1", with("water1", {}, [](sc::Host &h) {
              return ran(h.run("callScri7351")) && missions(h, 4, 0xA2, 0xA2) && quests(h) == 0 &&
                     pending(h) == PENDING_SENTINEL;
            }));
  for (const auto *level : {"wate2a", "wate2b", "wate2c", "watg2a", "watg2b", "watg2c"}) {
    tally.add("m4-back", travel(level, "newWorlde6b4", "wat1", ENTRANCE_SENTINEL));
    // 25 unpaused shardCheck calls with the client's empty bag: calls 1-24
    // make no item query; the 25th queries once, finds 0, does not grant and
    // queues shardCheck again.
    tally.add("m4-shard", with(level, {}, [](sc::Host &h) {
                const auto has = command("playerHasItem");
                for (int call = 1; call <= 25; ++call) {
                  h.variables().clear_deferred();
                  if (!ran(h.run("shardCheck")))
                    return false;
                  const auto queries = h.counts().executed[has];
                  if (call < 25 && (queries != 0 || h.queue().queued == 0 || !untouched(h)))
                    return false;
                  if (call == 25 && (queries != 1 || h.queue().queued == 0 || !untouched(h)))
                    return false;
                }
                return true;
              }));
    const bool wall = level[3] == 'e';
    tally.add("m4-refused", with(level, {}, [&](sc::Host &h) {
                return h.run(wall ? "brokeFinalWall" : "holeCheck") == sc::Stop::Refused &&
                       h.counts().entry_refusals == 1 && untouched(h);
              }));
    tally.add("m4-refused", with(level, {}, [](sc::Host &h) {
                return h.run_start() == sc::Stop::Refused && h.counts().entry_refusals == 1 && untouched(h);
              }));
  }
  tally.add("m4-iff46", returns("wate2a", "iff46", {}, 1));
  tally.add("m4-iff46", returns("wate2a", "iff46", {{{"wat_complete", 1}}, false, 0}, 0));
  tally.add("m4-iff46", returns("wate2a", "iff46", {{{"wat_complete", 1}, {"wat_gotShard", 1}}, false, 0}, 1));

  // ---- Mission 5 (section 8) ----
  for (const bool evil : {false, true})
    // The entrance is written; the client then stops at the refused
    // preloadResource (lump_load of bust.lmp returned 0 in the research).
    tally.add("m5-prestart", with("tormenta", {{}, evil, 0}, [&](sc::Host &h) {
                const auto stop = h.run_prestart();
                return (ran(stop) || stops_at(h, stop, "preloadResource")) &&
                       entrance(h) == (evil ? "evilstart" : "goodstart") && pending(h) == PENDING_SENTINEL &&
                       untouched(h);
              }));
  // Research: start sets mission 5 available and posts absorb. The client
  // stops earlier, at its refused autoPropSetState, before the availability
  // store. Counted as its own group so the gap stays visible.
  tally.add("m5-start", with("tormenta", {}, [](sc::Host &h) {
              return ran(h.run_start()) && missions(h, 5, 0xA1, 0xA0) && quests(h) == 0 &&
                     var(h, "startedMission5") == 1 && var(h, "currentMission") == 5 && var(h, "tor_beenHere") == 1;
            }));
  tally.add("m5-start-client-stop", with("tormenta", {}, [](sc::Host &h) {
              return stops_at(h, h.run_start(), "autoPropSetState") && untouched(h);
            }));
  tally.add("m5-cuts", with("tormenta", {}, [](sc::Host &h) {
              return h.run("if87a0") == sc::Stop::Refused && h.last_refusal().rfind("refused cutScene", 0) == 0;
            }));
  tally.add("m5-cuts", with("tormenta", {{{"sawtorb", 1}}, false, 0}, [](sc::Host &h) {
              return ran(h.run("if87a0")) && untouched(h);
            }));
  tally.add("m5-gol", returns("tormenta", "if1b74", {{}, true, 0}, 1));
  tally.add("m5-gol", returns("tormenta", "if1b74", {}, 0));
  tally.add("m5-gol", returns("tormenta", "ifb0b8", {}, 1));
  tally.add("m5-gol", returns("tormenta", "ifb0b8", {{{"tor_talkedToGol", 1}}, false, 0}, 0));
  tally.add("m5-gol", returns("tormenta", "if4fec", {{{"tor_finishedMission", 1}}, false, 0}, 1));
  tally.add("m5-gol", returns("tormenta", "if4fec", {}, 0));
  tally.add("m5-gol", with("tormenta", {}, [](sc::Host &h) {
              return ran(h.run("setv2725")) && var(h, "tor_talkedToGol") == 1 && quests(h) == 1 && untouched(h);
            }));
  for (const auto *entry : {"endtor", "callScri8c36"}) {
    tally.add("m5-complete", with("tormenta", {}, [&](sc::Host &h) {
                const auto stop = h.run(entry);
                return (ran(stop) || stops_at(h, stop, "soundSequence")) && missions(h, 5, 0xA2, 0xA2) &&
                       var(h, "tor_finishedMission") == 1;
              }));
    tally.add("m5-complete", with("tormenta", {{{"tor_finishedMission", 1}}, false, 0}, [&](sc::Host &h) {
                return ran(h.run(entry)) && untouched(h);
              }));
  }
  struct Door {
    const char *level, *entry, *to, *door;
  };
  for (const auto &d : std::vector<Door>{{"tormenta", "newWorld3aa4", "tormentb", ENTRANCE_SENTINEL},
                                         {"tormentb", "newWorld1223", "tormenta", "torb"},
                                         {"tormentb", "newWorld9066", "torment2", "frontdoor"},
                                         {"torment2", "newWorlda629", "tormentb", "frontdoor"},
                                         {"torment2", "newWorldf608", "torment3", "leftdoor"},
                                         {"torment2", "newWorld181f", "torment3", "rightdoor"},
                                         {"torment3", "newWorld6aa", "torment2", "rightdoor"},
                                         {"torment3", "newWorldbc33", "torment2", "leftdoor"}})
    tally.add("m5-travel", travel(d.level, d.entry, d.to, d.door));

  // ---- Hub and hub2 availability for missions 5 and 6 ----
  for (const auto &entry : route::availability_entries()) {
    if (entry.mission == 4)
      continue;
    tally.add("m56-offer", with(entry.level, {}, [&](sc::Host &h) {
                const auto stop = h.run(entry.name);
                const bool stop_ok = entry.store_calls
                                         ? stop == sc::Stop::Refused && h.last_refusal() == "refused activateStore"
                                         : ran(stop);
                return stop_ok && missions(h, entry.mission, 0xA1, 0xA0) && quests(h) == (entry.adds_quest ? 1 : 0);
              }));
  }

  int passed = 0, cases = 0;
  std::cout << "levels_loaded " << levels_loaded << '\n';
  for (const auto &[group, counts] : tally.groups) {
    std::cout << "group " << group << ' ' << counts.first << '/' << counts.second << '\n';
    passed += counts.first;
    cases += counts.second;
  }
  for (const auto &fail : tally.failed)
    std::cout << "failed " << fail << '\n';
  std::cout << "total " << passed << '/' << cases << '\n';
  return passed == cases ? 0 : 1;
}
