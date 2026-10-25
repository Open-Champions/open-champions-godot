#!/usr/bin/env python3
"""Independent Controller glue around immutable Python World/HP/animation bodies.
No executable native output is consumed. Checkpoints leave this oracle untouched.
"""
import argparse, copy, gzip, hashlib, importlib.util, json, os, pathlib, sys, tempfile
sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PINS = {
 'generate_session_facing_vectors': '4bd169b0d7c647c275f6143422796b6ec9155eee74fc376057df0dc3f88f85ac',
 'generate_session_anim_cache_vectors': 'd0efa1a0329612b30b9ac9ada7f8680cfa97eece276897923bae45c412e5247f'}
def helper(name):
 p = ROOT/'tools'/(name+'.py'); data=p.read_bytes()
 if hashlib.sha256(data).hexdigest()!=PINS[name]: raise ValueError('helper pin '+name)
 spec=importlib.util.spec_from_file_location('control_cache_'+name,p)
 m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m);return m
F=helper('generate_session_facing_vectors');CACHE=helper('generate_session_anim_cache_vectors')
SCENARIOS=('lifecycle','paused_pending','reset','key_failure','history_failure','full_cap')
def actions(name):
 if name=='full_cap': return [('start',1),('advance',21)] + [('advance',0)]*129
 if name in ('key_failure','history_failure'):return [('start',1)]+[('advance',0)]*4
 if name=='reset':return [('start',1),('advance',1),('advance',0),('reset',0),('start',2),('advance',4),('advance',0)]
 if name=='paused_pending':return [('start',1),('advance',21),('advance',0),('pause',1),('restart',2),('checkpoint',0),('checkpoint',1),('advance',31),('pause',0),('advance',0),('advance',0),('stopclip',2),('advance',0),('advance',0)]
 return [('start',1),('advance',21),('checkpoint',0),('advance',0),('start',2),('advance',5),('start',3),('advance',0),('stopclip',1),('advance',0),('checkpoint',1),('restart',1),('advance',0),('advance',0),('stopclip',1),('advance',0),('advance',0)]

def diagnostic(A):
 # Separately authored fixture definition, MSB-first fixed-width fields.
 fields=[(7,4),(16,8),(0,8),(0,8),(15,4),(32767,16),(0,16),(0,16),(0,16),
         (0,8),(1,1),(0,6),(7,4),(32,8),(0,8),(0,8),
         (255,8),(1,1),(0,6),(0,4),(0,3)]
 bits=''.join(format(v,'0%db'%n) for v,n in fields);bits+='0'*(-len(bits)%16)
 c=A.Clip(A.encode_clip([((0,0,0),(0,0,0,0))],120),'Authored diagnostic joint')
 c.data=b'';c.bind=[(0,0,0,0)];c.depth=[0];c.events=[(0x3d088888,1),(0,0),(0,0),(0,0)]
 c.sound_times=None;c.sound_group=0;c.joint_mask=0
 c.words=[int(bits[i:i+16],2)for i in range(0,len(bits),16)]
 return c

class Control(F.Control):
 def __init__(self,modules,helpers,f,ref,turn,cache_modules,cache_helpers,name):
  self.A=cache_modules['anim'];self.cache_modules=cache_modules;self.cache_helpers=cache_helpers;self.scenario=name
  super().__init__(modules,helpers,f,0,1,ref,turn)
 def reset(self):
  super().reset();self.max_ready=128
  key=1 if self.scenario=='key_failure'else 256
  history=3 if self.scenario=='history_failure'else 1024
  self.cache_limits=[[1024,16384,4,8,4,key],1,8,256]
  self.cache_setup=[2,self.cache_limits,10,history]
  self.runtime=self.cache_helpers['animation_runtime'].Oracle(self.cache_modules,
     [[0,0],self.cache_limits,[],[],[],[[i,0,0,0,0,0,[[0]*16 for _ in range(64)]]for i in range(2)],[2,1],[]])
  self.runtime.clips=[diagnostic(self.A)for _ in range(3)];self.runtime.add_state(4)
  self.initial_clips=copy.deepcopy(self.runtime.snapshot()[2][2])
  self.next_id=1;self.commands=[];self.selected=0;self.cache_calls=0;self.advanced=False
  self.last_event=None;self.last_decode=None;self.cache_phase='idle';self.cache_ops=[]
  ready=self.o.ready_tick
  def reset_counts(machine):
   self.cache_calls=0;self.advanced=False
   return ready(machine)
  self.o.ready_tick=reset_counts;self.o.network.run_tasks=reset_counts
 def bounded_decode(self,t):
  A=self.A;left=self.cache_limits[0][5]
  def trace(frame,event,arg):
   nonlocal left
   if event=='line'and frame.f_code.co_filename==A.__file__ and frame.f_code.co_name=='animtrack_decode'and frame.f_lineno==494:
    if not left:raise F.CC.CONTROL.Refusal('decoder key work exceeded')
    left-=1
   return trace
  old=sys.gettrace();sys.settrace(trace)
  try:return A.animtrack_decode(self.runtime.system,t)
  finally:sys.settrace(old)
 def invoke(self,kind,clip=0,track=0):
  A=self.A;r=self.runtime;s=r.states[0];frame=self.o.w.frame
  parameter=A.ONE if kind=='advance_animation_cache'else 0
  flags=A.LOOP|A.KEEP if kind=='start_animation_cache'else 0
  op=[kind,1,clip,track,frame,parameter,flags,0,[0,0],None,None,False]
  def body():
   self.cache_ops.append(op);r.system.sim_frame=frame;self.cache_calls+=1;self.cache_phase=kind
   if kind=='start_animation_cache':op[3]=r.identity(A.anim_start(0,s,r.clip(clip),flags),r.tracks)
   elif kind=='stop_animation_cache':A.anim_stop(0,s,r.clip(clip))
   elif kind=='advance_animation_cache':
    self.advanced=True;self.last_event=None
    self.last_event=op[9]=A.anim_advance(r.system,A.ONE,s,False,False,False)[0]
   else:
    self.last_decode=None;t=r.tracks[track-1];slot=self.bounded_decode(t)
    d=[track,r.identity(slot,r.system.decoders.slots),t.time,[]];op[10]=copy.deepcopy(d)
    for j in range(t.clip.joint_count):
     d[3].append([list(A.joint_pos(slot,j,t.time)),list(A.joint_quat(slot,j,t.time))]);op[10]=copy.deepcopy(d)
    self.last_decode=copy.deepcopy(d)
   op[11]=True;self.cache_phase='idle'
  return self.o.guard(kind,body)
 def ready(self):
  # Tick counter and work reservation precede movement in native policy.
  frame=self.o.w.frame;work=2+sum(2 if c[2]==2 else 1 for c in self.commands if not c[4]and c[1]==frame)
  if work>10 or work>self.cache_setup[3]-len(self.cache_ops):
   self.phase='task';self.ticks+=1;raise F.CC.CONTROL.Refusal('control animation task operation/history budget')
  super().ready()
  for c in self.commands:
   if c[4]or c[1]!=frame:continue
   c[4]=True
   if c[2]!=0:self.invoke('stop_animation_cache',c[3])
   if c[2]!=1:self.invoke('start_animation_cache',c[3])
   c[5]=True
  self.invoke('advance_animation_cache')
  r=self.runtime;s=r.states[0];self.selected=r.identity(s.tracks[s.count-1],r.tracks)if s.count else 0
  if self.selected:self.invoke('decode_animation_cache',track=self.selected)
 def action(self,a,v):
  if a in ('start','stopclip','restart'):
   self.commands.append([self.next_id,self.o.network.lif,{'start':0,'stopclip':1,'restart':2}[a],v,False,False]);self.next_id+=1
   return
  if a=='checkpoint':return
  try:return super().action(a,v)
  except Exception as e:
   self.failed=self.stopped=True;self.error=str(e);self.phase='failed';self.busy=False
 def snapshot(self):
  base=super().snapshot();r=self.runtime
  binding=[copy.deepcopy(self.o.hp_bindings[0]),1,self.cache_calls,self.advanced,self.last_event,self.last_decode]
  cache=[self.cache_setup,r.snapshot(),[binding],'rta-session-anim-cache-v1',self.cache_phase,0,self.cache_calls,self.cache_ops,self.o.object_run_completed,self.o.task_priority]
  setup=[self.initial_clips,2,4,10,self.cache_setup[3],self.cache_limits[0][5],256]
  return copy.deepcopy([base,setup,cache,self.next_id,self.commands,self.selected])

def run(modules,helpers,ref,turn,cache_modules,cache_helpers,f,name):
 for m in (ref,turn,cache_modules['anim']):m.C.set_fpu(('ps2','ps2g1','chop','nearest')[f//4],bool(f&2),bool(f&1))
 c=Control(modules,helpers,f,ref,turn,cache_modules,cache_helpers,name)
 try:
  start=c.snapshot();before=start;steps=[];hp=helpers['session_hp']
  commands=actions(name)
  for k,(a,v)in enumerate(commands):
   c.action(a,v)
   if name=='full_cap'and k+1<len(commands):continue
   current=c.snapshot();d=hp.snapshot_delta(before,current)
   if hp.apply_delta(before,d)!=current:raise ValueError('lossy composition delta')
   steps.append([[a,v],d]);before=current
  return [start,steps]
 finally:c.close()

def generate(research,output):
 modules=F.CC.BASE.load_references(research);helpers=F.CC.BASE.load_helpers()
 _,ref=F.REC.PLAYER.load(research);turn=F.TURN.load_references(research)['creature']
 cache_helpers=CACHE.load_helpers();cache_modules=cache_helpers['session_animation'].load_references(research)
 sc=helpers['session_objects'].sc;rows=[]
 for name in SCENARIOS:
  for f in range(16):
   first=sc(run(modules,helpers,ref,turn,cache_modules,cache_helpers,f,name))
   if first!=sc(run(modules,helpers,ref,turn,cache_modules,cache_helpers,f,name)):raise ValueError('fresh oracle divergence')
   rows.append(f'{name}\t{f}\t'+first+'\n')
  print(name,len(rows),flush=True)
 data=('# control_anim_cache_vectors_v1\n'+''.join(rows)).encode();trace=''.join(rows).encode();packed=gzip.compress(data,mtime=0)
 pins={**PINS,**F.PINS,**F.REC.PINS}
 manifest={'suite':'control_anim_cache','policy':'rta-control-anim-cache-v1','count':len(rows),'scenarios':SCENARIOS,'references':CACHE.REFERENCES,'specification':CACHE.SPECIFICATION_EVIDENCE,'helpers':{**F.CC.HELPERS,**CACHE.HELPERS,**pins},'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(data).hexdigest(),'expected_trace_sha256':hashlib.sha256(trace).hexdigest(),'compressed_sha256':hashlib.sha256(packed).hexdigest()}
 output.parent.mkdir(parents=True,exist_ok=True)
 for path,content in ((output,packed),(output.with_name(output.name[:-7]+'.json'),(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode())):
  fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.'+path.name)
  try:
   with os.fdopen(fd,'wb')as stream:stream.write(content);stream.flush();os.fsync(stream.fileno())
   os.replace(tmp,path)
  finally:pathlib.Path(tmp).unlink(missing_ok=True)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=ROOT/'native/tests/vectors/control_anim_cache.tsv.gz');a=p.parse_args();generate(a.research_dir,a.output)
