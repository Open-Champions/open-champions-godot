// Authored from ref_arena_monsters.py at 356add27d50e50972b2a671b31e8f4ec93e6778c
// SHA256 84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16.
#include "champions/arena_monsters.hpp"
#include <utility>
namespace champions::simulation::rta::arena_monsters {
namespace {
struct Depth {
  std::size_t &n;
  Depth(std::size_t &v, std::size_t max) : n(v) {
    if (n >= max) throw creature_hp::ResourceLimit("arena operation depth limit");
    ++n;
  }
  ~Depth() { --n; }
};
std::int64_t integer(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v)) return *p;
  throw Error("arena callback requires integer result");
}
Value i(std::uint64_t v) { return std::int64_t(v & UINT32_MAX); }
}
Model::Model(Memory &m, creature_hp::Limits limits) : limits_(limits), mem(m) {}
Raw Model::u32(Address a) const { return Raw(mem.load(a,4)); }
Vec3 Model::vec(Address a) const { return {u32(a),u32(a+4),u32(a+8)}; }
Raw Model::clip(Address c, std::uint32_t off) const { return u32(Address(u32(c+0x24))+off); }
void Model::write(Address a, std::size_t size, std::uint64_t value) {
  if (a > UINT32_MAX || size > 8 || size > UINT64_C(0x100000000)-a)
    throw creature_hp::ResourceLimit("arena write outside raw32 domain");
  if (writes.size() >= limits_.history_entries || events.size() >= limits_.history_entries)
    throw creature_hp::ResourceLimit("arena history limit");
  if (size < 8) value &= (UINT64_C(1) << (size*8))-1;
  Write w{std::uint32_t(a),size,value}; writes.push_back(w); events.emplace_back(w);
  mem.store(a,size,value);
}
Value Model::call(const std::string &name, std::vector<Value> args) {
  if (callbacks.size() > limits_.callback_entries || calls.size() >= limits_.history_entries ||
      events.size() >= limits_.history_entries)
    throw creature_hp::ResourceLimit("arena callback/history limit");
  for (auto &v : args) if (auto p = std::get_if<std::int64_t>(&v)) *p = std::int64_t(std::uint64_t(*p)&UINT32_MAX);
  Call c{name,std::move(args)}; calls.push_back(c); events.emplace_back(c);
  auto it = callbacks.find(name);
  if (it == callbacks.end()) throw MissingHandler("missing arena handler: " + name);
  auto fn = it->second;
  return fn(*this,c.args);
}
Raw Model::model_slot16(Address c) {
  auto table = u32(c+0x48);
  auto adjusted = (c + std::uint64_t(std::int64_t(s16(mem.load(Address(table)+128,2))))) & UINT32_MAX;
  if (!external_slot16 && u32(Address(table)+132) == 0x3c2738)
    return u32(Address(u32(adjusted+0x24))+0xc);
  // Every other known arena/base body is outside this subset. Do not substitute
  // an external handler for a body the reference would execute internally.
  const auto function = u32(Address(table)+132);
  if (!external_slot16) for (auto known : {0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,
      0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,0x10e658u,0x10beb8u,0x3c2708u})
    if (function == known) throw Unsupported("slot16 redirected to another modeled arena body");
  return Raw(std::uint64_t(integer(call("Creature.v16",{i(adjusted)}))) & UINT32_MAX);
}
FilterResult Model::filter_state(Address c, std::uint32_t requested) {
  Depth guard(depth_,limits_.depth);
  if (c > UINT32_MAX) throw creature_hp::ResourceLimit("arena creature outside raw32 domain");
  auto state = s32(requested);
  if (u32(0x4f7120) && !u32(0x4f711c) && u32(0x503890) && !u32(0x503894)) return {true,state};
  if (mem.load(c+0x231,1) && state == 1) return {true,state};
  if (state == 0x14) for (auto off : {0x31c,0x318,0x314,0x31a}) write(c+off,2,0);
  if (s16(mem.load(c+0x314,2)) && state != 0 && state != 5 && !(6 <= state && state <= 15))
    state = u32(c+0x234) ? 5 : 0;
  else if (s16(mem.load(c+0x31c,2)) && state != 0x13) state = 0x13;
  else {
    if (!u32(c+0x234)) write(c+0x318,2,0);
    if (s16(mem.load(c+0x318,2)) && state != 0x10) state = 0x10;
  }
  if (!state) { call("handle_clear",{i(c+0x234)}); write(c+0x234,4,0); }
  return {false,state};
}
void Model::skelight_shatter(Address, std::uint32_t) {
  Depth guard(depth_,limits_.depth); write(G_SKELIGHT_STATIC,8,0);
}
void Model::skelight_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_,limits_.depth);
  auto filtered = filter_state(c,requested);
  if (filtered.refused) return;
  auto state = filtered.state;
  const auto anim = c+0x114;
  auto idle = [&] { call("anim_start",{i(anim),i(clip(c,0x34)),i(3),i(0x3dcccccc)}); };
  switch (state) {
  case 0: idle(); write(c+0xf4,4,std::uint64_t(u32(c+0x300))+60); break;
  case 1: throw Unsupported("Skelight wander state 1 not ported");
  case 3: case 0x10:
    call("anim_start",{i(anim),i(clip(c,0x44)),i(0x803),i(0x3dcccccc)});
    call("anim_syncToSimFrame",{i(anim)}); break;
  case 5: case 0x13: idle(); break;
  case 6: {
    auto seed = std::uint32_t(std::uint64_t(u32(c+0x44))*0x19660d+0x3c6ef35f);
    write(c+0x44,4,seed);
    call("anim_startBlendInOut",{i(anim),i(clip(c,0x1c+8*((seed>>16)&1))),i(0x808),i(0x3dcccccc),i(0x3dcccccc)});
    write(c+0xf4,4,1); break;
  }
  case 0x12: state = s16(mem.load(c+0x232,2)); break;
  case 0x14: {
    write(c+0xc,4,u32(c+0xc)|2);
    write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x20000000000));
    skelight_shatter(c,8);
    auto count = s32(std::uint64_t(integer(call("model_getSamplePointCount",{i(model_slot16(c))}))));
    for (std::int64_t index = 0; index < count; index += 3) {
      auto result = call("model_getSamplePoint",{i(model_slot16(c)),i(anim),vec(c+0x10),
          i(std::uint64_t(std::int64_t(s16(mem.load(c+0x38,2))))),i(index),i(1),i(0),i(ONE)});
      auto sample = std::get_if<Sample>(&result);
      if (!sample) throw Error("arena sample callback requires two vectors");
      call("fx_spawn",{i(2),sample->point});
    }
    call("obj_delete",{i(c)}); break;
  }
  case 0x17: break;
  default: state = 0; idle(); break;
  }
  write(c+0x232,2,std::uint64_t(std::int64_t(state)));
}
} // namespace champions::simulation::rta::arena_monsters
