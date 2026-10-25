#!/usr/bin/env python3
"""Regenerate synthetic C++ expectations from the hash-pinned research contract.
No disc input; reference files are imported read-only. Generated output is checked in.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import sys

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--research', type=Path, required=True)
p.add_argument('--output', type=Path, default=root/'native/tests/level_flow_vectors.inc')
a = p.parse_args()
pin = json.loads((root/'native/tests/level_flow_contract.json').read_text())
for rel, expected in pin['sha256'].items():
    if hashlib.sha256((a.research/rel).read_bytes()).hexdigest() != expected:
        raise SystemExit('Contract hash mismatch: '+rel)
sys.dont_write_bytecode = True
sys.path.append(str(a.research/'tools'))
spec = importlib.util.spec_from_file_location('ref_levels', a.research/'tools/ref_levels.py')
L = importlib.util.module_from_spec(spec)
spec.loader.exec_module(L)
F = L.f32
out = ['// Authored synthetic vectors generated from the pinned reference. Do not edit.']
def u(v): return str(v)+'u'
def vec(v): return '{'+','.join(map(u,v))+'}'
def string(v): return json.dumps(v)
rng = random.Random(0x1e55c0)
# Exact raw32 distance/pivot results, including mirrored rotations and signed positions.
for k in range(192):
    values = [rng.randint(-4000,4000) for _ in range(3)] + [rng.randint(-32768,32767) for _ in range(2)]
    mirrored = bool(k%2)
    piece = L.Piece(*values, mirrored=mirrored)
    pivot = L.piece_pivot(piece)
    pos = tuple(F(L.to_float(x)+rng.choice([-80,-40,0,40,80])) for x in pivot)
    hit = L.find_moving_piece([piece],pos)
    out.append('{ Piece p{'+','.join(map(str,values))+','+str(mirrored).lower()+'}; CHECK(piece_pivot(p)==Vec3('+vec(pivot)+')); std::vector<Piece> ps{p}; CHECK(find_moving_piece(ps,'+vec(pos)+')=='+str(hit)+'); }')
# Full rank/property matrix, not just a successful single marker.
propsets = [[],['default=1'],['defualt=1'],['default= -2junk'],['from=old'],['from=gate'],['name=door'],['from=old','name=door'],['default=0','default=1']]
for rank in [-1,0,1,2,3]:
    for recall in [False,True]:
        for props in propsets:
            r=L.Record('Player1',0,0,0,0,props)
            take,newrank=L.marker_decision(rank,r,'old','door',recall)
            out.append('CHECK(marker_decision('+str(rank)+', record("Player1",{'+','.join(map(string,props))+'}),"old","door",'+str(recall).lower()+')==std::make_pair('+str(take).lower()+','+str(newrank)+'));')
# Exact complete spawn vectors from ordered marker passes and placement.
for k in range(24):
    rs=[L.Record('Player1',100,1,2,3,[]),L.Record('Player1',65530,4,5,6,['default=1']),L.Record('Portal',0,7,8,9,[]),L.Record('Player1',0,10,11,12,['name=door'])]
    spawn=L.SpawnState();calls=[];draws=iter(range(1000,1100))
    def spot(c,spread,radius,any_height):
        calls.append((c,spread,radius,any_height))
        return (c[0]+len(calls),c[1],c[2])
    n=k%5
    took=L.run_markers(spawn,rs,'old','door',bool(k%2),spot,lambda:next(draws),loaded_from_save=bool(k%3==0),gate_arrival=bool(k%4==0),num_players=n)
    built=L.place_players(spawn,n,lambda r,p:4.0,spot,alive=lambda s:s!=1,medal_slot=2 if k%3==1 else None)
    out.append('{ auto s=vector_spawn_case('+','.join([str(n),str(bool(k%2)).lower(),str(bool(k%3==0)).lower(),str(bool(k%4==0)).lower(),str(2 if k%3==1 else -1)])+');')
    for j in range(4):
        out.append('CHECK(s.spawn.pos['+str(j)+']==Vec3('+vec([F(v) for v in spawn.pos[j]])+')); CHECK(s.spawn.facing['+str(j)+']=='+str(spawn.facing[j])+');')
    out.append('CHECK(s.spawn.rank=='+str(spawn.rank)+'); CHECK(s.calls=='+str(len(calls))+'); CHECK(s.built==std::vector<unsigned>({'+','.join(map(str,built))+'})); }')
# Python-authored objects bytes: class/property offsets are exercised by the parser.
blob=L.build_objects([('Player1',100,1.5,-2.0,3.0,['from=leva','default=1']),('Trigger',0,0,0,0,['w=10','h=4','t=go'])])
out.append('{ const Blob b{'+','.join(map(str,blob))+'}; auto rs=parse_objects({b.data(),b.size()}); CHECK(rs.size()==2); CHECK(rs[0].cls=="Player1"); CHECK(rs[0].angle==100); CHECK(rs[0].position==Vec3({'+','.join(u(F(v)) for v in [1.5,-2,3])+'})); CHECK(rs[0].prop("from")==std::optional<std::string>("leva")); CHECK(!rs[0].prop("name")); }')
# ---- Round 2 (docs/level-flow.md section 16). Own RNG: round-1 lines above stay identical.
# Input distributions follow tools/difftest_levels.py; expected values come from ref_levels.
r2 = random.Random(20973)
def cstr(v): return json.dumps(v)
def names_vec(ns): return 'std::vector<std::string>{'+','.join(map(cstr,ns))+'}'
# 16.2 base light level and per-chunk constant.
flags = [0, L.LEVEL_FLAG_LIGHT_20, L.LEVEL_FLAG_LIGHT_40, L.LEVEL_FLAG_LIGHT_20|L.LEVEL_FLAG_LIGHT_40]
flags += [r2.getrandbits(32) for _ in range(200)]
for fl in flags:
    out.append('CHECK(base_light_level('+u(fl)+')=='+u(L.base_light_level(fl))+');')
cflags = []
for bits in range(8):
    fl = (0x100 if bits&1 else 0)|(0x200 if bits&2 else 0)|(0x2000 if bits&4 else 0)
    cflags += [fl, fl|0x10|0x400]
cflags += [r2.getrandbits(32) for _ in range(200)]
for fl in cflags:
    light = L.chunk_header_state(fl)[3]
    dyn = light == 'dynamic'
    out.append('{ auto c=chunk_light('+u(fl)+'); CHECK(c.dynamic=='+str(dyn).lower()+' && c.level=='+str(0 if dyn else light)+' && c.flags=='+u(fl & ~0x100 & 0xffffffff)+'); }')
# 16.1 WORLD entry: directory order, text after the last dot, exact strcmp.
PARTS = ('a','lev','x.y','world','WORLD','worl','worlds','ob','scr','tex','')
def random_names():
    ns = []
    for _ in range(r2.randint(0,9)):
        stem = r2.choice(('a','b','lev','x.y','z1','','world'))
        k = r2.random()
        if k < 0.35: ns.append(stem+'.world')
        elif k < 0.45: ns.append(stem+'.world.'+r2.choice(PARTS))
        elif k < 0.55: ns.append(stem)
        else: ns.append(stem+'.'+r2.choice(PARTS))
    r2.shuffle(ns)
    return ns
for _ in range(300):
    ns = random_names()
    for n in (0,1,2):
        w = L.first_entry_with_extension(ns,'world',n)
        out.append('CHECK(first_entry_with_extension('+names_vec(ns)+',"world",'+str(n)+')=='+('std::optional<std::string>('+cstr(w)+')' if w is not None else 'std::optional<std::string>()')+');')
base_disc = ['LEVA.GOB','LEVA.TEX','LOADMISC.LMP','FX.LMP','HUD.LMP','MAPICONS.LMP','ICONS.LMP','GOODDOG.LMP','SKELE_B.LMP']
for _ in range(100):
    ns = random_names()+['objects.ob']
    r2.shuffle(ns)
    try:
        m = L.level_manifest('leva', gob_lumps=['leva.lmp','waterenv.lmp'], level_entries=ns, disc_files=base_disc)
        want = cstr([x.name for x in m if x.step == '8'][0])
    except L.MissingResource:
        want = None
    out.append('CHECK(manifest_world('+names_vec(ns)+')=='+('std::optional<std::string>('+want+')' if want else 'std::optional<std::string>()')+');')
# 16.3 recall countdown: per start value, 70 calls with random flags (cutscene, alive).
EV = {None:0, L.RECALL_AURA:1, L.RECALL_FINISH:2, L.RECALL_DROPPED_CUTSCENE:3, L.RECALL_DROPPED_NOBODY:4}
starts = [0,1,2,59,60,61,0x7F,0x80,0xFF]+[r2.randrange(256) for _ in range(40)]
calls = 0
for st in starts:
    rc = L.RecallCountdown(st); bias = r2.random(); rows = []
    for _ in range(70):
        cut = 1 if r2.random() < bias else 0
        alive = r2.choice((0,1,1,2,4))
        e = rc.tick(cut, alive)
        rows.append('{'+','.join(map(str,(cut,alive,EV[e],rc.value)))+'}')
        calls += 1
    out.append('{ RecallCountdown rc{'+str(st)+'}; static const std::uint8_t t[][4]={'+','.join(rows)+'}; for (auto &r : t) { CHECK(int(rc.tick(r[0]!=0,r[1]))==r[2]); CHECK(rc.value==r[3]); } }')
for st in (0, 100, 0xfffffff0):
    for net in (True, False):
        out.append('CHECK(recall_request_tick('+str(st)+'u,'+str(net).lower()+')=='+str(L.recall_request_tick(st,net))+'u);')
# 16.4 world_movePiece.
def random_offset():
    o = []
    for _ in range(3):
        k = r2.random()
        if k < 0.3: x = 0.0
        elif k < 0.5: x = float(r2.randint(-300,300))
        elif k < 0.7: x = r2.randint(-4000,4000)*0.25
        else: x = r2.uniform(-1200.0,1200.0)
        o.append(F(x))
    if r2.random() < 0.1:
        o = [F(0.0),F(0.0),F(-256.0)]
    return o
def random_angle():
    if r2.random() < 0.3:
        return r2.choice((0,0x4000,-0x4000,-0x8000,1,-1,0x637C))
    return r2.randint(-0x8000,0x7FFF)
def random_rest():
    if r2.random() < 0.2:
        return r2.randint(-0x8000,0x7FFF), r2.randint(-0x8000,0x7FFF)
    if r2.random() < 0.3:
        return r2.choice(((32767,0),(0,32767),(-32767,0),(0,-32767)))
    import math
    t = r2.uniform(-3.14159,3.14159)
    return int(round(math.cos(t)*32767)), int(round(math.sin(t)*32767))
for _ in range(400):
    k = r2.choice((1,1,1,2,3)) if r2.random() > 0.05 else r2.choice((0,-1))
    mirrored = [r2.random() < 0.4 for _ in range(max(k,0))]
    tx,ty,tz = (r2.randint(-0x100000,0x100000) for _ in range(3))
    c,sn = random_rest()
    piece = L.Piece(tx,ty,tz,c,sn)
    off = random_offset(); ang = random_angle()
    res = L.move_piece(piece, off, ang, mirrored)
    out.append('CHECK(move_piece(Piece{'+','.join(map(str,(tx,ty,tz,c,sn)))+'},Vec3('+vec(off)+'),'+str(ang)+',std::vector<bool>{'+','.join(str(m).lower() for m in mirrored)+'})==std::vector<ChunkTransform>{'+','.join('{'+','.join(map(str,t))+'}' for t in res)+'});')
rest = L.move_piece(L.Piece(400,-800,40,32767,0),(F(0.0),F(0.0),F(-256.0)),0)[0]
assert rest == (399,-800,-984,32767,0)
out.append('CHECK(move_piece(Piece{400,-800,40,32767,0},Vec3('+vec((F(0.0),F(0.0),F(-256.0)))+'),0)==std::vector<ChunkTransform>{{'+','.join(map(str,rest))+'}});')
# ---- Round 3 (docs/level-flow.md section 17). Appended so round-1/2 lines stay identical.
for slot, count in ((0, 1), (1, 1), (2, 4), (2, 3)):
    out.append('CHECK(arena_ring_angle(%d,%d)==%d);' % (slot, count, L.arena_ring_angle(slot, count)))
entry = (F(10.0), F(-4.0), F(2.0))
plans = (
    dict(slot=0, count=1, mode_a=False, table=False, fail=False),
    dict(slot=2, count=4, mode_a=True, table=False, fail=False),
    dict(slot=1, count=4, mode_a=False, table=True, fail=False),
    dict(slot=1, count=2, mode_a=False, table=False, fail=True),
    dict(slot=3, count=3, mode_a=True, table=True, fail=True),
)
def words(values):
    return '{'+','.join(u(int(v) & 0xFFFFFFFF) for v in values)+'}'
for plan in plans:
    def search(centre, spread, radius, any_height, plan=plan):
        if radius != 24 or any_height:
            raise SystemExit('arena search arguments changed')
        if plan['fail'] and spread == L.ARENA_FIRST_SPREAD:
            return centre, 0
        return (centre[0], centre[1], F(6.0)), 1
    pos, facing, ok = L.arena_initial_place(
        entry, plan['slot'], plan['count'], plan['mode_a'], search,
        table_facing=plan['table'], table_angle=0x1234)
    out.append(
        '{ const Vec3 entry'+words(entry)+'; auto placed=arena_initial_place(entry,'
        +str(plan['slot'])+','+str(plan['count'])+','+str(plan['mode_a']).lower()+','
        '[](const Vec3 &centre, int spread, int radius, bool any){ CHECK(radius==24 && !any); '
        'const bool fail='+str(plan['fail']).lower()+' && spread==36; Vec3 pos=centre; '
        'if(!fail) pos[2]=0x40c00000u; return std::make_pair(pos, fail?0:1); },'
        +str(plan['table']).lower()+',0x1234); CHECK(placed.ok=='+str(ok)
        +' && placed.facing=='+str(facing & 0xFFFF)+' && placed.pos==Vec3'+words(pos)+'); }')
# ---- Round 4 (docs/level-flow.md section 18). Appended so earlier lines stay identical.
import ref_core as C
import ref_objects as Obj
import ref_world as W
def pat(v):
    return int(v) & 0xFFFFFFFF
def hit(pos, ok, snap):
    return 'Round4Hit{Vec3{%su,%su,%su},%d,%su}' % (
        pat(pos[0]), pat(pos[1]), pat(pos[2]), int(bool(ok)), pat(snap))
for index, saved in ((0, 4), (7, 4), (11, 9), (12, 4), (12, 0)):
    out.append('CHECK(arena_mode_a_next(%d,%d)==%d);' % (index, saved, L.arena_mode_a_next(index, saved)))
for saved in range(10):
    seen = [saved]
    index = saved
    for _ in range(13):
        index = L.arena_mode_a_next(index, saved)
        if index == saved:
            break
        seen.append(index)
    out.append('CHECK(arena_mode_a_cycle(%d)==std::vector<int>{%s});' % (
        saved, ','.join(str(v) for v in seen)))
for byte in (0x30, 0x31, 0x35, 0x33, 0x42, 0x08, 0x80):
    out.append('CHECK(arena_record_table_offset(%d)==%d);' % (byte, L.arena_record_table_offset(byte)))
ring_entry = (F(10.0), F(-4.0), F(2.0))
ring = L.arena_ring_centre(ring_entry, 0, 1, True)
out.append('CHECK(arena_ring_centre(Vec3%s,0,1,true)==Vec3%s);' % (words(ring_entry), words(ring)))
out.append('CHECK(arena_ring_centre(Vec3%s,0,2,true)==Vec3%s);' % (words(ring_entry), words(L.arena_ring_centre(ring_entry, 0, 2, True))))
out.append('CHECK(arena_ring_centre(Vec3%s,0,4,true)==Vec3%s);' % (words(ring_entry), words(L.arena_ring_centre(ring_entry, 0, 4, True))))
ox = W.fmul(C.cos16(0), L.ARENA_RING_MODE_A)
oy = W.fmul(C.sin16(0), L.ARENA_RING_MODE_A)
out.append('CHECK(mul(cos16(0),ARENA_RING_MODE_A)==%su);' % pat(ox))
out.append('CHECK(mul(sin16(0),ARENA_RING_MODE_A)==%su);' % pat(oy))

def flat_level(x0, y0, w, h, height, element_z=0, element_xy=(0, 0)):
    samples = [height * 16] * (w * h)
    return W.Level(W.build_world_file((-2, -2), (8, 8), [(x0, y0, w, h, samples)],
                                      [(0, 0, element_xy[0], element_xy[1], element_z, 32767, 0)]))

def search_snap(level, centre, seed, with_object):
    world = Obj.World()
    world.shared = C.SharedRandom(seed)
    if with_object:
        world.construct(0x1000, centre)
    pos, ok = world.find_free_spot(
        centre, L.ARENA_FIRST_SPREAD, L.ARENA_SEARCH_RADIUS, False,
        lambda r, p: W.terrain_test_flag1(level, r, p),
        lambda r, p: W.terrain_height(level, r, p))
    snap = L.player_ctor_z(tuple(pos), L.PLACEMENT_RADIUS,
                           lambda r, p: W.terrain_height(level, r, p))
    return pos, ok, snap

solid = flat_level(-40, -40, 80, 80, 0)
hole = flat_level(0, 0, 1, 1, 0)
origin = (F(0.0), F(0.0), F(0.0))
solid_fail = 0
for seed in list(range(16)) + [20973]:
    for with_object in (False, True):
        pos, ok, snap = search_snap(solid, origin, seed, with_object)
        if not ok:
            solid_fail += 1
        if snap == W.NO_FLOOR:
            raise SystemExit('solid disc lost the floor')
        out.append('CHECK(round4_solid(%d,%s)==%s);' % (seed, str(with_object).lower(), hit(pos, ok, snap)))
out.append('CHECK(round4_solid_failures()==%d);' % solid_fail)
kept = lost = 0
lost_ok = None
for seed in range(32):
    pos, ok, snap = search_snap(hole, origin, seed, False)
    floor = snap != W.NO_FLOOR
    if floor:
        kept += 1
    else:
        lost += 1
        lost_ok = int(bool(ok))
    out.append('CHECK(round4_hole(%d)==%s);' % (seed, hit(pos, ok, snap)))
if kept <= 0 or lost <= 0 or lost_ok != 1:
    raise SystemExit('hole disc did not match the reference split')
out.append('CHECK(round4_hole_kept()==%d);' % kept)
out.append('CHECK(round4_hole_lost()==%d);' % lost)
misses, tested = 0, 0
limit = 37
for ix in range(-limit, limit + 1):
    for iy in range(-limit, limit + 1):
        if ix * ix + iy * iy > limit * limit:
            continue
        tested += 1
        pos = (F(float(ix)), F(float(iy)), origin[2])
        if W.terrain_height(solid, F(11.25), pos) == W.NO_FLOOR:
            misses += 1
out.append('CHECK(round4_lattice()==std::make_pair(%d,%d));' % (misses, tested))
# Mode-A arena start on a synthetic field at height 3 (radius 12 at the origin is
# the sentinel; floor under the slot-0 centre). Composed only from the pinned
# reference: arena_register_spawn (18.2) -> arena_ring_centre / arena_initial_place
# (17.3) with ref_objects.World.find_free_spot -> player_ctor_z (17.1).
bare = flat_level(-20, -20, 60, 60, 3, 0, (180, 0))
if W.terrain_height(bare, L.PLACEMENT_RADIUS, origin) != W.NO_FLOOR:
    raise SystemExit('bare field has a floor at the origin')
def bare_height(r, p):
    return W.terrain_height(bare, r, p)
def arena_entry0(records):
    entries, count = L.arena_register_spawn([], 1, 0, origin, 0)
    seen = False
    for cls, angle, pos in records:
        if cls == 'ArenaPlayerStart':
            entries, count = L.arena_register_spawn(entries, count, 0, pos, angle)
            seen = True
        elif cls == 'ArenaPlayerAlternate':
            entries, count = L.arena_register_spawn(entries, count, -1, pos, angle)
    return entries[0][0] if seen else None
def rec_list(records):
    return '{'+','.join('record(%s,{},Vec3%s,%d)' % (cstr(c), words(p), a) for c, a, p in records)+'}'
def fp(x, y, z):
    return (F(float(x)), F(float(y)), F(float(z)))
arena_sets = (
    (('ArenaPlayerStart', 0, origin),),
    (('ArenaPlayerStart', 90, fp(8, 0, 0)), ('ArenaPlayerAlternate', 4, fp(1, 2, 3)),
     ('ArenaPlayerStart', 0, origin)),
    (('ArenaPlayerAlternate', 4, fp(1, 2, 3)), ('ArenaPlayerStart', 0x1234, fp(150, 20, 7))),
)
for records in arena_sets:
    entry0 = arena_entry0(records)
    centre = L.arena_ring_centre(entry0, 0, 1, True)
    z = L.player_ctor_z(centre, L.PLACEMENT_RADIUS, bare_height)
    if z == W.NO_FLOOR:
        raise SystemExit('bare field lost the floor at the ring centre')
    out.append('{ auto g=round4_arena_pre_search(%s); CHECK(g.position==Vec3%s && g.spawn_z==%su && '
               'g.object_z==%su && g.ground && g.marker_taken && g.pre_search && g.rank==0 && '
               '!g.facing.has_value()); }' % (
                   rec_list(records), words((centre[0], centre[1], z)), pat(centre[2]), pat(z)))
    for seed in (0, 20973):
        world = Obj.World()
        world.shared = C.SharedRandom(seed)
        def search(c, spread, radius, any_height, world=world):
            return world.find_free_spot(c, spread, radius, any_height,
                                        lambda r, p: W.terrain_test_flag1(bare, r, p), bare_height)
        pos, facing, ok = L.arena_initial_place(entry0, 0, 1, True, search, table_facing=False, table_angle=0)
        snap = L.player_ctor_z(tuple(pos), L.PLACEMENT_RADIUS, bare_height)
        out.append('{ auto g=round4_arena_wave(%s,%d); CHECK(g.position==Vec3%s && g.spawn_z==%su && '
                   'g.object_z==%su && g.ground==%s && g.marker_taken && !g.pre_search && '
                   'g.facing.has_value() && *g.facing==%d); }' % (
                       rec_list(records), seed, words((pos[0], pos[1], snap)), pat(pos[2]), pat(snap),
                       str(snap != W.NO_FLOOR).lower(), facing & 0xFFFF))
a.output.write_text('\n'.join(out)+'\n')
print('Generated 192 piece, 90 marker, 24 complete spawn cases and one object fixture;')
print('round 2: %d light, %d chunk light, 900 WORLD pick, 100 manifest WORLD, %d recall calls, 6 request ticks, 401 move_piece' % (len(flags), len(cflags), calls))
print('round 3: 4 ring angles and %d arena plans' % len(plans))
print('round 4: mode-A steps, record offsets, ring centre, %d solid searches, 32 hole searches, lattice %d/%d, %d arena starts' % (
    17 * 2, misses, tested, len(arena_sets) * 3))
