// Synthetic checks of the native-fx-2 wiring (fx_simulation.hpp).  The parts themselves are
// checked bit for bit against their references by the fx-exact, fx-render, fx-recipes and
// fx-skills tests; here: that the simulation half makes the same gameplay draws with or
// without a renderer, routes damage and objects to the game, refuses missing answers, and
// that the world draws what the primitives were given, in GS order.  No game data.
#include "champions/fx_simulation.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace sim = champions::rta::fx::sim;
namespace R = champions::rta::fx::render;
namespace S = champions::rta::fx::skills;
namespace RC = champions::rta::fx::recipes;
namespace core = champions::simulation::rta;
using core::Raw;

namespace {
int failures = 0, checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << what << '\n';
  }
}
Raw F(double d) {
  const float f = static_cast<float>(d);
  Raw r;
  std::memcpy(&r, &f, 4);
  return r;
}
std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
const R::Mat34 IDENT = {F(1), 0, 0, 0, F(1), 0, 0, 0, F(1), 0, 0, 0};
sim::Camera camera() {
  sim::Camera c;
  c.view = IDENT;
  c.proj_scale = F(477.5);
  c.camera = IDENT;
  c.viewproj = core::mat4_mul_mat34(IDENT, core::mat4_set_projection(0x3F7FFFFF, 0x3EEEEEEE, F(477.5),
                                                                     core::ONE, 0x457FFF00, 0x41900000, 0x44E10000));
  return c;
}
// fx_particle_defs.py --json rows with made-up values.
std::string defs_json(const std::vector<std::uint32_t> &addresses, int kind = 0) {
  std::string out = "[";
  int site = 1;
  for (std::uint32_t a : addresses) {
    if (site > 1) out += ",";
    out += "{\"site\":" + std::to_string(site++) + ",\"call\":\"particleDef_ctor\",\"def\":" + std::to_string(a) +
           ",\"complete\":true,\"size_start\":2,\"size_end\":2,\"gravity\":0,\"start_rgba\":[100,90,80,128]"
           ",\"end_rgba\":[100,90,80,128],\"life_class\":2,\"kind\":" + std::to_string(kind) +
           ",\"tile\":17,\"callback\":0}";
  }
  return out + "]";
}

std::shared_ptr<const S::RecipeConstants> constants(const std::string &dir) {
  return std::make_shared<const S::RecipeConstants>(
      S::RecipeConstants::from_json(read(dir + "/../fixtures/fx_skill_recipes_synthetic.json")));
}

// ---- fx_spawn id 5 and the ice shatter: shared draws with and without a world -----------
void test_shared_recipes(const std::string &dir) {
  const core::Vec3 pos{F(100), F(-40), F(12)};
  core::Lcg direct_shared(0x1234567), direct_cos(0);
  RC::MadeFlags made;
  std::uint32_t next_block = 0x7000;
  const RC::Calls want = RC::fx_spawn(5, pos, direct_cos, direct_shared, true, 2, &made,
                                      [&](std::uint32_t) { return next_block += 0x50; });
  for (int attach = 0; attach < 2; ++attach) {
    core::Lcg shared(0x1234567);
    sim::EffectSimulation s(shared, constants(dir));
    sim::EffectWorld world;
    if (attach) s.attach(&world);
    std::uint32_t block = 0x7000;
    std::vector<std::uint32_t> fires;
    s.hooks.pool_new = [&](std::uint32_t size) {
      check(size == 0x50, "Fire block size");
      return block += 0x50;
    };
    s.hooks.construct = [&](std::uint32_t b, const RC::Call &c) {
      check(c.kind == RC::Kind::Fire, "id 5 constructs Fire objects");
      fires.push_back(b);
    };
    const RC::Calls got = s.fx_spawn(5, pos, false, 0);
    check(RC::stream_text(got) == RC::stream_text(want), "id 5 call stream (attach " + std::to_string(attach) + ")");
    check(shared.state == direct_shared.state, "id 5 shared seed (attach " + std::to_string(attach) + ")");
    check(!fires.empty() && fires.front() == 0x7050, "Fire objects get their pool blocks");
    // A cosmetic id never touches the shared seed; headless it does nothing at all.
    const std::uint32_t before = shared.state;
    const RC::Calls dust = s.fx_spawn(2, pos, true, 2);
    check(shared.state == before, "id 2 leaves the shared seed");
    check(attach ? !dust.empty() : dust.empty(), "id 2 runs only with a world");
  }
  // Missing pool hook for a gameplay object: refused, not invented.
  core::Lcg shared(1);
  sim::EffectSimulation s(shared, constants(dir));
  bool refused = false;
  try {
    s.fx_spawn(5, pos, true, 2);
  } catch (const sim::Refusal &) {
    refused = true;
  }
  check(refused, "id 5 without GameHooks::pool_new refuses");
}

struct OneModel : RC::SampleModel {
  std::int32_t sample_count() override { return 7; }
  RC::SamplePoint sample_point(Raw, std::uint32_t, const core::Vec3 &pos, std::int32_t, std::int32_t i) override {
    return {{core::add(pos[0], F(i)), pos[1], pos[2]}, F(3)};
  }
};

void test_ice(const std::string &dir) {
  OneModel m;
  const core::Vec3 pos{F(10), F(20), F(30)};
  core::Lcg direct(99);
  const RC::Calls want = RC::shatter_to_loose_ice(m, pos, 0x100, F(3), F(1), 0, 0, direct,
                                                  [](std::uint32_t) { return 0x9000u; });
  core::Lcg shared(99);
  sim::EffectSimulation s(shared, constants(dir));     // headless
  int chunks = 0;
  s.hooks.pool_new = [](std::uint32_t size) { return size == 0x248 ? 0x9000u : 0u; };
  s.hooks.construct = [&](std::uint32_t b, const RC::Call &c) {
    check(c.kind == RC::Kind::PhProp && b == 0x9000, "ice chunk constructor");
    ++chunks;
  };
  const RC::Calls got = s.shatter_to_loose_ice(m, pos, 0x100, F(3), F(1), 0, 0);
  check(RC::stream_text(got) == RC::stream_text(want), "ice shatter call stream headless");
  check(shared.state == direct.state && chunks == 3, "ice shatter: 1 + n shared draws, n chunks");
}

// ---- DiseaseCloud: own-generator draws and damage with no renderer -----------------------
void setup_cloud(S::Model &m, std::int64_t obj, std::int64_t victim) {
  m.mem.poke(0x44FDCC, 4, 60);              // g_simFrame: a multiple of 30 (damage frame)
  m.mem.poke(obj + 0x10, 4, F(500));
  m.mem.poke(obj + 0x14, 4, F(500));
  m.mem.poke(obj + 0x18, 4, F(20));
  m.mem.poke(obj + 0x44, 4, 0xC0FFEE);      // the cloud's own generator
  m.mem.poke(obj + 0x4C, 4, 0x6000);        // owner node
  m.mem.poke(0x6000 + 0xC, 4, 0x6100);      // the owner
  m.mem.poke(obj + 0x54, 4, 200);           // end frame
  m.mem.poke(obj + 0x58, 4, 30);            // radius target (int): 3 particles per stream
  m.mem.poke(obj + 0x5C, 4, F(10));
  m.mem.poke(obj + 0x60, 4, 7);             // disease strength
  m.mem.poke(victim + 0xC, 4, 0);
}

void test_disease_cloud(const std::string &dir) {
  const std::int64_t obj = 0x5000, victim = 0x5800;
  std::uint32_t seeds[2] = {};
  std::vector<S::Callout> damage[2];
  const auto k = constants(dir);
  const Raw def0 = k->needed(0x2CE6E0, "particle", 0, "a0");
  for (int attach = 0; attach < 2; ++attach) {
    core::Lcg shared(5);
    sim::EffectSimulation s(shared, k);
    sim::EffectWorld world;
    std::string error;
    check(world.load_definitions(defs_json({def0, k->needed(0x2CE6E0, "particle", 1, "a0"),
                                            k->needed(0x2CE6E0, "particle", 2, "a0")}), error),
          "synthetic definitions load: " + error);
    if (attach) s.attach(&world);
    s.hooks.callout = [&](const S::Callout &c) -> std::optional<S::Value> {
      if (c.name == "world_collectObjectsInBox") return S::Value::list({S::Value::integer(victim)});
      if (c.name == "damage_apply") {
        damage[attach].push_back(c);
        return S::Value::integer(0);
      }
      return std::nullopt;
    };
    setup_cloud(s.classes(), obj, victim);
    s.classes().diseasecloud_run(obj);
    seeds[attach] = s.classes().u32(obj + 0x44);
    if (attach) check(world.particles.live().size() == 9, "three streams of 30 / 10 particles reach the world");
  }
  check(seeds[0] == seeds[1] && seeds[0] != 0xC0FFEE, "own generator advances the same headless and drawn");
  check(damage[0].size() == 1 && damage[1].size() == 1, "damage_apply every 30th frame, with or without a world");
  check(!damage[0].empty() && damage[0][0].args.size() > 1 && damage[0][0].args[1].is_int() &&
            damage[0][0].args[1].i == 0, "DiseaseCloud damage has no attacker");
  // Without a game answer the damage is refused, never dropped.
  core::Lcg shared(5);
  sim::EffectSimulation s(shared, k);
  setup_cloud(s.classes(), obj, victim);
  bool refused = false;
  try {
    s.classes().diseasecloud_run(obj);
  } catch (const sim::Refusal &) {
    refused = true;
  }
  check(refused, "gameplay call-out without GameHooks::callout refuses");
}

// ---- the world: draw order, groups, lines, presentation ---------------------------------------
void test_world() {
  sim::EffectWorld w;
  std::string error;
  check(w.load_definitions(defs_json({0x111110, 0x222220}), error), "defs: " + error);
  check(w.load_definitions(defs_json({0x111110, 0x222220}), error), "defs again");
  const core::Vec3 at{0, 0, F(200)};
  w.particle_spawn(0x111110, at, {0, 0, 0}, 0);
  w.particle_spawn(0x999990, at, {0, 0, 0}, 0);
  check(w.missing_definitions == 1, "a definition missing from the user's JSON is counted");
  check(!w.load_definitions(defs_json({0x111110}), error), "no reload while definitions are resident");
  // A group translated by +20 in x draws its particle 20 units to the side.
  const std::uint32_t g = w.group_create();
  R::Mat34 m = IDENT;
  m[9] = F(20);
  w.group_set_matrix(g, m);
  w.group_begin(g);
  w.particle_spawn(0x111110, at, {0, 0, 0}, 0);
  w.group_end(g);
  w.glow_dims = {{0x4F848C, {32, 32}}, {0x44FE04, {32, 32}}};
  R::Glow pass1;
  pass1.pos = {0, 0, F(400)};
  pass1.size = F(10);
  pass1.rgba = {0x80, 0x80, 0x80, 0x80};
  pass1.tex = 0x4F848C;
  pass1.flags = 4 | 2;
  R::Glow pass0 = pass1;
  pass0.tex = 0x44FE04;
  pass0.flags = 0;
  w.glow_add(pass1);   // called first ...
  w.glow_add(pass0);
  const auto packets = w.draw(camera(), 1, 1);
  std::vector<int> keys;
  for (const auto &p : packets) keys.push_back(p.key * 10 + static_cast<int>(p.kind));
  check(packets.size() == 4 && packets[0].key == 3 && packets[0].kind == R::PacketKind::Glow &&
            packets[1].kind == R::PacketKind::Particles && packets[2].kind == R::PacketKind::Glow &&
            packets[3].kind == R::PacketKind::Trails,
        "GS order: glow pass 0 (key 3), particles, glow pass 1, trails");
  const auto &draws = packets[1].particles.draws;
  int drawn_global = -1, drawn_group = -1;
  for (std::size_t i = 0; i < draws.size(); ++i)
    if (draws[i].slot != 0) (draws[i].set == 0 ? drawn_global : drawn_group) = static_cast<int>(i);
  check(drawn_global >= 0 && drawn_group >= 0, "global and group particles sent");
  if (drawn_global >= 0 && drawn_group >= 0)
    check(draws[static_cast<std::size_t>(drawn_group)].v[0].x > draws[static_cast<std::size_t>(drawn_global)].v[0].x,
          "the group matrix moves its particles");
  const sim::Presentation pr = sim::present(packets, w.glow_dims);
  check(pr.runs.size() == 3 && pr.runs[0].texture == "halo1.tex" && pr.runs[1].texture == sim::PARTICLE_SHEET &&
            pr.runs[2].texture == "halo2.tex" && pr.runs[2].alpha1 == 0x48,
        "presentation runs in GS order with their textures and blends");
  check(pr.runs.size() > 1 && pr.runs[1].vertices.size() == 12, "two sprites, two triangles each");
  if (pr.runs.size() == 3) {
    const auto &v = pr.runs[0].vertices;
    float cx = 0, cy = 0;
    for (const auto &p : v) { cx += p[0]; cy += p[1]; }
    check(std::fabs(cx / v.size()) < 1e-3 && std::fabs(cy / v.size()) < 1e-3, "a glow on the axis is at the centre");
    check(std::fabs(v[0][2] - 400.0f) < 1e-3, "glow depth for the depth test");
  }
  // A decal: tinted by the light at its position, drawn through the camera.
  w.decal_add(F(10), {0, 0, F(197)}, 0, 9, 0);
  const sim::Presentation dp = sim::present_decals(w.decals, camera());
  check(dp.runs.size() == 1 && dp.runs[0].texture == sim::FLOOR_TEXTURE && dp.runs[0].vertices.size() == 6,
        "a floor decal is one quad of floorsprite.tex");
  if (!dp.runs.empty()) check(std::fabs(dp.runs[0].vertices[0][2] - 200.0f) < 1e-3, "decal z + 3, depth 200");
  // An unknown group matrix (the reference's 'stack') is not drawn.
  w.group_set_unknown_matrix(g);
  const auto later = w.draw(camera(), 2, 2);
  bool group_sent = false;
  for (const auto &p : later)
    for (const auto &d : p.particles.draws) group_sent = group_sent || d.set != 0;
  check(!group_sent, "a group whose matrix is unknown is not drawn");
}

void test_lines() {
  sim::EffectWorld w;
  std::string error;
  check(w.load_definitions(defs_json({0x444440}, 2), error), "line defs: " + error);
  w.particle_spawn(0x444440, {0, 0, F(200)}, {F(30), 0, 0}, 0);
  for (int t = 1; t <= 5; ++t) w.tick(t);
  const auto packets = w.draw(camera(), 5, 5);
  const sim::Presentation pr = sim::present(packets, w.glow_dims);
  bool found = false;
  for (const auto &r : pr.runs)
    if (r.lines && r.colors.size() == 2) {
      found = true;
      check(r.colors[0][3] == 128 && r.colors[1][3] == 0, "kind-2 line: head alpha, tail alpha 0");
      check(r.alpha1 == 0x48 && r.texture.empty(), "kind-2 line: untextured, ALPHA_1 0x48");
    }
  check(found, "kind-2 particle drawn as a line");
}

void test_light_effect_and_beams() {
  sim::EffectWorld w;
  core::Lcg shared(3);
  // A LightEffect call-out (the shared draw is the model's): colour fractions kept.
  S::Callout c{"LightEffect_ctor",
               {S::Value::integer(0x8000), S::Value::vec({F(1), F(2), F(3)}), S::Value::integer(0x77),
                S::Value::integer(0), S::Value::integer(F(0)), S::Value::integer(F(0)), S::Value::integer(F(0)),
                S::Value::integer(F(0)), S::Value::integer(F(60)), S::Value::integer(F(0)), S::Value::integer(F(0)),
                S::Value::integer(F(0)), S::Value::integer(F(100)), S::Value::integer(F(100)),
                S::Value::integer(F(0)), S::Value::integer(F(1))}};
  const auto fx = S::to_primitive(c);
  check(fx && fx->primitive == "light_effect", "LightEffect_ctor decodes");
  if (fx) w.apply(*fx);
  w.tick(1, false, false, {{0x77, {F(10), F(20), F(30)}}});
  const auto nodes = w.light_nodes();
  check(nodes.size() == 1 && nodes[0].model_rgb[0] == 0, "60 over 60 ticks: 0.99999994 truncates to 0 after one tick");
  w.tick(2, false, false, {{0x77, {F(10), F(20), F(30)}}});
  const auto later = w.light_nodes();
  check(later.size() == 1 && later[0].model_rgb[0] == 1, "fractions kept: 1 after two ticks");
  check(later.size() == 1 && std::fabs(later[0].pos.z - 66.0) < 1e-9, "follows the owner + 36");
  // Beams: the shape is rebuilt from the frame seed and the cosmetic seed put back.
  const auto h = w.beams.create(F(4), F(2), {0, 0, F(200)}, {F(50), 0, F(200)}, 0x40, 3,
                                {0x80, 0x80, 0x80, 0x80}, 7, false, w.cosmetic, w.libc);
  check(h != 0, "beam created");
  const std::uint32_t seed = w.cosmetic.state;
  w.draw(camera(), 7, 7, true);
  check(w.cosmetic.state == seed, "frozen draw: the frame seed is restored, no drift draws");
  auto rings = [&w]() {
    std::vector<R::TrailPoint> out;
    for (const auto &t : w.trails.trails)
      if (t.handle) out.insert(out.end(), t.points.begin(), t.points.begin() + t.newest);
    return out;
  };
  const std::vector<R::TrailPoint> first = rings();
  w.draw(camera(), 7, 7, true);
  w.draw(camera(), 7, 7, true);
  const std::vector<R::TrailPoint> again = rings();
  check(!first.empty() && first == again, "same simulation frame, same beam shape");
}

struct Blade : RC::WeaponModel {
  std::int32_t sample_count() override { return 4; }
  RC::SamplePoint transform_sample_point(const core::Mat34 &m, std::int32_t i) override {
    return {core::mat34_transform_point(m, {F(i), F(2 * i), F(10)}), F(2)};
  }
  std::array<core::Vec3, 2> effect_segment() override { return {{{0, F(4.5), 0}, {0, F(38), 0}}}; }
  std::uint32_t attach_base() override { return 0; }
};

void test_weapon_status(const std::string &dir) {
  core::Lcg shared(1);
  sim::EffectSimulation s(shared, constants(dir));
  sim::EffectWorld w;
  s.attach(&w);
  w.tick(10);
  RC::WeaponItem item;
  item.entries[0] = {0x18, 1};   // lightning
  RC::WeaponFxState state;
  Blade blade;
  const core::Vec3 pos{F(5), F(5), F(200)};
  s.model_status_effects(F(1), F(1), item, &blade, state, pos, 0x1000, nullptr, 0, true, 0, nullptr, 0, false);
  const R::Trail *t = w.trails.find(state.trail);
  check(state.trail != 0 && t != nullptr, "the swing trail's handle is the world's");
  check(t && t->newest == 2, "the swing pair reached the world's trail");
  check(state.beam != 0 && w.beams.is_alive(state.beam), "the lightning beam's handle is the world's");
  w.tick(11);
  const std::uint32_t beam = state.beam;
  s.model_status_effects(F(1), F(1), item, &blade, state, pos, 0x1100, nullptr, 0, true, 0, nullptr, 0, false);
  check(state.beam == beam && w.beams.is_alive(beam), "a live beam is updated, not made again");
  sim::EffectSimulation headless(shared, constants(dir));
  RC::WeaponFxState other;
  check(headless.model_status_effects(F(1), F(1), item, &blade, other, pos, 0, nullptr, 0, true, 0, nullptr, 0,
                                      false).empty() && other.trail == 0,
        "headless: weapon visuals are not run");
}

void test_present_mapping() {
  // A hand-made particle draw: the GS centre (0x8000) maps to (0, 0).
  R::Packet p;
  p.kind = R::PacketKind::Particles;
  R::ParticleDraw d;
  d.slot = 3;
  d.prim = R::PRIM_SPRITE;
  d.alpha1 = 0x44;
  d.v[0].x = 0x8000 - 16 * 64;
  d.v[0].y = 0x8000 - 16 * 32;
  d.v[1].x = 0x8000 + 16 * 64;
  d.v[1].y = 0x8000 + 16 * 32;
  d.v[0].u = d.v[0].v = 16;
  d.v[1].u = 16 * 31;
  d.v[1].v = 16 * 31;
  d.v[1].clip_w = F(50);
  p.particles.draws.push_back(d);
  R::ParticleDraw low = d;
  low.v[0].v = 16 * 161;
  low.v[1].v = 16 * 191;
  p.particles.draws.push_back(low);   // tile row 5: below the sheet
  const auto pr = sim::present({p}, {});
  check(pr.runs.size() == 1 && pr.runs[0].vertices.size() == 6, "one sprite drawn, the row-5 tile refused");
  check(pr.refusals.size() == 1, "the row-5 tile is reported");
  if (!pr.runs.empty()) {
    const auto &v = pr.runs[0].vertices;
    check(std::fabs(v[0][0] + 0.1f) < 1e-6 && std::fabs(v[0][1] - 32.0f / 224.0f) < 1e-6, "1280 x 448 frame, +y up");
    check(std::fabs(v[0][2] - 50.0f) < 1e-6, "depth from clip w");
    check(std::fabs(pr.runs[0].uvs[2][1] - 31.0f / 160.0f) < 1e-6, "UV over the 512 x 160 sheet");
  }
}
} // namespace

int main(int argc, char **argv) {
  const std::string dir = argc > 1 ? argv[1] : "native/tests/vectors";
  try {
    test_shared_recipes(dir);
    test_ice(dir);
    test_disease_cloud(dir);
    test_world();
    test_lines();
    test_light_effect_and_beams();
    test_weapon_status(dir);
    test_present_mapping();
  } catch (const std::exception &e) {
    std::cerr << "FAIL: exception " << e.what() << '\n';
    return 1;
  }
  std::cout << "fx-sim: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
