#!/usr/bin/env python3
"""Exact round-3 fixtures from research commit 22649cd, never from native output.

The compared words are the ones both sides compute with the same object seed and a
velocity of 0, so the real move (native) and the research collide stub leave the
arrow where it was planted. Handle addresses and the shared seed are not compared:
LightEffect_ctor and damage_apply draw the shared generator inside call-outs the
research stubs do not run.
"""
import gzip
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '22649cdac2a0b8d5e8803d29eb20db99defbd5e1'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat'
PINS = {
    'tools/ref_combat_round3.py': '7a0e7706a2680c66189d7c52ddba87e3930d58ae727226aea9b28fb5550163e2',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_skills_a.py': '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae',
    'tools/ref_combat_ranged.py': '7755ff203383b92d39a190475b5704fb3fc512abcc107924e09189913d42bda5',
    'tools/test_ref_combat_round3.py': '10a08fcb1a9dd16954bd19eb647c91a28abe1c5f54e6b79c9d516ac766583aa7',
    'tools/difftest_combat_round3.py': 'deeaf1a22b38ae729b7c239c9ff3d636c9a818111c8bc1e5b80c29de4ff6a817',
    'docs/spec/combat-round3.md': '075c537c03a8df1efd2a461609b962b8517e986c15bd5e0df4c0c87c959c2c37',
    'docs/combat-runtime.md': 'd1462c7a7c57d7618990cca77a19ffc6171e732b88272f0068d4db3bbe9cb40f',
}


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


def particles(calls):
    rows = []
    for name, args in calls:
        if name != 'particle_spawn':
            continue
        definition, point, velocity, _speed = args
        rows.append((definition, *point, *velocity))
    return rows


def cloud_case():
    import ref_core as C
    import ref_creature as RC
    import ref_skills_a as SA
    import ref_combat_round3 as R3
    C.set_fpu('ps2')
    cloud = 0x2000
    mem = RC.Memory()
    mem.store(SA.G_SIM_FRAME, 4, 61)
    calls = []

    class CB:
        def get(self, name):
            def answer(*args):
                calls.append((name, args))
                return 0
            return answer

    model = R3.Model(mem, CB())
    mem.store(cloud + 0x48, 4, 0x7000)
    mem.store(0x7000 + 12, 4, 0)
    mem.store(cloud + 0x44, 4, 0x12345678)
    mem.store(cloud + 0x54, 4, 1000)
    mem.store(cloud + 0x58, 4, 48)
    mem.store(cloud + 0x5C, 4, C.f32(10.0))
    for i, word in enumerate((C.f32(1.0), C.f32(2.0), C.f32(3.0))):
        mem.store(cloud + 0x10 + 4 * i, 4, word)
    model.diseasecloud_run(cloud)
    lines = ['CASE cloud', f'RADIUS {mem.load(cloud + 0x5C, 4):08x}']
    for row in particles(calls):
        lines.append('P ' + ' '.join(f'{w & 0xffffffff:08x}' for w in row))
    return lines


def arrow_case(name, kind, collide):
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round3 as R3
    C.set_fpu('ps2')
    missile = 0x9000
    mem = RC.Memory()
    mem.store(RC.G_SIM_FRAME, 4, 60)
    calls = []

    class CB:
        def get(self, name_):
            def answer(*args):
                calls.append((name_, args))
                if name_ == 'obj_moveAndCollide':
                    return collide
                if name_ == 'pool_new':
                    return 0x9300
                if name_ == 'res_find':
                    return 0xA10000
                return 0
            return answer

    model = R3.ArrowModel(mem, CB())
    mem.store(missile + 0x48, 4, 0x9100)
    mem.store(0x9100 + 12, 4, 0x151454)
    mem.store(missile + 0xD0, 4, kind)
    mem.store(missile + 0x44, 4, 0x1234)
    mem.store(missile + 0xD8, 4, 40)
    mem.store(missile + 0xEC, 4, 1000)
    for i, word in enumerate((C.f32(1.0), C.f32(2.0), C.f32(3.0))):
        mem.store(missile + 0x10 + 4 * i, 4, word)
    # Velocity 0: the native move then leaves the arrow on the planted point.
    model.missile_run(missile)
    lines = [f'CASE {name}', f'EXPIRY {mem.load(missile + 0xEC, 4):08x}']
    for row in particles(calls):
        lines.append('P ' + ' '.join(f'{w & 0xffffffff:08x}' for w in row))
    for call, args in calls:
        if call == 'fx_spawn':
            lines.append(f'FX {args[0] & 0xffffffff:08x}')
        elif call == 'fx_ringBurst':
            lines.append('RING ' + ' '.join(f'{w & 0xffffffff:08x}' for w in (*args[0], args[1])))
        elif call == 'LightEffect_ctor':
            lines.append('LIGHT ' + ' '.join(f'{w & 0xffffffff:08x}' for w in args[4:8]))
        elif call == 'res_find' and len(args) >= 2:
            lines.append(f'SAMPLE {args[1] & 0xffffffff:08x}')
        elif call == 'player_getPassiveLevel':
            lines.append(f'PASSIVE {args[0] & 0xffffffff:08x} {args[1] & 0xffffffff:08x}')
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    lines = ['# combat-runtime-3'] + cloud_case()
    lines += arrow_case('arrow5', 5, 0)
    lines += arrow_case('arrow1', 1, 1)
    lines += arrow_case('arrow2', 2, 1)
    lines += arrow_case('arrow6', 6, 1)
    text = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text)
    (out / 'combat_runtime_3.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-runtime-3-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'cases': text.count(b'\nCASE '),
    }
    (out / 'combat_runtime_3.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'{meta["cases"]} cases, {len(text)} bytes')


if __name__ == '__main__':
    main()
