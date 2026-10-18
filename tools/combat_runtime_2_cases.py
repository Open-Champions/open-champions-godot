"""Synthetic scenarios of the round-2 combat fixtures (see generate_combat_runtime_2_vectors.py).

Each scenario is an AUTHORED set-up; what happens afterwards is the research runtime's.
The composed scenarios follow tools/test_ref_combat_full.py; the direct-call families
cover the executed bodies' branches with seeded random inputs, in the domains of the
research's difftests, on the composed world instead of an emulator."""

PLANES_INSIDE = [((0.0, 0.0, -1000.0), (0.0, 0.0, 1.0))] * 4
WAVE = {0: (1, 10, 0.0), 1: (1, 20, 0.0)}


def build(gen):
    composed(gen)


# ---------------------------------------------------------------------------------------
# The composed scenarios (tools/test_ref_combat_full.py)
# ---------------------------------------------------------------------------------------
def composed(gen):
    # (a) round 1's duel in the full world
    c = gen.case('full-duel')
    p = c.spawn_player(0, (0.0, 0.0, 0.0), strength=40)
    s = c.spawn_skelight((30.0, 0.0, 0.0), facing=0x8000, hp=40.0)
    c.swing_clips(s)
    for t in range(80):
        if t == 30:
            c.attack(0)
        c.tick()
    c.end()
    # (b) walk into a Skelight, then an armed combo kill
    c = gen.case('walk-and-combo')
    p = c.spawn_player(0, (0.0, 0.0, 0.0), strength=40)
    c.weapon(0, dmg=(30, 40), attack=50, dmg_type=8)
    s = c.spawn_skelight((80.0, 0.0, 0.0), facing=0x8000, hp=120.0)
    c.swing_clips(s)
    c.stick(0, 1.0, 0.0)
    for _ in range(40):
        c.tick()
    c.stick(0, 0.0, 0.0)
    for t in range(200):
        if s not in c.rt.world.objects or c.dead:
            break
        if t % 12 == 0:
            c.attack(0)
        c.tick()
    c.end()
    # (c) an arrow kill
    c = gen.case('arrow-kill')
    p = c.spawn_player(0, (0.0, 0.0, 0.0))
    s = c.spawn_skelight((150.0, 0.0, 0.0), facing=0x8000, hp=3.0)
    for _ in range(3):
        c.tick()
    a = c.missile(p, (20.0, 0.0, 36.0), (1000.0, 0.0, 0.0), dmin=4, dmax=8, attack=100000)
    for _ in range(12):
        c.tick()
    c.end()
    # (d) a burn kill from Wave of Flame
    c = gen.case('burn-kill')
    p = c.spawn_player(0, (0.0, 0.0, 0.0))
    c.skill(95, WAVE)
    s = c.spawn_skelight((60.0, 0.0, 0.0), hp=31.0)
    c.cast(p, 95, level=3)
    for _ in range(160):
        if s not in c.rt.world.objects:
            break
        c.tick()
    c.end()
    # (e) two players, two monsters: draw accounting
    c = gen.case('two-and-two', planes=PLANES_INSIDE)
    p0 = c.spawn_player(0, (0.0, 0.0, 0.0), strength=40)
    p1 = c.spawn_player(1, (0.0, 120.0, 0.0), strength=40, idle_voice_limit=40)
    a = c.spawn_skelight((30.0, 0.0, 0.0), facing=0x8000, hp=60.0)
    b = c.spawn_skelight((30.0, 120.0, 0.0), facing=0x8000, hp=60.0)
    c.swing_clips(a)
    c.swing_clips(b)
    for t in range(90):
        if t in (30, 55):
            c.attack(0)
        c.tick()
    c.end()
    # (f) the 200-tick mixed scenario
    c = gen.case('mixed-200', planes=PLANES_INSIDE)
    p0 = c.spawn_player(0, (0.0, 0.0, 0.0), strength=40)
    p1 = c.spawn_player(1, (0.0, 150.0, 0.0), strength=30, idle_voice_limit=90)
    c.weapon(0, dmg=(20, 30), attack=50, dmg_type=8)
    c.skill(95, WAVE)
    a = c.spawn_skelight((90.0, 0.0, 0.0), facing=0x8000, hp=90.0)
    b = c.spawn_skelight((60.0, 150.0, 0.0), facing=0x8000, hp=40.0)
    c.swing_clips(a)
    c.swing_clips(b)
    for t in range(200):
        c.stick(0, 1.0 if t < 25 else 0.0, 0.0)
        if t >= 30 and t % 12 == 0:
            c.attack(0)
        if t == 10:
            c.missile(p1, (20.0, 150.0, 36.0), (1000.0, 0.0, 0.0), dmin=4, dmax=8, attack=100000)
        if t == 60:
            c.cast(p1, 95, level=3)
        c.tick()
    c.end()
