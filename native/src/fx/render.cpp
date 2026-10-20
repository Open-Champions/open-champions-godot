// Port of tools/ref_fx_render.py (research commit 19f80a6); see fx_render.hpp.
// Every function names the original address it mirrors, as the reference does.

#include "champions/fx_render.hpp"

#include <algorithm>
#include <functional>
#include <string>

namespace champions::rta::fx::render {

namespace {

constexpr Raw SIGN = 0x80000000u;
constexpr Raw M32 = 0xFFFFFFFFu;
constexpr std::uint64_t M64 = ~std::uint64_t{0};

std::uint32_t u32(std::int64_t v) { return static_cast<std::uint32_t>(v); }
std::int32_t s16(std::int64_t v) { return core::s16(static_cast<std::uint32_t>(v)); }

// Python's floor division (the reference's `//`).
std::int64_t floordiv(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

} // namespace

// ---------------------------------------------------------------------------------------
// Vu: the vector unit's arithmetic (class Vu of the reference)
// ---------------------------------------------------------------------------------------

Raw Vu::vin(Raw u) noexcept {
    const Raw e = u & 0x7F800000u;
    if (e == 0x7F800000u)
        return (u & SIGN) | BIGGEST;
    if (e == 0)
        return u & SIGN;
    return u;
}

Raw Vu::vout(Raw u) noexcept { return vin(u); }    // the same two rules on the result

Raw Vu::add(Raw a, Raw b) const { return vout(f.add(vin(a), vin(b))); }
Raw Vu::sub(Raw a, Raw b) const { return vout(f.add(vin(a), vin(b) ^ SIGN)); }
Raw Vu::mul(Raw a, Raw b) const { return vout(f.mul(vin(a), vin(b))); }
Raw Vu::madd(Raw acc, Raw a, Raw b) const { return add(acc, mul(a, b)); }

Raw Vu::div(Raw a, Raw b) const {
    a = vin(a);
    b = vin(b);
    if ((b & 0x7FFFFFFFu) == 0)
        return ((a ^ b) & SIGN) | BIGGEST;
    return vout(f.div(a, b));
}

Raw Vu::rsqrt(Raw a, Raw b) const {
    a = vin(a);
    b = vin(b);
    if ((b & 0x7FFFFFFFu) == 0)
        return (a & SIGN) | BIGGEST;
    return vout(f.div(a, vout(f.sqrt(b & 0x7FFFFFFFu))));
}

Raw Vu::itof(std::uint32_t word, unsigned shift) const {
    Raw r = vout(f.from_int(word));
    if (shift)                                       // 1 / 2^shift, an exact pattern
        r = mul(r, static_cast<Raw>(127u - shift) << 23);
    return r;
}

std::uint32_t Vu::ftoi(Raw a, unsigned shift) const {
    if (shift)
        a = mul(vin(a), static_cast<Raw>(127u + shift) << 23);
    return static_cast<std::uint32_t>(core::Fpu::to_int(vin(a)));
}

std::int64_t Vu::key(Raw u) noexcept {
    return (u & SIGN) ? -static_cast<std::int64_t>(u & 0x7FFFFFFFu) : static_cast<std::int64_t>(u);
}
Raw Vu::vmax(Raw a, Raw b) noexcept { return key(a) > key(b) ? a : b; }
Raw Vu::vmin(Raw a, Raw b) noexcept { return key(a) < key(b) ? a : b; }

std::uint32_t Vu::clip(const Lanes &v, Raw w) const {
    w = vin(w) & 0x7FFFFFFFu;
    std::uint32_t c = 0;
    const std::uint32_t bits[3] = {1, 4, 16};
    for (int k = 0; k < 3; ++k) {
        const Raw x = vin(v[k]);
        if (core::Fpu::compare(x, w) > 0)
            c |= bits[k];
        if (core::Fpu::compare(x, w | SIGN) < 0)
            c |= bits[k] << 1;
    }
    return c;
}

Lanes Vu::transform(const Columns &m, Raw x, Raw y, Raw z, bool w_last) const {
    Lanes acc{}, out{};
    for (int k = 0; k < 4; ++k)
        acc[k] = mul(m[0][k], x);
    for (int k = 0; k < 4; ++k)
        acc[k] = madd(acc[k], m[1][k], y);
    if (w_last) {
        for (int k = 0; k < 4; ++k)
            acc[k] = madd(acc[k], m[2][k], z);
        for (int k = 0; k < 4; ++k)
            out[k] = madd(acc[k], m[3][k], ONE);
        return out;
    }
    for (int k = 0; k < 4; ++k)
        acc[k] = madd(acc[k], m[3][k], ONE);
    for (int k = 0; k < 4; ++k)
        out[k] = madd(acc[k], m[2][k], z);
    return out;
}

GsVertex gs_xyzf(const Lanes &l) {
    GsVertex v;
    v.x = l[0] & 0xFFFFu;
    v.y = l[1] & 0xFFFFu;
    v.z = (l[2] >> 4) & 0xFFFFFFu;
    v.f = (l[3] >> 4) & 0xFFu;
    v.adc = (l[3] >> 15) & 1u;
    return v;
}

std::array<std::uint8_t, 4> gs_rgba(const Lanes &l) {
    return {static_cast<std::uint8_t>(l[0]), static_cast<std::uint8_t>(l[1]),
            static_cast<std::uint8_t>(l[2]), static_cast<std::uint8_t>(l[3])};
}

namespace {
GsVertex vertex(const Lanes &lanes, const std::array<std::uint8_t, 4> &rgba, bool has_uv,
                std::uint32_t u, std::uint32_t v, Raw clip_w) {
    GsVertex out = gs_xyzf(lanes);
    out.rgba = rgba;
    out.has_uv = has_uv;
    out.u = has_uv ? u : 0;
    out.v = has_uv ? v : 0;
    out.clip_w = clip_w;
    return out;
}
} // namespace

// ---------------------------------------------------------------------------------------
// The particle packet: matrices and constants (EE side, particle_draw 0x194888)
// ---------------------------------------------------------------------------------------

namespace {
// mat4_setProjection(0x3f7fffff, 0x3eeeeeee, g_projScale, 1.0, 4095.94, 18, 1800)
constexpr Raw PROJ_SX = 0x3F7FFFFFu, PROJ_SY = 0x3EEEEEEEu, PROJ_ZA = ONE,
              PROJ_ZB = 0x457FFF00u, PROJ_NEAR = 0x41900000u, PROJ_FAR = 0x44E10000u;
constexpr Raw K_COL0 = 0x3AA3D70Au;    // 1/800, element [0][0]
constexpr Raw K_COL1 = 0x3B23D70Au;    // 1/400, element [1][1]
constexpr Raw K_DEPTH = 0x3A000080u;   // elements [2][2] (then - 1.0) and [3][2]
constexpr Lanes HEADER_SCREEN = {0x44480000u, 0x43C80000u, 0x5DFFFF00u, ONE};   // vf6
constexpr Lanes HEADER_OFFSET = {0x49008000u, 0x49008000u, 0x6007FFF8u, 0};     // vf7
constexpr Lanes SPRITE_HALF = {0x40000000u, ONE, 0, 0};                          // vf9
} // namespace

Mat4 particle_projection(Raw proj_scale) {
    Mat4 m = core::mat4_set_projection(PROJ_SX, PROJ_SY, proj_scale, PROJ_ZA, PROJ_ZB,
                                       PROJ_NEAR, PROJ_FAR);
    m[0] = core::mul(m[0], K_COL0);
    m[5] = core::mul(m[5], K_COL1);
    m[10] = core::sub(core::mul(m[10], K_DEPTH), ONE);
    m[14] = core::mul(m[14], K_DEPTH);
    return m;
}

Mat4 particle_matrix(const Mat34 &view, Raw proj_scale) {
    return core::mat4_mul_mat34(view, particle_projection(proj_scale));
}

Mat4 group_matrix(const Mat34 &group, const Mat34 &view, Raw proj_scale) {
    return core::mat4_mul_mat34(core::mat34_mul(group, view), particle_projection(proj_scale));
}

Columns columns(const Mat4 &m) {
    Columns c{};
    for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 4; ++k)
            c[r][k] = m[4 * r + k];
    return c;
}

ParticleHeader particle_header(const Mat34 &view, Raw proj_scale, std::int32_t clock) {
    ParticleHeader h;
    const Raw clock_f = core::from_int(u32(s16(clock)));
    h.m = columns(particle_matrix(view, proj_scale));
    h.vf5 = {0x20, TWO23, TWO23_255, clock_f};
    h.vf6 = HEADER_SCREEN;
    h.vf7 = HEADER_OFFSET;
    h.vf9 = SPRITE_HALF;
    return h;
}

// ---------------------------------------------------------------------------------------
// VU1: one particle
// ---------------------------------------------------------------------------------------

namespace {
void cell_lanes(const ParticleCell &cell, Lanes &q0, Lanes &q1) {
    for (int k = 0; k < 4; ++k) {
        q0[k] = u32(cell[k]);
        q1[k] = u32(cell[4 + k]);
    }
}

Lanes colour(const Vu &vu, const TypeRecord &rec, Raw age) {
    Lanes out{};
    for (int k = 0; k < 4; ++k) {
        const Raw c = vu.add(vu.mul(rec[8 + k], ONE), vu.mul(rec[12 + k], age));
        out[k] = Vu::vmin(Vu::vmax(c, TWO23), TWO23_255);
    }
    return out;
}
} // namespace

std::array<std::uint8_t, 4> colour_bytes(const TypeRecord &rec, Raw age, const Vu &vu) {
    return gs_rgba(colour(vu, rec, age));
}

std::array<GsVertex, 2> sprite_vertices(const ParticleCell &cell, const TypeRecord &rec,
                                        const ParticleHeader &h, const Vu &vu,
                                        std::array<Raw, 2> stale_zw) {
    Lanes q0, q1;
    cell_lanes(cell, q0, q1);
    Lanes pos{}, vel{};
    for (int k = 0; k < 4; ++k)
        pos[k] = vu.itof(q0[k]);
    for (int k = 0; k < 4; ++k)
        vel[k] = vu.itof(q1[k], 12);
    const Raw age = vu.sub(h.vf5[3], pos[3]);
    for (int k = 0; k < 3; ++k)
        vel[k] = vu.mul(vel[k], age);
    const Raw g = rec[2], rate = rec[3], size0 = rec[7];
    const Raw gz = vu.mul(vu.mul(g, age), age);
    Raw a[3];
    for (int k = 0; k < 3; ++k)
        a[k] = vu.add(pos[k], vel[k]);
    const Raw z = vu.add(gz, a[2]);
    const Raw size = vu.add(vu.mul(rate, age), size0);
    const Lanes clip = vu.transform(h.m, a[0], a[1], z, false);
    const std::uint32_t culled = vu.clip(clip, clip[3]);
    const Raw q = vu.div(ONE, clip[3]);
    Lanes t{}, c{}, d{};
    for (int k = 0; k < 3; ++k)
        t[k] = vu.mul(clip[k], h.vf6[k]);
    t[3] = size;
    for (int k = 0; k < 4; ++k)
        c[k] = vu.mul(t[k], q);
    for (int k = 0; k < 3; ++k)
        c[k] = vu.add(c[k], h.vf7[k]);
    for (int k = 0; k < 4; ++k)
        d[k] = vu.mul(h.vf9[k], c[3]);
    const Lanes v1 = {vu.sub(c[0], d[0]), vu.sub(c[1], d[1]), stale_zw[0], stale_zw[1]};
    Lanes v2{};
    for (int k = 0; k < 3; ++k)
        v2[k] = vu.add(c[k], d[k]);
    v2[3] = culled ? M32 : 0;
    const auto rgba = gs_rgba(colour(vu, rec, age));
    return {vertex(v1, rgba, true, rec[0] & 0x3FFF, rec[1] & 0x3FFF, clip[3]),
            vertex(v2, rgba, true, rec[4] & 0x3FFF, rec[5] & 0x3FFF, clip[3])};
}

std::array<GsVertex, 2> line_vertices(const ParticleCell &cell, const TypeRecord &rec,
                                      const ParticleHeader &h, const Vu &vu, WaitQ waitq) {
    Lanes q0, q1;
    cell_lanes(cell, q0, q1);
    Lanes pos{}, vel{};
    for (int k = 0; k < 4; ++k)
        pos[k] = vu.itof(q0[k]);
    for (int k = 0; k < 4; ++k)
        vel[k] = vu.itof(q1[k], 12);
    vel[3] = rec[0];                                // MR32.w: the line gravity (+0x10)
    const Raw age = vu.sub(h.vf5[3], pos[3]);
    const Raw lag_age = vu.sub(age, rec[3]);
    auto at = [&](Raw t) {
        Lanes p{};
        for (int k = 0; k < 4; ++k)
            p[k] = vu.mul(vel[k], t);
        p[3] = vu.mul(p[3], t);
        p[2] = vu.add(p[2], p[3]);
        for (int k = 0; k < 4; ++k)
            p[k] = vu.add(p[k], pos[k]);
        return p;
    };
    const Lanes tail = at(lag_age), head = at(age);
    const Lanes ch = vu.transform(h.m, head[0], head[1], head[2]);
    const std::uint32_t culled = vu.clip(ch, ch[3]);
    const Raw qh = vu.div(ONE, ch[3]);
    Lanes hv{};
    for (int k = 0; k < 3; ++k)
        hv[k] = vu.mul(ch[k], h.vf6[k]);
    hv[3] = ch[3];
    for (int k = 0; k < 4; ++k)
        hv[k] = vu.mul(hv[k], qh);
    for (int k = 0; k < 3; ++k)
        hv[k] = vu.add(hv[k], h.vf7[k]);
    const Lanes ct = vu.transform(h.m, tail[0], tail[1], tail[2]);
    Lanes tv{};
    for (int k = 0; k < 3; ++k)
        tv[k] = vu.mul(ct[k], h.vf6[k]);
    tv[3] = ct[3];
    // 13.6: 0x0d2 MULq | WAITQ - the tail's own 1/w, or the head's still in place.
    const Raw qt = waitq == WaitQ::WaitFirst ? vu.div(ONE, ct[3]) : qh;
    for (int k = 0; k < 4; ++k)
        tv[k] = vu.mul(tv[k], qt);
    for (int k = 0; k < 3; ++k)
        tv[k] = vu.add(tv[k], h.vf7[k]);
    tv[3] = culled ? 0xFFFFu : 0;                   // the head's clip test, on the tail
    const Lanes col = colour(vu, rec, age);
    const auto rgba = gs_rgba(col);
    auto tail_rgba = rgba;
    tail_rgba[3] = static_cast<std::uint8_t>(vu.sub(col[3], col[3]));
    return {vertex(hv, rgba, false, 0, 0, ch[3]), vertex(tv, tail_rgba, false, 0, 0, ct[3])};
}

// ---------------------------------------------------------------------------------------
// Which cells a list sends (particle_emitLists 0x194450)
// ---------------------------------------------------------------------------------------

std::vector<ListBlock> list_blocks(const std::vector<std::uint32_t> &pages, std::uint32_t head,
                                   std::uint32_t tail) {
    std::vector<ListBlock> out;
    const std::size_t last = pages.size() - 1;
    for (std::size_t n = 0; n < pages.size(); ++n) {
        const std::uint32_t first = n == 0 ? (head & ~0x1Fu) : 0;
        const std::uint32_t end = n == last ? ((tail + 0x1E) & ~0x1Fu) : 0x100;
        for (std::uint32_t blk = first >> 5; blk < (end >> 5); ++blk) {
            ListBlock b;
            b.page = pages[n];
            b.block = blk;
            for (std::uint32_t c = 1; c < 31; ++c) {
                const std::uint32_t cell = blk * 32 + c;
                b.cells[c - 1] = (n == last && cell >= tail) ? -1 : static_cast<std::int32_t>(cell);
            }
            out.push_back(b);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------
// Trails: trail_drawAll 0x193ad0 (EE side) and VU1 entries 0x28, 0x26, 0x79e
// ---------------------------------------------------------------------------------------

namespace {
constexpr Raw UV_BASE = 0x49000000u;    // 2^19: u = 2^19 + age * 128 / lifetime
constexpr std::uint32_t TRAIL_CHUNK = 32;
} // namespace

TrailHeader trail_header(const Trail &t, const RowTable &rows) {
    if (t.row < 0 || t.row > 8)
        throw Refusal("trail_header: texture row " + std::to_string(t.row) +
                      " is outside the documented table (rows 0..8)");
    const bool paired = (t.flags & 2) != 0;
    const Qword tag = {0x801E, paired ? 0x50AE4000u : 0x40AA4000u, paired ? 0x43431u : 0x4343u, 0};
    const Raw v0 = rows[static_cast<std::size_t>(t.row)];
    const Raw v1 = rows[static_cast<std::size_t>(t.row) + 1];
    const Raw vrange = core::sub(core::sub(v1, v0), ONE);
    const std::uint32_t alpha = (t.flags & 8) ? 0x09 : (t.flags & 4) ? 0x48 : 0x44;
    const Qword q4 = {UV_BASE, core::add(v0, 0x49000008u),
                      core::div(0x43000000u, core::from_int(u32(t.lifetime))),
                      core::from_int(u32(std::int32_t{t.width4} * 40))};
    return {tag,
            Qword{1, 0x20000000u, 0xE1, vrange},
            Qword{t.rgba[0], t.rgba[1], t.rgba[2], t.rgba[3]},
            Qword{alpha, 0, 0x42, 0},
            q4,
            Qword{0, 0, static_cast<std::uint32_t>((t.oldest & 1) + 1), core::from_int(u32(t.clock))}};
}

std::vector<TrailChunk> trail_chunks(const Trail &t) {
    std::uint32_t start = t.oldest & 0xFEu;
    std::uint32_t left = (static_cast<std::uint32_t>(t.newest) - start) & 0x7Fu;
    std::vector<TrailChunk> out;
    while (left > 0) {
        const std::uint32_t n = std::min({left, TRAIL_CHUNK, 0x80u - start});
        out.push_back({start, n});
        left -= n;
        start = (start + n) & 0x7Fu;
    }
    return out;
}

std::vector<GsVertex> trail_vertices(const Trail &t, const Columns &m, const Lanes &vf6,
                                     const Lanes &vf7, const Vu &vu, WaitQ waitq,
                                     const RowTable &rows) {
    const TrailHeader hdr = trail_header(t, rows);
    const Qword &q4 = hdr[4];
    const Raw clock_f = hdr[5][3];
    const bool paired = (t.flags & 2) != 0;
    std::vector<GsVertex> out;
    std::uint32_t vi10 = (0x8000u + hdr[5][1]) & 0xFFFFu;
    Lanes prev{0, 0, 0, 0};
    const std::array<std::uint8_t, 4> rgba = {static_cast<std::uint8_t>(hdr[2][0]),
                                              static_cast<std::uint8_t>(hdr[2][1]),
                                              static_cast<std::uint8_t>(hdr[2][2]),
                                              static_cast<std::uint8_t>(hdr[2][3])};
    std::vector<GsVertex> saved;
    bool first = true;
    auto mfir = [](std::uint32_t v) -> Raw { return (v & 0x8000u) ? (v | 0xFFFF0000u) : v; };
    auto proj = [&](const TrailPoint &p) {
        Raw c[3];
        for (int k = 0; k < 3; ++k)                  // half units: /16 then * 8
            c[k] = vu.mul(vu.itof(u32(p[k]), 4), 0x41000000u);
        return vu.transform(m, c[0], c[1], c[2]);
    };
    for (const TrailChunk &ch : trail_chunks(t)) {
        std::vector<TrailPoint> pts;
        for (std::uint32_t k = 0; k < ch.count; ++k)
            pts.push_back(t.points[(ch.start + k) & 0x7F]);
        if (!first)                                  // the strip goes on: last two again
            out.insert(out.end(), saved.begin(), saved.end());
        std::vector<GsVertex> chunk;
        const Raw v0 = q4[1] & 0x3FFF;
        const Raw v1 = vu.add(q4[1], hdr[1][3]) & 0x3FFF;
        if (!paired) {                               // program 0x26
            for (const TrailPoint &p : pts) {
                const Lanes clip = proj(p);
                const std::uint32_t culled = vu.clip(clip, clip[3]);
                const Raw q = vu.div(ONE, clip[3]);
                const Raw w = mfir(culled ? 0x8000u : vi10);
                Lanes nd{}, d{}, sq{};
                for (int k = 0; k < 4; ++k)
                    nd[k] = vu.mul(clip[k], q);
                for (int k = 0; k < 4; ++k)
                    d[k] = vu.sub(nd[k], prev[k]);
                prev = nd;
                for (int k = 0; k < 4; ++k)
                    sq[k] = vu.mul(d[k], d[k]);
                const Raw ln = vu.add(sq[0], sq[1]);
                Lanes side = {d[1], vu.sub(0, d[0]), 0, 0};
                for (int k = 0; k < 4; ++k)
                    side[k] = vu.mul(side[k], q);
                // 13.6: 0x1ca MULq | WAITQ - W / sqrt(L), or 1/w again (width lost).
                const Raw q2 = waitq == WaitQ::WaitFirst ? vu.rsqrt(q4[3], ln) : q;
                for (int k = 0; k < 4; ++k)
                    side[k] = vu.mul(side[k], q2);
                side[0] = vu.add(side[0], side[0]);
                vi10 = (vi10 - 1) & 0xFFFFu;
                Raw c[3];
                for (int k = 0; k < 3; ++k)
                    c[k] = vu.add(vu.mul(nd[k], vf6[k]), vf7[k]);
                const Lanes left = {vu.add(c[0], side[0]), vu.add(c[1], side[1]),
                                    vu.add(c[2], side[2]), w};
                const Lanes right = {vu.sub(c[0], side[0]), vu.sub(c[1], side[1]),
                                     vu.sub(c[2], side[2]), w};
                const Raw age = vu.mul(vu.sub(clock_f, vu.itof(u32(p[3]))), q4[2]);
                const Raw u = vu.add(q4[0], age) & 0x3FFF;
                chunk.push_back(vertex(left, rgba, true, u, v0, clip[3]));
                chunk.push_back(vertex(right, rgba, true, u, v1, clip[3]));
            }
        } else {                                     // program 0x79e
            std::uint32_t flags = 0;
            for (std::size_t k = 0; k + 1 < pts.size(); k += 2) {
                const TrailPoint &a = pts[k], &b = pts[k + 1];
                const Lanes ca = proj(a), cb = proj(b);
                flags = ((flags << 6) | vu.clip(ca, ca[3])) & 0xFFFFFFu;
                flags = ((flags << 6) | vu.clip(cb, cb[3])) & 0xFFFFFFu;
                const Raw qa = vu.div(ONE, ca[3]), qb = vu.div(ONE, cb[3]);
                const Raw w = mfir(flags ? 0x8000u : vi10);
                Lanes pa{}, pb{};
                for (int j = 0; j < 3; ++j) {
                    pa[j] = vu.add(vu.mul(vu.mul(ca[j], qa), vf6[j]), vf7[j]);
                    pb[j] = vu.add(vu.mul(vu.mul(cb[j], qb), vf6[j]), vf7[j]);
                }
                pa[3] = pb[3] = w;
                vi10 = (vi10 - 1) & 0xFFFFu;
                const Raw age = vu.mul(vu.sub(clock_f, vu.itof(u32(a[3]))), q4[2]);
                const Raw u = vu.add(q4[0], age) & 0x3FFF;
                auto col = rgba;
                col[3] = static_cast<std::uint8_t>(b[3] & 0xFF);
                chunk.push_back(vertex(pa, col, true, u, v0, ca[3]));
                chunk.push_back(vertex(pb, col, true, u, v1, cb[3]));
            }
            if (pts.size() & 1)
                throw Refusal("trail_vertices: a paired trail chunk with an odd number of points");
        }
        out.insert(out.end(), chunk.begin(), chunk.end());
        saved.assign(chunk.end() - std::min<std::size_t>(2, chunk.size()), chunk.end());
        first = false;
    }
    return out;
}

// ---------------------------------------------------------------------------------------
// Beams: nodes 0x192a40, drift 0x192e80, points 0x192c50 (EE side)
// ---------------------------------------------------------------------------------------

namespace {
constexpr Raw K_DRIFT = 0x3DCCCCCCu;           // 0.099999994
constexpr std::uint32_t BEAM_SEED_FRAME = 0x229, BEAM_SEED_INDEX = 0x3F3;

void check_depth(std::int32_t depth, const char *what) {
    if (depth < 0)
        throw Refusal(std::string(what) + ": negative depth (the recursion would not end)");
}
} // namespace

std::array<std::uint32_t, 7> BeamNode::words() const {
    return {off[0], off[1], off[2], drift[0], drift[1], drift[2], count};
}

std::vector<BeamNode> beam_build_nodes(Raw jitter, std::int32_t depth, core::Lcg &cosmetic,
                                       core::LibcRand &libc) {
    check_depth(depth, "beam_build_nodes");
    std::vector<BeamNode> out;
    std::function<void(Raw, std::int32_t)> node = [&](Raw j, std::int32_t d) {
        Raw s[6];
        for (Raw &x : s)
            x = cosmetic.signed_unit();
        BeamNode n;
        n.off = {core::mul(s[0], j), core::mul(s[1], j), core::mul(s[2], j)};
        // the z drift subtracts offset.y, as the code does (13.5)
        n.drift = {core::mul(core::sub(core::mul(s[3], j), n.off[0]), K_DRIFT),
                   core::mul(core::sub(core::mul(s[4], j), n.off[1]), K_DRIFT),
                   core::mul(core::sub(core::mul(s[5], j), n.off[1]), K_DRIFT)};
        n.count = libc.rand() & 7;
        out.push_back(n);
        if (d != 0) {
            node(core::mul(j, core::HALF), d - 1);
            node(core::mul(j, core::HALF), d - 1);
        }
    };
    node(jitter, depth);
    return out;
}

void beam_drift_nodes(std::vector<BeamNode> &nodes, Raw jitter, std::int32_t depth,
                      core::Lcg &cosmetic) {
    check_depth(depth, "beam_drift_nodes");
    std::size_t it = 0;
    std::function<void(Raw, std::int32_t)> node = [&](Raw j, std::int32_t d) {
        if (it >= nodes.size())
            throw Refusal("beam_drift_nodes: fewer nodes than the depth needs");
        BeamNode &n = nodes[it++];
        const std::int32_t old = core::s32(n.count);
        for (int k = 0; k < 3; ++k)
            n.off[k] = core::add(n.off[k], n.drift[k]);
        n.count = static_cast<std::uint32_t>(old - 1);
        if (old < 1) {
            Raw s[3];
            for (Raw &x : s)
                x = cosmetic.signed_unit();
            for (int k = 0; k < 3; ++k)
                n.drift[k] = core::mul(core::sub(core::mul(s[k], j), n.off[k]), K_DRIFT);
            cosmetic.draw();
            n.count = (cosmetic.state >> 21) & 7;
        }
        if (d != 0) {
            node(core::mul(j, core::HALF), d - 1);
            node(core::mul(j, core::HALF), d - 1);
        }
    };
    node(jitter, depth);
}

std::vector<Vec3> beam_points(const std::vector<BeamNode> &nodes, const Vec3 &a, const Vec3 &b,
                              std::int32_t depth, Raw scale, core::Lcg &cosmetic, Raw amplitude) {
    check_depth(depth, "beam_points");
    std::size_t it = 0;
    std::vector<Vec3> out;
    std::function<void(Raw, const Vec3 &, const Vec3 &, std::int32_t)> seg =
        [&](Raw amp, const Vec3 &p, const Vec3 &q, std::int32_t d) {
            Raw s[3];
            for (Raw &x : s)
                x = cosmetic.signed_unit();
            if (it >= nodes.size())
                throw Refusal("beam_points: fewer nodes than the depth needs");
            const BeamNode &n = nodes[it++];
            Vec3 mid{};
            for (int k = 0; k < 3; ++k)
                mid[k] = core::add(core::add(core::mul(core::add(p[k], q[k]), core::HALF), n.off[k]),
                                   core::mul(core::mul(s[k], amp), scale));
            if (d != 0)
                seg(core::mul(amp, core::HALF), p, mid, d - 1);
            out.push_back(mid);
            if (d != 0)
                seg(core::mul(amp, core::HALF), mid, q, d - 1);
        };
    seg(amplitude, a, b, depth);
    return out;
}

std::uint32_t beam_frame_seed(std::uint32_t sim_frame, std::uint32_t index) {
    return sim_frame * BEAM_SEED_FRAME + index * BEAM_SEED_INDEX;
}

TrailPoint trail_point(const Vec3 &pos, std::int32_t stamp) {
    TrailPoint p{};
    for (int k = 0; k < 3; ++k)
        p[k] = static_cast<std::int16_t>(s16(core::to_int(core::add(pos[k], pos[k]))));
    p[3] = static_cast<std::int16_t>(s16(stamp));
    return p;
}

BeamTrailPoints beam_trail_points(const std::vector<Vec3> &points) {
    BeamTrailPoints out;
    for (std::size_t k = 0; k < points.size(); ++k)
        out.ring.push_back(trail_point(points[k], static_cast<std::int32_t>(k + 1)));
    out.clock = static_cast<std::int32_t>(points.size());
    return out;
}

// ---------------------------------------------------------------------------------------
// Glow sprites: glow_add 0x18fc78, glow_draw 0x1907d0 (EE side)
// ---------------------------------------------------------------------------------------

namespace {
constexpr Raw K_SIXTEEN = 0x41800000u;
constexpr Raw K_DEPTH_MIN = 0x41A00000u, K_X_LIMIT = 0x442F0000u, K_Y_LIMIT = 0x43960000u;

AdWrite ad(std::int64_t value, std::uint32_t reg) {
    return {reg, static_cast<std::uint64_t>(value) & M64};
}
// X, Y, Z or-ed into one 64-bit value from signed results: a negative X spills its sign
// into Y and Z, as the code does.
std::int64_t xyz(std::int64_t x, std::int64_t y, std::int64_t z) {
    const auto ux = static_cast<std::uint64_t>(x), uy = static_cast<std::uint64_t>(y),
               uz = static_cast<std::uint64_t>(z);
    return static_cast<std::int64_t>(ux | (uy << 16) | (uz << 32));
}
} // namespace

std::optional<GlowScreen> glow_project(const Glow &g, const Mat34 &cam, const Mat4 &viewproj) {
    const Raw fwd[3] = {cam[2], cam[5], cam[8]};
    Vec3 p{};
    for (int k = 0; k < 3; ++k)
        p[k] = core::sub(g.pos[k], core::mul(fwd[k], g.bias));
    const core::TransformPoint tp = core::mat4_transform_point(viewproj, p);
    const Vec3 &q = tp.point;
    const Raw depth = core::add(tp.w, g.bias);
    if (core::lt(depth, K_DEPTH_MIN))
        return std::nullopt;
    if (core::lt(core::mul(depth, K_X_LIMIT), core::fabs(q[0])) ||
        core::lt(core::mul(depth, K_Y_LIMIT), core::fabs(q[1])))
        return std::nullopt;
    const Raw inv = core::div(ONE, depth);
    const Raw invz = core::div(ONE, core::sub(depth, g.bias));
    const Raw x = core::mul(q[0], inv), y = core::mul(q[1], inv), z = core::mul(q[2], invz);
    GlowScreen s;
    s.x = std::int64_t{core::to_int(core::mul(x, K_SIXTEEN))} + 0x8000;
    s.y = std::int64_t{core::to_int(core::mul(y, K_SIXTEEN))} + 0x8000;
    s.z = core::to_int(core::mul(z, K_SIXTEEN));
    s.h = core::to_int(core::mul(core::mul(g.size, K_SIXTEEN), inv));
    s.depth = depth;
    return s;
}

std::vector<AdWrite> glow_vertices(const Glow &g, std::int64_t X, std::int64_t Y, std::int64_t Z,
                                   std::int64_t H, std::int32_t tex_w, std::int32_t tex_h) {
    const std::uint32_t rgba = std::uint32_t{g.rgba[0]} | std::uint32_t{g.rgba[1]} << 8 |
                               std::uint32_t{g.rgba[2]} << 16 | std::uint32_t{g.rgba[3]} << 24;
    std::vector<AdWrite> out = {ad(rgba, REG_RGBAQ)};
    const std::int64_t u1 = static_cast<std::int64_t>(static_cast<std::uint64_t>(std::int64_t{tex_w} - 1) << 4);
    const std::int64_t v1 = static_cast<std::int64_t>(static_cast<std::uint64_t>(std::int64_t{tex_h} - 1) << 20);
    if (!(g.flags & 1)) {
        out.push_back(ad(0x100010, REG_UV));
        out.push_back(ad(xyz(X - 2 * H, Y - H, Z), REG_XYZ2));
        out.push_back(ad(u1 | v1, REG_UV));
        out.push_back(ad(xyz(X + 2 * H, Y + H, Z), REG_XYZ2));
        return out;
    }
    const Raw hf = core::from_int(u32(H));
    // glow_draw's rotated quad: math_cos16 / math_sin16 of the record angle.
    const std::int64_t c = core::to_int(core::mul(core::cos16(g.angle), hf));
    const std::int64_t s = core::to_int(core::mul(core::sin16(g.angle), hf));
    out.push_back(ad(0x100010, REG_UV));
    out.push_back(ad(xyz(X - 2 * c, Y - s, Z), REG_XYZ3));
    out.push_back(ad(u1 | 0x100000, REG_UV));
    out.push_back(ad(xyz(X + 2 * s, Y - c, Z), REG_XYZ3));
    out.push_back(ad(v1 | 0x10, REG_UV));
    out.push_back(ad(xyz(X - 2 * s, Y + c, Z), REG_XYZ2));
    out.push_back(ad(u1 | v1, REG_UV));
    out.push_back(ad(xyz(X + 2 * c, Y + s, Z), REG_XYZ2));
    return out;
}

std::vector<GlowPacket> glow_packets(std::vector<Glow> &queue, std::int32_t pass,
                                     const Mat34 &cam, const Mat4 &viewproj,
                                     const TextureDims &dims) {
    std::vector<GlowPacket> out;
    auto in_pass = [&](const Glow &g) {
        return g.tex != 0 && static_cast<std::int32_t>((static_cast<std::uint32_t>(
                                                            std::int32_t{g.flags}) >> 2) & 1) == pass;
    };
    for (;;) {
        const auto first = std::find_if(queue.begin(), queue.end(), in_pass);
        if (first == queue.end())
            return out;
        const std::uint32_t tex = first->tex;
        const std::int16_t flags = first->flags;
        GlowPacket p;
        p.key = pass ? 4 : 3;
        p.texture = tex;
        p.prim = (flags & 1) ? PRIM_GLOW_QUAD : PRIM_GLOW;
        p.alpha1 = (flags & 2) ? 0x48 : 0x44;
        p.events = {ad(0, REG_TEXFLUSH), ad(0, REG_TEX0_1), ad(0x61, REG_TEX1_1),
                    ad(0x71001, REG_TEST_1), ad(static_cast<std::int64_t>(GLOW_DIMX_ON), REG_DIMX),
                    ad(p.alpha1, REG_ALPHA_1)};
        for (auto g = first; g != queue.end(); ++g) {
            if (g->tex != tex || g->flags != flags)
                continue;
            g->tex = 0;                              // consumed, drawn or culled
            const auto sp = glow_project(*g, cam, viewproj);
            if (!sp)
                continue;
            const auto d = dims.find(tex);
            if (d == dims.end())
                throw Refusal("glow_packets: no texture dimensions for a drawn glow's texture");
            const auto ev = glow_vertices(*g, sp->x, sp->y, sp->z, sp->h, d->second.first,
                                          d->second.second);
            p.sprites.push_back({p.events.size(), ev.size(), *sp});
            p.events.insert(p.events.end(), ev.begin(), ev.end());
        }
        p.events.push_back(ad(static_cast<std::int64_t>(GLOW_DIMX_OFF), REG_DIMX));
        out.push_back(std::move(p));
    }
}

// ---------------------------------------------------------------------------------------
// Trail builders (EE side): trail_addPair 0x192650, spline_eval 0x1911e8, trail_setSpline
// 0x1918a0, trail_sweepPairs 0x191ea0, trail_setCircle 0x1920e0
// ---------------------------------------------------------------------------------------

namespace {
constexpr Raw K_NEAR_X = 0x3A83126Eu;     // 0.0009999999: "normal is (1, 0, 0)"
constexpr Raw K_255 = 0x437F0000u;
constexpr std::size_t SPLINE_POINT_LIMIT = std::size_t{1} << 20;   // a resource guard only

const Vec3 &at(const std::vector<Vec3> &pts, std::int64_t i) {
    if (i < 0 || i >= static_cast<std::int64_t>(pts.size()))
        throw Refusal("spline: point " + std::to_string(i) + " is outside the points given");
    return pts[static_cast<std::size_t>(i)];
}

Vec3 tangent(const Vec3 &p, const Vec3 &q, const Vec3 &r) {
    // ((q - p) + (r - q)) * 0.5 per axis
    Vec3 out{};
    for (int k = 0; k < 3; ++k)
        out[k] = core::mul(core::add(core::sub(q[k], p[k]), core::sub(r[k], q[k])), core::HALF);
    return out;
}
} // namespace

std::array<TrailPoint, 2> trail_pair(Raw alpha, const Vec3 &a, const Vec3 &b, std::int32_t clock) {
    return {trail_point(a, clock + 1), trail_point(b, core::to_int(core::mul(alpha, K_255)))};
}

SplineResult spline_eval(Raw t, const std::vector<Vec3> &pts, std::int32_t count, bool want_pos,
                         bool want_tangent) {
    t = core::fmin(core::fmax(t, core::ZERO), ONE);
    const Raw f = core::mul(t, core::from_int(u32(std::int64_t{count} - 1)));
    const std::int64_t i = core::to_int(f);
    const Raw s = core::sub(f, core::from_int(u32(i)));
    Vec3 m0{}, m1{};
    if (!(i < 1))       // ((p[i] - p[i-1]) + (p[i+1] - p[i])) * 0.5
        m0 = tangent(at(pts, i - 1), at(pts, i), at(pts, i + 1));
    if (!(i >= std::int64_t{count} - 2)) {
        // ((p[i+2] - p[i+1]) + (p[i+1] - p[i])) * 0.5, in this order
        const Vec3 &p0 = at(pts, i), &p1 = at(pts, i + 1), &p2 = at(pts, i + 2);
        for (int k = 0; k < 3; ++k)
            m1[k] = core::mul(core::add(core::sub(p2[k], p1[k]), core::sub(p1[k], p0[k])), core::HALF);
    }
    SplineResult r;
    if (want_pos)
        r.pos = core::vec3_hermite(s, at(pts, i), at(pts, i + 1), m0, m1);
    if (want_tangent)
        r.tangent = core::vec3_hermite_derivative(s, at(pts, i), at(pts, i + 1), m0, m1);
    return r;
}

SplineTrail trail_set_spline(Raw start_t, Raw end_t, const std::vector<Vec3> &pts,
                             std::int32_t count, std::int32_t steps) {
    if (count == 1)
        throw Refusal("trail_setSpline: count 1 divides by zero");
    const std::int64_t segs = std::int64_t{count} - 1;
    const std::int64_t per = steps >= 0 ? floordiv(steps, segs) : -floordiv(-std::int64_t{steps}, segs);
    SplineTrail out;
    const Raw perf = core::from_int(u32(per));
    const Raw inv = core::div(ONE, perf);
    for (std::int64_t i = 0; i < segs; ++i) {
        Vec3 m0{}, m1{};
        std::int64_t k0, k1;
        if (i < 1) {
            k0 = core::to_int(core::mul(start_t, perf));
        } else {
            for (int k = 0; k < 3; ++k)
                m0[k] = core::mul(core::add(core::sub(at(pts, i)[k], at(pts, i - 1)[k]),
                                            core::sub(at(pts, i + 1)[k], at(pts, i)[k])), core::HALF);
            k0 = 0;
        }
        if (i < std::int64_t{count} - 2) {
            for (int k = 0; k < 3; ++k)
                m1[k] = core::mul(core::add(core::sub(at(pts, i + 2)[k], at(pts, i + 1)[k]),
                                            core::sub(at(pts, i + 1)[k], at(pts, i)[k])), core::HALF);
            k1 = per;
        } else {
            k1 = core::to_int(core::mul(end_t, perf));
        }
        for (std::int64_t k = k0; k < k1; ++k) {
            if (out.points.size() >= SPLINE_POINT_LIMIT)
                throw Refusal("trail_setSpline: more than 2^20 points");
            out.points.push_back(core::vec3_hermite(core::mul(inv, core::from_int(u32(k))),
                                                    at(pts, i), at(pts, i + 1), m0, m1));
        }
    }
    out.lifetime = static_cast<std::int16_t>(s16(per * segs));
    return out;
}

SweepPairs trail_sweep_pairs(const Vec3 &centre, const Vec3 &axis, Vec3 a, Vec3 b,
                             std::int32_t steps) {
    steps = std::min(steps, 60);
    if (steps == 2)
        throw Refusal("trail_sweepPairs: steps 2 divides by zero");
    const std::int64_t angle = s16(floordiv(0x10000, std::int64_t{steps} - 2));
    const core::Quat q = core::quat_from_axis_angle(axis[0], axis[1], axis[2], u32(angle));
    SweepPairs out;
    for (std::int32_t n = 0; n < steps; ++n) {
        Vec3 p0{}, p1{};
        for (int k = 0; k < 3; ++k) {
            p0[k] = core::add(a[k], centre[k]);
            p1[k] = core::add(b[k], centre[k]);
        }
        out.pairs.emplace_back(p0, p1);
        a = core::quat_rotate_vec(q, a);
        b = core::quat_rotate_vec(q, b);
    }
    out.lifetime = steps;
    return out;
}

std::vector<Vec3> trail_set_circle(Raw radius, const Vec3 &centre, const Vec3 &n,
                                   std::int32_t steps) {
    steps = std::min(steps, 120);
    using core::add;
    using core::mul;
    using core::sub;
    const Raw x1 = sub(n[0], ONE);
    const Raw test = add(add(mul(x1, x1), mul(n[1], n[1])), mul(n[2], n[2]));
    const Vec3 r = core::lt(test, K_NEAR_X) ? Vec3{0, 0, ONE} : Vec3{ONE, 0, 0};
    Vec3 u = {sub(mul(n[1], r[2]), mul(n[2], r[1])), sub(mul(n[2], r[0]), mul(n[0], r[2])),
              sub(mul(n[0], r[1]), mul(n[1], r[0]))};
    Raw inv = core::rsqrt(ONE, add(add(mul(u[0], u[0]), mul(u[1], u[1])), mul(u[2], u[2])));
    for (Raw &x : u)
        x = mul(x, inv);
    Vec3 v = {sub(mul(u[1], n[2]), mul(u[2], n[1])), sub(mul(u[2], n[0]), mul(u[0], n[2])),
              sub(mul(u[0], n[1]), mul(u[1], n[0]))};
    inv = core::rsqrt(ONE, add(add(mul(v[0], v[0]), mul(v[1], v[1])), mul(v[2], v[2])));
    for (Raw &x : v)
        x = mul(mul(x, inv), radius);
    for (Raw &x : u)
        x = mul(x, radius);
    if (steps == 0)
        throw Refusal("trail_setCircle: steps 0 divides by zero");
    const std::int64_t step = floordiv(0xFFFF, steps);
    std::int64_t angle = 0;
    std::vector<Vec3> out;
    for (std::int32_t k = 0; k < steps; ++k) {
        const Raw c = core::cos16(u32(angle)), s = core::sin16(u32(angle));
        Vec3 p{};
        for (int j = 0; j < 3; ++j)
            p[j] = add(add(centre[j], mul(u[j], c)), mul(v[j], s));
        out.push_back(p);
        angle = s16(angle + step);
    }
    return out;
}

// ---------------------------------------------------------------------------------------
// TrailStore (docs 7, 13.4, 13.8)
// ---------------------------------------------------------------------------------------

namespace {
constexpr Raw K_SIXTY = 0x42700000u;      // 60.0: lifetime ticks = trunc(60 * seconds)
constexpr std::size_t RING_ACCEPTS = 126; // entries an empty ring takes before refusing

bool near_full(const Trail &t) {
    return ((t.newest + 1) & 0x7F) == t.oldest || ((t.newest + 2) & 0x7F) == t.oldest;
}

// NATIVE: the next generation value that does not make handle (gen << 8 | index) zero.
std::uint32_t next_generation(std::uint32_t &generation, std::uint32_t index) {
    do
        generation = (generation + 1) & 0xFF;
    while (((generation << 8) | index) == 0);
    return generation;
}
} // namespace

std::uint16_t TrailStore::create(Raw lifetime_seconds, std::int32_t row, std::int32_t width,
                                 std::uint32_t flags, std::uint8_t r, std::uint8_t g,
                                 std::uint8_t b, std::uint8_t a) {
    for (std::uint32_t i = 0; i < COUNT; ++i) {
        Trail &t = trails[i];
        if (t.handle != 0)
            continue;
        t = Trail{};
        t.handle = static_cast<std::uint16_t>((next_generation(generation, i) << 8) | i);
        t.row = static_cast<std::int8_t>(row);
        t.flags = static_cast<std::uint8_t>(flags);
        t.lifetime = static_cast<std::int16_t>(s16(core::to_int(core::mul(K_SIXTY, lifetime_seconds))));
        t.rgba = {r, g, b, a};
        t.width4 = static_cast<std::uint8_t>(static_cast<std::uint32_t>(width) >> 2);
        return t.handle;
    }
    return 0;
}

Trail *TrailStore::find(std::uint32_t handle) {
    const std::uint32_t i = handle & 0xFF;
    if (handle == 0 || i >= COUNT || trails[i].handle != handle)
        return nullptr;
    return &trails[i];
}

const Trail *TrailStore::find(std::uint32_t handle) const {
    return const_cast<TrailStore *>(this)->find(handle);
}

bool TrailStore::add_point(std::uint32_t handle, const Vec3 &pos) {
    Trail *t = find(handle);
    if (!t || near_full(*t))
        return false;
    t->points[t->newest] = trail_point(pos, std::int32_t{t->clock} + 1);
    t->newest = static_cast<std::uint8_t>((t->newest + 1) & 0x7F);
    return true;
}

bool TrailStore::add_pair(Raw alpha, std::uint32_t handle, const Vec3 &a, const Vec3 &b) {
    Trail *t = find(handle);
    if (!t || near_full(*t))
        return false;
    const auto pair = trail_pair(alpha, a, b, t->clock);
    for (const TrailPoint &p : pair) {
        t->points[t->newest] = p;
        t->newest = static_cast<std::uint8_t>((t->newest + 1) & 0x7F);
    }
    return true;
}

void TrailStore::release(std::uint32_t handle) {
    if (Trail *t = find(handle))
        t->handle = 0;
}

void TrailStore::set_color(std::uint32_t handle, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                           std::uint8_t a) {
    if (Trail *t = find(handle))
        t->rgba = {r, g, b, a};
}

void TrailStore::set_length(std::uint32_t handle, std::int32_t value) {
    // value >> 2 stored as a byte: the low byte is the same for either shift.
    if (Trail *t = find(handle))
        t->width4 = static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> 2);
}

void TrailStore::clear(std::uint32_t handle) {
    if (Trail *t = find(handle)) {
        t->clock = 0;
        t->oldest = 0;
        t->newest = 0;
    }
}

void TrailStore::set_spline(Raw start_t, Raw end_t, std::uint32_t handle,
                            const std::vector<Vec3> &pts, std::int32_t count, std::int32_t steps) {
    Trail *t = find(handle);
    if (!t)
        return;
    const SplineTrail s = trail_set_spline(start_t, end_t, pts, count, steps);
    if (s.points.size() > RING_ACCEPTS)
        throw Refusal("trail_setSpline: more points than the ring accepts (not executed)");
    clear(handle);
    t->lifetime = s.lifetime;
    for (const Vec3 &p : s.points) {
        add_point(handle, p);
        t->clock = static_cast<std::int16_t>(t->clock + 1);
    }
}

void TrailStore::sweep_pairs(std::uint32_t handle, const Vec3 &centre, const Vec3 &axis,
                             const Vec3 &a, const Vec3 &b, std::int32_t steps) {
    Trail *t = find(handle);
    if (!t)
        return;
    const SweepPairs s = trail_sweep_pairs(centre, axis, a, b, steps);
    if (2 * s.pairs.size() > RING_ACCEPTS)
        throw Refusal("trail_sweepPairs: more points than the ring accepts (not executed)");
    clear(handle);
    t->lifetime = static_cast<std::int16_t>(s16(s.lifetime));
    for (const auto &p : s.pairs) {
        add_pair(ONE, handle, p.first, p.second);
        t->clock = static_cast<std::int16_t>(t->clock + 1);
    }
}

void TrailStore::set_circle(Raw radius, std::uint32_t handle, const Vec3 &centre,
                            const Vec3 &normal, std::int32_t steps) {
    Trail *t = find(handle);
    if (!t)
        return;
    const std::vector<Vec3> pts = trail_set_circle(radius, centre, normal, steps);
    clear(handle);                              // the lifetime is left as it was
    for (const Vec3 &p : pts) {
        add_point(handle, p);
        t->clock = static_cast<std::int16_t>(t->clock + 1);
    }
}

void TrailStore::update(bool hold) {
    if (hold)                                   // 0x44fc98 set: trail_updateAll not called
        return;
    for (Trail &t : trails) {
        if (t.handle == 0 || (t.flags & 1))     // flag 1: the update does not age it
            continue;
        if (!(t.flags & 0x10)) {
            if (t.clock == 0x7FFF)
                throw Refusal("trail update: the trail clock would pass 0x7fff (not documented)");
            t.clock = static_cast<std::int16_t>(t.clock + 1);
        }
        const unsigned step = (t.flags & 2) ? 2 : 1;
        while (t.oldest != t.newest &&
               std::int32_t{t.clock} - std::int32_t{t.points[t.oldest][3]} >= t.lifetime) {
            if (step == 2 && ((t.oldest + 1) & 0x7F) == t.newest)
                throw Refusal("trail update: a paired trail with a single entry left (not documented)");
            t.oldest = static_cast<std::uint8_t>((t.oldest + step) & 0x7F);
        }
        if (t.oldest == t.newest)               // no point left, or never had one: freed
            t.handle = 0;
    }
}

// ---------------------------------------------------------------------------------------
// BeamStore (docs 7, 13.5)
// ---------------------------------------------------------------------------------------

std::uint32_t BeamStore::create(Raw jitter, Raw scale, const Vec3 &from, const Vec3 &to,
                                std::int32_t width, std::int32_t depth,
                                const std::array<std::uint8_t, 4> &rgba, std::int32_t sim_frame,
                                bool in_sim_tick, core::Lcg &cosmetic, core::LibcRand &libc) {
    if (depth < 0)
        throw Refusal("beam_create: negative depth (the node recursion would not end)");
    std::uint32_t index = BEAM_COUNT;
    for (std::uint32_t i = 0; i < BEAM_COUNT; ++i)
        if (!beams[i].active) {
            index = i;
            break;
        }
    if (index == BEAM_COUNT)
        return 0;
    TrailStore &ts = *trails_;
    // two trails: lifetime 1 s, row 0, flags 5 ("not aged" | additive)
    const std::uint16_t a = ts.create(ONE, 0, width, 5, rgba[0], rgba[1], rgba[2], rgba[3]);
    if (!a)
        return 0;
    const std::uint16_t b = ts.create(ONE, 0, width, 5, rgba[0], rgba[1], rgba[2], rgba[3]);
    if (!b) {
        ts.release(a);
        return 0;
    }
    ts.add_point(a, from);
    ts.add_point(b, from);
    Beam &beam = beams[index];
    beam = Beam{};
    beam.depth = std::min<std::int32_t>(depth, static_cast<std::int32_t>(BEAM_MAX_DEPTH));
    beam.nodes = beam_build_nodes(jitter, beam.depth, cosmetic, libc);
    beam.active = true;
    beam.in_sim_tick = in_sim_tick;
    beam.from = from;
    beam.to = to;
    beam.jitter = jitter;
    beam.scale = scale;
    beam.trail_a = a;
    beam.trail_b = b;
    beam.expiry = static_cast<std::int32_t>(static_cast<std::uint32_t>(sim_frame) + 10u);
    beam.handle = (next_generation(ts.generation, index) << 8) | index;
    return beam.handle;
}

void BeamStore::update(std::uint32_t handle, const Vec3 &from, const Vec3 &to, std::int32_t alpha,
                       std::int32_t width, std::int32_t sim_frame) {
    const std::uint32_t i = handle & 0xFF;
    if (handle == 0 || i >= BEAM_COUNT || !beams[i].active || beams[i].handle != handle)
        return;
    Beam &beam = beams[i];
    beam.from = from;
    beam.to = to;
    for (std::uint16_t th : {beam.trail_a, beam.trail_b}) {
        if (Trail *t = trails_->find(th)) {
            t->rgba[3] = static_cast<std::uint8_t>(alpha);
            if (width >= 0)
                t->width4 = static_cast<std::uint8_t>(static_cast<std::uint32_t>(width) >> 2);
        }
    }
    beam.expiry = static_cast<std::int32_t>(static_cast<std::uint32_t>(sim_frame) + 2u);
}

bool BeamStore::is_alive(std::uint32_t handle) const {
    const std::uint32_t i = handle & 0xFF;
    return handle != 0 && i < BEAM_COUNT && beams[i].active && beams[i].handle == handle;
}

void BeamStore::destroy(std::uint32_t handle) {
    if (!is_alive(handle))
        return;
    Beam &beam = beams[handle & 0xFF];
    trails_->release(beam.trail_a);
    trails_->release(beam.trail_b);
    beam.active = false;
    beam.handle = 0;
}

void BeamStore::draw_step(std::int32_t sim_frame, std::int32_t net_frame, std::uint32_t draw_frame,
                          bool frozen, core::Lcg &cosmetic) {
    for (std::uint32_t i = 0; i < BEAM_COUNT; ++i) {
        Beam &beam = beams[i];
        if (!beam.active)
            continue;
        const std::int32_t now = beam.in_sim_tick ? net_frame : sim_frame;
        if (beam.expiry < now) {
            destroy(beam.handle);
            continue;
        }
        if (!frozen)                            // the live cosmetic seed, per drawn frame
            beam_drift_nodes(beam.nodes, beam.jitter, beam.depth, cosmetic);
        Trail *ta = trails_->find(beam.trail_a), *tb = trails_->find(beam.trail_b);
        for (Trail *t : {ta, tb})
            if (t) {
                t->oldest = 0;
                t->newest = 0;
                t->clock = 0;
            }
        const std::uint32_t saved = cosmetic.state;
        cosmetic.state = beam_frame_seed(static_cast<std::uint32_t>(sim_frame), i);
        const std::vector<Vec3> pts =
            beam_points(beam.nodes, beam.from, beam.to, beam.depth, beam.scale, cosmetic, ONE);
        cosmetic.state = saved;
        const std::uint16_t target = (draw_frame & 1) == 0 ? beam.trail_b : beam.trail_a;
        if (Trail *t = trails_->find(target))
            for (const Vec3 &p : pts) {
                trails_->add_point(target, p);
                t->clock = static_cast<std::int16_t>(t->clock + 1);
            }
    }
}

bool GlowQueue::add(const Glow &g) {
    if (records.size() >= GLOW_MAX)
        return false;                           // the 0x401st is dropped
    records.push_back(g);
    return true;
}

// ---------------------------------------------------------------------------------------
// The particle packet, the trail packet, the sorted list, the frame
// ---------------------------------------------------------------------------------------

void draw_haze() {
    throw Refusal(
        "particle_drawHaze: whether only kind 3 reaches this drawer is not established, "
        "and which screen axis receives the sine word is not established");
}

ParticlePacket particle_packet(const ParticleFrame &frame, const Mat34 &view, Raw proj_scale,
                               std::array<Raw, 2> stale_zw, WaitQ waitq, const Vu &vu) {
    ParticlePacket out;
    const ParticleHeader header = particle_header(view, proj_scale, frame.clock);
    std::vector<std::pair<const ParticleListSet *, ParticleHeader>> sets;
    sets.emplace_back(&frame.global, header);
    for (const ParticleListSet &g : frame.groups) {
        ParticleHeader h = header;
        h.m = columns(group_matrix(g.matrix, view, proj_scale));
        sets.emplace_back(&g, h);
    }
    const ParticleCell zero{};
    for (std::size_t s = 0; s < sets.size(); ++s) {
        const ParticleListSet &set = *sets[s].first;
        const ParticleHeader &h = sets[s].second;
        std::uint32_t alpha1 = 0;
        for (std::uint32_t kind = 0; kind < 3; ++kind) {
            if (kind < 2) {
                alpha1 = kind == 0 ? 0x44 : 0x48;
                out.alpha_writes.push_back(alpha1);
            }
            for (std::uint32_t life = 0; life < 4; ++life) {
                const ParticleList &lst = set.lists[kind * 4 + life];
                if (lst.pages.empty())
                    continue;
                for (const ListBlock &b : list_blocks(lst.pages, lst.head, lst.tail)) {
                    if (b.page >= frame.pages.size())
                        throw Refusal("particle_packet: page index outside the frame's pages");
                    for (std::int32_t c : b.cells) {
                        const ParticleCell &cell = c < 0 ? zero : frame.pages[b.page][static_cast<std::size_t>(c)];
                        const std::uint32_t slot = (static_cast<std::uint16_t>(cell[7]) >> 2) & 31;
                        ParticleDraw d;
                        d.set = static_cast<std::uint32_t>(s);
                        d.kind = kind;
                        d.life = life;
                        d.alpha1 = alpha1;
                        d.slot = slot;
                        d.zero_filled = c < 0;
                        if (kind == 2) {
                            d.prim = PRIM_LINE;
                            d.v = line_vertices(cell, frame.types[slot], h, vu, waitq);
                        } else {
                            d.prim = PRIM_SPRITE;
                            d.v = sprite_vertices(cell, frame.types[slot], h, vu, stale_zw);
                        }
                        out.draws.push_back(d);
                    }
                }
            }
        }
    }
    return out;
}

TrailPacket trail_packet(const TrailStore &store, const ParticleHeader &h, WaitQ waitq,
                         const RowTable &rows, const Vu &vu) {
    TrailPacket out;
    for (std::uint32_t i = 0; i < TrailStore::COUNT; ++i) {
        const Trail &t = store.trails[i];
        if (t.handle == 0 || t.oldest == t.newest)
            continue;
        TrailDraw d;
        d.index = i;
        d.handle = t.handle;
        d.header = trail_header(t, rows);
        d.prim = (t.flags & 2) ? PRIM_TRAIL_PAIRED : PRIM_TRAIL;
        d.alpha1 = d.header[3][0];
        d.vertices = trail_vertices(t, h.m, h.vf6, h.vf7, vu, waitq, rows);
        out.trails.push_back(std::move(d));
    }
    return out;
}

void SortedPackets::add(Packet p) {
    if (p.key < 0 || p.key > 7)
        throw Refusal("SortedPackets: key outside 0..7");
    auto &list = keys_[static_cast<std::size_t>(p.key)];
    for (std::size_t i = list.size(); i-- > 0;)
        if (list[i].texture == p.texture) {
            list.insert(list.begin() + static_cast<std::ptrdiff_t>(i) + 1, std::move(p));
            return;
        }
    list.insert(list.begin(), std::move(p));
}

std::vector<Packet> SortedPackets::gs_order() const {
    std::vector<Packet> out;
    for (const auto &list : keys_)
        out.insert(out.end(), list.begin(), list.end());
    return out;
}

std::vector<Packet> draw_frame(const FrameInput &in, const ParticleFrame &particles,
                               TrailStore &trails, BeamStore *beams, GlowQueue &glows,
                               const TextureDims &glow_dims, core::Lcg &cosmetic) {
    const Vu vu;
    SortedPackets sorted;
    // The global particle header: the matrix, vf6 and vf7 the trail programs use (13.3).
    const ParticleHeader header = particle_header(in.view, in.proj_scale, particles.clock);
    if (beams)
        beams->draw_step(in.sim_frame, in.net_frame, in.draw_frame, in.frozen, cosmetic);
    Packet tp;
    tp.kind = PacketKind::Trails;
    tp.key = 4;
    tp.texture = in.trail_texture;
    tp.trails = trail_packet(trails, header, in.waitq, in.rows, vu);
    sorted.add(std::move(tp));
    for (std::int32_t pass : {1, 0}) {
        if (pass == 0) {
            Packet pp;
            pp.kind = PacketKind::Particles;
            pp.key = 4;
            pp.texture = in.particle_texture;
            pp.particles = particle_packet(particles, in.view, in.proj_scale, in.stale_zw, in.waitq, vu);
            sorted.add(std::move(pp));
        }
        for (GlowPacket &g : glow_packets(glows.records, pass, in.camera, in.viewproj, glow_dims)) {
            Packet gp;
            gp.kind = PacketKind::Glow;
            gp.key = g.key;
            gp.texture = g.texture;
            gp.glow = std::move(g);
            sorted.add(std::move(gp));
        }
    }
    glows.clear();
    return sorted.gs_order();
}

} // namespace champions::rta::fx::render
