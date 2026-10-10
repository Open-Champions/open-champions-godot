#include "champions/objects.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <type_traits>
namespace R = champions::simulation::rta;
namespace O = R::objects;
namespace {
unsigned checks = 0;
void check(bool value, const char *label) {
  ++checks;
  if (!value)
    throw std::runtime_error(label);
}
template <class E = O::ObjectError, class F>
void rejects(F fn, const char *label) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, label);
}
O::Props props(std::initializer_list<std::string> p) {
  return std::make_shared<std::vector<std::string>>(p);
}
O::Vec3 pos(int x = 0, int y = 0, int z = 0) {
  return {R::from_int(x), R::from_int(y), R::from_int(z)};
}
std::vector<std::uint64_t> ids(const std::vector<O::GameObject *> &v) {
  std::vector<std::uint64_t> out;
  for (auto *p : v)
    out.push_back(p->serial);
  return out;
}
void primitives() {
  check(O::cell_coord(0) == 60 && O::cell_coord(0xbf000000) == 60,
        "negative fractional cell truncation");
  check(O::cell_coord(R::from_int(-1)) == 59, "negative integer cell boundary");
  check(O::atoi(" \t-4294967297tail") == -1, "atoi signed wrap");
  check(O::atoi("+not digits") == 0, "atoi missing decimal digits");
  auto p = props({"NAME=upper", "name=one", "name=two", "hide=-2"});
  check(O::props_find(p, "NAME") == std::optional<std::string>("one"),
        "property key only folded");
  check(O::props_find(p, "name", 1) == std::optional<std::string>("two"),
        "nth property");
  check(!O::props_find(p, "name", -1), "negative nth absent");
  check(O::props_int(p, "hide") == -2, "integer property");
  check(O::strip_quotes("\"abc\"tail") == "abc" &&
            O::strip_quotes("abc\"tail") == "abc\"tail",
        "quote stripping");
  check(O::name_hash("ALPHA") == O::name_hash("alpha"),
        "ASCII hash case folding");
  rejects([&] { O::name_hash(std::string(1, char(0x80))); },
          "non-ASCII name refused");
  O::NameTable t;
  O::GameObject a(100), b(200);
  const auto slot = t.add("Alpha", &a);
  t.add("alpha", &b);
  check(t.find("ALPHA", 3) == std::vector<O::GameObject *>({&a, &b}),
        "name insertion order");
  check(t.find("alpha", 2) == std::vector<O::GameObject *>({&a}),
        "name null terminator capacity");
  t.remove("alpha", &a);
  check(t.keys[slot].has_value() && !t.objs[slot], "name tombstone retained");
  check(t.add("ALPHA", &a) == slot, "same-name tombstone reused");
  auto count = t.count;
  rejects([&] { t.add(std::string(1, char(0x80)), &a); }, "invalid add name");
  check(t.count == count + 1, "name count mutation precedes hash error");
  rejects([&] { t.find("alpha", 1); }, "invalid find limit");
}
void tasks() {
  O::TaskList t;
  t.add(1, 10, "one", 0);
  t.add(2, 10, "two", 0);
  t.add(3, 20, "draw", 1);
  check(!t.add(1, 0, "duplicate", 0), "task function uniqueness");
  std::vector<unsigned> ran;
  t.call(1, -1, [&](unsigned fn) {
    ran.push_back(fn);
    if (fn == 1) {
      t.remove(1);
      t.add(4, 15, "later", 0);
    }
  });
  check(ran == std::vector<unsigned>({1, 2, 4}),
        "task mutation cursor and later insertion");
  check(t.is_running("two") && !t.is_running("Two"), "task name exact case");
  O::TaskList full;
  for (unsigned i = 0; i < O::MAX_TASKS; ++i)
    full.add(i, 0, "", 0);
  rejects([&] { full.add(100, 0, "", 0); }, "task bound");
  O::TaskList throwing;
  throwing.add(1, 0, "a", 0);
  throwing.add(2, 0, "b", 0);
  rejects<std::runtime_error>(
      [&] {
        throwing.call(0, -1,
                      [&](unsigned) { throw std::runtime_error("authored"); });
      },
      "task callback exception");
  check(throwing.cursor == 0 && throwing.tasks.size() == 2,
        "task failure retains preadvance cursor");
}
void lifetime() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  auto p = props({"name=\"hero\"trailing"});
  auto *a = w.new_object(0x4c, pos(), {}, p);
  check(a && (*p)[0] == "name=hero", "constructor edits owned property alias");
  const auto oldserial = a->serial;
  const auto address = a->addr;
  w.run_every_frame(*a);
  w.delete_object(*a);
  check(w.names.find_first("hero") == a &&
            w.collect_in_box(pos(-10, -10, -10), pos(10, 10, 100), 10) ==
                std::vector<O::GameObject *>({a}),
        "deferred delete remains named and spatial");
  check(w.find_by_class("10GameObject", 10).empty(),
        "deferred delete leaves run list");
  w.exclude_mask = 4;
  a->flags |= 4;
  w.flush_delete_list();
  check(a->alive, "delete flush honors exclusion");
  w.exclude_mask = 0;
  w.flush_delete_list();
  check(!a->alive && w.objects.empty() && !w.names.find_first("hero"),
        "delete flush destroys object");
  auto *b = w.new_object(0x4c, pos());
  check(b && b->addr == address && b->serial != oldserial && a != b,
        "address reuse keeps allocation identity stable");
  auto snapshot = w.snapshot();
  check(snapshot.objects.size() == 2, "snapshot includes retired objects");
  b->pos[0] = R::ONE;
  check(snapshot.objects.back().pos[0] == 0, "snapshot values own storage");
  auto owner_cascade =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &cascade = *owner_cascade;
  auto *c = cascade.new_object(0x4c, pos());
  auto *d = cascade.new_object(0x4c, pos());
  cascade.delete_object(*c);
  cascade.on_destroy = [&](O::GameObject &o, std::int64_t f) {
    cascade.destroy(o, f);
    if (&o == c)
      cascade.delete_object(*d);
  };
  cascade.flush_delete_list();
  check(!c->alive && d->alive, "new deletion head waits for next flush");
  cascade.flush_delete_list();
  check(!d->alive, "cascade next flush");
}
void running() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  w.frame = 1;
  auto *a = w.new_object(0x4c, pos()), *b = w.new_object(0x4c, pos());
  w.run_every_frame(*a);
  w.run_every_frame(*b);
  std::vector<std::uint64_t> ran;
  w.on_run = [&](O::GameObject &o) {
    ran.push_back(o.serial);
    if (&o == b) {
      w.destroy(o);
      w.run_every_15(*a);
    }
  };
  w.run_objects();
  check(ran == std::vector<std::uint64_t>({b->serial, a->serial}),
        "self destruction and later-list scheduling");
  auto owner_twice =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &twice = *owner_twice;
  twice.frame = 1;
  auto *c = twice.new_object(0x4c, pos());
  twice.run_every_frame(*c);
  unsigned count = 0;
  twice.on_run = [&](O::GameObject &o) {
    ++count;
    if (count == 1)
      twice.run_every_60(o);
  };
  twice.run_objects();
  check(count == 2, "active to matching slow list runs twice");
  auto owner_relink =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &relink = *owner_relink;
  relink.frame = 1;
  auto *d = relink.new_object(0x4c, pos());
  relink.run_every_frame(*d);
  unsigned calls = 0;
  relink.on_run = [&](O::GameObject &o) {
    ++calls;
    relink.run_every_frame(o);
  };
  relink.run_objects();
  check(calls == 1, "self relink not revisited");
  relink.is_frozen = [] { return true; };
  relink.delete_object(*d);
  relink.run_objects();
  check(!d->alive && calls == 1, "frozen still flushes deletions");
  auto owner_failure =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &failure = *owner_failure;
  auto *e = failure.new_object(0x4c, pos());
  failure.run_every_frame(*e);
  failure.on_run = [](O::GameObject &) {
    throw std::runtime_error("authored run failure");
  };
  rejects<std::runtime_error>([&] { failure.run_objects(); },
                              "run exception propagates");
  auto s = failure.snapshot();
  check(std::any_of(s.links.begin(), s.links.end(),
                    [](const O::LinkSnapshot &l) {
                      return l.kind == R::LinkKind::Iterator && l.prev;
                    }),
        "failed walk retains safe iterator marker");
}
void spatial() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    R::set_fpu(static_cast<R::Fpu::Model>(cfg / 4), (cfg / 2) % 2, cfg % 2);
    auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner_w;
    auto *a = w.new_object(0x4c, pos()), *b = w.new_object(0x4c, pos());
    check(ids(w.collect_in_box(pos(-1, -1, -1), pos(1, 1, 100), 3)) ==
              std::vector<std::uint64_t>({b->serial, a->serial}),
          "box newest first");
    check(w.collect_in_box(pos(-1, -1, -1), pos(1, 1, 100), 1).empty(),
          "box reserves null slot");
    w.update_grid_cell(*a);
    check(w.collect_in_box(pos(-1, -1, -1), pos(1, 1, 100), 2).front() == a,
          "same-cell move reorders");
    w.add_to_always_draw(*a);
    w.move_off_grid(*a);
    check(w.is_off_grid(*a), "explicit offgrid");
    w.update_grid_cell(*a);
    check(!w.is_off_grid(*a), "update ignores flag2");
    a->flags |= 4;
    w.exclude_mask = 4;
    w.add_to_always_draw_once(*a);
    check(R::HandlePool::items(w.always_draw).size() == 2,
          "masked once allows duplicate");
    w.clear_always_draw();
    check((a->flags & 2) && !w.always_draw.next,
          "masked draw flag retained while links cleared");
    w.exclude_mask = 0;
    a->pos = pos(3, 4, 0);
    w.camera_focus = pos();
    check(!w.is_active(*a, 5) && w.is_active(*a, 6),
          "activity strict squared radius");
    check(!w.is_active(*a, 46341), "integer radius square wraps");
    check(w.is_near_view(pos(5, 5, 999), R::from_int(5), {}) == 1,
          "near view inclusive square ignores height");
    w.set_object_vis(a, 0, false, {b});
    a->pos = pos(99);
    check(w.centres.front() == pos(3, 4, 0), "visibility centre copied");
  }
  R::set_fpu(R::Fpu::Model::Ps2, false, false);
}
void tick() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  w.tasks.add(1, 0, "run", 0);
  w.tasks.add(2, 1, "draw", 1);
  w.pads[0] = {1, 7, 9};
  w.menu_lockout = 3;
  std::vector<std::int64_t> observed;
  unsigned ends = 0;
  w.end_of_tick = [&] { ++ends; };
  w.tasks_run(2, [&](unsigned fn) {
    check(fn == 1, "draw task excluded");
    observed.push_back(w.pads[0].pressed);
  });
  check(observed == std::vector<std::int64_t>({7, 0}) &&
            w.pads[0].pressed == 7 && w.pads[0].edge2 == 9,
        "pad edge once then restored");
  check(w.frame == 2 && ends == 2 && w.menu_lockout == 2,
        "offline ticks and held lockout");
  w.warmup_frames = 1;
  w.tasks_run(1, [](unsigned) {});
  check(w.frame == 2, "warmup inhibits frame advance");
  w.network_mode = 1;
  w.tasks_run(1, [](unsigned) {});
  check(w.frame == 2 && w.menu_lockout == 0, "network frame externally owned");
  w.network_mode = 0;
  w.warmup_frames = 0;
  w.end_of_tick = [] { throw std::runtime_error("end failure"); };
  rejects<std::runtime_error>([&] { w.tasks_run(1, [](unsigned) {}); },
                              "tick end failure");
  check(w.pads[0].pressed == 0 && w.frame == 3,
        "failed tick leaves partial state and unrestored edges");
}
void records() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  auto p = props({"name=loaded"});
  O::RecordBlob blob({O::Record("first", 65535, pos(1), p, {1, 2}),
                      O::Record("second", 3, pos(2), {}, {3})},
                     true);
  auto bytes = blob.build();
  check(blob.size() > 8 && bytes.size() > blob.size(),
        "record strings follow consumed record extent");
  std::vector<std::string> called;
  auto factory = [&](const std::optional<std::string> &name, O::Vec3 position,
                     std::int64_t angle, O::Props values) {
    called.push_back(*name);
    check(angle == -1 || angle == 3, "signed record angle");
    auto *o = w.new_object(0x4c, position, {}, values);
    if (*name == "first")
      w.delete_object(*o);
    return o;
  };
  unsigned loaded = 0;
  const auto consumed = w.load_records(
      blob, factory, false, {}, [&](O::GameObject &o, const O::Bytes &s) {
        ++loaded;
        check(o.class_name.has_value() && !s.empty(),
              "class published before state callback");
      });
  check(blob.relocated && consumed == blob.size() &&
            called == std::vector<std::string>({"first", "second"}) &&
            loaded == 2,
        "record factory state ordering");
  check(w.objects.size() == 1 && !w.names.find_first("loaded"),
        "record load flushes constructor deletion");
  O::RecordBlob filtered({O::Record("skip", 0, pos())});
  w.load_records(filtered, factory, true);
  check(filtered.relocated && called.size() == 2,
        "missing save filter skips all records");
  auto *created = O::create_object("created", pos(), 65535, {}, factory);
  check(created && created->class_name == std::optional<std::string>("created"),
        "factory class naming");
}

void failures_and_aliases() {
  {
    auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner_w;
    auto *a = w.new_object(0x4c, pos());
    w.delete_object(*a);
    w.on_destroy = [&](O::GameObject &o, std::int64_t) {
      o.tag = 77;
      throw std::runtime_error("destructor");
    };
    rejects<std::runtime_error>([&] { w.flush_delete_list(); },
                                "destructor failure propagates");
    auto snapshot = w.snapshot();
    check(a->alive && a->tag == 77 && snapshot.live.size() == 1,
          "destructor partial state retained");
    check(std::any_of(snapshot.links.begin(), snapshot.links.end(),
                      [](const O::LinkSnapshot &l) {
                        return l.kind == R::LinkKind::Iterator && l.prev;
                      }),
          "destructor failure sentinel retained");
  }
  {
    auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner_w;
    auto *a = w.new_object(0x4c, pos());
    w.delete_object(*a);
    w.is_frozen = []() -> bool { throw std::runtime_error("frozen"); };
    rejects<std::runtime_error>([&] { w.run_objects(); },
                                "frozen callback exception");
    check(!a->alive && w.pool_free_count == 1056,
          "flush and pool sample before frozen callback");
  }
  {
    auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner_w;
    auto d = std::make_shared<O::ClassDef>(
        "authored", std::vector<std::string>{"first", "second"});
    unsigned calls = 0;
    w.res_find = [&](const std::string &,
                     const std::string &) -> std::uint32_t {
      if (++calls == 2)
        throw std::runtime_error("resource");
      return 123;
    };
    const auto seed = w.shared.state;
    rejects<std::runtime_error>([&] { w.new_object(0x4c, pos(), d); },
                                "resource callback exception");
    auto snapshot = w.snapshot();
    check(snapshot.objects.size() == 1 && snapshot.live.empty() &&
              snapshot.shared_seed == seed,
          "constructor failure keeps allocation before RNG/link");
    check(d->entries[0].second == 123 && d->entries[1].second == 0 &&
              d->generation == 0,
          "resource partial entry writes retained");
    d->entries[0].second = 456;
    check(snapshot.objects[0].class_def->entries[0].second == 123,
          "snapshot owns class definition values");
  }
  {
    auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner_w;
    auto p = props({"name=alias"});
    auto d = std::make_shared<O::ClassDef>("lump",
                                           std::vector<std::string>{"asset"});
    auto *a = w.new_object(0x4c, pos(), d, p);
    auto *b = w.new_object(0x4c, pos(), d, p);
    auto snapshot = w.snapshot();
    p->push_back("new=field");
    d->entries[0].second = 456;
    check(a->props == b->props && a->class_def == b->class_def,
          "live shared input identity retained");
    check(snapshot.objects[0].props->size() == 1 &&
              snapshot.objects[1].class_def->entries[0].second == 0,
          "snapshot input values deeply owned");
  }
  {
    auto owner_w = std::make_unique<O::World>();
    auto &w = *owner_w;
    auto *a = w.new_object(0x4c, pos());
    w.delete_object(*a);
    rejects([&] { w.flush_delete_list(); },
            "default destructor requires handler");
    check(a->alive, "missing destructor keeps pending object");
  }
  {
    auto owner_w = std::make_unique<O::World>();
    auto &w = *owner_w;
    w.tasks.add(1, 0, "run", 0);
    w.is_frozen = [] { return false; };
    rejects([&] { w.tasks_run(1, [](unsigned) {}); },
            "default end tick requires handler");
    check(w.frame == 1, "missing end tick preserves earlier frame mutation");
  }
}
void free_spots_and_markers() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  unsigned blocked = 0, heights = 0;
  auto result = w.find_free_spot(
      pos(1, 2, 3), 0, 24, false,
      [&](R::Raw, const O::Vec3 &) {
        ++blocked;
        return R::ONE;
      },
      [&](R::Raw, const O::Vec3 &) {
        ++heights;
        return 0U;
      });
  check(result.first == pos(1, 2, 3) && result.second == 0 && blocked == 50 &&
            heights == 0,
        "blocked free-spot probes preserve centre");
  result = w.find_free_spot(
      pos(1, 2, 3), 0, 24, false, [](R::Raw, const O::Vec3 &) { return 0U; },
      [](R::Raw, const O::Vec3 &) { return R::from_int(-100); });
  check(result.second == 1 && result.first == pos(1, 2, -100),
        "far lower terrain accepted");
  w.start_rank = 3;
  const auto seed = w.shared.state;
  auto *a = w.player1_marker(w.pool.allocate(0x4c), pos(), 65537,
                             props({"name=entrance"}), "old", "entrance", false,
                             {}, {});
  R::SharedRandom expected(seed);
  check(a->flags == 3 && a->angle == 1 &&
            w.shared.state == (expected.draw(), expected.state),
        "rank3 marker only consumes constructor draw");
  check(a->vtable == O::VT_PLAYER1 && a->type_name == "7Player1",
        "marker type identity");
  auto owner_gate =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &gate = *owner_gate;
  gate.start_rank = 2;
  auto *m = gate.player1_marker(
      gate.pool.allocate(0x4c), pos(), 0, props({"from=gate"}), "else", "else",
      true, [](R::Raw, const O::Vec3 &) { return R::ONE; }, {});
  check(gate.start_rank == 3 && m->alive, "gate marker promotion");
  for (const auto &p : gate.spawn_pos)
    check(p == pos(), "failed marker spot retained centre");
}

void reaping_and_walks() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  auto *a = w.new_object(0x4c, pos(-15104, -15360)),
       *b = w.new_object(0x4c, pos(-15104, -15360));
  for (auto *o : {a, b}) {
    o->flags |= O::FLAG_GRID_B | O::FLAG_EXPENDABLE;
    w.update_grid_cell(*o);
  }
  w.pool_free_count = 250;
  w.camera_focus = a->pos;
  check(w.reap().empty(), "offline reaper spares near camera above threshold");
  w.network_mode = 1;
  w.exclude_mask = O::FLAG_EXPENDABLE;
  check(w.reap() == std::vector<O::GameObject *>({b}) && !b->alive && a->alive,
        "network reaper ignores mask and destroys first only");
  check(w.reap() == std::vector<O::GameObject *>({a}),
        "remaining reaper object next pass");
  w.frame = 899;
  auto cells = w.reap_cells();
  check(cells.front() == 14385 && cells.back() == 0,
        "reaper wrap at final grid cell");
  auto owner_order =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &order = *owner_order;
  auto *off = order.new_object(0x4c, pos(40000)),
       *ga = order.new_object(0x4c, pos()), *gb = order.new_object(0x4c, pos());
  gb->flags |= O::FLAG_GRID_B;
  order.update_grid_cell(*gb);
  std::vector<std::uint64_t> found;
  order.for_each_grid_object(
      [&](O::GameObject &o) { found.push_back(o.serial); });
  check(found ==
            std::vector<std::uint64_t>({off->serial, ga->serial, gb->serial}),
        "grid walker offgrid A B order");
  order.exclude_mask = 4;
  off->flags |= 4;
  order.delete_all_objects();
  check(order.objects.empty() && order.names.count == 0 &&
            order.tearing_down == 0,
        "teardown grid destruction ignores exclude mask");
}
void owned_cells() {
  auto owner_w = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner_w;
  auto *a = w.new_object(0x4c, pos());
  auto *b = w.new_object(0x4c, pos());
  R::Cell borrowed;
  rejects([&] { w.collect_in_sphere(R::from_int(10), pos(), borrowed); },
          "borrowed sphere head refused");
  auto &head = w.make_cell(0x90000000);
  auto added = w.collect_in_sphere(R::from_int(10), pos(), head);
  check(added == std::vector<O::GameObject *>({b, a}),
        "sphere returns box query order");
  check(R::HandlePool::items(head) == std::vector<R::Object *>({a, b}),
        "sphere destination reverse query order");
  auto snap = w.snapshot();
  check(std::any_of(snap.links.begin(), snap.links.end(),
                    [](const O::LinkSnapshot &l) {
                      return l.id == 0x90000000 && l.next;
                    }),
        "owned external head snapshot");
  rejects([&] { w.make_cell(0x90000000); },
          "duplicate external identity refused");
  rejects([&] { w.make_cell(0x5db3a8); },
          "external identity collision refused");
  auto owner_foreign =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &foreign = *owner_foreign;
  auto &other = foreign.make_cell(0x90000001);
  rejects([&] { w.collect_in_sphere(R::from_int(10), pos(), other); },
          "foreign owned head refused");
}
void callback_inputs_and_budgets() {
  auto owner = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &w = *owner;
  auto cd = std::make_shared<O::ClassDef>("original",
                                          std::vector<std::string>{"entry"});
  w.res_find = [&](const std::string &lump, const std::string &name) {
    cd->lump = "changed";
    cd->entries[0].first = "different";
    check(lump == "original" && name == "entry",
          "resource callback receives stable string values");
    return 7U;
  };
  rejects([&] { w.new_object(0x4c, pos(), cd); },
          "class entry mutation explicitly refused");
  check(w.snapshot().objects.size() == 1 && w.objects.empty(),
        "structural refusal retains constructor partial state");
  std::optional<std::string> classname = "before";
  auto *o =
      O::create_object(classname, pos(), 0, {},
                       [&](const std::optional<std::string> &name, O::Vec3 p,
                           std::int64_t, O::Props) {
                         classname = "after";
                         check(name == std::optional<std::string>("before"),
                               "factory receives stable classname");
                         return w.new_object(0x4c, p);
                       });
  check(o->class_name == std::optional<std::string>("before"),
        "factory result retains input classname");
  w.network_mode = 1;
  w.menu_lockout = 9;
  rejects<O::ResourceLimit>(
      [&] { w.tasks_run(O::MAX_WORK + 1, [](unsigned) {}); },
      "tick work cap refuses");
  check(w.menu_lockout == 0,
        "tick budget refusal retains earlier network lockout clear");
  auto p = props({std::string(O::MAX_BYTES + 1, 'x')});
  rejects<O::ResourceLimit>([&] { O::props_find(p, "x"); },
                            "property byte budget");
  for (unsigned i = 0; i < 66000; ++i)
    w.is_off_grid(*o);
  check(w.snapshot().iterator_serial >= 66000,
        "ordinary completed walkers do not exhaust lifetime cap");
}
void ownership_and_state_policy() {
  {
    auto owner = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner;
    R::Object foreign;
    foreign.id = 123;
    w.handles.add(w.run_active, &foreign);
    rejects([&] { w.run_objects(); },
            "foreign base object rejected before downcast");
  }
  {
    auto owner = std::make_unique<O::World>();
    auto &w = *owner;
    O::RecordBlob blob({O::Record("stateful", 0, pos(), {}, {1})}, true);
    rejects(
        [&] {
          w.load_records(blob, [&](const std::optional<std::string> &,
                                   O::Vec3 p, std::int64_t, O::Props props) {
            return w.new_object(0x4c, p, {}, props);
          });
        },
        "required load-state callback refused");
    check(blob.relocated && w.objects.size() == 1 &&
              w.objects.begin()->second->class_name ==
                  std::optional<std::string>("stateful"),
          "missing load-state retains constructed named object");
  }
  {
    auto owner = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
    auto &w = *owner;
    O::RecordBlob blob({O::Record("base", 0, pos(), {}, {1})}, true);
    w.load_records(
        blob, [&](const std::optional<std::string> &, O::Vec3 p, std::int64_t,
                  O::Props props) { return w.new_object(0x4c, p, {}, props); });
    check(w.objects.size() == 1,
          "explicit reference base permits no-op load-state");
  }
}
void explicit_policy() {
  static_assert(!std::is_copy_constructible<O::World>::value,
                "world stable identity");
  static_assert(!std::is_move_constructible<O::World>::value,
                "world stable callbacks");
  auto owner_w = std::make_unique<O::World>();
  auto &w = *owner_w;
  auto *a = w.new_object(0x4c, pos());
  w.run_every_frame(*a);
  rejects([&] { w.run_objects(); },
          "required runtime handlers not silently no-op");
  auto owner_negative =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto &negative = *owner_negative;
  negative.frame = -1;
  rejects([&] { negative.run_objects(); }, "negative slow-list index refused");
}
} // namespace
int main() {
  try {
    primitives();
    tasks();
    lifetime();
    running();
    spatial();
    tick();
    records();
    failures_and_aliases();
    free_spots_and_markers();
    reaping_and_walks();
    owned_cells();
    callback_inputs_and_budgets();
    ownership_and_state_policy();
    explicit_policy();
    std::cout << checks << " object runtime checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "object runtime check failed: " << e.what() << '\n';
    return 1;
  }
}
