#pragma once

#include "champions/fx_rz.hpp"
#include "champions/fx_trig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Exact port of the Return to Arms particle reference tools/ref_particles.py (research
// commit 19f80a6, round 2): definitions, spawn records, ageing, the page / slot / group
// pools, the flipbook, the 0x48-byte emitter record, the cosmetic generator and
// emitter_run. Every binary64 operation of the reference is done here in the same order
// and cut with the same rz()/f32(); integers follow Python's semantics where the two
// could differ (floor modulo, unbounded group handles held in 64 bits).
//
// Header-only on purpose: the round-1 library champions-native-fx (fx.cpp) delegates to
// these classes without linking champions-fx-exact, and champions-fx-sim links both
// libraries, so every function here is inline (ODR-safe). No particle-definition values
// are compiled in: definitions are built from the caller's runtime data.
namespace champions::rta::fx::sim {
struct WorldCodec;
}
namespace champions::rta::fx::exact {

using rounding::f32;
using rounding::rz;
using Vec3d = std::array<double, 3>;
using Mat3d = std::array<std::array<double, 3>, 3>;
using Mat34d = std::array<double, 12>;  // four rows of (x, y, z): a mat3 then the translation

// --- constants ---------------------------------------------------------------------------
inline constexpr std::array<int, 4> LIFE_TICKS{11, 30, 60, 90};  // 0x44fa88
inline constexpr int KIND_BLEND = 0, KIND_ADD = 1, KIND_LINE = 2, KIND_HIDDEN = 3;
inline constexpr std::array<int, 3> ALPHA_REGISTER{0x44, 0x48, 0x48};  // kinds 0..2
inline constexpr std::uint32_t TEST_REGISTER = 0x71001;
inline constexpr double COLOUR_BIAS = 8388608.0;
inline constexpr int SLOT_COUNT = 32;
inline constexpr std::uint32_t SLOT0_DEFINITION = 0x7e1d00;  // holds slot 0 for good
inline constexpr int PAGE_COUNT = 64;
inline constexpr int CELLS_PER_PAGE = 240;
inline constexpr int CURSOR_FULL = 0x101;
inline constexpr int GROUP_COUNT = 16;
inline constexpr Mat34d GROUP_IDENTITY{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
inline constexpr int GROUP_IDLE_FREE = 31;
inline constexpr int CLOCK_WRAP = -0x8000;

// Python int(x) of a float: toward zero. A non-finite value is an error, as in Python.
inline std::int64_t trunc(double x) {
    if (!std::isfinite(x) || std::fabs(x) >= 9.2233720368547758e18)
        throw std::domain_error("trunc of a non-finite or out-of-range value");
    return static_cast<std::int64_t>(x);
}
inline std::int32_t s16(std::int64_t v) noexcept {
    const std::int64_t m = v & 0xffff;
    return static_cast<std::int32_t>((m & 0x8000) ? m - 0x10000 : m);
}
template <class T> constexpr T clamp(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }
inline std::int64_t py_mod(std::int64_t a, std::int64_t b) {  // Python a % b (b != 0)
    std::int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        r += b;
    return r;
}

struct EmitterRecord;

// --- the definition (0x50 bytes in the original) ------------------------------------------
struct ParticleDef {
    int list_index = 0, slot = -1, live = 0, callback = 0;
    // +0x10 u0 (absent for kind 2, whose +0x10 holds line_gravity), +0x14 v0, +0x20 u1, +0x24 v1
    std::int32_t u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    double gravity = 0.0, size_rate = 0.0, size0 = 0.0;  // single-precision values
    std::array<double, 4> c0{}, crate{};
    int kind = KIND_ADD, life_class = 3, tile = 1, life = 90;
    double line_gravity = 0.0, line_lag = 0.0;  // kind 2 only

    bool has_u0() const noexcept { return kind != KIND_LINE; }

    // particleDef_init 0x18ee70. `previous` stands for the bytes already in the object:
    // kind 2 keeps its +0x14, +0x20 and +0x24 (0 for a fresh one).
    static ParticleDef build(double size_start, double size_end, double gravity,
                             const std::array<int, 4> &start_rgba,
                             const std::array<int, 4> &end_rgba, int life_class = 3,
                             int kind = KIND_ADD, int tile = 1, int callback = 0,
                             const ParticleDef *previous = nullptr) {
        if (callback)
            throw std::invalid_argument("particle.cc line 0x1f8: terminate callback must be 0");
        ParticleDef d;
        life_class = clamp(life_class, 0, 3);
        kind = clamp(kind, 0, 3);
        d.kind = kind;
        d.life_class = life_class;
        d.tile = tile;
        d.life = LIFE_TICKS[static_cast<std::size_t>(life_class)];
        const double inv = rz(1.0 / static_cast<double>(d.life));
        d.list_index = kind * 4 + life_class;
        d.slot = -1;
        d.live = 0;
        d.callback = 0;
        const double g = rz(f32(gravity) * 0.00390625);
        d.gravity = g;
        const double a = rz(f32(size_start) * 100.0);
        if (kind == KIND_LINE) {
            d.line_gravity = g;
            d.line_lag = rz(a * f32(0.01));
            d.u0 = 0;
            d.v0 = previous ? previous->v0 : 0;
            d.u1 = previous ? previous->u1 : 0;
            d.v1 = previous ? previous->v1 : 0;
            d.size_rate = d.line_lag;
        } else {
            d.line_gravity = 0.0;
            d.line_lag = 0.0;
            const int col = (d.tile & 0xf) * 32, row = (d.tile >> 4) * 32;
            d.u0 = (col + 1) * 16;
            d.v0 = (row + 1) * 16;
            d.u1 = (col + 31) * 16;
            d.v1 = (row + 31) * 16;
            const double b = rz(f32(size_end) * 100.0);
            d.size_rate = rz(rz(b - a) * inv);
        }
        d.size0 = a;
        const double bias = kind == KIND_HIDDEN ? 0.0 : COLOUR_BIAS;
        for (std::size_t i = 0; i < 4; ++i) {
            d.c0[i] = rz(rz(static_cast<double>(start_rgba[i])) + bias);
            d.crate[i] = rz(rz(static_cast<double>(end_rgba[i] - start_rgba[i])) * inv);
        }
        return d;
    }

    // 0x18f360: size 1 -> 1, gravity 0, colours 0, lifeClass 3, kind 1, tile 1.
    static ParticleDef reset_default() {
        return build(1.0, 1.0, 0.0, {0, 0, 0, 0}, {0, 0, 0, 0}, 3, KIND_ADD, 1);
    }

    // The 17 definition bytes of an emitter record (world_fixup, AddParticle).
    static ParticleDef from_emitter_record(const EmitterRecord &rec);

    // clamp(c0 + crate * age, 0, 255) per channel in the vector unit's rounding.
    std::array<int, 4> colour_at(double age) const {
        const double bias = kind == KIND_HIDDEN ? 0.0 : COLOUR_BIAS;
        std::array<int, 4> out{};
        for (std::size_t i = 0; i < 4; ++i)
            out[i] = static_cast<int>(trunc(clamp(rz(c0[i] + rz(crate[i] * age)) - bias, 0.0, 255.0)));
        return out;
    }
    double size_at(double age) const { return rz(size0 + rz(size_rate * age)); }

    // The 0x40-byte record the vertex program reads (+0x10..+0x4f) as 16 little-endian
    // words. Word 6 (+0x28, the definition's own address) is written as 0, as the
    // reference does.
    std::array<std::uint32_t, 16> record() const {
        using rounding::pattern;
        std::array<std::uint32_t, 16> w{};
        if (kind == KIND_LINE) {
            w[0] = pattern(line_gravity);
            w[1] = static_cast<std::uint32_t>(v0);
            w[2] = pattern(gravity);
            w[3] = pattern(line_lag);
        } else {
            w[0] = static_cast<std::uint32_t>(u0);
            w[1] = static_cast<std::uint32_t>(v0);
            w[2] = pattern(gravity);
            w[3] = pattern(size_rate);
        }
        w[4] = static_cast<std::uint32_t>(u1);
        w[5] = static_cast<std::uint32_t>(v1);
        w[6] = 0;
        w[7] = pattern(size0);
        for (std::size_t i = 0; i < 4; ++i) {
            w[8 + i] = pattern(c0[i]);
            w[12 + i] = pattern(crate[i]);
        }
        return w;
    }
};

// --- one particle --------------------------------------------------------------------------
using CellRecord = std::array<std::int16_t, 8>;  // the 16-byte particle record, s16 fields

struct Particle {
    std::int32_t x = 0, y = 0, z = 0, birth = 0, vx = 0, vy = 0, vz = 0, type4 = 0;
    CellRecord cell() const {
        return {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                static_cast<std::int16_t>(z), static_cast<std::int16_t>(birth),
                static_cast<std::int16_t>(vx), static_cast<std::int16_t>(vy),
                static_cast<std::int16_t>(vz), static_cast<std::int16_t>(type4)};
    }
    static Particle from_cell(const CellRecord &c) {
        return {c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]};
    }
    bool operator==(const Particle &o) const {
        return x == o.x && y == o.y && z == o.z && birth == o.birth && vx == o.vx &&
               vy == o.vy && vz == o.vz && type4 == o.type4;
    }
};

struct View {
    Vec3d pos{};
    double size = 0.0;
    std::array<int, 4> rgba{};
    int age = 0;
};

// The record particle_spawn 0x18f3f8 writes. vel is in world units per 16 ticks; a
// positive backdate (seconds) back-dates the birth by trunc(60 * backdate) ticks in
// truncating single precision.
inline Particle spawn_record(const ParticleDef &d, const Vec3d &pos, const Vec3d &vel,
                             int clock, double backdate = 0.0, int slot = 0) {
    const double px = pos[0], py = pos[1], pz = pos[2];
    const double vx = vel[0], vy = vel[1], vz = vel[2];
    if (backdate > 0.0) {
        const std::int64_t n = trunc(rz(f32(backdate) * 60.0));
        const double g = d.gravity;
        const double nf = static_cast<double>(n);
        const double k = rz(nf * 0.0625);
        const double vz2 = rz(f32(vz) - rz(static_cast<double>(n * 32) * g));
        const double x = rz(f32(px) - rz(f32(vx) * k));
        const double y = rz(f32(py) - rz(f32(vy) * k));
        const double z = rz(rz(f32(pz) - rz(vz2 * k)) - rz(rz(g * nf) * nf));
        return {s16(trunc(x)), s16(trunc(y)), s16(trunc(z)), s16(clock - n),
                s16(trunc(rz(f32(vx) * 256.0))), s16(trunc(rz(f32(vy) * 256.0))),
                s16(trunc(rz(vz2 * 256.0))), slot * 4};
    }
    return {s16(trunc(px)), s16(trunc(py)), s16(trunc(pz)), s16(clock),
            s16(trunc(f32(vx * 256.0))), s16(trunc(f32(vy * 256.0))),
            s16(trunc(f32(vz * 256.0))), slot * 4};
}

// pos = p + v * age / 16, z += g * age^2.
inline Vec3d position_at(const Particle &p, const ParticleDef &d, double age,
                         std::optional<double> gravity = std::nullopt) {
    const double g = gravity ? *gravity : d.gravity;
    return {static_cast<double>(p.x) + static_cast<double>(p.vx) / 4096.0 * age,
            static_cast<double>(p.y) + static_cast<double>(p.vy) / 4096.0 * age,
            static_cast<double>(p.z) + static_cast<double>(p.vz) / 4096.0 * age + g * age * age};
}
inline View evaluate(const Particle &p, const ParticleDef &d, int clock) {
    const int age = clock - p.birth;
    return {position_at(p, d, age), d.size_at(age), d.colour_at(age), age};
}
// Kind 2: (tail, head) = (pos(age - lag), pos(age)), gravity from +0x10.
inline std::pair<Vec3d, Vec3d> line_endpoints(const Particle &p, const ParticleDef &d, int clock) {
    const int age = clock - p.birth;
    return {position_at(p, d, static_cast<double>(age) - d.line_lag, d.line_gravity),
            position_at(p, d, age, d.line_gravity)};
}
// Half width and height in pixels of the 1280 x 448 buffer; w = camera-space depth.
inline std::pair<double, double> screen_half_extent(double size, double w) {
    return {2.0 * size / w, size / w};
}
// 0x18f8b4: retired when birth + life < clock (drawn for ages 0..life).
inline bool expired(const Particle &p, const ParticleDef &d, int clock) {
    return p.birth + d.life < clock;
}

// --- texture flipbook ---------------------------------------------------------------------
// particle_draw 0x194930-0x194980: rows 1 and 2 step on even g_simFrame, row 4 on every
// call; 16 cells then wrap. Mutates d.u0 / d.u1 as the reference does. Kind 2 is left out.
inline bool flipbook_step(ParticleDef &d, std::int64_t sim_frame) {
    if (d.kind == KIND_LINE)
        return false;
    const int row = d.v0 >> 9;
    if (!(((row == 1 || row == 2) && (sim_frame & 1) == 0) || row == 4))
        return false;
    std::int32_t u0 = d.u0 + 0x200, u1 = d.u1 + 0x200;
    if (u0 > 0x1fff) {
        u0 -= 0x2000;
        u1 -= 0x2000;
    }
    d.u0 = u0;
    d.u1 = u1;
    return true;
}

// --- the pools ------------------------------------------------------------------------------
struct ListState {
    std::vector<int> pages;   // page ids, first .. last
    int head = 1;             // +0x8: next cell to examine in the first page
    int tail = CURSOR_FULL;   // +0xc: next free cell in the last page
};
using ListSet = std::array<ListState, 16>;

inline int next_cell(int c) {
    c += 1;
    if ((c & 0x1f) == 0x1f)
        c += 2;
    return c;
}

struct GroupEntry {
    std::int64_t handle = 0;
    Mat34d matrix = GROUP_IDENTITY;
    int idle = 0;
};
struct GroupSlotView {
    std::int64_t handle = 0;  // 0 when no live group holds the slot
    bool live = false;
    Mat34d matrix = GROUP_IDENTITY;
    int idle = 0;
    const ListSet *lists = nullptr;  // the slot's lists (kept by a freed slot)
};
struct SlotRecord {
    bool used = false;          // flag word of 0x82c408
    bool held = false;          // slot 0: the definition 0x7e1d00
    const ParticleDef *definition = nullptr;
    std::array<std::uint32_t, 16> words{};
    std::uint16_t known = 0;    // bit i: words[i] is established by the research
};
struct LiveParticle {
    int set = -1;  // -1 global, else the group slot
    int list = 0, page = 0, cell = 0;
    Particle particle;
    const ParticleDef *definition = nullptr;
};

// Global list set plus 16 group slots; 64 shared pages of 240 cells; 32 type slots of
// which slot 0 is held for good. Definitions are owned by the caller and must outlive
// their residency (the system keeps pointers to them in the slots, as the original keeps
// their addresses in the type records).
class ParticleSystem {
    friend struct ::champions::rta::fx::sim::WorldCodec;   // fx_world_save.cpp
public:
    ParticleSystem() : memory_(static_cast<std::size_t>(PAGE_COUNT) * 256) {
        for (int i = 0; i < PAGE_COUNT; ++i)
            free_pages_.push_back(i);
        slots_.fill(nullptr);
    }

    // --- groups: particleGroup_create / setMatrix / begin / end / free
    std::int64_t group_create() {
        for (int i = 0; i < GROUP_COUNT; ++i) {
            bool used = false;
            for (const auto &g : groups_)
                used = used || (g.handle & 0xff) == i;
            if (!used) {
                const std::int64_t h = (group_seq_ << 8) | i;
                ++group_seq_;
                groups_.push_back({h, GROUP_IDENTITY, 0});
                return h;
            }
        }
        return 0;
    }
    void group_set_matrix(std::int64_t h, const Mat34d &matrix) {
        if (GroupEntry *g = find_group(h))
            g->matrix = matrix;
    }
    void group_begin(std::int64_t h) {
        if (find_group(h))
            current_ = static_cast<int>(h & 0xff);
    }
    void group_end(std::int64_t h) {
        if (find_group(h))
            current_ = -1;
    }
    // particleGroup_free 0x18ea90: pages go back (each pushed in list order) but the live
    // counts of the definitions are NOT lowered.
    void group_free(std::int64_t h) {
        for (auto it = groups_.begin(); it != groups_.end(); ++it) {
            if (it->handle != h)
                continue;
            const int i = static_cast<int>(h & 0xff);
            groups_.erase(it);
            for (ListState &lst : group_lists_[static_cast<std::size_t>(i)]) {
                for (int pg : lst.pages)
                    free_pages_.push_front(pg);
                lst = ListState{};
            }
            return;
        }
    }

    // particle_spawn 0x18f3f8. Returns the record, or nothing when dropped.
    std::optional<Particle> spawn(ParticleDef &d, const Vec3d &pos, const Vec3d &vel,
                                  double backdate = 0.0) {
        ListState &lst = current_lists()[static_cast<std::size_t>(d.list_index)];
        if (d.live == 0 && first_free_slot() < 0) {
            ++dropped_;
            return std::nullopt;
        }
        if (lst.tail == CURSOR_FULL && free_pages_.empty()) {
            ++dropped_;
            return std::nullopt;
        }
        // The record is made before any state changes (same values as the reference, which
        // makes it last), so a value trunc() refuses leaves the pools untouched.
        const int slot = d.live == 0 ? first_free_slot() : d.slot;
        const Particle p = spawn_record(d, pos, vel, clock_, backdate, slot);
        if (lst.tail == CURSOR_FULL) {
            lst.pages.push_back(free_pages_.front());
            free_pages_.pop_front();
            lst.tail = 1;
        }
        if (d.live == 0) {
            slots_[static_cast<std::size_t>(slot)] = &d;
            d.slot = slot;
        }
        const int page = lst.pages.back(), cell = lst.tail;
        lst.tail = next_cell(lst.tail);
        d.live += 1;
        cell_ref(page, cell) = p.cell();
        return p;
    }

    // particle_updateTask 0x18f970, one simulated tick.
    void update(bool in_cutscene = false) {
        clock_ += 1;
        if (clock_ == 0x8000) {  // the s16 reached -0x8000
            clock_ = 0;
            rebase(global_);
            for (const auto &g : groups_)
                rebase(group_lists_[static_cast<std::size_t>(g.handle & 0xff)]);
        }
        for (ListState &lst : global_)
            retire(lst);
        const std::vector<GroupEntry> order = groups_;
        for (const GroupEntry &entry : order) {
            const std::int64_t h = entry.handle;
            ListSet &lists = group_lists_[static_cast<std::size_t>(h & 0xff)];
            bool busy = false;
            for (ListState &lst : lists)
                busy = retire(lst) || busy;
            GroupEntry *g = find_group(h);
            if (in_cutscene || busy) {
                g->idle = 0;
            } else {
                const int idle = g->idle;
                g->idle = idle + 1;
                if (idle >= GROUP_IDLE_FREE)
                    group_free(h);
            }
        }
    }

    // Live particles: global set, then each live group in creation order, each list
    // 0..15, oldest first.
    std::vector<LiveParticle> live() const {
        std::vector<LiveParticle> out;
        collect(-1, global_, out);
        for (const auto &g : groups_) {
            const int i = static_cast<int>(g.handle & 0xff);
            collect(i, group_lists_[static_cast<std::size_t>(i)], out);
        }
        return out;
    }
    // Drawn particles (kinds 0-2) with their state.
    std::vector<std::pair<LiveParticle, View>> visible() const {
        std::vector<std::pair<LiveParticle, View>> out;
        for (const LiveParticle &lp : live())
            if (lp.definition->kind != KIND_HIDDEN)
                out.emplace_back(lp, evaluate(lp.particle, *lp.definition, clock_));
        return out;
    }

    // --- state and renderer read access
    int clock() const noexcept { return clock_; }
    void set_clock(int clock) noexcept { clock_ = clock; }
    int dropped() const noexcept { return dropped_; }
    std::int64_t group_seq() const noexcept { return group_seq_; }
    int current_set() const noexcept { return current_; }  // -1 global, else group slot
    const std::deque<int> &free_pages() const noexcept { return free_pages_; }
    bool slot_used(int i) const { return i == 0 || slots_.at(static_cast<std::size_t>(i)) != nullptr; }
    const ParticleDef *slot_definition(int i) const { return slots_.at(static_cast<std::size_t>(i)); }
    const ListSet &global_set() const noexcept { return global_; }
    const ListSet &group_lists(int slot) const { return group_lists_.at(static_cast<std::size_t>(slot)); }
    const std::vector<GroupEntry> &groups() const noexcept { return groups_; }  // creation order
    const GroupEntry *group(std::int64_t h) const {
        for (const auto &g : groups_)
            if (g.handle == h)
                return &g;
        return nullptr;
    }
    GroupSlotView group_slot(int slot) const {
        GroupSlotView v;
        v.lists = &group_lists_.at(static_cast<std::size_t>(slot));
        for (const auto &g : groups_)
            if ((g.handle & 0xff) == slot) {
                v.handle = g.handle;
                v.live = true;
                v.matrix = g.matrix;
                v.idle = g.idle;
            }
        return v;
    }
    // The 16-byte record in cell `cell` (0..255) of page `page`. A retired cell keeps its
    // record with +0xe set to 0 (O 0x18f82c); cells never written read as 0.
    const CellRecord &cell(int page, int cell) const {
        if (page < 0 || page >= PAGE_COUNT || cell < 0 || cell > 255)
            throw std::out_of_range("particle cell");
        return memory_[static_cast<std::size_t>(page) * 256 + static_cast<std::size_t>(cell)];
    }
    // The live cells of one list, oldest first, as (page, cell) pairs.
    static std::vector<std::pair<int, int>> list_cells(const ListState &lst) {
        std::vector<std::pair<int, int>> out;
        if (lst.pages.empty())
            return out;
        const std::size_t last = lst.pages.size() - 1;
        std::size_t pi = 0;
        int c = lst.head;
        while (!(pi == last && c == lst.tail)) {
            out.emplace_back(lst.pages[pi], c);
            c += 1;
            if ((c & 0x1f) == 0x1f) {
                if (c == 0xff && pi != last) {
                    pi += 1;
                    c = 1;
                } else {
                    c += 2;
                }
            }
            if (c > 0x101 || pi > last)
                throw std::logic_error("particle list cursor out of range");
        }
        return out;
    }
    // The type record a slot uploads. Slot 0 is the static initialiser's record of the
    // definition 0x7e1d00: colour lanes r, g, b = 2^23, colour rates 0, size rate 0 and
    // size 0 are established (docs/effects.md 13.1); its u/v, gravity, address and the
    // alpha lane's float are not (the alpha byte it yields is 0 either way), so those
    // words are 0 with their `known` bits clear. A free slot reports nothing.
    SlotRecord slot_record(int i) const {
        SlotRecord r;
        if (i == 0) {
            r.used = r.held = true;
            r.words[3] = 0;
            r.words[7] = 0;
            r.words[8] = r.words[9] = r.words[10] = 0x4b000000u;
            r.known = static_cast<std::uint16_t>((1u << 3) | (1u << 7) | (1u << 8) | (1u << 9) |
                                                 (1u << 10) | (0xfu << 12));
            return r;
        }
        const ParticleDef *d = slots_.at(static_cast<std::size_t>(i));
        if (!d)
            return r;
        r.used = true;
        r.definition = d;
        r.words = d->record();
        r.known = static_cast<std::uint16_t>(0xffffu & ~(1u << 6));
        return r;
    }

private:
    int clock_ = 0;
    int dropped_ = 0;
    std::int64_t group_seq_ = 1;  // 0x44fc9c
    int current_ = -1;
    std::deque<int> free_pages_;  // front = the head of 0x82c400
    std::array<ParticleDef *, SLOT_COUNT> slots_{};  // slot 0 is held (not a pointer)
    ListSet global_{};
    std::array<ListSet, GROUP_COUNT> group_lists_{};
    std::vector<GroupEntry> groups_;  // live groups in creation order
    std::vector<CellRecord> memory_;  // 64 pages x 256 cells

    ListSet &current_lists() {
        return current_ < 0 ? global_ : group_lists_[static_cast<std::size_t>(current_)];
    }
    int first_free_slot() const {
        for (int i = 1; i < SLOT_COUNT; ++i)
            if (!slots_[static_cast<std::size_t>(i)])
                return i;
        return -1;
    }
    GroupEntry *find_group(std::int64_t h) {
        for (auto &g : groups_)
            if (g.handle == h)
                return &g;
        return nullptr;
    }
    CellRecord &cell_ref(int page, int cell) {
        return memory_[static_cast<std::size_t>(page) * 256 + static_cast<std::size_t>(cell)];
    }
    void rebase(ListSet &lists) {
        for (const ListState &lst : lists)
            for (const auto &pc : list_cells(lst)) {
                CellRecord &c = cell_ref(pc.first, pc.second);
                c[3] = static_cast<std::int16_t>(s16(static_cast<std::int64_t>(c[3]) - 0x8000));
            }
    }
    void dead(ParticleDef &d) {
        d.live -= 1;
        if (d.live == 0) {
            slots_[static_cast<std::size_t>(d.slot)] = nullptr;
            d.slot = -1;
        }
    }
    ParticleDef &definition_of(const CellRecord &c) {
        // Retirement reads +0xe: slot = +0xe >> 2 (O 0x18f744).
        ParticleDef *d = slots_[static_cast<std::size_t>((c[7] >> 2) & 31)];
        if (!d)
            throw std::logic_error("live particle without a resident definition");
        return *d;
    }
    // 0x18f6e0 for one list; true when the list was not empty.
    bool retire(ListState &lst) {
        if (lst.pages.empty())
            return false;
        bool nonempty = false;
        while (!lst.pages.empty()) {
            const int page = lst.pages.front(), last = lst.pages.back();
            if (page == last && lst.head == lst.tail)
                break;
            nonempty = true;
            CellRecord &c = cell_ref(page, lst.head);
            ParticleDef &d = definition_of(c);
            if (!expired(Particle::from_cell(c), d, clock_))
                break;
            c[7] = 0;
            dead(d);
            int n = lst.head + 1;
            if ((n & 0x1f) == 0x1f) {
                if (n != 0xff || page == last) {
                    n += 2;
                } else {
                    free_pages_.push_front(lst.pages.front());
                    lst.pages.erase(lst.pages.begin());
                    n = 1;
                }
            }
            lst.head = n;
        }
        if (lst.pages.size() == 1 && lst.head == lst.tail) {
            free_pages_.push_front(lst.pages.back());
            lst.pages.pop_back();
            lst.head = 1;
            lst.tail = CURSOR_FULL;
        }
        return nonempty;
    }
    void collect(int set, const ListSet &lists, std::vector<LiveParticle> &out) const {
        for (int li = 0; li < 16; ++li)
            for (const auto &pc : list_cells(lists[static_cast<std::size_t>(li)])) {
                const CellRecord &c = cell(pc.first, pc.second);
                LiveParticle lp;
                lp.set = set;
                lp.list = li;
                lp.page = pc.first;
                lp.cell = pc.second;
                lp.particle = Particle::from_cell(c);
                lp.definition = slots_[static_cast<std::size_t>((c[7] >> 2) & 31)];
                out.push_back(lp);
            }
    }
};

// --- emitters --------------------------------------------------------------------------------
// The 0x48-byte emitter record: level chunk data (flag 0x4000) and AddParticle +0x64.
// Default values are those of the reference's EmitterRecord.new().
struct EmitterRecord {
    std::array<int, 4> start_rgba{0x80, 0x80, 0x80, 0x80}, end_rgba{0, 0, 0, 0};
    int size_start = 1, size_end = 1, gravity = 0, life_class = 1, kind = KIND_ADD, tile = 0;
    int rate = 1, off_x = 0, off_y = 0, off_z = 0;
    std::array<int, 3> vel{0, 0, 0};
    int yaw = 0, pitch = 0;
    std::array<int, 3> box{0, 0, 0};
    int spread = 0, cone = 0;
    std::int64_t on_time = 0, off_time = 0;
    int flags = 0;
    std::int64_t next = 0;
    int on = 0;
    std::string name;

    static EmitterRecord parse(const std::uint8_t *data, std::size_t size) {
        if (!data || size < 0x48)
            throw std::invalid_argument("emitter record needs 0x48 bytes");
        auto u16 = [&](std::size_t o) { return static_cast<int>(data[o] | (data[o + 1] << 8)); };
        auto i16 = [&](std::size_t o) { return s16(u16(o)); };
        auto i32 = [&](std::size_t o) {
            const std::uint32_t v = static_cast<std::uint32_t>(data[o]) |
                                    (static_cast<std::uint32_t>(data[o + 1]) << 8) |
                                    (static_cast<std::uint32_t>(data[o + 2]) << 16) |
                                    (static_cast<std::uint32_t>(data[o + 3]) << 24);
            return static_cast<std::int64_t>(static_cast<std::int32_t>(v));
        };
        EmitterRecord r;
        for (std::size_t i = 0; i < 4; ++i) {
            r.start_rgba[i] = data[i];
            r.end_rgba[i] = data[4 + i];
        }
        r.size_start = i16(8);
        r.size_end = i16(10);
        r.gravity = i16(12);
        r.life_class = data[14];
        r.kind = data[15];
        r.tile = data[16];
        r.rate = static_cast<std::int8_t>(data[17]);
        r.off_x = i16(18);
        r.off_y = i16(20);
        r.off_z = i16(22);
        r.vel = {i16(24), i16(26), i16(28)};
        r.yaw = u16(30);
        r.pitch = u16(32);
        r.box = {u16(34), u16(36), u16(38)};
        r.spread = i16(40);
        r.cone = i16(42);
        r.on_time = i32(44);
        r.off_time = i32(48);
        r.flags = data[52];
        r.next = i32(56);
        r.on = data[60];
        std::size_t n = 0;
        while (n < 11 && data[0x3d + n] != 0)
            ++n;
        r.name.assign(reinterpret_cast<const char *>(data + 0x3d), n);
        return r;
    }

    // struct.pack of the record; a field out of its range throws, as struct.error does.
    std::array<std::uint8_t, 0x48> pack() const {
        std::array<std::uint8_t, 0x48> b{};
        auto check = [](std::int64_t v, std::int64_t lo, std::int64_t hi) {
            if (v < lo || v > hi)
                throw std::out_of_range("emitter record field out of range");
            return v;
        };
        auto put8 = [&](std::size_t o, std::int64_t v) { b[o] = static_cast<std::uint8_t>(v & 0xff); };
        auto put16 = [&](std::size_t o, std::int64_t v) {
            b[o] = static_cast<std::uint8_t>(v & 0xff);
            b[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
        };
        auto put32 = [&](std::size_t o, std::int64_t v) {
            for (std::size_t k = 0; k < 4; ++k)
                b[o + k] = static_cast<std::uint8_t>((v >> (8 * k)) & 0xff);
        };
        for (std::size_t i = 0; i < 4; ++i) {
            put8(i, check(start_rgba[i], 0, 255));
            put8(4 + i, check(end_rgba[i], 0, 255));
        }
        put16(8, check(size_start, -32768, 32767));
        put16(10, check(size_end, -32768, 32767));
        put16(12, check(gravity, -32768, 32767));
        put8(14, check(life_class, 0, 255));
        put8(15, check(kind, 0, 255));
        put8(16, check(tile, 0, 255));
        put8(17, check(rate, -128, 127));
        put16(18, check(off_x, -32768, 32767));
        put16(20, check(off_y, -32768, 32767));
        put16(22, check(off_z, -32768, 32767));
        for (std::size_t i = 0; i < 3; ++i)
            put16(24 + 2 * i, check(vel[i], -32768, 32767));
        put16(30, yaw & 0xffff);
        put16(32, pitch & 0xffff);
        for (std::size_t i = 0; i < 3; ++i)
            put16(34 + 2 * i, check(box[i], 0, 65535));
        put16(40, check(spread, -32768, 32767));
        put16(42, check(cone, -32768, 32767));
        put32(44, check(on_time, INT32_MIN, INT32_MAX));
        put32(48, check(off_time, INT32_MIN, INT32_MAX));
        put8(52, check(flags, 0, 255));
        put32(56, check(next, INT32_MIN, INT32_MAX));
        put8(60, check(on, 0, 255));
        for (std::size_t i = 0; i < name.size() && i < 10; ++i)
            b[0x3d + i] = static_cast<std::uint8_t>(name[i]);
        return b;
    }

    // 0x195318: two chunks share an emitter type when bytes 0..0x10 are equal.
    bool same_definition(const EmitterRecord &other) const {
        const auto a = pack(), b = other.pack();
        return std::equal(a.begin(), a.begin() + 0x11, b.begin());
    }
};

inline ParticleDef ParticleDef::from_emitter_record(const EmitterRecord &rec) {
    return build(static_cast<double>(rec.size_start), static_cast<double>(rec.size_end),
                 static_cast<double>(rec.gravity), rec.start_rgba, rec.end_rgba, rec.life_class,
                 rec.kind, rec.tile);
}

// g_cosmeticSeed 0x404460 as 0x195400 draws it.
struct CosmeticRng {
    std::uint32_t seed = 0;
    CosmeticRng() = default;
    explicit CosmeticRng(std::uint32_t s) : seed(s) {}
    std::uint32_t step() noexcept {
        seed = seed * 0x19660du + 0x3c6ef35fu;
        return seed;
    }
    double r() noexcept { return static_cast<double>(step() >> 16) * (1.0 / 65536); }
};

// math_cos16 / math_sin16 (fx_trig.hpp), each result promoted to binary64. The
// multiplies after the cosine stay binary64, as ref_particles does once _cs is
// the polynomial. sin16(0) is +2^-22, so a yaw of 0 is not a pure x offset.
inline std::pair<double, double> angle_cos_sin(std::int64_t angle) {
    return trig::cos_sin(static_cast<std::uint32_t>(angle));
}

// mat34_rotateAxis 0x163000 on a 3 x 3 list of rows (the reference's float form).
inline Mat3d rotate_axis(const Mat3d &m, int axis, std::int64_t angle) {
    const auto cs = angle_cos_sin(angle);
    const double c = cs.first, s = cs.second;
    const auto &r0 = m[0], &r1 = m[1], &r2 = m[2];
    Mat3d out = m;
    for (std::size_t i = 0; i < 3; ++i) {
        if (axis == 0) {
            out[1][i] = c * r1[i] + s * r2[i];
            out[2][i] = -s * r1[i] + c * r2[i];
        } else if (axis == 1) {
            out[0][i] = c * r0[i] - s * r2[i];
            out[2][i] = s * r0[i] + c * r2[i];
        } else if (axis == 2) {
            out[0][i] = c * r0[i] + s * r1[i];
            out[1][i] = -s * r0[i] + c * r1[i];
        }
    }
    return out;
}
// mat34_transformPoint without translation: v.x*row0 + v.y*row1 + v.z*row2.
inline Vec3d transform(const Mat3d &m, const Vec3d &v) {
    Vec3d out{};
    for (std::size_t i = 0; i < 3; ++i)
        out[i] = v[0] * m[0][i] + v[1] * m[1][i] + v[2] * m[2][i];
    return out;
}
inline constexpr Mat3d IDENTITY{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};

inline Vec3d flip(Vec3d v, bool neg_x, bool neg_y, bool swap_xy) {
    if (neg_x)
        v[0] = -v[0];
    if (neg_y)
        v[1] = -v[1];
    if (swap_xy)
        std::swap(v[0], v[1]);
    return v;
}

struct Emission {
    Vec3d pos{};
    Vec3d vel{};
};

// emitter_run 0x195400: updates rec.next / rec.on and returns the (position, velocity)
// pairs it hands to particle_spawn (backdate 0). Angles are math_cos16 / math_sin16.
inline std::vector<Emission> emitter_run(EmitterRecord &rec, const Vec3d &pos, std::int64_t now,
                                         CosmeticRng &rng, std::int64_t yaw = 0,
                                         bool neg_x = false, bool neg_y = false,
                                         bool swap_xy = false, std::int64_t weather_angle = 0,
                                         bool weather_on = false) {
    std::vector<Emission> out;
    const int rate = rec.rate;
    int count;
    if (rate > 0) {
        count = rate;
    } else if (rate == 0) {
        count = 0;
    } else {
        if (py_mod(now, -static_cast<std::int64_t>(rate)) != 0)
            return out;
        count = 1;
    }
    if (rec.next < now) {
        if (rec.on_time == 0) {
            rec.on = 1;
        } else {
            const std::int64_t d = rec.on ? rec.off_time : rec.on_time;
            const double jitter = rz((rng.r() - 0.5) * static_cast<double>(d));
            rec.next = trunc(rz(rz(static_cast<double>(now + d)) + jitter));
            rec.on = rec.on ? 0 : 1;
        }
    }
    if (!rec.on || count <= 0)
        return out;
    for (int k = 0; k < count; ++k) {
        const double ox = (rng.r() - 0.5) * rec.box[0];
        const double oy = (rng.r() - 0.5) * rec.box[1];
        const double oz = (rng.r() - 0.5) * rec.box[2];
        const Mat3d m1 = rotate_axis(rotate_axis(IDENTITY, 2, s16(yaw + rec.yaw)), 1, rec.pitch);
        Vec3d o = transform(m1, {ox, oy, oz});
        const auto cs = angle_cos_sin(yaw);
        const double c = cs.first, s = cs.second;
        o = {o[0] + rec.off_x * c + rec.off_y * s, o[1] - rec.off_x * s + rec.off_y * c, o[2]};
        o = flip(o, neg_x, neg_y, swap_xy);
        const Vec3d p{pos[0] + o[0], pos[1] + o[1], pos[2] + o[2] + rec.off_z};
        Vec3d v{static_cast<double>(rec.vel[0]), static_cast<double>(rec.vel[1]),
                static_cast<double>(rec.vel[2])};
        const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        const double j = rng.r() - 0.5;
        if (n > 0.0)
            for (double &ch : v)
                ch = ch + ch / n * j * rec.spread;
        const std::int64_t spin = s16(rng.step() >> 16);
        const std::int64_t tilt = s16(trunc(rng.r() * rec.cone));
        const Mat3d m2 = rotate_axis(rotate_axis(IDENTITY, 2, spin), 1, tilt);
        v = transform(m2, v);
        if (rec.flags & 1)
            v = transform(rotate_axis(rotate_axis(IDENTITY, 2, weather_angle), 1, rec.pitch), v);
        else if (rec.flags & 2)
            v = transform(rotate_axis(rotate_axis(IDENTITY, 2, rec.yaw), 1, rec.pitch), v);
        else
            v = flip(transform(m1, v), neg_x, neg_y, swap_xy);
        if (!(rec.flags & 1) || weather_on)
            out.push_back({p, v});
    }
    return out;
}

}  // namespace champions::rta::fx::exact
