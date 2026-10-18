#include "champions/level_flow.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace champions::simulation::rta::level_flow {
namespace {
std::string lower(std::string s) {
  for (auto &c : s)
    if (c >= 'A' && c <= 'Z')
      c = char(c + 'a' - 'A');
  return s;
}
std::string upper(std::string s) {
  for (auto &c : s)
    if (c >= 'a' && c <= 'z')
      c = char(c + 'A' - 'a');
  return s;
}
bool starts(const std::string &s, const std::string &p) {
  return s.compare(0, p.size(), p) == 0;
}
void count(unsigned n) {
  if (n > 4)
    throw Error("player count exceeds four");
}
void name_limit(const std::string &s, std::size_t n) {
  if (s.size() >= n || s.find('\0') != std::string::npos)
    throw Error("name exceeds fixed buffer or contains NUL");
}
int atoi_prefix(const std::optional<std::string> &v) {
  if (!v)
    return 0;
  std::size_t p = v->find_first_not_of(" \t\n\r\f\v");
  if (p == std::string::npos)
    return 0;
  bool neg = false;
  if ((*v)[p] == '+' || (*v)[p] == '-') {
    neg = (*v)[p] == '-';
    ++p;
  }
  std::uint32_t n = 0;
  while (p < v->size() && (*v)[p] >= '0' && (*v)[p] <= '9') {
    unsigned digit = unsigned((*v)[p++] - '0');
    if (n > (std::uint32_t(INT32_MAX) + unsigned(neg) - digit) / 10)
      throw Error("integer property outside signed32");
    n = n * 10 + digit;
  }
  return s32(neg ? 0u - n : n);
}
std::uint32_t read(Bytes b, std::size_t p, unsigned n) {
  if (p > b.size || n > b.size - p)
    throw Error("truncated level data");
  std::uint32_t v = 0;
  for (unsigned i = 0; i < n; ++i)
    v |= std::uint32_t(b.data[p + i]) << (8 * i);
  return v;
}
std::string string_at(Bytes b, std::size_t p, std::size_t end) {
  if (p >= end || end > b.size)
    throw Error("invalid string range");
  auto q = p;
  while (q < end && b.data[q])
    ++q;
  if (q == end)
    throw Error("unterminated string");
  return std::string(reinterpret_cast<const char *>(b.data + p), q - p);
}
const Act &act_rows(const State &s) {
  static const Act empty;
  return s.act >= 0 && std::size_t(s.act) < s.acts.size() ? s.acts[s.act]
                                                          : empty;
}
} // namespace
std::pair<std::string, std::string> split_request(const std::string &s) {
  auto p = s.find('_');
  return p == std::string::npos
             ? std::make_pair(s, std::string{})
             : std::make_pair(s.substr(0, p), s.substr(p + 1));
}
std::string qualified_name(const std::string &b, const std::string &s) {
  return s.empty() ? b : b + "_" + s;
}
std::string cd_file_name(const std::string &s) { return upper(s) + ";1"; }
std::string loading_picture_alias(const std::string &b) {
  if (b.find("blo") != std::string::npos)
    return "blood1";
  if (b == "innovat2")
    return "innova2";
  if (b.find("val") != std::string::npos)
    return "valor";
  if (b.find("bas") != std::string::npos)
    return "bastion1";
  if (b.find("nitdung") != std::string::npos) {
    if (b == "nitdung1" || b == "nitdung2" || b == "nitdung4")
      return "nitdun" + b.substr(7);
    return b;
  }
  if (b.find("torme") != std::string::npos) {
    if (b == "tormenta" || b == "tormentb" || b == "torment2" ||
        b == "torment3")
      return "torm" + b.substr(7);
    return b;
  }
  return b == "pvpfire2" ? "pvpfir2" : b;
}
std::vector<std::string> loading_picture_files(const std::string &b) {
  return {("l" + loading_picture_alias(b)).substr(0, 8) + ".lmp", "lmiss.lmp"};
}
LevelRow find_level_row(const std::vector<LevelRow> &rows, const std::string &b,
                        const std::string &s) {
  for (auto &r : rows)
    if (r.name == b + "_" + s)
      return r;
  for (auto &r : rows)
    if (starts(r.name, b))
      return r;
  for (auto &r : rows)
    if (b.find(r.name) != std::string::npos)
      return r;
  return {};
}
Raw monster_level(const LevelRow &r, int d, const std::string &b, bool pvp) {
  ScopedFpu scope(Fpu{});
  Raw v = from_int(std::uint32_t(int(r.monster_level) + 2));
  if (pvp && d == 1)
    v = 0x40400000;
  if (d >= 2 && d <= 5)
    v = add(mul(from_int(std::uint32_t(int(r.monster_level) + 2)), 0x3f400000),
            from_int(34 + 27 * (d - 2)));
  if (b == "stomedal")
    v = ONE;
  else if (b == "innpit")
    v = TWO;
  return v;
}
std::vector<Resource> level_manifest(const ManifestInput &i) {
  std::set<std::string> disc, lumps(i.gob_lumps.begin(), i.gob_lumps.end());
  for (auto &s : i.disc_files)
    disc.insert(upper(s));
  lumps.insert(i.loaded_lumps.begin(), i.loaded_lumps.end());
  std::vector<Resource> out;
  auto emit = [&](std::string step, std::string n, std::string where,
                  bool required, bool present = true) {
    out.push_back({step, n, where, required});
    if (required && !present)
      throw MissingResource(n + " (" + where + ", step " + step + ")");
  };
  auto has = [&](const std::string &s) {
    return disc.count(upper(s)) || lumps.count(s);
  };
  if (i.base != "intro" && !i.hud_loaded)
    for (auto s : {"fx.lmp", "hud.lmp", "mapicons.lmp", "icons.lmp"})
      emit("4a", s, "disc", true, has(s));
  if (i.base != "intro")
    emit("4b", "player model lumps", "disc", true);
  auto pics = loading_picture_files(i.base);
  emit("5", disc.count(upper(pics[0])) ? pics[0] : pics[1], "disc", false);
  emit("5", "loadmisc.lmp", "disc", true, has("loadmisc.lmp"));
  emit("7", i.base + ".gob", "disc", true, disc.count(upper(i.base + ".gob")));
  emit("7", i.base + ".tex", "disc", true, disc.count(upper(i.base + ".tex")));
  emit("7", i.base + ".lmp", "gob", true, lumps.count(i.base + ".lmp"));
  auto world = first_entry_with_extension(i.level_entries, "world");
  emit("8", world ? *world : "*.world", i.base + ".lmp", true, bool(world));
  if (!i.recall_leg)
    emit("10", "dialog.bin", i.base + ".lmp", false);
  emit("11", "ambient.bin", i.base + ".lmp", false);
  if (i.pvp)
    for (auto s : {"arena.lmp", "pvpfont.lmp"})
      emit("14", s, "disc", true, has(s));
  if (i.base != "intro")
    for (auto s : {"gooddog.lmp", "skele_b.lmp"})
      emit("15", s, "disc", true, has(s));
  for (auto &s : i.model_lumps)
    emit("16", s + ".lmp", "gob or disc", true, has(s + ".lmp"));
  emit("17", "dome.lmp", "gob", false);
  emit("18", "waterenv.lmp", "gob", true, lumps.count("waterenv.lmp"));
  if (i.base == "hub")
    emit("26", "msel.lmp", "disc", true, has("msel.lmp"));
  emit("28", "script.scr", i.base + ".lmp", false);
  emit("28", "objects.ob", i.base + ".lmp", true,
       std::find(i.level_entries.begin(), i.level_entries.end(),
                 "objects.ob") != i.level_entries.end());
  emit("32", "env.lmp", "gob", false);
  return out;
}
namespace {
bool has_extension(const std::string &name, const std::string &ext) {
  auto dot = name.rfind('.');
  return dot != std::string::npos && name.compare(dot + 1, std::string::npos,
                                                  ext) == 0;
}
} // namespace
std::optional<std::string>
first_entry_with_extension(const std::vector<std::string> &names,
                           const std::string &ext, unsigned n) {
  for (auto &name : names)
    if (has_extension(name, ext) && n-- == 0)
      return name;
  return {};
}
const Entry *first_entry_with_extension(const std::vector<Entry> &entries,
                                        const std::string &ext, unsigned n) {
  for (auto &e : entries)
    if (has_extension(e.name, ext) && n-- == 0)
      return &e;
  return nullptr;
}
std::uint32_t base_light_level(std::uint32_t flags) {
  std::uint32_t level = 0x80;
  if (flags & LEVEL_FLAG_LIGHT_20)
    level = 0x20;
  if (flags & LEVEL_FLAG_LIGHT_40)
    level = 0x40;
  return level;
}
ChunkLight chunk_light(std::uint32_t flags) {
  ChunkLight r;
  r.flags = flags & ~std::uint32_t(0x100);
  if (flags & 0x100)
    r = {false, -230, r.flags};
  else if (flags & 0x2000)
    r = {false, 230, r.flags};
  else if (flags & 0x200)
    r = {false, 0, r.flags};
  return r;
}
std::vector<Entry> gob_entries(Bytes b) {
  if (b.size < 0x800)
    throw Error("truncated GOB directory");
  std::vector<Entry> out;
  std::uint64_t offset = 0x800;
  for (std::size_t p = 0; p + 0x28 <= 0x800; p += 0x28) {
    if (!b.data[p])
      return out;
    auto n = string_at(b, p, p + 0x20);
    auto size = read(b, p + 0x24, 4);
    if (offset > b.size || size > b.size - offset || offset > UINT32_MAX)
      throw Error("GOB sequential payload out of range");
    if (n == "gaydark1.lmp")
      n = "faydark1.lmp";
    else if (n == "gaydark2.lmp")
      n = "faydark2.lmp";
    out.push_back({n, std::uint32_t(offset), size});
    offset += (std::uint64_t(size) + 0x7ff) & ~std::uint64_t(0x7ff);
  }
  throw Error("GOB directory lacks terminator");
}
std::optional<std::string> Record::prop(const std::string &key) const {
  for (auto &p : props)
    if (starts(p, key + "="))
      return p.substr(key.size() + 1);
  return {};
}
std::vector<Record> parse_objects(Bytes b) {
  auto n = s16(read(b, 0, 2));
  auto table = read(b, 4, 4);
  if (n < 0 || table > b.size || table < 8)
    throw Error("invalid objects header");
  std::vector<Record> out;
  std::size_t p = 8;
  auto str = [&](std::uint32_t off) {
    if (off > b.size - table)
      throw Error("object string offset out of range");
    return string_at(b, table + off, b.size);
  };
  for (int i = 0; i < n; ++i) {
    auto size = s16(read(b, p + 4, 2));
    if (size < 24 || p > table || std::size_t(size) > table - p || size % 4)
      throw Error("invalid object record size");
    Record r;
    r.cls = str(read(b, p, 4));
    r.angle = std::uint16_t(read(b, p + 6, 2));
    for (unsigned j = 0; j < 3; ++j)
      r.position[j] = read(b, p + 8 + j * 4, 4);
    bool terminated = false;
    for (std::size_t q = p + 20; q < p + size; q += 4) {
      auto off = read(b, q, 4);
      if (!off) {
        terminated = true;
        break;
      }
      r.props.push_back(str(off));
    }
    if (!terminated)
      throw Error("object properties lack terminator");
    out.push_back(std::move(r));
    p += size;
  }
  return out;
}
bool save_filter_admits(const std::string &s) {
  static const std::set<std::string> names = {"lantern",
                                              "torch",
                                              "firepot",
                                              "firepot2",
                                              "fire",
                                              "kelethinbomb",
                                              "smokebomb",
                                              "faytree",
                                              "line",
                                              "blocker",
                                              "cdeyeball",
                                              "customvars",
                                              "charambient_light",
                                              "chardirectional_light",
                                              "chardirectional_lightd",
                                              "clearcolor",
                                              "zzroutepoint"};
  return names.count(lower(s));
}
std::pair<bool, int> marker_decision(int rank, const Record &r,
                                     const std::string &prev,
                                     const std::string &entrance, bool recall) {
  if (rank == 3)
    return {false, rank};
  bool take = false;
  if (rank <= 0 || (rank == 1 && atoi_prefix(r.prop("default")) != 0)) {
    take = true;
    rank = 1;
  }
  auto src = r.prop("from"), name = r.prop("name");
  if (src) {
    if (*src == prev && !recall) {
      take = true;
      rank = 2;
    } else if (recall && *src == "gate") {
      take = true;
      rank = 3;
    }
  }
  if (name && !recall && *name == entrance) {
    take = true;
    rank = 3;
  }
  return {take, rank};
}
std::vector<std::size_t> run_markers(SpawnState &s,
                                     const std::vector<Record> &rs,
                                     const std::string &prev,
                                     const std::string &entrance, bool recall,
                                     const FindSpot &spot, const Draw &draw,
                                     bool saved, bool gate, unsigned players) {
  count(players);
  s.rank = saved ? 3 : 0;
  std::vector<std::size_t> took;
  for (std::size_t i = 0; i < rs.size(); ++i) {
    auto &r = rs[i];
    auto cls = lower(r.cls);
    if (cls == "player1") {
      auto d = marker_decision(s.rank, r, prev, entrance, recall);
      s.rank = d.second;
      if (d.first) {
        took.push_back(i);
        for (unsigned j = 0; j < 4; ++j) {
          s.pos[j] = spot(r.position, 0x42100000, 0x41c00000, false);
          s.facing[j] = std::uint16_t(r.angle + draw() % 4000 - 2000);
        }
      }
    } else if ((cls == "portal" || cls == "portalhurt") && gate) {
      took.push_back(i);
      for (unsigned j = 0; j < players; ++j) {
        s.pos[j] = spot(r.position, 0x42100000, 0x41c00000, true);
        s.facing[j] = draw();
      }
      s.rank = 3;
    }
  }
  return took;
}
std::vector<unsigned> place_players(SpawnState &s, const Placement &p,
                                    const Height &h, const FindSpot &spot) {
  count(p.players);
  if (p.restore_xy)
    for (unsigned i = 0; i < p.players; ++i) {
      s.pos[i][0] = (*p.restore_xy)[i][0];
      s.pos[i][1] = (*p.restore_xy)[i][1];
    }
  for (unsigned i = 0; i < p.players; ++i)
    s.pos[i][2] = h(0x41400000, s.pos[i]);
  std::vector<unsigned> built;
  if (!p.level_loaded || p.pvp)
    return built;
  for (unsigned i = 0; i < p.players; ++i) {
    if (i && s.rank != 3) {
      s.pos[i] = spot(s.pos[i], 0x42100000, 0x41600000, false);
      s.facing[i] = std::uint16_t(s.facing[i] + 8000);
      s.pos[i][2] = h(0x41400000, s.pos[i]);
    }
    if (p.alive[i] && p.active[i] && (!p.medal_slot || *p.medal_slot == i))
      built.push_back(i);
  }
  return built;
}
void Pending::request(const std::string &s, std::uint8_t o) {
  name_limit(s, 0x50);
  name = s;
  orientation = o;
}
std::uint8_t orientation_from_props(const std::vector<std::string> &ps) {
  std::uint8_t result = 0xff;
  for (auto &p : ps)
    if (starts(p, "orientation="))
      result = std::uint8_t(atoi_prefix(p.substr(12)));
  return result;
}
std::pair<bool, int> new_world_gate(bool net, int active, int cooldown,
                                    int alive, bool post) {
  if (post || !net || active == -1)
    return {false, cooldown};
  if (cooldown && alive >= 2)
    return {true, cooldown};
  return {false, 600};
}
bool cancel_pending(bool loaded, bool frontend, bool busy, bool pvp,
                    bool alive) {
  return loaded && !frontend && !busy && !pvp && !alive;
}
int find_act(const Acts &acts, const std::string &name, int current) {
  auto matches = [&](int a) {
    for (auto &r : acts[a])
      if (starts(name, r.first))
        return true;
    return false;
  };
  if (current >= 0 && std::size_t(current) < acts.size() && matches(current))
    return current;
  for (std::size_t a = 0; a < acts.size(); ++a)
    if (matches(int(a)))
      return int(a);
  return -1;
}
bool LevelStates::store(const std::string &name, const Act &rows, int card,
                        std::uint32_t flags, bool fixed, const Blob &blob) {
  if (card == 4 || (flags & 0x800))
    return false;
  name_limit(name, 0x14);
  std::uint32_t size = 35000;
  if (!fixed) {
    auto i = starts(name, "hub")
                 ? rows.begin()
                 : std::find_if(rows.begin(), rows.end(),
                                [&](auto &r) { return r.first == name; });
    if (i == rows.end())
      return false;
    size = i->second;
  }
  std::size_t offset = 0;
  auto it = entries.begin();
  for (; it != entries.end(); ++it) {
    if (it->first == name)
      break;
    offset += it->second;
  }
  if (it == entries.end()) {
    if (entries.size() >= 10)
      throw Error("11th level state would overflow original directory");
    entries.push_back({name, size});
    it = entries.end() - 1;
  }
  if (offset <= LIMIT && it->second <= LIMIT - offset) {
    if (blob.size() > it->second)
      throw Error("serialized state exceeds act allocation");
    blobs[name] = blob;
    return true;
  }
  return false;
}
std::pair<bool, std::size_t>
LevelStates::lookup(const std::string &name) const {
  std::size_t off = 0;
  for (auto &e : entries) {
    if (e.first == name)
      return {blobs.count(name) != 0, off};
    off += e.second;
  }
  return {false, off};
}
void GateList::reset(const std::string &hub) {
  name_limit(hub, 0x20);
  names = {hub};
}
bool GateList::add_current(const std::string &q, const std::string &hub) {
  name_limit(q, 0x20);
  if (std::find(names.begin(), names.end(), q) != names.end())
    return false;
  if (names.size() >= 15)
    throw Error("16th destination would overwrite original count");
  names.push_back(q);
  auto i = std::find_if(names.begin(), names.end(), [&](auto &s) {
    return s.find("hub") != std::string::npos && s != hub;
  });
  if (i != names.end())
    names.erase(i);
  return true;
}
std::vector<std::string> GateList::choosable(const std::string &s) const {
  std::vector<std::string> out;
  for (auto &n : names)
    if (n != s)
      out.push_back(n);
  return out;
}
GatePrompt gate_prompt(bool found, bool menu, bool script, std::size_t n,
                       const std::vector<Raw> &d2) {
  if (menu || !script)
    return {false, {}, found};
  if (n == 1)
    return {false, "JumpGateVirgin", true};
  for (auto d : d2)
    if (lt(0x46100000, d))
      return {false, "JumpGateNeedPlayers", true};
  return {true, "JumpGate", true};
}
void GateTravel::open(bool net) {
  if (ticks)
    throw Error("gate already travelling");
  menu_open = true;
  paused = !net;
}
bool GateTravel::confirm(const std::string &s, const std::string &current,
                         const GateList &g, bool f) {
  if (!menu_open || s == current)
    return false;
  if (!f && std::find(g.names.begin(), g.names.end(), s) == g.names.end())
    return false;
  name_limit(s, 0x50);
  destination = s;
  fixed = f;
  ticks = 1;
  request_written = false;
  menu_open = false;
  paused = false;
  return true;
}
bool GateTravel::tick(Pending &p) {
  if (!ticks || request_written)
    return false;
  ++ticks;
  if (ticks == 181) {
    p.request(destination);
    request_written = true;
    return true;
  }
  return false;
}
void GateTravel::arrived() { *this = {}; }
Trigger::Trigger(Raw x, Raw y, std::int32_t w, std::int32_t h, std::string t)
    : script(std::move(t)) {
  ScopedFpu scope(Fpu{});
  auto x1 = add(x, from_int(std::uint32_t(w))),
       y1 = sub(y, from_int(std::uint32_t(h)));
  min = {fmin(x, x1), fmin(y, y1), 0};
  max = {fmax(x, x1), fmax(y, y1), 0};
}
bool Trigger::contains(const Vec3 &p) const {
  return le(min[0], p[0]) && le(p[0], max[0]) && le(min[1], p[1]) &&
         le(p[1], max[1]);
}
TriggerEvent Trigger::run(const std::vector<TriggerPlayer> &players,
                          bool recall) {
  if (recall)
    return {};
  std::optional<unsigned> first;
  for (auto &p : players)
    if (contains(p.position) && (!first || p.slot < *first))
      first = p.slot;
  bool now = bool(first);
  if (now == inside)
    return {};
  inside = now;
  return {now ? script : "not" + script, first};
}
TriggerState Trigger::save() const { return {delay, inside}; }
void Trigger::restore(TriggerState state) {
  delay = state.delay;
  inside = state.inside;
}
bool push_trigger_allowed(const std::vector<int> &r) {
  return std::find(r.begin(), r.end(), 0) == r.end();
}
bool PushTrigger::prompt(
    bool net, unsigned slot, unsigned first,
    const std::function<int(const std::string &)> &predicate) const {
  if (deleted || (net && net_host_only && slot != first))
    return false;
  for (auto &key : keys)
    if (!predicate(key))
      return false;
  return true;
}
bool PushTrigger::interact(
    bool busy, const std::function<void(const std::string &, bool)> &run) {
  if (deleted || (shop && busy))
    return false;
  for (auto &a : actions)
    run(a, !off_only);
  if (once)
    deleted = true;
  return true;
}
ChangeResult
begin_change(State &s, const ChangeOptions &o,
             const std::function<void(const std::string &)> &event) {
  if (s.pending.name.empty())
    throw Error("no pending level");
  ChangeResult r;
  s.sim_frame = 0;
  if (!s.arrived_by_recall && !s.restore_xy)
    s.ambient_countdown = 60;
  if (!s.level.empty() && !o.death_task)
    r.stored = s.states.store(qualified_name(s.level, s.sub), act_rows(s),
                              o.card_state, s.level_flags, o.fixed_state_size,
                              o.old_blob);
  if (event && !o.death_task)
    event("restore_medal_player_if_saved");
  if (!s.frontend) {
    auto a = find_act(s.acts, s.pending.name, s.act);
    if (a >= 0 && a != s.act) {
      s.act = a;
      r.act_changed = true;
    }
  }
  r.reset_act = r.act_changed && !o.card_latch && !o.death_task;
  if (r.reset_act) {
    s.states = {};
    s.save_buffer.clear();
    s.gates.reset(o.hub);
    for (auto &q : s.quest_values)
      q.second = 0;
    for (auto &p : s.players)
      p.quests.clear();
    if (event)
      event("act_reset");
  }
  if (event)
    event("player_selection_apply_pending");
  s.prev = qualified_name(s.level, s.sub);
  auto n = split_request(s.pending.name);
  s.level = n.first;
  s.sub = n.second;
  s.pending.name.clear();
  r.orientation = s.pending.orientation;
  s.orientation_copy = r.orientation;
  s.pending.orientation = ORIENT_CONSUMED;
  if (!o.defer_recall_clear && r.act_changed && o.card_state != 4 &&
      !o.death_task)
    for (auto &p : s.players)
      p.recall_dest = "None";
  s.transient = {};
  r.had_saved_state = s.states.lookup(qualified_name(s.level, s.sub)).first;
  if (!r.had_saved_state)
    s.idle_voice_countdown = 300;
  return r;
}
RecallResult recall_finish(const std::string &level, const std::string &subname,
                           unsigned n, const std::array<Vec3, 4> &pos,
                           const std::array<std::string, 4> &dest,
                           const std::string &hub) {
  count(n);
  RecallResult r;
  r.destinations = dest;
  if (level.find("hub") == std::string::npos) {
    r.request = hub;
    r.arrived_by_recall = true;
    r.saved = pos;
    for (unsigned i = 0; i < n; ++i)
      r.destinations[i] = qualified_name(level, subname);
  } else {
    r.request = dest[0];
    r.restore_xy = pos;
  }
  return r;
}
RecallEvent RecallCountdown::tick(bool cutscene, std::uint32_t alive) {
  if (value == 0)
    return RecallEvent::None;
  bool aura = value == RECALL_TICKS;
  value = std::uint8_t(value - 1);
  if (value != 0)
    return aura ? RecallEvent::Aura : RecallEvent::None;
  if (cutscene)
    return RecallEvent::DroppedCutscene;
  if (alive == 0)
    return RecallEvent::DroppedNobody;
  return RecallEvent::Finish;
}
std::uint64_t recall_request_tick(std::uint64_t set_tick, bool before_run) {
  return (before_run ? set_tick : set_tick + 1) + RECALL_TICKS - 1;
}
namespace {
void apply_recall_finish(State &s, unsigned n,
                         const std::array<Vec3, 4> &positions,
                         const std::string &hub) {
  std::array<std::string, 4> dest;
  std::array<Vec3, 4> pos = positions;
  for (unsigned i = 0; i < 4; ++i) {
    dest[i] = s.players[i].recall_dest;
    if (s.level.find("hub") != std::string::npos)
      pos[i] = s.players[i].recall_position;
  }
  auto r = recall_finish(s.level, s.sub, n, pos, dest, hub);
  s.pending.request(r.request, ORIENT_RESTORE);
  s.arrived_by_recall = r.arrived_by_recall;
  s.restore_xy = r.restore_xy;
  for (unsigned i = 0; i < 4; ++i) {
    s.players[i].recall_dest = r.destinations[i];
    if (r.saved && i < n)
      s.players[i].recall_position = (*r.saved)[i];
  }
}
} // namespace
bool complete_recall(State &s, unsigned n, bool cutscene,
                     const std::array<Vec3, 4> &positions,
                     const std::string &hub) {
  count(n);
  if (s.transient.recall_countdown.running() || cutscene)
    return false;
  bool alive = false;
  for (unsigned i = 0; i < n; ++i)
    alive |= s.players[i].hp > 0;
  if (!alive)
    return false;
  apply_recall_finish(s, n, positions, hub);
  return true;
}
RecallEvent recall_tick(State &s, unsigned n, bool cutscene,
                        std::uint32_t alive,
                        const std::array<Vec3, 4> &positions,
                        const std::string &hub) {
  count(n);
  auto e = s.transient.recall_countdown.tick(cutscene, alive);
  if (e == RecallEvent::Finish)
    apply_recall_finish(s, n, positions, hub);
  return e;
}
std::optional<Checkpoint> checkpoint(State &s, const Blob &blob,
                                     const Blob &camera, int card) {
  if (find_act(s.acts, s.level, s.act) < 0)
    return {};
  s.states.store(qualified_name(s.level, s.sub), act_rows(s), card,
                 s.level_flags, false, blob);
  return Checkpoint{s.level,   s.sub,          s.act,   s.difficulty, s.spawn,
                    s.players, s.quest_values, s.gates, s.states,     camera};
}
Wipe party_wipe(State &s, const std::optional<Checkpoint> &cp) {
  if (s.transient.alternate_death)
    return Wipe::Wait;
  if (find_act(s.acts, s.level, s.act) < 0) {
    for (auto &p : s.players)
      p.hp = p.max_hp;
    s.pending.request(s.level, ORIENT_RESTORE);
    return Wipe::Restart;
  }
  if (!cp)
    throw MissingResource("party wipe requires last save buffer");
  s.level = cp->level;
  s.sub = cp->sub;
  s.act = cp->act;
  s.difficulty = cp->difficulty;
  s.spawn = cp->spawn;
  s.players = cp->players;
  s.quest_values = cp->quest_values;
  s.gates = cp->gates;
  s.states = cp->states;
  s.loaded_from_save = true;
  s.spawn.rank = 3;
  s.pending.request(qualified_name(s.level, s.sub), ORIENT_RESTORE);
  return Wipe::Reload;
}
Vec3 piece_pivot(const Piece &p) {
  ScopedFpu scope(Fpu{});
  auto x = mul(from_int(std::uint32_t(p.tx)), 0x3e800000),
       y = mul(from_int(std::uint32_t(p.ty)), 0x3e800000),
       z = mul(from_int(std::uint32_t(p.tz)), 0x3e800000);
  auto c = mul(from_int(std::uint32_t(p.cosine)), 0x38000100),
       s = mul(from_int(std::uint32_t(p.sine)), 0x38000100);
  return {p.mirrored ? add(mul(x, c), mul(y, s)) : sub(mul(x, c), mul(y, s)),
          p.mirrored ? sub(mul(x, s), mul(y, c)) : add(mul(x, s), mul(y, c)),
          z};
}
int find_moving_piece(std::vector<Piece> &ps, const Vec3 &pos) {
  ScopedFpu scope(Fpu{});
  Raw best = 0x45c80000;
  int index = -1;
  for (std::size_t i = 0; i < ps.size(); ++i) {
    auto p = piece_pivot(ps[i]);
    auto dx = sub(pos[0], p[0]), dy = sub(pos[1], p[1]), dz = sub(pos[2], p[2]);
    auto d = add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz));
    if (!lt(best, d)) {
      best = d;
      index = int(i);
    }
  }
  if (index < 0 || ps[index].claimed)
    return -1;
  ps[index].claimed = true;
  return index;
}
namespace {
constexpr Raw K_ROT = 0x38000100, K_32767 = 0x46fffe00, K_FOUR = 0x40800000;
std::int16_t round_s16(Raw x) {
  auto v = to_int(rta::floorf(add(mul(x, K_32767), HALF)));
  return std::int16_t(std::max(-0x8000, std::min(0x7fff, v)));
}
} // namespace
std::vector<ChunkTransform> move_piece(const Piece &p, const Vec3 &offset,
                                       std::int32_t angle,
                                       const std::vector<bool> &mirrored) {
  ScopedFpu scope(Fpu{});
  auto a = std::uint32_t(s16(std::uint32_t(angle)));
  auto ca = cos16(a), sa = sin16(a);
  auto c = from_int(std::uint32_t(p.cosine)), s = from_int(std::uint32_t(p.sine));
  auto new_cos =
      round_s16(sub(mul(mul(ca, c), K_ROT), mul(mul(sa, s), K_ROT)));
  auto new_sin =
      round_s16(add(mul(mul(sa, c), K_ROT), mul(mul(ca, s), K_ROT)));
  Vec3 o{mul(offset[0], K_FOUR), mul(offset[1], K_FOUR),
         mul(offset[2], K_FOUR)};
  std::vector<ChunkTransform> out;
  for (bool m : mirrored) {
    auto x = add(from_int(std::uint32_t(p.tx)), o[0]),
         y = add(from_int(std::uint32_t(p.ty)), o[1]),
         z = add(from_int(std::uint32_t(p.tz)), o[2]);
    auto xc = mul(x, ca), yc = mul(y, ca), ys = mul(y, sa);
    auto tx = m ? sub(xc, ys) : add(xc, ys);
    auto ty = m ? add(mul(x, sa), yc) : add(mul(neg(x), sa), yc);
    out.push_back({to_int(tx), to_int(ty), to_int(z), new_cos, new_sin});
  }
  return out;
}
std::vector<ChunkTransform> move_piece(const Piece &p, const Vec3 &offset,
                                       std::int32_t angle) {
  return move_piece(p, offset, angle,
                    p.chunk_mirrored ? *p.chunk_mirrored
                                     : std::vector<bool>{p.mirrored});
}
void release_piece(std::vector<Piece> &ps, int i) {
  if (i >= 0)
    ps.at(std::size_t(i)).claimed = false;
}
void disable_unclaimed(const std::vector<Piece> &ps,
                       const std::function<void(int, bool)> &fn) {
  for (std::size_t i = 0; i < ps.size(); ++i)
    if (!ps[i].claimed)
      fn(int(i), false);
}
PiecePose autodoor_pose(bool open) {
  PiecePose p;
  p.collision = !open;
  p.offset = {0, 0, open ? 0xc3800000u : 0u};
  return p;
}
Pieces::Pieces(std::vector<Piece> ps, world_queries::Level &t)
    : pieces_(std::move(ps)), poses_(pieces_.size()), terrain_(t) {
  if (pieces_.size() != t.data().pieces.size())
    throw Error("piece/terrain tables disagree");
  for (std::size_t i = 0; i < pieces_.size(); ++i)
    if (pieces_[i].element != std::size_t(t.data().pieces[i]))
      throw Error("piece element mapping disagrees");
}
void Pieces::disable_unclaimed() {
  level_flow::disable_unclaimed(pieces_,
                                [&](int i, bool on) { collision(i, on); });
}
void Pieces::collision(int i, bool on) {
  if (i < 0)
    return;
  poses_.at(std::size_t(i)).collision = on;
  terrain_.set_piece_collision(i, on);
}
void Pieces::height(int i, Raw h) { terrain_.set_piece_height(i, h); }
std::int64_t Pieces::height(int i) const {
  return terrain_.get_piece_height(i);
}
void Pieces::move(int i, const Vec3 &off, std::uint16_t angle) {
  if (i < 0)
    return;
  auto &p = poses_.at(std::size_t(i));
  p.offset = off;
  p.angle = angle;
  p.chunks = move_piece(pieces_.at(std::size_t(i)), off, s16(angle));
}
void Pieces::autodoor(int i, bool open) {
  if (i < 0)
    return;
  auto p = autodoor_pose(open);
  collision(i, p.collision);
  move(i, p.offset, p.angle);
}
void Pieces::reset() {
  for (std::size_t i = 0; i < pieces_.size(); ++i) {
    pieces_[i].claimed = false;
    poses_[i] = {};
    terrain_.set_piece_collision(i, true);
  }
}
} // namespace champions::simulation::rta::level_flow
