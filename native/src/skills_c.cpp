// Authored from ref_skills_c.py at 356add27d50e50972b2a671b31e8f4ec93e6778c.
// SHA256 9e673475a0b89292e483075d76b2f277d1251ce820a0956053e68c83faa81e91.
#include "champions/skills_c.hpp"
#include "champions/detail/skill_kernel.hpp"
#include "champions/detail/skill_spell_kernel.hpp"
#include "champions/detail/skill_handle_kernel.hpp"
#include <array>
#include "champions/core_math.hpp"

namespace champions::simulation::rta::skills_c {
namespace kernel = detail::skill_kernel;
namespace spells = detail::skill_spell_kernel;
std::int64_t c_div(std::int32_t a, std::int32_t b) {
  if (!b) throw Unsupported("reference integer division by zero");
  return std::int64_t(a) / std::int64_t(b);
}
std::int64_t c_rem(std::int32_t a, std::int32_t b) {
  return std::int64_t(a) - std::int64_t(b)*c_div(a,b);
}
DamageDescriptor desc(const std::vector<std::vector<Raw>> &slots) {
  DamageDescriptor result{};
  result.header[16]=result.header[17]=255;
  for (const auto &slot: slots) {
    std::vector<std::uint16_t> narrowed;
    for (auto word: slot) narrowed.push_back(static_cast<std::uint16_t>(word));
    result.slots.push_back(std::move(narrowed));
  }
  while (result.slots.size()<4) result.slots.push_back({65535});
  return result;
}
ResultValue shape_result(const std::string &ints, char ret,
                         const std::vector<ResultValue> &args,
                         ResultValue integer, ResultValue floating,
                         const std::vector<ResultValue> &outs) {
  using V=ResultValue;
  std::vector<V> values;
  std::size_t position=0, given=0;
  const auto truth=[](const V &v) { return v.kind==V::Scalar ? v.scalar!=0 : !v.values.empty(); };
  for (char kind: ints) {
    if (std::string("oOwWbnL").find(kind)!=std::string::npos) {
      V value;
      if (given<outs.size()) value=outs[given++];
      else if (kind=='o' || kind=='O') value={V::Tuple,0,std::vector<V>(3,{V::Scalar,0,{}})};
      else if (kind=='n' || kind=='L') value={V::List,0,{}};
      else value={V::Scalar,0,{}};
      if (kind=='O' || kind=='W') {
        if (position>=args.size()) throw Unsupported("optional output argument missing");
        if (truth(args[position])) values.push_back(std::move(value));
      } else values.push_back(std::move(value));
    }
    if (std::string("ipvelsSOWdTI").find(kind)!=std::string::npos) ++position;
  }
  if (ret=='s') return {V::Buffer,0,{given<outs.size() ? outs[given] : V{V::Bytes,0,{}}}};
  if (ret!='n') values.insert(values.begin(),ret=='f' ? floating : integer);
  if (values.empty()) return {};
  if (values.size()==1) return values[0];
  return {V::Tuple,0,std::move(values)};
}
ResultValue shape_result(const std::map<std::string,ExternalResultSignature> &registry,
                         const std::string &name, const std::vector<ResultValue> &args,
                         ResultValue integer, ResultValue floating,
                         const std::vector<ResultValue> &outs) {
  const auto found=registry.find(name);
  if (found==registry.end()) throw Unsupported("unknown EXTERNALS result name");
  return shape_result(found->second.ints,found->second.ret,args,
                      std::move(integer),std::move(floating),outs);
}
namespace {
// Each variant retains the pinned RTA contract; no Norrath assumptions.
struct StormConfig {
  Raw table, size, light, handle, extra, lump, sound, adjust, element, warning_life;
  std::array<Raw,4> colour, warning;
};
constexpr StormConfig lightning{0x51c5e0,0xbc,0x80,0x7c,0x3f199999,0x553330,0x553338,2,0x18,0x3faccccc,
    {0x6f,0x70,0x9a,0xff},{0x42de0000,0x42e00000,0x431a0000,0x42ff0000}};
constexpr StormConfig pus{0x51b830,0xc0,0x84,0x80,0x3f333333,0x552328,0x552330,20,0x26,0x3fac8b32,
    {0x4f,0x84,0,0xe7},{0x429e0000,0x43040000,0,0x42e70000}};
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
namespace {
using V = ResultValue;
V raw_value(Raw word) { return {V::Scalar,word,{}}; }
std::vector<V> raw_values(const std::vector<Raw> &words) {
  std::vector<V> out;
  for (auto word:words) out.push_back(raw_value(word));
  return out;
}
std::vector<Raw> raw_result(const V &value) {
  if (value.kind==V::None) return {};
  if (value.kind==V::Scalar) {
    if (value.scalar>UINT32_MAX) throw Unsupported("named result outside raw32 domain");
    return {static_cast<Raw>(value.scalar)};
  }
  if (value.kind!=V::Tuple) throw Unsupported("virtual result must be scalar, tuple or None");
  std::vector<Raw> out;
  for (const auto &child:value.values) {
    if (child.kind!=V::Scalar || child.scalar>UINT32_MAX)
      throw Unsupported("virtual result must contain raw32 scalars");
    out.push_back(static_cast<Raw>(child.scalar));
  }
  return out;
}
struct ValueReader {
  const std::vector<Raw> &words;
  std::size_t at=0;
  Raw word() {
    if (at>=words.size()) throw Unsupported("dispatch transport truncated");
    return words[at++];
  }
  std::string string() {
    const auto count=word();
    if (count>words.size()-at) throw Unsupported("dispatch string truncated");
    std::string out;
    for (Raw k=0;k<count;++k) {
      const auto c=word();
      if (c>127) throw Unsupported("dispatch string outside ASCII domain");
      out.push_back(static_cast<char>(c));
    }
    return out;
  }
  V value(unsigned depth=0) {
    if (depth>=64) throw Unsupported("dispatch value depth");
    const auto kind=word();
    if (kind>V::Buffer) throw Unsupported("dispatch value kind");
    V out;out.kind=static_cast<V::Kind>(kind);
    if (kind==V::Scalar) out.scalar=word();
    else if (kind!=V::None) {
      const auto count=word();
      if (count>words.size()-at) throw Unsupported("dispatch value count");
      for (Raw k=0;k<count;++k) out.values.push_back(value(depth+1));
      if (kind==V::Buffer && count!=1) throw Unsupported("dispatch buffer shape");
      if (kind==V::Bytes) for (const auto &child:out.values)
        if (child.kind!=V::Scalar || child.scalar>255) throw Unsupported("dispatch byte domain");
    }
    return out;
  }
  void end() { if (at!=words.size()) throw Unsupported("dispatch trailing words"); }
};
void encode_value(std::vector<Raw> &out,const V &v,unsigned depth=0) {
  if (depth>=64) throw Unsupported("dispatch result depth");
  out.push_back(v.kind);
  if (v.kind==V::Scalar) {
    if (v.scalar>UINT32_MAX) throw Unsupported("dispatch raw64 outside transport");
    out.push_back(static_cast<Raw>(v.scalar));
  } else if (v.kind!=V::None) {
    out.push_back(static_cast<Raw>(v.values.size()));
    for (const auto &child:v.values) encode_value(out,child,depth+1);
  }
}
}
ResultValue Model::call(const std::string &name,std::vector<ResultValue> args) {
  if (depth_>=64) throw Unsupported("named callback recursion limit");
  ++depth_;
  struct Guard { std::size_t &depth; ~Guard() {--depth;} } guard{depth_};
  // Base.call masks only top-level integers, never integers nested in a value.
  for (auto &arg:args) if (arg.kind==V::Scalar) arg.scalar &= UINT32_MAX;
  if (calls.size()+named_calls.size()>=65536) throw Unsupported("named call history limit");
  named_calls.push_back({calls.size()+named_calls.size(),name,args});
  const auto found=named_callbacks.find(name);
  if (found!=named_callbacks.end()) {
    auto function=found->second;
    return function(*this,args);
  }
  return shape_result(signatures,name,args);
}
std::vector<Raw> Model::table_call(const std::string &cls,Address object,
    unsigned offset,unsigned slot,const std::vector<Raw> &args) {
  if (object>UINT32_MAX || slot>65535) throw Unsupported("table call host domain");
  if (depth_>=64) throw Unsupported("table recursion limit");
  ++depth_;
  struct Guard { std::size_t &depth; ~Guard() {--depth;} } guard{depth_};
  const Address table=u32(object+offset);
  const auto adjusted=kernel::adjust_this(static_cast<Raw>(object), static_cast<Raw>(half(table+8*slot)));
  const std::string name=cls+".v"+std::to_string(slot);
  if (cls=="Spell" && !named_external.count(name) && !(slot==1 && external.count(SpellDestroyCallback))) {
    const Raw body=u32(table+8*slot+4);
    const std::array<std::pair<Raw,Operation>,18> spells{{
      {0x314618,Operation::RetributionRun},{0x3151b4,Operation::SpiritShieldRun},
      {0x317ec0,Operation::FrostArmorRun},{0x317000,Operation::WaveOfFlameRun},
      {0x2fd5d4,Operation::UnspokenWordRun},{0x31bcd0,Operation::ConeOfShockRun},
      {0x30e6f0,Operation::DrowsyRun},{0x3003d8,Operation::RoarRun},
      {0x301a34,Operation::BattleCryRun},{0x319068,Operation::DiseaseBladeRun},
      {0x310cd0,Operation::SummonBadgerRun},{0x31399c,Operation::RaiseArmyRun},
      {0x3034dc,Operation::BloodLustRun},{0x2feddc,Operation::SummonAxeRun},
      {0x2ffad4,Operation::SummonArrowsRun},{0x2e90b8,Operation::HealingRun},
      {0x2e7acc,Operation::CriticalHitRun},{0x2e699c,Operation::RootRun}}};
    for (const auto &entry:spells) if (entry.first==body) {
      const bool required_position=entry.second==Operation::RetributionRun || entry.second==Operation::ConeOfShockRun ||
          entry.second==Operation::RoarRun || entry.second==Operation::BattleCryRun || entry.second==Operation::DiseaseBladeRun;
      const bool required_facing=entry.second==Operation::ConeOfShockRun || entry.second==Operation::RoarRun;
      if (args.size()>2 || (required_position && args.empty()) || (required_facing && args.size()!=2))
        throw Unsupported("spell virtual run argument shape");
      const Raw position=args.empty() ? 0 : args[0], facing=args.size()==2 ? args[1] : 0;
      if (entry.second==Operation::RetributionRun || entry.second==Operation::ConeOfShockRun ||
          entry.second==Operation::BattleCryRun || entry.second==Operation::DiseaseBladeRun) {
        const auto pos=vec(position);
        return execute(entry.second,{adjusted,pos[0],pos[1],pos[2],facing});
      }
      return execute(entry.second,{adjusted,position,facing});
    }
    // These are selected inherited implementations, not opaque virtual callees.
    if (body==0x30cbc8) {
      if (args.size()>2) throw Unsupported("Tagar virtual run shape");
      return execute(Operation::TagarRun,{adjusted,args.empty() ? 0 : args[0],args.size()==2 ? args[1] : 0});
    }
    if (body==0x2e177c) {
      if (args.size()>2) throw Unsupported("Skeleton virtual run shape");
      return execute(Operation::SkeletonRun,{adjusted,args.empty() ? 0 : args[0],args.size()==2 ? args[1] : 0});
    }
  }
  if (name=="Spell.v1" && !named_callbacks.count(name) && !signatures.count(name)) {
    auto values=args;values.insert(values.begin(),adjusted);
    return service(SpellDestroyCallback,std::move(values));
  }
  auto arguments=raw_values(args);
  if (cls=="Spell" && args.size()==2)
    arguments[0]={V::Tuple,0,raw_values(vec(args[0]))};
  arguments.insert(arguments.begin(),raw_value(adjusted));
  return raw_result(call(name,std::move(arguments)));
}
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
  const auto delta = kernel::delta(static_cast<Raw>(level), start);
  return static_cast<std::uint32_t>(round_half_away(kernel::statistic_sum(
      kernel::statistic_base(static_cast<std::uint32_t>(half(indexed(skill, base, 2, stat)))),
      kernel::statistic_step(u32(indexed(skill, step, 4, stat)), delta))));
}
std::uint32_t Model::draw() {
  const auto seed = kernel::lcg_step(u32(seed_address));
  if (writes.size() >= 65536)
    throw Unsupported("write history limit");
  writes.push_back({static_cast<std::uint32_t>(seed_address), 4, seed});
  mem.store(seed_address, 4, seed);
  ++draws;
  return kernel::lcg_result(seed);
}
std::vector<std::uint32_t> Model::invoke(Address skill, unsigned slot,
                                         std::vector<std::uint32_t> args) {
  if (skill > UINT32_MAX)
    throw Unsupported("skill address outside raw32 domain");
  if (slot > 65535)
    throw Unsupported("virtual slot outside host domain");
  const auto table = u32(skill + 0x60);
  const auto adjusted =
      kernel::adjust_this(static_cast<Raw>(skill), static_cast<Raw>(half(Address(table) + 8 * slot)));
  const auto body = u32(Address(table) + 8 * slot + 4);
  const bool forced=external.count(slot) || named_external.count("Skill.v"+std::to_string(slot));
  for (const auto &entry : bodies) {
    if (entry.second == body && !forced) {
      args.insert(args.begin(), adjusted);
      return execute(slot_operation(entry.first), args);
    }
  }
  const std::array<std::pair<std::uint32_t, Operation>, 63> protection_bodies{{
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
      {0x301464,Operation::BattleCryCanUse},{0x301498,Operation::BattleCryCast},{0x3016e0,Operation::ManaCost},
      {0x31a920,Operation::LightningStrikeCast},{0x31ae14,Operation::ManaCost},
      {0x30aaa8,Operation::PusStormCast},{0x30af38,Operation::ManaCost},
      {0x318730,Operation::DiseaseBladeCanUse},{0x318764,Operation::DiseaseBladeCast},
      {0x318aa0,Operation::ManaCost},
      {0x3134b0,Operation::RaiseArmyCanUse},{0x3134e4,Operation::RaiseArmyCast},
      {0x3137a4,Operation::ManaCost},
      {0x30e1f0,Operation::DrowsyCast},{0x30e5ec,Operation::ManaCost},
      {0x316b1c,Operation::WaveOfFlameCanUse},{0x316b24,Operation::WaveOfFlameCast},
      {0x316cdc,Operation::ManaCost},
      {0x2fff64,Operation::RoarCanUse},{0x2fff98,Operation::RoarCast},{0x300138,Operation::ManaCost},
      {0x2fd030,Operation::UnspokenWordCast},{0x2fd334,Operation::ManaCost},
      {0x30f168,Operation::PoisonBoltCast},{0x30f364,Operation::ManaCost},
      {0x310690,Operation::SummonBadgerCanUse},{0x3106f4,Operation::SummonBadgerCast},
      {0x310c08,Operation::ManaCost},
      {0x2e6534,Operation::RootCanUse},{0x2e6598,Operation::RootCast},{0x2e6894,Operation::ManaCost},
      {0x31b8f0,Operation::ConeOfShockCast},{0x31ba94,Operation::ManaCost}}};
  for (const auto &entry : protection_bodies) {
    if (entry.first == body && !forced) {
      args.insert(args.begin(), adjusted);
      // Position-using virtual casts expand the supplied pointer to raw xyz.
      // The protection casts ignore the position.
      if (entry.second == Operation::BloodLustCast ||
          entry.second == Operation::SummonAxeCast || entry.second == Operation::SummonArrowsCast ||
          entry.second == Operation::FireAxeCast || entry.second == Operation::ColdAxeCast ||
          entry.second == Operation::LightningStrikeCast || entry.second == Operation::PusStormCast ||
          entry.second == Operation::RaiseArmyCast || entry.second == Operation::DrowsyCast ||
          entry.second == Operation::UnspokenWordCast || entry.second == Operation::PoisonBoltCast ||
          entry.second == Operation::SummonBadgerCast || entry.second == Operation::RootCast || entry.second == Operation::ConeOfShockCast) {
        if (args.size()!=5) throw Unsupported("virtual position cast shape");
        const auto at=vec(args[3]);
        args={args[0],args[1],args[2],at[0],at[1],at[2],args[4]};
      }
      return execute(entry.second, args);
    }
  }
  if (body==0x30c708 && !forced) {
    if (args.size()!=4) throw Unsupported("Tagar virtual cast shape");
    const auto at=vec(args[2]);
    return execute(Operation::TagarCast,{adjusted,args[0],args[1],at[0],at[1],at[2],args[3]});
  }
  if (body==0x30c708 && forced && slot<=15) {
    if (args.size()!=4) throw Unsupported("forced Tagar cast shape");
    const auto at=vec(args[2]);
    args={args[0],args[1],at[0],at[1],at[2],args[3]};
  }
  if (body==0x2e2368 && !forced) {
    if (args.size()!=1) throw Unsupported("Skeleton virtual can-use shape");
    return execute(Operation::SkeletonCanUse,{adjusted,args[0]});
  }
  if (body==0x2e18a4 && !forced) {
    if (args.size()!=4) throw Unsupported("Skeleton virtual cast shape");
    const auto at=vec(args[2]);
    return execute(Operation::SkeletonCast,{adjusted,args[0],args[1],at[0],at[1],at[2],args[3]});
  }
  if (body==0x2e18a4 && forced && slot<=15) {
    if (args.size()!=4) throw Unsupported("forced Skeleton cast shape");
    const auto at=vec(args[2]);args={args[0],args[1],at[0],at[1],at[2],args[3]};
  }
  if (slot>15) {
    auto arguments=raw_values(args);
    if (body==0x30c708 || body==0x2e18a4) {
      if (args.size()!=4) throw Unsupported("named Tagar cast shape");
      arguments[2]={V::Tuple,0,raw_values(vec(args[2]))};
    }
    arguments.insert(arguments.begin(),raw_value(adjusted));
    return raw_result(call("Skill.v"+std::to_string(slot),std::move(arguments)));
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
  if (slot > 15)
    throw Unsupported("slot outside supported domain");
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
  return spells::rng_publication([&]{return u32(a);},
                                 [&](Raw seed){write(a,4,seed);});
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
  return detail::skill_handle_kernel::take(Address(0x864ea8),
      [&](Address at) { return u32(at); },
      [&](Address at, unsigned width, Raw value) { write(at, width, value); },
      [](Address at, Address displacement) { return at + displacement; });
}
void Model::handle_insert(Address head, Raw node) {
  write(node, 4, u32(head));
  if (u32(head)) write(Address(u32(head))+4, 4, node);
  write(Address(node)+4, 4, static_cast<Raw>(head));
  write(head, 4, node);
}
void Model::handle_recycle(Address node) {
  detail::skill_handle_kernel::recycle(Address(0x864ea8), node,
      [&](Address at) { return u32(at); },
      [&](Address at, unsigned width, Raw value) { write(at, width, value); });
}
void Model::handle_unlink(Address node) {
  if (u32(node+4)) write(u32(node+4), 4, u32(node));
  if (u32(node)) write(Address(u32(node))+4, 4, u32(node+4));
  const Address partner = u32(node+8);
  if (partner) {
    if (u32(partner+4)) write(u32(partner+4), 4, u32(partner));
    if (u32(partner)) write(Address(u32(partner))+4, 4, u32(partner+4));
    handle_recycle(partner);
  }
  handle_recycle(node);
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
// The holder's table has no modelled bodies in the pinned contract.
void Model::holder_call(Address holder, unsigned slot, std::vector<Raw> args) {
  const Address table=u32(holder);
  args.insert(args.begin(),static_cast<Raw>(holder+half(table+8*slot)));
  service(slot==7 ? HolderDisease : HolderClearElement,std::move(args));
}
void Model::write_disease(Address item, Raw raw_value, std::int64_t frames,
                          std::int64_t limit) {
  const auto value=s32(raw_value);
  if (!item || (mem.load(item+8,8)&0x1000000u)) return;
  std::int32_t old=0;
  for (unsigned k=0;k<4;++k) {
    if (half(item+0x24+8*k)==0x26) {
      old=half(item+0x26+8*k); break;
    }
  }
  if (old<0 || old>=limit || old>value) return;
  const auto top=static_cast<Raw>(round_half_away(add(from_int(raw_value),mul(from_int(raw_value),spread20))));
  for (unsigned k=0;k<4;++k) {
    const Address at=item+0x24+8*k;
    if (half(at)==-1 || half(at)==0x26) {
      write(at,2,0x26); write(at+2,2,raw_value);
      write(at+4,2,top); write(at+6,2,static_cast<Raw>(frames)); break;
    }
  }
}
// Inherited Tagar contract; the arena base constructor has no skills-C age store.
void Model::arena_spell_base(Address s,Raw rank,Address owner,Address skill,Raw table) {
  write(s,1,rank);write(s+0x14,4,0x515ba0);write(s+0xc,4,owner);
  write(s+4,4,0);write(s+8,4,skill);write(s+0x10,4,draw());write(s+0x14,4,table);
}
std::vector<Raw> Model::arena_ball(Address seed) {
  for (;;) {
    std::vector<Raw> v;
    for (unsigned k=0;k<3;++k) {
      const auto m=mul(from_int(own_draw(seed)),TWO_M16);
      v.push_back(sub(add(m,m),ONE));
    }
    if (!lt(ONE,add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(v[2],v[2])))) return v;
  }
}
Raw Model::tagar_virtual(Address obj,unsigned slot,const std::vector<Raw> &args) {
  const Address table=u32(obj+0x48);
  const Address self=static_cast<Raw>(obj+half(table+8*slot));
  const Raw body=u32(table+8*slot+4);
  const unsigned id=slot==20 ? CreatureEnemy : CreatureGetTeam;
  if (!external.count(id) && !named_external.count("Creature.v"+std::to_string(slot))) {
    if (slot==37 && body==0x3c2730) return static_cast<Raw>(signed_byte(mem.load(self+0x310,1)));
    if (slot==20 && body==0x119328) {
      const Address other=args.at(0);
      if (!other || other==self) return 0;
      const auto flags=u32(other+0xc);
      if (!(flags&0x60) || (flags&0x20008) || (mem.load(other+0xa8,8)&0x4000800)) return 0;
      const auto ours=tagar_virtual(self,37,{});
      if (ours==tagar_virtual(other,37,{})) return 0;
      return (mem.load(other+0xa8,8)&0x100000000000ull) ? 4 : 1;
    }
    // Arena dispatch selects these by address, even at an unusual slot. Never
    // invent a callback for a modelled creature body outside this family.
    for (auto known:std::array<Raw,12>{0x27236c,0x272f4c,0x2a8b84,0x2a91b8,
        0x2c2440,0x2c382c,0x2c34e4,0x2c3c54,0x3c2738,0x10e658,0x10beb8,0x3c2708})
      if (body==known) throw Unsupported("arena creature body outside Tagar domain");
  }
  auto values=args;values.insert(values.begin(),static_cast<Raw>(self));
  return service_scalar(id,std::move(values));
}
Raw Model::tagar_valid(Address s,Address obj,bool marker) {
  if (!obj || (u32(obj+0xc)&9)) return 0;
  if (!tagar_virtual(u32(s+0xc),20,{static_cast<Raw>(obj)})) return 0;
  if (marker && service_scalar(FindSpellMarker,{static_cast<Raw>(obj),0x57,u32(s+0xc)})) return 0;
  return 1;
}
Raw Model::tagar_pick(Address s,bool later) {
  const auto swarm=vec(s+0x88);
  const auto heading=static_cast<Raw>(half(s+0x84));
  const auto found=later ? service(CollectInSphere,{swarm[0],swarm[1],swarm[2],heading,0x42f00000})
      : service(CollectObjectsInArc,{swarm[0],swarm[1],swarm[2],heading,u32(s+0xc),0x42f00000,0x42b40000,ZERO});
  Raw best=0,best_d=0x49742400;
  for (auto obj:found) {
    if (!tagar_valid(s,obj,true)) continue;
    bool present=false;
    for (int k=0;k<half(s+0x82);++k) {
      const Address node=u32(s+0x18+4*k);
      if (node && u32(node+0xc)==obj) {present=true;break;}
    }
    if (present) continue;
    if (!later) {service(HandleClearStack,{0});return obj;}
    const auto dx=sub(u32(s+0x88),u32(Address(obj)+0x10));
    const auto dy=sub(u32(s+0x8c),u32(Address(obj)+0x14));
    const auto dz=sub(u32(s+0x90),u32(Address(obj)+0x18));
    const auto d=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
    if (lt(d,best_d)) {best=obj;best_d=d;}
  }
  service(HandleClearStack,{0});return best;
}
Raw Model::tagar_tick(Address s) {
  unsigned active=0;
  for (int i=0;i<half(s+0x82);++i) {
    const Address node=u32(s+0x18+4*i),victim=node ? u32(node+0xc) : 0,slot=s+0x38+4*i;
    if (!victim || lt(u32(slot),ZERO)) continue;
    if (le(u32(slot),u32(s+0x78)) || !tagar_valid(s,victim,false)) {
      const Address marker=service_scalar(FindSpellMarker,{static_cast<Raw>(victim),0x57,u32(s+0xc)});
      if (marker) destroy(marker);
      write(slot,4,0xbf800000);continue;
    }
    write(victim+0x98,2,100);write(victim+0x9e,2,4);write(victim+0x9a,2,0);write(victim+0x9c,2,0);
    ++active;
    const Address tick=s+0x58+4*i;
    if (le(u32(tick),u32(s+0x78))) {
      const Raw owner=u32(s+0xc),rank=level(s),skill=u32(s+8);
      Raw low,high;
      if (owner && (u32(Address(owner)+0xc)&0x40)) {
        const auto range=skill_call(skill,8,{1,rank,0,spread20});
        if (range.size()!=2) throw Unsupported("Tagar range result shape");
        low=range[0];high=range[1];
      } else {
        const auto value=from_int(scalar(skill,12,{1,rank,owner,0})),spread=mul(value,spread20);
        high=static_cast<Raw>(to_int(add(value,spread)));low=static_cast<Raw>(to_int(sub(value,spread)));
      }
      service(DamageApply,{static_cast<Raw>(victim),u32(s+0xc),100000,0,0,0x4000000,0,0,from_int(low),from_int(high)});
      write(tick,4,add(u32(tick),ONE));
    }
    for (unsigned j=0;j<5;++j) {
      const auto frame=u32(0x44fdcc),sign=j&1 ? 0xbf800000 : ONE;
      const auto b=arena_ball(s+0x10);
      const Raw swirl=(frame+j*0x2aaa)<<9;
      auto px=mul(mul(sign,u32(victim+0x1c)),cos16(swirl));
      auto py=mul(mul(sign,u32(victim+0x1c)),sin16(swirl));
      const auto bob=sin16((frame-j*0x2aaa)<<10);
      auto pz=mul(mul(sign,bob),mul(u32(victim+0x20),0x3e800000));
      px=add(px,u32(victim+0x10));py=add(py,u32(victim+0x14));
      pz=add(add(pz,u32(victim+0x18)),mul(u32(victim+0x20),HALF));
      service(ParticleSpawn,{0x8e7598,px,py,pz,mul(b[0],0x40800000),mul(b[1],0x40800000),mul(b[2],0x40800000),ZERO});
    }
  }
  return active ? 1 : 0;
}
void Model::tagar_run(Address s) {
  const auto active=tagar_tick(s);
  auto clock=add(u32(s+0x78),dt);write(s+0x78,4,clock);
  if (le(u32(s+0x7c),clock)) {if (!active && s) destroy(s);return;}
  const Raw skill=u32(s+8);
  const auto limit=std::min(8,s32(scalar(skill,12,{0,level(s),u32(s+0xc),10})));
  if (!(half(s+0x82)<limit)) return;
  Raw target=0;const auto current=half(s+0x80);
  if (current>=0) {
    const Address node=u32(s+0x18+4*current);
    if (node) target=u32(node+0xc);
    if (!node || !tagar_valid(s,target,false)) write(s+0x80,2,0xffff);
  }
  if (half(s+0x80)<0) {
    target=tagar_pick(s,half(s+0x82)>0);
    if (target) {
      const Raw count=mem.load(s+0x82,2);write(s+0x80,2,count);
      service(HandleLinkStackCallback,{static_cast<Raw>(s+0x18+4*s16(count)),target,target});
      const auto dy=sub(u32(Address(target)+0x14),u32(s+0x8c));
      const auto dx=sub(u32(Address(target)+0x10),u32(s+0x88));
      const auto dist=sqrt(add(mul(dx,dx),mul(dy,dy)));
      write(s+0x38+4*half(s+0x80),4,add(u32(s+0x78),mul(dist,0x3ba1605b)));
    }
  }
  Raw speed=0x434b0da1;
  if (target) {
    auto dx=sub(u32(Address(target)+0x10),u32(s+0x88));
    auto dy=sub(u32(Address(target)+0x14),u32(s+0x8c));
    if (lt(ONE,add(mul(dx,dx),mul(dy,dy)))) {
      auto error=atan2_16(dy,dx)-half(s+0x84);
      if (error>=0x4000) error=0x7fff-error;
      else if (error < -0x3fff) error=-0x8000-error;
      write(s+0x84,2,static_cast<Raw>(mem.load(s+0x84,2)+error/5));
    }
    dx=sub(u32(Address(target)+0x10),u32(s+0x88));dy=sub(u32(Address(target)+0x14),u32(s+0x8c));
    const Address slot=s+0x38+4*half(s+0x80);
    const auto dist=sqrt(add(mul(dx,dx),mul(dy,dy)));
    const auto eta=add(u32(s+0x78),mul(dist,0x3ba1605b));
    if (lt(eta,u32(slot))) write(slot,4,eta);
    clock=u32(s+0x78);const auto left=sub(u32(slot),clock);
    speed=div(dist,left); // Arrival deliberately computes this before replacing it.
    if (lt(dt,left)) {
      const auto sz=u32(s+0x90),gap=sub(add(u32(Address(target)+0x18),0x42400000),sz);
      write(s+0x90,4,add(sz,div(gap,mul(left,0x42700000))));
    }
    if (le(u32(slot),add(clock,0x3d088888))) {
      write(s+0x82,2,mem.load(s+0x82,2)+1);
      const auto seconds=scalar(u32(s+8),12,{2,level(s),u32(s+0xc),10});
      const auto index=half(s+0x80);
      write(s+0x38+4*index,4,add(from_int(seconds),0x3d088888));
      write(s+0x58+4*index,4,u32(s+0x78));
      const Address marker=service_scalar(PoolNew,{0x20});
      if (marker) {
        const auto owner=u32(s+0xc);
        write(marker,1,1);write(marker+0x14,4,0x515ba0);write(marker+4,4,0);
        write(marker+0xc,4,target);write(marker+8,4,0);write(marker+0x10,4,draw());
        write(marker+0x14,4,0x51ba90);write(marker+0x18,4,owner);write(marker+0x1c,4,0x57);
      }
      service(CreatureAddSpell,{target,static_cast<Raw>(marker)});
      speed=0x434b0da1;sound(0x552428,0x552460,vec(Address(target)+0x10));
      write(s+0x80,2,0xffff);write(s+0x7c,4,add(u32(s+0x7c),0x3f0041aa));
    }
  }
  if (own_draw(s+0x10)<0x4906) {
    const auto turn=sub(mul(from_int(own_draw(s+0x10)),TWO_M16),HALF);
    write(s+0x86,2,static_cast<Raw>(to_int(mul(turn,0x45f81000))));
  }
  const auto angle=static_cast<Raw>(s16(mem.load(s+0x84,2)+mem.load(s+0x86,2)));
  write(s+0x88,4,add(u32(s+0x88),mul(mul(cos16(angle),speed),dt)));
  const auto sine=sin16(angle);auto grow=mul(u32(s+0x78),0x40226fa7);
  write(s+0x8c,4,add(u32(s+0x8c),mul(mul(sine,speed),dt)));
  if (lt(ONE,grow)) grow=ONE;
  for (unsigned k=0;k<50;++k) {
    const auto b=arena_ball(s+0x10);const auto factor=mul(mul(grow,0x436d90b9),dt);
    auto c=arena_ball(s+0x10);
    const auto inverse=rsqrt(ONE,add(add(mul(c[0],c[0]),mul(c[1],c[1])),mul(c[2],c[2])));
    for (auto &v:c) v=mul(v,inverse);
    const auto reach=mul(mul(grow,0x4151d268),mul(from_int(own_draw(s+0x10)),TWO_M16));
    const auto kind=own_draw(s+0x10);
    service(ParticleSpawn,{kind>=0x7d01 ? 0x8e7598u : kind>=0x3a99 ? 0x8e75e8u : 0x8e7638u,
        add(mul(c[0],reach),u32(s+0x88)),add(mul(c[1],reach),u32(s+0x8c)),add(mul(c[2],reach),u32(s+0x90)),
        mul(b[0],factor),mul(b[1],factor),mul(b[2],factor),ZERO});
  }
  const Raw ring=0x4151d268,lean=0x3f0f56fb,spin=u32(0x44fdcc)<<10;
  const auto side=neg(sin16(static_cast<Raw>(half(s+0x84))));
  const auto px=mul(mul(side,lean),mul(cos16(spin),ring));
  const auto ahead=mul(cos16(static_cast<Raw>(half(s+0x84))),lean);
  const auto py=mul(ahead,mul(cos16(spin),ring));
  const auto pz=mul(mul(sin16(spin),ring),lean);
  service(ParticleSpawn,{0x8e7688,add(px,u32(s+0x88)),add(py,u32(s+0x8c)),add(pz,u32(s+0x90)),ZERO,ZERO,ZERO,ZERO});
}

std::vector<Raw> Model::skeleton_virtual(Address obj,unsigned slot,const std::vector<Raw> &args) {
  const Address table=u32(obj+0x48), self=static_cast<Raw>(obj+half(table+slot*8));
  const Raw body=u32(table+slot*8+4);
  const unsigned id=slot==16 ? CreatureModel : slot==34 ? CreatureSetHomeRange : CreatureGetTeam;
  if (!external.count(id) && !named_external.count("Creature.v"+std::to_string(slot))) {
    if (body==0x3c2738) {
      if (!args.empty()) throw Unsupported("arena model virtual shape");
      return {u32(Address(u32(self+0x24))+0xc)};
    }
    if (body==0x3c2708) {
      if (args.size()!=1) throw Unsupported("arena home virtual shape");
      write(self+0x3c0,4,from_int(args[0]&65535));return {};
    }
    if (slot==37 && body==0x3c2730) return {static_cast<Raw>(signed_byte(mem.load(self+0x310,1)))};
    for (auto known:std::array<Raw,10>{0x27236c,0x272f4c,0x2a8b84,0x2a91b8,0x2c2440,0x2c382c,0x2c34e4,0x2c3c54,0x10e658,0x10beb8})
      if (body==known) throw Unsupported("arena creature body outside Skeleton construction domain");
  }
  auto values=args;values.insert(values.begin(),static_cast<Raw>(self));return service(id,std::move(values));
}
// Inherited arena construction: no type-name statistics branch is reached here.
void Model::skelight_ctor(Address c,const std::vector<Raw> &pos,Raw facing,Address props,Raw rank) {
  service(GameObjectConstructor,{static_cast<Raw>(c),pos[0],pos[1],pos[2],0x502728,static_cast<Raw>(props)});
  write(c+0x50,4,0);write(c+0x48,4,0x50cc58);write(c+0x4c,4,0x50d010);write(c+0x119,1,8);
  for (auto off:{0x2f4,0x2ec,0x2f0,0x234,0x238,0x2e4,0x360,0x364}) write(c+off,4,0);
  write_vec(c+0x3b4,pos);write(c+0x38,2,facing);write(c+0x22c,4,0x41200000);
  write(c+0x220,8,UINT64_MAX);write(c+0xc,4,u32(c+0xc)|0x20);write(c+0xc8,4,0);
  write(c+0x228,4,0x41200000);write(c+0x23c,4,UINT32_MAX);write(c+0x240,4,UINT32_MAX);
  for (auto off:{0x2bc,0x2c0,0x2dc,0xf4,0x2c4,0x300}) write(c+off,4,0);
  for (auto off:{0x110,0x31c,0x312,0x316}) write(c+off,2,0);
  for (auto off:{0x231,0x2fc}) write(c+off,1,0);
  write(c+0x310,1,255);write(c+0x5a,1,255);write(c+0xa0,1,0);
  write(c+0x3c,4,u32(c+0x18));write(c+0xdc,4,ONE);write(c+0x58,2,65535);write(c+0x3c4,2,600);
  for (auto off:{0xb8,0x304,0xe0,0x2d8}) write(c+off,4,0);
  for (auto off:{0x314,0x31a,0x318}) write(c+off,2,0);
  const auto serial=mem.load(0x4f85c4,1);write(0x4f85c4,1,serial+1);write(c+0x230,1,serial);
  const auto prop=[&](Raw name) {return service_scalar(ObjectPropertyInt,{static_cast<Raw>(c),name,0});};
  const auto str=[&](Raw name) {return service_scalar(ObjectPropertyString,{static_cast<Raw>(c),name,0});};
  if (prop(0x52f9f0)) write(c+0x231,1,mem.load(c+0x231,1)|1);
  if (prop(0x52f9f8)) {write(c+0x231,1,mem.load(c+0x231,1)|8);service(ObjectMoveOffGrid,{static_cast<Raw>(c)});write(c+0xc,4,u32(c+0xc)|0x8000);}
  if (str(0x52fa00)) {write(c+0x368,4,str(0x52fa00));write(c+0xc,4,u32(c+0xc)|0x100000);}
  if (prop(0x52fa10)) write(c+0xc8,4,0x447a0000);
  write(c+0x3c0,4,0x447a0000);
  if (prop(0x52fa18)) write(c+0x3c0,4,from_int(prop(0x52fa18)));
  for (auto entry:std::array<std::array<Raw,3>,3>{{{0x3ac,0x52fa28,300},{0x3ae,0x52fa38,500},{0x3b0,0x52fa48,0x4000}}}) {
    write(c+entry[0],2,entry[2]);if (prop(entry[1])) write(c+entry[0],2,prop(entry[1]));
  }
  if (prop(0x52fa58)) write(c+0xdc,4,mul(from_int(prop(0x52fa58)),percent));
  if (prop(0x52fa60)) write(c+0xa8,8,mem.load(c+0xa8,8)|1);
  if (prop(0x52fa70)) write(c+0xa8,8,mem.load(c+0xa8,8)|0x1000000000000ull);
  if (str(0x52fa80)||str(0x52fa88)||str(0x52fa90)) write(c+0xa8,8,mem.load(c+0xa8,8)|0x40000000);
  write(c+0x2fa,2,mem.load(c+0x38,2));
  const Address holder=c+0x3c8;
  write(c+0x48,4,0x5174b0);write(holder,4,0x512148);write(holder+4,1,0);
  write(holder,4,0x50cf40);write(holder+8,1,0);
  for (unsigned k=0;k<4;++k) write(holder+0x74+8*k,2,65535);
  write(holder+0x71,1,255);write(holder+0x70,1,255);
  for (unsigned k=0;k<4;++k) {write(holder+0x60+2*k,2,0);write(holder+0x68+2*k,2,0);}
  write(c+0x28,1,20);write(c+0xb0,4,0x3fc00000);write(c+0x1c,4,0x41400000);
  write(c+0xb8,4,0);write(c+0x232,2,0);write(c+0xec,4,u32(0x44fdcc));
  write(c+0xa8,8,mem.load(c+0xa8,8)|0x10000);write(c+0x68,4,ONE);write(c+0x64,4,ONE);write(c+0x50,4,holder);
  service(HolderSetModel,{static_cast<Raw>(holder+half(Address(u32(holder))+8)),1,4});
  write(c+0x460,4,rank);const auto saved=u32(0x4f714c);write(0x4f714c,4,from_int(rank));
  const auto lower_string=[&](Address addr) {
    std::vector<unsigned> result;
    for (unsigned k=0;k<64;++k) {auto b=static_cast<unsigned>(mem.load(addr+k,1));if (!b) break;if (b>='A'&&b<='Z') b+=32;result.push_back(b);}return result;
  };
  const auto same=lower_string(0x44fe18)==lower_string(0x54c080);
  service(CreatureSetStats,{static_cast<Raw>(c),1,0x3faa3d70,0x3f999999,0x3f999999,ONE,same ? ZERO : ONE});
  write(0x4f714c,4,saved);write(c+0xe8,4,0);
  auto mask=service(ModelBuildHideMask,{u32(Address(u32(c+0x24))+0xc),0});
  if (mask.size()!=1 && mask.size()!=2) throw Unsupported("hide mask raw64 result shape");
  write(c+0x220,8,std::uint64_t(mask[0])|(mask.size()==2 ? std::uint64_t(mask[1])<<32 : 0));
  service(AnimationClear,{static_cast<Raw>(c+0x114)});
  service(AnimationPush,{static_cast<Raw>(c+0x114),u32(Address(u32(c+0x24))+0x3c),12});
  // Only the constructor's state 0x17 and its exact filter outcomes are reachable.
  if (!(u32(0x4f7120)&&!u32(0x4f711c)&&u32(0x503890)&&!u32(0x503894))) {
    unsigned state=0x17;
    if (half(c+0x314)) state=u32(c+0x234) ? 5 : 0;
    else if (half(c+0x31c)) state=0x13;
    else {if (!u32(c+0x234)) write(c+0x318,2,0);if (half(c+0x318)) state=0x10;}
    if (!state) {service(HandleClearStack,{static_cast<Raw>(c+0x234)});write(c+0x234,4,0);}
    if (state==0 || state==5 || state==0x13) {
      service(AnimationStart,{static_cast<Raw>(c+0x114),u32(Address(u32(c+0x24))+0x34),3,0x3dcccccc});
      if (!state) write(c+0xf4,4,u32(c+0x300)+60);
    } else if (state==0x10) {
      service(AnimationStart,{static_cast<Raw>(c+0x114),u32(Address(u32(c+0x24))+0x44),0x803,0x3dcccccc});
      service(AnimationSync,{static_cast<Raw>(c+0x114)});
    }
    write(c+0x232,2,state);
  }
  service(ObjectRunEveryFrame,{static_cast<Raw>(c)});
  for (unsigned entry:{3,4}) for (unsigned k=0;k<4;++k) {
    const Address record=u32(Address(u32(c+0x24))+8*entry+4);
    write(record+0x18+8*k,1,k==0 || (entry==4 && k==1) ? 1 : 0);
    write(record+0x14+8*k,4,k==0 ? 0x3e999999u : entry==4 && k==1 ? ONE : ZERO);
  }
  service(CreatureInit,{static_cast<Raw>(c)});write(c+0xb4,1,16);
  write(c+0xa8,8,mem.load(c+0xa8,8)|0x8000000000000000ull);
}
std::vector<Raw> Model::skeleton_sample(Address owner,Raw count) {
  const auto model=skeleton_virtual(owner,16,{}).at(0), roll=own_draw(0x404460);
  const auto denominator=std::int64_t(s32(count));
  if (!denominator) throw Unsupported("reference cosmetic sample divides by zero");
  const auto at=vec(owner+0x10);
  auto point=service(SamplePoint,{model,static_cast<Raw>(owner+0x114),at[0],at[1],at[2],static_cast<Raw>(half(owner+0x38)),
      static_cast<Raw>(roll%(denominator<0 ? -denominator : denominator)),0,0,ONE});
  if (point.size()!=3) throw Unsupported("cosmetic sample point shape");return point;
}

std::vector<std::uint32_t> Model::execute(Operation op,
                                          const std::vector<std::uint32_t> &a) {
  const auto n = static_cast<unsigned>(op);
  constexpr std::array<unsigned, 188> sizes{
      {3, 4, 5, 5, 3, 3, 5, 2, 5, 2, 2, 4, 4, 4, 2, 2, 2, 2, 5, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 2,5,4,5,2, 2,5,4,3,3,1, 2,5,4,3,3, 2,1,1,6,18,6,5,1, 2,7,1,4,10,3,7,1,1,0,4,4, 5,4,2,2,5,4,2,2,3, 2,7,7,7,3,5,3,3,2,0, 2,5,2,4,2,3,3,11, 2,2,2,7,7,7, 2,5,4,3,2,5,3,4, 7,7,8,14,1,1,2,11, 2,5,1,4,4,5, 2,7,2,4,3,2,3, 7,4,4,3,1, 2,5,7,3,2, 2,5,4,8,3,2, 7,4,2,3,2, 7,8,1,2,1, 2,7,5,3, 2,7,4,3, 7,4,5, 3,1, 0,2,2,0,0,0,0,7,4,3,2,1,3,2,7,5,3,2,1,1,7}};
  if (n >= sizes.size() || (op==Operation::ShoutAnimation ? a.size()<5 : op==Operation::ItemRecord ? a.size()<3 : op==Operation::ShapeResult || op==Operation::Descriptor || n>=171 ? a.empty() : a.size()!=sizes[n]))
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
  case Operation::SkeletonCanUse: {
    if (a.size()!=2) throw Unsupported("Skeleton can-use shape");
    Address name;
    if (u32(Address(a[1])+0xc)&0x40) {
      const auto slot=u32(Address(a[1])+0x3c8);if (slot>=4) return {0};name=u32(0x503778+4*slot);
    } else name=u32(0x503788);
    return {service_scalar(WorldFindObjectByName,{static_cast<Raw>(name)}) ? 0u : 1u};
  }
  case Operation::CosmeticDraw:
    if (a.size()!=1 || a[0]) throw Unsupported("cosmetic draw requires zero marker");
    return {own_draw(0x404460)};
  case Operation::CosmeticBall:
    if (a.size()!=1 || a[0]) throw Unsupported("cosmetic ball requires zero marker");
    return arena_ball(0x404460);
  case Operation::SkeletonSample:
    if (a.size()!=2) throw Unsupported("Skeleton sample shape");
    return skeleton_sample(s,a[1]);
  case Operation::SkelightCtor:
    if (a.size()!=7) throw Unsupported("Skelight constructor shape");
    skelight_ctor(s,{a[1],a[2],a[3]},a[4],a[5],a[6]);return {};
  case Operation::SkeletonCtor:
    if (a.size()!=5) throw Unsupported("Skeleton spell constructor shape");
    arena_spell_base(s,a[1],a[2],a[3],0x519918);write(s+0x18,4,0);
    if (a[4]) service(HandleLinkStackCallback,{static_cast<Raw>(s+0x18),a[4],a[4]});return {};
  case Operation::SkeletonRun:
    if (a.size()!=3) throw Unsupported("Skeleton run shape");
    if (!u32(s+0x18) && s) table_call("Spell",s,0x14,1,{3});return {};
  case Operation::SkeletonCast: {
    if (a.size()!=7) throw Unsupported("Skeleton cast shape");
    const Address owner=a[2];const auto facing=static_cast<Raw>(s16(a[6]));
    write(owner+0x9a,2,0xff92);write(owner+0x9c,2,0xff80);write(owner+0x9e,2,30);write(owner+0x98,2,0xff80);
    const auto count=service_scalar(SamplePointCount,{skeleton_virtual(owner,16,{}).at(0)});
    if (count) {
      for (unsigned k=0;k<232;++k) {
        auto point=skeleton_sample(owner,count);auto v=arena_ball(0x404460);
        if (k<32) {
          for (unsigned j=0;j<3;++j) point[j]=add(point[j],mul(v[j],0x3e72af9a));
          v=arena_ball(0x404460);
        }
        const Raw speed=k<32 ? 0x3d8b4634 : 0x41c000e5;
        service(ParticleSpawn,{k<32 ? 0x8e5c80u : 0x8e5cd0u,point[0],point[1],point[2],mul(v[0],speed),mul(v[1],speed),mul(v[2],speed),ZERO});
      }
    }
    const auto x=add(a[3],mul(cos16(facing),0x42900000)), y=add(a[4],mul(sin16(facing),0x42900000));
    const auto z=service_scalar(TerrainHeight,{u32(0x44fdc8),x,y,a[5],0x41400000});
    std::vector<Raw> spot{x,y,z};
    // The reference compares the rounded float difference as a double.
    const auto difference=sub(z,u32(owner+0x18));
    if (owner && (difference&0x7fffffffu)>0x41c00000u && (difference&0x7fffffffu)<=0x7f800000u) {
      const auto at=vec(owner+0x10);
      auto found=service(WorldFindFreeSpot,{at[0],at[1],at[2],72,12,0});
      if (found.size()!=4) throw Unsupported("free spot result shape");
      if (signed_byte(found[3])) spot={found[0],found[1],found[2]};
    }
    const auto d1=mul(from_int(draw()),TWO_M16);
    const auto strength=scalar(s,12,{0,a[1],a[2],10});
    Raw props=0x503770;
    if (u32(owner+0xc)&0x40) props=u32(owner+0x3c8)<4 ? 0x503750+8*u32(owner+0x3c8) : 0;
    const auto skel=service_scalar(PoolNew,{0x468});
    if (skel) {
      skelight_ctor(skel,spot,facing,props,strength);write(Address(skel)+0xc,4,u32(Address(skel)+0xc)|0x4000);
      const auto chance=scalar(s,12,{1,a[1],a[2],10});
      if (lt(d1,mul(from_int(chance),percent))) {
        const auto pick=draw()%3;const Address holder=u32(Address(skel)+0x50);
        if (holder) {
          const auto slot=2+pick*2;
          if (slot==2) table_call("Holder",holder,0,2,{0});
          else service(slot==4 ? HolderElement4 : HolderElement6,{static_cast<Raw>(holder+half(Address(u32(holder))+8*slot)),0});
        }
      }
      if ((u32(owner+0xc)&0x40) && s32(a[1])>0) {
        write(Address(skel)+0x5a,1,mem.load(owner+0x3c8,1));
        write(Address(skel)+0xa8,8,mem.load(Address(skel)+0xa8,8)|0x400000);
        const auto team=skeleton_virtual(owner,37,{}).at(0);service(CreatureSetCharm,{skel,UINT32_MAX,team});
        write(Address(skel)+0x3ac,2,400);
        skeleton_virtual(skel,34,{500});
      }
    }
    sound(0x54fdd0,0x54fdd8,{a[3],a[4],a[5]});
    if (!skel) return {0};const auto spell=service_scalar(PoolNew,{0x1c});
    if (spell) execute(Operation::SkeletonCtor,{spell,a[1],a[2],static_cast<Raw>(s),skel});return {spell};
  }
  case Operation::TagarCast: {
    if (a.size()!=7) throw Unsupported("Tagar cast shape");
    const auto rank=scalar(s,14,{a[1],20});sound(0x552428,0x552430,{a[3],a[4],a[5]});
    const auto block=service_scalar(PoolNew,{0x94});
    if (block) execute(Operation::TagarCtor,{block,rank,a[2],static_cast<Raw>(s)});
    return {block};
  }
  case Operation::TagarCtor: {
    if (a.size()!=4) throw Unsupported("Tagar constructor shape");
    arena_spell_base(s,a[1],a[2],a[3],0x51ba60);
    for (unsigned k=0;k<8;++k) write(s+0x18+4*k,4,0);
    write(s+0x80,2,0xffff);write(s+0x82,2,0);write(s+0x7c,4,0x4000101f);write(s+0x78,4,0);
    write_vec(s+0x88,vec(Address(u32(s+0xc))+0x10));write(s+0x86,2,0);
    write(s+0x84,2,mem.load(Address(u32(s+0xc))+0x38,2));
    write(s+0x88,4,add(u32(s+0x88),mul(cos16(static_cast<Raw>(half(s+0x84))),0x41c00000)));
    const auto y=add(u32(s+0x8c),mul(sin16(static_cast<Raw>(half(s+0x84))),0x41c00000));
    write(s+0x90,4,add(u32(s+0x90),0x42400000));write(s+0x8c,4,y);return {};
  }
  case Operation::TagarValid:
    if (a.size()!=3) throw Unsupported("Tagar valid shape");
    return {tagar_valid(s,a[1],a[2]!=0)};
  case Operation::TagarPickTarget:
    if (a.size()!=2) throw Unsupported("Tagar pick shape");
    return {tagar_pick(s,a[1]!=0)};
  case Operation::TagarTickVictims:
    if (a.size()!=1) throw Unsupported("Tagar tick shape");
    return {tagar_tick(s)};
  case Operation::TagarRun:
    if (a.size()!=3) throw Unsupported("Tagar run shape");
    tagar_run(s);return {};
  case Operation::NamedCall: {
    ValueReader reader{a};
    const auto name=reader.string(), ints=reader.string();
    const auto ret=reader.word();
    if (ret>127) throw Unsupported("dispatch return kind domain");
    const auto count=reader.word();
    std::vector<V> args;
    for (Raw k=0;k<count;++k) args.push_back(reader.value());
    reader.end();
    signatures[name]={ints,static_cast<char>(ret)};
    std::vector<Raw> out;encode_value(out,call(name,std::move(args)));return out;
  }
  case Operation::SkillCall: {
    if (a.size()<2) throw Unsupported("skill dispatch shape");
    if (a[1]<=15) return skill_call(a[0],a[1],{a.begin()+2,a.end()});
    return invoke(a[0],a[1],{a.begin()+2,a.end()});
  }
  case Operation::TableCall: {
    ValueReader reader{a};
    const auto cls=reader.string();
    const auto object=reader.word(), offset=reader.word(), slot=reader.word();
    return table_call(cls,object,offset,slot,{a.begin()+reader.at,a.end()});
  }
  case Operation::SignedDivide:
  case Operation::SignedRemainder: {
    const auto value=op==Operation::SignedDivide ? c_div(s32(a[0]),s32(a[1])) : c_rem(s32(a[0]),s32(a[1]));
    const auto bits=static_cast<std::uint64_t>(value);
    return {static_cast<Raw>(bits),static_cast<Raw>(bits>>32)};
  }
  case Operation::Descriptor: {
    std::size_t at=1;
    std::vector<std::vector<Raw>> slots;
    for (Raw k=0;k<a[0];++k) {
      if (at>=a.size() || a[at]>a.size()-at-1) throw Unsupported("descriptor slot shape");
      const auto size=a[at++];
      slots.emplace_back(a.begin()+at,a.begin()+at+size); at+=size;
    }
    if (at!=a.size()) throw Unsupported("descriptor trailing words");
    const auto value=desc(slots);
    std::vector<Raw> out(value.header.begin(),value.header.end());
    out.push_back(static_cast<Raw>(value.slots.size()));
    for (const auto &slot:value.slots) {
      out.push_back(static_cast<Raw>(slot.size())); out.insert(out.end(),slot.begin(),slot.end());
    }
    return out;
  }
  case Operation::ShapeResult: {
    using V=ResultValue;
    std::size_t at=0;
    const auto word=[&]() {if (at>=a.size()) throw Unsupported("structured result truncated"); return a[at++];};
    const char ret=static_cast<char>(word());
    const auto length=word();
    std::string ints;
    if (length>a.size()-at) throw Unsupported("signature truncated");
    for (Raw k=0;k<length;++k) ints.push_back(static_cast<char>(word()));
    std::function<V(unsigned)> read=[&](unsigned depth) -> V {
      if (depth>=64) throw Unsupported("structured result depth");
      const auto kind=word();
      if (kind>V::Buffer) throw Unsupported("structured result kind");
      V v; v.kind=static_cast<V::Kind>(kind);
      if (kind==V::Scalar) v.scalar=word();
      else if (kind!=V::None) {
        const auto count=word();
        if (count>a.size()-at) throw Unsupported("structured result count");
        for (Raw k=0;k<count;++k) v.values.push_back(read(depth+1));
        if (kind==V::Buffer && count!=1) throw Unsupported("buffer result needs one child");
        if (kind==V::Bytes) {
          for (const auto &byte:v.values)
            if (byte.kind!=V::Scalar || byte.scalar>255) throw Unsupported("byte result outside byte domain");
        }
      }
      return v;
    };
    const auto count=word();
    std::vector<V> args;
    for (Raw k=0;k<count;++k) args.push_back(read(0));
    const auto integer=read(0), floating=read(0);
    const auto outputs=word();
    std::vector<V> outs;
    for (Raw k=0;k<outputs;++k) outs.push_back(read(0));
    if (at!=a.size()) throw Unsupported("structured result trailing words");
    const std::map<std::string,ExternalResultSignature> registry{{"supplied",{ints,ret}}};
    const auto value=shape_result(registry,"supplied",args,integer,floating,outs);
    std::vector<Raw> out;
    std::function<void(const V &)> emit=[&](const V &v) {
      out.push_back(v.kind);
      if (v.kind==V::Scalar) {
        if (v.scalar>UINT32_MAX) throw Unsupported("raw64 result outside raw32 transport");
        out.push_back(static_cast<Raw>(v.scalar));
      }
      else if (v.kind!=V::None) {
        out.push_back(static_cast<Raw>(v.values.size()));
        for (const auto &child:v.values) emit(child);
      }
    };
    emit(value); return out;
  }
  case Operation::ConeOfShockCast: {
    const auto rank=scalar(s,14,{a[1],1});
    const auto spell=service_scalar(PoolNew,{0x58});
    if (spell) execute(Operation::ConeOfShockCtor,{spell,rank,a[2],a[0]});
    return {spell};
  }
  case Operation::ConeOfShockCtor: {
    const auto place=std::vector<Raw>{add(u32(Address(a[2])+0x10),ZERO),
        u32(Address(a[2])+0x14),add(u32(Address(a[2])+0x18),0x42700000)};
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51c6f8});
    execute(Operation::LightRecord,{static_cast<Raw>(s+0x18),0x6f,0x70,0x9a,0,
        0x431c0000,0x382c598a,ZERO,place[0],place[1],place[2]});
    service(LightLink,{static_cast<Raw>(s+0x18)});
    const auto handle=sound(0x553408,0x553410,vec(Address(u32(s+0xc))+0x10));
    if (handle.size()!=1) throw Unsupported("sound handle result must be scalar");
    write(s+0x54,4,handle[0]);
    return {};
  }
  case Operation::ConeOfShockRun: {
    const auto facing=static_cast<Raw>(s16(a[4]));
    const auto arc=from_int(scalar(u32(s+8),12,{2,level(s),u32(s+0xc),10}));
    const auto arc_angle=static_cast<Raw>(s16(to_int(mul(arc,0x43360b60))));
    const auto reach=mul(from_int(scalar(u32(s+8),12,{1,level(s),u32(s+0xc),25})),0x41400000);
    const auto frame=s32(u32(0x44fdcc));
    const auto denominator=s32(level(s))/4*2+2;
    if (!denominator) throw Unsupported("reference decoration period divides by zero");
    const auto period=60/denominator;
    if (!period) throw Unsupported("reference decoration remainder divides by zero");
    if (std::int64_t(frame)%period==0) {
      const auto spread=sub(mul(from_int(own_draw(s+0x10)),TWO_M16),HALF);
      const auto angle=static_cast<Raw>(s16(facing+to_int(mul(from_int(arc_angle),spread))));
      const auto cosine=cos16(angle), sine=sin16(angle), length=add(reach,0x41400000);
      const auto near=std::vector<Raw>{add(a[1],mul(cosine,0x41400000)),add(a[2],mul(sine,0x41400000)),a[3]};
      const auto far=std::vector<Raw>{add(a[1],mul(cosine,length)),add(a[2],mul(sine,length)),a[3]};
      const auto block=service_scalar(PoolNew,{0x60});
      if (block) service(LightningBoltConstructor2,{block,far[0],far[1],far[2],near[0],near[1],near[2],11,0});
    }
    if (s32(u32(0x44fdcc))%15==0) {
      const auto found=service(CollectObjectsInArc,{sub(a[1],mul(cos16(facing),0x41400000)),
          sub(a[2],mul(sin16(facing),0x41400000)),a[3],facing,0,reach,add(arc,0x41200000),ZERO});
      bool hit=false;
      for (const auto obj:found) {
        const auto owner=u32(s+0xc);
        if (!service_scalar(CreatureEnemy,{static_cast<Raw>(Address(owner)+half(Address(u32(Address(owner)+0x48))+160)),obj})) continue;
        const auto amount=from_int(scalar(u32(s+8),12,{0,level(s),u32(s+0xc),0}));
        const auto quarter=to_int(mul(amount,0x3e800000));
        const auto share=mul(from_int(own_draw(s+0x10)),TWO_M16);
        const auto top=quarter+to_int(mul(mul(from_int(quarter),share),spread20));
        std::vector<Raw> args{obj,u32(s+0xc),100000,0,0,0x40};
        args.insert(args.end(),16,0); args.insert(args.end(),{255,255,0x18,static_cast<Raw>(quarter)&0xffff,static_cast<Raw>(top)&0xffff,0});
        for (unsigned k=0;k<3;++k) args.insert(args.end(),{0xffff,0,0,0});
        args.insert(args.end(),{0,ZERO,ZERO}); service(DamageApply,std::move(args)); hit=true;
      }
      if (hit) sound(0x553408,0x553428,vec(Address(u32(s+0xc))+0x10));
      service(HandleClearStack,{0});
    }
    const auto glow=from_int(to_int(mul(sin16(u32(0x44fdcc)*1000u),0x40c00000))+0x6c);
    write(s+0x34,4,glow); write(s+0x3c,4,glow);
    const auto falloff=div(ONE,mul(glow,glow));
    write(s+0x40,4,falloff); write(s+0x38,4,falloff);
    service(LightRelink,{static_cast<Raw>(s+0x18)}); return {};
  }
  case Operation::RootCanUse:
    if (!u32(0x4f711c) || !(u32(Address(a[1])+0xc)&0x40)) return {1};
    {
      const auto text=service_scalar(TextLookup,{0x550178});
      service(HudShowMessage,{text,0x5a,u32(Address(a[1])+0x3c8),0});
      return {0};
    }
  case Operation::RootCast: {
    const auto facing=static_cast<Raw>(s16(a[6]));
    const auto rank=scalar(s,14,{a[1],20});
    const auto clip=service_scalar(ResourceFind,{0x550188,0x550190});
    service(SoundPlayOnObject,{clip,a[2],ONE});
    const auto most=s32(scalar(s,10,{0,rank}));
    const auto found=service(CollectObjectsInArc,{a[3],a[4],a[5],facing,0,0x43780000,0x42f00000,ZERO});
    std::int64_t taken=0;
    for (const auto obj:found) {
      if (!(taken<most)) break;
      if (a[2] && !service_scalar(CreatureEnemy,{static_cast<Raw>(Address(a[2])+half(Address(u32(Address(a[2])+0x48))+160)),obj})) continue;
      if ((u32(Address(obj)+0xc)&0x1000000) || service_scalar(CreatureBossRank,{obj}) || !(half(Address(obj)+0x314)<1)) continue;
      const auto spell=service_scalar(PoolNew,{0x34});
      if (spell) execute(Operation::RootCtor,{spell,rank,obj,a[0]});
      service(CreatureAddSpell,{obj,spell}); ++taken;
    }
    service(HandleClearStack,{0}); service(HandleClearStack,{0});
    return {0};
  }
  case Operation::RootCtor:
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x519b70});
    write(s+0x18,4,0); write(s+0x1c,4,0);
    write(s+0x24,4,service_scalar(ParticleGroupCreate,{}));
    write_vec(s+0x28,vec(Address(a[2])+0x10));
    write(s+0x20,4,scalar(u32(s+8),10,{1,level(s)}));
    return {};
  case Operation::RootRun: {
    constexpr Raw delay=0x3f04dcca;
    write(s+0x18,4,add(u32(s+0x18),dt));
    const auto phase=u32(s+0x1c);
    if (!phase) {
      const auto victim=u32(s+0xc);
      // The matrix is callback-owned stack state; the reference exposes only its sentinel.
      const auto at=vec(Address(victim)+0x10);
      mat34_set_pos_rot_z({at[0],at[1],at[2]},static_cast<Raw>(half(Address(victim)+0x38)));
      service(ParticleGroupBegin,{u32(s+0x24)});
      service(ParticleGroupSetMatrix,{u32(s+0x24),0});
      for (unsigned i=0;i<7;++i) {
        std::array<Raw,2> around{};
        for (auto &v:around) {
          const auto share=add(mul(from_int(own_draw(s+0x10)),0x37800000),0xbf000000);
          v=mul(mul(share,u32(Address(u32(s+0xc))+0x1c)),0x3f9fdebd);
        }
        service(ParticleSpawn,{0x8e5f00,around[0],around[1],add(u32(Address(u32(s+0xc))+0x20),0x41400000),ZERO,ZERO,0xc2980000,ZERO});
      }
      service(ParticleGroupEnd,{u32(s+0x24)});
      if (lt(delay,u32(s+0x18))) {
        const auto roll=draw();
        const auto chance=s32(scalar(u32(s+8),10,{2,level(s)}));
        const Address target=u32(s+0xc);
        if (chance<=static_cast<std::int32_t>(roll%100+1)) {
          const std::vector<Raw> point{u32(target+0x10),u32(target+0x14),add(u32(target+0x18),u32(target+0x20))};
          for (unsigned i=0;i<100;++i) {
            const auto v=ball(s+0x10);
            service(ParticleSpawn,{0x8e5f50,point[0],point[1],point[2],mul(v[0],0x41a80000),mul(v[1],0x41a80000),mul(v[2],0x41a80000),ZERO});
          }
          destroy(s); return {};
        }
        flash(target,1,{4,0x5c,0,0x51}); flash(target,2,{4,0x5c,0,0x51});
        if (!(u32(target+0xc)&0x40)) write(target+0x314,2,u32(s+0x20)*60u);
        write(s+0x1c,4,u32(s+0x1c)+1);
      }
    } else if (phase==1) {
      const auto points=s32(service_scalar(SamplePointCount,{creature_model(u32(s+0xc))}));
      for (unsigned i=0;i<22;++i) {
        const auto model=creature_model(u32(s+0xc));
        const auto roll=own_draw(s+0x10);
        if (!points) throw Unsupported("reference sample-point remainder divides by zero");
        const auto index=static_cast<Raw>(std::int64_t(roll)%points);
        const Address target=u32(s+0xc);
        const auto at=vec(target+0x10);
        const auto point=service(SamplePoint,{model,static_cast<Raw>(target+0x114),at[0],at[1],at[2],static_cast<Raw>(half(target+0x38)),index,1,0,ONE});
        if (point.size()!=3) throw Unsupported("sample point result must be three raw floats");
        ball(s+0x10); // Required discarded draw sequence.
        service(ParticleSpawn,{0x8e5eb0,point[0],point[1],point[2],ZERO,ZERO,ZERO,ZERO});
      }
      const Address target=u32(s+0xc);
      if (u32(target+0xc)&0x40) {
        const auto dx=sub(u32(target+0x10),u32(s+0x28)),dy=sub(u32(target+0x14),u32(s+0x2c));
        write(target+0x2e8,4,mul(sqrt(add(mul(dx,dx),mul(dy,dy))),0x40800000));
        const auto angle=atan2_16(sub(u32(target+0x10),u32(s+0x28)),sub(u32(target+0x14),u32(s+0x2c)));
        write(Address(u32(s+0xc))+0x2e0,2,0xffffc000u-angle);
      }
    }
    if (lt(from_int(u32(s+0x20)),sub(u32(s+0x18),delay)) && s) destroy(s);
    return {};
  }
  case Operation::SummonBadgerCanUse: {
    auto name=u32(0x503908);
    if (u32(Address(a[1])+0xc)&0x40) {
      const auto slot=u32(Address(a[1])+0x3c8);
      if (slot>3) return {0};
      name=u32(0x5038f8+4*slot);
    }
    return {service_scalar(WorldFindObjectByName,{name}) ? 0u : 1u};
  }
  case Operation::SummonBadgerCast: {
    const auto rank=scalar(s,14,{a[1],2});
    const auto sx=add(a[3],mul(mul(cos16(a[6]),0x40bfff9d),0x41400000));
    const auto sy=add(a[4],mul(mul(sin16(a[6]),0x40bfff9d),0x41400000));
    const auto sz=service_scalar(TerrainHeight,{u32(0x44fdc8),sx,sy,a[5],0x42400000});
    std::vector<Raw> spot{sx,sy,sz};
    // Python converts the rounded binary32 difference to double before abs and
    // comparison. Ordered magnitude comparison preserves infinities and NaNs.
    const auto magnitude=sub(sz,u32(Address(a[2])+0x18))&0x7fffffffu;
    if (magnitude>0x41c00000 && magnitude<=0x7f800000) {
      auto at=vec(Address(a[2])+0x10);
      auto found=service(WorldFindFreeSpot,{at[0],at[1],at[2],0x47,0x18,0});
      if (found.size()!=4) throw Unsupported("free spot needs xyz and found byte");
      if (signed_byte(found[3])) spot={found[0],found[1],found[2]};
    }
    const auto strength=scalar(s,12,{0,rank,a[2],10});
    Raw props=0;
    if (!(u32(Address(a[2])+0xc)&0x40)) props=0x5038f0;
    else if (u32(Address(a[2])+0x3c8)<4) props=0x5038d0+8*u32(Address(a[2])+0x3c8);
    const auto badger=service_scalar(PoolNew,{0x3d0});
    if (!badger) return {0};
    service(CastBadgerConstructor,{badger,0x5527a8,spot[0],spot[1],spot[2],
        static_cast<Raw>(s16(a[6])),props,strength});
    const auto flags=mem.load(Address(badger)+0xa8,8);
    write(Address(badger)+0xc,4,u32(Address(badger)+0xc)|0x4002);
    write(Address(badger)+0x34,4,0x5527a8);
    write(Address(badger)+0xa8,8,flags|0x1000000);
    if (u32(Address(a[2])+0xc)&0x40) {
      write(Address(badger)+0x5a,1,mem.load(Address(a[2])+0x3c8,1));
      write(Address(badger)+0xa8,8,flags|0x1400000);
      const Address table=u32(Address(a[2])+0x48);
      const auto team=service_scalar(CreatureGetTeam,{static_cast<Raw>(a[2]+half(table+37*8))});
      service(CreatureSetCharm,{badger,0xffffffff,team});
      write(Address(badger)+0x3ac,2,400);
      const Address bt=u32(Address(badger)+0x48);
      service(CreatureSetHomeRange,{static_cast<Raw>(badger+half(bt+34*8)),500});
    }
    flash(a[2],1,{0x54,0xc0,0x33,0x41});flash(a[2],2,{0x54,0xc0,0x33,0x41});
    const auto block=service_scalar(PoolNew,{0xbc});
    if (block) execute(Operation::LightEffect,{block,add(spot[0],ZERO),spot[1],add(spot[2],0x42100000),a[2],0,
        0x42a80000,0x43400000,0x424c0000,0x42820000,0,0,0,0,0x42f00000,0x43340000,ZERO,ONE});
    sound(0x5527b8,0x5527c0,{a[3],a[4],a[5]});
    const auto spell=service_scalar(PoolNew,{0x20});
    if (spell) execute(Operation::SummonBadgerCtor,{spell,1,a[2],a[0],badger});
    return {spell};
  }
  case Operation::SummonBadgerCtor:
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51bd18});
    write(s+0x1c,4,0);write(s+0x18,4,0);
    if (a[4]) execute(Operation::HandleLink,{static_cast<Raw>(s+0x18),a[4],a[4]});
    return {};
  case Operation::SummonBadgerRun: {
    const auto node=u32(s+0x18);
    if (node) {
      const auto badger=u32(Address(node)+0xc);
      if (!badger) return {};
      auto clock=u32(s+0x1c), share=clock;
      if (!le(clock,ONE)) share=sub(ONE,sub(clock,ONE));
      if (le(clock,ONE) || le(ZERO,share)) {
        clock=add(clock,dt);write(s+0x1c,4,clock);
        if (le(share,ZERO)) return {};
        if (lt(ONE,clock)) write(Address(badger)+0xc,4,u32(Address(badger)+0xc)&~2u);
        const auto points=s32(service_scalar(SamplePointCount,{creature_model(badger)}));
        const auto at=execute(Operation::VisualPosition,{badger});
        const auto count=to_int(mul(share,0x42180000));
        std::int64_t index=0;
        for (std::int64_t k=0;k<count;++k) {
          const auto model=creature_model(badger);
          const auto point=sample(badger,model,at,static_cast<Raw>(index));
          const auto b=ball(s+0x10);
          service(ParticleSpawn,{0x8e8468,sub(point[0],mul(b[0],0x423eaf66)),
              sub(point[1],mul(b[1],0x423eaf66)),sub(point[2],mul(b[2],0x423eaf66)),
              mul(b[0],0x42092e61),mul(b[1],0x42092e61),mul(b[2],0x42092e61),ZERO});
          index+=8;
          if (points) index%=points;
        }
        return {};
      }
    }
    if (s) destroy(s);
    return {};
  }
  case Operation::PoisonBoltCast: {
    const auto rank=scalar(s,14,{a[1],20});
    flash(a[2],1,{0,0x80,0,0}); flash(a[2],2,{0,0x80,0,0});
    const auto obj=service_scalar(PoolNew,{0xb8});
    if (obj) execute(Operation::PoisonBoltCtor,{obj,a[3],a[4],a[5],a[6],rank,a[2],a[0]});
    return {0};
  }
  case Operation::PoisonBoltCtor: {
    service(ProjectileConstructor,{a[0],a[1],a[2],a[3],a[6],0});
    write(s+0x48,4,0x51bb78);
    execute(Operation::LightRecord,{static_cast<Raw>(s+0x54),0,0x80,0,0,
        0x42c20000,0x38dee34b,0x3f4ccccc,a[1],a[2],a[3]});
    service(LightLink,{static_cast<Raw>(s+0x54)});
    write(s+0x90,4,0); write(s+0x98,4,a[7]); write(s+0x9c,2,a[5]);
    write(s+0x94,4,0); write(s+0x38,2,a[4]); write(s+0xa8,4,0);
    write(s+0xa0,4,mul(cos16(a[4]),0x43af0000));
    write(s+0xa4,4,mul(sin16(a[4]),0x43af0000));
    if (a[6]) execute(Operation::HandleLink,{static_cast<Raw>(s+0x90),a[6],a[6]});
    const auto frame=u32(0x44fdcc);
    write(s+0xb4,4,0); write(s+0xac,4,frame+0x71); write(s+0xb0,4,frame+0xb);
    if (a[6]) {
      const auto found=service(CollectObjectsInArc,{a[1],a[2],a[3],static_cast<Raw>(s16(a[4])),0,0x438c0000,0x42700000,ZERO});
      for (auto other:found) {
        const Address caster=a[6];
        const auto adjusted=static_cast<Raw>(caster+half(Address(u32(caster+0x48))+160));
        if (service_scalar(CreatureEnemy,{adjusted,other})) {
          execute(Operation::HandleLink,{static_cast<Raw>(s+0x94),other,other}); break;
        }
      }
      service(HandleClearStack,{0});
    }
    service(ObjectRunEveryFrame,{a[0]});
    const auto sound=service_scalar(ResourceFind,{0x5525f8,0x552600});
    service(SoundPlayOnObject,{sound,a[0],ONE}); return {};
  }
  case Operation::PoisonBoltRun: {
    const auto frame=s32(u32(0x44fdcc));
    if (u32(s+0x94) && s32(u32(s+0xb0))<frame) {
      const Address target=u32(Address(u32(s+0x94))+0xc);
      const auto turn=atan2_16(sub(u32(target+0x14),u32(s+0x14)),sub(u32(target+0x10),u32(s+0x10)))-half(s+0x38);
      auto heading=half(s+0x38);
      if (turn>-0x222 && turn<0x222) heading+=turn;
      else if (turn<1) heading-=0x222;
      else heading+=0x222;
      write(s+0x38,2,static_cast<Raw>(heading));
      write(s+0xa0,4,mul(cos16(half(s+0x38)),0x43af0000));
      const auto sine=sin16(half(s+0x38));
      write(s+0xa8,4,0); write(s+0xa4,4,mul(sine,0x43af0000));
    }
    const auto old=vec(s+0x10), velocity=vec(s+0xa0);
    const auto result=service_scalar(ObjectMoveAndCollide,{a[0],velocity[0],velocity[1],velocity[2]});
    if ((result&2) && u32(s+0xb4)) write(s+0xac,4,0);
    if (result&1) write(s+0xac,4,0);
    if (s32(u32(0x44fdcc))<s32(u32(s+0xac))) {
      const std::array<Raw,3> delta{sub(u32(s+0x10),old[0]),sub(u32(s+0x14),old[1]),sub(u32(s+0x18),old[2])};
      const auto pulse=to_int(mul(mul(add(sin16(u32(0x44fdcc)<<12),ONE),0x42100000),HALF));
      for (const auto &part:std::array<std::pair<Raw,std::int64_t>,2>{{{0x8e8378,36ll-pulse},{0x8e83c8,pulse}}}) {
        for (std::int64_t i=0;i<part.second;++i) {
          const auto b=ball(s+0x44);
          std::array<Raw,3> point;
          for (unsigned k=0;k<3;++k) point[k]=add(mul(b[k],0x40dfc651),old[k]);
          const auto share=mul(from_int(own_draw(s+0x44)),TWO_M16);
          for (unsigned k=0;k<3;++k) point[k]=add(point[k],mul(delta[k],share));
          const auto v=ball(s+0x44);
          service(ParticleSpawn,{part.first,point[0],point[1],point[2],mul(v[0],0x4129ce76),mul(v[1],0x4129ce76),mul(v[2],0x4129ce76),ZERO});
        }
      }
      write_vec(s+0x84,vec(s+0x10)); service(LightRelink,{static_cast<Raw>(s+0x54)});
    } else {
      const auto sound=service_scalar(ResourceFind,{0x5525f8,0x552640});
      service(SoundPlayOnObject,{sound,a[0],ONE});
      execute(Operation::PoisonBoltBurst,{a[0]});
      if (s) service(ObjectDestroyCallback,{static_cast<Raw>(s+half(Address(u32(s+0x48))+8)),3});
    }
    return {};
  }
  case Operation::PoisonBoltOnHit: {
    const Address other=a[1];
    if (u32(s+0xb4)) return {};
    if (!u32(s+0x90)) {
      if (!(u32(other+0xc)&0x20)) write(s+0xb4,4,1);
      return {};
    }
    const Address owner=u32(Address(u32(s+0x90))+0xc);
    if (!service_scalar(CreatureEnemy,{static_cast<Raw>(owner+half(Address(u32(owner+0x48))+160)),a[1]})) return {};
    if ((u32(other+0xc)&0x20) && (mem.load(other+0xa8,8)&0x200)) return {};
    if (!u32(s+0x90)) return {};
    const auto skill=u32(s+0x98), rank=static_cast<Raw>(half(s+0x9c));
    const auto bounds=skill_call(skill,8,{0,rank,0,spread20});
    if (bounds.size()!=2) throw Unsupported("Poison Bolt damage range must have two values");
    const Raw flags=s32(rank)>15 ? 0x90040 : s32(rank)>10 ? 0x50040 : 0x10040;
    service(DamageApply,{a[1],u32(Address(u32(s+0x90))+0xc),100000,0,0,flags,0,0,from_int(bounds[0]),from_int(bounds[1])});
    if (u32(other+0xc)&0x20) {
      const auto seconds=scalar(u32(s+0x98),10,{3,static_cast<Raw>(half(s+0x9c))});
      service(CreatureSetEffectTimer,{a[1],3,u32(Address(u32(s+0x90))+0xc),from_int(seconds)});
    }
    write(s+0xb4,4,1); return {};
  }
  case Operation::PoisonBoltBurst: {
    if (u32(s+0x90)) {
      const auto skill=u32(s+0x98), rank=static_cast<Raw>(half(s+0x9c));
      const auto seconds=s32(scalar(skill,10,{3,rank}));
      const auto steps=s32(scalar(u32(s+0x98),10,{1,static_cast<Raw>(half(s+0x9c))}));
      const auto value=scalar(u32(s+0x98),10,{2,static_cast<Raw>(half(s+0x9c))});
      if (std::int64_t(steps)*12>0) {
        const auto block=service_scalar(PoolNew,{0x70});
        if (block) {
          const auto pos=vec(s+0x10);
          service(PoisonGasConstructor,{block,pos[0],pos[1],pos[2],static_cast<Raw>(std::int64_t(seconds)*60),static_cast<Raw>(std::int64_t(steps)*12),0,u32(Address(u32(s+0x4c))+0xc),value});
        }
      }
    }
    for (unsigned i=0;i<0x141;++i) {
      const auto b=ball(s+0x44);
      auto up=f2d(b[2]); if (dcmp(up,0)<0) up=dsub(0,up);
      up=dmul(dmul(up,mem.load(0x552658,8)),mem.load(0x552660,8));
      const auto pos=vec(s+0x10);
      service(ParticleSpawn,{0x8e8418,pos[0],pos[1],pos[2],mul(b[0],0x422b2ba5),mul(b[1],0x422b2ba5),d2f(up),ZERO});
    }
    return {};
  }
  case Operation::UnspokenWordCast: {
    const auto rank=scalar(s,14,{a[1],0x14});
    flash(a[2],1,{0x40,0x40,0x80,0x80});
    flash(a[2],2,{0x40,0x40,0x80,0x80});
    sound(0x5513b0,0x5513b8,{a[3],a[4],a[5]});
    const auto block=service_scalar(PoolNew,{0xbc});
    if (block) service(LightEffectConstructor,{block,add(a[3],ZERO),a[4],add(a[5],0x42100000),a[2],0,
        0x42c80000,0x42c80000,0x437f0000,0xc2c00000,0,0,0,0,0x42f00000,0x43340000,ZERO,ONE});
    const auto spell=service_scalar(PoolNew,{0x38});
    if (spell) execute(Operation::UnspokenWordCtor,{spell,rank,a[2],a[0]});
    return {spell};
  }
  case Operation::UnspokenWordCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51af10});
    write(s+0x1c,4,0);write(s+0x2c,4,u32(0x44fdcc));
    write(s+0x30,4,service_scalar(TrailCreate,{1,500,5,0x40,0x40,0x80,0x80,TWO}));
    const auto trail=service_scalar(TrailCreate,{1,300,5,0x40,0x80,0x40,0x80,TWO});
    const auto owner=u32(s+0xc);
    write(s+0x34,4,trail);write(s+0x18,4,owner);
    write_vec(s+0x20,vec(Address(owner)+0x10));
    const auto steps=from_int(scalar(u32(s+8),10,{1,level(s)}));
    const auto reach=mul(mul(steps,0x41400000),0x3fc00000);
    const auto at=vec(s+0x20);
    const auto found=service(CollectInSphere,{at[0],at[1],at[2],static_cast<Raw>(half(Address(u32(s+0xc))+0x38)),reach});
    for (const Address obj:found) {
      if (obj && (u32(obj+0xc)&0x20) && !(u32(obj+0xc)&0xd))
        execute(Operation::HandleLink,{static_cast<Raw>(s+0x1c),static_cast<Raw>(obj),static_cast<Raw>(obj)});
    }
    service(HandleClearStack,{0});return {};
  }
  case Operation::UnspokenWordPhase: {
    const auto frame=s32(u32(s+0x2c)*400u);
    return {static_cast<Raw>(std::int64_t(frame)%0x10001 + std::int64_t(s32(a[1]))*0x578)};
  }
  case Operation::UnspokenWordRun: {
    const auto steps=scalar(u32(s+8),10,{1,level(s)});
    const auto age=s32(u32(0x44fdcc)-u32(s+0x2c));
    const auto share=div(from_int(static_cast<Raw>(age)),0x41f00000);
    Raw fade=ZERO;
    if (lt(HALF,share)) fade=add(sub(share,HALF),sub(share,HALF));
    if (age>=30) {destroy(s);return {};}
    const auto ring=mul(share,mul(from_int(steps),0x41400000));
    const auto centre=vec(s+0x20);
    auto phase=[&]() {return execute(Operation::UnspokenWordPhase,{a[0],static_cast<Raw>(age)})[0];};
    auto first=[&](Raw angle,Raw other) {
      return std::vector<Raw>{mul(ring,cos16(angle)),mul(ring,sin16(angle)),add(mul(sin16(other<<1),0x41a2c994),0x4200654b)};
    };
    auto second=[&](Raw angle,Raw other,bool sine) {
      return std::vector<Raw>{mul(neg(ring),cos16(other)),mul(ring,sin16(other)),
          add(mul(sine ? sin16(angle<<2) : cos16(angle<<2),0x41a2c994),0x4200654b)};
    };
    auto angle=phase(),other=angle+0x8000;
    std::vector<Raw> trail1,trail2;
    for (unsigned i=0;i<10;++i) {
      const auto p=first(angle,other),q=second(angle,other,false);
      for (unsigned k=0;k<3;++k) {trail1.push_back(add(p[k],centre[k]));trail2.push_back(add(q[k],centre[k]));}
      angle+=0x1999;other+=0x1999;
    }
    if (lt(ZERO,fade)) {
      service(TrailSetLength,{u32(s+0x30),static_cast<Raw>(to_int(sub(0x43fa0000,mul(fade,0x43fa0000))))});
      service(TrailSetLength,{u32(s+0x34),static_cast<Raw>(to_int(sub(0x43960000,mul(fade,0x43960000))))});
      const auto alpha=0x80u-static_cast<Raw>(to_int(mul(fade,0x43000000)));
      service(TrailSetColor,{u32(s+0x30),0x40,0x40,0x80,alpha});
      service(TrailSetColor,{u32(s+0x34),0x40,0x80,0x40,alpha});
    }
    trail1.insert(trail1.begin(),u32(s+0x30));trail1.insert(trail1.end(),{10,0x78,ZERO,ONE});
    service(TrailSetPoints,std::move(trail1));
    trail2.insert(trail2.begin(),u32(s+0x34));trail2.insert(trail2.end(),{10,0x78,ZERO,ONE});
    service(TrailSetPoints,std::move(trail2));
    if (lt(ZERO,fade)) {
      const auto count=std::int64_t(s32(scalar(u32(s+8),10,{1,level(s)})))*6;
      angle=phase();other=angle+0x8000;
      for (std::int64_t i=0;i<count;++i) {
        auto p=first(angle,other);
        for (unsigned k=0;k<3;++k) p[k]=add(p[k],centre[k]);
        const auto v=ball(s+0x10);
        const std::vector<Raw> velocity{mul(v[0],0x41000000),mul(v[1],0x41000000),mul(v[2],0x41000000)};
        angle+=static_cast<Raw>(0x10000/count);other+=static_cast<Raw>(0x10000/count);
        service(ParticleSpawn,{0x8e6c90,add(p[0],velocity[0]),add(p[1],velocity[1]),add(p[2],velocity[2]),velocity[0],velocity[1],velocity[2],ZERO});
      }
      const auto half_count=count/2;
      angle=phase();other=angle+0x8000;
      for (std::int64_t i=0;i<half_count;++i) {
        auto p=second(angle,other,true);
        for (unsigned k=0;k<3;++k) p[k]=add(p[k],u32(s+0x20+4*k));
        const auto v=ball(s+0x10);
        const std::vector<Raw> velocity{mul(v[0],0x41000000),mul(v[1],0x41000000),mul(v[2],0x41000000)};
        angle+=static_cast<Raw>(0x10000/half_count);other+=static_cast<Raw>(0x10000/half_count);
        service(ParticleSpawn,{0x8e6ce0,add(p[0],velocity[0]),add(p[1],velocity[1]),add(p[2],velocity[2]),velocity[0],velocity[1],velocity[2],ZERO});
      }
    }
    execute(Operation::UnspokenWordHitRing,{a[0],ring});return {};
  }
  case Operation::UnspokenWordHitRing: {
    const auto items=execute(Operation::HandleItems,{static_cast<Raw>(s+0x1c)});
    for (std::size_t i=0;i<items.size();i+=2) {
      const auto node=items[i];const Address obj=items[i+1];
      if (!obj) {execute(Operation::HandleRemove,{node});continue;}
      const auto dy=sub(u32(s+0x24),u32(obj+0x14));
      const auto dx=sub(u32(s+0x20),u32(obj+0x10));
      if (!le(sub(sqrt(add(mul(dx,dx),mul(dy,dy))),u32(obj+0x1c)),a[1])) continue;
      if (u32(obj+0xc)&0x20) {
        if ((u32(obj+0xc)&9) || !service_scalar(CreatureEnemy,{
            static_cast<Raw>(Address(u32(s+0x18))+half(Address(u32(Address(u32(s+0x18))+0x48))+160)),static_cast<Raw>(obj)})) {
          execute(Operation::HandleRemove,{node});continue;
        }
      }
      const auto rank=level(s);
      const auto bounds=skill_call(u32(s+8),8,{0,rank,0,spread20});
      if (bounds.size()!=2) throw Unsupported("Unspoken Word damage range must have two values");
      const Raw flags=s32(rank)>=16 ? 0x2200940 : s32(rank)>=11 ? 0x2100940 : s32(rank)>=6 ? 0x80940 : 0x40940;
      service(DamageApply,{static_cast<Raw>(obj),u32(s+0x18),100000,0,0,flags,0,0,from_int(bounds[0]),from_int(bounds[1])});
      if ((u32(obj+0xc)&0x20) && le(ZERO,u32(obj+0x228))) {
        const auto roll=own_draw(s+0x10);
        const auto chance=s32(scalar(u32(s+8),10,{2,level(s)}));
        if (100-std::int64_t(chance)<roll%100+1) {
          const auto seconds=scalar(u32(s+8),10,{3,level(s)});
          const Address table=u32(obj+0x48);
          service(CreatureStun,{static_cast<Raw>(obj+half(table+240)),seconds*60u,1});
        }
      }
      execute(Operation::HandleRemove,{node});
    }
    return {};
  }
  case Operation::RoarCanUse:
    return {execute(Operation::FindSpell,{a[1],0x3d07b8})[0] ? 0u : 1u};
  case Operation::RoarCast: {
    const auto rank=scalar(s,14,{a[1],20});
    const auto spell=service_scalar(PoolNew,{0x50});
    if (spell) execute(Operation::RoarCtor,{spell,rank,a[2],a[0]});
    return {spell};
  }
  case Operation::RoarCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51b2d0});
    write(s+0x3c,4,0xffffffff); write(s+0x18,4,a[2]);
    write(s+0x34,4,0xbf800000); write(s+0x40,4,1);
    write(Address(a[2])+0x58c,4,6);
    for (unsigned off:{0x1c,0x2c,0x48,0x44,0x4c}) write(s+off,4,0);
    write(s+0x30,4,0xbf800000); write_vec(s+0x20,vec(Address(a[2])+0x10));
    execute(Operation::ShoutAnimation,{u32(s+0x18),0x5517e8,0x22,5,
        0,0x5517d8,1,0x551808,2,0x5517f8,3,0x551818,7,0x551828,
        2,1,0x3ea24db1,2,2,0x3f9e3443,4});
    const auto result=sound(0x551838,0x551840,vec(s+0x20));
    if (result.size()!=1) throw Unsupported("Roar sound handle must be scalar");
    write(s+0x3c,4,result[0]); return {};
  }
  case Operation::RoarBreath: {
    auto share=mul(from_int(own_draw(s+0x10)),TWO_M16);
    if (a[7]) share=sub(add(share,share),ONE);
    // Python adds its integer facing and truncated offset before cos16/sin16.
    // Only the low 16 bits are observed by the trig functions.
    const auto angle=a[1]+static_cast<Raw>(to_int(mul(share,0x45753000)));
    const auto cosine=cos16(angle), sine=sin16(angle);
    service(ParticleSpawn,{a[5],add(a[2],mul(cosine,0x40f891c0)),
        add(a[3],mul(sine,0x40f891c0)),a[4],mul(cosine,a[6]),mul(sine,a[6]),ZERO,ZERO});
    return {};
  }
  case Operation::RoarRun: {
    const auto facing=static_cast<Raw>(s16(a[2]));
    auto owner=u32(s+0x18);
    if (mem.load(0x402791,1) && service_scalar(NetIsLocalSlot,{u32(Address(owner)+0x3c8)}))
      write(0x402797,1,1);
    if (u32(s+0x40)) {
      if (u32(Address(u32(s+0x18))+0x58c)!=6) {
        write(s+0x40,4,0);
        if (lt(u32(s+0x30),ZERO)) {
          execute(Operation::ShoutInterrupted,{a[0],u32(s+0x18),1,0x3c});
          write(s+0x44,4,1); write(s+0x34,4,add(u32(s+0x2c),HALF));
        }
      }
      if (u32(s+0x40)) {
        owner=u32(s+0x18);
        const auto event=service_scalar(PlayerAnimationStep,{owner,ONE});
        if (!(mem.load(Address(u32(s+0x18))+0x11a,2)&0x20)) {
          write(s+0x40,4,0); write(Address(u32(s+0x18))+0x58c,4,2);
        } else {
          if (event==2) {
            write(s+0x48,4,1); write(s+0x30,4,u32(s+0x2c));
            service(CameraShake,{0,add(mul(from_int(level(s)),HALF),0x40600000)});
            owner=u32(s+0x18);
            const auto mouth=attach_point(owner,7);
            for (unsigned k=0;k<3;++k) mem.poke(s+0x20+4*k,4,mouth[k]);
            const auto centre=vec(s+0x20);
            const auto ground=service_scalar(TerrainHeight,{u32(0x44fdc8),centre[0],centre[1],centre[2],0x40c00000});
            write(s+0x38,4,ground);
            const auto wet=service_scalar(WaterTest,{u32(0x44fdc8),centre[0],centre[1],ground,0,0x41000000});
            write(s+0x4c,4,wet ? 1 : 0);
            const auto steps=from_int(scalar(u32(s+8),10,{0,level(s)}));
            const auto at=vec(s+0x20);
            const auto found=service(CollectInSphere,{at[0],at[1],at[2],
                static_cast<Raw>(half(Address(u32(s+0x18))+0x38)),mul(mul(steps,0x41400000),0x3fc00000)});
            for (const auto obj:found)
              if (obj && !(u32(Address(obj)+0xc)&0xd)) execute(Operation::HandleLink,{static_cast<Raw>(s+0x1c),obj,obj});
            service(HandleClearStack,{0});
          }
          if (event==4) write(s+0x48,4,0);
          if (u32(s+0x48)) {
            const auto mouth=attach_point(u32(s+0x18),7);
            if (lt(mul(from_int(own_draw(s+0x10)),TWO_M16),0x3d81059e))
              execute(Operation::RoarBreath,{a[0],facing,mouth[0],mouth[1],mouth[2],0x8e6e20,0x418fca03,0});
            if (!le(0x3e2f1737,mul(from_int(own_draw(s+0x10)),TWO_M16)))
              execute(Operation::RoarBreath,{a[0],facing,mouth[0],mouth[1],mouth[2],0x8e6dd0,0x4129b986,1});
          }
        }
      }
    }
    if (!u32(s+0x44)) {
      if (le(ZERO,u32(s+0x30))) {
        auto grown=sub(u32(s+0x2c),u32(s+0x30)); grown=add(grown,grown);
        if (le(grown,ONE)) {
          const auto rank=level(s);
          const auto steps=from_int(scalar(u32(s+8),10,{0,rank}));
          const auto ring=mul(mul(grown,0x41400000),steps);
          execute(Operation::RoarHitRing,{a[0],ring});
          const auto height=add(u32(s+0x28),0x398f2170);
          const int ring_count=s32(level(s))>9 ? 0x172 : 0x115;
          for (int i=0;i<ring_count;++i) {
            const auto angle=own_draw(s+0x10);
            const auto x=add(u32(s+0x20),mul(cos16(angle),ring));
            const auto y=add(u32(s+0x24),mul(sin16(angle),ring));
            service(ParticleSpawn,{0x8e6e70,x,y,height,0x3f57582b,0x3f57582b,0x4106971b,ZERO});
          }
          const int dust_count=s32(level(s))>9 ? 12 : 9;
          for (int i=0;i<dust_count;++i) {
            const auto angle=own_draw(s+0x10);
            const auto x=add(u32(s+0x20),mul(cos16(angle),ring));
            const auto y=add(u32(s+0x24),mul(sin16(angle),ring));
            const auto velocity=ball(s+0x10);
            service(ParticleSpawn,{0x8e6ec0,x,y,height,mul(velocity[0],0x40800000),
                mul(velocity[1],0x40800000),mul(velocity[2],0x40800000),ZERO});
          }
          if (u32(s+0x4c) && eq(u32(s+0x2c),u32(s+0x30)))
            service(WaterDisturb,{u32(0x44fdc8),u32(s+0x20),u32(s+0x24),u32(s+0x38),0x455ac000,0x42100000});
        } else write(s+0x34,4,u32(s+0x2c));
      }
    } else {
      const auto mouth=attach_point(u32(s+0x18),7);
      if (lt(mul(from_int(own_draw(s+0x10)),TWO_M16),0x3e2f1737))
        execute(Operation::RoarBreath,{a[0],facing,mouth[0],mouth[1],mouth[2],0x8e6ec0,0x4129b986,1});
    }
    if (!u32(s+0x40)) {
      if (!(le(ZERO,u32(s+0x34)) && lt(u32(s+0x2c),u32(s+0x34)))) {
        if (!u32(s+0x44) && u32(Address(u32(s+0x18))+0x58c)==6) write(Address(u32(s+0x18))+0x58c,4,2);
        if (s) destroy(s);
        return {};
      }
    }
    const auto clock=add(u32(s+0x2c),dt); write(s+0x2c,4,clock);
    if (lt(0x40c00000,clock) && s) destroy(s);
    return {};
  }
  case Operation::RoarHitRing: {
    const auto items=execute(Operation::HandleItems,{static_cast<Raw>(s+0x1c)});
    for (std::size_t i=0;i<items.size();i+=2) {
      const auto node=items[i]; const Address obj=items[i+1];
      if (!obj) {execute(Operation::HandleRemove,{node});continue;}
      const auto dy=sub(u32(s+0x24),u32(obj+0x14));
      const auto dx=sub(u32(s+0x20),u32(obj+0x10));
      if (!le(sub(sqrt(add(mul(dx,dx),mul(dy,dy))),u32(obj+0x1c)),a[1])) continue;
      if (u32(obj+0xc)&0x20) {
        if ((u32(obj+0xc)&9) || !service_scalar(CreatureEnemy,{
            static_cast<Raw>(Address(u32(s+0x18))+half(Address(u32(Address(u32(s+0x18))+0x48))+160)),static_cast<Raw>(obj)})) {
          execute(Operation::HandleRemove,{node});continue;
        }
      }
      const auto rank=level(s);
      const auto bounds=skill_call(u32(s+8),8,{1,rank,0,spread20});
      if (bounds.size()!=2) throw Unsupported("Roar damage range must have two values");
      const Raw flags=s32(rank)>=10 ? 0x200940 : s32(rank)>=5 ? 0x100940 : 0x80940;
      service(DamageApply,{static_cast<Raw>(obj),u32(s+0x18),100000,0,0,flags,0,0,from_int(bounds[0]),from_int(bounds[1])});
      execute(Operation::HandleRemove,{node});
    }
    return {};
  }
  case Operation::WaveOfFlameCanUse:
    return {1};
  case Operation::WaveOfFlameCast: {
    const auto rank=scalar(s,14,{a[1],20});
    const auto spell=service_scalar(PoolNew,{0x2c});
    if (spell) execute(Operation::WaveOfFlameCtor,{spell,rank,a[2],ZERO,ZERO,ZERO,a[0]});
    return {spell};
  }
  case Operation::WaveOfFlameCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[6],0x51c330});
    write(s+0x18,4,0);
    const auto visual=execute(Operation::VisualPosition,{a[2]});
    write(s+0x1c,4,add(visual[0],a[3]));
    write(s+0x20,4,add(visual[1],a[4]));
    write(s+0x24,4,add(visual[2],a[5]));
    write(s+0x28,4,0);
    const auto centre=vec(s+0x1c);
    sound(0x552f40,0x552f48,centre);
    const auto block=service_scalar(PoolNew,{0xbc});
    if (block) service(LightEffectConstructor,{block,add(centre[0],ZERO),centre[1],add(centre[2],0x42100000),
        0,0,0x43400000,0x42900000,0x42140000,0x43730000,ZERO,ZERO,ZERO,ZERO,
        0x42f00000,0x43340000,ZERO,ONE});
    const auto steps=from_int(scalar(u32(s+8),12,{0,level(s),a[2],10}));
    const auto reach=add(mul(steps,0x41400000),mul(steps,0x41400000));
    const auto found=service(CollectInSphere,{centre[0],centre[1],centre[2],
        static_cast<Raw>(half(Address(u32(s+0xc))+0x38)),reach});
    for (const auto obj:found)
      if (obj && !(u32(Address(obj)+0xc)&0xd))
        execute(Operation::HandleLink,{static_cast<Raw>(s+0x18),obj,obj});
    service(HandleClearStack,{0});
    return {};
  }
  case Operation::WaveOfFlameRun: {
    const auto clock=add(u32(s+0x28),dt);
    write(s+0x28,4,clock);
    const auto ring=add(mul(mul(clock,0x40c00000),0x41400000),0x41a0c8b4);
    const auto steps=scalar(u32(s+8),12,{0,level(s),u32(s+0xc),10});
    if (lt(mul(from_int(steps),0x41400000),ring)) {destroy(s);return {};}
    execute(Operation::WaveOfFlameHitTest,{a[0],ring});
    const auto wobble=add(mul(sin16(u32(0x44fdcc)*0xd09u),0x40b51ea5),0x4226189c);
    auto share=mul(ring,0x3ce56041);
    if (!le(share,ONE)) share=ONE;
    constexpr std::array<Raw,3> definitions{0x8e8698,0x8e86e8,0x8e8738};
    constexpr std::array<Raw,3> scales{0x40a00000,0x42820000,0x40c00000};
    for (unsigned k=0;k<3;++k) {
      const auto count=to_int(mul(share,scales[k]));
      for (std::int32_t i=0;i<count;++i) {
        const auto angle=own_draw(s+0x10);
        const auto px=add(u32(s+0x1c),mul(cos16(angle),ring));
        const auto sine=sin16(angle);
        const auto pz=add(u32(s+0x24),wobble);
        const auto py=add(u32(s+0x20),mul(sine,ring));
        const auto velocity=ball(s+0x10);
        service(ParticleSpawn,{definitions[k],px,py,pz,mul(velocity[0],0x40cc078f),
            mul(velocity[1],0x40cc078f),mul(velocity[2],0x40cc078f),ZERO});
      }
    }
    return {};
  }
  case Operation::WaveOfFlameHitTest: {
    const auto items=execute(Operation::HandleItems,{static_cast<Raw>(s+0x18)});
    for (std::size_t i=0;i<items.size();i+=2) {
      const auto node=items[i]; const Address obj=items[i+1];
      if (!obj) {execute(Operation::HandleRemove,{node});continue;}
      const auto dx=sub(u32(s+0x1c),u32(obj+0x10));
      const auto dy=sub(u32(s+0x20),u32(obj+0x14));
      if (!le(sub(add(mul(dx,dx),mul(dy,dy)),u32(obj+0x1c)),mul(a[1],a[1]))) continue;
      if (u32(obj+0xc)&0x20) {
        if ((u32(obj+0xc)&9) || !service_scalar(CreatureEnemy,{
            static_cast<Raw>(Address(u32(s+0xc))+half(Address(u32(Address(u32(s+0xc))+0x48))+160)),
            static_cast<Raw>(obj)})) {
          execute(Operation::HandleRemove,{node});continue;
        }
      }
      const auto owner=u32(s+0xc);
      const auto bounds=execute(Operation::StatisticSpread,{u32(s+8),1,level(s),owner});
      std::vector<Raw> args{static_cast<Raw>(obj),u32(s+0xc),100000,0,0,0x160};
      args.insert(args.end(),16,0);args.insert(args.end(),{255,255,0x15,10,10,0});
      for (unsigned k=0;k<3;++k) args.insert(args.end(),{0xffff,0,0,0});
      args.insert(args.end(),{0,from_int(bounds[0]),from_int(bounds[1])});
      service(DamageApply,std::move(args));
      sound(0x552f40,0x552f60,vec(obj+0x10));
      execute(Operation::HandleRemove,{node});
    }
    return {};
  }
  case Operation::DrowsyCast: {
    const auto facing=static_cast<Raw>(s16(a[6]));
    const auto rank=scalar(s,14,{a[1],10});
    const auto clip=service_scalar(ResourceFind,{0x552530,0x552538});
    service(SoundPlayOnObject,{clip,a[2],ONE});
    flash(a[2],1,{0x40,0x40,0x80,0x2e}); flash(a[2],2,{0x40,0x40,0x80,0x2e});
    const auto block=service_scalar(PoolNew,{0xbc});
    if (block) service(LightEffectConstructor,{block,add(a[3],ZERO),a[4],add(a[5],0x42100000),
        a[2],0,0x42800000,0x42800000,0x43000000,0x42380000,ZERO,ZERO,ZERO,ZERO,
        0x42f00000,0x43340000,ZERO,ONE});
    const auto most=s32(scalar(s,10,{0,rank}));
    const auto found=service(CollectObjectsInArc,{a[3],a[4],a[5],facing,0,0x43700000,0x42f00000,ZERO});
    std::int64_t taken=0;
    for (const auto obj:found) {
      if (!(taken<most)) break;
      if (a[2] && !service_scalar(CreatureEnemy,{static_cast<Raw>(Address(a[2])+half(Address(u32(Address(a[2])+0x48))+160)),obj})) continue;
      if ((u32(Address(obj)+0xc)&0x1000000) || service_scalar(CreatureBossRank,{obj}) || !(half(Address(obj)+0x31a)<1)) continue;
      const auto spell=service_scalar(PoolNew,{0x34});
      if (spell) execute(Operation::DrowsyCtor,{spell,rank,obj,a[0]});
      service(CreatureAddSpell,{obj,spell}); ++taken;
    }
    service(HandleClearStack,{0});
    return {0};
  }
  case Operation::DrowsyCtor:
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51bb48});
    write(s+0x18,4,0); write(s+0x1c,4,0); write(s+0x24,4,0);
    write(s+0x20,4,scalar(a[3],10,{1,static_cast<Raw>(signed_byte(a[1]))}));
    write(s+0x28,4,0); write(s+0x30,4,0xc207b1cf); write(s+0x2c,4,0xc187b1cf);
    return {};
  case Operation::DrowsyBurst: {
    const auto owner=u32(s+0xc);
    const auto at=execute(Operation::VisualPosition,{owner});
    const auto top=add(at[2],u32(Address(u32(s+0xc))+0x20));
    const auto radius=u32(Address(u32(s+0xc))+0x1c);
    for (unsigned i=0;i<15;++i) {
      const auto share=mul(from_int(own_draw(s+0x10)),0x37800000);
      const auto angle=own_draw(s+0x10);
      const auto ox=mul(mul(cos16(angle),share),radius);
      const auto oy=mul(mul(sin16(angle),share),radius);
      const auto point=ball(s+0x10);
      const auto vx=mul(point[0],a[2]),vy=mul(point[1],a[2]),vz=a[3] ? ZERO : mul(point[2],a[2]);
      const auto tx=add(vx,at[0]),ty=add(vy,at[1]);
      const auto tz=a[3] ? add(top,ZERO) : add(vz,top);
      service(ParticleSpawn,{a[1],add(ox,tx),add(oy,ty),add(ZERO,tz),vx,vy,vz,ZERO});
    }
    return {};
  }
  case Operation::DrowsyRun: {
    const auto owner=u32(s+0xc);
    const auto clock=add(u32(s+0x18),dt);
    write(s+0x18,4,clock);
    const auto phase=u32(s+0x24);
    if (!(u32(Address(owner)+0xc)&8)) {
      if (phase==0) {
        execute(Operation::DrowsyBurst,{a[0],0x8e82d8,0x40800000,0});
        if (!le(HALF,u32(s+0x18))) return {};
        const auto roll=own_draw(s+0x10)%100;
        const auto chance=s32(scalar(u32(s+8),10,{2,level(s)}));
        if (std::int64_t(roll)<chance) {
          write(s+0x24,4,2); write(s+0x1c,4,u32(s+0x18));
          write(Address(u32(s+0xc))+0x31a,2,u32(s+0x20)*0x3cu);
        } else {write(s+0x1c,4,u32(s+0x18)); write(s+0x24,4,1);}
        return {};
      }
      if (phase==1) {
        execute(Operation::DrowsyBurst,{a[0],0x8e8328,0x41400000,1});
        if (le(sub(u32(s+0x18),u32(s+0x1c)),0x3e3645a1)) return {};
      } else if (phase==2) {
        if (le(sub(clock,u32(s+0x1c)),from_int(u32(s+0x20)))) {
          for (unsigned k=0;k<3;++k) {
            const auto height=add(u32(s+0x28+4*k),0x3ea6018c);
            write(s+0x28+4*k,4,height);
            if (lt(0x424e7203,height)) write(s+0x28+4*k,4,0);
          }
          flash(owner,1,{0x40,0x40,0x80,0x2e}); flash(owner,2,{0x40,0x40,0x80,0x2e});
          if (signed_byte(mem.load(0x402791,1)) && (u32(Address(owner)+0xc)&0x40) &&
              service_scalar(NetIsLocalSlot,{u32(Address(owner)+0x3c8)})) write(0x402797,1,1);
          return {};
        }
      } else return {};
    }
    if (s) destroy(s);
    return {};
  }
  case Operation::DrowsyFraction:
    return {mul(from_int(scalar(u32(s+8),10,{3,level(s)})),percent)};
  case Operation::RaiseArmyCanUse:
    return {execute(Operation::FindSpell,{a[1],0x3d16f0})[0] ? 0u : 1u};
  case Operation::RaiseArmyCast: {
    const auto rank=scalar(s,14,{a[1],20});
    flash(a[2],1,{0x80,0x32,0,0x32}); flash(a[2],2,{0x80,0x32,0,0x32});
    sound(0x552a98,0x552aa0,{a[3],a[4],a[5]});
    const auto block=service_scalar(PoolNew,{0xbc});
    if (block) service(LightEffectConstructor,{block,add(a[3],ZERO),a[4],add(a[5],0x42100000),
        a[2],0,0x43000000,0x42480000,ZERO,0x42480000,ZERO,ZERO,ZERO,ZERO,
        0x42f00000,0x43340000,ZERO,ONE});
    const auto spell=service_scalar(PoolNew,{0x34});
    if (spell) execute(Operation::RaiseArmyCtor,{spell,rank,a[2],static_cast<Raw>(s)});
    return {spell};
  }
  case Operation::RaiseArmyIsRaisable: {
    const Address c=a[1];
    if (!c || (u32(c+0xc)&0x4a)!=8 || !(mem.load(c+0x11a,2)&0x80)) return {0};
    if (mem.load(c+0xa8,8)&UINT64_C(0x105000801)) return {0};
    if (service_scalar(CreatureBossRank,{a[1]})) return {0};
    return {(u32(c+0xc)&0x1000000) ? 0u : 1u};
  }
  case Operation::RaiseArmyCtor: {
    execute(Operation::SpellBase,{a[0],a[1],a[2],a[3],0x51bf40});
    write(s+0x18,4,0); write(s+0x28,4,0);
    const auto reach=mul(from_int(scalar(a[3],10,{0,level(s)})),0x41400000);
    write(s+0x30,2,0); write(s+0x2c,4,mul(reach,0x3bc196b0));
    write_vec(s+0x1c,vec(Address(u32(s+0xc))+0x10));
    const auto centre=vec(s+0x1c);
    const auto found=service(CollectObjectsInBox,{sub(centre[0],reach),sub(centre[1],reach),sub(centre[2],reach),
        add(centre[0],reach),add(centre[1],reach),add(centre[2],reach),0x100});
    for (const auto obj:found)
      if ((u32(Address(obj)+0xc)&0x20) && execute(Operation::RaiseArmyIsRaisable,{a[0],obj})[0])
        execute(Operation::HandleLink,{static_cast<Raw>(s+0x18),obj,obj});
    return {};
  }
  case Operation::RaiseArmyRun: {
    const auto share=sub(ONE,div(sub(u32(s+0x2c),u32(s+0x28)),u32(s+0x2c)));
    if (lt(ONE,share)) { if (s) destroy(s); return {}; }
    const auto steps=from_int(scalar(u32(s+8),10,{0,level(s)}));
    const auto height=add(u32(s+0x24),0x41c043ae);
    const auto ring=mul(mul(share,0x41400000),steps);
    for (unsigned i=0;i<0xce;++i) {
      const auto angle=own_draw(s+0x10);
      service(ParticleSpawn,{0x8e85a8,add(u32(s+0x1c),mul(cos16(angle),ring)),
          add(u32(s+0x20),mul(sin16(angle),ring)),height,ZERO,ZERO,0x3b8259e1,ZERO});
    }
    execute(Operation::RaiseArmyRaiseInRing,{a[0],ring});
    write(s+0x28,4,add(u32(s+0x28),dt));
    return {};
  }
  case Operation::RaiseArmyRaiseInRing: {
    const auto items=execute(Operation::HandleItems,{static_cast<Raw>(s+0x18)});
    for (std::size_t i=0;i<items.size();i+=2) {
      const auto node=items[i]; const Address corpse=items[i+1];
      if (!corpse) {execute(Operation::HandleRemove,{node}); continue;}
      const auto dy=sub(u32(s+0x20),u32(corpse+0x14));
      const auto dx=sub(u32(s+0x1c),u32(corpse+0x10));
      if (!le(sub(sqrt(add(mul(dx,dx),mul(dy,dy))),u32(corpse+0x1c)),a[1])) continue;
      if ((u32(corpse+0xc)&0x20) && !execute(Operation::RaiseArmyIsRaisable,{a[0],static_cast<Raw>(corpse)})[0]) {
        execute(Operation::HandleRemove,{node}); continue;
      }
      const auto name=u32(corpse+0x34);
      if (name) {
        const auto at=vec(corpse+0x10);
        const auto created=service_scalar(WorldCreateObject,{name,at[0],at[1],at[2],static_cast<Raw>(half(corpse+0x38)),0});
        if (created && (u32(Address(created)+0xc)&0x20))
          execute(Operation::RaiseArmyDress,{a[0],static_cast<Raw>(corpse),created});
      }
      sound(0x552a98,0x552af8,vec(corpse+0x10));
      execute(Operation::HandleRemove,{node});
      const auto count=s16(static_cast<Raw>(half(s+0x30)+1));
      write(s+0x30,2,static_cast<Raw>(count));
      if (s16(scalar(u32(s+8),10,{1,level(s)}))<=count) {
        write(s+0x2c,4,u32(s+0x28)); return {};
      }
    }
    return {};
  }
  case Operation::RaiseArmyDress: {
    const Address corpse=a[1], n=a[2];
    const auto share=mul(from_int(scalar(u32(s+8),10,{2,level(s)})),percent);
    const auto hp=mul(share,u32(corpse+0x22c));
    write(n+0x228,4,hp); if (lt(hp,ONE)) write(n+0x228,4,ONE);
    service(ObjectRunEveryFrame,{a[2]});
    write(n+0xa8,8,mem.load(n+0xa8,8)|UINT64_C(0x8002000101400000));
    write(n+0x5a,1,mem.load(Address(u32(s+0xc))+0x3c8,1));
    const auto owner=u32(s+0xc);
    const Address owner_table=u32(Address(owner)+0x48);
    const auto team=service_scalar(CreatureGetTeam,{static_cast<Raw>(owner+half(owner_table+37*8))});
    service(CreatureSetCharm,{a[2],0xe10,team});
    write(n+0x118,1,0);
    flash(n,1,{0x80,0x32,0,0x32}); flash(n,2,{0x80,0x32,0,0x32});
    const auto top=signed_byte(static_cast<Raw>(mem.load(corpse+0x118,1)))*0x20;
    service(AnimationPushWithWeights,{static_cast<Raw>(n+0x114),u32(corpse+top+0xfc),
        static_cast<Raw>(half(corpse+top+0x118)),u32(corpse+top+0x114),u32(corpse+top+0x104),
        u32(corpse+top+0x108),u32(corpse+top+0x10c),u32(corpse+top+0x110)});
    const auto next_top=signed_byte(static_cast<Raw>(mem.load(corpse+0x118,1)))*0x20;
    write(n+0x134,4,u32(corpse+next_top+0x114));
    const Address table=u32(n+0x48);
    service(CreatureSetState,{static_cast<Raw>(n+half(table+27*8)),0});
    for (Raw hand:{1u,2u}) {
      for (unsigned ask:{15u,16u}) {
        Address holder=u32(corpse+0x50); Raw value=0;
        if (holder) {
          const Address ht=u32(holder);
          value=service_scalar(ask==15 ? HolderGetModel : HolderGetElement,
              {static_cast<Raw>(holder+half(ht+ask*8)),hand});
        }
        holder=u32(n+0x50);
        if (holder) {
          const unsigned give=ask==15 ? 1 : 8; const Address ht=u32(holder);
          service(give==1 ? HolderSetModel : HolderClearElement,
              {static_cast<Raw>(holder+half(ht+give*8)),hand,value});
        }
      }
    }
    if (service_scalar(StringPrefixDiffers,{u32(n+0x34),0x552ad8}) &&
        service_scalar(StringPrefixDiffers,{u32(n+0x34),0x552ae0}))
      write(n+0xe8,4,u32(corpse+0xe8));
    const auto model=creature_model(n);
    const auto mask=service(ModelBuildHideMask,{model,u32(n+0xe8)});
    if (mask.size()!=2) throw Unsupported("hide mask needs low/high raw32 words");
    write(n+0x220,8,std::uint64_t(mask[0])|(std::uint64_t(mask[1])<<32));
    if (!service_scalar(StringPrefixDiffers,{u32(n+0x34),0x552af0})) {
      const auto kind=u32(corpse+0x3e0); write(n+0x3e0,4,kind);
      if (kind==1 || kind==2) write(n+0xa0,1,kind==1 ? 2 : 4);
    }
    const auto flags=mem.load(corpse+0xa8,8);
    write(corpse+0x2f4,4,0); write(corpse+0xa8,8,flags|0x1000000);
    return {};
  }
  case Operation::DiseaseBladeCanUse:
    return {execute(Operation::FindSpell,{a[1],0x3d1e94})[0] ? 0u : 1u};
  case Operation::DiseaseBladeCast: {
    const auto rank=scalar(s,14,{a[1],1});
    const auto spell=service_scalar(PoolNew,{0x24});
    if (spell) execute(Operation::DiseaseBladeCtor,{spell,rank,a[2],static_cast<Raw>(s)});
    return {spell};
  }
  case Operation::PlayerWeapons: {
    const Address rec=record(s);
    const auto active=signed_byte(static_cast<Raw>(mem.load(rec+0x4a7c,1)));
    return {u32(indexed(rec,0x4434,4,static_cast<Raw>(active))),u32(rec+0x443c)};
  }
  case Operation::WriteDisease:
    write_disease(s,a[1],s32(a[2]),s32(a[3])); return {};
  case Operation::DiseaseBladeCtor: {
    execute(Operation::SpellBase,{static_cast<Raw>(s),a[1],a[2],a[3],0x51c4a0});
    const auto seconds=s32(scalar(a[3],12,{1,static_cast<Raw>(signed_byte(a[1])),a[2],20}));
    const auto frame=u32(0x44fdcc);
    write(s+0x18,4,frame+static_cast<Raw>(std::int64_t(seconds)*60));
    write(s+0x1c,4,frame+30u);
    const auto radius=scalar(u32(s+8),10,{2,level(s)});
    write(s+0x20,4,mul(from_int(radius),0x41400000));
    const auto value=scalar(u32(s+8),12,{0,level(s),u32(s+0xc),0});
    const auto frames=std::int64_t(s32(scalar(u32(s+8),12,{1,level(s),u32(s+0xc),20})))*60;
    const Address owner=u32(s+0xc);
    if (!(u32(owner+0xc)&0x40)) {
      const auto holder=u32(owner+0x50);
      if (holder) holder_call(holder,7,{1});
    } else {
      const auto weapons=execute(Operation::PlayerWeapons,{static_cast<Raw>(owner)});
      for (const auto item:weapons) write_disease(item,value,frames,frames);
    }
    const auto block=service_scalar(PoolNew,{0xbc});
    if (block) {
      const auto at=vec(Address(u32(s+0xc))+0x10);
      execute(Operation::LightEffect,{block,at[0],at[1],at[2],0,0,
          0x43000000,0x43000000,ZERO,0x43000000,
          0x43000000,0x42e00000,ZERO,0x43000000,
          0x42dc0000,ZERO,0x3f199999,HALF});
    }
    const Address current=u32(s+0xc);
    write(current+0x9e,2,30); write(current+0x9a,2,0x80);
    write(current+0x98,2,0x80); write(current+0x9c,2,0);
    sound(0x553120,0x553128,vec(Address(u32(s+0xc))+0x10));
    return {};
  }
  case Operation::DiseaseBladeRun: {
    const auto frame=s32(u32(0x44fdcc));
    if (frame<s32(u32(s+0x18))) {
      const auto reach=u32(s+0x20);
      if (!eq(reach,ZERO) && (u32(Address(u32(s+0xc))+0xc)&0x40) && frame%30==0) {
        const auto found=service(CollectObjectsInBox,{sub(a[1],reach),sub(a[2],reach),sub(a[3],reach),
            add(a[1],reach),add(a[2],reach),add(a[3],reach),0x40});
        const Address owner=u32(s+0xc);
        for (const auto other:found) {
          if (owner==other || !(u32(Address(other)+0xc)&0x40)) continue;
          if (s32(u32(0x4f711c))) {
            const Address current=u32(s+0xc), table=u32(current+0x48);
            if (!service_scalar(CreatureTeam,{static_cast<Raw>(current+half(table+22*8)),other})) continue;
          }
          const auto weapons=execute(Operation::PlayerWeapons,{other});
          const auto value=scalar(u32(s+8),12,{3,level(s),u32(s+0xc),0});
          write_disease(weapons[0],value,31,31); write_disease(weapons[1],value,31,31);
        }
      }
      return {};
    }
    const Address owner=u32(s+0xc);
    if ((u32(owner+0xc)&0x60)==0x20 && u32(owner+0x50))
      holder_call(u32(owner+0x50),8,{1,0});
    if (s) destroy(s);
    return {};
  }
  case Operation::LightningStrikeCast:
  case Operation::PusStormCast: {
    auto args=a; args.insert(args.begin(),op==Operation::PusStormCast ? 1u : 0u);
    return execute(Operation::StormCast,args);
  }
  case Operation::StormCast: {
    if (a[0]>1) throw Unsupported("storm variant must be boolean");
    const auto &cfg=a[0] ? pus : lightning;
    const Address skill=a[1], caster=a[3];
    const auto rank=scalar(skill,14,{a[2],cfg.adjust});
    const auto range=a[0] ? skill_call(skill,8,{1,rank,0,spread20})
        : execute(Operation::StatisticSpread,{a[1],1,rank,a[3]});
    if (range.size()!=2) throw Unsupported("storm range must have two values");
    const auto steps=from_int(scalar(skill,12,{0,rank,a[3],10}));
    const auto seconds=scalar(skill,12,{2,rank,a[3],10});
    const auto speed=from_int(scalar(skill,12,{3,rank,a[3],10}));
    const auto ahead=(u32(caster+0xc)&0x40) ? 0x41400000u : 0xc0c00000u;
    const auto x=add(mul(mul(cos16(a[7]),steps),ahead),a[4]);
    const auto y=add(mul(mul(sin16(a[7]),steps),ahead),a[5]);
    const auto ground=service_scalar(TerrainHeight,{u32(0x44fdc8),x,y,add(ZERO,a[6]),0x41c00000});
    const std::vector<Raw> velocity{mul(cos16(a[7]),speed),mul(sin16(a[7]),speed),ZERO};
    const auto interval=20u-static_cast<Raw>(to_int(mul(from_int(rank),HALF)));
    if (a[0]) sound(cfg.lump,cfg.sound,{a[4],a[5],a[6]});
    const auto block=service_scalar(PoolNew,{cfg.size});
    if (block) execute(Operation::StormCtor,{a[0],block,x,y,ground,velocity[0],velocity[1],velocity[2],
        seconds*60u,interval,range[0],range[1],a[3],steps});
    if (!a[0]) sound(cfg.lump,cfg.sound,{a[4],a[5],a[6]});
    return {0};
  }
  case Operation::StormCtor: {
    if (a[0]>1) throw Unsupported("storm variant must be boolean");
    const auto &cfg=a[0] ? pus : lightning;
    const Address obj=a[1], light=obj+cfg.light;
    const std::vector<Raw> centre{a[2],a[3],a[4]};
    service(GameObjectConstructor,{a[1],a[2],a[3],a[4],0,0});
    write(obj+0x48,4,cfg.table);
    const auto glow=fmin(0x43960000,mul(mul(a[13],0x3fe00000),0x41400000));
    write(obj+cfg.handle,4,0);
    execute(Operation::LightRecord,{static_cast<Raw>(light),cfg.colour[0],cfg.colour[1],cfg.colour[2],cfg.colour[3],
        glow,div(ONE,mul(glow,glow)),cfg.extra,a[2],a[3],a[4]});
    service(LightLink,{static_cast<Raw>(light)});
    write(obj+0xc,4,u32(obj+0xc)|0x420f);
    const std::vector<Raw> step{mul(a[5],dt),mul(a[6],dt),mul(a[7],dt)};
    write_vec(obj+0x4c,step); write(obj+0x78,4,mul(a[13],0x41400000));
    write(obj+0x28,1,255); write(obj+0x68,4,a[9]);
    write(obj+0x70,4,a[10]); write(obj+0x74,4,a[11]);
    if (a[0]) write(obj+0x7c,4,sqrt(0x45160189));
    write(obj+0x29,1,255);
    const auto frame=u32(0x44fdcc); write(obj+0x58,4,frame+1u);
    if ((u32(Address(a[12])+0xc)&0x60)==0x20) {
      write(obj+0x58,4,frame+41u);
      const auto block=service_scalar(PoolNew,{0xbc});
      if (block) {
        const auto radius=u32(obj+0x78);
        execute(Operation::LightEffect,{block,add(centre[0],ZERO),centre[1],add(centre[2],0x41400000),0,1,
            cfg.warning[0],cfg.warning[1],cfg.warning[2],cfg.warning[3],
            cfg.warning[0],cfg.warning[1],cfg.warning[2],cfg.warning[3],
            fmin(0x43960000,add(radius,radius)),ZERO,cfg.extra,cfg.warning_life});
      }
    }
    write(obj+0x6c,4,UINT32_MAX);
    const auto start=u32(obj+0x58);
    if (a[0]) {
      const auto fall=u32(obj+0x7c);
      write(obj+0x60,4,start+a[8]); write(obj+0x5c,4,start+static_cast<Raw>(to_int(fall)));
      write(obj+0x64,4,start+static_cast<Raw>(to_int(fall))+a[8]);
      std::vector<Raw> at(3);
      for (unsigned k=0;k<3;++k) at[k]=add(centre[k],mul(step[k],neg(fall)));
      write_vec(light+0x30,at);
    } else {
      write(obj+0x5c,4,start); write(obj+0x64,4,start+a[8]); write(obj+0x60,4,start+a[8]);
      write_vec(light+0x30,centre);
    }
    service(LightRelink,{static_cast<Raw>(light)});
    execute(Operation::HandleLink,{static_cast<Raw>(obj+cfg.handle),a[12],a[12]});
    service(ObjectRunEveryFrame,{a[1]}); return {};
  }
  case Operation::LightningStrikeRun:
  case Operation::PusStormRun:
    return execute(Operation::StormRun,{op==Operation::PusStormRun ? 1u : 0u,a[0]});
  case Operation::StormRun: {
    if (a[0]>1) throw Unsupported("storm variant must be boolean");
    const auto &cfg=a[0] ? pus : lightning;
    const Address obj=a[1], light=obj+cfg.light;
    auto frame=s32(u32(0x44fdcc));
    if (s32(u32(obj+0x58))>frame) return {};
    if (s32(u32(obj+0x64))<frame || !u32(obj+cfg.handle)) {
      if (obj) {
        const auto table=u32(obj+0x48);
        service(ObjectDestroyCallback,{static_cast<Raw>(obj+half(Address(table)+8)),3});
      }
      return {};
    }
    for (unsigned k=0;k<3;++k) write(obj+0x10+4*k,4,add(u32(obj+0x10+4*k),u32(obj+0x4c+4*k)));
    service(ObjectUpdateGridCell,{a[1]});
    auto centre=vec(obj+0x10);
    if (a[0]) {
      const auto back=neg(u32(obj+0x7c));
      for (unsigned k=0;k<3;++k) centre[k]=add(u32(obj+0x10+4*k),mul(u32(obj+0x4c+4*k),back));
    }
    write_vec(light+0x30,centre); service(LightRelink,{static_cast<Raw>(light)});
    auto glow=mul(u32(obj+0x78),0x3fe00000);
    glow=add(glow,mul(mul(sin16(static_cast<Raw>(frame)*1024u),0x3d4ccccc),glow));
    const auto spawn_end=s32(u32(obj+0x60));
    if (spawn_end<frame) {
      const auto left=div(from_int(static_cast<Raw>(std::int64_t(frame)-spawn_end)),
          from_int(static_cast<Raw>(std::int64_t(s32(u32(obj+0x64)))-spawn_end)));
      glow=mul(glow,sub(ONE,left));
    }
    glow=fmin(0x43960000,glow); write(light+0x1c,4,glow); write(light+0x24,4,glow);
    const auto falloff=div(ONE,mul(glow,glow)); write(light+0x28,4,falloff); write(light+0x20,4,falloff);
    if (s32(u32(obj+0x6c))<0) write(obj+0x6c,4,static_cast<Raw>(frame));
    if (frame<spawn_end && s32(u32(obj+0x6c))<=frame) {
      const auto b=ball(obj+0x44); const auto radius=u32(obj+0x78);
      const auto x=add(u32(obj+0x10),mul(b[0],radius));
      const auto y=add(u32(obj+0x14),mul(b[1],radius));
      const auto z=add(u32(obj+0x18),mul(b[2],radius));
      const auto ground=service_scalar(TerrainHeight,{u32(0x44fdc8),x,y,z,0x41000000});
      if (a[0]) {
        const auto reach=mul(add(mul(mul(from_int(own_draw(obj+0x44)),TWO_M16),0x41200000),0x41200000),0x41400000);
        const auto angle=own_draw(obj+0x44);
        const auto sx=add(x,mul(reach,cos16(angle))), sy=add(y,mul(reach,sin16(angle))), sz=add(ground,0x43700000);
        const auto inverse=div(ONE,u32(obj+0x7c));
        const auto dx=mul(sub(x,sx),inverse), dy=mul(sub(y,sy),inverse);
        const auto block=service_scalar(PoolNew,{0x160});
        if (block) execute(Operation::PustuleCtor,{block,sx,sy,sz,x,y,ground,dx,dy,ZERO,angle});
      } else {
        const auto block=service_scalar(PoolNew,{0x60});
        if (block) service(LightningBoltConstructor,{block,x,y,ground,17,0});
      }
      const auto roll=own_draw(obj+0x44); const auto interval=s32(u32(obj+0x68));
      const auto rest=interval ? std::int64_t(roll)%interval : std::int64_t(roll);
      write(obj+0x6c,4,static_cast<Raw>(std::int64_t(s32(u32(0x44fdcc)))+std::int64_t(interval)/2+rest));
    }
    frame=s32(u32(0x44fdcc)); const auto first=s32(u32(obj+0x5c));
    if (first<=frame && (std::int64_t(frame)-first)%30==0) {
      const auto radius=u32(obj+0x78), twice=add(radius,radius);
      const auto low=s32(u32(obj+0x70))/2, high=s32(u32(obj+0x74))/2;
      const auto found=service(CollectObjectsInBox,{sub(centre[0],radius),sub(centre[1],radius),sub(centre[2],twice),
          add(centre[0],radius),add(centre[1],radius),add(add(centre[2],radius),radius),64});
      const auto owner=u32(Address(u32(obj+cfg.handle))+0xc);
      for (const auto other:found) {
        const auto table=u32(Address(owner)+0x48);
        if (service_scalar(CreatureEnemy,{static_cast<Raw>(Address(owner)+half(Address(table)+160)),other})) {
          std::vector<Raw> args{other,owner,100000,0,0,0x40};
          args.insert(args.end(),16,0); args.insert(args.end(),{255,255,cfg.element,
              static_cast<Raw>(low)&0xffff,static_cast<Raw>(high)&0xffff,0});
          for (unsigned k=0;k<3;++k) args.insert(args.end(),{0xffff,0,0,0});
          args.insert(args.end(),{0,from_int(static_cast<Raw>(low)),from_int(static_cast<Raw>(high))});
          service(DamageApply,std::move(args));
        }
      }
    }
    return {};
  }
  case Operation::PustuleCtor:
    service(GameObjectConstructor,{a[0],a[1],a[2],a[3],0,0});
    write(s+0x48,4,0x51b8c0); write_vec(s+0x4c,{a[4],a[5],a[6]}); write_vec(s+0x58,{a[7],a[8],a[9]});
    write(s+0x68,2,a[10]); write(s+0x64,4,0); write(s+0x6c,4,0); write(s+0xc,4,u32(s+0xc)|0x420d);
    service(ObjectUpdateGridCell,{a[0]}); service(ObjectRunEveryFrame,{a[0]}); return {};

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
    spells::constructor_tail([&]{return draw();},
        [&](Raw seed){write(s+0x10,4,seed);},
        [&]{write(s+0x14,4,a[4]);});
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
  case Operation::HandleLinkStack:
  case Operation::HandleClearStack:
    if (a[0] != 0) throw Unsupported("stack handle marker must be zero");
    // Preserve the delegated return, calls and callback effects verbatim.
    return service(op == Operation::HandleLinkStack ? HandleLinkStackCallback : HandleClearStack, a);
  case Operation::HandleLink: {
    const auto front = detail::skill_handle_kernel::paired_link(s, Address(a[1]), a[2],
        [&] { return handle_take(); },
        [&](Address head, Raw node) { handle_insert(head, node); },
        [&](Address at, unsigned width, Raw value) { write(at, width, value); },
        [](Address at, Address displacement) { return at + displacement; });
    return {front};
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
    level = s32(kernel::capped_level(static_cast<Raw>(level), static_cast<Raw>(cap)));
    return {kernel::tier2_fallback(half(indexed(s, 0x38, 2, a[1])))
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
        return kernel::range_combine(x, y, side != 0);
      };
      std::int32_t bound;
      if (a[3]) {
        auto sample = [&]() {
          return combine(value,
                         kernel::range_random_scale(value, draw(), a[4]));
        };
        const auto probe = sample();
        const auto second = sample();
        bound = to_int(le(ZERO, probe) ? add(second, HALF) : sub(second, HALF));
      } else
        bound = round_half_away(combine(value, kernel::range_scale(value, a[4])));
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
