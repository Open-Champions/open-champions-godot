// Independent authored API anchors for the published a0d89502 ref_creature
// movement/water/settle contract. No retail data or generated oracle fixture.
#include "champions/creature_movement.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace R = champions::simulation::rta;
namespace M = R::creature_movement;
namespace {
unsigned checks = 0;
constexpr std::uint32_t C = 0x1000, A = 0x2000, B = 0x3000, VT = 0x8000,
                        NODE = 0x9000;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
template <class E, class F> void rejects(F fn, const char *message) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, message);
}
struct CallbackFailure : std::runtime_error {
  CallbackFailure() : std::runtime_error("authored callback failure") {}
};
using V = M::Value;
using Args = std::vector<V>;
constexpr std::uint64_t WET = UINT64_C(1) << 61;
void setup(M::Model &m) {
  m.mem.poke(C + 0x1c, 4, R::ONE);
  m.mem.poke(C + 0x20, 4, 0x41000000); // Height eight.
  m.mem.poke(C + 0xa8, 8, 0x100000);   // Disable water effects by default.
  m.mem.poke(M::G_WORLD, 4, 0xabcdef00);
  m.mem.poke(M::G_SIM_FRAME, 4, 1);
}
std::vector<std::string> names(const M::Model &m) {
  std::vector<std::string> out;
  for (const auto &c : m.calls)
    out.push_back(c.name);
  return out;
}
void contact(M::Model &m, std::uint32_t object, R::Raw x = R::ONE) {
  m.mem.poke(object + 0x10, 4, x);
  m.mem.poke(object + 0x1c, 4, R::ONE);
  m.mem.poke(object + 0x20, 4, R::TWO);
}
V slide(std::int64_t hit = 0, M::Vec3 normal = {}) {
  return V::tuple({V::integer(hit), V::vector(normal)});
}
void quiet_handlers(M::Model &m) {
  m.callbacks["world_collectObjectsInBox"] = [](M::Model &, const Args &) {
    return V::list({});
  };
  m.callbacks["terrain_heightBelow"] = [](M::Model &, const Args &) {
    return V::integer(0);
  };
}
void all_contexts() {
  static_assert(!std::is_copy_constructible<M::Model>::value,
                "callback-bearing model identity must remain stable");
  static_assert(!std::is_move_constructible<M::Model>::value,
                "active model must not move");
  for (unsigned mode = 0; mode < 4; ++mode)
    for (bool bias : {false, true})
      for (bool nearest : {false, true}) {
        R::Fpu f(static_cast<R::Fpu::Model>(mode), bias, nearest);
        R::ScopedFpu context(f);
        {
          M::Model m;
          setup(m);
          m.mem.poke(C + 0x231, 1, 0x88);
          m.mem.poke(C + 0xbc, 8, UINT64_C(0xdeadbeef12345678));
          const auto before = m.mem.bytes();
          check(m.move_by(C, {0xffffffff, 0x7fffffff, 0x80000000}) == 0,
                "hidden movement returns zero in every arithmetic context");
          check(m.mem.bytes() == before && m.writes.empty() &&
                    m.calls.empty() && m.events.empty() && m.draws == 0,
                "hidden actor preserves all memory, callbacks, and histories");
        }
        for (unsigned timer : {0U, 1U, 0xffffU}) {
          M::Model m(M::CallbackPolicy::ReferenceZero);
          setup(m);
          m.mem.poke(C + 0x38, 2, 0xfffe);
          m.mem.poke(C + 0xc4, 2, 5);
          m.mem.poke(C + 0xbc, 4, R::ONE);
          m.mem.poke(C + 0xc0, 4, R::MINUS_ONE);
          m.mem.poke(C + 0x314, 2, timer);
          unsigned slides = 0;
          const auto vx = timer ? 0 : f.add(R::ONE, f.mul(R::ONE, 0x42700000));
          const auto vy = timer ? 0 : f.add(0, f.mul(R::MINUS_ONE, 0x42700000));
          m.callbacks["world_slideMove"] = [&](M::Model &, const Args &args) {
            ++slides;
            check(args.size() == 5 && args[0].scalar == C + 0x10 &&
                      args[1].vec == M::Vec3{f.mul(vx, 0x3c888888),
                                             f.mul(vy, 0x3c888888),
                                             f.mul(R::TWO, 0x3c888888)} &&
                      args[2].scalar == 3 && args[3].scalar == 1 &&
                      args[4].scalar == R::ONE,
                  "root velocity uses explicit selected FPU and ordered dt");
            return slide();
          };
          check(m.move_by(C, {R::ONE, 0, R::TWO}) == 4,
                "zero-ground gravity lands after movement");
          check(m.mem.load(C + 0x38, 2) == 3 && m.mem.load(C + 0xbc, 8) == 0 &&
                    m.mem.load(C + 0xc4, 2) == 0,
                "yaw wraps before timer suppression and root fields are "
                "consumed");
          check(
              m.mem.load(C + 0x308, 4) == vx &&
                  m.mem.load(C + 0x30c, 4) == vy && slides == (timer ? 0U : 1U),
              "positive and negative root timers suppress every velocity axis");
          check(m.mem.load(C + 0x18, 4) == 0 && m.mem.load(C + 0x30, 4) == 0,
                "landing clears negative vertical speed");
        }
        {
          M::Model m(M::CallbackPolicy::ReferenceZero);
          setup(m);
          m.mem.poke(C + 0x18, 4, 0x42c80000);
          const auto fall = f.sub(0, 0x410fffff);
          check(m.move_by(C, {}) == 0,
                "airborne zero-velocity actor does not land");
          check(m.mem.load(C + 0x30, 4) == fall &&
                    m.mem.load(C + 0x18, 4) ==
                        f.add(0x42c80000, f.mul(fall, 0x3c888888)),
                "gravity uses caller FPU, never host floating point");
        }
      }
}
void ordered_contacts_and_virtuals() {
  R::ScopedFpu fixed(R::Fpu{});
  M::Model m(M::CallbackPolicy::ReferenceZero);
  setup(m);
  contact(m, A);
  contact(m, B, R::MINUS_ONE);
  m.mem.poke(C + 0xc, 4, 0x40); // A player's touch activates base slot 6.
  m.mem.poke(A + 0x48, 4, VT);
  m.mem.poke(VT + 6 * 8, 2, 16);
  m.mem.poke(VT + 6 * 8 + 4, 4, 0x11a418);
  m.mem.poke(B + 0x48, 4, VT + 0x100);
  m.mem.poke(VT + 0x100 + 6 * 8, 2, 0xfff0);
  m.callbacks["world_collectObjectsInBox"] = [](M::Model &, const Args &args) {
    check(args.size() == 3 && args[2].scalar == 16,
          "collection capacity remains the original sixteen");
    return V::list(
        {V::integer(C), V::integer(A), V::integer(B), V::integer(B)});
  };
  unsigned slides = 0, other_calls = 0;
  m.callbacks["world_slideMove"] = [&](M::Model &, const Args &args) {
    ++slides;
    check(args[3].scalar == (slides == 1 ? 1 : 0),
          "initial slide requests a normal; object push-outs do not");
    return slide(1, {0, R::ONE, 0});
  };
  m.callbacks["Creature.v6"] = [&](M::Model &, const Args &args) {
    ++other_calls;
    check(args.size() == 2 && args[0].scalar == B - 16 && args[1].scalar == C,
          "foreign slot six uses signed this adjustment and toucher address");
    return V{};
  };
  m.callbacks["Creature.v10"] = [](M::Model &, const Args &args) {
    check(args.size() == 2 && args[0].scalar == C &&
              args[1].vec == M::Vec3{0, R::ONE, 0},
          "terrain callback receives saved initial normal");
    return V{};
  };
  m.callbacks["Creature.v11"] = [](M::Model &self, const Args &args) {
    check(args.size() == 2 && args[1].kind == M::ValueKind::Tuple &&
              args[1].items.size() == 3 && args[1].items[0].scalar == A &&
              args[1].items[1].scalar == B && args[1].items[2].scalar == B,
          "touch tuple preserves original order and duplicate contacts");
    check(self.mem.load(C + 0x30, 4) == 0,
          "touch notification occurs after gravity and landing");
    return V{};
  };
  check(m.move_by(C, {0x42700000, 0, 0}) == 7,
        "terrain slide, contact and landing bits compose");
  check(slides == 4 && other_calls == 2,
        "base slot body is direct and duplicate foreign touches remain "
        "observable");
  check(m.mem.load(A + 16 + 0xa8, 8) == (UINT64_C(1) << 59),
        "base slot six writes bit59 at adjusted receiver");
  check(
      names(m) ==
          std::vector<std::string>{
              "world_slideMove", "obj_updateGridCell",
              "world_collectObjectsInBox", "world_slideMove",
              "obj_updateGridCell", "world_slideMove", "obj_updateGridCell",
              "Creature.v6", "world_slideMove", "obj_updateGridCell",
              "Creature.v6", "terrain_heightBelow", "Creature.v10",
              "Creature.v11"},
      "terrain, push, touch, ground and final virtual callbacks retain order");
  m.external.insert("Creature.v6");
  m.callbacks["Creature.v6"] = [](M::Model &, const Args &args) {
    check(args[0].scalar == A + 16,
          "external override retains this adjustment");
    return V{};
  };
  const auto count = m.writes.size();
  m.vcall(A, 6, {V::integer(C)});
  check(m.writes.size() == count && m.calls.back().name == "Creature.v6",
        "explicit external base override bypasses base-body write");
}
void standing_and_settle() {
  R::ScopedFpu fixed(R::Fpu{});
  {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    contact(m, A, 0);
    m.mem.poke(A + 0xc, 4, 0x10);
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &, const Args &) {
      return V::list({V::integer(A)});
    };
    check(m.move_by(C, {}) == 4 && m.mem.load(C + 0x18, 4) == R::TWO,
          "small standable step overrides lower terrain without touching");
    check(names(m) == std::vector<std::string>{"world_collectObjectsInBox",
                                               "terrain_heightBelow"},
          "standing branch skips push and slot-six notification");
  }
  for (bool throw_last : {false, true}) {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    contact(m, A);
    contact(m, B);
    m.mem.poke(C + 0xc, 4, 0x20);
    m.mem.poke(C + 0x234, 4, NODE);
    m.mem.poke(NODE + 0xc, 4, A);
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &, const Args &) {
      return V::list({V::integer(A), V::integer(B)});
    };
    m.callbacks["world_slideMove"] = [](M::Model &self, const Args &) {
      self.mem.poke(C + 0x10, 4, 0x40a00000);
      return slide();
    };
    unsigned touches = 0;
    m.callbacks["Creature.v6"] = [&](M::Model &, const Args &) {
      ++touches;
      return V{};
    };
    m.callbacks["Creature.v11"] = [&](M::Model &self, const Args &args) {
      check(self.mem.load(C + 0x10, 8) == 0 && self.mem.load(C + 0x18, 4) == 0,
            "settling restores original complete position before final touch");
      check(args[1].items.size() == 1 && args[1].items[0].scalar == A,
            "settling breaks at raw handle target before second contact");
      if (throw_last)
        throw CallbackFailure();
      return V{};
    };
    if (throw_last)
      rejects<CallbackFailure>([&] { m.settle(C); },
                               "settle propagates last callback error");
    else
      m.settle(C);
    check(touches == 1 &&
              m.mem.load(C + 0xc, 4) == (throw_last ? 0x8000020U : 0x20U),
          "settle clears temporary flag only after normal completion");
  }
  {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.mem.poke(C + 0x30, 4, 0x41200000); // Rising, but within snap tolerance.
    m.callbacks["terrain_heightBelow"] = [](M::Model &, const Args &) {
      return V::integer(R::ONE);
    };
    check(m.move_by(C, {}) == 0 && m.mem.load(C + 0x18, 4) == R::ONE &&
              m.mem.load(C + 0x30, 4) != 0,
          "rising actor snaps to ground without landing bit or velocity "
          "clearing");
  }
}
void water_states() {
  for (unsigned mode = 0; mode < 4; ++mode)
    for (bool bias : {false, true})
      for (bool nearest : {false, true}) {
        R::ScopedFpu selected(
            R::Fpu(static_cast<R::Fpu::Model>(mode), bias, nearest));
        M::Model m;
        setup(m);
        m.mem.poke(C + 0xa8, 8, 0x400);
        m.mem.poke(C + 0x44, 4, 1);
        m.mem.poke(C + 0x230, 1,
                   0xf9); // Signed -7 still has cadence residue 1.
        m.mem.poke(0x404464, 4, 0x11223344);
        m.mem.poke(0x404460, 4, 0x55667788);
        m.callbacks["water_test"] = [](M::Model &self, const Args &args) {
          check(args.size() == 4 && args[0].scalar == 0xabcdef00 &&
                    args[1].vec == M::Vec3{} && args[2].scalar == 0 &&
                    args[3].scalar == 0x41400000,
                "entry water query receives original world, position and "
                "constants");
          self.mem.poke(C + 0xa8, 8, 0x800);
          return V::integer(-1);
        };
        m.callbacks["libc_sprintf"] = [](M::Model &self, const Args &args) {
          check(args[0].scalar == M::S_SPLASH && args[1].scalar == 1 &&
                    self.mem.load(C + 0x44, 4) == 0x3c88596c,
                "one own-generator draw precedes fixed splash number one");
          return V::buffer(std::string("splash\0one", 10));
        };
        m.callbacks["res_find"] = [](M::Model &, const Args &args) {
          check(
              args[0].scalar == M::S_FX_LUMP &&
                  args[1].kind == M::ValueKind::Buffer &&
                  args[1].bytes == std::string("splash\0one", 10),
              "resource lookup preserves byte buffer including embedded null");
          return V::integer(0x76543210);
        };
        m.callbacks["sound_playOnObject"] = [](M::Model &, const Args &args) {
          check(
              args.size() == 3 && args[0].scalar == 0x76543210 &&
                  args[1].scalar == C && args[2].scalar == 0x3f199999,
              "sound callback receives selected entry, actor, and raw volume");
          return V::integer(0);
        };
        m.callbacks["water_disturb"] = [](M::Model &, const Args &args) {
          check(args[2].scalar == 0x44bb8000 && args[3].scalar == 0x41c00000,
                "entry splash uses its separate large disturbance constants");
          return V::integer(0); // Entry does not clear WET from this answer.
        };
        m.water(C);
        check(m.mem.load(C + 0xa8, 8) == (WET | 0x800) &&
                  m.mem.load(C + 0x44, 4) == 0x3c88596c,
              "water entry reloads callback-mutated flags then marks wet");
        check(m.draws == 0 && m.mem.load(0x404464, 4) == 0x11223344 &&
                  m.mem.load(0x404460, 4) == 0x55667788,
              "water affects neither shared nor cosmetic random stream");
        check(names(m) == std::vector<std::string>{"water_test", "libc_sprintf",
                                                   "res_find",
                                                   "sound_playOnObject",
                                                   "water_disturb"},
              "water entry service order is preserved");
        check(m.events.size() == 7 && m.events[0].kind == M::EventKind::Call &&
                  m.events[1].kind == M::EventKind::Write &&
                  m.events[1].write.address == C + 0xa8 &&
                  m.events[2].kind == M::EventKind::Write &&
                  m.events[2].write.address == C + 0x44,
              "ordered event stream places both state writes before sound "
              "lookup");
      }
  {
    M::Model m;
    setup(m);
    m.mem.poke(C + 0xa8, 8, 0);
    m.water(C);
    check(m.calls.empty() && m.writes.empty(),
          "dry cadence miss needs no handler");
    m.mem.poke(C + 0xa8, 8, WET);
    m.water(C);
    check(m.calls.empty() && m.writes.empty(),
          "quiet wet actor skips off-cadence disturbance");
    m.mem.poke(M::G_SIM_FRAME, 4, 16);
    m.callbacks["water_disturb"] = [](M::Model &self, const Args &args) {
      check(args[2].scalar == 0x43960000 && args[3].scalar == 0x41400000,
            "wet cadence uses small disturbance constants");
      self.mem.poke(C + 0xa8, 8, WET | 0x123);
      return V::integer(0);
    };
    m.water(C);
    check(m.mem.load(C + 0xa8, 8) == 0x123,
          "leaving water preserves fresh callback flag mutations");
    m.mem.poke(C + 0xa8, 8, WET | 0x100001);
    const auto calls = m.calls.size();
    m.water(C);
    check(m.mem.load(C + 0xa8, 8) == 0x100001 && m.calls.size() == calls,
          "effects-disabled path clears wet without service or RNG calls");
  }
  {
    M::Model m;
    setup(m);
    m.mem.poke(C + 0xa8, 8, 0);
    m.mem.poke(C + 0x230, 1, 1);
    m.mem.poke(C + 0x44, 4, 1);
    m.callbacks["water_test"] = [](M::Model &, const Args &) {
      return V::integer(1);
    };
    m.callbacks["libc_sprintf"] = [](M::Model &, const Args &) -> V {
      throw CallbackFailure();
    };
    rejects<CallbackFailure>([&] { m.water(C); },
                             "water callback exception propagates");
    check(
        m.mem.load(C + 0xa8, 8) == WET &&
            m.mem.load(C + 0x44, 4) == 0x3c88596c && m.writes.size() == 2 &&
            m.calls.size() == 2 && m.events.size() == 4,
        "water failure retains wet flag and own seed, without later callbacks");
  }
}
void callbacks_and_failure_prefixes() {
  R::ScopedFpu fixed(R::Fpu{});
  {
    M::Model m;
    setup(m);
    rejects<M::MissingHandler>([&] { m.move_by(C, {}); },
                               "missing collector is strict by default");
    const std::array<std::uint32_t, 6> addresses{C + 0x38, C + 0x308, C + 0x30c,
                                                 C + 0xbc, C + 0xc4,  C + 0xa8};
    check(m.writes.size() == addresses.size() && m.calls.size() == 1 &&
              m.events.size() == 7 && m.mem.log().size() == 6,
          "missing collector keeps exactly the pre-collection mutation prefix");
    for (std::size_t k = 0; k < addresses.size(); ++k)
      check(m.writes[k].address == addresses[k] &&
                m.events[k].kind == M::EventKind::Write &&
                m.events[k].write.address == addresses[k],
            "ordered writes precede missing-handler call event");
    check(m.events.back().call.name == "world_collectObjectsInBox",
          "missing handler still records attempted call");
    quiet_handlers(m);
    check(m.move_by(C, {}) == 4,
          "movement primitive has no invented failure latch");
  }
  {
    M::Model m;
    setup(m);
    quiet_handlers(m);
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &self,
                                                  const Args &) {
      self.callbacks["world_collectObjectsInBox"] =
          [](M::Model &, const Args &) { return V::list({}); };
      self.callbacks["terrain_heightBelow"] = [](M::Model &later,
                                                 const Args &) {
        later.mem.poke(C + 0x30, 4, 0x41200000);
        return V::integer(0);
      };
      return V::list({});
    };
    check(m.move_by(C, {}) == 0 && m.mem.load(C + 0x30, 4) == 0x3f800008,
          "callback self-replacement is safe and later registry/velocity reads "
          "stay live");
    m.callbacks["terrain_heightBelow"] = [](M::Model &self, const Args &) -> V {
      self.mem.poke(C + 0x18, 4, R::TWO);
      throw CallbackFailure();
    };
    const auto writes = m.writes.size();
    rejects<CallbackFailure>([&] { m.move_by(C, {}); },
                             "ground callback exception propagates unchanged");
    check(m.mem.load(C + 0x18, 4) == R::TWO && m.writes.size() == writes + 6 &&
              m.calls.back().name == "terrain_heightBelow",
          "raw callback poke remains, with no gravity write after throwing "
          "ground");
  }
  {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.callbacks["terrain_heightBelow"] = [](M::Model &, const Args &) {
      R::set_fpu(R::Fpu::Model::Nearest);
      return V::integer(0);
    };
    R::ScopedFpu restore(R::Fpu{});
    m.mem.poke(C + 0x18, 4, R::ONE);
    m.move_by(C, {});
    R::Fpu nearest(R::Fpu::Model::Nearest);
    check(m.mem.load(C + 0x18, 4) ==
              nearest.add(R::ONE, nearest.mul(0xc10fffff, 0x3c888888)),
          "callback FPU selection affects subsequent original arithmetic");
  }
}
void optional_slide_normal() {
  R::ScopedFpu fixed(R::Fpu{});
  for (bool hit : {false, true}) {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.callbacks["world_slideMove"] = [hit](M::Model &self, const Args &args) {
      check(args[3].scalar == 1,
            "optional normal boundary exercises the initial terrain slide");
      // A callback can publish its position before returning either shape.
      self.mem.poke(C + 0x10, 4, R::ONE);
      return V::tuple({V::integer(hit ? 1 : 0), V{}});
    };
    if (hit) {
      rejects<M::Error>([&] { m.move_by(C, {0x42700000, 0, 0}); },
                        "hit without a normal is refused");
      check(names(m) == std::vector<std::string>{"world_slideMove"},
            "hit without normal refuses before grid, collect, ground or v10");
      check(m.writes.size() == 6 && m.mem.log().size() == 6 &&
                m.events.size() == 7 &&
                m.events.back().kind == M::EventKind::Call &&
                m.events.back().call.name == "world_slideMove",
            "invalid hit retains only pre-slide writes and attempted call");
    } else {
      check(m.move_by(C, {0x42700000, 0, 0}) == 4,
            "clear slide accepts None and still performs gravity and landing");
      check(names(m) == std::vector<std::string>{"world_slideMove",
                                                 "obj_updateGridCell",
                                                 "world_collectObjectsInBox",
                                                 "terrain_heightBelow"},
            "clear slide preserves callback order and never invokes v10");
      check(m.writes.size() == 10 && m.mem.log().size() == 10 &&
                m.events.size() == 14 && m.mem.load(C + 0x18, 4) == 0 &&
                m.mem.load(C + 0x30, 4) == 0,
            "clear slide retains complete normal landing mutation state");
    }
    check(m.mem.load(C + 0x10, 4) == R::ONE &&
              m.mem.load(C + 0x308, 4) == 0x42700000 &&
              m.mem.load(C + 0xbc, 8) == 0 && m.mem.load(C + 0xc4, 2) == 0,
          "callback position and consumed root prefix survive either outcome");
    const std::array<std::uint32_t, 6> prefix{C + 0x38, C + 0x308, C + 0x30c,
                                              C + 0xbc, C + 0xc4,  C + 0xa8};
    for (std::size_t k = 0; k < prefix.size(); ++k)
      check(m.writes[k].address == prefix[k] &&
                m.events[k].kind == M::EventKind::Write &&
                m.events[k].write.address == prefix[k],
            "optional normal handling preserves exact ordered write prefix");
  }
}
void invalid_shapes_and_limits() {
  for (const auto &bad :
       {V{}, V::integer(0), V::tuple({}), V::list({V::vector({})})}) {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.callbacks["world_collectObjectsInBox"] = [bad](M::Model &, const Args &) {
      return bad;
    };
    rejects<M::Error>([&] { m.move_by(C, {}); },
                      "invalid collection shape refused");
    check(m.calls.size() == 1 && m.writes.size() == 6,
          "collection shape failure occurs after recorded callback and before "
          "ground");
  }
  {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &, const Args &) {
      return V::list(std::vector<V>(17, V::integer(C)));
    };
    rejects<M::ResourceLimit>([&] { m.move_by(C, {}); },
                              "seventeenth collected entry refused");
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &, const Args &) {
      return V::list(std::vector<V>(16, V::integer(C)));
    };
    check(m.move_by(C, {}) == 4,
          "sixteen collected self entries safely skipped");
    for (auto a : {INT64_C(-1), INT64_C(0x100000000)}) {
      m.callbacks["world_collectObjectsInBox"] = [a](M::Model &, const Args &) {
        return V::list({V::integer(a)});
      };
      rejects<M::ResourceLimit>([&] { m.move_by(C, {}); },
                                "invalid collected address cannot wrap");
    }
  }
  for (const auto &bad : {V::integer(0), V::tuple({V::integer(0)}),
                          V::tuple({V::integer(0), V::integer(0)})}) {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.callbacks["world_slideMove"] = [bad](M::Model &, const Args &) {
      return bad;
    };
    rejects<M::Error>([&] { m.move_by(C, {0x42700000, 0, 0}); },
                      "initial slide requires hit and normal tuple");
    check(m.calls.size() == 1 && m.calls.back().name == "world_slideMove",
          "malformed slide output prevents grid and collection callbacks");
  }
  {
    M::Model m;
    const auto before = m.mem.bytes();
    rejects<M::ResourceLimit>([&] { m.move_by(UINT64_MAX, {}); },
                              "huge actor address refused");
    rejects<M::ResourceLimit>([&] { m.water(UINT64_C(0x100000000)); },
                              "raw32 water address bound");
    rejects<M::ResourceLimit>(
        [&] { m.move_by(UINT32_MAX - 0x230, {}); },
        "field address overflow does not wrap low memory");
    check(m.mem.bytes() == before && m.events.empty(),
          "invalid initial read changes no state");
    rejects<M::Error>([&] { m.vcall(C, 2, {V::integer(A)}); },
                      "unported virtual slot refuses");
    rejects<M::Error>([&] { m.vcall(C, 10, {V::integer(A)}); },
                      "typed virtual normal required");
    V bad = V::integer(0);
    bad.bytes = "inactive";
    rejects<M::Error>([&] { m.vcall(C, 6, {bad}); },
                      "inactive Value payload refused");
    V nested = V::integer(0);
    for (unsigned k = 0; k < 6; ++k)
      nested = V::tuple({nested});
    rejects<M::ResourceLimit>([&] { m.vcall(C, 11, {nested}); },
                              "recursive Value depth bounded");
    auto large =
        V::tuple({V::buffer(std::string(M::MAX_BUFFER_BYTES + 1, 'x'))});
    rejects<M::ResourceLimit>([&] { m.vcall(C, 11, {large}); },
                              "callback buffer bytes bounded");
    auto wide = V::tuple(
        std::vector<V>(16, V::tuple(std::vector<V>(4, V::integer(0)))));
    rejects<M::ResourceLimit>([&] { m.vcall(C, 11, {wide}); },
                              "total callback value nodes bounded");
    m.external.insert("Creature.v2");
    rejects<M::Error>([&] { m.vcall(C, 6, {V::integer(A)}); },
                      "unported external virtual override refuses explicitly");
  }
  {
    M::Limits limits;
    limits.history_entries = 2;
    M::Model m(M::CallbackPolicy::ReferenceZero, limits);
    setup(m);
    rejects<M::ResourceLimit>([&] { m.move_by(C, {}); },
                              "history cap prevents third write");
    check(m.writes.size() == 2 && m.events.size() == 2 &&
              m.mem.log().size() == 2 && m.writes[0].address == C + 0x38 &&
              m.writes[1].address == C + 0x308,
          "history refusal retains bounded exact initial prefix");
  }
  {
    M::Limits limits;
    limits.memory_bytes = 1;
    M::Model m(M::CallbackPolicy::ReferenceZero, limits);
    rejects<M::ResourceLimit>([&] { m.obj_rand(C); },
                              "seed write respects sparse memory cap");
    check(
        m.mem.bytes().empty() && m.writes.size() == 1 && m.events.size() == 1 &&
            m.mem.log().size() == 1 && m.writes[0].value == 0x3c6ef35f,
        "failed backing allocation retains attempted write/event/store prefix");
  }
  {
    M::Limits limits;
    limits.depth = 1;
    M::Model m(M::CallbackPolicy::ReferenceZero, limits);
    setup(m);
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &self,
                                                  const Args &) {
      self.move_by(C, {});
      return V::list({});
    };
    rejects<M::ResourceLimit>([&] { m.move_by(C, {}); },
                              "nested operation depth bounded");
    check(m.writes.size() == 6 && m.calls.size() == 1,
          "nested refusal occurs before inner movement writes");
    m.callbacks.clear();
    check(m.move_by(C, {}) == 4,
          "exception unwinds operation depth for later explicit call");
  }
  {
    M::Limits limits;
    limits.callback_entries = 0;
    M::Model m(M::CallbackPolicy::ReferenceZero, limits);
    setup(m);
    quiet_handlers(m);
    rejects<M::ResourceLimit>([&] { m.move_by(C, {}); },
                              "callback registry cap checked at dispatch");
    check(m.calls.size() == 1,
          "callback registry refusal records attempted dispatch");
  }
  {
    M::Model m(M::CallbackPolicy::ReferenceZero);
    setup(m);
    m.callbacks[std::string(129, 'x')] = [](M::Model &, const Args &) {
      return V{};
    };
    rejects<M::ResourceLimit>([&] { m.move_by(C, {}); },
                              "callback label length bounded");
    check(m.calls.size() == 1 && m.writes.size() == 6,
          "overlong registration retains ordinary pre-dispatch prefix");
  }
}
} // namespace

int main() {
  try {
    all_contexts();
    ordered_contacts_and_virtuals();
    standing_and_settle();
    water_states();
    callbacks_and_failure_prefixes();
    optional_slide_normal();
    invalid_shapes_and_limits();
    std::cout << "creature movement: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "creature movement failed after " << checks
              << " checks: " << e.what() << '\n';
    return 1;
  }
}
