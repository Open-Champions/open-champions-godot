// Authored object-runtime replay. Expected values are produced independently by
// the pinned Python contracts; this executable computes every output field.
#include "champions/objects.hpp"
#include "champions/raw_handles.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
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
        !l.object && !l.object_raw && !l.tag)
      keep = false;
    if (kind == 1) {
      const auto index = (l.id - R::HandlePool::NODE_BASE) / 20;
      const auto next = index + 1 < R::HandlePool::NODE_COUNT ? l.id + 20 : 0;
      keep = l.next != next || l.prev || l.partner || l.object ||
             l.object_raw || l.tag;
    }
    if (keep)
      links.push_back(
          sequence({std::to_string(l.id), n(kind), std::to_string(l.next),
                    std::to_string(l.prev), std::to_string(l.partner),
                    std::to_string(l.object), n(l.tag), n(l.object_raw)}));
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
namespace H = R::creature_hp;
namespace B = R::raw_handles;
constexpr std::uint32_t HEAD = 0x91000000, HEAD2 = 0x91000010,
                        DRAIN = 0x91000020, BASE = R::HandlePool::NODE_BASE,
                        FREE = 0x864ea8;
constexpr const char *names[] = {
    "pool_identity", "partner_destroy", "remove_side", "front_clear",
    "add_insert",    "tag_reuse",       "padding",     "dirty_init",
    "last_free",     "empty_free",      "hp_link",     "hp_destroy_live",
    "source_alias",  "null_source",     "opaque",      "unknown_head",
    "reuse",         "aliases",         "hp_failure",  "nested_destroy",
    "poke_refuse",   "store_refuse",    "mixed_load",  "detach"};
bool floating(const std::string &name) {
  return name == "hp_link" || name == "hp_destroy_live" ||
         name == "source_alias" || name == "null_source" ||
         name == "unknown_head" || name == "hp_failure" ||
         name == "nested_destroy";
}
struct CallbackFailure : std::runtime_error {
  CallbackFailure() : std::runtime_error("authored callback") {}
};
struct Replay {
  std::unique_ptr<O::World> owner =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  O::World &w = *owner;
  H::Model model;
  B::RawHandles bridge{w, model.mem};
  O::GameObject *a = nullptr, *b = nullptr;
  unsigned variant;
  std::string mode;
  std::map<std::uint32_t, R::Cell *> heads;
  std::vector<std::string> results, reads, pokes, effects;
  std::size_t logged = 0;
  static O::Vec3 pos(int x, int y, int z) {
    return {R::from_int(x), R::from_int(y), R::from_int(z)};
  }
  explicit Replay(unsigned v) : variant(v) {
    w.shared.state = v;
    w.on_run = [](O::GameObject &) {};
    w.on_destroy = [this](O::GameObject &o, std::int64_t flags) {
      w.destroy(o, flags);
    };
    w.is_frozen = [] { return false; };
    w.res_find = [](const std::string &, const std::string &) { return 0U; };
    w.end_of_tick = [] {};
    a = w.new_object(0x400, pos(1 + v, 2, 0));
    b = w.new_object(0x400, pos(3, 4 + v, 0));
    bind_object(*a);
    bind_object(*b);
    intern(HEAD, "head");
    intern(HEAD2, "head2");
    intern(DRAIN, "drain");
    for (unsigned i = 0; i < 8; ++i) {
      model.mem.poke(BASE + 20 * i + 18, 1, 0x80 + i);
      model.mem.poke(BASE + 20 * i + 19, 1, v);
    }
    for (auto *obj : {a, b}) {
      const std::vector<std::array<std::uint64_t, 3>> initial = {
          {0xc, 4, 32},  {0x10, 4, obj->pos[0]}, {0x14, 4, obj->pos[1]},
          {0xa8, 8, 0},  {0xc8, 4, 0},           {0x228, 4, 0x42c80000},
          {0x231, 1, 0}, {0x2dc, 4, 0},          {0x2e0, 2, 0},
          {0x304, 4, 0}, {0x3b0, 2, 0},          {0x4c, 4, 0},
          {0x360, 4, 0}};
      for (auto x : initial)
        model.mem.poke(obj->id + x[0], x[1], x[2]);
    }
    intern(a->id + 0x2e4, "last");
    model.mem.poke(H::G_SIM_FRAME, 4, 32 + v);
    model.callbacks["handle_clear"] =
        [this](H::Model &, const std::vector<std::uint32_t> &args) {
          flush();
          effects.push_back(
              sequence({text("call"), text("handle_clear"), integers(args)}));
          bridge.clear(args.at(0));
          if (mode == "destroy_read" || mode == "destroy_throw") {
            w.destroy(*b, 3);
            read("inside_destroy", HEAD);
            read("inside_free", FREE);
            if (mode == "destroy_throw")
              throw CallbackFailure();
          }
        };
    model.callbacks["handle_link"] =
        [this](H::Model &, const std::vector<std::uint32_t> &args) {
          flush();
          effects.push_back(
              sequence({text("call"), text("handle_link"), integers(args)}));
          bridge.link(args.at(0), args.at(1), args.at(2));
        };
  }
  void bind_object(O::GameObject &o) {
    bridge.bind_object(o);
    for (unsigned i = 0; i < 3; ++i)
      heads[o.id + 4 * i] = &o.heads[i];
  }
  R::Cell &intern(std::uint32_t raw, const std::string &label) {
    auto &c = bridge.intern_raw_head(raw, label);
    heads[raw] = &c;
    return c;
  }
  std::uint32_t ptr(R::Link *p) {
    if (!p)
      return 0;
    if (p->kind == R::LinkKind::Node)
      return BASE + 20 * static_cast<R::Node *>(p)->index;
    for (auto h : heads)
      if (h.second == p)
        return h.first;
    throw B::Error("unaddressed pointer");
  }
  std::uint32_t known(std::uint32_t head, O::GameObject *o) {
    return bridge.link_known(head, o->id, *o);
  }
  R::Node *node(std::uint32_t raw) {
    return w.handles.nodes().at((raw - BASE) / 20);
  }
  std::uint64_t read(const std::string &label, std::uint32_t addr,
                     unsigned size = 4) {
    auto value = model.mem.load(addr, size);
    reads.push_back(
        sequence({text(label), n(addr), n(size), std::to_string(value)}));
    return value;
  }
  void poke(std::uint32_t addr, unsigned size, std::uint64_t value) {
    pokes.push_back(sequence({n(addr), n(size), std::to_string(value)}));
    model.mem.poke(addr, size, value);
  }
  static std::string write(const H::Write &x) {
    return sequence({n(x.address), n(x.size), std::to_string(x.value)});
  }
  void flush() {
    while (logged < model.mem.log().size()) {
      const auto &x = model.mem.log()[logged++];
      effects.push_back(sequence(
          {text("store"), n(x.address), n(x.size), std::to_string(x.value)}));
    }
  }
  template <class F> void invoke(const std::string &label, F f) {
    std::string status = "ok", value = "_";
    try {
      value = f();
    } catch (const CallbackFailure &) {
      status = "CallbackFailure";
    } catch (const B::Error &) {
      status = "OverlayError";
    } catch (const H::Error &) {
      status = "OverlayError";
    } catch (const R::HandleError &) {
      status = "HandleError";
    }
    flush();
    results.push_back(sequence({text(label), text(status), value}));
  }
  std::string remove(R::Link *p) {
    auto r = w.handles.remove_ref(p);
    if (r.raw)
      return sequence({text("raw"), n(r.raw)});
    if (r.object)
      return sequence(
          {text("known"), n(static_cast<O::GameObject *>(r.object)->serial)});
    return "_";
  }
  void drain(unsigned left) {
    const auto count = w.handles.count_free();
    for (std::size_t i = left; i < count; ++i)
      w.handles.add(*heads.at(DRAIN), nullptr);
  }
  void hp() { model.take_damage(a->id, b->id, 0x40, 0x40800000); }
  void recipe(const std::string &s) {
    auto &p = w.handles;
    auto &mem = model.mem;
    if (s == "pool_identity") {
      invoke("link", [&] { return n(known(HEAD, b)); });
      read("head", HEAD);
      read("free", FREE);
    } else if (s == "partner_destroy") {
      known(HEAD, b);
      bridge.link(HEAD2, b->id, b->id);
      invoke("destroy", [&] {
        w.destroy(*b, 3);
        return "_";
      });
      read("first", HEAD);
      read("second", HEAD2);
    } else if (s == "remove_side") {
      auto *x = node(known(HEAD, b));
      invoke("remove", [&] { return remove(variant ? x->partner : x); });
      read("free", FREE);
    } else if (s == "front_clear") {
      known(HEAD, a);
      known(HEAD, b);
      invoke("clear", [&] {
        bridge.clear(HEAD);
        return "_";
      });
      read("head", HEAD);
    } else if (s == "add_insert") {
      auto *first = p.add(*heads.at(HEAD), a);
      auto *second = p.insert(*heads.at(HEAD), first, b);
      invoke("remove", [&] { return remove(second); });
      invoke("clear", [&] {
        bridge.clear(HEAD);
        return "_";
      });
    } else if (s == "tag_reuse") {
      auto *x = node(known(HEAD, b));
      x->tag = 0x1234 + variant;
      x->partner->tag = 0xab00 + variant;
      p.remove(x);
      invoke("link",
             [&] { return n(bridge.link(HEAD, a->id, 0xdead0000 + variant)); });
      read("tag", ptr(x) + 16, 4);
    } else if (s == "padding") {
      auto *x = node(known(HEAD, b));
      p.remove(x);
      known(HEAD, a);
      for (unsigned i = 0; i < 8; ++i)
        read("pad" + std::to_string(i), BASE + i * 20 + 16, 4);
    } else if (s == "dirty_init") {
      auto *x = node(known(HEAD, b));
      x->tag = 17 + variant;
      x->partner->tag = 33;
      invoke("init", [&] {
        p.init();
        return "_";
      });
      read("stale", ptr(x) + 8, 8);
    } else if (s == "last_free" || s == "empty_free") {
      drain(s == "last_free" ? 1 : 0);
      if (p.free)
        p.free->tag = 0xbeef;
      invoke("exhaust", [&] { return n(known(HEAD, b)); });
      read("free", FREE);
    } else if (s == "hp_link") {
      invoke("hp", [&] {
        hp();
        return "_";
      });
      read("last", a->id + 0x2e4);
    } else if (s == "hp_destroy_live") {
      known(HEAD, b);
      mode = "destroy_read";
      invoke("hp", [&] {
        hp();
        return "_";
      });
    } else if (s == "source_alias") {
      intern(a->id + 0x4c, "owner");
      intern(a->id + 0x360, "effect");
      known(a->id + 0x4c, b);
      known(a->id + 0x360, b);
      mem.poke(a->id + 0xc, 4, 0xa0);
      invoke("hp", [&] {
        model.take_damage(a->id, a->id, 0x10000000, 0x40000000);
        return "_";
      });
      invoke("destroy", [&] {
        w.destroy(*b, 3);
        return "_";
      });
      read("owner", a->id + 0x4c);
      read("source", a->id + 0x360);
      read("last", a->id + 0x2e4);
    } else if (s == "null_source") {
      mem.poke(b->id + 0xc, 4, 0xa0);
      if (variant) {
        p.add(intern(b->id + 0x4c, "owner"), nullptr);
      }
      invoke("hp", [&] {
        hp();
        return "_";
      });
    } else if (s == "opaque") {
      invoke("link",
             [&] { return n(bridge.link(HEAD, b->id, 0xdead0000 + variant)); });
      read("object", read("head", HEAD) + 12);
      invoke("clear", [&] {
        bridge.clear(HEAD);
        return "_";
      });
    } else if (s == "unknown_head") {
      auto *x = p.add(intern(b->id + 0x4c, "owner"), nullptr);
      x->set_object(R::HandleObjectRef::opaque(0xdead0000 + variant));
      mem.poke(b->id + 0xc, 4, 0xa0);
      invoke("hp", [&] {
        hp();
        return "_";
      });
    } else if (s == "reuse") {
      known(HEAD, b);
      w.destroy(*b, 3);
      auto *replacement = w.new_object(0x400, pos(7, 8, 0));
      results.push_back(sequence({text("reused"), text("ok"),
                                  sequence({n(replacement->id), n(b->serial),
                                            n(replacement->serial)})}));
      invoke("bind_conflict", [&] {
        bind_object(*replacement);
        return "_";
      });
      if (variant)
        invoke("rebind", [&] {
          bridge.rebind_object(*replacement);
          for (unsigned i = 0; i < 3; ++i)
            heads[replacement->id + 4 * i] = &replacement->heads[i];
          return "_";
        });
      read("last_free_object", ptr(p.free) + 12);
    } else if (s == "aliases") {
      invoke("same", [&] {
        bridge.bind_head(HEAD, *heads.at(HEAD), "head");
        return "_";
      });
      invoke("label", [&] {
        bridge.bind_head(HEAD, *heads.at(HEAD), "alias");
        return "_";
      });
      invoke("conflict", [&] {
        bridge.bind_head(HEAD, *heads.at(HEAD2), "other");
        return "_";
      });
      known(HEAD, b);
      read("alias", HEAD);
    } else if (s == "hp_failure") {
      drain(1);
      invoke("hp", [&] {
        hp();
        return "_";
      });
      read("free", FREE);
      read("hp", a->id + 0x228);
    } else if (s == "nested_destroy") {
      known(HEAD, b);
      intern(a->id + 0x4c, "owner");
      known(a->id + 0x4c, b);
      mem.poke(a->id + 0xc, 4, 0xa0);
      mode = variant ? "destroy_throw" : "destroy_read";
      invoke("hp", [&] {
        model.take_damage(a->id, a->id, 0x40, 0x40000000);
        return "_";
      });
    } else if (s == "poke_refuse") {
      invoke("head", [&] {
        poke(HEAD, 4, 0);
        return "_";
      });
      invoke("mixed", [&] {
        poke(BASE + 17, 3, 0xabcdef);
        return "_";
      });
      invoke("padding", [&] {
        poke(BASE + 18, 2, 0x1234 + variant);
        return "_";
      });
    } else if (s == "store_refuse") {
      invoke("store", [&] {
        mem.store(HEAD, 4, 0x12345678);
        return "_";
      });
      invoke("model_write", [&] {
        model.take_damage(HEAD - 0x3b0, 0, 0, 0);
        return "_";
      });
      invoke("padding", [&] {
        mem.store(BASE + 18, 2, 0xabcd);
        return "_";
      });
    } else if (s == "mixed_load") {
      auto *x = node(known(HEAD, b));
      x->tag = 0x1200 + variant;
      read("mixed", ptr(x) + 16, 8);
      invoke("pad", [&] {
        poke(ptr(x) + 18, 2, 0xabcd);
        return "_";
      });
      read("after_pad", ptr(x) + 16, 8);
      p.remove(x);
      read("after_remove", ptr(x) + 16, 8);
      read("absent_padding", BASE + 20 * 20 + 18, 2);
    } else if (s == "detach") {
      known(HEAD, b);
      read("attached", HEAD);
      bridge.detach();
      read("detached", HEAD);
      invoke("ordinary", [&] {
        mem.store(HEAD, 4, 0xabcdef01);
        return "_";
      });
      read("ordinary", HEAD);
    } else
      throw std::runtime_error("unknown recipe");
  }
  std::string state() {
    flush();
    auto s = bridge.snapshot();
    std::vector<std::string> registry, nodes, headvalues, backing, writes, logs,
        calls;
    for (const auto &h : s.heads) {
      registry.push_back(
          sequence({n(h.address), std::to_string(h.identity),
                    n(h.object_serial), text(h.label), n(h.active)}));
    }
    for (auto h : heads)
      headvalues.push_back(sequence({n(h.first), n(ptr(h.second->next))}));
    for (const auto &x : s.nodes) {
      const auto index = (x.address - BASE) / 20;
      const auto next = index + 1 < 6096 ? x.address + 20 : 0;
      if (x.next != next || x.prev || x.partner || x.object || x.tag ||
          x.object_serial || x.opaque)
        nodes.push_back(
            sequence({n(x.address), n(x.next), n(x.prev), n(x.partner),
                      n(x.object), n(x.tag), n(x.object_serial), n(x.opaque)}));
    }
    for (auto x : model.mem.bytes())
      backing.push_back(sequence({n(x.first), n(x.second)}));
    for (auto &x : model.writes)
      writes.push_back(write(x));
    for (auto &x : model.mem.log())
      logs.push_back(write(x));
    for (auto &x : model.calls)
      calls.push_back(sequence({text(x.name), integers(x.args)}));
    return sequence(
        {sequence(results), snapshot(w.snapshot()), sequence(registry),
         n(s.attached), n(s.free), sequence(nodes), sequence(headvalues),
         sequence(backing), sequence(writes), sequence(logs), sequence(calls),
         sequence(effects), sequence(pokes), sequence(reads), n(model.draws)});
  }
};
std::pair<std::string, std::string> scenario(const std::string &name,
                                             std::int64_t config,
                                             const std::string &arguments) {
  const auto args = split(arguments, ';');
  if (args.size() != 2 || config < 0 || config > 15)
    throw std::runtime_error("invalid configuration");
  const auto id = integer(args[0]), variant = integer(args[1]);
  std::int64_t counter = 0;
  bool valid = false;
  for (int c = 0; c < 16; ++c)
    for (int v = 0; v < 2; ++v)
      for (auto s : names)
        if (c == 0 || floating(s)) {
          if (counter == id)
            valid = c == config && v == variant && name == s;
          ++counter;
        }
  if (!valid)
    throw std::runtime_error("inconsistent scenario identity");
  R::ScopedFpu context(
      R::Fpu(static_cast<R::Fpu::Model>(config / 4), config & 2, config & 1));
  Replay replay{unsigned(variant)};
  replay.recipe(name);
  return {"ok", replay.state()};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x37d934a45a894e7fULL;
constexpr std::size_t expected_rows = 258;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: raw-handles-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 64ULL * 1024 * 1024;
  constexpr std::size_t max_line = 1024 * 1024;
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
  if (line() != "# raw_handles_vectors_v1")
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
      std::cerr << "raw-handles mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
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
    throw std::runtime_error("incomplete or changed raw-handles corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows << " exact raw-handles vectors passed; FNV1a64=" << std::hex
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
