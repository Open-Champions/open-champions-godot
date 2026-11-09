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
// Round 3 (docs/level-flow.md section 17). The placement radius is the word
// stored at object +0x1c and passed in f12. terrain_height ignores the query z.
inline constexpr Raw PLACEMENT_RADIUS = 0x41400000u;
inline constexpr Raw ARENA_RING_MODE_A = 0x43340000u; // 180 when mode A is set
inline constexpr Raw ARENA_RING_MODE_B = 0x42100000u; // 36 otherwise
inline constexpr int ARENA_FIRST_SPREAD = 36;
inline constexpr int ARENA_RETRY_SPREAD = 240;
inline constexpr int ARENA_SEARCH_RADIUS = 24;
// Round 4 (docs/level-flow.md section 18). Mode-A table length, mode-B
// table length, and the six-byte records at 0x503868.
inline constexpr int ARENA_MODE_A_COUNT = 13;
inline constexpr int ARENA_MODE_B_COUNT = 7;
inline constexpr int ARENA_RECORD_BYTES = 6;
// Player_ctor 0x1a2420 stores terrain_height(radius, pos) at object +0x18.
// It does not test the sentinel and does not move x or y. radius is
// PLACEMENT_RADIUS after the store at 0x1a2244.
Raw player_ctor_z(const Vec3 &pos, Raw radius, const Height &);
// What sections 17 and 18 leave unexecuted. Calling one throws Unsupported.
enum class OpenItem {
  LiveFindFreeSpotSeed,  // g_randSeed at Player1_ctor is not a fixed constant
  RealObjectList,        // class-constructor radii were not executed
  ArenaRespawn,          // arena_pickSpawnPoint a0 = 0: not in research-levels; arena_respawn forwards to research-arena
  ArenaRotationTable,    // the mode-A name table is not copied into this module
  PlayerCtorRemainder,   // Player_ctor after the radius store and the snap
  OrientationCopyReaders, // readers of the copied orientation byte 0x41b7c1
  SentinelFirstTick,     // a first-tick reject other than the snap and kill plane
  FacingWordPointerStore, // a nonzero store of 0x4f7124 through a taken address
  GateAnyHeightOnDisc,   // the gate's anyHeight search was not re-run on the disc
  RecallReturnField,     // 0x41b520 is the previous position, not a level field
  NetPeerIndex,          // message +0xf4 is not clamped; what a peer writes is unknown
};
const char *open_item_reason(OpenItem);
[[noreturn]] void refuse_open(OpenItem);
[[noreturn]] void arena_net_peer_index(); // message offset 0xf4, unchecked
// Low 16 bits of s16(65535 / count) * slot. A non-positive count divides by
// zero in the original and is refused here.
std::int32_t arena_ring_angle(int slot, int active_count);
// Entry 0 plus (cos16(angle) * R, sin16(angle) * R). z is the entry's z.
Vec3 arena_ring_centre(const Vec3 &entry, int slot, int active_count,
                       bool mode_a);
// world_findFreeSpot's (position, ok) pair. ok == 0 keeps the centre.
using ArenaSearch = std::function<std::pair<Vec3, int>(
    const Vec3 &centre, int spread, int radius, bool any_height)>;
struct ArenaPlace {
  Vec3 pos{};
  std::uint16_t facing = 0;
  int ok = 0;
};
// arena_pickSpawnPoint 0x3050a4 when its first argument is not 0. Spread 36,
// then spread 240 from the same centre when the ok byte is 0. There is no
// third search. table_facing selects the executed branch for a nonzero word
// at 0x4f7124; whether a pointer store writes that word is OpenItem.
ArenaPlace arena_initial_place(const Vec3 &entry, int slot, int active_count,
                               bool mode_a, const ArenaSearch &,
                               bool table_facing = false,
                               std::uint16_t table_angle = 0);
// The rotation byte at 0x8e7343. Index must be 0..12. A zero remainder is
// replaced by `saved` (the menu index at 0x4f7110). A net peer's word at
// message +0xf4 is not a value of `saved`; that path is arena_net_peer_index.
int arena_mode_a_next(int index, int saved);
// The record byte at 0x503868, shifted left 28 and arithmetic-shifted right
// 26. The high nibble falls off. A low nibble of 8 or more is negative.
std::int32_t arena_record_table_offset(int first_byte);
// One spawn-table slot. occupied is false for a hole this registration grew past.
struct ArenaSpawnSlot {
  Vec3 point{};
  std::uint16_t angle = 0;
  bool occupied = false;
};
struct ArenaSpawnTable {
  std::vector<ArenaSpawnSlot> entries;
  int count = 0;
};
// 0x30503c. Index -1 writes at count and then increments that byte. Any other
// index overwrites that slot and leaves count alone. A slot above 255 is
// refused: the count is a byte, and the factories pass 0 or -1.
ArenaSpawnTable arena_register_spawn(ArenaSpawnTable, int index,
                                     const Vec3 &point, std::uint16_t angle);
// arena_pickSpawnPoint 0x3050a4 with a0 == 0, the respawn pick. Research-levels
// section 18 read it and did not execute it (OpenItem::ArenaRespawn).
// Research-arena 17810bb executed both arguments, and this forwards the
// registered table to arena_modes::pick_spawn with flag 0: networked, a
// shared-seed index into the whole table; offline, a shared-seed index into
// the entries near `focus`, or findFreeSpot (spread 72, radius 24) around
// `camera` when none is near. The search draws from the shared seed and
// leaves it where its draws end. A hole below count refuses: no factory wrote
// those bytes.
using ArenaSeededSearch = std::function<std::pair<Vec3, int>(
    const Vec3 &centre, int spread, int radius, std::uint32_t &seed)>;
struct ArenaRespawnPick {
  Vec3 pos{};
  int facing = 0;
  std::uint32_t seed = 0;
};
ArenaRespawnPick arena_respawn(const ArenaSpawnTable &, std::uint32_t seed,
                               bool network, const Vec3 &focus,
                               const Vec3 &camera, const ArenaSeededSearch &);
// Acts do not choose a marker. act_changed only clears recall destinations,
// and only when the card state is not 4 and no death task is running.
struct StartEntry {
  std::string prev, entrance;
  bool arrived_by_recall = false, gate = false, loaded_from_save = false,
       act_changed = false, death_task = false;
  int card_state = 0;
};
struct StartResult {
  SpawnState spawn{};
  std::array<Raw, 4> object_z{};
  std::array<bool, 4> ground{};
  std::vector<std::size_t> took;
  std::vector<unsigned> built;
  bool recall_cleared = false;
  bool slot0_ground = false;
};
// Marker choice, the caller's search, the radius-12 placement snap, then
// player_ctor_z on each constructed slot. The caller supplies the search.
// This function does not read g_randSeed or a retail object list.
StartResult resolve_start(const SpawnState &incoming,
                          const std::vector<Record> &, const StartEntry &,
                          const Placement &, const Height &, const FindSpot &,
                          const Draw &);
// Cold story start the real-data job can call. Identity search: the file
// position, or the incoming slot when no marker takes. z is player_ctor_z at
// PLACEMENT_RADIUS. ground is that z other than the sentinel. One player.
// A taken marker sets pre_search: the xy is before findFreeSpot, whose live
// seed and class-constructor radii are not documented, and facing is left
// unset. There is no arena fallback: a level reached only through the mode-A
// table (section 18.1) is not a story start, and its caller must say so by
// calling arena_mode_a_start_ground.
struct StoryGround {
  Vec3 position{};
  Raw spawn_z = 0, object_z = 0;
  std::optional<std::uint16_t> facing;
  int rank = 0;
  bool marker_taken = false, ground = false, recall_cleared = false;
  bool pre_search = false;
};
// Mode-A wave start. arena_init plants the origin at entry 0 with count 1.
// Each ArenaPlayerStart then overwrites entry 0; each ArenaPlayerAlternate
// appends. The ring uses entry 0, mode A (radius 180), the caller's search
// (findFreeSpot: spread 36, then 240 when ok is 0, radius 24), and
// player_ctor_z at PLACEMENT_RADIUS. The facing word stays clear. Slot 0's
// angle is 0 for every positive count, so its centre does not depend on count.
StoryGround arena_mode_a_wave_ground(const std::vector<Record> &, int slot,
                                     int active_count, const ArenaSearch &,
                                     const Height &);
// The caller asserts the level was loaded in mode A (g_pvpModeA and
// g_pvpModeB set, 0x4f7124 clear: the executed case of 18.2); this module
// cannot see the mode-A table. The same mode-A wave start for slot 0 and
// count 1 before the search: the
// ring centre, then player_ctor_z there. pre_search is set and facing is
// unset (the original's point and facing depend on g_randSeed and the object
// list). Empty (ground false) when no ArenaPlayerStart is registered.
StoryGround arena_mode_a_start_ground(const std::vector<Record> &,
                                      world_queries::Level &);
StoryGround story_start_ground(const SpawnState &incoming,
                               const std::vector<Record> &,
                               world_queries::Level &);
// Same snap for an explicit entry (previous level, entrance, recall flag,
// gate, owner-supplied restore xy). A gate entry that takes a Portal or
// PortalHurt would run JumpGate_ctor's anyHeight search, which was not re-run
// on the disc: it throws Unsupported (OpenItem::GateAnyHeightOnDisc). A gate
// flag with no portal takes nothing and is the plain marker snap.
StoryGround entry_start_ground(const SpawnState &incoming,
                               const std::vector<Record> &, const StartEntry &,
                               const Placement &, world_queries::Level &);
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
// save_findAct 0x217d30: the current act first, then acts 0..n-1; a row
// matches when the name starts with it, and the first match wins (hub2 takes
// hub, storm1a takes an earlier storm1). -1 when nothing matches.
// research-exits-r3@e86f3de docs/exits-r4-travel.md:12-23.
int find_act(const Acts &, const std::string &, int current);
// save_storeLevelState 0x21777c. The original returns 0 on every path
// (docs/exits-r4-travel.md:27); ret is that register and is never a status.
// skipped: card word 0x88d3a0 is 4 or level flag 0x800 is set, so the
// directory is untouched. entry: the key exists afterwards, created or kept,
// including when the writer is not called (:36-41). writer_offset: the data
// offset save_writeLevelState 0x218600 is called at, or none.
struct StoreResult {
  int ret = 0;
  bool skipped = false, entry = false;
  std::optional<std::size_t> writer_offset;
  bool written() const { return writer_offset.has_value(); }
};
// level_loadScriptAndObjects 0x217978, the restore test at 0x217a18: names
// are compared in slots 0..9 (loop 0x2179c0-0x2179f4); the test reads the
// first name byte of the slot where the search stopped. A hit restores that
// slot. A stop at an empty slot loads fresh. After ten misses the stop is gap
// slot 10: an empty gap loads fresh, a written gap restores its body for
// every missing name. research-save-world@31a0ca6 docs/save-world.md:44-52,
// research-death-reload@98ea7a6 docs/death-reload-client-path.md:96.
enum class Restore { Fresh, Saved, Gap };
struct RestoreDecision {
  Restore kind = Restore::Fresh;
  std::size_t slot = 0; // the stopped slot
};
struct LevelStates {
  static constexpr std::size_t LIMIT = 0x4bc78 - 0x16118;
  // 0x8ab590 to the data at 0x8ab698 holds eleven 0x18-byte entries. The key
  // search covers the first ten; a new key is appended, as an 11th entry
  // after ten (research-exits-r3@e86f3de tools/ref_exit_travel.py:109-128).
  // Every later miss (a twelfth key, or the gap's own name, which the
  // ten-slot search never compares) rewrites the slot-10 body at its offset
  // with its kept size and keeps the ghost name: no 12th entry is written
  // (research-save-world@31a0ca6 docs/save-world.md:35-39, 196, Executed).
  static constexpr std::size_t SEARCHED = 10, ENTRIES = 11;
  std::vector<std::pair<std::string, std::uint32_t>> entries;
  // Owner serialization handed to the writer. Retail object, decal and
  // explored-map bytes are Unknown (docs/exits-r4-travel.md:41-43).
  std::map<std::string, Blob> blobs;
  // rows is the current act (empty for act -1). Size: 35000 when `fixed`
  // (byte 0x4f85c5); else hub* takes row 0; else save_levelIndexInAct
  // 0x2187a0, the first row the name starts with; no row stores nothing.
  StoreResult store(const std::string &, const Act &rows, int card = 0,
                    std::uint32_t flags = 0, bool fixed = false,
                    const Blob &blob = {});
  // save_levelStateStored 0x2182d4: the directory key, written or not
  // (docs/exits-r4-travel.md:110-111). Refuses when the name is not among
  // the first ten keys of an 11-entry directory: that scan was not run.
  bool stored(const std::string &) const;
  // Owner bytes present for the key, and the key's data offset.
  std::pair<bool, std::size_t> lookup(const std::string &) const;
  // The stopped-slot rule above. Does not refuse; see require_restorable.
  RestoreDecision restore_decision(const std::string &) const;
  // Client policy for that rule. A gap restore is the original's behaviour
  // (save-world.md:44-52), but the port must choose a gap-slot policy and
  // must not present slot 10 as an eleventh saved level (save-world.md
  // :314-316; client-path.md:153-157). The chosen policy is to refuse, with
  // the key and the gap name. With cleared_slot0 checks on (a death
  // reload), the key an act-change clear kept in slot 0 (below) also
  // refuses, and so does any key once the dropped slot shifts the client's
  // slot 9 onto the original's gap.
  RestoreDecision require_restorable(const std::string &,
                                     bool check_cleared_slot0) const;
  // save_clearBuffer(0) keeps directory slot 0 with its name, its first
  // act-0 row-0 bytes (15000 in the retail table) and that row's size; every
  // other key is gone (save-world.md:194-195, Executed). begin_change keeps
  // slot 0 only in the case whose restore was executed: the exact act-0
  // row-0 name (`hub`), already that size, with a body that fits. Otherwise
  // (a larger body, a name such as `blood1` whose size the clear cuts, or a
  // hub sub-area such as `hub_west`, none restored there) it drops the whole
  // directory and records the slot-0 key here, so a death reload into it
  // refuses.
  std::optional<std::string> cleared_slot0;
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
struct GateArrival {
  int rank = 0;
  std::vector<Vec3> positions;
  std::vector<std::uint16_t> facing;
  std::uint32_t seed = 0;
};
GateArrival portal_arrival(bool travelling, bool fixed_destination,
                           unsigned players, const Vec3 &portal,
                           std::uint32_t &shared_seed, int previous_rank,
                           const FindSpot &);
struct TriggerPlayer {
  unsigned slot;
  Vec3 position;
};
// Player_interact 0x1b26b8-0x1b27e0: the box query supplies candidates in
// grid order. Preserve that order for equal distances.
struct UseCandidate {
  std::size_t record = 0;
  Vec3 position{};
  std::uint32_t flags = 0;
};
bool use_query_contains_centre(const Vec3 &player, const Vec3 &candidate);
std::optional<std::size_t> nearest_use_candidate(
    const Vec3 &player, const std::vector<UseCandidate> &query_results);
struct TriggerEvent {
  std::optional<std::string> entry;
  std::optional<unsigned> active;
};
struct TriggerState {
  std::int32_t delay;
  bool inside;
};
struct TriggerParams {
  bool on_only = false, off_only = false, need_script_hit = false;
  std::int32_t timeout = 0;
};
struct Trigger {
  Vec3 min{}, max{};
  std::string script;
  bool inside = false;
  std::int32_t delay = 0; // persisted, scheduling owned by Session
  std::int32_t slot = -1; // +0x68; reset before recall suppression
  Trigger(Raw x, Raw y, std::int32_t w, std::int32_t h, std::string t);
  bool contains(const Vec3 &) const;
  TriggerEvent run(const std::vector<TriggerPlayer> &, bool recall = false);
  TriggerEvent run_exact(
      const std::vector<TriggerPlayer> &, bool recall,
      const TriggerParams *linked,
      const std::function<void(bool)> &linked_actions,
      const std::function<int(const std::string &, unsigned)> &script_run);
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
  // Placement's peer test (exits-r4-travel.md:107-117, 0x13dd40): a slot that
  // is not active is not built on arrival.
  bool active = false;
  // save_game's object test (save-world.md:247-249, Executed; 0x218170-
  // 0x2181a0): the record's object pointer at +0x43d8 is set, and that
  // object's flag 8 (+0xc & 8). A different test from `active`; owners that
  // do not track the player object leave object_built false and use
  // SaveGameOptions::object_test off.
  bool object_built = false, object_flag8 = false;
  std::string recall_dest = "None";
  Vec3 recall_position{};
  Blob persistent; // owner serialization: stats/bags/gold/medals; kept verbatim
  std::vector<std::string> quests;
};
// Act change writes "None" into every record's recall destination. It does
// not choose a start marker.
void apply_act_entry(std::array<PlayerRecord, 4> &, bool act_changed,
                     int card_state, bool death_task);
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
// The header part of the live save buffer g_saveBuffer 0x895580: players,
// poses, quests, gate destinations, level, act and difficulty. The level
// directory is not part of it: it stays in State::states, live, and later
// departure stores and act-change clears keep writing there. A death reload
// reads the header with each level's latest stored body; it never restores a
// frozen directory copy. research-death-reload@98ea7a6
// docs/death-reload-client-path.md:55-73,137-143 (Inferred there, from
// research-save-world@31a0ca6 docs/save-world.md:285-301).
// With the game complete, save_build writes this fixed name to header +0x00
// instead of the level name (research-saves@8d43286 docs/save-format.md:37).
inline constexpr char GAME_COMPLETE_NAME[] = "gameComplete";
struct Checkpoint {
  std::string level, sub;
  int act = -1, difficulty = 1;
  SpawnState spawn;
  std::array<PlayerRecord, 4> players;
  std::map<std::string, std::int32_t> quest_values;
  GateList gates;
  Blob camera;
};
struct State {
  Acts acts;
  int act = -1, difficulty = 1;
  std::string level, sub, prev, entrance, post_action;
  Pending pending;
  LevelStates states;
  Blob save_buffer; // owner serialization; cleared on an unlatched act change
  std::optional<Checkpoint> saved; // live save-buffer header (see Checkpoint)
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
  // g_saveClearBlock 0x4f8f2a: when set, save_clearBuffer leaves the buffer
  // (directory included) untouched (research-saves@8d43286
  // docs/save-format.md:296-297; save-world.md:194, Executed). Nothing in
  // this client sets it.
  bool clear_block = false;
  int card_state = 0;
  std::string hub = "hub";
  Blob old_blob;
};
// Not a status: mainloop_loadRequestedWorld's return register is not one
// (docs/exits-r4-travel.md:122-123). stored: the departure writer ran.
// had_saved_state: save_levelStateStored hit for the new name, which keeps
// the idle-voice countdown; saved_bytes: owner bytes exist for that key.
struct ChangeResult {
  bool stored = false, act_changed = false, reset_act = false,
       had_saved_state = false, saved_bytes = false;
  std::uint8_t orientation = 0;
};
// Preparation through split/reset of mainloop_loadRequestedWorld 0x320edc
// (docs/exits-r4-travel.md:51-85). Empty level or death task skips the
// departure store; the front end skips the act check; the act index changes
// even when the clear block is skipped; the clear needs an act change, no
// card latch and no death task. prev is written before the split at the
// first '_'; the orientation byte is saved and replaced with 0xfe. The
// original store returns 0, so the caller's nonzero early return is not
// modelled. Host executes teardown/load/placement in the adapter.
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
// save_game 0x218128 then save_build 0x221914 (save-world.md:241-275,
// Executed). players: g_numPlayers; only slots below it count, and a slot
// counts when its object is built, that object's flag 8 is clear and HP is
// above 0 (PlayerRecord::object_built, object_flag8). object_test off counts
// HP alone, for owners that do not track the player object.
// mission_menu_task: the `missionmenurun` task is running (save-world.md:253).
struct SaveGameOptions {
  unsigned players = 4;
  bool game_complete = false, object_test = true, mission_menu_task = false;
  int card_state = 0;
};
enum class SaveGame {
  NoLivingPlayer, // nothing happens (save-world.md:247-252)
  NoActRow,       // CheckPoint text, no build, no store (save-world.md:255-260)
  // Reached save_build directly (act -1 or the mission-menu task,
  // save-world.md:253-254); save_build's own save_findAct found no act and
  // the game is not complete, so nothing is written (research-saves@8d43286
  // docs/save-format.md:55-58, Executed).
  BuildNoAct,
  Built,
};
struct SaveGameResult {
  SaveGame outcome = SaveGame::NoLivingPlayer;
  bool level_stored = false; // the current level's body went into the directory
};
// Builds the header into State::saved and, unless the game is complete,
// stores the current level into the live directory. save_game's own act-row
// test (step 3) is unconditional: only act -1 and the mission-menu task skip
// it (step 2). A completed game skips save_build's save_findAct and the level
// store, so that level keeps whatever body the directory already held
// (save-world.md:269-275; save-format.md:55-60; client-path.md:69-71).
SaveGameResult save_game(State &, const Blob &level_state, const Blob &camera,
                         const SaveGameOptions & = {});
// Earlier entry point: save_game without the object test. Returns the header
// it built into State::saved, or nothing when no checkpoint was built.
std::optional<Checkpoint> checkpoint(State &, const Blob &level_state,
                                     const Blob &camera, int card_state = 0);
// A card load copies the whole card image into g_saveBuffer, directory
// included (research-save-screen@c8895e2 docs/save-screen.md:258-279;
// client-path.md:103-108): header and directory both come from the image.
void card_load(State &, Checkpoint header, LevelStates directory);
enum class Wipe { Wait, Reload, Restart };
// deathTask 0x1e75e8 phase 1 (client-path.md:88-91). The act lookup is on the
// current level. Found: save_load on the live buffer, i.e. the header in
// State::saved with the live State::states, the loaded flag (rank 3 at load)
// and the saved level requested with 0xfd. Not found: request the current
// level with 0xfd and refill HP; nothing else changes. Alternate death: wait
// for the script (phase 10). Found with no header ever built or card-loaded
// refuses: the original's uninitialised-buffer behaviour is Unknown
// (client-path.md:183).
// players: g_numPlayers 0x4d8ae0. The step-5b refill copies +0xb0 to +0xac
// only for slots below it (deathTask 0x1e76dc-0x1e7710); later slots keep
// their HP.
Wipe party_wipe(State &, unsigned players);
// Same, with an explicit header instead of State::saved. The directory still
// comes from State::states.
Wipe party_wipe(State &, const std::optional<Checkpoint> &, unsigned players);
// deathTask phase 2 (client-path.md:99): the task ends once the pending
// request is empty and the loaded qualified name matches the requested one.
bool death_task_done(const State &, const std::string &requested);
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
