#include "combat_runtime_internal.hpp"
#include <limits>
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Address ACTIVE = 0x5db3a8, RUN15 = 0x5db3b0, RUN60 = 0x5db3f0,
                  DELETED = 0x5db4e0, GRID = 0x5db4f0, GRID_B = 0x5e95f0,
                  OFF_GRID = 0x5f76f0;
int cell(Raw f, int bias = 0x3c00) {
  auto n = s32(Raw(to_int(f)) + Raw(bias));
  return n >= 0 ? n / 256 : -int((std::int64_t(-std::int64_t(n)) + 255) / 256);
}
} // namespace
struct World::State::Overlay final : creature_hp::ReadOverlay {
  State *s;
  explicit Overlay(State *p) : s(p) {}
  bool covers(Address a) const override {
    if (!s)
      throw Error("combat World memory expired");
    if (a >= HandlePool::NODE_BASE &&
        a < HandlePool::NODE_BASE +
                HandlePool::NODE_SIZE * HandlePool::NODE_COUNT)
      return true;
    auto i = s->heads.find(a & ~3u);
    // Empty heads have canonical zero backing. Trusted legacy models can
    // perform their documented clear; store-zero sequence. Public raw stores
    // are guarded.
    return i != s->heads.end() && i->second->next;
  }
  std::uint8_t read_byte(Address a) const override {
    Raw word = 0;
    unsigned shift = (a & 3) * 8;
    if (a >= HandlePool::NODE_BASE &&
             a < HandlePool::NODE_BASE +
                     HandlePool::NODE_SIZE * HandlePool::NODE_COUNT) {
      auto off = a - HandlePool::NODE_BASE;
      auto *n = s->handles.nodes()[off / 20];
      off %= 20;
      shift = (off & 3) * 8;
      switch (off / 4) {
      case 0:
        word = s->ptr(n->next);
        break;
      case 1:
        word = s->ptr(n->prev);
        break;
      case 2:
        word = s->ptr(n->partner);
        break;
      case 3:
        word = n->obj ? n->obj->id : n->object_raw;
        break;
      case 4:
        word = n->tag & 0xffff;
        break;
      }
    } else {
      auto i = s->heads.find(a & ~3u);
      if (i == s->heads.end())
        throw Error("unmanaged handle read");
      word = s->ptr(i->second->next);
    }
    return std::uint8_t(word >> shift);
  }
};
World::State::State(Services v, int mode, Composition c)
    : State(std::make_shared<WorldAuthority>(), std::move(v), mode, c) {}
World::State::State(std::shared_ptr<WorldAuthority> owner, Services v, int mode, Composition c)
    : authority(std::move(owner)), pool(authority->objects->pool),
      handles(authority->objects->handles), mem(authority->memory),
      model(mem, creature_hp::Limits{1024 * 1024, 65536,
                                            // A host resource bound, not an RTA value:
                                            // shared callback registration has over 128
                                            // names even in the earlier compositions.
                                            c == Composition::Full ? 512u : 256u,
                                            64}),
      movement(mem), services(std::move(v)), composition(c), camera_mode(mode) {
  if (mode != 0 && mode != 1)
    throw Unsupported("camera mode outside composed domain");
  overlay = std::make_shared<Overlay>(this);
  mem.attach_overlay(overlay);
  for (Address a : {ACTIVE, DELETED, OFF_GRID, 0x5db4e8u})
    head(a);
  for (unsigned k = 0; k < 15; ++k)
    head(RUN15 + 4 * k);
  for (unsigned k = 0; k < 60; ++k)
    head(RUN60 + 4 * k);
  for (unsigned k = 0; k < 14400; ++k) {
    head(GRID + 4 * k);
    head(GRID_B + 4 * k);
  }
  w(0x44fdc8, 0xf00);
  w(0x4f8418, ONE);
  w(0x4f714c, ONE);
  if (!full())
    w(0x403a04, 100000);
  w(0x4039c1, 1, 1);
  for (unsigned k = 0; k < 4; ++k)
    w(PLAYERS + k * RECORD_SIZE + 0x4b08, 2, 0xffff);
  if (full())
    full_fixtures();
  else
    tables();
  install_services();
}
World::State::~State() {
  // Handle nodes still linked into this World's own list heads (a creature's
  // target list, an external retained handle) are unlinked from both lists
  // first: their other list belongs to a host-owned object that may outlive
  // this World.
  for (auto &entry : owned_heads) handles.clear_nodes(*entry.second);
  overlay->s = nullptr;
  mem.detach_overlay(overlay);
  for (const auto &entry : objects) authority->unregister_object(*entry.second);
}
void World::State::note(std::string n, std::vector<std::uint64_t> a) {
  if (trace.size() >= 1000000)
    throw Error("combat trace budget exceeded");
  trace.push_back({std::move(n), std::move(a), {}});
}
World::State::Actor &World::State::actor(Address a) const {
  auto i = objects.find(a);
  if (i == objects.end())
    throw Error("not a live pool object: " + std::to_string(a));
  return *i->second;
}
Cell &World::State::head(Address a) {
  if (a & 3)
    throw Error("unaligned handle head");
  auto i = heads.find(a);
  if (i != heads.end())
    return *i->second;
  if (mem.load(a, 4) != 0)
    throw Error("handle head holds a raw value");
  Cell *cell = nullptr;
  auto &world = *authority->objects;
  if (a >= GRID && a < GRID + 4 * world.grid.size())
    cell = &world.grid[(a - GRID) / 4];
  else if (a >= GRID_B && a < GRID_B + 4 * world.grid_b.size())
    cell = &world.grid_b[(a - GRID_B) / 4];
  else if (a == ACTIVE) cell = &world.run_active;
  else if (a == DELETED) cell = &world.delete_list;
  else if (a == OFF_GRID) cell = &world.off_grid;
  else if (a == 0x5db4e8u) cell = &world.always_draw;
  else if (a >= RUN15 && a < RUN15 + 4 * world.run15.size())
    cell = &world.run15[(a - RUN15) / 4];
  else if (a >= RUN60 && a < RUN60 + 4 * world.run60.size())
    cell = &world.run60[(a - RUN60) / 4];
  else {
    auto p = std::make_unique<Cell>();
    cell = p.get();
    owned_heads.emplace(a, std::move(p));
  }
  links[cell] = a;
  heads.emplace(a, cell);
  return *cell;
}
Address World::State::ptr(const Link *p) const {
  if (!p)
    return 0;
  if (p->kind == LinkKind::Node)
    return HandlePool::NODE_BASE +
           HandlePool::NODE_SIZE * Raw(static_cast<const Node *>(p)->index);
  auto i = links.find(p);
  if (i != links.end())
    return i->second;
  for (const auto &entry : authority->objects->objects)
    for (unsigned k = 0; k < 3; ++k)
      if (p == &entry.second->heads[k])
        return entry.first + 4 * k;
  throw Error("iterator is not a readable node");
}
Address World::State::target(Address a) const {
  auto i = heads.find(a);
  if (i == heads.end() || !i->second->next)
    return 0;
  auto *o = i->second->next->obj;
  return o ? o->id : 0;
}
void World::State::link(Address a, Address back, Address obj) {
  auto *o = authority->find(obj);
  if (!o || (!objects.count(obj) && !authority->is_factory(obj)))
    throw Error("not a registered combat or ported factory object");
  Cell *back_cell = nullptr;
  // A script-host factory object's own head wins over any cached combat head
  // left at its address by a reused pool block.
  if (authority->is_factory(obj) && back >= obj && back - obj <= 8 && !((back - obj) & 3)) {
    auto external = authority->objects->objects.find(obj);
    if (external != authority->objects->objects.end())
      back_cell = &external->second->heads[(back - obj) / 4];
  }
  if (!back_cell) {
    auto i = heads.find(back);
    if (i != heads.end()) back_cell = i->second;
  }
  if (!back_cell && back == obj) {
    auto external = authority->objects->objects.find(obj);
    if (external != authority->objects->objects.end())
      back_cell = &external->second->heads[0];
  }
  if (!back_cell) throw Error("back reference is not a registered head");
  handles.link(head(a), *back_cell, o);
}
void World::State::clear(Address a) {
  handles.clear(head(a));
  mem.poke(a, 4, 0);
}
void World::State::relink(Address a, Address to) {
  if (to && !objects.count(to) && !authority->is_factory(to))
    throw Error("not a registered combat or ported factory object");
  clear(a);
  if (to)
    link(a, to, to);
}
void World::State::remove(Address a) {
  if (a < HandlePool::NODE_BASE || (a - HandlePool::NODE_BASE) % 20)
    throw Error("invalid node");
  auto k = (a - HandlePool::NODE_BASE) / 20;
  if (k >= handles.nodes().size())
    throw Error("invalid node");
  handles.remove(handles.nodes()[k]);
}
Address World::State::home(Address a, bool flag2) const {
  auto f = u(a + 12);
  auto p = vec(a + 16);
  auto x = cell(p[0]), y = cell(p[1]);
  if (x < 0 || x >= 120 || y < 0 || y >= 120 || (flag2 && (f & 2)))
    return OFF_GRID;
  return ((f & 0x800) ? GRID_B : GRID) + 4 * Raw(y * 120 + x);
}
void World::State::grid(Address a) {
  actor(a);
  auto *back = head(a + 8).next;
  if (!back || !back->partner)
    throw Error("object has no grid node");
  auto *n = back->partner;
  HandlePool::unlink(n);
  HandlePool::push(head(home(a, false)), n);
}
void World::State::schedule(Address a, unsigned period) {
  actor(a);
  if (period != 1 && period != 15 && period != 60)
    throw Error("run period must be 1, 15 or 60");
  if (head(a + 4).next)
    handles.remove(head(a + 4).next);
  Address list = ACTIVE;
  if (period != 1) {
    Address cursor = period == 15 ? 0x403a00 : 0x4039fc;
    auto k = u(cursor) + 1;
    if (k >= period)
      k = 0;
    w(cursor, k);
    list = (period == 15 ? RUN15 : RUN60) + 4 * k;
  }
  link(list, a + 4, a);
}
Address World::State::alloc_fixture(unsigned n) {
  auto a = fixture_next;
  if (!n || n > 0x100000u - a ||
      ((std::uint64_t(n) + 15) & ~UINT64_C(15)) >= 0x100000u - a)
    throw Error("fixture allocation exhausted");
  fixture_next += (n + 15) & ~15u;
  fixture_extents.emplace(a, n);
  return a;
}
Address World::State::construct(Address a, const Vec3 &pos, Kind kind) {
  // GameObject_ctor on a fresh block (ref_objects World.construct): heads,
  // bound fields, grid link at the front, then ONE shared draw.
  auto extent = Pool::CLASSES[pool.class_of(a)][0];
  prop_model_pointers.erase(a);
  forget_heads(a, a + extent);
  for (unsigned k = 0; k < extent; ++k)
    mem.poke(a + k, 1, 0);
  auto p = std::make_unique<Actor>();
  p->id = a;
  p->kind = kind;
  p->player = kind == Kind::Player;
  objects[a] = std::move(p);
  authority->register_object(*objects.at(a));
  for (unsigned off : {0, 4, 8})
    head(a + off);
  vec(a + 16, pos);
  w(a + 0x1c, 0x41c00000);
  w(a + 0x20, 0x42900000);
  w(a + 0x48, VT_GAMEOBJECT);
  link(home(a, true), a + 8, a);
  w(a + 0x44, draw());
  return a;
}
void World::State::forget_heads(Address lo, Address hi) {
  for (auto i = heads.lower_bound(lo); i != heads.end() && i->first < hi;) {
    if (i->second->next)
      throw Error("reused pool block retains a handle");
    if (!full()) {
      ++i;
      continue;
    }
    auto owned = owned_heads.find(i->first);
    if (owned == owned_heads.end())
      throw Error("shared World list head lies in a reused pool block");
    retired_cells.push_back(std::move(owned->second));
    owned_heads.erase(owned);
    i = heads.erase(i);
  }
}
void World::State::write_table(Address table,
                              const std::map<unsigned, Raw> *extra) {
  constexpr Raw base[] = {
      0x3c2608, 0x117128, 0x116638, 0x11a5f0, 0x3c7e98, 0x3c7ea0, 0x11a418,
      0x11a260, 0x116920, 0x116780, 0x11a3f0, 0x11a448, 0x11a290, 0x116d50,
      0x116bd8, 0x11c298, 0x3c2738, 0x11a868, 0x118410, 0x118418, 0x119328,
      0x119400, 0x10de10, 0x10ded0, 0x10e2c0, 0x10e658, 0x10bb08, 0x3c2688,
      0x10beb8, 0x11a668, 0x118a58, 0x118af8, 0x10bfc8, 0x3c2700, 0x3c2708,
      0x118c50, 0x118c78, 0x3c2730, 0x3c2748};
  std::map<unsigned, Raw> slots;
  for (unsigned k = 0; k < 39; ++k)
    slots[k] = base[k];
  if (table == VT_PLAYER)
    for (auto p : std::map<unsigned, Raw>{{0, 0x3c3ab8},
                                          {1, 0x1a1bb8},
                                          {2, 0x1b5330},
                                          {4, 0x1a5030},
                                          {5, 0x1a46c0},
                                          {6, 0x1beaf0},
                                          {15, 0x11e870},
                                          {16, 0x3c3b38},
                                          {20, 0x1b5ea8},
                                          {27, 0x1b5d68},
                                          {29, 0x3c3b08},
                                          {30, 0x3c3b10},
                                          {37, 0x3c3b18},
                                          {39, 0x3c3b40}})
      slots[p.first] = p.second;
  if (table == VT_SKELIGHT)
    for (auto p : std::map<unsigned, Raw>{{0, 0x3cc6dc},
                                          {1, 0x3cc67c},
                                          {4, 0x2a8b84},
                                          {5, 0x2a8a64},
                                          {8, 0x2a90b0},
                                          {9, 0x2a8fdc},
                                          {15, 0x126b58},
                                          {27, 0x2a91b8}})
      slots[p.first] = p.second;
  if (extra)
    for (const auto &p : *extra)
      slots[p.first] = p.second;
  for (auto &p : slots) {
    w(table + 8 * p.first, 2, 0);
    w(table + 8 * p.first + 4, p.second);
  }
}
Address World::State::spawn(const Spawn &v, bool player, unsigned slot,
                            int strength, int level) {
  if (player && (slot >= 4 || u(PLAYERS + slot * RECORD_SIZE + 0x43d8)))
    throw Error("player slot occupied or out of range");
  auto a = pool.allocate(player ? 0x590 : 0x3c8);
  if (!a)
    throw Error("actor pool exhausted");
  if (full()) {
    construct(a, v.position, player ? Kind::Player : Kind::Creature);
    auto table = player ? VT_PLAYER : VT_SKELIGHT;
    w(a + 0x48, table);
    write_table(table);
  } else {
    auto extent = Pool::CLASSES[pool.class_of(a)][0];
    forget_heads(a, a + extent);
    for (unsigned k = 0; k < extent; ++k)
      mem.poke(a + k, 1, 0);
    auto p = std::make_unique<Actor>();
    p->id = a;
    p->player = player;
    p->kind = player ? Kind::Player : Kind::Creature;
    objects[a] = std::move(p);
    authority->register_object(*objects.at(a));
  }
  for (unsigned off : {0, 4, 8, 0x234, 0x238, 0x2e4, 0x360, 0x364})
    head(a + off);
  if (player) {
    head(a + 0x588);
    head(a + 0x4b8);
  }
  if (!full()) {
    vec(a + 16, v.position);
    w(a + 0x1c, 0x41c00000);
    w(a + 0x20, 0x42900000);
    link(home(a, true), a + 8, a);
    w(a + 0x44, draw());
    w(a + 0x48, player ? VT_PLAYER : VT_SKELIGHT);
  }
  w(a + 12, player ? 0x60 : 0x20);
  w(a + 0x1c, player ? 0x41800000 : 0x41400000);
  w(a + 0x38, 2, v.facing);
  w(a + 0xb0, player ? 0x3fb33333 : 0x3fc00000);
  w(a + 0xdc, ONE);
  w(a + 0xe4, 2, Raw(v.armor));
  w(a + 0x228, v.hp);
  if (player) {
    auto r = PLAYERS + slot * RECORD_SIZE;
    auto def = alloc_fixture(v.session_record ? 0x400 : 0x340);
    w(a + 0x24, def);
    if (!v.session_record)
      for (unsigned k = 0; k < 0x340; k += 4)
        w(def + k, 0xa0000000 + k);
    w(a + 0x3c8, slot);
    w(a + 0x58c, 2);
    w(a + 0x4b2, 2, 0x7fff);
    w(r + 0x43d8, a);
    if (!v.session_record) {
      w(r + 0x50, 1);
      w(r + 0x58, 1, Raw(level));
      w(r + 0x59, 1, Raw(level));
      w(r + 0x90, Raw(strength));
      w(r + 0xac, v.hp);
      w(r + 0xb0, v.hp);
    }
    // Weapon class 5 = unarmed (research docs/player.md). For the Full fixture
    // this is the authored setup. For a session record (ordinary play) it is a
    // HOST STAND-IN: the research does not establish the route's equipped item,
    // so the player is left unarmed and the caller counts a refusal.
    w(r + 0x4a86, 1, 5);
    w(r + 0x4c24, 1, Raw(v.team));
    w(NUM_PLAYERS, std::max(u(NUM_PLAYERS), slot + 1));
    if (full()) {
      // PlayerAnim.reset_player: the four base cells player_resetIdleAnim
      // leaves (authored animation fixture), the idle-voice limit and the
      // record speed multiplier (MoveRuntime set-up).
      if (!v.session_record) {
        w(a + 0x118, 1, 4);
        w(a + 0x11a, 2, 0);
        for (unsigned k = 0; k < 4; ++k)
          w(a + 0x11c + 0x20 * k, 0xa0000044 + 8 * k);
      }
      w(a + 0x4b2, 2, v.idle_voice_limit);
      w(r + 0x4c20, v.speed_mul);
    }
  } else {
    auto def = alloc_fixture(0x80);
    for (unsigned k = 1; k < 16; ++k)
      w(def + 8 * k + 4, 0xc1100000 + 8 * k);
    w(def + 12, 0);
    w(a + 0x24, def);
    w(a + 0xa8, 8, v.creature_flags | (UINT64_C(1) << 63));
    w(a + 0xb4, 1, 0x10);
    w(a + 0xd0, 2, Raw(v.minimum));
    w(a + 0xd2, 2, Raw(v.maximum));
    w(a + 0xd4, 2, Raw(v.attack));
    w(a + 0x22c, v.hp);
    w(a + 0x310, 1, Raw(v.team));
    w(a + 0x3ac, 2, 48);
    w(a + 0x3ae, 2, 200);
    w(a + 0x3b0, 2, 0x2000);
    vec(a + 0x3b4, v.position);
    w(a + 0x3c0, v.home_range);
  }
  schedule(a, 1);
  if (player)
    alive();
  return a;
}
void World::State::erase_later(Address a) {
  if (actor(a).player)
    throw Unsupported("a dead player is never deleted");
  note("obj_delete", {a});
  if (head(a + 4).next)
    handles.remove(head(a + 4).next);
  link(DELETED, a, a);
}
void World::State::destroy(Address a) {
  if (!full())
    return destroy_round1(a);
  // Slot 1 with 3 (research FullRuntime._slot1).
  actor(a);
  auto fn = u(u(a + 0x48) + 12);
  if (fn == 0x151454 || fn == 0x3c6468)
    return projectile_destroy(a, 3);
  if (fn == DISEASE_CLOUD_DTOR)
    throw Unsupported(DISEASE_CLOUD_DTOR_REFUSAL);
  if (combat_ctors::is_class_dtor(fn)) {
    note("destroy", {a});
    return class_dtor(a, fn, 3);
  }
  if (combat_ctors::is_prop_dtor(fn)) {
    note("destroy",{a});
    combat_ctors::prop_dtor(model,fn,a,3);
    return;
  }
  if (fn == 0x238750) {
    note("destroy",{a});
    combat_ctors::antwave_dtor(model,a,3);
    return;
  }
  if (fn == 0x3cf534 || fn == 0x3c5b90 || fn == 0x3c2940 || fn == 0x3c6888) {
    note("destroy", {a});
    return effect_dtor(fn, a, 3);
  }
  if (fn == 0x117128 || fn == 0x3cc67c) {
    note("destroy", {a});
    if (target(a + 0x364))
      throw Unsupported("destroying a creature with attached spells");
    for (unsigned off : {0x364, 0x360, 0x2e4, 0x238, 0x234})
      clear(a + off);
    gameobject_dtor_call(a, 3);
    return;
  }
  if (fn == 0x1a1bb8)
    throw Unsupported("Player_dtor: a player object is not destroyed");
  note("destroy", {a});
  gameobject_dtor_call(a, 3);
}
void World::State::gameobject_dtor_call(Address a, Raw flags) {
  // GameObject_dtor 0x14cf90 on the shared world (no name properties here):
  // the GameObject table, then +8, +4, +0 cleared; bit 0 frees the block.
  auto &o = actor(a);
  w(a + 0x48, VT_GAMEOBJECT);
  for (unsigned off : {8, 4, 0})
    clear(a + off);
  animations.erase(a);
  if (flags & 1) {
    auto extent = Pool::CLASSES[pool.class_of(a)][0];
    pool.free(a);
    o.alive = false;
    authority->unregister_object(o);
    retired.push_back(std::move(objects.at(a)));
    objects.erase(a);
    forget_heads(a, a + extent);
  }
}
void World::State::destroy_round1(Address a) {
  auto &o = actor(a);
  if (o.player)
    throw Unsupported("Player_dtor remains partial");
  if (target(a + 0x364) || u(a + 0x320) || u(a + 0x50))
    throw Unsupported("complete destructor chain, attachments or spells");
  note("destroy", {a});
  for (unsigned off : {0x364, 0x360, 0x2e4, 0x238, 0x234}) {
    note("clear", {a + off});
    clear(a + off);
  }
  animations.erase(a);
  w(a + 0x48, 0x5122a8);
  for (unsigned off : {8, 4, 0}) {
    note("clear", {a + off});
    clear(a + off);
  }
  pool.free(a);
  o.alive = false;
  authority->unregister_object(o);
  retired.push_back(std::move(objects.at(a)));
  objects.erase(a);
}
std::vector<Address> World::State::box(const Vec3 &lo, const Vec3 &hi,
                                       int limit) {
  std::vector<Address> out;
  auto mask = u(0x4039b8);
  for (int r = std::max(cell(lo[1], 0x3ba0), 0);
       r <= std::min(cell(hi[1], 0x3c60), 119); ++r)
    for (int c = std::max(cell(lo[0], 0x3ba0), 0);
         c <= std::min(cell(hi[0], 0x3c60), 119); ++c) {
      for (auto *n = head(GRID + 4 * Raw(r * 120 + c)).next; n; n = n->next) {
        if (!n->obj)
          continue;
        auto a = n->obj->id;
        if (!objects.count(a) && !authority->is_factory(a)) {
          // A host stub (a record whose class constructor is not ported) is
          // registered with the authority but has no constructed state to
          // collide with. It is left out of the query and counted; an object
          // the authority does not know at all is still a refusal.
          if (!authority->find(a))
            throw Error("grid node outside the shared World registry");
          ++stub_box_skips;
          continue;
        }
        auto f = u(a + 12);
        if (f & (mask | 0x20000))
          continue;
        auto p = vec(a + 16);
        auto rad = u(a + 0x1c);
        if (lt(add(p[0], rad), lo[0]) || lt(hi[0], sub(p[0], rad)) ||
            lt(add(p[1], rad), lo[1]) || lt(hi[1], sub(p[1], rad)) ||
            lt(hi[2], p[2]) || lt(add(p[2], u(a + 0x20)), lo[2]))
          continue;
        if (std::int64_t(out.size()) >= std::int64_t(limit) - 1) {
          note("box", {out.begin(), out.end()});
          return out;
        }
        out.push_back(a);
      }
    }
  note("box", {out.begin(), out.end()});
  return out;
}
void World::State::reserve_iterator() {
  if (retention == IteratorRetention::Always || (retention == IteratorRetention::Lazy && iterators.size() >= prune_at)) {
    prune_iterators();
    prune_at = std::max<std::size_t>(4096, 2 * iterators.size());
  }
  if (iterators.size() >= 65536)
    throw Error("combat iterator retention budget exceeded");
}
void World::State::end_iterator(Iterator &it) {
  it.end();
  ended_iterators.insert(&it);
}
void World::State::prune_iterators() {
  // An ended iterator keeps its next/prev/partner (a stale node reference may
  // read its prev link), so a retained ended iterator keeps every ended
  // iterator it links to alive too. The kept set is the closure of the roots
  // (list heads, pool nodes, object heads, live iterators) under the links of
  // the iterators already kept: no retained iterator is left holding a pointer
  // to a released one.
  std::set<const Link *> referred;
  authority->objects->collect_link_references(ended_iterators, referred);
  std::vector<const Link *> pending;
  auto add_refs = [&](const Link &n) {
    for (const auto *ref : {n.next, n.prev, n.partner})
      if (ref && ended_iterators.count(ref) && referred.insert(ref).second)
        pending.push_back(ref);
  };
  for (const auto *ref : referred) pending.push_back(ref);
  for (const auto &entry : owned_heads) add_refs(*entry.second);
  for (const auto &entry : heads) add_refs(*entry.second);
  for (const auto &it : iterators)
    if (!ended_iterators.count(it.get())) add_refs(*it);
  while (!pending.empty()) {
    const auto *kept_iterator = pending.back();
    pending.pop_back();
    add_refs(*kept_iterator);
  }
  std::vector<std::unique_ptr<Iterator>> kept;
  kept.reserve(iterators.size());
  for (auto &it : iterators) {
    if (ended_iterators.count(it.get()) && !referred.count(it.get())) {
      ended_iterators.erase(it.get());
    } else
      kept.push_back(std::move(it));
  }
  iterators = std::move(kept);
}
void World::State::walk(Address h, const std::function<void(Address)> &fn) {
  reserve_iterator();
  auto p = std::make_unique<Iterator>(head(h), 0);
  auto &it = *p;
  iterators.push_back(std::move(p));
  try {
    while (!it.done()) {
      auto *o = it.current();
      if (o && !(u(o->id + 12) & u(0x4039b8)))
        fn(o->id);
      if (!it.done())
        it.advance();
    }
    end_iterator(it);
  } catch (...) {
    end_iterator(it);
    throw;
  }
}
void World::State::run_objects() {
  domain();
  walk(DELETED, [&](Address a) { destroy(a); });
  if (!(u(FRAME) & 63) && !full())
    w(0x403a04, Raw(pool.count_free()));
  if (pool.count_free() < 300)
    throw Unsupported("pool pressure reaper outside combat actor domain");
  walk(ACTIVE, [&](Address a) {
    if (!(u(a + 12) & 0x8000))
      run(a);
  });
  walk(RUN15 + 4 * (u(FRAME) % 15), [&](Address a) {
    if (!(u(a + 12) & 0x8000))
      run(a);
  });
  walk(RUN60 + 4 * (u(FRAME) % 60), [&](Address a) {
    if (!(u(a + 12) & 0x8000))
      run(a);
  });
}
void World::State::alive() {
  unsigned count = 0;
  for (unsigned k = 0; k < u(NUM_PLAYERS); ++k) {
    auto a = u(PLAYERS + k * RECORD_SIZE + 0x43d8);
    if (a && objects.count(a) && !(u(a + 12) & 8)) {
      w(0x4f8440 + 4 * count, k);
      w(0x4f8430 + 4 * count, k);
      ++count;
    }
  }
  w(0x4f8420, count);
  w(0x4f8424, count);
  w(0x4f841c, count ? u(0x4f8430) : UINT32_MAX);
}
void World::State::tick() {
  domain();
  if (full()) {
    // Observations of the borrowed models are not retained across ticks.
    model.writes.clear();
    model.calls.clear();
    model.events.clear();
    movement.writes.clear();
    movement.calls.clear();
    movement.events.clear();
    mem.clear_log();
  }
  note("visibility");
  w(0x4039c1, 1, camera_mode == 0 ? 1 : 0);
  unsigned n = 0;
  for (unsigned k = 0; k < u(NUM_PLAYERS); ++k) {
    auto a = u(PLAYERS + k * RECORD_SIZE + 0x43d8);
    if (a && objects.count(a))
      vec(0x4039c8 + 12 * n++, vec(a + 16));
  }
  w(0x403a08, 1, n);
  run_objects();
  // Authored ordering (audit C3-6): gold-run.md has hud_tickGold run from
  // hud_run, a run task, so its order against the object runs is not pinned;
  // it is placed after them here. hud_flush_gold is never called, and the
  // gore option 0x44fdc0 stays 0 (the research gives no host value), so
  // gutchunk_ctor sets bit 1. None of the three is reached by the 300-tick
  // Full trace.
  if (full())
    combat_ctors::hud_tick_gold(model);
  w(FRAME, u(FRAME) + 1);
  if (full())
    for (unsigned k = 0; k < 4; ++k) {
      w(PADS + k * PAD_SIZE + 0xc, 0);
      w(PADS + k * PAD_SIZE + 0x10, 0);
    }
  alive();
  if (u(NUM_PLAYERS) && !u(0x4f8420))
    throw Unsupported("party wipe: deathTask");
}
void World::State::domain() const {
  if (b(0x402791) || b(0x44fcd0) || u(0x4f711c) || u(0x4f7120) || u(0x44fcf8) ||
      u(0x44fdc0))
    throw Unsupported("network, cutscene, PvP, damage numbers or decals");
  if (u(NUM_PLAYERS) > 4)
    throw Error("player count out of bounds");
}
void World::State::actor_domain(Address a) const {
  actor(a);
  if (full())
    return;
  if (target(a + 0x364) || u(a + 0x50))
    throw Unsupported("spells or weapon holder");
  if (u(a + 12) & 0x20000)
    throw Unsupported("prediction copy runtime");
}
Raw World::State::draw() {
  auto next = seed() * 0x19660dU + 0x3c6ef35fU;
  set_seed(next);
  note("shared_draw", {next >> 16, next});
  return next >> 16;
}
Raw World::State::own(Address a) {
  auto v = u(a + 0x44) * 0x19660dU + 0x3c6ef35fU;
  w(a + 0x44, v);
  return v >> 16;
}
Raw World::State::record(Address a) const {
  if (!actor(a).player)
    throw Error("player service on nonplayer");
  auto slot = u(a + 0x3c8);
  if (slot >= 4)
    throw Error("invalid player slot");
  return PLAYERS + slot * RECORD_SIZE;
}
Raw World::State::skill_rank(Raw slot, Raw id) const {
  if (slot >= 4)
    throw Error("invalid skill slot");
  auto p = PLAYERS + slot * RECORD_SIZE + 0x4b08;
  for (unsigned k = 0; k < 64; ++k, p += 4) {
    if (s16(h(p)) == -1)
      return 0;
    if (s16(h(p)) == s32(id))
      return Raw(s16(h(p + 2)));
  }
  throw Error("unterminated skill rank list");
}
void World::State::full_fixtures() {
  // research ref_combat_full.FullRuntime.__init__, in order.
  constexpr std::pair<unsigned, Raw> missile[] = {
      {0, 0x3c62c0}, {1, 0x151454}, {2, 0x3c7e88},  {3, 0x3c7e90},
      {4, 0x15153c}, {5, 0x15253c}, {6, 0x152a18},  {7, 0x3c7eb0},
      {8, 0x3c7eb8}, {9, 0x3c7ec0}, {10, 0x3c7ec8}, {11, 0x3c7ef0},
      {12, 0x3c7ef8}, {13, 0x3c7f20}, {14, 0x3c7f28}, {15, 0x12d498},
      {16, 0x3c8120}, {17, 0x153314}};
  for (auto &p : missile) {
    w(VT_MISSILE + 8 * p.first, 2, 0);
    w(VT_MISSILE + 8 * p.first + 4, p.second);
  }
  for (auto &p : missile) {
    Raw fn = p.second;
    switch (p.first) {
    case 0: fn = 0x3c64d8; break;
    case 1: fn = 0x3c6468; break;
    case 4: fn = 0x1ba940; break;
    case 5: fn = 0x1ba730; break;
    case 15: fn = 0x12d9e0; break;
    default: break;
    }
    w(VT_THROWN + 8 * p.first, 2, 0);
    w(VT_THROWN + 8 * p.first + 4, fn);
  }
  // Type-info descriptors' name words (mangled form), sorted by descriptor.
  const std::pair<Raw, const char *> names[] = {
      {0x908130, "8Creature"},        {0x908140, "9Container"},
      {0x908330, "13MissileWeapon"},  {0x9083d0, "6Player"},
      {0x908560, "15PushPhysicsProp"}, {0x9089e0, "13AutoContainer"},
      {0x9089f0, "8AutoProp"},        {0x908e10, "8Skelight"},
      {0x908e20, "8Skeleton"}};
  for (auto &n : names) {
    std::string text = n.second;
    auto at = alloc_fixture(unsigned(text.size() + 1));
    for (unsigned k = 0; k <= text.size(); ++k)
      w(at + k, 1, k < text.size() ? Raw(std::uint8_t(text[k])) : 0);
    w(n.first, at);
  }
  head(STACK_HEAD);
  const Raw defaults[] = {0x3c7eb0, 0x3c7eb8, 0x3c7ec0, 0x3c7ec8,
                          0x3c7ef0, 0x3c7ef8, 0x3c7f20, 0x3c7f28};
  auto object_table = [&](Address table, std::initializer_list<Raw> head7,
                          std::initializer_list<Raw> tail2) {
    std::vector<Raw> words(head7);
    words.insert(words.end(), std::begin(defaults), std::end(defaults));
    words.insert(words.end(), tail2);
    for (unsigned k = 0; k < words.size(); ++k) {
      w(table + 8 * k, 2, 0);
      w(table + 8 * k + 4, words[k]);
    }
  };
  object_table(VT_HOLYSTRIKE,
               {0x3cf5bc, 0x3cf534, 0x3c7e88, 0x3c7e90, 0x2ebc50, 0x2ecd90,
                0x2ecae8},
               {0x11c7f8, 0x3c8120});
  object_table(VT_SPARKS,
               {0x3c5c18, 0x3c5b90, 0x3c7e88, 0x3c7e90, 0x28fa3c, 0x2902e4,
                0x2900c8},
               {0x12c668, 0x3c8120});
  object_table(VT_LIGHTEFFECT,
               {0x3c6908, 0x3c6888, 0x3c7e88, 0x3c7e90, 0x1ee050, 0x3c7ea0,
                0x3c7ea8},
               {0x12e690, 0x3c8120});
  const Raw spell[] = {0x3d1c4c, 0x3d1bd8, 0x317000, 0x3cb04c, 0x3cb054};
  for (unsigned k = 0; k < 5; ++k) {
    w(VT_SPELL_WAVE + 8 * k, 2, 0);
    w(VT_SPELL_WAVE + 8 * k + 4, spell[k]);
  }
  // AUTHORED: the timed-modifier id list 0x537c08 is empty (-1 first).
  w(G_TIMED_MODIFIERS, 0xffffffff);
  // Round-3 class tables are written by the constructor that first uses them.
  // Seeding them here would put words into every Full image, and the round-2
  // digest still expects those addresses to stay zero.
}
World::World(Services s, int m, Composition c)
    : World(std::make_shared<WorldAuthority>(), std::move(s), m, c) {}
World::World(std::shared_ptr<WorldAuthority> authority, Services s, int m, Composition c)
    : s_(std::make_unique<State>(std::move(authority), std::move(s), m, c)) {}
Composition World::composition() const { return s_->composition; }
World::~World() = default;
const creature_hp::Memory &World::memory() const { return s_->mem; }
std::uint64_t World::read(Address a, unsigned n) const {
  return s_->mem.load(a, n);
}
void World::write(Address a, unsigned n, std::uint64_t v) {
  if (n > 8 || std::uint64_t(a) + n > UINT64_C(0x100000000))
    throw Error("raw write range");
  for (unsigned k = 0; k < n; ++k)
    if (s_->heads.count((a + k) & ~3u) ||
        (a + k >= HandlePool::NODE_BASE &&
         a + k < HandlePool::NODE_BASE + 20 * HandlePool::NODE_COUNT))
      throw Error("raw handle or node store");
  s_->w(a, n, v);
}
Address World::spawn_skelight(const Spawn &v) {
  return s_->spawn(v, false, 0, 0, 0);
}
Address World::spawn_class(const std::string &name, const Spawn &v) {
  return s_->spawn_class(name, v);
}
Address World::spawn_player(unsigned n, const Spawn &v, int str, int level) {
  return s_->spawn(v, true, n, str, level);
}
bool World::contains(Address a) const {
  return s_->objects.count(a) != 0 || s_->authority->is_factory(a);
}
std::vector<Address> World::actors() const {
  std::vector<Address> out;
  for (auto &p : s_->objects)
    out.push_back(p.first);
  return out;
}
Address World::retain(Address a) {
  if (!contains(a)) throw Error("not a registered combat or ported factory object");
  if (s_->external_next >= 0x90040000)
    throw Error("external handle budget exceeded");
  auto h = s_->external_next;
  s_->external_next += 4;
  s_->relink(h, a);
  return h;
}
void World::relink(Address h, Address a) { s_->relink(h, a); }
void World::clear_handle(Address h) { s_->clear(h); }
Address World::target(Address h) const { return s_->target(h); }
const objects::GameObject *World::factory_object(Address h) const {
  const auto address = target(h);
  if (!address) return nullptr;
  const auto found = s_->authority->objects->objects.find(address);
  return s_->authority->is_factory(address) &&
                 found != s_->authority->objects->objects.end() &&
                 found->second == s_->authority->find(address)
             ? found->second : nullptr;
}
void World::destroy(Address a) { s_->destroy(a); }
void World::delete_object(Address a) { s_->erase_later(a); }
void World::update_grid(Address a) { s_->grid(a); }
void World::run_every_frame(Address a) { s_->schedule(a, 1); }
void World::run_every(Address a, unsigned p) { s_->schedule(a, p); }
std::vector<Address> World::collect_box(const Vec3 &a, const Vec3 &b, int l) {
  return s_->box(a, b, l);
}
void World::run_objects() {
  if (s_->failed || s_->running)
    throw Error("combat pass failed or reentrant");
  s_->running = true;
  try {
    s_->run_objects();
    s_->running = false;
  } catch (...) {
    s_->failed = true;
    s_->running = false;
    throw;
  }
}
void World::tick() {
  if (s_->failed || s_->running)
    throw Error("combat tick failed or reentrant");
  s_->running = true;
  try {
    s_->tick();
    s_->running = false;
  } catch (...) {
    s_->failed = true;
    s_->running = false;
    throw;
  }
}
const std::vector<Event> &World::events() const { return s_->trace; }
void World::set_iterator_retention(IteratorRetention r) { s_->retention = r; }
void World::clear_history() {
  s_->trace.clear();
  s_->mem.clear_log();
  s_->model.writes.clear();
  s_->model.calls.clear();
  s_->model.events.clear();
  s_->movement.writes.clear();
  s_->movement.calls.clear();
  s_->movement.events.clear();
}
Raw World::slot_function(Address a, unsigned n) const {
  s_->actor(a);
  if (n > 39)
    throw Unsupported("virtual slot not documented");
  return s_->u(s_->u(a + 0x48) + 8 * n + 4);
}
void World::refuse(const std::string &f) const {
  throw Unsupported(f + " is explicitly unsupported by combat-runtime");
}
void World::animation_events(Address a, Raw mask) {
  s_->actor(a);
  for (auto &e : s_->queued_events)
    if (s_->target(e.first) == a) {
      e.second.second |= mask;
      return;
    }
  auto h = retain(a);
  s_->queued_events[h] = {h, mask};
}
Services flat_fixture_services() {
  Services s;
  s.dialog_is_open = [] { return false; };
  s.jumpgate_travel_state = [] { return Raw(0); };
  s.raycast = [](const Vec3 &, const Vec3 &, Raw, Raw) { return MINUS_ONE; };
  s.slide = [](const Vec3 &p, const Vec3 &d, Raw, Raw) {
    Slide out;
    for (unsigned k = 0; k < 3; ++k)
      out.position[k] = add(p[k], d[k]);
    return out;
  };
  s.height_below = [](const Vec3 &, Raw) { return Raw(0); };
  s.water_test = [](const Vec3 &, Raw) { return Raw(0); };
  s.water_disturb = [](const Vec3 &, Raw, Raw) { return Raw(0); };
  s.skill_stat = [](const std::string &, Raw, Raw) { return Raw(0); };
  s.camera_yaw = [](Raw) { return Raw(0); };
  s.camera_flag = [](Raw) { return Raw(0); };
  s.animation_root = [](Address) { return std::array<Raw, 2>{0, 0}; };
  s.attach_point = [](Address, Raw, const Vec3 &pos, Raw, Raw) { return pos; };
  s.sample_point_count = [](Raw) { return Raw(0); };
  s.resource = [](const ResourceQuery &) { return Raw(0); };
  // Synthetic: the placements' class definitions hold no model (+0xc is 0),
  // so every part is shown, the mask Creature_ctor itself stores.
  s.hide_mask = [](Raw, Raw) { return UINT64_MAX; };
  s.height_at_point = [](const Vec3 &) { return Raw(0); };
  s.particle_group = [] { return Raw(0); };
  return s;
}
} // namespace champions::simulation::rta::combat_runtime
