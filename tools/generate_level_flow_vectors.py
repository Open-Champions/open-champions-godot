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
a.output.write_text('\n'.join(out)+'\n')
print('Generated 192 piece, 90 marker, 24 complete spawn cases and one object fixture;')
print('round 2: %d light, %d chunk light, 900 WORLD pick, 100 manifest WORLD, %d recall calls, 6 request ticks, 401 move_piece' % (len(flags), len(cflags), calls))
