// Replays native/tests/dialog_vectors.tsv (tools/generate_dialog_vectors.py,
// t3/research-dialog 334578d tools/ref_dialog.py) on the native dialog machine
// and compares, after every operation, the result, the log, the scripts run,
// the inputs asked for, every state field and every object.
#include "champions/dialog.hpp"
#include "dialog_host_tests.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace dlg = champions::simulation::rta::dialog;
using champions::simulation::rta::Raw;

namespace {
int failures = 0;
std::size_t checks = 0;
std::string where;

void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failures;
    if (failures <= 40)
      std::cerr << "FAIL " << where << ": " << what << "\n";
  }
}

std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}
std::string unesc(const std::string &s) {
  if (s == "-")
    return "";
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      out.push_back(char(std::stoi(s.substr(i + 1, 2), nullptr, 16)));
      i += 2;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}
std::string esc(const std::string &s) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  for (const unsigned char c : s) {
    if (c == '%' || c == ' ' || c == '\t' || c == ',' || c == '[' || c == ']' || c < 0x21 || c > 0x7e) {
      out.push_back('%');
      out.push_back(digits[c >> 4]);
      out.push_back(digits[c & 15]);
    } else {
      out.push_back(char(c));
    }
  }
  return out;
}
std::vector<std::int64_t> ints(const std::string &s) {
  std::vector<std::int64_t> out;
  if (s == "-" || s.empty())
    return out;
  for (const auto &part : split(s, ','))
    out.push_back(std::stoll(part));
  return out;
}
dlg::Bytes unhex(const std::string &s) {
  dlg::Bytes out;
  if (s == "-")
    return out;
  for (std::size_t i = 0; i + 1 < s.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
  return out;
}
std::string hex(const dlg::Bytes &b) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  for (auto c : b) {
    out.push_back(digits[c >> 4]);
    out.push_back(digits[c & 15]);
  }
  return out;
}
template <class T> std::string csv(const T &values) {
  std::string out;
  bool first = true;
  for (const auto v : values) {
    out += (first ? "" : ",") + std::to_string(static_cast<std::int64_t>(v));
    first = false;
  }
  return out.empty() ? "-" : out;
}
std::string fnv(const dlg::Bytes &data) {
  std::uint64_t h = 0xCBF29CE484222325ull;
  for (auto b : data)
    h = (h ^ b) * 0x100000001B3ull;
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
  return buf;
}
std::int64_t s32v(std::int64_t v) { return champions::simulation::rta::s32(static_cast<std::uint32_t>(v & 0xFFFFFFFF)); }

std::map<std::string, std::string> snapshot(const dlg::Dialog &d) {
  std::map<std::string, std::string> out;
  const auto put = [&](const char *k, std::int64_t v) { out[k] = std::to_string(s32v(v)); };
  out["node"] = d.node ? std::to_string(*d.node) : "none";
  put("node_index", d.node_index);
  put("prompt_node", d.prompt_node);
  put("prompt_shown", d.prompt_shown);
  put("revealing", d.revealing);
  put("reveal_done", d.reveal_done);
  put("reveal", d.reveal);
  put("at_end", d.at_end);
  put("more", d.more);
  put("cursor", d.cursor);
  put("cursor_target", d.cursor_target);
  put("scroll", d.scroll);
  put("scroll_target", d.scroll_target);
  put("total", d.total);
  put("cam_move", d.cam_move);
  put("closing", d.closing);
  put("end_sent", d.end_sent);
  put("drawn", d.drawn);
  put("swoop", d.swoop);
  put("side", d.side);
  put("remote", d.remote);
  put("name_len", d.name_len);
  put("queue_mark", d.queue_mark);
  put("idle_anim", d.idle_anim);
  put("mutex", d.mutex);
  put("swoop_done", d.swoop_done);
  put("active_player", d.active_player);
  put("focus_word", d.focus_word);
  put("store_pending", d.store_pending);
  put("text_len", d.text_len);
  put("hold", d.hold);
  put("anm_ready", d.anm_ready);
  put("buf_sel", d.buf_sel);
  put("cur_buf", d.cur_buf);
  put("talking", d.talking);
  put("hud_suppressed", d.hud_suppressed);
  put("shown", d.shown);
  put("last_position", d.last_position);
  out["blend"] = std::to_string(d.blend);
  out["span"] = std::to_string(d.span);
  out["nudge"] = std::to_string(d.nudge);
  out["visible"] = csv(d.visible);
  std::vector<std::int64_t> ys;
  for (auto y : d.choice_y)
    ys.push_back(s32v(y));
  out["choice_y"] = csv(ys);
  out["text"] = csv(d.text);
  out["name"] = csv(d.name);
  out["next_world"] = d.next_world.empty() ? "-" : esc(d.next_world);
  std::string buffers;
  for (std::size_t i = 0; i < d.buffers.size(); ++i) {
    const auto &b = d.buffers[i];
    buffers += (i ? "/" : "") + std::to_string(b.ptr ? 1 : 0) + ":" + std::to_string(b.has) + ":" +
               std::to_string(s32v(b.size)) + ":" + std::to_string(s32v(b.vag));
  }
  out["buffers"] = buffers;
  out["focus"] = std::to_string(d.focus ? d.focus->addr : 0);
  out["store_index"] = d.store_index ? std::to_string(*d.store_index) : "none";
  out["pending_world"] = !d.pending_world ? "none" : (d.pending_world->empty() ? "-" : esc(*d.pending_world));
  out["orient"] = d.orient ? std::to_string(*d.orient) : "none";
  out["nodes"] = d.has_nodes ? std::to_string(d.nodes.size()) : "none";
  return out;
}

std::string obj_state(const dlg::Object &o) {
  std::string store = "-";
  if (o.track_store)
    store = std::to_string(o.track_store->first) + ":" + std::to_string(o.track_store->second);
  return std::to_string(o.flags) + ":" + std::to_string(o.anim_count) + ":" + std::to_string(o.anim_cur) + ":" +
         store + ":" + csv(o.tracks);
}

std::string describe(const std::vector<dlg::Node> &nodes) {
  std::string out;
  for (std::size_t k = 0; k < nodes.size(); ++k) {
    const auto &x = nodes[k];
    const auto e = [](const std::string &s) { return s.empty() ? std::string("-") : esc(s); };
    out += (k ? "|" : "") + e(x.condition) + "/" + e(x.text) + "/" + e(x.voice) + "/" + e(x.speaker) + "/" +
           std::to_string(x.raw_choices) + "/";
    std::string ch;
    for (std::size_t i = 0; i < x.choices.size(); ++i) {
      const auto &c = x.choices[i];
      ch += (i ? ";" : "") + e(c.text) + "," + e(c.condition) + "," + e(c.action) + "," + std::to_string(c.next);
    }
    out += ch.empty() ? "-" : ch;
  }
  return out;
}

struct Case {
  dlg::Host host;
  dlg::Dialog d;
  std::map<std::uint32_t, std::unique_ptr<dlg::Object>> objs;
  std::vector<dlg::Node> nodes;
  std::map<std::string, std::string> expected;
  std::map<std::uint32_t, std::string> expected_objs;
  // last operation
  std::string result = "-";
  bool refused = false;
  std::vector<std::pair<std::string, std::int64_t>> asked;
  bool has_io = false;
  bool dead = false; // a refusal ended the case

  dlg::Object *obj(std::uint32_t addr) {
    if (!addr)
      return nullptr;
    const auto found = objs.find(addr);
    if (found == objs.end())
      throw std::runtime_error("vector names an unknown object");
    return found->second.get();
  }
};

void plant(Case &c, const std::string &spec) {
  auto &d = c.d;
  for (const auto &part : split(spec, ' ')) {
    if (part.empty())
      continue;
    const auto eqpos = part.find('=');
    const auto k = part.substr(0, eqpos), v = part.substr(eqpos + 1);
    if (k == "node") {
      if (v == "none")
        d.node.reset();
      else
        d.node = std::stoi(v);
    } else if (k == "focus") {
      d.focus = c.obj(static_cast<std::uint32_t>(std::stoll(v)));
    } else if (k == "next_world") {
      d.next_world = unesc(v);
    } else if (k.rfind("buf", 0) == 0) {
      auto &b = d.buffers[std::size_t(k[3] - '0')];
      if (k.substr(5) == "has")
        b.has = std::stoi(v);
      else
        b.vag = std::stoll(v);
    } else {
      const auto n = std::stoll(v);
      if (k == "active_player") d.active_player = int(n);
      else if (k == "anm_ready") d.anm_ready = int(n);
      else if (k == "swoop_done") d.swoop_done = int(n);
      else if (k == "mutex") d.mutex = int(n);
      else if (k == "revealing") d.revealing = int(n);
      else if (k == "reveal_done") d.reveal_done = int(n);
      else if (k == "at_end") d.at_end = int(n);
      else if (k == "more") d.more = int(n);
      else if (k == "blend") d.blend = Raw(n);
      else if (k == "scroll") d.scroll = n;
      else if (k == "scroll_target") d.scroll_target = n;
      else if (k == "cursor") d.cursor = n;
      else if (k == "cursor_target") d.cursor_target = n;
      else if (k == "closing") d.closing = int(n);
      else if (k == "store_pending") d.store_pending = int(n);
      else if (k == "cam_move") d.cam_move = int(n);
      else if (k == "hold") d.hold = int(n);
      else if (k == "talking") d.talking = int(n);
      else if (k == "end_sent") d.end_sent = int(n);
      else if (k == "drawn") d.drawn = int(n);
      else if (k == "name_len") d.name_len = n;
      else if (k == "cur_buf") d.cur_buf = int(n);
      else if (k == "idle_anim") d.idle_anim = n;
      else if (k == "last_position") d.last_position = n;
      else if (k == "nudge") d.nudge = Raw(n);
      else throw std::runtime_error("unknown planted field " + k);
    }
  }
}

void set_host(Case &c, const std::string &key, const std::string &value) {
  auto &h = c.host;
  if (key == "network") h.network = value == "1";
  else if (key == "local") { h.local.clear(); for (auto v : ints(value)) h.local.insert(int(v)); }
  else if (key == "num_players") h.num_players = std::stoi(value);
  else if (key == "alive") { h.alive.clear(); for (auto v : ints(value)) h.alive.push_back(int(v)); }
  else if (key == "confirm_mask") h.confirm_mask = std::uint32_t(std::stoul(value));
  else if (key == "recall") h.recall = std::stoi(value);
  else if (key == "mission_menu") h.mission_menu = value == "1";
  else if (key == "pause_menu") h.pause_menu = value == "1";
  else if (key == "shop_open") h.shop_open = std::stoi(value);
  else if (key == "layout_height") h.layout_height = std::stoll(value);
  else if (key == "pads" || key == "players") {
    if (key == "pads") h.raw_pad.clear(); else h.player_obj.clear();
    if (value != "-")
      for (const auto &kv : split(value, ',')) {
        const auto parts = split(kv, ':');
        const int slot = std::stoi(parts[0]);
        const auto v = std::stoll(parts[1]);
        if (key == "pads") h.raw_pad[slot] = std::uint32_t(v);
        else h.player_obj[slot] = c.obj(std::uint32_t(v));
      }
  } else throw std::runtime_error("unknown host field " + key);
}

void verify_objects(Case &c) {
  for (const auto &[addr, o] : c.objs) {
    const auto want = c.expected_objs.find(addr);
    if (want == c.expected_objs.end())
      continue;
    check(want->second == obj_state(*o), "object " + std::to_string(addr) + " is " + obj_state(*o) +
                                             ", reference " + want->second);
  }
}

// Runs one operation on the native machine; a Refused is the reference's ValueError.
template <class F> void run_op(Case &c, F &&fn) {
  c.d.log.clear();
  c.host.ran.clear();
  c.refused = false;
  c.has_io = false;
  c.asked.clear();
  try {
    c.result = fn();
  } catch (const dlg::Refused &) {
    c.refused = true;
    c.result = "-";
  }
}

dlg::TickIO make_io(const std::vector<std::string> &f) {
  dlg::TickIO io;
  io.camera = ints(f[1]);
  io.look = Raw(std::stoll(f[2]));
  io.held = ints(f[3]);
  io.queue = ints(f[4]);
  io.position = ints(f[5]);
  io.status = ints(f[6]);
  io.fade = Raw(std::stoll(f[7]));
  io.text_option = std::stoi(f[8]);
  io.shown = std::stoll(f[9]);
  return io;
}
std::string asked_text(const std::vector<std::pair<std::string, std::int64_t>> &asked) {
  std::string out;
  for (const auto &[k, v] : asked) {
    if (!out.empty())
      out += "\t";
    out += k == "camera" ? k + ":" + std::to_string(v) : k;
  }
  return out;
}

void finish_file(Case &c, std::size_t count, const std::string &want_fnv) {
  check(c.nodes.size() == count, "file node count");
  try {
    const auto data = dlg::build_dbin(c.nodes);
    check(fnv(data) == want_fnv, "build_dbin bytes match the reference's");
    const auto back = dlg::parse_dbin(data.data(), data.size());
    check(describe(back) == describe(c.nodes), "parse_dbin(build_dbin) round trip");
  } catch (const dlg::Refused &e) {
    check(false, std::string("file refused: ") + e.what());
  }
}

int replay(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    std::cerr << "cannot open " << path << "\n";
    return 2;
  }
  std::unique_ptr<Case> c;
  std::string line;
  std::size_t lineno = 0, ops = 0, ticks = 0, cases = 0;
  std::size_t pending_file = 0;
  std::string pending_fnv;
  bool in_file = false;
  bool ended = false;
  while (std::getline(in, line)) {
    ++lineno;
    const auto f = split(line, '\t');
    const auto &kind = f[0];
    if (in_file && kind != "node" && kind != "choice") {
      finish_file(*c, pending_file, pending_fnv);
      in_file = false;
    }
    try {
      if (kind == "policy") {
        check(f[1] == dlg::POLICY, "policy");
        check(f[2] == "334578d", "research commit");
      } else if (kind == "end") {
        ended = true;
      } else if (kind == "case") {
        if (c)
          verify_objects(*c);
        c = std::make_unique<Case>();
        where = f[1] + " " + f[2];
        ++cases;
      } else if (c && c->dead && kind != "log" && kind != "ran" && kind != "state" && kind != "objstate" &&
                 kind != "asked" && kind != "refused" && kind != "ok") {
        continue; // the reference stopped this case at a refusal
      } else if (kind == "obj") {
        auto o = std::make_unique<dlg::Object>();
        o->addr = std::uint32_t(std::stoull(f[1]));
        const auto pos = ints(f[2]);
        for (int i = 0; i < 3; ++i)
          o->pos[std::size_t(i)] = Raw(pos[std::size_t(i)]);
        o->facing = std::uint32_t(std::stoul(f[3]));
        o->flags = std::stoull(f[4]);
        o->npc = f[5] == "1";
        o->word24 = std::uint32_t(std::stoul(f[6]));
        o->anim_count = std::stoi(f[7]);
        o->anim_cur = std::stoll(f[8]);
        o->display_name = std::uint32_t(std::stoul(f[9]));
        o->flag20_track = std::stoll(f[10]);
        const auto tracks = ints(f[11]);
        for (std::size_t i = 0; i < 8; ++i)
          o->tracks[i] = Raw(tracks[i]);
        if (f[12] != "-")
          for (const auto &kv : split(f[12], ';')) {
            const auto eqpos = kv.find('=');
            o->props[unesc(kv.substr(0, eqpos))] = unesc(kv.substr(eqpos + 1));
          }
        c->objs[o->addr] = std::move(o);
      } else if (kind == "objlist") {
        auto &list = c->host.objects[unesc(f[1])];
        list.clear();
        for (auto a : ints(f[2]))
          list.push_back(c->obj(std::uint32_t(a)));
      } else if (kind == "host") {
        set_host(*c, f[1], f[2]);
      } else if (kind == "script") {
        c->host.scripts[unesc(f[1])] = std::int32_t(std::stol(f[2]));
      } else if (kind == "text") {
        dlg::Wide units;
        for (auto u : ints(f[2]))
          units.push_back(std::uint16_t(u));
        c->host.texts[unesc(f[1])] = units;
      } else if (kind == "role") {
        c->host.role_anims[{std::uint32_t(std::stoull(f[1])), unesc(f[2])}] = std::stoll(f[3]);
      } else if (kind == "anm_length") {
        c->host.anm_length[unesc(f[1])] = std::stoll(f[2]);
      } else if (kind == "vag_length") {
        c->host.vag_length[unesc(f[1])] = std::stoll(f[2]);
      } else if (kind == "anm_entry") {
        c->host.anm_entry[unesc(f[1])] = {std::stoll(f[2]), std::stoll(f[3])};
      } else if (kind == "file") {
        c->nodes.clear();
        pending_file = std::stoul(f[1]);
        pending_fnv = f[2];
        in_file = true;
      } else if (kind == "node") {
        dlg::Node n;
        n.condition = unesc(f[1]);
        n.text = unesc(f[2]);
        n.voice = unesc(f[3]);
        n.speaker = unesc(f[4]);
        n.raw_choices = std::stoi(f[5]);
        c->nodes.push_back(n);
      } else if (kind == "choice") {
        c->nodes.back().choices.push_back(dlg::Choice{unesc(f[1]), unesc(f[2]), unesc(f[3]), std::stoi(f[4])});
      } else if (kind == "dialog") {
        verify_objects(*c);
        run_op(*c, [&] {
          plant(*c, f.size() > 1 ? f[1] : "");
          return std::string("-");
        });
      } else if (kind == "objflags") {
        c->obj(std::uint32_t(std::stoull(f[1])))->flags = std::stoull(f[2]);
      } else if (kind == "msg") {
        verify_objects(*c);
        ++ops;
        const auto msg = unhex(f[1]);
        run_op(*c, [&] {
          const bool r = c->d.on_message(c->host, msg, &c->nodes);
          return msg[0] == dlg::MSG_OPEN ? std::string(r ? "1" : "0") : std::string("-");
        });
      } else if (kind == "tick") {
        verify_objects(*c);
        ++ops;
        ++ticks;
        auto io = make_io(f);
        run_op(*c, [&] {
          const auto sent = c->d.tick(c->host, io);
          std::string out;
          for (const auto &m : sent)
            out += (out.empty() ? "" : ",") + hex(m);
          return out.empty() ? std::string("-") : out;
        });
        c->asked = io.asked;
        c->has_io = true;
      } else if (kind == "draw") {
        verify_objects(*c);
        ++ops;
        dlg::TickIO io;
        io.status = {std::stoll(f[4])};
        run_op(*c, [&] {
          c->d.draw(c->host, Raw(std::stoll(f[1])), std::stoi(f[2]), std::stoi(f[3]),
                    [&io] { return io.stream_status(); });
          return std::string("-");
        });
        c->asked = io.asked;
        c->has_io = true;
      } else if (kind == "free") {
        ++ops;
        run_op(*c, [&] {
          c->d.free();
          return std::string("-");
        });
      } else if (kind == "start") {
        ++ops;
        const auto cam = ints(f[4]);
        const std::array<Raw, 3> raw{Raw(cam[0]), Raw(cam[1]), Raw(cam[2])};
        run_op(*c, [&] {
          const auto msg = c->d.start(c->host, unesc(f[1]), std::stoi(f[2]), std::stoi(f[3]), raw,
                                      std::stoi(f[5]), std::stoi(f[6]));
          return msg ? hex(*msg) : std::string("-");
        });
      } else if (kind == "startmsg") {
        const auto cam = ints(f[5]);
        const std::array<Raw, 3> raw{Raw(cam[0]), Raw(cam[1]), Raw(cam[2])};
        const auto msg = dlg::start_message(unesc(f[1]), std::stoi(f[2]), std::stoi(f[3]), std::stoi(f[4]), raw,
                                            std::stoi(f[6]), std::stoi(f[7]));
        check(hex(msg) == f[8], "start_message bytes");
        const auto m = dlg::parse_start_message(msg);
        const auto mc = ints(f[13]);
        check(m.name == unesc(f[9]) && m.player == std::stoi(f[10]) && m.a == std::stoi(f[11]) &&
                  m.b == std::stoi(f[12]) && Raw(mc[0]) == m.cam[0] && Raw(mc[1]) == m.cam[1] &&
                  Raw(mc[2]) == m.cam[2] && m.yaw == std::stoi(f[14]) && m.pitch == std::stoi(f[15]),
              "parse_start_message fields");
      } else if (kind == "interact" || kind == "trigger") {
        ++ops;
        auto *o = c->obj(std::uint32_t(std::stoull(f[1])));
        run_op(*c, [&] {
          return esc(kind == "trigger" ? c->d.trigger(c->host, *o)
                                       : c->d.interact(c->host, *o, std::stoi(f[2]), f[3] == "1"));
        });
      } else if (kind == "store") {
        run_op(*c, [&] { c->d.activate_store(c->host, std::stoll(f[1])); return std::string("-"); });
      } else if (kind == "menu") {
        run_op(*c, [&] { c->d.activate_mission_menu(c->host, std::stoi(f[1])); return std::string("-"); });
      } else if (kind == "swoop") {
        run_op(*c, [&] { c->d.change_swoop(std::uint32_t(std::stoull(f[1]))); return std::string("-"); });
      } else if (kind == "swoopdone") {
        run_op(*c, [&] { c->d.start_with_swoop_done(); return std::string("-"); });
      } else if (kind == "nextworld") {
        run_op(*c, [&] { c->d.new_world_post(unesc(f[1])); return std::string("-"); });
      } else if (kind == "running") {
        run_op(*c, [&] { return std::string(c->d.is_open() ? "1" : "0"); });
      } else if (kind == "release") {
        run_op(*c, [&] { c->d.mutex_release(); return std::string("-"); });
      } else if (kind == "talk") {
        verify_objects(*c);
        run_op(*c, [&] {
          const auto store = c->d.talk_anim(c->host);
          return store ? std::to_string(store->first) + ":" + std::to_string(store->second) : std::string("-");
        });
      } else if (kind == "sync") {
        verify_objects(*c);
        const auto pos = std::stoll(f[1]);
        run_op(*c, [&] { c->d.sync_mouth(c->host, [pos] { return pos; }); return std::string("-"); });
      } else if (kind == "ok" || kind == "refused") {
        const bool want_refused = kind == "refused";
        check(c->refused == want_refused, want_refused ? "the reference refused here" : "native refused here");
        if (!want_refused)
          check(c->result == f[1], "result " + c->result + ", reference " + f[1]);
        if (want_refused)
          c->dead = true;
      } else if (kind == "log") {
        std::vector<std::string> got;
        for (const auto &e : c->d.log)
          got.push_back(dlg::format_event(e));
        std::vector<std::string> want(f.begin() + 1, f.end());
        if (want.size() == 1 && want[0].empty())
          want.clear();
        if (got != want) {
          std::string g, w;
          for (const auto &x : got) g += " | " + x;
          for (const auto &x : want) w += " | " + x;
          check(false, "log\n   native   " + g + "\n   reference" + w);
        } else {
          check(true, "log");
        }
      } else if (kind == "ran") {
        std::vector<std::string> want;
        for (std::size_t i = 1; i < f.size(); ++i)
          if (!f[i].empty())
            want.push_back(unesc(f[i]));
        check(c->host.ran == want, "scripts run");
      } else if (kind == "asked") {
        std::string want;
        for (std::size_t i = 1; i < f.size(); ++i)
          want += (i > 1 ? "\t" : "") + f[i];
        check(c->has_io && asked_text(c->asked) == want,
              "inputs asked: native " + asked_text(c->asked) + ", reference " + want);
      } else if (kind == "state") {
        if (f.size() > 1)
          for (const auto &part : split(f[1], ' ')) {
            if (part.empty())
              continue;
            const auto eqpos = part.find('=');
            c->expected[part.substr(0, eqpos)] = part.substr(eqpos + 1);
          }
        const auto got = snapshot(c->d);
        for (const auto &[k, v] : c->expected) {
          const auto g = got.find(k);
          check(g != got.end() && g->second == v,
                "field " + k + " is " + (g == got.end() ? "?" : g->second) + ", reference " + v);
        }
        check(got.size() == c->expected.size(), "snapshot field count");
      } else if (kind == "objstate") {
        c->expected_objs[std::uint32_t(std::stoull(f[1]))] = f[2];
      } else if (kind == "dbin") {
        if (f[1] == "-") {
          bool refused = false;
          try { dlg::parse_dbin(nullptr, 0); } catch (const dlg::Refused &) { refused = true; }
          check(refused, "empty .dbin refused");
          continue;
        }
        const auto data = unhex(f[1]);
        bool refused = false;
        std::vector<dlg::Node> parsed;
        try { parsed = dlg::parse_dbin(data.data(), data.size()); } catch (const dlg::Refused &) { refused = true; }
        if (f[2] == "refused") {
          check(refused, "corrupted .dbin refused");
        } else {
          check(!refused, ".dbin accepted");
          if (!refused) {
            check(std::to_string(parsed.size()) == f[3], ".dbin node count");
            check(csv(dlg::start_candidates(parsed)) == f[4], "start candidates");
            check(csv(dlg::reachable_nodes(parsed)) == f[5], "reachable nodes");
            check(describe(parsed) == f[6], ".dbin fields");
            check(hex(dlg::build_dbin(parsed)) == f[1], "build_dbin(parse_dbin) bytes");
          }
        }
      } else if (kind == "pick") {
        std::vector<dlg::Node> nodes(3);
        std::map<std::string, std::int32_t> scripts;
        const auto results = split(f[1], ',');
        for (int i = 0; i < 3; ++i) {
          nodes[std::size_t(i)].condition = "c" + std::to_string(i);
          if (results[std::size_t(i)] != "-")
            scripts["c" + std::to_string(i)] = std::stoi(results[std::size_t(i)]);
        }
        std::vector<std::string> ran;
        const int start = dlg::pick_start(nodes, [&](const std::string &n) {
          ran.push_back(n);
          const auto it = scripts.find(n);
          return it == scripts.end() ? dlg::MISSING_ENTRY : it->second;
        });
        std::string r;
        for (const auto &x : ran) r += (r.empty() ? "" : ",") + x;
        check(std::to_string(start) == f[2] && r == f[3], "pick_start");
      } else {
        throw std::runtime_error("unknown vector line kind " + kind);
      }
    } catch (const std::exception &e) {
      check(false, "line " + std::to_string(lineno) + ": " + e.what());
    }
  }
  if (c)
    verify_objects(*c);
  check(ended, "vector file ends with its end line");
  std::cout << "dialog vectors: " << lineno << " lines, " << cases << " cases, " << ops << " operations, "
            << ticks << " ticks\n";
  return 0;
}

void test_contract(const std::string &path, std::size_t lines) {
  std::ifstream in(path);
  std::stringstream text;
  text << in.rdbuf();
  const auto s = text.str();
  where = "contract";
  check(s.find("\"commit\": \"334578d") != std::string::npos, "contract commit");
  check(s.find("\"policy\": \"rta-dialog-v1\"") != std::string::npos, "contract policy");
  check(s.find("tools/ref_dialog.py") != std::string::npos, "contract lists the reference");
  check(s.find("docs/dialog.md") != std::string::npos, "contract lists the dialog doc");
  check(s.find("\"lines\": " + std::to_string(lines) + ",") != std::string::npos,
        "vector line count matches the contract");
}

std::size_t count_lines(const std::string &path) {
  std::ifstream in(path);
  std::size_t n = 0;
  std::string line;
  while (std::getline(in, line))
    ++n;
  return n;
}

void test_quirks() {
  where = "quirks";
  // Refusals the reference states as raises (docs/DIALOG.md "Refusals").
  bool refused = false;
  try {
    dlg::build_dbin(std::vector<dlg::Node>(128));
  } catch (const dlg::Refused &) {
    refused = true;
  }
  check(refused, "128 nodes refused");
  dlg::Node big;
  big.text = "t";
  big.choices.resize(13);
  big.raw_choices = 13;
  refused = false;
  try {
    dlg::build_dbin({big});
  } catch (const dlg::Refused &) {
    refused = true;
  }
  check(refused, "13 choices refused");
  check(dlg::prop_int(" 12x") == 12 && dlg::prop_int("-3") == -3 && dlg::prop_int("x") == 0 &&
            dlg::prop_int("+0") == 0,
        "prop_int atoi rules");
  refused = false;
  try {
    dlg::prop_int("\xb2");
  } catch (const dlg::Refused &) {
    refused = true;
  }
  check(refused, "superscript digit refused as the reference's int() does");
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: champions-dialog-tests VECTORS CONTRACT\n";
    return 2;
  }
  const auto rc = replay(argv[1]);
  if (rc)
    return rc;
  test_contract(argv[2], count_lines(argv[1]));
  test_quirks();
  run_host_tests(check, where);
  std::cout << "dialog: " << checks << " checks, " << failures << " failed\n";
  return failures ? 1 : 0;
}
