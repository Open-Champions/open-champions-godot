#!/usr/bin/env python3
"""Independent recovery composition. Original pinned bodies; explicit authored glue."""
import argparse,copy,gzip,hashlib,importlib.util,json,os,pathlib,sys,tempfile,types
sys.dont_write_bytecode=True
ROOT=pathlib.Path(__file__).resolve().parents[1]
PINS={
 'generate_session_control_creature_vectors':'dc9c191e290eef1a12b0bbae7785e5209155e929217d2817e659104b2024d5c1',
 'generate_player_vectors':'58dcef968b9c89b3f80f7064f021df63dc43be813709750ef81abcf7a14625b9'}
# The player loader independently verifies all four immutable blobs.
def helper(name):
    p=ROOT/'tools'/(name+'.py');data=p.read_bytes()
    if hashlib.sha256(data).hexdigest()!=PINS[name]:raise ValueError('helper pin mismatch: '+name)
    spec=importlib.util.spec_from_file_location('recovery_'+name,p);m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m);return m
CC=helper('generate_session_control_creature_vectors');PLAYER=helper('generate_player_vectors')
R=0x4d8af0

def initial(v):
    i=[0,R,1,0x42480000,0x42c80000,0x41a00000,0x42480000,0x41200000,0x3f800000,0x40c00000,0x3f000000,0,0,[0x10,0x200]+[0]*46,[0]*48,[[-1,0]],[0]*17,0,0xf5000001,False]
    if v==1:i[2]=0
    elif v==2:i[3]=0
    elif v==3:i[3]=0x80000000
    elif v==4:i[3]=0xbf800000
    elif v==5:i[3]=i[4];i[5]=i[6]
    elif v==6:i[7]=i[9]=1;i[8]=0x42c80000;i[10]=0x42480000
    elif v==7:i[11]=-128;i[12]=-1;i[14][0]=-1;i[14][1]=-128
    elif v==8:i[11]=i[12]=i[14][0]=i[14][1]=127
    elif v==9:i[8]=0xbf800000;i[10]=i[7]=0x80000000;i[9]=0xbf800000
    elif v==10:i[3]=0x42c7ffff;i[5]=0x4247ffff;i[8]=i[10]=0
    elif v==11:i[19]=True
    elif v==12:i[8]=0xbf800000;i[10]=0xbf000000
    elif v==13:i[4]=0
    return i

def actions(n):
    return {
      'ready':[('advance',0)]*4,
      'ordering':[('advance',17),('advance',17),('advance',0),('advance',16),('advance',0)],
      'pause':[('advance',17),('pause',1),('advance',31),('pause',0),('advance',0),('advance',16),('advance',0)],
      'release':[('advance',17),('release',0),('advance',0),('advance',16),('advance',0)],
      'reset':[('advance',17),('advance',0),('reset',0),('advance',0),('advance',0)],
      'stop':[('advance',0),('advance',0),('stop',0),('advance',16),('reset',0)]}[n]

def setup(p,i):
    out=[[p+0x3c8,4,0],[p+0x475,1,i[12]&255],[p+0x477,1,i[11]&255],[R+0x50,4,i[2]]]
    out += [[R+off,4,i[k]] for off,k in ((0xac,3),(0xb0,4),(0xb4,7),(0xb8,8),(0xbc,10),(0xc0,5),(0xc4,6),(0xc8,9))]
    for k in range(48):out += [[0x4ef538+i[2]*0xa88+k*0x38,8,i[13][k]],[R+0xef+k,1,i[14][k]&255]]
    out += [[R+0x43f0+k*4,4,0]for k in range(17)]+[[R+0x4b08,2,0xffff],[R+0x4b0a,2,0]]
    return out

class Control(CC.Control):
    def __init__(self,modules,helpers,config,name,variant,ref):
        self.recovery_initial=initial(variant);self.ref=ref
        # Keep creature factory's variant0 root/phase; variant belongs to recovery.
        super().__init__(modules,helpers,config,name,0)
    def reset(self):
        super().reset();self.max_ready=256
        o=self.o;p=o.a.addr;i=self.recovery_initial;mem=o.hp.model.mem
        o.a.flags |= 0x20000 if i[19] else 0
        o.hp.objects[0].flags=o.a.flags;o.hp.objects[0].hp=i[3];o.project_hp()
        for w in setup(p,i):mem.poke(*w)
        for k,(a,n) in enumerate(((R,0x4c30),(0x4ef538+i[2]*0xa88,0xa80),(p+0x3c8,0xb0))):
            o.hp.regions.append([a,n,'session.recovery.'+str(k)])
        class Tick(self.ref.Tick):
            def __init__(self):super().__init__(mem,p);self.events=[]
            def _w(t,a,n,v):t.events.append([0,[a,n,v&((1<<(8*n))-1)]]);return super()._w(a,n,v)
            def call(t,n,*args):t.events.append([1,[n,list(args)]]);return super().call(n,*args)
        self.tick=Tick()
        self.tick.cb={'SpellBlessed_typeinfo':lambda:i[18], 'creature_findSpellByType':lambda actor,token:0}
        self.recovery_calls=0
        ready=o.ready_tick
        def ready_tick(_):self.recovery_calls=0;return ready(_)
        o.ready_tick=ready_tick;o.network.run_tasks=ready_tick
    def ready(self):
        super().ready()
        o=self.o;t=self.tick;p=o.a.addr
        o.import_hp_scalars();o.project_hp();t.mem.poke(R+0xac,4,o.hp.objects[0].hp)
        self.recovery_calls+=1
        t.regen_health(p);t.regen_mana(p)
        t.mem.poke(p+0x228,4,t.mem.load(R+0xac,4));o.publish_hp()
        o.a.flags=o.hp.objects[0].flags;o.a.pos=list(o.hp.objects[0].pos);o.w.frame=o.hp.world.frame
    def snapshot(self):
        s=super().snapshot();s[0].append(copy.deepcopy(self.recovery_initial));o=self.o;t=self.tick
        recovery=[o.hp_bindings[0],copy.deepcopy(self.recovery_initial),'rta-session-recovery-v1','idle','','hp.adapter.model.mem',0,self.recovery_calls,
          t.writes,[[n,list(args)]for n,args in t.calls],t.events,t.draws,0,0,
          [['SpellBlessed_typeinfo','supplied.type_token',True],['creature_findSpellByType','NoActiveSpells',True]],o.object_run_completed,o.task_priority]
        s[3]=[s[3],recovery]
        return s

def run(modules,helpers,ref,f,v,n):
    ref.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1))
    c=Control(modules,helpers,f,n,v,ref)
    try:
        start=copy.deepcopy(c.snapshot());steps=[]
        for a,value in actions(n):
            c.action(a,value);steps.append([[a,value],copy.deepcopy(c.snapshot())])
        return [start,steps]
    finally:c.close()

def run_direct(modules,helpers,ref,f,name):
    import struct
    ref.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1))
    o=CC.CONTROL.make_oracle(modules,helpers,f,0,'recovery_direct')
    i=initial(0);p=o.a.addr;mem=o.hp.model.mem
    o.a.flags|=0x20;o.hp.objects[0].flags=o.a.flags;o.hp.objects[0].hp=i[3];o.project_hp()
    for w in setup(p,i):mem.poke(*w)
    for k,(a,n)in enumerate(((R,0x4c30),(0x4ef538+i[2]*0xa88,0xa80),(p+0x3c8,0xb0))):o.hp.regions.append([a,n,'session.recovery.'+str(k)])
    limit=1 if name=='direct_health_failure' else 3 if name=='direct_mana_failure' else 65536
    stored=mem.store
    def store(a,n,v):
        if len(mem.log)>=limit:raise CC.CONTROL.Refusal('memory history budget')
        return stored(a,n,v)
    mem.store=store
    class Tick(ref.Tick):
        def __init__(self):super().__init__(mem,p);self.events=[]
        def _w(t,a,n,v):t.events.append([0,[a,n,v&((1<<(8*n))-1)]]);return super()._w(a,n,v)
        def call(t,n,*args):t.events.append([1,[n,list(args)]]);return super().call(n,*args)
    tick=Tick();tick.cb={'SpellBlessed_typeinfo':lambda:i[18],'creature_findSpellByType':lambda actor,token:0}
    phase='idle';depth=calls=0
    def publish():
        mem.poke(p+0x228,4,mem.load(R+0xac,4));o.publish_hp()
        o.a.flags=o.hp.objects[0].flags;o.a.pos=list(o.hp.objects[0].pos);o.w.frame=o.hp.world.frame
    def recover():
        def body():
            nonlocal phase,depth,calls
            calls+=1;depth=1;phase='import';o.import_hp_scalars();o.project_hp();mem.poke(R+0xac,4,o.hp.objects[0].hp)
            try:
                phase='regen_health';tick.regen_health(p);phase='regen_mana';tick.regen_mana(p)
                phase='publish';publish();phase='idle'
            except Exception:publish();raise
            finally:depth=0
        return o.guard('recover_player',body)
    o.control=types.SimpleNamespace(ready=recover)
    if name=='direct_pending':o.network.level_pending=True
    def snapshot():
        base=o.snapshot();base[1]=None
        h=base[0][1];h[0][2][1]=limit;h[5][0][3][1]=limit
        r=[o.hp_bindings[0],i,'rta-session-recovery-v1',phase,'','hp.adapter.model.mem',depth,calls,
          tick.writes,[[n,list(args)]for n,args in tick.calls],tick.events,0,0,0,
          [['SpellBlessed_typeinfo','supplied.type_token',True],['creature_findSpellByType','NoActiveSpells',True]],o.object_run_completed,o.task_priority]
        return copy.deepcopy([[base,None],r])
    try:
        start=snapshot();before=start;steps=[]
        for k in range(4 if name in ('direct_pending','direct_no_sim')else 3):
            if name=='direct_pending' and k==3:o.network.level_pending=False
            result='ok'
            def submit():
                o.started=True
                return o.network.net_main(struct.pack('<H6B',0,128,128,128,128,0,0),no_sim=name=='direct_no_sim'and k<3)
            try:o.guard('submit',submit)
            except Exception:result='error'
            current=snapshot();delta=helpers['session_hp'].snapshot_delta(before,current)
            if helpers['session_hp'].apply_delta(before,delta)!=current:raise ValueError('lossy direct delta')
            steps.append([result,delta]);before=current
        return [start,steps]
    finally:o.close()

def generate(research,output):
    modules=CC.BASE.load_references(research);helpers=CC.BASE.load_helpers();_,ref=PLAYER.load(research)
    sc=helpers['session_objects'].sc;hp=helpers['session_hp'];rows=[]
    for n in ('ready','ordering','pause','release','reset','stop'):
      for f in range(16):
       for v in (range(14) if n == 'ready' else (0,6,7) if n == 'ordering' else (0,)):
        first=run(modules,helpers,ref,f,v,n);second=run(modules,helpers,ref,f,v,n)
        if first!=second:raise ValueError('fresh reference disagreement')
        # Complete native records are used for comparison; fixture stores only
        # lossless deltas. Reapply every delta before publishing it.
        before=first[0];steps=[]
        for action,current in first[1]:
            delta=hp.snapshot_delta(before,current)
            if hp.apply_delta(before,delta)!=current:raise ValueError('lossy delta')
            steps.append([action,delta]);before=current
        recipe=f'{n}\t{f}\t{v}'
        rows.append(recipe+'\t'+sc([first[0],steps])+'\n')
        if len(rows)%32==0: print('generated',len(rows),flush=True)
    for n in ('direct_health_failure','direct_mana_failure','direct_pending','direct_no_sim'):
      for f in range(16):
        first=run_direct(modules,helpers,ref,f,n);second=run_direct(modules,helpers,ref,f,n)
        if first!=second:raise ValueError('fresh direct reference disagreement')
        rows.append(f'{n}\t{f}\t0\t'+sc(first)+'\n')
    data=('# session_recovery_vectors_v1\n'+''.join(rows)).encode('ascii')
    trace=''.join(rows).encode('ascii')
    manifest={'suite':'session-recovery','policy':'rta-session-recovery-v1','count':len(rows),'references':CC.REFERENCES,
      'player_commit':PLAYER.COMMIT,'player_sources':PLAYER.PINS,'helpers':{**CC.HELPERS,**PINS},
      'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
      'vector_sha256':hashlib.sha256(data).hexdigest(),'expected_trace_sha256':hashlib.sha256(trace).hexdigest()}
    output=pathlib.Path(output);packed=gzip.compress(data,mtime=0);manifest['compressed_sha256']=hashlib.sha256(packed).hexdigest()
    # Atomic vector first, manifest last. Failed generation leaves old pair.
    for path,content in ((output,packed),(output.with_name(output.name[:-7]+'.json'),(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode())):
      fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.'+path.name)
      try:
        with os.fdopen(fd,'wb')as stream:stream.write(content);stream.flush();os.fsync(stream.fileno())
        os.replace(tmp,path)
      finally:pathlib.Path(tmp).unlink(missing_ok=True)
    print(json.dumps(manifest,sort_keys=True))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=ROOT/'native/tests/vectors/session_recovery.tsv.gz');a=p.parse_args();generate(a.research_dir,a.output)
