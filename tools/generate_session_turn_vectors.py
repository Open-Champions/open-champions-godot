#!/usr/bin/env python3
"""Original-model expectations for an explicitly authored ready-task turning gate.

Integer/list input contains all actor metadata, task commands and submitted packets.
Only composition policy is authored; turning, scheduler, wire, pool and HP operations
run immutable original Python bodies. There is no native-output oracle input.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import types

HELPERS = {
 'session_hp': {'path':'tools/generate_session_hp_vectors.py','sha256':'8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'},
 'session_objects': {'path':'tools/generate_session_objects_vectors.py','sha256':'05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'},
 'combat_snapshot': {'path':'tools/generate_combat_creature_vectors.py','sha256':'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
}
REFERENCES = {
 'core': {'commit':'41e847af5360724ab62d208594f8027ea3c1b686','path':'tools/ref_core.py','sha256':'0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
 'objects': {'commit':'0dd02ce416389dd440540119727d946636507703','path':'tools/ref_objects.py','sha256':'541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'},
 'netwire': {'commit':'54258f9d7e281ef07f0675040e82e6264f9386cf','path':'tools/ref_netwire.py','sha256':'4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'},
 'combat': {'commit':'7212ecfecbeae8dd2015f6c82897226f49a94695','path':'tools/ref_combat.py','sha256':'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
 'creature': {'commit':'a0d89502ce1431375a6fc045b91a73039114e6b9','path':'tools/ref_creature.py','sha256':'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256=hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
SPECIFICATION_EVIDENCE={'commit':'a0d89502ce1431375a6fc045b91a73039114e6b9','path':'docs/spec/creature-tick.md','sha256':'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
ORDERING_EVIDENCE={'commit':'0dd02ce416389dd440540119727d946636507703','path':'docs/mainloop.md','sha256':'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}
COMPOSITION_POLICY='rta-session-turn-v1'
MODELS=('ps2','ps2g1','chop','nearest')
SCENARIOS=('ready','opposite_mode','other_mode','slow_one','slow_sixty','slow_bypass','negative_rate',
 'raw_rate','velocity_wrap','world_authority','health_before','health_after','two_bindings','frozen',
 'delayed','held','no_ready','pending','no_sim','paused','retired','reused','twice','post_failure')
FPU_SCENARIOS=set(SCENARIOS)-{'no_ready','pending','no_sim','paused','retired','reused','twice','post_failure'}
CONFIGURATIONS=tuple(range(16))
LIMITS=[64,64]
INPUT_PROTOCOL={
 'version':1,'columns':['operation','config','id;explicit-input-DSL','status','observation'],
 'setup':'[network[peers,local,shared_seed,combat_seed,network_seed],frozen,actors[[posRaw3,flags,angle,hp,cflags,status5,runEveryFrame]],turnLimits2,bindings[[HpId,rateRaw,av16,slow8]],setupWrites[[HpId,offset,size,value]],programsByReadyTick[[pre,apply,post]],actions]',
 'factory':'Exactly two input actors in table order, extent0x400/tag0; original World allocation/grid placement. Bind HP in table order. Six unlogged zero spans (c8,4),(231,1),(2dc,4),(2e0,2),(304,4),(3b0,2) per actor; then input setupWrites before turning bindings. Fixed task IDs f1000001..3 hp_pre/hp_apply/hp_post priority20/30/40.',
 'commands':'turn[0,id,mode,pointRaw3]; null-attacker HP[1,id,amountRaw,flags]; World scalar assignment[2,id,angle,posRaw3]; destroy[3,id]; recreate[4,posRaw3] using new0x400/flags32/tag0; authored failure[5]',
 'actions':'no-submit[0]; submit[1,sample8bytes,yaw,camera,frame_step,no_sim,hold,render,pending,inboxByteLists]',
 'observation':'[input,initial_full_snapshot,[[result,lossless_state_delta],...],authored_effects]',
 'snapshot':'[complete prior Session/World/HP/Terrain base fields,TurningSnapshot20 in declaration order]; no creature-movement or animation extension admitted',
 'effects':'run/end/hp task labels use existing helper [name,args...,worldframe,netframe,sharedseed]; completed commands append [command,submittedCommand,result,worldframe]',
 'errors':'Refusal=authored Session preflight/latch; TaskFailure=explicit supplied later task failure. Original finite turning remains unmodified and successful; native budget failures are separate API tests.'}

def load_helpers():
    root=pathlib.Path(__file__).resolve().parents[1];out={}
    for name,pin in HELPERS.items():
        path=root/pin['path'];source=path.read_bytes()
        if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('helper hash mismatch: '+name)
        m=types.ModuleType('session_turn_helper_'+name);m.__file__=str(path);sys.modules[m.__name__]=m
        exec(compile(source,str(path),'exec'),m.__dict__);out[name]=m
    return out

def load_references(research_dir,scratch_dir=None):
    del scratch_dir
    result={};previous=sys.modules.get('ref_core')
    try:
        for name,pin in list(REFERENCES.items())+[('specification',SPECIFICATION_EVIDENCE),('ordering',ORDERING_EVIDENCE)]:
            source=subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],capture_output=True,check=True,timeout=30).stdout
            if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('reference hash mismatch: '+name)
            if name in ('specification','ordering'):continue
            m=types.ModuleType('session_turn_pinned_'+name);m.__file__='/__verified_models__/ref_'+name+'.py';sys.modules[m.__name__]=m
            exec(compile(source,m.__file__,'exec'),m.__dict__);result[name]=m
            if name=='core':sys.modules['ref_core']=m
    finally:
        if previous is None:sys.modules.pop('ref_core',None)
        else:sys.modules['ref_core']=previous
    return result

def sc(value):
    if value is None:return '_'
    if isinstance(value,bool):return str(int(value))
    if isinstance(value,int):return str(value)
    if isinstance(value,str):return 's'+value.encode().hex()
    if isinstance(value,(bytes,bytearray)):return 'b'+value.hex()
    if isinstance(value,(list,tuple)):return '['+','.join(map(sc,value))+']'
    raise TypeError(type(value))

# Author only setup and orchestration. Arithmetic and scheduler bodies below
# are always the immutable modules; the native replay consumes this exact DSL.
def recipe(modules,name):
    wire=modules['netwire']
    actors=[[[0x3f800000,0,0],32,0xc000,0x42c80000,0,[0]*5,True],
            [[0x40400000,0x40800000,0],32,17,0x42c80000,0,[0]*5,False]]
    bind=[[1,0x3f800000,0,0],[2,0x3f000001,0xffff,0]]
    point=[0x40000000,0,0x7fc01234]
    command=[0,1,0,point]
    programs=[[[],[command],[]] for _ in range(3)]
    if name=='opposite_mode':command[2]=1
    if name=='other_mode':command[2]=-2147483648
    if name=='slow_one':bind[0][3]=1
    if name=='slow_sixty':bind[0][3]=60
    if name=='slow_bypass':bind[0][3]=255
    if name=='negative_rate':bind[0][1]=0xbf800000
    if name=='raw_rate':bind[0][1]=0x7fc00001;point[:]=[0x7f800000,0x80000001,0xffffffff]
    if name=='velocity_wrap':bind[0][2]=0x8001;actors[0][2]=0x7fff
    if name=='world_authority':
        programs=[[[[2,1,0xffff,[0x40800000,0xc0000000,0x41200000]]],[command],[]],
                  [[[2,1,0x8000,[0xbf800000,0x40000000,0x41400000]]],[command],[]],
                  [[],[command,[1,1,0x40000000,0x40]],[]]]
    if name=='health_before':programs=[[[[1,1,0x40000000,0x40]],[command],[]]for _ in range(3)]
    if name=='health_after':programs=[[[],[command],[[1,1,0x40000000,0x40]]]for _ in range(3)]
    if name=='two_bindings':programs=[[[],[command,[0,2,1,[0,0,0]]],[]]for _ in range(3)]
    if name in ('retired','reused'):
        programs[1]=[[[3,1]]+([[4,[0x41100000,0,0]]]if name=='reused'else[]),[command],[]]
    if name=='twice':programs[0][1]=[command,command]
    if name=='post_failure':programs[0][2]=[[1,1,0x40000000,0x40],[5]]
    remote=wire.Machine(2,1)
    for frame in range(1,13):
        remote.peers[1].set_record(frame,wire.rec12(frame*17,(128+frame,64+frame,255-frame,frame*9),(frame*11,frame*13),frame*123,frame&1),start=0,count=0)
    packet=remote.build_packet(0,1,12,wire.PKT_RAW_RELAY,1,None,raw=True).data
    actions=[]
    for k in range(4):
        inbox=[list(packet)]if k==(2 if name=='delayed'else 0)and name!='no_ready'else[]
        sample=list(struct.pack('<H6B',1,128+k,64+3*k,255-k,17*k,31*k,47*k))
        actions.append([1,sample,k*1234,k&1,4 if name=='held'and k==0 else 1,
            name=='no_sim' or(name=='held'and k==0),name=='held'and k>0,k,name=='pending'and k<3,inbox])
        if name=='paused':actions.append([0])
    writes=[[i,0x44,4,0xdeadbeef if i==1 else 0xffffffff]for i in (1,2)]
    return [[2,0,1,0xa5a5a5a4,0],name=='frozen',actors,LIMITS,bind,writes,programs,actions]


def run_case(modules,helpers,config,name,input_value=None):
    inp=recipe(modules,name)if input_value is None else copy.deepcopy(input_value)
    hp=helpers['session_hp'];core=modules['core'];objects=modules['objects'];creature=modules['creature'];wire=modules['netwire']
    actors=inp[2]
    if len(actors)!=2:raise ValueError('authored factory requires exactly two actors')
    original_new=objects.World.new_object
    original_run=objects.World.run_every_frame
    created=[]
    def new(world,size,pos,*args,**kwargs):
        # Change only the authored factory's supplied position, then invoke the
        # original allocator/grid code. Later allocations consume their command.
        if not created:world.shared.state=inp[0][2]
        if len(created)<2:pos=actors[len(created)][0]
        obj=original_new(world,size,pos,*args,**kwargs);created.append(obj);return obj
    def run(world,obj):
        # The helper's authored actor1 registration is optional input setup;
        # suppress it before it consumes nodes when that actor is inactive.
        if created and obj is created[0] and not actors[0][6]:return None
        return original_run(world,obj)
    objects.World.new_object=new
    objects.World.run_every_frame=run
    try:oracle=hp.make_oracle(modules,helpers,config,0,'frozen'if inp[1]else'ready')
    finally:
        objects.World.new_object=original_new
        objects.World.run_every_frame=original_run
    if actors[1][6]:oracle.w.run_every_frame(oracle.b)
    for obj,record,values in zip((oracle.a,oracle.b),oracle.hp.objects,actors):
        pos,flags,angle,health,cflags,status,_=values
        obj.flags=flags;obj.angle=angle
        record.flags=flags;record.pos=list(pos);record.hp=health;record.cflags=cflags;record.status=list(status)
    oracle.project_hp()
    peers,local,shared,combat,network_seed=inp[0]
    oracle.seed_initial=shared;oracle.combat_seed=combat
    machine=type(oracle.network)(peers,local,wire.Fpu(MODELS[config//4],bool(config&2),bool(config&1)))
    machine.network_mode=1;machine.num_players=peers;oracle.network=machine
    oracle.publish_world()
    initial_shared=oracle.w.shared.state
    memory=oracle.hp.model.mem
    for ident,offset,size,value in inp[5]:memory.poke(oracle.hp_bindings[ident-1][2]+offset,size,value)
    class Turn(creature.Model):
        def __init__(self,mem):super().__init__(mem);self.events=[]
        def _w(self,address,size,value):
            self.events.append([0,[address,size,value&((1<<(size*8))-1)],['',[]]])
            return super()._w(address,size,value)
    model=Turn(memory);bindings=[];phase='idle';calls=0;operations=[]
    def import_selected(ident):
        obj=oracle.retained[ident-1];record=oracle.hp.objects[ident-1];a=obj.addr
        record.pos[0:2]=obj.pos[0:2]
        memory.poke(a+0x10,4,obj.pos[0]);memory.poke(a+0x14,4,obj.pos[1]);memory.poke(a+0x38,2,obj.angle)
    for ident,rate,av,slow in inp[4]:
        a=oracle.hp_bindings[ident-1][2]
        memory.poke(a+0xb0,4,rate);memory.poke(a+0x110,2,av);memory.poke(a+0x2fc,1,slow)
        import_selected(ident);bindings.append([oracle.hp_bindings[ident-1],[rate,av,slow],0,False,None])
    def turn(ident,mode,point):
        def action():
            nonlocal calls,phase
            if oracle.phase!='tasks' or oracle.depth!=1 or not oracle.object_run_completed or oracle.task_priority is None or oracle.task_priority<=10:raise hp.Refusal('phase')
            for entry in oracle.hp_bindings:
                known=oracle.retained[entry[1]-1];current=oracle.w.objects.get(entry[2])
                if current is not None and current is not known:raise hp.Refusal('reused')
            binding=next((b for b in bindings if b[0][0]==ident),None)
            if binding is None:raise hp.Refusal('binding')
            obj=oracle.retained[binding[0][1]-1]
            if not obj.alive or oracle.w.objects.get(obj.addr)is not obj:raise hp.Refusal('retired')
            if binding[3] or calls>=inp[3][1]:raise hp.Refusal('calls')
            op=['turn_toward',ident,mode,list(point),oracle.w.frame,None,False];operations.append(op)
            calls+=1;binding[2]+=1;binding[3]=True;binding[4]=None;phase='import'
            import_selected(ident)
            try:
                phase='turn_toward';result=model.turn_toward(obj.addr,mode,tuple(point));op[5]=result;binding[4]=result
                phase='publish';obj.angle=model.u16(obj.addr+0x38);op[6]=True;phase='idle';return result
            except Exception:
                obj.angle=model.u16(obj.addr+0x38);raise
        return oracle.guard('turn_toward',action)
    previous_ready=oracle.ready_tick
    def ready(unused):
        nonlocal calls
        calls=0
        for b in bindings:b[2]=0;b[3]=False
        return previous_ready(unused)
    machine.run_tasks=ready
    def command(cmd):
        code=cmd[0];result=None
        if code==0:result=turn(cmd[1],cmd[2],cmd[3])
        elif code==1:oracle.take_damage(cmd[1]-1,cmd[2],None,cmd[3])
        elif code==2:
            def change():
                obj=oracle.retained[cmd[1]-1];obj.angle=cmd[2];obj.pos=list(cmd[3])
            oracle.with_world(change)
        elif code==3:oracle.with_world(lambda:oracle.w.destroy(oracle.retained[cmd[1]-1],3))
        elif code==4:
            def create():
                obj=oracle.w.new_object(0x400,cmd[1]);obj.flags=32;obj.tag=0
            oracle.with_world(create)
        elif code==5:raise hp.TaskFailure('authored')
        else:raise ValueError('unknown command')
        oracle.events.append(['command',cmd,result,oracle.w.frame])
    def task(fn):
        index=next(i for i,t in enumerate(hp.TASKS)if t[0]==fn);label=hp.TASKS[index][1]
        oracle.host_events.append(['task',label,oracle.network.net_frame,[]]);oracle.effect(label)
        ready_index=oracle.ready_calls-1
        for cmd in inp[6][ready_index][index]if ready_index<len(inp[6])else[]:command(cmd)
    oracle.task=task
    def snapshot():
        base=oracle.snapshot();base[0][0][0]=['rta',config//4,bool(config&2),bool(config&1),peers,local,1,combat,network_seed]
        base[1].append(None);base[1][5][11].append(None)
        state=[inp[3],bindings,COMPOSITION_POLICY,phase,'','hp.adapter.model.mem',0,calls,
            [1048576,65536,64,64],0,True,[],[],model.writes,model.calls,model.events,model.draws,
            operations,oracle.object_run_completed,oracle.task_priority]
        return copy.deepcopy([[base,None],state])
    try:
        initial=snapshot();previous=initial;steps=[]
        for action in inp[7]:
            result=None
            try:
                if action[0]:
                    _,sample,yaw,camera,step,no_sim,hold,render,pending,inbox=action
                    if oracle.failed:raise hp.Refusal('latched')
                    oracle.network.level_pending=pending
                    def submit():
                        oracle.started=True
                        return oracle.network.net_main(bytes(sample),yaw=yaw,camera_flag=camera,frame_step=step,no_sim=no_sim,hold_arg=hold,render_frame=render,inbox=[bytes(x)for x in inbox])
                    returned=oracle.guard('submit',submit)
                    result=[[helpers['session_objects'].sent(x)for x in returned[0]],returned[1],returned[2]]
            except hp.TaskFailure:result='TaskFailure'
            except hp.Refusal:result='Refusal'
            current=snapshot();patch=hp.snapshot_delta(previous,current)
            if hp.apply_delta(previous,patch)!=current:raise AssertionError('nonlossless delta')
            steps.append([result,patch]);previous=current
        if model.draws or model.calls or (oracle.w.shared.state!=initial_shared and not any(c[0]==4 for p in inp[6] for t in p for c in t)) or oracle.combat_seed!=combat or oracle.hp.world.seed.draws or oracle.hp.world.cosmetic.draws:
            raise AssertionError('unexpected turning callbacks/random streams')
        return [inp,initial,steps,oracle.events]
    finally:oracle.close()


def cases(modules):
    for config in CONFIGURATIONS:
        for name in SCENARIOS:
            if config and name not in FPU_SCENARIOS:continue
            yield name,config,recipe(modules,name)


def independent_anchors(modules,helpers):
    hp=helpers['session_hp']
    def states(name):
        value=run_case(modules,helpers,0,name);state=value[1];out=[]
        for _,patch in value[2]:state=hp.apply_delta(state,patch);out.append(state)
        return value,out
    ready,ss=states('ready')
    turns=[e[2]for e in ready[3]if e[0]=='command'and e[1][0]==0]
    assert turns[0]==16384 and len(turns)==3
    assert [s[1][1][0][2]for s in ss]==[0,1,1,1]
    slow,sl=states('slow_sixty')
    assert all(e[2]==16384 for e in slow[3]if e[0]=='command')
    for name in ('health_before','health_after'):
        _,rows=states(name)
        assert [r[0][0][1][5][7][0][16]for r in rows]==[0x42c80000,0x42c40000,0x42c00000,0x42bc0000]
    frozen,_=states('frozen')
    assert not any(e[0]=='run'for e in frozen[3]) and any(e[0]=='command'for e in frozen[3])
    for name in ('no_ready','no_sim'):
        value,_=states(name);assert not any(e[0]=='command'for e in value[3])
    paused,_=states('paused')
    assert all(step==[None,[]]for step in paused[2][1::2])
    twice,rows=states('twice')
    assert len(rows[1][1][17])==1 and rows[1][1][17][0][-1]
    assert all(not step[1] for step in twice[2][2:])


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'session-turn-generator'
    modules = load_references(research_dir, scratch_dir)
    helpers=load_helpers()
    independent_anchors(modules,helpers)
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
                header=b'# session_turn_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for label,config,inputs in cases(modules):
                    first=sc(run_case(modules,helpers,config,label,inputs))
                    if first!=sc(run_case(modules,helpers,config,label,inputs)):
                        raise AssertionError('fresh reference executions diverged')
                    row=('\t'.join([label,str(config),str(number)+';'+sc(inputs),'ok',first])+'\n').encode('ascii')
                    stream.write(row);vh.update(row);th.update(row)
                    for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[label]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'session-turn','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'helpers':HELPERS,'ordering_evidence':ORDERING_EVIDENCE,
          'input_protocol':INPUT_PROTOCOL,
          'recipe_configurations':{name:list(CONFIGURATIONS)if name in FPU_SCENARIOS else[0]for name in SCENARIOS},
          'composition_contract':{
           'scope':'Authored ready-task turning adapter. No original player or AI ordering, scene/controller, terrain or movement dependency.',
           'oracle':'Immutable original core/creature/objects/netwire/combat bodies and hash-pinned authored snapshot helpers; only input setup and Session scheduling/projection/latch policy are authored.',
           'authority':'Selected WorldXY copied to typedHP XY and rawXY, each Z preserved independently. Import Worldangle and publish only rawfacing on success/failure. No Adapter.project_all or implicit frame/seed/status projection.',
           'identity':'Existing HP ID/World serial/address/Adapter generation; one accepted turn per binding per actual ready tick, aggregate supplied cap. Separate callback-free borrowed Model uses shared HP Memory/log once.',
           'random':'Turn invokes no callback or random generator; all shared/object/combat/cosmetic state remains observed.',
           'safety':'Finite original calls plus explicitly authored identity/once/latch/task failures. Native memory/history/overlay bounds are independent API tests, not original-model outcomes.',
           'publication':'Sibling temporaries flushed/fsynced, fixture replaced then manifest last. Interrupted pair detected by hashes; not a two-file transaction.'}}
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
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_turn.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
