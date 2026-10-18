#pragma once
#include "champions/fx.hpp"
#include "champions/lighting.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <map>
#include <vector>

// Presentation bridge. The simulation and contract data remain native/Godot-free.
class ChampionsFxBridge : public godot::RefCounted {
    GDCLASS(ChampionsFxBridge, godot::RefCounted)
    champions::rta::fx::System particles_;
    champions::rta::fx::Primitives primitives_;
    champions::rta::fx::Rng rng_;
    champions::rta::lighting::Level level_;
    std::vector<champions::rta::lighting::Node> nodes_;
    std::map<int,std::pair<std::uint32_t,champions::rta::fx::Emitter>> emitters_;
    int next_emitter_ = 1;
protected:
    static void _bind_methods();
public:
    godot::Dictionary load_definitions_json(const godot::String &json);
    godot::Dictionary spawn_particle(int64_t definition,const godot::Vector3 &position,
                                     const godot::Vector3 &velocity,double backdate);
    int64_t add_emitter(int64_t definition,const godot::PackedByteArray &record);
    godot::Dictionary run_emitter(int64_t handle,const godot::Vector3 &position,int64_t frame,
                                  int64_t yaw,bool neg_x,bool neg_y,bool swap_xy,
                                  int64_t weather_angle,bool weather_on);
    godot::Dictionary spawn_event(int64_t source,int64_t tick,int64_t spawn_id,
                                  const godot::Vector3 &position,const godot::Vector3 &camera_focus,
                                  int64_t gore);
    void tick(bool cutscene);
    void draw_step(int64_t sim_frame,bool frozen);
    godot::Array particle_snapshot() const;
    bool add_glow(const godot::Vector3 &position,const godot::Color &colour,int64_t texture_id,
                  double bias,double size,int64_t angle,int64_t flags);
    godot::Array take_glows(bool before_particles);
    int64_t create_trail(double seconds,int64_t row,int64_t width,int64_t flags,
                         const godot::Color &colour);
    bool add_trail_point(int64_t handle,const godot::Vector3 &position);
    bool release_trail(int64_t handle);
    godot::Array trail_snapshot() const;
    int64_t create_beam(double jitter,double scale,const godot::Vector3 &from,
                        const godot::Vector3 &to,int64_t width,int64_t depth,int64_t alpha,int64_t now);
    bool update_beam(int64_t handle,const godot::Vector3 &from,const godot::Vector3 &to,
                     int64_t alpha,int64_t width,int64_t now);
    godot::Array beam_snapshot() const;
    godot::Dictionary set_level_light(const godot::Vector3 &sun_direction,
                                      const godot::Color &sun,const godot::Color &ambient,
                                      int64_t base,double scale,int64_t layer);
    void clear_light_nodes();
    void add_light_node(const godot::Vector3 &position,const godot::Vector3 &model_rgb,
                        const godot::Vector3 &world_rgb,double model_radius,double world_radius,
                        double floor,int64_t layer,bool tint_decals);
    godot::Dictionary model_light(const godot::Vector3 &position) const;
    godot::Vector3 shade_model(const godot::Vector3 &position,const godot::Vector3 &normal) const;
    godot::Vector3 shade_world(const godot::Vector3 &position,const godot::Vector3 &normal) const;
    godot::Vector3 decal_tint(const godot::Vector3 &position) const;
    godot::Dictionary add_decal(double size,const godot::Vector3 &position,int64_t angle,
                                int64_t type,int64_t grow,int64_t detail);
    godot::Array decal_snapshot() const;
};
