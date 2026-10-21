#!/usr/bin/env python3
"""Exact AntQueen, Skeleton and Soul fixtures from research commit e9777eb, never from
native output.

Each case runs tools/ref_combat_ctors.py (docs/spec/combat-ctors.md: the three
constructors, set-states 0x237918, 0x2a7768 and 0x2aac38, the run prologues up to the
state switch and Soul_getModel) on an authored memory image, then a sequence of
operations. Every call-out of the model is answered by this script from a seeded
generator and recorded with its arguments and answer; the native replay gives the same
answers in the same order and compares the call names, the arguments, the bytes each
operation stored and the shared draw count.

The image is the research synthetic fixture's (test_ref_combat_ctors.py) widened: class
definitions with record pointers at their documented addresses, the skelcom block, the
Skeleton prefix texts, the airhub text, the documented virtual-table words of the three
classes over the base table the combat World writes, and an activity centre. The C
library class table gets bit 0 on 'A'..'Z' (ref_monsters_1.G_CTYPE: "bit 0 = upper
case"). Nothing is read from a game file.
"""
import gzip
import hashlib
import json
import random
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = 'e9777eb25a150123e48452f25968e8ee263d0003'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat-ctors'
PINS = {
    'docs/spec/combat-ctors.md': '4ad864cf79408a82f4b84794d9f097c7b61818c4f8f8dffa4755e4bf57fb4040',
    'tools/ref_combat_ctors.py': '118fb30578b872b871b49b62038941739fd32ff869dc9cf14ed381e8a5e793cc',
    'tools/test_ref_combat_ctors.py': 'b8d784fd4a4f1f390694ee22e8ef29904a39d5e9941d7f42346f445559e47100',
    'tools/difftest_combat_ctors.py': '2e9dccdf17f21721bc168e4c9a3973e2c80947d2f2ff2465c6e6ca74ee370404',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_monsters_1.py': '33a76a127c66ffd0b4c8c91c014d250c91c567cd168c2bc8870d5abae647c4d0',
}
M32 = 0xFFFFFFFF
CRE, ENEMY, OWNER, NODE = 0x100000, 0x110000, 0x130000, 0x120000
RECORDS, TEXT, LUMP_NAMES = 0x200000, 0x300000, 0x310000
# The base Creature table the combat World writes (combat_runtime_world.cpp write_table).
BASE_TABLE = (
    0x3c2608, 0x117128, 0x116638, 0x11a5f0, 0x3c7e98, 0x3c7ea0, 0x11a418, 0x11a260,
    0x116920, 0x116780, 0x11a3f0, 0x11a448, 0x11a290, 0x116d50, 0x116bd8, 0x11c298,
    0x3c2738, 0x11a868, 0x118410, 0x118418, 0x119328, 0x119400, 0x10de10, 0x10ded0,
    0x10e2c0, 0x10e658, 0x10bb08, 0x3c2688, 0x10beb8, 0x11a668, 0x118a58, 0x118af8,
    0x10bfc8, 0x3c2700, 0x3c2708, 0x118c50, 0x118c78, 0x3c2730, 0x3c2748)
# Documented overrides (symbols/monsters.tsv, symbols/runtime.tsv, symbols/creatures.tsv,
# docs/monsters-2.md; combat-ctors.md section 7 lists every slot that differs).
OVERRIDES = {
    'antqueen': {0: 0x3c3c78, 1: 0x3c3c10, 2: 0x2378fc, 3: 0x2378f4, 4: 0x236f78, 5: 0x236f10,
                 15: 0x11f728, 27: 0x237918},
    'skeleton': {0: 0x3cc62c, 1: 0x3cc5b8, 2: 0x2a774c, 4: 0x2a6864, 5: 0x2a67c4, 8: 0x2a75d4,
                 9: 0x2a7508, 12: 0x2a7688, 15: 0x1264b8, 20: 0x2a7e04, 27: 0x2a7768},
    'soul': {0: 0x3cc7a0, 1: 0x3cc72c, 2: 0x2aa908, 3: 0x2aab60, 4: 0x2a9870, 5: 0x2a9678,
             8: 0x2aabd0, 9: 0x2aab68, 16: 0x2ab298, 27: 0x2aac38},
}
VT_ENEMY = 0x140000
SKEL_TEXTS = (b'skeletonred', b'skeletonblue', b'skeletonswordohwred', b'skeletonswordohwblue',
              b'skeletonsworddhwred', b'skeletonsworddhwblue', b'skeletonaxered', b'skeletonaxeblue',
              b'skeletonspearred', b'skeletonspearblue', b'skeletonbowred', b'skeletonbowblue',
              b'skeletonswordohw', b'skeletonsworddhw', b'skeletonaxe', b'skeletonspear', b'skeletonbow')
SKEL_TYPES = (b'Skeleton', b'WatSkel', b'SkeletonRed', b'skeletonblue', b'SkeletonSwordOHWRed',
              b'SkeletonSwordOHWBlue', b'SkeletonSwordDHWRed', b'skeletonsworddhwblue', b'SkeletonAxeRed',
              b'SkeletonAxeBlue', b'SkeletonSpearRed', b'SkeletonSpearBlue', b'SkeletonBowRed',
              b'SkeletonBowBlue', b'SkeletonSwordOHW', b'SkeletonSwordDHW', b'SkeletonAxe', b'SkeletonSpear',
              b'SkeletonBow', b'skeletonbow3', b'SkelRed')
LEVELS = (b'drunare', b'airhub', b'AirHub', b'airhub2', b'blomedal', b'')
STATES = {
    'antqueen': list(range(0, 0x1C)) + [0x40, 0xFFFFFFFF],
    'skeleton': list(range(0, 0x1A)) + [0x40, 0xFFFFFFFF],
    'soul': list(range(0, 0x24)) + [0x40, 0xFFFFFFFF],
}
LIMIT = 4096


def pin_ok():
    for path, want in PINS.items():
        blob = subprocess.check_output(['git', '-C', str(RESEARCH), 'show', f'{COMMIT}:{path}'])
        got = hashlib.sha256(blob).hexdigest()
        if got != want:
            raise SystemExit(f'{path} at {COMMIT} hashed {got}, pin is {want}')
        if path.startswith('tools/'):
            disk = (RESEARCH / path).read_bytes()
            if hashlib.sha256(disk).hexdigest() != want:
                raise SystemExit(f'{path} on disk is not {COMMIT}')


def enc(v):
    """Canonical text of an argument or an answer: integers in decimal (signed as the
    model holds them), None as n, tuples in (), lists in []."""
    if v is None:
        return 'n'
    if isinstance(v, bool):
        return str(int(v))
    if isinstance(v, int):
        return str(v)
    if isinstance(v, tuple):
        return '(' + ','.join(enc(x) for x in v) + ')'
    if isinstance(v, list):
        return '[' + ','.join(enc(x) for x in v) + ']'
    if isinstance(v, (bytes, bytearray)):
        return 'b' + bytes(v).hex()
    if isinstance(v, str):
        return 's' + v.encode().hex()
    raise TypeError(type(v))


class Answers:
    """The call-out side of the model: authored answers from a seeded generator."""

    def __init__(self, model_mod, rng, mem, scenario):
        self.CT, self.rng, self.mem, self.scenario = model_mod, rng, mem, scenario
        self.log = []

    def get(self, name):
        def answer(*args):
            value = self.decide(name, args)
            if name == 'GameObject_ctor':           # what the stub leaves: the class definition
                self.mem.poke(args[0] + 0x24, 4, args[2])
            self.log.append((name, args, value))
            return value
        return answer

    def decide(self, name, args):
        rng, s = self.rng, self.scenario
        f = lambda x: self.CT.C.f32(x)          # noqa: E731
        if name == 'balance_lookup':
            found = rng.randrange(2)
            return (found, f(1.0), f(0.5), f(2.0), f(1.25), f(0.75))
        if name in ('obj_getPropInt', 'obj_getPropStr'):
            return 0
        if name == 'res_find':
            return 0x00D00000 + 0x40 * rng.randrange(64)
        if name == 'model_buildHideMask':
            return rng.getrandbits(64)
        if name == 'anim_advance':
            return rng.choice((0, 0, 1, 2, 3))
        if name == 'world_collectObjectsInBox':
            box = args[2]
            if box == 0xC0 and s.get('enemy'):
                return [ENEMY]
            return []
        if name == 'Creature.v20':
            return rng.randrange(2)
        if name == 'creature_findEnemyInCone':
            return ENEMY if rng.randrange(3) else 0
        if name == 'anim_findTrack':
            return rng.randrange(2) * 0x00E00000
        if name == 'AntQueen_pickFlyDest':
            return (f(rng.randrange(-500, 500)) & M32, f(rng.randrange(-500, 500)) & M32, 0)
        if name == 'model_getSamplePointCount':
            return rng.choice((0, 1, 2, 3))
        if name == 'model_getSamplePoint':
            p = tuple(f(rng.randrange(-40, 40)) & M32 for _ in range(3))
            return (p, (0, 0, 0))
        if name in ('terrain_heightBelow', 'terrain_height'):
            return 0
        if name == 'player_speedScale':
            return f(1.0)
        if name == 'water_test':
            return 0
        return self.CT.shape_result(name, args)


def text(mem, addr, value):
    mem.poke(addr, len(value) + 1, int.from_bytes(value + b'\0', 'little'))


def build_image(CT, RC, mem, rng, cls, level):
    rec = RECORDS
    for block, count in ((CT.DEF_ANTQUEEN, 25), (CT.DEF_SOUL, 19), (CT.SKEL_COMMON, 72)):
        for k in range(2, count):
            mem.poke(block + 8 * k + 4, 4, rec)
            rec += 0x40
    for n in range(4):
        mem.poke(CT.DEF_SKELETON + CT.DEF_SKELETON_STEP * n + 0xC, 4, 0xC00040 + 0x10 * n)
    mem.poke(CT.DEF_ANTQUEEN + 0xC, 4, 0xC00040)
    mem.poke(CT.DEF_SOUL + 0xC, 4, 0xC00040)
    mem.poke(CT.DEF_SOUL + 0x1C, 4, 0xC00080)
    if cls == 'skeleton' and rng.randrange(3) == 0:
        # A skelcom block whose generation is stale: the names are resolved again.
        mem.poke(CT.SKEL_COMMON, 4, 0x00ABC000)
        mem.poke(CT.SKEL_COMMON + 4, 4, 7)
        for k in range(1, 6):
            mem.poke(CT.SKEL_COMMON + 8 * k, 4, LUMP_NAMES + 0x10 * k)
        mem.poke(CT.G_LUMP_GENERATION, 4, 9)
    for (addr, _kind, _side), value in zip(CT.SKEL_PREFIXES, SKEL_TEXTS):
        text(mem, addr, value)
    text(mem, CT.S_AIRHUB, b'airhub')
    text(mem, CT.G_LEVEL_NAME, level)
    for ch in range(ord('A'), ord('Z') + 1):
        mem.poke(CT.M1.G_CTYPE + ch, 1, 1)
    vt = {'antqueen': CT.VT_ANTQUEEN, 'skeleton': CT.VT_SKELETON, 'soul': CT.VT_SOUL}[cls]
    for slot, fn in enumerate(BASE_TABLE):
        mem.poke(vt + 8 * slot + 4, 4, OVERRIDES[cls].get(slot, fn))
        mem.poke(VT_ENEMY + 8 * slot + 4, 4, fn)
    mem.poke(RC.G_SIM_FRAME, 4, rng.randrange(0, 2000))
    mem.poke(RC.G_RAND_SEED, 4, rng.getrandbits(32))
    # One activity centre (0x4039c8, count byte 0x403a08), near or far.
    mem.poke(0x403A08, 1, 1)
    far = rng.randrange(2)
    mem.poke(0x4039C8, 4, CT.C.f32(5000.0 if far else 30.0))
    # An enemy object for target picks.
    mem.poke(ENEMY + 0xC, 4, 0x20)
    mem.poke(ENEMY + 0x48, 4, VT_ENEMY)
    mem.poke(ENEMY + 0x10, 4, CT.C.f32(60.0))
    mem.poke(ENEMY + 0x14, 4, CT.C.f32(-20.0))
    mem.poke(ENEMY + 0x310, 1, 3)
    mem.poke(NODE + 0xC, 4, ENEMY)          # a handle node whose object is the enemy


def snapshot(mem):
    return dict(mem.bytes)


def diff(before, after):
    return sorted((a, v) for a, v in after.items() if before.get(a) != v)


def run_case(CT, RC, C, index, cls, rng):
    C.set_fpu('ps2')
    mem = RC.Memory()
    level = rng.choice(LEVELS)
    scenario = {'enemy': rng.randrange(2)}
    build_image(CT, RC, mem, rng, cls, level)
    answers = Answers(CT, rng, mem, scenario)
    model = CT.Model(mem, answers)
    seed44 = rng.getrandbits(32) if rng.randrange(4) else 0
    mem.poke(CRE + 0x44, 4, seed44)
    facing = rng.randrange(0x10000)
    pos = (C.f32(rng.randrange(-200, 200)) & M32, C.f32(rng.randrange(-200, 200)) & M32, 0)
    lines = [f'CASE {cls}-{index}', 'CLASS ' + cls]
    init = sorted(mem.bytes.items())
    lines.append('MEM ' + ' '.join(f'{a:x}:{v:x}' for a, v in init))
    ops = []
    if cls == 'antqueen':
        name = rng.choice((b'AntQueen', b'antqueen', b'AntQueen2'))
        text(mem, TEXT, name)
        ops.append(('ctor', (TEXT, pos, facing, 0)))
    elif cls == 'skeleton':
        name = rng.choice(SKEL_TYPES)
        text(mem, TEXT, name)
        ops.append(('ctor', (pos, facing, 0, TEXT, rng.randrange(4))))
    else:
        name = rng.choice((b'Soul', b'soul'))
        text(mem, TEXT, name)
        owner = OWNER if rng.randrange(4) == 0 else 0
        ops.append(('ctor', (TEXT, pos, facing, 0, owner)))
    # The type name text joins the initial image.
    lines[2] = 'MEM ' + ' '.join(f'{a:x}:{v:x}' for a, v in sorted(mem.bytes.items()))
    for _ in range(rng.randrange(6, 16)):
        roll = rng.randrange(11)
        if roll == 10:
            # A focused request with a live target and the filter's timers clear: the
            # AntQueen rear cone, the Skeleton swings and the Soul's flinch chain.
            for off, size, value in ((0x234, 4, NODE), (0x314, 2, 0), (0x31C, 2, 0), (0x318, 2, 0),
                                     (0x3C8, 4, 0)):
                ops.append(('poke', (CRE + off, size, value)))
            x, y = rng.randrange(-300, 300), rng.randrange(-300, 300)
            ops.append(('poke', (ENEMY + 0x10, 4, C.f32(x) & M32)))
            ops.append(('poke', (ENEMY + 0x14, 4, C.f32(y) & M32)))
            focus = {'antqueen': (7, 7, 6, 0x12, 9), 'skeleton': (6, 7, 8, 0x11, 0x12),
                     'soul': (0x18, 0x12, 0x19, 0x14, 0x21)}[cls]
            ops.append(('set', (rng.choice(focus),)))
            continue
        if roll < 4:
            ops.append(('set', (rng.choice(STATES[cls]),)))
        elif roll < 7:
            ops.append(('run', ()))
        elif roll == 7 and cls == 'soul':
            ops.append(('model', ()))
        else:
            field = rng.choice(((0x314, 2), (0x31C, 2), (0x318, 2), (0x234, 4), (0x11A, 2), (0x2E0, 2),
                                (0x300, 4), (0x3C8, 4), (0xA8, 8), (0x232, 2), (0x118, 1), (RC.G_SIM_FRAME, 4)))
            off, size = field
            addr = off if off > 0x10000 else CRE + off
            if off == 0x234:
                value = rng.choice((0, NODE))
            elif off == 0x3C8:
                value = rng.choice((0, NODE))
            elif off == 0xA8:
                value = rng.getrandbits(64) & 0x8000000000000000 | rng.choice((0, 0x200, 0x200800, 0x1000000))
            elif off in (0x314, 0x31C, 0x318):
                value = rng.choice((0, 0, 5))
            else:
                value = rng.getrandbits(8 * size)
            ops.append(('poke', (addr, size, value)))
    for op, args in ops:
        answers.log.clear()
        model.calls.clear()
        draws = model.draws
        before = snapshot(mem)
        result = None
        error = None
        try:
            if op == 'ctor':
                getattr(model, cls + '_ctor')(CRE, *args)
            elif op == 'set':
                getattr(model, cls + '_set_state')(CRE, *args)
            elif op == 'run':
                getattr(model, cls + '_run_prologue')(CRE)
            elif op == 'model':
                result = model.soul_get_model(CRE)
            else:
                mem.poke(*args)
        except NotImplementedError as exc:
            error = str(exc)
        if op == 'poke':
            lines.append('POKE ' + ' '.join(f'{x:x}' for x in args))
            continue
        lines.append('OP ' + op + ' ' + enc(tuple(args)))
        for name, cargs, value in answers.log:
            lines.append('CALL ' + name + ' ' + enc(tuple(a & M32 if isinstance(a, int) else a for a in cargs))
                         + ' ' + enc(value))
        # Calls the model answers itself (shape_result defaults) are in model.calls but
        # not in answers.log only when no callback object answers; every name here goes
        # through Answers.get, so the two lists agree.
        if len(model.calls) != len(answers.log):
            raise SystemExit('a call-out bypassed the answer log')
        lines.append('DIFF ' + ' '.join(f'{a:x}:{v:x}' for a, v in diff(before, snapshot(mem))))
        lines.append(f'DRAWS {model.draws - draws}')
        if result is not None:
            lines.append(f'RESULT {result & M32:x}')
        if error is not None:
            lines.append('REFUSED ' + error.replace('\n', ' '))
            break
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    import ref_core as C
    import ref_creature as RC
    import ref_combat_ctors as CT
    rng = random.Random(20973)
    lines = ['# combat ctors (ref_combat_ctors at e9777eb)']
    counts = {}
    for cls in ('antqueen', 'skeleton', 'soul'):
        for index in range(160):
            lines += run_case(CT, RC, C, index, cls, random.Random(rng.getrandbits(64)))
            counts[cls] = counts.get(cls, 0) + 1
    text_out = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text_out, mtime=0)
    (out / 'combat_ctors.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-ctors-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text_out).hexdigest(),
        'cases': counts,
        'operations': text_out.count(b'\nOP '),
        'calls': text_out.count(b'\nCALL '),
    }
    (out / 'combat_ctors.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(json.dumps(meta['cases']), meta['operations'], 'operations', meta['calls'], 'calls',
          len(text_out), 'bytes')


if __name__ == '__main__':
    main()
