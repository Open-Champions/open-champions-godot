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
a.output.write_text('\n'.join(out)+'\n')
print('Generated 192 piece, 90 marker, 24 complete spawn cases and one object fixture')
