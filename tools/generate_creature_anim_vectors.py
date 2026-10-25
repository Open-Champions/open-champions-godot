#!/usr/bin/env python3
"""Generate authored synthetic inputs and exact expected observations from pinned Git blobs.
Never reads native output or private game data. The reference files stay outside the repo.
"""
import argparse, gzip, hashlib, importlib, json, pathlib, subprocess, sys, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
PINS=ROOT/'native/tests/vectors/creature_anim_sources.json'

def reference(repo, destination):
    pins=json.loads(PINS.read_text())
    for name,digest in pins['sha256'].items():
        data=subprocess.check_output(['git','-C',repo,'show',pins['commit']+':'+name])
        if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('source hash mismatch: '+name)
        if name.startswith('tools/'):
            (destination/pathlib.Path(name).name).write_bytes(data)
    sys.path.insert(0,str(destination))
    return importlib.import_module('ref_core'),importlib.import_module('ref_anim')

def generate(C,A):
    cases=[]
    M=importlib.import_module("ref_monsters_1");RC=importlib.import_module("ref_creature")
    class Case:
        def __init__(self,name,mode=0,bias=0,divn=0,slots=6,poses=3):
            self.name=name; self.lines=[];self.output=[];self.clips=[];self.states=[]
            self.command('init',mode,bias,divn,slots,poses)
        def command(self,op,*args):
            self.lines.append(' '.join(map(str,(op,)+args)))
            def state(i): return self.states[i-1]
            def track(i): return [t for s in self.states for t in s.tracks][i-1]
            result=None
            if op=='init':
                mode,bias,divn,slots,poses=args;C.set_fpu(('ps2','ps2g1','chop','nearest')[mode],bool(bias),bool(divn));self.system=A.AnimSystem(slots,poses)
            elif op=='clip': self.clips.append(A.Clip(bytes.fromhex(args[0])))
            elif op=='state': self.states.append(A.AnimState(args[0]))
            elif op=='frames': self.system.sim_frame,self.system.draw_frame=args
            elif op=='seed': self.system.cosmetic_seed=args[0]
            elif op=='push':
                s,c,f,w,rate,life,lr=args;A.anim_push_with_weights(state(s),self.clips[c-1],f,w,rate,life,lr)
            elif op=='time': track(args[0]).time=args[1]
            elif op=='mask': self.clips[args[0]-1].joint_mask=args[1]
            elif op=='event': self.clips[args[0]-1].events[args[1]]=(args[2],args[3])
            elif op=='sound':
                c,g,limit,count,*times=args;self.clips[c-1].sound_group=g;self.clips[c-1].sound_times=times;self.system.group_limits[g]=limit
            elif op=='advance': result=A.anim_advance(self.system,args[1],state(args[0]),bool(args[2]))
            elif op=='creature':
                i,speed,f,scale,slow,active,x,y,yaw=args
                result=A.creature_step_anim(self.system,speed,state(i),A.s16(f),scale,slow,True,bool(active),(x,y),A.s16(yaw))
            elif op=='draw':
                p=A.anim_emit_pose(self.system,state(args[0]));result=[p.joint_count]+[v for m in p.mats[:p.joint_count] for row in m for v in row]
            elif op=='build': result=[int(A.anim_build_pose(self.system,state(args[0]),bool(args[1])) is not None)]
            elif op=='joint': result=A.anim_get_joint_matrix(self.system,state(args[0]),args[1])
            elif op=='copy': self.states.append(A.anim_state_copy(self.system,state(args[0])))
            elif op=='start': i,c,f,blend=args;A.anim_start(blend,state(i),self.clips[c-1],f)
            elif op=='stop': i,c,blend=args;A.anim_stop(blend,state(i),self.clips[c-1])
            elif op in ('jump','random','sync','below'):
                fn={'jump':A.anim_jump_top_to_end,'random':A.anim_randomize_top_time,'sync':A.anim_sync_to_sim_frame,'below':A.anim_sync_top_to_below}[op];fn(self.system,state(args[0]))
            elif op in ('snap','clear','release','dtor'):
                fn={'snap':A.anim_snap_top_to_end,'clear':A.anim_clear,'release':A.anim_release_caches,'dtor':A.anim_state_dtor}[op];fn(state(args[0]))
            elif op=='reset': A.vu0_reset_stack(self.system)
            elif op=='compose': result=[v for row in A.vu0_compose_joint(self.system,list(args[:4]),list(args[4:7]),list(args[7:10]),C.s32(args[10])) for v in row]
            elif op=='spider_init':
                self.system=A.AnimSystem(8,4);self.states=[A.AnimState(8)]
                self.memory=RC.Memory();mem=self.memory;c=0x1000;d=0x502818;t=0x50fd40
                for address,value in [(c+0x24,d),(d+12,0x9000),(d+20,0xa000),(d+0x3c,0x6000),(d+0x4c,0x6100),(c+0x48,t),(t+27*8+4,0x2ab704),(t+28*8+4,0x10beb8),(c+0x300,100)]:mem.poke(address,4,value)
                for i in range(4):mem.poke(d+0x1c+8*i,4,0x6200+0x100*i)
                for i,clip in enumerate(self.clips):
                    for j,(sec,mask) in enumerate(clip.events):mem.poke(0x6000+0x100*i+0x18+8*j,1,mask);mem.poke(0x6000+0x100*i+0x14+8*j,4,sec)
                st=self.states[0]
                def publish():mem.store(c+0x11a,2,st.flags)
                def push(anim,resource,flags):A.anim_push(st,self.clips[(resource-0x6000)//0x100],flags);publish()
                def start(anim,resource,flags,blend):A.anim_start(blend,st,self.clips[(resource-0x6000)//0x100],flags);publish()
                def random(anim):
                    self.system.cosmetic_seed=mem.load(0x404460,4);A.anim_randomize_top_time(self.system,st);mem.store(0x404460,4,self.system.cosmetic_seed)
                def advance(anim,sound,root,yaw,speed):
                    for i,clip in enumerate(self.clips):clip.events=[(mem.load(0x6000+0x100*i+0x14+8*j,4),mem.load(0x6000+0x100*i+0x18+8*j,1)) for j in range(4)]
                    ev,x,y,z=A.anim_advance(self.system,speed,st,bool(sound));mem.store(root,4,x);mem.store(root+4,4,y);mem.store(yaw,2,z);publish();return ev&0xffffffff
                self.monster=M.Model(mem,{'anim_push':push,'anim_start':start,'anim_randomizeTopTime':random,'anim_advance':advance,
                    'anim_jumpTopToEnd':lambda a:A.anim_jump_top_to_end(self.system,st),
                    'anim_snapTopToEnd':lambda a:A.anim_snap_top_to_end(st),
                    'anim_releaseCaches':lambda a:A.anim_release_caches(st)})
            elif op=='poke': self.memory.poke(*args)
            elif op=='spider_state': self.monster.smallspider_set_state(0x1000,args[0])
            elif op=='spider_construct': self.monster.smallspider_ctor(0x1000,0,[0,0,0],0,0)
            elif op=='spider_touch': self.monster.smallspider_on_touch(0x1000,args[0])
            elif op=='spider_tick':
                self.system.sim_frame,self.system.draw_frame=args;self.memory.poke(0x44fdcc,4,args[0]);self.monster.smallspider_run(0x1000);A.anim_emit_pose(self.system,self.states[0])
            elif op=='spider_snapshot':
                self.output.append(' '.join(map(str,self.snapshot())))
                result=[len(self.memory.bytes)]+[v for pair in sorted(self.memory.bytes.items()) for v in pair]+[len(self.memory.log)]+[v-(1<<64) if v>=(1<<63) else v for row in self.memory.log for v in row]
            elif op=='snapshot': result=self.snapshot()
            else: raise ValueError(op)
            if result is not None:self.output.append(' '.join(map(str,result)))
        def snapshot(self):
            s=self.system;tracks=[t for st in self.states for t in st.tracks];pool=s.poses;poses=pool.buffers+[pool.fallback,pool.default]
            def ident(values,value):return 0 if value is None else next(i+1 for i,v in enumerate(values) if v is value)
            out=[s.sim_frame,s.cosmetic_seed,s.draw_frame,len(self.states)]
            for i,st in enumerate(self.states):out.extend([i+1,ident(poses,st.pose),st.count,st.capacity,st.flags]);out.extend(ident(tracks,t) for t in st.tracks)
            out.append(len(tracks))
            for t in tracks:out.extend([ident(self.clips,t.clip),ident(s.decoders.slots,t.slot),t.weight,t.rate,t.life,t.life_rate,t.time,t.flags,t.sound_idx])
            for slot in s.decoders.slots:
                out.extend([slot.index,ident(self.clips,slot.clip),slot.cursor,slot.tick,ident(tracks,slot.owner),slot.yaw])
                for j in slot.joints:out.extend(j)
            out.extend(ident(s.decoders.slots,v) for v in s.decoders.order)
            for p in poses:
                out.extend([p.tag,p.stamp,p.joint_count,ident(self.states,p.owner)])
                out.extend(v for m in p.mats for row in m for v in row)
            out.extend(ident(poses,p) for p in pool.order)
            out.extend(v for row in s.vu_mem for v in row)
            out.append(len(s.sound_log))
            for clip,first,end in s.sound_log:out.extend([ident(self.clips,clip),first,end])
            out.extend(v for row in s.cue_throttle for v in row)
            return out
        def clip(self,joints,length=8,**kw):self.command('clip',A.encode_clip(joints,length,**kw).hex());return len(self.clips)
        def finish(self):self.command('snapshot');cases.append({'name':self.name,'input':'\n'.join(self.lines)+'\n','expected':'\n'.join(self.output)+'\n'})
    f=C.f32;one=A.ONE
    for mode in range(4):
      for bias in range(2):
       for divn in range(2):
        suffix=f'{mode}-{bias}-{divn}'
        c=Case('hierarchy-blend-'+suffix,mode,bias,divn)
        joints=[((640,0,0),(0,0,0,4096)),((128,0,0),(4096,0,0,0)),((192,0,0),(4096,0,0,0))]
        c.clip(joints,bind=[(0,0,0),(64,0,0),(0,0,0)])
        c.clip([((0,128,-32),(2048,1024,-512,128))]*3,bind=[(64,-32,0)]*3)
        c.command('state',4);c.command('frames',1,10)
        c.command('push',1,1,A.LOOP,one,0,one,0);c.command('draw',1);c.command('joint',1,1)
        # Cached pose survives start; advance is the invalidation boundary.
        c.command('push',1,2,A.EASE|A.LOOP,f(.5),0,one,0);c.command('draw',1)
        c.command('advance',1,one,0);c.command('draw',1);c.command('mask',2,2)
        c.command('advance',1,f(.25),0);c.command('draw',1)
        c.command('time',2,f(-1));c.command('advance',1,0,0);c.command('draw',1)
        c.command('stop',1,2,0);c.command('draw',1);c.finish()
        c=Case('root-yaw-loop-copy-'+suffix,mode,bias,divn,slots=10)
        c.clip([((64,-32,8),(4096,0,0,0))],length=3,keys=[(0,0,'p',(256,-128,0)),(0,0,'q',(0,0,0,4096)),(2,0,'p',(128,64,32))])
        c.clip([((0,64,0),(4096,0,0,0))],length=5,keys=[(0,0,'p',(-64,128,0))])
        c.command('state',4);c.command('frames',12,10);c.command('seed',0x12345678)
        c.command('push',1,1,A.LOOP|A.ROOT|A.YAW,one,0,one,0);c.command('random',1)
        c.command('push',1,2,A.LOOP|A.ROOT,f(.25),f(.125),one,0);c.command('mask',2,1)
        for speed in [one,f(2),f(8),f(.25)]:c.command('advance',1,speed,0);c.command('draw',1);c.command('snapshot')
        c.command('copy',1);c.command('advance',2,one,0);c.command('draw',2)
        c.command('sync',1);c.command('below',1);c.command('jump',1);c.command('advance',1,one,0)
        c.command('snap',1);c.command('creature',1,one,0x1234,f(.2),20,1,0,0,0)
        c.command('creature',1,one,0x4000,f(2),0,0,f(1),f(-2),123)
        c.command('dtor',2);c.finish()
    c=Case('pose-pool-fallback-and-default',poses=2)
    c.clip([((64,0,0),(4096,0,0,0))]);c.command('state',2);c.command('push',1,1,1,one,0,one,0)
    c.command('joint',1,0);c.command('draw',1);c.command('frames',0,10);c.command('draw',1)
    c.command('state',2);c.command('push',2,1,1,one,0,one,0);c.command('joint',2,0);c.command('draw',2)
    c.command('state',2);c.command('push',3,1,1,f(.5),0,one,0);c.command('frames',0,14);c.command('draw',3);c.finish()
    c=Case('sound-events-pause-and-purge')
    c.clip([((0,0,0),(4096,0,0,0))],length=4)
    c.command('sound',1,3,4,6,0,1,1,2,3,2147483647)
    c.command('event',1,0,0,128);c.command('state',4);c.command('frames',0,10)
    c.command('push',1,1,A.LOOP|A.PERSISTENT,one,0,one,0)
    c.command('push',1,1,A.LOOP|A.PURGE_BELOW,one,0,one,0)
    for speed in [one,f(2),f(2),one]:c.command('advance',1,speed,1)
    c.command('push',1,1,A.PAUSED|A.FIXED_STEP,one,0,one,0);c.command('advance',1,one,1);c.finish()
    c=Case('quantization-wrap-stale-stack')
    c.command('reset')
    for q,p,b,d in [([0,0,one,0],[f(10),0,0],[0,0,0],0),([0,0,0,one],[f(2),0,0],[64,0,0],1),([0,0,0,one],[f(3),0,0],[0,0,0],1),([0,0,0,one],[f(512),0,0],[0,0,0],0)]:c.command('compose',*q,*p,*b,d)
    c.command('reset');c.command('compose',0,0,0,one,f(1),f(2),f(3),0,0,0,2);c.finish()
    c=Case('smallspider-constructor-active-inactive-wander-death-touch',slots=8,poses=4)
    for i in range(6):c.clip([((64*i,0,0),(4096,0,0,0))],length=20,keys=[(0,0,'p',(32,0,0))])
    c.command('spider_init');c.command('poke',0x1044,4,0x12345678);c.command('spider_construct');c.command('spider_snapshot')
    c.command('poke',0x13c4,2,600);c.command('poke',0x403a08,1,1);c.command('poke',0x1228,4,f(100));c.command('poke',0x1232,2,2)
    c.command('spider_tick',1,10);c.command('spider_snapshot')
    c.command('spider_state',0);c.command('spider_snapshot');c.command('spider_state',1);c.command('spider_snapshot')
    for state in (5,16,18,19):c.command('spider_state',state)
    c.command('spider_snapshot');c.command('poke',0x403a08,1,0);c.command('spider_tick',2,11);c.command('spider_snapshot')
    c.command('poke',0x100c,4,2);c.command('poke',0x800c,4,0x40);c.command('spider_touch',0x8000);c.command('spider_snapshot');c.finish()
    c=Case('smallspider-active-idle-walk-visible-death',slots=8,poses=4)
    for i in range(6):c.clip([((64*i,0,0),(4096,0,0,0))],length=4,keys=[(0,0,'p',(32,0,0))])
    c.command('spider_init');c.command('spider_construct');c.command('poke',0x13c4,2,600);c.command('poke',0x403a08,1,1);c.command('poke',0x1228,4,f(100))
    c.command('spider_state',0);c.command('poke',0x10f4,4,0);c.command('spider_tick',1,10);c.command('spider_snapshot')
    c.command('spider_tick',2,11);c.command('spider_snapshot')
    c.command('spider_state',20)
    for tick in range(3,9):c.command('spider_tick',tick,tick+10);c.command('spider_snapshot')
    c.finish()
    return cases

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--research',required=True);args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='creature-anim-oracle-') as d:
        C,A=reference(args.research,pathlib.Path(d));cases=generate(C,A)
    payload=json.dumps(cases,separators=(',',':')).encode()
    path=ROOT/'native/tests/vectors/creature_anim.json.gz';path.write_bytes(gzip.compress(payload,mtime=0))
    manifest={'source_manifest_sha256':hashlib.sha256(PINS.read_bytes()).hexdigest(),'cases':len(cases),'sha256':hashlib.sha256(payload).hexdigest(),'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest()}
    (ROOT/'native/tests/vectors/creature_anim.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest))
if __name__=='__main__':main()
