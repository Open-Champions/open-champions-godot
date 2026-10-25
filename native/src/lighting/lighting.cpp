#include "champions/lighting.hpp"
#include "champions/fx_rz.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace champions::rta::lighting {
namespace {
using fx::rounding::f32;
using fx::rounding::rz;
int trunc(double x) { return static_cast<int>(x); }
double sq(double x) { return std::pow(x, 2.0); }  // Python x ** 2
// Python's built-in sum() of three floats (start 0): CPython 3.12 and later add floats
// with Neumaier's compensated summation. The reference's dot(), norm() and squared
// distances are such sums; the vectors were made with CPython 3.12.
double py_sum3(double x0,double x1,double x2) {
    double f=0.0+x0, c=0.0;
    for (double x:{x1,x2}) {
        const double t=f+x;
        if (std::fabs(f)>=std::fabs(x)) c+=(f-t)+x;
        else c+=(x-t)+f;
        f=t;
    }
    if (c!=0.0 && std::isfinite(c)) f+=c;
    return f;
}
double dot(Vec3 a,Vec3 b) { return py_sum3(a.x*b.x,a.y*b.y,a.z*b.z); }
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 norm(Vec3 v) {
    const double n=std::sqrt(py_sum3(v.x*v.x,v.y*v.y,v.z*v.z));
    return n>0.0?Vec3{v.x/n,v.y/n,v.z/n}:Vec3{0.0,0.0,0.0};
}
// The reference's clamp: lo if v < lo else hi if v > hi else v.
double clamp(double v,double lo,double hi) { return v<lo?lo:v>hi?hi:v; }
int clampi(int v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
double comp(Vec3 v,int i) { return i==0?v.x:i==1?v.y:v.z; }
}
double fill_factor() { return f32(0.79999995); }

Vec3 Level::default_sun_direction() { return norm({0.0,-1.0,1.0}); }
Level Level::make(Vec3 dir,Rgba sun,Rgba ambient,int base,double scale,int layer) {
    Level l;
    l.sun_direction=norm(dir); l.sun=sun; l.ambient=ambient;
    l.base=base; l.scale=scale; l.layer=layer;
    return l;
}
Level Level::boot() {
    return make({-0.57735,-0.57735,0.57735},{0x40,0x46,0x50,0},{0x20,0x20,0x20,0});
}
int Level::base_level(unsigned flags) {
    if (flags&0x04000000u) return 0x40;
    if (flags&0x20000000u) return 0x20;
    return 0x80;
}
Level Level::from_markers(const Vec3 *amb,const Vec3 *sun_pos,const Vec3 *direction,unsigned flags) {
    const Rgba a=amb?Rgba{trunc(amb->x*0.5),trunc(amb->y*0.5),trunc(amb->z*0.5),0}:Rgba{0x20,0x20,0x20,0};
    const Rgba s=sun_pos?Rgba{trunc(sun_pos->x*0.5),trunc(sun_pos->y*0.5),trunc(sun_pos->z*0.5),0}
                        :Rgba{0x44,0x44,0x44,0};
    const Vec3 d=direction?*direction:Vec3{0.0,-1.0,1.0};
    return make(d,s,a,base_level(flags));
}
std::pair<Rgba,Rgba> Level::scaled() const {
    Rgba s{},a{};
    const double k=layer!=0?1.0:scale;
    for (int i=0;i<4;++i) {
        s[i]=clampi(trunc(k*sun[i]),0,255);
        a[i]=clampi(trunc(k*ambient[i]),0,255);
    }
    return {s,a};
}
Vec3 Level::fill_direction() const {
    return norm({-sun_direction.x,-sun_direction.y,0.5*sun_direction.z});
}
Object Object::reset(const Level &l) {
    Object o;
    std::tie(o.sun,o.ambient)=l.scaled();
    o.ambient_set=l.ambient;
    const double k=fill_factor();
    for (int i=0;i<3;++i) o.fill[i]=trunc(rz(static_cast<double>(l.ambient[i])*k));
    o.fill[3]=0;
    o.sun_direction=l.sun_direction; o.fill_direction=l.fill_direction();
    return o;
}
void Object::add_ambient(const Rgba &bias) {
    for (int i=0;i<4;++i) ambient[i]=std::max(ambient_set[i]+bias[i],0);
}
void Object::scale(double r,double g,double b) {
    const double k[3]={r,g,b};
    for (Rgba *arr : {&ambient,&sun,&fill})
        for (int i=0;i<3;++i) (*arr)[i]=trunc((*arr)[i]*k[i]);
    point_factors={r,g,b};
}
void Object::chill(double f) {
    f=std::min(f,1.0);
    if (f>0) { const int n=trunc(f*100); add_ambient({-n,-n,n,n>>1}); }
}
void Object::poison(double f) {
    f=std::min(f,1.0);
    if (f>0) { const int n=trunc(f*100); add_ambient({-n,n>>1,-n,n>>1}); }
}
Node::Node(Vec3 p,Rgb m,Rgb w,double mr,double wr,double fl,int ly,bool tint):
    pos(p),model_rgb(m),world_rgb(w),model_radius(mr),
    world_radius(wr<0?mr:wr),floor(fl),layer(ly),tint_decals(tint) {
    model_falloff=model_radius!=0.0?1.0/sq(model_radius):INFINITY;
    world_falloff=world_radius!=0.0?1.0/sq(world_radius):INFINITY;
}
Node Node::fire(Vec3 p,double r,double wr) {
    return Node(p,{200,150,50},{51,39,18},r,wr<0?r:wr,1,0,true);
}
Cell grid_cell(Vec3 p) {
    const int x=(trunc(p.x)+11520)>>8,y=(trunc(p.y)+11520)>>8;
    return x>=0 && x<90 && y>=0 && y<90?Cell{x,y}:Cell{};
}
std::vector<const Node *> collect_near(const std::vector<Node> &nodes,Vec3 p,int layer) {
    std::vector<const Node *> out;
    const int y0=std::max((trunc(p.y)+11520-300)>>8,0);
    const int y1=std::min((trunc(p.y)+11520+300)>>8,89);
    const int x0=std::max((trunc(p.x)+11520-300)>>8,0);
    const int x1=std::min((trunc(p.x)+11520+300)>>8,89);
    for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x)
        for (auto it=nodes.rbegin();it!=nodes.rend();++it) {
            const Cell c=grid_cell(it->pos);
            if (c.x!=x || c.y!=y) continue;
            const double r=it->model_radius;
            bool inside=true;
            for (int i=0;i<3;++i)
                inside=inside && comp(it->pos,i)-r<=comp(p,i) && comp(p,i)<=comp(it->pos,i)+r;
            if (inside && it->layer==layer && out.size()<31) out.push_back(&*it);
        }
    return out;
}
ModelLighting model_lights(Vec3 p,const std::vector<Node> &nodes,const Object &obj,int layer,
                           const std::vector<const Node *> *preselected) {
    std::vector<const Node *> chosen;
    if (preselected) {
        chosen=*preselected;
    } else {
        auto cand=collect_near(nodes,p,layer);
        std::vector<std::tuple<long long,int,const Node *>> ranked;
        for (int i=0;i<int(cand.size());++i) {
            const Node &n=*cand[i];
            const double d2=py_sum3(sq(p.x-n.pos.x),sq(p.y-n.pos.y),sq(p.z-n.pos.z));
            const long long key=static_cast<long long>(static_cast<std::uint64_t>(
                static_cast<long long>(trunc(d2-sq(n.model_radius))))<<7)|i;
            ranked.emplace_back(key,i,cand[i]);
        }
        std::sort(ranked.begin(),ranked.end());
        for (const auto &t:ranked) chosen.push_back(std::get<2>(t));
    }
    ModelLighting result; result.ambient={obj.ambient[0],obj.ambient[1],obj.ambient[2]};
    for (int i=0;i<int(chosen.size()) && i<2;++i) {
        const Node &n=*chosen[i];
        const Vec3 d=sub(n.pos,p); const double d2=dot(d,d);
        const double w=clamp((sq(n.model_radius)-d2)*n.model_falloff,0.0,1.0);
        result.lights[i].direction=norm(d);
        for (int ch=0;ch<3;++ch)
            result.lights[i].rgb[ch]=trunc(n.model_rgb[ch]*w*obj.point_factors[ch]);
    }
    const double shade=1.0;  // 0x18b18c returns 1.0
    result.lights[2]={obj.fill_direction,{obj.fill[0],obj.fill[1],obj.fill[2]}};
    result.lights[3]={obj.sun_direction,{trunc(obj.sun[0]*shade),trunc(obj.sun[1]*shade),
                                         trunc(obj.sun[2]*shade)}};
    return result;
}
Rgb shade_model(Vec3 normal,const ModelLighting &l) {
    normal=norm(normal); Rgb out{};
    for (int ch=0;ch<3;++ch) {
        double v=l.ambient[ch];
        for (const auto &light:l.lights) v+=std::max(dot(normal,light.direction)*0.99,0.0)*light.rgb[ch];
        out[ch]=std::min(trunc(v),255);
    }
    return out;
}
Rgb shade_world(int base,const std::vector<Node> &nodes,Vec3 pos,Vec3 normal) {
    normal=norm(normal);
    bool lit=false;
    for (const auto &nd:nodes) {
        const double d2=py_sum3(sq(nd.pos.x-pos.x),sq(nd.pos.y-pos.y),sq(nd.pos.z-pos.z));
        if (d2<sq(nd.world_radius) && nd.floor<1) lit=true;
    }
    std::array<double,3> acc{double(base),double(base),double(base)};
    for (std::size_t i=0;i<nodes.size() && i<4;++i) {
        const Node &n=nodes[i]; const Vec3 d=sub(n.pos,pos); const double d2=dot(d,d);
        double a=std::max((sq(n.world_radius)-d2)*n.world_falloff,0.0);
        if (lit) {
            double dl=std::sqrt(d2);
            if (dl==0.0) dl=1.0;
            a*=clamp(dot(d,normal)/dl*0.99,n.floor,1.0);
        }
        a=std::min(a,1.0);
        for (int ch=0;ch<3;++ch) acc[ch]+=a*n.world_rgb[ch];
    }
    Rgb out{}; for (int ch=0;ch<3;++ch) out[ch]=std::min(trunc(acc[ch]),255);
    return out;
}
Rgb decal_tint(Vec3 pos,const std::vector<Node> &nodes,const Level &level) {
    std::array<double,3> c{};
    for (const Node *n:collect_near(nodes,pos,level.layer)) {
        if (!n->tint_decals) continue;
        const Vec3 d=sub(n->pos,pos); const double d2=dot(d,d);
        if (d2<=0) continue;
        double w=(sq(n->model_radius)-d2)*n->model_falloff*d.z/std::sqrt(d2);
        w=clamp(w,0.0,1.0);
        for (int ch=0;ch<3;++ch) c[ch]+=n->model_rgb[ch]*w;
    }
    const double s=std::max(level.sun_direction.z*1.0,0.0);
    Rgb out{};
    for (int ch=0;ch<3;++ch)
        out[ch]=clampi(trunc(c[ch]+level.sun[ch]*s+level.ambient[ch]),0,255);
    return out;
}
std::optional<Node> Chunk::step(int frame,const std::function<double()> &rnd) {
    int r=radius;
    const int m=mode;
    if (m==0) {
        r=low;
    } else if (m==1) {
        if (frame>timer) { timer=frame+period; r=low+trunc((high-low)*rnd()); }
    } else if (m==2) {
        if (frame>timer) {
            timer=frame+period;
            if (down) { r=low; down=false; }
            else { r=high; down=true; }
        }
        if (period) {
            const int step=trunc(static_cast<double>(high-low)/period);
            r+=down?-step:step;
        }
    } else if (m==3) {
        if (frame>timer) { timer=frame+period; r=r==low?high:low; }
    }
    if (m==4) return std::nullopt;
    r=clampi(r,0,299);
    radius=r;
    const Rgb rgb{rgba[0],rgba[1],rgba[2]};
    return Node({0,0,0},rgb,rgb,r,r,0.0,0,false);
}
bool Chunk::step(int frame,double random,Node &out) {
    const auto node=step(frame,[random] { return random; });
    if (!node) return false;
    out=*node;
    return true;
}
int LightSlots::claim(const Chunk &chunk,std::int64_t draw_frame) {
    int s;
    const auto it=slot_of.find(&chunk);
    if (it==slot_of.end()) {
        s=0;
        std::int64_t oldest=draw_frame;
        for (int i=0;i<16;++i)
            if (stamp[i]<oldest) { s=i; oldest=stamp[i]; }
        if (const Chunk *prev=owner[s]) slot_of.erase(prev);
        owner[s]=&chunk;
        slot_of[&chunk]=s;
    } else {
        s=it->second;
    }
    stamp[s]=draw_frame;
    return s;
}
std::pair<int,std::optional<Node>> LightSlots::run(Chunk &chunk,int sim_frame,std::int64_t draw_frame,
                                                   const std::function<double()> &rnd) {
    const int s=claim(chunk,draw_frame);
    chunk.radius=radius[s];
    auto node=chunk.step(sim_frame,rnd);
    if (node) radius[s]=chunk.radius;
    return {s,node};
}
int glow_size(int base,int shift,double noise) { return base+(trunc(noise*22.0)<<(shift&31)); }
int fire_radius(const std::string &kind,double noise) {
    if (kind=="torch" || kind=="lantern") return 160+trunc(noise*22.0);
    if (kind=="lamp") return 216+trunc(noise*22.0);
    return -1;
}
int candle_radius(int r,bool big) { return (big?84:48)+((r-8)&15); }
int lantern_world_radius(int r,const std::string &name) {
    return name.find("mine")==std::string::npos?r:0;
}
void LightEffect::init(const std::array<double,4> &start,const std::array<double,4> &finish,
                       double r0,double r1,double seconds,int now) {
    life=std::max(1,trunc(rz(f32(seconds)*60.0)));
    end=now+life;
    const double inv=rz(1.0/life);
    for (int i=0;i<4;++i) {
        colour[i]=f32(start[i]);
        rate[i]=rz(rz(f32(finish[i])-f32(start[i]))*inv);
    }
    radius=f32(r0);
    radius_rate=rz(rz(f32(r1)-f32(r0))*inv);
}
LightEffect::LightEffect(Rgba start,Rgba finish,double r0,double r1,double seconds,
                         int now,bool signed_col,double floor_,Vec3 p):
    floor(floor_),signed_colour(signed_col),pos(p) {
    init({double(start[0]),double(start[1]),double(start[2]),double(start[3])},
         {double(finish[0]),double(finish[1]),double(finish[2]),double(finish[3])},r0,r1,seconds,now);
}
LightEffect LightEffect::from_floats(const std::array<double,4> &start,const std::array<double,4> &finish,
                                     double r0,double r1,double seconds,int now,bool signed_col,
                                     double floor_,Vec3 p) {
    LightEffect le(Rgba{},Rgba{},0.0,0.0,0.0,now,signed_col,floor_,p);
    le.init(start,finish,r0,r1,seconds,now);
    return le;
}
bool LightEffect::run(int frame,Node &out,const Vec3 *owner) {
    for (int i=0;i<4;++i) colour[i]=rz(colour[i]+rate[i]);
    radius=rz(radius+radius_rate);
    const double lo=signed_colour?-255.0:0.0;
    for (int i=0;i<3;++i) colour[i]=clamp(colour[i],lo,255.0);
    if (owner) pos={f32(owner->x),f32(owner->y),rz(f32(owner->z)+36.0)};
    const Rgb rgb{trunc(colour[0]),trunc(colour[1]),trunc(colour[2])};
    out=Node(pos,rgb,rgb,radius,radius,floor);
    return frame<end;
}
} // namespace champions::rta::lighting
