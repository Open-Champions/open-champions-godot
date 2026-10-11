#!/usr/bin/env python3
"""Independent, asset-free Creature_moveBy/water/settle vectors from immutable RTA models.

Only supplied memory/callback recipes are authored here. Movement arithmetic, pointer
selection, write order and callback dispatch execute the pinned Python method.
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

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'tools/ref_creature.py', 'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'docs/spec/creature-tick.md', 'sha256': 'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
COMPOSITION_POLICY = 'rta-creature-movement-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')
SCENARIOS = (
    'basic', 'hidden', 'root_motion', 'root_timer', 'slide', 'slide_no_hit',
    'slide_mutate', 'slide_throw', 'grid_throw', 'contacts_order', 'contacts_filter',
    'contacts_coincident', 'contacts_nopush', 'standing', 'standing_skip',
    'standing_passthrough', 'standing_ignore', 'touch_mutate', 'touch_throw',
    'vtable_adjust', 'base_v6', 'settle', 'settle_target', 'settle_throw',
    'water_enter', 'water_skip', 'water_stay', 'water_exit', 'water_disable',
    'water_mutate', 'water_throw', 'ground_mutate', 'ground_throw', 'rising_snap',
    'repeat', 'raw_bits', 'callback_store', 'virtual_throw', 'radius_reload',
    'missing_handlers', 'water_false', 'water_cadence', 'buffer_token', 'base_empty', 'external_base', 'replace_handler')
VARIANTS = 2
CONFIGURATIONS = tuple(range(16))
C, A, B, T, TA, TB, NODE = 0x10000, 0x20000, 0x21000, 0x30000, 0x31000, 0x32000, 0x40000
G_SIM_FRAME, G_WORLD, G_RAND_SEED = 0x44fdcc, 0x44fdc8, 0x404464


def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, bytes): return 'b' + value.hex()
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned creature state')


def callback_value(value):
    # Preserve Python's tuple/list distinction inside callback data. A native
    # Vec3 represents an original three-word tuple, not a separate Python type.
    # ('buf', bytes) remains a tuple token with its string/bytes leaves intact.
    if isinstance(value, tuple):
        return ['tuple', [callback_value(v) for v in value]]
    if isinstance(value, list):
        return ['list', [callback_value(v) for v in value]]
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
                module = types.ModuleType('pinned_creature_movement_' + name)
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


class CallbackFailure(Exception):
    pass


def run_case(modules, config, variant, scenario):
    core, reference = modules['core'], modules['creature']
    core.set_fpu(MODELS[config // 4], bool(config & 2), bool(config & 1))
    memory = reference.Memory()
    ordered_events, callback_pokes, callback_results = [], [], []

    class TracedModel(reference.Model):
        def _w(self, address, size, value):
            ordered_events.append(['write', address, size, value & ((1 << (8 * size)) - 1)])
            return super()._w(address, size, value)

        def call(self, name, *args):
            canonical = [v & 0xffffffff if isinstance(v, int) else v for v in args]
            ordered_events.append(['call', name, [callback_value(v) for v in canonical]])
            supplied = name in self.cb
            result = super().call(name, *args)
            if supplied: callback_results.append([len(self.calls), callback_value(result)])
            return result

    model = TracedModel(memory)
    p = memory.poke
    def vi(x, y, z): return tuple(core.from_int(v) for v in (x, y, z))
    def pv(address, values):
        for i, value in enumerate(values): p(address + 4*i, 4, value)
    for address, table, x in ((C,T,0),(A,TA,3),(B,TB,-3)):
        p(address+0xc,4,0x20)
        pv(address+0x10,vi(x,0,0))
        p(address+0x1c,4,core.from_int(4))
        p(address+0x20,4,core.from_int(8))
        p(address+0x30,4,0)
        p(address+0x44,4,0xffffffff if variant else 1)
        p(address+0x48,4,table)
        p(address+0xa8,8,0x100000)  # normal cases intentionally bypass water services
        for slot in (6,10,11):
            p(table+8*slot,2,0); p(table+8*slot+4,4,0x900000+slot)
    p(C+0x38,2,0x7fff if variant else 17)
    p(C+0xb8,4,0)
    pv(C+0xbc,(0,0,0))
    p(C+0x230,1,variant)
    p(C+0x231,1,0)
    p(C+0x234,4,0)
    p(C+0x314,2,0)
    p(G_WORLD,4,0x50000);p(G_SIM_FRAME,4,variant)
    p(G_RAND_SEED,4,0x12345678)
    velocity=(0,0,0)
    found=[]
    ground=0
    slide_hit=1
    normal=(0x3f800000,0,0)
    if scenario=='hidden': p(C+0x231,1,8|variant)
    if scenario in ('root_motion','root_timer'):
        pv(C+0xbc,(0x3f000001,0xbf800000,0xffff if variant else 0x4000))
        velocity=vi(1,2,3)
        if scenario=='root_timer':p(C+0x314,2,0xffff if variant else 1)
    if scenario in ('slide','slide_no_hit','slide_mutate','slide_throw','grid_throw','callback_store','virtual_throw','base_empty'):
        velocity=vi(60,-30,7)
    if scenario=='slide_no_hit':slide_hit=0
    if scenario in ('contacts_order','contacts_filter','contacts_coincident','contacts_nopush','standing',
                    'standing_skip','standing_passthrough','standing_ignore','touch_mutate','touch_throw',
                    'vtable_adjust','base_v6','base_empty','external_base','replace_handler','settle','settle_target','settle_throw','radius_reload'):
        found=[A,B]
    if scenario=='contacts_order':found=[B,A,B] if variant else [A,B,A]
    if scenario=='contacts_filter':found=[C,A,B];p(A+0xc,4,1);p(B+0x10,4,core.from_int(100))
    if scenario=='contacts_coincident':pv(A+0x10,vi(0,0,0));found=[A]
    if scenario=='contacts_nopush':p(C+0xc,4,0x401 if variant else 0x400)
    if scenario.startswith('standing'):
        p(A+0xc,4,0x10);p(B+0xc,4,0x10)
        p(A+0x20,4,core.from_int(3));p(B+0x20,4,core.from_int(4))
        if scenario=='standing_skip':p(A+0x18,4,core.from_int(20));p(B+0x18,4,core.from_int(-20))
        if scenario=='standing_passthrough':p(A+0xc,4,0x210);p(B+0xc,4,0x210)
        if scenario=='standing_ignore':p(C+0xc,4,0x400)
    if scenario=='vtable_adjust':
        p(TA+48,2,0xfff0 if variant else 16)
        p(T+88,2,0xffe0 if variant else 32)
    if scenario in ('base_v6','external_base'):
        p(C+0xc,4,0x40 if variant else 0x20)
        p(TA+52,4,0x11a418);p(TB+52,4,0x11a418)
        p(TA+48,2,16 if variant else 0)
    if scenario=='external_base':model.external.add('Creature.v6')
    if scenario=='base_empty':p(T+84,4,0x11a3f0);p(T+92,4,0x11a448)
    if scenario=='settle_target':p(C+0x234,4,NODE);p(NODE+0xc,4,A if variant else B)
    if scenario.startswith('water_') or scenario=='buffer_token':
        p(C+0xa8,8,0);p(G_SIM_FRAME,4,variant);p(C+0x230,1,variant)
        if scenario=='water_skip':p(G_SIM_FRAME,4,3+variant)
        if scenario in ('water_stay','water_exit','water_cadence'):
            p(C+0xa8,8,0x2000000000000000)
            p(C+0xb8,4,0 if variant else 0x3f800000)
        if scenario=='water_cadence':p(C+0xb8,4,0);p(G_SIM_FRAME,4,16 if variant else 1)
        if scenario=='water_disable':p(C+0xa8,8,0x2000000000100000)
    if scenario=='rising_snap':p(C+0x30,4,core.from_int(10));ground=core.from_int(2)
    if scenario=='raw_bits':
        velocity=(0x7fffffff if variant else 1,0x80000000,0xffffffff if variant else 0x00800000)
        p(C+0x30,4,0xff7fffff if variant else 0x80000000)
        ground=0xc7c35000

    def poke(address,size,value):
        value &= (1 << (size*8))-1
        callback_pokes.append([len(model.calls),address,size,value]);memory.poke(address,size,value)
    def pokevec(address,values):
        for i,value in enumerate(values):poke(address+4*i,4,value)
    slides=0
    touches=0
    def slide(address,delta,iterations,primary,radius):
        nonlocal slides
        slides+=1
        pokevec(address,tuple(core.add(memory.load(address+4*i,4),delta[i]) for i in range(3)))
        if scenario=='slide_mutate':
            poke(C+0x1c,4,core.from_int(2+variant));poke(C+0x30,4,core.from_int(15))
        if scenario=='callback_store':memory.store(C+0x30,4,core.from_int(12+variant))
        if scenario=='slide_throw':raise CallbackFailure()
        return (slide_hit,None if scenario=='slide_no_hit' and variant==1 else normal)
    def grid(obj):
        if scenario=='grid_throw':poke(C+0x38,2,0x1234);raise CallbackFailure()
        return None
    def collect(low,high,limit):return list(found)
    def touch(this,other):
        nonlocal touches
        touches+=1
        if scenario=='replace_handler':
            def replacement(*args):
                poke(C+0x38,2,0x9000+variant)
                return None
            model.cb['Creature.v6']=replacement
        if scenario in ('touch_mutate','radius_reload'):
            poke(C+0x1c,4,core.from_int(1 if touches==1 else 6))
            if scenario=='touch_mutate':poke(B+0xc,4,1);poke(C+0x30,4,core.from_int(20))
        if scenario in ('touch_throw','settle_throw'):poke(C+0x18,4,core.from_int(9));raise CallbackFailure()
        return None
    def height(world,position,radius):
        if scenario in ('ground_mutate','ground_throw'):
            poke(C+0x18,4,core.from_int(5+variant));poke(C+0x30,4,core.from_int(10))
        if scenario=='ground_throw':raise CallbackFailure()
        return ground
    def wet(*args):
        if scenario=='water_mutate':poke(C+0xa8,8,0x8000000000000001)
        return 0 if scenario=='water_false' else 1
    def disturb(*args):
        if scenario=='water_throw':poke(C+0xb8,4,0x3f800000);raise CallbackFailure()
        return 0 if scenario=='water_exit' else 1
    def virtual(*args):
        if scenario=='virtual_throw':poke(C+0x38,2,0x55aa);raise CallbackFailure()
        return None
    model.cb.update({'world_slideMove':slide,'obj_updateGridCell':grid,'world_collectObjectsInBox':collect,
        'Creature.v6':touch,'Creature.v10':virtual,'Creature.v11':virtual,'terrain_heightBelow':height,
        'water_test':wet,'water_disturb':disturb,'libc_sprintf':lambda *a:('buf',b'splash\x00\xff'+bytes([variant])) if scenario=='buffer_token' else 0x60000,
        'res_find':lambda *a:0x60010,'sound_playOnObject':lambda *a:None})
    if scenario=='missing_handlers':model.cb.clear()
    results=[]
    for _ in range(3 if scenario=='repeat' else 1):
        try:
            result=model.settle(C) if scenario.startswith('settle') else model.move_by(C,velocity)
            results.append(result)
        except CallbackFailure:results.append('CallbackFailure')
    return [results,sorted(memory.bytes.items()),model.writes,memory.log,[[name,[callback_value(v) for v in args]] for name,args in model.calls],callback_pokes,
            model.draws,ordered_events,callback_results]


def independent_anchors(modules):
    """Literal semantic checks, separate from replica agreement and native output."""
    def load(result, address, size):
        data=dict(result[1])
        return sum(data.get(address+i,0) << (8*i) for i in range(size))
    basic=run_case(modules,0,0,'basic')
    assert basic[0]==[4] and load(basic,C+0x18,4)==0 and load(basic,C+0x30,4)==0
    hidden=run_case(modules,0,0,'hidden')
    assert hidden[0]==[0] and not hidden[2] and not hidden[3] and not hidden[4]
    water=run_case(modules,0,0,'water_enter')
    assert load(water,C+0x44,4)==0x3c88596c and load(water,G_RAND_SEED,4)==0x12345678 and water[6]==0
    assert [c[0] for c in water[4]][:5]==['water_test','libc_sprintf','res_find','sound_playOnObject','water_disturb']
    dry=run_case(modules,0,0,'water_false')
    assert load(dry,C+0x44,4)==1 and load(dry,C+0xa8,8)==0
    skipped=run_case(modules,0,0,'water_cadence')
    cadence=run_case(modules,0,1,'water_cadence')
    assert all(c[0]!='water_disturb' for c in skipped[4]) and cadence[4][0][0]=='water_disturb'
    base=run_case(modules,0,0,'base_empty')
    assert base[0]==[7] and not any(c[0] in ('Creature.v10','Creature.v11') for c in base[4])
    forced=run_case(modules,0,1,'external_base')
    assert sum(c[0]=='Creature.v6' for c in forced[4])==2
    buffer=run_case(modules,0,1,'buffer_token')
    assert buffer[4][2][1][1]==['tuple',['buf',b'splash\x00\xff\x01']]
    assert callback_value([])!=callback_value(())
    assert callback_value((1,2,3))==['tuple',[1,2,3]]
    failure=run_case(modules,0,0,'settle_throw')
    assert failure[0]==['CallbackFailure'] and load(failure,C+0xc,4)&0x8000000
    assert load(failure,C+0x18,4)==0x41100000
    replaced=run_case(modules,0,0,'replace_handler')
    assert load(replaced,C+0x38,2)==0x9000


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'creature-movement-generator'
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
                header=b'# creature_movement_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for config in CONFIGURATIONS:
                    for variant in range(VARIANTS):
                        for scenario in SCENARIOS:
                            first=sc(run_case(modules,config,variant,scenario))
                            if first!=sc(run_case(modules,config,variant,scenario)):
                                raise AssertionError('fresh reference executions diverged')
                            row=('\t'.join([scenario,str(config),f'{number};{variant}','ok',first])+'\n').encode('ascii')
                            stream.write(row);vh.update(row);th.update(row)
                            for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                            counts[scenario]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'creature-movement','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'composition_contract':{'scope':'Original pinned Creature_moveBy, water bookkeeping and settle; explicit authored terrain/contact/virtual/resource/sound callbacks. No original service implementation or Session/player/controller binding.',
          'memory':'All sparse backing bytes including explicit zeros; complete store/write/call histories and interleaved model events. Callback pokes and successful callback results retain call ordinal. Callback lists and tuples carry explicit shape tags; native Vec3 maps to the original tuple; buffer tokens retain the original tuple/string/bytes structure.',
          'rng':'Water entry executes original object generator. Shared seed remains an independently observed raw memory word; Model.draws retains shared draw count.',
          'errors':'Authored callback failures retain all prior writes/calls; unsupported address/work/shape bounds are independent native-only tests.',
          'references':'Only git-show immutable source with SHA256 verification; no helper imports or native outputs.',
          'publication':'Sibling temporary files flushed and fsynced, fixture replaced then manifest last. Interrupted pair publication is detectable by hashes; not a two-file transaction.'}}
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
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/creature_movement.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
