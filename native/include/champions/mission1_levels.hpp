#pragma once
// Mission 1 level rules that are not script-image contracts: the cutscene file
// layout, cut_setup's actor search, the object key order on a bound actor, and
// the Unknowns that stay refused. Research t3/research-strife-drundar de4aafe,
// docs/mission1-levels.md (cited below as doc:LINE) and its PASS audit
// research-mission1-levels-fix2. Nothing here names a level, record or actor:
// classes and positions come from the caller's cutscene file and objects.
#include "champions/level_flow.hpp"
#include <array>
#include <map>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::mission1_levels {
struct CutError : std::runtime_error { using std::runtime_error::runtime_error; };

// ---- Cutscene file. doc:338-343: header word +0 flags, +8 key count, +0x10
// actor count; actor records 0x2c bytes (class +8, position +0x1c, create byte
// +0x2b); keys 20 bytes. The key and actor table offsets are header words +4
// and +0xc, and a key starts time f32, actor s16, type s16
// (tools/difftest_mission1_levels.py:1024,1028-1030,1041).
inline constexpr std::size_t CUT_HEADER = 0x14, CUT_ACTOR = 0x2c, CUT_KEY = 20;
inline constexpr std::size_t CUT_CLASS_AT = 8, CUT_CLASS_END = 0x1c, CUT_POSITION_AT = 0x1c,
                             CUT_CREATE_AT = 0x2b;
// Host bounds, not game facts: the largest observed file has 64 keys and 5 actors.
inline constexpr std::uint32_t CUT_MAX_KEYS = 4096, CUT_MAX_ACTORS = 256;
// Object key types (doc:341-343): 3 and 4 call the object's virtual slot 3
// (trigger) with 1 and 0; 8 puts the object back on the grid.
inline constexpr std::int16_t KEY_TRIGGER_ON = 3, KEY_TRIGGER_OFF = 4, KEY_SHOW = 8;

using Vec3f = std::array<float, 3>;
struct CutActor {
  std::string cls;
  Vec3f position{};
  bool create = false; // the cutscene makes this actor; it is not searched for
};
struct CutKey {
  float time = 0;
  std::int16_t actor = -1, kind = 0;
};
struct Cut {
  std::uint32_t flags = 0;
  std::vector<CutKey> keys;
  std::vector<CutActor> actors;
};
Cut parse_cut(const std::uint8_t *data, std::size_t size);
inline Cut parse_cut(const std::vector<std::uint8_t> &bytes) { return parse_cut(bytes.data(), bytes.size()); }

// ---- cut_setup actor search, 0x19b098-0x19b324 (doc:345-379). The constants
// were decoded from the instruction fields (doc:348-353).
inline constexpr float BIND_HALF_XY = 240.0f, BIND_HALF_Z = 100000.0f, BIND_FIRST = 600.0f;
inline constexpr std::uint32_t BIND_CAPACITY = 0x40;  // a3 at 0x19b0f8
inline constexpr std::uint32_t BIND_KEPT = BIND_CAPACITY - 1; // first 63 in walk order (doc:369-370)
inline constexpr std::uint32_t BOUND_FLAG = 0x80000;  // an already bound object is skipped (doc:355-356)
struct Box { Vec3f lo{}, hi{}; };
// The world_collectObjectsInBox query box: actor position +-240 on x and y,
// +-100000 on z, each bound rounded to f32 (doc:346-348).
Box actor_box(const Vec3f &position);
// The C++ type name as cut_setup compares it: the type-info string with its
// leading digits skipped (doc:357-358; "7Player1" gives "Player1", doc:362-363).
std::string compared_type_name(const std::string &type_info);

// Where an object in the query result came from. Only placed records, player
// slots and the cutscene's own created actors were counted by the research
// (doc:370-375). The rest are Unknown in the capacity count (doc:376-379,
// 543-545; audit fix2 F2-2 adds objects a constructor or init script makes).
enum class Origin { Placed, PlayerSlot, CutsceneCreated, RunTimeSpawn, Moved, LoadSpawn };
struct Candidate {
  std::string class_string; // the factory class name at +0x34
  std::string type_info;    // the type-info string at virtual table + 4, unskipped
  Vec3f position{};
  std::uint32_t flags = 0;
  Origin origin = Origin::Placed;
};
enum class BindStatus { Bound, NoMatch, Created, Refused };
struct Binding {
  BindStatus status = BindStatus::NoMatch;
  std::optional<std::size_t> index; // into the query result
  bool by_class_string = false;
  float distance = 0;
  std::size_t kept = 0;   // query results inside the capacity
  bool truncated = false; // the query returned more than the capacity keeps
  std::string reason;     // for Refused
};
// `query` is the world_collectObjectsInBox result over actor_box(actor), in
// walk order and before the capacity cut. Created actors are not searched.
Binding bind_actor(const CutActor &actor, const std::vector<Candidate> &query);

// ---- Object keys on one bound actor, in key order (doc:313-317, 380-382).
struct TriggerCall {
  float time = 0;
  std::int32_t argument = 0; // the slot-3 argument: 1 for a type-3 key, 0 for type 4
};
struct Slot3 {
  bool ok = false;
  std::vector<TriggerCall> calls;
  std::string reason;
};
// Refused when the keys are not in ascending time: only that order was
// observed (doc:380-382; SLOT3_ORDER, audit fix2 N-3).
Slot3 slot3_calls(const Cut &, std::size_t actor);
// Actors with a show key (type 8), ascending and unique (doc:384-394 "Shows").
std::vector<std::size_t> shown_actors(const Cut &);

// ---- Record facts the levels rely on (doc:169-184, 223-228, 271-277; the
// research asserts them as RECORD_FACTS, CLASS_KEYS, CLASS_VALUES and DROPS).
struct RecordFacts {
  std::map<std::string, std::size_t> drops;       // `dropitem` value -> records
  std::map<std::string, std::size_t> propertyless; // class -> records with no properties
  // class -> each record's property keys in record order
  std::map<std::string, std::vector<std::vector<std::string>>> keys;
};
RecordFacts record_facts(const std::vector<level_flow::Record> &);
// The value of `key` on the record whose `name` is `name` (and class `cls`),
// when exactly one record matches; otherwise nullopt.
std::optional<std::string> named_value(const std::vector<level_flow::Record> &, const std::string &cls,
                                       const std::string &name, const std::string &key);
// A record's `t`/`script` value names an entry only when it is not the record's
// own placeholder `<empty>` (doc:178) and is non-empty.
bool names_entry(const std::string &value);

// ---- Unknowns that stay refused (doc:520-550).
enum class Open {
  DoorStateAfterTrigger, // the door state after doors, cazic and bossdefeated
  ShownStaysHidden,      // whether hideMonster's flags are cleared by a show key
  RunTimeObjectsInSearch,// run-time spawns, moved or load-made objects in the capacity count
  CutscenePlayback,      // real playback, length and camera
  UnrequestedCutscene,   // a cutscene file no script requests
  DemoBranch,            // exitzone's demoRunning branch
  PropertylessLive,      // property-less creatures live from load (Inferred only)
};
const char *open_reason(Open);
} // namespace champions::simulation::rta::mission1_levels
