// Authored from champions-return-to-arms-re tools/ref_creature.py at
// a0d89502ce1431375a6fc045b91a73039114e6b9, SHA256
// debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7.
// Creature_turnToward, Creature_moveBy, _water, settle and base virtual slots
// 6/10/11. Core
// arithmetic uses the independently pinned
// 41e847af5360724ab62d208594f8027ea3c1b686 model.
#include "champions/creature_movement.hpp"
#include "champions/core_math.hpp"
#include "champions/detail/checkpoint.hpp"
#include <utility>

namespace champions::simulation::rta::creature_movement {
namespace {
constexpr Raw DT = 0x3c888888, F60 = 0x42700000, EPS = 0x3a83126e;
void span(Address address, std::size_t size = 0) {
  if (address > UINT32_MAX || size > 8 ||
      size > UINT64_C(0x100000000) - address)
    throw ResourceLimit("movement address or span outside raw32 domain");
}
struct Depth {
  std::size_t &value;
  Depth(std::size_t &v, std::size_t limit) : value(v) {
    if (value >= limit)
      throw ResourceLimit("movement nested operation budget exceeded");
    ++value;
  }
  ~Depth() { --value; }
};
void validate_value(const Value &v, std::size_t depth, std::size_t &nodes,
                    std::size_t &bytes) {
  if (depth > MAX_VALUE_DEPTH || ++nodes > MAX_VALUE_NODES)
    throw ResourceLimit("movement callback value complexity exceeded");
  // Reject inactive payloads so complete observation has one canonical meaning.
  if ((v.kind != ValueKind::Integer && v.scalar != 0) ||
      (v.kind != ValueKind::Vector && v.vec != Vec3{}) ||
      (v.kind != ValueKind::List && v.kind != ValueKind::Tuple &&
       !v.items.empty()) ||
      (v.kind != ValueKind::Buffer && !v.bytes.empty()))
    throw Error("movement callback value has inactive payload");
  switch (v.kind) {
  case ValueKind::None:
  case ValueKind::Integer:
  case ValueKind::Vector:
    break;
  case ValueKind::List:
  case ValueKind::Tuple:
    if (v.items.size() > MAX_CONTACTS)
      throw ResourceLimit("movement callback sequence exceeds 16 entries");
    for (const auto &item : v.items)
      validate_value(item, depth + 1, nodes, bytes);
    break;
  case ValueKind::Buffer:
    if (v.bytes.size() > MAX_BUFFER_BYTES - bytes)
      throw ResourceLimit("movement callback buffer budget exceeded");
    bytes += v.bytes.size();
    break;
  default:
    throw Error("unknown movement callback value kind");
  }
}
void validate_values(const std::vector<Value> &values) {
  if (values.size() > MAX_CONTACTS)
    throw ResourceLimit("movement callback argument count exceeded");
  std::size_t nodes = 0, bytes = 0;
  for (const auto &v : values)
    validate_value(v, 0, nodes, bytes);
}
std::int64_t integer(const Value &v) {
  if (v.kind != ValueKind::Integer)
    throw Error("movement callback requires integer result");
  return v.scalar;
}
Address address(const Value &v) {
  const auto a = integer(v);
  if (a < 0 || std::uint64_t(a) > UINT32_MAX)
    throw ResourceLimit("movement callback address outside raw32 domain");
  return std::uint64_t(a);
}
Value i(Address n) { return Value::integer(std::int64_t(n)); }
Value default_result(const std::string &name, const std::vector<Value> &args) {
  if (name == "world_collectObjectsInBox")
    return Value::list({});
  if (name == "world_slideMove" && integer(args.at(3)))
    return Value::tuple({i(0), Value::vector({})});
  if (name == "libc_sprintf")
    return Value::buffer({});
  if (name == "obj_updateGridCell" || name == "Creature.v6" ||
      name == "Creature.v10" || name == "Creature.v11")
    return {};
  return i(0);
}
} // namespace
Value Value::integer(std::int64_t n) {
  Value v;
  v.kind = ValueKind::Integer;
  v.scalar = n;
  return v;
}
Value Value::vector(Vec3 n) {
  Value v;
  v.kind = ValueKind::Vector;
  v.vec = n;
  return v;
}
Value Value::list(std::vector<Value> n) {
  Value v;
  v.kind = ValueKind::List;
  v.items = std::move(n);
  return v;
}
Value Value::tuple(std::vector<Value> n) {
  Value v;
  v.kind = ValueKind::Tuple;
  v.items = std::move(n);
  return v;
}
Value Value::buffer(std::string n) {
  Value v;
  v.kind = ValueKind::Buffer;
  v.bytes = std::move(n);
  return v;
}
Model::Model(CallbackPolicy selected, Limits limits)
    : limits_(limits), owned_memory_(std::make_unique<Memory>(limits)),
      mem(*owned_memory_), policy(selected) {
  if (selected != CallbackPolicy::RequireHandlers &&
      selected != CallbackPolicy::ReferenceZero)
    throw Error("unknown movement callback policy");
}
Model::Model(Memory &backing, CallbackPolicy selected, Limits limits)
    : limits_(limits), mem(backing), policy(selected) {
  if (limits.memory_bytes > creature_hp::MAX_MEMORY_BYTES ||
      limits.history_entries > creature_hp::MAX_HISTORY ||
      limits.callback_entries > creature_hp::MAX_CALLBACKS ||
      limits.depth > creature_hp::MAX_DEPTH)
    throw ResourceLimit("movement limits exceed supported hard caps");
  if (selected != CallbackPolicy::RequireHandlers &&
      selected != CallbackPolicy::ReferenceZero)
    throw Error("unknown movement callback policy");
}
Raw Model::u32(Address a) const { return Raw(mem.load(a, 4)); }
Vec3 Model::vec(Address a) const {
  span(a);
  return {u32(a), u32(a + 4), u32(a + 8)};
}
void Model::write(Address a, std::size_t size, std::uint64_t value) {
  span(a, size);
  if (writes_prefix_ + writes.size() >= limits_.history_entries ||
      events_prefix_ + events.size() >= limits_.history_entries)
    throw ResourceLimit("movement write/event history budget exceeded");
  if (size < 8)
    value &= (UINT64_C(1) << (size * 8)) - 1;
  const Write entry{std::uint32_t(a), size, value};
  writes.push_back(entry);
  events.push_back({EventKind::Write, entry, {}});
  mem.store(a, size, value);
}
void Model::wvec(Address a, Vec3 v) {
  write(a, 8, std::uint64_t(v[0]) | (std::uint64_t(v[1]) << 32));
  write(a + 8, 4, v[2]);
}
Value Model::call(const std::string &name, std::vector<Value> args) {
  validate_values(args);
  for (auto &arg : args)
    if (arg.kind == ValueKind::Integer)
      arg.scalar = std::uint32_t(arg.scalar);
  if (calls_prefix_ + calls.size() >= limits_.history_entries ||
      events_prefix_ + events.size() >= limits_.history_entries)
    throw ResourceLimit("movement call/event history budget exceeded");
  const Call entry{name, std::move(args)};
  calls.push_back(entry);
  events.push_back({EventKind::Call, {}, entry});
  if (callbacks.size() > limits_.callback_entries)
    throw ResourceLimit("movement callback registration budget exceeded");
  for (const auto &binding : callbacks)
    if (binding.first.size() > creature_hp::MAX_CALLBACK_NAME)
      throw ResourceLimit("movement callback name budget exceeded");
  const auto found = callbacks.find(name);
  Value result;
  if (found != callbacks.end() && found->second) {
    auto callback = found->second;
    result = callback(*this, entry.args);
  } else if (policy == CallbackPolicy::ReferenceZero) {
    result = default_result(name, entry.args);
  } else if (policy == CallbackPolicy::RequireHandlers) {
    throw MissingHandler("missing movement callback: " + name);
  } else {
    throw Error("unknown movement callback policy");
  }
  validate_values({result});
  return result;
}
std::uint32_t Model::obj_rand(Address obj) {
  span(obj);
  Depth guard(depth_, limits_.depth);
  const auto seed = u32(obj + 0x44) * UINT32_C(0x19660d) + UINT32_C(0x3c6ef35f);
  write(obj + 0x44, 4, seed);
  return seed >> 16;
}
Value Model::vcall(Address obj, std::uint32_t slot, std::vector<Value> args) {
  span(obj);
  Depth guard(depth_, limits_.depth);
  return vcall_body(obj, slot, std::move(args));
}
Value Model::vcall_body(Address obj, std::uint32_t slot,
                        std::vector<Value> args) {
  if (virtual_dispatch) {
    auto dispatch = virtual_dispatch;
    return dispatch(obj, slot, std::move(args));
  }
  if (slot != 6 && slot != 10 && slot != 11)
    throw Error("unsupported movement virtual slot");
  validate_values(args);
  if (args.size() != 1 || (slot == 6 && args[0].kind != ValueKind::Integer) ||
      (slot == 10 && args[0].kind != ValueKind::Vector) ||
      (slot == 11 && args[0].kind != ValueKind::Tuple))
    throw Error("invalid movement virtual arguments");
  if (external.size() > 3)
    throw ResourceLimit("movement external override count exceeded");
  for (const auto &name : external)
    if (name != "Creature.v6" && name != "Creature.v10" &&
        name != "Creature.v11")
      throw Error("unsupported movement external override");
  const Address table = u32(obj + 0x48);
  const auto adjustment =
      rta::s16(std::uint32_t(mem.load(table + 8 * slot, 2)));
  const auto self =
      std::uint32_t(obj + std::uint64_t(std::int64_t(adjustment)));
  const auto function = u32(table + 8 * slot + 4);
  const auto name = "Creature.v" + std::to_string(slot);
  const auto base = slot == 6 ? 0x11a418u : slot == 10 ? 0x11a3f0u : 0x11a448u;
  if (function == base && !external.count(name)) {
    if (slot == 6 && (u32(address(args[0]) + 0xc) & 0x40))
      write(Address(self) + 0xa8, 8,
            mem.load(Address(self) + 0xa8, 8) | UINT64_C(0x800000000000000));
    return {};
  }
  args.insert(args.begin(), i(self));
  return call(name, std::move(args));
}
void Model::water(Address c) {
  span(c);
  Depth guard(depth_, limits_.depth);
  water_body(c);
}
void Model::water_body(Address c) {
  const auto cf = mem.load(c + 0xa8, 8);
  if (cf & 0x100000) {
    write(c + 0xa8, 8, cf & UINT64_C(0xdfffffffffffffff));
    return;
  }
  if (cf & UINT64_C(0x2000000000000000)) {
    bool wet = true;
    if (!lt(u32(c + 0xb8), 0x3c23d70a) || !(u32(G_SIM_FRAME) & 0xf))
      wet = integer(call("water_disturb",
                         {i(u32(G_WORLD)), Value::vector(vec(c + 0x10)),
                          i(0x43960000), i(0x41400000)})) != 0;
    if (!wet)
      write(c + 0xa8, 8, mem.load(c + 0xa8, 8) & UINT64_C(0xdfffffffffffffff));
    return;
  }
  // Only the low three bits of the signed byte enter this mask.
  if ((u32(G_SIM_FRAME) ^ std::uint32_t(mem.load(c + 0x230, 1))) & 7)
    return;
  if (!integer(
          call("water_test", {i(u32(G_WORLD)), Value::vector(vec(c + 0x10)),
                              i(0), i(0x41400000)})))
    return;
  write(c + 0xa8, 8, mem.load(c + 0xa8, 8) | UINT64_C(0x2000000000000000));
  const auto seed = u32(c + 0x44) * UINT32_C(0x19660d) + UINT32_C(0x3c6ef35f);
  write(c + 0x44, 4, seed); // Object generator; no shared draw count.
  const auto text = call("libc_sprintf", {i(S_SPLASH), i(1)});
  const auto entry = call("res_find", {i(S_FX_LUMP), text});
  call("sound_playOnObject", {entry, i(c), i(0x3f199999)});
  call("water_disturb", {i(u32(G_WORLD)), Value::vector(vec(c + 0x10)),
                         i(0x44bb8000), i(0x41c00000)});
}
std::int16_t Model::turn_toward(Address c, std::int32_t mode, Vec3 point) {
  span(c);
  Depth guard(depth_, limits_.depth);
  const auto limit = to_int(mul(u32(c + 0xb0), 0x44888888));
  const auto dy = sub(point[1], u32(c + 0x14));
  const auto dx = sub(point[0], u32(c + 0x10));
  auto target = atan2_16(dy, dx);
  if (mode == 1)
    target += 0x8000;
  std::int64_t facing = mem.load(c + 0x38, 2);
  const auto d = s16(std::uint32_t(std::int64_t(target) - facing));
  std::int64_t av = std::int64_t(mem.load(c + 0x110, 2)) +
                    to_int(mul(from_int(std::uint32_t(d)), 0x3d4ccccc));
  av -= to_int(
      mul(sqrt(0x3e4ccccc), from_int(std::uint32_t(s16(std::uint32_t(av))))));
  write(c + 0x110, 2, std::uint64_t(av));
  const auto negative_limit = -std::int64_t(limit);
  if (limit < s16(std::uint32_t(av)))
    write(c + 0x110, 2, std::uint64_t(limit));
  else if (s16(std::uint32_t(av)) < s32(std::uint32_t(negative_limit)))
    write(c + 0x110, 2, std::uint64_t(negative_limit));
  const auto slow = mem.load(c + 0x2fc, 1);
  if (slow >= 1 && slow <= 60) {
    const auto raw_slow = std::int32_t(mem.load(c + 0x2fc, 1));
    const auto signed_slow = raw_slow < 128 ? raw_slow : raw_slow - 256;
    const auto k = mul(from_int(std::uint32_t(60 - signed_slow)), DT);
    const auto stored_av = s16(std::uint32_t(mem.load(c + 0x110, 2)));
    facing += to_int(mul(from_int(std::uint32_t(stored_av)), k));
  } else {
    facing += mem.load(c + 0x110, 2);
  }
  write(c + 0x38, 2, std::uint64_t(facing));
  return std::int16_t(d);
}
std::uint32_t Model::move_by(Address c, Vec3 velocity) {
  span(c);
  Depth guard(depth_, limits_.depth);
  return move_body(c, velocity);
}
std::uint32_t Model::move_body(Address c, Vec3 velocity) {
  if (mem.load(c + 0x231, 1) & 8)
    return 0;
  const auto old = vec(c + 0x10);
  auto vx = add(velocity[0], mul(u32(c + 0xbc), F60));
  auto vy = add(velocity[1], mul(u32(c + 0xc0), F60));
  auto vz = velocity[2];
  write(c + 0x38, 2, mem.load(c + 0x38, 2) + mem.load(c + 0xc4, 2));
  if (mem.load(c + 0x314, 2))
    vx = vy = vz = 0;
  write(c + 0x308, 4, vx);
  write(c + 0x30c, 4, vy);
  write(c + 0xbc, 8, 0);
  write(c + 0xc4, 2, 0);
  water_body(c);
  const Vec3 dt{mul(vx, DT), mul(vy, DT), mul(vz, DT)};
  std::uint32_t bits = 0;
  Vec3 normal{};
  if (lt(EPS, add(mul(dt[0], dt[0]), mul(dt[1], dt[1])))) {
    const auto result = call("world_slideMove", {i(c + 0x10), Value::vector(dt),
                                                 i(3), i(1), i(u32(c + 0x1c))});
    if (result.kind != ValueKind::Tuple || result.items.size() != 2)
      throw Error("slide callback requires a two-element tuple result");
    const auto hit = integer(result.items[0]);
    if (result.items[1].kind == ValueKind::Vector)
      normal = result.items[1].vec;
    else if (hit || result.items[1].kind != ValueKind::None)
      throw Error("slide callback requires a vector normal when hit");
    // The published terrain model leaves normal absent when no sweep wrote
    // one. Creature_moveBy never consumes it unless hit is nonzero; retain
    // that absence in the callback observation without inventing a normal.
    call("obj_updateGridCell", {i(c)});
    bits = hit ? 1 : 0;
  }
  const auto r = u32(c + 0x1c);
  const auto p = vec(c + 0x10);
  const Vec3 low{sub(p[0], r), sub(p[1], r), sub(p[2], 0x447a0000)};
  const Vec3 high{add(p[0], r), add(p[1], r), add(p[2], 0x447a0000)};
  const auto found = call("world_collectObjectsInBox",
                          {Value::vector(low), Value::vector(high), i(16)});
  if (found.kind != ValueKind::List)
    throw Error("collect callback requires list result");
  const auto top = add(u32(c + 0x18), u32(c + 0x20));
  Raw stand = 0xc7c35000;
  std::vector<Value> touched;
  auto radius = u32(c + 0x1c);
  for (const auto &value : found.items) {
    const auto other = address(value);
    const auto oflags = u32(other + 0xc);
    if ((oflags & 1) || other == c)
      continue;
    const auto dx = sub(u32(c + 0x10), u32(other + 0x10));
    const auto dy = sub(u32(c + 0x14), u32(other + 0x14));
    const auto oradius = u32(other + 0x1c);
    const auto dist = rta::sqrt(add(mul(dx, dx), mul(dy, dy)));
    if (!lt(add(dist, EPS), add(radius, oradius)))
      continue;
    const auto cflags = u32(c + 0xc);
    bool push = true;
    if ((oflags | cflags) & 0x10) {
      const auto oz = u32(other + 0x18);
      if (lt(top, add(oz, 0x40400000)))
        continue;
      const auto otop = add(oz, u32(other + 0x20));
      if (lt(otop, u32(c + 0x18)))
        continue;
      if (lt(sub(otop, u32(c + 0x18)), 0x40a00000)) {
        if (oflags & 0x200)
          push = false;
        else if (!(cflags & 0x400)) {
          if (lt(stand, otop))
            stand = otop;
          continue;
        }
      }
    }
    if (push && !(oflags & 0x200) && !(cflags & 0x401) &&
        lt(EPS, rta::fabs(dist))) {
      const auto k = rta::div(sub(add(radius, oradius), dist), dist);
      call("world_slideMove",
           {i(c + 0x10), Value::vector({mul(dx, k), mul(dy, k), 0}), i(3), i(0),
            i(radius)});
      call("obj_updateGridCell", {i(c)});
    }
    bits |= 2;
    vcall_body(other, 6, {i(c)});
    touched.push_back(i(other));
    if ((u32(c + 0xc) & 0x8000000) && u32(c + 0x234)) {
      const Address node = u32(c + 0x234);
      if (u32(node + 0xc) == other) {
        radius = u32(c + 0x1c);
        break;
      }
    }
    radius = u32(c + 0x1c);
  }
  auto ground = Raw(integer(
      call("terrain_heightBelow",
           {i(u32(G_WORLD)), Value::vector(vec(c + 0x10)), i(radius)})));
  if (lt(ground, stand))
    ground = stand;
  const auto fall = sub(u32(c + 0x30), 0x410fffff);
  write(c + 0x30, 4, fall);
  const auto z = add(u32(c + 0x18), mul(fall, DT));
  write(c + 0x18, 4, z);
  if (le(sub(z, 0x3c23d70a), ground)) {
    write(c + 0x18, 4, ground);
    if (lt(fall, 0)) {
      write(c + 0x30, 4, 0);
      bits |= 4;
    }
  }
  if (bits & 1)
    vcall_body(c, 10, {Value::vector(normal)});
  if (bits & 2) {
    if (u32(c + 0xc) & 0x8000000) {
      wvec(c + 0x10, old);
      call("obj_updateGridCell", {i(c)});
    }
    vcall_body(c, 11, {Value::tuple(std::move(touched))});
  }
  return bits;
}
void Model::settle(Address c) {
  span(c);
  Depth guard(depth_, limits_.depth);
  write(c + 0xc, 4, u32(c + 0xc) | 0x8000000);
  move_body(c, {});
  write(c + 0xc, 4, u32(c + 0xc) & 0xf7ffffff);
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
} // namespace champions::simulation::rta::creature_movement
