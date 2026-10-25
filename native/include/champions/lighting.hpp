#pragma once
#include "champions/fx.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Port of the Return to Arms lighting reference tools/ref_lighting.py, research commit
// 19f80a6 (round 2, executed against the original there): the binary64 work of the
// reference in the same order, its single-precision steps (fill factor, LightEffect) cut
// with the same rz()/f32() (champions/fx_rz.hpp), and `x ** 2` taken with std::pow as
// Python does.
namespace champions::rta::lighting {
using fx::Vec3; using fx::Rgb; using fx::Rgba;

double fill_factor();  // f32(0.79999995): light_resetObjectLight 0x148340, 0x3f4ccccc

struct Level {
    Vec3 sun_direction = default_sun_direction();
    Rgba sun{68,68,68,0}, ambient{32,32,32,0};
    int base = 128, layer = 0;
    double scale = 1;
    // LevelLight(sun_dir, sun_rgba, ambient_rgba, level, scale, layer): the direction is
    // normalised.
    static Level make(Vec3 sun_direction, Rgba sun, Rgba ambient, int base = 0x80,
                      double scale = 1.0, int layer = 0);
    static Level boot();  // light_init 0x1489c0 values (LevelLight.BOOT)
    static Vec3 default_sun_direction();  // normalise(0, -1, 1)
    static Level from_markers(const Vec3 *ambient_pos = nullptr, const Vec3 *sun_pos = nullptr,
                              const Vec3 *direction = nullptr, unsigned flags = 0);
    std::pair<Rgba,Rgba> scaled() const;
    Vec3 fill_direction() const;
    static int base_level(unsigned flags);
};
struct Object {
    Rgba ambient{}, sun{}, fill{};
    std::array<double,3> point_factors{1,1,1};
    Vec3 sun_direction{}, fill_direction{};
    Rgba ambient_set{};
    // light_resetObjectLight: fill = trunc(rz(as-set x 0.79999995)) (round 2), alpha 0 as
    // the reference keeps it.
    static Object reset(const Level &);
    void add_ambient(const Rgba &bias);
    void set_ambient(Rgba rgba) { ambient = rgba; }
    void scale(double r,double g,double b);
    void hit_flash() { ambient = {255,255,255,0}; }
    void chill(double f);
    void poison(double f);
};
struct Node {
    Vec3 pos{};
    Rgb model_rgb{}, world_rgb{};
    double model_radius = 0, world_radius = 0;
    double model_falloff = 0, world_falloff = 0, floor = 0;
    int layer = 0;
    bool tint_decals = false;
    Node(Vec3 pos, Rgb model, Rgb world, double model_radius,
         double world_radius = -1, double floor = 0, int layer = 0, bool tint = false);
    static Node fire(Vec3 pos,double radius,double world_radius = -1);
};
struct Cell { int x = -1, y = -1; bool valid() const { return x>=0 && y>=0; } };
Cell grid_cell(Vec3 pos);
std::vector<const Node *> collect_near(const std::vector<Node> &,Vec3 pos,int layer = 0);
struct ModelLight { Vec3 direction{}; Rgb rgb{}; };
struct ModelLighting { std::array<ModelLight,4> lights{}; Rgb ambient{}; };
ModelLighting model_lights(Vec3 pos,const std::vector<Node> &,const Object &,int layer,
                           const std::vector<const Node *> *preselected = nullptr);
Rgb shade_model(Vec3 normal,const ModelLighting &);
Rgb shade_world(int base,const std::vector<Node> &,Vec3 pos,Vec3 normal);
Rgb decal_tint(Vec3 pos,const std::vector<Node> &,const Level &);

struct Chunk {
    int low=0, high=0, mode=0, period=0, timer=0, radius=0;
    bool down=false;
    Rgba rgba{};
    // world_runLightChunk 0x18ba18, one call (a drawn frame). `rnd` is the cosmetic draw
    // of mode 1, called only when that mode's timer fires. Nothing in mode 4 (off); a
    // mode above 4 keeps the radius (as the reference does).
    std::optional<Node> step(int sim_frame,const std::function<double()> &rnd);
    // Round-1 form: the draw is supplied in advance. Returns false in mode 4.
    bool step(int sim_frame,double random,Node &out);
};
// The 16 shared light slots (0x7d3f68) and world_claimLightSlot 0x18b93c. A chunk keeps
// its slot while drawn; otherwise it takes the slot with the oldest draw stamp below the
// current draw frame (slot 0 when none is older) and the previous owner loses it. The
// radius a mode keeps "unchanged" is the slot's (round 2). Chunks are identified by
// address and must stay where they are while they hold a slot.
struct LightSlots {
    std::array<const Chunk *,16> owner{};
    std::array<std::int64_t,16> stamp{};
    std::array<int,16> radius{};
    std::map<const Chunk *,int> slot_of;
    int claim(const Chunk &chunk,std::int64_t draw_frame);
    std::pair<int,std::optional<Node>> run(Chunk &chunk,int sim_frame,std::int64_t draw_frame,
                                           const std::function<double()> &rnd = {});
};
int glow_size(int base,int shift,double noise);
int fire_radius(const std::string &kind,double noise);
int candle_radius(int rand16,bool big);
int lantern_world_radius(int radius,const std::string &level_name);
// LightEffect_ctor 0x201bd8 / LightEffect_run 0x1ee050 in truncating single precision
// (round 2): the colour accumulators keep their fractions; the node gets trunc() of them.
// Note: constructing a LightEffect draws the shared seed in the original (a GameObject);
// that draw belongs to the simulation and is not made here.
struct LightEffect {
    std::array<double,4> colour{},rate{};
    double radius=0,radius_rate=0,floor=0;
    int life=0,end=0;
    bool signed_colour=false;
    Vec3 pos{};
    LightEffect(Rgba start,Rgba finish,double radius0,double radius1,double seconds,
                int now,bool signed_colour=false,double floor=0,Vec3 pos={});
    // The reference's constructor with float colours (the original takes floats).
    static LightEffect from_floats(const std::array<double,4> &start,const std::array<double,4> &finish,
                                   double radius0,double radius1,double seconds,int now,
                                   bool signed_colour=false,double floor=0,Vec3 pos={});
    bool run(int frame,Node &out,const Vec3 *owner=nullptr);
private:
    void init(const std::array<double,4> &start,const std::array<double,4> &finish,
              double radius0,double radius1,double seconds,int now);
};
} // namespace champions::rta::lighting
