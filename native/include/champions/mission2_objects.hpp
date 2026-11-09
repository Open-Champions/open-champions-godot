#pragma once
// Mission 2 objects: the hub2 dialog selector, the hub2 PushTrigger key and
// action, the particle reset after init, the archive-open read contract, the
// object pool and constructors, and Player_interact's two executed exits.
//
// Research pin: ~/Projects/champions-re-wt/research-mission2-route d17b46f.
// Audit research-mission2-route-r3r4-fix.md is PASS. Contracts:
// docs/mission2-compose.md (cited "compose:N") and docs/mission2-round4.md
// (cited "round4:N"). Difftest lines are tools/difftest_mission2_compose.py
// ("compose.py:N") and tools/difftest_mission2_r4.py ("r4.py:N") at d17b46f.
//
// Like mission2_route, nothing in the session calls this module: the level
// load stops before these objects are reached in play (round4:47-69), and the
// prompt's use and arrival are Unknown. Values below are the executed
// results; every path the research did not execute throws Refused.
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::mission2_objects {
struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// One difficulty row of mission bytes, missions 0..12. Bit 0 is available,
// bit 1 is complete. tools/ref_mission2_compose.py:76-100.
inline constexpr int MISSIONS = 13;
using MissionRow = std::array<std::uint8_t, MISSIONS>;
// Quest variable of missions 1..12 (index 0 is mission 1). Missions 1..9 use
// a 15-character name and 10..12 a 16-character name; no name is stored
// here. compose:169-171.
using GateVariables = std::array<std::int32_t, 12>;

// ---------------------------------------------------------------- hub2 dialog
// compose:124-155. compose.py:675-801.
enum class Node1Variable {
  Absent,  // quest variables absent: the cold open.
  Stored,  // set by node 1's choice when it lands on node 3 (compose:151-154).
  Unknown, // node 1's choice landed on node 2; that variable was not read back.
};
struct Hub2Dialog {
  MissionRow row{};
  Node1Variable variable = Node1Variable::Absent;
  int node = 0;
  int visible = 0;
};
struct Hub2Choice {
  // Script entry offset the choice enters last; empty when the choice adds
  // no script entry (node 4, choice 1, compose:145-146).
  std::optional<std::uint32_t> script;
  std::vector<std::string> natives;
  // Landing node. Empty where the research did not record it (node 3).
  std::optional<int> node;
  // mission_setAvailable calls, by mission index. Each set byte becomes 1.
  std::vector<int> available;
  Hub2Dialog after;
};

// Cold open: mission 0's byte is 3, mission 1's byte is 0 or 3, mission 2's
// byte is 0..3, missions 3..12 are 0, quest variables absent. Mission 2's byte
// selects node 1 (byte 0 or 2, one choice), 4 (byte 1, two), or 5 (byte 3,
// two). compose:134-139. A reopen with the node-1 variable stored, on row
// (3, 3, 0, 0...), selects node 3 with two choices. compose:151-155.
Hub2Dialog hub2_open(const MissionRow &row, Node1Variable variable = Node1Variable::Absent);
// One choice on an open node. compose:141-155. compose.py:684-697, 775-800.
Hub2Choice hub2_choose(const Hub2Dialog &dialog, int choice);

// ----------------------------------------------------- hub2 trigger key 0x98
// Returns 1 when any mission 1..11 is available and not complete, or when
// mission 12 is available, complete or not. demoRunning is not an input.
// compose:159-165. tools/ref_mission2_compose.py:89-100.
int hub2_trigger_key(const MissionRow &row);

// --------------------------------------------- gate 0x1a84 and action 0x54
// 1 when a mission 1..12 is available, not complete, and its own quest
// variable is nonzero. compose:167-171. tools/ref_mission2_compose.py:76-86.
int hub2_action_gate(const MissionRow &row, const GateVariables &variables);

inline constexpr char EVIL_ENTRANCE[] = "evilstart";
struct Hub2Action {
  int gate = 0;
  int returned = 0;
  // 0x2210, then 0x22d0 on the true gate. compose:167-173.
  std::vector<std::uint32_t> scripts;
  std::vector<std::string> natives;
  std::string entrance; // setEntrance argument; empty on the true gate.
  // activateMissionMenu's dialog_isOpen call at 0x143fb0 was the seeded
  // responder stub (compose:178-183). Which branch retail takes there is
  // not established, so the menu open is never decided here.
  bool mission_menu_activated = false;
  std::string menu_open_refusal;
};
// Action 0x54 (callScript 0x2210). compose:167-190. compose.py:728-773.
Hub2Action hub2_action(const MissionRow &row, const GateVariables &variables,
                       const std::string &entrance_before = {});

// ------------------------------------------------------ particle_resetAll
// The state particle_resetAll 0x18ebb8 reads, after particle_staticInit
// 0x195d30 and particle_init 0x18e648. Those two ran with 0x19e4a0 and
// 0x173768 stubbed to return 0 (compose.py:486-495). compose:194-210.
inline constexpr int PARTICLE_SLOTS = 32;
struct ParticleDefinition {
  bool defaults_set = false; // particleDef_setDefaults: +0x28 = own address.
  bool claimed = false;
  std::int32_t slot = 0;  // +4
  std::int32_t word8 = 0; // +8
};
struct ParticlePool {
  bool initialized = false;
  bool claimed_once = false;
  bool reset = false;
  // Valid only after the reset; the init's own slot words are not recorded.
  std::array<std::int32_t, PARTICLE_SLOTS> slots{};
  std::uint32_t group_handle = 0;
  std::uint32_t trail_handle = 0;
  std::uint32_t group_generation = 0;
  std::uint32_t trail_generation = 0;
  std::uint32_t trails_frozen = 0; // g_trailsFrozen 0x44fc98
  ParticleDefinition *type = nullptr; // 0x82c4e8
};
struct ParticleReset {
  std::uint32_t returned = 0;
  std::vector<std::uint32_t> calls;
};
inline constexpr std::uint32_t PARTICLE_EMPTY_FN = 0x18ebb0;
inline constexpr std::uint32_t TRAIL_RESET_ALL = 0x1929c8;
inline constexpr std::uint32_t GROUP_RESET_ALL = 0x18e840;
ParticlePool particle_init();
void particle_set_defaults(ParticleDefinition &definition);
// Claiming the first definition after init takes slot 1. compose:206-208.
int particle_claim(ParticlePool &pool, ParticleDefinition &definition);
// Throws Refused on a pool that was not initialized: the menu image faults
// on a 4-byte write at 0x00000004, pc 0x0018ec10. compose:194-198.
ParticleReset particle_reset_all(ParticlePool &pool);

// --------------------------------------------- gob_loadAll / cd_readBlocking
// cd_readBlocking 0x172ad0 (name, destination, size, offset). round4:14-45.
struct ReadRequest {
  std::string name;
  std::uint32_t size = 0;
  std::uint32_t offset = 0;
};
struct ReadRecord {
  ReadRequest request;
  int hits = 0; // disc files with that name
};
inline constexpr std::uint32_t GOB_HEADER_BYTES = 0x800;
inline constexpr std::uint32_t TEX_HEADER_BYTES = 0x5000;
inline constexpr std::uint32_t ARCHIVE_LUMPS = 3;
// The first read: 0x800 bytes at offset 0 of the .gob name. round4:16-17.
ReadRequest gob_header_request(const std::string &gob);
// The tex read names g_levelTexName 0x44fe18 plus ".tex". With that word
// unset the request is ".tex" and misses. round4:24-27, :148-151.
ReadRequest tex_request(const std::string &level_tex_name);
struct ArchiveOpen {
  bool accepted = false;
  std::uint32_t lump_count = 0;
  std::string refusal;
};
// Accepts a recorded gob_loadAll read list against the executed contract:
// the first read is the header, one read is the tex request, every read hits
// one file, and the lump count goes from 0 to 3. The running offsets and the
// rounded lump sizes in between are recorded, not generated: the rounding is
// not given (audit, round 4 reads). round4:29-34.
ArchiveOpen accept_archive_open(const std::string &gob, const std::string &level_tex_name,
                                const std::vector<ReadRecord> &reads,
                                std::uint32_t lump_count_before,
                                std::uint32_t lump_count_after);

// game_loadLevel past the archive lookup. round4:47-69, :158-160.
[[noreturn]] void refuse_load_level(bool clear_entered);

// -------------------------------------------- pool, handles, constructors
// round4:71-92. r4.py:111-175. compose:237-243.
inline constexpr std::uint32_t HANDLE_FREE_HEAD = 0x847268;
struct ObjectPool {
  bool pool_initialized = false;
  std::uint32_t free_head = 0; // 0x864ea8
  // Shared seed 0x404464. Empty once a constructor whose seed use was not
  // measured has run.
  std::optional<std::uint32_t> seed;
  int built = 0; // each executed constructor ran first on its own fresh pool
};
void pool_init(ObjectPool &pool);
// handle_initPool 0x1a17d0 stores 0x847268 at the free-list head.
void handle_init_pool(ObjectPool &pool);
struct GameObjectBuilt {
  std::uint32_t returned = 0;
  std::uint32_t word44 = 0; // object +0x44
};
// GameObject_ctor 0x14cb10. One shared-seed step; +0x44 gets the stepped
// seed shifted right 16. A null free list is the round-3 null link-head
// store at pc 0x1a1780. round4:73-80. compose:237-243.
GameObjectBuilt game_object_ctor(ObjectPool &pool);
enum class ObjectKind { PushTrigger, Trigger, Npc, JumpGate };
struct Constructed {
  std::uint32_t size = 0;  // pool_new size the fixture used (r4.py:161-164)
  std::uint32_t flags = 0; // body +0x0c
};
// On a fresh pool after handle_initPool. JumpGate refuses at the lump_load
// missing-file store. round4:82-92.
Constructed construct(ObjectPool &pool, ObjectKind kind);

// ------------------------------------------------------- Player_interact
// Player_interact 0x1b2640. compose:228-235, round4:94-125.
inline constexpr std::uint32_t PLAYER_RECORDS = 0x4d8af0;
inline constexpr std::uint32_t PLAYER_STRIDE = 0x4c30;
inline constexpr std::uint32_t INIT_HIT_POINT_BITS = 0x4e6e6b27;
struct InteractPlayer {
  std::uint32_t word540 = 0; // argument +0x540
  int slot = 0;              // argument +0x3c8
  std::uint32_t hit_point_bits = 0; // float at record +0xac
  std::uint32_t maximum = 0;   // record +0xb0
  bool linked_object = false;  // record +0x43d8, stored by the fixture
  bool initialized_null_r11 = false;
};
// player_initNewCharacter 0x1b1d70 on slot 0 with a null r11: hit-point bits
// 0x4e6e6b27, maximum 0, object word 0. round4:102-114.
InteractPlayer init_new_character_null_r11();
struct InRange {
  ObjectKind kind = ObjectKind::PushTrigger;
  bool at_linked_position = false;
  bool use_bits_planted = false; // 0x40 at 0x41b9c0 and pad +8, +0xc
};
struct Interact {
  std::uint32_t returned = 0;
  bool box_query = false;
  int prompt_calls = 0;
  std::optional<int> prompt_returned;
  int get_prop_calls = 0;
  int strlen_calls = 0;
  bool use_entered = false;
  std::string use_refusal;
};
Interact player_interact(const InteractPlayer &player, const std::vector<InRange> &objects);
// PushTrigger_interact 0x1ecc70. round4:120-124, :157.
[[noreturn]] void refuse_use();
// Arrival after the load. compose:72-74, round4:162-169.
[[noreturn]] void refuse_arrival();
} // namespace champions::simulation::rta::mission2_objects
