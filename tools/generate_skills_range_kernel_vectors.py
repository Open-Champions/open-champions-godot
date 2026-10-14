#!/usr/bin/env python3
"""Author independent synthetic observations from immutable RTA Python contracts."""
import argparse
import gzip
import hashlib
import importlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PIN = '356add27d50e50972b2a671b31e8f4ec93e6778c'
MASK = 0xffffffff

def digest(data):
    return hashlib.sha256(data).hexdigest()

def generate(research, target):
    refs = json.loads((ROOT/'native/tests/vectors/skill_kernel.json').read_text())['references']
    for name, expected in refs.items():
        assert digest(subprocess.check_output(['git','-C',str(research),'show',PIN+':'+name])) == expected, name
        assert digest((research/name).read_bytes()) == expected, name
    sys.dont_write_bytecode = True
    sys.path.insert(0,str(research.resolve()/'tools'))
    modules = [importlib.import_module('ref_skills_'+p) for p in 'abc']
    core = importlib.import_module('ref_core')
    creature = importlib.import_module('ref_creature')
    cases = []
    # kind,value,random,spread,seed,alias,budget,adjust,object,slot,mode
    values = [0,1,7,0xffffffff,0x80000000,0x7fffffff]
    spreads = [0,0x80000000,0x3dcccccd,0x3f000000,0x3f000001,0x3effffff,
               0xbf800000,0x40000000,0x40400000,0x7fc12345,0xffc12345,0x7f800000,1]
    for i in range(len(values)*len(spreads)):
        for random in [0,1]:
            cases.append([0,values[i%6],random,spreads[i//6],0x12345678,0,100,0,0x1000,10,0])
    for alias in range(6):
        for budget in range(8):
            for random in [0,1]:
                cases.append([0,7,random,0x40400000,0x12345678,alias,budget,0xffff,0x1000,10,0])
    for obj in [0,1,0x1000,0xffffff00]:
        for adjust in [0,1,0x7fff,0x8000,0xffff]:
            for mode in [0,1,2,3,4,5,6,7]:
                cases.append([1,7,0,0,0x12345678,0,100,adjust,obj,10,mode])
    # Full raw32 operation surface, including values not reachable from from_int.
    for value in [0,0x80000000,0x3f800000,0xbf800000,0x7fc12345,0xffc12345,0x7f800000,1]:
        for spread in spreads:
            for seed in [0,0x10000,0x7fff0000,0xffff0000]:
                cases.append([2,value,0,spread,seed,0,100,0x8000,0,10,0])
    rows=[]
    for config in range(16):
        core.set_fpu(['ps2','ps2g1','chop','nearest'][config//4],bool(config&2),bool(config&1))
        for port,module in enumerate(modules):
            for case in cases:
                kind,value,random,spread,seed,alias,budget,adjust,obj,slot,mode=case
                class Memory(creature.Memory):
                    def store(self,at,width,v):
                        if len(self.log)>=budget:
                            raise RuntimeError('memory store history budget exceeded')
                        super().store(at,width,v)
                    def load(self,at,width):
                        failure={1:obj+0x60,2:0x2050,3:0x2054}.get(mode)
                        if kind==1 and at==failure:
                            raise RuntimeError('range read probe')
                        return super().load(at,width)
                mem=Memory()
                for at,width,v in [(obj+0x60,4,0x2000),(0x2050,2,adjust),(0x2054,4,0x3c85fc if mode==7 else 0x9000)]:
                    if not (kind==1 and at=={1:obj+0x60,2:0x2050,3:0x2054}.get(mode)):
                        mem.poke(at,width,v)
                mem.poke(0x404464,4,seed);mem.poke(0x8010,4,seed);mem.poke(0x1024,4,value)
                calls=[]
                model=module.Model(mem,external=('Skill.v10',) if mode==7 else ())
                def callback(*args):
                    calls.append(tuple(x&MASK for x in args))
                    if mode==4: raise RuntimeError('range callback probe')
                    if mode==5:
                        model.spell_rand(0x8000)
                        raise RuntimeError('range callback probe')
                    if mode==6:
                        def replacement(*args):
                            calls.append(tuple(x&MASK for x in args))
                            return (value+1)&MASK
                        model.cb['Skill.v10']=replacement
                    return value
                model.cb['Skill.v10']=callback
                status='ok'; result=[]
                lo=0x404464 if alias in [1,3] else 0x1024 if alias==4 else 0x5000
                hi=0x404464 if alias in [2,3] else 0x1018 if alias==4 else lo if alias==5 else 0x5004
                try:
                    if kind==0:
                        if port==1: model.skill_stat_range(obj,0,1,random,lo,hi,spread)
                        else: result=list(model.skill_stat_range(obj,0,1,random,spread))
                    elif kind==1:
                        result=[model.skill_call(obj,slot,0,1)]
                        if mode==6: result.append(model.skill_call(obj,slot,0,1))
                    else:
                        fixed=core.mul(value,spread)
                        scaled=core.mul(core.mul(value,core.mul(core.from_int(seed>>16),0x37800000)),spread)
                        result=[fixed,scaled,core.sub(value,fixed),core.add(value,scaled),(obj+(adjust-65536 if adjust&32768 else adjust))&MASK]
                except RuntimeError as error:
                    status=str(error)
                def writes(entries): return ''.join(f'{a},{n},{v&MASK};' for a,n,v in entries)
                expected='|'.join([status,','.join(str(x&MASK) for x in result),str(model.draws),
                    writes([] if port==1 else model.writes),writes(mem.log),
                    ''.join(','.join(map(str,a))+';' for a in calls),
                    ''.join(f'{a},{v};' for a,v in sorted(mem.bytes.items()))])
                rows.append(' '.join(map(str,[config,port]+case))+'\t'+expected+'\n')
    plain=''.join(rows).encode('ascii')
    target.write_bytes(gzip.compress(plain,mtime=0))
    target.with_suffix('').with_suffix('.json').write_text(json.dumps({
        'reference_commit':PIN,'references':refs,'rows':len(rows),'vector_sha256':digest(plain),
        'compressed_sha256':digest(target.read_bytes()),'generator_sha256':digest(Path(__file__).read_bytes())},indent=2)+'\n')
    print(f'Generated {len(rows)} observations; SHA256 {digest(plain)}')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=Path,required=True)
    parser.add_argument('--vectors',type=Path,default=ROOT/'native/tests/vectors/skills_range_kernel.tsv.gz')
    args=parser.parse_args()
    generate(args.research_dir,args.vectors)
