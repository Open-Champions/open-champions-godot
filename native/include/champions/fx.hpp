#pragma once

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

struct Definition {
    std::uint32_t address = 0; // Identifier from the user's runtime JSON, never bundled data.
    float size_start = 1, size_end = 1, gravity_input = 0;
    Rgba start{}, end{};
    int life_class = 3, kind = 1, tile = 1;
    int slot = -1, live = 0;
    float g = 0, size0 = 0, size_rate = 0, line_lag = 0;
    std::array<float, 4> colour0{}, colour_rate{};
    int u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    static Definition build(float size_start, float size_end, float gravity,
                            Rgba start, Rgba end, int life_class, int kind, int tile,
                            std::uint32_t address = 0);
    int life() const;
    int list_index() const { return kind * 4 + life_class; }
    Rgba colour_at(double age) const;
    float size_at(double age) const { return size0 + size_rate * static_cast<float>(age); }
    bool flipbook_step(int sim_frame);
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

class System {
public:
    static constexpr int pages = 64, cells_per_page = 240, slots = 32, groups = 16;
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
    void end_group();
    bool free_group(int handle);
    void reset();
    int clock() const { return clock_; }
    int free_page_count() const { return free_pages_; }
    int dropped() const { return dropped_; }
    int live_count() const;
private:
    struct Page { std::deque<Particle> particles; };
    struct List { std::deque<Page> pages; };
    using Lists = std::array<List, 16>;
    struct Group { Lists lists{}; int idle = 0; int handle = 0; };
    std::map<std::uint32_t, Definition> definitions_;
    std::array<std::uint32_t, slots> slot_ids_{};
    Lists global_{};
    std::array<Group, groups> groups_{};
    int current_group_ = -1, clock_ = 0, free_pages_ = pages, dropped_ = 0, group_seq_ = 1;
    Lists &current_lists();
    void retire(List &);
    void rebase(List &);
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
};
struct Emission { Vec3 pos, vel; };
std::vector<Emission> run_emitter(Emitter &, Vec3 pos, int now, Rng &, int yaw = 0,
                                  bool neg_x = false, bool neg_y = false, bool swap_xy = false,
                                  int weather_angle = 0, bool weather_on = false);

struct Glow {
    Vec3 pos{};
    Rgba rgba{};
    std::uint32_t texture = 0; // symbolic resource key; caller supplies the texture.
    double bias = 0, size = 0;
    int angle = 0, flags = 0;
};
struct TrailPoint { Vec3 pos{}; int stamp = 0; };
struct Trail {
    int handle = 0, clock = 0, lifetime = 0, row = 0, flags = 0, width_quarters = 0;
    Rgba rgba{};
    std::deque<TrailPoint> points;
};
struct Beam {
    int handle = 0, deadline = 0, depth = 0;
    Vec3 from{}, to{};
    double jitter = 0, scale = 0;
    int width = 0, alpha = 0;
};
struct Decal {
    Vec3 pos{};
    Rgb tint{};
    double size = 0, target_size = 0;
    int angle = 0, type = 0, grow = 0, cell_x = 0, cell_y = 0;
};
struct TextureRect { int x0, y0, x1, y1; };
extern const std::array<TextureRect, 11> decal_rects;

class Primitives {
public:
    bool add_glow(Glow glow);
    std::vector<Glow> take_glows(bool before_particles);
    int create_trail(double seconds, int row, int width, int flags, Rgba rgba);
    bool add_trail_point(int handle, Vec3 pos);
    bool release_trail(int handle);
    void tick_trails();
    const std::array<Trail, 64> &trails() const { return trails_; }
    int create_beam(double jitter, double scale, Vec3 from, Vec3 to, int width, int depth, int alpha, int now);
    bool update_beam(int handle, Vec3 from, Vec3 to, int alpha, int width, int now);
    void draw_beams(int sim_frame);
    const std::array<Beam, 20> &beams() const { return beams_; }
    Refusal add_decal(double size, Vec3 pos, int angle, int type, int grow, int detail, Rgb tint);
    void grow_decals(bool frozen = false);
    const std::vector<Decal> &decals() const { return decals_; }
private:
    std::vector<Glow> glows_;
    std::array<Trail, 64> trails_{};
    std::array<Beam, 20> beams_{};
    std::vector<Decal> decals_;
    std::array<int, 900> decal_cells_{};
    int trail_seq_ = 1, beam_seq_ = 1, growing_ = 0;
};

struct SpawnResult { Refusal refusal = Refusal::None; int spawned = 0; };
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
SpawnResult spawn_fx(System &, int id, Vec3 pos, Vec3 camera_focus, int gore, Rng &);

} // namespace champions::rta::fx
