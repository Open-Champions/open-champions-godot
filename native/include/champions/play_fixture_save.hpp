#pragma once
// Fixture continuation v1 (docs/NATIVE_SAVE.md "Fixture continuation"): the
// versioned save of the opt-in combat fixtures (Combat, Animated, Full with its
// EffectSimulation) and of the two-level gate fixture. Like the OCNSAVE replay
// checkpoint it is not a struct dump: the file holds the fixture's complete
// initial input (selected terrain, recipe-constants identity) and the ordered
// journal of every operation made through these facades, including refused ones
// and the attach/draw-slot calls the effect half's per-machine state depends on.
// Loading builds a fresh fixture, replays the journal, and requires a redundant
// witness of the observable state (seeds, health, actors, flow, effect counts)
// and the canonical re-encoding to match byte for byte. The per-machine
// EffectWorld is carried as an opaque, integrity-checked presentation block
// (fx_world_save.hpp / ChampionsFxBridge), restored by the caller.
//
// No PS2 save structure, original data or host pointer is read or written.
#include "champions/combat_play.hpp"
#include "champions/combat_play_effects.hpp"
#include "champions/fx_simulation.hpp"
#include "champions/level_play.hpp"
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>

namespace champions::simulation::rta::fixture_save {
namespace fxsim = champions::rta::fx::sim;
namespace fxskills = champions::rta::fx::skills;
using Bytes = std::vector<std::uint8_t>;
inline constexpr const char *FORMAT = "fixture-continuation-v1";
inline constexpr std::uint32_t VERSION = 1, RTA_PROFILE = 1;
inline constexpr std::size_t MAX_BYTES = 32u * 1024u * 1024u;
inline constexpr std::size_t MAX_ACTIONS = 1u << 20;
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
// True when `bytes` start with the fixture continuation magic.
bool is_fixture_save(const Bytes &bytes);
// The recipe constants a Full save was made with: byte length and CRC32 of the
// fx_skill_recipes JSON text. Loading requires the same constants; the values
// themselves are never written.
struct ConstantsIdentity {
  std::uint32_t length = 0, crc = 0;
  bool operator==(const ConstantsIdentity &o) const { return length == o.length && crc == o.crc; }
};
ConstantsIdentity identify(const std::string &json);
std::uint32_t crc32(const Bytes &bytes, std::size_t skip_at = SIZE_MAX);

class CombatScene {
public:
  // constants/identity: required for Fixture::Full, refused otherwise.
  CombatScene(const world_queries::Snapshot &terrain, combat_play::Fixture which,
              std::shared_ptr<const fxskills::RecipeConstants> constants = {},
              ConstantsIdentity identity = {});
  ~CombatScene();
  CombatScene(const CombatScene &) = delete;
  CombatScene &operator=(const CombatScene &) = delete;

  const combat_play::Session &session() const { return *session_; }
  const combat_play::Effects *effects() const { return effects_.get(); }
  const world_queries::Snapshot &terrain() const { return terrain_; }
  combat_play::Fixture fixture() const { return fixture_; }
  std::size_t actions() const { return journal_.size(); }
  // Read-only forwarding for observers (no mutable Session escapes the journal).
  session_control::Observation observation() const { return session_->observation(); }
  bool full() const { return session_->full(); }
  bool animated() const { return session_->animated(); }
  const combat_runtime::World &world() const { return session_->world(); }
  std::vector<combat_play::Actor> actors() const { return session_->actors(); }
  const std::string &own_creature_class() const { return session_->own_creature_class(); }
  const std::vector<combat_play::VisualEvent> &visual_events() const { return session_->visual_events(); }
  const play_animation::Frame &animation_frame(unsigned slot) const { return session_->animation_frame(slot); }
  const std::vector<combat_runtime::Event> &tick_events() const { return session_->tick_events(); }
  Raw shared_seed() const { return session_->shared_seed(); }
  // Synthetic Full-fixture observation probe: make walk life distinct for one
  // read, then restore it without advancing or recording a journal action.
  Raw probe_walk_life_zero(const std::function<Raw()> &read_exposed);

  // Journaled operations (each forwards to combat_play::Session / Effects and
  // rethrows its refusal after recording it).
  session_control::Observation advance(std::uint32_t command);
  void hit_event(unsigned slot, Raw mask);
  void press_attack(bool held);
  void release_attack();
  void set_block(bool held);
  void cast_holy_strike();
  void select_disease_bolt(unsigned rank = 1);
  void press_disease_bolt(bool held);
  void release_disease_bolt();
  // Wave-12 Full-fixture extensions (combat_play::Session).
  void spawn_creature(unsigned own_class); // index into combat_play::OWN_CLASSES
  void spawn_mounted_horseman(unsigned variant);
  void drop_potion(unsigned kind);
  void release_input();
  void set_paused(bool);
  void stop();
  // Full only: the per-machine world EffectSimulation draws into (nullptr: headless).
  void attach(fxsim::EffectWorld *world);
  void draw_slots();

  // presentation: the caller's snapshot of the attached per-machine state, if any.
  Bytes save(const Bytes *presentation = nullptr) const;
  struct Loaded {
    std::unique_ptr<CombatScene> scene;
    std::optional<Bytes> presentation;
  };
  static Loaded load(const Bytes &bytes, const std::string &profile,
                     std::shared_ptr<const fxskills::RecipeConstants> constants = {},
                     ConstantsIdentity identity = {});

  struct Action {
    std::uint8_t op = 0, refused = 0;
    std::uint32_t value = 0;
  };

private:
  world_queries::Snapshot terrain_;
  combat_play::Fixture fixture_;
  ConstantsIdentity identity_;
  std::unique_ptr<combat_play::Session> session_;
  std::unique_ptr<combat_play::Effects> effects_;
  std::vector<Action> journal_;
  // Replay only: stands in for the attached world while the journal says one was.
  std::unique_ptr<fxsim::EffectWorld> stand_in_;
  bool replaying_ = false;
  template <class F> auto record(std::uint8_t op, std::uint32_t value, F &&f) -> decltype(f());
  void apply(const Action &);
  Bytes witness() const;
};

class LevelScene {
public:
  LevelScene();
  ~LevelScene();
  LevelScene(const LevelScene &) = delete;
  LevelScene &operator=(const LevelScene &) = delete;

  const level_play::Session &session() const { return *session_; }
  std::size_t actions() const { return journal_.size(); }
  session_control::Observation observation() const { return session_->observation(); }
  level_play::Flow flow() const { return session_->flow(); }
  world_queries::Snapshot terrain() const { return session_->terrain(); }

  session_control::Observation advance(std::uint32_t command);
  void open_gate();
  void confirm_gate(const std::string &destination);
  void request_level(const std::string &destination);
  void release_input();
  void set_paused(bool);
  void stop();

  Bytes save() const;
  static std::unique_ptr<LevelScene> load(const Bytes &bytes, const std::string &profile);

  struct Action {
    std::uint8_t op = 0, refused = 0;
    std::uint32_t value = 0;
    std::string text;
  };

private:
  std::unique_ptr<level_play::Session> session_;
  std::vector<Action> journal_;
  template <class F> auto record(Action a, F &&f) -> decltype(f());
  void apply(const Action &);
  Bytes witness() const;
};
} // namespace champions::simulation::rta::fixture_save
