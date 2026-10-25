#!/usr/bin/env python3
"""Independent complete Session cache composition from immutable Python bodies.

Only host glue is authored. No native-output input or expected-value injection.
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

# Frozen serializers and host contracts, checked before importing executable code.
HELPERS = {
    'session_animation': {'path': 'tools/generate_session_animation_vectors.py', 'sha256': '8c68a52d0cabb4dd60c822b4db0f6e95cc986bd61edd759d3eded263fd792b3c'},
    'animation_runtime': {'path': 'tools/generate_animation_runtime_vectors.py', 'sha256': '3f05cc05042a55f7a37714bd22f0180f45ce62eb3c7d681737ff852836df31cb'},
    'session_hp': {'path': 'tools/generate_session_hp_vectors.py', 'sha256': '8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'},
    'session_objects': {'path': 'tools/generate_session_objects_vectors.py', 'sha256': '05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'},
    'combat_snapshot': {'path': 'tools/generate_combat_creature_vectors.py', 'sha256': 'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
}
CONFIGURATIONS=tuple(range(16))
COMPOSITION_POLICY = 'rta-session-anim-cache-v1'
SCENARIOS = ('changing_joints', 'shared_events', 'distinct_clips', 'eviction',
             'stop_restart', 'compaction', 'stale_inactive', 'backwards',
             'repeat_decode', 'paused', 'negative_time', 'loop', 'end',
             'retired', 'reused', 'no_ready', 'delayed', 'pending', 'no_sim',
             'frozen', 'post_failure', 'decoder_failure', 'loop_budget',
             'key_budget', 'operation_budget', 'history_budget', 'once', 'invalid_slots', 'invalid_capacity', 'invalid_bytes', 'intermittent')
INPUT_PROTOCOL = {
 'version': 1, 'columns': ['operation','config','id;explicit-input-DSL','status','observation'],
 'setup': '[seed_variant,frozen,[slots,runtimeLimits4,calls,history],clips[[nameBytes,fileBytes]],bindings[[HpId,capacity]],ready_programs,submissions]',
 'commands': 'start[0,hp,clip,blend,flags]; stop[1,hp,clip,blend]; advance[2,hp,speed,consume_mask,damage]; event[3,clip,index,seconds,mask]; destroy[4,hp]; failure[5]; decode[6,hp,liveTrackId]; reuse[7,hp]',
 'snapshot': '[complete legacy Session base,cache[limits4,runtimeSnapshot3,bindings6,policy,phase,depth,calls,operations12,runCompleted,priority]]',
 'binding': '[HpBinding4,StateId,calls,advanced,lastEvent,lastDecoded]',
 'operation': '[kind,hp,clip,track,frame,rawParameter,rawFlags,eventIndex,event2,result,lastDecode,completed]',
 'decoded': '[TrackId,SlotId,rawTime,[[rawPosition3,rawQuaternion4],...]]',
 'submissions': '[sample8bytes,yaw,camera,step,no_sim,hold,render,pending,inboxByteLists]',
 'observation': '[input,initialSnapshot,[[result,losslessDelta],...],authoredEffects]',
 'host_budgets': 'Trace hook before original LOOP subtraction and decoder delta read; executed original stores preserved. No rewritten arithmetic.',
}

def load_helpers():
    root = pathlib.Path(__file__).resolve().parents[1]
    out = {}
    for name, pin in HELPERS.items():
        path = root / pin['path']; source = path.read_bytes()
        if hashlib.sha256(source).hexdigest() != pin['sha256']:
            raise ValueError('helper hash mismatch: '+name)
        module = types.ModuleType('cache_helper_'+name); module.__file__ = str(path)
        sys.modules[module.__name__] = module
        exec(compile(source, str(path), 'exec'), module.__dict__); out[name] = module
    return out

# Reference maps copied from the immutable historical manifest, verified by the
# existing pinned loader, not selected from ambient research HEAD.
REFERENCES = {
 'core': {'commit':'41e847af5360724ab62d208594f8027ea3c1b686','path':'tools/ref_core.py','sha256':'0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
 'objects': {'commit':'0dd02ce416389dd440540119727d946636507703','path':'tools/ref_objects.py','sha256':'541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'},
 'netwire': {'commit':'54258f9d7e281ef07f0675040e82e6264f9386cf','path':'tools/ref_netwire.py','sha256':'4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'},
 'combat': {'commit':'7212ecfecbeae8dd2015f6c82897226f49a94695','path':'tools/ref_combat.py','sha256':'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
 'creature': {'commit':'a0d89502ce1431375a6fc045b91a73039114e6b9','path':'tools/ref_creature.py','sha256':'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
 'anim': {'commit':'f516ac798bf28375554fe53ffb31b74802844136','path':'tools/ref_anim.py','sha256':'6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
SPECIFICATION_EVIDENCE = {'commit':'f2b996dded898f1b0b558b901cab875f775cfdc7','path':'docs/spec/animation.md','sha256':'be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab'}
ORDERING_EVIDENCE = {'commit':'0dd02ce416389dd440540119727d946636507703','path':'docs/mainloop.md','sha256':'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}

def recipe(modules, helpers, name):
    A = modules['anim']; old = helpers['session_animation']
    value = old.recipe(modules,0,name if name in old.SCENARIOS else 'events')
    one = A.encode_clip([((64,-64,32),(4096,0,0,0))], 20,
                       keys=[(1,0,'p',(256,-128,17)), (1,0,'r',(128,31,-47,99)),
                             (3,0,'p',(-77,123,256))])
    one=bytearray(one)
    for i,(seconds,mask) in enumerate([(0,1),(A.K60,2),(0x3d088888,4),(0,0)]):
        one[0x14+8*i:0x18+8*i]=seconds.to_bytes(4,'little');one[0x18+8*i]=mask
    two = A.encode_clip([((32,17,-64),(4096,0,0,0))], 20,
                       keys=[(1,0,'p',(-128,256,31))])
    limits = [[16384,65536,64,512,2,64],16,512,64]
    value[2] = [2,limits,64,4096]
    value[3] = [[list(b'one'),list(one)],[list(b'two'),list(two)]]
    start = lambda c=1,f=0,h=1:[0,h,c,0,f]
    advance = lambda speed=A.ONE,h=1,mask=0:[2,h,speed,mask,0x40000000]
    decode = lambda t=1,h=1:[6,h,t]
    value[5] = [[start(),advance(mask=1),decode()], [advance(mask=2),decode()],
                [advance(mask=4),decode(),[1,1,1,0],start(),decode()]]
    programs = {
      'shared_events': [[start(),start(h=2),[3,1,0,0,255],advance(),advance(h=2),decode(),decode(5,2)]],
      'distinct_clips': [[start(),start(2,h=2),[3,1,0,0,255],advance(),advance(h=2),decode(),decode(5,2)]],
      'eviction': [[start(),start(2),start(h=2),decode(),decode(2),decode(5,2),decode()]],
      'stop_restart': [[start(),advance(),decode()],[[1,1,1,0],start(2),advance(),decode()],[advance(),decode()]],
      'compaction': [[start(),start(2),advance(),decode(2),[1,1,1,0],decode()]],
      'stale_inactive': [[start(),start(2),decode(2),[1,1,1,0],decode(),start(),decode(2)]],
      'backwards': [[start(),advance(0x40a00000),decode()],[advance(0xc0800000),decode()],[advance(),decode()]],
      'repeat_decode': [[start(),advance(),decode(),decode(),decode()],[advance(),decode(),decode()]],
      'paused': [[start(f=A.PAUSED),advance(),decode()],[advance(),decode()]],
      'negative_time': [[start(),advance(0xbf000001),decode()],[advance(),decode()]],
      'loop': [[start(f=A.LOOP),advance(0x41c00000),decode()],[advance(),decode()]],
      'end': [[start(f=A.REPORT_END|A.KEEP),advance(0x41c00000),decode()],[advance(),decode()]],
      'retired': [[start(),decode()],[[4,1],decode()]],
      'reused': [[start(),decode()],[[7,1],decode()]],
      'post_failure': [[start(),advance(mask=1),decode(),[5]]],
      'decoder_failure': [[start(),advance(),decode()]],
      'loop_budget': [[start(f=A.LOOP),advance(0x42c80000),decode()]],
      'key_budget': [[start(),advance(0x40a00000),decode()]],
      'operation_budget': [[start(),advance(),decode()]],
      'history_budget': [[start(),decode()],[advance(),decode()]],
      'once': [[start(),advance(),decode(),advance()]],
    }
    if name in programs:value[5] = programs[name]
    if name=='distinct_clips': value[3][1]=copy.deepcopy(value[3][0])
    if name=='decoder_failure':
        # Valid parsed clip with deliberately missing key stream. Native and
        # original decoding both retain initialization/allocation prefixes.
        data = bytearray(one); data[8:12]=(len(data)).to_bytes(4,'little')
        value[3][0][1]=list(data)
    if name=='loop_budget':value[2][1][3]=1
    if name=='key_budget':value[2][1][0][5]=1
    if name=='operation_budget':value[2][2]=2
    if name=='history_budget':value[2][3]=2
    if name=='invalid_slots':value[2][0]=1
    if name=='invalid_capacity':value[4][0][1]=128
    if name=='invalid_bytes':value[3][0][1]=[0]*5
    if name=='intermittent':value[6][2][4]=True
    return value


def run_case(modules, helpers, config, name, supplied=None):
    inp = copy.deepcopy(supplied) if supplied is not None else recipe(modules,helpers,name)
    hp=helpers['session_hp']; A=modules['anim']; runtime=helpers['animation_runtime']
    oracle=hp.make_oracle(modules,helpers,config,inp[0],'frozen' if inp[1] else 'ready')
    slots,limits,calls_limit,history_limit=inp[2]
    r=None;states={};state_ids={}
    try:
        def enable():
            nonlocal r
            if slots<2:raise ValueError('decoder list needs two slots')
            rs=[[0,0],limits,[],[],[],
                [[i,0,0,0,0,0,[[0]*16 for _ in range(64)]] for i in range(slots)],list(range(slots,0,-1)),[]]
            r=runtime.Oracle(modules,rs)
        oracle.guard('enable_animation_cache',enable)
        for label,data in inp[3]:
            oracle.guard('add_animation_cache_clip',lambda:r.clips.append(A.Clip(bytes(data),bytes(label).decode('latin1'))))
        for ident,cap in inp[4]:
            def bind():
                if cap<1 or cap>127:raise ValueError('positive signed byte')
                sid=r.add_state(cap);states[ident]=r.states[sid-1];state_ids[ident]=sid
            oracle.guard('bind_animation_cache',bind)
    except (ValueError,A.HostFault,hp.Refusal):pass
    counts={i:0 for i in states};advanced={i:False for i in states};last={i:None for i in states};decoded={i:None for i in states}
    ops=[];phase='idle';calls=0
    old_ready=oracle.ready_tick
    def ready(machine):
        nonlocal calls
        calls=0
        for i in states:counts[i]=0;advanced[i]=False
        return old_ready(machine)
    oracle.network.run_tasks=ready
    def bounded(fn,loop=None,key=None):
        # Pinned exact line events precede arithmetic/read work. These are host
        # charge boundaries, not replacement implementations of either body.
        remaining=[loop,key]
        def trace(frame,event,arg):
            if event=='line' and frame.f_code.co_filename==A.__file__:
                charge = 0 if frame.f_code.co_name=='anim_advance' and frame.f_lineno==1242 else 1 if frame.f_code.co_name=='animtrack_decode' and frame.f_lineno==494 else None
                if charge is not None and remaining[charge] is not None:
                    if remaining[charge]==0:raise hp.Refusal('host work budget')
                    remaining[charge]-=1
            return trace
        previous=sys.gettrace();sys.settrace(trace)
        try:return fn()
        finally:sys.settrace(previous)
    def task(fn):
        nonlocal phase,calls
        label=next(t[1] for t in hp.TASKS if t[0]==fn)
        oracle.host_events.append(['task',label,oracle.network.net_frame,[]]);oracle.effect(label)
        if label!='hp_apply':return
        index=oracle.ready_calls-1
        for command in inp[5][index] if index<len(inp[5]) else []:
            code=command[0]
            if code in (4,7):
                def destroy():
                    old=oracle.retained[command[1]-1];oracle.w.destroy(old,3)
                    if code==7:
                        new=oracle.w.new_object(0x400,(0,0,0));new.flags=32;new.tag=0
                oracle.with_world(destroy);continue
            if code==5:raise hp.TaskFailure('authored')
            kind={0:'start_animation_cache',1:'stop_animation_cache',2:'advance_animation_cache',3:'set_animation_cache_event',6:'decode_animation_cache'}[code]
            binding=0 if code==3 else command[1]
            def action():
                nonlocal calls,phase
                if binding:
                    actor=oracle.retained[oracle.hp_bindings[binding-1][1]-1]
                    if not actor.alive:raise hp.Refusal('retired')
                if calls>=calls_limit or len(ops)>=history_limit:raise hp.Refusal('host limit')
                if code==2 and advanced[binding]:raise hp.Refusal('once')
                if code==6 and r.tracks[command[2]-1] not in states[binding].tracks[:states[binding].count]:raise hp.Refusal('inactive')
                cid=command[1] if code==3 else command[2] if code in (0,1) else 0
                parameter=command[2] if code==2 else command[3] if code in (0,1) else 0
                op=[kind,binding,cid,command[2] if code==6 else 0,oracle.w.frame,parameter,command[4] if code==0 else 0,command[2] if code==3 else 0,command[3:5] if code==3 else [0,0],None,None,False]
                ops.append(op);r.system.sim_frame=oracle.w.frame;calls+=1;phase=kind
                if binding:counts[binding]+=1
                result=None
                if code==0:
                    track=A.anim_start(parameter,states[binding],r.clip(cid),command[4])
                    result=r.identity(track,r.tracks);op[3]=result
                elif code==1:A.anim_stop(parameter,states[binding],r.clip(cid))
                elif code==2:
                    advanced[binding]=True;last[binding]=None
                    result=bounded(lambda:A.anim_advance(r.system,parameter,states[binding],False,False,False),loop=limits[3])[0]
                    last[binding]=result;op[9]=result
                elif code==3:r.clip(cid).events[command[2]]=tuple(command[3:5])
                elif code==6:
                    decoded[binding]=None;t=r.tracks[command[2]-1]
                    slot=bounded(lambda:A.animtrack_decode(r.system,t),key=limits[0][5])
                    result=[command[2],r.identity(slot,r.system.decoders.slots),t.time,[]];op[10]=copy.deepcopy(result)
                    for j in range(t.clip.joint_count):
                        result[3].append([list(A.joint_pos(slot,j,t.time)),list(A.joint_quat(slot,j,t.time))]);op[10]=copy.deepcopy(result)
                    decoded[binding]=copy.deepcopy(result)
                op[11]=True;phase='idle'
                return result
            result=oracle.guard(kind,action)
            oracle.events.append(['command',command,result,oracle.w.frame])
            if code==2 and result&command[3]:oracle.take_damage(binding-1,command[4],None,0x40)
    oracle.task=task
    def snapshot():
        base=oracle.snapshot();base[1].append(None);base[1][5][11].append(None)
        bindings=[[oracle.hp_bindings[i-1],state_ids[i],counts[i],advanced[i],last[i],decoded[i]] for i in states]
        cache=None if r is None else [inp[2],r.snapshot(),bindings,COMPOSITION_POLICY,phase,0,calls,ops,oracle.object_run_completed,oracle.task_priority]
        return copy.deepcopy([[base,None],cache])
    try:
        initial=snapshot();previous=initial;steps=[];retained=copy.deepcopy(initial)
        for sample,yaw,camera,step,no_sim,hold,render,pending,inbox in inp[6]:
            try:
                if oracle.failed:raise hp.Refusal('latched')
                oracle.network.level_pending=pending
                def submit():
                    oracle.started=True
                    return oracle.network.net_main(bytes(sample),yaw=yaw,camera_flag=camera,frame_step=step,no_sim=no_sim,hold_arg=hold,render_frame=render,inbox=[bytes(p) for p in inbox])
                result=oracle.guard('submit',submit)
                result=[[helpers['session_objects'].sent(x) for x in result[0]],result[1],result[2]]
            except hp.TaskFailure:result='TaskFailure'
            except hp.Refusal:result='Refusal'
            except A.HostFault:result='HostFault'
            current=snapshot();patch=hp.snapshot_delta(previous,current)
            if hp.apply_delta(previous,patch)!=current:raise AssertionError('lossy delta')
            steps.append([result,patch]);previous=current
        if initial!=retained:raise AssertionError('observation escaped storage')
        if (r is not None and r.system.cosmetic_seed) or oracle.hp.world.seed.draws or oracle.hp.world.cosmetic.draws:raise AssertionError('random ledger changed')
        return [inp,initial,steps,oracle.events]
    finally:oracle.close()


def anchors(modules,helpers):
    value=run_case(modules,helpers,0,'changing_joints')
    samples=[e[2][3][0][0] for e in value[3] if e[0]=='command' and e[1][0]==6]
    if len(set(map(tuple,samples)))<3:raise AssertionError('joints must change across ready ticks')
    events=[e[2] for e in value[3] if e[0]=='command' and e[1][0]==2]
    if events[:3]!=[1,2,4]:raise AssertionError('event windows changed')
    state=value[1];history=[]
    for _,patch in value[2]:
        state=helpers['session_hp'].apply_delta(state,patch);history.append(state)
    if [s[0][0][1][5][7][0][16] for s in history]!=[0x42c80000,0x42c40000,0x42c00000,0x42bc0000]:
        raise AssertionError('authored HP consumer must retain 100 to 98 to 96 to 94')
    if not all(op[11] for op in state[1][7]):raise AssertionError('end-to-end operations incomplete')
    for name in ('no_ready','no_sim'):
        if any(e[0]=='command' for e in run_case(modules,helpers,0,name)[3]):raise AssertionError('no-ready work')


def generate(research_dir,output):
    helpers=load_helpers();modules=helpers['session_animation'].load_references(research_dir)
    anchors(modules,helpers)
    output=pathlib.Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end in .tsv.gz')
    lines=[b'# session_anim_cache_vectors_v1\n'];count=collections.Counter();number=0
    for config in CONFIGURATIONS:
        for name in SCENARIOS:
            inp=recipe(modules,helpers,name)
            first=helpers['session_animation'].sc(run_case(modules,helpers,config,name,inp))
            second=helpers['session_animation'].sc(run_case(modules,helpers,config,name,inp))
            if first!=second:raise AssertionError('fresh oracle divergence')
            row=('\t'.join([name,str(config),str(number)+';'+helpers['session_animation'].sc(inp),'ok',first])+'\n').encode('ascii')
            lines.append(row);count[name]+=1;number+=1
    data=b''.join(lines);trace=b''.join(lines[1:]);fnv=14695981039346656037
    for byte in trace:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
    compressed=gzip.compress(data,mtime=0)
    manifest={'schema_version':1,'suite':'session-anim-cache','reference_sha256':REFERENCE_SHA256,
      'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'ordering_evidence':ORDERING_EVIDENCE,
      'helpers':HELPERS,'composition_policy':COMPOSITION_POLICY,'input_protocol':INPUT_PROTOCOL,
      'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
      'vector_count':number,'vector_sha256':hashlib.sha256(data).hexdigest(),
      'compressed_sha256':hashlib.sha256(compressed).hexdigest(),'expected_trace_sha256':hashlib.sha256(trace).hexdigest(),
      'expected_trace_fnv1a64':f'{fnv:016x}','expanded_bytes':len(data),'max_line_bytes':max(map(len,lines)),
      'operations':dict(sorted(count.items())),'replicas_per_case':2,
      'settings':{'models':['ps2','ps2g1','chop','nearest'],'mul_bias':[False,True],'div_nearest':[False,True]}}
    pending=[]
    try:
        for dest,payload in ((output,compressed),(output.with_name(output.name[:-7]+'.json'),(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode())):
            fd,path=tempfile.mkstemp(prefix='.'+dest.name+'.',dir=dest.parent);pending.append((path,dest))
            with os.fdopen(fd,'wb') as stream:stream.write(payload);stream.flush();os.fsync(stream.fileno())
        for temp,dest in pending:os.replace(temp,dest)
    finally:
        for temp,_ in pending:pathlib.Path(temp).unlink(missing_ok=True)
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_anim_cache.tsv.gz'))
    args=parser.parse_args();print(json.dumps(generate(args.research_dir,args.output),sort_keys=True))
