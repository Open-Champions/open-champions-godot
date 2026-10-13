#!/usr/bin/env python3
"""Drive immutable monsters-1 Python with authored memory and callback recipes."""
import argparse
import copy
import gzip
import hashlib
import importlib
import json
import pathlib
import subprocess
import sys
import tempfile

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
 'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
 'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
 'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
 'tools/ref_monsters_1.py': '33a76a127c66ffd0b4c8c91c014d250c91c567cd168c2bc8870d5abae647c4d0',
 'docs/spec/monsters-1.md': '4c70c678d0f2d0060f330054b157778e2b3535fed56e9fca456ce2bedbe76348',
 'tools/difftest_monsters_1.py': 'b68ce2a06c46e3919d8859fbf0c415059f6790fe0dc81e24b50231336447849e',
}
NAMES = ['handle_clear','anim_start','decal_add','anim_jumpTopToEnd','Creature.v27',
         'Creature.v28','terrain_heightBelow','model_buildHideMask','pool_new',
         'GutChunk_ctorFromProp','obj_getPropStr','script_runByName','loot_spawnNamedItem',
         'obj_delete','arena_addKill','creature_stopBossMusic',
         'anim_startBlendInOut','anim_syncToSimFrame','Creature_dropLoot','GutChunk_ctor',
         'PhProp_initPhysics','fx_spawn']
C,D,T,B,O=0x10000,0x20000,0x30000,0x40000,0x50000

def sc(v):
    if v is None:return '_'
    if isinstance(v,str):return 's'+v.encode().hex()
    if isinstance(v,(list,tuple)):return '['+','.join(sc(x) for x in v)+']'
    return str(int(v))

def typed(v):
    return ['tuple',list(v)] if isinstance(v,tuple) else v

def load(research,scratch):
    for path,digest in PINS.items():
        source=subprocess.run(['git','-C',str(research),'show',COMMIT+':'+path],capture_output=True,check=True).stdout
        if hashlib.sha256(source).hexdigest()!=digest:raise ValueError('reference hash mismatch '+path)
        if path.startswith('tools/ref_'):(scratch/pathlib.Path(path).name).write_bytes(source)
    sys.path.insert(0,str(scratch))
    for name in ['ref_core','ref_creature','ref_arena_monsters','ref_monsters_1']:sys.modules.pop(name,None)
    return importlib.import_module('ref_monsters_1')

def setup(seed=0x12345678):
    return [[C+0x24,4,D],[C+0x48,4,T],[C+0x44,4,seed],[C+0x300,4,0xfffffff0],
            [C+0x38,2,0x8001],[C+0xa8,8,0x8000000000001000],[C+0x10,4,0x3f800000],
            [C+0x14,4,0xbf800000],[C+0x18,4,0],[C+0x3c0,4,0x43960000],
            [O+0xc,4,0x40],[0x404464,4,0x89abcdef],[0x404460,4,0x98765432],
            [0x44fdc8,4,0x6789],[T+8*27+4,4,0x2ab704],[T+8*28+4,4,0x10beb8],
            [D,4,0x123],[D+8,4,0x234],[D+12,4,0xabc],[D+16,4,0x345]] + [
            [D+off,4,0x60000+off] for off in (0x1c,0x24,0x2c,0x34,0x3c,0x4c)]

def callbacks(pool=0,mask=0xfedcba9876543210):
    return [[i,[mask if i==7 else pool if i==8 else 0],[],-1] for i in range(16)]

def state_recipes():
    out=[]
    def cb_all(pool=0):
        return callbacks(pool)+[[i,[0],[],-1] for i in range(16,len(NAMES))]
    def add(label,pokes,ops,cb=None,external=None):
        # Distinct synthetic handles catch every clip-offset error, including
        # paths beyond the legacy SmallSpider definition entries.
        clips=[[D+off,4,0x60000+off] for off in range(0x1c,0xa0,8)]
        out.append((label,[clips+pokes,cb_all() if cb is None else cb,external or [],ops]))
    states=list(range(32))+[0xffff,0x8000,0x10000,0x80000000,0xffffffff]
    for species in range(6,10):
        for state in states:
            for root,stun,cool,target in ((0,0,0,0),(0,0,0,1),(1,1,1,1),(1,1,1,0),(0,1,1,1),(0,0,1,1)):
                add('remaining_state_filter',setup()+[[C+0x232,2,3],[C+0x314,2,root],[C+0x31c,2,stun],
                    [C+0x318,2,cool],[C+0x234,4,target]],[[species,C,state]])
        for current in (0,3,5,6,7,8,9,10,19,20,0xffff):
            for flags in (0,8,0x10):
                for charm in (0,1,0xffff):
                    add('reaction_and_chase',setup()+[[C+0x232,2,current],[C+0x11a,2,flags],[C+0x312,2,charm]],
                        [[species,C,18],[species,C,3],[species,C,1]])
        for seed in range(24):
            for hidden in (0,2):
                cb=cb_all(B if seed%2 else 0);cb[8][1]=[B,0] if seed%3==0 else cb[8][1]
                add('own_draw_branches',setup(seed)+[[C+0xc,4,hidden]],
                    [[species,C,5],[species,C,6],[species,C,18],[species,C,20],[species,C,23],[species,C,24]],cb)
        for state in (0,1,3,6,16,18,20):
            for ident in range(len(NAMES)):
                for fail in (-1,0):
                    cb=cb_all(B);cb[ident][2]=[[C+0x232,2,6],[C+0x11a,2,8],[C+0x300,4,0x80000000],
                        [C+0xa8,8,0x123456789abcdef0],[C+0x10,4,0x7fffffff],[D+12,4,0x9876],
                        [C+0x24,4,D+8],[C+0x44,4,0xffffffff]];cb[ident][3]=fail
                    add('state_callback_prefix',setup(),[[species,C,state]],cb)
        for gate in (0,1):
            add('state_pvp_refusal',setup()+[[0x4f7120,4,1],[0x503890,4,gate]],[[species,C,20],[species,C,1]])
        fn=(0x267a0c,0x297f54,0x2a55ec,0x244aa4)[species-6]
        for adjust in (0,4,0xfffe):
            for external in ([],[27]):
                add('new_virtual_states',setup()+[[T+8*27,2,adjust],[T+8*27+4,4,fn]],[[1,C,O]],external=external)
    return out

def recipes():
    out=[]
    def add(label,pokes,ops,cb=None,external=None):
        out.append((label,[pokes,callbacks() if cb is None else cb,external or [],ops]))
    states=list(range(32))+[0xffff,0x8000,0x10000,0x80000000,0xffffffff]
    for state in states:
        for root,stun,cool,target in ((0,0,0,0),(0,0,0,1),(1,1,1,1),(1,1,1,0),(0,1,1,1),(0,0,1,1)):
            add('state_filter',setup()+[[C+0x232,2,3],[C+0x314,2,root],[C+0x31c,2,stun],
                [C+0x318,2,cool],[C+0x234,4,target]],[[0,C,state]])
    for flag in (0,1,2,0x8000000,0x400000,0x800000,0xc00000):
        for home_range in (0,0x3f800000,0x43960000,0xbf800000,0x7fffffff):
            add('wander',setup()+[[C+0xa8,8,flag],[C+0x3c0,4,home_range]],[[0,C,1]])
    for flag in (0,0x8000000,0x400000,0x800000,0xc00000):
        for x,z in ((0x42f00000,0),(0x43480000,0),(0,0x42f20000)):
            add('wander_distance',setup()+[[C+0xa8,8,flag],[C+0x10,4,x],
                [C+0x14,4,0],[C+0x18,4,z]],[[0,C,1]])
    for ground in (0,0x41400000,0xc1400000,0x7fffffff):
        cb=callbacks();cb[6][1]=[ground]
        add('wander_ten_tries',setup()+[[C+0x10,4,0],[C+0x14,4,0]],[[0,C,1]],cb)
    for modea in (0,1):
        for modeb in (0,1):
            for gate in (0,1):
                for clear in (0,1):
                    add('pvp_filter',setup()+[[0x4f711c,4,modea],[0x4f7120,4,modeb],
                        [0x503890,4,gate],[0x503894,4,clear],[C+0x231,1,255]],[[0,C,1],[0,C,20],[1,C,O]])
    for hidden in (0,2):
        for virtual in (0,1,2):
            cb=callbacks();cb[10][1]=[0x7777,0x8888]
            add('death_hook',setup()+[[C+0xc,4,hidden],[0x4f7120,4,1],
                [C+0x2e4,4,O],[T+8*28+4,4,0x10beb8 if virtual!=1 else 0x1234]],
                [[1,C,O],[1,C,O],[0,C,20]],cb,[28] if virtual==2 else [])
    for current in (0,20,0xffff):
        for flags in (0,0x40,0x80):
            for toucher in (0,O):
                add('touch',setup()+[[C+0x232,2,current],[O+0xc,4,flags],[12,4,0x40]],[[1,C,toucher]])
    for fn in (0x3c2688,0x2ab704,0x1234):
        for adjust in (0,0xfffe,4):
            add('virtual_dispatch',setup()+[[T+8*27,2,adjust],[T+8*27+4,4,fn]],[[1,C,O]])
    for species in (2,3):
        for part in list(range(40))+[0xffffffff,0x80000000,0x10000001]:
            for pool in (0,B):
                cb=callbacks(pool);cb[7][1]=[0x0123456789abcdef,0xfedcba9876543210]
                cb[9][2]=[[B+0x1c4,4,0xbf800000]]
                add('part_shedding',setup()+[[C+0xe8,4,0x12345678]],[[species,C,part],[species,C,part]],cb)
    # Callback mutation reaches rereads of definitions, variants, positions and chunk lift.
    for species in (0,1,2,3):
        for callback_id in range(16):
            cb=callbacks(B);cb[callback_id][2]=[[C+0xe8,4,0xffffffff],[D+12,4,0x9876],
                [C+0xc,4,2],[C+0x38,2,0x1234],[B+0x1c4,4,0x7fffffff]]
            for fail in (-1,0,1):
                cb2=copy.deepcopy(cb);cb2[callback_id][3]=fail
                add('callback_effects',setup(),[[species,C,20 if species==0 else O if species==1 else 3]],cb2)
    for raw_value in (0,0x80000000,0x00000001,0x7fc12345,0xffc12345,0x7fffffff,0xffffffff,0x3f800001):
        cb=callbacks(B)
        add('chunk_arithmetic',setup()+[[B+0x1c4,4,raw_value]],[[2,C,1],[3,C,33]],cb)
    for classification in (0,1):
        for byte in range(256):
            pokes=setup()+[[0x70000,1,byte],[0x71000,1,byte],
                           [0x5628e1+(byte if byte<128 else byte-256),1,classification]]
            add('signed_prefix',pokes,[[4,0x70000,0x71000],[5,0x70000,1,(byte+32)&255],[4,0x70000,0x71000]])
    add('prefix_words',setup()+[[0x70000+i,1,b] for i,b in enumerate(b'MoSqUiTo!\0')]+
        [[0x71000+i,1,b] for i,b in enumerate(b'mosquito\0')]+
        [[0x5628e1+i,1,1] for i in range(65,91)],[[4,0x70000,0x71000],[4,0x71000,0x70000]])
    add('sequence',setup(),[[0,C,0],[0,C,1],[0,C,18],[1,C,O],[0,C,0],[2,C,5],[3,C,63]])
    return out

def execute(ref,config,inputs):
    ref.C.set_fpu(('ps2','ps2g1','chop','nearest')[config//4],bool(config&2),bool(config&1))
    memory=ref.RC.Memory();events=[];counts=[0]*max(16,1+max((row[0] for row in inputs[1]),default=-1))
    class Trace(ref.Model):
        def _w(self,a,n,v):
            events.append(['write',a,n,v&((1<<(n*8))-1)])
            return super()._w(a,n,v)
        def call(self,name,*args):
            events.append(['call',name,[typed(a&0xffffffff if isinstance(a,int) else a) for a in args]])
            return super().call(name,*args)
    model=Trace(memory,external=['Creature.v'+str(i) for i in inputs[2]])
    for ident,returns,mutations,fail in inputs[1]:
        def cb(*args,ident=ident,returns=returns,mutations=mutations,fail=fail):
            index=counts[ident];counts[ident]+=1
            for row in mutations:memory.store(*row)
            if index==fail:raise ValueError('authored callback failure')
            return returns[min(index,len(returns)-1)]
        model.cb[NAMES[ident]]=cb
    for row in inputs[0]:memory.poke(*row)
    def snapshot():
        return copy.deepcopy([sorted(memory.bytes.items()),memory.log,model.writes,
            [[name,[typed(a) for a in args]] for name,args in model.calls],events,model.draws,counts])
    observation=[snapshot(),[]]
    methods=(model.smallspider_set_state,model.smallspider_on_touch,model.mosquito_lose_part,
             model.scorpion_sever_part,model.str_prefix_differs, None,
             model.gnaw_set_state,model.mosquito_set_state,model.scorpion_set_state,model.badger_set_state)
    for op in inputs[3]:
        status='ok';result=None
        try:
            if op[0]!=5:result=methods[op[0]](*op[1:])
            else:memory.poke(*op[1:])
        except (ValueError,IndexError,OverflowError) as e:status=type(e).__name__
        observation[1].append([status,result,snapshot()])
    return observation

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--output',type=pathlib.Path,required=True)
    args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True)
    scratch=pathlib.Path('work');scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch,prefix='monsters-reference-') as tmp:
        ref=load(args.research_dir,pathlib.Path(tmp));lines=[]
        for config in range(16):
            for label,inputs in recipes()+state_recipes():
                lines.append(str(config)+'\t'+sc(inputs)+'\t'+sc(execute(ref,config,inputs))+'\n')
    payload=''.join(lines).encode('ascii')
    with args.output.open('wb') as target:
        with gzip.GzipFile(fileobj=target,filename='',mtime=0,mode='wb') as stream:stream.write(payload)
    manifest={'schema_version':1,'research_commit':COMMIT,'references':PINS,'callback_names':NAMES,
              'rows':len(lines),'vector_sha256':hashlib.sha256(payload).hexdigest(),
              'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
              'protocol':'config TAB input TAB expected; input=[pokes,callback_specs,external_slots,commands]; snapshot=[bytes,store_log,writes,calls,events,shared_draws,callback_counts]'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(str(len(lines))+' pinned monsters-1 exact cases')
if __name__=='__main__':main()
