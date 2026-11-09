#include "champions/mission2_objects.hpp"
#include <algorithm>
#include <cstring>

namespace champions::simulation::rta::mission2_objects {
namespace {
bool available(std::uint8_t byte) { return (byte & 1u) != 0; }
bool complete(std::uint8_t byte) { return (byte & 2u) != 0; }

bool rest_clear(const MissionRow &row, int from) {
  for (int m = from; m < MISSIONS; ++m)
    if (row[static_cast<std::size_t>(m)] != 0)
      return false;
  return true;
}

void set_available(Hub2Choice &out, int mission) {
  // The executed availability write leaves that mission byte at 1 from 0.
  // compose:147-148, :153-154. compose.py:690-693, 796-797.
  auto &byte = out.after.row[static_cast<std::size_t>(mission)];
  if (byte != 0)
    throw Refused("mission_setAvailable was executed only on a clear mission byte");
  byte = 1;
  out.available.push_back(mission);
}

float as_float(std::uint32_t bits) {
  float value;
  std::memcpy(&value, &bits, sizeof value);
  return value;
}
} // namespace

// ---------------------------------------------------------------- hub2 dialog
Hub2Dialog hub2_open(const MissionRow &row, Node1Variable variable) {
  Hub2Dialog out;
  out.row = row;
  out.variable = variable;
  if (variable == Node1Variable::Unknown)
    throw Refused("hub2 reopen after node 2: whether node 1's choice stored the variable there "
                  "was not read back (compose:141-155)");
  if (variable == Node1Variable::Stored) {
    // compose:151-155. compose.py:775-800 opens row (3, 3, 0, 0...).
    MissionRow executed{};
    executed[0] = 3;
    executed[1] = 3;
    if (row != executed)
      throw Refused("hub2 reopen with the node-1 variable was executed only on row (3, 3, 0)");
    out.node = 3;
    out.visible = 2;
    return out;
  }
  // Cold open. compose:134-139. compose.py:699-704.
  if (row[0] != 3)
    throw Refused("hub2 cold open was executed with mission 0's byte at 3");
  if (row[1] != 0 && row[1] != 3)
    throw Refused("hub2 cold open was executed with mission 1's byte 0 or 3");
  if (row[2] > 3 || !rest_clear(row, 3))
    throw Refused("hub2 cold open was executed with mission 2's byte 0..3 and missions 3..12 clear");
  switch (row[2]) {
  case 0:
  case 2:
    out.node = 1;
    out.visible = 1;
    break;
  case 1:
    out.node = 4;
    out.visible = 2;
    break;
  default:
    out.node = 5;
    out.visible = 2;
    break;
  }
  return out;
}

Hub2Choice hub2_choose(const Hub2Dialog &dialog, int choice) {
  if (choice < 0 || choice >= dialog.visible)
    throw Refused("hub2 choice is not visible on this node");
  Hub2Choice out;
  out.after = dialog;
  switch (dialog.node) {
  case 1:
    // Script 0x29bc, no availability write. Mission 1 complete lands on
    // node 3 and stores the variable the reopen reads; otherwise node 2.
    // compose:141-144, :151-152. compose.py:680-683.
    out.script = 0x29bcu;
    if (dialog.row[1] == 3) {
      out.node = 3;
      out.after.variable = Node1Variable::Stored;
    } else {
      out.node = 2;
      out.after.variable = Node1Variable::Unknown;
    }
    break;
  case 4:
    // Choice 0 enters 0x41b4 (changeSwoop, then activateStore 0). Choice 1
    // adds no further script entry. Both stay on node 4 and change no mission
    // byte. compose:144-146. compose.py:684-687.
    if (choice == 0) {
      out.script = 0x41b4u;
      out.natives = {"changeSwoop", "activateStore"};
    }
    out.node = 4;
    break;
  case 5:
    // 0x2dd8 or 0x42c8; both make mission 3 available. Mission 2 stays 3.
    // compose:146-148. compose.py:688-691.
    out.script = choice == 0 ? 0x2dd8u : 0x42c8u;
    out.natives = {"mission_setAvailable"};
    set_available(out, 3);
    out.node = 5;
    break;
  case 3:
    // 0x2b6c or 0x4224; both make mission 2 available. Landing node not
    // recorded. compose:151-155. compose.py:775-800.
    out.script = choice == 0 ? 0x2b6cu : 0x4224u;
    out.natives = {"mission_setAvailable"};
    set_available(out, 2);
    break;
  default:
    throw Refused("hub2 node " + std::to_string(dialog.node) + " choices were not executed");
  }
  // The landing node's own choices were not executed in the same open; the
  // continuation is a reopen (compose.py:775-800).
  out.after.node = out.node ? *out.node : -1;
  out.after.visible = 0;
  return out;
}

// ----------------------------------------------------- hub2 trigger key 0x98
int hub2_trigger_key(const MissionRow &row) {
  // compose:159-163. tools/ref_mission2_compose.py:96-100.
  for (int m = 1; m <= 11; ++m) {
    const auto byte = row[static_cast<std::size_t>(m)];
    if (available(byte) && !complete(byte))
      return 1;
  }
  return available(row[12]) ? 1 : 0;
}

// --------------------------------------------- gate 0x1a84 and action 0x54
int hub2_action_gate(const MissionRow &row, const GateVariables &variables) {
  // compose:168-171. tools/ref_mission2_compose.py:82-86.
  for (int m = 1; m <= 12; ++m) {
    const auto byte = row[static_cast<std::size_t>(m)];
    if (available(byte) && !complete(byte) && variables[static_cast<std::size_t>(m - 1)] != 0)
      return 1;
  }
  return 0;
}

Hub2Action hub2_action(const MissionRow &row, const GateVariables &variables,
                       const std::string &entrance_before) {
  Hub2Action out;
  out.scripts = {0x2210u};
  out.gate = hub2_action_gate(row, variables);
  if (out.gate) {
    // compose:172-173, :184-187. compose.py:756-763.
    out.scripts.push_back(0x22d0u);
    out.natives = {"addHelpMessage", "addDeferedCall"};
    out.entrance = entrance_before;
  } else {
    // compose:171-172, :176-178, :187-188. compose.py:735-749, 765-772.
    out.natives = {"setEntrance", "activateMissionMenu"};
    out.entrance = EVIL_ENTRANCE;
    out.mission_menu_activated = true;
    out.menu_open_refusal =
        "activateMissionMenu's dialog_isOpen at 0x143fb0 was the seeded responder stub; the "
        "seed split is a fixture, and retail missionMenu_open 0x231520 stayed unentered "
        "(compose:178-183, :252-254)";
  }
  out.returned = 0; // Both branches return 0. compose:178, compose.py:736, 760.
  return out;
}

// ------------------------------------------------------ particle_resetAll
ParticlePool particle_init() {
  // particle_staticInit then particle_init, with 0x19e4a0 and 0x173768
  // stubbed to 0. compose:200, compose.py:486-495.
  ParticlePool pool;
  pool.initialized = true;
  return pool;
}

void particle_set_defaults(ParticleDefinition &definition) {
  // particleDef_setDefaults 0x18f360 stores the definition's own address at
  // +0x28. compose:205-206.
  definition.defaults_set = true;
}

int particle_claim(ParticlePool &pool, ParticleDefinition &definition) {
  // compose:206-208. compose.py:528-531.
  if (!pool.initialized)
    throw Refused("particle claim was executed after particle_init");
  if (pool.claimed_once || pool.reset)
    throw Refused("only the first claim after particle_init was executed");
  if (!definition.defaults_set)
    throw Refused("particle claim was executed on a definition after particleDef_setDefaults");
  pool.claimed_once = true;
  definition.claimed = true;
  definition.slot = 1;
  pool.type = &definition;
  return 1;
}

ParticleReset particle_reset_all(ParticlePool &pool) {
  if (!pool.initialized)
    throw Refused("particle_resetAll 0x18ebb8 on an uninitialized pool faults on a 4-byte write at "
                  "0x00000004, pc 0x0018ec10 (compose:194-198)");
  // compose:200-209. compose.py:497-535.
  ParticleReset out;
  if (pool.trails_frozen)
    out.calls.push_back(PARTICLE_EMPTY_FN);
  out.calls.push_back(TRAIL_RESET_ALL);
  out.calls.push_back(GROUP_RESET_ALL);
  pool.slots.fill(-1);
  pool.slots[0] = 1;
  pool.group_handle = 0;
  pool.trail_handle = 0;
  pool.group_generation = 1;
  pool.trail_generation = 1;
  if (pool.type && pool.type->claimed) {
    pool.type->slot = -1;
    pool.type->word8 = 0;
  }
  pool.reset = true;
  out.returned = 1;
  return out;
}

// --------------------------------------------- gob_loadAll / cd_readBlocking
ReadRequest gob_header_request(const std::string &gob) {
  return {gob, GOB_HEADER_BYTES, 0};
}

ReadRequest tex_request(const std::string &level_tex_name) {
  return {level_tex_name + ".tex", TEX_HEADER_BYTES, 0};
}

ArchiveOpen accept_archive_open(const std::string &gob, const std::string &level_tex_name,
                                const std::vector<ReadRecord> &reads,
                                std::uint32_t lump_count_before,
                                std::uint32_t lump_count_after) {
  ArchiveOpen out;
  auto refuse = [&](const std::string &why) {
    out.accepted = false;
    out.refusal = why;
    return out;
  };
  auto same = [](const ReadRequest &a, const ReadRequest &b) {
    return a.name == b.name && a.size == b.size && a.offset == b.offset;
  };
  // round4:24-27: an unset g_levelTexName makes the request ".tex".
  if (level_tex_name.empty())
    return refuse("g_levelTexName is unset: the tex request is \".tex\" and misses (round4:26-27)");
  if (lump_count_before != 0)
    return refuse("the lump count was executed from an explicit 0 (round4:34)");
  if (reads.empty() || !same(reads.front().request, gob_header_request(gob)))
    return refuse("the first read is 0x800 bytes at offset 0 of the .gob (round4:16-17, :31)");
  const auto tex = tex_request(level_tex_name);
  if (std::none_of(reads.begin(), reads.end(),
                   [&](const ReadRecord &r) { return same(r.request, tex); }))
    return refuse("no read of 0x5000 bytes at offset 0 of the tex name (round4:32)");
  if (std::any_of(reads.begin(), reads.end(), [](const ReadRecord &r) { return r.hits != 1; }))
    return refuse("every read hits one disc file (round4:33)");
  if (lump_count_after != ARCHIVE_LUMPS)
    return refuse("the lump count is written to 3 (round4:34)");
  out.accepted = true;
  out.lump_count = lump_count_after;
  return out;
}

void refuse_load_level(bool clear_entered) {
  if (clear_entered)
    throw Refused("game_loadLevel: game_clearLevel does not return; texcache_reset 0x179b08 stops "
                  "on a 2-byte write at 0x18, pc 0x00179b30 (round4:58-60)");
  throw Refused("game_loadLevel: with clear stubbed the run stops at sceSifSetDma 0x352da0, "
                "syscall 119 at pc 0x00352da4; the retail result is Unknown and is not "
                "invented (round4:62-68)");
}

// -------------------------------------------- pool, handles, constructors
void pool_init(ObjectPool &pool) {
  // pool_init 0x14fcd8 leaves the free-list head as it was. compose:237-238.
  pool.pool_initialized = true;
}

void handle_init_pool(ObjectPool &pool) {
  // round4:73-75. r4.py:121-126.
  if (!pool.pool_initialized)
    throw Refused("handle_initPool was executed after pool_init (round4:49, :152)");
  pool.free_head = HANDLE_FREE_HEAD;
}

GameObjectBuilt game_object_ctor(ObjectPool &pool) {
  if (pool.built != 0)
    throw Refused("GameObject_ctor was executed as the first object on a fresh pool (r4.py:142-155)");
  if (pool.free_head == 0)
    throw Refused("GameObject_ctor with a null free list faults on the null link-head store, "
                  "pc 0x001a1780 (compose:237-242, round4:79-80)");
  if (!pool.seed)
    throw Refused("GameObject_ctor was executed on a known shared seed");
  // round4:75-79. r4.py:148-155.
  const std::uint32_t stepped = *pool.seed * 0x19660du + 0x3c6ef35fu;
  pool.seed = stepped;
  ++pool.built;
  return {0u, (stepped >> 16) & 0xffffu};
}

Constructed construct(ObjectPool &pool, ObjectKind kind) {
  if (pool.free_head != HANDLE_FREE_HEAD || pool.built != 0)
    throw Refused("constructors were executed on a fresh pool after handle_initPool (round4:82)");
  // round4:84-89. r4.py:157-166.
  Constructed out;
  switch (kind) {
  case ObjectKind::PushTrigger:
    out = {0x4cu, 0x0010000bu};
    break;
  case ObjectKind::Trigger:
    out = {0x70u, 0x3u};
    break;
  case ObjectKind::Npc:
    out = {0x480u, 0x01100028u};
    break;
  case ObjectKind::JumpGate:
    throw Refused("JumpGate_ctor 0x27c694 stops at lump_load's missing-file store, 4-byte write at "
                  "0, pc 0x0017363c; a resident portal lump is Unknown (round4:89-92)");
  }
  // These constructors' shared-seed use was not measured.
  pool.seed.reset();
  ++pool.built;
  return out;
}

// ------------------------------------------------------- Player_interact
InteractPlayer init_new_character_null_r11() {
  // round4:102-114. r4.py:180-189.
  InteractPlayer player;
  player.slot = 0;
  player.hit_point_bits = INIT_HIT_POINT_BITS;
  player.maximum = 0;
  player.linked_object = false;
  player.initialized_null_r11 = true;
  return player;
}

Interact player_interact(const InteractPlayer &player, const std::vector<InRange> &objects) {
  if (player.word540 != 0)
    throw Refused("Player_interact was executed with the argument word +0x540 at 0 (compose:228)");
  if (player.slot < 0 || player.slot > 3)
    throw Refused("Player_interact slot is outside the four player records");
  Interact out;
  const std::uint32_t record = PLAYER_RECORDS + static_cast<std::uint32_t>(player.slot) * PLAYER_STRIDE;
  // round4:96-99: bc1t to the epilogue when the float compares <= +0.
  if (as_float(player.hit_point_bits) <= 0.0f) {
    // compose:229-234. compose.py:578-598. No calls; returns the record.
    out.returned = record;
    return out;
  }
  // The in-range prompt. round4:108-125. r4.py:177-229.
  if (!player.initialized_null_r11 || player.slot != 0 || player.hit_point_bits != INIT_HIT_POINT_BITS)
    throw Refused("the in-range path was executed only after player_initNewCharacter with a null "
                  "r11 on slot 0 (round4:102-114)");
  if (!player.linked_object)
    throw Refused("the in-range path was executed with a GameObject at record +0x43d8 (round4:112-114)");
  if (objects.size() != 1 || !objects[0].at_linked_position || !objects[0].use_bits_planted)
    throw Refused("the in-range path was executed with one PushTrigger at the linked object's "
                  "position and the 0x40 use bits planted (round4:112-123)");
  if (objects[0].kind != ObjectKind::PushTrigger)
    throw Refused("NPC and Trigger prompts are Unknown (round4:168)");
  out.box_query = true;
  out.prompt_calls = 1;
  out.prompt_returned = 1;
  out.get_prop_calls = 2;
  out.strlen_calls = 2;
  out.use_entered = false;
  out.use_refusal = "PushTrigger_interact 0x1ecc70 is not entered after the prompt returns 1; "
                    "which later test skips it is Unknown (round4:120-124, :157)";
  // 0x4d8ac0, 0x30 below slot 0's record (round4:122-124 labels it +0x30).
  out.returned = PLAYER_RECORDS - 0x30u;
  return out;
}

void refuse_use() {
  throw Refused("PushTrigger_interact 0x1ecc70: the use was not entered and its effects are "
                "Unknown (round4:120-124, :157, :167-168)");
}

void refuse_arrival() {
  throw Refused("arrival after the level load is Unknown (compose:72-74, round4:162-169)");
}
} // namespace champions::simulation::rta::mission2_objects
