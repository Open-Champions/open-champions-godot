#!/usr/bin/env python3
"""Exact AntQueen_run, AntQueen_pickFlyDest, Skeleton_isEnemy, the three take-damage slots
and the six inline destructors, from research commit 5b92837, never from native output.

Each case runs the pinned reference models on an authored memory image:

- tools/ref_antqueen_run.py (docs/spec/antqueen-run.md): an AntQueen built by its
  constructor, then ticks of AntQueen_run in any state, set-state requests (0x17 runs
  AntQueen_pickFlyDest inside it), direct pickFlyDest calls and pokes of the timers, the
  track count, the clip flags, the stuck counter, the bite flag and the target;
- tools/ref_combat_ctors2.py (docs/spec/combat-ctors-2.md): a Skeleton, Soul or AntQueen
  built by its constructor, then Skeleton_isEnemy against objects of every class table
  the research names (and synthetic ones), the take-damage slots with attackers of every
  class (and none), and the destructors with flags 0 to 3, property lists and attached
  spells; the GameObject destructors (Fire, LooseFire, poison gas) on a planted object.

Every call-out is answered by this script from a seeded generator and recorded with its
arguments and answer; the native replay gives the same answers in the same order and
compares the call names, the arguments, the bytes each operation stored, the shared draw
count and the result. Nothing is read from a game file.
"""
import gzip
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '5b928373a4bc31e5566312de598f861cc9f58071'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat-ctors-2'
PINS = {
    'docs/spec/combat-ctors-2.md': 'd3324aa50d9cd5c0c44121aa98141fb44763e9f25f57ef9a7421710eb7e5a296',
    'docs/spec/antqueen-run.md': 'e2065673ef4d7e5cd96e0b6254e9d38e06db0e939680a0f431b9a334295a7791',
    'tools/ref_combat_ctors2.py': 'c0dc0ee6ea659ee7ecd2085c99c83b6ebf9bf62a285c9461f69ee3b8dda89fc5',
    'tools/ref_antqueen_run.py': 'cbe6f5c983e9cf8b38c64406506522b0703c66ede7b26304fb6d5c0c632dd562',
    'tools/test_ref_combat_ctors2.py': '8258fd9fee92ddc24d35dbb210368861cd4b60e581d71a7527fe7539d338fdf3',
    'tools/test_ref_antqueen_run.py': '6d457af1cbc3c10737e08eb2fee19acb1198515c6dbe83340969dd034e8c1dfb',
    'tools/difftest_combat_ctors2.py': 'dd38a6734bf042a80ff4ac78ac1a8271ee52d05498417cd4e42b02e085aac7f4',
    'tools/difftest_antqueen_run.py': 'd93bce105572cf3785cf109975b73edfb60a7bf5f43c225ab96396b012681ec3',
    'tools/ref_combat_ctors.py': '118fb30578b872b871b49b62038941739fd32ff869dc9cf14ed381e8a5e793cc',
    'tools/ref_combat_monsters.py': '1c9628ba39a4f6eda0db02f914e03e473f931a1850f6367291efdcf47e208d29',
    'tools/ref_combat_round5.py': '488ae487234fba45bd1aa0e1c89b6ef740e4af7355d698230059895c1369bced',
    'tools/ref_potion_ctors.py': '4b7a2520c01f7acf831ec0041384d5af9aec4a4466c671018ad63de0f249af42',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_monsters_1.py': '33a76a127c66ffd0b4c8c91c014d250c91c567cd168c2bc8870d5abae647c4d0',
}
M32 = 0xFFFFFFFF
CRE, ENEMY, NODE, OWNER = 0x100000, 0x110000, 0x120000, 0x130000
SKILL, SKILL_TABLE, WAVE = 0x140000, 0x141000, 0x150000
OTHERS = 0x160000                       # objects isEnemy and takeDamage see, 0x1000 apart
TABLES = 0x180000                       # their class tables, 0x200 apart
SYN_DESC = 0x1A0000                     # synthetic type-info descriptors and names
PROPS, PROP_TEXT, SPELLS = 0x1B0000, 0x1B1000, 0x1C0000
RECORDS, TEXT, LUMP_NAMES = 0x200000, 0x300000, 0x310000
VT_ENEMY = 0x170000
# The base Creature table the combat World writes (combat_runtime_world.cpp write_table).
BASE_TABLE = (
    0x3c2608, 0x117128, 0x116638, 0x11a5f0, 0x3c7e98, 0x3c7ea0, 0x11a418, 0x11a260,
    0x116920, 0x116780, 0x11a3f0, 0x11a448, 0x11a290, 0x116d50, 0x116bd8, 0x11c298,
    0x3c2738, 0x11a868, 0x118410, 0x118418, 0x119328, 0x119400, 0x10de10, 0x10ded0,
    0x10e2c0, 0x10e658, 0x10bb08, 0x3c2688, 0x10beb8, 0x11a668, 0x118a58, 0x118af8,
    0x10bfc8, 0x3c2700, 0x3c2708, 0x118c50, 0x118c78, 0x3c2730, 0x3c2748)
OVERRIDES = {
    'antqueen': {0: 0x3c3c78, 1: 0x3c3c10, 2: 0x2378fc, 3: 0x2378f4, 4: 0x236f78, 5: 0x236f10,
                 15: 0x11f728, 27: 0x237918},
    'skeleton': {0: 0x3cc62c, 1: 0x3cc5b8, 2: 0x2a774c, 4: 0x2a6864, 5: 0x2a67c4, 8: 0x2a75d4,
                 9: 0x2a7508, 12: 0x2a7688, 15: 0x1264b8, 20: 0x2a7e04, 27: 0x2a7768},
    'soul': {0: 0x3cc7a0, 1: 0x3cc72c, 2: 0x2aa908, 3: 0x2aab60, 4: 0x2a9870, 5: 0x2a9678,
             8: 0x2aabd0, 9: 0x2aab68, 16: 0x2ab298, 27: 0x2aac38},
}
# Tables of the other objects (slot 0 the getter of combat-ctors-2.md section 3; the
# Player's slot 37 is its own 0x3c3b18 and the Demon's slot 20 its own 0x25f378, both
# call-outs, as in the research).
OTHER_CLASSES = {
    'skeleton': {0: 0x3cc62c},
    'skelight': {0: 0x3cc6dc},
    'soul': {0: 0x3cc7a0},
    'antqueen': {0: 0x3c3c78},
    'player': {0: 0x3c3ab8, 37: 0x3c3b18},
    'demon': {0: 0x3ca81c, 20: 0x25f378},
    'creature': {0: 0x3c2608},
    'syn_demon': {0: 0x3b0000},          # a getter the model does not know: Creature.v0
    'syn_skeleton': {0: 0x3b0010},
    'syn_other': {0: 0x3b0020},
}
SYN_NAMES = {'syn_demon': b'5Demon', 'syn_skeleton': b'8Skeleton', 'syn_other': b'7Gremlin'}
SKEL_TEXTS = (b'skeletonred', b'skeletonblue', b'skeletonswordohwred', b'skeletonswordohwblue',
              b'skeletonsworddhwred', b'skeletonsworddhwblue', b'skeletonaxered', b'skeletonaxeblue',
              b'skeletonspearred', b'skeletonspearblue', b'skeletonbowred', b'skeletonbowblue',
              b'skeletonswordohw', b'skeletonsworddhw', b'skeletonaxe', b'skeletonspear', b'skeletonbow')
SKEL_TYPES = (b'Skeleton', b'SkeletonRed', b'skeletonblue', b'SkeletonSwordOHWRed', b'SkeletonAxeBlue',
              b'SkeletonSpearRed', b'SkeletonBow', b'SkelRed')
AQ_STATES = list(range(0, 0x1A)) + [0x1A, 0x1B, 0x40, 0x7FFF, 0x8000, 0xFFFF]
AQ_REQUESTS = list(range(0, 0x1A)) + [0x17] * 6 + [8, 8, 9, 10, 3, 3, 0x18, 0x19, 0x19, 6, 7]
HEIGHTS = (0.0, 0.0, 0.0, 3.0, -8.0, 11.0, 11.9, 12.0, -11.9, -12.0, -13.0, 24.0, -30.0, 60.0)


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
    if isinstance(v, str):
        return 's' + v.encode().hex()
    raise TypeError(type(v))


def text(mem, addr, value):
    mem.poke(addr, len(value) + 1, int.from_bytes(value + b'\0', 'little'))


class Answers:
    """The call-out side of the models: authored answers from a seeded generator."""

    def __init__(self, mod, C, rng, mem, scenario):
        self.mod, self.C, self.rng, self.mem, self.s = mod, C, rng, mem, scenario
        self.log = []

    def get(self, name):
        def answer(*args):
            value = self.decide(name, args)
            if name == 'GameObject_ctor':
                self.mem.poke(args[0] + 0x24, 4, args[2])
            self.log.append((name, args, value))
            return value
        return answer

    def decide(self, name, args):
        rng, s = self.rng, self.s
        f = lambda x: self.C.f32(x) & M32          # noqa: E731
        if name == 'balance_lookup':
            return (rng.randrange(2), f(1.0), f(0.5), f(2.0), f(1.25), f(0.75))
        if name in ('obj_getPropInt', 'obj_getPropStr'):
            return 0
        if name == 'res_find':
            return 0x00D00000 + 0x40 * rng.randrange(64)
        if name == 'model_buildHideMask':
            return rng.getrandbits(64)
        if name == 'anim_advance':
            return rng.choice((0, 0, 1, 1, 2, 3))
        if name == 'world_collectObjectsInBox':
            if args[2] == 0xC0 and s.get('enemy'):
                return [ENEMY]
            return []
        if name == 'creature_canSeeObj':
            return 1 if rng.random() < s.get('sight', 0.4) else 0
        if name == 'creature_findEnemyInCone':
            return ENEMY if rng.randrange(3) == 0 else 0
        if name in ('Creature_hitTargetAtReach', 'Creature_hitTargetOrPlayerInArc'):
            return rng.choice((0, 1))
        if name == 'terrain_height':
            z = struct.unpack('<f', struct.pack('<I', args[1][2] & M32))[0]
            return f(z + rng.choice(HEIGHTS))
        if name == 'pool_new':
            return rng.choice((0, WAVE))
        if name == 'Skill.v11':
            return rng.choice((0, 3, 10, 40, 133, 0x12345))
        if name == 'Creature.v0':
            return s['syn_desc']
        if name == 'Creature.v37':
            return rng.choice((0, 1, 3))
        if name == 'Creature.v20':
            return rng.randrange(2)
        if name == 'model_getSamplePointCount':
            return rng.choice((0, 1, 2, 3, 7, 0xFFFFFFFD))
        if name == 'model_getSamplePoint':
            p = tuple(f(rng.randrange(-40, 40)) for _ in range(3))
            return (p, (0, 0, 0))
        if name == 'iter_begin':
            nodes = s.get('spells', [])
            s['iter'] = list(nodes[1:])
            return nodes[0] if nodes else 0
        if name == 'iter_next':
            rest = s.get('iter', [])
            return rest.pop(0) if rest else 0
        if name in ('terrain_heightBelow',):
            return 0
        if name == 'player_speedScale':
            return f(1.0)
        if name == 'water_test':
            return 0
        return self.mod.shape_result(name, args)


def class_table(mem, vt, overrides):
    for slot, fn in enumerate(BASE_TABLE):
        mem.poke(vt + 8 * slot + 4, 4, overrides.get(slot, fn))


def build_image(CT, RC, C, mem, rng, cls):
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
    for (addr, _kind, _side), value in zip(CT.SKEL_PREFIXES, SKEL_TEXTS):
        text(mem, addr, value)
    text(mem, CT.S_AIRHUB, b'airhub')
    text(mem, CT.G_LEVEL_NAME, rng.choice((b'drunare', b'blomedal', b'')))
    for ch in range(ord('A'), ord('Z') + 1):
        mem.poke(CT.M1.G_CTYPE + ch, 1, 1)
    for name, vt in (('antqueen', CT.VT_ANTQUEEN), ('skeleton', CT.VT_SKELETON), ('soul', CT.VT_SOUL)):
        class_table(mem, vt, OVERRIDES[name])
    class_table(mem, VT_ENEMY, {})
    mem.poke(RC.G_SIM_FRAME, 4, rng.randrange(0, 2000))
    mem.poke(RC.G_RAND_SEED, 4, rng.getrandbits(32))
    mem.poke(0x403A08, 1, 1)
    mem.poke(0x4039C8, 4, C.f32(5000.0 if rng.randrange(2) else 30.0))
    mem.poke(ENEMY + 0xC, 4, 0x20)
    mem.poke(ENEMY + 0x48, 4, VT_ENEMY)
    mem.poke(ENEMY + 0x10, 4, C.f32(float(rng.randrange(-120, 120))) & M32)
    mem.poke(ENEMY + 0x14, 4, C.f32(float(rng.randrange(-120, 120))) & M32)
    mem.poke(ENEMY + 0x1C, 4, C.f32(12.0))
    mem.poke(ENEMY + 0x310, 1, 3)
    mem.poke(NODE + 0xC, 4, ENEMY)
    # SkillPoisonWeapons at g_skillTable[26] with a table at +0x60, and the level.
    mem.poke(0x41B1D8, 4, SKILL)
    mem.poke(SKILL + 0x60, 4, SKILL_TABLE)
    mem.poke(0x4F714C, 4, C.f32(float(rng.choice((1, 4, 7, 12)))) & M32)
    # The key GameObject_dtor scans for, authored in mixed case (it is lowered in place).
    text(mem, 0x5321B0, b'NaMe')
    # Some type-info descriptors already built, as after the first comparison.
    for getter, (desc, name, base) in sorted(CT2_TYPEINFO.items()):
        if rng.randrange(3) == 0:
            continue
        mem.poke(desc, 4, name)
        mem.poke(desc + 4, 4, 0x51DF48 if base else 0x51DF78)
        if base:
            mem.poke(desc + 8, 4, CT2_TYPEINFO[base][0])
    for getter, (desc, name, base) in CT2_TYPEINFO.items():
        text(mem, name, CT2_TEXTS[getter])


CT2_TYPEINFO = {}
RUN_STATES = {}
CT2_TEXTS = {
    0x3C7E10: b'10GameObject', 0x3C2608: b'8Creature', 0x3CC62C: b'8Skeleton', 0x3CC7A0: b'4Soul',
    0x3C3C78: b'8AntQueen', 0x3CC6DC: b'8Skelight', 0x3C3AB8: b'6Player', 0x3CA81C: b'5Demon',
}


def snapshot(mem):
    return dict(mem.bytes)


def diff(before, after):
    return sorted((a, v) for a, v in after.items() if before.get(a) != v)


def place_other(C, mem, rng, index, cls, scenario):
    o = OTHERS + 0x1000 * index
    vt = TABLES + 0x200 * index
    class_table(mem, vt, OTHER_CLASSES[cls])
    mem.poke(o + 0x48, 4, vt)
    mem.poke(o + 0xC, 4, rng.choice((0x20, 0x20, 0x60, 0x28, 0x1, 0x20020)))
    mem.poke(o + 0xA8, 8, rng.choice((0, 0, 0x100000000000, 0x4000000, 0x800)))
    mem.poke(o + 0x310, 1, rng.choice((0, 1, 3)))
    mem.poke(o + 0x3D4, 1, rng.choice((0, 1, 2, 2, 1, 0xFF)))
    mem.poke(o + 0x10, 4, C.f32(float(rng.randrange(-200, 200))) & M32)
    mem.poke(o + 0x14, 4, C.f32(float(rng.randrange(-200, 200))) & M32)
    if cls.startswith('syn_'):
        desc = SYN_DESC + 0x40 * index
        name = desc + 0x10
        text(mem, name, SYN_NAMES[cls])
        mem.poke(desc, 4, name)
        scenario['syn_desc'] = desc
    return o


def run_ops(model, answers, mem, ops, lines, runner):
    for op, args in ops:
        answers.log.clear()
        model.calls.clear()
        draws = model.draws
        before = snapshot(mem)
        result, error = None, None
        if op == 'wave':
            # State 0x19's wave frame: +0xf4 one past clock + 120, so the next run
            # (Creature_run adds 1 to the clock first) meets clock == +0xf4 - 120.
            op, args = 'poke', (CRE + 0xF4, 4, (mem.load(CRE + 0x300, 4) + 121) & M32)
        if op == 'poke':
            mem.poke(*args)
            lines.append('POKE ' + ' '.join(f'{x:x}' for x in args))
            continue
        try:
            result = runner(op, args)
        except NotImplementedError as exc:
            error = str(exc)
        except Exception as exc:             # BadTypeid (ee_emu.EmuError) and the like
            if type(exc).__name__ not in ('BadTypeid',):
                raise
            error = str(exc)
        lines.append('OP ' + op + ' ' + enc(tuple(args)))
        for name, cargs, value in answers.log:
            lines.append('CALL ' + name + ' ' + enc(tuple(a & M32 if isinstance(a, int) else a for a in cargs))
                         + ' ' + enc(value))
        if len(model.calls) != len(answers.log):
            raise SystemExit('a call-out bypassed the answer log')
        lines.append('DIFF ' + ' '.join(f'{a:x}:{v:x}' for a, v in diff(before, snapshot(mem))))
        lines.append(f'DRAWS {model.draws - draws}')
        if result is not None:
            lines.append('RESULT ' + enc(result if isinstance(result, tuple) else result & M32))
        if error is not None:
            lines.append('REFUSED ' + error.replace('\n', ' '))
            break


def antqueen_case(mods, index, rng):
    C, RC, CT, AQ, CT2 = mods
    C.set_fpu('ps2')
    mem = RC.Memory()
    scenario = {'enemy': rng.randrange(2), 'sight': rng.choice((0.1, 0.4, 0.8))}
    build_image(CT, RC, C, mem, rng, 'antqueen')
    answers = Answers(AQ, C, rng, mem, scenario)
    model = AQ.Model(mem, answers)
    mem.poke(CRE + 0x44, 4, rng.getrandbits(32))
    pos = (C.f32(float(rng.randrange(-200, 200))) & M32, C.f32(float(rng.randrange(-200, 200))) & M32, 0)
    text(mem, TEXT, rng.choice((b'AntQueen', b'antqueen')))
    lines = [f'CASE aq-{index}', 'CLASS antqueen',
             'MEM ' + ' '.join(f'{a:x}:{v:x}' for a, v in sorted(mem.bytes.items()))]
    ops = [('ctor', (TEXT, pos, rng.randrange(0x10000), 0))]
    if rng.randrange(2):
        ops.append(('poke', (CRE + 0x234, 4, NODE)))
    for _ in range(rng.randrange(10, 30)):
        roll = rng.randrange(20)
        if roll < 9:
            ops.append(('run', ()))
        elif roll < 11:
            ops.append(('set', (rng.choice(AQ_REQUESTS),)))
        elif roll == 11:
            ops.append(('fly', ()))
        elif roll == 12:
            ops.append(('poke', (CRE + 0x232, 2, rng.choice(AQ_STATES))))
        elif roll == 13 and rng.randrange(2):
            ops += [('poke', (CRE + 0x232, 2, 0x19)), ('poke', (CRE + 0x234, 4, NODE)), ('wave', ()),
                    ('run', ())]
        else:
            off, size = rng.choice(((0xF4, 4), (0x3D4, 4), (0x3D8, 4), (0x118, 1), (0x11A, 2), (0x2C4, 4),
                                    (0x3CC, 4), (0x234, 4), (0x314, 2), (0x31C, 2), (0x318, 2), (0xA8, 8),
                                    (0x11C, 4), (0x13C, 4)))
            if off == 0x234:
                value = rng.choice((0, NODE, NODE))
            elif off in (0xF4, 0x3D4, 0x3D8):
                clock = mem.load(CRE + 0x300, 4) if mem.load(CRE + 0x300, 4) else 0
                value = (clock + rng.randrange(-3, 4)) & M32
            elif off == 0x118:
                value = rng.choice((0, 1, 3, 4, 5, 0xFF))
            elif off == 0x11A:
                value = rng.choice((0, 8, 0x10, 0x18, 0x80))
            elif off == 0x2C4:
                value = rng.choice((0, 30, 31, 32))
            elif off == 0x3CC:
                value = rng.choice((0, 1))
            elif off in (0x11C, 0x13C):
                d = mem.load(CRE + 0x24, 4)
                value = mem.load(d + rng.choice((0xB4, 0x44, 0xBC, 0x4C, 0xAC)), 4) if d else 0
            elif off == 0xA8:
                value = rng.getrandbits(64) & 0x8000000000000000 | rng.choice((0, 0x200, 0x1000000))
            else:
                value = rng.choice((0, 0, 5))
            ops.append(('poke', (CRE + off, size, value)))

    def runner(op, args):
        if op == 'ctor':
            return model.antqueen_ctor(CRE, *args)
        if op == 'run':
            RUN_STATES[mem.load(CRE + 0x232, 2)] = RUN_STATES.get(mem.load(CRE + 0x232, 2), 0) + 1
            return model.antqueen_run(CRE)
        if op == 'set':
            return model.antqueen_set_state(CRE, *args)
        if op == 'fly':
            return tuple(model.antqueen_pick_fly_dest(CRE))
        raise ValueError(op)
    run_ops(model, answers, mem, ops, lines, runner)
    return lines


def creature_case(mods, index, rng, cls):
    C, RC, CT, AQ, CT2 = mods
    C.set_fpu('ps2')
    mem = RC.Memory()
    scenario = {'enemy': 0, 'sight': 0.3, 'syn_desc': 0}
    build_image(CT, RC, C, mem, rng, cls)
    others = []
    for k, ocls in enumerate(rng.sample(sorted(OTHER_CLASSES), 5)):
        others.append(place_other(C, mem, rng, k, ocls, scenario))
    if not scenario['syn_desc']:
        desc = SYN_DESC + 0x800
        text(mem, desc + 0x10, b'8Skeleton')
        mem.poke(desc, 4, desc + 0x10)
        scenario['syn_desc'] = desc
    answers = Answers(CT2, C, rng, mem, scenario)
    model = CT2.Model(mem, answers)
    mem.poke(CRE + 0x44, 4, rng.getrandbits(32))
    pos = (C.f32(float(rng.randrange(-200, 200))) & M32, C.f32(float(rng.randrange(-200, 200))) & M32, 0)
    if cls == 'skeleton':
        text(mem, TEXT, rng.choice(SKEL_TYPES))
        ctor = ('ctor', (pos, rng.randrange(0x10000), 0, TEXT, rng.randrange(4)))
    elif cls == 'soul':
        text(mem, TEXT, b'Soul')
        ctor = ('ctor', (TEXT, pos, rng.randrange(0x10000), 0, OWNER if rng.randrange(4) == 0 else 0))
    else:
        text(mem, TEXT, b'AntQueen')
        ctor = ('ctor', (TEXT, pos, rng.randrange(0x10000), 0))
    # A property list with name= entries, and attached spells.
    entries = [b'NAME=one', b'name=two', b'nam=x', b'other=3', b'name=three', b'name']
    rng.shuffle(entries)
    addr = PROP_TEXT
    for k, e in enumerate(entries[:rng.randrange(0, 6)]):
        text(mem, addr, e)
        mem.poke(PROPS + 4 * k, 4, addr)
        addr += 0x20
    spells = []
    for k in range(rng.choice((0, 0, 1, 2))):
        node, spell = SPELLS + 0x100 * k, SPELLS + 0x100 * k + 0x40
        mem.poke(node + 0xC, 4, spell if rng.randrange(4) else 0)
        mem.poke(spell + 0x14, 4, SPELLS + 0x800)
        spells.append(node)
    mem.poke(SPELLS + 0x800 + 8 * 1 + 4, 4, 0x3CF0C4)
    scenario['spells'] = spells
    lines = [f'CASE {cls}2-{index}', 'CLASS ' + cls,
             'MEM ' + ' '.join(f'{a:x}:{v:x}' for a, v in sorted(mem.bytes.items()))]
    ops = [ctor]
    if rng.randrange(3):
        ops.append(('poke', (CRE + 0x2C, 4, PROPS)))
    for _ in range(rng.randrange(4, 14)):
        roll = rng.randrange(10)
        if roll < 4 and cls == 'skeleton':
            ops.append(('enemy', (rng.choice(others + [0, ENEMY]),)))
        elif roll < 7:
            attacker = rng.choice(others + [0, ENEMY] if cls == 'soul' else others + [ENEMY])
            ops.append(('damage', (attacker, rng.choice((0, 1, 0x400000)),
                                   C.f32(float(rng.choice((1, 5, 10, 200)))) & M32)))
        elif roll < 9:
            off, size, value = rng.choice(((0x312, 2, rng.choice((0, 0, 30))), (0x3D4, 1, rng.choice((0, 1, 2))),
                                           (0x228, 4, C.f32(rng.choice((0.0, 50.0))) & M32),
                                           (0x231, 1, rng.choice((0, 0, 1))),
                                           (0x310, 1, rng.choice((0, 1, 3)))))
            ops.append(('poke', (CRE + off, size, value)))
        else:
            o = rng.choice(others)
            ops.append(('poke', (o + 0xC, 4, rng.choice((0x20, 0x28, 0x60)))))
    ops.append(('dtor', (rng.randrange(4),)))

    def runner(op, args):
        if op == 'ctor':
            return getattr(model, cls + '_ctor')(CRE, *args)
        if op == 'enemy':
            return model.vcall(CRE, 20, *args)
        if op == 'damage':
            return model.vcall(CRE, 2, *args)
        if op == 'dtor':
            return model.vcall(CRE, 1, *args)
        raise ValueError(op)
    run_ops(model, answers, mem, ops, lines, runner)
    return lines


def gameobject_case(mods, index, rng, PC):
    C, RC, CT, AQ, CT2 = mods
    C.set_fpu('ps2')
    mem = RC.Memory()
    scenario = {}
    answers = Answers(CT2, C, rng, mem, scenario)
    model = CT2.GOModel(mem, answers)
    for ch in range(ord('A'), ord('Z') + 1):
        mem.poke(CT.M1.G_CTYPE + ch, 1, 1)
    text(mem, 0x5321B0, b'name')
    cls, table, dtor = rng.choice((('fire', 0x50F208, 0x3C5050), ('loosefire', 0x50E2B8, 0x3C6778),
                                   ('gas', 0x50D500, 0x3C7878)))
    mem.poke(table + 8 + 4, 4, dtor)
    mem.poke(CRE + 0x48, 4, table)
    mem.poke(CRE + 0x4C, 4, rng.choice((0, NODE)))
    entries = [b'name=a', b'NAME=b', b'kind=c']
    addr = PROP_TEXT
    for k, e in enumerate(entries[:rng.randrange(0, 4)]):
        text(mem, addr, e)
        mem.poke(PROPS + 4 * k, 4, addr)
        addr += 0x20
    if rng.randrange(2):
        mem.poke(CRE + 0x2C, 4, PROPS)
    lines = [f'CASE go-{index}', 'CLASS ' + cls,
             'MEM ' + ' '.join(f'{a:x}:{v:x}' for a, v in sorted(mem.bytes.items()))]

    def runner(op, args):
        return model.vcall(CRE, 1, *args)
    run_ops(model, answers, mem, [('dtor', (rng.randrange(4),))], lines, runner)
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    import ref_core as C
    import ref_creature as RC
    import ref_combat_ctors as CT
    import ref_antqueen_run as AQ
    import ref_combat_ctors2 as CT2
    import ref_potion_ctors as PC
    CT2_TYPEINFO.update(CT2.TYPEINFO)
    mods = (C, RC, CT, AQ, CT2)
    rng = random.Random(20973)
    lines = [f'# combat ctors 2 (ref_antqueen_run, ref_combat_ctors2 at {COMMIT[:7]})']
    counts = {}
    for index in range(200):
        lines += antqueen_case(mods, index, random.Random(rng.getrandbits(64)))
        counts['antqueen_run'] = counts.get('antqueen_run', 0) + 1
    for cls in ('skeleton', 'soul', 'antqueen'):
        for index in range(80):
            lines += creature_case(mods, index, random.Random(rng.getrandbits(64)), cls)
            counts[cls] = counts.get(cls, 0) + 1
    for index in range(40):
        lines += gameobject_case(mods, index, random.Random(rng.getrandbits(64)), PC)
        counts['gameobject'] = counts.get('gameobject', 0) + 1
    text_out = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    blob = gzip.compress(text_out, mtime=0)
    (out / 'combat_ctors_2.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-ctors-2-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text_out).hexdigest(),
        'cases': counts,
        'operations': text_out.count(b'\nOP '),
        'calls': text_out.count(b'\nCALL '),
        'antqueen_run_states': {f'{k:#x}': v for k, v in sorted(RUN_STATES.items())},
    }
    (out / 'combat_ctors_2.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(json.dumps(counts), meta['operations'], 'operations', meta['calls'], 'calls', len(text_out), 'bytes')


if __name__ == '__main__':
    main()
