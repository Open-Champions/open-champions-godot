// RTA water-services contract: research t3/research-anim 2502e2bfac58203ac0174fccd7e0953b36d06439.
// docs/spec/water-services.md SHA256 7747301090f3f09e482c5bf173e0fe3381979999f475d7a275e2a366521dcd58
// tools/ref_water_services.py SHA256 b822e671af336c2f73f75aacf02a3fbd0ab33128a70058f4985ec67599b0bdf1
#include "champions/creature_water.hpp"
#include "champions/core_arithmetic.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace champions::simulation::rta::creature_water {
namespace {
constexpr Raw F12 = 0x41400000, F16 = 0x41800000, F32 = 0x42000000,
              F60 = 0x42700000, QUARTER = 0x3e800000,
              APPROX_TWELFTH = 0x3daaaaaa;
constexpr Address CUT_SKIP = 0x44fcec;
Address checked(std::int64_t address, std::size_t bytes = 1) {
  if (address < 0 || std::uint64_t(address) > UINT32_MAX ||
      bytes > UINT64_C(0x100000000) - std::uint64_t(address))
    throw Unsupported("water address outside raw32 memory");
  return Address(address);
}
Address offset(Address base, std::int64_t delta, std::size_t bytes = 1) {
  if (base > UINT32_MAX)
    throw Unsupported("water address outside raw32 memory");
  return checked(std::int64_t(base) + delta, bytes);
}
Raw threshold(std::uint32_t low, std::uint32_t high) {
  // Two signed halfword loads, bitwise OR, then signed32 shift by two.
  const auto combined = std::uint32_t(s16(low)) | (high << 16);
  return from_int(std::uint32_t(combined << 2));
}
std::int32_t signed_add(std::int32_t a, std::int32_t b) {
  return s32(std::uint32_t(a) + std::uint32_t(b));
}
} // namespace

std::uint64_t Service::read(Address address, std::size_t size) const {
  return memory_.load(checked(std::int64_t(address), size), size);
}
void Service::write(Address address, std::size_t size, std::uint64_t value) {
  checked(std::int64_t(address), size);
  if (writes_.size() >= creature_hp::MAX_HISTORY)
    throw Unsupported("water write history budget");
  if (size < 8)
    value &= (UINT64_C(1) << (size * 8)) - 1;
  writes_.push_back({std::uint32_t(address), size, value});
  memory_.store(address, size, value);
}

std::vector<std::uint32_t> Service::query(Address world, Vec3 lo, Vec3 hi,
                                           std::size_t capacity,
                                           std::optional<Address> out) {
  if (capacity < 2)
    throw Unsupported("water query capacity below two");
  if (out)
    write(*out, 4, 0);
  const auto root = read(offset(world, 4, 4), 4);
  const auto chunks = read(offset(world, 0x24, 4), 4);
  std::vector<Address> stack{root};
  std::vector<std::uint32_t> results;
  std::size_t visited = 0;
  while (!stack.empty()) {
    if (++visited > 65536)
      throw Unsupported("water tree traversal budget");
    const auto node = stack.back();
    stack.pop_back();
    const auto tag = std::uint32_t(read(node, 2));
    if (tag & 0x8000) {
      if (!(tag & 0x4000))
        continue;
      const auto axis = tag & 0xfff;
      if (axis > 2)
        throw Unsupported("water split axis outside 0..2");
      const auto first = threshold(read(offset(node, 2, 2), 2),
                                   read(offset(node, 4, 2), 2));
      const auto second = threshold(read(offset(node, 6, 2), 2),
                                    read(offset(node, 8, 2), 2));
      if (le(lo[axis], first))
        stack.push_back(offset(node, 20));
      if (le(second, hi[axis])) {
        const auto relative = s16(std::uint32_t(read(offset(node, 18, 2), 2)));
        stack.push_back(offset(node, 18 + 2 * std::int64_t(relative)));
      }
      if (stack.size() > 16)
        throw Unsupported("water tree stack exceeds 16 nodes");
      continue;
    }
    for (std::uint32_t index = 0; index < tag; ++index) {
      const auto chunk_index = read(offset(node, 2 + 2 * index, 2), 2);
      const auto chunk = offset(chunks, 0x3c * std::int64_t(chunk_index), 0x3c);
      if (!(read(offset(chunk, 0x24, 4), 4) & 0x80))
        continue;
      bool hit = true;
      for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto minimum = Raw(read(offset(chunk, 8 + 4 * axis, 4), 4));
        const auto maximum = Raw(read(offset(chunk, 0x14 + 4 * axis, 4), 4));
        if (lt(hi[axis], minimum) || lt(maximum, lo[axis])) {
          hit = false;
          break;
        }
      }
      if (!hit)
        continue;
      if (out)
        write(offset(*out, 4 * std::int64_t(results.size()), 4), 4, chunk);
      results.push_back(std::uint32_t(chunk));
      if (results.size() == capacity - 1) {
        if (out)
          write(offset(*out, 4 * std::int64_t(results.size()), 4), 4, 0);
        return results;
      }
    }
  }
  if (out)
    write(offset(*out, 4 * std::int64_t(results.size()), 4), 4, 0);
  return results;
}

std::uint32_t Service::water_test(Raw radius, Address world, Vec3 position,
                                   Address height_out) {
  const Vec3 lo{sub(sub(position[0], radius), ONE),
                sub(sub(position[1], radius), ONE), sub(position[2], ONE)};
  const Vec3 hi{add(add(position[0], radius), ONE),
                add(add(position[1], radius), ONE), add(position[2], F60)};
  const auto chunks = query(world, lo, hi, 2);
  if (chunks.empty())
    return 0;
  if (height_out)
    write(height_out, 4, read(offset(chunks[0], 0x10, 4), 4));
  return 1;
}

std::uint32_t Service::water_disturb(Raw strength, Raw radius, Address world,
                                      Vec3 position) {
  if (read(CUT_SKIP, 1))
    return 0;
  if (!lt(0, radius))
    throw Unsupported("water disturbance radius must be positive");
  const Vec3 lo{sub(position[0], radius), sub(position[1], radius),
                sub(position[2], ONE)};
  const Vec3 hi{add(position[0], radius), add(position[1], radius),
                add(position[2], F60)};
  const auto chunks = query(world, lo, hi, 16);
  for (const auto chunk : chunks) {
    const auto payload = read(chunk, 4);
    const auto grid = read(offset(payload, 8, 4), 4);
    if (!grid)
      continue;
    const Raw width = sub(Raw(read(offset(chunk, 0x14, 4), 4)),
                          Raw(read(offset(chunk, 8, 4), 4)));
    const auto extent = to_int(add(mul(width, APPROX_TWELFTH), HALF));
    if (extent <= 0)
      throw Unsupported("water rounded chunk width must be positive");
    const Raw inv = div(ONE, mul(from_int(std::uint32_t(extent)), F12));
    const auto coord = [&](std::size_t axis) {
      const auto origin = mul(from_int(Raw(read(offset(chunk, 0x28 + 4 * axis, 4), 4))), QUARTER);
      return add(mul(mul(sub(position[axis], origin), F32), inv), F16);
    };
    const Raw gx = coord(0), gy = coord(1);
    const Raw gr = mul(mul(radius, F32), inv);
    const Raw r2 = mul(gr, gr);
    const Raw factor = div(strength, r2);
    const auto bounds = [&](Raw g) {
      return std::array<std::int32_t, 2>{
          std::max(0, to_int(sub(sub(g, radius), ONE))),
          std::min(31, to_int(add(add(g, radius), ONE)))};
    };
    const auto xb = bounds(gx), yb = bounds(gy);
    for (std::int32_t x = xb[0]; x <= xb[1]; ++x) {
      for (std::int32_t y = yb[0]; y <= yb[1]; ++y) {
        const Raw dy = sub(from_int(std::uint32_t(y)), gy);
        const Raw dx = sub(from_int(std::uint32_t(x)), gx);
        const Raw d2 = add(mul(dy, dy), mul(dx, dx));
        if (lt(r2, d2))
          continue;
        const auto amount = to_int(sub(strength, mul(d2, factor)));
        const auto address = offset(grid, 0xa90 + 2 * (32 * x + y), 2);
        const auto old = s16(std::uint32_t(read(address, 2)));
        const auto value = signed_add(amount, old);
        write(address, 2, std::uint32_t(std::min(value, 32767)));
      }
    }
  }
  return std::uint32_t(chunks.size());
}

void install_creature_callbacks(creature_movement::Model &creature,
                                Service &service) {
  if (&creature.mem != &service.memory())
    throw Unsupported("water service must share creature memory");
  creature.callbacks["water_test"] = [&service](creature_movement::Model &,
                                                 const auto &args) {
    if (args.size() != 4 || args[0].kind != creature_movement::ValueKind::Integer ||
        args[1].kind != creature_movement::ValueKind::Vector ||
        args[2].kind != creature_movement::ValueKind::Integer ||
        args[3].kind != creature_movement::ValueKind::Integer)
      throw Unsupported("invalid water_test callback arguments");
    return creature_movement::Value::integer(service.water_test(
        Raw(args[3].scalar), Address(args[0].scalar), args[1].vec,
        Address(args[2].scalar)));
  };
  creature.callbacks["water_disturb"] = [&service](creature_movement::Model &,
                                                    const auto &args) {
    if (args.size() != 4 || args[0].kind != creature_movement::ValueKind::Integer ||
        args[1].kind != creature_movement::ValueKind::Vector ||
        args[2].kind != creature_movement::ValueKind::Integer ||
        args[3].kind != creature_movement::ValueKind::Integer)
      throw Unsupported("invalid water_disturb callback arguments");
    return creature_movement::Value::integer(service.water_disturb(
        Raw(args[2].scalar), Raw(args[3].scalar), Address(args[0].scalar),
        args[1].vec));
  };
}
} // namespace champions::simulation::rta::creature_water
