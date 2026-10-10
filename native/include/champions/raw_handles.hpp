#pragma once
#include "champions/creature_hp.hpp"
#include "champions/objects.hpp"
#include <memory>
#include <string>
#include <vector>

namespace champions::simulation::rta::raw_handles {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Limits {
  std::size_t heads = 65536;
};
struct HeadBinding {
  std::uint32_t address = 0;
  std::uint64_t identity = 0, object_serial = 0;
  std::string label;
  bool active = true;
};
struct NodeSnapshot {
  std::uint32_t address = 0, next = 0, prev = 0, partner = 0, object = 0;
  std::uint16_t tag = 0;
  std::uint64_t object_serial = 0;
  bool opaque = false;
};
struct RegistrySnapshot {
  bool attached = false;
  std::vector<HeadBinding> heads;
};
struct Snapshot {
  bool attached = false;
  std::uint32_t free = 0;
  std::vector<HeadBinding> heads;
  std::vector<NodeSnapshot> nodes;
};
// Uses exactly World.handles. A physical head address is separate from its
// semantic Cell/generation identity. No GameObject is fabricated for raw who.
// Constructor requires the full 6096-node pool and encodable quiescent links.
// Only explicit head bindings have read-through coverage; other World cells
// are cached for pointer encoding, not overlaid on ordinary Memory backing.
// Tags must fit 16 bits; node padding stays normal backing memory. World/Memory
// must be alive for operations. Labels are 1..128 bytes; at most 65536 head
// records are retained.
class RawHandles {
  struct State;
  std::shared_ptr<State> state_;

public:
  RawHandles(objects::World &, creature_hp::Memory &, Limits = {});
  ~RawHandles();
  RawHandles(const RawHandles &) = delete;
  RawHandles &operator=(const RawHandles &) = delete;
  RawHandles(RawHandles &&) = delete;
  RawHandles &operator=(RawHandles &&) = delete;
  HandlePool &pool();
  void bind_object(objects::GameObject &);
  // Explicit physical address reuse after old generation is dead and heads
  // empty. Old semantic prev references in free nodes remain old identities.
  void rebind_object(objects::GameObject &);
  void bind_head(std::uint32_t, Cell &, const std::string &label);
  Cell &intern_raw_head(std::uint32_t, const std::string &label);
  void clear(std::uint32_t head);
  std::uint32_t link(std::uint32_t head, std::uint32_t back,
                     std::uint32_t raw_object);
  std::uint32_t link_known(std::uint32_t head, std::uint32_t back,
                           objects::GameObject &);
  HandleObjectRef remove(std::uint32_t node);
  // Separate graph/registry observation; Memory.bytes() remains sparse backing.
  // Snapshot remains available after explicit detach while World is alive.
  Snapshot snapshot() const;
  // Registry-only observation never walks/dereferences borrowed graph links.
  RegistrySnapshot registry_snapshot() const;
  // Explicit detach needs Memory alive. Destructor only deactivates its token:
  // retained Memory reads then refuse, never call a destroyed bridge/World.
  void detach();
};
} // namespace champions::simulation::rta::raw_handles
