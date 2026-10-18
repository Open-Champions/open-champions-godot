#include "champions/lighting.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace champions::rta::lighting {
namespace {
int trunc(double x) { return static_cast<int>(x); }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 norm(Vec3 v) {
    const double n=std::sqrt(dot(v,v));
    return n>0?Vec3{v.x/n,v.y/n,v.z/n}:Vec3{};
}
double clamp(double x,double a,double b) { return std::max(a,std::min(x,b)); }
int clampi(int x,int a,int b) { return std::max(a,std::min(x,b)); }
}
int Level::base_level(unsigned flags) {
    if (flags&0x04000000u) return 0x40;
    if (flags&0x20000000u) return 0x20;
    return 0x80;
}
Level Level::from_markers(const Vec3 *amb,const Vec3 *sun_pos,const Vec3 *direction,unsigned flags) {
    Level l; l.base=base_level(flags);
    if (amb) l.ambient={trunc(amb->x*0.5),trunc(amb->y*0.5),trunc(amb->z*0.5),0};
    if (sun_pos) l.sun={trunc(sun_pos->x*0.5),trunc(sun_pos->y*0.5),trunc(sun_pos->z*0.5),0};
    if (direction) l.sun_direction=norm(*direction);
    return l;
}
std::pair<Rgba,Rgba> Level::scaled() const {
    Rgba s{},a{};
    const double k=layer?1.0:scale;
    for (int i=0;i<4;++i) {
        s[i]=clampi(trunc(sun[i]*k),0,255);
        a[i]=clampi(trunc(ambient[i]*k),0,255);
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
    for (int i=0;i<3;++i) o.fill[i]=trunc(0.8*l.ambient[i]);
    o.sun_direction=l.sun_direction; o.fill_direction=l.fill_direction();
    return o;
}
void Object::add_ambient(const Rgba &bias) {
    for (int i=0;i<4;++i) ambient[i]=std::max(ambient_set[i]+bias[i],0);
}
void Object::scale(double r,double g,double b) {
    point_factors={r,g,b};
    for (Rgba *arr : {&ambient,&sun,&fill})
        for (int i=0;i<3;++i) (*arr)[i]=trunc((*arr)[i]*point_factors[i]);
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
    model_falloff=mr?1/(mr*mr):INFINITY;
    world_falloff=world_radius?1/(world_radius*world_radius):INFINITY;
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
    const int y0=clampi((trunc(p.y)+11520-300)>>8,0,89);
    const int y1=clampi((trunc(p.y)+11520+300)>>8,0,89);
    const int x0=clampi((trunc(p.x)+11520-300)>>8,0,89);
    const int x1=clampi((trunc(p.x)+11520+300)>>8,0,89);
    for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x)
        for (auto it=nodes.rbegin();it!=nodes.rend();++it) {
            const Cell c=grid_cell(it->pos);
            if (c.x!=x || c.y!=y || it->layer!=layer) continue;
            const double r=it->model_radius;
            if (std::abs(it->pos.x-p.x)<=r && std::abs(it->pos.y-p.y)<=r &&
                std::abs(it->pos.z-p.z)<=r && out.size()<31) out.push_back(&*it);
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
            const double d2=dot(sub(cand[i]->pos,p),sub(cand[i]->pos,p));
            const auto key=(static_cast<long long>(trunc(d2-cand[i]->model_radius*cand[i]->model_radius))*128)|i;
            ranked.emplace_back(key,i,cand[i]);
        }
        std::sort(ranked.begin(),ranked.end());
        for (const auto &t:ranked) chosen.push_back(std::get<2>(t));
    }
    ModelLighting result; result.ambient={obj.ambient[0],obj.ambient[1],obj.ambient[2]};
    for (int i=0;i<int(chosen.size()) && i<2;++i) {
        const Node &n=*chosen[i];
        const Vec3 d=sub(n.pos,p); const double d2=dot(d,d);
        const double w=clamp((n.model_radius*n.model_radius-d2)*n.model_falloff,0,1);
        result.lights[i].direction=norm(d);
        for (int ch=0;ch<3;++ch)
            result.lights[i].rgb[ch]=trunc(n.model_rgb[ch]*w*obj.point_factors[ch]);
    }
    result.lights[2]={obj.fill_direction,{obj.fill[0],obj.fill[1],obj.fill[2]}};
    result.lights[3]={obj.sun_direction,{obj.sun[0],obj.sun[1],obj.sun[2]}};
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
    bool directional=false;
    for (const auto &node:nodes) if (dot(sub(node.pos,pos),sub(node.pos,pos))<
        node.world_radius*node.world_radius && node.floor<1) directional=true;
    std::array<double,3> acc{double(base),double(base),double(base)};
    for (std::size_t i=0;i<nodes.size() && i<4;++i) {
        const Node &n=nodes[i]; const Vec3 d=sub(n.pos,pos); const double d2=dot(d,d);
        double a=std::max((n.world_radius*n.world_radius-d2)*n.world_falloff,0.0);
        if (directional) {
            const double len=std::sqrt(d2);
            a*=clamp((len?dot(d,normal)/len:0)*0.99,n.floor,1);
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
        const double w=clamp((n->model_radius*n->model_radius-d2)*n->model_falloff*
                             d.z/std::sqrt(d2),0,1);
        for (int ch=0;ch<3;++ch) c[ch]+=n->model_rgb[ch]*w;
    }
    const double sun=std::max(level.sun_direction.z,0.0);
    Rgb out{};
    for (int ch=0;ch<3;++ch)
        out[ch]=clampi(trunc(c[ch]+level.sun[ch]*sun+level.ambient[ch]),0,255);
    return out;
}
bool Chunk::step(int frame,double rnd,Node &out) {
    int r=radius;
    if (mode==0) r=low;
    else if (mode==1) {
        if (frame>timer) { timer=frame+period; r=low+trunc((high-low)*rnd); }
    } else if (mode==2) {
        if (frame>timer) {
            timer=frame+period;
            if (down) { r=low; down=false; }
            else { r=high; down=true; }
        }
        if (period) r+=(down?-1:1)*trunc(double(high-low)/period);
    } else if (mode==3) {
        if (frame>timer) { timer=frame+period; r=r==low?high:low; }
    } else if (mode==4) return false;
    else return false;
    radius=clampi(r,0,299);
    const Rgb rgb{rgba[0],rgba[1],rgba[2]};
    out=Node({},rgb,rgb,radius,radius);
    return true;
}
int glow_size(int base,int shift,double noise) { return base+(trunc(noise*22)<<(shift&31)); }
int fire_radius(const std::string &kind,double noise) {
    if (kind=="torch" || kind=="lantern") return 160+trunc(noise*22);
    if (kind=="lamp") return 216+trunc(noise*22);
    return -1;
}
int candle_radius(int r,bool big) { return (big?84:48)+((r-8)&15); }
int lantern_world_radius(int r,const std::string &name) {
    return name.find("mine")==std::string::npos?r:0;
}
LightEffect::LightEffect(Rgba start,Rgba finish,double r0,double r1,double seconds,
                         int now,bool signed_col,double floor_,Vec3 p):
    radius(r0),floor(floor_),signed_colour(signed_col),pos(p) {
    life=std::max(1,trunc(seconds*60)); end=now+life;
    radius_rate=(r1-r0)/life;
    for (int i=0;i<4;++i) { colour[i]=start[i]; rate[i]=double(finish[i]-start[i])/life; }
}
bool LightEffect::run(int frame,Node &out,const Vec3 *owner) {
    for (int i=0;i<4;++i) colour[i]+=rate[i];
    radius+=radius_rate;
    const double lo=signed_colour?-255:0;
    for (int i=0;i<3;++i) colour[i]=trunc(clamp(colour[i],lo,255));
    if (owner) pos={owner->x,owner->y,owner->z+36};
    const Rgb rgb{trunc(colour[0]),trunc(colour[1]),trunc(colour[2])};
    out=Node(pos,rgb,rgb,radius,radius,floor);
    return frame<end;
}
} // namespace champions::rta::lighting
