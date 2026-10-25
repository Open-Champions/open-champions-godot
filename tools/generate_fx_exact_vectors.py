#!/usr/bin/env python3
"""Generate native/tests/vectors/fx_exact.tsv (+ fx_exact.json) for champions-fx-exact.

Runs the pinned research references (tools/ref_particles.py, ref_decals.py,
ref_fx_props.py, ref_lighting.py at the commit in fx2_research.py, extracted with SHA-256
checks) on seeded random SYNTHETIC inputs. No game data is read or written: definitions,
emitter records, noise tables and positions are all random.

    python3 -I tools/generate_fx_exact_vectors.py [--research PATH] [--out DIR]
    python3 -I tools/generate_fx_exact_vectors.py --dump PROPERTY CASE   (print state lines)

Row format (tab separated, one case per row):
    property  case  input fields...  digest
Inputs are decimal integers, IEEE patterns in hex (16 digits for binary64, 8 for
binary32) and ';'-separated operation lists. Every row ends with the first 16 hex digits
of the SHA-256 of the state lines the reference produced for it (each line ends with a
newline); native/tests/fx_exact_tests.cpp rebuilds the same lines from the C++ port and
compares. --dump prints the lines of one case for diffing against the C++ dump
(champions-fx-exact-tests DIR --dump PROPERTY CASE).
"""
import argparse
import hashlib
import json
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fx2_research  # noqa: E402

ROOT = os.path.dirname(HERE)
SEED = 20261018
M32 = 0xFFFFFFFF


def dbits(x):
    return struct.unpack('<Q', struct.pack('<d', x))[0]


def dhex(x):
    return '%016x' % dbits(x)


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def fhex_pattern(p):
    return '%08x' % (p & M32)


def bits_d(h):
    return struct.unpack('<d', struct.pack('<Q', int(h, 16)))[0]


class Lines:
    def __init__(self):
        self.lines = []

    def add(self, tag, values):
        self.lines.append(tag + ' ' + ','.join(str(int(v)) for v in values))

    def digest(self):
        h = hashlib.sha256()
        for line in self.lines:
            h.update(line.encode('ascii') + b'\n')
        return h.hexdigest()[:16]


def lcg_next(state):
    return (state * 0x19660D + 0x3C6EF35F) & M32


def noise_table(seed):
    """A synthetic g_noiseTable: 256 s16 values from the engine LCG."""
    t, s = [], seed & M32
    for _ in range(256):
        s = lcg_next(s)
        v = s >> 16
        t.append(v - 0x10000 if v & 0x8000 else v)
    return t


# ---------------------------------------------------------------------------------------
# particles
# ---------------------------------------------------------------------------------------

def random_def_args(rng, kind=None):
    def colour():
        return rng.choice([rng.randint(0, 255), rng.randint(-120, 400), 0, 255, 0x80])
    return {'s0': rng.choice([rng.uniform(0, 4), rng.uniform(0, 40), 1.0, 0.0]),
            's1': rng.choice([rng.uniform(0, 4), rng.uniform(0, 40), 1.0, 0.0]),
            'grav': rng.choice([0.0, rng.uniform(-60, 60), float(rng.randint(-40, 40))]),
            'start': [colour() for _ in range(4)], 'end': [colour() for _ in range(4)],
            'life': rng.choice([0, 1, 2, 3, rng.randint(-3, 7)]),
            'kind': rng.choice([0, 1, 1, 2, 3, rng.randint(-2, 6)]) if kind is None else kind,
            'tile': rng.randint(0, 255)}


def def_fields(a):
    return [dhex(a['s0']), dhex(a['s1']), dhex(a['grav']), ','.join(map(str, a['start'])),
            ','.join(map(str, a['end'])), str(a['life']), str(a['kind']), str(a['tile'])]


def def_token(a):
    return ':'.join(def_fields(a))


def build(P, a, previous=None):
    return P.ParticleDef.build(a['s0'], a['s1'], a['grav'], a['start'], a['end'], a['life'],
                               a['kind'], a['tile'], previous=previous)


def record_words(d):
    return list(struct.unpack('<16I', d.record()))


def def_lines(P, d, out):
    out.add('D', [d.list_index, d.slot, d.live, d.callback, d.kind, d.life_class, d.tile, d.life]
            + record_words(d))
    for age in (0, 1, 7, d.life // 2, d.life - 1, d.life, d.life + 1, 100, -3):
        out.add('A', [age] + list(d.colour_at(age)) + [dbits(d.size_at(age))])


def gen_rz(P, rng, rows):
    specials = [0.0, -0.0, 1.0, -1.0, 1.0 / 3.0, -1.0 / 3.0, 1e-40, -1e-40, 2.0 ** -126,
                -(2.0 ** -126), 2.0 ** -126 * 0.999999, 2.0 ** -127, 3.4028234663852886e38,
                3.5e38, -3.5e38, 1e300, float('inf'), float('-inf'), float('nan'), 16777217.0,
                0.1, 60.0 * (17 / 60.0)]
    values = list(specials)
    while len(values) < 3000:
        e = rng.randint(-160, 140)
        values.append(rng.choice([-1, 1]) * rng.random() * 2.0 ** e)
    for i, x in enumerate(values):
        out = Lines()
        r = P.rz(x)
        f = P.f32(x) if (x == x and abs(x) < 3.4e38) else 0.0
        out.add('R', [dbits(r), dbits(f)])
        rows.append(['rz', str(i), dhex(x), out.digest()])


def gen_definitions(P, rng, rows):
    n = 0
    for i in range(3000):
        a = random_def_args(rng)
        prev = None
        prev_fields = '-'
        if a['kind'] == 2 and rng.random() < 0.5:
            pa = random_def_args(rng, kind=rng.choice([0, 1, 3]))
            prev = build(P, pa)
            prev_fields = '%d,%d,%d' % (prev.v0, prev.u1, prev.v1)
        d = build(P, a, prev)
        out = Lines()
        def_lines(P, d, out)
        rows.append(['def', str(i)] + def_fields(a) + [prev_fields, out.digest()])
        n += 1
    d = P.ParticleDef.reset_default()
    out = Lines()
    def_lines(P, d, out)
    rows.append(['defreset', '0', out.digest()])


def gen_spawns(P, rng, rows):
    for i in range(2000):
        a = random_def_args(rng, kind=rng.choice([0, 1, 2, 3]))
        d = build(P, a)
        clock = rng.randint(-0x8000, 0x7FFF)
        pos = tuple(P.f32(rng.choice([rng.uniform(-3000, 3000), rng.uniform(-40000, 40000),
                                      float(rng.randint(-100, 100)) + 0.5])) for _ in range(3))
        vel = tuple(P.f32(rng.choice([rng.uniform(-30, 30), rng.uniform(-200, 200), 0.0]))
                    for _ in range(3))
        if rng.random() < 0.3:      # binary64 inputs that are not singles
            pos = tuple(rng.uniform(-3000, 3000) for _ in range(3))
            vel = tuple(rng.uniform(-30, 30) for _ in range(3))
        back = rng.choice([0.0, 0.0, rng.uniform(0, 2), -rng.uniform(0, 1), rng.randint(1, 90) / 60.0])
        slot = rng.randint(0, 31)
        p = P.spawn_record(d, pos, vel, clock, back, slot)
        out = Lines()
        out.add('S', list(p))
        for age in (0, 5, d.life, d.life + 1):
            v = P.evaluate(p, d, p.birth + age)
            out.add('V', [age] + [dbits(c) for c in v.pos] + [dbits(v.size)] + list(v.rgba) + [v.age]
                    + [int(P.expired(p, d, p.birth + age))])
        if d.kind == P.KIND_LINE:
            tail, head = P.line_endpoints(p, d, p.birth + 7)
            out.add('L', [dbits(c) for c in tail + head])
        size, w = rng.uniform(0, 400), rng.uniform(1, 4000)
        out.add('X', [dbits(c) for c in P.screen_half_extent(size, w)])
        rows.append(['spawn', str(i), def_token(a), str(clock), ','.join(dhex(c) for c in pos),
                     ','.join(dhex(c) for c in vel), dhex(back), str(slot), dhex(size), dhex(w),
                     out.digest()])


def pool_state(P, m, defs, out, full):
    cur = -1
    for i, lists in enumerate(m.group_lists):
        if m.current is lists:
            cur = i
    v = [m.clock, m.dropped, m.group_seq, cur, len(m.free_pages)] + list(m.free_pages)
    v += [1 if x is not None else 0 for x in m.slots]
    for d in defs:
        v += [d.slot, d.live]
    for lists in [m.global_set] + m.group_lists:
        for lst in lists:
            v += [len(lst.pages)] + lst.pages + [lst.head, lst.tail]
    v.append(len(m.groups))
    for h, (lists, mat, idle) in m.groups.items():
        v += [h, idle, m.group_lists.index(lists)] + [fbits(x) for row in mat for x in row]
    out.add('P', v)
    if not full:
        return
    c = []
    for lists in [m.global_set] + m.group_lists:
        for lst in lists:
            if not lst.pages:
                continue
            pages, pi, cc = lst.pages, 0, lst.head
            while not (pi == len(pages) - 1 and cc == lst.tail):
                p, _ = lst.cells[(pages[pi], cc)]
                c += [pages[pi], cc] + list(p)
                cc += 1
                if cc & 0x1F == 0x1F:
                    if cc == 0xFF and pi != len(pages) - 1:
                        pi, cc = pi + 1, 1
                    else:
                        cc += 2
    out.add('C', c)
    vis = []
    for p, view in m.visible():
        vis += list(p) + [dbits(x) for x in view.pos] + [dbits(view.size)] + list(view.rgba) + [view.age]
    out.add('W', vis)


def gen_pools(P, rng, rows, stats):
    for case in range(42):
        slotty = rng.random() < 0.3          # many definitions, small bursts: slot pressure
        nd = 40 if slotty else rng.choice([6, 12, 40])
        args = []
        for _ in range(nd):
            a = random_def_args(rng, kind=rng.choice([0, 1, 2, 3]))
            a['life'] = rng.choice([0, 0, 1, 2, 3])
            args.append(a)
        defs = [build(P, a) for a in args]
        m = P.ParticleSystem()
        start = 0
        if rng.random() < 0.3:
            start = 0x7FFF - rng.randint(0, 120)
        m.clock = start
        out = Lines()
        handles, current, ops = [], None, []
        steps = rng.choice([150, 400, 900])
        for step in range(steps):
            r = rng.random()
            if r < 0.55:
                i = rng.randrange(nd)
                pos = tuple(float(rng.randint(-2000, 2000)) for _ in range(3))
                vel = tuple(float(rng.randint(-60, 60)) for _ in range(3))
                burst = rng.choice([1, 2, 5] if slotty else [1, 1, 1, 5, 40, 300, 300, 4000])
                back = rng.choice([0.0] * 6 + [rng.randint(1, 90) / 60.0, rng.uniform(0, 2)])
                for _ in range(burst):
                    stats['pool spawns'] = stats.get('pool spawns', 0) + 1
                    d = defs[i]
                    no_slot = d.live == 0 and None not in m.slots
                    if m.spawn(d, pos, vel, back) is None:
                        k = 'pool drops, no free slot' if no_slot else 'pool drops, no free page'
                        stats[k] = stats.get(k, 0) + 1
                    elif m.current is not m.global_set and not any(m.current is g[0] for g in m.groups.values()):
                        stats['pool spawns into a freed group'] = stats.get('pool spawns into a freed group', 0) + 1
                ops.append('s%d,%d,%s,%s,%s' % (i, burst, ','.join('%d' % c for c in pos),
                                                ','.join('%d' % c for c in vel),
                                                dhex(back) if back else '0'))
            elif r < 0.85:
                ticks = rng.choice([1, 1, 2] if slotty else [1, 1, 2, 12, 31, 61, 91])
                cut = 1 if rng.random() < 0.05 else 0
                for _ in range(ticks):
                    if m.clock == 0x7FFF:
                        stats['pool clock wraps'] = stats.get('pool clock wraps', 0) + 1
                    m.update(in_cutscene=bool(cut))
                ops.append('u%d,%d' % (ticks, cut))
            elif r < 0.90:
                h = m.group_create()
                if h:
                    handles.append(h)
                ops.append('c')
            elif r < 0.94 and handles:
                h = rng.choice(handles + [rng.randint(1, 0xFFFF)])
                m.group_begin(h)
                current = h
                ops.append('b%d' % h)
            elif r < 0.97 and current is not None:
                m.group_end(current)
                ops.append('e%d' % current)
                current = None
            elif r < 0.985 and handles:
                h = rng.choice(handles)
                mat = [P.f32(rng.uniform(-2, 2)) for _ in range(12)]
                m.group_set_matrix(h, [mat[3 * k:3 * k + 3] for k in range(4)])
                ops.append('m%d,%s' % (h, ','.join('%08x' % fbits(x) for x in mat)))
            elif handles:
                h = rng.choice(handles)
                m.group_free(h)
                ops.append('f%d' % h)
            else:
                continue
            pool_state(P, m, defs, out, full=(len(ops) % 64 == 0))
        pool_state(P, m, defs, out, full=True)
        stats['pool operations'] = stats.get('pool operations', 0) + len(ops)
        rows.append(['pool', str(case), str(start), ';'.join(def_token(a) for a in args), ';'.join(ops),
                     out.digest()])


def emitter_record_random(P, rng):
    rec = P.EmitterRecord.new(
        start_rgba=tuple(rng.randint(0, 255) for _ in range(4)),
        end_rgba=tuple(rng.randint(0, 255) for _ in range(4)),
        size_start=rng.randint(-50, 300), size_end=rng.randint(-50, 300),
        gravity=rng.randint(-2000, 2000), life_class=rng.randint(0, 6), kind=rng.randint(0, 5),
        tile=rng.randint(0, 255),
        rate=rng.choice([1, 2, 5, -1, -3, -7, 0, rng.randint(-20, 20)]),
        off_x=rng.choice([0, rng.randint(-200, 200)]), off_y=rng.choice([0, rng.randint(-200, 200)]),
        off_z=rng.choice([0, rng.randint(-200, 200)]),
        vel=tuple(rng.choice([0, rng.randint(-60, 60)]) for _ in range(3)),
        yaw=rng.randint(0, 0xFFFF), pitch=rng.choice([0, rng.randint(0, 0xFFFF)]),
        box=tuple(rng.choice([0, rng.randint(0, 400)]) for _ in range(3)),
        spread=rng.choice([0, rng.randint(-40, 40)]), cone=rng.choice([0, rng.randint(-0x4000, 0x4000)]),
        on_time=rng.choice([0, 0, rng.randint(1, 200)]), off_time=rng.randint(0, 200),
        flags=rng.choice([0, 0, 1, 2, 3]), next=rng.choice([0, rng.randint(-50, 500)]),
        on=rng.choice([0, 1]), name=rng.choice(['fx', '', 'smoke', 'abcdefghij']))
    return rec


def gen_emitter_definitions(P, rng, rows):
    for i in range(1000):
        r = emitter_record_random(P, rng)
        b = r.pack()
        out = Lines()
        back = P.EmitterRecord.parse(b)
        out.add('E', list(back.pack()) + [len(back.name)])
        d = P.ParticleDef.from_emitter_record(b)
        def_lines(P, d, out)
        other = P.EmitterRecord.parse(b)
        k = rng.randrange(4)
        if k == 0:
            other.rate = rng.randint(-128, 127)
        elif k == 1:
            other.tile = (other.tile + 1) & 0xFF
        elif k == 2:
            other.start_rgba = (other.start_rgba[0] ^ 1,) + tuple(other.start_rgba[1:])
        else:
            other.name = 'other'
        out.add('Q', [int(r.same_definition(other))] + list(other.pack()))
        rows.append(['emitdef', str(i), b.hex(), str(k), str(other.rate if k == 0 else 0), out.digest()])


def gen_flipbooks(P, rng, rows):
    for i in range(600):
        a = random_def_args(rng, kind=rng.choice([0, 1, 2]))
        a['tile'] = rng.choice([rng.randint(0, 255), 0x10 + rng.randint(0, 15), 0x20 + rng.randint(0, 15),
                                0x40 + rng.randint(0, 15)])
        d = build(P, a)
        frames = [rng.choice([rng.randint(0, 100000), rng.randint(-50, 50)]) for _ in range(rng.choice([1, 5, 40]))]
        out = Lines()
        for f in frames:
            s = P.flipbook_step(d, f)
            out.add('F', [int(s), d.u0 if d.u0 is not None else -1, d.v0, d.u1, d.v1])
        rows.append(['flipbook', str(i), def_token(a), ','.join(map(str, frames)), out.digest()])


def gen_emitters(P, rng, rows, stats):
    for i in range(2000):
        rec = emitter_record_random(P, rng)
        b = rec.pack()
        pos = tuple(rng.uniform(-3000, 3000) for _ in range(3))
        now = rng.choice([rng.randint(0, 100000), rng.randint(-100, 100)])
        nows = []
        for _ in range(rng.choice([1, 4, 12])):
            nows.append(now)
            now += rng.choice([0, 1, 1, 1, 2, 7, rng.randint(0, 120)])
        yaw = rng.choice([0, rng.randint(-0x8000, 0x7FFF)])
        flips = tuple(int(rng.random() < 0.2) for _ in range(3))
        wa, wo = rng.randint(0, 0xFFFF), rng.choice([0, 1])
        seed = rng.getrandbits(32)
        crng = P.CosmeticRng(seed)
        out = Lines()
        for t in nows:
            on = rec.on
            got = P.emitter_run(rec, pos, t, crng, yaw=yaw, neg_x=bool(flips[0]), neg_y=bool(flips[1]),
                                swap_xy=bool(flips[2]), weather_angle=wa, weather_on=bool(wo))
            out.add('T', [rec.next, rec.on, crng.seed, len(got)])
            for p, v in got:
                out.add('O', [dbits(c) for c in tuple(p) + tuple(v)])
            stats['emitter runs'] = stats.get('emitter runs', 0) + 1
            stats['emitter spawns'] = stats.get('emitter spawns', 0) + len(got)
            stats['emitter on/off switches'] = stats.get('emitter on/off switches', 0) + int(on != rec.on)
        rows.append(['emitter', str(i), b.hex(), ','.join(dhex(c) for c in pos), ','.join(map(str, nows)),
                     str(yaw), ','.join(map(str, flips)), str(wa), str(wo), str(seed), out.digest()])


# ---------------------------------------------------------------------------------------
# decals
# ---------------------------------------------------------------------------------------

def decal_state(C, D, st, made, out):
    v = [int(made), st.total, len(st.free)] + list(st.free)
    cells = sorted(st.cells.items(), key=lambda kv: (kv[0][1], kv[0][0]))
    v.append(len(cells))
    for (cx, cy), b in cells:
        v += [cx, cy, b.index, len(b.decals), b.zmin, b.zmax, b.counted]
        for d in b.decals:
            v += list(struct.unpack('<12h', d.record()))
    for i, gr in enumerate(st.growers):
        if gr is not None:
            blk = next(bb for bb in st.cells.values() if gr.decal in bb.decals)
            v += [i, gr.mode, blk.index, blk.decals.index(gr.decal), gr.angle, gr.size, gr.final]
    out.add('G', v)


def gen_decals(C, D, rng, rows, stats):
    for case in range(30):
        detail = rng.choice([0, 1, 2, 2])
        st = D.DecalStore(detail)
        spread = rng.choice([300.0, 1500.0, 5000.0])
        out = Lines()
        ops = []
        for step in range(rng.choice([100, 400, 700])):
            if rng.random() < 0.75:
                size = C.f32(rng.choice([12.0, rng.uniform(0, 80), float(rng.randint(1, 40))]))
                pos = tuple(C.f32(v) for v in (rng.uniform(-spread, spread), rng.uniform(-spread, spread),
                                               rng.uniform(-500, 500)))
                angle = rng.randint(0, 0xFFFF)
                kind = rng.choice([0, 1, 2, 3, 4, 6, 9, 10, rng.randint(-1, 12)])
                grow = rng.choice([0, 0, 0, 1, 2, 3])
                tint = rng.getrandbits(24)
                d = st.add(C.to_float(size), tuple(C.to_float(p) for p in pos), angle, kind, grow,
                           (tint & 0xFF, (tint >> 8) & 0xFF, (tint >> 16) & 0xFF))
                stats['decals made' if d is not None else 'decals dropped'] = \
                    stats.get('decals made' if d is not None else 'decals dropped', 0) + 1
                ops.append('a%08x,%08x,%08x,%08x,%d,%d,%d,%d' % (size, pos[0], pos[1], pos[2], angle, kind,
                                                                  grow, tint))
                decal_state(C, D, st, d is not None, out)
            else:
                frozen = 1 if rng.random() < 0.1 else 0
                st.grow_step(frozen=bool(frozen))
                stats['decal growth steps'] = stats.get('decal growth steps', 0) + 1
                ops.append('g%d' % frozen)
                decal_state(C, D, st, False, out)
        rows.append(['decals', str(case), str(detail), ';'.join(ops), out.digest()])
    # the gate and the grid on their own
    for i in range(400):
        k = rng.choice([rng.randint(-5, 15), rng.randint(-2 ** 40, 2 ** 40)])
        det = rng.randint(-1, 3)
        x = rng.choice([rng.uniform(-4000, 4000), float(rng.randint(-3842, 3842)), -3840.0, 3839.9])
        y = rng.choice([rng.uniform(-4000, 4000), float(rng.randint(-3842, 3842))])
        c = D.cell_of(x, y)
        out = Lines()
        out.add('K', [int(D.allowed(k, det))] + ([-1, -1] if c is None else list(c)))
        rows.append(['decalgate', str(i), str(k), str(det), dhex(x), dhex(y), out.digest()])


# ---------------------------------------------------------------------------------------
# fire props
# ---------------------------------------------------------------------------------------

def gen_props(C, F, rng, rows, stats):
    classes = [F.TORCH, F.TORCH, F.CANDLE, F.CANDLE3, F.LANTERN, F.LAMP]
    for case in range(400):
        cls = rng.choice(classes)
        tseed = rng.getrandbits(32)
        table = noise_table(tseed)
        name = rng.choice(['d_town', 'mine01', 'f_minefield', 'castle'])
        addr = rng.getrandbits(32) & ~0xF
        pos = [C.f32(rng.uniform(-5000, 5000)) for _ in range(3)]
        seed = rng.getrandbits(rng.choice([16, 32]))
        kind = rng.choice([rng.randint(0, 5), rng.randint(0, 5), rng.randint(6, 255)])
        big = rng.random() < 0.5
        model = F.FireProp(cls, addr, pos, seed, kind=kind, big=big, level_name=name)
        start_undrawn = rng.choice([0, 0, 25, 30, 127, 200])
        model.undrawn = start_undrawn
        frame = rng.choice([rng.randint(0, 200000), rng.randint(-100, 100)])
        ticks = rng.choice([1, 8, 40, 300])
        out = Lines()
        for _ in range(ticks):
            frame += 1
            t = F.RUN[cls](model, frame, table)
            v = [len(t.spawns)]
            for s in t.spawns:
                v += [s.backdate, s.definition] + list(s.position) + list(s.velocity)
            v += [int(t.stopped), model.rng.state, model.undrawn, model.model_radius, model.model_falloff,
                  model.level_radius, model.level_falloff] + list(model.light_pos) + [model.counter,
                                                                                    int(model.running)]
            out.add('T', v)
            stats['prop ticks'] = stats.get('prop ticks', 0) + 1
            stats['prop spawns'] = stats.get('prop spawns', 0) + len(t.spawns)
            stats['prop stops'] = stats.get('prop stops', 0) + int(t.stopped)
        rows.append(['props', str(case), cls, str(addr), ','.join(fhex_pattern(p) for p in pos), str(seed),
                     str(kind), str(int(big)), name, str(tseed), str(start_undrawn), str(frame - ticks),
                     str(ticks), out.digest()])


# ---------------------------------------------------------------------------------------
# lighting
# ---------------------------------------------------------------------------------------

def random_level(P, rng):
    d = [rng.uniform(-1, 1) for _ in range(3)]
    sun = [rng.randint(0, 255) for _ in range(4)]
    amb = [rng.randint(0, 255) for _ in range(4)]
    level = rng.choice([0x80, 0x40, 0x20])
    scale = P.f32(rng.choice([1.0, rng.uniform(0, 2), 0.5, 1.5]))
    layer = rng.choice([0, 0, 0, 1, 12])
    return d, sun, amb, level, scale, layer


def level_fields(lv_args):
    d, sun, amb, level, scale, layer = lv_args
    return [','.join(dhex(c) for c in d), ','.join(map(str, sun)), ','.join(map(str, amb)), str(level),
            dhex(scale), str(layer)]


def obj_line(P, obj, out):
    out.add('J', list(obj.ambient) + list(obj.sun) + list(obj.fill) + [fbits(x) for x in obj.point_scale])


def gen_object_light(P, L, rng, rows, stats):
    for case in range(1000):
        args = random_level(P, rng)
        d, sun, amb, level, scale, layer = args
        lv = L.LevelLight(d, sun, amb, level, scale, layer)
        out = Lines()
        s_sun, s_amb = lv.scaled()
        out.add('Y', list(s_sun) + list(s_amb) + [dbits(c) for c in lv.sun_dir] + [dbits(c) for c in lv.fill_dir()])
        obj = lv.object_state()
        obj_line(P, obj, out)
        ops = []
        for _ in range(rng.randint(1, 8)):
            r = rng.random()
            if r < 0.3:
                bias = [rng.randint(-200, 200) for _ in range(4)]
                obj.add_ambient(bias)
                ops.append('a' + ','.join(map(str, bias)))
            elif r < 0.45:
                rgba = [rng.randint(-50, 300) for _ in range(4)]
                obj.set_ambient(rgba)
                ops.append('s' + ','.join(map(str, rgba)))
            elif r < 0.6:
                k = [P.f32(rng.choice([1.0, 0.6, rng.uniform(0, 2)])) for _ in range(3)]
                obj.scale(*k)
                ops.append('k' + ','.join(dhex(x) for x in k))
            elif r < 0.7:
                f = rng.choice([rng.uniform(-0.5, 2.0), 0.5, 1.0])
                obj.chill(f)
                ops.append('c' + dhex(f))
            elif r < 0.8:
                f = rng.choice([rng.uniform(-0.5, 2.0), 0.5, 1.0])
                obj.poison(f)
                ops.append('p' + dhex(f))
            elif r < 0.85:
                obj.hit_flash()
                ops.append('h')
            else:
                obj = lv.object_state()
                ops.append('r')
            obj_line(P, obj, out)
        stats['object light operations'] = stats.get('object light operations', 0) + len(ops)
        rows.append(['objlight', str(case)] + level_fields(args) + [';'.join(ops), out.digest()])
    # markers
    for case in range(300):
        amb = None if rng.random() < 0.3 else tuple(float(rng.randint(-10, 600)) for _ in range(3))
        sun = None if rng.random() < 0.3 else tuple(rng.uniform(-10, 600) for _ in range(3))
        dpos = None if rng.random() < 0.3 else tuple(rng.uniform(-5, 5) for _ in range(3))
        flags = rng.choice([0, 0x04000000, 0x20000000, 0x24000000, rng.getrandbits(32)])
        lv = L.LevelLight.from_markers(amb, sun, dpos, flags)
        out = Lines()
        out.add('M', list(lv.ambient_set) + list(lv.sun_set) + [dbits(c) for c in lv.sun_dir] + [lv.level])
        enc = lambda t: '-' if t is None else ','.join(dhex(c) for c in t)
        rows.append(['markers', str(case), enc(amb), enc(sun), enc(dpos), str(flags), out.digest()])
    lv = L.LevelLight.BOOT
    out = Lines()
    out.add('M', list(lv.ambient_set) + list(lv.sun_set) + [dbits(c) for c in lv.sun_dir] + [lv.level])
    rows.append(['boot', '0', out.digest()])


def gen_light_slots(P, L, rng, rows, stats):
    for case in range(150):
        count = rng.choice([1, 4, 16, 20])
        models, toks = [], []
        for _ in range(count):
            low, high = rng.randint(0, 400), rng.randint(0, 400)
            mode = rng.choice([0, 1, 2, 3, 4, 5])
            period = rng.choice([0, 1, 5, 30, rng.randint(0, 200)])
            rgba = [rng.randint(-0x8000, 0x7FFF) for _ in range(4)]
            models.append(L.LightChunk(low, high, mode, period, rgba))
            toks.append('%d,%d,%d,%d,%s' % (low, high, mode, period, ','.join(map(str, rgba))))
        slots = L.LightSlots()
        frame, draw = rng.randint(0, 1000), rng.randint(1, 1000)
        seed = rng.getrandbits(32)
        crng = P.CosmeticRng(seed)
        out = Lines()
        steps = []
        for _ in range(rng.choice([30, 120, 400])):
            df = rng.choice([1, 1, 1, 0, 2])
            frame += df
            draw += 1
            mask = 0
            for k in range(count):
                if rng.random() < 0.7:
                    mask |= 1 << k
            for k in range(count):
                if mask >> k & 1:
                    lc = models[k]
                    s, node = slots.run(lc, frame, draw, crng.r)
                    v = [k, s, lc.timer, int(lc.down), lc.radius]
                    if node is None:
                        v.append(0)
                    else:
                        v += [1, dbits(node.model_radius), dbits(node.model_falloff), dbits(node.level_radius)]
                        v += list(node.model_rgb) + list(node.level_rgb)
                    out.add('H', v)
                    stats['light chunk runs'] = stats.get('light chunk runs', 0) + 1
            steps.append('%d,%x' % (df, mask))
        out.add('Z', [crng.seed] + [slots.slot_of.get(id(m), -1) for m in models] + list(slots.radius)
                + list(slots.stamp))
        rows.append(['lightslots', str(case), ';'.join(toks), str(frame - sum(int(s.split(',')[0]) for s in steps)),
                     str(draw - len(steps)), str(seed), ';'.join(steps), out.digest()])


def owner_positions(P, seed, n):
    s, out = seed & M32, []
    for _ in range(n):
        v = []
        for _ in range(3):
            s = lcg_next(s)
            v.append(P.f32((s >> 8) * (6000.0 / 16777216.0) - 3000.0))
        out.append(tuple(v))
    return out


def gen_light_effects(P, L, rng, rows, stats):
    f = P.f32
    for case in range(600):
        start = [f(rng.choice([rng.uniform(-300, 300), float(rng.randint(0, 255))])) for _ in range(4)]
        end = [f(rng.choice([rng.uniform(-300, 300), float(rng.randint(0, 255)), 0.0])) for _ in range(4)]
        r0 = f(rng.choice([rng.uniform(1, 400), float(rng.randint(1, 299))]))
        r1 = f(rng.choice([rng.uniform(1, 400), 0.0, r0]))
        floor = f(rng.choice([0.0, 1.0, rng.uniform(0, 1)]))
        secs = f(rng.choice([rng.uniform(0, 2), 0.0, 0.25, 1.0, 0.01, 0.5]))
        signed = rng.choice([0, 0, 1])
        has_owner = int(rng.random() < 0.4)
        pos = [f(rng.uniform(-3000, 3000)) for _ in range(3)]
        frame = rng.randint(0, 100000)
        oseed = rng.getrandbits(32)
        le = L.LightEffect(start, end, r0, r1, secs, frame, signed=bool(signed), floor=floor, pos=pos)
        out = Lines()
        out.add('B', [le.life, le.end] + [dbits(c) for c in le.c] + [dbits(c) for c in le.rate]
                + [dbits(le.radius), dbits(le.rrate)])
        owners = owner_positions(P, oseed, 400)
        fr = frame
        for step in range(400):
            fr += 1
            alive, node = le.run(fr, owners[step] if has_owner else None)
            out.add('N', [int(alive)] + list(node.model_rgb) + [dbits(node.model_radius), dbits(node.model_falloff)]
                    + [dbits(c) for c in node.pos] + [dbits(c) for c in le.c])
            stats['light effect frames'] = stats.get('light effect frames', 0) + 1
            if not alive:
                break
        rows.append(['lighteffect', str(case), ','.join(dhex(c) for c in start), ','.join(dhex(c) for c in end),
                     dhex(r0), dhex(r1), dhex(floor), dhex(secs), str(signed), str(has_owner),
                     ','.join(dhex(c) for c in pos), str(frame), str(oseed), out.digest()])


def gen_light_nodes(P, L, rng, rows, stats):
    for case in range(150):
        args = random_level(P, rng)
        d, sun, amb, level, scale, layer = args
        layer = rng.choice([0, 0, 1])
        lv = L.LevelLight(d, sun, amb, level, scale, layer)
        obj = lv.object_state()
        scale_tok = '-'
        if rng.random() < 0.3:
            k = [P.f32(rng.uniform(0, 2)) for _ in range(3)]
            obj.scale(*k)
            scale_tok = ','.join(dhex(x) for x in k)
        centre = [rng.uniform(-11000, 11000), rng.uniform(-11000, 11000), rng.uniform(-500, 500)]
        nodes, toks = [], []
        for _ in range(rng.choice([1, 3, 8, 20])):
            p = tuple(c + rng.choice([rng.uniform(-400, 400), float(rng.randint(-300, 300))]) for c in centre)
            mr = rng.choice([rng.uniform(1, 400), float(rng.randint(1, 300))])
            lr = rng.choice([mr, rng.uniform(1, 400)])
            mrgb = tuple(rng.randint(-50, 300) for _ in range(3))
            lrgb = tuple(rng.randint(-50, 300) for _ in range(3))
            fl = rng.choice([0.0, 1.0, rng.uniform(0, 1.5)])
            ly = rng.choice([0, 0, 1])
            tint = rng.choice([0, 1])
            nodes.append(L.LightNode(p, mrgb, lrgb, mr, lr, fl, ly, bool(tint)))
            toks.append('%s,%s,%s,%s,%s,%s,%d,%d' % (','.join(dhex(c) for c in p), dhex(mr), dhex(lr),
                                                    ','.join(map(str, mrgb)), ','.join(map(str, lrgb)),
                                                    dhex(fl), ly, tint))
        out = Lines()
        qs = []
        for _ in range(4):
            q = tuple(c + rng.uniform(-300, 300) for c in centre)
            nrm = tuple(rng.uniform(-1, 1) for _ in range(3))
            qs.append(q + nrm)
            got = L.collect_near(nodes, q, layer)
            out.add('U', [nodes.index(n) for n in got])
            lights, ambient = L.model_lights(q, nodes, obj)
            v = list(ambient)
            for lt in lights:
                v += [dbits(c) for c in lt.direction] + list(lt.rgb)
            out.add('I', v)
            out.add('O', list(L.shade_model_vertex(nrm, (lights, ambient))))
            out.add('V', list(L.shade_level_vertex(level, nodes, q, nrm)))
            out.add('T', list(L.decal_tint(q, nodes, lv)))
            stats['light node queries'] = stats.get('light node queries', 0) + 1
        rows.append(['lightnodes', str(case)] + level_fields((d, sun, amb, level, scale, layer))
                    + [scale_tok, ';'.join(toks), ';'.join(','.join(dhex(c) for c in q) for q in qs), out.digest()])


# ---------------------------------------------------------------------------------------

TRIG_BLOCK = 4096


def poly_cos_sin(core, angle):
    """math_cos16 / math_sin16 as Python floats (exact promotions of the float32 results)."""
    return core.to_float(core.cos16(angle)), core.to_float(core.sin16(angle))


def trig_block_digest(core, block):
    h = hashlib.sha256()
    for k in range(-32768 + block * TRIG_BLOCK, -32768 + (block + 1) * TRIG_BLOCK):
        c, s = poly_cos_sin(core, k)
        h.update(('C %d,%d,%d\n' % (k, dbits(c), dbits(s))).encode('ascii'))
    return h.hexdigest()[:16]


def gen_trig(core, rows):
    # Every 16-bit angle of the polynomial the emitter rows use.
    for b in range(65536 // TRIG_BLOCK):
        rows.append(['trig', str(b), trig_block_digest(core, b)])


def generate(research):
    with fx2_research.references(research):
        import ref_core as C
        import ref_particles as P
        # ref_particles._cs is the host libm. The original routine is math_cos16.
        # The vectors use that polynomial (ref_core.cos16, the same body as ref_trig).
        C.set_fpu()
        P._cs = lambda angle, core=C: poly_cos_sin(core, angle)
        import ref_decals as D
        import ref_fx_props as F
        import ref_lighting as L
        rows, stats = [], {}
        gen_rz(P, random.Random(SEED + 1), rows)
        gen_definitions(P, random.Random(SEED + 2), rows)
        gen_spawns(P, random.Random(SEED + 3), rows)
        gen_pools(P, random.Random(SEED + 4), rows, stats)
        gen_emitter_definitions(P, random.Random(SEED + 5), rows)
        gen_flipbooks(P, random.Random(SEED + 6), rows)
        gen_emitters(P, random.Random(SEED + 7), rows, stats)
        gen_decals(C, D, random.Random(SEED + 8), rows, stats)
        gen_props(C, F, random.Random(SEED + 9), rows, stats)
        gen_object_light(P, L, random.Random(SEED + 10), rows, stats)
        gen_light_slots(P, L, random.Random(SEED + 11), rows, stats)
        gen_light_effects(P, L, random.Random(SEED + 12), rows, stats)
        gen_light_nodes(P, L, random.Random(SEED + 13), rows, stats)
        C.set_fpu()
        gen_trig(C, rows)
        return rows, stats


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--research', default=str(fx2_research.DEFAULT_RESEARCH))
    ap.add_argument('--out', default=os.path.join(ROOT, 'native', 'tests', 'vectors'))
    ap.add_argument('--dump', nargs=2, metavar=('PROPERTY', 'CASE'))
    a = ap.parse_args(argv)
    if a.dump:
        global Lines
        base = Lines
        captured = []

        class Capture(base):
            def digest(self):
                captured.append(list(self.lines))
                return base.digest(self)
        Lines = Capture
        rows, _ = generate(a.research)
        for row, lines in zip(rows, captured):
            if (row[0], row[1]) == tuple(a.dump):
                print('\n'.join(lines))
                return 0
        print('no such case', file=sys.stderr)
        return 1
    rows, stats = generate(a.research)
    data = ''.join('\t'.join(r) + '\n' for r in rows).encode('ascii')
    os.makedirs(a.out, exist_ok=True)
    tsv = os.path.join(a.out, 'fx_exact.tsv')
    with open(tsv, 'wb') as h:
        h.write(data)
    counts = {}
    for r in rows:
        counts[r[0]] = counts.get(r[0], 0) + 1
    manifest = fx2_research.manifest(len(rows), data, os.path.abspath(__file__))
    manifest['properties'] = counts
    manifest['coverage'] = stats
    manifest['seed'] = SEED
    manifest['python'] = sys.version.split()[0]   # sum() of floats is compensated from 3.12 on
    manifest['trig'] = ('ref_particles._cs replaced by ref_core.cos16 / sin16 (math_cos16, '
                        'ps2 adder), each float32 result promoted to binary64')
    manifest['note'] = ('Synthetic inputs only (no game data). Each row ends with the first 16 hex digits '
                        'of SHA-256 over the reference state lines; see the generator docstring.')
    with open(os.path.join(a.out, 'fx_exact.json'), 'w') as h:
        json.dump(manifest, h, indent=2, sort_keys=True)
        h.write('\n')
    print('%d rows, %d bytes' % (len(rows), len(data)))
    for k in sorted(counts):
        print('  %-12s %6d' % (k, counts[k]))
    for k in sorted(stats):
        print('  %-34s %8d' % (k, stats[k]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
