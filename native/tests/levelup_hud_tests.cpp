// Synthetic vectors for docs/LEVELUP_HUD.md. The experience table is authored
// (i * 100); no executable word is copied. Expected values are the rules in
// research-levelup 98a8a47 docs/spec/levelup.md and research-hud-inputs
// fb24f5e docs/spec/hud-live-producers.md, cited per case.
#include "champions/levelup_hud.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/hud_dynamic.hpp"
#include "champions/strings.hpp"
#include "champions/ui_layout.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace champions::levelup_hud;
namespace F = champions::simulation::rta;
namespace S = champions::simulation::rta::strings;
namespace L = champions::ui_layout;

namespace {
int checks = 0;
void expect(bool ok, const char *name) {
  ++checks;
  if (!ok)
    throw std::runtime_error(name);
}
template <class Fn> void reject(Fn fn, const char *name) {
  bool threw = false;
  try {
    fn();
  } catch (const Refusal &) {
    threw = true;
  }
  expect(threw, name);
}

std::uint32_t bits(float value) {
  std::uint32_t out = 0;
  std::memcpy(&out, &value, 4);
  return out;
}

std::array<std::int32_t, 81> rising_table() {
  std::array<std::int32_t, 81> table{};
  for (int i = 0; i < 81; ++i)
    table[static_cast<std::size_t>(i)] = i * 100;
  return table;
}

ExperienceState alive_at(int level, std::int32_t experience) {
  ExperienceState state;
  state.level = level;
  state.experience = experience;
  state.health = bits(1.f);
  state.max_health = bits(40.f);
  state.mana = bits(3.f);
  state.max_mana = bits(9.f);
  return state;
}

// levelup.md:47-54, hud-live-producers.md:86-112.
void experience() {
  const auto table = rising_table();
  const std::pair<int, int> bands[] = {{5, 1}, {6, 2}, {15, 2}, {16, 3}, {25, 3},
                                        {26, 4}, {35, 4}, {36, 5}, {79, 5}};
  for (const auto &[old_level, grant] : bands) {
    auto state = alive_at(old_level, table[static_cast<std::size_t>(old_level)] - 1);
    state.skill_word = 41;
    state.stat_byte = 70;
    expect(skill_grant(old_level) == grant, "skill grant by the old level");
    const auto stayed = give_experience(state, 0, table, {});
    expect(stayed.steps == 0 && stayed.state.level == old_level && stayed.state.skill_word == 41 &&
               stayed.state.stat_byte == 70,
           "one below the table word does not level");
    const auto gained = give_experience(state, 1, table, {});
    expect(gained.steps == 1 && gained.state.level == old_level + 1 &&
               gained.state.skill_word == 41u + static_cast<unsigned>(grant) &&
               gained.state.stat_byte == 73 && gained.state.health == state.max_health &&
               gained.state.mana == state.max_mana,
           "exact word grants the 1-5 skill word, 3 stat points and the refill");
  }
  // Experience already equal to table[7] at level 5 takes levels 5, 6 and 7.
  auto multi = alive_at(5, table[7]);
  multi.skill_word = 4;
  multi.stat_byte = 1;
  const auto crossed = give_experience(multi, 0, table, {});
  expect(crossed.state.level == 8 && crossed.state.skill_word == 4 + 1 + 2 + 2 &&
             crossed.state.stat_byte == 10 && crossed.steps == 3,
         "multi-level award takes each threshold");
  auto wrapped = alive_at(5, table[5]);
  wrapped.stat_byte = 254;
  expect(give_experience(wrapped, 0, table, {}).state.stat_byte == 1, "stat byte 254 becomes 1");
  auto dead = alive_at(5, table[5]);
  dead.health = 0;
  const auto quiet = give_experience(dead, 0, table, {});
  expect(quiet.steps == 1 && quiet.state.health == 0 && quiet.state.mana == bits(3.f),
         "zero health leaves health and mana");
  auto negative = dead;
  negative.health = bits(-1.f);
  expect(give_experience(negative, 0, table, {}).state.health == bits(-1.f),
         "negative health is not refilled");
  // Cap: table[80] - 1 is stored when it is below the wrapped sum.
  auto capped = alive_at(5, static_cast<std::int32_t>(0x00a5a5a5));
  auto ceiling = table;
  ceiling[80] = 1000;
  const auto stored = give_experience(capped, 1, ceiling, {});
  expect(stored.state.experience == 999 && stored.state.level == 10, "ceiling table[80]-1 is stored");
  auto top = alive_at(80, 0);
  const auto clamp = give_experience(top, 0x7fffffff, table, {});
  expect(clamp.state.experience == table[80] - 1 && clamp.steps == 0, "level 80 stays below table[80]");
  // The refill reads the maxima recalc left. levelup.md:35-41.
  auto grown = alive_at(5, table[5]);
  const auto recalced = give_experience(grown, 0, table, [](ExperienceState &step) {
    step.max_health = bits(50.f);
    step.max_mana = bits(12.f);
  });
  expect(recalced.state.health == bits(50.f) && recalced.state.mana == bits(12.f),
         "refill uses the maxima after recalc");
  reject([&] {
    auto bad = alive_at(81, 0);
    give_experience(bad, 0, table, {});
  }, "level byte above 80 is outside the executed table");
}

SkillAsk skill_row(int class_id, std::uint64_t ident) {
  SkillAsk ask;
  ask.class_id = class_id;
  ask.points = 1;
  ask.acknowledged = 20;
  ask.max_mana = bits(10.f);
  ask.kind = 1;
  ask.need = 1;
  ask.ident = ident;
  ask.pre_kind = 1;
  ask.pre = 999;  // a skill nobody holds
  ask.cursor = Cursor::Inside;
  ask.edge = 0x40;
  ask.page = 5;
  ask.virtual_float = bits(10.f);
  ask.virtual_float_planted = true;
  ask.skill_rank = [](int) { return 0; };
  ask.passive_rank = [](std::uint64_t) { return 0; };
  return ask;
}

// levelup.md:92-164.
void skills() {
  // The two rows the task names, plus the class key of the table.
  auto knight = skill_row(4, 95);
  knight.skill_rank = [](int id) { return id == 18 ? 1 : 0; };
  const auto k = skill_screen(knight);
  expect(k.sent && k.recalc && k.kind == 1 && k.value == 95, "class 4 ident 95 also unlocks from skill 18");
  knight.skill_rank = [](int) { return 0; };
  const auto kn = skill_screen(knight);
  expect(!kn.sent && !kn.recalc, "class 4 ident 95 without skill 18 does not send");
  auto ranger = skill_row(6, 90);
  ranger.skill_rank = [](int id) { return id == 86 ? 1 : 0; };
  const auto r = skill_screen(ranger);
  expect(r.sent && r.value == 90, "class 6 ident 90 also unlocks from skill 86");
  ranger.skill_rank = [](int) { return 0; };
  expect(!skill_screen(ranger).sent, "class 6 ident 90 without skill 86 does not send");
  auto wrong_class = skill_row(6, 95);
  wrong_class.skill_rank = [](int id) { return id == 18 ? 1 : 0; };
  expect(!skill_screen(wrong_class).recalc, "the skill-18 alternative belongs to class 4");
  // Every executed alternative row, with and without its rank, on the chain
  // it was executed on: the passive-0x40 rows on the passive-prerequisite
  // chain, the skill rows on the skill chain. levelup.md:118-119, 133-138.
  for (const auto &alt : alternatives()) {
    auto row = skill_row(alt.class_id, alt.ident);
    row.kind = alt.kind;
    row.pre_kind = alt.kind;
    row.pre = alt.kind == 0 ? 0x1234 : 999;  // a passive or a skill nobody holds
    const int skill = alt.skill;
    row.skill_rank = [skill](int id) { return id == skill ? 1 : 0; };
    const auto got = skill_screen(row);
    expect(got.sent && got.kind == (alt.kind == 0 ? 0 : 1) && got.value == alt.ident,
           "alternative unlock sends the row on its executed chain");
    row.skill_rank = [](int) { return 0; };
    expect(!skill_screen(row).sent, "alternative row without the extra rank does not send");
  }
  // Mixed chains. The passive-0x40 compare is only on the passive-prerequisite
  // chain, so a skill prerequisite does not see it (levelup.md:133-136).
  for (const int cls : {2, 3}) {
    auto passive40 = skill_row(cls, 0x40);
    passive40.kind = 0;
    passive40.pre_kind = 1;
    passive40.pre = 999;
    passive40.skill_rank = [](int id) { return id == 0x3b || id == 0x3d ? 1 : 0; };
    const auto unseen = skill_screen(passive40);
    expect(!unseen.sent && !unseen.recalc, "passive 0x40 row with a skill prerequisite is not unlocked by the alternative");
  }
  // A skill alternative on the passive-prerequisite chain was not executed.
  auto skill_on_passive = skill_row(3, 33);
  skill_on_passive.pre_kind = 0;
  skill_on_passive.pre = 0x1234;
  skill_on_passive.skill_rank = [](int id) { return id == 0x30 ? 1 : 0; };
  reject([&] { skill_screen(skill_on_passive); }, "skill alternative on the passive chain is refused");
  auto skill_on_passive_95 = skill_row(4, 95);
  skill_on_passive_95.pre_kind = 0;
  skill_on_passive_95.pre = 0x1234;
  skill_on_passive_95.skill_rank = [](int id) { return id == 18 ? 1 : 0; };
  reject([&] { skill_screen(skill_on_passive_95); }, "class 4 ident 95 on the passive chain is refused");
  // A failed prerequisite 0 does not say which chain follows.
  auto zero_then_alt = skill_row(6, 90);
  zero_then_alt.pre = 0;
  zero_then_alt.skill_rank = [](int id) { return id == 86 ? 1 : 0; };
  reject([&] { skill_screen(zero_then_alt); }, "alternative after a failed prerequisite 0 is refused");
  zero_then_alt.skill_rank = [](int id) { return id == 60 ? 1 : 0; };
  expect(skill_screen(zero_then_alt).sent, "prerequisite 0 that passes needs no alternative");
  // An alternative ident on a row of the other kind was not executed.
  auto other_kind = skill_row(3, 33);
  other_kind.kind = 0;
  other_kind.pre_kind = 0;
  other_kind.pre = 0x1234;
  other_kind.skill_rank = [](int id) { return id == 0x30 ? 1 : 0; };
  reject([&] { skill_screen(other_kind); }, "alternative ident on a passive row is refused");
  // A row with no alternative is simply locked, on either chain.
  auto plain = skill_row(3, 34);
  plain.pre_kind = 0;
  plain.pre = 0x1234;
  plain.skill_rank = [](int) { return 1; };
  expect(!skill_screen(plain).recalc, "a row without an alternative stays locked");
  expect(alternatives().size() == 9, "nine executed alternatives");
  // Class 1 ident 55: mask 0x40 does not send. levelup.md:133-136.
  auto ident55 = skill_row(1, 55);
  ident55.passive_rank = [](std::uint64_t mask) { return mask == 0x40 ? 1 : 0; };
  const auto i55 = skill_screen(ident55);
  expect(!i55.sent && !i55.recalc, "class 1 ident 55 is not unlocked by passive 0x40");
  // Passive row: (slot, 0, low 32 bits of the mask), no float compare.
  auto passive = skill_row(0, 0x100000040ull);
  passive.kind = 0;
  passive.pre_kind = 0;
  passive.pre = SkillAsk::kNoPrerequisite;
  passive.slot = 1;
  passive.virtual_float_planted = false;
  passive.max_mana = 0;
  const auto mask = skill_screen(passive);
  expect(mask.sent && mask.slot == 1 && mask.kind == 0 && mask.value == 0x40,
         "passive row sends slot, 0, low mask word");
  // Float compare c.ole.s against maximum mana.
  auto cost = skill_row(0, 33);
  cost.pre_kind = 1;
  cost.pre = SkillAsk::kNoPrerequisite;
  cost.virtual_float = bits(11.f);
  const auto expensive = skill_screen(cost);
  expect(!expensive.sent && expensive.recalc, "11 against maximum 10 does not send and still recalcs");
  cost.virtual_float = bits(10.f);
  const auto priced = skill_screen(cost);
  expect(priced.sent && priced.kind == 1 && priced.value == 33, "10 against maximum 10 sends");
  // Points: signed word above 0.
  auto broke = cost;
  broke.points = 0;
  const auto unpaid = skill_screen(broke);
  expect(!unpaid.sent && unpaid.recalc, "zero points still recalc");
  broke.points = -1;
  expect(!skill_screen(broke).sent && skill_screen(broke).recalc, "negative points do not send");
  // Rank and acknowledged level.
  auto ranked = cost;
  ranked.acknowledged = 30;
  ranked.skill_rank = [](int id) { return id == 33 ? 20 : 0; };
  expect(!skill_screen(ranked).sent && !skill_screen(ranked).recalc, "rank 20 does not send");
  ranked.skill_rank = [](int id) { return id == 33 ? 19 : 0; };
  expect(skill_screen(ranked).sent, "rank 19 sends");
  auto low = cost;
  low.acknowledged = 0;
  expect(!skill_screen(low).sent && !skill_screen(low).recalc, "acknowledged 0 does not send a need-1 row");
  low.acknowledged = 1;
  expect(skill_screen(low).sent, "acknowledged 1 sends a need-1 row");
  // Prerequisite 0: skill 60 or 61.
  auto either = cost;
  either.pre_kind = 1;
  either.pre = 0;
  either.skill_rank = [](int id) { return id == 60 ? 1 : 0; };
  expect(skill_screen(either).sent, "prerequisite 0 passes on skill 60");
  either.skill_rank = [](int id) { return id == 61 ? 1 : 0; };
  expect(skill_screen(either).sent, "prerequisite 0 passes on skill 61");
  either.skill_rank = [](int) { return 0; };
  expect(!skill_screen(either).sent, "prerequisite 0 fails without 60 and 61");
  auto chain = cost;
  chain.pre_kind = 0;
  chain.pre = 0x80;
  chain.passive_rank = [](std::uint64_t m) { return m == 0x80 ? 1 : 0; };
  expect(skill_screen(chain).sent, "passive prerequisite needs that mask's rank");
  chain.pre_kind = 1;
  chain.pre = 7;
  chain.skill_rank = [](int id) { return id == 7 ? 1 : 0; };
  expect(skill_screen(chain).sent, "skill prerequisite needs that id's rank");
  // Prerequisite -1 is a value: it passes on either kind; a negative kind with
  // another value is not executed. levelup.md:114.
  auto none = cost;
  none.pre_kind = 0;
  expect(skill_screen(none).sent, "prerequisite value -1 passes with kind 0");
  none.pre_kind = 1;
  expect(skill_screen(none).sent, "prerequisite value -1 passes with kind 1");
  auto negative_kind = cost;
  negative_kind.pre_kind = -1;
  negative_kind.pre = 7;
  negative_kind.skill_rank = [](int id) { return id == 7 ? 1 : 0; };
  reject([&] { skill_screen(negative_kind); }, "prerequisite kind -1 with a real value is refused");
  negative_kind.pre_kind = 2;
  reject([&] { skill_screen(negative_kind); }, "prerequisite kind 2 is refused");
  auto low_word = cost;
  low_word.pre = 0x00000001ffffffffull;
  reject([&] { skill_screen(low_word); }, "a -1 low word with a high word is refused");
  // Pad, page, panel gate, resource and table walk.
  auto edge = cost;
  edge.edge = 0;
  expect(!skill_screen(edge).sent && !skill_screen(edge).recalc, "edge 0 does not send");
  edge.edge = 0x10;
  expect(!skill_screen(edge).sent, "edge 0x10 does not send");
  auto page = cost;
  page.held = 2;
  const auto latch = skill_screen(page);
  expect(!latch.recalc && latch.gate == 2, "page 5 latches held bit 2 and returns the word");
  // The word persists: a page-0 call with the stored word still skips.
  auto later = cost;
  later.page = 0;
  later.gate = latch.gate;
  const auto held_over = skill_screen(later);
  expect(!held_over.sent && !held_over.recalc && held_over.gate == 2, "the stored latch word skips the next call");
  later.page = 5;
  later.held = 0;
  const auto cleared = skill_screen(later);
  expect(cleared.sent && cleared.gate == 0, "page 5 without bit 2 clears the word and sends");
  // An exit whose order against the latch store is unknown is refused only
  // when the order would change the word.
  auto unordered = cost;
  unordered.held = 2;
  unordered.edge = 0;
  reject([&] { skill_screen(unordered); }, "confirm miss that would change the latch is refused");
  unordered.edge = 0x40;
  unordered.cursor = Cursor::Outside;
  reject([&] { skill_screen(unordered); }, "outside cursor that would change the latch is refused");
  unordered.gate = 2;
  expect(skill_screen(unordered).gate == 2, "outside cursor with an unchanged latch leaves it");
  auto no_art = cost;
  no_art.held = 2;
  no_art.resource = false;
  expect(skill_screen(no_art).gate == 0, "missing resource leaves before the latch store");
  page.page = 0;
  expect(skill_screen(page).sent, "page 0 does not latch held bit 2");
  page.held = 0;
  page.gate = 1;
  expect(!skill_screen(page).sent, "a preset gate word suppresses on page 0");
  page.page = 5;
  page.held = 2;
  page.panel30 = 1;
  page.gate = 0;
  expect(skill_screen(page).sent, "panel +0x30 == 1 keeps a zero gate on page 5");
  auto missing = cost;
  missing.resource = false;
  expect(!skill_screen(missing).recalc, "missing picture resource leaves before any send");
  auto ended = cost;
  ended.in_table = false;
  expect(!skill_screen(ended).recalc, "END before the row suppresses");
  auto outside = cost;
  outside.cursor = Cursor::Outside;
  expect(!skill_screen(outside).recalc, "a point outside the tree does not send");
  // Refused, not guessed.
  auto box = cost;
  box.cursor = Cursor::Edge;
  reject([&] { skill_screen(box); }, "inclusive cursor-box edges stay refused");
  auto body = cost;
  body.virtual_float_planted = false;
  reject([&] { skill_screen(body); }, "per-skill virtual bodies stay refused");
}

// The existing bar, message and alpha producers against the observed rules.
void hud_rules() {
  // trunc(S + cur/max*99), S = origin + 15, x = 27. hud-live-producers.md:40-55.
  const auto origins = L::hudOrigins(false, {true, false, false, false}, 1);
  expect(origins[0] + 15 == 27, "offline slot 0 S is 27");
  expect(L::barClipRight(27, 0.f, 4.f) == 27, "empty bar right edge is S");
  expect(L::barClipRight(27, 4.f, 4.f) == 126, "full bar right edge is S + 99");
  expect(L::barClipRight(27, 8.f, 4.f) == 225, "current above the maximum is not clamped");
  expect(L::barClipRight(27, 40.f, 0.f) == 0x7fffffff, "health 40 over maximum 0 saturates");
  expect(L::barClipRight(27, -5.f, 0.f) == std::numeric_limits<int>::min(), "mana -5 over maximum 0 saturates");
  expect(L::barClipRight(27, -1.f, 2.f) == -22, "negative current is not clamped");
  // Green bar: acknowledged level, wrapped differences. hud-live-producers.md:114-118.
  auto green = [](const std::vector<L::HudLayer> &layers) {
    for (const auto &layer : layers)
      if (layer.draw.resource.rfind("bar_green", 0) == 0)
        return layer.draw.clip.right;
    return -1;
  };
  const auto skipped = L::hudBaseLayers(12, 0, 1, 1, 1, 1, 350, 300, 400, 0);
  expect(green(skipped) == -1 && skipped.size() == 3, "acknowledged 0 skips the green bar and keeps mana");
  const auto half = L::hudBaseLayers(12, 0, 1, 1, 1, 1, 350, 300, 400, 4);
  expect(green(half) == L::barClipRight(27, 50.f, 100.f) && green(half) == 76, "green bar from T[ack-1]..T[ack]");
  const auto equal = L::hudBaseLayers(12, 0, 1, 1, 1, 1, 450, 400, 400, 4);
  expect(green(equal) == 0x7fffffff, "equal thresholds divide by zero and saturate");
  // cvt.s.w of differences above 2^24 goes through the core from_int, not the
  // host's round-to-nearest cast. hud-live-producers.md:116-117.
  auto as_float = [](std::uint32_t word) {
    float out = 0;
    std::memcpy(&out, &word, 4);
    return out;
  };
  const auto wide = L::hudBaseLayers(12, 0, 1, 1, 1, 1, 16777218, 0, 16777219, 4);
  const int core_fill = L::barClipRight(27, as_float(F::from_int(16777218u)), as_float(F::from_int(16777219u)));
  const int host_fill = L::barClipRight(27, static_cast<float>(16777218), static_cast<float>(16777219));
  expect(core_fill != host_fill && green(wide) == core_fill, "green differences convert with the core from_int");

  // hud_showMessage / hud_tickMessage, 360 frames and force 0. hud-live-producers.md:169-186.
  S::Hud hud;
  const S::Units text = {'L', 'v'};
  hud.inputs.local_menu = true;
  expect(!hud.show_message(text, kLevelMessageFrames, 0, kLevelMessageForce) && hud.msg_frames == 0,
         "force 0 with the local menu open does not store");
  hud.inputs.local_menu = false;
  expect(hud.show_message(text, kLevelMessageFrames, 0, kLevelMessageForce) && hud.msg_frames == 360 &&
             hud.msg_force == 0,
         "level-up message stores 360 frames, force 0");
  hud.inputs.local_menu = true;
  expect(!hud.show_message({'x'}, 12, 0, 0) && hud.msg_frames == 360, "the stored message is kept");
  hud.tick_message();
  expect(hud.msg_frames == 359, "one tick takes one frame");
  hud.inputs.suppressed = true;
  hud.tick_message();
  expect(hud.msg_frames == 359, "suppressed HUD holds the timer");
  // Alpha 0x4f66ec. hud-live-producers.md:188-193.
  namespace H = champions::hud_dynamic;
  expect(H::messageFrame(360, 10, false, false, 211).alpha == 13, "alpha rises by 3");
  expect(H::messageFrame(31, 9, false, false, 211).alpha == 124, "below 32 frames alpha is 4 * timer");
  expect(H::messageFrame(360, 126, false, false, 211).alpha == 128, "not below 129 becomes 128");
  expect(H::messageFrame(0, 40, false, false, 211).alpha == 40, "a zero timer leaves alpha");
  expect(H::messageFrame(0x8000, 40, false, false, 211).alpha == 40, "a negative signed halfword leaves alpha");
  expect(H::messageFrame(360, 40, false, true, 211).alpha == 40, "suppressed with force 0 leaves alpha");
  expect(H::messageFrame(360, 40, true, true, 211).alpha == 43, "suppressed with a force byte still steps");
}

// hud-live-producers.md:146-165; tools/ref_hud_inputs.py:77-132.
void automap() {
  std::vector<std::uint16_t> grid(static_cast<std::size_t>(kAutomapSide * kAutomapSide), 0x0abc);
  reveal_world(grid, 16, 16, 0, 0, 0, 0);
  expect((grid[0] & 0x0fff) == 0x0abc && (grid[0] >> 12) == 15, "centre raises bits 12..15 and keeps 0..11");
  expect(grid[9] == 0x0abc, "a cell 9 away stays");
  expect((grid[8] >> 12) == 0 && (grid[8] & 0x0fff) == 0x0abc, "radius 8 has level 0");
  // (1 - 4/8) * 254 = 127 -> nibble 7.
  expect((grid[4] >> 12) == 7, "radius 4 is nibble 7");
  std::vector<std::uint16_t> bottom(grid.size(), 0);
  reveal_world(bottom, 16, 16, 0, bits(15.f), 0, 0);
  expect((bottom[15 * kAutomapSide] >> 12) == 15 && bottom[16 * kAutomapSide] == 0,
         "the loop stops at the loaded height");
  std::vector<std::uint16_t> shifted(grid.size(), 0);
  reveal_world(shifted, 16, 16, 0, 0, bits(120.f), 0);
  expect((shifted[1] >> 12) == 15 && (shifted[0] >> 12) != 15, "one cell is 120 world units");
  std::vector<std::uint16_t> fraction(grid.size(), 0);
  reveal_world(fraction, 16, 16, 0, 0, bits(60.f), 0);
  expect((fraction[0] >> 12) > 0 && (fraction[0] >> 12) == (fraction[1] >> 12),
         "a half-cell position reveals both neighbours equally");
  std::vector<std::uint16_t> kept(grid.size(), 0x3000);
  reveal_world(kept, 16, 16, 0, 0, bits(240.f), 0);
  expect(kept[2] == 0xf000 && kept[9] == 0x3000, "the greater nibble wins");
  std::vector<std::uint16_t> origin(grid.size(), 0);
  reveal_world(origin, 16, 16, bits(3.f), bits(2.f), 0, 0);
  expect((origin[2 * kAutomapSide + 3] >> 12) == 15, "the origin words offset the cell");
  reject([] {
    std::vector<std::uint16_t> small(16);
    reveal_world(small, 4, 4, 0, 0, 0, 0);
  }, "only the 216 grid");
}

} // namespace

int main() {
  try {
    experience();
    skills();
    hud_rules();
    automap();
  } catch (const std::exception &error) {
    std::cerr << "FAIL levelup-hud: " << error.what() << "\n";
    return 1;
  }
  std::cout << "PASS levelup-hud " << checks << " checks\n";
  return 0;
}
