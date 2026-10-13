#!/usr/bin/env python3
"""Execute the immutable RTA statistics/passive contract on authored sparse memory.

No game files, retail tables, decompiler text, or native results are read.
"""
import argparse
import copy
import gzip
import hashlib
import importlib
import json
import pathlib
import random
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'native/tests/vectors/skills_c_evidence.json'
METHODS = ('skill_get_stat', 'skill_get_stat_tier2', 'skill_stat_for',
           'skill_stat_for_handle', 'skill_adjust_level', 'skill_mana_from_stat4',
           'skill_stat_range', 'skill_can_use_default', 'skill_cast_default',
           'skill_mana_default', 'skill_mana_cost', 'statistic_spread',
           'berserkerrage_swing_factor', 'berserkerrage_damage_factor',
           'passive_complement', 'throwing_fraction', 'battlemage_fraction',
           'penetratingarrows_fractions')
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')
S, TABLE, RECORD, OBJ, HANDLE, NODE = 0x1000, 0x2000, 0x10000, 0x3000, 0x4000, 0x4100
M32 = 0xffffffff


def flatten(value):
    if isinstance(value, (list, tuple)):
        return [len(value)] + [n for v in value for n in flatten(v)]
    return [int(value)]


def encode(value):
    return ' '.join(map(str, flatten(value)))


def snapshot(model, configs):
    return copy.deepcopy([sorted(model.mem.bytes.items()), model.mem.log, model.writes,
            [[int(name.split('v')[1]), list(args)] for name, args in model.calls],
            model.draws, sorted(int(name.split('v')[1]) for name in model.external), configs])


def observe(SK, RC, C, config, scenario):
    C.set_fpu(MODELS[config // 4], bool(config & 2), bool(config & 1))
    pokes, externals, configs, commands = scenario
    mem = RC.Memory()
    for address, width, value in pokes:
        mem.poke(address, width, value)
    callbacks = {}
    for slot, result, poke, throwing in configs:
        def callback(*args, result=result, poke=poke, throwing=throwing):
            if poke:
                mem.poke(*poke)
            if throwing:
                raise RuntimeError('authored callback exception')
            return result[0] if len(result) == 1 else tuple(result)
        callbacks['Skill.v%d' % slot] = callback
    model = SK.Model(mem, callbacks, ['Skill.v%d' % slot for slot in externals])
    out = [snapshot(model, configs)]
    for operation, args in commands:
        try:
            value = getattr(model, METHODS[operation])(*args)
            result = list(value) if isinstance(value, tuple) else [value & M32]
            result = [v & M32 for v in result]
            status = 0
        except RuntimeError:
            status, result = 1, []
        out.append([status, result, snapshot(model, configs)])
    return encode(out)


def setup(SK, rng, variant):
    pokes = [[S+0x60, 4, TABLE], [SK.G_RAND_SEED, 4, rng.getrandbits(32)],
             [RECORD+0xac, 4, (0,0x3f000000,0x3f800000,0xbf800000,0x7fffffff)[variant%5]],
             [RECORD+0xb0, 4, (0x3f800000,0,0xbf800000)[variant%3]],
             [RECORD+0x43d8,4, OBJ if variant%4 else 0], [OBJ+0xc,4,0x40 if variant%2 else 0x20],
             [HANDLE,4,NODE if variant%3 else 0], [NODE+0xc,4,OBJ if variant%5 else 0], [0xc,4,0x40 if variant%7==0 else 0]]
    for slot, body in SK.SKILL_BASE.items():
        pokes += [[TABLE+slot*8,2,0],[TABLE+slot*8+4,4,body]]
    steps = [0, 0x3f000000,0xbf000000,0x40400fb6,0x40001034,0x40800fe6,0x3e800000,0x7fffffff,0x80000000]
    for i in range(5):
        pokes += [[S+0x0e+2*i,2, rng.choice([-3,0,1,5,20])&0xffff],
                  [S+0x18+2*i,2,rng.choice([-32768,-120,-1,0,1,20,100,32767])&0xffff],
                  [S+0x24+4*i,4,rng.choice(steps)],
                  [S+0x38+2*i,2,rng.choice([-1,0,1,3,20])&0xffff],
                  [S+0x42+2*i,2,rng.choice([-100,0,10,32767])&0xffff],
                  [S+0x4c+4*i,4,rng.choice(steps)]]
    for i in range(8):
        pokes += [[OBJ+0x90+2*i,2,rng.randrange(65536)]]
    return pokes


def scenarios(SK):
    rng = random.Random(69101)
    levels = [0,1,2,5,20,40,41,42,60,100,127,M32,M32-2,0x80000000,0x7fffffff]
    for variant in range(96):
        pokes = setup(SK,rng,variant)
        commands = []
        for stat in range(5):
            level=levels[(variant+stat)%len(levels)]
            cap=[0,10,20,25,M32][(variant+stat)%5]
            commands += [[0,[S,stat,level]], [1,[S,stat,level,cap]],
                         [2,[S,stat,level,OBJ,cap]], [2,[S,stat,level,0,cap]],
                         [3,[S,stat,level,HANDLE,cap]],
                         [6,[S,stat,level,variant%2,[0,0x3e4ccccc,0x3f800000,0xbf800000,0x40000000][variant%5]]],
                         [11,[S,stat,level,OBJ]], [11,[S,stat,level,0]]]
        rank = levels[variant%len(levels)]
        commands += [[4,[S,rank,0x14]], [5,[S,rank,2]], [7,[S,OBJ]],
                     [8,[S,rank,OBJ,0,0]], [9,[S,rank]], [10,[S,rank]],
                     [12,[S,rank,RECORD,[0,1,30,59,60,61,120,M32,0x80000000][variant%9]]],
                     [13,[S,rank,RECORD,30]], [14,[S,rank]], [15,[S,rank]],
                     [16,[S,rank]], [17,[S,rank]]]
        yield [pokes,[],[],commands]
    # All delegated shapes, forced and unknown-body dispatch, and mutation before
    # failure. Adjustment receives its own signed16 this pointer on each route.
    for slot in (8,10,11,12,14,15):
        for throwing in (0,1):
            for forced in (0,1):
                pokes=setup(SK,rng,slot)
                pokes += [[TABLE+8*slot,2,0xfff0]]
                if not forced: pokes += [[TABLE+8*slot+4,4,0xdeadbeef]]
                configs=[[slot,[23,31] if slot==8 else [0xffffffed], [OBJ+0x94,2,0x3456],throwing]]
                commands=[[10,[S,20]],[6,[S,0,20,1,0x40000000]],
                          [11,[S,0,20,OBJ]],[11,[S,0,20,0]],
                          [12,[S,20,RECORD,30]],[13,[S,20,RECORD,30]],
                          [14,[S,20]],[15,[S,20]],[16,[S,20]],[17,[S,20]]]
                yield [pokes,[slot] if forced else [],configs,commands]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',required=True,type=pathlib.Path)
    args=parser.parse_args()
    pin=json.loads(EVIDENCE.read_text())
    # Scratch files are authored Python contracts retrieved from exact Git
    # objects, never copied from the research working tree or committed here.
    (ROOT/'work').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT/'work',prefix='skills-c-pin-') as tmp:
        for path,digest in pin['files'].items():
            data=subprocess.check_output(['git','-C',str(args.research_dir),'show',pin['commit']+':'+path])
            if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('reference hash mismatch: '+path)
            if path.startswith('tools/ref_'): pathlib.Path(tmp,pathlib.Path(path).name).write_bytes(data)
        sys.path.insert(0,tmp)
        SK=importlib.import_module('ref_skills_c'); RC=importlib.import_module('ref_creature'); C=importlib.import_module('ref_core')
        cases=list(scenarios(SK))
        lines=['# skills_c_vectors_v1\n']
        for config in range(16):
            for case in cases:
                lines.append(f'{config}\t{encode(case)}\t{observe(SK,RC,C,config,case)}\n')
    data=''.join(lines).encode('ascii')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('wb') as f:
        with gzip.GzipFile(filename='',fileobj=f,mode='wb',mtime=0) as z:z.write(data)
    manifest={'schema_version':1,'suite':'skills-c','rows':len(lines)-1,'scenario_count':len(cases),
              'vector_sha256':hashlib.sha256(data).hexdigest(), 'evidence':pin,
              'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
              'protocol':'config TAB recursive count-prefixed integer input TAB complete observation; see docs/SKILLS_C.md'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(lines)-1} exact complete-state vectors')


if __name__=='__main__':main()
