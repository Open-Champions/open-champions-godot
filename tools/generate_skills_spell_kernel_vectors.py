#!/usr/bin/env python3
"""Independent spell observations from immutable Python bodies, synthetic inputs only."""
import argparse
import gzip
import hashlib
import importlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PIN = '356add27d50e50972b2a671b31e8f4ec93e6778c'
MASK = 0xffffffff
GLOBAL = 0x404464
SPAN = 'memory address, width or span outside supported domain'
OFFSET = 'address offset exceeds raw32 memory domain'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def generate(research, target):
    refs = json.loads((ROOT/'native/tests/vectors/skills_range_kernel.json').read_text())['references']
    # Import only Git-object bytes, after checking the existing manifests and
    # read-only checkout. HEAD is never an oracle and no bytecode is written.
    with tempfile.TemporaryDirectory(prefix='spell-reference-') as folder:
        for name, expected in refs.items():
            data = subprocess.check_output(['git', '-C', str(research), 'show', PIN+':'+name])
            assert digest(data) == expected, name
            assert digest((research/name).read_bytes()) == expected, name
            if name.startswith('tools/ref_'):
                (Path(folder)/Path(name).name).write_bytes(data)
        sys.dont_write_bytecode = True
        sys.path.insert(0, folder)
        modules = [importlib.import_module('ref_skills_'+p) for p in 'abc']
        creature = importlib.import_module('ref_creature')
        rows = []
        cases = []
        # Every prefix field and private/final seed publication can alias shared
        # RNG, including partial byte overlaps at all offsets around the object.
        for seed in [0, MASK, 0x12345678, 0x80000000, 0x10000]:
            for spell in [0x8000, *range(GLOBAL-24, GLOBAL+5)]:
                for level in [0, 0x100, MASK]:
                    pokes = [(GLOBAL,4,seed),(spell,4,0xa1b2c3d4),(spell+0x10,4,seed)]
                    cases.append((pokes, [(0,spell,level,0x12345678,0xabcdef01,0xdeadbeef),
                                           (1,spell),(1,spell),(1,spell)]))
        # Constructors: every store-budget prefix, including the draw store,
        # seed store and final table. Model budget is independent of Memory.
        prefixes = [(0,0x8000,MASK,0x12345678,0xabcdef01,0xdeadbeef)]
        faults = [([],prefixes), ([(GLOBAL,4,MASK)],prefixes),
                  ([(0x8010,4,MASK)],[(1,0x8000)]),
                  ([],[(1,0x8000),(1,0x8000)]),
                  ([],[(0,GLOBAL-16,0x100,7,9,11),(1,GLOBAL-16)])]
        rowspec = [(p,o,65536,65536,0) for p,o in cases]
        # Modular-inverse seeds force the complete stepped state to zero/max;
        # expected values still come exclusively from the pinned draw bodies.
        inverse = pow(0x19660d,-1,1<<32)
        seeds = [0,MASK,0x80000000,0x12345678,
                 (-0x3c6ef35f*inverse)&MASK,((MASK-0x3c6ef35f)*inverse)&MASK]
        for seed in seeds:
            for spell in [0x8000,GLOBAL-16,GLOBAL-17,GLOBAL-15]:
                rowspec.append(([(GLOBAL,4,0xa1b2c3d4),(spell+16,4,seed)],
                                [(1,spell)]*4,65536,65536,0))
        for p,o in faults:
            rowspec += [(p,o,n,65536,0) for n in range(10)]
            rowspec += [(p,o,65536,n,0) for n in range(10)]
            rowspec += [(p,o,65536,65536,n) for n in range(1,16)]
        # Raw32 span edges: A can draw after its shorter prefix; C/B encounter
        # the earlier temporary-table store. Invalid input remains safety proof.
        for spell in [0, MASK-0x18, MASK-0x17, MASK-0x14, MASK-0x13,
                      MASK-0x10, MASK-0xf, MASK-3, MASK]:
            rowspec += [([],[(0,spell,0x12345678,7,9,11)],65536,65536,0),
                        ([],[(1,spell)],65536,65536,0)]
        for port,module in enumerate(modules):
            selected = list(rowspec)
            if port == 1:
                selected += [([],[(0,1<<32,1,7,9,11)],65536,65536,0),
                             ([],[(1,1<<32)],65536,65536,0)]
                for seed in [0,MASK,0x12345678]:
                    selected.append(([(GLOBAL,4,seed),(0x8044,4,seed),
                                      (0x404460,4,seed),(0x404468,4,seed)],
                                     [(2,0x8000),(3,),(4,),(5,),(2,0x8000)],65536,65536,0))
            for pokes,ops,budget,room,fail in selected:
                reads = []
                class Under:
                    def load(self, at, width):
                        reads.append((at,width))
                        if fail and len(reads) == fail:
                            raise RuntimeError('spell read probe')
                        return 0
                # Authored native safety policy only. Algorithmic expectations
                # and draw accounting execute the unmodified pinned methods.
                class Memory(creature.Memory):
                    @staticmethod
                    def span(at,width):
                        if at > MASK or width > 8 or at+width > 1<<32:
                            raise RuntimeError(SPAN)
                    def load(self,at,width):
                        self.span(at,width)
                        return super().load(at,width)
                    def store(self,at,width,value):
                        self.span(at,width)
                        if len(self.log) >= budget:
                            raise RuntimeError('memory store history budget exceeded')
                        super().store(at,width,value)
                mem = Memory(Under())
                for poke in pokes: mem.poke(*poke)
                model = module.Model(mem)
                shared = False
                original_draw = model.draw
                def draw():
                    nonlocal shared
                    shared = True
                    try: return original_draw()
                    finally: shared = False
                model.draw = draw
                def write(at,width,value):
                    if port == 1:
                        # B checked offsets (level byte has no offset).
                        if width == 4 and at > MASK: raise RuntimeError(OFFSET)
                    else:
                        if len(model.writes) >= room:
                            raise RuntimeError('Skill write history limit' if port == 0 else 'write history limit')
                        if port == 2 and not shared: mem.load(at,width)
                        model.writes.append((at & MASK,width,value & ((1<<(width*8))-1)))
                    mem.store(at,width,value)
                model._w = write
                status = 'ok'; result = []
                try:
                    for op,*args in ops:
                        if port == 1 and op in [1,2] and args[0]+(16 if op==1 else 68)>MASK:
                            raise RuntimeError(OFFSET)
                        if op == 0:
                            (model.spell_base_ctor if port==1 else model.spell_base)(*args)
                        elif op == 1: result.append(model.spell_rand(*args))
                        elif op == 2: result.append(model.obj_rand(*args))
                        elif op == 3: result.append(model.cosmetic_draw())
                        elif op == 4: result.append(model.seed3_draw())
                        elif op == 5: result.append(model.draw())
                except RuntimeError as error: status = str(error)
                def entries(values): return ''.join(','.join(map(str,v))+';' for v in values)
                expected = '|'.join([status,','.join(map(str,result)),str(model.draws),
                                     entries(model.writes),entries(mem.log),entries(reads),
                                     entries(sorted(mem.bytes.items()))])
                tokens = [port,budget,room,fail,len(pokes)]
                for poke in pokes: tokens.extend(poke)
                tokens.append(len(ops))
                for op,*args in ops: tokens.extend([op,len(args),*args])
                rows.append(' '.join(map(str,tokens))+'\t'+expected+'\n')
        plain = ''.join(rows).encode('ascii')
        target.write_bytes(gzip.compress(plain,mtime=0))
        target.with_suffix('').with_suffix('.json').write_text(json.dumps({
            'reference_commit':PIN,'references':refs,'rows':len(rows),
            'vector_sha256':digest(plain),'compressed_sha256':digest(target.read_bytes()),
            'generator_sha256':digest(Path(__file__).read_bytes()),
            'safety_policy':'Existing native raw32 spans, B checked offsets, per-port Model and Memory budgets; read-probe injection'},indent=2)+'\n')
        print(f'Generated {len(rows)} independent spell observations; SHA256 {digest(plain)}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=Path,required=True)
    parser.add_argument('--vectors',type=Path,default=ROOT/'native/tests/vectors/skills_spell_kernel.tsv.gz')
    args = parser.parse_args()
    generate(args.research_dir,args.vectors)
