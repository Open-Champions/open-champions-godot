#pragma once

#include "champions/creature_hp.hpp"
#include <array>
#include <functional>
#include <memory>
#include <vector>

// RTA US v2.00 only. Authored from research-arena-match d4f58da:
// arena-modes.md, arena-match-core.md and arena-match-probe.md round 11.
// R5 request stages: research-arena-live@9a0c26a5 docs/spec/arena-live-scheduler.md.
// Task/constructor/UI call-outs require an explicit host; no retail recipes,
// classes, names, saved records or rewards are embedded here.
namespace champions::simulation::rta::arena_match {
using Address = std::uint32_t;
using Memory = creature_hp::Memory;
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};
inline constexpr Address SCORE = 0x503898, NUM_PLAYERS = 0x4d8ae0,
    PLAYERS = 0x4d8af0, MODE_A = 0x4f711c, MODE_B = 0x4f7120,
    NETWORK = 0x402791, WAVE = 0x8e7338, WAVES_LEFT = 0x8e733a,
    INTERMISSION = 0x8e733c, LIVE_COUNT = 0x8e733e, SAVED_COUNT = 0x8e7340,
    INTRO = 0x5038c8, END = 0x503894, FADE = 0x8e735c,
    FRAME = 0x44fdcc, PAUSED = 0x41b4f4, COUNTDOWN = 0x4f84a8,
    KILL_LIMIT = 0x4f8408, RANK = 0x8e7348, EXIT = 0x41b508;
inline constexpr unsigned RECORD_SIZE = 0x4c30, SCORE_STRIDE = 12;

bool blocks_wave(Raw flags, Raw state);
enum class SaveAction { Skip, Save, Destroy };
SaveAction save_action(Raw flags, Raw state);
int compare_kills(unsigned a, unsigned b, Raw kills_a, Raw kills_b);
Raw monster_level(Raw wave);
struct Group { int class_index = 0, count = 0; };
using Recipe = std::array<std::uint8_t, 6>;
// Only the recipe's arithmetic. Construction, placement and their RNG draws
// are separate services. Zero-size groups and invalid table indices refuse.
std::vector<Group> wave_groups(Raw wave, int active,
                              const std::vector<Recipe> &recipes);
int next_level_record(Raw wave, int recipe_count);

// research-arena-live@9a0c26a5 docs/spec/arena-live-scheduler.md:16-34,41-47.
// Runtime-owned pointers, never shipped names/data. A request is not a created
// creature, completed startWave, rendered result or successful level load.
struct RequestTables {
  std::vector<Recipe> recipes;
  std::vector<Address> classes;
  std::vector<Address> levels;
};
struct SpawnGroup { Address class_pointer; int count; };
struct WaveRequest {
  Raw increment, wave, waves_left, monster_level;
  std::vector<SpawnGroup> groups;
};
struct RequestServices {
  RequestTables tables;
  std::function<int()> active_count;
  std::function<Address(Address)> preload_factory;
  // Explicit boundary for the player/resource portion of startWave. This may
  // consume the shared seed; a fixture must identify itself as such.
  std::function<void(const WaveRequest &)> prepare_players;
  // Original 0x3065fc requests; callback return does not prove construction/ticks.
  std::function<void(Address class_pointer, Address saved_object)> spawn;
  std::function<void()> camera_reset;
  std::function<void(Address destination, Address source)> copy_level;
  std::function<void(Raw)> close_menus;
};

struct Services {
  // Explicit opt-in to native request selection. Absent tables never fall back
  // to invented recipes; the existing complete-body services remain supported.
  std::shared_ptr<const RequestServices> requests;
  // world_forEachGridObject 0x14a610, in original grid/list order.
  std::function<void(const std::function<void(Address)> &)> walk;
  std::function<bool(unsigned)> active;
  std::function<void()> prepare;             // 0x19f018
  unsigned saved_capacity = 0;               // host array bound, not an RTA field
  std::function<void(Address)> move_off_grid; // 0x14ef48
  // The audited callback establishes slot 1's target/self, not its extra
  // destructor arguments. Do not default to a different task's flags.
  std::function<void(Address)> destroy;
  std::function<void(Raw)> screen_effect;     // 0x322954
  std::function<void(Raw)> start_wave;        // 0x305ed4 (not a fake spawn)
  std::function<void()> intro_cue;
  std::function<void()> advance_level;        // resolve tables + pending buffer
  std::function<std::vector<std::uint8_t>(unsigned)> saved_record;
  std::function<void()> shutdown;
  std::function<void()> restart_level;        // arena_levelName -> pending buffer
};

class Model {
public:
  explicit Model(Memory &memory, Services services = {});
  void reset_scores();
  void add_kill(Address killer, Raw amount);
  void lose_life(unsigned slot, bool prediction = false);
  void on_player_killed(Address victim, Address killer);
  void add_life(unsigned slot);
  void tick_overlay();
  void draw_prompt_step(); // fade arithmetic only, no displayed UI claim
  void open_prompt();     // ranking; unestablished multi-slot ties refuse
  void prepare_wave();
  void begin(); // 0x304f5c: preload + three-wave setup + start request
  void start_wave(Raw increment); // bounded native request stages, explicit hosts
  void next_wave(Raw increment);
  // Native RequestServices supplies the researched scheduler stages; omitted
  // engine work is still explicit. Legacy complete-body services remain valid.
  // Neither path by itself establishes live match acceptance.
  void visit(Raw pressed = 0, Raw confirm = 0, Raw cancel = 0);

private:
  Memory &mem_;
  Services services_;
  Raw get(Address a, unsigned n = 4) const;
  void put(Address a, Raw value, unsigned n = 4);
  unsigned players() const;
  Address score(unsigned slot) const;
  bool still_in(unsigned slot, bool intermission) const;
  WaveRequest wave_request(Raw increment, Raw left, Raw saved) const;
  std::vector<Address> preload_requests() const;
  Address level_request(bool restart) const;
  void require_start_wave() const;
};
} // namespace champions::simulation::rta::arena_match
