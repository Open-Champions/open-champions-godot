#!/usr/bin/env python3
"""Original-model expectations for an explicitly authored ready-task animation gate.

Integer/list input contains all clip metadata, task commands and submitted packets.
Only composition policy is authored; clock, scheduler, wire, pool and HP operations
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
 'anim': {'commit':'f516ac798bf28375554fe53ffb31b74802844136','path':'tools/ref_anim.py','sha256':'6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8'},
}
REFERENCE_SHA256=hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
SPECIFICATION_EVIDENCE={'commit':'f2b996dded898f1b0b558b901cab875f775cfdc7','path':'docs/spec/animation.md','sha256':'be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab'}
ORDERING_EVIDENCE={'commit':'0dd02ce416389dd440540119727d946636507703','path':'docs/mainloop.md','sha256':'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}
COMPOSITION_POLICY='rta-session-animation-v1'
MODELS=('ps2','ps2g1','chop','nearest')
SCENARIOS=('events','shared_events','layers','stop_stale','end_flags','paused','waiting','null_clip','delayed','no_ready','pending','no_sim','held','frozen','retired','post_failure','loop','negative_speed','clock_failure','distinct_clips')
CONFIGURATIONS=tuple(range(16))
VARIANTS=1
LIMITS=[64,16,512,64,65536]

def load_helpers():
    root=pathlib.Path(__file__).resolve().parents[1];out={}
    for name,pin in HELPERS.items():
        path=root/pin['path'];source=path.read_bytes()
        if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('helper hash mismatch: '+name)
        m=types.ModuleType('session_animation_helper_'+name);m.__file__=str(path);sys.modules[m.__name__]=m
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
            m=types.ModuleType('session_animation_pinned_'+name);m.__file__='/__verified_models__/ref_'+name+'.py';sys.modules[m.__name__]=m
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

# Input [seed_variant,frozen,limits,clips,bindings,programs,submissions].
# Clips [length,mask,events4,sound_presence], bindings [HpId,capacity].
# Ready programs are selected once per actual ready tick; beyond their end empty.
# Commands: start[0,id,clip,blend,flags], stop[1,id,clip,blend],
# advance[2,id,speed,consume_mask,damage], event[3,clip,slot,seconds,mask],
# destroy[4,id], authored_throw[5]. No native expectation determines execution.
# Submit [sample8bytes,yaw,camera,step,no_sim,hold,render,pending,inboxByteLists].
def recipe(modules,variant,name):
    del variant
    A=modules['anim'];C=modules['core'];wire=modules['netwire']
    clips=[[3,0xffffffffffffffff,[[0,0x80],[A.K60,1],[0x3d088888,0x40],[0,0]],False],
           [2,0x8000000000000001,[[0,2],[0,0],[0,0],[0,0]],True]]
    start=lambda c=1,f=0:[0,1,c,0,f]
    advance=lambda mask=0:[2,1,A.ONE,mask,0x40000000]
    programs=[[start(),advance(0x80)],[advance(1)],[advance(0x40)]]
    if name=='distinct_clips':
        clips[1]=copy.deepcopy(clips[0]);programs=[[[0,1,1,0,65568],[0,2,2,0,0],[3,1,0,0,1],advance(),[2,2,A.ONE,0,0]],[advance()],[advance()]]
    if name=='shared_events':programs=[[start(),[0,2,1,0,0],[3,1,0,0,255],advance(),[2,2,A.ONE,0,0]],[[3,1,1,A.K60,128],advance(),[2,2,A.ONE,0,0]],[advance()]]
    if name=='layers':programs=[[start(1,A.PERSISTENT),[0,1,2,0x3f000001,0],advance()],[advance()],[advance()]]
    if name=='stop_stale':programs=[[start(),[0,1,2,0,0],advance()],[[1,1,1,0],advance()],[[1,1,2,0],advance()]]
    if name=='end_flags':programs=[[start(2,A.REPORT_END|A.KEEP),advance()],[advance()],[advance()]]
    if name=='paused':programs=[[start(1,A.PAUSED),advance()],[advance()],[advance()]]
    if name=='waiting':programs=[[start(),[0,1,2,0,A.PURGE_BELOW|A.PERSISTENT],advance()],[advance()],[advance()]]
    if name=='null_clip':programs=[[[0,1,0,0,0],[1,1,0,0],advance()],[start(),advance()],[[1,1,1,0x3f000001],advance()]]
    if name=='retired':programs=[[start(),advance()],[[4,1],advance()],[advance()]]
    if name=='loop':programs=[[start(2,A.LOOP),[2,1,0x41200000,0,0]],[advance()],[advance()]]
    if name=='negative_speed':programs=[[start(),[2,1,0xbf800000,0,0]],[advance()],[advance()]]
    if name=='clock_failure':
        clips[0][0]=0;programs=[[start(1,A.LOOP),advance()],[advance()],[advance()]]
    if name=='post_failure':programs=[[start(),advance(0x80),[5]],[advance()],[advance()]]
    remote=wire.Machine(2,1)
    for frame in range(1,13):
        remote.peers[1].set_record(frame,wire.rec12(frame*17,(128+frame,64+frame,255-frame,frame*9),
            (frame*11,frame*13),frame*123,frame&1),start=0,count=0)
    packet=remote.build_packet(0,1,12,wire.PKT_RAW_RELAY,1,None,raw=True).data
    submissions=[]
    for k in range(4):
        sample=list(struct.pack('<H6B',1,128+k,64+3*k,255-k,17*k,31*k,47*k))
        inbox=[list(packet)] if k==(2 if name=='delayed' else 0) and name!='no_ready' else []
        submissions.append([sample,k*1234,k&1,4 if name=='held' and k==0 else 1,name=='no_sim' or (name=='held' and k==0),name=='held' and k>0,k,name=='pending' and k<3,inbox])
    return [0,name=='frozen',LIMITS,clips,[[1,4],[2,2]],programs,submissions]


def run_case(modules,helpers,config,variant,name,input_value=None):
    inp=recipe(modules,variant,name) if input_value is None else copy.deepcopy(input_value)
    hp=helpers['session_hp'];A=modules['anim'];core=modules['core']
    oracle=hp.make_oracle(modules,helpers,config,inp[0],'frozen' if inp[1] else 'ready')
    clips=[]
    for length,mask,events,sound in inp[3]:
        clip=object.__new__(A.Clip);clip.length=length;clip.joint_mask=mask;clip.events=[tuple(e) for e in events];clip.sound_times=[] if sound else None;clips.append(clip)
    states={i:A.AnimState(cap) for i,cap in inp[4]};system=types.SimpleNamespace(sim_frame=0,cosmetic_seed=0)
    counts={i:0 for i in states};advanced={i:False for i in states};last={i:None for i in states}
    ops=[];animphase='idle';animcalls=0
    old_ready=oracle.ready_tick
    def ready(_):
        nonlocal animcalls
        animcalls=0
        for i in states:counts[i]=0;advanced[i]=False
        return old_ready(_)
    oracle.network.run_tasks=ready
    def task(fn):
        nonlocal animphase,animcalls
        label=next(t[1] for t in hp.TASKS if t[0]==fn)
        oracle.host_events.append(['task',label,oracle.network.net_frame,[]]);oracle.effect(label)
        if label!='hp_apply':return
        index=oracle.ready_calls-1
        for command in inp[5][index] if index<len(inp[5]) else []:
            code=command[0]
            if code==4:
                oracle.with_world(lambda:oracle.w.destroy(oracle.retained[command[1]-1],3));continue
            if code==5:raise hp.TaskFailure('authored')
            kind=('start_animation','stop_animation','advance_animation','set_animation_event')[code];binding=0 if code==3 else command[1]
            def action():
                nonlocal animcalls,animphase
                if binding and not oracle.retained[oracle.hp_bindings[binding-1][1]-1].alive:raise hp.Refusal('retired')
                if binding and code==2 and advanced[binding]:raise hp.Refusal('once')
                clip=command[1] if code==3 else command[2] if code in (0,1) else 0
                parameter=command[2] if code==2 else command[3] if code in (0,1) else 0
                flags=command[4] if code==0 else 0
                op=[kind,binding,clip,oracle.w.frame,parameter,flags,command[2] if code==3 else 0,command[3:5] if code==3 else [0,0],None,False]
                ops.append(op);system.sim_frame=oracle.w.frame;animcalls+=1
                if binding:counts[binding]+=1
                animphase=kind
                if code==0:
                    result=A.anim_start(parameter,states[binding],clips[clip-1] if clip else None,flags)
                    result=None if result is None else next(i for i,t in enumerate(states[binding].tracks) if t is result)
                elif code==1:A.anim_stop(parameter,states[binding],clips[clip-1] if clip else None);result=None
                elif code==2:
                    advanced[binding]=True;last[binding]=None
                    result=A.anim_advance(system,parameter,states[binding],False,False,False)[0];last[binding]=result
                else:clips[clip-1].events[command[2]]=tuple(command[3:5]);result=None
                op[8]=result;op[9]=True;animphase='idle'
                return result
            result=oracle.guard(kind,action)
            oracle.events.append(['command',command,result,oracle.w.frame])
            if code==2 and result&command[3]:oracle.take_damage(binding-1,command[4],None,0x40)
    oracle.task=task
    def snapshot():
        base=oracle.snapshot();base[1].append(None);base[1][5][11].append(None)
        bs=[]
        for ident,state in states.items():
            tracks=[[0 if t.clip is None else next(i+1 for i,c in enumerate(clips) if c is t.clip),0,t.weight,t.rate,t.life,t.life_rate,t.time,t.flags,t.sound_idx] for t in state.tracks]
            bs.append([oracle.hp_bindings[ident-1],0,state.count,state.capacity,state.flags,tracks,counts[ident],advanced[ident],last[ident]])
        registry=[[i+1,[c.length,c.joint_mask,[list(e) for e in c.events],c.sound_times is not None]] for i,c in enumerate(clips)]
        animation=[inp[2],[system.sim_frame,system.cosmetic_seed,inp[2][4]],registry,bs,COMPOSITION_POLICY,animphase,0,animcalls,sum(c for _,c in inp[4]),ops,oracle.object_run_completed,oracle.task_priority]
        return copy.deepcopy([[base,None],animation])
    try:
        initial=snapshot();previous=initial;steps=[]
        for sample,yaw,camera,step,no_sim,hold,render,pending,inbox in inp[6]:
            try:
                if oracle.failed:raise hp.Refusal('latched')
                oracle.network.level_pending=pending
                def submit():
                    oracle.started=True
                    return oracle.network.net_main(bytes(sample),yaw=yaw,camera_flag=camera,frame_step=step,no_sim=no_sim,hold_arg=hold,render_frame=render,inbox=[bytes(x) for x in inbox])
                returned=oracle.guard('submit',submit)
                result=[[helpers['session_objects'].sent(x) for x in returned[0]],returned[1],returned[2]]
            except hp.TaskFailure:result='TaskFailure'
            except hp.Refusal:result='Refusal'
            except A.HostFault:result='HostFault'
            current=snapshot();patch=hp.snapshot_delta(previous,current)
            if hp.apply_delta(previous,patch)!=current:raise AssertionError('delta is not lossless')
            steps.append([result,patch]);previous=current
        if system.cosmetic_seed or oracle.hp.world.seed.draws or oracle.hp.world.cosmetic.draws:raise AssertionError('animation changed RNG')
        return [inp,initial,steps,oracle.events]
    finally:oracle.close()


def independent_anchors(modules,helpers):
    value=run_case(modules,helpers,0,0,'events');hp=helpers['session_hp']
    states=[];state=value[1]
    for result,patch in value[2]:
        state=hp.apply_delta(state,patch);states.append(state)
    events=[e[2] for e in value[3] if e[0]=='command' and e[1][0]==2]
    if events!=[-128,1,64]:raise AssertionError(('signed original event windows',events))
    if [s[0][0][1][5][7][0][16] for s in states]!=[0x42c80000,0x42c40000,0x42c00000,0x42bc0000]:raise AssertionError('event consumer must persist HP100->98->96->94')
    if states[-1][1][1][1]!=0:raise AssertionError('cosmetic seed changed')
    distinct=run_case(modules,helpers,0,0,'distinct_clips')
    results=[e[2] for e in distinct[3] if e[0]=='command' and e[1][0]==2]
    if results[:2]!=[1,-128]:raise AssertionError('distinct clips with equal metadata were aliased')
    frozen=run_case(modules,helpers,0,0,'frozen')
    if any(e[0]=='run' for e in frozen[3]):raise AssertionError('frozen World unexpectedly ran objects')
    if not any(e[0]=='command' for e in frozen[3]):raise AssertionError('frozen World incorrectly stopped scheduler animation')
    for name in ('no_ready','no_sim'):
        blocked=run_case(modules,helpers,0,0,name)
        if any(e[0]=='command' for e in blocked[3]):raise AssertionError('blocked frame ran animation')

def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'session-animation-generator'
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
                header=b'# session_animation_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for config in CONFIGURATIONS:
                    for variant in range(VARIANTS):
                        for scenario in SCENARIOS:
                            first=sc(run_case(modules,helpers,config,variant,scenario))
                            if first!=sc(run_case(modules,helpers,config,variant,scenario)):
                                raise AssertionError('fresh reference executions diverged')
                            row=('\t'.join([scenario,str(config),str(number)+';'+sc(recipe(modules,variant,scenario)),'ok',first])+'\n').encode('ascii')
                            stream.write(row);vh.update(row);th.update(row)
                            for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                            counts[scenario]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'session-animation','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'helpers':HELPERS,'ordering_evidence':ORDERING_EVIDENCE,
          'input_protocol':{'version':1,'columns':['operation','config','id;explicit-input-DSL','status','observation'],
          'setup':'[seed_variant,frozen,limits5,clips[[length,jointmask,events4,sound]],bindings[[HpId,capacity]],ready_programs,submissions]',
          'commands':'start[0,id,clip,blend,flags]; stop[1,id,clip,blend]; advance[2,id,speed,consume_mask,damage]; event[3,clip,index,seconds,mask]; destroy[4,id]; authored_failure[5]',
          'submissions':'[sample8bytes,yaw,camera,frame_step,no_sim,hold,render,pending,inboxByteLists]',
          'observation':'[input,initial_full_snapshot,[[result,lossless_state_delta],...],authored_effects]'},
          'composition_contract':{'scope':'Authored post-runObjects timing adapter, not original Creature_stepAnim ordering. No pose, caches, ROOT/YAW, sounds, UI or asset data.',
          'oracle':'Immutable original anim/core/objects/netwire/combat/creature bodies with frozen helper serializers; no native expectation inputs.',
          'identity':'Owned clips keep stable independent IDs, shared mutable event metadata, complete fixed cells including stale cells, retained retired actor bindings.',
          'rng':'No randomize call; Session combat/cosmetic and World shared streams remain separate and unchanged.',
          'safety':'Finite original calls only. Native LOOP budget and other wrapper-only guards are independently tested, not invented model successes.',
          'publication':'Two sibling temporary files fsynced; fixture replaced then manifest last. Interrupted pair detectable by hash mismatch.'}}
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
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_animation.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
