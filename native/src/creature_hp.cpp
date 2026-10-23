#include "champions/detail/checkpoint_fields.hpp"
// Authored from tools/ref_creature.py at
// a0d89502ce1431375a6fc045b91a73039114e6b9 SHA256
// debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7,
// docs/spec/creature-tick.md section12.1, in champions-return-to-arms-re.
// Player underlying reads and unlogged byte writes authored from ref_player.py
// at 356add27d50e50972b2a671b31e8f4ec93e6778c, SHA256
// ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d.
// Raw arithmetic delegates to the already pinned core model; no retail bytes.
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
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
Memory::Memory(Limits limits, UnderlyingLoad under)
    : limits_(limits), under_(std::move(under)) { check_limits(limits); }
std::uint64_t Memory::underlying(Address address, std::size_t size) const {
  if (!under_) return 0;
  if (under_depth_ >= MAX_DEPTH)
    throw ResourceLimit("underlying memory recursion budget exceeded");
  auto fn = under_;
  ++under_depth_;
  struct Unwind { std::size_t &depth; ~Unwind() { --depth; } } unwind{under_depth_};
  auto value = fn(address, size);
  if (size < 8 && value != masked(value, size))
    throw Error("underlying memory result exceeds unsigned width");
  return value;
}
std::uint64_t Memory::load(Address address, std::size_t size) const {
  span(address, size);
  if (under_) {
    // Snapshot presence and byte values before any delegated read. A callback
    // may mutate backing, but the reference's parts list retains old values.
    std::array<std::uint8_t, 8> parts{};
    std::array<bool, 8> present{};
    std::size_t count = 0;
    for (std::size_t i = 0; i < size; ++i) {
      auto found = bytes_.find(std::uint32_t(address + i));
      if (found != bytes_.end()) { present[i] = true; parts[i] = found->second; ++count; }
    }
    if (!count) return underlying(address, size);
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < size; ++i)
      result |= (present[i] ? std::uint64_t(parts[i]) : underlying(address + i, 1)) << (8 * i);
    return result;
  }
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < size; ++i) {
    const auto a = std::uint32_t(address + i);
    if (overlay_ && overlay_->covers(a)) {
      result |= std::uint64_t(overlay_->read_byte(a)) << (8 * i);
    } else {
      auto found = bytes_.find(a);
      if (found != bytes_.end())
        result |= std::uint64_t(found->second) << (8 * i);
    }
  }
  return result;
}
void Memory::poke(Address address, std::size_t size, std::uint64_t value) {
  span(address, size);
  // Preflight every covered byte before writing any backing bytes. In
  // particular an unaligned span may start in writable padding and end in a
  // managed pointer. No partial padding write is permitted on that refusal.
  for (std::size_t i = 0; i < size; ++i)
    if (overlay_ && overlay_->covers(std::uint32_t(address + i)))
      throw Error("raw write intersects read-only overlay");
  std::size_t additions = 0;
  for (std::size_t i = 0; i < size; ++i)
    additions += !bytes_.count(std::uint32_t(address + i));
  if (additions && (combat_bytes() > limits_.memory_bytes ||
                    additions > limits_.memory_bytes - combat_bytes()))
    throw ResourceLimit("sparse memory byte budget exceeded");
  for (std::size_t i = 0; i < size; ++i)
    bytes_[std::uint32_t(address + i)] = std::uint8_t(value >> (8 * i));
}
void Memory::store(Address address, std::size_t size, std::uint64_t value) {
  span(address, size);
  if (log_prefix_ + log_.size() >= limits_.history_entries)
    throw ResourceLimit("memory store history budget exceeded");
  value = masked(value, size);
  log_.push_back({std::uint32_t(address), size, value});
  poke(address, size, value);
}
void Memory::write_bytes(Address address, const std::vector<std::uint8_t> &data) {
  if (data.empty()) return;
  if (address > UINT32_MAX || data.size() > MAX_MEMORY_BYTES ||
      data.size() > UINT64_C(0x100000000) - address)
    throw ResourceLimit("byte write outside supported memory span");
  std::size_t additions = 0;
  for (std::size_t i = 0; i < data.size(); ++i) {
    auto a = std::uint32_t(address + i);
    if (overlay_ && overlay_->covers(a))
      throw Error("raw write intersects read-only overlay");
    additions += !bytes_.count(a);
  }
  if (additions && (combat_bytes() > limits_.memory_bytes ||
                    additions > limits_.memory_bytes - combat_bytes()))
    throw ResourceLimit("sparse memory byte budget exceeded");
  for (std::size_t i = 0; i < data.size(); ++i)
    bytes_[std::uint32_t(address + i)] = data[i];
}
void Memory::write_host_bytes(Address address,
                              const std::vector<std::uint8_t> &data) {
  if (data.empty()) return;
  if (address > UINT32_MAX || data.size() > UINT64_C(0x100000000) - address)
    throw ResourceLimit("host byte write outside supported memory span");
  for (std::size_t i = 0; i < data.size(); ++i) {
    const auto a = std::uint32_t(address + i);
    const auto found = bytes_.find(a);
    if (data[i] || under_) {
      if (found == bytes_.end()) {
        bytes_.emplace(a, data[i]);
        ++host_extra_;
      } else {
        found->second = data[i];
      }
    } else if (found != bytes_.end()) {
      bytes_.erase(found);
      if (host_extra_) --host_extra_;
    }
  }
}
const std::map<std::uint32_t, std::uint8_t> &Memory::bytes() const noexcept {
  return bytes_;
}
std::vector<std::uint8_t> Memory::effective_bytes(Address address,
                                                  std::size_t count) const {
  if (address > UINT32_MAX || count > MAX_MEMORY_BYTES ||
      count > UINT64_C(0x100000000) - address)
    throw ResourceLimit("effective memory observation exceeds range budget");
  std::vector<std::uint8_t> out;
  out.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    out.push_back(static_cast<std::uint8_t>(load(address + i, 1)));
  return out;
}
void Memory::attach_overlay(std::shared_ptr<const ReadOverlay> overlay) {
  if (under_)
    throw Error("read-only overlay and underlying memory are separate policies");
  if (!overlay || overlay_)
    throw Error("memory requires one nonnull overlay attachment");
  overlay_ = std::move(overlay);
}
void Memory::detach_overlay(
    const std::shared_ptr<const ReadOverlay> &expected) {
  if (!overlay_ || !expected || overlay_.get() != expected.get() ||
      overlay_.owner_before(expected) || expected.owner_before(overlay_))
    throw Error("overlay detach token does not own this attachment");
  overlay_.reset();
}
std::shared_ptr<const ReadOverlay> Memory::overlay() const noexcept {
  return overlay_;
}
const std::vector<Write> &Memory::log() const noexcept { return log_; }
void Memory::clear_log() noexcept {
  log_.clear();
  log_prefix_ = 0;
}
Model::Model(CallbackPolicy selected, Limits limits)
    : limits_(limits), owned_memory_(std::make_unique<Memory>(limits)),
      mem(*owned_memory_), policy(selected) {
  if (selected != CallbackPolicy::RequireHandlers &&
      selected != CallbackPolicy::ReferenceZero)
    throw Error("unknown creature callback policy");
}
Model::Model(Memory &shared, CallbackPolicy selected, Limits limits)
    : limits_(limits), mem(shared), policy(selected) {
  if (selected != CallbackPolicy::RequireHandlers &&
      selected != CallbackPolicy::ReferenceZero)
    throw Error("unknown creature callback policy");
}
void Model::write(Address address, std::size_t size, std::uint64_t value) {
  span(address, size);
  if (writes_prefix_ + writes.size() >= limits_.history_entries ||
      events_prefix_ + events.size() >= limits_.history_entries)
    throw ResourceLimit("creature write/event history budget exceeded");
  const Write entry{std::uint32_t(address), size, masked(value, size)};
  writes.push_back(entry);
  events.push_back({EventKind::Write, entry, {}});
  mem.store(address, size, entry.value);
}
void Model::call(const std::string &name, std::vector<std::uint32_t> args) {
  if (calls_prefix_ + calls.size() >= limits_.history_entries ||
      events_prefix_ + events.size() >= limits_.history_entries)
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
void Memory::checkpoint(detail::Checkpoint &a) {
  a.value(bytes_);
  a.history(log_);
  if (a.reading)
    log_prefix_ = a.deferred() ? a.records(log_) : 0;
  // The snapshot format (version 1) does not carry which entries the script
  // store created, so after a restore every restored byte is treated as
  // host-owned: the combat budget then counts only entries added afterwards,
  // and a script store never makes a later combat addition throw (WA-F4). The
  // restore is already capped at memory_bytes entries (below), so total
  // growth stays bounded.
  if (a.reading) host_extra_ = bytes_.size();
  detail::Checkpoint::require(bytes_.size() <= limits_.memory_bytes &&
                                  log_prefix_ + log_.size() <=
                                      limits_.history_entries,
                              "checkpoint memory limits");
}
void Model::checkpoint_history(detail::Checkpoint &a) {
  if (!a.reading)
    return;
  writes_prefix_ = a.deferred() ? a.records(writes) : 0;
  calls_prefix_ = a.deferred() ? a.records(calls) : 0;
  events_prefix_ = a.deferred() ? a.records(events) : 0;
  detail::Checkpoint::require(
      writes_prefix_ + writes.size() <= limits_.history_entries &&
          calls_prefix_ + calls.size() <= limits_.history_entries &&
          events_prefix_ + events.size() <= limits_.history_entries,
      "checkpoint model history limits");
}
} // namespace champions::simulation::rta::creature_hp
