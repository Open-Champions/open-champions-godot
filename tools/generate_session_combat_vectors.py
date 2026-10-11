#!/usr/bin/env python3
"""Independent original-model composition for bounded shared-stream RTA combat.

Only the host wiring, supplied tasks and observation format are authored here.
All arithmetic, network, scheduler, damage and HP bodies are immutable references.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import pathlib
import struct
import sys
import types

HELPERS = {
    'session_hp': {'path':'tools/generate_session_hp_vectors.py','sha256':'8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'},
    'session_objects': {'path':'tools/generate_session_objects_vectors.py','sha256':'05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'},
    'combat_snapshot': {'path':'tools/generate_combat_creature_vectors.py','sha256':'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
}
COMPOSITION_POLICY='rta-session-combat-v1'
SCENARIOS=('fixed_hit','ranged_hit','miss','armor_split','range_gate','blocked','delayed_hold','pool_failure','post_failure','legacy_isolation','opposite','invulnerable','domain_refusal')
VARIANTS={n:((0,1,2) if n=='blocked' else (0,1) if n in ('fixed_hit','ranged_hit','miss','armor_split','range_gate','delayed_hold') else (0,)) for n in SCENARIOS}
FPU_SCENARIOS=set(SCENARIOS)-{'blocked','domain_refusal'}


def load_helpers():
    root=pathlib.Path(__file__).resolve().parents[1];out={}
    for name,pin in HELPERS.items():
        path=root/pin['path'];source=path.read_bytes()
        if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('authored helper hash mismatch: '+name)
        module=types.ModuleType('session_combat_helper_'+name);module.__file__=str(path)
        sys.modules[module.__name__]=module;exec(compile(source,str(path),'exec'),module.__dict__);out[name]=module
    return out


class DrawView:
    """A live view, never a copy or replacement of the original shared LCG."""
    def __init__(self,authority):self.authority=authority;self.draws=[]
    @property
    def state(self):return self.authority.state
    def draw(self):
        value=self.authority.draw();self.draws.append([value,self.authority.state]);return value


def make_oracle(modules,helpers,config,variant,scenario,local=0):
    hp,so=helpers['session_hp'],helpers['session_objects'];core,combat,wire=(modules[k] for k in ('core','combat','netwire'))
    base='ready'
    if scenario=='blocked':base=('no_ready','pending','no_sim')[variant]
    if scenario=='delayed_hold':base=('delayed','hold')[variant]
    o=hp.make_oracle(modules,helpers,config,min(variant,1),base)
    o.core=core;o.local=local;o.scenario_name=scenario;o.variant=variant
    o.legacy_initial=o.combat_seed;o.legacy=combat.Seed(o.combat_seed)
    o.fallback=o.hp.world.seed;o.draw_view=DrawView(o.w.shared);o.hp.world.seed=o.draw_view
    o.focus=[0,0] if not (scenario=='fixed_hit' and variant) else [core.from_int(1000),0]
    o.cutscene=scenario=='ranged_hit' and bool(variant)
    o.hp.world.focus=list(o.focus);o.hp.world.cutscene=o.cutscene;o.hp.world.network_mode=1
    def unavailable(*_):raise hp.Refusal('unexpected service')
    for name in ('stat_fn','stat_for_fn','adjust_fn','pool_fn','enemy_fn'):setattr(o.hp.world,name,unavailable)
    o.configured=[1,2];o.hp.objects[0].armor=100 if scenario in ('miss','armor_split') else 0
    if scenario=='invulnerable':o.hp.objects[0].cflags=combat.CF_INVULNERABLE;o.project_hp()
    if local:
        o.network=type(o.network)(2,local,wire.Fpu(hp.MODELS[config//4],bool(config&2),bool(config&1)))
        o.network.network_mode=1;o.network.num_players=2;o.network.run_tasks=o.ready_tick;o.publish_world()
    original_snapshot=o.snapshot
    def snapshot():
        result=original_snapshot()
        # Project dormant fallback by value; never swap/mutate the live seed.
        world=result[1][5][11]
        world[0]=[o.fallback.state,list(o.fallback.draws)]
        world.append(['objects.shared',o.w.shared.state,copy.deepcopy(o.draw_view.draws)])
        result[1].append([[list(o.focus),o.cutscene],list(o.configured)])
        result[0][0][0][5]=o.local;result[0][0][0][7]=o.legacy_initial
        result[0][0][13]=o.legacy.state;result[0][0][14]=list(o.legacy.draws)
        return result
    o.snapshot=snapshot
    def apply(minimum,maximum,attack,flags):
        def action():
            if o.phase!='tasks' or not o.object_run_completed or o.task_priority<=10:raise hp.Refusal('phase')
            if flags & ~(0x40|0x4000|0x8000):raise hp.Refusal('flags')
            for entry in o.hp_bindings:
                known=o.retained[entry[1]-1]
                if o.w.objects.get(entry[2]) is not known or not known.alive:raise hp.Refusal('retired or reused')
                if not known.flags&0x20 or known.flags&(0x40|0x80|0x10000000):raise hp.Refusal('kind')
            if o.hp.objects[0].cflags&(combat.CF_BLOCKING|combat.CF_THORNS):raise hp.Refusal('blocking or thorns')
            if not o.raw_snapshot()[0]:raise hp.Refusal('physical unavailable')
            for record,entry in zip(o.hp.objects,o.hp_bindings):
                obj=o.retained[entry[1]-1];record.flags=obj.flags;record.pos=list(obj.pos)
            o.hp.world.frame=o.w.frame;o.calls_this_tick+=1;o.hp_phase='damage_apply';o.hp_depth=1;o.hp.depth+=1
            try:result=combat.damage_apply(o.hp.world,o.hp.objects[0],o.hp.objects[1],minimum,maximum,attack,flags,None,1)
            finally:
                o.hp.depth-=1;o.hp_depth=0
                for record,entry in zip(o.hp.objects,o.hp_bindings):
                    obj=o.retained[entry[1]-1];obj.flags=record.flags;obj.pos=list(record.pos)
                o.w.frame=o.hp.world.frame
            o.hp_phase='idle';o.hp_calls+=1;return result
        return o.guard('apply_base_damage',action)
    o.apply=apply
    def task(fn):
        name=next(t[1] for t in hp.TASKS if t[0]==fn);o.host_events.append(['task',name,o.network.net_frame,[]]);o.effect(name)
        if name=='hp_pre':
            # The ordinary object stream is consumed directly on both sides of combat.
            o.effect('shared_pre',o.w.shared.draw())
            if scenario=='armor_split':
                for _ in range((0,1)[variant]):o.effect('shared_extra',o.w.shared.draw())
            if scenario=='pool_failure':
                for _ in range(o.w.handles.count_free()-1):o.w.handles.add(o.w.always_draw,None)
            if scenario=='legacy_isolation':
                value=o.legacy.draw();o.host_events.append(['draw','',o.network.net_frame,[value,o.legacy.state]]);o.effect('legacy_draw',value)
        elif name=='hp_apply':
            if o.w.pads[0].held&1:
                minimum=maximum=0x40000000;attack=100;flags=0x40
                if scenario=='fixed_hit' and variant:minimum=maximum=0x3f000001;flags|=0x4000
                if scenario=='ranged_hit':minimum,maximum=0x3fa00000,0x40b00000;flags|=0x4000 if variant else 0
                if scenario=='miss':attack=0;flags|=0x8000 if variant else 0
                if scenario=='armor_split':attack=25
                if scenario=='range_gate':minimum,maximum=(0x40400000,0x3f800000) if variant else (0,0)
                if scenario=='domain_refusal':flags=1
                result=o.apply(minimum,maximum,attack,flags);o.effect('damage_result',result)
        else:
            o.effect('shared_post',o.w.shared.draw())
            if scenario=='post_failure':raise hp.TaskFailure('post')
    o.task=task
    return o


def submit(o,helpers,k,inbox,scenario,variant):
    hp=helpers['session_hp']
    def action():
        o.started=True
        sample=struct.pack('<H6B',1,(128+k)&255,(64+3*k)&255,(255-k)&255,(17*k)&255,(31*k)&255,(47*k)&255)
        hold=scenario=='delayed_hold' and variant==1
        return o.network.net_main(sample,yaw=(k*1234)&65535,camera_flag=k&1,frame_step=4 if hold and k==0 else 1,
            no_sim=(scenario=='blocked' and variant==2) or (hold and k==0),hold_arg=hold and k>0,render_frame=k,inbox=inbox)
    return o.guard('submit',action)


def capture_step(o,helpers,k,inbox,scenario,variant,previous):
    hp,so=helpers['session_hp'],helpers['session_objects']
    try:
        datagrams,held,steps=submit(o,helpers,k,inbox,scenario,variant);result=[[so.sent(x) for x in datagrams],held,steps]
    except (hp.Refusal,hp.TaskFailure,o.core.HandleError) as e:
        datagrams=[];result=['error',type(e).__name__]
    current=copy.deepcopy(o.snapshot());delta=hp.snapshot_delta(previous,current)
    if hp.apply_delta(previous,delta)!=current:raise AssertionError('snapshot delta not lossless')
    return [result,o.step_view(),delta],current,datagrams


def run_scenario(modules,helpers,config,variant,scenario):
    hp,so=helpers['session_hp'],helpers['session_objects']
    if scenario=='opposite':return run_opposite(modules,helpers,config)
    o=make_oracle(modules,helpers,config,variant,scenario)
    try:
        if scenario=='blocked' and variant==1:o.network.level_pending=True
        initial=copy.deepcopy(o.snapshot());previous=initial;steps=[];packet=so.ready_packet(modules['netwire'],config)
        for k in range(4):
            if scenario=='blocked' and variant==1 and k==2:o.network.level_pending=False
            delayed=scenario=='delayed_hold' and variant==0
            no_ready=scenario=='blocked' and variant==0
            inbox=[packet] if (delayed and k==2) or (not delayed and not no_ready and k==0) else []
            step,previous,_=capture_step(o,helpers,k,inbox,scenario,variant,previous);steps.append(step)
        return [initial,steps,o.events,o.step_effects]
    finally:o.close()


def run_opposite(modules,helpers,config):
    hp=helpers['session_hp'];peers=[];instrumentation=[]
    # The pinned authored helper instruments allocation identities with subclasses.
    # Each peer keeps its own instrumentation; original bodies remain unchanged.
    for i in (0,1):
        o=make_oracle(modules,helpers,config,0,'opposite',i);peers.append(o)
        instrumentation.append((modules['objects'].GameObject,modules['core'].Iterator));o.close()
    def activate(i):modules['objects'].GameObject,modules['core'].Iterator=instrumentation[i]
    try:
        initial=[]
        for i,o in enumerate(peers):activate(i);initial.append(copy.deepcopy(o.snapshot()))
        previous=copy.deepcopy(initial);steps=[];queues=[[],[]];completed=[{},{}]
        # Deliberately asymmetric cadence and delayed delivery; both original Machines run.
        for turn,which in enumerate((0,0,1,0,1,1,0,1,0,1,0,1)):
            activate(which);o=peers[which];inbox=queues[which] if turn>=3 else [];queues[which]=[] if turn>=3 else queues[which]
            old_calls=o.ready_calls;step,state,sent=capture_step(o,helpers,turn,inbox,'opposite',0,previous[which]);previous[which]=state
            for packet in sent:queues[packet.peer].append(packet.data)
            if o.ready_calls>old_calls:
                completed[which][o.network.net_frame]=[o.w.shared.state,[obj.hp for obj in o.hp.objects],copy.deepcopy(o.draw_view.draws)]
            steps.append([which,step])
        common=sorted(set(completed[0])&set(completed[1]));equal=[[frame,completed[0][frame]==completed[1][frame]] for frame in common]
        return [initial,steps,[o.events for o in peers],[queues,equal]]
    finally:
        for o in peers:o.close()


def check_case(value,scenario,variant,helpers):
    hp=helpers['session_hp']
    if scenario=='opposite':
        if not value[3][1] or not all(row[1] for row in value[3][1]):raise AssertionError('opposite endpoints lack matching completed ticks')
        return
    state=copy.deepcopy(value[0]);states=[]
    for step in value[1]:state=hp.apply_delta(state,step[2]);states.append(copy.deepcopy(state))
    world=state[1][5][11];authority=world[7]
    if world[0]!=[0,[]] or world[1]!=[0,[]]:raise AssertionError('dormant adapter RNG changed')
    if state[0][1][1]!=authority[1] or authority[1]!=state[0][0][8][0][19]:raise AssertionError('shared RNG identity diverged')
    if scenario!='legacy_isolation' and state[0][0][14]:raise AssertionError('legacy stream changed')
    if scenario=='fixed_hit' and variant==0:
        if [row[1][4][0] for row in value[1]]!=[0x42c80000,0x42c40000,0x42c00000,0x42bc0000]:raise AssertionError('fixed damage HP progression')
        if states[1][1][5][11][7][2]!=[[46195,3027450565],[3312,217083232],[24216,1587069247],[50774,3327581586],[36450,2388811721],[1080,70837908]]:raise AssertionError('shared draw literal interleave')
    if scenario=='armor_split' and variant==0:
        # First miss/second hit consumes two draws; later first hit/second miss
        # consumes six. These independent counts distinguish both hit tests.
        ledgers=[s[1][5][11][7][2] for s in states]
        if [len(v) for v in ledgers]!=[0,2,4,10]:raise AssertionError('armor hit-stage split draw order')
        outcomes=[e[1] for e in value[2] if e[0]=='damage_result']
        if outcomes!=[1,1,0]:raise AssertionError('armor first/second hit split outcome')
    if scenario=='invulnerable' and authority[2]:raise AssertionError('invulnerable consumed combat RNG')
    if scenario=='legacy_isolation':
        if len(state[0][0][14])!=3:raise AssertionError('legacy independent draw count')
        if len(authority[2])!=18:raise AssertionError('legacy draw altered shared combat history')
    if scenario=='pool_failure' and (len(authority[2])!=6 or state[1][4]!=1):raise AssertionError('post-draw HP failure lost prefix')
    if scenario in ('pool_failure','post_failure','domain_refusal'):
        if not state[0][0][2] or states[-1]!=states[-2]:raise AssertionError('failure latch changed after retry')
    if scenario in ('miss','invulnerable') or (scenario=='blocked' and variant in (0,2)):
        if value[1][-1][1][4][0]!=0x42c80000:raise AssertionError('blocked/miss/invulnerable changed HP')


def generate(research_dir,output):
    helpers=load_helpers();hp=helpers['session_hp'];modules=hp.load_references(research_dir);so=helpers['session_objects'];output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=0;size=0;maxline=0
    with output.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as f:
        header=b'# session_combat_vectors_v1\n';f.write(header);vh.update(header);size+=len(header)
        for name in SCENARIOS:
            for config in range(16) if name in FPU_SCENARIOS else (0,):
                for variant in VARIANTS[name]:
                    value=run_scenario(modules,helpers,config,variant,name);check_case(value,name,variant,helpers)
                    first=so.sc(value);second=so.sc(run_scenario(modules,helpers,config,variant,name))
                    if first!=second:raise AssertionError('fresh replay disagreement')
                    line=('\t'.join((name,str(config),f'{number};{variant}','ok',first))+'\n').encode('ascii');f.write(line);vh.update(line);th.update(line);size+=len(line);maxline=max(maxline,len(line))
                    for b in line:fnv=((fnv^b)*1099511628211)&0xffffffffffffffff
                    counts[name]+=1;number+=1
    if size>64*1024*1024 or maxline>1024*1024:raise AssertionError('fixture exceeds runner resource contract')
    manifest={'schema_version':1,'suite':'session-combat','references':hp.REFERENCES,'reference_sha256':hp.REFERENCE_SHA256,'composition_policy':COMPOSITION_POLICY,'helpers':HELPERS,'ordering_evidence':hp.ORDERING_EVIDENCE,'specification_evidence':hp.SPECIFICATION_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'recipe_configurations':{n:list(range(16)) if n in FPU_SCENARIOS else [0] for n in SCENARIOS},'recipe_variants':{n:list(VARIANTS[n]) for n in SCENARIOS},'uncompressed_bytes':size,'maximum_line_bytes':maxline}
    manifest['composition_contract']={'scope':'RTA configured nonplayer EMPTY_DESC/no_crit base damage only; no player, spell, reflection, DOT, derived dispatch or Norrath claim','oracle':'Five immutable SHA-verified original Python modules; three pinned authored snapshot helpers; no C++ expected-output source','random_authority':'Direct DrawView delegates original object World.shared.draw and records each value/state pair; no state mirroring. Legacy Session and dormant Adapter fallback/cosmetic seeds remain separate.','snapshots':'Complete SessionHP snapshot plus base config/configured IDs and shared authority/current state/attributed draws; lossless deltas are reconstructed before comparison. Opposite peers retain each complete local state.','errors':'Original model mutation prefix retained; authored phase/domain/resource guards and failure latch identified separately','determinism':'Every case executed twice from fresh model instances; standalone regeneration is independently repeatable with deterministic gzip'}
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8',newline='\n');return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_combat.tsv.gz'));a=p.parse_args();print(json.dumps(generate(a.research_dir,a.output),sort_keys=True))
if __name__=='__main__':main()
