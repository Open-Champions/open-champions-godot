#include "champions/fx.hpp"
#include "champions/lighting.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <iostream>
#include <string>

using namespace champions;
namespace F=champions::rta::fx;
namespace L=champions::rta::lighting;
namespace {
int assertions=0;
void check(bool ok,const char *what) {
    ++assertions;
    if (!ok) { std::cerr<<"FAIL: "<<what<<"\n"; std::exit(1); }
}
void near(double a,double b,double tol,const char *what) { check(std::abs(a-b)<=tol,what); }
// tools/generate_fx_trig_cases.py runs the pinned ref_particles (math_cos16 in
// place of the host cosine) on the same inputs (audit TRIG-1). Keys "case/field".
std::string vector_dir;
std::map<std::string,std::string> trig_cases() {
    std::map<std::string,std::string> out;
    std::ifstream in(vector_dir+"/fx_trig_cases.tsv");
    std::string c,f,v;
    while (in>>c>>f>>v) out[c+"/"+f]=v;
    check(!out.empty(),"fx_trig_cases.tsv present");
    return out;
}
void same_bits(double got,const std::map<std::string,std::string> &want,const std::string &key,const char *what) {
    const auto it=want.find(key);
    std::uint64_t bits=0; std::memcpy(&bits,&got,sizeof bits);
    check(it!=want.end() && bits==std::stoull(it->second,nullptr,16),what);
}
F::Definition make(int kind=1,int life=1,int tile=0x12,float gravity=-2.56f) {
    return F::Definition::build(1,3,gravity,{128,64,32,128},{0,64,255,0},life,kind,tile,0x100);
}
void particles() {
    auto d=make(0,2);
    check(d.life()==60 && d.list_index()==2,"life classes/list index");
    check(d.u0==65*16 && d.v0==33*16 && d.u1==95*16 && d.v1==63*16,"tile inset");
    near(d.size0,100,0,"size0"); near(d.size_rate,200.0/60,1e-4,"size rate");
    near(d.g,-0.01,1e-6,"gravity");
    check(d.colour_at(0)==F::Rgba{128,64,32,128},"start colour");
    // round 2: the rate (255-32)/60 is cut toward zero, so blue ends one short (was 255)
    check(d.colour_at(60)==F::Rgba{0,64,254,0},"end colour (rz rate)");
    check(d.colour0[0]==128+8388608,"colour bias");
    auto hidden=make(3);
    check(hidden.colour0[0]==128,"hidden colour lacks bias");
    auto line=make(2);
    near(line.line_lag,1,1e-5,"line lag");
    auto p=F::make_record(d,{10.7,-3.9,5},{1,-0.5,0.25},100,0,0);
    check(p.x==10 && p.y==-3 && p.z==5 && p.birth==100 &&
          p.vx==256 && p.vy==-128 && p.vz==64 && p.type4==0,"plain record");
    auto motion=F::make_record(make(1,1,0,-25.6f),{0,0,100},{16,0,0},0,0,0);
    const auto v=F::position_at(motion,make(1,1,0,-25.6f),10);
    near(v.x,10,1e-6,"motion x"); near(v.z,90,1e-3,"motion gravity");
    auto back=F::make_record(make(1,1,0,-25.6f),{100,200,300},{16,0,32},50,0.25,0);
    check(back.birth==35,"backdated birth");
    const auto now=F::position_at(back,make(1,1,0,-25.6f),15);
    near(now.x,100,1,"backdated x"); near(now.z,300,1,"backdated z");
    auto lp=F::make_record(line,{0,0,0},{16,0,0},0,0,0);
    auto lv=F::evaluate(lp,line,10);
    near(lv.pos.x,10,1e-6,"line head"); near(lv.tail.x,9,1e-6,"line tail");
    auto flip=make(1,1,0x1f);
    check(!flip.flipbook_step(1) && flip.flipbook_step(2),"flipbook timing");
    check(flip.u0==16 && flip.u1==31*16,"flipbook wrap");
    F::System s; s.put(make(1,0));
    check(s.spawn(0x100,{0,0,0})==F::Refusal::None,"spawn");
    for (int i=0;i<11;++i) s.tick();
    check(s.live_count()==1,"inclusive lifetime");
    s.tick(); check(s.live_count()==0 && s.free_page_count()==64,"FIFO retirement and page return");
    F::System pool; pool.put(make());
    for (int i=0;i<64*240+5;++i) pool.spawn(0x100,{});
    check(pool.live_count()==64*240 && pool.dropped()==5,"page capacity/drop");
    F::System slots;
    for (int i=1;i<=33;++i) {
        auto def=make(); def.address=i; slots.put(def);
        slots.spawn(i,{});
    }
    // round 2: slot 0 is held for good by 0x7e1d00, so 31 serve definitions (was 32 / 1)
    check(slots.live_count()==31 && slots.dropped()==2,"slot cap/drop (31 usable)");
    F::System groups; groups.put(make());
    const int h=groups.create_group();
    groups.begin_group(h); groups.spawn(0x100,{}); groups.end_group();
    groups.free_group(h);
    check(groups.free_page_count()==64 && groups.live_count()==1,"group-free resident-slot quirk");
    F::System idle;
    const int ih=idle.create_group();
    for (int i=0;i<31;++i) idle.tick();
    check(idle.begin_group(ih),"idle 31 retains group");
    idle.end_group(); idle.tick(); check(!idle.begin_group(ih),"idle 32 frees group");
    const std::string json=R"([{"site":1,"call":"particleDef_ctor","def":256,"complete":true,"size_start":1,"size_end":3,"gravity":-2.56,"start_rgba":[128,64,32,128],"end_rgba":[0,64,255,0],"life_class":1,"kind":1,"tile":18,"callback":0}])";
    F::System loaded; std::string error;
    check(loaded.load_json(json,error) && loaded.find(256),"user JSON loaded");
    check(!loaded.load_json(R"([{"complete":false}])",error) && loaded.find(256),"partial JSON refused atomically");
}
void emitters_and_primitives() {
    std::array<std::uint8_t,0x48> b{};
    b[0]=1;b[4]=5;b[8]=2;b[10]=0xff;b[11]=0xff;b[12]=0xfb;b[13]=0xff;
    b[14]=2;b[15]=0;b[16]=0x33;b[17]=0xfc;b[18]=10;b[20]=0xec;b[21]=0xff;
    b[22]=30;b[28]=40;b[31]=0x40;b[33]=1;b[34]=8;b[36]=8;b[38]=2;
    b[40]=6;b[42]=0;b[43]=8;b[44]=120;b[48]=60;b[52]=2;
    F::Emitter e;
    check(F::Emitter::parse(b.data(),b.size(),e),"0x48 emitter parse");
    check(e.rate==-4 && e.off_y==-20 && e.velocity.z==40 && e.yaw==0x4000,"emitter fields");
    check(e.definition(123).life()==60 && e.definition(123).size0==200,"emitter definition");
    F::Rng rng{0}; check(rng.step()==0x3c6ef35f,"cosmetic generator");
    F::Emitter rate; rate.rate=3; rng.seed=1;
    check(F::run_emitter(rate,{},10,rng).size()==3,"positive emitter rate");
    rate={}; rate.rate=-4;
    check(F::run_emitter(rate,{},10,rng).empty() &&
          F::run_emitter(rate,{},12,rng).size()==1,"negative emitter rate");
    rate={}; rate.rate=1;
    check(F::run_emitter(rate,{},0,rng).empty(),"first tick zero off");
    rate={}; rate.rate=2; rate.flags=1; rate.velocity={10,0,0};
    check(F::run_emitter(rate,{},1,rng,0,false,false,false,0,false).empty(),"weather gate");
    F::Emitter vector; vector.rate=1; vector.velocity={10,20,30};
    vector.off_x=4;vector.off_y=-6;vector.off_z=3;vector.yaw=0x2000;
    vector.pitch=0x1000;vector.box={8,10,12};vector.spread=6;vector.cone=0x800;
    F::Rng vector_rng{7};
    const auto output=F::run_emitter(vector,{100,50,25},1,vector_rng,0x1000);
    const auto want=trig_cases();
    check(output.size()==1 && want.count("vector/count") && want.at("vector/count")=="1" &&
          vector_rng.seed==std::stoul(want.at("vector/seed")) &&
          vector.on==std::stoi(want.at("vector/on")),"emitter vector draw order (generated)");
    same_bits(output[0].pos.x,want,"vector/pos0.x","emitter vector x (generated)");
    same_bits(output[0].pos.y,want,"vector/pos0.y","emitter vector y (generated)");
    same_bits(output[0].pos.z,want,"vector/pos0.z","emitter vector z (generated)");
    same_bits(output[0].vel.x,want,"vector/vel0.x","emitter vector vx (generated)");
    same_bits(output[0].vel.y,want,"vector/vel0.y","emitter vector vy (generated)");
    same_bits(output[0].vel.z,want,"vector/vel0.z","emitter vector vz (generated)");
    check(F::decal_rects[10].x0==192 && F::decal_rects[10].x1==256,"floor sprite cell");
    F::BodyMaterial body;
    check(F::body_material(0x2000,body)==F::Refusal::None &&
          body.hit_spawn_id==3 && body.gib_bits==5 && body.corpse_pool==3,
          "green body material");
    check(F::body_material(0x20000,body)==F::Refusal::None && body.metal_sparks,
          "metal body material");
    F::GutChunkPolicy gut;
    check(F::gut_chunk_policy(2,1,gut)==F::Refusal::None &&
          gut.invisible_drop && gut.particle_definition==0x870408 &&
          gut.splat_first==7 && gut.pool==6,"gore one forces invisible yellow drop");
    check(F::gut_chunk_policy(4|8,2,gut)==F::Refusal::None &&
          !gut.decals && !gut.invisible_drop && gut.particle_definition==0x870458,
          "green no-decal policy");
    check(F::gut_chunk_policy(2|4,2,gut)==F::Refusal::InvalidType,
          "undocumented simultaneous blood colours refused");
}
void lighting_vectors() {
    auto lv=L::Level::from_markers();
    check(lv.ambient[0]==32 && lv.sun[0]==68 && lv.base==128,"level defaults");
    check(L::Level::base_level(0x04000000)==64 && L::Level::base_level(0x20000000)==32,
          "base level flags");
    lv.sun={200,100,50,0};lv.ambient={100,100,100,0};lv.scale=0.5;
    auto obj=L::Object::reset(lv);
    // round 2: fill = trunc(0.79999995 x as-set) (was 80)
    check(obj.sun[0]==100 && obj.ambient[0]==50 && obj.fill[0]==79,"fill ignores scale");
    obj.hit_flash(); check(obj.ambient[0]==255,"hit flash");
    obj=L::Object::reset(lv);obj.chill(0.5);
    check(obj.ambient==F::Rgba{50,50,150,25},"chill bias uses as-set");
    check(L::grid_cell({0,0,0}).x==45 && !L::grid_cell({11520,0,0}).valid(),"light grid");
    L::Level dark;dark.sun={0,0,0,0};dark.ambient={0,0,0,0};dark.sun_direction={0,0,1};
    const auto plain=L::Object::reset(dark);
    std::vector<L::Node> nodes;
    nodes.emplace_back(F::Vec3{0,0,90},F::Rgb{0,0,200},F::Rgb{},100);
    nodes.emplace_back(F::Vec3{0,50,0},F::Rgb{0,200,0},F::Rgb{},100);
    nodes.emplace_back(F::Vec3{10,0,0},F::Rgb{200,100,0},F::Rgb{},100);
    const auto lit=L::model_lights({},nodes,plain,0);
    check(lit.lights[0].rgb==F::Rgb{198,99,0} && lit.lights[1].rgb==F::Rgb{0,150,0},
          "two best point lights");
    L::Level shade;shade.sun_direction={0,0,1};shade.sun={100,100,100,0};shade.ambient={20,20,20,0};
    const auto sl=L::model_lights({}, {},L::Object::reset(shade),0);
    // round 2: the fill is 15, not 16, so 20 + 99 + 14.85 (was 134)
    check(L::shade_model({0,0,1},sl)==F::Rgb{133,133,133},"model vertex shade");
    check(L::shade_model({0,0,-1},sl)==F::Rgb{20,20,20},"model back face");
    auto torch=L::Node::fire({0,0,100},200);
    check(L::shade_world(128,{torch},{},{0,0,1})==
          L::shade_world(128,{torch},{},{0,0,-1}),"world fire omni");
    check(L::shade_world(128,{torch},{},{0,0,1})[0]==166,"world fire value");
    L::Chunk ch;ch.low=100;ch.high=200;ch.mode=2;ch.period=10;
    L::Node n({}, {}, {}, 0);
    check(ch.step(1,0,n) && n.model_radius==190,"chunk triangle first");
    for (int i=2;i<=10;++i) ch.step(i,0,n);
    check(n.model_radius==100,"chunk triangle tenth");
    L::LightEffect fx({200,100,0,0},{0,0,0,0},100,50,0.5,1000);
    check(fx.run(1001,n) && n.model_rgb==F::Rgb{193,96,0},"light effect tick");
    for (int i=1002;i<=1030;++i) fx.run(i,n);
    check(n.model_rgb[0]==0 && std::abs(n.model_radius-50)<1e-3,"light effect end");
    L::Level decal;decal.sun_direction={0,0,1};decal.sun={10,10,10,0};decal.ambient={5,5,5,0};
    check(L::decal_tint({}, {L::Node::fire({0,0,50},100)},decal)[0]==165,
          "decal light tint");
    check(L::fire_radius("torch",0.5)==171 && L::candle_radius(3,false)==59 &&
          L::lantern_world_radius(170,"goblinmine2")==0,"fire object variants");
}
// Round 2 (research 19f80a6) through the round-1 API, which now delegates to the exact port.
void round2_delegation() {
    F::System s; s.put(make(1,3));
    const int h=s.create_group();
    check(s.begin_group(h),"begin live group");
    check(s.free_group(h),"free current group");
    check(s.spawn(0x100,{5,0,0})==F::Refusal::None,"freed-but-current group still receives");
    s.end_group();
    check(s.visible().empty() && s.live_count()==1,"neither drawn nor retired");
    check(s.pool().current_set()==0,"stale end keeps the freed slot current");
    const int h2=s.create_group();
    check((h2&255)==0 && s.visible().size()==1,"next group inherits the slot's lists");
    check(s.set_group_matrix(h2,{0,1,0,-1,0,0,0,0,1,10,20,30}) &&
          s.pool().group_slot(0).matrix[9]==10,"group matrix");
    check(s.slot_definition(1)==s.find(0x100) && s.pool().slot_record(0).held,"slot read access");
    F::Emitter e; e.rate=1; e.on_time=100; e.off_time=50;
    F::Rng rng{7};
    check(F::run_emitter(e,{},1,rng).size()==1 && e.on==1 && e.next>=51 && e.next<151,"emitter timer");
    L::Chunk ch; ch.low=100; ch.high=200; ch.mode=5; ch.radius=123;
    L::Node n({}, {}, {}, 0);
    check(ch.step(1,0,n) && n.model_radius==123,"chunk mode above 4 keeps its radius");
    L::LightSlots slots; L::Chunk a; a.mode=0; a.low=150; L::Chunk b; b.mode=3; b.low=1; b.high=2; b.timer=99;
    slots.run(a,1,10);
    const auto r=slots.run(b,2,20);
    check(r.first==1 && r.second && r.second->model_radius==0,"slots: free slot radius 0");
    L::LightEffect fx({0,0,0,0},{10,0,0,0},50,50,0.5,0);
    int last=0; for (int f=1;f<=30;++f) { fx.run(f,n); last=n.model_rgb[0]; }
    check(last>=9,"light effect keeps fractions");
}
}
int main(int argc,char **argv) {
    if (argc<2) { std::cerr<<"usage: champions-native-fx-tests VECTOR_DIR\n"; return 2; }
    vector_dir=argv[1];
    particles();emitters_and_primitives();lighting_vectors();round2_delegation();
    std::cout<<assertions<<" native FX synthetic assertions passed\n";
}
