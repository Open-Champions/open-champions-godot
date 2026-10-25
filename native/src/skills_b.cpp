#include "champions/skills_b.hpp"
#include "champions/detail/skill_kernel.hpp"
#include "champions/detail/skill_spell_kernel.hpp"
#include "champions/detail/skill_handle_kernel.hpp"
#include <algorithm>
#include <limits>
#include <utility>
#ifdef alloca
#undef alloca
#endif

namespace champions::simulation::rta::skills_b {
namespace kernel = detail::skill_kernel;
namespace spells = detail::skill_spell_kernel;
namespace handles = detail::skill_handle_kernel;
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
Model::Model(Memory initial, std::map<std::string, Callback> services,
             std::set<std::string> delegated)
    : mem(std::move(initial)), callbacks(std::move(services)), external(std::move(delegated)) {}
Raw Model::f32(Address address) const { return word(mem, address); }
std::string Model::cstr(Address address, std::int64_t limit) const {
  if (limit > static_cast<std::int64_t>(creature_hp::MAX_HISTORY))
    throw Unsupported("string read limit exceeds 65536 bytes");
  std::string out;
  for (std::int64_t i=0; i<limit; ++i) {
    auto byte=static_cast<unsigned char>(mem.load(offset(address,static_cast<Address>(i)),1));
    if (!byte) break;
    out.push_back(static_cast<char>(byte));
  }
  return out;
}
bool Model::is_player(Address object) const { return (word(mem,offset(object,0xc)) & 0x40) != 0; }
Raw Model::half_toward_zero(Raw value) noexcept { return static_cast<Raw>(s32(value)/2); }
Raw Model::stat_via_slot14(Address skill, Raw index, Raw level, Raw n) {
  auto adjusted=skill_call(skill,14,{level,n});
  return stat(skill,index,adjusted);
}
Model::ManaCostMethod Model::mana_cost_method(Raw slot, Raw n) {
  return [slot,n](Model &model, Address skill, Raw level) {
    if (slot==15) return model.skill_call(skill,15,{level,n});
    model.skill_call(skill,14,{level,n});
    return ZERO;
  };
}
Raw Model::invoke(const std::string &name, std::vector<Raw> args) {
  if (events.size() >= creature_hp::MAX_HISTORY || depth_ >= 64)
    throw Unsupported("callback history or recursion limit");
  // The reference canonicalizes addresses in its private stack range.
  for (auto &a : args)
    if (a >= 0x1e00000 && a < 0x1e10000)
      throw Unsupported("reference stack-token callback arguments unsupported");
  calls.push_back({name, args});
  std::vector<Argument> typed(args.begin(), args.end());
  events.push_back({name, std::move(typed)});
  auto it = callbacks.find(name);
  if (it == callbacks.end()) throw Unsupported("missing callback: " + name);
  auto fn = it->second;
  ++depth_;
  try { auto result = fn(*this, args); --depth_; return result; }
  catch (...) { --depth_; throw; }
}
std::optional<Raw> Model::dispatch_skill(Address self, Raw body, const std::vector<Raw> &a) {
  for (const auto &rule : mana_rules)
    if (body == rule.body) { arity(a,1); return mana_cost(self,rule.id,a[0]); }
  switch (body) {
  case 0x2f8d54: arity(a,4); return lifetap_cast(self,a[0],a[1],a[2],a[3]);
  case 0x308434: arity(a,4); return strongstance_cast(self,a[0],a[1],a[2],a[3]);
  case 0x311340: arity(a,4); return shadowstep_cast(self,a[0],a[1],a[2],a[3]);
  case 0x309acc: arity(a,4); return planarwind_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2ed4e4: arity(a,4); return shockoffire_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2cf1e0: arity(a,4); return darkness_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2d2984: arity(a,1); return harmtouch_can_use(self,a[0]);
  case 0x2d2a04: arity(a,4); return harmtouch_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2c7140: arity(a,1); return groundpound_can_use(self,a[0]);
  case 0x2c71ec: arity(a,4); return groundpound_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2ed9a0: arity(a,4); return froststorm_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2c3d68: arity(a,4); return shock_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2d5fc4: arity(a,1); return arrow_can_use(43,self,a[0]);
  case 0x2d603c: arity(a,4); return arrow_cast(43,self,a[0],a[1],a[2],a[3]);
  case 0x2d6d40: arity(a,1); return arrow_can_use(44,self,a[0]);
  case 0x2d6db8: arity(a,4); return arrow_cast(44,self,a[0],a[1],a[2],a[3]);
  case 0x2d877c: arity(a,1); return arrow_can_use(45,self,a[0]);
  case 0x2d87f4: arity(a,4); return arrow_cast(45,self,a[0],a[1],a[2],a[3]);
  case 0x2dac30: arity(a,1); return arrow_can_use(47,self,a[0]);
  case 0x2daca8: arity(a,4); return arrow_cast(47,self,a[0],a[1],a[2],a[3]);
  case 0x2d9a18: arity(a,1); return multifire_can_use(self,a[0]);
  case 0x2d9a90: arity(a,4); return multifire_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2e9930: arity(a,4); return coneoffrost_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2d1484: arity(a,1); return cyclone_can_use(self,a[0]);
  case 0x2d161c: arity(a,4); return cyclone_cast(self,a[0],a[1],a[2],a[3]);
  case 0x312168: arity(a,1); return resurrect_can_use(self,a[0]);
  case 0x3123e4: arity(a,4); return resurrect_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2de334: arity(a,1); return bash_can_use(self,a[0]);
  case 0x2de3bc: arity(a,4); return bash_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2e7224: arity(a,1); return criticalhit_can_use(self,a[0]);
  case 0x2e74b0: arity(a,4); return criticalhit_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2c8864: arity(a,4); return charge_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2e060c: arity(a,1); return ancestralcall_can_use(self,a[0]);
  case 0x2e0750: arity(a,4); return ancestralcall_cast(self,a[0],a[1],a[2],a[3]);
  case 0x3c8218: arity(a, 1); return can_use_base();
  case 0x308f2c: arity(a,1); return giantgrowth_can_use(self,a[0]);
  case 0x308f60: arity(a,4); return giantgrowth_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2cffb8: arity(a,1); return convertenemy_can_use(self,a[0]);
  case 0x2d0a58: arity(a,1); return convertundead_can_use(self,a[0]);
  case 0x2cff1c: arity(a,4); return convertenemy_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2d09bc: arity(a,4); return convertundead_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2e022c: arity(a,4); return minorhealing_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2e80fc: arity(a,4); return holyshield_cast(self,a[0],a[1],a[2],a[3]);
  case 0x2e8cf8: arity(a,4); return healing_cast(self,a[0],a[1],a[2],a[3]);
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

  return std::nullopt;
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
  Raw value = kernel::statistic_sum(kernel::statistic_base(static_cast<Raw>(base)),
                  kernel::statistic_step(word(mem, offset(skill, step_off + 4 * stat)), kernel::delta(level, first)));
  return round(value, lt(value, ZERO));
}
Raw Model::get_stat(Address skill, Raw stat, Raw level) {
  valid_stat(stat);
  return level == 0 ? 0 : stat_impl(skill, 0xe, 0x18, 0x24, stat, level);
}
Raw Model::get_stat_tier2(Address skill, Raw stat, Raw level, Raw cap) {
  valid_stat(stat);
  if (!level) return 0;
  level = kernel::capped_level(level, cap);
  if (kernel::tier2_fallback(s16(static_cast<Raw>(mem.load(offset(skill, 0x38 + 2 * stat), 2)))))
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
    return mana_cost_method(rule.slot,rule.n)(*this,skill,level);
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
    auto op = [i](Raw a, Raw b) { return kernel::range_combine(a, b, i != 0); };
    Raw bound;
    bool negative;
    if (!randomise) {
      bound = op(value, kernel::range_scale(value, spread)); negative = lt(bound, ZERO);
    } else {
      auto probe = op(value, kernel::range_random_scale(value, draw(), spread));
      negative = lt(probe, ZERO);
      bound = op(value, kernel::range_random_scale(value, draw(), spread));
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
  return stat_via_slot14(skill,1,level,1);
}
Raw Model::strongstance_stat0(Address skill, Raw level) {
  return stat_via_slot14(skill,0,level,20);
}
Raw Model::lifetap_fraction(Address skill, Raw level) {
  return mul(from_int(stat_via_slot14(skill,0,level,1)), 0x3c23d70a);
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
  return spells::rng_publication([&]{return word(mem,address);},
                                 [&](Raw seed){mem.store(address,4,seed);});
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
  // Retain this argument's checked address evaluation before draw acquisition.
  const auto seed_address = offset(spell, 0x10);
  spells::constructor_tail([&]{return draw();},
      [&](Raw seed){mem.store(seed_address,4,seed);},
      [&]{mem.store(offset(spell,0x14),4,table);});
}

namespace {
constexpr Raw HANDLE_FREE = 0x864ea8;
void walk_step(std::size_t &steps) {
  if (steps++ >= creature_hp::MAX_HISTORY)
    throw Unsupported("list traversal limit");
}
}
Raw Model::node_take() {
  return handles::take(HANDLE_FREE,
      [&](Address at) { return word(mem, at); },
      [&](Address at, unsigned width, Raw value) { mem.store(at, width, value); },
      offset);
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
  handles::recycle(HANDLE_FREE, node,
      [&](Address at) { return word(mem, at); },
      [&](Address at, unsigned width, Raw value) { mem.store(at, width, value); });
}
Raw Model::handle_link(Address head, Address back, Raw object) {
  return handles::paired_link(head, back, object,
      [&] { return node_take(); },
      [&](Address at, Raw node) { node_push(at, node); },
      [&](Address at, unsigned width, Raw value) { mem.store(at, width, value); },
      offset);
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

Model::Vec Model::vec(Address a) const {
  return {word(mem,a),word(mem,offset(a,4)),word(mem,offset(a,8))};
}
void Model::wvec(Address a, const Vec &v) {
  mem.store(a,8,std::uint64_t(v[0]) | (std::uint64_t(v[1]) << 32));
  mem.store(offset(a,8),4,v[2]);
}
Address Model::alloca(std::size_t size) {
  if (size > 0x10000 || ((size+15)&~std::size_t(15)) > sp_-0x1e00000)
    throw Unsupported("model stack exhausted");
  sp_ -= (size+15)&~std::size_t(15);
  return sp_;
}
Model::Result Model::typed_invoke(const std::string &name, std::vector<Argument> args) {
  if (events.size() >= creature_hp::MAX_HISTORY || depth_ >= 64)
    throw Unsupported("callback history or recursion limit");
  for (const auto &a : args) if (const auto *v=std::get_if<Raw>(&a))
    if (*v>=0x1e00000 && *v<0x1e10000)
      throw Unsupported("reference stack-token callback arguments unsupported");
  events.push_back({name,args});
  auto it=typed_callbacks.find(name);
  if (it==typed_callbacks.end()) throw Unsupported("missing typed callback: "+name);
  auto fn=it->second;
  ++depth_;
  try { auto result=fn(*this,args); --depth_; return result; }
  catch (...) { --depth_; throw; }
}
namespace {
constexpr Raw object_bodies[] = {0x2d66ac,0x2d685c,0x2d73c8,0x2d7aac,0x2d8e80,
  0x2d932c,0x2db17c,0x2db4f0,0x2cf690,0x2c42e4,0x2c7808,0x1ee330,
  0x1ee7b0,0x28fa3c,0x2900c8,0x1fe950,0x1fdef8};
Raw scalar(const Model::Result &r) {
  if (auto p=std::get_if<Raw>(&r)) return *p;
  throw Unsupported("callback requires raw32 return");
}

}
std::optional<Raw> Model::dispatch_spell(Address self, Raw body, const std::vector<Raw> &args) {
  if (body==0x311e3c) { arity(args,2); shadowstep_run(self,args[0],args[1]); return 0; }
  if (body==0x30a0b0) { arity(args,2); planarwind_run(self,args[0],args[1]); return 0; }
  if (body==0x2d2c0c) { arity(args,2); harmtouch_run(self,args[0],args[1]); return 0; }
  if (body==0x2e9d10) { arity(args,2); coneoffrost_run(self,args[0],args[1]); return 0; }
  if (body==0x2d1a94) { arity(args,2); cyclone_run(self,args[0],args[1]); return 0; }
  if (body==0x312b30) { arity(args,2); resurrect_run(self,args[0],args[1]); return 0; }
  if (body==0x2de7dc) { arity(args,2); bash_run(self,args[0],args[1]); return 0; }
  if (body==0x2e7acc) { arity(args,2); criticalhit_run(self,args[0],args[1]); return 0; }
  if (body==0x2c8f2c) { arity(args,2); charge_run(self,args[0],args[1]); return 0; }
  if (body==0x2e04f8) { arity(args,2); minorhealing_run(self,args[0],args[1]); return 0; }
  if (body==0x2e8428) { arity(args,2); holyshield_run(self,args[0],args[1]); return 0; }
  if (body==0x2e90b8) { arity(args,2); healing_run(self,args[0],args[1]); return 0; }
  if (body==0x2e0d10) { arity(args,2); ancestralcall_run(self,args[0],args[1]); return 0; }
  if (body==0x309420) { arity(args,2); giantgrowth_run(self,args[0],args[1]); return 0; }
  if (body==0x2d0354) { arity(args,2); convertenemy_run(self,args[0],args[1]); return 0; }
  if (body==0x2d0edc) { arity(args,2); convertundead_run(self,args[0],args[1]); return 0; }

  return std::nullopt;
}
std::optional<Raw> Model::dispatch_object(Address self, Raw body, const std::vector<Raw> &args) {
  switch (body) {
  case 0x28fa3c: arity(args,0); sparks_run(self); return 0;
  case 0x2900c8: arity(args,1); sparks_on_touch(self,args[0]); return 0;
  case 0x2cf690: arity(args,0); darkness_run(self); return 0;
  case 0x2c7808: arity(args,0); groundpound_effect_run(self); return 0;
  case 0x1fe950: arity(args,0); froststorm_run(self); return 0;
  case 0x1fdef8: arity(args,0); iceshard_run(self); return 0;
  case 0x2c42e4: arity(args,0); shock_run(self); return 0;
  case 0x2d66ac: arity(args,0); arrow_run(43,self); return 0;
  case 0x2d685c: arity(args,1); arrow_on_hit(43,self,args[0]); return 0;
  case 0x2d73c8: arity(args,0); arrow_run(44,self); return 0;
  case 0x2d7aac: arity(args,1); arrow_on_hit(44,self,args[0]); return 0;
  case 0x2d8e80: arity(args,0); arrow_run(45,self); return 0;
  case 0x2d932c: arity(args,1); arrow_on_hit(45,self,args[0]); return 0;
  case 0x2db17c: arity(args,0); arrow_run(47,self); return 0;
  case 0x2db4f0: arity(args,1); arrow_on_hit(47,self,args[0]); return 0;
  default: break;
  }
  if (body==0x1ee330 || body==0x1ee7b0) {
    if (body==0x1ee330) { arity(args,0); dummydamager_run(self); }
    else { arity(args,1); dummydamager_on_touch(self,args[0]); }
    return 0;
  }
  return std::nullopt;
}
Raw Model::skill_call(Address obj, unsigned slot, const std::vector<Raw> &args) {
  return virtual_call(VirtualClass::Skill,obj,word(mem,offset(obj,0x60)),slot,args);
}
Raw Model::spell_call(Address obj, unsigned slot, const std::vector<Raw> &args) {
  return virtual_call(VirtualClass::Spell,obj,word(mem,offset(obj,0x14)),slot,args);
}
Raw Model::vcall(Address obj, unsigned slot, const std::vector<Raw> &args) {
  return virtual_call(VirtualClass::Creature,obj,word(mem,offset(obj,0x48)),slot,args);
}
Raw Model::virtual_call(VirtualClass cls, Address obj, Raw table, unsigned slot,
                        const std::vector<Raw> &args) {
  if (slot > (cls==VirtualClass::Creature ? 64u : 15u) || depth_>=64)
    throw Unsupported("virtual slot or recursion limit");
  struct Guard { unsigned &d; Guard(unsigned &v):d(v){++d;} ~Guard(){--d;} } guard(depth_);
  Raw self=kernel::adjust_this(static_cast<Raw>(obj), static_cast<Raw>(mem.load(Address(table)+8*slot,2)));
  Raw body=word(mem,Address(table)+8*slot+4);
  std::string name=cls==VirtualClass::Skill ? "Skill" : cls==VirtualClass::Spell ? "Spell" : "Creature";
  name+=".v"+std::to_string(slot);
  if (!external.count(name)) {
    auto result=cls==VirtualClass::Skill ? dispatch_skill(self,body,args) :
                cls==VirtualClass::Spell ? dispatch_spell(self,body,args) : dispatch_object(self,body,args);
    if (result) return *result;
  }
  auto named=addr_names.find(body);
  if (named!=addr_names.end() && !named->second.empty()) name=named->second;
  std::vector<Argument> forwarded{self};
  for (std::size_t i=0;i<args.size();++i) {
    if (slot==2 && ((cls==VirtualClass::Spell && i==0) || (cls==VirtualClass::Skill && i==2)))
      forwarded.emplace_back(vec(args[i]));
    else forwarded.emplace_back(args[i]);
  }
  auto result=call(name,std::move(forwarded));
  if (auto value=std::get_if<Raw>(&result)) return *value;
  throw Unsupported("virtual callback requires raw32 return");
}
Raw Model::table_call(const std::string &cls, Address obj, Raw table_offset,
                      unsigned slot, const std::vector<Raw> &args) {
  if (cls=="Skill") return skill_call(obj,slot,args);
  if (cls=="Spell") return spell_call(obj,slot,args);
  if (slot>64) throw Unsupported("table slot limit");
  auto table=word(mem,offset(obj,table_offset));
  Raw self=kernel::adjust_this(static_cast<Raw>(obj), static_cast<Raw>(mem.load(Address(table)+8*slot,2)));
  std::vector<Argument> forwarded{self};
  forwarded.insert(forwarded.end(),args.begin(),args.end());
  auto result=call(cls+".v"+std::to_string(slot),std::move(forwarded));
  if (auto value=std::get_if<Raw>(&result)) return *value;
  throw Unsupported("table callback requires raw32 return");
}
Model::Result Model::call(const std::string &name, std::vector<Argument> args) {
  if (name=="handle_link" || name=="handle_clear" || name=="handle_removeNode") {
    std::vector<Raw> scalar_args;
    for (const auto &arg:args) {
      auto value=std::get_if<Raw>(&arg);
      if (!value) throw Unsupported("internal handle call requires raw32 arguments");
      scalar_args.push_back(*value);
    }
    if (name=="handle_link") { arity(scalar_args,3); return handle_link(scalar_args[0],scalar_args[1],scalar_args[2]); }
    arity(scalar_args,1);
    if (name=="handle_clear") { handle_clear(scalar_args[0]); return Raw(0); }
    return handle_remove_node(scalar_args[0]);
  }
  std::vector<Raw> scalar_args;
  for (const auto &arg:args) {
    auto value=std::get_if<Raw>(&arg);
    if (!value) return typed_invoke(name,std::move(args));
    scalar_args.push_back(*value);
  }
  if (!callbacks.count(name) && typed_callbacks.count(name))
    return typed_invoke(name,std::move(args));
  return invoke(name,std::move(scalar_args));
}
void Model::creature_update_spells(Address creature, Address pos) {
  auto frame=alloca(0x30), iterator=frame+0x10;
  // Scratch copies are pokes, not original stores. Read each word just before
  // copying it, including when caller memory aliases the scratch frame.
  for (unsigned i=0;i<3;++i) mem.poke(frame+4*i,4,word(mem,offset(pos,4*i)));
  iter_begin(iterator,offset(creature,0x364),0);
  std::size_t steps=0;
  while (word(mem,iterator+4)) {
    walk_step(steps);
    auto spell=word(mem,offset(word(mem,iterator+4),0xc));
    spell_call(spell,2,{static_cast<Raw>(frame),static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(creature,0x38),2))))});
    iter_next(iterator);
  }
  iter_end(iterator,2);
}
void Model::spell_end(Raw spell) { if (spell) spell_call(spell,1,{3}); }
void Model::game_object_ctor(Address object, Address pos, Raw class_def, Raw props) {
  ++draws; // Callee owns the actual shared seed write, even on failure.
  typed_invoke("GameObject_ctor",{static_cast<Raw>(offset(object,0)),vec(pos),class_def,props});
}
Raw Model::projectile_ctor(Address object, Address pos, Raw owner, Raw class_def) {
  game_object_ctor(object,pos,class_def,0);
  mem.store(offset(object,0x4c),4,0);
  mem.store(offset(object,0x48),4,0x5120b8);
  mem.store(offset(object,0x1c),4,0x41400000);
  mem.store(offset(object,0x50),1,0);
  mem.store(offset(object,0xc),4,word(mem,offset(object,0xc))|0x4081);
  if (owner) handle_link(offset(object,0x4c),owner,owner);
  return static_cast<Raw>(object);
}
Raw Model::obj_move_and_collide(Address object, Address vel) {
  Vec step=vec(vel);
  for (auto &v:step) v=mul(v,0x3c888888);
  for (unsigned i=0;i<3;++i) mem.store(offset(object,0x10+4*i),4,add(word(mem,offset(object,0x10+4*i)),step[i]));
  invoke("obj_updateGridCell",{static_cast<Raw>(object)});
  constexpr Raw WORLD=0x44fdc8;
  Raw result=0;
  auto ground=scalar(typed_invoke("terrain_height",{word(mem,WORLD),vec(offset(object,0x10)),word(mem,offset(object,0x1c))}));
  if (lt(word(mem,offset(object,0x18)),ground)) {
    result=1;
    Raw k=0x3dcccccc;
    Vec candidate{};
    std::size_t steps=0;
    for (;;) {
      walk_step(steps);
      for (unsigned i=0;i<3;++i) candidate[i]=sub(word(mem,offset(object,0x10+4*i)),mul(step[i],k));
      auto height=scalar(typed_invoke("terrain_height",{word(mem,WORLD),candidate,word(mem,offset(object,0x1c))}));
      if (le(height,candidate[2])) break;
      k=add(k,0x3dcccccc);
      if (!lt(k,0x40000000)) break;
    }
    wvec(offset(object,0x10),candidate);
    if (static_cast<std::uint8_t>(mem.load(offset(object,0x50),1))==0 &&
        scalar(typed_invoke("water_test",{word(mem,WORLD),vec(offset(object,0x10)),Raw(0),Raw(0x41400000)}))) {
      mem.store(offset(object,0x50),1,1);
      object_rand(object);
      auto name=typed_invoke("libc_sprintf",{Raw(0x532458),Raw(1)});
      auto text=std::get_if<std::string>(&name);
      if (!text) throw Unsupported("sprintf callback requires string return");
      auto resource=scalar(typed_invoke("res_find",{Raw(0x532478),*text}));
      invoke("sound_playOnObject",{resource,static_cast<Raw>(object),0x3e4ccccc});
      typed_invoke("water_disturb",{word(mem,WORLD),vec(offset(object,0x10)),Raw(0x442f0000),Raw(0x41c00000)});
    }
  }
  auto radius=word(mem,offset(object,0x1c)), height=word(mem,offset(object,0x20));
  auto position=vec(offset(object,0x10));
  Vec low{sub(position[0],radius),sub(position[1],radius),sub(position[2],height)};
  Vec high{add(position[0],radius),add(position[1],radius),add(position[2],height)};
  auto owner=handle_object(offset(object,0x4c));
  auto found=typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x100)});
  auto objects=std::get_if<std::vector<Raw>>(&found);
  if (!objects) throw Unsupported("box query callback requires object list return");
  if (objects->size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
  for (auto other:*objects) {
    auto flags=word(mem,offset(other,0xc));
    if ((flags&0x201)||(flags&0x20000000)||other==object||other==owner) continue;
    if (lt(add(word(mem,offset(other,0x18)),word(mem,offset(other,0x20))),sub(word(mem,offset(object,0x18)),word(mem,offset(object,0x20))))) continue;
    if (lt(add(word(mem,offset(object,0x18)),word(mem,offset(object,0x20))),word(mem,offset(other,0x18)))) continue;
    auto dx=sub(word(mem,offset(object,0x10)),word(mem,offset(other,0x10)));
    auto dy=sub(word(mem,offset(object,0x14)),word(mem,offset(other,0x14)));
    auto reach=add(word(mem,offset(object,0x1c)),word(mem,offset(other,0x1c)));
    if (lt(add(mul(dx,dx),mul(dy,dy)),mul(reach,reach))) {
      result|=2;
      vcall(other,6,{static_cast<Raw>(object)});
      vcall(object,6,{other});
    }
  }
  return result;
}

// Healing and protection: RTA 50, 54 and 55. External services own their
// effects; local arithmetic, reads and ordered writes are the contract body.
namespace {
constexpr Raw PLAYERS=0x4d8af0, REC_SIZE=0x4c30, REC_OBJ=0x43d8;
constexpr Raw ALIVE_COUNT=0x4f8420, ALIVE_LIST=0x4f8440;
constexpr Raw DT=0x3c888888, PERCENT=0x3c23d70a, HEAL_RATE=0x3c71aa2e;
Raw level_byte(const Memory &m, Address spell) {
  auto b=static_cast<Raw>(m.load(spell,1));
  return b&0x80 ? b|0xffffff00u : b;
}
Raw player_record(Raw slot) { return PLAYERS+slot*REC_SIZE; }
void heal_tint(Memory &m, Raw object) {
  for (auto [off,value] : {std::pair<unsigned,Raw>{0x9e,0x32},
       {0x90,0x35},{0x92,0x14},{0x94,0x0d},{0x98,0x35},{0x9a,0x14},{0x9c,0x0d},{0x96,0x32}})
    m.store(offset(object,off),2,value);
}
}
Raw Model::record(Address player) const { return player_record(word(mem,offset(player,0x3c8))); }
Raw Model::stat(Address skill, Raw index, Raw level) { return skill_call(skill,10,{index,level}); }
Raw Model::stat_for(Address skill, Raw index, Raw level, Raw caster, Raw cap) {
  return skill_call(skill,12,{index,level,caster,cap});
}
Raw Model::unit(Raw value) { return mul(from_int(value),TWO_M16); }
Raw Model::signed_unit(Raw value) { return sub(mul(unit(value),0x40000000),ONE); }
Model::Vec Model::ball(const std::function<Raw()> &generator) {
  std::size_t steps=0;
  for (;;) {
    walk_step(steps);
    Raw x=signed_unit(generator()), y=signed_unit(generator()), z=signed_unit(generator());
    if (!lt(ONE,add(add(mul(x,x),mul(y,y)),mul(z,z)))) return {x,y,z};
  }
}
Model::Vec Model::spell_ball(Address spell) { return ball([&]{return spell_rand(spell);}); }
Model::Vec Model::sample_point(Raw creature, Raw model, Raw value, std::int32_t count) {
  if (!count) throw Unsupported("model sample count is zero");
  auto index=static_cast<Raw>(static_cast<std::int64_t>(value)%count);
  auto result=typed_invoke("model_getSamplePoint",{model,creature+0x114u,vec(offset(creature,0x10)),
    static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(creature,0x38),2)))),index,Raw(1),Raw(0),ONE});
  if (auto point=std::get_if<SamplePoint>(&result)) return point->point;
  throw Unsupported("model sample callback requires point and second output");
}
Model::Result Model::object_ctor(const std::string &name, std::vector<Argument> args) {
  ++draws;
  return typed_invoke(name,std::move(args));
}
void Model::light_effect(Raw block, const Vec &pos, Raw f12, Raw f13, Raw f14,
                        Raw s0, Raw s1, Raw s2, Raw s3, Raw a2, Raw a3,
                        const std::array<Raw,5> &rest) {
  ++draws;
  std::vector<Argument> args{block,pos,a2,a3,f12,f13,f14};
  for (Raw v:rest) args.emplace_back(v);
  for (Raw v:{s0,s1,s2,s3}) args.emplace_back(v);
  typed_invoke("LightEffect_ctor",std::move(args));
}
Raw Model::minorhealing_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,20});
  Raw spell=invoke("pool_new",{0x24});
  if (spell) minorhealing_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
// Passive effects: the model owns selection/arithmetic, services own construction.
void Model::strongstance_trigger(Address, Raw player, Raw a2, Raw a3) {
  auto count=s32(invoke("model_getSamplePointCount",{word(mem,offset(player,0x544))}));
  auto light=invoke("pool_new",{0xbc});
  if (light) {
    Vec spot{add(word(mem,offset(player,0x10)),ZERO),word(mem,offset(player,0x14)),
             add(word(mem,offset(player,0x18)),0x42100000)};
    light_effect(light,spot,0x43000000,0x42c80000,0x42c80000,0x42f00000,0x42100000,HALF,HALF);
  }
  for (unsigned i=0;i<25;++i) {
    auto d=seed3_draw();
    auto point=sample_point(player,word(mem,offset(player,0x544)),d,count);
    auto direction=ball([&]{return cosmetic_draw();});
    auto inverse=rsqrt(ONE,add(add(mul(direction[0],direction[0]),mul(direction[1],direction[1])),
                             mul(direction[2],direction[2])));
    Vec velocity;
    for (unsigned j=0;j<3;++j) velocity[j]=mul(mul(direction[j],inverse),0x41200000);
    typed_invoke("particle_spawn",{Raw(0x8e7368),point,velocity,ZERO});
  }
  auto phantom=invoke("pool_new",{0x3d8});
  if (phantom) {
    ++draws;
    invoke("StrongStancePhantom_ctor",{phantom,player,a2,a3,Raw(0)});
  }
}
void Model::stunningstrike_effect(Address, Raw target) {
  if (!target || !(word(mem,offset(target,0xc))&0x20)) return;
  auto sound=invoke("res_find",{0x5520f0,0x5520f8});
  typed_invoke("sound_playAtPos",{sound,vec(offset(target,0x10)),ONE});
  auto light=invoke("pool_new",{0xbc});
  if (light) {
    Vec spot{add(word(mem,offset(target,0x10)),ZERO),word(mem,offset(target,0x14)),
             add(word(mem,offset(target,0x18)),0x42800000)};
    light_effect(light,spot,0x43210000,0x430a0000,0x42800000,0x43340000,0x42700000,HALF,HALF);
  }
  auto frames=to_int(mul(add(unit(draw()),0x3fc00000),0x42700000));
  vcall(target,30,{static_cast<Raw>(frames),Raw(1)});
}
void Model::minorhealing_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  spell_base_ctor(spell,level,owner,skill,0x519720);
  auto sound=invoke("res_find",{0x54fb00,0x54fb08});
  invoke("sound_playOnObject",{sound,word(mem,offset(spell,0xc)),ONE});
  mem.store(offset(spell,0x18),4,owner);
  auto shaft=invoke("pool_new",{0x5c});
  if (shaft) object_ctor("GlowingShaft_ctor",{shaft,vec(offset(word(mem,offset(spell,0xc)),0x10)),
    Raw(0x5a),Raw(0xfa),Raw(200),Raw(100)});
  mem.store(offset(spell,0x1c),4,0);
  auto percent=from_int(stat(word(mem,offset(spell,8)),0,level_byte(mem,spell)));
  auto rec=record(word(mem,offset(spell,0x18)));
  mem.store(offset(spell,0x20),4,mul(mul(mul(word(mem,offset(rec,0xb0)),percent),PERCENT),0x3c360b81));
}
void Model::minorhealing_run(Address spell, Raw, Raw) {
  auto target=word(mem,offset(spell,0x18)), rec=record(target);
  mem.store(offset(spell,0x1c),4,add(word(mem,offset(spell,0x1c)),DT));
  if (le(add(word(mem,offset(rec,0xac)),ONE),word(mem,offset(rec,0xb0))))
    mem.store(offset(rec,0xac),4,add(word(mem,offset(rec,0xac)),word(mem,offset(spell,0x20))));
  auto clock=word(mem,offset(spell,0x1c));
  if (lt(clock,0x3effff78))
    for (auto [off,value]:{std::pair<unsigned,Raw>{0x96,0x3f},{0x90,0x68},{0x92,0x6b},
                          {0x94,0x40},{0x98,0x68},{0x9a,0x6b},{0x9c,0x40}})
      mem.store(offset(target,off),2,value);
  if (lt(0x3fbfffde,clock)) spell_end(static_cast<Raw>(spell));
}
Raw Model::holyshield_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,20});
  auto spell=invoke("pool_new",{0x28});
  if (spell) holyshield_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::holyshield_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  spell_base_ctor(spell,level,owner,skill,0x519ce0);
  auto sound=invoke("res_find",{0x550350,0x550358});
  invoke("sound_playOnObject",{sound,word(mem,offset(spell,0xc)),ONE});
  auto shaft=invoke("pool_new",{0x5c});
  if (shaft) object_ctor("GlowingShaft_ctor",{shaft,vec(offset(word(mem,offset(spell,0xc)),0x10)),
    Raw(0x5a),Raw(0x56),Raw(0x7e),Raw(0xfa)});
  mem.store(offset(spell,0x1c),4,0);
  mem.store(offset(spell,0x18),4,0);
  auto duration=from_int(stat(word(mem,offset(spell,8)),1,level_byte(mem,spell)));
  mem.store(offset(spell,0x20),4,duration);
  auto group=invoke("particleGroup_create",{});
  mem.store(offset(spell,0x24),4,group);
}
void Model::holyshield_run(Address spell, Raw, Raw) {
  auto clock=add(word(mem,offset(spell,0x18)),DT);
  mem.store(offset(spell,0x18),4,clock);
  mem.store(offset(spell,0x1c),4,sub(word(mem,offset(spell,0x1c)),DT));
  if (lt(word(mem,offset(spell,0x20)),clock)) { spell_end(static_cast<Raw>(spell)); return; }
  auto owner=word(mem,offset(spell,0xc));
  auto matrix=mat34_set_pos_rot_z(vec(offset(owner,0x10)),static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))));
  typed_invoke("particleGroup_setMatrix",{word(mem,offset(spell,0x24)),matrix});
  owner=word(mem,offset(spell,0xc));
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x9e,0x37},{0x90,0x15},{0x92,0x1c},
      {0x94,0x73},{0x98,0x15},{0x9a,0x1c},{0x9c,0x73},{0x96,0x37}}) mem.store(offset(owner,off),2,value);
}
Raw Model::holyshield_absorb(Address spell) {
  auto owner=word(mem,offset(spell,0xc));
  auto model=vcall(owner,16,{});
  auto count=s32(invoke("model_getSamplePointCount",{model}));
  model=vcall(word(mem,offset(spell,0xc)),16,{});
  invoke("particleGroup_begin",{word(mem,offset(spell,0x24))});
  for (unsigned i=0;i<0xf1;++i) {
    auto value=spell_rand(spell);
    auto point=sample_point(word(mem,offset(spell,0xc)),model,value,count);
    auto velocity=spell_ball(spell);
    for (auto &v:velocity) v=mul(v,0x42140000);
    typed_invoke("particle_spawn",{Raw(0x8e5ff0),point,velocity,ZERO});
  }
  invoke("particleGroup_end",{word(mem,offset(spell,0x24))});
  if (le(word(mem,offset(spell,0x1c)),ZERO)) {
    auto light=invoke("pool_new",{0xbc});
    if (light) {
      owner=word(mem,offset(spell,0xc));
      Vec spot{add(word(mem,offset(owner,0x10)),ZERO),word(mem,offset(owner,0x14)),add(word(mem,offset(owner,0x18)),0x42700000)};
      light_effect(light,spot,0x42800000,0x42800000,0x42fa0000,0x42f00000,0x42700000,HALF,0x3e86a832);
    }
    mem.store(offset(spell,0x1c),4,0x3e86a832);
  }
  return mul(from_int(stat(word(mem,offset(spell,8)),0,level_byte(mem,spell))),PERCENT);
}
Raw Model::healing_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,14});
  auto spell=invoke("pool_new",{0x24});
  if (spell) healing_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::healing_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  spell_base_ctor(spell,level,owner,skill,0x519e20);
  auto sound=invoke("res_find",{0x550488,0x550490});
  invoke("sound_playOnObject",{sound,word(mem,offset(spell,0xc)),ONE});
  auto shaft=invoke("pool_new",{0x5c});
  if (shaft) object_ctor("GlowingShaft_ctor",{shaft,vec(offset(word(mem,offset(spell,0xc)),0x10)),
    Raw(0x5a),Raw(0xfa),Raw(200),Raw(100)});
  auto group=invoke("particleGroup_create",{});
  mem.store(offset(spell,0x20),4,group);
  owner=word(mem,offset(spell,0xc));
  mem.store(offset(spell,0x18),4,0);
  Raw percent,most;
  if (word(mem,offset(owner,0xc))&0x40) {
    percent=stat_for(word(mem,offset(spell,8)),0,level_byte(mem,spell),owner,10);
    most=word(mem,offset(record(owner),0xb0));
  } else {
    percent=stat_for(word(mem,offset(spell,8)),0,level_byte(mem,spell),owner,20);
    most=word(mem,offset(word(mem,offset(spell,0xc)),0x22c));
  }
  mem.store(offset(spell,0x1c),4,mul(mul(most,HEAL_RATE),mul(from_int(percent),PERCENT)));
}
Raw Model::heal_share(Address spell, Raw most) {
  auto percent=stat_for(word(mem,offset(spell,8)),2,level_byte(mem,spell),word(mem,offset(spell,0xc)),20);
  return mul(mul(most,HEAL_RATE),mul(from_int(percent),PERCENT));
}
void Model::healing_run(Address spell, Raw, Raw) {
  auto skill=word(mem,offset(spell,8));
  auto wide=std::int64_t(s32(stat_for(skill,1,level_byte(mem,spell),word(mem,offset(spell,0xc)),20)))*12;
  if (wide<INT32_MIN || wide>INT32_MAX) throw Unsupported("healing reach exceeds signed32 conversion domain");
  Raw reach=static_cast<Raw>(wide), reach2=from_int(reach*reach);
  auto owner=word(mem,offset(spell,0xc));
  auto early=[&]{return lt(word(mem,offset(spell,0x18)),0x3e050ef8);};
  if (!(word(mem,offset(owner,0xc))&0x40)) {
    if ((word(mem,offset(owner,0xc))&0x28)==0x20) {
      auto edge=from_int(reach); auto center=vec(offset(owner,0x10)); Vec low,high;
      for (unsigned i=0;i<3;++i) { low[i]=sub(center[i],edge); high[i]=add(center[i],edge); }
      auto result=typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x80)});
      auto objects=std::get_if<std::vector<Raw>>(&result);
      if (!objects || objects->size()>creature_hp::MAX_HISTORY) throw Unsupported("box callback requires bounded object list");
      for (Raw other:*objects) {
        owner=word(mem,offset(spell,0xc));
        if (other!=owner && !vcall(owner,21,{other})) continue;
        owner=word(mem,offset(spell,0xc));
        auto dy=sub(word(mem,offset(owner,0x14)),word(mem,offset(other,0x14)));
        auto dx=sub(word(mem,offset(owner,0x10)),word(mem,offset(other,0x10)));
        if (!le(add(mul(dx,dx),mul(dy,dy)),reach2)) continue;
        auto amount=other==owner ? word(mem,offset(spell,0x1c)) : heal_share(spell,word(mem,offset(other,0x22c)));
        mem.store(offset(other,0x228),4,add(word(mem,offset(other,0x228)),amount));
        if (early()) heal_tint(mem,other);
      }
    }
  } else {
    std::vector<Raw> chosen;
    auto count=s32(word(mem,ALIVE_COUNT));
    if (count>static_cast<std::int32_t>(creature_hp::MAX_HISTORY)) throw Unsupported("alive player traversal limit");
    for (std::int32_t i=0;i<count;++i) {
      auto cell=ALIVE_LIST+4*static_cast<Raw>(i);
      auto obj=word(mem,offset(player_record(word(mem,cell)),REC_OBJ));
      if (word(mem,G_PVP_MODE_A) && !vcall(word(mem,offset(spell,0xc)),22,{obj})) continue;
      owner=word(mem,offset(spell,0xc));
      obj=word(mem,offset(player_record(word(mem,cell)),REC_OBJ));
      auto dx=sub(word(mem,offset(owner,0x10)),word(mem,offset(obj,0x10)));
      auto dy=sub(word(mem,offset(owner,0x14)),word(mem,offset(obj,0x14)));
      auto dz=sub(word(mem,offset(owner,0x18)),word(mem,offset(obj,0x18)));
      if (le(add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz)),reach2)) chosen.push_back(word(mem,cell));
    }
    for (Raw slot:chosen) {
      auto rec=player_record(slot);
      if (le(add(word(mem,offset(rec,0xac)),ONE),word(mem,offset(rec,0xb0)))) {
        // The reference reads HP before invoking the recipient statistic callback.
        auto hp=word(mem,offset(rec,0xac));
        auto amount=word(mem,offset(rec,REC_OBJ))==word(mem,offset(spell,0xc)) ?
          word(mem,offset(spell,0x1c)) : heal_share(spell,word(mem,offset(rec,0xb0)));
        mem.store(offset(rec,0xac),4,add(hp,amount));
      }
      if (early()) heal_tint(mem,word(mem,offset(rec,REC_OBJ)));
    }
  }
  if (s32(reach)>=1) {
    auto level=s32(level_byte(mem,spell));
    auto first=s16(static_cast<Raw>(mem.load(offset(word(mem,offset(spell,8)),0x10),2)));
    auto anchor=invoke("net_objOrGhost",{word(mem,offset(spell,0xc))});
    invoke("particleGroup_begin",{word(mem,offset(spell,0x20))});
    auto matrix=mat34_set_pos_rot_z(vec(offset(anchor,0x10)),word(mem,creature_hp::G_SIM_FRAME)*0x5fu);
    typed_invoke("particleGroup_setMatrix",{word(mem,offset(spell,0x20)),matrix});
    for (int i=0;i<std::max(level-first+5,0);++i) {
      auto angle=spell_rand(spell);
      Vec ring{mul(cos16(angle),from_int(reach)),mul(sin16(angle),from_int(reach)),ZERO};
      auto world=mat34_transform_point(matrix,ring);
      auto height=sub(scalar(typed_invoke("terrain_heightAtPoint",{word(mem,0x44fdc8),world})),word(mem,offset(anchor,0x18)));
      if (lt(0x42900000,height) || lt(height,0xc2900000)) continue;
      typed_invoke("particle_spawn",{Raw(0x8e6040),Vec{ring[0],ring[1],height},Vec{ZERO,ZERO,ONE},ZERO});
    }
    invoke("particleGroup_end",{word(mem,offset(spell,0x20))});
  }
  auto clock=add(word(mem,offset(spell,0x18)),DT);
  mem.store(offset(spell,0x18),4,clock);
  if (lt(0x3f90a1df,clock)) spell_call(spell,1,{3});
}

// Conversion family (39/40). Query nodes are callback-model pokes, whereas
// target attachments use the logged paired-handle operations above.
namespace {
struct ConvertProfile {
  Raw table, frame, groups, sound0, sound1, radius, arc, turn, tick, speed, particle, resist, immune;
};
const ConvertProfile &convert_profile(unsigned id) {
  static constexpr ConvertProfile enemy{0x5189b0,0x1c,0x20,0x54ed98,0x54eda0,0x43a68000,
    0x430b0000,0x172d,0x3df9a5ca,0x40db3332,0x8e5460,0x54edc0,0x54edb8};
  static constexpr ConvertProfile undead{0x518a68,0x6c,0x1c,0x54ee98,0x54eea0,0x43a48000,
    0x42f00000,0x10bb,0x3e0519ea,0x419fbbbb,0x8e54b0,0x54eeb8,0};
  if (id==39) return enemy;
  if (id==40) return undead;
  throw Unsupported("conversion requires RTA skill 39 or 40");
}
Raw signed_byte(const Memory &m, Address a) {
  Raw byte=static_cast<Raw>(m.load(a,1));
  return byte<128 ? byte : byte|0xffffff00u;
}
std::vector<Raw> query_objects(const Model::Result &r) {
  if (auto p=std::get_if<std::vector<Raw>>(&r)) {
    if (p->size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
    return *p;
  }
  // Python's 'found or ()' also accepts the null scalar callback result.
  if (auto p=std::get_if<Raw>(&r); p && !*p) return {};
  throw Unsupported("query callback requires object list or zero");
}
}
void Model::list_fill(Address head, const std::vector<Raw> &objects) {
  if (objects.size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
  for (auto i=objects.rbegin();i!=objects.rend();++i) {
    auto node=word(mem,HANDLE_FREE);
    mem.poke(HANDLE_FREE,4,word(mem,node));
    mem.poke(offset(node,0x10),2,0);
    auto first=word(mem,head);
    mem.poke(node,4,first);
    if (first) mem.poke(offset(first,4),4,node);
    mem.poke(offset(node,4),4,head);
    mem.poke(head,4,node);
    mem.poke(offset(node,0xc),4,*i);
    mem.poke(offset(node,8),4,0);
  }
}
void Model::query_into(Address head, const std::string &name, std::vector<Argument> args) {
  auto found=query_objects(typed_invoke(name,std::move(args)));
  list_fill(head,found);
}
Address Model::query_list(const std::string &name, std::vector<Argument> args) {
  auto head=alloca(4);
  mem.poke(head,4,0);
  query_into(head,name,std::move(args));
  return head;
}
Raw Model::convertenemy_can_use(Address, Raw caster) {
  if (word(mem,G_PVP_MODE_A) && (word(mem,offset(caster,0xc))&0x40)) {
    auto text=invoke("text_lookup",{0x54ed70});
    invoke("hud_showMessage",{text,0x5a,word(mem,offset(caster,0x3c8)),0});
    return 0;
  }
  return 1;
}
Raw Model::convertundead_can_use(Address, Raw caster) {
  if (word(mem,G_PVP_MODE_A) && (word(mem,offset(caster,0xc))&0x40)) {
    auto text=invoke("text_lookup",{0x54ee58});
    invoke("hud_showMessage",{text,0x5a,word(mem,offset(caster,0x3c8)),0});
    return 0;
  }
  auto lo=vec(offset(caster,0x10)), hi=lo;
  for (unsigned i=0;i<3;++i) { lo[i]=sub(lo[i],0x43a48000); hi[i]=add(hi[i],0x43a48000); }
  auto found=query_objects(typed_invoke("world_collectObjectsInBox",{lo,hi,Raw(0x200)}));
  for (auto other:found)
    if (vcall(caster,20,{other}) && signed_byte(mem,offset(other,0xb4))==0x10) return 1;
  auto text=invoke("text_lookup",{0x54ee68});
  invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
  return 0;
}
Raw Model::convert_cast(unsigned id, Address skill, Raw level, Raw caster) {
  convert_profile(id);
  auto head=alloca(4);
  mem.poke(head,4,0);
  auto spell=invoke("pool_new",{0x78});
  if (spell) convert_spell_ctor(id,spell,level,caster,static_cast<Raw>(skill));
  handle_clear(head);
  return spell;
}
void Model::convert_spell_ctor(unsigned id, Address spell, Raw level, Raw owner, Raw skill) {
  const auto &p=convert_profile(id);
  spell_base_ctor(spell,level,owner,skill,p.table);
  mem.store(offset(spell,0x18),4,0);
  mem.store(offset(spell,p.frame),4,word(mem,0x44fdcc));
  auto most=s32(stat(skill,0,signed_byte(mem,spell)));
  mem.store(offset(spell,0x70),2,0);
  mem.store(offset(spell,0x74),4,0);
  auto sound=invoke("res_find",{p.sound0,p.sound1});
  invoke("sound_playOnObject",{sound,word(mem,offset(spell,0xc)),ONE});
  auto caster=word(mem,offset(spell,0xc));
  auto head=query_list("world_collectObjectsInArc",{vec(offset(caster,0x10)),
    Raw(s16(static_cast<Raw>(mem.load(offset(caster,0x38),2)))),Raw(0),p.radius,p.arc,ZERO});
  unsigned linked=0;
  if (most>0) for (auto other:list_objects(head)) {
    auto flags=word(mem,offset(other,0xc));
    if (!(flags&0x1000000) && !(flags&0x40)) {
      bool fits;
      if (id==39) fits=s16(static_cast<Raw>(mem.load(offset(other,0x312),2)))==0 && !invoke("creature_bossRank",{other});
      else fits=!invoke("creature_bossRank",{other}) && s16(static_cast<Raw>(mem.load(offset(other,0x312),2)))==0 && signed_byte(mem,offset(other,0xb4))==0x10;
      if (fits && le(word(mem,offset(other,0x354)),ZERO)) {
        handle_link(offset(spell,0x18),other,other);
        auto group=invoke("particleGroup_create",{});
        mem.store(offset(spell,p.groups+4*linked),4,group);
        ++linked;
      }
    }
    if (linked>=static_cast<unsigned>(most)) break;
  }
  handle_clear(head);
}
void Model::convert_word(Raw target, Raw key) {
  auto block=invoke("pool_new",{0x98});
  if (block) {
    auto text=invoke("text_lookup",{key});
    object_ctor("FlyingText_ctor",{block,vec(offset(target,0x10)),Raw(0),Raw(0),Raw(0),Raw(0),Raw(0),text});
  }
}
void Model::convert_run(unsigned id, Address spell) {
  const auto &p=convert_profile(id);
  auto seconds=stat(word(mem,offset(spell,8)),1,signed_byte(mem,spell));
  auto angle=(static_cast<Raw>(mem.load(offset(spell,0x70),2))+p.turn)&0xffff;
  mem.store(offset(spell,0x70),2,angle);
  mem.store(offset(spell,0x74),4,add(word(mem,offset(spell,0x74)),p.tick));
  Vec spiral{mul(cos16(Raw(s16(angle))),HALF),
    mul(sin16(Raw(s16(static_cast<Raw>(mem.load(offset(spell,0x70),2))))),HALF),ZERO};
  auto waiting=le(word(mem,offset(spell,0x74)),0x40800000);
  auto chance=waiting ? 0 : stat(word(mem,offset(spell,8)),2,signed_byte(mem,spell));
  unsigned index=0;
  for (auto target:list_objects(offset(spell,0x18))) {
    if (!target) continue;
    if (waiting) {
      auto group=word(mem,offset(spell,p.groups+4*index));
      auto matrix=mat34_set_pos_rot_z(vec(offset(target,0x10)),Raw(s16(static_cast<Raw>(mem.load(offset(target,0x38),2)))));
      typed_invoke("particleGroup_setMatrix",{group,matrix});
      auto reach=word(mem,offset(target,0x1c));
      Vec point{mul(spiral[0],reach),mul(spiral[1],reach),mul(word(mem,offset(spell,0x74)),word(mem,offset(target,0x1c)))};
      Vec velocity=spiral;
      for (auto &v:velocity) v=mul(v,p.speed);
      invoke("particleGroup_begin",{group});
      typed_invoke("particle_spawn",{p.particle,point,velocity,ZERO});
      invoke("particleGroup_end",{group});
    } else if (p.immune && signed_byte(mem,offset(target,0xb4))==0x10) {
      convert_word(target,p.immune);
      continue;
    } else {
      auto wide=lt(0x41a00000,word(mem,offset(target,0x1c)));
      auto need=wide ? round_half_away(mul(from_int(chance),HALF)) : s32(chance);
      if (static_cast<std::int32_t>(draw()%100+1)<need) {
        Raw team=UINT32_MAX;
        auto caster=word(mem,offset(spell,0xc));
        if (word(mem,offset(caster,0xc))&0x40) team=signed_byte(mem,offset(record(caster),0x4c24));
        mem.store(offset(target,0xa8),8,mem.load(offset(target,0xa8),8)|0x400000);
        auto frames=from_int(seconds*60u);
        mem.store(offset(target,0x354),4,mul(frames,0x3c888888));
        invoke("Creature_setCharm",{target,Raw(to_int(frames)),team});
      } else if (wide) convert_word(target,p.resist);
    }
    ++index;
  }
  if (!waiting) spell_end(static_cast<Raw>(spell));
}

// Giant Growth: section 65 of the pinned RTA contract. Engine effects delegate.
Raw Model::giantgrowth_can_use(Address, Raw caster) {
  auto type=invoke("SpellGiantGrowth_typeinfo",{});
  return invoke("creature_findSpellByType",{caster,type}) ? 0 : 1;
}
Raw Model::giantgrowth_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,10});
  if (!(word(mem,offset(caster,0xc))&0x40)) return 0;
  auto block=invoke("pool_new",{0x138});
  if (block) giantgrowth_spell_ctor(block,level,caster,static_cast<Raw>(skill));
  return block;
}
namespace {
Raw growth_remainder(Raw draw, Raw count) {
  auto divisor=s32(count);
  if (!divisor) throw Unsupported("zero growth sample count");
  return static_cast<Raw>(static_cast<std::int64_t>(draw)%divisor);
}
}
void Model::giantgrowth_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  auto w=[&](unsigned off,unsigned size,std::uint64_t value){mem.store(offset(spell,off),size,value);};
  spell_base_ctor(spell,level,owner,skill,0x51b6c0);
  w(0xf8,4,0); w(0x105,1,0); wvec(offset(spell,0x12c),vec(offset(owner,0x10)));
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x120,0x42f00000},{0x118,0x42f00000},
      {0x124,0x3891a2b3},{0x11c,0x3891a2b3},{0x128,0}}) w(off,4,value);
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x10e,200},{0x106,200},{0x112,0x55},
      {0x110,0x54},{0x108,0x54},{0x10a,0x55},{0x10c,0},{0x114,0}}) w(off,2,value);
  w(0x104,1,0); invoke("light_link",{static_cast<Raw>(offset(spell,0xfc))});
  auto holder=word(mem,offset(spell,0xc));
  handle_link(offset(spell,0xf8),owner,owner);
  w(0x18,4,word(mem,offset(holder,0xdc)));
  w(0x2c,4,invoke("particleGroup_create",{}));
  w(0x1c,4,add(mul(from_int(s32(stat(word(mem,offset(spell,8)),0,level_byte(mem,spell)))),PERCENT),ONE));
  auto frames=stat(word(mem,offset(spell,8)),1,level_byte(mem,spell));
  w(0x24,4,0); w(0x20,4,frames*60u);
  w(0x28,4,invoke("model_getSamplePointCount",{word(mem,offset(holder,0x544))}));
  for (unsigned off=0x30;off<0xf8;off+=4) w(off,4,0);
  for (unsigned off=0x30;off<0xf8;off+=4) {
    w(off+2,2,spell_rand(spell)); w(off,1,0);
    auto d=spell_rand(spell); w(off+1,1,growth_remainder(d,word(mem,offset(spell,0x28))));
  }
  auto sound=invoke("res_find",{0x552180,0x552188});
  typed_invoke("sound_playAtPos",{sound,vec(offset(holder,0x10)),ONE});
}
void Model::giantgrowth_step_scale(Address spell, Raw owner) {
  auto w=[&](unsigned off,Raw value){mem.store(offset(spell,off),4,value);};
  auto scale=word(mem,offset(owner,0xdc)), aim=word(mem,offset(spell,0x1c));
  if (eq(scale,aim)) {
    // Python tests the wide subtraction before the wrapped write at this branch.
    auto frames=static_cast<std::int64_t>(s32(word(mem,offset(spell,0x20))))-1;
    w(0x20,static_cast<Raw>(frames));
    if (frames<30) w(0x1c,word(mem,offset(spell,0x18)));
    if (s32(word(mem,offset(spell,0x20)))<0) w(0x24,1);
  } else {
    auto step=lt(scale,aim)?0x3bc49ba5u:0xbbc49ba5u;
    scale=add(scale,step); mem.store(offset(owner,0xdc),4,scale);
    if (lt(ZERO,step)) {
      if (lt(word(mem,offset(spell,0x1c)),scale)) mem.store(offset(owner,0xdc),4,word(mem,offset(spell,0x1c)));
    } else if (lt(step,ZERO)&&lt(scale,word(mem,offset(spell,0x1c))))
      mem.store(offset(owner,0xdc),4,word(mem,offset(spell,0x1c)));
    auto reach=mul(add(mul(sub(word(mem,offset(owner,0xdc)),ONE),0x40400000),0x40e00000),0x41400000);
    w(0x118,reach); w(0x120,reach);
    auto inverse=div(ONE,mul(reach,reach)); w(0x124,inverse); w(0x11c,inverse);
  }
  auto factor=mul(word(mem,offset(owner,0xdc)),0x3f400000);
  auto rec=record(owner); mem.store(offset(rec,0x4c20),4,factor);
  if (lt(factor,ONE)) mem.store(offset(rec,0x4c20),4,ONE);
}
void Model::giantgrowth_emit(Address spell, Raw owner) {
  invoke("particleGroup_begin",{word(mem,offset(spell,0x2c))});
  typed_invoke("particleGroup_setMatrix",{word(mem,offset(spell,0x2c)),mat34_set_pos_rot_z(vec(offset(owner,0x10)),0)});
  auto aim=word(mem,offset(spell,0x1c));
  unsigned count=eq(aim,word(mem,offset(spell,0x18)))?11:le(aim,word(mem,offset(owner,0xdc)))?4:47;
  for (unsigned k=0;k<count;++k) {
    auto slot=offset(spell,0x30+4*k);
    auto result=typed_invoke("model_getSamplePoint",{word(mem,offset(owner,0x544)),static_cast<Raw>(offset(owner,0x114)),
      vec(offset(owner,0x10)),Raw(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))),
      Raw(mem.load(slot+1,1)),Raw(1),Raw(1),word(mem,offset(owner,0xdc))});
    auto sample=std::get_if<GrowthSample>(&result);
    if (!sample || sample->number.empty()) throw Unsupported("growth sample requires point, number and matrix");
    auto matrix=mat3_rotate_axis(sample->matrix,2,Raw(s16(static_cast<Raw>(mem.load(slot+2,2)))));
    auto reach=mul(mul(sample->number[0],0x3a83126e),0x3f29ba0a);
    Vec here;
    for (unsigned i=0;i<3;++i) here[i]=add(sub(sample->point[i],word(mem,offset(owner,0x10+4*i))),mul(matrix[i],reach));
    typed_invoke("particle_spawn",{Raw(0x8e73b8),here,Vec{0,0,0},ZERO});
    mem.store(slot+2,2,mem.load(slot+2,2)+0x5da);
    auto age=Raw((mem.load(slot,1)+1)&255); mem.store(slot,1,age);
    if ((spell_rand(spell)&3)+0x29<age) {
      mem.store(slot,1,0); auto d=spell_rand(spell);
      mem.store(slot+1,1,growth_remainder(d,word(mem,offset(spell,0x28))));
    }
  }
  invoke("particleGroup_end",{word(mem,offset(spell,0x2c))});
}
void Model::giantgrowth_run(Address spell, Raw, Raw) {
  if (!word(mem,offset(spell,0xf8))) mem.store(offset(spell,0x24),4,1);
  else {
    auto owner=word(mem,offset(word(mem,offset(spell,0xf8)),0xc));
    giantgrowth_step_scale(spell,owner); giantgrowth_emit(spell,owner);
    wvec(offset(spell,0x12c),{add(word(mem,offset(owner,0x10)),ZERO),word(mem,offset(owner,0x14)),add(word(mem,offset(owner,0x18)),0x42480000)});
    invoke("light_relink",{static_cast<Raw>(offset(spell,0xfc))});
  }
  if (word(mem,offset(spell,0x24))) spell_end(static_cast<Raw>(spell));
}
Raw Model::giantgrowth_owner_scale(Address spell) const {
  return word(mem,offset(word(mem,offset(spell,0xc)),0xdc));
}
// Ancestral Call: complete section 51 contract, with caller-owned services.
Raw Model::ancestralcall_can_use(Address, Raw caster) {
  auto type=invoke("SpellAncestralCall_typeinfo",{});
  return invoke("creature_findSpellByType",{caster,type}) ? 0 : 1;
}
Raw Model::ancestralcall_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,10});
  auto spell=invoke("pool_new",{0x84});
  if (spell) ancestralcall_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::ancestralcall_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  auto w=[&](Address off,Raw v){mem.store(offset(spell,off),4,v);};
  spell_base_ctor(spell,level,owner,skill,0x5197d8);
  w(0x18,owner); w(0x1c,ZERO);
  auto sound=invoke("res_find",{0x54fbe0,0x54fbe8});
  invoke("sound_playOnObject",{sound,owner,ONE});
  auto weapon=static_cast<std::int8_t>(mem.load(offset(record(r(0x18)),0x4a86),1));
  Raw clipname=0x54fc10;
  switch (weapon) {
    case 0: clipname=0x54fc00; break; case 5: break;
    case 2: clipname=0x54fc20; break; case 1: clipname=0x54fc30; break;
    case 3: clipname=0x54fc40; break; case 7: clipname=0x54fc50; break;
  }
  auto name=typed_invoke("libc_sprintf",{clipname,std::monostate{}});
  Argument namearg;
  if (auto n=std::get_if<std::string>(&name)) namearg=*n;
  else if (auto n=std::get_if<Raw>(&name)) namearg=*n;
  else throw Unsupported("animation name callback requires string or raw32");
  auto player=r(0x18);
  auto clip=scalar(typed_invoke("res_find",{word(mem,word(mem,offset(player,0x24))),namearg}));
  invoke("anim_startBlendInOut",{r(0x18)+0x114u,clip,0x22,0x3dcccccc,0x3dcccccc});
  if (word(mem,offset(r(0xc),0xc))&0x20000) return;
  auto light=invoke("pool_new",{0xbc});
  if (light) {
    player=r(0x18);
    Vec spot{add(word(mem,offset(player,0x10)),ZERO),word(mem,offset(player,0x14)),
             add(word(mem,offset(player,0x18)),0x42100000)};
    light_effect(light,spot,ZERO,0x431b0000,0x437f0000,0x43340000,0x43100000,ZERO,
                 0x40a00000,player,0,{0xc2c00000,0,0,0,0});
  }
  mem.store(offset(r(0x18),0x58c),4,6);
  w(0x7c,0); w(0x28,word(mem,creature_hp::G_SIM_FRAME));
  for (unsigned k=0;k<3;++k) w(0x2c+4*k,0);
  w(0x20,from_int(stat(r(8),2,level_byte(mem,spell))));
  w(0x24,from_int(stat(r(8),3,level_byte(mem,spell))*12u));
  w(0x80,invoke("particleGroup_create",{}));
}
void Model::ancestralcall_tint_and_glow(Address, Raw obj) {
  for (auto [off,value]: {std::pair<unsigned,Raw>{0x92,0x30},{0x94,0xbe},
       {0x9a,0x30},{0x9c,0xbe},{0x90,0x2c},{0x9e,0x29},{0x98,0x2c},{0x96,0x29}})
    mem.store(offset(obj,off),2,value);
  typed_invoke("fx_shockModel",{word(mem,offset(obj,0x544)),obj+0x114u,vec(offset(obj,0x10)),
    static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x38),2)))),obj+0x320u,
    Raw(0x5e808080),ONE,ONE,ZERO});
}
Raw Model::ancestralcall_stat0_fraction(Address spell) {
  return mul(from_int(stat(word(mem,offset(spell,8)),0,level_byte(mem,spell))),PERCENT);
}
Raw Model::ancestralcall_stat1_fraction(Address spell) {
  return mul(from_int(stat(word(mem,offset(spell,8)),1,level_byte(mem,spell))),PERCENT);
}
Raw Model::ancestralcall_query_for(Address spell, Raw creature, Address out0, Address out1) {
  auto reach=stat(word(mem,offset(spell,8)),3,level_byte(mem,spell));
  if (out0) mem.store(out0,4,ancestralcall_stat0_fraction(spell));
  if (out1) mem.store(out1,4,ancestralcall_stat1_fraction(spell));
  if (!creature) return 0;
  auto owner=word(mem,offset(spell,0xc));
  if (creature==owner) return 1;
  if (word(mem,G_PVP_MODE_A)) {
    if (!vcall(owner,22,{creature})) return 0;
    owner=word(mem,offset(spell,0xc));
  }
  auto dx=sub(word(mem,offset(creature,0x10)),word(mem,offset(owner,0x10)));
  auto dy=sub(word(mem,offset(creature,0x14)),word(mem,offset(owner,0x14)));
  auto limit=from_int(reach*12u*reach*12u);
  return lt(limit,add(mul(dx,dx),mul(dy,dy))) ? 0 : 1;
}
void Model::ancestralcall_run(Address spell, Raw pos, Raw) {
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  auto w=[&](Address off,Raw v){mem.store(offset(spell,off),4,v);};
  auto here=vec(pos);
  w(0x1c,add(r(0x1c),DT));
  if (!r(0x7c)) {
    invoke("Player_animStep",{r(0x18),ONE});
    auto player=r(0x18);
    if (!(mem.load(offset(player,0x11a),2)&0x20)) {
      for (unsigned k=0;k<3;++k) w(0x2c+4*k,0);
      if (word(mem,offset(player,0x4a8))!=UINT32_MAX) {
        invoke("sound_stop",{word(mem,offset(player,0x4a8))});
        mem.store(offset(r(0x18),0x4a8),4,UINT32_MAX);
      }
      mem.store(offset(r(0x18),0x58c),4,2); w(0x7c,1);
    } else {
      auto attach=[&](Raw object,Raw index) {
        auto result=typed_invoke("model_getAttachPoint",{word(mem,offset(object,0x544)),
          vec(offset(object,0x10)),static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(object,0x38),2)))),
          object+0x114u,index,Raw(0),ONE});
        auto point=std::get_if<std::vector<Raw>>(&result);
        if (!point || point->size()!=3) throw Unsupported("attachment point requires three raw patterns");
        return Vec{(*point)[0],(*point)[1],(*point)[2]};
      };
      auto first=attach(player,0); player=r(0x18);
      auto second=attach(player,1);
      std::array<Vec,3> ends{{{add(here[0],ZERO),here[1],here[2]},first,second}};
      Vec top{add(here[0],ZERO),here[1],add(here[2],0x43b40000)};
      for (unsigned k=0;k<3;++k) {
        auto off=0x2c+4*k;
        if (word(mem,creature_hp::G_SIM_FRAME)-r(0x28)==word(mem,0x54fc60+4*k)) {
          player=r(0x18);
          if (word(mem,offset(player,0x4a8))==UINT32_MAX && k==0) {
            auto sound=invoke("res_find",{0x54fbe0,0x54fc70});
            invoke("sound_playOnObject",{sound,r(0x18),ONE});
            sound=invoke("res_find",{0x54fbe0,0x54fc88});
            auto handle=invoke("sound_playOnObject",{sound,r(0x18),ONE});
            mem.store(offset(r(0x18),0x4a8),4,handle);
            invoke("sound_release",{handle});
          }
          if (k==1) {
            auto sound=invoke("res_find",{0x54fbe0,0x54fc70});
            invoke("sound_playOnObject",{sound,r(0x18),ONE});
          }
          if (!r(off)) {
            w(off,scalar(typed_invoke("beam_create",{top,ends[k],Raw(0x168),Raw(5),
                         Raw(0x5e808080),Raw(0x417e6666),Raw(0x41080000)})));
            if (!r(off)) continue;
          }
          typed_invoke("beam_update",{r(off),top,ends[k],Raw(0x80),Raw(0x168)});
        } else if (r(off)) typed_invoke("beam_update",{r(off),top,ends[k],Raw(0x80),Raw(0x168)});
      }
    }
  }
  ancestralcall_tint_and_glow(spell,r(0x18));
  auto level=level_byte(mem,spell);
  auto reach=stat(r(8),3,level)*12u;
  invoke("particleGroup_begin",{r(0x80)});
  auto anchor=invoke("net_objOrGhost",{r(0xc)});
  auto matrix=mat34_set_pos_rot_z(vec(offset(anchor,0x10)),word(mem,creature_hp::G_SIM_FRAME)*0x43u);
  typed_invoke("particleGroup_setMatrix",{r(0x80),matrix});
  for (int k=0;k<std::max(s32(level)+14,0);++k) {
    auto angle=spell_rand(spell);
    Vec ring{mul(cos16(angle),from_int(reach)),mul(sin16(angle),from_int(reach)),ZERO};
    auto world=mat34_transform_point(matrix,ring);
    auto ground=scalar(typed_invoke("terrain_heightAtPoint",{word(mem,0x44fdc8),world}));
    anchor=invoke("net_objOrGhost",{r(0xc)});
    auto height=sub(ground,word(mem,offset(anchor,0x18)));
    if (le(height,0x42900000) && le(0xc2900000,height))
      typed_invoke("particle_spawn",{Raw(0x8e5c30),Vec{ring[0],ring[1],height},Vec{0,0,0},ZERO});
  }
  invoke("particleGroup_end",{r(0x80)});
  std::size_t visits=0;
  for (std::int64_t i=0;i<s32(word(mem,ALIVE_COUNT));++i) {
    walk_step(visits);
    auto slot=word(mem,ALIVE_LIST+4*i);
    auto obj=word(mem,static_cast<Raw>(player_record(slot)+REC_OBJ));
    auto caster=r(0x18);
    if (!obj || obj==caster) continue;
    if (word(mem,G_PVP_MODE_A) && !vcall(caster,22,{obj})) continue;
    caster=r(0x18);
    auto dy=sub(word(mem,offset(caster,0x14)),word(mem,offset(obj,0x14)));
    auto dx=sub(word(mem,offset(caster,0x10)),word(mem,offset(obj,0x10)));
    if (lt(add(mul(dx,dx),mul(dy,dy)),from_int(reach*reach)))
      ancestralcall_tint_and_glow(spell,obj);
  }
  if (lt(r(0x20),r(0x1c))) spell_end(static_cast<Raw>(spell));
}
// Melee buffs and strikes: sections 49, 53 and 35 of the pinned RTA contract.
std::array<Raw,2> Model::stat_range(Address skill, Raw index, Raw level, Raw randomise, Raw spread) {
  auto out=alloca(8);
  skill_call(skill,8,{index,level,randomise,static_cast<Raw>(out),static_cast<Raw>(out+4),spread});
  return {word(mem,out),word(mem,out+4)};
}
Raw Model::bash_can_use(Address, Raw caster) {
  if (!(word(mem,offset(caster,0xc))&0x40)) return 1;
  auto rec=record(caster);
  if (word(mem,offset(rec,0x43fc)) && level_byte(mem,offset(rec,0x4a86))!=4) return 1;
  auto text=invoke("text_lookup",{0x54f988});
  invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
  return 0;
}
Raw Model::bash_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,19});
  auto sound=invoke("res_find",{0x54f998,0x54f9a0});
  invoke("sound_playOnObject",{sound,caster,ONE});
  auto spell=invoke("pool_new",{0x28});
  if (spell) bash_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::bash_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  auto w=[&](Address off,Raw v){mem.store(offset(spell,off),4,v);};
  mem.store(spell,1,level); w(8,skill); w(0x14,0x515ba0); w(4,0); w(0xc,owner); w(0x10,draw());
  auto lump=word(mem,word(mem,offset(owner,0x24)));
  mem.store(offset(owner,0x58c),4,6); w(0x14,0x5195b0);
  auto clip=invoke("res_find",{lump,0x54f9d8});
  invoke("anim_startBlendInOut",{owner+0x114u,clip,0x812,0x3dccccccu,0x3dccccccu});
  mem.store(offset(clip,0x20),1,2); mem.store(offset(clip,0x1c),4,0x3f178d60);
  for (auto off:{0x20,0x1c,0x18,0x24}) w(off,0);
  auto head=query_list("world_collectObjectsInArc",{vec(offset(owner,0x10)),
    static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))),Raw(0),Raw(0x42c80000),Raw(0x42b40000),word(mem,offset(spell,0x20))});
  auto node=word(mem,head); std::size_t visits=0;
  if (node) {
    auto first=word(mem,offset(node,0xc));
    if (!(word(mem,offset(first,0xc))&0x1000000)) w(0x24,first);
    else {
      node=word(mem,node);
      while (node) {
        walk_step(visits);
        if (!(word(mem,offset(word(mem,offset(node,0xc)),0xc))&0x1000000)) { w(0x24,first); break; }
        node=word(mem,node);
      }
    }
  }
  handle_clear(head);
}
void Model::bash_windup_fx(Address spell, Raw) {
  auto anchor=invoke("net_objOrGhost",{word(mem,offset(spell,0xc))});
  auto count=s32(invoke("model_getSamplePointCount",{word(mem,offset(anchor,0x544))}));
  for (unsigned k=0;k<203;++k) {
    auto d=spell_rand(spell);
    auto point=sample_point(anchor,word(mem,offset(anchor,0x544)),d,count);
    auto velocity=spell_ball(spell);
    for (auto &v:velocity) v=mul(v,0x41500000);
    typed_invoke("particle_spawn",{Raw(0x8e5b40),point,velocity,ZERO});
  }
}
std::array<Raw,2> Model::bash_range(Address spell) {
  auto range=stat_range(word(mem,offset(spell,8)),0,level_byte(mem,spell),0,0x3e4ccccc);
  return {from_int(range[0]),from_int(range[1])};
}
void Model::bash_done(Address spell, Raw) { mem.store(offset(spell,0x18),4,1); }
void Model::bash_run(Address spell, Raw, Raw) {
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  auto owner=r(0xc); auto head=alloca(4); mem.poke(head,4,0);
  auto ev=invoke("Player_animStep",{owner,ONE});
  invoke("Player_move",{owner,3,0x41bfd8u+word(mem,offset(owner,0x3c8))*0x58u,0});
  auto target=r(0x24);
  if (target) {
    // This Creature slot takes a vector by value, unlike scalar vcall.
    auto table=word(mem,offset(owner,0x48));
    auto self=static_cast<Raw>(owner+static_cast<std::int64_t>(s16(static_cast<Raw>(mem.load(Address(table)+8*26,2)))));
    auto body=word(mem,Address(table)+8*26+4);
    auto name=std::string("Creature.v26");
    if (!external.count(name) && std::find(std::begin(object_bodies),std::end(object_bodies),body)!=std::end(object_bodies))
      throw Unsupported("unported registered object body");
    auto named=addr_names.find(body); if (named!=addr_names.end()) name=named->second;
    scalar(typed_invoke(name,{self,Raw(0),vec(offset(target,0x10))}));
  }
  if (ev&2) mem.store(offset(spell,0x1c),4,1);
  if (!r(0x18)) {
    if (!r(0x1c)) bash_windup_fx(spell);
    else {
      auto level=level_byte(mem,spell);
      auto most=s32(stat(r(8),3,level));
      auto chance=s32(stat(r(8),1,level));
      auto frames=static_cast<std::int64_t>(s32(stat(r(8),2,level)))*60;
      // Python carries this product at arbitrary width until callback masking.
      Raw flags=s32(level)>15 ? 0x200000 : s32(level)<6 ? 0x40000 : 0x80000;
      auto hand=typed_invoke("model_getAttachPoint",{word(mem,offset(owner,0x544)),vec(offset(owner,0x10)),
        static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))),owner+0x114u,Raw(1),Raw(0),ONE});
      auto point=std::get_if<std::vector<Raw>>(&hand);
      if (!point || point->size()!=3) throw Unsupported("attachment point requires three raw patterns");
      query_into(head,"world_collectObjectsInArc",{Vec{(*point)[0],(*point)[1],(*point)[2]},
        static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))),Raw(0),Raw(0x42240000),Raw(0xbf800000),ZERO});
      int hits=0; auto node=word(mem,head); std::size_t visits=0;
      if (node && most>=1) {
        for (;;) {
          walk_step(visits); auto other=word(mem,offset(node,0xc));
          if (!(word(mem,offset(other,0xc))&0x1000000) && vcall(owner,20,{other})) {
            auto range=bash_range(spell);
            invoke("damage_apply",{other,owner,100000,0,0,flags|0x40,0,1,range[0],range[1]});
            if (lt(ZERO,word(mem,offset(other,0x228))) && chance>=static_cast<int>(draw()%100+1))
              vcall(other,30,{static_cast<Raw>(frames),1});
            else for (auto [off,value]:{std::pair<unsigned,Raw>{0x92,100},{0x96,0x40},{0x98,100},
                        {0x90,100},{0x9a,100},{0x9c,0x40},{0x94,0x40}}) mem.store(offset(other,off),2,value);
            if (!r(0x18)) {
              auto light=invoke("pool_new",{0xbc});
              if (light) {
                Vec spot{add(word(mem,offset(other,0x10)),ZERO),word(mem,offset(other,0x14)),add(word(mem,offset(other,0x18)),0x42800000)};
                light_effect(light,spot,0x43210000,0x430a0000,0x42800000,0x43340000,0x42700000,HALF,HALF);
              }
            }
            ++hits; mem.store(offset(spell,0x18),4,1);
          }
          node=word(mem,node); if (!node || most<=hits) break;
        }
      }
      if (!r(0x18)) {
        query_into(head,"world_collectInSphere",{vec(offset(owner,0x10)),
          static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))),Raw(0x42240000)});
        node=word(mem,head); visits=0;
        while (node && (word(mem,offset(word(mem,offset(node,0xc)),0xc))&0x1000060)) { walk_step(visits); node=word(mem,node); }
        if (node) {
          auto range=bash_range(spell);
          invoke("damage_apply",{word(mem,offset(node,0xc)),owner,100000,0,0,flags|0x40,0,0,range[0],range[1]});
          mem.store(offset(spell,0x18),4,1);
        }
      }
      bash_done(spell,static_cast<Raw>(head));
    }
  }
  if (!(mem.load(offset(owner,0x11a),2)&0x10) || word(mem,offset(owner,0x450))) {
    mem.store(offset(owner,0x58c),4,2); spell_end(static_cast<Raw>(spell));
  } else if (invoke("net_isLocalSlot",{word(mem,offset(owner,0x3c8))})) mem.store(0x402797,1,60);
  handle_clear(head);
}
Raw Model::criticalhit_can_use(Address, Raw caster) {
  auto info=invoke("SpellCriticalHit_typeinfo",{});
  return invoke("creature_findSpellByType",{caster,info}) ? 0 : 1;
}
Raw Model::criticalhit_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  auto spell=invoke("pool_new",{0x5c});
  if (spell) criticalhit_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::criticalhit_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  spell_base_ctor(spell,level,owner,skill,0x519c28);
  mem.store(offset(spell,0x25),1,0); wvec(offset(spell,0x4c),vec(offset(owner,0x10)));
  for (auto off:{0x40,0x38}) { mem.store(offset(spell,off),4,0x42f00000); mem.store(offset(spell,off+4),4,0x3891a2b3); }
  for (auto off:{0x32,0x28,0x2a,0x30}) mem.store(offset(spell,off),2,0x40);
  mem.store(offset(spell,0x2e),2,0xd7); mem.store(offset(spell,0x26),2,0xd7);
  mem.store(offset(spell,0x48),4,0); mem.store(offset(spell,0x2c),2,0); mem.store(offset(spell,0x34),2,0); mem.store(offset(spell,0x24),1,0);
  invoke("light_link",{static_cast<Raw>(offset(spell,0x1c))});
  mem.store(offset(spell,0x18),4,from_int(stat(word(mem,offset(spell,8)),2,level_byte(mem,spell))));
  auto group=invoke("particleGroup_create",{}); mem.store(offset(spell,0x58),4,group);
}
void Model::criticalhit_set_tint(Address, Raw creature) {
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x90,0x44},{0x96,0x0f},{0x92,0x0e},{0x94,0x18}}) mem.store(offset(creature,off),2,value);
}
void Model::criticalhit_run(Address spell, Raw, Raw) {
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  auto w=[&](Address off,Raw v){mem.store(offset(spell,off),4,v);};
  auto left=sub(r(0x18),DT); w(0x18,left);
  if (lt(left,ZERO)) { spell_end(static_cast<Raw>(spell)); return; }
  criticalhit_set_tint(spell,r(0xc));
  auto duration=from_int(stat(r(8),2,level_byte(mem,spell)));
  auto age=sub(duration,r(0x18));
  if (le(age,0x40000000)) {
    auto owner=r(0xc); auto xyz=vec(offset(owner,0x10)); xyz[0]=add(xyz[0],ZERO); xyz[2]=add(xyz[2],0x42700000);
    wvec(offset(spell,0x4c),xyz); invoke("light_relink",{static_cast<Raw>(offset(spell,0x1c))});
    if (lt(ONE,age)) {
      auto dim=static_cast<Raw>(to_int(mul(sub(0x40000000,age),0x42800000)));
      auto bright=static_cast<Raw>(to_int(mul(sub(0x40000000,age),0x43570000)));
      mem.store(offset(spell,0x2a),2,dim); mem.store(offset(spell,0x26),2,bright); mem.store(offset(spell,0x2e),2,bright);
      for (auto off:{0x30,0x28,0x32}) mem.store(offset(spell,off),2,dim);
    }
  } else invoke("light_unlink",{static_cast<Raw>(offset(spell,0x1c))});
  auto skill=r(8); auto id=s32(word(mem,offset(skill,0x64))); int base=id==0x21 ? 10 : id==0x51 ? 5 : 1;
  auto level=level_byte(mem,spell);
  // The reference does not narrow reach before the positive test or from_int.
  auto reach=static_cast<std::int64_t>(s32(stat(skill,1,level)))*12;
  auto anchor=invoke("net_objOrGhost",{r(0xc)});
  if (reach>0) {
    invoke("particleGroup_begin",{r(0x58)});
    auto matrix=mat34_set_pos_rot_z(vec(offset(anchor,0x10)),word(mem,creature_hp::G_SIM_FRAME)*0x76u);
    typed_invoke("particleGroup_setMatrix",{r(0x58),matrix});
    for (int k=0;k<std::max(s32(level)-base+14,0);++k) {
      auto angle=spell_rand(spell); auto radius=from_int(static_cast<Raw>(reach));
      Vec ring{mul(cos16(angle),radius),mul(sin16(angle),radius),ZERO};
      auto world=mat34_transform_point(matrix,ring);
      auto height=sub(scalar(typed_invoke("terrain_heightAtPoint",{word(mem,0x44fdc8),world})),word(mem,offset(anchor,0x18)));
      if (lt(0x42900000,height) || lt(height,0xc2900000)) continue;
      typed_invoke("particle_spawn",{Raw(0x8e5fa0),Vec{ring[0],ring[1],height},Vec{0,0,ONE},ZERO});
    }
    invoke("particleGroup_end",{r(0x58)});
  }
  std::size_t visits=0;
  for (std::int64_t i=0;i<s32(word(mem,ALIVE_COUNT));++i) {
    walk_step(visits); auto owner=r(0xc);
    auto obj=word(mem,static_cast<Raw>(player_record(word(mem,ALIVE_LIST+4*i))+REC_OBJ));
    if (obj==owner) continue;
    if (word(mem,G_PVP_MODE_A) && !vcall(owner,22,{obj})) continue;
    owner=r(0xc);
    auto dy=sub(word(mem,offset(owner,0x14)),word(mem,offset(obj,0x14)));
    auto dx=sub(word(mem,offset(owner,0x10)),word(mem,offset(obj,0x10)));
    auto radius=static_cast<Raw>(reach);
    if (lt(add(mul(dx,dx),mul(dy,dy)),from_int(radius*radius))) criticalhit_set_tint(spell,obj);
  }
}
Raw Model::criticalhit_bonus_for(Address skill, Raw attacker, Raw own_rank) {
  auto rank=s32(own_rank); bool found=false;
  if (rank) { auto info=invoke("SpellCriticalHit_typeinfo",{}); found=invoke("creature_findSpellByType",{attacker,info})!=0; }
  if (!found) {
    rank=0; std::size_t visits=0;
    for (std::int64_t i=0;i<s32(word(mem,ALIVE_COUNT));++i) {
      walk_step(visits); auto cell=ALIVE_LIST+4*i;
      auto slot=word(mem,cell); if (slot==word(mem,offset(attacker,0x3c8))) continue;
      auto obj=word(mem,static_cast<Raw>(player_record(slot)+REC_OBJ));
      Vec d; for (unsigned k=0;k<3;++k) d[k]=sub(word(mem,offset(obj,0x10+4*k)),word(mem,offset(attacker,0x10+4*k)));
      if (!lt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])),0x49742400)) continue;
      if (word(mem,G_PVP_MODE_A) && !vcall(obj,22,{attacker})) continue;
      auto info=invoke("SpellCriticalHit_typeinfo",{});
      if (!invoke("creature_findSpellByType",{word(mem,static_cast<Raw>(player_record(word(mem,cell))+REC_OBJ)),info})) continue;
      auto best=s32(invoke("player_getSkillRank",{word(mem,cell),0x35}));
      best=std::max(best,s32(invoke("player_getSkillRank",{word(mem,cell),0x51})));
      best=std::max(best,s32(invoke("player_getSkillRank",{word(mem,cell),0x21})));
      if (!best) continue;
      auto reach=from_int(stat(skill,1,static_cast<Raw>(best))*12u);
      obj=word(mem,static_cast<Raw>(player_record(word(mem,cell))+REC_OBJ));
      auto dy=sub(word(mem,offset(obj,0x14)),word(mem,offset(attacker,0x14)));
      auto dx=sub(word(mem,offset(obj,0x10)),word(mem,offset(attacker,0x10)));
      if (!lt(add(mul(dx,dx),mul(dy,dy)),mul(reach,reach))) continue;
      if (best>0) rank=best;
      break;
    }
  }
  auto level=skill_call(skill,14,{static_cast<Raw>(rank),20});
  return level ? mul(from_int(stat(skill,0,level)),PERCENT) : ZERO;
}
Raw Model::charge_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  auto spell=invoke("pool_new",{0x88});
  if (spell) {
    charge_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
    if (word(mem,offset(caster,0xc))&0x40) {
      mem.store(offset(caster,0x58c),4,9);
      auto ghost=invoke("net_objOrGhost",{caster}); if (ghost) mem.store(offset(ghost,0x58c),4,9);
    }
  }
  return spell;
}
Model::Vec Model::charge_trail_point(Raw owner, const std::array<Raw,2> &ahead, Raw index) const {
  auto rise=add(mul(from_int(index),0x40bfffff),0x41400000);
  return {add(word(mem,offset(owner,0x10)),ahead[0]),add(word(mem,offset(owner,0x14)),ahead[1]),add(word(mem,offset(owner,0x18)),rise)};
}
void Model::charge_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  auto w=[&](Address off,Raw v){mem.store(offset(spell,off),4,v);};
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  spell_base_ctor(spell,level,owner,skill,0x5183a8);
  mem.store(offset(spell,0x55),1,0); wvec(offset(spell,0x7c),vec(offset(owner,0x10)));
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x70,0x42ce0000},{0x68,0x42ce0000},{0x74,0x38c5ad3b},{0x6c,0x38c5ad3b},{0x78,0}}) w(off,value);
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x5e,0x6c},{0x60,0x66},{0x56,0x6c},{0x58,0x66},{0x62,0xd0},{0x5a,0xd0},{0x5c,0},{0x64,0}}) mem.store(offset(spell,off),2,value);
  mem.store(offset(spell,0x54),1,0); invoke("light_link",{static_cast<Raw>(offset(spell,0x4c))});
  auto sound=invoke("res_find",{0x54e8e8,0x54e8f0}); invoke("sound_playOnObject",{sound,r(0xc),ONE});
  w(0x44,word(mem,creature_hp::G_SIM_FRAME));
  auto facing=static_cast<Raw>(mem.load(offset(r(0xc),0x38),2)); w(0x48,0);
  std::array<Raw,2> ahead{mul(cos16(facing),0x41200000),mul(sin16(static_cast<Raw>(mem.load(offset(r(0xc),0x38),2))),0x41200000)};
  for (Raw k=0;k<10;++k) {
    auto trail=invoke("trail_create",{1,800,4,0x40,0x40,0x80,0x20,0x3f4ccccc}); w(0x1c+4*k,trail);
    typed_invoke("trail_addPoint",{trail,charge_trail_point(r(0xc),ahead,k)});
  }
}
Raw Model::from_uint(Raw value) {
  if (!(value&0x80000000)) return from_int(value);
  auto half=from_int((value>>1)|(value&1)); return add(half,half);
}
void Model::charge_run(Address spell, Raw, Raw) {
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  auto owner=r(0xc);
  if (r(0x48) || lt(0x42340000,from_uint(word(mem,creature_hp::G_SIM_FRAME)-r(0x44)))) {
    if (word(mem,offset(owner,0xc))&0x40) {
      auto ghost=invoke("net_objOrGhost",{owner});
      if (!r(0x48)) { mem.store(offset(ghost,0x58c),4,2); mem.store(offset(owner,0x58c),4,2); }
      else {
        mem.store(offset(ghost,0x58c),4,3); mem.store(offset(owner,0x58c),4,3); mem.store(offset(owner,0x190),4,0xbd6eeeee);
        invoke("player_voice",{word(mem,offset(owner,0x3c8)),12});
        auto weapon=s32(level_byte(mem,offset(record(owner),0x4a86)));
        auto clip=word(mem,static_cast<Raw>(word(mem,offset(owner,0x24))+weapon*std::int64_t(0x68)+0x7c));
        invoke("anim_startBlendInOut",{owner+0x114u,clip,10,0x3dcccccc,0x3dcccccc});
        invoke("damage_apply",{owner,owner,100000,0,0,0x4000c040,0,0,0x41200000,0x41200000});
      }
    }
    mem.store(0x402797,1,1); spell_end(static_cast<Raw>(spell)); return;
  }
  std::array<Raw,2> ahead{mul(cos16(static_cast<Raw>(mem.load(offset(owner,0x38),2))),0x41200000),
    mul(sin16(static_cast<Raw>(mem.load(offset(r(0xc),0x38),2))),0x41200000)};
  for (Raw k=0;k<10;++k) typed_invoke("trail_addPoint",{r(0x1c+4*k),charge_trail_point(r(0xc),ahead,k)});
  owner=r(0xc); auto model=vcall(owner,16,{}); auto count=s32(invoke("model_getSamplePointCount",{model}));
  for (auto [definition,number,speed]:{std::array<Raw,3>{0x8e5000,4,0x41500000},{0x8e4fb0,11,0x41200000}}) {
    for (Raw k=0;k<number;++k) {
      auto current=r(0xc); auto d=spell_rand(spell); auto point=sample_point(current,model,d,count);
      auto velocity=spell_ball(spell); for (auto &v:velocity) v=mul(v,speed);
      typed_invoke("particle_spawn",{definition,point,velocity,ZERO});
    }
  }
  owner=r(0xc); wvec(offset(spell,0x7c),Vec{add(word(mem,offset(owner,0x10)),ZERO),word(mem,offset(owner,0x14)),add(word(mem,offset(owner,0x18)),0x42700000)});
  invoke("light_relink",{static_cast<Raw>(offset(spell,0x4c))}); mem.store(0x402797,1,1);
}
Raw Model::charge_stat3_scaled(Address spell) {
  return mul(from_int(stat(word(mem,offset(spell,8)),3,level_byte(mem,spell))),0x3c83126e);
}
void Model::charge_on_contact(Address spell, Raw target) {
  auto r=[&](Address off){return word(mem,offset(spell,off));};
  if (r(0x48) || (target && (word(mem,offset(target,0xc))&8))) return;
  auto level=level_byte(mem,spell);
  auto sound=invoke("res_find",{0x54e8e8,0x54e908}); invoke("sound_playOnObject",{sound,r(0xc),ONE}); invoke("camera_shake",{0,0x41200000});
  if (!target) {
    for (unsigned k=0;k<8;++k) {
      auto xbase=sub(word(mem,offset(r(0xc),0x10)),0x40c00000); auto x=add(xbase,from_int(spell_rand(spell)%12));
      auto ybase=sub(word(mem,offset(r(0xc),0x14)),0x40c00000); auto y=add(ybase,from_int(spell_rand(spell)%12));
      auto zbase=sub(add(word(mem,offset(r(0xc),0x18)),0x42100000),0x42100000); auto z=add(zbase,from_int(spell_rand(spell)%72));
      typed_invoke("fx_spawn",{Raw(2),Vec{x,y,z}});
    }
  } else {
    auto amount=from_int(stat(r(8),0,level));
    if (!(word(mem,offset(target,0xc))&0x20) || vcall(r(0xc),20,{target})) {
      auto high=add(amount,mul(mul(amount,unit(spell_rand(spell))),0x3e4ccccc));
      invoke("damage_apply",{target,r(0xc),100000,0,0,0x8c040,0,0,amount,high});
    }
  }
  auto reach=stat(r(8),2,level)*12u;
  if (reach) {
    auto owner=r(0xc); auto half=from_int(reach); Vec lo,hi;
    for (unsigned k=0;k<3;++k) lo[k]=sub(word(mem,offset(owner,0x10+4*k)),half);
    for (unsigned k=0;k<3;++k) hi[k]=add(word(mem,offset(owner,0x10+4*k)),half);
    auto found=query_objects(typed_invoke("world_collectObjectsInBox",{lo,hi,Raw(128)}));
    if (found.size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
    auto amount=from_int(stat(r(8),1,level));
    for (auto other:found) {
      owner=r(0xc); if (other==owner || other==target || (word(mem,offset(other,0xc))&8)) continue;
      if ((word(mem,offset(other,0xc))&0x20) && !vcall(owner,20,{other})) continue;
      auto high=add(amount,mul(mul(amount,unit(spell_rand(spell))),0x3e4ccccc));
      invoke("damage_apply",{other,r(0xc),100000,0,0,0x8c040,0,0,amount,high});
    }
    for (Raw angle=0;angle<0x10000;angle+=500) {
      owner=r(0xc); std::array<Raw,2> out{mul(cos16(angle),half),mul(sin16(angle),half)};
      Vec point{add(word(mem,offset(owner,0x10)),out[0]),add(word(mem,offset(owner,0x14)),out[1]),add(word(mem,offset(owner,0x18)),0x40a00000)};
      auto b=spell_ball(spell); auto rise=add(mul(unit(spell_rand(spell)),0x40a00000),0x41200000);
      Vec velocity{mul(b[0],0x41200000),mul(b[1],0x41200000),rise};
      typed_invoke("particle_spawn",{Raw(0x8e5050),point,velocity,ZERO});
      point={add(word(mem,offset(owner,0x10)),mul(out[0],HALF)),add(word(mem,offset(owner,0x14)),mul(out[1],HALF)),point[2]};
      typed_invoke("particle_spawn",{Raw(0x8e5050),point,velocity,ZERO});
    }
  }
  mem.store(offset(spell,0x48),4,1);
}

// Resurrection: section 68 of the pinned RTA contract. The target is the
// spell owner; the attaching caller is the caster. Services remain callbacks.
namespace {
constexpr Raw NUM_PLAYERS=0x4d8ae0, DEAD_COUNTDOWN=0x4f84a8;
constexpr Raw SIM_FRAME_RES=0x44fdcc;
void resurrect_tint(Memory &m, Raw object) {
  for (auto [off,value] : {std::pair<unsigned,Raw>{0x90,0x62},
       {0x9a,0x4b},{0x92,0x4b},{0x9c,0x2e},{0x94,0x2e},{0x9e,0x2e},{0x96,0x2e}})
    m.store(offset(object,off),2,value);
}
}
Raw Model::resurrect_can_use(Address skill, Raw caster) {
  if (word(mem,G_PVP_MODE_A) && (word(mem,offset(caster,0xc))&0x40)) {
    auto text=invoke("text_lookup",{0x552978});
    invoke("hud_showMessage",{text,0x5a,word(mem,offset(caster,0x3c8)),0});
    return 0;
  }
  if (s32(word(mem,NUM_PLAYERS))>=2 && resurrect_find_target(skill,caster)==UINT32_MAX) {
    if (word(mem,offset(caster,0xc))&0x40) {
      auto text=invoke("text_lookup",{0x552988});
      invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
    }
    return 0;
  }
  auto info=invoke("SpellResurrect_typeinfo",{});
  return invoke("creature_findSpellByType",{caster,info}) ? 0 : 1;
}
Raw Model::resurrect_find_target(Address skill, Raw caster) {
  auto facing=s16(static_cast<Raw>(mem.load(offset(caster,0x38),2)));
  Vec ahead{cos16(facing),sin16(facing),ZERO};
  Vec side{cos16(facing+0x4000),sin16(facing+0x4000),ZERO};
  Raw best=UINT32_MAX, best_d=0x4b18967f;
  std::size_t visits=0;
  for (std::int32_t slot=0; slot<s32(word(mem,NUM_PLAYERS)); ++slot) {
    walk_step(visits);
    if (s16(static_cast<Raw>(mem.load(DEAD_COUNTDOWN+2*Address(slot),2)))>=0x3d) continue;
    auto obj=word(mem,player_record(static_cast<Raw>(slot))+REC_OBJ);
    if (!obj) continue;
    auto d=vec3_dot_delta(vec(offset(caster,0x10)),ahead,vec(offset(obj,0x10)));
    if (!lt(0xc1400000,d) || !lt(d,0x43080000)) continue;
    obj=word(mem,player_record(static_cast<Raw>(slot))+REC_OBJ);
    auto across=vec3_dot_delta(vec(offset(caster,0x10)),side,vec(offset(obj,0x10)));
    if (lt(across,0x42600000) && lt(d,best_d) && !resurrect_is_being_raised(skill,static_cast<Raw>(slot))) {
      best=static_cast<Raw>(slot); best_d=d;
    }
  }
  return best;
}
Raw Model::resurrect_is_being_raised(Address, Raw slot) {
  std::size_t visits=0;
  for (std::int32_t i=0; i<s32(word(mem,ALIVE_COUNT)); ++i) {
    walk_step(visits);
    auto info=invoke("SpellResurrect_typeinfo",{});
    auto obj=word(mem,static_cast<Raw>(player_record(word(mem,ALIVE_LIST+4*Address(i)))+REC_OBJ));
    auto spell=invoke("creature_findSpellByType",{obj,info});
    if (spell && word(mem,offset(word(mem,offset(spell,0x18)),0x3c8))==slot) return 1;
  }
  return 0;
}
Raw Model::resurrect_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,1});
  Raw target, spell;
  if (s32(word(mem,NUM_PLAYERS))<2) {
    target=caster; spell=invoke("pool_new",{0x70});
    if (!spell) return 0;
  } else {
    auto slot=resurrect_find_target(skill,caster);
    if (slot==UINT32_MAX) return 0;
    spell=invoke("pool_new",{0x70});
    if (!spell) return 0;
    target=word(mem,static_cast<Raw>(player_record(slot)+REC_OBJ));
  }
  resurrect_spell_ctor(spell,level,target,static_cast<Raw>(skill),caster);
  return spell;
}
void Model::resurrect_spell_ctor(Address spell, Raw level, Raw target, Raw skill, Raw caster) {
  auto w=[&](unsigned off,Raw value){mem.store(offset(spell,off),4,value);};
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  mem.store(spell,1,level);
  auto position=vec(offset(target,0x10));
  w(0x14,0x515ba0); w(8,skill); w(0xc,target); w(4,0);
  w(0x10,draw()); w(0x14,0x51be00);
  mem.store(offset(spell,0x2d),1,0);
  wvec(offset(spell,0x54),{add(position[0],ZERO),position[1],add(position[2],ONE)});
  for (unsigned off:{0x48u,0x40u}) {w(off,0x41400000);w(off+4,0x3be38e38);}
  w(0x50,0);
  for (unsigned off:{0x36u,0x2eu}) {mem.store(offset(spell,off),2,0x73);mem.store(offset(spell,off+2),2,0x56);}
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x32,0x77},{0x34,0},{0x3a,0x77},{0x3c,0}}) mem.store(offset(spell,off),2,value);
  mem.store(offset(spell,0x2c),1,0);
  invoke("light_link",{static_cast<Raw>(offset(spell,0x24))});
  w(0x18,r(0xc));
  auto seconds=stat(r(8),s32(word(mem,NUM_PLAYERS))==1 ? 2 : 0,level);
  w(0x60,from_int(seconds)); w(0x68,caster); w(0x1c,0);
  auto rate=dmul(ddiv(0x3ff0000000000000ULL,f2d(r(0x60))),0x3fc645bae0000000ULL);
  w(0x20,0); w(0x64,d2f(rate)); w(0x6c,0);
  auto sound=invoke("res_find",{0x5529e8,0x5529f0});
  typed_invoke("sound_playAtPos",{sound,vec(offset(r(0xc),0x10)),ONE});
  if (s32(word(mem,NUM_PLAYERS))!=1) {
    caster=r(0x68);
    if (word(mem,offset(caster,0xc))&0x40) mem.store(offset(record(caster),0x4c20),4,HALF);
    target=r(0x18);
    invoke("anim_start",{static_cast<Raw>(offset(target,0x114)),word(mem,offset(word(mem,offset(target,0x24)),0x44)),3,0x3dcccccc});
  }
}
void Model::resurrect_run(Address spell, Raw, Raw) {
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto w=[&](unsigned off,Raw value){mem.store(offset(spell,off),4,value);};
  auto frame=s32(word(mem,SIM_FRAME_RES)*0xf3cu);
  auto low=frame%65536; // signed remainder, including wrapped frame product
  auto radius=add(mul(sin16(low),0x40e00000),0x42c20000);
  w(0x40,radius); w(0x48,radius);
  auto inverse=div(ONE,mul(radius,radius)); w(0x4c,inverse); w(0x44,inverse);
  if (s32(word(mem,NUM_PLAYERS))==1) {
    auto position=vec(offset(r(0xc),0x10));
    wvec(offset(spell,0x54),{add(position[0],ZERO),position[1],add(position[2],0x42400000)});
    invoke("light_relink",{static_cast<Raw>(offset(spell,0x24))});
  }
  auto target=r(0x18);
  bool several=s32(word(mem,NUM_PLAYERS))>=2;
  resurrect_tint(mem,target);
  if (several) {
    auto caster=r(0x68); resurrect_tint(mem,caster);
    Vec direction;
    for (unsigned i=0;i<3;++i) direction[i]=sub(word(mem,offset(target,0x10+4*i)),word(mem,offset(caster,0x10+4*i)));
    inverse=rsqrt(ONE,add(add(mul(direction[0],direction[0]),mul(direction[1],direction[1])),mul(direction[2],direction[2])));
    for (auto &v:direction) v=mul(v,inverse);
    typed_invoke("fx_emitFromModelVerts",{caster,Raw(0x12),Raw(0x8e8508),direction,Raw(0),Raw(0x42240000)});
    Vec back; for (unsigned i=0;i<3;++i) back[i]=mul(direction[i],0xc2280000);
    typed_invoke("fx_emitFromModelVerts",{r(0x18),Raw(0x26),Raw(0x8e8558),direction,back,Raw(0x421c0000)});
    if (lt(r(0x60),0x3e800000) && !r(0x1c)) {
      target=r(0x18); auto slot=word(mem,offset(target,0x3c8));
      typed_invoke("player_respawn",{slot,vec(offset(target,0x10)),static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(target,0x38),2)))),UINT32_MAX,UINT32_MAX});
      invoke("player_resetIdleAnim",{slot}); invoke("arena_addLife",{slot});
      w(0x18,word(mem,static_cast<Raw>(player_record(slot)+REC_OBJ)));
      auto percent=from_int(stat(r(8),1,level_byte(mem,spell)));
      auto rec=record(r(0x18)); w(0x1c,1);
      mem.store(offset(rec,0xac),4,mul(mul(word(mem,offset(rec,0xb0)),percent),PERCENT));
    }
    invoke("Player_animStep",{r(0x18),r(0x64)});
    mem.store(0x402797,1,1);
  }
  auto left=sub(r(0x60),DT); w(0x60,left);
  if (lt(left,ZERO)) spell_end(static_cast<Raw>(spell));
}
Raw Model::resurrect_on_owner_death(Address spell) {
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto value=draw(); auto chance=stat(r(8),3,level_byte(mem,spell));
  // Python compares a nonnegative draw remainder with a signed statistic.
  if (static_cast<std::int64_t>(value%100)<s32(chance)) {
    auto phantom=invoke("pool_new",{0x3d8});
    if (phantom) {
      auto target=r(0x18);
      object_ctor("StrongStancePhantom_ctor",{phantom,target,word(mem,offset(word(mem,offset(target,0x24)),8)),Raw(0),Raw(1)});
    }
    auto light=invoke("pool_new",{0xbc});
    if (light) {
      auto caster=r(0x68);
      Vec spot{add(word(mem,offset(caster,0x10)),ZERO),word(mem,offset(caster,0x14)),add(word(mem,offset(caster,0x18)),0x42100000)};
      light_effect(light,spot,0x43090000,0x42cc0000,0x42b80000,0x42f00000,0x43340000,ZERO,0x40800000,0,0,{0xc2c00000,0,0,0,0});
    }
    return mul(word(mem,offset(record(r(0x18)),0xb0)),HALF);
  }
  mem.store(offset(spell,0x20),4,1); mem.store(offset(spell,0x6c),4,word(mem,SIM_FRAME_RES));
  invoke("light_unlink",{static_cast<Raw>(offset(spell,0x24))});
  return ZERO;
}
// Complete Cyclone contract (41/84): raw arithmetic and ordered side effects.
namespace {
std::int32_t cyclone_signed_byte(Raw byte) {
  byte &= 255; return byte < 128 ? static_cast<std::int32_t>(byte) : static_cast<std::int32_t>(byte)-256;
}
}
Raw Model::cyclone_can_use(Address, Raw caster) {
  if (!(word(mem,offset(caster,0xc))&0x40)) return 1;
  auto weapon=mem.load(offset(record(caster),0x4a86),1);
  if (weapon!=4 && weapon!=6) return 1;
  auto text=invoke("text_lookup",{0x54ef60});
  invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
  return 0;
}
Raw Model::cyclone_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  auto spell=invoke("pool_new",{0x7c});
  if (spell) cyclone_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::cyclone_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  auto r=[&](unsigned o){return word(mem,offset(spell,o));};
  auto w=[&](unsigned o,Raw v){mem.store(offset(spell,o),4,v);};
  auto h=[&](unsigned o,Raw v){mem.store(offset(spell,o),2,v);};
  auto b=[&](unsigned o,Raw v){mem.store(offset(spell,o),1,v);};
  spell_base_ctor(spell,level,owner,skill,0x518a98);
  w(0x18,0); wvec(offset(spell,0x4c),vec(offset(owner,0x10)));
  b(0x25,0); h(0x2e,0x9e); h(0x26,0x9e);
  w(0x40,0x42100000); w(0x44,0x3a4a4587); w(0x38,0x42100000); w(0x3c,0x3a4a4587);
  h(0x30,0x6f); h(0x28,0x6f); h(0x32,0x43); h(0x2a,0x43);
  w(0x48,0); h(0x2c,0); h(0x34,0); b(0x24,0);
  invoke("light_link",{static_cast<Raw>(offset(spell,0x1c))});
  mem.store(offset(owner,0x58c),4,6); w(0x58,word(mem,creature_hp::G_SIM_FRAME));
  auto clip=invoke("res_find",{word(mem,word(mem,offset(owner,0x24))),0x54ef88});
  w(0x78,clip);
  auto turns=stat(r(8),1,level_byte(mem,spell));
  clip=r(0x78);
  w(0x68,mul(mul(from_int(s32(word(mem,offset(clip,4)))),DT),from_int(s32(turns))));
  mem.store(offset(clip,0x18),1,1); mem.store(offset(clip,0x14),4,0x3d4ccccc);
  invoke("anim_push",{owner+0x114u,r(0x78),0x23});
  w(0x5c,0); w(0x60,0);
  h(0x64,spell_rand(spell)); h(0x66,spell_rand(spell));
  wvec(offset(spell,0x6c),vec(offset(owner,0x10)));
  if (word(mem,offset(r(0xc),0xc))&0x20000) return;
  invoke("player_voice",{word(mem,offset(owner,0x3c8)),10});
  auto sound=invoke("res_find",{0x54ef98,0x54efa0});
  invoke("sound_playOnObject",{sound,owner,ONE});
  auto head=query_list("world_collectInSphere",{vec(offset(owner,0x10)),
      static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(owner,0x38),2)))),Raw(0x43f00000)});
  for (auto obj:list_objects(head)) handle_link(offset(spell,0x18),obj,obj);
  handle_clear(head);
}
void Model::cyclone_run(Address spell, Raw pos, Raw facing) {
  auto r=[&](unsigned o){return word(mem,offset(spell,o));};
  auto w=[&](unsigned o,Raw v){mem.store(offset(spell,o),4,v);};
  facing=static_cast<Raw>(s16(facing));
  auto here=vec(pos); auto owner=r(0xc);
  auto ghost=mem.load(0x402791,1) ? invoke("net_objOrGhost",{owner}) : 0;
  auto events_mask=invoke("Player_animStep",{owner,ONE});
  if (events_mask&1) {
    auto clip=r(0x78); w(0x68,sub(r(0x68),0x3d4ccccc));
    auto length=mul(from_int(s32(word(mem,offset(clip,4)))),DT);
    auto when=add(word(mem,offset(clip,0x14)),0x3d4ccccc);
    mem.store(offset(clip,0x14),4,when);
    if (lt(length,when)) mem.store(offset(clip,0x14),4,sub(when,length));
  }
  if (events_mask&4) w(0x60,1);
  auto period=to_int(word(mem,0x44fe08)&0x100 ? 0x40abde74 : 0x408f3962);
  auto frame=static_cast<std::int64_t>(s32(word(mem,creature_hp::G_SIM_FRAME)));
  bool hit_tick=(frame<0 ? -frame : frame)%period==0;
  if (!(mem.load(offset(owner,0x11a),2)&0x30)) {
    mem.store(offset(owner,0x58c),4,2); spell_end(static_cast<Raw>(spell)); return;
  }
  if (word(mem,offset(owner,0x58c))!=6) { spell_end(static_cast<Raw>(spell)); return; }
  if (r(0x5c)) {
    auto radius=mul(r(0x38),0x3f6a7ef9); w(0x38,radius); w(0x40,radius);
    auto inverse=div(ONE,mul(radius,radius)); w(0x44,inverse); w(0x3c,inverse);
  } else {
    if (le(r(0x68),0x3d93708a)) {
      auto track=static_cast<Raw>(owner+static_cast<std::int64_t>(cyclone_signed_byte(static_cast<Raw>(mem.load(offset(owner,0x118),1))))*0x20);
      mem.store(offset(track,0x10c),4,ONE);
      auto lump=word(mem,word(mem,offset(owner,0x24)));
      mem.store(offset(track,0x110),4,0xbf555554);
      auto clip=invoke("res_find",{lump,0x54efb8}); w(0x78,clip);
      mem.store(offset(clip,0x28),1,4); mem.store(offset(clip,0x24),4,0x3e2b00bc);
      invoke("anim_startBlendInOut",{owner+0x114u,r(0x78),0x32,ZERO,0x3e676c48});
      if (ghost) invoke("anim_startBlendInOut",{ghost+0x114u,r(0x78),0x32,ZERO,0x3e676c48});
      w(0x5c,1);
    }
    auto speed=mul(mul(from_int(s32(stat(r(8),2,level_byte(mem,spell)))),0x430f1f71),PERCENT);
    // Unsigned angle lanes give exactly the low16 of Python's wide negation.
    auto yaw=Raw(0)-invoke("net_getCamYaw",{word(mem,offset(owner,0x3c8))});
    Vec side{neg(cos16(yaw-0x4000u)),neg(sin16(yaw-0x4000u)),ZERO};
    Vec ahead{neg(cos16(yaw-0x8000u)),neg(sin16(yaw-0x8000u)),ZERO};
    Raw pad=0x41bfd8u+word(mem,offset(owner,0x3c8))*0x58u;
    auto along=mul(neg(speed),word(mem,offset(pad,0x18)));
    auto across=mul(speed,word(mem,offset(pad,0x14)));
    Vec push{}; for(unsigned i=0;i<3;++i) push[i]=add(mul(side[i],along),mul(ahead[i],across));
    auto before=vec(offset(owner,0x10));
    typed_invoke("Player_move",{owner,Raw(5),pad,push});
    for(unsigned i=0;i<3;++i) {
      auto moved=mul(sub(word(mem,offset(owner,0x10+4*i)),before[i]),0x3f7686d7);
      w(0x6c+4*i,add(r(0x6c+4*i),moved));
    }
  }
  if (!r(0x5c) || !r(0x60)) {
    cyclone_update_fx(spell,here);
    auto result=typed_invoke("model_getAttachPoint",{word(mem,offset(owner,0x544)),here,facing,
        owner+0x114u,Raw(0),Raw(0),ONE});
    auto hand=std::get_if<std::vector<Raw>>(&result);
    if (!hand || hand->size()!=3) throw Unsupported("attachment point requires three raw patterns");
    auto dx=sub((*hand)[0],here[0]), dy=sub((*hand)[1],here[1]);
    auto inverse=rsqrt(ONE,add(mul(dx,dx),mul(dy,dy)));
    Vec tip{add(here[0],mul(mul(dx,inverse),0x41e4acfd)),
            add(here[1],mul(mul(dy,inverse),0x41e4acfd)),
            add(here[2],mul(mul(ZERO,inverse),0x41e4acfd))};
    if (hit_tick) {
      auto scale=mul(from_int(s32(stat(r(8),0,level_byte(mem,spell)))),PERCENT);
      auto node=r(0x18); std::size_t visits=0;
      while (node) {
        if (++visits>creature_hp::MAX_HISTORY) throw Unsupported("cyclone target traversal limit");
        auto obj=word(mem,offset(node,0xc));
        if (obj && vcall(owner,20,{obj})) {
          auto ox=sub(word(mem,offset(obj,0x10)),tip[0]), oy=sub(word(mem,offset(obj,0x14)),tip[1]);
          if (lt(sqrt(add(mul(ox,ox),mul(oy,oy))),add(word(mem,offset(obj,0x1c)),0x41b80000)))
            invoke("Player_meleeHit",{owner,obj,0,0x40000000,scale});
        }
        node=word(mem,node);
      }
    }
  }
  if (invoke("net_isLocalSlot",{word(mem,offset(owner,0x3c8))})) mem.store(0x402797,1,0x78);
}
void Model::cyclone_update_fx(Address spell, const Vec &here) {
  auto r=[&](unsigned o){return word(mem,offset(spell,o));};
  auto w=[&](unsigned o,Raw v){mem.store(offset(spell,o),4,v);};
  auto angle=[&](unsigned o){return static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(spell,o),2))));};
  wvec(offset(spell,0x4c),here); w(0x54,add(r(0x54),0x41880000));
  invoke("light_relink",{static_cast<Raw>(offset(spell,0x1c))});
  auto frame=static_cast<std::int64_t>(s32(word(mem,creature_hp::G_SIM_FRAME)));
  // Signed truncation to a multiple of 65536, rather than a low16 mask.
  auto low=frame-(frame/65536)*65536;
  auto pulse=mul(cos16(static_cast<Raw>(low)),0x42b40000);
  mem.store(offset(spell,0x64),2,static_cast<Raw>(mem.load(offset(spell,0x64),2))+0x6f5u);
  auto radius=add(pulse,0x42100000); w(0x38,radius); w(0x40,radius);
  auto inverse=div(ONE,mul(radius,radius)); w(0x44,inverse); w(0x3c,inverse);
  auto c1=cos16(angle(0x64)),s1=sin16(angle(0x64)); auto anchor=vec(offset(spell,0x6c));
  constexpr Raw three=0x40400000,far=0x41f80000,tangent=0x43280000;
  Vec p0{add(mul(c1,three),anchor[0]),add(mul(s1,three),anchor[1]),add(ZERO,anchor[2])};
  mem.store(offset(spell,0x66),2,static_cast<Raw>(mem.load(offset(spell,0x66),2))+0x2e7u);
  auto c2=cos16(angle(0x66)),s2=sin16(angle(0x66));
  Vec p1{add(mul(c2,far),anchor[0]),add(mul(s2,far),anchor[1]),add(0x42b40000,anchor[2])};
  Vec m0{mul(ZERO,tangent),mul(ZERO,tangent),mul(ONE,tangent)};
  Vec m1{mul(c2,tangent),mul(s2,tangent),mul(ZERO,tangent)};
  auto previous=p0; auto t=ZERO;
  do {
    auto swirl=mul(mul(t,0x46686400),0x447a0000);
    auto width=add(mul(t,0x41e00000),three);
    auto turn=static_cast<Raw>(s16(static_cast<Raw>(to_int(le(ZERO,swirl)?add(swirl,HALF):sub(swirl,HALF)))));
    auto a=angle(0x64)+turn;
    auto ox=mul(cos16(a),width),oy=mul(sin16(a),width);
    auto point=vec3_hermite(t,p0,p1,m0,m1);
    point={add(point[0],ox),add(point[1],oy),add(point[2],ZERO)};
    Vec velocity{}; for(unsigned i=0;i<3;++i) velocity[i]=mul(sub(previous[i],point[i]),0xbf065e67);
    typed_invoke("particle_spawn",{Raw(0x8e5500),point,velocity,ZERO});
    t=add(t,0x3c656041); previous=point;
  } while (le(t,ONE));
  if (cyclone_signed_byte(static_cast<Raw>(mem.load(spell,1)))>=9 && (frame<0?-frame:frame)%3==0) {
    Vec point{r(0x6c),r(0x70),add(r(0x74),0x42840000)};
    auto vx=mul(sub(unit(spell_rand(spell)),HALF),0x41f00000);
    auto vy=mul(sub(unit(spell_rand(spell)),HALF),0x41f00000);
    auto vz=mul(unit(spell_rand(spell)),0x421c0000);
    typed_invoke("particle_spawn",{Raw(0x8e5550),point,Vec{vx,vy,vz},ZERO});
  }
}
// RTA 56: channel presentation and a reusable projectile damage carrier.
Address Model::local_vec(const Vec &v) {
  auto address=alloca(12);
  for (unsigned i=0;i<3;++i) mem.poke(address+4*i,4,v[i]);
  return address;
}
Model::Vec Model::object_ball(Address obj) { return ball([&]{return object_rand(obj);}); }
Model::Descriptor Model::descriptor(std::vector<std::array<Raw,4>> entries) {
  for (auto &entry:entries) for (auto &field:entry) field &= 0xffff;
  return {std::move(entries)};
}
Raw Model::coneoffrost_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,1});
  auto spell=invoke("pool_new",{0x58});
  if (spell) coneoffrost_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::coneoffrost_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  auto here=vec(offset(owner,0x10));
  spell_base_ctor(spell,level,owner,skill,0x519ed8);
  mem.store(offset(spell,0x21),1,0);
  wvec(offset(spell,0x48),{add(here[0],ZERO),here[1],add(here[2],0x42700000)});
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x3c,0x431c0000},
       {0x40,0x382c598a},{0x44,0},{0x34,0x431c0000},{0x38,0x382c598a}})
    mem.store(offset(spell,off),4,value);
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x2a,0x4d},
       {0x2c,0x4c},{0x22,0x4d},{0x24,0x4c},{0x26,0xbb},{0x28,0},{0x2e,0xbb},{0x30,0}})
    mem.store(offset(spell,off),2,value);
  mem.store(offset(spell,0x20),1,0);
  invoke("light_link",{static_cast<Raw>(offset(spell,0x18))});
  auto entry=invoke("res_find",{0x550538,0x550540});
  auto sound=scalar(typed_invoke("sound_playAtPos",{entry,vec(offset(word(mem,offset(spell,0xc)),0x10)),ONE}));
  mem.store(offset(spell,0x54),4,sound);
}
void Model::coneoffrost_run(Address spell, Address pos, Raw facing) {
  auto here=vec(pos);
  auto amplitude=to_int(mul(from_int(s32(stat(word(mem,offset(spell,8)),2,level_byte(mem,spell)))),0x43360b60));
  auto scale=add(mul(from_int(s32(stat(word(mem,offset(spell,8)),1,level_byte(mem,spell)))),PERCENT),ONE);
  auto wide=from_int(amplitude);
  auto angle=[&](Raw reach) {
    auto d=to_int(mul(sub(unit(spell_rand(spell)),HALF),reach));
    return static_cast<Raw>(s16(static_cast<Raw>(s16(facing))+static_cast<Raw>(d)));
  };
  auto rise=[&](Raw factor) { return mul(mul(sub(unit(spell_rand(spell)),HALF),factor),scale); };
  for (unsigned i=0;i<15;++i) {
    auto a=angle(wide);
    auto speed=mul(mul(unit(spell_rand(spell)),0x42940000),scale);
    Vec velocity{mul(cos16(a),speed),mul(sin16(a),speed),rise(0x40c00000)};
    spell_rand(spell);
    typed_invoke("particle_spawn",{Raw(0x8e6090),here,velocity,ZERO});
  }
  auto reach=from_int(to_int(mul(wide,0x3f7ffe7e)));
  for (unsigned i=0;i<6;++i) {
    auto a=angle(reach);
    auto speed=mul(mul(add(mul(unit(spell_rand(spell)),0x3e4ccccc),0x3f666666),0x42700000),scale);
    Vec velocity{mul(cos16(a),speed),mul(sin16(a),speed),rise(0x40800000)};
    typed_invoke("particle_spawn",{Raw(0x8e60e0),here,velocity,ZERO});
  }
  reach=from_int(to_int(mul(wide,0x3fdfded6)));
  if (spell_rand(spell)%5==0) for (unsigned i=0;i<24;++i) {
    auto a=angle(reach); spell_rand(spell);
    Vec velocity{mul(cos16(a),0x42280000),mul(sin16(a),0x42280000),rise(0x41880000)};
    typed_invoke("particle_spawn",{Raw(0x8e6130),here,velocity,ZERO});
  }
  if ((word(mem,creature_hp::G_SIM_FRAME)&7)==0) {
    auto a=angle(wide);
    auto x=mul(cos16(a),0x42940000),y=mul(sin16(a),0x42940000);
    Vec velocity{mul(add(x,x),scale),mul(add(y,y),scale),ZERO};
    auto damage=mul(from_int(s32(stat(word(mem,offset(spell,8)),0,level_byte(mem,spell)))),0x3e088888);
    auto block=invoke("pool_new",{0x74});
    if (block) {
      auto point=local_vec(here),speed=local_vec(velocity);
      dummydamager_ctor(block,point,speed,0x2d,10,0x12,0x80c040,word(mem,offset(spell,0xc)),damage,1);
    }
  }
  auto pulse=to_int(mul(sin16(static_cast<Raw>(s16(word(mem,creature_hp::G_SIM_FRAME)*1000u))),0x40c00000));
  reach=from_int(pulse+0x6c);
  mem.store(offset(spell,0x34),4,reach); mem.store(offset(spell,0x3c),4,reach);
  auto inverse=div(ONE,mul(reach,reach));
  mem.store(offset(spell,0x40),4,inverse); mem.store(offset(spell,0x38),4,inverse);
  invoke("light_relink",{static_cast<Raw>(offset(spell,0x18))});
}
void Model::dummydamager_ctor(Address obj, Address pos, Address velocity, Raw life, Raw radius,
                            Raw bits, Raw flags, Raw owner, Raw damage, Raw extra) {
  auto speed=vec(velocity);
  projectile_ctor(obj,local_vec(vec(pos)),owner,0);
  mem.store(offset(obj,0x48),4,0x50ea28); wvec(offset(obj,0x54),speed);
  mem.store(offset(obj,0x6c),2,life);
  auto before=word(mem,offset(obj,0xc));
  mem.store(offset(obj,0x1c),4,from_int(s32(radius)));
  mem.store(offset(obj,0xc),4,before|0x4008);
  mem.store(offset(obj,0x68),4,damage); mem.store(offset(obj,0x64),4,flags);
  mem.store(offset(obj,0x6e),2,extra); mem.store(offset(obj,0x60),4,bits);
  if (bits&4) mem.store(offset(obj,0xc),4,before|0x4009);
  invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
}
void Model::dummydamager_run(Address obj) {
  auto life=(static_cast<Raw>(mem.load(offset(obj,0x6c),2))-1)&0xffff;
  mem.store(offset(obj,0x6c),2,life);
  if (s16(life)<1) { vcall(obj,1,{3}); return; }
  auto result=obj_move_and_collide(obj,local_vec(vec(offset(obj,0x54))));
  if (!result) return;
  auto bits=word(mem,offset(obj,0x60));
  if (result==1) {
    if (bits&8) {
      auto here=vec(offset(obj,0x10));
      auto number=mul(add(mul(unit(object_rand(obj)),0x40c00000),0x40000000),0x42700000);
      auto block=invoke("pool_new",{0x4ac});
      if (block) object_ctor("StaticFire_ctor",{block,here,static_cast<Raw>(to_int(number)),Raw(0),Raw(0),number,HALF});
      bits=word(mem,offset(obj,0x60));
    }
    if (bits&0x10) {
      for (unsigned j=0;j<0x80;++j) {
        auto b=object_ball(obj); Vec point;
        for (unsigned i=0;i<3;++i) point[i]=add(mul(b[i],0x41200000),word(mem,offset(obj,0x10+4*i)));
        auto w=object_ball(obj); Vec back;
        for (unsigned i=0;i<3;++i) back[i]=mul(word(mem,offset(obj,0x54+4*i)),0x3d4ccccc);
        Vec velocity{sub(mul(w[0],0x41421062),back[0]),sub(mul(w[1],0x41421062),back[1]),
          sub(add(mul(w[2],0x41421062),0x41200000),back[2])};
        typed_invoke("particle_spawn",{Raw(0x4f8760),point,velocity,ZERO});
      }
      bits=word(mem,offset(obj,0x60));
    }
    if (bits&2) { vcall(obj,1,{3}); return; }
  }
  if (bits&1) vcall(obj,1,{3});
}
void Model::dummydamager_on_touch(Address obj, Raw other) {
  if (word(mem,offset(other,0xc))&0x20)
    if (!vcall(other,20,{word(mem,offset(word(mem,offset(obj,0x4c)),0xc))})) return;
  std::vector<std::array<Raw,4>> entries;
  auto bits=word(mem,offset(obj,0x60));
  if (bits&0x18) {
    auto amount=static_cast<Raw>(to_int(word(mem,offset(obj,0x68))));
    entries.push_back({bits&0x10 ? Raw(0x16):Raw(0x15),amount,amount,0});
    mem.store(offset(obj,0x68),4,0);
  }
  auto head=word(mem,offset(obj,0x4c));
  auto attacker=head ? word(mem,offset(head,0xc)):0;
  typed_invoke("damage_apply",{other,attacker,Raw(1000000),Raw(0),Raw(0),word(mem,offset(obj,0x64)),
    descriptor(std::move(entries)),Raw(0),word(mem,offset(obj,0x68)),word(mem,offset(obj,0x68))});
}

// RTA bow family, authored from ref_skills_b.py sections 43--47. No weapon,
// particle, gas, light, resource or damage service is substituted here.
namespace {
struct ArrowInfo { Raw warning, table, lump, launch, hit; };
ArrowInfo arrow_info(unsigned id) {
  switch (id) {
  case 43: return {0x54f278,0x518de0,0x54f2a0,0x54f2a8,0x54f2c0};
  case 44: return {0x54f360,0x518ef8,0x54f388,0x54f390,0x54f3a8};
  case 45: return {0x54f448,0x519010,0x54f488,0x54f490,0x54f4a8};
  case 46: return {0x54f550,0,0x54f560,0x54f568,0};
  case 47: return {0x54f648,0x519240,0x54f670,0x54f678,0x54f6c0};
  default: throw Unsupported("RTA bow skill ID required");
  }
}
void projectile_id(unsigned id) { arrow_info(id); if (id==46) throw Unsupported("Multi Fire delegates MissileWeapon"); }
Raw wrapped_float(std::int64_t value) {
  return from_int(static_cast<Raw>(value));
}
}
Raw Model::bow_can_use(Raw caster, Raw key) {
  if (!(word(mem,offset(caster,0xc))&0x40) || static_cast<std::int8_t>(mem.load(offset(record(caster),0x4a86),1))==4) return 1;
  auto text=invoke("text_lookup",{key});
  invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
  return 0;
}
Raw Model::arrow_can_use(unsigned id, Address, Raw caster) { return bow_can_use(caster,arrow_info(id).warning); }
Raw Model::arrow_cast(unsigned id, Address skill, Raw level, Raw caster, Address pos, Raw facing) {
  projectile_id(id);
  // Explode adjusts before copying position and allocating; the others adjust
  // only after allocation succeeds. A callback may change any subsequent read.
  if (id==47) level=skill_call(skill,14,{level,9});
  auto start=vec(pos); start[2]=sub(start[2],0x41400000);
  auto obj=invoke("pool_new",{id==47 ? Raw(0x74):Raw(0xb0)});
  if (obj) {
    if (id!=47) level=skill_call(skill,14,{level,id==45 ? Raw(1):Raw(20)});
    arrow_ctor(id,obj,local_vec(start),facing,caster,level,static_cast<Raw>(skill));
  }
  return 0;
}
void Model::arrow_light(Address obj, const Vec &pos, Raw radius, Raw falloff, const Vec &colour) {
  mem.store(offset(obj,0x79),1,0); wvec(offset(obj,0xa0),pos);
  for (auto off:{0x94,0x8c}) mem.store(offset(obj,off),4,radius);
  for (auto off:{0x98,0x90}) mem.store(offset(obj,off),4,falloff);
  for (auto off:{0x7a,0x82}) for (unsigned i=0;i<3;++i) mem.store(offset(obj,off+2*i),2,colour[i]);
  mem.store(offset(obj,0x9c),4,0); mem.store(offset(obj,0x80),2,0); mem.store(offset(obj,0x88),2,0);
  mem.store(offset(obj,0x78),1,0); invoke("light_link",{static_cast<Raw>(offset(obj,0x70))});
}
void Model::arrow_velocity_facing(Address obj, Raw speed) {
  auto f=from_int(speed);
  mem.store(offset(obj,0x54),4,mul(cos16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x64),2))))),f));
  mem.store(offset(obj,0x58),4,mul(sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x64),2))))),f));
  mem.store(offset(obj,0x5c),4,0);
}
void Model::arrow_velocity_dir(Address obj, const Vec &direction, Raw speed) {
  auto f=from_int(speed); wvec(offset(obj,0x54),direction);
  auto v=vec(offset(obj,0x54));
  auto inverse=rsqrt(ONE,add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(v[2],v[2])));
  for (unsigned i:{2u,0u,1u}) mem.store(offset(obj,0x54+4*i),4,mul(mul(v[i],inverse),f));
}
bool Model::arrow_blocked_for_player(Raw other) const {
  auto flags=word(mem,offset(other,0xc));
  return (flags&0x1000000) || ((flags&0x20) && (mem.load(offset(other,0xa8),8)&0x4000000));
}
void Model::arrow_common(unsigned id, Address obj, Address pos, Raw owner, Raw skill) {
  projectile_id(id); if (id==47) throw Unsupported("Explode has no common light body");
  auto here=vec(pos); projectile_ctor(obj,local_vec(here),owner,0);
  mem.store(offset(obj,0x48),4,arrow_info(id).table);
  if (id==43) arrow_light(obj,here,0x42c00000,0x38e38e38,{0x6d,100,0x4c});
  else if (id==44) arrow_light(obj,here,0x431c0000,0x382c598a,{0x59,0x59,0x82});
  else arrow_light(obj,here,0x42f00000,0x3891a2b3,{0x3c,0x84,0x44});
  mem.store(offset(obj,0xac),4,skill);
}
Raw Model::firearrow_stats(Address obj, Raw owner) {
  auto skill=word(mem,offset(obj,0xac));
  auto radius=from_int(stat_for(skill,2,word(mem,offset(obj,0x6c)),owner,10));
  mem.store(offset(obj,0x1c),4,radius);
  return stat_for(word(mem,offset(obj,0xac)),1,word(mem,offset(obj,0x6c)),owner,10);
}
void Model::arrow_finish(Address obj, Raw lump, Raw sound) {
  mem.store(offset(obj,0x6c),4,0); mem.store(offset(obj,0x60),4,0);
  invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
  auto resource=invoke("res_find",{lump,sound}); invoke("sound_playOnObject",{resource,static_cast<Raw>(obj),ONE});
}
void Model::arrow_ctor(unsigned id, Address obj, Address pos, Raw facing, Raw owner, Raw level, Raw skill) {
  projectile_id(id);
  if (id==47) {
    projectile_ctor(obj,local_vec(vec(pos)),owner,0);
    mem.store(offset(obj,0x64),2,facing); mem.store(offset(obj,0x48),4,arrow_info(id).table);
    owner=word(mem,offset(word(mem,offset(obj,0x4c)),0xc));
    mem.store(offset(obj,0x1c),4,0x41000000); mem.store(offset(obj,0x70),4,skill); mem.store(offset(obj,0x68),4,level);
    auto speed=stat_for(skill,1,level,owner,10); arrow_velocity_facing(obj,speed);
  } else {
    arrow_common(id,obj,pos,owner,skill); mem.store(offset(obj,0x64),2,facing);
    if (id==43) {
      mem.store(offset(obj,0x6c),4,level); auto speed=firearrow_stats(obj,owner); arrow_velocity_facing(obj,speed);
      mem.store(offset(obj,0x68),4,0); mem.store(offset(obj,0x60),4,0);
      invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
      auto info=arrow_info(id); auto resource=invoke("res_find",{info.lump,info.launch});
      invoke("sound_playOnObject",{resource,static_cast<Raw>(obj),ONE}); return;
    }
    mem.store(offset(obj,0x1c),4,id==44 ? 0x40e00000:0x40a00000); mem.store(offset(obj,0x68),4,level);
    auto speed=stat_for(skill,1,level,owner,10); arrow_velocity_facing(obj,speed);
  }
  auto info=arrow_info(id); arrow_finish(obj,info.lump,info.launch);
}
void Model::arrow_ctor2(unsigned id, Address obj, Address pos, Address dir, Raw owner, Raw level, Raw skill) {
  projectile_id(id); auto direction=vec(dir);
  if (id==47) {
    projectile_ctor(obj,local_vec(vec(pos)),owner,0);
    mem.store(offset(obj,0x48),4,arrow_info(id).table); mem.store(offset(obj,0x70),4,skill);
  } else arrow_common(id,obj,pos,owner,skill);
  auto facing=static_cast<Raw>(atan2_16(direction[1],direction[0]));
  mem.store(offset(obj,id==43 ? 0x6c:0x68),4,level); mem.store(offset(obj,0x64),2,facing);
  Raw speed;
  if (id==43) speed=firearrow_stats(obj,owner);
  else {
    if (id==47) owner=word(mem,offset(word(mem,offset(obj,0x4c)),0xc));
    mem.store(offset(obj,0x1c),4,id==44 ? 0x40e00000:id==45 ? 0x40a00000:0x41000000);
    speed=stat_for(word(mem,offset(obj,id==47 ? 0x70:0xac)),1,level,owner,10);
  }
  arrow_velocity_dir(obj,direction,speed);
  auto info=arrow_info(id);
  if (id==43) {
    mem.store(offset(obj,0x68),4,0); mem.store(offset(obj,0x60),4,0); invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
    auto resource=invoke("res_find",{info.lump,info.launch}); invoke("sound_playOnObject",{resource,static_cast<Raw>(obj),ONE});
  } else arrow_finish(obj,info.lump,info.launch);
}
std::array<Raw,2> Model::monster_spread(Raw amount, Raw value) {
  auto base=from_int(amount); auto extra=to_int(mul(mul(base,0x3e4ccccc),unit(value)));
  return {base,wrapped_float(std::int64_t(s32(amount))+extra)};
}
void Model::flight_particle(Address obj, Raw definition, Raw speed, Raw reach) {
  auto velocity=object_ball(obj); for (auto &v:velocity) v=mul(v,speed);
  auto w=object_ball(obj); auto radius=word(mem,offset(obj,0x1c)); Vec point{};
  for (unsigned i=0;i<3;++i) point[i]=add(word(mem,offset(obj,0x10+4*i)),mul(mul(w[i],radius),reach));
  typed_invoke("particle_spawn",{definition,point,velocity,ZERO});
}
void Model::arrow_particles(unsigned id, Address obj) {
  projectile_id(id); if (id==43) throw Unsupported("Fire uses camera puff");
  if (id!=47) {
    wvec(offset(obj,0xa0),vec(offset(obj,0x10))); invoke("light_relink",{static_cast<Raw>(offset(obj,0x70))});
    if (id==44) {
      for (unsigned i=0;i<7;++i) flight_particle(obj,0x8e5730,0x41700000,0x3e800000);
      flight_particle(obj,0x8e5780,0x41600000,0x3e800000);
    } else for (unsigned i=0;i<46;++i) flight_particle(obj,i&1 ? 0x8e5820:0x8e5870,0x41f00000,0x3f020bf5);
  } else for (unsigned n=0;n<6;++n) {
    auto velocity=object_ball(obj); for (auto &v:velocity) v=mul(v,0x40400000);
    auto step=vec(offset(obj,0x54)); for (auto &v:step) v=mul(v,DT);
    auto along=unit(object_rand(obj)); Vec point{};
    for (unsigned i=0;i<3;++i) point[i]=add(word(mem,offset(obj,0x10+4*i)),mul(step[i],along));
    typed_invoke("particle_spawn",{Raw(0x8e5960),point,velocity,ZERO});
  }
}
void Model::arrow_flight(unsigned id, Address obj, Raw terrain_reason) {
  projectile_id(id); if (id==43) throw Unsupported("Fire has a distinct flight body");
  arrow_flight(obj,[&](Address o,Raw r){arrow_end(id,o,r);},[&](Address o){arrow_particles(id,o);},terrain_reason);
}
void Model::arrow_flight(Address obj, const std::function<void(Address,Raw)> &end,
                         const std::function<void(Address)> &particles, Raw terrain_reason) {
  if (word(mem,offset(obj,0x60))) return;
  mem.store(offset(obj,0x6c),4,word(mem,offset(obj,0x6c))+1);
  auto result=obj_move_and_collide(obj,local_vec(vec(offset(obj,0x54))));
  if (result&1) { if (!end) throw Unsupported("missing arrow end callback"); end(obj,terrain_reason); return; }
  if (s32(word(mem,offset(obj,0x6c)))>=601) { if (!end) throw Unsupported("missing arrow end callback"); end(obj,0); return; }
  if (!particles) throw Unsupported("missing arrow particle callback");
  particles(obj);
}
void Model::arrow_run(unsigned id, Address obj) {
  projectile_id(id); if (id!=43) { arrow_flight(id,obj); return; }
  if (word(mem,offset(obj,0x60))) return;
  mem.store(offset(obj,0x68),4,word(mem,offset(obj,0x68))+1);
  auto result=obj_move_and_collide(obj,local_vec(vec(offset(obj,0x54))));
  if (result&1) { arrow_end(id,obj,result&1); return; }
  if (s32(word(mem,offset(obj,0x68)))>=601) { arrow_end(id,obj,0); return; }
  auto radius=word(mem,offset(obj,0x1c)), size=add(mul(mul(radius,radius),0x3cb42246),0x3e88b439);
  auto step=vec(offset(obj,0x54)); for (auto &v:step) v=mul(v,DT);
  typed_invoke("fx_impactPuffNearCamera",{vec(offset(obj,0x10)),step,Raw(1),radius,size});
  wvec(offset(obj,0xa0),vec(offset(obj,0x10))); invoke("light_relink",{static_cast<Raw>(offset(obj,0x70))});
}
void Model::arrow_on_hit(unsigned id, Address obj, Raw other) {
  projectile_id(id); if (id==47) { arrow_end(id,obj,2); return; }
  if (!word(mem,offset(obj,0x4c))) return;
  auto skill=word(mem,offset(obj,0xac));
  if (id==43 || id==45) {
    auto amount=skill_call(skill,13,{0,word(mem,offset(obj,id==43 ? 0x6c:0x68)),static_cast<Raw>(offset(obj,0x4c)),0});
    auto value=object_rand(obj); std::array<Raw,2> damage{}; std::int32_t fire_extra=0;
    if (id==43) { damage[0]=from_int(amount); fire_extra=to_int(mul(mul(damage[0],0x3e4ccccc),unit(value))); } // calculated even on player/rejected paths
    auto owner=handle_object(offset(obj,0x4c));
    if (!(word(mem,offset(owner,0xc))&0x40)) {
      if (!vcall(owner,20,{other})) return;
      if (id==45) damage=monster_spread(amount,value);
      else damage[1]=wrapped_float(std::int64_t(s32(amount))+fire_extra);
      Argument desc=Raw(0); if (id==43) desc=descriptor({{0x15,amount,amount,0}});
      typed_invoke("damage_apply",{other,handle_object(offset(obj,0x4c)),Raw(100000),Raw(0),Raw(0),id==43 ? Raw(0x60):Raw(0x10040),desc,Raw(0),damage[0],damage[1]});
    } else {
      if (arrow_blocked_for_player(other) || !vcall(owner,20,{other})) return;
      // Poison re-reads the owner before level; Fire reads it after descriptor.
      if (id==45) owner=handle_object(offset(obj,0x4c));
      auto lv=s32(word(mem,offset(obj,id==43 ? 0x6c:0x68)));
      Raw flags=id==43 ? (lv>=16 ? 0x200060:lv>10 ? 0x80060:lv>5 ? 0x40060:0x60) : (lv>15 ? 0x90040:lv>10 ? 0x50040:0x10040);
      Argument desc=Raw(0); if (id==43) { desc=descriptor({{0x1f,100,amount,amount}}); owner=handle_object(offset(obj,0x4c)); }
      auto rec=record(owner);
      auto low=wrapped_float(std::int64_t(s32(word(mem,offset(rec,0xd0))))+(id==45 ? s32(amount):0));
      auto high=wrapped_float(std::int64_t(s32(word(mem,offset(rec,0xd4))))+(id==45 ? s32(amount):0));
      typed_invoke("damage_apply",{other,owner,Raw(100000),Raw(0),Raw(0),flags,desc,Raw(0),low,high});
    }
    if (id==45 && (word(mem,offset(other,0xc))&0x20)) {
      auto seconds=stat_for(word(mem,offset(obj,0xac)),3,word(mem,offset(obj,0x68)),handle_object(offset(obj,0x4c)),10);
      invoke("Creature_setEffectTimer",{other,3,handle_object(offset(obj,0x4c)),from_int(seconds)});
    }
  } else {
    auto owner=handle_object(offset(obj,0x4c));
    if (word(mem,offset(owner,0xc))&0x40) {
      if (arrow_blocked_for_player(other) || !vcall(owner,20,{other})) return;
      typed_invoke("fx_spawn",{Raw(10),vec(offset(obj,0x10))});
      auto lv=word(mem,offset(obj,0x68)); auto signed_lv=s32(lv);
      Raw flags=signed_lv>=16 ? 0x80040:signed_lv>10 ? 0x40040:0x40;
      auto a=skill_call(skill,13,{2,lv,static_cast<Raw>(offset(obj,0x4c)),0});
      auto b=skill_call(word(mem,offset(obj,0xac)),13,{2,word(mem,offset(obj,0x68)),static_cast<Raw>(offset(obj,0x4c)),0});
      owner=handle_object(offset(obj,0x4c)); auto rec=record(owner);
      typed_invoke("damage_apply",{other,owner,Raw(100000),Raw(0),Raw(0),flags,descriptor({{0x20,100,a,b}}),Raw(0),from_int(word(mem,offset(rec,0xd0))),from_int(word(mem,offset(rec,0xd4)))});
    } else {
      if (!vcall(owner,20,{other})) return;
      typed_invoke("fx_spawn",{Raw(10),vec(offset(obj,0x10))});
      auto amount=skill_call(skill,13,{0,word(mem,offset(obj,0x68)),static_cast<Raw>(offset(obj,0x4c)),0});
      auto damage=monster_spread(amount,object_rand(obj));
      typed_invoke("damage_apply",{other,handle_object(offset(obj,0x4c)),Raw(100000),Raw(0),Raw(0),Raw(0x40),Raw(0),Raw(0),damage[0],damage[1]});
    }
    if (word(mem,offset(other,0xc))&0x20) {
      auto seconds=s32(skill_call(word(mem,offset(obj,0xac)),13,{3,word(mem,offset(obj,0x68)),static_cast<Raw>(offset(obj,0x4c)),10}));
      invoke("Creature_setEffectTimer",{other,1,handle_object(offset(obj,0x4c)),add(word(mem,offset(other,0x344)),from_int(static_cast<Raw>(seconds/2)))});
    }
  }
  arrow_end(id,obj,2);
}
void Model::arrow_end(unsigned id, Address obj, Raw reason) {
  projectile_id(id); if (word(mem,offset(obj,0x60))) return;
  auto info=arrow_info(id);
  if (id==43) {
    if (reason) { auto res=invoke("res_find",{info.lump,info.hit}); invoke("sound_playOnObject",{res,static_cast<Raw>(obj),ONE}); }
  } else if (id==44 && reason) {
    if (s32(reason)==1) {
      auto radius=word(mem,offset(obj,0x1c));
      auto bx=add(word(mem,offset(obj,0x10)),mul(cos16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x64),2))))),radius));
      auto by=add(word(mem,offset(obj,0x14)),mul(sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x64),2))))),word(mem,offset(obj,0x1c))));
      auto bz=word(mem,offset(obj,0x18)); std::size_t steps=0;
      for (std::int64_t count=0;count<std::int64_t(s32(word(mem,offset(obj,0x68))))*10+27;++count) {
        walk_step(steps); auto v=object_ball(obj);
        auto along=add(mul(v[0],word(mem,offset(obj,0x54))),mul(v[1],word(mem,offset(obj,0x58))));
        auto x=v[0], y=v[1]; if (lt(0x3f34fdf3,along)) {x=neg(x);y=neg(y);}
        auto k=from_int(static_cast<Raw>(s32(word(mem,offset(obj,0x68)))/3+2));
        typed_invoke("particle_spawn",{Raw(0x8e57d0),Vec{add(bx,mul(x,k)),add(by,mul(y,k)),add(bz,mul(v[2],k))},Vec{0,0,0},ZERO});
      }
    }
    std::size_t steps=0;
    for (Raw index=0;lt(from_int(index),mul(wrapped_float(std::int64_t(s32(word(mem,offset(obj,0x68))))*20),HALF));++index) {
      walk_step(steps); auto velocity=object_ball(obj); for (auto &v:velocity) v=mul(v,0x41f80000);
      if (lt(0x3f34fdf3,add(mul(velocity[0],word(mem,offset(obj,0x54))),mul(velocity[1],word(mem,offset(obj,0x58)))))) { velocity[0]=neg(velocity[0]);velocity[1]=neg(velocity[1]); }
      object_ball(obj); // Required discarded second sample.
      typed_invoke("particle_spawn",{Raw(0x8e5780),vec(offset(obj,0x10)),velocity,ZERO});
      if (!(index&3)) { for (auto &v:velocity) v=mul(v,0x3f400000); typed_invoke("particle_spawn",{Raw(0x8e5730),vec(offset(obj,0x10)),velocity,ZERO}); }
    }
    auto res=invoke("res_find",{info.lump,info.hit}); invoke("sound_playOnObject",{res,static_cast<Raw>(obj),ONE});
  } else if (id==45) {
    for (unsigned angle=0;angle<65536;angle+=0x135) {
      Vec velocity{mul(cos16(angle),0x41100000),mul(sin16(angle),0x41100000),0};
      typed_invoke("particle_spawn",{Raw(0x8e58c0),vec(offset(obj,0x10)),velocity,ZERO});
    }
    if (word(mem,offset(obj,0x4c))) {
      auto cloud=invoke("pool_new",{0x70});
      if (cloud) {
        auto radius=stat(word(mem,offset(obj,0xac)),2,word(mem,offset(obj,0x68)));
        auto life=stat(word(mem,offset(obj,0xac)),3,word(mem,offset(obj,0x68)));
        ++draws;
        typed_invoke("poisonGasSpell_ctor",{cloud,vec(offset(obj,0x10)),life*60u,radius*12u,Raw(0),handle_object(offset(obj,0x4c)),Raw(5)});
      }
    }
    auto res=invoke("res_find",{info.lump,info.hit}); invoke("sound_playOnObject",{res,static_cast<Raw>(obj),ONE});
  } else if (id==47 && reason) {
    auto here=vec(offset(obj,0x10)); auto pick=(object_rand(obj)&3)+1;
    bool ice=word(mem,0x44fe08)&0x40000;
    auto name=typed_invoke("libc_sprintf",{ice ? Raw(0x54f690):Raw(0x54f6a8),pick});
    typed_invoke("fx_spawn",{ice ? Raw(8):Raw(2),here});
    auto text=std::get_if<std::string>(&name); if (!text) throw Unsupported("sprintf callback requires string return");
    auto res=scalar(typed_invoke("res_find",{info.lump,*text})); invoke("sound_playOnObject",{res,static_cast<Raw>(obj),ONE});
    res=invoke("res_find",{info.lump,info.hit}); invoke("sound_playOnObject",{res,static_cast<Raw>(obj),ONE});
    typed_invoke("fx_ringBurst",{here,fmax(ONE,mul(from_int(word(mem,offset(obj,0x68))),0x3dcccccc))});
    auto light=invoke("pool_new",{0xbc}); if (light) light_effect(light,here,0x437f0000,0x41200000,0x41200000,0x42f00000,0x42700000,HALF,0x3e800000);
    auto lv=s32(word(mem,offset(obj,0x68))); invoke("camera_shake",{0,from_int(static_cast<Raw>(lv/2))});
    if (word(mem,offset(obj,0x4c))) {
      auto skill=word(mem,offset(obj,0x70));
      auto edge=from_int(stat_for(skill,2,word(mem,offset(obj,0x68)),handle_object(offset(obj,0x4c)),10));
      lv=s32(word(mem,offset(obj,0x68))); Raw knock=0;
      if (lv>=6) {
        knock=lv<12 ? 0x40000:0x80000;
        if (!(word(mem,offset(handle_object(offset(obj,0x4c)),0xc))&0x40) && lv>=12 && (object_rand(obj)&1)) knock=0x40000;
      }
      auto amount=stat_for(word(mem,offset(obj,0x70)),0,word(mem,offset(obj,0x68)),handle_object(offset(obj,0x4c)),0);
      auto p=vec(offset(obj,0x10)); Vec low{},high{};
      for (unsigned i=0;i<3;++i) {low[i]=sub(p[i],edge);high[i]=add(p[i],edge);}
      auto result=typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x80)});
      auto objects=std::get_if<std::vector<Raw>>(&result); if (!objects) throw Unsupported("box query requires object list");
      if (objects->size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
      for (auto other:*objects) {
        auto owner=handle_object(offset(obj,0x4c));
        if (other!=owner) {
          if ((word(mem,offset(other,0xc))&0x20) && !vcall(owner,20,{other})) continue;
          auto damage=monster_spread(amount,object_rand(obj));
          typed_invoke("damage_apply",{other,static_cast<Raw>(obj),Raw(100000),Raw(0),Raw(0),knock,Raw(0),Raw(0),ZERO,ZERO});
          typed_invoke("damage_apply",{other,handle_object(offset(obj,0x4c)),Raw(100000),Raw(0),Raw(0),Raw(0x840),Raw(0),Raw(0),damage[0],damage[1]});
        } else {
          auto dx=sub(word(mem,offset(other,0x10)),word(mem,offset(obj,0x10)));
          auto dy=sub(word(mem,offset(other,0x14)),word(mem,offset(obj,0x14)));
          auto dz=sub(word(mem,offset(other,0x18)),word(mem,offset(obj,0x18)));
          auto distance=sqrt(add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz)));
          if (lt(distance,mul(edge,HALF))) typed_invoke("damage_apply",{other,static_cast<Raw>(obj),Raw(100000),Raw(0),Raw(0),knock,Raw(0),Raw(0),ZERO,ZERO});
        }
      }
    }
  }
  invoke("obj_delete",{static_cast<Raw>(obj)}); mem.store(offset(obj,0x60),4,1);
}
Raw Model::multifire_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing) {
  auto angle0=s16(facing); auto here=vec(pos);
  auto lv=skill_call(skill,14,{level,20}); auto light=invoke("pool_new",{0xbc});
  if (light) light_effect(light,{add(here[0],ZERO),here[1],add(here[2],0x41a00000)},0x42f80000,0x42e60000,0x434c0000,0x43021771,0x42700000,HALF,ONE);
  auto count=stat_for(skill,2,lv,caster,10); auto fcount=from_int(count);
  auto angle=s16(static_cast<Raw>(std::int64_t(angle0)-to_int(mul(mul(fcount,HALF),0x4405a000))));
  auto speed=from_int(stat_for(skill,1,lv,caster,10)); Raw rating=0,flags=0,item_flags=0,low,high;
  if (word(mem,offset(caster,0xc))&0x40) {
    auto rec=record(caster); auto set=static_cast<std::int8_t>(mem.load(offset(rec,0x4a7c),1));
    auto bow=word(mem,offset(rec,static_cast<Address>(0x4434+4*set)));
    auto share=div(mul(from_int(stat_for(skill,0,level,caster,10)),PERCENT),fcount);
    rec=record(caster);
    low=static_cast<Raw>(round_half_away(mul(from_int(word(mem,offset(rec,0xd0))),share)));
    high=static_cast<Raw>(round_half_away(mul(from_int(word(mem,offset(rec,0xd4))),share)));
    flags=0x40000; rating=invoke("item_getModifiedField1a",{bow}); item_flags=word(mem,offset(bow,8));
  } else {
    low=stat_for(skill,0,level,caster,10); auto spread=mul(from_int(low),0x3e4ccccc);
    auto probe=mul(spread,unit(cosmetic_draw())); auto extra=mul(spread,unit(cosmetic_draw()));
    auto delta=to_int(le(ZERO,probe) ? add(extra,HALF):sub(extra,HALF));
    auto sum=std::int64_t(s32(low))+delta;
    high=static_cast<Raw>(sum);
  }
  std::size_t steps=0;
  for (std::int64_t i=0;i<s32(count);++i) {
    walk_step(steps); Vec velocity{mul(cos16(static_cast<Raw>(angle)),speed),mul(sin16(static_cast<Raw>(angle)),speed),0};
    auto dip=mul(unit(draw()),0x40c00000); Vec start{here[0],here[1],add(sub(here[2],dip),0xc0c00000)};
    auto arrow=invoke("pool_new",{0x1c8});
    if (arrow) {
      ++draws;
      typed_invoke("MissileWeapon_ctor",{arrow,Raw(0),word(mem,0x3e9ce8),word(mem,0x3e9cec),start,velocity,static_cast<Raw>(angle-0x4000),caster,Raw(0),Raw(0),low,high,rating,item_flags,Raw(10000),flags,Raw(0)});
    }
    angle=s16(static_cast<Raw>(angle+0x42d));
  }
  auto resource=invoke("res_find",{0x54f560,0x54f568}); invoke("sound_playOnObject",{resource,caster,ONE}); return 0;
}
// RTA Shock: documented direct GameObject lifetime and depth-first victim chain.
Raw Model::shock_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing) {
  auto obj=invoke("pool_new",{0x198});
  if (obj) shock_ctor(obj,local_vec(vec(pos)),static_cast<Raw>(s16(facing)),caster,skill,level);
  return 0;
}
void Model::shock_ctor(Address obj, Address pos, Raw facing, Raw caster, Raw skill, Raw level) {
  auto start=vec(pos);
  auto w=[&](Raw off,Raw v){mem.store(offset(obj,off),4,v);};
  auto h=[&](Raw off,Raw v){mem.store(offset(obj,off),2,v);};
  auto b=[&](Raw off,Raw v){mem.store(offset(obj,off),1,v);};
  game_object_ctor(obj,pos,0,0);
  w(0x50,0); w(0x54,0); w(0x48,0x50e228); b(0x155,0);
  wvec(offset(obj,0x17c),start);
  for (auto pair:std::array<std::array<Raw,2>,5>{{{0x174,0x382c598a},{0x16c,0x382c598a},{0x170,0x431c0000},{0x168,0x431c0000},{0x178,0}}}) w(pair[0],pair[1]);
  for (auto pair:std::array<std::array<Raw,2>,8>{{{0x160,0xe1},{0x164,0xffe0},{0x158,0xe1},{0x15c,0xffe0},{0x162,0x140},{0x15a,0x140},{0x156,0},{0x15e,0}}}) h(pair[0],pair[1]);
  b(0x154,0); invoke("light_link",{static_cast<Raw>(offset(obj,0x14c))}); w(0x194,0);
  auto sound=invoke("res_find",{0x54e6a0,0x54e6a8}); invoke("sound_playOnObject",{sound,caster,ONE});
  h(0x38,facing); w(0xc,word(mem,offset(obj,0xc))|1); w(0x4c,skill);
  w(0x58,word(mem,creature_hp::G_SIM_FRAME)); b(0x188,level); w(0x1c,0x41400000);
  if (caster) handle_link(offset(obj,0x194),caster,caster);
  b(0x189,0); w(0x190,0x43aa8000);
  auto angle=static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x38),2))));
  auto c=cos16(angle),s=sin16(angle); w(0x128,0);
  auto inverse=rsqrt(ONE,add(mul(c,c),mul(s,s)));
  w(0x124,mul(s,inverse));
  w(0x120,mul(mul(c,inverse),mul(word(mem,offset(obj,0x190)),DT)));
  w(0x124,mul(word(mem,offset(obj,0x124)),mul(word(mem,offset(obj,0x190)),DT)));
  auto most=skill_call(word(mem,offset(obj,0x4c)),13,{1,signed_byte(mem,offset(obj,0x188)),static_cast<Raw>(offset(obj,0x194)),10});
  b(0x18a,most+1); invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
  sound=invoke("res_find",{0x54e6a0,0x54e6a8}); invoke("sound_playOnObject",{sound,static_cast<Raw>(obj),ONE});
}
Raw Model::shock_in_chain(Address obj, Raw other) {
  auto values=list_objects(offset(obj,0x54));
  return std::find(values.begin(),values.end(),other)!=values.end();
}
void Model::shock_build_chain(Address obj, Address origin) {
  if (depth_>=64) throw Unsupported("Shock chain recursion limit");
  struct Guard { unsigned &d; Guard(unsigned &v):d(v){++d;} ~Guard(){--d;} } guard(depth_);
  if (signed_byte(mem,offset(obj,0x18b))==signed_byte(mem,offset(obj,0x18a))) return;
  Raw best=0,best_distance=0x4cbeb25c;
  for (auto other:list_objects(offset(obj,0x50))) {
    if (!other || (word(mem,offset(other,0xc))&8)) continue;
    auto dy=sub(word(mem,offset(other,0x14)),word(mem,offset(origin,4)));
    auto dx=sub(word(mem,offset(other,0x10)),word(mem,origin));
    auto distance=add(mul(dx,dx),mul(dy,dy));
    if (lt(distance,word(mem,offset(obj,0x18c))) && lt(distance,best_distance) && !shock_in_chain(obj,other)) {
      best=other; best_distance=distance;
    }
  }
  if (!best) return;
  handle_link(offset(obj,0x54),best,best);
  if (signed_byte(mem,offset(obj,0x18a))) {
    mem.store(offset(obj,0x18b),1,static_cast<Raw>(mem.load(offset(obj,0x18b),1))+1);
    shock_build_chain(obj,offset(best,0x10));
    shock_build_chain(obj,origin);
  }
}
Raw Model::shock_wants(Address obj, Raw other, Raw target) {
  auto node=word(mem,offset(obj,0x194));
  if (node) return vcall(word(mem,offset(node,0xc)),20,{other})!=0;
  auto flags=word(mem,offset(other,0xc));
  return (flags&0x20) && !(flags&0xd) && other!=obj && (!target || other!=target);
}
void Model::shock_run(Address obj) {
  auto r=[&](Raw off){return word(mem,offset(obj,off));};
  auto w=[&](Raw off,Raw v){mem.store(offset(obj,off),4,v);};
  auto b=[&](Raw off,Raw v){mem.store(offset(obj,off),1,v);};
  auto state=[&](){return s32(signed_byte(mem,offset(obj,0x189)));};
  auto frame=[&](){return word(mem,creature_hp::G_SIM_FRAME);};
  auto remaining=s32(r(0x58)-frame()+300u);
  invoke("sound_setVolume",{r(0x5c),ONE});
  w(0x10,add(r(0x10),r(0x120))); w(0x14,add(r(0x14),r(0x124)));
  invoke("obj_updateGridCell",{static_cast<Raw>(obj)});
  if (state()==0) {
    bool stopped=false;
    if (!eq(r(0x190),ZERO)) {
      auto ground=scalar(typed_invoke("terrain_height",{word(mem,0x44fdc8),vec(offset(obj,0x10)),Raw(0x40c00000)}));
      if (!le(ground,r(0x18))) {
        stopped=true; w(0x124,0); b(0x189,2); w(0x190,0); w(0x120,0); w(0x58,frame()-0x10e);
        auto sound=invoke("res_find",{0x54e6a0,0x54e6c0}); invoke("sound_playOnObject",{sound,static_cast<Raw>(obj),ONE});
      }
    }
    auto box=[&](const Vec &point,Raw radius) {
      Vec low{},high{}; for (unsigned i=0;i<3;++i) {low[i]=sub(point[i],radius); high[i]=add(point[i],radius);}
      return query_objects(typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x200)}));
    };
    if (!stopped && !(frame()&7)) {
      auto found=box(vec(offset(obj,0x10)),0x41900000); Raw target=0;
      for (auto other:found) if (shock_wants(obj,other)) {
        auto dy=sub(r(0x14),word(mem,offset(other,0x14))),dx=sub(r(0x10),word(mem,offset(other,0x10)));
        if (lt(sqrt(add(mul(dx,dx),mul(dy,dy))),add(word(mem,offset(other,0x1c)),0x41900000))) {target=other;break;}
      }
      if (target) {
        found=box(vec(offset(target,0x10)),0x44960000);
        for (auto other:found) if (shock_wants(obj,other,target)) handle_link(offset(obj,0x50),other,other);
        handle_link(offset(obj,0x50),target,target); b(0x18b,0); w(0x18c,0x472de900);
        shock_build_chain(obj,offset(target,0x10));
        auto amount=skill_call(r(0x4c),13,{0,signed_byte(mem,offset(obj,0x188)),static_cast<Raw>(offset(obj,0x194)),10});
        auto extra=static_cast<Raw>(to_int(mul(mul(from_int(amount),0x3e4ccccc),unit(draw()))));
        auto desc=descriptor({{0x18,amount,amount+extra,0}});
        for (auto victim:list_objects(offset(obj,0x54))) if (victim) {
          auto node=r(0x194),attacker=node?word(mem,offset(node,0xc)):static_cast<Raw>(obj);
          typed_invoke("damage_apply",{victim,attacker,Raw(100000),Raw(0),Raw(0),Raw(0x40),desc,Raw(0),ZERO,ZERO});
        }
        w(0x190,0); b(0x189,1); w(0x120,0); w(0x124,0); w(0x58,frame()-0x10e);
        auto count=s32(signed_byte(mem,offset(obj,0x18b)));
        for (std::int32_t k=0;k<count;++k) w(0x12c+4*static_cast<Raw>(k),UINT32_MAX);
      }
    }
    Raw fade=0;
    if (le(from_int(static_cast<Raw>(remaining)),0x41f00000)) fade=0xb4u-static_cast<Raw>(remaining)*6;
    wvec(offset(obj,0x17c),vec(offset(obj,0x10)));
    auto wave=static_cast<Raw>(to_int(mul(sin16(static_cast<Raw>(s16(frame()*6000))),0x41200000)));
    auto reach=from_int(wave-fade+0x60); w(0x168,reach); w(0x170,reach);
    auto inverse=div(ONE,mul(reach,reach)); w(0x174,inverse); w(0x16c,inverse);
    invoke("light_relink",{static_cast<Raw>(offset(obj,0x14c))});
  }
  if (state()==1) {
    auto node=r(0x54); Raw k=0; std::size_t visits=0;
    while (node && word(mem,node)) {
      walk_step(visits);
      auto victim=word(mem,offset(node,0xc)),after=word(mem,offset(word(mem,node),0xc));
      if (victim && after) {
        auto slot=offset(obj,0x12c+4*k);
        Vec a{add(word(mem,offset(victim,0x10)),ZERO),word(mem,offset(victim,0x14)),add(word(mem,offset(victim,0x18)),0x42100000)};
        Vec c{add(word(mem,offset(after,0x10)),ZERO),word(mem,offset(after,0x14)),add(word(mem,offset(after,0x18)),0x42100000)};
        if (word(mem,slot)==UINT32_MAX) mem.store(slot,4,scalar(typed_invoke("beam_create",{a,c,Raw(0x104),Raw(4),Raw(0x5e808080),Raw(0x41800000),Raw(0x41080000)})));
        else typed_invoke("beam_update",{word(mem,slot),a,c,Raw(0x80),Raw(UINT32_MAX)});
      }
      node=word(mem,node); ++k;
    }
  }
  for (unsigned k=0;k<42;++k) {
    auto ball=object_ball(obj);
    auto inverse=rsqrt(ONE,add(add(mul(ball[0],ball[0]),mul(ball[1],ball[1])),mul(ball[2],ball[2])));
    Vec point{}; for (unsigned i=0;i<3;++i) point[i]=add(r(0x10+4*i),mul(mul(ball[i],inverse),0x41880000));
    auto velocity=object_ball(obj);
    typed_invoke("particle_spawn",{Raw(0x8e4bf0),point,velocity,ZERO});
    if (!(object_rand(obj)&0x1f)) typed_invoke("particle_spawn",{Raw(0x8e4c40),point,velocity,ZERO});
  }
  if (remaining<1) vcall(obj,1,{3});
}

// Frost Storm and Ice Shards: raw-pattern arithmetic, original store/call order.
Raw Model::down(Raw value) noexcept {
  return (value & 0x7fffffffu) ? value | 0x80000000u : 0;
}
Raw Model::c_rem(Raw dividend, Raw divisor) noexcept {
  auto a=std::int64_t(s32(dividend)), b=std::int64_t(s32(divisor));
  return b ? static_cast<Raw>(a % b) : dividend;
}
Raw Model::froststorm_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing) {
  auto centre=vec(pos); auto angle=static_cast<Raw>(s16(facing));
  level=skill_call(skill,14,{level,20});
  if (word(mem,offset(caster,0xc)) & 0x40) {
    Vec ahead{mul(mul(cos16(angle),0x40400000),0x41400000),mul(mul(sin16(angle),0x40400000),0x41400000),0};
    for (unsigned i=0;i<3;++i) centre[i]=add(centre[i],ahead[i]);
  }
  auto damage=skill_call(skill,12,{0,level,caster,0});
  Raw radius=skill_call(skill,12,{1,level,caster,10})*12u;
  Raw frames=skill_call(skill,12,{2,level,caster,20})*60u;
  auto speed=from_int(skill_call(skill,12,{3,level,caster,10}));
  Vec drift{mul(cos16(angle),speed),mul(sin16(angle),speed),0};
  if (!(word(mem,offset(caster,0xc)) & 0x40) && s32(radius)>180) radius=180;
  auto obj=invoke("pool_new",{0xac});
  if (obj) {
    auto here=local_vec(centre), velocity=local_vec(drift);
    froststorm_ctor(obj,here,frames,damage,caster,velocity,from_int(radius));
  }
  auto sound=invoke("res_find",{0x550838,0x550840});
  invoke("sound_playOnObject",{sound,caster,ONE}); return 0;
}
void Model::froststorm_ctor(Address obj, Address pos, Raw frames, Raw damage, Raw caster, Address drift, Raw radius) {
  auto start=vec(pos), speed=vec(drift);
  auto w=[&](Raw off,Raw value){mem.store(offset(obj,off),4,value);};
  auto h=[&](Raw off,Raw value){mem.store(offset(obj,off),2,value);};
  auto b=[&](Raw off,Raw value){mem.store(offset(obj,off),1,value);};
  game_object_ctor(obj,local_vec(start),0,0);
  auto reach=fmax(0x429e0000,fmin(0x43960000,mul(radius,0x3fdba483)));
  auto inverse=div(ONE,mul(reach,reach));
  w(0x6c,0); w(0x48,0x50d3e0); b(0x79,0); wvec(offset(obj,0xa0),start);
  for (auto p:std::array<std::array<Raw,2>,5>{{{0x9c,0x3f333333},{0x8c,reach},{0x94,reach},{0x98,inverse},{0x90,inverse}}}) w(p[0],p[1]);
  for (auto p:std::array<std::array<Raw,2>,8>{{{0x88,0x96},{0x7a,0},{0x7c,0x80},{0x7e,0xff},{0x80,0x96},{0x82,0},{0x84,0x80},{0x86,0xff}}}) h(p[0],p[1]);
  b(0x78,0); invoke("light_link",{static_cast<Raw>(offset(obj,0x70))});
  w(0x54,radius); w(0xc,word(mem,offset(obj,0xc))|1);
  if ((word(mem,offset(caster,0xc)) & 0x60)==0x20) {
    auto begin=static_cast<Raw>(to_int(add(from_int(word(mem,0x44fdcc)),0x4221ffff)));
    w(0x50,begin); w(0x4c,begin+frames);
    auto light=invoke("pool_new",{0xbc});
    if (light) {
      Vec spot{add(start[0],ZERO),start[1],add(start[2],0x41400000)};
      auto wide=fmin(0x43960000,add(word(mem,offset(obj,0x54)),word(mem,offset(obj,0x54))));
      game_object_ctor(light,local_vec(spot),0,0);
      auto lw=[&](Raw off,Raw value){mem.store(offset(light,off),4,value);};
      lw(0x48,0x50e198); inverse=div(ONE,mul(wide,wide));
      mem.store(offset(light,0x55),1,0); wvec(offset(light,0x7c),spot);
      for (auto p:std::array<std::array<Raw,2>,5>{{{0x78,0x3f266666},{0x74,inverse},{0x6c,inverse},{0x68,wide},{0x70,wide}}}) lw(p[0],p[1]);
      for (auto p:std::array<std::array<Raw,2>,8>{{{0x64,100},{0x5c,100},{0x60,0x80},{0x62,0xff},{0x56,0},{0x58,0x80},{0x5a,0xff},{0x5e,0}}}) mem.store(offset(light,p[0]),2,p[1]);
      mem.store(offset(light,0x54),1,0); invoke("light_link",{static_cast<Raw>(offset(light,0x4c))});
      lw(0x88,0); lw(0x90,0x43000000); lw(0xc,word(mem,offset(light,0xc))|3);
      lw(0xb4,word(mem,0x44fdcc)+0x50); lw(0x94,0x437f0000); lw(0x98,0x42c80000);
      lw(0xb0,mul(neg(wide),0x3c4ccccc));
      for (Raw off:{0xa8u,0x8cu,0x9cu,0xa0u,0xa4u}) lw(off,0);
      lw(0xac,wide); invoke("obj_runEveryFrame",{light}); lw(0xb8,1);
    }
  } else { w(0x50,0); w(0x4c,word(mem,0x44fdcc)+frames); }
  w(0x1c,0); w(0x58,ONE); w(0x5c,damage);
  for (auto &v:speed) v=mul(v,0x3c888888);
  wvec(offset(obj,0x60),speed); handle_link(offset(obj,0x6c),caster,caster);
  invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
}
Model::Vec Model::froststorm_point(Address obj, Raw spread) {
  auto b=object_ball(obj); auto k=mul(word(mem,offset(obj,0x54)),spread);
  auto x=mul(b[0],k), y=mul(b[1],k);
  auto up=add(mul(unit(object_rand(obj)),0x41400000),0x42f00000);
  return {add(x,word(mem,offset(obj,0x10))),add(y,word(mem,offset(obj,0x14))),add(up,word(mem,offset(obj,0x18)))};
}
void Model::froststorm_run(Address obj) {
  constexpr Raw FRAME=0x44fdcc, WORLD=0x44fdc8;
  auto w=[&](Raw off,Raw value){mem.store(offset(obj,off),4,value);};
  if (!(s32(word(mem,offset(obj,0x50)))<s32(word(mem,FRAME)))) return;
  for (unsigned i=0;i<3;++i) w(0x10+4*i,add(word(mem,offset(obj,0x10+4*i)),word(mem,offset(obj,0x60+4*i))));
  w(0x54,mul(word(mem,offset(obj,0x54)),word(mem,offset(obj,0x58))));
  wvec(offset(obj,0xa0),vec(offset(obj,0x10))); invoke("light_relink",{static_cast<Raw>(offset(obj,0x70))});
  auto reach=fmin(0x43960000,mul(word(mem,offset(obj,0x8c)),word(mem,offset(obj,0x58))));
  w(0x8c,reach); w(0x94,reach); auto inverse=div(ONE,mul(reach,reach)); w(0x98,inverse); w(0x90,inverse);
  std::size_t steps=0;
  auto loop=[&](Raw count,auto body) {
    for (Raw i=0;lt(from_int(i),count);++i) { walk_step(steps); body(); }
  };
  if (eq(word(mem,offset(obj,0x58)),ONE)) {
    auto radius=word(mem,offset(obj,0x54));
    auto snow=mul(mul(radius,0x3c088888),mul(radius,0x3e4ccccc));
    for (auto p:std::array<std::array<Raw,4>,2>{{{0x871128,0x3f800032,0x40e00000,0x4130974d},{0x871178,0x3f7fffef,0x40d99999,0x4004bc51}}}) {
      loop(snow,[&] {
        auto point=froststorm_point(obj,p[1]), b=object_ball(obj);
        Vec velocity{mul(b[0],p[2]),mul(b[1],p[2]),down(mul(mul(b[2],p[2]),p[3]))};
        typed_invoke("particle_spawn",{p[0],point,velocity,ZERO});
      });
    }
    if (c_rem(word(mem,FRAME),10)==0) {
      auto shards=mul(mul(mul(radius,0x3d50e560),0x3be38e37),radius);
      loop(shards,[&] {
        auto point=froststorm_point(obj,0x3f800008), b=object_ball(obj);
        Vec velocity{mul(b[0],0x3e51ec91),mul(b[1],0x3e51ec91),down(mul(b[2],0x407fffc9))};
        auto shard=invoke("pool_new",{0x64});
        if (shard) {
          game_object_ctor(shard,local_vec(point),0,0);
          auto sw=[&](Raw off,Raw value){mem.store(offset(shard,off),4,value);};
          sw(0x48,0x512a40); sw(0xc,word(mem,offset(shard,0xc))|0x421d); wvec(offset(shard,0x10),point);
          sw(0x4c,0); sw(0x60,word(mem,FRAME)+0x78); wvec(offset(shard,0x54),velocity); sw(0x50,ONE); sw(0x1c,0x41400000);
          invoke("obj_updateGridCell",{shard}); invoke("obj_runEveryFrame",{shard});
        }
      });
    }
    auto ground=mul(mul(radius,0x3c088888),mul(radius,0x3eccccab));
    loop(ground,[&] {
      auto angle=static_cast<Raw>(s16(object_rand(obj)));
      auto far=mul(unit(object_rand(obj)),word(mem,offset(obj,0x54)));
      Vec point{add(mul(cos16(angle),far),word(mem,offset(obj,0x10))),add(mul(sin16(angle),far),word(mem,offset(obj,0x14))),add(ZERO,word(mem,offset(obj,0x18)))};
      auto height=add(scalar(typed_invoke("terrain_heightAtPoint",{word(mem,WORLD),point})),0x40a00000);
      if (le(sub(word(mem,offset(obj,0x18)),0x43140000),height)) typed_invoke("particle_spawn",{Raw(0x871218),Vec{point[0],point[1],height},Vec{0,0,0},ZERO});
    });
  }
  if (c_rem(word(mem,FRAME),30)==0) {
    auto radius=word(mem,offset(obj,0x54)); auto here=vec(offset(obj,0x10)); Vec low{},high{};
    for (unsigned i=0;i<3;++i) {low[i]=sub(here[i],radius);high[i]=add(here[i],radius);}
    auto result=typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x40)});
    std::vector<Raw> found;
    if (auto p=std::get_if<std::vector<Raw>>(&result)) found=*p;
    else if (scalar(result)) throw Unsupported("box query requires object list or zero");
    if (found.size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
    for (auto other:found) {
      auto node=word(mem,offset(obj,0x6c));
      if (node) {
        if (!vcall(word(mem,offset(node,0xc)),20,{other})) continue;
        node=word(mem,offset(obj,0x6c));
      }
      auto half=mul(from_int(word(mem,offset(obj,0x5c))),HALF); auto amount=static_cast<Raw>(to_int(half));
      typed_invoke("damage_apply",{other,node ? word(mem,offset(node,0xc)):Raw(0),Raw(100000),Raw(0),Raw(0),Raw(0),descriptor({{0x16,amount,amount,0}}),Raw(0),half,half});
    }
  }
  if (s32(word(mem,offset(obj,0x4c)))<s32(word(mem,FRAME))) {
    w(0x58,0x3f733333); if (lt(word(mem,offset(obj,0x54)),0x41400000)) vcall(obj,1,{3});
  }
}
void Model::iceshard_particle(Address obj, Raw speed) {
  auto b=object_ball(obj); Vec point{};
  for (unsigned i=0;i<3;++i) point[i]=add(mul(b[i],0x3f5aa012),word(mem,offset(obj,0x10+4*i)));
  auto velocity=object_ball(obj); for (auto &v:velocity) v=mul(v,speed);
  typed_invoke("particle_spawn",{Raw(0x8711c8),point,velocity,ZERO});
}
void Model::iceshard_run(Address obj) {
  constexpr Raw FRAME=0x44fdcc, WORLD=0x44fdc8;
  auto scale=word(mem,offset(obj,0x50));
  auto particles=[&](Raw count,Raw speed) {
    std::size_t steps=0;
    for (Raw i=0;lt(from_int(i),count);++i) {walk_step(steps);iceshard_particle(obj,speed);}
  };
  if (!word(mem,offset(obj,0x4c)) && s32(word(mem,FRAME))<s32(word(mem,offset(obj,0x60)))) {
    mem.store(offset(obj,0x5c),4,add(word(mem,offset(obj,0x5c)),0xbeced8f4));
    for (unsigned i=0;i<3;++i) mem.store(offset(obj,0x10+4*i),4,add(word(mem,offset(obj,0x10+4*i)),word(mem,offset(obj,0x54+4*i))));
    particles(mul(mul(scale,0x3c088888),mul(scale,0x40825391)),0x3f89fc08);
    auto ground=scalar(typed_invoke("terrain_height",{word(mem,WORLD),vec(offset(obj,0x10)),word(mem,offset(obj,0x1c))}));
    if (lt(sub(word(mem,offset(obj,0x18)),ground),0x41800000)) mem.store(offset(obj,0x4c),4,1);
    return;
  }
  particles(mul(mul(scale,scale),0x40825391),0x412c7b0a); vcall(obj,1,{3});
}


Raw Model::groundpound_can_use(Address, Raw caster) {
  if (!(word(mem,offset(caster,0xc))&0x40)) return 1;
  Raw rec=record(caster);
  bool allowed=s32(signed_byte(mem,offset(rec,0x4a86)))==2;
  Raw item=word(mem,rec+0x4434u+4u*signed_byte(mem,offset(rec,0x4a7c)));
  if (item && word(mem,offset(word(mem,item),0x20))!=1) allowed=false;
  if (!allowed) {
    auto text=invoke("text_lookup",{0x54e7a0});
    invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
  }
  return allowed ? 1 : 0;
}
Raw Model::groundpound_cast(Address, Raw, Raw caster, Raw, Raw) {
  if (word(mem,offset(caster,0xc))&0x40) {
    mem.store(offset(caster,0x58c),4,10);
    auto clip=invoke("res_find",{word(mem,word(mem,offset(caster,0x24))),0x54e7b8});
    mem.store(offset(clip,0x20),1,2);
    mem.store(offset(clip,0x14),4,0x3e800000);
    mem.store(offset(clip,0x18),1,1);
    mem.store(offset(clip,0x1c),4,0x3f000000);
    mem.store(offset(clip,0x24),4,0x3f400000);
    mem.store(offset(clip,0x28),1,4);
    mem.store(offset(clip,0x2c),4,0x3fbd2e2f);
    mem.store(offset(clip,0x30),1,8);
    invoke("anim_startBlendInOut",{caster+0x114u,clip,10,0x3dcccccc,0x3dcccccc});
    auto sound=invoke("res_find",{0x54e7c8,0x54e7d0});
    invoke("sound_playOnObject",{sound,caster,ONE});
  }
  return 0;
}
void Model::groundpound_effect_ctor(Address obj, Address pos, Raw facing, Raw skill, Raw level, Raw owner) {
  auto start=vec(pos);
  auto r=[&](unsigned off){return word(mem,offset(obj,off));};
  auto w=[&](unsigned off,Raw v){mem.store(offset(obj,off),4,v);};
  auto b=[&](unsigned off,Raw v){mem.store(offset(obj,off),1,v);};
  game_object_ctor(obj,pos,0,0);
  w(0x48,0x512218); b(0x65,0); wvec(offset(obj,0x8c),start);
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x84,0x38b3cc07},{0x7c,0x38b3cc07},
      {0x78,0x42d80000},{0x80,0x42d80000},{0x88,0}}) w(off,value);
  for (unsigned base:{0x6eu,0x66u}) {
    unsigned i=0; for (Raw v:{0xc1u,0x75u,0x39u,0xffe0u}) mem.store(offset(obj,base+2*i++),2,v);
  }
  b(0x64,0); invoke("light_link",{static_cast<Raw>(obj+0x5c)});
  w(0x98,0); w(0x1c,0); mem.store(offset(obj,0x38),2,facing); w(0x58,skill); b(0x4d,level);
  auto sound=invoke("res_find",{0x54e7c8,0x54e818}); invoke("sound_playOnObject",{sound,owner,ONE});
  if (owner) handle_link(offset(obj,0x98),owner,owner);
  w(0x54,0x42d80000); b(0x4c,0); invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
  auto angle=[&]{return s16(static_cast<Raw>(mem.load(offset(obj,0x38),2)));};
  w(0x10,add(start[0],mul(cos16(angle()),0x41f00000)));
  w(0x14,add(start[1],mul(sin16(angle()),0x41f00000)));
  auto ground=scalar(typed_invoke("terrain_height",{word(mem,0x44fdc8),vec(offset(obj,0x10)),Raw(0x40c00000)}));
  b(0x50,lt(fabs(sub(ground,r(0x18))),0x43100000) ? 0 : 1); w(0x18,ground);
  auto water=scalar(typed_invoke("water_test",{word(mem,0x44fdc8),vec(offset(obj,0x10)),std::string("stack"),Raw(0x41000000)}));
  b(0x4f,water);
  if (signed_byte(mem,offset(obj,0x4f))) { b(0x50,0); invoke("light_unlink",{static_cast<Raw>(obj+0x5c)}); }
  else if (signed_byte(mem,offset(obj,0x50))) invoke("light_unlink",{static_cast<Raw>(obj+0x5c)});
  wvec(offset(obj,0x8c),{add(r(0x10),ZERO),r(0x14),add(r(0x18),0x42100000)});
  invoke("light_relink",{static_cast<Raw>(obj+0x5c)});
}
Raw Model::groundpound_stat(Address obj, Raw index, Raw cap) {
  auto skill=word(mem,offset(obj,0x58)), level=signed_byte(mem,offset(obj,0x4d));
  if (!word(mem,offset(obj,0x98))) return stat(skill,index,level);
  return stat_for(skill,index,level,word(mem,offset(word(mem,offset(obj,0x98)),0xc)),cap);
}
void Model::groundpound_effect_run(Address obj) {
  auto r=[&](unsigned off){return word(mem,offset(obj,off));};
  auto byte=[&](unsigned off){return s32(signed_byte(mem,offset(obj,off)));};
  auto w=[&](unsigned off,Raw v){mem.store(offset(obj,off),4,v);};
  auto here=[&]{return vec(offset(obj,0x10));};
  auto unit3=[&]{return unit(seed3_draw());};
  auto counter=byte(0x4c);
  if (!counter) {
    Raw reach=from_int(groundpound_stat(obj,1,10)*12u);
    if (byte(0x50)) reach=0x41c00000;
    for (unsigned angle=0;angle<0x10000;angle+=400) {
      Raw x=add(r(0x10),mul(cos16(s16(angle)),reach));
      Vec point{x,add(r(0x14),mul(sin16(s16(angle)),reach)),add(r(0x18),0x41400000)};
      Raw vx=mul(mul(sub(unit(object_rand(obj)),HALF),0x41400000),0x3e5cf548);
      Raw vy=mul(mul(sub(unit(object_rand(obj)),HALF),0x41400000),0x3e5cf548);
      Raw vz=mul(mul(unit(object_rand(obj)),0x41400000),0x401f9f81);
      typed_invoke("particle_spawn",{Raw(0x8e4d80),point,Vec{vx,vy,vz},ZERO});
    }
    auto angle=[&]{return s16(static_cast<Raw>(mem.load(offset(obj,0x38),2)));};
    Vec front{add(mul(cos16(angle()),0x41c00000),r(0x10)),
              add(mul(sin16(angle()),0x41c00000),r(0x14)),add(r(0x18),ZERO)};
    auto light=invoke("pool_new",{0xbc});
    if (light) light_effect(light,front,0x43410000,0x42ea0000,0x42640000,0x41100000,ZERO,0x3f4ccccc,0x3e999999,
                           0,0,{0xc2000000,0x43410000,0x42ea0000,0x42640000,0xc2000000});
    auto definition=lt(reach,0x43180000) ? 0x8e4ec0u : lt(reach,0x43680000) ? 0x8e4f10u : 0x8e4f60u;
    std::int32_t a=0;
    for (unsigned i=0;i<60;++i) {
      Vec velocity{mul(cos16(a),0x42c80000),mul(sin16(a),0x42c80000),mul(unit3(),0x41a00000)};
      typed_invoke("particle_spawn",{definition,front,velocity,Raw(0x3dcccccc)}); a=s16(static_cast<Raw>(a+0x444));
    }
    if (!byte(0x50)) {
      unsigned count=byte(0x4f) ? 46 : 23;
      for (unsigned i=0;i<count;++i) {
        Raw vx=mul(mul(sub(unit3(),HALF),0x41400000),0x3f9ddddd);
        Raw vy=mul(mul(sub(unit3(),HALF),0x41400000),0x3f9ddddd);
        Raw vz=mul(mul(unit3(),0x41400000),0x40733332);
        typed_invoke("particle_spawn",{Raw(byte(0x4f) ? 0x8e4e70 : 0x8e4dd0),here(),Vec{vx,vy,vz},ZERO});
      }
    }
    bool shake=false;
    if (!byte(0x4f)) {
      if (!byte(0x50)) {
        seed3_draw();
        typed_invoke("decal_add",{here(),static_cast<Raw>(s32(word(mem,G_SEED3))>>16),Raw(9),Raw(0),Raw(0x41400000)});
        shake=!byte(0x50);
      }
    } else {
      typed_invoke("water_disturb",{word(mem,0x44fdc8),here(),Raw(0x455f5000),Raw(0x42100000)}); shake=!byte(0x50);
    }
    if (shake) invoke("camera_shake",{0,add(mul(from_int(signed_byte(mem,offset(obj,0x4d))),0x3f3709c7),0x40a00000)});
    auto centre=here(); Vec lo,hi;
    for (unsigned i=0;i<3;++i) {lo[i]=sub(centre[i],reach);hi[i]=add(centre[i],reach);}
    auto found=query_objects(typed_invoke("world_collectObjectsInBox",{lo,hi,Raw(0x200)}));
    for (Raw other:found) {
      auto node=r(0x98);
      if (!node || !vcall(word(mem,offset(node,0xc)),20,{other})) continue;
      auto dy=sub(r(0x14),word(mem,offset(other,0x14))), dx=sub(r(0x10),word(mem,offset(other,0x10)));
      if (!lt(sqrt(add(mul(dx,dx),mul(dy,dy))),add(reach,word(mem,offset(other,0x1c))))) continue;
      auto amount=from_int(groundpound_stat(obj,0,0));
      auto low=std::max<std::int32_t>(round_half_away(sub(amount,mul(amount,0x3e4ccccc))),1);
      auto high=std::max<std::int32_t>(round_half_away(add(amount,mul(amount,0x3e4ccccc))),1);
      invoke("damage_apply",{other,word(mem,offset(r(0x98),0xc)),100000,0,0,0x80100,0,0,
                            from_int(static_cast<Raw>(low)),from_int(static_cast<Raw>(high))});
      if ((word(mem,offset(other,0xc))&0x20) && le(ZERO,word(mem,offset(other,0x228)))) {
        auto roll=object_rand(obj);
        auto chance=stat(r(0x58),2,signed_byte(mem,offset(obj,0x4d)));
        // Python subtracts an integer here; retain its unbounded comparison.
        if (std::int64_t(100)-s32(chance)<roll%100+1) {
          auto frames=stat(r(0x58),3,signed_byte(mem,offset(obj,0x4d)));
          vcall(other,30,{frames*60u,1});
        }
      }
    }
  } else if (counter==30) invoke("obj_delete",{static_cast<Raw>(obj)});
  if (!byte(0x4f) && !byte(0x50) && byte(0x4c)<4) {
    auto b=ball([&]{return seed3_draw();}); for (auto &c:b) c=mul(c,0x40400000);
    typed_invoke("particle_spawn",{Raw(0x8e4e20),here(),b,ZERO});
  }
  if (byte(0x4f)) {
    auto x=add(r(0x10),mul(sub(unit3(),HALF),0x42480000));
    auto y=add(r(0x14),mul(sub(unit3(),HALF),0x42480000));
    typed_invoke("water_disturb",{word(mem,0x44fdc8),Vec{x,y,sub(r(0x18),0x42100000)},Raw(0x455bc000),Raw(0x40a00000)});
  }
  auto radius=r(0x54); w(0x78,radius); w(0x80,radius);
  auto inverse=div(ONE,mul(radius,radius)); w(0x84,inverse); w(0x7c,inverse);
  radius=sub(r(0x54),0x40795445); w(0x54,radius); if (lt(radius,ZERO)) w(0x54,0);
  mem.store(offset(obj,0x4c),1,static_cast<Raw>(mem.load(offset(obj,0x4c),1))+1u);
}
Raw Model::harmtouch_can_use(Address, Raw caster) {
  if (!(word(mem,offset(caster,0xc))&0x40)) return 1;
  auto weapon=mem.load(offset(record(caster),0x4a86),1);
  if (weapon!=4 && weapon!=6) return 1;
  auto text=invoke("text_lookup",{0x54f048});
  invoke("hud_showMessage",{text,0xb4,word(mem,offset(caster,0x3c8)),0});
  return 0;
}
Raw Model::harmtouch_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  auto spell=invoke("pool_new",{0x1c});
  if (spell) harmtouch_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::harmtouch_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  spell_base_ctor(spell,level,owner,skill,0x518b50);
  mem.store(offset(spell,0x18),4,0x3f29372a);
  auto sound=invoke("res_find",{0x54f070,0x54f078});
  invoke("sound_playOnObject",{sound,owner,ONE});
}
void Model::harmtouch_tint(Address spell, Raw left) {
  auto owner=word(mem,offset(spell,0xc));
  mem.store(offset(owner,0x96),2,0x71);
  auto shade=to_int(mul(fmin(left,ONE),0x43000000));
  mem.store(offset(owner,0x92),2,static_cast<Raw>(shade)&0xffff);
  mem.store(offset(owner,0x9a),2,static_cast<Raw>(shade)&0xffff);
}
void Model::harmtouch_run(Address spell, Raw, Raw) {
  auto left=sub(word(mem,offset(spell,0x18)),DT);
  mem.store(offset(spell,0x18),4,left);
  if (lt(left,ZERO)) { spell_end(static_cast<Raw>(spell)); return; }
  harmtouch_tint(spell,left);
}
void Model::harmtouch_apply_to_hit(Address spell, Raw target, Address low_out, Address high_out) {
  harmtouch_tint(spell,word(mem,offset(spell,0x18)));
  if (!(word(mem,offset(target,0xc))&0x20)) return;
  for (unsigned i=0;i<0x66;++i) {
    auto value=spell_rand(spell);
    auto model=vcall(target,16,{});
    auto count=s32(invoke("model_getSamplePointCount",{model}));
    // The pinned Python divides before its second virtual call. No defined
    // sample index exists for zero points; retain the reached draw/calls.
    if (!count) throw Unsupported("model sample count is zero");
    model=vcall(target,16,{});
    auto point=sample_point(target,model,value,count);
    auto owner=word(mem,offset(spell,0xc));
    auto dx=sub(point[0],word(mem,offset(owner,0x10)));
    auto dy=sub(point[1],word(mem,offset(owner,0x14)));
    auto inverse=rsqrt(ONE,add(mul(dx,dx),mul(dy,dy)));
    Vec velocity{mul(mul(dx,inverse),0x4255bd8f),mul(mul(dy,inverse),0x4255bd8f),ZERO};
    spell_rand(spell);
    typed_invoke("particle_spawn",{Raw(0x8e55a0),point,velocity,ZERO});
    for (auto &v:velocity) v=mul(v,0x3e0c49ba);
    typed_invoke("particle_spawn",{Raw(0x8e55f0),point,velocity,ZERO});
  }
  auto light=invoke("pool_new",{0xbc});
  if (light) {
    Vec spot{add(word(mem,offset(target,0x10)),ZERO),word(mem,offset(target,0x14)),
             add(word(mem,offset(target,0x18)),0x42920000)};
    light_effect(light,spot,0x42a60000,0x435a0000,0x42920000,0x43580000,0x42700000,HALF,0x3ebb63b2);
  }
  auto amount=stat_for(word(mem,offset(spell,8)),0,level_byte(mem,spell),word(mem,offset(spell,0xc)),10);
  mem.store(low_out,4,amount);
  auto spread=mul(from_int(amount),0x3e4ccccc);
  auto extra=static_cast<Raw>(to_int(mul(spread,unit(spell_rand(spell)))));
  mem.store(high_out,4,word(mem,low_out)+extra);
  spell_call(spell,1,{3});
}
// Darkness is a GameObject with its own movement/query path, not a Projectile.
Raw Model::darkness_cast(Address skill, Raw level, Raw caster, Raw, Raw facing) {
  Vec velocity{mul(cos16(facing),0x43ba8000),mul(sin16(facing),0x43ba8000),mul(ZERO,0x43ba8000)};
  auto obj=invoke("pool_new",{0xb0});
  if (obj) {
    Vec start{add(word(mem,offset(caster,0x10)),ZERO),word(mem,offset(caster,0x14)),
              add(word(mem,offset(caster,0x18)),0x42100000)};
    // Evaluate scratch allocations in reference order.
    auto position=local_vec(start);
    auto speed=local_vec(velocity);
    darkness_ctor(obj,position,speed,static_cast<Raw>(skill),level,caster);
  }
  return 0;
}
std::array<Raw,4> Model::ambient_bytes() const {
  std::array<Raw,4> colour{};
  for (unsigned i=0;i<4;++i) colour[i]=static_cast<Raw>(mem.load(0x5d3478+i,1));
  return colour;
}
void Model::darkness_ctor(Address obj, Address pos, Address speed, Raw skill, Raw level, Raw caster) {
  auto start=vec(pos), velocity=vec(speed);
  game_object_ctor(obj,pos,0,0);
  mem.store(offset(obj,0x48),4,0x518810);
  mem.store(offset(obj,0x61),1,0);
  wvec(offset(obj,0x88),start);
  for (auto [off,value]:std::initializer_list<std::pair<unsigned,Raw>>{
      {0x7c,0x43280000},{0x80,0x38149b92},{0x74,0x43280000},{0x78,0x38149b92},{0x84,0}})
    mem.store(offset(obj,off),4,value);
  for (auto off:{0x6e,0x62,0x64,0x66,0x6a,0x6c}) mem.store(offset(obj,off),2,0xff82);
  mem.store(offset(obj,0x68),2,0); mem.store(offset(obj,0x70),2,0);
  mem.store(offset(obj,0x60),1,0);
  invoke("light_link",{static_cast<Raw>(offset(obj,0x58))});
  mem.store(offset(obj,0xa4),4,0); mem.store(offset(obj,0xa8),4,0);
  mem.store(offset(obj,0xc),4,word(mem,offset(obj,0xc))|0x4001);
  for (auto &v:velocity) v=mul(v,DT);
  wvec(offset(obj,0x4c),velocity);
  mem.store(offset(obj,0x94),4,108); mem.store(offset(obj,0xac),4,skill);
  mem.store(offset(obj,0x98),4,level); mem.store(offset(obj,0xa0),1,0);
  mem.store(offset(obj,0x9c),4,1);
  auto sound=invoke("res_find",{0x54ecd0,0x54ecd8});
  invoke("sound_playOnObject",{sound,caster,ONE});
  if (caster) handle_link(offset(obj,0xa8),caster,caster);
  auto colour=ambient_bytes();
  for (unsigned i=0;i<3;++i) mem.store(offset(obj,0x62+2*i),2,1u-colour[i]);
  invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
}
std::vector<Raw> Model::darkness_box(Address obj) {
  auto here=vec(offset(obj,0x10)); Vec low{},high{};
  for (unsigned i=0;i<3;++i) { low[i]=sub(here[i],0x41c00000); high[i]=add(here[i],0x41c00000); }
  return query_objects(typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x80)}));
}
void Model::darkness_run(Address obj) {
  auto level=word(mem,offset(obj,0x98));
  auto owner=[&] { return word(mem,offset(word(mem,offset(obj,0xa8)),0xc)); };
  if (signed_byte(mem,offset(obj,0xa0))==1) {
    if (c_rem(word(mem,creature_hp::G_SIM_FRAME),30)==0 && word(mem,offset(obj,0xa8))) {
      for (auto other:darkness_box(obj)) {
        if (!vcall(owner(),20,{other})) continue;
        if (!lt(ZERO,word(mem,offset(other,0x228))) || s16(static_cast<Raw>(mem.load(offset(other,0x316),2)))) continue;
        auto chance=stat(word(mem,offset(obj,0xac)),0,level);
        // Python compares the nonnegative roll with the signed statistic integer.
        if (static_cast<std::int64_t>(object_rand(obj)%100+1)<s32(chance)) {
          Raw frames=stat(word(mem,offset(obj,0xac)),3,level)*60u;
          if (invoke("creature_bossRank",{other})) frames=static_cast<Raw>((s32(frames)>>1)+1);
          vcall(other,31,{frames});
        }
      }
    }
  } else {
    if (word(mem,offset(obj,0x9c))) {
      for (unsigned i=0;i<3;++i)
        mem.store(offset(obj,0x10+4*i),4,add(word(mem,offset(obj,0x10+4*i)),word(mem,offset(obj,0x4c+4*i))));
      auto ground=scalar(typed_invoke("terrain_height",{word(mem,0x44fdc8),vec(offset(obj,0x10)),Raw(0x41c00000)}));
      if (lt(word(mem,offset(obj,0x18)),ground)) mem.store(offset(obj,0x9c),4,0);
    }
    if (word(mem,offset(obj,0xa8))) {
      for (auto other:darkness_box(obj)) {
        if (!vcall(owner(),20,{other})) continue;
        if (signed_byte(mem,offset(obj,0xa0))==0) {
          auto life=stat(word(mem,offset(obj,0xac)),2,level);
          mem.store(offset(obj,0xa0),1,1); mem.store(offset(obj,0x94),4,life*60u);
          wvec(offset(obj,0x10),{add(word(mem,offset(other,0x10)),ZERO),word(mem,offset(other,0x14)),
                               add(word(mem,offset(other,0x18)),0x42100000)});
        }
        if (vcall(owner(),20,{other})) {
          auto range=stat_range(word(mem,offset(obj,0xac)),1,level,0,0x3e4ccccc);
          typed_invoke("damage_apply",{other,owner(),Raw(100000),Raw(0),Raw(0),Raw(0x40),Raw(0),Raw(0),
                                       from_int(range[0]),from_int(range[1])});
        }
      }
    }
  }
  wvec(offset(obj,0x88),vec(offset(obj,0x10)));
  invoke("light_relink",{static_cast<Raw>(offset(obj,0x58))});
  for (unsigned n=0;n<8;++n) {
    auto b=object_ball(obj); Vec velocity{},point{};
    for (unsigned i=0;i<3;++i) {
      velocity[i]=add(word(mem,offset(obj,0x4c+4*i)),mul(b[i],0xbea22221));
      point[i]=add(mul(add(b[i],b[i]),0x41400000),word(mem,offset(obj,0x10+4*i)));
    }
    typed_invoke("particle_spawn",{Raw(0x8e5410),point,velocity,ZERO});
  }
  mem.store(offset(obj,0x94),4,word(mem,offset(obj,0x94))-1u);
  if (word(mem,offset(obj,0x94))==UINT32_MAX) {
    invoke("obj_delete",{static_cast<Raw>(obj)});
    invoke("light_unlink",{static_cast<Raw>(offset(obj,0x58))});
  }
}
// RTA section 57. Engine services and damage remain explicit callback boundaries.
Raw Model::shockoffire_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing) {
  auto start=vec(pos);
  level=skill_call(skill,14,{level,20});
  auto damage=stat_range(skill,0,level,0,0x3e4ccccc);
  auto splash=skill_call(skill,12,{1,level,caster,0});
  auto radius=skill_call(skill,12,{2,level,caster,10});
  auto speed=from_int(s32(skill_call(skill,12,{3,level,caster,10})));
  Vec velocity{mul(cos16(s16(facing)),speed),mul(sin16(s16(facing)),speed),ZERO};
  auto obj=invoke("pool_new",{0xb8});
  if (obj) {
    auto position=local_vec(start), motion=local_vec(velocity);
    sparks_ctor(obj,position,motion,caster,0,0,damage[0],damage[1],splash,radius*12u);
  }
  return 0;
}
void Model::sparks_ctor(Address obj, Address pos, Address velocity, Raw owner, Raw,
                        Raw tag, Raw low, Raw high, Raw splash, Raw radius) {
  auto start=vec(pos), speed=vec(velocity);
  projectile_ctor(obj,local_vec(start),owner,0);
  mem.store(offset(obj,0x48),4,0x50e998);
  mem.store(offset(obj,0x6d),1,0);
  wvec(offset(obj,0x94),start);
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x88,0x42f00000},{0x8c,0x3891a2b3},
       {0x80,0x42f00000},{0x84,0x3891a2b3},{0x90,0}}) mem.store(offset(obj,off),4,value);
  for (auto [off,value]:{std::pair<unsigned,Raw>{0x76,0x13e},{0x78,0xc1},{0x7c,0xffe0},
       {0x6e,0x13e},{0x70,0xc1},{0x74,0xffe0},{0x72,0},{0x7a,0}}) mem.store(offset(obj,off),2,value);
  mem.store(offset(obj,0x6c),1,0);
  invoke("light_link",{static_cast<Raw>(offset(obj,0x64))});
  wvec(offset(obj,0x58),speed);
  mem.store(offset(obj,0xb0),4,splash); mem.store(offset(obj,0xb4),4,radius);
  mem.store(offset(obj,0x54),4,word(mem,creature_hp::G_SIM_FRAME)+600u);
  mem.store(offset(obj,0xa8),4,high); mem.store(offset(obj,0xac),4,low);
  invoke("obj_runEveryFrame",{static_cast<Raw>(obj)});
  auto sound=invoke("res_find",{0x549088,0x549458});
  invoke("sound_playOnObject",{sound,static_cast<Raw>(obj),ONE});
  mem.store(offset(obj,0xa0),4,tag);
}
void Model::sparks_run(Address obj) {
  auto particle=[&](Raw definition, Raw scale) {
    auto velocity=object_ball(obj);
    for (auto &c:velocity) c=mul(c,scale);
    typed_invoke("particle_spawn",{definition,vec(offset(obj,0x10)),velocity,ZERO});
  };
  if (s32(word(mem,creature_hp::G_SIM_FRAME))<s32(word(mem,offset(obj,0x54)))) {
    if (obj_move_and_collide(obj,local_vec(vec(offset(obj,0x58))))==1)
      mem.store(offset(obj,0x54),4,0);
    typed_invoke("voice_setPosition",{word(mem,offset(obj,0xa4)),vec(offset(obj,0x10)),ONE});
    for (unsigned i=0;i<3;++i) particle(0x8e4010,0x41199999);
    for (unsigned i=0;i<5;++i) { object_ball(obj); particle(0x8e4060,0x41199999); }
    if (object_rand(obj)&1) {
      Vec point;
      for (unsigned i=0;i<3;++i)
        point[i]=sub(word(mem,offset(obj,0x10+4*i)),mul(mul(word(mem,offset(obj,0x58+4*i)),DT),0x40a00000));
      auto velocity=object_ball(obj);
      for (auto &c:velocity) c=mul(c,0x41200000);
      typed_invoke("particle_spawn",{Raw(0x8e4150),point,velocity,ZERO});
    }
    wvec(offset(obj,0x94),vec(offset(obj,0x10)));
    invoke("light_relink",{static_cast<Raw>(offset(obj,0x64))});
    return;
  }
  for (unsigned i=0;i<50;++i) particle(0x8e4100,0x42666666);
  if (s32(word(mem,offset(obj,0xb0)))>0) {
    auto node=word(mem,offset(obj,0x4c));
    auto attacker=node ? word(mem,offset(node,0xc)):static_cast<Raw>(obj);
    typed_invoke("fx_ringBurst",{vec(offset(obj,0x10)),TWO});
    typed_invoke("world_damageInRadius",{attacker,vec(offset(obj,0x10)),word(mem,offset(obj,0xb0)),
      Raw(0x40860),word(mem,offset(obj,0xb0)),Raw(0),Raw(0x42200000),from_int(s32(word(mem,offset(obj,0xb4))))});
  }
  vcall(obj,1,{3});
}
void Model::sparks_on_touch(Address obj, Raw other) {
  auto node=word(mem,offset(obj,0x4c));
  auto owner=node ? word(mem,offset(node,0xc)):0;
  if (!word(mem,offset(obj,0x54)) || !owner || !vcall(owner,20,{other})) return;
  auto light=invoke("pool_new",{0xbc});
  if (light) light_effect(light,vec(offset(obj,0x10)),0x437f0000,0x43480000,0x42c80000,
    0x43340000,0x42f00000,ONE,ONE,0,0,{0xc2000000,0,0,0,0});
  auto sound=invoke("res_find",{0x549088,0x549470});
  invoke("sound_playOnObject",{sound,static_cast<Raw>(obj),ONE});
  auto low=word(mem,offset(obj,0xac)), high=word(mem,offset(obj,0xa8));
  typed_invoke("damage_apply",{other,owner,Raw(10000000),low,high,Raw(0xc060),
    descriptor({{0x1f,100,low,high}}),Raw(0),ZERO,ZERO});
  mem.store(offset(obj,0x54),4,0);
}

// Section 66, pinned RTA contract. All arithmetic uses the selected raw32 FPU.
Raw Model::planarwind_cast(Address skill, Raw level, Raw caster, Address pos, Raw) {
  auto start=vec(pos);
  level=skill_call(skill,14,{level,20});
  auto spell=invoke("pool_new",{0xac});
  if (spell) planarwind_spell_ctor(spell,level,caster,static_cast<Raw>(skill),local_vec(start));
  return spell;
}
void Model::planarwind_spell_ctor(Address spell, Raw level, Raw owner, Raw skill, Address pos) {
  auto start=vec(pos);
  spell_base_ctor(spell,level,owner,skill,0x51b778);
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto w=[&](unsigned off,Raw value){mem.store(offset(spell,off),4,value);};
  w(0x6c,0); mem.store(offset(spell,0x79),1,0);
  wvec(offset(spell,0xa0),vec(offset(owner,0x10))); w(0x9c,0);
  for (auto [off,value]:std::initializer_list<std::pair<unsigned,Raw>>{
      {0x82,0x9b},{0x84,0x4c},{0x86,0x1a2},{0x7a,0x9b},{0x7c,0x4c},{0x7e,0x1a2},{0x80,0},{0x88,0}})
    mem.store(offset(spell,off),2,value);
  mem.store(offset(spell,0x78),1,0);
  invoke("light_link",{static_cast<Raw>(offset(spell,0x70))});
  w(0x34,from_int(s32(stat_for(r(8),2,level_byte(mem,spell),r(0xc),10))));
  w(0x24,cos16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(r(0xc),0x38),2))))));
  w(0x28,sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(r(0xc),0x38),2))))));
  w(0x2c,0); w(0x18,start[0]); w(0x1c,start[1]); w(0x20,add(start[2],0x41400000));
  auto width=from_int(s32(stat_for(r(8),1,level_byte(mem,spell),r(0xc),10))); w(0x30,width);
  auto reach=fmin(0x43960000,fmax(0x42700000,mul(width,0x3fc00000)));
  w(0x8c,reach); w(0x94,reach);
  auto inverse=div(ONE,mul(reach,reach));
  w(0x5c,static_cast<Raw>(to_int(mul(mul(mul(width,0x40648a4c),0x3be38e37),width))));
  w(0x98,inverse); w(0x90,inverse);
  auto sound=invoke("res_find",{0x552260,0x552268});
  typed_invoke("sound_playAtPos",{sound,start,ONE});
  auto matrix=mat3_set_rot_z(static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(r(0xc),0x38),2))+0x4000u)));
  for (unsigned i=0;i<9;++i) w(0x38+4*i,matrix[i]);
  auto rank=s32(signed_byte(mem,spell));
  w(0x64,rank>=11?0x200000:rank<6?0x80000:0x100000);
  w(0x60,skill_call(r(8),12,{3,level_byte(mem,spell),r(0xc),10})); w(0x68,0);
  auto head=query_list("world_collectObjectsInArc",{start,
    static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(r(0xc),0x38),2)))),owner,
    Raw(0x44700000),Raw(0xbf800000),r(0x30)});
  for (auto target:list_objects(head)) handle_link(offset(spell,0x6c),target,target);
  handle_clear(head);
}
void Model::planarwind_strike(Address spell, Raw target, const Vec &step) {
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto owner=r(0xc),skill=r(8); std::array<Raw,2> range{};
  if (owner && (word(mem,offset(owner,0xc))&0x40))
    range=stat_range(skill,0,level_byte(mem,spell),0,0x3e4ccccc);
  else {
    auto value=from_int(s32(skill_call(skill,12,{0,level_byte(mem,spell),owner,0})));
    auto spread=mul(value,0x3e4ccccc);
    auto high=static_cast<Raw>(to_int(add(value,spread))),low=static_cast<Raw>(to_int(sub(value,spread)));
    range={low,high};
  }
  auto flags=r(0x64)|0x40u;
  if (word(mem,G_PVP_MODE_A) && (word(mem,offset(target,0xc))&0x40) && !(draw()%100<21)) flags=0x40;
  typed_invoke("damage_apply",{target,r(0xc),Raw(100000),Raw(0),Raw(0),flags,Raw(0),Raw(1),from_int(s32(range[0])),from_int(s32(range[1]))});
  auto roll=draw()%100;
  if (static_cast<std::int64_t>(roll)<s32(r(0x60)))
    invoke("Creature_setEffectTimer",{target,Raw(0),r(0xc),Raw(0x40000000)});
  auto sound=invoke("res_find",{0x552260,0x552280});
  typed_invoke("sound_playAtPos",{sound,vec(offset(target,0x10)),ONE});
  auto model=vcall(target,16,{});
  if (model) {
    auto count=s32(invoke("model_getSamplePointCount",{model}));
    for (unsigned i=0;i<72;++i) {
      auto value=spell_rand(spell);
      auto point=sample_point(target,model,value,count);
      typed_invoke("particle_spawn",{Raw(0x8e74a8),point,step,ZERO});
    }
  }
}
void Model::planarwind_run(Address spell, Raw, Raw) {
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto w=[&](unsigned off,Raw value){mem.store(offset(spell,off),4,value);};
  auto speed=r(0x34); Vec step{};
  for (unsigned i=0;i<3;++i) step[i]=mul(mul(r(0x24+4*i),speed),DT);
  w(0x1c,add(r(0x1c),step[1])); w(0x20,add(r(0x20),step[2])); w(0x18,add(r(0x18),step[0]));
  auto node=r(0x6c); std::size_t visits=0;
  while (node) {
    walk_step(visits);
    auto target=word(mem,offset(node,0xc)),after=word(mem,node);
    if (target && lt(vec3_dot_delta(vec(offset(spell,0x18)),vec(offset(spell,0x24)),vec(offset(target,0x10))),ZERO)) {
      planarwind_strike(spell,target,step); handle_remove_node(node);
    }
    node=after;
  }
  auto front=vec(offset(spell,0x18));
  wvec(offset(spell,0xa0),{sub(add(front[0],ZERO),mul(r(0x24),0x42700000)),
    sub(front[1],mul(r(0x28),0x42700000)),sub(front[2],mul(r(0x2c),0x42700000))});
  invoke("light_relink",{static_cast<Raw>(offset(spell,0x70))});
  speed=r(0x34); w(0x34,sub(speed,mul(mul(speed,0x3c83126e),0x3f7fffff)));
  auto rank=s32(signed_byte(mem,spell)); Raw mask=rank>=10?15:rank>=5?31:rank<2?127:63;
  std::size_t done=0;
  while (static_cast<std::int64_t>(done)<s32(r(0x5c))) {
    if (done>=creature_hp::MAX_HISTORY) throw Unsupported("Planar Wind particle limit");
    Mat3 matrix{}; for (unsigned i=0;i<9;++i) matrix[i]=r(0x38+4*i);
    auto angle=spell_rand(spell); matrix=mat3_rotate_axis(matrix,1,static_cast<Raw>(s16(angle)));
    auto far=mul(unit(spell_rand(spell)),r(0x30)); Vec point{};
    for (unsigned i=0;i<3;++i) point[i]=add(r(0x18+4*i),mul(matrix[i],far));
    for (unsigned i=0;i<3;++i) step[i]=add(step[i],mul(sub(unit(spell_rand(spell)),HALF),HALF));
    auto definition=(spell_rand(spell)&mask)?0x8e7458u:0x8e7408u;
    typed_invoke("particle_spawn",{definition,point,step,ZERO}); ++done;
  }
  auto ground=scalar(typed_invoke("terrain_height",{word(mem,0x44fdc8),vec(offset(spell,0x18)),Raw(0x41000000)}));
  auto runs=r(0x68); w(0x68,runs+1u);
  if (!(s32(runs)<53) || lt(r(0x20),ground)) spell_end(spell);
}

// Section 67: retain live owner reads and delegated engine services.
Raw Model::shadowstep_cast(Address skill, Raw level, Raw caster, Raw, Raw) {
  level=skill_call(skill,14,{level,20});
  auto spell=invoke("pool_new",{0x34});
  if (spell) shadowstep_spell_ctor(spell,level,caster,static_cast<Raw>(skill));
  return spell;
}
void Model::shadowstep_spell_ctor(Address spell, Raw level, Raw owner, Raw skill) {
  spell_base_ctor(spell,level,owner,skill,0x51bd48);
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto w=[&](unsigned off,Raw value){mem.store(offset(spell,off),4,value);};
  auto facing_at=[&](Raw obj){return static_cast<Raw>(s16(static_cast<Raw>(mem.load(offset(obj,0x38),2))));};
  w(0x1c,0x43b90000); w(0x18,0);
  w(0x20,cos16(facing_at(owner))); w(0x24,sin16(facing_at(r(0xc)))); w(0x28,0);
  auto velocity=vec(offset(spell,0x20)); auto facing=facing_at(r(0xc));
  auto flags=word(mem,offset(owner,0xc));
  mem.store(offset(owner,0x58c),4,0xe); mem.store(offset(spell,0x30),2,facing-0x8000u);
  mem.store(offset(owner,0xc),4,flags|5u); w(0x2c,owner);
  auto inverse=rsqrt(ONE,add(add(mul(velocity[0],velocity[0]),mul(velocity[1],velocity[1])),mul(velocity[2],velocity[2])));
  Vec scaled{}; for (unsigned i=0;i<3;++i) scaled[i]=mul(mul(velocity[i],inverse),r(0x1c));
  wvec(offset(spell,0x20),scaled);
  auto sound=invoke("res_find",{0x5528c8,0x5528d0});
  typed_invoke("sound_playAtPos",{sound,vec(offset(r(0xc),0x10)),ONE});
  auto mover=r(0x2c); Vec low{},high{},reach{0x42c20000,0x42c20000,0x447a0000};
  for (unsigned i=0;i<3;++i) low[i]=sub(word(mem,offset(mover,0x10+4*i)),reach[i]);
  for (unsigned i=0;i<3;++i) high[i]=add(word(mem,offset(mover,0x10+4*i)),reach[i]);
  auto calm=skill_call(r(8),10,{0,level_byte(mem,spell)});
  auto fires=s32(skill_call(r(8),10,{1,level_byte(mem,spell)}));
  std::int64_t ring=0;
  if (fires) {
    auto block=invoke("pool_new",{0x4ac});
    if (block) { mover=r(0x2c); object_ctor("StaticFire_ctor",{block,vec(offset(mover,0x10)),Raw(0x168),mover,Raw(0),Raw(0x42f00000),ONE}); }
    ring=static_cast<std::int64_t>(fires)-1;
    if (ring) {
      auto angle=facing_at(r(0x2c)); auto step=ring>0?static_cast<Raw>(0xffff/ring):0;
      Raw height=ZERO;
      for (std::int64_t i=0;i<ring;++i) {
        if (i>=static_cast<std::int64_t>(creature_hp::MAX_HISTORY)) throw Unsupported("Shadow Step ring limit");
        auto x=add(word(mem,offset(r(0x2c),0x10)),mul(sin16(static_cast<Raw>(s16(angle))),0x42960000));
        auto y=add(word(mem,offset(r(0x2c),0x14)),mul(cos16(static_cast<Raw>(s16(angle))),0x42960000));
        height=scalar(typed_invoke("terrain_heightAtPoint",{word(mem,0x44fdc8),Vec{x,y,height}}));
        if (!lt(0x42900000,fabs(sub(word(mem,offset(r(0xc),0x18)),height)))) {
          block=invoke("pool_new",{0x4ac});
          if (block) object_ctor("StaticFire_ctor",{block,Vec{x,y,height},Raw(0xf0),r(0x2c),Raw(1),Raw(0x42f00000),ONE});
        }
        angle+=step;
      }
    }
  }
  auto result=typed_invoke("world_collectObjectsInBox",{low,high,Raw(0x40)});
  std::vector<Raw> objects;
  if (auto list=std::get_if<std::vector<Raw>>(&result)) objects=*list;
  else if (scalar(result)) throw Unsupported("box query requires object list or zero");
  if (objects.size()>creature_hp::MAX_HISTORY) throw Unsupported("query result limit");
  for (auto other:objects) {
    if (!(word(mem,offset(other,0xc))&0x20) || !vcall(r(0x2c),20,{other})) continue;
    auto handle=word(mem,offset(other,0x234));
    if (!handle || word(mem,offset(handle,0xc))!=r(0x2c)) continue;
    vcall(other,31,{calm});
    if (ring) {
      mem.store(offset(other,0x2e8),4,0x43170000); mover=r(0x2c);
      auto angle=atan2_16(sub(word(mem,offset(other,0x14)),word(mem,offset(mover,0x14))),
        sub(word(mem,offset(other,0x10)),word(mem,offset(mover,0x10))));
      mem.store(offset(other,0x2e0),2,angle-0x8000u);
    }
  }
  mover=r(0x2c); auto model=vcall(mover,16,{});
  auto count=s32(invoke("model_getSamplePointCount",{model}));
  for (unsigned i=0;i<86;++i) {
    auto velocity=spell_ball(spell); for (auto &c:velocity) c=mul(c,0x40400000);
    auto creature=r(0x2c); auto value=spell_rand(spell); auto point=sample_point(creature,model,value,count);
    typed_invoke("particle_spawn",{Raw(0x8e84b8),point,velocity,ZERO});
  }
  if (!ring) {
    auto light=invoke("pool_new",{0xbc});
    if (light) {
      mover=r(0x2c); Vec spot{add(word(mem,offset(mover,0x10)),ZERO),word(mem,offset(mover,0x14)),add(word(mem,offset(mover,0x18)),0x42100000)};
      light_effect(light,spot,0x43000000,0x429e0000,0x42540000,0x42f00000,0x43340000,ZERO,ONE,0,0,{0xc2c00000,0,0,0,0});
    }
  }
}
void Model::shadowstep_run(Address spell, Raw, Raw) {
  auto r=[&](unsigned off){return word(mem,offset(spell,off));};
  auto half=sub(r(0x18),HALF); half=add(half,half);
  auto velocity=vec(offset(spell,0x20));
  if (!mem.load(0x402791,1)) {
    auto result=typed_invoke("Player_clampToScreen",{r(0x2c)});
    auto corrected=std::get_if<std::vector<Raw>>(&result);
    if (!corrected || corrected->size()!=3) throw Unsupported("screen clamp requires three raw patterns");
    std::copy(corrected->begin(),corrected->end(),velocity.begin());
  }
  auto mover=r(0x2c); for (auto &c:velocity) c=mul(c,HALF);
  mem.store(offset(mover,0xdc),4,mul(mul(half,half),half));
  typed_invoke("obj_moveWithCollision",{mover,velocity,Raw(0),Raw(1)});
  typed_invoke("obj_moveWithCollision",{r(0x2c),velocity,Raw(0),Raw(1)});
  mem.store(0x402797,1,1); mover=r(0x2c);
  if (lt(word(mem,offset(mover,0xdc)),HALF)) mem.store(offset(mover,0xa8),8,mem.load(offset(mover,0xa8),8)|0x200000u);
  mem.store(offset(spell,0x18),4,add(r(0x18),0x3d277507));
  if (lt(HALF,r(0x18))) mem.store(offset(r(0xc),0x38),2,mem.load(offset(spell,0x30),2));
  if (lt(ONE,r(0x18))) spell_end(spell);
}
} // namespace champions::simulation::rta::skills_b
