// Tests of the native port of ref_fx_render (native/src/fx/render.cpp).
//   (a) every assertion of the research tools/test_ref_fx_render.py, ported;
//   (b) the runtime stores, the sorted packet order and the frame drawer (synthetic);
//   (c) <argv[1]>/fx_render.tsv, written by tools/generate_fx_render_vectors.py from the
//       pinned reference: every row recomputed, any difference fails.
// Synthetic values only.

#include "champions/fx_render.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace core = champions::simulation::rta;
namespace R = champions::rta::fx::render;
using core::Raw;
using R::Vec3;

namespace {

int failures = 0;

void check(bool ok, const std::string &what) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

Raw F(double x) {
    const float f = static_cast<float>(x);
    Raw r;
    std::memcpy(&r, &f, 4);
    return r;
}

double to_float(Raw r) {
    float f;
    std::memcpy(&f, &r, 4);
    return f;
}

const R::Mat34 IDENT_VIEW = {F(1.0), 0, 0, 0, F(1.0), 0, 0, 0, F(1.0), 0, 0, 0};

// ---- (a) the reference's own unit tests ----------------------------------------------------

R::TypeRecord typerec(std::uint32_t u0 = 0x610, std::uint32_t v0 = 0x210, double g = 0.0,
                      double rate = 0.0, std::uint32_t u1 = 0x7F0, std::uint32_t v1 = 0x3F0,
                      double size0 = 100.0, std::array<double, 4> c0 = {128, 64, 32, 128},
                      std::array<double, 4> crate = {0, 0, 0, 0}) {
    R::TypeRecord r = {u0, v0, F(g), F(rate), u1, v1, 0, F(size0)};
    for (int k = 0; k < 4; ++k) {
        r[8 + k] = F(c0[k] + 8388608.0);
        r[12 + k] = F(crate[k]);
    }
    return r;
}

R::ParticleCell cell(std::array<int, 3> pos, int birth, std::array<int, 3> vel = {0, 0, 0}, int slot = 1) {
    return {static_cast<std::int16_t>(pos[0]), static_cast<std::int16_t>(pos[1]),
            static_cast<std::int16_t>(pos[2]), static_cast<std::int16_t>(birth),
            static_cast<std::int16_t>(vel[0]), static_cast<std::int16_t>(vel[1]),
            static_cast<std::int16_t>(vel[2]), static_cast<std::int16_t>(slot * 4)};
}

void test_vu_arithmetic() {
    const R::Vu vu;
    check(vu.add(F(1.0), F(std::ldexp(3.0, -24))) == F(1.0) + 1, "vu: sum cut toward zero");
    check(R::Vu::vin(0x7F800000) == 0x7F7FFFFF, "vu: exponent 255 read as largest");
    check(R::Vu::vin(0x00000001) == 0, "vu: exponent 0 read as zero");
    check(vu.div(F(1.0), 0) == 0x7F7FFFFF, "vu: 1/0");
    check(vu.div(F(-1.0), 0) == 0xFF7FFFFF, "vu: -1/0");
    check(R::Vu::vmax(F(-5.0), R::TWO23) == R::TWO23, "vu: max by pattern");
    check(R::Vu::vmin(F(9e6), R::TWO23_255) == R::TWO23_255, "vu: min by pattern");
}

void test_particles() {
    const auto hdr = R::particle_header(IDENT_VIEW, F(477.5), 10);
    const auto m = R::particle_projection(F(477.5));
    check(m[11] == R::ONE, "projection [2][3] is 1");
    check(hdr.vf5[3] == F(10.0), "header clock");
    check(hdr.vf6[0] == F(800.0) && hdr.vf6[1] == F(400.0), "header screen");

    auto v = R::sprite_vertices(cell({0, 0, 200}, 10), typerec(), hdr);
    const double cx = (double(v[0].x) + v[1].x) / 2, cy = (double(v[0].y) + v[1].y) / 2;
    check(std::fabs(cx - 0x8000) <= 1 && std::fabs(cy - 0x8000) <= 1, "sprite straight ahead is centred");
    check(std::fabs((double(v[1].x) - v[0].x) - 2 * (double(v[1].y) - v[0].y)) <= 2, "sprite is 2:1");
    check(v[1].adc == 0, "sprite not culled");
    check(v[0].rgba == std::array<std::uint8_t, 4>{128, 64, 32, 128}, "sprite colour");
    check(v[0].u == 0x610 && v[0].v == 0x210 && v[1].u == 0x7F0 && v[1].v == 0x3F0, "sprite uv");

    const auto rec = typerec(0x610, 0x210, 0, 0, 0x7F0, 0x3F0, 100, {10, 250, 0, 255}, {-1.0, 1.0, 0.5, -0.25});
    check(R::colour_bytes(rec, F(20.0)) == std::array<std::uint8_t, 4>{0, 255, 10, 250}, "colour runs and clamps");

    v = R::sprite_vertices(cell({0, 0, -200}, 10), typerec(), hdr);
    check(v[1].adc == 1, "behind the camera is culled");

    auto lrec = typerec(0x610, 0x210, 0, 0, 0x7F0, 0x3F0, 100, {200, 100, 50, 90});
    lrec[0] = 0;
    lrec[3] = F(3.0);
    const auto l = R::line_vertices(cell({0, 0, 200}, 0, {0, 4096, 0}), lrec, hdr);
    check(l[0].rgba == std::array<std::uint8_t, 4>{200, 100, 50, 90}, "line head colour");
    check(l[1].rgba == std::array<std::uint8_t, 4>{200, 100, 50, 0}, "line tail alpha 0");
    check(l[1].y < l[0].y, "line tail behind the head");

    const auto blocks = R::list_blocks({7}, 5, 37);
    check(blocks.size() == 2 && blocks[0].page == 7 && blocks[0].block == 0 && blocks[1].block == 1,
          "list_blocks blocks");
    check(blocks[0].cells[0] == 1, "list_blocks retired cell sent");
    check(blocks[1].cells[0] == 33 && blocks[1].cells[3] == 36 && blocks[1].cells[4] == -1,
          "list_blocks zero fill");
    check(R::list_blocks({3, 4}, 0x80, 0x101).back().block == 7, "list_blocks full last page");
}

R::Trail trail(const std::vector<R::TrailPoint> &points, int oldest = 0, int flags = 0, int clock = 10,
               int lifetime = 60, int row = 3, int width4 = 250) {
    R::Trail t;
    for (std::size_t k = 0; k < points.size() && k < 128; ++k)
        t.points[k] = points[k];
    t.handle = 0x101;
    t.clock = static_cast<std::int16_t>(clock);
    t.oldest = static_cast<std::uint8_t>(oldest);
    t.newest = static_cast<std::uint8_t>(points.size() & 0x7F);
    t.row = static_cast<std::int8_t>(row);
    t.flags = static_cast<std::uint8_t>(flags);
    t.lifetime = static_cast<std::int16_t>(lifetime);
    t.rgba = {1, 2, 3, 4};
    t.width4 = static_cast<std::uint8_t>(width4);
    return t;
}

void test_trails() {
    const std::vector<R::TrailPoint> four(4, R::TrailPoint{0, 0, 0, 1});
    const auto h = R::trail_header(trail(four, 0, 4));
    check(h[3][0] == 0x48, "trail header additive");
    check(to_float(h[1][3]) == 31.0, "trail header row range");
    check(to_float(h[4][1]) == 524288.0 + 96.5, "trail header v0");
    check(to_float(h[4][3]) == 10000.0, "trail header width");
    check(R::trail_header(trail({}, 0, 8))[3][0] == 0x09, "trail header flag 8");

    auto t = trail(std::vector<R::TrailPoint>(100, R::TrailPoint{0, 0, 0, 1}));
    auto ch = R::trail_chunks(t);
    check(ch.size() == 4 && ch[0].start == 0 && ch[0].count == 32 && ch[3].start == 96 && ch[3].count == 4,
          "trail chunks of 32");
    t.oldest = 3;
    t.newest = 10;
    ch = R::trail_chunks(t);
    check(ch.size() == 1 && ch[0].start == 2 && ch[0].count == 8, "oldest rounded down to even");
    t.oldest = 120;
    t.newest = 6;
    ch = R::trail_chunks(t);
    check(ch.size() == 2 && ch[0].start == 120 && ch[0].count == 8 && ch[1].start == 0 && ch[1].count == 6,
          "chunks split at the ring end");

    const std::vector<R::TrailPoint> pts = {{0, 0, 400, 1}, {100, 0, 400, 2}, {200, 0, 400, 3}};
    const auto hdr = R::particle_header(IDENT_VIEW, F(477.5), 0);
    const auto vs = R::trail_vertices(trail(pts), hdr.m, hdr.vf6, hdr.vf7);
    check(vs.size() == 6, "strip vertex count");
    check(vs[0].adc == 1 && vs[1].adc == 1 && vs[2].adc == 0, "the first point starts the strip");
    check(vs[2].x == vs[3].x, "horizontal trail offset in y only");
    check(vs[2].y != vs[3].y, "strip has a width");
    const auto narrow = R::trail_vertices(trail(pts, 0, 0, 10, 60, 3, 1), hdr.m, hdr.vf6, hdr.vf7);
    check(std::abs(int(narrow[2].y) - int(narrow[3].y)) < std::abs(int(vs[2].y) - int(vs[3].y)), "narrower");
    const auto lost = R::trail_vertices(trail(pts, 0, 0, 10, 60, 3, 1), hdr.m, hdr.vf6, hdr.vf7, R::Vu(),
                                        R::WaitQ::Vu1CppReading);
    const auto same = R::trail_vertices(trail(pts), hdr.m, hdr.vf6, hdr.vf7, R::Vu(), R::WaitQ::Vu1CppReading);
    check(lost[2].x == same[2].x && lost[2].y == same[2].y, "width ignored under vu1.cpp's reading");
    check(vs[2].clip_w != 0 && to_float(vs[2].clip_w) > 0, "trail vertex carries clip.w");

    const std::vector<R::TrailPoint> pp = {{0, 0, 400, 1}, {0, 20, 400, 255}, {50, 0, 400, 2}, {50, 20, 400, 128}};
    const auto pv = R::trail_vertices(trail(pp, 0, 2), hdr.m, hdr.vf6, hdr.vf7);
    check(pv.size() == 4 && pv[0].rgba[3] == 255 && pv[1].rgba[3] == 255 && pv[2].rgba[3] == 128 &&
              pv[3].rgba[3] == 128, "pairs carry alpha");
}

void test_beams() {
    core::Lcg cos(1234);
    core::LibcRand libc(1);
    const auto nodes = R::beam_build_nodes(F(10.0), 3, cos, libc);
    check(nodes.size() == 15, "beam tree size");
    core::Lcg chk(1234);
    for (int k = 0; k < 15 * 6; ++k)
        chk.draw();
    check(cos.state == chk.state, "six cosmetic draws per node");
    bool counts = true;
    for (const auto &n : nodes)
        counts = counts && n.count < 8;
    check(counts, "node counts below 8");
    core::Lcg c(1234);
    Raw s[6];
    for (Raw &x : s)
        x = c.signed_unit();
    check(nodes[0].drift[2] == core::mul(core::sub(core::mul(s[5], F(10.0)), nodes[0].off[1]), 0x3DCCCCCCu),
          "drift.z subtracts offset.y");

    core::Lcg c5(5), c9(9);
    core::LibcRand l1(1);
    const auto n2 = R::beam_build_nodes(0, 2, c5, l1);
    const auto pts = R::beam_points(n2, {0, 0, 0}, {F(70.0), 0, 0}, 2, 0, c9);
    bool line = pts.size() == 7;
    for (std::size_t k = 0; line && k < 7; ++k)
        line = to_float(pts[k][0]) == 8.75 * double(k + 1);
    check(line, "points between the ends without jitter, ends excluded");

    std::vector<R::BeamNode> one(1);
    one[0].drift = {F(1.0), 0, 0};
    core::Lcg c77(77);
    R::beam_drift_nodes(one, F(4.0), 0, c77);
    check(one[0].off[0] == F(1.0), "drift moves the offset");
    check(one[0].count == ((c77.state >> 21) & 7), "retarget count from the fourth draw");

    check(R::beam_frame_seed(100, 2) == 100 * 0x229 + 2 * 0x3F3, "beam frame seed");
    const auto bt = R::beam_trail_points({{F(1.5), F(-2.0), 0}});
    check(bt.ring.size() == 1 && bt.ring[0] == R::TrailPoint{3, -4, 0, 1} && bt.clock == 1, "beam trail points");
}

R::Glow glow(double z, int flags = 0, std::uint32_t tex = 0x100, double bias = 0.0, double size = 8000.0) {
    R::Glow g;
    g.pos = {0, 0, F(z)};
    g.size = F(size);
    g.bias = F(bias);
    g.rgba = {255, 150, 0, 60};
    g.tex = tex;
    g.angle = 0;
    g.flags = static_cast<std::int16_t>(flags);
    return g;
}

R::Mat4 test_viewproj() {
    const auto proj = core::mat4_set_projection(0x3F7FFFFF, 0x3EEEEEEE, F(477.5), R::ONE, 0x457FFF00,
                                                 0x41900000, 0x44E10000);
    return core::mat4_mul_mat34(IDENT_VIEW, proj);
}

void test_glows() {
    const auto vp = test_viewproj();
    const auto a = R::glow_project(glow(400.0), IDENT_VIEW, vp);
    check(a && a->x == 0x8000 && a->y == 0x8000, "glow centre");
    check(a && (a->h == 319 || a->h == 320), "glow size");
    check(!R::glow_project(glow(10.0), IDENT_VIEW, vp), "glow depth < 20 culled");
    const auto b = R::glow_project(glow(400.0, 0, 0x100, 100.0), IDENT_VIEW, vp);
    check(a && b && a->h == b->h, "bias keeps the size");
    check(a && b && b->z > a->z, "bias moves toward the camera");

    std::vector<R::Glow> q = {glow(400, 0, 0x100), glow(400, 2, 0x100), glow(400, 0, 0x200),
                              glow(400, 0, 0x100), glow(400, 4, 0x100)};
    const R::TextureDims dims = {{0x100, {32, 32}}, {0x200, {32, 32}}};
    const auto p1 = R::glow_packets(q, 1, IDENT_VIEW, vp, dims);
    check(p1.size() == 1 && p1[0].key == 4 && p1[0].texture == 0x100, "pass 1 packets");
    const auto p0 = R::glow_packets(q, 0, IDENT_VIEW, vp, dims);
    check(p0.size() == 3 && p0[0].key == 3 && p0[0].texture == 0x100 && p0[1].texture == 0x100 &&
              p0[2].texture == 0x200, "pass 0 packets");
    int xyz2 = 0;
    for (const auto &e : p0[0].events)
        xyz2 += e.reg == 0x05;
    check(xyz2 == 4, "two sprites");
    bool additive = false;
    for (const auto &e : p0[1].events)
        additive = additive || (e.reg == 0x42 && e.value == 0x48);
    check(additive, "flags 2: additive");
    check(p0[0].sprites.size() == 2 && p0[0].sprites[0].screen.depth == a->depth, "glow sprites carry depth");

    const auto ev = R::glow_vertices(glow(400.0), 10, 0x8000, 5, 100, 32, 32);
    check((ev[2].value >> 48) == 0xFFFF, "negative x spills into y and z");
}

void test_builders() {
    const std::vector<Vec3> pts = {{F(0.0), 0, 0}, {F(10.0), F(5.0), 0}, {F(20.0), 0, 0}, {F(30.0), 0, 0}};
    check(*R::spline_eval(0, pts, 4).pos == pts[0], "spline starts at its first point");
    const auto mid = R::spline_eval(F(1.0 / 3.0), pts, 4).pos;
    check(std::fabs(to_float((*mid)[0]) - 10.0) < 1e-4, "spline hits its points");
    check(*R::spline_eval(F(-1.0), pts, 4).pos == pts[0], "spline t clamped to 0");

    const std::vector<Vec3> p3 = {{F(0.0), 0, 0}, {F(10.0), 0, 0}, {F(20.0), 0, 0}};
    const auto s = R::trail_set_spline(0, F(1.0), p3, 3, 20);
    check(s.points.size() == 20 && s.lifetime == 20, "spline trail counts");

    const auto c = R::trail_set_circle(F(10.0), {0, 0, 0}, {0, 0, R::ONE}, 4);
    bool round = c.size() == 4;
    for (const auto &p : c) {
        const double r2 = to_float(p[0]) * to_float(p[0]) + to_float(p[1]) * to_float(p[1]) +
                          to_float(p[2]) * to_float(p[2]);
        round = round && std::fabs(r2 - 100.0) < 0.01 && (p[2] & 0x7FFFFFFF) == 0;
    }
    check(round, "circle");

    const auto sw = R::trail_sweep_pairs({0, 0, 0}, {0, 0, R::ONE}, {F(10.0), 0, 0}, {F(20.0), 0, 0}, 100);
    check(sw.pairs.size() == 60 && sw.lifetime == 60, "sweep clamps to 60");
    check(R::trail_pair(F(0.5), {0, 0, 0}, {F(1.0), 0, 0}, 7)[1][3] == 127, "pair alpha byte");
}

// ---- (b) stores, sorted list, frame --------------------------------------------------------

template <class Fn> bool refuses(Fn fn) {
    try {
        fn();
    } catch (const R::Refusal &) {
        return true;
    }
    return false;
}

void test_haze_refusal() {
    check(refuses([] { R::draw_haze(); }), "haze draw refuses kind 3 and the screen axis");
}

void test_trail_store() {
    R::TrailStore ts;
    const auto h = ts.create(F(0.2), 1, 1000, 0, 1, 2, 3, 4);
    check(h != 0 && ts.find(h), "trail_create");
    const R::Trail *t = ts.find(h);
    check(t->lifetime == core::to_int(core::mul(0x42700000u, F(0.2))), "lifetime = trunc(60 s)");
    check(t->width4 == (1000 >> 2) % 256, "width / 4 stored as a byte");
    check(t->row == 1 && t->rgba == std::array<std::uint8_t, 4>{1, 2, 3, 4}, "row, colour");
    int added = 0;
    while (ts.add_point(h, {F(1.0), F(2.0), F(3.0)}))
        ++added;
    check(added == 126, "the ring refuses within two of full");
    check(t->points[0] == R::TrailPoint{2, 4, 6, 1}, "stamp = clock + 1, half units");
    ts.set_color(h, 9, 8, 7, 6);
    ts.set_length(h, 400);
    check(t->rgba == std::array<std::uint8_t, 4>{9, 8, 7, 6} && t->width4 == 100, "setColor, setLength");
    ts.clear(h);
    check(t->clock == 0 && t->oldest == 0 && t->newest == 0, "trail_clear");

    // aging: lifetime 3, one point at stamp 1, dropped when clock - 1 >= 3, then freed
    R::TrailStore a;
    const auto h1 = a.create(F(0.05), 0, 4, 0, 0, 0, 0, 0);
    a.trails[h1 & 0xFF].lifetime = 3;
    a.add_point(h1, {0, 0, 0});
    a.update();
    a.update();
    a.update();
    check(a.find(h1) && a.find(h1)->clock == 3, "clock advances per tick");
    a.update(true);
    check(a.find(h1) && a.find(h1)->clock == 3, "not aged while 0x44fc98 is set");
    a.update();
    check(!a.find(h1), "freed when no point is left");

    R::TrailStore b;
    const auto e = b.create(R::ONE, 0, 4, 0, 0, 0, 0, 0);
    const auto f1 = b.create(R::ONE, 0, 4, 1, 0, 0, 0, 0);
    const auto f10 = b.create(R::ONE, 0, 4, 0x10, 0, 0, 0, 0);
    b.add_point(f10, {0, 0, 0});
    b.update();
    check(!b.find(e), "a trail that never had a point is freed");
    check(b.find(f1) && b.find(f1)->clock == 0, "flag 1: not aged, not freed");
    check(b.find(f10) && b.find(f10)->clock == 0, "flag 0x10: the clock does not advance");

    R::TrailStore p;
    const auto hp = p.create(R::ONE, 0, 4, 2, 0, 0, 0, 0);
    p.trails[hp & 0xFF].lifetime = 2;
    p.add_pair(R::ONE, hp, {0, 0, 0}, {0, 0, 0});
    p.trails[hp & 0xFF].clock = 1;
    p.add_pair(R::ONE, hp, {0, 0, 0}, {0, 0, 0});
    p.update();                         // clock 2: first pair (stamp 1) age 1
    p.update();                         // clock 3: first pair age 2 -> dropped (two entries)
    check(p.find(hp) && p.find(hp)->oldest == 2 && p.find(hp)->newest == 4, "pairs dropped two at a time");

    ts.release(h);
    check(!ts.find(h), "trail_release");
    ts.set_color(h, 1, 1, 1, 1);        // stale handle: ignored
    const auto h2 = ts.create(R::ONE, 0, 4, 0, 0, 0, 0, 0);
    check(h2 != h && (h2 & 0xFF) == (h & 0xFF), "a reused slot gets a new generation");

    R::TrailStore full;
    int made = 0;
    while (full.create(R::ONE, 0, 4, 0, 0, 0, 0, 0))
        ++made;
    check(made == 64, "64 trails");

    R::TrailStore bs;
    const auto hb = bs.create(R::ONE, 0, 4, 0, 0, 0, 0, 0);
    bs.set_spline(0, R::ONE, hb, {{0, 0, 0}, {F(10.0), 0, 0}, {F(20.0), 0, 0}}, 3, 20);
    check(bs.find(hb)->newest == 20 && bs.find(hb)->clock == 20 && bs.find(hb)->lifetime == 20 &&
              bs.find(hb)->points[19][3] == 20, "setSpline into the ring");
    check(refuses([&] { bs.set_spline(0, R::ONE, hb, {{0, 0, 0}, {F(10.0), 0, 0}}, 2, 200); }),
          "setSpline past the ring refused");
    bs.sweep_pairs(hb, {0, 0, 0}, {0, 0, R::ONE}, {F(1.0), 0, 0}, {F(2.0), 0, 0}, 10);
    check(bs.find(hb)->newest == 20 && bs.find(hb)->clock == 10 && bs.find(hb)->lifetime == 10 &&
              bs.find(hb)->points[1][3] == 255, "sweepPairs into the ring");
    bs.set_circle(F(5.0), hb, {0, 0, 0}, {0, 0, R::ONE}, 12);
    check(bs.find(hb)->newest == 12 && bs.find(hb)->clock == 12 && bs.find(hb)->lifetime == 10,
          "setCircle keeps the lifetime");
}

void test_beam_store() {
    R::TrailStore ts;
    R::BeamStore bs(ts);
    core::Lcg cos(42);
    core::LibcRand libc(1);
    const std::array<std::uint8_t, 4> rgba = {0x80, 0x80, 0x80, 0x5E};
    const auto h = bs.create(F(10.0), F(2.0), {0, 0, 0}, {F(100.0), 0, 0}, 0x104, 9, rgba, 1000, false, cos, libc);
    check(h != 0 && bs.is_alive(h), "beam_create");
    const R::Beam &b = bs.beams[h & 0xFF];
    check(b.depth == 5 && b.nodes.size() == 63 && b.expiry == 1010, "depth clamp, expiry");
    const R::Trail *ta = ts.find(b.trail_a), *tb = ts.find(b.trail_b);
    check(ta && tb && ta->flags == 5 && ta->lifetime == 60 && ta->newest == 1 && tb->newest == 1 &&
              ta->width4 == (0x104 >> 2), "beam trails");
    check(refuses([&] { bs.create(0, 0, {}, {}, 4, -1, rgba, 0, false, cos, libc); }), "negative depth refused");

    const std::uint32_t before = cos.state;
    bs.draw_step(1005, 0, 2, true, cos);     // frozen: no drift, the seed is put back
    check(cos.state == before, "frozen draw leaves the cosmetic seed");
    check(tb->newest == 63 && tb->clock == 63 && ta->newest == 0 && ta->clock == 0, "even draw frame: +0x724");
    bs.draw_step(1005, 0, 3, false, cos);
    check(cos.state != before, "drift draws the live seed");
    check(ta->newest == 63 && tb->newest == 0, "odd draw frame: +0x720");
    bs.update(h, {0, 0, 0}, {F(50.0), 0, 0}, 0x1F7, -1, 1006);
    check(ta->rgba[3] == 0xF7 && ta->width4 == (0x104 >> 2) && b.expiry == 1008, "beam_update");
    bs.update(h, {0, 0, 0}, {F(50.0), 0, 0}, 0x10, 400, 1006);
    check(tb->width4 == 100, "beam_update width");
    ts.update();
    check(ts.find(b.trail_a) && ts.find(b.trail_b), "beam trails are not aged");
    const auto ha = b.trail_a;
    bs.draw_step(1009, 0, 4, false, cos);
    check(!bs.is_alive(h) && !ts.find(ha), "expired beam destroyed with its trails");

    const auto h2 = bs.create(F(1.0), F(1.0), {}, {}, 4, 0, rgba, 50, true, cos, libc);
    bs.draw_step(1000, 59, 0, false, cos);
    check(bs.is_alive(h2), "in-sim-tick beam tested against g_netFrame");
    bs.draw_step(0, 61, 0, false, cos);
    check(!bs.is_alive(h2), "in-sim-tick beam expires by g_netFrame");
    check(((h2 >> 8) & 0xFF) != ((h >> 8) & 0xFF), "generation shared and advancing");
}

void test_glow_queue_and_order() {
    R::GlowQueue q;
    int ok = 0;
    for (int k = 0; k < 0x401; ++k)
        ok += q.add(glow(400.0));
    check(ok == 0x400 && q.records.size() == 0x400, "the 0x401st glow is dropped");

    R::SortedPackets s;
    auto pk = [](int key, std::uint32_t tex) {
        R::Packet p;
        p.key = key;
        p.texture = tex;
        return p;
    };
    s.add(pk(4, 2));            // trail
    s.add(pk(4, 10));           // glow(1) A
    s.add(pk(4, 11));           // glow(1) B
    s.add(pk(4, 10));           // glow(1) A again: after A
    s.add(pk(4, 1));            // particles
    s.add(pk(3, 10));
    s.add(pk(3, 11));
    std::vector<std::pair<int, std::uint32_t>> got;
    for (const auto &p : s.gs_order())
        got.emplace_back(p.key, p.texture);
    const std::vector<std::pair<int, std::uint32_t>> want = {{3, 11}, {3, 10}, {4, 1}, {4, 11},
                                                             {4, 10}, {4, 10}, {4, 2}};
    check(got == want, "sorted-list insertion rule");
}

void test_frame() {
    R::TrailStore ts;
    R::BeamStore bs(ts);
    R::GlowQueue gq;
    core::Lcg cos(7);
    core::LibcRand libc(1);
    const auto th = ts.create(R::ONE, 0, 400, 0, 1, 2, 3, 4);
    ts.add_point(th, {0, 0, F(200.0)});
    ts.add_point(th, {F(10.0), 0, F(200.0)});
    bs.create(F(5.0), F(1.0), {0, 0, F(300.0)}, {F(40.0), 0, F(300.0)}, 400, 2, {1, 1, 1, 1}, 0, false, cos, libc);
    gq.add(glow(400.0, 0, 0x100));
    gq.add(glow(400.0, 4, 0x200));
    gq.add(glow(400.0, 4, 0x300));
    R::ParticleFrame pf;
    pf.clock = 10;
    pf.pages.resize(1);
    pf.types[1] = typerec();
    pf.pages[0][1] = cell({0, 0, 200}, 10);
    pf.global.lists[0].pages = {0};
    pf.global.lists[0].head = 1;
    pf.global.lists[0].tail = 2;
    R::ParticleListSet g;
    g.matrix = IDENT_VIEW;
    g.lists[4].pages = {0};
    g.lists[4].head = 1;
    g.lists[4].tail = 2;
    pf.groups.push_back(g);
    R::FrameInput in;
    in.view = IDENT_VIEW;
    in.proj_scale = F(477.5);
    in.camera = IDENT_VIEW;
    in.viewproj = test_viewproj();
    in.particle_texture = 1;
    in.trail_texture = 2;
    const R::TextureDims dims = {{0x100, {32, 32}}, {0x200, {16, 16}}, {0x300, {8, 8}}};
    const auto out = R::draw_frame(in, pf, ts, &bs, gq, dims, cos);
    std::vector<std::pair<int, std::uint32_t>> order;
    for (const auto &p : out)
        order.emplace_back(p.key, p.texture);
    const std::vector<std::pair<int, std::uint32_t>> want = {{3, 0x100}, {4, 1}, {4, 0x300}, {4, 0x200}, {4, 2}};
    check(order == want, "frame: key 3 glows, particles, glow(1) last texture first, trails");
    check(gq.records.empty(), "frame empties the glow queue");
    const auto &pp = out[1].particles;
    check(pp.alpha_writes == std::vector<std::uint32_t>{0x44, 0x48, 0x44, 0x48}, "ALPHA_1 per kind and set");
    check(pp.draws.size() == 60 && pp.draws[0].prim == R::PRIM_SPRITE && pp.draws[0].alpha1 == 0x44 &&
              pp.draws[30].set == 1 && pp.draws[30].alpha1 == 0x48 && pp.draws[1].zero_filled,
          "particle draws");
    check(pp.draws[0].v[0].clip_w == pp.draws[0].v[1].clip_w && to_float(pp.draws[0].v[0].clip_w) > 0,
          "sprite corners carry the centre's clip.w");
    const auto &tr = out[4].trails.trails;
    check(tr.size() == 2 && tr[0].alpha1 == 0x44 && tr[0].prim == R::PRIM_TRAIL && tr[1].alpha1 == 0x48 &&
              tr[1].vertices.size() == 14, "trail packet (trail, then the beam's 7 points)");
    check(out[0].glow.prim == R::PRIM_GLOW && out[0].glow.alpha1 == 0x44, "glow packet prim and ALPHA_1");
}

// ---- (c) vectors -------------------------------------------------------------------------

std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::uint64_t hex64(const std::string &s) { return std::stoull(s, nullptr, 16); }
std::uint32_t hex(const std::string &s) { return static_cast<std::uint32_t>(hex64(s)); }

std::vector<std::uint32_t> hexes(const std::string &s) {
    std::vector<std::uint32_t> out;
    if (s.empty())
        return out;
    for (const auto &x : split(s, ','))
        out.push_back(hex(x));
    return out;
}

template <std::size_t N> std::array<Raw, N> arr(const std::string &s) {
    const auto v = hexes(s);
    if (v.size() != N)
        throw std::runtime_error("bad vector field: " + s);
    std::array<Raw, N> a{};
    for (std::size_t k = 0; k < N; ++k)
        a[k] = v[k];
    return a;
}

std::vector<Vec3> vecs(const std::string &s) {
    const auto v = hexes(s);
    std::vector<Vec3> out;
    for (std::size_t k = 0; k + 2 < v.size(); k += 3)
        out.push_back({v[k], v[k + 1], v[k + 2]});
    return out;
}

std::string hx(std::uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(v));
    return b;
}

template <class C> std::string hl(const C &c) {
    std::string s;
    for (auto v : c) {
        if (!s.empty())
            s += ',';
        s += hx(static_cast<std::uint32_t>(v));
    }
    return s;
}

std::string flat(const std::vector<Vec3> &vs) {
    std::vector<Raw> w;
    for (const auto &v : vs)
        w.insert(w.end(), v.begin(), v.end());
    return hl(w);
}

std::string vtx(const R::GsVertex &v) {
    char b[96];
    std::snprintf(b, sizeof b, "%x.%x.%x.%x.%x.%02x%02x%02x%02x.", v.x, v.y, v.z, v.f, v.adc, v.rgba[0],
                  v.rgba[1], v.rgba[2], v.rgba[3]);
    std::string s = b;
    if (v.has_uv) {
        std::snprintf(b, sizeof b, "%x:%x", v.u, v.v);
        s += b;
    } else {
        s += "-";
    }
    return s;
}

std::string vtxs(const std::vector<R::GsVertex> &vs) {
    std::string s;
    for (const auto &v : vs) {
        if (!s.empty())
            s += ' ';
        s += vtx(v);
    }
    return s;
}

std::string pt(const R::TrailPoint &p) {
    std::string s;
    for (int k = 0; k < 4; ++k) {
        if (k)
            s += '.';
        s += hx(static_cast<std::uint16_t>(p[k]));
    }
    return s;
}

R::TrailPoint parse_pt(const std::string &s) {
    const auto f = split(s, '.');
    return {static_cast<std::int16_t>(hex(f[0])), static_cast<std::int16_t>(hex(f[1])),
            static_cast<std::int16_t>(hex(f[2])), static_cast<std::int16_t>(hex(f[3]))};
}

struct Stat {
    long rows = 0, items = 0, bad = 0;
};

// Each runner returns (outputs, items).  Outputs are compared with the row's last columns.
using Runner = std::function<std::vector<std::string>(const std::vector<std::string> &, long &)>;

std::vector<std::string> run_vu(const std::vector<std::string> &f, long &items) {
    const R::Vu vu;
    const std::string &op = f[2];
    const auto a = hexes(f[3]);
    Raw r = 0;
    if (op == "add") r = vu.add(a[0], a[1]);
    else if (op == "sub") r = vu.sub(a[0], a[1]);
    else if (op == "mul") r = vu.mul(a[0], a[1]);
    else if (op == "madd") r = vu.madd(a[0], a[1], a[2]);
    else if (op == "div") r = vu.div(a[0], a[1]);
    else if (op == "rsqrt") r = vu.rsqrt(a[0], a[1]);
    else if (op == "itof") r = vu.itof(a[0], a[1]);
    else if (op == "ftoi") r = vu.ftoi(a[0], a[1]);
    else if (op == "vmax") r = R::Vu::vmax(a[0], a[1]);
    else if (op == "vmin") r = R::Vu::vmin(a[0], a[1]);
    else if (op == "clip") r = vu.clip({a[0], a[1], a[2], 0}, a[3]);
    else throw std::runtime_error("unknown vu op " + op);
    items = 1;
    return {hx(r)};
}

std::vector<std::string> run_proj(const std::vector<std::string> &f, long &items) {
    const auto view = arr<12>(f[2]);
    const Raw proj = hex(f[3]);
    const auto group = arr<12>(f[4]);
    const auto hdr = R::particle_header(view, proj, static_cast<std::int16_t>(hex(f[5])));
    items = 1;
    return {hl(R::particle_projection(proj)), hl(R::particle_matrix(view, proj)),
            hl(R::group_matrix(group, view, proj)), hl(hdr.vf5)};
}

std::vector<std::string> run_blocks(const std::vector<std::string> &f, long &items) {
    const auto blocks = R::list_blocks(hexes(f[2]), hex(f[3]), hex(f[4]));
    std::string s;
    for (const auto &b : blocks) {
        int k = 0;
        bool shape = true;
        for (int c = 0; c < 30; ++c) {
            if (b.cells[c] >= 0) {
                shape = shape && k == c && b.cells[c] == static_cast<std::int32_t>(b.block * 32 + c + 1);
                ++k;
            }
        }
        if (!s.empty())
            s += ',';
        s += hx(b.page) + "." + hx(b.block) + "." + hx(k) + (shape ? "" : "!");
    }
    items = 1;
    return {s};
}

R::ParticleCell parse_cell(const std::string &s) {
    const auto w = split(s, '.');
    R::ParticleCell c{};
    for (int k = 0; k < 8; ++k)
        c[k] = static_cast<std::int16_t>(hex(w[k]));
    return c;
}

std::vector<std::string> run_particles(const std::vector<std::string> &f, long &items) {
    const R::WaitQ wq = f[2] == "w" ? R::WaitQ::WaitFirst : R::WaitQ::Vu1CppReading;
    const auto view = arr<12>(f[3]);
    const Raw proj = hex(f[4]);
    R::ParticleFrame pf;
    pf.clock = static_cast<std::int16_t>(hex(f[5]));
    const auto stale = arr<2>(f[6]);
    for (const auto &t : split(f[7], ';')) {
        const auto kv = split(t, '=');
        pf.types[hex(kv[0])] = arr<16>(kv[1]);
    }
    pf.pages.resize(hex(f[8]));
    if (!f[9].empty())
        for (const auto &c : split(f[9], ';')) {
            const auto kv = split(c, '=');
            const auto pc = split(kv[0], '/');
            pf.pages.at(hex(pc[0]))[hex(pc[1])] = parse_cell(kv[1]);
        }
    const auto sets = split(f[10], ';');
    for (std::size_t s = 0; s < sets.size(); ++s) {
        const auto ml = split(sets[s], '/');
        R::ParticleListSet set;
        set.matrix = arr<12>(ml[0]);
        if (!ml[1].empty())
            for (const auto &l : split(ml[1], '+')) {
                const auto p = split(l, ':');
                R::ParticleList &lst = set.lists.at(hex(p[0]));
                for (const auto &pg : split(p[1], '.'))
                    lst.pages.push_back(hex(pg));
                lst.head = hex(p[2]);
                lst.tail = hex(p[3]);
            }
        if (s == 0)
            pf.global = set;
        else
            pf.groups.push_back(set);
    }
    const auto pk = R::particle_packet(pf, view, proj, stale, wq);
    std::vector<R::GsVertex> vs;
    for (const auto &d : pk.draws) {
        vs.push_back(d.v[0]);
        vs.push_back(d.v[1]);
    }
    items = static_cast<long>(pk.draws.size());
    return {hl(pk.alpha_writes), vtxs(vs)};
}

std::vector<std::string> run_trail(const std::vector<std::string> &f, long &items) {
    const R::WaitQ wq = f[2] == "w" ? R::WaitQ::WaitFirst : R::WaitQ::Vu1CppReading;
    const auto view = arr<12>(f[3]);
    const Raw proj = hex(f[4]);
    const R::RowTable rows = f[5] == "d" ? R::TRAIL_ROW_V : arr<10>(f[5]);
    const auto fl = hexes(f[6]);
    R::Trail t;
    t.handle = 0x101;
    t.clock = static_cast<std::int16_t>(fl[0]);
    t.oldest = static_cast<std::uint8_t>(fl[1]);
    t.newest = static_cast<std::uint8_t>(fl[2]);
    t.row = static_cast<std::int8_t>(fl[3]);
    t.flags = static_cast<std::uint8_t>(fl[4]);
    t.lifetime = static_cast<std::int16_t>(fl[5]);
    for (int k = 0; k < 4; ++k)
        t.rgba[k] = static_cast<std::uint8_t>(fl[6] >> (8 * k));
    t.width4 = static_cast<std::uint8_t>(fl[7]);
    const auto ring = split(f[7], ':');
    const std::uint32_t start = hex(ring[0]);
    if (!ring[1].empty()) {
        const auto ps = split(ring[1], ';');
        for (std::size_t k = 0; k < ps.size(); ++k)
            t.points[(start + k) & 0x7F] = parse_pt(ps[k]);
    }
    const auto hdr = R::particle_header(view, proj, 0);
    const auto header = R::trail_header(t, rows);
    std::vector<std::uint32_t> hw;
    for (const auto &q : header)
        hw.insert(hw.end(), q.begin(), q.end());
    std::string chunks;
    for (const auto &c : R::trail_chunks(t)) {
        if (!chunks.empty())
            chunks += ',';
        chunks += hx(c.start) + "." + hx(c.count);
    }
    const auto vs = R::trail_vertices(t, hdr.m, hdr.vf6, hdr.vf7, R::Vu(), wq, rows);
    items = static_cast<long>(vs.size());
    return {hl(hw), chunks, vtxs(vs)};
}

Vec3 v3(const std::string &s) { return arr<3>(s); }

std::vector<std::string> run_beam(const std::vector<std::string> &f, long &items) {
    R::TrailStore ts;
    R::BeamStore bs(ts);
    core::Lcg cos(hex(f[2]));
    core::LibcRand libc(hex64(f[3]));
    const std::int32_t sim0 = static_cast<std::int32_t>(hex(f[4]));
    std::vector<std::uint32_t> handles;
    std::string problems;
    for (const auto &spec : split(f[5], ';')) {
        const auto p = hexes(spec);
        const std::array<std::uint8_t, 4> rgba = {static_cast<std::uint8_t>(p[4]), static_cast<std::uint8_t>(p[4] >> 8),
                                                  static_cast<std::uint8_t>(p[4] >> 16),
                                                  static_cast<std::uint8_t>(p[4] >> 24)};
        const auto h = bs.create(p[0], p[1], {p[6], p[7], p[8]}, {p[9], p[10], p[11]},
                                 static_cast<std::int32_t>(p[3]), static_cast<std::int32_t>(p[2]), rgba, sim0,
                                 p[5] != 0, cos, libc);
        if ((h & 0xFF) != handles.size())
            problems += "!slot";
        handles.push_back(h);
    }
    const std::string after = hx(cos.state) + "," + hx(libc.state);
    std::string frames_out;
    items = 0;
    for (const auto &fr : split(f[6], '|')) {
        const auto parts = split(fr, '/');
        const auto fv = hexes(parts[0]);
        const auto sim = static_cast<std::int32_t>(fv[0]), net = static_cast<std::int32_t>(fv[1]);
        const std::uint32_t draw = fv[2];
        if (parts.size() > 1)
            for (const auto &u : split(parts[1], '+')) {
                const auto q = split(u, ':');
                bs.update(handles.at(hex(q[0])), v3(q[1]), v3(q[2]), static_cast<std::int32_t>(hex(q[3])),
                          static_cast<std::int32_t>(hex(q[4])), sim);
            }
        bs.draw_step(sim, net, draw, fv[3] != 0, cos);
        std::string s = hx(cos.state);
        for (std::size_t i = 0; i < handles.size(); ++i) {
            const R::Beam &b = bs.beams[i];
            s += "|";
            if (!b.active || b.handle != handles[i]) {
                s += "-";
                continue;
            }
            ++items;
            const bool even = (draw & 1) == 0;
            const R::Trail *t = ts.find(even ? b.trail_b : b.trail_a);
            const R::Trail *o = ts.find(even ? b.trail_a : b.trail_b);
            if (!t || !o) {
                s += "!trail";
                continue;
            }
            if (o->oldest || o->newest || o->clock)
                s += "!other";
            const std::uint32_t rgba32 = t->rgba[0] | t->rgba[1] << 8 | t->rgba[2] << 16 |
                                         static_cast<std::uint32_t>(t->rgba[3]) << 24;
            s += std::string(even ? "b" : "a") + ":" + hx(rgba32) + ":" + hx(t->width4) + ":";
            const std::uint32_t n = (t->newest - t->oldest) & 0x7F;
            if (t->oldest != 0 || static_cast<std::uint32_t>(t->clock) != n)
                s += "!clock";
            for (std::uint32_t k = 0; k < n; ++k)
                s += (k ? "," : "") + pt(t->points[(t->oldest + k) & 0x7F]);
        }
        if (!frames_out.empty())
            frames_out += ';';
        frames_out += s;
    }
    std::string nodes;
    for (std::size_t i = 0; i < handles.size(); ++i) {
        std::vector<std::uint32_t> w;
        for (const auto &n : bs.beams[i].nodes) {
            const auto ws = n.words();
            w.insert(w.end(), ws.begin(), ws.end());
        }
        nodes += (i ? ";" : "") + hl(w);
    }
    return {after + problems, frames_out, nodes};
}

std::vector<std::string> run_beamfn(const std::vector<std::string> &f, long &items) {
    const Raw jitter = hex(f[2]);
    const auto depth = static_cast<std::int32_t>(hex(f[3]));
    core::Lcg cos(hex(f[4]));
    core::LibcRand libc(hex64(f[5]));
    auto nodes = R::beam_build_nodes(jitter, depth, cos, libc);
    auto words = [&] {
        std::vector<std::uint32_t> w;
        for (const auto &n : nodes) {
            const auto ws = n.words();
            w.insert(w.end(), ws.begin(), ws.end());
        }
        return hl(w);
    };
    const std::string n0 = words() + ";" + hx(cos.state) + ";" + hx(libc.state);
    core::Lcg dc(hex(f[6]));
    for (std::uint32_t k = 0; k < hex(f[7]); ++k)
        R::beam_drift_nodes(nodes, jitter, depth, dc);
    core::Lcg pc(hex(f[12]));
    const auto pts = R::beam_points(nodes, v3(f[8]), v3(f[9]), depth, hex(f[10]), pc, hex(f[11]));
    items = static_cast<long>(nodes.size() + pts.size());
    return {n0, words(), hx(dc.state), flat(pts), hx(pc.state)};
}

std::vector<std::string> run_glows(const std::vector<std::string> &f, long &items) {
    const auto cam = arr<12>(f[2]);
    const auto vp = arr<16>(f[3]);
    R::TextureDims dims;
    for (const auto &d : split(f[4], ';')) {
        const auto kv = split(d, '=');
        const auto wh = split(kv[1], '.');
        dims[hex(kv[0])] = {static_cast<std::int32_t>(hex(wh[0])), static_cast<std::int32_t>(hex(wh[1]))};
    }
    std::vector<R::Glow> q;
    for (const auto &gs : split(f[5], ';')) {
        const auto p = hexes(gs);
        R::Glow g;
        g.pos = {p[0], p[1], p[2]};
        g.size = p[3];
        g.bias = p[4];
        for (int k = 0; k < 4; ++k)
            g.rgba[k] = static_cast<std::uint8_t>(p[5] >> (8 * k));
        g.tex = p[6];
        g.angle = static_cast<std::uint16_t>(p[7]);
        g.flags = static_cast<std::int16_t>(p[8]);
        q.push_back(g);
    }
    std::string proj;
    for (const auto &g : q) {
        const auto sp = R::glow_project(g, cam, vp);
        if (!proj.empty())
            proj += ';';
        proj += sp ? std::to_string(sp->x) + "." + std::to_string(sp->y) + "." + std::to_string(sp->z) + "." +
                         std::to_string(sp->h)
                   : "-";
    }
    auto packets = [&](int pass) {
        std::string s;
        for (const auto &p : R::glow_packets(q, pass, cam, vp, dims)) {
            if (!s.empty())
                s += ';';
            s += hx(static_cast<std::uint32_t>(p.key)) + "." + hx(p.texture) + "." + hx(p.prim) + ":";
            for (std::size_t k = 0; k < p.events.size(); ++k)
                s += (k ? "," : "") + hx(p.events[k].reg) + "=" + hx(p.events[k].value);
        }
        return s;
    };
    const std::string p1 = packets(1);
    const std::string p0 = packets(0);
    items = static_cast<long>(q.size());
    return {proj, p1, p0};
}

// Builders: the free functions against the reference, then the TrailStore version of the same
// call against the ring the free function's points imply.
std::vector<std::string> run_builder(const std::vector<std::string> &f, long &items) {
    items = 1;
    const std::string &kind = f[2];
    if (kind == "spline") {
        const auto pts = vecs(f[7]);
        const auto r = R::spline_eval(hex(f[3]), pts, static_cast<std::int32_t>(hex(f[4])), hex(f[5]) != 0,
                                      hex(f[6]) != 0);
        return {r.pos ? hl(*r.pos) : "-", r.tangent ? hl(*r.tangent) : "-"};
    }
    R::TrailStore ts;
    const auto h = ts.create(R::ONE, 0, 4, kind == "sweep" ? 2 : 0, 1, 2, 3, 4);
    ts.trails[h & 0xFF].lifetime = 77;
    ts.add_point(h, {R::ONE, 0, 0});            // something in the ring first
    auto ring_ok = [&](const std::vector<R::TrailPoint> &want, std::int32_t clock, std::int32_t life) {
        const R::Trail &t = ts.trails[h & 0xFF];
        if (t.oldest != 0 || t.newest != want.size() || t.clock != clock || t.lifetime != life)
            return false;
        for (std::size_t k = 0; k < want.size(); ++k)
            if (t.points[k] != want[k])
                return false;
        return true;
    };
    if (kind == "setspline") {
        const auto pts = vecs(f[7]);
        const auto count = static_cast<std::int32_t>(hex(f[5])), steps = static_cast<std::int32_t>(hex(f[6]));
        const auto s = R::trail_set_spline(hex(f[3]), hex(f[4]), pts, count, steps);
        std::string tag;
        std::vector<R::TrailPoint> want;
        for (std::size_t k = 0; k < s.points.size(); ++k)
            want.push_back(R::trail_point(s.points[k], static_cast<std::int32_t>(k + 1)));
        if (want.size() <= 126) {
            ts.set_spline(hex(f[3]), hex(f[4]), h, pts, count, steps);
            if (!ring_ok(want, static_cast<std::int32_t>(want.size()), s.lifetime))
                tag = "!store";
        } else if (!refuses([&] { ts.set_spline(hex(f[3]), hex(f[4]), h, pts, count, steps); })) {
            tag = "!refusal";
        }
        items = static_cast<long>(s.points.size());
        return {hx(static_cast<std::uint16_t>(s.lifetime)) + tag, flat(s.points)};
    }
    if (kind == "circle") {
        const auto steps = static_cast<std::int32_t>(hex(f[6]));
        const auto out = R::trail_set_circle(hex(f[3]), v3(f[4]), v3(f[5]), steps);
        ts.set_circle(hex(f[3]), h, v3(f[4]), v3(f[5]), steps);
        std::vector<R::TrailPoint> want;
        for (std::size_t k = 0; k < out.size(); ++k)
            want.push_back(R::trail_point(out[k], static_cast<std::int32_t>(k + 1)));
        items = static_cast<long>(out.size());
        return {flat(out) + (ring_ok(want, static_cast<std::int32_t>(want.size()), 77) ? "" : "!store")};
    }
    if (kind == "sweep") {
        const auto steps = static_cast<std::int32_t>(hex(f[7]));
        const auto s = R::trail_sweep_pairs(v3(f[3]), v3(f[4]), v3(f[5]), v3(f[6]), steps);
        std::vector<Raw> w;
        std::vector<R::TrailPoint> want;
        for (std::size_t k = 0; k < s.pairs.size(); ++k) {
            const auto &p = s.pairs[k];
            w.insert(w.end(), p.first.begin(), p.first.end());
            w.insert(w.end(), p.second.begin(), p.second.end());
            const auto tp = R::trail_pair(R::ONE, p.first, p.second, static_cast<std::int32_t>(k));
            want.push_back(tp[0]);
            want.push_back(tp[1]);
        }
        ts.sweep_pairs(h, v3(f[3]), v3(f[4]), v3(f[5]), v3(f[6]), steps);
        const bool ok = ring_ok(want, static_cast<std::int32_t>(s.pairs.size()),
                                static_cast<std::int16_t>(s.lifetime));
        items = static_cast<long>(s.pairs.size());
        return {hx(static_cast<std::uint32_t>(s.lifetime)) + (ok ? "" : "!store"), hl(w)};
    }
    if (kind == "pair") {
        const auto clock = static_cast<std::int16_t>(hex(f[6]));
        const auto out = R::trail_pair(hex(f[3]), v3(f[4]), v3(f[5]), clock);
        // the store's addPair: the same two entries, stamped from the trail clock
        R::Trail &t = ts.trails[h & 0xFF];
        t.clock = clock;
        ts.add_pair(hex(f[3]), h, v3(f[4]), v3(f[5]));
        const bool ok = t.newest == 3 && t.points[1] == out[0] && t.points[2] == out[1];
        return {pt(out[0]) + ";" + pt(out[1]) + (ok ? "" : "!store")};
    }
    throw std::runtime_error("unknown builder " + kind);
}

core::Fpu::Model model_of(const std::string &m) {
    if (m == "chop")
        return core::Fpu::Model::Chop;
    return core::Fpu::Model::Ps2;
}

int run_vectors(const std::string &dir) {
    std::ifstream in(dir + "/fx_render.tsv");
    if (!in) {
        std::cerr << "FAIL: cannot open " << dir << "/fx_render.tsv\n";
        return 1;
    }
    const std::map<std::string, std::pair<Runner, int>> runners = {
        {"vu", {run_vu, 1}},           {"proj", {run_proj, 4}},     {"blocks", {run_blocks, 1}},
        {"particles", {run_particles, 2}}, {"trail", {run_trail, 3}}, {"beam", {run_beam, 3}},
        {"beamfn", {run_beamfn, 5}},   {"glows", {run_glows, 3}},   {"builder", {run_builder, 0}}};
    std::map<std::string, Stat> stats;
    std::string line;
    int shown = 0;
    long bad_rows = 0;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        const auto f = split(line, '\t');
        std::string name = f[0];
        if ((name == "particles" || name == "trail") && f[2] == "c")
            name += " vu1cpp";
        if (f[1] != "-")
            name += f[1] == "chop" ? " [chop]" : "";
        Stat &st = stats[name];
        ++st.rows;
        const auto it = runners.find(f[0]);
        bool ok = false;
        std::string detail;
        try {
            if (it == runners.end())
                throw std::runtime_error("unknown property");
            core::set_fpu(f[1] == "-" ? core::Fpu::Model::Ps2 : model_of(f[1]));
            long items = 0;
            const auto got = it->second.first(f, items);
            st.items += items;
            const std::size_t first = f.size() - got.size();
            ok = true;
            for (std::size_t k = 0; k < got.size(); ++k)
                if (got[k] != f[first + k]) {
                    ok = false;
                    detail = "column " + std::to_string(first + k) + ": got " + got[k].substr(0, 300) +
                             " want " + f[first + k].substr(0, 300);
                    break;
                }
        } catch (const std::exception &e) {
            detail = std::string("exception: ") + e.what();
        }
        core::set_fpu();
        if (!ok) {
            ++st.bad;
            ++bad_rows;
            if (shown++ < 8)
                std::cerr << "MISMATCH " << name << " row " << st.rows << ": " << detail << "\n";
        }
    }
    for (const auto &kv : stats)
        std::printf("%-22s %6ld rows %8ld items %4ld mismatches\n", kv.first.c_str(), kv.second.rows,
                    kv.second.items, kv.second.bad);
    return bad_rows ? 1 : 0;
}

} // namespace

int main(int argc, char **argv) {
    test_vu_arithmetic();
    test_particles();
    test_trails();
    test_beams();
    test_glows();
    test_builders();
    test_haze_refusal();
    test_trail_store();
    test_beam_store();
    test_glow_queue_and_order();
    test_frame();
    const int unit_failures = failures;
    std::printf("unit checks: %d failures\n", unit_failures);
    int vec = 0;
    if (argc > 1)
        vec = run_vectors(argv[1]);
    else
        std::printf("no vector directory given: vectors skipped\n");
    return (unit_failures || vec) ? 1 : 0;
}
