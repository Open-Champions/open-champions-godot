#pragma once
#include "champions/fx.hpp"
#include <array>
#include <string>
#include <vector>

namespace champions::lighting {
using fx::Vec3; using fx::Rgb; using fx::Rgba;
struct Level {
    Vec3 sun_direction{0,-0.70710678118,0.70710678118};
    Rgba sun{68,68,68,0}, ambient{32,32,32,0};
    int base = 128, layer = 0;
    double scale = 1;
    static Level from_markers(const Vec3 *ambient_pos = nullptr, const Vec3 *sun_pos = nullptr,
                              const Vec3 *direction = nullptr, unsigned flags = 0);
    std::pair<Rgba,Rgba> scaled() const;
    Vec3 fill_direction() const;
    static int base_level(unsigned flags);
};
struct Object {
    Rgba ambient{}, sun{}, fill{};
    Rgb point_scale{1,1,1}; // computed with the separate floating values below.
    std::array<double,3> point_factors{1,1,1};
    Vec3 sun_direction{}, fill_direction{};
    Rgba ambient_set{};
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
    // Returns false in mode 4. Caller supplies the cosmetic draw in mode 1.
    bool step(int sim_frame,double random,Node &out);
};
int glow_size(int base,int shift,double noise);
int fire_radius(const std::string &kind,double noise);
int candle_radius(int rand16,bool big);
int lantern_world_radius(int radius,const std::string &level_name);
struct LightEffect {
    std::array<double,4> colour{},rate{};
    double radius=0,radius_rate=0,floor=0;
    int life=0,end=0;
    bool signed_colour=false;
    Vec3 pos{};
    LightEffect(Rgba start,Rgba finish,double radius0,double radius1,double seconds,
                int now,bool signed_colour=false,double floor=0,Vec3 pos={});
    bool run(int frame,Node &out,const Vec3 *owner=nullptr);
};
} // namespace champions::lighting
