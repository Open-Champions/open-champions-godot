#include "fx_bridge.hpp"
#include "champions/archive.hpp"
#include "champions/fx_world_save.hpp"
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
Dictionary accepted() {
    Dictionary d;
    d["ok"] = true;
    return d;
}
// No C++ exception may cross the GDExtension boundary (godot-cpp does not turn it
// into a GDScript error; the process aborts). Every bound mutator runs inside this
// and answers {ok: false, refusal} instead (review finding 1).
// latch: an exception part-way through a world update (tick, draw) leaves the world
// part-updated; the reason is kept and every later mutation refuses with it.
template <class F> Dictionary guarded(F &&f, String *latch = nullptr) {
    if (latch && !latch->is_empty()) return refusal(String("effect world stopped after a refusal: ") + *latch);
    String why;
    try {
        return f();
    } catch (const std::exception &e) {
        why = String::utf8(e.what());
    } catch (...) {
        why = "unknown native exception";
    }
    if (latch) *latch = why;
    return refusal(why);
}
bool finite_value(double v) { return std::isfinite(v); }
bool finite_value(const Vector3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
// The particles of definitions the user's JSON does not have since `before`:
// counted by EffectWorld and reported, never guessed (review finding 5).
String missing_since(const S::EffectWorld &w, const std::map<std::uint32_t, std::int64_t> &before) {
    String out;
    for (const auto &kv : w.missing_by_definition) {
        const auto it = before.find(kv.first);
        const std::int64_t n = kv.second - (it == before.end() ? 0 : it->second);
        if (n <= 0) continue;
        if (!out.is_empty()) out += ", ";
        out += String("0x") + String::num_int64(kv.first, 16) + " x" + String::num_int64(n);
    }
    return out.is_empty() ? out : String("particle definitions missing from the user's JSON: ") + out;
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
    ClassDB::bind_method(D_METHOD("shade_world_many", "positions", "normals"), &ChampionsFxBridge::shade_world_many);
    ClassDB::bind_method(D_METHOD("decal_tint", "position"), &ChampionsFxBridge::decal_tint);
    ClassDB::bind_method(D_METHOD("game_camera", "view", "proj_scale"), &ChampionsFxBridge::game_camera);
    ClassDB::bind_method(D_METHOD("set_glow_texture_size", "name", "width", "height"),
                         &ChampionsFxBridge::set_glow_texture_size);
    ClassDB::bind_method(D_METHOD("save_state"), &ChampionsFxBridge::save_state);
    ClassDB::bind_method(D_METHOD("load_state", "bytes"), &ChampionsFxBridge::load_state);
}

Dictionary ChampionsFxBridge::game_camera(const PackedFloat32Array &view, double proj_scale) const {
    if (view.size() != 12 || !std::isfinite(proj_scale) || proj_scale <= 0)
        return refusal("game_camera needs view[12] and a positive proj_scale");
    core::Mat34 v{};
    for (int i = 0; i < 12; ++i) {
        if (!std::isfinite(view[i])) return refusal("game_camera needs a finite view");
        v[i] = raw(view[i]);
    }
    // Client derivation (outside the effect contract, docs/NATIVE_FX.md Godot): the
    // same mat4_set_projection inputs as fx_sim_tests' camera and the particle
    // projection (sx 0x3F7FFFFF, sy 0x3EEEEEEE, z 1.0 / 0x457FFF00, near 18, far 1800).
    const core::Mat4 vp = core::mat4_mul_mat34(
        v, core::mat4_set_projection(0x3F7FFFFFu, 0x3EEEEEEEu, raw(proj_scale), 0x3F800000u, 0x457FFF00u,
                                     0x41900000u, 0x44E10000u));
    PackedFloat32Array viewproj;
    for (const core::Raw r : vp) {
        float f;
        std::memcpy(&f, &r, sizeof f);
        viewproj.push_back(f);
    }
    Dictionary d;
    d["ok"] = true;
    d["view"] = view;
    d["camera"] = view;
    d["viewproj"] = viewproj;
    d["proj_scale"] = proj_scale;
    return d;
}

bool ChampionsFxBridge::set_glow_texture_size(const String &name, int64_t width, int64_t height) {
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return false;
    const std::string wanted = text(name);
    for (const S::GlowTexture &g : S::glow_textures())
        if (wanted == g.name) {
            world_->glow_dims[g.global] = {std::int32_t(width), std::int32_t(height)};
            return true;
        }
    return false;
}

Dictionary ChampionsFxBridge::load_definitions_json(const String &json) {
    return guarded([&] {
        std::string error;
        const bool ok = world_->load_definitions(text(json), error);
        Dictionary out;
        out["ok"] = ok;
        out["error"] = String::utf8(error.c_str());
        if (!ok) out["refusal"] = out["error"];
        return out;
    });
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

Dictionary ChampionsFxBridge::set_detail(int64_t detail) {
    // g_decalDetail: the three gore settings 0, 1, 2 (docs/effects.md 8, 16.2).
    if (detail < 0 || detail > 2) return refusal("gore detail must be 0, 1 or 2");
    world_->decals.detail = detail;
    return accepted();
}
void ChampionsFxBridge::set_wait_first(bool wait_first) {
    world_->waitq = wait_first ? R::WaitQ::WaitFirst : R::WaitQ::Vu1CppReading;
}

Dictionary ChampionsFxBridge::spawn_particle(int64_t id, const Vector3 &p, const Vector3 &v, double backdate) {
    if (!finite_value(p) || !finite_value(v) || !finite_value(backdate)) return refusal("spawn_particle needs finite position, velocity and backdate");
    return guarded([&] {
        const auto before = world_->missing_by_definition;
        world_->particle_spawn(std::uint32_t(id), rv(p), rv(v), raw(backdate));
        const String missing = missing_since(*world_, before);
        return missing.is_empty() ? accepted() : refusal(missing);
    }, &failed_);
}

Dictionary ChampionsFxBridge::fx_spawn(int64_t id, const Vector3 &p, bool near) {
    // Id 5 makes Fire objects from the shared seed: gameplay, run by the combat runtime's
    // EffectSimulation, never by a presentation bridge.
    if (id == 5) return refusal("fx_spawn id 5 is gameplay (shared seed, Fire objects)");
    if (!finite_value(p)) return refusal("fx_spawn needs a finite position");
    return guarded([&] {
        const auto before = world_->missing_by_definition;
        core::Lcg unused;
        const RC::Calls calls = RC::fx_spawn(std::int32_t(id), rv(p), world_->cosmetic, unused, near,
                                             std::int32_t(world_->decals.detail), &world_->made);
        world_->apply(calls);
        const String missing = missing_since(*world_, before);
        Dictionary d = missing.is_empty() ? accepted() : refusal(missing);
        d["calls"] = int64_t(calls.size());
        return d;
    }, &failed_);
}

Dictionary ChampionsFxBridge::add_emitter(int64_t id, const PackedByteArray &bytes) {
    if (id <= 0 || id > 0xffffffffll) return refusal("emitter definition id must be a positive 32-bit address");
    if (bytes.size() < 0x48) return refusal("emitter record needs 0x48 bytes");
    return guarded([&] {
        X::EmitterRecord rec = X::EmitterRecord::parse(bytes.ptr(), std::size_t(bytes.size()));
        if (!world_->add_definition(std::uint32_t(id), X::ParticleDef::from_emitter_record(rec)))
            return refusal("the definition has live particles; it cannot be replaced");
        const int handle = next_emitter_++;
        emitters_[handle] = {std::uint32_t(id), rec};
        Dictionary d = accepted();
        d["handle"] = int64_t(handle);
        return d;
    });   // parse/pack refusals come before any mutation: no latch
}

Dictionary ChampionsFxBridge::run_emitter(int64_t h, const Vector3 &p, int64_t frame, int64_t yaw, bool nx,
                                          bool ny, bool sw, int64_t weather_angle, bool weather_on) {
    const auto it = emitters_.find(int(h));
    if (it == emitters_.end()) return refusal("unknown emitter");
    if (!finite_value(p)) return refusal("run_emitter needs a finite position");
    return guarded([&] {
        // Work on copies so a refusal leaves the record and the cosmetic seed untouched.
        X::EmitterRecord rec = it->second.second;
        X::CosmeticRng rng(world_->cosmetic.state);   // emitters draw the cosmetic seed
        const auto emissions = X::emitter_run(rec, {double(p.x), double(p.y), double(p.z)}, frame, rng,
                                              yaw, nx, ny, sw, weather_angle, weather_on);
        for (const auto &e : emissions)
            for (double c : {e.pos[0], e.pos[1], e.pos[2], e.vel[0], e.vel[1], e.vel[2]})
                if (!std::isfinite(c)) return refusal("emitter produced a non-finite emission");
        it->second.second = rec;
        world_->cosmetic.state = rng.seed;
        const auto before = world_->missing_by_definition;
        for (const auto &e : emissions) world_->particle_spawn_unrounded(it->second.first, e.pos, e.vel);
        const String missing = missing_since(*world_, before);
        Dictionary d = missing.is_empty() ? accepted() : refusal(missing);
        d["count"] = int64_t(emissions.size());
        return d;
    }, &failed_);
}

Dictionary ChampionsFxBridge::add_glow(const Vector3 &p, const Color &colour, int64_t word, double bias, double size,
                                       int64_t angle, int64_t flags) {
    if (!finite_value(p) || !finite_value(bias) || !finite_value(size)) return refusal("add_glow needs finite position, bias and size");
    R::Glow g;
    g.pos = rv(p);
    const auto c = rgba(colour);
    g.rgba = {std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3])};
    g.tex = std::uint32_t(word);
    g.bias = raw(bias);
    g.size = raw(size);
    g.angle = std::uint16_t(angle);
    g.flags = std::int16_t(flags);
    return world_->glow_add(g) ? accepted() : refusal("glow queue full (0x400 a frame; the original drops it)");
}

Dictionary ChampionsFxBridge::create_trail(double seconds, int64_t row, int64_t width, int64_t flags, const Color &colour) {
    if (!finite_value(seconds)) return refusal("create_trail needs a finite lifetime");
    // trail_header refuses rows outside the documented table when the trail is drawn
    // (render.cpp); refuse at creation, before the trail exists.
    if (row < 0 || row > 8) return refusal("trail row must be 0 ... 8 (docs/effects.md 7)");
    const auto c = rgba(colour);
    return guarded([&] {
        const auto handle = world_->trails.create(raw(seconds), std::int32_t(row), std::int32_t(width), std::uint32_t(flags),
                                                  std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3]));
        if (!handle) return refusal("all 64 trails are in use");
        Dictionary d = accepted();
        d["handle"] = int64_t(handle);
        return d;
    }, &failed_);
}
Dictionary ChampionsFxBridge::add_trail_point(int64_t h, const Vector3 &p) {
    if (!finite_value(p)) return refusal("add_trail_point needs a finite position");
    return guarded([&] {
        if (!world_->trails.find(std::uint32_t(h))) return refusal("unknown trail");
        return world_->trails.add_point(std::uint32_t(h), rv(p)) ? accepted()
                                                                  : refusal("trail ring within two of full (trail_addPoint refuses)");
    }, &failed_);
}
void ChampionsFxBridge::release_trail(int64_t h) { world_->trails.release(std::uint32_t(h)); }

Dictionary ChampionsFxBridge::create_beam(double jitter, double scale, const Vector3 &from, const Vector3 &to,
                                          int64_t width, int64_t depth, const Color &colour) {
    if (!finite_value(jitter) || !finite_value(scale) || !finite_value(from) || !finite_value(to)) return refusal("create_beam needs finite values");
    if (depth < 0) return refusal("beam_create: negative depth (the node recursion would not end)");
    const auto c = rgba(colour);
    return guarded([&] {
        const auto handle = world_->beams.create(raw(jitter), raw(scale), rv(from), rv(to), std::int32_t(width),
                                                 std::int32_t(depth),
                                                 {std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3])},
                                                 sim_frame_, world_->in_sim_tick, world_->cosmetic, world_->libc);
        if (!handle) return refusal("no free beam (20) or trail (64)");
        Dictionary d = accepted();
        d["handle"] = int64_t(handle);
        return d;
    }, &failed_);
}
Dictionary ChampionsFxBridge::update_beam(int64_t h, const Vector3 &from, const Vector3 &to, int64_t alpha, int64_t width) {
    if (!finite_value(from) || !finite_value(to)) return refusal("update_beam needs finite ends");
    return guarded([&] {
        if (!world_->beams.is_alive(std::uint32_t(h))) return refusal("unknown or expired beam");
        world_->beams.update(std::uint32_t(h), rv(from), rv(to), std::int32_t(alpha), std::int32_t(width), sim_frame_);
        return accepted();
    }, &failed_);
}
Dictionary ChampionsFxBridge::add_decal(double size, const Vector3 &p, int64_t angle, int64_t type, int64_t grow) {
    if (!finite_value(size) || !finite_value(p)) return refusal("add_decal needs a finite size and position");
    return guarded([&] {
        const auto before = world_->decals.decals().size();
        world_->decal_add(raw(size), rv(p), std::int32_t(angle), std::int32_t(type), std::int32_t(grow));
        // The gate, the grid, the blocks or the cell may drop a decal: original
        // behaviour (docs/effects.md 16.2), reported, not refused.
        Dictionary d = accepted();
        d["added"] = world_->decals.decals().size() > before;
        return d;
    }, &failed_);
}

Dictionary ChampionsFxBridge::tick(int64_t sim_frame, bool cutscene) {
    return guarded([&] {
        sim_frame_ = std::int32_t(sim_frame);
        world_->tick(sim_frame_, cutscene);
        return accepted();
    }, &failed_);
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
    if (!failed_.is_empty()) return refusal(String("effect world stopped after a refusal: ") + failed_);
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
        failed_ = String::utf8(e.what());
        return refusal(failed_);
    } catch (...) {
        failed_ = "unknown native exception";
        return refusal(failed_);
    }
    const String missing = missing_since(*world_, {});
    if (!missing.is_empty()) refusals.push_back(missing);
    // A frame that carries refusals is not a successful present (review finding 6):
    // ok is false, the runs that could be made are still returned for drawing.
    Dictionary out;
    out["ok"] = refusals.is_empty();
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
    if (base < 0 || base > 255 || layer < -128 || layer > 127 || !finite_value(dir) || dir.length_squared() == 0 ||
        !std::isfinite(scale))
        return refusal("invalid level light");
    return guarded([&] {
        world_->level = L::Level::make(lv(dir), rgba(sun), rgba(ambient), int(base), scale, int(layer));
        return accepted();
    }, &failed_);
}
void ChampionsFxBridge::clear_light_nodes() {
    for (std::uint64_t k : bridge_lights_) world_->unlink_light(k);
    bridge_lights_.clear();
}
Dictionary ChampionsFxBridge::add_light_node(const Vector3 &p, const Vector3 &m, const Vector3 &w, double mr, double wr,
                                             double floor, int64_t layer, bool tint) {
    // Non-finite values would reach the lighting's float-to-int conversions (UB).
    if (!finite_value(p) || !finite_value(m) || !finite_value(w) || !finite_value(mr) || !finite_value(wr) || !finite_value(floor) || layer < -128 ||
        layer > 127)
        return refusal("add_light_node needs finite values and a layer in -128 ... 127");
    return guarded([&] {
        // Bridge-made nodes use keys above the 32-bit address space of the effect classes.
        const std::uint64_t key = (std::uint64_t(1) << 40) + bridge_lights_.size();
        world_->link_light(key, L::Node(lv(p), irgb(m), irgb(w), mr, wr, floor, int(layer), tint));
        bridge_lights_.insert(key);
        return accepted();
    }, &failed_);
}
// The lighting queries take a position from the scene: a non-finite one reaches
// lighting.cpp's static_cast<int> of NaN (undefined; UBSan, work/ubsan), so it is
// refused here. Results are {ok, rgb} / {ok, ambient, directions, colours}.
Dictionary ChampionsFxBridge::model_light(const Vector3 &p) const {
    if (!finite_value(p)) return refusal("model_light needs a finite position");
    return guarded([&] {
        const auto nodes = world_->light_nodes();
        const auto l = L::model_lights(lv(p), nodes, L::Object::reset(world_->level), world_->level.layer);
        Dictionary out = accepted();
        out["ambient"] = rgb(l.ambient);
        Array directions, colours;
        for (const auto &n : l.lights) {
            directions.push_back(gv(n.direction));
            colours.push_back(rgb(n.rgb));
        }
        out["directions"] = directions;
        out["colours"] = colours;
        return out;
    });
}
namespace {
Dictionary rgb_result(const Vector3 &v) {
    Dictionary d = accepted();
    d["rgb"] = v;
    return d;
}
} // namespace
Dictionary ChampionsFxBridge::shade_model(const Vector3 &p, const Vector3 &normal) const {
    if (!finite_value(p) || !finite_value(normal)) return refusal("shade_model needs a finite position and normal");
    return guarded([&] {
        const auto nodes = world_->light_nodes();
        const auto l = L::model_lights(lv(p), nodes, L::Object::reset(world_->level), world_->level.layer);
        return rgb_result(rgb(L::shade_model(lv(normal), l)));
    });
}
Dictionary ChampionsFxBridge::shade_world(const Vector3 &p, const Vector3 &normal) const {
    if (!finite_value(p) || !finite_value(normal)) return refusal("shade_world needs a finite position and normal");
    return guarded([&] { return rgb_result(rgb(L::shade_world(world_->level.base, world_->light_nodes(), lv(p), lv(normal)))); });
}
Dictionary ChampionsFxBridge::shade_world_many(const PackedVector3Array &positions, const PackedVector3Array &normals) const {
    if (positions.size() != normals.size()) return refusal("shade_world_many needs one normal per position");
    for (int64_t i = 0; i < positions.size(); ++i)
        if (!finite_value(positions[i]) || !finite_value(normals[i])) return refusal("shade_world needs a finite position and normal");
    return guarded([&] {
        const auto nodes = world_->light_nodes();
        PackedVector3Array out;
        out.resize(positions.size());
        Vector3 *w = out.ptrw();
        for (int64_t i = 0; i < positions.size(); ++i)
            w[i] = rgb(L::shade_world(world_->level.base, nodes, lv(positions[i]), lv(normals[i])));
        Dictionary d;
        d["ok"] = true;
        d["rgb"] = out;
        return d;
    });
}
Dictionary ChampionsFxBridge::decal_tint(const Vector3 &p) const {
    if (!finite_value(p)) return refusal("decal_tint needs a finite position");
    return guarded([&] { return rgb_result(rgb(L::decal_tint(lv(p), world_->light_nodes(), world_->level))); });
}

// ---- presentation state (fixture continuation) ------------------------------------------
namespace {
constexpr std::uint8_t BRIDGE_MAGIC[8] = {'O', 'C', 'F', 'X', 'B', 'R', 'G', 0};
constexpr std::uint32_t BRIDGE_VERSION = 1;
struct BridgeState {
    S::SaveBytes world;
    std::map<int, std::pair<std::uint32_t, X::EmitterRecord>> emitters;
    std::set<std::uint64_t> lights;
    int next_emitter = 1;
    std::int32_t sim_frame = 0;
    std::string failed;
};
void put(std::vector<std::uint8_t> &b, std::uint64_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i) b.push_back(std::uint8_t(v >> (8 * i)));
}
BridgeState decode_state(const std::vector<std::uint8_t> &b) {
    std::size_t at = 0;
    auto u = [&](unsigned n) {
        if (n > b.size() - at) throw std::runtime_error("truncated effect bridge state");
        std::uint64_t v = 0;
        for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(b[at++]) << (8 * i);
        return v;
    };
    auto bytes = [&](std::size_t n) {
        if (n > b.size() - at) throw std::runtime_error("truncated effect bridge state");
        std::vector<std::uint8_t> out(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(at + n));
        at += n;
        return out;
    };
    for (auto c : BRIDGE_MAGIC)
        if (u(1) != c) throw std::runtime_error("not an effect bridge state");
    if (u(4) != BRIDGE_VERSION) throw std::runtime_error("unsupported effect bridge state version");
    BridgeState st;
    st.world = bytes(std::size_t(u(4)));
    for (std::uint64_t n = u(4), k = 0; k < n; ++k) {
        const int handle = int(std::int32_t(u(4)));
        const auto def = std::uint32_t(u(4));
        const auto rec = bytes(0x48);
        const X::EmitterRecord parsed = X::EmitterRecord::parse(rec.data(), rec.size());
        const auto canonical = parsed.pack();
        if (!std::equal(rec.begin(), rec.end(), canonical.begin()))
            throw std::runtime_error("noncanonical emitter record");
        if (!st.emitters.emplace(handle, std::make_pair(def, parsed)).second)
            throw std::runtime_error("duplicate emitter handle");
    }
    for (std::uint64_t n = u(4), k = 0; k < n; ++k) st.lights.insert(u(8));
    st.next_emitter = int(std::int32_t(u(4)));
    st.sim_frame = std::int32_t(u(4));
    const auto text = bytes(std::size_t(u(4)));
    st.failed.assign(text.begin(), text.end());
    if (at != b.size()) throw std::runtime_error("trailing bytes after the effect bridge state");
    return st;
}
} // namespace

std::vector<std::uint8_t> ChampionsFxBridge::state_bytes() const {
    std::vector<std::uint8_t> b(std::begin(BRIDGE_MAGIC), std::end(BRIDGE_MAGIC));
    put(b, BRIDGE_VERSION, 4);
    const S::SaveBytes w = S::save_world(*world_);
    put(b, w.size(), 4);
    b.insert(b.end(), w.begin(), w.end());
    put(b, emitters_.size(), 4);
    for (const auto &kv : emitters_) {
        put(b, std::uint32_t(std::int32_t(kv.first)), 4);
        put(b, kv.second.first, 4);
        const auto rec = kv.second.second.pack();   // refuses a field outside its range
        b.insert(b.end(), rec.begin(), rec.end());
    }
    put(b, bridge_lights_.size(), 4);
    for (auto k : bridge_lights_) put(b, k, 8);
    put(b, std::uint32_t(std::int32_t(next_emitter_)), 4);
    put(b, std::uint32_t(sim_frame_), 4);
    const std::string failed = text(failed_);
    put(b, failed.size(), 4);
    b.insert(b.end(), failed.begin(), failed.end());
    return b;
}
void ChampionsFxBridge::validate_state(const std::vector<std::uint8_t> &bytes) {
    const BridgeState st = decode_state(bytes);
    S::EffectWorld scratch;
    S::load_world(scratch, st.world);
}
void ChampionsFxBridge::restore_state(const std::vector<std::uint8_t> &bytes) {
    validate_state(bytes);
    BridgeState st = decode_state(bytes);
    S::load_world(*world_, st.world);
    emitters_ = std::move(st.emitters);
    bridge_lights_ = std::move(st.lights);
    next_emitter_ = st.next_emitter;
    sim_frame_ = st.sim_frame;
    failed_ = String::utf8(st.failed.c_str());
}
PackedByteArray ChampionsFxBridge::save_state() const {
    PackedByteArray out;
    try {
        const auto b = state_bytes();
        out.resize(int64_t(b.size()));
        std::memcpy(out.ptrw(), b.data(), b.size());
    } catch (const std::exception &) {
        out.clear();   // an empty array is the refusal (load_state names the reason)
    }
    return out;
}
Dictionary ChampionsFxBridge::load_state(const PackedByteArray &bytes) {
    return guarded([&] {
        restore_state(std::vector<std::uint8_t>(bytes.ptr(), bytes.ptr() + bytes.size()));
        return accepted();
    });
}
