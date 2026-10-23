#include "champions/detail/checkpoint.hpp"
// Authored port of ref_objects.py at 0dd02ce416389dd440540119727d946636507703,
// SHA256 541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2.
// See that commit's docs/spec/object-system.md; no original code or data
// included.
#include "objects_internal.hpp"
#include <set>

namespace champions::simulation::rta::objects {
using namespace detail;
std::int32_t cell_coord(Raw value) {
  return asr(std::uint32_t(to_int(value)) + 0x3c00, 8);
}
std::optional<std::string> props_find(const Props &p, const std::string &key,
                                      std::int64_t nth) {
  if (!p)
    return {};
  check_props(p);
  const auto k = lower(key);
  for (const auto &e : *p)
    if (e.size() > k.size() && e.compare(0, k.size(), k) == 0 &&
        e[k.size()] == '=') {
      if (nth == 0)
        return e.substr(k.size() + 1);
      nth = step(nth, -1);
    }
  return {};
}
std::int32_t atoi(const std::string &s) {
  bounded(s.size());
  std::size_t p = 0;
  while (p < s.size() &&
         std::string(" \t\n\v\f\r").find(s[p]) != std::string::npos)
    ++p;
  bool negative = false;
  if (p < s.size() && (s[p] == '+' || s[p] == '-'))
    negative = s[p++] == '-';
  std::uint32_t v = 0;
  while (p < s.size() && s[p] >= '0' && s[p] <= '9')
    v = v * 10 + std::uint32_t(s[p++] - '0');
  return s32(negative ? 0U - v : v);
}
std::int32_t props_int(const Props &p, const std::string &key,
                       std::int64_t nth) {
  auto value = props_find(p, key, nth);
  return value ? atoi(*value) : 0;
}
std::uint32_t name_hash(const std::string &s) {
  bounded(s.size());
  std::uint32_t h = 0;
  for (unsigned char c : s) {
    if (c >= 128)
      throw ObjectError("non-ASCII object name");
    if (c >= 'A' && c <= 'Z')
      c += 32;
    h += c;
    h += h << 10;
    h ^= std::uint32_t(asr(h, 6));
  }
  h += h << 3;
  h ^= std::uint32_t(asr(h, 11));
  return h & 511;
}
std::string strip_quotes(const std::string &s) {
  bounded(s.size());
  if (s.empty() || s.front() != '"')
    return s;
  auto end = s.find('"', 1);
  return s.substr(1, end == std::string::npos ? end : end - 1);
}
ClassDef::ClassDef(std::string l, const std::vector<std::string> &names,
                   std::int64_t gen)
    : lump(std::move(l)), generation(gen) {
  bounded(names.size(), MAX_RETAINED);
  for (const auto &n : names) {
    bounded(n.size());
    entries.emplace_back(n, 0);
  }
}
GameObject::GameObject(std::uint32_t a, std::uint64_t identity)
    : addr(a), serial(identity) {
  id = a;
}
std::uint32_t GameObject::rand() {
  seed = seed * 0x19660dU + 0x3c6ef35fU;
  return seed >> 16;
}
void NameTable::clear() {
  keys.fill({});
  objs.fill(nullptr);
  count = 0;
}
std::size_t NameTable::add(const std::string &text, GameObject *obj) {
  auto name = strip_quotes(text);
  count = step(count, 1);
  auto slot = name_hash(name);
  for (unsigned i = 0; i < 512; ++i) {
    if (!keys[slot] || (!objs[slot] && lower(*keys[slot]) == lower(name))) {
      keys[slot] = name;
      objs[slot] = obj;
      return slot;
    }
    slot = (slot + 1) & 511;
  }
  throw ObjectError("name table full");
}
std::optional<std::size_t> NameTable::remove(const std::string &name,
                                             GameObject *obj) {
  count = step(count, -1);
  auto slot = name_hash(name);
  for (unsigned i = 0; i < 0x20b; ++i) {
    if (objs[slot] == obj) {
      objs[slot] = nullptr;
      return slot;
    }
    slot = (slot + 1) & 511;
  }
  return {};
}
std::vector<GameObject *> NameTable::find(const std::string &name,
                                          std::int64_t limit) const {
  if (limit < 2)
    throw ObjectError("name query limit below two");
  std::vector<GameObject *> out;
  auto slot = name_hash(name);
  for (unsigned i = 0; i < 513; ++i) {
    if (!keys[slot])
      return out;
    if (lower(*keys[slot]) == lower(name) && objs[slot]) {
      out.push_back(objs[slot]);
      if (out.size() == std::uint64_t(limit - 1))
        return out;
    }
    slot = (slot + 1) & 511;
  }
  throw ObjectError("name table has no empty key");
}
GameObject *NameTable::find_first(const std::string &name) const {
  auto found = find(name, 2);
  return found.empty() ? nullptr : found.front();
}
GameObject &World::checked_object(rta::Object *object) const {
  auto found = owned_objects_.find(object);
  if (found == owned_objects_.end())
    throw ObjectError("list refers to an object outside this World");
  return *found->second;
}
void World::register_external(GameObject &object) {
  owned_objects_[&object] = &object;
}
void World::unregister_external(GameObject &object) {
  if (!owned_objects_.count(&object)) return;
  // Remove every paired world-list node before the binder releases the
  // object's embedded list heads. Restart at the head after each removal:
  // both partners may be in this same list. An active iterator is detached
  // and invalidated instead of throwing during external-owner teardown.
  for (auto &head : object.heads)
    while (auto *node = head.next) {
      if (node->kind == LinkKind::Node) {
        handles.remove_ref(node);
      } else {
        HandlePool::unlink(node);
        node->prev = node->next = nullptr;
      }
    }
  for (std::size_t slot = 0; slot < names.objs.size(); ++slot)
    if (names.objs[slot] == &object) {
      names.objs[slot] = nullptr;
      --names.count;
    }
  if (auto it = objects.find(object.addr); it != objects.end() && it->second == &object)
    objects.erase(it);
  owned_objects_.erase(&object);
}
World::World(CallbackPolicy policy) : callback_policy(policy) {}
World::~World() {
  *lifetime_ = false;
  lifetime_.reset();
}
void World::init() {
  tasks.add(0x14b910, 10, "runObjects", 0);
  tasks.add(0x14b168, 21, "drawObjects", 1);
  for (auto &c : grid)
    c.next = nullptr;
  for (auto &c : grid_b)
    c.next = nullptr;
  off_grid.next = nullptr;
  names.clear();
}
Cell &World::home_cell(const GameObject &o, bool flag2) {
  auto col = cell_coord(o.pos[0]), row = cell_coord(o.pos[1]);
  if ((flag2 && (o.flags & 2)) || row < 0 || col < 0 || row >= 120 ||
      col >= 120)
    return off_grid;
  return (o.flags & 0x800 ? grid_b
                          : grid)[std::size_t(row) * 120 + std::size_t(col)];
}
GameObject *World::construct(std::uint32_t addr, const Vec3 &pos,
                             std::shared_ptr<ClassDef> def, Props props,
                             Props stale) {
  bounded(storage_.size() + 1, MAX_RETAINED);
  check_props(props);
  check_props(stale);
  auto fresh = std::make_unique<GameObject>(addr, storage_.size() + 1);
  auto *o = fresh.get();
  storage_.push_back(std::move(fresh));
  owned_objects_.emplace(o, o);
  o->pos = pos;
  o->class_def = def;
  if (def && def->generation != lump_generation) {
    bounded(def->entries.size(), MAX_RETAINED);
    const auto *entries_data = def->entries.data();
    std::vector<std::string> entry_names;
    for (const auto &entry : def->entries)
      entry_names.push_back(entry.first);
    for (std::size_t i = 0; i < def->entries.size(); ++i) {
      auto fn = res_find;
      if (!fn && callback_policy == CallbackPolicy::RequireHandlers)
        throw ObjectError("missing res_find handler");
      const auto name = def->entries[i].first;
      const auto lump = def->lump;
      const auto value = fn ? fn(lump, name) : 0;
      if (def->entries.data() != entries_data ||
          def->entries.size() != entry_names.size())
        throw ObjectError("resource callback changed class entry structure");
      for (std::size_t n = 0; n < entry_names.size(); ++n)
        if (def->entries[n].first != entry_names[n])
          throw ObjectError("resource callback changed class entry identity");
      def->entries[i].second = value;
    }
    def->generation = lump_generation;
  }
  handles.link(home_cell(*o, true), o->heads[2], o);
  if (props_int(stale, "hide"))
    move_off_grid(*o);
  o->props = std::move(props);
  o->seed = shared.draw();
  if (o->props)
    for (auto &e : *o->props)
      if (e.compare(0, 5, "name=") == 0) {
        auto name = strip_quotes(e.substr(5));
        e = "name=" + name;
        names.add(name, o);
      }
  objects[addr] = o;
  return o;
}
GameObject *World::new_object(std::uint32_t size, const Vec3 &p,
                              std::shared_ptr<ClassDef> d, Props props) {
  bounded(storage_.size() + 1, MAX_RETAINED);
  auto addr = pool.allocate(size);
  return addr ? construct(addr, p, std::move(d), std::move(props)) : nullptr;
}
void World::destroy(GameObject &o, std::int64_t flags) {
  o.vtable = VT_GAMEOBJECT;
  if (o.props)
    for (const auto &e : *o.props)
      if (e.compare(0, 5, "name=") == 0)
        names.remove(strip_quotes(e.substr(5)), &o);
  handles.clear(o.heads[2]);
  handles.clear(o.heads[1]);
  handles.clear(o.heads[0]);
  if (flags & 1) {
    pool.free(o.addr);
    o.alive = false;
    objects.erase(o.addr);
  }
}
void World::delete_object(GameObject &o) {
  if (o.heads[1].next)
    handles.remove(o.heads[1].next);
  handles.link(delete_list, o.heads[0], &o);
}
void World::walk_list(Cell &head, std::uint32_t mask,
                      const std::function<void(GameObject &)> &fn) {
  if (iterators_.size() >= MAX_RETAINED)
    prune_iterators();
  bounded(iterators_.size() + 1, MAX_RETAINED);
  if (iterator_serial_ >= UINT64_C(0x7fffffffffffffff))
    throw ResourceLimit("iterator identity exhausted");
  auto id = ++iterator_serial_;
  auto it = std::make_unique<Iterator>(head, mask);
  auto *ptr = it.get();
  iterators_.emplace(id, std::move(it));
  if (ptr->done()) {
    // No node was linked and no callback observed this sentinel.
    ptr->end();
    iterators_.erase(id);
    return;
  }
  std::size_t work = 0;
  while (!ptr->done()) {
    budget(work);
    auto *o = ptr->current();
    if (o)
      fn(checked_object(o));
    ptr->advance();
  }
  ptr->end();
  completed_iterators_.insert(id);
  prune_iterators();
}
void World::collect_link_references(const std::set<const Link *> &candidates,
                                    std::set<const Link *> &referred) const {
  auto add_refs = [&](const Link &n) {
    for (const auto *ref : {n.next, n.prev, n.partner})
      if (ref && candidates.count(ref))
        referred.insert(ref);
  };
  for (auto *n : handles.nodes())
    add_refs(*n);
  for (const auto &e : storage_)
    for (const auto &c : e->heads)
      add_refs(c);
  for (const auto &e : objects)
    for (const auto &c : e.second->heads)
      add_refs(c);
  for (const auto &e : extra_cells_)
    add_refs(*e.second);
  for (const auto &c : grid)
    add_refs(c);
  for (const auto &c : grid_b)
    add_refs(c);
  for (const auto &c : run15)
    add_refs(c);
  for (const auto &c : run60)
    add_refs(c);
  add_refs(off_grid);
  add_refs(always_draw);
  add_refs(delete_list);
  add_refs(run_active);
}
void World::prune_iterators() {
  std::set<const Link *> referred, candidates;
  for (const auto &entry : iterators_)
    candidates.insert(entry.second.get());
  collect_link_references(candidates, referred);
  for (const auto &e : iterators_)
    for (const auto *ref : {e.second->next, e.second->prev, e.second->partner})
      if (ref && candidates.count(ref))
        referred.insert(ref);
  for (auto i = completed_iterators_.begin();
       i != completed_iterators_.end();) {
    auto found = iterators_.find(*i);
    if (found != iterators_.end() && !referred.count(found->second.get())) {
      iterators_.erase(found);
      i = completed_iterators_.erase(i);
    } else
      ++i;
  }
}
void World::call_run(GameObject &o) {
  auto fn = on_run;
  if (fn)
    fn(o);
  else if (callback_policy == CallbackPolicy::RequireHandlers)
    throw ObjectError("missing object run handler");
}
void World::call_destroy(GameObject &o, std::int64_t flags) {
  auto fn = on_destroy;
  if (fn)
    fn(o, flags);
  else if (callback_policy == CallbackPolicy::ReferenceBase)
    destroy(o, flags);
  else
    throw ObjectError("missing object destructor handler");
}
bool World::frozen() {
  auto fn = is_frozen;
  if (fn)
    return fn();
  if (callback_policy == CallbackPolicy::ReferenceBase)
    return false;
  throw ObjectError("missing frozen-state handler");
}
void World::finish_tick() {
  auto fn = end_of_tick;
  if (fn)
    fn();
  else if (callback_policy == CallbackPolicy::RequireHandlers)
    throw ObjectError("missing end-of-tick handler");
}
void World::flush_delete_list() {
  walk_list(delete_list, exclude_mask,
            [this](GameObject &o) { call_destroy(o, 3); });
}
Link *World::grid_node(GameObject &o) {
  auto *back = o.heads[2].next;
  if (!back || !back->partner)
    throw ObjectError("object has no grid link");
  return back->partner;
}
void World::link_to_grid(GameObject &o) {
  handles.link(home_cell(o, true), o.heads[2], &o);
}
void World::update_grid_cell(GameObject &o) {
  auto *n = grid_node(o);
  if (!n->prev)
    throw ObjectError("unlinked grid node");
  HandlePool::unlink(n);
  HandlePool::push(home_cell(o, false), n);
}
void World::move_off_grid(GameObject &o) {
  auto *n = grid_node(o);
  if (!n->prev)
    throw ObjectError("unlinked grid node");
  HandlePool::unlink(n);
  HandlePool::push(off_grid, n);
}
bool World::is_off_grid(const GameObject &o) {
  bool found = false;
  walk_list(off_grid, 0, [&](GameObject &other) {
    if (&other == &o)
      found = true;
  });
  return found;
}
void World::add_to_always_draw(GameObject &o) {
  handles.link(always_draw, o.heads[0], &o);
  o.flags |= 2;
}
void World::add_to_always_draw_once(GameObject &o) {
  bool found = false;
  walk_list(always_draw, exclude_mask, [&](GameObject &other) {
    if (&other == &o)
      found = true;
  });
  if (!found)
    add_to_always_draw(o);
}
void World::clear_always_draw() {
  walk_list(always_draw, exclude_mask, [](GameObject &o) { o.flags &= ~2U; });
  handles.clear(always_draw);
}
void World::leave_run_list(GameObject &o) {
  if (o.heads[1].next)
    handles.remove(o.heads[1].next);
}
void World::stop_running(GameObject &o) { leave_run_list(o); }
void World::run_every_frame(GameObject &o) {
  leave_run_list(o);
  handles.link(run_active, o.heads[1], &o);
}
void World::run_every_60(GameObject &o) {
  leave_run_list(o);
  cursor60 = step(cursor60, 1);
  if (cursor60 >= 60)
    cursor60 = 0;
  if (cursor60 < 0)
    throw ObjectError("negative run cursor");
  handles.link(run60[std::size_t(cursor60)], o.heads[1], &o);
}
void World::run_every_15(GameObject &o) {
  leave_run_list(o);
  cursor15 = step(cursor15, 1);
  if (cursor15 >= 15)
    cursor15 = 0;
  if (cursor15 < 0)
    throw ObjectError("negative run cursor");
  handles.link(run15[std::size_t(cursor15)], o.heads[1], &o);
}
} // namespace champions::simulation::rta::objects
namespace champions::simulation::rta::objects {
Cell &World::make_cell(std::uint32_t id) {
  if (id < 0x90000000)
    throw ObjectError("extra cell id overlaps reserved virtual ranges");
  if (extra_cells_.count(id))
    throw ObjectError("duplicate extra cell id");
  detail::bounded(extra_cells_.size() + 1, MAX_RETAINED);
  auto cell = std::make_unique<Cell>();
  auto &out = *cell;
  extra_cells_.emplace(id, std::move(cell));
  return out;
}
Cell &World::retain_handle_cell() {
  for (std::uint32_t i = 0; i < MAX_RETAINED; ++i) {
    const auto id = UINT32_C(0x90000000) + 4 * i;
    if (!extra_cells_.count(id))
      return make_cell(id);
  }
  throw ResourceLimit("retained handle-cell capacity exceeded");
}
std::vector<HandleCellInfo> World::handle_cells() const {
  std::vector<HandleCellInfo> out;
  auto add = [&](const Cell &c, std::uint32_t address,
                 std::uint64_t identity = 0, std::uint64_t serial = 0) {
    out.push_back({&c, identity ? identity : address, address, serial});
  };
  add(run_active, G_RUN_ACTIVE);
  add(off_grid, G_OFF_GRID);
  add(always_draw, G_ALWAYS_DRAW);
  add(delete_list, G_DELETE_LIST);
  for (std::size_t i = 0; i < grid.size(); ++i) {
    add(grid[i], G_GRID + std::uint32_t(4 * i));
    add(grid_b[i], G_GRID_B + std::uint32_t(4 * i));
  }
  for (std::size_t i = 0; i < run15.size(); ++i)
    add(run15[i], G_RUN_15 + std::uint32_t(4 * i));
  for (std::size_t i = 0; i < run60.size(); ++i)
    add(run60[i], G_RUN_60 + std::uint32_t(4 * i));
  for (const auto &e : extra_cells_)
    add(*e.second, e.first);
  for (const auto &e : storage_) {
    if (!e->addr || std::uint64_t(e->addr) + 12 > UINT64_C(0x100000000))
      throw ObjectError(
          "object head span cannot be represented in raw32 memory");
    for (std::uint32_t i = 0; i < 3; ++i)
      add(e->heads[i], e->addr + 4 * i,
          (e->serial << 32) | (std::uint64_t(e->addr) + 4 * i), e->serial);
  }
  return out;
}
std::optional<std::pair<std::uint64_t, std::uint32_t>>
World::handle_object_identity(const rta::Object *object) const {
  const auto found = owned_objects_.find(object);
  if (found == owned_objects_.end())
    return {};
  return std::make_pair(found->second->serial, found->second->addr);
}
std::vector<std::pair<std::uint64_t, std::uint32_t>>
World::live_retained_identities() const {
  std::vector<std::pair<std::uint64_t, std::uint32_t>> out;
  for (const auto &entry : storage_)
    if (entry->alive)
      out.emplace_back(entry->serial, entry->addr);
  return out;
}
void World::validate_snapshot_references() const {
  std::set<const Link *> links;
  auto add = [&](const Cell &cell) { links.insert(&cell); };
  add(run_active);
  add(off_grid);
  add(always_draw);
  add(delete_list);
  for (const auto &cell : grid)
    add(cell);
  for (const auto &cell : grid_b)
    add(cell);
  for (const auto &cell : run15)
    add(cell);
  for (const auto &cell : run60)
    add(cell);
  for (const auto &entry : extra_cells_)
    add(*entry.second);
  for (const auto *node : handles.nodes())
    links.insert(node);
  for (const auto &entry : storage_)
    for (const auto &head : entry->heads)
      add(head);
  std::set<const Link *> iterators;
  for (const auto &entry : iterators_) {
    iterators.insert(entry.second.get());
    if (!completed_iterators_.count(entry.first))
      links.insert(entry.second.get());
  }
  std::vector<const Link *> pending(links.begin(), links.end());
  for (std::size_t i = 0; i < pending.size(); ++i)
    for (const auto *p :
         {pending[i]->next, pending[i]->prev, pending[i]->partner})
      if (iterators.count(p) && links.insert(p).second)
        pending.push_back(p);
  if (handles.free && !links.count(handles.free))
    throw ObjectError("snapshot refers to a foreign link");
  auto object = [&](const rta::Object *p) {
    if (p && !owned_objects_.count(p))
      throw ObjectError("snapshot refers to a foreign object");
  };
  for (const auto *link : links) {
    for (const auto *p : {link->next, link->prev, link->partner})
      if (p && !links.count(p))
        throw ObjectError("snapshot refers to a foreign link");
    object(link->object_ref().object);
  }
  for (const auto &entry : objects)
    object(entry.second);
  for (const auto *entry : names.objs)
    object(entry);
}
WorldSnapshot World::snapshot() const {
  WorldSnapshot out;
  out.pool = pool;
  out.shared_seed = shared.state;
  out.iterator_serial = iterator_serial_;
  std::map<const Link *, std::uint64_t> ids;
  std::map<const rta::Object *, std::uint64_t> object_ids;
  auto cell = [&](const Cell &c, std::uint64_t id) { ids.emplace(&c, id); };
  cell(run_active, G_RUN_ACTIVE);
  cell(off_grid, G_OFF_GRID);
  cell(always_draw, G_ALWAYS_DRAW);
  cell(delete_list, G_DELETE_LIST);
  for (std::size_t i = 0; i < grid.size(); ++i) {
    cell(grid[i], G_GRID + 4 * i);
    cell(grid_b[i], G_GRID_B + 4 * i);
  }
  for (std::size_t i = 0; i < 15; ++i)
    cell(run15[i], G_RUN_15 + 4 * i);
  for (std::size_t i = 0; i < 60; ++i)
    cell(run60[i], G_RUN_60 + 4 * i);
  for (const auto &e : extra_cells_)
    cell(*e.second, e.first);
  for (const auto &n : handles.nodes())
    ids.emplace(n, HandlePool::NODE_BASE + HandlePool::NODE_SIZE * n->index);
  for (const auto &e : storage_) {
    object_ids.emplace(e.get(), e->serial);
    for (std::size_t i = 0; i < 3; ++i)
      cell(e->heads[i], (e->serial << 32) | (std::uint64_t(e->addr) + i * 4));
  }
  std::map<const Link *, std::uint64_t> iterator_ids;
  for (const auto &e : iterators_) {
    const auto id = UINT64_C(0x8000000000000000) | e.first;
    iterator_ids.emplace(e.second.get(), id);
    if (!completed_iterators_.count(e.first))
      ids.emplace(e.second.get(), id);
  }
  std::vector<const Link *> pending;
  for (const auto &e : ids)
    pending.push_back(e.first);
  for (std::size_t i = 0; i < pending.size(); ++i) {
    const auto *link = pending[i];
    for (const auto *next : {link->next, link->prev, link->partner}) {
      auto found = iterator_ids.find(next);
      if (found != iterator_ids.end() &&
          ids.emplace(found->first, found->second).second)
        pending.push_back(found->first);
    }
  }
  auto ref = [&](const Link *p) -> std::uint64_t {
    if (!p)
      return 0;
    auto i = ids.find(p);
    if (i == ids.end())
      throw ObjectError("snapshot refers to a foreign link");
    return i->second;
  };
  auto objref = [&](const rta::Object *p) -> std::uint64_t {
    if (!p)
      return 0;
    auto i = object_ids.find(p);
    if (i == object_ids.end())
      throw ObjectError("snapshot refers to a foreign object");
    return i->second;
  };
  out.handle_free = ref(handles.free);
  for (const auto &e : ids) {
    const auto &p = *e.first;
    auto tag = p.kind == LinkKind::Iterator
                   ? static_cast<const Iterator &>(p).mask
                   : p.tag;
    const auto object = p.object_ref();
    out.links.push_back({e.second, ref(p.next), ref(p.prev), ref(p.partner),
                         objref(object.object), p.kind, tag, object.raw});
  }
  std::sort(out.links.begin(), out.links.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  std::map<const void *, std::uint64_t> props_ids, defs_ids;
  for (const auto &p : storage_) {
    const auto &o = *p;
    ObjectSnapshot v;
    v.serial = o.serial;
    v.addr = o.addr;
    v.flags = o.flags;
    v.pos = o.pos;
    v.radius = o.radius;
    v.height = o.height;
    v.vz = o.vz;
    v.angle = o.angle;
    v.seed = o.seed;
    v.vtable = o.vtable;
    v.draw_half_width = o.draw_half_width;
    v.draw_height = o.draw_height;
    v.class_name = o.class_name;
    v.type_name = o.type_name;
    v.alive = o.alive;
    v.tag = o.tag;
    if (o.props) {
      v.props_id =
          props_ids.emplace(o.props.get(), props_ids.size() + 1).first->second;
      v.props = *o.props;
    }
    if (o.class_def) {
      v.class_def_id = defs_ids.emplace(o.class_def.get(), defs_ids.size() + 1)
                           .first->second;
      v.class_def = *o.class_def;
    }
    out.objects.push_back(std::move(v));
  }
  for (const auto &e : objects)
    out.live.emplace(e.first, objref(e.second));
  out.name_keys = names.keys;
  for (std::size_t i = 0; i < 512; ++i)
    out.name_objects[i] = objref(names.objs[i]);
  out.name_count = names.count;
  out.tasks = tasks;
  out.cursor15 = cursor15;
  out.cursor60 = cursor60;
  out.frame = frame;
  out.network_mode = network_mode;
  out.cull_by_camera = cull_by_camera;
  out.pool_free_count = pool_free_count;
  out.tearing_down = tearing_down;
  out.lump_generation = lump_generation;
  out.menu_lockout = menu_lockout;
  out.menu_lockout_hold = menu_lockout_hold;
  out.warmup_frames = warmup_frames;
  out.start_rank = start_rank;
  out.exclude_mask = exclude_mask;
  out.camera_focus = camera_focus;
  out.centres = centres;
  out.pads = pads;
  out.spawn_pos = spawn_pos;
  out.spawn_facing = spawn_facing;
  out.callback_policy = callback_policy;
  out.callbacks = {bool(on_run), bool(on_destroy), bool(is_frozen),
                   bool(res_find), bool(end_of_tick)};
  return out;
}
void World::checkpoint(rta::detail::Checkpoint &a) {
  using A = rta::detail::Checkpoint;
  A::require(storage_.size() == 1 && objects.size() == 1,
             "checkpoint requires closed singleton World");
  auto &o = *storage_[0];
  a.fields(o.flags, o.pos, o.radius, o.height, o.vz, o.angle, o.seed);
  a.fields(shared.state, cursor15, cursor60, frame, menu_lockout,
           menu_lockout_hold, warmup_frames, start_rank, tasks.cursor,
           iterator_serial_, pool_free_count);
  for (auto &p : pads)
    a.fields(p.held, p.pressed, p.edge2);
  A::require(frame >= 0 &&
                 (a.play_continuation
                      ? (frame <= 0x7ffffffd &&
                         iterator_serial_ <= UINT64_C(0x7fffffffffffffff))
                      : (frame <= 2000 && iterator_serial_ <= MAX_WORK)),
             "checkpoint World counters");
  auto view = snapshot();
  std::map<std::uint64_t, Link *> refs;
  for (const auto &c : handle_cells())
    refs.emplace(c.identity, const_cast<Cell *>(c.cell));
  for (auto *n : handles.nodes())
    refs.emplace(HandlePool::NODE_BASE + HandlePool::NODE_SIZE * n->index, n);
  // Finished iterators retained by stale node references are value state too.
  // Their constructors touch a fresh empty cell only; the checked graph below
  // subsequently replaces every link without invoking gameplay callbacks.
  if (a.reading) {
    iterators_.clear();
    completed_iterators_.clear();
  } else
    for (const auto &e : iterators_)
      refs.emplace(UINT64_C(0x8000000000000000) | e.first, e.second.get());
  auto count = a.count(view.links.size());
  std::vector<LinkSnapshot> links(count);
  if (!a.reading)
    links = std::move(view.links);
  std::uint64_t previous = 0;
  for (auto &v : links) {
    a.fields(v.id, v.next, v.prev, v.partner, v.object, v.kind, v.tag,
             v.object_raw);
    A::require(v.id > previous && !v.object_raw &&
                   (v.object == 0 || v.object == o.serial),
               "checkpoint link identity");
    previous = v.id;
    if (a.reading && v.kind == LinkKind::Iterator) {
      auto id = v.id & UINT64_C(0x7fffffffffffffff);
      A::require((v.id >> 63) && id && id <= iterator_serial_ && !v.prev,
                 "checkpoint idle iterator");
      auto it = std::make_unique<Iterator>(off_grid, v.tag);
      refs.emplace(v.id, it.get());
      iterators_.emplace(id, std::move(it));
      completed_iterators_.insert(id);
    }
    auto found = refs.find(v.id);
    A::require(found != refs.end() && found->second->kind == v.kind,
               "checkpoint unknown link");
  }
  A::require(count == refs.size(), "checkpoint missing graph links");
  auto resolve = [&](std::uint64_t id) -> Link * {
    if (!id)
      return nullptr;
    auto i = refs.find(id);
    A::require(i != refs.end(), "checkpoint dangling link");
    return i->second;
  };
  auto free = view.handle_free;
  a.value(free);
  if (a.reading) {
    for (const auto &v : links) {
      auto *p = refs.at(v.id);
      p->next = resolve(v.next);
      p->prev = resolve(v.prev);
      p->partner = resolve(v.partner);
      p->obj = v.object ? &o : nullptr;
      p->object_raw = 0;
      p->tag = v.kind == LinkKind::Iterator ? 0 : v.tag;
    }
    handles.free = resolve(free);
    // Next chains are acyclic and contain no list heads. Completed iterators
    // may retain stale prev/partner fields; do not invent reciprocal rules.
    std::map<Link *, unsigned> colour;
    for (const auto &entry : refs) {
      std::vector<Link *> chain;
      auto *p = entry.second;
      while (p && colour[p] == 0) {
        colour[p] = 1;
        chain.push_back(p);
        A::require(!p->next || p->next->kind != LinkKind::Cell,
                   "checkpoint next points to head");
        p = p->next;
      }
      A::require(!p || colour[p] == 2, "checkpoint next-chain cycle");
      for (auto *link : chain)
        colour[link] = 2;
    }

    // All links point to known owned storage. Validate free-list termination
    // before any simulation can traverse it.
    std::set<Link *> seen;
    for (auto *p = handles.free; p; p = p->next)
      A::require(p->kind == LinkKind::Node && seen.insert(p).second,
                 "checkpoint free-list cycle");
  }
}
} // namespace champions::simulation::rta::objects
