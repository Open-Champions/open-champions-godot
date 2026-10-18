#include "champions/fx.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>

namespace champions::rta::fx {
namespace {
exact::Vec3d d3(Vec3 v) { return {v.x, v.y, v.z}; }
Vec3 v3(const exact::Vec3d &v) { return {v[0], v[1], v[2]}; }
int le16(const std::uint8_t *b, int o) { return b[o] | (int(b[o+1]) << 8); }
int les16(const std::uint8_t *b, int o) { return exact::s16(le16(b,o)); }
int le32(const std::uint8_t *b, int o) {
    return static_cast<int>(std::uint32_t(b[o]) | (std::uint32_t(b[o+1]) << 8) |
                            (std::uint32_t(b[o+2]) << 16) | (std::uint32_t(b[o+3]) << 24));
}
exact::Particle to_exact(const Particle &p) {
    return {p.x, p.y, p.z, p.birth, p.vx, p.vy, p.vz, p.type4};
}
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}

void Definition::mirror() {
    slot = exact.slot; live = exact.live;
    u0 = exact.u0; v0 = exact.v0; u1 = exact.u1; v1 = exact.v1;
}
Definition Definition::build(float ss, float se, float gravity, Rgba a, Rgba b,
                             int lc, int kd, int tx, std::uint32_t address) {
    Definition d;
    d.address = address; d.size_start = ss; d.size_end = se; d.gravity_input = gravity;
    d.start = a; d.end = b;
    d.exact = exact::ParticleDef::build(ss, se, gravity, a, b, lc, kd, tx);
    d.life_class = d.exact.life_class; d.kind = d.exact.kind; d.tile = d.exact.tile;
    d.g = static_cast<float>(d.exact.gravity);
    d.size0 = static_cast<float>(d.exact.size0);
    d.size_rate = static_cast<float>(d.exact.size_rate);
    d.line_lag = static_cast<float>(d.exact.line_lag);
    for (int i=0; i<4; ++i) {
        d.colour0[i] = static_cast<float>(d.exact.c0[i]);
        d.colour_rate[i] = static_cast<float>(d.exact.crate[i]);
    }
    d.mirror();
    return d;
}
int Definition::life() const { return exact.life; }
Rgba Definition::colour_at(double age) const {
    const auto c = exact.colour_at(age);
    return {c[0], c[1], c[2], c[3]};
}
bool Definition::flipbook_step(int frame) {
    const bool stepped = exact::flipbook_step(exact, frame);
    mirror();
    return stepped;
}
Particle make_record(const Definition &d, Vec3 pos, Vec3 vel, int clock, double backdate, int slot) {
    const exact::Particle p = exact::spawn_record(d.exact, d3(pos), d3(vel), clock, backdate, slot);
    return {static_cast<std::int16_t>(p.x), static_cast<std::int16_t>(p.y),
            static_cast<std::int16_t>(p.z), static_cast<std::int16_t>(p.birth),
            static_cast<std::int16_t>(p.vx), static_cast<std::int16_t>(p.vy),
            static_cast<std::int16_t>(p.vz), static_cast<std::int16_t>(p.type4), d.address};
}
Vec3 position_at(const Particle &p, const Definition &d, double age) {
    return v3(exact::position_at(to_exact(p), d.exact, age));
}
ParticleView evaluate(const Particle &p, const Definition &d, int clock) {
    const exact::Particle e = to_exact(p);
    const exact::View x = exact::evaluate(e, d.exact, clock);
    ParticleView v;
    v.age = x.age; v.pos = v3(x.pos);
    v.tail = d.kind == exact::KIND_LINE ? v3(exact::line_endpoints(e, d.exact, clock).first) : v.pos;
    v.size = static_cast<float>(x.size); v.rgba = {x.rgba[0], x.rgba[1], x.rgba[2], x.rgba[3]};
    v.kind = d.kind; v.tile = d.tile; v.u0 = d.exact.u0; v.v0 = d.exact.v0;
    v.u1 = d.exact.u1; v.v1 = d.exact.v1; v.definition = d.address;
    return v;
}

Definition *System::find(std::uint32_t id) {
    const auto it = definitions_.find(id); return it == definitions_.end() ? nullptr : &it->second;
}
const Definition *System::find(std::uint32_t id) const {
    const auto it = definitions_.find(id); return it == definitions_.end() ? nullptr : &it->second;
}
bool System::put(Definition d) {
    if (d.live || d.exact.live || d.address == 0 || d.kind < 0 || d.kind > 3) return false;
    auto old = find(d.address);
    if (old && old->exact.live) return false;
    d.exact.slot = -1; d.exact.live = 0; d.mirror();
    definitions_[d.address] = d; return true;
}
void System::mirror_all() {
    for (auto &kv : definitions_) kv.second.mirror();
}
Refusal System::spawn(std::uint32_t id, Vec3 pos, Vec3 vel, double backdate) {
    Definition *d = find(id);
    if (!d) return Refusal::MissingDefinition;
    if (!std::isfinite(backdate) || !finite(pos) || !finite(vel)) return Refusal::InvalidDefinition;
    std::optional<exact::Particle> p;
    try {
        p = pool_.spawn(d->exact, d3(pos), d3(vel), backdate);
    } catch (const std::domain_error &) {
        return Refusal::InvalidDefinition; // a value beyond 64-bit truncation; state untouched
    }
    d->mirror();
    return p ? Refusal::None : Refusal::PoolExhausted;
}
void System::tick(bool cutscene) {
    pool_.update(cutscene);
    mirror_all();
}
void System::flipbook_draw(int frame) {
    for (auto &kv : definitions_)
        if (kv.second.exact.slot >= 0) kv.second.flipbook_step(frame);
}
std::vector<ParticleView> System::visible() const {
    std::map<const exact::ParticleDef *, const Definition *> by_exact;
    for (const auto &kv : definitions_) by_exact[&kv.second.exact] = &kv.second;
    std::vector<ParticleView> out;
    for (const auto &lp : pool_.live()) {
        if (lp.definition->kind == exact::KIND_HIDDEN) continue;
        const auto it = by_exact.find(lp.definition);
        if (it == by_exact.end()) continue;
        const exact::Particle &e = lp.particle;
        const Particle p{static_cast<std::int16_t>(e.x), static_cast<std::int16_t>(e.y),
                         static_cast<std::int16_t>(e.z), static_cast<std::int16_t>(e.birth),
                         static_cast<std::int16_t>(e.vx), static_cast<std::int16_t>(e.vy),
                         static_cast<std::int16_t>(e.vz), static_cast<std::int16_t>(e.type4),
                         it->second->address};
        out.push_back(evaluate(p, *it->second, pool_.clock()));
    }
    return out;
}
const Definition *System::slot_definition(int slot) const {
    const exact::ParticleDef *e = pool_.slot_definition(slot);
    if (!e) return nullptr;
    for (const auto &kv : definitions_) if (&kv.second.exact == e) return &kv.second;
    return nullptr;
}
int System::create_group() { return static_cast<int>(pool_.group_create()); }
bool System::begin_group(int h) {
    if (!pool_.group(h)) return false;
    pool_.group_begin(h); current_handle_ = h; return true;
}
void System::end_group() { pool_.group_end(current_handle_); }
void System::end_group(int h) { pool_.group_end(h); }
bool System::free_group(int h) {
    if (!pool_.group(h)) return false;
    pool_.group_free(h); mirror_all(); return true;
}
bool System::set_group_matrix(int h, const exact::Mat34d &m) {
    if (!pool_.group(h)) return false;
    pool_.group_set_matrix(h, m); return true;
}
void System::reset() {
    pool_ = exact::ParticleSystem();
    for (auto &kv : definitions_) { kv.second.exact.live = 0; kv.second.exact.slot = -1; kv.second.mirror(); }
    current_handle_ = 0;
}
int System::live_count() const {
    int n=0;
    for (const auto &kv : definitions_) n += kv.second.exact.live;
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
exact::EmitterRecord Emitter::record() const {
    exact::EmitterRecord r;
    r.start_rgba = start; r.end_rgba = end;
    r.size_start = size_start; r.size_end = size_end; r.gravity = gravity;
    r.life_class = life_class; r.kind = kind; r.tile = tile; r.rate = rate;
    r.off_x = off_x; r.off_y = off_y; r.off_z = off_z;
    r.vel = {static_cast<int>(velocity.x), static_cast<int>(velocity.y), static_cast<int>(velocity.z)};
    r.yaw = static_cast<int>(yaw); r.pitch = static_cast<int>(pitch);
    r.box = {static_cast<int>(box[0]), static_cast<int>(box[1]), static_cast<int>(box[2])};
    r.spread = spread; r.cone = cone; r.on_time = on_time; r.off_time = off_time;
    r.flags = flags; r.next = next; r.on = on;
    return r;
}
std::vector<Emission> run_emitter(Emitter &r, Vec3 pos, int now, Rng &rng, int yaw,
                                  bool nx, bool ny, bool swap, int weather_angle, bool weather_on) {
    exact::EmitterRecord rec = r.record();
    exact::CosmeticRng c(rng.seed);
    const auto out = exact::emitter_run(rec, d3(pos), now, c, yaw, nx, ny, swap, weather_angle, weather_on);
    r.next = static_cast<int>(rec.next); r.on = rec.on; rng.seed = c.seed;
    std::vector<Emission> result;
    for (const auto &e : out) result.push_back({v3(e.pos), v3(e.vel)});
    return result;
}

const std::array<TextureRect,11> decal_rects = {{
    {0,0,64,64},{64,64,96,96},{64,96,96,128},{64,0,128,64},
    {96,64,128,96},{96,96,128,128},{128,0,192,64},{128,64,160,96},
    {128,96,160,128},{0,64,64,128},{192,0,256,64}
}};
Refusal body_material(std::uint32_t flag,BodyMaterial &out) {
    switch (flag) {
    case 0x40000: out={flag,0,0,0,false}; break;
    case 0x1000: out={flag,9,3,6,false}; break;
    case 0x2000: out={flag,3,5,3,false}; break;
    case 0x4000: out={flag,10,-1,10,false}; break;
    case 0x8000: out={flag,11,-1,-1,false}; break;
    case 0x10000: out={flag,2,-1,-1,false}; break;
    case 0x20000: out={flag,-1,-1,-1,true}; break;
    default: return Refusal::InvalidType;
    }
    return Refusal::None;
}
Refusal gut_chunk_policy(unsigned bits,int detail,GutChunkPolicy &out) {
    if (detail<0 || detail>2 || ((bits&2) && (bits&4))) return Refusal::InvalidType;
    out={};
    out.invisible_drop=(bits&1)!=0 || detail<2;
    out.decals=(bits&8)==0;
    out.particle_trail=detail>=1;
    if (bits&2) {
        out.particle_definition=0x870408;out.splat_first=7;out.splat_second=8;out.pool=6;
    } else if (bits&4) {
        out.particle_definition=0x870458;out.splat_first=4;out.splat_second=5;out.pool=3;
    } else {
        out.particle_definition=0x8703b8;out.splat_first=1;out.splat_second=2;out.pool=0;
    }
    return Refusal::None;
}
} // namespace champions::rta::fx
