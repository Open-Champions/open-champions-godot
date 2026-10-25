#include "champions/raw_handles.hpp"
#include <algorithm>
#include <map>

// Authored composition of the pinned core handle and object lifetime models.
// ref_core.py at 41e847af5360724ab62d208594f8027ea3c1b686 and ref_objects.py
// at 0dd02ce416389dd440540119727d946636507703 in champions-return-to-arms-re.
// Physical-address registration is host policy. Core operation order is reused.
namespace champions::simulation::rta::raw_handles {
namespace {
constexpr std::uint32_t free_address = 0x864ea8;
constexpr std::uint64_t end_address = UINT64_C(0x100000000);
constexpr std::size_t max_heads = 65536;
bool overlap(std::uint32_t a, std::uint32_t n, std::uint32_t b,
             std::uint32_t m) {
  return std::uint64_t(a) < std::uint64_t(b) + m &&
         std::uint64_t(b) < std::uint64_t(a) + n;
}
void label_check(const std::string &s) {
  if (s.empty() || s.size() > 128)
    throw Error("head label requires 1..128 bytes");
}
} // namespace
struct RawHandles::State final : creature_hp::ReadOverlay {
  struct Head {
    HeadBinding binding;
    Cell *cell;
    objects::GameObject *object;
  };
  objects::World *world;
  creature_hp::Memory *memory;
  std::weak_ptr<const bool> world_lifetime;
  Limits limits;
  bool active = true, attached = false;
  std::vector<Head> heads;
  std::map<std::uint32_t, std::size_t> by_address;
  std::map<const Link *, std::uint32_t> physical;
  std::map<const Link *, objects::HandleCellInfo> cells;
  std::map<const Link *, std::uint32_t> nodes;
  State(objects::World &w, creature_hp::Memory &m, Limits l)
      : world(&w), memory(&m), world_lifetime(w.handle_lifetime()), limits(l) {
    if (l.heads > max_heads)
      throw Error("head limit exceeds supported capacity");
    if (w.handles.nodes().size() != HandlePool::NODE_COUNT)
      throw Error(
          "raw bridge requires the complete existing World handle pool");
    for (std::size_t i = 0; i < w.handles.nodes().size(); ++i) {
      const auto *n = w.handles.nodes()[i];
      if (!n || n->index != i ||
          !nodes
               .emplace(n, HandlePool::NODE_BASE +
                               std::uint32_t(i * HandlePool::NODE_SIZE))
               .second)
        throw Error(
            "pool node identity/index does not match its owned position");
    }
    refresh_cells();
  }
  void ensure(bool require_active = true) const {
    const auto lifetime = world_lifetime.lock();
    if ((require_active && !active) || !lifetime || !*lifetime)
      throw Error("raw handle view is inactive or its World expired");
    for (const auto &h : heads)
      if (!h.binding.active && h.cell->next)
        throw Error("retired physical-head alias was mutated");
  }
  void refresh_cells() {
    ensure();
    for (const auto &c : world->handle_cells())
      cells[c.cell] = c;
  }
  std::uint32_t encode(const Link *p) const {
    if (!p)
      return 0;
    if (const auto n = nodes.find(p); n != nodes.end())
      return n->second;
    if (const auto e = physical.find(p); e != physical.end())
      return e->second;
    if (const auto e = cells.find(p); e != cells.end())
      return e->second.address;
    throw Error("unregistered link or iterator has no supported raw address");
  }
  NodeSnapshot node(const Node &n) const {
    const auto found = nodes.find(&n);
    if (found == nodes.end() ||
        n.index !=
            (found->second - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE)
      throw Error("pool node index was mutated");
    if (n.tag > UINT16_MAX)
      throw Error("handle tag does not fit its raw16 field");
    const auto ref = n.object_ref();
    std::uint32_t object = ref.raw;
    std::uint64_t serial = 0;
    if (ref.object) {
      const auto id = world->handle_object_identity(ref.object);
      if (!id)
        throw Error("foreign typed handle object has no World identity");
      serial = id->first;
      object = id->second;
    }
    return {nodes.at(&n),   encode(n.next),
            encode(n.prev), encode(n.partner),
            object,         static_cast<std::uint16_t>(n.tag),
            serial,         ref.is_opaque()};
  }
  std::vector<bool> chain(const Link *start, bool reciprocal = false) const {
    std::vector<bool> seen(HandlePool::NODE_COUNT, false);
    for (auto *p = start; p;) {
      const auto found = nodes.find(p);
      if (found == nodes.end())
        throw Error("chain contains an unregistered node or iterator");
      const auto index =
          (found->second - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE;
      if (seen[index])
        throw Error("cyclic handle chain is unsupported");
      seen[index] = true;
      if (reciprocal) {
        if (!p->prev || encode(p->prev) == 0 || p->prev->next != p)
          throw Error("handle predecessor relation is inconsistent");
        if (p->next && (encode(p->next) == 0 || p->next->prev != p))
          throw Error("handle successor relation is inconsistent");
      }
      p = p->next;
    }
    return seen;
  }
  void validate(bool require_active = true) const {
    ensure(require_active);
    (void)encode(world->handles.free);
    for (const auto *n : world->handles.nodes())
      (void)node(*n);
    (void)chain(world->handles.free);
    for (const auto &h : heads)
      if (h.binding.active) {
        (void)encode(h.cell->next);
        (void)chain(h.cell->next);
      }
  }
  bool covers(std::uint32_t a) const override {
    if (a >= HandlePool::NODE_BASE && a < free_address)
      return (a - HandlePool::NODE_BASE) % HandlePool::NODE_SIZE < 18;
    if (a >= free_address && a < free_address + 4)
      return true;
    auto it = by_address.upper_bound(a);
    if (it == by_address.begin())
      return false;
    --it;
    return std::uint64_t(a) < std::uint64_t(it->first) + 4;
  }
  std::uint8_t read_byte(std::uint32_t a) const override {
    ensure();
    std::uint32_t word = 0, shift = 0;
    if (a >= HandlePool::NODE_BASE && a < free_address) {
      const auto index = (a - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE;
      const auto offset = (a - HandlePool::NODE_BASE) % HandlePool::NODE_SIZE;
      if (offset >= 18)
        throw Error("padding is not a handle overlay field");
      const auto value = node(*world->handles.nodes()[index]);
      if (offset < 4)
        word = value.next;
      else if (offset < 8)
        word = value.prev;
      else if (offset < 12)
        word = value.partner;
      else if (offset < 16)
        word = value.object;
      else
        word = value.tag;
      shift = (offset % 4) * 8;
    } else if (a >= free_address && a < free_address + 4) {
      word = encode(world->handles.free);
      shift = (a - free_address) * 8;
    } else {
      auto it = by_address.upper_bound(a);
      if (it == by_address.begin())
        throw Error("address is not a registered handle field");
      --it;
      if (std::uint64_t(a) >= std::uint64_t(it->first) + 4)
        throw Error("address is not a registered handle field");
      word = encode(heads[it->second].cell->next);
      shift = (a - it->first) * 8;
    }
    return static_cast<std::uint8_t>(word >> shift);
  }
  void check_address(std::uint32_t a, const Cell *cell,
                     bool replacing = false) const {
    if (!a || std::uint64_t(a) + 4 > end_address)
      throw Error("head must have a nonzero bounded raw32 address");
    if (overlap(a, 4, HandlePool::NODE_BASE,
                HandlePool::NODE_COUNT * HandlePool::NODE_SIZE + 4))
      throw Error("head overlaps the handle pool or free word");
    if (overlap(a, 4, creature_hp::G_SIM_FRAME, 4))
      throw Error("head overlaps the simulation frame word");
    for (const auto &e : by_address) {
      if (replacing && e.first == a)
        continue;
      if (overlap(a, 4, e.first, 4) &&
          (e.first != a || heads[e.second].cell != cell))
        throw Error("physical head overlaps a different registered Cell");
    }
    for (const auto &e : cells) {
      if (overlap(a, 4, e.second.address, 4) && e.first != cell) {
        // Retained generations may share exactly the same physical address.
        if (replacing && e.second.address == a && e.second.object_serial)
          continue;
        if (e.second.object_serial && e.second.address == a &&
            physical.count(e.first))
          continue;
        throw Error("physical head aliases an implicit World Cell");
      }
    }
  }
  void bind(std::uint32_t a, Cell &cell, const std::string &label,
            objects::GameObject *object = nullptr) {
    ensure();
    label_check(label);
    const auto info = cells.find(&cell);
    if (info == cells.end())
      throw Error("handle Cell is not retained by this World");
    check_address(a, &cell);
    if (const auto existing = by_address.find(a);
        existing != by_address.end()) {
      const auto &h = heads[existing->second];
      if (h.cell == &cell && h.binding.label == label)
        return;
      throw Error("head already registered with different metadata");
    }
    if (physical.count(&cell))
      throw Error("one Cell cannot have multiple physical addresses");
    if (heads.size() >= limits.heads)
      throw Error("registered head capacity exceeded");
    const auto i = heads.size();
    heads.push_back(
        {{a, info->second.identity, info->second.object_serial, label, true},
         &cell,
         object});
    by_address[a] = i;
    physical[&cell] = a;
  }
  Cell &head(std::uint32_t a) {
    ensure();
    const auto it = by_address.find(a);
    if (it == by_address.end())
      throw Error("raw head is not explicitly registered");
    return *heads[it->second].cell;
  }
};
RawHandles::RawHandles(objects::World &w, creature_hp::Memory &m, Limits limits)
    : state_(std::make_shared<State>(w, m, limits)) {
  state_->validate();
  m.attach_overlay(state_);
  state_->attached = true;
}
RawHandles::~RawHandles() { state_->active = false; }
HandlePool &RawHandles::pool() {
  state_->ensure();
  return state_->world->handles;
}
void RawHandles::bind_head(std::uint32_t a, Cell &cell,
                           const std::string &label) {
  state_->refresh_cells();
  state_->bind(a, cell, label);
}
Cell &RawHandles::intern_raw_head(std::uint32_t a, const std::string &label) {
  auto &s = *state_;
  s.refresh_cells();
  label_check(label);
  s.check_address(a, nullptr);
  if (s.heads.size() >= s.limits.heads)
    throw Error("registered head capacity exceeded");
  auto &cell = s.world->retain_handle_cell();
  s.refresh_cells();
  s.bind(a, cell, label);
  return cell;
}
void RawHandles::bind_object(objects::GameObject &o) {
  auto &s = *state_;
  s.refresh_cells();
  const auto id = s.world->handle_object_identity(&o);
  if (!id || !o.alive || !s.world->objects.count(o.addr) ||
      s.world->objects.at(o.addr) != &o)
    throw Error("bind_object requires a current owned World object");
  std::size_t count = 0;
  for (std::uint32_t i = 0; i < 3; ++i) {
    const auto a = o.addr + 4 * i;
    s.check_address(a, &o.heads[i]);
    const auto existing = s.by_address.find(a);
    if (existing == s.by_address.end()) {
      if (s.physical.count(&o.heads[i]))
        throw Error("one Cell cannot have multiple physical addresses");
      ++count;
    } else {
      const auto &h = s.heads[existing->second];
      if (h.cell != &o.heads[i] ||
          h.binding.label != "object+" + std::to_string(4 * i))
        throw Error("object head already has different binding metadata");
    }
  }
  if (count > s.limits.heads - s.heads.size())
    throw Error("registered head capacity exceeded");
  for (std::uint32_t i = 0; i < 3; ++i)
    s.bind(o.addr + 4 * i, o.heads[i], "object+" + std::to_string(4 * i), &o);
}
void RawHandles::rebind_object(objects::GameObject &o) {
  auto &s = *state_;
  s.refresh_cells();
  if (!s.world->handle_object_identity(&o) || !o.alive ||
      !s.world->objects.count(o.addr) || s.world->objects.at(o.addr) != &o)
    throw Error("rebind requires a current owned World object");
  if (s.limits.heads - s.heads.size() < 3)
    throw Error("registered head capacity exceeded");
  for (std::uint32_t i = 0; i < 3; ++i) {
    const auto a = o.addr + 4 * i;
    const auto it = s.by_address.find(a);
    if (it == s.by_address.end())
      throw Error("rebind requires three prior generation heads");
    const auto &old = s.heads[it->second];
    if (!old.object || old.object == &o || old.object->alive || old.cell->next)
      throw Error("prior generation head is not dead and empty");
    s.check_address(a, &o.heads[i], true);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    const auto a = o.addr + 4 * i;
    s.heads[s.by_address.at(a)].binding.active = false;
    const auto info = s.cells.at(&o.heads[i]);
    s.by_address[a] = s.heads.size();
    s.physical[&o.heads[i]] = a;
    s.heads.push_back(
        {{a, info.identity, o.serial, "object+" + std::to_string(4 * i), true},
         &o.heads[i],
         &o});
  }
}
void RawHandles::clear(std::uint32_t a) {
  auto &s = *state_;
  s.validate();
  auto &h = s.head(a);
  (void)s.chain(h.next, true);
  const auto free = s.chain(s.world->handles.free);
  for (auto *n = h.next; n; n = n->next) {
    const auto index =
        (s.nodes.at(n) - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE;
    if (free[index])
      throw Error("cannot clear a node also present on the free list");
    if (!n->partner)
      continue;
    const auto found = s.nodes.find(n->partner);
    if (found == s.nodes.end() || n->partner == n || n->partner->partner != n ||
        free[(found->second - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE])
      throw Error("handle partner relation is inconsistent");
    (void)s.chain(n->partner, true);
  }
  s.world->handles.clear(h);
}
std::uint32_t RawHandles::link(std::uint32_t a, std::uint32_t b,
                               std::uint32_t object) {
  auto &s = *state_;
  s.validate();
  auto &head = s.head(a);
  auto &back = s.head(b);
  const auto *n =
      s.world->handles.link_ref(head, back, HandleObjectRef::opaque(object));
  return s.nodes.at(n);
}
std::uint32_t RawHandles::link_known(std::uint32_t a, std::uint32_t b,
                                     objects::GameObject &object) {
  auto &s = *state_;
  s.validate();
  if (!s.world->handle_object_identity(&object))
    throw Error("known object is not owned by injected World");
  auto &head = s.head(a);
  auto &back = s.head(b);
  const auto *n = s.world->handles.link(head, back, &object);
  return s.nodes.at(n);
}
HandleObjectRef RawHandles::remove(std::uint32_t a) {
  auto &s = *state_;
  s.validate();
  if (a < HandlePool::NODE_BASE || a >= free_address ||
      (a - HandlePool::NODE_BASE) % HandlePool::NODE_SIZE)
    throw Error("remove requires an exact pool node address");
  const auto index = (a - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE;
  const auto free = s.chain(s.world->handles.free);
  auto *n = s.world->handles.nodes()[index];
  if (free[index])
    throw Error("cannot remove a free node");
  (void)s.chain(n, true);
  if (n->partner) {
    const auto found = s.nodes.find(n->partner);
    if (found == s.nodes.end() || n->partner == n || n->partner->partner != n ||
        free[(found->second - HandlePool::NODE_BASE) / HandlePool::NODE_SIZE])
      throw Error("handle partner relation is inconsistent");
    (void)s.chain(n->partner, true);
  }
  return s.world->handles.remove_ref(n);
}
void RawHandles::validate() const { state_->validate(false); }
Snapshot RawHandles::snapshot() const {
  auto &s = *state_;
  s.validate(false);
  Snapshot out;
  out.attached = s.attached;
  out.free = s.encode(s.world->handles.free);
  for (const auto &h : s.heads)
    out.heads.push_back(h.binding);
  for (const auto *n : s.world->handles.nodes())
    out.nodes.push_back(s.node(*n));
  return out;
}
RegistrySnapshot RawHandles::registry_snapshot() const {
  RegistrySnapshot out;
  out.attached = state_->attached;
  for (const auto &h : state_->heads)
    out.heads.push_back(h.binding);
  return out;
}
void RawHandles::detach() {
  auto &s = *state_;
  s.ensure();
  if (!s.attached)
    throw Error("raw handle overlay is already detached");
  s.memory->detach_overlay(state_);
  s.attached = false;
  s.active = false;
}
} // namespace champions::simulation::rta::raw_handles
