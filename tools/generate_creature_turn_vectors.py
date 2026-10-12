#!/usr/bin/env python3
"""Explicit synthetic Creature_turnToward inputs executed by immutable Python.

The authored memory and command sequences are supplied to both implementations.
Only the pinned original method computes turning; no native output is imported.
"""
import argparse
import collections
import gzip
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import types
import copy
import struct

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'tools/ref_creature.py', 'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'docs/spec/creature-tick.md', 'sha256': 'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
COMPOSITION_POLICY = 'rta-creature-turn-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')

INPUT_PROTOCOL = {
    'version': 1,
    'header': '# creature_turn_vectors_v1',
    'columns': ['operation', 'configuration', 'id;input', 'status', 'observation'],
    'input': '[initial_pokes,operations]',
    'initial_poke': '[address,size,value] (unlogged sparse write; size 0..8)',
    'commands': {'0': '[0,address,signed_mode,[xRaw,yRaw,zRaw]]',
                 '1': '[1,address,size,value] (unlogged poke)',
                 '2': '[2,address,size,value] (logged memory store)'},
    'observation': '[submitted_input,initial_snapshot,[[status,result,snapshot],...]]',
    'snapshot': ['sorted_sparse_byte_pairs','memory_store_log','model_writes',
                 'model_calls','model_events','draws','callback_names','external_names',
                 'policy','borrowed_memory','overlay_present'],
    'defaults': {'policy': 0, 'borrowed_memory': True, 'overlay_present': False,
                 'limits': [1048576,65536,64,64], 'callbacks': [], 'external': []},
    'events': 'write=[write,address,size,maskedvalue]; call=[call,name,typed_arguments]',
    'statuses': 'ok or actual original ValueError/IndexError/OverflowError; top-level ok means completed observation',
    'configuration': 'model=config//4; mul_bias=bool(config&2); div_nearest=bool(config&1)',
    'canonical': 'integer decimal, null _, strings sUTF8hex, bytes bHEX, lists [comma-separated]'
}
CONFIGURATIONS = tuple(range(16))
C = 0x10000
G_SIM_FRAME, G_WORLD, G_RAND_SEED = 0x44fdcc, 0x44fdc8, 0x404464


def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, bytes): return 'b' + value.hex()
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned turn observation')


def typed(value):
    if isinstance(value, tuple): return ['tuple', [typed(v) for v in value]]
    if isinstance(value, list): return ['list', [typed(v) for v in value]]
    return value


def load_references(research_dir, scratch_dir):
    """Never import a live research working-tree file or follow its module path."""
    scratch_dir = pathlib.Path(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)
    modules = {}
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    try:
        with tempfile.TemporaryDirectory(prefix='pinned-', dir=scratch_dir) as directory:
            for name in ('core', 'creature', 'specification'):
                pin = SPECIFICATION_EVIDENCE if name == 'specification' else REFERENCES[name]
                source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']],
                                        capture_output=True, check=True, timeout=30).stdout
                if hashlib.sha256(source).hexdigest() != pin['sha256']:
                    raise ValueError('immutable reference hash mismatch: ' + name)
                path = pathlib.Path(directory) / pathlib.Path(pin['path']).name
                path.write_bytes(source)
                if name == 'specification': continue
                module = types.ModuleType('pinned_creature_turn_' + name)
                module.__file__ = str(path)
                sys.modules[module.__name__] = module
                exec(compile(source, str(path), 'exec'), module.__dict__)
                modules[name] = module
                if name == 'core': sys.modules['ref_core'] = module
    finally:
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core', None)
        else: sys.modules['ref_core'] = previous
    return modules



def raw(value):
    # Author fixture values only, never compute a turning expectation here.
    return struct.unpack('<I', struct.pack('<f', value))[0]


def setup(address=C, facing=0, av=0, rate=0x3f800000, slow=0, pos=(0,0,0)):
    return [[address+0x10+4*i,4,v] for i,v in enumerate(pos)] + [
        [address+0x38,2,facing],[address+0x110,2,av],[address+0xb0,4,rate],
        [address+0x2fc,1,slow], [address+0x44,4,0xdeadbeef],
        [address+0xa8,8,0x8000000000100001],[address+0x230,1,0xff],
        [address+0x231,1,8],[address+0x314,2,0x1234],
        [G_SIM_FRAME,4,0xffffffff],[G_WORLD,4,0x55667788],
        [G_RAND_SEED,4,0x12345678]]


def turn(point=(0x3f800000,0,0), mode=0, address=C):
    return [0,address,mode,list(point)]


def recipes():
    out=[]
    def add(label,pokes,ops):out.append((label,[pokes,ops]))
    for mode in (0,1,2,-1,2147483647,-2147483648):
        add('modes',setup(),[turn((0,raw(1),raw(9)),mode)])
    for x,y in ((1,0),(0,1),(-1,0),(0,-1),(1,1),(-1,1),(-1,-1),(1,-1),(0,0)):
        add('directions',setup(),[turn((raw(x),raw(y),0))])
    for f in (0,1,0x3fff,0x4000,0x7fff,0x8000,0x8001,0xbfff,0xffff):
        add('facing_wrap',setup(facing=f),[turn(),turn(mode=1),turn()])
    for av in (0,1,0x3fff,0x7fff,0x8000,0x8001,0xfffe,0xffff):
        add('velocity_wrap',setup(facing=0x7fff,av=av,rate=raw(100)),[turn((0,raw(-1),0)),turn()])
    for rate in (0,0x80000000,raw(1),raw(-1),raw(0.001),raw(-0.001),raw(60),raw(-60),
                 1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,
                 0x7f800000,0xff800000,0x7fc00001,0xffa12345):
        add('rates',setup(rate=rate,av=0xffff),[turn((0,raw(1),0)),turn((raw(-1),0,0))])
    for slow in (0,1,30,59,60,61,127,128,255):
        add('slow',setup(slow=slow),[turn((0,raw(1),0)) for _ in range(3)])
    bits=(0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x3f000001,
          0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00001,0xffa12345)
    for i,value in enumerate(bits):
        add('point_bits',setup(),[turn((value,bits[(i+3)%len(bits)],bits[-i-1])),
                                  turn((bits[(i+3)%len(bits)],value,0xffffffff),1)])
    for value in bits:
        add('position_bits',setup(pos=(value,0x80000000,0x7fc01234)),
            [turn((raw(1),raw(-1),0)),turn((value,value,0))])
    for mode in (0,1,2):
        add('repeated',setup(facing=0xffff),[turn((raw(-5),raw(3),raw(7)),mode) for _ in range(12)])
    for slow in (0,60,255):
        add('interleaved',setup(),[turn((0,raw(1),0)),[1,C+0x2fc,1,slow],
            [2,C+0x110,2,0x8001],turn((raw(-1),0,0),1),[1,C+0xb0,4,raw(-1)],turn()])
    for address in (0,0x10001,0xfffff000):
        add('addresses',setup(address=address),[turn((raw(1),raw(2),0),address=address)])
    add('sparse',[],[turn(),turn((0,raw(1),0),1)])
    add('storage',setup(),[[2,0x70000,8,0xffffffffffffffff],[1,0x70001,2,0],
        [2,0xffffffff,0,0],[1,0xffffffff,0,0],turn(),[2,C+0x38,2,0x18000],turn()])
    add('two_addresses',setup()+setup(address=0x20000,facing=0x8000,slow=60),[
        turn((0,raw(1),0)),turn((0,raw(1),0),address=0x20000),turn(),turn(address=0x20000)])
    add('overlap',setup()+[[C+0x37,8,0xfedcba9876543210],[C+0x10f,4,0x98765432]],[
        turn(),[2,C+0x110,1,255],turn((0,raw(1),0))])
    return out


def cases():
    for config in CONFIGURATIONS:
        for label,inputs in recipes():yield label,config,inputs


def validate_inputs(inputs):
    """Reject invalid authored transport before invoking the unbounded model."""
    if not isinstance(inputs,list) or len(inputs)!=2:raise ValueError('input shape')
    initial,operations=inputs
    if len(initial)>512 or len(operations)>128:raise ValueError('input count')
    def write(row):
        if len(row)!=3 or any(type(v) is not int for v in row):raise ValueError('write shape')
        address,size,value=row
        if not 0<=address<=0xffffffff or not 0<=size<=8 or address+size>0x100000000:
            raise ValueError('write span')
        if not 0<=value<=0xffffffffffffffff:raise ValueError('write value')
    for row in initial:write(row)
    for row in operations:
        if not row or row[0] not in (0,1,2):raise ValueError('opcode')
        if row[0]==0:
            if len(row)!=4:raise ValueError('turn shape')
            _,address,mode,point=row
            if type(address) is not int or not 0<=address<=0xffffffff-0x2fc:
                raise ValueError('turn span')
            if type(mode) is not int or not -0x80000000<=mode<=0x7fffffff:
                raise ValueError('turn mode')
            if len(point)!=3 or any(type(v) is not int or not 0<=v<=0xffffffff for v in point):
                raise ValueError('point words')
        else:write(row[1:])


def run_case(modules,config,inputs):
    validate_inputs(inputs)
    core,reference=modules['core'],modules['creature']
    core.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
    memory=reference.Memory();events=[]
    class Traced(reference.Model):
        def _w(self,address,size,value):
            events.append(['write',address,size,value&((1<<(8*size))-1)])
            return super()._w(address,size,value)
        def call(self,name,*args):
            events.append(['call',name,[typed(a) for a in args]])
            return super().call(name,*args)
    model=Traced(memory)
    def snapshot():
        return copy.deepcopy([sorted(memory.bytes.items()),memory.log,model.writes,
            [[name,[typed(a) for a in args]] for name,args in model.calls],events,
            model.draws,sorted(model.cb),sorted(model.external),0,True,False])
    for address,size,value in inputs[0]:memory.poke(address,size,value)
    initial=snapshot();steps=[]
    for command in inputs[1]:
        result=None;status='ok'
        try:
            if command[0]==0:
                _,address,mode,point=command
                result=model.turn_toward(address,mode,tuple(point))
            elif command[0]==1:memory.poke(*command[1:])
            elif command[0]==2:memory.store(*command[1:])
            else:raise AssertionError('unknown authored command')
        except (ValueError,IndexError,OverflowError) as error:
            status=type(error).__name__
        steps.append([status,result,snapshot()])
    return [copy.deepcopy(inputs),initial,steps]


def independent_anchors(modules):
    def execute(pokes,ops):return run_case(modules,0,[pokes,ops])
    def load(state,address,size):
        data=dict(state[0]);return sum(data.get(address+i,0)<<(8*i) for i in range(size))
    straight=execute(setup(),[turn()])
    assert straight[2][0][1]==0
    assert straight[2][0][2][2]==[(C+0x110,2,0),(C+0x38,2,0)]
    quarter=execute(setup(rate=0),[turn((0,raw(1),0))])
    assert quarter[2][0][1]==16383  # Published raw angle scale truncates below a mathematical quarter turn.
    assert len(quarter[2][0][2][2])==3
    assert quarter[2][0][2][2][-2:]==[(C+0x110,2,0),(C+0x38,2,0)]
    opposite=execute(setup(rate=0),[turn(mode=1)])
    assert opposite[2][0][1]==-32768
    slow=execute(setup(slow=60),[turn((0,raw(1),0))])
    assert load(slow[2][0][2],C+0x38,2)==0
    assert load(slow[2][0][2],C+0x110,2)>0
    # Slow 61 and 255 take the same arithmetic path as zero; ignore their distinct backing byte.
    normal=execute(setup(),[turn((0,raw(1),0))])[2][0][2]
    for v in (61,255):
        other=execute(setup(slow=v),[turn((0,raw(1),0))])[2][0][2]
        assert other[2]==normal[2]
    assert execute(setup(),[turn((raw(1),0,0))])[2] == execute(setup(),[turn((raw(1),0,0x7fc00001))])[2]
    for state in (straight[2][0][2],normal,slow[2][0][2]):
        assert load(state,G_RAND_SEED,4)==0x12345678 and load(state,C+0x44,4)==0xdeadbeef
        assert load(state,G_SIM_FRAME,4)==0xffffffff and load(state,G_WORLD,4)==0x55667788
        assert state[3]==[] and state[5]==0 and state[6:]==[[],[],0,True,False]


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'creature-turn-generator'
    modules = load_references(research_dir, scratch_dir)
    independent_anchors(modules)
    output = pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    manifest_path=output.with_name(output.name[:-7]+'.json')
    fd,name=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent)
    temp=pathlib.Path(name);manifest_temp=None
    vh,th=hashlib.sha256(),hashlib.sha256()
    fnv=14695981039346656037;counts=collections.Counter();number=size=longest=0
    try:
        with os.fdopen(fd,'wb') as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
                header=b'# creature_turn_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for label,config,inputs in cases():
                    first=sc(run_case(modules,config,inputs))
                    if first!=sc(run_case(modules,config,inputs)):
                        raise AssertionError('fresh reference executions diverged')
                    row=('\t'.join([label,str(config),str(number)+';'+sc(inputs),'ok',first])+'\n').encode('ascii')
                    stream.write(row);vh.update(row);th.update(row)
                    for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[label]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'creature-turn','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'helpers':{},'input_protocol':INPUT_PROTOCOL,
          'composition_contract':{
            'scope':'Pinned original Creature_turnToward and slow_factor only; standalone borrowed sparse Memory, no Session or presentation integration.',
            'inputs':'Synthetic explicit initial pokes and turn/poke/store commands; native replay executes the submitted DSL rather than recipes or expectations.',
            'memory':'Every sparse byte including explicit zero, all attempted memory stores, model writes, calls, interleaved events and draw count. All steps retain full snapshots.',
            'rng':'Object seed and unrelated world/frame/shared seed words retained as supplied backing. Turning invokes neither callbacks nor RNG.',
            'errors':'Finite original exceptions retain previous stores. Invalid address, callback, memory and history bounds are separate native policy tests.',
            'references':'Only immutable git-show source with SHA256 verification; no helper imports or native outputs.',
            'publication':'Sibling temporary files flushed and fsynced, fixture replaced then manifest last; interrupted pairs detectable by hashes, not a two-file transaction.'}}
        fd2,name=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent);manifest_temp=pathlib.Path(name)
        with os.fdopen(fd2,'w',encoding='ascii',newline='\n') as stream:
            json.dump(manifest,stream,indent=2,sort_keys=True);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.replace(temp,output);os.replace(manifest_temp,manifest_path)
        return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/creature_turn.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
