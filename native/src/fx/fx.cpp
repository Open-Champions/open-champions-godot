#include "champions/fx.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace champions::fx {
namespace {
constexpr int life_ticks[4] = {11, 30, 60, 90};
constexpr double pi = 3.14159265358979323846;
int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
std::int16_t s16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
float f32(double v) { return static_cast<float>(v); }
double angle_rad(int a) { return s16(a) * (2.0 * pi / 65536.0); }
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 mul(Vec3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
double norm(Vec3 a) { return std::sqrt(a.x*a.x + a.y*a.y + a.z*a.z); }
Vec3 rotate_z(Vec3 v, int angle) {
    const double c = std::cos(angle_rad(angle)), s = std::sin(angle_rad(angle));
    return {c*v.x - s*v.y, s*v.x + c*v.y, v.z};
}
Vec3 rotate_y(Vec3 v, int angle) {
    const double c = std::cos(angle_rad(angle)), s = std::sin(angle_rad(angle));
    return {c*v.x + s*v.z, v.y, -s*v.x + c*v.z};
}
Vec3 rotate(Vec3 v, int yaw, int pitch) { return rotate_y(rotate_z(v, yaw), pitch); }
Vec3 flip(Vec3 v, bool nx, bool ny, bool swap) {
    if (nx) v.x = -v.x;
    if (ny) v.y = -v.y;
    if (swap) std::swap(v.x, v.y);
    return v;
}
int le16(const std::uint8_t *b, int o) { return b[o] | (int(b[o+1]) << 8); }
int les16(const std::uint8_t *b, int o) { return s16(le16(b,o)); }
int le32(const std::uint8_t *b, int o) {
    return static_cast<int>(std::uint32_t(b[o]) | (std::uint32_t(b[o+1]) << 8) |
                            (std::uint32_t(b[o+2]) << 16) | (std::uint32_t(b[o+3]) << 24));
}
}

Definition Definition::build(float ss, float se, float gravity, Rgba a, Rgba b,
                             int lc, int kd, int tx, std::uint32_t address) {
    Definition d;
    d.address = address; d.size_start = ss; d.size_end = se; d.gravity_input = gravity;
    d.start = a; d.end = b; d.life_class = clampi(lc,0,3); d.kind = clampi(kd,0,3); d.tile = tx;
    d.g = f32(double(gravity) * f32(1.0/256.0));
    d.size0 = f32(double(ss) * 100.0);
    if (d.kind == 2) {
        d.line_lag = f32(d.size0 * f32(0.01));
        d.size_rate = d.line_lag;
    } else {
        const float end_size = f32(double(se) * 100.0);
        d.size_rate = f32(f32(end_size - d.size0) * f32(1.0 / d.life()));
        const int col = tx & 15, row = tx >> 4;
        d.u0 = (col*32+1)*16; d.v0 = (row*32+1)*16;
        d.u1 = (col*32+31)*16; d.v1 = (row*32+31)*16;
    }
    const float bias = d.kind == 3 ? 0.0f : 8388608.0f;
    for (int i=0; i<4; ++i) {
        d.colour0[i] = f32(double(a[i]) + bias);
        d.colour_rate[i] = f32(double(b[i]-a[i]) * f32(1.0 / d.life()));
    }
    return d;
}
int Definition::life() const { return life_ticks[clampi(life_class,0,3)]; }
Rgba Definition::colour_at(double age) const {
    Rgba out{};
    const float bias = kind == 3 ? 0.0f : 8388608.0f;
    for (int i=0; i<4; ++i)
        out[i] = clampi(static_cast<int>(f32(colour0[i] + f32(colour_rate[i] * age)) - bias), 0, 255);
    return out;
}
bool Definition::flipbook_step(int frame) {
    if (kind == 2) return false;
    const int row = v0 >> 9;
    if (!((row == 1 || row == 2) && !(frame & 1)) && row != 4) return false;
    u0 += 0x200; u1 += 0x200;
    if (u0 > 0x1fff) { u0 -= 0x2000; u1 -= 0x2000; }
    return true;
}
Particle make_record(const Definition &d, Vec3 pos, Vec3 vel, int clock, double backdate, int slot) {
    int birth = clock;
    if (backdate > 0) {
        const int n = static_cast<int>(f32(backdate * 60.0));
        const float k = f32(double(n) * 0.0625);
        const float vz = f32(vel.z - f32(double(n << 5) * d.g));
        pos.x = f32(pos.x - f32(vel.x * k));
        pos.y = f32(pos.y - f32(vel.y * k));
        pos.z = f32(f32(pos.z - f32(vz * k)) - f32(f32(d.g * n) * n));
        vel.z = vz; birth -= n;
    }
    return {s16(static_cast<int>(pos.x)), s16(static_cast<int>(pos.y)),
            s16(static_cast<int>(pos.z)), s16(birth),
            s16(static_cast<int>(f32(vel.x * 256.0))),
            s16(static_cast<int>(f32(vel.y * 256.0))),
            s16(static_cast<int>(f32(vel.z * 256.0))), s16(slot * 4), d.address};
}
Vec3 position_at(const Particle &p, const Definition &d, double age) {
    return {p.x + p.vx / 4096.0 * age, p.y + p.vy / 4096.0 * age,
            p.z + p.vz / 4096.0 * age + d.g * age * age};
}
ParticleView evaluate(const Particle &p, const Definition &d, int clock) {
    ParticleView v;
    v.age = clock - p.birth; v.pos = position_at(p,d,v.age);
    v.tail = d.kind == 2 ? position_at(p,d,v.age-d.line_lag) : v.pos;
    v.size = d.size_at(v.age); v.rgba = d.colour_at(v.age);
    v.kind = d.kind; v.tile = d.tile; v.u0 = d.u0; v.v0 = d.v0;
    v.u1 = d.u1; v.v1 = d.v1; v.definition = d.address;
    return v;
}

Definition *System::find(std::uint32_t id) {
    const auto it = definitions_.find(id); return it == definitions_.end() ? nullptr : &it->second;
}
const Definition *System::find(std::uint32_t id) const {
    const auto it = definitions_.find(id); return it == definitions_.end() ? nullptr : &it->second;
}
bool System::put(Definition d) {
    if (d.live || d.address == 0 || d.kind < 0 || d.kind > 3) return false;
    auto old = find(d.address);
    if (old && old->live) return false;
    definitions_[d.address] = d; return true;
}
System::Lists &System::current_lists() { return current_group_ < 0 ? global_ : groups_[current_group_].lists; }
Refusal System::spawn(std::uint32_t id, Vec3 pos, Vec3 vel, double backdate) {
    Definition *d = find(id);
    if (!d) return Refusal::MissingDefinition;
    if (!std::isfinite(backdate) || !std::isfinite(pos.x) || !std::isfinite(pos.y) ||
        !std::isfinite(pos.z) || !std::isfinite(vel.x) || !std::isfinite(vel.y) ||
        !std::isfinite(vel.z)) return Refusal::InvalidDefinition;
    auto &lst = current_lists()[d->list_index()];
    if (d->live == 0 && std::find(slot_ids_.begin(),slot_ids_.end(),0) == slot_ids_.end()) {
        ++dropped_; return Refusal::PoolExhausted;
    }
    if (lst.pages.empty() || lst.pages.back().particles.size() == cells_per_page) {
        if (!free_pages_) { ++dropped_; return Refusal::PoolExhausted; }
        lst.pages.emplace_back(); --free_pages_;
    }
    if (d->live == 0) {
        const auto i = std::find(slot_ids_.begin(),slot_ids_.end(),0) - slot_ids_.begin();
        slot_ids_[i] = id; d->slot = static_cast<int>(i);
    }
    lst.pages.back().particles.push_back(make_record(*d,pos,vel,clock_,backdate,d->slot));
    ++d->live; return Refusal::None;
}
void System::retire(List &lst) {
    while (!lst.pages.empty()) {
        auto &pg = lst.pages.front();
        if (pg.particles.empty()) { lst.pages.pop_front(); ++free_pages_; continue; }
        const auto &p = pg.particles.front();
        Definition *d = find(p.definition);
        if (!d || p.birth + d->life() >= clock_) break;
        pg.particles.pop_front();
        if (--d->live == 0) { slot_ids_[d->slot] = 0; d->slot = -1; }
    }
    if (!lst.pages.empty() && lst.pages.front().particles.empty()) {
        lst.pages.pop_front(); ++free_pages_;
    }
}
void System::rebase(List &lst) {
    for (auto &pg : lst.pages) for (auto &p : pg.particles) p.birth = s16(p.birth - 0x8000);
}
void System::tick(bool cutscene) {
    ++clock_;
    if (clock_ == 0x8000) {
        clock_ = 0;
        for (auto &lst : global_) rebase(lst);
        for (auto &g : groups_) if (g.handle) for (auto &lst : g.lists) rebase(lst);
    }
    for (auto &lst : global_) retire(lst);
    for (auto &g : groups_) if (g.handle) {
        bool busy = false;
        for (auto &lst : g.lists) { busy |= !lst.pages.empty(); retire(lst); }
        if (cutscene || busy) g.idle = 0;
        else if (++g.idle >= 32) free_group(g.handle);
    }
}
void System::flipbook_draw(int frame) {
    for (auto &kv : definitions_) if (kv.second.slot >= 0) kv.second.flipbook_step(frame);
}
std::vector<ParticleView> System::visible() const {
    std::vector<ParticleView> out;
    auto collect = [&](const Lists &lists) {
        for (int li=0; li<12; ++li) for (const auto &pg : lists[li].pages)
            for (const auto &p : pg.particles) {
                const Definition *d = find(p.definition);
                if (d) out.push_back(evaluate(p,*d,clock_));
            }
    };
    collect(global_);
    for (const auto &g : groups_) if (g.handle) collect(g.lists);
    return out;
}
int System::create_group() {
    for (int i=0; i<groups; ++i) if (!groups_[i].handle) {
        const int h = (group_seq_++ << 8) | i;
        groups_[i].handle = h; groups_[i].idle = 0; return h;
    }
    return 0;
}
bool System::begin_group(int h) {
    const int i = h & 255;
    if (i >= groups || groups_[i].handle != h) return false;
    current_group_ = i; return true;
}
void System::end_group() { current_group_ = -1; }
bool System::free_group(int h) {
    const int i = h & 255;
    if (i >= groups || groups_[i].handle != h) return false;
    if (current_group_ == i) current_group_ = -1;
    for (auto &lst : groups_[i].lists) {
        free_pages_ += static_cast<int>(lst.pages.size());
        lst.pages.clear(); // original explicitly leaves definition live counts/slots.
    }
    groups_[i].handle = 0; return true;
}
void System::reset() {
    for (auto &kv : definitions_) { kv.second.live = 0; kv.second.slot = -1; }
    slot_ids_.fill(0); global_ = {}; groups_ = {};
    current_group_ = -1; clock_ = 0; free_pages_ = pages; dropped_ = 0;
}
int System::live_count() const {
    int n=0;
    for (const auto &kv : definitions_) n += kv.second.live;
    return n;
}

bool Emitter::parse(const std::uint8_t *b, std::size_t size, Emitter &out) {
    if (!b || size < 0x48) return false;
    std::copy(b,b+0x48,out.bytes.begin());
    for (int i=0;i<4;++i) { out.start[i]=b[i]; out.end[i]=b[i+4]; }
    out.size_start=les16(b,8); out.size_end=les16(b,10); out.gravity=les16(b,12);
    out.life_class=b[14]; out.kind=b[15]; out.tile=b[16]; out.rate=static_cast<std::int8_t>(b[17]);
    out.off_x=les16(b,18); out.off_y=les16(b,20); out.off_z=les16(b,22);
    out.velocity={double(les16(b,24)),double(les16(b,26)),double(les16(b,28))};
    out.yaw=le16(b,30); out.pitch=le16(b,32);
    for (int i=0;i<3;++i) out.box[i]=le16(b,34+2*i);
    out.spread=les16(b,40); out.cone=les16(b,42);
    out.on_time=le32(b,44); out.off_time=le32(b,48); out.flags=b[52];
    out.next=le32(b,56); out.on=b[60]; return true;
}
bool Emitter::same_definition(const Emitter &other) const {
    return std::equal(bytes.begin(),bytes.begin()+0x11,other.bytes.begin());
}
Definition Emitter::definition(std::uint32_t addr) const {
    return Definition::build(float(size_start),float(size_end),float(gravity),start,end,
                             life_class,kind,tile,addr);
}
std::vector<Emission> run_emitter(Emitter &r, Vec3 pos, int now, Rng &rng, int yaw,
                                  bool nx, bool ny, bool swap, int weather_angle, bool weather_on) {
    std::vector<Emission> out;
    const int count = r.rate > 0 ? r.rate : r.rate < 0 && now % -r.rate == 0 ? 1 : 0;
    if (r.rate < 0 && !count) return out;
    if (r.next < now) {
        if (!r.on_time) r.on = 1;
        else {
            const int d = r.on ? r.off_time : r.on_time;
            r.next = static_cast<int>(f32(now+d+f32((rng.draw()-0.5)*d)));
            r.on = !r.on;
        }
    }
    if (!r.on || !count) return out;
    for (int i=0;i<count;++i) {
        Vec3 o{(rng.draw()-0.5)*r.box[0],(rng.draw()-0.5)*r.box[1],(rng.draw()-0.5)*r.box[2]};
        o=rotate(o,yaw+int(r.yaw),int(r.pitch));
        const double c=std::cos(angle_rad(yaw)),s=std::sin(angle_rad(yaw));
        o.x += r.off_x*c+r.off_y*s; o.y += -r.off_x*s+r.off_y*c;
        o=flip(o,nx,ny,swap);
        const Vec3 p=add(pos,{o.x,o.y,o.z+r.off_z});
        Vec3 v=r.velocity;
        const double n=norm(v),j=rng.draw()-0.5;
        if (n>0) v=add(v,mul(v,j*r.spread/n));
        const int spin=s16(static_cast<int>(rng.step()>>16));
        const int tilt=s16(static_cast<int>(rng.draw()*r.cone));
        v=rotate(v,spin,tilt);
        if (r.flags&1) v=rotate(v,weather_angle,int(r.pitch));
        else if (r.flags&2) v=rotate(v,int(r.yaw),int(r.pitch));
        else v=flip(rotate(v,yaw+int(r.yaw),int(r.pitch)),nx,ny,swap);
        if (!(r.flags&1) || weather_on) out.push_back({p,v});
    }
    return out;
}

const std::array<TextureRect,11> decal_rects = {{
    {0,0,64,64},{64,64,96,96},{64,96,96,128},{64,0,128,64},
    {96,64,128,96},{96,96,128,128},{128,0,192,64},{128,64,160,96},
    {128,96,160,128},{0,64,64,128},{192,0,256,64}
}};
bool Primitives::add_glow(Glow glow) {
    if (glows_.size() >= 0x400) return false;
    glows_.push_back(glow); return true;
}
std::vector<Glow> Primitives::take_glows(bool before) {
    std::vector<Glow> out, keep;
    for (const auto &g:glows_) ((bool(g.flags&4)==before)?out:keep).push_back(g);
    glows_.swap(keep); return out;
}
int Primitives::create_trail(double seconds,int row,int width,int flags,Rgba rgba) {
    for (int i=0;i<64;++i) if (!trails_[i].handle) {
        Trail &t=trails_[i]; t={};
        t.handle=(trail_seq_++<<8)|i; t.lifetime=static_cast<int>(seconds*60);
        t.row=row; t.width_quarters=width/4; t.flags=flags; t.rgba=rgba; return t.handle;
    }
    return 0;
}
bool Primitives::add_trail_point(int h,Vec3 pos) {
    const int i=h&255;
    if (i>=64 || trails_[i].handle!=h || trails_[i].points.size()>=125) return false;
    trails_[i].points.push_back({pos,trails_[i].clock+1}); return true;
}
bool Primitives::release_trail(int h) {
    const int i=h&255;
    if (i>=64 || trails_[i].handle!=h) return false;
    trails_[i]={}; return true;
}
void Primitives::tick_trails() {
    for (auto &t:trails_) if (t.handle && !(t.flags&1)) {
        if (!(t.flags&0x10)) ++t.clock;
        while (!t.points.empty() && t.clock-t.points.front().stamp>=t.lifetime) {
            t.points.pop_front();
            if (t.flags&2 && !t.points.empty()) t.points.pop_front();
        }
        if (t.points.empty()) t={};
    }
}
int Primitives::create_beam(double jitter,double scale,Vec3 from,Vec3 to,int width,int depth,int alpha,int now) {
    for (int i=0;i<20;++i) if (!beams_[i].handle) {
        Beam &b=beams_[i]; b={};
        b.handle=(beam_seq_++<<8)|i; b.jitter=jitter; b.scale=scale;
        b.from=from; b.to=to; b.width=width; b.depth=clampi(depth,0,5); b.alpha=alpha;
        b.deadline=now+2; return b.handle;
    }
    return 0;
}
bool Primitives::update_beam(int h,Vec3 from,Vec3 to,int alpha,int width,int now) {
    const int i=h&255;
    if (i>=20 || beams_[i].handle!=h) return false;
    Beam &b=beams_[i]; b.from=from; b.to=to; b.alpha=alpha; b.width=width; b.deadline=now+2;
    return true;
}
void Primitives::draw_beams(int frame) {
    for (auto &b:beams_) if (b.handle && frame>b.deadline) b={};
}
Refusal Primitives::add_decal(double size,Vec3 pos,int angle,int type,int grow,int detail,Rgb tint) {
    if (type<0 || type>10) return Refusal::InvalidType;
    if ((detail==0 && type<3) || (detail==1 && type==0)) return Refusal::None;
    const int cx=(static_cast<int>(pos.x)+3840)>>8,cy=(static_cast<int>(pos.y)+3840)>>8;
    if (cx<0 || cx>=30 || cy<0 || cy>=30) return Refusal::PoolExhausted;
    const int cell=cy*30+cx;
    if (decal_cells_[cell]>=15) return Refusal::PoolExhausted;
    int used=0; for (int n:decal_cells_) if (n) ++used;
    if (!decal_cells_[cell] && used>=64) return Refusal::PoolExhausted;
    if (grow && growing_>=16) grow=0;
    decals_.push_back({pos,tint,grow?0.0:size,size,angle,type,grow,cx,cy});
    ++decal_cells_[cell]; if (grow) ++growing_;
    return Refusal::None;
}
void Primitives::grow_decals(bool frozen) {
    if (frozen) return;
    for (auto &d:decals_) if (d.grow) {
        d.size=std::min(d.target_size,d.size+(d.grow==1?0.2:0.05));
        if (d.size>=d.target_size) { d.grow=0; --growing_; }
    }
}

namespace {
struct Recipe { int id; std::uint32_t def; int count; double speed; int jitter; int spread; int zlift; };
constexpr Recipe recipes[] = {
    {0,0x8704a8,300,7.86,12,0,0},{1,0x8704f8,300,7.86,12,0,0},
    {2,0x4f8710,150,10.45,24,12,36},{3,0x86fcd8,150,10.16,12,0,18},
    {4,0x86fd28,150,16,12,24,18},{6,0x86fcd8,150,10.16,12,0,18},
    {8,0x86fe18,300,10.45,24,24,36},{9,0x86fc38,300,7.86,12,0,0},
    {10,0x86fe68,25,5.45,12,24,18},{10,0x86feb8,80,20,12,24,18},
    {11,0x86ff08,15,5.45,12,0,18},{11,0x86ff58,20,40,12,24,18},
    {11,0x86ffa8,5,0,12,0,18},{12,0x86ff08,15,5.45,12,0,18},
    {12,0x86ff58,20,40,12,24,18},{13,0x86fff8,1,0,0,0,0},
    {14,0x870048,1,0,0,0,0},{15,0x870098,1,0,0,0,0},
    {16,0x8700e8,1,0,0,0,0},{17,0x870188,1,0,0,0,0},
    {18,0x870138,1,0,0,0,0},{19,0x8701d8,1,21.6,0,0,0},
    {19,0x870228,1,5.4,0,0,0}
};
Vec3 unit_ball(Rng &rng) {
    for (;;) {
        Vec3 u{2*rng.draw()-1,2*rng.draw()-1,2*rng.draw()-1};
        if (norm(u)<=1) return u;
    }
}
int jitter(Rng &rng,int range) { return range ? int((rng.step()>>16)%unsigned(range))-range/2 : 0; }
}
SpawnResult spawn_fx(System &s,int id,Vec3 pos,Vec3 focus,int gore,Rng &rng) {
    if (id<0 || id>0x16) return {Refusal::InvalidType,0};
    if (id!=5 && norm({pos.x-focus.x,pos.y-focus.y,pos.z-focus.z})>480) return {};
    if ((id==0 || id==1) && gore==0) return {};
    if (id==5) return {Refusal::UnsupportedRecipe,0}; // Fire object and shared seed, not a particle recipe.
    if (id==7 || id==0x14 || id==0x15 || id==0x16) return {Refusal::UnknownDirection,0};
    SpawnResult result;
    for (const Recipe &r:recipes) if (r.id==id && !s.find(r.def))
        return {Refusal::MissingDefinition,0};
    Vec3 center=pos;
    const int first_jitter = (id==2 || id==8)?24:(id==17 || id==19 || (id>=13 && id<=18))?0:12;
    if (first_jitter) {
        center=add(center,{double(jitter(rng,first_jitter)),double(jitter(rng,first_jitter)),
                           double(jitter(rng,first_jitter))});
    }
    if (id==2 || id==8) center.z+=36;
    else if (id!=0 && id!=1 && id!=9 && id!=17 && id!=19) center.z+=18;
    Vec3 direction{};
    if (id==0 || id==1 || id==9) {
        Vec3 u=unit_ball(rng); const double n=norm(u);
        if (n) direction=mul(u,34.0/n);
        direction.z+=4;
    }
    for (const Recipe &r:recipes) if (r.id==id) for (int i=0;i<r.count;++i) {
        Vec3 p=center,v{};
        if ((id>=13 && id<=16) || id==18) {
            const double a=rng.draw()*2*pi,rad=rng.draw()*15;
            p=add(p,{std::cos(a)*rad,std::sin(a)*rad,(rng.draw()-0.5)*15});
        } else if (r.speed) v=mul(unit_ball(rng),r.speed);
        if (id==0 || id==1 || id==9) v=add(v,mul(direction,rng.draw()));
        if (r.spread) p=add(p,{double(jitter(rng,r.spread)),double(jitter(rng,r.spread)),
                               double(jitter(rng,r.spread))});
        const auto status=s.spawn(r.def,p,v);
        if (status==Refusal::None) ++result.spawned;
        else if (status!=Refusal::PoolExhausted) result.refusal=status;
    }
    return result;
}
} // namespace champions::fx
