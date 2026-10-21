#!/usr/bin/env python3
"""Exact round-5 fixtures from research commit 2980af1, never from native output.

The compared words are the ones both sides compute. Volley cases plant a zero
velocity so the real move and the research collide stub leave the bolt where
it was planted. Handle addresses are not compared.
"""
import gzip
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '2980af166632b1c7fa585be6880c2f2dd506bfa7'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat'
PINS = {
    'docs/spec/combat-round5.md': '88c15e5fb9de7bcfee7067c64258cc5acfa14ff0ef3836957e937bfdc1e4b566',
    'docs/combat-runtime.md': 'd65d84f71fdf75519705c2f21d72a8a56ef700188b8d102dbf18d21402f7b9ef',
    'tools/ref_combat_round5.py': '488ae487234fba45bd1aa0e1c89b6ef740e4af7355d698230059895c1369bced',
    'tools/test_ref_combat_round5.py': '9929eda89170347a739460a44d39053287fa5ea6209a5236b86c71004d7dbe71',
    'tools/difftest_combat_round5.py': 'c2c04846d1d6d5100aece39ab6ee3bc8761c6a47ad16ae408c0f1353f4b331f6',
    'tools/ref_anim.py': '6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_combat_round4.py': 'e4b83af89ac4d06d12be58b74259dc56664c8d1cf59cdb83098c15ace3c1e3dd',
    'tools/ref_combat_runtime.py': '328fbc428e26d50ac5cdb455d1614cf35148216151ba7904bb2ef92984e25ddd',
}
CREATURE, CLASSDEF = 0x1000, 0x2000


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


def word(value):
    return f'{value & 0xffffffff:08x}'


def words(values):
    return ' '.join(word(v) for v in values)


def queen(count, half, clips, vel=0, pos=0, keep=0x111):
    import ref_creature as RC
    import ref_combat_round5 as R5
    mem = RC.Memory()
    mem.poke(CREATURE + 0x24, 4, CLASSDEF)
    for off, value in zip(R5.BANK_CLIPS, (0x11, 0x22, 0x33, 0x44)):
        mem.poke(CLASSDEF + off, 4, value)
    mem.poke(CREATURE + 0x110, 2, half & 0xFFFF)
    mem.poke(CREATURE + 0x118, 1, count & 0xFF)
    mem.poke(CREATURE + 0x3E0, 4, vel)
    mem.poke(CREATURE + 0x3DC, 4, pos)
    planted = list(clips) + [0] * (2 - len(clips))
    for index in range(2):
        mem.poke(CREATURE + 0x11C + 32 * index, 4, planted[index])
        mem.poke(CREATURE + 0x124 + 32 * index, 4, keep)
    return mem, R5.QueenModel(mem, {})


def bank_case(name, count, half, clips):
    import ref_core as C
    C.set_fpu('ps2')
    mem, model = queen(count, half, clips)
    model.update_bank(CREATURE)
    out = [mem.load(CREATURE + off, size) for off, size in
           ((0x3E0, 4), (0x3DC, 4), (0x3E4, 2), (0x124, 4), (0x144, 4))]
    line = 'BANK ' + words([half & 0xFFFF, count & 0xFF, 0x11, 0x22, 0x33, 0x44,
                            clips[0], clips[1] if len(clips) > 1 else 0, 0, 0, 0x111])
    return [f'CASE {name}', line, 'OUT ' + words(out)]


def fade_case(name, count, clips):
    mem, model = queen(count, 0, clips, keep=0)
    model.fade_bank_tracks(CREATURE)
    out = [mem.load(CREATURE + off, 4) for off in (0x128, 0x148, 0x124)]
    line = 'FADE ' + words([count & 0xFF, 0x11, 0x22, 0x33, 0x44,
                            clips[0], clips[1] if len(clips) > 1 else 0])
    return [f'CASE {name}', line, 'OUT ' + words(out)]


def shield_case():
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round4 as R4
    import ref_combat_round5 as R5
    C.set_fpu('ps2')
    spell, caster, skill, model, pose = 0x5000, 0x6000, 0x7000, 0x3600, 0x3900
    mem = RC.Memory()
    mem.poke(spell, 1, 1)
    mem.poke(spell + 8, 4, skill)
    mem.poke(spell + 0xC, 4, caster)
    mem.poke(spell + 0x10, 4, 1)
    mem.poke(spell + 0x1C, 4, C.ONE)
    mem.poke(spell + 0x24, 4, 7)
    mem.poke(caster + 0x10, 4, 0x41200000)
    mem.poke(caster + 0x14, 4, 0x41A00000)
    mem.poke(caster + 0x18, 4, 0x41F00000)
    mem.poke(caster + 0x38, 2, 0x1000)
    mem.poke(caster + 0x114, 4, pose)
    mem.poke(pose + 8, 2, 1)
    mem.poke(pose + 0x10, 2, 0x1000)
    mem.poke(pose + 0x18, 2, 0x1000)
    mem.poke(pose + 0x20, 2, 0x1000)
    mem.poke(model + 0x24, 1, 6)
    mem.poke(model + 0x64, 4, 1)
    mem.poke(model + 0x80, 2, 16)

    class Skill:
        def get(self, name):
            def answer(*_args):
                if name == 'Creature.v16':
                    return model
                if name == 'Skill.v10':
                    return 100
                return 0
            return answer

    model_obj = R5.ShieldPoints(mem, Skill())
    got = model_obj.holy_shield_sample(spell)
    lines = ['CASE shield', f'RESULT {word(got)}']
    for name, args in model_obj.calls:
        if name == 'particle_spawn':
            definition, point, direction, speed = args
            lines.append('PARTICLE ' + words((definition, *point, *direction, speed)))
    return lines


def volley_case(name, ended, age, seed, collide, ground):
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round5 as R5
    C.set_fpu('ps2')
    obj = 0x4000
    mem = RC.Memory()
    mem.poke(obj + 0x60, 4, ended)
    mem.poke(obj + 0x6C, 4, age)
    mem.poke(obj + 0x44, 4, seed)
    mem.poke(obj + 0x74, 4, 0xAB)
    for i, value in enumerate((0x41000000, 0x41800000, 0x41F00000)):
        mem.poke(obj + 0x10 + 4 * i, 4, value)
        mem.poke(obj + 0x54 + 4 * i, 4, 0)

    class Move:
        def get(self, which):
            def answer(*_args):
                return collide if which == 'obj_moveAndCollide' else 0
            return answer

    model = R5.VolleyModel(mem, Move())
    model.multifire_run(obj)
    lines = [f'CASE {name}',
             'VOLLEY ' + words([ended, age, seed, ground])]
    lines.append(f'AGE {word(mem.load(obj + 0x6C, 4))}')
    for call, args in model.calls:
        if call == 'MultiFireProjectile_end':
            lines.append(f'END {word(args[1])}')
        elif call == 'particle_spawn':
            definition, point, direction, speed = args
            lines.append('PARTICLE ' + words((definition, *point, *direction, speed)))
    return lines


def shield_pose_case():
    """Two joints, two sample points, a facing and a position that are not the
    identity, so a missed joint matrix or facing turn moves the particle."""
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round5 as R5
    C.set_fpu('ps2')
    spell, caster, skill, model, pose = 0x5000, 0x6000, 0x7000, 0x3600, 0x3900
    facing, pos = 0x2345, (0xC1480000, 0x42200000, 0x40400000)
    points = ((16, -32, 48, 0x0104), (100, 7, -20, 0x0010))
    joints = (((0x1000, 0, 0, 0x40), (0, 0x1000, 0, -0x80), (0, 0, 0x1000, 0x20)),
              ((0, -0x1000, 0, 0x40), (0x1000, 0, 0, 0), (0, 0, 0x800, 0x80)))
    mem = RC.Memory()
    mem.poke(spell, 1, 1)
    mem.poke(spell + 8, 4, skill)
    mem.poke(spell + 0xC, 4, caster)
    mem.poke(spell + 0x10, 4, 0x2468)
    mem.poke(spell + 0x1C, 4, C.ONE)
    mem.poke(spell + 0x24, 4, 7)
    for i, value in enumerate(pos):
        mem.poke(caster + 0x10 + 4 * i, 4, value)
    mem.poke(caster + 0x38, 2, facing)
    mem.poke(caster + 0x114, 4, pose)
    mem.poke(pose + 8, 2, len(joints))
    flat = []
    for j, rows in enumerate(joints):
        for r, row in enumerate(rows):
            for col, value in enumerate(row):
                mem.poke(pose + 0x10 + j * 24 + r * 8 + 2 * col, 2, value & 0xFFFF)
                flat.append(value & 0xFFFF)
    mem.poke(model + 0x24, 1, 6)
    mem.poke(model + 0x64, 4, len(points))
    for i, point in enumerate(points):
        for col, value in enumerate(point):
            mem.poke(model + 0x80 + 8 * i + 2 * col, 2, value & 0xFFFF)

    class Skill:
        def get(self, name):
            def answer(*_args):
                if name == 'Creature.v16':
                    return model
                if name == 'Skill.v10':
                    return 100
                return 0
            return answer

    model_obj = R5.ShieldPoints(mem, Skill())
    got = model_obj.holy_shield_sample(spell)
    lines = ['CASE shield-pose',
             'SHIELDSET ' + words([facing, *pos, 0x2468, len(points)]
                                  + [v & 0xFFFF for point in points for v in point]
                                  + [len(joints)] + flat),
             f'RESULT {word(got)}']
    for name, args in model_obj.calls:
        if name == 'particle_spawn':
            definition, point, direction, speed = args
            lines.append('PARTICLE ' + words((definition, *point, *direction, speed)))
    return lines


def demon_case():
    """Soul_isDemonNear: the box arguments for one position, and the answer for
    lists of (demon, object flag 8) entries."""
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round5 as R5
    C.set_fpu('ps2')
    creature, demon_token = 0x1000, 0xD000
    pos = (0x41200000, 0xC2C80000, 0x3F800000)
    lists = ((), ((0, 0),), ((0, 0), (1, 0)), ((1, 1),), ((1, 1), (1, 0)), ((0, 1), (0, 0)))
    lines = ['CASE demon']
    for entries in lists:
        mem = RC.Memory()
        for i, value in enumerate(pos):
            mem.poke(creature + 0x10 + 4 * i, 4, value)
        objects = [0x3000 + 0x40 * i for i in range(len(entries))]
        tokens = {}
        for obj, (demon, flag) in zip(objects, entries):
            mem.poke(obj + 0xC, 4, 8 if flag else 0)
            tokens[obj] = demon_token if demon else 0xE000 + obj
        box = []

        class Calls:
            def get(self, name):
                def answer(*args):
                    if name == 'world_collectObjectsInBox':
                        box.append(args)
                    if name == 'Creature.v0':
                        return tokens[args[0]]
                    if name == 'Demon_typeinfo':
                        return demon_token
                    if name == 'rtti_typeInfoEquals':
                        return 1 if args[0] == args[1] else 0
                    return 0
                return answer

        got = R5.SoulModel(mem, Calls()).is_demon_near(creature, objects)
        low, high, limit = box[0]
        if not lines[1:]:
            lines.append('DEMONBOX ' + words([*pos, *low, *high, limit]))
        lines.append('DEMONLIST ' + words([got] + [d * 2 + f for d, f in entries]))
    return lines


def soul_phase_rows():
    import ref_combat_round5 as R5
    lines = ['CASE soul-phase']
    for state, frame, serial in ((0, 9, 9), (0, 1, 0), (0, 0, 0), (0, 16, 0), (0, 16, 1),
                                 (0, 9, 0), (0x18, 0, 0), (0x18, 1, 0), (0, 0x29, 0x19)):
        due = R5.soul_demon_due(frame, serial)
        lines.append('SOULPHASE ' + words([state, frame, serial, 1 if due else 0]))
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    import ref_core as C
    import ref_combat_round5 as R5
    C.set_fpu('ps2')
    lines = ['# combat-runtime-5', 'CASE angles']
    for hit in (0, 1, 0x8000, 0x7FFF):
        lines.append(f'HIT {word(hit)} {word(R5.hit_effect_angle(hit))}')
    for facing in (0x1234, 0xC000, 0x8000):
        lines.append(f'FACE {word(facing)} {word(R5.facing_angle(facing))}')
    lines.append('GIBHEAD ' + words([0, 0, 16000, 0xE0C0, R5.gib_heading(0, 0, 16000, 0xE0C0)]))
    angles, seed = R5.gib_angles(0x12345678, 0x100)
    lines.append('GIBSEED ' + word(seed))
    lines.append('GIBANGLE ' + words(angles))
    lines += bank_case('bank-zero', 2, 0, (0x11, 0x33))
    lines += bank_case('bank-pos', 2, 100, (0x11, 0x33))
    lines += bank_case('bank-neg', 2, -100 & 0xFFFF, (0x11, 0x33))
    lines += bank_case('bank-count0', 0, 100, (0x11, 0))
    lines += bank_case('bank-countff', 0xFF, 100, (0x11, 0))
    lines += fade_case('fade-match', 2, (0x11, 0x99))
    lines += fade_case('fade-zero', 0, (0x11, 0))
    lines += fade_case('fade-ff', 0xFF, (0x11, 0))
    lines += ['CASE shield-zero', 'REFUSE']
    lines += shield_case()
    lines += shield_pose_case()
    lines += demon_case()
    lines += soul_phase_rows()
    lines += volley_case('volley-clear', 0, 0, 0x1234, 0, 0)
    lines += volley_case('volley-age', 0, 600, 1, 0, 0)
    lines += volley_case('volley-hit', 0, 0, 9, 1, 0x43000000)
    lines += volley_case('volley-ended', 1, 5, 1, 0, 0)
    text = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text)
    (out / 'combat_runtime_5.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-runtime-5-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'cases': text.count(b'\nCASE '),
    }
    (out / 'combat_runtime_5.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'{meta["cases"]} cases, {len(text)} bytes')


if __name__ == '__main__':
    main()
