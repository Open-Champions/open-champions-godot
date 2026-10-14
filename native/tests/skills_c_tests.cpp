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
    std::cout
        << "skills-c API refusals, callback lifetime and recovery passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
