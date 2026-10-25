#pragma once
#include "champions/combat.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/item_generation.hpp"
#include "champions/player_state.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// What a kill gives the session player, over the shared World's bytes:
//  * player_giveExp 0x1d3150 (research-levelup 576b41c, docs/spec/levelup.md
//    "XP table and level transition"; research-combat docs/data-tables.md 4.4)
//    with the maxima player_recalcStats stores (data-tables.md 4.3);
//  * the loot_generateDrops 0x100d54 roll (research-pickups 9911a6b
//    docs/spec/items.md section 8, over the audited items generator), with
//    the shared seed the World holds.
// Anything the research does not establish is reported through `Refusals`,
// never replaced by an invented value.
namespace champions::simulation::rta::campaign_rewards {
using Address = combat_runtime::Address;

struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };

// Reads and writes the shared World's bytes (size 1, 2 or 4).
struct Memory {
  std::function<std::uint64_t(Address, unsigned)> read;
  std::function<void(Address, unsigned, std::uint64_t)> write;
};

// ---- experience -----------------------------------------------------------------------
struct ExpResult {
  std::int32_t levels_gained = 0;
  std::int32_t experience_before = 0, experience_after = 0;
  std::int8_t level_before = 0, level_after = 0;
};
// player_giveExp(slot, amount) on the record at 0x4d8af0 + slot * 0x4c30:
// experience += amount, clamped to the stored ceiling table[80] - 1; while
// experience >= table[earned level (+0x59)]: skill points (+0xe8) by the old
// level band, stat points (+0xec) += 3, earned level += 1, the maxima of
// player_recalcStats (class +0x50, effective statistics +0x90), and if the
// record's current hit points were positive both currents are refilled.
// The acknowledged level (+0x58) is not touched. Refuses a slot outside 0..3.
ExpResult give_exp(const player_state::Tables &, Memory &, std::int64_t slot, std::int64_t amount);

// ---- loot ----------------------------------------------------------------------------
struct GroundItem {
  std::string kind;            // consumable, reagent, figurine, equipment
  std::int32_t category = 0;   // definition category of the generated record
  std::int32_t count = 0;      // stack count
  std::array<std::uint8_t, items::ITEM_SIZE> record{}; // the 0x48-byte item record
};
struct LootOutcome {
  std::string ending;          // loot_generate_drops' result word
  std::vector<GroundItem> items;
  std::int32_t gold_objects = 0; // loot_spawnGold calls (never built here)
  std::uint64_t shared_draws = 0;
};
// Runs loot_generateDrops for one Creature_dropLoot call over the World's
// shared seed (0x404464). Each "make the item prop" step takes the one shared
// draw GameObject_ctor takes (items.md section 8) and records the item; no
// PhProp is built (the loot constructor's lump/model/texture/launch/scale
// arguments are not in the research). A positive Gold roll is refused.
//
// The level's monster level (g_levelMonsterLevel 0x4f714c) and the difficulty
// (g_difficultyIdx 0x4f719c) are NOT read from the World's bytes: nothing in the
// fight path stores them there (the World holds a 1.0 constructor default and a
// zero). The caller binds them from the level load (bind_level_globals); until
// then, or for a difficulty outside 1..5, roll() refuses rather than rolling
// with a default.
class Loot {
public:
  Loot(std::shared_ptr<const items::Tables>, Memory);
  void bind_level_globals(Raw monster_level_bits, std::int32_t difficulty);
  bool level_globals_bound() const { return level_bound_; }
  LootOutcome roll(const combat_runtime::LootDrop &, const std::string &reagent_filter);
  const items::Tables &tables() const { return *tables_; }
private:
  std::shared_ptr<const items::Tables> tables_;
  Memory mem_;
  bool level_bound_ = false;
  Raw level_ = 0;
  std::int32_t difficulty_ = 0;
};
} // namespace champions::simulation::rta::campaign_rewards
