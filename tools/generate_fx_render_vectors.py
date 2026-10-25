#!/usr/bin/env python3
"""Vectors for the native port of ref_fx_render (native/src/fx/render.cpp).

Runs the PINNED research reference tools/ref_fx_render.py (commit fx2_research.COMMIT,
extracted with SHA-256 checks; nothing is read from the research working tree) on seeded
random SYNTHETIC inputs and writes

    native/tests/vectors/fx_render.tsv    one case per row: property, EE FPU model, the
                                          inputs and the expected outputs as hex patterns
    native/tests/vectors/fx_render.json   fx2_research.manifest(...)

No game data is read or written: views, projection scales, group matrices, particle cells,
type records, trails, beams and glows are all made up here.

    python3 -I tools/generate_fx_render_vectors.py [--seed N] [--research PATH]

Row layouts (tab separated; lists comma separated; '%x' hex of the 32-bit pattern unless
noted) are documented next to each writer below and parsed by native/tests/fx_render_tests.cpp.
"""
import argparse
import json
import math
import os
import pathlib
import random
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import fx2_research                                         # noqa: E402

ROOT = HERE.parent
OUT_TSV = ROOT / 'native/tests/vectors/fx_render.tsv'
OUT_JSON = ROOT / 'native/tests/vectors/fx_render.json'
M32 = 0xFFFFFFFF
M64 = (1 << 64) - 1

C = R = None                    # ref_core, ref_fx_render (imported from the pinned files)


def f2r(x):
    try:
        return struct.unpack('<I', struct.pack('<f', x))[0]
    except OverflowError:
        return 0x7F7FFFFF if x > 0 else 0xFF7FFFFF


def hx(v):
    return '%x' % (v & M32)


def hl(vals):
    return ','.join(hx(v) for v in vals)


def h16(v):
    return '%x' % (v & 0xFFFF)


def vtx(v):
    x, y, z, f, adc, rgba, uv = v
    return '%x.%x.%x.%x.%x.%02x%02x%02x%02x.%s' % (
        x, y, z, f, adc, rgba[0], rgba[1], rgba[2], rgba[3], '-' if uv is None else '%x:%x' % uv)


def vtxs(vs):
    return ' '.join(vtx(v) for v in vs)


def pt(p):
    return '.'.join(h16(c) for c in p)


# ---------------------------------------------------------------------------------------
# synthetic inputs
# ---------------------------------------------------------------------------------------

def random_view(rnd, target):
    """A camera mat34 looking at target (rows: the world axes' images, then translation)."""
    yaw, pitch = rnd.uniform(0, 2 * math.pi), rnd.uniform(-1.2, 1.2)
    dist = rnd.choice((rnd.uniform(30, 200), rnd.uniform(200, 1500)))
    fwd = (math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch))
    cam = [target[k] - fwd[k] * dist for k in range(3)]
    right = (fwd[1], -fwd[0], 0.0)
    n = math.sqrt(sum(x * x for x in right)) or 1.0
    right = [x / n for x in right]
    down = [right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2],
            right[0] * fwd[1] - right[1] * fwd[0]]
    rot = [[right[0], down[0], fwd[0]], [right[1], down[1], fwd[1]], [right[2], down[2], fwd[2]]]
    trans = [-sum(cam[r] * rot[r][c] for r in range(3)) for c in range(3)]
    if rnd.random() < 0.1:
        rot = [[x + rnd.uniform(-0.05, 0.05) for x in row] for row in rot]
    return [f2r(x) for row in rot for x in row] + [f2r(x) for x in trans]


def random_mat34(rnd, scale=1.0):
    a, b = rnd.uniform(0, 2 * math.pi), rnd.uniform(-1, 1)
    ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
    rot = [[ca, sa, 0.0], [-sa * cb, ca * cb, sb], [sa * sb, -ca * sb, cb]]
    if rnd.random() < 0.3:
        rot = [[x * rnd.uniform(0.5, 2.0) for x in row] for row in rot]
    t = [rnd.uniform(-500, 500) * scale for _ in range(3)]
    return [f2r(x) for row in rot for x in row] + [f2r(x) for x in t]


def random_target(rnd):
    return [rnd.uniform(-3000, 3000), rnd.uniform(-3000, 3000), rnd.uniform(-200, 400)]


def random_proj(rnd):
    return f2r(rnd.choice((477.5, rnd.uniform(200, 1200))))


def special_pattern(rnd):
    return rnd.choice((0, 0x80000000, 0x7F800000, 0xFF800000, 0x7FFFFFFF, 0x00000001,
                       0x80400000, 0x3F800000, 0xBF800000, 0x4B000000, 0x4B0000FF,
                       rnd.getrandbits(32), rnd.getrandbits(32)))


def random_float_pattern(rnd):
    r = rnd.random()
    if r < 0.15:
        return special_pattern(rnd)
    if r < 0.3:
        return rnd.getrandbits(32)
    return f2r(rnd.choice((rnd.uniform(-1e4, 1e4), rnd.uniform(-2, 2), rnd.uniform(-1e-3, 1e-3),
                           float(rnd.randrange(-300, 300)))))


def random_typerec(rnd, kind):
    """16 words shaped like a resident type record (all values made up)."""
    def col0():
        return f2r(8388608.0 + rnd.choice((rnd.randrange(256), rnd.randrange(-100, 400), 0, 255, 128)))

    def crate():
        return f2r(rnd.choice((0.0, rnd.uniform(-8, 8), rnd.uniform(-0.5, 0.5))))
    rec = [rnd.getrandbits(32) if rnd.random() < 0.2 else rnd.randrange(0x4000),
           rnd.getrandbits(32) if rnd.random() < 0.2 else rnd.randrange(0x4000),
           f2r(rnd.choice((0.0, rnd.uniform(-0.2, 0.2), rnd.uniform(-2, 2)))),
           f2r(rnd.choice((0.0, rnd.uniform(-0.5, 0.5), rnd.uniform(-5, 5)))),
           rnd.randrange(0x4000), rnd.randrange(0x4000), rnd.getrandbits(32) if rnd.random() < 0.1 else 0,
           f2r(rnd.choice((0.0, rnd.uniform(0, 30), rnd.uniform(0, 300))))]
    rec += [col0() for _ in range(4)] + [crate() for _ in range(4)]
    if kind == 2:
        rec[0] = f2r(rnd.choice((0.0, rnd.uniform(-1, 1), rnd.uniform(-20, 20))))   # line gravity
        rec[3] = f2r(rnd.choice((rnd.uniform(0.0, 20.0), float(rnd.randrange(0, 12)))))  # lag
    return rec


def s16c(x):
    return max(-0x8000, min(0x7FFF, int(round(x))))


def random_cell(rnd, centre, spread, clock, slot):
    pos = [s16c(centre[k] + rnd.uniform(-spread, spread)) for k in range(3)]
    birth = C.s16(clock - rnd.choice((0, rnd.randrange(96), rnd.randrange(400))))
    vel = [s16c(rnd.choice((0.0, rnd.uniform(-60, 60), rnd.uniform(-400, 400))) * 4096 / 60) if rnd.random() < 0.7
           else rnd.randrange(-0x8000, 0x8000) for _ in range(3)]
    typ = slot * 4 + rnd.randrange(4) if rnd.random() < 0.2 else slot * 4
    return [pos[0], pos[1], pos[2], birth, vel[0], vel[1], vel[2], C.s16(typ)]


# ---------------------------------------------------------------------------------------
# properties
# ---------------------------------------------------------------------------------------

def prop_vu(rnd, rows, n):
    """vu  -  op  args  result"""
    vu = R.Vu()
    ops = ('add', 'sub', 'mul', 'madd', 'div', 'rsqrt', 'itof', 'ftoi', 'vmax', 'vmin', 'clip')
    for i in range(n):
        op = ops[i % len(ops)]
        a, b, c = (random_float_pattern(rnd) for _ in range(3))
        if op in ('add', 'sub', 'mul', 'div', 'rsqrt', 'vmax', 'vmin'):
            if op in ('div', 'rsqrt') and rnd.random() < 0.1:
                b = rnd.choice((0, 0x80000000, 0x00000005))
            args, res = [a, b], getattr(vu, op)(a, b)
        elif op == 'madd':
            args, res = [a, b, c], vu.madd(a, b, c)
        elif op == 'itof':
            word = rnd.choice((rnd.getrandbits(32), rnd.randrange(-0x8000, 0x8000) & M32))
            sh = rnd.choice((0, 4, 12, 15))
            args, res = [word, sh], vu.itof(word, sh)
        elif op == 'ftoi':
            sh = rnd.choice((0, 4, 12, 15))
            args, res = [a, sh], vu.ftoi(a, sh)
        else:
            w = random_float_pattern(rnd)
            args, res = [a, b, c, w], vu.clip([a, b, c], w)
        rows.append(['vu', '-', op, hl(args), hx(res)])
    return n


def prop_proj(rnd, rows, n, model):
    """proj  model  view12  proj  group12  clock  P16  M16  G16  vf5"""
    for _ in range(n):
        target = random_target(rnd)
        view = random_view(rnd, target)
        proj = random_proj(rnd) if rnd.random() < 0.9 else random_float_pattern(rnd)
        group = random_mat34(rnd, rnd.choice((0.2, 1.0)))
        clock = rnd.choice((0, rnd.randrange(1 << 16), 0x7FF0, 0x8000))
        hdr = R.particle_header(view, proj, clock)
        rows.append(['proj', model, hl(view), hx(proj), hl(group), h16(clock),
                     hl(R.particle_projection(proj)), hl(R.particle_matrix(view, proj)),
                     hl(R.group_matrix(group, view, proj)), hl(hdr['vf5'])])
    return n


def prop_blocks(rnd, rows, n):
    """blocks  -  pages  head  tail  out (page.block.k: k cells sent, the rest zero-filled)"""
    for _ in range(n):
        pages = [rnd.randrange(64) for _ in range(rnd.choice((1, 1, 2, 3)))]
        head = rnd.randrange(0x100)
        tail = rnd.randrange(0x101) if len(pages) > 1 else rnd.randrange(head, 0x101)
        if rnd.random() < 0.2:
            head, tail = rnd.choice(((0, 0x100), (0x1F, 0x20), (0x20, 0x21), (0, 1)))
        out = []
        for page, blk, cells in R.list_blocks(pages, head, tail):
            k = sum(1 for c in cells if c is not None)
            assert all(c is None for c in cells[k:])
            assert cells[:k] == [blk * 32 + c for c in range(1, k + 1)]
            out.append('%x.%x.%x' % (page, blk, k))
        rows.append(['blocks', '-', ','.join('%x' % p for p in pages), '%x' % head, '%x' % tail,
                     ','.join(out)])
    return n


def prop_particles(rnd, rows, n, model, waitq_first):
    """particles  model  waitq  view12  proj  clock  stale2  types  npages  cells  sets
                  alphas  vertices
    types: slot=w0,..,w15;...    cells: page/cell=8 u16 joined by '.';...
    sets:  matrix12/idx:p.p:head:tail+...;...   (the first set is the global one)"""
    cells_total = 0
    for _ in range(n):
        target = random_target(rnd)
        view = random_view(rnd, target)
        proj = random_proj(rnd)
        clock = C.s16(rnd.choice((rnd.randrange(1 << 16), 0x7FF0, 100)))
        stale = [rnd.getrandbits(32), rnd.getrandbits(32)] if rnd.random() < 0.5 else [0, 0]
        nslots = rnd.randrange(1, 6)
        slots = rnd.sample(range(1, 32), nslots)
        kinds_of = {s: rnd.randrange(3) for s in slots}
        types = {s: random_typerec(rnd, kinds_of[s]) for s in slots}
        if rnd.random() < 0.5:
            types[0] = [0] * 8 + [0x4B000000] * 4 + [0] * 4        # an invisible slot 0
        typerecs = [types.get(s, [0] * 16) for s in range(32)]
        npages = rnd.randrange(1, 8)
        page_cells = {}
        sets = []
        for sidx in range(rnd.choice((1, 1, 2, 3))):
            matrix = [0] * 12 if sidx == 0 else random_mat34(rnd, 0.2)
            lists = []
            for idx in rnd.sample(range(12), rnd.randrange(1, 3)):
                kind = idx // 4
                cand = [s for s in slots if kinds_of[s] == kind] or slots
                pages = [rnd.randrange(npages) for _ in range(rnd.choice((1, 1, 2)))]
                head = rnd.randrange(0x60) if rnd.random() < 0.7 else rnd.randrange(0x100)
                if len(pages) == 1:
                    tail = min(0x100, head + rnd.randrange(1, 50))
                else:
                    tail = rnd.randrange(1, 0x30)
                centre = target if sidx == 0 else [0, 0, 0]
                spread = rnd.choice((30.0, 300.0))
                for pn, page in enumerate(pages):
                    lo = (head & ~0x1F) if pn == 0 else 0
                    hi = tail if pn == len(pages) - 1 else 0x100
                    for c in range(lo, hi):
                        if c & 0x1F in (0, 31):
                            continue
                        retired = pn == 0 and c < head
                        slot = 0 if retired else rnd.choice(cand)
                        page_cells[(page, c)] = random_cell(rnd, centre, spread, clock, slot)
                lists.append((idx, pages, head, tail))
            sets.append((matrix, lists))
        # the reference: as difftest_fx_render.reference_particle_packet walks the lists
        vu = R.Vu('chop')
        header = R.particle_header(view, proj, clock)
        want, alphas = [], []
        for sidx, (matrix, lists) in enumerate(sets):
            hdr = header
            if sidx:
                hdr = dict(header)
                hdr['m'] = R.columns(R.group_matrix(matrix, view, proj))
            by_idx = {idx: (pages, head, tail) for idx, pages, head, tail in lists}
            for kind in range(3):
                if kind < 2:
                    alphas.append(0x44 if kind == 0 else 0x48)
                for life in range(4):
                    if kind * 4 + life not in by_idx:
                        continue
                    pages, head, tail = by_idx[kind * 4 + life]
                    for page, blk, cells in R.list_blocks(pages, head, tail):
                        for c in cells:
                            w = [0] * 8 if c is None else page_cells.get((page, c), [0] * 8)
                            cell = struct.pack('<8h', *w)
                            slot = struct.unpack_from('<H', cell, 14)[0] // 4
                            rec = typerecs[slot & 31]
                            if kind == 2:
                                want += R.line_vertices(cell, rec, hdr, vu, waitq_first)
                            else:
                                want += R.sprite_vertices(cell, rec, hdr, vu, tuple(stale))
                            cells_total += 1
        rows.append(['particles', model, 'w' if waitq_first else 'c', hl(view), hx(proj), h16(clock),
                     hl(stale),
                     ';'.join('%x=%s' % (s, hl(r)) for s, r in sorted(types.items())),
                     '%x' % npages,
                     ';'.join('%x/%x=%s' % (p, c, '.'.join(h16(x) for x in w))
                              for (p, c), w in sorted(page_cells.items())),
                     ';'.join('%s/%s' % (hl(m), '+'.join('%x:%s:%x:%x' % (i, '.'.join('%x' % p for p in pg), hd, tl)
                                                          for i, pg, hd, tl in ls))
                              for m, ls in sets),
                     hl(alphas), vtxs(want)])
    return cells_total


def random_trail(rnd, target):
    t = R.Trail()
    paired = rnd.random() < 0.35
    flags = rnd.choice((0, 4, 8, 0x10, 0x14, 1, 0xC)) | (2 if paired else 0)
    t.clock = C.s16(rnd.choice((rnd.randrange(0, 400), rnd.randrange(1 << 16), 0x7FFF)))
    count = rnd.choice((1, 2, rnd.randrange(1, 20), rnd.randrange(1, 40), rnd.randrange(30, 127)))
    t.oldest = rnd.randrange(128)
    if paired:
        t.oldest &= 0xFE
        count = max(2, count & ~1)
    t.newest = (t.oldest + count) & 0x7F
    t.row = rnd.randrange(9)
    t.flags = flags
    t.lifetime = C.s16(rnd.choice((12, 30, 60, rnd.randrange(1, 200), rnd.randrange(-5, 1))))
    t.rgba = tuple(rnd.randrange(256) for _ in range(4))
    t.width4 = rnd.randrange(256)
    t.handle = 0x101
    pts = [(0, 0, 0, 0)] * 128
    p = [target[k] + rnd.uniform(-100, 100) for k in range(3)]
    step = rnd.choice((5.0, 20.0, 60.0))
    start = t.oldest & 0xFE
    n = (t.newest - start) & 0x7F
    for k in range(n):
        p = [p[j] + rnd.uniform(-step, step) for j in range(3)]
        stamp = C.s16(t.clock - rnd.randrange(0, 80) if rnd.random() < 0.9 else rnd.randrange(-0x8000, 0x8000))
        pos = tuple(s16c(2 * x) for x in p)
        if paired and k % 2 == 1:
            stamp = rnd.choice((255, 0, 128, rnd.randrange(256), rnd.randrange(-0x8000, 0x8000)))
        pts[(start + k) & 0x7F] = pos + (C.s16(stamp),)
    t.points = pts
    return t


def prop_trails(rnd, rows, n, model, waitq_first):
    """trail  model  waitq  view12  proj  rows('d' or 10 patterns)
              clock,oldest,newest,row,flags,lifetime,rgba32,width4  start:point;point;...
              header(24)  chunks(start.count,...)  vertices"""
    total = 0
    for _ in range(n):
        target = random_target(rnd)
        view = random_view(rnd, target)
        proj = random_proj(rnd)
        t = random_trail(rnd, target)
        if rnd.random() < 0.8:
            row_v, rows_field = R.TRAIL_ROW_V, 'd'
        else:
            row_v = tuple(sorted(rnd.uniform(0, 500) for _ in range(10)))
            row_v = tuple(C.to_float(f2r(x)) for x in row_v)
            rows_field = hl(f2r(x) for x in row_v)
        hdr = R.particle_header(view, proj, 0)
        try:
            want = R.trail_vertices(t, hdr['m'], hdr['vf6'], hdr['vf7'], R.Vu(), waitq_first, row_v)
        except ValueError:
            continue
        header = R.trail_header(t, row_v)
        start = t.oldest & 0xFE
        cnt = (t.newest - start) & 0x7F
        ring = '%x:%s' % (start, ';'.join(pt(t.points[(start + k) & 0x7F]) for k in range(cnt)))
        rgba32 = t.rgba[0] | t.rgba[1] << 8 | t.rgba[2] << 16 | t.rgba[3] << 24
        fields = ','.join([h16(t.clock), '%x' % t.oldest, '%x' % t.newest, '%x' % t.row, '%x' % t.flags,
                           h16(t.lifetime), '%x' % rgba32, '%x' % t.width4])
        rows.append(['trail', model, 'w' if waitq_first else 'c', hl(view), hx(proj), rows_field, fields,
                     ring, hl(x for q in header for x in q),
                     ','.join('%x.%x' % c for c in R.trail_chunks(t)), vtxs(want)])
        total += len(want)
    return total


def prop_beams(rnd, rows, n, model):
    """beam  model  cos  libc(64)  beams  frames  after-create(cos,libc)  per-frame  nodes
    beams:  jitter,scale,depth,width,rgba32,insim,from3,to3;...   (all made at frame sim0)
    frames: sim,net,draw,frozen[/b:from3:to3:alpha:width+...]|...  (sim0 = first beam field 'S')
    per-frame: cos|b0|b1..;... with bN = '-' (not alive) or which(a/b):rgba32:width4:points
    nodes: per beam the 7 words of every node (after the last frame), ';' between beams"""
    frames_total = 0
    for _ in range(n):
        seed = rnd.getrandbits(32)
        lstate = rnd.choice((1, rnd.getrandbits(64)))
        cos, libc = C.Lcg(seed), C.LibcRand(lstate)
        target = random_target(rnd)
        sim0 = rnd.randrange(1000, 1 << 20)
        beams = []
        for idx in range(rnd.randrange(1, 5)):
            b = {'idx': idx}
            b['jitter'] = f2r(rnd.choice((10.0, rnd.uniform(0, 60))))
            b['scale'] = f2r(rnd.choice((1.0, rnd.uniform(0, 20))))
            b['depth_arg'] = rnd.choice((0, 1, 2, 3, 4, 5, 6, 9))
            b['depth'] = min(b['depth_arg'], 5)
            b['width'] = rnd.choice((1000, rnd.randrange(4, 1024)))
            b['rgba'] = [rnd.randrange(256) for _ in range(4)]
            b['insim'] = rnd.randrange(2)
            b['from'] = [f2r(target[k] + rnd.uniform(-150, 150)) for k in range(3)]
            b['to'] = [f2r(target[k] + rnd.uniform(-150, 150)) for k in range(3)]
            b['nodes'] = R.beam_build_nodes(b['jitter'], b['depth'], cos, libc)
            b['expiry'] = sim0 + 10
            b['alive'] = True
            b['width4'] = (b['width'] >> 2) & 0xFF
            b['rgba32_0'] = b['rgba'][0] | b['rgba'][1] << 8 | b['rgba'][2] << 16 | b['rgba'][3] << 24
            b['from0'] = list(b['from'])
            beams.append(b)
        after_create = '%x,%x' % (cos.state, libc.state)
        sim = sim0
        net = sim0 + rnd.randrange(-3, 3)
        draw = rnd.randrange(1 << 20)
        frame_specs, frame_out = [], []
        for _f in range(rnd.randrange(1, 8)):
            ups = []
            for b in beams:
                if b['alive'] and rnd.random() < 0.3:
                    b['from'] = [f2r(C.to_float(x) + rnd.uniform(-5, 5)) for x in b['from']]
                    alpha = rnd.choice((rnd.randrange(256), rnd.randrange(-300, 300)))
                    width = rnd.choice((-1, rnd.randrange(1024)))
                    ups.append('%x:%s:%s:%x:%x' % (b['idx'], hl(b['from']), hl(b['to']), alpha & M32, width & M32))
                    b['rgba'][3] = alpha & 0xFF
                    if width >= 0:
                        b['width4'] = (width >> 2) & 0xFF
                    b['expiry'] = sim + 2
            frozen = int(rnd.random() < 0.15)
            frame_specs.append('%x,%x,%x,%x%s' % (sim & M32, net & M32, draw, frozen,
                                                   ('/' + '+'.join(ups)) if ups else ''))
            per = []
            for b in beams:
                if not b['alive']:
                    per.append('-')
                    continue
                now = net if b['insim'] else sim
                if b['expiry'] < now:
                    b['alive'] = False
                    per.append('-')
                    continue
                if not frozen:
                    R.beam_drift_nodes(b['nodes'], b['jitter'], b['depth'], cos)
                pts = R.beam_points(b['nodes'], b['from'], b['to'], b['depth'], b['scale'],
                                    C.Lcg(R.beam_frame_seed(sim, b['idx'])))
                ring, clock = R.beam_trail_points(pts)
                assert clock == len(ring)
                rgba32 = b['rgba'][0] | b['rgba'][1] << 8 | b['rgba'][2] << 16 | b['rgba'][3] << 24
                per.append('%s:%x:%x:%s' % ('b' if draw & 1 == 0 else 'a', rgba32, b['width4'],
                                            ','.join(pt(p) for p in ring)))
                frames_total += 1
            frame_out.append('%x|%s' % (cos.state, '|'.join(per)))
            draw = (draw + 1) & M32
            if rnd.random() < 0.5:
                sim += rnd.randrange(1, 4)
                net += rnd.randrange(0, 4)
            elif rnd.random() < 0.3:
                sim += rnd.randrange(3, 12)
                net += rnd.randrange(3, 12)
        spec = ';'.join('%s,%s,%x,%x,%x,%x,%s,%s' % (
            hx(b['jitter']), hx(b['scale']), b['depth_arg'], b['width'],
            b['rgba32_0'], b['insim'], hl(b['from0']), hl(b['to']))
            for b in beams)
        rows.append(['beam', model, '%x' % seed, '%x' % lstate, '%x' % sim0, spec, '|'.join(frame_specs),
                     after_create, ';'.join(frame_out),
                     ';'.join(hl(w for nd in b['nodes'] for w in nd.words()) for b in beams)])
    return frames_total



def prop_beam_fns(rnd, rows, n, model):
    """beamfn  model  jitter  depth  cos  libc  drift-seed  drifts  a3  b3  scale  amp  pseed
               nodes0  nodes1  cos-after-drift  points  pseed-after
    nodes0: after beam_build_nodes (then cos, libc states); nodes1: after `drifts` drifts"""
    total = 0
    for _ in range(n):
        jitter = f2r(rnd.choice((10.0, 0.0, rnd.uniform(0, 80), rnd.uniform(-5, 5))))
        depth = rnd.choice((0, 1, 2, 3, 3, 4, 5))
        seed, lst = rnd.getrandbits(32), rnd.getrandbits(64)
        cos, libc = C.Lcg(seed), C.LibcRand(lst)
        nodes = R.beam_build_nodes(jitter, depth, cos, libc)
        n0 = hl(w for nd in nodes for w in nd.words()) + ';%x;%x' % (cos.state, libc.state)
        dseed = rnd.getrandbits(32)
        drifts = rnd.randrange(0, 12)
        dc = C.Lcg(dseed)
        for _d in range(drifts):
            R.beam_drift_nodes(nodes, jitter, depth, dc)
        a = [f2r(rnd.uniform(-500, 500)) for _ in range(3)]
        b = [f2r(rnd.uniform(-500, 500)) for _ in range(3)]
        scale = f2r(rnd.choice((1.0, 0.0, rnd.uniform(0, 20))))
        amp = rnd.choice((R.ONE, f2r(rnd.uniform(0, 3))))
        pseed = rnd.getrandbits(32)
        pc = C.Lcg(pseed)
        pts = R.beam_points(nodes, a, b, depth, scale, pc, amp)
        rows.append(['beamfn', model, hx(jitter), '%x' % depth, '%x' % seed, '%x' % lst, '%x' % dseed,
                     '%x' % drifts, hl(a), hl(b), hx(scale), hx(amp), '%x' % pseed, n0,
                     hl(w for nd in nodes for w in nd.words()), '%x' % dc.state,
                     hl(x for p in pts for x in p), '%x' % pc.state])
        total += len(nodes) + len(pts)
    return total


def glow_project_str(g, cam, vp):
    sp = R.glow_project(g, cam, vp)
    return '-' if sp is None else '%d.%d.%d.%d' % sp


def packets_str(pk):
    return ';'.join('%x.%x.%x:%s' % (k, t, prim, ','.join('%x=%x' % (r, v) for r, v in ev))
                    for k, t, prim, ev in pk)


def prop_glows(rnd, rows, n, model):
    """glows  model  cam12  vp16  dims(tex=w.h;...)  queue  projects  pass1  pass0
    queue: pos3,size,bias,rgba32,tex,angle,flags(u16);...
    packets: key.tex.prim:reg=value,...;..."""
    total = 0
    for _ in range(n):
        target = random_target(rnd)
        view = random_view(rnd, target)
        proj = C.mat4_set_projection(0x3F7FFFFF, 0x3EEEEEEE, f2r(rnd.uniform(300, 900)), R.ONE,
                                     0x457FFF00, 0x41900000, 0x44E10000)
        vp = C.mat4_mul_mat34(view, proj)
        texs = {}
        for _t in range(rnd.randrange(1, 4)):
            tex = rnd.randrange(1, 1 << 32)
            texs[tex] = rnd.choice(((32, 32), (64, 64), (rnd.randrange(0, 300), rnd.randrange(0, 300))))
        queue = []
        for _g in range(rnd.choice((rnd.randrange(1, 10), rnd.randrange(1, 30)))):
            pos = [f2r(target[k] + rnd.uniform(-400, 400)) for k in range(3)]
            bias = f2r(rnd.choice((0.0, 5.0, rnd.uniform(-50, 200))))
            size = f2r(rnd.choice((8000.0, rnd.uniform(0, 20000), rnd.uniform(0, 300), rnd.uniform(0, 2e5))))
            col = tuple(rnd.randrange(256) for _ in range(4))
            tex = rnd.choice(list(texs) + [0] * (1 if rnd.random() < 0.1 else 0))
            flags = rnd.choice((0, 1, 2, 3, 4, 5, 6, 7, rnd.randrange(1 << 16)))
            queue.append(R.Glow(pos, size, bias, col, tex, rnd.randrange(1 << 16), flags))
        qfield = ';'.join('%s,%x,%x,%x,%x,%x,%x' % (
            hl(g.pos), g.size, g.bias, g.rgba[0] | g.rgba[1] << 8 | g.rgba[2] << 16 | g.rgba[3] << 24,
            g.tex, g.angle, g.flags & 0xFFFF) for g in queue)
        projects = ';'.join(glow_project_str(g, view, vp) for g in queue)
        p1 = R.glow_packets(queue, 1, view, vp, texs)
        p0 = R.glow_packets(queue, 0, view, vp, texs)
        rows.append(['glows', model, hl(view), hl(vp),
                     ';'.join('%x=%x.%x' % (t, w, h) for t, (w, h) in sorted(texs.items())),
                     qfield, projects, packets_str(p1), packets_str(p0)])
        total += len(queue)
    return total


def rvec(rnd, scale=300.0):
    return [f2r(rnd.choice((rnd.uniform(-scale, scale), 0.0, float(rnd.randrange(-50, 50))))) for _ in range(3)]


def flat(vs):
    return hl(x for v in vs for x in v)


def prop_builders(rnd, rows, n, model):
    """builder  model  kind  ...  (see each kind)"""
    for case in range(n):
        kind = case % 5
        if kind == 0:       # spline  t count wp wt pts  |  pos tan
            count = rnd.randrange(2, 9)
            pts = [rvec(rnd) for _ in range(count + 1)]
            t = f2r(rnd.choice((rnd.uniform(-0.2, 1.2), 0.0, 1.0, rnd.random())))
            wp, wt = int(rnd.random() < 0.8), int(rnd.random() < 0.8)
            pos, tan = R.spline_eval(t, pts, count, wp, wt)
            rows.append(['builder', model, 'spline', hx(t), '%x' % count, '%x' % wp, '%x' % wt, flat(pts),
                         '-' if pos is None else hl(pos), '-' if tan is None else hl(tan)])
        elif kind == 1:     # setspline  start end count steps pts  |  lifetime points
            count = rnd.randrange(2, 7)
            pts = [rvec(rnd) for _ in range(count)]
            steps = rnd.choice((rnd.randrange(1, 30), rnd.randrange(1, 60), 40, rnd.randrange(60, 125)))
            st = f2r(rnd.choice((0.0, rnd.random(), -0.3)))
            en = f2r(rnd.choice((1.0, rnd.random(), 0.5, 1.4)))
            out, life = R.trail_set_spline(st, en, pts, count, steps)
            rows.append(['builder', model, 'setspline', hx(st), hx(en), '%x' % count, '%x' % steps, flat(pts),
                         h16(life), flat(out)])
        elif kind == 2:     # circle  radius centre normal steps  |  points
            nrm = rvec(rnd, 1.0) if rnd.random() < 0.7 else rnd.choice(([R.ONE, 0, 0], [R.ONE, f2r(0.01), 0]))
            if all(x & 0x7FFFFFFF == 0 for x in nrm):
                nrm = [R.ONE, f2r(0.01), 0]
            steps = rnd.choice((rnd.randrange(1, 20), rnd.randrange(1, 126), 32, 130, -3))
            radius = f2r(rnd.uniform(0, 200))
            centre = rvec(rnd)
            out = R.trail_set_circle(radius, centre, nrm, steps)
            rows.append(['builder', model, 'circle', hx(radius), hl(centre), hl(nrm), '%x' % (steps & M32),
                         flat(out)])
        elif kind == 3:     # sweep  centre axis a b steps  |  lifetime pairs
            steps = rnd.choice((rnd.randrange(3, 70), 1, 0, -4))
            centre, axis, pa, pb = rvec(rnd), rvec(rnd, 1.0), rvec(rnd, 50.0), rvec(rnd, 50.0)
            if all(x & 0x7FFFFFFF == 0 for x in axis):
                axis = [0, 0, R.ONE]
            pairs, life = R.trail_sweep_pairs(centre, axis, pa, pb, steps)
            rows.append(['builder', model, 'sweep', hl(centre), hl(axis), hl(pa), hl(pb), '%x' % (steps & M32),
                         '%x' % (life & M32), hl(x for p0, p1 in pairs for x in p0 + p1)])
        else:               # pair  alpha a b clock  |  two ring entries
            alpha = f2r(rnd.choice((1.0, rnd.random(), 0.0, 1.5, -0.5)))
            a, b = rvec(rnd, 3000.0), rvec(rnd, 3000.0)
            clock = C.s16(rnd.choice((rnd.randrange(400), rnd.randrange(1 << 16))))
            out = R.trail_pair(alpha, a, b, clock)
            rows.append(['builder', model, 'pair', hx(alpha), hl(a), hl(b), h16(clock),
                         ';'.join(pt(p) for p in out)])
    return n


def main(argv):
    global C, R
    ap = argparse.ArgumentParser()
    ap.add_argument('--seed', type=int, default=20973)
    ap.add_argument('--research', default=str(fx2_research.DEFAULT_RESEARCH))
    args = ap.parse_args(argv)
    counts = {}
    rows = []
    with fx2_research.references(pathlib.Path(args.research)):
        import ref_core
        import ref_fx_render
        C, R = ref_core, ref_fx_render

        def run(name, fn, *a):
            rnd = random.Random('%s/%d/%s' % (name, args.seed, '/'.join(map(str, a))))
            c = fn(rnd, rows, *a)
            counts[name] = counts.get(name, 0) + c

        run('vu', prop_vu, 5000)
        run('blocks', prop_blocks, 2000)
        for model, share in (('ps2', 1), ('chop', 3)):
            C.set_fpu(model)
            run('proj', prop_proj, 300 // share, model)
            run('particles', prop_particles, 21 // share, model, True)
            run('trail', prop_trails, 170 // share, model, True)
            run('beam', prop_beams, 45 // share, model)
            run('beamfn', prop_beam_fns, 100 // share, model)
            run('glows', prop_glows, 150 // share, model)
            run('builder', prop_builders, 600 // share, model)
        C.set_fpu('ps2')
        run('particles vu1cpp', prop_particles, 7, 'ps2', False)
        run('trail vu1cpp', prop_trails, 60, 'ps2', False)
        C.set_fpu('ps2')
    data = ('\n'.join('\t'.join(r) for r in rows) + '\n').encode()
    OUT_TSV.write_bytes(data)
    OUT_JSON.write_text(json.dumps(fx2_research.manifest(len(rows), data, __file__), indent=1,
                                   sort_keys=True) + '\n')
    per_prop = {}
    for r in rows:
        per_prop[r[0]] = per_prop.get(r[0], 0) + 1
    print('%d rows, %d bytes' % (len(rows), len(data)))
    for k in sorted(per_prop):
        print('  %-10s %6d rows' % (k, per_prop[k]))
    for k in sorted(counts):
        print('  %-18s %7d items' % (k, counts[k]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
