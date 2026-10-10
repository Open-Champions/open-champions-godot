#!/usr/bin/env python3
"""AMX image contract vectors, entirely authored; no retail input or executable."""
import argparse,hashlib,importlib.util,json,random,struct,subprocess,sys
from pathlib import Path
PIN='091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f'
COMMIT='5ad489a445d41612d0795c063e1ce67557d5c441'
SCHEMA='rta-amx-image-v1'
STATE=('cip','frm','hea','stk','stp','flags','curline','curfile','dbgcode','dbgaddr','dbgparam','dbgname','error','hdr_stp')
HEADER=('size','cod','dat','hea0','start','num_publics','publics_off','num_natives','natives_off','num_libraries','num_pubvars','data_offset')
def num(x):return f'{x&((1<<64)-1):016x}'
def nums(a):return ','.join(num(x) for x in a) or '-'
def raw(x):return bytes(x).hex() or '-'
def encode(x):
 if x is None:return 'N'
 if isinstance(x,(tuple,list)):return '['+';'.join(encode(v) for v in x)+']'
 return num(x)
def state(m,R):
 decoded=None if m._decoded is None else [(off,*item) for off,item in sorted(m._decoded.items())]
 bound=None if m._bound is None else [0 if x is None else 1 if x is R._FOREIGN else 2 for x in m._bound]
 return '|'.join([raw(m.image),encode([getattr(m,n,None) for n in STATE]),encode([getattr(m,n,None) for n in HEADER]),num(m._parsed),encode(decoded),encode(bound)])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',type=Path,required=True);p.add_argument('--output',type=Path,default=Path('native/tests/vectors/amx_image.tsv'));a=p.parse_args()
 path=a.research_dir/'tools/ref_amx.py';src=path.read_bytes();assert hashlib.sha256(src).hexdigest()==PIN
 assert hashlib.sha256(subprocess.check_output(['git','-C',str(a.research_dir),'show',COMMIT+':tools/ref_amx.py'])).hexdigest()==PIN
 sys.dont_write_bytecode=True;s=importlib.util.spec_from_file_location('amx_reference',path);R=importlib.util.module_from_spec(s);s.loader.exec_module(R)
 rows=[];counts={};errors={};rng=random.Random(0x414d5849)
 def emit(op,args,result,st='-'):
  rows.append('\t'.join(['amx-image',op,str(len(rows))+'|'+args,result,st]));counts[op]=counts.get(op,0)+1
  if result in ['value','host','limit']:errors[result]=errors.get(result,0)+1
 def image(data,mode=0,name=b'run'):
  m=None
  try:
   m=R.Machine(data,{'native':lambda m,args:7} if mode==3 else None)
   if mode==4:result=encode((m.cip,m.frm,m.hea,m.stk,m.stp,m.flags,m.error))
   else:
    result=m.load()
    if mode==1:result=(result,m.load())
    elif mode==2:result=(result,m.find_public(name))
    result=encode(result)
  except R.HostFault:result='host'
  except (ValueError,KeyError):result='value'
  except (IndexError,struct.error):result='host' # bounded owned-image refusal
  emit('image',nums([mode])+'|'+raw(name)+'|'+raw(data),result,state(m,R) if m else '-')
 def patch(data,off,fmt,value):
  b=bytearray(data);struct.pack_into(fmt,b,off,value);return bytes(b)
 simple=R.build_image([R.OP['PROC'],R.OP['CONST.pri'],7,R.OP['RETN']],data=[3,4],publics=[('run',0)],natives=['native'],stack_cells=32)
 for mode in range(5):image(simple,mode)
 for size in range(161):image(simple[:size],4)
 for off,fmt,values in [(0x64,'<H',[0,0xf1df,0xf1e0,65535]),(0x6a,'<H',[0,23,24,25,65535]),(0x67,'<B',[0,1,3,4,127,128,255]),(0x68,'<H',[0,1,2,3,4,5,8,0xffff]),(0x78,'<i',[-2147483648,-1,0,1,3,4,100,2147483646,2147483647]),(0x20,'<I',[0,0x2000,0x4000,0x8000,0xa000,0xffffffff]),(4,'<I',[0,1,0xffffffff])]:
  for value in values:
   for mode in [0,1]:image(patch(simple,off,fmt,value),mode)
 for off in [0x60,0x6c,0x70,0x74,0x7c,0x84,0x8c]:
  for value in [-2147483648,-4,-1,0,1,3,4,0x40,0x7fffffff]:image(patch(simple,off,'<i',value))
 for off in [0x80,0x88,0x90,0x98]:
  for value in [-32768,-1,0,1,2,32767]:image(patch(simple,off,'<h',value),2)
 for op,(name,kind,_) in enumerate(R.OPCODES):
  for seed in range(10):
   v=rng.randrange(-20,21)
   if kind in (R.V,R.C):code=[op,v]
   elif kind==R.FILE:code=[op,8,v,0]
   elif kind==R.LINE:code=[op,v,seed]
   elif kind==R.SYMBOL:code=[op,12,v,seed,0]
   elif kind==R.SRANGE:code=[op,v,seed]
   elif kind==R.CASETBL:code=[op,2,0,1,0,-1,0]
   else:code=[op]
   image(R.build_image(code,stack_cells=32))
   if seed<2:image(R.build_image(code[:-1],stack_cells=32))
 for op in [R.OP['FILE'],R.OP['SYMBOL'],R.OP['CASETBL']]:
  for count in [-2147483648,-16,-4,-3,-2,-1,0,1,2,3,4,7,8,12,2147483647]:image(R.build_image([op,count,0,0,0,0,0,0],stack_cells=32))
 for op in [-2147483648,-1,0,0x83,0xff,0x100,0x7fffffff]:image(R.build_image([op,0],stack_cells=32))
 names=[b'',b'a',b'a\x00tail',b'b',b'z',b'\x80',b'\xff']
 for sorted_ in [False,True]:
  img=R.build_image([R.OP['HALT'],0],publics=[(n,i*4) for i,n in enumerate(names)],stack_cells=32,sort_publics=sorted_)
  for n in names+[b'missing',b'\x7f',b'\x81']:image(img,2,n)
 for i in range(256):
  code=[R.OP['CONST.pri'],rng.randrange(-64,64),R.OP['HALT'],i%7]
  data=[rng.randrange(-8192,8192) for _ in range(i%17)]
  img=R.build_image(code,data=data,publics=[('run',0)],stack_cells=20+i%11,compact=True)
  image(img,i%4)
 for i in range(512):
  cells=[R.s32(rng.getrandbits(32)) for _ in range(i%13)];encoded=R.encode_compact(cells)
  emit('encode',nums(cells),raw(encoded))
  # expansion is independent of in-place prefix restrictions here: bytes are
  # positioned at the end of a large authored zero buffer.
  buffer=bytearray(max(len(encoded),len(cells)*4)+12);buffer[:len(encoded)]=encoded
  result='ok'
  try:R.expand_compact(buffer,0,len(encoded),len(cells)*4)
  except R.HostFault:result='host'
  emit('expand',nums([len(encoded),len(cells)*4])+'|'+raw(bytearray(encoded)+bytes(len(buffer)-len(encoded))),result,raw(buffer))
 for rawcode,expanded in [(b'\x80'*9+b'\x01',4),(b'\x7f',0),(b'\x7f',3),(b'\x01\x02',4),(b'\xff\xff\xff\xff\x7f',8),(b'\x81',4)]:
  buf=bytearray(rawcode+bytes(16));before=bytes(buf);res='ok'
  try:R.expand_compact(buf,0,len(rawcode),expanded)
  except R.HostFault:res='host'
  emit('expand',nums([len(rawcode),expanded])+'|'+raw(before),res,raw(buf))
 # Build and assemble helpers use an authored family with all variable metadata.
 for i in range(256):
  program=['start:',('FILE',i,b'file'),('LINE',i,-i),('SYMBOL',i,3,b'symbol'),('SRANGE',i,4),('SWITCH','table'),('HALT',i),'table:',('CASETBL','start',[(i,'start'),(-i,'table')])]
  cells,labels=R.assemble(program);emit('assemble',nums([i]),nums(cells),nums([labels['start'],labels['table']]))
  code=[R.OP['CONST.pri'],i-128,R.OP['HALT'],0];dat=[i,-i];compact=bool(i&1)
  img=R.build_image(code,data=dat,publics=[(b'z',0),(b'\x80',4),(b'a',8)],natives=[b'native'],stack_cells=8+i%9,start=0,flags=i&2,file_version=-1,amx_version=3,compact=compact,sort_publics=bool(i&2))
  emit('build',nums([i]),raw(img))
 out=('# '+SCHEMA+'\n'+'\n'.join(rows)+'\n').encode();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(out);trace=b''.join(x+b'\n' for x in out.splitlines() if not x.startswith(b'#'))
 manifest={'schema_version':1,'schema':SCHEMA,'reference_commit':COMMIT,'reference_path':'tools/ref_amx.py','reference_sha256':PIN,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(out).hexdigest(),'vector_count':len(rows),'op_counts':counts,'errors':errors,'expected_trace_sha256':hashlib.sha256(trace).hexdigest(),'scope':'Authored images only; complete mutated image, saved/header fields, decoded operands and binding state. Outside-owned-buffer IndexError/struct.error normalized to HostFault.'}
 a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n');print(len(rows),'AMX image vectors',counts)
if __name__=='__main__':main()
