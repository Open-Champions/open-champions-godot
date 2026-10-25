#!/usr/bin/env python3
"""Generate native/tests/vectors/fx_trig_cases.tsv (+ fx_trig_cases.json).

The two emitter cases that native/tests/native_fx_tests.cpp and
native/tests/fx_exact_tests.cpp check by value (audit TRIG-1). They used to be
typed into the tests. This runs the pinned research reference instead:
ref_particles.emitter_run and ref_core at the commit in fx2_research.py
(extracted with `git show` and SHA-256 checked), with ref_particles._cs replaced
by ref_core.cos16 / sin16 exactly as tools/generate_fx_exact_vectors.py does.
research-fx fe137cc changes only the ref_particles docstring against that commit.

    python3 -I tools/generate_fx_trig_cases.py [--research PATH] [--out DIR]

Row format (tab separated): case  field  value. Floats are the 16 hex digits of
the binary64 pattern; seeds and counts are decimal. Synthetic inputs only.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fx2_research  # noqa: E402
import generate_fx_exact_vectors as G  # noqa: E402

ROOT = os.path.dirname(HERE)


def cases(P):
    rows = []

    def emit(name, rec, pos, now, seed, yaw=0):
        rng = P.CosmeticRng(seed)
        got = P.emitter_run(rec, pos, now, rng, yaw=yaw)
        rows.append([name, 'count', str(len(got))])
        rows.append([name, 'seed', str(rng.seed)])
        rows.append([name, 'on', str(rec.on)])
        for k, (p, v) in enumerate(got):
            for axis, value in zip('xyz', p):
                rows.append([name, 'pos%d.%s' % (k, axis), G.dhex(value)])
            for axis, value in zip('xyz', v):
                rows.append([name, 'vel%d.%s' % (k, axis), G.dhex(value)])

    # native_fx_tests.cpp emitters_and_primitives: the "vector" emitter.
    emit('vector', P.EmitterRecord.new(rate=1, vel=(10, 20, 30), off_x=4, off_y=-6, off_z=3,
                                       yaw=0x2000, pitch=0x1000, box=(8, 10, 12), spread=6,
                                       cone=0x800),
         (100.0, 50.0, 25.0), 1, 7, yaw=0x1000)
    # fx_exact_tests.cpp: straight up from sin16(0).
    emit('straight_up', P.EmitterRecord.new(rate=1, vel=(0, 0, 32), off_z=5),
         (100.0, 50.0, 10.0), 1, 3)
    return rows


def generate(research):
    with fx2_research.references(research):
        import ref_core as C
        import ref_particles as P
        C.set_fpu()
        P._cs = lambda angle, core=C: G.poly_cos_sin(core, angle)
        return cases(P)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--research', default=str(fx2_research.DEFAULT_RESEARCH))
    ap.add_argument('--out', default=os.path.join(ROOT, 'native', 'tests', 'vectors'))
    a = ap.parse_args(argv)
    rows = generate(a.research)
    data = ''.join('\t'.join(r) + '\n' for r in rows).encode('ascii')
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, 'fx_trig_cases.tsv'), 'wb') as h:
        h.write(data)
    manifest = fx2_research.manifest(len(rows), data, os.path.abspath(__file__))
    manifest['trig'] = ('ref_particles._cs replaced by ref_core.cos16 / sin16 (math_cos16, '
                        'ps2 adder), each float32 result promoted to binary64')
    manifest['note'] = 'Synthetic inputs only (no game data). Reference-model exact, not original-exact.'
    with open(os.path.join(a.out, 'fx_trig_cases.json'), 'w') as h:
        json.dump(manifest, h, indent=2, sort_keys=True)
        h.write('\n')
    print('%d rows, %d bytes' % (len(rows), len(data)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
