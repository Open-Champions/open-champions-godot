#!/usr/bin/env python3
"""Independent pinned turning + existing control/recovery, authored target glue."""
import argparse,copy,struct,gzip,hashlib,importlib.util,json,os,pathlib,sys,tempfile
sys.dont_write_bytecode=True
ROOT=pathlib.Path(__file__).resolve().parents[1]
PINS={}
def helper(name):
 p=ROOT/'tools'/(name+'.py');data=p.read_bytes()
 if hashlib.sha256(data).hexdigest()!=PINS[name]:raise ValueError('helper pin '+name)
 spec=importlib.util.spec_from_file_location('facing_'+name,p);m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m);return m
# Literal pins filled once when authored; never refreshed during generation.
PINS.update({
 'generate_session_recovery_vectors':'8275dd1537d5011c90a72615e957b3c275e11c5d641f28aef929613c0cc1ce1e',
 'generate_session_turn_vectors':'a1ef9d149aee0066504e9f3a9d8d5b2e50a70cad0ee49f8bdcfb5509bb51a8be'})
REC=helper('generate_session_recovery_vectors');TURN=helper('generate_session_turn_vectors');CC=REC.CC

def actions(name):
 if name=='directions':return [('advance',k)for k in range(16)]+[('advance',0)]
 if name=='limit':return [('advance',21)]*257
 return [('advance',21),('checkpoint',0),('advance',21),('advance',26),('advance',26),('pause',1),('checkpoint',1),('advance',31),('pause',0),('advance',0),('release',0),('advance',9),('advance',0),('stop',0),('advance',31),('reset',0),('advance',8),('advance',0)]

class Control(REC.Control):
 def __init__(self,modules,helpers,f,v,r,ref,turn):
  self.history_limit=65536;self.turn_phase="idle"
  self.variant_facing=v;self.has_recovery=r;self.turn_module=turn
  self.descriptor=[1,0xbf800000 if v==5 else 0x3f800000,0xfff0 if v==6 else 0,1 if v==2 else 60 if v==3 else 61 if v==4 else 0,1 if v==1 else 0]
  # Default native recovery has zero passive masks.
  self.recovery_initial=REC.initial(0);self.recovery_initial[13]=[0]*48;self.ref=ref
  CC.Control.__init__(self,modules,helpers,f,'facing',0)
 def reset(self):
  if self.has_recovery:REC.Control.reset(self)
  else:CC.Control.reset(self)
  self.max_ready=256;o=self.o;p=o.a.addr;mem=o.hp.model.mem;d=self.descriptor
  class Turn(self.turn_module.Model):
   def __init__(self):super().__init__(mem);self.events=[]
   def _w(t,a,n,v):
    if len(t.writes)>=self.history_limit or len(t.events)>=self.history_limit:raise CC.CONTROL.Refusal('model history')
    t.events.append([0,[a,n,v&((1<<(8*n))-1)],['',[]]])
    return super()._w(a,n,v)
  self.turn_phase="idle"
  self.turn=Turn();self.binding=[copy.deepcopy(o.hp_bindings[0]),d[1:4],0,False,None];self.operations=[]
  self.turn_calls=0
  for off,n,v in ((0xb0,4,d[1]),(0x110,2,d[2]),(0x2fc,1,d[3]),(0x10,4,o.a.pos[0]),(0x14,4,o.a.pos[1]),(0x38,2,o.a.angle)):mem.poke(p+off,n,v)
  ready=o.ready_tick
  def ready_tick(_):
   self.turn_calls=0;self.binding[2]=0;self.binding[3]=False
   return ready(_)
  o.ready_tick=ready_tick;o.network.run_tasks=ready_tick
  move=o.move
  def moving(velocity):
   if velocity[0] or velocity[1]:
    point=[self.turn_module.C.add(o.a.pos[k],velocity[k])for k in range(2)]+[o.a.pos[2]]
    def body():
     self.turn_calls+=1;self.binding[2]+=1;self.binding[3]=True;self.binding[4]=None
     op=['turn_toward',1,d[4],point,o.w.frame,None,False];self.operations.append(op)
     o.hp.objects[0].pos[:2]=o.a.pos[:2]
     for off,n,v in ((0x10,4,o.a.pos[0]),(0x14,4,o.a.pos[1]),(0x38,2,o.a.angle)):mem.poke(p+off,n,v)
     self.turn_phase='turn_toward'
     try:
      result=self.turn.turn_toward(p,d[4],tuple(point));self.binding[4]=op[5]=result
      o.a.angle=mem.load(p+0x38,2);op[6]=True;self.turn_phase='idle'
     except Exception:
      o.a.angle=mem.load(p+0x38,2);raise
    o.guard('turn_toward',body)
   return move(velocity)
  o.move=moving
 def action(self,a,v):
  if a=="checkpoint":return self.observation()
  return super().action(a,v)
 def ready(self):
  if self.has_recovery:REC.Control.ready(self)
  else:CC.Control.ready(self)
 def snapshot(self):
  if self.has_recovery:s=REC.Control.snapshot(self)
  else:
   s=CC.Control.snapshot(self);s[0].append(None);s[3]=[s[3],None]
  o=self.o;t=self.turn
  turn=[[1,1],[self.binding],'rta-session-turn-v1',self.turn_phase,'','hp.adapter.model.mem',0,self.turn_calls,[1048576,self.history_limit,64,64],0,True,[],[],t.writes,t.calls,t.events,t.draws,self.operations,o.object_run_completed,o.task_priority]
  return [s,self.descriptor,turn]

def run_direct(modules,helpers,ref,turn,f,name):
 c=Control(modules,helpers,f,0,0,ref,turn);o=c.o
 if name=='direct_pending':o.network.level_pending=True
 if name=='direct_failure':
  c.history_limit=1;store=o.hp.model.mem.store
  def limited(a,n,v):
   if len(o.hp.model.mem.log)>=1:raise CC.CONTROL.Refusal('memory history')
   return store(a,n,v)
  o.hp.model.mem.store=limited
 def snapshot():
  s=c.snapshot();base=s[0][3]
  if name=='direct_failure':
   h=base[0][0][0][1];h[0][2][1]=1;h[5][0][3][1]=1
   base[0][1][10][1]=1
  return copy.deepcopy([base,s[2]])
 try:
  start=snapshot();before=start;steps=[];hp=helpers['session_hp']
  for k in range(4):
   if k==3 and name=='direct_pending':o.network.level_pending=False
   result='ok'
   def submit():
    o.started=True
    return o.network.net_main(struct.pack('<H6B',5,128,128,128,128,0,0),no_sim=name=='direct_no_sim'and k<3)
   try:o.guard('submit',submit)
   except Exception:result='error'
   current=snapshot();delta=hp.snapshot_delta(before,current)
   if hp.apply_delta(before,delta)!=current:raise ValueError('lossy direct delta')
   steps.append([result,delta]);before=current
  return [start,steps]
 finally:c.close()

def generate(research,output):
 modules=CC.BASE.load_references(research);helpers=CC.BASE.load_helpers();_,ref=REC.PLAYER.load(research);turn=TURN.load_references(research)['creature'];hp=helpers['session_hp'];sc=helpers['session_objects'].sc
 rows=[]
 for name in ('directions','lifecycle','limit'):
  for f in range(16):
   for v in (range(7) if name!="limit" else (0,)):
    for r in (0,1):
     ref.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1));turn.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1))
     c=Control(modules,helpers,f,v,r,ref,turn)
     try:
      start=copy.deepcopy(c.snapshot());before=start;steps=[]
      commands=actions(name)
      for k,(a,value) in enumerate(commands):
       c.action(a,value)
       if name=="limit" and k+1<len(commands):continue
       current=copy.deepcopy(c.snapshot());delta=hp.snapshot_delta(before,current)
       if hp.apply_delta(before,delta)!=current:raise ValueError('lossy delta')
       steps.append([[a,value],delta]);before=current
      rows.append(f'{name}\t{f}\t{v}\t{r}\t'+sc([start,steps])+'\n')
     finally:c.close()
  print(name,len(rows),flush=True)
 for name in ('direct_pending','direct_no_sim','direct_failure'):
  for f in range(16):
   ref.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1));turn.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1))
   rows.append(f'{name}\t{f}\t0\t0\t'+sc(run_direct(modules,helpers,ref,turn,f,name))+'\n')
 data=('# session_facing_vectors_v1\n'+''.join(rows)).encode();trace=''.join(rows).encode();packed=gzip.compress(data,mtime=0)
 manifest={'suite':'session-facing','policy':'rta-session-facing-v1','count':len(rows),'references':TURN.REFERENCES,'specification':TURN.SPECIFICATION_EVIDENCE,'helpers':{**CC.HELPERS,**REC.PINS,**PINS},'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(data).hexdigest(),'expected_trace_sha256':hashlib.sha256(trace).hexdigest(),'compressed_sha256':hashlib.sha256(packed).hexdigest()}
 for path,content in ((output,packed),(output.with_name(output.name[:-7]+'.json'),(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode())):
  fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.'+path.name)
  try:
   with os.fdopen(fd,'wb')as stream:stream.write(content);stream.flush();os.fsync(stream.fileno())
   os.replace(tmp,path)
  finally:pathlib.Path(tmp).unlink(missing_ok=True)
 print(json.dumps(manifest,sort_keys=True))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=ROOT/'native/tests/vectors/session_facing.tsv.gz');a=p.parse_args();generate(a.research_dir,a.output)
