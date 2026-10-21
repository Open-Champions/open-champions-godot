#pragma once
// The item, gold and skill script commands' callees, composed on the binder's
// memory. Research: t3/research-items 49bc578, docs/script-items.md,
// tools/ref_script_items.py. The handler halves (the loops over players, the
// medal-mode skip, the drop choice) are the binder's (script_builtins.cpp);
// this module answers the calls those halves make through
// Variables::inventory_service. inventory_giveItem, inventory_removeItem and
// inventory_equipItem are messages::rta::Machine (docs/GAME_MESSAGES.md), run
// over the binder memory.
//
// Nothing from the game is in this file: addresses, offsets and constants are
// the research's. Item tables come from the owner's executable at run time
// (image_tables) or from authored test data.
#include "champions/core_arithmetic.hpp"
#include "champions/items.hpp"
#include "champions/quests.hpp"
#include "champions/script_builtins.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::script_items {
namespace builtins = champions::simulation::rta::builtins;
inline constexpr const char *POLICY = "rta-script-items-v1";

// Globals (docs/script-items.md section 1).
inline constexpr std::uint32_t G_NUM_PLAYERS = 0x4d8ae0, G_PLAYERS = 0x4d8af0, REC_SIZE = 0x4c30;
// 0x4d8ae8: the word player_recalcStats tests for the +5000 load bonus. It has no
// writer in the executable (research items-gaps ecf9742 docs/spec/items-gaps.md 1:
// every load, store, addiu, ori, data word and block-copy search found only the
// reader at 0x1b69e0), so the image's 0 is a constant: recalc_load_limits never
// reads the binder memory for it, and nothing in the client writes it.
inline constexpr std::uint32_t G_LOAD_BONUS = 0x4d8ae8;
inline constexpr std::int32_t LOAD_BONUS_VALUE = 0;
inline constexpr std::uint32_t G_RAND_SEED = 0x404464;
// g_weaponValueMult (7 floats) and g_lootQualityRows (12 rows of 3 floats), read for the generator.
inline constexpr std::uint32_t A_WEAPON_MULT = 0x3e9cb0, A_QUALITY = 0x3e9c18, WEAPON_MULT_ROWS = 7, QUALITY_ROWS = 12;
inline constexpr std::uint32_t G_MEDAL_MISSION = 0x4f713c, G_MEDAL_PLAYER = 0x4f7148;
inline constexpr std::uint32_t G_ALIVE_LIST = 0x4f8440;
inline constexpr std::uint32_t G_DIFFICULTY = 0x4f719c;   // g_difficultyIdx, 1..5; the image holds 1
inline constexpr std::int32_t DIFFICULTY_IMAGE = quests::IMAGE_DIFFICULTY;
inline constexpr std::uint32_t G_CM1_WORD = 0x4f7110;     // -1 in the executable image
inline constexpr std::int32_t CM1_IMAGE = -1;
inline constexpr std::uint32_t G_CLASS_SKILLS = 0x4ef538, CLASS_SKILLS_SIZE = 0xa88, CLASS_SKILL_ROW = 0x38;
inline constexpr std::uint32_t G_SKILL_OBJECTS = 0x41b170;
inline constexpr std::int32_t SKILL_AXE_BANDOLIER = 0x49, SKILL_LIGHT_QUIVER = 0x4a;
// Player record offsets.
inline constexpr std::uint32_t P_CLASS = 0x50, P_EFF_STRENGTH = 0x90, P_PASSIVES = 0xef;
inline constexpr std::uint32_t P_LOAD = 0x120, P_LOAD_LIMIT = 0x124;
inline constexpr std::uint32_t P_BAGS = 0x130, BAG_SIZE = 0x1208, BAG_COUNT = 0x1200, ITEM_SIZE = 0x48;
inline constexpr std::int32_t BAG_SLOTS = 64, BAGS = 3;
inline constexpr std::uint32_t P_OBJECT = 0x43d8, P_SKILL_PAIRS = 0x4b08;
inline constexpr std::uint32_t OBJ_FLAGS = 0x0c, OBJ_DEAD = 0x8, OBJ_SLOT = 0x3c8;
// Load limit (player_recalcStats 0x1b6964-0x1b6a20) and item weight (0x1b6338).
inline constexpr std::int32_t LOAD_STRENGTH_BASE = 125;   // 0x1b6988
inline constexpr std::uint64_t PASSIVE_LOAD = 0x40, PASSIVE_AMMO = 0x10000;
inline constexpr Raw F_15 = 0x41700000, F_5000 = 0x459c4000, F_9999 = 0x461c3c00;
inline constexpr std::int32_t ITEM_SLOT_AMMO = 8;
inline constexpr std::uint64_t ITEM_FLAG_THROWN_AXE = UINT64_C(1) << 42;
// game_scanCm1Vars: the profile names at 0x53e170 + 8k, in the order tested.
inline constexpr std::array<const char *, 9> CM1_NAMES = {
    "m11cm1", "m9cm1", "m8cm1", "m7cm1", "m6cm1", "m5cm1", "m4cm1", "m2cm1", "m1cm1"};
inline constexpr std::int32_t CM1_NONE = 9;

// A call this port does not make: it stops the command (and, through the
// script host, the entry). Never answered with an invented value.
class Refused : public amx::HostFault {
public:
  using amx::HostFault::HostFault;
};

std::uint32_t rec(std::int32_t slot);
std::uint32_t bag_entry(std::int32_t slot, std::int32_t bag, std::int32_t index);
std::uint32_t bag_count_at(std::int32_t slot, std::int32_t bag);

// Byte memory the callees read and write: the binder memory itself, or an
// overlay over it for a call that runs messages::rta::Machine.
class View {
public:
  virtual ~View() = default;
  virtual std::uint64_t load(std::uint32_t address, unsigned size) const = 0;
  virtual void store(std::uint32_t address, unsigned size, std::uint64_t value) = 0;
  std::int32_t s8(std::uint32_t a) const;
  std::int32_t s16(std::uint32_t a) const;
  std::int32_t s32(std::uint32_t a) const;
  // Zero-terminated bytes; a string with no terminator in 0x4000 bytes refuses.
  std::string cstr(std::uint32_t address) const;
};
class BinderView : public View {
public:
  explicit BinderView(builtins::Memory &memory) : memory_(memory) {}
  std::uint64_t load(std::uint32_t, unsigned) const override;
  void store(std::uint32_t, unsigned, std::uint64_t) override;
private:
  builtins::Memory &memory_;
};

// inventory_countItemByName 0x1c5880: the 32-bit sum of the s16 stack counts
// of every entry (inside each bag's count, re-read per entry) whose definition
// name equals name exactly. The definition pointer is not tested.
std::int32_t count_item_by_name(const View &, const std::string &name, std::int32_t slot);
// inventory_findItemBySubstring 0x1c5a70: the last entry, of the last bag
// with a match, whose definition name occurs inside text. index -1 and no bag
// when nothing matched.
struct Found {
  std::int32_t index = -1;
  std::optional<std::int32_t> bag;
};
Found find_item_by_substring(const View &, std::int32_t slot, const std::string &text);
// player_getPassiveLevel 0x1bed48: 0 for an unsigned slot of 5 or more.
std::int32_t passive_level(const View &, std::int32_t slot, std::uint64_t mask);
// player_getSkillRank 0x1bf330: the pairs up to the first id -1. The original
// has no count limit; a walk that leaves the player record refuses.
std::int32_t skill_rank(const View &, std::int32_t slot, std::int32_t skill);

// The weight complements of Axe Bandolier and Light Quiver
// (docs/spec/skills-69-101.md 73 and 74): (skill object, rank) -> factor.
// Not part of this contract; without one a non-zero rank refuses.
struct Complements {
  std::function<Raw(std::uint32_t, std::int32_t)> light_quiver, axe_bandolier;
};
// item_getWeight 0x103e08 on the record's bytes.
Raw item_get_weight(const View &, const items::Tables &, std::uint32_t item);
// player_itemWeight 0x1b6338. A null item, or one whose definition pointer is
// 0, weighs 1.0.
Raw item_weight(const View &, const items::Tables *, const Complements &, std::int32_t slot,
                std::uint32_t item);
// player_recalcLoad 0x1bec10: +0x120 = 0, then one float add per entry inside
// the count, bag by bag. An empty entry inside the count weighs 1.0.
void recalc_load(View &, const items::Tables *, const Complements &, std::int32_t slot);
// The load limit player_recalcStats stores (0x1b6978-0x1b6a20):
// float(strength + 125) + float(passive 0x40) * 15.0, + 5000.0 when the word
// 0x4d8ae8 is non-zero, capped at 9999.0.
Raw load_limit(std::int32_t effective_strength, std::int32_t passive_load_level, std::int32_t bonus_word);
// The load-limit part of player_recalcStats over slots 0 .. g_numPlayers - 1:
// a record whose object pointer is 0 keeps its old limit; the passive is asked
// for the slot the object names (+0x3c8). The bonus word is read, never
// written: it has no writer, so the bonus is the constant 0 (LOAD_BONUS_VALUE).
void recalc_load_limits(View &);
// The handlers' weight test: load + weight < limit, or, when not strict, a
// weight that is exactly 0 (-0.0 too).
bool fits(const View &, std::int32_t slot, Raw weight, bool strict);

// game_scanCm1Vars 0x1eb950. recompute 0 returns the cached word 0x4f7110 and
// reads nothing else. Otherwise the index of the first non-zero profile
// variable of CM1_NAMES, or 9. profile_get must append an unknown name with 0
// as profile_getVar does.
std::int32_t scan_cm1_vars(std::int32_t recompute, const std::function<std::int32_t(const std::string &)> &profile_get,
                           std::uint32_t word);
// game_set4f7110 0x1ec830: the word = value, or game_scanCm1Vars(1) when value is -1.
void set_cm1_word(builtins::Memory &, std::int32_t value,
                  const std::function<std::int32_t(const std::string &)> &profile_get);

// The item tables item_parseDescriptor and item_getWeight read, from an image
// (the owner's executable, or authored bytes) at the addresses ref_items reads.
using ImageReader = std::function<std::vector<std::uint8_t>(std::uint32_t, std::size_t)>;
std::shared_ptr<items::Tables> image_tables(const ImageReader &);

// The service behind Variables::inventory_service for the item commands, and
// the ScriptGenerator_spawn scene call. One per Variables, which it must not
// outlive.
class Service {
public:
  // Host-side identity of the handler's 0x48-byte stack record. Never stored
  // in the binder memory; game code reaches it only through the record pointer.
  static constexpr std::uint32_t SCRATCH = 0xffff0000u;
  struct Call {
    std::string name;
    std::vector<std::uint64_t> args;
  };
  using Delegate = std::function<std::uint64_t(const std::vector<std::uint64_t> &)>;
  struct Counts {
    std::uint64_t parsed = 0, given = 0, lost = 0, removed = 0, equipped = 0;
    // Ground items built by ItemProp_ctor2 (one shared and one own draw each).
    std::uint64_t ground_items = 0;
    // Call-outs whose bodies are not in this contract and that the script host
    // does not own (player_countPotions, the rest of player_recalcStats, the
    // idle-animation reset, sounds, HUD text). Counted, not applied.
    std::map<std::string, std::uint64_t> deferred;
  };

  explicit Service(builtins::Variables &);
  Service(const Service &) = delete;
  Service &operator=(const Service &) = delete;

  // Without tables the parse and the weight of a parsed record refuse. The
  // definitions the records point at are read from the binder memory, which
  // must then hold the same image.
  void bind_tables(std::shared_ptr<const items::Tables>);
  bool has_tables() const { return tables_ != nullptr; }
  Complements complements;
  // Replaces a call-out (by name) for tests that pin the delegated calls to a
  // reference log. Every delegated call is recorded in calls.
  std::map<std::string, Delegate> delegates;
  std::vector<Call> calls;
  Counts counts;

  // world_createObject(typeName, position, angle, props) for
  // ScriptGenerator_spawn: the new object's address or 0. Without it a spawn
  // refuses. dynamic_type gives the object's RTTI name ("8Skeleton").
  std::function<std::uint32_t(const std::string &, const std::array<std::uint32_t, 3> &, std::int32_t,
                              std::uint32_t)> create_object;
  std::function<std::string(std::uint32_t)> dynamic_type;

  // The service entry point (Variables::inventory_service).
  builtins::Variables::InventoryResult call(const std::string &, const std::vector<builtins::Variables::InventoryValue> &);
  // ScriptGenerator_spawn 0x265fc4 (generator, typeName): docs/script-items.md 5.1.
  std::uint32_t generator_spawn(std::uint32_t generator, std::uint32_t type_name_at, const std::string &type_name);

  const std::array<std::uint8_t, ITEM_SIZE> &record() const { return scratch_; }

private:
  std::uint64_t delegate(const std::string &, const std::vector<std::uint64_t> &);
  std::uint64_t machine_service(View &, const std::string &, const std::vector<std::uint64_t> &);
  static constexpr std::int64_t GROUND_ITEM_BLOCK = 0x190;
  std::uint32_t scratch_definition() const;
  void blank_record();
  items::Game random_game() const;
  void ground_item(std::uint32_t block, const std::array<std::uint32_t, 3> &pos);
  template <class Body> auto with_machine(Body body);
  builtins::Variables &variables_;
  std::shared_ptr<const items::Tables> tables_;
  std::array<std::uint8_t, ITEM_SIZE> scratch_{};
};

// Commands this module lets through the script host's refusal gate, and the
// ones whose parse may draw from the shared seed.
bool routes(const char *command);
bool draws_seed(const char *command);
} // namespace champions::simulation::rta::script_items
