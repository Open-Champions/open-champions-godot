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
Raw scalar(const Model::Result &r) {
  if (auto p=std::get_if<Raw>(&r)) return *p;
  throw Unsupported("callback requires raw32 return");
}
constexpr Raw spell_bodies[] = {0x2d1a94,0x2d2c0c,0x2e04f8,0x2e8428,0x2e90b8,
  0x312b30,0x2de7dc,0x2e7acc,0x2c8f2c,0x311e3c,0x2e0d10,0x2d0354,
  0x2d0edc,0x2e9d10,0x309420,0x30a0b0};
constexpr Raw object_bodies[] = {0x2d66ac,0x2d685c,0x2d73c8,0x2d7aac,0x2d8e80,
  0x2d932c,0x2db17c,0x2db4f0,0x2cf690,0x2c42e4,0x2c7808,0x1ee330,
  0x1ee7b0,0x28fa3c,0x2900c8,0x1fe950,0x1fdef8};
}
Raw Model::spell_call(Address spell, unsigned slot, const std::vector<Raw> &args) {
  if (slot>15 || depth_>=64) throw Unsupported("virtual slot or recursion limit");
  auto table=word(mem,offset(spell,0x14));
  Raw self=static_cast<Raw>(spell+static_cast<std::int64_t>(s16(static_cast<Raw>(mem.load(Address(table)+8*slot,2)))));
  auto body=word(mem,Address(table)+8*slot+4);
  auto name="Spell.v"+std::to_string(slot);
  if (!external.count(name)) {
    struct Guard { unsigned &d; Guard(unsigned &v):d(v){++d;} ~Guard(){--d;} } guard(depth_);
    if (body==0x2e04f8) { arity(args,2); minorhealing_run(self,args[0],args[1]); return 0; }
    if (body==0x2e8428) { arity(args,2); holyshield_run(self,args[0],args[1]); return 0; }
    if (body==0x2e90b8) { arity(args,2); healing_run(self,args[0],args[1]); return 0; }
  }
  if (!external.count(name) && std::find(std::begin(spell_bodies),std::end(spell_bodies),body)!=std::end(spell_bodies))
    throw Unsupported("unported registered Spell body");
  auto named=addr_names.find(body);
  if (named!=addr_names.end()) name=named->second;
  if (slot==2) {
    arity(args,2);
    return scalar(typed_invoke(name,{self,vec(args[0]),args[1]}));
  }
  std::vector<Raw> forwarded{self};
  forwarded.insert(forwarded.end(),args.begin(),args.end());
  return invoke(name,std::move(forwarded));
}
Raw Model::vcall(Address object, unsigned slot, const std::vector<Raw> &args) {
  if (slot>64 || depth_>=64) throw Unsupported("virtual slot or recursion limit");
  auto table=word(mem,offset(object,0x48));
  Raw self=static_cast<Raw>(object+static_cast<std::int64_t>(s16(static_cast<Raw>(mem.load(Address(table)+8*slot,2)))));
  auto body=word(mem,Address(table)+8*slot+4);
  auto name="Creature.v"+std::to_string(slot);
  if (!external.count(name) && std::find(std::begin(object_bodies),std::end(object_bodies),body)!=std::end(object_bodies))
    throw Unsupported("unported registered object body");
  auto named=addr_names.find(body);
  if (named!=addr_names.end()) name=named->second;
  std::vector<Raw> forwarded{self};
  forwarded.insert(forwarded.end(),args.begin(),args.end());
  return invoke(name,std::move(forwarded));
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
} // namespace champions::simulation::rta::skills_b
