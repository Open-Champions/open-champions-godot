#!/usr/bin/env python3
"""Write native/tests/vectors/hud_pending.tsv from pinned UI research.

The pending potion overlays of hud_drawPlayerStatus 0x1d9cd4 are documented in
research-ui 3e39392 docs/hud-layout.md ("Known partial contracts", O/high,
0x1d9fb8-0x1da1d8) but have no reference function. This generator applies the
addressed arithmetic of that section with the research's PS2 float model
(tools/ee_emu.py fp_div / fp_mul / fp_add / fp_to_int / fp_from_int). Both
files are taken with `git show 3e39392:` (never the working tree) and checked
against SHA-256 pins. No game data is read.

  python3 -I tools/generate_hud_pending_vectors.py [--research DIR]
"""
import hashlib
import pathlib
import subprocess
import sys
import tempfile

COMMIT = '3e3939244fdaf72bdc06e1587550c4cff00e46b8'
PINS = {
    'docs/hud-layout.md': '803cc2b6c0e17b06741af815623744cd4df1ad50c094b4090ef13c4a02660040',
    'tools/ee_emu.py': 'e41dde52442978a6bf24e4dcce5f2ace5cd62d861a54d4da5f4173102360ffdc',
}
DEFAULT_RESEARCH = pathlib.Path.home() / 'Projects/champions-re-wt/research-ui'
OUT = pathlib.Path(__file__).resolve().parent.parent / 'native/tests/vectors/hud_pending.tsv'
# Caps by original slot and the overlay rows/colours, docs/hud-layout.md.
CAPS = (180, 605, 333, 447)
HEALTH = (47, 51, 0x80000060)
MANA = (66, 70, 0x805a4632)


def pinned(research, path):
    data = subprocess.run(['git', '-C', str(research), 'show', COMMIT + ':' + path],
                          check=True, capture_output=True).stdout
    digest = hashlib.sha256(data).hexdigest()
    if digest != PINS[path]:
        raise SystemExit('%s at %s has sha256 %s, expected %s' % (path, COMMIT, digest, PINS[path]))
    return data


def main():
    research = DEFAULT_RESEARCH
    if len(sys.argv) == 3 and sys.argv[1] == '--research':
        research = pathlib.Path(sys.argv[2])
    pinned(research, 'docs/hud-layout.md')
    with tempfile.TemporaryDirectory() as tmp:
        (pathlib.Path(tmp) / 'ee_emu.py').write_bytes(pinned(research, 'tools/ee_emu.py'))
        sys.path.insert(0, tmp)
        from ee_emu import float_to_raw, raw_to_float, fp_add, fp_div, fp_mul, fp_to_int, fp_from_int
        sys.path.pop(0)

        def s32(v):
            v &= 0xffffffff
            return v - (1 << 32) if v & 0x80000000 else v

        def overlay(origin, slot, current, maximum, pending, rows):
            c, m = float_to_raw(current), float_to_raw(maximum)
            start = origin + 15
            a = s32(fp_to_int(fp_mul(fp_div(c, m), float_to_raw(99.0))))
            t = s32(fp_to_int(fp_add(c, float_to_raw(pending))))
            if raw_to_float(fp_from_int(t)) > raw_to_float(m):
                t = s32(fp_to_int(m))
            b = s32(fp_to_int(fp_mul(fp_div(fp_from_int(t), m), float_to_raw(99.0))))
            return (start + a, rows[0], min(start + b, CAPS[slot]), rows[1], rows[2])

        cases = [
            # origin, slot, health, max, pending health, mana, max, pending mana
            (12, 0, 50.0, 100.0, 20.0, 30.0, 60.0, 10.0),
            (12, 0, 1.0, 3.0, 1.5, 2.0, 7.0, 0.25),
            (12, 0, 90.0, 100.0, 40.0, 59.5, 60.0, 5.0),    # t capped at trunc(M)
            (12, 0, 33.3, 97.7, 64.4, 0.0, 41.9, 41.9),
            (478, 1, 10.0, 250.0, 0.6, 249.0, 250.0, 9.0),
            (167, 2, 0.0, 1.0, 1.0, 0.5, 1.0, 0.5),
            (323, 3, 17.0, 31.0, 13.0, 5.0, 9.0, 3.0),
            (12, 2, 99.0, 100.0, 1.0, 1.0, 100.0, 98.0),     # slot-2 cap with slot-0 origin
            (12, 0, 25.0, 100.0, 0.0, 25.0, 100.0, 0.0),     # nothing pending
        ]
        lines = ['# hud pending overlays: docs/hud-layout.md + tools/ee_emu.py at research-ui ' + COMMIT,
                 '# origin slot health health_max pending_health mana mana_max pending_mana'
                 ' | per rect: left top right bottom colour']
        for case in cases:
            origin, slot, h, hm, ph, mn, mm, pm = case
            rects = []
            if ph > 0:
                rects.append(overlay(origin, slot, h, hm, ph, HEALTH))
            if pm > 0:
                rects.append(overlay(origin, slot, mn, mm, pm, MANA))
            fields = [origin, slot] + ['0x%08x' % float_to_raw(v) for v in (h, hm, ph, mn, mm, pm)]
            fields.append(len(rects))
            for r in rects:
                fields += [r[0], r[1], r[2], r[3], '0x%08x' % r[4]]
            lines.append('\t'.join(str(f) for f in fields))
    OUT.write_text('\n'.join(lines) + '\n')
    print('wrote %d cases to %s' % (len(cases), OUT))


if __name__ == '__main__':
    main()
