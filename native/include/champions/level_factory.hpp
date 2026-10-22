#pragma once
// Which objects.ob records world_objectFactory 0x159c94 turns into objects.
// Research t3/research-script-session 6841b8a, docs/script-session.md section 3
// and tools/ref_level_factory.py (executed against every record of the disc's
// level files, 0 mismatches). The test strings and property keys are named by
// their addresses and read from the owner's executable at run time; nothing
// read is kept beyond the call.
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::level_factory {

// The owner's executable: a C string at a virtual address, and one byte (the
// C library class table at CTYPE + signed character). Either may throw.
struct Executable {
  std::function<std::string(std::uint32_t)> string;
  std::function<std::uint8_t(std::uint32_t)> byte;
};

inline constexpr std::uint32_t CTYPE = 0x5628e1;
inline constexpr std::uint32_t CLEAR_COLOR_WORD = 0x506044;

enum class Kind : std::uint8_t {
  Object,     // pool_new(size), then the last of calls is the constructor
  Call,       // calls a function and builds no object
  ClearColor, // stores g_backgroundColor 0x506044
  Nothing,    // no test and no fall-back matched: the factory returns 0
};

struct Outcome {
  Kind kind = Kind::Nothing;
  // The string address of the name test that matched; 0 for a fall-back.
  std::uint32_t test = 0;
  std::uint32_t size = 0;
  std::vector<std::uint32_t> calls;
  std::uint32_t constructor() const { return kind == Kind::Object ? calls.back() : 0; }
};

// props null: a null property list (every property test fails). A pool_new
// that fails is not modelled: the caller allocates.
Outcome classify(const std::string &class_name,
                 const std::vector<std::string> *props, const Executable &);

// Name-test and property-key comparisons of the factory (libc_strcasecmp,
// str_prefixDiffers, props_getInt / props_getStr after lowering the key) and
// libc_atoi (newlib strtol base 10, 64-bit long, then the low 32 bits).
bool str_eq(const std::string &, const std::string &, const Executable &);
bool str_prefix(const std::string &text, const std::string &prefix, const Executable &);
std::optional<std::string> prop_find(const std::vector<std::string> *props,
                                     const std::string &key, const Executable &);
std::uint32_t atoi(const std::string &, const Executable &);

// Whether the object a row builds is a Creature (object flag 0x20, set only by
// Creature_ctor 0x109d68). Decided per factory row from the research's probe
// (research t3/research-items-gaps ecf9742 docs/spec/hide-monster.md 3-4,
// native/src/creature_class_table.inc). Unknown: the outcome has no name-test
// row (a property fall-back, the Line/WaveMaker row) or the probe did not decide.
enum class Creature : std::uint8_t { No, Yes, Unknown };
Creature creature_kind(const Outcome &);

struct Row {
  bool prefix;
  std::uint32_t name;
  std::uint32_t size; // 0: the row builds no object
  std::uint32_t calls[2];
  std::uint8_t call_count;
  bool clear_color;
};
const std::vector<Row> &tests();

} // namespace champions::simulation::rta::level_factory
