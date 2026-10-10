#!/usr/bin/env python3
"""Author integer AMX programs and record the pinned published Python interpreter.
No retail script images or engine-native code are read.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import types
PIN='5ad489a445d41612d0795c063e1ce67557d5c441'
SHA='091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f'


def enc(x):
    if x is None:return 'n'
    if isinstance(x,(list,tuple)):return '['+','.join(map(enc,x))+']'
    return str(x)


def run(r,image,operation,parameters,setup):
    m=r.Machine(image);events=[]
    mode=0;repeat=1
    def native(machine,args):
        events.append([0,list(args),machine.params,machine.cip,machine.frm,machine.hea,machine.stk])
        machine.set_cell(0,sum(args)+7)
        if mode==1:
            machine.hea+=4;machine.stk-=4;machine.raise_error(37)
        elif mode==2:
            error,result=machine.execute(1,[5],123);events.append([2,error,result,machine.stk]);return result
        elif mode==3:raise r.HostFault('authored native exception')
        elif mode==4:
            machine.profile={};machine._decoded=dict(machine._decoded)
        elif mode==5:machine.callback=None
        elif mode==6:machine.budget=0
        return sum(args)+0x100000001
    loaded=m.load();m.register({'native':native});m.profile={};m.budget=200
    for command in setup.split(';'):
        if not command:continue
        key,val=command.split('=');value=int(val)
        if key=='mode':mode=value
        elif key=='repeat':repeat=value
        elif key=='callback':
            if value==0:m.callback=None
            elif value==1:
                def custom(index,params):events.append([3,index,params]);return 41,99
                m.callback=custom
        elif key=='budget':m.budget=None if value==-999 else value
        elif key=='profile':m.profile=None
        elif key=='decoded':m._decoded=None
        elif key=='inject_none':m._decoded[0]=(0,None,4);m.flags|=r.FLAG_RELOC
        elif key=='parsed':m._parsed=bool(value)
        else:setattr(m,key,value)
    outcomes=[];codes=set()
    for _ in range(repeat):
        try:
            if operation=='call':result=m.call('missing' if parameters[0] else 'entry',parameters[1])
            elif operation=='divide':result=r.floored_div(*parameters)
            else:result=m.execute(parameters[0],parameters[2:],parameters[1])
            if operation=='execute':codes.add(result[0])
            outcomes.append(enc(result))
        except (r.HostFault,r.Limit,ZeroDivisionError) as e:
            outcomes.append('!'+type(e).__name__)
    state=[m.cip,m.frm,m.hea,m.stk,m.stp,m.flags,m.curline,m.curfile,m.dbgcode,m.dbgaddr,m.dbgparam,m.dbgname,m.error,m.hdr_stp,m.params,m.budget,None if m.profile is None else sorted(m.profile.items()),events]
    return ':'.join(outcomes),enc(state)+':'+m.image.hex(),set(() if m.profile is None else m.profile),codes


def generate(research,output):
    raw=subprocess.run(['git','-C',str(research),'show',PIN+':tools/ref_amx.py'],check=True,capture_output=True).stdout
    if hashlib.sha256(raw).hexdigest()!=SHA:raise ValueError('wrong pinned reference')
    sys.dont_write_bytecode=True;r=types.ModuleType('external_amx');exec(compile(raw,'pinned_ref_amx.py','exec'),r.__dict__)
    rows=[];counts={};coverage=set();error_classes=set();result_codes=set()
    def emit(name,program,args=(0,0),setup='',data=None,operation='execute',stack=64,compact=False):
        code,labels=r.assemble(program)
        if data is None:data=[4,0x11223344,0x7fffffff,-2147483648,0,1,2,3]+[0]*24
        publics=[('entry',0),('nested',labels.get('nested',0))]
        image=r.build_image(code,data,publics=publics,natives=['native'],stack_cells=stack,start=0,compact=compact)
        result,state,profile,codes=run(r,image,operation,list(args),setup)
        arg=image.hex()+'|'+','.join(map(str,args))+'|'+setup
        rows.append('\t'.join(['amx',operation,arg,result,state]));counts[name]=counts.get(name,0)+1;coverage.update(profile);result_codes.update(codes)
        for kind in ('HostFault','Limit','ZeroDivisionError'):
            if '!'+kind in result:error_classes.add(kind)
    halt=[('HALT',0)]
    patterns=[0,1,-1,127,128,255,0x7fffffff,-2147483648]
    # Every opcode, with valid and hostile operands. Each instruction has a
    # terminal HALT unless its own control transfer/exception determines exit.
    for opcode,(name,kind,_) in enumerate(r.OPCODES):
        for sample in range(8):
            pri=patterns[sample];alt=patterns[(sample+3)%8]
            operand=sample if name in ('LCTRL','SCTRL','ALIGN.pri','ALIGN.alt','LODB.I','STRB.I') else [0,4,8,-4,3,128,0x7fffffff,-2147483648][sample]
            prefix=[('PROC',),('CONST.pri',pri),('CONST.alt',alt)]
            inst=(name,) if kind==r.N else (name,operand)
            suffix=halt
            if kind==r.C:inst=(name,'target');suffix=[('CONST.pri',77),('HALT',0),'target:',('CONST.pri',88),('HALT',0)]
            if name in ('RET','RETN'):prefix=[('PROC',),('CONST.pri',pri)]
            elif name in ('CALL','CALL.pri'):
                prefix=[('PROC',),('PUSH.C',0)]
                if name=='CALL.pri':prefix += [('CONST.pri','target')];inst=(name,)
                else:inst=(name,'target')
                suffix=[('HALT',0),'target:',('PROC',),('CONST.pri',pri),('RETN',)]
            elif name=='JREL':inst=(name,8)
            elif name=='JUMP.pri':prefix=[('CONST.pri','target')];suffix=[('HALT',7),'target:',('HALT',0)]
            elif name=='FILE':inst=(name,sample,b'authored.cpp')
            elif name=='LINE':inst=(name,pri,alt)
            elif name=='SYMBOL':inst=(name,pri,alt,b'authored_symbol')
            elif name=='SRANGE':inst=(name,pri,alt)
            elif name=='SWITCH':
                inst=(name,'table');suffix=[('HALT',17),'target:',('HALT',3),'fallback:',('HALT',5),'table:',('CASETBL','fallback',[(1,'target'),(1,'fallback'),(-1,'target')])]
            elif name=='CASETBL':inst=(name,'target',[]);suffix=['target:',('HALT',0)]
            elif name in ('SYSREQ.C','SYSREQ.pri'):
                prefix=[('PROC',),('PUSH.C',pri),('PUSH.C',4),('CONST.pri',0)];inst=(name,0) if name=='SYSREQ.C' else (name,)
            elif name in ('MOVS','CMPS','FILL'):
                prefix=[('CONST.pri',0),('CONST.alt',4)];inst=(name,[0,1,3,4,8,40,400,-1][sample])
            elif name in ('LOAD.I','STOR.I','LIDX','LIDX.B','INC.I','DEC.I'):
                prefix=[('CONST.pri',[0,4,3,-4,128,384,400,0x7fffffff][sample]),('CONST.alt',4)]
            emit(name,prefix+[inst]+suffix,args=(0,pri,11,-12) if sample%2 else (0,pri))
    # Zero opcode cannot pass relocation. Explicit decoded-state injection checks
    # the otherwise unreachable invalid-instruction dispatcher path as well.
    emit('NONE',[('HALT',0)],setup='inject_none=1')
    # Exact integer edge division/comparison/shift programs, independent of the
    # generalized opcode smoke programs above.
    randoms=random.Random(0xa4e2026)
    arithmetic=['SDIV','SDIV.alt','UDIV','UDIV.alt','SMUL','UMUL','ADD','SUB','SUB.alt','SHL','SHR','SSHR']
    pairs=list(__import__('itertools').product(patterns,patterns))+[(r.s32(randoms.getrandbits(32)),r.s32(randoms.getrandbits(32))) for _ in range(256)]
    for name in arithmetic:
        for a,b in pairs:
            emit(name,[('CONST.pri',a),('CONST.alt',b),(name,),('STOR.alt',4),('HALT',0)])
    for name in ['JEQ','JNEQ','JLESS','JLEQ','JGRTR','JGEQ','JSLESS','JSLEQ','JSGRTR','JSGEQ']:
        for a,b in __import__('itertools').product(patterns,patterns):
            emit(name,[('CONST.pri',a),('CONST.alt',b),(name,'yes'),('CONST.pri',0),('HALT',0),'yes:',('CONST.pri',1),('HALT',0)])
    for op in ['MOVS','CMPS','FILL']:
        for source,target,count in __import__('itertools').product([0,1,4,380,384,-1],[0,1,4,380,384,-1],[0,1,3,4,8,16,-1]):
            emit(op,[('CONST.pri',source),('CONST.alt',target),(op,count),('HALT',0)])
    for op in ['STACK','HEAP','BOUNDS','LOAD.I','STOR.I']:
        for value in [-2147483648,-400,-64,-1,0,4,64,400,2147483647]:
            p=[('PROC',),('PUSH.C',0),('SYSREQ.C',0),('CONST.pri',value),('CONST.alt',value)]
            p += [(op,value)] if r.OPCODES[r.OP[op]][1]==r.V else [(op,)]
            emit(op,p+halt)
    native_program=[('PROC',),('PUSH.C',9),('PUSH.C',-7),('PUSH.C',8),('SYSREQ.C',0),('HALT',0),'nested:',('PROC',),('LOAD.S.pri',12),('ADD.C',10),('RETN',)]
    for mode,budget in __import__('itertools').product(range(7),[0,1,5,15,100]):
        emit('native',native_program,setup=f'mode={mode};budget={budget}')
    for setup in ['callback=0','callback=1','flags=16384','flags=49152;parsed=0','decoded=0','hea=-1','stk=2147483647','hea=4294967296','stp=4294967296','profile=0','budget=-1','budget=0','repeat=3','hdr_stp=0']:
        emit('entry',[('PROC',),('CONST.pri',13),('RETN',)],setup=setup)
    for index in [-3,-2,-1,0,1,2,2147483647]:emit('entry',[('HALT',9)],args=(index,-99))
    for initial in [-2147483649,-2147483648,0,2147483647,4294967295,4294967296]:emit('entry',[('HALT',9)],args=(0,initial))
    for missing,code in __import__('itertools').product([0,1],[0,3,11]):emit('call',[('CONST.pri',73),('HALT',code)],args=(missing,17),operation='call')
    # Retain the historical saved-STK drift after nested RETN then native/abort.
    for end in [[('HALT',0)],[('BOUNDS',-1)],[('STACK',-400)],[('HEAP',-500)]]:
        p=[('PROC',),('PUSH.C',4),('PUSH.C',4),('CALL','sub')]+end+['sub:',('PROC',),('LOAD.S.pri',12),('RETN',)]
        emit('retn_drift',p,setup='repeat=3')
    # Direct dispatcher and debug/case boundaries, compact execution, jumps into
    # operands, and callbacks replacing captured profile/code maps.
    emit('switch_invalid',[('CONST.pri',1),('SWITCH','bad'),'bad:',('HALT',0)])
    emit('jump_invalid',[('JUMP',4),('HALT',0)])
    emit('jump_invalid',[('CONST.pri',3),('SCTRL',6),('HALT',0)])
    emit('compact',[('PROC',),('CONST.pri',19),('RETN',)],compact=True)
    emit('debug',[('FILE',2,b'fixture'),('LINE',77,9),('SYMBOL',4,12,b'node'),('SRANGE',8,16),('HALT',0)])
    emit('native',native_program,setup='callback=1')
    emit('wide_control',[('LCTRL',2),('MOVE.alt',),('CONST.pri',7),('UDIV',),('HALT',0)],setup='hea=4294967296')
    emit('wide_control',[('LCTRL',2),('SYSREQ.pri',),('HALT',0)],setup='hea=4294967296')
    for reg in [2,3]:emit('wide_control',[('LCTRL',reg),('HALT',0)],setup=('hea' if reg==2 else 'stp')+'=4294967296')
    if coverage!=set(range(131)):raise AssertionError(('missing opcodes',set(range(131))-coverage))
    if not {'HostFault','Limit','ZeroDivisionError'}<=error_classes:raise AssertionError('missing failure coverage')
    required_codes={0,3,4,5,6,7,8,9,11,20,22,37,41}
    if not required_codes<=result_codes:raise AssertionError(('missing errorcodes',required_codes-result_codes))
    body=('\n'.join(rows)+'\n').encode('ascii');content=b'# amx_execute_vectors_v1\n'+body;output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(content)
    meta={'schema_version':1,'reference_commit':PIN,'reference_path':'tools/ref_amx.py','reference_sha256':SHA,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(content).hexdigest(),'vector_count':len(rows),'op_counts':counts,'expected_trace_sha256':hashlib.sha256(body).hexdigest(),'executed_opcodes':sorted(coverage),'error_classes':sorted(error_classes),'returned_codes':sorted(result_codes),'scope':'Authored programs exercising published integer interpreter, complete image and savedstate/native trace/profile; no game-native implementation or original game data.'}
    output.with_suffix('.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(meta,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',type=Path,required=True);p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'native/tests/vectors/amx_execute.tsv');a=p.parse_args();generate(a.research_dir.resolve(),a.output.resolve())
