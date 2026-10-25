// Replays tests/vectors/strings.tsv.gz (tools/generate_strings_vectors.py: the
// research model tools/ref_strings.py at d4987fd on synthetic cases) on the
// native port, then runs the hand checks that docs/strings.md states directly.
// The vectors arrive decompressed on stdin (tools/run_strings_vectors.py).
#include "champions/strings.hpp"
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace S = champions::simulation::rta::strings;
namespace rta = champions::simulation::rta;

namespace {
int failures = 0;
long checks = 0;
long line_no = 0;

template <class A, class B> void eq(const A &got, const B &want, const std::string &what) {
  ++checks;
  if (!(got == want)) {
    if (++failures <= 25) {
      std::ostringstream o;
      o << "line " << line_no << ": " << what << "\n  got  " << got << "\n  want " << want;
      std::cerr << o.str() << "\n";
    }
  }
}
void ok(bool cond, const std::string &what) { eq(cond ? 1 : 0, 1, what); }

std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> out;
  std::istringstream in(line);
  std::string token;
  while (in >> token)
    out.push_back(token);
  return out;
}
std::vector<std::uint8_t> bytes(const std::string &hex) {
  std::vector<std::uint8_t> out;
  if (hex == "-")
    return out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  return out;
}
std::string str(const std::string &hex) {
  const auto b = bytes(hex);
  return std::string(b.begin(), b.end());
}
S::Units units(const std::string &hex) {
  S::Units out;
  if (hex == "-")
    return out;
  for (std::size_t i = 0; i + 3 < hex.size(); i += 4)
    out.push_back(static_cast<std::uint16_t>(std::stoul(hex.substr(i, 4), nullptr, 16)));
  return out;
}
std::string uh(const S::Units &u) {
  std::string out;
  char buf[8];
  for (const auto x : u) {
    std::snprintf(buf, sizeof buf, "%04x", x);
    out += buf;
  }
  return out.empty() ? "-" : out;
}
std::string hx(const std::vector<std::uint8_t> &b) {
  std::string out;
  char buf[4];
  for (const auto x : b) {
    std::snprintf(buf, sizeof buf, "%02x", x);
    out += buf;
  }
  return out.empty() ? "-" : out;
}
std::string hx(const std::string &s) { return hx(std::vector<std::uint8_t>(s.begin(), s.end())); }
std::uint32_t u32(const std::string &s) { return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)); }
std::string h(std::uint64_t v) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "%llx", static_cast<unsigned long long>(v));
  return buf;
}
std::string h8(std::uint32_t v) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%08x", v);
  return buf;
}

// --- table
struct TableRun {
  S::TextTable table;
};
std::string slots_line(const S::TextTable &t) {
  std::string body;
  int n = 0;
  for (std::size_t i = 0; i < S::TEXT_SLOTS; ++i)
    if (t.slots()[i]) {
      body += (n ? " " : "") + h(i) + ":" + h(t.slots()[i]);
      ++n;
    }
  return "TS " + std::to_string(n) + " " + (n ? body : "-");
}

// --- handle pool dump, as the generator writes it
std::string ref(const rta::Link *node, const rta::Link *cell) {
  if (!node)
    return "n";
  if (node == cell)
    return "c";
  if (node->kind == rta::LinkKind::Node)
    return h(static_cast<const rta::Node *>(node)->index);
  return "?";
}
std::string dump_pool(const rta::HandlePool &pool, const std::vector<rta::Cell *> &cells) {
  std::string out = "P";
  for (const auto *cell : cells) {
    std::string list;
    for (const rta::Link *node = cell->next; node; node = node->next) {
      if (!list.empty())
        list += ".";
      const auto *n = static_cast<const rta::Node *>(node);
      list += h(n->index) + "/" + ref(node->prev, cell) + "/" + ref(node->partner, nullptr) + "/" +
              (node->obj ? h(node->obj->id) : std::string("n"));
    }
    out += " " + (list.empty() ? std::string("-") : list);
  }
  std::string free;
  int count = 0;
  for (const rta::Link *node = pool.free; node && count < 16; node = node->next, ++count) {
    if (!free.empty())
      free += ".";
    free += h(static_cast<const rta::Node *>(node)->index);
  }
  return out + " | " + (free.empty() ? std::string("-") : free);
}

struct HudRun {
  std::unique_ptr<S::TextTable> table;
  std::unique_ptr<S::Hud> hud;
  std::unique_ptr<S::Commands> cmds;
  std::map<std::string, std::array<std::uint32_t, 3>> objects;
  unsigned local_mask = 0xf;
  void reset() {
    table = std::make_unique<S::TextTable>();
    hud = std::make_unique<S::Hud>();
    cmds = std::make_unique<S::Commands>(*table, *hud);
    objects.clear();
    local_mask = 0xf;
    hud->inputs.local = [this](int slot) { return slot >= 0 && slot < 4 && ((local_mask >> slot) & 1); };
    cmds->find_object = [this](const std::string &name) -> std::optional<std::array<std::uint32_t, 3>> {
      const auto found = objects.find(name);
      if (found == objects.end())
        return std::nullopt;
      return found->second;
    };
  }
  std::string state() const {
    const auto &u = *hud;
    std::string out = "E " + uh(u.message) + " " + h(u.msg_frames) + " " + h(u.msg_force) + " " +
                      std::to_string(u.help_open ? 1 : 0) + " " + h(u.help_title) + " " + h(u.help_body) +
                      " " + uh(cmds->resolve(u.help_title)) + " " + uh(cmds->resolve(u.help_body)) + " " +
                      std::to_string(u.help_close_delay) + " " + std::to_string(u.bust_shown) + " " +
                      std::to_string(u.compass_shown) + " " + hx(u.bust_texture) + " " + uh(u.bust_text) +
                      " " + h8(u.bust_value) + " " + h8(u.bust_max) + " " + uh(u.compass_label) + " " +
                      h8(u.compass_target[0]) + " " + h8(u.compass_target[1]) + " " + h8(u.compass_target[2]) +
                      " " + std::to_string(u.latch) + " " + std::to_string(table->nf_index());
    return out;
  }
};

struct SoundRun {
  std::unique_ptr<rta::HandlePool> pool;
  std::unique_ptr<S::SoundSeqs> model;
  struct O {
    rta::Object object;
    std::array<rta::Cell, 3> heads;
    std::array<std::uint32_t, 3> position{};
  };
  std::vector<std::unique_ptr<O>> objects;
  std::vector<rta::Cell *> cells() {
    std::vector<rta::Cell *> out;
    for (auto &slot : model->slots)
      out.push_back(&slot.handle_head);
    for (auto &o : objects)
      for (auto &c : o->heads)
        out.push_back(&c);
    return out;
  }
};
std::string slot_text(const S::SeqSlot &s) {
  std::vector<std::uint32_t> w = {s.tracks, s.capacity, s.flags, s.seq, s.track_link, s.track_time,
                                  s.track_flags, s.serial, s.no_pos, s.frozen, s.released, s.volume,
                                  s.near, s.far};
  for (auto x : s.track_words)
    w.push_back(x);
  for (auto x : s.pos)
    w.push_back(x);
  std::string out;
  for (std::size_t i = 0; i < w.size(); ++i)
    out += (i ? "," : "") + h(w[i]);
  return out;
}

struct CharmRun {
  std::unique_ptr<rta::HandlePool> pool;
  std::vector<std::unique_ptr<S::Creature>> creatures;
  std::array<rta::Cell, 4> lists;
  std::vector<rta::Cell *> cells() {
    std::vector<rta::Cell *> out;
    for (auto &c : creatures) {
      for (auto &head : c->heads)
        out.push_back(&head);
      out.push_back(&c->target);
    }
    for (auto &l : lists)
      out.push_back(&l);
    return out;
  }
};

std::array<std::uint32_t, 8> args8(const std::vector<std::string> &t, std::size_t from) {
  std::array<std::uint32_t, 8> a{};
  for (std::size_t i = 0; i < 8; ++i)
    a[i] = u32(t[from + i]);
  return a;
}

void replay(std::istream &in) {
  TableRun tables;
  HudRun hud;
  SoundRun sound;
  CharmRun charm;
  std::map<std::uint32_t, std::vector<std::uint8_t>> cstrings;
  std::map<std::uint32_t, S::Units> wstrings;
  std::string line;
  std::string pending_hud; // the HUD step's expected state follows on the next line
  std::string pending_sound;
  std::vector<S::UniEntry> walked;
  std::size_t walk_index = 0;
  bool saw_end = false;
  while (std::getline(in, line)) {
    ++line_no;
    if (line.empty() || line[0] == '#')
      continue;
    const auto t = split(line);
    const auto &k = t[0];
    if (k == "END") {
      saw_end = true;
      break;
    }
    if (k == "H") {
      eq(h(S::text_hash(str(t[1]))), t[2], "text_hash");
    } else if (k == "T") {
      const auto &op = t[1];
      if (op == "new") {
        tables.table = S::TextTable();
      } else if (op == "reg") {
        tables.table.register_table(bytes(t[3]), u32(t[2]));
      } else if (op == "unreg") {
        tables.table.unregister_table(bytes(t[3]), u32(t[2]));
      } else if (op == "find") {
        eq(h(tables.table.find(str(t[2]))), t[3], "text_find");
      } else if (op == "look") {
        const auto r = tables.table.lookup(str(t[2]), bytes(t[3]));
        eq(h(r.address), t[4], "lookup address");
        eq(std::to_string(r.hit ? 1 : 0), t[5], "lookup hit");
        eq(uh(r.units), t[6], "lookup units");
        eq(std::to_string(tables.table.nf_index()), t[7], "miss index");
        const auto &entry = tables.table.nf_ring()[static_cast<std::size_t>(tables.table.nf_index())];
        eq(uh(S::Units(entry.begin(), entry.end())), t[8], "miss ring entry");
      } else if (op == "refuse") {
        bool refused = false;
        try {
          tables.table.lookup(str(t[2]));
        } catch (const S::Refused &) {
          refused = true;
        }
        ok(refused, "an over-long key is refused");
      } else if (op == "walk") {
        const auto data = bytes(t[2]);
        walked = S::uni_entries(data.data(), data.size());
        walk_index = 0;
      } else if (op == "entry") {
        if (walk_index < walked.size()) {
          const auto &e = walked[walk_index];
          eq(h(e.key_offset) + " " + hx(e.key) + " " + h(e.text_offset) + " " + uh(e.units) + " " + h(e.next),
             t[2] + " " + t[3] + " " + t[4] + " " + t[5] + " " + t[6], "uni entry");
        }
        ++walk_index;
      } else if (op == "entries-end") {
        eq(walked.size(), static_cast<std::size_t>(std::stoul(t[2])), "uni entry count");
      }
    } else if (k == "TS") {
      eq(slots_line(tables.table), line, "slots after registration");
    } else if (k == "W") {
      if (t[1] == "copy")
        eq(uh(S::wstr_copy(units(t[2]))), t[3], "wstr_copy");
      else if (t[1] == "from")
        eq(uh(S::wstr_from_ascii(bytes(t[2]))), t[3], "wstr_fromAscii");
      else if (t[1] == "to")
        eq(hx(S::wstr_to_ascii(units(t[2]))), t[3], "wstr_toAscii");
    } else if (k == "D") {
      eq(S::float_text(u32(t[1])), t[2], "float text");
    } else if (k == "FM") {
      if (t[1] == "c")
        cstrings[u32(t[2])] = bytes(t[3]);
      else
        wstrings[u32(t[2])] = units(t[3]);
    } else if (k == "F" || k == "FA") {
      const bool wide = k == "F";
      const auto args = args8(t, 2);
      const S::CStr cs = [&](std::uint32_t a) {
        auto v = cstrings.at(a);
        v.push_back(0);
        return v;
      };
      const S::WStr ws = [&](std::uint32_t a) {
        auto v = wstrings.at(a);
        v.push_back(0);
        return v;
      };
      std::string got;
      try {
        const auto r = wide ? S::wstr_format(units(t[1]), args, cs, ws)
                            : S::wstr_format_ascii(bytes(t[1]), args, cs, ws);
        got = uh(r.written) + " " + std::to_string(r.used);
      } catch (const S::Refused &) {
        got = "R";
      }
      std::string want = t[10];
      if (want != "R")
        want += " " + t[11];
      eq(got, want, wide ? "wstr_format" : "wstr_formatAscii");
    } else if (k == "U") {
      const auto &op = t[1];
      if (op == "new") {
        hud.reset();
        continue;
      }
      if (op == "reg") {
        hud.table->register_table(bytes(t[3]), u32(t[2]));
        continue;
      }
      if (op == "obj") {
        hud.objects[str(t[2])] = {u32(t[3]), u32(t[4]), u32(t[5])};
        continue;
      }
      if (op == "in") {
        auto &in = hud.hud->inputs;
        in.suppressed = t[2] == "1";
        in.local_menu = t[3] == "1";
        in.menu_open = t[4] == "1";
        const int cam = std::stoi(t[5]);
        in.camera_mode = cam < 0 ? std::nullopt : std::optional<int>(cam);
        in.loadscreen = t[6] == "1";
        in.warmup = t[7] == "1";
        in.num_players = std::stoi(t[8]);
        in.network = t[9] == "1";
        in.alive.clear();
        if (t[10] != "-") {
          std::istringstream list(t[10]);
          std::string item;
          while (std::getline(list, item, ','))
            in.alive.push_back(std::stoi(item));
        }
        hud.local_mask = u32(t[11]);
        pending_hud = "in";
        continue;
      }
      hud.hud->events.clear();
      bool refused = false;
      try {
        if (op == "setinfo") {
          if (hud.cmds->check_set_info(str(t[2])))
            throw S::Refused("setInfo");
          hud.cmds->set_info(str(t[2]));
        } else if (op == "help") {
          hud.cmds->add_help_message(str(t[2]), str(t[3]));
        } else if (op == "helpf") {
          hud.cmds->add_help_message_f(str(t[2]), u32(t[3]), str(t[4]), u32(t[5]));
        } else if (op == "bust") {
          hud.cmds->show_bust(str(t[2]), str(t[3]), u32(t[4]), u32(t[5]));
        } else if (op == "compass") {
          hud.cmds->show_compass(str(t[2]), str(t[3]));
        } else if (op == "msg") {
          hud.hud->show_message(units(t[2]), u32(t[3]), std::stoi(t[4]), u32(t[5]));
        } else if (op == "tickmsg") {
          hud.hud->tick_message();
        } else if (op == "tickw") {
          hud.hud->tick_widgets();
        }
      } catch (const S::Refused &) {
        refused = true;
      }
      const auto &status = t[t.size() - (t.back() == "R" ? 1 : 2)];
      if (t.back() == "R") {
        ok(refused, "HUD step " + op + " is refused");
      } else {
        ok(!refused, "HUD step " + op + " runs");
        eq(status, std::string("ok"), "status token");
        std::string events;
        for (const auto &e : hud.hud->events)
          events += (events.empty() ? "" : ";") + e.text();
        if (events.empty())
          events = "-";
        eq(events, t.back(), "HUD events of " + op);
      }
      pending_hud = op;
    } else if (k == "E") {
      eq(hud.state(), line, "HUD state after " + pending_hud);
    } else if (k == "S" || k == "SS") {
      if (k == "SS") {
        std::string got = "SS " + h(sound.model->serial);
        for (const auto &s : sound.model->slots)
          got += " " + slot_text(s);
        eq(got, line, "sound slots after " + pending_sound);
        continue;
      }
      const auto &op = t[1];
      if (op == "new") {
        sound.pool = std::make_unique<rta::HandlePool>();
        sound.model = std::make_unique<S::SoundSeqs>(sound.pool.get());
        sound.objects.clear();
      } else if (op == "group") {
        sound.model->groups[u32(t[2])] = static_cast<std::uint8_t>(u32(t[3]));
      } else if (op == "limit") {
        sound.model->limits[u32(t[2])] = static_cast<std::int8_t>(u32(t[3]));
      } else if (op == "serial") {
        sound.model->serial = static_cast<std::uint16_t>(u32(t[2]));
      } else if (op == "slot") {
        auto &s = sound.model->slots[u32(t[2])];
        s.capacity = static_cast<std::uint8_t>(u32(t[3]));
        s.tracks = static_cast<std::uint8_t>(u32(t[4]));
        s.seq = u32(t[5]);
      } else if (op == "obj") {
        auto o = std::make_unique<SoundRun::O>();
        o->object.id = u32(t[2]);
        o->position = {u32(t[3]), u32(t[4]), u32(t[5])};
        sound.objects.push_back(std::move(o));
      } else if (op == "finish") {
        sound.model->slots[u32(t[2])].tracks = 0;
      } else if (op == "kill") {
        for (auto &c : sound.objects[u32(t[2])]->heads)
          sound.pool->clear(c);
        pending_sound = "kill";
      } else if (op == "cut") {
        sound.model->slots[u32(t[2])].track_link = u32(t[3]);
      } else if (op == "play") {
        const int obj = std::stoi(t[3]);
        S::SoundTarget target;
        if (obj >= 0) {
          auto &o = *sound.objects[static_cast<std::size_t>(obj)];
          target = {&o.heads[0], &o.object, o.position};
        }
        sound.model->frozen = static_cast<std::uint8_t>(u32(t[4]));
        const auto played = sound.model->play_on_object(u32(t[2]), obj >= 0 ? &target : nullptr, u32(t[5]));
        eq(h8(played.handle), t[6], "sound handle");
        eq(played.slot ? static_cast<long>(*played.slot) : -1L, std::stol(t[7]), "sound slot");
        eq(h(played.cut_link), t[8], "cut track link");
        pending_sound = "play";
      }
    } else if (k == "SP") {
      eq("SP" + dump_pool(*sound.pool, sound.cells()).substr(1), line, "handle pool after " + pending_sound);
    } else if (k == "CP") {
      eq("CP" + dump_pool(*charm.pool, charm.cells()).substr(1), line, "handle pool after charm step");
    } else if (k == "C") {
      const auto &op = t[1];
      if (op == "new") {
        charm.pool = std::make_unique<rta::HandlePool>();
        charm.creatures.clear();
        for (auto &l : charm.lists)
          l.next = nullptr;
      } else if (op == "creature") {
        auto c = std::make_unique<S::Creature>();
        c->object.id = u32(t[2]);
        c->flags = u32(t[3]);
        c->object.flags = c->flags;
        charm.creatures.push_back(std::move(c));
      } else if (op == "take") {
        auto &c = *charm.creatures[u32(t[2])];
        auto &target = *charm.creatures[u32(t[3])];
        if (t[4] == "1")
          charm.pool->clear(c.target);
        charm.pool->link(c.target, target.heads[0], &target.object);
      } else if (op == "list") {
        auto &target = *charm.creatures[u32(t[3])];
        charm.pool->link(charm.lists[u32(t[2])], target.heads[0], &target.object);
      } else if (op == "kill") {
        for (auto &head : charm.creatures[u32(t[2])]->heads)
          charm.pool->clear(head);
      } else if (op == "charm") {
        auto &c = *charm.creatures[u32(t[2])];
        c.charm = static_cast<std::uint16_t>(u32(t[3]));
        const bool stored = S::set_charm(*charm.pool, c, u32(t[4]), u32(t[5]));
        eq(std::to_string(stored ? 1 : 0), t[6], "setCharm stored");
        eq(h(c.team), t[7], "setCharm team");
        eq(h(c.charm), t[8], "setCharm charm");
      }
    } else {
      std::cerr << "line " << line_no << ": unknown record " << k << "\n";
      ++failures;
    }
  }
  ok(saw_end, "the vectors end with END");
}

// Hand checks from docs/strings.md that the vectors do not reach.
void hand_checks() {
  // 1.2: an entry, the alignment hole, then a key whose first byte is 0 ends.
  const std::vector<std::uint8_t> uni = {'a', 0, 'x', 0, 0, 0, 'b', 'c', 0, 0, 'y', 0, 'z', 0, 0, 0, 0, 0};
  const auto entries = S::uni_entries(uni.data(), uni.size());
  eq(entries.size(), std::size_t(2), "two .uni entries");
  eq(entries[1].text_offset, std::size_t(10), "text starts at (key + strlen + 2) & ~1");
  eq(uh(entries[1].units), std::string("0079007a"), "second text");
  // 1.3: the first copy of a repeated key wins.
  S::TextTable table;
  const std::vector<std::uint8_t> dup = {'k', 0, 'A', 0, 0, 0, 'k', 0, 'B', 0, 0, 0, 0, 0};
  table.register_table(dup, 0x1000);
  eq(uh(table.lookup("k").units), std::string("0041"), "first copy of a repeated key");
  eq(uh(table.lookup("\"k\"").units), std::string("0041"), "quotes are dropped from a request");
  const auto miss = table.lookup("ABCDEFGHIJKLMNOPQRSTUV");
  ok(!miss.hit, "a missing key misses");
  eq(miss.units.size(), std::size_t(19), "NF: plus 16 key bytes");
  eq(miss.address, S::G_NF_RING + 40u, "the first miss takes ring entry 1");
  // 2.3: the port refuses formats that would read past their terminator.
  bool refused = false;
  try {
    S::wstr_format(S::Units{'a', '%', 'd', 0, 'b', 0}, {});
  } catch (const S::Refused &) {
    refused = true;
  }
  ok(refused, "a format ending in %d is refused");
  // 2.2: %f prints two decimals; exponent 255 is a number, not inf.
  eq(S::float_text(0x40400000u), std::string("3.00"), "%f of 3.0");
  ok(S::float_text(0x7f800000u).find("inf") == std::string::npos, "exponent 255 is not inf");
  // 3.3: help_task: 90 frames before a confirm closes the box; offline pause.
  S::Hud hud;
  hud.show_help(-1, 0x1000, 0x2000);
  int frames = 0;
  while (hud.help_open && frames < 200) {
    const auto f = hud.help_task(true);
    ++frames;
    if (!f.closed)
      ok(f.paused, "an open offline help box pauses");
    else
      ok(f.run_post_action && !f.paused, "closing runs the post action and unpauses");
  }
  eq(frames, 90, "the box closes on the 90th displayed frame with confirm held");
  eq(hud.help_close_delay, 60, "closing stores 60 into the delay");
  eq(hud.menu_input_lockout, 60, "and into g_menuInputLockout");
  // A second call while open replaces both pointers: no queue.
  hud.show_help(-1, 1, 2);
  hud.show_help(-1, 3, 4);
  eq(hud.help_title, 3u, "the last call wins (title)");
  eq(hud.help_close_delay, 90, "and re-arms the delay");
  // 3.4: the bust lease: 5 run ticks after the last call.
  S::Hud bust;
  bust.set_bust("t", {'x'}, 1, 0xffffffffu);
  int shown = 0;
  while (bust.bust_shown) {
    bust.tick_widgets();
    ++shown;
  }
  eq(shown, 5, "a bust lasts 5 ticks");
  // 4: group limits observed on the disc.
  const auto limits = S::disc_group_limits();
  eq(int(limits[14]), 9, "group 14 limit");
  eq(int(limits[50]), 8, "group 50 limit");
  eq(int(limits[3]), 2, "ordinary group limit");
}
} // namespace

int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "--hand") {
    hand_checks();
  } else {
    replay(std::cin);
    hand_checks();
  }
  std::cout << "strings: " << checks << " checks, " << failures << " failed\n";
  return failures ? 1 : 0;
}
