#pragma once
#include "champions/core_math.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Per-tick creature decisions for the twelve hostile classes of missions 1-3
// (RTA US v2.00). Authored port of research branch t3/research-campaign-combat
// at fbb2e1d (audits research-creature-ai-m1-3, -fix, -fix2: PASS, 4 LOW).
// Citations "C:<line>" are docs/creature-ai-m1-3.md, "M2:<line>" are
// docs/monsters-2.md, "MOD:<line>" are tools/models_creature_ai_m1_3.py and
// "REF:<line>" tools/ref_creature_ai_m1_3.py, all at fbb2e1d.
//
// The module is a decision kernel: the caller answers every query (sight,
// cone, see, counts, hitAtReach) and executes every action it emits. Only
// claims labelled Executed or Observed are ported. Unknowns refuse:
// Goblin 7/8, Froglock 7/8 and Kobold 9 bodies, Cazic/Critter/Catapult runs,
// the IronGuardian Droid spawn, HP rules inside the stubbed base
// Creature_run/processDamage (C:145-146, C:528-529, C:573-580).
namespace champions::simulation::rta::creature_ai_m1_3 {
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };

enum class Class : unsigned {
  Kobold, WarWraith, Orc, Goblin, Ogre, Warrior, Clockwork, Droid,
  IronGuardian, Froglock, Amphian, SeaMonster
};
inline constexpr unsigned CLASS_COUNT = 12;

struct ClassInfo {
  Class cls;
  const char *name;
  // REF:30-47 (constructor from C:41-56), run/setState/vtable/pool size.
  std::uint32_t constructor, run, set_state, vtable, size;
  Raw knockback;                // C:77-80, Executed head() scale
  std::optional<Raw> corpse;    // C:148-150 deadUpdate distance (none: own death body)
  // Vtable words, Observed from the loaded image (C:86-95, REF via SLOTS).
  std::uint32_t slot2, slot20, slot21, slot23, slot24, slot25;
  std::uint32_t home_range;     // C:103-106 (base 1000, Observed)
};
const ClassInfo &info(Class);
std::optional<Class> class_of_constructor(std::uint32_t constructor);

// --- Level records (C:28-60, REF:16-56): per-level creature record counts
// and the property-key counts; counts and key names only, no values.
struct LevelCreature {
  const char *level, *type;
  unsigned count;
  std::uint32_t pool_size, constructor;
};
const std::vector<LevelCreature> &level_creatures();
const std::map<std::string, unsigned> &level_record_totals();
struct KeyCount {
  const char *level, *type;
  std::vector<const char *> with, without;
  unsigned count;
};
const std::vector<KeyCount> &key_counts();
// Factory pick for one placed record, by type name and level (C:33-34, C:58-59).
// Unlisted names refuse; Critter types are listed (no run is ported for them).
const LevelCreature &factory_pick(const std::string &level, const std::string &type);

// --- One tick ------------------------------------------------------------
struct Event {
  std::string name;
  std::vector<double> args;
  bool operator==(const Event &o) const { return name == o.name && args == o.args; }
};
struct Context {
  std::uint32_t state = 0;       // +0x232
  bool target = false;           // +0x234 handle present
  std::uint32_t ev = 0;          // Creature_stepAnim result (animation-event mask)
  std::uint32_t anim = 0;        // +0x11a animation flags
  std::int64_t clock = 100;      // +0x300
  std::int64_t deadline = 0;     // +0xf4
  std::map<std::uint32_t, std::int64_t> fields;  // class fields as stored
  Raw hp = 0x42c80000u, max_hp = 0x42c80000u;    // +0x228 / +0x22c floats
  std::uint32_t alive_players = 0;               // 0x4f8420
  bool ally = false;             // Kobold: an ally handle is held (MOD:101)
  std::uint32_t holder_hand1 = 0;// Froglock holder word +0x7c (MOD:892)
  std::int32_t dmg_low = 0, dmg_high = 0, rating = 0;  // +0xd0/+0xd2/+0xd4
  bool base_run_ends = false;    // Creature_run 0x112948 returned nonzero
  bool active = true;            // Creature_updateActive 0x10c0f0 result
  std::uint32_t seed = 0;        // own generator +0x44 (GameObject_rand 0x1509d0)
  std::int64_t field(std::uint32_t off) const {
    const auto it = fields.find(off);
    return it == fields.end() ? 0 : it->second;
  }
};
// Answers queries in order; unanswered queries read 0 like the probe (REF:110-114).
using Answer = std::function<std::int64_t(const Event &)>;
struct Tick {
  std::vector<Event> events;
  std::uint32_t seed = 0;
  unsigned draws = 0;
  std::vector<std::uint32_t> requests() const;  // slot-27 requests in order
  bool has(const std::string &name) const;
};
Tick tick(Class, const Context &, const Answer & = {});

// --- setState (slot 27) ------------------------------------------------------
struct SetContext {
  bool target = false;
  std::uint16_t root = 0, cooldown = 0, stun = 0, x31a = 0;  // +0x314/+0x318/+0x31c/+0x31a
  std::int8_t x231 = 0;
  // Caller's global network/PvP words (monsters_1 filter 0x4f7120/0x4f711c/
  // 0x503890/0x503894): when they refuse, every request is refused.
  bool global_refuses = false;
};
// One entry write: Exact value, clock + base + draw % mod, or base + draw % mod.
struct EntryWrite {
  enum Kind { Exact, ClockDraw, AbsDraw, Clock } kind;
  std::uint32_t offset;
  std::int64_t base;
  std::uint32_t mod;
  std::int64_t resolve(std::int64_t clock, const std::function<std::uint32_t()> &draw) const;
};
struct SetResult {
  bool refused = false;
  std::uint32_t requested = 0, filtered = 0, landed = 0;
  std::vector<std::pair<std::uint32_t, std::int64_t>> cleared;  // filter writes
  bool target_cleared = false;
  // False when the executed entry table (MOD:1164-1196) has no row for the
  // filtered state: the state is stored, but other entry writes are unknown.
  bool entry_known = false;
  std::vector<EntryWrite> writes;
  std::vector<std::string> calls;   // loot / death calls by name
  std::vector<std::string> absent;  // names asserted not called
  std::vector<std::uint32_t> unwritten;
};
SetResult set_state(Class, const SetContext &, std::uint32_t requested);

// --- Hits (C:156-185) ------------------------------------------------------
// Creature_hitTargetAtReach 0x1196e8 reach test: Euclidean distance of the
// target centre from attacker position + facing x reach strictly below 60;
// radii do not enter; object flag 8 skips (C:166-179).
struct ReachProbe {
  Vec3 attacker{}, target{};
  std::uint32_t facing = 0;
  Raw reach = 0x42100000u;  // 36
  std::uint32_t target_flags = 0;
};
bool hit_at_reach(const ReachProbe &);
} // namespace champions::simulation::rta::creature_ai_m1_3
