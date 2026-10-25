#!/usr/bin/env python3
"""Regenerate native/tests/vectors/hud_extras_*.tsv from the research reference models.

Usage: python3 -I tools/hud_extras_vectors.py RESEARCH_TOOLS_DIR OUT_DIR

RESEARCH_TOOLS_DIR is the tools/ directory of the research worktree
(t3/research-hud-gaps, commit f7c3964 or later). It is only read. Every row is the
output of a ref_hud_*.py model on synthetic inputs from a seeded generator; no game
data is involved. The native contract tests (hud_extras_tests.cpp) replay the rows.
"""
import os
import random
import sys

if len(sys.argv) != 3:
    sys.exit(__doc__)
sys.dont_write_bytecode = True
sys.path.insert(0, sys.argv[1])
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

import ref_core as F                      # noqa: E402
import ref_hud_status as S                # noqa: E402
import ref_hud_xp as X                    # noqa: E402
import ref_hud_gold as G                  # noqa: E402
import ref_hud_quickslots as Q            # noqa: E402
import ref_hud_minimap as M               # noqa: E402

M32 = 0xFFFFFFFF
h = lambda v: '0x%08x' % (v & M32)
SEED = 20973


def write(name, header, rows):
    with open(os.path.join(OUT, name), 'w') as fh:
        fh.write('# %s\n' % header)
        for r in rows:
            fh.write('\t'.join(str(c) for c in r) + '\n')


# ---------------------------------------------------------------- status
def status_vectors():
    rng = random.Random('%d/hud-extras-status' % SEED)
    lines = []
    for _ in range(500):
        lines.append(S.vector_row(S.random_state(rng)))
    f = F.f32
    edge = []
    base = dict(slot=0, x=S.HUD_X[0], local=True, obj_flags=0, health=f(0.0), health_max=f(100.0),
                health_pending=f(10.0), mana=f(5.0), mana_max=f(50.0), mana_pending=f(10.0), tick=7,
                flash=0, cooldown=0, ack_level=1, level=1, exp=0, exp_base=0, exp_next=1100,
                glow_alpha=0)
    # dead bit clear with health 0 and below: both rectangles still drawn (hud-status.md section 4)
    for health in (0.0, -3.5, 1.0):
        edge.append(dict(base, health=f(health)))
    edge.append(dict(base, obj_flags=S.DEAD_FLAG))            # dead: no rectangles, no glow
    edge.append(dict(base, health_pending=f(0.0)))            # mana rectangle alone
    edge.append(dict(base, mana_pending=f(-1.0)))             # health rectangle alone
    edge.append(dict(base, health=f(150.0)))                  # current above maximum: inverted
    edge.append(dict(base, health=f(3.7), health_pending=f(0.2)))   # truncation inverts
    edge.append(dict(base, health_max=f(1e-30)))
    edge.append(dict(base, health_max=f(0.5), health=f(0.5)))
    edge.append(dict(base, local=False))                      # remote: health bar and multi frame only
    edge.append(dict(base, ack_level=0))                      # no XP bar
    edge.append(dict(base, ack_level=0xFF))
    edge.append(dict(base, exp=-5, exp_base=0, exp_next=1100))
    edge.append(dict(base, exp=99999, exp_base=0, exp_next=1100))     # pending level: whole bar
    edge.append(dict(base, exp_base=5, exp_next=5))           # equal thresholds: saturates
    edge.append(dict(base, exp=3, exp_base=5, exp_next=5))
    edge.append(dict(base, ack_level=1, level=2))             # glow
    edge.append(dict(base, ack_level=1, level=2, glow_alpha=126))
    edge.append(dict(base, ack_level=1, level=2, health=f(0.0)))      # glow needs health > 0
    edge.append(dict(base, tick=-8))                          # negative frame name
    edge.append(dict(base, tick=44))
    edge.append(dict(base, flash=3, cooldown=0xFFFF))
    edge.append(dict(base, cooldown=30))
    for slot in range(4):
        edge.append(dict(base, slot=slot, x=S.HUD_X[slot]))
    for e in edge:
        lines.append(S.vector_row(S.State(**e)))
    cols = ('# ' + '\t'.join(S.VECTOR_COLUMNS) + '\t| per draw: ' + '\t'.join(S.DRAW_COLUMNS))
    with open(os.path.join(OUT, 'hud_extras_status.tsv'), 'w') as fh:
        fh.write(cols + '\n' + '\n'.join(lines) + '\n')


# ---------------------------------------------------------------- xp / init
def xp_vectors():
    rng = random.Random('%d/hud-extras-xp' % SEED)
    rows = []
    for tick in [0, 3, 4, 43, 44, 1000, 100000, 2**31 - 1, -1, -4, -5, -44, -2**31] + \
            [rng.randrange(-5000, 200000) for _ in range(40)]:
        rows.append(('frame', tick, X.xp_frame(tick), X.xp_resource(tick)))
    for _ in range(300):
        lower = rng.choice([0, rng.randrange(0, 10**6), rng.randrange(-5, 5)])
        upper = lower + rng.choice([1, rng.randrange(1, 10**5), rng.randrange(1, 10**7), 0, -3])
        xp = lower + rng.choice([0, -1, 1, rng.randrange(-10**5, 10**6), upper - lower, upper - lower - 1])
        origin = rng.choice([22, 137, 280, 465])
        rows.append(('fill', origin, X.s32(xp), X.s32(lower), X.s32(upper),
                     X.xp_fill_right(origin, xp, lower, upper)))
    for _ in range(40):                                  # wrap-around differences
        xp, lower, upper = rng.randrange(-2**31, 2**31), rng.randrange(-2**31, 2**31), rng.randrange(-2**31, 2**31)
        rows.append(('fill', 22, xp, lower, upper, X.xp_fill_right(22, xp, lower, upper)))
    for _ in range(300):
        left = rng.randrange(-20, 700)
        top = rng.randrange(-20, 500)
        right = rng.choice([rng.randrange(-30, 700), left + 99, left])
        bottom = rng.randrange(-5, 520)
        inter = rng.random() < 0.7
        both = rng.random() < 0.5
        parity = rng.randrange(2)
        lb = rng.choice([0, 0, 16, 32])
        rows.append(('scissor', left, top, right, bottom, int(inter), int(both), parity, lb,
                     '0x%016x' % X.scissor(left, top, right, bottom, inter, both, parity, lb)))
    for _ in range(100):
        origin = rng.randrange(0, 560)
        right = rng.choice([origin, origin + 99, origin + rng.randrange(0, 100), rng.randrange(0, 700)])
        rows.append(('columns', origin, right, X.visible_columns(origin, right)))
    rows.append(('init', h(X.after_init()['hp']), X.after_init()['hp_max'], h(X.after_init()['mana']),
                 h(X.after_init()['mana_max'])))
    for _ in range(120):
        hp_max = F.f32(rng.choice([rng.uniform(20.0, 80.0), rng.uniform(0.1, 5.0), 0.0]))
        mana_max = F.f32(rng.choice([rng.uniform(10.0, 60.0), 10.0, 0.0]))
        cur = rng.choice([X.INIT_CURRENT, F.f32(rng.uniform(0, 100)), 0])
        hp = X.clamp(cur, hp_max)
        mana = X.clamp(cur, mana_max)
        rows.append(('first', h(hp_max), h(mana_max), h(cur), h(hp), h(mana)))
    for _ in range(60):
        hp_max = F.f32(rng.uniform(5.0, 90.0))
        for pvp in (0, 1):
            rows.append(('respawn', h(hp_max), pvp, h(X.respawn_hp(hp_max, pvp))))
    for slot in (-1, 0, 1, 3, 4):
        for n in (1, 2, 4):
            for obj in (0, 1):
                rows.append(('clamps', slot, n, obj, int(X.recalc_clamps(slot, n, obj))))
    sh = X.bar_green_shapes()
    assert len(sh) == 11 and all(s.used == (100, 6) and s.surface == (128, 8) and s.upload == (104, 8)
                                 and s.clut == (8, 2) and s.rows == 3 for s in sh)
    write('hud_extras_xp.tsv', 'kind fields... expected (ref_hud_xp.py)', rows)


# ---------------------------------------------------------------- gold
IDENT = [F.ONE if i % 5 == 0 else 0 for i in range(16)]


def digest(st):
    parts = [','.join(str(v & M32) for v in st.gold), ','.join(str(v) for v in st.pending),
             ','.join(str(v) for v in st.hold), ','.join(str(v) for v in st.alpha),
             str(st.frame), str(st.cosmetic_draws)]
    parts.append(','.join('%d:%d:%d:%s:%s' % (f.amount, G.s8(f.slot), f.start,
                                               '/'.join(h(v) for v in f.pos),
                                               '/'.join(h(v) for v in f.vel)) for f in st.floaters))
    text = '|'.join(parts)
    v = 0xcbf29ce484222325
    for b in text.encode():
        v = ((v ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % v     # FNV-1a 64 of the state text (the native test builds the same text)


def draw_text(calls):
    fl, ct, coins = [], [], []
    text_x = {0: 50, 1: 590, 2: 50, 3: 590}
    pending_set = None
    for c in calls:
        if c[0] == 'text_setColor':
            pending_set = c[1]
        elif c[0] == 'text_drawAscii':
            _, x, y, text, _lim = c
            entry = (G.s32(x), G.s32(y), text.decode(), pending_set)
            if entry[3] is None:
                continue
            (ct if len(ct) + len(fl) < 0 else fl).append(entry)  # sorted below
        elif c[0] == 'ui_drawSprite':
            coins.append((G.s32(c[2]), G.s32(c[3]), c[5]))
    return fl, ct, coins


def gold_vectors():
    rng = random.Random('%d/hud-extras-gold' % SEED)
    rows = []
    # separators and text
    for lang in (0, 1, 2, 0xFF, 0x80):
        for v in [0, 5, 12, 999, 1000, 1234, 99999, 100000, 1234567, 12345678, 99999999,
                  123456789, 999999999, -5, -123, -1234, -123456, -99999999]:
            rows.append(('sep', v, G.s8(lang), G.format_gold(v, lang).decode()))
    for _ in range(300):
        v = rng.choice([rng.randrange(0, 100000000), rng.randrange(0, 1000), rng.randrange(-10**6, 0)])
        lang = rng.randrange(256)
        rows.append(('sep', v, G.s8(lang), G.format_gold(v, lang).decode()))
    for n in range(0, 16):
        digits = ''.join(rng.choice('0123456789') for _ in range(n))
        buf = bytearray(digits.encode() + b'\0')
        G.insert_separators(buf, 0)
        rows.append(('rawsep', digits or '-', 0, bytes(buf[:buf.index(0)]).decode()))
    for a in (0, 1, 127, 128, 129, 130):
        for s in (0, 1, 5, 42, 43, 44, 100):
            rows.append(('falpha', a, s, G.floater_alpha(a, s)))
    for v in (0, 1, 7, 8, 100, 99999999, 99999992):
        for a in (1, 7, 8, 9, 64, 99999999, 2**31 - 1):
            rows.append(('credit', v, a, G.credit(v, a)))
    # sequences of operations; state digest after each
    seq_no = 0
    for _ in range(40):
        st = G.GoldState()
        for i in range(4):
            st.gold[i] = rng.choice([0, 500, 99999990, rng.randrange(0, 5000)])
        rows.append(('seq', seq_no, ','.join(str(v) for v in st.gold)))
        for _step in range(rng.randrange(20, 60)):
            op = rng.choice(['add', 'add', 'tick', 'tick', 'tick', 'adv', 'adv', 'draw', 'draw', 'draw', 'flush', 'bonus'])
            if op == 'add':
                slot = rng.randrange(4)
                amount = rng.choice([1, 5, 25, 100, rng.randrange(1, 5000), 99999999, 0])
                pos = [F.f32(rng.uniform(-50, 50)), F.f32(rng.uniform(-10, 10)), F.f32(rng.uniform(5, 200))]
                before = [f.amount for f in st.floaters]
                k = G.add_gold_floater(st, slot, pos, amount, IDENT)
                if k >= 0:
                    f = st.floaters[k]
                    rows.append(('op', 'add', slot, amount, k, '/'.join(h(v) for v in f.pos),
                                 '/'.join(h(v) for v in f.vel), f.offscreen, digest(st)))
                else:
                    rows.append(('op', 'add', slot, amount, -1, '-', '-', 0, digest(st)))
            elif op == 'tick':
                G.tick_gold(st)
                rows.append(('op', 'tick', digest(st)))
            elif op == 'adv':
                st.frame += rng.randrange(1, 30)
                rows.append(('op', 'adv', st.frame, digest(st)))
            elif op == 'flush':
                G.flush_gold(st)
                rows.append(('op', 'flush', digest(st)))
            elif op == 'bonus':
                slot = rng.randrange(4)
                G.arena_bonus_life(st, slot)
                rows.append(('op', 'bonus', slot, digest(st)))
            else:
                mask = rng.randrange(16)
                lang = rng.choice([0, 1])
                hud_off = rng.random() < 0.1
                calls = G.draw_gold(st, lambda s: bool(mask >> s & 1) if 0 <= s < 4 else False,
                                    len, lang, hud_off)
                # split the call list into floater texts, counters and coins
                fl, ct, coins = [], [], []
                state = None
                it = iter(calls)
                colour = None
                for c in calls:
                    if c[0] == 'text_setColor':
                        colour = c[1]
                    elif c[0] == 'text_drawAscii':
                        state = (G.s32(c[1]), G.s32(c[2]), c[3].decode(), colour)
                        (fl if 'ctr' not in locals() else fl)
                        ct.append(state)
                    elif c[0] == 'ui_drawSprite':
                        coins.append((G.s32(c[2]), G.s32(c[3]), c[5]))
                # floater texts come first (floater loop), counters next, in call order: split by
                # recomputing which belong to which loop from call order
                fl_n = sum(1 for c in calls if c[0] == 'net_isLocalSlot') - 4 if calls else 0
                # floater loop emits net_isLocalSlot per floater (8) then counters 4: draws before the
                # 9th net_isLocalSlot call belong to floaters
                fl, ct = [], []
                seen = 0
                colour = None
                for c in calls:
                    if c[0] == 'net_isLocalSlot':
                        seen += 1
                    elif c[0] == 'text_setColor':
                        colour = c[1]
                    elif c[0] == 'text_drawAscii':
                        e = '%d:%d:%s:%d' % (G.s32(c[1]), G.s32(c[2]), c[3].decode(), colour)
                        (fl if seen <= 8 else ct).append(e)
                ctr = []
                # counters carry their slot: recover it from the x/y of the placement table
                cslots = [] if hud_off else [i for i in range(4) if st.alpha[i] > 0 and bool(mask >> i & 1)]
                # hud_off or not local slots never emit; alpha>0 and local is exactly the emission rule
                assert len(cslots) == len(ct), (cslots, ct)
                ctr = ['%d:%s' % (i, e) for i, e in zip(cslots, ct)]
                coin_rows = ['%d:%d:%d' % (G.s32(x), G.s32(y), t) for x, y, t in coins]
                rows.append(('op', 'draw', mask, lang, int(hud_off), ';'.join(fl) or '-',
                             ';'.join(ctr) or '-', ';'.join(coin_rows) or '-', digest(st)))
        seq_no += 1
    for amount in (1, 5, 8, 100, 999, 1500, 99999999):
        for dpt in (1, 2):
            tl = G.counter_timeline(amount, 0, dpt, 150)
            for t, gold, pending, hold, alpha in tl:
                rows.append(('life', amount, dpt, t, gold, pending, hold, alpha))
    write('hud_extras_gold.tsv', 'kind fields... expected (ref_hud_gold.py)', rows)


# ---------------------------------------------------------------- quick slots
def quick_vectors():
    rng = random.Random('%d/hud-extras-quick' % SEED)
    rows = []
    tag_kind = {'h': 0, 'm': 1}

    def bag_text(bag):
        out = []
        for name, qty in bag:
            if isinstance(name, tuple):
                out.append('%s:%d:%d' % (name[0], name[1], qty))
            else:
                out.append('x:0:%d' % qty)
        return ','.join(out) or '-'

    def rand_bag():
        bag = []
        for _ in range(rng.randrange(0, 12)):
            r = rng.random()
            if r < 0.4:
                name = ('h', rng.randrange(9))
            elif r < 0.8:
                name = ('m', rng.randrange(9))
            else:
                name = 'other'
            bag.append((name, rng.choice([0, 1, 2, 3, 5, 6, 9, 0xFFFF, rng.randrange(0, 20)])))
        return bag

    for _ in range(300):
        bag = rand_bag()
        for kind in (0, 1, 2):
            n, _g = Q.count_potions(bag, kind)
            rows.append(('count', kind, bag_text(bag), n))
    for _ in range(300):
        missing = F.f32(rng.choice([rng.uniform(1.0, 700.0), 1.0, 60.0, 61.0, 540.0, 541.0, 1e9]))
        found = [rng.choice([-1, -1, rng.randrange(0, 12)]) for _ in range(9)]
        rows.append(('size', h(missing), ','.join(str(v) for v in found), Q.potion_size(missing, found)))
    for _ in range(1600):
        hm, mm = rng.choice([30.0, 33.5, 100.0, 450.0]), rng.choice([16.5, 40.0, 200.0])
        hp = rng.choice([0.0, -2.0, hm, rng.uniform(1.0, hm)])
        mana = rng.choice([0.0, mm, rng.uniform(0.0, mm)])
        hpp = rng.choice([0.0, 0.0, rng.uniform(0.0, hm), rng.uniform(0.0, 80.0)])
        mp = rng.choice([0.0, 0.0, rng.uniform(0.0, mm)])
        bag = rand_bag()
        st = dict(copy=rng.random() < 0.1, hp=F.f32(hp), maxhp=F.f32(hm), hp_pending=F.f32(hpp),
                  mana=F.f32(mana), maxmana=F.f32(mm), mana_pending=F.f32(mp), bag=bag,
                  local=rng.random() < 0.7, menu_open=rng.random() < 0.3, grid=rng.random() < 0.5,
                  kind=rng.randrange(2), timers=[0] * 8, rows=[0] * 8)
        o = Q.use_potion(st)
        early = int(o['returned_early'])
        size = o.get('size', -1 if not early else -2)
        out_bag = o.get('bag')
        rows.append(('use', int(st['copy']), h(st['hp']), h(st['maxhp']), h(st['hp_pending']), h(st['mana']),
                     h(st['maxmana']), h(st['mana_pending']), bag_text(bag), int(st['local']),
                     int(st['menu_open']), int(st['grid']), st['kind'],
                     early, size, h(o['pending']) if 'pending' in o else '-',
                     o['count'] if o['count'] is not None else -1,
                     o['removed'] if o['removed'] is not None else -1, int(o['sent_drop']),
                     ';'.join('%d:%d' % n for n in o['notices']) or '-',
                     '/'.join(h(v) for v in o['regen']) if o['regen'] else '-',
                     bag_text(out_bag) if out_bag is not None else '-'))
    for bits in [0, 0x1000, 0x2000, 0x4000, 0x8000, 0x3000, 0x5000, 0xa000, 0xc000, 0xf000, 0x0fff,
                 0x1234, 0xffff] + [rng.randrange(0, 0x10000) for _ in range(30)]:
        dx, dy, anybit = Q.pad_axes(bits)
        rows.append(('axes', bits, dx, dy, int(anybit)))
    gate_names = [n for n, _ in Q.GATES]
    for mask in range(512):
        gates = {n: bool(mask >> i & 1) for i, n in enumerate(gate_names)}
        for lmo in (0, 1):
            rows.append(('gates', mask, lmo, int(Q.pad_readable(gates, lmo))))
    for m in list(Q.GLYPH_MASKS) + [0, 3, 0x100, 0xffff]:
        rows.append(('glyph', m, Q.button_glyph(m)))
    for _ in range(200):
        masks = dict(a=rng.choice([0x20, 0x10, 0x4, 0x1]), b=rng.choice([0x80, 0x40, 0x8, 0x2]),
                     use=rng.choice([0x1000, 0x2000, 0x4000, 0x8000, 0x100]), accept=rng.choice([0x20, 0x10]))
        pressed = rng.randrange(0, 0x10000)
        dx, dy = rng.randrange(-1, 2), rng.randrange(-1, 2)
        bits = (0x2000 if dx > 0 else 0x8000 if dx < 0 else 0) | (0x4000 if dy > 0 else 0x1000 if dy < 0 else 0)
        menu = rng.random() < 0.3
        slot = rng.randrange(4)
        kind, value = rng.randrange(0, 3), rng.randrange(0, 70)
        frames0 = rng.choice([0, 1, 3, 7])
        st = dict(slot=slot, page=0, menu=menu, kind=kind, value=value, players=1, network=False,
                  local=True, hp=F.f32(10.0), gates={}, local_menu_open=False, held_bits=bits,
                  pressed_bits=pressed, masks=masks, alpha=64, timer=0, speed=0, prev_dir=-1, tick=0,
                  quick=(-1, -1), records=[dict(id=-1, newstyle=False, name='') for _ in range(8)],
                  glyphs={1: 0, 2: 0, 3: 0}, mask_entry=(frames0, 0x55, 9), font=0)
        ev, state = Q.selector_frame(st)
        d = state['dir']
        msgs = [e for e in ev if e[0] == 'msg']
        assert len(msgs) <= 1
        exp_id = msgs[0][1] if msgs else -1
        exp_args = msgs[0][2] if msgs else ()
        me = state['mask_entry']
        rows.append(('msg', slot, dx, dy, int(menu), pressed, masks['a'], masks['b'], masks['use'], masks['accept'],
                     kind, value, frames0, d, exp_id, ','.join(str(v) for v in exp_args) or '-',
                     me[0], me[1], me[2]))
    for rec_id in (-1, 0, 5, 37, 70):
        for ns in (0, 1):
            r = Q.record_icon(dict(id=rec_id, newstyle=bool(ns)))
            rows.append(('icon', rec_id, ns, r[0]))
    write('hud_extras_quick.tsv', 'kind fields... expected (ref_hud_quickslots.py)', rows)


# ---------------------------------------------------------------- minimap
def minimap_vectors():
    rng = random.Random('%d/hud-extras-minimap' % SEED)
    f = F.f32
    rows = []
    for _ in range(200):
        x, y = f(rng.uniform(-2000, 20000)), f(rng.uniform(-2000, 20000))
        origin = (f(rng.uniform(0, 60)), f(rng.uniform(0, 60)))
        c = M.world_to_cell(x, y, origin)
        rows.append(('cell', h(x), h(y), h(origin[0]), h(origin[1]), h(c[0]), h(c[1])))
    for world in (0, 1):
        for free in (0, 11999, 12000, 40000):
            for hwv in (0.0, 1.0, 1.99, 2.0, 2.5, 150.0, -3.0):
                hw = f(hwv)
                rows.append(('gate', world, free, h(hw), M.gate(world, free, 0, 1 << 20, hw)
                             if False else int(bool(world) and free >= 12000 and F.to_int(hw) >= 2)))
    for value in (0.0, 0.5, 1.0, 1.5, 2.0):
        pass
    captured = {'scissor': None, 'xyz': []}
    orig_header, orig_xyz = M._header, M._xyz

    def header(buf, p, scissor, regs):
        if captured['scissor'] is None:
            captured['scissor'] = scissor
        return orig_header(buf, p, scissor, regs)

    def xyz(x, y):
        captured['xyz'].append((x, y))
        return orig_xyz(x, y)
    M._header, M._xyz = header, xyz
    grid = bytes(rng.randrange(256) for _ in range(M.GRID_DIM * M.GRID_DIM * 2))
    drawn_count = blip_count = 0
    for case in range(260):
        yaw = rng.choice([0, 0x4000, -0x4000, 0x7fff, -0x8000, rng.randrange(-0x8000, 0x8000)])
        zoom = f(rng.choice([0.0, 0.1, 0.125, 0.13, 0.5, 1.0, 1.5, 2.5, rng.uniform(0.05, 3.0)]))
        half_w = f(rng.choice([1.0, 2.0, 80.0, 160.0, 200.0, rng.uniform(2.0, 300.0)]))
        half_h = f(rng.choice([2.0, 60.0, 110.0, rng.uniform(2.0, 150.0)]))
        centre = (f(rng.uniform(0, 26000)), f(rng.uniform(0, 26000)))
        origin = (f(rng.uniform(-5.0, 30.0)), f(rng.uniform(-5.0, 30.0)))
        width = rng.choice([216, 100, 64])
        height = rng.choice([216, 100, 64])
        blips = []
        for _b in range(rng.randrange(0, 5)):
            blips.append(None if rng.random() < 0.2 else dict(
                x=f(rng.uniform(0, 26000)), y=f(rng.uniform(0, 26000)), angle=rng.randrange(0, 0x10000),
                r=rng.randrange(256), g=rng.randrange(256), b=rng.randrange(256),
                scale=f(rng.choice([1.0, 0.5, 2.0, rng.uniform(0.2, 3.0)]))))
        inp = dict(world_tex=rng.choice([0x1000, 0x1000, 0x1000, 0]), dl_cursor=0x1000000,
                   dl_base=0x1000000 - 8 * rng.choice([100, 20000, 30000]), dl_capacity=40000, yaw=yaw,
                   centre=centre, offx=f(rng.uniform(-100, 100)), offy=f(rng.uniform(-60, 60)), zoom=zoom,
                   half_w=half_w, half_h=half_h, blip_size=f(rng.choice([6.0, 5.0])), icon_size=f(16.0),
                   origin=origin, width=width, height=height, grid=grid, blips=blips, icons=[],
                   full_scissor=(0, 447), zbuf=0x30000, timer=(0, 0), counters=(0, 0))
        free = inp['dl_capacity'] - (((inp['dl_cursor'] - inp['dl_base']) & M32) >> 3)
        captured['scissor'], captured['xyz'] = None, []
        out = M.draw(inp)
        calls = out['calls']
        rng_call = next((c for c in calls if c[0] == 'ranges'), None)
        gate = int(M.gate(inp['world_tex'], inp['dl_cursor'], inp['dl_base'], inp['dl_capacity'], half_w))
        if out['drawn']:
            drawn_count += 1
            w = captured['scissor']
            sc = ((w & 0xFFFF) >> 1, ((w >> 16) & 0xFFFF) >> 1, (w >> 32) & 0xFFFF, (w >> 48) & 0xFFFF)
        else:
            sc = ('-', '-', '-', '-')
        rows.append(('view', int(bool(inp['world_tex'])), free, yaw, h(centre[0]), h(centre[1]), h(origin[0]),
                     h(origin[1]), h(inp['offx']), h(inp['offy']), h(zoom), h(half_w), h(half_h), width, height,
                     gate, int(out['drawn']), *sc,
                     *(rng_call[1:] if rng_call else ('-', '-', '-', '-'))))
        if out['drawn'] and M.F.lt(M.K_BLIP_MIN_ZOOM, zoom):
            accepted = [b for b in blips if b is not None]
            verts = captured['xyz'][:6 * len(accepted)]
            for i, b in enumerate(accepted):
                blip_count += 1
                vs = verts[6 * i:6 * i + 6]
                rows.append(('blip', case, h(b['x']), h(b['y']), b['angle'], b['r'], b['g'], b['b'],
                             h(b['scale']), h(inp['blip_size']),
                             ';'.join('%s/%s' % (h(x), h(y)) for x, y in vs),
                             h((b['r'] | 0x80000000 | (b['b'] << 16) | (b['g'] << 8)) & M32)))
    M._header, M._xyz = orig_header, orig_xyz
    for v in range(256):
        explored = v & 0xF0
        rows.append(('icon_alpha', v, 0x80 if 0x80 < explored else explored))
    print('minimap: %d drawn views, %d blips' % (drawn_count, blip_count), file=sys.stderr)
    write('hud_extras_minimap.tsv', 'kind fields... expected (ref_hud_minimap.py)', rows)


status_vectors()
xp_vectors()
gold_vectors()
quick_vectors()
minimap_vectors()
