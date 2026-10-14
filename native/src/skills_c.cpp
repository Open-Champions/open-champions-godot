// Authored from ref_skills_c.py at 356add27d50e50972b2a671b31e8f4ec93e6778c.
// SHA256 9e673475a0b89292e483075d76b2f277d1251ce820a0956053e68c83faa81e91.
#include "champions/skills_c.hpp"
#include <array>
#include "champions/core_math.hpp"

namespace champions::simulation::rta::skills_c {
namespace {
struct SummonConfig {
  Raw table, adjust, lump, sound, particle, count, outward, upward, speed;
  std::array<Raw,4> flash, light;
};
constexpr SummonConfig summon_axe{0x51b050,10,0x5514e8,0x5514f0,0x8e6d30,
    0x42480000,0x41807cfc,0x41bde66d,0x413e8aeb,
    {0x40,0x40,0x80,0x66},{0x42800000,0x42800000,0x43000000,0x42cc0000}};
constexpr SummonConfig summon_arrows{0x51b218,20,0x5516f8,0x551700,0x8e6d80,
    0x41700000,0x41851c19,0x41b857c1,0x411b8d73,
    {0x40,0x80,0x40,0x80},{0x42800000,0x43000000,0x42800000,0x43000000}};
std::int32_t signed_byte(Raw v) {return (v & 255u)<128u ? int(v & 255u) : int(v & 255u)-256;}
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
  const std::array<std::pair<std::uint32_t, Operation>, 33> protection_bodies{{
      {0x3142bc,Operation::RetributionCanUse},{0x3142f0,Operation::RetributionCast},
      {0x314454,Operation::ManaCost},{0x314e1c,Operation::SpiritShieldCanUse},
      {0x314e50,Operation::SpiritShieldCast},{0x314fd8,Operation::ManaCost},
      {0x317afc,Operation::FrostArmorCanUse},{0x317b30,Operation::FrostArmorCast},
      {0x317ce8,Operation::ManaCost},{0x30300c,Operation::BloodLustCanUse},
      {0x303040,Operation::BloodLustCast},{0x303220,Operation::ManaCost},
      {0x2ddb90,Operation::RiposteCast},{0x2e8a88,Operation::DodgeManaCost},
      {0x2e8cf8,Operation::HealingCast},{0x2e8e94,Operation::ManaCost},
      {0x2fe644,Operation::SummonAxeCast},{0x2fe938,Operation::ManaCost},
      {0x2ff5ec,Operation::SummonArrowsCanUse},{0x2ff5f4,Operation::SummonArrowsCast},
      {0x2ff848,Operation::ManaCost},{0x2e7224,Operation::CriticalHitCanUse},
      {0x2e74b0,Operation::CriticalHitCast},{0x2e7690,Operation::CriticalHitManaCost},
      {0x319710,Operation::FireAxeCanUse},{0x31979c,Operation::FireAxeCast},{0x319ea0,Operation::ManaCost},
      {0x31a04c,Operation::ColdAxeCanUse},{0x31a0d8,Operation::ColdAxeCast},{0x31a7d4,Operation::ManaCost},
      {0x301464,Operation::BattleCryCanUse},{0x301498,Operation::BattleCryCast},{0x3016e0,Operation::ManaCost}}};
  for (const auto &entry : protection_bodies) {
    if (entry.first == body && !external.count(slot)) {
      args.insert(args.begin(), adjusted);
      // Virtual casts receive a position pointer. Blood Lust, SummonItems
      // and ThrownAxes read xyz; the protection casts ignore the position.
      if (entry.second == Operation::BloodLustCast ||
          entry.second == Operation::SummonAxeCast || entry.second == Operation::SummonArrowsCast ||
          entry.second == Operation::FireAxeCast || entry.second == Operation::ColdAxeCast) {
        if (args.size()!=5) throw Unsupported("virtual position cast shape");
        const auto at=vec(args[3]);
        args={args[0],args[1],args[2],at[0],at[1],at[2],args[4]};
      }
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

void Model::write_vec(Address address, const std::vector<Raw> &v) {
  write(address,8,std::uint64_t(v[0]) | (std::uint64_t(v[1])<<32));
  write(address+8,4,v[2]);
}
std::vector<Raw> Model::ghost_hands(Address owner) {
  const Address ghost=service_scalar(NetObjectOrGhost,{static_cast<Raw>(owner)});
  const auto at=execute(Operation::VisualPosition,{static_cast<Raw>(ghost)});
  std::vector<Raw> hands;
  for (unsigned i=0;i<2;++i) {
    auto hand=service(AttachPoint,{u32(ghost+0x544),at[0],at[1],at[2],
        static_cast<Raw>(half(ghost+0x38)),static_cast<Raw>(ghost+0x114),i,0,ONE});
    if (hand.size()!=3) throw Unsupported("attach point result must be three raw floats");
    hands.insert(hands.end(),hand.begin(),hand.end());
  }
  return hands;
}
void Model::hand_streamers(Address spell, Raw definition, const std::vector<Raw> &hands,
    unsigned last0, unsigned last1) {
  std::array<Raw,6> moved;
  for (unsigned k=0;k<3;++k) {
    moved[k]=sub(hands[k],u32(spell+last0+4*k));
    moved[k+3]=sub(hands[k+3],u32(spell+last1+4*k));
  }
  for (unsigned i=0;i<10;++i) {
    auto first=ball(spell+0x10), second=ball(spell+0x10);
    for (auto &v:first) v=add(v,v);
    for (auto &v:second) v=add(v,v);
    const auto share0=mul(from_int(own_draw(spell+0x10)),TWO_M16);
    std::vector<Raw> p0(3),p1(3);
    for (unsigned k=0;k<3;++k) p0[k]=add(add(hands[k],second[k]),mul(moved[k],share0));
    const auto share1=mul(from_int(own_draw(spell+0x10)),TWO_M16);
    for (unsigned k=0;k<3;++k) p1[k]=add(add(hands[k+3],first[k]),mul(moved[k+3],share1));
    service(ParticleSpawn,{definition,p0[0],p0[1],p0[2],first[0],first[1],first[2],ZERO});
    service(ParticleSpawn,{definition,p1[0],p1[1],p1[2],second[0],second[1],second[2],ZERO});
  }
}
void Model::bloodlust_glow(Address spell, const std::vector<Raw> &hands) {
  const auto frame=u32(0x44fdcc);
  auto fade=ONE;
  const auto since=sub(u32(spell+0x1c),u32(spell+0x20));
  if (lt(since,spread20)) fade=mul(since,0x40a00000);
  const auto left=sub(u32(spell+0x24),u32(spell+0x1c));
  if (lt(left,spread20)) fade=mul(left,0x40a00000);
  const Address owner=u32(spell+0x18);
  write(owner+0x92,2,5); write(owner+0x94,2,2); write(owner+0x90,2,12);
  for (auto off:{0x9e,0x98,0x9a,0x9c}) write(owner+off,2,0);
  const auto wave=sin16(frame<<10);
  const auto pulse=dadd(dmul(f2d(add(wave,ONE)),mem.load(0x551b70,8)),mem.load(0x551b78,8));
  write(Address(u32(spell+0x18))+0x96,2,static_cast<Raw>(d2i(dmul(f2d(fade),pulse))));
  // Portable arithmetic shift of the original signed 32-bit turn.
  const auto turn=static_cast<Raw>(((u32(0x44fdcc)<<22)>>16) | ((u32(0x44fdcc)&0x200u) ? 0xffff0000u : 0u));
  const std::array<std::array<Raw,8>,2> glows{{
      {0x3ec18676,0x45a0d000,0x44d2c000,255,33,15,0x42840000,0x4f8498},
      {0x4108d0bc,0x44d60000,0x4406c000,123,124,15,0x435c0000,0x4f8490}}};
  for (const auto &g:glows) {
    const auto opacity=static_cast<Raw>(to_int(mul(fade,g[6])));
    for (unsigned h=0;h<2;++h) {
      const auto reach=add(mul(sin16(u32(0x44fdcc)<<10),g[2]),g[1]);
      service(GlowAdd,{hands[3*h],hands[3*h+1],hands[3*h+2],g[3],g[4],g[5],
          opacity,u32(g[7]),turn,3,g[0],reach});
    }
  }
  std::array<Raw,6> moved;
  for (unsigned h=0;h<2;++h)
    for (unsigned k=0;k<3;++k) moved[3*h+k]=sub(hands[3*h+k],u32(spell+0x34+12*h+4*k));
  if (s32(u32(0x44fdc0))<=0) return;
  if (u32(Address(u32(spell+0x18))+0x58c)<2) {
    for (unsigned h=0;h<2;++h) {
      const auto far=add(add(mul(moved[3*h],moved[3*h]),mul(moved[3*h+1],moved[3*h+1])),
          mul(moved[3*h+2],moved[3*h+2]));
      unsigned count=3;
      if (le(far,0x42c80000)) {
        count=2;
        if (le(far,0x41c80000)) {
          count=1;
          if (le(far,0x40800000)) count=(u32(0x44fdcc)&3u)==0 ? 1 : 0;
        }
      }
      for (unsigned i=0;i<count;++i) {
        const auto share=mul(from_int(own_draw(spell+0x10)),TWO_M16);
        std::vector<Raw> point(3),velocity(3);
        for (unsigned k=0;k<3;++k) point[k]=add(u32(spell+0x34+12*h+4*k),mul(moved[3*h+k],share));
        const auto b=ball(spell+0x10);
        for (unsigned k=0;k<3;++k) {
          point[k]=add(point[k],mul(b[k],0x40400000));
          velocity[k]=mul(moved[3*h+k],0x40200000);
        }
        service(ParticleSpawn,{0x8e70a0,point[0],point[1],point[2],velocity[0],velocity[1],velocity[2],ZERO});
      }
    }
  } else {
    for (unsigned h=0;h<2;++h) {
      const auto shift=(own_draw(spell+0x10)%0x5dcu+10u)&31u;
      const auto oscillation=sin16(u32(0x44fdcc)<<shift);
      const auto share=mul(from_int(own_draw(spell+0x10)),TWO_M16);
      const auto most=from_int(static_cast<Raw>(to_int(sub(mul(oscillation,0x40c00000),0x40400000))));
      const auto count=to_int(mul(share,most));
      for (std::int32_t i=0;i<count;++i) {
        const auto b=ball(spell+0x10);
        std::vector<Raw> point(3);
        for (unsigned k=0;k<3;++k) point[k]=add(hands[3*h+k],add(b[k],b[k]));
        service(ParticleSpawn,{0x8e70f0,point[0],point[1],point[2],ZERO,ZERO,ZERO,ZERO});
      }
    }
  }
}

std::vector<Raw> item_record(Raw definition, Raw quantity,
    const std::vector<std::vector<Raw>> &slots) {
  std::vector<Raw> out{definition,quantity&0xffffu};
  out.insert(out.end(),16,0); out.insert(out.end(),{255,255});
  out.push_back(static_cast<Raw>(std::max<std::size_t>(4,slots.size())));
  for (const auto &slot:slots) {
    out.push_back(static_cast<Raw>(slot.size()));
    for (auto value:slot) out.push_back(value&0xffffu);
  }
  for (std::size_t i=slots.size();i<4;++i) out.insert(out.end(),{1,0xffff});
  return out;
}
std::vector<Raw> Model::summon_build_item(Raw rank, Raw quantity, bool plain) {
  const auto narrowed=rank&255u;
  const auto signed_rank=narrowed<128 ? std::int32_t(narrowed) : std::int32_t(narrowed)-256;
  constexpr std::array<std::pair<Raw,Raw>,5> names{{
      {5,0x551520},{9,0x551530},{13,0x551548},{17,0x551560},{21,0x551578}}};
  Raw required=21,name=0x551578;
  for (const auto &entry:names) {
    required=entry.first; name=entry.second;
    if (signed_rank<static_cast<std::int32_t>(required) || required==21) break;
  }
  const auto definition=service_scalar(ItemParseDescriptor,{name,plain ? 0u : UINT32_MAX,0});
  auto out=item_record(definition,quantity,{{0x25,1,0,0},{0x24,10,0,0},{0xe,required,0,0}});
  out.push_back(definition); return out;
}
std::uint32_t Model::summon_cast(bool arrows, Address skill, Raw rank, Address caster,
    const std::vector<Raw> &pos) {
  const auto &cfg=arrows ? summon_arrows : summon_axe;
  rank=scalar(skill,14,{rank,cfg.adjust});
  const std::vector<Raw> colour(cfg.flash.begin(),cfg.flash.end());
  flash(caster,1,colour); flash(caster,2,colour);
  sound(cfg.lump,cfg.sound,pos);
  const auto block=service_scalar(PoolNew,{0xbc});
  if (block) service(LightEffectConstructor,{block,add(pos[0],ZERO),pos[1],add(pos[2],0x42100000),
      static_cast<Raw>(caster),0,cfg.light[0],cfg.light[1],cfg.light[2],cfg.light[3],
      ZERO,ZERO,ZERO,ZERO,0x42f00000,0x43340000,ZERO,ONE});
  const auto spell=service_scalar(PoolNew,{0x20});
  if (spell) summon_ctor(arrows,spell,rank,caster,skill);
  return spell;
}
void Model::summon_ctor(bool arrows, Address spell, Raw rank, Address caster, Address skill) {
  const auto &cfg=arrows ? summon_arrows : summon_axe;
  execute(Operation::SpellBase,{static_cast<Raw>(spell),rank,static_cast<Raw>(caster),
      static_cast<Raw>(skill),cfg.table});
  write(spell+0x18,4,u32(0x44fdcc));
  write(spell+0x1c,4,service_scalar(ParticleGroupCreate,{}));
  const auto quantity=scalar(u32(spell+8),10,{0,level(spell)});
  Raw definition;
  std::vector<Raw> item;
  if (!arrows) {
    item=summon_build_item(level(spell),quantity,false);
    definition=item.back(); item.pop_back();
  } else {
    definition=service_scalar(ItemParseDescriptor,{0x551730,UINT32_MAX,0});
    item=item_record(definition,quantity);
  }
  const Address owner=u32(spell+0xc);
  const auto slot=u32(owner+0x3c8);
  Address rec=record(owner);
  std::vector<Raw> args{slot}; args.insert(args.end(),item.begin(),item.end());
  const auto weight=service_scalar(PlayerItemWeight,args);
  if (le(add(u32(rec+0x120),weight),u32(rec+0x124))) {
    args.push_back(1);
    const auto index=s32(service_scalar(InventoryGiveItem,args));
    service(PlayerRecalcLoad,{slot});
    const auto bagbyte=static_cast<Raw>(mem.load(Address(definition)+0x1d,1));
    const auto bag=bagbyte<128 ? std::int32_t(bagbyte) : std::int32_t(bagbyte)-256;
    // Reference call() wraps integer arguments; negative bag/index are not clamped.
    const auto entry=static_cast<Raw>(std::int64_t(rec)+0x130+std::int64_t(bag)*0x1208+std::int64_t(index)*0x48);
    service(InventoryEquipItem,{slot,entry,0});
    service(PlayerResetIdleAnimation,{slot});
  } else {
    service(PlayerVoice,{u32(owner+0x3c8),0});
    rec=record(owner);
    // Python evaluates existing mana before the callback supplying the cost.
    const auto before=u32(rec+0xc0);
    const auto cost=scalar(u32(spell+8),5,{level(spell)});
    const auto mana=add(before,cost); write(rec+0xc0,4,mana);
    if (lt(u32(rec+0xc4),mana)) write(rec+0xc0,4,u32(rec+0xc4));
    write(spell+0x18,4,UINT32_MAX);
  }
}
void Model::summon_run(bool arrows, Address spell) {
  const auto &cfg=arrows ? summon_arrows : summon_axe;
  if (u32(spell+0x18)==UINT32_MAX) {
    service(ParticleGroupFree,{u32(spell+0x1c)});
    if (spell) destroy(spell);
    return;
  }
  const auto age=s32(u32(0x44fdcc)-u32(spell+0x18));
  const auto age_float=from_int(static_cast<Raw>(age));
  auto count=to_int(mul(sin16(static_cast<Raw>(to_int(mul(mul(age_float,0x3d088888),0x47000000)))),cfg.count));
  const Address owner=u32(spell+0xc);
  const auto model=creature_model(owner);
  const auto points=s32(service_scalar(SamplePointCount,{model}));
  const auto ghost=service_scalar(NetObjectOrGhost,{u32(spell+0xc)});
  if (lt(0x41f00000,age_float)) count=0;
  service(ParticleGroupBegin,{u32(spell+0x1c)});
  execute(Operation::VisualPosition,{ghost});
  service(ParticleGroupSetMatrix,{u32(spell+0x1c),0});
  for (std::int32_t i=0;i<count;++i) {
    const auto particle_owner=u32(spell+0xc);
    const auto particle_model=creature_model(particle_owner);
    const auto roll=own_draw(spell+0x10);
    if (!points) throw Unsupported("reference sample-point remainder divides by zero");
    const auto index=static_cast<Raw>(std::int64_t(roll)%points);
    const auto point=sample(u32(spell+0xc),particle_model,{ZERO,ZERO,ZERO},index);
    const auto angle=own_draw(spell+0x10);
    const auto cosine=cos16(angle), sine=sin16(angle);
    service(ParticleSpawn,{cfg.particle,sub(point[0],mul(cosine,cfg.outward)),
        sub(point[1],mul(sine,cfg.outward)),add(sub(point[2],ZERO),cfg.upward),
        mul(cosine,cfg.speed),mul(sine,cfg.speed),ZERO,ZERO});
  }
  service(ParticleGroupEnd,{u32(spell+0x1c)});
  if (age>120) {
    service(ParticleGroupFree,{u32(spell+0x1c)});
    if (spell) destroy(spell);
  }
}
std::vector<Raw> Model::attach_point(Address owner, Raw index, bool supplied, Raw model) {
  const auto at=vec(owner+0x10);
  auto point=service(AttachPoint,{supplied ? model : u32(owner+0x544),at[0],at[1],at[2],
      static_cast<Raw>(half(owner+0x38)),static_cast<Raw>(owner+0x114),index,0,ONE});
  if (point.size()!=3) throw Unsupported("body attach point result must be three raw floats");
  return point;
}
void Model::battlecry_streamers(Address spell, bool emit, Raw definition) {
  const auto hand0=attach_point(u32(spell+0x18),0);
  const auto hand1=attach_point(u32(spell+0x18),1);
  if (emit) hand_streamers(spell,definition,
      {hand0[0],hand0[1],hand0[2],hand1[0],hand1[1],hand1[2]},0x40,0x4c);
  write_vec(spell+0x40,hand0); write_vec(spell+0x4c,hand1);
}
std::vector<std::uint32_t> Model::execute(Operation op,
                                          const std::vector<std::uint32_t> &a) {
  const auto n = static_cast<unsigned>(op);
  constexpr std::array<unsigned, 107> sizes{
      {3, 4, 5, 5, 3, 3, 5, 2, 5, 2, 2, 4, 4, 4, 2, 2, 2, 2, 5, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 2,5,4,5,2, 2,5,4,3,3,1, 2,5,4,3,3, 2,1,1,6,18,6,5,1, 2,7,1,4,10,3,7,1,1,0,4,4, 5,4,2,2,5,4,2,2,3, 2,7,7,7,3,5,3,3,2,0, 2,5,2,4,2,3,3,11, 2,2,2,7,7,7, 2,5,4,3,2,5,3,4}};
  if (n >= sizes.size() || (op==Operation::ShoutAnimation ? a.size()<5 : op==Operation::ItemRecord ? a.size()<3 : a.size()!=sizes[n]))
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
  case Operation::BodyAttachPoint:
    if (a[2]>1) throw Unsupported("attach model option must be boolean");
    return attach_point(s,a[1],a[2]!=0,a[3]);
  case Operation::BattleCryCanUse:
    return {execute(Operation::FindSpell,{a[1],0x3d08dc})[0] ? 0u : 1u};
  case Operation::BattleCryCast: {
    const auto rank=scalar(s,14,{a[1],5});
    const auto spell=service_scalar(PoolNew,{0x58});
    if (spell) execute(Operation::BattleCryCtor,{spell,rank,a[2],a[0]});
    return {spell};
  }
  case Operation::BattleCryCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51b388});
    write(s+0x28,4,MINUS_ONE); write(s+0x24,4,MINUS_ONE); write(s+0x1c,4,0);
    const auto seconds=scalar(a[3],10,{2,static_cast<Raw>(signed_byte(a[1]))});
    write(s+0x30,4,1); write(Address(a[2])+0x58c,4,6);
    write(s+0x20,4,from_int(seconds)); write(s+0x34,4,0); write(s+0x38,4,0); write(s+0x18,4,a[2]);
    execute(Operation::ShoutAnimation,{u32(s+0x18),0x551948,0x22,5,
        0,0x551930,1,0x551978,2,0x551960,3,0x551990,7,0x5519a0,
        3,1,0x3dceda22,2,3,0x3f96c83e,8,2,0x401675ee,4});
    const auto sound_handle=sound(0x5519b8,0x5519c0,vec(Address(u32(s+0x18))+0x10));
    if (sound_handle.size()!=1) throw Unsupported("BattleCry sound handle must be scalar");
    write(s+0x2c,4,sound_handle[0]);
    for (unsigned h=0;h<2;++h) {
      const auto hand=attach_point(u32(s+0x18),h);
      for (unsigned k=0;k<3;++k) mem.poke(s+0x40+12*h+4*k,4,hand[k]);
    }
    return {};
  }
  case Operation::BattleCryStreamers:
    if (a[1]>1) throw Unsupported("streamer definition option must be boolean");
    battlecry_streamers(s,a[1]!=0,a[2]); return {};
  case Operation::BattleCryTint:
    flash(a[1],1,{0x5c,0x21,0xf,0x23}); flash(a[1],2,{0x5c,0x21,0xf,0x23}); return {};
  case Operation::BattleCryAffects: {
    const auto fraction=mul(from_int(scalar(u32(s+8),10,{1,level(s)})),percent);
    Raw covered=0;
    if (a[1]) {
      const Address owner=u32(s+0xc);
      covered=1;
      if (a[1]!=owner) {
        covered=0;
        const auto table=u32(owner+0x48);
        if (!s32(u32(0x4f711c)) || service_scalar(CreatureTeam,
            {static_cast<Raw>(owner+half(Address(table)+176)),a[1]})) {
          if (le(ZERO,u32(s+0x24))) {
            const Address at=u32(s+0x18);
            const auto x=sub(u32(at+0x10),u32(Address(a[1])+0x10));
            const auto y=sub(u32(at+0x14),u32(Address(a[1])+0x14));
            const auto z=sub(u32(at+0x18),u32(Address(a[1])+0x18));
            covered=lt(mul(u32(s+0x28),u32(s+0x28)),add(add(mul(x,x),mul(y,y)),mul(z,z))) ? 0u : 1u;
          }
        }
      }
    }
    return {covered,fraction};
  }
  case Operation::BattleCryRun: {
    const auto virtual_call=[&](Address obj,unsigned slot,unsigned id,std::vector<Raw> args) {
      const auto table=u32(obj+0x48);
      args.insert(args.begin(),static_cast<Raw>(obj+half(Address(table)+8*slot)));
      return service_scalar(id,std::move(args));
    };
    const auto clock=add(u32(s+0x1c),dt); write(s+0x1c,4,clock);
    if (mem.load(0x402791,1) && service_scalar(NetIsLocalSlot,{u32(Address(u32(s+0x18))+0x3c8)}))
      write(0x402797,1,1);
    if (u32(s+0x30)) {
      if (u32(Address(u32(s+0x18))+0x58c)!=6) {
        write(s+0x30,4,0); write(s+0x34,4,0);
        if (lt(u32(s+0x24),ZERO)) {
          execute(Operation::ShoutInterrupted,{a[0],u32(s+0x18),1,0x2c});
          write(s+0x38,4,1); write(s+0x20,4,add(u32(s+0x1c),HALF));
        }
      }
      if (u32(s+0x30)) {
        const auto event=service_scalar(PlayerAnimationStep,{u32(s+0x18),ONE});
        if (!(mem.load(Address(u32(s+0x18))+0x11a,2)&0x20)) {
          write(s+0x30,4,0); write(Address(u32(s+0x18))+0x58c,4,2);
        } else {
          if (event==2) write(s+0x34,4,1);
          if (event==4) write(s+0x34,4,0);
          if (event==8) {
            write(s+0x24,4,u32(s+0x1c)); write(s+0x3c,4,service_scalar(ParticleGroupCreate,{}));
          }
          battlecry_streamers(s,u32(s+0x34)!=0,0x8e6f60);
        }
      }
    }
    if (le(u32(s+0x20),u32(s+0x1c))) {
      if (!u32(s+0x38) && u32(Address(u32(s+0x18))+0x58c)==6) write(Address(u32(s+0x18))+0x58c,4,2);
      if (s) destroy(s);
      return {};
    }
    if (u32(s+0x38)) battlecry_streamers(s,true,0x8e7000);
    if (le(ZERO,u32(s+0x24))) {
      const auto grown=mul(sub(u32(s+0x1c),u32(s+0x24)),0x40000032);
      const auto share=le(grown,ONE) ? grown : ONE;
      const auto reach=mul(from_int(scalar(u32(s+8),10,{3,level(s)})),0x41400000);
      write(s+0x28,4,0x4219b1e4);
      if (lt(0x404ced30,reach)) write(s+0x28,4,add(mul(sub(reach,0x4219b1e4),share),0x4219b1e4));
      auto count=signed_byte(static_cast<Raw>(mem.load(s,1)))+13;
      const auto lift=mul(sub(ONE,mul(share,share)),0x428c0000);
      const Address ghost=service_scalar(NetObjectOrGhost,{u32(s+0x18)});
      const auto at=vec(ghost+0x10);
      const auto matrix=mat34_set_pos_rot_z({at[0],at[1],at[2]},u32(0x44fdcc)*61u);
      service(ParticleGroupBegin,{u32(s+0x3c)}); service(ParticleGroupSetMatrix,{u32(s+0x3c),0});
      const bool growing=lt(share,ONE);
      if (growing) count*=4;
      for (int i=0;i<count;++i) {
        const auto angle=own_draw(s+0x10);
        const Vec3 local{mul(cos16(angle),u32(s+0x28)),mul(sin16(angle),u32(s+0x28)),ZERO};
        const auto world=mat34_transform_point(matrix,local);
        const auto height=service_scalar(TerrainHeightAtPoint,{u32(0x44fdc8),world[0],world[1],world[2]});
        const auto drop=sub(height,u32(ghost+0x18));
        if (le(drop,0x42900000) && le(0xc2900000,drop))
          service(ParticleSpawn,{growing ? 0x8e7050u : 0x8e6fb0u,local[0],local[1],add(drop,lift),ZERO,ZERO,growing ? ZERO : ONE,ZERO});
      }
      service(ParticleGroupEnd,{u32(s+0x3c)});
      execute(Operation::BattleCryTint,{a[0],u32(s+0x18)});
      const auto radius=u32(s+0x28);
      const auto alive_count=s32(u32(0x4f8420));
      for (std::int64_t i=0;i<alive_count;++i) {
        const auto slot=u32(0x4f8440+4*i);
        const Address other=u32(static_cast<Raw>(0x4d8af0u+0x43d8u+slot*0x4c30u));
        auto owner=u32(s+0x18);
        if (!other || other==owner) continue;
        if (s32(u32(0x4f711c)) && !virtual_call(owner,22,CreatureTeam,{static_cast<Raw>(other)})) continue;
        owner=u32(s+0x18);
        const auto x=sub(u32(Address(owner)+0x10),u32(other+0x10));
        const auto y=sub(u32(Address(owner)+0x14),u32(other+0x14));
        const auto z=sub(u32(Address(owner)+0x18),u32(other+0x18));
        if (le(add(add(mul(x,x),mul(y,y)),mul(z,z)),mul(radius,radius)))
          execute(Operation::BattleCryTint,{a[0],static_cast<Raw>(other)});
      }
    }
    if (eq(u32(s+0x24),u32(s+0x1c))) {
      const auto block=service_scalar(PoolNew,{0xbc});
      if (block) execute(Operation::LightEffect,{block,add(a[1],ZERO),a[2],add(a[3],0x42100000),u32(s+0x18),0,
          0x437f0000,0x42b80000,0x42480000,0xc2c00000,0,0,0,0,0x42f00000,0x43340000,ZERO,TWO});
      const auto reach=mul(from_int(scalar(u32(s+8),10,{3,level(s)})),0x41400000);
      const auto found=service(CollectObjectsInBox,{sub(a[1],reach),sub(a[2],reach),sub(a[3],reach),
          add(a[1],reach),add(a[2],reach),add(a[3],reach),0x200});
      for (const Address obj:found) {
        if (!(u32(obj+0xc)&0x20) || !virtual_call(u32(s+0x18),20,CreatureEnemy,{static_cast<Raw>(obj)})) continue;
        const auto x=sub(a[1],u32(obj+0x10)), y=sub(a[2],u32(obj+0x14)), z=sub(a[3],u32(obj+0x18));
        const auto far=sqrt(add(add(mul(x,x),mul(y,y)),mul(z,z)));
        if (!le(far,add(reach,u32(obj+0x1c))) || !le(ZERO,u32(obj+0x228))) continue;
        const auto roll=own_draw(s+0x10)%100u;
        if (static_cast<int>(roll)<s32(scalar(u32(s+8),10,{0,level(s)}))) {
          const auto seconds=from_int(scalar(u32(s+8),10,{2,level(s)}));
          const auto table=u32(obj+0x48);
          service(CreatureStun,{static_cast<Raw>(obj+half(Address(table)+240)),
              static_cast<Raw>(to_int(mul(seconds,0x419fd6ea))),1});
        }
      }
    }
    return {};
  }
  case Operation::FireAxeCanUse:
  case Operation::ColdAxeCanUse:
    return execute(Operation::AxeCanUse,{op==Operation::ColdAxeCanUse ? 1u : 0u,a[1]});
  case Operation::AxeCanUse: {
    if (!(u32(Address(a[1])+0xc)&0x40)) return {1};
    const auto slot=u32(Address(a[1])+0x3c8);
    const Address item=u32(Address(record(a[1]))+0x43f8);
    if (item && (mem.load(item+8,8)&UINT64_C(0x40000000000))) return {1};
    const auto text=service_scalar(TextLookup,{a[0] ? 0x553288u : 0x5531d8u});
    service(HudShowMessage,{text,0xb4,slot,0}); return {0};
  }
  case Operation::FireAxeCast:
  case Operation::ColdAxeCast:
    return execute(Operation::AxeCast,{op==Operation::ColdAxeCast ? 1u : 0u,
        a[0],a[1],a[2],a[3],a[4],a[5]});
  case Operation::AxeCast: {
    const bool cold=a[0]!=0;
    const Address skill=a[1],caster=a[3];
    const auto rank=scalar(skill,14,{a[2],cold ? 2u : 0x14u});
    const std::vector<Raw> colour=cold ? std::vector<Raw>{0x3c,0x3c,0x5e,0x26}
        : std::vector<Raw>{0x73,0x4f,0x38,0x23};
    flash(caster,1,colour); flash(caster,2,colour);
    const auto light=service_scalar(PoolNew,{0xbc});
    if (light) {
      const std::array<Raw,4> c=cold ? std::array<Raw,4>{0x42700000,0x42700000,0x42bc0000,0x42180000}
          : std::array<Raw,4>{0x42e60000,0x429e0000,0x42600000,0x420c0000};
      service(LightEffectConstructor,{light,add(a[4],ZERO),a[5],add(a[6],0x42100000),a[3],0,
          c[0],c[1],c[2],c[3],ZERO,ZERO,ZERO,ZERO,0x42f00000,0x43340000,ZERO,HALF});
    }
    sound(cold ? 0x553298u : 0x5531e8u,cold ? 0x5532a0u : 0x5531f0u,{a[4],a[5],a[6]});
    const Address rec=record(caster);
    const auto speed=from_int(scalar(skill,10,{0,rank}));
    const auto at=vec(caster+0x10);
    const auto hand=service(AttachPoint,{u32(caster+0x544),at[0],at[1],at[2],
        static_cast<Raw>(half(caster+0x38)),static_cast<Raw>(caster+0x114),0,1,ONE});
    // This call asks for both output vectors; direction is returned but unused.
    if (hand.size()!=6) throw Unsupported("axe attach result must contain point and direction");
    Raw heading,climb;
    if (!u32(caster+0x234)) {
      const auto first=draw(),second=draw();
      heading=static_cast<Raw>(mem.load(caster+0x38,2))+first%0xa28u-0x514u;
      climb=add(from_int(second%0x28u),0x42200000);
    } else {
      const auto target=vec(Address(u32(Address(u32(caster+0x234))+0xc))+0x10);
      std::array<Raw,3> away;
      for (unsigned k=0;k<3;++k) away[k]=sub(hand[k],target[k]);
      heading=0x4000u-atan2_16(sub(target[0],hand[0]),sub(target[1],hand[1]));
      climb=sub(sqrt(add(add(mul(away[0],away[0]),mul(away[1],away[1])),mul(away[2],away[2]))),0x42f00000);
    }
    heading=static_cast<Raw>(s16(heading));
    const std::array<Raw,3> velocity{mul(cos16(heading),speed),mul(sin16(heading),speed),climb};
    const Address stack=u32(rec+0x43f8);
    const auto row0=s32(service_scalar(ItemResourceIndex,{static_cast<Raw>(stack)}));
    const auto model0=u32(static_cast<Address>(INT64_C(0x3e9ce8)+16*std::int64_t(row0)));
    const auto row1=s32(service_scalar(ItemResourceIndex,{static_cast<Raw>(stack)}));
    const auto model1=u32(static_cast<Address>(INT64_C(0x3e9cec)+16*std::int64_t(row1)));
    const auto set_byte=static_cast<Raw>(mem.load(rec+0x4a7c,1));
    const auto active=set_byte<128 ? std::int32_t(set_byte) : std::int32_t(set_byte)-256;
    const auto main=u32(static_cast<Address>(std::int64_t(rec)+0x4434+4*active));
    if (!main) return {0};
    auto item_address=[&]() {return static_cast<Raw>(skill+u32(caster+0x3c8)*0x48u+0x68u);};
    service(ItemCopyMutableFields,{item_address(),main});
    const auto low=u32(rec+0xd0),high=u32(rec+0xd4);
    const auto value=scalar(skill,10,{1,rank});
    const auto top=static_cast<Raw>(round_half_away(add(from_int(value),mul(from_int(value),spread20))));
    const Address item=item_address();
    const Raw element=cold ? 0x16u : 0x15u;
    for (unsigned k=0;k<4;++k) {
      const auto kind=half(item+0x24+8*k);
      if (kind==-1 || kind==static_cast<std::int32_t>(element)) {
        write(item+0x24+8*k,2,element); write(item+0x26+8*k,2,value);
        write(item+0x28+8*k,2,top); write(item+0x2a+8*k,2,600); break;
      }
    }
    const auto block=service_scalar(PoolNew,{0x1d0});
    if (block) service(PlayerProjectileConstructor,{block,model0,model1,hand[0],hand[1],hand[2],
        velocity[0],velocity[1],velocity[2],heading+0x4000u,a[3],0,0,low,high,0,0,100000,4,item_address()});
    if ((static_cast<Raw>(mem.load(stack+4,2))-1u)<0x14u) {
      write(0x41b844+Address(u32(caster+0x3c8))*0x20,4,0xf0);
      const auto shown=service_scalar(HudCountActiveNotices,{u32(caster+0x3c8),7});
      write(0x41b8c4+Address(u32(caster+0x3c8))*0x20,4,shown);
    }
    if (!(u32(caster+0xc)&0x20000)) write(stack+4,2,mem.load(stack+4,2)-1);
    if (half(stack+4)<=0) {
      for (unsigned index=0;index<0x40;++index)
        if (rec+0x130+index*0x48==u32(rec+0x43f8))
          service(InventoryRemoveItem,{u32(caster+0x3c8),index,0,0});
    }
    return {0};
  }
  case Operation::LightRecord:
    write(s+9,1,0);
    write_vec(s+0x30,{a[8],a[9],a[10]});
    for (unsigned k=0;k<4;++k) {
      write(s+0x12+2*k,2,a[1+k]); write(s+0xa+2*k,2,a[1+k]);
    }
    for (const auto &v:std::array<std::pair<unsigned,Raw>,5>{{
        {0x1c,a[5]},{0x24,a[5]},{0x28,a[6]},{0x20,a[6]},{0x2c,a[7]}}})
      write(s+v.first,4,v.second);
    write(s+8,1,0); return {};
  case Operation::CriticalHitCanUse:
    return {execute(Operation::FindSpell,{a[1],0x3cf000})[0] ? 0u : 1u};
  case Operation::CriticalHitCast: {
    const auto spell=service_scalar(PoolNew,{0x5c});
    if (spell) execute(Operation::CriticalHitCtor,{spell,a[1],a[2],a[0]});
    return {spell};
  }
  case Operation::CriticalHitManaCost: return skill_call(s,15,{a[1],0x14});
  case Operation::CriticalHitCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x519c28});
    const auto at=vec(Address(a[2])+0x10);
    execute(Operation::LightRecord,{static_cast<Raw>(s+0x1c),0xd7,0x40,0x40,0,
        0x42f00000,0x3891a2b3,ZERO,at[0],at[1],at[2]});
    service(LightLink,{static_cast<Raw>(s+0x1c)});
    const auto seconds=scalar(u32(s+8),10,{2,level(s)});
    write(s+0x18,4,from_int(seconds));
    write(s+0x58,4,service_scalar(ParticleGroupCreate,{})); return {};
  }
  case Operation::CriticalHitTint:
    flash(a[1],1,{0x44,0xe,0x18,0xf}); return {};
  case Operation::CriticalHitRun: {
    const auto left=sub(u32(s+0x18),dt); write(s+0x18,4,left);
    if (lt(left,ZERO)) { if (s) destroy(s); return {}; }
    execute(Operation::CriticalHitTint,{a[0],u32(s+0xc)});
    const auto seconds=scalar(u32(s+8),10,{2,level(s)});
    const auto age=sub(from_int(seconds),u32(s+0x18));
    const Address light=s+0x1c;
    if (le(age,TWO)) {
      const Address owner=u32(s+0xc);
      write_vec(light+0x30,{add(u32(owner+0x10),ZERO),u32(owner+0x14),add(u32(owner+0x18),0x42700000)});
      service(LightRelink,{static_cast<Raw>(light)});
      if (lt(ONE,age)) {
        const auto dim=static_cast<Raw>(to_int(mul(sub(TWO,age),0x42800000)));
        const auto red=static_cast<Raw>(to_int(mul(sub(TWO,age),0x43570000)));
        for (const auto &v:std::array<std::pair<unsigned,Raw>,6>{{
            {0x2a,dim},{0x26,red},{0x2e,red},{0x30,dim},{0x28,dim},{0x32,dim}}})
          write(s+v.first,2,v.second);
      }
    } else service(LightUnlink,{static_cast<Raw>(light)});
    const Address skill=u32(s+8);
    const auto id=s32(u32(skill+0x64));
    const auto first=id==0x35 ? 1 : id==0x21 ? 10 : id==0x51 ? 5 : 1;
    const auto rank=s32(level(s));
    // Signed Python products are narrowed by the FPU conversion, not for the
    // positive-radius gate; use wide integer arithmetic to avoid C++ overflow.
    const auto reach=std::int64_t(s32(scalar(skill,10,{1,static_cast<Raw>(rank)})))*12;
    const Address ghost=service_scalar(NetObjectOrGhost,{u32(s+0xc)});
    if (reach>0) {
      service(ParticleGroupBegin,{u32(s+0x58)});
      const auto at=vec(ghost+0x10);
      const auto matrix=mat34_set_pos_rot_z({at[0],at[1],at[2]},u32(0x44fdcc)*0x76u);
      service(ParticleGroupSetMatrix,{u32(s+0x58),0});
      for (std::int32_t i=0;i<rank-first+14;++i) {
        const auto angle=own_draw(s+0x10);
        const Vec3 local{mul(cos16(angle),from_int(static_cast<Raw>(reach))),
            mul(sin16(angle),from_int(static_cast<Raw>(reach))),ZERO};
        const auto world=mat34_transform_point(matrix,local);
        const auto ground=service_scalar(TerrainHeightAtPoint,{u32(0x44fdc8),world[0],world[1],world[2]});
        const auto drop=sub(ground,u32(ghost+0x18));
        if (le(drop,0x42900000) && le(0xc2900000,drop))
          service(ParticleSpawn,{0x8e5fa0,local[0],local[1],drop,ZERO,ZERO,ONE,ZERO});
      }
      service(ParticleGroupEnd,{u32(s+0x58)});
    }
    const auto square=from_int(static_cast<Raw>(std::uint64_t(reach)*std::uint64_t(reach)));
    const auto count=s32(u32(0x4f8420));
    if (count>65536) throw Unsupported("alive list host limit");
    for (std::int32_t i=0;i<count;++i) {
      const Address owner=u32(s+0xc);
      const auto slot=u32(0x4f8440+4*Address(i));
      const Address other=u32(0x4d8af0u+slot*0x4c30u+0x43d8u);
      if (other==owner) continue;
      if (s32(u32(0x4f711c))) {
        const auto table=u32(owner+0x48);
        if (!service_scalar(CreatureTeam,{static_cast<Raw>(owner+half(Address(table)+176)),static_cast<Raw>(other)})) continue;
      }
      const auto dy=sub(u32(Address(u32(s+0xc))+0x14),u32(other+0x14));
      const auto dx=sub(u32(Address(u32(s+0xc))+0x10),u32(other+0x10));
      if (lt(add(mul(dx,dx),mul(dy,dy)),square)) execute(Operation::CriticalHitTint,{a[0],static_cast<Raw>(other)});
    }
    return {};
  }
  case Operation::CriticalHitBonus: {
    Raw rank=0;
    const Address attacker=a[1];
    if (a[2] && execute(Operation::FindSpell,{a[1],0x3cf000})[0]) rank=a[2];
    else {
      const auto count=s32(u32(0x4f8420));
      if (count>65536) throw Unsupported("alive list host limit");
      auto player_object=[&](Raw slot) {return u32(0x4d8af0u+slot*0x4c30u+0x43d8u);};
      for (std::int32_t i=0;i<count;++i) {
        const auto slot=u32(0x4f8440+4*Address(i));
        if (slot==u32(attacker+0x3c8)) continue;
        const Address other=player_object(slot);
        Vec3 gap{};
        for (unsigned k=0;k<3;++k) gap[k]=sub(u32(other+0x10+4*k),u32(attacker+0x10+4*k));
        if (!lt(add(add(mul(gap[0],gap[0]),mul(gap[1],gap[1])),mul(gap[2],gap[2])),0x49742400)) continue;
        if (s32(u32(0x4f711c))) {
          const auto table=u32(other+0x48);
          if (!service_scalar(CreatureTeam,{static_cast<Raw>(other+half(Address(table)+176)),a[1]})) continue;
        }
        if (!execute(Operation::FindSpell,{player_object(slot),0x3cf000})[0]) continue;
        const auto r53=s32(service_scalar(PlayerGetSkillRank,{slot,0x35}));
        const auto r81=s32(service_scalar(PlayerGetSkillRank,{slot,0x51}));
        auto best=std::max(r53,r81);
        best=std::max(best,s32(service_scalar(PlayerGetSkillRank,{slot,0x21})));
        if (!best) continue;
        const auto reach=from_int(static_cast<Raw>(std::int64_t(s32(scalar(s,10,{1,static_cast<Raw>(best)})))*12));
        const Address refreshed=player_object(slot);
        const auto dy=sub(u32(refreshed+0x14),u32(attacker+0x14));
        const auto dx=sub(u32(refreshed+0x10),u32(attacker+0x10));
        if (lt(add(mul(dx,dx),mul(dy,dy)),mul(reach,reach))) {
          rank=best>=1 ? static_cast<Raw>(best) : 0; break;
        }
      }
    }
    rank=scalar(s,14,{rank,0x14});
    if (!rank) return {ZERO};
    return {mul(from_int(scalar(s,10,{0,rank})),percent)};
  }
  case Operation::SummonArrowsCanUse: return {1};
  case Operation::SummonAxeCast:
  case Operation::SummonArrowsCast:
    return {summon_cast(op==Operation::SummonArrowsCast,s,a[1],a[2],{a[3],a[4],a[5]})};
  case Operation::SummonCast:
    if (a[0]>1) throw Unsupported("summon configuration must be axe=0 or arrows=1");
    return {summon_cast(a[0]!=0,a[1],a[2],a[3],{a[4],a[5],a[6]})};
  case Operation::SummonAxeBuildItem: return summon_build_item(a[0],a[1],a[2]!=0);
  case Operation::SummonCtor:
    if (a[0]>1) throw Unsupported("summon configuration must be axe=0 or arrows=1");
    summon_ctor(a[0]!=0,a[1],a[2],a[3],a[4]); return {};
  case Operation::SummonAxeRun:
  case Operation::SummonArrowsRun:
    summon_run(op==Operation::SummonArrowsRun,s); return {};
  case Operation::SummonRun:
    if (a[0]>1) throw Unsupported("summon configuration must be axe=0 or arrows=1");
    summon_run(a[0]!=0,a[1]); return {};
  case Operation::ItemRecord: {
    std::vector<std::vector<Raw>> slots;
    if (a[2]>a.size()-3) throw Unsupported("item slot count shape");
    std::size_t at=3;
    for (Raw i=0;i<a[2];++i) {
      if (at==a.size() || a[at]>a.size()-at-1) throw Unsupported("item slot shape");
      const auto count=a[at++];
      slots.emplace_back(a.begin()+at,a.begin()+at+count); at+=count;
    }
    if (at!=a.size()) throw Unsupported("item trailing fields");
    return item_record(a[0],a[1],slots);
  }
  case Operation::RiposteCast: return {0};
  case Operation::DodgeManaCost:
    skill_call(s,14,{a[1],1}); return {ZERO};
  case Operation::DodgeFraction:
    if (!a[1]) return {ZERO};
    return {mul(from_int(scalar(s,10,{0,scalar(s,14,{a[1],1})})),percent)};
  case Operation::RiposteCounterAttack: {
    if (!a[3]) return {};
    const Address victim=a[1], attacker=a[2];
    const auto damage=scalar(s,12,{0,a[3],a[1],10});
    const auto chance=s32(scalar(s,12,{1,a[3],a[1],10}));
    if (to_int(mul(mul(from_int(draw()),TWO_M16),0x42c80000))>=chance) return {};
    flash(victim,1,{0x82,0x82,0x83,0x55});
    flash(victim,2,{0x82,0x82,0x83,0x55});
    const auto points=s32(service_scalar(SamplePointCount,{creature_model(victim)}));
    if (!points) return {};
    for (unsigned step=0;step<200;++step) {
      const auto model=creature_model(victim);
      const auto index=static_cast<Raw>(std::int64_t(own_draw(0x404460)) % points);
      const auto at=vec(victim+0x10);
      auto point=service(SamplePoint,{model,static_cast<Raw>(victim+0x114),at[0],at[1],at[2],
          static_cast<Raw>(half(victim+0x38)),index,0,0,ONE});
      if (point.size()!=3) throw Unsupported("Riposte sample result shape");
      // Cosmetic ball associates share+share, unlike the general unit helper.
      std::vector<Raw> b(3);
      do {
        for (auto &v:b) { const auto share=mul(from_int(own_draw(0x404460)),TWO_M16); v=sub(add(share,share),ONE); }
      } while (lt(ONE,add(add(mul(b[0],b[0]),mul(b[1],b[1])),mul(b[2],b[2]))));
      std::vector<Raw> toward(3);
      for (unsigned k=0;k<3;++k) toward[k]=sub(u32(attacker+0x10+4*k),u32(victim+0x10+4*k));
      service(ParticleSpawn,{0x8e5aa0,point[0],point[1],point[2],toward[0],toward[1],toward[2],ZERO});
      if (step&1) service(ParticleSpawn,{0x8e5af0,point[0],point[1],point[2],
          mul(b[0],0x424abc67),mul(b[1],0x424abc67),mul(b[2],0x424abc67),ZERO});
    }
    const auto low=from_int(damage);
    const auto high=add(low,mul(mul(low,mul(from_int(draw()),TWO_M16)),spread20));
    std::vector<Raw> args{a[2],a[1],100000,0,0,0x40};
    args.insert(args.end(),16,0); args.insert(args.end(),{255,255});
    for (unsigned k=0;k<4;++k) args.insert(args.end(),{0xffff,0,0,0});
    args.insert(args.end(),{1,low,high}); service(DamageApply,std::move(args)); return {};
  }
  case Operation::HealingCast: {
    const auto rank=scalar(s,14,{a[1],14});
    const auto block=service_scalar(PoolNew,{0x24});
    if (block) execute(Operation::HealingCtor,{block,rank,a[2],a[0]});
    return {block};
  }
  case Operation::HealingAmount:
    return {mul(mul(a[0],0x3c71aa2e),mul(from_int(a[1]),percent))};
  case Operation::HealingFlash:
    if (lt(u32(s+0x18),0x3e050ef8)) {
      flash(a[1],1,{0x35,0x14,0xd,0x32}); flash(a[1],2,{0x35,0x14,0xd,0x32});
    }
    return {};
  case Operation::HealingCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x519e20});
    const auto resource=service_scalar(ResourceFind,{0x550488,0x550490});
    service(SoundPlayOnObject,{resource,u32(s+0xc),ONE});
    const auto block=service_scalar(PoolNew,{0x5c});
    if (block) { const auto at=vec(Address(u32(s+0xc))+0x10);
      service(GlowingShaftConstructor,{block,at[0],at[1],at[2],0x5a,0xfa,200,100}); }
    write(s+0x20,4,service_scalar(ParticleGroupCreate,{}));
    const Address owner=u32(s+0xc);
    write(s+0x18,4,0);
    const bool player=u32(owner+0xc)&0x40;
    const auto share=scalar(u32(s+8),12,{0,level(s),static_cast<Raw>(owner),player?10u:20u});
    const auto most=player ? u32(Address(record(owner))+0xb0) : u32(Address(u32(s+0xc))+0x22c);
    write(s+0x1c,4,execute(Operation::HealingAmount,{most,share})[0]); return {};
  }
  case Operation::HealingRun: {
    Address owner=u32(s+0xc);
    // Python's signed multiplication is narrowed only when converted to float.
    const auto reach=std::int64_t(s32(scalar(u32(s+8),12,{1,level(s),static_cast<Raw>(owner),20})))*12;
    owner=u32(s+0xc);
    const auto square=from_int(static_cast<Raw>(static_cast<std::uint64_t>(reach)*static_cast<std::uint64_t>(reach)));
    auto virtual_test=[&](unsigned id,Address obj,Raw other,unsigned slot) {
      const auto table=u32(obj+0x48);
      return service_scalar(id,{static_cast<Raw>(obj+half(Address(table)+8*slot)),other});
    };
    auto player_object=[&](Raw slot) {return u32(static_cast<Raw>(0x4d8af0u+slot*0x4c30u+0x43d8u));};
    if (!(u32(owner+0xc)&0x40)) {
      if ((u32(owner+0xc)&0x28)==0x20) {
        const auto span=from_int(static_cast<Raw>(reach)); const auto at=vec(owner+0x10);
        const auto found=service(CollectObjectsInBox,{sub(at[0],span),sub(at[1],span),sub(at[2],span),
            add(at[0],span),add(at[1],span),add(at[2],span),128});
        for (auto other:found) {
          owner=u32(s+0xc);
          if (other!=owner && !virtual_test(CreatureAlly,owner,other,21)) continue;
          owner=u32(s+0xc);
          const auto dy=sub(u32(owner+0x14),u32(Address(other)+0x14));
          const auto dx=sub(u32(owner+0x10),u32(Address(other)+0x10));
          if (!le(add(mul(dx,dx),mul(dy,dy)),square)) continue;
          Raw amount=u32(s+0x1c);
          if (other!=owner) { const auto share=scalar(u32(s+8),12,{2,level(s),static_cast<Raw>(owner),20});
            amount=execute(Operation::HealingAmount,{u32(Address(other)+0x22c),share})[0]; }
          write(Address(other)+0x228,4,add(u32(Address(other)+0x228),amount));
          execute(Operation::HealingFlash,{a[0],other});
        }
      }
    } else {
      std::vector<Raw> near;
      const auto count=s32(u32(0x4f8420));
      if (count>65536) throw Unsupported("alive list host limit");
      for (std::int32_t i=0;i<count;++i) {
        const auto slot=u32(0x4f8440+4*Address(i));
        if (s32(u32(0x4f711c)) && !virtual_test(CreatureTeam,u32(s+0xc),player_object(slot),22)) continue;
        owner=u32(s+0xc); const Address other=player_object(slot); std::vector<Raw> gap(3);
        for (unsigned k=0;k<3;++k) gap[k]=sub(u32(owner+0x10+4*k),u32(other+0x10+4*k));
        if (le(add(add(mul(gap[0],gap[0]),mul(gap[1],gap[1])),mul(gap[2],gap[2])),square)) near.push_back(slot);
      }
      for (auto slot:near) {
        const Address rec=0x4d8af0u+slot*0x4c30u;
        if (le(add(u32(rec+0xac),ONE),u32(rec+0xb0))) {
          Raw amount=u32(s+0x1c);
          if (player_object(slot)!=u32(s+0xc)) {
            const auto share=scalar(u32(s+8),12,{2,level(s),u32(s+0xc),20});
            amount=execute(Operation::HealingAmount,{u32(rec+0xb0),share})[0]; }
          write(rec+0xac,4,add(u32(rec+0xac),amount));
        }
        execute(Operation::HealingFlash,{a[0],player_object(slot)});
      }
    }
    if (reach>=1) {
      const auto count=s32(level(s))-half(Address(u32(s+8))+0x10)+5;
      const Address ghost=service_scalar(NetObjectOrGhost,{u32(s+0xc)});
      service(ParticleGroupBegin,{u32(s+0x20)});
      const auto at=vec(ghost+0x10);
      const auto matrix=mat34_set_pos_rot_z({at[0],at[1],at[2]},u32(0x44fdcc)*0x5fu);
      // The reference exposes a stack marker, not the matrix's address/bytes.
      service(ParticleGroupSetMatrix,{u32(s+0x20),0});
      for (std::int32_t i=0;i<count;++i) {
        const auto angle=own_draw(s+0x10);
        const Vec3 local{mul(cos16(angle),from_int(static_cast<Raw>(reach))),
            mul(sin16(angle),from_int(static_cast<Raw>(reach))),ZERO};
        const auto world=mat34_transform_point(matrix,local);
        const auto ground=service_scalar(TerrainHeightAtPoint,{u32(0x44fdc8),world[0],world[1],world[2]});
        const auto drop=sub(ground,u32(ghost+0x18));
        if (le(drop,0x42900000) && le(0xc2900000,drop))
          service(ParticleSpawn,{0x8e6040,local[0],local[1],drop,ZERO,ZERO,ONE,ZERO});
      }
      service(ParticleGroupEnd,{u32(s+0x20)});
    }
    const auto clock=add(u32(s+0x18),dt); write(s+0x18,4,clock);
    if (lt(0x3f90a1df,clock)) destroy(s);
    return {};
  }
  case Operation::BloodLustCanUse:
    return {execute(Operation::FindSpell,{a[1],0x3d0a00})[0] ? 0u : 1u};
  case Operation::BloodLustCast: {
    const auto rank=scalar(s,14,{a[1],2});
    sound(0x551a90,0x551a98,{a[3],a[4],a[5]});
    const auto spell=service_scalar(PoolNew,{0x4c});
    if (spell) execute(Operation::BloodLustCtor,{spell,rank,a[2],a[0]});
    return {spell};
  }
  case Operation::GhostHands: return ghost_hands(s);
  case Operation::BloodLustCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51b440});
    write(s+0x1c,4,0);
    const auto byte=a[1]&255u, rank=byte<128 ? byte : byte|0xffffff00u;
    const auto seconds=scalar(a[3],10,{0,rank});
    write(s+0x28,4,1); write(Address(a[2])+0x58c,4,6);
    write(s+0x24,4,from_int(seconds));
    write(s+0x2c,4,0); write(s+0x30,4,0); write(s+0x18,4,a[2]);
    execute(Operation::ShoutAnimation,{a[2],0x551ae0,0x822,5,
        0,0x551ac8,1,0x551b10,2,0x551af8,3,0x551b28,7,0x551b38,
        1,1,0x3ef9db01,2});
    const auto hands=ghost_hands(u32(s+0x18));
    for (unsigned h=0;h<2;++h)
      for (unsigned k=0;k<3;++k) mem.poke(s+0x34+12*h+4*k,4,hands[3*h+k]);
    return {};
  }
  case Operation::HandStreamers:
    hand_streamers(s,a[1],{a[2],a[3],a[4],a[5],a[6],a[7]},a[8],a[9]); return {};
  case Operation::BloodLustRun: {
    const auto clock=add(u32(s+0x1c),dt); write(s+0x1c,4,clock);
    if (mem.load(0x402791,1) && service_scalar(NetIsLocalSlot,{u32(Address(u32(s+0x18))+0x3c8)}))
      write(0x402797,1,1);
    if (u32(s+0x28)) {
      if (u32(Address(u32(s+0x18))+0x58c)!=6) {
        write(s+0x28,4,0);
        if (!u32(s+0x2c)) {
          execute(Operation::ShoutInterrupted,{a[0],u32(s+0x18),0,0});
          write(s+0x30,4,1); write(s+0x24,4,add(u32(s+0x1c),HALF));
        }
      }
      if (u32(s+0x28)) {
        const auto event=service_scalar(PlayerAnimationStep,{u32(s+0x18),ONE});
        if (!(mem.load(Address(u32(s+0x18))+0x11a,2)&0x20)) {
          write(s+0x28,4,0); write(Address(u32(s+0x18))+0x58c,4,2);
        } else if (event==2) {
          write(s+0x2c,4,1); write(s+0x20,4,u32(s+0x1c));
        }
      }
    }
    if (le(u32(s+0x24),u32(s+0x1c))) {
      if (!u32(s+0x30) && u32(Address(u32(s+0x18))+0x58c)==6)
        write(Address(u32(s+0x18))+0x58c,4,2);
      if (s) destroy(s);
      return {};
    }
    const auto hands=ghost_hands(u32(s+0x18));
    if (u32(s+0x30)) hand_streamers(s,0x8e7140,hands,0x34,0x40);
    else if (u32(s+0x2c)) bloodlust_glow(s,hands);
    write_vec(s+0x34,{hands[0],hands[1],hands[2]});
    write_vec(s+0x40,{hands[3],hands[4],hands[5]});
    return {};
  }
  case Operation::BloodLustGlow:
    bloodlust_glow(s,{a[1],a[2],a[3],a[4],a[5],a[6]}); return {};
  case Operation::BloodLustComplement:
  case Operation::BloodLustMultiplier: {
    if (!u32(s+0x2c)) return {ONE};
    const auto fraction=mul(from_int(scalar(u32(s+8),10,
        {op==Operation::BloodLustComplement ? 2u : 1u,level(s)})),percent);
    return {op==Operation::BloodLustComplement ? sub(ONE,fraction) : add(fraction,ONE)};
  }
  case Operation::ShoutAnimation: {
    // [owner, default, flags, name-count, (style,name)...,
    //  event-count, (slot,seconds,mask)...]. Validate before effects.
    if (a.size()<5 || a[3]>(a.size()-5)/2) throw Unsupported("shout name shape");
    const auto events_at=4+2*std::size_t(a[3]);
    if (a[events_at]>(a.size()-events_at-1)/3 ||
        a.size()!=events_at+1+3*std::size_t(a[events_at])) throw Unsupported("shout event shape");
    const auto byte=static_cast<Raw>(mem.load(Address(record(s))+0x4a86,1));
    const auto style=byte<128 ? byte : byte|0xffffff00u;
    auto name=a[1];
    for (unsigned i=0;i<a[3];++i) if (a[4+2*i]==style) name=a[5+2*i];
    // None is an explicit null marker in this fixed callback signature.
    const auto text=service_scalar(StringFormat,{name,0});
    const Address clip=service_scalar(ResourceFind,{u32(u32(s+0x24)),text});
    for (std::size_t i=events_at+1;i<a.size();i+=3) write(indexed(clip,0x18,8,a[i]),1,a[i+2]);
    for (std::size_t i=events_at+1;i<a.size();i+=3) write(indexed(clip,0x14,8,a[i]),4,a[i+1]);
    service(AnimationBlend,{static_cast<Raw>(s+0x114),static_cast<Raw>(clip),a[2],0x3dcccccc,0x3dcccccc});
    return {};
  }
  case Operation::ShoutInterrupted: {
    if (a[2]>1) throw Unsupported("sound option must be boolean");
    const Address rec=record(a[1]);
    const auto cost=scalar(u32(s+8),5,{level(s)});
    const auto mana=add(u32(rec+0xc0),mul(cost,HALF)); write(rec+0xc0,4,mana);
    if (lt(u32(rec+0xc4),mana)) write(rec+0xc0,4,u32(rec+0xc4));
    if (a[2]) service(SoundStop,{u32(s+a[3])});
    return {};
  }
  case Operation::CalleeStore:
    mem.poke(s,a[1],std::uint64_t(a[2]) | (std::uint64_t(a[3])<<32)); return {};
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
