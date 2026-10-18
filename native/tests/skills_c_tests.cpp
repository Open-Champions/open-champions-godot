#include "champions/skills_c.hpp"
#include <iostream>
#include <array>
#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
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
    {
      sk::Model spell;unsigned shared_reads=0;
      spell.mem=sk::Memory({},[&](auto at,auto width) -> std::uint64_t {
        if(at==0x404464 && width==4) {
          ++shared_reads;require(spell.draws==0 && spell.writes.size()==5 && spell.mem.log().size()==5,"C draw prefix");
          require(spell.mem.load(0x8014,4)==0x515ba0,"C temporary table before draw");
          spell.mem.poke(0x8010,4,0xffffffff);return 0;
        }
        return 0;
      });
      spell.mem.poke(0x8000,4,0xa1b2c3d4);
      spell.execute(sk::Operation::SpellBase,{0x8000,0x1234,7,9,0xdeadbeef});
      require(shared_reads==1 && spell.draws==1 && spell.writes.size()==8,"C single draw/tail history");
      require(spell.mem.load(0x8000,4)==0xa1b2c334 && spell.mem.load(0x8010,4)==0x3c6e,"C byte neighbors and upper16 seed");
      const std::array<unsigned,8> addresses={0x8000,0x8008,0x800c,0x8014,0x8004,0x404464,0x8010,0x8014};
      for(unsigned i=0;i<addresses.size();++i) require(spell.writes[i].address==addresses[i],"C store order");
      require(spell.writes[3].value==0x515ba0 && spell.writes[7].value==0xdeadbeef,"C temporary/final tables");
      require(spell.execute(sk::Operation::SpellRand,{0x8000})==std::vector<std::uint32_t>{0x3b43} && spell.mem.load(0x8010,4)==0x3b43d8f5,"C private full state");
      require(spell.draws==1 && spell.calls.empty(),"C private draw ownership");
    }
    // Same admitted alias as B: C rereads the head after the node write.
    {
      sk::Model handles;
      handles.mem.poke(0x864ea8,4,0x4000);handles.mem.poke(0x4000,4,0x4100);
      handles.mem.poke(0x4100,4,0);handles.mem.poke(0x4010,4,0xa1b2ffff);
      require(handles.execute(sk::Operation::HandleLink,{0x4001,0x6000,7})==
              std::vector<std::uint32_t>{0x4000},"alias link return");
      require(handles.mem.load(0x45,4)==0,"C reread skips cached backlink");
      require(handles.mem.load(0x4010,4)==0xa1b20000,"16-bit tag preserves neighbor");
      require(handles.calls.empty() && handles.writes.size()==handles.mem.log().size(),
              "C handle write/service records");
    }

    {
      unsigned reads=0;
      sk::Model bounded;
      bounded.mem=sk::Memory({},[&](auto,auto) -> std::uint64_t { ++reads; return 0; });
      refuses([&]{ bounded.skill_call(UINT64_C(0x100000000),10,{}); });
      refuses([&]{ bounded.skill_call(0x1000,65536,{}); });
      require(reads==0,"C validates address and host slot before table reads");
    }
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

    // Poison Bolt: public virtual bodies, external construction and collision,
    // scalar refusal, reentrant touch dispatch, failure prefixes and recovery.
    sk::Model bolt;
    base(bolt);
    bolt.mem.poke(0x2014,4,0x30f168);
    bolt.mem.poke(0x202c,4,0x30f364);
    bolt.mem.poke(0x207c,4,0x3c88e4);
    bolt.mem.poke(0x1016,2,0);bolt.mem.poke(0x1020,2,20);
    require(bolt.skill_call(0x1000,5,{5})[0]==0x41a00000,"Poison Bolt mana body");
    bolt.mem.poke(0x4010,4,ONE);bolt.mem.poke(0x4014,4,0x40000000);bolt.mem.poke(0x4018,4,0x40400000);
    refuses([&] {bolt.skill_call(0x1000,2,{5,0x7000,0x4010,0xffff8001});});
    require(bolt.writes.size()==8 && bolt.calls.back().slot==sk::PoolNew,"Poison Bolt flash before allocator refusal");
    bolt.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0xb8},"Poison Bolt allocation size");return std::vector<std::uint32_t>{0};
    };
    require(bolt.skill_call(0x1000,2,{5,0x7000,0x4010,0xffff8001})[0]==0,"Poison Bolt failed cast returns zero");
    bolt.mem.poke(0x2010,2,0xfff8); // negative Skill this adjustment
    bolt.mem.poke(0x1058,4,0x2000);
    require(bolt.skill_call(0x1000,2,{5,0x7000,0x4010,0})[0]==0,"Poison Bolt adjusted virtual cast");
    bolt.mem.poke(0x2010,2,0);
    bolt.callbacks[sk::PoolNew]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0x6000};};
    refuses([&] {bolt.skill_call(0x1000,2,{5,0x7000,0x4010,0xffff8001});});
    require(bolt.calls.back().slot==sk::ProjectileConstructor && bolt.draws==0,"delegated Projectile constructor retains no invented draw");
    bolt.callbacks[sk::ProjectileConstructor]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x6000,ONE,0x40000000,0x40400000,0,0},"Projectile constructor signature");
      self.mem.poke(0x6010,4,ONE);self.mem.poke(0x6014,4,0x40000000);self.mem.poke(0x6018,4,0x40400000);
      self.callbacks[sk::ProjectileConstructor]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      return std::vector<std::uint32_t>{};
    };
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltCtor,{0x6000,ONE,0x40000000,0x40400000,0,5,0,0x1000});});
    require(bolt.calls.back().slot==sk::LightLink && bolt.mem.load(0x6048,4)==0x51bb78,"missing light retains constructor prefix");
    for (auto id:{sk::LightLink,sk::ObjectRunEveryFrame,sk::SoundPlayOnObject,sk::ParticleSpawn,sk::LightRelink})
      bolt.callbacks[id]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    bolt.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{123};};
    bolt.execute(sk::Operation::PoisonBoltCtor,{0x6000,ONE,0x40000000,0x40400000,0,5,0,0x1000});
    require(bolt.mem.load(0x60ac,4)==113 && bolt.mem.load(0x60b0,4)==11 && bolt.draws==0,"Poison Bolt constructor clocks and external seed");
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltRun,{0x6000});});
    bolt.callbacks[sk::ObjectMoveAndCollide]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltRun,{0x6000});});
    bolt.callbacks[sk::ObjectMoveAndCollide]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x6000,0x43af0000,0x38af0000,0},"Poison Bolt collision velocity");
      self.mem.poke(0x6010,4,0x40000000);
      return std::vector<std::uint32_t>{0};
    };
    bolt.execute(sk::Operation::PoisonBoltRun,{0x6000});
    require(bolt.mem.load(0x6084,4)==0x40000000 && bolt.mem.load(0x6044,4)!=0,"Poison Bolt trail updates private stream and light position");
    bolt.mem.poke(0x60b4,4,0);bolt.mem.poke(0x800c,4,0x20);
    bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});
    require(bolt.mem.load(0x60b4,4)==0,"ownerless Poison Bolt passes creature");
    bolt.mem.poke(0x800c,4,0);
    bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});
    require(bolt.mem.load(0x60b4,4)==1,"ownerless Poison Bolt stops on prop");
    bolt.mem.poke(0x60b4,4,0);bolt.mem.poke(0x6090,4,0xa000);bolt.mem.poke(0xa00c,4,0x7000);
    bolt.mem.poke(0x7048,4,0xd000);bolt.mem.poke(0xd0a0,2,0xfff0);
    bolt.callbacks[sk::CreatureEnemy]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x6ff0,0x8000},"Poison Bolt adjusted enemy call");
      self.mem.poke(0x6090,4,0);return std::vector<std::uint32_t>{1};
    };
    bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});
    require(bolt.mem.load(0x60b4,4)==0,"Poison Bolt reloads removed owner after callback");
    bolt.mem.poke(0x6090,4,0xa000);bolt.mem.poke(0x800c,4,0x20);
    bolt.callbacks[sk::CreatureEnemy]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    bolt.mem.poke(0x8000+0xa8,8,0x200);
    bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});
    require(bolt.mem.load(0x60b4,4)==0,"Poison Bolt blocking creature gate");
    bolt.mem.poke(0x8000+0xa8,8,0);bolt.mem.poke(0x609c,2,16);
    bolt.external.insert(8);bolt.callbacks[8]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});});
    bolt.callbacks[8]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x1000,0,16,0,0x3e4ccccc},"Poison Bolt tier-one range signature");
      return std::vector<std::uint32_t>{10,20};
    };
    bolt.callbacks[sk::DamageApply]=[](sk::Model &self,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x8000,0x7000,100000,0,0,0x90040,0,0,0x41200000,0x41a00000},"Poison Bolt damage signature");
      self.mem.poke(0x609c,2,5);return std::vector<std::uint32_t>{};
    };
    bolt.external.insert(10);bolt.callbacks[10]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x1000,3,5},"Poison Bolt timer reloads rank after damage");
      return std::vector<std::uint32_t>{3};
    };
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});});
    require(bolt.mem.load(0x60b4,4)==0 && bolt.mem.load(0x609c,2)==5,"timer refusal retains damage before hit flag");
    bolt.mem.poke(0x609c,2,16);
    bolt.callbacks[sk::CreatureSetEffectTimer]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x8000,3,0x7000,0x40400000},"Poison Bolt effect timer transport");return std::vector<std::uint32_t>{};
    };
    bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});
    const auto after_hit=bolt.calls.size();bolt.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});
    require(bolt.calls.size()==after_hit,"Poison Bolt ignores repeated touch");
    bolt.mem.poke(0x604c,4,0xa020);bolt.mem.poke(0xa02c,4,0x9000);
    bolt.callbacks[10]=[](sk::Model &,const auto &args) {return std::vector<std::uint32_t>{args[1]==3 ? 3u : args[1]==1 ? 2u : 100u};};
    bolt.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x70},"Poison Bolt gas allocation");return std::vector<std::uint32_t>{0xc000};
    };
    bolt.callbacks[sk::PoisonGasConstructor]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0xc000,0x40000000,0x40000000,0x40400000,180,24,0,0x9000,100},"Poison Bolt gas uses inherited owner handle");return std::vector<std::uint32_t>{};
    };
    bolt.mem.poke(0x552658,8,0x4008000000000000ull);bolt.mem.poke(0x552660,8,0x4031000000000000ull);
    bolt.execute(sk::Operation::PoisonBoltBurst,{0x6000});
    bolt.mem.poke(0x51bb80,2,0xfff8);
    bolt.callbacks[sk::ObjectMoveAndCollide]=[](sk::Model &self,const auto &) {
      self.mem.poke(0x60b4,4,0);self.mem.poke(0x6090,4,0);self.mem.poke(0x800c,4,0);
      self.execute(sk::Operation::PoisonBoltOnHit,{0x6000,0x8000});return std::vector<std::uint32_t>{2};
    };
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltRun,{0x6000});});
    require(bolt.calls.back().slot==sk::ObjectDestroyCallback && bolt.mem.load(0x60ac,4)==0,"reentrant collision touch triggers expiry and destroy refusal");
    bolt.callbacks[sk::ObjectDestroyCallback]=[](sk::Model &,const auto &args) {
      require(args==std::vector<std::uint32_t>{0x5ff8,3},"Poison Bolt signed destroy adjustment");
      return std::vector<std::uint32_t>{};
    };
    bolt.callbacks[sk::ObjectMoveAndCollide]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::PoisonBoltRun,{0x6000});
    };
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltRun,{0x6000});});
    bolt.callbacks[sk::ObjectMoveAndCollide]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
    bolt.execute(sk::Operation::PoisonBoltRun,{0x6000});
    refuses([&] {bolt.execute(sk::Operation::PoisonBoltCtor,{0x6000});});
    sk::Model null_bolt;
    unsigned null_particles=0;
    null_bolt.callbacks[sk::ObjectMoveAndCollide]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    null_bolt.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
    null_bolt.callbacks[sk::SoundPlayOnObject]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    null_bolt.callbacks[sk::ParticleSpawn]=[&](sk::Model &,const auto &) {++null_particles;return std::vector<std::uint32_t>{};};
    null_bolt.execute(sk::Operation::PoisonBoltRun,{0});
    require(null_particles==321 && null_bolt.draws==0,"null Poison Bolt bursts but skips destroy callback");

    {
    // Summon Badger: allocation sequence, published dispatch and callback shapes.
    sk::Model badger;
    base(badger);
    const auto constant=[](std::uint32_t value) {
      return [value](sk::Model &,const auto &) {return std::vector<std::uint32_t>{value};};
    };
    const auto nothing=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
    badger.mem.poke(0x700c,4,0x40);badger.mem.poke(0x73c8,4,3);
    badger.mem.poke(0x503904,4,0x1234);
    badger.mem.poke(0x2014,4,0x3106f4);badger.mem.poke(0x200a,2,0xfff8);
    badger.mem.poke(0x200c,4,0x310690);
    refuses([&] {badger.execute(sk::Operation::SummonBadgerCanUse,{0x1000,0x7000});});
    require(badger.calls.back().slot==sk::WorldFindObjectByName,"badger missing name service");
    badger.callbacks[sk::WorldFindObjectByName]=[](sk::Model &self,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x1234},"badger player name");
      self.callbacks[sk::WorldFindObjectByName]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
      return std::vector<std::uint32_t>{0};
    };
    require(badger.skill_call(0x1000,1,{0x7000})==std::vector<std::uint32_t>{1},"badger published can-use");
    require(badger.execute(sk::Operation::SummonBadgerCanUse,{0x1000,0x7000})[0]==0,"badger callback replacement");
    badger.mem.poke(0x73c8,4,4);const auto nc=badger.calls.size();
    require(badger.execute(sk::Operation::SummonBadgerCanUse,{0x1000,0x7000})[0]==0 && badger.calls.size()==nc,"badger invalid slot short circuit");
    badger.mem.poke(0x73c8,4,3);badger.mem.poke(0x7018,4,0);
    badger.mem.poke(0x44fdc8,4,0x8888);
    badger.mem.poke(0x5000,4,ONE);badger.mem.poke(0x5004,4,ONE);badger.mem.poke(0x5008,4,ONE);
    badger.callbacks[sk::TerrainHeight]=[](sk::Model &,const auto &a) {
      require(a.size()==5 && a[0]==0x8888 && a[4]==0x42400000,"badger terrain signature");
      return std::vector<std::uint32_t>{0x41c00001};
    };
    badger.callbacks[sk::WorldFindFreeSpot]=constant(1);
    refuses([&] {badger.skill_call(0x1000,2,{5,0x7000,0x5000,0xffff8001});});
    require(badger.calls.back().slot==sk::WorldFindFreeSpot,"badger malformed free spot");
    badger.callbacks[sk::WorldFindFreeSpot]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0,0,0,71,24,0}),"badger fallback signature");
      return std::vector<std::uint32_t>{ONE,ONE,ONE,128};
    };
    badger.external.insert(12);badger.callbacks[12]=constant(7);
    badger.mem.poke(0x7048,4,0xd000);badger.mem.poke(0xd128,2,0xfff0);
    badger.mem.poke(0x8048,4,0xe000);badger.mem.poke(0xe110,2,0xfffc);
    badger.mem.poke(0x80a8,8,UINT64_C(0x8002000000000000));
    badger.mem.poke(0x864ea8,4,0xb000);badger.mem.poke(0xb000,4,0xb020);
    unsigned allocations=0;
    badger.callbacks[sk::PoolNew]=[&](sk::Model &,const auto &a) {
      const std::array<std::uint32_t,3> sizes{0x3d0,0xbc,0x20},blocks{0x8000,0,0x6000};
      require(allocations<3 && a==std::vector<std::uint32_t>{sizes[allocations]},"badger allocation order");
      return std::vector<std::uint32_t>{blocks[allocations++]};
    };
    badger.callbacks[sk::CastBadgerConstructor]=[](sk::Model &,const auto &a) {
      require(a.size()==8 && a[0]==0x8000 && a[1]==0x5527a8 && a[2]==ONE && a[5]==0xffff8001 && a[6]==0x5038e8 && a[7]==7,"badger delegated ctor and replacement spot");
      return std::vector<std::uint32_t>{};
    };
    badger.callbacks[sk::CreatureGetTeam]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>{0x6ff0},"badger team signed adjustment");return std::vector<std::uint32_t>{9};
    };
    badger.callbacks[sk::CreatureSetCharm]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x8000,0xffffffff,9}),"badger charm signature");return std::vector<std::uint32_t>{};
    };
    badger.callbacks[sk::CreatureSetHomeRange]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x7ffc,500}),"badger home signed adjustment");return std::vector<std::uint32_t>{};
    };
    badger.callbacks[sk::ResourceFind]=constant(0x9999);badger.callbacks[sk::SoundPlayAtPosition]=nothing;
    require(badger.skill_call(0x1000,2,{5,0x7000,0x5000,0xffff8001})==std::vector<std::uint32_t>{0x6000},"badger published cast expands position");
    require(allocations==3 && badger.draws==1 && badger.mem.load(0x6000,1)==1 && badger.mem.load(0x601c,4)==0,"badger spell draw/level/clock");
    require(badger.mem.load(0x80a8,8)==UINT64_C(0x8002000001400000) && badger.mem.load(0x805a,1)==3 && badger.mem.load(0x83ac,2)==400,"badger complete flags/player stores");
    allocations=0;
    badger.callbacks[sk::PoolNew]=[&](sk::Model &,const auto &) {
      const std::array<std::uint32_t,3> blocks{0x8000,0x9000,0};
      return std::vector<std::uint32_t>{blocks[allocations++]};
    };
    badger.callbacks[sk::LightEffectConstructor]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x9000,ONE,ONE,0x42140000,0x7000,0,
          0x42a80000,0x43400000,0x424c0000,0x42820000,0,0,0,0,0x42f00000,0x43340000,0,ONE}),"badger light signature");
      return std::vector<std::uint32_t>{};
    };
    require(badger.skill_call(0x1000,2,{5,0x7000,0x5000,0xffff8001})[0]==0 &&
            allocations==3 && badger.draws==1,"badger final allocation failure retains effects without spell draw");
    // Empty handle destroys; dead node is inert. Failure retains clock/visibility.
    badger.mem.poke(0x51bd20,2,0xfff8);
    badger.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &a) {
      require(a==std::vector<std::uint32_t>({0x5ff8,3}),"badger spell destroy adjustment");return std::vector<std::uint32_t>{};
    };
    badger.mem.poke(0x6018,4,0);badger.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});
    badger.mem.poke(0x6018,4,0xb000);badger.mem.poke(0xb00c,4,0);const auto deadcalls=badger.calls.size();
    badger.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});
    require(badger.calls.size()==deadcalls && badger.mem.load(0x601c,4)==0,"badger dead node inert");
    badger.mem.poke(0xb00c,4,0x8000);badger.mem.poke(0x601c,4,ONE);
    refuses([&] {badger.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});});
    require(badger.mem.load(0x601c,4)>ONE && !(badger.mem.load(0x800c,4)&2),"badger failure retains clock and visible flag");
    badger.callbacks[sk::CreatureModel]=constant(0x3333);badger.callbacks[sk::SamplePointCount]=constant(17);
    unsigned samples=0,particles=0;
    badger.callbacks[sk::SamplePoint]=[&](sk::Model &,const auto &a) {
      require(a.size()==10 && a[6]==(samples*8)%17,"badger fixed sample stride");++samples;
      return std::vector<std::uint32_t>{ONE,ONE,ONE};
    };
    badger.callbacks[sk::ParticleSpawn]=[&](sk::Model &,const auto &a) {
      require(a.size()==8 && a[0]==0x8e8468,"badger particle signature");++particles;return std::vector<std::uint32_t>{};
    };
    badger.mem.poke(0x601c,4,ONE);badger.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});
    require(samples==38 && particles==38 && badger.draws==1,"badger private stream and 38 fixed samples");
    badger.mem.poke(0x601c,4,0x3f000000);
    badger.callbacks[sk::CreatureModel]=[](sk::Model &self,const auto &) {
      return self.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});
    };
    refuses([&] {badger.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});});
    badger.callbacks[sk::CreatureModel]=constant(0x3333);samples=0;particles=0;badger.mem.poke(0x601c,4,ONE);
    badger.execute(sk::Operation::SummonBadgerRun,{0x6000,0,0});
    require(particles==38,"badger recursion depth recovers");
    refuses([&] {badger.execute(sk::Operation::SummonBadgerCtor,{0x6000});});

    }
    {
      sk::Model root; base(root);
      root.mem.poke(0x2000+8+4,4,0x2e6534);
      root.mem.poke(0x2000+8,2,0xfff8);
      root.mem.poke(0x700c,4,0x40); root.mem.poke(0x73c8,4,3);
      require(root.skill_call(0x1000,1,{0x7000})[0]==1,"root PvP disabled");
      root.mem.poke(0x4f711c,4,1);
      refuses([&] {root.skill_call(0x1000,1,{0x7000});});
      require(root.calls.back().slot==sk::TextLookup,"root missing text prefix");
      root.callbacks[sk::TextLookup]=[](sk::Model &self,const auto &args) {
        require(args==std::vector<std::uint32_t>{0x550178},"root warning text");
        self.callbacks[sk::TextLookup]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{9};};
        return std::vector<std::uint32_t>{8};
      };
      root.callbacks[sk::HudShowMessage]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({8,90,3,0}),"root warning signature");return std::vector<std::uint32_t>{};
      };
      require(root.skill_call(0x1000,1,{0x7000})[0]==0,"root warning refusal");
      root.callbacks[sk::TextLookup]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      refuses([&] {root.execute(sk::Operation::RootCanUse,{0x1000,0x7000});});
      root.mem.poke(0x2000+16+4,4,0x2e6598);root.mem.poke(0x2000+16,2,0xfff8);
      root.external.insert(14);
      root.callbacks[14]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0xff8,1,20}),"root signed cast this");return std::vector<std::uint32_t>{1};
      };
      root.external.insert(10);root.callbacks[10]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
      root.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x550188,0x550190}),"root sound resource");return std::vector<std::uint32_t>{7};
      };
      root.callbacks[sk::SoundPlayOnObject]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({7,0,ONE}),"root sound signature");return std::vector<std::uint32_t>{};
      };
      root.mem.poke(0x9010,4,1);root.mem.poke(0x9014,4,2);root.mem.poke(0x9018,4,3);
      root.callbacks[sk::CollectObjectsInArc]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({1,2,3,0xffff8001,0,0x43780000,0x42f00000,0}),"root arc position expansion");
        return std::vector<std::uint32_t>{0x7000,0x8000};
      };
      root.callbacks[sk::CreatureBossRank]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      root.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>{0x34},"root allocation size");return std::vector<std::uint32_t>{0};
      };
      unsigned added=0,cleared=0;
      root.callbacks[sk::CreatureAddSpell]=[&](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x7000,0}),"root null allocation attachment");++added;return std::vector<std::uint32_t>{};
      };
      root.callbacks[sk::HandleClearStack]=[&](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>{0},"root stack clear sentinel");++cleared;return std::vector<std::uint32_t>{};
      };
      require(root.skill_call(0x1000,2,{1,0,0x9010,0x8001})[0]==0 && added==1 && cleared==2 && root.draws==0,"root failed allocation counts target");
      root.callbacks[sk::ParticleGroupCreate]=[](sk::Model &self,const auto &) {
        self.mem.poke(0x7010,4,0x40400000);return std::vector<std::uint32_t>{0x1234};
      };
      root.execute(sk::Operation::RootCtor,{0x6000,255,0x7000,0x1000});
      require(root.draws==1 && root.mem.load(0x6028,4)==0x40400000 && root.mem.load(0x6020,4)==1,"root constructor draw and callback position reload");
      root.mem.poke(0x519b78,2,0xfff8);
      root.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x5ff8,3}),"root destroy adjustment");return std::vector<std::uint32_t>{};
      };
      root.mem.poke(0x601c,4,1);root.mem.poke(0x7048,4,0xd000);root.mem.poke(0xd080,2,0xfff0);
      root.callbacks[sk::CreatureModel]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>{0x6ff0},"root model adjustment");return std::vector<std::uint32_t>{7};
      };
      root.callbacks[sk::SamplePointCount]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      const auto shared=root.draws,seed=root.mem.load(0x6010,4);
      refuses([&] {root.execute(sk::Operation::RootRun,{0x6000,0,0});});
      require(root.draws==shared && root.mem.load(0x6010,4)!=seed && root.mem.load(0x6018,4)!=0,"root zero sample failure retains clock and own draw");
      root.callbacks[sk::SamplePointCount]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
      unsigned particles=0;
      root.callbacks[sk::SamplePoint]=[](sk::Model &,const auto &args) {
        require(args.size()==10 && args[1]==0x7114 && args[6]==0 && args[7]==1 && args[8]==0 && args[9]==ONE,"root hold sample signature");
        return std::vector<std::uint32_t>{1,2,3};
      };
      root.callbacks[sk::ParticleSpawn]=[&](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x8e5eb0,1,2,3,0,0,0,0}),"root hold particle signature");++particles;return std::vector<std::uint32_t>{};
      };
      root.mem.poke(0x7010,4,0x40400000);root.mem.poke(0x7014,4,0x40800000);
      root.mem.poke(0x6028,4,0);root.mem.poke(0x602c,4,0);
      root.execute(sk::Operation::RootRun,{0x6000,0,0});
      require(particles==22 && root.draws==shared && root.mem.load(0x72e8,4)==0x41a00000,"root hold particles and player pullback");
      root.callbacks[sk::SamplePoint]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
      refuses([&] {root.execute(sk::Operation::RootRun,{0x6000,0,0});});
      root.callbacks[sk::CreatureModel]=[](sk::Model &self,const auto &) {return self.execute(sk::Operation::RootRun,{0x6000,0,0});};
      refuses([&] {root.execute(sk::Operation::RootRun,{0x6000,0,0});});
      root.mem.poke(0x601c,4,2);root.mem.poke(0x6020,4,5);
      root.execute(sk::Operation::RootRun,{0x6000,0,0});
      root.mem.poke(0x2000+5*8+4,4,0x2e6894);
      root.external.insert(15);
      root.callbacks[15]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x1000,7,20}),"root mana body");return std::vector<std::uint32_t>{ONE};
      };
      require(root.skill_call(0x1000,5,{7})[0]==ONE,"root published mana");
      root.mem.poke(0x601c,4,0);root.mem.poke(0x6018,4,0x3f04dcca);
      root.mem.poke(0x6020,4,5);root.mem.poke(0x700c,4,0x20);
      root.mem.poke(0x701c,4,0x42000000);root.mem.poke(0x7020,4,0x42800000);
      root.callbacks[sk::ParticleGroupBegin]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      root.callbacks[sk::ParticleGroupSetMatrix]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x1234,0}),"root pending stack matrix");return std::vector<std::uint32_t>{};
      };
      root.callbacks[sk::ParticleGroupEnd]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      root.callbacks[sk::ParticleSpawn]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      const auto before=root.draws;
      root.callbacks[10]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{101};};
      root.execute(sk::Operation::RootRun,{0x6000,0,0});
      require(root.draws==before+1 && root.mem.load(0x601c,4)==1 && root.mem.load(0x7314,2)==300,"root success shared draw and raw-word timer");
      root.mem.poke(0x601c,4,0);root.mem.poke(0x6018,4,0x3f04dcca);
      root.callbacks[10]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      unsigned bursts=0,destroys=0;
      root.callbacks[sk::ParticleSpawn]=[&](sk::Model &,const auto &args) {
        if (args[0]==0x8e5f50) ++bursts;return std::vector<std::uint32_t>{};
      };
      root.callbacks[sk::SpellDestroyCallback]=[&](sk::Model &,const auto &) {++destroys;return std::vector<std::uint32_t>{};};
      root.execute(sk::Operation::RootRun,{0x6000,0,0});
      require(root.draws==before+2 && bursts==100 && destroys==1 && root.mem.load(0x601c,4)==0,"root failure bursts then destroys once");
      refuses([&] {root.execute(sk::Operation::RootRun,{0x6000});});
    }
    {
      sk::Model cone; base(cone); cone.external={12,14};
      cone.mem.poke(0x6008,4,0x1000); cone.mem.poke(0x600c,4,0x7000);
      cone.mem.poke(0x6000,1,8);cone.mem.poke(0x7048,4,0xd000);cone.mem.poke(0xd0a0,2,0xfff0);
      cone.callbacks[12]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{40};};
      cone.callbacks[14]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x1000,8,1}),"cone adjusts with one");return std::vector<std::uint32_t>{8};
      };
      cone.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x58}),"cone cast allocation size");return std::vector<std::uint32_t>{0};
      };
      require(cone.execute(sk::Operation::ConeOfShockCast,{0x1000,8,0x7000,1,2,3,4})[0]==0 && cone.draws==0,"cone failed allocation consumes no draw");
      cone.mem.poke(0x2014,4,0x31b8f0);
      require(cone.skill_call(0x1000,2,{8,0x7000,0x9000,0})[0]==0,"cone published cast body");
      cone.mem.poke(0x202c,4,0x31ba94);cone.external.insert(15);
      cone.callbacks[15]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x1000,8,20}),"cone mana parameters");return std::vector<std::uint32_t>{ONE};
      };
      require(cone.skill_call(0x1000,5,{8})[0]==ONE,"cone published mana body");
      cone.callbacks[sk::LightLink]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x6018}),"cone embedded light");return std::vector<std::uint32_t>{};
      };
      cone.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{456};};
      cone.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{789};};
      cone.execute(sk::Operation::ConeOfShockCtor,{0x6000,8,0x7000,0x1000});
      require(cone.draws==1 && cone.mem.load(0x6054,4)==789 && cone.mem.load(0x6050,4)==0x42700000,"cone constructor seed light sound");
      cone.callbacks[sk::PoolNew]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0x60}),"cone bolt allocation size");return std::vector<std::uint32_t>{0xa000};
      };
      unsigned bolts=0,hits=0,clears=0;
      cone.callbacks[sk::LightningBoltConstructor2]=[&](sk::Model &self,const auto &args) {
        require(args.size()==9 && args[0]==0xa000 && args[7]==11 && args[8]==0,"cone bolt callback signature");
        ++bolts;self.callbacks[sk::LightningBoltConstructor2]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
        return std::vector<std::uint32_t>{};
      };
      cone.callbacks[sk::CollectObjectsInArc]=[](sk::Model &,const auto &args) {
        require(args.size()==8 && args[3]==0xffffffff && args[4]==0 && args[5]==0x43f00000 && args[6]==0x42480000 && args[7]==0,"cone arc signature");
        return std::vector<std::uint32_t>{0x8000,0x8100};
      };
      cone.callbacks[sk::CreatureEnemy]=[](sk::Model &,const auto &args) {
        require(args[0]==0x6ff0,"cone enemy signed this adjustment");return std::vector<std::uint32_t>{args[1]==0x8000 ? 1u:0u};
      };
      cone.callbacks[sk::DamageApply]=[&](sk::Model &,const auto &args) {
        require(args.size()==43 && args[0]==0x8000 && args[1]==0x7000 && args[5]==0x40 && args[24]==0x18 && args[25]==10 && args[26]>=10 && args[26]<=11 && args[40]==0 && args[41]==0 && args[42]==0,"cone lightning descriptor");
        ++hits;return std::vector<std::uint32_t>{};
      };
      cone.callbacks[sk::HandleClearStack]=[&](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>({0}),"cone stack sentinel");++clears;return std::vector<std::uint32_t>{};
      };
      cone.callbacks[sk::LightRelink]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      const auto shared=cone.draws;cone.mem.poke(0x6010,4,1);
      cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0xffff});
      require(bolts==1 && hits==1 && clears==1 && cone.draws==shared,"cone independent shared stream and callback order");
      // A negative rank produces a zero period denominator: refuse after stats.
      cone.mem.poke(0x6000,1,252);const auto seed=cone.mem.load(0x6010,4);
      refuses([&] {cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});});
      require(cone.mem.load(0x6010,4)==seed,"cone divide failure before own draw");
      cone.mem.poke(0x6000,1,132);cone.mem.poke(0x44fdcc,4,0x80000000);
      const auto private_before=cone.mem.load(0x6010,4);
      cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});
      require(cone.mem.load(0x6010,4)!=private_before,"cone INT_MIN remainder with negative one");
      cone.mem.poke(0x6000,1,8);cone.mem.poke(0x44fdcc,4,1);
      cone.callbacks.erase(sk::LightRelink);
      refuses([&] {cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});});
      require(cone.mem.load(0x6034,4)!=0,"cone missing relink retains flicker writes");
      cone.callbacks[sk::LightRelink]=[](sk::Model &self,const auto &) {
        self.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});return std::vector<std::uint32_t>{};
      };
      refuses([&] {cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});});
      cone.callbacks[sk::LightRelink]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});
      cone.callbacks[12]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1,2};};
      refuses([&] {cone.execute(sk::Operation::ConeOfShockRun,{0x6000,0,0,0,0});});
      cone.callbacks[sk::SoundPlayAtPosition]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      refuses([&] {cone.execute(sk::Operation::ConeOfShockCtor,{0x6000,8,0x7000,0x1000});});
      refuses([&] {cone.execute(sk::Operation::ConeOfShockRun,{0x6000});});
    }
    {
      sk::Model handles;
      handles.mem.poke(0x864ea8,4,0x4100);
      refuses([&] { handles.execute(sk::Operation::HandleLinkStack,{0,0x3000,UINT32_MAX}); });
      require(handles.calls.back().slot==sk::HandleLinkStackCallback && handles.writes.empty(),
              "stack link missing callback logs before refusing without pool writes");
      handles.callbacks[sk::HandleLinkStackCallback]=[](sk::Model &self,const auto &args) {
        require(args==std::vector<std::uint32_t>{0,0x3000,UINT32_MAX},"stack link signature");
        self.mem.poke(0x864ea8,4,0x4200);
        self.callbacks[sk::HandleLinkStackCallback]=[](sk::Model &,const auto &) {
          return std::vector<std::uint32_t>{UINT32_MAX};
        };
        return std::vector<std::uint32_t>{0x4100};
      };
      require(handles.execute(sk::Operation::HandleLinkStack,{0,0x3000,UINT32_MAX})==
              std::vector<std::uint32_t>{0x4100},"stack link callback lifetime");
      require(handles.execute(sk::Operation::HandleLinkStack,{0,0x3000,UINT32_MAX})==
              std::vector<std::uint32_t>{UINT32_MAX},"stack link callback replacement");
      require(handles.mem.load(0x864ea8,4)==0x4200 && handles.writes.empty() && !handles.draws,
              "stack link delegates all allocation and retains callback mutation");
      handles.callbacks[sk::HandleLinkStackCallback]=[](sk::Model &self,const auto &) -> std::vector<std::uint32_t> {
        self.mem.poke(0x3000,4,UINT32_MAX);
        throw std::runtime_error("authored link failure");
      };
      bool propagated=false;
      try { handles.execute(sk::Operation::HandleLinkStack,{0,0x3000,UINT32_MAX}); }
      catch (const std::runtime_error &e) { propagated=std::string(e.what())=="authored link failure"; }
      require(propagated && handles.mem.load(0x3000,4)==UINT32_MAX,
              "stack link exception and prefix propagate without rollback");
      refuses([&] { handles.execute(sk::Operation::HandleClearStack,{0}); });
      handles.callbacks[sk::HandleClearStack]=[](sk::Model &self,const auto &args) -> std::vector<std::uint32_t> {
        require(args==std::vector<std::uint32_t>{0},"stack clear signature");
        self.mem.poke(0x864ea8,4,0);
        throw sk::Unsupported("authored clear failure");
      };
      refuses([&] { handles.execute(sk::Operation::HandleClearStack,{0}); });
      require(handles.mem.load(0x864ea8,4)==0,"stack clear failure preserves mutation");
      handles.callbacks[sk::HandleClearStack]=[](sk::Model &self,const auto &) {
        return self.execute(sk::Operation::HandleClearStack,{0});
      };
      refuses([&] { handles.execute(sk::Operation::HandleClearStack,{0}); });
      handles.callbacks[sk::HandleClearStack]=[](sk::Model &,const auto &) {
        return std::vector<std::uint32_t>{1,2,3};
      };
      require(handles.execute(sk::Operation::HandleClearStack,{0})==std::vector<std::uint32_t>{1,2,3},
              "stack clear forwards delegated result and recovers recursion depth");
      const auto calls=handles.calls.size();
      refuses([&] { handles.execute(sk::Operation::HandleClearStack,{1}); });
      refuses([&] { handles.execute(sk::Operation::HandleLinkStack,{0,1}); });
      require(handles.calls.size()==calls,"malformed stack selection has no side effects");
      handles.execute(sk::Operation::HandleLink,{0,0,0});
      require(!handles.writes.empty(),"address zero is still memory-backed");
    }
    // Complete general helper family, including structured direct API results.
    {
      using V=sk::ResultValue;
      require(sk::c_div(-17,3)==-5 && sk::c_rem(-17,3)==-2,"signed truncation");
      require(sk::c_div(17,-3)==-5 && sk::c_rem(17,-3)==2,"dividend remainder sign");
      require(sk::c_div(INT32_MIN,-1)==2147483648LL && sk::c_rem(INT32_MIN,-1)==0,
              "division widens before INT_MIN overflow");
      refuses([] { sk::c_div(7,0); });
      refuses([] { sk::c_rem(7,0); });
      const V zero{V::Scalar,0,{}}, seven{V::Scalar,7,{}}, nine{V::Scalar,9,{}};
      const V wide{V::Scalar,UINT64_MAX,{}};
      require(sk::shape_result("",'i',{},wide).scalar==UINT64_MAX,
              "raw64 integer callback return preserved");
      require(sk::shape_result("W",'n',{wide},zero,zero,{wide}).scalar==UINT64_MAX,
              "raw64 nullable output and truth preserved");
      const auto optional=sk::shape_result("OW",'n',{zero,seven},zero,zero,{seven,nine});
      require(optional.kind==V::Scalar && optional.scalar==9,"suppressed output is consumed");
      const auto defaults=sk::shape_result("oOnLbwW",'n',{seven,seven});
      require(defaults.kind==V::Tuple && defaults.values.size()==7 &&
              defaults.values[0].kind==V::Tuple && defaults.values[0].values.size()==3 &&
              defaults.values[2].kind==V::List && defaults.values[2].values.empty(),
              "output-only fields do not advance argument position");
      const auto floating=sk::shape_result("",'f',{},seven,nine);
      require(floating.kind==V::Scalar && floating.scalar==9,"float return selection");
      const std::map<std::string,sk::ExternalResultSignature> registry{{"authored",{"",'f'}}};
      require(sk::shape_result(registry,"authored",{},seven,nine).scalar==9,
              "named EXTERNALS row lookup");
      refuses([&] { sk::shape_result(registry,"unknown",{}); });
      require(sk::shape_result("",'n',{}).kind==V::None,"void return");
      const auto buffer=sk::shape_result("o",'s',{},zero,zero,{seven,nine});
      require(buffer.kind==V::Buffer && buffer.values[0].scalar==9,"buffer follows consumed outputs");
      const auto empty=sk::shape_result("",'s',{});
      require(empty.kind==V::Buffer && empty.values[0].kind==V::Bytes &&
              empty.values[0].values.empty(),"empty byte buffer default");
      refuses([&] { sk::shape_result("O",'n',{}); });
      const auto damage=sk::desc({{}, {UINT32_MAX,0x10000,0x80000001,7,8}});
      require(damage.header[0]==0 && damage.header[16]==255 && damage.header[17]==255 &&
              damage.slots.size()==4 && damage.slots[0].empty() &&
              damage.slots[1]==std::vector<std::uint16_t>{65535,0,1,7,8} &&
              damage.slots[2]==std::vector<std::uint16_t>{65535},"descriptor masks and empty slots");
      require(sk::desc(std::vector<std::vector<std::uint32_t>>(5,{1})).slots.size()==5,
              "descriptor retains more than four supplied slots");
      sk::Model helpers;
      for (const auto &bad:std::vector<std::vector<std::uint32_t>>{{},{1},{1,2,0},{0,7}})
        refuses([&] {helpers.execute(sk::Operation::Descriptor,bad);});
      for (const auto &bad:std::vector<std::vector<std::uint32_t>>{
           {},{110},{110,0,1,99},{110,0,0,1},{110,0,0,1,0,1,0,0,7}})
        refuses([&] {helpers.execute(sk::Operation::ShapeResult,bad);});
      require(helpers.calls.empty() && helpers.writes.empty() && !helpers.draws,
              "pure helpers have no world, memory or RNG effects");
      for (const auto &bad:std::vector<std::vector<std::uint32_t>>{
           {110,0,1,5,0,1,0,1,0,0}, {110,0,1,4,1,1,256,1,0,1,0,0}})
        refuses([&] { helpers.execute(sk::Operation::ShapeResult,bad); });
      std::vector<std::uint32_t> deep{110,0,1};
      for (unsigned k=0;k<64;++k) deep.insert(deep.end(),{3,1});
      deep.insert(deep.end(),{1,0,1,0,1,0,0});
      refuses([&] { helpers.execute(sk::Operation::ShapeResult,deep); });
      require(helpers.execute(sk::Operation::ShapeResult,{110,0,0,1,0,1,0,0})==
              std::vector<std::uint32_t>{0},"structured refusal releases operation depth");
      require(helpers.execute(sk::Operation::SignedDivide,{0x80000000,UINT32_MAX})==
              std::vector<std::uint32_t>{0x80000000,0},"division transport preserves widened result");
    }
    // Base dispatch plumbing: named calls preserve Python values and mask only
    // top-level integers. Inherited bodies never become an unknown virtual call.
    {
      using V=sk::ResultValue;
      sk::Model dispatch;
      const V wide{V::Scalar,UINT64_MAX,{}}, zero{V::Scalar,0,{}};
      dispatch.named_callbacks["authored"]=[&](sk::Model &self,const auto &args) {
        require(args.size()==2 && args[0].scalar==UINT32_MAX &&
                args[1].values[0].scalar==UINT64_MAX,"named call masks only top-level integers");
        self.named_callbacks["authored"]=[](sk::Model &,const auto &) {return V{V::Bytes,0,{}};};
        self.mem.poke(0x3000,4,7);
        return wide;
      };
      require(dispatch.call("authored",{wide,{V::Tuple,0,{wide}}}).scalar==UINT64_MAX,
              "named callback forwards wide result and keeps active callable alive");
      require(dispatch.call("authored",{}).kind==V::Bytes,"named callback replacement");
      require(dispatch.named_calls.size()==2 && dispatch.named_calls[0].order==0 &&
              dispatch.named_calls[0].args[0].scalar==UINT32_MAX && dispatch.writes.empty(),
              "named calls log canonical values without invented model writes");
      dispatch.signatures["optional"]={"OW",'n'};
      const auto defaults=dispatch.call("optional",{zero,wide});
      require(defaults.kind==V::Scalar && defaults.scalar==0,"named default optional suppression");
      dispatch.signatures["buffer"]={"",'s'};
      require(dispatch.call("buffer",{}).kind==V::Buffer,"named default byte buffer");
      dispatch.signatures["void"]={"",'n'};
      require(dispatch.call("void",{}).kind==V::None,"named default void");
      const auto count=dispatch.named_calls.size();
      refuses([&] { dispatch.call("unknown",{}); });
      require(dispatch.named_calls.size()==count+1,"unknown metadata refuses after logging");
      dispatch.named_callbacks["failure"]=[](sk::Model &self,const auto &) -> V {
        self.mem.store(0x3000,4,9);throw std::runtime_error("named callback failure");
      };
      bool propagated=false;
      try {dispatch.call("failure",{});} catch (const std::runtime_error &error) {
        propagated=std::string(error.what())=="named callback failure";
      }
      require(propagated && dispatch.mem.load(0x3000,4)==9,"named failure retains mutation");
      dispatch.named_callbacks["recursive"]=[](sk::Model &self,const auto &) {return self.call("recursive",{});};
      refuses([&] {dispatch.call("recursive",{});});
      dispatch.named_callbacks["recursive"]=[](sk::Model &,const auto &) {return V{};};
      require(dispatch.call("recursive",{}).kind==V::None,"named recursion guard recovers");
      dispatch.mem.poke(0x6014,4,0x2000);dispatch.mem.poke(0x2010,2,0xffff);
      dispatch.mem.poke(0x2014,4,0x310cd0);dispatch.mem.poke(0x6017,4,1);
      // Use a separate offset to avoid overlapping the tiny synthetic spell.
      dispatch.mem.poke(0x6100,4,0x2000);
      dispatch.mem.poke(0x6017,4,0x4100);
      dispatch.table_call("Spell",0x6000,0x100,2,{0,0});
      require(dispatch.named_calls.back().name=="recursive","known spell runs without virtual callback log");
      dispatch.named_external.insert("Spell.v2");
      dispatch.named_callbacks["Spell.v2"]=[](sk::Model &,const auto &args) {
        require(args.size()==3 && args[0].scalar==0x5fff && args[1].kind==V::Tuple &&
                args[1].values.size()==3 && args[2].scalar==UINT32_MAX,"forced spell adjusted-this and canonical position");
        return V{V::Tuple,0,{{V::Scalar,7,{}},{V::Scalar,9,{}}}};
      };
      require(dispatch.table_call("Spell",0x6000,0x100,2,{0,UINT32_MAX})==
              std::vector<std::uint32_t>{7,9},"forced spell callback result");
      dispatch.named_external.clear();
      dispatch.mem.poke(0x2014,4,0x2e177c);
      dispatch.mem.poke(0x5fff+0x18,4,1);
      require(dispatch.table_call("Spell",0x6000,0x100,2,{0,0}).empty(),"inherited Skeleton run dispatch");
      dispatch.mem.poke(0x2014,4,0x310cd0);
      dispatch.named_callbacks["Other.v3"]=[](sk::Model &,const auto &args) {return args[0];};
      dispatch.mem.poke(0x2018,2,0x8000);
      require(dispatch.table_call("Other",1,0x100,3,{}).size()==1,"non-Spell classes always delegate");
      dispatch.named_callbacks["Holder.v2"]=[](sk::Model &,const auto &args) {return args[0];};
      dispatch.mem.poke(0xfffffff0,4,0x2000);dispatch.mem.poke(0x2010,2,32767);
      require(dispatch.table_call("Holder",0xfffffff0,0,2,{})[0]==0x7fef,"table adjusted this wraps raw32");
      const auto before=dispatch.named_calls.size();
      refuses([&] {dispatch.table_call("Spell",UINT64_MAX,0,2,{});});
      refuses([&] {dispatch.table_call("Spell",0,0,65536,{});});
      require(dispatch.named_calls.size()==before,"invalid table domain has no callback effect");
      dispatch.mem.poke(0x1060,4,0x2000);
      dispatch.named_callbacks["Skill.v16"]=[](sk::Model &,const auto &args) {return args[0];};
      dispatch.mem.poke(0x2080,2,0xffff);dispatch.mem.poke(0x2084,4,0);
      require(dispatch.execute(sk::Operation::SkillCall,{0x1000,16})[0]==0xfff,
              "general SkillCall adapter supports slots beyond legacy wrapper");
      dispatch.mem.poke(0x2084,4,0x3c8218);dispatch.named_external.insert("Skill.v16");
      require(dispatch.execute(sk::Operation::SkillCall,{0x1000,16,0})[0]==0xfff,
              "named external forces known Skill body at general slot");
      dispatch.named_external.clear();
      require(dispatch.execute(sk::Operation::SkillCall,{0x1000,16,0})[0]==1,
              "clearing named external restores known Skill body dispatch");
      for (const auto &bad:std::vector<std::vector<std::uint32_t>>{{},{0},{1,200},{1,65,1}})
        refuses([&] {dispatch.execute(sk::Operation::NamedCall,bad);});
      for (const auto &bad:std::vector<std::uint32_t>{0,1})
        refuses([&] {dispatch.execute(sk::Operation::TableCall,{bad});});
      refuses([&] {dispatch.execute(sk::Operation::SkillCall,{1});});
      require(dispatch.call("void",{}).kind==V::None,"dispatch transport refusal restores depth");
    }
    {
      sk::Model skeleton;
      skeleton.mem.poke(0x1060,4,0x2000);skeleton.mem.poke(0x200c,4,0x2e2368);
      skeleton.mem.poke(0x503788,4,0xabc);
      skeleton.callbacks[sk::WorldFindObjectByName]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>{0xabc},"Skeleton uses caller supplied monster name");
        return std::vector<std::uint32_t>{0};
      };
      require(skeleton.skill_call(0x1000,1,{0x7000})[0]==1,"inherited Skeleton can-use dispatch");
      skeleton.mem.poke(0x700c,4,0x40);skeleton.mem.poke(0x73c8,4,4);
      const auto before=skeleton.calls.size();
      require(skeleton.execute(sk::Operation::SkeletonCanUse,{0x1000,0x7000})[0]==0 && skeleton.calls.size()==before,
              "invalid player slot refuses before name lookup");
      skeleton.execute(sk::Operation::SkeletonCtor,{0x8000,256,0x7000,0x1000,0});
      require(skeleton.draws==1 && skeleton.mem.load(0x8000,1)==0 && skeleton.mem.load(0x8014,4)==0x519918,
              "Skeleton arena base level byte and single shared seed draw");
      skeleton.mem.poke(0x519920,2,0xffff);
      skeleton.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &args) {
        require(args==std::vector<std::uint32_t>{0x7fff,3},"Skeleton destroy signed this adjustment");return std::vector<std::uint32_t>{};
      };
      skeleton.execute(sk::Operation::SkeletonRun,{0x8000,0,0});
      skeleton.mem.poke(0x8018,4,0x777);const auto calls=skeleton.calls.size();
      skeleton.execute(sk::Operation::SkeletonRun,{0x8000,0,0});
      require(skeleton.calls.size()==calls,"live Skeleton handle keeps spell alive");
      skeleton.mem.poke(0x519918+16+4,4,0x2e177c);
      require(skeleton.table_call("Spell",0x8000,0x14,2,{}).empty(),"inherited spell run uses Python default position and facing");
      skeleton.mem.poke(0x8018,4,0);skeleton.mem.poke(0x519924,4,0x2e177c);
      skeleton.mem.poke(0x519920,2,0);
      const auto before_recursive=skeleton.calls.size();
      refuses([&] {skeleton.execute(sk::Operation::SkeletonRun,{0x8000,0,0});});
      require(skeleton.calls.size()==before_recursive,"modelled destructor-slot body recurses locally without fabricated callback");
      skeleton.external.insert(sk::SpellDestroyCallback);
      skeleton.callbacks[sk::SpellDestroyCallback]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      skeleton.execute(sk::Operation::SkeletonRun,{0x8000,0,0});
      require(skeleton.calls.size()==before_recursive+1,"forced numeric Spell.v1 delegates selected inherited body");
      const auto draws=skeleton.draws;
      skeleton.execute(sk::Operation::CosmeticBall,{0});
      require(skeleton.draws==draws && skeleton.mem.load(0x404460,4)!=0,"cosmetic stream leaves shared draws untouched");
      refuses([&] {skeleton.execute(sk::Operation::CosmeticDraw,{1});});
      sk::Model ctor;
      ctor.mem.poke(0x4f714c,4,0x12345678);ctor.mem.poke(0x4f85c4,1,255);
      ctor.mem.poke(0x6000+0x450,4,0xdeadbeef);
      ctor.callbacks[sk::GameObjectConstructor]=[](sk::Model &self,const auto &a) {
        require(a.size()==6 && std::vector<std::uint32_t>(a.begin(),a.begin()+5)==std::vector<std::uint32_t>{0x6000,1,2,3,0x502728} && (a[5]==0 || a[5]==0x503770),"Skelight delegates exact GameObject construction");
        self.mem.poke(0x6024,4,0xa000);return std::vector<std::uint32_t>{};
      };
      for (auto id:{sk::ObjectPropertyInt,sk::ObjectPropertyString}) ctor.callbacks[id]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      for (auto id:{sk::HolderSetModel,sk::AnimationClear,sk::AnimationPush,sk::ObjectRunEveryFrame,sk::CreatureInit})
        ctor.callbacks[id]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      ctor.callbacks[sk::ModelBuildHideMask]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0x89abcdef,0xfedcba98};};
      ctor.callbacks[sk::CreatureSetStats]=[](sk::Model &self,const auto &a) {
        require(self.mem.load(0x4f714c,4)==0xbf800000 && a.back()==0,"temporary signed level and equal empty level names");
        return std::vector<std::uint32_t>{};
      };
      ctor.execute(sk::Operation::SkelightCtor,{0x6000,1,2,3,0xffff,0x503770,UINT32_MAX});
      require(ctor.mem.load(0x4f714c,4)==0x12345678 && ctor.mem.load(0x4f85c4,1)==0 && ctor.mem.load(0x6230,1)==255,
              "Skelight restores temporary level and wraps serial byte");
      require(ctor.mem.load(0x6220,8)==0xfedcba9889abcdefull && ctor.mem.load(0x6450,4)==0xdeadbeef &&
              ctor.mem.load(0x6232,2)==23 && (ctor.mem.load(0x60a8,8)>>63)==1,
              "Skelight retains raw64 hide mask, stale memory, rise and active state");
      ctor.callbacks[sk::CreatureSetStats]=[](sk::Model &,const auto &) -> std::vector<std::uint32_t> {throw std::runtime_error("authored failure");};
      bool propagated=false;
      try {ctor.execute(sk::Operation::SkelightCtor,{0x6000,1,2,3,0,0,5});}
      catch (const std::runtime_error &error) {propagated=std::string(error.what())=="authored failure";}
      require(propagated,"statistics callback exception propagates unchanged");
      require(ctor.mem.load(0x4f714c,4)==0x40a00000,"failed statistics preserves temporary override prefix");
      ctor.callbacks[sk::CreatureSetStats]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      ctor.execute(sk::Operation::SkelightCtor,{0x6000,1,2,3,0,0,5});
      require(ctor.mem.load(0x6232,2)==23,"constructor depth recovers after callback exception");
      sk::Model cast;
      cast.callbacks=ctor.callbacks;
      cast.mem.poke(0x1060,4,0x2000);cast.mem.poke(0x700c,4,0x40);
      cast.mem.poke(0x7048,4,0x9000);cast.mem.poke(0x9000+37*8+4,4,0x3c2730);
      cast.mem.poke(0x7310,1,255);cast.mem.poke(0x5174b0+34*8+4,4,0x3c2708);
      cast.external.insert(12);
      cast.callbacks[12]=[](sk::Model &,const auto &a) {return std::vector<std::uint32_t>{a[1]==0 ? 5u : 100u};};
      cast.callbacks[sk::CreatureModel]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      cast.callbacks[sk::SamplePointCount]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      cast.callbacks[sk::TerrainHeight]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{0};};
      cast.callbacks[sk::GameObjectConstructor]=[](sk::Model &self,const auto &a) {
        require(a==std::vector<std::uint32_t>{0x6000,champions::simulation::rta::mul(champions::simulation::rta::cos16(0),0x42900000),champions::simulation::rta::mul(champions::simulation::rta::sin16(0),0x42900000),0,0x502728,0x503750},("Skeleton supplied spot and player property list: "+std::to_string(a[1])+","+std::to_string(a[2])+","+std::to_string(a[3])+","+std::to_string(a[5])).c_str());
        self.mem.poke(0x6024,4,0xa000);return std::vector<std::uint32_t>{};
      };
      cast.callbacks[sk::CreatureSetCharm]=[](sk::Model &,const auto &a) {
        require(a==std::vector<std::uint32_t>{0x6000,UINT32_MAX,UINT32_MAX},"Skeleton charm uses signed team byte");return std::vector<std::uint32_t>{};
      };
      for (auto id:{sk::HolderElement4,sk::HolderElement6,sk::SoundPlayAtPosition,sk::HandleLinkStackCallback})
        cast.callbacks[id]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{};};
      cast.named_callbacks["Holder.v2"]=[](sk::Model &,const auto &) {return sk::ResultValue{};};
      cast.callbacks[sk::ResourceFind]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{123};};
      unsigned allocations=0;
      cast.callbacks[sk::PoolNew]=[&](sk::Model &,const auto &a) {
        require(a==std::vector<std::uint32_t>{allocations ? 0x1cu : 0x468u},"Skeleton pool allocation order and sizes");
        return std::vector<std::uint32_t>{allocations++ ? 0x8000u : 0x6000u};
      };
      require(cast.execute(sk::Operation::SkeletonCast,{0x1000,1,0x7000,0,0,0,0})==std::vector<std::uint32_t>{0x8000},
              "Skeleton returns separately allocated spell");
      require(cast.draws==3 && allocations==2 && cast.mem.load(0x63c0,4)==0x43fa0000 &&
              cast.mem.load(0x63ac,2)==400 && (cast.mem.load(0x60a8,8)&0x400000),
              "Skeleton exact chance, spell draws and inherited home-range body");

    }
    {
      sk::Model tagar;
      tagar.mem.poke(0x7000+0x38,2,0x4000);
      tagar.mem.poke(0x7000+0x10,4,ONE);
      tagar.mem.poke(0x6070,4,0xdeadbeef);
      tagar.execute(sk::Operation::TagarCtor,{0x6000,256,0x7000,0x1000});
      require(tagar.draws==1 && tagar.mem.load(0x6000,1)==0 &&
              tagar.mem.load(0x6014,4)==0x51ba60 && tagar.mem.load(0x6070,4)==0xdeadbeef,
              "arena base preserves stale bytes and draws once");
      require(tagar.mem.load(0x6080,2)==65535 && tagar.mem.load(0x6082,2)==0,
              "Tagar victim state initialized");
      // Base enemy/team bodies execute without fabricated Creature callbacks.
      tagar.mem.poke(0x7048,4,0x2000);tagar.mem.poke(0x2000+20*8+4,4,0x119328);
      tagar.mem.poke(0x2000+37*8+4,4,0x3c2730);tagar.mem.poke(0x7310,1,255);
      tagar.mem.poke(0x8048,4,0x3000);tagar.mem.poke(0x3000+37*8+4,4,0x3c2730);
      tagar.mem.poke(0x8310,1,1);tagar.mem.poke(0x800c,4,0x20);
      require(tagar.execute(sk::Operation::TagarValid,{0x6000,0x8000,0})[0]==1 && tagar.calls.empty(),
              "Tagar uses inherited signed team bytes");
      tagar.mem.poke(0x8048,4,0x3000);tagar.mem.poke(0x8310,1,255);
      require(tagar.execute(sk::Operation::TagarValid,{0x6000,0x8000,0})[0]==0,
              "same team rejected without marker service");
      tagar.mem.poke(0x8310,1,1);
      tagar.callbacks[sk::FindSpellMarker]=[](sk::Model &self,const auto &a) {
        require(a==std::vector<std::uint32_t>{0x8000,0x57,0x7000},"marker lookup signature");
        self.mem.poke(0x8100,4,7);
        self.callbacks[sk::FindSpellMarker]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{9};};
        return std::vector<std::uint32_t>{0};
      };
      require(tagar.execute(sk::Operation::TagarValid,{0x6000,0x8000,1})[0]==1,
              "active Tagar marker callback lifetime");
      require(tagar.execute(sk::Operation::TagarValid,{0x6000,0x8000,1})[0]==0 && tagar.mem.load(0x8100,4)==7,
              "Tagar callback replacement and mutation retained");
      // Modelled arena bodies at unrelated virtual slots explicitly refuse.
      tagar.mem.poke(0x2000+20*8+4,4,0x27236c);
      const auto before=tagar.calls.size();
      refuses([&] {tagar.execute(sk::Operation::TagarValid,{0x6000,0x8000,0});});
      require(tagar.calls.size()==before,"unported creature body never turns into a callback");
      tagar.external.insert(sk::CreatureEnemy);
      tagar.callbacks[sk::CreatureEnemy]=[](sk::Model &,const auto &) {return std::vector<std::uint32_t>{1};};
      require(tagar.execute(sk::Operation::TagarValid,{0x6000,0x8000,0})[0]==1,
              "external force overrides modelled creature dispatch");
      // Run expiry at address zero advances clock but skips destruction.
      tagar.execute(sk::Operation::TagarRun,{0,0,0});
      require(tagar.mem.load(0x78,4)==0x3c888888,"null spell expiry clock store");
      for (const auto &entry:std::vector<std::pair<sk::Operation,std::vector<std::uint32_t>>>{
          {sk::Operation::TagarCast,{0}}, {sk::Operation::TagarCtor,{0}},
          {sk::Operation::TagarValid,{0}}, {sk::Operation::TagarPickTarget,{0}},
          {sk::Operation::TagarTickVictims,{0,0}}, {sk::Operation::TagarRun,{0}}})
        refuses([&] {tagar.execute(entry.first,entry.second);});
    }
    std::cout
        << "skills-c API refusals, callback lifetime and recovery passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
