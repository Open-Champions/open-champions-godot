#!/usr/bin/env python3
"""Independent stateful raw32 animation decoder vectors from immutable RTA models.

Synthetic clip bytes and supplied state are authored. Clip parsing, allocation, decoding
and raw interpolation execute original pinned Python bodies.
"""
import argparse
import collections
import gzip
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import types
import copy
import struct

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'anim': {'commit': 'f516ac798bf28375554fe53ffb31b74802844136', 'path': 'tools/ref_anim.py', 'sha256': '6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'f2b996dded898f1b0b558b901cab875f775cfdc7', 'path': 'docs/spec/animation.md', 'sha256': 'be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab'}
COMPOSITION_POLICY = 'rta-animation-decoder-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')

def load_references(research_dir, scratch_dir):
    """Never import a live research working-tree file or follow its module path."""
    scratch_dir = pathlib.Path(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)
    modules = {}
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    old_bytecode = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        with tempfile.TemporaryDirectory(prefix='pinned-', dir=scratch_dir) as directory:
            for name in ('core', 'anim', 'specification'):
                pin = SPECIFICATION_EVIDENCE if name == 'specification' else REFERENCES[name]
                source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']],
                                        capture_output=True, check=True, timeout=30).stdout
                if hashlib.sha256(source).hexdigest() != pin['sha256']:
                    raise ValueError('immutable reference hash mismatch: ' + name)
                path = pathlib.Path(directory) / pathlib.Path(pin['path']).name
                path.write_bytes(source)
                if name == 'specification': continue
                module = types.ModuleType('pinned_animation_decoder_' + name)
                module.__file__ = str(path)
                sys.modules[module.__name__] = module
                exec(compile(source, str(path), 'exec'), module.__dict__)
                modules[name] = module
                if name == 'core': sys.modules['ref_core'] = module
    finally:
        sys.dont_write_bytecode = old_bytecode
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core', None)
        else: sys.modules['ref_core'] = previous
    return modules


LIMITS = [67108864, 134217728, 1024, 65536, 1024, 65536]
TRACK_FIELDS = ('clip', 'slot', 'weight', 'rate', 'life', 'life_rate', 'time', 'flags', 'sound_idx')
MODELED_OPERATIONS = ('Clip', 'DecoderPool.alloc', 'DecoderPool.touch', 'animtrack_decode',
                      'joint_pos', 'joint_quat', 'decode_clip', 'Track.copy_from', 'Clip.word')
INPUT_PROTOCOL = {
    'version': 1,
    'columns': ['operation', 'config', 'id;setup-and-command-DSL', 'status', 'observation'],
    'setup': '[system2,limits6,clips[[nameBytes,fileBytes]],tracks9,slots7,LRU,commands]',
    'system': '[sim_frame,cosmetic_seed]',
    'limits': '[input_bytes,owned_bytes,clips,tracks,slots,key_records]',
    'track': '[clipID,slotID,weight,rate,life,life_rate,time,flags,sound_idx]',
    'slot': '[index0,clipID,cursor,tick,ownerTrackID,yaw,joints64x16]',
    'joint': '[PT,RT,PX,PY,PZ,VX,VY,VZ,QX,QY,QZ,QW,RX,RY,RZ,RW]',
    'clip': '[id,nameBytes,dataBytes,joint_count,length,bind,depth,events4,sound_times_orNone,sound_group,joint_mask,words]',
    'snapshot': '[system2,limits6,clips12,tracks[[id,track9]],slots7,LRU]',
    'commands': '0 add_clip(nameBytes,fileBytes);1 alloc;2 touch(slot);3 decode(track);4 pos(slot,joint,time);5 quat(slot,joint,time);6 decode_clip(clip,time,flags);7 set_track(track,field,value);8 copy_track(dst,src);9 set_event(clip,index,seconds,mask);10 word(clip,index)',
    'observation': '[initial_snapshot,submitted_commands,[[result_or_error_name,complete_snapshot],...]]',
    'identity': 'Clip/track/slot IDs are 1-based, 0=null; slot.index is 0-based. decode_clip retains a newly constructed track even on failure.',
}


def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned decoder state')


def track(clip=1, time=0, flags=0, slot=0):
    return [clip, slot, 0x3f800000, 0xbf000000, 0x3f000001, 0x80000000,
            time, flags, -123]


def slot(index, patterned=False):
    joints = [[0] * 16 for _ in range(64)]
    if patterned:
        for j, row in enumerate(joints):
            row[:] = [j-32, 31-j] + [0x3e800000 + j*4096 + k*64 for k in range(14)]
    return [index, 0, 0, 0, 0, -12345, joints]


def setup(data, tracks=None, count=2, patterned=False):
    return [[0xffffffff, 0xdeadbeef], list(LIMITS), [[list(b'authored'), list(data)]],
            tracks if tracks is not None else [track()],
            [slot(i, patterned) for i in range(count)], list(range(count, 0, -1)), []]


def fixtures(modules):
    """All bytes and supplied states are synthetic; no expected values enter inputs."""
    A,C = modules['anim'],modules['core']
    C.set_fpu('ps2', False, False)
    def encoded(n=1, keys=(), width=None, **kwargs):
        joints=[((64+j,-64-j,32+j),(4096,0,0,0)) for j in range(n)]
        return A.encode_clip(joints,100,keys=keys,width=width,**kwargs)
    rows=[]
    def add(name, arithmetic, state):
        rows.append((name, arithmetic, state))
    for width in range(1,17):
        low=-(1 << (width-1));high=(1 << (width-1))-1
        data=A.encode_clip([((low,high,0),(high,low,0,high)),
                            ((high,low,high),(low,0,high,low))],20,
                           keys=[(1,1,'p',(low,high,low)),(1,0,'r',(high,low,high,low))],width=width)
        s=setup(data);s[6]=[[3,1],[7,1,6,0x3fc00000],[3,1],[4,2,1,0x3fc00000],[5,2,0,0x3fc00000]]
        add('field_width',True,s)
    data=encoded(keys=[(1,0,'p',(256,-128,17)),(1,0,'r',(128,31,-47,99)),
                         (3,0,'p',(-77,123,256)),(3,0,'r',(-256,257,0,-333))])
    times=[0,0x80000000,0xbf666666,0xbf800000,0x3f7fffff,0x3f800000,
           0x3f800001,0x3fc00000,0x40400000,0x40f00001]
    for raw in times:
        s=setup(data);s[6]=[[7,1,6,raw],[3,1],[4,2,0,raw],[5,2,0,raw],[3,1]]
        add('sample_time',True,s)
    s=setup(data)
    for raw in (0x40a00000,0x3f000001,0x40400000,0x40400000):
        s[6] += [[7,1,6,raw],[3,1],[4,2,0,raw],[5,2,0,raw]]
    add('seek_restart',True,s)
    # Identity and shared mutable event metadata; equal data never deduplicates.
    s=setup(data,tracks=[track(),track(2)]);s[2].append([list(b'equal'),list(data)])
    s[6]=[[9,1,2,0x3f000001,255],[3,1],[3,2],[8,1,2],[7,2,7,65568],[1]]
    add('clip_identity',False,s)
    # Copy does not fix owner pointer; allocation consults the live/stale cell flags.
    s=setup(data,tracks=[track(),track(),track()],count=3,patterned=True)
    s[6]=[[3,1],[3,2],[8,3,1],[7,1,7,A.ROOT],[1],[2,3],[7,1,7,0],[1]]
    add('stale_owner_copy',False,s)
    for protected in (A.ROOT,A.YAW,A.ROOT|A.YAW,65536|A.ROOT):
        s=setup(data,tracks=[track(flags=protected),track(flags=protected)])
        s[6]=[[3,1],[3,2],[1],[7,1,7,0],[1]]
        add('protected_pool',False,s)
    s=setup(data,patterned=True);s[4][1][1:6]=[1,37,444,1,12345];s[3][0][1]=2
    s[6]=[[1],[3,1]];add('allocation_preserves',False,s)
    for flags in (0,A.ROOT,A.YAW,A.ROOT|A.YAW,0xffff0000|A.ROOT):
        s=setup(data,tracks=[track(flags=flags,slot=2)],patterned=True)
        s[4][1][1:6]=[1,0,1000000,1,-12345]
        s[6]=[[3,1],[4,2,0,0x3f000001],[5,2,0,0x3f000001]]
        add('root_baseline',True,s)
    # Stream is selected from SLOT unless target triggers restart.
    other=A.encode_clip([((640,1280,-64),(2048,0,0,0))],100)
    s=setup(data);s[2].append([list(b'other'),list(other)])
    s[6]=[[3,1],[7,1,0,2],[3,1],[4,2,0,0],[7,1,6,0xbf800000],[3,1],[4,2,0,0]]
    add('slot_clip_mismatch',True,s)
    # Read 64 stale records after smaller clips; stream keys may address beyond n.
    large=encoded(n=64,keys=[(1,63,'p',(256,128,-256)),(1,63,'r',(512,0,0,256))],joint_mask=0xffffffffffffffff)
    small=encoded(keys=[(1,63,'p',(-64,32,16))])
    s=setup(large,tracks=[track(time=0x3f800000),track(2)],patterned=True)
    s[2].append([[],list(small)])
    s[6]=[[3,1],[2,1],[3,2],[7,2,6,0x3f800000],[3,2],[4,2,63,0x40000000],[5,2,63,0x40000000]]
    add('stale_64_joints',True,s)
    # A fresh track at large time can skip the sentinel-triggered restart.
    for stale in (False,True):
        s=setup(data,tracks=[track(time=C.from_int(1000000))],patterned=True)
        if stale:
            # A real finite future key prevents runaway past-stream reads.
            s[2][0][1]=list(encoded(keys=[]))
            s[4][1][1:4]=[1,0,0]
        s[6]=[[3,1]];add('million_no_restart',False,s)
    # Closing 255 is decoded as another key when reached, then EOF faults.
    s=setup(encoded());s[6]=[[6,1,C.from_int(255),0]];add('closing_delta',False,s)
    # Explicit temporary ownership survives both successful and failed calls.
    s=setup(data,tracks=[]);s[6]=[[6,1,0,0],[6,1,0x3fc00000,A.ROOT],[6,1,0,A.YAW],[6,1,0,0]]
    add('convenience_owner',True,s)
    # Parser and stream extent checks, including odd final word padding.
    baseline=encoded(n=2,depth=[-128,127],bind=[(-32768,0,32767),(1,-1,2)],joint_mask=0x8000000000000001,
                     sound_times=[-7,0,99],sound_group=255)
    for variant in range(15):
        value=bytearray(baseline)
        if variant==0:value=value[:0x6f]
        elif variant==1:struct.pack_into('<i',value,0,-1)
        elif variant==2:struct.pack_into('<i',value,0,65)
        elif variant==3:struct.pack_into('<I',value,8,len(value)+1)
        elif variant==4:struct.pack_into('<I',value,12,len(value)-1)
        elif variant==5:struct.pack_into('<I',value,16,len(value))
        elif variant==6:struct.pack_into('<I',value,0x38,len(value)-3)
        elif variant==7:value=value[:-28]
        elif variant==8:value+=b'\xab'
        elif variant==9:struct.pack_into('<i',value,4,-2147483648)
        elif variant==10:struct.pack_into('<I',value,0x34,0xffffffff)
        elif variant==11:struct.pack_into('<I',value,8,len(value))
        elif variant==12:struct.pack_into('<I',value,8,1)
        elif variant==13:struct.pack_into('<II',value,0x14,0x80000000,0xffffffff)
        else:struct.pack_into('<i',value,0,0)
        s=setup(data);s[6]=[[0,[0,255,65],list(value)]];add('clip_parse',False,s)
    # Every prefix of the initialization stream preserves actual partial writes.
    full=encoded(n=2,width=16);key=struct.unpack_from('<I',full,8)[0]
    for size in (0,1,2,3,6,8,10,14,16,20,26,29):
        cut=full[:key+size];s=setup(cut,patterned=True);s[6]=[[3,1]]
        add('initialization_eof',False,s)
    keyfull=encoded(keys=[(1,0,'p',(32767,-32768,1)),(1,0,'r',(32767,0,-1,-32768))],width=16)
    keyoff=struct.unpack_from('<I',keyfull,8)[0]
    for size in (16,18,20,22,24,26,28,30,32):
        s=setup(keyfull[:keyoff+size],tracks=[track(time=0x3f800000)],patterned=True);s[6]=[[3,1]]
        add('record_eof',False,s)
    for raw in (0,0x80000000,0x3f000001,0xbf000001,0x00000001,0x7f800000,0xff800000,0x7fffffff):
        s=setup(data,patterned=True)
        s[6]=[[4,1,j,raw] for j in (0,63,-1,-64,64,-65)]+[[5,1,j,raw] for j in (0,63,-1,-64,64,-65)]
        add('joint_raw',True,s)
    s=setup(data,count=165);s[6]=[[1],[2,1],[1]];add('full_pool',False,s)
    # Signed16 stored key ticks are distinct from the unwrapped slot tick.
    keys=[(i*255,0,'p',(1,-1,1)) for i in range(1,130)]
    s=setup(encoded(keys=keys),tracks=[track(time=C.from_int(32895))]);s[6]=[[3,1],[4,2,0,C.from_int(32896)]]
    add('tick_wrap',True,s)
    # Empty clips preserve every joint; restart still derives yaw from stale joint0.
    empty=A.encode_clip([],0)
    s=setup(empty,patterned=True);s[6]=[[3,1],[4,2,0,0],[5,2,0,0],[6,1,0,A.ROOT]]
    add('zero_joints',True,s)
    s=setup(data,count=3);s[6]=[[2,1],[2,1],[2,3],[2,2],[1]]
    add('lru_touch',False,s)
    s=setup(data);s[6]=[[10,1,i] for i in (-2,-1,0,1,2,3,999999)]
    add('clip_word',False,s)
    return rows


class Oracle:
    def __init__(self, modules, supplied):
        self.A=modules['anim'];self.C=modules['core'];A=self.A
        system, self.limits, clips, tracks, slots, order, _commands=supplied
        self.system=A.AnimSystem(decoder_slots=len(slots),pose_buffers=2)
        self.system.sim_frame,self.system.cosmetic_seed=system
        self.clips=[A.Clip(bytes(data),bytes(name).decode('latin1')) for name,data in clips]
        self.tracks=[A.Track() for _ in tracks]
        for dst,fields in zip(self.tracks,tracks):
            for i,value in enumerate(fields):self.set_track_value(dst,i,value)
        for dst,fields in zip(self.system.decoders.slots,slots):
            index,clip,cursor,tick,owner,yaw,joints=fields
            dst.index=index;dst.clip=self.clips[clip-1] if clip else None
            dst.cursor=cursor;dst.tick=tick;dst.owner=self.tracks[owner-1] if owner else None
            dst.yaw=yaw;dst.joints=copy.deepcopy(joints)
        self.system.decoders.order=[self.system.decoders.slots[i-1] for i in order]

    def set_track_value(self,target,field,value):
        if field==0:value=self.clips[value-1] if value else None
        if field==1:value=self.system.decoders.slots[value-1] if value else None
        setattr(target,TRACK_FIELDS[field],value)

    @staticmethod
    def identity(value, registry):
        if value is None:return 0
        return next(i+1 for i,item in enumerate(registry) if item is value)

    def snapshot(self):
        A=self.A;slots=self.system.decoders.slots
        clips=[[i+1,list(c.name.encode('latin1')),list(c.data),c.joint_count,c.length,
                c.bind,c.depth,c.events,c.sound_times,c.sound_group,c.joint_mask,c.words]
               for i,c in enumerate(self.clips)]
        tracks=[[i+1,[self.identity(t.clip,self.clips),self.identity(t.slot,slots),
                       t.weight,t.rate,t.life,t.life_rate,t.time,t.flags,t.sound_idx]]
                for i,t in enumerate(self.tracks)]
        pool=[[s.index,self.identity(s.clip,self.clips),s.cursor,s.tick,
               self.identity(s.owner,self.tracks),s.yaw,s.joints] for s in slots]
        return copy.deepcopy([[self.system.sim_frame,self.system.cosmetic_seed],self.limits,
                              clips,tracks,pool,[self.identity(s,slots) for s in self.system.decoders.order]])

    def execute(self, command):
        A=self.A;op,*args=command;pool=self.system.decoders
        if op==0:
            name,data=args;clip=A.Clip(bytes(data),bytes(name).decode('latin1'))
            self.clips.append(clip);return len(self.clips)
        if op==1:return self.identity(pool.alloc(),pool.slots)
        if op==2:pool.touch(pool.slots[args[0]-1]);return None
        if op==3:return self.identity(A.animtrack_decode(self.system,self.tracks[args[0]-1]),pool.slots)
        if op in (4,5):
            sid,joint,time=args
            return (A.joint_pos if op==4 else A.joint_quat)(pool.slots[sid-1],joint,time)
        if op==6:
            cid,time,flags=args
            original=A.Track
            def retain_track():
                t=original();self.tracks.append(t);return t
            A.Track=retain_track
            try:return A.decode_clip(self.clips[cid-1],time,flags,self.system)
            finally:A.Track=original
        if op==7:
            tid,field,value=args;self.set_track_value(self.tracks[tid-1],field,value);return None
        if op==8:
            dst,src=args;self.tracks[dst-1].copy_from(self.tracks[src-1]);return None
        if op==9:
            cid,index,seconds,mask=args;self.clips[cid-1].events[index]=[seconds,mask];return None
        if op==10:return self.clips[args[0]-1].word(args[1])
        raise ValueError('unknown authored DSL operation')


def run_case(modules, config, supplied):
    C=modules['core'];C.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
    oracle=Oracle(modules,supplied);initial=oracle.snapshot();steps=[]
    for command in supplied[6]:
        try:result=oracle.execute(command)
        except (modules['anim'].HostFault,ValueError,IndexError) as error:result=type(error).__name__
        steps.append([result,oracle.snapshot()])
    return [initial,copy.deepcopy(supplied[6]),steps]


def cases(modules):
    for name,arithmetic,supplied in fixtures(modules):
        for config in (range(16) if arithmetic else (0,)):
            yield name,config,supplied


def independent_anchors(modules):
    A,C=modules['anim'],modules['core'];C.set_fpu('ps2',False,False)
    data=A.encode_clip([((64,-64,32),(4096,0,0,0))],10,keys=[(1,0,'p',(256,0,-256))])
    s=setup(data);s[6]=[[6,1,0x3fc00000,0]]
    v=run_case(modules,0,s)
    assert sc(v[2][0][0])==sc([[[0x3fc00000,0xbf800000,0],[0,0,0,0x3f800000]]])
    assert len(v[2][0][1][3])==2 and v[2][0][1][4][1][4]==2
    s=setup(data,tracks=[track(flags=A.ROOT),track(flags=A.YAW)])
    s[6]=[[3,1],[3,2],[1]];v=run_case(modules,0,s)
    assert v[2][-1][0]=='HostFault' and v[2][-1][1][5]==[1,2]
    s=setup(data,tracks=[track(time=C.from_int(1000000))]);s[6]=[[3,1]]
    v=run_case(modules,0,s);assert v[2][0][0]=='HostFault'
    assert v[2][0][1][4][1][3:5]==[1000000,1]
    s=setup(data,patterned=True);s[6]=[[1]];v=run_case(modules,0,s)
    before=v[0][4][1];after=v[2][0][1][4][1]
    assert after[3]==1000000 and after[5]==before[5]
    assert after[6][0][2:4]==[0,0] and after[6][63]==before[6][63]


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'animation-decoder-generator'
    modules = load_references(research_dir, scratch_dir)
    independent_anchors(modules)
    output = pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    manifest_path=output.with_name(output.name[:-7]+'.json')
    fd,name=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent)
    temp=pathlib.Path(name);manifest_temp=None
    vh,th=hashlib.sha256(),hashlib.sha256()
    fnv=14695981039346656037;counts=collections.Counter();number=size=longest=0
    try:
        with os.fdopen(fd,'wb') as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
                header=b'# animation_decoder_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for scenario,config,supplied in cases(modules):
                    first=sc(run_case(modules,config,supplied))
                    if first!=sc(run_case(modules,config,supplied)):
                        raise AssertionError('fresh reference executions diverged')
                    row=('\t'.join([scenario,str(config),str(number)+';'+sc(supplied),'ok',first])+'\n').encode('ascii')
                    stream.write(row);vh.update(row);th.update(row)
                    for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[scenario]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'animation-decoder','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'helpers':{},
          'input_protocol':INPUT_PROTOCOL,
          'modeled_operations':list(MODELED_OPERATIONS),
          'composition_contract':{'scope':'Standalone original Clip, DecoderPool, animtrack_decode, joint_pos/joint_quat and decode_clip. No clock advancement, root/yaw extraction, pose, sound, UI or Session integration.',
          'state':'Complete owned clip metadata/bytes/words, stable track IDs, every64x16 joint slot record, slot owner/clip/cursor/tick/yaw and LRU. Unused AnimSystem pose/VU/sound fields remain default and outside this API.',
          'identity':'Original decode_clip Track construction is instrumented only to retain its stable owner identity on success or failure; no original numerical or branch body is replaced.',
          'errors':'Finite original ValueError/HostFault/IndexError retain completed writes and list rotations; native-only resource caps/foreign identities are tested separately.',
          'arithmetic':'All raw interpolation/decode arithmetic executes pinned ref_core Fpu. No host binary64 sampling, quaternion normalization, time clamping or stream-sentinel invention.',
          'publication':'Sibling temporary files flushed and fsynced, fixture replaced then manifest last. Interrupted pair publication is detectable by hashes, not a two-file transaction.'}}
        fd2,name=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent);manifest_temp=pathlib.Path(name)
        with os.fdopen(fd2,'w',encoding='ascii',newline='\n') as stream:
            json.dump(manifest,stream,indent=2,sort_keys=True);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.replace(temp,output);os.replace(manifest_temp,manifest_path)
        return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/animation_decoder.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
