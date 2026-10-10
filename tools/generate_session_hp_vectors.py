#!/usr/bin/env python3
"""Independent pinned-model oracle for the authored ready-frame base-HP gate.

The host policy is authored. Network, scheduler, pool and HP algorithms execute
immutable original Python bodies; no native output supplies expected values.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import importlib.util
import json
import pathlib
import struct
import subprocess
import sys
import types

HELPERS = {
    'session_objects': {'path': 'tools/generate_session_objects_vectors.py', 'sha256': '05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'},
    'combat_snapshot': {'path': 'tools/generate_combat_creature_vectors.py', 'sha256': 'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
}
REFERENCES = {
    'core': {'commit':'41e847af5360724ab62d208594f8027ea3c1b686','path':'tools/ref_core.py','sha256':'0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'objects': {'commit':'0dd02ce416389dd440540119727d946636507703','path':'tools/ref_objects.py','sha256':'541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'},
    'netwire': {'commit':'54258f9d7e281ef07f0675040e82e6264f9386cf','path':'tools/ref_netwire.py','sha256':'4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'},
    'combat': {'commit':'7212ecfecbeae8dd2015f6c82897226f49a94695','path':'tools/ref_combat.py','sha256':'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
    'creature': {'commit':'a0d89502ce1431375a6fc045b91a73039114e6b9','path':'tools/ref_creature.py','sha256':'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256=hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
COMPOSITION_POLICY='rta-session-hp-v1'
ORDERING_EVIDENCE={'commit':'0dd02ce416389dd440540119727d946636507703','path':'docs/mainloop.md','sha256':'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}
SPECIFICATION_EVIDENCE={'commit':'a0d89502ce1431375a6fc045b91a73039114e6b9','path':'docs/spec/creature-tick.md','sha256':'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
MODELS=('ps2','ps2g1','chop','nearest')
NODE_BASE,NODE_COUNT,FREE_HEAD=0x847268,6096,0x864ea8
RUN=0x14b910
TASKS=((0xf1000001,'hp_pre',20,0),(0xf1000002,'hp_apply',30,0),(0xf1000003,'hp_post',40,0))
SCENARIOS=('ready','delayed','no_ready','no_sim','pending','hold','frozen','scalar_authority','negative_zero','inert_ignore','delete_after','delete_before','reuse','pool_failure','post_failure','end_failure','physical_unavailable','seed_domain')
FPU_SCENARIOS=set(SCENARIOS)-{'no_ready','no_sim','reuse','physical_unavailable'}
VARIANTS={name: (0,1) if name in {'ready','scalar_authority','negative_zero','inert_ignore','seed_domain'} else (0,) for name in SCENARIOS}


def load_helpers():
    root=pathlib.Path(__file__).resolve().parents[1];out={}
    for name,pin in HELPERS.items():
        path=root/pin['path'];source=path.read_bytes()
        if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('authored helper hash mismatch: '+name)
        module=types.ModuleType('session_hp_helper_'+name);module.__file__=str(path)
        sys.modules[module.__name__]=module
        exec(compile(source,str(path),'exec'),module.__dict__);out[name]=module
    return out


def load_references(research_dir):
    out={};previous=sys.modules.get('ref_core')
    try:
        for name in ('core','objects','netwire','combat','creature','ordering','specification'):
            pin=ORDERING_EVIDENCE if name=='ordering' else SPECIFICATION_EVIDENCE if name=='specification' else REFERENCES[name]
            source=subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],check=True,capture_output=True,timeout=30).stdout
            if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('immutable reference hash mismatch: '+name)
            if name in ('ordering','specification'):continue
            module=types.ModuleType('session_hp_pinned_'+name);module.__file__='/__verified_models__/ref_'+name+'.py'
            sys.modules[module.__name__]=module;exec(compile(source,module.__file__,'exec'),module.__dict__);out[name]=module
            if name=='core':sys.modules['ref_core']=module
    finally:
        if previous is None:sys.modules.pop('ref_core',None)
        else:sys.modules['ref_core']=previous
    return out


class Refusal(Exception): pass
class TaskFailure(Exception): pass
class Opaque:
    def __init__(self,raw):self.raw=raw


def make_oracle(modules,helpers,config,variant,scenario):
    so,cs=helpers['session_objects'],helpers['combat_snapshot']
    core,objects,wire,combat,creature=(modules[k] for k in ('core','objects','netwire','combat','creature'))
    class Oracle(so.WorldOracle):
        def __init__(self):
            super().__init__(core,objects,config,variant)
            combat.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
            self.config,self.scenario=config,scenario
            self.seed_initial=(1,0xdeadbeef)[variant];self.combat_seed=self.seed_initial^0xa5a5a5a5
            self.w.shared.state=self.seed_initial;self.w.network_mode=1
            self.started=self.failed=False;self.failure_operation='';self.phase='idle';self.depth=0
            self.host_events=[];self.ready_calls=0;self.hp_calls=0;self.hp_depth=0
            self.registry={};self.binding_history=[];self.hp_bindings=[];self.hp_phase='idle';self.known_cells={};self.object_run_completed=False;self.task_priority=None
            self.step_effects=[];self.attached=True;self.calls_this_tick=0;owner=self
            class Machine(wire.Machine):
                def tick(machine,steps):
                    for _ in range(steps):
                        if not machine.level_pending:
                            owner.phase='pre_frame';owner.w.frame=machine.net_frame
                            machine.sim_frame=machine.net_frame;machine.rand_seed=owner.w.shared.state;owner.phase='idle'
                        wire.Machine.tick(machine,1)
            self.network=Machine(2,0,wire.Fpu(MODELS[config//4],bool(config&2),bool(config&1)))
            self.network.network_mode=1;self.network.num_players=2;self.network.run_tasks=self.ready_tick
            self.w.frame=self.network.sim_frame
            self.w.on_run=self.on_run;self.w.on_destroy=lambda obj,flags:self.w.destroy(obj,flags)
            self.w.is_frozen=lambda:scenario=='frozen';self.w.res_find=lambda lump,name:0;self.w.end_of_tick=self.end_tick
            self.w.tasks.add(RUN,10,b'runObjects',0)
            for fn,name,priority,flags in TASKS:self.w.tasks.add(fn,priority,name.encode(),flags)
            self.a=self.w.new_object(0x400,self.pos(1,variant,0));self.b=self.w.new_object(0x400,self.pos(3,4,0))
            for obj in (self.a,self.b):obj.flags=32;obj.tag=0
            self.w.run_every_frame(self.a)
            self.hp=types.SimpleNamespace(results=[],world=combat.World(0,0),objects=[],spells=[],bindings=[],regions=[],callbacks=[['handle_clear','session.raw_handles.clear'],['handle_link','session.raw_handles.link']],effects=[],pokes=[],depth=0,raw_depth=0)
            self.hp.ident=lambda obj:None if obj is None else self.hp.objects.index(obj)
            self.hp.address=lambda obj:0 if obj is None else self.hp.bindings[self.hp.ident(obj)][0]
            class Memory(creature.Memory):
                def load(mem,address,size):
                    return sum((owner.overlay_byte(address+i) if owner.covered(address+i) else super(Memory,mem).load(address+i,1))<<(8*i) for i in range(size))
                def poke(mem,address,size,value):
                    if any(owner.covered(address+i) for i in range(size)):raise Refusal('readonly')
                    return super().poke(address,size,value)
            class Model(creature.Model):
                def _w(model,address,size,value):
                    owner.hp.effects.append(['write',address,size,value&((1<<(8*size))-1)])
                    return super()._w(address,size,value)
                def call(model,name,*args):
                    owner.hp.effects.append(['call',name,list(args)])
                    return super().call(name,*args)
            self.hp.model=Model(Memory());self.hp.model.cb={'handle_clear':self.clear,'handle_link':self.link}
            self.hp.world.take_damage_fn=self.dispatch
            for obj in (self.a,self.b):self.bind_hp(obj)
            # Setup bytes are supplied backing, not model writes.
            for obj in (self.a,self.b):
                for off,size,value in ((0xc8,4,0),(0x231,1,0),(0x2dc,4,0),(0x2e0,2,0),(0x304,4,0),(0x3b0,2,0)):
                    self.hp.model.mem.poke(obj.addr+off,size,value)
            if scenario=='inert_ignore':
                if variant:self.hp.objects[0].cflags=1<<48;self.hp.objects[0].status=[0x3f800000]*5
                else:self.hp.model.mem.poke(self.a.addr+0x231,1,1)
            if scenario=='negative_zero':self.hp.objects[0].hp=(0,0xbf800000)[variant]
            self.project_hp();self.publish_world()

        def oid(self,obj):
            return -(obj.raw+1) if isinstance(obj,Opaque) else super().oid(obj)
        def world_snapshot(self):
            result=super().world_snapshot()
            for row in result[5]:
                raw=-row[5]-1 if row[5]<0 else 0
                if raw:row[5]=0
                row.append(raw)
            return result
        def bind_hp(self,obj):
            record=combat.Obj(flags=obj.flags,pos=list(obj.pos),hp=0x42c80000)
            self.hp.objects.append(record);index=len(self.hp.objects)
            self.hp.bindings.append([obj.addr,index,0x3b2,0,0,''])
            self.hp_bindings.append([index,self.oid(obj),obj.addr,index])
            for i,cell in enumerate(obj.heads):self.bind(obj.addr+4*i,cell,'object+'+str(4*i))
            for offset in (0x4c,0x2e4,0x360):self.intern(obj.addr+offset,'hp:'+str(index)+':'+str(offset))
            self.hp.world.frame=self.w.frame;self.project_hp()
        def refresh_cells(self):
            cells=self.w.grid+self.w.grid_b+[self.w.off_grid,self.w.always_draw,self.w.delete_list,self.w.run_active]+self.w.run15+self.w.run60
            cells += [c for obj in self.retained for c in obj.heads]+self.extra
            for cell in cells:self.known_cells[id(cell)]=cell.name
        def bind(self,raw,cell,label):
            self.refresh_cells()
            if raw in self.registry:raise Refusal('registered')
            self.registry[raw]=(cell,label)
            serial=next((self.oid(o) for o in self.retained if any(h is cell for h in o.heads)),0)
            self.binding_history.append([raw,cell,serial,label,True])
        def intern(self,raw,label):
            cell=core.Cell(0x90000000+4*len(self.extra));self.extra.append(cell);self.bind(raw,cell,label);return cell
        def ptr(self,link):
            if link is None:return 0
            if isinstance(link,core.Node):return NODE_BASE+20*link.index
            if isinstance(link,core.Cell):
                for raw,(cell,_) in self.registry.items():
                    if cell is link:return raw
                if id(link) in self.known_cells:return self.known_cells[id(link)]
            raise Refusal('physical_unavailable')
        def raw_obj(self,obj):return 0 if obj is None else obj.raw if isinstance(obj,Opaque) else obj.addr
        def covered(self,address):
            if NODE_BASE<=address<FREE_HEAD:return (address-NODE_BASE)%20<18
            if FREE_HEAD<=address<FREE_HEAD+4:return True
            return any(raw<=address<raw+4 for raw in self.registry)
        def node(self,n):
            if not 0<=n.tag<=65535:raise Refusal('tag')
            return [self.ptr(n),self.ptr(n.next),self.ptr(n.prev),self.ptr(n.partner),self.raw_obj(n.obj),n.tag,0 if isinstance(n.obj,Opaque) else self.oid(n.obj),isinstance(n.obj,Opaque)]
        def overlay_byte(self,address):
            if NODE_BASE<=address<FREE_HEAD:
                n=self.w.handles.nodes[(address-NODE_BASE)//20];row=self.node(n);off=(address-NODE_BASE)%20
                value=row[1+off//4] if off<16 else row[5]
                return (value>>(8*(off%4 if off<16 else off-16)))&255
            if FREE_HEAD<=address<FREE_HEAD+4:return (self.ptr(self.w.handles.free)>>(8*(address-FREE_HEAD)))&255
            for raw,(cell,_) in self.registry.items():
                if raw<=address<raw+4:return (self.ptr(cell.next)>>(8*(address-raw)))&255
            raise Refusal('not managed')
        def registry_snapshot(self):
            return [[raw,(serial<<32)|cell.name if serial else cell.name,serial,label,active] for raw,cell,serial,label,active in self.binding_history]
        def raw_snapshot(self):
            try:
                rows=[]
                for n in self.w.handles.nodes:
                    row=self.node(n);default=[row[0],row[0]+20 if n.index+1<NODE_COUNT else 0,0,0,0,0,0,False]
                    if row!=default:rows.append(row)
                heads=[[a,self.ptr(c.next)] for a,(c,_) in sorted(self.registry.items())]
                return [True,self.ptr(self.w.handles.free),rows,heads]
            except Refusal:return [False,0,[],[]]
        def clear(self,head):
            self.hp.raw_depth+=1
            try:return self.w.handles.clear(self.registry[head][0])
            finally:self.hp.raw_depth-=1
        def link(self,head,back,who):
            self.hp.raw_depth+=1
            try:return self.w.handles.link(self.registry[head][0],self.registry[back][0],Opaque(who) if who else None)
            finally:self.hp.raw_depth-=1
        def project_hp(self):
            p=self.hp.model.mem.poke;p(0x44fdcc,4,self.hp.world.frame)
            for record,binding in zip(self.hp.objects,self.hp.bindings):
                a=binding[0];p(a+0xc,4,record.flags)
                for i,v in enumerate(record.pos):p(a+0x10+4*i,4,v)
                p(a+0xa8,8,record.cflags);p(a+0x228,4,record.hp)
                for i,v in enumerate(record.status):p(a+0x340+4*i,4,v)
        def publish_hp(self):
            load=self.hp.model.mem.load
            for record,binding in zip(self.hp.objects,self.hp.bindings):
                a=binding[0];record.flags=load(a+0xc,4);record.pos=[load(a+0x10+4*i,4) for i in range(3)]
                record.cflags=load(a+0xa8,8);record.hp=load(a+0x228,4);record.status=[load(a+0x340+4*i,4) for i in range(5)]
            self.hp.world.frame=load(0x44fdcc,4)
        def dispatch(self,obj,amount,attacker,flags):
            self.project_hp()
            try:self.hp.model.take_damage(self.hp.address(obj),self.hp.address(attacker),flags,amount)
            finally:self.publish_hp()
        def take_damage(self,victim,amount,attacker,flags):
            def action():
                if self.phase!='tasks':raise Refusal('phase')
                for entry in self.hp_bindings:
                    known=self.retained[entry[1]-1];current=self.w.objects.get(entry[2])
                    if current is not None and current is not known:raise Refusal('reused')
                for index in (victim,attacker):
                    if index is not None and not self.retained[self.hp_bindings[index][1]-1].alive:raise Refusal('retired')
                if not self.raw_snapshot()[0]:raise Refusal('physical_unavailable')
                for record,entry in zip(self.hp.objects,self.hp_bindings):
                    world_obj=self.retained[entry[1]-1];record.flags=world_obj.flags;record.pos=list(world_obj.pos)
                self.hp.world.frame=self.w.frame;self.hp_phase='take_damage';self.hp_depth+=1;self.hp.depth+=1;self.calls_this_tick+=1
                try:self.hp.world.take_damage(self.hp.objects[victim],amount,None if attacker is None else self.hp.objects[attacker],flags)
                finally:
                    self.hp.depth-=1;self.hp_depth-=1
                    for record,entry in zip(self.hp.objects,self.hp_bindings):
                        world_obj=self.retained[entry[1]-1];world_obj.flags=record.flags;world_obj.pos=list(record.pos)
                    self.w.frame=self.hp.world.frame
                self.hp_phase='idle';self.hp_calls+=1
            return self.guard('take_damage',action)
        def import_pads(self):
            for target,source in zip(self.w.pads,self.network.pads):target.held,target.pressed,target.edge2=struct.unpack_from('<III',source,8)
        def publish_world(self):
            self.network.rand_seed=self.w.shared.state;self.network.sim_frame=self.w.frame
            for source,target in zip(self.w.pads,self.network.pads):struct.pack_into('<III',target,8,source.held&0xffffffff,source.pressed&0xffffffff,source.edge2&0xffffffff)
        def guard(self,label,fn):
            if self.failed:raise Refusal('latched')
            try:return fn()
            except Exception:
                if not self.failed:self.failed=True;self.failure_operation=label
                if self.depth==0 and self.phase=='idle':self.import_pads()
                self.publish_world()
                raise
        def scope(self,phase,fn):
            if not self.depth:self.import_pads()
            previous=self.phase;self.phase=phase;self.depth+=1
            try:
                result=fn()
                if self.failed:raise Refusal('latched')
                self.publish_world();self.phase=previous;return result
            finally:self.publish_world();self.depth-=1
        def with_world(self,fn):return self.guard('with_world',lambda:self.scope('world',fn))
        def effect(self,name,*values):self.events.append([name,*values,self.w.frame,self.network.net_frame,self.w.shared.state])
        def on_run(self,obj):
            self.effect('run',self.oid(obj));obj.tag+=1
            if scenario=='physical_unavailable':raise TaskFailure('run')
        def end_tick(self):
            self.effect('end')
            if scenario=='end_failure':raise TaskFailure('end')
        def task(self,fn):
            name=next(t[1] for t in TASKS if t[0]==fn);self.host_events.append(['task',name,self.network.net_frame,[]])
            self.effect(name)
            if name=='hp_pre':
                if scenario=='scalar_authority':self.a.pos[0]=core.from_int(7+self.ready_calls);self.a.flags=32|(0x80 if variant else 0)
                if scenario=='seed_domain':self.effect('shared_draw',self.w.shared.draw())
                if scenario in ('delete_before','reuse') and self.ready_calls==2:
                    self.w.destroy(self.b,3);self.effect('destroy',self.oid(self.b))
                    if scenario=='reuse':
                        replacement=self.w.new_object(0x400,self.pos(9,0,0));self.effect('replacement',self.oid(replacement),replacement.addr)
                    self.step_effects.append(copy.deepcopy(['after_destroy',self.raw_snapshot()]))
                if scenario=='pool_failure':
                    for _ in range(self.w.handles.count_free()-1):self.w.handles.add(self.w.always_draw,None)
            elif name=='hp_apply':
                amount=(0xbf800000,0)[variant] if scenario=='negative_zero' else 0x3f000001 if scenario=='ready' and variant else 0x40000000
                attacker=None if scenario=='delete_before' and self.ready_calls>=2 else 1
                # Explicit authored task input: one supplied held bit gates damage.
                if self.w.pads[0].held&1:self.take_damage(0,amount,attacker,0x40)
            else:
                if scenario=='delete_after' and self.ready_calls==1:
                    self.w.destroy(self.b,3);self.effect('destroy',self.oid(self.b));self.step_effects.append(copy.deepcopy(['after_destroy',self.raw_snapshot()]))
                if scenario=='post_failure':self.w.shared.draw();raise TaskFailure('post')
        def ready_tick(self,_):
            self.ready_calls+=1;self.calls_this_tick=0;self.object_run_completed=False
            def body():
                def invoke(fn):
                    self.task_priority=10 if fn==RUN else next(t[2] for t in TASKS if t[0]==fn)
                    try:
                        if fn==RUN:
                            self.host_events.append(['task','runObjects',self.network.net_frame,[]]);self.w.run_objects();self.object_run_completed=True
                        else:self.task(fn)
                    finally:self.task_priority=None
                self.w.tasks_run(1,invoke)
            return self.guard('task',lambda:self.scope('tasks',body))
        def submit(self,k,inbox):
            def action():
                self.started=True;sample=struct.pack('<H6B',1,128+k,64+3*k,255-k,17*k,31*k,47*k)
                return self.network.net_main(sample,yaw=k*1234,camera_flag=k&1,frame_step=4 if scenario=='hold' and k==0 else 1,no_sim=scenario=='no_sim' or (scenario=='hold' and k==0),hold_arg=scenario=='hold' and k>0,render_frame=k,inbox=inbox)
            return self.guard('submit',action)
        def session_snapshot(self):
            old=[['rta',config//4,bool(config&2),bool(config&1),2,0,1,self.combat_seed,0],self.started,self.failed,False,0,self.failure_operation,[],[],so.network_snapshot(self.network,config),True,[],[],[],self.combat_seed,[],self.host_events]
            return [old,self.world_snapshot(),[[RUN,'runObjects']]+[[t[0],t[1]] for t in TASKS],[[k,'authored_'+k,True] for k in so.BINDINGS],self.phase,self.depth,True]
        def adapter_snapshot(self):
            old=cs.Oracle.snapshot(self.hp)
            memory_limits=[1048576,65536,64,64]
            model_events=[]
            for event in self.hp.effects:
                if event[0]=='write':model_events.append([0,event[1:],["",[]]])
                else:model_events.append([1,[0,0,0],[event[1],event[2]]])
            model=[old[9],old[11],old[10],old[12],model_events,self.hp.model.draws,0,
                   [['handle_clear',True],['handle_link',True]],True]
            # The old serializer's complete field-ordered records/world plus
            # explicit empty owned_spells cover the new owned public tables.
            world=old[1]+[[]]
            return [[64,1024,64,memory_limits],self.hp.depth,self.hp.raw_depth,[],old[6],old[7],old[8],old[2],old[3],old[4],old[5],world,model]
        def snapshot(self):
            registry=self.registry_snapshot();raw=self.raw_snapshot()
            physical=[True,raw[1],registry,raw[2]] if raw[0] else None
            error='' if raw[0] else 'physical_unavailable'
            hp=[[64,64,[1048576,65536,64,64]],self.hp_bindings,self.hp_phase,self.hp_depth,self.calls_this_tick,
                self.adapter_snapshot(),[True,registry],physical,error,'',self.object_run_completed,self.task_priority]
            return [self.session_snapshot(),hp]
        def step_view(self):
            return [self.ready_calls,self.hp_calls,self.network.net_frame,self.w.frame,[o.hp for o in self.hp.objects],self.failed]
    return Oracle()


def snapshot_delta(before,after,path=()):
    """Lossless deterministic replacements, computed solely from actual values."""
    if before==after:return []
    if isinstance(before,(bytes,bytearray)) and isinstance(after,(bytes,bytearray)) and len(before)==len(after):
        result=[];i=0
        while i<len(before):
            if before[i]==after[i]:i+=1;continue
            start=i
            while i<len(before) and before[i]!=after[i]:i+=1
            result.append([list(path),start,bytes(after[start:i])])
        return result
    if isinstance(before,(list,tuple)) and isinstance(after,(list,tuple)) and len(before)==len(after):
        result=[]
        for i,(a,b) in enumerate(zip(before,after)):result.extend(snapshot_delta(a,b,path+(i,)))
        return result
    return [[list(path),copy.deepcopy(after)]]


def apply_delta(before,delta):
    value=copy.deepcopy(before)
    for entry in delta:
        path=entry[0]
        if len(entry)==3:
            target=value
            for index in path:target=target[index]
            changed=bytearray(target);changed[entry[1]:entry[1]+len(entry[2])]=entry[2];replacement=bytes(changed)
        else:replacement=entry[1]
        if not path:value=copy.deepcopy(replacement);continue
        target=value
        for index in path[:-1]:target=target[index]
        target[path[-1]]=copy.deepcopy(replacement)
    return value


def run_scenario(modules,helpers,config,variant,scenario):
    o=make_oracle(modules,helpers,config,variant,scenario);so=helpers['session_objects']
    try:
        packet=so.ready_packet(modules['netwire'],config);results=[]
        if scenario=='pending':o.network.level_pending=True
        initial=copy.deepcopy(o.snapshot());previous=initial
        for k in range(4):
            if scenario=='pending' and k==2:o.network.level_pending=False
            inbox=[packet] if (scenario=='delayed' and k==2) or (scenario not in ('delayed','no_ready') and k==0) else []
            try:
                datagrams,held,steps=o.submit(k,inbox);result=[[so.sent(x) for x in datagrams],held,steps]
            except (Refusal,TaskFailure,modules['core'].HandleError) as e:result=['error',type(e).__name__]
            current=copy.deepcopy(o.snapshot());delta=snapshot_delta(previous,current)
            if apply_delta(previous,delta)!=current:raise AssertionError('snapshot delta is not lossless')
            results.append([result,o.step_view(),delta]);previous=current
        return [initial,results,o.events,o.step_effects]
    finally:o.close()


def check_case(value,scenario,variant):
    """Independent outcome anchors stop a broken composition passing itself."""
    expected_calls={'ready':3,'delayed':2,'no_ready':0,'no_sim':0,'pending':2,'hold':3,
        'frozen':3,'scalar_authority':3,'negative_zero':3,'inert_ignore':3,
        'delete_after':1,'delete_before':3,'reuse':1,'pool_failure':0,'post_failure':1,
        'end_failure':1,'physical_unavailable':0,'seed_domain':3}
    summary=value[1][-1][1]
    if summary[1]!=expected_calls[scenario]:raise AssertionError('unexpected completed HP call count: '+scenario)
    if scenario=='ready' and variant==0:
        if [step[1][4][0] for step in value[1]]!=[0x42c80000,0x42c40000,0x42c00000,0x42bc0000]:
            raise AssertionError('ready HP must persist 100 -> 98 -> 96 -> 94')
    if scenario in ('no_ready','no_sim','inert_ignore') and summary[4][0]!=0x42c80000:
        raise AssertionError('blocked/inert branch changed HP')
    failed=scenario in ('delete_after','reuse','pool_failure','post_failure','end_failure','physical_unavailable')
    if summary[5]!=failed:raise AssertionError('unexpected failure state: '+scenario)
    state=copy.deepcopy(value[0]);states=[]
    for step in value[1]:state=apply_delta(state,step[2]);states.append(copy.deepcopy(state))
    if failed and states[-1]!=states[-2]:raise AssertionError('latched retry changed owned state')
    if scenario=='physical_unavailable' and (state[1][7] is not None or state[1][8]!='physical_unavailable'):
        raise AssertionError('failed iterator graph must remain explicitly unavailable')
    # Numeric state domains must never be conflated by this base-only route.
    old=state[0][0];adapter=state[1][5]
    if old[13] != ((1,0xdeadbeef)[variant]^0xa5a5a5a5) or old[14]:raise AssertionError('legacy combat RNG changed')
    if adapter[11][0]!=[0,[]] or adapter[11][1]!=[0,[]]:raise AssertionError('adapter RNG changed')
    if state[0][1][1]!=old[8][0][19]:raise AssertionError('World/shared and network seed diverged')


def generate(research_dir,output):
    modules=load_references(research_dir);helpers=load_helpers();so=helpers['session_objects'];output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=0
    with output.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as f:
        header=b'# session_hp_vectors_v1\n';f.write(header);vh.update(header)
        for name in SCENARIOS:
            for config in range(16) if name in FPU_SCENARIOS else (0,):
                for variant in VARIANTS[name]:
                    first_value=run_scenario(modules,helpers,config,variant,name);check_case(first_value,name,variant)
                    first=so.sc(first_value);second=so.sc(run_scenario(modules,helpers,config,variant,name))
                    if first!=second:raise AssertionError('fresh replay disagreement')
                    line=('\t'.join((name,str(config),f'{number};{variant}','ok',first))+'\n').encode('ascii');f.write(line);vh.update(line);th.update(line)
                    for b in line:fnv=((fnv^b)*1099511628211)&0xffffffffffffffff
                    counts[name]+=1;number+=1
    manifest={'schema_version':1,'suite':'session-hp','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'composition_policy':COMPOSITION_POLICY,'helpers':HELPERS,'ordering_evidence':ORDERING_EVIDENCE,'specification_evidence':SPECIFICATION_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'recipe_configurations':{n:list(range(16)) if n in FPU_SCENARIOS else [0] for n in SCENARIOS},'recipe_variants':{n:list(VARIANTS[n]) for n in SCENARIOS}}
    manifest['composition_contract']={'scope':'RTA ready-frame base take_damage only; no damage_apply, derived dispatch, original attack mapping or Norrath claim','oracle':'Five immutable SHA-verified original Python modules; SHA-verified authored serializer helpers only; no C++ expected-output source','frames':'Original net_main/tick/tasks_run with separately pinned observed pre-frame assignment; explicit post-runObjects task and decoded held-bit input','storage':'One original World pool; raw read-only overlay; explicit generation bindings; flags/position/frame aliases, persistent other HP backing; no implicit address reuse','rng':'World.shared is authoritative object/network domain; separate Session and Adapter combat/cosmetic streams remain inert in base HP','snapshots':'Complete initial state plus deterministic lossless list replacements and equal-length byte-span edits at every step; optional physical unavailable preserves semantic graph and backing; publication diagnostics explicit','errors':'Original model prefix retained; authored Session failure latch prevents retry; wrapper phase/lifetime/quiescence limits separately identified','determinism':'Each case two fresh original-model instances; explicit complete-state deltas reapplied and compared; numeric outcomes and inert RNG independently anchored'}
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8',newline='\n');return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_hp.tsv.gz'));a=p.parse_args();print(json.dumps(generate(a.research_dir,a.output),sort_keys=True))
if __name__=='__main__':main()
