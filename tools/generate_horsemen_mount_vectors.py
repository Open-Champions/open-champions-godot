#!/usr/bin/env python3
"""Synthetic reference cases for the Nightmare mount, from the pinned research model.

Replays the memory rig of research tools/test_ref_horsemen_mount.py (no game files; a blank
memory, two vtables, a class definition, four players, a node per object) through
tools/ref_horsemen_mount.py and records, per case, the initial bytes, the operation, the
scripted answers of the external services, and the model's ordered stores, call-outs, shared
draws and result.  The native test replays the same operations on the same bytes.

    python3 -I tools/generate_horsemen_mount_vectors.py RESEARCH_DIR OUT.tsv OUT.json
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

COMMIT = '397c35cd99ac3fd3660834f5e4ac37767bbfbc3a'
PINS = {
    'tools/ref_horsemen_mount.py': '09612c24210f0ee24b5be26e45881ef772c018163417a6bcc31e4a595873ac8f',
    'tools/ref_arena_monsters.py': '80dbcdcc890a12838989d9acae501062849eaa6ffc51c03cb6ddf36a3d9cb5b8',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/test_ref_horsemen_mount.py': 'c7c9b5da93fac895a26fe7050396e58bb644fcabcabcc7f1ab9c4d409b5d6af1',
    'docs/spec/horsemen-mount.md': '49132a2465d0464dd2754be58902deaff340171b05f66aa63e1e61b96186a8bf',
    'tools/difftest_horsemen_mount.py': '2b556e6f68fca6f164447bfb9ce712093cd6f6dcb91b4d24d0e8a53e908582c1',
}
RIDER, MOUNT, PLAYERS, DEF, NODES = 0x10000, 0x11000, 0x12000, 0x20000, 0x70000
NODE_RIDER, NODE_MOUNT = NODES, NODES + 0x20
MASK64 = (1 << 64) - 1


def f32(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def main():
    root, out_tsv, out_json = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    for rel, digest in PINS.items():
        got = hashlib.sha256((root / rel).read_bytes()).hexdigest()
        if got != digest:
            raise SystemExit(f'pin mismatch for {rel}: {got}')
    sys.path.append(str(root / 'tools'))  # appended: the research tools dir has a dis.py
    import test_ref_horsemen_mount as T
    import ref_horsemen_mount as HM
    import ref_creature as RC

    def val(v):
        if isinstance(v, (tuple, list)):
            return 'v' + ','.join(str(x & 0xFFFFFFFF) for x in v)
        if v is None:
            return 'n'
        return 'i' + str(v & MASK64)

    def build_answers(spec):
        """spec: name -> ('const', v) | ('seq', [v..]) | ('arg0', {a: v}, d) | ('link', {obj: node})."""
        answers, text = {}, []
        for name, s in spec.items():
            kind = s[0]
            if kind == 'const':
                answers[name] = (lambda v: (lambda *a: v))(s[1])
                text.append(f'{name}=const:{s[1]}')
            elif kind == 'seq':
                it = [list(s[1]), 0]

                def seq(*a, it=it):
                    v = it[0][min(it[1], len(it[0]) - 1)]
                    it[1] += 1
                    return v
                answers[name] = seq
                text.append(f'{name}=seq:' + ','.join(map(str, s[1])))
            elif kind == 'arg0':
                answers[name] = (lambda m, d: (lambda a, *r: m.get(a, d)))(s[1], s[2])
                text.append(f'{name}=arg0:{s[2]}:' + ','.join(f'{k}>{v}' for k, v in s[1].items()))
            elif kind == 'link':
                holder = {}

                def link(head, obj, _again, m=s[1], ref=holder):
                    ref['rig'].poke(head, 4, m.get(obj, 0))
                answers[name] = link
                holder['_fn'] = link
                text.append(f'{name}=link:' + ','.join(f'{k}>{v}' for k, v in s[1].items()))
                answers.setdefault('_links', []).append(holder)
        return answers, '|'.join(text) or '-'

    cases = []
    baselines = {True: dict(T.Rig(linked=True).mem.bytes), False: dict(T.Rig(linked=False).mem.bytes)}
    header = ['\t'.join([tag] + [','.join(f'{a}:{b}' for a, b in sorted(baselines[flag].items()))])
              for tag, flag in (('L', True), ('U', False))]

    def case(name, op, args=(), pokes=(), spec=None, linked=True):
        spec = spec or {}
        answers, answer_text = build_answers(spec)
        links = answers.pop('_links', [])
        rig = T.Rig(answers=answers, linked=linked)
        for h in links:
            h['rig'] = rig
        for addr, size, v in pokes:
            rig.poke(addr, size, v)
        model = rig.model
        base = dict(rig.mem.bytes)
        n_writes = len(model.writes) if hasattr(model, 'writes') else None
        try:
            if op == 'nm_set_state':
                result = model.nightmare_set_state(MOUNT, args[0])
            elif op == 'nm_run':
                result = model.nightmare_run(MOUNT)
            elif op == 'rider_run':
                result = model.horsemen_run(RIDER)
            elif op == 'rider_set_state':
                result = model.horsemen_set_state(RIDER, args[0])
            elif op == 'nm_ctor':
                result = model.nightmare_ctor(MOUNT, 0, (0, 0, 0), args[0], args[1])
            elif op == 'hm_ctor':
                result = model.horsemen_ctor(RIDER, 0, (0, 0, 0), 0, 0x9000, args[0], 1)
            elif op == 'pick':
                result = model.nightmare_pick_charge_target(MOUNT)
            elif op == 'touch':
                result = model.nightmare_on_touch_list(MOUNT, tuple(args))
            elif op == 'hit_terrain':
                result = model.nightmare_on_hit_terrain(MOUNT)
            elif op == 'draw':
                result = model.nightmare_draw(MOUNT)
            elif op == 'angle':
                result = model.angle_to_target(MOUNT)
            elif op == 'seat':
                result = model.nightmare_seat_transform(MOUNT, args[0])
            else:
                raise ValueError(op)
        except Exception as exc:  # a case the model itself refuses is not a vector
            print('skip', name, type(exc).__name__, exc, file=sys.stderr)
            return
        writes = model.writes if hasattr(model, 'writes') else rig.mem.log
        calls = ';'.join('c' + n + '(' + ';'.join(val(a) for a in a_) + ')' for n, a_ in model.calls)
        wtext = ';'.join(f'{a},{s},{v}' for a, s, v in writes)
        ref = baselines[linked]
        init = ','.join(f'{a}:{b}' for a, b in sorted(base.items()) if ref.get(a) != b) or '-'
        res = 'n' if result is None else str(result & 0xFFFFFFFF)
        cases.append('\t'.join([name, op, ','.join(str(a) for a in args) or '-', answer_text, 'L' if linked else 'U', init,
                                wtext or '-', calls or '-', str(model.draws), res]))

    # ---- set-state: every state, linked and unlinked --------------------------------
    for linked in (True, False):
        for s in list(range(0, 0x24)) + [0x40]:
            case(f'setstate-{"l" if linked else "u"}-{s:#x}', 'nm_set_state', (s,),
                 [(RIDER + 0x232, 2, 0x1B), (MOUNT + 0x232, 2, 5), (MOUNT + 0x44, 4, 12345 + s),
                  (MOUNT + 0x300, 4, 1000), (RIDER + 0x18, 4, f32(10.0))], linked=linked)
    for seed in (1, 2, 3, 12345, 0xFFFFFFFF):
        case(f'state8-seed-{seed}', 'nm_set_state', (8,), [(MOUNT + 0x44, 4, seed), (RIDER + 0x232, 2, 0x1B)])
    for off in (0x31C, 0x314, 0x318):
        case(f'stranded-{off:#x}', 'nm_set_state', (0x14,),
             [(RIDER + 0x232, 2, 0x1B), (RIDER + 0x234, 4, NODE_MOUNT), (RIDER + off, 2, 30)])
    case('hit-reaction-keeps-state', 'nm_set_state', (0x12,), [(MOUNT + 0x232, 2, 7), (RIDER + 0x232, 2, 0x1B)])
    for filt in ((0x31C, 30), (0x314, 30), (0x318, 30), (0x31A, 30)):
        for s in (0, 1, 3, 5, 7, 0x10, 0x13):
            case(f'filter-{filt[0]:#x}-{s:#x}', 'nm_set_state', (s,),
                 [(MOUNT + filt[0], 2, filt[1]), (MOUNT + 0x234, 4, NODE_RIDER), (RIDER + 0x232, 2, 0x1B)])
    # ---- construction ----------------------------------------------------------------
    for facing in (0, 0x1234, 0xFFFF8000, 0x7FFF):
        case(f'ctor-{facing:#x}', 'nm_ctor', (facing, 0x9000), linked=False)
        for variant in range(4):
            pass
    for variant in range(4):
        case(f'horsemen-ctor-v{variant}', 'hm_ctor', (variant,), linked=False,
             spec={'pool_new': ('const', MOUNT), 'handle_link': ('link', {RIDER: NODE_RIDER, MOUNT: NODE_MOUNT})},
             pokes=[(RIDER + 0x470, 1, variant)])
    case('horsemen-ctor-pool-fails', 'hm_ctor', (1,), linked=False, spec={'pool_new': ('const', 0)})
    # ---- picker ----------------------------------------------------------------------
    case('pick-first-blocked', 'pick', spec={'terrain_raycast': ('seq', [f32(0.5)] + [f32(-1.0)] * 8)})
    case('pick-one-is-a-miss', 'pick', spec={'terrain_raycast': ('const', f32(1.0))})
    case('pick-all-hit', 'pick', spec={'terrain_raycast': ('const', f32(0.999))})
    case('pick-dead-skipped', 'pick', spec={'terrain_raycast': ('const', f32(-1.0))},
         pokes=[(PLAYERS + 0x400 * i + 0xC, 4, 0x48) for i in range(3)])
    case('pick-zero-players', 'pick', pokes=[(RC.G_NUM_PLAYERS, 4, 0)])
    case('pick-negative-players', 'pick', pokes=[(RC.G_NUM_PLAYERS, 4, 0xFFFFFFFF)])
    case('pick-one-player', 'pick', spec={'terrain_raycast': ('const', f32(0.0))}, pokes=[(RC.G_NUM_PLAYERS, 4, 1)])
    # ---- contact slots ---------------------------------------------------------------
    touch_pokes = [(MOUNT + 0x232, 2, 7), (MOUNT + 0xD0, 2, 30), (MOUNT + 0xD2, 2, 0xFFFB),
                   (MOUNT + 0x300, 4, 1000), (0x13000 + 0xC, 4, 0x20)]
    players = [PLAYERS + 0x400 * i for i in range(4)]
    case('touch-charging', 'touch', (players[0], players[1], 0x13000), touch_pokes,
         spec={'creature_inKnockdownAnim': ('arg0', {players[1]: 1}, 0)})
    case('touch-not-charging', 'touch', (players[0],), [(MOUNT + 0x232, 2, 3)])
    case('touch-empty', 'touch', (), touch_pokes)
    case('hit-terrain-charging', 'hit_terrain', pokes=[(MOUNT + 0x232, 2, 7)])
    case('hit-terrain-other', 'hit_terrain', pokes=[(MOUNT + 0x232, 2, 3)])
    # ---- draw, seat, angle ----------------------------------------------------------
    for fade in (0, 1, 97, 98, 99, 120):
        case(f'draw-fade-{fade}', 'draw', pokes=[(MOUNT + 0x3CC, 2, fade)])
    case('seat', 'seat', (0x7F000,), [(MOUNT + 0x38, 2, 0x2000), (MOUNT + 0x10, 4, f32(100.0)),
                                      (MOUNT + 0x14, 4, f32(50.0)), (MOUNT + 0x18, 4, f32(3.0)),
                                      (MOUNT + 0xE0, 4, 0xC1A00000), (MOUNT + 0xDC, 4, 0x3F999999)])
    for k, (tx, ty, face) in enumerate(((10.0, 0.0, 0), (0.0, 10.0, 0), (-5.0, -5.0, 0x4000), (3.0, 4.0, 0xC000), (0.0, 0.0, 0x1234))):
        case(f'angle-{k}', 'angle', pokes=[(MOUNT + 0x234, 4, NODES + 0x40), (NODES + 0x40 + 0xC, 4, players[0]),
                                           (players[0] + 0x10, 4, f32(tx)), (players[0] + 0x14, 4, f32(ty)),
                                           (MOUNT + 0x38, 2, face)])
    case('angle-no-target', 'angle')
    # ---- run: every state against a matrix of answers --------------------------------
    run_base = [(MOUNT + 0x300, 4, 1000), (MOUNT + 0x44, 4, 777)]
    answer_sets = {
        'quiet': {},
        'see': {'creature_canSeeObj': ('const', 1)},
        'cone': {'creature_findEnemyInCone': ('const', 1)},
        'both': {'creature_canSeeObj': ('const', 1), 'creature_findEnemyInCone': ('const', 1)},
        'ray-hit': {'terrain_raycast': ('const', f32(0.5))},
        'ray-miss': {'terrain_raycast': ('const', f32(-1.0))},
        'events': {'anim_advance': ('seq', [1, 2, 4, 5, 0, 3])},
    }
    for state in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x19, 0x1A, 0x1B, 0x40):
        for aname, spec in answer_sets.items():
            for target in (False, True):
                for flags in (0, 0x8, 0x20, 0x28, 0x18):
                    if flags and aname not in ('quiet', 'ray-hit', 'events', 'both'):
                        continue
                    pokes = list(run_base) + [(MOUNT + 0x232, 2, state), (MOUNT + 0x11A, 2, flags),
                                              (MOUNT + 0x3CC, 2, 98 if state == 0x14 else 5)]
                    if target:
                        pokes += [(MOUNT + 0x234, 4, NODES + 0x40), (NODES + 0x40 + 0xC, 4, players[1]),
                                  (players[1] + 0x10, 4, f32(30.0)), (players[1] + 0x14, 4, f32(5.0))]
                    case(f'run-{state:#x}-{aname}-{"t" if target else "n"}-{flags:#x}', 'nm_run', (), pokes, spec)
    case('run-trample-timer', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 3), (MOUNT + 0xC, 4, 0x21),
                                                         (MOUNT + 0x3D0, 4, 500)])
    case('run-dead-linked', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 0x14), (MOUNT + 0x3CC, 2, 99),
                                                       (MOUNT + 0x38, 2, 0x2000)])
    case('run-reappear', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 0x1A), (MOUNT + 0x3CC, 2, 3)])
    case('run-reappear-end', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 0x1A), (MOUNT + 0x3CC, 2, 0)])
    case('run-state-8-rider-swinging', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 8), (RIDER + 0x11A, 2, 8)])
    case('run-state-8-rider-done', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 8)])
    case('run-state-8-no-rider', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 8)], linked=False)
    # kick angle bands: the target's bearing relative to the facing
    for k, (tx, ty) in enumerate(((10, 0), (0, 10), (0, -10), (-10, 0), (7, 7), (-7, 7), (7, -7), (-7, -7), (10, 1), (1, 10), (-1, -10))):
        case(f'run-5-angle-{k}', 'nm_run', (), run_base + [(MOUNT + 0x232, 2, 5), (MOUNT + 0x38, 2, 0),
             (MOUNT + 0x234, 4, NODES + 0x40), (NODES + 0x40 + 0xC, 4, players[2]),
             (players[2] + 0x10, 4, f32(tx)), (players[2] + 0x14, 4, f32(ty))],
             {'creature_canSeeObj': ('const', 1)})
    # ---- rider runs mounted --------------------------------------------------------
    case('rider-copies-mount', 'rider_run', (), [(MOUNT + 0x10, 4, f32(100.0)), (MOUNT + 0x14, 4, f32(200.0)),
         (MOUNT + 0x18, 4, f32(5.0)), (MOUNT + 0x38, 2, 0x1234), (NODES + 0x40 + 0xC, 4, players[2]),
         (MOUNT + 0x234, 4, NODES + 0x40), (RIDER + 0x232, 2, 0x1B)])
    for s in (0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23):
        case(f'rider-state-{s:#x}', 'rider_run', (), [(RIDER + 0x232, 2, s), (RIDER + 0x11A, 2, 8), (RIDER + 0x300, 4, 1000)])
    for s in (0, 1, 0x14, 0x19, 0x1A, 0x1B, 0x1C, 0x23):
        case(f'rider-setstate-{s:#x}', 'rider_set_state', (s,), [(RIDER + 0x232, 2, 0x1B), (RIDER + 0x18, 4, f32(10.0))])

    out_tsv.write_text('# horsemen-mount-v1\n' + '\n'.join(header + cases) + '\n')
    out_json.write_text(json.dumps({
        'schema': 'horsemen-mount-v1', 'research_commit': COMMIT, 'references': PINS, 'cases': len(cases),
        'rig': {'rider': RIDER, 'mount': MOUNT, 'players': PLAYERS, 'definition': DEF, 'nodes': NODES},
        'note': 'Cases replay the rig of tools/test_ref_horsemen_mount.py with scripted external answers; '
                'expected values are the research model output. No game data.'}, indent=2) + '\n')
    print(len(cases), 'cases')


main()
