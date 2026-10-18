#!/usr/bin/env python3
"""Generate native/tests/vectors/fx_skills.tsv (+ fx_skills.json) and the synthetic recipe
constants native/tests/fixtures/fx_skill_recipes_synthetic.json for the native port of part 3
of the pinned research reference tools/ref_fx_skills.py (the 21 skill-file effect functions).

Every case drives one method of the pinned ref_fx_skills.Model on a seeded random SYNTHETIC
memory image with scripted callback answers, as tools/difftest_fx_skills.py drives the
original, and records what the native model must reproduce: the call-outs (canonical
arguments), the memory writes (generator words included) and the shared draws.

No retail value is written.  The constant effect arguments that tools/fx_skill_recipes.py
reports per call site (particle definitions, glow colours / alphas / flags / bias / size /
angle, trail row / width / flags / colours / lifetime, decal type / growth, fx_spawn ids,
particle back-dates) are replaced at the recorded call boundary by synthetic stand-ins, per
call site; the light-record colours / radii / falloff / +0x2c word the constructors store
are replaced at the store.  The stand-ins are written into the synthetic fixture in the
fx_skill_recipes.py --json format (built with that tool's own build()), which the native test
loads exactly as a native client loads the user's JSON.  The site of a model call is resolved
from the reference's own source (line order of the call-out inside the method, and the class
constant tuple a looped call iterates), never from a retail value; the replaced originals are
only compared in memory (the scan at the end) and never printed or stored.

  python3 -I tools/generate_fx_skills_vectors.py [--seed N] [--check]

--check regenerates in memory and compares with the files on disk.
"""
import argparse
import hashlib
import inspect
import json
import pathlib
import random
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import fx2_research                                                   # noqa: E402

VECTORS = ROOT / 'native/tests/vectors/fx_skills.tsv'
MANIFEST = ROOT / 'native/tests/vectors/fx_skills.json'
FIXTURE = ROOT / 'native/tests/fixtures/fx_skill_recipes_synthetic.json'
M32 = 0xFFFFFFFF

# method -> the original function whose call sites its call-outs are (helpers inlined in the
# original belong to their caller: _fsf_trail to FireStormFlare_run, _sf_burn to StaticFire_run)
FUNCTION_OF = {
    'shock_draw': 'ShockProjectile_draw', 'firearrow_draw': 'FireArrowProjectile_draw',
    'holystrike_draw': 'HolyStrike_draw', 'badger_draw': 'SpellSummonBadger_draw',
    'resurrect_draw': 'SpellResurrect_draw', 'smokegen_draw': 'SmokeGen_draw',
    'multifire_ctor': 'MultiFireProjectile_ctor', 'multifire_run': 'MultiFireProjectile_run',
    'multifire_end': 'MultiFireProjectile_end', 'firestormflare_run': 'FireStormFlare_run',
    '_fsf_trail': 'FireStormFlare_run', 'pustule_run': 'Pustule_run', 'diseasecloud_run': 'DiseaseCloud_run',
    'arena_run': 'SpellArena_run', 'hornomen_ctor': 'SpellHornOmenEffect_ctor',
    'hornomen_run': 'SpellHornOmenEffect_run', 'planarsteel_ctor': 'SpellPlanarSteelEffect_ctor',
    'planarsteel_run': 'SpellPlanarSteelEffect_run', 'staticfire_ctor': 'StaticFire_ctor',
    'staticfire_ctor2': 'StaticFire_ctor2', 'staticfire_init_common': 'StaticFire_initCommon',
    'staticfire_wind': 'StaticFire_wind', 'staticfire_add_flame': 'StaticFire_addFlame',
    'staticfire_move_flames': 'StaticFire_moveFlames', 'staticfire_run': 'StaticFire_run',
    '_sf_burn': 'StaticFire_run', 'staticfire_draw': 'StaticFire_draw',
}
PRIMITIVE_OF = {'particle_spawn': 'particle', 'glow_add': 'glow', 'trail_create': 'trail',
                'decal_add': 'decal', 'fx_spawn': 'fx_spawn'}
# canonical argument position -> fx_skill_recipes.py key (register / slot), per call-out
KEY_AT = {
    'particle_spawn': {0: 'a0', 3: 'f12'},
    'glow_add': {1: 'a1', 2: 'a2', 3: 'a3', 4: 't0', 5: 't1', 6: 't2', 7: 't3', 8: 'f12', 9: 'f13'},
    'trail_create': {0: 'a0', 1: 'a1', 2: 'a2', 3: 'a3', 4: 't0', 5: 't1', 6: 't2', 7: 'f12'},
    'decal_add': {1: 'a1', 2: 'a2', 3: 'a3', 4: 'f12'},
    'fx_spawn': {0: 'a0'},
}
P2 = ('a0', 'f12')
# (function, primitive, ordinal in ascending site address) -> the keys the model passes as
# literals there.  Must equal native/src/fx/skills_fx.cpp's NEEDS (the native test checks the
# fixture against it) and is confirmed below: exactly these positions are constant over all cases.
SITES = {
    ('ShockProjectile_draw', 'glow', 0): ('a1', 'a2', 'a3', 't3', 'f12'),
    ('FireArrowProjectile_draw', 'glow', 0): ('a1', 'a2', 'a3', 't0', 't2', 't3', 'f12'),
    ('HolyStrike_draw', 'glow', 0): ('a1', 'a2', 'a3', 't0', 't3', 'f12'),
    ('SpellSummonBadger_draw', 'glow', 0): ('a1', 'a2', 'a3', 't3', 'f12'),
    ('SpellResurrect_draw', 'glow', 0): ('a1', 'a2', 'a3', 't2', 't3', 'f12', 'f13'),
    ('SmokeGen_draw', 'glow', 0): ('t3', 'f12'),
    ('MultiFireProjectile_ctor', 'trail', 0): ('a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 'f12'),
    ('MultiFireProjectile_run', 'particle', 0): P2,
    ('MultiFireProjectile_end', 'fx_spawn', 0): ('a0',),
    ('MultiFireProjectile_end', 'fx_spawn', 1): ('a0',),
    ('FireStormFlare_run', 'particle', 0): P2, ('FireStormFlare_run', 'particle', 1): P2,
    ('FireStormFlare_run', 'particle', 2): P2, ('FireStormFlare_run', 'particle', 3): P2,
    ('FireStormFlare_run', 'particle', 4): P2,
    ('FireStormFlare_run', 'decal', 0): ('a2', 'a3'),
    ('Pustule_run', 'particle', 0): P2, ('Pustule_run', 'particle', 1): P2,
    ('DiseaseCloud_run', 'particle', 0): P2, ('DiseaseCloud_run', 'particle', 1): P2,
    ('DiseaseCloud_run', 'particle', 2): P2,
    ('SpellArena_run', 'particle', 0): P2, ('SpellArena_run', 'particle', 1): P2,
    ('SpellHornOmenEffect_run', 'particle', 0): P2, ('SpellHornOmenEffect_run', 'particle', 1): P2,
    ('SpellPlanarSteelEffect_run', 'particle', 0): P2, ('SpellPlanarSteelEffect_run', 'particle', 1): P2,
    ('SpellPlanarSteelEffect_run', 'particle', 2): P2, ('SpellPlanarSteelEffect_run', 'particle', 3): P2,
    ('SpellPlanarSteelEffect_run', 'particle', 4): P2,
    ('SpellPlanarSteelEffect_run', 'glow', 0): ('t2', 't3', 'f12', 'f13'),
    ('StaticFire_run', 'particle', 0): P2, ('StaticFire_run', 'particle', 1): P2,
    ('StaticFire_draw', 'glow', 0): ('a1', 'a2', 'a3', 't0', 't2', 't3', 'f12'),
    ('StaticFire_draw', 'glow', 1): ('t3', 'f12'),
}
# sites the model fills from ONE class constant: their definition must be equal (the native
# loader refuses a JSON where it is not); HornOmen's two sites are one model call (all keys)
EQUAL = ((('FireStormFlare_run', 'particle', 0), ('FireStormFlare_run', 'particle', 3), ('a0',)),
         (('FireStormFlare_run', 'particle', 1), ('FireStormFlare_run', 'particle', 4), ('a0',)),
         (('SpellPlanarSteelEffect_run', 'particle', 1), ('SpellPlanarSteelEffect_run', 'particle', 4), ('a0',)),
         (('SpellHornOmenEffect_run', 'particle', 0), ('SpellHornOmenEffect_run', 'particle', 1), P2))
FLOAT_KEYS = ('f12', 'f13')
LIGHTS = ('StaticFire_ctor', 'StaticFire_ctor2', 'SpellPlanarSteelEffect_ctor')


def f32(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32bits(bits):
    return struct.unpack('<f', struct.pack('<I', bits & M32))[0]


# ---------------------------------------------------------------------------------------
# stand-ins
# ---------------------------------------------------------------------------------------
def make_standins(rng):
    """Synthetic values per site and key, and per light record.  Integers are chosen in
    ranges unlike the effect arguments of a real executable; floats are odd fractions."""
    out = {}
    definition = 0x00A10000
    small = iter(range(0x200, 0x10000, 0x37))
    fraction = iter([1.0 + k / 64.0 + 0.0078125 for k in range(1, 400)])
    for site in sorted(SITES):
        values = {}
        for key in SITES[site]:
            if key in FLOAT_KEYS:
                values[key] = f32(next(fraction) * (1 if key == 'f12' else 2048))
            elif site[1] == 'particle':
                definition += 0x150
                values[key] = definition
            else:
                values[key] = next(small)
        out[site] = values
    for a, b, keys in EQUAL:
        for key in keys:
            out[b][key] = out[a][key]
    lights = {}
    for name in LIGHTS:
        lights[name] = {'colour': [next(small) for _ in range(8)], 'radius': f32(next(fraction) * 77),
                        'extra': f32(next(fraction) / 3)}
        if name != 'SpellPlanarSteelEffect_ctor':
            lights[name]['falloff'] = f32(next(fraction) / 9000)
    return out, lights


# ---------------------------------------------------------------------------------------
# Python value <-> text (the native test parses exactly this)
# ---------------------------------------------------------------------------------------
def ser(v):
    if isinstance(v, bool):
        v = int(v)
    if isinstance(v, int):
        if v < 0:
            raise ValueError('negative integer in a vector: %d' % v)
        return '%x' % v
    if v is None:
        return 'N'
    if isinstance(v, str):
        if any(c in v for c in ",;()[]'\t\n"):
            raise ValueError('text: %r' % v)
        return "'" + v
    if isinstance(v, (bytes, bytearray)):
        return 'y' + bytes(v).hex()
    if isinstance(v, tuple):
        return '(' + ','.join(ser(x) for x in v) + ')'
    if isinstance(v, list):
        return '[' + ','.join(ser(x) for x in v) + ']'
    raise ValueError('cannot write %r' % (v,))


def ser_writes(writes):
    """'addr,size,value' per write; '=value' repeats the previous address and size."""
    out, last = [], None
    for a, s, v in writes:
        out.append('=%x' % v if (a, s) == last else '%x,%x,%x' % (a, s, v))
        last = (a, s)
    return ';'.join(out)


def ser_memory(mem_bytes):
    """Runs of present non-zero bytes ('addr:hex', ';'-separated); a zero byte reads as an
    absent one, so short zero gaps are merged into runs."""
    keys = sorted(a for a, b in mem_bytes.items() if b)
    runs, start, data, last = [], None, bytearray(), None
    for a in keys:
        if start is not None and a - last <= 4:
            data.extend(b'\0' * (a - last - 1))
            data.append(mem_bytes[a])
        else:
            if start is not None:
                runs.append('%x:%s' % (start, data.hex()))
            start, data = a, bytearray([mem_bytes[a]])
        last = a
    if start is not None:
        runs.append('%x:%s' % (start, data.hex()))
    return ';'.join(runs) or '-'


# ---------------------------------------------------------------------------------------
# the generator proper (runs inside fx2_research.references)
# ---------------------------------------------------------------------------------------
def run(seed):
    import ref_core as C                        # noqa: F401  (pinned)
    import ref_creature as RC
    import ref_skills_c as SC
    import ref_arena_monsters as AM
    import ref_fx_skills as FS
    import fx_skill_recipes as R
    import fx_callsites as FC

    rng = random.Random(seed)
    standins, lights = make_standins(random.Random(seed ^ 0x5A5A))
    addr_of = {name: addr for name, addr in FS.DRAW_MODELLED.items()}
    for row in (r for rs in FS.RECIPES.values() for r in rs):
        addr_of.setdefault(row.function, row.addr)

    # line numbers of every effect call-out inside each method of part 3
    call_lines = {}
    for method in set(FUNCTION_OF):
        lines, start = inspect.getsourcelines(getattr(FS.Model, method))
        for k, text in enumerate(lines):
            for name in PRIMITIVE_OF:
                if "self.call('%s'" % name in text:
                    call_lines.setdefault((method, name), []).append(start + k)

    body_addrs = set(SC.SKILL_BODIES) | set(AM.SKILL_BODIES) | set(SC.SPELL_BODIES) | set(AM.SPELL_BODIES) \
        | set(AM.CLASS_BODIES) | {b[0] for b in RC.BASE_BODIES.values()}
    seen = {}               # (site, key) -> set of values the reference passed (constant check)
    retail = {}             # (site, key) -> set of the replaced originals (scan only, never written)
    light_retail = {}       # (light name, field) -> set of replaced originals

    class VModel(FS.Model):
        """ref_fx_skills.Model with the call boundary recording answers and replacing the
        JSON-constant arguments by stand-ins (Calls.call otherwise unchanged)."""

        def __init__(self, mem, cb):
            super().__init__(mem, cb)
            self.answers = []
            self.store_map = None
            self.planar_fall_seen = False

        def site(self, name, args):
            frame = sys._getframe(2)
            method = frame.f_code.co_name
            if method not in FUNCTION_OF or (method, name) not in call_lines:
                raise AssertionError('effect call-out outside a part-3 method: %s in %s' % (name, method))
            lines = call_lines[(method, name)]
            ordinal = max(k for k, line in enumerate(lines) if line <= frame.f_lineno)
            function, prim = FUNCTION_OF[method], PRIMITIVE_OF[name]
            if method == 'multifire_end':
                index = 0 if self.u32(FS.MultiFire.LEVEL_FLAGS) & 0x40000 else 1
            elif method == '_fsf_trail':
                index = FS.FireStormFlare.FSF_PARTICLES.index(args[0])
            elif method == 'firestormflare_run' and prim == 'particle':
                index = 3 + ordinal
            elif method == 'diseasecloud_run':
                index = FS.DiseaseCloud.DC_PARTICLES.index(args[0])
            elif method == 'arena_run':
                index = FS.Arena.ARENA_PARTICLES.index(args[0])
            elif method == 'planarsteel_run' and prim == 'particle':
                index = (0, 1, 3, 4)[ordinal]
                if ordinal == 1:
                    index = 1 + (FS.PlanarSteel.PS_SPARK, FS.PlanarSteel.PS_SPARK2).index(args[0])
            else:
                index = ordinal
            return (function, prim, index)

        def call(self, name, *args):
            if name in PRIMITIVE_OF:
                site = self.site(name, args)
                args = list(args)
                for pos, key in KEY_AT[name].items():
                    if pos >= len(args):
                        continue
                    seen.setdefault((site, key), set()).add(args[pos] & M32 if isinstance(args[pos], int) else args[pos])
                    if key in SITES.get(site, ()):
                        retail.setdefault((site, key), set()).add(args[pos] & M32)
                        args[pos] = standins[site][key]
            args = tuple(a & M32 if isinstance(a, int) else a for a in args)
            self.calls.append((name, args))
            function = self.cb.get(name)
            if function is not None:
                answer = function(*args)
                self.answers.append(ser(answer))
                return answer
            self.answers.append('-')
            return FS.shape_result(name, args)

        # light records: the stores of the constant light-record words take stand-ins
        def _w(self, a, size, v):
            if self.store_map is not None and a in self.store_map:
                field, value = self.store_map[a]
                light_retail.setdefault(field, set()).add(v & ((1 << (8 * size)) - 1))
                v = value
            return super()._w(a, size, v)

        def _light_map(self, name, light, with_falloff):
            rec = lights[name]
            m = {}
            for k in range(8):
                m[light + 0xA + 2 * k] = ((name, 'colour%d' % k), rec['colour'][k])
            for off in (0x1C, 0x24):
                m[light + off] = ((name, 'radius'), rec['radius'])
            if with_falloff:
                for off in (0x20, 0x28):
                    m[light + off] = ((name, 'falloff'), rec['falloff'])
            m[light + 0x2C] = ((name, 'extra'), rec['extra'])
            return m

        def _sf_light(self, obj, pos, colour, alpha_word):
            name = 'StaticFire_ctor' if self.ctx == 'ctor' else 'StaticFire_ctor2'
            self.store_map = self._light_map(name, obj + 0x80, True)
            try:
                return super()._sf_light(obj, pos, colour, alpha_word)
            finally:
                self.store_map = None

        def staticfire_ctor(self, *a):
            self.ctx = 'ctor'
            return super().staticfire_ctor(*a)

        def staticfire_ctor2(self, *a):
            self.ctx = 'ctor2'
            return super().staticfire_ctor2(*a)

        def planarsteel_ctor(self, spell, kind, owner, extra):
            self.store_map = self._light_map('SpellPlanarSteelEffect_ctor', spell + 0x30, False)
            try:
                return super().planarsteel_ctor(spell, kind, owner, extra)
            finally:
                self.store_map = None

    # -----------------------------------------------------------------------------------
    # synthetic world
    # -----------------------------------------------------------------------------------
    def nonbody():
        while True:
            a = 0x00600000 + 4 * rng.randrange(0x4000)
            if a not in body_addrs:
                return a

    class World:
        def __init__(self):
            self.mem = RC.Memory()
            self.top = 0x00200000
            self.objects, self.monsters = [], []
            self.drop = set()
            self.globals()

        def poke(self, a, size, v):
            self.mem.poke(a, size, v & ((1 << (8 * size)) - 1))

        def pf(self, a, x):
            self.poke(a, 4, f32(x))

        def pvec(self, a, values):
            for i, v in enumerate(values):
                self.pf(a + 4 * i, v)

        def alloc(self, size):
            a = self.top
            self.top += (size + 0x1F) & ~0xF
            return a

        def globals(self):
            for k, word in enumerate((FS.G_GLOW_TEX_A, FS.G_GLOW_TEX_B, FS.G_GLOW_TEX_C, FS.G_GLOW_TEX_RES,
                                      FS.G_GLOW_TEX_FIRE, FS.G_GLOW_TEX_FIRE_DIM)):
                self.poke(word, 4, 0x00D30000 + 0x100 * k + rng.randrange(16))
            self.poke(RC.G_RAND_SEED, 4, rng.getrandbits(32))
            self.poke(FS.G_WORLD, 4, 0x00777000 + rng.randrange(16) * 4)
            self.set_frame()

        def set_frame(self, frame=None):
            if frame is None:
                frame = rng.choice((rng.randrange(0, 4000), rng.randrange(0, 1 << 20), rng.randrange(1 << 31),
                                    -rng.randrange(1, 1 << 20)))
            self.poke(FS.G_SIM_FRAME, 4, frame)
            return frame

        def tick(self, n=1):
            self.poke(FS.G_SIM_FRAME, 4, self.mem.load(FS.G_SIM_FRAME, 4) + n)

        def table(self, bodies):
            """A virtual table: slot -> body address (or a non-body function); the this
            adjustment is usually 0."""
            t = self.alloc(8 * 40)
            for slot in (1, 10, 11, 12, 13, 16, 20, 37):   # the slots part 3 reaches
                fn = bodies.get(slot)
                if fn is None or rng.random() < 0.25:
                    fn = nonbody() if rng.random() < 0.9 else 0
                self.poke(t + 8 * slot, 2, rng.choice((0, 0, 0, 0, 4, 0xFFF8)))
                self.poke(t + 8 * slot + 4, 4, fn)
            return t

        def creature(self, flags=None, pos=None):
            c = self.alloc(0x320)
            self.poke(c + 0xC, 4, rng.choice((0x40, 0x20, 0x60, 0x41, 0x21, 0, 0x1, 0x20008 | 0x40))
                      if flags is None else flags)
            self.pvec(c + 0x10, pos or (rng.uniform(-3000, 3000), rng.uniform(-3000, 3000), rng.uniform(-100, 300)))
            self.pf(c + 0x20, rng.choice((40.0, 72.0, 110.5, rng.uniform(0, 200))))
            cdef = self.alloc(0x10)
            self.poke(cdef + 0xC, 4, 0x00C40000 + rng.randrange(0x1000) * 4)
            self.poke(c + 0x24, 4, cdef)
            self.poke(c + 0x38, 2, rng.randrange(0x10000))
            self.pf(c + 0x3C, rng.uniform(-50, 300))
            self.poke(c + 0x48, 4, self.table({16: 0x3C2738, 20: 0x119328, 37: 0x3C2730}))
            self.poke(c + 0xA8, 8, rng.choice((0, 0, 0x4000000, 0x800, 0x100000000000, rng.getrandbits(64))))
            self.pf(c + 0xDC, rng.choice((1.0, 0.5, 2.0)))
            self.pf(c + 0xE0, rng.choice((0.0, 0.0, 12.5, -7.0)))
            self.poke(c + 0x310, 1, rng.choice((0, 1, 2, 0xFF)))
            self.objects.append(c)
            if self.u32(c + 0xC) & 0x20:
                self.monsters.append(c)
            return c

        def u32(self, a):
            return self.mem.load(a, 4)

        def skill(self):
            s = self.alloc(0x80)
            for k in range(5):
                self.poke(s + 0x0E + 2 * k, 2, rng.choice((0, 1, 3, 5)))
                self.poke(s + 0x18 + 2 * k, 2, rng.randrange(0, 40))
                self.pf(s + 0x24 + 4 * k, rng.choice((0.0, 0.5, 1.25, 3.0)))
                self.poke(s + 0x38 + 2 * k, 2, rng.choice((0xFFFF, 2, 6)))
                self.poke(s + 0x42 + 2 * k, 2, rng.randrange(0, 60))
                self.pf(s + 0x4C + 4 * k, rng.choice((0.0, 0.75, 2.5)))
            self.poke(s + 0x60, 4, self.table({10: 0x3C85FC, 11: 0x3C8690, 12: 0x3C87F0, 13: 0x3C8864}))
            return s

        # ---- callbacks ----------------------------------------------------------------
        def get(self, name):
            if name in self.drop:
                return None
            return getattr(self, 'on_' + name.replace('.', '_'), None)

        def rfloat(self, lo, hi):
            return f32(rng.uniform(lo, hi))

        def on_isNearView(self, *a):
            return 1 if rng.random() < 0.8 else 0

        def on_model_getSamplePointCount(self, *a):
            return rng.choice((0, 1, 3, 7, 20, 0xFFFFFFFD))

        def on_model_getSamplePoint(self, *a):
            point = (self.rfloat(-500, 500), self.rfloat(-500, 500), self.rfloat(0, 200))
            if a[5]:                            # the 'O' output (second point) is asked for
                return point, (self.rfloat(1000, 9000), self.rfloat(-5, 5), self.rfloat(-5, 5))
            return point

        def on_trail_create(self, *a):
            return rng.choice((0, rng.randrange(1, 64)))

        def on_obj_moveAndCollide(self, *a):
            return rng.choice((0, 0, 0, 0, 0, 0, 1, 2, 3))

        def on_libc_sprintf(self, *a):
            return ('buf', b'snd%d' % a[1])

        def on_res_find(self, *a):
            return rng.randrange(0x00E00000, 0x00E10000)

        def on_sound_playOnObject(self, *a):
            return rng.randrange(0, 50)

        def on_sound_playAtPos(self, *a):
            return rng.randrange(0, 50)

        def on_terrain_height(self, *a):
            z = f32bits(a[1][2])
            return f32(z - rng.choice((0.0, 5.0, 15.9, 16.0, 40.0, 300.0)))

        def on_terrain_heightAtPoint(self, *a):
            return self.rfloat(-20, 60)

        def on_world_collectObjectsInBox(self, *a):
            pool = self.objects + ([self.current] if getattr(self, 'current', None) else [])
            return [x for x in pool if rng.random() < 0.5]

        def on_damage_apply(self, *a):
            return rng.randrange(0, 3)

        def on_particleGroup_create(self, *a):
            return rng.randrange(0, 16)

        def on_Creature_v0(self, *a):
            return rng.choice((FS.StaticFire.SF_TYPEINFO, 0x003D07B8))

        def on_typeinfo(self, *a):
            return a[0]

        def on_rtti_typeInfoEquals(self, *a):
            return 1 if a[0] == a[1] else 0

        def on_mem_allocTagged(self, *a):
            if not hasattr(self, 'flame_block'):
                self.flame_block = self.alloc(0xA00)
            return self.flame_block

        def on_obj_getPropStr(self, *a):
            return 0x00D40000 if rng.random() < 0.5 else 0

        def on_obj_getPropInt(self, *a):
            return rng.choice((0, 1, 1, 2, 8, 30))

        def on_Creature_v16(self, *a):
            return 0x00C50000 + rng.randrange(0x100) * 4

        def on_Creature_v20(self, *a):
            return rng.choice((0, 1, 4))

        def on_Creature_v37(self, *a):
            return rng.choice((0, 1, 2))

        def on_Skill_v10(self, *a):
            return rng.randrange(0, 90)

        def on_Skill_v11(self, *a):
            return rng.randrange(0, 90)

        def on_Skill_v13(self, *a):
            return rng.randrange(0, 90)

    DROPPABLE = ('isNearView', 'model_getSamplePointCount', 'model_getSamplePoint', 'trail_create',
                 'obj_moveAndCollide', 'libc_sprintf', 'res_find', 'terrain_height', 'terrain_heightAtPoint',
                 'world_collectObjectsInBox', 'particleGroup_create', 'obj_getPropStr', 'obj_getPropInt',
                 'mem_allocTagged', 'Creature.v16', 'Creature.v20', 'Creature.v0', 'typeinfo',
                 'rtti_typeInfoEquals', 'damage_apply')

    rows, counts, skipped = [], {}, {}

    def do(w, method, *args, record=True):
        """Run one method; record the case (pre-state, answers, calls, writes, draws)."""
        w.drop = set(n for n in DROPPABLE if rng.random() < 0.06)
        snapshot = dict(w.mem.bytes)
        m = VModel(w.mem, w)
        try:
            getattr(m, method)(*args)
        except Exception as exc:                # noqa: BLE001 - a reference refusal: not a case
            skipped[method] = skipped.get(method, 0) + 1
            w.mem.bytes = snapshot
            if skipped[method] < 3:
                print('skipped %s: %r' % (method, exc), file=sys.stderr)
            return m
        if record:
            calls = ';'.join(name + ser(tuple(a)) for name, a in m.calls)
            writes = ser_writes(m.writes)
            row = '\t'.join((method, ser(tuple(a & M32 if isinstance(a, int) else a for a in args)),
                              ser_memory(snapshot), ';'.join(m.answers) or '.', calls or '-',
                              writes or '-', '%x' % m.draws))
            if len(row) > 8000 and rng.random() < 0.75:     # keep the file small: fewer of the long cases
                return m
            rows.append(row)
            counts[method] = counts.get(method, 0) + 1
        return m

    def vec(values):
        return tuple(f32(v) for v in values)

    def rpos():
        return (rng.uniform(-4000, 4000), rng.uniform(-4000, 4000), rng.uniform(-200, 400))

    # ---- the draw slots -----------------------------------------------------------------
    for _ in range(160):
        w = World()
        obj = w.alloc(0x80)
        frame = w.set_frame()
        w.pvec(obj + 0x10, rpos())
        w.poke(obj + 0x58, 4, frame + rng.randrange(-400, 400))
        do(w, 'shock_draw', obj)
        w.pf(obj + 0x1C, rng.choice((0.0, 1.0, 3.5, 8.0, 16.0, rng.uniform(0, 40))))
        do(w, 'firearrow_draw', obj)
        d = rng.choice(((0.0, 0.0), (1.0, 0.0), (rng.uniform(-50, 50), rng.uniform(-50, 50))))
        w.pf(obj + 0x6C, d[0])
        w.pf(obj + 0x70, d[1])
        do(w, 'holystrike_draw', obj)
    for _ in range(115):
        w = World()
        spell, node = w.alloc(0x80), w.alloc(0x20)
        frame = w.set_frame()
        badger = w.creature()
        w.poke(spell + 0x18, 4, node if rng.random() < 0.9 else 0)
        w.poke(node + 0xC, 4, badger if rng.random() < 0.9 else 0)
        w.pf(spell + 0x1C, rng.choice((0.0, 0.5, 1.0, 1.5, 2.0, 2.5, rng.uniform(-1, 3))))
        do(w, 'badger_draw', spell)
        owner = w.creature()
        w.poke(spell + 0x18, 4, owner)
        w.poke(FS.G_NUM_PLAYERS, 4, rng.choice((1, 1, 1, 2, 4)))
        w.poke(spell + 0x20, 4, rng.choice((0, 1)))
        w.poke(spell + 0x6C, 4, frame - rng.choice((0, 1, 30, 59, 60, 61, 500, -5)))
        do(w, 'resurrect_draw', spell)
    for _ in range(100):
        w = World()
        obj = w.alloc(0x500)
        w.pvec(obj + 0x10, rpos())
        first = rng.randrange(100)
        span = rng.choice((0, 1, 2, 5, 12, rng.randrange(1, 100)))
        for k in range(span):
            i = (first + k) % 100
            for j in range(5):
                w.poke(obj + 0x50 + 10 * i + 2 * j, 2, rng.randrange(-3000, 3000))
        w.poke(obj + 0x438, 2, first)
        w.poke(obj + 0x43A, 2, (first + span) % 100)
        for off in range(0x43C, 0x45C, 4):
            w.poke(obj + off, 4, rng.randrange(0, 256))
        w.poke(obj + 0x45C, 4, rng.randrange(1000, 20000))
        w.poke(obj + 0x460, 4, rng.randrange(1000, 20000))
        w.poke(obj + 0x464, 4, rng.choice((1, 2, 3, 5)))
        w.pf(obj + 0x474, rng.choice((1.0, 0.5, 2.0)))
        w.pf(obj + 0x47C, rng.uniform(0, 50))
        w.poke(obj + 0x488, 4, rng.randrange(0, 4000))
        w.poke(obj + 0x472, 2, rng.randrange(-200, 200))
        w.poke(obj + 0x4C, 4, 0x00D20000 + rng.randrange(16))
        do(w, 'smokegen_draw', obj)

    # ---- MultiFireProjectile --------------------------------------------------------------
    for _ in range(45):
        w = World()
        obj = w.alloc(0x80)
        owner = rng.choice((0, w.creature(), w.creature()))
        skill = w.skill()
        w.poke(FS.MultiFire.LEVEL_FLAGS, 4, rng.choice((0, 0x40000, 0x10040000)))
        w.poke(obj + 0x44, 4, rng.getrandbits(32))
        level = rng.randrange(0, 41)
        if rng.random() < 0.7:                  # the owner handle Projectile_ctor would have set
            node = w.alloc(0x10)
            w.poke(node + 0xC, 4, rng.choice((owner or 0, w.creature())))
            w.poke(obj + 0x4C, 4, node)
        do(w, 'multifire_ctor', obj, vec(rpos()), rng.randrange(0x10000), owner, level, skill)
        if rng.random() < 0.4:
            w.poke(obj + 0x6C, 4, rng.choice((590, 598, 599, 600)))
        ticks = rng.choice((3, 10, 30, 120))
        for t in range(ticks):
            if w.u32(obj + 0x60):
                break
            w.tick()
            do(w, 'multifire_run', obj, record=t < 4 or rng.random() < 0.25)
        w.poke(obj + 0x60, 4, 0 if rng.random() < 0.8 else 1)
        do(w, 'multifire_end', obj, rng.choice((0, 1, 2)))

    # ---- FireStormFlare -------------------------------------------------------------------
    for _ in range(36):
        w = World()
        frame = w.set_frame()
        obj = w.alloc(0x80)
        w.poke(obj + 0x48, 4, w.table({}))
        w.pvec(obj + 0x10, rpos())
        w.pvec(obj + 0x5C, (rng.uniform(-6, 6), rng.uniform(-6, 6), rng.uniform(-12, 2)))
        w.poke(obj + 0x44, 4, rng.getrandbits(32))
        w.pf(obj + 0x4C, rng.choice((0.25, 0.5, 1.0, 1.3, 2.0, 0.0)))
        w.pf(obj + 0x1C, rng.choice((8.0, 12.0, 20.0)))
        w.poke(obj + 0x68, 4, frame + rng.choice((0, 5, 40, 200)))
        w.poke(obj + 0x50, 4, rng.choice((0, 0, 0, 1)))
        w.poke(obj + 0x58, 2, rng.choice((0, 0, 30, 200)))
        for t in range(rng.choice((2, 10, 40))):
            w.tick()
            do(w, 'firestormflare_run', obj, record=t < 1 or rng.random() < 0.03)

    # ---- Pustule --------------------------------------------------------------------------
    for _ in range(40):
        w = World()
        obj = w.alloc(0x180)
        w.poke(obj + 0x48, 4, w.table({}))
        pos = rpos()
        w.pvec(obj + 0x10, pos)
        w.pvec(obj + 0x58, (rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-8, 1)))
        w.pf(obj + 0x54, pos[2] - rng.choice((0.0, 5.0, 30.0, 120.0)))
        w.poke(obj + 0x44, 4, rng.getrandbits(32))
        w.poke(obj + 0x64, 4, rng.choice((0, 0, 10, 299, 300)))
        w.poke(obj + 0x6C, 4, rng.choice((0, 0, 0, 1)))
        for t in range(rng.choice((2, 20, 60))):
            w.tick()
            do(w, 'pustule_run', obj, record=t < 1 or rng.random() < 0.12)

    # ---- DiseaseCloud ---------------------------------------------------------------------
    for _ in range(40):
        w = World()
        frame = w.set_frame(rng.choice((rng.randrange(0, 4000), 30 * rng.randrange(1, 1000) - rng.randrange(0, 3))))
        obj = w.alloc(0x80)
        w.poke(obj + 0x48, 4, w.table({}))
        w.pvec(obj + 0x10, (2000.0 + rng.uniform(-50, 50), 3000.0 + rng.uniform(-50, 50), 10.0))
        w.poke(obj + 0x44, 4, rng.getrandbits(32))
        w.poke(obj + 0x54, 4, frame + rng.choice((0, 3, 40, 100)))
        w.poke(obj + 0x58, 4, rng.choice((0, 9, 10, 25, 60)))
        w.pf(obj + 0x5C, rng.choice((0.0, 10.0, 59.5, 200.0)))
        w.poke(obj + 0x60, 4, rng.randrange(0, 70000))
        for _k in range(rng.randrange(1, 5)):
            w.creature()
        node = w.alloc(0x20)
        w.poke(node + 0xC, 4, rng.choice(w.objects))
        w.poke(obj + 0x4C, 4, node if rng.random() < 0.85 else 0)
        for t in range(rng.choice((2, 31, 61))):
            w.tick()
            do(w, 'diseasecloud_run', obj, record=t < 1 or rng.random() < 0.08 or w.u32(FS.G_SIM_FRAME) % 30 == 0)

    # ---- SpellArena -----------------------------------------------------------------------
    for _ in range(120):
        w = World()
        spell = w.alloc(0x40)
        c = w.creature()
        w.poke(spell + 0xC, 4, c)
        w.poke(spell + 0x10, 4, rng.getrandbits(32))
        w.poke(FS.Arena.G_ARENA_COUNTER, 2, rng.choice((0, 29, 30, 31, 60, 61, 100, 180, 200,
                                                        rng.randrange(-100, 230))))
        do(w, 'arena_run', spell, vec(rpos()))

    # ---- SpellHornOmenEffect --------------------------------------------------------------
    for _ in range(40):
        w = World()
        c = w.creature()
        spell = w.alloc(0x40)
        do(w, 'hornomen_ctor', spell, rng.randrange(256), c, rng.getrandbits(32))
        if rng.random() < 0.25:                 # ends within the run
            w.poke(spell + 0x18, 4, w.u32(FS.G_SIM_FRAME) + rng.choice((0, 1, 2)))
        elif rng.random() < 0.5:
            w.poke(spell + 0x18, 4, w.u32(spell + 0x18) - rng.choice((5, 20, 60, 100, 135, 139, 140, 141, 598)))
        for t in range(rng.choice((1, 4, 8))):
            w.tick(rng.choice((1, 1, 7, 30)))
            do(w, 'hornomen_run', spell, vec(rpos()), record=(t < 1 and rng.random() < 0.7) or rng.random() < 0.05)

    # ---- SpellPlanarSteelEffect -----------------------------------------------------------
    for _ in range(40):
        w = World()
        c = w.creature()
        spell = w.alloc(0x80)
        do(w, 'planarsteel_ctor', spell, rng.randrange(256), c, rng.getrandbits(32))
        if rng.random() < 0.3:
            w.poke(spell + 0x1C, 2, rng.choice((0, 1, 2)))
        if rng.random() < 0.25:
            w.poke(spell + 0x24, 4, w.u32(FS.G_SIM_FRAME) + rng.choice((0, 1, 2, 3)))
        for t in range(rng.choice((1, 4, 8))):
            w.tick(rng.choice((1, 1, 7, 30)))
            burst = w.mem.load(spell + 0x1C, 2) == 0 and not w.u32(spell + 0x20)
            do(w, 'planarsteel_run', spell, vec(rpos()), record=t < 1 or burst or rng.random() < 0.08)

    # ---- StaticFire -----------------------------------------------------------------------
    def level_name(w):
        cut, dark = b'CUTLEVEL', b'DIM'
        w.mem.bytes.update({FS.StaticFire.SF_LEVEL_CUTSCENE + k: b for k, b in enumerate(cut + b'\0')})
        w.mem.bytes.update({FS.StaticFire.SF_LEVEL_DARK + k: b for k, b in enumerate(dark + b'\0')})
        name = rng.choice((cut, b'X' + dark + b'2', dark, b'town', b''))
        w.mem.bytes.update({FS.StaticFire.LEVEL_NAME + k: b for k, b in enumerate(name + b'\0')})

    for scenario in range(56):
        w = World()
        level_name(w)
        w.poke(FS.StaticFire.SF_SKILL, 4, w.skill())
        w.pf(RC.G_LEVEL_MONSTER_LEVEL, rng.choice((1.0, 7.0, 25.0)))
        w.poke(RC.G_CUTSCENE_ACTIVE, 1, rng.choice((0, 0, 1)))
        for _k in range(rng.randrange(1, 6)):
            w.creature(pos=(2000.0 + rng.uniform(-30, 30), 3000.0 + rng.uniform(-30, 30), 10.0))
        obj = w.alloc(0x4B0)
        w.current = obj
        w.poke(obj + 0x44, 4, rng.getrandbits(32))
        pos = (2000.0 + rng.uniform(-20, 20), 3000.0 + rng.uniform(-20, 20),
               rng.choice((10.0, -300.0, -470.0, 300.0, -420.0, -360.0)))
        if rng.random() < 0.6:
            do(w, 'staticfire_ctor', obj, vec(pos), rng.choice((-1, 0, 1, 60, 121, 400)) & M32,
               rng.choice(w.objects + [0]), rng.choice((0, 0, 1)),
               f32(rng.choice((240.0, 480.0, 60.0))), f32(rng.choice((1.0, 0.5, 2.0))))
        else:
            w.pvec(obj + 0x10, pos)
            do(w, 'staticfire_ctor2', obj, vec(pos), 0, rng.getrandbits(32))
            w.poke(obj + 0x7C, 4, rng.choice((-1, -1, 0, 5, 200)))
        if rng.random() < 0.5:
            w.poke(obj + 0x4A8, 4, rng.choice(w.objects + [0]))
        elif rng.random() < 0.5:                # a monster owner: slot 20 decides who is hurt
            w.poke(obj + 0x4A8, 4, w.creature(flags=rng.choice((0x20, 0x60, 0x21))))
        if rng.random() < 0.3:
            w.pf(obj + 0x60, rng.choice((80.0, 108.0, 95.0)))
        if rng.random() < 0.3:
            w.pf(obj + 0x64, rng.choice((0.004, 0.5, 0.995, 1.0)))
        if rng.random() < 0.3:
            end = w.mem.load(obj + 0x76, 2)
            w.poke(obj + 0x78, 2, end - 1)
            w.poke(obj + 0x7A, 2, end - 1)
        if w.u32(obj + 0x4C) == 0 and w.u32(obj + 0x4A4) == 0:
            continue
        ticks = rng.choice((5, 40, 120))
        for t in range(ticks):
            w.tick()
            hurts = w.u32(FS.G_SIM_FRAME) % 12 == 0 and rng.random() < 0.5     # the damage tick
            do(w, 'staticfire_run', obj, record=t < 1 or hurts or rng.random() < 0.03)
            if rng.random() < 0.3:
                do(w, 'staticfire_draw', obj, record=rng.random() < 0.2)
        # the helpers on their own
        do(w, 'staticfire_wind', obj, rng.randrange(0x10000))
        do(w, 'staticfire_add_flame', obj)
        do(w, 'staticfire_move_flames', obj)
    for _ in range(40):
        w = World()
        obj = w.alloc(0x4B0)
        w.current = obj
        w.pvec(obj + 0x10, rpos())
        w.pf(obj + 0x1C, 16.0)
        w.poke(obj + 0x44, 4, rng.getrandbits(32))
        w.poke(obj + 0x6F, 1, rng.choice((0, 0, 1)))
        w.poke(obj + 0x70, 1, rng.choice((0, 1)))
        do(w, 'staticfire_init_common', obj)

    # -----------------------------------------------------------------------------------
    # checks: the literal positions, the stand-ins, the scan
    # -----------------------------------------------------------------------------------
    problems = []
    for (site, key), values in sorted(seen.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        literal = key in SITES.get(site, ())
        if literal and len(retail.get((site, key), ())) != 1:
            problems.append('literal %s %s varies' % (site, key))
        if not literal and len(values) == 1 and key not in ('t1',):
            problems.append('computed %s %s was constant over %d cases (check)' % (site, key, len(values)))
    for site in SITES:
        for key in SITES[site]:
            if (site, key) not in retail and site != ('SpellHornOmenEffect_run', 'particle', 1):
                problems.append('no case reached %s %s' % (site, key))
            elif (site, key) in retail and standins[site][key] in retail[(site, key)]:
                problems.append('stand-in equals the original at %s %s' % (site, key))
    for field, values in light_retail.items():
        name, part = field
        rec = lights[name]
        want = rec['colour'][int(part[6:])] if part.startswith('colour') else rec[part]
        if want in values:
            problems.append('light stand-in equals the original: %s' % (field,))
    # scan: no recorded literal position holds an original, no original definition is a token
    originals = set()
    for (site, key), values in retail.items():
        originals |= values
    definitions = {v for (site, key), vs in retail.items() if site[1] == 'particle' and key == 'a0' for v in vs}
    definitions |= {FS.MultiFire.MF_PARTICLE, FS.Pustule.PUS_FALL, FS.Pustule.PUS_DROP, FS.HornOmen.HO_PARTICLE,
                    FS.PlanarSteel.PS_BURST, FS.PlanarSteel.PS_SPARK, FS.PlanarSteel.PS_SPARK2,
                    FS.PlanarSteel.PS_FALL, FS.StaticFire.SF_FLAME, FS.StaticFire.SF_EMBER}
    definitions |= set(FS.FireStormFlare.FSF_PARTICLES) | set(FS.DiseaseCloud.DC_PARTICLES) \
        | set(FS.Arena.ARENA_PARTICLES)
    text = '\n'.join(rows)
    tokens = set()
    for row in rows:
        for part in row.replace('(', ',').replace(')', ',').replace(';', ',').replace('\t', ',').split(','):
            tokens.add(part)
    leaked = sorted(d for d in definitions if '%x' % d in tokens)
    if leaked:
        problems.append('%d original definition addresses appear as tokens' % len(leaked))
    # the fixture holds no original either (the stand-ins, synthetic noise elsewhere)
    fixture_text = fixture(R, FC, FS, rng, standins, lights)
    fixture_values = set()

    def walk(node):
        if isinstance(node, dict):
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)
        elif isinstance(node, float):
            fixture_values.add(f32(node))
        elif isinstance(node, int) and not isinstance(node, bool):
            fixture_values.add(node)
    fixture_json = json.loads(fixture_text)
    for entry in fixture_json['classes'].values():
        for c in entry['calls']:
            walk(c['args'])
    walk(fixture_json['light_records'])
    light_originals = set().union(*light_retail.values()) if light_retail else set()
    clash = fixture_values & (definitions | originals | light_originals)
    if clash:
        problems.append('%d fixture values equal an original constant' % len(clash))
    replaced = sorted('%s/%s#%d %s' % (site[0], site[1], site[2], key) for site in SITES for key in SITES[site])
    replaced += sorted('%s light %s' % f for f in light_retail)
    return {'rows': rows, 'counts': counts, 'skipped': skipped, 'problems': problems, 'replaced': replaced,
            'fixture': fixture_text, 'text': text,
            'needs': ';'.join('%x/%s/%d:%s' % (addr_of[f], p, k, ','.join(SITES[(f, p, k)]))
                              for f, p, k in sorted(SITES, key=lambda x: (addr_of[x[0]], x[1], x[2])))}


def fixture(R, FC, FS, rng, standins, lights):
    """The synthetic recipe constants in the fx_skill_recipes.py --json format, built with that
    tool's own build() from synthetic scan rows: every recipe row's primitive calls at synthetic
    sites, stand-ins where the native model needs a constant, synthetic noise or null elsewhere."""
    rows = []
    for row in sorted((r for rs in FS.RECIPES.values() for r in rs), key=lambda r: r.addr):
        k = 0
        for part in row.calls.split():
            prim, n = part.split(':')
            for i in range(int(n)):
                k += 1
                site = row.addr + 0x10 + 0x24 * k
                fregs, iregs, stack = FC.SIGNATURES.get(prim, ((), (), ()))
                keys = ['f%d' % f for f in fregs] + [R.REG_NAMES[r] for r in iregs] + ['sp+%#x' % s for s in stack]
                need = SITES.get((row.function, prim, i))
                args = {}
                for key in keys:
                    if need is not None and key in need:
                        value = standins[(row.function, prim, i)][key]
                    elif rng.random() < 0.4:
                        value = None
                    else:
                        value = f32(rng.uniform(-9, 9)) if (key.startswith('f') or key.startswith('sp+')) \
                            else rng.randrange(0x40000000, 0x40100000)
                    if value is not None and (key.startswith('f') or (key.startswith('sp+') and prim in FC.STACK_FLOATS)):
                        value = f32bits(value)
                    args[key] = value
                rows.append((row.addr, row.function, site, prim, args))
    rng.shuffle(rows)                           # the native loader must not rely on list order
    classes = R.build(rows)
    out = {'executable_sha256': hashlib.sha256(b'synthetic fx skill recipes').hexdigest(),
           'range': [R.SKILL_LO, R.SKILL_HI], 'classes': classes,
           'particle_definitions': R.definitions(classes),
           'light_records': {name: {'colour': lights[name]['colour'],
                                    'radius': f32bits(lights[name]['radius']),
                                    'extra': f32bits(lights[name]['extra']),
                                    **({'falloff': f32bits(lights[name]['falloff'])} if 'falloff' in lights[name] else {})}
                             for name in LIGHTS}}
    return json.dumps(out, indent=1, sort_keys=True) + '\n'


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--seed', type=int, default=20973)
    ap.add_argument('--check', action='store_true')
    opts = ap.parse_args(argv[1:])
    with fx2_research.references(extra=['tools/ps2elf.py']):
        result = run(opts.seed)
    for p in result['problems']:
        print('PROBLEM', p, file=sys.stderr)
    data = (result['text'] + '\n').encode()
    header = ('# fx_skills vectors: method\targs\tmemory\tanswers (- = no handler, . = none)\tcalls\twrites\tdraws (generated by '
              'tools/generate_fx_skills_vectors.py --seed %d; synthetic stand-ins only)\n' % opts.seed).encode()
    data = header + ('# needs %s\n' % result['needs']).encode() + data
    manifest = fx2_research.manifest(len(result['rows']), data, __file__)
    manifest['counts'] = dict(sorted(result['counts'].items()))
    manifest['replaced_constants'] = result['replaced']
    manifest_text = json.dumps(manifest, indent=1, sort_keys=True) + '\n'
    if opts.check:
        same = (VECTORS.read_bytes() == data and FIXTURE.read_text() == result['fixture']
                and MANIFEST.read_text() == manifest_text)
        print('fx_skills vectors %s' % ('match' if same else 'DIFFER'))
        return 0 if same and not result['problems'] else 1
    VECTORS.parent.mkdir(parents=True, exist_ok=True)
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    VECTORS.write_bytes(data)
    FIXTURE.write_text(result['fixture'])
    MANIFEST.write_text(manifest_text)
    print('%d rows, %d bytes; per method: %s' % (len(result['rows']), len(data),
                                                 ' '.join('%s:%d' % kv for kv in sorted(result['counts'].items()))))
    if result['skipped']:
        print('skipped (the reference raised): %s' % result['skipped'])
    print('replaced constant positions: %d' % len(result['replaced']))
    return 1 if result['problems'] else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
