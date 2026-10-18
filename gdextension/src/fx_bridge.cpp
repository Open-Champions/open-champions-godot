#include "fx_bridge.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <cmath>
#include <string>

using namespace godot;
namespace F=champions::rta::fx;
namespace L=champions::rta::lighting;
namespace {
F::Vec3 v3(const Vector3 &v) { return {v.x,v.y,v.z}; }
Vector3 gv(F::Vec3 v) { return {float(v.x),float(v.y),float(v.z)}; }
Vector3 rgb(const F::Rgb &v) { return {float(v[0]),float(v[1]),float(v[2])}; }
F::Rgb irgb(const Vector3 &v) { return {int(v.x),int(v.y),int(v.z)}; }
Dictionary status(F::Refusal r,int count=0) {
    Dictionary d; d["ok"]=r==F::Refusal::None; d["refusal"]=int(r); d["count"]=count;
    return d;
}
int byte(float c) { return std::max(0,std::min(255,int(c*255.0f))); }
F::Rgba rgba(const Color &c) { return {byte(c.r),byte(c.g),byte(c.b),byte(c.a)}; }
}
void ChampionsFxBridge::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_definitions_json","json"),&ChampionsFxBridge::load_definitions_json);
    ClassDB::bind_method(D_METHOD("spawn_particle","definition","position","velocity","backdate"),
                         &ChampionsFxBridge::spawn_particle);
    ClassDB::bind_method(D_METHOD("add_emitter","definition","record"),&ChampionsFxBridge::add_emitter);
    ClassDB::bind_method(D_METHOD("run_emitter","handle","position","frame","yaw","neg_x","neg_y",
                                  "swap_xy","weather_angle","weather_on"),&ChampionsFxBridge::run_emitter);
    ClassDB::bind_method(D_METHOD("spawn_event","source","tick","spawn_id","position","camera_focus","gore"),
                         &ChampionsFxBridge::spawn_event);
    ClassDB::bind_method(D_METHOD("tick","cutscene"),&ChampionsFxBridge::tick);
    ClassDB::bind_method(D_METHOD("draw_step","sim_frame","frozen"),&ChampionsFxBridge::draw_step);
    ClassDB::bind_method(D_METHOD("particle_snapshot"),&ChampionsFxBridge::particle_snapshot);
    ClassDB::bind_method(D_METHOD("set_level_light","sun_direction","sun","ambient","base","scale","layer"),
                         &ChampionsFxBridge::set_level_light);
    ClassDB::bind_method(D_METHOD("clear_light_nodes"),&ChampionsFxBridge::clear_light_nodes);
    ClassDB::bind_method(D_METHOD("add_light_node","position","model_rgb","world_rgb","model_radius",
                                  "world_radius","floor","layer","tint_decals"),&ChampionsFxBridge::add_light_node);
    ClassDB::bind_method(D_METHOD("model_light","position"),&ChampionsFxBridge::model_light);
    ClassDB::bind_method(D_METHOD("shade_model","position","normal"),&ChampionsFxBridge::shade_model);
    ClassDB::bind_method(D_METHOD("shade_world","position","normal"),&ChampionsFxBridge::shade_world);
    ClassDB::bind_method(D_METHOD("decal_tint","position"),&ChampionsFxBridge::decal_tint);
    ClassDB::bind_method(D_METHOD("add_decal","size","position","angle","type","grow","detail"),
                         &ChampionsFxBridge::add_decal);
    ClassDB::bind_method(D_METHOD("decal_snapshot"),&ChampionsFxBridge::decal_snapshot);
}
Dictionary ChampionsFxBridge::load_definitions_json(const String &json) {
    std::string error;
    const bool ok=particles_.load_json(std::string(json.utf8().get_data()),error);
    Dictionary out;out["ok"]=ok;out["error"]=String::utf8(error.c_str());return out;
}
Dictionary ChampionsFxBridge::spawn_particle(int64_t id,const Vector3 &p,const Vector3 &v,double backdate) {
    return status(particles_.spawn(std::uint32_t(id),v3(p),v3(v),backdate));
}
int64_t ChampionsFxBridge::add_emitter(int64_t id,const PackedByteArray &bytes) {
    F::Emitter rec;
    if (!F::Emitter::parse(bytes.ptr(),std::size_t(bytes.size()),rec) || id<=0) return 0;
    if (!particles_.put(rec.definition(std::uint32_t(id)))) return 0;
    const int handle=next_emitter_++;
    emitters_[handle]={std::uint32_t(id),rec}; return handle;
}
Dictionary ChampionsFxBridge::run_emitter(int64_t h,const Vector3 &p,int64_t frame,int64_t yaw,
                                          bool nx,bool ny,bool sw,int64_t weather_angle,bool weather_on) {
    auto it=emitters_.find(int(h));
    if (it==emitters_.end()) return status(F::Refusal::InvalidType);
    const auto emissions=F::run_emitter(it->second.second,v3(p),int(frame),rng_,int(yaw),nx,ny,sw,
                                         int(weather_angle),weather_on);
    // Definition id is the emitter's registered source address.
    int spawned=0;
    for (const auto &e:emissions) {
        if (particles_.spawn(it->second.first,e.pos,e.vel)==F::Refusal::None) ++spawned;
    }
    return status(F::Refusal::None,spawned);
}
Dictionary ChampionsFxBridge::spawn_event(int64_t source,int64_t tick,int64_t id,
                                          const Vector3 &p,const Vector3 &focus,int64_t gore) {
    if (source<0 || source>6 || tick<0) return status(F::Refusal::InvalidType);
    const auto r=F::spawn_fx(particles_,int(id),v3(p),v3(focus),int(gore),rng_);
    return status(r.refusal,r.spawned);
}
void ChampionsFxBridge::tick(bool cutscene) { particles_.tick(cutscene);primitives_.tick_trails(); }
void ChampionsFxBridge::draw_step(int64_t frame,bool frozen) {
    if (!frozen) { particles_.flipbook_draw(int(frame));primitives_.grow_decals();primitives_.draw_beams(int(frame)); }
}
Array ChampionsFxBridge::particle_snapshot() const {
    Array out;
    for (const auto &p:particles_.visible()) {
        Dictionary d;d["position"]=gv(p.pos);d["tail"]=gv(p.tail);
        d["size"]=p.size;d["rgba"]=Color(p.rgba[0]/255.0f,p.rgba[1]/255.0f,
                                       p.rgba[2]/255.0f,p.rgba[3]/255.0f);
        d["kind"]=p.kind;d["u0"]=p.u0;d["v0"]=p.v0;d["u1"]=p.u1;d["v1"]=p.v1;
        d["age"]=p.age;d["definition"]=int64_t(p.definition);out.push_back(d);
    }
    return out;
}
Dictionary ChampionsFxBridge::set_level_light(const Vector3 &dir,const Color &sun,
                                               const Color &ambient,int64_t base,double scale,int64_t layer) {
    if (base<0 || base>255 || layer<-128 || layer>127 || dir.length_squared()==0 ||
        !std::isfinite(scale)) return status(F::Refusal::InvalidType);
    level_.sun_direction=v3(dir.normalized());level_.sun=rgba(sun);level_.ambient=rgba(ambient);
    level_.base=int(base);level_.scale=scale;level_.layer=int(layer);
    return status(F::Refusal::None);
}
void ChampionsFxBridge::clear_light_nodes() { nodes_.clear(); }
void ChampionsFxBridge::add_light_node(const Vector3 &p,const Vector3 &m,const Vector3 &w,
                                       double mr,double wr,double floor,int64_t layer,bool tint) {
    nodes_.emplace_back(v3(p),irgb(m),irgb(w),mr,wr,floor,int(layer),tint);
}
Dictionary ChampionsFxBridge::model_light(const Vector3 &p) const {
    const auto l=L::model_lights(v3(p),nodes_,L::Object::reset(level_),level_.layer);
    Dictionary out;out["ambient"]=rgb(l.ambient);
    Array directions,colours;
    for (const auto &n:l.lights) { directions.push_back(gv(n.direction));colours.push_back(rgb(n.rgb)); }
    out["directions"]=directions;out["colours"]=colours;return out;
}
Vector3 ChampionsFxBridge::shade_model(const Vector3 &p,const Vector3 &normal) const {
    const auto l=L::model_lights(v3(p),nodes_,L::Object::reset(level_),level_.layer);
    return rgb(L::shade_model(v3(normal),l));
}
Vector3 ChampionsFxBridge::shade_world(const Vector3 &p,const Vector3 &normal) const {
    return rgb(L::shade_world(level_.base,nodes_,v3(p),v3(normal)));
}
Vector3 ChampionsFxBridge::decal_tint(const Vector3 &p) const {
    return rgb(L::decal_tint(v3(p),nodes_,level_));
}
Dictionary ChampionsFxBridge::add_decal(double size,const Vector3 &p,int64_t angle,
                                        int64_t type,int64_t grow,int64_t detail) {
    return status(primitives_.add_decal(size,v3(p),int(angle),int(type),int(grow),
                                         int(detail),L::decal_tint(v3(p),nodes_,level_)));
}
Array ChampionsFxBridge::decal_snapshot() const {
    Array out;
    for (const auto &d:primitives_.decals()) {
        Dictionary item;item["position"]=gv(d.pos);item["size"]=d.size;
        item["angle"]=d.angle;item["type"]=d.type;item["tint"]=rgb(d.tint);
        out.push_back(item);
    }
    return out;
}
