// Authored object-runtime replay. Expected values are produced independently by
// the pinned Python contracts; this executable computes every output field.
#include "champions/objects.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    const auto end = s.find(separator, start);
    result.push_back(s.substr(start, end == s.npos ? end : end - start));
    if (end == s.npos)
      return result;
    start = end + 1;
  }
}
std::int64_t integer(const std::string &s) {
  if (s.empty() || s == "-0" || s.front() == '+' ||
      (s.size() > 1 && s.front() == '0') ||
      (s.size() > 2 && s[0] == '-' && s[1] == '0'))
    throw std::runtime_error("noncanonical integer");
  for (std::size_t i = s.front() == '-' ? 1 : 0; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '9')
      throw std::runtime_error("invalid integer");
  std::size_t used = 0;
  const auto value = std::stoll(s, &used);
  if (used != s.size())
    throw std::runtime_error("invalid integer");
  return value;
}
std::string sequence(const std::vector<std::string> &values) {
  std::string out = "[";
  for (const auto &value : values) {
    if (out.size() > 1)
      out += ',';
    out += value;
  }
  return out + ']';
}
template <class C> std::string bytes(const C &values) {
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "b";
  for (const auto value : values) {
    const auto ch = static_cast<unsigned char>(value);
    out += hex[ch >> 4];
    out += hex[ch & 15];
  }
  return out;
}
std::string text(const std::string &value) {
  auto out = bytes(value);
  out.front() = 's';
  return out;
}
template <class C> std::string integers(const C &values) {
  std::vector<std::string> out;
  for (const auto value : values)
    out.push_back(std::to_string(value));
  return sequence(out);
}

namespace R = champions::simulation::rta;
namespace O = R::objects;
std::string n(std::int64_t x) { return std::to_string(x); }
template <class T> std::string opt(const std::optional<T> &x) {
  return x ? std::to_string(*x) : "_";
}
std::string str(const std::optional<std::string> &x) {
  return x ? bytes(*x) : "_";
}
std::string strings(const std::vector<std::string> &x) {
  std::vector<std::string> out;
  for (const auto &v : x)
    out.push_back(bytes(v));
  return sequence(out);
}
std::string snapshot(const O::WorldSnapshot &s) {
  R::Pool baseline;
  std::set<std::uint32_t> keys;
  for (const auto &v : baseline.link)
    keys.insert(v.first);
  for (const auto &v : s.pool.link)
    keys.insert(v.first);
  std::vector<std::string> delta, links, objs, live, names, tasks, centres,
      pads, spawn;
  for (auto k : keys) {
    auto a = baseline.link.find(k);
    auto b = s.pool.link.find(k);
    if (a == baseline.link.end() || b == s.pool.link.end() ||
        a->second != b->second)
      delta.push_back(
          sequence({n(k), b == s.pool.link.end() ? "_" : n(b->second)}));
  }
  for (const auto &l : s.links) {
    const auto kind = static_cast<int>(l.kind);
    bool keep = true;
    if (kind == 0 && l.id < 0x90000000ULL && !l.next && !l.prev && !l.partner &&
        !l.object && !l.tag)
      keep = false;
    if (kind == 1) {
      const auto index = (l.id - R::HandlePool::NODE_BASE) / 20;
      const auto next = index + 1 < R::HandlePool::NODE_COUNT ? l.id + 20 : 0;
      keep = l.next != next || l.prev || l.partner || l.object || l.tag;
    }
    if (keep)
      links.push_back(
          sequence({std::to_string(l.id), n(kind), std::to_string(l.next),
                    std::to_string(l.prev), std::to_string(l.partner),
                    std::to_string(l.object), n(l.tag)}));
  }
  for (const auto &o : s.objects) {
    std::string cd = "_";
    if (o.class_def) {
      std::vector<std::string> entries;
      for (const auto &e : o.class_def->entries)
        entries.push_back(sequence({bytes(e.first), n(e.second)}));
      cd = sequence({bytes(o.class_def->lump), n(o.class_def->generation),
                     sequence(entries)});
    }
    objs.push_back(sequence({n(o.serial),
                             n(o.addr),
                             n(o.flags),
                             integers(o.pos),
                             n(o.radius),
                             n(o.height),
                             n(o.vz),
                             n(o.angle),
                             n(o.seed),
                             n(o.vtable),
                             n(o.draw_half_width),
                             n(o.draw_height),
                             str(o.class_name),
                             bytes(o.type_name),
                             n(o.alive),
                             opt(o.tag),
                             n(o.props_id),
                             o.props ? strings(*o.props) : "_",
                             n(o.class_def_id),
                             cd}));
  }
  for (const auto &x : s.live)
    live.push_back(sequence({n(x.first), n(x.second)}));
  for (unsigned i = 0; i < 512; ++i)
    if (s.name_keys[i] || s.name_objects[i])
      names.push_back(
          sequence({n(i), str(s.name_keys[i]), n(s.name_objects[i])}));
  for (const auto &t : s.tasks.tasks)
    tasks.push_back(
        sequence({n(t.fn), n(t.priority), bytes(t.name), n(t.flags)}));
  for (const auto &x : s.centres)
    centres.push_back(integers(x));
  for (const auto &p : s.pads)
    pads.push_back(sequence({n(p.held), n(p.pressed), n(p.edge2)}));
  for (const auto &p : s.spawn_pos)
    spawn.push_back(integers(p));
  return sequence(
      {sequence({integers(s.pool.base), integers(s.pool.head), n(s.pool.last),
                 n(s.pool.end), sequence(delta)}),
       n(s.shared_seed), std::to_string(s.handle_free), n(s.iterator_serial),
       "[6096,28879]", sequence(links), sequence(objs), sequence(live),
       sequence(names), n(s.name_count),
       sequence({sequence(tasks), n(s.tasks.cursor)}),
       sequence({n(s.cursor15), n(s.cursor60), n(s.frame), n(s.network_mode),
                 n(s.cull_by_camera), n(s.pool_free_count), n(s.tearing_down),
                 n(s.lump_generation), n(s.menu_lockout),
                 n(s.menu_lockout_hold), n(s.warmup_frames), n(s.start_rank),
                 n(s.exclude_mask)}),
       integers(s.camera_focus), sequence(centres), sequence(pads),
       sequence(spawn), integers(s.spawn_facing),
       n(static_cast<int>(s.callback_policy)), integers(s.callbacks)});
}
constexpr const char *scenarios[] = {
    "primitives", "names",      "tasks",     "construct", "lifetime",
    "grid",       "queries",    "activity",  "reaper",    "run_mutation",
    "tasks_run",  "visibility", "free_spot", "marker",    "records",
    "walkers",    "init",       "errors",    "callbacks"};
struct CallbackFailure : std::runtime_error {
  CallbackFailure() : std::runtime_error("authored callback") {}
};
struct Scenario {
  std::unique_ptr<O::World> owner =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  O::World &w = *owner;
  int v;
  std::vector<std::string> results, events;
  explicit Scenario(int variant) : v(variant) {
    constexpr std::uint32_t seeds[] = {0, 1, 0xdeadbeef, 0xffffffff};
    w.shared.state = seeds[v];
    w.on_run = [](O::GameObject &) {};
    w.on_destroy = [this](O::GameObject &o, std::int64_t f) {
      w.destroy(o, f);
    };
    w.is_frozen = [] { return false; };
    w.res_find = [](const std::string &, const std::string &) { return 0U; };
    w.end_of_tick = [] {};
  }
  static O::Vec3 pos(int x = 0, int y = 0, int z = 0) {
    return {R::from_int(x), R::from_int(y), R::from_int(z)};
  }
  static O::Props props(std::initializer_list<std::string> p) {
    return std::make_shared<std::vector<std::string>>(p);
  }
  O::GameObject *create(int x = 0, int y = 0, int z = 0, O::Props p = {},
                        std::shared_ptr<O::ClassDef> d = {}) {
    return w.new_object(0x180, pos(x, y, z), std::move(d), std::move(p));
  }
  static std::string oid(O::GameObject *o) { return o ? n(o->serial) : "0"; }
  static std::string ids(const std::vector<O::GameObject *> &objs) {
    std::vector<std::string> x;
    for (auto *o : objs)
      x.push_back(oid(o));
    return sequence(x);
  }
  std::uint32_t cell_address(const R::Cell &head) {
    if (&head == &w.off_grid)
      return O::G_OFF_GRID;
    for (unsigned i = 0; i < 14400; ++i) {
      if (&head == &w.grid[i])
        return O::G_GRID + 4 * i;
      if (&head == &w.grid_b[i])
        return O::G_GRID_B + 4 * i;
    }
    throw std::runtime_error("unexpected home cell");
  }
  void destroy(O::GameObject &o, std::int64_t flags) {
    events.push_back(
        sequence({text("destroy"), oid(&o), n(flags), n(w.tearing_down)}));
    w.destroy(o, flags);
  }
  void run(const std::string &op) {
    auto &out = results;
    if (op == "primitives") {
      auto p = props({"name=\"One\"tail", "Name=Upper", "name=Two",
                      "n= -4294967297x", "name=Three", std::string("\xff=7")});
      for (auto i : {-1, 0, 1, 2, 3})
        out.push_back(str(O::props_find(p, "NAME", i)));
      out.push_back(str(O::props_find({}, "x")));
      out.push_back(n(O::props_int(p, "N")));
      out.push_back(n(O::props_int({}, "n")));
      for (const std::string value :
           {" \t-2147483649junk", "+4294967296", "--2",
            "\xff"
            "99",
            "00017", "\"MiXeD\"rest", "\"open"}) {
        std::string hash;
        try {
          hash = n(O::name_hash(value));
        } catch (const O::ObjectError &) {
          hash = text("ObjectError");
        }
        out.push_back(
            sequence({n(O::atoi(value)), hash, bytes(O::strip_quotes(value))}));
      }
      for (auto raw : {0U, 0x80000000U, 0x3f7fffffU, 0xbf7fffffU, 0xc66fff00U,
                       0x466fff00U, 0x7fffffffU, 0xffffffffU}) {
        try {
          out.push_back(sequence({n(raw), n(O::cell_coord(raw))}));
        } catch (const O::ObjectError &) {
          out.push_back(sequence({n(raw), text("ObjectError")}));
        }
      }
    } else if (op == "names") {
      auto *a = create(), *b = create(1), *c = create(2);
      out.push_back(n(w.names.add("\"Alpha\"tail", a)));
      out.push_back(n(w.names.add("ALPHA", b)));
      out.push_back(n(w.names.add("Beta", c)));
      out.push_back(ids(w.names.find("alpha", 8)));
      out.push_back(opt(w.names.remove("wrong-key", a)));
      out.push_back(n(w.names.add("alpha", c)));
      out.push_back(oid(w.names.find_first("ALPHA")));
      out.push_back(opt(w.names.remove("Beta", b)));
      out.push_back(ids(w.names.find("alpha", 2 + v)));
      if (v == 3)
        w.names.clear();
      out.push_back(n(w.names.count));
    } else if (op == "tasks") {
      auto &t = w.tasks;
      for (const auto &row : std::vector<std::array<int, 3>>{
               {10, 2, 0}, {20, 1, 1}, {30, 2, 2}, {40, 3, 3}})
        out.push_back(opt(t.add(row[0], row[1], "t" + n(row[0]), row[2])));
      out.push_back(opt(t.add(10, -99, "duplicate", 7)));
      out.push_back(n(t.is_running("t10")));
      out.push_back(n(t.is_running("T10")));
      bool changed = false;
      t.call(v == 1 ? 0 : 1, -1, [&](unsigned fn) {
        events.push_back(sequence({text("task"), n(fn), n(t.cursor)}));
        if (!changed && fn == unsigned(v == 1 ? 20 : 10)) {
          changed = true;
          if (v == 0)
            t.remove(fn);
          else if (v == 1)
            t.add(50, 1, "insert", 0);
          else if (v == 2)
            t.remove(30);
          else
            t.add(60, 4, "late", 2);
        }
      });
      t.call(0, 1, [&](unsigned fn) {
        events.push_back(sequence({text("draw"), n(fn), n(t.cursor)}));
      });
      out.push_back(opt(t.remove(999)));
    } else if (op == "construct") {
      auto p = props({"name=\"Twin\"suffix", "name=Second", "hide=0"});
      auto cd = std::make_shared<O::ClassDef>(
          "authored.lmp", std::vector<std::string>{"one", "two"}, 0);
      w.res_find = [&](const std::string &lump, const std::string &name) {
        events.push_back(sequence(
            {text("resource"), bytes(lump), bytes(name), n(cd->generation)}));
        return 0x1000 + std::uint32_t(events.size()) * 4;
      };
      auto *a = w.construct(w.pool.allocate(0x180), pos(), cd, p,
                            v & 1 ? props({"hide=1"}) : O::Props{});
      auto *b = create(256, 0, 0, p, cd);
      ++w.lump_generation;
      auto *c = create(
          512, 0, 0, v & 2 ? p : std::make_shared<std::vector<std::string>>(*p),
          cd);
      out.push_back(n(a->rand()));
      out.push_back(n(b->rand()));
      out.push_back(n(c->rand()));
      out.push_back(ids(w.names.find("twin", 9)));
      out.push_back(n(w.is_off_grid(*a)));
      out.push_back(oid(w.new_object(4000, pos())));
    } else if (op == "lifetime") {
      auto *a = create(0, 0, 0, props({"name=x"})),
           *b = create(1, 0, 0, props({"name=x"})),
           *c = create(2, 0, 0, props({"name=x"}));
      for (auto *o : {a, b, c})
        w.run_every_frame(*o);
      w.on_destroy = [&](O::GameObject &o, std::int64_t f) {
        destroy(o, f);
        if (v == 2 && &o == b)
          w.delete_object(*c);
      };
      w.delete_object(*a);
      w.delete_object(*b);
      if (v == 1) {
        b->flags |= 0x400;
        w.exclude_mask = 0x400;
      }
      w.flush_delete_list();
      if (v == 3)
        w.destroy(*c, 2);
      out.push_back(oid(create(7)));
      w.exclude_mask = 0;
      w.flush_delete_list();
    } else if (op == "grid") {
      auto *a = create(), *b = create(16000);
      out.push_back(n(w.is_off_grid(*b)));
      a->flags |= 0x800;
      w.update_grid_cell(*a);
      out.push_back(n(cell_address(w.home_cell(*a, true))));
      a->flags |= 2;
      w.update_grid_cell(*a);
      out.push_back(n(cell_address(w.home_cell(*a, true))));
      w.move_off_grid(*a);
      out.push_back(n(w.is_off_grid(*a)));
      w.link_to_grid(*a);
      out.push_back(n(w.is_off_grid(*a)));
      a->flags &= ~2U;
      w.update_grid_cell(*a);
      w.add_to_always_draw(*a);
      w.add_to_always_draw_once(*a);
      w.add_to_always_draw_once(*b);
      if (v & 1)
        w.exclude_mask = 0x800;
      w.clear_always_draw();
      if (v & 2) {
        b->pos = pos(-15361);
        w.update_grid_cell(*b);
      }
    } else if (op == "queries") {
      std::vector<O::GameObject *> objs;
      for (auto p : std::vector<std::array<int, 3>>{{0, 0, 0},
                                                    {24, 0, 0},
                                                    {0, 24, 80},
                                                    {300, 0, 0},
                                                    {-260, 0, 0},
                                                    {0, 0, -72}})
        objs.push_back(create(p[0], p[1], p[2]));
      constexpr unsigned flags[] = {0, 1, 8, 0x20000};
      objs[1]->flags = flags[v];
      objs[2]->flags |= 0x400;
      w.exclude_mask = v & 1 ? 0x400 : 0;
      objs[3]->flags |= 0x800;
      w.update_grid_cell(*objs[3]);
      constexpr int limits[] = {1, 2, 4, 9};
      out.push_back(
          ids(w.collect_in_box(pos(-25, -25, -1), pos(25, 25, 80), limits[v])));
      auto &head = w.make_cell(0x90000000);
      out.push_back(ids(w.collect_in_sphere(R::from_int(100), pos(), head)));
      std::vector<O::GameObject *> members;
      for (auto *o : R::HandlePool::items(head))
        members.push_back(static_cast<O::GameObject *>(o));
      out.push_back(ids(members));
      w.handles.clear(head);
      for (unsigned i = 0; i < objs.size(); ++i) {
        auto &o = *objs[i];
        o.type_name = i % 2 == 0 ? "authored" : "other";
        if (i % 3 == 0)
          w.run_every_frame(o);
        else if (i % 3 == 1)
          w.run_every_15(o);
        else
          w.run_every_60(o);
      }
      out.push_back(ids(w.find_by_class("authored", 3 + v)));
    } else if (op == "activity") {
      auto *a = create(100, 0, 10);
      w.centres = {pos()};
      if (v == 3)
        a->flags |= 0x80000;
      std::vector<int> active;
      for (auto radius : {0, 100, 101, 46341})
        active.push_back(w.is_active(*a, radius));
      out.push_back(integers(active));
      w.run_every_frame(*a);
      out.push_back(n(w.update_active(*a, 1, 10)));
      w.centres = {a->pos};
      out.push_back(n(w.update_active(*a, 0, 100)));
      w.stop_running(*a);
      w.cursor15 = 14;
      w.cursor60 = 59;
      w.run_every_15(*a);
      w.run_every_60(*a);
    } else if (op == "reaper") {
      constexpr int frames[] = {0, 7, 899, -1};
      w.frame = frames[v];
      auto indices = w.reap_cells();
      out.push_back(integers(indices));
      O::GameObject *last = nullptr;
      for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < 2; ++j) {
          auto index = indices[i];
          last = create(int(index % 120) * 256 - 15360 + 10,
                        int(index / 120) * 256 - 15360 + 10);
          last->flags = 0x1800;
          w.update_grid_cell(*last);
        }
      w.camera_focus = last->pos;
      constexpr int counts[] = {199, 200, 299, 200};
      w.pool_free_count = counts[v];
      w.network_mode = v == 3;
      w.on_destroy = [&](O::GameObject &o, std::int64_t f) { destroy(o, f); };
      w.exclude_mask = 0x1000;
      out.push_back(ids(w.reap()));
    } else if (op == "run_mutation") {
      auto *a = create(), *b = create(1), *c = create(2);
      w.frame = 1;
      w.run_every_frame(*a);
      w.run_every_frame(*b);
      c->flags = v == 3 ? 0x8000 : 0;
      w.run_every_15(*c);
      bool once = false;
      w.on_run = [&](O::GameObject &o) {
        events.push_back(sequence({text("run"), oid(&o), n(w.frame)}));
        if (!once) {
          once = true;
          if (v == 0) {
            w.destroy(*a);
            w.destroy(o);
          } else if (v == 1)
            w.run_every_60(o);
          else if (v == 2) {
            auto *d = create(3);
            w.run_every_60(*d);
            w.frame = 1;
          } else
            w.exclude_mask = 0x8000;
        }
      };
      w.on_destroy = [&](O::GameObject &o, std::int64_t f) { destroy(o, f); };
      w.run_objects();
    } else if (op == "tasks_run") {
      w.tasks.add(10, 1, "run", 0);
      w.tasks.add(20, 2, "draw", 1);
      w.network_mode = v == 1;
      w.warmup_frames = v == 2;
      w.menu_lockout = 4;
      w.menu_lockout_hold = v == 3;
      w.frame = v == 0 ? 0x7fffffff : 7;
      for (unsigned i = 0; i < 4; ++i)
        w.pads[i] = {v != 2 ? 1 : 0x00f00000, int(i + 1), int(i + 9)};
      auto edges = [&]() {
        std::vector<std::string> x;
        for (auto &p : w.pads)
          x.push_back(sequence({n(p.pressed), n(p.edge2)}));
        return sequence(x);
      };
      w.end_of_tick = [&] {
        events.push_back(sequence({text("end"), n(w.frame), edges()}));
        if (v == 3)
          w.pads[0].pressed = 99;
      };
      w.is_frozen = [&] { return v == 3; };
      w.tasks_run(v == 2 ? -1 : 2, [&](unsigned fn) {
        events.push_back(sequence({text("task"), n(fn), n(w.frame), edges()}));
      });
    } else if (op == "visibility") {
      auto *a = create(), *b = create(100, 100, 10000), *c = create(-5);
      b->flags = 8;
      std::vector<O::GameObject *> players{a, nullptr, b, c};
      w.network_mode = v == 1;
      w.camera_focus = pos(500, 500);
      w.set_object_vis(v == 3 ? c : nullptr, v % 2, v == 2, players);
      out.push_back(
          n(w.is_near_view(pos(24, 24, -10000), R::from_int(24), players)));
      auto alive = w.alive_players(players, [&](std::size_t slot) {
        events.push_back(sequence({text("local"), n(slot)}));
        return slot % 2 == unsigned(v % 2);
      });
      out.push_back(
          sequence({integers(std::get<0>(alive)), integers(std::get<1>(alive)),
                    n(std::get<2>(alive))}));
    } else if (op == "free_spot") {
      if (v == 3)
        create();
      auto blocked = [&](R::Raw radius, const O::Vec3 &p) {
        events.push_back(sequence({text("blocked"), n(radius), integers(p)}));
        return v == 1 ? R::from_int(1) : 0U;
      };
      auto height = [&](R::Raw radius, const O::Vec3 &p) {
        events.push_back(sequence({text("height"), n(radius), integers(p)}));
        return R::from_int(v == 2 ? 6 : -1);
      };
      auto result =
          w.find_free_spot(pos(), v == 3 ? 0 : 36, 24, false, blocked, height);
      out.push_back(sequence({integers(result.first), n(result.second)}));
      if (v == 2) {
        result = w.find_free_spot(pos(), 1, 1, true, blocked, height);
        out.push_back(sequence({integers(result.first), n(result.second)}));
      }
    } else if (op == "marker") {
      const char *p[] = {"from=old", "from=gate", "name=door", "default=1"};
      constexpr int ranks[] = {0, 2, 1, 1};
      w.start_rank = ranks[v];
      auto blocked = [&](R::Raw radius, const O::Vec3 &p) {
        events.push_back(sequence({text("blocked"), n(radius), integers(p)}));
        return 0U;
      };
      auto height = [&](R::Raw radius, const O::Vec3 &p) {
        events.push_back(sequence({text("height"), n(radius), integers(p)}));
        return p[2];
      };
      out.push_back(oid(w.player1_marker(
          w.pool.allocate(0x180), pos(10, -20, 3), -40000, props({p[v]}), "old",
          "door", v == 1, blocked, height)));
      if (v == 0 || v == 3) {
        if (v == 3)
          w.start_rank = 3;
        out.push_back(
            oid(w.player1_marker(w.pool.allocate(0x180), pos(10, -20, 3), 0, {},
                                 "old", "door", false, blocked, height)));
      }
    } else if (op == "records") {
      auto p = props({"name=\"Shared\"tail", "x=1"});
      O::RecordBlob blob({O::Record("one", -32769, pos(1, 2, 3), p, {1, 2}),
                          O::Record({}, 65537, pos(-4), p, {0}),
                          O::Record("skip", 7, pos(8), {}, {})},
                         v & 1);
      out.push_back(bytes(blob.build()));
      out.push_back(n(blob.size()));
      O::Factory factory = [&](const std::optional<std::string> &name,
                               O::Vec3 position, std::int64_t angle,
                               O::Props pr) {
        events.push_back(
            sequence({text("factory"), str(name), integers(position), n(angle),
                      pr ? strings(*pr) : "_"}));
        if (!name)
          return static_cast<O::GameObject *>(nullptr);
        auto *o = w.new_object(0x180, position, {}, pr);
        if (*name == "skip")
          w.delete_object(*o);
        return o;
      };
      std::function<bool(const std::optional<std::string> &)> filter;
      if (v == 2)
        filter = [&](const std::optional<std::string> &name) {
          events.push_back(sequence({text("filter"), str(name)}));
          return name == std::optional<std::string>("one");
        };
      out.push_back(n(w.load_records(
          blob, factory, v >= 2, filter,
          [&](O::GameObject &o, const O::Bytes &data) {
            events.push_back(sequence({text("state"), oid(&o), bytes(data)}));
            o.tag = data.size();
          })));
      out.push_back(n(blob.relocated));
      out.push_back(opt(blob.cached_size));
      out.push_back(bytes(blob.build()));
      out.push_back(
          oid(O::create_object("created", pos(2), 65535, {}, factory)));
      out.push_back(oid(O::create_object({}, pos(), 0, {}, {})));
    } else if (op == "walkers") {
      auto *a = create(), *b = create(16000), *c = create(256);
      c->flags = 0x800;
      w.update_grid_cell(*c);
      for (auto *o : {a, b, c})
        w.run_every_frame(*o);
      a->flags |= 0x400;
      w.exclude_mask = v & 1 ? 0x400 : 0;
      w.for_each_grid_object([&](O::GameObject &o) {
        events.push_back(sequence({text("grid"), oid(&o)}));
      });
      w.for_each_run_object([&](O::GameObject &o) {
        events.push_back(sequence({text("run"), oid(&o)}));
      });
      w.on_destroy = [&](O::GameObject &o, std::int64_t f) { destroy(o, f); };
      if (v & 2)
        w.delete_object(*b);
      w.delete_all_objects();
    } else if (op == "init") {
      auto *a = create(0, 0, 0, props({"name=before"}));
      w.run_every_frame(*a);
      w.add_to_always_draw(*a);
      w.init();
      w.init();
      out.push_back(ids(w.names.find("before", 2)));
      out.push_back(n(w.tasks.is_running("runObjects")));
      if (v & 1)
        w.destroy(*a);
    } else if (op == "errors") {
      auto *a = create();
      auto *b = create(1);
      if (v == 0)
        w.names.find("x", 1);
      else if (v == 1) {
        for (unsigned i = 0; i < 33; ++i)
          w.tasks.add(i, 0, "x", 0);
      } else if (v == 2) {
        w.frame = -1;
        w.run_every_frame(*a);
        w.on_run = [&](O::GameObject &o) {
          events.push_back(sequence({text("run"), oid(&o)}));
        };
        w.run_objects();
      } else {
        w.run_every_frame(*a);
        w.run_every_frame(*b);
        w.on_run = [&](O::GameObject &o) {
          events.push_back(sequence({text("throw"), oid(&o)}));
          o.tag = 77;
          throw CallbackFailure();
        };
        w.run_objects();
      }
    } else if (op == "callbacks") {
      if (v == 0) {
        auto cd = std::make_shared<O::ClassDef>(
            "callback.lmp", std::vector<std::string>{"first", "second"});
        w.res_find = [&](const std::string &lump, const std::string &name) {
          events.push_back(
              sequence({text("resource"), bytes(lump), bytes(name)}));
          if (name == "second")
            throw CallbackFailure();
          return 0x10203040U;
        };
        create(0, 0, 0, {}, cd);
      } else if (v == 1) {
        O::RecordBlob blob(
            {O::Record("one", 0, pos(), props({"name=x"}), {'a', 'b', 'c'})},
            true);
        O::Factory factory = [&](const std::optional<std::string> &, O::Vec3 p,
                                 std::int64_t, O::Props values) {
          return w.new_object(0x180, p, {}, values);
        };
        try {
          w.load_records(blob, factory, false, {},
                         [&](O::GameObject &o, const O::Bytes &data) {
                           events.push_back(
                               sequence({text("state"), oid(&o), bytes(data)}));
                           o.tag = 42;
                           w.delete_object(o);
                           throw CallbackFailure();
                         });
        } catch (...) {
          out.push_back(sequence({n(blob.relocated), opt(blob.cached_size)}));
          throw;
        }
        out.push_back(sequence({n(blob.relocated), opt(blob.cached_size)}));
      } else if (v == 2) {
        auto *a = create();
        w.find_free_spot(
            pos(), 36, 24, false,
            [&](R::Raw radius, const O::Vec3 &p) -> R::Raw {
              events.push_back(
                  sequence({text("blocked"), n(radius), integers(p)}));
              a->flags = 1;
              throw CallbackFailure();
            },
            [](R::Raw, const O::Vec3 &) { return 0U; });
      } else {
        w.pads[0].pressed = 11;
        w.pads[0].edge2 = 12;
        w.menu_lockout = 5;
        w.tasks.add(1, 0, "one", 0);
        w.end_of_tick = [&] {
          events.push_back(sequence({text("end"), n(w.frame)}));
          w.pads[0].pressed = 99;
          throw CallbackFailure();
        };
        w.tasks_run(2, [&](unsigned fn) {
          events.push_back(sequence({text("task"), n(fn)}));
        });
      }
    } else
      throw std::runtime_error("unknown authored object scenario");
  }
};
std::pair<std::string, std::string> scenario(const std::string &operation,
                                             std::int64_t config,
                                             const std::string &args) {
  const auto parts = split(args, ';');
  if (parts.size() != 2)
    throw std::runtime_error("scenario args require id;variant");
  const auto id = integer(parts[0]), variant = integer(parts[1]);
  if (config < 0 || config >= 16 || variant < 0 || variant >= 4 || id < 0 ||
      id >= 1216)
    throw std::runtime_error("scenario outside coverage");
  if (config != id / 76 || variant != (id / 19) % 4 ||
      operation != scenarios[id % 19])
    throw std::runtime_error("scenario identity mismatch");
  R::ScopedFpu fpu(R::Fpu(static_cast<R::Fpu::Model>(config / 4),
                          bool(config & 2), bool(config & 1)));
  Scenario input{int(variant)};
  std::string status = "ok";
  try {
    input.run(operation);
  } catch (const CallbackFailure &) {
    status = "CallbackFailure";
  } catch (const O::ObjectError &) {
    status = "ObjectError";
  } catch (const R::HandleError &) {
    status = "HandleError";
  }
  return {status, sequence({sequence(input.results), sequence(input.events),
                            snapshot(input.w.snapshot())})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0xe1b67210f7cacedfULL;
constexpr std::size_t expected_rows = 1216;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error("usage: objects-replay VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 1024ULL * 1024 * 1024;
  constexpr std::size_t max_line = 16 * 1024 * 1024;
  if (!std::filesystem::is_regular_file(args[0]) ||
      std::filesystem::file_size(args[0]) > max_fixture)
    throw std::runtime_error(
        "fixture is missing, not regular, or exceeds budget");
  if (args.size() == 2) {
    if (std::filesystem::weakly_canonical(args[0]) ==
            std::filesystem::weakly_canonical(args[1]) ||
        (std::filesystem::exists(args[1]) &&
         std::filesystem::equivalent(args[0], args[1])))
      throw std::runtime_error("trace must not overwrite fixture");
  }
  std::ifstream input(args[0], std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open fixture");
  std::ofstream output;
  if (args.size() == 2) {
    output.open(args[1], std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open trace");
  }
  auto line = [&]() {
    std::string value;
    for (char ch; input.get(ch);) {
      if (ch == '\n')
        return value;
      if (value.size() == max_line)
        throw std::runtime_error("fixture line exceeds budget");
      if (static_cast<unsigned char>(ch) > 127 || ch == '\r' || ch == '\0')
        throw std::runtime_error("fixture must be ASCII with LF separators");
      value += ch;
    }
    if (!value.empty())
      throw std::runtime_error("fixture ends without newline");
    return value;
  };
  if (line() != "# objects_vectors_v1")
    throw std::runtime_error("fixture schema mismatch");
  std::set<std::string> seen;
  std::size_t rows = 0;
  std::uint64_t digest = 14695981039346656037ULL;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto columns = split(line(), '\t');
    if (columns.size() != 5)
      throw std::runtime_error("fixture requires five columns");
    const auto key = columns[0] + '\t' + columns[1] + '\t' + columns[2];
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate scenario");
    const auto actual = scenario(columns[0], integer(columns[1]), columns[2]);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::size_t offset = 0;
      while (offset < actual.second.size() && offset < columns[4].size() &&
             actual.second[offset] == columns[4][offset])
        ++offset;
      std::cerr << "objects mismatch at row " << rows + 1 << " (" << columns[0]
                << ") status=" << actual.first << " expected=" << columns[3]
                << " offset=" << offset << "\nactual "
                << actual.second.substr(offset, 180) << "\nexpected "
                << columns[4].substr(offset, 180) << '\n';
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (args.size() == 2) {
      output << record;
      if (!output)
        throw std::runtime_error("trace write failed");
    }
    for (unsigned char ch : record) {
      digest ^= ch;
      digest *= 1099511628211ULL;
    }
    if (++rows > 65536)
      throw std::runtime_error("fixture row budget exceeded");
  }
  if (input.bad())
    throw std::runtime_error("fixture read failed");
  if (rows != expected_rows || digest != expected_digest)
    throw std::runtime_error("incomplete or changed objects corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows << " exact objects vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    return run(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return entry(args);
}
