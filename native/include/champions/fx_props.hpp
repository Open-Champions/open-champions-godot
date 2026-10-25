#pragma once

#include "champions/core_math.hpp"
#include "champions/core_state.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Exact port of the Return to Arms fire-prop reference tools/ref_fx_props.py (research
// commit 19f80a6, round 2, executed bit for bit there): what Torch_run 0x2b2088,
// Candle_run 0x2b2dc4, Candle2_run 0x2b3164 (factory name Candle3), Lantern_run 0x2b3554
// and Lamp_run 0x2b3858 do each tick. Spawns are returned as call records
// (particle_spawn's four arguments) for the caller to apply; definitions are named by
// address only and their values come from the user's executable at run time. Floats are
// raw single-precision patterns through the champions-core port of tools/ref_core.py.
namespace champions::rta::fx::props {

using Raw = champions::simulation::rta::Raw;
using Vec3 = std::array<Raw, 3>;
using NoiseTable = champions::simulation::rta::NoiseTable;  // g_noiseTable 0x6e0d90 (per machine)

enum class FireClass { Torch, Candle, Candle3, Lantern, Lamp };

inline constexpr std::uint32_t DEF_TORCH_FLAME = 0x8E4740, DEF_TORCH_SMOKE = 0x8E47E0,
                               DEF_TORCH_SPARK = 0x8E4830, DEF_CANDLE = 0x8E4880,
                               DEF_CANDLE3 = 0x8E48D0, DEF_LAMP = 0x8E4920;
inline constexpr Raw K_INV65536 = 0x37800000u, K_FLAME_LIFT = 0x40999999u, K_SMOKE = 0x41A00000u,
                     K_SPARK = 0x41F2E147u, K_SPARK_LIFT = 0x414A6666u, K_LAMP = 0x403624DDu,
                     K_CANDLE_RISE = 0x3E999999u, K_CANDLE_LIFT = 0x40600000u;
inline constexpr std::uint32_t NOISE_A = 0x19660D, NOISE_B = 0x7EFE41, NOISE_Z = 0x32CC1A;
inline constexpr int STOP_AFTER = 31;  // run calls without a draw before obj_stopRunning

// One particle_spawn call: f12 back-date (a pattern, 0 or 0.25), the definition address,
// position and velocity.
struct Spawn {
    Raw backdate = 0;
    std::uint32_t definition = 0;
    Vec3 position{};
    Vec3 velocity{};
    bool operator==(const Spawn &o) const {
        return backdate == o.backdate && definition == o.definition && position == o.position &&
               velocity == o.velocity;
    }
};
struct Tick {
    std::vector<Spawn> spawns;
    bool stopped = false;  // obj_stopRunning was called on this tick
};

// The fields the run functions read and write. Positions and radii are float patterns;
// `addr` is the object's address (it feeds the noise index); `rng` is the object's own
// generator (GameObject_rand, state at +0x44).
struct FireProp {
    FireClass cls = FireClass::Torch;
    std::uint32_t addr = 0;
    Vec3 pos{};
    champions::simulation::rta::Lcg rng;
    std::uint8_t kind = 0;      // Torch +0x8a: 0-5 (Torch, Fire, Campfire, Firepot, Firepot2, OilLamp)
    bool big = false;           // Candle +0x90 == 1
    std::string level_name;     // Lantern: g_levelName 0x41b721
    Raw model_radius = 0x43700000u, level_radius = 0x43700000u;     // 240.0
    Raw model_falloff = 0x3791A2B3u, level_falloff = 0x3791A2B3u;   // 1 / 240^2 as the ctors store it
    Vec3 light_pos{};
    int counter = 0;            // candle: ticks since the last radius change
    std::uint8_t undrawn = 0;   // s8 counter (+0x89, +0x8d candle, +0x95 lantern, +0x4d candle3)
    bool running = true;

    FireProp(FireClass cls, std::uint32_t addr, const Vec3 &pos, std::uint32_t seed,
             int kind = 0, bool big = false, std::string level_name = {});
};

Tick torch_run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table);
Tick candle_run(FireProp &p, std::int64_t sim_frame = 0, const NoiseTable *table = nullptr);
Tick candle3_run(FireProp &p, std::int64_t sim_frame = 0, const NoiseTable *table = nullptr);
Tick lantern_run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table);
Tick lamp_run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table);
// The run function of the prop's class (RUN in the reference).
Tick run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table);

}  // namespace champions::rta::fx::props
