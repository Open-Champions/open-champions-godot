#!/usr/bin/env python3
"""Exact arena-mode vectors from the pinned research reference.

The generated include is the native test. It does not copy recipe bytes from
the executable; those bytes are not in the research prose. Synthetic records
are the ones in tools/test_ref_arena_modes.py. The level-name tables are
synthetic too: the research prints the names, but this tree does not copy
level file names.
"""
import hashlib
import sys
from pathlib import Path

COMMIT = "17810bb0f9a3474fbeae9e86d339cb72ccd0f154"
PINS = {
    "docs/arena-modes.md": "1dafe8b50e9d604ac387a87fe9d3c94490b6026f3eda4501cfd72da607c07a58",
    "tools/ref_arena_modes.py": "839f500b3e77b4e830b43d4f131177b43a54542a52335c181edc30f455a06cbe",
    "tools/test_ref_arena_modes.py": "e15df52033d91b4c5acc7eabc91b2d2c5a3f16d5fc010bb75695d809bd7e555b",
    "tools/difftest_arena_modes.py": "1d9d226a1d674841ee3eb4fcd0fcc28d8d2e98476105fa5d16b60763bd572de8",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
    "docs/issues/claims-audit.md": "a734087616c3a2853e5496092b82cedd2052b7352bd042282e44300f80cad30f",
}


def hx(value):
    return "0x%08xu" % (value & 0xFFFFFFFF)


def point(x, y, z, angle):
    return (A.from_int(x), A.from_int(y), A.from_int(z), angle & 0xFFFF)


def emit_point(p):
    return "SpawnPoint{%s, %s, %s, %d}" % (hx(p[0]), hx(p[1]), hx(p[2]), p[3])


def emit_vec(v):
    return "Vec3{%s, %s, %s}" % (hx(v[0]), hx(v[1]), hx(v[2]))


def main():
    research = Path(sys.argv[1] if len(sys.argv) > 1 else "")
    destination = Path(sys.argv[2] if len(sys.argv) > 2 else "")
    if not research.is_dir() or not destination:
        raise SystemExit("usage: generate_arena_modes_vectors.py RESEARCH OUT.inc")
    for rel, digest in PINS.items():
        got = hashlib.sha256((research / rel).read_bytes()).hexdigest()
        if got != digest:
            raise SystemExit("%s hash %s != %s" % (rel, got, digest))
    sys.path.insert(0, str(research / "tools"))
    global A
    import ref_arena_modes as A
    from ref_core import ONE, TWO, ZERO, from_int, neg, s16

    lines = []
    check = lines.append

    def eq(expr, value, kind="u"):
        if kind == "u":
            check("CHECK((%s) == %s);" % (expr, hx(value) if isinstance(value, int) and value > 0xFFFF else value))
        else:
            check("CHECK((%s) == %s);" % (expr, value))

    seed, draw = A.lcg(1)
    check("CHECK(lcg(1).seed == %s);" % hx(seed))
    check("CHECK(lcg(1).value == %d);" % draw)
    seed, draw = A.lcg(20973)
    check("CHECK(lcg(20973).seed == %s);" % hx(seed))
    check("CHECK(lcg(20973).value == %d);" % draw)
    check("CHECK(kill_choice(0) == 5);")
    check("CHECK(kill_choice(1) == 10);")
    check("CHECK(kill_choice(2) == 25);")
    check("CHECK(kill_choice(9) == 25);")
    check("CHECK(high_nibble(0x42) == 4);")
    check("CHECK(low_nibble(0x35) == 5);")
    check("CHECK(high_nibble(0x80) == %d);" % A.high_nibble(0x80))
    check("CHECK(low_nibble(0x08) == %d);" % A.low_nibble(0x08))
    check("CHECK(low_nibble(0x8f) == %d);" % A.low_nibble(0x8F))

    # Scoring sequence from test_ref_arena_modes.py.
    check("{")
    check("Slot slots[2] = {Slot{4, 9, 3, ONE}, Slot{}};")
    check("CHECK(reset_scores(slots, 2, true) == K_MOVE_NET);")
    check("CHECK(reset_scores(slots, 2, false) == K_MOVE_OFF);")
    check("CHECK(slots[0].lives == 2 && slots[0].kills == 0 && slots[0].timer == TWO);")
    check("CHECK(slots[1].lives == 2 && slots[1].timer == TWO);")
    check("Killer pet{0x20, 0, 1};")
    check("add_kill(slots, 2, &pet, 5, true, 2);")
    check("CHECK(slots[1].kills == 1);")
    check("Killer negative{0x20, 0, 0xff};")
    check("add_kill(slots, 2, &negative, 1, true, 2);")
    check("CHECK(slots[0].kills == 0);")
    check("add_kill(slots, 2, nullptr, 1, true, 1);")
    check("CHECK(slots[0].kills == 1);")
    check("Killer player{0x60, 0, 0};")
    check("add_kill(slots, 2, &player, 0, true, 2);")
    check("CHECK(slots[0].kills == 1);")
    check("slots[0].lives = 2;")
    check('CHECK(on_killed(slots, 2, 0, &player, false, 2) == KillResult::Suicide);')
    check("CHECK(s16(slots[0].lives) == 1);")
    check("Killer other{0x60, 1, 0};")
    check('CHECK(on_killed(slots, 2, 0, &other, false, 2) == KillResult::Life);')
    check("CHECK(s16(slots[0].lives) == 0);")
    check('CHECK(on_killed(slots, 2, 0, nullptr, true, 1) == KillResult::Kill);')
    check("CHECK(slots[0].kills == 2);")
    check('CHECK(on_killed(slots, 2, 0, &other, true, 1, true) == KillResult::Ignore);')
    check("Slot dead{};")
    check("lose_life(dead, true);")
    check("CHECK(dead.kills == 0xffff);")
    check("Slot lives{0, 1, 0, ZERO};")
    check("lose_life(lives, false);")
    check("lose_life(lives, false);")
    check("CHECK(s16(lives.lives) == -1);")
    check("add_life(lives, true, false);")
    check("CHECK(lives.lives == 0);")
    check("add_life(lives, true, true);")
    check("CHECK(lives.lives == 0);")
    check("add_life(lives, false, false);")
    check("CHECK(lives.lives == 0);")
    check("CHECK(grant_life(lives) == 0x80u);")
    check("CHECK(lives.lives == 1 && lives.timer == ZERO);")
    check("}")

    check("CHECK(respawn_allowed(Slot{0, 0, 0, ZERO}, false, true, 0));")
    check("CHECK(!respawn_allowed(Slot{0, 0xffff, 0, ZERO}, false, true, 0));")
    check("CHECK(respawn_allowed(Slot{0, 0xffff, 0, ZERO}, true, true, 0));")
    check("CHECK(!respawn_allowed(Slot{0, 2, 0, ZERO}, false, true, 1));")
    check("CHECK(!respawn_allowed(Slot{0, 2, 0, ZERO}, false, false, 0));")

    climbed = A.Slot(lives=2, snapshot=0, timer=ZERO)
    A.tick_overlay([climbed])
    check("{")
    check("Slot slot{0, 2, 0, ZERO};")
    check("tick_overlay(&slot, 1);")
    check("CHECK(slot.timer == %s);" % hx(climbed.timer))
    check("CHECK(slot.snapshot == 0);")
    check("slot.timer = TWO;")
    check("tick_overlay(&slot, 1);")
    check("CHECK(slot.timer == TWO);")
    check("}")
    # Observed clamp: a timer one step under 2 copies lives. The difftest did
    # not take this branch; the reference specifies the stores.
    # Observed clamp: the last timer whose next step is above 2.0 copies lives.
    # The difftest did not take this branch; the reference specifies the stores.
    probe = A.Slot(lives=4, snapshot=9, timer=ZERO)
    for _ in range(100000):
        nxt = A.add(probe.timer, A.K_CLIMB)
        if A.lt(A.TWO, nxt):
            break
        A.tick_overlay([probe])
        if probe.timer == A.TWO:
            raise SystemExit("overlay climb landed on 2.0 without crossing")
    else:
        raise SystemExit("overlay climb did not reach 2.0")
    cross = A.Slot(lives=4, snapshot=9, timer=probe.timer)
    A.tick_overlay([cross])
    if cross.snapshot != 4 or cross.timer != A.TWO:
        raise SystemExit("overlay clamp did not copy lives: %s %s" % (hex(cross.timer), cross.snapshot))
    check("{")
    check("Slot slot{0, 4, 9, %s};" % hx(probe.timer))
    check("tick_overlay(&slot, 1);")
    check("CHECK(slot.timer == TWO);")
    check("CHECK(slot.snapshot == 4);")
    check("}")

    check("CHECK(overlay_draws(ZERO, 0));")
    check("CHECK(!overlay_draws(TWO, 0));")
    check("CHECK(!overlay_draws(TWO, 4));")
    check("CHECK(overlay_anchor_x(100, K_259) == K_259);")
    check("CHECK(overlay_anchor_x(400, K_259) == neg(K_259));")
    check("CHECK(overlay_anchor_x(100, neg(K_259)) == neg(K_259));")
    p0 = [from_int(1), from_int(2), from_int(3)]
    p1 = [from_int(4), from_int(5), from_int(6)]
    m0 = [from_int(7), from_int(8), from_int(9)]
    m1 = [from_int(10), from_int(11), from_int(12)]
    for label, t in (("ZERO", ZERO), ("ONE", ONE)):
        got = A.hermite(t, p0, p1, m0, m1)
        check("{")
        check("const auto got = hermite(%s, %s, %s, %s, %s);" % (
            "ZERO" if t == ZERO else "ONE", emit_vec(p0), emit_vec(p1), emit_vec(m0), emit_vec(m1)))
        check("CHECK(got[0] == %s && got[1] == %s && got[2] == %s);" % (hx(got[0]), hx(got[1]), hx(got[2])))
        check("}")
    samples = [
        (ZERO, [from_int(1), from_int(2), from_int(3)], [from_int(9), from_int(8), from_int(7)],
         [from_int(4), ZERO, from_int(-2)], [ONE, TWO, ZERO]),
        (from_int(1), [A.K_259, A.K_290, ZERO], [from_int(100), from_int(10), ZERO],
         [A.K_259, A.K_290, ZERO], [A.K_M500, A.K_M500, ZERO]),
        (A.overlay_t(from_int(1), 2), [ZERO, ZERO, ZERO], [ONE, ONE, ONE], [TWO, TWO, TWO],
         [A.HALF, A.HALF, A.HALF]),
    ]
    for index, (t, a, b, c, d) in enumerate(samples):
        got = A.hermite(t, a, b, c, d)
        check("{")
        check("const auto got = hermite(%s, %s, %s, %s, %s);" % (
            hx(t), emit_vec(a), emit_vec(b), emit_vec(c), emit_vec(d)))
        check("CHECK(got[0] == %s && got[1] == %s && got[2] == %s);" % (hx(got[0]), hx(got[1]), hx(got[2])))
        check("}")
    faded = A.fade_step(ZERO)
    check("CHECK(fade_step(ZERO) == %s);" % hx(faded))
    check("CHECK(fade_step(ONE) == ONE);")

    count, quot, rem = A.spawn_budget(0, 1)
    check("CHECK(spawn_budget(0, 1).count == %d);" % count)
    check("CHECK(spawn_budget(0, 1).quot == %d && spawn_budget(0, 1).rem == %d);" % (quot, rem))
    check("CHECK(monster_level(0) == K_FIVE);")
    for wave in (0, 1, 2, 3, 5, 8):
        check("CHECK(monster_level(%d) == %s);" % (wave, hx(A.monster_level(wave))))
        for active in (1, 4):
            try:
                budget = A.spawn_budget(wave, active)
            except ZeroDivisionError:
                check("CHECK_THROWS(spawn_budget(%d, %d));" % (wave, active))
                continue
            check("CHECK(spawn_budget(%d, %d).count == %d);" % (wave, active, budget[0]))
            check("CHECK(spawn_budget(%d, %d).quot == %d && spawn_budget(%d, %d).rem == %d);" % (
                wave, active, budget[1], wave, active, budget[2]))
    big = A.spawn_budget(300, 4)
    check("CHECK(spawn_budget(300, 4).count == %d);" % big[0])
    record = bytes((0x30, 3, 1, 0, 0, 0))
    check("{")
    check("const Recipe record{{0x30, 3, 1, 0, 0, 0}};")
    for rem, count in ((0, 4), (1, 5), (2, 5), (4, 5)):
        groups = A.recipe_groups(record, rem, count)
        check("CHECK((recipe_groups(record, %d, %d) == std::vector<int>{%s}));" % (
            rem, count, ", ".join(str(n) for n in groups)))
    check("}")
    check("CHECK(level_record_index(2, 5) == %d);" % A.level_record_index(2, 5))
    check("CHECK(mode_a_next_index(12, 4) == 4);")
    check("CHECK(mode_a_next_index(0, 4) == 1);")
    check("CHECK(round_expired(0, false) == Round::Level);")
    check("CHECK(round_expired(2, false) == Round::Wave);")
    check("CHECK(round_expired(2, true) == Round::Level);")
    check("CHECK(intermission_done(INTERMISSION_OFFLINE, false));")
    check("CHECK(!intermission_done(INTERMISSION_OFFLINE, true));")
    check("CHECK(intermission_done(INTERMISSION_ONLINE, true));")
    # Synthetic tables. The real names are read from the owner's executable.
    check("{")
    check("NameTable a, b;")
    check("for (int i = 0; i < %d; ++i) a.push_back(\"a\" + std::to_string(i));" % A.MODE_A_MAPS)
    check("for (int i = 0; i < 7; ++i) b.push_back(\"b\" + std::to_string(i));")
    check('CHECK(mode_a_name(a, 12) == "a12");')
    check('CHECK(mode_a_name(a, %d) == "a%d");' % (A.mode_a_next_index(12, 2), A.mode_a_next_index(12, 2)))
    check('CHECK(mode_b_name(b, 0) == "b0");')
    check('CHECK(mode_b_name(b, 6) == "b6");')
    check("}")
    for wave, active in ((0, 1), (1, 1), (2, 4), (5, 1)):
        classes, seed, steps = A.wave_plan(wave, active, 1, [record], 20973, 0)
        check("{")
        check("const Recipe record{{0x30, 3, 1, 0, 0, 0}};")
        check("const auto plan = wave_plan(%d, %d, &record, 1, 20973, 0);" % (wave, active))
        check("CHECK(plan.has_classes);")
        check("CHECK((plan.classes == std::vector<int>{%s}));" % ", ".join(str(n) for n in classes))
        check("CHECK(plan.seed == %s);" % hx(seed))
        check("CHECK(plan.steps.size() == %d);" % len(steps))
        for i, (n, angle, draw) in enumerate(steps):
            check("CHECK(plan.steps[%d].count == %d);" % (i, n))
            check("CHECK(plan.steps[%d].angle == %s);" % (i, hx(angle)))
            check("CHECK(plan.steps[%d].draw == %d);" % (i, draw))
        check("}")
    saved_classes, saved_seed, saved_steps = A.wave_plan(0, 1, 1, [record], 20973, 3)
    check("{")
    check("const auto plan = wave_plan(0, 1, nullptr, 0, 20973, 3);")
    check("CHECK(!plan.has_classes);")
    check("CHECK(plan.seed == %s);" % hx(saved_seed))
    check("CHECK(plan.steps.size() == 1 && plan.steps[0].count == %d);" % saved_steps[0][0])
    check("CHECK(plan.steps[0].angle == %s && plan.steps[0].draw == %d);" % (hx(saved_steps[0][1]), saved_steps[0][2]))
    check("}")
    check("CHECK_THROWS_ZERO(([&]{ const Recipe record{{0x10, 3, 1, 0, 0, 0}}; recipe_groups(record, 1, 5); }()));")

    # Spawn picks. Searches return scripted positions.
    points = [point(0, 0, 5, 0x1000), point(128, 0, 1, 0x2000), point(256, 0, 9, 0x0100)]
    focus = (ZERO, ZERO, ZERO)
    camera = (from_int(3), from_int(4), from_int(5))

    def run_pick(name, flag, slot, seed, mode_a, scramble, network, active, script, table=None):
        used = table if table is not None else points
        searches = []

        def search(centre, spread, radius):
            searches.append((spread, radius))
            pos, ok = script[len(searches) - 1]
            return list(pos), ok

        pos, facing, out_seed = A.pick_spawn(
            flag, slot, used, seed, mode_a, scramble, network, focus, active, search, camera)
        check("{")
        check("const SpawnPoint table[] = {%s};" % ", ".join(emit_point(p) for p in used))
        check("std::vector<Spot> script{%s};" % ", ".join(
            "Spot{%s, %d}" % (emit_vec(pos), ok) for pos, ok in script))
        check("std::size_t at = 0;")
        check("std::vector<std::pair<int, int>> spreads;")
        check("const auto got = pick_spawn(%d, %d, table, %d, %d, %s, %s, %s, %s, %d," % (
            flag, slot, len(used), seed, "true" if mode_a else "false",
            "true" if scramble else "false", "true" if network else "false",
            emit_vec(focus), active))
        check("  [&](const Vec3 &, int spread, int radius, std::uint32_t &) {")
        check("    spreads.push_back({spread, radius});")
        check("    return script[at++];")
        check("  }, %s);" % emit_vec(camera))
        check("CHECK(spreads.size() == %d);" % len(searches))
        for i, (spread, radius) in enumerate(searches):
            check("CHECK(spreads[%d].first == %d && spreads[%d].second == %d);" % (i, spread, i, radius))
        check("CHECK(got.pos[0] == %s && got.pos[1] == %s && got.pos[2] == %s);" % (hx(pos[0]), hx(pos[1]), hx(pos[2])))
        check("CHECK(got.facing == %d);" % facing)
        check("CHECK(got.seed == %s);" % hx(out_seed))
        check("}")

    here = (from_int(9), from_int(8), from_int(7))
    run_pick("ring-a", 1, 1, 1, True, False, False, 4, [(here, 1)])
    run_pick("ring-retry", 1, 2, 1, False, False, False, 4,
             [((from_int(1), from_int(1), from_int(1)), 0), (here, 1)])
    run_pick("ring-scramble", 1, 0, 1, True, True, False, 3, [(here, 1)])
    run_pick("net", 0, 1, 20973, False, False, True, 1, [])
    run_pick("offline", 0, 0, 7, False, False, False, 1, [])
    far = [point(256, 0, 9, 0x0100)]
    run_pick("camera", 0, 0, 3, False, False, False, 1, [(camera, 1)], table=far)

    bound = A.armour_bound(from_int(100), 300)
    check("CHECK(armour_bound(from_int(100), 300) == %s);" % hx(bound))
    check("CHECK(armour_bound(from_int(100), 300) != from_int(100));")
    check("CHECK(pvp_half(from_int(100)) == from_int(50));")
    check("CHECK(pvp_half(from_int(40)) == %s);" % hx(A.pvp_half(from_int(40))))
    check("CHECK(spell_arena_flag(61) && !spell_arena_flag(60));")
    check("CHECK(spell_arena_tint(30) && !spell_arena_tint(29) && !spell_arena_tint(-1));")
    check("{")
    check("const auto up = spell_arena(180, 0x11);")
    check("CHECK(up.flags == 0x13u && up.tint && up.particles);")
    check("const auto down = spell_arena(40, 0x13);")
    check("CHECK(down.flags == 0x11u && down.tint);")
    check("const auto early = spell_arena(10, 0x13);")
    check("CHECK(early.flags == 0x11u && !early.tint && !early.particles);")
    check("const auto steady = spell_arena(0xffff, 0x13);")
    check("CHECK(steady.flags == 0x11u && !steady.tint);")
    check("}")
    check("{")
    check("const auto bought = buy_life(100, 100);")
    check("CHECK(bought.gold == 0 && bought.budget == 200 && bought.bought);")
    check("CHECK(!buy_life(99, 100).bought);")
    check("}")
    for a, b, ka, kb, want in ((0, 1, 1, 5, A.compare_kills(0, 1, 1, 5)),
                               (1, 0, 5, 1, A.compare_kills(1, 0, 5, 1)),
                               (0, 2, 1, 1, A.compare_kills(0, 2, 1, 1)),
                               (2, 0, 1, 1, A.compare_kills(2, 0, 1, 1))):
        check("CHECK(compare_kills(%d, %d, %d, %d) == %d);" % (a, b, ka, kb, want))
    check("CHECK(kill_limit_reached(10, 10));")
    check("CHECK(!kill_limit_reached(9, 10));")
    check("CHECK(kill_limit_reached(0, 0));")
    check("CHECK(!kill_limit_reached(0xffff, 0));")
    check("CHECK(still_in(true, 0, ZERO, 0));")
    check("CHECK(still_in(true, 0xffff, ZERO, 1));")
    check("CHECK(!still_in(true, 0xffff, ZERO, 0));")
    check("CHECK(!still_in(false, 2, from_int(10), 0));")
    check("CHECK(can_play_intermission(true, 0xffff, from_int(1)));")
    check("CHECK(!can_play_intermission(true, 0xffff, ZERO));")

    # Overlay glyphs for the two-slot draw the difftest executed.
    slots = [A.Slot(lives=2, timer=ZERO), A.Slot(lives=2, timer=from_int(1))]
    screens = [(100, 10), (400, 20)]
    anchors_x = [11, 22]
    anchors_y = [33, 44]
    expect = []
    anchor = A.K_259
    for slot, column in enumerate(slots):
        sx, sy = screens[slot]
        anchor = A.overlay_anchor_x(sx, anchor)
        for k in range(5):
            if not A.overlay_draws(column.timer, k):
                continue
            p0 = [from_int(anchors_x[slot]), from_int(anchors_y[slot]), ZERO]
            p1 = [from_int(sx), from_int(sy), ZERO]
            m0 = [anchor, A.K_290, ZERO]
            m1 = [A.K_M500, A.K_M500, ZERO]
            t = A.overlay_t(column.timer, k)
            expect.append((k, t, p0, p1, m0, m1, A.hermite(t, p0, p1, m0, m1),
                           128 - 16 * k, 2000 - 200 * k, 0 if k == 0 else 1))
    check("{")
    check("Slot columns[2] = {Slot{0, 2, 0, ZERO}, Slot{0, 2, 0, from_int(1)}};")
    check("const int sx[] = {100, 400}, sy[] = {10, 20}, ax[] = {11, 22}, ay[] = {33, 44};")
    check("const auto hud = draw_hud(true, false, false, false, columns, 2, sx, sy, ax, ay);")
    check("CHECK(hud.drawn);")
    check("CHECK(hud.glyphs.size() == %d);" % len(expect))
    for i, item in enumerate(expect):
        k, t, p0, p1, m0, m1, out, colour, figure, font = item
        check("CHECK(hud.glyphs[%d].k == %d);" % (i, k))
        check("CHECK(hud.glyphs[%d].t == %s);" % (i, hx(t)))
        check("CHECK(hud.glyphs[%d].colour == %d && hud.glyphs[%d].figure == %d && hud.glyphs[%d].font == %d);" % (
            i, colour, i, figure, i, font))
        check("CHECK(hud.glyphs[%d].out[0] == %s && hud.glyphs[%d].out[1] == %s && hud.glyphs[%d].out[2] == %s);" % (
            i, hx(out[0]), i, hx(out[1]), i, hx(out[2])))
        check("CHECK(hud.glyphs[%d].m0[0] == %s);" % (i, hx(m0[0])))
    check("const auto hidden = draw_hud(true, true, false, false, columns, 2, sx, sy, ax, ay);")
    check("CHECK(!hidden.drawn && hidden.glyphs.empty());")
    check("const auto menu = draw_hud(true, false, false, true, columns, 2, sx, sy, ax, ay);")
    check("CHECK(!menu.drawn);")
    check("}")

    nxt, bit = A.lcg(1)
    check("{")
    check("const auto player = visit_creature(0x40, 1);")
    check("CHECK(player.seed == 1 && !player.called);")
    check("const auto both = visit_creature(0x60, 1);")
    check("CHECK(both.seed == 1 && !both.called);")
    check("const auto creature = visit_creature(0x20, 1);")
    check("CHECK(creature.seed == %s);" % hx(nxt))
    check("CHECK(creature.called == %s);" % ("true" if bit & 1 else "false"))
    check("}")
    other = 1
    while (A.lcg(other)[1] & 1) == (bit & 1):
        other += 1
    nxt2, bit2 = A.lcg(other)
    check("{")
    check("const auto creature = visit_creature(0x20, %d);" % other)
    check("CHECK(creature.seed == %s && creature.called == %s);" % (hx(nxt2), "true" if bit2 & 1 else "false"))
    check("}")
    check("CHECK_THROWS(visit_creature(0, 1));")

    text = "\n".join(lines) + "\n"
    destination.write_text(text)
    print("checks %d" % text.count("CHECK("))


if __name__ == "__main__":
    main()
