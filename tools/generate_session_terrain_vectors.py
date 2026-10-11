#!/usr/bin/env python3
"""Independent original-model oracle for explicit singleton Session terrain tasks.

Supplied Level and task inputs are authored. Original immutable network, object,
terrain and optional HP bodies execute; no native output provides expectations.
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
REFERENCES={'combat': {'commit': '7212ecfecbeae8dd2015f6c82897226f49a94695', 'path': 'tools/ref_combat.py', 'sha256': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'}, 'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'}, 'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'tools/ref_creature.py', 'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'}, 'netwire': {'commit': '54258f9d7e281ef07f0675040e82e6264f9386cf', 'path': 'tools/ref_netwire.py', 'sha256': '4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'}, 'objects': {'commit': '0dd02ce416389dd440540119727d946636507703', 'path': 'tools/ref_objects.py', 'sha256': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'}, 'world': {'commit': '6831458ee7ee12bb166d1dce4eb8e4340095bc48', 'path': 'tools/ref_world.py', 'sha256': '30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8'}, 'world_info': {'commit': '2f937fca823488bc97b9680fde9d957c3fa9907a', 'path': 'tools/world_info.py', 'sha256': '2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5'}}
HELPERS={'session_objects': {'path': 'tools/generate_session_objects_vectors.py', 'sha256': '05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'}, 'session_hp': {'path': 'tools/generate_session_hp_vectors.py', 'sha256': '8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'}, 'combat_snapshot': {'path': 'tools/generate_combat_creature_vectors.py', 'sha256': 'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'}, 'world_queries': {'path': 'tools/generate_world_queries_vectors.py', 'sha256': '293626ad96ed54022dbc1bb30953ba0b0c91b562d70ddae53d40695cd263c1cf'}}
REFERENCE_SHA256=hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
COMPOSITION_POLICY='rta-session-terrain-v1'
ORDERING_EVIDENCE={'commit':'0dd02ce416389dd440540119727d946636507703','path':'docs/mainloop.md','sha256':'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}
SPECIFICATION_EVIDENCE={'commit':'b6881f52457bd715058248c34d65888c797ba657','path':'docs/spec/world-queries.md','sha256':'1527e9f32e315ce72a9a2706fe39d3d7c65fd6fc568eceb4ec0629fd695c35c2'}
CREATURE_SPECIFICATION_EVIDENCE={'commit':'a0d89502ce1431375a6fc045b91a73039114e6b9','path':'docs/spec/creature-tick.md','sha256':'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
MODELS=('ps2','ps2g1','chop','nearest')
NODE_BASE,NODE_COUNT,FREE_HEAD=0x847268,6096,0x864ea8
RUN=0x14b910
TASKS=((0xf2000001,'terrain_pre',20,0),(0xf2000002,'terrain_move',30,0),(0xf2000003,'terrain_post',40,0))
SCENARIOS=('ready','threshold','slide','grid','blocked','delayed_hold','frozen','hp','second_actor','destroy_reuse','full_pool','missing_grid','post_failure','end_failure','opposite')
FPU_SCENARIOS={'ready','threshold','slide','grid','delayed_hold','frozen','hp','opposite'}
VARIANTS={n:tuple(range(3 if n=='blocked' else 2 if n in {'ready','threshold','slide','grid','delayed_hold','hp','second_actor','destroy_reuse'} else 1))for n in SCENARIOS}
class Refusal(Exception):pass
class TaskFailure(Exception):pass
class Opaque:
    def __init__(self,raw):self.raw=raw


def load_helpers():
    root=pathlib.Path(__file__).resolve().parents[1];out={}
    for name,pin in HELPERS.items():
        path=root/pin['path'];source=path.read_bytes()
        if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('authored helper hash mismatch: '+name)
        module=types.ModuleType('session_terrain_helper_'+name);module.__file__=str(path)
        sys.modules[module.__name__]=module;exec(compile(source,str(path),'exec'),module.__dict__);out[name]=module
    return out


def load_references(research_dir):
    out={};saved={k:sys.modules.get(k)for k in ('ref_core','world_info')}
    try:
        for name in ('core','objects','netwire','combat','creature','world_info','world','ordering','specification','creature_specification'):
            pin={'ordering':ORDERING_EVIDENCE,'specification':SPECIFICATION_EVIDENCE,'creature_specification':CREATURE_SPECIFICATION_EVIDENCE}.get(name,REFERENCES.get(name))
            source=subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],capture_output=True,check=True,timeout=30).stdout
            if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('immutable reference hash mismatch: '+name)
            if name not in REFERENCES:continue
            module=types.ModuleType('session_terrain_pinned_'+name);module.__file__='/__verified_models__/ref_'+name+'.py';sys.modules[module.__name__]=module
            path=sys.path[:]
            try:exec(compile(source,module.__file__,'exec'),module.__dict__)
            finally:sys.path[:]=path
            out[name]=module
            if name=='core':sys.modules['ref_core']=module
            if name=='world_info':sys.modules['world_info']=module
    finally:
        for name,old in saved.items():
            if old is None:sys.modules.pop(name,None)
            else:sys.modules[name]=old
    return out


def make_level(w,helper,scenario,variant):
    level=helper.fixture(w,(3 if variant==0 else 4)if scenario=='slide' else 2)
    if scenario!='slide':
        level.heightfields[0].samples=[0]*25;level.heightfields[0].lo=level.heightfields[0].hi=0
        level.elements[1].z=0
    return level


def make_oracle(modules,helpers,config,variant,scenario,local=0):
    so,cs=helpers['session_objects'],helpers['combat_snapshot']
    core,objects,wire,combat,creature,world=(modules[k]for k in ('core','objects','netwire','combat','creature','world'))
    class Oracle(so.WorldOracle):
        def __init__(self):
            super().__init__(core,objects,config,min(variant,1))
            combat.set_fpu(MODELS[config//4],bool(config&2),bool(config&1));world.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
            self.config,self.scenario,self.local=config,scenario,local
            self.seed_initial=1 if variant==0 else 0xdeadbeef;self.combat_seed=self.seed_initial^0xa5a5a5a5
            self.w.shared.state=self.seed_initial;self.w.network_mode=1
            self.started=self.failed=False;self.failure_operation='';self.phase='idle';self.depth=0
            self.host_events=[];self.ready_calls=0;self.move_calls=0;self.hp_calls=0;self.hp_depth=0
            self.registry={};self.binding_history=[];self.hp_bindings=[];self.hp_phase='idle';self.known_cells={};self.object_run_completed=False;self.task_priority=None
            self.step_effects=[];self.attached=True;self.calls_this_tick=0
            self.terrain_phase='idle';self.terrain_depth=0;self.terrain_calls=0;self.last_velocity=None;self.last_result=None;self.publication_error='';owner=self
            class Machine(wire.Machine):
                def tick(machine,steps):
                    for _ in range(steps):
                        if not machine.level_pending:
                            owner.phase='pre_frame';owner.w.frame=machine.net_frame;machine.sim_frame=machine.net_frame;machine.rand_seed=owner.w.shared.state;owner.phase='idle'
                        wire.Machine.tick(machine,1)
            self.network=Machine(2,local,wire.Fpu(MODELS[config//4],bool(config&2),bool(config&1)))
            self.network.network_mode=1;self.network.num_players=2;self.network.run_tasks=self.ready_tick;self.w.frame=self.network.sim_frame
            self.w.on_run=self.on_run;self.w.on_destroy=lambda obj,flags:self.w.destroy(obj,flags)
            self.w.is_frozen=lambda:scenario=='frozen';self.w.res_find=lambda lump,name:0;self.w.end_of_tick=self.end_tick
            self.w.tasks.add(RUN,10,b'runObjects',0)
            for fn,name,priority,flags in TASKS:self.w.tasks.add(fn,priority,name.encode(),flags)
            self.a=self.w.new_object(0x400,self.pos(255 if scenario=='grid' and variant==0 else 1,min(variant,1),0 if scenario=='ready' and variant else 12));self.a.flags=32;self.a.tag=0
            self.a.vz=0;self.w.run_every_frame(self.a)
            if scenario=='grid' and variant==1:self.w.add_to_always_draw(self.a)
            self.level=make_level(world,helpers['world_queries'],scenario,variant)
            self.terrain_binding=[1,self.oid(self.a),self.a.addr]
            self.hp_enabled=scenario=='hp';self.hp=None
            if self.hp_enabled:self.init_hp()
            self.publish_world()
        def init_hp(self):
            owner=self
            self.hp=types.SimpleNamespace(results=[],world=combat.World(0,0),objects=[],spells=[],bindings=[],regions=[],callbacks=[['handle_clear','session.raw_handles.clear'],['handle_link','session.raw_handles.link']],effects=[],pokes=[],depth=0,raw_depth=0)
            self.hp.ident=lambda obj:None if obj is None else self.hp.objects.index(obj)
            self.hp.address=lambda obj:0 if obj is None else self.hp.bindings[self.hp.ident(obj)][0]
            class Memory(creature.Memory):
                def load(mem,address,size):return sum((owner.overlay_byte(address+i)if owner.covered(address+i)else super(Memory,mem).load(address+i,1))<<(8*i)for i in range(size))
                def poke(mem,address,size,value):
                    if any(owner.covered(address+i)for i in range(size)):raise Refusal('readonly')
                    return super().poke(address,size,value)
            class Model(creature.Model):
                def _w(model,address,size,value):
                    owner.hp.effects.append(['write',address,size,value&((1<<(8*size))-1)]);return super()._w(address,size,value)
                def call(model,name,*args):
                    owner.hp.effects.append(['call',name,list(args)]);return super().call(name,*args)
            self.hp.model=Model(Memory());self.hp.model.cb={'handle_clear':self.clear,'handle_link':self.link};self.hp.world.take_damage_fn=self.dispatch
            self.bind_hp(self.a)
            for off,size,value in ((0xc8,4,0),(0x231,1,0),(0x2dc,4,0),(0x2e0,2,0),(0x304,4,0),(0x3b0,2,0)):self.hp.model.mem.poke(self.a.addr+off,size,value)
            self.project_hp()
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
        def on_run(self,obj):self.effect('run',self.oid(obj));obj.tag+=1
        def end_tick(self):
            self.effect('end')
            if scenario=='end_failure':raise TaskFailure('end')
        def import_hp_scalars(self):
            if not self.hp_enabled:return
            for record,entry in zip(self.hp.objects,self.hp_bindings):
                obj=self.retained[entry[1]-1];record.flags=obj.flags;record.pos=list(obj.pos)
            self.hp.world.frame=self.w.frame;self.project_hp()
        def move(self,velocity):
            def action():
                if self.phase!='tasks' or self.depth!=1 or not self.object_run_completed or self.task_priority is None or self.task_priority<=10 or self.terrain_calls:raise Refusal('phase')
                alive=[o for o in self.retained if o.alive]
                if len(alive)!=1 or alive[0]is not self.a or self.w.objects.get(self.a.addr)is not self.a:raise Refusal('singleton')
                self.terrain_calls+=1;self.last_velocity=list(velocity);self.last_result=None;self.terrain_phase='terrain_query';self.terrain_depth=1
                try:
                    position,vz,bits=world.object_move(self.level,self.a.radius,self.a.pos,velocity,self.a.vz,self.a.flags)
                    self.a.pos=list(position);self.a.vz=vz;self.last_result=[list(position),vz,bits];self.terrain_phase='grid_update'
                    try:self.w.update_grid_cell(self.a)
                    except Exception:
                        self.import_hp_scalars();raise
                    if self.hp_enabled:self.terrain_phase='hp_publish';self.import_hp_scalars()
                    self.terrain_phase='idle';self.move_calls+=1
                    return copy.deepcopy(self.last_result)
                finally:self.terrain_depth=0
            return self.guard('move_isolated',action)
        def velocity(self):
            if scenario=='threshold':return [0 if variant==0 else 0x3f000001,0,0]
            if scenario=='slide':return self.pos(120 if variant==0 else -120,30,0)
            if scenario=='grid':return self.pos(120,0,0)
            return [core.from_int(30) if variant==0 else 0x3f000001,core.from_int(-7),0]
        def task(self,fn):
            name=next(t[1]for t in TASKS if t[0]==fn);self.host_events.append(['task',name,self.network.net_frame,[]]);self.effect(name)
            if name=='terrain_pre':
                if scenario=='second_actor' and self.ready_calls==1:
                    self.b=self.w.new_object(0x400,self.pos(2,3,0))
                    if variant:self.w.move_off_grid(self.b)
                    self.effect('second',self.oid(self.b))
                if scenario=='destroy_reuse' and self.ready_calls==2:
                    self.w.destroy(self.a,3);self.effect('destroy',self.oid(self.a))
                    if variant:
                        replacement=self.w.new_object(0x400,self.pos(9,0,0));self.effect('replacement',self.oid(replacement),replacement.addr)
                if scenario=='full_pool' and self.ready_calls==1:
                    for _ in range(self.w.handles.count_free()):self.w.handles.add(self.w.always_draw,None)
                if scenario=='missing_grid' and self.ready_calls==1:self.w.handles.clear(self.a.heads[2])
                if self.hp_enabled and variant==0:self.take_damage(0,0x40000000,None,0x40)
            elif name=='terrain_move':self.step_effects.append(['move',self.move(self.velocity())])
            else:
                if self.hp_enabled and variant==1:self.take_damage(0,0x40000000,None,0x40)
                if scenario=='post_failure':self.w.shared.draw();raise TaskFailure('post')
        def ready_tick(self,_):
            self.ready_calls+=1;self.calls_this_tick=0;self.terrain_calls=0;self.object_run_completed=False
            def body():
                def invoke(fn):
                    self.task_priority=10 if fn==RUN else next(t[2]for t in TASKS if t[0]==fn)
                    try:
                        if fn==RUN:
                            self.host_events.append(['task','runObjects',self.network.net_frame,[]]);self.w.run_objects();self.object_run_completed=True
                        else:self.task(fn)
                    finally:self.task_priority=None
                self.w.tasks_run(1,invoke)
            return self.guard('task',lambda:self.scope('tasks',body))
        def submit(self,k,inbox):
            def action():
                self.started=True;sample=struct.pack('<H6B',1,*[x&255 for x in (128+k,64+3*k,255-k,17*k,31*k,47*k)])
                hold=scenario=='delayed_hold' and variant==1
                return self.network.net_main(sample,yaw=k*1234,camera_flag=k&1,frame_step=4 if hold and k==0 else 1,no_sim=(scenario=='blocked' and variant==2)or(hold and k==0),hold_arg=hold and k>0,render_frame=k,inbox=inbox)
            return self.guard('submit',action)
        def session_snapshot(self):
            old=[['rta',config//4,bool(config&2),bool(config&1),2,local,1,self.combat_seed,0],self.started,self.failed,False,0,self.failure_operation,[],[],so.network_snapshot(self.network,config),True,[],[],[],self.combat_seed,[],self.host_events]
            return [old,self.world_snapshot(),[[RUN,'runObjects']]+[[t[0],t[1]]for t in TASKS],[[k,'authored_'+k,True]for k in so.BINDINGS],self.phase,self.depth,True]
        def adapter_snapshot(self):
            old=cs.Oracle.snapshot(self.hp);model_events=[]
            for event in self.hp.effects:model_events.append([0,event[1:],['',[]]]if event[0]=='write'else[1,[0,0,0],[event[1],event[2]]])
            model=[old[9],old[11],old[10],old[12],model_events,self.hp.model.draws,0,[['handle_clear',True],['handle_link',True]],True]
            return [[64,1024,64,[1048576,65536,64,64]],self.hp.depth,self.hp.raw_depth,[],old[6],old[7],old[8],old[2],old[3],old[4],old[5],old[1]+[[],None],model]
        def snapshot(self):
            hp=None
            if self.hp_enabled:
                registry=self.registry_snapshot();raw=self.raw_snapshot();physical=[True,raw[1],registry,raw[2]]if raw[0]else None
                hp=[[64,64,[1048576,65536,64,64]],self.hp_bindings,self.hp_phase,self.hp_depth,self.calls_this_tick,self.adapter_snapshot(),[True,registry],physical,''if raw[0]else'physical_unavailable','',self.object_run_completed,self.task_priority,None]
            terrain=[helpers['world_queries'].snapshot(self.level),self.terrain_binding,COMPOSITION_POLICY,self.terrain_phase,self.publication_error,self.terrain_depth,self.terrain_calls,self.last_velocity,self.last_result,self.object_run_completed,self.task_priority]
            return [[self.session_snapshot(),hp],terrain]
        def step_view(self):return [self.ready_calls,self.move_calls,self.network.net_frame,self.w.frame,list(self.a.pos),self.a.vz,self.failed,self.w.shared.state,[x.hp for x in self.hp.objects]if self.hp_enabled else []]
    return Oracle()


def capture(o,helpers,k,inbox,previous):
    hp,so=helpers['session_hp'],helpers['session_objects']
    try:
        datagrams,held,steps=o.submit(k,inbox);result=[[so.sent(x)for x in datagrams],held,steps]
    except (Refusal,TaskFailure,o.r.HandleError,o.o.ObjectError)as e:datagrams=[];result=['error',type(e).__name__]
    current=copy.deepcopy(o.snapshot());delta=hp.snapshot_delta(previous,current)
    if hp.apply_delta(previous,delta)!=current:raise AssertionError('lossless snapshot reconstruction failed')
    return [result,o.step_view(),delta],current,datagrams


def run_scenario(modules,helpers,config,variant,scenario):
    if scenario=='opposite':return run_opposite(modules,helpers,config)
    o=make_oracle(modules,helpers,config,variant,scenario)
    try:
        if scenario=='blocked' and variant==1:o.network.level_pending=True
        initial=copy.deepcopy(o.snapshot());previous=initial;steps=[];packet=helpers['session_objects'].ready_packet(modules['netwire'],config)
        for k in range(4):
            if scenario=='blocked' and variant==1 and k==2:o.network.level_pending=False
            delayed=scenario=='delayed_hold' and variant==0;no_ready=scenario=='blocked' and variant==0
            inbox=[packet]if(delayed and k==2)or(not delayed and not no_ready and k==0)else[]
            step,previous,_=capture(o,helpers,k,inbox,previous);steps.append(step)
        return [initial,steps,o.events,o.step_effects]
    finally:o.close()


def run_opposite(modules,helpers,config):
    peers=[];instrumentation=[]
    for i in (0,1):
        o=make_oracle(modules,helpers,config,0,'opposite',i);peers.append(o);instrumentation.append((modules['objects'].GameObject,modules['core'].Iterator));o.close()
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
            if o.ready_calls>old:completed[which][o.network.net_frame]=[copy.deepcopy(o.world_snapshot()),copy.deepcopy(state[1])]
            steps.append([which,step])
        common=sorted(set(completed[0])&set(completed[1]));equal=[[frame,completed[0][frame]==completed[1][frame]]for frame in common]
        return [initial,steps,[o.events for o in peers],[queues,equal]]
    finally:
        for o in peers:o.close()


def check_case(value,scenario,variant,helpers):
    if scenario=='opposite':
        if not value[3][1]or not all(x[1]for x in value[3][1]):raise AssertionError('opposite completed states differ')
        return
    hp=helpers['session_hp'];state=copy.deepcopy(value[0]);states=[]
    for step in value[1]:state=hp.apply_delta(state,step[2]);states.append(copy.deepcopy(state))
    expected={'ready':3,'threshold':3,'slide':3,'grid':3,'blocked':2 if variant==1 else 0,'delayed_hold':2 if variant==0 else 3,'frozen':3,'hp':3,'second_actor':0,'destroy_reuse':1,'full_pool':3,'missing_grid':0,'post_failure':1,'end_failure':1}
    if value[1][-1][1][1]!=expected[scenario]:raise AssertionError(('unexpected movement count',scenario,variant,value[1][-1][1]))
    failed=scenario in {'second_actor','destroy_reuse','missing_grid','post_failure','end_failure'}
    if bool(value[1][-1][1][6])!=failed:raise AssertionError('failure state mismatch')
    if failed and states[-1]!=states[-2]:raise AssertionError('latched retry changed state')
    session=state[0][0];old=session[0]
    if old[13]!=((1 if variant==0 else 0xdeadbeef)^0xa5a5a5a5)or old[14]:raise AssertionError('legacy RNG changed')
    if session[1][1]!=old[8][0][19]:raise AssertionError('shared network domain mismatch')
    if scenario=='ready' and variant and any(step[1][4][2]!=0 or step[1][5]!=0 for step in value[1]):raise AssertionError('grounded movement lost height/vz clamp')
    if scenario=='hp' and value[1][-1][1][8]!=[0x42bc0000]:raise AssertionError('HP persistence expected100to94')
    if scenario in {'second_actor','blocked'}and expected[scenario]==0 and state[1][0]!=value[0][1][0]:raise AssertionError('refused/blocked query changed terrain')


def generate(research_dir,output):
    modules=load_references(research_dir);helpers=load_helpers();so=helpers['session_objects'];output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=0;size=0;longest=0
    with output.open('wb')as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0)as f:
        header=b'# session_terrain_vectors_v1\n';f.write(header);vh.update(header);size+=len(header)
        for name in SCENARIOS:
            for config in range(16)if name in FPU_SCENARIOS else(0,):
                for variant in VARIANTS[name]:
                    value=run_scenario(modules,helpers,config,variant,name);check_case(value,name,variant,helpers)
                    first=so.sc(value);second=so.sc(run_scenario(modules,helpers,config,variant,name))
                    if first!=second:raise AssertionError('two fresh executions differ')
                    line=('\t'.join((name,str(config),f'{number};{variant}','ok',first))+'\n').encode('ascii');f.write(line);vh.update(line);th.update(line);size+=len(line);longest=max(longest,len(line))
                    for byte in line:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[name]+=1;number+=1
    manifest={'schema_version':1,'suite':'session-terrain','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'composition_policy':COMPOSITION_POLICY,'helpers':HELPERS,'ordering_evidence':ORDERING_EVIDENCE,'specification_evidence':SPECIFICATION_EVIDENCE,'creature_specification_evidence':CREATURE_SPECIFICATION_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'recipe_configurations':{n:list(range(16))if n in FPU_SCENARIOS else[0]for n in SCENARIOS},'recipe_variants':{n:list(VARIANTS[n])for n in SCENARIOS},'expanded_bytes':size,'max_line_bytes':longest}
    manifest['composition_contract']={'scope':'RTA one-live-actor supplied post-fixup terrain movement only; no Player_move, dynamic contacts, original pad mapping, terrain file parsing or Norrath','oracle':'Seven immutable SHA-verified original models; pinned authored fixture/snapshot/delta helpers; original object_move/update_grid_cell/net_main/tasks_run bodies execute','frames':'Explicit post-runObjects task; observed pre-frame assignment evidence separate from authored velocity and singleton policy; real opposite endpoint packets','state':'Complete initial Session/World/optional HP and Level plus owned lossless deltas reconstructed on every step; existing base/shared optionals explicitly absent','errors':'No rollback; original query/grid and callback prefixes plus declared wrapper isolation/phase/latch; native-only malformed topology/work budget guards not simulated','rng':'Movement draws none; original object constructor uses World.shared; Session/Adapter streams separately observed and inert','determinism':'Each case twice with fresh instances; separate process regeneration supported; deterministic gzip'}
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8',newline='\n');return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_terrain.tsv.gz'));a=p.parse_args();print(json.dumps(generate(a.research_dir,a.output),sort_keys=True))
if __name__=='__main__':main()
