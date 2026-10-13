// MIT; Copyright (c) 2026 TrustlessMike and contributors.
// Authored from ref_skills_a.py at 356add27d50e50972b2a671b31e8f4ec93e6778c,
// SHA256 6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae.
#include "champions/skills_a.hpp"
#include <array>

namespace champions::simulation::rta::skills_a {
namespace {
Address indexed(Raw skill, unsigned offset, unsigned width, Raw stat) {
  const auto address = std::int64_t(skill) + offset + std::int64_t(s32(stat)) * width;
  if (address < 0 || address > UINT32_MAX)
    throw Unsupported("statistic address outside raw32 domain");
  return Address(address);
}
void arity(const Values &a, std::size_t low, std::size_t high) {
  if (a.size() < low || a.size() > high) throw Unsupported("Skill argument count");
}
}
void Model::write(Address a, std::size_t n, Raw v) {
  if (writes.size() >= creature_hp::MAX_HISTORY) throw Unsupported("Skill write history limit");
  v &= n == 4 ? UINT32_MAX : 0xffffu;
  writes.push_back({Raw(a), n, v});
  mem.store(a, n, v);
}
Raw Model::draw() {
  const Raw seed = Raw(mem.load(G_RAND_SEED, 4)) * 0x19660du + 0x3c6ef35fu;
  write(G_RAND_SEED, 4, seed);
  ++draws;
  return seed >> 16;
}
Raw Model::skill_by_id(Raw id) const { return Raw(mem.load(Address(G_SKILL_TABLE) + Address(id) * 4, 4)); }
Raw Model::skill_can_use_base(Raw, Raw) const { return 1; }
Raw Model::skill_cast_base(Raw, Raw, Raw) const { return 0; }
Raw Model::skill_mana_base(Raw, Raw) const { return ONE; }
void Model::skill_init_stats_base(Raw skill) {
  for (unsigned i = 0; i < 5; ++i) {
    write(Address(skill) + 0xe + 2*i, 2, 0);
    write(Address(skill) + 0x24 + 4*i, 4, TWO);
    write(Address(skill) + 0x18 + 2*i, 2, 1);
  }
}
Raw Model::skill_stat(Raw skill, Raw stat, Raw level, Raw) const {
  if (!level) return 0;
  const auto first = s16(Raw(mem.load(indexed(skill, 0xe, 2, stat), 2)));
  if (s32(level) < first) return 0;
  const auto base = s16(Raw(mem.load(indexed(skill, 0x18, 2, stat), 2)));
  const auto step = Raw(mem.load(indexed(skill, 0x24, 4, stat), 4));
  // Python from_int truncates to signed32 even when the subtraction is wider.
  return Raw(round_half_away(add(from_int(Raw(base)), mul(step, from_int(level - Raw(first))))));
}
Raw Model::skill_stat_tier2(Raw skill, Raw stat, Raw level, Raw cap) const {
  if (!level) return 0;
  if (s32(cap) > 0 && s32(cap) < s32(level)) level = cap;
  const auto first = s16(Raw(mem.load(indexed(skill, 0x38, 2, stat), 2)));
  if (first < 0) return skill_stat(skill, stat, level);
  if (s32(level) < first) return 0;
  const auto base = s16(Raw(mem.load(indexed(skill, 0x42, 2, stat), 2)));
  const auto step = Raw(mem.load(indexed(skill, 0x4c, 4, stat), 4));
  return Raw(round_half_away(add(from_int(Raw(base)), mul(step, from_int(level - Raw(first))))));
}
Raw Model::scalar(Raw skill, unsigned slot, Values args) {
  const auto v = skill_call(skill, slot, args);
  if (v.size() != 1) throw Unsupported("Skill scalar result required");
  return v[0];
}
Raw Model::skill_stat_for(Raw skill, Raw stat, Raw level, Raw caster, Raw cap) {
  const bool player = caster && (mem.load(Address(caster) + 0xc, 4) & 0x40);
  return scalar(skill, player ? 10 : 11, {stat, level, cap});
}
Raw Model::skill_stat_for_handle(Raw skill, Raw stat, Raw level, Raw handle, Raw cap) {
  const auto node = Raw(mem.load(handle, 4));
  // An existing node with a null object still reads address 0xc, as the reference.
  const bool player = node && (mem.load(Address(Raw(mem.load(Address(node) + 0xc, 4))) + 0xc, 4) & 0x40);
  return scalar(skill, player ? 10 : 11, {stat, level, cap});
}
Raw Model::skill_adjust_level(Raw, Raw level, Raw) const { return level; }
Raw Model::skill_mana_from_stat4(Raw skill, Raw level, Raw n) {
  level = scalar(skill, 14, {level, n});
  return from_int(scalar(skill, 10, {4, level}));
}
Values Model::skill_stat_range(Raw skill, Raw stat, Raw level, Raw randomise, Raw spread) {
  const auto value = from_int(scalar(skill, 10, {stat, level}));
  Values out;
  for (unsigned i = 0; i < 2; ++i) {
    const auto combine = [&](Raw x) { return i ? add(value, x) : sub(value, x); };
    Raw probe, bound;
    if (randomise) {
      probe = combine(mul(mul(value, mul(from_int(draw()), TWO_M16)), spread));
      bound = combine(mul(mul(value, mul(from_int(draw()), TWO_M16)), spread));
    } else probe = bound = combine(mul(value, spread));
    const auto result = to_int(le(ZERO, probe) ? add(bound, HALF) : sub(bound, HALF));
    out.push_back(result > 0 ? Raw(result) : 1);
  }
  return out;
}
Raw Model::passive_cast(Raw, Raw, Raw) const { return 0; }
Raw Model::dodge_fraction(Raw skill, Raw rank) {
  if (!rank) return ZERO;
  const auto level = scalar(skill, 14, {rank, 1});
  return mul(from_int(scalar(skill, 10, {0, level})), 0x3c23d70a);
}
Raw Model::regeneration_stat0(Raw skill, Raw level) {
  return scalar(skill, 10, {0, level});
}
Raw Model::mana_cost_at(Raw function, Raw skill, Raw level) {
  std::array<Raw, 14> words{};
  for (unsigned i = 0; i < words.size(); ++i) words[i] = Raw(mem.load(Address(function) + 4*i, 4));
  if (words[0] == 0x24a5ffff) {
    std::map<unsigned, Raw> constants;
    Raw value = 0;
    for (unsigned i = 1; i < words.size(); ++i) {
      const auto word = words[i];
      if (word >> 16 == 0x3c01) value = (word & 0xffff) << 16;
      else if (word >> 16 == 0x3421) value |= word & 0xffff;
      else if ((word & 0xffff07ff) == 0x44810000) constants[(word >> 11) & 31] = value;
    }
    if (!constants.count(1) || !constants.count(2)) throw Unsupported("mana constants absent");
    return add(mul(from_int(level - 1), constants.at(2)), constants.at(1));
  }
  if ((words[3] >> 16 == 0x2406 || words[3] == 0x3021 || words[3] == 0x3025 || words[3] == 0x302d) && words[5] >> 16 == 0x2442) {
    const auto n = words[3] >> 16 == 0x2406 ? words[3] & 0xffff : 0;
    const auto slot = (words[5] & 0xffff) / 8;
    if (slot == 15) return scalar(skill, 15, {level, n});
    if (slot == 14) { scalar(skill, 14, {level, n}); return ZERO; }
  }
  throw Unsupported("unrecognised mana-cost function");
}
Values Model::skill_call(Raw skill, unsigned slot, const Values &args) {
  if (slot > 15 || depth_ >= 64) throw Unsupported("Skill slot or recursion limit");
  ++depth_;
  struct Guard { std::size_t &d; ~Guard() { --d; } } guard{depth_};
  const auto table = Raw(mem.load(Address(skill) + 0x60, 4));
  const auto row = Address(table) + 8*slot;
  const auto self = skill + Raw(s16(Raw(mem.load(row, 2))));
  const auto body = Raw(mem.load(row + 4, 4));
  if (!external.count(slot)) {
    const auto a = [&](std::size_t i) { return i < args.size() ? args[i] : 0; };
    switch (body) {
    case 0x3c8218: arity(args, 0, 1); return {skill_can_use_base(self, a(0))};
    case 0x2ddb90:
    case 0x2f1440:
    case 0x3c8220: arity(args, 0, 6); return {skill_cast_base(self, a(0), a(1))};
    case 0x3c82a8: arity(args, 0, 1); return {skill_mana_base(self, a(0))};
    case 0x3c82e4: arity(args, 4, 4); return skill_stat_range(self, a(0), a(1), a(2), a(3));
    case 0x3c85b0: arity(args, 0, 0); skill_init_stats_base(self); return {};
    case 0x3c85fc: arity(args, 2, 3); return {skill_stat(self, a(0), a(1), a(2))};
    case 0x3c8690: arity(args, 3, 3); return {skill_stat_tier2(self, a(0), a(1), a(2))};
    case 0x3c87f0: arity(args, 4, 4); return {skill_stat_for(self, a(0), a(1), a(2), a(3))};
    case 0x3c8864: arity(args, 4, 4); return {skill_stat_for_handle(self, a(0), a(1), a(2), a(3))};
    case 0x3c88dc: arity(args, 1, 2); return {skill_adjust_level(self, a(0), a(1))};
    case 0x3c88e4: arity(args, 2, 2); return {skill_mana_from_stat4(self, a(0), a(1))};
    default: break;
    }
  }
  // Slot 5 always enters the shape reader in the pinned reference, even forced external.
  if (slot == 5) { arity(args, 1, 1); return {mana_cost_at(body, self, args[0])}; }
  if (calls.size() >= creature_hp::MAX_HISTORY) throw Unsupported("Skill callback history limit");
  Values canonical{self}; canonical.insert(canonical.end(), args.begin(), args.end());
  calls.push_back({slot, canonical});
  const auto found = callbacks.find(slot);
  if (found == callbacks.end() || !found->second) throw Unsupported("Skill body needs explicit callback");
  auto callback = found->second;
  return callback(*this, canonical);
}
} // namespace champions::simulation::rta::skills_a
