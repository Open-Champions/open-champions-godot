#!/usr/bin/env python3
"""Generate shared Skill foundation state vectors from a verified external pin.
Only authored sparse memory, statistic values and callback scripts are used.
"""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_skills_a.py': '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_items.py': '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e',
    'tools/difftest_skills_a.py': 'd91f55a4d6ad75dc878e3fad279f865abb679cbc34d3d28b25aab29da8728b30',
    'docs/spec/skills-01-34.md': '3e7ea2c519e301d5a70633d04763a9fec2c198ace5969f86b6ef12a9280f80aa',
    'docs/spec/skills-01-34-executed.md': 'ad12fac09365575437c123d2d0c78ef364a7358a97d37fe5895fdf865a4bd60c',
}
MASK = 0xffffffff
BODIES = {1: 0x3c8218, 2: 0x3c8220, 5: 0x3c82a8, 8: 0x3c82e4,
          9: 0x3c85b0, 10: 0x3c85fc, 11: 0x3c8690, 12: 0x3c87f0,
          13: 0x3c8864, 14: 0x3c88dc, 15: 0x3c88e4}


def nums(values):
    return ','.join(f'{v & MASK:08x}' for v in values) or '-'


def tuples(values):
    return ';'.join(nums(v) for v in values) or '-'


def snapshot(model):
    return '|'.join([tuples(sorted(model.mem.bytes.items())), tuples(model.writes),
                     tuples(model.mem.log), ';'.join(str(int(name[7:])) + ':' + nums(args)
                                                    for name, args in model.calls) or '-',
                     nums([model.draws, model.hidden_draws])])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('native/tests/vectors/skills_a.tsv.gz'))
    a = parser.parse_args()
    for path, expected in PINS.items():
        data = (a.research_dir / path).read_bytes()
        committed = subprocess.check_output(['git', '-C', str(a.research_dir), 'show', COMMIT + ':' + path])
        if hashlib.sha256(data).hexdigest() != expected or hashlib.sha256(committed).hexdigest() != expected:
            parser.error('research pin mismatch: ' + path)
    sys.dont_write_bytecode = True
    sys.path.insert(0, str((a.research_dir / 'tools').resolve()))
    spec = importlib.util.spec_from_file_location('skills_reference', a.research_dir / 'tools/ref_skills_a.py')
    R = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(R)
    for module in (R.C, R.RC, R.AM, R.RI):
        if Path(module.__file__).resolve().parent != (a.research_dir / 'tools').resolve():
            parser.error('reference import came from another checkout')
    rng = random.Random(0x534b494c)
    rows, counts = [], {}

    def put(op, args, patches, config=0, scripts=(), external=()):
        R.C.set_fpu(('ps2', 'ps2g1', 'chop', 'nearest')[config // 4], bool(config & 2), bool(config & 1))
        mem = R.RC.Memory()
        for addr, size, value in patches:
            mem.poke(addr, size, value)
        model = R.Model(mem, external=['Skill.v%d' % slot for slot in external])
        for slot, value, mode, addr, size, mutation in scripts:
            def cb(*params, value=value, mode=mode, addr=addr, size=size, mutation=mutation):
                if mode:
                    mem.poke(addr, size, mutation)
                if mode == 2:
                    raise RuntimeError('authored callback failure')
                return R.s32(value)
            model.cb['Skill.v%d' % slot] = cb
        try:
            if op == 'call':
                result = model.skill_call(args[0], args[1], *args[2:])
            else:
                result = getattr(model, op)(*args)
            result = [] if result is None else list(result) if isinstance(result, tuple) else [result]
            encoded = nums(result)
        except (NotImplementedError, KeyError):
            encoded = 'unsupported'
        except RuntimeError:
            encoded = 'callback-error'
        rows.append('\t'.join([op, str(config), nums(args), tuples(patches), tuples(scripts), nums(external),
                               encoded, snapshot(model)]))
        counts[op] = counts.get(op, 0) + 1

    def base(seed=0):
        p = [(0x1060, 4, 0x2000), (R.G_RAND_SEED, 4, seed), (0x300c, 4, 0x40),
             (0x4000, 4, 0x5000), (0x500c, 4, 0x3000), (R.G_SKILL_TABLE + 4*17, 4, 0x1000)]
        for slot, body in BODIES.items():
            p += [(0x2000 + 8*slot, 2, 0), (0x2004 + 8*slot, 4, body)]
        return p

    firsts = [-32768, -8, -2, -1, 0, 1, 2, 20, 32767]
    levels = [0, 1, 2, 19, 20, 40, -1, -32768, 2147483647, -2147483648]
    steps = [0, 0x80000000, 0x3f000000, 0xbf000000, 0x40000000, 0xc0000000,
             0x00800000, 0x007fffff, 0x7fffffff, 0xffffffff, 0x3f800001, 0x4b800001]
    for config in range(16):
        for i in range(96):
            p = base(rng.getrandbits(32))
            for stat in range(5):
                for off, width, value in [(0xe, 2, rng.choice(firsts)), (0x18, 2, rng.randrange(-32768, 32768)),
                                          (0x24, 4, rng.choice(steps)), (0x38, 2, rng.choice(firsts)),
                                          (0x42, 2, rng.randrange(-32768, 32768)), (0x4c, 4, rng.choice(steps))]:
                    p.append((0x1000 + off + width*stat, width, value))
            stat, level, cap = i % 5, rng.choice(levels), rng.choice([0, 1, 10, 20, -1, 2147483647])
            for op, tail in [('skill_stat', [stat, level]), ('skill_stat_tier2', [stat, level, cap]),
                             ('skill_stat_for', [stat, level, [0, 0x3000, 0x3100][i % 3], cap]),
                             ('skill_stat_for_handle', [stat, level, 0x4000, cap]),
                             ('skill_mana_from_stat4', [level, cap]),
                             ('dodge_fraction', [level]), ('regeneration_stat0', [level]),
                             ('skill_stat_range', [stat, level, i % 3, rng.choice(steps)])]:
                put(op, [0x1000] + tail, p, config)
            put('call', [0x1000, 10, stat, level], p, config)
        for level in levels:
            # Synthetic immediates: no instruction bytes from any game file.
            p = base() + [(0x2004 + 8*5, 4, 0x6000), (0x6000, 4, 0x24a5ffff),
                          (0x6004, 4, 0x3c014060), (0x6008, 4, 0x44810800),
                          (0x600c, 4, 0x3c01bf00), (0x6010, 4, 0x34210001), (0x6014, 4, 0x44811000)]
            put('call', [0x1000, 5, level], p, config)
            for slot in (14, 15):
                for instruction in (0x24060014, 0x3021, 0x3025, 0x302d):
                    p = base() + [(0x2004 + 8*5, 4, 0x6000), (0x600c, 4, instruction),
                                  (0x6014, 4, 0x24420000 | slot*8)]
                    put('call', [0x1000, 5, level], p, config)
    for op, args in [('skill_can_use_base', [0x1000]), ('skill_cast_base', [0x1000]),
                     ('skill_mana_base', [0x1000]), ('passive_cast', [0x1000]), ('skill_init_stats_base', [0x1000]),
                     ('skill_adjust_level', [0x1000, 0xffffffff, 77]), ('skill_by_id', [17])]:
        put(op, args, base())
    for slot, args in [(1, []), (2, []), (5, []), (9, []), (14, [0xffffffff])]:
        put('call', [0x1000, slot] + args, base())
    for body in (0x2ddb90, 0x2f1440):
        put('call', [0x1000, 2], base() + [(0x2004 + 8*2, 4, body)])
    # Initialized first tier, second tier and sentinel bytes remain observable.
    p = base() + [(0x1000+i, 1, (i*73+11) & 255) for i in range(0x68)]
    put('skill_init_stats_base', [0x1000], p)
    # Handle node present/object absent still reads low memory's flags.
    for node, obj, flag in [(0, 0, 0x40), (0x5000, 0, 0x40), (0x5000, 0, 0), (0x5000, 0x3000, 0)]:
        p = base() + [(0x4000, 4, node), (0x500c, 4, obj), (0xc, 4, flag)]
        put('skill_stat_for_handle', [0x1000, 0, 40, 0x4000, 1], p)
    # Negative stat indices read adjacent supplied memory rather than typed-array guesses.
    for stat in (-1, 5, 8):
        put('skill_stat', [0x1000, stat, 40], base())
        put('skill_stat_tier2', [0x1000, stat, 40, 0], base())
    for delta in (-16, 16):
        p = base() + [(0x2000 + 8*10, 2, delta), (0x1000+delta+0xe, 2, 0),
                      (0x1000+delta+0x18, 2, 17), (0x1000+delta+0x24, 4, 0x40000000)]
        put('call', [0x1000, 10, 0, 2], p)
    # External overrides include writes, errors and virtual this offsets.
    for mode in range(3):
        p = base(0x12345678) + [(0x2000 + 8*10, 2, -16)]
        scripts = [(10, 0xfffffff9, mode, 0x100e, 2, 0xffff)]
        for op, tail in [('skill_stat_range', [0, 20, 1, 0x40000000]),
                         ('skill_stat_for', [0, 20, 0x3000, 1]), ('dodge_fraction', [20]),
                         ('regeneration_stat0', [20]), ('skill_mana_from_stat4', [20, 0])]:
            put(op, [0x1000] + tail, p, scripts=scripts, external=[10])
    put('skill_mana_from_stat4', [0x1000, 20, 3], base(), scripts=[(14, 7, 1, 0x100e, 2, 9)], external=[14])
    # Unknown callbacks and mana shape refusals, preserving exact state prefixes.
    p = base() + [(0x2004 + 8*10, 4, 0xdeadbeef)]
    put('call', [0x1000, 10, 0, 20], p, scripts=[(10, 5, 0, 0, 1, 0)])
    for p in [base() + [(0x2004 + 8*5, 4, 0x6000)],
              base() + [(0x2004 + 8*5, 4, 0x6000), (0x6000, 4, 0x24a5ffff)]]:
        put('call', [0x1000, 5, 1], p)
    plain = ('# rta-skills-a-shared-v1\n' + '\n'.join(rows) + '\n').encode('ascii')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open('wb') as stream:
        with gzip.GzipFile(filename='', fileobj=stream, mode='wb', mtime=0) as zipped:
            zipped.write(plain)
    manifest = {'schema': 'rta-skills-a-shared-v1', 'reference_commit': COMMIT,
                'reference_sha256': PINS['tools/ref_skills_a.py'], 'evidence_sha256': PINS,
                'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'vector_sha256': hashlib.sha256(plain).hexdigest(), 'vector_count': len(rows), 'operations': counts}
    a.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'{len(rows)} shared Skill state vectors')


if __name__ == '__main__':
    main()
