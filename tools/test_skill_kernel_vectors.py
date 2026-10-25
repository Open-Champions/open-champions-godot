#!/usr/bin/env python3
"""Independent pinned Python expectations for the RTA kernel/adaptor boundary.

Generation reads only public contracts and authored inputs, never native output.
Normal checks need no research checkout. Historical skills corpora are untouched.
"""
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


def digest(data):
    return hashlib.sha256(data).hexdigest()


def s32(v):
    return v - (1 << 32) if v & (1 << 31) else v


def s16(v):
    v &= 65535
    return v - 65536 if v & 32768 else v


def generate(research, target):
    references = {}
    for port in 'abc':
        manifest = json.loads((ROOT / f'native/tests/vectors/skills_{port}.json').read_text())
        references.update(manifest.get('evidence_sha256', manifest.get('references',
                           manifest.get('evidence', {}).get('files', {}))))
    for name, expected in references.items():
        assert digest(subprocess.check_output(['git', '-C', str(research), 'show', PIN+':'+name])) == expected
        assert digest((research / name).read_bytes()) == expected
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(research.resolve() / 'tools'))
    modules = [importlib.import_module('ref_skills_'+p) for p in 'abc']
    core = importlib.import_module('ref_core')
    creature = importlib.import_module('ref_creature')
    cases = []
    levels = [0, 1, 2, 32767, 65535, 0x7fffffff, 0x80000000, 0xffffffff]
    steps = [0,0x80000000,0x3f000000,0xbf000000,0x3f800000,0x4f000000,
             0x7f800000,0xff800000,0x7fc12345,0xffc12345,0x00800000,0x00000001]
    for i in range(96):
        cases.append([0, levels[i % 8], [0,1,32767,0x80000000,0xffffffff][i % 5],
                      [0,1,32767,32768,65535][i % 5],
                      [0,1,32767,32768,65535][i // 5 % 5], steps[i // 8 % 12],
                      [0,1,32767,32768,65535][i // 3 % 5],
                      [32768,32767,65535][i % 3], steps[(i+5) % 12],
                      [0,1,0x7fffffff,0x80000000,0xffffffff,0x12345678][i % 6]])
    # Adjacent signed indices versus B's explicit index refusals, including zero.
    for stat in [0xffffffff,4,5,0x80000000,0x7fffffff]:
        for level in ([0] if stat in [0x80000000,0x7fffffff] else [0,1,0x7fffffff]):
            cases.append([stat,level,1,65535,32768,0x3f000000,32768,32767,0xbf000000,0xffffffff])
    rows = []
    for config in range(16):
        core.set_fpu(['ps2','ps2g1','chop','nearest'][config // 4],bool(config & 2),bool(config & 1))
        for port, module in enumerate(modules):
            for op in [0,1,2]:
                selected = cases if op<2 else [
                    [0,1,alias,1,7,0,65535,0,spread,0x12345678]
                    for alias in range(6) for spread in [0x40400000,0xbf800000,0x7fc12345]]
                for case in selected:
                    stat,level,cap,f,b,step,f2,b2,step2,seed = case
                    memory = creature.Memory()
                    for off,width,value in [(0xe,2,f),(0x18,2,b),(0x24,4,step),
                                            (0x38,2,f2),(0x42,2,b2),(0x4c,4,step2)]:
                        address = 0x1000 + off + s32(stat)*width
                        if 0 <= address and address+width-1 <= MASK:
                            memory.poke(address,width,value)
                    memory.poke(0x404464,4,seed); memory.poke(0x8010,4,seed)
                    if op==2:
                        memory.poke(0x1060,4,0x2000); memory.poke(0x2054,4,0x3c85fc)
                    lo=0x404464 if cap in [1,3] else 0x1024 if cap==4 else 0x5000
                    hi=0x404464 if cap in [2,3] else 0x1018 if cap==4 else lo if cap==5 else 0x5004
                    model = module.Model(memory)
                    status='ok'; value=shared=own=0
                    # These are pre-existing native domain refusals, not changes
                    # to the reference's broader Python memory/integer domain.
                    if op<2 and port == 1 and stat > 4:
                        status='only statistics 0..4 are supported'
                    elif op<2 and port == 1 and level:
                        lv=s32(level)
                        if op and s32(cap)>0: lv=min(lv,s32(cap))
                        off=0x38 if op else 0xe
                        first=s16(memory.load(0x1000+off+2*stat,2))
                        if op and first<0: first=s16(memory.load(0x100e+2*stat,2))
                        if lv-first>0x7fffffff:
                            status='level delta exceeds core signed32 conversion domain'
                    if status=='ok' and op==2:
                        if port==1:
                            model.skill_stat_range(0x1000,stat,level,1,lo,hi,step2)
                            value=memory.load(lo,4); shared=memory.load(hi,4)
                        else:
                            value,shared=model.skill_stat_range(0x1000,stat,level,1,step2)
                    elif status=='ok':
                        if port==0:
                            method=model.skill_stat_tier2 if op else model.skill_stat
                        else:
                            method=model.skill_get_stat_tier2 if op else model.skill_get_stat
                        value=method(0x1000,stat,level,*([cap] if op else [])) & MASK
                        if port==2:
                            model.spell_base(0x8000,7,0x9000,0x1000,0x6000)
                            shared=memory.load(0x8010,4)
                        else:
                            shared=model.draw()
                        own=model.spell_rand(0x8000)
                    delta=(level-s16(f)) & MASK
                    raw=core.add(core.from_int(s16(b)),core.mul(step,core.from_int(delta)))
                    capped=cap if s32(cap)>0 and s32(cap)<s32(level) else level
                    stepped=modules[0].lcg(seed)
                    fields=[status,value,shared,own,model.draws,0 if port==1 else len(model.writes),
                            raw,capped,int(s16(f2)<0),stepped,stepped>>16]
                    log=''.join(f'{a},{n},{v};' for a,n,v in memory.log)
                    data=''.join(f'{a},{v};' for a,v in sorted(memory.bytes.items()))
                    expected='|'.join(map(str,fields))+'|'+log+'|'+data
                    source=' '.join(map(str,[config,port,op]+case))
                    rows.append(source+'\t'+expected+'\n')
    plain=''.join(rows).encode('ascii')
    target.write_bytes(gzip.compress(plain,mtime=0))
    target.with_suffix('').with_suffix('.json').write_text(json.dumps({
        'reference_commit':PIN,'references':references,'rows':len(rows),
        'vector_sha256':digest(plain),'generator_sha256':digest(Path(__file__).read_bytes())},indent=2)+'\n')
    print(f'Generated {len(rows)} independent observations; SHA256 {digest(plain)}')


def check(exe, vectors):
    manifest=json.loads(vectors.with_suffix('').with_suffix('.json').read_text())
    plain=gzip.decompress(vectors.read_bytes())
    assert manifest['reference_commit']==PIN
    assert digest(plain)==manifest['vector_sha256']
    assert digest(Path(__file__).read_bytes())==manifest['generator_sha256']
    rows=[line.split('\t') for line in plain.decode('ascii').splitlines()]
    assert len(rows)==manifest['rows']
    with tempfile.TemporaryDirectory(prefix='skill-kernel-') as directory:
        source=Path(directory)/'input'; actual=Path(directory)/'actual'
        source.write_text('\n'.join(row[0] for row in rows)+'\n')
        subprocess.run([str(exe.resolve()),str(source),str(actual)],check=True)
        results=actual.read_text().splitlines()
        assert len(results)==len(rows)
        for index,(result,row) in enumerate(zip(results,rows)):
            if result!=row[1]:
                raise ValueError(f'kernel/adaptor mismatch row {index}: {row[0]}\n{result}\n{row[1]}')
        print(f'{len(rows)} independent kernel/adaptor observations passed; trace SHA256 {digest(actual.read_bytes())}')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generate',action='store_true')
    parser.add_argument('--research-dir',type=Path)
    parser.add_argument('--exe',type=Path)
    parser.add_argument('--vectors',type=Path,default=ROOT/'native/tests/vectors/skill_kernel.tsv.gz')
    args=parser.parse_args()
    if args.generate:
        if args.research_dir is None: parser.error('--generate requires --research-dir')
        generate(args.research_dir,args.vectors)
    else:
        if args.exe is None: parser.error('--exe required')
        check(args.exe,args.vectors)


if __name__=='__main__':
    main()
