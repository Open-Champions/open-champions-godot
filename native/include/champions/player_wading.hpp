#pragma once
// Player wading, RTA US v2.00 only: Player_move 0x1b4020 step (f) (the water
// test, the in-water bit, the entry splash and the wading ripple) and the
// Player_footstep 0x1beec8 / sound_playFoley 0x1e7c40 selection.
//
// Authored from t3/research-wading 942a6ba (docs/spec/player-wading.md,
// tools/ref_wading.py; Executed by tools/difftest_wading.py). Every constant
// below is pinned to the instruction address that loads it in that document's
// section 11. Nothing here moves the player: wading has no speed effect
// (section 7), so this module never touches velocity, facing or the blend.
//
// The depth band is not coded here. It is the box the two water services
// query, which `creature_water::Service` already implements:
//   water_test(12.0)      ((x-12)-1, (y-12)-1, z-1) .. ((x+12)+1, (y+12)+1, z+60)
//   water_disturb at z-12 (x-12, y-12, z-13) .. (x+12, y+12, z+48), no padding
// so a dry player enters for dz in [-1, 60] and a wader stays for [-13, 48].
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <functional>

namespace champions::simulation::rta::player_wading {
using Vec3 = std::array<Raw, 3>;

inline constexpr std::uint64_t WATER_BIT = UINT64_C(1) << 61; // +0xa8 bit 61
inline constexpr std::uint32_t LEVEL_HURTS = 0x1000;          // word 0x44fe08, 0x1b4440
inline constexpr std::uint32_t SIXTEENTH = 0xf;               // 0x1b4454, 0x1b44b8
inline constexpr std::uint32_t ENTRY_GATE = 3;                // 0x1b45c0
inline constexpr Raw RADIUS_TEST = 0x41400000;                // 12.0, 0x1b45d4
inline constexpr Raw Z_OFFSET = 0x41400000;                   // 12.0, 0x1b44c4 / 0x1b4528
inline constexpr Raw MOVING = 0x3c23d70a;                     // 0.01, 0x1b4498
inline constexpr Raw WADE_STRENGTH = 0x43960000;              // 300.0, 0x1b4500 / 0x1b4564
inline constexpr Raw WADE_RADIUS = 0x41400000;                // 12.0, 0x1b44c4
inline constexpr Raw ENTRY_STRENGTH_AIR = 0x453b8000;         // 3000.0, 0x1b46a8
inline constexpr Raw ENTRY_STRENGTH_FOOT = 0x447a0000;        // 1000.0, 0x1b475c
inline constexpr Raw ENTRY_RADIUS = 0x41c00000;               // 24.0, 0x1b46cc / 0x1b477c
inline constexpr Raw VOLUME_AIR = 0x3f800000;                 // 1.0, 0x1b4668
inline constexpr Raw VOLUME_FOOT = 0x3ecccccc;                // 0.4, 0x1b4728
// damage_apply on itself (0x1b4490): attack 100000000, flags 0x1000030, range 1e8.
inline constexpr Raw HURT_AMOUNT = 100000000, HURT_FLAGS = 0x1000030,
                     HURT_RANGE = 0x4cbebc20;

struct TestResult {
  bool hit = false;
  bool has_height = false;
  Raw height = 0; // the first chunk's minimum z, used only by the airborne burst
};
// Each callback is the call the original makes at the same point.
struct Host {
  std::function<TestResult(const Vec3 &position, Raw radius)> water_test;
  // water_disturb(g_world, point; strength, radius) -> number of chunks touched.
  std::function<Raw(const Vec3 &point, Raw strength, Raw radius)> water_disturb;
  std::function<void()> hurt;                 // damage_apply(P, P, ...)
  std::function<void()> draw;                 // one shared-seed draw (value unused)
  std::function<void(Raw volume)> splash;     // sound_playOnObject, presentation
  std::function<void(const Vec3 &, const TestResult &)> burst; // fx_burst_1fbdf0
};
struct Input {
  Raw frame = 0;        // g_simFrame 0x44fdcc
  std::uint32_t level_flags = 0; // word 0x44fe08
  Raw speed = 0;        // the stick speed `s` of step (e), after the mode-2 / state-1 override
  Vec3 position{};      // player +0x10, +0x14, +0x18; z is the feet
  bool in_water = false; // +0xa8 bit 61
  bool airborne = false; // +0x47c
};

// The point both ripples use: (x - 0.0, y, z - 12.0), computed as written.
inline Vec3 ripple_point(const Vec3 &p) { return {sub(p[0], ZERO), p[1], sub(p[2], Z_OFFSET)}; }

// Step (f), in the original's call order. Returns the in-water bit afterwards.
// A missing callback is a refusal (std::bad_function_call), never a silent dry.
inline bool step(const Input &in, const Host &host) {
  if (in.in_water) {
    if ((in.level_flags & LEVEL_HURTS) && !(in.frame & SIXTEENTH))
      host.hurt();
    Raw touched = 1;
    if (!lt(in.speed, MOVING) || !(in.frame & SIXTEENTH))
      touched = host.water_disturb(ripple_point(in.position), WADE_STRENGTH, WADE_RADIUS);
    return touched != 0;
  }
  if (in.frame & ENTRY_GATE)
    return false;
  const auto hit = host.water_test(in.position, RADIUS_TEST);
  if (!hit.hit)
    return false;
  host.draw();
  host.splash(in.airborne ? VOLUME_AIR : VOLUME_FOOT);
  // The entry ripple's return value is ignored: the bit stays set.
  host.water_disturb(ripple_point(in.position),
                     in.airborne ? ENTRY_STRENGTH_AIR : ENTRY_STRENGTH_FOOT, ENTRY_RADIUS);
  if (in.airborne)
    host.burst(in.position, hit);
  return true;
}

// ---- the footstep ----------------------------------------------------------
inline constexpr Raw FOLEY_DEFAULT = 0x80000;   // flags with no item, 0x1beef8
inline constexpr Raw FOLEY_ANY = 0x780000;      // 0x1e7ce0
inline constexpr std::uint32_t FOLEY_WATER_FORMAT = 0x53ddd8, FOLEY_PLAIN_FORMAT = 0x53de30;
struct FoleyChoice {
  bool audible = false;
  std::uint32_t format = 0; // executable string address of the sprintf format
  unsigned n = 0;           // the variant number given to the format
};
// `draw` is one cosmetic-seed draw (seed 0x404460, d = seed >> 16 after the
// LCG step). It is called only when the step is audible. Never the shared seed.
inline FoleyChoice foley(Raw flags, bool wet, const std::function<Raw()> &draw) {
  if (wet)
    return {true, FOLEY_WATER_FORMAT, (draw() & 1) + 1};
  if (!(flags & FOLEY_ANY))
    return {};
  struct Row { Raw bit; std::uint32_t format; };
  for (const Row row : {Row{0x400000, 0x53dde8}, Row{0x200000, 0x53de00}, Row{0x100000, 0x53de18}})
    if (flags & row.bit)
      return {true, row.format, draw() % 5 + 1};
  return {true, FOLEY_PLAIN_FORMAT, (draw() & 3) + 1};
}
// Player_footstep: low word of item+8 for the first non-null of record
// +0x4400, +0x4418, else 0x80000.
inline Raw footstep_flags(bool has_item, Raw item_flags) { return has_item ? item_flags : FOLEY_DEFAULT; }
// One cosmetic draw: the same LCG as the shared seed, returning seed >> 16.
inline Raw cosmetic_draw(Raw &seed) {
  seed = Raw(seed * 0x19660dU + 0x3c6ef35fU);
  return seed >> 16;
}
} // namespace champions::simulation::rta::player_wading
