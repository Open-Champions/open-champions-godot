#!/usr/bin/env python3
"""Independent original-model singleton creature movement composition.

Only the explicit Session service/authority policy is authored. Immutable
Creature_moveBy, terrain queries, World/grid and network bodies perform work.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import os
import pathlib
import sys
import tempfile
import types

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE_PIN = {'path': 'tools/generate_session_terrain_vectors.py',
            'sha256': 'dfffb7ad22bacf5d62b2c3ed30617af6fcd3298982724fa5ced0ae9be01b527b'}

def load_base():
    path = ROOT / BASE_PIN['path']
    source = path.read_bytes()
    if hashlib.sha256(source).hexdigest() != BASE_PIN['sha256']:
        raise ValueError('authored Session terrain helper hash mismatch')
    module = types.ModuleType('session_creature_terrain_helper')
    module.__file__ = str(path)
    sys.modules[module.__name__] = module
    exec(compile(source, str(path), 'exec'), module.__dict__)
    return module

BASE = load_base()
REFERENCES, REFERENCE_SHA256 = BASE.REFERENCES, BASE.REFERENCE_SHA256
HELPERS = {**BASE.HELPERS, 'session_terrain': BASE_PIN}
ORDERING_EVIDENCE = BASE.ORDERING_EVIDENCE
SPECIFICATION_EVIDENCE = BASE.SPECIFICATION_EVIDENCE
CREATURE_SPECIFICATION_EVIDENCE = BASE.CREATURE_SPECIFICATION_EVIDENCE
COMPOSITION_POLICY = 'rta-session-creature-v1'
MODELS = BASE.MODELS
Refusal, TaskFailure = BASE.Refusal, BASE.TaskFailure
RUN, TASKS = BASE.RUN, BASE.TASKS


def callback_value(value):
    if isinstance(value, tuple):
        return ['tuple', [callback_value(v) for v in value]]
    if isinstance(value, list):
        return ['list', [callback_value(v) for v in value]]
    return value

SCENARIOS = ('ready','root','timer','inert','threshold','slide','grid','blocked',
             'delayed_hold','frozen','hp','retired','double_move','missing_grid',
             'post_failure','opposite')
FPU_SCENARIOS = {'ready','root','timer','threshold','slide','grid','delayed_hold','frozen','hp','opposite'}
VARIANTS = {n:tuple(range(3 if n=='blocked' else 2 if n in {'ready','root','threshold','slide','grid','delayed_hold','hp','retired'} else 1)) for n in SCENARIOS}
TABLE = 0x10000000


def make_oracle(modules, helpers, config, variant, scenario, local=0):
    core, creature, world = (modules[k] for k in ('core','creature','world'))
    o = BASE.make_oracle(modules, helpers, config, variant, scenario, local)
    # Reuse the frozen singleton host/HP factory, not its generic mover. The
    # explicit supplied services below call the original Creature_moveBy body.
    if not o.hp_enabled:
        o.hp_enabled = True
        o.init_hp()
    o.a.radius, o.a.height, o.a.angle = 0x40800000, 0x41000000, 0
    o.hp.objects[0].cflags = 0x100000
    o.project_hp()
    o.terrain_binding = None
    o.creature_phase='idle';o.creature_publication_error='';o.creature_depth=0;o.creature_calls=0
    o.creature_last_velocity=None;o.creature_last_bits=None
    o.creature_results=[]
    root=[0x3f000001,0xbf800000] if scenario=='root' else [0,0]
    yaw=(0xffff if variant else 0x4000) if scenario=='root' else 0
    timer=0xffff if scenario=='timer' else 0
    if scenario=='timer':root=[0x3f800000,0xbf800000];yaw=123
    inert=8 if scenario=='inert' else 0
    o.creature_initial=[root,yaw,timer,variant,inert,TABLE]
    p=o.hp.model.mem.poke;a=o.a.addr
    p(a+0xbc,8,root[0]|root[1]<<32);p(a+0xc4,2,yaw);p(a+0x314,2,timer)
    p(a+0x230,1,variant);p(a+0x231,1,inert)
    for slot,fn in ((6,0x11a418),(10,0x11a3f0),(11,0x11a448)):
        p(TABLE+slot*8,2,0);p(TABLE+slot*8+4,4,fn)
    o.hp.regions.append([TABLE,128,'session.creature.base_table'])
    o.a.vtable=TABLE;p(a+0x48,4,TABLE)
    for off,size,value in ((0x1c,4,o.a.radius),(0x20,4,o.a.height),(0x30,4,o.a.vz),(0x38,2,o.a.angle)):
        p(a+off,size,value)
    class Movement(creature.Model):
        def __init__(self, mem):
            super().__init__(mem);self.events=[]
        def _w(self,address,size,value):
            self.events.append(['write',address,size,value&((1<<(size*8))-1)])
            return super()._w(address,size,value)
        def call(self,name,*args):
            av=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            self.events.append(['call',name,[callback_value(v) for v in av]])
            result=super().call(name,*args)
            o.creature_results.append([len(self.calls),callback_value(result)])
            return result
    o.movement=Movement(o.hp.model.mem)
    def publish():
        o.publish_hp()
        record=o.hp.objects[0];o.a.flags=record.flags;o.a.pos=list(record.pos)
        o.w.frame=o.hp.world.frame
        m=o.movement
        o.a.radius=m.u32(a+0x1c);o.a.height=m.u32(a+0x20);o.a.vz=m.u32(a+0x30)
        o.a.angle=m.u16(a+0x38)
    def service(phase,fn):
        o.creature_phase=phase
        publish()
        result=fn()
        o.creature_phase='move_by'
        return result
    def slide(address,delta,mask,want_normal,radius):
        def body():
            hit,pos,normal=world.world_slide_move(o.level,radius,o.movement.vec(address),delta,mask)
            for i,v in enumerate(pos):p(address+4*i,4,v)
            publish()
            return (hit,normal) if want_normal else hit
        return service('slide',body)
    def grid(address):
        def body():publish();o.w.update_grid_cell(o.a)
        return service('grid_update',body)
    def collect(lo,hi,limit):
        def body():publish();return [obj.addr for obj in o.w.collect_in_box(lo,hi,limit)]
        return service('collect',body)
    def height(_level,pos,radius):
        return service('height',lambda:world.terrain_height_below(o.level,radius,pos))
    o.movement.cb={'world_slideMove':slide,'obj_updateGridCell':grid,'world_collectObjectsInBox':collect,'terrain_heightBelow':height}
    original_snapshot=o.snapshot
    original_ready=o.ready_tick
    def snapshot(self):
        base=original_snapshot()
        m=self.movement
        calls=[[name,[callback_value(v)for v in args]]for name,args in m.calls]
        events=[[0,e[1:],['',[]]]if e[0]=='write'else[1,[0,0,0],[e[1],e[2]]]for e in m.events]
        movement=[self.hp_bindings[0],self.creature_initial,COMPOSITION_POLICY,self.creature_phase,
          self.creature_publication_error,'hp.adapter.model.mem',self.creature_depth,self.creature_calls,
          self.creature_last_velocity,self.creature_last_bits,[1048576,65536,64,64],0,True,
          [[k,label,True]for k,label in sorted((('world_slideMove','session.creature.slide'),('obj_updateGridCell','session.creature.grid'),('world_collectObjectsInBox','session.creature.collect'),('terrain_heightBelow','session.creature.height')))],
          [],m.writes,calls,events,self.creature_results,m.draws,self.object_run_completed,self.task_priority]
        return [base,movement]
    def move(self,velocity):
        def action():
            if self.phase!='tasks' or self.depth!=1 or not self.object_run_completed or self.task_priority is None or self.task_priority<=10 or self.creature_calls:raise Refusal('phase')
            alive=[obj for obj in self.retained if obj.alive]
            if len(alive)!=1 or alive[0]is not self.a or self.w.objects.get(a)is not self.a:raise Refusal('singleton')
            self.creature_calls+=1;self.creature_depth=1;self.creature_phase='import'
            self.creature_last_velocity=list(velocity);self.creature_last_bits=None
            self.import_hp_scalars()
            for off,size,value in ((0x1c,4,self.a.radius),(0x20,4,self.a.height),(0x30,4,self.a.vz),(0x38,2,self.a.angle)):
                p(a+off,size,value)
            try:
                self.creature_phase='move_by'
                bits=self.movement.move_by(a,velocity)
                self.creature_last_bits=bits;self.creature_phase='publish';publish()
                self.creature_phase='idle';self.move_calls+=1
                return bits
            except Exception:
                publish();raise
            finally:self.creature_depth=0
        return self.guard('move_creature',action)
    def task(self,fn):
        name=next(t[1]for t in TASKS if t[0]==fn)
        self.host_events.append(['task',name,self.network.net_frame,[]]);self.effect(name)
        if name=='terrain_pre':
            if scenario=='retired' and self.ready_calls==2:
                self.w.destroy(self.a,3);self.effect('destroy',self.oid(self.a))
                if variant:
                    other=self.w.new_object(0x400,self.pos(9,0,0));self.effect('replacement',self.oid(other),other.addr)
            if scenario=='missing_grid' and self.ready_calls==1:self.w.handles.clear(self.a.heads[2])
            if scenario=='hp' and variant==0:self.take_damage(0,0x40000000,None,0x40)
        elif name=='terrain_move':
            self.step_effects.append(['creature',self.move(self.velocity())])
            if scenario=='double_move':self.move(self.velocity())
        else:
            if scenario=='hp' and variant==1:self.take_damage(0,0x40000000,None,0x40)
            if scenario=='post_failure':raise TaskFailure('post')
    def ready(self,unused):
        self.creature_calls=0
        return original_ready(unused)
    o.snapshot=types.MethodType(snapshot,o);o.move=types.MethodType(move,o);o.task=types.MethodType(task,o)
    o.ready_tick=types.MethodType(ready,o);o.network.run_tasks=o.ready_tick
    return o


def capture(o, helpers, k, inbox, previous):
    return BASE.capture(o,helpers,k,inbox,previous)


def run_scenario(modules, helpers, config, variant, scenario):
    if scenario=='opposite':return run_opposite(modules,helpers,config)
    o=make_oracle(modules,helpers,config,variant,scenario)
    try:
        if scenario=='blocked' and variant==1:o.network.level_pending=True
        initial=copy.deepcopy(o.snapshot());previous=initial;steps=[]
        packet=helpers['session_objects'].ready_packet(modules['netwire'],config)
        for k in range(4):
            if scenario=='blocked' and variant==1 and k==2:o.network.level_pending=False
            delayed=scenario=='delayed_hold' and variant==0
            no_ready=scenario=='blocked' and variant==0
            inbox=[packet] if (delayed and k==2)or(not delayed and not no_ready and k==0)else[]
            step,previous,_=capture(o,helpers,k,inbox,previous);steps.append(step)
        return [initial,steps,o.events,o.step_effects]
    finally:o.close()


def run_opposite(modules, helpers, config):
    peers=[];instrumentation=[]
    for i in (0,1):
        o=make_oracle(modules,helpers,config,0,'opposite',i)
        peers.append(o);instrumentation.append((modules['objects'].GameObject,modules['core'].Iterator));o.close()
    def activate(i):modules['objects'].GameObject,modules['core'].Iterator=instrumentation[i]
    try:
        initial=[]
        for i,o in enumerate(peers):activate(i);initial.append(copy.deepcopy(o.snapshot()))
        previous=copy.deepcopy(initial);steps=[];queues=[[],[]];completed=[{},{}]
        for turn,which in enumerate((0,0,1,0,1,1,0,1,0,1,0,1)):
            activate(which);o=peers[which];inbox=queues[which]if turn>=3 else[]
            if turn>=3:queues[which]=[]
            old=o.ready_calls;step,state,sent=capture(o,helpers,turn,inbox,previous[which]);previous[which]=state
            for packet in sent:queues[packet.peer].append(packet.data)
            if o.ready_calls>old:
                completed[which][o.network.net_frame]=copy.deepcopy([o.world_snapshot(),state[0][0][1],state[0][1],state[1]])
            steps.append([which,step])
        common=sorted(set(completed[0])&set(completed[1]))
        equal=[[frame,completed[0][frame]==completed[1][frame]]for frame in common]
        return [initial,steps,[o.events for o in peers],[queues,equal]]
    finally:
        for o in peers:o.close()


def check_case(value,scenario,variant,helpers):
    if scenario=='opposite':
        assert value[3][1] and all(row[1]for row in value[3][1]),'opposite completed states differ'
        return
    state=copy.deepcopy(value[0]);states=[]
    for step in value[1]:
        state=helpers['session_hp'].apply_delta(state,step[2]);states.append(copy.deepcopy(state))
    failed=scenario in {'retired','double_move','missing_grid','post_failure'}
    assert bool(value[1][-1][1][6])==failed,(scenario,'failure state')
    if failed:assert states[-1]==states[-2],(scenario,'retry changed state')
    expected={'blocked':2 if variant==1 else 0,'delayed_hold':2 if variant==0 else 3,
              'retired':1,'double_move':1,'missing_grid':0,'post_failure':1}.get(scenario,3)
    assert value[1][-1][1][1]==expected,(scenario,'unexpected movement count',value[1][-1][1])
    # Complete numeric domains remain separate; dry movement performs no draw.
    base,movement=state
    old=base[0][0][0];w=base[0][0][1];hp=base[0][1]
    assert old[13]==((1 if variant==0 else 0xdeadbeef)^0xa5a5a5a5) and not old[14]
    assert w[1]==old[8][0][19]
    assert movement[19]==0
    if scenario=='hp':assert value[1][-1][1][8]==[0x42bc0000]
    if scenario=='inert':
        assert movement[15]==[] and movement[16]==[] and movement[17]==[]
        assert value[1][-1][1][4]==value[1][0][1][4]
    if scenario=='root':
        backing=dict(hp[5][12][0]);address=hp[1][0][2]
        assert all(backing[address+off]==0 for off in range(0xbc,0xc6))
    if scenario=='blocked' and expected==0:assert state[0][1][0]==value[0][0][1][0]


def generate(research_dir,output):
    modules=BASE.load_references(research_dir);helpers=BASE.load_helpers();sc=helpers['session_objects'].sc
    output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    manifest_path=output.with_name(output.name[:-7]+'.json')
    fd,name=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent)
    temp=pathlib.Path(name);manifest_temp=None
    vh,th=hashlib.sha256(),hashlib.sha256();fnv=14695981039346656037
    counts=collections.Counter();number=size=longest=0
    try:
        with os.fdopen(fd,'wb') as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
                header=b'# session_creature_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for name in SCENARIOS:
                    for config in range(16)if name in FPU_SCENARIOS else(0,):
                        for variant in VARIANTS[name]:
                            value=run_scenario(modules,helpers,config,variant,name);check_case(value,name,variant,helpers)
                            first=sc(value);second=sc(run_scenario(modules,helpers,config,variant,name))
                            if first!=second:raise AssertionError('two fresh reference executions differ')
                            row=('\t'.join([name,str(config),f'{number};{variant}','ok',first])+'\n').encode('ascii')
                            stream.write(row);vh.update(row);th.update(row)
                            for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                            number+=1;counts[name]+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'session-creature','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,
          'composition_policy':COMPOSITION_POLICY,'helpers':HELPERS,'ordering_evidence':ORDERING_EVIDENCE,
          'specification_evidence':SPECIFICATION_EVIDENCE,'creature_specification_evidence':CREATURE_SPECIFICATION_EVIDENCE,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),
          'recipe_configurations':{n:list(range(16))if n in FPU_SCENARIOS else[0]for n in SCENARIOS},
          'recipe_variants':{n:list(VARIANTS[n])for n in SCENARIOS},'expanded_bytes':size,'max_line_bytes':longest,
          'composition_contract':{'scope':'RTA dry singleton base Creature_moveBy with caller velocity; supplied post-fixup terrain, actual World collector/grid and HP backing. No original Player_move/water/contact-service/derived creature or Norrath claim.',
            'state':'Complete Session/network/World/pools/HP/Level plus every movement snapshot field; owned lossless per-step deltas, including typed callback results and ordered writes/calls. Borrowed backing is serialized once in HP.',
            'authority':'Only live World scalar import and raw movement publication are authored. Original immutable Creature_moveBy, world_slide_move, terrain_height_below, collect_in_box, update_grid_cell, tasks_run and net_main bodies execute.',
            'rng':'Dry operation does not draw; World shared, object own and dormant combat states remain independently observed.',
            'limits':'Malformed identity, water/derived domain and bounded work failures are separate native API tests, never fabricated Python success.',
            'publication':'Temporary siblings flushed/fsynced; fixture replacement then manifest last. Each case runs twice fresh, separate-process regeneration supported.'}}
        fd2,name=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent);manifest_temp=pathlib.Path(name)
        with os.fdopen(fd2,'w',encoding='ascii',newline='\n')as stream:
            json.dump(manifest,stream,sort_keys=True,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.replace(temp,output);os.replace(manifest_temp,manifest_path)
        return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_creature.tsv.gz'))
    args=parser.parse_args();print(json.dumps(generate(args.research_dir,args.output),sort_keys=True))

if __name__=='__main__':main()
