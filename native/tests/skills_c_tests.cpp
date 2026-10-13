#include "champions/skills_c.hpp"
#include <iostream>
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
    std::cout
        << "skills-c API refusals, callback lifetime and recovery passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
