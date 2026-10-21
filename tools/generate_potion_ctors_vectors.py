#!/usr/bin/env python3
"""Exact thrown-potion ending fixtures from research commit e9777eb (potion work
98c081a), never from native output.

Each case runs tools/ref_potion_ctors.py (docs/spec/potion-ctors.md) on an authored
image: LooseFire_ctor 0x202b70 with fx_spawn id 5 and Fire_ctor 0x2020e0, Fire_run
0x1f34e0, LooseFire_run 0x202c48, LooseFire_onTouch 0x1fcb70 and poisonGasSpell_ctor
0x202de8. The pool gives every block (the native World's pool does); handle words and
block addresses are not compared, the words each body stores are.

The fx_spawn definition words (0x4f87bc + 4i) are planted as made, as the research's
own synthetic tests do: their first-use construction is presentation.
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
    'docs/spec/potion-ctors.md': 'db27aa470106689a9f073f3beadc7c37a6bb8d6a49896c67a5efea25e52d9364',
    'tools/ref_potion_ctors.py': '4b7a2520c01f7acf831ec0041384d5af9aec4a4466c671018ad63de0f249af42',
    'tools/test_ref_potion_ctors.py': 'db3dfd87d12c3d1e07f791fc04145eeb64053927289b094726f422b8219f1dc0',
    'tools/difftest_potion_ctors.py': 'b9dac01c745c9eb27157668d654ab1961a729d017df2b1e10cab9c4d30b88d8c',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_skills_a.py': '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae',
}
M32 = 0xFFFFFFFF
OBJ, OWNER_NODE = 0x100000, 0x200000
BLOCKS = [0x110000 + 0x100 * i for i in range(8)]


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


def word(v):
    return f'{v & M32:08x}'


def words(vs):
    return ' '.join(word(v) for v in vs)


class Answers:
    def __init__(self, answers=None):
        self.calls = []
        self.pool = list(BLOCKS)
        self.answers = answers or {}

    def get(self, name):
        def answer(*args):
            self.calls.append((name, args))
            if name == 'pool_new':
                return self.pool.pop(0)
            if name == 'handle_link':
                return 0
            return self.answers.get(name, 0)
        return answer


def model_on(PC, RC, C, seed, frame):
    C.set_fpu('ps2')
    mem = RC.Memory()
    cb = Answers()
    m = PC.Model(mem, cb)
    for i in range(PC.FX_DEF_COUNT):
        mem.poke(PC.FX_DEF_FLAGS + 4 * i, 4, 1)
    mem.poke(PC.G_RAND_SEED, 4, seed)
    mem.poke(PC.G_SIM_FRAME, 4, frame)
    return mem, cb, m


def loosefire_case(PC, RC, C, name, seed, frame, pos, owner):
    mem, cb, m = model_on(PC, RC, C, seed, frame)
    owner_addr = 0x300000 if owner else 0
    m.loosefire_ctor(OBJ, pos, owner_addr)
    fires = [b for n, a in cb.calls if n == 'pool_new' for b in [None]]
    count = len(fires)
    lines = [f'CASE {name}', 'LOOSEFIRE ' + words([seed, frame, *pos, owner])]
    lines.append('OBJ ' + words([mem.load(OBJ + o, 4) for o in (0x0C, 0x1C, 0x20, 0x44, 0x48, 0x50)]
                                + [mem.load(OBJ + 0x28, 1), mem.load(OBJ + 0x29, 1)]))
    for block in BLOCKS[:count]:
        lines.append('FIRE ' + words([mem.load(block + 0x10, 4), mem.load(block + 0x14, 4),
                                      mem.load(block + 0x18, 4), mem.load(block + 0x44, 4),
                                      mem.load(block + 0x0C, 4), mem.load(block + 0x28, 1),
                                      mem.load(block + 0x48, 4), mem.load(block + 0x4C, 2)]))
    lines.append('SEED ' + words([mem.load(PC.G_RAND_SEED, 4), m.draws]))
    if m.draws != PC.loosefire_draws([True] * count):
        raise SystemExit('loosefire_draws disagrees with the model')
    return lines


def fire_run_case(PC, RC, C, name, life, frame, own, pos):
    mem, cb, m = model_on(PC, RC, C, 1, frame)
    mem.poke(OBJ + 0x4C, 2, life & 0xFFFF)
    mem.poke(OBJ + 0x44, 4, own)
    for i, v in enumerate(pos):
        mem.poke(OBJ + 0x10 + 4 * i, 4, v)
    mem.poke(OBJ + 0x48, 4, PC.VT_FIRE)
    mem.poke(PC.VT_FIRE + 8 * 1 + 4, 4, 0x3C5050)
    m.fire_run(OBJ)
    lines = [f'CASE {name}', 'FIRERUN ' + words([life, frame, own, *pos])]
    for n, a in cb.calls:
        if n == 'glow_add':
            lines.append('GLOW ' + words([*a[0], *a[1:]]))
        elif n == 'particle_spawn':
            lines.append('PARTICLE ' + words([a[0], *a[1], *a[2], a[3]]))
        elif n == 'Creature.v1' or n.endswith('.v1'):
            lines.append('DELETE')
    lines.append('AFTER ' + words([mem.load(OBJ + 0x4C, 2), mem.load(OBJ + 0x44, 4)]))
    if m.draws:
        raise SystemExit('Fire_run drew the shared seed')
    return lines


def loosefire_run_case(PC, RC, C, name, end, frame):
    mem, cb, m = model_on(PC, RC, C, 1, frame)
    mem.poke(OBJ + 0x50, 4, end)
    mem.poke(OBJ + 0x48, 4, PC.VT_LOOSEFIRE)
    m.loosefire_run(OBJ)
    deleted = any(n.endswith('.v1') for n, _a in cb.calls)
    return [f'CASE {name}', 'LFRUN ' + words([end, frame, int(deleted)])]


def touch_case(PC, RC, C, name, frame, owner_kind, other_kind, difficulty, enemy):
    """owner_kind 0 none, 1 a player; other_kind 0 plain object, 1 creature, 2 player."""
    mem, cb, m = model_on(PC, RC, C, 1, frame)
    cb.answers['Creature.v20'] = enemy
    owner, other = 0x300000, 0x400000
    mem.poke(PC.G_DIFFICULTY, 4, difficulty & M32)
    if owner_kind:
        mem.poke(OBJ + 0x4C, 4, OWNER_NODE)
        mem.poke(OWNER_NODE + 0xC, 4, owner)
        mem.poke(owner + 0xC, 4, 0x60)
    flags = {0: 0x4001, 1: 0x20, 2: 0x60}[other_kind]
    mem.poke(other + 0xC, 4, flags)
    m.loosefire_on_touch(OBJ, other)
    out = 'NONE'
    for n, a in cb.calls:
        if n == 'damage_apply':
            desc = a[6]
            entry = desc[1] if isinstance(desc, tuple) and len(desc) > 1 else None
            if entry is None:
                out = 'EMPTY ' + words([a[2], a[5], int(a[1] == owner)])
            else:
                out = 'ENTRY ' + words([entry[0], entry[1], entry[2], entry[3], a[2], a[5], int(a[1] == owner)])
    asked = sum(1 for n, _a in cb.calls if n == 'Creature.v20')
    return [f'CASE {name}', 'TOUCH ' + words([frame, owner_kind, other_kind, difficulty, enemy]),
            f'ASKED {asked}', 'OUT ' + out]


def gas_case(PC, RC, C, name, seed, frame, pos, life, radius, word68, owner, damage):
    mem, cb, m = model_on(PC, RC, C, seed, frame)
    m.poison_gas_ctor(OBJ, pos, life, radius, word68, 0x300000 if owner else 0, damage)
    lines = [f'CASE {name}', 'GAS ' + words([seed, frame, *pos, life, radius, word68, owner, damage])]
    lines.append('OBJ ' + words([mem.load(OBJ + o, 4) for o in
                                 (0x0C, 0x1C, 0x20, 0x44, 0x48, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C)]))
    lines.append('SEED ' + words([mem.load(PC.G_RAND_SEED, 4), m.draws]))
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    import ref_core as C
    import ref_creature as RC
    import ref_potion_ctors as PC
    rng = random.Random(98)
    f = lambda x: C.f32(x) & M32               # noqa: E731
    lines = ['# thrown potion endings (ref_potion_ctors at e9777eb)']
    for k in range(24):
        pos = (f(rng.randrange(-300, 300) + 0.5), f(rng.randrange(-300, 300)), f(rng.randrange(0, 40)))
        lines += loosefire_case(PC, RC, C, f'loosefire-{k}', rng.getrandbits(32), rng.randrange(0, 5000), pos,
                                k % 2)
    for k, life in enumerate((100, 99, 81, 80, 60, 21, 20, 19, 2, 1, 0, -3, 0x7FFF)):
        pos = (f(rng.randrange(-300, 300)), f(rng.randrange(-300, 300)), f(rng.randrange(0, 40)))
        lines += fire_run_case(PC, RC, C, f'fire-{k}', life & 0xFFFF, rng.randrange(0, 5000), rng.getrandbits(32), pos)
    for k, (end, frame) in enumerate(((220, 219), (220, 220), (220, 221), (100, 400), (0x7FFFFFF0, 5),
                                      (5, 0x80000005))):
        lines += loosefire_run_case(PC, RC, C, f'lfrun-{k}', end, frame)
    k = 0
    for frame in (0, 20, 40, 21, 39, 0x7FFFFFEC):
        for owner_kind, other_kind, enemy in ((0, 2, 0), (0, 1, 0), (0, 0, 0), (1, 1, 1), (1, 2, 0)):
            for difficulty in (0, 2, 5, 6, -1):
                if other_kind != 2 and difficulty not in (0,):
                    continue
                lines += touch_case(PC, RC, C, f'touch-{k}', frame, owner_kind, other_kind, difficulty, enemy)
                k += 1
    for k, (life, radius, word68, damage) in enumerate(((300, 20, 0, 5), (300, 9, 0, 5), (1, 10, 7, 0),
                                                         (60, -4 & M32, 3, 9), (300, 0x7FFFFFFF, 0, 5))):
        pos = (f(rng.randrange(-300, 300)), f(rng.randrange(-300, 300)), f(rng.randrange(0, 40)))
        lines += gas_case(PC, RC, C, f'gas-{k}', rng.getrandbits(32), rng.randrange(0, 5000), pos, life, radius,
                          word68, k % 2, damage)
    text = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    blob = gzip.compress(text, mtime=0)
    (out / 'potion_ctors.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'potion-ctors-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'cases': text.count(b'\nCASE '),
    }
    (out / 'potion_ctors.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(meta['cases'], 'cases', len(text), 'bytes')


if __name__ == '__main__':
    main()
