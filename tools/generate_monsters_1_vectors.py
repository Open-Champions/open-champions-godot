#!/usr/bin/env python3
"""Drive immutable monsters-1 Python with authored memory and callback recipes."""
import argparse
import ast
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
         'PhProp_initPhysics','fx_spawn','GameObject_ctor','obj_getPropInt','obj_moveOffGrid',
         'balance_lookup','Creature_setStats','anim_push','anim_randomizeTopTime','obj_runEvery60',
         'Creature_init','anim_clear','obj_runEveryFrame',
         'creature_canSeeObj','Creature_deadUpdate','Creature_hitTargetAtReach','Creature_hitAllEnemiesInArc',
         'Creature.v26','water_disturb','water_test','libc_sprintf','res_find','sound_playOnObject',
         'world_slideMove','obj_updateGridCell','world_collectObjectsInBox','Creature.v6','Creature.v10','Creature.v11',
         'creature_findEnemyInCone','world_raycast','path_requestToPoint','world_findFreeSpot',
         'SpellDrowsy_typeinfo','creature_findSpellByType','SpellDrowsy_getStat3Fraction']
# Declared units only: imported/inherited methods are not added to this count.
PORTED_UNITS = {
    'anim_flags','see','start','swing','dead_update','drop_loot','events','str_prefix_differs',
    '_shed_chunk','_badger_pick','_badger_chase','_scorpion_attack','_badger_attack',
    'smallspider_ctor','gnaw_ctor','mosquito_ctor','scorpion_ctor','badger_ctor',
    'smallspider_set_state','gnaw_set_state','mosquito_set_state','scorpion_set_state','badger_set_state',
    'smallspider_on_touch','mosquito_lose_part','scorpion_sever_part',
}
C,D,T,B,O=0x10000,0x20000,0x30000,0x40000,0x50000

def sc(v):
    if isinstance(v,bytes):return sc(["bytes",list(v)])
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
    tree=ast.parse((scratch/'ref_monsters_1.py').read_text())
    units={n.name for n in tree.body if isinstance(n,ast.FunctionDef)}
    units.update(n.name for c in tree.body if isinstance(c,ast.ClassDef)
                 for n in c.body if isinstance(n,ast.FunctionDef))
    if len(units)!=34 or not PORTED_UNITS <= units:
        raise ValueError('reference progress inventory changed')
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
        return callbacks(pool)+[[i,[0],[],-1] for i in range(16,22)]
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
            for ident in range(22):
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

def constructor_recipes():
    out=[]
    def cb_all():
        cb=callbacks()+[[i,[0],[],-1] for i in range(16,33)]
        cb[NAMES.index('balance_lookup')][1]=[[0,0,0,0,0,0]]
        return cb
    def add(code,seed=0,variant=0,name=0x70000,facing=0x8001,cb=None,extra=None):
        # Definition clip records and text are entirely authored. GameObject's
        # external implementation is supplied through an explicit mutation recipe.
        pokes=setup(seed)+[[D+off,4,0x60000+off*256] for off in range(0x1c,0xa0,8)]
        pokes += [[0x44fdcc,4,0xfffffff0],[0x4f85c4,1,255],
                  [0x545ea0,8,0x3fd3333333333333],[0x545ea8,8,0x3fdccccccccccccd]]
        pokes += [[0x70000+i,1,b] for i,b in enumerate(b'MoSqUiTo-big\0')]
        pokes += [[0x549af0+i,1,b] for i,b in enumerate(b'mosquito\0')]
        pokes += [[0x5628e1+i,1,1] for i in range(65,91)]
        service=cb_all() if cb is None else copy.deepcopy(cb)
        # Base external must supply the position and selected definition pointer.
        service[22][2]=[[C+0x24,4,D],[C+0x10,4,0x3f800000],[C+0x14,4,0xbf800000],[C+0x18,4,0x7fffffff]]+service[22][2]
        pos=[0x3f800001,0x80000001,0xffc12345]
        op=[code,C,name,pos,facing,0x80000]
        if code==12:op=[code,C,pos,facing,0x80000,name]
        if code==14:op.append(variant)
        out.append(('constructors',[pokes+(extra or []),service,[],[op]]))
    for code in range(10,15):
        for seed in range(24):
            for name in (0,0x70000,0x70001):
                add(code,seed,seed,name,facing=(0,0x7fff,0x8000,0xffff,0x12348001)[seed%5])
        for variant in (0,1,2,0x80,0xff,0x101,0xffffffff,0x80000000):
            add(code,variant,variant)
        # All nonzero properties, repeated-call value changes, and short-circuit
        # light queries are independently observed, including partial failures.
        for value in (0,1,0xffffffff,0x80000000,0x7fffffff):
            cb=cb_all();cb[23][1]=[value,2,value,3];cb[10][1]=[0x7777,0x8888,0,0,1]
            cb[25][1]=[[1,0x3f800001,0xbf800000,0x7fffffff,0x80000001,0x40000000]]
            add(code,cb=cb)
        for ident in range(33):
            for failure in (-1,0,1):
                cb=cb_all();cb[ident][2]=[[C+0x24,4,D+8],[C+0x38,2,0xffff],[C+0xa8,8,0xfedcba9876543210],
                    [C+0x44,4,0xffffffff],[0x44fdcc,4,0x80000000],[C+0x11a,2,8],
                    [D+0x24,4,0x71234],[0x545ea0,8,0x7ff0000000000000]]
                cb[ident][3]=failure
                add(code,cb=cb)
        for mul,plus in ((0,0),(0x8000000000000000,0x8000000000000000),
                         (1,0x7ff8000000000123),(0x7ff0000000000000,0xfff0000000000000),
                         (0x3ff0000000000001,0x0010000000000000)):
            add(code,extra=[[0x545ea0,8,mul],[0x545ea8,8,plus]])
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

def decode_structured(v):
    if not isinstance(v,list):return v
    if v[0]==0:return None
    if v[0]==1:return v[1]
    if v[0]==2:return tuple(v[1:])
    if v[0]==3:return [decode_structured(x) for x in v[1:]]
    if v[0]==4:return tuple(decode_structured(x) for x in v[1:])
    if v[0]==5:return ('buf',bytes(v[1:]))
    raise ValueError('unknown structured recipe')

def attack_recipes():
    out=[]
    def cb_all():
        cb=callbacks()+[[i,[0],[],-1] for i in range(16,49)]
        cb[25][1]=[[0,0,0,0,0,0]]
        for i in range(33,49):cb[i][1]=[[1,0]]
        cb[43][1]=[[4,[1,0],[2,0,0,0]]]
        cb[45][1]=[[3]]
        cb[40][1]=[[5,65,66]]
        return cb
    def add(code,extra=(),arg=0,cb=None,ops=None,seed=0x12345678):
        pokes=setup(seed)+[[D+off,4,0x60000+off] for off in range(0x1c,0xa0,8)]
        pokes += [[C+0x234,4,B],[B+12,4,O],[C+0x1c,4,0x41c80000],
                  [C+0x11a,2,8],[C+0x231,1,8],[C+0xf4,4,2],
                  [C+0xd0,2,0xfff9],[C+0xd2,2,0x7fff],[C+0xd4,2,0x8000],
                  [T+8*27+4,4,0x2a55ec if code==18 else 0x244aa4]]
        commands=ops if ops is not None else [[code,C]] if code==17 else [[code,C,arg]]
        out.append(('attack_resolution',[pokes+list(extra),cb_all() if cb is None else cb,[],commands]))
    for node in (0,B):
        for face in (0,0x7fff,0x8000,0xffff,0x12348001):
            for arc in (0,7000,0x7fff,0x8000,0xffffffff):
                add(15,[[C+0x234,4,node],[C+0x38,2,face]],ops=[[15,C,arc,0x7fffffff,0xffc12345],[15,C,arc,0x80000001,1,face]])
    for flag in (0,1,0xffffffff):
        add(16,ops=[[16,C,0xffffffff,flag,0x7fffffff,[1,0x80000001,0xffc12345]]])
    for seed in range(64):add(17,seed=seed)
    for state in (6,7,0xffff):
        for level in (0,1,2,3,4,0xffffffff):
            for lo,hi in ((0,0),(1,0xffff),(0x8000,0x7fff),(0xfff9,7)):
                for flags,timer,node in ((8,2,B),(0,2,B),(8,0,B),(0,0,B),(8,0,0),(0,0,0),(0,0x80000000,B)):
                    add(18,[[C+0x232,2,state],[0x4f719c,4,level],[C+0xd0,2,lo],[C+0xd2,2,hi],
                            [C+0x11a,2,flags],[C+0xf4,4,timer],[C+0x234,4,node]],arg=3)
    for state in (6,7,8,9,10,0xffff):
        for variant in (0,1,0xff,0x81):
            for ev in (0,1,2,3,0xffffffff):
                for flags,latch,node in ((8,0,B),(8,1,B),(0,0,B),(0,0,0)):
                    cb=cb_all();cb[33][1]=[[1,1],[1,0],[1,1]]
                    add(19,[[C+0x232,2,state],[C+0x3cc,1,variant],[C+0x11a,2,flags],
                            [C+0x3c8,4,latch],[C+0x234,4,node]],ev,cb)
    # Full inherited settling: dry/wet, root motion, contacts, base/derived virtuals,
    # signed this adjustment, software turning and mutation/exception prefixes.
    for water in (0,1,2):
        for contact in (0,1,2,3):
            cb=cb_all();cb[39][1]=[[1,1 if water else 0]];cb[38][1]=[[1,0]]
            cb[45][1]=[[3,[1,O]]] if contact else [[3]]
            extra=[[C+0x231,1,0],[C+0x232,2,7],[C+0xbc,4,0x3f800000],[C+0x30,4,0xbf800000],
                   [0x44fdcc,4,0],[C+0xa8,8,0x2000000000000000 if water==2 else 0],
                   [C+0xb8,4,0x3f800000],[O+0xc,4,0x40],[O+0x1c,4,0x41c80000],[O+0x48,4,T+0x400],
                   [T+0x400+8*6,2,0xfffe],[T+0x400+8*6+4,4,(0x11a418,0x2ab8e0,0x1234)[max(0,contact-1)]],
                   [T+8*11+4,4,0x244a9c],[T+8*10+4,4,0x11a3f0],[T+8*26+4,4,0x10bb08],
                   [C+0xb0,4,0x3f800000]]
            add(19,extra,3,cb)
    for code in (18,19):
        for ident in (16,17,33,35,36,37,38,39,40,41,42,43,44,45,46,47,48):
            for fail in (-1,0):
                cb=cb_all();cb[ident][2]=[[C+0x232,2,10],[C+0x11a,2,0],[C+0x234,4,B],
                    [C+0x38,2,0x7fff],[C+0xf4,4,0],[C+0xd0,2,0xfff9],[C+0x3c8,4,0],
                    [C+0x44,4,0xffffffff]];cb[ident][3]=fail
                cb[33][1]=[[1,1]];cb[39][1]=[[1,1]]
                cb[45][1]=[[3,[1,O]]]
                add(code,[[C+0x231,1,0],[C+0x232,2,7],[0x44fdcc,4,0],
                    [C+0xbc,4,0x3f800000],[O+0x1c,4,0x41c80000]],3,cb)
    # Real inherited turning for Scorpion, reread flags after the virtual callback,
    # and own draw branch selection without a playing-clip countdown.
    for seed in range(24):
        for table,adjust in ((0x10bb08,0),(0x1234,0xfffe)):
            cb=cb_all();cb[37][2]=[[C+0x11a,2,0],[C+0x232,2,7]]
            add(18,[[C+0x232,2,7],[C+0xf4,4,0],[T+8*26+4,4,table],[T+8*26,2,adjust],
                [C+0xb0,4,0x3f800000],[C+0x2fc,1,seed%62],[O+0x10,4,0x40000000]],1,cb,seed=seed)
    return out

def chase_recipes():
    out=[]
    def cb_all():
        cb=callbacks()+[[i,[0],[],-1] for i in range(16,len(NAMES))]
        cb[25][1]=[[0,0,0,0,0,0]]
        for i in range(33,len(NAMES)):cb[i][1]=[[1,0]]
        cb[43][1]=[[4,[1,0],[2,0,0,0]]]
        cb[45][1]=[[3]];cb[40][1]=[[5,65,66]]
        cb[50][1]=[[1,0x3f800000]] # world_raycast: clear
        cb[52][1]=[[4,[2,0,0,0],[1,0]]] # free spot: (point, found)
        return cb
    def add(extra=(),cb=None,seed=0x12345678,external=None,ops=None,label='badger_chase'):
        pokes=setup(seed)+[[D+off,4,0x60000+off] for off in range(0x1c,0xa0,8)]
        pokes += [[C+0x234,4,B],[B+12,4,O],[C+0x232,2,3],[C+0x1c,4,0x41200000],
                  [C+0x38,2,0],[C+0x231,1,8],[C+0x300,4,100],[0x44fdcc,4,1],
                  [C+0x23c,4,0xffffffff],[C+0x240,4,0xffffffff],[C+0x2d4,4,100],
                  [C+0x2c8,4,0x3f800000],[C+0x2cc,4,0xbf800000],
                  [O+0x10,4,0x43480000],[O+0x14,4,0],[O+0x18,4,0],[O+0x1c,4,0x41200000],
                  [T+8*27+4,4,0x244aa4],[T+8*26+4,4,0x10bb08],[C+0xb0,4,0x3f800000]]
        out.append((label,[pokes+list(extra),cb_all() if cb is None else cb,external or [],ops or [[20,C]]]))
    # Every first-match decision, including all own-pick residues and wrapped
    # side facings. Earlier matches prevent later calls even if all later match.
    for seed in range(25):
        for face in (0,0x7fff,0x8000,0xffff):
            for branch in range(9):
                cb=cb_all()
                if branch<4:cb[33][1]=[[1,0]]*branch+[[1,1]]
                elif branch<7:cb[49][1]=[[1,0]]*(branch-4)+[[1,1]]
                elif branch==7:cb[33][1]=[[1,0]]*4+[[1,1]]
                add([[C+0x38,2,face],[C+0x234,4,0 if branch==8 else B]],cb,seed)
    # State/turn dispatch with signed this adjustments and explicit overrides.
    for slot in (26,27):
        for adjust in (0,4,0xfffe):
            for body in ((0x10bb08,0x1234) if slot==26 else (0x244aa4,0x3c2688,0x1234)):
                for override in (False,True):
                    cb=cb_all();cb[33][1]=[[1,0]]*4+[[1,1]] if slot==26 else [[1,1]]
                    add([[T+8*slot,2,adjust],[T+8*slot+4,4,body]],cb,
                        external=[slot] if override else [])
    # Complete movement to a target rim: arrival, behind-close arrival, direct
    # steering, pending/negative indexes, waypoint following and all slow bytes.
    for index in (-2,-1,0,1,14,15,16,17,0x7fffffff):
        for frame in (0,1,8,64,0xffffffff):
            for ray in (0x3f800000,0,0xbf800000,0x3f000000,0x7fffffff):
                cb=cb_all();cb[50][1]=[[1,ray]]
                extra=[[C+0x2bc,4,index&0xffffffff],[0x44fdcc,4,frame]]
                if index<16:extra += [[C+0x23c+8*index,4,100],[C+0x240+8*index,4,0],
                    [C+0x244+8*index,4,0xffffffff],[C+0x248+8*index,4,0xffffffff]]
                # Negative indexes can overlap the handle word: retain a valid
                # raw32 target rather than manufacturing an oversized pointer.
                extra += [[C+0x234,4,B],[B+12,4,O]]
                add(extra,cb,label='chase_navigation')
    for x,y,z in ((0,0,0),(0x3f800000,0xbf800000,0),(0x41a00000,0,0),(0x43480000,0,0),
                  (0xc3480000,0,0),(0,0x43480000,0),(0,0,0x43480000),
                  (1,0x80000001,0x7fffffff),(0x7f800000,0xff800000,0xffc12345)):
        for radius in (0,0x41200000,0x43480000,0xbf800000):
            for face in (0,0x7fff,0x8000):
                add([[O+0x10,4,x],[O+0x14,4,y],[O+0x18,4,z],[O+0x1c,4,radius],
                     [C+0x1c,4,radius],[C+0x38,2,face],[0x44fdcc,4,0]],label='chase_rim_arrival')
    for slow in (0,1,30,60,61,128,255):
        for off in (0,16000,16001,0xffffc17f,0xffff8000):
            for spell in (0,0x8888):
                cb=cb_all();cb[37][1]=[[1,off]];cb[53][1]=[[1,0x7777]]
                cb[54][1]=[[1,spell]];cb[55][1]=[[1,0x3e800000]]
                add([[T+8*26+4,4,0x1234],[C+0x31a,2,1],[C+0x2fc,1,slow],
                     [C+0xbc,4,0x40000000],[C+0xc0,4,0x3f800000],[C+0xb8,4,0x3f800000]],cb,
                    label='chase_speed')
    # Avoidance uses three forward rays, then left/right side rays; fractional
    # ray t multiplies distance squared (not squared itself).
    for rays in ((-1,), (0,), (0x3f800000,), (0x40000000,), (0x7fffffff,),
                 (0x3b800000,0xbf800000), (0x3b800000,0x3f800000),
                 (0x3b800000,0,0xbf800000), (0x3b800000,0,0x3f800000),
                 (0x3b800000,0x3f400000,0x3e800000),(0x3b800000,0x3e800000,0x3f400000),
                 (0x3b800000,0x3e800000,0x3e800000),
                 (0x3b800000,0x3b800000,0xbf800000),
                 (0x3b800000,0x3b800000,0x3f800000),
                 (0x3b800000,0x3b800000,0,0xbf800000),
                 (0x3b800000,0x3b800000,0,0x3f800000),
                 (0x3b800000,0x3b800000,0x3f400000,0x3e800000),
                 (0x3b800000,0x3b800000,0x3e800000,0x3f400000),
                 (0x3b800000,0x3b800000,0x3e800000,0x3e800000)):
        for frame in (0,8,64):
            cb=cb_all();cb[50][1]=[[1,x&0xffffffff] for x in rays]
            add([[C+0x1c,4,0x41200000],[C+0x10,4,0],[C+0x14,4,0],
                 [C+0x2ec,4,0x43390000],[C+0x2f0,4,0],[0x44fdcc,4,frame]],cb,
                label='chase_avoidance')
    # Sixteen advances, passed/far/current/expired waypoint rules and path writes
    # made by the service are observed before the pending index is stored.
    for mode in range(6):
        cb=cb_all();cb[50][1]=[[1,0x3f000000]]
        extra=[[C+0x2c0,4,0 if mode&1 else 1000],[0x44fdcc,4,64]]
        for i in range(16):
            extra += [[C+0x23c+8*i,4,(0 if mode<2 else 100 if mode<4 else i+1)],
                      [C+0x240+8*i,4,0]]
        if mode==5:cb[51][2]=[[C+0x23c,4,999],[C+0x2bc,4,0]]
        add(extra,cb,label='chase_waypoint_progress')
    # Stuck side-step: one own draw, two signs; 300-tick teleport only when
    # side-step ends. A free spot's found flag uses the signed LOW BYTE.
    for seed in range(12):
        for clock,until,progress in ((100,0,0),(100,101,0),(100,100,0xffffffff),
                                     (100,100,0),(301,301,0),(0x80000000,0x80000000,0x7fffffff)):
            for found in (0,1,0x80,0x100):
                cb=cb_all();cb[52][1]=[[4,[2,0x42c80000,0x42480000,1],[1,found]]]
                add([[C+0x300,4,clock],[C+0x2d8,4,until],[C+0x2d4,4,progress]],cb,seed,
                    label='chase_stuck')
    # Full dry/wet collision and root-motion path, retaining derived contact
    # dispatch, plus cutscene/progress recovery after movement.
    for wet in (0,1,2):
        for cutscene in (0,1,255):
            cb=cb_all();cb[39][1]=[[1,1 if wet else 0]];cb[45][1]=[[3,[1,O]]]
            add([[C+0x231,1,0],[C+0xb8,4,0x3f800000],[C+0xbc,4,0x3f800000],
                 [C+0x30,4,0xbf800000],[0x44fcd0,1,cutscene],[0x44fdcc,4,0],
                 [C+0xa8,8,0x2000000000000000 if wet==2 else 0],
                 [O+0xc,4,0x40],[O+0x48,4,T+0x400],[T+0x400+8*6+4,4,0x11a418],
                 [T+8*11+4,4,0x244a9c],[T+8*10+4,4,0x11a3f0]],cb,label='chase_collision')
    # Each service may mutate the target, definition, facing, clock and steering
    # inputs, or throw. Retain prefixes without pretending callbacks are atomic.
    for ident in (1,4,6,7,16,33,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55):
        for fail in (-1,0,1):
            for branch in (0,4,8):
                cb=cb_all()
                if branch==0:cb[33][1]=[[1,1]]
                if branch==4:cb[49][1]=[[1,1]]
                cb[53][1]=[[1,0x7777]];cb[54][1]=[[1,0x8888]]
                cb[ident][2]=[[C+0x38,2,0x8000],[C+0x234,4,B],[B+12,4,O],
                    [C+0x232,2,3],[C+0x300,4,400],[C+0x2d8,4,400],[C+0x2d4,4,0],
                    [C+0x2ec,4,0x43480000],[C+0x1c,4,0x3f800000],[C+0x31a,2,1],
                    [C+0x11a,2,8],[C+0x44,4,0xffffffff]]
                cb[ident][3]=fail
                cb[45][1]=[[3,[1,O]]]
                cb[43][1]=[[4,[1,1],[2,0,0,0]]]
                if ident in (50,51):cb[50][1]=[[1,0x3f000000]]
                add([[0x44fdcc,4,64 if ident in (50,51) else 0],[O+0x48,4,T+0x400],
                     [C+0x231,1,0],[C+0x31a,2,1],[C+0x300,4,400],[C+0x2d8,4,400],
                     [C+0x2d4,4,0],[C+0xb8,4,0x3f800000],[T+8*26+4,4,0x1234]],cb,
                    label='chase_callback_prefix')
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
            value=returns[min(index,len(returns)-1)]
            return decode_structured(value) if ident>=33 else value
        model.cb[NAMES[ident]]=cb
    for row in inputs[0]:memory.poke(*row)
    def snapshot():
        return copy.deepcopy([sorted(memory.bytes.items()),memory.log,model.writes,
            [[name,[typed(a) for a in args]] for name,args in model.calls],events,model.draws,counts])
    observation=[snapshot(),[]]
    methods=(model.smallspider_set_state,model.smallspider_on_touch,model.mosquito_lose_part,
             model.scorpion_sever_part,model.str_prefix_differs, None,
             model.gnaw_set_state,model.mosquito_set_state,model.scorpion_set_state,model.badger_set_state,
             model.smallspider_ctor,model.gnaw_ctor,model.mosquito_ctor,model.scorpion_ctor,model.badger_ctor,
             model.see,model.dead_update,model._badger_pick,model._scorpion_attack,model._badger_attack,model._badger_chase)
    for op in inputs[3]:
        status='ok';result=None
        try:
            if op[0]!=5:
                args=list(op[1:])
                if 10<=op[0]<=14:
                    idx=1 if op[0]==12 else 2
                    args[idx]=tuple(args[idx])
                if op[0]==16:args[4]=tuple(args[4])
                result=methods[op[0]](*args)
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
            for label,inputs in recipes()+state_recipes()+constructor_recipes()+attack_recipes()+chase_recipes():
                lines.append(str(config)+'\t'+sc(inputs)+'\t'+sc(execute(ref,config,inputs))+'\n')
    payload=''.join(lines).encode('ascii')
    with args.output.open('wb') as target:
        with gzip.GzipFile(fileobj=target,filename='',mtime=0,mode='wb') as stream:stream.write(payload)
    manifest={'schema_version':1,'research_commit':COMMIT,'references':PINS,'callback_names':NAMES,
              'reference_unit_count':34,'ported_units':sorted(PORTED_UNITS),
              'rows':len(lines),'vector_sha256':hashlib.sha256(payload).hexdigest(),
              'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
              'protocol':'config TAB input TAB expected; input=[pokes,callback_specs,external_slots,commands]; snapshot=[bytes,store_log,writes,calls,events,shared_draws,callback_counts]'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(str(len(lines))+' pinned monsters-1 exact cases')
if __name__=='__main__':main()
