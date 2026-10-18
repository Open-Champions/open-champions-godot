#pragma once
// RTA SLUS-20973 v2.00 only. Evidence: tests/level_flow_contract.json.
#include "champions/archive.hpp"
#include "champions/world_queries.hpp"
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::level_flow {
using Vec3 = world_queries::Vec3;
using Blob = std::vector<std::uint8_t>;
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct MissingResource : Error {
  using Error::Error;
};
struct Unsupported : Error {
  using Error::Error;
};
std::pair<std::string, std::string> split_request(const std::string &);
std::string qualified_name(const std::string &, const std::string &);
std::string cd_file_name(const std::string &);
std::string loading_picture_alias(const std::string &);
std::vector<std::string> loading_picture_files(const std::string &);
struct LevelRow {
  std::string name;
  std::int8_t monster_level = 1, b05 = 2;
  Raw kill_plane = 0xc7c35000, f0c = 0xc7c35000, f10 = 0x47c35000;
  std::uint32_t water_pool = 48;
  std::int8_t b18 = 0;
  Raw f1c = ONE;
  std::uint32_t flags = 0;
  std::array<std::int16_t, 4> sound{};
  std::uint16_t w2c = 0;
};
LevelRow find_level_row(const std::vector<LevelRow> &, const std::string &,
                        const std::string &);
Raw monster_level(const LevelRow &, int difficulty, const std::string &base,
                  bool pvp = false);
struct Resource {
  std::string step, name, where;
  bool required;
};
struct ManifestInput {
  std::string base, sub;
  std::vector<std::string> gob_lumps, level_entries, model_lumps, disc_files,
      loaded_lumps;
  bool hud_loaded = false, recall_leg = false, pvp = false;
};
// level_entries are in directory order (after lump_fixup, which drops entries
// but never reorders them). Step 8 is the first `.world` in that order.
std::vector<Resource> level_manifest(const ManifestInput &);
// lump_findNthByExtension 0x171fd8: directory order, text after the last '.',
// exact case-sensitive comparison; names without '.' are skipped. n=0 is the
// first match. No sorting.
std::optional<std::string>
first_entry_with_extension(const std::vector<std::string> &,
                           const std::string &ext, unsigned n = 0);
const Entry *first_entry_with_extension(const std::vector<Entry> &,
                                        const std::string &ext,
                                        unsigned n = 0);
// Level flag word (row +0x20, 0x44fe08) -> g_worldLightLevel 0x5d347c: 0x80;
// 0x20000000 tested first gives 0x20, 0x04000000 tested second gives 0x40, so
// both set gives 0x40. A base vertex light level, not an alpha.
inline constexpr std::uint32_t LEVEL_FLAG_LIGHT_20 = 0x20000000,
                               LEVEL_FLAG_LIGHT_40 = 0x04000000;
std::uint32_t base_light_level(std::uint32_t level_flags);
// world_drawChunk 0x189008-0x189070, per chunk flags: 0x100 gives -230, else
// 0x2000 gives +230, else 0x200 gives 0, else dynamic lighting. The returned
// flags have run-time bit 0x100 cleared.
struct ChunkLight {
  bool dynamic = true;
  std::int32_t level = 0;
  std::uint32_t flags = 0;
};
ChunkLight chunk_light(std::uint32_t chunk_flags);
// Runtime GOB reads ignore stored offsets. Unlike inspector readGob().
std::vector<Entry> gob_entries(Bytes);
struct Record {
  std::string cls;
  std::uint16_t angle = 0;
  Vec3 position{};
  std::vector<std::string> props;
  std::optional<std::string> prop(const std::string &) const;
};
std::vector<Record> parse_objects(Bytes);
bool save_filter_admits(const std::string &);
using FindSpot =
    std::function<Vec3(const Vec3 &, Raw spread, Raw radius, bool any_height)>;
using Draw = std::function<std::uint16_t()>; // shared generator's draw >> 16,
                                             // not a new RNG
using Height = std::function<Raw(Raw radius, const Vec3 &)>;
struct SpawnState {
  std::array<Vec3, 4> pos{};
  std::array<std::uint16_t, 4> facing{};
  int rank = 0;
};
std::pair<bool, int> marker_decision(int, const Record &,
                                     const std::string &prev,
                                     const std::string &entrance, bool recall);
std::vector<std::size_t> run_markers(SpawnState &, const std::vector<Record> &,
                                     const std::string &prev,
                                     const std::string &entrance, bool recall,
                                     const FindSpot &, const Draw &,
                                     bool saved = false, bool gate = false,
                                     unsigned players = 4);
struct Placement {
  unsigned players = 4;
  bool level_loaded = true, pvp = false;
  std::optional<std::array<Vec3, 4>> restore_xy;
  std::array<bool, 4> alive{{true, true, true, true}},
      active{{true, true, true, true}};
  std::optional<unsigned> medal_slot;
};
std::vector<unsigned> place_players(SpawnState &, const Placement &,
                                    const Height &, const FindSpot &);
inline constexpr std::uint8_t ORIENT_NONE = 0xff, ORIENT_RESTORE = 0xfd,
                              ORIENT_CONSUMED = 0xfe;
struct Pending {
  std::string name;
  std::uint8_t orientation = ORIENT_NONE;
  void request(const std::string &, std::uint8_t = ORIENT_NONE);
};
std::uint8_t orientation_from_props(const std::vector<std::string> &);
std::pair<bool, int> new_world_gate(bool net, int active, int cooldown,
                                    int alive, bool post = false);
bool cancel_pending(bool loaded, bool frontend, bool card_busy, bool pvp,
                    bool any_alive);
using Act = std::vector<std::pair<std::string, std::uint32_t>>;
using Acts = std::vector<Act>;
int find_act(const Acts &, const std::string &, int current);
struct LevelStates {
  static constexpr std::size_t LIMIT = 0x4bc78 - 0x16118;
  std::vector<std::pair<std::string, std::uint32_t>> entries;
  std::map<std::string, Blob> blobs;
  bool store(const std::string &, const Act &, int card = 0,
             std::uint32_t flags = 0, bool fixed = false,
             const Blob &blob = {});
  std::pair<bool, std::size_t> lookup(const std::string &) const;
};
struct GateList {
  std::vector<std::string> names;
  void reset(const std::string &hub);
  bool add_current(const std::string &qualified, const std::string &hub);
  std::vector<std::string> choosable(const std::string &current) const;
};
struct GatePrompt {
  bool usable = false;
  std::optional<std::string> text;
  bool found = false;
};
GatePrompt gate_prompt(bool found, bool menu, bool script,
                       std::size_t destinations,
                       const std::vector<Raw> &other_alive_d2);
struct GateTravel {
  bool menu_open = false, paused = false, fixed = false,
       request_written = false;
  unsigned ticks = 0;
  std::string destination;
  void open(bool net);
  bool confirm(const std::string &, const std::string &current,
               const GateList &, bool fixed_destination = false);
  bool tick(Pending &); // 180 ready ticks after confirm; never on render frames
  void arrived();       // integration calls on first ready tick after placement
};
struct TriggerPlayer {
  unsigned slot;
  Vec3 position;
};
struct TriggerEvent {
  std::optional<std::string> entry;
  std::optional<unsigned> active;
};
struct TriggerState {
  std::int32_t delay;
  bool inside;
};
struct Trigger {
  Vec3 min{}, max{};
  std::string script;
  bool inside = false;
  std::int32_t delay = 0; // persisted, scheduling owned by Session
  Trigger(Raw x, Raw y, std::int32_t w, std::int32_t h, std::string t);
  bool contains(const Vec3 &) const;
  TriggerEvent run(const std::vector<TriggerPlayer> &, bool recall = false);
  TriggerState save() const;
  void restore(TriggerState);
};
bool push_trigger_allowed(const std::vector<int> &);
struct PushTrigger {
  bool net_host_only = false, shop = false, off_only = false, once = false,
       deleted = false;
  std::vector<std::string> keys, actions;
  bool prompt(bool net, unsigned presser, unsigned first_active,
              const std::function<int(const std::string &)> &) const;
  bool interact(bool another_busy,
                const std::function<void(const std::string &, bool raw)> &);
};
struct PlayerRecord {
  std::int32_t hp = 0, max_hp = 0, cooldown = 0;
  bool active = false;
  std::string recall_dest = "None";
  Vec3 recall_position{};
  Blob persistent; // owner serialization: stats/bags/gold/medals; kept verbatim
  std::vector<std::string> quests;
};
// g_recallCountdown 0x41b51d and player_recallTick 0x1b5fa0, called first in
// gameMiscSync (priority 1) every tick, with no cutscene/pause/freeze test, so
// it keeps counting through cutscenes and pause. Only the 1->0 call reads the
// cutscene flag and alive count, as the previous tick left them. A cutscene
// then, or nobody alive, loses the recall: nothing re-arms it and no potion is
// refunded. Aura: owner starts aura lights. Finish/Dropped*: owner clears them.
inline constexpr std::uint8_t RECALL_TICKS = 60;
enum class RecallEvent { None, Aura, Finish, DroppedCutscene, DroppedNobody };
struct RecallCountdown {
  std::uint8_t value = 0; // tested as s8; any non-zero byte counts down
  void start() { value = RECALL_TICKS; } // recall_execute, game message 3
  bool running() const { return value != 0; }
  RecallEvent tick(bool cutscene_active, std::uint32_t alive_count);
};
// Tick whose call writes the request, for message 3 executed in set_tick: net
// games run messages before the run pass (set_tick + 59); offline runs them
// after player_recallTick or in the draw pass (set_tick + 60).
std::uint64_t recall_request_tick(std::uint64_t set_tick,
                                  bool set_before_run_pass);
struct Transient {
  Blob objects, names, pool, decals, paths, particles, deferred, dialog,
      level_lumps;
  RecallCountdown recall_countdown;
  bool alternate_death = false, save_inhibit = false, paused = false;
};
struct State {
  Acts acts;
  int act = -1, difficulty = 1;
  std::string level, sub, prev, entrance, post_action;
  Pending pending;
  LevelStates states;
  Blob save_buffer; // owner serialization; cleared on an unlatched act change
  std::map<std::string, std::int32_t> quest_values;
  GateList gates;
  GateTravel travel;
  SpawnState spawn;
  std::array<PlayerRecord, 4> players;
  Transient transient;
  bool frontend = false, arrived_by_recall = false, loaded_from_save = false;
  std::optional<std::array<Vec3, 4>> restore_xy;
  std::uint32_t level_flags = 0, sim_frame = 0;
  int ambient_countdown = 0, idle_voice_countdown = 0;
  std::uint8_t orientation_copy = 0;
};
struct ChangeOptions {
  bool death_task = false, card_latch = false, fixed_state_size = false,
       defer_recall_clear = false;
  int card_state = 0;
  std::string hub = "hub";
  Blob old_blob;
};
struct ChangeResult {
  bool stored = false, act_changed = false, reset_act = false,
       had_saved_state = false;
  std::uint8_t orientation = 0;
};
// Preparation through split/reset. Host executes teardown/load/placement in the
// adapter.
ChangeResult
begin_change(State &, const ChangeOptions &,
             const std::function<void(const std::string &)> &event = {});
struct RecallResult {
  std::string request;
  bool arrived_by_recall = false;
  std::optional<std::array<Vec3, 4>> restore_xy, saved;
  std::array<std::string, 4> destinations;
};
RecallResult recall_finish(const std::string &, const std::string &, unsigned,
                           const std::array<Vec3, 4> &,
                           const std::array<std::string, 4> &,
                           const std::string &hub);
// Applies recall_finish when the countdown is not running, outside a
// cutscene and with a live player. Prefer recall_tick, which owns the timing.
bool complete_recall(State &, unsigned players, bool cutscene,
                     const std::array<Vec3, 4> &resolved_positions,
                     const std::string &hub);
// One player_recallTick call. On Finish it applies recall_finish to the state
// (pending request with ORIENT_RESTORE). Call once per simulation tick before
// the tick's objects/scripts/cutscene run, never per render frame.
RecallEvent recall_tick(State &, unsigned players, bool cutscene_active,
                        std::uint32_t alive_count,
                        const std::array<Vec3, 4> &resolved_positions,
                        const std::string &hub);
struct Checkpoint {
  std::string level, sub;
  int act = -1, difficulty = 1;
  SpawnState spawn;
  std::array<PlayerRecord, 4> players;
  std::map<std::string, std::int32_t> quest_values;
  GateList gates;
  LevelStates states;
  Blob camera;
};
std::optional<Checkpoint> checkpoint(State &, const Blob &level_state,
                                     const Blob &camera, int card_state = 0);
enum class Wipe { Wait, Reload, Restart };
Wipe party_wipe(State &, const std::optional<Checkpoint> &);
struct Piece {
  std::int32_t tx = 0, ty = 0, tz = 0;
  std::int16_t cosine = 32767, sine = 0;
  bool mirrored = false, claimed = false;
  std::size_t element = 0;
  // Flag 0x40 of each drawn chunk, first..first+count-1. nullopt: one chunk
  // with `mirrored`. Empty: a count <= 0, which move_piece leaves untouched.
  std::optional<std::vector<bool>> chunk_mirrored;
};
// Drawn chunk fields +0x28/+0x2c/+0x30 (s32 quarter units), +0x36/+0x38.
struct ChunkTransform {
  std::int32_t tx = 0, ty = 0, tz = 0;
  std::int16_t cosine = 0, sine = 0;
  bool operator==(const ChunkTransform &o) const {
    return tx == o.tx && ty == o.ty && tz == o.tz && cosine == o.cosine &&
           sine == o.sine;
  }
};
// world_movePiece 0x182c98, operation by operation, from the piece's rest
// values only (absolute; never accumulates). offset: world-unit patterns;
// angle: low 16 bits, sign-extended. sin16(0) is 2^-22, so angle 0 is not an
// identity: rest x 400 gives 399. Collision is never touched.
std::vector<ChunkTransform> move_piece(const Piece &rest, const Vec3 &offset,
                                       std::int32_t angle,
                                       const std::vector<bool> &chunk_mirrored);
std::vector<ChunkTransform> move_piece(const Piece &rest, const Vec3 &offset,
                                       std::int32_t angle);
Vec3 piece_pivot(const Piece &);
int find_moving_piece(std::vector<Piece> &, const Vec3 &);
void release_piece(std::vector<Piece> &, int);
void disable_unclaimed(const std::vector<Piece> &,
                       const std::function<void(int, bool)> &);
struct PiecePose {
  bool collision = true;
  Vec3 offset{};
  std::uint16_t angle = 0;
  // Exact drawn chunk fields after the last move; empty means never moved since
  // load/reset, so the chunks still hold their file values.
  std::vector<ChunkTransform> chunks;
};
PiecePose autodoor_pose(bool open);
// Draw poses are rendering only, never used to transform collision.
class Pieces {
  std::vector<Piece> pieces_;
  std::vector<PiecePose> poses_;
  world_queries::Level &terrain_;

public:
  Pieces(std::vector<Piece>, world_queries::Level &);
  const std::vector<Piece> &data() const { return pieces_; }
  const std::vector<PiecePose> &poses() const { return poses_; }
  int claim(const Vec3 &p) { return find_moving_piece(pieces_, p); }
  void release(int i) { release_piece(pieces_, i); }
  void disable_unclaimed();
  void collision(int, bool);
  void height(int, Raw);
  std::int64_t height(int) const;
  void move(int, const Vec3 &, std::uint16_t); // world_movePiece
  void autodoor(int, bool);
  void reset(); // file draw pose, release claims, enable collision; retain
                // element z
};
} // namespace champions::simulation::rta::level_flow
