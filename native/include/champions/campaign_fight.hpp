#pragma once
#include "champions/arena_executable.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/player_services.hpp"
#include "champions/script_commands.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The first real enemy fight on a campaign level (RTA US v2.00, `valor`).
// One WorldAuthority holds the script host's factory objects, the WarBoar-family
// actors the level factory finished through campaign_actors, and the session's
// Player (the executed player tick, Full composition) with the audited
// real-level services. Everything the research does not establish stays a
// labelled refusal: it is counted, never replaced by an invented value.
namespace champions::simulation::rta::campaign_fight {
using Address = combat_runtime::Address;

struct EnemyView {
  Address address = 0;
  bool small = false, dead = false, ai_frozen = false;
  Raw hp = 0, max_hp = 0, facing = 0;
  std::uint32_t state = 0;
  Vec3 position{};
};
struct PlayerView {
  bool present = false;
  Vec3 position{};
  Raw facing = 0, hp = 0, max_hp = 0;
  std::int32_t action_state = 0;
  std::int32_t weapon_class = 0;
  std::uint32_t gold = 0;
};
struct Counts {
  std::size_t ticks = 0;
  std::size_t actors_built = 0;
  std::size_t attack_edges = 0;      // J presses fed to the player's pad
  std::size_t player_swings = 0;     // ticks the player was in a melee state
  std::size_t player_hits = 0;       // damage_apply calls with the player as attacker
  std::size_t damage_events = 0;     // of those, results that dealt damage
  std::size_t deaths = 0;            // enemy death transitions
  std::size_t loot_calls = 0;        // Creature_dropLoot -> loot_generateDrops calls
  std::size_t factory_runs_skipped = 0;
};

class Fight {
public:
  // `bundle` must outlive the Fight. `dialog_open` reports the script dialog.
  Fight(std::shared_ptr<WorldAuthority>,
        std::shared_ptr<const arena_modes::Executable>, player_services::Bundle &,
        std::function<bool()> dialog_open);
  ~Fight();
  Fight(const Fight &) = delete;
  Fight &operator=(const Fight &) = delete;
  // Installs the boar-family class port on the host. Call before the host's
  // adopt_objects; the host's resources must already hold the level's lumps.
  void install(script_commands::Host &host);
  // The player's animation lump (class model definition) name for a player
  // model index 0..11, read from the executable; empty when unavailable.
  static std::string player_animation_lump(const arena_modes::Executable &, unsigned model_index);
  struct PlayerStart {
    Vec3 position{};
    Raw facing = 0, hp = 0;
    unsigned model_index = 0;
    // The base level name (g_levelName) whose g_levelTable row supplies the
    // kill plane and level flag word the original game_loadLevel stores.
    std::string level_name;
  };
  // After the objects are adopted: adopts the built actors into the combat
  // World and spawns the session player. Returns an empty string on success or
  // a label when the player could not be established.
  std::string start(const PlayerStart &);
  // One ready tick of the shared World. `command` holds the session command
  // bits (POS_X, NEG_X, POS_Y, NEG_Y, DAMAGE-as-J).
  void tick(std::uint32_t command);
  bool started() const;
  bool player_ready() const;
  bool failed() const;
  const std::string &error() const;
  PlayerView player() const;
  std::vector<EnemyView> enemies() const;
  const Counts &counts() const;
  // Labelled refusals by reason (what was needed, and why it stayed refused).
  const std::map<std::string, std::size_t> &refusals() const;
  combat_runtime::World &world();

private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};
} // namespace champions::simulation::rta::campaign_fight
