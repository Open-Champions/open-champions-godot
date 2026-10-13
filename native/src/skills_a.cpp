// MIT; Copyright (c) 2026 TrustlessMike and contributors.
// Authored from ref_skills_a.py at 356add27d50e50972b2a671b31e8f4ec93e6778c,
// SHA256 6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae.
#include "champions/skills_a.hpp"
#include <algorithm>
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
  v &= n == 4 ? UINT32_MAX : n == 2 ? 0xffffu : 0xffu;
  writes.push_back({Raw(a), n, v});
  mem.store(a, n, v);
}
void Model::write_vec(Address at, const Vec3 &value) {
  if (writes.size() >= creature_hp::MAX_HISTORY) throw Unsupported("Skill write history limit");
  const auto pair=std::uint64_t(value[0]) | (std::uint64_t(value[1]) << 32);
  writes.push_back({Raw(at),8,pair}); mem.store(at,8,pair);
  write(at+8,4,value[2]);
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
    case 0x2cce28: arity(args,1,1); return {diseasetrail_can_use(self,a(0))};
    case 0x2ccba0: arity(args,5,6); return {diseasetrail_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2e34ac: case 0x2e616c: case 0x315bb0:
      arity(args, 1, 1); return {weaponbuff_can_use(body == 0x2e34ac ? 12 : body == 0x2e616c ? 14 : 26, a(0))};
    case 0x2e3184: case 0x2e5e44: case 0x315be4:
      arity(args, 2, 6); return {weaponbuff_cast(body == 0x2e3184 ? 12 : body == 0x2e5e44 ? 14 : 26, self, a(0), a(1))};
    case 0x2d532c: arity(args,1,1); return {firestorm_can_use(self,a(0))};
    case 0x2d4f7c: arity(args,6,6); return {firestorm_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2faa00: arity(args,1,1); return {shieldbash_can_use(self,a(0))};
    case 0x2faa88: arity(args,2,6); return {shieldbash_cast(self,a(0),a(1))};
    case 0x2e6534: arity(args,1,1); return {root_can_use(self,a(0))};
    case 0x2e6598: arity(args,6,6); return {root_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2e7224: arity(args,1,1); return {crit_can_use(self,a(0))};
    case 0x2e74b0: arity(args,2,6); return {crit_cast(self,a(0),a(1))};
    case 0x2eb970: arity(args,1,1); return {undeadshield_can_use(self,a(0))};
    case 0x2eb7cc: arity(args,2,6); return {undeadshield_cast(self,a(0),a(1))};
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
  calls.push_back({slot, canonical, {}});
  const auto found = callbacks.find(slot);
  if (found == callbacks.end() || !found->second) throw Unsupported("Skill body needs explicit callback");
  auto callback = found->second;
  return callback(*this, canonical);
}
namespace {
constexpr Raw FRAME = 0x44fdcc, PLAYERS = 0x4d8af0, RECORD = 0x4c30, PVP = 0x4f711c;
struct Buff {
  Raw table, getter, kind, cap, holder, off;
  std::array<Raw, 12> light;
  std::array<Raw, 4> flash;
  std::array<Raw, 2> sound;
};
const Buff &buff(unsigned sid) {
  static const Buff lightning{0x5199d0,0x3ceba4,0x18,20,6,0,
    {0x42de0000,0x42e00000,0x431a0000,0x437f0000,0x42040000,0x41f00000,0x42580000,0x434d0000,0x42dc0000,0,0x3f1999aa,0x3effffbc},
    {0x6f,0x70,0x9a,0x1e},{0x54fe88,0x54fe90}};
  static const Buff cold{0x519b40,0x3ced8c,0x16,0,4,1,
    {0x42340000,0x42480000,0x437f0000,0x43480000,0x42d80000,0x42fa0000,0x43740000,0x43480000,0x42c80000,0,0x3f1ef9db,0x3effff15},
    {0x2d,0x32,0xff,0},{0x550060,0x550068}};
  static const Buff poison{0x51c278,0x3d1b14,0x17,10,5,0,
    {0x420c0000,0x43000000,0x42820000,0x43000000,0x420c0000,0x43000000,0x42820000,0x43000000,0x42dc0000,0,0x3f199999,HALF},
    {0x23,0x80,0x41,0x1e},{0x552e78,0x552e80}};
  switch (sid) { case 12: return lightning; case 14: return cold; case 26: return poison; }
  throw Unsupported("weapon buff id requires 12, 14 or 26");
}
}
Values Model::call(const std::string &name, const Values &args) {
  if (depth_ >= 64 || calls.size() >= creature_hp::MAX_HISTORY)
    throw Unsupported("named callback depth or history limit");
  calls.push_back({0, args, name});
  const auto it = named_callbacks.find(name);
  if (it == named_callbacks.end() || !it->second) throw Unsupported("missing named callback: " + name);
  auto cb = it->second;
  ++depth_;
  struct Guard { std::size_t &d; ~Guard() { --d; } } guard{depth_};
  return cb(*this, args);
}
Raw Model::named_scalar(const std::string &name, const Values &args) {
  const auto result = call(name,args);
  if (result.size() != 1) throw Unsupported("named callback scalar required: " + name);
  return result[0];
}
Vec3 Model::vec(Address at) const {
  return {Raw(mem.load(at,4)),Raw(mem.load(at+4,4)),Raw(mem.load(at+8,4))};
}
Raw Model::signed_byte(Address at) const {
  const auto byte = Raw(mem.load(at,1));
  return byte & 0x80 ? byte | 0xffffff00u : byte;
}
Values Model::table_call(const std::string &cls, Raw obj, unsigned offset, unsigned slot, const Values &args) {
  if (cls == "Spell" && offset == 0x14) return spell_call(obj,slot,args);
  if (slot > 63) throw Unsupported("named virtual slot limit");
  const auto table = Raw(mem.load(Address(obj)+offset,4));
  const auto row = Address(table)+8*slot;
  const auto self=obj + Raw(s16(Raw(mem.load(row,2))));
  if (cls=="Creature" && offset==0x48 && mem.load(row+4,4)==0x2d470c && !external_named.count("Creature.v"+std::to_string(slot))) {
    arity(args,0,0); firestorm_run(self); return {};
  }
  if (cls=="Creature" && offset==0x48 && mem.load(row+4,4)==0x2cb65c && !external_named.count("Creature.v"+std::to_string(slot))) {
    arity(args,0,0); diseasetrail_run(self); return {};
  }
  Values lanes{self};
  lanes.insert(lanes.end(),args.begin(),args.end());
  return call(cls+".v"+std::to_string(slot),lanes);
}
Values Model::spell_call(Raw spell, unsigned slot, const Values &args) {
  if (depth_ >= 64 || slot > 63) throw Unsupported("Spell depth or slot limit");
  ++depth_;
  struct Guard { std::size_t &d; ~Guard() { --d; } } guard{depth_};
  const auto table = Raw(mem.load(Address(spell)+0x14,4));
  const auto row = Address(table)+8*slot;
  const auto body = Raw(mem.load(row+4,4));
  if (body==0x2cc248 && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,3,4);
    diseasetrail_spell_run(spell+Raw(s16(Raw(mem.load(row,2)))),{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2e2ac4 || body == 0x2e5578 || body == 0x3164fc)) {
    arity(args,3,4);
    const auto self = spell+Raw(s16(Raw(mem.load(row,2))));
    weaponbuff_run(body == 0x2e2ac4 ? 12 : body == 0x2e5578 ? 14 : 26,
                   self,{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2ea74c || body == 0x2eb170)) {
    arity(args,3,4);
    const auto self = spell+Raw(s16(Raw(mem.load(row,2))));
    if (body == 0x2ea74c) undeadshield_run(self,{args[0],args[1],args[2]});
    else secondary_shield_run(self);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2e699c || body == 0x2e7acc)) {
    arity(args,0,4);
    const auto self=spell+Raw(s16(Raw(mem.load(row,2))));
    if (body == 0x2e699c) root_spell_run(self); else crit_spell_run(self);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2fae80 || body == 0x2fb67c)) {
    const auto self=spell+Raw(s16(Raw(mem.load(row,2))));
    if (body == 0x2fb67c) { arity(args,0,0); return {shieldbash_done(self)}; }
    arity(args,0,4); shieldbash_run(self); return {};
  }
  Values lanes{spell + Raw(s16(Raw(mem.load(row,2))))};
  lanes.insert(lanes.end(),args.begin(),args.end());
  return call("Spell.v"+std::to_string(slot),lanes);
}
Raw Model::spell_rand(Raw spell) {
  const auto at = Address(spell)+0x10;
  const Raw seed = Raw(mem.load(at,4))*0x19660du+0x3c6ef35fu;
  write(at,4,seed); return seed >> 16;
}
void Model::spell_base(Raw spell, Raw level, Raw owner, Raw skill, Raw table) {
  write(spell,1,level); write(Address(spell)+4,4,0); write(Address(spell)+8,4,skill);
  write(Address(spell)+0xc,4,owner);
  const auto seed = draw(); write(Address(spell)+0x10,4,seed);
  write(Address(spell)+0x14,4,table);
}
Raw Model::spell_type(Raw spell) {
  const auto table = Raw(mem.load(Address(spell)+0x14,4));
  const auto getter = Raw(mem.load(Address(table)+4,4));
  const auto it = type_info.find(getter);
  return it != type_info.end() ? it->second : named_scalar("Spell.v0",{spell});
}
Raw Model::find_spell_by_type(Raw creature, Raw type) {
  auto node = Raw(mem.load(Address(creature)+0x364,4));
  std::size_t visited=0;
  while (node) {
    if (++visited > creature_hp::MAX_HISTORY) throw Unsupported("spell list traversal limit");
    const auto spell = Raw(mem.load(Address(node)+0xc,4));
    if (spell && spell_type(spell) == type) return spell;
    node = Raw(mem.load(node,4));
  }
  return 0;
}
Raw Model::carries(Raw creature, Raw getter) {
  const auto it = type_info.find(getter);
  if (it == type_info.end()) throw Unsupported("missing type-info getter identity");
  return find_spell_by_type(creature,it->second);
}
void Model::add_spell(Raw creature, Raw spell) {
  if (spell) call("handle_link",{creature+0x364,spell+4,spell});
}
Values Model::weapons_of(Raw player) const {
  const Raw rec = PLAYERS + Raw(mem.load(Address(player)+0x3c8,4))*RECORD;
  const auto active = s32(signed_byte(Address(rec)+0x4a7c));
  const auto at = std::int64_t(rec)+0x4434+4*std::int64_t(active);
  if (at < 0 || at > UINT32_MAX) throw Unsupported("weapon record address outside raw32");
  return {Raw(mem.load(Address(at),4)),Raw(mem.load(Address(rec)+0x443c,4))};
}
void Model::enchant(Raw item, Raw kind, Raw value, Raw frames, Raw limit) {
  if (!item || (mem.load(Address(item)+8,8)&0x1000000)) return;
  std::int32_t cur=0;
  for (unsigned k=0;k<4;++k) {
    if (s16(Raw(mem.load(Address(item)+0x24+8*k,2))) == s32(kind)) {
      cur=s16(Raw(mem.load(Address(item)+0x26+8*k,2))); break;
    }
  }
  if (cur < 0 || cur >= s32(limit) || cur > s32(value)) return;
  const auto amount=from_int(value);
  const auto high=Raw(round_half_away(add(amount,mul(amount,0x3e4ccccc))));
  for (unsigned k=0;k<4;++k) {
    const auto at=Address(item)+0x24+8*k;
    const auto type=s16(Raw(mem.load(at,2)));
    if (type == -1 || type == s32(kind)) {
      write(at,2,kind); write(at+2,2,value); write(at+4,2,high); write(at+6,2,frames); break;
    }
  }
}
Raw Model::weaponbuff_can_use(unsigned sid, Raw caster) {
  return carries(caster,buff(sid).getter) ? 0 : 1;
}
Raw Model::weaponbuff_cast(unsigned sid, Raw skill, Raw level, Raw caster) {
  buff(sid);
  if (sid == 26) level=scalar(skill,14,{level,1});
  const auto spell=named_scalar("pool_new",{0x24});
  if (spell) weaponbuff_ctor(sid,spell,level,caster,skill);
  return spell;
}
void Model::weaponbuff_ctor(unsigned sid, Raw spell, Raw level, Raw owner, Raw skill) {
  const auto &p=buff(sid);
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  spell_base(spell,level,owner,skill,p.table);
  const auto seconds=scalar(skill,12,{1,level,owner,p.cap});
  write(Address(spell)+0x18,4,u(FRAME)+seconds*60u);
  if (sid == 26) {
    write(Address(spell)+0x1c,4,u(FRAME)+30);
    const auto r=scalar(skill,10,{2,signed_byte(spell)});
    write(Address(spell)+0x20,4,mul(from_int(r),0x41400000));
  } else {
    const auto r=scalar(skill,12,{2,signed_byte(spell),owner,10});
    write(Address(spell)+0x1c,4,r*12u);
    write(Address(spell)+0x20,4,Raw(to_int(add(from_int(u(FRAME)),sid==12 ? 0x41efffc0 : 0x41b3ff5a))));
  }
  const auto dmg=scalar(skill,12,{0,signed_byte(spell),owner,0});
  const auto frames=scalar(skill,12,{1,signed_byte(spell),owner,p.cap})*60u;
  owner=u(Address(spell)+0xc);
  if (!(u(Address(owner)+0xc)&0x40)) {
    const auto holder=u(Address(owner)+0x50);
    if (holder) table_call("Holder",holder,0,p.holder,{1});
  } else {
    for (auto item : weapons_of(owner)) enchant(item,p.kind,dmg,frames,frames);
  }
  // Position is evaluated before allocation; a callback can mutate the owner.
  const auto pos=vec(Address(u(Address(spell)+0xc))+0x10);
  const auto block=named_scalar("pool_new",{0xbc});
  if (block) {
    Values args{block,pos[0],pos[1],pos[2],0,0};
    args.insert(args.end(),p.light.begin(),p.light.end());
    call("LightEffect_ctor",args); ++hidden_draws;
  }
  const auto flash_owner=u(Address(spell)+0xc);
  for (unsigned i=0;i<4;++i) write(Address(flash_owner)+0x98+2*i,2,p.flash[i]);
  const auto resource=named_scalar("res_find",{p.sound[0],p.sound[1]});
  const auto sound_pos=vec(Address(u(Address(spell)+0xc))+0x10);
  call("sound_playAtPos",{resource,sound_pos[0],sound_pos[1],sound_pos[2],ONE});
}
void Model::weaponbuff_run(unsigned sid, Raw spell, const Vec3 &pos, Raw) {
  const auto &p=buff(sid);
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  const auto frame=s32(u(FRAME));
  auto owner=u(Address(spell)+0xc);
  if (frame >= s32(u(Address(spell)+0x18))) {
    if ((u(Address(owner)+0xc)&0x60)==0x20 && u(Address(owner)+0x50))
      table_call("Holder",u(Address(owner)+0x50),0,8,{p.off,0});
    if (spell) spell_call(spell,1,{3});
    return;
  }
  Raw radius;
  if (sid == 26) {
    radius=u(Address(spell)+0x20); if (eq(radius,ZERO)) return;
  } else {
    if (frame < s32(u(Address(spell)+0x20))) {
      if (sid == 12) {
        const auto result=table_call("Creature",owner,0x48,16,{});
        if (result.size()!=1) throw Unsupported("Creature.v16 scalar required");
        owner=u(Address(spell)+0xc);
        const auto at=vec(Address(owner)+0x10);
        call("fx_shockModel",{result[0],owner+0x114,at[0],at[1],at[2],Raw(s16(Raw(mem.load(Address(owner)+0x38,2)))),
                             owner+0x320,0x5e808080,0x409fff79,u(Address(owner)+0xdc),u(Address(owner)+0xe0)});
      } else {
        for (unsigned n=0;n<8;++n) {
          constexpr Raw k=0x421fff8b;
          const auto c=mul(cos16(Raw(s16(spell_rand(spell)))),k);
          const auto x=mul(c,sin16(Raw(s16(spell_rand(spell)>>1))));
          const auto s=mul(sin16(Raw(s16(spell_rand(spell)))),k);
          const auto y=mul(s,sin16(Raw(s16(spell_rand(spell)>>1))));
          const auto z=mul(cos16(Raw(s16(spell_rand(spell)>>1))),k);
          const Vec3 point{add(x,pos[0]),add(y,pos[1]),add(z,pos[2])};
          call("particle_spawn",{n<6 ? 0x8e5e10u : 0x8e5e60u,point[0],point[1],point[2],
                                sub(pos[0],point[0]),sub(pos[1],point[1]),sub(pos[2],point[2]),ZERO});
        }
      }
    }
    if (!u(Address(spell)+0x1c)) return;
    radius=from_int(u(Address(spell)+0x1c));
  }
  owner=u(Address(spell)+0xc);
  if (!(u(Address(owner)+0xc)&0x40) || s32(u(FRAME))%30 != 0) return;
  const auto found=call("world_collectObjectsInBox",{sub(pos[0],radius),sub(pos[1],radius),sub(pos[2],radius),
                                                    add(pos[0],radius),add(pos[1],radius),add(pos[2],radius),0x40});
  if (found.size()>creature_hp::MAX_HISTORY) throw Unsupported("box callback result limit");
  owner=u(Address(spell)+0xc);
  for (auto other : found) {
    if (other==owner || !(u(Address(other)+0xc)&0x40)) continue;
    if (u(PVP)) {
      const auto result=table_call("Creature",u(Address(spell)+0xc),0x48,22,{other});
      if (result.size()!=1) throw Unsupported("Creature.v22 scalar required");
      if (!result[0]) continue;
    }
    const auto weapons=weapons_of(other);
    const auto skill=u(Address(spell)+8);
    const auto aura=sid==14 ? scalar(skill,12,{3,signed_byte(spell),u(Address(spell)+0xc),0})
                            : scalar(skill,10,{3,signed_byte(spell)});
    enchant(weapons[0],p.kind,aura,31,31); enchant(weapons[1],p.kind,aura,31,31);
  }
}
Raw Model::undeadshield_can_use(Raw, Raw caster) { return carries(caster,0x3cf3ac) ? 0 : 1; }
Raw Model::undeadshield_cast(Raw skill, Raw level, Raw caster) {
  const auto spell = named_scalar("pool_new",{0x64});
  if (spell) undeadshield_ctor(spell,level,caster,skill);
  return spell;
}
void Model::undeadshield_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  spell_base(spell,level,owner,skill,0x51a048);
  write(Address(spell)+0x29,1,0);
  const auto pos=vec(Address(owner)+0x10);
  write_vec(Address(spell)+0x50,pos);
  const auto never=div(ONE,mul(ZERO,ZERO));
  for (const auto &p : std::array<std::pair<unsigned,Raw>,5>{{{0x48,never},{0x4c,0x3eccccee},{0x3c,0},{0x44,0},{0x40,never}}})
    write(Address(spell)+p.first,4,p.second);
  for (const auto &p : std::array<std::pair<unsigned,Raw>,8>{{{0x34,200},{0x36,100},{0x2c,200},{0x2e,100},{0x32,250},{0x2a,250},{0x38,150},{0x30,150}}})
    write(Address(spell)+p.first,2,p.second);
  write(Address(spell)+0x28,1,0);
  call("light_link",{spell+0x20});
  const auto seconds=scalar(Raw(mem.load(Address(spell)+8,4)),10,{1,signed_byte(spell)});
  write(Address(spell)+0x1c,4,85);
  write(Address(spell)+0x18,4,Raw(mem.load(FRAME,4))+seconds*60);
  const auto group=named_scalar("particleGroup_create",{});
  write(Address(spell)+0x5c,4,group);
  const auto resource=named_scalar("res_find",{0x5505d8,0x5505e0});
  const auto point=vec(Address(Raw(mem.load(Address(spell)+0xc,4)))+0x10);
  const auto handle=named_scalar("sound_playAtPos",{resource,point[0],point[1],point[2],ONE});
  write(Address(spell)+0x60,4,handle);
  call("sound_release",{handle});
}
Vec3 Model::spell_ball(Raw spell) {
  for (unsigned tries=0;tries<65536;++tries) {
    Vec3 b;
    for (auto &x:b) { const auto f=mul(from_int(spell_rand(spell)),TWO_M16); x=sub(add(f,f),ONE); }
    if (!lt(ONE,add(add(mul(b[0],b[0]),mul(b[1],b[1])),mul(b[2],b[2])))) return b;
  }
  throw Unsupported("spell ball rejection limit");
}
void Model::spell_delete(Raw spell) { if (spell) spell_call(spell,1,{3}); }
void Model::flash_both(Raw creature, Raw a, Raw b, Raw c, Raw d) {
  const Values values{a,b,c,d};
  for (unsigned base : {0x90,0x98}) for (unsigned i=0;i<4;++i) write(Address(creature)+base+2*i,2,values[i]);
}
void Model::shield_glow(Raw spell, unsigned group) {
  const auto wave=sin16(Raw(mem.load(FRAME,4))*0x5b5);
  const auto owner=Raw(mem.load(Address(spell)+0xc,4));
  const auto five=mul(wave,0x40a00000);
  write(Address(owner)+0x94,2,Raw(to_int(add(five,0x427c0000))));
  write(Address(owner)+0x96,2,Raw(to_int(add(add(wave,wave),0x41a00000))));
  write(Address(owner)+0x90,2,Raw(to_int(add(five,0x42c80000))));
  write(Address(owner)+0x92,2,Raw(to_int(add(five,0x429a0000))));
  const auto matrix=mat34_set_pos_rot_z(vec(Address(owner)+0x10),Raw(s16(Raw(mem.load(Address(owner)+0x38,2)))));
  Values lanes{Raw(mem.load(Address(spell)+group,4))}; lanes.insert(lanes.end(),matrix.begin(),matrix.end());
  call("particleGroup_setMatrix",lanes);
  call("particleGroup_begin",{Raw(mem.load(Address(spell)+group,4))});
  const auto models=table_call("Creature",Raw(mem.load(Address(spell)+0xc,4)),0x48,16,{});
  if (models.size()!=1) throw Unsupported("model scalar required");
  const auto model=models[0];
  const auto count=named_scalar("model_getSamplePointCount",{model});
  if (count) for (unsigned i=0;i<4;++i) {
    const auto creature=Raw(mem.load(Address(spell)+0xc,4));
    const auto result=table_call("Creature",creature,0x48,16,{});
    if (result.size()!=1) throw Unsupported("model scalar required");
    const auto index=Raw(std::int64_t(spell_rand(spell)) % s32(count));
    auto point=call("model_getSamplePoint",{result[0],Raw(mem.load(Address(spell)+0xc,4))+0x114,ZERO,ZERO,ZERO,0,index,0,0,ONE});
    if (point.size()!=3) throw Unsupported("sample point requires three lanes");
    const auto b=spell_ball(spell);
    for (unsigned j=0;j<3;++j) point[j]=add(point[j],b[j]);
    const auto velocity=spell_ball(spell);
    call("particle_spawn",{0x8e6180,point[0],point[1],point[2],mul(velocity[0],0x41874e39),mul(velocity[1],0x41874e39),mul(velocity[2],0x41874e39),ZERO});
  }
  call("particleGroup_end",{Raw(mem.load(Address(spell)+group,4))});
}
void Model::undeadshield_run(Raw spell, const Vec3 &pos, Raw) {
  const auto frame=Raw(mem.load(FRAME,4));
  if (s32(frame)>=s32(Raw(mem.load(Address(spell)+0x18,4)))) { spell_delete(spell); return; }
  const auto wave=sin16(frame*0x5b5);
  const auto seconds=scalar(Raw(mem.load(Address(spell)+8,4)),10,{1,signed_byte(spell)});
  auto age=from_int(60-(seconds*60-(Raw(mem.load(Address(spell)+0x18,4))-frame)));
  age=mul(add(age,age),0x3c888888);
  if (lt(age,ZERO)) age=ZERO;
  if (lt(ZERO,age)) {
    const auto point=vec(Address(Raw(mem.load(Address(spell)+0xc,4)))+0x10);
    write_vec(Address(spell)+0x50,point);
    call("light_relink",{spell+0x20});
    const auto size=add(add(wave,wave),0x42f0001f);
    write(Address(spell)+0x3c,4,size); write(Address(spell)+0x44,4,size);
    const auto inverse=div(ONE,mul(size,size));
    write(Address(spell)+0x48,4,inverse); write(Address(spell)+0x40,4,inverse);
    if (lt(age,ONE)) {
      const std::array<Raw,4> shades{0x437a0000,0x43480000,0x42c80000,0x43160000};
      for (unsigned i=0;i<8;++i) write(Address(spell)+0x2a+2*i,2,Raw(to_int(mul(age,shades[i%4]))));
    }
  } else call("light_unlink",{spell+0x20});
  shield_glow(spell,0x5c);
  if (!mem.load(Address(spell)+0x1c,4) || s32(Raw(mem.load(FRAME,4)))%15) return;
  const auto half=from_int(Raw(mem.load(Address(spell)+0x1c,4)));
  auto found=call("world_collectObjectsInBox",{sub(pos[0],half),sub(pos[1],half),sub(pos[2],half),add(pos[0],half),add(pos[1],half),add(pos[2],half),64});
  if (found.size()>65536) throw Unsupported("box result limit");
  const auto owner=Raw(mem.load(Address(spell)+0xc,4));
  for (auto other:found) {
    if (other==owner || !(mem.load(Address(other)+0xc,4)&0x40)) continue;
    if (mem.load(PVP,4)) {
      const auto same=table_call("Creature",Raw(mem.load(Address(spell)+0xc,4)),0x48,22,{other});
      if (same.size()!=1) throw Unsupported("team scalar required");
      if (!same[0]) continue;
    }
    const auto first=carries(other,0x3cf3ac), second=carries(other,0x3cf3fc);
    if (first || second) continue;
    const auto fresh=named_scalar("pool_new",{0x24});
    if (fresh) secondary_shield_ctor(fresh,scalar(Raw(mem.load(Address(spell)+8,4)),10,{2,signed_byte(spell)}),other,HALF);
    add_spell(other,fresh);
  }
}
void Model::shield_reflect(Raw spell, Raw victim, Raw attacker, Raw incoming, Raw percent) {
  const auto share=mul(from_int(percent),0x3c23d70a);
  const auto amount=from_int(Raw(to_int(mul(from_int(incoming),share))));
  const auto fraction=mul(from_int(spell_rand(spell)),TWO_M16);
  const auto high=add(amount,mul(mul(amount,fraction),0x3e4ccccc));
  // Empty descriptor: four absent effect slots, encoded as raw32 -1 lanes.
  call("damage_apply",{attacker,victim,100000,0,0,0x40,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,amount,high});
}
void Model::undeadshield_damage_attacker(Raw spell, Raw victim, Raw attacker, Raw incoming) {
  if (!signed_byte(spell)) return;
  flash_both(victim,100,0x4d,0x3f,0x14);
  const auto percent=scalar(Raw(mem.load(Address(spell)+8,4)),10,{0,signed_byte(spell)});
  shield_reflect(spell,victim,attacker,incoming,percent);
}
void Model::secondary_shield_ctor(Raw spell, Raw percent, Raw victim, Raw seconds) {
  spell_base(spell,0,victim,0,0x51a018);
  write(Address(spell)+0x1c,4,percent);
  write(Address(spell)+0x18,4,Raw(to_int(add(from_int(Raw(mem.load(FRAME,4))),mul(seconds,0x42700000)))));
  const auto group=named_scalar("particleGroup_create",{}); write(Address(spell)+0x20,4,group);
}
void Model::secondary_shield_run(Raw spell) {
  if (s32(Raw(mem.load(FRAME,4)))>=s32(Raw(mem.load(Address(spell)+0x18,4)))) { spell_delete(spell); return; }
  shield_glow(spell,0x20);
}
void Model::secondary_shield_damage_attacker(Raw spell, Raw victim, Raw attacker, Raw incoming) {
  flash_both(victim,100,0x4d,0x3f,0x14);
  shield_reflect(spell,victim,attacker,incoming,Raw(mem.load(Address(spell)+0x1c,4)));
}
namespace {
constexpr Raw SUPPORT_DT=0x3c888888, ROOT_DELAY=0x3f04dcca;
constexpr Raw ALIVE_COUNT=0x4f8420, ALIVE_LIST=0x4f8440, CRIT_GETTER=0x3cf000;
Raw one_lane(const Values &v) {
  if (v.size()!=1) throw Unsupported("Creature scalar required");
  return v[0];
}
}
Raw Model::player_object(Raw slot) const {
  return Raw(mem.load(PLAYERS+slot*RECORD+0x43d8u,4));
}
Raw Model::root_can_use(Raw, Raw caster) {
  if (mem.load(PVP,4) && (mem.load(Address(caster)+0xc,4)&0x40)) {
    const auto text=named_scalar("text_lookup",{0x550178});
    call("hud_showMessage",{text,0x5a,Raw(mem.load(Address(caster)+0x3c8,4)),0});
    return 0;
  }
  return 1;
}
Raw Model::root_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing) {
  level=scalar(skill,14,{level,20});
  const auto sound=named_scalar("res_find",{0x550188,0x550190});
  call("sound_playOnObject",{sound,caster,ONE});
  const auto limit=s32(scalar(skill,10,{0,level}));
  const auto found=call("world_collectObjectsInArc",{pos[0],pos[1],pos[2],Raw(s16(facing)),STACK_QUERY,0,0x43780000,0x42f00000,ZERO});
  if (found.size()>65536) throw Unsupported("arc query count limit");
  std::int32_t made=0;
  for (auto other:found) {
    if (made>=limit) break;
    if (caster && !one_lane(table_call("Creature",caster,0x48,20,{other}))) continue;
    if ((mem.load(Address(other)+0xc,4)&0x1000000) || named_scalar("creature_bossRank",{other})) continue;
    if (s16(Raw(mem.load(Address(other)+0x314,2)))>=1) continue;
    const auto spell=named_scalar("pool_new",{0x34});
    if (spell) root_spell_ctor(spell,level,other,skill);
    add_spell(other,spell);
    ++made;
  }
  call("handle_clear",{STACK_QUERY}); call("handle_clear",{STACK_QUERY});
  return 0;
}
void Model::root_spell_ctor(Raw spell, Raw level, Raw victim, Raw skill) {
  spell_base(spell,level,victim,skill,0x519b70);
  write(Address(spell)+0x18,4,0); write(Address(spell)+0x1c,4,0);
  const auto group=named_scalar("particleGroup_create",{});
  write(Address(spell)+0x24,4,group);
  write_vec(Address(spell)+0x28,vec(Address(victim)+0x10));
  const auto duration=scalar(Raw(mem.load(Address(spell)+8,4)),10,{1,signed_byte(spell)});
  write(Address(spell)+0x20,4,duration);
}
void Model::root_spell_run(Raw spell) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  write(Address(spell)+0x18,4,add(u(Address(spell)+0x18),SUPPORT_DT));
  const auto phase=s32(u(Address(spell)+0x1c));
  auto owner=u(Address(spell)+0xc);
  if (phase==0) {
    const auto matrix=mat34_set_pos_rot_z(vec(Address(owner)+0x10),Raw(s16(Raw(mem.load(Address(owner)+0x38,2)))));
    call("particleGroup_begin",{u(Address(spell)+0x24)});
    Values lanes{u(Address(spell)+0x24)}; lanes.insert(lanes.end(),matrix.begin(),matrix.end());
    call("particleGroup_setMatrix",lanes);
    for (unsigned i=0;i<7;++i) {
      owner=u(Address(spell)+0xc);
      const auto fx=mul(from_int(spell_rand(spell)),TWO_M16);
      const auto x=mul(mul(add(fx,0xbf000000),u(Address(owner)+0x1c)),0x3f9fdebd);
      const auto fy=mul(from_int(spell_rand(spell)),TWO_M16);
      const auto y=mul(mul(add(fy,0xbf000000),u(Address(u(Address(spell)+0xc))+0x1c)),0x3f9fdebd);
      const auto z=add(u(Address(u(Address(spell)+0xc))+0x20),0x41400000);
      call("particle_spawn",{0x8e5f00,x,y,z,ZERO,ZERO,0xc2980000,ZERO});
    }
    call("particleGroup_end",{u(Address(spell)+0x24)});
    if (lt(ROOT_DELAY,u(Address(spell)+0x18))) {
      const auto roll=draw()%100+1;
      const auto chance=s32(scalar(u(Address(spell)+8),10,{2,signed_byte(spell)}));
      owner=u(Address(spell)+0xc);
      if (!(std::int64_t(roll)<chance)) {
        const Vec3 base{u(Address(owner)+0x10),u(Address(owner)+0x14),add(u(Address(owner)+0x18),u(Address(owner)+0x20))};
        for (unsigned i=0;i<100;++i) {
          const auto b=spell_ball(spell);
          call("particle_spawn",{0x8e5f50,base[0],base[1],base[2],mul(b[0],0x41a80000),mul(b[1],0x41a80000),mul(b[2],0x41a80000),ZERO});
        }
        spell_call(spell,1,{3}); return;
      }
      flash_both(owner,4,0x5c,0,0x51);
      if (!(u(Address(owner)+0xc)&0x40)) write(Address(owner)+0x314,2,u(Address(spell)+0x20)*60);
      write(Address(spell)+0x1c,4,u(Address(spell)+0x1c)+1);
    }
  } else if (phase==1) {
    const auto model=one_lane(table_call("Creature",owner,0x48,16,{}));
    const auto count=named_scalar("model_getSamplePointCount",{model});
    for (unsigned i=0;i<22;++i) {
      owner=u(Address(spell)+0xc);
      const auto current=one_lane(table_call("Creature",owner,0x48,16,{}));
      const auto d=spell_rand(spell);
      const auto index=count ? Raw(std::int64_t(d)%s32(count)) : d;
      owner=u(Address(spell)+0xc);
      const auto pos=vec(Address(owner)+0x10);
      auto point=call("model_getSamplePoint",{current,owner+0x114,pos[0],pos[1],pos[2],Raw(s16(Raw(mem.load(Address(owner)+0x38,2)))),index,1,0,ONE});
      if (point.size()!=3) throw Unsupported("sample point requires three lanes");
      spell_ball(spell);
      call("particle_spawn",{0x8e5eb0,point[0],point[1],point[2],ZERO,ZERO,ZERO,ZERO});
    }
    owner=u(Address(spell)+0xc);
    if (u(Address(owner)+0xc)&0x40) {
      const auto dx=sub(u(Address(owner)+0x10),u(Address(spell)+0x28));
      const auto dy=sub(u(Address(owner)+0x14),u(Address(spell)+0x2c));
      write(Address(owner)+0x2e8,4,mul(sqrt(add(mul(dx,dx),mul(dy,dy))),0x40800000));
      const auto angle=atan2_16(sub(u(Address(owner)+0x10),u(Address(spell)+0x28)),sub(u(Address(owner)+0x14),u(Address(spell)+0x2c)));
      write(Address(u(Address(spell)+0xc))+0x2e0,2,Raw(-0x4000-angle));
    }
  }
  if (lt(from_int(u(Address(spell)+0x20)),sub(u(Address(spell)+0x18),ROOT_DELAY))) spell_delete(spell);
}
Raw Model::crit_can_use(Raw, Raw caster) { return carries(caster,CRIT_GETTER) ? 0 : 1; }
Raw Model::crit_cast(Raw skill, Raw level, Raw caster) {
  const auto spell=named_scalar("pool_new",{0x5c});
  if (spell) crit_spell_ctor(spell,level,caster,skill);
  return spell;
}
void Model::crit_spell_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  spell_base(spell,level,owner,skill,0x519c28);
  write(Address(spell)+0x25,1,0);
  write_vec(Address(spell)+0x4c,vec(Address(owner)+0x10));
  for (const auto &p:std::array<std::pair<unsigned,Raw>,5>{{{0x40,0x42f00000},{0x38,0x42f00000},{0x44,0x3891a2b3},{0x3c,0x3891a2b3},{0x48,0}}}) write(Address(spell)+p.first,4,p.second);
  for (const auto &p:std::array<std::pair<unsigned,Raw>,8>{{{0x32,0x40},{0x28,0x40},{0x2a,0x40},{0x30,0x40},{0x2e,0xd7},{0x26,0xd7},{0x2c,0},{0x34,0}}}) write(Address(spell)+p.first,2,p.second);
  write(Address(spell)+0x24,1,0);
  call("light_link",{spell+0x1c});
  const auto duration=scalar(Raw(mem.load(Address(spell)+8,4)),10,{2,signed_byte(spell)});
  write(Address(spell)+0x18,4,from_int(duration));
  const auto group=named_scalar("particleGroup_create",{});
  write(Address(spell)+0x58,4,group);
}
void Model::crit_set_tint(Raw, Raw creature) {
  for (const auto &p:std::array<std::pair<unsigned,Raw>,4>{{{0x90,0x44},{0x96,0xf},{0x92,0xe},{0x94,0x18}}}) write(Address(creature)+p.first,2,p.second);
}
void Model::crit_spell_run(Raw spell) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  const auto left=sub(u(Address(spell)+0x18),SUPPORT_DT);
  write(Address(spell)+0x18,4,left);
  if (lt(left,ZERO)) { spell_delete(spell); return; }
  auto owner=u(Address(spell)+0xc);
  crit_set_tint(spell,owner);
  const auto seconds=scalar(u(Address(spell)+8),10,{2,signed_byte(spell)});
  const auto elapsed=sub(from_int(seconds),u(Address(spell)+0x18));
  if (le(elapsed,TWO)) {
    owner=u(Address(spell)+0xc);
    write_vec(Address(spell)+0x4c,{add(u(Address(owner)+0x10),ZERO),u(Address(owner)+0x14),add(u(Address(owner)+0x18),0x42700000)});
    call("light_relink",{spell+0x1c});
    if (lt(ONE,elapsed)) {
      const auto fade=sub(TWO,elapsed);
      const auto a=Raw(to_int(mul(fade,0x42800000))), b=Raw(to_int(mul(fade,0x43570000)));
      for (const auto &p:std::array<std::pair<unsigned,Raw>,6>{{{0x2a,a},{0x26,b},{0x2e,b},{0x30,a},{0x28,a},{0x32,a}}}) write(Address(spell)+p.first,2,p.second);
    }
  } else call("light_unlink",{spell+0x1c});
  const auto skill=u(Address(spell)+8), id=u(Address(skill)+0x64);
  const int k=id==0x21 ? 10 : id==0x51 ? 5 : 1;
  const auto level=signed_byte(spell);
  const Raw radius_bits=scalar(skill,10,{1,level})*12;
  const auto radius=s32(radius_bits);
  const auto ghost=named_scalar("net_objOrGhost",{u(Address(spell)+0xc)});
  if (radius>0) {
    call("particleGroup_begin",{u(Address(spell)+0x58)});
    const auto matrix=mat34_set_pos_rot_z(vec(Address(ghost)+0x10),Raw(s16(u(FRAME)*118)));
    Values lanes{u(Address(spell)+0x58)}; lanes.insert(lanes.end(),matrix.begin(),matrix.end());
    call("particleGroup_setMatrix",lanes);
    for (int i=0;i<std::max(s32(level)-k+14,0);++i) {
      const auto angle=Raw(s16(spell_rand(spell)));
      const Vec3 local{mul(cos16(angle),from_int(radius_bits)),mul(sin16(angle),from_int(radius_bits)),ZERO};
      const auto world=mat34_transform_point(matrix,local);
      const auto ground=named_scalar("terrain_heightAtPoint",{u(0x44fdc8),world[0],world[1],world[2]});
      const auto height=sub(ground,u(Address(ghost)+0x18));
      if (!lt(0x42900000,height) && !lt(height,0xc2900000))
        call("particle_spawn",{0x8e5fa0,local[0],local[1],height,ZERO,ZERO,ONE,ZERO});
    }
    call("particleGroup_end",{u(Address(spell)+0x58)});
  }
  const auto reach=from_int(radius_bits*radius_bits);
  const auto alive=std::max(s32(u(ALIVE_COUNT)),0);
  for (std::int32_t i=0;i<alive;++i) {
    if (i>=65536) throw Unsupported("alive player traversal limit");
    const auto other=player_object(u(Address(ALIVE_LIST)+4*Address(i)));
    owner=u(Address(spell)+0xc);
    if (other==owner) continue;
    if (u(PVP) && !one_lane(table_call("Creature",owner,0x48,22,{other}))) continue;
    owner=u(Address(spell)+0xc);
    const auto dy=sub(u(Address(owner)+0x14),u(Address(other)+0x14));
    const auto dx=sub(u(Address(owner)+0x10),u(Address(other)+0x10));
    if (lt(add(mul(dx,dx),mul(dy,dy)),reach)) crit_set_tint(spell,other);
  }
}
Raw Model::crit_bonus_for(Raw skill, Raw attacker, Raw rank) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  Raw level=0;
  if (rank && carries(attacker,CRIT_GETTER)) level=rank;
  else {
    const auto alive=std::max(s32(u(ALIVE_COUNT)),0);
    for (std::int32_t i=0;i<alive;++i) {
      if (i>=65536) throw Unsupported("alive player traversal limit");
      const auto slot=u(Address(ALIVE_LIST)+4*Address(i));
      if (slot==u(Address(attacker)+0x3c8)) continue;
      auto other=player_object(slot);
      Vec3 d;
      for (unsigned j=0;j<3;++j) d[j]=sub(u(Address(other)+0x10+4*j),u(Address(attacker)+0x10+4*j));
      if (!lt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])),0x49742400)) continue;
      if (u(PVP) && !one_lane(table_call("Creature",other,0x48,22,{attacker}))) continue;
      if (!carries(player_object(slot),CRIT_GETTER)) continue;
      auto best=s32(named_scalar("player_getSkillRank",{slot,0x35}));
      best=std::max(best,s32(named_scalar("player_getSkillRank",{slot,0x51})));
      best=std::max(best,s32(named_scalar("player_getSkillRank",{slot,0x21})));
      if (!best) continue;
      const auto radius=scalar(skill,10,{1,Raw(best)})*12;
      const auto reach=from_int(radius);
      other=player_object(slot);
      const auto dy=sub(u(Address(other)+0x14),u(Address(attacker)+0x14));
      const auto dx=sub(u(Address(other)+0x10),u(Address(attacker)+0x10));
      if (lt(add(mul(dx,dx),mul(dy,dy)),mul(reach,reach))) {
        if (best>=1) level=Raw(best);
        break;
      }
    }
  }
  level=scalar(skill,14,{level,20});
  if (!level) return ZERO;
  const auto percent=scalar(skill,10,{0,level});
  return mul(from_int(percent),0x3c23d70a);
}
Raw Model::light_effect(Raw size, const Vec3 &pos, const Values &numbers) {
  const auto block=named_scalar("pool_new",{size});
  if (block) {
    Values args{block,pos[0],pos[1],pos[2]};
    args.insert(args.end(),numbers.begin(),numbers.end());
    call("LightEffect_ctor",args); ++hidden_draws;
  }
  return block;
}
Raw Model::shieldbash_can_use(Raw, Raw caster) {
  if (mem.load(Address(caster)+0xc,4)&0x40) {
    const Raw rec=PLAYERS+Raw(mem.load(Address(caster)+0x3c8,4))*RECORD;
    if (!mem.load(Address(rec)+0x43fc,4) || signed_byte(Address(rec)+0x4a86)==4) {
      const auto text=named_scalar("text_lookup",{0x551258});
      call("hud_showMessage",{text,0xb4,Raw(mem.load(Address(caster)+0x3c8,4)),0});
      return 0;
    }
  }
  return 1;
}
Raw Model::shieldbash_cast(Raw skill, Raw level, Raw caster) {
  level=scalar(skill,14,{level,0});
  const auto spell=named_scalar("pool_new",{0x28});
  if (spell) shieldbash_ctor(spell,level,caster,skill);
  return spell;
}
void Model::shieldbash_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  const auto load=[&](Address a) { return Raw(mem.load(a,4)); };
  write(spell,1,level); write(Address(spell)+8,4,skill); write(Address(spell)+4,4,0);
  write(Address(spell)+0xc,4,owner); const auto seed=draw(); write(Address(spell)+0x10,4,seed);
  const auto lump=load(load(Address(owner)+0x24));
  write(Address(owner)+0x58c,4,6); write(Address(spell)+0x14,4,0x51ad70);
  const auto clip=named_scalar("res_find",{lump,0x551298});
  call("anim_startBlendInOut",{owner+0x114,clip,0x812,0x3dcccccc,0x3dcccccc});
  write(Address(clip)+0x20,1,2); write(Address(clip)+0x1c,4,0x3f181040);
  for (unsigned off : {0x20,0x1c,0x18,0x24}) write(Address(spell)+off,4,0);
  const auto pos=vec(Address(owner)+0x10);
  const auto found=call("world_collectObjectsInArc",{pos[0],pos[1],pos[2],Raw(s16(Raw(mem.load(Address(owner)+0x38,2)))),STACK_QUERY,load(Address(spell)+0x20),0x42c80000,0x42b40000,ZERO});
  if (found.size()>creature_hp::MAX_HISTORY) throw Unsupported("arc query limit");
  for (auto other : found) if (!(load(Address(other)+0xc)&0x1000000)) {
    write(Address(spell)+0x24,4,found.front()); break;
  }
  call("handle_clear",{STACK_QUERY});
}
void Model::shieldbash_windup_fx(Raw spell) {
  const auto load=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto anchor=named_scalar("net_objOrGhost",{load(Address(spell)+0xc)});
  const auto count=named_scalar("model_getSamplePointCount",{load(Address(anchor)+0x544)});
  for (unsigned i=0;i<203;++i) {
    const auto draw=spell_rand(spell);
    const auto divisor=s32(count);
    const Raw index=divisor ? Raw(std::int64_t(s32(draw))%divisor) : draw;
    const auto pos=vec(Address(anchor)+0x10);
    const auto point=call("model_getSamplePoint",{load(Address(anchor)+0x544),anchor+0x114,pos[0],pos[1],pos[2],Raw(s16(Raw(mem.load(Address(anchor)+0x38,2)))),index,1,0,ONE});
    if (point.size()!=3) throw Unsupported("sample point requires three lanes");
    const auto b=spell_ball(spell);
    call("particle_spawn",{0x8e6ab0,point[0],point[1],point[2],mul(b[0],0x41500000),mul(b[1],0x41500000),mul(b[2],0x41500000),ZERO});
  }
}
void Model::shieldbash_run(Raw spell) {
  const auto load=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto facing=[&](Raw obj) { return Raw(s16(Raw(mem.load(Address(obj)+0x38,2)))); };
  // Creature vcall uses the reference's +0x48 table, with signed16 this adjustment.
  const auto vcall=[&](Raw obj,unsigned slot,const Values &args) { return table_call("Creature",obj,0x48,slot,args); };
  const auto vscalar=[&](Raw obj,unsigned slot,const Values &args) {
    const auto result=vcall(obj,slot,args);
    if (result.size()!=1) throw Unsupported("Creature scalar required");
    return result[0];
  };
  const auto owner=load(Address(spell)+0xc);
  const auto events=named_scalar("Player_animStep",{owner,ONE});
  call("Player_move",{owner,3,0x41bfd8u+load(Address(owner)+0x3c8)*0x58u,0});
  const auto target=load(Address(spell)+0x24);
  if (target) { const auto p=vec(Address(target)+0x10); vcall(owner,26,{0,p[0],p[1],p[2]}); }
  if (events&2) write(Address(spell)+0x1c,4,1);
  if (!load(Address(spell)+0x18)) {
    if (!load(Address(spell)+0x1c)) shieldbash_windup_fx(spell);
    else {
      const auto most=s32(scalar(load(Address(spell)+8),10,{3,signed_byte(spell)}));
      const auto chance=s32(scalar(load(Address(spell)+8),10,{1,signed_byte(spell)}));
      const Raw frames=scalar(load(Address(spell)+8),10,{2,signed_byte(spell)})*60u;
      const auto level=s32(signed_byte(spell));
      const Raw knock=level>15 ? 0x200000 : level<6 ? 0x40000 : 0x80000;
      const auto pos=vec(Address(owner)+0x10);
      const auto hand=call("model_getAttachPoint",{load(Address(owner)+0x544),pos[0],pos[1],pos[2],facing(owner),owner+0x114,1,0,ONE});
      if (hand.size()!=3) throw Unsupported("attach point requires three lanes");
      const auto found=call("world_collectObjectsInArc",{hand[0],hand[1],hand[2],facing(owner),STACK_QUERY,0,0x42240000,0xbf800000,ZERO});
      if (found.size()>creature_hp::MAX_HISTORY) throw Unsupported("arc query limit");
      const auto damage=[&](Raw other) {
        const auto range=skill_call(load(Address(spell)+8),8,{0,signed_byte(spell),0,0x3e4ccccc});
        if (range.size()!=2) throw Unsupported("damage range requires two lanes");
        call("damage_apply",{other,owner,100000,0,0,knock|0x40,0,1,from_int(range[0]),from_int(range[1])});
      };
      std::int32_t hits=0;
      if (most>=1) for (auto other : found) {
        if (!(load(Address(other)+0xc)&0x1000000) && vscalar(owner,20,{other})) {
          damage(other);
          if (lt(ZERO,load(Address(other)+0x228)) && !(chance<std::int32_t(draw()%100+1)))
            vcall(other,30,{frames,1});
          else for (const auto &field : std::array<std::pair<unsigned,Raw>,7>{{{0x92,100},{0x96,0x40},{0x98,100},{0x90,100},{0x9a,100},{0x9c,0x40},{0x94,0x40}}})
            write(Address(other)+field.first,2,field.second);
          if (!load(Address(spell)+0x18)) {
            const Vec3 spot{add(load(Address(other)+0x10),ZERO),load(Address(other)+0x14),add(load(Address(other)+0x18),0x42800000)};
            light_effect(0xbc,spot,{0,0,0x43210000,0x430a0000,0x42800000,ZERO,ZERO,ZERO,ZERO,ZERO,0x43340000,0x42700000,HALF,0x3f01479d});
          }
          ++hits; write(Address(spell)+0x18,4,1);
        }
        if (!(hits<most)) break;
      }
      if (!load(Address(spell)+0x18)) {
        const auto p=vec(Address(owner)+0x10);
        auto more=call("world_collectInSphere",{p[0],p[1],p[2],facing(owner),STACK_QUERY,0x42240000});
        if (more.size()>creature_hp::MAX_HISTORY || more.size()+found.size()>creature_hp::MAX_HISTORY) throw Unsupported("sphere query limit");
        more.insert(more.end(),found.begin(),found.end());
        for (auto other : more) if (!(load(Address(other)+0xc)&0x1000060)) { damage(other); break; }
      }
      write(Address(spell)+0x18,4,1);
    }
  }
  if (!(mem.load(Address(owner)+0x11a,2)&0x10) || load(Address(owner)+0x450)) {
    write(Address(owner)+0x58c,4,2); spell_delete(spell);
  } else if (named_scalar("net_isLocalSlot",{load(Address(owner)+0x3c8)})) write(0x402797,1,60);
  call("handle_clear",{STACK_QUERY});
}
Raw Model::shieldbash_done(Raw spell) const { return Raw(mem.load(Address(spell)+0x18,4)); }
// Fire Storm uses the same caller-owned memory and explicit service callbacks.
Raw Model::obj_rand(Raw obj) {
  const Raw seed=Raw(mem.load(Address(obj)+0x44,4))*0x19660du+0x3c6ef35fu;
  write(Address(obj)+0x44,4,seed); return seed>>16;
}
Vec3 Model::obj_ball(Raw obj) {
  for (unsigned tries=0;tries<65536;++tries) {
    Vec3 b;
    for (auto &x:b) { const auto f=mul(from_int(obj_rand(obj)),TWO_M16); x=sub(add(f,f),ONE); }
    if (!lt(ONE,add(add(mul(b[0],b[0]),mul(b[1],b[1])),mul(b[2],b[2])))) return b;
  }
  throw Unsupported("object ball rejection limit");
}
void Model::delete_self(Raw obj) { if (obj) table_call("Creature",obj,0x48,1,{3}); }
void Model::gameobject_ctor(Raw obj,const Vec3 &pos,Raw definition,Raw props) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  for (unsigned off:{0,4,8,0xc,0x34}) write(Address(obj)+off,4,0);
  write(Address(obj)+0x48,4,0x5122a8);
  write(Address(obj)+0x1c,4,0x41c00000); write(Address(obj)+0x20,4,0x42900000);
  write(Address(obj)+0x28,1,0x19); write(Address(obj)+0x29,1,0x49);
  write_vec(Address(obj)+0x10,pos); write(Address(obj)+0x24,4,definition);
  if (definition && u(Address(definition)+4)!=u(0x41bf78)) {
    unsigned count=0;
    for (Address k=8;u(Address(definition)+k);k+=8) {
      if (++count>65536) throw Unsupported("class definition name limit");
      const auto result=named_scalar("res_find",{u(definition),u(Address(definition)+k)});
      write(Address(definition)+k+4,4,result);
    }
    write(Address(definition)+4,4,u(0x41bf78));
  }
  // Python's arithmetic shift after an unbounded signed addition.
  const auto grid_index=[](Raw v) {
    const auto n=std::int64_t(s32(Raw(to_int(v))))+0x3c00;
    return n>=0 ? n/256 : -((-n+255)/256);
  };
  const auto col=grid_index(u(Address(obj)+0x10)), row=grid_index(u(Address(obj)+0x14));
  const Raw cell=col>=0 && col<120 && row>=0 && row<120 ? Raw(0x5db4f0+4*(row*120+col)) : 0x5f76f0;
  call("handle_link",{cell,obj+8,obj});
  if (named_scalar("obj_getPropInt",{obj,0x5321a8,0})) throw Unsupported("hidden object grid relocation is unmodeled");
  write(Address(obj)+0x2c,4,props); const auto seed=draw(); write(Address(obj)+0x44,4,seed);
  if (props) throw Unsupported("object properties are unmodeled");
}
Raw Model::firestorm_can_use(Raw skill,Raw caster) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  if (!(u(Address(caster)+0xc)&0x40)) return 1;
  const auto rank=named_scalar("player_getSkillRank",{u(Address(caster)+0x3c8),7});
  const auto reach=from_int(scalar(skill,10,{1,rank}));
  const Raw facing=Raw(s16(Raw(mem.load(Address(caster)+0x38,2))));
  const auto x=add(mul(mul(cos16(facing),reach),0x41400000),u(Address(caster)+0x10));
  const auto y=add(mul(mul(sin16(facing),reach),0x41400000),u(Address(caster)+0x14));
  const auto ground=named_scalar("terrain_height",{u(0x44fdc8),x,y,ZERO,0x41400000});
  return lt(sub(ground,u(Address(caster)+0x18)),0xc3100000) ? 0 : 1;
}
Raw Model::firestorm_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  const auto reach=from_int(scalar(skill,12,{1,level,caster,10}));
  Raw ox=ZERO,oy=ZERO;
  if (mem.load(Address(caster)+0xc,4)&0x40) {
    ox=mul(mul(cos16(Raw(s16(facing))),reach),0x41400000);
    oy=mul(mul(sin16(Raw(s16(facing))),reach),0x41400000);
  }
  const auto x=add(ox,pos[0]),y=add(oy,pos[1]);
  const auto ground=named_scalar("terrain_height",{Raw(mem.load(0x44fdc8,4)),x,y,ZERO,reach});
  const Vec3 centre{x,y,add(add(ZERO,ground),0x40c00000)};
  const auto obj=named_scalar("pool_new",{0xb4});
  if (obj) firestorm_ctor(obj,level,caster,centre,skill);
  return 0;
}
void Model::firestorm_base(Raw obj,const Vec3 &pos,Raw owner) {
  gameobject_ctor(obj,pos,0,0);
  write(Address(obj)+0x48,4,0x518c98); write(Address(obj)+0x4c,4,0);
  write(Address(obj)+0x59,1,0); write_vec(Address(obj)+0x80,pos); write(Address(obj)+0x7c,4,0x3f004156);
  const auto never=div(ONE,mul(ZERO,ZERO));
  write(Address(obj)+0x78,4,never); write(Address(obj)+0x70,4,never);
  for (const auto &p:std::array<std::pair<unsigned,Raw>,8>{{{0x62,0xff},{0x64,0x80},{0x68,0xfff0},{0x5a,0xff},{0x5c,0x80},{0x5e,0},{0x60,0xfff0},{0x66,0}}}) write(Address(obj)+p.first,2,p.second);
  write(Address(obj)+0x6c,4,0); write(Address(obj)+0x74,4,0); write(Address(obj)+0x58,1,0);
  call("light_link",{obj+0x50}); if (owner) call("handle_link",{obj+0x4c,owner,owner});
}
void Model::firestorm_ctor(Raw obj,Raw level,Raw owner,const Vec3 &pos,Raw skill) {
  firestorm_base(obj,pos,owner); const Raw handle=obj+0x4c;
  auto result=scalar(skill,13,{0,level,handle,0}); write(Address(obj)+0x8c,4,result);
  result=scalar(skill,13,{2,level,handle,20}); write(Address(obj)+0x94,4,result*60u);
  const auto r=scalar(skill,13,{1,level,handle,10});
  write(Address(obj)+0xb0,4,0); write(Address(obj)+0x9c,4,r*12u); write_vec(Address(obj)+0x10,pos);
  const Raw k=owner && (mem.load(Address(owner)+0xc,4)&0x40) ? 0x3d4ccccc : 0x3ca3d70a;
  write(Address(obj)+0xa4,4,add(mul(mul(from_int(level),k),HALF),0x3e999999));
  write(Address(obj)+0xa8,4,15); firestorm_init_common(obj,owner);
}
void Model::firestorm_ctor2(Raw obj,Raw owner,const Vec3 &pos,Raw radius,Raw damage,Raw duration,Raw period,Raw scale) {
  firestorm_base(obj,pos,owner);
  write(Address(obj)+0x8c,4,damage); write(Address(obj)+0xb0,4,1); write(Address(obj)+0x94,4,duration);
  write(Address(obj)+0x9c,4,radius); write(Address(obj)+0xa8,4,period); write(Address(obj)+0xa4,4,scale);
  write_vec(Address(obj)+0x10,pos); firestorm_init_common(obj,owner);
}
void Model::firestorm_init_common(Raw obj,Raw owner) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  write(Address(obj)+0xc,4,u(Address(obj)+0xc)|0x421b); Raw start;
  if ((u(Address(owner)+0xc)&0x60)==0x20) {
    write(Address(obj)+0x98,4,Raw(to_int(add(from_int(u(FRAME)),0x4221ffff))));
    const auto r=u(Address(obj)+0x9c);
    const Raw near=from_int(s32(r)>300 ? 300 : r),far=from_int(s32(r<<1)>300 ? 300 : r<<1);
    const Vec3 pos{add(u(Address(obj)+0x10),ZERO),u(Address(obj)+0x14),add(u(Address(obj)+0x18),0x41400000)};
    light_effect(0xbc,pos,{0,1,0x437f0000,0x43000000,ZERO,0x42c80000,0x437f0000,0x43000000,ZERO,0x42c80000,near,far,0x3f266666,0x40019999});
    start=u(Address(obj)+0x98);
  } else { write(Address(obj)+0x98,4,0); start=u(FRAME); }
  write(Address(obj)+0x90,4,u(Address(obj)+0x94)+start); write(Address(obj)+0xa0,4,0);
  call("light_relink",{obj+0x50}); call("obj_runEveryFrame",{obj});
  if (!u(Address(obj)+0xb0)) {
    const auto resource=named_scalar("res_find",{0x54f110,0x54f150});
    const auto handle=named_scalar("sound_playOnObject",{resource,obj,ONE});
    write(Address(obj)+0xac,4,handle); call("sound_release",{handle});
  }
}
void Model::firestorm_run(Raw obj) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  const auto frame=s32(u(FRAME)); if (!(s32(u(Address(obj)+0x98))<frame)) return;
  const auto end=s32(u(Address(obj)+0x90));
  if (!(frame<end && u(Address(obj)+0x4c))) { delete_self(obj); return; }
  const auto left=std::int64_t(end)-frame;
  const auto radius=from_int(u(Address(obj)+0x9c));
  if (lt(u(Address(obj)+0xa0),radius) && s32(Raw(frame)-(Raw(end)-u(Address(obj)+0x94)))>=31) write(Address(obj)+0xa0,4,add(u(Address(obj)+0xa0),ONE));
  const auto glow=u(Address(obj)+0xa0); Raw size;
  if (left<61) size=fmin(mul(add(glow,glow),mul(from_int(Raw(left)),0x3c888888)),0x43960000);
  else {
    size=add(glow,glow);
    const auto flicker=add(sin16(Raw(frame)*3000u),sin16(Raw(frame)*1500u));
    const auto scale=fmin(mul(div(u(Address(obj)+0xa0),radius),0x41400000),0x43960000);
    size=add(size,from_int(Raw(to_int(mul(flicker,scale)))));
  }
  write(Address(obj)+0x6c,4,size); write(Address(obj)+0x74,4,size);
  const auto inverse=div(ONE,mul(size,size)); write(Address(obj)+0x78,4,inverse); write(Address(obj)+0x70,4,inverse);
  const auto period=s32(u(Address(obj)+0xa8));
  if ((period ? std::int64_t(frame)%period : frame)==0) {
    const auto p=obj_ball(obj);
    const Vec3 start{add(mul(p[0],radius),u(Address(obj)+0x10)),add(mul(p[1],radius),u(Address(obj)+0x14)),add(u(Address(obj)+0x18),0x437005b8)};
    Vec3 aim=start;
    if (!u(Address(obj)+0xb0)) { const auto q=obj_ball(obj); for (unsigned i=0;i<3;++i) aim[i]=add(mul(q[i],radius),u(Address(obj)+0x10+4*i)); }
    aim[2]=named_scalar("terrain_height",{u(0x44fdc8),aim[0],aim[1],aim[2],0x41400000});
    Vec3 d; for (unsigned i=0;i<3;++i) d[i]=sub(aim[i],start[i]);
    const auto inv=rsqrt(ONE,add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])));
    Vec3 velocity; for (unsigned i=0;i<3;++i) velocity[i]=mul(mul(d[i],inv),0x417002d2);
    const auto block=named_scalar("pool_new",{0x6c});
    if (block) { call("FireStormFlare_ctor",{block,start[0],start[1],start[2],velocity[0],velocity[1],velocity[2],u(Address(obj)+0xb0),u(Address(obj)+0xa4)}); ++hidden_draws; }
  }
  if (s32(u(FRAME))%30==0) {
    const auto centre=vec(Address(obj)+0x10);
    const auto found=call("world_collectObjectsInBox",{sub(centre[0],radius),sub(centre[1],radius),sub(centre[2],radius),add(centre[0],radius),add(centre[1],radius),add(centre[2],radius),0x40});
    if (found.size()>65536) throw Unsupported("Fire Storm query result limit");
    const auto owner=u(Address(u(Address(obj)+0x4c))+0xc); bool hit=false;
    for (const auto other:found) {
      if (!one_lane(table_call("Creature",owner,0x48,20,{other}))) continue;
      const Raw half=Raw(s32(u(Address(obj)+0x8c))/2);
      const auto r=obj_rand(obj);
      const Raw extra=Raw(to_int(mul(mul(from_int(half),mul(from_int(r),TWO_M16)),0x3e4ccccc)));
      hit=true;
      // Canonical descriptor: populated slot lanes then three absent-slot tokens.
      call("damage_apply",{other,owner,100000,0,0,0x40,0x15,half&0xffff,(half+extra)&0xffff,0,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,ZERO,ZERO});
    }
    if (hit) { const auto resource=named_scalar("res_find",{0x54f110,0x54f168}); call("sound_playOnObject",{resource,obj,ONE}); }
  }
}
// Disease Trail: scalar-pattern arithmetic and callback-visible rereads.
void Model::flash2(Raw creature,Raw a,Raw b,Raw c,Raw d) {
  const Values v{a,b,c,d};
  for (unsigned i=0;i<4;++i) write(Address(creature)+0x98+2*i,2,v[i]);
}
Values Model::box_around(const Vec3 &centre,Raw half,Raw capacity) {
  auto result=call("world_collectObjectsInBox",{sub(centre[0],half),sub(centre[1],half),sub(centre[2],half),
    add(centre[0],half),add(centre[1],half),add(centre[2],half),capacity});
  if (result.size()>65536) throw Unsupported("box query result limit");
  return result;
}
Raw Model::diseasetrail_can_use(Raw,Raw caster) { return carries(caster,0x3cd120) ? 0 : 1; }
Raw Model::diseasetrail_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw) {
  flash2(caster,Raw(-100),Raw(-100),Raw(-100),0);
  const auto spell=named_scalar("pool_new",{0x30});
  if (spell) diseasetrail_spell_ctor(spell,pos,level,caster,skill);
  return spell;
}
void Model::diseasetrail_spell_ctor(Raw spell,const Vec3 &pos,Raw level,Raw owner,Raw skill) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  spell_base(spell,level,owner,skill,0x518608);
  write(Address(spell)+0x18,4,0); write_vec(Address(spell)+0x1c,pos);
  write(Address(spell)+0x2c,4,from_int(Raw(to_int(add(from_int(u(FRAME)),0x43340000)))));
  const auto block=named_scalar("pool_new",{0x68});
  if (!block) return;
  diseasetrail_ctor(block,signed_byte(spell),vec(Address(spell)+0x1c),vec(Address(spell)+0x1c),u(Address(spell)+0xc),u(Address(spell)+8));
  call("handle_link",{spell+0x18,block,block});
  const auto resource=named_scalar("res_find",{0x54ea78,0x54eaa0});
  const auto centre=vec(Address(u(Address(spell)+0xc))+0x10);
  call("sound_playAtPos",{resource,centre[0],centre[1],centre[2],ONE});
}
void Model::trail_puffs(const std::function<Raw()> &random,const std::function<Vec3()> &ball,
                        const Vec3 &near,const Vec3 &far) {
  const std::array<Raw,3> defs{0x8e5190,0x8e51e0,0x8e5230};
  for (unsigned kind=0;kind<3;++kind) for (unsigned n=0;n<(kind==2 ? 2u : 1u);++n) {
    const auto t=mul(from_int(random()),TWO_M16), other=sub(ONE,t);
    Vec3 point;
    for (unsigned k=0;k<3;++k) point[k]=add(mul(near[k],t),mul(far[k],other));
    const auto spread=ball();
    for (unsigned k=0;k<3;++k) point[k]=add(point[k],mul(spread[k],0x41200000));
    const auto b=ball(); const Raw speed=kind==2 ? 0x41400000 : 0x40c00000;
    call("particle_spawn",{defs[kind],point[0],point[1],point[2],mul(b[0],speed),mul(b[1],speed),mul(b[2],speed),ZERO});
  }
}
void Model::diseasetrail_spell_run(Raw spell,const Vec3 &pos,Raw) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  if (!lt(from_int(u(FRAME)),u(Address(spell)+0x2c))) { spell_delete(spell); return; }
  trail_puffs([&] { return spell_rand(spell); },[&] { return spell_ball(spell); },pos,vec(Address(spell)+0x1c));
  const auto last=vec(Address(spell)+0x1c); Vec3 d;
  for (unsigned k=0;k<3;++k) d[k]=sub(last[k],pos[k]);
  if (!lt(0x42100000,sqrt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]))))) return;
  const auto dx=sub(pos[0],last[0]),dy=sub(pos[1],u(Address(spell)+0x20));
  if (!lt(0x4480060c,add(mul(dx,dx),mul(dy,dy)))) return;
  call("handle_clear",{spell+0x18});
  const auto block=named_scalar("pool_new",{0x68});
  if (block) {
    diseasetrail_ctor(block,signed_byte(spell),vec(Address(spell)+0x1c),pos,u(Address(spell)+0xc),u(Address(spell)+8));
    call("handle_link",{spell+0x18,block,block});
  }
  write_vec(Address(spell)+0x1c,pos);
}
void Model::diseasetrail_ctor(Raw obj,Raw level,const Vec3 &start,const Vec3 &end,Raw owner,Raw skill) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  gameobject_ctor(obj,start,0,0);
  write(Address(obj)+0x48,4,0x5184f0); write(Address(obj)+0xc,4,u(Address(obj)+0xc)|0x4421f);
  write(Address(obj)+0x5c,4,0);
  if (owner) call("handle_link",{obj+0x5c,owner,owner});
  write(Address(obj)+0x64,4,skill); write(Address(obj)+0x60,4,level);
  write_vec(Address(obj)+0x10,start); write(Address(obj)+0x1c,4,0x42040000); write(Address(obj)+0x20,4,0x42100000);
  call("obj_updateGridCell",{obj}); write_vec(Address(obj)+0x4c,end);
  const auto seconds=scalar(u(Address(obj)+0x64),10,{1,u(Address(obj)+0x60)});
  write(Address(obj)+0x58,4,u(FRAME)+seconds*60u); call("obj_runEveryFrame",{obj});
}
void Model::diseasetrail_run(Raw obj) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  if (!(s32(u(FRAME))<s32(u(Address(obj)+0x58)))) { delete_self(obj); return; }
  trail_puffs([&] { return obj_rand(obj); },[&] { return obj_ball(obj); },vec(Address(obj)+0x10),vec(Address(obj)+0x4c));
  if (s32(u(FRAME))%15) return;
  bool hit=false;
  for (const auto other:box_around(vec(Address(obj)+0x10),0x41980000,0x40)) {
    if ((u(Address(other)+0xc)&0x28)!=0x20) continue;
    Raw owner=0;
    if (u(Address(obj)+0x5c)) {
      owner=u(Address(u(Address(obj)+0x5c))+0xc);
      if (!one_lane(table_call("Creature",owner,0x48,20,{other}))) continue;
    }
    const auto damage=scalar(u(Address(obj)+0x64),10,{0,u(Address(obj)+0x60)});
    const auto amount=from_int(damage);
    const auto high=add(amount,mul(mul(amount,mul(from_int(obj_rand(obj)),TWO_M16)),0x3e4ccccc));
    hit=true;
    call("damage_apply",{other,owner,100000,0,0,0x40,0x26,damage&0xffff,damage&0xffff,0,
                         UINT32_MAX,UINT32_MAX,UINT32_MAX,0,amount,high});
  }
  if (hit) {
    const auto resource=named_scalar("res_find",{0x54ea78,0x54ea80});
    call("sound_playOnObject",{resource,obj,ONE}); delete_self(obj);
  }
}
} // namespace champions::simulation::rta::skills_a
