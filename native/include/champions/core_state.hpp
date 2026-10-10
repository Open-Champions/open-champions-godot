#pragma once
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace champions::simulation::rta {
// Separate instances/domains are intentional; cosmetic draws must never advance
// shared simulation state. Raw floating values use the portable arithmetic
// layer.
struct Lcg {
  std::uint32_t state;
  explicit Lcg(std::uint32_t seed = 0) : state(seed) {}
  std::uint32_t draw();
  Raw unit();
  Raw signed_unit();
  std::array<Raw, 3> in_unit_sphere();
};
struct SharedRandom : Lcg {
  using Lcg::Lcg;
};
struct CosmeticRandom : Lcg {
  using Lcg::Lcg;
};
struct EffectRandom : Lcg {
  using Lcg::Lcg;
};
struct ObjectRandom : Lcg {
  explicit ObjectRandom(SharedRandom &shared) : Lcg(shared.draw()) {}
};
struct SpellRandom : Lcg {
  explicit SpellRandom(SharedRandom &shared) : Lcg(shared.draw()) {}
};
struct LibcRand {
  std::uint64_t state;
  explicit LibcRand(std::uint64_t value = 1) : state(value) {}
  void seed(std::uint32_t value) { state = value; }
  std::uint32_t rand();
};
struct PoolError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct PoolMissingLink : std::out_of_range {
  using std::out_of_range::out_of_range;
};
// Virtual addresses and free-list metadata, as in ref_core.Pool. No host memory
// is exposed; the reference itself does not model allocated block payload
// bytes.
struct Pool {
  inline static constexpr std::uint32_t BASE = 0x5f88c0;
  inline static constexpr std::array<std::array<std::uint32_t, 2>, 3> CLASSES{
      {{{0x180, 512}}, {{0x500, 512}}, {{3000, 32}}}};
  std::array<std::uint32_t, 3> base{}, head{};
  std::map<std::uint32_t, std::uint32_t> link;
  std::uint32_t last = 0, end = 0;
  Pool() { init(); }
  void init();
  std::uint32_t allocate(std::uint32_t size);
  unsigned class_of(std::uint32_t address) const;
  void free(std::uint32_t address);
  std::vector<std::uint32_t> free_list(unsigned index) const;
  std::size_t count_free() const;
};
struct HandleError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Object {
  std::uint32_t id = 0, flags = 0;
};
enum class LinkKind { Cell, Node, Iterator };
struct Link {
  LinkKind kind;
  Link *next = nullptr, *prev = nullptr, *partner = nullptr;
  Object *obj = nullptr;
  std::uint32_t tag = 0;
  explicit Link(LinkKind type) : kind(type) {}
  Link(const Link &) = delete;
  Link &operator=(const Link &) = delete;
};
struct Cell : Link {
  Cell() : Link(LinkKind::Cell) {}
};
struct Node : Link {
  std::size_t index;
  explicit Node(std::size_t i) : Link(LinkKind::Node), index(i) {}
};
class HandlePool {
  std::vector<Node *> nodes_;

public:
  inline static constexpr std::size_t NODE_COUNT = 6096;
  inline static constexpr std::uint32_t NODE_BASE = 0x847268, NODE_SIZE = 20;
  Link *free = nullptr;
  explicit HandlePool(std::size_t count = NODE_COUNT);
  ~HandlePool();
  HandlePool(const HandlePool &) = delete;
  HandlePool &operator=(const HandlePool &) = delete;
  const std::vector<Node *> &nodes() const { return nodes_; }
  void init();
  Node *take();
  void release(Link *node);
  static void push(Link &head, Link *node);
  static void unlink(Link *node);
  Node *link(Cell &head, Cell &back, Object *obj);
  void link_both(Cell &head_a, Object *obj_b, Cell &head_b, Object *obj_a);
  Node *add(Cell &head, Object *obj);
  Node *insert(Cell &head, Link *after, Object *obj);
  Object *remove(Link *node);
  void clear(Cell &head);
  std::size_t count_free() const;
  static std::vector<Object *> items(const Cell &head);
};
// Explicit end(), not RAII unlinking: the Python/original end preserves fields.
// Cells, objects and pool nodes must outlive every iterator referencing them.
struct Iterator : Link {
  std::uint32_t mask;
  explicit Iterator(Cell &head, std::uint32_t skip = 0);
  bool done() const { return prev == nullptr; }
  Object *current() const;
  void advance();
  void end();

private:
  Link *find(Link *node) const;
  void stand_behind(Link *node);
};
std::vector<Object *> walk(Cell &head, std::uint32_t mask = 0);
} // namespace champions::simulation::rta
