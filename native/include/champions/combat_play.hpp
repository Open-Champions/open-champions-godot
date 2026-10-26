#pragma once
#include "champions/session_combat_runtime_adapter.hpp"
#include "champions/session_control.hpp"
#include "champions/play_animation.hpp"

namespace champions::simulation::rta::combat_play {
// Authored, opt-in synthetic composition. Not original encounter/input policy.
inline constexpr const char *POLICY = "rta-combat-fixture-v1";
inline constexpr const char *ANIMATED_POLICY = "rta-animated-combat-fixture-v1";
// Wave 11: the released Composition::Full world (real Player_run, movement,
// melee/combos, blocking, projectiles, Holy Strike). Authored fixture set-up
// and controller masks; not an original encounter or button layout.
inline constexpr const char *FULL_POLICY = "rta-full-combat-fixture-v1";
enum class Fixture : std::uint8_t { Combat, Animated, Full };
// Client controller mapping for the Full fixture (combat_runtime leaves the
// masks to the caller; these are the research fixture's authored words).
inline constexpr Raw ATTACK_MASK = 0x10, BLOCK_MASK = 0x20;
inline constexpr Raw ABILITY_A_MASK = 0x40; // authored Full-fixture button
// Holy Strike skill id (combat_runtime World::give_skill / cast).
inline constexpr int HOLY_STRIKE = 16;
// The own-state-machine classes World::spawn_class runs (creature-ai), in
// the contract's order. spawn_creature journals the index.
inline constexpr const char *OWN_CLASSES[] = {
    "Critter", "Soul", "NPC", "Natasla", "Mermaid",
    "GnomeNavigator", "Skeleton", "Skelight", "Gnaw", "AntQueen"};
inline constexpr unsigned OWN_CLASS_COUNT = 10;
struct Actor {
  unsigned slot;
  combat_runtime::Address handle;
  bool present, dead;
  Vec3 position;
  Raw hp, state;
};
struct VisualEvent {
  bool death = false;
  unsigned slot = 0;
  Vec3 position{};
};
class Session {
  struct State;
  std::unique_ptr<State> s_;
public:
  explicit Session(const world_queries::Snapshot &, bool animated = false);
  Session(const world_queries::Snapshot &, Fixture);
  bool animated() const;
  Fixture fixture() const;
  bool full() const;
  const play_animation::Frame &animation_frame(unsigned slot) const;
  ~Session();
  session_control::Observation advance(std::uint32_t command);
  session_control::Observation observation() const;
  std::vector<Actor> actors() const;
  // Owned observations from this ready tick; no FX policy or RNG in simulation.
  const std::vector<VisualEvent> &visual_events() const;
  // Explicit animation event fixture, not an attack-selection/action switch.
  void hit_event(unsigned slot, Raw mask);
  void release_input();
  void set_paused(bool);
  void stop();
  Raw shared_seed() const;

  // ---- Full fixture -------------------------------------------------------
  // Movement commands (session_control POS_X..NEG_Y) become the player's
  // stick on the next ready tick. Attack: a pressed edge on the next ready
  // tick, optionally held afterwards (research Runtime.press_attack). Block:
  // held while set; its press edge is made on the first held tick.
  void press_attack(bool held = true);
  void release_attack();
  void set_block(bool held);
  void cast_holy_strike(); // made in the player's next run (World::queue_cast)
  void select_disease_bolt(unsigned rank = 1); // authored Full-fixture quick-slot A
  void press_disease_bolt(bool held = true); // original Player_run pad path
  void release_disease_bolt();
  // ---- Wave-12 client extensions of the Full fixture (authored set-up) ----
  // Each runs merged executed bodies; placement and set-up values are the
  // fixture's own authored ones, not original spawn data. A refusal leaves
  // the session unchanged when it happens before the World is touched.
  // spawn_creature: one World::spawn_class own-machine creature (creature-ai)
  // in slot 3, at (0, 80) facing the player, on the fixture's 120-HP authored
  // placement. Once per session. Only Skelight and Gnaw (Gnaw_ctor's class
  // words) can be built; the other eight classes refuse because their
  // constructors were not executed.
  void spawn_creature(const std::string &own_class);
  std::array<combat_runtime::Address, 2> spawn_mounted_horseman(
      unsigned variant, const combat_runtime::Spawn &rider,
      const combat_runtime::Spawn &mount);
  const std::string &own_creature_class() const; // empty until spawned
  // drop_potion: a thrown potion of kind 1..3 (combat-runtime-4
  // playerProjectile_run) at the player's position with the Thrown defaults
  // and zero velocity. The throw call site and its arc are not contracted.
  void drop_potion(unsigned kind);
  // Holy Shield is armed on the Holy Strike skill already in the table
  // (arm_fixture_holy_shield). give_skill(16) is not called again. The
  // player's model at +0x544 has one sample point and +0x114 holds a
  // one-joint pose, so absorb follows the round-5 sample points.
  // The World is the sole actor owner. Read access for observers; the
  // mutable reference exists only for explicit effect hooks (damage/queries).
  const combat_runtime::World &world() const;
  combat_runtime::World &world();
  // This ready tick's World trace (cleared at the start of each ready tick).
  const std::vector<combat_runtime::Event> &tick_events() const;
  // Called once inside every successful ready tick, after the priority-10
  // World pass and before the shared seed is published to the network
  // machine. Effects use it to run on every machine, drawing or not.
  using TickObserver = std::function<void(Session &)>;
  void set_tick_observer(TickObserver);
};
}
