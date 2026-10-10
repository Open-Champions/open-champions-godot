#!/usr/bin/env python3
"""Synthetic RTA session traces from one pinned, published Python model.
No socket, retail data, emulator, or unpublished research is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import types
PIN='54258f9d7e281ef07f0675040e82e6264f9386cf'
SHA='4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'
GLOBAL='network_mode num_peers lif last_sent net_frame cycle phase modem last_hash_frame stall ended eq min_ready win long_stall kicked kick_state msg_attached sim_frame rand_seed sync_acc level_pending num_players'.split()
PF='port send_frame msg_write msg_read newest acked lead sim silence heard local dropped kick oos drop_frame kicked_notice'.split()

def enc(x):
    if x is None:return 'n'
    if isinstance(x,bool):return str(int(x))
    if isinstance(x,(bytes,bytearray)):return x.hex()
    if isinstance(x,(list,tuple)):return '['+','.join(map(enc,x))+']'
    return str(x)
def sent(s):return [s.peer,s.ip,s.port,s.data,list(s.undefined)]
def packet(p):
    s=p.symbols
    symbols=None if s is None else [s.count,s.buttons,s.sticks,[[{'keep':0,'delta':1,'abs':2}[v[0]],v[1] if len(v)>1 else 0] for v in s.yaw],s.flags,s.bits]
    return [p.kind,p.last,p.slot,p.mask,p.byte9,p.count,p.ack_delta,p.first,p.ack,p.sim_frame,symbols,p.raw_records,p.stream_bytes,p.sync_offset,p.sync,p.sync_word,p.blocks,p.blocks_offset]
def snapshot(m,events):
    state=[[getattr(m,k) for k in GLOBAL],m.kick_acks,m.cam_yaw,m.input_lock,m.send_cycles,m.positions,[m.pad_globals.start_block,m.pad_globals.mute,m.pad_globals.allow],[[getattr(p,k) for k in PF]+[p.pad_prev] for p in m.peers],[sent(s) for s in m.outbox],[x.encode().hex() for x in m.log],events]
    return enc(state)+':'+':'.join([p.pack().hex() for p in m.peers]+[m.sim_enc.pack().hex(),m.sim_dec.pack().hex()]+[p.hex() for p in m.pads])
def fill(r,m,slot,first,count,seed):
    p=m.peers[slot]
    for i in range(count):
        frame=first+i
        b=bytes(((seed+frame*17+j*31)&255) for j in range(12))
        p.set_record(frame,b,start=0,count=0)
def run(r,script):
    m=None;events=[];results=[]
    for cmd in script.split(';'):
        a=cmd.split(',');op=a[0]
        try:
            if op=='init':m=r.Machine(int(a[1]),int(a[2]),r.Fpu(a[3],bool(int(a[4])),bool(int(a[5]))));continue
            if op=='m':setattr(m,a[1],int(a[2]));continue
            if op=='p':setattr(m.peers[int(a[1])],a[2],int(a[3]));continue
            if op=='fill':fill(r,m,*map(int,a[1:]));continue
            if op=='sync':m.peers[int(a[1])].set_sync(*map(int,a[2:]));continue
            if op=='record':
                slot,f=map(int,a[1:3]);m.peers[slot].set_record(f,bytes.fromhex(a[3]),None if a[4]=='n' else int(a[4]),None if a[5]=='n' else int(a[5]));continue
            if op=='ring':
                p=m.peers[int(a[1])];pos=int(a[2]);data=bytes.fromhex(a[3]);
                for i,b in enumerate(data):p.msg_ring[(pos+i)&8191]=b
                continue
            if op=='peerimage':m.peers[int(a[1])]=r.Peer.unpack(bytes.fromhex(a[2]));continue
            if op=='hook':
                index,mode=map(int,a[1:])
                def hook(payload,index=index,mode=mode):
                    events.append([index,payload]);m.cycle+=1
                    if mode==1:m.peers[1].dropped=1;m.num_peers=0;m.direct_handlers[index]=None
                    if mode==2:raise ValueError('authored callback')
                    if mode==3:m.queue_message(index,payload)
                m.direct_handlers[index]=hook;continue
            if op=='stack':
                mode=int(a[1])
                def stack(slot,frame):
                    events.append([slot,frame]);m.phase+=1
                    if mode==2 and m.phase==2:raise ValueError('authored pressure callback')
                    if mode==3:m.stack_bytes=None
                    return bytes((slot&255,frame&255))
                m.stack_bytes=stack;continue
            if op=='header':v=r.header(*map(int,a[1:]))
            elif op=='parse':v=packet(r.parse_packet(bytes.fromhex(a[1])))
            elif op=='kick_request':v=r.kick_request(*map(int,a[1:]))
            elif op=='notice':v=r.kicked_notice()
            elif op=='lan':v=r.lan_packet(bool(int(a[1])))
            elif op=='direct':v=r.direct_message(int(a[1]),bytes.fromhex(a[2]))
            elif op=='queries':v=[m.local_slot,m.active_mask(),m.remote_active(),m.is_first_active()]
            elif op=='pack':v=m.peers[int(a[1])].pack()
            elif op=='unpack':v=r.Peer.unpack(bytes.fromhex(a[1])).pack()
            elif op=='getrecord':v=m.peers[int(a[1])].record(int(a[2]))
            elif op=='getsync':v=m.peers[int(a[1])].sync(int(a[2]))
            elif op=='queue':v=m.queue_message(int(a[1]),bytes.fromhex(a[2]))
            elif op=='attach':v=m.attach_messages()
            elif op=='messages':v=m.frame_messages(*map(int,a[1:]))
            elif op=='build':
                dest,first,last,kind,owner,raw=map(int,a[1:]);v=sent(m.build_packet(dest,first,last,kind,owner,None if raw else m.peers[dest].send_state,bool(raw)))
            elif op=='send':v=sent(m.send_input(int(a[1])))
            elif op=='sendmutable':
                payload=bytearray.fromhex(a[2])
                def own(arg):
                    events.append([arg]);payload.clear();events.append([arg])
                m.direct_handlers[0]=own;v=m.send_direct(0,int(a[1]),payload);m.direct_handlers[0]=None
            elif op=='receive_mutate':
                payload=bytearray.fromhex(a[1])
                def own(slot,frame):
                    events.append([slot,frame]);payload.clear();return bytes((17,23))
                m.stack_bytes=own;v=m.receive(payload);m.stack_bytes=None
            elif op=='senddirect':v=m.send_direct(int(a[1]),int(a[2]),bytes.fromhex(a[3]))
            elif op=='kick':v=m.kick_peer(int(a[1]))
            elif op=='receive':v=m.receive(bytes.fromhex(a[2]),bool(int(a[1])))
            elif op=='dispatch':v=m.dispatch(bytes.fromhex(a[1]))
            else:raise RuntimeError('unknown command '+op)
            results.append(enc(v))
        except (r.MalformedPacket,ValueError,IndexError) as e:results.append('!'+type(e).__name__)
    return ':'.join(results),snapshot(m,events)

def generate(research,out):
    raw=subprocess.run(['git','-C',str(research),'show',PIN+':tools/ref_netwire.py'],check=True,capture_output=True).stdout
    if hashlib.sha256(raw).hexdigest()!=SHA:raise ValueError('pinned reference hash mismatch')
    sys.dont_write_bytecode=True;r=types.ModuleType('pinned_netwire');exec(compile(raw,'pinned_ref_netwire.py','exec'),r.__dict__)
    rows=[];counts={};operations=set()
    def emit(group,commands,num=2,local=0,config=('ps2',0,0)):
        script=';'.join([f'init,{num},{local},{config[0]},{config[1]},{config[2]}']+commands)
        result,state=run(r,script)
        # Unique sequence number is carried as an unused label, covered by corpus digest.
        args=str(len(rows))+'|'+script
        rows.append('\t'.join(['netwire-session',group,args,result,state]));counts[group]=counts.get(group,0)+1
        operations.update(c.split(',')[0] for c in commands)
    values=[0,1,-1,127,128,255,256,0x7fffffff,-2147483648,0xffffffff,0x100000000]
    for x in values:emit('header',[f'header,{x},{x},{x},{x},{x},{x},{x}',f'kick_request,{x},{x}','notice','lan,0','lan,1'])
    for x in [-1,0,1,2,3,4]:emit('direct',[f'direct,{x},00017f80ff',f'senddirect,{x},15,00017f80ff'])
    for n in [-1,0,1,2,3,4,5]:
        for local in [-4,-1,0,3]:emit('queries',['queries','m,network_mode,0','queries'],n,local)
    for flags in range(64):
        emit('queries',[f'p,1,dropped,{flags&1}',f'p,1,kick,{flags>>1&1}',f'p,1,oos,{flags>>2&1}',f'p,0,dropped,{flags>>3&1}',f'p,0,local,{flags>>4&1}',f'p,3,local,{flags>>5&1}','queries'],4)
    for f in [-257,-1,0,1,255,256,257,0x7fffffff,0xffffffff,0x100000000]:
        emit('rings',[f'record,1,{f},00112233445566778899aabb,4294967295,4294967296',f'getrecord,1,{f}',f'getrecord,1,{f+256}',f'record,1,{f},ffeeddccbbaa998877665544,n,n',f'getrecord,1,{f}',f'sync,1,{f},4294967295,2147483648',f'getsync,1,{f}','pack,1'])
    rnd=random.Random(0x5373)
    for i in range(12):
        p=r.Peer();p.ip=bytes(rnd.randrange(256) for _ in range(4));p.unknown8=bytes(rnd.randrange(256) for _ in range(8))
        for field in PF:setattr(p,field,rnd.randrange(-2**40,2**40))

        for state in (p.recv_state,p.send_state):
            for channel in state.ch:
                channel.index=rnd.randrange(9);channel.pred=rnd.randrange(256);channel.value=rnd.getrandbits(32);channel.velocity=rnd.getrandbits(32)
            state.yaw_prev=rnd.randrange(-32768,32768);state.yaw_pred=rnd.randrange(-32768,32768);state.yaw_delta=rnd.randrange(-32768,32768);state.pad46=rnd.getrandbits(16);state.count=rnd.randrange(-2**40,2**40)
        p.pad_prev=[-1,0x100000001];p.ring[:]=rnd.randbytes(len(p.ring));p.sync_ring[:]=rnd.randbytes(len(p.sync_ring));p.msg_ring[:]=rnd.randbytes(len(p.msg_ring))
        image=p.pack();emit('peer_image',['peerimage,2,'+image.hex(),'pack,2','unpack,'+image.hex()],4)
    for size in [0,1,0x44df,0x44e1]:emit('peer_bad',['unpack,'+bytes(size).hex()])
    for start in [0,1,8190,8191,8192,-1,0xffffffff]:
        for size in [0,1,253,254,255,256,8193]:
            data=bytes(i&255 for i in range(size)).hex()
            emit('messages',[f'p,0,msg_write,{start}',f'm,msg_attached,{start}','m,lif,4',f'queue,257,{data}','attach','messages,0,2','attach'])
    for size in [0,1,2,3,4,255,256,257]:emit('message_count',[f'record,0,2,{bytes(12).hex()},8191,{size}','ring,0,8191,05026162060163','messages,0,2'])
    for frame in [0,1,254,255,256,257,0x7ffffffe,0x80000000]:
        for count in [1,2,31,32,41,127,255]:
            first=frame-count+1
            emit('build',[f'fill,0,{first},{count},11',f'p,1,send_frame,{first}',f'm,net_frame,{frame}',f'sync,0,{frame},123,4294967295',f'build,1,{first},{frame},48447,0,0',f'build,1,{first},{frame},48450,0,1'])
    for count in [0,256,-1]:emit('build_bad',[f'build,1,0,{count-1},48447,0,0'])
    emit('build_overflow',['fill,0,0,255,19','m,net_frame,2','record,0,0,'+bytes(12).hex()+',0,6000','build,1,0,254,48447,0,0'])
    for lif,acked,sf in [(4,1,0),(1,1,0),(258,1,0),(4,1,2),(4,3,0),(257,1,0)]:emit('send',[f'fill,0,0,256,31',f'm,lif,{lif}',f'p,1,acked,{acked}',f'p,1,send_frame,{sf}','send,1','send,1'])
    # Make valid packets using the independent Python builder, then send original
    # bytes into native parsing/receive. Include deterministic undefined zero fill
    # AND exact undefined-offset ownership in every builder result.
    packets=[]
    for config in [(model,bias,div) for model in r.Fpu.MODELS for bias in (0,1) for div in (0,1)]:
        for count in [1,2,7,31]:
            sender=r.Machine(2,1);fill(r,sender,1,0,count+1,23);sender.net_frame=2;sender.peers[1].set_sync(2,19,27)
            b=sender.build_packet(0,0,count,r.PKT_INPUT,1,sender.peers[0].send_state).data;packets.append(b)
            emit('receive',['stack,1','parse,'+b.hex(),'receive,0,'+b.hex(),'receive,0,'+b.hex()],config=config)
    b=packets[2];p=r.parse_packet(b)
    for cut in list(range(12))+list(range(p.sync_offset,len(b)+1)):
        emit('receive_truncated',['receive,0,'+b[:cut].hex()])
    for mode in [2,3]:emit('receive_callback',[f'stack,{mode}','receive,0,'+b.hex()])
    for setting in ['p,1,dropped,1','p,1,kick,1','p,1,oos,1','p,1,local,1','p,1,newest,999','p,1,newest,-100','m,num_peers,3']:
        emit('receive_ignore',[setting,'p,1,silence,19','receive,0,'+b.hex()])
    for last,count in [(0,0),(1,0),(-1,1),(2147483647,1),(-2147483648,1)]:
        data=r.header(r.PKT_RAW_RELAY,last,1,3,0,count,-128)+bytes(count*12)+r.kicked_notice()
        emit('raw_boundaries',['receive,1,'+data.hex(),'parse,'+data.hex()])
    raw=r.header(r.PKT_RAW_RELAY,3,1,3,0,4,127)+b''.join(r.rec12(i) for i in range(4))+b'\xff'*4
    for cut in range(len(raw)+1):emit('raw_truncated',['receive,1,'+raw[:cut].hex(),'parse,'+raw[:cut].hex()])
    for slot in range(16):
        bad=bytearray(raw);bad[8]=(3<<4)|slot;emit('raw_slot',['receive,1,'+bad.hex()])
    for relay in [0,1,3,4,255]:
        bad=bytearray(raw);bad[9]=relay;emit('raw_relay',['receive,1,'+bad.hex()])
    for tail in [b'\x00',b'\x00\x03a',b'\x00\x03abc',b'\x00\x00\x00\x01x',b'\x00\x01a\x00\x01b',b'\xff\x02ab']:
        emit('blocks',['p,1,msg_write,8191','dispatch,'+(raw+tail).hex(),'getrecord,1,3'])
    emit('sync_mismatch',['sync,0,2,19,99','receive,0,'+b.hex(),'queries'])
    for mode in range(4):emit('direct_callback',[f'hook,0,{mode}','senddirect,0,15,0001ff','dispatch,'+r.direct_message(0,b'abc').hex()],4)
    for slot in [-5,-4,-1,0,1,3,4]:emit('kick',[f'kick,{slot}','queries'],4)
    for requester in [0,1,3,4,0xffffffff]:
        for mask in [0,1,2,15,-1]:emit('kick_dispatch',['fill,1,0,44,31','p,1,newest,43','dispatch,'+r.kick_request(requester,mask).hex()],4)
    for data in [b'',b'x',b'abc',r.kicked_notice(),r.lan_packet(),r.lan_packet(True),bytes.fromhex('efbeadde'),r.kick_request(0,2)[:8]]:emit('dispatch',['dispatch,'+data.hex()])
    emit('owned_input',['p,2,local,1','sendmutable,15,abcdef'],4)
    emit('owned_input',['receive_mutate,'+b.hex()])
    for n in [5,64,65,1000]:
        for mask in [0,1,2,8,16,-1]:emit('wide_loop',[f'm,num_peers,{n}',f'senddirect,0,{mask},01'])
    for first in [1,2,17,255]:emit('catchup',[f'fill,0,0,256,97',f'build,1,{first},{first},48447,0,0','pack,1'])
    emit('attach_twice',['m,lif,4','queue,1,6162','attach','queue,2,6364','attach','messages,0,2','m,last_sent,4','queue,3,65','attach','m,lif,7','m,net_frame,6','attach','messages,0,6'])
    badbutton=r.header(r.PKT_INPUT,1,1,3,0,1,0)+bytes([255])*20
    emit('bad_button',['receive,0,'+badbutton.hex(),'parse,'+badbutton.hex()])
    for data in [bytes(16),r.header(r.PKT_INPUT,1,1,3,0,0,0)+bytes(4),r.header(123,1,1,3,0,1,0)+bytes(4)]:emit('parse_bad',['parse,'+data.hex()])
    # A malformed tail is evaluated after an earlier complete block was committed.
    emit('block_partial',['dispatch,'+(raw+b'\x00\x02ab\x01\x03x').hex(),'getrecord,1,3'])
    text='\n'.join(rows)+'\n';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(text,encoding='ascii',newline='\n')
    trace=''.join('\t'.join(row.split('\t')[:3]+row.split('\t')[3:])+'\n' for row in rows)
    fnv=14695981039346656037
    for byte in text.encode():fnv=((fnv^byte)*1099511628211)&((1<<64)-1)
    manifest={'schema_version':1,'reference_commit':PIN,'reference_path':'tools/ref_netwire.py','reference_sha256':SHA,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(text.encode()).hexdigest(),'vector_count':len(rows),'op_counts':counts,'operations':sorted(operations),'expected_trace_sha256':hashlib.sha256(trace.encode()).hexdigest(),'fnv1a64':f'{fnv:016x}','scope':'Authored packet/session state only; no sockets or retail bytes. Undefined output padding is zero-filled by the model and marked separately.'}
    out.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:manifest[k] for k in ('vector_count','expected_trace_sha256','fnv1a64')}))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--research-dir',type=Path,required=True);parser.add_argument('--output',type=Path,default=Path('native/tests/vectors/netwire_session.tsv'));args=parser.parse_args();generate(args.research_dir,args.output)
