#include "champions/arena_modes.hpp"

namespace champions::simulation::rta::arena_modes {
namespace {
std::int32_t arith_shr(std::int32_t value, int bits) {
  auto bits_u = static_cast<std::uint32_t>(value);
  if (value < 0)
    bits_u = (bits_u >> bits) | (0xffffffffu << (32 - bits));
  else
    bits_u >>= bits;
  return static_cast<std::int32_t>(bits_u);
}
int signed_byte(std::uint8_t byte) {
  return static_cast<std::int32_t>(static_cast<std::int8_t>(byte));
}
Raw fint(std::int32_t value) { return from_int(static_cast<std::uint32_t>(value)); }
void require_slots(int count) {
  if (count < 0 || count > SCORE_SLOTS)
    throw Unsupported("score block is 0x30 bytes");
}
const std::string &table_name(const NameTable &table, int index, int size,
                              const char *what) {
  if (static_cast<int>(table.size()) != size)
    throw Unsupported("level-name table was not supplied from the executable");
  if (index < 0 || index >= size)
    throw Unsupported(what);
  return table[static_cast<std::size_t>(index)];
}
} // namespace

[[noreturn]] void session_facing_word() {
  throw Unsupported("what a session stores in 0x4f7124 is unknown");
}
[[noreturn]] void net_message_peer() {
  throw Unsupported("what net message +0xf4 contains is unknown");
}
[[noreturn]] void monster_builder() {
  throw Unsupported("0x3065fc was executed only as the spawn call");
}
[[noreturn]] void next_wave_body() {
  throw Unsupported("arena_nextWave 0x305e9c body was not run");
}
[[noreturn]] void spell_arena_particles() {
  throw Unsupported("SpellArena particle positions were stubbed");
}
[[noreturn]] void empty_wave_callback() {
  throw Unsupported("empty-wave callback 0x3069e8 was not run");
}
[[noreturn]] void draw_helper_countdown() {
  throw Unsupported("draw helper 0x307bbc was not run");
}
[[noreturn]] void draw_task_lockstep() {
  throw Unsupported("whether every lockstep machine runs the draw task is unknown");
}
[[noreturn]] void player_respawn_body() {
  throw Unsupported("player_respawn was stubbed; spread 2 and radius 3 are the call");
}
[[noreturn]] void shutdown_body() {
  throw Unsupported("arena_shutdown was stubbed");
}
[[noreturn]] void saved_record_bytes() {
  throw Unsupported("the saved player-record layout is not in this research");
}

Draw lcg(std::uint32_t seed) {
  seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * 0x19660du +
                                    0x3c6ef35fu);
  return {seed, static_cast<std::uint16_t>(seed >> 16)};
}
std::pair<std::int32_t, std::int32_t> sdiv(std::int32_t n, std::int32_t d) {
  if (d == 0)
    throw DivideByZero("arena divides by zero");
  const auto q = static_cast<std::int64_t>(n) / d;
  return {static_cast<std::int32_t>(q),
          static_cast<std::int32_t>(static_cast<std::int64_t>(n) - q * d)};
}
int high_nibble(std::uint8_t byte) {
  return arith_shr(static_cast<std::int32_t>(static_cast<std::uint32_t>(byte) << 24), 28);
}
int low_nibble(std::uint8_t byte) {
  return arith_shr(static_cast<std::int32_t>(static_cast<std::uint32_t>(byte) << 28), 26) / 4;
}
const std::string &mode_a_name(const NameTable &table, int index) {
  return table_name(table, index, MODE_A_MAPS, "mode A name index is outside the 13-name table");
}
const std::string &mode_b_name(const NameTable &table, int index) {
  return table_name(table, index, MODE_B_MAPS, "mode B name index is outside the 7-name table");
}

std::uint32_t reset_scores(Slot *slots, int count, bool networked) {
  require_slots(count);
  for (int i = 0; i < count; ++i) {
    slots[i].kills = 0;
    slots[i].lives = LIVES_START;
    slots[i].snapshot = 0;
    slots[i].timer = TWO;
  }
  return networked ? K_MOVE_NET : K_MOVE_OFF;
}
void lose_life(Slot &slot, bool mode_a, bool prediction) {
  if (prediction)
    return;
  if (mode_a)
    slot.kills = static_cast<std::uint16_t>(slot.kills - 1);
  else
    slot.lives = static_cast<std::uint16_t>(slot.lives - 1);
}
void add_kill(Slot *slots, int count, const Killer *killer, std::uint32_t amount,
              bool mode_a, int num_players) {
  require_slots(count);
  if (mode_a && amount == 0)
    return;
  const auto credit = [&](int slot) {
    if (slot < 0 || slot >= count)
      throw Unsupported("kill credit is outside the score block");
    slots[slot].kills = static_cast<std::uint16_t>(slots[slot].kills + 1);
  };
  if (killer && (killer->flags & 0x40)) {
    credit(killer->slot);
    return;
  }
  if (killer && (killer->flags & 0x20) && signed_byte(static_cast<std::uint8_t>(killer->owner)) >= 0) {
    credit(signed_byte(static_cast<std::uint8_t>(killer->owner)));
    return;
  }
  if (num_players == 1)
    credit(0);
}
KillResult on_killed(Slot *slots, int count, int slot, const Killer *killer,
                     bool mode_a, int num_players, bool prediction) {
  require_slots(count);
  if (slot < 0 || slot >= count)
    throw Unsupported("victim slot is outside the score block");
  if (prediction)
    return KillResult::Ignore;
  if (killer && (killer->flags & 0x40) && killer->slot == slot) {
    lose_life(slots[slot], mode_a);
    return KillResult::Suicide;
  }
  if (!killer || (killer->flags & 0x20)) {
    if (mode_a) {
      add_kill(slots, count, killer, 1, true, num_players);
      return KillResult::Kill;
    }
    lose_life(slots[slot], false);
    return KillResult::Life;
  }
  lose_life(slots[slot], mode_a);
  return KillResult::Suicide;
}
void add_life(Slot &slot, bool mode_b, bool mode_a) {
  if (mode_b && !mode_a)
    slot.lives = static_cast<std::uint16_t>(slot.lives + 1);
}
std::uint32_t grant_life(Slot &slot) {
  slot.lives = static_cast<std::uint16_t>(slot.lives + 1);
  slot.timer = ZERO;
  return 0x80;
}
bool respawn_allowed(const Slot &slot, bool mode_a, bool active, int intermission) {
  if (!mode_a && s16(slot.lives) < 0)
    return false;
  if (!active || intermission != 0)
    return false;
  return true;
}

void tick_overlay(Slot *slots, int count) {
  require_slots(count);
  for (int i = 0; i < count; ++i) {
    if (eq(slots[i].timer, TWO))
      continue;
    const auto next = add(slots[i].timer, K_CLIMB);
    slots[i].timer = next;
    if (lt(TWO, next)) {
      slots[i].timer = TWO;
      slots[i].snapshot = slots[i].lives;
    }
  }
}
Raw overlay_t(Raw timer, int step) {
  return fmax(ZERO, sub(timer, mul(from_int(static_cast<std::uint32_t>(step)), K_STEP)));
}
bool overlay_draws(Raw timer, int step) { return le(overlay_t(timer, step), ONE); }
Raw overlay_anchor_x(int screen_x, Raw current) {
  if (lt(K_320, from_int(static_cast<std::uint32_t>(screen_x))) && lt(ZERO, current))
    return neg(current);
  return current;
}
Vec3 hermite(Raw t, const Vec3 &p0, const Vec3 &p1, const Vec3 &m0, const Vec3 &m1) {
  return vec3_hermite(t, p0, p1, m0, m1);
}
Raw fade_step(Raw value) {
  const auto next = add(value, K_FADE_STEP);
  if (lt(ONE, next))
    return ONE;
  return next;
}

RingOff ring_delta(int slot, int count, bool mode_a) {
  const auto quot = sdiv(0xffff, count).first;
  const auto product = static_cast<std::int64_t>(s16(static_cast<std::uint32_t>(quot))) *
                       static_cast<std::int64_t>(s32(static_cast<std::uint32_t>(slot)));
  const auto angle = s16(static_cast<std::uint32_t>(product));
  const auto radius = mode_a ? K_180 : K_36;
  return {angle, mul(cos16(static_cast<std::uint32_t>(angle)), radius),
          mul(sin16(static_cast<std::uint32_t>(angle)), radius)};
}
namespace {
int facing_from_entry(const Vec3 &pos, const SpawnPoint &entry, bool mode_a, bool scramble,
                      int slot, const SpawnPoint *points, int point_count) {
  int facing;
  if (scramble) {
    if (slot < 0 || slot >= point_count)
      throw Unsupported("scramble facing reads a missing spawn slot");
    facing = s16(static_cast<std::uint32_t>(points[slot].angle) - 0x4000u);
  } else {
    facing = s16(static_cast<std::uint32_t>(-0x4000 - atan2_16(sub(pos[0], entry.x), sub(pos[1], entry.y))));
  }
  if (!mode_a)
    facing = s16(static_cast<std::uint32_t>(facing) - 0x8000u);
  return facing;
}
bool near_focus(const SpawnPoint &point, const Vec3 &focus) {
  const auto dx = sub(point.x, focus[0]);
  const auto dy = sub(point.y, focus[1]);
  return lt(sqrt(add(mul(dx, dx), mul(dy, dy))), K_240);
}
std::pair<std::uint32_t, int> mod_index(std::uint32_t seed, int count) {
  const auto drawn = lcg(seed);
  return {drawn.seed, sdiv(drawn.value, count).second};
}
} // namespace

Pick pick_spawn(int flag, int slot, const SpawnPoint *points, int point_count,
                std::uint32_t seed, bool mode_a, bool scramble, bool network,
                const Vec3 &focus, int active, const Search &search, const Vec3 &camera) {
  if (flag) {
    if (point_count < 1)
      throw Unsupported("wave placement reads spawn entry 0");
    const auto ring = ring_delta(slot, active, mode_a);
    const Vec3 centre{add(points[0].x, ring.dx), add(points[0].y, ring.dy), points[0].z};
    if (!search)
      throw Unsupported("world_findFreeSpot was not supplied");
    auto hit = search(centre, 0x24, 0x18, seed);
    if (!hit.ok)
      hit = search(centre, 0xf0, 0x18, seed);
    const auto facing = facing_from_entry(hit.pos, points[0], mode_a, scramble, slot, points, point_count);
    return {hit.pos, facing, seed};
  }
  if (network) {
    const auto indexed = mod_index(seed, point_count);
    const auto &chosen = points[indexed.second];
    return {{chosen.x, chosen.y, chosen.z}, s16(static_cast<std::uint32_t>(chosen.angle) + 0x4000u),
            indexed.first};
  }
  std::vector<int> near;
  for (int i = 0; i < point_count; ++i)
    if (near_focus(points[i], focus))
      near.push_back(i);
  if (near.empty()) {
    if (!search)
      throw Unsupported("world_findFreeSpot was not supplied");
    const auto hit = search(camera, 0x48, 0x18, seed);
    return {hit.pos, 0, seed};
  }
  const auto indexed = mod_index(seed, static_cast<int>(near.size()));
  const auto &chosen = points[near[static_cast<std::size_t>(indexed.second)]];
  return {{chosen.x, chosen.y, chosen.z}, s16(static_cast<std::uint32_t>(chosen.angle) + 0x4000u),
          indexed.first};
}

Raw monster_level(int wave) {
  return add(mul(fint(s16(static_cast<std::uint32_t>(wave))), K_LEVEL), K_FIVE);
}
Budget spawn_budget(int wave, int active) {
  const auto parts = sdiv(s16(static_cast<std::uint32_t>(wave)), 3);
  const auto acc = add(mul(fint(parts.first), K_QUOT), mul(fint(parts.second), K_REM));
  const Raw value = le(ZERO, add(acc, TWO)) ? add(acc, K_TWO_HALF) : sub(add(acc, TWO), HALF);
  auto count = to_int(value);
  if (count >= 11)
    count = 10;
  return {count + s32(static_cast<std::uint32_t>(active)) - 1, parts.first, parts.second};
}
std::vector<int> recipe_groups(const Recipe &record, int rem, int count) {
  if (rem == 0)
    return std::vector<int>(static_cast<std::size_t>(count < 0 ? 0 : count), signed_byte(record[1]));
  if (rem < 0 || rem >= 3)
    return {};
  const int groups = high_nibble(record[0]) - (rem == 1 ? 1 : 0);
  const auto per = sdiv(count, groups).first;
  const int leftover = count - per * groups;
  std::vector<int> out;
  for (int group = 0; group < groups; ++group) {
    const int n = group == 0 ? per + leftover : per;
    const auto cls = signed_byte(record[static_cast<std::size_t>(1 + group)]);
    out.insert(out.end(), static_cast<std::size_t>(n < 0 ? 0 : n), cls);
  }
  return out;
}
std::pair<std::uint32_t, RingStep> ring_step(std::uint32_t seed, int count) {
  const auto quot = sdiv(0xffff, count).first;
  const auto drawn = lcg(seed);
  return {drawn.seed, RingStep{count, static_cast<std::uint32_t>(quot), drawn.value}};
}
WavePlan wave_plan(int wave, int active, const Recipe *records, int record_count,
                   std::uint32_t seed, int saved) {
  WavePlan plan;
  plan.seed = seed;
  if (saved) {
    const auto stepped = ring_step(seed, saved);
    plan.seed = stepped.first;
    plan.steps.push_back(stepped.second);
    return plan;
  }
  const auto budget = spawn_budget(wave, active);
  if (!records || record_count <= 0)
    throw Unsupported("recipe records are caller arguments; the executable bytes are not published");
  const auto index = sdiv(budget.quot, record_count).second;
  if (index < 0 || index >= record_count)
    throw Unsupported("recipe index is outside the records; that path was not executed");
  const auto &record = records[index];
  plan.classes = recipe_groups(record, budget.rem, budget.count);
  plan.has_classes = true;
  if (budget.rem == 0) {
    const auto stepped = ring_step(plan.seed, budget.count);
    plan.seed = stepped.first;
    plan.steps.push_back(stepped.second);
  } else if (budget.rem > 0 && budget.rem < 3) {
    const int groups = high_nibble(record[0]) - (budget.rem == 1 ? 1 : 0);
    const auto per = sdiv(budget.count, groups).first;
    const int leftover = budget.count - per * groups;
    for (int group = 0; group < groups; ++group) {
      const int n = group == 0 ? per + leftover : per;
      const auto stepped = ring_step(plan.seed, n);
      plan.seed = stepped.first;
      plan.steps.push_back(stepped.second);
    }
  }
  return plan;
}
int level_record_index(int wave, int record_count) {
  const auto quot = sdiv(s16(static_cast<std::uint32_t>(wave)) + 1, 3).first;
  return sdiv(quot, record_count).second;
}
int mode_a_next_index(int index, int scan0) {
  // Callers with no scan value use Match, which refuses before this point.
  const auto next = sdiv(s32(static_cast<std::uint32_t>(index)) + 1, MODE_A_MAPS).second;
  if (next == 0)
    return s32(static_cast<std::uint32_t>(scan0)) & 0xff;
  return next;
}
int kill_choice(int menu_index) {
  if (menu_index == 0)
    return 5;
  if (menu_index == 1)
    return 10;
  return 25;
}
int compare_kills(int slot_a, int slot_b, int kills_a, int kills_b) {
  const auto ka = s16(static_cast<std::uint32_t>(kills_a));
  const auto kb = s16(static_cast<std::uint32_t>(kills_b));
  if (kb < ka)
    return -1;
  if (ka < kb)
    return 1;
  if (slot_b < slot_a)
    return 1;
  return 0;
}
bool kill_limit_reached(int kills, int limit) {
  const auto byte = static_cast<std::uint32_t>(limit) & 0xffu;
  const auto signed_limit = arith_shr(static_cast<std::int32_t>(byte << 24), 24);
  return s16(static_cast<std::uint32_t>(kills)) >= signed_limit;
}
int intermission_limit(bool networked) {
  return networked ? INTERMISSION_ONLINE : INTERMISSION_OFFLINE;
}
bool intermission_done(int counter, bool networked) {
  return s16(static_cast<std::uint32_t>(counter)) == intermission_limit(networked);
}
Round round_expired(int waves_left, bool mode_a) {
  if (s16(static_cast<std::uint32_t>(waves_left)) == 0 || mode_a)
    return Round::Level;
  return Round::Wave;
}
bool still_in(bool active, int lives, Raw hp, int countdown) {
  if (!active)
    return false;
  if (s16(static_cast<std::uint32_t>(lives)) >= 0 || lt(ZERO, hp))
    return true;
  return s16(static_cast<std::uint32_t>(countdown)) > 0;
}
bool can_play_intermission(bool active, int lives, Raw hp) {
  if (!active)
    return false;
  return lt(ZERO, hp) || s16(static_cast<std::uint32_t>(lives)) >= 0;
}
Purchase buy_life(std::int32_t gold, std::int32_t budget) {
  if (gold < budget)
    return {gold, budget, false};
  return {gold - budget, static_cast<std::int32_t>(static_cast<std::uint32_t>(budget) << 1), true};
}
Raw armour_bound(Raw bound, int armor) {
  const auto cut = mul(fint(s16(static_cast<std::uint32_t>(armor))), K_1_30);
  auto low = sub(bound, cut);
  const auto floor = mul(bound, HALF);
  if (lt(low, floor))
    low = floor;
  if (lt(low, ZERO))
    low = ZERO;
  return low;
}
Raw pvp_half(Raw amount) { return mul(amount, HALF); }
bool spell_arena_flag(int counter) { return s16(static_cast<std::uint32_t>(counter)) >= 0x3d; }
bool spell_arena_tint(int counter) { return s16(static_cast<std::uint32_t>(counter)) >= 0x1e; }
SpellArena spell_arena(int counter, std::uint32_t flags) {
  SpellArena out;
  out.tint = spell_arena_tint(counter);
  out.particles = out.tint;
  if (spell_arena_flag(counter))
    out.flags = flags | 2u;
  else
    out.flags = flags & ~2u;
  return out;
}

Overlay draw_hud(bool mode_b, bool mode_a, bool frontend, bool menu, const Slot *slots,
                 int count, const int *screen_x, const int *screen_y, const int *anchor_x,
                 const int *anchor_y) {
  require_slots(count);
  Overlay overlay;
  if (!(mode_b && !mode_a && !frontend && !menu))
    return overlay;
  overlay.drawn = true;
  for (int slot = 0; slot < count; ++slot) {
    if (!lt(slots[slot].timer, TWO))
      continue;
    overlay.anchor_x = overlay_anchor_x(screen_x[slot], overlay.anchor_x);
    for (int k = 0; k < 5; ++k) {
      if (!overlay_draws(slots[slot].timer, k))
        continue;
      Glyph glyph;
      glyph.k = k;
      glyph.t = overlay_t(slots[slot].timer, k);
      glyph.p0 = {from_int(static_cast<std::uint32_t>(anchor_x[slot])),
                  from_int(static_cast<std::uint32_t>(anchor_y[slot])), ZERO};
      glyph.p1 = {from_int(static_cast<std::uint32_t>(screen_x[slot])),
                  from_int(static_cast<std::uint32_t>(screen_y[slot])), ZERO};
      glyph.m0 = {overlay.anchor_x, K_290, ZERO};
      glyph.m1 = {K_M500, K_M500, ZERO};
      glyph.out = hermite(glyph.t, glyph.p0, glyph.p1, glyph.m0, glyph.m1);
      glyph.colour = 128 - 16 * k;
      glyph.figure = 2000 - 200 * k;
      glyph.font = k == 0 ? 0 : 1;
      overlay.glyphs.push_back(glyph);
    }
  }
  return overlay;
}

Visit visit_creature(std::uint32_t flags, std::uint32_t seed) {
  if (flags & 0x40)
    return {seed, false};
  if (flags & 0x20) {
    const auto drawn = lcg(seed);
    return {drawn.seed, (drawn.value & 1) != 0};
  }
  throw Unsupported("arena_visitCreature on an object with neither flag was not executed");
}

void Match::init() {
  // arena_init 0x304c4c returns at once while mode B is clear.
  if (!mode_b)
    return;
  // arena_resetScores is called at 0x304dd0 (docs/issues/claims-audit.md row
  // 99), then wave -1 and the purchase budget 100 (0x304e70).
  movement = reset_scores(slots.data(), num_players, networked);
  wave = 0xffff;
  budget = 100;
}
WaveOut Match::begin() {
  if (!mode_b)
    return {};
  waves_left = 3;
  return start_wave(1);
}
WaveOut Match::start_wave(int delta) {
  const auto advance = [](std::uint16_t half, int amount, bool add_amount) {
    const auto bits = static_cast<std::uint32_t>(static_cast<std::int32_t>(amount));
    const auto sum = add_amount ? static_cast<std::uint32_t>(half) + bits
                                : static_cast<std::uint32_t>(half) - bits;
    return static_cast<std::uint16_t>(sum);
  };
  wave = advance(wave, delta, true);
  waves_left = advance(waves_left, delta, false);
  monster_level_value = monster_level(s16(wave));
  for (int i = 0; i < num_players; ++i)
    if (s16(slots[static_cast<std::size_t>(i)].lives) >= 0)
      slots[static_cast<std::size_t>(i)].snapshot = slots[static_cast<std::size_t>(i)].lives;
  WaveOut out;
  out.close_menus = true;
  out.wave = s16(wave);
  out.monster_level = monster_level_value;
  if (mode_a) {
    movement = reset_scores(slots.data(), num_players, networked);
    out.reset = true;
  } else if (saved_monsters) {
    const auto stepped = ring_step(seed, saved_monsters);
    seed = stepped.first;
    out.steps.push_back(stepped.second);
    out.saved_respawn = true;
    out.spawn_calls = saved_monsters;
    out.spawn_argument = 0;
  } else {
    const auto plan = wave_plan(s16(wave), active_count, recipes.data(),
                                static_cast<int>(recipes.size()), seed, 0);
    seed = plan.seed;
    out.used_recipe = true;
    out.classes = plan.classes;
    out.steps = plan.steps;
    out.spawn_calls = static_cast<int>(plan.classes.size());
    out.spawn_argument = 0;
  }
  intro = INTRO_FRAMES;
  match_word = 1;
  out.waves_left = s16(waves_left);
  out.seed = seed;
  out.intro = intro;
  out.match_word = match_word;
  out.camera_reset = true;
  return out;
}
void Match::open_end_prompt() {
  require_slots(num_players);
  fade = ZERO;
  rank_order.clear();
  rank_known = false;
  // With distinct kill counts every sort gives the same order. Equal counts
  // depend on the qsort the research did not assert.
  for (int i = 0; i < num_players; ++i)
    for (int j = i + 1; j < num_players; ++j)
      if (s16(slots[static_cast<std::size_t>(i)].kills) == s16(slots[static_cast<std::size_t>(j)].kills))
        return;
  rank_order.resize(static_cast<std::size_t>(num_players));
  for (int i = 0; i < num_players; ++i)
    rank_order[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
  rank_known = true;
  for (int i = 1; i < num_players; ++i) {
    const auto key = rank_order[static_cast<std::size_t>(i)];
    int j = i;
    while (j > 0 &&
           compare_kills(rank_order[static_cast<std::size_t>(j - 1)], key,
                         slots[rank_order[static_cast<std::size_t>(j - 1)]].kills,
                         slots[key].kills) > 0) {
      rank_order[static_cast<std::size_t>(j)] = rank_order[static_cast<std::size_t>(j - 1)];
      --j;
    }
    rank_order[static_cast<std::size_t>(j)] = key;
  }
}
std::string Match::level_name() const {
  if (mode_a) {
    if (!scan0)
      throw Unsupported("game_scanCm1Vars(0) was not supplied");
    return mode_a_name(names_a, *scan0);
  }
  if (recipes.empty())
    throw Unsupported("mode B level name needs recipe records; those bytes are not published");
  const auto record = sdiv(0, static_cast<std::int32_t>(recipes.size())).second;
  return mode_b_name(names_b, low_nibble(recipes[static_cast<std::size_t>(record)][0]));
}
void Match::register_spawn(int index, const SpawnPoint &point) {
  if (index == -1) {
    if (spawn_count == 255)
      throw Unsupported("spawn count byte would wrap");
    if (points.size() <= spawn_count)
      points.resize(static_cast<std::size_t>(spawn_count) + 1);
    points[spawn_count] = point;
    ++spawn_count;
    return;
  }
  if (index < 0 || index > 255)
    throw Unsupported("spawn index is outside the count byte");
  if (points.size() <= static_cast<std::size_t>(index))
    points.resize(static_cast<std::size_t>(index) + 1);
  points[static_cast<std::size_t>(index)] = point;
}
RespawnOut Match::respawn(int slot, const std::uint32_t *creature_flags, int creatures) {
  RespawnOut out;
  if (slot < 0 || slot >= num_players)
    throw Unsupported("respawn slot is outside the player count");
  const auto &column = slots[static_cast<std::size_t>(slot)];
  if (!respawn_allowed(column, mode_a, active[static_cast<std::size_t>(slot)], intermission))
    return out;
  dead[static_cast<std::size_t>(slot)] = DEAD_COUNTDOWN;
  slots[static_cast<std::size_t>(slot)].snapshot = column.lives;
  const auto picked = pick_spawn(0, slot, points.data(), spawn_count, seed, mode_a, scramble,
                                 networked, focus, active_count, search, camera);
  seed = picked.seed;
  hp[static_cast<std::size_t>(slot)] = max_hp[static_cast<std::size_t>(slot)];
  mana[static_cast<std::size_t>(slot)] = max_mana[static_cast<std::size_t>(slot)];
  out.ran = true;
  out.pos = picked.pos;
  out.facing = picked.facing;
  out.spread = 2;
  out.radius = 3;
  out.screen = slot == local_slot && (networked || num_players == 1);
  out.seed = seed;
  for (int i = 0; i < creatures; ++i) {
    const auto visit = visit_creature(creature_flags[i], seed);
    seed = visit.seed;
    ++out.visits;
    if (visit.called)
      ++out.virtual_calls;
  }
  out.seed = seed;
  return out;
}

TickOut Match::tick(bool confirm, bool cancel) {
  TickOut out;
  if (!mode_b) {
    out.skipped = true;
    return out;
  }
  if (paused) {
    out.skipped = true;
    return out;
  }
  if (s16(intro) > 0) {
    intro = static_cast<std::uint16_t>(s16(intro) - 1);
    lock_a = 1;
    lock_b = 1;
    match_word = 1;
    out.intro_lock = true;
    if (!mode_a && s16(intro) == INTRO_CUE)
      out.intro_cue = true;
    return out;
  }
  if (s16(intro) == 0) {
    intro = 0xffff;
    lock_a = 0;
    lock_b = 0;
    match_word = 0;
    out.intro_latch = true;
    return out;
  }
  tick_overlay(slots.data(), num_players);
  out.overlay_ticked = true;
  if (intermission != 0) {
    bool any_active = false;
    bool any_in = false;
    for (int i = 0; i < num_players; ++i) {
      if (!active[static_cast<std::size_t>(i)])
        continue;
      any_active = true;
      if (can_play_intermission(true, slots[static_cast<std::size_t>(i)].lives, hp[static_cast<std::size_t>(i)]))
        any_in = true;
    }
    if (!any_active)
      throw Unsupported("an intermission with no active slot was not executed");
    if (!any_in) {
      end_flag = 1;
      open_end_prompt();
      intermission = 0;
      out.end_prompt = true;
      out.rank_refused = !rank_known;
    } else {
      const auto old = s16(intermission);
      intermission = static_cast<std::uint16_t>(old + 1);
      if (old == intermission_limit(networked)) {
        if (round_expired(s16(waves_left), mode_a) == Round::Level) {
          if (mode_a) {
            const auto step = sdiv(s32(static_cast<std::uint32_t>(map_index)) + 1, MODE_A_MAPS).second;
            if (step == 0 && !scan0)
              throw Unsupported("the mode-A rotation wrapped and game_scanCm1Vars(0) was not supplied");
            map_index = mode_a_next_index(map_index, scan0 ? *scan0 : 0);
            pending_name = mode_a_name(names_a, map_index);
          } else {
            if (recipes.empty())
              throw Unsupported("mode B level change needs recipe records; those bytes are not published");
            const auto record = level_record_index(s16(wave), static_cast<int>(recipes.size()));
            if (record < 0 || record >= static_cast<int>(recipes.size()))
              throw Unsupported("recipe index is outside the records; that path was not executed");
            pending_name = mode_b_name(names_b, low_nibble(recipes[static_cast<std::size_t>(record)][0]));
          }
          pending_copied = true;
          pending_orientation = -1;
          out.level_change = true;
          out.close_menus = true;
        } else {
          out.next_wave = true;
          out.next_wave_arg = 1;
        }
      }
    }
  } else if (mode_a) {
    for (int i = 0; i < num_players; ++i) {
      if (kill_limit_reached(slots[static_cast<std::size_t>(i)].kills, kill_limit)) {
        // Section 4: the countdown becomes 1 and the prompt opens. No end-flag
        // store is documented on this path.
        intermission = 1;
        open_end_prompt();
        out.end_prompt = true;
        out.rank_refused = !rank_known;
        break;
      }
    }
  } else if (!end_flag) {
    if ((frame & 0x7f) == 0)
      out.empty_wave_refused = true;
    bool any_active = false;
    bool any_in = false;
    bool any_snap = false;
    for (int i = 0; i < num_players; ++i) {
      if (!active[static_cast<std::size_t>(i)])
        continue;
      any_active = true;
      const auto &column = slots[static_cast<std::size_t>(i)];
      if (still_in(true, column.lives, hp[static_cast<std::size_t>(i)], dead[static_cast<std::size_t>(i)]))
        any_in = true;
      if (s16(column.snapshot) > 0)
        any_snap = true;
    }
    if (!any_active)
      throw Unsupported("a live-match scan with no active slot was not executed");
    if (!any_in) {
      if (any_snap) {
        out.next_wave = true;
        out.next_wave_arg = 0;
      } else {
        end_flag = 1;
        open_end_prompt();
        out.end_prompt = true;
        out.rank_refused = !rank_known;
      }
    }
  }
  if (end_flag && eq(fade, ONE)) {
    if (confirm && cancel)
      throw Unsupported("confirm and cancel on the same frame were not executed");
    if (confirm) {
      for (int i = 0; i < num_players; ++i)
        if (active[static_cast<std::size_t>(i)])
          ++out.restores;
      out.shutdown = true;
      pending_name = level_name();
      pending_copied = true;
    } else if (cancel) {
      mode_b = false;
      exit_code = 1;
      out.session_exit = true;
    }
  }
  // The tail is gated only on mode A (branch 0x307170).
  if (!mode_a) {
    for (int n = 0; n < buyer_count; ++n) {
      const auto slot = buyers[static_cast<std::size_t>(n)];
      if (slot < 0 || slot >= num_players)
        throw Unsupported("life buyer is outside the player count");
      const auto bought = buy_life(gold[static_cast<std::size_t>(slot)], budget);
      if (!bought.bought)
        continue;
      gold[static_cast<std::size_t>(slot)] = bought.gold;
      budget = bought.budget;
      hud[static_cast<std::size_t>(slot)] = grant_life(slots[static_cast<std::size_t>(slot)]);
      out.granted = true;
      out.granted_slot = slot;
    }
  }
  return out;
}

} // namespace champions::simulation::rta::arena_modes
