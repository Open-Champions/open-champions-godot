"""Synthetic scenarios of the round-2 combat fixtures (see generate_combat_runtime_2_vectors.py).

Each scenario is an AUTHORED set-up; what happens afterwards is the research runtime's.
The composed scenarios follow tools/test_ref_combat_full.py; the direct-call families
cover the executed bodies' branches with seeded random inputs, in the domains of the
research's difftests, on the composed world instead of an emulator."""

PLANES_INSIDE = [((0.0, 0.0, -1000.0), (0.0, 0.0, 1.0))] * 4
WAVE = {0: (1, 10, 0.0), 1: (1, 20, 0.0)}


def build(gen):
    composed(gen)
    movement(gen)
    melee(gen)
    defence(gen)
    monsters(gen)
    queries(gen)
    ranged(gen)
    spells(gen)
    ticks(gen)
    homes(gen)


G_PADS, PAD_SIZE = 0x41BFD8, 0x58
G_PLAYERS, REC_SIZE = 0x4D8AF0, 0x4C30
G_SIM_FRAME = 0x44FDCC
G_LEVEL_NAME = 0x41B721
SCRATCH = 0x00080000                            # AUTHORED fixture words (vectors, masks, lists)


def f32(gen, v):
    return gen.T.f32(v)


def rand_planes(rng):
    """Four AUTHORED screen planes around the origin; some players stand outside."""
    out = []
    for k in range(4):
        angle = rng.choice((0.0, 1.0, -1.0, 0.5))
        normal = [(1.0, 0.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, -1.0, 0.0)][k]
        point = tuple(-n * rng.choice((40.0, 80.0, 200.0)) for n in normal)
        out.append((point, normal))
        del angle
    return out


def ftuple(gen, v):
    return tuple(gen.T.f32(x) for x in v)


# ---------------------------------------------------------------------------------------
# Movement: obj_moveWithCollision, Player_move (every mode), Player_animStep, clamp
# ---------------------------------------------------------------------------------------
def movement(gen):
    rng = gen.rng
    m = None
    for k in range(120):
        c = gen.case('move-wc-%d' % k)
        T = c.T
        player_mover = k % 3 == 0
        if player_mover:
            mover = c.spawn_player(0, (0.0, 0.0, rng.choice((0.0, 0.0, 30.0))))
        else:
            mover = c.spawn_skelight((0.0, 0.0, rng.choice((0.0, 0.0, 20.0))), team=1)
        others = []
        for j in range(rng.randrange(1, 5)):
            pos = (rng.uniform(-30, 30), rng.uniform(-30, 30), rng.choice((0.0, 0.0, 10.0, -50.0)))
            others.append(c.spawn_skelight(pos, team=rng.choice((1, 2, 0xFF))))
        for o in others:
            flags = c.rt.u32(o + 0xC)
            extra = rng.choice((0, 0, 0x10, 0x200, 0x1, 0x10 | 0x200))
            if extra:
                c.write(o + 0xC, 4, flags | extra)
            if rng.random() < 0.3:
                c.write(o + 0x20, 4, f32(gen, rng.choice((4.0, 40.0, 100.0))))
        mf = c.rt.u32(mover + 0xC)
        mextra = rng.choice((0, 0, 0x10, 0x400, 0x20000))
        if mextra:
            c.write(mover + 0xC, 4, mf | mextra)
        if rng.random() < 0.4:
            c.write(mover + 0x30, 4, f32(gen, rng.choice((-50.0, 5.0, 100.0))))
        vel = (rng.uniform(-600, 600), rng.uniform(-600, 600), rng.choice((0.0, 0.0, 50.0, -200.0)))
        if rng.random() < 0.15:
            vel = (0.01, 0.0, 0.0)
        ignore = rng.choice([0, 0] + others)
        skip = rng.choice((0, 0, 1))
        vv = ftuple(gen, vel)
        c.call('move_wc', [mover, *vv, ignore, skip],
               lambda: c.rt.model.move_with_collision(mover, vv, ignore, skip))
        c.end()
    for k in range(260):
        planes = rand_planes(rng) if k % 5 == 0 else None
        c = gen.case('player-move-%d' % k, planes=planes)
        P = c.spawn_player(0, (rng.uniform(-50, 50), rng.uniform(-50, 50), rng.choice((0.0, 0.0, 0.0, 12.0))),
                           facing=rng.randrange(0x10000))
        if planes is not None:
            c.spawn_player(1, (rng.uniform(-150, 150), rng.uniform(-150, 150), 0.0))
        R = G_PLAYERS
        mode = rng.choice((0, 0, 0, 1, 2, 3, 4, 5, 6, 7))
        state = rng.choice((1, 2, 2, 3, 8))
        c.write(P + 0x58C, 4, state)
        if rng.random() < 0.4:
            c.write(P + 0x44C, 4, 2)
        c.write(R + 0x4A86, 1, rng.randrange(8))
        c.write(G_SIM_FRAME, 4, rng.randrange(0, 400))
        c.write(P + 0x448, 4, rng.randrange(0, 400))
        if rng.random() < 0.3:
            c.write(P + 0x2FC, 1, rng.choice((1, 30, 60, 61)))
        if rng.random() < 0.3:
            c.write(P + 0x481, 1, rng.choice((1, 3)))
        for off in (0x47C, 0x47D, 0x47E):
            if rng.random() < 0.3:
                c.write(P + off, 1, 1)
        if rng.random() < 0.3:
            c.write(P + 0x46C, 4, f32(gen, rng.choice((0.0, 0.04, 0.5, 1.0))))
        if rng.random() < 0.3:
            c.write(P + 0x468, 4, f32(gen, rng.choice((0.0, 0.3, 0.9))))
        if rng.random() < 0.3:
            c.write(P + 0x482, 1, rng.choice((0, 1, 15)))
        if rng.random() < 0.2:
            c.write(P + 0x30, 4, f32(gen, rng.choice((-30.0, 20.0))))
        if rng.random() < 0.2:
            c.write(P + 0x490, 1, rng.choice((1, 2, 3)))
        if rng.random() < 0.15:
            c.write(R + 0x4C20, 4, f32(gen, rng.choice((0.0, 0.5, 2.0))))
        if rng.random() < 0.1:
            c.write(P + 0xA8, 8, (1 << 61) | c.rt.mem.load(P + 0xA8, 8))
            c.write(0x44FE08, 4, 0x1000)
        if rng.random() < 0.15:
            c.write(P + 0x31A, 2, 5)
        if rng.random() < 0.25:
            c.root(P, rng.uniform(-2, 2), rng.uniform(-2, 2))
        if rng.random() < 0.3:
            c.yaw(rng.choice((0x4000, -0x1234, rng.randrange(-0x8000, 0x8000))))
        if rng.random() < 0.1:
            c.dialog(1)
        if rng.random() < 0.15:
            others = [c.spawn_skelight((rng.uniform(-30, 30), rng.uniform(-30, 30), 0.0)) for _ in range(2)]
            del others
        pad = G_PADS
        stick = rng.choice(((0.0, 0.0), (1.0, 0.0), (0.0, -1.0), (rng.uniform(-1, 1), rng.uniform(-1, 1)),
                            (0.005, 0.0), (0.7, 0.7)))
        c.write(pad + 0x14, 4, f32(gen, stick[0]))
        c.write(pad + 0x18, 4, f32(gen, stick[1]))
        ext = ftuple(gen, (rng.uniform(-200, 200), rng.uniform(-200, 200), 0.0))
        c.call('player_move', [P, mode, pad, *ext], lambda: c.rt.model.player_move(P, mode, pad, ext))
        if k % 4 == 0:
            for _ in range(3):
                c.call('player_move', [P, mode, pad, *ext], lambda: c.rt.model.player_move(P, mode, pad, ext))
        c.end()
    for k in range(60):
        c = gen.case('anim-step-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.randrange(0x10000))
        if rng.random() < 0.5:
            c.root(P, rng.uniform(-3, 3), rng.uniform(-3, 3))
        if rng.random() < 0.3:
            c.write(P + 0x2FC, 1, rng.choice((1, 30, 60, 61)))
        if rng.random() < 0.2:
            c.write(P + 0x31A, 2, 3)
        if rng.random() < 0.3:
            c.write(P + 0xC, 4, c.rt.u32(P + 0xC) | 0x200000)
        speed = f32(gen, rng.choice((1.0, 0.5, 2.3)))
        c.call('anim_step', [P, speed], lambda: c.rt.model.player_anim_step(P, speed))
        c.end()
    for k in range(60):
        planes = rand_planes(rng)
        c = gen.case('clamp-%d' % k, planes=planes)
        P = c.spawn_player(0, (rng.uniform(-250, 250), rng.uniform(-250, 250), 0.0))
        v = ftuple(gen, (rng.uniform(-400, 400), rng.uniform(-400, 400), rng.uniform(-50, 50)))
        vptr, maskptr = SCRATCH, SCRATCH + 0x10
        for i in range(3):
            c.write(vptr + 4 * i, 4, v[i])
        c.call('clamp', [P, vptr, maskptr], lambda: c.rt.model.player_clamp_to_screen(P, vptr, maskptr))
        c.end()


# ---------------------------------------------------------------------------------------
# Player offence: Player_pickAttackAnim, Player_meleeHit (every branch), Player_hitSparks
# ---------------------------------------------------------------------------------------
def melee(gen):
    rng = gen.rng
    for k in range(120):
        c = gen.case('pick-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0))
        c.write(G_PLAYERS + 0x54, 4, rng.choice((0, 1, 4, 9, 9, 11, 11, 12)))
        c.write(P + 0x461, 1, rng.randrange(0, 7))
        c.write(G_SIM_FRAME, 4, rng.getrandbits(32))
        cls = rng.choice((0, 1, 2, 3, 4, 5, 6, 7))
        c.call('pick', [P, cls], lambda: c.rt.model.pick_attack_anim(P, cls))
        c.end()
    for k in range(260):
        c = gen.case('melee-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0), strength=rng.choice((10, 40, 90)))
        if rng.random() < 0.2:
            t = c.spawn_player(1, (30.0, 0.0, 0.0))
        else:
            t = c.spawn_skelight((30.0, rng.uniform(-10, 10), 0.0), armor=rng.choice((0, 10)),
                                 cflags=rng.choice((0x100000, 0x2000, 0x1000, 0x40000, 0)))
        R = G_PLAYERS
        if rng.random() < 0.7:
            mods = []
            for _ in range(rng.choice((0, 0, 1, 2))):
                mods.append((rng.choice((0x15, 0x16, 0x17, 0x18, 0x26, 0x1, 0x5)), rng.randrange(1, 20),
                             rng.randrange(0, 20), 0))
            c.weapon(0, dmg=(rng.randrange(1, 20), rng.randrange(20, 60)), attack=rng.randrange(0, 100),
                     subtype=rng.choice((0, 0, 1, 2, 3, 5, 6, 4)), dmg_type=rng.choice((1, 2, 4, 8, 0x40)),
                     cls=rng.choice((0, 1, 2, 3, 5, 7, 4)), mods=mods)
        else:
            c.write(R + 0x4A86, 1, rng.choice((0, 1, 2, 3, 5, 7)))
        if rng.random() < 0.15:
            second = c.weapon(1, dmg=(3, 5)) if False else None
            c.write(R + 0x43F4, 4, c.rt.u32(R + 0x4434) or 0)
            del second
        if rng.random() < 0.2:
            # the class skill table: the dual-wield or heavy passive at an entry
            cls = rng.randrange(0, 3)
            c.write(R + 0x50, 4, cls)
            n = rng.randrange(0, 48)
            table = 0x4EF538 + cls * 0xA88
            c.write(table + n * 0x38, 8, rng.choice((0x40000, 0x20000)))
            c.write(R + 0xEF + n, 1, rng.choice((1, 3, 0xFF)))
        c.write(P + 0x460, 1, rng.randrange(0, 8))
        c.write(G_SIM_FRAME, 4, rng.randrange(60, 200))
        c.write(P + 0x448, 4, rng.randrange(0, 200))
        if rng.random() < 0.25:
            c.write(P + 0x44C, 4, 2)
        if rng.random() < 0.1:
            c.write(P + 0xC, 4, c.rt.u32(P + 0xC) | 0x20000)
        if rng.random() < 0.15:
            c.ray(f32(gen, rng.choice((0.0, 0.5))))
        c.write(0x404464, 4, rng.getrandbits(32))
        scale = f32(gen, rng.choice((0.0, 0.0, 0.0, 1.5, 0.5)))
        extra = rng.choice((0, 0, 8, 0x40))
        c.call('melee', [P, t, extra, scale], lambda: c.rt.model.player_melee_hit_full(P, t, 0, extra, scale))
        c.end()
    for k in range(12):
        c = gen.case('hit-sparks-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0))
        t = c.spawn_skelight((30.0, 0.0, 0.0))
        c.call('hit_sparks', [P, t], lambda: c.rt.model.player_hit_sparks(P, t))
        c.end()


# ---------------------------------------------------------------------------------------
# Player defence: Player_takeDamage, Player_processDamage, Player_blockSparks, interact
# ---------------------------------------------------------------------------------------
def defence(gen):
    rng = gen.rng
    for k in range(220):
        c = gen.case('take-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.choice((0, 0x8000, 0x4000)))
        a = c.spawn_skelight((rng.choice((30.0, -30.0)), rng.uniform(-10, 10), 0.0))
        c.write(P + 0x58C, 4, rng.choice((2, 8, 8, 0xB, 1, 3)))
        if rng.random() < 0.1:
            c.write(G_PLAYERS + 0x4C27, 1, 3)
        if rng.random() < 0.08:
            c.write(P + 0x4B4, 2, 5)
        if rng.random() < 0.05:
            for i, ch in enumerate(b'actintro\0'):
                c.write(G_LEVEL_NAME + i, 1, ch)
        if rng.random() < 0.05:
            c.travel(1)
        if rng.random() < 0.2:
            c.write(P + 0xC8, 4, f32(gen, 5.0))
        c.write(0x404464, 4, rng.getrandbits(32))
        flags = rng.choice((8, 0x40, 0x20000 | 8, 0x800000 | 8, 0x10000000 | 8, 0x40008, 0x80008, 0x200000))
        amount = f32(gen, rng.choice((0.0, 1.0, 10.0, 200.0)))
        attacker = a if rng.random() < 0.9 else 0
        c.call('take', [P, attacker, flags, amount],
               lambda: c.rt.model.player_take_damage_full(P, attacker, flags, amount))
        c.end()
    for k in range(320):
        c = gen.case('process-%d' % k, skill_stat=rng.choice((0, 0, 0, 50, 100)))
        P = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.randrange(0x10000), hp=rng.choice((100.0, 5.0)))
        if rng.random() < 0.3:
            c.spawn_player(1, (100.0, 0.0, 0.0))
        a = c.spawn_skelight((30.0, 0.0, 0.0))
        c.write(P + 0x58C, 4, rng.choice((2, 2, 3, 4, 6, 7, 8, 10, 1)))
        c.write(P + 0xC8, 4, f32(gen, rng.choice((0.0, 3.0, 10.0, 200.0))))
        flags = 0
        for bit in (0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x4000000, 0x20000000, 0x10, 0x1, 0x4):
            if rng.random() < 0.15:
                flags |= bit
        c.write(P + 0x2DC, 4, flags)
        c.write(P + 0x2E0, 2, rng.randrange(0x10000))
        if rng.random() < 0.2:
            c.write(P + 0x44C, 4, 2)
        if rng.random() < 0.2:
            c.write(G_PLAYERS + 0xDC, 4, f32(gen, rng.choice((0.0, 0.5, 2.0))))
        if rng.random() < 0.08:
            c.write(0x44FDC0, 4, 1)
        if rng.random() < 0.05:
            c.write(0x4F711C, 4, 1)
        if rng.random() < 0.05:
            c.write(0x44FCD0, 1, 1)
        if rng.random() < 0.05:
            c.dialog(1)
        if rng.random() < 0.08:
            c.write(P + 0xC, 4, c.rt.u32(P + 0xC) | 0x20000)
        if rng.random() < 0.15:
            c.write(P + 0x118, 1, rng.choice((5, 6)))
            c.write(P + 0x10C + 32 * 5, 4, f32(gen, 3.0))
        if rng.random() < 0.05:
            c.write(0x44F20C, 4, 1)
        c.write(0x404464, 4, rng.getrandbits(32))
        if rng.random() < 0.5:
            c.link(P + 0x2E4, a)
        c.call('process', [P], lambda: c.rt.model.player_process_damage_full(P))
        c.end()
    for k in range(20):
        c = gen.case('block-sparks-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.randrange(0x10000))
        c.write(P + 0x44, 4, rng.getrandbits(32))
        silent = 1 if k == 7 else 0
        c.call('block_sparks', [P, silent], lambda: c.rt.model.player_block_sparks(P, silent))
        c.end()
    for k in range(40):
        c = gen.case('interact-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0))
        c.write(0x4D8AC0, 4, rng.choice((0, 5, 7, 100)) & 0xFFFFFFFF)
        c.write(0x864F50, 4, 0x41424344)
        if rng.random() < 0.15:
            c.write(P + 0x540, 4, 1)
        if rng.random() < 0.15:
            c.write(G_PLAYERS + 0xAC, 4, 0)
        if rng.random() < 0.2:
            o = c.spawn_skelight((20.0, 10.0, 0.0))
            c.write(o + 0xC, 4, c.rt.u32(o + 0xC) | 0x100000)
        pad = G_PADS
        c.call('interact', [P, pad, 0], lambda: c.rt.model.player_interact(P, pad, 0))
        c.end()


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


# ---------------------------------------------------------------------------------------
# Creature helpers, death and loot (ref_combat_monsters)
# ---------------------------------------------------------------------------------------
def monsters(gen):
    rng = gen.rng
    for k in range(200):
        c = gen.case('sight-%d' % k)
        cr = c.spawn_skelight((0.0, 0.0, rng.choice((0.0, 10.0))), facing=rng.randrange(0x10000), team=1)
        players = []
        for slot in range(rng.choice((1, 1, 2))):
            pos = (rng.uniform(-100, 100), rng.uniform(-100, 100), rng.choice((0.0, 0.0, 30.0)))
            players.append(c.spawn_player(slot, pos))
        o = c.spawn_skelight((rng.uniform(-100, 100), rng.uniform(-100, 100), 0.0), team=rng.choice((2, 0xFF)))
        if rng.random() < 0.2:
            c.write(players[0] + 0xC, 4, c.rt.u32(players[0] + 0xC) | 8)
        if rng.random() < 0.2:
            c.write(cr + 0x312, 2, 30)
        for off in (0x308, 0x30C):
            c.write(o + off, 4, f32(gen, rng.uniform(-20, 20)))
        c.ray(f32(gen, rng.choice((-1.0, -1.0, 0.0, 0.5, 1.0, 1.1))))
        facing = rng.randrange(0x10000)
        arc = rng.choice((0, 1, 6000, 0x2000, 0x7FFF))
        near = f32(gen, rng.choice((0.0, 10.0, 40.0)))
        far = f32(gen, rng.choice((20.0, 48.0, 100.0)))
        lead = f32(gen, rng.choice((0.0, 0.5)))
        mode = k % 2
        target = rng.choice(players + [o])
        pos = c.rt.model.vec(target + 0x10)
        c.call('see_point', [cr, *pos, facing, arc, near, far],
               lambda: c.rt.model.can_see_point(cr, pos, facing, arc, near, far))
        c.call('see_edge', [cr, target, facing, arc, mode, near, far, lead],
               lambda: c.rt.model.can_see_obj_edge(cr, target, facing, arc, mode, near, far, lead))
        c.call('see_obj', [cr, target, facing, arc, mode, near, far, lead],
               lambda: c.rt.model.can_see_obj(cr, target, facing, arc, mode, near, far, lead))
        c.call('player_cone', [cr, facing, arc, near, far],
               lambda: c.rt.model.find_player_in_cone(cr, facing, arc, near, far))
        c.call('enemy_cone', [cr, facing, arc, near, far],
               lambda: c.rt.model.find_enemy_in_cone(cr, facing, arc, near, far))
        c.end()
    for k in range(200):
        c = gen.case('hits-%d' % k)
        cr = c.spawn_skelight((0.0, 0.0, 0.0), facing=rng.choice((0, 0x4000, 0x8000)), team=1)
        P = c.spawn_player(0, (rng.uniform(-80, 80), rng.uniform(-80, 80), 0.0))
        o = c.spawn_skelight((rng.uniform(-80, 80), rng.uniform(-80, 80), 0.0), team=rng.choice((2, 1)),
                             hp=rng.choice((100.0, 1.0)))
        if rng.random() < 0.8:
            c.link(cr + 0x234, rng.choice((P, o)))
        if rng.random() < 0.2:
            t = rng.choice((P, o))
            c.write(t + 0xC, 4, c.rt.u32(t + 0xC) | 8)
        c.ray(f32(gen, rng.choice((-1.0, -1.0, 0.0, 0.5))))
        c.write(0x404464, 4, rng.getrandbits(32))
        facing = rng.choice((0, 0x4000, 0x8000, rng.randrange(0x10000)))
        arc = rng.choice((6000, 0x2000, 0x7FFF))
        lo, hi = rng.choice(((4, 8), (1, 2), (20, 30)))
        attack = rng.choice((100, 100000))
        flags = rng.choice((8, 0x40008, 0x80008))
        near = f32(gen, rng.choice((0.0, 10.0)))
        far = f32(gen, rng.choice((48.0, 100.0)))
        reach = f32(gen, rng.choice((0.0, 36.0, 60.0)))
        which = k % 5
        if which == 0:
            c.call('reach', [cr, lo, hi, attack, flags, reach],
                   lambda: c.rt.model.hit_target_at_reach(cr, lo, hi, attack, flags, 0, reach))
        elif which == 1:
            c.call('arc_edge', [cr, facing, arc, lo, hi, attack, flags, near, far],
                   lambda: c.rt.model.hit_target_in_arc_edge(cr, facing, arc, lo, hi, attack, flags, 0, near, far))
        elif which == 2:
            c.call('or_player', [cr, facing, arc, lo, hi, attack, flags, near, far],
                   lambda: c.rt.model.hit_target_or_player_in_arc(cr, facing, arc, lo, hi, attack, flags, 0, near,
                                                                 far))
        elif which == 3:
            c.call('arc_hit', [cr, facing, arc, lo, hi, attack, flags, near, far],
                   lambda: c.rt.model.hit_target_in_arc(cr, facing, arc, lo, hi, attack, flags, 0, near, far))
        else:
            c.call('arc_all', [cr, facing, arc, lo, hi, attack, flags, near, far],
                   lambda: c.rt.model.hit_all_enemies_in_arc(cr, facing, arc, lo, hi, attack, flags, 0, near, far))
        c.end()
    for k in range(80):
        c = gen.case('cooldown-%d' % k)
        cr = c.spawn_skelight((0.0, 0.0, 0.0), team=1, cflags=rng.choice((0x100000, 0x100001)))
        o = c.spawn_skelight((30.0, 20.0, 0.0), team=2)
        c.write(cr + 0x232, 2, rng.choice((0, 3, 0x14)))
        if rng.random() < 0.6:
            c.link(cr + 0x234, o)
        which = k % 3
        if which == 0:
            frames = rng.randrange(1, 100)
            c.call('cooldown', [cr, frames], lambda: c.rt.model.set_cooldown(cr, frames))
        elif which == 1:
            frames = rng.randrange(1, 100)
            obj = rng.choice((o, 0))
            c.call('cooldown_on', [cr, frames, obj], lambda: c.rt.model.set_cooldown_on(cr, frames, obj))
        else:
            speed = f32(gen, rng.choice((100.0, 250.0)))
            c.call('knock_target', [cr, speed], lambda: c.rt.model.knockback_target(cr, speed))
        c.end()
    for k in range(20):
        c = gen.case('notify-%d' % k)
        objs = [c.spawn_skelight((rng.uniform(-200, 200), rng.uniform(-200, 200), 0.0)) for _ in range(3)]
        P = c.spawn_player(0, (rng.uniform(-200, 200), 0.0, 0.0))
        pos = ftuple(gen, (rng.uniform(-50, 50), rng.uniform(-50, 50), 0.0))
        radius = f32(gen, rng.choice((60.0, 600.0)))
        c.call('notify', [5, *pos, objs[0], radius], lambda: c.rt.model.notify_objects_near(5, pos, objs[0], radius))
        del P
        c.end()
    for k in range(80):
        c = gen.case('dead-%d' % k)
        cf = rng.choice((0, 0x1000, 0x2000, 0x4000, 0x40000, 0x3000, 0x1000000))
        cr = c.spawn_skelight((10.0, -20.0, 0.0), cflags=cf)
        c.write(cr + 0x44, 4, rng.getrandbits(32))
        c.write(cr + 0x11A, 2, rng.choice((0, 8, 0x80)))
        c.write(cr + 0x118, 1, rng.choice((1, 2)))
        if rng.random() < 0.2:
            c.write(cr + 0xC, 4, c.rt.u32(cr + 0xC) | 2)
        c.write(cr + 0x2F4, 4, f32(gen, rng.choice((0.0, 71.9, 73.0))))
        events = rng.choice((0x80, 0, 0x81))
        hold = rng.choice((0, 0, 1))
        size = f32(gen, rng.choice((20.0, 0.0, 5.5)))
        off = ftuple(gen, (rng.uniform(-5, 5), rng.uniform(-5, 5), 0.0))
        c.call('dead', [cr, events, hold, size, *off], lambda: c.rt.model.dead_update(cr, events, off, hold, size))
        c.end()
    classes = ('SpiderQueen', 'AntQueen', 'UndeadKnight', 'VampireLord', 'Arenabeast', 'SeaMonster', 'CloudGiant',
               'Minion', 'MummyKing', 'Innoruuk', 'Zek', None, None)
    for k in range(90):
        c = gen.case('loot-%d' % k)
        cls = rng.choice(classes)
        for slot in range(rng.choice((1, 2, 3))):
            c.spawn_player(slot, (rng.uniform(-100, 100), 50.0 * slot, 0.0))
        cr = c.spawn_skelight((0.0, 0.0, 0.0), cflags=rng.choice((0x100000, 0x100001, 0x100000 | (1 << 32))))
        c.write(cr + 0x44, 4, rng.getrandbits(32))
        if rng.random() < 0.3:
            c.write(cr + 0xA0, 1, 6)
        if rng.random() < 0.3:
            c.write(cr + 0x3D8, 4, 1)
        c.loot_class(cls)
        pos = ftuple(gen, (rng.uniform(-10, 10), rng.uniform(-10, 10), 0.0))
        nums = ftuple(gen, (36.0, 2.0, 1.0, 0.6, 2.0, 0.25))
        angle = rng.randrange(0x10000)
        filt = rng.choice((0, 7))
        c.call('drop_loot', [cr, *pos, angle, filt, *nums],
               lambda: c.rt.model.drop_loot(cr, pos, angle, filt, *nums))
        c.end()
    for k in range(60):
        c = gen.case('goblin-%d' % k)
        c.spawn_player(0, (200.0, 0.0, 0.0))
        cr = c.spawn_skelight((0.0, 0.0, 0.0), cflags=rng.choice((0x100000, 0x100200, 0x100000 | (1 << 32))))
        for j in range(rng.choice((0, 2, 3, 4))):
            d = c.spawn_skelight((rng.uniform(-60, 60), rng.uniform(-60, 60), 0.0))
            c.write(d + 0xC, 4, c.rt.u32(d + 0xC) | rng.choice((8, 8, 0x48)))
        c.write(cr + 0x44, 4, rng.getrandbits(32))
        if rng.random() < 0.4:
            c.write(cr + 0x3D4, 4, 4)
        if rng.random() < 0.2:
            c.write(cr + 0xC, 4, c.rt.u32(cr + 0xC) | 2)
        if rng.random() < 0.2:
            c.write(cr + 0x314, 2, 5)
        c.loot_class(rng.choice((None, 'Zek')))
        state = rng.choice((0x14, 0x14, 0x14, 3, 0x18))
        c.call('goblin', [cr, state], lambda: c.rt.model.goblin_set_state(cr, state))
        c.end()


# ---------------------------------------------------------------------------------------
# Victim finders and radius damage (ref_combat_spells.QueryModel)
# ---------------------------------------------------------------------------------------
def queries(gen):
    rng = gen.rng
    STACK = 0x8000
    for k in range(160):
        c = gen.case('query-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.randrange(0x10000))
        objs = []
        for j in range(rng.randrange(1, 6)):
            pos = (rng.uniform(-150, 150), rng.uniform(-150, 150), rng.choice((0.0, 0.0, 40.0)))
            objs.append(c.spawn_skelight(pos, team=rng.choice((1, 2, 0xFF)), hp=rng.choice((100.0, 3.0))))
        for o in objs:
            if rng.random() < 0.15:
                c.write(o + 0xC, 4, c.rt.u32(o + 0xC) | rng.choice((8, 1, 4)))
            if rng.random() < 0.1:
                c.write(o + 0x312, 2, 9)
        c.write(0x404464, 4, rng.getrandbits(32))
        which = k % 3
        pos = ftuple(gen, (rng.uniform(-20, 20), rng.uniform(-20, 20), 0.0))
        if which == 0:
            facing = rng.randrange(0x10000)
            filt = rng.choice((0, P, objs[0]))
            radius = f32(gen, rng.choice((60.0, 160.0, 360.0)))
            arc = rng.choice((0xBF800000, f32(gen, 75.0), f32(gen, 30.0), f32(gen, 180.0)))
            width = f32(gen, rng.choice((0.0, 0.0, 20.0)))
            c.call('arc_q', [*pos, facing, STACK, filt, radius, arc, width],
                   lambda: c.rt.model.collect_in_arc(pos, facing, STACK, filt, radius, arc, width))
        elif which == 1:
            radius = f32(gen, rng.choice((30.0, 100.0, 300.0)))
            c.call('sphere_q', [*pos, STACK, radius],
                   lambda: c.rt.model.collect_in_sphere(pos, 0, STACK, radius))
        else:
            attacker = rng.choice((P, objs[0], 0))
            most = rng.choice((10, 50))
            flags = rng.choice((0x40860, 8))
            least = rng.choice((0, 5))
            skip = rng.choice((0, 8))
            inner = f32(gen, rng.choice((0.0, 40.0, 99.5)))
            outer = f32(gen, rng.choice((100.0, 200.0)))
            c.call('radius_dmg', [attacker, *pos, most, flags, least, skip, inner, outer],
                   lambda: c.rt.model.damage_in_radius(attacker, pos, most, flags, least, skip, inner, outer))
        c.end()


# ---------------------------------------------------------------------------------------
# Projectiles in the world (ref_combat_ranged)
# ---------------------------------------------------------------------------------------
def ranged(gen):
    rng = gen.rng
    for k in range(120):
        step = (rng.uniform(80, 300), rng.choice((10.0, 60.0))) if k % 6 == 0 else None
        c = gen.case('missile-%d' % k, step=step)
        shooter = c.spawn_player(0, (0.0, 0.0, 0.0)) if k % 2 == 0 else c.spawn_skelight((0.0, 0.0, 0.0), team=1)
        victims = []
        for j in range(rng.choice((0, 1, 2, 2, 3))):
            pos = (rng.uniform(40, 250), rng.choice((0.0, rng.uniform(-15, 15))), 0.0)
            if rng.random() < 0.2 and k % 2 == 1:
                victims.append(c.spawn_player(j, pos))
            else:
                victims.append(c.spawn_skelight(pos, team=rng.choice((2, 1, 0xFF)), hp=rng.choice((100.0, 3.0))))
        for v in victims:
            if rng.random() < 0.15:
                c.write(v + 0xC, 4, c.rt.u32(v + 0xC) | rng.choice((0x1000000, 0x200, 1)))
        item = 0
        if rng.random() < 0.3:
            c.write(0x00090000, 4, 0x00090100)
            c.write(0x00090100 + 0x1E, 1, rng.choice((0, 6)))
            for slot in range(4):
                c.write(0x00090000 + 0x24 + 8 * slot, 2, rng.choice((0xFFFF, 0x15, 0x18)))
                c.write(0x00090000 + 0x26 + 8 * slot, 2, rng.choice((0, 3)))
            item = 0x00090000
        speed = rng.choice((600.0, 1000.0, 1500.0))
        angle = rng.uniform(-0.2, 0.2)
        vel = (speed, speed * angle, rng.choice((0.0, 0.0, -100.0)))
        kind = rng.choice((0, 0, 0, 3, 1))
        a = c.missile(shooter, (20.0, 0.0, rng.choice((36.0, 10.0))), vel, facing=rng.randrange(0x10000),
                      dmin=rng.randrange(1, 10), dmax=rng.randrange(10, 20), attack=rng.choice((100, 100000)),
                      dflags=rng.choice((4, 0x40004, 0x80004, 0x100004, 0x200004)), item=item, kind=kind, launch=rng.choice((0, 0x1234)),
                      impact=rng.choice((0, 0x5678)))
        destroy_owner = rng.random() < 0.1 and k % 2 == 1
        for t in range(30):
            if destroy_owner and t == 2 and shooter in c.rt.world.objects:
                c.op('DESTROY', shooter, fn=lambda: c.rt._slot1(c.rt.world.obj(shooter), 3))
            c.tick()
            if a not in c.rt.world.objects:
                break
        c.end()
    for k in range(40):
        c = gen.case('thrown-%d' % k, step=(rng.uniform(80, 200), 30.0) if k % 4 == 0 else None)
        P = c.spawn_player(0, (0.0, 0.0, 0.0))
        for j in range(rng.choice((0, 1, 2))):
            c.spawn_skelight((rng.uniform(40, 200), rng.uniform(-10, 10), 0.0), team=2)
        kind = rng.choice((4, 5, 6, 0, 0, 7, 1))
        c.thrown(P, (20.0, 0.0, 36.0), (rng.choice((300.0, 600.0)), 0.0, rng.choice((0.0, 200.0))),
                 facing=rng.randrange(0x10000), dmin=2, dmax=6, kind=kind)
        for t in range(40):
            c.tick()
        c.end()


# ---------------------------------------------------------------------------------------
# Spells in the world (ref_combat_spells.SpellRuntime composition)
# ---------------------------------------------------------------------------------------
def spells(gen):
    rng = gen.rng
    for k in range(150):
        c = gen.case('spell-%d' % k)
        sid = (16, 57, 95)[k % 3]
        caster_player = rng.random() < 0.75
        if caster_player:
            caster = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.randrange(0x10000))
        else:
            caster = c.spawn_skelight((0.0, 0.0, 0.0), team=1, facing=rng.randrange(0x10000))
            if rng.random() < 0.5:
                c.spawn_player(0, (rng.uniform(-200, 200), rng.uniform(-200, 200), 0.0))
        spread = rng.choice((40.0, 90.0, 150.0))
        for j in range(rng.randrange(1, 5)):
            pos = (rng.uniform(-spread, spread), rng.uniform(-spread, spread), 0.0)
            c.spawn_skelight(pos, team=rng.choice((2, 1, 0xFF)), hp=rng.choice((100.0, 15.0, 3.0)),
                             facing=rng.randrange(0x10000))
        if sid == 16:
            stats = {0: (1, rng.randrange(5, 30), 1.0), 1: (1, rng.randrange(5, 30), 0.0),
                     2: (1, rng.randrange(1, 40), 0.0)}
        elif sid == 57:
            stats = {0: (1, rng.randrange(5, 20), 1.0), 1: (1, rng.randrange(0, 10), 0.0),
                     2: (1, rng.randrange(1, 6), 0.0), 3: (1, rng.choice((200, 300, 600)), 0.0)}
        else:
            stats = {0: (1, rng.randrange(3, 12), 0.0), 1: (1, rng.randrange(5, 30), 0.0)}
        c.skill(sid, stats)
        c.write(0x404464, 4, rng.getrandbits(32))
        level = rng.choice((1, 2, 3, 5))
        point = None if rng.random() < 0.7 else (rng.uniform(-30, 30), rng.uniform(-30, 30), 0.0)
        facing = None if rng.random() < 0.7 else rng.randrange(0x10000)
        c.cast(caster, sid, level, point, facing)
        for t in range(rng.choice((40, 90, 160))):
            if k % 7 == 0 and t == 20 and caster_player:
                c.cast(caster, sid, level)
            c.tick()
        c.end()


# ---------------------------------------------------------------------------------------
# The real player tick (ref_combat_ptick.TickRuntime, in the full world)
# ---------------------------------------------------------------------------------------
def ticks(gen):
    rng = gen.rng
    for k in range(90):
        planes = PLANES_INSIDE if k % 4 == 0 else None
        c = gen.case('tick-%d' % k, planes=planes)
        P = c.spawn_player(0, (0.0, 0.0, rng.choice((0.0, 0.0, 20.0))), facing=rng.randrange(0x10000),
                           strength=rng.choice((20, 60)), idle_voice_limit=rng.choice((0x7FFF, 20, 45)),
                           speed_mul=rng.choice((1.0, 1.0, 0.5)))
        if planes is not None:
            c.spawn_player(1, (0.0, 100.0, 0.0), idle_voice_limit=rng.choice((0x7FFF, 30)))
        cls = rng.choice((0, 1, 2, 3, 5, 7, 4, 6))
        if rng.random() < 0.5 and cls not in (4, 6):
            c.weapon(0, dmg=(rng.randrange(5, 20), rng.randrange(20, 40)), attack=rng.randrange(10, 80),
                     dmg_type=rng.choice((1, 8)), cls=cls)
        else:
            c.write(G_PLAYERS + 0x4A86, 1, cls)
        monsters = []
        for j in range(rng.choice((1, 1, 2))):
            m = c.spawn_skelight((rng.uniform(25, 90), rng.uniform(-30, 30), 0.0), facing=0x8000,
                                 hp=rng.choice((40.0, 120.0)), attack=rng.choice((100, 100000)))
            c.swing_clips(m, length=rng.choice((20, 30)), hit=rng.choice((8, 10)))
            monsters.append(m)
        if rng.random() < 0.3:
            c.root(P, rng.uniform(-1, 1), rng.uniform(-1, 1))
        if rng.random() < 0.2:
            c.yaw(rng.randrange(-0x8000, 0x8000))
        press = rng.choice((6, 9, 12, 15, 20))
        walk = rng.random() < 0.5
        for t in range(rng.choice((60, 100, 140))):
            if walk:
                c.stick(0, rng.choice((1.0, 0.0, 0.6)) if t < 30 else 0.0, rng.choice((0.0, 0.4)) if t < 30 else 0.0)
            if t >= 10 and t % press == 0:
                c.attack(0, held=rng.random() < 0.7)
            if t == 50 and rng.random() < 0.2:
                c.write(P + 0x58C, 4, 8)
            c.tick()
        c.end()

    # a player holding the block button while Skelights swing at it
    for k in range(40):
        c = gen.case('block-tick-%d' % k)
        P = c.spawn_player(0, (0.0, 0.0, 0.0), facing=rng.choice((0, 0x8000, 0x4000)))
        if rng.random() < 0.5:
            c.weapon(0, dmg=(5, 10), attack=20, cls=rng.choice((0, 1, 2, 3)))
        for j in range(rng.choice((1, 2))):
            m = c.spawn_skelight((rng.choice((28.0, -28.0)), rng.uniform(-10, 10), 0.0),
                                 facing=rng.choice((0, 0x8000)), hp=200.0, attack=100000)
            c.swing_clips(m, length=20, hit=8)
        mask = 0x20
        c.write(0x41B9D0, 4, mask)
        held = rng.random() < 0.8
        for t in range(rng.choice((60, 100))):
            if t == 5:
                c.write(G_PADS + 0xC, 4, mask)
                c.write(G_PADS + 8, 4, mask if held else 0)
            if t == 40 and rng.random() < 0.5:
                c.write(G_PADS + 8, 4, 0)
            c.tick()
        c.end()


# ---------------------------------------------------------------------------------------
# A Skelight away from home walks back (state 1, Creature_moveToDest)
# ---------------------------------------------------------------------------------------
def homes(gen):
    rng = gen.rng
    for k in range(30):
        c = gen.case('home-%d' % k)
        s = c.spawn_skelight((0.0, 0.0, 0.0), home_range=rng.choice((100.0, 300.0)), facing=rng.randrange(0x10000))
        c.write(s + 0x10, 4, f32(gen, rng.uniform(-600, 600)))
        c.write(s + 0x14, 4, f32(gen, rng.uniform(-600, 600)))
        c.op('GRID', s, fn=lambda: c.rt.world.update_grid_cell(c.rt.world.obj(s)))
        c.write(s + 0x44, 4, rng.getrandbits(32))
        if rng.random() < 0.3:
            c.spawn_player(0, (rng.uniform(-400, 400), rng.uniform(-400, 400), 0.0))
        for t in range(rng.choice((40, 120))):
            c.tick()
        c.end()
