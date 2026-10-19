// Playable integration wave 11: the Full combat fixture with native-fx-2's
// EffectSimulation in the session's ready tick. Synthetic fixtures only.
//  (a) the stick walks the player, attacks chain into a combo, a Skelight takes
//      the hits and dies, and a held block stops a Skelight's swing;
//  (b) a Holy Strike cast makes its effects and lands its damage on the tick the
//      documented flight gives;
//  (c) headless and drawing runs of one scenario keep byte-identical seeds/HP;
//  (d) effect-class damage and box queries reach the combat World through
//      GameHooks; effect constructors meet the combat runtime's refusals.
#include "champions/combat_play_effects.hpp"
#include "combat_runtime_internal.hpp"
#include <cmath>
#include <cstring>
#include <set>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace champions::simulation::rta;
namespace C = combat_runtime;
namespace FX = champions::rta::fx;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << what << '\n';
  }
}
std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}
std::shared_ptr<const FX::skills::RecipeConstants> constants;
world_queries::Snapshot terrain() { return session_control::Controller().setup().initial_terrain; }
C::Address actor(const combat_play::Session &s, unsigned slot) {
  return s.world().target(s.actors()[slot].handle);
}
bool has(const combat_play::Session &s, const std::string &name) {
  for (const auto &e : s.tick_events())
    if (e.name == name) return true;
  return false;
}
std::size_t count(const combat_play::Session &s, const std::string &name, std::size_t args) {
  std::size_t n = 0;
  for (const auto &e : s.tick_events())
    if (e.name == name && e.args.size() == args) ++n;
  return n;
}
bool damaged(const combat_play::Session &s, C::Address victim) {
  for (const auto &e : s.tick_events())
    if (e.name == "damage_result" && e.args.size() == 2 && e.args[0] == victim && e.args[1]) return true;
  return false;
}
void advance(combat_play::Session &s, std::uint32_t command = 0) {
  auto o = s.advance(command);
  if (o.failed) throw std::runtime_error("session failed: " + o.error);
}
// fx_particle_defs.py --json rows with made-up values for the definition keys
// the Full fixture reaches (Holy Strike's three puffs, fx_spawn id 0's burst).
std::string definitions() {
  std::string out = "[";
  int site = 1;
  for (std::uint32_t a : {0x8e61d0u, 0x8e6220u, 0x8e6270u, 0x8704a8u}) {
    if (site > 1) out += ",";
    out += "{\"site\":" + std::to_string(site++) + ",\"call\":\"particleDef_ctor\",\"def\":" + std::to_string(a) +
           ",\"complete\":true,\"size_start\":4,\"size_end\":2,\"gravity\":0,\"start_rgba\":[120,110,60,128]"
           ",\"end_rgba\":[60,50,20,0],\"life_class\":2,\"kind\":0,\"tile\":17,\"callback\":0}";
  }
  return out + "]";
}
// Authored glow texture sizes (the bridge reads them from the user's files).
void synthetic_glow_dims(FX::sim::EffectWorld &world) {
  for (const auto &g : FX::sim::glow_textures()) world.glow_dims[g.global] = {32, 32};
}
Raw F(float f) {
  Raw r;
  std::memcpy(&r, &f, 4);
  return r;
}
FX::sim::Camera camera() {
  // Looking along native +x from (-200, 0, 40): camera x right = native -y,
  // y down = native -z, z forward = native +x (row-vector view mat34).
  FX::sim::Camera c;
  c.view = {0, 0, F(1), F(-1), 0, 0, 0, F(-1), 0, 0, F(40), F(200)};
  c.proj_scale = F(480.0f);
  c.camera = c.view;
  // fx_sim_tests' projection fixture, through this view.
  c.viewproj = mat4_mul_mat34(c.view, mat4_set_projection(0x3F7FFFFF, 0x3EEEEEEE, F(480.0f), ONE, 0x457FFF00,
                                                          0x41900000, 0x44E10000));
  return c;
}

// ---- (a) walk, attack, combo, creature hit and death, block --------------------------------
void walk_and_combo() {
  combat_play::Session s(terrain(), combat_play::Fixture::Full);
  combat_play::Effects fx(s, constants);
  check(s.full() && s.world().composition() == C::Composition::Full, "Full composition selected");
  advance(s); // first network submission: no ready tick
  check(s.observation().ready_ticks == 0, "no-ready first submission");
  const auto p = actor(s, 0), a = actor(s, 1);
  const Raw x0 = Raw(s.world().read(p + 16));
  for (int t = 0; t < 40; ++t) advance(s, session_control::POS_X);
  const Raw x1 = Raw(s.world().read(p + 16));
  check(lt(add(x0, from_int(40)), x1), "the stick walks the player along +x (Player_move)");
  const Raw hp0 = Raw(s.world().read(a + 0x228));
  int swings = 0, hits = 0, deleted_at = -1, destroyed_at = -1, killed_at = -1;
  unsigned max_step = 0;
  std::set<int> variants;
  Raw first_start = 0, first_hit_frame = 0;
  for (int t = 0; t < 240 && destroyed_at < 0; ++t) {
    if (t % 12 == 0) s.press_attack(true);
    const auto start_before = s.world().read(p + 0x448);
    advance(s);
    if (s.world().read(p + 0x448) != start_before) {
      ++swings;
      if (!first_start) first_start = Raw(s.world().read(p + 0x448));
    }
    max_step = std::max<unsigned>(max_step, unsigned(s.world().read(p + 0x460, 1)));
    variants.insert(int(std::int8_t(s.world().read(p + 0x461, 1))));
    if (damaged(s, a)) {
      ++hits;
      if (!first_hit_frame) first_hit_frame = Raw(s.world().read(C::FRAME)) - 1;
      if (killed_at < 0 && !lt(0, Raw(s.world().read(a + 0x228)))) killed_at = t;
    }
    if (has(s, "obj_delete")) deleted_at = t;
    if (has(s, "destroy")) destroyed_at = t;
  }
  variants.erase(0);
  // combat-player-melee.md 3: hit on frame start + 16 at speed 1; the combo step
  // counts up by one on every next swing; research WalkAndCombo: >= 2 variants,
  // deletion in the creature's own run the next tick, destruction at the next flush.
  check(swings >= 2 && max_step == unsigned(swings - 1),
        "repeated presses chain swings; P+0x460 counts up by one per next swing: " + std::to_string(swings) + "/" +
            std::to_string(max_step));
  check(variants.size() >= 2, "Player_pickAttackAnim chose at least two swing variants");
  check(first_hit_frame == first_start + 16, "first armed hit on frame start + 16 (speed 1, combat-player-melee.md 3)");
  check(hits >= 2 && lt(Raw(s.world().read(a + 0x228)), hp0), "the Skelight takes the combo's hits");
  check(killed_at >= 0 && deleted_at == killed_at + 1 && destroyed_at == killed_at + 2,
        "the killed Skelight queues obj_delete in its own run next tick and is destroyed at the next flush");
  check(!s.actors()[1].present, "the dead Skelight is gone from the actors");
  check(s.actors()[2].present, "the second Skelight remains");
  std::cout << "walk/combo: x " << x0 << "->" << x1 << " swings " << swings << " step " << max_step
            << " first swing " << first_start << " hit " << first_hit_frame << " seed " << s.shared_seed() << '\n';

  // Block: a fresh fixture, turn toward the Skelight behind and hold block.
  combat_play::Session b(terrain(), combat_play::Fixture::Full);
  combat_play::Effects bfx(b, constants);
  advance(b);
  for (int t = 0; t < 3; ++t) advance(b, session_control::NEG_X);
  b.set_block(true);
  int blocked = 0, sparks = 0;
  const Raw php = b.observation().hp;
  for (int t = 0; t < 60; ++t) {
    advance(b);
    if (b.world().read(actor(b, 0) + 0x58c) == 8 && has(b, "presentation:sound_playAtPos") &&
        damaged(b, actor(b, 0))) {
      ++blocked;
      sparks += int(count(b, "presentation:particle_spawn", 6));
    }
  }
  check(blocked >= 1 && sparks >= 50, "a front block (state 8) meets the Skelight's swing with block sparks");
  check(b.observation().hp == php, "the blocked swings take no health");
  check(bfx.counts().unrouted.count("particle_spawn/6"), "block sparks have no definition in the trace: counted, not drawn");
  b.set_block(false);
  int released_after = -1;
  for (int t = 0; t < 120 && !lt(b.observation().hp, php); ++t) {
    advance(b);
    if (released_after < 0 && b.world().read(actor(b, 0) + 0x58c) == 2) released_after = t;
  }
  check(released_after >= 0 && released_after <= 1, "releasing block leaves state 8 for state 2 (top weight 1.0)");
  check(lt(b.observation().hp, php), "without the block the same Skelight's swing hurts");
  {
    bool refused = false;
    try { b.hit_event(0, 1); } catch (const std::exception &) { refused = true; }
    check(refused, "Full fixture refuses the round-1 hit-event button");
    refused = false;
    try { b.advance(session_control::DAMAGE); } catch (const std::exception &) { refused = true; }
    check(refused, "Full fixture refuses diagnostic damage commands");
  }
}

// ---- (b) Holy Strike ---------------------------------------------------------------------------
void holy_strike() {
  combat_play::Session s(terrain(), combat_play::Fixture::Full);
  combat_play::Effects fx(s, constants);
  FX::sim::EffectWorld world;
  std::string error;
  check(world.load_definitions(definitions(), error), "synthetic definitions: " + error);
  synthetic_glow_dims(world);
  fx.attach(&world);
  advance(s);
  advance(s);
  const auto a = actor(s, 1), p = actor(s, 0);
  const Raw hp0 = Raw(s.world().read(a + 0x228));
  s.cast_holy_strike();
  advance(s);
  check(has(s, "cast"), "the cast is made in the player's next run");
  const Raw cast_frame = Raw(s.world().read(C::FRAME)) - 1;
  // Documented flight (combat-spells.md 4.1): speed 450 per second, one frame of
  // 1/60 s per tick, from the caster's position along its facing, radius 12
  // (HolyStrike_ctor +0x1c); the touch comes when the bolt reaches the sum of radii.
  const double dx = 80.0, bolt = 12.0;
  float rs;
  const Raw rsr = Raw(s.world().read(a + 0x1c));
  std::memcpy(&rs, &rsr, 4);
  const int flight = int(std::ceil((dx - bolt - double(rs)) / 7.5));
  int damage_frame = -1;
  std::size_t particles_at_cast = world.particles.live().size(), glows = 0;
  for (int t = 0; t < 30 && damage_frame < 0; ++t) {
    advance(s);
    const auto before = fx.counts().glows;
    fx.draw_slots();
    glows += fx.counts().glows - before;
    world.draw(camera(), std::int32_t(s.world().read(C::FRAME)), std::int32_t(s.world().read(C::FRAME)));
    if (damaged(s, a)) {
      damage_frame = int(s.world().read(C::FRAME)) - 1;
      check(has(s, "light") && has(s, "destroy"), "the hit tick makes the LightEffect and the bolt ends in its own run");
    }
  }
  std::cout << "holy strike: cast frame " << cast_frame << " damage frame " << damage_frame << " skelight radius " << rs
            << " flight " << flight << " hp " << hp0 << "->" << s.world().read(a + 0x228) << '\n';
  check(damage_frame == int(cast_frame) + flight, "Holy Strike damage lands on the documented flight tick");
  check(lt(Raw(s.world().read(a + 0x228)), hp0), "the Skelight takes the Holy Strike damage");
  check(fx.counts().particle_spawns >= 24 && world.particles.live().size() > particles_at_cast,
        "the bolt's own-generator puffs reach the effect world");
  check(world.missing_definitions == 0, "every puff definition key is in the supplied JSON");
  check(glows >= 1, "HolyStrike_draw makes its glow while the bolt lives");
  int light_frames = 0;
  for (int t = 0; t < 70; ++t) {
    advance(s);
    bool lit = false;
    for (auto o : s.world().actors()) lit = lit || s.world().read(o + 0x48) == C::VT_LIGHTEFFECT;
    if (lit) ++light_frames;
  }
  check(light_frames >= 58 && light_frames <= 61, "the LightEffect lives one second (60 frames): " + std::to_string(light_frames));
  check(fx.counts().unrouted.count("LightEffect light part (no colour arguments in the combat trace)"),
        "the LightEffect's light colours are not in the combat trace: counted, not invented");
  (void)p;
}

// ---- (c) headless and drawing machines ----------------------------------------------------------
std::string scenario(bool draw) {
  combat_play::Session s(terrain(), combat_play::Fixture::Full);
  combat_play::Effects fx(s, constants);
  FX::sim::EffectWorld world;
  std::string error;
  if (draw) {
    if (!world.load_definitions(definitions(), error)) throw std::runtime_error(error);
    synthetic_glow_dims(world);
    fx.attach(&world);
  }
  std::string trace;
  auto record = [&] {
    trace += std::to_string(s.observation().ready_ticks) + " " + std::to_string(s.shared_seed()) + " " +
             std::to_string(s.observation().hp);
    for (const auto &a : s.actors()) trace += " " + std::to_string(a.present) + ":" + std::to_string(a.hp);
    trace += "\n";
  };
  advance(s);
  for (int t = 0; t < 300; ++t) {
    std::uint32_t cmd = 0;
    if (t < 3) cmd = session_control::NEG_X;
    if (t == 3) s.set_block(true);
    if (t == 50) s.set_block(false);
    if (t >= 52 && t < 90) cmd = session_control::POS_X;
    if (t >= 95 && t % 12 == 0) s.press_attack(true);
    if (t == 170) s.cast_holy_strike();
    advance(s, cmd);
    if (draw) {
      fx.draw_slots();
      world.draw(camera(), std::int32_t(s.world().read(C::FRAME)), std::int32_t(s.world().read(C::FRAME)));
    }
    record();
  }
  if (draw) check(world.particles.live().size() + fx.counts().particle_spawns > 0, "the drawing machine draws effects");
  return trace;
}
void headless_equals_drawn() {
  const std::string headless = scenario(false), drawn = scenario(true);
  check(!headless.empty() && headless == drawn, "headless and drawing runs: byte-identical seeds and health");
  std::cout << "headless/drawn trace bytes " << headless.size() << " / " << drawn.size() << '\n';
}

// ---- (d) GameHooks into the combat World ----------------------------------------------------------
std::string hooks(bool draw) {
  combat_play::Session s(terrain(), combat_play::Fixture::Full);
  combat_play::Effects fx(s, constants);
  FX::sim::EffectWorld world;
  std::string error;
  if (draw) {
    // DiseaseCloud's three stream definitions, synthetic values.
    std::string json = "[";
    for (unsigned k = 0; k < 3; ++k) {
      const auto def = constants->needed(0x2CE6E0, "particle", k, "a0");
      json += std::string(k ? "," : "") + "{\"site\":" + std::to_string(k + 1) +
              ",\"call\":\"particleDef_ctor\",\"def\":" + std::to_string(def) +
              ",\"complete\":true,\"size_start\":2,\"size_end\":2,\"gravity\":0,\"start_rgba\":[90,120,40,128]"
              ",\"end_rgba\":[90,120,40,0],\"life_class\":2,\"kind\":0,\"tile\":17,\"callback\":0}";
    }
    check(world.load_definitions(json + "]", error), "cloud definitions: " + error);
    fx.attach(&world);
  }
  advance(s);
  const auto a = actor(s, 1);
  // An authored DiseaseCloud on the effect model's image (fx_sim_tests' layout),
  // centred on Skelight A in the combat World, strength 7, end frame 200.
  auto &m = fx.simulation().classes();
  const std::int64_t obj = 0x5000;
  for (unsigned k = 0; k < 3; ++k) m.mem.poke(obj + 0x10 + 4 * k, 4, s.world().read(a + 16 + 4 * k));
  m.mem.poke(obj + 0x44, 4, 0xC0FFEE);
  m.mem.poke(obj + 0x4C, 4, 0x6000);
  m.mem.poke(0x6000 + 0xC, 4, 0x6100);
  m.mem.poke(obj + 0x54, 4, 200);
  m.mem.poke(obj + 0x58, 4, 30);
  m.mem.poke(obj + 0x5C, 4, F(10));
  m.mem.poke(obj + 0x60, 4, 7);
  fx.run_each_tick("diseasecloud_run", obj);
  // DiseaseCloud_run hands its every-30th-frame damage to the combat World
  // (damage_descriptor). Disease (0x26) always allocates a DiseaseEffect
  // (0x1b0), which combat-runtime-2 explicitly refuses: the session must fail
  // there, on the same frame headless and drawn, never silently skip it.
  std::string trace;
  int refused_frame = -1;
  std::string refusal;
  for (int t = 0; t < 70 && refused_frame < 0; ++t) {
    const auto o = s.advance(0);
    if (o.failed) {
      refused_frame = int(s.world().read(C::FRAME));
      refusal = o.error;
      break;
    }
    trace += std::to_string(s.world().read(C::FRAME)) + " " + std::to_string(s.shared_seed()) + " " +
             std::to_string(s.world().read(a + 0x228)) + "\n";
  }
  check(refused_frame >= 0 && refused_frame % 30 == 0, "the cloud's damage frame is a multiple of 30: " +
                                                           std::to_string(refused_frame));
  check(fx.counts().game_damage == 1 && refusal.find("damage effect allocation") != std::string::npos,
        "the cloud damage reached World::damage_descriptor and met the DiseaseEffect refusal: " + refusal);
  check(fx.counts().class_runs == fx.counts().ticks && fx.counts().ticks >= 29,
        "the cloud ran in every ready tick before it: " + std::to_string(fx.counts().class_runs));
  trace += "refused " + std::to_string(refused_frame) + " " + refusal + "\n";
  std::cout << "hooks draw=" << draw << " cloud ran " << fx.counts().class_runs << " ticks, refused on frame "
            << refused_frame << '\n';
  // Fire objects of fx_spawn id 5 are refused by the combat runtime.
  bool refused = false;
  try {
    fx.simulation().fx_spawn(5, {0, 0, 0}, true, 2);
  } catch (const C::Unsupported &) {
    refused = true;
  }
  check(refused, "fx_spawn id 5 Fire objects meet the combat runtime refusal");
  return trace;
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc < 2) throw std::runtime_error("usage: playable-combat-fx <fx_skill_recipes JSON>");
    constants = std::make_shared<const FX::skills::RecipeConstants>(FX::skills::RecipeConstants::from_json(read(argv[1])));
    walk_and_combo();
    holy_strike();
    headless_equals_drawn();
    const std::string headless_cloud = hooks(false), drawn_cloud = hooks(true);
    check(headless_cloud == drawn_cloud, "the cloud's seeds, health and refusal are identical headless and drawn");
  } catch (const std::exception &e) {
    std::cerr << "FAIL: exception " << e.what() << '\n';
    return 1;
  }
  std::cout << "playable-combat-fx: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
