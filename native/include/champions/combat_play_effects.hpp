#pragma once
// Playable integration (wave 11): native-fx-2's EffectSimulation inside the opt-in
// combat play session. It is installed as the session's ready-tick observer, so it
// runs inside every simulated tick on every machine, with or without a renderer:
//   - presentation call-outs that the combat World records with their arguments
//     (particle_spawn with a definition, fx_spawn with an id and position,
//     decal_add) are answered by EffectSimulation::answer; cosmetic ones reach the
//     attached per-machine EffectWorld only;
//   - effect-class call-outs that change gameplay go to the combat World through
//     GameHooks (damage_apply -> World::damage_descriptor, box queries ->
//     World::collect_box); constructors of Fire / LooseIceChunk / GutChunk objects
//     reach the combat runtime's documented refusals;
//   - after the priority-10 World pass the attached EffectWorld runs its priority-15
//     particle update (research docs/mainloop.md run-pass table).
// Shared draws of the effect half are made on the combat World's g_randSeed, so a
// headless machine and a drawing machine keep identical seeds and health.
#include "champions/combat_play.hpp"
#include "champions/fx_simulation.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace champions::simulation::rta::combat_play {
namespace fxsim = champions::rta::fx::sim;
namespace fxskills = champions::rta::fx::skills;

struct EffectCounts {
  std::uint64_t ticks = 0;           // ready ticks observed
  std::uint64_t particle_spawns = 0; // particle_spawn call-outs with a definition
  std::uint64_t fx_spawns = 0;       // fx_spawn call-outs with an id and position
  std::uint64_t decals = 0;          // decal_add call-outs
  std::uint64_t glows = 0;           // draw-slot glows (HolyStrike_draw)
  std::uint64_t class_runs = 0;      // effect-class run functions executed
  std::uint64_t game_damage = 0;     // damage_apply answered by the combat World
  std::uint64_t game_queries = 0;    // world queries answered by the combat World
  // Presentation call-outs the composed combat trace records without the
  // arguments a GS output needs (trail points, sounds, block sparks without a
  // definition, LightEffect colours ...): counted by name, never guessed.
  std::map<std::string, std::uint64_t> unrouted;
};

class Effects {
public:
  // constants: fx_skill_recipes.py --json (the user's) or the labelled synthetic
  // fixture. The session must outlive this object.
  Effects(Session &, std::shared_ptr<const fxskills::RecipeConstants> constants);
  ~Effects();
  Effects(const Effects &) = delete;
  Effects &operator=(const Effects &) = delete;

  // nullptr: headless. Only per-machine output depends on it.
  void attach(fxsim::EffectWorld *world);
  fxsim::EffectWorld *world() const { return sim_.world(); }
  fxsim::EffectSimulation &simulation() { return sim_; }
  // Object draw slots for one rendered frame (HolyStrike_draw 0x2ecd90 for every
  // live Holy Strike): glows into the attached world. Nothing without a world.
  void draw_slots();
  // Authored test fixture, not an original constructor: run an effect class's run
  // method (fxskills::Model::methods()) on `obj` of the effect model's memory
  // image once in every ready tick (skills::must_run_headless).
  void run_each_tick(const std::string &method, std::int64_t obj);
  const EffectCounts &counts() const { return counts_; }

private:
  Session &session_;
  Lcg shared_;
  fxsim::EffectSimulation sim_;
  std::vector<std::pair<std::string, std::int64_t>> runs_;
  EffectCounts counts_;
  void tick(Session &);
  // One shared generator across the World, the effect model and the recipes'
  // Lcg (combat_play_effects.cpp): with_shared opens an effect call, settle()
  // hands the seed over at every call-out.
  std::uint32_t chain_ = 0;
  bool in_shared_ = false;
  void settle();
  template <class F> void with_shared(F &&);
  std::optional<fxskills::Value> answer_game(const fxskills::Callout &);
};

} // namespace champions::simulation::rta::combat_play
