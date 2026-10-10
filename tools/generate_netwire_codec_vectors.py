#!/usr/bin/env python3
"""Exact authored netwire codec vectors; no executable or original packets."""
import argparse,hashlib,importlib.util,json,random,struct,subprocess,sys
from pathlib import Path
PIN='4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'
COMMIT='54258f9d7e281ef07f0675040e82e6264f9386cf'
SCHEMA='rta-netwire-codec-v1'
def n(x):return f'{int(x)&((1<<64)-1):016x}'
def ns(v):return ','.join(n(x) for x in v) or '-'
def raw(v):return bytes(v).hex() or '-'
def ch(c):return ns((c.index,c.pred,c.value,c.velocity))
def cs(c):return ';'.join([*(ch(x) for x in c.ch),ns((c.yaw_prev,c.yaw_pred,c.yaw_delta,c.pad46,c.count))])
def sy(s):return ';'.join(ns(x) for x in s) or '-'
def fs(s):return '/'.join([ns((s.count,s.bits)),ns(s.buttons),*(sy(x) for x in s.sticks),sy([(dict(keep=0,delta=1,abs=2)[x[0]],x[1] if len(x)>1 else 0) for x in s.yaw]),ns(s.flags)])
def records(v):return ','.join(raw(x) for x in v) or '-'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',type=Path,required=True);p.add_argument('--output',type=Path,default=Path('native/tests/vectors/netwire_codec.tsv'));a=p.parse_args();path=a.research_dir/'tools/ref_netwire.py';assert hashlib.sha256(path.read_bytes()).hexdigest()==PIN;assert hashlib.sha256(subprocess.check_output(['git','-C',str(a.research_dir),'show',COMMIT+':tools/ref_netwire.py'])).hexdigest()==PIN
 sys.dont_write_bytecode=True;spec=importlib.util.spec_from_file_location('ref_netwire_vectors',path);R=importlib.util.module_from_spec(spec);spec.loader.exec_module(R);rng=random.Random(0x4e455457);rows=[];counts={};errors={}
 def emit(op,args,result,state='-'):
  rows.append('\t'.join(['netwire-codec',op,str(len(rows))+'|'+args,str(result),state]));counts[op]=counts.get(op,0)+1
  if result in ('value','index','malformed'):errors[result]=errors.get(result,0)+1
 def call(fn):
  try:return fn()
  except R.MalformedPacket:return 'malformed'
  except (ValueError,struct.error):return 'value'
  except IndexError:return 'index'
 def makec(i):return R.Channel(i%9,(i*79)%256,rng.getrandbits(32) if i%4==0 else 0x43000000,rng.getrandbits(32) if i%4==0 else 0)
 def makes(i):
  s=R.CodecState();s.ch=[makec(i+j) for j in range(4)];s.yaw_prev=R.s16(i*3101);s.yaw_pred=R.s16(i*2783);s.yaw_delta=R.s16(i*349);s.pad46=(i*901)&65535;s.count=R.s32(0x7ffffffe+i);return s
 # Every arithmetic operation is evaluated with the netwire model, independent
 # of the earlier core corpus, with all16 combinations and raw edge patterns.
 edge=[0,0x80000000,1,0x80000001,0x7fffff,0x807fffff,0x800000,0x80800000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fffffff,0xffffffff,0x3d4ccccc,0x3e4ccccc]
 for mode in range(16):
  f=R.Fpu(R.Fpu.MODELS[mode//4],bool(mode&2),bool(mode&1))
  pairs=[(x,y) for x in edge for y in edge]+[(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(512)]
  for op in ('add','sub','mul','div','lt'):
   for x,y in pairs:emit(op,ns((mode,x,y)),n(getattr(f,op)(x,y)))
  for op in ('sqrt','from_int','to_int'):
   for x in edge+[rng.getrandbits(32) for _ in range(512)]:emit(op,ns((mode,x)),n(getattr(f,op)(x)))
 for i in range(192):
  c=makec(i);before=ch(c);emit('channel_pack',before,raw(c.pack()),ch(c));b=c.pack()+bytes(i%3);emit('channel_unpack',raw(b),ch(R.Channel.unpack(b)))
  s=makes(i);emit('state_pack',cs(s),raw(s.pack()),cs(s));b=s.pack()+bytes(i%3);emit('state_unpack',raw(b),cs(R.CodecState.unpack(b)))
  vals=[rng.randrange(-65536,65536),*[rng.randrange(256) for _ in range(6)],rng.randrange(-100000,100000),rng.randrange(-2,3)];b=R.rec12(vals[0],vals[1:5],vals[5:7],vals[7],vals[8]);emit('record',ns(vals),raw(b));f=R.rec12_fields(b);emit('record_fields',raw(b+bytes(i%3)),ns([f[0],*f[1],*f[2],f[3]]))
 for length in range(76):
  b=bytes(length)
  if length<16:emit('channel_unpack',raw(b),call(lambda:ch(R.Channel.unpack(b))))
  emit('state_unpack',raw(b),call(lambda:cs(R.CodecState.unpack(b))))
  if length<12:emit('record_fields',raw(b),call(lambda:ns(R.rec12_fields(b))))
 for i in range(256):
  w=R.BitWriter();w.words=[rng.randrange(65536) for _ in range(3)];w.pos=i%32;before=raw(w.tobytes());steps=[(rng.randrange(-65536,65536),rng.randrange(17)) for _ in range(1+i%15)];args='/'.join([n(w.pos),before,sy(steps)])
  for value,bits in steps:w.put(value,bits)
  emit('write',args,raw(w.tobytes()),ns((w.pos,w.written)))
  b=bytes(rng.getrandbits(8) for _ in range(i%33));pos=0 if i%3 else -min(len(b)//2,1)*16;r=R.BitReader(b,pos);steps=[rng.randrange(33) for _ in range(1+i%12)];beforepos=pos;out=[]
  result=call(lambda:[out.append(r.get(bits)) for bits in steps]);emit('read','/'.join([n(beforepos),raw(b),ns(steps)]),result if isinstance(result,str) else ns(out),n(r.pos))
 lengths=[0,1,2,5,6,7,8,14,30,31,32,62,63,127,255]
 for mode in range(16):
  f=R.Fpu(R.Fpu.MODELS[mode//4],bool(mode&2),bool(mode&1))
  for i in range(48):
   count=lengths[i%len(lengths)];samples=[(j*71+i*43)%256 if i%3 else i%256 for j in range(count)];c=makec(i);before=ch(c);symbols=R.axis_symbols(c,samples);emit('axis_symbols',before+'/'+ns(samples),sy(symbols),ch(c));w=R.BitWriter();R.write_axis_symbols(w,symbols);emit('axis_write',sy(symbols),raw(w.tobytes()),n(w.pos));r=R.BitReader(w.tobytes());decoded=R.read_axis_symbols(r,count);emit('axis_read',raw(w.tobytes())+'/'+n(count),sy(decoded),n(r.pos));c=R.Channel(*[int(x,16) if k>1 else R.s32(int(x,16)) for k,x in enumerate(before.split(','))]);w2=R.BitWriter();R.encode_axis(w2,c,samples);emit('axis_encode',before+'/'+ns(samples),raw(w2.tobytes()),ch(c)+'/'+n(w2.pos))
   c=makec(i);before=ch(c);out=R.apply_axis_symbols(c,symbols,f);emit('axis_apply',n(mode)+'/'+before+'/'+sy(symbols),raw(out),ch(c));c=makec(i);before=ch(c);skip=count//3;r=R.BitReader(w.tobytes());out=R.decode_axis(r,c,count-skip,skip,f);emit('axis_decode',n(mode)+'/'+before+'/'+raw(w.tobytes())+'/'+ns((count-skip,skip)),raw(out),ch(c)+'/'+n(r.pos))
   count=max(1,count);recs=[R.rec12((j*117+i)&65535,[(j*11+p*57+i)%256 for p in range(4)],(i%256,j%256),R.s16(j*([2,200,1002,4000][i%4])+i),j%2 if i%3 else 0) for j in range(count)];s=makes(i);before=cs(s);writer=R.encode_frames(s,recs);emit('frames_encode',before+'/'+records(recs),raw(writer.tobytes()),cs(s)+'/'+n(writer.pos));symbols=R.read_frames(writer.tobytes(),count);emit('frames_read',raw(writer.tobytes())+'/'+n(count),fs(symbols));s=makes(i);before=cs(s);skip=count//3;out=R.apply_frames(s,symbols,skip,f);emit('frames_apply',n(mode)+'/'+before+'/'+n(skip)+'/'+fs(symbols),records(out),cs(s));s=makes(i);before=cs(s);out,bits=R.decode_frames(s,writer.tobytes(),count,skip,f);emit('frames_decode',n(mode)+'/'+before+'/'+ns((count,skip))+'/'+raw(writer.tobytes()),records(out)+'/'+n(bits),cs(s));e=makes(i);d=makes(i+1);before=cs(e)+'/'+cs(d);out=R.local_roundtrip(recs[0],e,d,f);emit('local',n(mode)+'/'+before+'/'+raw(recs[0]),raw(out),cs(e)+'/'+cs(d))
 # Explicit long equal button runs, simultaneous bit flips, and keep/delta yaw.
 for mode in range(16):
  f=R.Fpu(R.Fpu.MODELS[mode//4],bool(mode&2),bool(mode&1))
  for count in (1,7,31,32,62,255):
   for pattern in range(3):
    recs=[R.rec12(0xffff if pattern==1 and j%32==31 else 0, yaw=2*j if pattern==2 else 0,flag=int(j>=31)) for j in range(count)]
    s=R.CodecState();before=cs(s);w=R.encode_frames(s,recs);emit('frames_encode',before+'/'+records(recs),raw(w.tobytes()),cs(s)+'/'+n(w.pos));symbols=R.read_frames(w.tobytes(),count);emit('frames_read',raw(w.tobytes())+'/'+n(count),fs(symbols));d=R.CodecState();before=cs(d);out,bits=R.decode_frames(d,w.tobytes(),count,0,f);emit('frames_decode',n(mode)+'/'+before+'/'+ns((count,0))+'/'+raw(w.tobytes()),records(out)+'/'+n(bits),cs(d))
 # Direct caller-supplied symbol and packed-state failures retain exact mutation.
 for case in range(16):
  s=R.CodecState();symbols=R.read_frames(R.encode_frames(R.CodecState(),[R.rec12()]).tobytes(),1)
  if case==0:symbols.buttons=[]
  elif case==1:symbols.flags=[]
  elif case==2:symbols.yaw=[]
  elif case in (3,4,5,6):symbols.sticks[case-3]=[]
  elif case in (7,8):symbols.flags[0]= -1 if case==7 else 1<<32
  elif case in (9,10):symbols.buttons[0]= -1 if case==9 else 1<<16
  elif case==11:symbols.count=0
  elif case==12:symbols.sticks[0]=[(2,0)]
  elif case==13:s.ch[2].index=9
  elif case==14:s.ch[2].index=-1
  else:symbols.yaw[0]=('delta',100000)
  before=cs(s);encoded=fs(symbols);result=call(lambda:records(R.apply_frames(s,symbols)));emit('frames_apply',n(0)+'/'+before+'/'+n(0)+'/'+encoded,result,cs(s))
 for index in (-10,-9,-1,8,9):
  for pred in (-1000,0,255,1000):
   c=R.Channel(index,pred);before=ch(c);emit('axis_symbols',before+'/'+ns((-1,0,255,256)),call(lambda:sy(R.axis_symbols(c,[-1,0,255,256]))),ch(c))
 # Finite malformed branches; exhausted zero-stream loops are native-only
 # refusal anchors, not invented Python expected values.
 for op in ('axis_read','frames_read'):
  for count in (1,2,4,7,15,30):
   data=b'\xff'*128;r=R.BitReader(data)
   result=call(lambda:sy(R.read_axis_symbols(r,count)) if op=='axis_read' else fs(R.read_frames(data,count)))
   emit(op,raw(data)+'/'+n(count),result,n(r.pos) if op=='axis_read' else '-')
 for skip in (-1,2,2147483647):
  s=R.CodecState();symbols=R.read_frames(R.encode_frames(R.CodecState(),[R.rec12()]).tobytes(),1);before=cs(s);emit('frames_apply',n(0)+'/'+before+'/'+n(skip)+'/'+fs(symbols),call(lambda:records(R.apply_frames(s,symbols,skip))),cs(s))
 for i in range(16):
  samples=[(j*37+i)%256 for j in range(i*3)];result=R.fpu_sensitivity(samples);emit('sensitivity',ns(samples),'/'.join(raw(result[m]) for m in R.Fpu.MODELS))
 emit('frames_encode',cs(R.CodecState())+'/-','value',cs(R.CodecState()))
 out=('# '+SCHEMA+'\n'+'\n'.join(rows)+'\n').encode();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(out);trace=('\n'.join(rows)+'\n').encode();manifest={'schema_version':1,'schema':SCHEMA,'reference_commit':COMMIT,'reference_path':'tools/ref_netwire.py','reference_sha256':PIN,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(out).hexdigest(),'vector_count':len(rows),'op_counts':counts,'errors':errors,'expected_trace_sha256':hashlib.sha256(trace).hexdigest(),'scope':'Authored raw arithmetic, bitstreams, channels and frames; complete states under16 explicit FPU configurations. Infinite zero-stream cases excluded from Python corpus and separately refused by native budget anchors.'};a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n');print(len(rows),counts)
if __name__=='__main__':main()
