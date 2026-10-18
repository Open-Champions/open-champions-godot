#include "fx_bridge.hpp"
#include "champions/archive.hpp"
#include "champions/texture.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

using namespace godot;
namespace S = champions::rta::fx::sim;
namespace R = champions::rta::fx::render;
namespace RC = champions::rta::fx::recipes;
namespace X = champions::rta::fx::exact;
namespace L = champions::rta::lighting;
namespace core = champions::simulation::rta;

namespace {
core::Raw raw(float f) {
    core::Raw r;
    std::memcpy(&r, &f, sizeof r);
    return r;
}
core::Raw raw(double d) { return raw(static_cast<float>(d)); }
S::Vec3 rv(const Vector3 &v) { return {raw(v.x), raw(v.y), raw(v.z)}; }
L::Vec3 lv(const Vector3 &v) { return {v.x, v.y, v.z}; }
Vector3 gv(const L::Vec3 &v) { return {float(v.x), float(v.y), float(v.z)}; }
Vector3 rgb(const L::Rgb &v) { return {float(v[0]), float(v[1]), float(v[2])}; }
L::Rgb irgb(const Vector3 &v) { return {int(v.x), int(v.y), int(v.z)}; }
std::uint8_t byte(float c) { return static_cast<std::uint8_t>(std::max(0, std::min(255, int(std::lround(c * 255.0f))))); }
L::Rgba rgba(const Color &c) { return {byte(c.r), byte(c.g), byte(c.b), byte(c.a)}; }
std::string text(const String &s) { return std::string(s.utf8().get_data()); }
std::string lower(std::string s) {
    for (char &c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}
std::string basename(const std::string &name) {
    const auto at = name.find_last_of("/\\");
    return lower(at == std::string::npos ? name : name.substr(at + 1));
}
bool floats(const Dictionary &d, const char *key, std::size_t n, std::vector<core::Raw> &out) {
    if (!d.has(key)) return false;
    const PackedFloat32Array a = d[key];
    if (static_cast<std::size_t>(a.size()) != n) return false;
    out.clear();
    for (int64_t i = 0; i < a.size(); ++i) out.push_back(raw(a[i]));
    return true;
}
Dictionary refusal(const String &why) {
    Dictionary d;
    d["ok"] = false;
    d["refusal"] = why;
    return d;
}
// The effect textures by lump and name (docs/effects.md 12.2, 14.2): read from the user's
// own archives, never bundled.
struct Wanted {
    const char *lump;
    const char *name;
};
const Wanted WANTED[] = {{S::ENGINE_LUMP, S::PARTICLE_SHEET}, {S::ENGINE_LUMP, S::TRAIL_TEXTURE},
                         {S::ENGINE_LUMP, S::FLOOR_TEXTURE}};
} // namespace

ChampionsFxBridge::ChampionsFxBridge() : world_(std::make_unique<S::EffectWorld>()) {}

void ChampionsFxBridge::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_definitions_json", "json"), &ChampionsFxBridge::load_definitions_json);
    ClassDB::bind_method(D_METHOD("load_effect_textures", "archive_paths"), &ChampionsFxBridge::load_effect_textures);
    ClassDB::bind_method(D_METHOD("set_detail", "detail"), &ChampionsFxBridge::set_detail);
    ClassDB::bind_method(D_METHOD("set_wait_first", "wait_first"), &ChampionsFxBridge::set_wait_first);
    ClassDB::bind_method(D_METHOD("spawn_particle", "definition", "position", "velocity", "backdate"),
                         &ChampionsFxBridge::spawn_particle);
    ClassDB::bind_method(D_METHOD("fx_spawn", "id", "position", "near"), &ChampionsFxBridge::fx_spawn);
    ClassDB::bind_method(D_METHOD("add_emitter", "definition", "record"), &ChampionsFxBridge::add_emitter);
    ClassDB::bind_method(D_METHOD("run_emitter", "handle", "position", "frame", "yaw", "neg_x", "neg_y",
                                  "swap_xy", "weather_angle", "weather_on"), &ChampionsFxBridge::run_emitter);
    ClassDB::bind_method(D_METHOD("add_glow", "position", "colour", "texture_word", "bias", "size", "angle", "flags"),
                         &ChampionsFxBridge::add_glow);
    ClassDB::bind_method(D_METHOD("create_trail", "seconds", "row", "width", "flags", "colour"),
                         &ChampionsFxBridge::create_trail);
    ClassDB::bind_method(D_METHOD("add_trail_point", "handle", "position"), &ChampionsFxBridge::add_trail_point);
    ClassDB::bind_method(D_METHOD("release_trail", "handle"), &ChampionsFxBridge::release_trail);
    ClassDB::bind_method(D_METHOD("create_beam", "jitter", "scale", "from", "to", "width", "depth", "colour"),
                         &ChampionsFxBridge::create_beam);
    ClassDB::bind_method(D_METHOD("update_beam", "handle", "from", "to", "alpha", "width"),
                         &ChampionsFxBridge::update_beam);
    ClassDB::bind_method(D_METHOD("add_decal", "size", "position", "angle", "type", "grow"),
                         &ChampionsFxBridge::add_decal);
    ClassDB::bind_method(D_METHOD("tick", "sim_frame", "cutscene"), &ChampionsFxBridge::tick);
    ClassDB::bind_method(D_METHOD("draw", "camera", "frozen"), &ChampionsFxBridge::draw);
    ClassDB::bind_method(D_METHOD("counts"), &ChampionsFxBridge::counts);
    ClassDB::bind_method(D_METHOD("set_level_light", "sun_direction", "sun", "ambient", "base", "scale", "layer"),
                         &ChampionsFxBridge::set_level_light);
    ClassDB::bind_method(D_METHOD("clear_light_nodes"), &ChampionsFxBridge::clear_light_nodes);
    ClassDB::bind_method(D_METHOD("add_light_node", "position", "model_rgb", "world_rgb", "model_radius",
                                  "world_radius", "floor", "layer", "tint_decals"), &ChampionsFxBridge::add_light_node);
    ClassDB::bind_method(D_METHOD("model_light", "position"), &ChampionsFxBridge::model_light);
    ClassDB::bind_method(D_METHOD("shade_model", "position", "normal"), &ChampionsFxBridge::shade_model);
    ClassDB::bind_method(D_METHOD("shade_world", "position", "normal"), &ChampionsFxBridge::shade_world);
    ClassDB::bind_method(D_METHOD("decal_tint", "position"), &ChampionsFxBridge::decal_tint);
}

Dictionary ChampionsFxBridge::load_definitions_json(const String &json) {
    std::string error;
    const bool ok = world_->load_definitions(text(json), error);
    Dictionary out;
    out["ok"] = ok;
    out["error"] = String::utf8(error.c_str());
    return out;
}

Dictionary ChampionsFxBridge::load_effect_textures(const PackedStringArray &paths) {
    std::vector<Wanted> wanted(std::begin(WANTED), std::end(WANTED));
    for (const S::GlowTexture &g : S::glow_textures()) wanted.push_back({g.lump, g.name});
    Dictionary textures;
    Array refusals;
    std::map<std::string, int> seen;
    for (int64_t pi = 0; pi < paths.size(); ++pi) {
        const String path = paths[pi];
        const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
        if (file.is_null()) {
            refusals.push_back(String("cannot open ") + path);
            continue;
        }
        const PackedByteArray data = file->get_buffer(int64_t(file->get_length()));
        const champions::Bytes bytes{data.ptr(), static_cast<std::size_t>(data.size())};
        try {
            std::vector<std::pair<std::string, champions::Bytes>> lumps;
            const std::string name = text(path);
            if (champions::hasExtension(name, ".gob")) {
                for (const auto &e : champions::readGob(bytes))
                    if (champions::hasExtension(e.name, ".lmp")) lumps.emplace_back(basename(e.name), champions::payload(bytes, e));
            } else {
                lumps.emplace_back(basename(name), bytes);
            }
            for (const auto &lump : lumps)
                for (const auto &e : champions::readLmp(lump.second)) {
                    const std::string entry = basename(e.name);
                    for (const Wanted &w : wanted) {
                        if (lump.first != w.lump || entry != w.name) continue;
                        if (++seen[entry] > 1) {
                            refusals.push_back(String("more than one ") + w.lump + ":" + w.name);
                            textures.erase(String(w.name));
                            continue;
                        }
                        const auto tex = champions::decodeRtaTexture(champions::payload(lump.second, e));
                        Dictionary row;
                        row["width"] = int64_t(tex.width);
                        row["height"] = int64_t(tex.height);
                        PackedByteArray pixels;
                        pixels.resize(int64_t(tex.rgba.size()));
                        std::memcpy(pixels.ptrw(), tex.rgba.data(), tex.rgba.size());
                        row["rgba"] = pixels;
                        textures[String(w.name)] = row;
                        for (const S::GlowTexture &g : S::glow_textures())
                            if (std::string(g.name) == entry && std::string(g.lump) == lump.first)
                                world_->glow_dims[g.global] = {std::int32_t(tex.width), std::int32_t(tex.height)};
                        if (entry == S::PARTICLE_SHEET && (tex.width != S::SHEET_WIDTH || tex.height != S::SHEET_HEIGHT))
                            refusals.push_back("particletex.tex is not 512 x 160 as documented (docs/effects.md 13.9)");
                    }
                }
        } catch (const std::exception &e) {
            refusals.push_back(path + String(": ") + String::utf8(e.what()));
        }
    }
    Array missing;
    for (const Wanted &w : wanted)
        if (!textures.has(String(w.name))) missing.push_back(String(w.lump) + ":" + w.name);
    Dictionary out;
    out["ok"] = missing.is_empty() && refusals.is_empty();
    out["textures"] = textures;
    out["missing"] = missing;
    out["refusals"] = refusals;
    return out;
}

void ChampionsFxBridge::set_detail(int64_t detail) { world_->decals.detail = detail; }
void ChampionsFxBridge::set_wait_first(bool wait_first) {
    world_->waitq = wait_first ? R::WaitQ::WaitFirst : R::WaitQ::Vu1CppReading;
}

Dictionary ChampionsFxBridge::spawn_particle(int64_t id, const Vector3 &p, const Vector3 &v, double backdate) {
    const auto missing = world_->missing_definitions;
    world_->particle_spawn(std::uint32_t(id), rv(p), rv(v), raw(backdate));
    Dictionary d;
    d["ok"] = world_->missing_definitions == missing;
    if (world_->missing_definitions != missing) d["refusal"] = "definition not in the user's JSON";
    return d;
}

Dictionary ChampionsFxBridge::fx_spawn(int64_t id, const Vector3 &p, bool near) {
    // Id 5 makes Fire objects from the shared seed: gameplay, run by the combat runtime's
    // EffectSimulation, never by a presentation bridge.
    if (id == 5) return refusal("fx_spawn id 5 is gameplay (shared seed, Fire objects)");
    core::Lcg unused;
    const RC::Calls calls = RC::fx_spawn(std::int32_t(id), rv(p), world_->cosmetic, unused, near,
                                         std::int32_t(world_->decals.detail), &world_->made);
    world_->apply(calls);
    Dictionary d;
    d["ok"] = true;
    d["calls"] = int64_t(calls.size());
    return d;
}

int64_t ChampionsFxBridge::add_emitter(int64_t id, const PackedByteArray &bytes) {
    if (id <= 0 || bytes.size() < 0x48) return 0;
    try {
        X::EmitterRecord rec = X::EmitterRecord::parse(bytes.ptr(), std::size_t(bytes.size()));
        if (!world_->add_definition(std::uint32_t(id), X::ParticleDef::from_emitter_record(rec))) return 0;
        const int handle = next_emitter_++;
        emitters_[handle] = {std::uint32_t(id), rec};
        return handle;
    } catch (const std::exception &) {
        return 0;
    }
}

Dictionary ChampionsFxBridge::run_emitter(int64_t h, const Vector3 &p, int64_t frame, int64_t yaw, bool nx,
                                          bool ny, bool sw, int64_t weather_angle, bool weather_on) {
    const auto it = emitters_.find(int(h));
    if (it == emitters_.end()) return refusal("unknown emitter");
    X::CosmeticRng rng(world_->cosmetic.state);   // emitters draw the cosmetic seed
    const auto emissions = X::emitter_run(it->second.second, {double(p.x), double(p.y), double(p.z)}, frame, rng,
                                          yaw, nx, ny, sw, weather_angle, weather_on);
    world_->cosmetic.state = rng.seed;
    for (const auto &e : emissions) world_->particle_spawn(it->second.first, e.pos, e.vel);
    Dictionary d;
    d["ok"] = true;
    d["count"] = int64_t(emissions.size());
    return d;
}

bool ChampionsFxBridge::add_glow(const Vector3 &p, const Color &colour, int64_t word, double bias, double size,
                                 int64_t angle, int64_t flags) {
    R::Glow g;
    g.pos = rv(p);
    const auto c = rgba(colour);
    g.rgba = {std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3])};
    g.tex = std::uint32_t(word);
    g.bias = raw(bias);
    g.size = raw(size);
    g.angle = std::uint16_t(angle);
    g.flags = std::int16_t(flags);
    return world_->glow_add(g);
}

int64_t ChampionsFxBridge::create_trail(double seconds, int64_t row, int64_t width, int64_t flags, const Color &colour) {
    const auto c = rgba(colour);
    try {
        return world_->trails.create(raw(seconds), std::int32_t(row), std::int32_t(width), std::uint32_t(flags),
                                     std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3]));
    } catch (const std::exception &) {
        return 0;
    }
}
bool ChampionsFxBridge::add_trail_point(int64_t h, const Vector3 &p) {
    return world_->trails.add_point(std::uint32_t(h), rv(p));
}
void ChampionsFxBridge::release_trail(int64_t h) { world_->trails.release(std::uint32_t(h)); }

int64_t ChampionsFxBridge::create_beam(double jitter, double scale, const Vector3 &from, const Vector3 &to,
                                       int64_t width, int64_t depth, const Color &colour) {
    const auto c = rgba(colour);
    try {
        return world_->beams.create(raw(jitter), raw(scale), rv(from), rv(to), std::int32_t(width), std::int32_t(depth),
                                    {std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3])},
                                    sim_frame_, world_->in_sim_tick, world_->cosmetic, world_->libc);
    } catch (const std::exception &) {
        return 0;
    }
}
void ChampionsFxBridge::update_beam(int64_t h, const Vector3 &from, const Vector3 &to, int64_t alpha, int64_t width) {
    world_->beams.update(std::uint32_t(h), rv(from), rv(to), std::int32_t(alpha), std::int32_t(width), sim_frame_);
}
void ChampionsFxBridge::add_decal(double size, const Vector3 &p, int64_t angle, int64_t type, int64_t grow) {
    world_->decal_add(raw(size), rv(p), std::int32_t(angle), std::int32_t(type), std::int32_t(grow));
}

void ChampionsFxBridge::tick(int64_t sim_frame, bool cutscene) {
    sim_frame_ = std::int32_t(sim_frame);
    world_->tick(sim_frame_, cutscene);
}

// camera: {"view": PackedFloat32Array(12), "proj_scale": float, "camera": PackedFloat32Array(12),
//          "viewproj": PackedFloat32Array(16), "sim_frame": int, "net_frame": int}: the game's
// own view mat34, g_projScale, g_cameraMatrix and g_viewProj (fx_simulation.hpp Camera).
Dictionary ChampionsFxBridge::draw(const Dictionary &camera, bool frozen) {
    S::Camera cam;
    std::vector<core::Raw> view, camm, vp;
    if (!floats(camera, "view", 12, view) || !floats(camera, "camera", 12, camm) ||
        !floats(camera, "viewproj", 16, vp) || !camera.has("proj_scale"))
        return refusal("camera needs view[12], camera[12], viewproj[16] and proj_scale from the game");
    std::copy(view.begin(), view.end(), cam.view.begin());
    std::copy(camm.begin(), camm.end(), cam.camera.begin());
    std::copy(vp.begin(), vp.end(), cam.viewproj.begin());
    cam.proj_scale = raw(double(camera["proj_scale"]));
    const std::int32_t sim = camera.has("sim_frame") ? std::int32_t(int64_t(camera["sim_frame"])) : sim_frame_;
    const std::int32_t net = camera.has("net_frame") ? std::int32_t(int64_t(camera["net_frame"])) : sim;
    Array runs, refusals;
    try {
        const auto packets = world_->draw(cam, sim, net, frozen);
        S::Presentation decals = S::present_decals(world_->decals, cam);
        S::Presentation fx = S::present(packets, world_->glow_dims);
        for (const auto &why : fx.refusals) refusals.push_back(String::utf8(why.c_str()));
        for (const auto *p : {&decals, &fx})
            for (const S::Run &r : p->runs) {
                Dictionary d;
                d["texture"] = String::utf8(r.texture.c_str());
                d["blend"] = int64_t(r.alpha1);
                d["filter"] = r.linear ? "linear" : "nearest";
                d["prim"] = r.lines ? "lines" : "triangles";
                PackedVector3Array vs;
                PackedColorArray cs;
                PackedVector2Array uvs;
                for (std::size_t i = 0; i < r.vertices.size(); ++i) {
                    vs.push_back(Vector3(r.vertices[i][0], r.vertices[i][1], r.vertices[i][2]));
                    cs.push_back(Color::from_rgba8(r.colors[i][0], r.colors[i][1], r.colors[i][2], r.colors[i][3]));
                    uvs.push_back(Vector2(r.uvs[i][0], r.uvs[i][1]));
                }
                d["vertices"] = vs;
                d["colors"] = cs;
                d["uvs"] = uvs;
                runs.push_back(d);
            }
    } catch (const std::exception &e) {
        return refusal(String::utf8(e.what()));
    }
    if (world_->missing_definitions)
        refusals.push_back(String("particles of definitions missing from the user's JSON: ") +
                           String::num_int64(world_->missing_definitions));
    Dictionary out;
    out["ok"] = true;
    out["runs"] = runs;
    out["refusals"] = refusals;
    return out;
}

Dictionary ChampionsFxBridge::counts() const {
    Dictionary d;
    d["particles"] = int64_t(world_->particles.live().size());
    int64_t trails = 0, beams = 0;
    for (const auto &t : world_->trails.trails) trails += t.handle != 0;
    for (const auto &b : world_->beams.beams) beams += b.active;
    d["trails"] = trails;
    d["beams"] = beams;
    d["glows"] = int64_t(world_->glows.records.size());
    d["decals"] = int64_t(world_->decals.decals().size());
    d["lights"] = int64_t(world_->light_nodes().size());
    return d;
}

Dictionary ChampionsFxBridge::set_level_light(const Vector3 &dir, const Color &sun, const Color &ambient,
                                               int64_t base, double scale, int64_t layer) {
    if (base < 0 || base > 255 || layer < -128 || layer > 127 || dir.length_squared() == 0 || !std::isfinite(scale))
        return refusal("invalid level light");
    world_->level = L::Level::make(lv(dir), rgba(sun), rgba(ambient), int(base), scale, int(layer));
    Dictionary d;
    d["ok"] = true;
    return d;
}
void ChampionsFxBridge::clear_light_nodes() {
    for (std::uint64_t k : bridge_lights_) world_->unlink_light(k);
    bridge_lights_.clear();
}
void ChampionsFxBridge::add_light_node(const Vector3 &p, const Vector3 &m, const Vector3 &w, double mr, double wr,
                                       double floor, int64_t layer, bool tint) {
    // Bridge-made nodes use keys above the 32-bit address space of the effect classes.
    const std::uint64_t key = (std::uint64_t(1) << 40) + bridge_lights_.size();
    bridge_lights_.insert(key);
    world_->link_light(key, L::Node(lv(p), irgb(m), irgb(w), mr, wr, floor, int(layer), tint));
}
Dictionary ChampionsFxBridge::model_light(const Vector3 &p) const {
    const auto nodes = world_->light_nodes();
    const auto l = L::model_lights(lv(p), nodes, L::Object::reset(world_->level), world_->level.layer);
    Dictionary out;
    out["ambient"] = rgb(l.ambient);
    Array directions, colours;
    for (const auto &n : l.lights) {
        directions.push_back(gv(n.direction));
        colours.push_back(rgb(n.rgb));
    }
    out["directions"] = directions;
    out["colours"] = colours;
    return out;
}
Vector3 ChampionsFxBridge::shade_model(const Vector3 &p, const Vector3 &normal) const {
    const auto nodes = world_->light_nodes();
    const auto l = L::model_lights(lv(p), nodes, L::Object::reset(world_->level), world_->level.layer);
    return rgb(L::shade_model(lv(normal), l));
}
Vector3 ChampionsFxBridge::shade_world(const Vector3 &p, const Vector3 &normal) const {
    return rgb(L::shade_world(world_->level.base, world_->light_nodes(), lv(p), lv(normal)));
}
Vector3 ChampionsFxBridge::decal_tint(const Vector3 &p) const {
    return rgb(L::decal_tint(lv(p), world_->light_nodes(), world_->level));
}
