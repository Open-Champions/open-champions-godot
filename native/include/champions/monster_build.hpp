#pragma once
// arena_spawnMonster 0x3065fc: where an arena monster is placed, its facing,
// flags, per-class scale, SpellArena record and states. Research:
// t3/research-items 49bc578, docs/script-items.md 5.2, tools/ref_monster_build.py.
// The class constructor world_createObject reaches is not this contract; the
// world is reached through explicit calls, and a missing call refuses.
#include "champions/core_arithmetic.hpp"
#include "champions/script_builtins.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::monster_build {
namespace builtins = champions::simulation::rta::builtins;
using Vec3 = std::array<Raw, 3>;

inline constexpr std::uint32_t G_WAVE = 0x8e7338;          // s16
inline constexpr std::uint32_t G_RECIPE_COUNT = 0x503886;  // s16, 5 in the image
inline constexpr std::uint32_t G_RING_ANGLE = 0x8e7360, G_RING_STEP = 0x8e7364;
inline constexpr std::uint32_t G_RING_CENTRE = 0x503888;   // pointer to the centre vector
inline constexpr std::uint32_t G_ARENA_PROPS = 0x8e7230;
inline constexpr std::uint32_t G_RAND_SEED = 0x404464;
inline constexpr std::size_t ARENA_CLASS_COUNT = 30;       // the names at 0x5037f0
inline constexpr Raw RING_RADIUS = 0x43480000;             // 200.0 (0x306768)
inline constexpr std::int32_t SPOT_RANGE = 0x60, SPOT_RADIUS = 0x18;
inline constexpr std::uint32_t RETRY_STEP = 0xfa0, RETRY_LIMIT = 0x10000, FACING_FLIP = 0x8000;
inline constexpr std::uint32_t OBJ_FLAG_ARENA = 0x2;
inline constexpr std::uint32_t HOME_RANGE = 5000;
inline constexpr int VSLOT_HOME_RANGE = 34, VSLOT_AI_STATE = 27;
inline constexpr std::uint32_t SPELL_SIZE = 0x18, VT_SPELL_ARENA = 0x51b4f8;

// The world calls the builder makes, in the original's order.
struct ArenaCalls {
  // world_findFreeSpot(out, point, 0x60, 0x18, &ok, 0): (spot, ok byte).
  std::function<std::pair<Vec3, std::uint32_t>(const Vec3 &, std::int32_t, std::int32_t, std::int32_t)> find_free_spot;
  // world_createObject(name, spot, facing, props): the object or 0.
  std::function<std::uint32_t(const std::string &, const Vec3 &, std::int32_t, std::uint32_t)> create_object;
  // True for a class whose states stay refused (combat-ctors keeps the
  // AntQueen and Skeleton states refused). Asked before anything is written.
  std::function<bool(std::uint32_t)> refused_class;
  std::function<void(std::uint32_t, const std::array<Raw, 5> &)> scale_stats;  // Creature_scaleStats
  std::function<void(std::uint32_t)> clear_status;                            // Creature_clearStatus
  std::function<std::uint32_t(std::uint32_t)> pool_new;
  std::function<void(std::uint32_t, std::uint32_t)> add_spell;                 // creature_addSpell
  std::function<void(int, std::uint32_t, std::uint32_t)> virtual_call;        // slot, object, value
};

// float((s16 wave / 3) / s16 recipe count), both toward zero. A recipe count
// of 0 refuses (MIPS DIV leaves LO unpredictable).
Raw arena_cycle(std::uint32_t wave, std::uint32_t recipe_count);
// cycle + ADD[k] for k < 30 (0x306670-0x306760). A name outside the table
// reads an unwritten stack word: refused.
Raw arena_scale(Raw cycle, std::size_t index);
// (cx + cos16(a) * 200, cy + sin16(a) * 200, cz).
Vec3 ring_point(const Vec3 &centre, std::uint32_t angle);

// arena_spawnMonster(className or none, creature). class_names are the 30
// names of 0x5037f0, from the owner's executable (or authored test names).
// Returns the object, or nothing when the ring search gave up.
std::optional<std::uint32_t> arena_spawn_monster(builtins::Memory &, const ArenaCalls &,
                                                 const std::optional<std::string> &class_name,
                                                 std::uint32_t creature,
                                                 const std::vector<std::string> &class_names);
} // namespace champions::simulation::rta::monster_build
