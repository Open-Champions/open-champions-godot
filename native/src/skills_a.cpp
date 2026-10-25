// MIT; Copyright (c) 2026 TrustlessMike and contributors.
// Authored from ref_skills_a.py at 356add27d50e50972b2a671b31e8f4ec93e6778c,
// SHA256 6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae.
#include "champions/skills_a.hpp"
#include "champions/detail/skill_kernel.hpp"
#include "champions/detail/skill_spell_kernel.hpp"
#include <climits>
#include <algorithm>
#include <array>

namespace champions::simulation::rta::skills_a {
namespace kernel = detail::skill_kernel;
namespace spells = detail::skill_spell_kernel;
Model::Model(Memory initial, std::map<std::string, Callback> services,
             std::set<std::string> forced)
    : mem(std::move(initial)), named_callbacks(std::move(services)),
      external_named(std::move(forced)) {
  for (unsigned slot=0;slot<=63;++slot)
    if (external_named.count("Skill.v"+std::to_string(slot))) external.insert(slot);
}
Descriptor descriptor(const std::vector<Values> &entries) {
  if (entries.size()>4) throw Unsupported("damage descriptor slot count");
  Descriptor result{};
  for (std::size_t i=0;i<entries.size();++i) {
    result[i]=entries[i];
    for (auto &lane:*result[i]) lane &= 0xffffu;
  }
  return result;
}
ImageExe::ImageExe(Load reader) : load(std::move(reader)) {}
std::string ImageExe::read(Address addr,std::int32_t size) const {
  if (size>65536) throw Unsupported("item image read length limit");
  std::string result;
  for (std::int32_t i=0;i<size;++i) {
    if (addr>UINT32_MAX || Address(i)>UINT32_MAX-addr) throw Unsupported("item image address");
    const auto at=addr+Address(i);
    const auto reader=load; // allow the callback to replace itself safely
    const auto byte=reader(at,1);
    if (byte>255) throw Unsupported("item image byte required");
    result.push_back(char(byte));
  }
  return result;
}
std::string ImageExe::cstr(Address addr) const {
  std::string result;
  for (std::size_t i=0;i<65536;++i) {
    if (addr>UINT32_MAX || i>UINT32_MAX-addr) throw Unsupported("item image address");
    const auto at=addr+i;
    const auto reader=load; // allow the callback to replace itself safely
    const auto byte=reader(at,1);
    if (byte>255) throw Unsupported("item image byte required");
    if (!byte) return result;
    result.push_back(char(byte));
  }
  throw Unsupported("item string length limit");
}
SharedDraws::SharedDraws(Model &owner) : model(&owner) {}
Raw SharedDraws::next() {
  if (!model) throw Unsupported("shared RNG model required");
  return model->draw();
}
Raw lcg(Raw seed) { return kernel::lcg_step(seed); }
Raw frac(Raw draw) { return mul(from_int(draw), TWO_M16); }
Raw unit(Raw draw) { const auto m=frac(draw); return sub(add(m,m),ONE); }
Raw round_half(Raw value) { return Raw(to_int(le(ZERO,value) ? add(value,HALF) : sub(value,HALF))); }
Raw spread_max(Raw base, Raw draw, Raw factor) { return add(base,mul(mul(base,frac(draw)),factor)); }
Raw rem(Raw a, Raw b) { return b ? Raw(std::int64_t(s32(a)) % s32(b)) : a; }
Raw quot(Raw a, Raw b) { return b ? Raw(std::int64_t(s32(a)) / s32(b)) : (s32(a)<0 ? 1 : UINT32_MAX); }
Raw half_toward_zero(Raw value) { return Raw(s32(value)/2); }
std::optional<Raw> carries_getter(const std::function<Raw(Address)> &read, Raw function) {
  const auto first=read(Address(function)+0xc);
  const auto second=read(Address(function)+0x18);
  if ((first>>26)==3 && second==(0x0c000000u | (0x11a710u>>2)))
    return (first & 0x3ffffffu)<<2;
  return std::nullopt;
}
Raw Model::frame() const { return Raw(mem.load(0x44fdcc,4)); }
bool Model::is_player(Raw obj) const { return obj && (mem.load(Address(obj)+0xc,4)&0x40); }
Raw Model::mana_cost(Raw skill, Raw level) { return scalar(skill,5,{level}); }
Raw Model::can_use_slot(Raw skill, Raw caster) { return scalar(skill,1,{caster}); }
Values Model::skill_call(Raw skill, unsigned slot, const Values &args) {
  return skill_dispatch(skill,slot,args,true);
}
Values Model::first_skill_call(Raw skill, unsigned slot, const Values &args) {
  return skill_dispatch(skill,slot,args,false);
}
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
  const Raw seed = kernel::lcg_step(Raw(mem.load(G_RAND_SEED, 4)));
  write(G_RAND_SEED, 4, seed);
  ++draws;
  return kernel::lcg_result(seed);
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
  return Raw(round_half_away(kernel::statistic_sum(kernel::statistic_base(Raw(base)), kernel::statistic_step(step, kernel::delta(level, first)))));
}
Raw Model::skill_stat_tier2(Raw skill, Raw stat, Raw level, Raw cap) const {
  if (!level) return 0;
  level = kernel::capped_level(level, cap);
  const auto first = s16(Raw(mem.load(indexed(skill, 0x38, 2, stat), 2)));
  if (kernel::tier2_fallback(first)) return skill_stat(skill, stat, level);
  if (s32(level) < first) return 0;
  const auto base = s16(Raw(mem.load(indexed(skill, 0x42, 2, stat), 2)));
  const auto step = Raw(mem.load(indexed(skill, 0x4c, 4, stat), 4));
  return Raw(round_half_away(kernel::statistic_sum(kernel::statistic_base(Raw(base)), kernel::statistic_step(step, kernel::delta(level, first)))));
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
    const auto combine = [&](Raw x) { return kernel::range_combine(value, x, i != 0); };
    Raw probe, bound;
    if (randomise) {
      probe = combine(kernel::range_random_scale(value, draw(), spread));
      bound = combine(kernel::range_random_scale(value, draw(), spread));
    } else probe = bound = combine(kernel::range_scale(value, spread));
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
Raw Model::cosmetic_draw() {
  const auto seed=lcg(Raw(mem.load(G_COSMETIC_SEED,4)));
  write(G_COSMETIC_SEED,4,seed);
  return kernel::lcg_result(seed);
}
Vec3 Model::cosmetic_ball() {
  // The existing write-history bound also bounds this rejection loop.
  for (;;) {
    const auto x=unit(cosmetic_draw());
    const auto y=unit(cosmetic_draw());
    const auto z=unit(cosmetic_draw());
    if (!lt(ONE,add(add(mul(x,x),mul(y,y)),mul(z,z)))) return {x,y,z};
  }
}
void Model::riposte_counter_attack(Raw skill,Raw defender,Raw attacker,Raw level) {
  if (!level) return;
  const auto dmg=scalar(skill,12,{0,level,defender,10});
  const auto chance=scalar(skill,12,{1,level,defender,10});
  if (to_int(mul(frac(draw()),0x42c80000))>=s32(chance)) return;
  flash_both(defender,0x82,0x82,0x83,0x55);
  const auto get_model=[&] {
    const auto value=vcall(defender,16,{});
    if (value.size()!=1) throw Unsupported("Riposte model scalar required");
    return value[0];
  };
  const auto count=named_scalar("model_getSamplePointCount",{get_model()});
  if (!count) return;
  for (unsigned i=0;i<200;++i) {
    const auto model=get_model();
    const auto index=rem(cosmetic_draw(),count);
    const auto p=vec(Address(defender)+0x10);
    const auto point=call("model_getSamplePoint",{model,defender+0x114,p[0],p[1],p[2],
        Raw(s16(Raw(mem.load(Address(defender)+0x38,2)))),index,0,0,ONE});
    if (point.size()!=3) throw Unsupported("Riposte sample point required");
    const auto ball=cosmetic_ball();
    Vec3 toward;
    for (unsigned j=0;j<3;++j) toward[j]=sub(Raw(mem.load(Address(attacker)+0x10+4*j,4)),Raw(mem.load(Address(defender)+0x10+4*j,4)));
    call("particle_spawn",{0x8e5aa0,point[0],point[1],point[2],toward[0],toward[1],toward[2],ZERO});
    if (i&1) call("particle_spawn",{0x8e5af0,point[0],point[1],point[2],
        mul(ball[0],0x424abc67),mul(ball[1],0x424abc67),mul(ball[2],0x424abc67),ZERO});
  }
  const auto amount=from_int(dmg);
  const auto high=spread_max(amount,draw());
  call("damage_apply",{attacker,defender,100000,0,0,0x40,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,1,amount,high});
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
Values Model::skill_dispatch(Raw skill, unsigned slot, const Values &args, bool recognise_can_use) {
  if (slot > 15 || depth_ >= 64) throw Unsupported("Skill slot or recursion limit");
  ++depth_;
  struct Guard { std::size_t &d; ~Guard() { --d; } } guard{depth_};
  const auto table = Raw(mem.load(Address(skill) + 0x60, 4));
  const auto row = Address(table) + 8*slot;
  const auto self = kernel::adjust_this(skill, Raw(mem.load(row, 2)));
  const auto body = Raw(mem.load(row + 4, 4));
  if (!external.count(slot)) {
    const auto a = [&](std::size_t i) { return i < args.size() ? args[i] : 0; };
    switch (body) {
    case 0x2e2368: arity(args,1,1); return {summon_can_use(self,a(0))};
    case 0x2e18a4: arity(args,6,6); return {summon_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2efef8: arity(args,1,1); return {hammer_can_use(self,a(0))};
    case 0x2efb20: arity(args,6,6); return {hammer_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2f9dec: arity(args,0,1); return {poisonrain_can_use(self,a(0))};
    case 0x2f9a3c: arity(args,6,6); return {poisonrain_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2f4d34: arity(args,5,6); return {repulse_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2f645c: arity(args,6,6); return {dismissundead_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2e000c: arity(args,1,1); return {blessed_can_use(self,a(0))};
    case 0x2dfeb8: arity(args,2,6); return {blessed_cast(self,a(0),a(1))};
    case 0x2f3b0c: arity(args,2,6); return {wizardbeam_cast(self,a(0),a(1))};
    case 0x2e4df8: arity(args,1,1); return {fireweapons_can_use(self,a(0))};
    case 0x2e4ad0: arity(args,2,6); return {fireweapons_cast(self,a(0),a(1))};
    case 0x2f123c: arity(args,0,1); return {coneoffire_can_use(self,a(0))};
    case 0x2f1078: arity(args,2,6); return {coneoffire_cast(self,a(0),a(1))};
    case 0x2ce2b0: arity(args,1,1); return {diseaseshield_can_use(self,a(0))};
    case 0x2cdff8: arity(args,2,6); return {diseaseshield_cast(self,a(0),a(1))};
    case 0x2f4b10: arity(args,0,1); return {blindinglight_can_use(self,a(0))};
    case 0x2f47dc: arity(args,6,6); return {blindinglight_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2cb368: arity(args,0,1); return {diseasebolt_can_use(self,a(0))};
    case 0x2cb1c0: arity(args,6,6); return {diseasebolt_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2ed20c: arity(args,0,1); return {holystrike_can_use(self,a(0))};
    case 0x2ecfcc: arity(args,6,6); return {holystrike_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2dd8b8: arity(args,1,1); return {fireflares_can_use(self,a(0))};
    case 0x2dd4e8: arity(args,5,6); return {fireflares_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
    case 0x2f8b24: arity(args,0,1); return {shockoffrost_can_use(self,a(0))};
    case 0x2f8840: arity(args,6,6); return {shockoffrost_cast(self,a(0),a(1),{a(2),a(3),a(4)},a(5))};
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
    default:
      // Imported Summon Skeleton bodies are known to the reference registry.
      // They must never fall through to instruction-shape inference.
      if (recognise_can_use && slot==1 && body!=0x2e2368 && body!=0x2e18a4) {
        const auto getter=carries_getter([&](Address at) { return Raw(mem.load(at,4)); },body);
        if (getter && type_info.count(*getter)) {
          arity(args,1,1); return {carries(args[0],*getter) ? 0u : 1u};
        }
        if (mem.load(body,4)==0x03e00008 && mem.load(Address(body)+4,4)==0x24020001)
          return {1};
      }
      break;
    }
  }
  // Slot 5 always enters the shape reader in the pinned reference, even forced external.
  if (slot == 5) { arity(args, 1, 1); return {mana_cost_at(body, self, args[0])}; }
  if (calls.size() >= creature_hp::MAX_HISTORY) throw Unsupported("Skill callback history limit");
  Values canonical{self}; canonical.insert(canonical.end(), args.begin(), args.end());
  calls.push_back({slot, canonical, {}});
  const auto found = callbacks.find(slot);
  if (found == callbacks.end() || !found->second) {
    const auto service=named_callbacks.find("Skill.v"+std::to_string(slot));
    if (service==named_callbacks.end() || !service->second) throw Unsupported("Skill body needs explicit callback");
    auto callback=service->second; return callback(*this,canonical);
  }
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
  if (cls=="Spell") return spell_dispatch(obj,offset,slot,args);
  if (slot>63) throw Unsupported("named virtual slot limit");
  const auto table=Raw(mem.load(Address(obj)+offset,4));
  const auto self=kernel::adjust_this(obj, Raw(mem.load(Address(table)+8*slot, 2)));
  Values lanes{self}; lanes.insert(lanes.end(),args.begin(),args.end());
  return call(cls+".v"+std::to_string(slot),lanes);
}
Values Model::vcall(Raw obj, unsigned slot, const Values &args) {
  if (depth_>=64) throw Unsupported("object dispatch depth limit");
  ++depth_;
  struct Guard { std::size_t &d; ~Guard() { --d; } } guard{depth_};
  if (slot > 63) throw Unsupported("named virtual slot limit");
  const auto table = Raw(mem.load(Address(obj)+0x48,4));
  const auto row = Address(table)+8*slot;
  const auto self=kernel::adjust_this(obj, Raw(mem.load(row, 2)));
  if (mem.load(row+4,4)==0x2d470c) {
    arity(args,0,0); firestorm_run(self); return {};
  }
  if (mem.load(row+4,4)==0x2cb65c) {
    arity(args,0,0); diseasetrail_run(self); return {};
  }
  {
    switch (Raw(mem.load(row+4,4))) {
    case 0x2ee068: arity(args,0,0); hammer_run(self); return {};
    case 0x2f9254: arity(args,0,0); poisonrain_run(self); return {};
    case 0x2f57b0: arity(args,0,0); holybolt_run(self); return {};
    case 0x2f5fb4: arity(args,1,1); holybolt_hit(self,args[0]); return {};
    case 0x2f42e4: arity(args,0,0); blindinglight_effect_run(self); return {};
    case 0x2ca1a8: arity(args,0,0); diseasebolt_run(self); return {};
    case 0x2caec8: arity(args,1,1); diseasebolt_hit(self,args[0]); return {};
    case 0x2ebc50: arity(args,0,0); holystrike_run(self); return {};
    case 0x2ecae8: arity(args,1,1); holystrike_hit(self,args[0]); return {};
    case 0x2dbf10: arity(args,0,0); fireflare_run(self); return {};
    case 0x2f77c0: arity(args,0,0); shockoffrost_run(self); return {};
    case 0x2f6b3c: arity(args,0,0); frostshard_run(self); return {};
    case 0x2f84e4: arity(args,1,1); shockoffrost_hit(self,args[0]); return {};
    case 0x2f71ac: arity(args,1,1); frostshard_hit(self,args[0]); return {};
    default: break;
    }
  }
  Values lanes{self};
  lanes.insert(lanes.end(),args.begin(),args.end());
  return call("Creature.v"+std::to_string(slot),lanes);
}
Values Model::spell_call(Raw spell, unsigned slot, const Values &args) {
  return spell_dispatch(spell,0x14,slot,args);
}
Values Model::spell_dispatch(Raw spell, unsigned offset, unsigned slot, const Values &args) {
  if (depth_ >= 64 || slot > 63) throw Unsupported("Spell depth or slot limit");
  ++depth_;
  struct Guard { std::size_t &d; ~Guard() { --d; } } guard{depth_};
  const auto table = Raw(mem.load(Address(spell)+offset,4));
  const auto row = Address(table)+8*slot;
  const auto body = Raw(mem.load(row+4,4));
  if (body==0x2e177c && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,0,4); summon_spell_run(kernel::adjust_this(spell, Raw(mem.load(row, 2)))); return {};
  }
  if (body==0x2efa04 && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,0,4); hammer_spell_run(kernel::adjust_this(spell, Raw(mem.load(row, 2)))); return {};
  }
  if ((body==0x2df42c || body==0x2df4bc) && !external_named.count("Spell.v"+std::to_string(slot))) {
    const auto self=kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body==0x2df42c) { arity(args,1,1); blessed_dtor(self,args[0]); }
    else { arity(args,0,4); blessed_run(self); }
    return {};
  }
  if (body==0x2f1a44 && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,3,4);
    wizardbeam_run(kernel::adjust_this(spell, Raw(mem.load(row, 2))),{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    return {};
  }
  if ((body==0x2e3b28 || body==0x2f02fc) && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,3,4); const auto self=kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body==0x2e3b28) fireweapons_run(self,{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    else coneoffire_run(self,{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    return {};
  }
  if ((body==0x2cd318 || body==0x2d5854) && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,3,4); const auto self=kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body==0x2cd318) diseaseshield_run(self,{args[0],args[1],args[2]});
    else disease_spell_run(self,{args[0],args[1],args[2]});
    return {};
  }
  if (body==0x2f4538 && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,0,4); blindinglight_spell_run(kernel::adjust_this(spell, Raw(mem.load(row, 2)))); return {};
  }
  if ((body==0x2dd1ac || body==0x2dd0c8) && !external_named.count("Spell.v"+std::to_string(slot))) {
    const auto self=kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body==0x2dd0c8) { arity(args,1,1); fireflares_dtor(self,args[0]); }
    else { arity(args,3,4); fireflares_run(self,{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0); }
    return {};
  }
  if (body==0x2cc248 && !external_named.count("Spell.v"+std::to_string(slot))) {
    arity(args,3,4);
    diseasetrail_spell_run(kernel::adjust_this(spell, Raw(mem.load(row, 2))),{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2e2ac4 || body == 0x2e5578 || body == 0x3164fc)) {
    arity(args,3,4);
    const auto self = kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    weaponbuff_run(body == 0x2e2ac4 ? 12 : body == 0x2e5578 ? 14 : 26,
                   self,{args[0],args[1],args[2]},args.size()==4 ? args[3] : 0);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2ea74c || body == 0x2eb170)) {
    arity(args,3,4);
    const auto self = kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body == 0x2ea74c) undeadshield_run(self,{args[0],args[1],args[2]});
    else secondary_shield_run(self);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2e699c || body == 0x2e7acc)) {
    arity(args,0,4);
    const auto self=kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body == 0x2e699c) root_spell_run(self); else crit_spell_run(self);
    return {};
  }
  if (!external_named.count("Spell.v"+std::to_string(slot)) &&
      (body == 0x2fae80 || body == 0x2fb67c)) {
    const auto self=kernel::adjust_this(spell, Raw(mem.load(row, 2)));
    if (body == 0x2fb67c) { arity(args,0,0); return {shieldbash_done(self)}; }
    arity(args,0,4); shieldbash_run(self); return {};
  }
  Values lanes{kernel::adjust_this(spell, Raw(mem.load(row, 2)))};
  lanes.insert(lanes.end(),args.begin(),args.end());
  return call("Spell.v"+std::to_string(slot),lanes);
}
Raw Model::spell_rand(Raw spell) {
  const auto at = Address(spell)+0x10;
  return spells::rng_publication([&]{return Raw(mem.load(at,4));},
                                 [&](Raw seed){write(at,4,seed);});
}
void Model::spell_base(Raw spell, Raw level, Raw owner, Raw skill, Raw table) {
  write(spell,1,level); write(Address(spell)+4,4,0); write(Address(spell)+8,4,skill);
  write(Address(spell)+0xc,4,owner);
  spells::constructor_tail([&]{return draw();},
      [&](Raw seed){write(Address(spell)+0x10,4,seed);},
      [&]{write(Address(spell)+0x14,4,table);});
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
        const auto result=vcall(owner,16,{});
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
      const auto result=vcall(u(Address(spell)+0xc),22,{other});
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
  const auto models=vcall(Raw(mem.load(Address(spell)+0xc,4)),16,{});
  if (models.size()!=1) throw Unsupported("model scalar required");
  const auto model=models[0];
  const auto count=named_scalar("model_getSamplePointCount",{model});
  if (count) for (unsigned i=0;i<4;++i) {
    const auto creature=Raw(mem.load(Address(spell)+0xc,4));
    const auto result=vcall(creature,16,{});
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
      const auto same=vcall(Raw(mem.load(Address(spell)+0xc,4)),22,{other});
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
    if (caster && !one_lane(vcall(caster,20,{other}))) continue;
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
    const auto model=one_lane(vcall(owner,16,{}));
    const auto count=named_scalar("model_getSamplePointCount",{model});
    for (unsigned i=0;i<22;++i) {
      owner=u(Address(spell)+0xc);
      const auto current=one_lane(vcall(owner,16,{}));
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
    if (u(PVP) && !one_lane(vcall(owner,22,{other}))) continue;
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
      if (u(PVP) && !one_lane(vcall(other,22,{attacker}))) continue;
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
  const Raw seed=kernel::lcg_step(Raw(mem.load(Address(obj)+0x44,4)));
  write(Address(obj)+0x44,4,seed); return kernel::lcg_result(seed);
}
Vec3 Model::obj_ball(Raw obj) {
  for (unsigned tries=0;tries<65536;++tries) {
    Vec3 b;
    for (auto &x:b) { const auto f=mul(from_int(obj_rand(obj)),TWO_M16); x=sub(add(f,f),ONE); }
    if (!lt(ONE,add(add(mul(b[0],b[0]),mul(b[1],b[1])),mul(b[2],b[2])))) return b;
  }
  throw Unsupported("object ball rejection limit");
}
void Model::delete_self(Raw obj) { if (obj) vcall(obj,1,{3}); }
void Model::gameobject_ctor(Raw obj,const Vec3 &pos,Raw definition,Raw props) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  if (legacy_constructor_vectors) {
    // The immutable 195-unit corpus records the earlier restricted reference.
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
    return;
  }
  const auto text=[&](Address at) {
    std::string out;
    for (unsigned i=0;i<=511;++i) {
      const auto c=Raw(mem.load(at+i,1));
      if (!c) return out;
      if (c>=128) throw Unsupported("constructor non-ASCII string");
      out.push_back(char(c));
    }
    throw Unsupported("constructor string length");
  };
  const auto lower=[](unsigned char c) { return c>='A' && c<='Z' ? static_cast<unsigned char>(c+32) : c; };
  const auto property=[&](Address key_at,unsigned nth) -> Raw {
    if (!u(Address(obj)+0x2c)) return 0;
    auto key=text(key_at);
    for (std::size_t i=0;i<key.size();++i) {
      key[i]=char(lower(static_cast<unsigned char>(key[i])));
      write(key_at+i,1,static_cast<unsigned char>(key[i]));
    }
    const auto list=u(Address(obj)+0x2c);
    for (unsigned i=0;i<4096;++i) {
      const Raw entry=u(Address(list)+4*i);
      if (!entry) return 0;
      const auto value=text(entry);
      if (value.size()>key.size() && value.compare(0,key.size(),key)==0 && value[key.size()]=='=') {
        if (!nth) return entry+Raw(key.size())+1;
        --nth;
      }
    }
    throw Unsupported("constructor property traversal limit");
  };
  const auto name_add=[&](Raw value_at) {
    auto value=text(value_at);
    if (!value.empty() && value[0]=='"') {
      value.erase(0,1);
      const auto quote=value.find('"');
      if (quote!=std::string::npos) value.resize(quote);
      for (std::size_t i=0;i<value.size();++i) write(Address(value_at)+i,1,static_cast<unsigned char>(value[i]));
      write(Address(value_at)+value.size(),1,0);
    }
    write(0x5f86f8,4,u(0x5f86f8)+1);
    Raw hash=0;
    for (unsigned char c:value) {
      hash+=lower(c); hash+=hash<<10;
      hash^=Raw(std::int32_t(hash)>>6);
    }
    hash+=hash<<3; hash^=Raw(std::int32_t(hash)>>11);
    Raw slot=hash&511;
    for (unsigned probe=0;probe<512;++probe,slot=(slot+1)&511) {
      const auto key=u(0x5f76f8+4*slot);
      bool available=!key;
      if (key && !u(0x5f7ef8+4*slot)) {
        const auto old=text(key);
        available=old.size()==value.size() &&
          std::equal(old.begin(),old.end(),value.begin(),[&](char a,char b) {
            return lower(static_cast<unsigned char>(a))==lower(static_cast<unsigned char>(b));
          });
      }
      if (available) {
        write(0x5f76f8+4*slot,4,value_at);
        write(0x5f7ef8+4*slot,4,obj);
        return;
      }
    }
    throw Unsupported("constructor full name table");
  };
  for (const auto &[off,value]:std::array<std::pair<unsigned,Raw>,7>{{{0,0},{4,0},{0x48,0x5122a8},
      {8,0},{0x1c,0x41c00000},{0x20,0x42900000},{0xc,0}}}) write(Address(obj)+off,4,value);
  write(Address(obj)+0x28,1,0x19); write(Address(obj)+0x29,1,0x49);
  write_vec(Address(obj)+0x10,pos);
  write(Address(obj)+0x34,4,0); write(Address(obj)+0x24,4,definition);
  if (definition && u(Address(definition)+4)!=u(0x41bf78)) {
    unsigned count=0;
    for (Address k=8;u(Address(definition)+k);k+=8) {
      if (++count>4096) throw Unsupported("constructor resource traversal limit");
      const auto result=named_scalar("res_find",{u(definition),u(Address(definition)+k)});
      definition=u(Address(obj)+0x24);
      write(Address(definition)+k+4,4,result);
    }
    write(Address(definition)+4,4,u(0x41bf78));
  }
  const auto grid_index=[](Raw v) {
    const auto n=Raw(to_int(v)+0x3c00u);
    return std::int32_t(n)>>8;
  };
  const auto col=grid_index(u(Address(obj)+0x10)), row=grid_index(u(Address(obj)+0x14));
  const auto flags=u(Address(obj)+0xc);
  const Raw cell=!(flags&2) && col>=0 && col<120 && row>=0 && row<120 ?
    Raw((flags&0x800 ? 0x5e95f0 : 0x5db4f0)+4*(row*120+col)) : 0x5f76f0;
  call("handle_link",{cell,obj+8,obj});
  const auto hidden=property(0x5321a8,0);
  if (hidden) {
    const auto value=text(hidden);
    std::size_t i=value.find_first_not_of(" \t\n\v\f\r");
    if (i!=std::string::npos) {
      bool negative=false;
      if (value[i]=='+' || value[i]=='-') negative=value[i++]=='-';
      Raw number=0;
      while (i<value.size() && value[i]>='0' && value[i]<='9') number=number*10+Raw(value[i++]-'0');
      if (negative) number=Raw(-number);
      if (number) {
        const Raw node=u(Address(u(Address(obj)+8))+8);
        const Raw previous=u(Address(node)+4);
        write(previous,4,u(node));
        const Raw following=u(node);
        if (following) write(Address(following)+4,4,u(Address(node)+4));
        const Raw old_head=u(0x5f76f0);
        write(Address(node)+4,4,0x5f76f0);
        write(node,4,old_head);
        write(0x5f76f0,4,node);
        const Raw new_next=u(node);
        if (new_next) write(Address(new_next)+4,4,node);
      }
    }
  }
  write(Address(obj)+0x2c,4,props);
  const auto seed=draw(); write(Address(obj)+0x44,4,seed);
  for (unsigned nth=0;nth<4096;++nth) {
    if (!property(0x5321b0,nth)) return;
    name_add(property(0x5321b0,nth));
  }
  throw Unsupported("constructor name ordinal limit");
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
      if (!one_lane(vcall(owner,20,{other}))) continue;
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
      if (!one_lane(vcall(owner,20,{other}))) continue;
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
// Shock of Frost / Frost Shard, and their shared projectile step.
void Model::projectile_ctor(Raw obj,const Vec3 &pos,Raw owner,Raw definition) {
  gameobject_ctor(obj,pos,definition,0);
  write(Address(obj)+0x4c,4,0); write(Address(obj)+0x1c,4,0x41400000);
  write(Address(obj)+0x48,4,0x5120b8);
  write(Address(obj)+0xc,4,Raw(mem.load(Address(obj)+0xc,4))|0x4081);
  write(Address(obj)+0x50,1,0);
  if (owner) call("handle_link",{obj+0x4c,owner,owner});
}
Raw Model::move_and_collide(Raw obj,const Vec3 &velocity) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  Vec3 step;
  for (unsigned i=0;i<3;++i) step[i]=mul(velocity[i],0x3c888888);
  for (unsigned i=0;i<3;++i) write(Address(obj)+0x10+4*i,4,add(u(Address(obj)+0x10+4*i),step[i]));
  call("obj_updateGridCell",{obj});
  const auto terrain=[&](const Vec3 &p) { return named_scalar("terrain_height",{u(0x44fdc8),p[0],p[1],p[2],u(Address(obj)+0x1c)}); };
  Raw result=0;
  const auto ground=terrain(vec(Address(obj)+0x10));
  if (lt(u(Address(obj)+0x18),ground)) {
    result=1; Raw k=0x3dcccccc; Vec3 attempt;
    for (;;) {
      Vec3 back;
      for (unsigned i=0;i<3;++i) back[i]=mul(step[i],k);
      for (unsigned i=0;i<3;++i) attempt[i]=sub(u(Address(obj)+0x10+4*i),back[i]);
      const auto g=terrain(attempt);
      if (le(g,attempt[2])) break;
      k=add(k,0x3dcccccc); if (!lt(k,TWO)) break;
    }
    write_vec(Address(obj)+0x10,attempt);
    if (!signed_byte(Address(obj)+0x50)) {
      const auto p=vec(Address(obj)+0x10);
      if (named_scalar("water_test",{u(0x44fdc8),p[0],p[1],p[2],0,0x41400000})) {
        write(Address(obj)+0x50,1,1); obj_rand(obj);
        const auto text=named_scalar("libc_sprintf",{0x532458,1});
        const auto sound=named_scalar("res_find",{0x532478,text});
        call("sound_playOnObject",{sound,obj,0x3e4ccccc});
        const auto at=vec(Address(obj)+0x10);
        call("water_disturb",{u(0x44fdc8),at[0],at[1],at[2],0x442f0000,0x41c00000});
      }
    }
  }
  const Raw radius=u(Address(obj)+0x1c),height=u(Address(obj)+0x20);
  const Vec3 low{sub(u(Address(obj)+0x10),radius),sub(u(Address(obj)+0x14),radius),sub(u(Address(obj)+0x18),height)};
  const Vec3 high{add(u(Address(obj)+0x10),radius),add(u(Address(obj)+0x14),radius),add(u(Address(obj)+0x18),height)};
  const auto node=u(Address(obj)+0x4c),owner=node ? u(Address(node)+0xc) : 0;
  const auto found=call("world_collectObjectsInBox",{low[0],low[1],low[2],high[0],high[1],high[2],0x100});
  if (found.size()>65536) throw Unsupported("projectile query limit");
  for (const Raw other:found) {
    const auto flags=u(Address(other)+0xc);
    if ((flags&0x201) || (flags&0x20000000) || other==obj || other==owner) continue;
    if (lt(add(u(Address(other)+0x18),u(Address(other)+0x20)),sub(u(Address(obj)+0x18),u(Address(obj)+0x20)))) continue;
    if (lt(add(u(Address(obj)+0x18),u(Address(obj)+0x20)),u(Address(other)+0x18))) continue;
    const auto dx=sub(u(Address(obj)+0x10),u(Address(other)+0x10));
    const auto dy=sub(u(Address(obj)+0x14),u(Address(other)+0x14));
    const auto reach=add(u(Address(obj)+0x1c),u(Address(other)+0x1c));
    if (!lt(add(mul(dx,dx),mul(dy,dy)),mul(reach,reach))) continue;
    result|=2; vcall(other,6,{obj}); vcall(obj,6,{other});
  }
  return result;
}
void Model::puff(const std::function<Vec3()> &ball,Raw definition,const Vec3 &centre,const std::optional<Raw> &spread,Raw speed) {
  Vec3 point=centre;
  if (spread) { const auto b=ball(); for (unsigned i=0;i<3;++i) point[i]=add(mul(b[i],*spread),centre[i]); }
  const auto b=ball(); Vec3 velocity;
  for (unsigned i=0;i<3;++i) velocity[i]=mul(b[i],speed);
  call("particle_spawn",{definition,point[0],point[1],point[2],velocity[0],velocity[1],velocity[2],ZERO});
}
Raw Model::shockoffrost_can_use(Raw,Raw) const { return 1; }
Raw Model::shockoffrost_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  const auto obj=named_scalar("pool_new",{0xd0});
  if (obj) shockoffrost_ctor(obj,pos,facing,level,caster,skill);
  return 0;
}
void Model::shockoffrost_ctor(Raw obj,const Vec3 &pos,Raw facing,Raw level,Raw owner,Raw skill) {
  projectile_ctor(obj,pos,owner,0); write(Address(obj)+0x48,4,0x51a838);
  write(Address(obj)+0x54,4,0); write(Address(obj)+0x61,1,0); write_vec(Address(obj)+0x88,pos);
  for (auto pair:std::array<std::pair<unsigned,Raw>,5>{{{0x7c,0x42d20000},{0x80,0x38be37c6},{0x84,0x3f4d0e45},{0x74,0x42d20000},{0x78,0x38be37c6}}}) write(Address(obj)+pair.first,4,pair.second);
  for (auto pair:std::array<std::pair<unsigned,Raw>,8>{{{0x6c,0x80},{0x6e,0xff},{0x70,0x96},{0x62,0},{0x64,0x80},{0x66,0xff},{0x68,0x96},{0x6a,0}}}) write(Address(obj)+pair.first,2,pair.second);
  write(Address(obj)+0x60,1,0); call("light_link",{obj+0x58});
  if (owner) call("handle_link",{obj+0x54,owner,owner});
  write(Address(obj)+0x94,2,level); write(Address(obj)+0x38,2,facing);
  write(Address(obj)+0x9c,4,mul(cos16(Raw(s16(facing))),0x43af0000));
  write(Address(obj)+0xa0,4,mul(sin16(Raw(s16(Raw(mem.load(Address(obj)+0x38,2))))),0x43af0000));
  write(Address(obj)+0xa4,4,0); write(Address(obj)+0xc8,4,skill); write(Address(obj)+0xc4,4,0);
  write(Address(obj)+0xc0,4,Raw(mem.load(FRAME,4))+0x78); write(Address(obj)+0xcc,4,0);
  call("obj_runEveryFrame",{obj}); const auto sound=named_scalar("res_find",{0x550ff8,0x551030});
  call("sound_playOnObject",{sound,obj,ONE});
}
void Model::frost_debris(Raw obj,Raw velocity_at) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  for (Raw made=0;;++made) {
    const auto r=obj_rand(obj);
    if (!(s32(made)<to_int(mul(mul(from_int(r),TWO_M16),0x40800000)))) break;
    const auto block=named_scalar("pool_new",{0x248});
    if (block) {
      const auto p=vec(Address(obj)+0x10);
      call("PhProp_ctor",{block,p[0],p[1],p[2],0x550ff8,0x551000,0x551010,Raw(1)<<(made&31),0x112,0,ONE});
      ++hidden_draws;
      write(Address(block)+0xfc,4,add(mul(u(Address(block)+0xfc),0x3e999999),mul(mul(u(velocity_at),0x3e999999),0x3fdba5e3)));
      write(Address(block)+0x30,4,mul(u(Address(block)+0x30),0x3f199999));
      write(Address(block)+0x100,4,add(mul(u(Address(block)+0x100),0x3e999999),mul(mul(u(Address(velocity_at)+4),0x3e999999),0x3fdba5e3)));
      call("PhProp_initPhysics",{block,0x112});
    }
  }
}
void Model::shockoffrost_run(Raw obj) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  write_vec(Address(obj)+0x88,vec(Address(obj)+0x10)); call("light_relink",{obj+0x58});
  const auto matrix=mat34_set_pos_rot_z(vec(Address(obj)+0x10),Raw(s16(Raw(mem.load(Address(obj)+0x38,2))-0x7fff)));
  const Raw turn=Raw(s16(u(FRAME)*4000));
  write(Address(obj)+0xa8,4,0); write(Address(obj)+0xac,4,mul(cos16(turn),0x40c00000));
  write(Address(obj)+0xb0,4,mul(sin16(turn),0x40c00000)); write(Address(obj)+0xb4,4,0);
  write(Address(obj)+0xb8,4,mul(neg(cos16(turn)),0x40c00000)); write(Address(obj)+0xbc,4,mul(neg(sin16(turn)),0x40c00000));
  for (unsigned off:{0xa8,0xb4}) { const auto p=mat34_transform_point(matrix,vec(Address(obj)+off)); for (unsigned i=0;i<3;++i) write(Address(obj)+off+4*i,4,p[i]); }
  const std::function<Vec3()> ball=[&] { return obj_ball(obj); };
  for (unsigned group=0;group<2;++group) for (unsigned j=0;j<(group ? 20u : 30u);++j) {
    const auto b=ball(); const Raw heading=Raw(s16(Raw(mem.load(Address(obj)+0x38,2))));
    const Raw spread=group ? 0x40dfff27 : 0x41109750;
    const Vec3 p{add(add(mul(b[0],spread),mul(cos16(heading),0x41400000)),u(Address(obj)+0x10)),
                 add(add(mul(b[1],spread),mul(sin16(heading),0x41400000)),u(Address(obj)+0x14)),add(mul(b[2],spread),u(Address(obj)+0x18))};
    puff(ball,group ? 0x8e6970 : 0x8e6920,p,std::nullopt,group ? 0x41800003 : 0x41f995ed);
  }
  for (unsigned i=0;i<7;++i) { puff(ball,0x8e69c0,vec(Address(obj)+0xa8),0x403fffef,0x40a00000); puff(ball,0x8e69c0,vec(Address(obj)+0xb4),0x403fffef,0x40a00000); }
  write(Address(obj)+0x98,4,u(Address(obj)+0x98)-2000);
  const auto hit=move_and_collide(obj,vec(Address(obj)+0x9c));
  if ((hit&2) && u(Address(obj)+0xc4)) write(Address(obj)+0xc0,4,0);
  if (hit&1) write(Address(obj)+0xc0,4,0);
  if (s32(u(FRAME))<s32(u(Address(obj)+0xc0))) return;
  const auto sound=named_scalar("res_find",{0x550ff8,0x551048}); call("sound_playOnObject",{sound,obj,ONE});
  auto p=vec(Address(obj)+0x10); call("fx_spawn",{10,p[0],p[1],p[2]}); frost_debris(obj,obj+0x9c);
  const auto count=s32(scalar(u(Address(obj)+0xc8),13,{1,Raw(s16(Raw(mem.load(Address(obj)+0x94,2)))),obj+0x54,0x14}));
  if (count>65536) throw Unsupported("frost shard count limit");
  for (std::int32_t i=0;i<count;++i) {
    const auto facing=Raw(mem.load(Address(obj)+0x38,2)),r=obj_rand(obj);
    Raw heading=Raw(s16(facing+r%0x7fff-0x4000));
    for (unsigned attempt=0;attempt<10;++attempt) {
      const Raw c=Raw(s16(obj_rand(obj)));
      const auto tx=add(u(Address(obj)+0x10),mul(cos16(c),0x41c00000));
      const auto ty=add(u(Address(obj)+0x14),mul(sin16(c),0x41c00000));
      const auto tz=u(Address(obj)+0x18);
      const Vec3 toward{sub(tx,u(Address(obj)+0x10)),sub(ty,u(Address(obj)+0x14)),sub(tz,u(Address(obj)+0x18))};
      p=vec(Address(obj)+0x10);
      const auto t=named_scalar("terrain_raycast",{u(0x44fdc8),p[0],p[1],p[2],toward[0],toward[1],toward[2],0,TWO});
      if (!lt(ZERO,t)) heading=c;
    }
    const auto block=named_scalar("pool_new",{0x7c});
    if (block) frostshard_ctor(block,vec(Address(obj)+0x10),heading,Raw(s16(Raw(mem.load(Address(obj)+0x94,2)))),u(Address(u(Address(obj)+0x54))+0xc),u(Address(obj)+0xc8),u(Address(obj)+0xcc));
  }
  delete_self(obj);
}
bool Model::frost_hit(Raw obj,Raw other,Raw stat,Raw level,Raw hit_off) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  Raw attacker=0; const auto flags=u(Address(other)+0xc);
  if (flags&0x20) {
    if (u(Address(obj)+0x4c)) {
      const auto node=u(Address(obj)+0x4c); attacker=node ? u(Address(node)+0xc) : 0;
      const auto enemy=vcall(attacker,20,{other});
      if (enemy.size()!=1) throw Unsupported("enemy scalar required");
      if (!enemy[0]) return false;
    }
  } else write(Address(obj)+hit_off,4,1);
  if (other && !(flags&0x40) && (flags&0x20) && (mem.load(Address(other)+0xa8,8)&0x200)) return false;
  const auto skill=u(Address(obj)+(hit_off==0xc4 ? 0xc8 : 0x74));
  const auto d=scalar(skill,13,{stat,level,obj+0x4c,0});
  const auto scaled=mul(from_int(d),0x3e4ccccc); const auto r=obj_rand(obj);
  const auto extra=Raw(to_int(mul(scaled,mul(from_int(r),TWO_M16))));
  call("damage_apply",{other,attacker,100000,0,0,0,0x16,d&0xffff,(d+extra)&0xffff,0,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,ZERO,ZERO});
  return true;
}
void Model::shockoffrost_hit(Raw obj,Raw other) {
  write(Address(obj)+0xcc,4,0);
  if (frost_hit(obj,other,0,Raw(s16(Raw(mem.load(Address(obj)+0x94,2)))),0xc4)) { write(Address(obj)+0xcc,4,other); write(Address(obj)+0xc4,4,1); }
}
void Model::frostshard_ctor(Raw obj,const Vec3 &pos,Raw facing,Raw level,Raw owner,Raw skill,Raw exclude) {
  projectile_ctor(obj,pos,owner,0); write(Address(obj)+0x48,4,0x51a8c8); write(Address(obj)+0x54,4,0);
  if (owner) call("handle_link",{obj+0x54,owner,owner});
  write(Address(obj)+0x58,2,level); write(Address(obj)+0x38,2,facing);
  write(Address(obj)+0x60,4,mul(cos16(Raw(s16(facing))),0x43af0000));
  write(Address(obj)+0x64,4,mul(sin16(Raw(s16(Raw(mem.load(Address(obj)+0x38,2))))),0x43af0000));
  write(Address(obj)+0x68,4,0); write(Address(obj)+0x74,4,skill); write(Address(obj)+0x78,4,exclude);
  write(Address(obj)+0x6c,4,Raw(mem.load(FRAME,4))+0x3c); write(Address(obj)+0x70,4,0); call("obj_runEveryFrame",{obj});
}
void Model::frostshard_run(Raw obj) {
  const auto u=[&](Address at) { return Raw(mem.load(at,4)); };
  const std::function<Vec3()> ball=[&] { return obj_ball(obj); };
  for (unsigned group=0;group<2;++group) for (unsigned j=0;j<(group ? 20u : 30u);++j)
    puff(ball,group ? 0x8e6970 : 0x8e6920,vec(Address(obj)+0x10),group ? 0x40dfff27 : 0x41109750,group ? 0x41800003 : 0x41f995ed);
  write(Address(obj)+0x5c,4,u(Address(obj)+0x5c)-2000); const auto hit=move_and_collide(obj,vec(Address(obj)+0x60));
  if ((hit&2) && u(Address(obj)+0x70)) write(Address(obj)+0x6c,4,0);
  if (hit&1) write(Address(obj)+0x6c,4,0);
  if (s32(u(FRAME))<s32(u(Address(obj)+0x6c))) return;
  const auto r=obj_rand(obj); const auto text=named_scalar("libc_sprintf",{0x551020,r%3+1});
  const auto sound=named_scalar("res_find",{0x550ff8,text}); auto p=vec(Address(obj)+0x10);
  call("sound_playAtPos",{sound,p[0],p[1],p[2],ONE}); p=vec(Address(obj)+0x10);
  call("fx_spawn",{10,p[0],p[1],p[2]}); frost_debris(obj,obj+0x60); delete_self(obj);
}
void Model::frostshard_hit(Raw obj,Raw other) {
  if (mem.load(Address(obj)+0x70,4) || other==mem.load(Address(obj)+0x78,4)) return;
  if (frost_hit(obj,other,2,Raw(s16(Raw(mem.load(Address(obj)+0x58,2)))),0x70)) write(Address(obj)+0x70,4,1);
}
// Skill 8: authored from the pinned Fire Flares contract (reference lines 4535-4766).
Raw Model::fireflares_can_use(Raw, Raw caster) { return carries(caster,0x3ce418) ? 0 : 1; }
Raw Model::fireflares_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw) {
  const auto spell=named_scalar("pool_new",{0x78});
  if (spell) fireflares_ctor(spell,level,caster,pos,skill);
  return spell;
}
namespace {
// Widening avoids signed overflow; zero divisors preserve the model's reported
// R5900 quotient, including the unused division when the flare count is zero.
Raw flare_quot(Raw a, Raw b) {
  if (!b) return s32(a)<0 ? 1 : UINT32_MAX;
  return Raw(std::int64_t(s32(a))/s32(b));
}
}
void Model::fireflares_ctor(Raw spell, Raw level, Raw owner, const Vec3 &pos, Raw skill) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  spell_base(spell,level,owner,skill,0x5194f8);
  write(at+0x18,4,0);
  const auto huge=div(ONE,ZERO);
  write(at+0x35,1,0); write_vec(at+0x5c,pos);
  for (const auto &v:std::array<std::pair<unsigned,Raw>,5>{{{0x58,0x3f09fbd6},{0x54,huge},{0x48,0},{0x50,0},{0x4c,huge}}}) write(at+v.first,4,v.second);
  for (const auto &v:std::array<std::pair<unsigned,Raw>,8>{{{0x3e,0xff},{0x40,0x80},{0x44,0xfff0},{0x38,0x80},{0x3c,0xfff0},{0x36,0xff},{0x3a,0},{0x42,0}}}) write(at+v.first,2,v.second);
  write(at+0x34,1,0); call("light_link",{spell+0x2c});
  write(at+0x1c,4,owner);
  const Raw target=level*2+0x41;
  write(at+0x68,4,target); write(at+0x20,4,level); write(at+0x24,4,skill); write(at+0x70,4,0);
  write(at+0x6c,4,flare_quot(0x1194,flare_quot(target,12)));
  const auto seconds=scalar(skill,10,{2,level});
  write(at+0x28,4,u(FRAME)+seconds*60);
  write(at+0x64,4,add(u(at+0x64),0x37b88ca3)); call("light_relink",{spell+0x2c});
  auto n=s32(scalar(u(at+0x24),10,{3,u(at+0x20)}));
  if (n>=17) n=16;
  const Raw step=Raw(s16(flare_quot(0xfffe,Raw(n))));
  Raw angle=0;
  for (std::int32_t i=0;i<n;++i) {
    const auto around=from_int(u(at+0x70));
    const Vec3 point{add(mul(cos16(angle),around),pos[0]),add(mul(sin16(angle),around),pos[1]),add(ZERO,pos[2])};
    const auto block=named_scalar("pool_new",{0x6c});
    if (block) {
      fireflare_ctor(block,point,u(at+0x20),u(at+0x1c),u(at+0x24),Raw(s16(angle)));
      call("handle_link",{spell+0x18,block,block});
    }
    angle+=step;
  }
  const auto resource=named_scalar("res_find",{0x54f7c8,0x54f808});
  const auto point=vec(Address(u(at+0xc))+0x10);
  const auto handle=named_scalar("sound_playAtPos",{resource,point[0],point[1],point[2],ONE});
  write(at+0x74,4,handle); call("sound_release",{handle});
}
void Model::fireflares_dtor(Raw spell, Raw flags) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  write(at+0x14,4,0x5194f8);
  auto node=u(at+0x18); std::size_t seen=0;
  while (node) {
    if (++seen>creature_hp::MAX_HISTORY) throw Unsupported("flare list traversal limit");
    const auto flare=u(Address(node)+0xc); node=u(node);
    if (flare) vcall(flare,1,{3});
  }
  call("sound_stop",{u(at+0x74)}); write(at+0x74,4,UINT32_MAX);
  call("light_unlink",{spell+0x2c}); call("light_dtor",{spell+0x2c,2}); call("handle_clear",{spell+0x18});
  write(at+0x14,4,0x515ba0); // VT_SPELL_BASE from the pinned contract
  call("handle_clear",{spell+4});
  if (flags&1) call("pool_free",{spell});
}
void Model::fireflares_run(Raw spell, const Vec3 &pos, Raw) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto frame=s32(u(FRAME));
  if (frame>=s32(u(at+0x28))) { spell_delete(spell); return; }
  const auto reach=u(at+0x68);
  if (s32(u(at+0x70))<s32(reach)) write(at+0x70,4,u(at+0x70)+2);
  const auto wave=add(sin16(Raw(frame)*3000),sin16(Raw(frame)*0x5dc));
  const auto size=from_int(reach+u(at+0x70)+Raw(to_int(mul(wave,0x41400000))));
  write(at+0x48,4,size); write(at+0x50,4,size);
  const auto inverse=div(ONE,mul(size,size));
  write(at+0x54,4,inverse); write(at+0x4c,4,inverse); write_vec(at+0x5c,pos); call("light_relink",{spell+0x2c});
  auto node=u(at+0x18); std::size_t seen=0;
  while (node) {
    if (++seen>creature_hp::MAX_HISTORY) throw Unsupported("flare list traversal limit");
    const auto flare=u(Address(node)+0xc), following=u(node);
    write(Address(flare)+0x5c,2,Raw(mem.load(Address(flare)+0x5c,2))+Raw(mem.load(at+0x6c,2)));
    const auto x=mul(cos16(Raw(s16(Raw(mem.load(Address(flare)+0x5c,2))))),from_int(u(at+0x70)));
    const auto y=mul(sin16(Raw(s16(Raw(mem.load(Address(u(Address(node)+0xc))+0x5c,2))))),from_int(u(at+0x70)));
    write_vec(Address(u(Address(node)+0xc))+0x10,{add(x,pos[0]),add(y,pos[1]),add(ZERO,pos[2])});
    call("obj_updateGridCell",{u(Address(node)+0xc)}); node=following;
  }
  if (!seen) spell_delete(spell);
}
void Model::fireflare_ctor(Raw obj, const Vec3 &pos, Raw level, Raw owner, Raw skill, Raw angle) {
  const auto at=Address(obj);
  gameobject_ctor(obj,pos,0,0);
  write(at+0x48,4,0x5193e0); write(at+0xc,4,Raw(mem.load(at+0xc,4))|0x421d);
  write(at+0x4c,4,owner); write(at+0x54,4,skill); write(at+0x50,4,level); write_vec(at+0x10,pos);
  write(at+0x1c,4,0x41c00000); write(at+0x5c,2,angle); write(at+0x58,4,0); write(at+0x5e,2,0);
  call("obj_updateGridCell",{obj}); call("obj_runEveryFrame",{obj});
}
void Model::fireflare_run(Raw obj) {
  const auto at=Address(obj);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto burst=[&] { return Raw(s16(Raw(mem.load(at+0x5e,2)))); };
  const auto enemy=[&](Raw other) {
    const auto result=vcall(u(at+0x4c),20,{other});
    if (result.size()!=1) throw Unsupported("flare enemy scalar required");
    return result[0];
  };
  const auto damage=[&](Raw other,Raw dmg,Raw high) {
    call("damage_apply",{other,u(at+0x4c),100000,0,0,0x40,0x15,dmg&0xffff,high&0xffff,0,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,ZERO,ZERO});
  };
  if (u(at+0x58)) {
    if (lt(from_int(burst()),mul(u(at+0x1c),0x40400000)) && scalar(u(at+0x54),10,{1,u(at+0x50)})) {
      write(at+0x5e,2,Raw(mem.load(at+0x5e,2))+7);
      for (unsigned i=0;i<110;++i) {
        const auto turn=obj_rand(obj);
        const auto x=mul(cos16(turn),from_int(burst())), y=mul(sin16(turn),from_int(burst()));
        const auto b=obj_ball(obj);
        const Vec3 point{add(add(x,mul(b[0],0x40d8f5c2)),u(at+0x60)),add(add(y,mul(b[1],0x40d8f5c2)),u(at+0x64)),add(add(ZERO,mul(b[2],0x40d8f5c2)),u(at+0x68))};
        const auto v=obj_ball(obj);
        call("particle_spawn",{0x8e59b0,point[0],point[1],point[2],v[0],v[1],v[2],ZERO});
        call("particle_spawn",{0x8e5a00,point[0],point[1],point[2],v[0],v[1],v[2],ZERO});
      }
      return;
    }
    delete_self(obj); return;
  }
  for (const auto &kind:std::array<std::array<Raw,4>,3>{{{{0x8e59b0,6,0x40e2e0e7,0}},{{0x8e5a00,7,0x40db4b57,0x3f8cab60}},{{0x8e5a50,4,0x40e1ca7f,0}}}}) {
    for (unsigned i=0;i<kind[1];++i) {
      const auto b=obj_ball(obj);
      const Vec3 point{add(mul(b[0],0x410d5fd4),u(at+0x10)),add(mul(b[1],0x410d5fd4),u(at+0x14)),add(add(mul(b[2],0x410d5fd4),u(at+0x18)),0x37b88ca3)};
      auto v=obj_ball(obj); for (auto &c:v) c=mul(c,kind[2]);
      if (kind[3]) v[2]=mul(v[2],kind[3]);
      call("particle_spawn",{kind[0],point[0],point[1],point[2],v[0],v[1],v[2],ZERO});
    }
  }
  if (s32(u(FRAME))%10) return;
  for (const auto other:box_around(vec(at+0x10),from_int(Raw(to_int(u(at+0x1c)))),0x40)) {
    if (!enemy(other)) continue;
    const auto dmg=scalar(u(at+0x54),10,{0,u(at+0x50)});
    const auto amount=from_int(dmg), fraction=mul(from_int(obj_rand(obj)),0x37800000);
    const Raw extra=Raw(to_int(mul(mul(amount,fraction),0x3e4ccccc)));
    damage(other,dmg,dmg+extra);
    write(at+0x58,4,1); write_vec(at+0x60,vec(Address(other)+0x10)); break;
  }
  if (!u(at+0x58)) return;
  const auto resource=named_scalar("res_find",{0x54f7c8,0x54f7f0});
  call("sound_playOnObject",{resource,obj,ONE});
  if (!scalar(u(at+0x54),10,{1,u(at+0x50)})) return;
  const auto wide=from_int(Raw(to_int(u(at+0x1c)))*2);
  for (const auto other:box_around(vec(at+0x10),wide,0x40)) {
    if (!enemy(other)) continue;
    const auto dmg=scalar(u(at+0x54),10,{1,u(at+0x50)});
    const auto amount=from_int(dmg), fraction=mul(from_int(obj_rand(obj)),0x37800000);
    const Raw high=Raw(to_int(add(amount,mul(mul(amount,fraction),0x3e4ccccc))));
    damage(other,dmg,high);
  }
}
// Disease Bolt / Holy Strike: plain (unwrapped) heading differences are intentional.
namespace {
Raw homing_heading(Raw old, std::int32_t desired, Raw raw_rate) {
  const auto diff=std::int64_t(desired)-s16(old);
  const auto rate=std::int64_t(s32(raw_rate));
  const auto magnitude=diff<0 ? -diff : diff;
  const auto change=magnitude<rate ? diff : diff<=0 ? -rate : rate;
  return old+Raw(change);
}
Raw homing_spread(Raw amount, Raw random) {
  return add(amount,mul(mul(amount,mul(from_int(random),TWO_M16)),0x3e4ccccc));
}
}
Raw Model::diseasebolt_can_use(Raw,Raw) const { return 1; }
Raw Model::diseasebolt_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  flash2(caster,Raw(-100),Raw(-100),Raw(-100),0);
  const auto obj=named_scalar("pool_new",{0x7c});
  if (obj) diseasebolt_ctor(obj,facing,level,pos,caster,skill);
  return 0;
}
void Model::diseasebolt_ctor(Raw obj,Raw facing,Raw level,const Vec3 &pos,Raw owner,Raw skill) {
  const Address at=obj;
  projectile_ctor(obj,pos,owner,0);
  write(at+0x54,4,0); write(at+0x48,4,0x5183d8); write(at+0x58,4,0);
  write(at+0x38,2,facing); write(at+0x6c,4,0);
  write(at+0x64,4,mul(cos16(Raw(s16(facing))),0x43af0000));
  write(at+0x68,4,mul(sin16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43af0000));
  write(at+0x60,2,level);
  if (owner) call("handle_link",{obj+0x54,owner,owner});
  const auto frame=Raw(mem.load(FRAME,4));
  write(at+0x5c,4,skill); write(at+0x70,4,frame+300); write(at+0x74,4,frame+10); write(at+0x78,4,0);
  if (owner) {
    const auto p=vec(at+0x10);
    const auto found=call("world_collectObjectsInArc",{p[0],p[1],p[2],Raw(s16(Raw(mem.load(at+0x38,2)))),STACK_QUERY,0,0x438bfffc,0x42700000,ZERO});
    if (found.size()>65536) throw Unsupported("Disease Bolt arc query limit");
    for (const Raw other:found) {
      const auto enemy=vcall(owner,20,{other});
      if (enemy.size()!=1) throw Unsupported("Disease Bolt enemy scalar required");
      if (enemy[0]) { call("handle_link",{obj+0x58,other,other}); break; }
    }
    call("handle_clear",{STACK_QUERY});
  }
  call("obj_runEveryFrame",{obj});
  const auto sound=named_scalar("res_find",{0x54e988,0x54e990}); call("sound_playOnObject",{sound,obj,ONE});
}
void Model::diseasebolt_run(Raw obj) {
  const Address at=obj;
  const auto u=[&](Address p) { return Raw(mem.load(p,4)); };
  const std::function<Vec3()> ball=[&] { return obj_ball(obj); };
  const std::array<Raw,3> defs{0x8e50a0,0x8e50f0,0x8e5140};
  for (unsigned i=0;i<3;++i) for (unsigned n=0;n<(i==2 ? 6u : 5u);++n)
    puff(ball,defs[i],vec(at+0x10),i ? 0x41700000 : 0x41200000,i ? 0x40a00000 : 0x41800000);
  if (u(at+0x58) && s32(u(at+0x74))<s32(u(FRAME))) {
    const auto skill=u(at+0x5c);
    const auto rate=scalar(skill,13,{1,Raw(s16(Raw(mem.load(at+0x60,2)))),obj+0x4c,10})*182u;
    const auto target=u(Address(u(at+0x58))+0xc);
    const auto desired=atan2_16(sub(u(Address(target)+0x14),u(at+0x14)),sub(u(Address(target)+0x10),u(at+0x10)));
    write(at+0x38,2,homing_heading(Raw(mem.load(at+0x38,2)),desired,rate));
    write(at+0x64,4,mul(cos16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43af0000));
    write(at+0x6c,4,0);
    write(at+0x68,4,mul(sin16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43af0000));
  }
  const auto hit=move_and_collide(obj,vec(at+0x64));
  if ((hit&2) && u(at+0x78)) write(at+0x70,4,0);
  if (hit&1) write(at+0x70,4,0);
  if (s32(u(at+0x70))<=s32(u(FRAME))) {
    const auto sound=named_scalar("res_find",{0x54e988,0x54e9c8}); call("sound_playOnObject",{sound,obj,ONE});
    const auto p=vec(at+0x10); call("fx_spawn",{12,p[0],p[1],p[2]});
    for (unsigned n=0;n<10;++n) {
      puff(ball,defs[0],vec(at+0x10),0x41200000,0x42c00000);
      puff(ball,defs[1],vec(at+0x10),0x41700000,0x41f00000);
      puff(ball,defs[2],vec(at+0x10),0x41700000,0x41f00000);
    }
    delete_self(obj);
  }
}
void Model::diseasebolt_hit(Raw obj,Raw other) {
  const Address at=obj;
  const auto u=[&](Address p) { return Raw(mem.load(p,4)); };
  const auto owner=[&] { const auto node=u(at+0x54); return node ? u(Address(node)+0xc) : 0; };
  if (u(at+0x78)) return;
  if (!u(at+0x54)) {
    if (!(u(Address(other)+0xc)&0x20)) write(at+0x78,4,1);
  } else {
    const auto enemy=vcall(owner(),20,{other});
    if (enemy.size()!=1) throw Unsupported("Disease Bolt enemy scalar required");
    if (!enemy[0]) return;
  }
  const auto flags=u(Address(other)+0xc);
  if (!(flags&0x40) && (flags&0x20) && (mem.load(Address(other)+0xa8,8)&0x200)) return;
  const auto damage=scalar(u(at+0x5c),13,{0,Raw(s16(Raw(mem.load(at+0x60,2)))),obj+0x4c,0});
  const auto amount=from_int(damage),high=homing_spread(amount,obj_rand(obj));
  call("damage_apply",{other,owner(),100000,0,0,0,0x26,damage&0xffff,damage&0xffff,0,
       UINT32_MAX,UINT32_MAX,UINT32_MAX,0,amount,high});
  write(at+0x78,4,1);
}
Raw Model::holystrike_can_use(Raw,Raw) const { return 1; }
Raw Model::holystrike_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  flash_both(caster,0xff,200,0x87,0x40);
  const auto obj=named_scalar("pool_new",{0x80});
  if (obj) holystrike_ctor(obj,facing,level,pos,caster,skill);
  return 0;
}
void Model::holystrike_ctor(Raw obj,Raw facing,Raw level,const Vec3 &pos,Raw owner,Raw skill) {
  const Address at=obj;
  projectile_ctor(obj,pos,owner,0); write(at+0x54,4,0); write(at+0x38,2,facing);
  write(at+0x48,4,0x51a078); write(at+0x58,4,0);
  if (owner) call("handle_link",{obj+0x54,owner,owner});
  write(at+0x5c,2,level);
  const auto frame=Raw(mem.load(FRAME,4));
  write(at+0x68,4,skill); write(at+0x64,4,frame+15); write(at+0x60,4,frame+180);
  write(at+0x6c,4,mul(cos16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43e10000));
  write(at+0x74,4,0); write(at+0x78,4,0);
  write(at+0x70,4,mul(sin16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43e10000));
  const auto trail=named_scalar("trail_create",{1,0xfa,0,0xfa,200,100,0x80,0x3e4ccccc});
  write(at+0x7c,4,trail);
  auto p=vec(at+0x10); call("trail_addPoint",{trail,p[0],p[1],p[2]});
  const auto who=owner && (mem.load(Address(owner)+0xc,4)&0x20) ? owner : 0;
  p=vec(at+0x10);
  call("world_collectObjectsInArc",{p[0],p[1],p[2],Raw(s16(Raw(mem.load(at+0x38,2)))),obj+0x58,who,0x43b40000,0x42960000,ZERO});
  call("obj_runEveryFrame",{obj});
  const auto sound=named_scalar("res_find",{0x5506c0,0x5506c8}); call("sound_playOnObject",{sound,obj,ONE});
}
void Model::holystrike_run(Raw obj) {
  const Address at=obj;
  const auto u=[&](Address p) { return Raw(mem.load(p,4)); };
  const std::function<Vec3()> ball=[&] { return obj_ball(obj); };
  const std::array<Raw,3> defs{0x8e61d0,0x8e6220,0x8e6270};
  const std::array<Raw,3> spread{0x40efffdc,0x411fff59,0x411fffca},speed{0x41800000,0x4147ff03,0x419fffcf};
  const std::array<unsigned,3> count{5,10,9};
  for (unsigned i=0;i<3;++i) for (unsigned n=0;n<count[i];++n) puff(ball,defs[i],vec(at+0x10),spread[i],speed[i]);
  const auto p=vec(at+0x10); call("trail_addPoint",{u(at+0x7c),p[0],p[1],p[2]});
  if (u(at+0x58) && s32(u(at+0x64))<s32(u(FRAME))) {
    const auto rate=scalar(u(at+0x68),10,{2,Raw(s16(Raw(mem.load(at+0x5c,2))))})*182u;
    const auto target=u(Address(u(at+0x58))+0xc);
    const auto desired=atan2_16(sub(u(Address(target)+0x14),u(at+0x14)),sub(u(Address(target)+0x10),u(at+0x10)));
    write(at+0x38,2,homing_heading(Raw(mem.load(at+0x38,2)),desired,rate));
    write(at+0x6c,4,mul(cos16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43e10000));
    write(at+0x74,4,0);
    write(at+0x70,4,mul(sin16(Raw(s16(Raw(mem.load(at+0x38,2))))),0x43e10000));
  }
  const auto hit=move_and_collide(obj,vec(at+0x6c));
  if ((hit&2) && u(at+0x78)) write(at+0x60,4,0);
  if (hit&1) write(at+0x60,4,0);
  if (s32(u(FRAME))<s32(u(at+0x60))) return;
  const auto sound=named_scalar("res_find",{0x5506c0,0x5506e0}); call("sound_playOnObject",{sound,obj,ONE});
  light_effect(0xbc,vec(at+0x10),{0,0,0x437f0000,0x43480000,0x43070000,0xc2000000,ZERO,ZERO,ZERO,ZERO,0x43700000,0x42f00000,HALF,ONE});
  for (unsigned n=0;n<25;++n) {
    puff(ball,defs[0],vec(at+0x10),spread[0],0x42c00000);
    puff(ball,defs[1],vec(at+0x10),spread[1],0x4295ff42);
    puff(ball,defs[2],vec(at+0x10),spread[2],0x42efffb6);
  }
  delete_self(obj);
}
void Model::holystrike_hit(Raw obj,Raw other) {
  const Address at=obj;
  const auto u=[&](Address p) { return Raw(mem.load(p,4)); };
  const auto owner=[&] { const auto node=u(at+0x54); return node ? u(Address(node)+0xc) : 0; };
  if (u(at+0x78)) return;
  if (u(at+0x54)) {
    const auto enemy=vcall(owner(),20,{other});
    if (enemy.size()!=1) throw Unsupported("Holy Strike enemy scalar required");
    if (!enemy[0]) return;
  }
  const auto flags=u(Address(other)+0xc);
  if (!(flags&0x40) && (flags&0x20) && (mem.load(Address(other)+0xa8,8)&0x200)) return;
  const auto stat=signed_byte(Address(other)+0xb4)==0x10 ? 1u : 0u;
  const auto amount=from_int(scalar(u(at+0x68),10,{stat,Raw(s16(Raw(mem.load(at+0x5c,2))))}));
  const auto how=u(PVP) ? 0u : 0x80000u;
  const auto probe=add(homing_spread(amount,obj_rand(obj)),HALF);
  const auto rolled=homing_spread(amount,obj_rand(obj));
  const auto high=le(ZERO,probe) ? to_int(add(rolled,ONE)) : to_int(sub(add(rolled,HALF),HALF));
  call("damage_apply",{other,owner(),100000,0,0,how,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,amount,from_int(Raw(high))});
  write(at+0x78,4,1); flash_both(other,0xff,200,0x87,0x40);
}

Raw Model::blindinglight_can_use(Raw, Raw) const { return 1; }
Raw Model::blindinglight_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing) {
  flash_both(caster,0x56,0x80,0xfa,0x6e);
  call("gfx_setColorOffset",{0x80,0x80,0x80,0});
  const auto found=call("world_collectObjectsInArc",{pos[0],pos[1],pos[2],Raw(s16(facing)),STACK_QUERY,0,0x43b40000,0x42b40000,ZERO});
  // Only the first returned entry is inspected; the contract does not traverse
  // this list or impose a result bound on it.
  const auto sound=named_scalar("res_find",{0x550d70,0x550d78});
  call("sound_playOnObject",{sound,caster,ONE});
  if (!found.empty() && caster) {
    const auto first=found.front();
    const auto enemy=vcall(caster,20,{first});
    if (enemy.size()!=1) throw Unsupported("Creature enemy scalar required");
    if (enemy[0]) {
      const auto where=vec(Address(first)+0x10);
      const auto obj=named_scalar("pool_new",{0x54});
      if (obj) blindinglight_effect_ctor(obj,where,level,caster,skill);
    }
  }
  call("handle_clear",{STACK_QUERY});
  return 0;
}
void Model::blindinglight_effect_ctor(Raw obj, const Vec3 &pos, Raw level, Raw caster, Raw skill) {
  const Address at=obj;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  gameobject_ctor(obj,pos,0,0);
  write(at+0x48,4,0x51a5e0); write(at+0xc,4,u(at+0xc)|0x421b);
  write(at+0x4c,4,u(FRAME)+60);
  const Raw radius=scalar(skill,10,{3,level})*12u;
  write(at+0x50,4,radius);
  for (const auto other:box_around(vec(at+0x10),from_int(radius),0x40)) {
    const auto enemy=vcall(caster,20,{other});
    if (enemy.size()!=1) throw Unsupported("Creature enemy scalar required");
    if (!enemy[0]) continue;
    auto chance=s32(scalar(skill,10,{0,level}));
    if (named_scalar("creature_bossRank",{other})) chance=chance/2+1;
    if (lt(mul(mul(from_int(obj_rand(obj)),TWO_M16),0x42c80000),from_int(Raw(chance)))) {
      const auto spell=named_scalar("pool_new",{0x20});
      if (spell) blindinglight_spell_ctor(spell,level,caster,other,skill);
      add_spell(other,spell);
      const auto duration=scalar(skill,10,{2,level});
      vcall(other,31,{duration*60u});
    }
  }
  const auto r=s32(u(at+0x50));
  light_effect(0xbc,vec(at+0x10),{0,0,0x42ac0000,0x43000000,0x437a0000,0xc2000000,ZERO,ZERO,ZERO,ZERO,
    from_int(Raw(r)+24u),from_int(Raw(r/2)+24u),0x3f199999,ONE});
  const auto shaft=named_scalar("pool_new",{0x5c});
  if (shaft) {
    const auto point=vec(at+0x10);
    call("GlowingShaft_ctor",{shaft,point[0],point[1],point[2],60,0x56,0x80,0xfa});
    ++hidden_draws;
  }
  call("obj_runEveryFrame",{obj});
}
void Model::blindinglight_effect_run(Raw obj) {
  const Address at=obj;
  if (s32(Raw(mem.load(FRAME,4)))>=s32(Raw(mem.load(at+0x4c,4)))) { delete_self(obj); return; }
  for (unsigned i=0;i<12;++i) {
    const auto angle=s16(obj_rand(obj));
    const auto dx=obj_rand(obj);
    const auto x=mul(cos16(angle),mul(mul(from_int(dx),TWO_M16),from_int(Raw(mem.load(at+0x50,4)))));
    const auto dy=obj_rand(obj);
    const auto y=mul(sin16(angle),mul(mul(from_int(dy),TWO_M16),from_int(Raw(mem.load(at+0x50,4)))));
    const auto z=mul(mul(from_int(obj_rand(obj)),TWO_M16),0x413fff59);
    const auto point=vec(at+0x10);
    call("particle_spawn",{0x8e6590,add(x,point[0]),add(y,point[1]),add(z,point[2]),ZERO,ZERO,0x42d9bdc5,ZERO});
  }
}
void Model::blindinglight_spell_ctor(Raw spell, Raw level, Raw caster, Raw victim, Raw skill) {
  spell_base(spell,level,victim,skill,0x51a6f8);
  const auto seconds=scalar(skill,10,{2,level});
  write(Address(spell)+0x1c,4,caster);
  write(Address(spell)+0x18,4,Raw(mem.load(FRAME,4))+seconds*60u);
}
void Model::blindinglight_spell_run(Raw spell) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  if (s32(u(FRAME))>=s32(u(at+0x18))) { spell_delete(spell); return; }
  if (!lt(ZERO,u(Address(u(at+0xc))+0x228)) || s32(u(FRAME))%30!=0) return;
  const auto stat=s32(scalar(u(at+8),10,{1,signed_byte(at)}));
  const auto full=from_int(Raw(stat));
  const auto high=mul(homing_spread(full,spell_rand(spell)),HALF);
  call("damage_apply",{u(at+0xc),u(at+0x1c),100000,0,0,0x40,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,
    from_int(Raw(stat/2)),high});
}

// Disease Shield and Disease Effect: the two swarm contracts retain distinct
// offsets, radius constants, callback order and damage descriptor semantics.
Raw Model::diseaseshield_can_use(Raw, Raw caster) { return carries(caster,0x3cd300) ? 0 : 1; }
Raw Model::diseaseshield_cast(Raw skill, Raw level, Raw caster) {
  flash2(caster,Raw(-100),Raw(-100),Raw(-100),0);
  const auto spell=named_scalar("pool_new",{0x1b0});
  if (spell) diseaseshield_ctor(spell,level,caster,skill);
  return spell;
}
void Model::diseaseshield_ctor(Raw spell,Raw level,Raw owner,Raw skill) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  spell_base(spell,level,owner,skill,0x5186c0);
  const Raw byte=level&0xffu, signed_level=byte&0x80u ? byte|0xffffff00u : byte;
  const auto seconds=scalar(skill,12,{2,signed_level,owner,20});
  write(at+0x1a4,4,u(FRAME)+seconds*60u);
  const auto reach=scalar(u(at+8),12,{1,signed_byte(at),u(at+0xc),10});
  write(at+0x1a8,4,reach*12u);
  for (unsigned i=0;i<16;++i) {
    auto b=spell_ball(spell); const auto radius=u(Address(u(at+0xc))+0x1c);
    for (unsigned k=0;k<3;++k) write(at+0x18+12*i+4*k,4,mul(b[k],radius));
    b=spell_ball(spell);
    for (unsigned k=0;k<3;++k) write(at+0xd8+12*i+4*k,4,b[k]);
  }
  write_vec(at+0x198,vec(Address(u(at+0xc))+0x10));
  const auto resource=named_scalar("res_find",{0x54eb88,0x54eb90});
  const auto p=vec(Address(u(at+0xc))+0x10);
  call("sound_playAtPos",{resource,p[0],p[1],p[2],ONE});
}
void Model::disease_spell_ctor(Raw spell,Raw seconds,Raw level,Raw victim,Raw attacker,Raw skill) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  spell_base(spell,level,victim,0,0x518db0);
  write(at+0x18,4,0);
  if (attacker) call("handle_link",{Raw(at+0x18),attacker,attacker});
  write(at+0x1a4,4,seconds); write(at+0x19c,4,victim);
  write(at+0x1a0,4,u(FRAME)+seconds*60u);
  for (unsigned i=0;i<16;++i) {
    auto b=spell_ball(spell); const auto radius=u(Address(u(at+0xc))+0x1c);
    for (unsigned k=0;k<3;++k) write(at+0x1c+12*i+4*k,4,mul(b[k],radius));
    b=spell_ball(spell);
    for (unsigned k=0;k<3;++k) write(at+0xdc+12*i+4*k,4,b[k]);
  }
  write(at+0x1ac,4,skill); write(at+0x1a8,4,level);
}
void Model::diseaseshield_run(Raw spell,const Vec3 &pos,Raw) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  if (s32(u(FRAME))>=s32(u(at+0x1a4))) { spell_delete(spell); return; }
  auto owner=u(at+0xc);
  const Vec3 drift{mul(sub(u(Address(owner)+0x10),u(at+0x198)),0x40c00000),
    mul(sub(u(Address(owner)+0x14),u(at+0x19c)),0x40c00000),ZERO};
  if (lt(u(at+0x1ac),from_int(u(at+0x1a8)))) write(at+0x1ac,4,add(u(at+0x1ac),HALF));
  const auto count=[&](Raw density) { const auto r=u(at+0x1ac); return to_int(mul(mul(r,density),mul(r,0x40490fcf))); };
  for (unsigned n=0;std::int64_t(n)<count(0x3a5a740d);++n) {
    if (n>=65536) throw Unsupported("Disease Shield particle limit");
    const auto b=spell_ball(spell); const auto scale=mul(u(at+0x1ac),0x3f400000);
    const Vec3 p{mul(b[0],scale),mul(b[1],scale),mul(mul(b[2],0x3ecccccc),scale)};
    const auto up=lt(p[2],ZERO) ? neg(p[2]) : p[2];
    const Vec3 velocity{add(mul(p[0],0x3ecccccc),drift[0]),add(mul(p[1],0x3ecccccc),drift[1]),add(mul(up,0x3ecccccc),drift[2])};
    const auto lowered=sub(add(p[2],pos[2]),mul(u(Address(u(at+0xc))+0x20),0x3e800000));
    call("particle_spawn",{0x8e5280,add(p[0],pos[0]),add(p[1],pos[1]),lowered,velocity[0],velocity[1],velocity[2],ZERO});
  }
  for (unsigned n=0;std::int64_t(n)<count(0x39da740d);++n) {
    if (n>=65536) throw Unsupported("Disease Shield particle limit");
    const auto b=spell_ball(spell); const auto scale=mul(u(at+0x1ac),0x3ea8f5c2);
    Vec3 point{}; for (unsigned k=0;k<3;++k) point[k]=add(mul(b[k],scale),pos[k]);
    const auto v=spell_ball(spell); auto up=mul(v[2],0x3e999999); if (lt(up,ZERO)) up=neg(up);
    call("particle_spawn",{0x8e52d0,point[0],point[1],point[2],add(mul(v[0],0x41600000),drift[0]),
      add(mul(v[1],0x41600000),drift[1]),add(mul(up,0x41600000),drift[2]),ZERO});
  }
  for (unsigned i=0;i<16;++i) {
    const auto point=at+0x18+12*i,speed=at+0xd8+12*i;
    auto push=spell_ball(spell); for (auto &v:push) v=mul(v,0x3fc00000);
    auto p=vec(point); const auto r=u(at+0x1ac);
    if (lt(mul(mul(r,r),0x3f55554f),add(add(mul(p[0],p[0]),mul(p[1],p[1])),mul(p[2],p[2]))))
      for (unsigned k=0;k<3;++k) push[k]=add(push[k],mul(p[k],0xbca3d70a));
    Vec3 v{}; for (unsigned k=0;k<3;++k) v[k]=mul(add(u(speed+4*k),push[k]),0x3f4ccccc);
    for (unsigned k=0;k<3;++k) write(speed+4*k,4,v[k]);
    for (unsigned k=0;k<3;++k) p[k]=add(u(point+4*k),v[k]);
    for (unsigned k=0;k<3;++k) write(point+4*k,4,p[k]);
    owner=u(at+0xc); Vec3 glow{};
    for (unsigned k=0;k<3;++k) glow[k]=add(p[k],u(Address(owner)+0x10+4*k));
    glow[2]=add(glow[2],mul(u(Address(owner)+0x20),0x3f400000));
    call("glow_add",{glow[0],glow[1],glow[2],0,0,0,200,u(0x4f8498),0,0,0x41f00000,0x43c80000});
  }
  write_vec(at+0x198,vec(Address(u(at+0xc))+0x10));
  if (s32(u(FRAME))%30!=0) return;
  const auto found=box_around(pos,from_int(Raw(to_int(u(at+0x1ac)))),0x40);
  bool hit=false;
  for (const auto other:found) {
    if (!one_lane(vcall(u(at+0xc),20,{other}))) continue;
    const auto dmg=scalar(u(at+8),12,{0,signed_byte(at),u(at+0xc),0});
    const auto amount=from_int(dmg),high=mul(homing_spread(amount,spell_rand(spell)),HALF);
    hit=true;
    call("damage_apply",{other,u(at+0xc),100000,0,0,0x40,0x26,dmg&0xffff,dmg&0xffff,0,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,amount,high});
  }
  if (hit) {
    const auto resource=named_scalar("res_find",{0x54eb88,0x54eba8}); const auto p=vec(Address(u(at+0xc))+0x10);
    call("sound_playAtPos",{resource,p[0],p[1],p[2],ONE});
  }
}
void Model::disease_spell_run(Raw spell,const Vec3 &pos,Raw) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  if (s32(u(FRAME))>=s32(u(at+0x1a0))) { spell_delete(spell); return; }
  for (unsigned i=0;i<16;++i) {
    const auto point=at+0x1c+12*i,speed=at+0xdc+12*i;
    auto push=spell_ball(spell); for (auto &v:push) v=mul(v,0x3fc00000);
    auto p=vec(point); const auto radius=u(Address(u(at+0x19c))+0x1c);
    const auto limit=mul(mul(add(radius,radius),add(radius,radius)),0x3f55551d);
    if (lt(limit,add(add(mul(p[0],p[0]),mul(p[1],p[1])),mul(p[2],p[2]))))
      for (unsigned k=0;k<3;++k) push[k]=add(push[k],mul(p[k],0xbca3d70a));
    Vec3 v{}; for (unsigned k=0;k<3;++k) v[k]=mul(add(u(speed+4*k),push[k]),0x3f4ccccc);
    for (unsigned k=0;k<3;++k) write(speed+4*k,4,v[k]);
    for (unsigned k=0;k<3;++k) p[k]=add(u(point+4*k),v[k]);
    for (unsigned k=0;k<3;++k) write(point+4*k,4,p[k]);
    call("glow_add",{add(p[0],pos[0]),add(p[1],pos[1]),add(p[2],pos[2]),0,0,0,200,u(0x4f8498),0,0,0x41f00000,0x43c80000});
  }
  const auto victim=u(at+0x19c); flash2(victim,255,255,0,0);
  if (s32(u(at+0x1a4))>1 && s32(u(FRAME))%30==0) {
    const auto radius=u(Address(victim)+0x1c);
    const auto found=box_around(pos,from_int(Raw(to_int(add(radius,radius)))),0x40);
    for (const auto other:found) {
      const auto node=u(at+0x18);
      if (!node || !(u(Address(other)+0xc)&0x20)) continue;
      const auto source=u(Address(node)+0xc);
      if ((u(Address(source)+0xc)&0x20) && !one_lane(vcall(source,20,{other}))) continue;
      if (carries(other,0x3cdd40)) continue;
      const auto fresh=named_scalar("pool_new",{0x1b0});
      if (fresh) {
        const auto seconds=Raw(to_int(sub(mul(from_int(u(at+0x1a4)),HALF),mul(u(Address(other)+0x68),0x40a00000))));
        disease_spell_ctor(fresh,seconds,0,other,u(Address(u(at+0x18))+0xc),u(at+0x1ac));
      }
      add_spell(other,fresh);
    }
  }
  if (s32(u(FRAME))%30==0 && u(at+0x18)) {
    const auto source=u(Address(u(at+0x18))+0xc);
    const auto value=scalar(u(at+0x1ac),12,{0,u(at+0x1a8),source,0});
    const auto d=to_int(mul(from_int(value),HALF));
    const auto amount=from_int(d>=1 ? Raw(d) : 1),high=homing_spread(amount,spell_rand(spell));
    const auto node=u(at+0x18), attacker=node ? u(Address(node)+0xc) : 0;
    call("damage_apply",{u(at+0x19c),attacker,100000,0,0,0xc040,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,amount,high});
  }
}

// Fire Weapons (13) and Cone of Fire (18), pinned contract lifecycles.
Raw Model::fireweapons_can_use(Raw, Raw caster) { return carries(caster,0x3cec68) ? 0 : 1; }
Raw Model::fireweapons_cast(Raw skill, Raw level, Raw caster) {
  const auto spell=named_scalar("pool_new",{0x60});
  if (spell) fireweapons_ctor(spell,level,caster,skill);
  return spell;
}
void Model::fireweapons_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  spell_base(spell,level,owner,skill,0x519a88);
  const auto huge=div(ONE,ZERO);
  write(at+0x29,1,0); write_vec(at+0x50,vec(Address(owner)+0x10));
  for (const auto &p:std::array<std::pair<unsigned,Raw>,5>{{{0x48,huge},{0x4c,0x3f266655},{0x3c,0},{0x44,0},{0x40,huge}}}) write(at+p.first,4,p.second);
  for (const auto &p:std::array<std::pair<unsigned,Raw>,8>{{{0x32,0xff},{0x34,0x80},{0x2a,0xff},{0x2c,0x80},{0x2e,0},{0x30,0},{0x36,0},{0x38,0}}}) write(at+p.first,2,p.second);
  write(at+0x28,1,0); call("light_link",{spell+0x20});
  const auto seconds=scalar(skill,12,{1,level,owner,20});
  write(at+0x18,4,u(FRAME)+seconds*60u);
  const auto radius=scalar(u(at+8),12,{2,signed_byte(at),owner,10});
  write(at+0x5c,4,u(FRAME)); write(at+0x1c,4,radius*12u);
  const auto dmg=scalar(u(at+8),12,{0,signed_byte(at),owner,0});
  const auto frames=scalar(u(at+8),12,{1,signed_byte(at),owner,20})*60u;
  owner=u(at+0xc);
  if (!(u(Address(owner)+0xc)&0x40)) {
    const auto holder=u(Address(owner)+0x50);
    if (holder) table_call("Holder",holder,0,2,{1});
  } else for (auto item:weapons_of(owner)) enchant(item,0x15,dmg,frames,frames);
  const auto sound=named_scalar("res_find",{0x54ff80,0x54ff88});
  const auto point=vec(Address(u(at+0xc))+0x10);
  call("sound_playAtPos",{sound,point[0],point[1],point[2],ONE});
}
void Model::fireweapons_run(Raw spell, const Vec3 &pos, Raw facing) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto frame=s32(u(FRAME)); facing=Raw(s16(facing));
  auto owner=u(at+0xc);
  if (frame>=s32(u(at+0x18))) {
    if ((u(Address(owner)+0xc)&0x60)==0x20 && u(Address(owner)+0x50)) table_call("Holder",u(Address(owner)+0x50),0,8,{1,0});
    spell_delete(spell); return;
  }
  const Raw left=u(at+0x18)-Raw(frame);
  bool flicker=true;
  if (s32(Raw(frame)-u(at+0x5c))<30) {
    const std::array<std::array<Raw,4>,3> puffs{{{0x8e5d20,4,0x414a14c9,0x40ee541d},{0x8e5d70,5,0x414a0ac0,0x40df7c5e},{0x8e5dc0,1,0x418e63db,0x40d701a7}}};
    for (const auto &p:puffs) for (Raw n=0;n<p[1];++n) {
      const auto b=spell_ball(spell); Vec3 point;
      for (unsigned i=0;i<3;++i) point[i]=add(mul(b[i],p[2]),pos[i]);
      const Vec3 push{mul(cos16(facing),p[3]),mul(sin16(facing),p[3]),ZERO};
      const auto v=spell_ball(spell);
      call("particle_spawn",{p[0],point[0],point[1],point[2],add(push[0],v[0]),add(push[1],v[1]),add(push[2],v[2]),ZERO});
    }
  } else if (s32(left)<=30) flicker=false;
  Raw reach;
  if (flicker) {
    const auto wave=add(sin16(Raw(frame)*4000u),sin16(Raw(frame)*2000u));
    reach=from_int(Raw(to_int(mul(wave,0x40800000)))+110u);
  } else reach=fmin(0x43960000,mul(mul(from_int(left),0x3d088888),0x42dc0000));
  write(at+0x3c,4,reach); write(at+0x44,4,reach);
  const auto inverse=div(ONE,mul(reach,reach));
  write(at+0x48,4,inverse); write(at+0x40,4,inverse);
  write_vec(at+0x50,{add(mul(cos16(facing),0x41900000),pos[0]),add(mul(sin16(facing),0x41900000),pos[1]),add(ZERO,pos[2])});
  call("light_relink",{spell+0x20});
  if (u(at+0x1c) && (u(Address(u(at+0xc))+0xc)&0x40) && s32(u(FRAME))%30==0) {
    const auto found=box_around(pos,from_int(u(at+0x1c)),64); owner=u(at+0xc);
    for (const auto other:found) {
      if (other==owner || !(u(Address(other)+0xc)&0x40)) continue;
      if (u(PVP) && !one_lane(vcall(u(at+0xc),22,{other}))) continue;
      const auto weapons=weapons_of(other);
      const auto aura=scalar(u(at+8),12,{3,signed_byte(at),u(at+0xc),10});
      enchant(weapons[0],0x15,aura,31,31); enchant(weapons[1],0x15,aura,31,31);
    }
  }
  owner=u(at+0xc);
  if ((u(Address(owner)+0xc)&0x68)==0x28) {
    if (u(Address(owner)+0x50)) table_call("Holder",u(Address(owner)+0x50),0,8,{1,0});
    spell_delete(spell);
  }
}
Raw Model::coneoffire_can_use(Raw, Raw) const { return 1; }
Raw Model::coneoffire_cast(Raw skill, Raw level, Raw caster) {
  const auto spell=named_scalar("pool_new",{0x58});
  if (spell) coneoffire_ctor(spell,level,caster,skill);
  return spell;
}
void Model::coneoffire_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  const Address at=spell;
  spell_base(spell,level,owner,skill,0x51a470);
  const auto huge=div(ONE,ZERO);
  write(at+0x21,1,0); write_vec(at+0x48,vec(Address(owner)+0x10));
  for (const auto &p:std::array<std::pair<unsigned,Raw>,8>{{{0x2c,0x7d},{0x24,0x7d},{0x2a,0xff},{0x22,0xff},{0x30,0x42},{0x28,0x42},{0x26,0},{0x2e,0}}}) write(at+p.first,2,p.second);
  for (const auto &p:std::array<std::pair<unsigned,Raw>,5>{{{0x40,huge},{0x44,0x3f19db22},{0x34,0},{0x3c,0},{0x38,huge}}}) write(at+p.first,4,p.second);
  write(at+0x20,1,0); call("light_link",{spell+0x18});
  const auto resource=named_scalar("res_find",{0x550b00,0x550b08});
  const auto point=vec(Address(Raw(mem.load(at+0xc,4)))+0x10);
  const auto sound=named_scalar("sound_playAtPos",{resource,point[0],point[1],point[2],ONE});
  write(at+0x54,4,sound); call("sound_release",{sound});
}
void Model::coneoffire_run(Raw spell, const Vec3 &pos, Raw facing) {
  const Address at=spell;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  facing=Raw(s16(facing)); const auto frame=u(FRAME);
  const auto wave=add(sin16(frame*0xdacu),sin16(frame*0x6d6u));
  const auto reach=from_int(Raw(to_int(mul(wave,0x41200000)))+80u);
  write(at+0x34,4,reach); write(at+0x3c,4,reach);
  const auto inverse=div(ONE,mul(reach,reach));
  write(at+0x40,4,inverse); write(at+0x38,4,inverse);
  auto arc=scalar(u(at+8),12,{1,signed_byte(at),u(at+0xc),10});
  const std::array<std::array<Raw,4>,3> spray{{{0x8e63b0,20,0x409ffe80,0x426fffd7},{0x8e6400,5,0x40a00000,0x4270000d},{0x8e6450,3,0x40a0000e,0x42200057}}};
  for (const auto &p:spray) for (Raw n=0;n<p[1];++n) {
    const auto b=spell_ball(spell); Vec3 point;
    for (unsigned i=0;i<3;++i) point[i]=add(mul(b[i],p[2]),pos[i]);
    const auto v=spell_ball(spell);
    const auto base=from_int(facing-arc*91u), width=from_int(arc*182u);
    auto turn=to_int(add(base,mul(mul(from_int(spell_rand(spell)),TWO_M16),width)));
    const auto vx=add(v[0],mul(cos16(Raw(turn)),p[3]));
    turn=to_int(add(base,mul(mul(from_int(spell_rand(spell)),TWO_M16),width)));
    const auto vy=add(v[1],mul(sin16(Raw(turn)),p[3]));
    call("particle_spawn",{p[0],point[0],point[1],point[2],vx,vy,v[2],ZERO});
  }
  if (s32(u(FRAME))%15) return;
  const Vec3 origin{sub(pos[0],mul(cos16(facing),0x41400000)),sub(pos[1],mul(sin16(facing),0x41400000)),pos[2]};
  arc=scalar(u(at+8),12,{1,signed_byte(at),u(at+0xc),10});
  const auto found=call("world_collectObjectsInArc",{origin[0],origin[1],origin[2],facing,STACK_QUERY,0,0x42c00000,from_int(arc+10u),ZERO});
  if (found.size()>65536) throw Unsupported("arc query traversal limit");
  bool hit=false;
  for (const auto other:found) {
    const auto owner=u(at+0xc);
    if (!one_lane(vcall(owner,20,{other}))) continue;
    const auto s0=scalar(u(at+8),12,{0,signed_byte(at),u(at+0xc),0});
    const auto low=Raw(to_int(mul(from_int(s0),0x3e800000)));
    const auto base=from_int(low);
    const auto r=spell_rand(spell);
    const auto maximum=add(base,mul(mul(base,mul(from_int(r),TWO_M16)),0x3e4ccccc));
    const auto high=Raw(to_int(mul(maximum,0x3e800000)));
    hit=true;
    call("damage_apply",{other,u(at+0xc),100000,0,0,0x40,0x15,low&0xffff,high&0xffff,0,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,ZERO,ZERO});
  }
  if (hit) {
    const auto resource=named_scalar("res_find",{0x550b00,0x550b38});
    const auto point=vec(Address(u(at+0xc))+0x10);
    call("sound_playAtPos",{resource,point[0],point[1],point[2],ONE});
  }
  call("handle_clear",{STACK_QUERY});
}

// Wizard Beam: exact stored-state lifecycle; stack presentation arrays are tokens.
Raw Model::wizardbeam_cast(Raw skill, Raw level, Raw caster) {
  const auto spell=named_scalar("pool_new",{0x380});
  if (spell) wizardbeam_ctor(spell,level,caster,skill);
  return spell;
}
void Model::wizardbeam_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  write(at+4,4,0); write(at,1,level); write(at+0xc,4,owner); write(at+8,4,skill);
  const auto seed=draw(); write(at+0x10,4,seed); write(at+0x14,4,0x51a5b0);
  const auto hue=mul(from_int(spell_rand(spell)),TWO_M16);
  const auto colour=color_from_hsv(hue,0x3f4ccccc,ONE);
  const auto trail=[&](Raw kind,Raw duration,Raw col) {
    return named_scalar("trail_create",{kind,duration,5,col&255,(col>>8)&255,(col>>16)&255,0x80,TWO});
  };
  const auto first=trail(1,200,colour); write(at+0x1c,4,first);
  const auto other=add(hue,HALF);
  const auto second=color_from_hsv(fmodf(other,ONE),0x3f666666,ONE);
  const auto third=color_from_hsv(fmodf(other,ONE),0x3f199999,ONE);
  const auto t2=trail(1,100,second); write(at+0x20,4,t2);
  const auto t3=trail(1,100,third); write(at+0x24,4,t3);
  write(at+0x19,1,0); write(at+0x1a,1,0);
  for (unsigned i=0;i<5;++i) {
    write(at+0x354+8*i,4,0xfffe7960);
    const auto ring=trail(6,100,colour); write(at+0x350+8*i,4,ring);
  }
  const auto reach=scalar(u(at+8),10,{1,signed_byte(at)});
  write(at+0x348,4,from_int(reach*2u)); write(at+0x34c,4,from_int(reach*2u));
  const auto side=scalar(u(at+8),10,{3,signed_byte(at)});
  write(at+0x344,4,from_int(side*3u));
  for (unsigned k=0;k<6;++k) write(at+0x32c+4*k,4,u(at+0x344));
  write(at+0x378,4,colour);
  const auto target=trail(1,300,colour); write(at+0x37c,4,target);
  const auto beams=scalar(u(at+8),10,{2,signed_byte(at)});
  write(at+0xb8,4,beams); if (s32(beams)>6) write(at+0xb8,4,6);
  const auto n=s32(u(at+0xb8));
  if (n>65536) throw Unsupported("beam trail count limit");
  for (std::int32_t k=0;k<n;++k) {
    const auto a=trail(1,100,colour); write(at+0xbc+4*k,4,a);
    const auto b=trail(1,100,second); write(at+0xd4+4*k,4,b);
  }
  const auto res=named_scalar("res_find",{0x550c68,0x550c70});
  const auto point=vec(Address(u(at+0xc))+0x10);
  call("sound_playAtPos",{res,point[0],point[1],point[2],ONE});
}
Values Model::beam_flash(Raw spell, Raw) {
  const auto frame=[&] { return Raw(mem.load(FRAME,4)); };
  const auto colour=[&] { return Raw(mem.load(Address(spell)+0x378,4)); };
  const auto wave=sin16(frame()<<13);
  const auto first=color_scale_clamped(add(mul(add(wave,ONE),0x3e800000),0x3f4ccccc),colour());
  const auto second=color_scale_clamped(add(sin16(frame()<<13),0x3fe66666),colour());
  return {first,second};
}
void Model::beam_chain(Raw base, Raw velocity, Raw count, const Vec3 &direction, Raw length_at) {
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  Raw length=u(length_at);
  Vec3 prev{}; for (unsigned a=0;a<3;++a) prev[a]=mul(direction[a],length);
  const auto n=s32(count);
  if (n>65536) throw Unsupported("beam node count limit");
  for (std::int32_t i=1;i<n;++i) {
    const auto node=Address(base)+12*i, before=Address(base)+12*(i-1), speed=Address(velocity)+12*i;
    Vec3 want{},seg{};
    for (unsigned a=0;a<3;++a) want[a]=add(u(before+4*a),prev[a]);
    for (unsigned a=0;a<3;++a) write(node+4*a,4,add(u(node+4*a),u(speed+4*a)));
    if (i<n-1) for (unsigned a=0;a<3;++a)
      want[a]=mul(add(mul(want[a],0x40400000),mul(add(u(node+12+4*a),u(before+4*a)),HALF)),0x3e800000);
    for (unsigned a=0;a<3;++a) seg[a]=sub(u(node+4*a),u(before+4*a));
    for (unsigned a=0;a<3;++a)
      write(speed+4*a,4,mul(add(u(speed+4*a),mul(sub(want[a],u(node+4*a)),0x3d778fee)),0x3f5ae147));
    const auto inverse=rsqrt(ONE,add(add(mul(seg[0],seg[0]),mul(seg[1],seg[1])),mul(seg[2],seg[2])));
    length=u(length_at);
    for (unsigned a=0;a<3;++a) prev[a]=mul(mul(seg[a],inverse),length);
    for (unsigned a=0;a<3;++a) write(node+4*a,4,add(u(before+4*a),prev[a]));
  }
}
void Model::wizardbeam_run(Raw spell, const Vec3 &pos, Raw facing) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto frame=[&] { return u(FRAME); };
  // Snapshot Python range bounds at each loop, reread between separate loops.
  const auto beam_count=[&] {
    const auto n=s32(u(at+0xb8));
    if (n>65536) throw Unsupported("beam side count limit");
    return std::max(n,0);
  };
  const auto query=[&](const Vec3 &tip,const Vec3 &half) {
    Values args; for (unsigned a=0;a<3;++a) args.push_back(sub(tip[a],half[a]));
    for (unsigned a=0;a<3;++a) args.push_back(add(tip[a],half[a]));
    args.push_back(64); auto result=call("world_collectObjectsInBox",args);
    if (result.size()>65536) throw Unsupported("beam query count limit");
    return result;
  };
  const auto norm=[](const Vec3 &d) { return add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])); };
  const auto setpoints=[&](Raw trail,Raw count) { call("trail_setPoints_1918a0",{trail,STACK_QUERY,count,0x78,ZERO,ONE}); };
  const auto high=[&](Raw amount) { return add(amount,mul(mul(amount,mul(from_int(spell_rand(spell)),TWO_M16)),0x3e4ccccc)); };
  const Vec3 direction{cos16(facing),sin16(facing),ZERO};
  if (!mem.load(at+0x19,1)) {
    write(at+0x19,1,1);
    for (unsigned i=0;i<6;++i) {
      const auto f=from_int(i),length=u(at+0x348);
      write_vec(at+0x28+12*i,{mul(mul(direction[0],f),length),mul(mul(direction[1],f),length),mul(mul(f,ZERO),length)});
      write_vec(at+0x70+12*i,{});
    }
  }
  const auto reach=scalar(u(at+8),10,{1,signed_byte(at)})*12u;
  const auto side_reach=scalar(u(at+8),10,{3,signed_byte(at)})*12u;
  Vec3 tip{}; for (unsigned a=0;a<3;++a) tip[a]=add(pos[a],u(at+0x64+4*a));
  const auto found=query(tip,{from_int(reach),from_int(reach),0x47c35000});
  Raw best=0,best_d=from_int(reach*reach),best_far=ZERO;
  for (auto other:found) {
    if (!one_lane(vcall(u(at+0xc),20,{other}))) continue;
    Vec3 d{}; for (unsigned a=0;a<3;++a) d[a]=sub(u(Address(other)+0x10+4*a),tip[a]);
    const auto ex=sub(u(Address(other)+0x10),pos[0]),ey=sub(u(Address(other)+0x14),pos[1]);
    const auto dist=norm(d),far=sqrt(add(mul(ex,ex),mul(ey,ey)));
    if (lt(dist,best_d) && !le(mul(u(at+0x34c),0x40a00000),far)) { best=other;best_d=dist;best_far=far; }
  }
  const auto beams=beam_count();
  if (!best) {
    write(at+0x348,4,add(u(at+0x348),mul(sub(u(at+0x34c),u(at+0x348)),0x3d4ccccc)));
    call("trail_clear_191e58",{u(at+0x37c)});
    const auto n=beam_count();
    for (int k=0;k<n;++k) {
      call("trail_clear_191e58",{u(at+0xbc+4*k)}); call("trail_clear_191e58",{u(at+0xd4+4*k)});
    }
  } else {
    const auto res=named_scalar("res_find",{0x550c68,0x550c88});
    const auto point=vec(Address(u(at+0xc))+0x10);
    call("sound_playAtPos",{res,point[0],point[1],point[2],ONE});
    std::vector<Vec3> spokes; Raw offset=0;
    for (int k=0;k<beams;++k) {
      const auto turn=frame()*600u+offset+Raw(s16(Raw(mem.load(Address(best)+0x38,2))));
      // Both trigonometric arguments independently reread the frame/heading.
      spokes.push_back({cos16(turn),sin16(frame()*600u+offset+Raw(s16(Raw(mem.load(Address(best)+0x38,2))))),ZERO});
      offset+=Raw(s16(0xfffeu/Raw(beams)));
    }
    if (!mem.load(at+0x1a,1)) {
      write(at+0x1a,1,1); const auto n=beam_count();
      for (int k=0;k<n;++k) {
        if (std::size_t(k)>=spokes.size()) throw Unsupported("beam callback grew side count");
        for (unsigned j=0;j<4;++j) {
          const auto f=from_int(j),length=u(at+0x348); Vec3 node{};
          for (unsigned a=0;a<3;++a) node[a]=mul(mul(spokes[k][a],f),length);
          write_vec(at+0xec+0x30*k+12*j,node); write_vec(at+0x20c+0x30*k+12*j,{});
        }
      }
    }
    const auto candidates=query(tip,{from_int(side_reach),from_int(side_reach),0x42c80000});
    Values sides; std::vector<Raw> side_far;
    for (auto other:candidates) {
      const auto owner=u(at+0xc);
      if (other==owner || other==best || !(u(Address(other)+0xc)&0x20)) continue;
      if (!one_lane(vcall(owner,20,{other}))) continue;
      Vec3 d{}; for (unsigned a=0;a<3;++a) d[a]=sub(u(Address(other)+0x10+4*a),tip[a]);
      const auto flat=add(mul(d[0],d[0]),mul(d[1],d[1]));
      if (lt(add(flat,mul(d[2],d[2])),from_int(side_reach*side_reach)) &&
          std::int64_t(sides.size())<s32(u(at+0xb8))) { sides.push_back(other); side_far.push_back(sqrt(flat)); }
    }
    const Vec3 middle{add(u(Address(best)+0x10),ZERO),u(Address(best)+0x14),add(u(Address(best)+0x18),mul(u(Address(best)+0x20),HALF))};
    for (unsigned i=1;i<6;++i) {
      const auto node=at+0x28+12*i; Vec3 delta{};
      for (unsigned a=0;a<3;++a) delta[a]=sub(middle[a],add(u(node+4*a),pos[a]));
      const auto weight=powf(mul(from_int(i),0x3e4ccccc),0x40400000);
      for (unsigned a:{1u,2u,0u}) write(node+4*a,4,add(u(node+4*a),mul(delta[a],weight)));
    }
    const auto goal=fmin(mul(u(at+0x34c),0x40a00000),mul(best_far,0x3f8ccccc));
    write(at+0x348,4,add(u(at+0x348),mul(sub(mul(goal,0x3e4ccccc),u(at+0x348)),0x3d4ccccc)));
    const auto flash=beam_flash(spell,best);
    write(Address(best)+0x96,2,0x10); write(Address(best)+0x92,2,(flash[0]>>8)&255);
    write(Address(best)+0x94,2,(flash[0]>>16)&255); write(Address(best)+0x90,2,flash[0]&255);
    write(Address(best)+0x9e,2,0x10); write(Address(best)+0x98,2,flash[1]&255);
    write(Address(best)+0x9c,2,(flash[1]>>16)&255); write(Address(best)+0x9a,2,(flash[1]>>8)&255);
    const auto model=one_lane(vcall(best,16,{}));
    const auto count=named_scalar("model_getSamplePointCount",{model});
    if (count) {
      const auto centre=call("Creature_getVisualPos",{best});
      if (centre.size()!=3) throw Unsupported("beam visual position needs three lanes");
      for (unsigned i=0;i<10;++i) {
        const auto index=Raw(std::int64_t(spell_rand(spell))%s32(count));
        call("model_getSamplePoint",{model,best+0x114,centre[0],centre[1],centre[2],Raw(s16(Raw(mem.load(Address(best)+0x38,2)))),index,1,0,u(Address(best)+0xdc)});
      }
      setpoints(u(at+0x37c),10);
    }
    for (std::size_t k=0;k<sides.size();++k) {
      const auto other=Address(sides[k]);
      const Vec3 aim{add(u(other+0x10),ZERO),u(other+0x14),add(u(other+0x18),mul(u(other+0x20),HALF))};
      for (unsigned j=1;j<4;++j) {
        const auto node=at+0xec+0x30*k+12*j;
        const Vec3 world{add(add(u(node),u(Address(best)+0x10)),ZERO),add(u(node+4),u(Address(best)+0x14)),add(add(u(node+8),u(Address(best)+0x18)),mul(u(Address(best)+0x20),HALF))};
        Vec3 delta{}; for (unsigned a=0;a<3;++a) delta[a]=sub(aim[a],world[a]);
        const auto weight=powf(mul(from_int(j),0x3eaaaaaa),0x40400000);
        for (unsigned a:{1u,2u,0u}) write(node+4*a,4,add(u(node+4*a),mul(delta[a],weight)));
      }
      const auto wanted=fmin(mul(u(at+0x344),0x40400000),mul(side_far[k],0x3f8ccccc)),length=u(at+0x32c+4*k);
      write(at+0x32c+4*k,4,add(length,mul(sub(mul(wanted,0x3eaaaaaa),length),0x3d4ccccc)));
      const auto col=beam_flash(spell,Raw(other));
      write(other+0x92,2,(col[0]>>8)&255);write(other+0x94,2,(col[0]>>16)&255);write(other+0x90,2,col[0]&255);write(other+0x96,2,0x10);
      write(other+0x9e,2,0x10);write(other+0x98,2,col[1]&255);write(other+0x9a,2,(col[1]>>8)&255);write(other+0x9c,2,(col[1]>>16)&255);
    }
    const auto n=beam_count();
    for (std::size_t k=sides.size();k<std::size_t(n);++k) {
      const auto length=u(at+0x32c+4*k);
      write(at+0x32c+4*k,4,add(length,mul(sub(u(at+0x344),length),0x3d4ccccc)));
    }
    const auto trail_n=beam_count();
    for (int k=0;k<trail_n;++k) { setpoints(u(at+0xbc+4*k),4);setpoints(u(at+0xd4+4*k),4); }
    if (s32(frame())%20==0) {
      const auto amount=from_int(Raw(to_int(mul(from_int(scalar(u(at+8),10,{0,signed_byte(at)})),0x3eaaaaaa))));
      const auto maximum=high(amount);
      call("damage_apply",{best,u(at+0xc),100000,0,0,0x100,0,0,amount,maximum});
    }
    for (auto other:sides) if (s32(frame())%20==0) {
      const auto whole=from_int(scalar(u(at+8),10,{0,signed_byte(at)}));
      const auto amount=from_int(Raw(to_int(mul(mul(whole,0x3e800000),0x3eaaaaaa))));
      const auto maximum=high(amount);
      call("damage_apply",{other,u(at+0xc),100000,0,0,0x100,0,0,amount,maximum});
    }
    const auto chain_n=beam_count();
    for (int k=0;k<chain_n;++k) {
      if (std::size_t(k)>=spokes.size()) throw Unsupported("beam callback grew side count");
      beam_chain(Raw(at+0xec+0x30*k),Raw(at+0x20c+0x30*k),4,spokes[k],Raw(at+0x32c+4*k));
    }
  }
  for (unsigned trail:{0x1cu,0x20u,0x24u}) setpoints(u(at+trail),6);
  for (unsigned i=0;i<5;++i) {
    const auto age=frame()-u(at+0x354+8*i);
    if (s32(age)<30) {
      call("curve_sample_1911e8",{STACK_QUERY,6,STACK_QUERY,STACK_QUERY,mul(from_int(age),0x3d088888)});
      call("ring_draw_1920e0",{u(at+0x350+8*i),STACK_QUERY,STACK_QUERY,0x20,fmin(0x41200000,mul(from_int(30u-age),HALF))});
    } else call("trail_clear_191e58",{u(at+0x350+8*i)});
  }
  if (!(frame()&7)) {
    // Python signed arithmetic shift and truncating remainder, including negatives.
    const auto f=std::int64_t(s32(frame())); const auto shifted=f>=0 ? f/8 : -((-f+7)/8);
    write(indexed(spell,0x354,8,Raw(shifted%5)),4,frame());
  }
  beam_chain(spell+0x28,spell+0x70,6,direction,spell+0x348);
}

// Blessed: raw arithmetic and owned RNG only; presentation stays callback-owned.
Raw Model::blessed_can_use(Raw, Raw caster) { return carries(caster,0x3ce700) ? 0 : 1; }
Raw Model::blessed_cast(Raw skill, Raw level, Raw caster) {
  if (carries(caster,0x3ce700)) return 0;
  flash_both(caster,75,75,200,15);
  const auto spell=named_scalar("pool_new",{0xfc});
  if (spell) blessed_ctor(spell,level,caster,skill);
  return spell;
}
void Model::blessed_ctor(Raw spell, Raw level, Raw owner, Raw skill) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  spell_base(spell,level,owner,skill,0x5196f0);
  const auto huge=div(ONE,ZERO);
  write(at+0xc9,1,0); write_vec(at+0xf0,vec(Address(owner)+0x10));
  for (const auto &entry : std::array<std::pair<unsigned,Raw>,8>{{
      {0xd4,75},{0xca,75},{0xcc,75},{0xd2,75},{0xd6,200},{0xd8,255},{0xce,200},{0xd0,255}}})
    write(at+entry.first,2,entry.second);
  for (const auto &entry : std::array<std::pair<unsigned,Raw>,5>{{
      {0xec,0x3f266677},{0xdc,0},{0xe8,huge},{0xe4,0},{0xe0,huge}}})
    write(at+entry.first,4,entry.second);
  write(at+0xc8,1,0); call("light_link",{spell+0xc0});
  const auto seconds=scalar(skill,10,{1,Raw((level&0x80) ? (level&255u)|0xffffff00u : level&255u)});
  write(at+0xb4,4,u(FRAME)+seconds*60u);
  for (unsigned i=0;i<6;++i) {
    const auto sparkle=at+0x18+12*i, rate=at+0x60+12*i;
    write(sparkle,4,from_int(i*0x2aaa));
    const auto holder=u(at+0xc);
    const auto distance_draw=mul(from_int(spell_rand(spell)),TWO_M16);
    const auto out=mul(distance_draw,u(Address(holder)+0x1c));
    write(sparkle+4,4,add(out,out));
    const auto half=mul(u(Address(holder)+0x20),HALF);
    const auto t=mul(from_int(spell_rand(spell)),TWO_M16);
    write(sparkle+8,4,add(half,mul(t,mul(u(Address(u(at+0xc))+0x20),HALF))));
    const auto b=spell_ball(spell);
    write(rate,4,(i&1) ? add(b[0],0x43200000) : sub(b[0],0x43200000));
    write(rate+4,4,b[1]); write(rate+8,4,b[2]);
  }
  write_vec(at+0xa8,vec(Address(u(at+0xc))+0x10));
}
void Model::blessed_dtor(Raw spell, Raw flags) {
  const auto at=Address(spell);
  write(at+0x14,4,0x5196f0);
  call("light_unlink",{spell+0xc0}); call("light_dtor",{spell+0xc0,2});
  write(at+0x14,4,0x515ba0); call("handle_clear",{spell+4});
  if (flags&1) call("pool_free",{spell});
}
Raw Model::blessed_stat0(Raw spell) {
  return scalar(Raw(mem.load(Address(spell)+8,4)),10,{0,signed_byte(spell)});
}
void Model::blessed_run(Raw spell) {
  const auto at=Address(spell);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto frame=s32(u(FRAME));
  if (frame>=s32(u(at+0xb4))) { spell_delete(spell); return; }
  const auto wave=sin16(Raw(frame)*1000u);
  write_vec(at+0xf0,vec(Address(u(at+0xc))+0x10)); call("light_relink",{spell+0xc0});
  const auto size=add(from_int(signed_byte(spell)+0x42u),mul(wave,0x41200000));
  const auto strong=Raw(to_int(add(mul(wave,0x42440000),0x43480000)));
  const auto faint=Raw(to_int(add(mul(wave,0x40a00000),0x41700000)));
  const auto mid=Raw(to_int(add(mul(wave,0x42440000),0x42960000)));
  write(at+0xdc,4,size); write(at+0xe4,4,size);
  flash_both(u(at+0xc),mid,mid,strong,faint);
  const auto inverse=div(ONE,mul(size,size));
  write(at+0xe8,4,inverse); write(at+0xe0,4,inverse);
  const auto n=std::min(s32(scalar(u(at+8),10,{2,signed_byte(spell)})),6);
  for (int i=0;i<n;++i) {
    const auto sparkle=at+0x18+12*i, rate=at+0x60+12*i;
    const auto ball=spell_ball(spell);
    Vec3 v{}, state{};
    for (unsigned k=0;k<3;++k) v[k]=mul(add(u(rate+4*k),mul(ball[k],0x3df5be5d)),0x3f5999bb);
    v[0]=(i&1) ? add(v[0],0x43200000) : sub(v[0],0x43200000);
    for (unsigned k=0;k<3;++k) write(rate+4*k,4,v[k]);
    for (unsigned k=0;k<3;++k) state[k]=add(u(sparkle+4*k),v[k]);
    const auto holder=u(at+0xc);
    const auto widest=add(u(Address(holder)+0x1c),u(Address(holder)+0x1c));
    if (lt(widest,state[1])) state[1]=widest;
    if (lt(state[1],ZERO)) state[1]=ZERO;
    if (lt(u(Address(holder)+0x20),state[2])) state[2]=u(Address(holder)+0x20);
    const auto lowest=mul(u(Address(holder)+0x20),HALF);
    if (lt(state[2],lowest)) state[2]=lowest;
    for (unsigned k=0;k<3;++k) write(sparkle+4*k,4,state[k]);
    const auto turn=Raw(to_int(state[0]));
    const auto ring=mul(add(state[1],u(Address(holder)+0x1c)),0x3f7fefd0);
    const auto shade=mul(wave,0x42200000);
    const auto dim=Raw(to_int(add(shade,0x42480000))), lit=Raw(to_int(add(shade,0x42c80000)));
    const Vec3 point{add(mul(cos16(turn),ring),u(Address(holder)+0x10)),
                     add(mul(sin16(turn),ring),u(Address(holder)+0x14)),add(state[2],u(Address(holder)+0x18))};
    call("glow_add",{point[0],point[1],point[2],dim,dim,lit,dim,u(0x4f8498),0,2,ZERO,0x457a0000});
    for (const auto &group : std::array<std::array<Raw,4>,2>{{
        {{0x8e5b90,6,0x3ecccccc,0x4000008a}},{{0x8e5be0,4,0x40c1dab9,0x3fffff0c}}}}) {
      for (unsigned j=0;j<group[1];++j) {
        auto velocity=spell_ball(spell);
        for (auto &c:velocity) c=mul(c,group[2]);
        const auto q=spell_ball(spell);
        call("particle_spawn",{group[0],add(mul(q[0],group[3]),point[0]),add(mul(q[1],group[3]),point[1]),
            add(mul(q[2],group[3]),point[2]),velocity[0],velocity[1],velocity[2],ZERO});
      }
    }
  }
}
// Repulse/Dismiss Undead (21/22), authored from the pinned executable contract.
Raw Model::repulse_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw) {
  const Vec3 up{add(pos[0],ZERO),pos[1],add(pos[2],0x42100000)};
  const auto step=flare_quot(0x10000,level+4);
  Raw angle=0;
  for (unsigned count=0;;++count) {
    if (count>=65536) throw Unsupported("Repulse soul loop limit");
    const Vec3 velocity{mul(cos16(Raw(s16(angle))),0x43e10000),mul(sin16(Raw(s16(angle))),0x43e10000),0x42480000};
    const auto block=named_scalar("pool_new",{0xb0});
    if (block) {
      call("LostSoul_ctor",{block,up[0],up[1],up[2],velocity[0],velocity[1],velocity[2],angle,0,1,0});
      ++hidden_draws;
    }
    angle+=step;
    if (s32(angle)>0xffff) break;
  }
  light_effect(0xbc,up,{0,0,0x42c80000,0x42c80000,0x437f0000,0xc2c00000,ZERO,ZERO,ZERO,ZERO,0x42f00000,0x43b40000,ZERO,ONE});
  const auto dmg=scalar(skill,10,{0,level}),radius=scalar(skill,10,{1,level});
  damage_radial(caster,pos,dmg,0xa100900,0,0x40,from_int(radius*12),from_int(radius*24));
  const auto sound=named_scalar("res_find",{0x550e50,0x550e58});
  call("sound_playAtPos",{sound,pos[0],pos[1],pos[2],ONE});
  return 0;
}
void Model::damage_radial(Raw attacker, const Vec3 &pos, Raw dmg, Raw flags, Raw minimum, Raw skip, Raw inner, Raw outer) {
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto found=box_around(pos,outer,0x80);
  const auto full=from_int(dmg);
  auto scale=0x42c80000u;
  if (lt(ONE,sub(outer,inner))) scale=div(full,sub(outer,inner));
  for (auto other:found) {
    const auto at=Address(other);
    if (u(at+0xc)&0x20) {
      if ((flags&0x8000000) && signed_byte(at+0xb4)!=0x10) continue;
      if (attacker && (u(Address(attacker)+0xc)&0x20)) {
        const auto enemy=vcall(attacker,20,{other});
        if (enemy.size()!=1) throw Unsupported("Creature.v20 scalar required");
        if (!enemy[0]) continue;
      }
    } else if (flags&0x8000000) continue;
    if (other==attacker || (u(at+0xc)&skip)) continue;
    const auto dy=fabs(sub(u(at+0x14),pos[1])),dx=fabs(sub(u(at+0x10),pos[0]));
    const auto distance=sub(sub(add(dx,dy),mul(fmin(dx,dy),0x3eaa7ef9)),u(at+0x1c));
    if (le(outer,distance)) continue;
    auto amount=mul(sub(outer,distance),scale);
    const auto least=from_int(minimum);
    if (lt(amount,least)) amount=least;
    amount=fmin(amount,full);
    vcall(other,2,{attacker,flags,from_int(Raw(to_int(amount)))});
  }
}
Raw Model::dismissundead_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing) {
  const auto obj=named_scalar("pool_new",{0xc0});
  if (obj) holybolt_ctor(obj,pos,Raw(s16(facing)),level,caster,skill);
  return 0;
}
void Model::holybolt_ctor(Raw obj, const Vec3 &pos, Raw facing, Raw level, Raw owner, Raw skill) {
  const auto at=Address(obj);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto heading=[&](Address a) { return Raw(s16(Raw(mem.load(a,2)))); };
  projectile_ctor(obj,pos,owner,0);
  write(at+0x48,4,0x50e908); write(at+0x7d,1,0); write_vec(at+0xa4,pos);
  for (const auto &v:std::array<std::pair<unsigned,Raw>,8>{{{0x88,0xfc},{0x8a,500},{0x80,0xfc},{0x82,500},{0x8c,0xffe0},{0x84,0xffe0},{0x86,0xac},{0x7e,0xac}}}) write(at+v.first,2,v.second);
  for (const auto &v:std::array<std::pair<unsigned,Raw>,5>{{{0x98,0x42f00000},{0x90,0x42f00000},{0x9c,0x3891a2b3},{0x94,0x3891a2b3},{0xa0,0}}}) write(at+v.first,4,v.second);
  write(at+0x7c,1,0); call("light_link",{obj+0x74});
  write(at+0x38,2,facing); write(at+0x1c,4,0x41900000);
  write_vec(at+0x58,{mul(cos16(Raw(s16(facing))),0x43af0000),mul(sin16(heading(at+0x38)),0x43af0000),ZERO});
  write(at+0xb8,4,skill); write(at+0xbc,4,level); write(at+0xc,4,u(at+0xc)|0x4001);
  const auto frame=u(FRAME); // read before the statistic callback
  write(at+0x54,4,frame+scalar(skill,10,{2,level})*60);
  call("obj_runEveryFrame",{obj});
  const auto found=box_around(vec(at+0x10),0x43b40000,0x20);
  std::optional<std::size_t> front,back;
  Raw front_d=0x4b189680,back_d=0x4b189680;
  for (std::size_t i=0;i<found.size();++i) {
    const auto other=found[i]; const auto p=Address(other);
    if (other==obj || other==owner || !(u(p+0xc)&0x20)) continue;
    Vec3 d;
    for (unsigned k=0;k<3;++k) d[k]=sub(u(p+0x10+4*k),u(at+0x10+4*k));
    const auto distance=sqrt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])));
    if (le(front_d,distance) && le(back_d,distance)) continue;
    if (signed_byte(p+0xb4)!=0x10 || (u(p+0xc)&8)) continue;
    const auto enemy=vcall(owner,20,{other});
    if (enemy.size()!=1) throw Unsupported("Creature.v20 scalar required");
    if (!enemy[0]) continue;
    const auto co=cos16(heading(Address(owner)+0x38)),si=sin16(heading(Address(owner)+0x38));
    const auto dot=add(mul(co,sub(u(p+0x10),u(Address(owner)+0x10))),mul(si,sub(u(p+0x14),u(Address(owner)+0x14))));
    if (le(dot,ZERO)) { back=i; back_d=distance; } else { front=i; front_d=distance; }
  }
  const auto chosen=front ? front : back;
  write(at+0x64,4,chosen ? found[*chosen] : 0);
  if (!u(at+0x64)) {
    write(at+0x68,4,add(u(at+0x10),mul(mul(cos16(heading(Address(owner)+0x38)),0x41400000),0x457a0000)));
    const auto far=mul(mul(sin16(heading(Address(owner)+0x38)),0x41400000),0x457a0000);
    write(at+0x70,4,add(u(at+0x18),0x42100000)); write(at+0x6c,4,add(u(at+0x14),far));
  }
  write(at+0xb4,4,named_scalar("trail_create",{4,0x2ee,4,0x6b,100,0xea,0xab,0x3ea4dd2f}));
  const auto ax=mul(cos16(heading(at+0x38)),0x41200000),ay=mul(sin16(heading(at+0x38)),0x41200000);
  call("trail_addPoint",{u(at+0xb4),add(u(at+0x10),ax),add(u(at+0x14),ay),u(at+0x18)});
  const auto sound=named_scalar("res_find",{0x550ee8,0x550ef0});
  call("sound_playOnObject",{sound,obj,ONE}); // voice result deliberately dropped
}
void Model::holybolt_run(Raw obj) {
  const auto at=Address(obj);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto h=[&] { return Raw(s16(Raw(mem.load(at+0x38,2)))); };
  const auto target=u(at+0x64);
  if (target) {
    const auto ax=u(Address(target)+0x10),ay=u(Address(target)+0x14);
    write(at+0x68,4,ax); write(at+0x6c,4,ay); write(at+0x70,4,add(u(Address(target)+0x18),0x42100000));
    auto heading=Raw(mem.load(at+0x38,2));
    const auto diff=s16(atan2_16(sub(ay,u(at+0x14)),sub(ax,u(at+0x10)))-heading);
    if (diff>0x443 || diff< -0x443) heading+=diff<1 ? Raw(-0x444) : 0x444;
    else heading+=Raw(diff);
    write(at+0x38,2,heading);
    if (u(Address(u(at+0x64))+0xc)&8) write(at+0x64,4,0);
  }
  write(at+0x58,4,mul(cos16(h()),0x43700000)); write(at+0x5c,4,mul(sin16(h()),0x43700000));
  const auto dz=s16(Raw(to_int(sub(u(at+0x70),u(at+0x18)))));
  write(at+0x60,4,((Raw(dz)+9)&0xffff)<0x13 ? ZERO : dz<1 ? 0xc2480000 : 0x42480000);
  const auto result=move_and_collide(obj,vec(at+0x58));
  Vec3 side{u(at+0x5c),neg(u(at+0x58)),ZERO};
  const auto inverse=rsqrt(ONE,add(add(mul(side[0],side[0]),mul(side[1],side[1])),mul(side[2],side[2])));
  for (auto &v:side) v=mul(v,inverse);
  const auto frame=u(FRAME);
  auto wave=mul(cos16(Raw(s16(frame*0x1ae0))),0x3f410624);
  for (auto &v:side) v=mul(v,wave);
  wave=mul(sin16(Raw(s16(frame*0x1ae0))),0x3f410624);
  const Vec3 up{mul(ZERO,wave),mul(ZERO,wave),mul(ONE,wave)};
  Vec3 point;
  for (unsigned i=0;i<3;++i) point[i]=add(add(side[i],up[i]),u(at+0x10+4*i));
  const auto ahead=mul(cos16(h()),0x41200000);
  point[1]=add(point[1],mul(sin16(h()),0x41200000)); point[0]=add(point[0],ahead);
  call("trail_addPoint",{u(at+0xb4),point[0],point[1],point[2]});
  if (s32(result)==1) {
    write(at+0x38,2,Raw(mem.load(at+0x38,2))+Raw(s32(obj_rand(obj))%0x7fff)-0x4000);
    for (unsigned i=0;i<10;++i) {
      const auto old=Raw(mem.load(at+0x38,2));
      const auto candidate=Raw(s16(old+Raw(s32(obj_rand(obj))%0x7fff)-0x4000));
      const Vec3 spot{add(u(at+0x10),mul(cos16(candidate),0x41c00000)),add(u(at+0x14),mul(sin16(candidate),0x41c00000)),u(at+0x18)};
      Vec3 way;
      for (unsigned k=0;k<3;++k) way[k]=sub(spot[k],u(at+0x10+4*k));
      const auto p=vec(at+0x10);
      if (le(named_scalar("terrain_raycast",{u(0x44fdc8),p[0],p[1],p[2],way[0],way[1],way[2],0,TWO}),ZERO)) write(at+0x38,2,candidate);
    }
    const auto sound=named_scalar("res_find",{0x550ee8,0x550f08}); call("sound_playOnObject",{sound,obj,ONE});
    for (unsigned i=0;i<25;++i) {
      const auto b=obj_ball(obj),p=vec(at+0x10);
      call("particle_spawn",{0x8e6880,p[0],p[1],p[2],mul(b[0],0x42666666),mul(b[1],0x42666666),mul(b[2],0x42666666),ZERO});
    }
  }
  const auto p=vec(at+0x10); call("voice_setPosition",{u(at+0xb0),p[0],p[1],p[2],ONE});
  write_vec(at+0xa4,vec(at+0x10)); call("light_relink",{obj+0x74});
  if (s32(u(at+0x54))<=s32(u(FRAME))) delete_self(obj);
}
void Model::holybolt_hit(Raw obj, Raw other) {
  const auto at=Address(obj);
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto owner=[&] { const auto node=u(at+0x4c); return node ? u(Address(node)+0xc) : 0; };
  if (!(u(Address(other)+0xc)&0x20)) return;
  if (u(at+0x4c)) {
    const auto enemy=vcall(owner(),20,{other});
    if (enemy.size()!=1) throw Unsupported("Creature.v20 scalar required");
    if (!enemy[0]) return;
  }
  if (!u(at+0x54)) return;
  write(at+0x54,4,0);
  light_effect(0xbc,vec(at+0x10),{0,0,0x432c0000,0x437c0000,0x43fa0000,0xc2000000,ZERO,ZERO,ZERO,ZERO,0x43340000,0x42f00000,ONE,ONE});
  for (unsigned i=0;i<50;++i) {
    const auto b=obj_ball(obj),p=vec(at+0x10);
    call("particle_spawn",{0x8e68d0,p[0],p[1],p[2],mul(b[0],0x42666666),mul(b[1],0x42666666),mul(b[2],0x42666666),ZERO});
  }
  const auto sound=named_scalar("res_find",{0x550ee8,0x550f60}); call("sound_playOnObject",{sound,obj,ONE});
  const auto stat=signed_byte(Address(other)+0xb4)==0x10 ? 0u : 1u;
  const auto dmg=scalar(u(at+0xb8),10,{stat,u(at+0xbc)});
  const auto extra=Raw(to_int(mul(mul(mul(from_int(obj_rand(obj)),TWO_M16),0x3e4ccccc),from_int(dmg))));
  call("damage_apply",{other,owner(),10000000,0,0,0x200c040,0,0,from_int(dmg),from_int(dmg+extra)});
  if (le(u(Address(other)+0x228),ZERO)) {
    const auto soul=named_scalar("pool_new",{0xb0});
    if (soul) {
      const auto p=vec(at+0x10),v=vec(at+0x58);
      call("LostSoul_ctor",{soul,p[0],p[1],p[2],v[0],v[1],v[2],Raw(s16(Raw(mem.load(at+0x38,2)))),0,0,1});
      ++hidden_draws;
    }
  }
}
// Poison Rain: authored from the pinned executable Python contract, section 24.
Raw Model::poisonrain_can_use(Raw,Raw) const { return 1; }
Raw Model::poisonrain_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  const auto reach=from_int(scalar(skill,12,{1,level,caster,10}));
  Raw ox=ZERO,oy=ZERO;
  if (mem.load(Address(caster)+0xc,4)&0x40) {
    ox=mul(mul(cos16(Raw(s16(facing))),reach),0x41400000);
    oy=mul(mul(sin16(Raw(s16(facing))),reach),0x41400000);
  }
  const auto x=add(ox,pos[0]),y=add(oy,pos[1]);
  const auto ground=named_scalar("terrain_height",{Raw(mem.load(0x44fdc8,4)),x,y,ZERO,reach});
  const Vec3 centre{x,y,add(add(ZERO,ground),0x41400000)};
  const auto obj=named_scalar("pool_new",{0x6c});
  if (obj) poisonrain_ctor(obj,level,caster,centre,skill,0);
  return 0;
}
void Model::poisonrain_ctor(Raw obj,Raw level,Raw owner,const Vec3 &pos,Raw skill,Raw sound) {
  const Address at=obj;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  gameobject_ctor(obj,pos,0,0);
  write(at+0x48,4,0x51aa68); write(at+0x5c,4,level);
  write(at+0xc,4,u(at+0xc)|0x421b); write(at+0x60,4,skill); write(at+0x64,4,0);
  const Raw handle=obj+0x64;
  call("handle_link",{handle,owner,owner});
  skill=u(at+0x60);
  const auto radius=scalar(skill,13,{1,u(at+0x5c),handle,10});
  write(at+0x54,4,from_int(radius*12u));
  if ((u(Address(owner)+0xc)&0x60)==0x20)
    write(at+0x50,4,Raw(to_int(add(from_int(u(0x44fdcc)),0x4221ffff))));
  else write(at+0x50,4,0);
  const auto seconds=scalar(u(at+0x60),13,{2,u(at+0x5c),handle,10});
  write(at+0x4c,4,u(at+0x50)+seconds*60u);
  const auto block=named_scalar("pool_new",{0xbc});
  if (block) {
    const auto r=u(at+0x54);
    const auto again=scalar(u(at+0x60),13,{2,u(at+0x5c),handle,10});
    call("LightEffect_ctor",{block,add(pos[0],ZERO),pos[1],add(pos[2],0x41400000),0,1,
      ZERO,0x437f0000,ZERO,0x42c80000,ZERO,0x437f0000,ZERO,0x42c80000,
      fmin(0x43960000,r),fmin(0x43960000,add(r,r)),0x3f266666,add(from_int(again),0x3f2ccccc)});
    ++hidden_draws;
  }
  const auto strength=scalar(u(at+0x60),13,{0,u(at+0x5c),handle,0});
  write(at+0x58,4,strength); write(at+0x68,4,UINT32_MAX);
  if (sound) { const auto voice=named_scalar("sound_playOnObject",{sound,obj,ONE}); write(at+0x68,4,voice); }
  call("obj_runEveryFrame",{obj});
}
void Model::poisonrain_run(Raw obj) {
  const Address at=obj;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto frame=s32(u(0x44fdcc));
  if (!(s32(u(at+0x50))<frame)) return;
  if (!(frame<s32(u(at+0x4c)) && u(at+0x64))) {
    call("sound_stop",{u(at+0x68)});
    const auto radius=u(at+0x54);
    const Vec3 pos{add(u(at+0x10),ZERO),u(at+0x14),add(u(at+0x18),0x41400000)};
    light_effect(0xbc,pos,{0,1,ZERO,0x437f0000,ZERO,0x42c80000,ZERO,0x437f0000,ZERO,0x42c80000,
      fmin(0x43960000,add(radius,radius)),ZERO,0x3f266666,HALF});
    delete_self(obj); return;
  }
  const auto base=vec(at+0x10);
  const auto count=[&](Raw k) { const auto r=u(at+0x54); return mul(mul(mul(r,k),mul(r,0x40490fcf)),0x3be38e38); };
  // Reevaluate the radius after every callback. Refuse beyond the history domain.
  for (Raw i=0;lt(from_int(i),count(0x3ece0bb2));++i) {
    if (i>=65536) throw Unsupported("Poison Rain drop count limit");
    const auto angle=Raw(s16(obj_rand(obj)));
    const auto dist=mul(from_int(obj_rand(obj)),TWO_M16);
    const auto px=mul(cos16(angle),mul(dist,u(at+0x54)));
    const auto py=mul(sin16(angle),mul(dist,u(at+0x54)));
    const auto high=obj_rand(obj)%200;
    call("particle_spawn",{0x8e6a10,add(px,base[0]),add(py,base[1]),
      add(add(add(0x43480000,from_int(high)),0xc2c80000),base[2]),ZERO,ZERO,0xc2a00000,ZERO});
  }
  for (Raw i=0;lt(from_int(i),count(0x3ecf2e48));++i) {
    if (i>=65536) throw Unsupported("Poison Rain splash count limit");
    const auto angle=Raw(s16(obj_rand(obj)));
    const auto dist=mul(from_int(obj_rand(obj)),TWO_M16);
    const auto px=mul(cos16(angle),mul(dist,u(at+0x54)));
    const auto py=mul(sin16(angle),mul(dist,u(at+0x54)));
    const auto vx=add(from_int(obj_rand(obj)&1),0xbf800000);
    const auto vy=add(from_int(obj_rand(obj)&1),0xbf800000);
    const auto x=add(px,base[0]),y=add(py,base[1]);
    const auto ground=named_scalar("terrain_heightAtPoint",{u(0x44fdc8),x,y,add(ZERO,base[2])});
    call("particle_spawn",{0x8e6a60,x,y,add(ground,0x40a00000),vx,vy,ZERO,ZERO});
  }
  if (s32(u(0x44fdcc))%30!=0) return;
  const auto half=from_int(Raw(to_int(u(at+0x54))));
  const auto found=box_around(vec(at+0x10),half,0x40);
  const auto owner=u(Address(u(at+0x64))+0xc);
  for (const auto other:found) {
    const auto enemy=vcall(owner,20,{other});
    if (enemy.size()!=1) throw Unsupported("Poison Rain enemy scalar required");
    if (!enemy[0]) continue;
    const auto d=scalar(u(at+0x60),13,{0,u(at+0x5c),obj+0x64,0});
    const auto extra=Raw(to_int(mul(mul(from_int(d),mul(from_int(obj_rand(obj)),TWO_M16)),0x3e4ccccc)));
    call("damage_apply",{other,owner,100000,0,0,0x40,0x17,d&0xffffu,(d+extra)&0xffffu,0,
      UINT32_MAX,UINT32_MAX,UINT32_MAX,0,ZERO,ZERO});
  }
}
namespace {
std::string image_string(const Memory &mem, Address addr) {
  return ImageExe([&mem](Address at,std::size_t n) { return Raw(mem.load(at,n)); }).cstr(addr);
}
Raw square_length(const Vec3 &d) { return add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])); }
}
std::optional<std::string> ImageItemTables::prefix_text(std::size_t index,std::int32_t category) const {
  if (!image || index>65536) throw Unsupported("item image prefix index");
  const auto at=std::int64_t(0x400e50+0xc)+(std::int64_t(index)*10+category)*4;
  if (at<0 || at>UINT32_MAX) throw Unsupported("item image address");
  const auto pointer=Raw(image->load(Address(at),4));
  return pointer ? std::optional<std::string>(image_string(*image,pointer)) : std::nullopt;
}
std::optional<std::string> ImageItemTables::suffix_text(std::size_t index,std::int32_t category) const {
  if (!image || index>65536) throw Unsupported("item image suffix index");
  const auto at=std::int64_t(0x4019e0+8)+(std::int64_t(index)*5+category)*4;
  if (at<0 || at>UINT32_MAX) throw Unsupported("item image address");
  const auto pointer=Raw(image->load(Address(at),4));
  return pointer ? std::optional<std::string>(image_string(*image,pointer)) : std::nullopt;
}
Raw ImageItemTables::weapon_mult(std::int32_t subtype) const {
  const auto at=std::int64_t(0x3e9cb0)+std::int64_t(subtype)*4;
  if (!image || at<0 || at>UINT32_MAX) throw Unsupported("item image address");
  return Raw(image->load(Address(at),4));
}
std::array<Raw,3> ImageItemTables::quality_row(std::int32_t row) const {
  const auto at=std::int64_t(0x3e9c18)+std::int64_t(row)*12;
  if (!image || at<0 || at>UINT32_MAX) throw Unsupported("item image address");
  return {Raw(image->load(Address(at),4)),Raw(image->load(Address(at)+4,4)),Raw(image->load(Address(at)+8,4))};
}
const ImageItemTables &Model::item_tables() {
  if (item_tables_cache) return *item_tables_cache;
  ImageItemTables t; t.image=&mem;
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto h=[&](Address a) { return s16(Raw(mem.load(a,2))); };
  const auto str=[&](Address a) { return image_string(mem,a); };
  for (unsigned preset=0;preset<2;++preset) {
    auto &rows=preset ? t.presets : t.defs;
    Address a=preset ? 0x3ea550 : 0x3f7650;
    for (;u(a);a+=0x40) {
      if (rows.size()>=65536) throw Unsupported("item definition count limit");
      auto d=std::make_shared<items::Definition>();
      d->addr=Raw(a); d->index=int(rows.size()); d->preset=bool(preset); d->name=str(u(a));
      d->level=h(a+4); d->weight=u(a+8); d->count=h(a+0xc); d->damage_min=h(a+0xe); d->damage_max=h(a+0x10);
      d->price=s32(u(a+0x14)); d->armor=h(a+0x18); d->attack=h(a+0x1a);
      d->slot=s32(signed_byte(a+0x1c)); d->category=s32(signed_byte(a+0x1d)); d->subtype=s32(signed_byte(a+0x1e));
      d->damage_bits=u(a+0x20); d->flags=mem.load(a+0x28,8);
      if (u(a+0x30)) d->picture=str(u(a+0x30));
      d->model_row=h(a+0x34); rows.push_back(d);
    }
  }
  for (Address a=0x400e50;u(a);a+=0x28) {
    if (t.prefixes.size()>=65536) throw Unsupported("item prefix count limit");
    items::Prefix p; p.addr=Raw(a); p.index=int(t.prefixes.size()); p.key=str(u(a)); p.level=h(a+4);
    p.mask=mem.load(a+0x18,8); p.damage_mask=u(a+0x20); t.prefixes.push_back(p);
  }
  for (Address a=0x4019e0;u(a);a+=0x14) {
    if (t.suffixes.size()>=65536) throw Unsupported("item suffix count limit");
    items::Suffix v; v.addr=Raw(a); v.index=int(t.suffixes.size()); v.key=str(u(a)); v.level=h(a+4); t.suffixes.push_back(v);
  }
  const std::array<std::pair<Raw,int>,43> formats{{
    {0x520190,0x23},{0x5201a8,0x1e},{0x5201c0,0x1d},{0x5201d0,0x1c},{0x5201e0,0x1b},{0x5201f0,0x1a},
    {0x520200,0x19},{0x520210,0x15},{0x520220,0x16},{0x520230,0x18},{0x520248,0x17},{0x520260,0x14},
    {0x520270,0xe},{0x520280,0xf},{0x520298,0x10},{0x5202b0,0x11},{0x5202c8,0x12},{0x5202e0,0x13},
    {0x5202f0,0xd},{0x520300,0xb},{0x520318,0xc},{0x520330,5},{0x520340,6},{0x520350,7},{0x520360,8},
    {0x520378,9},{0x520390,0xa},{0x5203a0,0},{0x5203b0,1},{0x5203c0,2},{0x5203d0,3},{0x5203e0,4},
    {0x5203f0,0x21},{0x520408,0x1f},{0x520420,0x20},{0x520438,0x22},{0x520450,0x24},{0x520468,0x25},
    {0x520478,0x27},{0x520488,0x28},{0x5204a0,0x2a},{0x5204b0,-1},{0x5204b8,0x26}}};
  for (const auto &f:formats) t.formats.push_back({str(f.first),f.second<0 ? std::nullopt : std::optional<int>(f.second)});
  t.potion_stem=str(0x520180); t.thrown_stem=str(0x520070); t.circlet_stem=str(0x520188); t.circlet_stem_equip=str(0x5385e8);
  t.arrows=str(0x520040); t.recall=str(0x52b798); t.name_key_format=str(0x520058); t.name_space=str(0x520060); t.of_key=str(0x520068);
  t.gold_scale_multiplayer=mem.load(0x520048,8);
  item_tables_cache=std::move(t); return *item_tables_cache;
}
void Model::item_parse(Raw text,Raw record) {
  const auto raw=image_string(mem,text);
  items::Rng rng(Raw(mem.load(G_RAND_SEED,4)));
  // Parsing uses the existing exact item contract. Its RNG has no callbacks;
  // replay its consumed draws into this model before ordered record stores.
  items::Item item;
  try { item=items::parse_descriptor(item_tables(),raw,-1,false,&rng); }
  catch (const items::Error &) { throw Unsupported("item descriptor outside supported contract"); }
  SharedDraws shared(*this);
  for (std::uint64_t n=0;n<rng.draws;++n) shared.next();
  write(record,4,item.definition->addr); write(Address(record)+4,2,Raw(item.count));
  if (writes.size()>=creature_hp::MAX_HISTORY) throw Unsupported("Skill write history limit");
  writes.push_back({record+8,8,item.flags}); mem.store(Address(record)+8,8,item.flags);
  for (unsigned i=0;i<4;++i) if (item.mods[i]) {
    const auto &m=*item.mods[i]; write(Address(record)+0x24+8*i,2,Raw(m.code));
    for (unsigned k=0;k<3;++k) write(Address(record)+0x26+8*i+2*k,2,Raw(m.values[k]));
  }
}
Raw Model::item_resource_index(Raw record) const {
  const auto row=Raw(s16(Raw(mem.load(Address(Raw(mem.load(record,4)))+0x34,2))));
  const auto flags=mem.load(Address(record)+8,8);
  return row+Raw(flags&0x1000000000ull ? (flags>>11)&31 : 0);
}
Raw Model::hammer_can_use(Raw,Raw caster) { return carries(caster,0x3cf768) ? 0 : 1; }
Raw Model::hammer_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  flash_both(caster,Raw(-100),Raw(-100),Raw(-100),0);
  const auto shaft=named_scalar("pool_new",{0x5c});
  if (shaft) { call("GlowingShaft_ctor",{shaft,pos[0],pos[1],pos[2],0x5a,0x56,0x7e,0xfa}); ++hidden_draws; }
  light_effect(0xbc,{add(pos[0],ZERO),pos[1],add(pos[2],0x42100000)},
    {0,0,ZERO,0x431b0000,0x437f0000,0xc2c00000,ZERO,ZERO,ZERO,ZERO,0x43340000,0x43100000,ZERO,ONE});
  const auto hammer=named_scalar("pool_new",{0xe8});
  Raw spell=0;
  if (hammer) {
    hammer_ctor(hammer,{add(pos[0],ZERO),pos[1],add(pos[2],0x42700000)},Raw(s16(facing)),caster,level,skill);
    spell=named_scalar("pool_new",{0x1c});
    if (spell) hammer_spell_ctor(spell,level,caster,skill,hammer);
  }
  return spell;
}
void Model::hammer_ctor(Raw obj,const Vec3 &pos,Raw facing,Raw owner,Raw level,Raw skill) {
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  gameobject_ctor(obj,pos,0,0);
  write(Address(obj)+0x4c,4,0); write(Address(obj)+0x48,4,0x51a2a0); write(Address(obj)+0x50,4,0);
  const Address record=Address(obj)+0x58;
  for (unsigned k=0;k<4;++k) write(record+0x24+8*k,2,0xffff);
  write(record+0x21,1,0xff); write(record+0x20,1,0xff);
  for (unsigned k=0;k<8;++k) write(record+0x10+2*k,2,0);
  write(Address(obj)+0x38,2,facing);
  if (owner) call("handle_link",{obj+0x4c,owner,owner});
  write(Address(obj)+0xa4,4,level);
  const Raw texts[]={0x5508d0,0x5508f0,0x550910,0x550930};
  item_parse(s32(level)>=0 && s32(level)<4 ? texts[level] : 0x550950,Raw(record));
  write(Address(obj)+0xa4,4,u(Address(obj)+0xa4)+1); write(Address(obj)+0xc,4,u(Address(obj)+0xc)|1);
  call("obj_runEveryFrame",{obj});
  const auto sound=named_scalar("res_find",{0x550970,0x550978}); call("sound_playOnObject",{sound,obj,ONE});
  write(Address(obj)+0xb4,4,0x46800000); write_vec(Address(obj)+0xd8,vec(Address(obj)+0x10)); write(Address(obj)+0xc4,4,skill);
  const auto duration=scalar(skill,10,{1,u(Address(obj)+0xa4)}); write(Address(obj)+0xa8,4,duration*60u);
  write(0x41b4f0,1,Raw(mem.load(0x41b4f0,1))+1);
}
void Model::hammer_spell_ctor(Raw spell,Raw level,Raw owner,Raw skill,Raw hammer) {
  spell_base(spell,level,owner,skill,0x51a3b8); write(Address(spell)+0x18,4,0);
  if (hammer) call("handle_link",{spell+0x18,hammer,hammer});
}
void Model::hammer_spell_run(Raw spell) { if (!mem.load(Address(spell)+0x18,4)) spell_delete(spell); }
void Model::hammer_pull(Raw obj,const Vec3 &anchor,Raw slack) {
  const auto goal=vec(Address(obj)+0xd8); Vec3 d;
  for (unsigned i=0;i<3;++i) d[i]=sub(goal[i],anchor[i]);
  const auto length=sqrt(square_length(d));
  if (lt(slack,length)) {
    const auto f=sub(ONE,div(slack,length));
    for (int i=2;i>=0;--i) write(Address(obj)+0xd8+4*i,4,add(goal[i],mul(sub(anchor[i],goal[i]),f)));
  }
}
void Model::hammer_blow(Raw obj,Raw victim,Raw rating) {
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto amount=from_int(scalar(u(Address(obj)+0xc4),10,{0,u(Address(obj)+0xa4)}));
  const auto high=add(amount,mul(mul(amount,mul(from_int(obj_rand(obj)),0x37800000)),0x3e4ccccc));
  call("damage_apply",{victim,u(Address(u(Address(obj)+0x4c))+0xc),rating,0,0,0x101,obj+0x58,0,amount,high});
  const auto sound=named_scalar("res_find",{0x550970,0x5509a8}); call("sound_playOnObject",{sound,obj,ONE});
}
Raw Model::seed3_draw() { const auto seed=kernel::lcg_step(Raw(mem.load(0x404468,4))); write(0x404468,4,seed); return kernel::lcg_result(seed); }
void Model::hammer_run(Raw obj) {
  const auto u=[&](Address a) { return Raw(mem.load(a,4)); };
  const auto w=[&](unsigned off,Raw v) { write(Address(obj)+off,4,v); };
  const auto at=[&](unsigned off) { return u(Address(obj)+off); };
  const auto cut=[&]() { return mem.load(0x44fcd0,1)!=0; };
  const auto target_at=[&](Raw off) { return u(Address(at(off))+0xc); };
  const auto above=[&](Raw who,Raw z) { return Vec3{add(u(Address(who)+0x10),ZERO),u(Address(who)+0x14),add(u(Address(who)+0x18),z)}; };
  const auto query=[&](const Vec3 &p,const Vec3 &reach) {
    Values a; for (unsigned i=0;i<3;++i) a.push_back(sub(p[i],reach[i]));
    for (unsigned i=0;i<3;++i) a.push_back(add(p[i],reach[i]));
    a.push_back(0x80);
    auto found=call("world_collectObjectsInBox",a); if (found.size()>65536) throw Unsupported("Hammer query count limit"); return found;
  };
  Raw roll=0x46800000,pitch=0xc61c4000,turn_k=0x399d4951,turn_damp=0x3c23d70a,pos_k=0x3b449ba5,pos_damp=0x3d4ccccc;
  Raw aim=Raw(s16(Raw(mem.load(Address(obj)+0x38,2))));
  if (!at(0x4c)) { delete_self(obj); return; }
  auto owner=target_at(0x4c); auto anchor=above(owner,0x42700000); Vec3 d;
  for (unsigned i=0;i<3;++i) d[i]=sub(anchor[i],at(0x10+4*i));
  if (lt(0x48afc800,square_length(d))) w(0xc8,0);
  if (cut()) w(0xc8,0);
  bool blocked=false;
  if ((u(Address(owner)+0xc)&0x40) && u(Address(owner)+0x58c)==8) { w(0xc8,0); blocked=true; }
  auto state=at(0xc8);
  if (state==0) {
    const auto v=vec(Address(obj)+0xcc); if (lt(ONE,square_length(v))) aim=0u-Raw(atan2_16(v[0],v[1]));
    anchor=vec(Address(obj)+0x10);
    if (at(0x4c)) { owner=target_at(0x4c); anchor=above(owner,0x42700000); hammer_pull(obj,anchor,0x42700000); }
    const auto found=query(anchor,{0x43340000,0x43340000,0x44960000});
    Raw best=0,best_d=0x46fd2000;
    if (!cut() && !blocked) for (auto other:found) {
      Vec3 way; for (unsigned i=0;i<3;++i) way[i]=sub(u(Address(other)+0x10+4*i),at(0x10+4*i));
      const auto p=vec(Address(obj)+0x10);
      const auto t=named_scalar("terrain_raycast",{u(0x44fdc8),p[0],p[1],p[2],way[0],way[1],way[2],0,ONE});
      if (le(t,ZERO) && at(0x4c)) {
        owner=target_at(0x4c); auto enemy=vcall(owner,20,{other});
        if (enemy.size()!=1) throw Unsupported("Hammer enemy scalar required");
        if (enemy[0]) {
          Vec3 e; for (unsigned i=0;i<3;++i) e[i]=sub(anchor[i],u(Address(other)+0x10+4*i));
          const auto dist=square_length(e); if (lt(dist,best_d)) { best=other; best_d=dist; }
        }
      }
    }
    if (best) {
      call("handle_link",{obj+0x50,best,best}); state=3;
      if (!cut()) { const auto r=obj_rand(obj); const auto chance=scalar(at(0xc4),10,{2,at(0xa4)});
        if (lt(mul(mul(from_int(r),0x37800000),0x42c80000),from_int(chance))) state=4; }
      w(0xa0,0); w(0xc8,state);
    }
  } else if (state==3) {
    const auto target=target_at(0x50);
    if (!at(0x50) || (u(Address(target)+0xc)&8)) w(0xc8,0);
    else {
      turn_k=0x3dcccccc; turn_damp=0x3f199999;
      aim=0u-Raw(atan2_16(sub(u(Address(target)+0x10),at(0x10)),sub(u(Address(target)+0x14),at(0x14))));
      hammer_pull(obj,above(target,0x42700000),0x41c00000); pos_k=0x3bc49ba5; pos_damp=0x3e199999;
      const auto count=at(0xa0)+1; w(0xa0,count);
      if (s32(count)>0x1e) pitch=0xc69c4000;
      if (s32(count)>0x29) pitch=ZERO;
      if (count==0x2f) hammer_blow(obj,target,300000);
      if (s32(at(0xa0))>=0x3d) { w(0xa0,0); if (u(PVP)) w(0xc8,0); }
    }
  } else if (state==4) {
    pitch=ZERO; const auto count=at(0xa0)+1; w(0xa0,count); w(0xb8,0xc55ac000);
    pos_k=0x3c23d70a; pos_damp=0x3e4ccccc; roll=ZERO;
    if (s32(count)>=0x3d && std::int64_t(s32(count))%20==0) {
      const auto found=query(vec(Address(obj)+0x10),{0x42900000,0x42900000,0x44960000});
      Raw victim=0,best_d=0x45a20000;
      for (auto other:found) {
        const auto flags=u(Address(other)+0xc); if (!(flags&0x20) || (flags&0x48)) continue;
        Vec3 e; for (unsigned i=0;i<3;++i) e[i]=sub(at(0x10+4*i),u(Address(other)+0x10+4*i));
        const auto dist=square_length(e); if (lt(dist,best_d)) { victim=other; best_d=dist; }
      }
      if (victim) hammer_blow(obj,victim,3000);
    }
    bool ended=false;
    if (!at(0x50)) {
      owner=target_at(0x4c); const auto base=above(owner,0x42100000); const auto turn=Raw(s16(at(0xa0)<<9));
      write_vec(Address(obj)+0xd8,{add(base[0],mul(cos16(turn),0x42100000)),add(base[1],mul(sin16(turn),0x42100000)),base[2]});
    } else {
      const auto target=target_at(0x50);
      if (u(Address(target)+0xc)&8) { w(0xc8,0); ended=true; }
      else {
        turn_k=0x3dcccccc; turn_damp=0x3f199999;
        aim=0u-Raw(atan2_16(sub(u(Address(target)+0x10),at(0x10)),sub(u(Address(target)+0x14),at(0x14))));
        hammer_pull(obj,above(target,0x42700000),0x41c00000);
      }
    }
    if (!ended && s32(at(0xa0)<<9)>0x10000) w(0xc8,0);
  } else if (state==1 || state==2) throw Unsupported("Hammer states 1/2 absent from reference");
  const auto yaw=at(0xac);
  auto spin=add(at(0xc0),mul(sub(roll,at(0xb4)),0x3d4ccccc));
  auto tilt=add(at(0xbc),mul(sub(pitch,at(0xb0)),0x3d4ccccc));
  auto swing=add(at(0xb8),mul(from_int(Raw(s16(aim-Raw(to_int(yaw))))),turn_k));
  spin=sub(spin,mul(spin,0x3e4ccccc)); tilt=sub(tilt,mul(tilt,0x3e4ccccc)); swing=sub(swing,mul(swing,turn_damp));
  w(0xc0,spin); w(0xbc,tilt); w(0xb4,add(at(0xb4),spin)); w(0xb0,add(at(0xb0),tilt)); w(0xb8,swing); w(0xac,add(yaw,swing));
  const auto frame=u(FRAME);
  w(0xbc,add(at(0xbc),mul(sin16(Raw(s16(frame<<10))),0x41a00000)));
  w(0xc0,add(at(0xc0),mul(sin16(Raw(s16(frame*800u))),0x41a00000)));
  const Raw factors[]={0x193,0x166,0x14c};
  for (unsigned i=0;i<3;++i) w(0xcc+4*i,add(at(0xcc+4*i),mul(sin16(Raw(s16(frame*factors[i]))),0x3c23d70a)));
  Vec3 pull; for (unsigned i=0;i<3;++i) pull[i]=mul(sub(at(0xd8+4*i),at(0x10+4*i)),pos_k);
  for (unsigned i=0;i<3;++i) w(0xcc+4*i,add(at(0xcc+4*i),pull[i]));
  for (unsigned i=0;i<3;++i) w(0x10+4*i,add(at(0x10+4*i),at(0xcc+4*i)));
  for (unsigned i=0;i<3;++i) { const auto v=at(0xcc+4*i); w(0xcc+4*i,sub(v,mul(v,pos_damp))); }
  call("obj_updateGridCell",{obj});
  const auto model=u(0x3e9ce0u+item_resource_index(obj+0x58)*0x10u+8u);
  Mat34 base; for (unsigned i=0;i<12;++i) base[i]=u(0x5509c0+4*i);
  const auto turned=mat34_set_pos_euler_scale(ONE,vec(Address(obj)+0x10),Raw(s16(Raw(to_int(at(0xac))))),Raw(s16(Raw(to_int(at(0xb0))))),Raw(s16(Raw(to_int(at(0xb4))))));
  const auto matrix=mat34_mul(base,turned); const auto points=named_scalar("model_getSamplePointCount",{model});
  auto left=mul(from_int(at(0xa8)),0x3c088888); if (lt(ONE,left)) left=ONE;
  std::int64_t count=2; Raw speed=ZERO;
  if (lt(left,ONE)) { count=std::int64_t(to_int(mul(sub(ONE,left),0x42c80000)))+2; speed=mul(sub(ONE,left),0x42480000);
    if (at(0xa8)==1) { count=3000; speed=0x42c80000; } }
  if (count>65536) throw Unsupported("Hammer particle count limit");
  for (std::int64_t n=0;n<count;++n) {
    const auto r=seed3_draw(); const auto divisor=s32(points); const Raw index=Raw(divisor ? std::int64_t(r)%divisor : r);
    Values args{model}; args.insert(args.end(),matrix.begin(),matrix.end()); args.push_back(index);
    const auto sample=call("model_transformSamplePoint",args); if (sample.size()!=6) throw Unsupported("Hammer sample needs point and size vectors");
    const auto scale=mul(mul(sample[3],0x3727c5ac),0x420d6666); Vec3 point,b;
    for (unsigned i=0;i<3;++i) point[i]=add(sample[i],mul(sub(mul(from_int(seed3_draw()),0x37800000),HALF),scale));
    for (std::size_t attempts=0;;++attempts) {
      if (attempts>=65536) throw Unsupported("Hammer particle rejection limit");
      for (auto &lane:b) { const auto m=mul(from_int(seed3_draw()),0x37800000); lane=sub(add(m,m),ONE); }
      if (!lt(ONE,square_length(b))) break;
    }
    call("particle_spawn",{0x8e62c0,point[0],point[1],point[2],mul(b[0],speed),mul(b[1],speed),mul(b[2],speed),ZERO});
  }
  w(0xa8,at(0xa8)-1); if (s32(at(0xa8))<1) delete_self(obj);
}

// Imported Summon Skeleton contract from the pinned ref_arena_monsters.py.
Raw Model::summon_can_use(Raw, Raw caster) {
  Raw name;
  if (mem.load(Address(caster)+0xc,4)&0x40) {
    const auto slot=Raw(mem.load(Address(caster)+0x3c8,4));
    if (slot>=4) return 0;
    name=Raw(mem.load(0x503778+Address(slot)*4,4));
  } else name=Raw(mem.load(0x503788,4));
  return named_scalar("world_findObjectByName",{name}) ? 0 : 1;
}
Vec3 Model::summon_sample(Raw caster, Raw count) {
  const auto result=vcall(caster,16,{});
  if (result.size()!=1) throw Unsupported("Summon model scalar required");
  // abs(INT_MIN) is valid in the Python contract; compute in unsigned lanes.
  const auto magnitude=s32(count)<0 ? 0u-count : count;
  const auto draw=cosmetic_draw();
  if (!magnitude) throw Unsupported("Summon sample count zero");
  const auto p=vec(Address(caster)+0x10);
  const auto point=call("model_getSamplePoint",{result[0],caster+0x114,p[0],p[1],p[2],
    Raw(s16(Raw(mem.load(Address(caster)+0x38,2)))),draw%magnitude,0,0,ONE});
  if (point.size()!=3) throw Unsupported("Summon sample point required");
  return {point[0],point[1],point[2]};
}
void Model::skelight_ctor(Raw obj,const Vec3 &pos,Raw facing,Raw props,Raw level) {
  call("Skelight_ctor",{obj,pos[0],pos[1],pos[2],facing,props,level});
  ++hidden_draws; // Only after the constructor service returns successfully.
}
void Model::summon_spell_ctor(Raw spell,Raw level,Raw caster,Raw skill,Raw skeleton) {
  // Imported base constructor has a different write order from spell_base.
  write(spell,1,level); write(Address(spell)+0x14,4,0x515ba0);
  write(Address(spell)+0xc,4,caster); write(Address(spell)+4,4,0);
  write(Address(spell)+8,4,skill);
  const auto seed=draw(); write(Address(spell)+0x10,4,seed);
  write(Address(spell)+0x14,4,0x519918); write(Address(spell)+0x18,4,0);
  if (skeleton) call("handle_link",{spell+0x18,skeleton,skeleton});
}
void Model::summon_spell_run(Raw spell) {
  if (!mem.load(Address(spell)+0x18,4) && spell) spell_call(spell,1,{3});
}
Raw Model::summon_cast(Raw skill,Raw level,Raw caster,const Vec3 &pos,Raw facing) {
  facing=Raw(s16(facing));
  write(Address(caster)+0x9a,2,0xff92); write(Address(caster)+0x9c,2,0xff80);
  write(Address(caster)+0x9e,2,30); write(Address(caster)+0x98,2,0xff80);
  const auto model=vcall(caster,16,{});
  if (model.size()!=1) throw Unsupported("Summon model scalar required");
  const auto count=named_scalar("model_getSamplePointCount",{model[0]});
  if (count) {
    for (unsigned i=0;i<232;++i) {
      auto point=summon_sample(caster,count);
      if (i<32) {
        const auto ball=cosmetic_ball();
        for (unsigned j=0;j<3;++j) point[j]=add(point[j],mul(ball[j],0x3e72af9a));
      }
      const auto ball=cosmetic_ball();
      const auto k=i<32 ? 0x3d8b4634u : 0x41c000e5u;
      call("particle_spawn",{i<32 ? 0x8e5c80u : 0x8e5cd0u,point[0],point[1],point[2],
        mul(ball[0],k),mul(ball[1],k),mul(ball[2],k),ZERO});
    }
  }
  const auto sx=add(pos[0],mul(cos16(facing),0x42900000));
  const auto sy=add(pos[1],mul(sin16(facing),0x42900000));
  const auto sz=named_scalar("terrain_height",{Raw(mem.load(0x44fdc8,4)),sx,sy,pos[2],0x41400000});
  Vec3 spot{sx,sy,sz};
  // The reference compares this rounded float as an IEEE double, independent
  // of the emulated compare mode. Magnitude ordering also rejects IEEE NaNs.
  if (caster) {
    const auto magnitude=sub(sz,Raw(mem.load(Address(caster)+0x18,4)))&0x7fffffffu;
    if (magnitude>0x41c00000 && magnitude<=0x7f800000) {
      const auto p=vec(Address(caster)+0x10);
      const auto found=call("world_findFreeSpot",{p[0],p[1],p[2],72,12,0});
      if (found.size()!=4) throw Unsupported("Summon free spot and signed-byte flag required");
      if (found[3]&0xff) spot={found[0],found[1],found[2]};
    }
  }
  const auto d1=frac(draw());
  const auto strength=scalar(skill,12,{0,level,caster,10});
  Raw props=0x503770;
  if (mem.load(Address(caster)+0xc,4)&0x40) {
    const auto slot=Raw(mem.load(Address(caster)+0x3c8,4));
    props=slot<4 ? 0x503750+8*slot : 0;
  }
  const auto skeleton=named_scalar("pool_new",{0x468});
  if (skeleton) {
    skelight_ctor(skeleton,spot,facing,props,strength);
    write(Address(skeleton)+0xc,4,Raw(mem.load(Address(skeleton)+0xc,4))|0x4000);
    const auto chance=scalar(skill,12,{1,level,caster,10});
    if (lt(d1,mul(from_int(chance),0x3c23d70a))) {
      const auto pick=draw()%3;
      const auto holder=Raw(mem.load(Address(skeleton)+0x50,4));
      if (holder) table_call("Holder",holder,0,2+2*pick,{0});
    }
    if ((mem.load(Address(caster)+0xc,4)&0x40) && s32(level)>0) {
      write(Address(skeleton)+0x5a,1,Raw(mem.load(Address(caster)+0x3c8,1)));
      const auto flags=mem.load(Address(skeleton)+0xa8,8)|0x400000;
      if (writes.size()>=creature_hp::MAX_HISTORY) throw Unsupported("Skill write history limit");
      writes.push_back({skeleton+0xa8,8,flags}); mem.store(Address(skeleton)+0xa8,8,flags);
      const auto team=vcall(caster,37,{});
      if (team.size()!=1) throw Unsupported("Summon team scalar required");
      call("Creature_setCharm",{skeleton,UINT32_MAX,team[0]});
      write(Address(skeleton)+0x3ac,2,400);
      vcall(skeleton,34,{500});
    }
  }
  const auto sound=named_scalar("res_find",{0x54fdd0,0x54fdd8});
  call("sound_playAtPos",{sound,pos[0],pos[1],pos[2],ONE});
  if (!skeleton) return 0;
  const auto spell=named_scalar("pool_new",{0x1c});
  if (spell) summon_spell_ctor(spell,level,caster,skill,skeleton);
  return spell;
}

} // namespace champions::simulation::rta::skills_a
