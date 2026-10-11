#!/usr/bin/env python3
"""Independent authored controls over immutable RTA model bodies; no native output inputs."""
import argparse,collections,copy,gzip,hashlib,importlib.util,json,os,pathlib,struct,sys,tempfile,types
sys.dont_write_bytecode=True
HELPER_PIN={'path':'tools/generate_session_terrain_vectors.py','sha256':'dfffb7ad22bacf5d62b2c3ed30617af6fcd3298982724fa5ced0ae9be01b527b'}
ROOT=pathlib.Path(__file__).resolve().parents[1]
def load_base():
    path=ROOT/HELPER_PIN['path'];data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=HELPER_PIN['sha256']:raise ValueError('Session terrain helper hash mismatch')
    spec=importlib.util.spec_from_file_location('session_control_pinned_helper',path);m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m);return m
BASE=load_base()
REFERENCES=BASE.REFERENCES
REFERENCE_SHA256=BASE.REFERENCE_SHA256
HELPERS={**BASE.HELPERS,'session_terrain':HELPER_PIN}
MODELS=BASE.MODELS
RUN=BASE.RUN
NODE_BASE,NODE_COUNT,FREE_HEAD=BASE.NODE_BASE,BASE.NODE_COUNT,BASE.FREE_HEAD
TASKS=((0xf4000001,'authored_control',20,0),)
Refusal,TaskFailure,Opaque=BASE.Refusal,BASE.TaskFailure,BASE.Opaque
COMPOSITION_POLICY='rta-session-control-v1'
def default_level(w):
    d=w.Level();d.tg_x0=d.tg_y0=-2;d.tg_w=d.tg_h=4;d.grid=[[0]for _ in range(16)]
    hf=w.HeightField(-64,-64,129,129,0,0,[0]*(129*129));d.heightfields=[hf]
    d.elements=[w.Element(0,-256,-256,256,256,hf,0,0,0,0,32767,0)]
    return d

def make_oracle(modules,helpers,config,variant,scenario,local=0):
    so,cs=helpers['session_objects'],helpers['combat_snapshot']
    core,objects,wire,combat,creature,world=(modules[k]for k in ('core','objects','netwire','combat','creature','world'))
    class Oracle(so.WorldOracle):
        def __init__(self):
            super().__init__(core,objects,config,min(variant,1))
            combat.set_fpu(MODELS[config//4],bool(config&2),bool(config&1));world.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
            self.config,self.scenario,self.local=config,scenario,local
            self.seed_initial=17;self.combat_seed=0
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
            self.network=Machine(1,0,wire.Fpu(MODELS[config//4],bool(config&2),bool(config&1)))
            self.network.network_mode=1;self.network.num_players=1;self.network.run_tasks=self.ready_tick;self.w.frame=self.network.sim_frame
            self.w.tasks.add(RUN,10,b'runObjects',0)
            self.a=self.w.new_object(0x400,self.pos(0,0,0));self.a.radius=0x40800000;self.a.height=0x41000000
            self.w.run_every_frame(self.a)
            self.level=default_level(world)
            self.terrain_binding=[1,self.oid(self.a),self.a.addr]
            self.hp_enabled=True;self.hp=None;self.init_hp()
            for fn,name,priority,flags in TASKS:self.w.tasks.add(fn,priority,name.encode(),flags)
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
            self.project_hp()
        def oid(self,obj):
            return -(obj.raw+1) if isinstance(obj,Opaque) else super().oid(obj)
        def world_snapshot(self):
            result=super().world_snapshot()
            for row in result[5]:
                raw=-row[5]-1 if row[5]<0 else 0
                if raw:row[5]=0
                row.append(raw)
            result[-2]=1;result[-1]=[False]*5
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
        def task(self,fn):
            self.host_events.append(['task','authored_control',self.network.net_frame,[]])
            self.control.ready()
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
        def submit(self,mask):
            def action():
                self.started=True
                return self.network.net_main(struct.pack('<H6B',mask,128,128,128,128,0,0))
            return self.guard('submit',action)
        def session_snapshot(self):
            old=[['rta',config//4,bool(config&2),bool(config&1),1,0,1,self.combat_seed,17],self.started,self.failed,False,0,self.failure_operation,[],[],so.network_snapshot(self.network,config),True,[],[],[],self.combat_seed,[],self.host_events]
            return [old,self.world_snapshot(),[[RUN,'runObjects']]+[[t[0],t[1]]for t in TASKS],[[k,'',False]for k in so.BINDINGS],self.phase,self.depth,True]
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
            terrain=[helpers['world_queries'].snapshot(self.level),self.terrain_binding,'rta-session-terrain-v1',self.terrain_phase,self.publication_error,self.terrain_depth,self.terrain_calls,self.last_velocity,self.last_result,self.object_run_completed,self.task_priority]
            return [[self.session_snapshot(),hp],terrain]
        def step_view(self):return [self.ready_calls,self.move_calls,self.network.net_frame,self.w.frame,list(self.a.pos),self.a.vz,self.failed,self.w.shared.state,[x.hp for x in self.hp.objects]if self.hp_enabled else []]
    return Oracle()

SCENARIOS=('movement','edges','pause','focus','reset','limit','stop')
FPU_SCENARIOS={'movement','edges'}
VARIANTS={'movement':(0,1,2,3),'edges':(0,1),'pause':(0,),'focus':(0,),'reset':(0,),'limit':(0,),'stop':(0,)}

def commands(name,variant):
    if name=='movement':
        mask=(1,2,9,15)[variant];return [('advance',mask),('advance',mask),('advance',0),('advance',0)]
    if name=='edges':return [('advance',17 if variant==0 else 16),('advance',17 if variant==0 else 16),('advance',0),('advance',16),('advance',0)]
    if name=='pause':return [('advance',17),('pause',1),('advance',31),('advance',31),('pause',0),('advance',0),('advance',16),('advance',0)]
    if name=='focus':return [('advance',17),('release',0),('advance',0),('advance',18),('advance',0)]
    if name=='reset':return [('advance',17),('advance',0),('reset',0),('advance',16),('advance',0)]
    if name=='limit':return [('advance',17),('advance',17),('advance',17),('advance',0),('release',0),('pause',1)]
    if name=='stop':return [('advance',17),('advance',0),('stop',0),('advance',31),('release',0),('reset',0)]
    raise ValueError('unknown scenario')

class Control:
    def __init__(self,modules,helpers,config,name):
        self.modules,self.helpers,self.config,self.name=modules,helpers,config,name
        self.max_ready=2 if name=='limit' else 2000
        self.o=None;self.reset()
    def reset(self):
        if self.o is not None:self.o.close()
        self.o=make_oracle(self.modules,self.helpers,self.config,0,self.name);self.o.control=self
        self.initial_terrain=copy.deepcopy(self.helpers['world_queries'].snapshot(self.o.level))
        self.held=0;self.pending=[];self.cutoff=1;self.submissions=self.ticks=self.hits=0
        self.paused=self.stopped=self.failed=self.busy=False;self.phase='idle';self.error=''
    def ready(self):
        self.phase='task';self.ticks+=1
        frame,held=self.o.with_world(lambda:(self.o.w.frame,self.o.w.pads[0].held))
        if frame<self.cutoff:held=0
        def axis(p,n):return 0 if bool(held&p)==bool(held&n)else 0x42700000 if held&p else 0xc2700000
        self.o.move([axis(1,2),axis(4,8),0])
        damage=any(x==frame and frame>=self.cutoff and held&16 for x in self.pending)
        self.pending=[x for x in self.pending if x>frame]
        if damage:self.o.take_damage(0,0x40000000,None,0);self.hits+=1
        self.phase='submit'
    def release(self):
        if not self.stopped:self.held=0;self.pending=[];self.cutoff=self.o.network.lif
    def action(self,action,value):
        if action=='reset':self.reset()
        elif action=='release':self.release()
        elif action=='pause':
            if not self.stopped:
                if value and not self.paused:self.release()
                self.paused=bool(value)
        elif action=='stop':
            if not self.stopped:self.release();self.stopped=True;self.error='stopped'
        elif action=='advance':
            if not self.paused and not self.stopped:
                self.busy=True;self.phase='submit'
                if value&16 and not self.held&16:self.pending.append(self.o.network.lif)
                self.held=value;self.submissions+=1
                self.o.submit(value)
                if self.ticks>=self.max_ready:self.stopped=True;self.error='ready_tick_limit'
                elif self.submissions>=8008:self.stopped=True;self.error='submission_limit'
                self.phase='idle';self.busy=False
        else:raise ValueError(action)
        return self.observation()
    def observation(self):return [list(self.o.a.pos),self.o.a.vz,self.o.a.radius,self.o.hp.objects[0].hp,self.o.w.frame,self.submissions,self.ticks,self.hits,self.paused,self.stopped,self.failed or self.o.failed,self.error]
    def snapshot(self):return [['rta',self.config//4,bool(self.config&2),bool(self.config&1),17,self.max_ready],self.initial_terrain,False,self.o.snapshot(),self.o.oid(self.o.a),self.o.a.addr,1,1,self.held,list(self.pending),self.cutoff,self.submissions,self.ticks,self.hits,self.paused,self.stopped,self.failed,self.busy,self.phase,self.error]
    def close(self):self.o.close()

def run_scenario(modules,helpers,config,variant,name):
    c=Control(modules,helpers,config,name)
    try:
        initial=copy.deepcopy(c.snapshot());before=initial;steps=[];hp=helpers['session_hp']
        for action,value in commands(name,variant):
            observation=c.action(action,value);after=copy.deepcopy(c.snapshot());delta=hp.snapshot_delta(before,after)
            if hp.apply_delta(before,delta)!=after:raise AssertionError('lossless control delta failed')
            steps.append([[action,value],copy.deepcopy(observation),delta]);before=after
        if name=='edges' and c.hits!=2:raise AssertionError('held/repressed damage count')
        if name=='edges' and c.o.hp.objects[0].hp!=0x42c00000:raise AssertionError('100 ->98 ->96')
        if name=='movement' and variant==0 and steps[1][1][0]!=[0x3f7fffff,0,0]:raise AssertionError('literal movement anchor')
        if name=='movement' and variant==3 and c.o.a.pos!=[0,0,0]:raise AssertionError('opposite directions cancel')
        if c.o.w.shared.state!=initial[3][0][0][1][1]or c.o.hp.world.seed.draws:raise AssertionError('control RNG changed')
        return [initial,steps]
    finally:c.close()

def generate(research_dir,output):
    modules=BASE.load_references(research_dir);helpers=BASE.load_helpers();so=helpers['session_objects'];output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=0;size=0;longest=0
    manifest_path=output.with_name(output.name[:-7]+'.json')
    fd,name=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent)
    temp=pathlib.Path(name);manifest_temp=None
    try:
        with os.fdopen(fd,'wb')as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0)as f:
                header=b'# session_control_vectors_v1\n';f.write(header);vh.update(header);size+=len(header)
                for name in SCENARIOS:
                    for config in range(16)if name in FPU_SCENARIOS else(0,):
                        for variant in VARIANTS[name]:
                            first=so.sc(run_scenario(modules,helpers,config,variant,name));second=so.sc(run_scenario(modules,helpers,config,variant,name))
                            if first!=second:raise AssertionError('two fresh original-model executions differ')
                            line=('\t'.join((name,str(config),f'{number};{variant}','ok',first))+'\n').encode('ascii');f.write(line);vh.update(line);th.update(line);size+=len(line);longest=max(longest,len(line))
                            for byte in line:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                            counts[name]+=1;number+=1
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'session-control','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'composition_policy':COMPOSITION_POLICY,'helpers':HELPERS,'ordering_evidence':BASE.ORDERING_EVIDENCE,'specification_evidence':BASE.SPECIFICATION_EVIDENCE,'creature_specification_evidence':BASE.CREATURE_SPECIFICATION_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'recipe_configurations':{n:list(range(16))if n in FPU_SCENARIOS else[0]for n in SCENARIOS},'recipe_variants':{n:list(VARIANTS[n])for n in SCENARIOS},'expanded_bytes':size,'max_line_bytes':longest,'composition_contract':{'scope':'Authored single-peer control policy over original immutable net_main, tasks_run, object_move and take_damage. Not original Player_move or playable game.','state':'Every public Controller snapshot field plus complete Session/World/Level/HP; lossless deltas reapplied on every action.','input':'Actual Machine(1,0), no fabricated ready packets. Rising damage edges keyed to original input frame, pause/focus explicit cutoff.','rng':'No control/move/baseHP draws; constructor shared state preserved separately from dormant seeds.','determinism':'Every case twice fresh; deterministic gzip; separate-process regeneration supported.'}}
        fd2,name=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent)
        manifest_temp=pathlib.Path(name)
        with os.fdopen(fd2,'w',encoding='utf-8',newline='\n')as f:
            json.dump(manifest,f,indent=2,sort_keys=True);f.write('\n')
            f.flush();os.fsync(f.fileno())
        # Each artifact is complete before replacement. Publish the manifest last
        # so an interrupted second replacement remains detectably stale.
        os.replace(temp,output)
        os.replace(manifest_temp,manifest_path)
        return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_control.tsv.gz'));a=p.parse_args();print(json.dumps(generate(a.research_dir,a.output),sort_keys=True))
if __name__=='__main__':main()
