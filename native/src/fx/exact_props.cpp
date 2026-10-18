#include "champions/fx_props.hpp"

#include "champions/fx_decals.hpp"

#include <utility>

namespace champions::rta::fx::props {
namespace C = champions::simulation::rta;
namespace {

Raw k(double x) { return decals::f32(x); }  // ref_core.f32 of a constant of the code
Raw abs_if_negative(Raw z) { return C::lt(z, 0) ? C::fabs(z) : z; }

// The s8 counter: obj_stopRunning when it is 31 or more (as signed), then +1.
bool undrawn_s8(FireProp &p) {
    const int v = (p.undrawn & 0x80) ? p.undrawn - 0x100 : p.undrawn;
    const bool stopped = v >= STOP_AFTER;
    if (stopped)
        p.running = false;
    p.undrawn = static_cast<std::uint8_t>((p.undrawn + 1) & 0xFF);
    return stopped;
}

void set_radius(FireProp &p, Raw value) {
    p.model_radius = value;
    p.level_radius = value;
    const Raw inv = C::div(C::ONE, C::mul(value, value));
    p.model_falloff = p.level_falloff = inv;
}

std::uint32_t frame32(std::int64_t sim_frame) { return static_cast<std::uint32_t>(sim_frame); }

Raw flicker_radius(const FireProp &p, std::int64_t sim_frame, const NoiseTable &table, int base) {
    const Raw n = C::noise(table, p.addr * NOISE_A + frame32(sim_frame) * 16u + NOISE_B);
    return C::from_int(static_cast<std::uint32_t>(C::to_int(C::mul(n, k(22.0))) + base));
}

std::pair<Vec3, Vec3> candle_particle(C::Lcg &rng) {
    const auto v = rng.in_unit_sphere();
    const Vec3 vel{0, 0, C::add(C::mul(v[2], K_CANDLE_RISE), K_CANDLE_LIFT)};
    const auto u = rng.in_unit_sphere();
    return {vel, u};
}

}  // namespace

FireProp::FireProp(FireClass cls_, std::uint32_t addr_, const Vec3 &pos_, std::uint32_t seed,
                   int kind_, bool big_, std::string level_name_)
    : cls(cls_), addr(addr_), pos(pos_), rng(seed), kind(static_cast<std::uint8_t>(kind_ & 0xFF)),
      big(big_), level_name(std::move(level_name_)), light_pos(pos_) {}

Tick torch_run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table) {
    C::Lcg &rng = p.rng;
    std::vector<Spawn> spawns;
    const int kind = p.kind;
    const int count = kind == 2 ? 3 : kind == 4 ? 4 : kind == 5 ? 3 : 1;
    for (int n = 0; n < count; ++n) {
        const auto v = rng.in_unit_sphere();
        Vec3 vel{C::add(v[0], v[0]), C::add(v[1], v[1]), C::add(C::add(v[2], v[2]), K_FLAME_LIFT)};
        const auto u = rng.in_unit_sphere();
        Raw x = u[0], y = u[1], z = u[2];
        if (kind == 2 || kind == 5) {
            x = C::mul(x, k(10.0));
            y = C::mul(y, k(10.0));
            z = C::add(z, k(5.0));
        } else if (kind == 3) {
            x = C::mul(x, k(5.0));
            y = C::mul(y, k(5.0));
            z = C::add(z, k(10.0));
        } else if (kind == 4) {
            x = C::mul(x, k(15.0));
            y = C::mul(y, k(15.0));
            z = C::add(z, k(10.0));
        } else if (kind == 1) {
            x = C::mul(x, k(10.0));
            y = C::mul(y, k(10.0));
        } else {
            x = C::mul(x, k(4.0));
            y = C::mul(y, k(4.0));
            z = C::mul(z, k(4.0));
        }
        z = abs_if_negative(z);
        const Vec3 pos{C::add(x, p.pos[0]), C::add(y, p.pos[1]), C::sub(C::add(z, p.pos[2]), k(12.0))};
        const Raw back = kind == 3 ? k(0.25) : 0;
        spawns.push_back({back, DEF_TORCH_FLAME, pos, vel});
        if ((sim_frame & 7) == 0 && static_cast<std::uint32_t>(kind - 4) > 1u) {
            const std::uint32_t r = rng.draw();
            vel[2] = C::add(vel[2], C::mul(C::mul(C::from_int(r), K_INV65536), K_SMOKE));
            const auto w = rng.in_unit_sphere();
            const Raw sx = C::mul(w[0], k(10.0)), sy = C::mul(w[1], k(10.0));
            const Raw sz = abs_if_negative(C::mul(w[2], k(10.0)));
            spawns.push_back({0, DEF_TORCH_SMOKE,
                              {C::add(sx, p.pos[0]), C::add(sy, p.pos[1]), C::add(sz, p.pos[2])}, vel});
        }
    }
    const std::uint32_t base = p.addr * NOISE_A;
    set_radius(p, flicker_radius(p, sim_frame, table, 160));
    const Raw eight = k(8.0);
    const std::uint32_t f2 = frame32(sim_frame) * 2u;
    p.light_pos = {C::add(p.pos[0], C::mul(C::noise(table, f2 + base), eight)),
                   C::add(p.pos[1], C::mul(C::noise(table, base + f2 + NOISE_A), eight)),
                   C::add(p.pos[2], C::mul(C::noise(table, NOISE_Z + base + f2), eight))};
    if (kind != 0 && kind != 4) {
        if (rng.draw() <= 0x319) {
            const auto w = rng.in_unit_sphere();
            const Vec3 vel{C::mul(w[0], K_SPARK), C::mul(w[1], K_SPARK),
                           C::add(C::mul(w[2], K_SPARK), K_SPARK_LIFT)};
            spawns.push_back({0, DEF_TORCH_SPARK, p.pos, vel});
        }
    }
    const bool stopped = undrawn_s8(p);
    return {std::move(spawns), stopped};
}

Tick candle_run(FireProp &p, std::int64_t, const NoiseTable *) {
    C::Lcg &rng = p.rng;
    const auto vu = candle_particle(rng);
    const Vec3 &vel = vu.first;
    const Vec3 &u = vu.second;
    Raw z = abs_if_negative(u[2]);
    if (p.big)
        z = C::add(C::add(z, p.pos[2]), k(8.0));
    else
        z = C::sub(C::add(z, p.pos[2]), k(4.0));
    std::vector<Spawn> spawns{{0, DEF_CANDLE, {p.pos[0], p.pos[1], z}, vel}};
    p.counter += 1;
    if (p.counter == 5) {
        p.counter = 0;
        const std::uint32_t r = rng.draw();
        set_radius(p, C::from_int(((r - 8u) & 0xFu) + (p.big ? 0x54u : 0x30u)));
    }
    const bool stopped = undrawn_s8(p);
    return {std::move(spawns), stopped};
}

Tick candle3_run(FireProp &p, std::int64_t, const NoiseTable *) {
    const auto vu = candle_particle(p.rng);
    Raw z = C::add(vu.second[2], vu.second[2]);
    z = abs_if_negative(z);
    z = C::add(C::add(z, p.pos[2]), k(5.0));
    std::vector<Spawn> spawns{{0, DEF_CANDLE3, {p.pos[0], p.pos[1], z}, vu.first}};
    const bool stopped = undrawn_s8(p);
    return {std::move(spawns), stopped};
}

Tick lantern_run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table) {
    set_radius(p, flicker_radius(p, sim_frame, table, 160));
    if (p.level_name.find("mine") != std::string::npos)
        p.level_radius = 0;
    const bool stopped = undrawn_s8(p);
    return {{}, stopped};
}

Tick lamp_run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table) {
    set_radius(p, flicker_radius(p, sim_frame, table, 216));
    std::vector<Spawn> spawns;
    for (int n = 0; n < 2; ++n) {
        const auto w = p.rng.in_unit_sphere();
        const Raw inv = C::div(C::ONE, C::sqrt(C::add(C::add(C::mul(w[0], w[0]), C::mul(w[1], w[1])),
                                                      C::mul(w[2], w[2]))));
        spawns.push_back({0, DEF_LAMP, p.pos,
                          {C::mul(C::mul(w[0], inv), K_LAMP), C::mul(C::mul(w[1], inv), K_LAMP),
                           C::mul(C::mul(w[2], inv), K_LAMP)}});
    }
    const bool stopped = undrawn_s8(p);
    return {std::move(spawns), stopped};
}

Tick run(FireProp &p, std::int64_t sim_frame, const NoiseTable &table) {
    switch (p.cls) {
    case FireClass::Torch: return torch_run(p, sim_frame, table);
    case FireClass::Candle: return candle_run(p, sim_frame, &table);
    case FireClass::Candle3: return candle3_run(p, sim_frame, &table);
    case FireClass::Lantern: return lantern_run(p, sim_frame, table);
    case FireClass::Lamp: return lamp_run(p, sim_frame, table);
    }
    return {};
}

}  // namespace champions::rta::fx::props
