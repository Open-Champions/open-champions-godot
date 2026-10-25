// Counts-only NPC dialog pass over an explicitly selected, read-only RTA
// directory (docs/DIALOG.md "Census"). Part 1 parses every .dbin of every
// .lmp of every .gob with the native reader, as t3/research-dialog 334578d
// tools/dialog_census.py counts them. Part 2 loads each level script on the
// native host with the dialog attached and talks to every talker through
// Creature_interact, the run task and the end handler.
// Never prints paths, level names, object names, entry names or text.
#include "champions/archive.hpp"
#include "champions/dialog.hpp"
#include "champions/native_audio.hpp"
#include "champions/script_commands.hpp"
#include "champions/ui_layout.hpp"
#include "champions/ui_text_layout.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace sc = champions::simulation::rta::script_commands;
namespace dlg = champions::simulation::rta::dialog;
namespace rta = champions::simulation::rta;

namespace {
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;
constexpr int MAX_DEPTH = 4;
constexpr std::size_t MAX_FILES = 4096;
constexpr int TICKS_PER_TALK = 4000;

std::string lower(std::string text) {
  for (auto &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
bool ends_with(const std::string &s, const std::string &tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}
std::vector<std::uint8_t> read_file(const fs::path &path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > MAX_FILE)
    return {};
  std::ifstream in(path, std::ios::binary);
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
std::vector<std::uint8_t> bytes_of(champions::Bytes b) { return {b.data, b.data + b.size}; }

std::map<std::string, std::uint64_t> counts;
void add(const std::string &key, std::uint64_t n = 1) { counts[key] += n; }

struct Font {
  bool ok = false;
  champions::ui_layout::FontMetrics metrics;
};

// Part 2: one talker on a fresh host.
void talk(const std::vector<std::uint8_t> &script, const std::vector<std::uint8_t> *objects,
          const sc::DialogLevel &level, const sc::DialogServices &services, std::size_t which, bool last_choice) {
  sc::Host host;
  rta::amx::ByteVector image(script.begin(), script.end());
  if (!host.load(image)) {
    add("talk.script_refused");
    return;
  }
  host.run_prestart();
  if (objects)
    host.adopt_objects(objects->data(), objects->size());
  host.attach_dialog(level, services);
  host.run_start();
  const auto talkers = host.talkers();
  if (which >= talkers.size())
    return;
  const auto address = talkers[which];
  const auto player = host.in_front_of(address);
  const auto used = host.use(0, player);
  add("talk.use." + used);
  if (!host.dialog().is_open()) {
    add("talk.not_opened");
    const auto &c = host.dialog_counts();
    add("talk.not_opened.start_messages", c.start_messages);
    add("talk.not_opened.gate_refused", c.open_gate_refused);
    add("talk.not_opened.mutex_held", host.dialog().mutex ? 1 : 0);
    for (const auto &[reason, n] : c.refused)
      add("talk.not_opened.refused " + reason, n);
    const auto &q = host.counts();
    for (std::size_t i = 0; i < sc::COMMANDS; ++i)
      if (q.refused[i])
        add(std::string("talk.not_opened.command_refused ") + sc::row(i).name, q.refused[i]);
    return;
  }
  add("talk.opened");
  int ticks = 0;
  bool moved = false;
  for (; ticks < TICKS_PER_TALK && (host.dialog().is_open() || host.dialog().cam_move); ++ticks) {
    sc::DialogInputs in;
    const auto &d = host.dialog();
    // Confirm every 8 ticks; with last_choice, move down first when choices show.
    if (last_choice && d.at_end && d.visible.size() > 1 && d.cursor + 1 < std::int64_t(d.visible.size()) &&
        rta::eq(d.blend, rta::ONE) && !moved) {
      in.pad = 0x4000;
      moved = d.cursor + 2 >= std::int64_t(d.visible.size());
    } else if (ticks % 8 == 7) {
      in.pad = 0x40;
      moved = false;
    }
    // g_uiFadeIn[0] at 1.0: the UI fade is a draw-side value not run here.
    in.fade = rta::ONE;
    host.dialog_tick(in);
    host.on_ready_tick();
    if (host.dialog_stalled()) {
      add("talk.stalled");
      host.dialog_leave();
      break;
    }
  }
  if (host.dialog().is_open())
    add("talk.still_open_after_limit");
  else if (host.dialog_counts().client_leaves == 0)
    add("talk.closed_by_end");
  const auto &c = host.dialog_counts();
  add("talk.nodes_shown", c.nodes_shown);
  add("talk.choices_taken", c.choices_taken);
  add("talk.scripts_run", c.scripts_run);
  add("talk.missing_entries", c.missing_entries);
  add("talk.ends", c.ends);
  add("talk.pending_levels", c.pending_levels);
  add("talk.post_actions", c.post_actions);
  add("talk.deferred_shops", c.deferred_shops);
  add("talk.shop_requests", c.shop_requests);
  add("talk.voice_lines", c.voice_lines);
  add("talk.silent_lines", c.silent_lines);
  add("talk.ticks_open", c.ticks_open);
  add("talk.start_messages", c.start_messages);
  add("talk.opens", c.opens);
  add("talk.anm_reads", c.anm_reads);
  add("talk.stalls", c.stalls);
  for (const auto &[reason, n] : c.refused)
    add("talk.refused " + reason, n);
  for (const auto &[path, n] : c.mouth)
    add("talk.mouth " + path, n);
  const auto &q = host.counts();
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (q.refused[i])
      add(std::string("talk.command_refused ") + sc::row(i).name, q.refused[i]);
}
} // namespace

int run(int argc, char **argv);
int main(int argc, char **argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "dialog census stopped: " << e.what() << "\n";
    for (const auto &[k, v] : counts)
      std::cout << k << ' ' << v << '\n';
    return 1;
  }
}
int run(int argc, char **argv) {
  std::string root;
  bool talk_all = true;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--parse-only")
      talk_all = false;
    else if (!arg.empty() && arg[0] != '-')
      root = arg;
    else {
      std::cerr << "usage: champions-dialog-census [--parse-only] DIR\n";
      return 2;
    }
  }
  if (root.empty() || !fs::is_directory(root)) {
    std::cerr << "usage: champions-dialog-census [--parse-only] DIR\n";
    return 2;
  }
  std::map<std::string, fs::path> files;
  scan(root, 0, files);

  // Services: the text table (GTEXT.LMP gtext.uni) and gamefontmed.fnt.
  std::optional<dlg::TextTable> text;
  if (const auto it = files.find("gtext.lmp"); it != files.end()) {
    auto bytes = read_file(it->second);
    try {
      for (const auto &e : champions::readLmp({bytes.data(), bytes.size()}))
        if (lower(e.name) == "gtext.uni") {
          const auto data = champions::payload({bytes.data(), bytes.size()}, e);
          text = dlg::TextTable::parse(data.data, data.size);
        }
    } catch (const std::exception &) {
      add("text_table_refused");
    }
  }
  add("text_table_entries", text ? text->entries() : 0);
  Font font;
  std::vector<fs::path> gobs;
  for (const auto &[name, path] : files)
    if (ends_with(name, ".gob"))
      gobs.push_back(path);
  for (const auto &[name, path] : files) {
    if (font.ok || !(ends_with(name, ".lmp") || ends_with(name, ".gob")))
      continue;
    auto bytes = read_file(path);
    try {
      const champions::Bytes view{bytes.data(), bytes.size()};
      const auto top = ends_with(name, ".gob") ? champions::readGob(view) : champions::readLmp(view);
      for (const auto &e : top)
        if (lower(e.name) == "gamefontmed.fnt") {
          font.metrics = champions::ui_layout::fontMetrics(champions::payload(view, e));
          font.ok = true;
          break;
        }
    } catch (const std::exception &) {
    }
  }
  add("font_found", font.ok ? 1 : 0);

  std::uint64_t level_scripts = 0;
  for (const auto &path : gobs) {
    add("gob");
    auto gob = read_file(path);
    std::vector<champions::Entry> top;
    try {
      top = champions::readGob({gob.data(), gob.size()});
    } catch (const std::exception &) {
      add("gob_refused");
      continue;
    }
    for (const auto &lump_entry : top) {
      if (!ends_with(lower(lump_entry.name), ".lmp"))
        continue;
      champions::Bytes lump;
      std::vector<champions::Entry> inner;
      try {
        lump = champions::payload({gob.data(), gob.size()}, lump_entry);
        inner = champions::readLmp(lump);
      } catch (const std::exception &) {
        add("lmp_refused");
        continue;
      }
      std::set<std::string> names;
      for (const auto &e : inner)
        names.insert(lower(e.name));
      sc::DialogLevel level;
      level.level = lump_entry.name.substr(0, lump_entry.name.size() - 4);
      std::vector<std::uint8_t> script, objects, stream_dir;
      for (const auto &e : inner) {
        const auto n = lower(e.name);
        const auto data = champions::payload(lump, e);
        if (n == "script.scr")
          script = bytes_of(data);
        else if (n == "objects.ob")
          objects = bytes_of(data);
        else if (n == "ntsc_dialog.bin" || (n == "dialog.bin" && stream_dir.empty()))
          stream_dir = bytes_of(data);
        if (!ends_with(n, ".dbin"))
          continue;
        level.files[e.name] = bytes_of(data);
        std::vector<dlg::Node> nodes;
        try {
          nodes = dlg::parse_dbin(data.data, data.size);
        } catch (const dlg::Refused &) {
          add("dbin_refused");
          continue;
        }
        add("dbin_files");
        add("dbin_nodes", nodes.size());
        add("dbin_bytes", data.size);
        if (names.count(n.substr(0, n.size() - 5) + ".cut")) {
          add("dbin_cutscene_files");
          add("dbin_cutscene_nodes", nodes.size());
          continue;
        }
        add("npc_files");
        add("npc_nodes", nodes.size());
        add("npc_nodes_unreachable", nodes.size() - dlg::reachable_nodes(nodes).size());
        for (const auto &node : nodes) {
          add("npc_nodes_with_condition", node.condition.empty() ? 0 : 1);
          add("npc_choices_per_node " + std::to_string(node.choices.size()));
          for (const auto &ch : node.choices) {
            add("npc_choices");
            add("npc_choices_with_condition", ch.condition.empty() ? 0 : 1);
            add("npc_choices_with_action", ch.action.empty() ? 0 : 1);
            add(ch.next == -1 ? "npc_choices_end" : "npc_choices_jump");
            add("npc_choices_action_and_jump", !ch.action.empty() && ch.next != -1 ? 1 : 0);
          }
          if (node.choices.size() > dlg::VISIBLE_ROOM)
            add("npc_nodes_over_visible_room");
        }
      }
      if (!objects.empty()) {
        try {
          for (const auto &record : champions::simulation::rta::level_flow::parse_objects(
                   champions::Bytes{objects.data(), objects.size()})) {
            bool script_prop = false, show = false;
            for (const auto &p : record.props) {
              const auto eq = p.find('=');
              const auto key = lower(p.substr(0, eq));
              script_prop |= key == "dialogscript";
              show |= key == "showname";
            }
            if (script_prop)
              add("objects_with_dialogscript");
            if (script_prop && show)
              add("objects_with_dialogscript_and_showname");
          }
        } catch (const std::exception &) {
          add("objects_refused");
        }
      }
      if (script.empty() || !talk_all)
        continue;
      ++level_scripts;
      // The level stream directory and its .vat (the bank sizes the records).
      if (!stream_dir.empty()) {
        const auto vat = files.find(lower(level.level) + ".vat");
        if (vat != files.end()) {
          std::error_code ec;
          const auto size = fs::file_size(vat->second, ec);
          try {
            for (const auto &entry : champions::audio::parse_vat_directory({stream_dir.data(), stream_dir.size()},
                                                                            std::size_t(size)))
              level.stream[entry.name] = {entry.offset, entry.length};
            add("levels_with_stream_directory");
          } catch (const std::exception &) {
            add("stream_directory_refused");
          }
        }
      }
      sc::DialogServices services;
      if (text)
        services.text = [&text](const std::string &key) { return text->lookup(key); };
      if (font.ok) {
        const auto *metrics = &font.metrics;
        services.layout_height = [metrics](const dlg::Wide &t) -> std::int64_t {
          return champions::ui_text_layout::drawWrapped(*metrics, dlg::BODY_X, 0, t, dlg::BODY_W, 0, 0, dlg::LINE)
              .height;
        };
        services.shown = [metrics](const dlg::Wide &t, std::int64_t y, std::int64_t budget, int bottom) -> std::int64_t {
          const auto w = champions::ui_text_layout::drawWrapped(
              *metrics, dlg::BODY_X, int(y), t, dlg::BODY_W, dlg::BODY_TOP, bottom, dlg::LINE, true, 0,
              budget < 0 ? std::size_t(-1) : std::size_t(budget));
          return std::int64_t(w.shown.value_or(0));
        };
      }
      // How many talkers this level has, from a probe host.
      sc::Host probe;
      rta::amx::ByteVector image(script.begin(), script.end());
      if (!probe.load(image))
        continue;
      probe.run_prestart();
      if (!objects.empty())
        probe.adopt_objects(objects.data(), objects.size());
      probe.attach_dialog(level, services);
      probe.run_start();
      const auto talkers = probe.talkers();
      add("talkers", talkers.size());
      add("interactables", probe.interactables().size());
      for (std::size_t i = 0; i < talkers.size(); ++i) {
        talk(script, objects.empty() ? nullptr : &objects, level, services, i, false);
        talk(script, objects.empty() ? nullptr : &objects, level, services, i, true);
      }
    }
  }
  add("level_scripts", level_scripts);
  for (const auto &[k, v] : counts)
    std::cout << k << ' ' << v << '\n';
  return 0;
}
