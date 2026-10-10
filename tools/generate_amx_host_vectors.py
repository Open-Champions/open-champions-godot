#!/usr/bin/env python3
"""Authored AMX host fixtures from a pinned external Python model; no game scripts."""
import argparse,collections,hashlib,json,pathlib,random,struct,subprocess,types
COMMIT='5ad489a445d41612d0795c063e1ce67557d5c441'
PIN='091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f'
HEADER_FIELDS=('size','cod','dat','hea0','start','num_publics','publics_off','num_natives','natives_off','num_libraries','num_pubvars','data_offset')
def sc(v):
    if v is None:return '_'
    if isinstance(v,bool):return str(int(v))
    if isinstance(v,bytes):return 'b'+v.hex()
    if isinstance(v,str):return 's'+v.encode('latin1').hex()
    if isinstance(v,(list,tuple)):return '['+','.join(sc(x) for x in v)+']'
    return str(v)
def generate(research,output):
    source=subprocess.run(['git','-C',str(research),'show',COMMIT+':tools/ref_amx.py'],check=True,capture_output=True).stdout
    assert hashlib.sha256(source).hexdigest()==PIN
    r=types.ModuleType('pinned');exec(compile(source,'pinned_ref_amx.py','exec'),r.__dict__)
    code,labels=r.assemble(['outer:',('PROC',),('PUSH.C',7),('PUSH.C',4),('SYSREQ.C',0),('STACK',8),('RETN',),'inner:',('PROC',),('LINE',321,7),('PUSH.C',99),('PUSH.C',4),('SYSREQ.C',1),('STACK',8),('RETN',)])
    base=r.build_image(code,data=[0]*32,publics=[('outer',labels['outer']),('inner',labels['inner'])],natives=[b'f0',b'f1',b'\xffx'],stack_cells=48,start=labels['outer'])
    rows=[];counts=collections.Counter()
    def emit(op,commands,image=base):
        events=[];saved={};cb=2 if struct.unpack_from('<I',image,4)[0] else 0
        m=r.Machine(image)
        def native(name,variant):
            def fn(machine,args):
                events.append(('enter',name,variant,args,machine.params,machine.error))
                if variant==0:result=sum(args)+100
                elif variant==1:machine.raise_error(77);result=None
                elif variant==2:
                    e,v=machine._call_native(1,32);events.append(('nested_native',e,v,machine.params,machine.error));result=v+5
                elif variant==3:
                    machine.raise_error(45);e,v=machine.execute(0);events.append(('nested_exec',e,v,machine.params,machine.error));result=(v or 0)+7
                elif variant==4:machine._bound=None;machine.error=123;result=(1<<63)-1
                elif variant==5:machine.set_cell(12,0xabcddcba);raise r.HostFault('authored native fault')
                elif variant==6:result=-(1<<63)
                else:result=None
                events.append(('exit',name,result,machine.params,machine.error));return result
            fn.label='H'+str(variant)+':'+name.hex();return fn
        results=[]
        for command in commands:
            p=command.split(',');name=p[0];a=lambda i:int(p[i])
            try:
                if name=='load':v=m.load();cb=cb or (1 if m.callback else 0)
                elif name=='cell':v=m.cell(a(1))
                elif name=='put':m.set_cell(a(1),a(2));v=None
                elif name=='mem':v=m.memory()
                elif name=='addr':v=m.get_addr(a(1))
                elif name=='get':v=m.get_string(a(1))
                elif name=='len':v=m.str_len(a(1))
                elif name=='set':v=m.set_string(a(1),bytes.fromhex(p[3]),bool(a(2)))
                elif name=='allot':v=m.allot(a(1))
                elif name=='release':v=m.release(a(1))
                elif name=='error':v=m.raise_error(a(1))
                elif name=='reg':
                    names=[b'f0',b'f1',b'\xffx'];v=m.register({n:native(n,a(2) if i==0 else 1 if i==1 else 7) for i,n in enumerate(names) if a(1)&(1<<i)})
                elif name=='pending':m._pending_natives={n:native(n,a(2) if i==0 else 1 if i==1 else 7) for i,n in enumerate([b'f0',b'f1',b'\xffx']) if a(1)&(1<<i)};v=None
                elif name=='parsed':m._parsed=bool(a(1));v=None
                elif name=='data':m.data_offset=a(1);v=None
                elif name=='native':v=m._call_native(a(1),a(2))
                elif name=='cb':
                    cb=a(1)
                    if cb==0:m.callback=None
                    elif cb==1:m.callback=m._call_native
                    elif cb==2:m.callback=m._foreign_callback
                    else:
                        def custom(index,params):events.append(('callback',index,params));m.error=91;return 23,0x1ffffffff
                        m.callback=custom
                    v=None
                elif name=='exec':v=m.execute(a(1),params=[int(x) for x in p[3:]],pri=a(2))
                elif name=='call':v=m.call(bytes.fromhex(p[1]),a(2))
                elif name=='snap':saved[a(1)]=m.snapshot();v=None
                elif name=='restore':
                    snap=saved[a(1)];m.restore((snap[0],snap[1][:a(2)]));v=None
                elif name=='state':
                    key=list(r.Machine._STATE)[a(1)];setattr(m,key,None if p[2]=='_' else a(2));v=None
                elif name=='params':m.params=a(1);v=None
                elif name=='budget':m.budget=None if p[1]=='_' else a(1);v=None
                elif name=='profile':m.profile={11:9} if a(1) else None;v=None
                elif name=='raw':m.image[a(1)]=a(2);v=None
                elif name=='clearbound':m._bound=None;v=None
                elif name=='cleardecoded':m._decoded=None;v=None
                elif name=='find':v=m.find_public(bytes.fromhex(p[1]))
                else:raise AssertionError(name)
                results.append(sc(v))
            except r.HostFault:results.append('!HostFault')
            except r.Limit:results.append('!Limit')
            except AttributeError:results.append('!HostFault') # unparsed data/header refusal
        bindings=None if m._bound is None else [('U' if x is None else 'F' if x is r._FOREIGN else x.label) for x in m._bound]
        decoded=None if m._decoded is None else [(k,*v) for k,v in sorted(m._decoded.items())]
        state='/'.join([sc(bytes(m.image)),sc(tuple(getattr(m,k,None) for k in m._STATE)),sc(m.params),sc(m.budget),sc(None if m.profile is None else sorted(m.profile.items())),sc(bindings),str(cb),sc(m._parsed),sc([getattr(m,k,None) for k in HEADER_FIELDS]),sc(decoded),sc(events),sc([(k,v.label) for k,v in sorted(m._pending_natives.items())]),sc([(k,v[0],v[1]) for k,v in sorted(saved.items())])])
        args=str(len(rows))+';'+image.hex()+';'+'|'.join(commands)
        rows.append('\t'.join(['amx-host',op,args,'~'.join(results),state]));counts[op]+=1
    for address in [-2**63,-2147483648,-5,-4,-1,0,1,2,3,4,124,128,252,312,316,320,324,2**31,2**63-1]:
        for value in [0,1,-1,0x7fffffff,0x80000000,0xffffffff,0x1ffffffff]:emit('memory',['load',f'put,{address},{value}',f'cell,{address}',f'addr,{address}','mem'])
    strings=[b'',b'A',b'ABC',b'ABCD',b'ABCDE',bytes(range(1,33)),b'\xffx',b'\x80ABC',b'\x7f\xff\x80\x01',b'ab\0discard',b'\0x',b'a\xffb']
    for text in strings:
        for pack in [0,1]:
            for address in [-4,0,1,4,300,308,312,316,320]:emit('string',['load',f'set,{address},{pack},{text.hex()}',f'get,{address}',f'len,{address}','mem'])
    for first in [-2147483648,-1,0,1,255,256,0x41420000,0x41424344,0x00410042,0x7fffffff]:
        emit('detection',['load',f'put,0,{first}','put,4,256','put,8,0','get,0','len,0','mem'])
    for hea in [-2**63,-1,0,128,300,2**31-1,2**63-1]:
        for stk in [-1,0,64,128,316,2**63-1]:
            for cells in [-2**63,-2147483648,-17,-1,0,1,16,47,48,64,2147483647,2**63-1]:emit('heap',['load',f'state,2,{hea}',f'state,3,{stk}',f'allot,{cells}','release,-9','release,999','addr,-4'])
    for variant in range(8):
        for count in [-2147483648,-5,0,1,3,4,7,8,16,128,2147483647]:
            emit('dispatch',['load',f'reg,7,{variant}',f'put,0,{count}','put,4,11','put,8,-7','put,32,4','put,36,23','error,999','native,0,0','native,1,32'])
    for index in [-2**63,-1,0,1,2,3,1<<32,2**63-1]:
        for mask in [0,1,2,3,7]:emit('binding',['load',f'reg,{mask},0','put,0,0',f'native,{index},0','reg,7,7',f'native,{index},0'])
    for pos in [-4,1,2,3,316,320,2**63-1]:emit('params',['load','reg,7,0','error,999','params,44',f'native,0,{pos}'])
    for cb in range(4):
        for variant in range(4):
            for budget in [0,1,5,100]:emit('nested',['load',f'reg,7,{variant}',f'cb,{cb}',f'budget,{budget}','profile,1','exec,1,5','call,6d697373696e67,3','call,696e6e6572,17'])
    for prefix in range(16):
        for loaded in [False,True]:
            commands=['load'] if loaded else []
            commands+=['snap,0']+[f'state,{i},{1000+i}' for i in range(14)]+['params,999','budget,19','profile,1','cb,3','raw,0,55']
            if loaded:commands+=['reg,7,0','clearbound','reg,7,1','cleardecoded','parsed,0','data,4']
            commands+=['pending,7,5']
            commands+=[f'restore,0,{prefix}','snap,1','raw,1,77','mem']
            emit('snapshot',commands)
    for before in [[],['load']]:emit('unloaded',before+['cell,0','mem','set,0,0,6162','get,0','len,0','reg,7,0','native,0,0','allot,2','release,-4','addr,-4','snap,0'])
    for mask in range(8):emit('pending',[f'pending,{mask},0','load','put,0,0','native,0,0','reg,7,7','native,2,0'])
    foreign=bytearray(base);struct.pack_into('<I',foreign,4,0x12345678)
    emit('foreign',['load','reg,7,0','put,0,0','native,0,0','exec,1,0'],bytes(foreign))
    # A foreign record is never overwritten by registration.
    foreign=bytearray(base);natives_off=struct.unpack_from('<i',foreign,0x60+0x2c)[0];struct.pack_into('<I',foreign,0x60+natives_off,0x12345678)
    emit('foreign',['load','reg,7,0','native,0,0'],bytes(foreign))
    # Rebinding/replacing the vector inside its active native retains the callable.
    emit('rebind',['load','reg,1,4','put,0,0','native,0,0','reg,7,0','native,0,0'])
    data=('# Authored AMX host vectors; no game scripts\n'+'\n'.join(rows)+'\n').encode();output.write_bytes(data)
    meta=dict(schema=1,reference='tools/ref_amx.py',reference_commit=COMMIT,reference_sha256=PIN,generator_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),vector_sha256=hashlib.sha256(data).hexdigest(),rows=len(rows),op_counts=dict(counts),expected_trace_sha256=hashlib.sha256(('\n'.join(rows)+'\n').encode()).hexdigest(),scope='Authored images/native callbacks only. All14 snapshot fields and complete bytes; bindings/callback/params/profile/budget/decoded remain excluded from restore. Undefined preparse Python attributes are normalized to HostFault. No game-native handlers.')
    output.with_suffix('.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--research-dir',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/amx_host.tsv'));a=p.parse_args();generate(a.research_dir,a.output)
