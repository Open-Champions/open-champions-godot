// Authored from ref_skills_c.py at 356add27d50e50972b2a671b31e8f4ec93e6778c.
// SHA256 9e673475a0b89292e483075d76b2f277d1251ce820a0956053e68c83faa81e91.
#include "champions/skills_c.hpp"
#include <array>
#include "champions/core_math.hpp"

namespace champions::simulation::rta::skills_c {
namespace {
constexpr Address seed_address = 0x404464;
constexpr Raw percent = 0x3c23d70a, spread20 = 0x3e4ccccc, dt = 0x3c888888;
Address indexed(Address base, unsigned offset, unsigned stride,
                std::uint32_t index) {
  if (base > UINT32_MAX)
    throw Unsupported("address outside raw32 domain");
  const auto value =
      std::int64_t(base) + offset + std::int64_t(stride) * s32(index);
  if (value < 0 || value > UINT32_MAX)
    throw Unsupported("indexed address outside raw32 domain");
  return static_cast<Address>(value);
}
constexpr std::array<std::pair<unsigned, std::uint32_t>, 10> bodies{
    {{1, 0x3c8218},
     {2, 0x3c8220},
     {5, 0x3c82a8},
     {8, 0x3c82e4},
     {10, 0x3c85fc},
     {11, 0x3c8690},
     {12, 0x3c87f0},
     {13, 0x3c8864},
     {14, 0x3c88dc},
     {15, 0x3c88e4}}};
Operation slot_operation(unsigned slot) {
  switch (slot) {
  case 1:
    return Operation::CanUseDefault;
  case 2:
    return Operation::CastDefault;
  case 5:
    return Operation::ManaDefault;
  case 8:
    return Operation::Range;
  case 10:
    return Operation::Stat;
  case 11:
    return Operation::Tier2;
  case 12:
    return Operation::ForCaster;
  case 13:
    return Operation::ForHandle;
  case 14:
    return Operation::Adjust;
  case 15:
    return Operation::ManaFromStat4;
  default:
    throw Unsupported("unported slot");
  }
}
} // namespace
std::uint32_t Model::u32(Address a) const {
  return static_cast<std::uint32_t>(mem.load(a, 4));
}
std::int32_t Model::half(Address a) const {
  return s16(static_cast<std::uint32_t>(mem.load(a, 2)));
}
std::uint32_t Model::tier(Address skill, unsigned first, unsigned base,
                          unsigned step, std::uint32_t stat,
                          std::int32_t level) {
  const auto start = half(indexed(skill, first, 2, stat));
  if (level < start)
    return 0;
  const auto delta = static_cast<std::uint32_t>(std::int64_t(level) - start);
  return static_cast<std::uint32_t>(round_half_away(add(
      from_int(static_cast<std::uint32_t>(half(indexed(skill, base, 2, stat)))),
      mul(u32(indexed(skill, step, 4, stat)), from_int(delta)))));
}
std::uint32_t Model::draw() {
  const auto seed = u32(seed_address) * 0x19660du + 0x3c6ef35fu;
  if (writes.size() >= 65536)
    throw Unsupported("write history limit");
  writes.push_back({static_cast<std::uint32_t>(seed_address), 4, seed});
  mem.store(seed_address, 4, seed);
  ++draws;
  return seed >> 16;
}
std::vector<std::uint32_t> Model::invoke(Address skill, unsigned slot,
                                         std::vector<std::uint32_t> args) {
  if (skill > UINT32_MAX)
    throw Unsupported("skill address outside raw32 domain");
  if (slot > 15)
    throw Unsupported("slot outside supported domain");
  const auto table = u32(skill + 0x60);
  const auto adjusted =
      static_cast<std::uint32_t>(skill + half(Address(table) + 8 * slot));
  const auto body = u32(Address(table) + 8 * slot + 4);
  for (const auto &entry : bodies) {
    if (entry.second == body && !external.count(slot)) {
      args.insert(args.begin(), adjusted);
      return execute(slot_operation(entry.first), args);
    }
  }
  if (calls.size() >= 65536)
    throw Unsupported("call history limit");
  args.insert(args.begin(), adjusted);
  calls.push_back({slot, args});
  auto found = callbacks.find(slot);
  if (found == callbacks.end())
    throw Unsupported("missing Skill.v handler");
  auto function = found->second;
  return function(*this, args);
}
std::vector<std::uint32_t>
Model::skill_call(Address skill, unsigned slot,
                  const std::vector<std::uint32_t> &args) {
  if (depth_ >= 64)
    throw Unsupported("callback recursion limit");
  ++depth_;
  struct Guard {
    std::size_t &n;
    ~Guard() { --n; }
  } guard{depth_};
  return invoke(skill, slot, args);
}
std::uint32_t Model::scalar(Address s, unsigned slot,
                            std::vector<std::uint32_t> args) {
  const auto values = skill_call(s, slot, args);
  if (values.size() != 1)
    throw Unsupported("scalar callback result required");
  return values[0];
}
std::uint32_t Model::rage(Address skill, std::uint32_t rank, Address record,
                          std::uint32_t frames, unsigned stat, bool flash) {
  const auto level = scalar(skill, 14, {rank, 0x14});
  if (!le(div(u32(record + 0xac), u32(record + 0xb0)), HALF))
    return ONE;
  auto ramp = mul(from_int(frames), dt);
  if (lt(ONE, ramp))
    ramp = ONE;
  const auto object = u32(record + 0x43d8);
  if (flash && object) {
    const std::array<std::pair<unsigned, std::uint32_t>, 8> values{
        {{0x9e, 0},
         {0x92, 0},
         {0x94, 0},
         {0x98, static_cast<std::uint32_t>(to_int(mul(ramp, 0x42800000)))},
         {0x9c, static_cast<std::uint32_t>(to_int(mul(ramp, 0x41800000)))},
         {0x90, static_cast<std::uint32_t>(to_int(mul(ramp, 0x42c00000)))},
         {0x96, static_cast<std::uint32_t>(to_int(mul(ramp, 0x41700000)))},
         {0x9a, static_cast<std::uint32_t>(to_int(mul(ramp, 0x41800000)))}}};
    for (const auto &v : values) {
      if (writes.size() >= 65536)
        throw Unsupported("write history limit");
      const auto address = Address(object) + v.first;
      // Preflight the raw span before narrowing the observation address.
      mem.load(address, 2);
      writes.push_back(
          {static_cast<std::uint32_t>(address), 2, v.second & 0xffff});
      mem.store(address, 2, v.second);
    }
  }
  return add(
      mul(mul(from_int(scalar(skill, 10, {stat, level})), percent), ramp), ONE);
}
// All stores retain the reference's ordering and width, including exhausted
// handle-pool writes through address zero. No invented allocation fallback.
void Model::write(Address a, unsigned width, std::uint32_t value) {
  if (writes.size() >= 65536) throw Unsupported("write history limit");
  mem.load(a, width);
  const auto masked = width == 1 ? value & 0xffu : width == 2 ? value & 0xffffu : value;
  writes.push_back({static_cast<std::uint32_t>(a), width, masked});
  mem.store(a, width, value);
}
std::uint32_t Model::own_draw(Address a) {
  const auto seed = u32(a) * 0x19660du + 0x3c6ef35fu;
  write(a, 4, seed);
  return seed >> 16;
}
std::uint32_t Model::unit(Address a) {
  return sub(mul(mul(from_int(own_draw(a)), TWO_M16), TWO), ONE);
}
std::vector<std::uint32_t> Model::ball(Address a) {
  // The write-history limit bounds rejection on pathological host input.
  for (;;) {
    const auto x = unit(a), y = unit(a), z = unit(a);
    if (!lt(ONE, add(add(mul(x,x), mul(y,y)), mul(z,z)))) return {x,y,z};
  }
}
std::uint32_t Model::handle_take() {
  const auto node = u32(0x864ea8);
  if (node) {
    write(0x864ea8, 4, u32(node));
    write(Address(node)+0x10, 2, 0);
  }
  return node;
}
void Model::handle_unlink(Address node) {
  if (u32(node+4)) write(u32(node+4), 4, u32(node));
  if (u32(node)) write(Address(u32(node))+4, 4, u32(node+4));
  const Address partner = u32(node+8);
  if (partner) {
    if (u32(partner+4)) write(u32(partner+4), 4, u32(partner));
    if (u32(partner)) write(Address(u32(partner))+4, 4, u32(partner+4));
    write(partner, 4, u32(0x864ea8));
    write(0x864ea8, 4, static_cast<std::uint32_t>(partner));
  }
  write(node, 4, u32(0x864ea8));
  write(0x864ea8, 4, static_cast<std::uint32_t>(node));
}
std::vector<std::uint32_t> Model::execute(Operation op,
                                          const std::vector<std::uint32_t> &a) {
  const auto n = static_cast<unsigned>(op);
  constexpr std::array<unsigned, 30> sizes{
      {3, 4, 5, 5, 3, 3, 5, 2, 5, 2, 2, 4, 4, 4, 2, 2, 2, 2, 5, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1}};
  if (n >= sizes.size() || a.size() != sizes[n])
    throw Unsupported("operation/argument shape not supported");
  if (depth_ >= 64)
    throw Unsupported("operation recursion limit");
  ++depth_;
  struct Guard {
    std::size_t &n;
    ~Guard() { --n; }
  } guard{depth_};
  const Address s = a[0];
  switch (op) {
  case Operation::SpellBase:
    write(s, 1, a[1]);
    write(s+8, 4, a[3]);
    write(s+0xc, 4, a[2]);
    write(s+0x14, 4, 0x515ba0);
    write(s+4, 4, 0);
    write(s+0x10, 4, draw());
    write(s+0x14, 4, a[4]);
    return {};
  case Operation::SpellRand: return {own_draw(s+0x10)};
  case Operation::ObjectRand: return {own_draw(s+0x44)};
  case Operation::SpellUnit: return {unit(s+0x10)};
  case Operation::ObjectUnit: return {unit(s+0x44)};
  case Operation::SpellBall: return ball(s+0x10);
  case Operation::ObjectBall: return ball(s+0x44);
  case Operation::VisualPosition: {
    auto x = u32(s+0x10), y = u32(s+0x14);
    const auto offset = u32(s+0xe0);
    if (!eq(offset, ZERO)) {
      const auto angle = static_cast<std::uint32_t>(half(s+0x38));
      x = add(x, mul(cos16(angle), offset));
      y = add(y, mul(sin16(angle), offset));
    }
    return {x,y,u32(s+0x3c)};
  }
  case Operation::HandleLink: {
    const Address front = handle_take(), back = handle_take();
    write(front, 4, u32(s));
    if (u32(s)) write(Address(u32(s))+4, 4, static_cast<std::uint32_t>(front));
    write(front+4, 4, a[0]);
    write(s, 4, static_cast<std::uint32_t>(front));
    write(back, 4, u32(a[1]));
    if (u32(a[1])) write(Address(u32(a[1]))+4, 4, static_cast<std::uint32_t>(back));
    write(back+4, 4, a[1]);
    write(a[1], 4, static_cast<std::uint32_t>(back));
    write(front+0xc, 4, a[2]);
    write(front+8, 4, static_cast<std::uint32_t>(back));
    write(back+0xc, 4, 0);
    write(back+8, 4, static_cast<std::uint32_t>(front));
    return {static_cast<std::uint32_t>(front)};
  }
  case Operation::HandleRemove: {
    const auto object = u32(s+0xc);
    handle_unlink(s);
    return {object};
  }
  case Operation::HandleClear: {
    auto node = u32(s);
    while (node) {
      const auto next = u32(node);
      handle_unlink(node);
      node = next;
    }
    return {};
  }
  case Operation::HandleItems: {
    std::vector<std::uint32_t> result;
    std::set<std::uint32_t> visited;
    auto node = u32(s);
    while (node) {
      if (!visited.insert(node).second || visited.size() > 65536)
        throw Unsupported("handle traversal cycle/limit");
      result.push_back(node);
      result.push_back(u32(Address(node)+0xc));
      node = u32(node);
    }
    return result;
  }
  case Operation::Stat:
    return {a[2] == 0 ? 0 : tier(s, 0x0e, 0x18, 0x24, a[1], s32(a[2]))};
  case Operation::Tier2: {
    auto level = s32(a[2]);
    const auto cap = s32(a[3]);
    if (!level)
      return {0};
    if (cap > 0 && cap < level)
      level = cap;
    return {half(indexed(s, 0x38, 2, a[1])) < 0
                ? tier(s, 0x0e, 0x18, 0x24, a[1], level)
                : tier(s, 0x38, 0x42, 0x4c, a[1], level)};
  }
  case Operation::ForCaster:
  case Operation::ForHandle: {
    bool player = false;
    if (op == Operation::ForCaster)
      player = a[3] && (u32(Address(a[3]) + 0xc) & 0x40);
    else {
      const auto node = u32(a[3]);
      player = node && (u32(Address(u32(Address(node) + 0xc)) + 0xc) & 0x40);
    }
    return player ? skill_call(s, 10, {a[1], a[2]})
                  : skill_call(s, 11, {a[1], a[2], a[4]});
  }
  case Operation::Adjust:
    return {a[1]};
  case Operation::ManaFromStat4: {
    const auto level = scalar(s, 14, {a[1], a[2]});
    return {from_int(scalar(s, 10, {4, level}))};
  }
  case Operation::Range: {
    const auto value = from_int(scalar(s, 10, {a[1], a[2]}));
    std::vector<std::uint32_t> out;
    for (unsigned side = 0; side < 2; ++side) {
      auto combine = [side](Raw x, Raw y) {
        return side ? add(x, y) : sub(x, y);
      };
      std::int32_t bound;
      if (a[3]) {
        auto sample = [&]() {
          return combine(value,
                         mul(mul(value, mul(from_int(draw()), TWO_M16)), a[4]));
        };
        const auto probe = sample();
        const auto second = sample();
        bound = to_int(le(ZERO, probe) ? add(second, HALF) : sub(second, HALF));
      } else
        bound = round_half_away(combine(value, mul(value, a[4])));
      out.push_back(bound > 0 ? static_cast<std::uint32_t>(bound) : 1);
    }
    return out;
  }
  case Operation::CanUseDefault:
    return {1};
  case Operation::CastDefault:
    return {0};
  case Operation::ManaDefault:
    return {ONE};
  case Operation::ManaCost:
    return skill_call(s, 15, {a[1], 0x14});
  case Operation::StatisticSpread: {
    if (a[3] && (u32(Address(a[3]) + 0xc) & 0x40))
      return skill_call(s, 8, {a[1], a[2], 0, spread20});
    const auto value = from_int(scalar(s, 12, {a[1], a[2], a[3], 0}));
    const auto delta = mul(value, spread20);
    const auto high = to_int(add(value, delta));
    const auto low = to_int(sub(value, delta));
    return {static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(high)};
  }
  case Operation::RageSwing:
  case Operation::RageDamage:
    return {rage(s, a[1], a[2], a[3], op == Operation::RageSwing ? 0 : 1,
                 op == Operation::RageSwing)};
  case Operation::Complement:
  case Operation::Throwing:
  case Operation::BattleMage: {
    const auto level =
        scalar(s, 14, {a[1], op == Operation::BattleMage ? 1u : 2u});
    const auto fraction = mul(from_int(scalar(s, 10, {0, level})), percent);
    if (op != Operation::Complement)
      return {fraction};
    const auto left = sub(ONE, fraction);
    return {lt(left, ZERO) ? ZERO : left};
  }
  case Operation::PenetratingArrows: {
    const auto first = mul(from_int(scalar(s, 10, {0, a[1]})), percent);
    const auto second = mul(from_int(scalar(s, 10, {1, a[1]})), percent);
    return {first, second};
  }
  }
  throw Unsupported("unported operation");
}
void Model::cast_or_run(std::uint32_t) {
  throw Unsupported("active skill casts and lifecycles are not ported");
}
} // namespace champions::simulation::rta::skills_c
