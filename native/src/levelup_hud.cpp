#include "champions/levelup_hud.hpp"
#include "champions/core_arithmetic.hpp"

namespace champions::levelup_hud {
namespace {
namespace F = champions::simulation::rta;

constexpr F::Raw kZero = 0;
constexpr F::Raw kOne = 0x3f800000u;
constexpr F::Raw kHalf = 0x3f000000u;
constexpr F::Raw kEight = 0x41000000u;
constexpr F::Raw kEightHalf = 0x41080000u;
constexpr F::Raw kNegEightHalf = 0xc1080000u;
constexpr F::Raw kQuarter = 0x3e800000u;
constexpr F::Raw kCell = 0x3d088888u;     // 1/30: one cell is 120 world units
constexpr F::Raw kRadius2 = 0x42800000u;  // 64
constexpr F::Raw kEighth = 0x3e000000u;
constexpr F::Raw k254 = 0x437e0000u;

std::int32_t s32(std::uint32_t value) { return static_cast<std::int32_t>(value); }
std::int32_t s8(int value) {
  return static_cast<std::int32_t>(static_cast<std::int8_t>(static_cast<std::uint8_t>(value)));
}
int rank_of(const std::function<int(int)> &rank, int id) { return rank ? rank(id) : 0; }
int passive_of(const std::function<int(std::uint64_t)> &rank, std::uint64_t mask) {
  return rank ? rank(mask) : 0;
}

// The reveal loop bounds. tools/ref_hud_inputs.py:92-104.
int low_edge(F::Raw cell) {
  const auto shifted = F::sub(cell, kEight);
  const int edge = F::le(kZero, shifted) ? F::to_int(F::add(shifted, kHalf))
                                          : F::to_int(F::add(cell, kNegEightHalf));
  return edge < 0 ? 0 : edge;
}
int high_edge(F::Raw cell, int limit) {
  const auto shifted = F::add(cell, kEight);
  const int edge = F::le(kZero, shifted) ? F::to_int(F::add(cell, kEightHalf))
                                          : F::to_int(F::sub(shifted, kHalf));
  const int cap = limit - 1;
  return cap < edge ? cap : edge;
}
} // namespace

int skill_grant(int level_being_left) {
  const int band = s8(level_being_left);
  if (band < 6) return 1;
  if (band < 16) return 2;
  if (band < 26) return 3;
  if (band < 36) return 4;
  return 5;
}

ExperienceResult give_experience(ExperienceState state, std::int32_t amount,
                                 const std::array<std::int32_t, 81> &table,
                                 const std::function<void(ExperienceState &)> &recalc) {
  // 32-bit wrap, then table[80] - 1 when that ceiling is below the sum.
  // hud-live-producers.md:86-92. levelup.md:28-31.
  const auto wrapped = s32(static_cast<std::uint32_t>(state.experience) + static_cast<std::uint32_t>(amount));
  const auto ceiling = s32(static_cast<std::uint32_t>(table[80]) - 1u);
  state.experience = ceiling < wrapped ? ceiling : wrapped;
  ExperienceResult out;
  while (true) {
    const int index = s8(state.level);
    // Level bytes outside 0..80 are Unknown. hud-live-producers.md:123.
    if (index < 0 || index > 80)
      throw Refusal("level byte outside the executed 0..80 table");
    if (state.experience < table[static_cast<std::size_t>(index)])
      break;
    // Word store 0x1d3214, byte store 0x1d3220. levelup.md:39-40, 51-53.
    state.skill_word += static_cast<std::uint32_t>(skill_grant(state.level));
    state.stat_byte = static_cast<std::uint8_t>(state.stat_byte + 3u);
    state.level = s8(index + 1);
    if (recalc)
      recalc(state);
    // Only a current-HP value above 0 is refilled; mana goes with it.
    // levelup.md:40-41, 60-61. hud-live-producers.md:108-111.
    if (F::lt(kZero, state.health)) {
      state.health = state.max_health;
      state.mana = state.max_mana;
    }
    ++out.steps;
  }
  out.state = state;
  return out;
}

const std::vector<Alternative> &alternatives() {
  // levelup.md:121-131, each row executed with and without the extra rank.
  static const std::vector<Alternative> rows = {
      {2, 0, 0x40, 0x3b}, {3, 1, 33, 0x30}, {3, 1, 74, 0x2f},
      {3, 0, 0x40, 0x3d}, {4, 1, 57, 1},    {2, 1, 97, 0x28},
      {2, 1, 40, 0x0b},   {4, 1, 95, 18},   {6, 1, 90, 86},
  };
  return rows;
}

SkillDecision skill_screen(const SkillAsk &ask) {
  if (ask.cursor == Cursor::Edge)
    throw Refusal("cursor-box edges of skills_screen were not probed");
  const bool skill_row = ask.kind != 0;
  if (skill_row && !ask.virtual_float_planted)
    throw Refusal("per-skill virtual bodies were not executed; the vtable+0x28 float must be planted");
  SkillDecision out;
  out.slot = ask.slot;
  out.kind = skill_row ? 1 : 0;
  // Passive: low 32 bits of the row mask. Skill: the skill id. levelup.md:150-153.
  out.value = static_cast<std::uint32_t>(ask.ident & 0xffffffffu);
  out.gate = ask.gate;
  // The first picture lookup at 0x1d47b4 leaves before the latch store at
  // 0x1d5010. levelup.md:94-96, 107-111.
  if (!ask.resource)
    return out;
  // Page 5 with panel +0x30 zero latches pad+8 & 2 into 0x4f4878.
  // levelup.md:107-111.
  const bool latches = ask.panel30 == 0 && ask.page == 5;
  const std::uint32_t latched = latches ? (ask.held & 2u) : ask.gate;
  // Outside cursor and confirm edge. levelup.md:98-107. The order of those
  // exits and the latch store is not given, so a call where the order would
  // change the stored word is refused.
  if (ask.cursor == Cursor::Outside || (ask.edge & ask.confirm_mask) == 0) {
    if (latched != ask.gate)
      throw Refusal("order of the page-5 latch store and the cursor/confirm exits was not executed");
    return out;
  }
  out.gate = latched;
  if (latched != 0)
    return out;
  // Prerequisite walk 0x1d5530-0x1d576c. levelup.md:114-119. The tests are on
  // the prerequisite value first: -1 passes, 0 needs skill 60 or 61.
  const int row_kind = skill_row ? 1 : 0;
  bool unlocked = false;
  bool zero_prerequisite = false;
  if (ask.pre == SkillAsk::kNoPrerequisite) {
    unlocked = true;
  } else if ((ask.pre & 0xffffffffu) == 0xffffffffu) {
    throw Refusal("prerequisite with a -1 low word and a nonzero high word: compare width was not executed");
  } else if (ask.pre == 0) {
    zero_prerequisite = true;
    unlocked = rank_of(ask.skill_rank, 60) >= 1 || rank_of(ask.skill_rank, 61) >= 1;
  } else if (ask.pre_kind == 0) {
    unlocked = passive_of(ask.passive_rank, ask.pre) >= 1;
  } else if (ask.pre_kind == 1) {
    unlocked = rank_of(ask.skill_rank, static_cast<int>(ask.pre & 0xffffffffu)) >= 1;
  } else {
    throw Refusal("prerequisite kind other than 0 or 1 was not executed");
  }
  if (!unlocked) {
    // The alternatives sit on the prerequisite chains. levelup.md:121-138.
    const Alternative *match = nullptr;
    for (const auto &alt : alternatives())
      if (alt.class_id == ask.class_id && alt.ident == ask.ident)
        match = &alt;
    if (match) {
      if (zero_prerequisite)
        throw Refusal("alternative unlocks after a failed prerequisite-0 check were not executed");
      if (match->kind != row_kind)
        throw Refusal("alternative ident on a row of the other kind was not executed");
      if (match->kind == 0 && ask.pre_kind == 1) {
        // The passive-0x40 compare is only on the passive-prerequisite chain.
        // levelup.md:133-136.
      } else if (match->kind == ask.pre_kind) {
        unlocked = rank_of(ask.skill_rank, match->skill) >= 1;
      } else {
        throw Refusal("skill-row alternatives were executed on the skill-prerequisite chain only");
      }
    }
  }
  if (!unlocked || !ask.in_table)
    return out;
  // Rank 20 does not send (0x1d5778). Level test 0x1d57a8-0x1d57bc. levelup.md:140-142.
  const int current = skill_row ? rank_of(ask.skill_rank, static_cast<int>(ask.ident & 0xffffffffu))
                                : passive_of(ask.passive_rank, ask.ident);
  if (current >= 20)
    return out;
  if (s8(ask.acknowledged) < ask.need + current)
    return out;
  // player_recalcStats 0x1d5950 runs from here even when the send is refused.
  // levelup.md:160-164.
  out.recalc = true;
  // Signed points must be above 0 (0x1d57e4, 0x1d5850). levelup.md:145-147.
  if (ask.points <= 0)
    return out;
  // c.ole.s against maximum mana +0xc4. levelup.md:152-158.
  if (skill_row && !F::le(ask.virtual_float, ask.max_mana))
    return out;
  out.sent = true;
  return out;
}

void reveal_world(std::vector<std::uint16_t> &grid, int width, int height, std::uint32_t origin_x,
                  std::uint32_t origin_y, std::uint32_t world_x, std::uint32_t world_y) {
  if (grid.size() != static_cast<std::size_t>(kAutomapSide * kAutomapSide) || width < 0 || height < 0 ||
      width > kAutomapSide || height > kAutomapSide)
    throw Refusal("automap reveal needs the 216 grid and a loaded size inside it");
  // cell = origin + world * 0.25 * 0x3d088888. hud-live-producers.md:153-157.
  const auto cell_x = F::add(origin_x, F::mul(F::mul(world_x, kQuarter), kCell));
  const auto cell_y = F::add(origin_y, F::mul(F::mul(world_y, kQuarter), kCell));
  const int y0 = low_edge(cell_y), y1 = high_edge(cell_y, height);
  const int x0 = low_edge(cell_x), x1 = high_edge(cell_x, width);
  if (y1 < y0 || x1 < x0)
    return;
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const auto dx = F::sub(cell_x, F::from_int(static_cast<std::uint32_t>(x)));
      const auto dy = F::sub(cell_y, F::from_int(static_cast<std::uint32_t>(y)));
      const auto radius2 = F::add(F::mul(dx, dx), F::mul(dy, dy));
      if (!F::le(radius2, kRadius2))
        continue;
      // Round half away from zero, clamp 255, keep the high nibble; the
      // greater nibble wins and bits 0..11 stay. hud-live-producers.md:158-161.
      const auto level = F::mul(F::sub(kOne, F::mul(F::sqrt(radius2), kEighth)), k254);
      int bits = F::le(kZero, level) ? F::to_int(F::add(level, kHalf)) : F::to_int(F::sub(level, kHalf));
      if (bits >= 0x100)
        bits = 0xff;
      const auto at = static_cast<std::size_t>(y * kAutomapSide + x);
      const auto old = grid[at];
      auto high = static_cast<unsigned>((old >> 8) & 0xffu);
      const unsigned nibble = static_cast<unsigned>(bits) >> 4;
      if ((high >> 4) < nibble)
        high = (high & 0x0fu) | ((nibble & 0xffu) << 4);
      grid[at] = static_cast<std::uint16_t>((old & 0x00ffu) | (high << 8));
    }
  }
}

} // namespace champions::levelup_hud
