#!/usr/bin/env python3
"""Authored, asset-free composition of three immutable RTA Python model objects.

The composition (host task order, failure latch, budgets and replay inputs) is a
new test policy, not a recovered engine loop or game-native implementation.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import types

REFERENCES = {'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'}, 'objects': {'commit': '0dd02ce416389dd440540119727d946636507703', 'path': 'tools/ref_objects.py', 'sha256': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'}, 'netwire': {'commit': '54258f9d7e281ef07f0675040e82e6264f9386cf', 'path': 'tools/ref_netwire.py', 'sha256': '4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'}}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(",",":")).encode("ascii")).hexdigest()
COMPOSITION_POLICY = "rta-session-objects-v1"
MODELS=("ps2","ps2g1","chop","nearest")
ORDERING_EVIDENCE = {'commit': '0dd02ce416389dd440540119727d946636507703', 'path': 'docs/mainloop.md', 'sha256': 'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}
def sc(value):
    if value is None:
        return '_'
    if isinstance(value, bool):
        return str(int(value))
    if isinstance(value, (bytes, bytearray)):
        return 'b' + bytes(value).hex()
    if isinstance(value, str):
        return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)):
        return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int):
        return str(value)
    raise TypeError('unowned or noninteger state in session trace')

GLOBAL_FIELDS = 'network_mode num_peers lif last_sent net_frame cycle phase modem last_hash_frame stall ended eq min_ready win long_stall kicked kick_state msg_attached sim_frame rand_seed sync_acc level_pending num_players'.split()
PEER_FIELDS = 'port send_frame msg_write msg_read newest acked lead sim silence heard local dropped kick oos drop_frame kicked_notice'.split()
GLOBALS = 'cursor15 cursor60 frame network_mode cull_by_camera pool_free_count tearing_down lump_generation menu_lockout menu_lockout_hold warmup_frames start_rank exclude_mask'.split()
OBJECT_FIELDS = 'addr flags pos radius height vz angle seed vtable draw_half_width draw_height class_name type_name alive tag'.split()


def codec(c):
    return [[[x.index, x.pred, x.value, x.velocity] for x in c.ch], c.yaw_prev, c.yaw_pred, c.yaw_delta, c.pad46, c.count]


def sent(v):
    return [v.peer, v.ip, v.port, v.data, list(v.undefined)]


def network_snapshot(m, config):
    # Snapshot strips executable callbacks; the live session hook is separately recorded.
    return [[getattr(m, x) for x in GLOBAL_FIELDS],
            [[p.pack(), [getattr(p, x) for x in PEER_FIELDS], p.pad_prev, codec(p.recv_state), codec(p.send_state)] for p in m.peers],
            m.kick_acks, m.cam_yaw, m.input_lock, m.send_cycles, codec(m.sim_enc), codec(m.sim_dec), m.positions, m.pads,
            [m.pad_globals.start_block, m.pad_globals.mute, m.pad_globals.allow], [sent(x) for x in m.outbox], m.log,
            [], [0]*4, 0, 0, 0, 0, 0, [config//4, bool(config&2), bool(config&1)]]


class CallbackFailure(Exception):
    pass


class WorldOracle:
    def __init__(self, core, objects, config, variant):
        self.r, self.o, self.variant = core, objects, variant
        core.set_fpu(MODELS[config//4], bool(config&2), bool(config&1))
        self.retained, self.iterators, self.extra = [], [], []
        self.live_iterators, self.completed_iterators = {}, set()
        self.original_object, self.original_iterator = objects.GameObject, core.Iterator
        outer = self
        class RecordedObject(self.original_object):
            def __init__(self, addr):
                super().__init__(addr)
                outer.retained.append(self)
        class RecordedIterator(self.original_iterator):
            def __init__(self, head, mask=0):
                super().__init__(head, mask)
                outer.iterators.append(self)
                outer.live_iterators[len(outer.iterators)] = self
            def end(self):
                super().end()
                outer.completed_iterators.add(outer.iterators.index(self) + 1)
                outer.prune_iterators()
        objects.GameObject, core.Iterator = RecordedObject, RecordedIterator
        self.w = objects.World()
        self.w.shared.state = (0, 1, 0xdeadbeef, 0xffffffff)[variant]
        self.events, self.results = [], []

    def prune_iterators(self):
        # Completed sentinels have no runtime lifetime unless a stale retained
        # link still references them. This authored ownership policy preserves
        # Python's reachable graph without retaining every empty walk forever.
        referenced = set()
        for link in self.w.handles.nodes + list(self.live_iterators.values()):
            for key in ('next', 'prev', 'partner'):
                value = getattr(link, key, None)
                if isinstance(value, self.r.Iterator):
                    referenced.add(id(value))
        for serial in sorted(self.completed_iterators):
            if id(self.live_iterators[serial]) not in referenced:
                del self.live_iterators[serial]
        self.completed_iterators.intersection_update(self.live_iterators)

    def close(self):
        self.o.GameObject, self.r.Iterator = self.original_object, self.original_iterator

    def oid(self, obj):
        if obj is None:
            return 0
        return self.retained.index(obj) + 1

    def ids(self, objects):
        return [self.oid(x) for x in objects]

    def pos(self, x=0, y=0, z=0):
        return [self.r.from_int(v) for v in (x, y, z)]

    def new(self, x=0, y=0, z=0, props=None, class_def=None):
        return self.w.new_object(0x180, self.pos(x, y, z), class_def, props)

    def destroy(self, obj, flags):
        self.events.append(['destroy', self.oid(obj), flags, self.w.tearing_down])
        self.w.destroy(obj, flags)

    def world_snapshot(self):
        w, r = self.w, self.r
        cells = w.grid + w.grid_b + [w.off_grid, w.always_draw, w.delete_list, w.run_active] + w.run15 + w.run60
        ids = {id(c): c.name for c in cells}
        for i, obj in enumerate(self.retained, 1):
            for c in obj.heads:
                ids[id(c)] = (i << 32) | c.name
        for c in self.extra:
            ids[id(c)] = c.name
        for n in w.handles.nodes:
            ids[id(n)] = r.HandlePool.NODE_BASE + 20*n.index
        for i, it in self.live_iterators.items():
            ids[id(it)] = (1 << 63) | i
        def ref(x):
            return 0 if x is None else ids[id(x)]
        roots = cells + [c for obj in self.retained for c in obj.heads] + self.extra + w.handles.nodes
        roots += [it for serial,it in self.live_iterators.items() if serial not in self.completed_iterators]
        pending, seen, reachable_iterators = list(roots), set(), set()
        while pending:
            link = pending.pop()
            if id(link) in seen: continue
            seen.add(id(link))
            if isinstance(link, r.Iterator): reachable_iterators.add(id(link))
            for key in ('next','prev','partner'):
                value=getattr(link,key,None)
                if value is not None: pending.append(value)
        links = []
        for c in cells:
            if c.next is not None:
                links.append([ref(c), 0, ref(c.next), 0, 0, 0, 0])
        for c in [c for obj in self.retained for c in obj.heads] + self.extra:
            links.append([ref(c), 0, ref(c.next), 0, 0, 0, 0])
        for n in w.handles.nodes:
            row = [ref(n), 1, ref(n.next), ref(n.prev), ref(n.partner), self.oid(n.obj), n.tag]
            default_next = r.HandlePool.NODE_BASE + 20*(n.index+1) if n.index+1 < len(w.handles.nodes) else 0
            if row != [ref(n), 1, default_next, 0, 0, 0, 0]:
                links.append(row)
        for it in self.live_iterators.values():
            if id(it) not in reachable_iterators: continue
            links.append([ref(it), 2, ref(it.next), ref(it.prev), ref(it.partner), self.oid(it.obj), it.mask])
        props_ids, class_ids = {}, {}
        def alias(value, mapping):
            if value is None:
                return 0
            if id(value) not in mapping:
                mapping[id(value)] = len(mapping) + 1
            return mapping[id(value)]
        objs = []
        for i, obj in enumerate(self.retained, 1):
            cd = obj.class_def
            objs.append([i] + [getattr(obj, k) for k in OBJECT_FIELDS] +
                        [alias(obj.props, props_ids), obj.props, alias(cd, class_ids),
                         None if cd is None else [cd.lump, cd.generation, cd.entries]])
        # Lossless pool-link deltas from the public default pool, including removed keys.
        baseline = r.Pool()
        pool_delta = [[k, w.pool.link.get(k)] for k in sorted(set(baseline.link) | set(w.pool.link)) if baseline.link.get(k) != w.pool.link.get(k)]
        names = [[i, k, self.oid(w.names.objs[i])] for i, k in enumerate(w.names.keys) if k is not None or w.names.objs[i] is not None]
        return [[w.pool.base, w.pool.head, w.pool.last, w.pool.end, pool_delta], w.shared.state,
                ref(w.handles.free), len(self.iterators), [6096, 28879], sorted(links), objs,
                [[k, self.oid(v)] for k, v in sorted(w.objects.items())], names, w.names.count,
                [[[t.fn, t.priority, t.name, t.flags] for t in w.tasks.tasks], w.tasks.cursor],
                [getattr(w, k) for k in GLOBALS], w.camera_focus, w.centres,
                [[p.held, p.pressed, p.edge2] for p in w.pads], w.spawn_pos, w.spawn_facing,
                0, [True]*5]

def load_references(research_dir):
    modules = {}
    old_path, previous = sys.path[:], sys.modules.get('ref_core')
    try:
        for name in ('core','netwire','objects','ordering'):
            pin = ORDERING_EVIDENCE if name == 'ordering' else REFERENCES[name]
            source = subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],capture_output=True,check=True,timeout=30).stdout
            if hashlib.sha256(source).hexdigest() != pin['sha256']:
                raise ValueError('immutable reference hash mismatch: '+name)
            if name == 'ordering': continue
            module = types.ModuleType('pinned_session_objects_'+name)
            module.__file__ = '/__verified_models__/ref_'+name+'.py'
            sys.modules[module.__name__] = module
            exec(compile(source,module.__file__,'exec'),module.__dict__)
            modules[name] = module
            if name == 'core': sys.modules['ref_core'] = module
    finally:
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core',None)
        else: sys.modules['ref_core'] = previous
    return modules

SCENARIOS=('ready','two_ready','delayed','no_ready','hold','no_sim','pending','frozen','mutation','seed_domain','run_failure','end_failure','message','message_failure','nested_world','hash_collision')
RUN=0x14b910
TASKS=((0xf0000001,'authored_pre',0,0),(0xf0000002,'authored_post',125,0),(0xf0000003,'authored_draw',21,1),(0xf0000004,'authored_late',20,0))
BINDINGS=('on_run','on_destroy','is_frozen','res_find','end_of_tick')

class CompositionFailure(Exception): pass

class Oracle(WorldOracle):
    """Explicit host policy. All wire/object algorithms remain pinned calls."""
    def __init__(self,modules,config,variant,scenario):
        super().__init__(modules['core'],modules['objects'],config,variant)
        self.modules,self.config,self.scenario=modules,config,scenario
        self.seed_initial=(0,1,0xdeadbeef,0xffffffff)[variant]
        self.combat_seed=self.seed_initial ^ 0xa5a5a5a5
        self.started=self.failed=False
        self.failure_operation=''
        self.phase,self.depth='idle',0
        self.host_events=[]
        self.ready_calls=0
        r=modules['netwire']; owner=self
        class ObservedMachine(r.Machine):
            def tick(machine,steps):
                # The observed bridge is explicit; do not duplicate the wire loop.
                # Owned peer/local roles cannot mutate across these single ticks.
                for _ in range(steps):
                    if not machine.level_pending:
                        owner.phase='pre_frame'
                        owner.w.frame=machine.net_frame
                        machine.sim_frame=machine.net_frame
                        machine.rand_seed=owner.w.shared.state
                        owner.phase='idle'
                    r.Machine.tick(machine,1)
        self.network=ObservedMachine(2,0,r.Fpu(MODELS[config//4],bool(config&2),bool(config&1)))
        self.network.network_mode=1
        self.network.num_players=2
        self.network.run_tasks=self.ready_tick
        self.w.network_mode=1
        self.w.frame=self.network.sim_frame
        self.w.on_run=self.on_run
        self.w.on_destroy=self.on_destroy
        self.w.is_frozen=self.is_frozen
        self.w.res_find=lambda lump,name:0
        self.w.end_of_tick=self.end_tick
        self.w.tasks.add(RUN,10,b'runObjects',0)
        self.publish()
        for fn,name,priority,flags in TASKS: self.w.tasks.add(fn,priority,name.encode(),flags)
        self.with_world(self.setup_world)
        if scenario in ('message','message_failure'):
            self.network.handlers[7]=self.message
            self.network.queue_message(7,bytes([variant+1,0x44]))

    def guard(self,label,fn):
        if self.failed: raise CompositionFailure('latched')
        try: return fn()
        except Exception:
            if not self.failed: self.failed,self.failure_operation=True,label
            if self.depth==0 and self.phase=='idle': self.import_pads();self.publish()
            raise

    def import_pads(self):
        for target,source in zip(self.w.pads,self.network.pads):
            target.held,target.pressed,target.edge2=struct.unpack_from('<III',source,8)

    def publish(self):
        self.network.rand_seed=self.w.shared.state
        self.network.sim_frame=self.w.frame
        for source,target in zip(self.w.pads,self.network.pads):
            struct.pack_into('<III',target,8,source.held&0xffffffff,source.pressed&0xffffffff,source.edge2&0xffffffff)

    def scope(self,phase,fn):
        if not self.depth:self.import_pads()
        previous=self.phase;self.phase=phase;self.depth+=1
        try:
            result=fn()
            if self.failed:raise CompositionFailure('latched')
            self.publish();self.phase=previous
            return result
        finally:
            self.publish();self.depth-=1

    def with_world(self,fn):return self.guard('with_world',lambda:self.scope('world',fn))
    def pads(self):return [[p.held,p.pressed,p.edge2] for p in self.w.pads]
    def note(self,kind,*values):self.events.append([kind,*values,self.w.frame,self.network.net_frame,self.pads(),self.w.shared.state,self.phase,self.depth])
    def host_event(self,kind,name,values=()):self.host_events.append([kind,name,self.network.net_frame,list(values)])

    def setup_world(self):
        self.a,self.b,self.c=self.new(),self.new(256),self.new(-256)
        for obj in (self.a,self.b,self.c):obj.tag=0
        self.w.run_every_frame(self.a);self.w.run_every_15(self.b);self.w.run_every_60(self.c)
        self.w.tasks.remove(TASKS[3][0])
        if self.scenario=='frozen' and self.variant==0:self.w.delete(self.b)

    def on_run(self,obj):
        def action():
            self.note('run',self.oid(obj),obj.flags)
            obj.tag+=1
            draw=self.w.shared.draw();self.note('draw',draw)
            if self.scenario=='mutation' and obj is self.a and obj.tag==1:
                if self.variant==0:self.w.run_every_60(obj)
                elif self.variant==1:
                    added=self.new(512);added.tag=0;self.w.run_every_15(added)
                elif self.variant==2:self.w.delete(self.b)
                else:self.w.tasks.add(TASKS[3][0],20,b'authored_late',0)
            if self.scenario=='run_failure':raise CompositionFailure('authored run failure')
        self.with_world(action)

    def on_destroy(self,obj,flags):
        def action():self.note('destroy',self.oid(obj),flags);self.w.destroy(obj,flags)
        self.with_world(action)
    def is_frozen(self):
        def action():self.note('frozen');return self.scenario=='frozen' and self.variant!=3
        return self.with_world(action)
    def end_tick(self):
        def action():
            self.note('end')
            if self.scenario=='end_failure':self.w.pads[0].pressed=77;raise CompositionFailure('authored end failure')
        self.with_world(action)

    def task(self,fn):
        name=next(t[1] for t in TASKS if t[0]==fn)
        self.host_event('task',name)
        def action():
            self.note(name)
            if name=='authored_pre' and self.scenario=='nested_world':
                self.w.pads[0].pressed=0x55
                self.with_world(lambda:self.note('nested'))
        self.with_world(action)

    def ready_tick(self,_):
        self.ready_calls+=1
        def action():
            def invoke(fn):
                if fn==RUN:self.host_event('task','runObjects');self.w.run_objects()
                else:self.task(fn)
            self.w.tasks_run(1,invoke)
        self.guard('task',lambda:self.scope('tasks',action))

    def message(self,payload):
        def action():
            self.host_event('message','authored_message',payload)
            def body():
                self.note('message',list(payload))
                self.a.tag=payload[0]+10
                self.w.pads[0].held^=0x20
                draw=self.w.shared.draw();self.note('message_draw',draw)
                def nested():
                    self.w.pads[0].pressed=0x33;self.note('message_nested')
                    if self.scenario=='message_failure':raise CompositionFailure('authored message failure')
                self.with_world(nested)
            self.scope('message',lambda:self.with_world(body))
        self.guard('message:authored_message',action)

    def submit(self,k,inbox):
        def action():
            self.started=True
            sample=struct.pack('<H6B',257*(k+1),128+k,64+3*k,255-k,17*k,31*k,47*k)
            return self.network.net_main(sample,yaw=k*1234,camera_flag=k&1,
                frame_step=2 if self.scenario=='two_ready' else 4 if self.scenario=='hold' and k==0 else 1,
                no_sim=self.scenario=='no_sim' or (self.scenario=='hold' and k==0),
                hold_arg=self.scenario=='hold' and k>0,render_frame=k,inbox=inbox)
        return self.guard('submit',action)

    def step_view(self):
        return copy.deepcopy([self.ready_calls,self.network.net_frame,self.network.sim_frame,self.w.frame,self.w.shared.state,
                self.network.rand_seed,self.combat_seed,self.pads(),self.phase,self.depth,self.failed,self.world_snapshot()])

    def snapshot(self):
        old=[['rta',self.config//4,bool(self.config&2),bool(self.config&1),2,0,1,self.combat_seed,0],
             self.started,self.failed,False,0,self.failure_operation,[],
             [[7,'authored_message']] if self.scenario in ('message','message_failure') else [],
             network_snapshot(self.network,self.config),True,[],[],[],self.combat_seed,[],self.host_events]
        return [old,self.world_snapshot(),[[RUN,'runObjects']]+[[t[0],t[1]] for t in TASKS],
                [[k,'authored_'+k,True] for k in BINDINGS],self.phase,self.depth,True]


def ready_packet(r,config):
    m=r.Machine(2,1,r.Fpu(MODELS[config//4],bool(config&2),bool(config&1)))
    for frame in range(1,13):
        m.peers[1].set_record(frame,r.rec12(frame*17,(128+frame,64+frame,255-frame,frame*9),
            (frame*11,frame*13),frame*123,frame&1),start=0,count=0)
    return m.build_packet(0,1,12,r.PKT_RAW_RELAY,1,None,raw=True).data


def run_scenario(modules,config,variant,scenario):
    oracle=Oracle(modules,config,variant,scenario)
    try:
        packet=ready_packet(modules['netwire'],config)
        results=[]
        if scenario=='pending':oracle.network.level_pending=True
        for k in range(4):
            if scenario=='pending' and k==3:oracle.network.level_pending=False
            inbox=[packet] if (scenario=='delayed' and k==2) or (scenario not in ('delayed','no_ready') and k==0) else []
            try:
                datagrams,held,steps=oracle.submit(k,inbox)
                results.append([[sent(x) for x in datagrams],held,steps,oracle.step_view()])
            except CompositionFailure:results.append(['failed',oracle.step_view()])
        if scenario=='hash_collision':
            before=oracle.network.peers[0].sync(oracle.network.net_frame)
            tag=oracle.a.tag
            oracle.with_world(lambda:setattr(oracle.a,'tag',tag+777))
            results.append([before==oracle.network.peers[0].sync(oracle.network.net_frame),tag!=oracle.a.tag])
        if scenario=='seed_domain':results.append([oracle.combat_seed==oracle.seed_initial^0xa5a5a5a5,oracle.w.shared.state==oracle.network.rand_seed])
        return [results,oracle.events,oracle.snapshot()]
    finally:oracle.close()

def generate(research_dir,output):
    modules=load_references(research_dir)
    output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    vector_hash,trace_hash=hashlib.sha256(),hashlib.sha256()
    trace_fnv=14695981039346656037
    counts=collections.Counter();number=0
    with output.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
        header=b'# session_objects_vectors_v1\n';stream.write(header);vector_hash.update(header)
        for config in range(16):
            for variant in range(4):
                for name in SCENARIOS:
                    first=sc(run_scenario(modules,config,variant,name))
                    second=sc(run_scenario(modules,config,variant,name))
                    if first!=second:raise AssertionError('independent replay replicas diverged')
                    row=('\t'.join([name,str(config),f'{number};{variant}','ok',first])+'\n').encode('ascii')
                    stream.write(row);vector_hash.update(row);trace_hash.update(row)
                    for byte in row:trace_fnv=((trace_fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[name]+=1;number+=1
    manifest={
        'schema_version':1,'suite':'session-objects','reference_sha256':REFERENCE_SHA256,
        'references':REFERENCES,'ordering_evidence':ORDERING_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
        'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'vector_count':number,'vector_sha256':vector_hash.hexdigest(),
        'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
        'expected_trace_sha256':trace_hash.hexdigest(),'expected_trace_fnv1a64':f'{trace_fnv:016x}',
        'operations':dict(sorted(counts.items())),
        'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
        'seeds':[0,1,0xdeadbeef,0xffffffff],'replicas_per_case':2,
        'trace_columns':['operation','configuration','id;variant','status','results_events_complete_owned_snapshot'],
        'scope':'Authored RTA Session ownership adapter for pinned object and network models. No game-native implementations, rendering, original-client networking, Norrath parity or playable-world claim.',
        'composition_contract':{
            'ordering':'Before each nonpending original Machine.tick(1), assign World.frame and sim_frame from net_frame. This explicit extension is supported by the separately pinned observed mainloop document, not an invented frame increment.',
            'tasks':'Fresh World, runObjects priority10 only; authored registered pre/post/draw/late tasks. World.tasks_run(1) calls pinned run_objects, with no renderer or World.init.',
            'pads':'Import current three network pad words at each outer World scope; nested scopes retain mutations. Publish pads, World.frame and shared seed on normal and exceptional exit.',
            'seeds':'World.shared and net.rand_seed share an explicitly mapped domain. Combat seed is independent and never drawn in these cases.',
            'failures':'Authored first-error latch retains partial state and innermost World phase; later mutating calls refuse. Native resource and API-refusal limits are independently hand-tested, not attributed to Python.',
            'snapshot':'Self-contained authored serializer; complete graph represented by reconstructible default pool/cell layout and all nondefault links, identities and values. Per-step primitive snapshots are deep copies.',
            'equality':'Two fresh same-role replicas compare every complete trace. Opposite endpoint transport is independently tested by native anchors.',
            'callbacks':'Five named supplied handlers. Function code/capture identities are outside value-state proof; presence, labels, effects and ordered events are retained.',
        },
    }
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='ascii',newline='\n')
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_objects.tsv.gz'))
    args=parser.parse_args();manifest=generate(args.research_dir,args.output)
    print(json.dumps({k:manifest[k] for k in ('vector_count','vector_sha256','expected_trace_sha256','expected_trace_fnv1a64')}))

if __name__=='__main__':main()
