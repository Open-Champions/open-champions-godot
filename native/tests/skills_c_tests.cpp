#include "champions/skills_c.hpp"
#include <iostream>
#include <array>
#include "champions/core_arithmetic.hpp"
namespace sk = champions::simulation::rta::skills_c;
using champions::simulation::rta::ONE;
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void refuses(F f) {
  try {
    f();
  } catch (const sk::Unsupported &) {
    return;
  }
  throw std::runtime_error("expected explicit refusal");
}
void base(sk::Model &m) {
  m.mem.poke(0x1060, 4, 0x2000);
  m.mem.poke(0x2000 + 8 * 10 + 4, 4, 0x3c85fc);
  m.mem.poke(0x2000 + 8 * 14 + 4, 4, 0x3c88dc);
}
} // namespace
int main() {
  try {
    sk::Model m;
    base(m);
    // Active entry points never silently succeed for the unsupported range.
    for (unsigned id = 69; id <= 101; ++id)
      refuses([&] { m.cast_or_run(id); });
    refuses([&] { m.execute(static_cast<sk::Operation>(66), {0x1000}); });
    refuses([&] { m.execute(sk::Operation::Stat, {0x1000}); });
    refuses([&] { m.skill_call(0x1000, 16, {}); });
    refuses([&] { m.skill_call(UINT64_MAX, 10, {}); });
    // Unknown bodies record the callback prefix then require a handler.
    refuses([&] { m.skill_call(0x1000, 11, {0, 20, 10}); });
    require(m.calls.size() == 1 && m.calls[0].slot == 11,
            "missing callback prefix");
    // A handler can replace itself without invalidating its active callable.
    m.callbacks[11] = [](sk::Model &self, const auto &) {
      self.mem.store(0x3000, 2, 0x1234);
      self.callbacks[11] = [](sk::Model &, const auto &) {
        return std::vector<std::uint32_t>{8};
      };
      return std::vector<std::uint32_t>{7};
    };
    require(m.skill_call(0x1000, 11, {0, 20, 10})[0] == 7,
            "active handler changed");
    require(m.skill_call(0x1000, 11, {0, 20, 10})[0] == 8,
            "replacement not used");
    require(m.mem.load(0x3000, 2) == 0x1234 && m.mem.log().size() == 1,
            "callback write lost");
    // Recursion refuses explicitly; the depth guard restores after failure.
    m.callbacks[11] = [](sk::Model &self, const auto &a) {
      return self.skill_call(a[0], 11, {0, 20, 10});
    };
    refuses([&] { m.skill_call(0x1000, 11, {0, 20, 10}); });
    m.callbacks[11] = [](sk::Model &, const auto &) {
      return std::vector<std::uint32_t>{9};
    };
    require(m.skill_call(0x1000, 11, {0, 20, 10})[0] == 9,
            "depth not restored");
    // Queries refuse cyclic lists, and a failed runtime call releases depth.
    m.mem.poke(0x5000, 4, 0x5100);
    m.mem.poke(0x5100, 4, 0x5100);
    refuses([&] { m.execute(sk::Operation::HandleItems, {0x5000}); });
    m.mem.poke(0x5000, 4, 0);
    require(m.execute(sk::Operation::HandleItems, {0x5000}).empty(),
            "empty list/depth recovery");
    require(m.execute(sk::Operation::SpellBase,
                      {0x6000, 256, 0x3000, 0x1000, 0x1234}).empty(),
            "base constructor return");
    require(m.mem.load(0x6000,1) == 0 && m.mem.load(0x6014,4) == 0x1234,
            "level narrowing/final table");
    require(m.draws == 1, "constructor shared draw count");
    m.execute(sk::Operation::SpellBall, {0x6000});
    require(m.draws == 1, "own RNG changed shared count");
    // Reference this-adjustment wraps raw32; subsequent memory spans refuse.
    m.mem.poke(0x2050, 2, 0xfff0);
    m.external.insert(10);
    m.callbacks[10] = [](sk::Model &, const auto &a) {
      require(a[0] == 0xff0, "signed this adjustment");
      return std::vector<std::uint32_t>{17};
    };
    require(m.skill_call(0x1000, 10, {0, 2})[0] == 17,
            "forced callback failed");
    require(m.execute(sk::Operation::ManaDefault, {0x1000, 2})[0] == ONE,
            "default mana wrong");
    // Protection entry points require engine callbacks and preserve failure prefixes.
    sk::Model protection;
    refuses([&] { protection.execute(sk::Operation::RetributionCanUse,{0x1000,0x3000}); });
    require(protection.calls.size()==1 && protection.calls[0].slot==sk::TypeInfo,
            "type lookup refusal prefix");
    protection.callbacks[sk::TypeInfo]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x3d18a0}, "type getter argument");
      self.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &) {
        return std::vector<std::uint32_t>{9};
      };
      return std::vector<std::uint32_t>{8};
    };
    protection.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>({0x3000,8}),"find-spell arguments");
      return std::vector<std::uint32_t>{0};
    };
    require(protection.execute(sk::Operation::RetributionCanUse,{0x1000,0x3000})[0]==1,
            "can-use lookup");
    protection.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {
      return std::vector<std::uint32_t>{0};
    };
    base(protection);
    require(protection.execute(sk::Operation::FrostArmorCast,{0x1000,5,0x3000,0,0})[0]==0
            && protection.draws==0,"failed allocation constructed spell");
    protection.mem.poke(0x2014,4,0x317b30);
    require(protection.skill_call(0x1000,2,{5,0x3000,0,0})[0]==0,
            "active cast virtual dispatch");
    protection.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{9}; };
    protection.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{0}; };
    for (auto body : {0x3142bcu,0x314e1cu,0x317afcu}) {
      protection.mem.poke(0x200c,4,body);
      require(protection.skill_call(0x1000,1,{0x3000})[0]==1,"protection can-use body dispatch");
    }
    for (auto body : {0x3142f0u,0x314e50u,0x317b30u}) {
      protection.mem.poke(0x2014,4,body);
      require(protection.skill_call(0x1000,2,{5,0x3000,0,0})[0]==0,"protection cast body dispatch");
    }
    protection.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,20}),"mana wrapper arguments");
      return std::vector<std::uint32_t>{ONE};
    };
    for (auto body : {0x314454u,0x314fd8u,0x317ce8u}) {
      protection.mem.poke(0x202c,4,body);
      require(protection.skill_call(0x1000,5,{5})[0]==ONE,"protection mana body dispatch");
    }
    protection.mem.poke(0x6000,1,5);
    protection.mem.poke(0x6008,4,0x1000);
    protection.mem.poke(0x600c,4,0x3000);
    protection.mem.poke(0x1060,4,0x2000);
    protection.external.insert(12);
    protection.callbacks[12]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{10}; };
    protection.callbacks[sk::CreatureModel]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{7}; };
    protection.callbacks[sk::SamplePointCount]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{0}; };
    refuses([&] { protection.execute(sk::Operation::FrostArmorRun,{0x6000,0,0}); });
    require(protection.mem.load(0x6010,4)!=0 && protection.draws==0,
            "zero-point failure lost private draws");
    protection.callbacks[sk::SamplePointCount]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{7}; };
    protection.callbacks[sk::SamplePoint]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{1,2}; };
    refuses([&] { protection.execute(sk::Operation::FrostArmorRun,{0x6000,0,0}); });
    protection.callbacks[sk::SpellDestroyCallback]=[](sk::Model &self,const auto &) {
      self.mem.poke(0x3000,2,0x1234);
      throw sk::Unsupported("authored service failure");
      return std::vector<std::uint32_t>{};
    };
    refuses([&] { protection.execute(sk::Operation::SpellDestroy,{0x6000}); });
    require(protection.mem.load(0x3000,2)==0x1234,"service mutation lost on failure");
    protection.callbacks[sk::SpellDestroyCallback]=[](sk::Model &self,const auto &a) {
      return self.execute(sk::Operation::SpellDestroy,{a[0]});
    };
    refuses([&] { protection.execute(sk::Operation::SpellDestroy,{0x6000}); });
    protection.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{}; };
    require(protection.execute(sk::Operation::SpellDestroy,{0x6000}).empty(),"service depth recovery");
    // Blood Lust extends the existing registry, preserving signed this and
    // callback lifetime. No missing presentation service silently succeeds.
    sk::Model blood;
    base(blood);
    blood.mem.poke(0x200c,4,0x30300c);
    blood.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x3d0a00},"Blood Lust type getter");
      return std::vector<std::uint32_t>{9};
    };
    blood.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &) {
      return std::vector<std::uint32_t>{0};
    };
    require(blood.skill_call(0x1000,1,{0x3000})[0]==1,"Blood Lust can-use dispatch");
    blood.mem.poke(0x2014,4,0x303040);
    blood.mem.poke(0x5000,4,0x3f800000);
    blood.mem.poke(0x5004,4,0x80000000);
    blood.mem.poke(0x5008,4,0x40000000);
    blood.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x551a90,0x551a98}),"Blood Lust cast sound");
      return std::vector<std::uint32_t>{8};
    };
    blood.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({8,0x3f800000,0x80000000,0x40000000,ONE}),
              "virtual cast position values");
      return std::vector<std::uint32_t>{};
    };
    blood.callbacks[sk::PoolNew]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x4c},"Blood Lust allocation size");
      return std::vector<std::uint32_t>{0};
    };
    require(blood.skill_call(0x1000,2,{5,0x3000,0x5000,0})[0]==0 && blood.draws==0,
            "Blood Lust virtual cast/null allocation");
    blood.mem.poke(0x202c,4,0x303220);
    blood.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,20}),"Blood Lust mana wrapper");
      return std::vector<std::uint32_t>{ONE};
    };
    require(blood.skill_call(0x1000,5,{5})[0]==ONE,"Blood Lust mana dispatch");
    refuses([&] {blood.execute(sk::Operation::GhostHands,{0x3000});});
    require(blood.calls.back().slot==sk::NetObjectOrGhost,"ghost refusal prefix");
    blood.callbacks[sk::NetObjectOrGhost]=[](sk::Model &,const auto &) {
      return std::vector<std::uint32_t>{0x3000};
    };
    blood.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {
      return std::vector<std::uint32_t>{1,2};
    };
    refuses([&] {blood.execute(sk::Operation::GhostHands,{0x3000});});
    blood.callbacks[sk::AttachPoint]=[](sk::Model &self,const auto &a) {
      self.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {
        return std::vector<std::uint32_t>{4,5,6};
      };
      require(a[6]==0,"first hand index");
      return std::vector<std::uint32_t>{1,2,3};
    };
    require(blood.execute(sk::Operation::GhostHands,{0x3000})==
              std::vector<std::uint32_t>({1,2,3,4,5,6}),"hand callback replacement");
    blood.callbacks[sk::NetObjectOrGhost]=[](sk::Model &self,const auto &a) {
      return self.execute(sk::Operation::GhostHands,{a[0]});
    };
    refuses([&] {blood.execute(sk::Operation::GhostHands,{0x3000});});
    blood.callbacks[sk::NetObjectOrGhost]=[](sk::Model &,const auto &) {
      return std::vector<std::uint32_t>{0x3000};
    };
    require(blood.execute(sk::Operation::GhostHands,{0x3000}).size()==6,"Blood Lust depth recovery");
    const auto calls=blood.calls.size();
    refuses([&] {blood.execute(sk::Operation::ShoutAnimation,{0x3000,1,2,100,0});});
    refuses([&] {blood.execute(sk::Operation::ShoutAnimation,{0x3000,1,2,0,2,1});});
    require(blood.calls.size()==calls,"malformed shout mutated state");
    // Store outputs are sparse memory pokes, never model or callee store logs.
    const auto stores=blood.mem.log().size(), own=blood.writes.size();
    blood.execute(sk::Operation::CalleeStore,{0x6000,8,0x01234567,0x89abcdef});
    require(blood.mem.load(0x6000,8)==UINT64_C(0x89abcdef01234567) &&
            blood.mem.log().size()==stores && blood.writes.size()==own,"callee store provenance");
    sk::Model shared;
    base(shared);
    shared.mem.poke(0x2014,4,0x2ddb90);
    require(shared.skill_call(0x1000,2,{5,0x3000,0,0})==std::vector<std::uint32_t>{0},
            "Riposte cast dispatch");
    shared.callbacks[14]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,1}),"Dodge adjust arguments");
      return std::vector<std::uint32_t>{7};
    };
    shared.external.insert(14);
    shared.mem.poke(0x202c,4,0x2e8a88);
    require(shared.skill_call(0x1000,5,{5})==std::vector<std::uint32_t>{0},"Dodge mana dispatch");
    shared.callbacks[14]=[](sk::Model &self,const auto &a) {
      self.mem.poke(0x3000,1,77); throw sk::Unsupported("authored adjust failure");
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {shared.execute(sk::Operation::DodgeManaCost,{0x1000,5});});
    require(shared.mem.load(0x3000,1)==77,"Dodge mana failure mutation");
    shared.callbacks[14]=[](sk::Model &,const auto &) { return std::vector<std::uint32_t>{5}; };
    shared.callbacks[sk::PoolNew]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x24},"Healing allocation size");
      return std::vector<std::uint32_t>{0};
    };
    shared.mem.poke(0x2014,4,0x2e8cf8);
    require(shared.skill_call(0x1000,2,{5,0x3000,0,0})==std::vector<std::uint32_t>{0},"Healing cast dispatch");
    shared.mem.poke(0x202c,4,0x2e8e94);
    shared.callbacks[15]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{ONE};};
    require(shared.skill_call(0x1000,5,{5})==std::vector<std::uint32_t>{ONE},"Healing mana dispatch");
    const auto before=shared.calls.size();
    require(shared.execute(sk::Operation::DodgeFraction,{0x1000,0})==std::vector<std::uint32_t>{0},
            "rank-zero Dodge");
    require(shared.execute(sk::Operation::RiposteCounterAttack,{0x1000,0x3000,0x4000,0}).empty()
            && shared.calls.size()==before,"rank-zero Riposte side effects");
    shared.mem.poke(0x6008,4,0x1000); shared.mem.poke(0x600c,4,0x3000);
    shared.mem.poke(0x300c,4,0x20);
    shared.callbacks[12]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    shared.external.insert(12);
    refuses([&] {shared.execute(sk::Operation::HealingRun,{0x6000,0,0});});
    require(shared.calls.back().slot==sk::CollectObjectsInBox,"Healing missing-world prefix");
    shared.callbacks[sk::CollectObjectsInBox]=[](sk::Model &self,const auto &) {
      self.callbacks[sk::CollectObjectsInBox]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{0x3000};
    };
    shared.execute(sk::Operation::HealingRun,{0x6000,0,0});
    shared.execute(sk::Operation::HealingRun,{0x6000,0,0});
    shared.callbacks[sk::CollectObjectsInBox]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::HealingRun,{0x6000,0,0});
    };
    refuses([&] {shared.execute(sk::Operation::HealingRun,{0x6000,0,0});});
    shared.callbacks[sk::CollectObjectsInBox]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    require(shared.execute(sk::Operation::HealingRun,{0x6000,0,0}).empty(),"Healing depth recovery");
    // Terrain callback mutations precede reading prediction-copy height.
    shared.mem.poke(0x300c,4,0);
    shared.mem.poke(0x6000,1,0);
    shared.mem.poke(0x1010,2,4); // exactly one ground particle
    shared.callbacks[12]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    shared.callbacks[sk::NetObjectOrGhost]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0x7000};};
    for (auto id:{sk::ParticleGroupBegin,sk::ParticleGroupSetMatrix,sk::ParticleGroupEnd})
      shared.callbacks[id]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    shared.callbacks[sk::TerrainHeightAtPoint]=[](sk::Model &self,const auto &) {
      self.mem.poke(0x7018,4,ONE);
      return std::vector<std::uint32_t>{ONE};
    };
    unsigned particles=0;
    shared.callbacks[sk::ParticleSpawn]=[&particles](sk::Model &,const auto &a) {
      require(a[3]==0,"terrain mutation must precede ghost height read"); ++particles;
      return std::vector<std::uint32_t>{};
    };
    shared.execute(sk::Operation::HealingRun,{0x6000,0,0});
    require(particles==1,"Healing terrain mutation test did not reach particle");
    sk::Model summon;
    base(summon);
    summon.mem.poke(0x200c,4,0x2ff5ec);
    require(summon.skill_call(0x1000,1,{0x3000})==std::vector<std::uint32_t>{1},
            "Summon Arrows can-use dispatch");
    summon.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,20}),"summon mana arguments");
      return std::vector<std::uint32_t>{ONE};
    };
    for (auto body:{0x2fe938u,0x2ff848u}) {
      summon.mem.poke(0x202c,4,body);
      require(summon.skill_call(0x1000,5,{5})==std::vector<std::uint32_t>{ONE},"summon mana dispatch");
    }
    summon.mem.poke(0x5000,4,0x80000000);
    summon.mem.poke(0x5004,4,ONE);
    summon.mem.poke(0x5008,4,0x40000000);
    summon.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{8};};
    summon.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({8,0x80000000,ONE,0x40000000,ONE}),"summon virtual position");
      return std::vector<std::uint32_t>{};
    };
    summon.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    for (auto body:{0x2fe644u,0x2ff5f4u}) {
      summon.mem.poke(0x2014,4,body);
      require(summon.skill_call(0x1000,2,{5,0x3000,0x5000,0})==std::vector<std::uint32_t>{0}
              && summon.draws==0,"summon virtual cast/null allocations");
    }
    // The item is a value, with no synthesized unwritten stack bytes. General
    // slots retain their lengths and halfword narrowing even beyond four slots.
    const auto item=sk::item_record(0xffffffff,0xffffffff,{{},{0xffffffff,65536}});
    require(item.size()==29 && item[1]==65535 && item[20]==4 && item[21]==0 &&
            item[22]==2 && item[23]==65535 && item[24]==0,"canonical item transport");
    const auto before_summon=summon.calls.size();
    refuses([&] {summon.execute(sk::Operation::ItemRecord,{1,2,1,4,1,2});});
    refuses([&] {summon.execute(sk::Operation::ItemRecord,{1,2,0,7});});
    refuses([&] {summon.execute(sk::Operation::SummonCtor,{2,0x6000,5,0x3000,0x1000});});
    require(summon.calls.size()==before_summon,"malformed summon changed calls");
    refuses([&] {summon.execute(sk::Operation::SummonAxeBuildItem,{5,10,0});});
    require(summon.calls.back().slot==sk::ItemParseDescriptor,"item parser missing prefix");
    summon.callbacks[sk::ItemParseDescriptor]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {summon.execute(sk::Operation::SummonAxeBuildItem,{5,10,0});});
    summon.callbacks[sk::ItemParseDescriptor]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x551530,0xffffffff,0}),"axe item tier/parser variant");
      self.callbacks[sk::ItemParseDescriptor]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0x8000};};
      return std::vector<std::uint32_t>{0x7000};
    };
    require(summon.execute(sk::Operation::SummonAxeBuildItem,{5,10,0}).back()==0x7000 &&
            summon.execute(sk::Operation::SummonAxeBuildItem,{5,10,0}).back()==0x8000,"item parser self replacement");
    summon.callbacks[sk::ItemParseDescriptor]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::SummonAxeBuildItem,{5,10,0});
    };
    refuses([&] {summon.execute(sk::Operation::SummonAxeBuildItem,{5,10,0});});
    summon.callbacks[sk::ItemParseDescriptor]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0x8000};};
    require(summon.execute(sk::Operation::SummonAxeBuildItem,{5,10,0}).back()==0x8000,"summon depth recovery");
    // An allocated cast dispatches the light callback before the spell pool.
    summon.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    unsigned allocations=0;
    summon.callbacks[sk::PoolNew]=[&allocations](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{allocations++ ? 0x20u : 0xbcu},"summon allocation order");
      return std::vector<std::uint32_t>{allocations==1 ? 0x9000u : 0u};
    };
    summon.callbacks[sk::LightEffectConstructor]=[](sk::Model &,const auto &a) {
      require(a.size()==18 && a[0]==0x9000 && a[3]==0x42180000 && a[4]==0x3000 &&
              a[5]==0 && a[14]==0x42f00000,"summon light argument order");
      return std::vector<std::uint32_t>{};
    };
    require(summon.execute(sk::Operation::SummonAxeCast,{0x1000,5,0x3000,0,ONE,0x40000000,0})[0]==0 &&
            allocations==2,"summon light and null spell");
    sk::Model critical;
    base(critical);
    critical.mem.poke(0x200c,4,0x2e7224);
    critical.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x3cf000},"CriticalHit type getter");
      return std::vector<std::uint32_t>{9};
    };
    critical.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &) {
      return std::vector<std::uint32_t>{0};
    };
    require(critical.skill_call(0x1000,1,{0x3000})[0]==1,"CriticalHit can-use dispatch");
    critical.mem.poke(0x2014,4,0x2e74b0);
    critical.callbacks[sk::PoolNew]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x5c},"CriticalHit allocation");
      return std::vector<std::uint32_t>{0};
    };
    // Cast ignores the position pointer and does not route rank through slot 14.
    critical.callbacks[14]=[](sk::Model &,const auto &) {
      throw std::runtime_error("unexpected CriticalHit rank adjustment");
      return std::vector<std::uint32_t>{};
    };
    require(critical.skill_call(0x1000,2,{128,0x3000,UINT32_MAX,0})[0]==0 && critical.draws==0,
            "CriticalHit virtual cast/null allocation");
    critical.mem.poke(0x202c,4,0x2e7690);
    critical.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,128,20}),"CriticalHit mana arguments");
      return std::vector<std::uint32_t>{ONE};
    };
    require(critical.skill_call(0x1000,5,{128})[0]==ONE,"CriticalHit mana dispatch");
    critical.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {critical.execute(sk::Operation::CriticalHitCanUse,{0x1000,0x3000});});
    critical.callbacks[sk::TypeInfo]=[](sk::Model &self,const auto &) {
      self.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{2};};
      return std::vector<std::uint32_t>{1};
    };
    require(critical.execute(sk::Operation::CriticalHitCanUse,{0x1000,0x3000})[0]==1,
            "CriticalHit callback replacement");
    critical.callbacks[sk::TypeInfo]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::CriticalHitCanUse,{0x1000,0x3000});
    };
    refuses([&] {critical.execute(sk::Operation::CriticalHitCanUse,{0x1000,0x3000});});
    critical.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{2};};
    require(critical.execute(sk::Operation::CriticalHitCanUse,{0x1000,0x3000})[0]==1,
            "CriticalHit callback depth recovery");
    refuses([&] {critical.execute(sk::Operation::CriticalHitCtor,{0x6000,5,0x3000,0x1000});});
    require(critical.draws==1 && critical.calls.back().slot==sk::LightLink &&
            critical.mem.load(0x602e,2)==0xd7,"CriticalHit missing light callback prefix");
    refuses([&] {critical.execute(sk::Operation::CriticalHitBonus,{0x1000,0x3000});});
    refuses([&] {critical.execute(sk::Operation::LightRecord,{0x6000,0});});
    sk::Model bonus;
    base(bonus);
    bonus.mem.poke(0x3000+0x3c8,4,0);
    bonus.mem.poke(0x4f8420,4,2);
    bonus.mem.poke(0x4f8440,4,0);
    bonus.mem.poke(0x4f8444,4,1);
    bonus.mem.poke(0x4d8af0+0x4c30+0x43d8,4,0x4000);
    bonus.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    bonus.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &a) {
      require(a[0]==0x4000,"bonus must exclude attacker's slot");
      return std::vector<std::uint32_t>{0x6000};
    };
    bonus.external={10,14};
    bonus.callbacks[10]=[](sk::Model &,const auto &a) {
      require(a[2]==7,"bonus must use highest rank");
      return std::vector<std::uint32_t>{a[1]==1 ? 2u : 150u};
    };
    bonus.callbacks[14]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,7,20}),"bonus final adjustment");
      return std::vector<std::uint32_t>{7};
    };
    unsigned rank_calls=0;
    bonus.callbacks[sk::PlayerGetSkillRank]=[&](sk::Model &,const auto &a) {
      const std::array<unsigned,3> ids{53,81,33};
      require(a==std::vector<std::uint32_t>({1,ids[rank_calls%3]}),"bonus rank call order");
      const std::array<std::uint32_t,3> ranks{0xfffffffe,3,7};
      return std::vector<std::uint32_t>{ranks[rank_calls++%3]};
    };
    require(bonus.execute(sk::Operation::CriticalHitBonus,{0x1000,0x3000,0})[0]==
            champions::simulation::rta::mul(champions::simulation::rta::from_int(150),0x3c23d70a)
            && rank_calls==3,"bonus aura rank maximum/first eligible source");
    bonus.callbacks[sk::PlayerGetSkillRank]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {bonus.execute(sk::Operation::CriticalHitBonus,{0x1000,0x3000,0});});
    bonus.callbacks.erase(sk::PlayerGetSkillRank);
    refuses([&] {bonus.execute(sk::Operation::CriticalHitBonus,{0x1000,0x3000,0});});
    sk::Model axes;
    base(axes);
    constexpr unsigned caster=0x7000,rec=0x4d8af0,stack=rec+0x130,skill=0x1000;
    axes.mem.poke(caster+0xc,4,0x40);
    axes.mem.poke(rec+0x43f8,4,stack);
    axes.mem.poke(stack+8,8,UINT64_C(1)<<42);
    for (auto body: {0x319710u,0x31a04cu}) {
      axes.mem.poke(0x200c,4,body);
      require(axes.skill_call(skill,1,{caster})[0]==1,"axe can-use virtual body");
    }
    axes.mem.poke(stack+8,8,0);
    refuses([&] {axes.execute(sk::Operation::FireAxeCanUse,{skill,caster});});
    require(axes.calls.back().slot==sk::TextLookup,"axe missing text prefix");
    axes.callbacks[sk::TextLookup]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {axes.execute(sk::Operation::ColdAxeCanUse,{skill,caster});});
    axes.callbacks[sk::TextLookup]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x553288},"cold axe warning key");
      self.callbacks[sk::TextLookup]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{99};};
      return std::vector<std::uint32_t>{88};
    };
    axes.callbacks[sk::HudShowMessage]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({88,180,0,0}),"axe HUD warning");
      return std::vector<std::uint32_t>{};
    };
    require(axes.execute(sk::Operation::ColdAxeCanUse,{skill,caster})[0]==0,"axe warning result");
    axes.callbacks[sk::TextLookup]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::ColdAxeCanUse,{skill,caster});
    };
    refuses([&] {axes.execute(sk::Operation::ColdAxeCanUse,{skill,caster});});
    axes.mem.poke(stack+8,8,UINT64_C(1)<<42);
    require(axes.execute(sk::Operation::FireAxeCanUse,{skill,caster})[0]==1,"axe recursion recovery");
    axes.mem.poke(rec+0x4434,4,0xa000);
    axes.mem.poke(rec+0xd0,4,11); axes.mem.poke(rec+0xd4,4,27);
    axes.mem.poke(0x3e9ce8,4,0x1111); axes.mem.poke(0x3e9cfc,4,0x2222);
    axes.mem.poke(0x9010,4,ONE); axes.mem.poke(0x9014,4,0x80000000); axes.mem.poke(0x9018,4,0x40000000);
    axes.mem.poke(caster+0x544,4,0xabcd);
    axes.external={10,14};
    bool cold=false;
    axes.callbacks[14]=[&](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({skill,128,cold ? 2u : 20u}),"axe adjustment config");
      return std::vector<std::uint32_t>{5};
    };
    axes.callbacks[10]=[](sk::Model &,const auto &a) {
      require(a[2]==5,"axe adjusted rank"); return std::vector<std::uint32_t>{17};
    };
    axes.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{77};};
    axes.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({77,ONE,0x80000000,0x40000000,ONE}),"axe virtual position read");
      return std::vector<std::uint32_t>{};
    };
    unsigned axe_allocations=0,rows=0,projectiles=0;
    axes.callbacks[sk::PoolNew]=[&](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{axe_allocations++%2 ? 0x1d0u : 0xbcu},"axe allocation order");
      return std::vector<std::uint32_t>{axe_allocations%2 ? 0u : 0xb000u};
    };
    axes.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0xabcd,0,0,0,0,caster+0x114,0,1,ONE}),"axe attach outputs request");
      return std::vector<std::uint32_t>{ONE,0,0,0,0,0};
    };
    axes.callbacks[sk::ItemResourceIndex]=[&](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{stack},"axe resource uses stack");
      return std::vector<std::uint32_t>{rows++%2};
    };
    axes.callbacks[sk::ItemCopyMutableFields]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>({skill+0x68,0xa000}),"axe copy from primary");
      self.mem.poke(skill+0x68+0x24,2,0xffff);
      return std::vector<std::uint32_t>{};
    };
    axes.callbacks[sk::PlayerProjectileConstructor]=[&](sk::Model &,const auto &a) {
      require(a.size()==20 && a[0]==0xb000 && a[1]==0x1111 && a[2]==0x2222 &&
              a[3]==ONE && a[10]==caster && a[13]==11 && a[14]==27 &&
              a[17]==100000 && a[18]==4 && a[19]==skill+0x68,"axe projectile flattened args");
      ++projectiles; return std::vector<std::uint32_t>{};
    };
    axes.callbacks[sk::HudCountActiveNotices]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0,7}),"axe notice arguments");
      return std::vector<std::uint32_t>{3};
    };
    axes.callbacks[sk::InventoryRemoveItem]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0,0,0,0}),"axe inventory removal");
      return std::vector<std::uint32_t>{};
    };
    for (auto body: {0x31979cu,0x31a0d8u}) {
      axes.mem.poke(stack+4,2,1);
      axes.mem.poke(0x2014,4,body);
      require(axes.skill_call(skill,2,{128,caster,0x9010,UINT32_MAX})[0]==0,"axe virtual cast result");
      require(axes.mem.load(skill+0x68+0x24,2)==(cold ? 0x16u : 0x15u) &&
              axes.mem.load(stack+4,2)==0 && axes.mem.load(0x41b8c4,4)==3,"axe modifier/ammunition/HUD");
      cold=true;
    }
    require(rows==4 && projectiles==2 && axes.draws==4,"axe independent queries/shared draws");
    for (auto body: {0x319ea0u,0x31a7d4u}) {
      axes.mem.poke(0x202c,4,body);
      axes.callbacks[15]=[](sk::Model &,const auto &a) {
        require(a==std::vector<std::uint32_t>({skill,5,20}),"axe mana arguments");
        return std::vector<std::uint32_t>{ONE};
      };
      require(axes.skill_call(skill,5,{5})[0]==ONE,"axe mana virtual body");
    }
    axes.callbacks[14]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{5};};
    axes.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{ONE,0,0};};
    refuses([&] {axes.execute(sk::Operation::FireAxeCast,{skill,5,caster,ONE,0x80000000,0x40000000,0});});
    refuses([&] {axes.execute(sk::Operation::AxeCast,{0,skill,5,caster,0,0});});
    axes.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    axes.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{ONE,0,0,0,0,0};};
    axes.callbacks[sk::ItemResourceIndex]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {axes.execute(sk::Operation::FireAxeCast,{skill,5,caster,ONE,0x80000000,0x40000000,0});});
    axes.callbacks[sk::ItemResourceIndex]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0x80000000};};
    bool memory_refused=false;
    try { axes.execute(sk::Operation::FireAxeCast,{skill,5,caster,ONE,0x80000000,0x40000000,0}); }
    catch (const std::runtime_error &) {memory_refused=true;}
    require(memory_refused && axes.calls.back().slot==sk::ItemResourceIndex,"axe out-of-domain row refusal");
    axes.callbacks[sk::ItemResourceIndex]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    axes.callbacks[sk::HudCountActiveNotices]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    axes.mem.poke(stack+4,2,1);
    refuses([&] {axes.execute(sk::Operation::FireAxeCast,{skill,5,caster,ONE,0x80000000,0x40000000,0});});
    require(axes.calls.back().slot==sk::HudCountActiveNotices && axes.mem.load(stack+4,2)==1 &&
            axes.mem.load(0x41b844,4)==240,"axe malformed notice preserves prefix");
    sk::Model cry;
    base(cry);
    cry.mem.poke(0x300c,4,0x40);
    cry.mem.poke(0x3000+0x544,4,77);
    cry.mem.poke(0x3038,2,0xffff);
    cry.mem.poke(0x3010,4,ONE);
    for (const auto body : {0x301464u}) {
      cry.mem.poke(0x200c,4,body);
      refuses([&] {cry.skill_call(0x1000,1,{0x3000});});
      cry.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &a) {
        require(a==std::vector<std::uint32_t>{0x3d08dc},"BattleCry typeinfo");
        return std::vector<std::uint32_t>{7};
      };
      cry.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      require(cry.skill_call(0x1000,1,{0x3000})[0]==1,"BattleCry can-use body");
    }
    cry.external={14};
    cry.callbacks[14]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,128,5}),"BattleCry cast adjustment");
      return std::vector<std::uint32_t>{5};
    };
    cry.callbacks[sk::PoolNew]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x58},"BattleCry spell allocation");
      return std::vector<std::uint32_t>{0};
    };
    cry.mem.poke(0x2014,4,0x301498);
    require(cry.skill_call(0x1000,2,{128,0x3000,UINT32_MAX,UINT32_MAX})[0]==0,"BattleCry unused position");
    cry.mem.poke(0x202c,4,0x3016e0);
    cry.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,20}),"BattleCry mana route");
      return std::vector<std::uint32_t>{ONE};
    };
    require(cry.skill_call(0x1000,5,{5})[0]==ONE,"BattleCry mana body");
    refuses([&] {cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,2,0});});
    refuses([&] {cry.execute(sk::Operation::BattleCryStreamers,{0x6000,2,0});});
    refuses([&] {cry.execute(sk::Operation::BattleCryRun,{0x6000,0,0});});
    refuses([&] {cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,0,0});});
    cry.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{ONE};};
    refuses([&] {cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,0,0});});
    cry.callbacks[sk::AttachPoint]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>({77,ONE,0,0,UINT32_MAX,0x3114,1,0,ONE}),"body attach signature");
      self.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{2,3,4};};
      return std::vector<std::uint32_t>{1,2,3};
    };
    require(cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,0,0})[0]==1 &&
            cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,1,0})[0]==2,"body attach replacement");
    cry.callbacks[sk::AttachPoint]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::BodyAttachPoint,{0x3000,1,0,0});
    };
    refuses([&] {cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,0,0});});
    cry.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{2,3,4};};
    require(cry.execute(sk::Operation::BodyAttachPoint,{0x3000,1,0,0}).size()==3,"body attach depth recovery");
    cry.mem.poke(0x6018,4,0x3000);
    cry.mem.poke(0x6008,4,0x1000);
    cry.mem.poke(0x6030,4,1);
    cry.mem.poke(0x3000+0x58c,4,6);
    cry.mem.poke(0x3000+0x11a,2,0x20);
    cry.callbacks[sk::PlayerAnimationStep]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    refuses([&] {cry.execute(sk::Operation::BattleCryRun,{0x6000,0,0,0,0});});
    require(cry.calls.back().slot==sk::PlayerAnimationStep && cry.mem.load(0x601c,4)!=0,"malformed event prefix");
    cry.mem.poke(0x6030,4,0);
    cry.mem.poke(0x6020,4,0);
    cry.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    require(cry.execute(sk::Operation::BattleCryRun,{0x6000,0,0,0,0}).empty(),"BattleCry failure recovery");
    // Storm virtual casts dereference the supplied xyz pointer; constructors,
    // terrain and object scheduling remain explicit services.
    sk::Model storm;
    base(storm); storm.external={8,12,14,15};
    storm.mem.poke(0x300c,4,0x40); storm.mem.poke(0x4010,4,ONE);
    storm.callbacks[14]=[](sk::Model &,const auto &a) {
      require(a[1]==40 && (a[2]==2 || a[2]==20),"storm rank adjustment");
      return std::vector<std::uint32_t>{40};
    };
    storm.callbacks[8]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{3,7};};
    storm.callbacks[12]=[](sk::Model &,const auto &a) {
      return std::vector<std::uint32_t>{a[1]==0 ? 4u : a[1]==2 ? 3u : 7u};
    };
    storm.callbacks[sk::TerrainHeight]=[](sk::Model &,const auto &a) {
      require(a.size()==5 && a[4]==0x41c00000,"storm terrain radius signature");
      return std::vector<std::uint32_t>{0};
    };
    storm.callbacks[sk::PoolNew]=[](sk::Model &,const auto &a) {
      require(a[0]==0xbc || a[0]==0xc0,"storm allocation sizes");
      return std::vector<std::uint32_t>{0};
    };
    storm.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    storm.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &a) {
      require(a.size()==5 && a[1]==ONE,"storm virtual position pointer transport");
      return std::vector<std::uint32_t>{0};
    };
    for (const auto body : {0x31a920u,0x30aaa8u}) {
      storm.mem.poke(0x2014,4,body);
      require(storm.skill_call(0x1000,2,{40,0x3000,0x4010,0}).at(0)==0,"storm cast body");
    }
    storm.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,40,20}),"storm mana signature");
      return std::vector<std::uint32_t>{ONE};
    };
    for (const auto body : {0x31ae14u,0x30af38u}) {
      storm.mem.poke(0x202c,4,body);
      require(storm.skill_call(0x1000,5,{40})[0]==ONE,"storm mana body");
    }
    storm.callbacks[8]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{3};};
    refuses([&] {storm.execute(sk::Operation::PusStormCast,{0x1000,40,0x3000,0,0,0,0});});
    storm.callbacks[8]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{3,7};};
    storm.callbacks[sk::TerrainHeight]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    refuses([&] {storm.execute(sk::Operation::LightningStrikeCast,{0x1000,40,0x3000,0,0,0,0});});
    refuses([&] {storm.execute(sk::Operation::StormRun,{2,0x6000});});
    refuses([&] {storm.execute(sk::Operation::StormCtor,{2,0x6000,0,0,0,0,0,0,0,0,0,0,0,0});});
    refuses([&] {storm.execute(sk::Operation::PustuleCtor,{0x6000});});
    const std::vector<std::uint32_t> pustule{0x6000,1,2,3,4,5,6,7,8,9,0xffff};
    refuses([&] {storm.execute(sk::Operation::PustuleCtor,pustule);});
    storm.callbacks[sk::GameObjectConstructor]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x6000,1,2,3,0,0}),"pustule callee signature");
      self.mem.poke(0x600c,4,0x80000000);
      self.callbacks[sk::GameObjectConstructor]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{};
    };
    storm.callbacks[sk::ObjectUpdateGridCell]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x6000},"grid service signature");
      return std::vector<std::uint32_t>{};
    };
    storm.callbacks[sk::ObjectRunEveryFrame]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    storm.execute(sk::Operation::PustuleCtor,pustule);
    require(storm.mem.load(0x600c,4)==0x8000420d && storm.mem.load(0x6068,2)==0xffff,"pustule callee mutation");
    storm.callbacks[sk::ObjectRunEveryFrame]=[pustule](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::PustuleCtor,pustule);
    };
    refuses([&] {storm.execute(sk::Operation::PustuleCtor,pustule);});
    storm.callbacks[sk::ObjectRunEveryFrame]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    require(storm.execute(sk::Operation::PustuleCtor,pustule).empty(),"storm recursion recovery");
    storm.mem.poke(0x6058,4,0); storm.mem.poke(0x6064,4,0xffffffff);
    storm.mem.poke(0x6048,4,0x9000); storm.mem.poke(0x9008,2,0xfff0);
    storm.callbacks[sk::ObjectDestroyCallback]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x5ff0,3}),"storm adjusted destruction");
      return std::vector<std::uint32_t>{};
    };
    require(storm.execute(sk::Operation::LightningStrikeRun,{0x6000}).empty(),"storm destruction route");

    // Disease Blade: published routes, callback transport, and failure recovery.
    sk::Model disease;
    base(disease);
    disease.mem.poke(0x200c,4,0x318730);
    refuses([&] { disease.skill_call(0x1000,1,{0x7000}); });
    require(disease.calls.back().slot==sk::TypeInfo,"disease missing type prefix");
    disease.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x3d1e94},"disease type identity");
      return std::vector<std::uint32_t>{123};
    };
    disease.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x7000,123}),"disease availability arguments");
      return std::vector<std::uint32_t>{0};
    };
    require(disease.skill_call(0x1000,1,{0x7000})==std::vector<std::uint32_t>{1},"disease can-use route");
    disease.mem.poke(0x2014,4,0x318764);
    disease.external.insert(14);
    disease.callbacks[14]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,1}),"disease rank adjustment");
      return std::vector<std::uint32_t>{5};
    };
    disease.callbacks[sk::PoolNew]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x24},"disease allocation size");
      return std::vector<std::uint32_t>{0};
    };
    require(disease.skill_call(0x1000,2,{5,0x7000,UINT32_MAX,0})==std::vector<std::uint32_t>{0},"disease cast ignores position pointer");
    disease.mem.poke(0x202c,4,0x318aa0);
    disease.mem.poke(0x207c,4,0x3c88e4);
    disease.external.insert(15);
    disease.callbacks[15]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x1000,5,20}),"disease mana arguments");
      return std::vector<std::uint32_t>{ONE};
    };
    require(disease.skill_call(0x1000,5,{5})==std::vector<std::uint32_t>{ONE},"disease mana route");
    disease.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    refuses([&] {disease.execute(sk::Operation::DiseaseBladeCast,{0x1000,5,0x7000,0,0});});
    refuses([&] {disease.execute(sk::Operation::DiseaseBladeRun,{0x6000});});
    // Holder slot adjustment wraps to raw32 and callee writes survive refusal.
    disease.mem.poke(0x600c,4,0x7000); disease.mem.poke(0x6018,4,0);
    disease.mem.poke(0x700c,4,0x20); disease.mem.poke(0x7050,4,0x9000);
    disease.mem.poke(0x9000,4,0xa000); disease.mem.poke(0xa040,2,0xfff0);
    refuses([&] {disease.execute(sk::Operation::DiseaseBladeRun,{0x6000,0,0,0,0});});
    require(disease.calls.back().slot==sk::HolderClearElement &&
        disease.calls.back().args==std::vector<std::uint32_t>({0x8ff0,1,0}),"disease adjusted holder clear");
    disease.callbacks[sk::HolderClearElement]=[](sk::Model &self,const auto &) {
      self.mem.poke(0x8000,2,0x1234);
      throw sk::Unsupported("authored holder failure");
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {disease.execute(sk::Operation::DiseaseBladeRun,{0x6000,0,0,0,0});});
    require(disease.mem.load(0x8000,2)==0x1234,"disease callee write lost");
    disease.callbacks[sk::HolderClearElement]=[](sk::Model &self,const auto &) {
      self.callbacks[sk::HolderClearElement]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{};
    };
    disease.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    disease.execute(sk::Operation::DiseaseBladeRun,{0x6000,0,0,0,0});
    disease.callbacks[sk::HolderClearElement]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::DiseaseBladeRun,{0x6000,0,0,0,0});
    };
    refuses([&] {disease.execute(sk::Operation::DiseaseBladeRun,{0x6000,0,0,0,0});});
    disease.callbacks[sk::HolderClearElement]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    disease.execute(sk::Operation::DiseaseBladeRun,{0x6000,0,0,0,0});
    // Activation is likewise delegated, never an invented element implementation.
    disease.mem.poke(0xa038,2,0xfff8); disease.external.insert(12);
    disease.callbacks[12]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{15};};
    disease.callbacks[sk::HolderDisease]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x8ff8,1}),"disease adjusted holder activation");
      return std::vector<std::uint32_t>{};
    };
    disease.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    disease.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    disease.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    disease.execute(sk::Operation::DiseaseBladeCtor,{0x6000,5,0x7000,0x1000});
    require(disease.draws==1 && disease.mem.load(0x6014,4)==0x51c4a0 && disease.mem.load(0x709a,2)==0x80,"disease base shared draw");
    // An old value of 31 refuses the aura even if its timer is already zero.
    disease.mem.poke(0x8024,2,0x26); disease.mem.poke(0x8026,2,31);
    disease.mem.poke(0x802a,2,0);
    const auto before_disease=disease.writes.size();
    disease.execute(sk::Operation::WriteDisease,{0x8000,40,31,31});
    require(disease.writes.size()==before_disease,"disease value/frame limit confused");


    // Raise Army exposes all seven bodies. Unknown engine services refuse with
    // their prefix intact; active casts read an explicit xyz pointer.
    sk::Model army; base(army);
    army.mem.poke(0x200c,4,0x3134b0);
    refuses([&] {army.skill_call(0x1000,1,{0x7000});});
    require(army.calls.back().slot==sk::TypeInfo,"raise army lookup refusal");
    army.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x3d16f0},"raise army type identity");
      return std::vector<std::uint32_t>{123};
    };
    army.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x7000,123}),"raise army find args");
      return std::vector<std::uint32_t>{0};
    };
    require(army.skill_call(0x1000,1,{0x7000})==std::vector<std::uint32_t>{1},"raise army can use route");
    army.mem.poke(0x2014,4,0x3134e4); army.mem.poke(0x202c,4,0x3137a4);
    army.mem.poke(0x207c,4,0x3c88e4);
    require(army.skill_call(0x1000,5,{0})==std::vector<std::uint32_t>{0},"raise army mana route");
    army.mem.poke(0x4000,4,0x3f800000); army.mem.poke(0x4004,4,0xc0000000);
    army.mem.poke(0x4008,4,0x40400000);
    army.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{7};};
    army.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({7,0x3f800000,0xc0000000,0x40400000,ONE}),"raise army cast position");
      return std::vector<std::uint32_t>{9};
    };
    army.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    require(army.skill_call(0x1000,2,{5,0x7000,0x4000,0})==std::vector<std::uint32_t>{0},"raise army cast route");
    require(army.mem.load(0x7098,2)==0x80 && army.draws==0,"raise army failed allocation flash");
    army.mem.poke(0x800c,4,0x28); army.mem.poke(0x811a,2,0x80);
    refuses([&] {army.execute(sk::Operation::RaiseArmyIsRaisable,{0x6000,0x8000});});
    army.callbacks[sk::CreatureBossRank]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    require(army.execute(sk::Operation::RaiseArmyIsRaisable,{0x6000,0x8000})[0]==1,"raise army eligibility");
    army.callbacks[sk::CollectObjectsInBox]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    army.execute(sk::Operation::RaiseArmyCtor,{0x6000,5,0x7000,0x1000});
    require(army.draws==1 && army.mem.load(0x6014,4)==0x51bf40,"raise army constructor");
    army.mem.poke(0x602c,4,ONE); army.mem.poke(0x6028,4,0x40000000);
    army.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x6000,3}),"raise army destruction args");
      return std::vector<std::uint32_t>{};
    };
    army.execute(sk::Operation::RaiseArmyRun,{0x6000,0xffffffff,0});
    army.execute(sk::Operation::RaiseArmyRaiseInRing,{0x6000,0});
    // Dress: exact callback transport, a genuine high-half hide mask, callback
    // replacement, malformed results and recursion recovery.
    army.mem.poke(0x6008,4,0x1000); army.mem.poke(0x600c,4,0x7000);
    army.mem.poke(0x7048,4,0xb000); army.mem.poke(0x9048,4,0xb000);
    army.mem.poke(0xb128,2,0xfff8); army.mem.poke(0xb0d8,2,8);
    army.mem.poke(0xb080,2,0xfff0);
    army.mem.poke(0x83e0,4,1);
    for (auto id:{sk::ObjectRunEveryFrame,sk::CreatureSetCharm,sk::AnimationPushWithWeights,sk::CreatureSetState})
      army.callbacks[id]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    army.callbacks[sk::CreatureGetTeam]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x6ff8},"raise army adjusted team");
      return std::vector<std::uint32_t>{17};
    };
    army.callbacks[sk::CreatureSetCharm]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x9000,3600,17}),"raise army charm args");
      return std::vector<std::uint32_t>{};
    };
    army.callbacks[sk::CreatureSetState]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x9008,0}),"raise army adjusted state");
      return std::vector<std::uint32_t>{};
    };
    army.callbacks[sk::StringPrefixDiffers]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    army.callbacks[sk::CreatureModel]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x8ff0},"raise army adjusted model");
      return std::vector<std::uint32_t>{42};
    };
    army.callbacks[sk::ModelBuildHideMask]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    refuses([&] {army.execute(sk::Operation::RaiseArmyDress,{0x6000,0x8000,0x9000});});
    require(army.mem.load(0x9228,4)==ONE && army.mem.load(0x90a8,8)==UINT64_C(0x8002000101400000),"raise army failure prefix");
    army.callbacks[sk::ModelBuildHideMask]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>({42,0}),"raise army mask args");
      self.callbacks[sk::ModelBuildHideMask]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{7,8};};
      return std::vector<std::uint32_t>{0x89abcdef,0xfedcba98};
    };
    army.execute(sk::Operation::RaiseArmyDress,{0x6000,0x8000,0x9000});
    require(army.mem.load(0x9220,8)==UINT64_C(0xfedcba9889abcdef) && army.mem.load(0x90a0,1)==2 &&
        (army.mem.load(0x80a8,8)&0x1000000),"raise army mask/tint/corpse flags");
    army.execute(sk::Operation::RaiseArmyDress,{0x6000,0x8000,0x9000});
    require(army.mem.load(0x9220,8)==UINT64_C(0x800000007),"raise army callback replacement");
    army.callbacks[sk::CreatureBossRank]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::RaiseArmyIsRaisable,{0x6000,0x8000});
    };
    army.mem.poke(0x80a8,8,0);
    refuses([&] {army.execute(sk::Operation::RaiseArmyIsRaisable,{0x6000,0x8000});});
    army.callbacks[sk::CreatureBossRank]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    require(army.execute(sk::Operation::RaiseArmyIsRaisable,{0x6000,0x8000})[0]==1,"raise army recursion recovery");

    // Drowsy requires services, preserves callback failure prefixes, and expands
    // the published cast position pointer before invoking the arc query.
    sk::Model drowsy;
    base(drowsy);
    drowsy.mem.poke(0x2000+2*8+4,4,0x30e1f0);
    drowsy.mem.poke(0x2000+5*8+4,4,0x30e5ec);
    drowsy.mem.poke(0x2000+15*8+4,4,0x3c88e4);
    drowsy.mem.poke(0x2000+2*8,2,0xfff0);
    drowsy.mem.poke(0x1050,4,0x2000); // adjusted Skill object
    drowsy.mem.poke(0x4000,4,0x3f800000); drowsy.mem.poke(0x4004,4,0xc0000000);
    drowsy.mem.poke(0x4008,4,0x40400000);
    refuses([&] {drowsy.skill_call(0x1000,2,{5,0x7000,0x4000,0xffff8001});});
    require(drowsy.calls.back().slot==sk::ResourceFind,"drowsy missing resource prefix");
    drowsy.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x552530,0x552538},"drowsy sound lookup");
      return std::vector<std::uint32_t>{123};
    };
    drowsy.callbacks[sk::SoundPlayOnObject]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{123,0x7000,ONE},"drowsy sound transport");
      return std::vector<std::uint32_t>{};
    };
    drowsy.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    drowsy.callbacks[sk::CollectObjectsInArc]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x3f800000,0xc0000000,0x40400000,0xffff8001,0,0x43700000,0x42f00000,0},"drowsy cast position/facing");
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {drowsy.skill_call(0x1000,2,{5,0x7000,0x4000,0xffff8001});});
    require(drowsy.mem.load(0x7090,2)==0x40 && drowsy.calls.back().slot==sk::HandleClearStack,
            "drowsy stack cleanup refusal retains flash");
    drowsy.callbacks[sk::HandleClearStack]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0},"drowsy explicit stack marker");
      self.callbacks[sk::HandleClearStack]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{};
    };
    require(drowsy.skill_call(0x1000,2,{5,0x7000,0x4000,0xffff8001})[0]==0,"drowsy published cast");
    require(drowsy.skill_call(0x1000,5,{5})[0]==0,"drowsy published mana");
    refuses([&] {drowsy.execute(sk::Operation::DrowsyCast,{0x1000,5,0x7000,0x4000,0});});
    drowsy.mem.poke(0x6008,4,0x1000); drowsy.mem.poke(0x600c,4,0x7000);
    drowsy.mem.poke(0x6020,4,4); drowsy.mem.poke(0x6024,4,2);
    drowsy.mem.poke(0x700c,4,0x40); drowsy.mem.poke(0x402791,1,1);
    drowsy.callbacks[sk::NetIsLocalSlot]=[](sk::Model &self,const auto &) {
      self.mem.store(0x5000,1,7); throw sk::Unsupported("authored network failure");
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {drowsy.execute(sk::Operation::DrowsyRun,{0x6000,0,0});});
    require(drowsy.mem.load(0x5000,1)==7 && drowsy.mem.load(0x6018,4)!=0,"drowsy failure prefix");
    drowsy.callbacks[sk::NetIsLocalSlot]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::DrowsyRun,{0x6000,0,0});
    };
    refuses([&] {drowsy.execute(sk::Operation::DrowsyRun,{0x6000,0,0});});
    drowsy.callbacks[sk::NetIsLocalSlot]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    drowsy.mem.poke(0x6018,4,0);
    drowsy.execute(sk::Operation::DrowsyRun,{0x6000,0,0});
    require(drowsy.mem.load(0x402797,1)==1,"drowsy recursion recovery/rebase");
    drowsy.callbacks[sk::NetIsLocalSlot]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    refuses([&] {drowsy.execute(sk::Operation::DrowsyRun,{0x6000,0,0});});

    // Wave of Flame published bodies, explicit services and retained failure state.
    sk::Model wave;
    base(wave);
    wave.mem.poke(0x2000+8+4,4,0x316b1c);
    wave.mem.poke(0x2000+16+4,4,0x316b24);
    wave.mem.poke(0x2000+40+4,4,0x316cdc);
    wave.mem.poke(0x2000+120+4,4,0x3c88e4);
    wave.mem.poke(0x2000+16,2,0xfff0);
    wave.mem.poke(0x1050,4,0x2000);
    wave.mem.poke(0x2000+96+4,4,0x3c87f0);
    wave.mem.poke(0x2000+88+4,4,0x3c8690);
    wave.mem.poke(0x1018,2,5);
    wave.mem.poke(0x101a,2,100);
    wave.mem.poke(0x1020,2,60);
    wave.mem.poke(0x100e,2,0);
    wave.mem.poke(0x1010,2,0);
    wave.mem.poke(0x1038,2,0xffff);
    wave.mem.poke(0x103a,2,0xffff);
    require(wave.skill_call(0x1000,1,{0x7000})[0]==1,"wave published can-use");
    require(wave.skill_call(0x1000,5,{5})[0]==0x42700000,"wave published mana");
    refuses([&] {wave.skill_call(0x1000,2,{5,0x7000,0xffffffff,0xffff8001});});
    require(wave.calls.back().slot==sk::PoolNew,"wave cast missing allocator");
    wave.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x2c},"wave allocation size");
      return std::vector<std::uint32_t>{0};
    };
    require(wave.skill_call(0x1000,2,{5,0x7000,0xffffffff,0xffff8001})[0]==0,
            "wave allocation failure ignores position pointer");
    wave.mem.poke(0x2000+16,2,0);
    wave.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    wave.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{123};};
    wave.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{456};};
    wave.mem.poke(0x7038,2,0x8001);
    refuses([&] {wave.execute(sk::Operation::WaveOfFlameCtor,{0x6000,5,0x7000,0,0,0,0x1000});});
    require(wave.calls.back().slot==sk::CollectInSphere && wave.draws==1,"wave query refusal prefix");
    wave.callbacks[sk::CollectInSphere]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0,0,0,0xffff8001,0x42f00000},"wave sphere transport");
      self.callbacks[sk::CollectInSphere]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {wave.execute(sk::Operation::WaveOfFlameCtor,{0x6000,5,0x7000,0,0,0,0x1000});});
    require(wave.calls.back().slot==sk::HandleClearStack,"wave stack cleanup required");
    wave.callbacks[sk::HandleClearStack]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0},"wave stack marker");
      return std::vector<std::uint32_t>{};
    };
    wave.execute(sk::Operation::WaveOfFlameCtor,{0x6000,5,0x7000,0,0,0,0x1000});
    // A sound failure follows damage but precedes removal. Retry can hit again,
    // since callback failures have no rollback and no invented cleanup.
    wave.mem.poke(0x6018,4,0xa000);wave.mem.poke(0xa004,4,0x6018);wave.mem.poke(0xa00c,4,0x8000);
    wave.callbacks[sk::DamageApply]=[](sk::Model &self,const auto &args) {
      require(args.size()==43 && args[5]==0x160 && args[24]==0x15,"wave damage descriptor");
      self.mem.store(0x5000,1,7);return std::vector<std::uint32_t>{};
    };
    wave.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {
      throw sk::Unsupported("authored sound failure");return std::vector<std::uint32_t>{};
    };
    refuses([&] {wave.execute(sk::Operation::WaveOfFlameHitTest,{0x6000,0});});
    require(wave.mem.load(0x5000,1)==7 && wave.mem.load(0x6018,4)==0xa000,"wave failure retains hit and list");
    wave.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{456};};
    wave.execute(sk::Operation::WaveOfFlameHitTest,{0x6000,0});
    require(wave.mem.load(0x6018,4)==0,"wave hit removes candidate");
    const auto call_count=wave.calls.size();
    wave.execute(sk::Operation::WaveOfFlameHitTest,{0x6000,0});
    require(wave.calls.size()==call_count,"wave consumed candidate cannot hit twice");
    wave.mem.poke(0x6014,4,0x51c330);wave.mem.poke(0x51c338,2,0xfff8);
    wave.mem.poke(0x6028,4,0x3f800000);
    wave.callbacks[sk::SpellDestroyCallback]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x5ff8,3},"wave destruction this adjustment");
      return self.execute(sk::Operation::WaveOfFlameRun,{0x6000,0,0});
    };
    refuses([&] {wave.execute(sk::Operation::WaveOfFlameRun,{0x6000,0,0});});
    wave.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    wave.execute(sk::Operation::WaveOfFlameRun,{0x6000,0,0});
    refuses([&] {wave.execute(sk::Operation::WaveOfFlameCtor,{0x6000,5,0x7000,0,0,0});});
    wave.mem.poke(0x6028,4,0);
    wave.callbacks[sk::ParticleSpawn]=[](sk::Model &self,const auto &) {
      self.mem.store(0x5001,1,9);throw sk::Unsupported("authored particle failure");
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {wave.execute(sk::Operation::WaveOfFlameRun,{0x6000,0,0});});
    require(wave.mem.load(0x5001,1)==9 && wave.mem.load(0x6028,4)!=0,"wave particle failure prefix");

    // Roar published bodies and service boundaries, independent of vectors.
    sk::Model roar;
    base(roar);
    roar.mem.poke(0x200c,4,0x2fff64);roar.mem.poke(0x2014,4,0x2fff98);
    roar.mem.poke(0x202c,4,0x300138);roar.mem.poke(0x207c,4,0x3c88e4);
    roar.mem.poke(0x208c,4,0x3c8690);roar.mem.poke(0x2044,4,0x3c82e4);
    roar.mem.poke(0x1018,2,5);roar.mem.poke(0x101a,2,100);roar.mem.poke(0x1020,2,60);
    for (unsigned off:{0xe,0x10,0x16}) roar.mem.poke(0x1000+off,2,0);
    for (unsigned off:{0x38,0x3a,0x40}) roar.mem.poke(0x1000+off,2,0xffff);
    refuses([&] {roar.skill_call(0x1000,1,{0x7000});});
    require(roar.calls.back().slot==sk::TypeInfo,"roar can-use requires typeinfo");
    roar.callbacks[sk::TypeInfo]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x3d07b8},"roar typeinfo identity");return std::vector<std::uint32_t>{123};
    };
    roar.callbacks[sk::FindSpellByType]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    require(roar.skill_call(0x1000,1,{0x7000})[0]==1,"roar published can-use");
    require(roar.skill_call(0x1000,5,{5})[0]==0x42700000,"roar published mana");
    roar.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x50},"roar spell allocation size");return std::vector<std::uint32_t>{0};
    };
    require(roar.skill_call(0x1000,2,{5,0x7000,0xffffffff,0xffff8001})[0]==0,
            "roar cast ignores supplied position pointer");
    roar.mem.poke(0x2010,2,0xfff0);roar.mem.poke(0x1050,4,0x2000);
    require(roar.skill_call(0x1000,2,{5,0x7000,0xffffffff,0})[0]==0,"roar signed cast adjustment");
    roar.mem.poke(0x2010,2,0);
    roar.callbacks[sk::StringFormat]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x551828,0},"roar style animation name");return std::vector<std::uint32_t>{999};
    };
    roar.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0xc000};};
    roar.callbacks[sk::AnimationBlend]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x7114,0xc000,0x22,0x3dcccccc,0x3dcccccc},"roar animation transport");
      require(self.mem.load(0xc020,1)==2 && self.mem.load(0xc028,1)==4,"roar event masks");
      require(self.mem.load(0xc01c,4)==0x3ea24db1 && self.mem.load(0xc024,4)==0x3f9e3443,"roar event times");
      return std::vector<std::uint32_t>{};
    };
    roar.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{456};};
    roar.mem.poke(0x73c8,4,0);roar.mem.poke(0x4dd576,1,7);
    roar.execute(sk::Operation::RoarCtor,{0x6000,5,0x7000,0x1000});
    require(roar.draws==1 && roar.mem.load(0x758c,4)==6 && roar.mem.load(0x603c,4)==456,"roar construction state");
    roar.mem.poke(0x711a,2,0x20);
    roar.callbacks[sk::PlayerAnimationStep]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{2};};
    refuses([&] {roar.execute(sk::Operation::RoarRun,{0x6000,0,0});});
    require(roar.calls.back().slot==sk::CameraShake && roar.mem.load(0x6048,4)==1,"roar camera refusal prefix");
    roar.callbacks[sk::CameraShake]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0,0x40c00000},"roar camera strength");
      self.callbacks[sk::CameraShake]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{};
    };
    roar.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {roar.execute(sk::Operation::RoarRun,{0x6000,0,0});});
    roar.callbacks[sk::AttachPoint]=[](sk::Model &,const auto &args) {
      require(args[6]==7,"roar mouth attach index");return std::vector<std::uint32_t>{ONE,0x40000000,0x40400000};
    };
    roar.callbacks[sk::TerrainHeight]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0,ONE,0x40000000,0x40400000,0x40c00000},"roar terrain transport");
      return std::vector<std::uint32_t>{0x40800000};
    };
    refuses([&] {roar.execute(sk::Operation::RoarRun,{0x6000,0,0});});
    require(roar.calls.back().slot==sk::WaterTest && roar.mem.load(0x6038,4)==0x40800000,"roar water refusal after ground");
    roar.callbacks[sk::WaterTest]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0,ONE,0x40000000,0x40800000,0,0x41000000},"roar water uses ground z");
      self.mem.store(0x5000,1,9);throw sk::Unsupported("authored water failure");return std::vector<std::uint32_t>{};
    };
    refuses([&] {roar.execute(sk::Operation::RoarRun,{0x6000,0,0});});
    require(roar.mem.load(0x5000,1)==9 && roar.mem.load(0x602c,4)==0,"roar callee mutation retained before clock");
    roar.callbacks[sk::WaterTest]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    roar.callbacks[sk::CollectInSphere]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    roar.callbacks[sk::HandleClearStack]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0},"roar stack marker");return std::vector<std::uint32_t>{};
    };
    roar.callbacks[sk::ParticleSpawn]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    refuses([&] {roar.execute(sk::Operation::RoarRun,{0x6000,0,0});});
    require(roar.calls.back().slot==sk::WaterDisturb && roar.mem.load(0x602c,4)==0,"roar water-disturb refusal before tick");
    roar.callbacks[sk::WaterDisturb]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0,ONE,0x40000000,0x40800000,0x455ac000,0x42100000},"roar ripple transport");
      return std::vector<std::uint32_t>{};
    };
    roar.execute(sk::Operation::RoarRun,{0x6000,0,0});
    require(roar.mem.load(0x602c,4)==0x3c888888,"roar clock increments last");
    // Damage failure retains candidate; successful consumption prevents repeats.
    roar.mem.poke(0x6020,4,0);roar.mem.poke(0x6024,4,0);roar.mem.poke(0x601c,4,0xa000);
    roar.mem.poke(0xa004,4,0x601c);roar.mem.poke(0xa00c,4,0x8000);
    roar.callbacks[sk::DamageApply]=[](sk::Model &self,const auto &args) {
      require(args.size()==10 && args[5]==0x100940 && args[6]==0,"roar descriptor-free damage transport");
      self.mem.store(0x5001,1,7);throw sk::Unsupported("authored damage failure");return std::vector<std::uint32_t>{};
    };
    refuses([&] {roar.execute(sk::Operation::RoarHitRing,{0x6000,0});});
    require(roar.mem.load(0x601c,4)==0xa000 && roar.mem.load(0x5001,1)==7,"roar damage failure retains candidate");
    roar.callbacks[sk::DamageApply]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    roar.execute(sk::Operation::RoarHitRing,{0x6000,0});
    const auto roar_calls=roar.calls.size();roar.execute(sk::Operation::RoarHitRing,{0x6000,0});
    require(roar.mem.load(0x601c,4)==0 && roar.calls.size()==roar_calls,"roar one-time ring hit");
    roar.mem.poke(0x6040,4,0);roar.mem.poke(0x6030,4,0xbf800000);roar.mem.poke(0x6034,4,0);
    roar.mem.poke(0x51b2d8,2,0xfff8);
    roar.callbacks[sk::SpellDestroyCallback]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x5ff8,3},"roar destroy signed adjustment");
      return self.execute(sk::Operation::RoarRun,{0x6000,0,0});
    };
    refuses([&] {roar.execute(sk::Operation::RoarRun,{0x6000,0,0});});
    roar.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    roar.execute(sk::Operation::RoarRun,{0x6000,0,0});
    refuses([&] {roar.execute(sk::Operation::RoarBreath,{0x6000,0,0,0,0,0,0});});

    // Unspoken Word: published dispatch, new trail services and surviving-hit rolls.
    sk::Model word;
    base(word);
    word.mem.poke(0x2014,4,0x2fd030);word.mem.poke(0x202c,4,0x2fd334);
    word.mem.poke(0x207c,4,0x3c88e4);word.mem.poke(0x208c,4,0x3c8690);
    word.mem.poke(0x2044,4,0x3c82e4);
    for (unsigned k=0;k<5;++k) {
      word.mem.poke(0x100e + 2*k,2,0);word.mem.poke(0x1038+2*k,2,0xffff);
      word.mem.poke(0x1018+2*k,2,k==1 ? 0 : k==2 ? 100 : k==3 ? 3 : 20);
    }
    require(word.skill_call(0x1000,5,{5})[0]==0x41a00000,"Unspoken Word published mana");
    word.mem.poke(0x4010,4,ONE);word.mem.poke(0x4014,4,0x40000000);word.mem.poke(0x4018,4,0x40400000);
    refuses([&] {word.skill_call(0x1000,2,{5,0x7000,0x4010,0});});
    require(word.calls.back().slot==sk::ResourceFind && word.mem.load(0x7090,2)==0x40,
            "Unspoken Word refusal preserves both flashes");
    word.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x5513b0,0x5513b8},"Unspoken Word resource identity");
      return std::vector<std::uint32_t>{123};
    };
    word.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{123,ONE,0x40000000,0x40400000,ONE},"Unspoken Word virtual position read");
      return std::vector<std::uint32_t>{};
    };
    word.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    require(word.skill_call(0x1000,2,{5,0x7000,0x4010,0})[0]==0,"Unspoken Word allocation failure");
    word.mem.poke(0x2010,2,0xfff0);word.mem.poke(0x1050,4,0x2000);
    require(word.skill_call(0x1000,2,{5,0x7000,0x4010,0})[0]==0,"Unspoken Word adjusted virtual cast");
    word.mem.poke(0x2010,2,0);
    word.callbacks[sk::PoolNew]=[](sk::Model &self,const auto &args) {
      require(args[0]==0xbc,"Unspoken Word first light allocation");
      self.callbacks[sk::PoolNew]=[](sk::Model &,const auto &next) {
        require(next[0]==0x38,"Unspoken Word spell allocation follows light");return std::vector<std::uint32_t>{0};
      };
      return std::vector<std::uint32_t>{0x9000};
    };
    word.callbacks[sk::LightEffectConstructor]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x9000,ONE,0x40000000,0x421c0000,0x7000,0,
        0x42c80000,0x42c80000,0x437f0000,0xc2c00000,0,0,0,0,0x42f00000,0x43340000,0,ONE},"Unspoken Word light transport");
      return std::vector<std::uint32_t>{};
    };
    require(word.skill_call(0x1000,2,{5,0x7000,0x4010,0})[0]==0,"Unspoken Word self-replacing allocator");
    word.mem.poke(0x44fdcc,4,123);
    refuses([&] {word.execute(sk::Operation::UnspokenWordCtor,{0x6000,255,0x7000,0x1000});});
    require(word.calls.back().slot==sk::TrailCreate && word.draws==1 && word.mem.load(0x602c,4)==123,
            "Unspoken Word missing trail preserves base state");
    word.callbacks[sk::TrailCreate]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
    refuses([&] {word.execute(sk::Operation::UnspokenWordCtor,{0x6000,5,0x7000,0x1000});});
    word.callbacks[sk::TrailCreate]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{1,500,5,0x40,0x40,0x80,0x80,0x40000000},"Unspoken Word first trail transport");
      self.callbacks[sk::TrailCreate]=[](sk::Model &,const auto &next) {
        require(next==std::vector<std::uint32_t>{1,300,5,0x40,0x80,0x40,0x80,0x40000000},"Unspoken Word second trail transport");
        return std::vector<std::uint32_t>{0xd100};
      };return std::vector<std::uint32_t>{0xd000};
    };
    word.callbacks[sk::CollectInSphere]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    word.callbacks[sk::HandleClearStack]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0},"Unspoken Word stack cleanup callback");return std::vector<std::uint32_t>{};
    };
    word.execute(sk::Operation::UnspokenWordCtor,{0x6000,5,0x7000,0x1000});
    require(word.mem.load(0x6030,4)==0xd000 && word.mem.load(0x6034,4)==0xd100,"Unspoken Word retained trail handles");
    // Exactly age 15 has points but no fade/particles. A callback can replace itself.
    word.mem.poke(0x44fdcc,4,138);
    word.callbacks[sk::TrailSetPoints]=[](sk::Model &self,const auto &args) {
      require(args.size()==35 && args[0]==0xd000 && args[31]==10 && args[32]==0x78 && args[34]==ONE,
              "Unspoken Word flattened ten-point trail");
      self.callbacks[sk::TrailSetPoints]=[](sk::Model &,const auto &next) {
        require(next.size()==35 && next[0]==0xd100,"Unspoken Word second trail callback replacement");
        return std::vector<std::uint32_t>{};
      };return std::vector<std::uint32_t>{};
    };
    word.execute(sk::Operation::UnspokenWordRun,{0x6000,0,0});
    word.mem.poke(0x44fdcc,4,139);
    refuses([&] {word.execute(sk::Operation::UnspokenWordRun,{0x6000,0,0});});
    require(word.calls.back().slot==sk::TrailSetLength,"Unspoken Word fade starts after half");
    word.callbacks[sk::TrailSetLength]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    word.callbacks[sk::TrailSetColor]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    word.callbacks[sk::TrailSetPoints]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    const auto before_seed=word.mem.load(0x6010,4);
    word.execute(sk::Operation::UnspokenWordRun,{0x6000,0,0});
    require(word.mem.load(0x6010,4)==before_seed,"Unspoken Word zero particle count consumes no random draws");
    // Damage precedes survival test. Failed stun keeps the candidate and private draw.
    word.mem.poke(0x601c,4,0xa000);word.mem.poke(0xa004,4,0x601c);word.mem.poke(0xa00c,4,0x8000);
    word.mem.poke(0x800c,4,0x20);word.mem.poke(0x8048,4,0x9000);word.mem.poke(0x90f0,2,0xfff0);
    word.callbacks[sk::CreatureEnemy]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    word.callbacks[sk::DamageApply]=[](sk::Model &self,const auto &args) {
      require(args.size()==10 && args[5]==0x40940 && args[6]==0,"Unspoken Word damage without descriptor");
      self.mem.poke(0x8228,4,ONE);return std::vector<std::uint32_t>{};
    };
    refuses([&] {word.execute(sk::Operation::UnspokenWordHitRing,{0x6000,0});});
    require(word.calls.back().slot==sk::CreatureStun && word.mem.load(0x601c,4)==0xa000 && word.mem.load(0x6010,4)!=before_seed,
            "Unspoken Word failed stun retains candidate and roll");
    word.callbacks[sk::CreatureStun]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x7ff0,180,1},"Unspoken Word signed stun adjustment");
      self.mem.store(0x5001,1,7);throw sk::Unsupported("authored stun failure");return std::vector<std::uint32_t>{};
    };
    refuses([&] {word.execute(sk::Operation::UnspokenWordHitRing,{0x6000,0});});
    require(word.mem.load(0x5001,1)==7 && word.mem.load(0x601c,4)==0xa000,"Unspoken Word failure mutation retained");
    word.callbacks[sk::CreatureStun]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    word.execute(sk::Operation::UnspokenWordHitRing,{0x6000,0});
    const auto after_hits=word.calls.size();word.execute(sk::Operation::UnspokenWordHitRing,{0x6000,0});
    require(word.calls.size()==after_hits && word.mem.load(0x601c,4)==0,"Unspoken Word one-time hit");
    word.mem.poke(0x44fdcc,4,153);word.mem.poke(0x51af18,2,0xfff8);
    word.callbacks[sk::SpellDestroyCallback]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x5ff8,3},"Unspoken Word expiry destruction adjustment");
      return self.execute(sk::Operation::UnspokenWordRun,{0x6000,0,0});
    };
    refuses([&] {word.execute(sk::Operation::UnspokenWordRun,{0x6000,0,0});});
    word.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    word.execute(sk::Operation::UnspokenWordRun,{0x6000,0,0});
    refuses([&] {word.execute(sk::Operation::UnspokenWordPhase,{0x6000});});

    std::cout
        << "skills-c API refusals, callback lifetime and recovery passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
