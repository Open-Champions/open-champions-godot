#include "champions/skills_b.hpp"
#include <algorithm>
#include <limits>
#include <utility>

namespace champions::simulation::rta::skills_b {
namespace {
struct ManaRule { unsigned id; Raw body; unsigned slot; Raw n; };
constexpr ManaRule mana_rules[] = {
 {35,0x2c8c80,15,20},{36,0x2cf42c,15,1},{37,0x2c3f28,15,10},
 {38,0x2c7478,15,4},{39,0x2d0120,15,4},{40,0x2d0c8c,15,10},
 {41,0x2d1794,15,20},{42,0x2d2950,15,18},{43,0x2d6204,15,20},
 {44,0x2d6f80,15,20},{45,0x2d8a30,15,1},{46,0x2da130,15,20},
 {47,0x2dae80,15,9},{48,0x2dbc78,15,20},{49,0x2de60c,15,19},
 {50,0x2e0354,15,20},{51,0x2e0980,15,10},{52,0x2e6894,15,20},
 {53,0x2e7690,15,20},{54,0x2e82c8,15,20},{55,0x2e8e94,15,14},
 {56,0x2e9ad4,15,1},{57,0x2ed80c,15,20},{58,0x2edd44,15,20},
 {59,0x2f8e40,14,1},{62,0x3084e4,14,20},{65,0x30909c,15,10},
 {66,0x309cd8,15,20},{67,0x311604,15,20},{68,0x3126d8,15,1}
};
Address offset(Address base, Address displacement) {
  if (base > UINT32_MAX || displacement > UINT32_MAX - base)
    throw Unsupported("address offset exceeds raw32 memory domain");
  return base + displacement;
}
void valid_stat(Raw stat) {
  if (stat > 4) throw Unsupported("only statistics 0..4 are supported");
}
void arity(const std::vector<Raw> &a, std::size_t n) {
  if (a.size() != n) throw Unsupported("invalid virtual argument count");
}
Raw word(const Memory &m, Address a) { return static_cast<Raw>(m.load(a, 4)); }
Raw round(Raw v, bool negative) {
  return static_cast<Raw>(to_int(negative ? sub(v, HALF) : add(v, HALF)));
}
}
Raw Model::invoke(const std::string &name, std::vector<Raw> args) {
  if (calls.size() >= creature_hp::MAX_HISTORY || depth_ >= 64)
    throw Unsupported("callback history or recursion limit");
  // The reference canonicalizes addresses in its private stack range.
  for (auto &a : args)
    if (a >= 0x1e00000 && a < 0x1e10000)
      throw Unsupported("reference stack-token callback arguments unsupported");
  calls.push_back({name, args});
  auto it = callbacks.find(name);
  if (it == callbacks.end()) throw Unsupported("missing callback: " + name);
  auto fn = it->second;
  ++depth_;
  try { auto result = fn(*this, args); --depth_; return result; }
  catch (...) { --depth_; throw; }
}
Raw Model::skill_call(Address skill, unsigned slot, const std::vector<Raw> &a) {
  if (slot > 15 || depth_ >= 64) throw Unsupported("virtual slot or recursion limit");
  struct DepthGuard {
    unsigned &depth;
    explicit DepthGuard(unsigned &d) : depth(d) { ++depth; }
    ~DepthGuard() { --depth; }
  } guard(depth_);
  auto table = word(mem, offset(skill, 0x60));
  auto adjustment = s16(static_cast<Raw>(mem.load(Address(table) + 8 * slot, 2)));
  Raw self = static_cast<Raw>(skill + static_cast<std::int64_t>(adjustment));
  Raw body = word(mem, Address(table) + 8 * slot + 4);
  const auto name = "Skill.v" + std::to_string(slot);
  if (!external.count(name)) {
    for (const auto &rule : mana_rules)
      if (body == rule.body) { arity(a,1); return mana_cost(self,rule.id,a[0]); }
    switch (body) {
    case 0x3c8218: arity(a, 1); return can_use_base();
    case 0x3c8220: arity(a, 4); return cast_base();
    case 0x3c82a8: arity(a, 1); return mana_cost_base();
    case 0x3c85fc: arity(a, 2); return get_stat(self, a[0], a[1]);
    case 0x3c8690: arity(a, 3); return get_stat_tier2(self, a[0], a[1], a[2]);
    case 0x3c87f0: arity(a, 4); return get_stat_for(self, a[0], a[1], a[2], a[3]);
    case 0x3c8864: arity(a, 4); return get_stat_for_handle(self, a[0], a[1], a[2], a[3]);
    case 0x3c88dc: arity(a, 2); return adjust_level(a[0], a[1]);
    case 0x3c88e4: arity(a, 2); return mana_from_stat4(self, a[0], a[1]);
    case 0x3c82e4:
      arity(a, 6); stat_range(self, a[0], a[1], a[2], a[3], a[4], a[5]); return 0;
    default: break;
    }
  }
  // The out-of-subset cast callback carries a vector in the Python contract;
  // refusing avoids silently substituting a position pointer for that vector.
  if (slot == 2) throw Unsupported("active cast callback requires vector arguments");
  std::vector<Raw> args{self};
  args.insert(args.end(), a.begin(), a.end());
  auto it = addr_names.find(body);
  return invoke(it == addr_names.end() ? name : it->second, std::move(args));
}
Raw Model::stat_impl(Address skill, unsigned first_off, unsigned base_off,
                     unsigned step_off, Raw stat, Raw level) {
  valid_stat(stat);
  auto first = s16(static_cast<Raw>(mem.load(offset(skill, first_off + 2 * stat), 2)));
  auto lv = s32(level);
  if (first > lv) return 0;
  auto delta = static_cast<std::int64_t>(lv) - first;
  if (delta > std::numeric_limits<std::int32_t>::max())
    throw Unsupported("level delta exceeds core signed32 conversion domain");
  auto base = s16(static_cast<Raw>(mem.load(offset(skill, base_off + 2 * stat), 2)));
  Raw value = add(from_int(static_cast<Raw>(base)),
                  mul(word(mem, offset(skill, step_off + 4 * stat)), from_int(static_cast<Raw>(delta))));
  return round(value, lt(value, ZERO));
}
Raw Model::get_stat(Address skill, Raw stat, Raw level) {
  valid_stat(stat);
  return level == 0 ? 0 : stat_impl(skill, 0xe, 0x18, 0x24, stat, level);
}
Raw Model::get_stat_tier2(Address skill, Raw stat, Raw level, Raw cap) {
  valid_stat(stat);
  if (!level) return 0;
  if (s32(cap) > 0) level = static_cast<Raw>(std::min(s32(level), s32(cap)));
  if (s16(static_cast<Raw>(mem.load(offset(skill, 0x38 + 2 * stat), 2))) < 0)
    return stat_impl(skill, 0xe, 0x18, 0x24, stat, level);
  return stat_impl(skill, 0x38, 0x42, 0x4c, stat, level);
}
Raw Model::get_stat_for(Address skill, Raw stat, Raw level, Raw caster, Raw cap) {
  if (caster && (word(mem, Address(caster) + 0xc) & 0x40))
    return skill_call(skill, 10, {stat, level});
  return skill_call(skill, 11, {stat, level, cap});
}
Raw Model::get_stat_for_handle(Address skill, Raw stat, Raw level, Address handle, Raw cap) {
  auto node = word(mem, handle);
  if (node && (word(mem, Address(word(mem, Address(node) + 0xc)) + 0xc) & 0x40))
    return skill_call(skill, 10, {stat, level});
  return skill_call(skill, 11, {stat, level, cap});
}
Raw Model::mana_cost(Address skill, unsigned skill_id, Raw level) {
  for (const auto &rule : mana_rules) {
    if (rule.id != skill_id) continue;
    auto result = skill_call(skill, rule.slot, {level, rule.n});
    return rule.slot == 15 ? result : ZERO;
  }
  if (skill_id==60 || skill_id==61 || skill_id==63 || skill_id==64) return ONE;
  throw Unsupported("mana cost requires RTA skill id 35..68");
}
Raw Model::mana_from_stat4(Address skill, Raw level, Raw n) {
  auto lv = skill_call(skill, 14, {level, n});
  return from_int(skill_call(skill, 10, {4, lv}));
}
void Model::stat_range(Address skill, Raw stat, Raw level, Raw randomise,
                       Address lo, Address hi, Raw spread) {
  auto value = from_int(skill_call(skill, 10, {stat, level}));
  for (unsigned i = 0; i < 2; ++i) {
    auto op = [i](Raw a, Raw b) { return i ? add(a,b) : sub(a,b); };
    Raw bound;
    bool negative;
    if (!randomise) {
      bound = op(value, mul(value, spread)); negative = lt(bound, ZERO);
    } else {
      auto sample = [&]() { return mul(from_int(draw()), TWO_M16); };
      auto probe = op(value, mul(mul(value, sample()), spread));
      negative = lt(probe, ZERO);
      bound = op(value, mul(mul(value, sample()), spread));
    }
    auto number = round(bound, negative);
    auto out = i ? hi : lo;
    mem.store(out, 4, number);
    if (s32(number) < 1) mem.store(out, 4, 1);
  }
}
Raw Model::archery_fraction(Address skill, Raw level) {
  return mul(from_int(strongstance_stat0(skill, level)), 0x3c23d70a);
}
Raw Model::lifetap_stat1(Address skill, Raw level) {
  auto lv = skill_call(skill, 14, {level, 1});
  return skill_call(skill, 10, {1, lv});
}
Raw Model::strongstance_stat0(Address skill, Raw level) {
  auto lv = skill_call(skill, 14, {level, 20});
  return skill_call(skill, 10, {0, lv});
}
Raw Model::lifetap_fraction(Address skill, Raw level) {
  auto lv = skill_call(skill, 14, {level, 1});
  return mul(from_int(skill_call(skill, 10, {0, lv})), 0x3c23d70a);
}
Raw Model::naturalresistance_fraction(Address skill, Raw level) {
  return lifetap_fraction(skill, level);
}
Raw Model::stunningstrike_stat0_for(Address skill, Raw target, Raw level) {
  if (!target || !(word(mem, Address(target) + 0xc) & 0x20) || word(mem, G_PVP_MODE_A)) return 0;
  auto chance = skill_call(skill, 10, {0, level});
  auto rank = s32(invoke("creature_bossRank", {target}));
  if (rank >= 2) return 0;
  if (rank <= 0) return chance;
  return s32(chance) >= 6 ? 5 : chance;
}
Raw Model::next(Address address) {
  Raw seed = word(mem, address) * 1664525u + 1013904223u;
  mem.store(address, 4, seed);
  return seed >> 16;
}
Raw Model::draw() { auto v = next(G_RAND_SEED); ++draws; return v; }
Raw Model::spell_rand(Address spell) { return next(offset(spell, 0x10)); }
Raw Model::object_rand(Address object) { return next(offset(object, 0x44)); }
Raw Model::cosmetic_draw() { return next(G_COSMETIC_SEED); }
Raw Model::seed3_draw() { return next(G_SEED3); }
void Model::spell_base_ctor(Address spell, Raw level, Raw owner, Raw skill, Raw table) {
  mem.store(spell, 1, level);
  mem.store(offset(spell, 0x14), 4, 0x515ba0);
  mem.store(offset(spell, 8), 4, skill);
  mem.store(offset(spell, 4), 4, 0);
  mem.store(offset(spell, 0xc), 4, owner);
  // The seed stored here is the draw's upper16 result, not the full LCG state.
  mem.store(offset(spell, 0x10), 4, draw());
  mem.store(offset(spell, 0x14), 4, table);
}

namespace {
constexpr Raw HANDLE_FREE = 0x864ea8;
void walk_step(std::size_t &steps) {
  if (steps++ >= creature_hp::MAX_HISTORY)
    throw Unsupported("list traversal limit");
}
}
Raw Model::node_take() {
  auto node = word(mem, HANDLE_FREE);
  if (node) {
    mem.store(HANDLE_FREE, 4, word(mem, node));
    mem.store(offset(node, 0x10), 2, 0);
  }
  return node;
}
void Model::node_push(Address head, Raw node) {
  auto first = word(mem, head);
  mem.store(node, 4, first);
  if (first) mem.store(offset(first, 4), 4, node);
  mem.store(offset(node, 4), 4, head);
  mem.store(head, 4, node);
}
void Model::node_unlink(Raw node) {
  auto prev = word(mem, offset(node, 4)), next = word(mem, node);
  if (prev) {
    mem.store(prev, 4, next);
    next = word(mem, node);
  }
  if (next) mem.store(offset(next, 4), 4, word(mem, offset(node, 4)));
}
void Model::node_free(Raw node) {
  mem.store(node, 4, word(mem, HANDLE_FREE));
  mem.store(HANDLE_FREE, 4, node);
}
Raw Model::handle_link(Address head, Address back, Raw object) {
  auto node = node_take();
  auto partner = node_take();
  node_push(head, node);
  node_push(back, partner);
  mem.store(offset(node, 0xc), 4, object);
  mem.store(offset(node, 8), 4, partner);
  mem.store(offset(partner, 0xc), 4, 0);
  mem.store(offset(partner, 8), 4, node);
  return node;
}
Raw Model::list_push(Address head, Raw object) {
  auto node = node_take();
  node_push(head, node);
  mem.store(offset(node, 0xc), 4, object);
  mem.store(offset(node, 8), 4, 0);
  return node;
}
void Model::partner_drop(Raw partner) {
  auto prev = word(mem, offset(partner, 4));
  if (prev) mem.store(prev, 4, word(mem, partner));
  auto next = word(mem, partner);
  if (next) mem.store(offset(next, 4), 4, word(mem, offset(partner, 4)));
  node_free(partner);
}
Raw Model::handle_remove_node(Raw node) {
  auto object = word(mem, offset(node, 0xc));
  node_unlink(node);
  auto partner = word(mem, offset(node, 8));
  if (partner) partner_drop(partner);
  node_free(node);
  return object;
}
void Model::handle_clear(Address head) {
  auto node = word(mem, head);
  std::size_t steps = 0;
  while (node) {
    walk_step(steps);
    auto next = word(mem, node);
    node_unlink(node);
    auto partner = word(mem, offset(node, 8));
    if (partner) partner_drop(partner);
    node_free(node);
    node = next;
  }
}
Raw Model::handle_object(Address head) const {
  auto node = word(mem, head);
  return node ? word(mem, offset(node, 0xc)) : 0;
}
std::vector<Raw> Model::list_objects(Address head) const {
  std::vector<Raw> result;
  auto node = word(mem, head);
  std::size_t steps = 0;
  while (node) {
    walk_step(steps);
    result.push_back(word(mem, offset(node, 0xc)));
    node = word(mem, node);
  }
  return result;
}
Raw Model::iter_skip(Raw node, Raw mask) {
  std::size_t steps = 0;
  while (node) {
    walk_step(steps);
    auto object = word(mem, offset(node, 0xc));
    if (object && !(word(mem, offset(object, 0xc)) & mask)) break;
    node = word(mem, node);
  }
  return node;
}
void Model::iter_park(Address iterator, Raw node) {
  auto next = word(mem, node);
  mem.store(offset(iterator, 4), 4, node);
  mem.store(iterator, 4, next);
  if (next) mem.store(offset(next, 4), 4, iterator);
  mem.store(word(mem, offset(iterator, 4)), 4, iterator);
}
void Model::iter_begin(Address iterator, Address head, Raw mask) {
  mem.store(offset(iterator, 0x14), 4, mask);
  auto node = iter_skip(word(mem, head), mask);
  if (node) iter_park(iterator, node);
  else {
    mem.store(iterator, 4, 0);
    mem.store(offset(iterator, 4), 4, 0);
  }
  mem.store(offset(iterator, 0xc), 4, 0);
}
void Model::iter_next(Address iterator) {
  auto first = word(mem, iterator);
  auto node = iter_skip(first, word(mem, offset(iterator, 0x14)));
  auto prev = word(mem, offset(iterator, 4));
  Raw value = 0;
  if (first) {
    mem.store(offset(first, 4), 4, prev);
    value = word(mem, iterator);
    prev = word(mem, offset(iterator, 4));
  }
  mem.store(prev, 4, value);
  if (node) iter_park(iterator, node);
  else {
    mem.store(offset(iterator, 4), 4, 0);
    mem.store(iterator, 4, 0);
  }
}
void Model::iter_end(Address iterator, Raw) {
  if (word(mem, iterator))
    mem.store(offset(word(mem, iterator), 4), 4, word(mem, offset(iterator, 4)));
  if (word(mem, offset(iterator, 4)))
    mem.store(word(mem, offset(iterator, 4)), 4, word(mem, iterator));
}
void Model::creature_add_spell(Address creature, Raw spell) {
  if (spell) handle_link(offset(creature, 0x364), offset(spell, 4), spell);
}
} // namespace champions::simulation::rta::skills_b
