#!/usr/bin/env python3
"""Independent canonical-cell animation lifecycle vectors from immutable RTA models.

Synthetic inputs are authored. Lifecycle, clock and decoder bodies execute one
original AnimSystem with original fixed Track identities; no native expectations.
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
COMPOSITION_POLICY = 'rta-animation-runtime-v1'
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
                module = types.ModuleType('pinned_animation_runtime_' + name)
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



# Input/schema and finite cases are finalized against the runtime interface first.
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


CLOCK_OPERATIONS = (
    'anim_clear', 'anim_push', 'anim_push_with_weights', 'anim_push_chained',
    'anim_start', 'anim_start_blend_in_out', 'anim_stop', 'anim_drop_non_persistent',
    'anim_release_caches', 'anim_state_dtor', 'anim_sync_to_sim_frame',
    'anim_randomize_top_time', 'anim_sync_top_to_below', 'anim_jump_top_to_end',
    'anim_snap_top_to_end', 'anim_find_track', 'anim_find_track_by_flag',
    'anim_fade_in_track', 'anim_fade_out_track', 'anim_set_track_weight',
    'anim_state_copy', 'anim_advance')
TRACK_FIELDS = ('clip','slot','weight','rate','life','life_rate','time','flags','sound_idx')


def delta(before, after, path=()):
    if before == after:return []
    if isinstance(before,list) and isinstance(after,list) and len(before)==len(after):
        out=[]
        for index,(old,new) in enumerate(zip(before,after)):
            out.extend(delta(old,new,path+(index,)))
        return out
    return [[list(path),copy.deepcopy(after)]]


def apply_delta(before, changes):
    result=copy.deepcopy(before)
    for path,value in changes:
        if not path:result=copy.deepcopy(value);continue
        target=result
        for index in path[:-1]:target=target[index]
        target[path[-1]]=copy.deepcopy(value)
    return result

DECODER_LIMITS=[67108864,134217728,1024,65536,1024,65536]
LIMITS=[DECODER_LIMITS,1024,65536,65536]
MODELED_OPERATIONS=CLOCK_OPERATIONS+('DecoderPool.alloc','DecoderPool.touch','animtrack_decode','joint_pos','joint_quat','decode_clip','Track.copy_from','Clip.word')
INPUT_PROTOCOL={
 'version':1,
 'setup':'[system2,limits4,clips[[nameBytes,fileBytes]],standaloneTracks9,states[[pose0,count,capacity,rawflags,capacityTrackRows9]],slots7,LRU,commands]',
 'limits':'[decoder6,states,state_cells,max_loop_subtractions]',
 'track':'[clipID,slotID,weight,rate,life,life_rate,time,raw32flags,sound_idx]',
 'slot':'[index0,clipID,cursor,tick,ownerTrackID,yaw,joints64x16]',
 'snapshot':'[limits4,states[[id,[pose0,count,capacity,rawflags,orderedTrackIDs]]],decoderSnapshot[system2,limits6,clips12,tracks[[id,track9]],slots7,LRU]]',
 'observation':'[input,initial_snapshot,[[result_or_original_error_name,recursive_list_delta],...]]',
 'delta':'[indexPath,replacement] at unequal scalar/type/list length; equal-length lists recursively visited in ascending index order; integer arrays represent bytes.',
 'clock_commands':'0 clear(s);1 push(s,c,f);2 weighted_push(s,c,f,w,r,l,lr);3 chained(s,c,f);4 start(s,c,f,blend);5 start_blend(s,c,f,in,out);6 stop(s,c,blend);7 drop(s);8 release(s);9 dtor(s);10 sync_frame(s);11 randomize(s);12 sync_below(s);13 jump(s);14 snap(s);15 find(s,c);16 find_flag(s,mask);17 fade_in(s,c,blend);18 fade_out(s,c,blend);19 weight(s,c,value);20 copy(s);21 advance(s,speed)',
 'other_commands':'22 add_state(capacity);23 add_clip(nameBytes,fileBytes);24 add_track(track9);25 set_track(id,field,value);26 set_state(id,pose0,count,flags);27 event(clip,index,seconds,mask);28 system(field0frame_or1cosmetic,value);29 alloc;30 touch(slot);31 decode(track);32 pos(slot,joint,time);33 quat(slot,joint,time);34 decode_clip(clip,time,flags);35 copy_track(dst,src);36 word(clip,index);37 set_word(clip,index,u16)',
 'identity':'Clips then standalone tracks then states/capacity cells allocated in input order. One canonical Track registry, IDs1based/null0; slot.index0based. Copy adds a new state and fresh cells, dtor retains IDs, decode_clip retains its temporary even on failure.',
 'results':'push/start/find None becomes0 ID; other voidNone; copy/add_state/newclip/newtrack/alloc/decode returnID; advance signedbyte; finite original errors classnames preserve prefix and continue. Native host-policy refusals are separate.',
 'domain':'NonROOT/nonYAW across ALL cells,pose0,sound/root/yaw outputs disabled; full raw32 flags otherwise retained. No mirrors, repaired owners or native expectation input.'}


class Oracle:
    def __init__(self,modules,supplied):
        self.A=modules['anim'];self.C=modules['core'];A=self.A
        system,self.limits,clips,standalone,states,slots,order,_=supplied
        self.system=A.AnimSystem(decoder_slots=len(slots),pose_buffers=2)
        self.system.sim_frame,self.system.cosmetic_seed=system
        self.clips=[A.Clip(bytes(data),bytes(name).decode('latin1'))for name,data in clips]
        self.tracks=[];self.states=[]
        for row in standalone:self.add_track(row)
        for pose,count,capacity,flags,rows in states:
            assert pose==0 and 0<=count<=capacity and len(rows)==capacity
            s=A.AnimState(capacity);s.count=count;s.flags=flags
            self.states.append(s);self.tracks.extend(s.tracks)
            for t,row in zip(s.tracks,rows):self.assign(t,row)
        for target,row in zip(self.system.decoders.slots,slots):
            index,clip,cursor,tick,owner,yaw,joints=row
            target.index=index;target.clip=self.clip(clip);target.cursor=cursor;target.tick=tick
            target.owner=self.tracks[owner-1]if owner else None;target.yaw=yaw;target.joints=copy.deepcopy(joints)
        self.system.decoders.order=[self.system.decoders.slots[i-1]for i in order]

    def clip(self,cid):return self.clips[cid-1]if cid else None
    @staticmethod
    def identity(value,registry):
        if value is None:return 0
        return next(i+1 for i,item in enumerate(registry)if item is value)
    def field(self,t,index,value):
        if index==0:value=self.clip(value)
        if index==1:value=self.system.decoders.slots[value-1]if value else None
        setattr(t,TRACK_FIELDS[index],value)
    def assign(self,t,row):
        for i,value in enumerate(row):self.field(t,i,value)
    def add_track(self,row):
        t=self.A.Track();self.tracks.append(t);self.assign(t,row);return len(self.tracks)
    def add_state(self,capacity):
        s=self.A.AnimState(capacity);self.states.append(s);self.tracks.extend(s.tracks);return len(self.states)
    def snapshot(self):
        slots=self.system.decoders.slots;ident=self.identity
        clips=[[i+1,list(c.name.encode('latin1')),list(c.data),c.joint_count,c.length,c.bind,c.depth,c.events,c.sound_times,c.sound_group,c.joint_mask,c.words]for i,c in enumerate(self.clips)]
        tracks=[[i+1,[ident(t.clip,self.clips),ident(t.slot,slots),t.weight,t.rate,t.life,t.life_rate,t.time,t.flags,t.sound_idx]]for i,t in enumerate(self.tracks)]
        states=[[i+1,[0,s.count,s.capacity,s.flags,[ident(t,self.tracks)for t in s.tracks]]]for i,s in enumerate(self.states)]
        pool=[[s.index,ident(s.clip,self.clips),s.cursor,s.tick,ident(s.owner,self.tracks),s.yaw,s.joints]for s in slots]
        decoder=[[self.system.sim_frame,self.system.cosmetic_seed],self.limits[0],clips,tracks,pool,[ident(s,slots)for s in self.system.decoders.order]]
        def owned(value):
            return [owned(x)for x in value]if isinstance(value,(list,tuple))else value
        return owned([self.limits,states,decoder])
    def execute(self,command):
        A=self.A;op,*a=command;pool=self.system.decoders
        if op<=21:
            s=self.states[a[0]-1]
            if op in (0,7,8,9,14):result=getattr(A,CLOCK_OPERATIONS[op])(s)
            elif op in (1,3):result=getattr(A,CLOCK_OPERATIONS[op])(s,self.clip(a[1]),a[2])
            elif op==2:result=A.anim_push_with_weights(s,self.clip(a[1]),*a[2:])
            elif op==4:result=A.anim_start(a[3],s,self.clip(a[1]),a[2])
            elif op==5:result=A.anim_start_blend_in_out(a[3],a[4],s,self.clip(a[1]),a[2])
            elif op in (6,17,18,19):result=getattr(A,CLOCK_OPERATIONS[op])(a[2],s,self.clip(a[1]))
            elif op in (10,11,12,13):result=getattr(A,CLOCK_OPERATIONS[op])(self.system,s)
            elif op==15:result=A.anim_find_track(s,self.clip(a[1]))
            elif op==16:result=A.anim_find_track_by_flag(s,a[1])
            elif op==20:
                s=A.anim_state_copy(self.system,s);self.states.append(s);self.tracks.extend(s.tracks);return len(self.states)
            elif op==21:return A.anim_advance(self.system,a[1],s,False,False,False)[0]
            else:raise AssertionError(op)
            if op in (1,2,3,4,5,15):return self.identity(result,self.tracks)
            return result
        if op==22:return self.add_state(a[0])
        if op==23:
            c=A.Clip(bytes(a[1]),bytes(a[0]).decode('latin1'));self.clips.append(c);return len(self.clips)
        if op==24:return self.add_track(a[0])
        if op==25:self.field(self.tracks[a[0]-1],a[1],a[2]);return None
        if op==26:
            sid,pose,count,flags=a;assert pose==0;s=self.states[sid-1];assert 0<=count<=s.capacity;s.count=count;s.flags=flags;return None
        if op==27:self.clip(a[0]).events[a[1]]=[a[2],a[3]];return None
        if op==28:setattr(self.system,('sim_frame','cosmetic_seed')[a[0]],a[1]);return None
        if op==29:return self.identity(pool.alloc(),pool.slots)
        if op==30:pool.touch(pool.slots[a[0]-1]);return None
        if op==31:return self.identity(A.animtrack_decode(self.system,self.tracks[a[0]-1]),pool.slots)
        if op in (32,33):return (A.joint_pos if op==32 else A.joint_quat)(pool.slots[a[0]-1],a[1],a[2])
        if op==34:
            original=A.Track
            def retained():
                t=original();self.tracks.append(t);return t
            A.Track=retained
            try:return A.decode_clip(self.clip(a[0]),a[1],a[2],self.system)
            finally:A.Track=original
        if op==35:self.tracks[a[0]-1].copy_from(self.tracks[a[1]-1]);return None
        if op==36:return self.clip(a[0]).word(a[1])
        if op==37:
            clip=self.clip(a[0]);words=list(clip.words);words[a[1]]=a[2];clip.words=tuple(words);return None
        raise ValueError('unknown authored input command')


def run_case(modules,config,supplied):
    modules['core'].set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
    oracle=Oracle(modules,supplied);initial=oracle.snapshot();previous=initial;steps=[]
    for command in supplied[7]:
        try:result=oracle.execute(command)
        except (modules['anim'].HostFault,ValueError,IndexError)as error:result=type(error).__name__
        current=oracle.snapshot();changes=delta(previous,current)
        assert apply_delta(previous,changes)==current
        steps.append([result,changes]);previous=current
    return [copy.deepcopy(supplied),initial,steps]


def fixtures(modules):
    A,C=modules['anim'],modules['core'];C.set_fpu('ps2',False,False)
    data=A.encode_clip([((64,-64,32),(4096,0,0,0)),((-32,48,-16),(2048,512,0,1024))],6,
        keys=[(1,0,'p',(256,-128,17)),(1,1,'r',(128,31,-47,99)),(3,1,'p',(-77,123,256))])
    other=A.encode_clip([((128,64,-32),(4096,0,0,0))],8,keys=[(2,0,'p',(31,17,-9))])
    def cell(cid=1,flags=0,time=0):return [cid,0,0x3f800000,0,0x3f800000,0,time,flags,-123]
    def base(count=2,capacity=3,pool=3):
        rows=[cell(1 if i!=1 else 2)for i in range(capacity)]
        return [[0xffffffff,0xdeadbeef],copy.deepcopy(LIMITS),
            [[list(b'A'),list(data)],[list(b'equal-A'),list(data)],[list(b'B'),list(other)]],[],
            [[0,count,capacity,0x10020,rows]], [slot(i,True)for i in range(pool)],list(range(pool,0,-1)),[]]
    out=[]
    def add(name,arithmetic,s,commands):s[7]=commands;out.append((name,arithmetic,s))
    s=base();add('clear_reuse',False,s,[[31,1],[31,2],[0,1],[1,1,3,0],[31,1],[29]])
    s=base();s[4][0][4][2][1]=1;s[5][0][1:6]=[1,0,1000000,3,-12345]
    add('release_dtor_stale',False,s,[[31,1],[31,2],[26,1,0,1,0x10020],[8,1],[9,1],[9,1],[1,1,1,0],[31,2]])
    s=base(3);add('stop_all_caches',False,s,[[31,1],[31,2],[31,3],[6,1,1,0],[31,1],[29]])
    s=base(3);s[4][0][4][2][7]=A.PERSISTENT
    add('compact_owner',False,s,[[31,1],[31,2],[31,3],[7,1],[31,1],[29],[30,1]])
    s=base();s[4][0][4][0][4]=0
    add('advance_remove',True,s,[[31,1],[31,2],[21,1,0x3f000001],[31,1],[29]])
    s=base(3);s[4][0][4][1][7]=A.PERSISTENT;s[4][0][4][2][7]=A.PURGE_BELOW
    add('purge_owner',True,s,[[31,1],[31,2],[31,3],[21,1,0x3f800000],[31,1],[31,2]])
    s=base();s[4][0][4][2][1]=1;s[5][0][1:6]=[1,0,1000000,3,-12345]
    add('state_copy_alias',True,s,[[31,1],[31,2],[20,1],[27,1,0,0,128],[27,2,0,0,1],[21,2,0x3f800000],[31,4],[31,1],[8,2],[9,1]])
    s=base(1,1);add('full_start',True,s,[[31,1],[4,1,3,0,0x3f000001],[31,1],[5,1,1,A.REPORT_END,0x3c888889,0x3f000001],[31,1],[21,1,0x3f800000]])
    s=base(1);s[4][0][4][0][6]=0x40000000
    add('chained_wait',True,s,[[31,1],[3,1,3,A.FIXED_STEP],[31,2],[21,1,0x3f000001],[21,1,0x40000000],[21,1,0x40000000],[31,2]])
    for raw in (0x3f000001,0xbf000001):
        s=base();add('clock_seek',True,s,[[31,1],[28,0,123456789],[10,1],[31,1],[28,1,1],[11,1],[31,2],[12,1],[31,2],[13,1],[31,2],[14,1],[25,2,6,raw],[31,2],[32,2,-64,raw],[33,2,-1,raw]])
    s=base();add('fade_cache',True,s,[[31,1],[31,2],[17,1,1,0x3c888889],[18,1,2,0x3f000001],[19,1,1,0x80000000],[21,1,0x3f800000],[31,1]])
    s=base();s[4][0][4][0][7]=0x10020;s[4][0][4][1][7]=A.PAUSED|A.FIXED_STEP
    add('events_rawflags',True,s,[[27,1,0,0,128],[27,2,0,0,127],[31,1],[31,2],[21,1,0x3f000001],[15,1,1],[15,1,3],[16,1,32],[16,1,0x80000000]])
    s=base(1);s[4][0][4][0][6]=0x41400000;s[4][0][4][0][7]=A.LOOP
    add('loop_decoder_restart',True,s,[[31,1],[21,1,0x40400000],[31,1],[32,3,0,0x3fc00000],[33,3,1,0x3fc00000]])
    s=base();s[4][0][4][0][6]=0x40c00000;s[4][0][4][0][7]=A.REPORT_END
    add('report_end_keep',True,s,[[31,1],[25,2,7,A.KEEP],[25,2,4,0],[21,1,0x3f800000],[31,1],[31,2]])
    s=base(0);add('empty_and_identity',False,s,[[11,1],[15,1,0],[16,1,1],[3,1,1,0],[1,1,0,0],[22,1],[24,cell(3)],[23,[0,255,65],list(other)],[1,2,4,0],[31,4],[15,2,4],[0,2],[9,2]])
    s=base();add('weighted_push',True,s,[[31,1],[2,1,3,A.PERSISTENT,0x3f000001,0x3e800000,0x40000000,0xbe000000],[31,3],[6,1,1,0x3c888889],[21,1,0x3f000001],[7,1],[31,1]])
    s=base();add('transient_retention',True,s,[[34,1,0x3fc00000,0],[34,3,0x40000000,0],[31,1],[0,1],[29],[34,1,0xbf800000,0]])
    s=base(pool=2);add('eviction_live_stale',False,s,[[31,1],[31,2],[0,1],[31,3],[29],[35,1,3],[30,2],[29]])
    # A track's new clip does not replace an existing slot's old stream until restart.
    s=base();add('slot_clip_mismatch',True,s,[[31,1],[25,1,0,3],[25,1,6,0x40400000],[31,1],[25,1,6,0xbf800000],[31,1]])
    for cut in (0,2,8,14):
        key=struct.unpack_from('<I',data,8)[0];s=base();s[2][0][1]=list(data[:key+cut])
        add('eof_lifecycle_recovery',False,s,[[31,1],[8,1],[4,1,3,0,0],[31,3],[9,1],[34,1,0,0],[29]])
    s=base();add('word_mutation',False,s,[[36,1,0],[37,1,0,0],[36,1,0],[31,1],[0,1],[1,1,2,0],[31,1]])
    s=base();s[4][0][4][0][0]=0;s[4][0][4][0][1]=1;s[5][0][1:6]=[2,0,1000000,1,-7]
    add('null_active_lifecycle',False,s,[[8,1],[20,1],[15,1,0],[16,1,32],[9,2],[0,1]])
    s=base(pool=165);s[5]=[slot(i,False)for i in range(165)];s[5][-1]=slot(164,True)
    add('full_pool_cell_lifecycle',False,s,[[31,1],[31,2],[0,1],[29],[1,1,3,0],[31,1],[9,1]])
    return out


def cases(modules):
    for name,arithmetic,supplied in fixtures(modules):
        for config in (range(16)if arithmetic else (0,)):
            yield name,config,supplied


def independent_anchors(modules):
    rows={name:s for name,_,s in fixtures(modules)}
    def observations(name):
        v=run_case(modules,0,rows[name]);state=v[1];out=[]
        for result,changes in v[2]:state=apply_delta(state,changes);out.append((result,state))
        return out
    rows_clear=observations('clear_reuse')
    assert rows_clear[0][0]==3
    assert rows_clear[2][1][1][0][1][1]==0
    assert rows_clear[2][1][2][3][0][1][1]==3
    assert rows_clear[3][1][2][4][2][4]==0
    copied=observations('state_copy_alias')[2][1]
    assert copied[1][1][1][4]==[4,5,6]
    assert all(row[1][1]==0 for row in copied[2][3][3:6])
    assert observations('events_rawflags')[4][0]==-1
    assert observations('clock_seek')[5][1][2][0][1]==0x3c88596c
    assert observations('null_active_lifecycle')[2][0]==0
    assert observations('empty_and_identity')[3][0]=='HostFault'
    assert observations('stop_all_caches')[3][1][1][0][1][1]==1
    assert all(row[1][1]==0 for row in observations('stop_all_caches')[3][1][2][3])


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'animation-runtime-generator'
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
                header=b'# animation_runtime_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for scenario,config,supplied in cases(modules):
                    first=sc(run_case(modules,config,supplied))
                    if first!=sc(run_case(modules,config,supplied)):
                        raise AssertionError('fresh reference executions diverged')
                    row=('\t'.join([scenario,str(config),str(number)+';'+sc(supplied),'ok',first])+'\n').encode('ascii')
                    stream.write(row);vh.update(row);th.update(row)
                    for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[scenario]+=1;number+=1;size+=len(row);longest=max(longest,len(row))
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'animation-runtime','reference_sha256':REFERENCE_SHA256,
          'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,'composition_policy':COMPOSITION_POLICY,
          'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
          'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),
          'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
          'operations':dict(sorted(counts.items())),'replicas_per_case':2,'expanded_bytes':size,'max_line_bytes':longest,
          'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
          'helpers':{},
          'input_protocol':INPUT_PROTOCOL,
          'modeled_operations':list(MODELED_OPERATIONS),
          'composition_contract':{'scope':'Standalone original cache-bearing lifecycle/clock and decoder over one canonical registry. NonROOT/nonYAW only; no pose, sound, UI or Session integration.',
          'state':'Complete owned clips, canonical tracks including all state capacity and transient cells, state membership/count/rawflags, all64x16 slot cells/owners/LRU and one frame/cosmetic seed. Other original pose/VU/sound state remains outside the admitted API.',
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
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/animation_runtime.tsv.gz'))
    parser.add_argument('--scratch-dir',type=pathlib.Path)
    args=parser.parse_args()
    print(json.dumps(generate(args.research_dir,args.output,args.scratch_dir),sort_keys=True))

if __name__=='__main__':main()
