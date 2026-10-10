#include "champions/core_state.hpp"

namespace champions::simulation::rta {
std::uint32_t Lcg::draw() {
  state = state * 0x0019660dU + 0x3c6ef35fU;
  return state >> 16;
}
Raw Lcg::unit() { return mul(from_int(draw()), TWO_M16); }
Raw Lcg::signed_unit() {
  const auto u = unit();
  return sub(add(u, u), ONE);
}
std::array<Raw, 3> Lcg::in_unit_sphere() {
  for (;;) {
    const auto x = signed_unit(), y = signed_unit(), z = signed_unit();
    if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z))))
      return {x, y, z};
  }
}
std::uint32_t LibcRand::rand() {
  state = state * UINT64_C(6364136223846793005) + 1;
  return std::uint32_t((state >> 32) & 0x7fffffff);
}
void Pool::init() {
  link.clear();
  last = 0;
  auto address = BASE;
  for (unsigned c = 0; c < 3; ++c) {
    base[c] = head[c] = address;
    const auto size = CLASSES[c][0], count = CLASSES[c][1];
    for (std::uint32_t n = 0; n < count; ++n)
      link[address + n * size] = n + 1 < count ? address + (n + 1) * size : 0;
    address += size * count;
  }
  end = address;
}
std::uint32_t Pool::allocate(std::uint32_t raw) {
  const auto size = raw & 0x80000000U ? std::int64_t(raw) - INT64_C(0x100000000)
                                      : std::int64_t(raw);
  for (unsigned c = 0; c < 3; ++c)
    if (std::int64_t(CLASSES[c][0]) >= size && head[c]) {
      const auto address = head[c];
      const auto found = link.find(address);
      if (found == link.end())
        throw PoolError("free list is corrupt");
      head[c] = found->second;
      link.erase(found);
      last = address;
      return address;
    }
  return 0;
}
unsigned Pool::class_of(std::uint32_t address) const {
  if (address >= end)
    throw PoolError("address is past the pool");
  if (address < base[1])
    return 0;
  if (address < base[2])
    return 1;
  return 2;
}
void Pool::free(std::uint32_t address) {
  const auto c = class_of(address);
  link[address] = head[c];
  head[c] = address;
}
std::vector<std::uint32_t> Pool::free_list(unsigned index) const {
  if (index >= 3)
    throw std::out_of_range("pool class index");
  std::vector<std::uint32_t> result;
  auto address = head[index];
  while (address) {
    if (result.size() > 4096)
      throw PoolError("free list loops");
    result.push_back(address);
    const auto at = link.find(address);
    if (at == link.end())
      throw PoolMissingLink("missing free-list link");
    address = at->second;
  }
  return result;
}
std::size_t Pool::count_free() const {
  // Preserve Python's left-to-right error precedence when several lists are
  // corrupt.
  std::size_t count = 0;
  for (unsigned index = 0; index < 3; ++index)
    count += free_list(index).size();
  return count;
}
Object *HandleObjectRef::typed() const {
  if (raw)
    throw HandleError("opaque raw object has no typed actor");
  return object;
}
HandleObjectRef Link::object_ref() const {
  if (obj && object_raw)
    throw HandleError("conflicting handle object identities");
  return {obj, object_raw};
}
void Link::set_object(HandleObjectRef value) {
  if (value.object && value.raw)
    throw HandleError("conflicting handle object identities");
  obj = value.object;
  object_raw = value.raw;
}
HandlePool::HandlePool(std::size_t count) {
  nodes_.reserve(count);
  try {
    for (std::size_t i = 0; i < count; ++i)
      nodes_.push_back(new Node(i));
  } catch (...) {
    for (auto *n : nodes_)
      delete n;
    throw;
  }
  init();
}
HandlePool::~HandlePool() {
  for (auto *n : nodes_)
    delete n;
}
void HandlePool::init() {
  for (std::size_t i = 0; i < nodes_.size(); ++i)
    nodes_[i]->next = i + 1 < nodes_.size() ? nodes_[i + 1] : nullptr;
  free = nodes_.empty() ? nullptr : nodes_[0];
}
Node *HandlePool::take() {
  if (!free)
    throw HandleError("node pool exhausted");
  if (free->kind != LinkKind::Node)
    throw HandleError("free list does not contain a pool node");
  auto *node = static_cast<Node *>(free);
  free = node->next;
  node->tag = 0;
  return node;
}
void HandlePool::release(Link *node) {
  node->next = free;
  free = node;
}
void HandlePool::push(Link &head, Link *node) {
  node->next = head.next;
  if (head.next)
    head.next->prev = node;
  node->prev = &head;
  head.next = node;
}
void HandlePool::unlink(Link *node) {
  if (node->prev)
    node->prev->next = node->next;
  if (node->next)
    node->next->prev = node->prev;
}
Node *HandlePool::link(Cell &head, Cell &back, Object *obj) {
  return link_ref(head, back, HandleObjectRef::known(obj));
}
Node *HandlePool::link_ref(Cell &head, Cell &back, HandleObjectRef obj) {
  if (obj.object && obj.raw)
    throw HandleError("conflicting handle object identities");
  auto *a = take();
  auto *b = take();
  push(head, a);
  push(back, b);
  a->set_object(obj);
  a->partner = b;
  b->set_object({});
  b->partner = a;
  return a;
}
void HandlePool::link_both(Cell &head_a, Object *obj_b, Cell &head_b,
                           Object *obj_a) {
  auto *a = take();
  auto *b = take();
  push(head_a, a);
  push(head_b, b);
  a->set_object(HandleObjectRef::known(obj_a));
  b->set_object(HandleObjectRef::known(obj_b));
  a->partner = b;
  b->partner = a;
}
Node *HandlePool::add(Cell &head, Object *obj) {
  auto *n = take();
  push(head, n);
  n->set_object(HandleObjectRef::known(obj));
  n->partner = nullptr;
  return n;
}
Node *HandlePool::insert(Cell &head, Link *after, Object *obj) {
  auto *n = take();
  n->set_object(HandleObjectRef::known(obj));
  n->partner = nullptr;
  push(after ? *after : head, n);
  return n;
}
Object *HandlePool::remove(Link *node) {
  auto *obj = node->object_ref().typed(); // Refuse opaque before mutation.
  (void)remove_ref(node);
  return obj;
}
HandleObjectRef HandlePool::remove_ref(Link *node) {
  const auto obj = node->object_ref();
  unlink(node);
  auto *partner = node->partner;
  if (partner) {
    unlink(partner);
    release(partner);
  }
  release(node);
  return obj;
}
void HandlePool::clear(Cell &head) {
  auto *node = head.next;
  while (node) {
    if (node->kind == LinkKind::Iterator)
      throw HandleError("list cleared with iterator present");
    auto *following = node->next;
    if (following && following == node->partner)
      throw HandleError("both partners in cleared list");
    unlink(node);
    auto *partner = node->partner;
    if (partner) {
      unlink(partner);
      release(partner);
    }
    release(node);
    node = following;
  }
}
std::size_t HandlePool::count_free() const {
  std::size_t count = 0;
  for (auto *n = free; n; n = n->next)
    ++count;
  return count;
}
std::vector<Object *> HandlePool::items(const Cell &head) {
  std::vector<Object *> out;
  for (auto *n = head.next; n; n = n->next)
    if (auto *obj = n->object_ref().typed())
      out.push_back(obj);
  return out;
}
Iterator::Iterator(Cell &head, std::uint32_t skip)
    : Link(LinkKind::Iterator), mask(skip) {
  stand_behind(find(head.next));
}
Link *Iterator::find(Link *n) const {
  while (n) {
    auto *obj = n->object_ref().typed();
    if (obj && !(obj->flags & mask))
      return n;
    n = n->next;
  }
  return nullptr;
}
void Iterator::stand_behind(Link *n) {
  if (!n) {
    next = prev = nullptr;
    return;
  }
  prev = n;
  next = n->next;
  if (n->next)
    n->next->prev = this;
  n->next = this;
}
Object *Iterator::current() const {
  if (!prev)
    return nullptr;
  if (prev->kind == LinkKind::Cell)
    throw HandleError("iterator prev is list head");
  return prev->object_ref().typed();
}
void Iterator::advance() {
  if (!prev)
    throw HandleError("advance after end");
  auto *n = find(next);
  if (next)
    next->prev = prev;
  prev->next = next;
  stand_behind(n);
}
void Iterator::end() {
  if (next)
    next->prev = prev;
  if (prev)
    prev->next = next;
}
std::vector<Object *> walk(Cell &head, std::uint32_t mask) {
  std::vector<Object *> out;
  Iterator it(head, mask);
  while (!it.done()) {
    out.push_back(it.current());
    it.advance();
  }
  it.end();
  return out;
}
} // namespace champions::simulation::rta
