#pragma once
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// RTA US v2.00 only. Authored composition of pinned executed contracts.
// No Session, Godot, save, controller or original-player-loop dependency.
namespace champions::simulation::rta::combat_runtime {
using Address = std::uint32_t;
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Unsupported : Error {
  using Error::Error;
};
inline constexpr Address SHARED_SEED = 0x404464, FRAME = 0x44fdcc,
                         PLAYERS = 0x4d8af0, RECORD_SIZE = 0x4c30,
                         NUM_PLAYERS = 0x4d8ae0, TARGET = 0x234,
                         LAST_ATTACKER = 0x2e4, VT_CREATURE = 0x50cc58,
                         VT_PLAYER = 0x511308, VT_SKELIGHT = 0x5174b0;
struct Event {
  std::string name;
  std::vector<std::uint64_t> args;
};
struct Spawn {
  Vec3 position{};
  Raw hp = 0x42c80000, home_range = 0x44160000;
  std::uint32_t facing = 0;
  std::int32_t team = -1, armor = 0, minimum = 4, maximum = 8, attack = 100;
  std::uint64_t creature_flags = 0x100000;
};
struct Slide {
  Vec3 position{};
  Raw hit = 0;
  Vec3 normal{};
};
struct Services {
  std::function<bool()> dialog_is_open;
  std::function<Raw()> jumpgate_travel_state;
  // Required for reachable terrain/animation services. No silent zero defaults.
  std::function<Raw(const Vec3 &, const Vec3 &, Raw, Raw)> raycast;
  std::function<Slide(const Vec3 &, const Vec3 &, Raw, Raw)> slide;
  std::function<Raw(const Vec3 &, Raw)> height_below;
  std::function<Raw(const Vec3 &, Raw)> water_test;
  std::function<Raw(Address, Raw)>
      animation_advance; // actor, speed -> mask (hit=1)
  std::function<void(Address, Raw, Raw, Raw, Raw)> animation_start;
  std::function<Raw(const std::string &, Raw, Raw)> skill_stat;
};
struct Arc {
  Raw facing = 0, angle = 6000, minimum = 4, maximum = 8, attack = 100,
      flags = 8;
  Raw near = 0, far = 0x42400000;
};
// Owns one pool, one handle graph and one memory. Address results are
// transient; keep a handle for anything retained across calls/ticks or
// allocation reuse.
class World {
public:
  struct State;

private:
  std::unique_ptr<State> s_;

public:
  explicit World(Services services, int camera_mode = 1);
  ~World();
  World(const World &) = delete;
  World &operator=(const World &) = delete;
  const creature_hp::Memory &memory() const;
  std::uint64_t read(Address, unsigned size = 4) const;
  // Explicit authored setup, not an original constructor. Raw handle/node
  // writes refuse.
  void write(Address, unsigned size, std::uint64_t);
  Address spawn_skelight(const Spawn & = {});
  Address spawn_player(unsigned slot, const Spawn &, int strength = 20,
                       int level = 1);
  bool contains(Address) const;
  std::vector<Address> actors() const;
  Address retain(Address); // creates an owned external handle head
  void relink(Address head, Address target);
  void clear_handle(Address head);
  Address target(Address head) const;
  void destroy(Address);       // bounded resource-free Skelight teardown only
  void delete_object(Address); // unlink run list now; grid/handles persist
  void update_grid(Address);
  void run_every_frame(Address);
  void run_every(Address, unsigned period); // 15 or 60
  std::vector<Address> collect_box(const Vec3 &, const Vec3 &, int limit);
  void run_objects(); // flush, active list, frame%15, frame%60; does not
                      // increment frame
  void tick(); // visibility, run_objects, frame advance, alive list; party wipe
               // refuses
  void take_damage(Address victim, Address attacker, Raw flags, Raw amount);
  int damage(Address victim, Address attacker, Raw minimum, Raw maximum,
             Raw attack, Raw flags);
  int enemy(Address, Address);
  void pick_target(Address, bool force = false);
  int can_see(Address, Address, Raw facing, Raw arc, Raw ray_mode, Raw near,
              Raw far, Raw lead = 0);
  int hit_target_in_arc(Address, const Arc &);
  int hit_all_in_arc(Address, const Arc &);
  Address find_enemy_in_cone(Address, Raw facing, Raw arc, Raw near, Raw far);
  void process_damage(Address);
  void player_find_target(Address, Raw reach);
  void player_melee_hit(Address, Address, Raw extra = 0, Raw scale = 0);
  void apply_knockback(Address, Raw scale);
  Raw move_creature(Address, const Vec3 &);
  void dead_update(Address, Raw events, Raw hold, Raw size);
  // One supplied event batch is consumed only when this actor animates in its
  // run. Persistent pending events are held through handles and discarded on
  // inactivity.
  void animation_events(Address, Raw mask);
  const std::vector<Event> &events() const;
  void clear_history();
  Raw slot_function(Address, unsigned slot) const;
  [[noreturn]] void refuse(const std::string &feature) const;
};
// Explicit synthetic fixtures, never the production default.
Services flat_fixture_services();
} // namespace champions::simulation::rta::combat_runtime
