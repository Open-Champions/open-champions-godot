#!/usr/bin/env python3
"""Replay the t3/research-wading reference cases into a native contract corpus.

Runs tools/ref_wading.py (research 942a6ba) on authored synthetic memory and writes
the observable results of Player_move step (f) and the footstep: the in-water bit,
the call order with arguments, the shared draws, the ripple grid cells, and the
sound_playFoley choice. No game data is read: the chunk worlds are constructed here
and the foley format strings are replaced by placeholders, as test_ref_wading.py does.

usage: generate_player_wading_vectors.py RESEARCH_DIR OUT_DIR
"""
import gzip
import hashlib
import json
import os
import sys

sys.dont_write_bytecode = True
research = os.path.abspath(sys.argv[1])
out_dir = os.path.abspath(sys.argv[2])
sys.path.insert(0, os.path.join(research, 'tools'))
import ref_core as C                                   # noqa: E402
import ref_creature as R                               # noqa: E402
import ref_combat_move as MV                           # noqa: E402
import ref_water_services as W                         # noqa: E402
import ref_wading as WD                                # noqa: E402

COMMIT = '942a6bac3543ad14ab350b210da65dc51e9c695a'
PINS_FILES = ('docs/spec/player-wading.md', 'tools/ref_wading.py', 'tools/test_ref_wading.py',
              'tools/difftest_wading.py', 'tools/ref_water_services.py')
WORLD, TREE, CHUNKS, PAYLOAD, GRID = 0x10000, 0x10100, 0x10200, 0x10400, 0x11000
P = 0x20000
SEED0 = 0x12345678
F = C.f32


def build_world(mem, surface, x0, width, cut):
    mem.poke(MV.G_WORLD, 4, WORLD)
    mem.poke(WORLD + 4, 4, TREE)
    mem.poke(WORLD + 0x24, 4, CHUNKS)
    mem.poke(TREE, 2, 1)
    mem.poke(TREE + 2, 2, 0)
    mem.poke(CHUNKS, 4, PAYLOAD)
    mem.poke(CHUNKS + 0x24, 4, 0x80)
    for k, v in enumerate((x0, -192.0, surface)):
        mem.poke(CHUNKS + 8 + 4 * k, 4, F(v))
    for k, v in enumerate((x0 + width, 192.0, surface)):
        mem.poke(CHUNKS + 0x14 + 4 * k, 4, F(v))
    mem.poke(CHUNKS + 0x28, 4, 0)
    mem.poke(CHUNKS + 0x2C, 4, 0)
    mem.poke(PAYLOAD + 8, 4, GRID)
    mem.poke(W.G_CUT_SKIP, 1, cut)


def grid_cells(mem):
    out = []
    for x in range(32):
        for y in range(32):
            v = R.s16(mem.load(GRID + 0xA90 + 2 * (32 * x + y), 2))
            if v:
                out.append('%d:%d:%d' % (x, y, v))
    return ','.join(out) or '-'


class Logging(WD.WadeModel):
    def __init__(self, mem, events, names=None):
        cb = self.callbacks(events)
        if names is not None:
            cb['res_find'] = lambda lump, name: names.append(name) or 0x777
        WD.WadeModel.__init__(self, mem, cb)
        self.events = events

    @staticmethod
    def callbacks(events):
        def res_find(*a):
            return 0x777

        def damage(*a):
            events.append('hurt')
            return 0

        def sound(entry, obj, volume):
            events.append('splash:%08x' % volume)
            return 0

        def burst(pos, one):
            events.append('burst:%08x:%08x:%08x' % tuple(pos))
            return 0
        return {'res_find': res_find, 'damage_apply': damage, 'sound_playOnObject': sound,
                'fx_burst_1fbdf0': burst, 'sound_playAtPos': lambda *a: 0}

    def draw(self):
        self.events.append('draw')
        return WD.WadeModel.draw(self)

    def wade_water_disturb(self, world, pos, strength, radius):
        touched = WD.WadeModel.wade_water_disturb(self, world, pos, strength, radius)
        self.events.append('disturb:%08x:%08x:%08x:%08x:%08x=%d' % (strength, radius, pos[0], pos[1], pos[2], touched))
        return touched


def setup_player(mem, pz, wet, frame, air, level_flags):
    for k, v in enumerate((0.0, 0.0, pz)):
        mem.poke(P + 0x10 + 4 * k, 4, F(v))
    mem.poke(P + 0xA8, 8, WD.WATER_BIT if wet else 0)
    mem.poke(P + 0x47C, 1, air)
    mem.poke(MV.G_SIM_FRAME, 4, frame)
    mem.poke(MV.G_LEVEL_FLAGS, 4, level_flags)
    mem.poke(MV.S_SPLASH_FMT, 6, int.from_bytes(b'sp%d\0\0', 'little'))
    mem.poke(WD.G_RAND_SEED, 4, SEED0)


def bit(mem):
    return 1 if mem.load(P + 0xA8, 8) & WD.WATER_BIT else 0


def run_step(surface, x0, width, pz, wet, frame, air, s, level_flags, cut):
    mem = R.Memory()
    events = []
    model = Logging(mem, events)
    build_world(mem, surface, x0, width, cut)
    setup_player(mem, pz, wet, frame, air, level_flags)
    model._player_water(P, s)
    return mem, events, model


def line(tag, fields, result):
    return '%s %s -> %s' % (tag, ' '.join('%s=%s' % kv for kv in fields), ' '.join('%s=%s' % kv for kv in result))


def h(v):
    return '%08x' % F(v)


def main():
    lines = []
    # 1. band: the executed band sweep (difftest_wading `band`): 2,652 cases.
    def steps(a, b, d):
        n = int(round((b - a) / d))
        return [a + i * d for i in range(n + 1)]
    for pz in (0.0, 37.5, -812.25):
        for wet in (0, 1):
            cases = [(dz, 0.0) for dz in steps(-20.0, 70.0, 0.25)]
            cases += [(10.0, d) for d in steps(0.0, 20.0, 0.25)]
            for dz, edge in cases:
                mem, ev, _m = run_step(pz + dz, edge, 384.0, pz, wet, 4 if wet else 0, 0, F(1.0), 0, 0)
                lines.append(line('band', [('pz', h(pz)), ('surf', h(pz + dz)), ('x0', h(edge)),
                                           ('wet', wet)], [('bit', bit(mem))]))
    # 2. step: the full call record, the seed and the grid.
    for width in (384.0, 120.0, 768.0):
        for dz in (-14.0, -12.0, 10.0, 50.0, 55.0):
            for wet in (0, 1):
                for air in (0, 1):
                    for frame in (0, 4, 5, 16, 32):
                        for s in (0.0, 0.5):
                            for lf in (0, 0x1000):
                                for cut in (0, 1):
                                    pz = 0.0
                                    mem, ev, m = run_step(pz + dz, -width / 2, width, pz, wet, frame, air, F(s), lf, cut)
                                    fields = [('width', h(width)), ('surf', h(dz)), ('wet', wet), ('air', air),
                                              ('frame', frame), ('s', h(s)), ('lf', '%x' % lf), ('cut', cut)]
                                    result = [('bit', bit(mem)), ('seed', '%08x' % mem.load(WD.G_RAND_SEED, 4)),
                                              ('events', ','.join(ev) or '-'), ('grid', grid_cells(mem))]
                                    lines.append(line('step', fields, result))
    # 3. seq: 32 consecutive frames (section 9 table).
    for dz, stick, wet in ((10, 1.0, 0), (10, 0.0, 0), (55, 1.0, 0), (-5, 1.0, 0), (-12, 1.0, 1),
                           (-14, 1.0, 1), (50, 1.0, 1)):
        mem = R.Memory()
        events = []
        model = Logging(mem, events)
        build_world(mem, float(dz), -192.0, 384.0, 0)
        setup_player(mem, 0.0, wet, 16, 0, 0)
        bits, writes, prev = '', 0, grid_cells(mem)
        for k in range(32):
            mem.poke(MV.G_SIM_FRAME, 4, 16 + k)
            model._player_water(P, F(stick))
            bits += str(bit(mem))
            now = grid_cells(mem)
            writes += 1 if now != prev else 0
            # a frame "wrote the grid" when any ripple touched a cell; the grids accumulate,
            # so compare the disturb events instead of the snapshot
            prev = now
        disturbs = [e for e in events if e.startswith('disturb')]
        touched = sum(1 for e in disturbs if not e.endswith('=0'))
        lines.append(line('seq', [('surf', h(float(dz))), ('s', h(stick)), ('wet', wet)],
                          [('bits', bits), ('draws', model.draws), ('disturbs', len(disturbs)),
                           ('touched', touched), ('seed', '%08x' % mem.load(WD.G_RAND_SEED, 4)),
                           ('grid', grid_cells(mem))]))
    # 4. foley: the footstep choice and the cosmetic seed.
    for item, flags in ((0, 0), (1, 0), (1, 0x40000), (1, 0x80000), (1, 0x100000), (1, 0x200000), (1, 0x400000),
                        (1, 0x600000), (1, 0x300000), (1, 0x780000), (1, 0xfff80000)):
        for wet in (0, 1):
            for seed in (0, 1, SEED0, 0xDEADBEEF, 0x7FFFFFFF, 0xFFFFFFFF):
                mem = R.Memory()
                events = []
                names = []
                model = Logging(mem, events, names)
                fmts = {WD.S_FOLEY_WATER: 'W', WD.S_FOLEY_PLAIN: 'P'}
                for bit_, f_, _n in WD.S_FOLEY_BY_FLAG:
                    fmts[f_] = 'F%x_' % bit_
                for addr, tag in fmts.items():
                    for i, b in enumerate((tag + '%d').encode() + b'\0'):
                        mem.poke(addr + i, 1, b)
                for i, b in enumerate(b'L\0'):
                    mem.poke(WD.S_FOLEY_LUMP + i, 1, b)
                mem.poke(P + 0x3C8, 4, 1)
                rec = WD.G_PLAYERS + WD.REC_SIZE
                if item:
                    mem.poke(rec + WD.REC_BOOTS_A, 4, 0x30000)
                    mem.poke(0x30008, 4, flags)
                mem.poke(P + 0xA8, 8, WD.WATER_BIT if wet else 0)
                mem.poke(WD.G_COSMETIC_SEED, 4, seed)
                mem.poke(WD.G_RAND_SEED, 4, SEED0)
                result = model.player_footstep(P)
                name = names[0].decode() if names else '-'
                lines.append(line('foley', [('item', item), ('flags', '%x' % flags), ('wet', wet), ('seed', '%08x' % seed)],
                                  [('name', name), ('result', '%x' % (result & 0xFFFFFFFF) if not names else 'audible'),
                                   ('cosmetic', '%08x' % mem.load(WD.G_COSMETIC_SEED, 4)),
                                   ('shared', '%08x' % mem.load(WD.G_RAND_SEED, 4))]))
    raw = ('\n'.join(lines) + '\n').encode()
    data = gzip.compress(raw, mtime=0)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'player_wading.txt.gz'), 'wb') as f:
        f.write(data)
    pins = {}
    for rel in PINS_FILES:
        with open(os.path.join(research, rel), 'rb') as f:
            pins[rel] = hashlib.sha256(f.read()).hexdigest()
    manifest = {'schema': 'player-wading-v1', 'research_commit': COMMIT, 'references': pins,
                'cases': len(lines), 'vector_sha256': hashlib.sha256(data).hexdigest(),
                'content_sha256': hashlib.sha256(raw).hexdigest()}
    with open(os.path.join(out_dir, 'player_wading.json'), 'w') as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write('\n')
    print(len(lines), 'cases')


if __name__ == '__main__':
    main()
