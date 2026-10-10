#!/usr/bin/env python3
"""Generate authored tick/pad/launch vectors from an explicit pinned research checkout."""
import argparse
import collections
import hashlib
import json
import pathlib
import random
import struct
import subprocess
import types

COMMIT = '54258f9d7e281ef07f0675040e82e6264f9386cf'
PIN = '4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'
GLOBALS = 'network_mode num_peers lif last_sent net_frame cycle phase modem last_hash_frame stall ended eq min_ready win long_stall kicked kick_state msg_attached sim_frame rand_seed sync_acc level_pending num_players'.split()
PEER_FIELDS = 'port send_frame msg_write msg_read newest acked lead sim silence heard local dropped kick oos drop_frame kicked_notice'.split()

def sc(v):
    if v is None: return '_'
    if isinstance(v, bool): return str(int(v))
    if isinstance(v, (bytes, bytearray)): return 'b' + bytes(v).hex()
    if isinstance(v, str): return 's' + v.encode('ascii').hex()
    if isinstance(v, (list, tuple)): return '[' + ','.join(map(sc, v)) + ']'
    return str(v)

def sent(s): return [s.peer, s.ip, s.port, s.data, list(s.undefined)]
def state(m, events):
    return [[getattr(m, x) for x in GLOBALS],
            [[p.pack(), [getattr(p, x) for x in PEER_FIELDS], p.pad_prev] for p in m.peers],
            m.kick_acks, m.cam_yaw, m.input_lock, m.send_cycles, m.sim_enc.pack(), m.sim_dec.pack(),
            m.positions, m.pads, [m.pad_globals.start_block, m.pad_globals.mute, m.pad_globals.allow],
            [sent(s) for s in m.outbox], m.log, events,
            sorted(m.handlers), [bool(x) for x in m.direct_handlers], bool(m.run_tasks), bool(m.on_kill), bool(m.first_active_hook), bool(m.sample_garbage), bool(m.stack_bytes)]

def fpu(r, config):
    return r.Fpu(['ps2','ps2g1','chop','nearest'][config // 4], bool(config & 2), bool(config & 1))

def machine_case(r, config, scenario):
    m = r.Machine(4 if scenario in (3, 6, 10, 15, 16, 17) else 2, 0, fpu(r, config))
    events, results = [], []
    for i, p in enumerate(m.peers):
        p.ip, p.port = bytes([10, 0, 0, i+1]), 1000+i
        p.unknown8 = bytes([i+31])*8
        p.lead, p.sim, p.newest = 2, 1, 20
        for frame in range(1, 21):
            p.set_record(frame, r.rec12((frame*257+i*19)&65535,
                ((frame*13+i)&255, (frame*29+64)&255, (i*57+127)&255, (frame*3)&255),
                (frame&255, (i*63)&255), frame*313+i, frame&1), start=0, count=0)
        m.positions[i] = (0x3f800000+i*0x10000, 0xc0200000+i, 0x7f800000 if i==3 else 0)
        m.pads[i][:] = bytes([0xa0+i])*88
    def tasks(machine):
        events.append(['tasks', machine.net_frame, machine.num_peers])
        machine.rand_seed = (machine.rand_seed*1664525+1013904223)&0xffffffff
        machine.sync_acc = (machine.sync_acc+17)&0xffffffff
        machine.sim_frame += 1
        if scenario == 6:
            machine.level_pending = True
            machine.run_tasks = lambda again: events.append(['replacement_tasks', again.net_frame])
        if scenario == 12: raise ValueError('authored task exception')
    m.run_tasks = tasks
    m.on_kill = lambda i: (events.append(['kill',i]), setattr(m,'sync_acc',m.sync_acc+7))
    def first(i):
        events.append(['first',i]); m.queue_message(4, bytes([i, 0x55]))
    m.first_active_hook = first
    m.sample_garbage = lambda frame: (frame*0x1020304)&0xffffffff
    def handler(payload):
        events.append(['message',payload,m.net_frame,m.num_peers])
        m.sync_acc += len(payload)
        if scenario == 6:
            m.handlers[7] = lambda p: events.append(['replacement_message',p])
        if scenario == 15: m.num_peers = 1
    m.handlers[7] = handler
    def direct(payload):
        events.append(['direct',payload]); m.input_lock[0] = [2, 0xffff, 1]
        if scenario==22 and payload==b'append':
            inbox.clear();inbox.extend([r.lan_packet(),r.direct_message(0,b'second')])
    m.direct_handlers[0] = direct
    # Every live peer has two authored messages at frame1; later frames have none.
    for p in m.peers:
        msg=bytes([7,2,0x41,0x42,7,1,0x43]); p.msg_ring[:len(msg)]=msg
        p.set_record(1,p.wire_record(1),start=0,count=len(msg));p.msg_write=len(msg)
    m.msg_attached=7
    if scenario==1: m.modem=1;m.input_lock[0]=[3,0x1234,1]
    if scenario==2: m.peers[1].silence=901;m.peers[1].newest=0
    if scenario==3:
        m.peers[1].kick=2;m.peers[1].drop_frame=1;m.peers[2].kicked_notice=120
    if scenario==4:
        for p in m.peers[1:]:
            for frame in range(2,8):p.set_sync(frame,99,0xdeadbeef)
    if scenario==5:m.level_pending=True
    if scenario==7:m.lif=10;m.eq=30;m.win=61;m.min_ready=4;m.peers[1].lead=0
    if scenario==8:m.stall=301;m.long_stall=9
    if scenario==9:
        for p in m.peers:p.local=0
    if scenario==10:m.network_mode=0;m.pad_globals=r.PadGlobals(3,1,0x401)
    if scenario==11:m.peers[1].set_record(1,m.peers[1].wire_record(1),start=0,count=1)
    if scenario==14:m.peers[1].kicked_notice=10
    if scenario==16:
        m.kick_state=1;m.peers[1].kick=1;m.peers[1].drop_frame=-1
    if scenario==17:
        m.kick_state=1;m.peers[1].kick=1;m.peers[1].newest=1;m.kick_acks=[1]*4
    if scenario==18:m.stall=301;m.long_stall=9;m.ended=1
    if scenario==20:m.send_cycles=[-7,-3,-5,-9];m.modem=1
    if scenario==21:
        m.net_frame=0xffffffff;m.num_players=-1
        for p in m.peers:
            p.set_record(0xffffffff,r.rec12(1),start=0,count=0)
            p.set_record(0x100000000,r.rec12(2),start=0,count=0)
            p.set_sync(0,9,0x1234)
    if scenario==24:
        m.num_peers=5
        for p in m.peers:p.kicked_notice=10
    if scenario==23:m.num_peers=5
    if scenario in (4,5,6,11,12,15,21,23):
        try:m.tick(2 if scenario==21 else 3);results.append(None)
        except (ValueError, struct.error):results.append('!ValueError')
        except r.MalformedPacket:results.append('!MalformedPacket')
        except IndexError:results.append('!IndexError')
    else:
        for call in range(1 if scenario==24 else 6):
            sample=struct.pack('<H6B',0xf835^(call*17),0,255,127+call,40+call,33,220)
            if scenario==14:sample=b'1234567'
            inbox=[r.direct_message(0,b'lock')] if scenario==13 and call==0 else ([r.direct_message(0,b'append')] if scenario==22 and call==0 else [])
            try:
                out,held,steps=m.net_main(sample, yaw=-32768+call*511, camera_flag=call&1,
                    frame_step=(0 if call<3 else -1) if scenario==19 else (2 if scenario==1 else 1), no_sim=scenario in (8,18) or (scenario==1 and call==2),
                    hold_arg=scenario==1 and call==3, render_frame=call*30, inbox=inbox)
                results.append([[sent(x) for x in out],held,steps])
            except (ValueError, struct.error):results.append('!ValueError')
            except r.MalformedPacket:results.append('!MalformedPacket')
            except IndexError:results.append('!IndexError')
    return sc(results),sc(state(m,events))

def generate(research, output):
    source=subprocess.run(['git','-C',str(research),'show',COMMIT+':tools/ref_netwire.py'],check=True,capture_output=True).stdout
    if hashlib.sha256(source).hexdigest()!=PIN:raise RuntimeError('reference pin differs')
    r=types.ModuleType('pinned_netwire');exec(compile(source,'pinned_netwire.py','exec'),r.__dict__)
    rows=[]; counts=collections.Counter(); rng=random.Random(20973)
    def emit(op,args,result,state='_'):
        rows.append('\t'.join(['netwire-tick',op,args,result,state]));counts[op]+=1
    edge=[0,1,64,86,87,88,100,127,128,129,168,169,170,192,254,255]
    for config in range(16):
        f=fpu(r,config)
        for x in edge:
            for y in edge:emit('stick',f'{config},{x},{y}',sc(r.decode_stick(x,y,f)))
        for _ in range(128):
            x,y=rng.randrange(256),rng.randrange(256);emit('stick',f'{config},{x},{y}',sc(r.decode_stick(x,y,f)))
        for x,y in [(-2**63,2**63-1),(-2147483648,2147483647),(-1,-1),(4294967296,128),(128,-4294967296)]:
            emit('stick',f'{config},{x},{y}',sc(r.decode_stick(x,y,f)))
        for case in range(64):
            rec=r.rec12(rng.randrange(65536),[rng.randrange(256) for _ in range(4)],[rng.randrange(256) for _ in range(2)],rng.randrange(65536),case&1)
            pad=bytearray([case+64]*88);prev=[rng.randrange(1<<24),[0,19,20,21,23,24,0x7fffffff,0xffffffff][case%8]]
            if case%4==0:prev[0]=struct.unpack_from('<H',rec)[0]
            glob=r.PadGlobals([0,1,-1,3][case%4],case%2,[0,1,0xffffffff,0x800][case%4])
            args=','.join(map(str,[config,case,prev[0],prev[1],glob.start_block,glob.mute,glob.allow,32]))+','+rec.hex()
            for _ in range(32):r.pad_apply(pad,rec,prev,glob,f)
            emit('pad',args,sc(bytes(pad)),sc([prev,[glob.start_block,glob.mute,glob.allow]]))
        for scenario in range(25):
            a,b=machine_case(r,config,scenario);emit('machine',f'{config},{scenario}',a,b)
    rawedges=[0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x4effffff,0xcf000000,0x7fffffff,0xffffffff,0x7f800000,0xff800000]
    for i in range(512):
        positions=[None if (i+j)%5==0 else tuple(rng.choice(rawedges) if i<128 else rng.getrandbits(32) for _ in range(3)) for j in range(i%9)]
        seed,acc=rng.getrandbits(32),rng.getrandbits(32)
        args=f'{seed},{acc};'+';'.join('_' if p is None else ','.join(map(str,p)) for p in positions)
        emit('hash',args,sc(r.sync_hash(seed,acc,positions)))
    for length in [0,1,2,3,4,7,8,31,32,255,256,499,500,501,512,1300]:
        for i in range(16):
            payload=bytes(rng.randrange(256) for _ in range(length));id=rng.getrandbits(32);kind=i;arg=[-1,0,1,0x10000,0x12345678,0xffffffff,0x1ffffffff][i%7];size=[None,512,12,15,16,12+length,-1][i%7]
            args=','.join(map(str,[id,kind,arg,-999 if size is None else size]))+','+payload.hex()
            try:
                packet=r.launch_packet(id,kind,arg,payload,size);data,argument=r.launch_data_packet(id,arg,payload)
                result=[packet,r.launch_checksum(payload),data,argument,r.parse_launch_packet(packet),r.launch_data_ok(argument,payload),r.launch_data_ok(argument^0x10000,payload)]
                emit('launch',args,sc(result))
            except ValueError:emit('launch',args,'!ValueError')
    for length in range(40):
        data=bytes(rng.randrange(256) for _ in range(length));emit('parse',data.hex(),sc(r.parse_launch_packet(data)))
    body=('\n'.join(rows)+'\n').encode('ascii'); data=b'# netwire_tick_vectors_v1\n'+body;output.write_bytes(data)
    meta=dict(schema=1,reference='tools/ref_netwire.py',reference_commit=COMMIT,reference_sha256=PIN,generator_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),vector_sha256=hashlib.sha256(data).hexdigest(),rows=len(rows),op_counts=dict(counts),expected_trace_sha256=hashlib.sha256(body).hexdigest(),scope='Authored raw-pattern pad, launch, sync and stateful tick/main fixtures;16 explicit FPU settings. Full peer memory and mutable globals/callback events. No UDP, game simulation, original-client or Norrath compatibility claim.')
    output.with_suffix('.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--research-dir',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/netwire_tick.tsv'));a=p.parse_args();generate(a.research_dir,a.output)
