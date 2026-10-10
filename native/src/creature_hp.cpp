// Authored from tools/ref_creature.py at
// a0d89502ce1431375a6fc045b91a73039114e6b9 SHA256
// debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7,
// docs/spec/creature-tick.md section12.1, in champions-return-to-arms-re.
// Raw arithmetic delegates to the already pinned core model; no retail bytes.
#include "champions/creature_hp.hpp"
#include "champions/core_math.hpp"
#include <utility>

namespace champions::simulation::rta::creature_hp {
namespace {
void check_limits(const Limits &limits) {
  if (limits.memory_bytes > MAX_MEMORY_BYTES ||
      limits.history_entries > MAX_HISTORY ||
      limits.callback_entries > MAX_CALLBACKS || limits.depth > MAX_DEPTH)
    throw ResourceLimit("creature HP limits exceed supported hard caps");
}
void span(Address address, std::size_t size) {
  if (address > 0xffffffffu || size > 8 ||
      size > UINT64_C(0x100000000) - address)
    throw ResourceLimit(
        "memory address, width or span outside supported domain");
}
std::uint64_t masked(std::uint64_t value, std::size_t size) {
  return size == 8 ? value : value & ((UINT64_C(1) << (size * 8)) - 1);
}
} // namespace
Memory::Memory(Limits limits) : limits_(limits) { check_limits(limits); }
std::uint64_t Memory::load(Address address, std::size_t size) const {
  span(address, size);
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < size; ++i) {
    auto found = bytes_.find(std::uint32_t(address + i));
    if (found != bytes_.end())
      result |= std::uint64_t(found->second) << (8 * i);
  }
  return result;
}
void Memory::poke(Address address, std::size_t size, std::uint64_t value) {
  span(address, size);
  std::size_t additions = 0;
  for (std::size_t i = 0; i < size; ++i)
    additions += !bytes_.count(std::uint32_t(address + i));
  if (additions > limits_.memory_bytes - bytes_.size())
    throw ResourceLimit("sparse memory byte budget exceeded");
  for (std::size_t i = 0; i < size; ++i)
    bytes_[std::uint32_t(address + i)] = std::uint8_t(value >> (8 * i));
}
void Memory::store(Address address, std::size_t size, std::uint64_t value) {
  span(address, size);
  if (log_.size() >= limits_.history_entries)
    throw ResourceLimit("memory store history budget exceeded");
  value = masked(value, size);
  log_.push_back({std::uint32_t(address), size, value});
  poke(address, size, value);
}
const std::map<std::uint32_t, std::uint8_t> &Memory::bytes() const noexcept {
  return bytes_;
}
const std::vector<Write> &Memory::log() const noexcept { return log_; }
void Memory::clear_log() noexcept { log_.clear(); }
Model::Model(CallbackPolicy selected, Limits limits)
    : limits_(limits), mem(limits), policy(selected) {
  if (selected != CallbackPolicy::RequireHandlers &&
      selected != CallbackPolicy::ReferenceZero)
    throw Error("unknown creature callback policy");
}
void Model::write(Address address, std::size_t size, std::uint64_t value) {
  span(address, size);
  if (writes.size() >= limits_.history_entries ||
      events.size() >= limits_.history_entries)
    throw ResourceLimit("creature write/event history budget exceeded");
  const Write entry{std::uint32_t(address), size, masked(value, size)};
  writes.push_back(entry);
  events.push_back({EventKind::Write, entry, {}});
  mem.store(address, size, entry.value);
}
void Model::call(const std::string &name, std::vector<std::uint32_t> args) {
  if (calls.size() >= limits_.history_entries ||
      events.size() >= limits_.history_entries)
    throw ResourceLimit("creature call/event history budget exceeded");
  Call entry{name, std::move(args)};
  calls.push_back(entry);
  events.push_back({EventKind::Call, {}, entry});
  if (callbacks.size() > limits_.callback_entries)
    throw ResourceLimit("creature callback registration budget exceeded");
  for (const auto &binding : callbacks)
    if (binding.first.size() > MAX_CALLBACK_NAME)
      throw ResourceLimit("creature callback name budget exceeded");
  const auto found = callbacks.find(name);
  if (found != callbacks.end() && found->second) {
    auto function = found->second;
    function(*this, entry.args);
  } else if (policy == CallbackPolicy::RequireHandlers) {
    throw MissingHandler("missing creature callback: " + name);
  } else if (policy != CallbackPolicy::ReferenceZero) {
    throw Error("unknown creature callback policy");
  }
}
void Model::take_damage(Address creature, Address attacker, std::uint32_t flags,
                        Raw amount) {
  span(creature, 0);
  span(attacker, 0);
  if (depth_ >= limits_.depth)
    throw ResourceLimit("nested creature damage budget exceeded");
  ++depth_;
  struct Unwind {
    std::size_t &depth;
    ~Unwind() { --depth; }
  } unwind{depth_};
  const auto u32 = [&](Address address) { return Raw(mem.load(address, 4)); };
  if (mem.load(creature + 0x231, 1))
    return;
  write(creature + 0x3b0, 2, 0x7fff);
  if (mem.load(creature + 0xa8, 8) & UINT64_C(0x1000000000000)) {
    for (const auto offset : {0x348u, 0x344u, 0x340u, 0x350u, 0x34cu})
      write(creature + offset, 4, 0);
    return;
  }
  write(creature + 0x304, 4, u32(G_SIM_FRAME));
  write(creature + 0x2dc, 4, u32(creature + 0x2dc) | flags);
  if (attacker && lt(ZERO, u32(creature + 0x228))) {
    const auto dx = sub(u32(creature + 0x10), u32(attacker + 0x10));
    const auto dy = sub(u32(creature + 0x14), u32(attacker + 0x14));
    const auto angle = atan2_16(dx, dy);
    write(creature + 0x2e0, 2,
          UINT64_C(0x4000) - std::uint64_t(std::int64_t(angle)));
    call("handle_clear", {std::uint32_t(creature + 0x2e4)});
    auto who = attacker;
    if (u32(attacker + 0xc) & 0x80) {
      const auto node = u32(attacker + 0x4c);
      if (node)
        who = u32(Address(node) + 0xc);
    } else if (flags & 0x10000000) {
      const auto node = u32(creature + 0x360);
      if (node)
        who = u32(Address(node) + 0xc);
    }
    if (who)
      call("handle_link", {std::uint32_t(creature + 0x2e4), std::uint32_t(who),
                           std::uint32_t(who)});
  }
  write(creature + 0x228, 4, sub(u32(creature + 0x228), amount));
  write(creature + 0xc8, 4, add(u32(creature + 0xc8), amount));
}
} // namespace champions::simulation::rta::creature_hp
