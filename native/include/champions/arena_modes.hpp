#pragma once

#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// RTA US v2.00 arena and versus rules. Authored from research
// t3/research-arena 17810bb0f9a3474fbeae9e86d339cb72ccd0f154
// (docs/arena-modes.md, tools/ref_arena_modes.py). Floats are raw patterns.
// Recipe bytes and the two level-name tables are caller arguments. The
// research reads the recipes from the executable and does not publish them;
// it prints the names, but this tree does not copy level file names (the
// same rule as level_flow's ArenaRotationTable). An owner reads both from
// its own executable at run time.
namespace champions::simulation::rta::arena_modes {

class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class Unsupported : public Error {
public:
  using Error::Error;
};
class DivideByZero : public Error {
public:
  using Error::Error;
};

inline constexpr Raw K_180 = 0x43340000u;
inline constexpr Raw K_36 = 0x42100000u;
inline constexpr Raw K_240 = 0x43700000u;
inline constexpr Raw K_259 = 0x43818000u;
inline constexpr Raw K_290 = 0x43910000u;
inline constexpr Raw K_M500 = 0xc3fa0000u;
inline constexpr Raw K_320 = 0x43a00000u;
inline constexpr Raw K_STEP = 0x3cf5c28fu;
inline constexpr Raw K_CLIMB = 0x3c2af07fu;
inline constexpr Raw K_FADE_STEP = 0x3c61029bu;
inline constexpr Raw K_LEVEL = 0x3f2bc579u;
inline constexpr Raw K_FIVE = 0x40a00000u;
inline constexpr Raw K_QUOT = 0x3f814738u;
inline constexpr Raw K_REM = 0x3f80a393u;
inline constexpr Raw K_TWO_HALF = 0x40200000u;
inline constexpr Raw K_1_30 = 0x3d088888u;
inline constexpr Raw K_MOVE_NET = 0x3fa00000u;
inline constexpr Raw K_MOVE_OFF = 0x3fe00000u;
inline constexpr int LIVES_START = 2;
inline constexpr int INTRO_FRAMES = 0xb4;
inline constexpr int INTRO_CUE = 0x84;
inline constexpr int INTERMISSION_OFFLINE = 0x1e0;
inline constexpr int INTERMISSION_ONLINE = 0x3c0;
inline constexpr int DEAD_COUNTDOWN = 0xf0;
inline constexpr int MODE_A_MAPS = 13;
inline constexpr int MODE_B_MAPS = 7;
inline constexpr int SCORE_SLOTS = 4;
inline constexpr uint16_t TINT[4] = {0x56, 0x35, 0x21, 0x3d};

// What a session stores in 0x4f7124 is unknown. pick_spawn takes the word as
// an input; this refuses to invent the store.
[[noreturn]] void session_facing_word();
// Net message +0xf4 is unknown.
[[noreturn]] void net_message_peer();
// 0x3065fc was executed only as the call (class pointer, argument 0).
[[noreturn]] void monster_builder();
// arena_nextWave's body was not run. The task records the call.
[[noreturn]] void next_wave_body();
// SpellArena particle positions were stubbed. The particle run is combat
// round 6 on t3/research-combat.
[[noreturn]] void spell_arena_particles();
// Empty-wave callback 0x3069e8 was not run.
[[noreturn]] void empty_wave_callback();
// Draw helper 0x307bbc was not run.
[[noreturn]] void draw_helper_countdown();
// Whether every lockstep machine runs the draw task was not settled.
[[noreturn]] void draw_task_lockstep();
// player_respawn was stubbed. The arena caller records spread 2 and radius 3.
[[noreturn]] void player_respawn_body();
// arena_shutdown was stubbed. The confirm path records the call.
[[noreturn]] void shutdown_body();
// The saved-record byte layout was not part of this research.
[[noreturn]] void saved_record_bytes();

struct Draw {
  std::uint32_t seed = 0;
  std::uint16_t value = 0;
};
Draw lcg(std::uint32_t seed);
std::pair<std::int32_t, std::int32_t> sdiv(std::int32_t n, std::int32_t d);
int high_nibble(std::uint8_t byte);
int low_nibble(std::uint8_t byte);
// One of the two null-terminated pointer tables: 13 names at 0x5037b8 (mode
// A) and 7 at 0x503798 (mode B). A table of another size, or an index outside
// it, refuses.
using NameTable = std::vector<std::string>;
const std::string &mode_a_name(const NameTable &, int index);
const std::string &mode_b_name(const NameTable &, int index);

struct Slot {
  std::uint16_t kills = 0;
  std::uint16_t lives = 0;
  std::uint16_t snapshot = 0;
  Raw timer = ZERO;
};
struct Killer {
  std::uint32_t flags = 0;
  int slot = 0;
  int owner = 0;
};
enum class KillResult { Ignore, Suicide, Kill, Life };

std::uint32_t reset_scores(Slot *slots, int count, bool networked);
void lose_life(Slot &slot, bool mode_a, bool prediction = false);
void add_kill(Slot *slots, int count, const Killer *killer, std::uint32_t amount,
              bool mode_a, int num_players);
KillResult on_killed(Slot *slots, int count, int slot, const Killer *killer,
                     bool mode_a, int num_players, bool prediction = false);
void add_life(Slot &slot, bool mode_b, bool mode_a);
std::uint32_t grant_life(Slot &slot);
bool respawn_allowed(const Slot &slot, bool mode_a, bool active, int intermission);

void tick_overlay(Slot *slots, int count);
Raw overlay_t(Raw timer, int step);
bool overlay_draws(Raw timer, int step);
Raw overlay_anchor_x(int screen_x, Raw current);
Vec3 hermite(Raw t, const Vec3 &p0, const Vec3 &p1, const Vec3 &m0, const Vec3 &m1);
Raw fade_step(Raw value);

struct RingOff {
  int angle = 0;
  Raw dx = 0;
  Raw dy = 0;
};
RingOff ring_delta(int slot, int count, bool mode_a);

struct SpawnPoint {
  Raw x = 0;
  Raw y = 0;
  Raw z = 0;
  std::uint16_t angle = 0;
};
struct Spot {
  Vec3 pos{};
  int ok = 0;
};
// world_findFreeSpot. It draws from the shared generator (g_randSeed
// 0x404464), the same word the arena draws use, so the search receives the
// seed and leaves it where its own draws end.
using Search = std::function<Spot(const Vec3 &centre, int spread, int radius,
                                  std::uint32_t &seed)>;
struct Pick {
  Vec3 pos{};
  int facing = 0;
  std::uint32_t seed = 0;
};
Pick pick_spawn(int flag, int slot, const SpawnPoint *points, int point_count,
                std::uint32_t seed, bool mode_a, bool scramble, bool network,
                const Vec3 &focus, int active, const Search &search,
                const Vec3 &camera);

Raw monster_level(int wave);
struct Budget {
  int count = 0;
  int quot = 0;
  int rem = 0;
};
Budget spawn_budget(int wave, int active);
using Recipe = std::array<std::uint8_t, 6>;
std::vector<int> recipe_groups(const Recipe &record, int rem, int count);
struct RingStep {
  int count = 0;
  std::uint32_t angle = 0;
  std::uint16_t draw = 0;
};
std::pair<std::uint32_t, RingStep> ring_step(std::uint32_t seed, int count);
struct WavePlan {
  bool has_classes = false;
  std::vector<int> classes;
  std::uint32_t seed = 0;
  std::vector<RingStep> steps;
};
WavePlan wave_plan(int wave, int active, const Recipe *records, int record_count,
                   std::uint32_t seed, int saved = 0);
int level_record_index(int wave, int record_count);
int mode_a_next_index(int index, int scan0);
int kill_choice(int menu_index);
int compare_kills(int slot_a, int slot_b, int kills_a, int kills_b);
bool kill_limit_reached(int kills, int limit);
int intermission_limit(bool networked);
bool intermission_done(int counter, bool networked);
enum class Round { Level, Wave };
Round round_expired(int waves_left, bool mode_a);
bool still_in(bool active, int lives, Raw hp, int countdown);
bool can_play_intermission(bool active, int lives, Raw hp);
struct Purchase {
  std::int32_t gold = 0;
  std::int32_t budget = 0;
  bool bought = false;
};
Purchase buy_life(std::int32_t gold, std::int32_t budget);
Raw armour_bound(Raw bound, int armor);
Raw pvp_half(Raw amount);
bool spell_arena_flag(int counter);
bool spell_arena_tint(int counter);
struct SpellArena {
  std::uint32_t flags = 0;
  bool tint = false;
  bool particles = false;
};
SpellArena spell_arena(int counter, std::uint32_t flags);

struct Glyph {
  int k = 0;
  Raw t = 0;
  Vec3 p0{};
  Vec3 p1{};
  Vec3 m0{};
  Vec3 m1{};
  Vec3 out{};
  int colour = 0;
  int figure = 0;
  int font = 0;
};
struct Overlay {
  bool drawn = false;
  std::vector<Glyph> glyphs;
  Raw anchor_x = K_259;
};
Overlay draw_hud(bool mode_b, bool mode_a, bool frontend, bool menu,
                 const Slot *slots, int count, const int *screen_x,
                 const int *screen_y, const int *anchor_x, const int *anchor_y);

struct Visit {
  std::uint32_t seed = 0;
  bool called = false;
};
// arena_visitCreature 0x306af0. A player (0x40) takes no draw; a non-player
// creature (0x20) takes one. An object with neither flag was not executed
// and refuses.
Visit visit_creature(std::uint32_t flags, std::uint32_t seed);

// The run task and the functions it calls. Mode B must be set for init, begin
// and tick; start_wave itself follows the documented body.
struct WaveOut {
  int wave = 0;
  int waves_left = 0;
  Raw monster_level = 0;
  bool reset = false;
  bool used_recipe = false;
  bool saved_respawn = false;
  std::vector<int> classes;
  std::vector<RingStep> steps;
  int spawn_calls = 0;
  int spawn_argument = 0;
  std::uint32_t seed = 0;
  int intro = 0;
  std::uint32_t match_word = 0;
  bool close_menus = false;
  bool camera_reset = false;
};
struct TickOut {
  bool skipped = false;
  bool intro_lock = false;
  bool intro_cue = false;
  bool intro_latch = false;
  bool level_change = false;
  bool next_wave = false;
  int next_wave_arg = 0;
  bool end_prompt = false;
  bool shutdown = false;
  bool session_exit = false;
  bool granted = false;
  int granted_slot = -1;
  bool empty_wave_refused = false;
  bool close_menus = false;
  bool overlay_ticked = false;
  int restores = 0;
  bool rank_refused = false;
};
struct RespawnOut {
  bool ran = false;
  Vec3 pos{};
  int facing = 0;
  bool screen = false;
  int spread = 0;
  int radius = 0;
  int visits = 0;
  int virtual_calls = 0;
  std::uint32_t seed = 0;
};

struct Match {
  bool mode_a = false;
  bool mode_b = false;
  bool networked = false;
  bool paused = false;
  bool scramble = false;
  int num_players = 1;
  int active_count = 1;
  std::array<Slot, SCORE_SLOTS> slots{};
  std::array<bool, SCORE_SLOTS> active{};
  std::array<Raw, SCORE_SLOTS> hp{};
  std::array<Raw, SCORE_SLOTS> max_hp{};
  std::array<Raw, SCORE_SLOTS> mana{};
  std::array<Raw, SCORE_SLOTS> max_mana{};
  std::array<std::int32_t, SCORE_SLOTS> gold{};
  std::array<std::uint16_t, SCORE_SLOTS> dead{};
  std::array<std::uint32_t, SCORE_SLOTS> hud{};
  std::uint16_t wave = 0;
  std::uint16_t waves_left = 0;
  std::uint16_t intermission = 0;
  std::uint16_t intro = 0xffff;
  std::uint32_t end_flag = 0;
  Raw fade = ZERO;
  std::uint32_t frame = 1;
  int kill_limit = 0;
  std::int32_t budget = 0;
  int buyer_count = 0;
  std::array<int, SCORE_SLOTS> buyers{};
  Raw monster_level_value = 0;
  std::uint32_t movement = 0;
  std::uint32_t match_word = 0;
  std::uint32_t lock_a = 0;
  std::uint32_t lock_b = 0;
  std::uint32_t seed = 0;
  int map_index = 0;
  // game_scanCm1Vars(0). Its value in a live session is not in this research;
  // a mode-A step that needs it refuses while it is empty.
  std::optional<int> scan0;
  int saved_monsters = 0;
  std::vector<Recipe> recipes;
  NameTable names_a;
  NameTable names_b;
  // arena_rankSlots qsorts with arena_compareKills, which is not a strict
  // order for equal kills. A tie leaves rank_order empty and rank_known false.
  std::vector<std::uint8_t> rank_order;
  bool rank_known = false;
  int exit_code = 0;
  std::string pending_name;
  bool pending_copied = false;
  int pending_orientation = 0;
  int local_slot = 0;
  std::uint8_t spawn_count = 0;
  std::vector<SpawnPoint> points;
  Vec3 focus{};
  Vec3 camera{};
  Search search;

  Match() { active.fill(true); }
  void init();
  // arena_begin 0x304f5c: waves-left 3, then arena_startWave(1). Returns that
  // wave's output; an empty output while mode B is clear.
  WaveOut begin();
  WaveOut start_wave(int delta);
  TickOut tick(bool confirm = false, bool cancel = false);
  // arena_openEndPrompt 0x307ebc: rank, then fade 0. It does not store the
  // end flag; the mode-B callers store it before the call.
  void open_end_prompt();
  std::string level_name() const;
  void register_spawn(int index, const SpawnPoint &point);
  RespawnOut respawn(int slot, const std::uint32_t *creature_flags, int creatures);
};

} // namespace champions::simulation::rta::arena_modes
