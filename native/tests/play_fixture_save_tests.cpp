// Review finding 2: fixture continuation v1 (docs/NATIVE_SAVE.md "Fixture
// continuation"). Synthetic fixtures only.
//  (a) Full combat saved mid-fight with Holy Strike in flight and a drawing
//      EffectWorld; the continued run's seed / health trace and its per-machine
//      effect world are byte-identical to an uninterrupted run;
//  (b) the level fixture saved mid gate travel continues byte-identically;
//  (c) the exact EffectWorld snapshot round-trips and refuses atomically;
//  (d) integrity, profile, version, constants and journal refusals;
//  (e) the round-1 Combat and Animated fixtures save and continue too.
#include "champions/fx_world_save.hpp"
#include "champions/play_fixture_save.hpp"
#include "combat_runtime_internal.hpp"
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

using namespace champions::simulation::rta;
namespace C = combat_runtime;
namespace FS = fixture_save;
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
template <class F> bool refuses(F &&f, const std::string &expect = "") {
  try {
    f();
  } catch (const std::exception &e) {
    if (!expect.empty() && std::string(e.what()).find(expect) == std::string::npos) {
      std::cerr << "  refusal was: " << e.what() << '\n';
      return false;
    }
    return true;
  }
  return false;
}
std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}
std::string constants_json;
std::shared_ptr<const FX::skills::RecipeConstants> constants;
FS::ConstantsIdentity identity;
world_queries::Snapshot terrain() { return session_control::Controller().setup().initial_terrain; }
Raw F(float f) {
  Raw r;
  std::memcpy(&r, &f, 4);
  return r;
}
// The same synthetic definitions and camera as playable_combat_fx_tests.
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
FX::sim::Camera camera() {
  FX::sim::Camera c;
  c.view = {0, 0, F(1), F(-1), 0, 0, 0, F(-1), 0, 0, F(40), F(200)};
  c.proj_scale = F(480.0f);
  c.camera = c.view;
  c.viewproj = mat4_mul_mat34(c.view, mat4_set_projection(0x3F7FFFFF, 0x3EEEEEEE, F(480.0f), ONE, 0x457FFF00,
                                                          0x41900000, 0x44E10000));
  return c;
}
std::unique_ptr<FX::sim::EffectWorld> drawing_world() {
  auto w = std::make_unique<FX::sim::EffectWorld>();
  std::string error;
  if (!w->load_definitions(definitions(), error)) throw std::runtime_error(error);
  for (const auto &g : FX::sim::glow_textures()) w->glow_dims[g.global] = {32, 32};
  return w;
}
std::string digest(const FX::sim::SaveBytes &b) {
  std::uint64_t h = 1469598103934665603ull;
  for (auto c : b) h = (h ^ c) * 1099511628211ull;
  return std::to_string(h) + "/" + std::to_string(b.size());
}

// ---- (a) Full combat -----------------------------------------------------------------------------
// The wave-11 300-tick scenario (turn, block, walk, combo, Holy Strike at 170),
// drawn every tick (twice on even ticks: the cosmetic seed and the row-4
// flipbooks advance per drawn frame), with the draw slots before each draw.
struct FullRun {
  std::unique_ptr<FS::CombatScene> scene;
  std::unique_ptr<FX::sim::EffectWorld> world;
  std::string trace;
  void line() {
    const auto &s = scene->session();
    const auto o = s.observation();
    trace += std::to_string(o.ready_ticks) + " f" + std::to_string(o.frame) + " s" + std::to_string(s.shared_seed()) +
             " hp" + std::to_string(o.hp);
    for (const auto &a : s.actors()) trace += " " + std::to_string(a.present) + ":" + std::to_string(a.hp);
    // Round 5: the AntQueen, Skeleton and Soul state, position and the
    // queen's bank angle, and this tick's demon tests and Holy Shield sample
    // points, so the save/load comparison covers them.
    for (const auto &a : s.actors()) {
      if (a.slot < 4 || !a.present) continue;
      const auto addr = s.world().target(a.handle);
      trace += " s" + std::to_string(a.slot) + "=" + std::to_string(s.world().read(addr + 0x232, 2)) + "@" +
               std::to_string(s.world().read(addr + 0x10)) + "," + std::to_string(s.world().read(addr + 0x14));
      if (a.slot == 4) trace += "b" + std::to_string(s.world().read(addr + 0x3e4, 2));
    }
    int demon = 0, sample = 0;
    for (const auto &e : s.tick_events()) {
      demon += e.name == "Soul_isDemonNear";
      sample += e.name == "model_getSamplePoint";
    }
    demon_tests += demon;
    shield_samples += sample;
    trace += " d" + std::to_string(demon) + " p" + std::to_string(sample) + "\n";
  }
  int demon_tests = 0, shield_samples = 0;
  void step(int t) {
    std::uint32_t cmd = 0;
    if (t < 3) cmd = session_control::NEG_X;
    if (t == 3) scene->set_block(true);
    if (t == 50) scene->set_block(false);
    if (t >= 52 && t < 90) cmd = session_control::POS_X;
    if (t >= 95 && t % 12 == 0) scene->press_attack(true);
    if (t == 170) scene->cast_holy_strike();
    if (t == 120) {
      // A refused operation is journaled and refused again on replay.
      check(refuses([&] { scene->hit_event(0, 1); }), "Full fixture refuses the hit-event button");
    }
    const auto o = scene->advance(cmd);
    if (o.failed) throw std::runtime_error("session failed: " + o.error);
    const auto frame = std::int32_t(scene->session().world().read(C::FRAME));
    for (int k = 0; k < (t % 2 ? 1 : 2); ++k) {
      scene->draw_slots();
      world->draw(camera(), frame, frame, k == 1);
    }
    line();
  }
};
FullRun full_start() {
  FullRun r;
  r.scene = std::make_unique<FS::CombatScene>(terrain(), combat_play::Fixture::Full, constants, identity);
  r.world = drawing_world();
  r.scene->attach(r.world.get());
  r.scene->advance(0); // the no-ready first submission
  return r;
}
bool holy_strike_flying(const FS::CombatScene &scene) {
  for (auto a : scene.session().world().actors())
    if (scene.session().world().read(a + 0x48) == C::VT_HOLYSTRIKE) return true;
  return false;
}
void mounted_journal() {
  FS::CombatScene scene(terrain(), combat_play::Fixture::Full, constants, identity);
  scene.spawn_mounted_horseman(0);
  const auto before = scene.actors();
  bool rider = false, mount = false;
  for (const auto &actor : before) {
    rider |= actor.slot == 7 && actor.present;
    mount |= actor.slot == 8 && actor.present;
  }
  check(rider && mount, "mounted pair is visible in the Full fixture");
  const auto saved = scene.save();
  auto loaded = FS::CombatScene::load(saved, "rta", constants, identity);
  check(loaded.scene && loaded.scene->actors().size() == before.size() &&
            loaded.scene->shared_seed() == scene.shared_seed(),
        "mounted pair replays through the fixture journal");
}
void full_combat() {
  constexpr int TICKS = 300, SAVE_AT = 174; // cast on 170, the bolt flies until about 178
  FullRun whole = full_start();
  for (int t = 0; t < TICKS; ++t) whole.step(t);

  FullRun first = full_start();
  for (int t = 0; t < SAVE_AT; ++t) first.step(t);
  check(holy_strike_flying(*first.scene), "Holy Strike is in flight at the save point");
  const auto presentation = FX::sim::save_world(*first.world);
  const FS::Bytes bytes = first.scene->save(&presentation);
  const std::string prefix = first.trace;
  const auto actions = first.scene->actions();
  first = FullRun{}; // nothing survives but the file
  std::cout << "full combat save: " << bytes.size() << " bytes, " << actions << " operations, presentation "
            << presentation.size() << " bytes\n";

  auto loaded = FS::CombatScene::load(bytes, "rta", constants, identity);
  check(loaded.scene && loaded.presentation && *loaded.presentation == presentation, "load returns the scene and its presentation block");
  FullRun second;
  second.scene = std::move(loaded.scene);
  second.world = std::make_unique<FX::sim::EffectWorld>();
  FX::sim::load_world(*second.world, *loaded.presentation);
  second.scene->attach(second.world.get());
  check(holy_strike_flying(*second.scene), "the loaded fight still has the Holy Strike in flight");
  check(second.scene->save(&presentation).size() > 0, "a loaded scene saves again");
  for (int t = SAVE_AT; t < TICKS; ++t) second.step(t);

  check(prefix + second.trace == whole.trace, "mid-fight save/load: byte-identical seed and health trace");
  check(FX::sim::save_world(*second.world) == FX::sim::save_world(*whole.world),
        "the continued per-machine effect world is byte-identical to the uninterrupted one");
  check(second.scene->effects()->counts().glows == whole.scene->effects()->counts().glows &&
            second.scene->effects()->counts().particle_spawns == whole.scene->effects()->counts().particle_spawns,
        "the effect simulation's counts continue identically");
  // The Holy Strike damage happened after the load.
  const auto pos = whole.trace.find("\n", 0);
  (void)pos;
  std::size_t hp_changes = 0;
  {
    std::istringstream a(prefix), b(second.trace);
    std::string last, line;
    while (std::getline(b, line)) {
      auto cut = line.find(" 1:", line.find("hp"));
      if (!last.empty() && cut != std::string::npos && line.substr(cut) != last) ++hp_changes;
      if (cut != std::string::npos) last = line.substr(cut);
    }
  }
  // The AntQueen, Skeleton and Soul occupy slots 4, 5 and 6. The trace
  // includes their states and positions through the save/load boundary.
  bool queen = false, soul = false, skeleton = false;
  for (const auto &a : whole.scene->actors()) {
    queen = queen || (a.slot == 4 && a.present);
    soul = soul || (a.slot == 6 && a.present);
    skeleton = skeleton || (a.slot == 5 && a.present);
  }
  check(queen && soul && skeleton,
        "the Full fight places the AntQueen, Skeleton and Soul in slots 4, 5 and 6");
  std::cout << "full combat round 5: " << whole.demon_tests << " demon tests, " << whole.shield_samples
            << " Holy Shield sample points\n";
  check(whole.demon_tests == 300 / 16 + 1, "the Soul tests for a demon every 16 frames of the live fight");
  check(whole.shield_samples > 0 && whole.shield_samples % 241 == 0,
        "Holy Shield absorbs in the live fight, 241 sample points each time");
  auto player = whole.scene->session().world().target(whole.scene->actors().at(0).handle);
  check(whole.scene->session().world().read(player + 0x114) != 0, "Holy Shield pose stays planted");
  check(hp_changes >= 1, "creature health changes after the load (the bolt lands in the continued run)");
  std::cout << "full combat trace " << whole.trace.size() << " bytes, world " << digest(FX::sim::save_world(*whole.world))
            << '\n';
}

// Wave 12: the Full fixture's own-machine creature (creature-ai) and thrown
// potions (combat-runtime-4) are journaled operations; a mid-run save
// continues with the identical trace, including a refused second creature.
void wave12_extensions() {
  constexpr int TICKS = 200, SAVE_AT = 90;
  auto ops = [](FS::CombatScene &scene, int t) {
    // Critter's constructor is not executed (audit CAI-4): refused before
    // the World is touched. Gnaw (index 8) is the executed constructor.
    if (t == 4) check(refuses([&] { scene.spawn_creature(0); }), "an unexecuted constructor is refused");
    if (t == 5) scene.spawn_creature(8); // Gnaw
    if (t == 6) check(refuses([&] { scene.spawn_creature(7); }), "one own-machine creature per session");
    // Potions 1 and 2 are checked apart below.
    if (t == 60) scene.drop_potion(3);
    if (t >= 95 && t % 12 == 0) scene.press_attack(true);
  };
  auto run = [&](FullRun &r, int from, int to) {
    for (int t = from; t < to; ++t) {
      ops(*r.scene, t);
      const auto o = r.scene->advance(0);
      if (o.failed) throw std::runtime_error("wave-12 session failed: " + o.error);
      const auto frame = std::int32_t(r.scene->session().world().read(C::FRAME));
      r.scene->draw_slots();
      r.world->draw(camera(), frame, frame, false);
      r.line();
    }
  };
  for (unsigned kind : {1u, 2u}) {
    // LooseFire_ctor and poisonGasSpell_ctor run (research-combat-ctors
    // 98c081a). Kind 1's fires burn for 100 runs and end through Fire_dtor
    // 0x3c5050, the LooseFire at frame F + 121 through LooseFire_dtor
    // (research-combat-ctors-2 combat-ctors-2.md section 6). Kind 2's cloud
    // runs poisonGasSpell_run 0x1fd0f0 (combat round 6) and ends through
    // poisonGasSpell_dtor. Both run until the idle fight's own end.
    FullRun r = full_start();
    r.scene->drop_potion(kind);
    std::string error;
    int ticks = 0;
    for (; ticks < 2000 && error.empty(); ++ticks) {
      const auto o = r.scene->advance(0);
      if (o.failed) error = o.error;
    }
    check(ticks > (kind == 1 ? 130 : 30),
          "potion " + std::to_string(kind) + " runs past its round-1 stop: " + std::to_string(ticks) +
              " '" + error + "'");
    std::cout << "potion " << kind << " stops at ready tick " << ticks << ": " << error << "\n";
  }
  {
    // How far the Full fight runs with its Skeleton in slot 5, and with a
    // second Skeleton spawned through the authored operation.
    for (bool skeleton : {false, true}) {
      FullRun r = full_start();
      std::string error;
      int ticks = 0;
      int first_chase = -1;
      for (; ticks < 4000 && error.empty(); ++ticks) {
        if (skeleton && ticks == 3)
          r.scene->spawn_creature(6);
        const auto o = r.scene->advance(0);
        if (o.failed) error = o.error;
        if (!o.failed && first_chase < 0)
          for (const auto &a : r.scene->actors())
            if (a.slot == 5 && a.present) {
              auto obj=r.scene->session().world().target(a.handle);
              if (r.scene->session().world().read(obj+0x232,2)==3)
                first_chase=ticks;
            }
      }
      if (skeleton)
        check(ticks > 300 && error.find("party wipe") != std::string::npos,
              "a spawned Skeleton runs its chase until the party wipe: '" + error + "'");
      else
        check(ticks > 300, "the idle Full fight with the AntQueen runs past 300 ticks: '" + error + "'");
      std::cout << (skeleton ? "spawned Skeleton" : "idle Full fight") << " stops at ready tick "
                << ticks << ", first chase at frame " << first_chase << ": " << error << "\n";
    }
    FullRun r=full_start();
    std::string error;
    int ticks=0;
    for (;ticks<4000&&error.empty();++ticks) {
      if (ticks>=95&&ticks%12==0) r.scene->press_attack(true);
      const auto o=r.scene->advance(0);
      if (o.failed) error=o.error;
    }
    std::cout << "attacking Full fight stops at ready tick " << ticks
              << ": " << error << "\n";
    check(ticks>300,"attacking Full fight runs past 300 ticks");
  }
  FullRun whole = full_start();
  run(whole, 0, TICKS);
  check(whole.scene->actors().size() == 7 && whole.scene->own_creature_class() == "Gnaw",
        "the own-machine creature is observed in slot 3, beside the Full trio");
  FullRun first = full_start();
  run(first, 0, SAVE_AT);
  const auto presentation = FX::sim::save_world(*first.world);
  const FS::Bytes bytes = first.scene->save(&presentation);
  const std::string prefix = first.trace;
  first = FullRun{};
  auto loaded = FS::CombatScene::load(bytes, "rta", constants, identity);
  FullRun second;
  second.scene = std::move(loaded.scene);
  second.world = std::make_unique<FX::sim::EffectWorld>();
  FX::sim::load_world(*second.world, *loaded.presentation);
  second.scene->attach(second.world.get());
  check(second.scene->own_creature_class() == "Gnaw", "the loaded scene keeps its creature");
  run(second, SAVE_AT, TICKS);
  check(prefix + second.trace == whole.trace, "wave-12 operations: byte-identical continuation");
  check(FX::sim::save_world(*second.world) == FX::sim::save_world(*whole.world),
        "wave-12 operations: identical continued effect world");
}

// combat-ctors: a Soul built by its executed constructor in slot 3 (the
// fixture's Soul is in slot 6) and potion 1's LooseFire and Fire objects
// (fx_spawn id 5) are journaled; a save made while the fires burn continues
// byte for byte, through the fires' and the LooseFire's destructors.
void combat_ctors_extensions() {
  constexpr int SAVE_AT = 60, LIMIT = 300;
  auto ops = [](FS::CombatScene &scene, int t) {
    if (t == 3) scene.spawn_creature(1); // Soul
    if (t == 5) scene.drop_potion(1);
  };
  struct Stop {
    int tick = -1;
    std::string error;
  };
  auto run = [&](FullRun &r, int from, int to, Stop &stop) {
    for (int t = from; t < to && stop.tick < 0; ++t) {
      ops(*r.scene, t);
      const auto o = r.scene->advance(0);
      if (o.failed) {
        stop = {t, o.error};
        break;
      }
      const auto frame = std::int32_t(r.scene->session().world().read(C::FRAME));
      r.scene->draw_slots();
      r.world->draw(camera(), frame, frame, false);
      r.line();
      int fires = 0;
      for (auto a : r.scene->session().world().actors())
        fires += r.scene->session().world().read(a + 0x48) == 0x50f208;
      r.trace += " fires" + std::to_string(fires) + "\n";
    }
  };
  FullRun whole = full_start();
  Stop whole_stop;
  run(whole, 0, LIMIT, whole_stop);
  check(whole.scene->own_creature_class() == "Soul", "the Soul is the slot-3 creature");
  int left = 0;
  for (auto a : whole.scene->session().world().actors()) {
    auto table = whole.scene->session().world().read(a + 0x48);
    left += table == 0x50f208 || table == 0x50e2b8;
  }
  check((whole_stop.tick < 0 || whole_stop.tick > 130) && left == 0,
        "potion 1's fires and LooseFire end through their destructors: " +
            std::to_string(whole_stop.tick) + " '" + whole_stop.error + "'");
  FullRun first = full_start();
  Stop first_stop;
  run(first, 0, SAVE_AT, first_stop);
  int burning = 0;
  for (auto a : first.scene->session().world().actors())
    burning += first.scene->session().world().read(a + 0x48) == 0x50f208;
  check(first_stop.tick < 0 && burning >= 15, "fires burn at the save point");
  const auto presentation = FX::sim::save_world(*first.world);
  const FS::Bytes bytes = first.scene->save(&presentation);
  const std::string prefix = first.trace;
  first = FullRun{};
  auto loaded = FS::CombatScene::load(bytes, "rta", constants, identity);
  FullRun second;
  second.scene = std::move(loaded.scene);
  second.world = std::make_unique<FX::sim::EffectWorld>();
  FX::sim::load_world(*second.world, *loaded.presentation);
  second.scene->attach(second.world.get());
  check(second.scene->own_creature_class() == "Soul", "the loaded scene keeps its Soul");
  Stop second_stop;
  run(second, SAVE_AT, LIMIT, second_stop);
  check(prefix + second.trace == whole.trace, "combat-ctors operations: byte-identical continuation");
  check(second_stop.tick == whole_stop.tick && second_stop.error == whole_stop.error,
        "the continued run stops on the same tick for the same reason");
  std::cout << "combat ctors save: " << bytes.size() << " bytes, " << burning << " fires at the save, stop at "
            << whole_stop.tick << ": " << whole_stop.error << "\n";
}

// ---- (b) level fixture: mid gate travel ------------------------------------------------------------
struct LevelRun {
  std::unique_ptr<FS::LevelScene> scene = std::make_unique<FS::LevelScene>();
  std::string trace;
  void line() {
    const auto &s = scene->session();
    const auto o = s.observation();
    const auto f = s.flow();
    trace += std::to_string(o.ready_ticks) + " f" + std::to_string(o.frame) + " s" + std::to_string(s.shared_seed()) +
             " hp" + std::to_string(o.hp) + " " + f.level + " g" + std::to_string(f.generation) + " t" +
             std::to_string(f.travel_ticks) + " p" + std::to_string(o.position[0]) + "," + std::to_string(o.position[1]) +
             "\n";
  }
  void step(int t) {
    if (t == 16) scene->open_gate();
    if (t == 17) {
      check(refuses([&] { scene->confirm_gate("aa"); }), "the gate refuses the current level");
      scene->confirm_gate("aab");
    }
    if (t == 220) scene->request_level("aa");
    const auto o = scene->advance(0);
    if (o.failed) throw std::runtime_error("level failed: " + o.error);
    line();
  }
};
void level_travel() {
  constexpr int TICKS = 260, SAVE_AT = 90; // confirmed on 17, 180-tick travel
  LevelRun whole;
  for (int t = 0; t < TICKS; ++t) whole.step(t);
  check(whole.scene->session().flow().level == "aa" && whole.scene->session().flow().generation >= 3,
        "the uninterrupted run travels to aab and back");
  LevelRun first;
  for (int t = 0; t < SAVE_AT; ++t) first.step(t);
  const auto mid = first.scene->session().flow();
  check(mid.level == "aa" && mid.travel_ticks > 0, "saved mid gate travel (" + std::to_string(mid.travel_ticks) + " ticks)");
  const FS::Bytes bytes = first.scene->save();
  const std::string prefix = first.trace;
  first = LevelRun{};
  LevelRun second;
  second.scene = FS::LevelScene::load(bytes, "rta");
  check(second.scene->session().flow().travel_ticks == mid.travel_ticks, "the travel clock is restored");
  for (int t = SAVE_AT; t < TICKS; ++t) second.step(t);
  check(prefix + second.trace == whole.trace, "mid-gate-travel save/load: byte-identical seed, health and flow trace");
  std::cout << "level save: " << bytes.size() << " bytes, trace " << whole.trace.size() << " bytes\n";
  check(second.scene->save() != bytes && FS::LevelScene::load(second.scene->save(), "rta") != nullptr,
        "the continued level saves and loads again");
}

// ---- (c) EffectWorld snapshot ---------------------------------------------------------------------
void world_snapshot() {
  FullRun r = full_start();
  for (int t = 0; t < 180; ++t) r.step(t);
  const auto bytes = FX::sim::save_world(*r.world);
  check(r.world->particles.live().size() > 0, "the snapshot fixture has live particles");
  FX::sim::EffectWorld copy;
  FX::sim::load_world(copy, bytes);
  check(FX::sim::save_world(copy) == bytes, "an EffectWorld snapshot round-trips byte for byte");
  // Identical continuation of the copy and the original.
  for (int k = 0; k < 30; ++k) {
    r.world->tick(1000 + k);
    copy.tick(1000 + k);
    const auto a = r.world->draw(camera(), 1000 + k, 1000 + k), b = copy.draw(camera(), 1000 + k, 1000 + k);
    check(a.size() == b.size(), "the restored world draws the same packets");
  }
  check(FX::sim::save_world(copy) == FX::sim::save_world(*r.world), "and continues to the same state");
  auto bad = bytes;
  bad[bad.size() - 1] ^= 1;
  FX::sim::EffectWorld target;
  const auto before = FX::sim::save_world(target);
  check(refuses([&] { FX::sim::load_world(target, FS::Bytes(bytes.begin(), bytes.end() - 1)); }, "truncated"),
        "a truncated snapshot is refused");
  bad = bytes;
  bad[8] = 9;
  check(refuses([&] { FX::sim::load_world(target, bad); }, "version"), "another snapshot version is refused");
  check(FX::sim::save_world(target) == before, "a refused snapshot leaves the target unchanged");
}

// ---- (d) refusals ---------------------------------------------------------------------------------
void refusals() {
  FullRun r = full_start();
  for (int t = 0; t < 20; ++t) r.step(t);
  const FS::Bytes bytes = r.scene->save();
  check(FS::is_fixture_save(bytes) && FS::CombatScene::load(bytes, "rta", constants, identity).scene != nullptr,
        "a headless-presentation save loads");
  check(refuses([&] { FS::CombatScene::load(bytes, "norrath", constants, identity); }, "profile"), "Norrath refused");
  check(refuses([&] { FS::CombatScene::load(bytes, "rta", nullptr, {}); }, "recipe constants"),
        "a Full save needs the recipe constants");
  check(refuses([&] { FS::CombatScene::load(bytes, "rta", constants, FS::identify(constants_json + " ")); }, "differ"),
        "other recipe constants are refused");
  auto bad = bytes;
  bad[bad.size() - 2] ^= 0x40;
  check(refuses([&] { FS::CombatScene::load(bad, "rta", constants, identity); }, "checksum"), "a flipped byte fails the CRC");
  check(refuses([&] { FS::CombatScene::load(FS::Bytes(bytes.begin(), bytes.end() - 3), "rta", constants, identity); }),
        "a truncated save is refused");
  bad = bytes;
  bad[8] = 2;
  bad[20] = bad[21] = bad[22] = bad[23] = 0;
  const auto crc = FS::crc32(bad, 20);
  for (unsigned i = 0; i < 4; ++i) bad[20 + i] = std::uint8_t(crc >> (8 * i));
  check(refuses([&] { FS::CombatScene::load(bad, "rta", constants, identity); }, "version"), "version 2 is refused");
  // A journal command changed (and the CRC recomputed): the replayed state no
  // longer matches the recorded witness.
  bad = bytes;
  std::size_t at = std::string::npos;
  for (std::size_t i = 24; i + 6 < bad.size(); ++i)
    if (bad[i] == 0 && bad[i + 1] == 0 && bad[i + 2] == std::uint8_t(session_control::NEG_X)) {
      at = i + 2;
      break;
    }
  check(at != std::string::npos, "found a NEG_X advance in the journal");
  if (at != std::string::npos) {
    bad[at] = std::uint8_t(session_control::POS_X);
    for (unsigned i = 0; i < 4; ++i) bad[20 + i] = 0;
    const auto c = FS::crc32(bad, 20);
    for (unsigned i = 0; i < 4; ++i) bad[20 + i] = std::uint8_t(c >> (8 * i));
    check(refuses([&] { FS::CombatScene::load(bad, "rta", constants, identity); }, "witness"),
          "a changed journal fails the continuation witness");
  }
  check(refuses([&] { FS::LevelScene::load(bytes, "rta"); }, "not a level"), "a combat save is not a level save");
  LevelRun l;
  for (int t = 0; t < 3; ++t) l.step(t);
  check(refuses([&] { FS::CombatScene::load(l.scene->save(), "rta", constants, identity); }, "not a combat"),
        "a level save is not a combat save");
  check(refuses([&] { FS::CombatScene(terrain(), combat_play::Fixture::Combat, constants, identity); }),
        "only the Full fixture takes recipe constants");
}

void ranked_spell_save() {
  FS::CombatScene scene(terrain(), combat_play::Fixture::Full, constants, identity);
  const auto empty = scene.save();
  check(refuses([&] { scene.select_disease_bolt(0); }, "rank"),
        "out-of-range ranked selection refuses before journaling");
  check(scene.save() == empty, "invalid rank leaves the fixture save unchanged");
  scene.select_disease_bolt(2);
  const auto bytes = scene.save();
  auto loaded = FS::CombatScene::load(bytes, "rta", constants, identity);
  check(loaded.scene->world().read(C::PLAYERS + 0x4b0a, 2) == 2,
        "ranked Disease Bolt selection survives journal replay");
  check(loaded.scene->save() == bytes,
        "ranked Disease Bolt selection preserves the save witness");
}

// ---- (e) round-1 fixtures ----------------------------------------------------------------------------
void round_one(combat_play::Fixture which, const std::string &name) {
  auto run = [&](int ticks, FS::CombatScene &scene, std::string &trace) {
    for (int t = 0; t < ticks; ++t) {
      if (which == combat_play::Fixture::Combat && t % 7 == 3) {
        try {
          scene.hit_event(0, 1);
        } catch (const std::exception &) {
        }
      }
      const auto o = scene.advance(0);
      trace += std::to_string(o.ready_ticks) + " " + std::to_string(scene.session().shared_seed()) + " " +
               std::to_string(o.hp);
      for (const auto &a : scene.session().actors()) trace += " " + std::to_string(a.hp);
      trace += "\n";
    }
  };
  FS::CombatScene whole(terrain(), which);
  std::string a, b;
  run(80, whole, a);
  auto first = std::make_unique<FS::CombatScene>(terrain(), which);
  run(37, *first, b);
  const auto bytes = first->save();
  first.reset();
  auto loaded = FS::CombatScene::load(bytes, "rta");
  check(!loaded.presentation, name + ": no presentation block");
  run(43, *loaded.scene, b);
  check(a == b, name + " fixture: save/load continues byte-identically");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc < 2) throw std::runtime_error("usage: fixture-save <fx_skill_recipes JSON>");
    constants_json = read(argv[1]);
    constants = std::make_shared<const FX::skills::RecipeConstants>(FX::skills::RecipeConstants::from_json(constants_json));
    identity = FS::identify(constants_json);
    mounted_journal();
    full_combat();
    wave12_extensions();
    combat_ctors_extensions();
    level_travel();
    world_snapshot();
    refusals();
    ranked_spell_save();
    round_one(combat_play::Fixture::Combat, "Combat");
    round_one(combat_play::Fixture::Animated, "Animated");
  } catch (const std::exception &e) {
    std::cerr << "FAIL: exception " << e.what() << '\n';
    return 1;
  }
  std::cout << "fixture-save: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
