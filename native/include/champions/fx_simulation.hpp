#pragma once
// Return to Arms effects, wired together (native-fx-2).  Two halves, kept apart on purpose:
//
//   EffectSimulation  runs on EVERY machine, drawing or not.  docs/effects.md 15.2 and 17:
//                     effect classes place particles with the object's (+0x44), the spell's
//                     (+0x10) or the shared generator (0x404464), every LightEffect makes one
//                     shared draw in its constructor, fx_spawn id 5 and the ice shatter draw
//                     the shared seed, and DiseaseCloud and StaticFire deal damage from their
//                     "visual" run functions.  This half makes those draws and hands the
//                     gameplay results (damage, objects to construct, path blocking) to the
//                     combat runtime through GameHooks.  It owns no renderer.
//   EffectWorld       the per-machine effect state the primitives fill (particle pools, trails,
//                     beams, glows, decals, light effects, the cosmetic seed and libc rand()),
//                     and the draw task that turns it into what the GS receives.  Optional:
//                     a headless client attaches none and loses nothing that other machines
//                     see.
//
// Everything is RTA only and Godot-free.  Particle definition values come from the user's
// executable at run time (tools/fx_particle_defs.py --json), skill-recipe constants likewise
// (tools/fx_skill_recipes.py --json); no retail value is compiled in.  Contract: research
// t3/research-fx 19f80a6, docs/effects.md 12-17, docs/lighting.md 9.
#include "champions/core_math.hpp"
#include "champions/core_state.hpp"
#include "champions/fx_decals.hpp"
#include "champions/fx_exact.hpp"
#include "champions/fx_recipes.hpp"
#include "champions/fx_render.hpp"
#include "champions/fx_skills.hpp"
#include "champions/lighting.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace champions::rta::fx::sim {
namespace core = champions::simulation::rta;
using Raw = core::Raw;
using Vec3 = core::Vec3;     // three raw float patterns
using Mat34 = core::Mat34;
using Mat4 = core::Mat4;

struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---- glow textures (docs/effects.md 14.2) -------------------------------------------------
// A glow's texture word is what the drawing code loaded from one of these globals.  The
// native convention: each global holds its own address as a token, so the word names the
// texture.  Every texture is read from the user's own files by lump and name.
struct GlowTexture {
  std::uint32_t global;
  const char *lump;
  const char *name;
};
const std::vector<GlowTexture> &glow_textures();
const GlowTexture *glow_texture(std::uint32_t word);
// The other effect textures (docs/effects.md 12.2, 13.9).
inline constexpr const char *ENGINE_LUMP = "engine.lmp", *FX_LUMP = "fx.lmp";
inline constexpr const char *PARTICLE_SHEET = "particletex.tex", *TRAIL_TEXTURE = "trail.tex",
                            *FLOOR_TEXTURE = "floorsprite.tex";
// The particle sheet: 512 x 160 texels used (16 columns x 5 rows of 32 x 32 cells; the GS
// sees 512 x 256).  A cell at v >= 160 samples GS memory below the sheet (U): refused.
inline constexpr int SHEET_WIDTH = 512, SHEET_HEIGHT = 160;
inline constexpr int TRAIL_WIDTH = 128, TRAIL_HEIGHT = 448, FLOOR_WIDTH = 256, FLOOR_HEIGHT = 128;

// ---- the per-machine effect state ------------------------------------------------------------
// The game camera for one draw-task call: the view matrix the particle packet uses,
// g_projScale, g_cameraMatrix and g_viewProj (the glows).  The caller supplies all four; how
// the game derives them is outside this contract.
struct Camera {
  Mat34 view{};
  Raw proj_scale = 0;
  Mat34 camera{};
  Mat4 viewproj{};
};

// A light linked by an effect: either a LightEffect object or a class's own light record.
struct LinkedLight {
  lighting::Node node;
};

class EffectWorld {
  friend struct WorldCodec;   // fx_world_save.cpp: exact snapshot of the per-machine state
public:
  EffectWorld();
  EffectWorld(const EffectWorld &) = delete;
  EffectWorld &operator=(const EffectWorld &) = delete;

  // fx_particle_defs.py --json.  Refused while any definition is resident (the pools point
  // at them) or when the input is invalid; nothing changes then.
  bool load_definitions(const std::string &json, std::string &error);
  const exact::ParticleDef *definition(std::uint32_t address) const;
  // A definition built at run time from the user's data (a level emitter record, world_fixup;
  // AddParticle): refused while the address is resident.
  bool add_definition(std::uint32_t address, const exact::ParticleDef &definition);

  // ---- primitives (what the original's primitive functions do) ----
  // particle_spawn 0x18f3f8.  A definition missing from the user's JSON is counted, not
  // guessed.  Pool drops are silent, as in the original.
  void particle_spawn(std::uint32_t definition, const Vec3 &pos, const Vec3 &vel, Raw backdate);
  // The same with binary64 inputs (emitter_run hands its positions over unrounded).
  void particle_spawn_unrounded(std::uint32_t definition, const exact::Vec3d &pos,
                                const exact::Vec3d &vel, double backdate = 0.0);
  bool glow_add(const render::Glow &glow);
  std::uint32_t group_create();
  void group_begin(std::uint32_t handle);
  void group_end(std::uint32_t handle);
  void group_free(std::uint32_t handle);
  void group_set_matrix(std::uint32_t handle, const Mat34 &matrix);
  // particleGroup_setMatrix with a matrix the call-out does not carry (the reference's
  // 'stack' token, still used by skills_c's zero matrix): the group is drawn nowhere.
  void group_set_unknown_matrix(std::uint32_t handle);
  // decal_add 0x19f110; the tint is light_sampleFloor at the position (lighting).
  void decal_add(Raw size, const Vec3 &pos, std::int32_t angle, std::int32_t type, std::int32_t grow);
  // LightEffect_ctor 0x201bd8 (the light part; the shared draw of its GameObject is the
  // simulation's).  follow: an object handle whose position the caller reports each tick.
  void light_effect(const Vec3 &pos, std::uint32_t follow, bool signed_colour,
                    const std::array<Raw, 4> &c0, const std::array<Raw, 4> &c1, Raw radius,
                    Raw radius_end, Raw floor, Raw seconds);
  // A class's own light node (light_link / light_relink); key: the record's address.
  void link_light(std::uint64_t key, const lighting::Node &node);
  void unlink_light(std::uint64_t key);

  // Apply recorded calls.  Recipe records: spawns, glows, trails, beams; gameplay and query
  // records are ignored here (EffectSimulation has acted on them).  Returns the handle of a
  // create call, else 0.
  std::uint32_t apply(const recipes::Call &call);
  void apply(const recipes::Calls &calls);
  // One decoded skill call-out (skills::to_primitive).  Returns the value the original
  // primitive returns (a trail, beam or group handle; 0 otherwise).
  std::uint32_t apply(const skills::FxCall &call);

  // ---- clocks ----
  // particle_updateTask 0x18f970 for one simulated tick: particles (cutscene: groups do
  // not idle), trails (hold_trails: 0x44fc98 set; who sets it is open, docs 18), light
  // effects (followed objects at `positions`).
  void tick(std::int32_t sim_frame, bool cutscene = false, bool hold_trails = false,
            const std::map<std::uint32_t, Vec3> &positions = {});
  // One draw-task call: the flipbook (rows 1, 2, 4), decal growth unless frozen, the beams'
  // per-draw rebuild, and the packets in GS order (docs 13.3).
  std::vector<render::Packet> draw(const Camera &camera, std::int32_t sim_frame,
                                   std::int32_t net_frame, bool frozen = false);

  // The light nodes in effect now: linked class lights, then light effects.
  std::vector<lighting::Node> light_nodes() const;

  // ---- state (read access for presentation and tests) ----
  exact::ParticleSystem particles;
  render::TrailStore trails;
  render::BeamStore beams{trails};
  render::GlowQueue glows;
  decals::DecalStore decals;
  lighting::Level level;
  core::Lcg cosmetic;          // g_cosmeticSeed 0x404460 (per machine)
  core::LibcRand libc;         // newlib rand() state (per machine)
  recipes::MadeFlags made;     // fx_spawn's 24 constructed-definition flags (per machine)
  std::uint32_t draw_frame = 0;
  bool in_sim_tick = false;    // g_inSimTick: beams made now expire on g_netFrame
  render::WaitQ waitq = render::WaitQ::WaitFirst;
  std::array<Raw, 2> stale_zw{0, 0};
  render::TextureDims glow_dims;   // texture word -> header width/height (user's files)
  std::int64_t missing_definitions = 0;
  // The same misses by definition address (counted, never guessed): what a caller
  // reports as a refusal (review finding 5).
  std::map<std::uint32_t, std::int64_t> missing_by_definition;
  std::set<std::uint32_t> unplaced_groups;   // group slots whose matrix is unknown
  // isNearCameraFocus480 0x1ebd90 on this machine (an fx_spawn a recipe makes); none: near.
  std::function<bool(const Vec3 &)> near_camera_focus;
  std::int32_t sim_frame() const { return sim_frame_; }

private:
  std::map<std::uint32_t, exact::ParticleDef> definitions_;
  std::int32_t sim_frame_ = 0;
  struct Effect {
    lighting::LightEffect light;
    std::uint32_t follow = 0;
    lighting::Node node;
  };
  std::vector<Effect> effects_;
  std::map<std::uint64_t, lighting::Node> linked_;
  render::ParticleFrame particle_frame() const;
};

// ---- the simulation half ----------------------------------------------------------------------
// What the combat runtime supplies.  Effects never invent a gameplay answer: a missing hook
// for something that changes gameplay (damage, objects, path nodes) refuses.
struct GameHooks {
  // Any call-out of an effect-class function that is not an effect primitive (damage_apply,
  // world_collectObjectsInBox, terrain_height, isNearView, obj_moveAndCollide, sounds, model
  // sample points, constructors ...).  nullopt = no answer: the reference's default answer is
  // used for queries, and a gameplay call-out refuses.
  std::function<std::optional<skills::Value>(const skills::Callout &)> callout;
  // pool_new(size) of fx_spawn id 5 and the ice shatter (the shared object pool).
  std::function<std::uint32_t(std::uint32_t size)> pool_new;
  // The objects those recipes construct: Fire_ctor (id 5), PhProp_ctor of a LooseIceChunk,
  // GutChunk_ctorFromProp, with the call record that describes it.
  std::function<void(std::uint32_t block, const recipes::Call &ctor)> construct;
};

class EffectSimulation {
public:
  // shared: the game's g_randSeed 0x404464 (in the sync hash).  constants: the skill
  // recipe constants (fx_skill_recipes.py --json) for the round-2 effect classes.
  EffectSimulation(core::Lcg &shared, std::shared_ptr<const skills::RecipeConstants> constants);
  // nullptr: headless.  Only per-machine output depends on it.
  void attach(EffectWorld *world);
  EffectWorld *world() const { return world_; }
  GameHooks hooks;

  // fx_spawn 0x1ef270.  Id 5 (Fire objects, shared seed, no camera gate) always runs; the
  // other ids are cosmetic and run only when a world is attached.  near: what
  // isNearCameraFocus480 answers on this machine.  gore: g_decalDetail.
  recipes::Calls fx_spawn(std::int32_t id, const Vec3 &pos, bool near, std::int32_t gore);
  // fx_shatterToLooseIce 0x1fb240: shared seed, always runs; chunks go to hooks.construct.
  recipes::Calls shatter_to_loose_ice(recipes::SampleModel &model, const Vec3 &pos,
                                      std::uint32_t yaw16, Raw count, Raw scale, Raw offset,
                                      std::uint32_t anim);
  // The eight lose-a-body-part functions (the GutChunk goes to hooks.construct).
  recipes::Calls lose_body_part(recipes::PartClass cls, recipes::Creature &creature,
                                std::uint32_t part, const recipes::ShowMask &show_mask);
  // Cosmetic recipes (cosmetic seed, per machine): run only with a world.
  recipes::Calls disintegrate_model(recipes::SampleModel &model, const Vec3 &pos,
                                    std::uint32_t yaw16, Raw scale, Raw offset, bool alternate,
                                    std::uint32_t anim);

  // Per-frame model visuals (cosmetic seed; world only).  The trail and beam handles these
  // recipes store in the caller's state are this machine's (the world's stores).
  recipes::Calls model_status_effects(Raw amount, Raw scale, recipes::WeaponItem &item,
                                      recipes::WeaponModel *model, recipes::WeaponFxState &state,
                                      const Vec3 &pos, std::uint32_t yaw16, recipes::HolderModel *holder,
                                      std::uint32_t anim, bool swinging, std::int32_t joint_add,
                                      const Mat34 *override_matrix, std::uint32_t level_flags,
                                      bool dialog_open);
  recipes::Calls model_arcs(recipes::SampleModel *model, const Vec3 &pos, std::uint32_t yaw16,
                            std::uint32_t anim, Raw alpha, Raw scale, Raw offset,
                            recipes::ArcState &state, std::uint32_t rgba, bool near);
  recipes::Calls model_sparkle(recipes::SampleModel *model, const Vec3 &pos, std::uint32_t yaw16,
                               std::uint32_t anim, Raw amount, Raw offset, bool near);
  recipes::Calls model_glow(recipes::SampleModel *model, const std::uint32_t *obj_flags,
                            recipes::GlowRamp &ramp, const Vec3 &pos, std::uint32_t yaw16,
                            std::uint32_t anim, Raw scale, Raw offset, Raw fade,
                            const std::array<std::uint32_t, 4> &target_a,
                            const std::array<std::uint32_t, 4> &target_b, std::int32_t mode,
                            std::int32_t size_add, bool near);
  recipes::Calls emit_from_model_verts(recipes::SampleModel &model, const Vec3 &pos,
                                       std::uint32_t yaw16, std::uint32_t anim, std::int32_t count,
                                       std::uint32_t definition, const Vec3 *direction,
                                       const Vec3 *offset, Raw speed);

  // A call-out of any skill model (skills_a/b/c, arena_monsters, this module's Model):
  // effect primitives are applied to the world (when attached) and answered as the
  // original answers (handles; 0 headless); fx_spawn is run (id 5 draws the shared seed
  // here, as inside the original callee); LightEffect_ctor makes its light (the model has
  // already counted the shared draw of the GameObject); anything else -> nullopt.
  std::optional<skills::Value> answer(const skills::Callout &call);
  // Apply a finished call log (cosmetic only: handles cannot be returned any more).
  void replay(const std::vector<skills::Callout> &calls);

  // The round-2 effect classes (DiseaseCloud, StaticFire, FireStormFlare, MultiFire, Pustule,
  // Arena, HornOmen, PlanarSteel, the glow draw slots) over their memory image, wired to this
  // simulation: effect primitives -> answer(), everything else -> hooks.callout, gameplay
  // call-outs without an answer refuse.  The combat runtime runs each run function every
  // simulated tick on every machine (skills::must_run_headless).  Light records the classes
  // link are read back from the model's memory after each call (sync_lights).
  skills::Model &classes() { return *classes_; }
  void sync_lights();

  // Call-outs that change gameplay and must be answered by the combat runtime.
  static bool is_gameplay(std::string_view callout);

private:
  core::Lcg *shared_;
  core::Lcg headless_cosmetic_;          // stands in for the cosmetic seed when headless
  recipes::MadeFlags headless_made_;
  EffectWorld *world_ = nullptr;
  std::shared_ptr<const skills::RecipeConstants> constants_;
  std::unique_ptr<skills::Model> classes_;
  std::set<std::uint64_t> model_lights_;
  recipes::PoolNew pool(std::vector<std::uint32_t> &blocks, const char *who);
  // Apply recipe records to the world, replacing the placeholder handles a recipe was
  // given (trail_create / beam_create answers) by the handles the world's stores return.
  std::map<std::uint32_t, std::uint32_t> apply_with_handles(const recipes::Calls &calls);
  void construct(const recipes::Calls &calls, recipes::Kind kind,
                 const std::vector<std::uint32_t> &blocks);
};

// Decode a light record (0x3c-byte node, docs/lighting.md 2) from little-endian memory.
lighting::Node light_node_from_record(const std::function<std::uint64_t(std::int64_t, unsigned)> &load,
                                      std::int64_t addr);

// ---- presentation (Godot-free; the Godot renderer draws these runs) ----------------------------
// The GS frame is 1280 x 448 pixels centred on (2048, 2048) (offset 0x8000 in 1/16 pixel,
// docs/effects.md 6, 13.1; I: the centre) and is shown at 4:3.  A run is a sequence of
// primitives with one texture, one ALPHA_1 and one primitive type, in GS order.  Vertices
// are (x, y) in the frame's normalised device coordinates (+y up) and the view depth (the
// vertex's clip w, glows: depth), already resolved for ADC (only drawn primitives appear).
struct Run {
  std::string texture;            // "" untextured, PARTICLE_SHEET, TRAIL_TEXTURE, a glow name
  std::uint32_t alpha1 = 0x44;    // 0x44, 0x48 or 0x09
  bool linear = false;            // TEX1 0x61 (glows, trails) vs the particle sheet
  bool lines = false;             // else triangles
  std::vector<std::array<float, 3>> vertices;
  std::vector<std::array<std::uint8_t, 4>> colors;
  std::vector<std::array<float, 2>> uvs;    // normalised to the texture's used size
};
struct Presentation {
  std::vector<Run> runs;
  std::vector<std::string> refusals;
};
Presentation present(const std::vector<render::Packet> &packets, const render::TextureDims &glow_dims);
// Floor decals as world quads through the camera (corners centre +- (c, s, 0) and
// +- (s, -c, 0) of the stored record, z + 3; docs/effects.md 8).  The decal draw task's own GS
// output was not part of the round-2 executions, so this is presentation only: one run of
// FLOOR_TEXTURE with ALPHA_1 0x44, the decal's light tint and alpha 0x80, the rectangle of its
// type inset by half a texel.  Decals behind the camera are left out.
Presentation present_decals(const decals::DecalStore &store, const Camera &camera);

} // namespace champions::rta::fx::sim
