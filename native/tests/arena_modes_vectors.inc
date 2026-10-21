CHECK(lcg(1).seed == 0x3c88596cu);
CHECK(lcg(1).value == 15496);
CHECK(lcg(20973).seed == 0x5d3c8a68u);
CHECK(lcg(20973).value == 23868);
CHECK(kill_choice(0) == 5);
CHECK(kill_choice(1) == 10);
CHECK(kill_choice(2) == 25);
CHECK(kill_choice(9) == 25);
CHECK(high_nibble(0x42) == 4);
CHECK(low_nibble(0x35) == 5);
CHECK(high_nibble(0x80) == -8);
CHECK(low_nibble(0x08) == -8);
CHECK(low_nibble(0x8f) == -1);
{
Slot slots[2] = {Slot{4, 9, 3, ONE}, Slot{}};
CHECK(reset_scores(slots, 2, true) == K_MOVE_NET);
CHECK(reset_scores(slots, 2, false) == K_MOVE_OFF);
CHECK(slots[0].lives == 2 && slots[0].kills == 0 && slots[0].timer == TWO);
CHECK(slots[1].lives == 2 && slots[1].timer == TWO);
Killer pet{0x20, 0, 1};
add_kill(slots, 2, &pet, 5, true, 2);
CHECK(slots[1].kills == 1);
Killer negative{0x20, 0, 0xff};
add_kill(slots, 2, &negative, 1, true, 2);
CHECK(slots[0].kills == 0);
add_kill(slots, 2, nullptr, 1, true, 1);
CHECK(slots[0].kills == 1);
Killer player{0x60, 0, 0};
add_kill(slots, 2, &player, 0, true, 2);
CHECK(slots[0].kills == 1);
slots[0].lives = 2;
CHECK(on_killed(slots, 2, 0, &player, false, 2) == KillResult::Suicide);
CHECK(s16(slots[0].lives) == 1);
Killer other{0x60, 1, 0};
CHECK(on_killed(slots, 2, 0, &other, false, 2) == KillResult::Life);
CHECK(s16(slots[0].lives) == 0);
CHECK(on_killed(slots, 2, 0, nullptr, true, 1) == KillResult::Kill);
CHECK(slots[0].kills == 2);
CHECK(on_killed(slots, 2, 0, &other, true, 1, true) == KillResult::Ignore);
Slot dead{};
lose_life(dead, true);
CHECK(dead.kills == 0xffff);
Slot lives{0, 1, 0, ZERO};
lose_life(lives, false);
lose_life(lives, false);
CHECK(s16(lives.lives) == -1);
add_life(lives, true, false);
CHECK(lives.lives == 0);
add_life(lives, true, true);
CHECK(lives.lives == 0);
add_life(lives, false, false);
CHECK(lives.lives == 0);
CHECK(grant_life(lives) == 0x80u);
CHECK(lives.lives == 1 && lives.timer == ZERO);
}
CHECK(respawn_allowed(Slot{0, 0, 0, ZERO}, false, true, 0));
CHECK(!respawn_allowed(Slot{0, 0xffff, 0, ZERO}, false, true, 0));
CHECK(respawn_allowed(Slot{0, 0xffff, 0, ZERO}, true, true, 0));
CHECK(!respawn_allowed(Slot{0, 2, 0, ZERO}, false, true, 1));
CHECK(!respawn_allowed(Slot{0, 2, 0, ZERO}, false, false, 0));
{
Slot slot{0, 2, 0, ZERO};
tick_overlay(&slot, 1);
CHECK(slot.timer == 0x3c2af07fu);
CHECK(slot.snapshot == 0);
slot.timer = TWO;
tick_overlay(&slot, 1);
CHECK(slot.timer == TWO);
}
{
Slot slot{0, 4, 9, 0x3fff1260u};
tick_overlay(&slot, 1);
CHECK(slot.timer == TWO);
CHECK(slot.snapshot == 4);
}
CHECK(overlay_draws(ZERO, 0));
CHECK(!overlay_draws(TWO, 0));
CHECK(!overlay_draws(TWO, 4));
CHECK(overlay_anchor_x(100, K_259) == K_259);
CHECK(overlay_anchor_x(400, K_259) == neg(K_259));
CHECK(overlay_anchor_x(100, neg(K_259)) == neg(K_259));
{
const auto got = hermite(ZERO, Vec3{0x3f800000u, 0x40000000u, 0x40400000u}, Vec3{0x40800000u, 0x40a00000u, 0x40c00000u}, Vec3{0x40e00000u, 0x41000000u, 0x41100000u}, Vec3{0x41200000u, 0x41300000u, 0x41400000u});
CHECK(got[0] == 0x3f800000u && got[1] == 0x40000000u && got[2] == 0x40400000u);
}
{
const auto got = hermite(ONE, Vec3{0x3f800000u, 0x40000000u, 0x40400000u}, Vec3{0x40800000u, 0x40a00000u, 0x40c00000u}, Vec3{0x40e00000u, 0x41000000u, 0x41100000u}, Vec3{0x41200000u, 0x41300000u, 0x41400000u});
CHECK(got[0] == 0x40800000u && got[1] == 0x40a00000u && got[2] == 0x40c00000u);
}
{
const auto got = hermite(0x00000000u, Vec3{0x3f800000u, 0x40000000u, 0x40400000u}, Vec3{0x41100000u, 0x41000000u, 0x40e00000u}, Vec3{0x40800000u, 0x00000000u, 0xc0000000u}, Vec3{0x3f800000u, 0x40000000u, 0x00000000u});
CHECK(got[0] == 0x3f800000u && got[1] == 0x40000000u && got[2] == 0x40400000u);
}
{
const auto got = hermite(0x3f800000u, Vec3{0x43818000u, 0x43910000u, 0x00000000u}, Vec3{0x42c80000u, 0x41200000u, 0x00000000u}, Vec3{0x43818000u, 0x43910000u, 0x00000000u}, Vec3{0xc3fa0000u, 0xc3fa0000u, 0x00000000u});
CHECK(got[0] == 0x42c80000u && got[1] == 0x41200000u && got[2] == 0x00000000u);
}
{
const auto got = hermite(0x3f70a3d7u, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, Vec3{0x3f800000u, 0x3f800000u, 0x3f800000u}, Vec3{0x40000000u, 0x40000000u, 0x40000000u}, Vec3{0x3f000000u, 0x3f000000u, 0x3f000000u});
CHECK(got[0] == 0x3f784ad7u && got[1] == 0x3f784ad7u && got[2] == 0x3f784ad7u);
}
CHECK(fade_step(ZERO) == 0x3c61029bu);
CHECK(fade_step(ONE) == ONE);
CHECK(spawn_budget(0, 1).count == 2);
CHECK(spawn_budget(0, 1).quot == 0 && spawn_budget(0, 1).rem == 0);
CHECK(monster_level(0) == K_FIVE);
CHECK(monster_level(0) == 0x40a00000u);
CHECK(spawn_budget(0, 1).count == 2);
CHECK(spawn_budget(0, 1).quot == 0 && spawn_budget(0, 1).rem == 0);
CHECK(spawn_budget(0, 4).count == 5);
CHECK(spawn_budget(0, 4).quot == 0 && spawn_budget(0, 4).rem == 0);
CHECK(monster_level(1) == 0x40b578afu);
CHECK(spawn_budget(1, 1).count == 3);
CHECK(spawn_budget(1, 1).quot == 0 && spawn_budget(1, 1).rem == 1);
CHECK(spawn_budget(1, 4).count == 6);
CHECK(spawn_budget(1, 4).quot == 0 && spawn_budget(1, 4).rem == 1);
CHECK(monster_level(2) == 0x40caf15eu);
CHECK(spawn_budget(2, 1).count == 4);
CHECK(spawn_budget(2, 1).quot == 0 && spawn_budget(2, 1).rem == 2);
CHECK(spawn_budget(2, 4).count == 7);
CHECK(spawn_budget(2, 4).quot == 0 && spawn_budget(2, 4).rem == 2);
CHECK(monster_level(3) == 0x40e06a0du);
CHECK(spawn_budget(3, 1).count == 3);
CHECK(spawn_budget(3, 1).quot == 1 && spawn_budget(3, 1).rem == 0);
CHECK(spawn_budget(3, 4).count == 6);
CHECK(spawn_budget(3, 4).quot == 1 && spawn_budget(3, 4).rem == 0);
CHECK(monster_level(5) == 0x4105adb5u);
CHECK(spawn_budget(5, 1).count == 5);
CHECK(spawn_budget(5, 1).quot == 1 && spawn_budget(5, 1).rem == 2);
CHECK(spawn_budget(5, 4).count == 8);
CHECK(spawn_budget(5, 4).quot == 1 && spawn_budget(5, 4).rem == 2);
CHECK(monster_level(8) == 0x4125e2bcu);
CHECK(spawn_budget(8, 1).count == 6);
CHECK(spawn_budget(8, 1).quot == 2 && spawn_budget(8, 1).rem == 2);
CHECK(spawn_budget(8, 4).count == 9);
CHECK(spawn_budget(8, 4).quot == 2 && spawn_budget(8, 4).rem == 2);
CHECK(spawn_budget(300, 4).count == 13);
{
const Recipe record{{0x30, 3, 1, 0, 0, 0}};
CHECK((recipe_groups(record, 0, 4) == std::vector<int>{3, 3, 3, 3}));
CHECK((recipe_groups(record, 1, 5) == std::vector<int>{3, 3, 3, 1, 1}));
CHECK((recipe_groups(record, 2, 5) == std::vector<int>{3, 3, 3, 1, 0}));
CHECK((recipe_groups(record, 4, 5) == std::vector<int>{}));
}
CHECK(level_record_index(2, 5) == 1);
CHECK(mode_a_next_index(12, 4) == 4);
CHECK(mode_a_next_index(0, 4) == 1);
CHECK(round_expired(0, false) == Round::Level);
CHECK(round_expired(2, false) == Round::Wave);
CHECK(round_expired(2, true) == Round::Level);
CHECK(intermission_done(INTERMISSION_OFFLINE, false));
CHECK(!intermission_done(INTERMISSION_OFFLINE, true));
CHECK(intermission_done(INTERMISSION_ONLINE, true));
{
NameTable a, b;
for (int i = 0; i < 13; ++i) a.push_back("a" + std::to_string(i));
for (int i = 0; i < 7; ++i) b.push_back("b" + std::to_string(i));
CHECK(mode_a_name(a, 12) == "a12");
CHECK(mode_a_name(a, 2) == "a2");
CHECK(mode_b_name(b, 0) == "b0");
CHECK(mode_b_name(b, 6) == "b6");
}
{
const Recipe record{{0x30, 3, 1, 0, 0, 0}};
const auto plan = wave_plan(0, 1, &record, 1, 20973, 0);
CHECK(plan.has_classes);
CHECK((plan.classes == std::vector<int>{3, 3}));
CHECK(plan.seed == 0x5d3c8a68u);
CHECK(plan.steps.size() == 1);
CHECK(plan.steps[0].count == 2);
CHECK(plan.steps[0].angle == 0x00007fffu);
CHECK(plan.steps[0].draw == 23868);
}
{
const Recipe record{{0x30, 3, 1, 0, 0, 0}};
const auto plan = wave_plan(1, 1, &record, 1, 20973, 0);
CHECK(plan.has_classes);
CHECK((plan.classes == std::vector<int>{3, 3, 1}));
CHECK(plan.seed == 0x9bcf6aa7u);
CHECK(plan.steps.size() == 2);
CHECK(plan.steps[0].count == 2);
CHECK(plan.steps[0].angle == 0x00007fffu);
CHECK(plan.steps[0].draw == 23868);
CHECK(plan.steps[1].count == 1);
CHECK(plan.steps[1].angle == 0x0000ffffu);
CHECK(plan.steps[1].draw == 39887);
}
{
const Recipe record{{0x30, 3, 1, 0, 0, 0}};
const auto plan = wave_plan(2, 4, &record, 1, 20973, 0);
CHECK(plan.has_classes);
CHECK((plan.classes == std::vector<int>{3, 3, 3, 1, 1, 0, 0}));
CHECK(plan.seed == 0x34c4e7dau);
CHECK(plan.steps.size() == 3);
CHECK(plan.steps[0].count == 3);
CHECK(plan.steps[0].angle == 0x00005555u);
CHECK(plan.steps[0].draw == 23868);
CHECK(plan.steps[1].count == 2);
CHECK(plan.steps[1].angle == 0x00007fffu);
CHECK(plan.steps[1].draw == 39887);
CHECK(plan.steps[2].count == 2);
CHECK(plan.steps[2].angle == 0x00007fffu);
CHECK(plan.steps[2].draw == 13508);
}
{
const Recipe record{{0x30, 3, 1, 0, 0, 0}};
const auto plan = wave_plan(5, 1, &record, 1, 20973, 0);
CHECK(plan.has_classes);
CHECK((plan.classes == std::vector<int>{3, 3, 3, 1, 0}));
CHECK(plan.seed == 0x34c4e7dau);
CHECK(plan.steps.size() == 3);
CHECK(plan.steps[0].count == 3);
CHECK(plan.steps[0].angle == 0x00005555u);
CHECK(plan.steps[0].draw == 23868);
CHECK(plan.steps[1].count == 1);
CHECK(plan.steps[1].angle == 0x0000ffffu);
CHECK(plan.steps[1].draw == 39887);
CHECK(plan.steps[2].count == 1);
CHECK(plan.steps[2].angle == 0x0000ffffu);
CHECK(plan.steps[2].draw == 13508);
}
{
const auto plan = wave_plan(0, 1, nullptr, 0, 20973, 3);
CHECK(!plan.has_classes);
CHECK(plan.seed == 0x5d3c8a68u);
CHECK(plan.steps.size() == 1 && plan.steps[0].count == 3);
CHECK(plan.steps[0].angle == 0x00005555u && plan.steps[0].draw == 23868);
}
CHECK_THROWS_ZERO(([&]{ const Recipe record{{0x10, 3, 1, 0, 0, 0}}; recipe_groups(record, 1, 5); }()));
{
const SpawnPoint table[] = {SpawnPoint{0x00000000u, 0x00000000u, 0x40a00000u, 4096}, SpawnPoint{0x43000000u, 0x00000000u, 0x3f800000u, 8192}, SpawnPoint{0x43800000u, 0x00000000u, 0x41100000u, 256}};
std::vector<Spot> script{Spot{Vec3{0x41100000u, 0x41000000u, 0x40e00000u}, 1}};
std::size_t at = 0;
std::vector<std::pair<int, int>> spreads;
const auto got = pick_spawn(1, 1, table, 3, 1, true, false, false, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, 4,
  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
    spreads.push_back({spread, radius});
    return script[at++];
  }, Vec3{0x40400000u, 0x40800000u, 0x40a00000u});
CHECK(spreads.size() == 1);
CHECK(spreads[0].first == 36 && spreads[0].second == 24);
CHECK(got.pos[0] == 0x41100000u && got.pos[1] == 0x41000000u && got.pos[2] == 0x40e00000u);
CHECK(got.facing == -25188);
CHECK(got.seed == 0x00000001u);
}
{
const SpawnPoint table[] = {SpawnPoint{0x00000000u, 0x00000000u, 0x40a00000u, 4096}, SpawnPoint{0x43000000u, 0x00000000u, 0x3f800000u, 8192}, SpawnPoint{0x43800000u, 0x00000000u, 0x41100000u, 256}};
std::vector<Spot> script{Spot{Vec3{0x3f800000u, 0x3f800000u, 0x3f800000u}, 0}, Spot{Vec3{0x41100000u, 0x41000000u, 0x40e00000u}, 1}};
std::size_t at = 0;
std::vector<std::pair<int, int>> spreads;
const auto got = pick_spawn(1, 2, table, 3, 1, false, false, false, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, 4,
  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
    spreads.push_back({spread, radius});
    return script[at++];
  }, Vec3{0x40400000u, 0x40800000u, 0x40a00000u});
CHECK(spreads.size() == 2);
CHECK(spreads[0].first == 36 && spreads[0].second == 24);
CHECK(spreads[1].first == 240 && spreads[1].second == 24);
CHECK(got.pos[0] == 0x41100000u && got.pos[1] == 0x41000000u && got.pos[2] == 0x40e00000u);
CHECK(got.facing == 7580);
CHECK(got.seed == 0x00000001u);
}
{
const SpawnPoint table[] = {SpawnPoint{0x00000000u, 0x00000000u, 0x40a00000u, 4096}, SpawnPoint{0x43000000u, 0x00000000u, 0x3f800000u, 8192}, SpawnPoint{0x43800000u, 0x00000000u, 0x41100000u, 256}};
std::vector<Spot> script{Spot{Vec3{0x41100000u, 0x41000000u, 0x40e00000u}, 1}};
std::size_t at = 0;
std::vector<std::pair<int, int>> spreads;
const auto got = pick_spawn(1, 0, table, 3, 1, true, true, false, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, 3,
  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
    spreads.push_back({spread, radius});
    return script[at++];
  }, Vec3{0x40400000u, 0x40800000u, 0x40a00000u});
CHECK(spreads.size() == 1);
CHECK(spreads[0].first == 36 && spreads[0].second == 24);
CHECK(got.pos[0] == 0x41100000u && got.pos[1] == 0x41000000u && got.pos[2] == 0x40e00000u);
CHECK(got.facing == -12288);
CHECK(got.seed == 0x00000001u);
}
{
const SpawnPoint table[] = {SpawnPoint{0x00000000u, 0x00000000u, 0x40a00000u, 4096}, SpawnPoint{0x43000000u, 0x00000000u, 0x3f800000u, 8192}, SpawnPoint{0x43800000u, 0x00000000u, 0x41100000u, 256}};
std::vector<Spot> script{};
std::size_t at = 0;
std::vector<std::pair<int, int>> spreads;
const auto got = pick_spawn(0, 1, table, 3, 20973, false, false, true, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, 1,
  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
    spreads.push_back({spread, radius});
    return script[at++];
  }, Vec3{0x40400000u, 0x40800000u, 0x40a00000u});
CHECK(spreads.size() == 0);
CHECK(got.pos[0] == 0x00000000u && got.pos[1] == 0x00000000u && got.pos[2] == 0x40a00000u);
CHECK(got.facing == 20480);
CHECK(got.seed == 0x5d3c8a68u);
}
{
const SpawnPoint table[] = {SpawnPoint{0x00000000u, 0x00000000u, 0x40a00000u, 4096}, SpawnPoint{0x43000000u, 0x00000000u, 0x3f800000u, 8192}, SpawnPoint{0x43800000u, 0x00000000u, 0x41100000u, 256}};
std::vector<Spot> script{};
std::size_t at = 0;
std::vector<std::pair<int, int>> spreads;
const auto got = pick_spawn(0, 0, table, 3, 7, false, false, false, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, 1,
  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
    spreads.push_back({spread, radius});
    return script[at++];
  }, Vec3{0x40400000u, 0x40800000u, 0x40a00000u});
CHECK(spreads.size() == 0);
CHECK(got.pos[0] == 0x00000000u && got.pos[1] == 0x00000000u && got.pos[2] == 0x40a00000u);
CHECK(got.facing == 20480);
CHECK(got.seed == 0x3d20bdbau);
}
{
const SpawnPoint table[] = {SpawnPoint{0x43800000u, 0x00000000u, 0x41100000u, 256}};
std::vector<Spot> script{Spot{Vec3{0x40400000u, 0x40800000u, 0x40a00000u}, 1}};
std::size_t at = 0;
std::vector<std::pair<int, int>> spreads;
const auto got = pick_spawn(0, 0, table, 1, 3, false, false, false, Vec3{0x00000000u, 0x00000000u, 0x00000000u}, 1,
  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
    spreads.push_back({spread, radius});
    return script[at++];
  }, Vec3{0x40400000u, 0x40800000u, 0x40a00000u});
CHECK(spreads.size() == 1);
CHECK(spreads[0].first == 72 && spreads[0].second == 24);
CHECK(got.pos[0] == 0x40400000u && got.pos[1] == 0x40800000u && got.pos[2] == 0x40a00000u);
CHECK(got.facing == 0);
CHECK(got.seed == 0x00000003u);
}
CHECK(armour_bound(from_int(100), 300) == 0x42b40000u);
CHECK(armour_bound(from_int(100), 300) != from_int(100));
CHECK(pvp_half(from_int(100)) == from_int(50));
CHECK(pvp_half(from_int(40)) == 0x41a00000u);
CHECK(spell_arena_flag(61) && !spell_arena_flag(60));
CHECK(spell_arena_tint(30) && !spell_arena_tint(29) && !spell_arena_tint(-1));
{
const auto up = spell_arena(180, 0x11);
CHECK(up.flags == 0x13u && up.tint && up.particles);
const auto down = spell_arena(40, 0x13);
CHECK(down.flags == 0x11u && down.tint);
const auto early = spell_arena(10, 0x13);
CHECK(early.flags == 0x11u && !early.tint && !early.particles);
const auto steady = spell_arena(0xffff, 0x13);
CHECK(steady.flags == 0x11u && !steady.tint);
}
{
const auto bought = buy_life(100, 100);
CHECK(bought.gold == 0 && bought.budget == 200 && bought.bought);
CHECK(!buy_life(99, 100).bought);
}
CHECK(compare_kills(0, 1, 1, 5) == 1);
CHECK(compare_kills(1, 0, 5, 1) == -1);
CHECK(compare_kills(0, 2, 1, 1) == 0);
CHECK(compare_kills(2, 0, 1, 1) == 1);
CHECK(kill_limit_reached(10, 10));
CHECK(!kill_limit_reached(9, 10));
CHECK(kill_limit_reached(0, 0));
CHECK(!kill_limit_reached(0xffff, 0));
CHECK(still_in(true, 0, ZERO, 0));
CHECK(still_in(true, 0xffff, ZERO, 1));
CHECK(!still_in(true, 0xffff, ZERO, 0));
CHECK(!still_in(false, 2, from_int(10), 0));
CHECK(can_play_intermission(true, 0xffff, from_int(1)));
CHECK(!can_play_intermission(true, 0xffff, ZERO));
{
Slot columns[2] = {Slot{0, 2, 0, ZERO}, Slot{0, 2, 0, from_int(1)}};
const int sx[] = {100, 400}, sy[] = {10, 20}, ax[] = {11, 22}, ay[] = {33, 44};
const auto hud = draw_hud(true, false, false, false, columns, 2, sx, sy, ax, ay);
CHECK(hud.drawn);
CHECK(hud.glyphs.size() == 10);
CHECK(hud.glyphs[0].k == 0);
CHECK(hud.glyphs[0].t == 0x00000000u);
CHECK(hud.glyphs[0].colour == 128 && hud.glyphs[0].figure == 2000 && hud.glyphs[0].font == 0);
CHECK(hud.glyphs[0].out[0] == 0x41300000u && hud.glyphs[0].out[1] == 0x42040000u && hud.glyphs[0].out[2] == 0x00000000u);
CHECK(hud.glyphs[0].m0[0] == 0x43818000u);
CHECK(hud.glyphs[1].k == 1);
CHECK(hud.glyphs[1].t == 0x00000000u);
CHECK(hud.glyphs[1].colour == 112 && hud.glyphs[1].figure == 1800 && hud.glyphs[1].font == 1);
CHECK(hud.glyphs[1].out[0] == 0x41300000u && hud.glyphs[1].out[1] == 0x42040000u && hud.glyphs[1].out[2] == 0x00000000u);
CHECK(hud.glyphs[1].m0[0] == 0x43818000u);
CHECK(hud.glyphs[2].k == 2);
CHECK(hud.glyphs[2].t == 0x00000000u);
CHECK(hud.glyphs[2].colour == 96 && hud.glyphs[2].figure == 1600 && hud.glyphs[2].font == 1);
CHECK(hud.glyphs[2].out[0] == 0x41300000u && hud.glyphs[2].out[1] == 0x42040000u && hud.glyphs[2].out[2] == 0x00000000u);
CHECK(hud.glyphs[2].m0[0] == 0x43818000u);
CHECK(hud.glyphs[3].k == 3);
CHECK(hud.glyphs[3].t == 0x00000000u);
CHECK(hud.glyphs[3].colour == 80 && hud.glyphs[3].figure == 1400 && hud.glyphs[3].font == 1);
CHECK(hud.glyphs[3].out[0] == 0x41300000u && hud.glyphs[3].out[1] == 0x42040000u && hud.glyphs[3].out[2] == 0x00000000u);
CHECK(hud.glyphs[3].m0[0] == 0x43818000u);
CHECK(hud.glyphs[4].k == 4);
CHECK(hud.glyphs[4].t == 0x00000000u);
CHECK(hud.glyphs[4].colour == 64 && hud.glyphs[4].figure == 1200 && hud.glyphs[4].font == 1);
CHECK(hud.glyphs[4].out[0] == 0x41300000u && hud.glyphs[4].out[1] == 0x42040000u && hud.glyphs[4].out[2] == 0x00000000u);
CHECK(hud.glyphs[4].m0[0] == 0x43818000u);
CHECK(hud.glyphs[5].k == 0);
CHECK(hud.glyphs[5].t == 0x3f800000u);
CHECK(hud.glyphs[5].colour == 128 && hud.glyphs[5].figure == 2000 && hud.glyphs[5].font == 0);
CHECK(hud.glyphs[5].out[0] == 0x43c80000u && hud.glyphs[5].out[1] == 0x41a00000u && hud.glyphs[5].out[2] == 0x00000000u);
CHECK(hud.glyphs[5].m0[0] == 0xc3818000u);
CHECK(hud.glyphs[6].k == 1);
CHECK(hud.glyphs[6].t == 0x3f7851ebu);
CHECK(hud.glyphs[6].colour == 112 && hud.glyphs[6].figure == 1800 && hud.glyphs[6].font == 1);
CHECK(hud.glyphs[6].out[0] == 0x43ce718bu && hud.glyphs[6].out[1] == 0x4209b886u && hud.glyphs[6].out[2] == 0x00000000u);
CHECK(hud.glyphs[6].m0[0] == 0xc3818000u);
CHECK(hud.glyphs[7].k == 2);
CHECK(hud.glyphs[7].t == 0x3f70a3d7u);
CHECK(hud.glyphs[7].colour == 96 && hud.glyphs[7].figure == 1600 && hud.glyphs[7].font == 1);
CHECK(hud.glyphs[7].out[0] == 0x43d2db2eu && hud.glyphs[7].out[1] == 0x423ef3edu && hud.glyphs[7].out[2] == 0x00000000u);
CHECK(hud.glyphs[7].m0[0] == 0xc3818000u);
CHECK(hud.glyphs[8].k == 3);
CHECK(hud.glyphs[8].t == 0x3f68f5c2u);
CHECK(hud.glyphs[8].colour == 80 && hud.glyphs[8].figure == 1400 && hud.glyphs[8].font == 1);
CHECK(hud.glyphs[8].out[0] == 0x43d55c4bu && hud.glyphs[8].out[1] == 0x426fcd21u && hud.glyphs[8].out[2] == 0x00000000u);
CHECK(hud.glyphs[8].m0[0] == 0xc3818000u);
CHECK(hud.glyphs[9].k == 4);
CHECK(hud.glyphs[9].t == 0x3f6147aeu);
CHECK(hud.glyphs[9].colour == 64 && hud.glyphs[9].figure == 1200 && hud.glyphs[9].font == 1);
CHECK(hud.glyphs[9].out[0] == 0x43d61450u && hud.glyphs[9].out[1] == 0x428e2f7eu && hud.glyphs[9].out[2] == 0x00000000u);
CHECK(hud.glyphs[9].m0[0] == 0xc3818000u);
const auto hidden = draw_hud(true, true, false, false, columns, 2, sx, sy, ax, ay);
CHECK(!hidden.drawn && hidden.glyphs.empty());
const auto menu = draw_hud(true, false, false, true, columns, 2, sx, sy, ax, ay);
CHECK(!menu.drawn);
}
{
const auto player = visit_creature(0x40, 1);
CHECK(player.seed == 1 && !player.called);
const auto both = visit_creature(0x60, 1);
CHECK(both.seed == 1 && !both.called);
const auto creature = visit_creature(0x20, 1);
CHECK(creature.seed == 0x3c88596cu);
CHECK(creature.called == false);
}
{
const auto creature = visit_creature(0x20, 2);
CHECK(creature.seed == 0x3ca1bf79u && creature.called == true);
}
CHECK_THROWS(visit_creature(0, 1));
