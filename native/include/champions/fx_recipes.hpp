#pragma once

#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
#include "champions/core_state.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

// One-shot, death, body-part and status effect recipes of Champions: Return to Arms
// (SLUS_209.73, US v2.00): a handwritten port of the research contract
// tools/ref_fx_recipes.py (research commit 19f80a6, docs/effects.md sections 10 and 14).
//
// Every float is a raw 32-bit pattern computed with the portable console arithmetic of
// core_arithmetic.hpp (the same ref_core.py contract), in the order the original makes
// it, so the positions and velocities handed to particle_spawn match bit for bit.
// Particle definitions are named by ADDRESS only: the address is the key a client uses to
// look the values up in the JSON fx_particle_defs.py reads from the user's executable.
//
// A recipe does not draw anything. It returns the ordered list of primitive calls the
// original makes (Call below) and updates the caller-owned generators and state blocks.
// The per-machine answers (camera focus, dialog, pool, beam and trail handles) and the
// model queries are supplied by the caller exactly where the reference obtains them.
//
// Not everything here is cosmetic (docs/effects.md section 17): fx_spawn id 5 and the
// ice shatter draw the SHARED seed (g_randSeed 0x404464). A client must run those draws
// in its simulation on every machine even when it draws nothing.
//
// Refusals. Where the original's behaviour is not documented (a division by zero, a part
// the reference does not model) the recipe throws Refusal and leaves every caller-owned
// object (generators, made flags, state blocks, items, creatures) unchanged.
namespace champions::rta::fx::recipes {

using Raw = champions::simulation::rta::Raw;
using Vec3 = champions::simulation::rta::Vec3;
using Mat34 = champions::simulation::rta::Mat34;
using Lcg = champions::simulation::rta::Lcg;

struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------------------
// Call records
// ---------------------------------------------------------------------------------------
// Field use per kind (reference tuple order; f = float patterns, v = vectors, i = integers,
// s = names). Integers keep the reference's sign: yaw and angle are s16, sample and
// attach indices s32, the beam_update alpha s32; everything else is unsigned.
//
//   Ctor            particleDef_ctor 0x18f3d0           i0 definition
//   Near            isNearCameraFocus480 0x1ebd90       v0 pos
//   Spawn           particle_spawn 0x18f3f8             f0 backdate, i0 definition, v0 pos, v1 vel
//   SoundName       sprintf of a numbered sound name    i0 n
//   ResFind         res_find 0x17365c                   s0 lump, s1 name (nullptr: formatted name)
//   Sound           sound_playAtPos 0x214428            f0 volume, v0 pos
//   PoolNew         pool_new 0x14fdb8                   i0 size
//   Fire            Fire_ctor 0x2020e0                  v0 pos
//   Hide            obj +0xc |= 2                       -
//   SampleCount     model_getSamplePointCount 0x1799c8  -
//   Sample          model_getSamplePoint 0x178e18       f0 scale, i0 anim, v0 pos, i1 yaw, i2 index
//   PhProp          PhProp_ctor 0x155a0c                f0 scale, v0 pos, s0 lump, s1 model, s2 texture,
//                                                       i0 part mask, i1 launch flags, i2 item
//   VTable          table stored at block +0x48         i0 table
//   Mask            model_buildHideMask 0x179550        i0 model, i1 variant
//   Attach          model_getAttachPoint 0x1754a0       f0 scale, i0 model, v0 pos, i1 yaw, i2 anim, i3 index
//   Gut             GutChunk_ctorFromProp 0x201ad0      f0 scale, v0 pos, i0 yaw, i1 model, i2 anim,
//                                                       i3 lump, i4 model name, i5 texture,
//                                                       mask64, i6 launch, i7 blood bits
//   Lift            chunk +0x1c4 += 2.0                 -
//   GetModel        the object's model slot 0x84/0x80   -
//   BeamCreate      beam_create 0x193088                f0 jitter, f1 scale, v0 a, v1 b, i0 width,
//                                                       i1 depth, i2 rgba
//   BeamUpdate      beam_update 0x1933c8                i0 handle, v0 a, v1 b, i1 alpha, i2 width
//   Glow            glow_add 0x18fc78                   f0 bias, f1 size, v0 pos, rgba, i0 texture,
//                                                       i1 angle, i2 flags
//   DialogOpen      dialog_isOpen 0x22fbc0              -
//   AttachMatrix    model_getAttachMatrix 0x1795f8      f0 scale, i0 anim, i1 index
//   EffectSegment   model_getEffectSegment 0x1791a0     -
//   TrailCreate     trail_create 0x190ff8               f0 seconds, i0 row, i1 width, i2 flags,
//                                                       i3 r, i4 g, i5 b, i6 a
//   TrailPair       trail_addPair 0x192650              f0 width alpha, i0 handle, v0 tip, v1 base
//   FxSpawn         fx_spawn 0x1ef270 (not expanded)    i0 id, v0 pos
//   TransformSample model_transformSamplePoint 0x1799e8 matrix, i0 index
//   BeamAlive       beam_isAlive 0x193360               i0 handle
enum class Kind : std::uint8_t {
  Ctor, Near, Spawn, SoundName, ResFind, Sound, PoolNew, Fire, Hide, SampleCount, Sample,
  PhProp, VTable, Mask, Attach, Gut, Lift, GetModel, BeamCreate, BeamUpdate, Glow,
  DialogOpen, AttachMatrix, EffectSegment, TrailCreate, TrailPair, FxSpawn,
  TransformSample, BeamAlive
};

struct Call {
  Kind kind = Kind::Hide;
  std::array<Raw, 2> f{};
  std::array<Vec3, 2> v{};
  std::array<std::int64_t, 8> i{};
  std::uint64_t mask64 = 0;
  std::array<std::uint32_t, 4> rgba{};
  Mat34 matrix{};
  std::array<const char *, 3> s{};

  // The canonical text form: the reference tuple, name first, every integer and pattern
  // in lowercase hex ("-" before a negative integer), vectors as [a,b,c], fields
  // separated by one space.
  std::string text() const;
  bool operator==(const Call &other) const { return text() == other.text(); }
};
using Calls = std::vector<Call>;
// The calls' text forms joined by ';'.
std::string stream_text(const Calls &calls);

// pool_new(size) -> block, 0 for an exhausted pool. An empty function answers 1.
using PoolNew = std::function<std::uint32_t(std::uint32_t size)>;

// ---------------------------------------------------------------------------------------
// fx_spawn 0x1ef270 (id, pos*)  - docs/effects.md 14.1
// ---------------------------------------------------------------------------------------
inline constexpr std::array<std::uint32_t, 24> FX_SPAWN_DEFS{
    0x86FC38, 0x86FC88, 0x86FCD8, 0x86FD28, 0x86FD78, 0x86FDC8, 0x86FE18, 0x86FE68,
    0x86FEB8, 0x86FF08, 0x86FF58, 0x86FFA8, 0x86FFF8, 0x870048, 0x870098, 0x8700E8,
    0x870138, 0x870188, 0x8701D8, 0x870228, 0x870278, 0x8702C8, 0x870318, 0x870368};
inline constexpr std::uint32_t FX_SPAWN_MADE_FLAGS = 0x4F87BC; // + 4 i
inline constexpr std::uint32_t DEF_BLOOD_RED = 0x8704A8, DEF_BLOOD_RED_90 = 0x8704F8,
                               DEF_DUST = 0x4F8710;
inline constexpr std::int32_t FX_SPAWN_LAST_ID = 0x16;

// The 24 "made" flags at 0x4f87bc, in FX_SPAWN_DEFS order (caller-owned, per machine).
struct MadeFlags {
  std::array<bool, 24> made{};
};

// cosmetic: g_cosmeticSeed 0x404460; shared: g_randSeed 0x404464 (only id 5 draws it).
// near: what isNearCameraFocus480 answers. gore: g_decalDetail 0x44fdc0. made: nullptr
// when every definition is already constructed.
Calls fx_spawn(std::int32_t id, const Vec3 &pos, Lcg &cosmetic, Lcg &shared, bool near = true,
               std::int32_t gore = 2, MadeFlags *made = nullptr, const PoolNew &pool_new = {});

// ---------------------------------------------------------------------------------------
// Models (sample points) - the queries a native client answers from its own model code
// ---------------------------------------------------------------------------------------
struct SamplePoint {
  Vec3 point{};
  Raw size = 0;
};

// A posed model read through its sample points (difftest: a synthetic table).
class SampleModel {
public:
  virtual ~SampleModel() = default;
  // model_getSamplePointCount 0x1799c8
  virtual std::int32_t sample_count() = 0;
  // model_getSamplePoint 0x178e18: world position of sample point `index` of the model
  // posed with `anim` at pos / yaw, scaled, and its size value.
  virtual SamplePoint sample_point(Raw scale, std::uint32_t anim, const Vec3 &pos,
                                   std::int32_t yaw, std::int32_t index) = 0;
};

// fx_disintegrateModel 0x1f4340 [disintegrate], docs/effects.md 14.3. Cosmetic seed only.
Calls disintegrate_model(SampleModel &model, const Vec3 &pos, std::uint32_t yaw16, Raw scale,
                         Raw offset, bool alternate, std::uint32_t anim, Lcg &cosmetic);

// fx_shatterToLooseIce 0x1fb240 [ice shatter], docs/effects.md 14.4. SHARED seed.
// Refuses a count whose (int) is 0 (the original divides by it) and a model whose sample
// count drops to 0 between the queries (the reference has no answer for that modulo).
Calls shatter_to_loose_ice(SampleModel &model, const Vec3 &pos, std::uint32_t yaw16, Raw count,
                           Raw scale, Raw offset, std::uint32_t anim, Lcg &shared,
                           const PoolNew &pool_new = {});

// ---------------------------------------------------------------------------------------
// Losing a body part: the eight callers of GutChunk_ctorFromProp 0x201ad0 (14.5)
// ---------------------------------------------------------------------------------------
enum class PartClass : std::uint8_t {
  Ant,        // 0x236b0c
  AntQueen,   // 0x238360, no caller in the executable
  FireBeetle, // 0x247130
  Ghoul,      // 0x2672f4, no caller in the executable
  Mosquito,   // 0x2982cc
  Orc,        // 0x2a3de0
  Scorpion,   // 0x2a59f0
  BlackWidow  // 0x2acfd8
};
std::uint32_t part_class_address(PartClass cls);
const char *part_class_name(PartClass cls); // the reference's names ("AntQueen?", "Ghoul?")

// The creature fields the part functions read and write (patterns for floats).
struct Creature {
  std::uint32_t variant = 0;            // +0xe8 model-variant word
  std::array<std::uint32_t, 8> cls{};   // class data words +0 ... +0x1c (+0xc is the model)
  Vec3 pos{};                           // +0x10
  std::uint32_t yaw = 0;                // +0x38 (16 bits)
  std::uint32_t anim = 0;               // +0x114 anim state
  std::int32_t colour = 0;              // +0x3c8 colour byte (Ant)
  Raw scale = champions::simulation::rta::ONE; // +0xdc (BlackWidow)
  std::int32_t kind = 0;                // +0x414 (BlackWidow: 1 = do nothing)
  std::uint64_t show = 0;               // +0x220 show mask, written
};
// model_buildHideMask 0x179550 (model = class data +0xc, variant word) -> 64-bit mask.
using ShowMask = std::function<std::uint64_t(std::uint32_t model, std::uint32_t variant)>;

// Orc part 1: the head chunk's 64-bit mesh mask from the head variants already set.
std::uint64_t orc_head_mask(std::uint32_t variant);

// Refuses Orc parts 6 and 7 (drop a held item as a PhProp: not modelled by the
// reference, never passed by the only caller).
Calls lose_body_part(PartClass cls, Creature &creature, std::uint32_t part,
                     const ShowMask &show_mask, const PoolNew &pool_new = {});

// ---------------------------------------------------------------------------------------
// Model-vertex helpers (14.7)
// ---------------------------------------------------------------------------------------
// fx_emitFromModelVerts 0x1eb1e8. direction / offset: nullptr for a 0 pointer.
// Refuses particles from a model without sample points (the original divides by 0).
Calls emit_from_model_verts(SampleModel &model, const Vec3 &pos, std::uint32_t yaw16,
                            std::uint32_t anim, std::int32_t count, std::uint32_t definition,
                            const Vec3 *direction, const Vec3 *offset, Raw speed,
                            Lcg &cosmetic);

// fx_burnModel 0x1f5210 (fx_modelParticlesNearCamera_1f5210). model: nullptr for 0.
Calls model_sparkle(SampleModel *model, const Vec3 &pos, std::uint32_t yaw16,
                    std::uint32_t anim, Raw amount, Raw offset, Lcg &cosmetic,
                    bool near = true);

// The 0x18 bytes at t0 + 8 of fx_shockModel: three arcs of (s16 index a, s16 index b,
// beam handle).
struct Arc {
  std::int32_t a = 0, b = 0;
  std::uint32_t beam = 0;
};
struct ArcState {
  std::array<Arc, 3> arcs{};
};
// beam_create(a, b) -> handle (per machine). An empty function answers 0.
using ArcBeamCreate = std::function<std::uint32_t(const Vec3 &a, const Vec3 &b)>;

// fx_shockModel 0x1f61c0 (fx_modelParticlesNearCamera_1f61c0). model: nullptr for 0.
Calls model_arcs(SampleModel *model, const Vec3 &pos, std::uint32_t yaw16, std::uint32_t anim,
                 Raw alpha, Raw scale, Raw offset, ArcState &state, std::uint32_t rgba,
                 Lcg &cosmetic, bool near = true, const ArcBeamCreate &beam_create = {});

// Object +0x70 (ramp A) and +0x80 (ramp B, the glow colour): four patterns each.
struct GlowRamp {
  std::array<Raw, 4> a{}, b{};
};
// fx_drawModelGlowNearCamera 0x1f5890, per rendered frame. obj_flags: nullptr for a 0
// object, else its +0xc word. target_a / target_b: four 16-bit values each (read as s16).
Calls model_glow(SampleModel *model, const std::uint32_t *obj_flags, GlowRamp &ramp,
                 const Vec3 &pos, std::uint32_t yaw16, std::uint32_t anim, Raw scale,
                 Raw offset, Raw fade, const std::array<std::uint32_t, 4> &target_a,
                 const std::array<std::uint32_t, 4> &target_b, std::int32_t mode,
                 std::int32_t size_add, std::uint32_t sim_frame, bool near = true);

// ---------------------------------------------------------------------------------------
// Weapon and status visuals: fx_modelStatusEffects 0x1f6ac8 (14.6)
// ---------------------------------------------------------------------------------------
inline constexpr std::int32_t CODE_LIGHTNING = 0x18, CODE_COLD = 0x16, CODE_FIRE = 0x15,
                              CODE_POISON = 0x17, CODE_DISEASE = 0x26;
inline constexpr std::uint32_t LEVEL_FLAG_NO_STATUS_FX = 0x10000000; // 0x44fe08
inline constexpr std::uint32_t LEVEL_FLAG_NO_SWING_TRAIL = 0x100;

// The weapon model: sample points through a caller matrix, the blade segment, and the
// attach base byte +0x13.
class WeaponModel {
public:
  virtual ~WeaponModel() = default;
  virtual std::int32_t sample_count() = 0;                                  // 0x1799c8
  virtual SamplePoint transform_sample_point(const Mat34 &m, std::int32_t index) = 0; // 0x1799e8
  virtual std::array<Vec3, 2> effect_segment() = 0;                         // 0x1791a0: a, b
  virtual std::uint32_t attach_base() = 0;                                  // model +0x13 (s8)
};
// The holder's model: model_getAttachMatrix 0x1795f8.
class HolderModel {
public:
  virtual ~HolderModel() = default;
  virtual Mat34 attach_matrix(Raw scale, std::uint32_t anim, std::int32_t index) = 0;
};

// The caller's 0x40-byte state block (a2). The small fields keep the reference's integer
// values; the original stores their low byte.
struct WeaponFxState {
  Mat34 matrix{};             // +0x00 last matrix
  std::uint32_t frame = 0;    // +0x30 g_simFrame of the last swing
  std::uint32_t beam = 0;     // +0x34 lightning beam handle
  std::uint32_t trail = 0;    // +0x38 swing trail handle
  std::int32_t counter = 0;   // +0x3c swing counter (s8, 0 ... 100)
  std::int32_t spark_kind = 0; // +0x3d
  std::int32_t sparks = 0;    // +0x3e sparks left (s8)
};
// The item record (a0): +0x10 glow colour, +0x18 second colour (four s16 each), and the
// four effect entries at +0x24 (s16 code, s16 value).
struct WeaponItem {
  std::array<std::int32_t, 4> glow{}, colour2{};
  std::array<std::array<std::uint32_t, 2>, 4> entries{};
  // The value of the first entry with this code (0 when none).
  std::int32_t value(std::int32_t code) const;
};
// Per-machine answers of the primitives fx_modelStatusEffects asks.
struct WeaponAnswers {
  std::uint32_t trail_create = 0, beam_create = 0, beam_alive = 0;
};
struct WeaponContext {
  std::uint32_t level_flags = 0; // 0x44fe08
  std::int32_t gore = 2;         // g_decalDetail 0x44fdc0
  std::uint32_t sim_frame = 0;   // g_simFrame 0x44fdcc
  bool dialog_open = false;      // dialog_isOpen 0x22fbc0 (per machine)
  WeaponAnswers answers{};
};
// model: nullptr for 0; holder: nullptr for 0; override_matrix: nullptr for 0.
// Refuses an element that needs a random weapon sample point when the weapon model has
// none (the original divides by 0).
Calls model_status_effects(Raw amount, Raw scale, WeaponItem &item, WeaponModel *model,
                           WeaponFxState &state, const Vec3 &pos, std::uint32_t yaw16,
                           HolderModel *holder, std::uint32_t anim, bool swinging,
                           std::int32_t joint_add, const Mat34 *override_matrix,
                           Lcg &cosmetic, const WeaponContext &context = {});

} // namespace champions::rta::fx::recipes
