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
  const std::array<std::pair<std::uint32_t, Operation>, 9> protection_bodies{{
      {0x3142bc,Operation::RetributionCanUse},{0x3142f0,Operation::RetributionCast},
      {0x314454,Operation::ManaCost},{0x314e1c,Operation::SpiritShieldCanUse},
      {0x314e50,Operation::SpiritShieldCast},{0x314fd8,Operation::ManaCost},
      {0x317afc,Operation::FrostArmorCanUse},{0x317b30,Operation::FrostArmorCast},
      {0x317ce8,Operation::ManaCost}}};
  for (const auto &entry : protection_bodies) {
    if (entry.first == body && !external.count(slot)) {
      args.insert(args.begin(), adjusted);
      return execute(entry.second, args);
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
void Model::write(Address a, unsigned width, std::uint64_t value) {
  if (writes.size() >= 65536) throw Unsupported("write history limit");
  mem.load(a, width);
  const auto masked = width == 1 ? value & 0xffu : width == 2 ? value & 0xffffu : width == 4 ? value & 0xffffffffu : value;
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
std::vector<std::uint32_t> Model::service(unsigned id, std::vector<std::uint32_t> args) {
  if (calls.size() >= 65536) throw Unsupported("call history limit");
  calls.push_back({id, args});
  const auto found = callbacks.find(id);
  if (found == callbacks.end()) throw Unsupported("missing protection service handler");
  auto callback = found->second;
  return callback(*this, args);
}
std::uint32_t Model::service_scalar(unsigned id, std::vector<std::uint32_t> args) {
  const auto result = service(id, std::move(args));
  if (result.size() != 1) throw Unsupported("scalar service result required");
  return result[0];
}
std::vector<std::uint32_t> Model::vec(Address a) const {
  return {u32(a),u32(a+4),u32(a+8)};
}
std::uint32_t Model::level(Address s) const {
  const auto byte = static_cast<std::uint32_t>(mem.load(s,1));
  return byte < 128 ? byte : byte | 0xffffff00u;
}
std::uint32_t Model::record(Address obj) const {
  return 0x4d8af0u + u32(obj+0x3c8)*0x4c30u;
}
void Model::flash(Address obj, unsigned quad, const std::vector<std::uint32_t> &colour) {
  const Address base = obj + (quad == 1 ? 0x90 : 0x98);
  for (unsigned k=0;k<4;++k) write(base+2*k,2,colour[k]);
}
std::vector<std::uint32_t> Model::sound(std::uint32_t lump, std::uint32_t name, const std::vector<std::uint32_t> &at) {
  const auto entry = service_scalar(ResourceFind,{lump,name});
  return service(SoundPlayAtPosition,{entry,at[0],at[1],at[2],ONE});
}
void Model::destroy(Address spell) {
  const auto table = u32(spell+0x14);
  service(SpellDestroyCallback,{static_cast<std::uint32_t>(spell+half(Address(table)+8)),3});
}
std::uint32_t Model::creature_model(Address obj) {
  const auto table = u32(obj+0x48);
  // Slot 16 has no inherited base body in the pinned reference.
  return service_scalar(CreatureModel,{static_cast<std::uint32_t>(obj+half(Address(table)+128))});
}
std::vector<std::uint32_t> Model::sample(Address owner, std::uint32_t model,
    const std::vector<std::uint32_t> &at, std::uint32_t index) {
  auto result = service(SamplePoint,{model,static_cast<std::uint32_t>(owner+0x114),
      at[0],at[1],at[2],static_cast<std::uint32_t>(half(owner+0x38)),index,0,0,u32(owner+0xdc)});
  if (result.size()!=3) throw Unsupported("sample point result must be three raw floats");
  return result;
}
void Model::protection_ctor(Operation op, Address spell, std::uint32_t rank, Address caster, Address skill) {
  const bool ret = op==Operation::RetributionCtor, shield = op==Operation::SpiritShieldCtor;
  execute(Operation::SpellBase,{static_cast<std::uint32_t>(spell),rank,
      static_cast<std::uint32_t>(caster),static_cast<std::uint32_t>(skill),
      ret ? 0x51bf70u : shield ? 0x51c1c0u : 0x51c3e8u});
  if (ret) {
    const Address light=spell+0x18;
    const std::vector<std::uint32_t> place{add(u32(caster+0x10),ZERO),u32(caster+0x14),add(u32(caster+0x18),0x41a00000)};
    write(light+9,1,0);
    write(light+0x30,8,std::uint64_t(place[0]) | (std::uint64_t(place[1])<<32));
    write(light+0x38,4,place[2]);
    for (const auto &v : std::array<std::pair<unsigned,Raw>,5>{{
        {0x24,0x41400000},{0x28,0x3be38e38},{0x2c,ZERO},{0x1c,0x41400000},{0x20,0x3be38e38}}})
      write(light+v.first,4,v.second);
    for (const auto &v : std::array<std::pair<unsigned,unsigned>,8>{{
        {0x12,0x5f},{0x14,0x77},{0xa,0x5f},{0xc,0x77},{0xe,0x62},{0x10,0},{0x16,0x62},{0x18,0}}})
      write(light+v.first,2,v.second);
    write(light+8,1,0);
    service(LightLink,{static_cast<std::uint32_t>(light)});
    sound(0x552bb8,0x552bc0,vec(Address(u32(spell+0xc))+0x10));
    write(spell+0x54,4,service_scalar(ParticleGroupCreate,{}));
    return;
  }
  if (shield) write(spell+0x1c,4,0);
  write(spell+0x18,4,0);
  const auto block=service_scalar(PoolNew,{0xbc});
  if (block) {
    const auto at=execute(Operation::VisualPosition,{u32(spell+0xc)});
    std::vector<std::uint32_t> args{block,add(at[0],ZERO),at[1],add(at[2],0x42100000),u32(spell+0xc),0};
    const std::array<Raw,4> colour=shield ? std::array<Raw,4>{0x42a20000,0x424c0000,0x42b40000,0x42e40000}
        : std::array<Raw,4>{0x43400000,0x43000000,0x43800000,0x42080000};
    args.insert(args.end(),colour.begin(),colour.end());
    args.insert(args.end(),{0,0,0,0,0x42f00000,0x43340000,ZERO,ONE});
    service(LightEffectConstructor,std::move(args));
  }
  sound(shield ? 0x552d78 : 0x553020,shield ? 0x552d80 : 0x553028,vec(Address(u32(spell+0xc))+0x10));
}
void Model::shield_particles(Address spell, Raw progress, bool dissolve) {
  auto owner=u32(spell+0xc);
  auto ramp=add(progress,progress);
  if (lt(HALF,progress)) {
    ramp=sub(TWO,ramp);
    const auto flags=u32(Address(owner)+0xc);
    write(Address(owner)+0xc,4,dissolve ? flags|2u : flags&~2u);
  }
  const auto model=creature_model(owner);
  const auto points=s32(service_scalar(SamplePointCount,{model}));
  const auto at=execute(Operation::VisualPosition,{u32(spell+0xc)});
  const auto count=to_int(mul(ramp,dissolve ? 0x42180000 : 0x42940000));
  const Raw push=dissolve ? 0x416c4d85 : 0x40cdfbbf, speed=dissolve ? 0x413ff3b5 : 0x407de33e;
  for (std::int32_t i=0;i<count;++i) {
    const auto roll=own_draw(spell+0x10);
    if (!points) throw Unsupported("reference sample-point remainder divides by zero");
    const auto index=static_cast<std::uint32_t>(std::int64_t(roll)%points);
    const auto point=sample(u32(spell+0xc),model,at,index);
    const auto b=ball(spell+0x10);
    service(ParticleSpawn,{0x8e8648,sub(point[0],mul(b[0],push)),sub(point[1],mul(b[1],push)),
        sub(point[2],mul(b[2],push)),mul(b[0],speed),mul(b[1],speed),mul(b[2],speed),ZERO});
  }
}

std::vector<std::uint32_t> Model::execute(Operation op,
                                          const std::vector<std::uint32_t> &a) {
  const auto n = static_cast<unsigned>(op);
  constexpr std::array<unsigned, 54> sizes{
      {3, 4, 5, 5, 3, 3, 5, 2, 5, 2, 2, 4, 4, 4, 2, 2, 2, 2, 5, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 2,5,4,5,2, 2,5,4,3,3,1, 2,5,4,3,3, 2,1,1,6,18,6,5,1}};
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
  case Operation::FindSpell: {
    const auto token=service_scalar(TypeInfo,{a[1]});
    return {service_scalar(FindSpellByType,{a[0],token})};
  }
  case Operation::IsPlayer: return {s && (u32(s+0xc)&0x40) ? 1u : 0u};
  case Operation::PlayerRecord: return {record(s)};
  case Operation::Flash:
    flash(s,a[1],{a[2],a[3],a[4],a[5]}); return {};
  case Operation::LightEffect:
    service(LightEffectConstructor,a); return {};
  case Operation::ModelSample: return sample(s,a[1],{a[2],a[3],a[4]},a[5]);
  case Operation::SoundAt:
    return sound(a[0],a[1],{a[2],a[3],a[4]});
  case Operation::SpellDestroy: destroy(s); return {};
  case Operation::RetributionCanUse:
  case Operation::SpiritShieldCanUse:
  case Operation::FrostArmorCanUse: {
    const auto getter=op==Operation::RetributionCanUse ? 0x3d18a0u :
        op==Operation::SpiritShieldCanUse ? 0x3d19f0u : 0x3d1d70u;
    return {execute(Operation::FindSpell,{a[1],getter})[0] ? 0u : 1u};
  }
  case Operation::RetributionCast:
  case Operation::SpiritShieldCast:
  case Operation::FrostArmorCast: {
    const bool ret=op==Operation::RetributionCast, shield=op==Operation::SpiritShieldCast;
    const auto rank=scalar(s,14,{a[1],ret ? 0x14u : shield ? 2u : 0xeu});
    const auto spell=service_scalar(PoolNew,{ret ? 0x5cu : shield ? 0x24u : 0x1cu});
    if (spell) protection_ctor(ret ? Operation::RetributionCtor : shield ? Operation::SpiritShieldCtor :
        Operation::FrostArmorCtor,spell,rank,a[2],s);
    return {spell};
  }
  case Operation::RetributionCtor:
  case Operation::SpiritShieldCtor:
  case Operation::FrostArmorCtor:
    protection_ctor(op,s,a[1],a[2],a[3]); return {};
  case Operation::RetributionRun: {
    const Address light=s+0x18;
    service(ParticleGroupBegin,{u32(s+0x54)});
    service(EmitFromModelVertices,{u32(s+0xc),0x31,0x8e85f8,0,0,0x41100000});
    service(ParticleGroupEnd,{u32(s+0x54)});
    const auto phase=s32(u32(0x44fdcc)*0xb5eu)%0x10000;
    const auto radius=add(mul(sin16(static_cast<std::uint32_t>(phase)),0x41000000),0x42960000);
    write(light+0x1c,4,radius); write(light+0x24,4,radius);
    const auto falloff=div(ONE,mul(radius,radius));
    write(light+0x28,4,falloff); write(light+0x20,4,falloff);
    write(light+0x30,8,std::uint64_t(a[1]) | (std::uint64_t(a[2])<<32));
    write(light+0x38,4,a[3]);
    service(LightRelink,{static_cast<std::uint32_t>(light)});
    const auto clock=add(u32(s+0x58),dt); write(s+0x58,4,clock);
    const auto seconds=scalar(u32(s+8),10,{0,level(s)});
    if (lt(from_int(seconds),clock)) destroy(s);
    return {};
  }
  case Operation::RetributionHeal: {
    auto owner=u32(s+0xc);
    for (const auto &v : std::array<std::pair<unsigned,unsigned>,7>{{
        {0x96,0x3f},{0x90,0x46},{0x92,0x80},{0x94,0x4b},{0x9a,0x80},{0x9c,0x4b},{0x9e,0x3f}}})
      write(Address(owner)+v.first,2,v.second);
    const auto fraction=mul(from_int(scalar(u32(s+8),10,{1,level(s)})),percent);
    owner=u32(s+0xc);
    if (u32(Address(owner)+0xc)&0x40) {
      const Address rec=record(owner);
      auto hp=u32(rec+0xac);
      if (!lt(ZERO,hp)) return {ZERO};
      hp=add(hp,mul(a[1],fraction)); write(rec+0xac,4,hp);
      if (lt(u32(rec+0xb0),hp)) write(rec+0xac,4,u32(rec+0xb0));
    }
    return {fraction};
  }
  case Operation::SpiritShieldRun: {
    const auto owner=u32(s+0xc), clock=add(u32(s+0x1c),dt), phase=u32(s+0x18);
    flash(owner,1,{0x1b,0x11,0x1e,0x26}); write(s+0x1c,4,clock);
    if (phase==0) {
      const auto seconds=scalar(u32(s+8),12,{0,level(s),owner,10});
      if (lt(from_int(seconds),u32(s+0x1c))) destroy(s);
    } else if (phase==1 || phase==3) {
      const auto progress=sub(ONE,mul(sub(u32(s+0x20),clock),phase==1 ? 0x40a6aae4 : 0x4001d177));
      shield_particles(s,lt(ONE,progress) ? ONE : progress,phase==1);
      if (le(u32(s+0x20),u32(s+0x1c))) {
        write(s+0x18,4,phase==1 ? 2 : 0);
        if (phase==1) write(s+0x20,4,add(u32(s+0x1c),0x3d83126e));
      }
    } else if (phase==2 && le(u32(s+0x20),clock)) {
      write(s+0x18,4,3); write(s+0x20,4,add(clock,0x3efc6a1a));
    }
    return {};
  }
  case Operation::SpiritShieldParticles:
    if (a[2]>1) throw Unsupported("dissolve argument must be boolean");
    shield_particles(s,a[1],a[2]!=0); return {};
  case Operation::SpiritShieldAbsorb: {
    const auto skill=u32(s+8), rank=level(s), owner=u32(s+0xc);
    const auto seconds=scalar(skill,12,{0,rank,owner,10});
    const auto chance=s32(scalar(skill,12,{1,rank,u32(s+0xc),10}));
    if (!le(u32(s+0x1c),from_int(seconds))) return {0};
    if (static_cast<std::int32_t>(draw()%100)>=chance) return {0};
    if (!u32(s+0x18)) {
      write(s+0x18,4,1); write(s+0x20,4,add(u32(s+0x1c),0x3e449b62));
    }
    sound(0x552d78,0x552da0,vec(Address(u32(s+0xc))+0x10));
    return {1};
  }
  case Operation::FrostArmorRun: {
    const auto clock=add(u32(s+0x18),dt); write(s+0x18,4,clock);
    const auto seconds=scalar(u32(s+8),12,{0,level(s),u32(s+0xc),0x14});
    if (lt(from_int(seconds),u32(s+0x18))) { destroy(s); return {}; }
    const auto owner=u32(s+0xc);
    flash(owner,1,{0x40,0x40,0x80,0x11}); flash(owner,2,{0x40,0x40,0x80,0x11});
    const auto model=creature_model(owner);
    const auto points=s32(service_scalar(SamplePointCount,{model}));
    const auto at=execute(Operation::VisualPosition,{u32(s+0xc)});
    for (unsigned i=0;i<11;++i) {
      const auto b=ball(s+0x10);
      const Raw speed=i<10 ? 0x409a4484 : 0x40800000;
      const std::vector<Raw> velocity{mul(b[0],speed),mul(b[1],speed),mul(b[2],speed)};
      const auto roll=own_draw(s+0x10);
      if (!points) throw Unsupported("reference sample-point remainder divides by zero");
      const auto index=static_cast<std::uint32_t>(std::int64_t(roll)%points);
      const auto point=sample(u32(s+0xc),model,at,index);
      service(ParticleSpawn,{i<10 ? 0x8e8788u : 0x8e87d8u,point[0],point[1],point[2],
          velocity[0],velocity[1],velocity[2],ZERO});
    }
    return {};
  }
  case Operation::FrostArmorHit: {
    const auto skill=u32(s+8),rank=level(s);
    auto factor=sub(ONE,mul(from_int(scalar(skill,12,{1,rank,u32(s+0xc),0})),percent));
    if (lt(factor,ZERO)) factor=ZERO;
    if (a[1]) {
      const auto roll=draw();
      const auto chance=s32(scalar(u32(s+8),12,{2,level(s),u32(s+0xc),0}));
      if (static_cast<std::int32_t>(roll%100)<chance) {
        const auto amount=static_cast<std::uint32_t>(to_int(sub(a[2],factor)))&0xffff;
        std::vector<std::uint32_t> args{a[1],u32(s+0xc),100000,0,0,0x540};
        // Canonical desc header: sixteen zero bytes followed by two 0xff bytes.
        args.insert(args.end(),16,0); args.insert(args.end(),{255,255,0x16,amount,amount,0});
        for (unsigned k=0;k<3;++k) args.insert(args.end(),{0xffff,0,0,0});
        args.insert(args.end(),{0,ZERO,ZERO});
        service(DamageApply,std::move(args));
        sound(0x553020,0x553040,vec(Address(a[1])+0x10));
        const auto point=vec(Address(a[1])+0x10);
        service(EffectSpawn,{10,point[0],point[1],point[2]});
      }
    }
    return {factor};
  }
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
  throw Unsupported("id-based active-skill dispatch is not ported; use explicit operations");
}
} // namespace champions::simulation::rta::skills_c
