#!/usr/bin/env python3
"""Independent cache-free animation clock vectors from immutable RTA models.

Only timing metadata and operation recipes are authored here. Track mutation, events,
raw arithmetic and cosmetic random draws execute the pinned Python functions.
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
    'anim': {'commit': 'f516ac798bf28375554fe53ffb31b74802844136', 'path': 'tools/ref_anim.py', 'sha256': '6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'f2b996dded898f1b0b558b901cab875f775cfdc7', 'path': 'docs/spec/animation.md', 'sha256': 'be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab'}
COMPOSITION_POLICY = 'rta-animation-clock-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')

def load_references(research_dir, scratch_dir):
    """Never import a live research working-tree file or follow its module path."""
    scratch_dir = pathlib.Path(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)
    modules = {}
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    old_bytecode = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        with tempfile.TemporaryDirectory(prefix='pinned-', dir=scratch_dir) as directory:
            for name in ('core', 'anim', 'specification'):
                pin = SPECIFICATION_EVIDENCE if name == 'specification' else REFERENCES[name]
                source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']],
                                        capture_output=True, check=True, timeout=30).stdout
                if hashlib.sha256(source).hexdigest() != pin['sha256']:
                    raise ValueError('immutable reference hash mismatch: ' + name)
                path = pathlib.Path(directory) / pathlib.Path(pin['path']).name
                path.write_bytes(source)
                if name == 'specification': continue
                module = types.ModuleType('pinned_animation_clock_' + name)
                module.__file__ = str(path)
                sys.modules[module.__name__] = module
                exec(compile(source, str(path), 'exec'), module.__dict__)
                modules[name] = module
                if name == 'core': sys.modules['ref_core'] = module
    finally:
        sys.dont_write_bytecode = old_bytecode
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core', None)
        else: sys.modules['ref_core'] = previous
    return modules


SCENARIOS = (
    'anim_clear', 'anim_push', 'anim_push_with_weights', 'anim_push_chained',
    'anim_start', 'anim_start_blend_in_out', 'anim_stop', 'anim_drop_non_persistent',
    'anim_release_caches', 'anim_state_dtor', 'anim_sync_to_sim_frame',
    'anim_randomize_top_time', 'anim_sync_top_to_below', 'anim_jump_top_to_end',
    'anim_snap_top_to_end', 'anim_find_track', 'anim_find_track_by_flag',
    'anim_fade_in_track', 'anim_fade_out_track', 'anim_set_track_weight',
    'anim_state_copy', 'anim_advance')
VARIANTS = 12
CONFIGURATIONS = tuple(range(16))
LOOP_LIMIT = 65536
RAW = (0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000001, 0x3c888887,
       0x3c888888, 0x3c888889, 0x40000000, 0x00000001, 0x7f800000, 0xff800000)


def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned clock state')


# Opcode 0..21 is SCENARIOS order. Every API command starts with state index.
# 22=[state,count], 23=[state,cell,field,value], 24=[clipID,event,seconds,mask].
# Track field order is the complete snapshot's nine-cell order.
# 20(copy)=[source,destination]; 21(advance)=[state,speed].
# Other API args follow the model except state comes first and clip is a registry ID.
# Pointer results are cell indices; copy returns destination state index.


def recipe(modules, variant, scenario):
    C,A=modules['core'],modules['anim']
    clips=[[i+1,length,(0,0xffffffffffffffff,0x8000000000000001)[i],
            [[0,0x80],[A.K60,1],[0x3d088888,0x40],[0xbf800000,0x20]],i==2]
           for i,length in enumerate((8,3,17))]
    if variant==8:
        clips[2]=[3,clips[0][1],clips[0][2],[list(e) for e in clips[0][3]],clips[0][4]]
    tracks=[[i%3+1,0,C.from_int(i+1),0,A.ONE,0,C.from_int(i),
             (0,A.PERSISTENT,A.KEEP,0x8020)[i],(-32768,-1,32767,71)[i]] for i in range(4)]
    frames=(0,1,59,60,61,0x7fffffff,0x80000000,0xffffffff,16777217,999999,120,257)
    initial=[[frames[variant],(0,1,0xdeadbeef,0xffffffff)[variant%4]],[LOOP_LIMIT],clips,
             [[0,3,4,0xa5a5,tracks],[0,0,1,0,[[0,0,0,0,0,0,0,0,-123]]]]]
    commands=[]
    def call(op,*args):commands.append([op,*args])
    def count(value):call(22,0,value)
    def track(cell,field,value):call(23,0,cell,field,value)
    def advance(speed=A.ONE):call(21,0,speed)
    parameter=RAW[variant]
    target=variant%3+1 if variant!=9 else 0
    flags=(0,A.LOOP,A.REPORT_END,A.PERSISTENT,A.PAUSED,A.FIXED_STEP,A.KEEP,
           A.PURGE_BELOW,A.EASE,0x8000,0x10020,A.LOOP|A.KEEP)[variant]
    op=SCENARIOS.index(scenario)
    if op==0:
        call(0,0);call(3,0,target,flags);call(1,0,2,flags)
    elif op in (1,2,3):
        if variant==0:count(0)
        if variant==1:count(4)
        if variant==2:track(2,6,C.from_int(20))
        if variant==3:track(2,6,0xbf800000)
        args=[0,target,flags]
        if op==2:args += [parameter,RAW[(variant+4)%12],RAW[(variant+8)%12],0xbf000001]
        call(op,*args);call(op,*args)
    elif op in (4,5):
        if variant%3==0:count(0)
        if variant%3==1:count(4)
        call(op,0,target,flags,parameter,*([RAW[(variant+3)%12]] if op==5 else []))
        advance()
    elif op==6:
        track(2,0,1);track(0,7,A.REPORT_END|A.PERSISTENT);track(2,7,A.KEEP|A.REPORT_END|A.PERSISTENT)
        call(6,0,target,parameter);advance()
    elif op in (7,8,9):
        if variant%3==0:count(0)
        if variant%3==1:count(4)
        call(op,0);call(op,0)
    elif op in (10,11,12,13):
        n=variant%5;count(n)
        if n>=2:track(n-2,6,C.from_int((1,3,17,40)[variant%4]))
        call(op,0);call(op,0)
    elif op==14:
        count(variant%5);call(op,0);advance()
    elif op in (15,16):
        n=variant%5;count(n)
        if op==15:
            call(op,0,target);call(op,-1,target)
        else:
            if n:track(0,7,0x8000)
            call(op,0,(1,0x8000,0xffff0000,0)[variant%4])
    elif op in (17,18,19):
        track(0,0,1);track(0,2,parameter);track(0,4,RAW[(variant^1)%12])
        call(op,0,0 if variant==9 else 1,parameter);advance()
    elif op==20:
        count(variant%5);call(20,0,1)
        call(24,1,0,0,0xff);call(24,1,1,parameter,1)
        call(0,0);call(21,1,A.ONE)
    elif op==21:
        track(2,7,flags);track(2,0,variant%3+1)
        track(2,6,(0,0x41000000,0x41880000,0,0,0,0,0,0,0,0xbf800000,0x42100000)[variant])
        if variant in (1,11):track(2,0,2)
        if variant==2:track(2,7,A.REPORT_END)
        if variant==3:track(2,4,0);track(2,5,0)
        if variant==6:track(2,4,0);track(2,5,0xbf800000)
        if variant==7:track(2,7,A.PURGE_BELOW);track(2,2,A.ONE)
        if variant==8:track(2,3,0xbf800000);track(2,2,0)
        if variant==9:track(2,7,0x8000|A.PAUSED)
        if variant==10:track(2,6,0xc0000000);track(2,3,A.ONE)
        speed=0x3f000001 if variant%2 else A.ONE
        advance(speed);advance(speed);call(24,1,0,0,0xff);advance(0)
    return [initial,commands]


def execute(modules, config, inputs):
    C,A=modules['core'],modules['anim']
    C.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
    initial,commands=inputs
    clips=[]
    for identity,length,mask,events,sound in initial[2]:
        assert identity==len(clips)+1
        clip=object.__new__(A.Clip)
        clip.length,clip.joint_mask=length,mask
        clip.events=[list(e) for e in events]
        clip.sound_times=[0,0x7fffffff] if sound else None
        clips.append(clip)
    def clip(index):return clips[index-1] if index else None
    states=[]
    fields=('clip','slot','weight','rate','life','life_rate','time','flags','sound_idx')
    for pose,count,capacity,flags,tracks in initial[3]:
        assert pose==0
        state=A.AnimState(capacity);state.count=count;state.flags=flags
        for dst,src in zip(state.tracks,tracks):
            assert src[1]==0
            for name,value in zip(fields,src):setattr(dst,name,clip(value) if name=='clip' else None if name=='slot' else value)
        states.append(state)
    system=types.SimpleNamespace(sim_frame=initial[0][0],cosmetic_seed=initial[0][1])
    def snapshot():
        assert all(s.pose is None and all(t.slot is None for t in s.tracks) for s in states)
        def cid(value):return 0 if value is None else next(i+1 for i,c in enumerate(clips) if c is value)
        return [[system.sim_frame,system.cosmetic_seed],list(initial[1]),
                [[i+1,c.length,c.joint_mask,[list(e) for e in c.events],c.sound_times is not None] for i,c in enumerate(clips)],
                [[0,s.count,s.capacity,s.flags,[[cid(t.clip),0,t.weight,t.rate,t.life,t.life_rate,t.time,t.flags,t.sound_idx] for t in s.tracks]] for s in states]]
    assert snapshot()==initial
    steps=[]
    for command in commands:
        op,*args=command
        state=None if args[0]==-1 else states[args[0]] if op!=24 else None
        try:
            if op==22:state.count=args[1];value=None
            elif op==23:
                _,cell,field,new=args
                assert field!=1
                setattr(state.tracks[cell],fields[field],clip(new) if field==0 else new);value=None
            elif op==24:
                identity,event,seconds,mask=args;clip(identity).events[event]=[seconds,mask];value=None
            elif op in (0,7,8,9,14):value=getattr(A,SCENARIOS[op])(state)
            elif op in (1,3):value=getattr(A,SCENARIOS[op])(state,clip(args[1]),args[2])
            elif op==2:value=A.anim_push_with_weights(state,clip(args[1]),*args[2:])
            elif op==4:value=A.anim_start(args[3],state,clip(args[1]),args[2])
            elif op==5:value=A.anim_start_blend_in_out(args[3],args[4],state,clip(args[1]),args[2])
            elif op in (6,17,18,19):value=getattr(A,SCENARIOS[op])(args[2],state,clip(args[1]))
            elif op in (10,11,12,13):value=getattr(A,SCENARIOS[op])(system,state)
            elif op==15:value=A.anim_find_track(state,clip(args[1]))
            elif op==16:value=A.anim_find_track_by_flag(state,args[1])
            elif op==20:states[args[1]]=A.anim_state_copy(system,state);value=args[1]
            elif op==21:value=A.anim_advance(system,args[1],state,False,False,False)[0]
            else:raise ValueError('unknown opcode')
            if isinstance(value,A.Track):value=state.tracks.index(value)
        except A.HostFault:value='HostFault'
        steps.append([value,snapshot()])
    return [initial,commands,steps]


def run_case(modules,config,variant,scenario):
    return execute(modules,config,recipe(modules,variant,scenario))


def independent_anchors(modules):
    clear=run_case(modules,0,0,'anim_clear')
    assert clear[2][0][1][3][0][1]==0
    assert clear[2][0][1][3][0][4]==clear[0][3][0][4]
    assert clear[2][1][0]=='HostFault'
    chain=run_case(modules,0,0,'anim_push_chained')
    assert chain[2][1][0]=='HostFault'
    events=run_case(modules,0,0,'anim_advance')
    assert events[2][3][0]==-127  # event masks 0x80 | 0x01, signed byte
    random=run_case(modules,0,1,'anim_randomize_top_time')
    assert random[2][1][1][0][1]==0x3c88596c
    copied=run_case(modules,0,3,'anim_state_copy')
    assert copied[2][1][1][3][0]==copied[2][1][1][3][1]
    A=modules['anim']
    assert A.fmin(0,0x80000000)==0 and A.fmin(0x80000000,0)==0x80000000
    assert A.fmax(0,0x80000000)==0 and A.fmax(0x80000000,0)==0x80000000


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'animation-clock-generator'
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
                header=b'# animation_clock_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for config in CONFIGURATIONS:
                    for variant in range(VARIANTS):
                        for scenario in SCENARIOS:
                            first=sc(run_case(modules,config,variant,scenario))
                            if first!=sc(run_case(modules,config,variant,scenario)):
                                raise AssertionError('fresh reference executions diverged')
                            row=('\t'.join([scenario,str(config),str(number)+';'+sc(recipe(modules,variant,scenario)),'ok',first])+'\n').encode('ascii')
                            stream.write(row);vh.update(row);th.update(row)
                            for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                            counts[scenario]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'animation-clock','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'helpers':{},
          'input_protocol':{'version':1,'columns':['operation','config','id;setup-and-command-DSL','status','observation'],
          'setup':'[system,limits,clip_registry,states]; complete state schema equals each observed snapshot',
          'commands':'0..21 in modeled_operations order, state first; 22(state,count),23(state,cell,field,value),24(clip,event,seconds,mask)',
          'observation':'[initial_snapshot,submitted_commands,[[result_or_HostFault,complete_snapshot],...]]'},
          'modeled_operations':list(SCENARIOS),
          'composition_contract':{'scope':'22 original cache-free clock operations. Timing metadata only; no clip stream decoder, ROOT/YAW, cache, pose, sound execution, controller or gameplay binding.',
          'state':'Stable clip IDs with four mutable event slots and sound-table presence; complete track capacity cells, both states and system frame/cosmetic seed. All omitted model pools remain unused constants, not simulated state.',
          'errors':'HostFault records the original exception category and retained state; native policy refusals and bounded LOOP exhaustion remain separate tests and are not invented original outcomes.',
          'formula':'Chaining executes (below.time - length) + 2 exactly; nearby original prose says after but executable formula controls.',
          'publication':'Sibling temporary files flushed and fsynced, fixture replaced then manifest last. Interrupted pair publication is detectable by hashes, not a two-file transaction.'}}
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
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/animation_clock.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
