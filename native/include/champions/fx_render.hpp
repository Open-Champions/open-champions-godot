#pragma once

// What the effect drawers of Champions: Return to Arms (SLUS_209.73, US v2.00) send to the
// GS: particle sprites and lines, particle groups, trails, beams and glow sprites.
//
// Handwritten C++17 port of the research reference tools/ref_fx_render.py (research
// commit 19f80a6, docs/effects.md sections 6, 7, 13, 17), operation for operation and in
// the same order, on raw 32-bit float patterns.  No microcode words, no particle
// definition values and no retail bytes are embedded: the constants are the immediates of
// the drawing code and of the packet headers it builds, as the reference documents them.
//
// Numbers
//   The EE side (CPU code: matrices, trail headers, beams, glows, trail builders) uses the
//   thread's core arithmetic (champions::simulation::rta::set_fpu; Ps2 by default, as
//   ref_core's 'ps2').  The VU side uses class Vu: the same integer arithmetic in the
//   vector unit's form, with an explicit Fpu (Chop by default), exactly as the reference.
//
// Status markers used in comments: E = executed against the original by
// tools/difftest_fx_render.py; O = read in the code only; I = inferred; NATIVE = not
// documented for the original, a choice of this client (listed in the part's report).

#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
#include "champions/core_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace champions::rta::fx::render {

namespace core = champions::simulation::rta;
using Raw = core::Raw;
using Vec3 = core::Vec3;
using Mat34 = core::Mat34;
using Mat4 = core::Mat4;

// Raised for every input the documented contract does not cover (see each function).
struct Refusal : std::logic_error {
    using std::logic_error::logic_error;
};

inline constexpr Raw BIGGEST = 0x7F7FFFFFu;
inline constexpr Raw ONE = 0x3F800000u;
inline constexpr Raw TWO23 = 0x4B000000u;       // 2^23: colours are 2^23 + byte
inline constexpr Raw TWO23_255 = 0x4B0000FFu;

// PRIM values of the packets (docs 13.1, 13.4, 13.7).
inline constexpr std::uint32_t PRIM_SPRITE = 0x156;        // sprite, textured, blended
inline constexpr std::uint32_t PRIM_LINE = 0x049;          // line, Gouraud, blended
inline constexpr std::uint32_t PRIM_TRAIL = 0x154;         // strip, textured (flags without 2)
inline constexpr std::uint32_t PRIM_TRAIL_PAIRED = 0x15C;  // strip, Gouraud, textured (flag 2)
inline constexpr std::uint32_t PRIM_GLOW = 0x15E;          // glow sprite (flags bit 0 clear)
inline constexpr std::uint32_t PRIM_GLOW_QUAD = 0x15C;     // glow quad (flags bit 0 set)
inline constexpr std::uint32_t BLOCK = 30;                 // particles per VU block

// 13.6: how the `MULq | WAITQ` pairs 0x0d2 (line tail) and 0x1ca (trail side) read Q.
// WaitFirst: the pair waits for the divider first (Inferred; ref_fx_render's default,
// waitq_first=True).  Vu1CppReading: the upper half reads the Q still in place, as
// tools/recomp/render/vu1.cpp computes (waitq_first=False).  Not settled on a console.
enum class WaitQ { WaitFirst, Vu1CppReading };

using Lanes = std::array<Raw, 4>;
using Columns = std::array<Lanes, 4>;   // vf1..vf4

// The vector unit's arithmetic on raw patterns: an operand with exponent 255 is read as
// the largest finite number, exponent 0 as zero; results round toward zero (Chop).
class Vu {
public:
    explicit Vu(core::Fpu::Model model = core::Fpu::Model::Chop) : f(model) {}
    core::Fpu f;
    static Raw vin(Raw u) noexcept;
    static Raw vout(Raw u) noexcept;
    Raw add(Raw a, Raw b) const;
    Raw sub(Raw a, Raw b) const;
    Raw mul(Raw a, Raw b) const;
    Raw madd(Raw acc, Raw a, Raw b) const;
    Raw div(Raw a, Raw b) const;
    Raw rsqrt(Raw a, Raw b) const;
    Raw itof(std::uint32_t word, unsigned shift = 0) const;
    std::uint32_t ftoi(Raw a, unsigned shift = 0) const;
    static std::int64_t key(Raw u) noexcept;
    static Raw vmax(Raw a, Raw b) noexcept;
    static Raw vmin(Raw a, Raw b) noexcept;
    // CLIPw.xyz: the six judgement bits (x > |w| 1, x < -|w| 2, y 4/8, z 16/32).
    std::uint32_t clip(const Lanes &v, Raw w) const;
    // w_last: ((m0 x + m1 y) + m2 z) + m3 (line loop, trail programs); otherwise
    // ((m0 x + m1 y) + m3) + m2 z (sprite loop).
    Lanes transform(const Columns &m, Raw x, Raw y, Raw z, bool w_last = true) const;
};

// One vertex as the GS receives it (GIF packed XYZF2): x, y in 1/16 pixel (offset 0x8000),
// z 24 bits, f the fog byte, adc the "do not draw" bit, rgba, uv in 1/16 texel when the
// primitive is textured.  clip_w is NOT a GS field: the raw clip-space w (or, for glows,
// the depth) of the point the vertex was projected from, for a presentation layer that
// depth-tests against 3D geometry.  same_gs() compares the GS fields only.
struct GsVertex {
    std::uint32_t x = 0, y = 0, z = 0, f = 0, adc = 0;
    std::array<std::uint8_t, 4> rgba{};
    bool has_uv = false;
    std::uint32_t u = 0, v = 0;
    Raw clip_w = 0;
    bool same_gs(const GsVertex &o) const noexcept {
        return x == o.x && y == o.y && z == o.z && f == o.f && adc == o.adc &&
               rgba == o.rgba && has_uv == o.has_uv && (!has_uv || (u == o.u && v == o.v));
    }
};
GsVertex gs_xyzf(const Lanes &lanes);                       // x, y, z, f, adc only
std::array<std::uint8_t, 4> gs_rgba(const Lanes &lanes);

// ---- The particle packet: matrices and constants (particle_draw 0x194888) -----------
Mat4 particle_projection(Raw proj_scale);                    // 0x1949b8-0x194a7c
Mat4 particle_matrix(const Mat34 &view, Raw proj_scale);     // mat4_mulMat34(view, P)
Mat4 group_matrix(const Mat34 &group, const Mat34 &view, Raw proj_scale);
Columns columns(const Mat4 &m);
struct ParticleHeader {             // what entry 0x14 loads
    Columns m{};
    Lanes vf5{}, vf6{}, vf7{}, vf9{};
};
ParticleHeader particle_header(const Mat34 &view, Raw proj_scale, std::int32_t clock);

// A 16-byte particle cell: x, y, z, birth, vx, vy, vz, type (slot = type >> 2), int16 each.
using ParticleCell = std::array<std::int16_t, 8>;
// The 16 words of a type slot's record (+0x10..+0x4f; ref_particles ParticleDef.record()).
using TypeRecord = std::array<Raw, 16>;

std::array<std::uint8_t, 4> colour_bytes(const TypeRecord &rec, Raw age, const Vu &vu = Vu());
// VU1 0x085-0x0a6 (kinds 0, 1): the two sprite corners.  stale_zw: lanes z, w of vf18,
// never written by the loop (the first corner's z and ADC; the GS ignores them).
std::array<GsVertex, 2> sprite_vertices(const ParticleCell &cell, const TypeRecord &rec,
                                        const ParticleHeader &h, const Vu &vu = Vu(),
                                        std::array<Raw, 2> stale_zw = {0, 0});
// VU1 0x0a9-0x0e3 (kind 2): head (alpha as the colour says), tail (ALPHA 0; ADC from the
// head's clip test).
std::array<GsVertex, 2> line_vertices(const ParticleCell &cell, const TypeRecord &rec,
                                      const ParticleHeader &h, const Vu &vu = Vu(),
                                      WaitQ waitq = WaitQ::WaitFirst);

// particle_emitLists 0x194450: the blocks one list sends.  cells[c - 1] is the cell number
// (32 * block + c) of slot c = 1..30, or -1 for a cell the list zero-fills.
struct ListBlock {
    std::uint32_t page = 0, block = 0;
    std::array<std::int32_t, 30> cells{};
};
std::vector<ListBlock> list_blocks(const std::vector<std::uint32_t> &pages, std::uint32_t head,
                                   std::uint32_t tail);

// ---- Trails (trail_drawAll 0x193ad0, VU1 0x28 / 0x26 / 0x79e) ---------------------------
using TrailPoint = std::array<std::int16_t, 4>;     // 2x, 2y, 2z (half units), stamp

// The fields of one 0x410-byte trail (docs 7): ring +0x000, handle +0x400, clock +0x402,
// oldest +0x404, newest +0x405, row +0x406, flags +0x407, lifetime +0x408, rgba +0x40a,
// width / 4 +0x40f.
struct Trail {
    std::array<TrailPoint, 128> points{};
    std::uint16_t handle = 0;
    std::int16_t clock = 0;
    std::uint8_t oldest = 0, newest = 0;
    std::int8_t row = 0;
    std::uint8_t flags = 0;
    std::int16_t lifetime = 0;
    std::array<std::uint8_t, 4> rgba{};
    std::uint8_t width4 = 0;
};

// v of the texture rows (table 0x44fca8, docs 7: rows start at 0, 32, 64, 96, 128, 224,
// 256, 288, 351, the last row ends at 446) as raw patterns.  Callers may pass another.
using RowTable = std::array<Raw, 10>;
inline constexpr RowTable TRAIL_ROW_V = {0x00000000u, 0x42000000u, 0x42800000u, 0x42C00000u,
                                         0x43000000u, 0x43600000u, 0x43800000u, 0x43900000u,
                                         0x43AF8000u, 0x43DF8000u};
using Qword = std::array<std::uint32_t, 4>;
using TrailHeader = std::array<Qword, 6>;
// The six qwords uploaded before MSCAL 0x28.  Refusal: row outside 0..8 (the code would
// read past the table; not documented).
TrailHeader trail_header(const Trail &t, const RowTable &rows = TRAIL_ROW_V);
struct TrailChunk {
    std::uint32_t start = 0, count = 0;
};
std::vector<TrailChunk> trail_chunks(const Trail &t);
// Every vertex the trail programs send, in order.  m, vf6, vf7: what the particle packet
// left in VU1 (the global particle header).  Refusal: a paired chunk with an odd count.
std::vector<GsVertex> trail_vertices(const Trail &t, const Columns &m, const Lanes &vf6,
                                     const Lanes &vf7, const Vu &vu = Vu(),
                                     WaitQ waitq = WaitQ::WaitFirst,
                                     const RowTable &rows = TRAIL_ROW_V);

// ---- Beams (beam_create 0x193088, nodes 0x192a40, drift 0x192e80, points 0x192c50) ----
// The builders in 0x192a40-0x193fff do not call math_cos16 or math_sin16. Shape is
// the cosmetic seed and the offsets (effects.md 13.5).
inline constexpr std::uint32_t BEAM_COUNT = 20, BEAM_MAX_DEPTH = 5;
struct BeamNode {
    Vec3 off{}, drift{};
    std::uint32_t count = 0;
    std::array<std::uint32_t, 7> words() const;
};
// Refusal: depth < 0 (the recursion would not end).
std::vector<BeamNode> beam_build_nodes(Raw jitter, std::int32_t depth, core::Lcg &cosmetic,
                                       core::LibcRand &libc);
void beam_drift_nodes(std::vector<BeamNode> &nodes, Raw jitter, std::int32_t depth,
                      core::Lcg &cosmetic);
std::vector<Vec3> beam_points(const std::vector<BeamNode> &nodes, const Vec3 &a, const Vec3 &b,
                              std::int32_t depth, Raw scale, core::Lcg &cosmetic,
                              Raw amplitude = ONE);
std::uint32_t beam_frame_seed(std::uint32_t sim_frame, std::uint32_t index);
TrailPoint trail_point(const Vec3 &pos, std::int32_t stamp);    // trail_addPoint 0x192550
struct BeamTrailPoints {
    std::vector<TrailPoint> ring;
    std::int32_t clock = 0;
};
BeamTrailPoints beam_trail_points(const std::vector<Vec3> &points);

// ---- Glows (glow_add 0x18fc78, glow_draw 0x1907d0) --------------------------------------
inline constexpr std::size_t GLOW_MAX = 0x400;
inline constexpr std::uint64_t GLOW_DIMX_ON = 0x1212303012123030ull;
inline constexpr std::uint64_t GLOW_DIMX_OFF = 0x3131757531317575ull;
// GS register addresses of the A+D writes.
inline constexpr std::uint32_t REG_PRIM = 0x00, REG_RGBAQ = 0x01, REG_UV = 0x03,
                               REG_XYZ2 = 0x05, REG_TEX0_1 = 0x06, REG_XYZ3 = 0x0D,
                               REG_TEX1_1 = 0x14, REG_TEXFLUSH = 0x3F, REG_ALPHA_1 = 0x42,
                               REG_DIMX = 0x44, REG_TEST_1 = 0x47;

// One 0x20-byte record of g_glowQueue: pos, size, bias, rgba, texture word, angle, flags.
struct Glow {
    Vec3 pos{};
    Raw size = 0, bias = 0;
    std::array<std::uint8_t, 4> rgba{};
    std::uint32_t tex = 0;
    std::uint16_t angle = 0;
    std::int16_t flags = 0;
};
struct GlowScreen {
    std::int64_t x = 0, y = 0, z = 0, h = 0;
    Raw depth = 0;          // w + bias: not a GS field (the clip_w of the glow's vertices)
};
std::optional<GlowScreen> glow_project(const Glow &g, const Mat34 &cam, const Mat4 &viewproj);
struct AdWrite {
    std::uint32_t reg = 0;
    std::uint64_t value = 0;
    bool operator==(const AdWrite &o) const noexcept { return reg == o.reg && value == o.value; }
};
std::vector<AdWrite> glow_vertices(const Glow &g, std::int64_t x, std::int64_t y, std::int64_t z,
                                   std::int64_t h, std::int32_t tex_w, std::int32_t tex_h);
// Texture word -> (width, height) of its header (+0, +2).  A drawn glow whose texture is
// missing here is refused.
using TextureDims = std::map<std::uint32_t, std::pair<std::int32_t, std::int32_t>>;
struct GlowSprite {                 // one drawn glow inside a packet's events
    std::size_t first_event = 0, event_count = 0;
    GlowScreen screen{};
};
struct GlowPacket {
    std::int32_t key = 0;           // 4 for pass 1, 3 for pass 0
    std::uint32_t texture = 0, prim = 0, alpha1 = 0;
    std::vector<AdWrite> events;    // every A+D write, exact
    std::vector<GlowSprite> sprites;
};
// glow_draw(pass): one packet per (texture, flags) in queue order; records taken (texture
// cleared) by the packet that takes them, drawn or culled.
std::vector<GlowPacket> glow_packets(std::vector<Glow> &queue, std::int32_t pass,
                                     const Mat34 &cam, const Mat4 &viewproj,
                                     const TextureDims &dims);

// ---- Trail builders (EE side) ------------------------------------------------------------
std::array<TrailPoint, 2> trail_pair(Raw alpha, const Vec3 &a, const Vec3 &b, std::int32_t clock);
struct SplineResult {
    std::optional<Vec3> pos, tangent;
};
// spline_eval 0x1911e8.  pts may hold more than `count` points: at t = 1 the code reads one
// point past the array.  Refusal: an index outside pts.
SplineResult spline_eval(Raw t, const std::vector<Vec3> &pts, std::int32_t count,
                         bool want_pos = true, bool want_tangent = true);
struct SplineTrail {
    std::vector<Vec3> points;
    std::int16_t lifetime = 0;
};
// trail_setSpline 0x1918a0.  Refusal: count == 1 (division by zero), more than 2^20 points.
SplineTrail trail_set_spline(Raw start_t, Raw end_t, const std::vector<Vec3> &pts,
                             std::int32_t count, std::int32_t steps);
struct SweepPairs {
    std::vector<std::pair<Vec3, Vec3>> pairs;
    std::int32_t lifetime = 0;
};
// trail_sweepPairs 0x191ea0.  Refusal: steps == 2 (division by zero).
SweepPairs trail_sweep_pairs(const Vec3 &centre, const Vec3 &axis, Vec3 a, Vec3 b,
                             std::int32_t steps);
// trail_setCircle 0x1920e0, math_cos16 / math_sin16 of the step angle.
// Refusal: steps == 0 (division by zero).
std::vector<Vec3> trail_set_circle(Raw radius, const Vec3 &centre, const Vec3 &normal,
                                   std::int32_t steps);

// ---- Runtime stores ----------------------------------------------------------------------
// 64 trails (docs 7, 13.4, 13.8).  The generation counter (0x44fca4) is shared with beams.
//
// NATIVE (not documented for the original; see the report): a new trail takes the lowest
// free index; the counter goes up by one (8 bits, skipping a value that would make the
// handle 0) before each successful trail_create / beam_create; an operation whose handle
// does not match the slot's current handle is ignored.
class TrailStore {
public:
    static constexpr std::size_t COUNT = 64;
    std::array<Trail, COUNT> trails{};
    std::uint32_t generation = 0;

    // trail_create 0x190ff8: lifetime = trunc(60 * seconds) in the console's float rules,
    // width / 4 at +0x40f.  Returns the handle, or 0 when all 64 are used.
    std::uint16_t create(Raw lifetime_seconds, std::int32_t row, std::int32_t width,
                         std::uint32_t flags, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                         std::uint8_t a);
    Trail *find(std::uint32_t handle);
    const Trail *find(std::uint32_t handle) const;
    // trail_addPoint 0x192550: stamp = clock + 1; refused (false) when the ring would be
    // within two of full.
    bool add_point(std::uint32_t handle, const Vec3 &pos);
    // trail_addPair 0x192650: a stamped clock + 1, b's stamp field int(alpha * 255).
    bool add_pair(Raw alpha, std::uint32_t handle, const Vec3 &a, const Vec3 &b);
    void release(std::uint32_t handle);                                    // 0x192800
    void set_color(std::uint32_t handle, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                   std::uint8_t a);                                        // 0x191100
    void set_length(std::uint32_t handle, std::int32_t value);            // 0x1911a8: >> 2
    void clear(std::uint32_t handle);                                      // 0x191e58
    // The builders (13.8) empty the trail and add their points, the clock going up by one
    // after each point.  Refusal: more points than the ring accepts (126), not executed.
    void set_spline(Raw start_t, Raw end_t, std::uint32_t handle, const std::vector<Vec3> &pts,
                    std::int32_t count, std::int32_t steps);
    void sweep_pairs(std::uint32_t handle, const Vec3 &centre, const Vec3 &axis, const Vec3 &a,
                     const Vec3 &b, std::int32_t steps);
    void set_circle(Raw radius, std::uint32_t handle, const Vec3 &centre, const Vec3 &normal,
                    std::int32_t steps);
    // trail_updateAll 0x192840, once per tick from particle_updateTask.  hold: the flag at
    // 0x44fc98 (trails are not aged while it is set; who sets it is open, docs 18).
    // Per live trail without flag 1: clock += 1 unless flag 0x10, drop the oldest points
    // while clock - stamp >= lifetime (two at a time with flag 2), free the trail when no
    // point is left.  Refusals: a clock that would pass 0x7fff, a paired trail with a
    // single entry left (neither documented).
    void update(bool hold = false);
};

// 20 beams (docs 7, 13.5).  Callers own the cosmetic Lcg and LibcRand (per machine).
struct Beam {
    bool active = false;
    bool in_sim_tick = false;           // +0x72e: the expiry test uses g_netFrame
    std::uint32_t handle = 0;           // +0x730
    std::int32_t depth = 0;
    Vec3 from{}, to{};                  // +0x700, +0x70c
    Raw jitter = 0, scale = 0;          // +0x718, +0x71c
    std::uint16_t trail_a = 0;          // +0x720: drawn on odd g_drawFrame
    std::uint16_t trail_b = 0;          // +0x724: drawn on even g_drawFrame
    std::int32_t expiry = 0;            // +0x728
    std::vector<BeamNode> nodes;
};
class BeamStore {
public:
    explicit BeamStore(TrailStore &trails) : trails_(&trails) {}
    std::array<Beam, BEAM_COUNT> beams{};

    // beam_create 0x193088: two trails (lifetime 1 s, row 0, flags 5, the beam's width and
    // colour), each given `from` once; depth clamped to 5; expiry = sim_frame + 10.
    // rgba: the four colour bytes (how the original splits its t0 word is not documented,
    // so the caller decides).  Returns 0 when no beam or trail is free.  Refusal: depth < 0.
    std::uint32_t create(Raw jitter, Raw scale, const Vec3 &from, const Vec3 &to,
                         std::int32_t width, std::int32_t depth,
                         const std::array<std::uint8_t, 4> &rgba, std::int32_t sim_frame,
                         bool in_sim_tick, core::Lcg &cosmetic, core::LibcRand &libc);
    // beam_update 0x1933c8: ends, both trails' alpha (low byte), width >> 2 when width >= 0,
    // expiry = sim_frame + 2.
    void update(std::uint32_t handle, const Vec3 &from, const Vec3 &to, std::int32_t alpha,
                std::int32_t width, std::int32_t sim_frame);
    bool is_alive(std::uint32_t handle) const;                          // 0x193360
    void destroy(std::uint32_t handle);                                 // 0x193040
    // The beam part of trail_drawAll, once per draw: per active beam in index order,
    // destroyed when expiry < (net_frame if in_sim_tick else sim_frame); else, unless
    // frozen, the nodes drift with the LIVE cosmetic seed; both trails emptied; the seed
    // set to beam_frame_seed(sim_frame, index) for beam_points (amplitude 1) and put back;
    // the points go to trail_b on even draw_frame, trail_a on odd.
    void draw_step(std::int32_t sim_frame, std::int32_t net_frame, std::uint32_t draw_frame,
                   bool frozen, core::Lcg &cosmetic);
    TrailStore &trails() { return *trails_; }

private:
    TrailStore *trails_;
};

// g_glowQueue: at most 0x400 records a frame, the 0x401st dropped (add returns false).
class GlowQueue {
public:
    std::vector<Glow> records;
    bool add(const Glow &g);
    void clear() { records.clear(); }
};

// ---- The particle packet input ------------------------------------------------------------
// A plain description of the particle pool as particle_draw reads it, for the pool part to
// fill.  pages[i] is page i (0x1000 bytes = 256 cells); a list holds its pages oldest first
// with the first live cell `head` in its first page and the cursor `tail` (next free cell)
// in its last; a list with no pages is not sent.  lists[kind * 4 + life class], kinds 0..2
// (particle_emitLists does not draw kind 3).  Whether particle_drawHaze runs only for
// kind 3 is not established: draw_haze refuses that question and the screen-axis
// question.  types: the 32 type slots' records (slot 0: the held, invisible
// definition).  The flipbook step of particle_draw (docs 4, rows 1, 2, 4) belongs to the
// pool: apply it to `types` before drawing.
using ParticlePage = std::array<ParticleCell, 256>;
struct ParticleList {
    std::vector<std::uint32_t> pages;
    std::uint32_t head = 0, tail = 0;
};
struct ParticleListSet {
    Mat34 matrix{};                     // a group's mat34 (+0x100); unused for the global set
    std::array<ParticleList, 12> lists{};
};
struct ParticleFrame {
    std::int32_t clock = 0;             // the particle clock (s16)
    std::vector<ParticlePage> pages;
    std::array<TypeRecord, 32> types{};
    ParticleListSet global;
    std::vector<ParticleListSet> groups; // LIVE groups only, in index order 0..15
};

// ---- Packets and the frame ----------------------------------------------------------------
struct ParticleDraw {
    std::uint32_t set = 0;              // 0 global, 1 + n the n-th live group
    std::uint32_t kind = 0, life = 0;
    std::uint32_t prim = 0;             // PRIM_SPRITE or PRIM_LINE
    std::uint32_t alpha1 = 0;           // ALPHA_1 in effect (0x44 kind 0, 0x48 kinds 1, 2)
    std::uint32_t slot = 0;
    bool zero_filled = false;           // a cell the list zero-fills (UNPACK before MSCAL 0x17)
    std::array<GsVertex, 2> v{};
};
struct ParticlePacket {
    std::vector<std::uint32_t> alpha_writes;   // ALPHA_1: 0x44 before kind 0, 0x48 before kind 1
    std::vector<ParticleDraw> draws;           // every cell sent, in GS order
};
// particle_draw's packet: global set, each live group (group_matrix), then the global
// matrix again.  Refusal: a page index outside frame.pages.
ParticlePacket particle_packet(const ParticleFrame &frame, const Mat34 &view, Raw proj_scale,
                               std::array<Raw, 2> stale_zw = {0, 0},
                               WaitQ waitq = WaitQ::WaitFirst, const Vu &vu = Vu());
// particle_drawHaze 0x18fe18 reads one precomputed circle pair. The pair itself is
// trig::haze_circle. Placing it is refused: the screen axis of each word is not
// established, and it is not established that only kind 3 reaches the function.
void draw_haze();

struct TrailDraw {
    std::uint32_t index = 0;
    std::uint16_t handle = 0;
    TrailHeader header{};
    std::uint32_t prim = 0, alpha1 = 0;
    std::vector<GsVertex> vertices;
};
struct TrailPacket {
    std::vector<TrailDraw> trails;             // live trails with points, index order
};
// trail_drawAll's strips (after BeamStore::draw_step): m, vf6, vf7 from the particle packet.
TrailPacket trail_packet(const TrailStore &store, const ParticleHeader &h,
                         WaitQ waitq = WaitQ::WaitFirst, const RowTable &rows = TRAIL_ROW_V,
                         const Vu &vu = Vu());

enum class PacketKind { Particles, Trails, Glow };
struct Packet {
    PacketKind kind = PacketKind::Particles;
    std::int32_t key = 0;
    std::uint32_t texture = 0;          // identity used by the sorted list
    ParticlePacket particles;           // kind Particles
    TrailPacket trails;                 // kind Trails
    GlowPacket glow;                    // kind Glow
};

// gfx_addSortedPacket 0x16b7f0 / gfx_dmaHandler 0x16b998 (O, docs 13.3): keys drawn 0..7;
// within a key a packet whose texture already has an entry goes right after that texture's
// last entry, otherwise to the front of the key's list.
class SortedPackets {
public:
    void add(Packet p);
    std::vector<Packet> gs_order() const;

private:
    std::array<std::vector<Packet>, 8> keys_;
};

struct FrameInput {
    Mat34 view{};
    Raw proj_scale = 0;
    Mat34 camera{};                     // g_cameraMatrix (glows)
    Mat4 viewproj{};                    // g_viewProj (glows)
    std::uint32_t particle_texture = 1, trail_texture = 2;   // sorted-list identities
    std::array<Raw, 2> stale_zw{0, 0};
    WaitQ waitq = WaitQ::WaitFirst;
    RowTable rows = TRAIL_ROW_V;
    std::int32_t sim_frame = 0, net_frame = 0;
    std::uint32_t draw_frame = 0;
    bool frozen = false;
};
// One draw-task call, packets in GS order (docs 13.3): the beams' draw step, then the
// packets added in the order trail packet, glow_draw(1), particle packet, glow_draw(0)
// (I: the only add order the documented GS order and the insertion rule allow), so the GS
// sees key 3 glow_draw(0) packets, then key 4: the particle packet, the glow_draw(1)
// packets, the trail packet.  The glow queue is emptied.  The particle and trail packets
// are always present (possibly empty; NATIVE).  `beams` may be null.
std::vector<Packet> draw_frame(const FrameInput &in, const ParticleFrame &particles,
                               TrailStore &trails, BeamStore *beams, GlowQueue &glows,
                               const TextureDims &glow_dims, core::Lcg &cosmetic);

} // namespace champions::rta::fx::render
