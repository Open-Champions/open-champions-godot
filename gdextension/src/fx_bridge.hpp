#pragma once
#include "champions/fx_exact.hpp"
#include "champions/fx_simulation.hpp"
#include "champions/lighting.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <map>
#include <memory>
#include <set>

// Presentation bridge over the per-machine effect world (fx_simulation.hpp). Positions are
// game units (x, y, z with z up) as float32. The simulation half that touches gameplay
// (shared seed, damage, objects) belongs to the combat runtime; this bridge refuses the
// recipes that need it (fx_spawn id 5).
class ChampionsFxBridge : public godot::RefCounted {
    GDCLASS(ChampionsFxBridge, godot::RefCounted)
    std::unique_ptr<champions::rta::fx::sim::EffectWorld> world_;
    std::map<int, std::pair<std::uint32_t, champions::rta::fx::exact::EmitterRecord>> emitters_;
    std::set<std::uint64_t> bridge_lights_;
    int next_emitter_ = 1;
    std::int32_t sim_frame_ = 0;
protected:
    static void _bind_methods();
public:
    ChampionsFxBridge();
    godot::Dictionary load_definitions_json(const godot::String &json);
    godot::Dictionary load_effect_textures(const godot::PackedStringArray &archive_paths);
    void set_detail(int64_t detail);
    void set_wait_first(bool wait_first);
    godot::Dictionary spawn_particle(int64_t definition, const godot::Vector3 &position,
                                     const godot::Vector3 &velocity, double backdate);
    godot::Dictionary fx_spawn(int64_t id, const godot::Vector3 &position, bool near);
    int64_t add_emitter(int64_t definition, const godot::PackedByteArray &record);
    godot::Dictionary run_emitter(int64_t handle, const godot::Vector3 &position, int64_t frame,
                                  int64_t yaw, bool neg_x, bool neg_y, bool swap_xy,
                                  int64_t weather_angle, bool weather_on);
    bool add_glow(const godot::Vector3 &position, const godot::Color &colour, int64_t texture_word,
                  double bias, double size, int64_t angle, int64_t flags);
    int64_t create_trail(double seconds, int64_t row, int64_t width, int64_t flags,
                         const godot::Color &colour);
    bool add_trail_point(int64_t handle, const godot::Vector3 &position);
    void release_trail(int64_t handle);
    int64_t create_beam(double jitter, double scale, const godot::Vector3 &from,
                        const godot::Vector3 &to, int64_t width, int64_t depth, const godot::Color &colour);
    void update_beam(int64_t handle, const godot::Vector3 &from, const godot::Vector3 &to,
                     int64_t alpha, int64_t width);
    void add_decal(double size, const godot::Vector3 &position, int64_t angle, int64_t type, int64_t grow);
    void tick(int64_t sim_frame, bool cutscene);
    godot::Dictionary draw(const godot::Dictionary &camera, bool frozen);
    godot::Dictionary counts() const;
    godot::Dictionary set_level_light(const godot::Vector3 &sun_direction, const godot::Color &sun,
                                      const godot::Color &ambient, int64_t base, double scale, int64_t layer);
    void clear_light_nodes();
    void add_light_node(const godot::Vector3 &position, const godot::Vector3 &model_rgb,
                        const godot::Vector3 &world_rgb, double model_radius, double world_radius,
                        double floor, int64_t layer, bool tint_decals);
    godot::Dictionary model_light(const godot::Vector3 &position) const;
    godot::Vector3 shade_model(const godot::Vector3 &position, const godot::Vector3 &normal) const;
    godot::Vector3 shade_world(const godot::Vector3 &position, const godot::Vector3 &normal) const;
    godot::Vector3 decal_tint(const godot::Vector3 &position) const;
};
