#include "champions/monster_build.hpp"
#include "champions/core_math.hpp"
#include "champions/script_items.hpp"

namespace champions::simulation::rta::monster_build {
namespace {
using script_items::Refused;
std::int32_t s16v(std::uint32_t v) { return s16(v & 0xffffu); }
// The additive constants of the scale table (0x306670-0x306760).
Raw scale_add(std::size_t k) {
  switch (k) {
  case 5: return 0x3f0666a9;
  case 6: return 0x3f40c4cd;
  case 7: return 0x3f12b042;
  case 8: return 0x3f66a800;
  case 10: return 0x3f3a9ff0;
  case 14: return 0x3f410624;
  default: return ONE;
  }
}
template <class F> const F &need(const F &f, const char *what) {
  if (!f)
    throw Refused(std::string("arena_spawnMonster: ") + what + " is not bound");
  return f;
}
std::int32_t tdiv(std::int32_t a, std::int32_t b) {
  if (b == 0)
    throw Refused("arena_spawnMonster: a recipe count of 0");
  const auto q = static_cast<std::int64_t>(a) / b; // C++ truncates toward zero
  return static_cast<std::int32_t>(q);
}
} // namespace

Raw arena_cycle(std::uint32_t wave, std::uint32_t recipe_count) {
  return from_int(static_cast<std::uint32_t>(tdiv(tdiv(s16v(wave), 3), s16v(recipe_count))));
}
Raw arena_scale(Raw cycle, std::size_t index) {
  if (index >= ARENA_CLASS_COUNT)
    throw Refused("arena_spawnMonster: the class name is not in the 30-entry table");
  return add(cycle, scale_add(index));
}
Vec3 ring_point(const Vec3 &centre, std::uint32_t angle) {
  const auto a = static_cast<std::uint32_t>(s16v(angle));
  return {add(centre[0], mul(cos16(a), RING_RADIUS)), add(centre[1], mul(sin16(a), RING_RADIUS)), centre[2]};
}

std::optional<std::uint32_t> arena_spawn_monster(builtins::Memory &mem, const ArenaCalls &calls,
                                                 const std::optional<std::string> &class_name,
                                                 std::uint32_t creature,
                                                 const std::vector<std::string> &class_names) {
  const auto cycle = arena_cycle(mem.u16(G_WAVE), mem.u16(G_RECIPE_COUNT));
  const auto centre_at = mem.word(G_RING_CENTRE);
  std::uint32_t tries = 0;
  std::uint32_t angle = 0;
  Vec3 spot{};
  for (;;) {
    angle = mem.u16(G_RING_ANGLE);
    const Vec3 centre{mem.word(centre_at), mem.word(centre_at + 4), mem.word(centre_at + 8)};
    const auto found = need(calls.find_free_spot, "world_findFreeSpot")(ring_point(centre, angle), SPOT_RANGE,
                                                                        SPOT_RADIUS, 0);
    spot = found.first;
    if (found.second & 0xffu)
      break;
    tries += RETRY_STEP;
    mem.store(G_RING_ANGLE, 2, (angle + RETRY_STEP) & 0xffffu);
    if (tries > RETRY_LIMIT)
      return std::nullopt;
  }
  const auto facing = s16v(angle - FACING_FLIP);
  std::uint32_t obj = 0;
  if (class_name) {
    obj = need(calls.create_object, "world_createObject")(*class_name, spot, facing, G_ARENA_PROPS);
    if (obj == 0)
      throw Refused("arena_spawnMonster: world_createObject returned 0 (the original writes flags at 0xc)");
    if (need(calls.refused_class, "the refused-class test")(obj))
      throw Refused("arena_spawnMonster: this class's states stay refused");
    mem.word(obj + 0x0c, mem.word(obj + 0x0c) | OBJ_FLAG_ARENA);
    std::size_t index = 0;
    while (index < ARENA_CLASS_COUNT && (index >= class_names.size() || class_names[index] != *class_name))
      ++index;
    const auto scale = arena_scale(cycle, index);
    need(calls.scale_stats, "Creature_scaleStats")(obj, {scale, scale, scale, scale, scale});
  } else {
    obj = creature;
    if (need(calls.refused_class, "the refused-class test")(obj))
      throw Refused("arena_spawnMonster: this class's states stay refused");
    for (std::uint32_t k = 0; k < 3; ++k)
      mem.word(obj + 0x10 + 4 * k, spot[k]);
    mem.byte(obj + 0x231, 0);
    mem.store(obj + 0x38, 2, static_cast<std::uint32_t>(facing) & 0xffffu);
    need(calls.clear_status, "Creature_clearStatus")(obj);
  }
  const auto spell = need(calls.pool_new, "pool_new")(SPELL_SIZE);
  if (spell != 0) {
    const auto seed = mem.word(G_RAND_SEED) * 0x19660du + 0x3c6ef35fu;
    mem.word(G_RAND_SEED, seed);
    mem.byte(spell + 0x00, 0);
    mem.word(spell + 0x04, 0);
    mem.word(spell + 0x08, 0);
    mem.word(spell + 0x0c, obj);
    mem.word(spell + 0x10, seed >> 16);
    mem.word(spell + 0x14, VT_SPELL_ARENA); // Spell's table first, then SpellArena's
  }
  need(calls.add_spell, "creature_addSpell")(obj, spell);
  need(calls.virtual_call, "the virtual calls")(VSLOT_HOME_RANGE, obj, HOME_RANGE);
  calls.virtual_call(VSLOT_AI_STATE, obj, 0);
  mem.store(G_RING_ANGLE, 2, (mem.u16(G_RING_ANGLE) + mem.u16(G_RING_STEP)) & 0xffffu);
  return obj;
}
} // namespace champions::simulation::rta::monster_build
