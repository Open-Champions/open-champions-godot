#include "champions/arena_executable.hpp"
#include "champions/arena_modes.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace champions::simulation::rta;
using namespace champions::simulation::rta::arena_modes;

namespace {
int g_fails = 0;

void fail(const char *file, int line, const char *cond) {
  std::fprintf(stderr, "FAIL %s:%d %s\n", file, line, cond);
  ++g_fails;
}

#define CHECK(cond)                                                                            \
  do {                                                                                         \
    if (!(cond))                                                                               \
      fail(__FILE__, __LINE__, #cond);                                                        \
  } while (0)

#define CHECK_THROWS(expr)                                                                     \
  do {                                                                                         \
    bool threw = false;                                                                       \
    try {                                                                                      \
      expr;                                                                                    \
    } catch (const Unsupported &) {                                                            \
      threw = true;                                                                            \
    }                                                                                          \
    if (!threw)                                                                                \
      fail(__FILE__, __LINE__, #expr " did not refuse");                                      \
  } while (0)

#define CHECK_THROWS_ZERO(expr)                                                                \
  do {                                                                                         \
    bool threw = false;                                                                       \
    try {                                                                                      \
      expr;                                                                                    \
    } catch (const DivideByZero &) {                                                           \
      threw = true;                                                                            \
    }                                                                                          \
    if (!threw)                                                                                \
      fail(__FILE__, __LINE__, #expr " did not divide by zero");                              \
  } while (0)

void vectors() {
#include "arena_modes_vectors.inc"
}

Recipe recipe_nibble(std::uint8_t low) {
  return Recipe{low, 3, 1, 0, 0, 0};
}

// Synthetic stand-ins for the two executable tables.
NameTable names(const char *prefix, int count) {
  NameTable out;
  for (int i = 0; i < count; ++i)
    out.push_back(prefix + std::to_string(i));
  return out;
}
void name_tables(Match &match) {
  match.names_a = names("a", MODE_A_MAPS);
  match.names_b = names("b", MODE_B_MAPS);
}

void refusals() {
  CHECK_THROWS(session_facing_word());
  CHECK_THROWS(net_message_peer());
  CHECK_THROWS(monster_builder());
  CHECK_THROWS(next_wave_body());
  CHECK_THROWS(spell_arena_particles());
  CHECK_THROWS(empty_wave_callback());
  CHECK_THROWS(draw_helper_countdown());
  CHECK_THROWS(draw_task_lockstep());
  CHECK_THROWS(player_respawn_body());
  CHECK_THROWS(shutdown_body());
  CHECK_THROWS(saved_record_bytes());
  const auto table_a = names("a", MODE_A_MAPS);
  const auto table_b = names("b", MODE_B_MAPS);
  CHECK_THROWS(mode_a_name(table_a, 13));
  CHECK_THROWS(mode_a_name(table_a, -1));
  CHECK_THROWS(mode_b_name(table_b, 7));
  CHECK_THROWS(mode_b_name(table_b, low_nibble(0x08)));
  CHECK_THROWS(mode_a_name(NameTable{}, 0));
  CHECK_THROWS(mode_b_name(names("b", 6), 0));
  CHECK(low_nibble(0x08) < 0);
  CHECK_THROWS_ZERO(sdiv(1, 0));
  CHECK_THROWS_ZERO(ring_step(1, 0));
  const Recipe zero_group{{0x10, 3, 1, 0, 0, 0}};
  CHECK_THROWS_ZERO(wave_plan(1, 1, &zero_group, 1, 1, 0));
  Match bare;
  bare.mode_b = true;
  bare.mode_a = false;
  CHECK_THROWS(bare.start_wave(1));
  CHECK_THROWS(bare.level_name());
  Match unnamed;
  unnamed.recipes = {recipe_nibble(0x01)};
  CHECK_THROWS(unnamed.level_name());
  Match noscan;
  noscan.mode_a = true;
  name_tables(noscan);
  CHECK_THROWS(noscan.level_name());
  Match nobody;
  nobody.mode_b = true;
  nobody.active.fill(false);
  CHECK_THROWS(nobody.tick());
  nobody.intermission = 5;
  CHECK_THROWS(nobody.tick());
  Match wrap;
  wrap.mode_a = true;
  wrap.mode_b = true;
  name_tables(wrap);
  wrap.intermission = static_cast<std::uint16_t>(INTERMISSION_OFFLINE);
  wrap.map_index = 12;
  wrap.hp[0] = from_int(1);
  wrap.slots[0].lives = 2;
  CHECK_THROWS(wrap.tick());
}

void scoring_and_spawn_table() {
  Match match;
  match.mode_b = true;
  match.mode_a = true;
  match.num_players = 2;
  match.slots[0].lives = 5;
  match.slots[0].snapshot = 9;
  match.slots[1].lives = 0xffff;
  match.slots[1].snapshot = 4;
  match.wave = 0xffff;
  match.waves_left = 3;
  const auto started = match.start_wave(1);
  CHECK(started.wave == 0);
  CHECK(started.waves_left == 2);
  CHECK(started.reset);
  CHECK(!started.used_recipe);
  CHECK(started.spawn_calls == 0);
  CHECK(started.intro == INTRO_FRAMES);
  CHECK(started.match_word == 1);
  CHECK(started.camera_reset);
  CHECK(started.close_menus);
  CHECK(match.slots[0].lives == 2);
  CHECK(match.slots[0].snapshot == 0);
  CHECK(match.slots[1].lives == 2);
  CHECK(match.movement == K_MOVE_OFF);

  Match waves;
  waves.mode_b = true;
  waves.recipes.push_back(recipe_nibble(0x30));
  waves.num_players = 2;
  waves.slots[0] = Slot{0, 5, 1, TWO};
  waves.slots[1] = Slot{0, 0xffff, 9, TWO};
  waves.wave = 0xffff;
  waves.waves_left = 3;
  waves.seed = 20973;
  const auto planned = waves.start_wave(1);
  CHECK(planned.used_recipe);
  CHECK(planned.spawn_argument == 0);
  CHECK(waves.slots[0].snapshot == 5);
  CHECK(waves.slots[1].snapshot == 9);
  CHECK(planned.classes.size() == static_cast<std::size_t>(planned.spawn_calls));

  Match saved;
  saved.mode_b = true;
  saved.saved_monsters = 3;
  saved.seed = 20973;
  const auto restored = saved.start_wave(1);
  CHECK(restored.saved_respawn);
  CHECK(!restored.used_recipe);
  CHECK(restored.classes.empty());
  CHECK(restored.spawn_calls == 3);
  CHECK(restored.spawn_argument == 0);
  CHECK(restored.steps.size() == 1);

  Match table;
  SpawnPoint first{0x11, 0x22, 0x33, 0x1234};
  table.register_spawn(-1, first);
  CHECK(table.spawn_count == 1);
  CHECK(table.points[0].x == 0x11 && table.points[0].angle == 0x1234);
  SpawnPoint second{0x44, 0, 0, 0xfffe};
  table.register_spawn(1, second);
  CHECK(table.spawn_count == 1);
  CHECK(table.points[1].x == 0x44);
  CHECK(s16(table.points[1].angle) == -2);
  CHECK_THROWS(table.register_spawn(-2, first));
  CHECK_THROWS(table.register_spawn(256, first));
  table.spawn_count = 255;
  CHECK_THROWS(table.register_spawn(-1, first));
}

void task_frames() {
  Match paused;
  paused.mode_b = true;
  paused.paused = true;
  paused.intro = 50;
  const auto held = paused.tick();
  CHECK(held.skipped);
  CHECK(paused.intro == 50);

  Match off;
  off.intro = 50;
  CHECK(off.tick().skipped);
  CHECK(off.intro == 50);

  Match intro;
  intro.mode_b = true;
  intro.intro = 180;
  const auto stepped = intro.tick();
  CHECK(stepped.intro_lock);
  CHECK(!stepped.intro_cue);
  CHECK(intro.intro == 179);
  CHECK(intro.lock_a == 1 && intro.lock_b == 1 && intro.match_word == 1);
  CHECK(!stepped.overlay_ticked);

  Match cue;
  cue.mode_b = true;
  cue.intro = 0x85;
  CHECK(cue.tick().intro_cue);
  CHECK(cue.intro == 0x84);
  Match quiet;
  quiet.mode_b = true;
  quiet.mode_a = true;
  quiet.intro = 0x85;
  CHECK(!quiet.tick().intro_cue);
  CHECK(quiet.intro == 0x84);

  Match latch;
  latch.mode_b = true;
  latch.mode_a = true;
  latch.intro = 0;
  latch.kill_limit = 10;
  latch.slots[0].kills = 10;
  const auto latched = latch.tick();
  CHECK(latched.intro_latch);
  CHECK(!latched.end_prompt);
  CHECK(s16(latch.intro) == -1);
  CHECK(latch.lock_a == 0 && latch.lock_b == 0 && latch.match_word == 0);
  CHECK(latch.intermission == 0);
  const auto opened = latch.tick();
  CHECK(opened.end_prompt);
  CHECK(latch.intermission == 1);
  CHECK(latch.end_flag == 0);

  Match early;
  early.mode_b = true;
  early.intermission = 10;
  early.slots[0].lives = 2;
  early.hp[0] = from_int(10);
  const auto waiting = early.tick();
  CHECK(!waiting.level_change);
  CHECK(!waiting.next_wave);
  CHECK(early.intermission == 11);

  Match expired;
  expired.mode_b = true;
  expired.recipes = {recipe_nibble(0x01), recipe_nibble(0x04)};
  name_tables(expired);
  expired.intermission = static_cast<std::uint16_t>(INTERMISSION_OFFLINE);
  expired.waves_left = 0;
  expired.wave = 2;
  const auto changed = expired.tick();
  CHECK(changed.level_change);
  CHECK(changed.close_menus);
  CHECK(expired.pending_copied);
  CHECK(expired.pending_orientation == -1);
  CHECK(expired.pending_name == "b4");
  CHECK(expired.intermission == INTERMISSION_OFFLINE + 1);

  Match online;
  online.mode_b = true;
  online.networked = true;
  online.recipes = {recipe_nibble(0x01)};
  name_tables(online);
  online.intermission = static_cast<std::uint16_t>(INTERMISSION_ONLINE);
  online.waves_left = 0;
  online.wave = 2;
  CHECK(online.tick().level_change);

  Match shorter;
  shorter.mode_b = true;
  shorter.networked = true;
  shorter.recipes = {recipe_nibble(0x01)};
  shorter.intermission = static_cast<std::uint16_t>(INTERMISSION_OFFLINE);
  shorter.waves_left = 0;
  shorter.wave = 2;
  const auto ignored = shorter.tick();
  CHECK(!ignored.level_change);
  CHECK(shorter.intermission == INTERMISSION_OFFLINE + 1);

  Match another;
  another.mode_b = true;
  another.intermission = static_cast<std::uint16_t>(INTERMISSION_OFFLINE);
  another.waves_left = 2;
  another.wave = 0;
  const auto before = another.wave;
  const auto next = another.tick();
  CHECK(next.next_wave);
  CHECK(next.next_wave_arg == 1);
  CHECK(!next.level_change);
  CHECK(another.wave == before);

  Match wrapped;
  wrapped.mode_a = true;
  wrapped.mode_b = true;
  wrapped.intermission = static_cast<std::uint16_t>(INTERMISSION_OFFLINE);
  wrapped.map_index = 12;
  wrapped.scan0 = 2;
  name_tables(wrapped);
  wrapped.waves_left = 9;
  const auto spun = wrapped.tick();
  CHECK(spun.level_change);
  CHECK(wrapped.map_index == 2);
  CHECK(wrapped.pending_name == "a2");

  Match plain;
  plain.mode_a = true;
  plain.mode_b = true;
  plain.intermission = static_cast<std::uint16_t>(INTERMISSION_OFFLINE);
  plain.map_index = 0;
  name_tables(plain);
  plain.waves_left = 9;
  CHECK(plain.tick().level_change);
  CHECK(plain.map_index == 1);
  CHECK(plain.pending_name == "a1");

  Match won;
  won.mode_a = true;
  won.mode_b = true;
  won.kill_limit = 10;
  won.slots[0].kills = 10;
  const auto prompt = won.tick();
  CHECK(prompt.end_prompt);
  CHECK(won.intermission == 1);
  CHECK(won.end_flag == 0);
  CHECK(won.fade == ZERO);
  Match below;
  below.mode_a = true;
  below.mode_b = true;
  below.kill_limit = 10;
  below.slots[0].kills = 9;
  CHECK(!below.tick().end_prompt);
  CHECK(below.intermission == 0);

  Match wipe;
  wipe.mode_b = true;
  wipe.intermission = 10;
  wipe.slots[0].lives = 0xffff;
  wipe.hp[0] = ZERO;
  wipe.slots[0].snapshot = 0;
  const auto wiped = wipe.tick();
  CHECK(wiped.end_prompt);
  CHECK(wipe.intermission == 0);
  CHECK(wipe.end_flag == 1);

  Match retry;
  retry.mode_b = true;
  retry.slots[0].lives = 0xffff;
  retry.hp[0] = ZERO;
  retry.dead[0] = 0;
  retry.slots[0].snapshot = 2;
  retry.slots[0].timer = TWO;
  const auto kept = retry.wave;
  const auto again = retry.tick();
  CHECK(again.next_wave);
  CHECK(again.next_wave_arg == 0);
  CHECK(!again.end_prompt);
  CHECK(retry.wave == kept);
  CHECK(retry.slots[0].snapshot == 2);

  Match defeat;
  defeat.mode_b = true;
  defeat.slots[0].lives = 0xffff;
  defeat.hp[0] = ZERO;
  defeat.slots[0].snapshot = 0;
  defeat.slots[0].timer = TWO;
  const auto lost = defeat.tick();
  CHECK(lost.end_prompt);
  CHECK(!lost.next_wave);
  CHECK(defeat.end_flag == 1);

  Match bought;
  bought.mode_b = true;
  bought.slots[0].lives = 0;
  bought.slots[0].timer = TWO;
  bought.hp[0] = from_int(5);
  bought.gold[0] = 150;
  bought.budget = 100;
  bought.buyer_count = 1;
  bought.buyers[0] = 0;
  const auto sale = bought.tick();
  CHECK(sale.granted);
  CHECK(sale.granted_slot == 0);
  CHECK(bought.gold[0] == 50);
  CHECK(bought.budget == 200);
  CHECK(bought.slots[0].lives == 1);
  CHECK(bought.slots[0].timer == ZERO);
  CHECK(bought.hud[0] == 0x80);

  Match both;
  both.mode_b = true;
  both.num_players = 2;
  both.slots[0].lives = 0;
  both.slots[1].lives = 0;
  both.slots[0].timer = TWO;
  both.slots[1].timer = TWO;
  both.hp[0] = from_int(5);
  both.hp[1] = from_int(5);
  both.gold[0] = 100;
  both.gold[1] = 200;
  both.budget = 100;
  both.buyer_count = 2;
  both.buyers[0] = 0;
  both.buyers[1] = 1;
  CHECK(both.tick().granted);
  CHECK(both.gold[0] == 0);
  CHECK(both.gold[1] == 0);
  CHECK(both.budget == 400);
  CHECK(both.slots[0].lives == 1 && both.slots[1].lives == 1);

  Match confirm;
  confirm.mode_b = true;
  confirm.recipes = {recipe_nibble(0x01), recipe_nibble(0x04)};
  name_tables(confirm);
  confirm.end_flag = 1;
  confirm.fade = ONE;
  confirm.slots[0].lives = 0xffff;
  confirm.hp[0] = ZERO;
  Match both_buttons = confirm;
  CHECK_THROWS(both_buttons.tick(true, true));
  const auto yes = confirm.tick(true, false);
  CHECK(yes.shutdown);
  CHECK(!yes.session_exit);
  CHECK(yes.restores == 1);
  CHECK(confirm.mode_b);
  CHECK(confirm.exit_code == 0);
  CHECK(confirm.pending_name == "b1");
  CHECK_THROWS(saved_record_bytes());

  Match cancel;
  cancel.mode_b = true;
  cancel.end_flag = 1;
  cancel.fade = ONE;
  const auto no = cancel.tick(false, true);
  CHECK(no.session_exit);
  CHECK(!no.shutdown);
  CHECK(!cancel.mode_b);
  CHECK(cancel.exit_code == 1);
  CHECK(!cancel.pending_copied);

  Match unready;
  unready.mode_b = true;
  unready.end_flag = 1;
  unready.fade = ZERO;
  CHECK(!unready.tick(true, false).shutdown);

  Match ranked;
  ranked.num_players = 3;
  ranked.slots[0].kills = 1;
  ranked.slots[1].kills = 5;
  ranked.slots[2].kills = 1;
  ranked.fade = ONE;
  ranked.open_end_prompt();
  CHECK(!ranked.rank_known);
  CHECK(ranked.rank_order.empty());
  CHECK(ranked.fade == ZERO);
  CHECK(ranked.end_flag == 0);
  ranked.slots[2].kills = 3;
  ranked.fade = ONE;
  ranked.open_end_prompt();
  CHECK(ranked.rank_known);
  CHECK((ranked.rank_order == std::vector<std::uint8_t>{1, 2, 0}));
  CHECK(ranked.fade == ZERO);

  Match gate;
  gate.mode_b = true;
  gate.frame = 128;
  gate.slots[0].lives = 2;
  gate.hp[0] = from_int(1);
  CHECK(gate.tick().empty_wave_refused);
  CHECK(!gate.end_flag);
  Match gated_end;
  gated_end.mode_b = true;
  gated_end.frame = 128;
  gated_end.slots[0].lives = 0xffff;
  gated_end.hp[0] = ZERO;
  gated_end.slots[0].snapshot = 0;
  gated_end.slots[0].timer = TWO;
  const auto refused = gated_end.tick();
  CHECK(refused.empty_wave_refused);
  CHECK(refused.end_prompt);
  Match versus;
  versus.mode_a = true;
  versus.mode_b = true;
  versus.frame = 128;
  versus.slots[0].lives = 2;
  CHECK(!versus.tick().empty_wave_refused);
}

void respawn_and_names() {
  Match match;
  match.mode_b = true;
  match.num_players = 1;
  match.local_slot = 0;
  match.slots[0].lives = 2;
  match.max_hp[0] = from_int(80);
  match.hp[0] = from_int(1);
  match.max_mana[0] = from_int(40);
  match.mana[0] = from_int(1);
  match.register_spawn(-1, SpawnPoint{ZERO, ZERO, from_int(5), 0x1000});
  const auto first = match.respawn(0, nullptr, 0);
  CHECK(first.ran);
  CHECK(first.spread == 2 && first.radius == 3);
  CHECK(first.screen);
  CHECK(match.dead[0] == DEAD_COUNTDOWN);
  CHECK(match.slots[0].snapshot == 2);
  CHECK(match.hp[0] == from_int(80));
  CHECK(match.mana[0] == from_int(40));
  CHECK(first.facing == s16(0x1000u + 0x4000u));

  match.num_players = 2;
  match.local_slot = 0;
  match.networked = false;
  const auto remote = match.respawn(0, nullptr, 0);
  CHECK(remote.ran);
  CHECK(!remote.screen);

  match.networked = true;
  match.local_slot = 0;
  CHECK(match.respawn(0, nullptr, 0).screen);
  match.local_slot = 1;
  CHECK(!match.respawn(0, nullptr, 0).screen);

  match.networked = false;
  match.num_players = 1;
  match.slots[0].lives = 0xffff;
  match.mode_a = false;
  CHECK(!match.respawn(0, nullptr, 0).ran);
  match.mode_a = true;
  match.intermission = 4;
  CHECK(!match.respawn(0, nullptr, 0).ran);
  match.intermission = 0;
  CHECK(match.respawn(0, nullptr, 0).ran);

  const std::uint32_t flags[] = {0x40, 0x60};
  Match visit;
  visit.mode_b = true;
  visit.slots[0].lives = 2;
  visit.register_spawn(-1, SpawnPoint{ZERO, ZERO, ZERO, 0});
  const auto scattered = visit.respawn(0, flags, 2);
  CHECK(scattered.visits == 2);
  CHECK(scattered.virtual_calls == 0);
  const std::uint32_t prop[] = {0};
  CHECK_THROWS(visit.respawn(0, prop, 1));

  Match named;
  named.mode_a = true;
  name_tables(named);
  named.scan0 = 3;
  CHECK(named.level_name() == "a3");
  named.scan0 = 13;
  CHECK_THROWS(named.level_name());
  Match coop;
  name_tables(coop);
  coop.recipes = {recipe_nibble(0x01), recipe_nibble(0x04)};
  CHECK(coop.level_name() == "b1");

  Match begun;
  begun.mode_a = true;
  begun.mode_b = true;
  begun.num_players = 1;
  begun.slots[0] = Slot{7, 9, 3, ZERO};
  begun.init();
  CHECK(begun.wave == 0xffff);
  CHECK(begun.budget == 100);
  CHECK(begun.slots[0].lives == 2 && begun.slots[0].kills == 0 && begun.slots[0].timer == TWO);
  CHECK(begun.movement == K_MOVE_OFF);
  begun.begin();
  CHECK(s16(begun.wave) == 0);
  CHECK(s16(begun.waves_left) == 2);
  CHECK(begun.intro == INTRO_FRAMES);
  Match idle;
  idle.init();
  CHECK(idle.wave == 0);
  idle.begin();
  CHECK(idle.wave == 0);

  SpawnPoint ring[] = {SpawnPoint{ZERO, ZERO, from_int(5), 0x1000}};
  const auto scrambled = pick_spawn(1, 0, ring, 1, 1, true, true, false, Vec3{ZERO, ZERO, ZERO}, 1,
                                    [](const Vec3 &, int, int, std::uint32_t &) { return Spot{Vec3{from_int(9), from_int(8), from_int(7)}, 1}; },
                                    Vec3{});
  CHECK(scrambled.facing == s16(0x1000u - 0x4000u));
  CHECK_THROWS_ZERO(pick_spawn(1, 0, ring, 1, 1, false, false, false, Vec3{}, 0, {}, Vec3{}));

  // A search that draws leaves the shared seed where its draws end, on the
  // ring path, its retry, and the camera fallback.
  const Search drawing = [](const Vec3 &centre, int spread, int, std::uint32_t &seed) {
    seed = lcg(seed).seed;
    return Spot{centre, spread == 0xf0 ? 1 : 0};
  };
  const auto retried = pick_spawn(1, 0, ring, 1, 7, false, false, false, Vec3{}, 1, drawing, Vec3{});
  CHECK(retried.seed == lcg(lcg(7).seed).seed);
  SpawnPoint far[] = {SpawnPoint{from_int(256), ZERO, ZERO, 0}};
  const auto fallback = pick_spawn(0, 0, far, 1, 7, false, false, false, Vec3{}, 1, drawing, Vec3{});
  CHECK(fallback.seed == lcg(7).seed && fallback.facing == 0);
  Match threaded;
  threaded.mode_b = true;
  threaded.slots[0].lives = 2;
  threaded.seed = 7;
  threaded.search = drawing;
  threaded.register_spawn(-1, far[0]);
  const auto moved = threaded.respawn(0, nullptr, 0);
  CHECK(moved.ran && threaded.seed == lcg(7).seed);
}
} // namespace

// A synthetic ELF32 with one PT_LOAD segment over the two documented table
// addresses. The names are authored, not the executable's.
std::vector<std::uint8_t> synthetic_exe(int count_a, int count_b) {
  const std::uint32_t base = 0x503000, size = 0x1000, at = 0x100;
  std::vector<std::uint8_t> b(at + size, 0);
  auto put = [&](std::size_t o, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      b[o + std::size_t(i)] = std::uint8_t(v >> (8 * i));
  };
  b[0] = 0x7f, b[1] = 'E', b[2] = 'L', b[3] = 'F', b[4] = 1, b[5] = 1;
  put(0x1c, 0x34);                     // phoff
  b[0x2a] = 0x20, b[0x2c] = 1;         // phentsize, phnum
  put(0x34, 1);                        // PT_LOAD
  put(0x38, at);                       // offset
  put(0x3c, base);                     // vaddr
  put(0x44, size);                     // filesz
  std::uint32_t text = 0x503c00;
  auto table = [&](std::uint32_t va, int count, char tag) {
    for (int i = 0; i < count; ++i) {
      put(at + (va - base) + 4u * unsigned(i), text);
      const std::string name = std::string(1, tag) + "Map" + std::to_string(i);
      for (std::size_t k = 0; k < name.size(); ++k)
        b[at + (text - base) + k] = std::uint8_t(name[k]);
      text += 16;
    }
  };
  table(NAMES_A_TABLE, count_a, 'a');
  table(NAMES_B_TABLE, count_b, 'b');
  return b;
}

void executable_tables() {
  Executable exe;
  CHECK(exe.load(synthetic_exe(MODE_A_MAPS, MODE_B_MAPS)));
  const auto names = read_names(exe);
  CHECK(names.names_a.size() == 13u && names.names_b.size() == 7u);
  CHECK(names.names_a[0] == "aMap0" && names.names_b[6] == "bMap6");
  CHECK(mode_a_index(names.names_a, "AMAP12") == 12);
  CHECK(mode_a_index(names.names_a, "bMap0") == -1 && mode_a_index(names.names_a, "") == -1);
  CHECK_THROWS(exe.word(0x400000));
  Executable short_a;
  CHECK(short_a.load(synthetic_exe(12, MODE_B_MAPS)));
  CHECK_THROWS(read_names(short_a));
  Executable not_elf;
  CHECK(!not_elf.load(std::vector<std::uint8_t>(0x40, 0)));
}

int main() {
  vectors();
  refusals();
  scoring_and_spawn_table();
  task_frames();
  respawn_and_names();
  executable_tables();
  if (g_fails) {
    std::fprintf(stderr, "arena modes: %d failed\n", g_fails);
    return 1;
  }
  std::puts("arena modes: ok");
  return 0;
}
