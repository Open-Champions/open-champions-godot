#!/usr/bin/env python3
"""Generate bounded synthetic handle observations from pinned Python methods only."""
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
FREE = 0x864ea8

def digest(data):
    return hashlib.sha256(data).hexdigest()

def generate(research, target):
    refs = json.loads((ROOT/'native/tests/vectors/skills_range_kernel.json').read_text())['references']
    # Include the documented node layout/known clear corruption boundary.
    path = 'docs/objects.md'
    refs[path] = digest(subprocess.check_output(['git','-C',str(research),'show',PIN+':'+path]))
    for name, expected in refs.items():
        assert digest(subprocess.check_output(['git','-C',str(research),'show',PIN+':'+name])) == expected, name
        assert digest((research/name).read_bytes()) == expected, name
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(research.resolve()/'tools'))
    modules = [importlib.import_module('ref_skills_'+p) for p in 'bc']
    creature = importlib.import_module('ref_creature')
    scenarios = []
    # Explicit authored sparse pokes, then link/remove/clear/items commands.
    for pool in range(3):
        for head, back in [(0x5000,0x6000),(0x4000,0x6000),(0x5000,0x4000),
                           (FREE,0x6000),(0x5000,FREE),(0x5000,0x5000),
                           (0x4004,0x4010),(0,4),(0x4008,0x4108),(0x4001,0x6000)]:
            pokes = [(FREE,4,0x4000 if pool else 0),(0x4000,4,0x4100 if pool==2 else 0),
                     (0x4100,4,0),(0x4010,4,0xa1b2ffff),(0x4110,4,0xc3d48001)]
            scenarios.append((pokes,[(0,head,back,0xdeadbeef)]))
    # Nonempty lists and removal with and without partner; repeated reuse/clear.
    base=[(FREE,4,0x4000),(0x4000,4,0x4100),(0x4100,4,0),
          (0x4010,4,0xa1b2ffff),(0x4110,4,0xc3d48001),
          (0x5000,4,0x4200),(0x4200,4,0),(0x4204,4,0x5000),
          (0x4208,4,0),(0x420c,4,9),(0x6000,4,0x4300),
          (0x4300,4,0),(0x4304,4,0x6000),(0x4308,4,0),(0x430c,4,0)]
    scenarios += [(base,[(0,0x5000,0x6000,7),(3,0x5000),(1,0x4000),(2,0x5000),(2,0x6000),
                          (0,0x5000,0x6000,8),(2,0x6000),(2,0x5000)]),
                  (base,[(1,0x4200),(1,0x4300)]),
                  (base,[(2,0x5000),(2,0x5000),(0,0x5000,0x6000,8),(1,0x4200)])]
    # Unlink aliases: prev/node/next/free head/partner fields.
    for prev,nxt,partner in [(0,0,0),(0x4000,0x4100,0),(FREE,0x4100,0),
                              (0x4008,0x4100,0x4100),(0x5000,0x4000,0),
                              (0x5000,0x4100,0x4000),(0x5000,0,FREE)]:
        scenarios.append(([(FREE,4,0x4400),(0x4000,4,nxt),(0x4004,4,prev),
                           (0x4008,4,partner),(0x400c,4,0xabcdef01),
                           (0x4100,4,0),(0x4104,4,0x6000),(0x4108,4,0x4000)],[(1,0x4000)]))
    # A cycle is tested only with an injected finite write budget; never run
    # the unbounded Python clear loop without a terminating store refusal.
    cycle=([(FREE,4,0x4000),(0x5000,4,0x4000),(0x4000,4,0x4000),
            (0x4004,4,0),(0x4008,4,0)],[(2,0x5000)])
    rows=[]
    alias_results={}
    for port,module in enumerate(modules):
        for scenario,(pokes,ops) in enumerate(scenarios+[cycle]):
            # The longest successful sequence has 50 stores: budgets 0..50
            # cover every write boundary, including late reuse/clear operations.
            limits=[(n,65536) for n in range(51)]
            if scenario<len(scenarios):
                limits.append((65536,65536))
                if port: limits += [(65536,n) for n in range(51)]
            for budget,room in limits:
                class Memory(creature.Memory):
                    def store(self,at,width,value):
                        if len(self.log)>=budget:
                            raise RuntimeError('memory store history budget exceeded')
                        super().store(at,width,value)
                mem=Memory()
                for poke in pokes: mem.poke(*poke)
                model=module.Model(mem)
                # Only existing native adapter history policy is overlaid; all
                # list algorithms and expected full states come from the pin.
                def write(at,width,value):
                    if port:
                        if len(model.writes)>=room: raise RuntimeError('write history limit')
                        mem.load(at,width) # C's established write preflight
                        model.writes.append((at,width,value & ((1<<(8*width))-1)))
                    mem.store(at,width,value)
                model._w=write
                status='ok';result=[]
                try:
                    for op,*args in ops:
                        if op==0: result.append(model.handle_link(*args))
                        elif op==1: result.append((model.handle_remove if port else model.handle_removeNode)(*args))
                        elif op==2:
                            model.handle_clear(*args)
                            if not port: result.append(0) # B public return contract
                        elif op==3:
                            if port:
                                result.extend(v for pair in model.handle_items(*args) for v in pair)
                            else: result.extend(model.list_objects(*args))
                except RuntimeError as error: status=str(error)
                def writes(entries): return ''.join(f'{a},{n},{v};' for a,n,v in entries)
                expected='|'.join([status,','.join(map(str,result)),writes(model.writes),writes(mem.log),'',
                                   ''.join(f'{a},{v};' for a,v in sorted(mem.bytes.items()))])
                tokens=[port,budget,room,len(pokes)]
                for poke in pokes: tokens.extend(poke)
                tokens.append(len(ops))
                for op,*args in ops: tokens.extend([op,len(args),*args])
                rows.append(' '.join(map(str,tokens))+'\t'+expected+'\n')
                if scenario==29 and budget==room==65536: alias_results[port]=(writes(mem.log),dict(mem.bytes))
    assert alias_results[0]!=alias_results[1], 'cached versus reread insertion regression'
    plain=''.join(rows).encode('ascii');target.write_bytes(gzip.compress(plain,mtime=0))
    target.with_suffix('').with_suffix('.json').write_text(json.dumps({
        'reference_commit':PIN,'references':refs,'rows':len(rows),'vector_sha256':digest(plain),
        'compressed_sha256':digest(target.read_bytes()),'generator_sha256':digest(Path(__file__).read_bytes())},indent=2)+'\n')
    print(f'Generated {len(rows)} independent handle observations; SHA256 {digest(plain)}')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=Path,required=True)
    parser.add_argument('--vectors',type=Path,default=ROOT/'native/tests/vectors/skills_handle_kernel.tsv.gz')
    args=parser.parse_args();generate(args.research_dir,args.vectors)
