#pragma once

#include "champions/fx_exact.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

// Return to Arms v2.00 cosmetic contract. All work here is independent of Godot.
namespace champions::rta::fx {

struct Vec3 { double x = 0, y = 0, z = 0; };
using Rgba = std::array<int, 4>;
using Rgb = std::array<int, 3>;

enum class Blend : std::uint8_t { Alpha = 0, Add = 1, LineAdd = 2, Hidden = 3 };
enum class Refusal {
    None, MissingDefinition, InvalidDefinition, UnsupportedRecipe, UnknownDirection,
    MissingTextureMetadata, PoolExhausted, InvalidType
};

// A runtime definition keyed by its address. Since native-fx-2 the arithmetic is the exact
// port of ref_particles.py round 2 (`exact`, champions/fx_exact.hpp); the float/int fields
// below mirror it (single-precision values) and are refreshed by System after each call.
struct Definition {
    std::uint32_t address = 0; // Identifier from the user's runtime JSON, never bundled data.
    float size_start = 1, size_end = 1, gravity_input = 0;
    Rgba start{}, end{};
    int life_class = 3, kind = 1, tile = 1;
    int slot = -1, live = 0;
    float g = 0, size0 = 0, size_rate = 0, line_lag = 0;
    std::array<float, 4> colour0{}, colour_rate{};
    int u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    exact::ParticleDef exact{};
    static Definition build(float size_start, float size_end, float gravity,
                            Rgba start, Rgba end, int life_class, int kind, int tile,
                            std::uint32_t address = 0);
    int life() const;
    int list_index() const { return kind * 4 + life_class; }
    Rgba colour_at(double age) const;
    float size_at(double age) const { return static_cast<float>(exact.size_at(age)); }
    bool flipbook_step(int sim_frame);
    void mirror(); // copy slot, live, u0..v1 from `exact`
};

// Parses the JSON array produced by fx_particle_defs.py --json. Only complete,
// callback-free records enter the registry. No retail values are compiled in.
bool load_definition_json(const std::string &json, std::map<std::uint32_t, Definition> &out,
                          std::string &error);

struct Particle {
    std::int16_t x = 0, y = 0, z = 0, birth = 0, vx = 0, vy = 0, vz = 0, type4 = 0;
    std::uint32_t definition = 0;
};
struct ParticleView {
    Vec3 pos{}, tail{};
    float size = 0;
    Rgba rgba{};
    int age = 0, kind = 0, tile = 0, u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    std::uint32_t definition = 0;
};
Particle make_record(const Definition &, Vec3 pos, Vec3 vel, int clock,
                     double backdate, int slot);
Vec3 position_at(const Particle &, const Definition &, double age);
ParticleView evaluate(const Particle &, const Definition &, int clock);

// The particle pools, delegated to exact::ParticleSystem (31 usable slots, group lists per
// group slot, FIFO retirement reading +0xe, the clock wrap). Definitions live in this
// object, so a System is movable but not copyable.
class System {
public:
    static constexpr int pages = 64, cells_per_page = 240, slots = 32, groups = 16;
    System() = default;
    System(const System &) = delete;
    System &operator=(const System &) = delete;
    System(System &&) = default;
    System &operator=(System &&) = default;
    Definition *find(std::uint32_t address);
    const Definition *find(std::uint32_t address) const;
    bool load_json(const std::string &json, std::string &error);
    bool put(Definition definition);
    Refusal spawn(std::uint32_t definition, Vec3 pos, Vec3 vel = {}, double backdate = 0);
    void tick(bool cutscene = false);
    void flipbook_draw(int sim_frame);
    std::vector<ParticleView> visible() const;
    int create_group();
    bool begin_group(int handle);
    // particleGroup_end of the handle last begun; a stale handle is ignored, so a group
    // freed while current stays the current list set (round 2).
    void end_group();
    void end_group(int handle);
    bool free_group(int handle);
    bool set_group_matrix(int handle, const exact::Mat34d &matrix);
    void reset();
    int clock() const { return pool_.clock(); }
    int free_page_count() const { return static_cast<int>(pool_.free_pages().size()); }
    int dropped() const { return pool_.dropped(); }
    int live_count() const;
    // Renderer read access: lists, pages, cells, slot records, group slots.
    const exact::ParticleSystem &pool() const { return pool_; }
    // The definition resident in a slot (nullptr for slot 0 and free slots).
    const Definition *slot_definition(int slot) const;
private:
    std::map<std::uint32_t, Definition> definitions_;
    exact::ParticleSystem pool_;
    std::int64_t current_handle_ = 0;
    void mirror_all();
};

struct Rng {
    std::uint32_t seed = 0;
    std::uint32_t step() { seed = seed * 0x19660du + 0x3c6ef35fu; return seed; }
    double draw() { return (step() >> 16) / 65536.0; }
};

struct Emitter {
    std::array<std::uint8_t, 0x48> bytes{};
    Rgba start{}, end{};
    int size_start = 0, size_end = 0, gravity = 0, life_class = 0, kind = 0, tile = 0;
    int rate = 0, off_x = 0, off_y = 0, off_z = 0, spread = 0, cone = 0;
    Vec3 velocity{};
    unsigned yaw = 0, pitch = 0;
    std::array<unsigned, 3> box{};
    int on_time = 0, off_time = 0, next = 0, on = 0, flags = 0;
    static bool parse(const std::uint8_t *data, std::size_t size, Emitter &out);
    bool same_definition(const Emitter &other) const;
    Definition definition(std::uint32_t address) const;
    exact::EmitterRecord record() const; // the fields as the exact port's record
};
struct Emission { Vec3 pos, vel; };
// Delegates to exact::emitter_run (the on/off timer in truncating single precision).
std::vector<Emission> run_emitter(Emitter &, Vec3 pos, int now, Rng &, int yaw = 0,
                                  bool neg_x = false, bool neg_y = false, bool swap_xy = false,
                                  int weather_angle = 0, bool weather_on = false);

struct TextureRect { int x0, y0, x1, y1; };
extern const std::array<TextureRect, 11> decal_rects;

struct BodyMaterial {
    std::uint32_t flag = 0;
    int hit_spawn_id = -1, gib_bits = -1, corpse_pool = -1;
    bool metal_sparks = false;
};
// Pass one material flag. Multiple material flags have no documented priority.
Refusal body_material(std::uint32_t flag, BodyMaterial &out);
struct GutChunkPolicy {
    bool invisible_drop = false, decals = true, particle_trail = true;
    std::uint32_t particle_definition = 0;
    int splat_first = -1, splat_second = -1, pool = -1;
};
Refusal gut_chunk_policy(unsigned bits,int gore_detail,GutChunkPolicy &out);
// The hook is shared by later combat, creature-animation, and skill adapters.
struct Event {
    enum class Source { CombatHit, CombatDeath, CreatureAnimation, SkillCast, SkillTick, SkillDraw, SkillImpact };
    Source source = Source::CombatHit;
    int tick = 0, effect_class = 0, spawn_id = -1;
    Vec3 pos{};
};
// fx_spawn, trails, beams, glows and decals: the exact parts (fx_recipes.hpp, fx_render.hpp,
// fx_decals.hpp) wired by fx_simulation.hpp replace the round-1 approximations.

} // namespace champions::rta::fx
