#!/usr/bin/env python3
"""Generate native/tests/vectors/fx_recipes.tsv from the pinned research reference
tools/ref_fx_recipes.py (research commit pinned in tools/fx2_research.py).

Run:  python3 -I tools/generate_fx_recipes_vectors.py [--research PATH]

Every input is synthetic: random positions, seeds and option values, and synthetic models
whose sample tables are expanded from a 64-bit seed by the integer-only rule below (shared
with native/tests/fx_recipes_tests.cpp), so a row stays short.  No game data is read.

Row format (tab separated, one case per row, integers in lowercase hex, "-" before a
negative integer, vectors and lists comma separated):

    property  <inputs ...>  calls  digest  stream  <final state ...>

calls   number of call records the reference returned, or REFUSE where the reference
        divides by zero (or, for Orc parts 6/7, does not model the original) and the
        native port must refuse
digest  FNV-1a 64 of the canonical stream text (call texts joined by ';')
stream  the canonical stream text itself when it is short, else "-"

The canonical text of a call is the reference tuple: its name, then each argument (int:
hex; list: [a,b,...]; str: as is), separated by single spaces.

Synthetic tables.  mix(seed) is splitmix64; fixed(lo, hi, scale) = from_int(lo + mix % (hi -
lo + 1)) * scale with the console arithmetic (exact: |k| < 2**24, scale a power of two).
  model  (n, seed, big): max(n, 1) points of 3 x fixed(-3072000, 3072000, 2**-10), then as
         many sizes: kind = mix % 4: 0 fixed(0, top * 16, 2**-4) (top 45000 when big, else
         10000), 1 fixed(0, 12800, 2**-4), 2 0.0, 3 -50.0.  sample_point(scale, anim, pos,
         yaw, i): table[i mod len] + scale * pos per axis.
  weapon (n, seed): n points of 3 x fixed(-20480, 20480, 2**-10), n sizes fixed(0, 48000,
         2**-4), segment a = 3 x fixed(-5120, 5120, 2**-10), b = 3 x fixed(20480, 61440,
         2**-10).  transform_sample_point(m, i) = mat34_transform_point(m, point[i mod n]).
  holder (seed): 9 x fixed(-1229, 1229, 2**-10) then 3 x fixed(-61440, 61440, 2**-10);
         attach_matrix(scale, anim, index) = base + float(index & 7) * 0.125 per element.
show_mask(model, variant) is the difftest's hash.
"""
import argparse
import json
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import fx2_research  # noqa: E402

OUT = ROOT / 'native' / 'tests' / 'vectors'
M32 = 0xFFFFFFFF
M64 = (1 << 64) - 1
P_2M10 = 0x3A800000          # 2**-10
P_2M4 = 0x3D800000           # 2**-4
P_EIGHTH = 0x3E000000        # 0.125
MINUS_50 = 0xC2480000
STREAM_INLINE = 2000
INLINE_ROWS = 6

C = R = None                 # ref_core, ref_fx_recipes (bound in main)


# ---- text forms ---------------------------------------------------------------------------
def h(v):
    return '-%x' % -v if v < 0 else '%x' % v


def fmt(x):
    if isinstance(x, bool):
        raise TypeError('bool in a call record')
    if isinstance(x, int):
        return h(x)
    if isinstance(x, (list, tuple)):
        return '[' + ','.join(fmt(a) for a in x) + ']'
    if isinstance(x, str):
        return x
    raise TypeError('unexpected %r' % (x,))


def call_text(c):
    return ' '.join([c[0]] + [fmt(a) for a in c[1:]])


def fnv64(data):
    v = 0xCBF29CE484222325
    for b in data:
        v = ((v ^ b) * 0x100000001B3) & M64
    return v


def hl(values):
    return ','.join(h(v) for v in values)


INLINED = {}


def expected(calls, prop):
    """calls, digest, stream fields.  The stream text is written out for the first
    INLINE_ROWS rows of each property (when short), the digest for every row."""
    if calls is None:
        return ['REFUSE', '0', '-']
    text = ';'.join(call_text(c) for c in calls)
    inline = INLINED.get(prop, 0) < INLINE_ROWS and len(text) <= STREAM_INLINE
    if inline:
        INLINED[prop] = INLINED.get(prop, 0) + 1
    return [h(len(calls)), '%016x' % fnv64(text.encode()), text if inline else '-']


# ---- synthetic tables (mirrored in fx_recipes_tests.cpp) ------------------------------------
class Mix:
    def __init__(self, seed):
        self.s = seed & M64

    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & M64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)

    def fixed(self, lo, hi, scale):
        return C.mul(C.from_int(lo + self.next() % (hi - lo + 1)), scale)


class Model:
    def __init__(self, n, seed, big):
        self.n, self.seed, self.big = n, seed, big
        mix = Mix(seed)
        count = max(n, 1)
        self.points = [[mix.fixed(-3072000, 3072000, P_2M10) for _ in range(3)]
                       for _ in range(count)]
        top = 45000 if big else 10000
        self.sizes = []
        for _ in range(count):
            kind = mix.next() % 4
            if kind == 0:
                self.sizes.append(mix.fixed(0, top * 16, P_2M4))
            elif kind == 1:
                self.sizes.append(mix.fixed(0, 12800, P_2M4))
            elif kind == 2:
                self.sizes.append(0)
            else:
                self.sizes.append(MINUS_50)

    def field(self):
        return '%s,%x,%d' % (h(self.n), self.seed, 1 if self.big else 0)

    def sample_count(self):
        return self.n

    def sample_point(self, scale, anim, pos, yaw, index):
        i = index % len(self.points)
        return [C.add(self.points[i][k], C.mul(scale, pos[k])) for k in range(3)], self.sizes[i]


class Weapon:
    def __init__(self, n, seed, attach_base):
        self.n, self.seed, self.attach_base = n, seed, attach_base
        mix = Mix(seed)
        self.points = [[mix.fixed(-20480, 20480, P_2M10) for _ in range(3)] for _ in range(n)]
        self.sizes = [mix.fixed(0, 48000, P_2M4) for _ in range(n)]
        a = [mix.fixed(-5120, 5120, P_2M10) for _ in range(3)]
        b = [mix.fixed(20480, 61440, P_2M10) for _ in range(3)]
        self.segment = (a, b)
        self.n_samples = n

    def field(self):
        return '%s,%x,%s' % (h(self.n), self.seed, h(self.attach_base))

    def sample_count(self):
        return self.n

    def transform_sample_point(self, matrix, index):
        i = index % self.n
        return C.mat34_transform_point(matrix, self.points[i]), self.sizes[i]

    def effect_segment(self):
        return self.segment


class Holder:
    def __init__(self, seed):
        self.seed = seed
        mix = Mix(seed)
        self.base = [mix.fixed(-1229, 1229, P_2M10) for _ in range(9)] + \
                    [mix.fixed(-61440, 61440, P_2M10) for _ in range(3)]

    def attach_matrix(self, scale, anim, index):
        return [C.add(v, C.mul(C.from_int(index & 7), P_EIGHTH)) for v in self.base]


def show_mask(model, variant):
    x = ((model * 0x2545F491) ^ (variant * 0x9E3779B97F4A7C15)) & M64
    return (x ^ (x >> 29)) & M64


# ---- random inputs ---------------------------------------------------------------------------
def random_pos(rng):
    kind = rng.random()
    if kind < 0.6:
        return [C.f32(rng.uniform(-4000, 4000)) for _ in range(3)]
    if kind < 0.9:
        return [C.f32(float(rng.randint(-3000, 3000)) + rng.choice((0, 0.5, 0.25)))
                for _ in range(3)]
    return [C.f32(rng.uniform(-30000, 30000)) for _ in range(3)]


def random_matrix(rng):
    return [C.f32(rng.uniform(-1.1, 1.1)) for _ in range(9)] + \
           [C.f32(rng.uniform(-3000, 3000)) for _ in range(3)]


def guarded(fn):
    try:
        return fn()
    except ZeroDivisionError:
        return None


# ---- properties ------------------------------------------------------------------------------
def gen_fx_spawn(rows, counts):
    for fx_id in list(range(0, 0x17)) + [0x17, -1, 0x40, -0x80000000]:
        rng = random.Random('fx_spawn/%d' % fx_id)
        heavy = fx_id in (0, 1, 2, 3, 4, 6, 8, 9, 10, 11, 12)
        count = counts['heavy'] if heavy else counts['light'] if -1 <= fx_id <= 0x17 else counts['outside']
        for case in range(count):
            pos = random_pos(rng)
            cos_seed, shr_seed = rng.getrandbits(32), rng.getrandbits(32)
            near = rng.random() < 0.85
            gore = rng.choice((0, 1, 2, 2, -1, 3))
            if rng.random() < 0.1:
                made = None
            else:
                p = 0.9 if case % 4 else 0.3
                made = set(d for d in R.FX_SPAWN_DEFS if rng.random() < p)
            pool = 0 if rng.random() < 0.2 else rng.choice((1, 0x01800000))
            cos, shr = C.Lcg(cos_seed), C.Lcg(shr_seed)
            ref_made = None if made is None else set(made)
            calls = R.fx_spawn(fx_id, pos, cos, shr, near=near, gore=gore, made=ref_made,
                               pool_new=lambda size, a=pool: a)

            def bits(s):
                return '-' if s is None else '%x' % sum(1 << i for i, d in
                                                        enumerate(R.FX_SPAWN_DEFS) if d in s)
            rows.append(['fx_spawn', h(fx_id), hl(pos), '%x' % cos_seed, '%x' % shr_seed,
                         '%d' % near, h(gore), bits(made), h(pool)] + expected(calls, 'fx %d' % fx_id) +
                        ['%x' % cos.state, '%x' % shr.state, bits(ref_made)])


def gen_disintegrate(rows, counts):
    rng = random.Random('disintegrate')
    for _ in range(counts['disintegrate']):
        model = Model(rng.choice((rng.randint(0, 40), rng.randint(0, 6))), rng.getrandbits(64),
                      rng.random() < 0.3)
        pos = random_pos(rng)
        yaw = rng.choice((rng.getrandbits(16), rng.getrandbits(32)))
        scale = C.f32(rng.choice((1.0, rng.uniform(0.5, 2.0))))
        offset = C.f32(rng.choice((0.0, 0.0, -0.0, rng.uniform(-40, 40))))
        alternate = rng.choice((0, 1))
        anim = rng.choice((0, 0x01234560, rng.getrandbits(32)))
        seed = rng.getrandbits(32)
        cos = C.Lcg(seed)
        calls = R.disintegrate_model(model, pos, yaw, scale, offset, alternate, anim, cos)
        rows.append(['disintegrate', model.field(), hl(pos), h(yaw), h(scale), h(offset),
                     h(alternate), h(anim), h(seed)] + expected(calls, 'disintegrate') + [h(cos.state)])


def gen_shatter(rows, counts):
    rng = random.Random('ice shatter')
    for _ in range(counts['shatter']):
        model = Model(rng.choice((rng.randint(0, 40), rng.randint(0, 6))), rng.getrandbits(64),
                      False)
        pos = random_pos(rng)
        yaw = rng.getrandbits(16)
        kind = rng.random()
        if kind < 0.04:
            n = C.f32(rng.choice((0.0, -0.0, 0.5, -0.75)))      # (int) 0: refusal
        elif kind < 0.1:
            n = C.f32(rng.uniform(-12, -1))
        else:
            n = C.f32(rng.choice((rng.randint(1, 12), rng.uniform(0.6, 15.0),
                                  rng.randint(13, 60))))
        scale = C.f32(rng.uniform(0.5, 2.0))
        offset = C.f32(rng.choice((0.0, rng.uniform(-40, 40))))
        anim = rng.choice((0, 0x01234560))
        seed = rng.getrandbits(32)
        pool = 0 if rng.random() < 0.1 else 1
        shared = C.Lcg(seed)
        calls = guarded(lambda: R.shatter_to_loose_ice(model, pos, yaw, n, scale, offset, anim,
                                                       shared, pool_new=lambda size: pool))
        rows.append(['ice_shatter', model.field(), hl(pos), h(yaw), h(n), h(scale), h(offset),
                     h(anim), h(seed), h(pool)] + expected(calls, 'shatter') +
                    [h(shared.state) if calls is not None else h(seed)])


CLASS_ORDER = ('Ant', 'AntQueen?', 'FireBeetle', 'Ghoul?', 'Mosquito', 'Orc', 'Scorpion',
               'BlackWidow')


def gen_lose_part(rows, counts):
    for index, name in enumerate(CLASS_ORDER):
        rng = random.Random('lose part %s' % name)
        for _ in range(counts['lose_part']):
            cls = [rng.getrandbits(32) if k in (0, 2, 3, 4, 6) else 0 for k in range(8)]
            variant = rng.getrandbits(32) & rng.getrandbits(32)
            pos = random_pos(rng)
            yaw = rng.choice((rng.getrandbits(16), rng.getrandbits(32)))
            anim = rng.getrandbits(32)
            colour = rng.choice((0, 1, 2))
            scale = C.f32(rng.uniform(0.5, 2))
            kind = rng.choice((0, 0, 1, 2))
            part = rng.choice((rng.randint(-1, 13), rng.randint(0, 8), rng.getrandbits(32))) & M32
            pool = 0 if rng.random() < 0.1 else 1
            show0 = rng.choice((0, rng.getrandbits(64)))
            c = R.Creature(variant=variant, cls=cls, pos=pos, yaw=yaw, anim=anim, colour=colour,
                           scale=scale, kind=kind)
            c.show = show0
            if name == 'Orc' and part in (6, 7):
                calls = None                     # not modelled by the reference: refusal
                out_variant, out_show = variant, show0
            else:
                calls = R.lose_body_part(name, c, part, show_mask, pool_new=lambda size: pool)
                out_variant, out_show = c.variant, c.show
            rows.append(['lose_part', '%d' % index, h(variant), hl(cls), hl(pos), h(yaw), h(anim),
                         h(colour), h(scale), h(kind), h(part), h(pool), h(show0)] +
                        expected(calls, name) + [h(out_variant), h(out_show)])


def gen_emit(rows, counts):
    rng = random.Random('emit from model verts')
    for _ in range(counts['emit']):
        model = Model(rng.choice((rng.randint(1, 30), rng.randint(1, 30), 0)), rng.getrandbits(64),
                      False)
        pos, yaw = random_pos(rng), rng.getrandbits(16)
        n = rng.choice((0, 1, 3, rng.randint(1, 40), -2))
        speed = C.f32(rng.uniform(-20, 20))
        direction = rng.choice((None, [C.f32(rng.uniform(-1, 1)) for _ in range(3)]))
        offset = rng.choice((None, [C.f32(rng.uniform(-50, 50)) for _ in range(3)]))
        defn = rng.getrandbits(24)
        anim = rng.getrandbits(32)
        seed = rng.getrandbits(32)
        ref = C.Lcg(seed)
        calls = guarded(lambda: R.emit_from_model_verts(model, pos, yaw, anim, n, defn, direction,
                                                        offset, speed, ref))
        rows.append(['emit_verts', model.field(), hl(pos), h(yaw), h(anim), h(n), h(defn),
                     hl(direction) if direction else '-', hl(offset) if offset else '-', h(speed),
                     h(seed)] + expected(calls, 'emit') + [h(ref.state if calls is not None else seed)])


def gen_sparkle(rows, counts):
    rng = random.Random('model sparkle')
    for _ in range(counts['sparkle']):
        model = Model(rng.choice((0, rng.randint(1, 30))), rng.getrandbits(64), True)
        near = rng.random() < 0.9
        pos, yaw = random_pos(rng), rng.getrandbits(16)
        amount = C.f32(rng.choice((rng.uniform(0, 1.5), rng.uniform(-0.2, 0.2), 0.0)))
        offset = C.f32(rng.choice((0.0, rng.uniform(-40, 40))))
        has_model = rng.random() < 0.95
        anim = rng.choice((0, 0x01234560))
        seed = rng.getrandbits(32)
        ref = C.Lcg(seed)
        calls = R.model_sparkle(model if has_model else None, pos, yaw, anim, amount, offset, ref,
                                near=near)
        rows.append(['sparkle', model.field() if has_model else '-', hl(pos), h(yaw), h(anim),
                     h(amount), h(offset), h(seed), '%d' % near] + expected(calls, 'sparkle') +
                    [h(ref.state)])


def gen_arcs(rows, counts):
    rng = random.Random('model arcs')
    for _ in range(counts['arcs']):
        model = Model(rng.choice((0, rng.randint(1, 30))), rng.getrandbits(64), False)
        near = rng.random() < 0.9
        pos, yaw = random_pos(rng), rng.getrandbits(16)
        alpha = C.f32(rng.uniform(0, 1.6))
        scale = C.f32(rng.uniform(0.5, 2))
        offset = C.f32(rng.choice((0.0, rng.uniform(-40, 40))))
        has_model = rng.random() < 0.95
        anim = rng.choice((0, 0x01234560))
        rgba = rng.getrandbits(32)
        arcs = [[rng.randint(0, 35), rng.randint(0, 35), rng.choice((0, rng.getrandbits(32)))]
                for _ in range(3)]
        answer = rng.choice((0, rng.getrandbits(16), rng.getrandbits(32)))
        seed = rng.getrandbits(32)
        ref = C.Lcg(seed)
        st = R.ArcState(arcs)
        calls = R.model_arcs(model if has_model else None, pos, yaw, anim, alpha, scale, offset,
                             st, rgba, ref, near=near, beam_create=lambda a, b: answer)
        rows.append(['arcs', model.field() if has_model else '-', hl(pos), h(yaw), h(anim),
                     h(alpha), h(scale), h(offset), hl(sum(arcs, [])), h(rgba), h(answer),
                     h(seed), '%d' % near] + expected(calls, 'arcs') +
                    [h(ref.state), hl(sum(st.arcs, []))])


def gen_glow(rows, counts):
    rng = random.Random('model glow')
    for _ in range(counts['glow']):
        model = Model(rng.choice((0, rng.randint(1, 20))), rng.getrandbits(64), False)
        near = rng.random() < 0.9
        pos, yaw = random_pos(rng), rng.getrandbits(16)
        scale = C.f32(rng.uniform(0.5, 2))
        offset = C.f32(rng.choice((0.0, rng.uniform(-40, 40))))
        fade = C.f32(rng.choice((rng.uniform(0.1, 3), 1.0, 0.0)))
        mode = rng.choice((0, 1, 2, 3, 0, 2))
        ta = [rng.randint(-300, 300) & 0xFFFF for _ in range(4)]
        tb = [rng.randint(0, 255) for _ in range(4)]
        ramp = R.GlowRamp([C.f32(rng.uniform(-10, 300)) for _ in range(4)],
                          [C.f32(rng.uniform(-10, 300)) for _ in range(4)])
        a0, b0 = list(ramp.a), list(ramp.b)
        flags = rng.choice((0x20, 0x21, 0, 0x10))
        has_obj = rng.random() < 0.95
        has_model = rng.random() < 0.95
        size_add = rng.randint(-2000, 2000)
        frame = rng.getrandbits(31)
        anim = rng.choice((0, 0x01234560))
        calls = R.model_glow(model if has_model else None, flags if has_obj else None, ramp, pos,
                             yaw, anim, scale, offset, fade, ta, tb, mode, size_add, frame,
                             near=near)
        rows.append(['glow', model.field() if has_model else '-', h(flags) if has_obj else '-',
                     hl(a0), hl(b0), hl(pos), h(yaw), h(anim), h(scale), h(offset), h(fade),
                     hl(ta), hl(tb), h(mode), h(size_add), h(frame), '%d' % near] +
                    expected(calls, 'glow') + [hl(ramp.a), hl(ramp.b)])


def gen_weapon(rows, counts):
    rng = random.Random('weapon status')
    codes = (R.CODE_LIGHTNING, R.CODE_COLD, R.CODE_FIRE, R.CODE_POISON, R.CODE_DISEASE, 0x10, 0)
    for case in range(counts['weapon']):
        n = rng.randint(1, 24) if rng.random() < 0.98 else 0
        weapon = Weapon(n, rng.getrandbits(64), rng.randint(0, 255))
        holder = Holder(rng.getrandbits(64)) if rng.random() < 0.8 else None
        amount = C.f32(rng.choice((rng.uniform(0, 1.5), 1.0, 0.0, -0.5)))
        scale = C.f32(rng.uniform(0.6, 1.6))
        if case % 3 == 0:            # one element (or none) at a time, every combination
            entries = [(rng.choice(codes), rng.choice((1, 5, -3)))] + \
                      [(rng.choice((0x10, 0)), rng.choice((0, 1))) for _ in range(3)]
            rng.shuffle(entries)
        else:
            entries = [(rng.choice(codes), rng.choice((0, 1, 5, -3))) for _ in range(4)]
        glow = [rng.choice((0, 0, rng.randint(-300, 300))) for _ in range(4)]
        colour2 = [rng.choice((0, rng.randint(-300, 300))) for _ in range(4)]
        if rng.random() < 0.3:
            glow = [0, 0, 0, 0]
        item = R.WeaponItem(glow, colour2, entries)
        pos, yaw = random_pos(rng), rng.getrandbits(16)
        swinging = rng.choice((0, 1, 1))
        joint_add = rng.choice((rng.randint(0, 3), rng.randint(-200, 200)))
        prev = random_matrix(rng)
        r = rng.random()
        if r < 0.15:
            prev = [C.ONE, 0, 0, 0, C.ONE, 0, 0, 0, C.ONE] + [rng.choice((0, C.ONE)) for _ in range(3)]
        elif r < 0.6 and holder is not None:
            # last frame's matrix near this frame's: small motion, small counts
            m = C.mat34_mul(holder.attach_matrix(scale, 0, R.s8(weapon.attach_base) + joint_add),
                            C.mat34_set_pos_rot_z(pos, C.s16(yaw + 0x4000)))
            prev = [C.add(v, C.f32(rng.uniform(-0.3, 0.3) if k < 9 else rng.uniform(-25, 25)))
                    for k, v in enumerate(m)]
        sim_frame = rng.getrandbits(20)
        frame = rng.choice((sim_frame - 1, sim_frame - 3, 0)) & M32
        beam = rng.choice((0, rng.getrandbits(16)))
        trail = rng.choice((0, rng.getrandbits(16)))
        counter = rng.choice((0, rng.randint(0, 100), 100, rng.randint(-128, 127)))
        spark_kind = rng.randint(0, 4)
        sparks = rng.choice((0, 0, rng.randint(1, 5), rng.randint(-3, 255)))
        state = R.WeaponFxState(prev, frame=frame, beam=beam, trail=trail, counter=counter,
                                spark_kind=spark_kind, sparks=sparks)
        override = random_matrix(rng) if rng.random() < 0.1 else None
        level_flags = rng.choice((0, 0, 0, 0x100, 0x10000000, 0x10000100, 0x1))
        gore = rng.choice((0, 1, 2))
        dialog = rng.random() < 0.03
        answers = {'trail_create': rng.getrandbits(16), 'beam_create': rng.getrandbits(16),
                   'beam_alive': rng.choice((0, 1))}
        has_model = rng.random() < 0.97
        anim = rng.choice((0x01234560, rng.getrandbits(32)))
        seed = rng.getrandbits(32)
        cos = C.Lcg(seed)
        before = (list(item.glow), list(item.colour2))
        calls = guarded(lambda: R.model_status_effects(
            amount, scale, item, weapon if has_model else None, state, pos, yaw, holder, anim,
            swinging, joint_add, override, cos, level_flags=level_flags, gore=gore,
            sim_frame=sim_frame, dialog_open=dialog, answers=answers))
        if calls is None:            # refused: the caller's objects stay as they were
            cos = C.Lcg(seed)
            state = R.WeaponFxState(prev, frame=frame, beam=beam, trail=trail, counter=counter,
                                    spark_kind=spark_kind, sparks=sparks)
            item.glow, item.colour2 = before
        rows.append(['weapon', h(amount), h(scale), hl(before[0]), hl(before[1]),
                     hl(sum((list(e) for e in entries), [])),
                     weapon.field() if has_model else '-', hl(prev), h(frame), h(beam), h(trail),
                     h(counter), h(spark_kind), h(sparks), hl(pos), h(yaw),
                     '%x' % holder.seed if holder else '-', h(anim), '%d' % swinging,
                     h(joint_add), hl(override) if override else '-', h(seed), h(level_flags),
                     h(gore), h(sim_frame), '%d' % dialog,
                     hl([answers['trail_create'], answers['beam_create'], answers['beam_alive']])]
                    + expected(calls, 'weapon') +
                    [h(cos.state), hl(state.matrix), h(state.frame), h(state.beam),
                     h(state.trail), h(state.counter), h(state.spark_kind), h(state.sparks),
                     hl(item.glow), hl(item.colour2)])


COUNTS = {'heavy': 300, 'light': 600, 'outside': 200, 'disintegrate': 1000,
          'shatter': 1500, 'lose_part': 400, 'emit': 1500, 'sparkle': 1000, 'arcs': 1000,
          'glow': 1000, 'weapon': 1500}


def main(argv=None):
    global C, R
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--research', default=str(fx2_research.DEFAULT_RESEARCH))
    parser.add_argument('--scale', type=float, default=1.0, help='multiply every case count')
    args = parser.parse_args(argv)
    counts = {k: max(1, int(v * args.scale)) for k, v in COUNTS.items()}
    with fx2_research.references(pathlib.Path(args.research)):
        import ref_core
        import ref_fx_recipes
        C, R = ref_core, ref_fx_recipes
        rows = []
        gen_fx_spawn(rows, counts)
        gen_disintegrate(rows, counts)
        gen_shatter(rows, counts)
        gen_lose_part(rows, counts)
        gen_emit(rows, counts)
        gen_sparkle(rows, counts)
        gen_arcs(rows, counts)
        gen_glow(rows, counts)
        gen_weapon(rows, counts)
    data = ''.join('\t'.join(r) + '\n' for r in rows).encode()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'fx_recipes.tsv').write_bytes(data)
    manifest = fx2_research.manifest(len(rows), data, __file__)
    (OUT / 'fx_recipes.json').write_text(json.dumps(manifest, indent=1, sort_keys=True) + '\n')
    print('%d rows, %d bytes' % (len(rows), len(data)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
