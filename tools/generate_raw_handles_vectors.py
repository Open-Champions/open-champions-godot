#!/usr/bin/env python3
"""Independent shared-World raw-readonly handle composition oracle.

Only supplied memory/callback recipes are authored here. HP arithmetic, pointer
selection, write order and callback dispatch execute the pinned Python method.
"""
import argparse
import collections
import gzip
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import types

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'objects': {'commit': '0dd02ce416389dd440540119727d946636507703', 'path': 'tools/ref_objects.py', 'sha256': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'},
    'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'tools/ref_creature.py', 'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'docs/spec/creature-tick.md', 'sha256': 'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
COMPOSITION_POLICY = 'rta-raw-handles-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')

def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, bytes): return 'b' + value.hex()
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned creature state')


def load_references(research_dir, scratch_dir):
    """Never import a live research working-tree file or follow its module path."""
    scratch_dir = pathlib.Path(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)
    modules = {}
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    try:
        with tempfile.TemporaryDirectory(prefix='pinned-', dir=scratch_dir) as directory:
            for name in ('core', 'objects', 'creature', 'specification'):
                pin = SPECIFICATION_EVIDENCE if name == 'specification' else REFERENCES[name]
                source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']],
                                        capture_output=True, check=True, timeout=30).stdout
                if hashlib.sha256(source).hexdigest() != pin['sha256']:
                    raise ValueError('immutable reference hash mismatch: ' + name)
                path = pathlib.Path(directory) / pathlib.Path(pin['path']).name
                path.write_bytes(source)
                if name == 'specification': continue
                module = types.ModuleType('pinned_raw_handles_' + name)
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


HELPERS = {'objects_snapshot': {'path':'tools/generate_objects_vectors.py','sha256':'39d6b7385eeeba2a6a431aae8dfeb573d94c7c801a16bca45672f90b17f6b6be'}}
SCENARIOS = ('pool_identity','partner_destroy','remove_side','front_clear','add_insert',
             'tag_reuse','padding','dirty_init','last_free','empty_free','hp_link',
             'hp_destroy_live','source_alias','null_source','opaque','unknown_head',
             'reuse','aliases','hp_failure','nested_destroy','poke_refuse','store_refuse',
             'mixed_load','detach')
FPU_SCENARIOS = {'hp_link','hp_destroy_live','source_alias','null_source','unknown_head','hp_failure','nested_destroy'}
NODE_BASE, NODE_COUNT, NODE_SIZE, FREE_HEAD = 0x847268, 6096, 20, 0x864ea8
H, H2, DRAIN = 0x91000000, 0x91000010, 0x91000020


class OverlayError(Exception): pass
class Opaque:
    def __init__(self, raw): self.raw=raw


def load_helper():
    import importlib.util
    root=pathlib.Path(__file__).resolve().parents[1]
    for descriptor in HELPERS.values():
        path,sha=descriptor['path'],descriptor['sha256']
        source=(root/path).read_bytes()
        if hashlib.sha256(source).hexdigest()!=sha:raise ValueError('authored helper hash mismatch')
    spec=importlib.util.spec_from_file_location('raw_handles_object_helper',root/'tools/generate_objects_vectors.py')
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    return helper


def make_oracle(modules,helper,config,variant):
    core,objects,creature=(modules[n] for n in ('core','objects','creature'))
    class Oracle(helper.Oracle):
        def __init__(self):
            super().__init__(core,objects,config,variant)
            self.registry={};self.binding_history=[]  # raw address -> (Cell, label)
            self.attached=True
            self.actions=[];self.reads=[];self.pokes=[];self.hp_events=[]
            outer=self
            class Memory(creature.Memory):
                def load(mem,address,size):
                    # The original scalar memory remains the fallback. Overlay
                    # reads are authored address observations, never graph writes.
                    return sum((outer.overlay_byte(address+i) if outer.covered(address+i)
                                else super(Memory,mem).load(address+i,1)) << (8*i) for i in range(size))
                def poke(mem,address,size,value):
                    if any(outer.covered(address+i) for i in range(size)):raise OverlayError('readonly')
                    return super().poke(address,size,value)
                def store(mem,address,size,value):
                    outer.hp_events.append(['store',address,size,value&((1<<(8*size))-1)])
                    return super().store(address,size,value)
            self.mem=Memory();self.model=creature.Model(self.mem)
            self.model.cb={'handle_clear':self.hp_clear,'handle_link':self.hp_link}
            self.a=self.w.new_object(0x400,self.pos(1+variant,2,0))
            self.b=self.w.new_object(0x400,self.pos(3,4+variant,0))
            self.bind_object(self.a);self.bind_object(self.b)
            self.intern(H,'head');self.intern(H2,'head2');self.intern(DRAIN,'drain')
            # Retain ordinary padding/backing, including explicit zero bytes.
            for i in range(8):
                self.mem.poke(NODE_BASE+20*i+18,1,0x80+i)
                self.mem.poke(NODE_BASE+20*i+19,1,variant)
            for obj in (self.a,self.b):
                for off,size,value in ((0xc,4,32),(0x10,4,obj.pos[0]),(0x14,4,obj.pos[1]),
                                       (0xa8,8,0),(0xc8,4,0),(0x228,4,0x42c80000),
                                       (0x231,1,0),(0x2dc,4,0),(0x2e0,2,0),
                                       (0x304,4,0),(0x3b0,2,0),(0x4c,4,0),(0x360,4,0)):
                    self.mem.poke(obj.addr+off,size,value)
            self.intern(self.a.addr+0x2e4,'last')
            self.mem.poke(0x44fdcc,4,32+variant)
            self.mode='';self.actions.clear()

        def oid(self,obj):
            if isinstance(obj,Opaque):return -(obj.raw+1)
            return super().oid(obj)

        def ptr(self,link):
            if link is None:return 0
            if isinstance(link,core.Node):return NODE_BASE+20*link.index
            if isinstance(link,core.Cell):
                for raw,(cell,_) in self.registry.items():
                    if cell is link:return raw
                if link.name is None:raise OverlayError('unknown link')
                return link.name
            raise OverlayError('unaddressed iterator')

        def raw_obj(self,obj):
            if obj is None:return 0
            return obj.raw if isinstance(obj,Opaque) else obj.addr

        def covered(self,address):
            if not self.attached:return False
            if NODE_BASE<=address<FREE_HEAD:return (address-NODE_BASE)%20<18
            if FREE_HEAD<=address<FREE_HEAD+4:return True
            return any(a<=address<a+4 for a in self.registry)

        def overlay_byte(self,address):
            if NODE_BASE<=address<FREE_HEAD:
                node=self.w.handles.nodes[(address-NODE_BASE)//20]
                offset=(address-NODE_BASE)%20
                if not 0<=node.tag<=65535:raise OverlayError('tag width')
                if offset<16:
                    value=(self.ptr(node.next),self.ptr(node.prev),self.ptr(node.partner),self.raw_obj(node.obj))[offset//4]
                    return (value>>(8*(offset%4)))&255
                if not 0<=node.tag<=65535:raise OverlayError('tag width')
                return (node.tag>>(8*(offset-16)))&255
            if FREE_HEAD<=address<FREE_HEAD+4:return (self.ptr(self.w.handles.free)>>(8*(address-FREE_HEAD)))&255
            for raw,(cell,_) in self.registry.items():
                if raw<=address<raw+4:return (self.ptr(cell.next)>>(8*(address-raw)))&255
            raise AssertionError('not covered')

        def intern(self,raw,label):
            if raw in self.registry:raise OverlayError('already bound')
            cell=core.Cell(0x90000000+4*len(self.extra));self.extra.append(cell)
            self.bind(raw,cell,label);return cell

        def bind(self,raw,cell,label):
            if raw in self.registry:
                if self.registry[raw]!=(cell,label):raise OverlayError('conflicting alias')
                return
            if any(known is cell for known,_ in self.registry.values()):raise OverlayError('multiple physical addresses')
            if any(raw<a+4 and a<raw+4 for a in self.registry):raise OverlayError('overlap')
            self.registry[raw]=(cell,label)
            serial=next((self.oid(obj) for obj in self.retained if any(h is cell for h in obj.heads)),0)
            self.binding_history.append([raw,cell,serial,label,True])

        def bind_object(self,obj):
            for i,cell in enumerate(obj.heads):self.bind(obj.addr+4*i,cell,'object+'+str(4*i))

        def rebind_object(self,obj):
            for i in range(3):
                raw=obj.addr+4*i
                for entry in self.binding_history:
                    if entry[0]==raw and entry[4]:entry[4]=False
                self.registry.pop(raw,None)
            self.bind_object(obj)

        def head(self,raw):
            if raw not in self.registry:raise OverlayError('unknown head')
            return self.registry[raw][0]

        def link(self,head,back,obj):
            return self.w.handles.link(self.head(head),self.head(back),Opaque(obj) if obj else None)

        def link_known(self,head,obj):
            return self.w.handles.link(self.head(head),obj.heads[0],obj)

        def clear(self,head):return self.w.handles.clear(self.head(head))

        def read(self,label,address,size=4):
            value=self.mem.load(address,size);self.reads.append([label,address,size,value]);return value

        def poke(self,address,size,value):
            self.pokes.append([address,size,value]);return self.mem.poke(address,size,value)

        def hp_clear(self,head):
            self.hp_events.append(['call','handle_clear',[head]])
            self.clear(head)
            if self.mode in ('destroy_read','destroy_throw'):
                self.w.destroy(self.b,3)
                self.read('inside_destroy',H)
                self.read('inside_free',FREE_HEAD)
                if self.mode=='destroy_throw':raise CallbackFailure()

        def hp_link(self,head,back,obj):
            self.hp_events.append(['call','handle_link',[head,back,obj]])
            self.link(head,back,obj)

        def invoke(self,label,function):
            try:
                value=function()
                if isinstance(value,core.Node):value=self.ptr(value)
                elif isinstance(value,Opaque):value=['raw',value.raw]
                elif isinstance(value,objects.GameObject):value=['known',self.oid(value)]
                self.results.append([label,'ok',value])
            except (OverlayError,core.HandleError,CallbackFailure) as error:
                self.results.append([label,type(error).__name__,None])

        def drain(self,left):
            for _ in range(self.w.handles.count_free()-left):self.w.handles.add(self.head(DRAIN),None)

        def snapshot(self):
            world=super().snapshot()
            for row in world[5]:
                raw=-row[5]-1 if row[5]<0 else 0
                if raw:row[5]=0
                row.append(raw)
            semantic={}
            for obj in self.retained:
                for cell in obj.heads:semantic[id(cell)]=(self.oid(obj)<<32)|cell.name
            for cell in self.extra:semantic[id(cell)]=cell.name
            registry=[[raw,semantic[id(cell)],serial,label,active] for raw,cell,serial,label,active in self.binding_history]
            # Complete physical pool fields via reconstructible zero/default
            # suppression; backing/padding stay in their own byte map.
            raw_nodes=[]
            for node in self.w.handles.nodes:
                row=[self.ptr(node),self.ptr(node.next),self.ptr(node.prev),self.ptr(node.partner),self.raw_obj(node.obj),node.tag,0 if isinstance(node.obj,Opaque) else self.oid(node.obj),isinstance(node.obj,Opaque)]
                default=[row[0],row[0]+20 if node.index+1<NODE_COUNT else 0,0,0,0,0,0,False]
                if row!=default:raw_nodes.append(row)
            return [self.results,world,registry,self.attached,self.ptr(self.w.handles.free),raw_nodes,
                    [[a,self.ptr(c.next)] for a,(c,_) in sorted(self.registry.items())],
                    sorted(self.mem.bytes.items()),self.model.writes,self.mem.log,
                    [[n,list(a)] for n,a in self.model.calls],self.hp_events,self.pokes,self.reads,self.model.draws]
    return Oracle()


def run_case(modules,helper,config,variant,scenario):
    o=make_oracle(modules,helper,config,variant)
    w,p=o.w,o.w.handles;a,b=o.a,o.b
    def hp():return o.model.take_damage(a.addr,b.addr,0x40,0x40800000)
    try:
        if scenario=='pool_identity':
            o.invoke('link',lambda:o.link_known(H,b));o.read('head',H);o.read('free',FREE_HEAD)
        elif scenario=='partner_destroy':
            o.link_known(H,b);o.link(H2,b.addr,b.addr)
            o.invoke('destroy',lambda:w.destroy(b,3));o.read('first',H);o.read('second',H2)
        elif scenario=='remove_side':
            n=o.link_known(H,b)
            o.invoke('remove',lambda:p.remove(n.partner if variant else n));o.read('free',FREE_HEAD)
        elif scenario=='front_clear':
            o.link_known(H,a);o.link_known(H,b);o.invoke('clear',lambda:o.clear(H));o.read('head',H)
        elif scenario=='add_insert':
            first=p.add(o.head(H),a)
            second=p.insert(o.head(H),first,b)
            o.invoke('remove',lambda:p.remove(second));o.invoke('clear',lambda:o.clear(H))
        elif scenario=='tag_reuse':
            n=o.link_known(H,b);n.tag=0x1234+variant;n.partner.tag=0xab00+variant
            p.remove(n);o.invoke('link',lambda:o.link(H,a.addr,0xdead0000+variant))
            o.read('tag',o.ptr(n)+16,4)
        elif scenario=='padding':
            n=o.link_known(H,b);p.remove(n);o.link_known(H,a)
            for i in range(8):o.read('pad'+str(i),NODE_BASE+i*20+16,4)
        elif scenario=='dirty_init':
            n=o.link_known(H,b);n.tag=17+variant;n.partner.tag=33
            o.invoke('init',p.init);o.read('stale',o.ptr(n)+8,8)
        elif scenario in ('last_free','empty_free'):
            o.drain(1 if scenario=='last_free' else 0)
            if p.free:p.free.tag=0xbeef
            o.invoke('exhaust',lambda:o.link_known(H,b));o.read('free',FREE_HEAD)
        elif scenario=='hp_link':
            o.invoke('hp',hp);o.read('last',a.addr+0x2e4)
        elif scenario=='hp_destroy_live':
            o.link_known(H,b);o.mode='destroy_read';o.invoke('hp',hp)
        elif scenario=='source_alias':
            # Distinct owner/source heads share one target and its real back-head.
            o.intern(a.addr+0x4c,'owner');o.intern(a.addr+0x360,'effect')
            o.link_known(a.addr+0x4c,b);o.link_known(a.addr+0x360,b)
            o.mem.poke(a.addr+0xc,4,0xa0)
            o.invoke('hp',lambda:o.model.take_damage(a.addr,a.addr,0x10000000,0x40000000))
            o.invoke('destroy',lambda:w.destroy(b,3));o.read('owner',a.addr+0x4c);o.read('source',a.addr+0x360);o.read('last',a.addr+0x2e4)
        elif scenario=='null_source':
            o.mem.poke(b.addr+0xc,4,0xa0)
            if variant:
                head=o.intern(b.addr+0x4c,'owner');p.add(head,None)
            o.invoke('hp',hp)
        elif scenario=='opaque':
            o.invoke('link',lambda:o.link(H,b.addr,0xdead0000+variant))
            o.read('object',o.read('head',H)+12)
            o.invoke('clear',lambda:o.clear(H))
        elif scenario=='unknown_head':
            head=o.intern(b.addr+0x4c,'owner');p.add(head,Opaque(0xdead0000+variant))
            o.mem.poke(b.addr+0xc,4,0xa0)
            o.invoke('hp',hp)
        elif scenario=='reuse':
            o.link_known(H,b);w.destroy(b,3)
            replacement=w.new_object(0x400,o.pos(7,8,0))
            o.results.append(['reused','ok',[replacement.addr,o.oid(b),o.oid(replacement)]])
            o.invoke('bind_conflict',lambda:o.bind_object(replacement))
            if variant:o.invoke('rebind',lambda:o.rebind_object(replacement))
            o.read('last_free_object',o.ptr(p.free)+12)
        elif scenario=='aliases':
            o.invoke('same',lambda:o.bind(H,o.head(H),'head'))
            o.invoke('label',lambda:o.bind(H,o.head(H),'alias'))
            o.invoke('conflict',lambda:o.bind(H,o.head(H2),'other'))
            o.link_known(H,b);o.read('alias',H)
        elif scenario=='hp_failure':
            o.drain(1)
            o.invoke('hp',hp);o.read('free',FREE_HEAD);o.read('hp',a.addr+0x228)
        elif scenario=='nested_destroy':
            o.link_known(H,b);o.intern(a.addr+0x4c,'owner');o.link_known(a.addr+0x4c,b)
            o.mem.poke(a.addr+0xc,4,0xa0)
            o.mode='destroy_throw' if variant else 'destroy_read'
            o.invoke('hp',lambda:o.model.take_damage(a.addr,a.addr,0x40,0x40000000))
        elif scenario=='poke_refuse':
            o.invoke('head',lambda:o.poke(H,4,0))
            o.invoke('mixed',lambda:o.poke(NODE_BASE+17,3,0xabcdef))
            o.invoke('padding',lambda:o.poke(NODE_BASE+18,2,0x1234+variant))
        elif scenario=='store_refuse':
            o.invoke('store',lambda:o.mem.store(H,4,0x12345678))
            o.invoke('model_write',lambda:o.model.take_damage(H-0x3b0,0,0,0))
            o.invoke('padding',lambda:o.mem.store(NODE_BASE+18,2,0xabcd))
        elif scenario=='mixed_load':
            n=o.link_known(H,b);n.tag=0x1200+variant
            o.read('mixed',o.ptr(n)+16,8)
            o.invoke('pad',lambda:o.poke(o.ptr(n)+18,2,0xabcd));o.read('after_pad',o.ptr(n)+16,8)
            p.remove(n);o.read('after_remove',o.ptr(n)+16,8)
            o.read('absent_padding',NODE_BASE+20*20+18,2)
        elif scenario=='detach':
            o.link_known(H,b);o.read('attached',H)
            o.attached=False;o.read('detached',H)
            o.invoke('ordinary',lambda:o.mem.store(H,4,0xabcdef01));o.read('ordinary',H)
        else:raise ValueError('unknown scenario')
        return o.snapshot()
    finally:o.close()


def validate_anchors(modules,helper):
    def case(name,variant=0):return run_case(modules,helper,0,variant,name)
    first=case('pool_identity')
    assert first[0][0][2]==NODE_BASE+4*20 # two original World grid pairs first
    failed=case('last_free')
    assert failed[0]==[['exhaust','HandleError',None]] and failed[4]==0
    assert next((row[5] for row in failed[5] if row[0]==NODE_BASE+20*(NODE_COUNT-1)),0)==0
    assert dict(failed[6])[H]==0
    live=case('hp_destroy_live')
    assert next(r[3] for r in live[13] if r[0]=='inside_destroy')==0
    sources=case('source_alias')
    assert all(r[3]==0 for r in sources[13] if r[0] in ('owner','source','last'))
    null=case('null_source',1)
    assert [n for n,args in null[10]]==['handle_clear']
    opaque=case('opaque')
    assert any(row[4]==0xdead0000 and row[7] for row in opaque[5])
    rejected=case('store_refuse')
    assert rejected[8]==[(H,2,0x7fff)]
    assert rejected[9][:2]==[(H,4,0x12345678),(H,2,0x7fff)]
    assert rejected[11][:2]==[['store',H,4,0x12345678],['store',H,2,0x7fff]]


def generate(research_dir,output,scratch_dir=None):
    if scratch_dir is None:scratch_dir=pathlib.Path(__file__).resolve().parents[1]/'work'/'raw-handles-generator'
    modules=load_references(research_dir,scratch_dir);helper=load_helper()
    validate_anchors(modules,helper)
    output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    vector_hash,trace_hash=hashlib.sha256(),hashlib.sha256();fnv=14695981039346656037
    counts=collections.Counter();number=0
    with output.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
        header=b'# raw_handles_vectors_v1\n';stream.write(header);vector_hash.update(header)
        for config in range(16):
            for variant in range(2):
                for scenario in SCENARIOS:
                    if config and scenario not in FPU_SCENARIOS:continue
                    first=sc(run_case(modules,helper,config,variant,scenario))
                    second=sc(run_case(modules,helper,config,variant,scenario))
                    if first!=second:raise AssertionError('fresh composed executions differ')
                    row=('\t'.join((scenario,str(config),f'{number};{variant}','ok',first))+'\n').encode('ascii')
                    stream.write(row);vector_hash.update(row);trace_hash.update(row)
                    for b in row:fnv=((fnv^b)*1099511628211)&0xffffffffffffffff
                    counts[scenario]+=1;number+=1
    manifest={
        'schema_version':1,'suite':'raw-handles','composition_policy':COMPOSITION_POLICY,
        'reference_sha256':REFERENCE_SHA256,'references':REFERENCES,
        'specification_evidence':SPECIFICATION_EVIDENCE,'helpers':HELPERS,
        'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'vector_count':number,'vector_sha256':vector_hash.hexdigest(),
        'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
        'expected_trace_sha256':trace_hash.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
        'operations':dict(sorted(counts.items())),
        'recipe_configurations':{name:list(range(16)) if name in FPU_SCENARIOS else [0] for name in SCENARIOS},
        'replicas_per_case':2,
        'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
        'trace_columns':['operation','configuration','id;variant','status','complete_composed_state'],
        'composition_contract':{
            'references':'Immutable SHA-verified Git objects only; one shared verified ref_core module imported by ref_objects and ref_creature. No C++ outputs feed expectations.',
            'helper':'SHA-verified authored objects generator records stable allocation/iterator identities and complete World snapshots. Its constructor wrappers call original constructors once; handle/World/HP method algorithms are not replaced.',
            'pool':'Exactly the original World.handles, all6096nodes, including construction gridpairs. Exhaustion consumes the existingpool using originaladd; no replacementpool/free-list shortcut.',
            'memory':'Original Memory backing and log with an optional authored READONLY read-through overlay on registeredheads, nodefields0..17 andfreehead. Eachload observes current originalgraph immediately. No graphimport/refresh/modelmirror writes. Padding18..19 remains normalbacking.',
            'identity':'Physical raw32 addresses remain separate from semanticWorldallocation IDs. Opaque nonzero targets remain tagged values includingfreednodes; knownreferences retainserial. No fakeGameObjects or opaque actorbehavior.',
            'snapshot':'CompleteWorld sparse reconstructible graph with appended opaqueobject field; explicitregistry andphysicalnode/headview; completebackingbytes separately fromoverlay; HPwrites, Memorylog, calls, interleaved Memorystoreattempt/callbackevents, pokes, reads anddraws.',
            'exceptions':'Read-only pokes refusewholewrite; store retains original attemptedlog; HP retains its original writeprefix. Original HandleError leaves allocation/graph partialeffects. WrapperOverlayError is separate supported-domain policy, notoriginalUndefinedBehavior.',
            'configuration':'All16FPUsettings for7HP/arithmetic recipes; purehandle/padding/lifetime recipes use config0 to avoid redundantlargegraph expansion. Everycase repeatsfresh twice.',
            'scope':'RTA bounded sharedhandle backend, explicit registry/currentgraph reads, no Session installation, derivedCreature/death/loot, arbitraryrawcorruption or Norrath claim. Iterator/rawtag/lifetime/registration limits are separate nativehandtests.',
        },
    }
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='ascii',newline='\n')
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/raw_handles.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args();manifest=generate(args.research_dir,args.output,args.scratch_dir)
    print(json.dumps({k:manifest[k] for k in ('vector_count','vector_sha256','expected_trace_sha256','expected_trace_fnv1a64')}))

if __name__=='__main__':main()
