#!/usr/bin/env python3
"""Exact world_findFreeSpot fixtures from research commit 13fafa4, never from native output.

Each case runs ref_objects.World.find_free_spot (0x14acf8; docs/level-flow.md 17.2 and
18.3, docs/objects.md) on authored objects, an authored shared seed and an authored
two-rule terrain. The terrain rules are the fixture's own, not level data:

  terrain_testFlag1(radius, p) = 1.0 when p.x < BX, else 0
  terrain_height(radius, p)    = HI when p.y < HY, else LO
"""
import gzip
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '13fafa465d5dad7acaeb1ee8becd30c4c8e4788b'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-levels'
PINS = {
    'docs/level-flow.md': 'db46102cadc7a41f704068601c6d710caee7f6d64796d99c11e54babcfd13526',
    'docs/objects.md': '9b23931068b93dc69be2e2eca93fe688f8e7acbd6ee9ab143b1dfef9b6513dd8',
    'tools/ref_objects.py': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
}
ONE = 0x3F800000
NEG_INF, POS_INF = 0xFF800000, 0x7F800000
SENTINEL = 0xC9742400


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


def f(x):
    import ref_core as R
    return R.from_int(x & 0xffffffff)


def case(name, seed, objects, terrain, centre, spread, radius, any_height):
    import ref_core as R
    import ref_objects as O
    world = O.World()
    for index, (pos, rad, height, flags) in enumerate(objects):
        obj = world.construct(0x100000 + 0x400 * index, pos)
        obj.radius, obj.height, obj.flags = rad, height, flags
    bx, hy, hi, lo = terrain
    blocked = lambda r, p: ONE if R.lt(p[0], bx) else 0
    height = lambda r, p: hi if R.lt(p[1], hy) else lo
    world.shared.state = seed
    pos, ok = world.find_free_spot(list(centre), spread, radius, any_height, blocked, height)
    lines = [f'CASE {name}', 'SEED ' + word(seed)]
    lines += ['OBJ ' + words(list(p) + [r, h, fl]) for p, r, h, fl in objects]
    lines.append('TERRAIN ' + words(terrain))
    lines.append('CALL ' + words(list(centre) + [spread, radius, any_height]))
    lines.append('OUT ' + words(list(pos) + [ok, world.shared.state]))
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    import ref_core as R
    R.set_fpu('ps2')
    F24, F72 = 0x41C00000, 0x42900000
    open_ground = (NEG_INF, NEG_INF, 0, 0)
    stuck = (f(80), 0, 0)
    fight = [((0, 0, 0), f(16), F72, 0), ((f(80), 0, 0), f(12), F72, 0),
             ((f(-40) & 0xffffffff, 0, 0), f(12), F72, 0)]
    ring = [((f(80 + dx) & 0xffffffff, f(dy) & 0xffffffff, 0), F24, F72, 0)
            for dx, dy in ((0, 0), (30, 0), (-30, 0), (0, 30), (0, -30), (40, 40), (-40, -40))]
    lines = ['# combat free spot']
    lines += case('open', 0x00000000, [], open_ground, stuck, 72, 12, 0)
    lines += case('stuck-fight', 0x9E3779B9, fight, open_ground, stuck, 72, 12, 0)
    lines += case('ring', 0x00012345, ring, open_ground, stuck, 72, 12, 0)
    lines += case('all-blocked', 0x00000007, [], (POS_INF, NEG_INF, 0, 0), stuck, 72, 12, 0)
    lines += case('floor-six', 0x00000007, [], (NEG_INF, NEG_INF, 0, 0x40C00000), stuck, 72, 12, 0)
    lines += case('floor-six-any', 0x00000007, [], (NEG_INF, NEG_INF, 0, 0x40C00000), stuck, 72, 12, 1)
    lines += case('floor-below', 0x00000011, [], (NEG_INF, NEG_INF, 0, SENTINEL), stuck, 72, 12, 0)
    lines += case('not-solid', 0x00000011, [((f(80), 0, 0), f(200), F72, 1)], open_ground, stuck, 72, 12, 0)
    lines += case('solid-cover', 0x00000011, [((f(80), 0, 0), f(200), F72, 0)], open_ground, stuck, 72, 12, 0)
    lines += case('mixed', 0x0BADF00D, ring[:3], (f(60), 0, 0x41200000, 0), stuck, 72, 12, 0)
    lines += case('wide', 0x00C0FFEE, fight, (f(-100) & 0xffffffff, NEG_INF, 0, 0), (0, 0, 0), 240, 24, 0)
    text = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text, mtime=0)
    (out / 'combat_free_spot.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-free-spot-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'cases': text.count(b'\nCASE '),
    }
    (out / 'combat_free_spot.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'{meta["cases"]} cases, {len(text)} bytes')


if __name__ == '__main__':
    main()
