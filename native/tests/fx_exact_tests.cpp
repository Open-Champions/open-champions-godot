// champions-fx-exact tests: the synthetic assertions of the research tests
// (test_ref_particles.py, test_ref_decals.py, test_ref_fx_props.py, test_ref_lighting.py at
// research commit 19f80a6) and a replay of native/tests/vectors/fx_exact.tsv, made by
// tools/generate_fx_exact_vectors.py from the pinned references on synthetic inputs.
//
//   champions-fx-exact-tests VECTOR_DIR                 run everything
//   champions-fx-exact-tests VECTOR_DIR --dump PROP CASE print one case's state lines
#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
#include "champions/fx_decals.hpp"
#include "champions/fx_exact.hpp"
#include "champions/fx_props.hpp"
#include "champions/fx_rz.hpp"
#include "champions/fx_trig.hpp"
#include "champions/lighting.hpp"
// The lighting corrections live in the round-1 library champions-native-fx (linked).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace X = champions::rta::fx::exact;
namespace D = champions::rta::fx::decals;
namespace F = champions::rta::fx::props;
namespace L = champions::rta::lighting;
namespace C = champions::simulation::rta;
namespace R = champions::rta::fx::rounding;
using Raw = C::Raw;

namespace {

// ---------------------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), for the row digests.
class Sha256 {
public:
    Sha256() { reset(); }
    void reset() {
        static const std::uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::memcpy(h_, init, sizeof h_);
        len_ = 0;
        fill_ = 0;
    }
    void update(const std::string &s) {
        for (unsigned char c : s) {
            buf_[fill_++] = c;
            if (fill_ == 64) {
                block();
                fill_ = 0;
            }
        }
        len_ += s.size();
    }
    std::string hex() {
        const std::uint64_t bits = len_ * 8;
        buf_[fill_++] = 0x80;
        if (fill_ > 56) {
            while (fill_ < 64)
                buf_[fill_++] = 0;
            block();
            fill_ = 0;
        }
        while (fill_ < 56)
            buf_[fill_++] = 0;
        for (int i = 7; i >= 0; --i)
            buf_[fill_++] = static_cast<unsigned char>(bits >> (8 * i));
        block();
        static const char *digits = "0123456789abcdef";
        std::string out;
        for (std::uint32_t w : h_)
            for (int i = 28; i >= 0; i -= 4)
                out.push_back(digits[(w >> i) & 15]);
        return out;
    }

private:
    std::uint32_t h_[8];
    unsigned char buf_[64];
    std::uint64_t len_;
    std::size_t fill_;
    static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void block() {
        static const std::uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
            0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
            0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
            0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
            0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
            0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
            0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
            0xc67178f2};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (std::uint32_t(buf_[4 * i]) << 24) | (std::uint32_t(buf_[4 * i + 1]) << 16) |
                   (std::uint32_t(buf_[4 * i + 2]) << 8) | buf_[4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
        h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }
};

// ---------------------------------------------------------------------------------------
// State lines, as the generator writes them.
struct Line {
    std::string s;
    bool first = true;
    explicit Line(const char *tag) : s(tag) { s.push_back(' '); }
    Line &i(long long v) {
        if (!first)
            s.push_back(',');
        first = false;
        s += std::to_string(v);
        return *this;
    }
    Line &u(unsigned long long v) {
        if (!first)
            s.push_back(',');
        first = false;
        s += std::to_string(v);
        return *this;
    }
    Line &d(double x) { return u(R::double_bits(x)); }       // binary64 pattern
    Line &f(double x) { return u(R::float_bits(static_cast<float>(x))); }  // binary32 pattern
};
struct Lines {
    std::vector<std::string> lines;
    void add(const Line &l) { lines.push_back(l.s); }
    std::string digest() const {
        Sha256 h;
        for (const auto &l : lines)
            h.update(l + "\n");
        return h.hex().substr(0, 16);
    }
};

// ---------------------------------------------------------------------------------------
// Parsing helpers.
std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}
std::vector<std::string> split_nonempty(const std::string &s, char sep) {
    if (s.empty())
        return {};
    return split(s, sep);
}
long long ll(const std::string &s) { return std::stoll(s); }
unsigned long long ull(const std::string &s) { return std::stoull(s); }
double hexd(const std::string &h) { return R::bits_double(std::stoull(h, nullptr, 16)); }
Raw hexf(const std::string &h) { return static_cast<Raw>(std::stoul(h, nullptr, 16)); }
double f32val(Raw p) { return static_cast<double>(R::bits_float(p)); }
std::vector<long long> ints(const std::string &s) {
    std::vector<long long> out;
    for (const auto &t : split_nonempty(s, ','))
        out.push_back(ll(t));
    return out;
}
std::vector<double> hexds(const std::string &s) {
    std::vector<double> out;
    for (const auto &t : split_nonempty(s, ','))
        out.push_back(hexd(t));
    return out;
}
X::Vec3d v3(const std::vector<double> &v) { return {v.at(0), v.at(1), v.at(2)}; }
std::vector<std::uint8_t> unhex(const std::string &h) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(std::stoul(h.substr(i, 2), nullptr, 16)));
    return out;
}
std::uint32_t lcg_next(std::uint32_t s) { return s * 0x19660Du + 0x3C6EF35Fu; }

struct DefArgs {
    double s0 = 0, s1 = 0, grav = 0;
    std::array<int, 4> start{}, end{};
    int life = 0, kind = 0, tile = 0;
};
DefArgs def_args(const std::string &token) {
    const auto f = split(token, ':');
    DefArgs a;
    a.s0 = hexd(f.at(0));
    a.s1 = hexd(f.at(1));
    a.grav = hexd(f.at(2));
    const auto st = ints(f.at(3)), en = ints(f.at(4));
    for (std::size_t i = 0; i < 4; ++i) {
        a.start[i] = static_cast<int>(st.at(i));
        a.end[i] = static_cast<int>(en.at(i));
    }
    a.life = static_cast<int>(ll(f.at(5)));
    a.kind = static_cast<int>(ll(f.at(6)));
    a.tile = static_cast<int>(ll(f.at(7)));
    return a;
}
X::ParticleDef build(const DefArgs &a, const X::ParticleDef *previous = nullptr) {
    return X::ParticleDef::build(a.s0, a.s1, a.grav, a.start, a.end, a.life, a.kind, a.tile, 0, previous);
}

// ---------------------------------------------------------------------------------------
// Replays. Each takes the row's fields and fills the state lines.
void def_lines(const X::ParticleDef &d, Lines &out) {
    Line l("D");
    l.i(d.list_index).i(d.slot).i(d.live).i(d.callback).i(d.kind).i(d.life_class).i(d.tile).i(d.life);
    for (auto w : d.record())
        l.u(w);
    out.add(l);
    for (int age : {0, 1, 7, d.life / 2, d.life - 1, d.life, d.life + 1, 100, -3}) {
        Line a("A");
        a.i(age);
        for (int c : d.colour_at(age))
            a.i(c);
        a.d(d.size_at(age));
        out.add(a);
    }
}

void replay_rz(const std::vector<std::string> &f, Lines &out) {
    const double x = hexd(f.at(2));
    const double fv = (x == x && std::fabs(x) < 3.4e38) ? R::f32(x) : 0.0;
    out.add(Line("R").d(R::rz(x)).d(fv));
}

void replay_def(const std::vector<std::string> &f, Lines &out) {
    DefArgs a;
    a.s0 = hexd(f.at(2));
    a.s1 = hexd(f.at(3));
    a.grav = hexd(f.at(4));
    const auto st = ints(f.at(5)), en = ints(f.at(6));
    for (std::size_t i = 0; i < 4; ++i) {
        a.start[i] = static_cast<int>(st.at(i));
        a.end[i] = static_cast<int>(en.at(i));
    }
    a.life = static_cast<int>(ll(f.at(7)));
    a.kind = static_cast<int>(ll(f.at(8)));
    a.tile = static_cast<int>(ll(f.at(9)));
    X::ParticleDef prev;
    const X::ParticleDef *pp = nullptr;
    if (f.at(10) != "-") {
        const auto p = ints(f.at(10));
        prev.v0 = static_cast<std::int32_t>(p.at(0));
        prev.u1 = static_cast<std::int32_t>(p.at(1));
        prev.v1 = static_cast<std::int32_t>(p.at(2));
        pp = &prev;
    }
    def_lines(build(a, pp), out);
}

void replay_defreset(const std::vector<std::string> &, Lines &out) {
    def_lines(X::ParticleDef::reset_default(), out);
}

void replay_spawn(const std::vector<std::string> &f, Lines &out) {
    const X::ParticleDef d = build(def_args(f.at(2)));
    const int clock = static_cast<int>(ll(f.at(3)));
    const auto pos = v3(hexds(f.at(4))), vel = v3(hexds(f.at(5)));
    const double back = hexd(f.at(6));
    const int slot = static_cast<int>(ll(f.at(7)));
    const X::Particle p = X::spawn_record(d, pos, vel, clock, back, slot);
    out.add(Line("S").i(p.x).i(p.y).i(p.z).i(p.birth).i(p.vx).i(p.vy).i(p.vz).i(p.type4));
    for (int age : {0, 5, d.life, d.life + 1}) {
        const X::View v = X::evaluate(p, d, p.birth + age);
        Line l("V");
        l.i(age).d(v.pos[0]).d(v.pos[1]).d(v.pos[2]).d(v.size);
        for (int c : v.rgba)
            l.i(c);
        l.i(v.age).i(X::expired(p, d, p.birth + age) ? 1 : 0);
        out.add(l);
    }
    if (d.kind == X::KIND_LINE) {
        const auto e = X::line_endpoints(p, d, p.birth + 7);
        Line l("L");
        for (double c : e.first)
            l.d(c);
        for (double c : e.second)
            l.d(c);
        out.add(l);
    }
    const auto ext = X::screen_half_extent(hexd(f.at(8)), hexd(f.at(9)));
    out.add(Line("X").d(ext.first).d(ext.second));
}

void pool_state(const X::ParticleSystem &m, const std::vector<X::ParticleDef> &defs, Lines &out, bool full) {
    Line l("P");
    l.i(m.clock()).i(m.dropped()).i(m.group_seq()).i(m.current_set()).i(static_cast<long long>(m.free_pages().size()));
    for (int pg : m.free_pages())
        l.i(pg);
    for (int s = 0; s < X::SLOT_COUNT; ++s)
        l.i(m.slot_used(s) ? 1 : 0);
    for (const auto &d : defs)
        l.i(d.slot).i(d.live);
    auto lists = [&](const X::ListSet &set) {
        for (const auto &lst : set) {
            l.i(static_cast<long long>(lst.pages.size()));
            for (int pg : lst.pages)
                l.i(pg);
            l.i(lst.head).i(lst.tail);
        }
    };
    lists(m.global_set());
    for (int i = 0; i < X::GROUP_COUNT; ++i)
        lists(m.group_lists(i));
    l.i(static_cast<long long>(m.groups().size()));
    for (const auto &g : m.groups()) {
        l.i(g.handle).i(g.idle).i(g.handle & 0xff);
        for (double x : g.matrix)
            l.f(x);
    }
    out.add(l);
    if (!full)
        return;
    Line c("C");
    auto cells = [&](const X::ListSet &set) {
        for (const auto &lst : set)
            for (const auto &pc : X::ParticleSystem::list_cells(lst)) {
                c.i(pc.first).i(pc.second);
                for (auto v : m.cell(pc.first, pc.second))
                    c.i(v);
            }
    };
    cells(m.global_set());
    for (int i = 0; i < X::GROUP_COUNT; ++i)
        cells(m.group_lists(i));
    out.add(c);
    Line w("W");
    for (const auto &pv : m.visible()) {
        const X::Particle &p = pv.first.particle;
        const X::View &v = pv.second;
        w.i(p.x).i(p.y).i(p.z).i(p.birth).i(p.vx).i(p.vy).i(p.vz).i(p.type4);
        w.d(v.pos[0]).d(v.pos[1]).d(v.pos[2]).d(v.size);
        for (int ch : v.rgba)
            w.i(ch);
        w.i(v.age);
    }
    out.add(w);
}

struct Counts {
    long long ops = 0, spawns = 0;
};

void replay_pool(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    X::ParticleSystem m;
    m.set_clock(static_cast<int>(ll(f.at(2))));
    std::vector<X::ParticleDef> defs;
    for (const auto &t : split(f.at(3), ';'))
        defs.push_back(build(def_args(t)));
    const auto ops = split_nonempty(f.at(4), ';');
    long long n = 0;
    for (const auto &op : ops) {
        const char k = op.at(0);
        const std::string rest = op.substr(1);
        if (k == 's') {
            const auto t = split(rest, ',');
            const std::size_t i = static_cast<std::size_t>(ll(t.at(0)));
            const long long burst = ll(t.at(1));
            const X::Vec3d pos{double(ll(t.at(2))), double(ll(t.at(3))), double(ll(t.at(4)))};
            const X::Vec3d vel{double(ll(t.at(5))), double(ll(t.at(6))), double(ll(t.at(7)))};
            const double back = t.at(8) == "0" ? 0.0 : hexd(t.at(8));
            for (long long b = 0; b < burst; ++b)
                m.spawn(defs.at(i), pos, vel, back);
            counts.spawns += burst;
        } else if (k == 'u') {
            const auto t = ints(rest);
            for (long long j = 0; j < t.at(0); ++j)
                m.update(t.at(1) != 0);
        } else if (k == 'c') {
            m.group_create();
        } else if (k == 'b') {
            m.group_begin(ll(rest));
        } else if (k == 'e') {
            m.group_end(ll(rest));
        } else if (k == 'f') {
            m.group_free(ll(rest));
        } else if (k == 'm') {
            const auto t = split(rest, ',');
            X::Mat34d mat{};
            for (std::size_t j = 0; j < 12; ++j)
                mat[j] = f32val(hexf(t.at(1 + j)));
            m.group_set_matrix(ll(t.at(0)), mat);
        } else {
            throw std::runtime_error("unknown pool op " + op);
        }
        ++n;
        pool_state(m, defs, out, n % 64 == 0);
    }
    pool_state(m, defs, out, true);
    counts.ops += n;
}

void replay_emitdef(const std::vector<std::string> &f, Lines &out) {
    const auto b = unhex(f.at(2));
    const X::EmitterRecord back = X::EmitterRecord::parse(b.data(), b.size());
    Line e("E");
    for (auto c : back.pack())
        e.i(c);
    e.i(static_cast<long long>(back.name.size()));
    out.add(e);
    def_lines(X::ParticleDef::from_emitter_record(back), out);
    X::EmitterRecord other = X::EmitterRecord::parse(b.data(), b.size());
    const int k = static_cast<int>(ll(f.at(3)));
    if (k == 0)
        other.rate = static_cast<int>(ll(f.at(4)));
    else if (k == 1)
        other.tile = (other.tile + 1) & 0xFF;
    else if (k == 2)
        other.start_rgba[0] ^= 1;
    else
        other.name = "other";
    Line q("Q");
    q.i(back.same_definition(other) ? 1 : 0);
    for (auto c : other.pack())
        q.i(c);
    out.add(q);
}

void replay_flipbook(const std::vector<std::string> &f, Lines &out) {
    X::ParticleDef d = build(def_args(f.at(2)));
    for (long long frame : ints(f.at(3))) {
        const bool s = X::flipbook_step(d, frame);
        out.add(Line("F").i(s ? 1 : 0).i(d.has_u0() ? d.u0 : -1).i(d.v0).i(d.u1).i(d.v1));
    }
}

// Every angle_cos_sin argument, 4096 a row. The digest is math_cos16 / math_sin16
// promoted to binary64 (generate_fx_exact_vectors.py, ref_core.cos16).
void replay_trig(const std::vector<std::string> &f, Lines &out) {
    const long long block = ll(f.at(1));
    for (long long k = -32768 + block * 4096; k < -32768 + (block + 1) * 4096; ++k) {
        const auto cs = X::angle_cos_sin(k);
        out.add(Line("C").i(k).d(cs.first).d(cs.second));
    }
}

void replay_emitter(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    const auto b = unhex(f.at(2));
    X::EmitterRecord rec = X::EmitterRecord::parse(b.data(), b.size());
    const auto pos = v3(hexds(f.at(3)));
    const auto flips = ints(f.at(6));
    X::CosmeticRng rng(static_cast<std::uint32_t>(ull(f.at(9))));
    for (long long now : ints(f.at(4))) {
        const auto got = X::emitter_run(rec, pos, now, rng, ll(f.at(5)), flips.at(0) != 0, flips.at(1) != 0,
                                        flips.at(2) != 0, ll(f.at(7)), ll(f.at(8)) != 0);
        out.add(Line("T").i(rec.next).i(rec.on).u(rng.seed).i(static_cast<long long>(got.size())));
        for (const auto &e : got) {
            Line o("O");
            for (double c : e.pos)
                o.d(c);
            for (double c : e.vel)
                o.d(c);
            out.add(o);
        }
        counts.spawns += static_cast<long long>(got.size());
        ++counts.ops;
    }
}

void decal_state(const D::DecalStore &st, bool made, Lines &out) {
    Line l("G");
    l.i(made ? 1 : 0).i(st.total()).i(static_cast<long long>(st.free_blocks().size()));
    for (int b : st.free_blocks())
        l.i(b);
    std::vector<std::pair<int, int>> cells;
    for (int cy = 0; cy < D::GRID; ++cy)
        for (int cx = 0; cx < D::GRID; ++cx)
            if (st.cell_block(cx, cy) >= 0)
                cells.emplace_back(cx, cy);
    l.i(static_cast<long long>(cells.size()));
    for (const auto &c : cells) {
        const D::Block &b = st.block(st.cell_block(c.first, c.second));
        l.i(c.first).i(c.second).i(b.index).i(b.count).i(b.zmin).i(b.zmax).i(b.counted);
        for (int i = 0; i < b.count; ++i)
            for (auto v : b.decals[static_cast<std::size_t>(i)].record())
                l.i(v);
    }
    for (int i = 0; i < D::GROW_SLOTS; ++i) {
        const auto &g = st.growers()[static_cast<std::size_t>(i)];
        if (g)
            l.i(i).i(g->mode).i(g->block).i(g->index).i(g->angle).u(g->size).u(g->final_size);
    }
    out.add(l);
}

void replay_decals(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    D::DecalStore st(ll(f.at(2)));
    for (const auto &op : split_nonempty(f.at(3), ';')) {
        if (op.at(0) == 'a') {
            const auto t = split(op.substr(1), ',');
            const double size = f32val(hexf(t.at(0)));
            const std::array<double, 3> pos{f32val(hexf(t.at(1))), f32val(hexf(t.at(2))), f32val(hexf(t.at(3)))};
            const long long tint = ll(t.at(7));
            const D::Decal *d = st.add(size, pos, ll(t.at(4)), ll(t.at(5)), ll(t.at(6)),
                                       {tint & 0xFF, (tint >> 8) & 0xFF, (tint >> 16) & 0xFF});
            decal_state(st, d != nullptr, out);
        } else {
            st.grow_step(op.at(1) == '1');
            decal_state(st, false, out);
        }
        ++counts.ops;
    }
}

void replay_decalgate(const std::vector<std::string> &f, Lines &out) {
    const auto c = D::cell_of(hexd(f.at(4)), hexd(f.at(5)));
    out.add(Line("K").i(D::allowed(ll(f.at(2)), ll(f.at(3))) ? 1 : 0).i(c ? c->first : -1).i(c ? c->second : -1));
}

C::NoiseTable noise_table(std::uint32_t seed) {
    C::NoiseTable t{};
    std::uint32_t s = seed;
    for (auto &v : t) {
        s = lcg_next(s);
        v = static_cast<std::int16_t>(static_cast<std::uint16_t>(s >> 16));
    }
    return t;
}

void replay_props(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    static const std::map<std::string, F::FireClass> classes{{"torch", F::FireClass::Torch},
                                                             {"candle", F::FireClass::Candle},
                                                             {"candle3", F::FireClass::Candle3},
                                                             {"lantern", F::FireClass::Lantern},
                                                             {"lamp", F::FireClass::Lamp}};
    const auto pos = split(f.at(4), ',');
    F::FireProp p(classes.at(f.at(2)), static_cast<std::uint32_t>(ull(f.at(3))),
                  {hexf(pos.at(0)), hexf(pos.at(1)), hexf(pos.at(2))}, static_cast<std::uint32_t>(ull(f.at(5))),
                  static_cast<int>(ll(f.at(6))), ll(f.at(7)) != 0, f.at(8));
    const C::NoiseTable table = noise_table(static_cast<std::uint32_t>(ull(f.at(9))));
    p.undrawn = static_cast<std::uint8_t>(ll(f.at(10)));
    long long frame = ll(f.at(11));
    const long long ticks = ll(f.at(12));
    for (long long t = 0; t < ticks; ++t) {
        frame += 1;
        const F::Tick tick = F::run(p, frame, table);
        Line l("T");
        l.i(static_cast<long long>(tick.spawns.size()));
        for (const auto &s : tick.spawns) {
            l.u(s.backdate).u(s.definition);
            for (Raw v : s.position)
                l.u(v);
            for (Raw v : s.velocity)
                l.u(v);
        }
        l.i(tick.stopped ? 1 : 0).u(p.rng.state).i(p.undrawn).u(p.model_radius).u(p.model_falloff)
            .u(p.level_radius).u(p.level_falloff);
        for (Raw v : p.light_pos)
            l.u(v);
        l.i(p.counter).i(p.running ? 1 : 0);
        out.add(l);
        counts.spawns += static_cast<long long>(tick.spawns.size());
        ++counts.ops;
    }
}

L::Level level_from(const std::vector<std::string> &f, std::size_t at) {
    const auto d = hexds(f.at(at));
    const auto sun = ints(f.at(at + 1)), amb = ints(f.at(at + 2));
    return L::Level::make({d.at(0), d.at(1), d.at(2)},
                          {int(sun.at(0)), int(sun.at(1)), int(sun.at(2)), int(sun.at(3))},
                          {int(amb.at(0)), int(amb.at(1)), int(amb.at(2)), int(amb.at(3))},
                          static_cast<int>(ll(f.at(at + 3))), hexd(f.at(at + 4)), static_cast<int>(ll(f.at(at + 5))));
}

void obj_line(const L::Object &o, Lines &out) {
    Line l("J");
    for (int v : o.ambient) l.i(v);
    for (int v : o.sun) l.i(v);
    for (int v : o.fill) l.i(v);
    for (double v : o.point_factors) l.f(v);
    out.add(l);
}

void replay_objlight(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    const L::Level lv = level_from(f, 2);
    const auto sc = lv.scaled();
    Line y("Y");
    for (int v : sc.first) y.i(v);
    for (int v : sc.second) y.i(v);
    const auto fd = lv.fill_direction();
    y.d(lv.sun_direction.x).d(lv.sun_direction.y).d(lv.sun_direction.z).d(fd.x).d(fd.y).d(fd.z);
    out.add(y);
    L::Object obj = L::Object::reset(lv);
    obj_line(obj, out);
    for (const auto &op : split_nonempty(f.at(8), ';')) {
        const std::string rest = op.substr(1);
        switch (op.at(0)) {
        case 'a': { const auto b = ints(rest); obj.add_ambient({int(b[0]), int(b[1]), int(b[2]), int(b[3])}); break; }
        case 's': { const auto b = ints(rest); obj.set_ambient({int(b[0]), int(b[1]), int(b[2]), int(b[3])}); break; }
        case 'k': { const auto k = hexds(rest); obj.scale(k.at(0), k.at(1), k.at(2)); break; }
        case 'c': obj.chill(hexd(rest)); break;
        case 'p': obj.poison(hexd(rest)); break;
        case 'h': obj.hit_flash(); break;
        case 'r': obj = L::Object::reset(lv); break;
        default: throw std::runtime_error("unknown object light op " + op);
        }
        obj_line(obj, out);
        ++counts.ops;
    }
}

void level_marker_line(const L::Level &lv, Lines &out) {
    Line m("M");
    for (int v : lv.ambient) m.i(v);
    for (int v : lv.sun) m.i(v);
    m.d(lv.sun_direction.x).d(lv.sun_direction.y).d(lv.sun_direction.z).i(lv.base);
    out.add(m);
}

void replay_markers(const std::vector<std::string> &f, Lines &out) {
    auto vec = [&](const std::string &s, L::Vec3 &v) -> const L::Vec3 * {
        if (s == "-") return nullptr;
        const auto d = hexds(s);
        v = {d.at(0), d.at(1), d.at(2)};
        return &v;
    };
    L::Vec3 a, s, d;
    const L::Vec3 *pa = vec(f.at(2), a), *ps = vec(f.at(3), s), *pd = vec(f.at(4), d);
    level_marker_line(L::Level::from_markers(pa, ps, pd, static_cast<unsigned>(ull(f.at(5)))), out);
}

void replay_boot(const std::vector<std::string> &, Lines &out) { level_marker_line(L::Level::boot(), out); }

void replay_lightslots(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    std::vector<L::Chunk> chunks;
    for (const auto &t : split(f.at(2), ';')) {
        const auto v = ints(t);
        L::Chunk c;
        c.low = int(v.at(0)); c.high = int(v.at(1)); c.mode = int(v.at(2)); c.period = int(v.at(3));
        c.rgba = {int(v.at(4)), int(v.at(5)), int(v.at(6)), int(v.at(7))};
        chunks.push_back(c);
    }
    int frame = static_cast<int>(ll(f.at(3)));
    long long draw = ll(f.at(4));
    X::CosmeticRng rng(static_cast<std::uint32_t>(ull(f.at(5))));
    const std::function<double()> rnd = [&rng] { return rng.r(); };
    L::LightSlots slots;
    for (const auto &st : split_nonempty(f.at(6), ';')) {
        const auto t = split(st, ',');
        frame += static_cast<int>(ll(t.at(0)));
        draw += 1;
        const unsigned long long mask = std::stoull(t.at(1), nullptr, 16);
        for (std::size_t k = 0; k < chunks.size(); ++k) {
            if (!((mask >> k) & 1u))
                continue;
            L::Chunk &lc = chunks[k];
            const auto r = slots.run(lc, frame, draw, rnd);
            Line h("H");
            h.i(static_cast<long long>(k)).i(r.first).i(lc.timer).i(lc.down ? 1 : 0).i(lc.radius);
            if (!r.second) {
                h.i(0);
            } else {
                const L::Node &n = *r.second;
                h.i(1).d(n.model_radius).d(n.model_falloff).d(n.world_radius);
                for (int c : n.model_rgb) h.i(c);
                for (int c : n.world_rgb) h.i(c);
            }
            out.add(h);
            ++counts.ops;
        }
    }
    Line z("Z");
    z.u(rng.seed);
    for (const auto &c : chunks) {
        const auto it = slots.slot_of.find(&c);
        z.i(it == slots.slot_of.end() ? -1 : it->second);
    }
    for (int r : slots.radius) z.i(r);
    for (auto s : slots.stamp) z.i(s);
    out.add(z);
}

void replay_lighteffect(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    const auto s = hexds(f.at(2)), e = hexds(f.at(3));
    const auto pos = hexds(f.at(10));
    L::LightEffect le = L::LightEffect::from_floats(
                      std::array<double, 4>{s.at(0), s.at(1), s.at(2), s.at(3)},
                      std::array<double, 4>{e.at(0), e.at(1), e.at(2), e.at(3)}, hexd(f.at(4)),
                      hexd(f.at(5)), hexd(f.at(7)), static_cast<int>(ll(f.at(11))), ll(f.at(8)) != 0, hexd(f.at(6)),
                      {pos.at(0), pos.at(1), pos.at(2)});
    Line b("B");
    b.i(le.life).i(le.end);
    for (double c : le.colour) b.d(c);
    for (double c : le.rate) b.d(c);
    b.d(le.radius).d(le.radius_rate);
    out.add(b);
    const bool has_owner = ll(f.at(9)) != 0;
    std::uint32_t os = static_cast<std::uint32_t>(ull(f.at(12)));
    int frame = static_cast<int>(ll(f.at(11)));
    for (int step = 0; step < 400; ++step) {
        L::Vec3 owner;
        double c3[3];
        for (double &c : c3) {
            os = lcg_next(os);
            c = R::f32(static_cast<double>(os >> 8) * (6000.0 / 16777216.0) - 3000.0);
        }
        owner = {c3[0], c3[1], c3[2]};
        frame += 1;
        L::Node node({}, {}, {}, 0);
        const bool alive = le.run(frame, node, has_owner ? &owner : nullptr);
        Line n("N");
        n.i(alive ? 1 : 0);
        for (int c : node.model_rgb) n.i(c);
        n.d(node.model_radius).d(node.model_falloff).d(node.pos.x).d(node.pos.y).d(node.pos.z);
        for (double c : le.colour) n.d(c);
        out.add(n);
        ++counts.ops;
        if (!alive)
            break;
    }
}

void replay_lightnodes(const std::vector<std::string> &f, Lines &out, Counts &counts) {
    const L::Level lv = level_from(f, 2);
    L::Object obj = L::Object::reset(lv);
    if (f.at(8) != "-") {
        const auto k = hexds(f.at(8));
        obj.scale(k.at(0), k.at(1), k.at(2));
    }
    std::vector<L::Node> nodes;
    for (const auto &t : split(f.at(9), ';')) {
        const auto v = split(t, ',');
        nodes.emplace_back(L::Vec3{hexd(v.at(0)), hexd(v.at(1)), hexd(v.at(2))},
                           L::Rgb{int(ll(v.at(5))), int(ll(v.at(6))), int(ll(v.at(7)))},
                           L::Rgb{int(ll(v.at(8))), int(ll(v.at(9))), int(ll(v.at(10)))}, hexd(v.at(3)),
                           hexd(v.at(4)), hexd(v.at(11)), int(ll(v.at(12))), ll(v.at(13)) != 0);
    }
    for (const auto &qt : split(f.at(10), ';')) {
        const auto q = hexds(qt);
        const L::Vec3 p{q.at(0), q.at(1), q.at(2)}, nrm{q.at(3), q.at(4), q.at(5)};
        Line u("U");
        for (const L::Node *n : L::collect_near(nodes, p, lv.layer))
            u.i(static_cast<long long>(n - nodes.data()));
        out.add(u);
        const auto ml = L::model_lights(p, nodes, obj, lv.layer);
        Line i("I");
        for (int c : ml.ambient) i.i(c);
        for (const auto &lt : ml.lights) {
            i.d(lt.direction.x).d(lt.direction.y).d(lt.direction.z);
            for (int c : lt.rgb) i.i(c);
        }
        out.add(i);
        Line o("O");
        for (int c : L::shade_model(nrm, ml)) o.i(c);
        out.add(o);
        Line v("V");
        for (int c : L::shade_world(lv.base, nodes, p, nrm)) v.i(c);
        out.add(v);
        Line t("T");
        for (int c : L::decal_tint(p, nodes, lv)) t.i(c);
        out.add(t);
        ++counts.ops;
    }
}

int run_vectors(const std::string &dir, const std::string *dump_prop, const std::string *dump_case) {
    std::ifstream in(dir + "/fx_exact.tsv");
    if (!in) {
        std::cerr << "FAIL: cannot open " << dir << "/fx_exact.tsv\n";
        return 1;
    }
    std::map<std::string, long long> rows, bad;
    std::map<std::string, Counts> counts;
    std::string line;
    long long mismatches = 0;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        const auto f = split(line, '\t');
        const std::string &prop = f.at(0);
        if (dump_prop && (prop != *dump_prop || f.at(1) != *dump_case))
            continue;
        Lines out;
        Counts &c = counts[prop];
        try {
            if (prop == "rz") replay_rz(f, out);
            else if (prop == "def") replay_def(f, out);
            else if (prop == "defreset") replay_defreset(f, out);
            else if (prop == "spawn") replay_spawn(f, out);
            else if (prop == "pool") replay_pool(f, out, c);
            else if (prop == "emitdef") replay_emitdef(f, out);
            else if (prop == "flipbook") replay_flipbook(f, out);
            else if (prop == "emitter") replay_emitter(f, out, c);
            else if (prop == "decals") replay_decals(f, out, c);
            else if (prop == "decalgate") replay_decalgate(f, out);
            else if (prop == "props") replay_props(f, out, c);
            else if (prop == "objlight") replay_objlight(f, out, c);
            else if (prop == "markers") replay_markers(f, out);
            else if (prop == "boot") replay_boot(f, out);
            else if (prop == "lightslots") replay_lightslots(f, out, c);
            else if (prop == "lighteffect") replay_lighteffect(f, out, c);
            else if (prop == "lightnodes") replay_lightnodes(f, out, c);
            else if (prop == "trig") replay_trig(f, out);
            else throw std::runtime_error("unknown property");
        } catch (const std::exception &e) {
            std::cerr << "FAIL: " << prop << " case " << f.at(1) << ": " << e.what() << "\n";
            ++mismatches;
            ++bad[prop];
            ++rows[prop];
            continue;
        }
        if (dump_prop) {
            for (const auto &l : out.lines)
                std::cout << l << "\n";
            return 0;
        }
        ++rows[prop];
        if (out.digest() != f.back()) {
            if (bad[prop] < 3)
                std::cerr << "FAIL: " << prop << " case " << f.at(1) << " digest " << out.digest()
                          << " expected " << f.back() << "\n";
            ++bad[prop];
            ++mismatches;
        }
    }
    if (dump_prop) {
        std::cerr << "no such case\n";
        return 1;
    }
    long long total = 0;
    for (const auto &kv : rows) {
        total += kv.second;
        std::cout << "  " << kv.first << ": " << kv.second << " rows, " << bad[kv.first] << " mismatches";
        const Counts &c = counts[kv.first];
        if (c.ops)
            std::cout << ", " << c.ops << " operations";
        if (c.spawns)
            std::cout << ", " << c.spawns << " spawns";
        std::cout << "\n";
    }
    std::cout << "vectors: " << total << " rows, " << mismatches << " mismatches\n";
    return mismatches ? 1 : 0;
}

// ---------------------------------------------------------------------------------------
// Ported synthetic assertions.
int assertions = 0;
int failures = 0;
void check(bool ok, const char *what) {
    ++assertions;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

void unit_tests();

// tools/generate_fx_trig_cases.py: emitter cases from the pinned ref_particles with
// math_cos16 in place of the host cosine (audit TRIG-1). Keys are "case/field".
std::string vector_dir;
std::map<std::string, std::string> trig_cases() {
    std::map<std::string, std::string> out;
    std::ifstream in(vector_dir + "/fx_trig_cases.tsv");
    std::string c, f, v;
    while (in >> c >> f >> v)
        out[c + "/" + f] = v;
    check(!out.empty(), "fx_trig_cases.tsv present");
    return out;
}
bool same_bits(double got, const std::map<std::string, std::string> &want, const std::string &key) {
    const auto it = want.find(key);
    return it != want.end() && R::double_bits(got) == std::stoull(it->second, nullptr, 16);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: champions-fx-exact-tests VECTOR_DIR [--dump PROPERTY CASE]\n";
        return 2;
    }
    if (argc == 5 && std::string(argv[2]) == "--dump") {
        const std::string p = argv[3], c = argv[4];
        return run_vectors(argv[1], &p, &c);
    }
    vector_dir = argv[1];
    unit_tests();
    std::cout << assertions << " synthetic assertions, " << failures << " failed\n";
    const int v = run_vectors(argv[1], nullptr, nullptr);
    return (failures || v) ? 1 : 0;
}

namespace {
// assertAlmostEqual(a, b, places=n): round(a - b, n) == 0.
void almost(double a, double b, int places, const char *what) {
    check(std::fabs(a - b) < 0.5 * std::pow(10.0, -places) * (1.0 + 1e-9), what);
}

X::ParticleDef make(int kind = X::KIND_ADD, int life_class = 1, int tile = 0x12, double s0 = 1.0, double s1 = 3.0,
                    double gravity = -2.56, std::array<int, 4> start = {0x80, 0x40, 0x20, 0x80},
                    std::array<int, 4> end = {0, 0x40, 0xff, 0}) {
    return X::ParticleDef::build(s0, s1, gravity, start, end, life_class, kind, tile);
}
std::vector<int> live_x(const X::ParticleSystem &s) {
    std::vector<int> out;
    for (const auto &lp : s.live())
        out.push_back(lp.particle.x);
    return out;
}

void particle_tests() {
    // Definition
    check(X::LIFE_TICKS == std::array<int, 4>{11, 30, 60, 90}, "life ticks");
    {
        auto d = make(X::KIND_BLEND, 2);
        check(d.life == 60 && d.list_index == 2, "life and list index");
        check(make(X::KIND_LINE, 3).list_index == 11, "line list index");
        auto c = make(7, -4);
        check(c.kind == 3 && c.life_class == 0 && c.life == 11, "clamps");
        auto t = make(X::KIND_ADD, 1, 0x12);
        check(t.u0 == 65 * 16 && t.v0 == 33 * 16 && t.u1 == 95 * 16 && t.v1 == 63 * 16, "tile coordinates");
        auto sz = make(X::KIND_ADD, 1, 0x12, 1.0, 3.0, -2.56);
        almost(sz.size0, 100.0, 7, "size0");
        almost(sz.size_rate, 200.0 / 30, 4, "size rate");
        almost(sz.gravity, -0.01, 6, "gravity");
        almost(sz.size_at(30), 300.0, 3, "size at 30");
        auto col = make(X::KIND_ADD, 1);
        check(col.colour_at(0) == std::array<int, 4>{0x80, 0x40, 0x20, 0x80}, "start colour");
        check(col.colour_at(30) == std::array<int, 4>{0, 0x40, 0xfe, 0}, "inexact rate ends one short");
        check(col.colour_at(60) == std::array<int, 4>{0, 0x40, 255, 0}, "colour clamped");
        check(col.c0[0] == 0x80 + 8388608.0, "colour bias");
        check(make(X::KIND_HIDDEN).c0[0] == 0x80, "hidden kind has no bias");
        bool threw = false;
        try {
            X::ParticleDef::build(1, 1, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, 3, 1, 1, 1);
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        check(threw, "callback rejected");
        auto line = make(X::KIND_LINE, 1, 0x12, 4.0, 9.0, 2.56);
        almost(line.line_lag, 4.0, 5, "line lag");
        almost(line.line_gravity, 0.01, 6, "line gravity");
        check(line.record().size() == 16 && line.record()[0] == R::pattern(line.line_gravity), "line record");
        auto rec = make(X::KIND_ADD, 1, 0x01).record();
        check(rec[0] == 33 * 16 && rec[1] == 16, "record u0 v0");
        almost(R::bits_float(rec[7]), 100.0, 7, "record size0");
        check(R::bits_float(rec[8]) == 0x80 + 8388608.0f, "record colour");
        auto def = X::ParticleDef::reset_default();
        check(def.kind == 1 && def.life_class == 3 && def.tile == 1 && def.life == 90, "default reset");
    }
    // Spawn
    {
        auto d = make();
        auto p = X::spawn_record(d, {10.7, -3.9, 5.0}, {1.0, -0.5, 0.25}, 100);
        check(p == X::Particle{10, -3, 5, 100, 256, -128, 64, 0}, "plain record");
        auto g = make(X::KIND_ADD, 1, 0x12, 1.0, 3.0, -25.6);
        auto b = X::spawn_record(g, {100.0, 200.0, 300.0}, {16.0, 0.0, 32.0}, 50, 0.25);
        check(b.birth == 50 - 15, "backdated birth");
        auto at = X::position_at(b, g, 15);
        check(std::fabs(at[0] - 100) <= 1 && std::fabs(at[1] - 200) <= 1 && std::fabs(at[2] - 300) <= 1,
              "backdated position now");
        auto m = X::spawn_record(g, {0.0, 0.0, 100.0}, {16.0, 0.0, 0.0}, 0);
        auto mp = X::position_at(m, g, 10);
        almost(mp[0], 10.0, 7, "motion x");
        almost(mp[2], 100.0 - 0.1 * 100, 4, "motion gravity");
        auto line = make(X::KIND_LINE, 1, 0x12, 4.0, 4.0, 0.0);
        auto lp = X::spawn_record(line, {0.0, 0.0, 0.0}, {16.0, 0.0, 0.0}, 0);
        auto e = X::line_endpoints(lp, line, 10);
        almost(e.second[0], 10.0, 7, "line head");
        almost(e.first[0], 6.0, 4, "line tail");
        auto ext = X::screen_half_extent(100.0, 50.0);
        check(ext.first == 4.0 && ext.second == 2.0, "screen extent");
    }
    // Pools
    {
        X::ParticleSystem s;
        auto d = make(X::KIND_ADD, 0);
        s.spawn(d, {0, 0, 0}, {0, 0, 0});
        for (int i = 0; i < 11; ++i)
            s.update();
        check(s.live().size() == 1, "age 11 = life still drawn");
        s.update();
        check(s.live().empty() && d.live == 0 && d.slot == -1, "retired at age 12");
        check(s.free_pages().size() == X::PAGE_COUNT, "pages returned");
    }
    {
        X::ParticleSystem s;
        auto d = make(X::KIND_ADD, 3);
        for (int i = 0; i < X::CELLS_PER_PAGE; ++i)
            s.spawn(d, {0, 0, 0}, {0, 0, 0});
        check(s.free_pages().size() == X::PAGE_COUNT - 1, "240 cells a page");
        s.spawn(d, {0, 0, 0}, {0, 0, 0});
        check(s.free_pages().size() == X::PAGE_COUNT - 2, "241st takes a page");
    }
    {
        X::ParticleSystem s;
        auto d = make(X::KIND_ADD, 3);
        const int total = X::PAGE_COUNT * X::CELLS_PER_PAGE;
        for (int i = 0; i < total + 5; ++i)
            s.spawn(d, {0, 0, 0}, {0, 0, 0});
        check(d.live == total && s.dropped() == 5, "pool exhaustion drops");
    }
    {
        X::ParticleSystem s;
        std::vector<X::ParticleDef> defs;
        for (int i = 0; i < X::SLOT_COUNT + 1; ++i)
            defs.push_back(make(X::KIND_ADD, 1, i));
        std::vector<bool> got;
        for (auto &x : defs)
            got.push_back(s.spawn(x, {0, 0, 0}, {0, 0, 0}).has_value());
        bool first31 = true;
        for (int i = 0; i < X::SLOT_COUNT - 1; ++i)
            first31 = first31 && got[static_cast<std::size_t>(i)];
        check(first31 && !got[X::SLOT_COUNT - 1] && !got.back(), "31 slots serve definitions");
        check(defs[0].slot == 1 && defs[1].slot == 2 && defs[2].slot == 3, "slots from 1");
        check(s.slot_used(0) && s.slot_record(0).held, "slot 0 held by 0x7e1d00");
    }
    {
        X::ParticleSystem s;
        auto d = make(X::KIND_ADD, 0);
        s.spawn(d, {1, 0, 0}, {0, 0, 0});
        for (int i = 0; i < 5; ++i)
            s.update();
        s.spawn(d, {2, 0, 0}, {0, 0, 0});
        for (int i = 0; i < 7; ++i)
            s.update();
        check(live_x(s) == std::vector<int>{2}, "FIFO per list");
    }
    {
        X::ParticleSystem s;
        auto h = s.group_create();
        check((h & 0xff) == 0, "first group slot");
        for (int i = 0; i < 31; ++i)
            s.update();
        check(s.group(h) != nullptr, "idle 31 kept");
        s.update();
        check(s.group(h) == nullptr, "idle 32nd tick frees");
    }
    {
        X::ParticleSystem s;
        auto h = s.group_create();
        for (int i = 0; i < 100; ++i)
            s.update(true);
        check(s.group(h) != nullptr, "cutscene holds idle count");
    }
    {
        X::ParticleSystem s;
        auto h = s.group_create();
        auto d = make(X::KIND_ADD, 3);
        s.group_begin(h);
        s.spawn(d, {0, 0, 0}, {0, 0, 0});
        s.group_end(h);
        s.group_free(h);
        check(d.live == 1 && d.slot == 1, "group free keeps slot");
        check(s.free_pages().size() == X::PAGE_COUNT, "group free returns pages");
    }
    {
        X::ParticleSystem s;
        auto h = s.group_create();
        auto d = make(X::KIND_ADD, 3);
        s.group_begin(h);
        s.group_free(h);
        s.spawn(d, {5, 0, 0}, {0, 0, 0});
        s.group_end(h);
        check(s.current_set() == 0, "stale end keeps the freed slot current");
        check(live_x(s).empty(), "freed group's particles not live");
        auto h2 = s.group_create();
        check((h2 & 0xff) == 0, "same slot reused");
        check(live_x(s) == std::vector<int>{5}, "lists inherited by the next group");
    }
    {
        X::ParticleSystem s;
        auto h = s.group_create();
        check(s.group(h)->matrix == X::GROUP_IDENTITY, "identity matrix");
        s.group_set_matrix(h, {0, 1, 0, -1, 0, 0, 0, 0, 1, 10, 20, 30});
        check(s.group(h)->matrix[9] == 10 && s.group(h)->matrix[10] == 20 && s.group(h)->matrix[11] == 30,
              "matrix translation");
        s.group_set_matrix(h + 0x100, X::Mat34d{});
        check(s.group(h)->matrix[9] == 10, "stale handle ignored");
        check(s.group_slot(0).live && s.group_slot(0).matrix[10] == 20, "group slot view");
    }
    {
        check(R::rz(1.0 / 3.0) == 0.3333333134651184, "rz toward zero");
        check(R::rz(-1.0 / 3.0) == -0.3333333134651184, "rz negative");
        check(R::rz(1e-40) == 0.0, "rz flushes");
        auto d = make(X::KIND_ADD, 3);
        auto p = X::spawn_record(d, {0, 0, 0}, {0, 0, 0}, 100, 17 / 60.0);
        check(p.birth == 100 - 16, "17/60 s back-dates 16 ticks");
    }
    {
        X::ParticleSystem s;
        s.set_clock(0x7ffe);
        auto d = make(X::KIND_ADD, 3);
        s.spawn(d, {0, 0, 0}, {0, 0, 0});
        s.update();
        s.update();
        auto l = s.live();
        check(s.clock() == 0 && l.size() == 1 && s.clock() - l[0].particle.birth == 2, "clock wrap rebases");
    }
    // Flipbook
    {
        auto still = make(X::KIND_ADD, 1, 0x05);
        check(!X::flipbook_step(still, 0), "row 0 still");
        auto row1 = make(X::KIND_ADD, 1, 0x1f);
        check(!X::flipbook_step(row1, 1) && X::flipbook_step(row1, 2), "row 1 even frames");
        check(row1.u0 == 16 && row1.u1 == 31 * 16, "wrap to column 0");
        auto row4 = make(X::KIND_ADD, 1, 0x40);
        check(X::flipbook_step(row4, 1) && row4.u0 == 33 * 16, "row 4 every call");
    }
    // Emitters
    {
        X::EmitterRecord r;
        r.start_rgba = {1, 2, 3, 4};
        r.end_rgba = {5, 6, 7, 8};
        r.size_start = 2; r.size_end = -1; r.gravity = -5; r.life_class = 2; r.kind = 0; r.tile = 0x33;
        r.rate = -4; r.off_x = 10; r.off_y = -20; r.off_z = 30; r.vel = {0, 0, 40}; r.yaw = 0x4000;
        r.pitch = 0x100; r.box = {8, 8, 2}; r.spread = 6; r.cone = 0x800; r.on_time = 120; r.off_time = 60;
        r.flags = 2; r.name = "smoke";
        const auto b = r.pack();
        check(b.size() == 0x48 && b[0x11] == 0xfc, "packed rate");
        check(b[0x2c] == 120 && b[0x2d] == 0 && b[0x2e] == 0 && b[0x2f] == 0, "on time at 0x2c");
        check(std::string(reinterpret_cast<const char *>(b.data() + 0x3d), 5) == "smoke", "name at 0x3d");
        auto back = X::EmitterRecord::parse(b.data(), b.size());
        check(back.tile == 0x33 && back.rate == -4 && back.vel == std::array<int, 3>{0, 0, 40} && back.name == "smoke",
              "round trip");
        auto d = X::ParticleDef::from_emitter_record(back);
        check(d.kind == 0 && d.life_class == 2 && d.tile == 0x33, "emitter definition");
        almost(d.size0, 200.0, 7, "emitter size0");
        auto other = X::EmitterRecord::parse(b.data(), b.size());
        other.rate = 3;
        check(r.same_definition(other), "same definition ignores rate");
    }
    {
        X::CosmeticRng rng(1);
        X::EmitterRecord r;
        r.rate = 3;
        check(X::emitter_run(r, {0, 0, 0}, 10, rng).size() == 3, "positive rate");
        r = X::EmitterRecord{};
        r.rate = -4;
        check(X::emitter_run(r, {0, 0, 0}, 10, rng).empty(), "negative rate off-beat");
        check(X::emitter_run(r, {0, 0, 0}, 12, rng).size() == 1, "negative rate on-beat");
        r = X::EmitterRecord{};
        r.rate = 0;
        check(X::emitter_run(r, {0, 0, 0}, 12, rng).empty(), "zero rate");
    }
    {
        X::CosmeticRng rng(7);
        X::EmitterRecord r;
        r.rate = 1; r.on_time = 100; r.off_time = 50; r.next = 0; r.on = 0;
        auto out = X::emitter_run(r, {0, 0, 0}, 1, rng);
        check(r.on == 1 && out.size() == 1, "switches on");
        check(1 + 50 <= r.next && r.next < 1 + 150, "on period");
        const auto t = r.next + 1;
        check(X::emitter_run(r, {0, 0, 0}, t, rng).empty() && r.on == 0, "switches off");
        check(t + 25 <= r.next && r.next < t + 75, "off period");
    }
    {
        X::CosmeticRng rng(3);
        X::EmitterRecord r;
        r.rate = 1; r.vel = {0, 0, 32}; r.off_z = 5;
        auto out = X::emitter_run(r, {100.0, 50.0, 10.0}, 1, rng);
        const auto want = trig_cases();
        check(out.size() == 1 && want.count("straight_up/count") &&
                  want.at("straight_up/count") == "1" &&
                  rng.seed == std::stoul(want.at("straight_up/seed")),
              "straight up count and seed (generated)");
        // sin16(0) is +2^-22, so a zero tilt still moves the velocity off the axis.
        // Expected bits come from the pinned reference, not from this port.
        if (out.size() == 1) {
            check(same_bits(out[0].pos[0], want, "straight_up/pos0.x") &&
                      same_bits(out[0].pos[1], want, "straight_up/pos0.y") &&
                      same_bits(out[0].pos[2], want, "straight_up/pos0.z"),
                  "straight up position (generated)");
            check(same_bits(out[0].vel[0], want, "straight_up/vel0.x"),
                  "sideways speed from sin16(0) (generated)");
            check(same_bits(out[0].vel[1], want, "straight_up/vel0.y") &&
                      same_bits(out[0].vel[2], want, "straight_up/vel0.z"),
                  "upward speed (generated)");
        }
    }
    {
        X::CosmeticRng rng(3);
        X::EmitterRecord r;
        r.rate = 2; r.vel = {10, 0, 0}; r.flags = 1;
        check(X::emitter_run(r, {0, 0, 0}, 1, rng, 0, false, false, false, 0, false).empty(), "weather off");
        check(X::emitter_run(r, {0, 0, 0}, 2, rng, 0, false, false, false, 0, true).size() == 2, "weather on");
    }
    {
        X::EmitterRecord r;
        r.rate = 1;
        X::CosmeticRng rng(1);
        check(X::emitter_run(r, {0, 0, 0}, 0, rng).empty() && r.on == 0, "tick 0 stays off");
        X::CosmeticRng z(0);
        check(z.step() == 0x3c6ef35fu, "cosmetic generator");
        X::CosmeticRng z2(0);
        almost(z2.r(), 0x3c6e / 65536.0, 7, "r()");
        auto m = X::rotate_axis(X::IDENTITY, 2, 0x4000);
        auto v = X::transform(m, {1.0, 0.0, 0.0});
        // cos16(0x4000) is -2^-22, not 0. sin16(0x4000) is +1.
        almost(v[0], -2.384185791015625e-07, 12, "quarter turn x");
        almost(v[1], 1.0, 12, "quarter turn y");
    }
    // Beyond the reference: page memory, slot records (docs/effects.md 3, 13.1)
    {
        X::ParticleSystem s;
        auto d = make(X::KIND_ADD, 0);
        s.spawn(d, {7, 8, 9}, {0, 0, 0});
        const int page = s.global_set()[static_cast<std::size_t>(d.list_index)].pages.at(0);
        check(s.cell(page, 1)[7] == 4, "cell type = slot x 4");
        const auto r = s.slot_record(1);
        check(r.used && r.definition == &d && r.words == d.record() && r.known == 0xffbf, "slot record");
        const auto z = s.slot_record(0);
        check(z.known == 0xf788 && z.words[8] == 0x4b000000u && z.words[11] == 0, "slot 0 static record");
        for (int i = 0; i < 12; ++i)
            s.update();
        check(s.cell(page, 1)[7] == 0 && s.cell(page, 1)[0] == 7, "retired cell: type 0, record kept");
        check(!s.slot_used(1) && !s.slot_record(1).used, "slot freed");
        auto big = make();
        bool threw = false;
        try {
            s.spawn(big, {std::nan(""), 0, 0}, {0, 0, 0});
        } catch (const std::domain_error &) {
            threw = true;
        }
        check(threw && big.live == 0 && big.slot == -1 && s.free_pages().size() == X::PAGE_COUNT,
              "refused value leaves the pools untouched");
    }
}

void decal_tests() {
    check(!D::allowed(0, 0) && !D::allowed(1, 0) && !D::allowed(2, 0) && D::allowed(3, 0) && D::allowed(9, 0),
          "detail 0 gate");
    check(!D::allowed(0, 1) && D::allowed(1, 1) && D::allowed(2, 1) && D::allowed(3, 1), "detail 1 gate");
    bool all = true;
    for (int k = 0; k < 11; ++k)
        all = all && D::allowed(k, 2);
    check(all, "detail 2 gate");
    check(D::allowed(-1, 0), "unsigned type comparison");
    check(D::cell_of(-3840.0, -3840.0) == std::make_pair(0, 0), "grid origin");
    check(D::cell_of(3839.0, 0.0) == std::make_pair(29, 15), "grid edge");
    check(!D::cell_of(3840.0, 0.0) && !D::cell_of(0.0, -3841.0), "outside the grid");
    {
        D::DecalStore s(2);
        const D::Decal *d = s.add(12.0, {10.25, -3.0, 5.0}, 0, 9, 0, {1, 2, 3});
        check(d && d->rgb == std::array<int, 3>{1, 2, 3} && d->x4 == 41 && d->y4 == -12 && d->z4 == 32 &&
                  d->counted == 0 && d->tex == 0x3D3 + 18,
              "decal record fields");
        check(d->c4 == 48 && d->s4 == 0 && d->record().size() == 12, "corner vector");
        check(s.add(1.0, {0.0, 0.0, 0.0}, 0, 1)->counted == 1, "counted types");
    }
    {
        D::DecalStore s(2);
        int made = 0;
        for (int i = 0; i < 16; ++i)
            made += s.add(4.0, {1.0, 1.0, 0.0}, 0, 9) != nullptr;
        check(made == D::PER_BLOCK, "15 decals a cell");
        for (int i = 0; i < D::BLOCKS; ++i)
            s.add(4.0, {-3800.0 + 256 * (i % 29), -3800.0 + 256 * (i / 29 + 1), 0.0}, 0, 9);
        check(s.free_blocks().empty() && !s.add(4.0, {3800.0, 3800.0, 0.0}, 0, 9), "64 blocks");
    }
    {
        D::DecalStore s(2);
        s.add(4.0, {0.0, 0.0, 40.0}, 0, 9);
        const auto &b = s.block(s.cell_block(15, 15));
        check(b.zmin == 0 && b.zmax == 40, "z range starts at 0");
    }
    {
        D::DecalStore s(2);
        const D::Decal *d = s.add(1.0, {0.0, 0.0, 0.0}, 0, 10, 1);
        check(d->c4 == 4, "full size until the first step");
        int steps = 0;
        while (s.growers()[0]) {
            s.grow_step();
            ++steps;
        }
        check(steps == 6 && d->c4 == 4, "grow 1: 6 steps, overshoot");
        D::DecalStore s2(2);
        s2.add(1.0, {0.0, 0.0, 0.0}, 0, 10, 2);
        int n = 0;
        while (s2.growers()[0]) {
            s2.grow_step();
            ++n;
        }
        check(n == 21, "grow 2: 21 steps");
    }
    {
        D::DecalStore s(2);
        s.add(5.0, {0.0, 0.0, 0.0}, 0x4000, 10, 1);
        s.grow_step(true);
        check(s.growers()[0] && s.growers()[0]->size == 0, "frozen does not grow");
    }
    {   // decal_reset (docs/effects.md 16.2, I): growing slots survive and write into recycled blocks
        D::DecalStore s(2);
        s.add(8.0, {0.0, 0.0, 0.0}, 0, 10, 1);
        s.grow_step();
        s.reset();
        check(s.free_blocks().size() == D::BLOCKS && s.cell_block(15, 15) < 0 && s.decals().empty(),
              "reset clears grid and blocks");
        check(s.growers()[0].has_value() && s.total() == 1, "reset keeps growers and total");
        const D::Decal *fresh = s.add(2.0, {600.0, 600.0, 0.0}, 0, 9);  // takes block 0 again
        check(fresh && fresh->c4 == 8, "fresh decal in the recycled block");
        s.grow_step();
        check(fresh->c4 != 8, "stale grower writes into the recycled block");
    }
}

C::NoiseTable props_table() {
    C::NoiseTable t{};
    for (int i = 0; i < 256; ++i)
        t[static_cast<std::size_t>(i)] = static_cast<std::int16_t>((i * 37) % 2000 - 1000);
    return t;
}
double fv(Raw p) { return f32val(p); }

void props_tests() {
    const auto table = props_table();
    const F::Vec3 zero{0, 0, 0};
    const std::pair<int, int> flames[] = {{0, 1}, {1, 1}, {2, 3}, {3, 1}, {4, 4}, {5, 3}};
    for (const auto &kf : flames) {
        F::FireProp t(F::FireClass::Torch, 0x1000, zero, 77, kf.first);
        const auto out = F::torch_run(t, 1, table);
        int n = 0;
        bool backs = true;
        for (const auto &s : out.spawns)
            if (s.definition == F::DEF_TORCH_FLAME) {
                ++n;
                backs = backs && s.backdate == (kf.first == 3 ? D::f32(0.25) : 0u);
            }
        check(n == kf.second && backs, "torch flames by kind");
    }
    {
        F::FireProp t(F::FireClass::Torch, 0x1000, zero, 5, 0);
        const auto out = F::torch_run(t, 8, table);
        check(out.spawns.size() >= 2 && out.spawns[0].definition == F::DEF_TORCH_FLAME &&
                  out.spawns[1].definition == F::DEF_TORCH_SMOKE,
              "smoke every eighth frame");
        F::FireProp t4(F::FireClass::Torch, 0x1000, zero, 5, 4);
        bool smoke = false;
        for (const auto &s : F::torch_run(t4, 8, table).spawns)
            smoke = smoke || s.definition == F::DEF_TORCH_SMOKE;
        check(!smoke, "kind 4 no smoke");
    }
    {
        F::FireProp t(F::FireClass::Torch, 0x2000, zero, 1);
        bool ok = true;
        for (int frame = 0; frame < 50; ++frame) {
            F::torch_run(t, frame, table);
            ok = ok && 160 - 21 <= fv(t.model_radius) && fv(t.model_radius) <= 160 + 21 &&
                 t.model_radius == t.level_radius;
        }
        check(ok, "torch radius range");
    }
    {
        F::FireProp c(F::FireClass::Candle, 0x3000, {0, 0, D::f32(10.0)}, 9);
        std::vector<double> radii;
        for (int frame = 0; frame < 10; ++frame) {
            F::candle_run(c, frame);
            radii.push_back(fv(c.model_radius));
        }
        check(radii[0] == 240 && radii[3] == 240 && 48 <= radii[4] && radii[4] <= 63, "candle radius every 5");
        F::FireProp big(F::FireClass::Candle, 0x3000, {0, 0, D::f32(10.0)}, 9, 0, true);
        for (int frame = 0; frame < 5; ++frame)
            F::candle_run(big, frame);
        check(84 <= fv(big.model_radius) && fv(big.model_radius) <= 99, "big candle radius");
    }
    {
        F::FireProp c(F::FireClass::Candle3, 0x3000, {D::f32(1.0), D::f32(2.0), D::f32(3.0)}, 9);
        const auto s = F::candle3_run(c).spawns.at(0);
        check(s.velocity[0] == 0 && s.velocity[1] == 0 && 3.2 <= fv(s.velocity[2]) && fv(s.velocity[2]) <= 3.8 &&
                  fv(s.position[2]) >= 8.0,
              "candle3 particle rises");
    }
    {
        F::FireProp a(F::FireClass::Lantern, 0x4000, zero, 1, 0, false, "d_mine2");
        F::lantern_run(a, 3, table);
        check(a.level_radius == 0, "lantern in a mine");
        F::FireProp b(F::FireClass::Lantern, 0x4000, zero, 1, 0, false, "castle");
        F::lantern_run(b, 3, table);
        check(b.level_radius == b.model_radius, "lantern elsewhere");
    }
    {
        F::FireProp lp(F::FireClass::Lamp, 0x5000, zero, 3);
        const auto out = F::lamp_run(lp, 0, table);
        bool ok = out.spawns.size() == 2;
        for (const auto &s : out.spawns) {
            double sum = 0;
            for (Raw v : s.velocity)
                sum += fv(v) * fv(v);
            ok = ok && std::fabs(std::sqrt(sum) - 2.846) < 0.5e-4;
        }
        check(ok, "lamp speed");
    }
    {
        F::FireProp lp(F::FireClass::Lamp, 0x5000, zero, 3);
        int first = -1;
        for (int i = 0; i < 33; ++i)
            if (F::lamp_run(lp, i, table).stopped && first < 0)
                first = i;
        check(first == 31 && !lp.running, "stop after 31 undrawn");
    }
}

void lighting_tests() {
    {
        auto lv = L::Level::from_markers();
        check(lv.ambient[0] == 0x20 && lv.ambient[2] == 0x20 && lv.sun[0] == 0x44, "marker defaults");
        almost(lv.sun_direction.y, -std::sqrt(0.5), 7, "default sun direction");
        const L::Vec3 a{100, 60, 41}, s{300, 200, 100}, d{0, 0, 5};
        lv = L::Level::from_markers(&a, &s, &d);
        check(lv.ambient[0] == 50 && lv.ambient[1] == 30 && lv.ambient[2] == 20, "marker colours");
        check(lv.sun_direction.x == 0 && lv.sun_direction.y == 0 && lv.sun_direction.z == 1, "marker direction");
    }
    check(L::Level::base_level(0x01000000) == 0x80 && L::Level::base_level(0x04000000) == 0x40 &&
              L::Level::base_level(0x20000000) == 0x20,
          "base level");
    {
        auto lv = L::Level::make({0, 0, 1}, {200, 100, 50, 0}, {40, 40, 40, 0}, 0x80, 1.5);
        auto sc = lv.scaled();
        check(sc.first[0] == 255 && sc.first[1] == 150 && sc.first[2] == 75, "scaled sun");
        lv.layer = 1;
        check(lv.scaled().first[0] == 200, "layer disables scale");
    }
    {
        auto lv = L::Level::make({1, 0, 1}, {0, 0, 0, 0}, {100, 50, 10, 0});
        auto f = lv.fill_direction();
        almost(f.x, -1 / std::sqrt(1.25), 7, "fill x");
        almost(f.z, 0.5 / std::sqrt(1.25), 7, "fill z");
        auto obj = L::Object::reset(lv);
        check(obj.fill[0] == 79 && obj.fill[1] == 39 && obj.fill[2] == 7, "fill 0.79999995");
    }
    {
        auto lv = L::Level::make({0, 0, 1}, {100, 100, 100, 0}, {100, 100, 100, 0}, 0x80, 0.5);
        auto obj = L::Object::reset(lv);
        check(obj.ambient[0] == 50 && obj.sun[0] == 50 && obj.fill[0] == 79, "fill ignores scale");
    }
    {
        auto lv = L::Level::make({0, 0, 1}, {100, 100, 100, 0}, {30, 30, 30, 0});
        auto obj = L::Object::reset(lv);
        obj.chill(0.5);
        check(obj.ambient == L::Rgba{0, 0, 80, 25}, "chill");
        obj = L::Object::reset(lv);
        obj.poison(2.0);
        check(obj.ambient == L::Rgba{0, 80, 0, 50}, "poison");
        obj.hit_flash();
        check(obj.ambient == L::Rgba{255, 255, 255, 0}, "hit flash");
        obj = L::Object::reset(lv);
        obj.scale(0.6, 0.6, 0.6);
        check(obj.ambient[0] == 18 && obj.sun[0] == 60 && obj.point_factors[0] == 0.6, "object scale");
    }
    {
        auto c = L::grid_cell({0, 0, 0});
        check(c.x == 45 && c.y == 45, "grid centre");
        c = L::grid_cell({-11520, 11519, 0});
        check(c.x == 0 && c.y == 89, "grid corner");
        check(!L::grid_cell({11520, 0, 0}).valid(), "grid outside");
    }
    {
        std::vector<L::Node> n;
        n.emplace_back(L::Vec3{0, 0, 0}, L::Rgb{255, 0, 0}, L::Rgb{0, 0, 0}, 100);
        n.emplace_back(L::Vec3{150, 0, 0}, L::Rgb{0, 255, 0}, L::Rgb{0, 0, 0}, 100);
        n.emplace_back(L::Vec3{0, 0, 0}, L::Rgb{0, 0, 255}, L::Rgb{0, 0, 0}, 100, -1, 0, 1);
        n.emplace_back(L::Vec3{99, 99, 99}, L::Rgb{1, 1, 1}, L::Rgb{0, 0, 0}, 100);
        auto got = L::collect_near(n, {60, 0, 0});
        auto has = [&](int i) { return std::find(got.begin(), got.end(), &n[static_cast<std::size_t>(i)]) != got.end(); };
        check(has(0) && has(1) && has(3) && !has(2), "collect cube and layer");
        std::vector<L::Node> one{n[0]};
        check(L::collect_near(one, {101, 0, 0}).empty(), "outside the cube");
    }
    {
        auto lv = L::Level::make({0, 0, 1}, {0, 0, 0, 0}, {0, 0, 0, 0});
        auto obj = L::Object::reset(lv);
        std::vector<L::Node> n;
        n.emplace_back(L::Vec3{0, 0, 90}, L::Rgb{0, 0, 200}, L::Rgb{0, 0, 0}, 100);
        n.emplace_back(L::Vec3{0, 50, 0}, L::Rgb{0, 200, 0}, L::Rgb{0, 0, 0}, 100);
        n.emplace_back(L::Vec3{10, 0, 0}, L::Rgb{200, 100, 0}, L::Rgb{0, 0, 0}, 100);
        auto l = L::model_lights({0, 0, 0}, n, obj, lv.layer);
        check(l.lights[0].rgb == L::Rgb{198, 99, 0} && l.lights[1].rgb == L::Rgb{0, 150, 0}, "two best");
        check(l.lights[0].direction.x == 1.0 && l.lights[0].direction.y == 0 && l.lights.size() == 4, "direction");
    }
    {
        auto lv = L::Level::make({0, 0, 1}, {0, 0, 0, 0}, {10, 10, 10, 0});
        auto obj = L::Object::reset(lv);
        std::vector<L::Node> n;
        n.emplace_back(L::Vec3{10, 0, 0}, L::Rgb{200, 0, 0}, L::Rgb{0, 0, 0}, 100);
        auto l = L::model_lights({0, 0, 0}, n, obj, lv.layer);
        check(l.lights[1].rgb == L::Rgb{0, 0, 0} && l.ambient == L::Rgb{10, 10, 10}, "one light");
    }
    {
        auto lv = L::Level::make({0, 0, 1}, {100, 100, 100, 0}, {20, 20, 20, 0});
        auto obj = L::Object::reset(lv);
        auto l = L::model_lights({0, 0, 0}, {}, obj, lv.layer);
        check(L::shade_model({0, 0, 1}, l) == L::Rgb{133, 133, 133}, "model vertex");
        check(L::shade_model({0, 0, -1}, l) == L::Rgb{20, 20, 20}, "model back face");
    }
    {
        auto torch = L::Node::fire({0, 0, 100}, 200);
        auto up = L::shade_world(0x80, {torch}, {0, 0, 0}, {0, 0, 1});
        check(up == L::shade_world(0x80, {torch}, {0, 0, 0}, {0, 0, -1}) && up[0] == 0x80 + int(0.75 * 51),
              "floor 1 is omni");
        L::Node n({0, 0, 100}, {0, 0, 0}, {100, 0, 0}, 200);
        check(L::shade_world(0x80, {n}, {0, 0, 0}, {0, 0, 1})[0] == 0x80 + int(0.75 * 0.99 * 100) &&
                  L::shade_world(0x80, {n}, {0, 0, 0}, {0, 0, -1})[0] == 0x80,
              "directional level light");
    }
    {
        auto fixed = [](double v) { return std::function<double()>([v] { return v; }); };
        L::Chunk m0{100, 200, 0, 10};
        m0.rgba = {255, 200, 100, 0};
        check(m0.step(5, fixed(0))->model_radius == 100, "mode 0");
        L::Chunk m1{100, 200, 1, 10};
        auto n = m1.step(5, fixed(0.5));
        check(n->model_radius == 150 && m1.timer == 15, "mode 1 draw");
        n = m1.step(10, fixed(0.9));
        check(n->model_radius == 150, "mode 1 timer");
        almost(n->model_falloff, 1.0 / (150.0 * 150.0), 7, "falloff");
        L::Chunk m2{100, 200, 2, 10};
        std::vector<double> r;
        for (int f = 1; f < 23; ++f)
            r.push_back(m2.step(f, fixed(0))->model_radius);
        check(r[0] == 190 && r[9] == 100 && r[10] == 90 && r[11] == 110 && r[12] == 120, "mode 2 triangle");
        L::Chunk m3{100, 200, 3, 4};
        check(m3.step(1, fixed(0))->model_radius == 100 && m3.step(3, fixed(0))->model_radius == 100 &&
                  m3.step(6, fixed(0))->model_radius == 200,
              "mode 3 toggle");
        L::Chunk m4{100, 200, 4, 4};
        check(!m4.step(1, fixed(0)), "mode 4 off");
        L::Chunk m5{100, 200, 5, 4};
        m5.radius = 123;
        check(m5.step(1, fixed(0))->model_radius == 123, "mode 5 keeps the radius");
        L::Chunk cl{400, 500, 0, 1};
        check(cl.step(1, fixed(0))->model_radius == 299, "clamp");
        check(L::glow_size(100, 2, 0.5) == 100 + (11 << 2), "glow size");
    }
    {
        check(L::fire_radius("torch", 0.5) == 171 && L::fire_radius("lamp", -1.0) == 194, "fire radius");
        check(L::candle_radius(3, false) == 59 && L::candle_radius(10, true) == 86, "candle radius");
        check(L::lantern_world_radius(170, "goblinmine2") == 0, "lantern mine");
        auto n = L::Node::fire({0, 0, 0}, 171);
        check(n.model_rgb == L::Rgb{200, 150, 50} && n.world_rgb == L::Rgb{51, 39, 18} && n.floor == 1.0 &&
                  n.tint_decals,
              "fire node");
    }
    {
        L::LightEffect fx(L::Rgba{200, 100, 0, 0}, L::Rgba{0, 0, 0, 0}, 100, 50, 0.5, 1000);
        check(fx.life == 30 && fx.end == 1030, "light effect life");
        L::Node node({}, {}, {}, 0);
        check(fx.run(1001, node) && node.model_rgb == L::Rgb{193, 96, 0}, "light effect first frame");
        bool alive = true;
        for (int f = 1002; f < 1030; ++f)
            alive = fx.run(f, node);
        check(alive, "alive before the end");
        check(!fx.run(1030, node) && node.model_rgb[0] == 0, "end frame");
        almost(node.model_radius, 50.0, 3, "end radius");
    }
    {
        L::LightEffect fx(L::Rgba{0, 0, 0, 0}, L::Rgba{-300, 0, 0, 0}, 100, 100, 1 / 60.0, 0, true);
        L::Node node({}, {}, {}, 0);
        const L::Vec3 owner{1, 2, 3};
        fx.run(0, node, &owner);
        check(node.model_rgb[0] == -255 && node.pos.x == 1 && node.pos.y == 2 && node.pos.z == 39.0,
              "signed colour, owner position");
    }
    {
        auto lv = L::Level::make({0, 0, 1}, {10, 10, 10, 0}, {5, 5, 5, 0});
        auto above = L::Node::fire({0, 0, 50}, 100);
        auto below = L::Node::fire({0, 0, -50}, 100);
        L::Node effect({0, 0, 50}, {255, 255, 255}, {0, 0, 0}, 100);
        check(L::decal_tint({0, 0, 0}, {below, effect}, lv) == L::Rgb{15, 15, 15}, "decal tint ignores");
        check(L::decal_tint({0, 0, 0}, {above}, lv)[0] == 15 + int(200 * 0.75), "decal tint from above");
    }
    {
        L::LightEffect le(L::Rgba{0, 0, 0, 0}, L::Rgba{10, 0, 0, 0}, 50.0, 50.0, 0.5, 0);
        L::Node node({}, {}, {}, 0);
        std::vector<int> seen;
        for (int f = 1; f < 31; ++f) {
            le.run(f, node);
            seen.push_back(node.model_rgb[0]);
        }
        check(seen.front() == 0 && seen.back() >= 9 && std::is_sorted(seen.begin(), seen.end()),
              "light effect keeps fractions");
    }
    {
        L::LightSlots slots;
        std::vector<L::Chunk> chunks(16);
        bool order = true;
        for (int i = 0; i < 16; ++i) {
            chunks[static_cast<std::size_t>(i)] = L::Chunk{100 + i, 200, 0, 0};
            chunks[static_cast<std::size_t>(i)].rgba = {255, 255, 255, 0};
        }
        for (int i = 0; i < 16; ++i)
            order = order && slots.run(chunks[static_cast<std::size_t>(i)], 1, 10 + i).first == i;
        check(order, "slots in order");
        L::Chunk late{5, 9, 3, 100};
        late.rgba = {1, 2, 3, 0};
        late.timer = 1000;
        const auto r = slots.run(late, 2, 100);
        check(r.first == 0 && r.second && r.second->model_radius == 100.0, "takeover keeps the slot radius");
        check(slots.slot_of.count(&chunks[0]) == 0, "previous owner loses the slot");
    }
}

void trig_tests() {
    namespace T = champions::rta::fx::trig;
    namespace R = champions::simulation::rta;
    check(T::cos16(0) == 0x3f800000u && T::sin16(0x4000) == 0x3f800000u, "quarter cosine is +1");
    check(T::cos16(0x8000) == 0xbf800000u, "half turn is -1");
    check(T::sin16(0) == 0x34800000u && T::cos16(0xc000) == 0x34800000u, "sin16(0) is +2^-22");
    check(T::cos16(0x4000) == 0xb4800000u, "cos16 of a quarter turn is -2^-22");
    check(T::sin16(0) == T::cos16(0u - 0x4000u), "sine is the shifted cosine");
    const auto zero = T::haze_circle(0);
    check(zero.sine == 0 && zero.cosine == 5, "haze index 0 truncates the sine");
    const auto quarter = T::haze_circle(16);
    check(quarter.sine == 5 && quarter.cosine == 0, "haze index 16 truncates the cosine");
    check(T::haze_circle(0).sine == T::haze_circle(64).sine &&
              T::haze_circle(0).cosine == T::haze_circle(64).cosine,
          "haze index keeps six bits");
    std::array<std::pair<std::int32_t, std::int32_t>, 64> pairs{};
    for (std::uint32_t i = 0; i < 64; ++i)
        pairs[i] = {T::haze_circle(i).sine, T::haze_circle(i).cosine};
    int kinds = 0;
    for (std::uint32_t i = 0; i < 64; ++i) {
        bool seen = false;
        for (std::uint32_t j = 0; j < i; ++j)
            seen = seen || (pairs[j] == pairs[i]);
        if (!seen)
            ++kinds;
    }
    check(kinds == 28, "haze truncation collapses to 28 pairs");
    check(T::haze_index(0, 0, 0x41200000u, 0) == 10u, "haze index truncates the start alpha");
    R::set_fpu();
    bool same = true;
    for (std::uint32_t angle = 0; angle < 65536u && same; ++angle)
        same = T::cos16(angle) == R::cos16(angle) && T::sin16(angle) == R::sin16(angle);
    check(same, "fx trig matches core cos16 on every angle");
    auto throws = [](auto fn) {
        try {
            fn();
        } catch (const std::domain_error &) {
            return true;
        }
        return false;
    };
    check(T::process_damage_hit_angle(0) == 0x7fffu, "hit 0 adds 0x7fff");
    check(T::process_damage_hit_angle(1) == 0x8000u, "hit 1 wraps to 0x8000");
    check(T::process_damage_hit_angle(0x8000) == 0xffffffffu, "hit 0x8000 is all bits");
    check(T::process_damage_facing_angle(0x1234) == 0x1234u, "facing 0x1234 is itself");
    check(T::process_damage_facing_angle(0xc000) == 0xffffc000u, "facing 0xc000 sign-extends");
    check(T::process_damage_facing_angle(0x8000) == 0xffff8000u, "facing 0x8000 sign-extends");
    check(T::cos16(T::process_damage_facing_angle(0x1234)) == T::cos16(0x1234),
          "block-spark cosine consumes the low 16 bits");
    check(T::process_damage_gib_angle(0, 0, 16000, 0xe0c0) == 0xffffe0c0u,
          "two-chunk gib bias sign-extends");
    check(throws([] { T::refuse_haze_kind3(); }), "haze kind-3 question refused");
    check(throws([] { T::refuse_haze_screen_axis(); }), "haze screen axis refused");
}

void unit_tests() {
    particle_tests();
    trig_tests();
    decal_tests();
    props_tests();
    lighting_tests();
}
}  // namespace
