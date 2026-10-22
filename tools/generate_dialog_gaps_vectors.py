#!/usr/bin/env python3
"""Contract vectors of the dialog gaps (docs/spec/dialog-gaps.md of research t3/research-dialog-gaps).

Drives tools/ref_dialog_gaps.py (never the game) over random and fixed cases with seeds
20973 and 7 and writes one tab-separated case per line:  suite <TAB> inputs... <TAB> => <TAB> expected.
Every number is decimal; a float is its raw 32-bit pattern; a missing value is "-" or "x".
native/tests/dialog_gaps_tests.cpp replays the file on the native port.

    python3 -I tools/generate_dialog_gaps_vectors.py --research-dir ../../champions-re-wt/research-dialog-gaps \
        --tsv native/tests/dialog_gaps_vectors.tsv --contract native/tests/dialog_gaps_contract.json
"""
import argparse
import hashlib
import json
import os
import random
import subprocess
import sys

SEEDS = (20973, 7)
CASES = 120
FILES = ('docs/spec/dialog-gaps.md', 'tools/ref_dialog_gaps.py', 'tools/ref_core.py',
         'tools/test_ref_dialog_gaps.py', 'tools/difftest_dialog_gaps.py', 'tools/dialog_gaps_census.py')


def load(research):
    sys.path.insert(0, os.path.join(research, 'tools'))
    import ref_core as C
    import ref_dialog_gaps as R
    C.set_fpu('ps2')
    return C, R


def n(v):
    return 'x' if v is None else str(int(v))


def row(*vals):
    return ' '.join(n(v) for v in vals)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--research-dir', required=True)
    ap.add_argument('--tsv', required=True)
    ap.add_argument('--contract', required=True)
    args = ap.parse_args()
    C, R = load(args.research_dir)
    lines = []

    def emit(suite, ins, out):
        lines.append('\t'.join([suite] + list(ins) + ['=>', out]))

    def rf(rng, lo, hi):
        return C.f32(rng.uniform(lo, hi))

    def view_row(v):
        return row(v.yaw, v.pitch, *v.start, *v.target, v.start_yaw, v.target_yaw, v.target_pitch,
                   v.start_pitch, v.progress, v.k)

    def random_view(rng):
        v = R.View(rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000))
        v.start = [rf(rng, -500, 500) for _ in range(3)]
        v.target = [rf(rng, -500, 500) for _ in range(3)]
        v.start_yaw, v.target_yaw = rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000)
        v.start_pitch, v.target_pitch = rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000)
        v.progress = rng.choice([C.ZERO, C.ONE, rf(rng, 0, 1), rf(rng, -0.2, 1.2), rf(rng, 0.9, 1.0)])
        v.k = rng.choice([R.FAR_K, rf(rng, 0, 1), C.ZERO, C.ONE])
        return v

    def offset_text(rng):
        k = rng.random()
        if k < 0.15:
            return None
        if k < 0.65:
            return '%d,%d,%d,%d' % tuple(rng.randint(-60, 120) for _ in range(4))
        return rng.choice(['%d,%d' % (rng.randint(-9, 99), rng.randint(-9, 99)), ' 7, -3,12,900',
                           '%d,%d,%d' % (rng.randint(0, 50), rng.randint(0, 50), rng.randint(-30, 30)),
                           '5 ,6,7,8', 'x', '', '+4,-5,+6,-7', '1,2,3,4,5', '12,34;56,78', '  9 ,  8,7,6', '9,8,7,6x'])

    def stale_of(rng):
        return tuple(rng.choice([None, rng.randint(-40, 40)]) for _ in range(4))

    def text_field(t):
        return '-' if t is None else 'T:' + t

    def view_case(rng, emit_suite=True, fixed=None):
        t = offset_text(rng)
        stale = stale_of(rng) if rng.random() < 0.5 else tuple(rng.randint(-40, 40) for _ in range(4))
        npos = (rf(rng, -500, 500), rf(rng, -500, 500), rf(rng, -50, 50))
        facing = rng.randrange(-0x8000, 0x8000)
        players = []
        for _ in range(rng.randrange(0, 5)):
            if rng.random() < 0.7:
                p = tuple(C.add(npos[i], rf(rng, -70, 70)) for i in range(2)) + (C.add(npos[2], rf(rng, -20, 20)),)
            else:
                p = tuple(C.add(npos[i], rf(rng, -300, 300)) for i in range(3))
            players.append(p)
        if rng.random() < 0.3 and players:
            players[0] = tuple(npos)           # a player standing on the speaker
        campos = tuple(C.add(npos[i], rf(rng, -400, 400)) for i in range(3))
        yaw, pitch = rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000)
        side = rng.choice([0, 1, 1, 2])
        return yaw, pitch, npos, facing, side, players, campos, t, stale

    def run_view(case):
        yaw, pitch, npos, facing, side, players, campos, t, stale = case
        v = R.View(yaw, pitch)
        ins = [row(yaw, pitch), row(*npos), row(facing, side), row(*[x for p in players for x in p]),
               row(*campos), text_field(t), row(*stale)]
        try:
            info = R.setup_dialog_view(v, npos, facing, side, players, campos, t, stale)
        except ValueError:
            return v, ins, 'refused'
        f = info['fields']
        out = row(*v.target, v.target_yaw, v.target_pitch, *v.start, v.start_yaw, v.start_pitch,
                  v.progress, v.k, *info['flags'], info['angle'], *f, info['converted'])
        return v, ins, out

    for seed in SEEDS:
        rng = random.Random(seed)
        tag = 's%d' % seed
        for _ in range(CASES):
            t = offset_text(rng)
            stale = stale_of(rng)
            try:
                f, got = R.parse_dialog_offset(t, stale)
                out = row(*f, got)
            except ValueError:
                out = 'refused'
            emit('offset', [tag, text_field(t), row(*stale)], out)
        for a in (0, 1):
            for b in (0, 1):
                for c in (0, 1):
                    for behind in (0, 1):
                        asked = []
                        idx = R.side_index(a, b, c, lambda: (asked.append(1), bool(behind))[1])
                        emit('side', [tag, row(a, b, c, behind)], row(idx, len(asked)))
        for _ in range(CASES):
            case = view_case(rng)
            v, ins, out = run_view(case)
            emit('view', [tag] + ins, out)
        for _ in range(CASES):
            v = random_view(rng)
            steps = rng.choice([45, -45, 45, -45, rng.randint(1, 90), -rng.randint(1, 90)])
            before = view_row(v)
            res, h, pos = R.look_at(v, steps)
            emit('lookat', [tag, before, str(steps)],
                 row(res, h if pos is not None else None, *(pos if pos is not None else (None,) * 3),
                     v.yaw, v.pitch, v.progress))
        for _ in range(40):
            v = random_view(rng)
            v.progress = C.ZERO
            before = view_row(v)
            up = 0
            while True:
                up += 1
                if R.look_at(v, 45)[0] == 1 or up > 400:
                    break
            down = 0
            while True:
                down += 1
                if R.look_at(v, -45)[0] == 1 or down > 400:
                    break
            emit('chain', [tag, before], row(up, down))
        for _ in range(CASES):
            v = random_view(rng)
            direction = rng.choice([1, -1])
            swoop_done = rng.choice([0, 0, 1])
            shop = rng.choice([0, 0, 0, 1])
            fz = rng.choice([rf(rng, -1, 0), C.f32(-0.5), C.f32(-0.6), C.f32(-0.7), rf(rng, -1, 1)])
            before = view_row(v)
            acts, h = R.run_task_camera(v, direction, swoop_done, shop, fz)
            toks = []
            for a in acts:
                if a[0] == 'far_clip':
                    toks.append('F%d' % a[1])
                elif a[0] == 'apply':
                    toks.append('A%d,%d,%d' % tuple(a[1]))
                elif a[0] == 'arrived':
                    toks.append('R')
                else:
                    toks.append('S%d' % a[1])
            emit('runcam', [tag, before, row(direction, swoop_done, shop, fz)],
                 row(h, v.yaw, v.pitch, v.progress) + ' |' + ' '.join(toks))
        for _ in range(CASES):
            pos = tuple(rf(rng, -1000, 1000) for _ in range(3))
            yaw, pitch = rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000)
            shake = rng.choice([C.ZERO, rf(rng, 0.01, 8), rf(rng, 0.01, 8), C.f32(-1.0), C.HALF])
            sd = rng.randrange(1 << 32)
            frozen, dialog = rng.choice([0, 0, 1]), rng.choice([0, 1])
            p, m, sh, sd2, draws = R.camera_apply(pos, yaw, pitch, shake, sd, frozen, dialog)
            emit('apply', [tag, row(*pos, yaw, pitch, shake, sd, frozen, dialog)], row(*p, *m, sh, sd2, draws))
        for bits in range(256):
            kw = dict(network=bits & 1, paused=(bits >> 1) & 1, cutscene=(bits >> 2) & 1,
                      warmup=((bits >> 3) & 1) * rng.choice([1, 7]), player_select=(bits >> 4) & 1,
                      sim_frame=rng.choice([0, 59, 60, 61, 1000]) if (bits >> 5) & 1 else 0,
                      controller_out=((bits >> 6) & 1) * rng.choice([1, 2]), dialog=(bits >> 7) & 1)
            log = []
            val = R.game_is_frozen(R.Frozen(**kw), log)
            emit('frozen', [tag, row(kw['network'], kw['paused'], kw['cutscene'], kw['warmup'],
                                     kw['player_select'], kw['sim_frame'], kw['controller_out'], kw['dialog'])],
                 row(val) + ' |' + ','.join(x[0] for x in log))
        for _ in range(CASES):
            players = []
            for slot in rng.sample(range(4), rng.randrange(0, 5)):
                p = {'slot': slot}
                for k in R.RECALL_CHECKS[:11]:
                    p[k] = rng.random() < 0.1
                p['dialog'] = rng.random() < 0.3
                p['use'] = rng.random() < 0.7
                p['b100'] = rng.random() < 0.7
                players.append(p)
            ins = ' ; '.join(row(p['slot'], *[int(p[k]) for k in R.RECALL_CHECKS]) for p in players)
            emit('recall', [tag, ins or '-'], row(*R.recall_poll(players)) if R.recall_poll(players) else '-')
        for bits in range(256):
            args8 = [(bits >> i) & 1 for i in range(8)]
            emit('camctl', [tag, row(*args8)], row(R.camera_update_control(*args8)))
        site_keys = ['dialog', 'pvp_b', 'active', 'hp_le0', 'network', 'jumpgate', 'cam_mode', 'no_save', 'paused',
                     'lockout', 'selector', 'flag400000', 'local', 'pause_open', 'frontend', 'compass_on',
                     'cutscene', 'flag10000000', 'model', 'pending_level', 'list_on', 'countdown', 'local_menu',
                     'players']
        for name in R.SITES:
            for _ in range(CASES // 2):
                c = {}
                for k in site_keys:
                    if k == 'active':
                        c[k] = rng.randrange(0, 5)
                    elif k == 'cam_mode':
                        c[k] = rng.choice([1, 1, 0, 2, 6])
                    elif k == 'countdown':
                        c[k] = rng.choice([0, -3, 1, 2, 2, 3, rng.randint(4, 900)])
                    elif k == 'players':
                        c[k] = rng.randrange(0, 5)
                    else:
                        c[k] = int(rng.random() < 0.3)
                try:
                    outcome = R.site_outcome(name, c)
                except KeyError:        # the four sites run as whole functions (frozen, recall, camctl, apply)
                    outcome = 'refused'
                emit('site', [tag, name, row(*[c[k] for k in site_keys])], outcome)
        for _ in range(CASES):
            cd = rng.choice([0, -3, 1, 2, 2, 3, rng.randint(4, 900)])
            fl = [int(rng.random() < 0.15) for _ in range(3)]
            dialog = rng.choice([0, 1])
            players = rng.randrange(0, 5)
            sd = rng.randrange(1 << 32)
            cd2, sd2, voice = R.idle_voice(cd, fl[0], fl[1], fl[2], dialog, players, sd)
            emit('idle', [tag, row(cd, *fl, dialog, players, sd)], row(cd2, sd2, voice))
        for level in ('intro', 'Intro', 'INTRO', 'level01', '', 'introx'):
            for pvp in (0, 1):
                for old in (0, 0x1234):
                    for found in (0, 0x5678):
                        emit('box', [tag, level or '-', row(pvp, old, found)], row(R.load_box(level, pvp, old, found)))
        emit('boxreads', [tag, '0'], 'refused')
        emit('boxreads', [tag, '4660'], row(*R.box_sprite_reads(0x1234)))
        for layout in (-1, 0, 1, 2, 7, 255):
            words = R.init_camera_buttons(layout)
            emit('buttons', [tag, row(layout)], ' '.join('%d:%d' % (k, words[k]) for k in sorted(words)))
        keys = ['CreatureContainer', 'AutoContainer', 'AutoChest', 'AutoProp', 'AutoEmitter', 'AutoDoor',
                'AutoSwitch', 'Critter', 'Generator', 'SmokeGen', 'ParticleEmitter', 'DynamicLight', 'Halo',
                'ParticleEmitter2', 'Halo3', 'dialogScript', 'showName', 'dialogOffset', 'key', 'Radius']
        for _ in range(CASES * 2):
            entries = []
            for _ in range(rng.randrange(0, 7)):
                k = rng.choice(keys)
                if rng.random() < 0.3:
                    k = k.lower()
                elif rng.random() < 0.2:
                    k = k.upper()
                v = rng.choice(['0', '1', '2', ' 3', '-1', '', 'x', '7abc', '+5'])
                entries.append(k + '=' + v)
            R_fb = R.factory_fallback(entries)
            emit('factory', [tag, '|'.join(entries) or '-'],
                 (R_fb or '-') + ' ' + n(R.has_any_light(entries)) + ' ' + n(R.props_get_int(entries, 'Autoprop')) +
                 ' ' + n(R.props_get_int(entries, 'Halo', 1)))
    # fixed cases: the straight view, the chain counts and the TestLocation-shaped records
    fixed = [(0, 100, (C.f32(10), C.f32(20), C.f32(5)), 0, 0, [], (C.f32(10), 0, 0), '0,0,0,0',
              (0, 0, 0, 0))]
    for case in fixed:
        v, ins, out = run_view(case)
        emit('view', ['fixed'] + ins, out)
    for records in (['showName=Foo', 'dialogScript=bar', 'dialogOffset=1,2,3,4', 'key=Hero'],
                    ['TestLocation=1', 'Foo=2'], ['AutoChest=1', 'Halo1=1'], ['Generator=', 'AutoProp=1']):
        fb = R.factory_fallback(records)
        emit('factory', ['fixed', '|'.join(records)],
             (fb or '-') + ' ' + n(R.has_any_light(records)) + ' ' + n(R.props_get_int(records, 'Autoprop')) +
             ' ' + n(R.props_get_int(records, 'Halo', 1)))
    text = '\n'.join(lines) + '\n'
    with open(args.tsv, 'w') as fh:
        fh.write('policy\trta-dialog-gaps-v1\t%s\n' % subprocess.check_output(
            ['git', '-C', args.research_dir, 'rev-parse', '--short', 'HEAD']).decode().strip())
        fh.write(text)
    with open(args.tsv, 'rb') as fh:
        vec = hashlib.sha256(fh.read()).hexdigest()
    sha = {}
    for f in FILES:
        with open(os.path.join(args.research_dir, f), 'rb') as fh:
            sha[f] = hashlib.sha256(fh.read()).hexdigest()
    head = subprocess.check_output(['git', '-C', args.research_dir, 'rev-parse', 'HEAD']).decode().strip()
    with open(args.contract, 'w') as fh:
        json.dump({'commit': head, 'policy': 'rta-dialog-gaps-v1', 'seeds': list(SEEDS), 'lines': len(lines),
                   'sha256': sha, 'vectors_sha256': vec}, fh, indent=2)
        fh.write('\n')
    print('wrote', len(lines), 'cases')


main()
