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
ABI_NAMES = ['Creature.v10', 'Creature.v11', 'Creature.v16', 'Creature.v17', 'Creature.v19', 'Creature.v2', 'Creature.v20', 'Creature.v23', 'Creature.v25', 'Creature.v26', 'Creature.v27', 'Creature.v28', 'Creature.v30', 'Creature.v31', 'Creature.v34', 'Creature.v35', 'Creature.v36', 'Creature.v37', 'Creature.v6', 'Creature.v7', 'Creature_applyKnockback', 'Creature_deadUpdate', 'Creature_dropLoot', 'Creature_hitAllEnemiesInArc', 'Creature_hitTargetAtReach', 'Creature_hitTargetInArc', 'Creature_hitTargetInArcEdge', 'Creature_init', 'Creature_setCharm', 'Creature_setStats', 'Critter_ctor', 'GameObject_ctor', 'GroundPoundEffect_ctor', 'GutChunk_ctor', 'GutChunk_ctorFromProp', 'Holder.v1', 'Holder.v11', 'Holder.v15', 'Holder.v17', 'Holder.v2', 'Holder.v21', 'Holder.v22', 'Holder.v4', 'Holder.v6', 'LightEffect_ctor', 'Nightmare_ctor', 'OniBall_ctor', 'PhProp_initPhysics', 'Skill.v1', 'Skill.v11', 'Skill.v12', 'Skill.v14', 'Skill.v2', 'Skill.v8', 'SkillLifeTap_getStat0Fraction', 'Spell.v1', 'Spell.v2', 'SpellDrowsy_getStat3Fraction', 'SpellDrowsy_typeinfo', 'anim_advance', 'anim_clear', 'anim_jumpTopToEnd', 'anim_push', 'anim_randomizeTopTime', 'anim_releaseCaches', 'anim_snapTopToEnd', 'anim_start', 'anim_startBlendInOut', 'anim_syncToSimFrame', 'arena_addKill', 'balance_lookup', 'combat_applyAreaDamage', 'creature_addSpell', 'creature_bossRank', 'creature_canSeeObj', 'creature_countAlliesNear', 'creature_findEnemyInCone', 'creature_findSpell', 'creature_findSpellByType', 'creature_isBlockedBy', 'creature_stopBossMusic', 'damage_apply', 'decal_add', 'fx_burnModel', 'fx_disintegrateModel', 'fx_ringBurst', 'fx_shatterToLooseIce', 'fx_shockModel', 'fx_spawn', 'handle_clear', 'handle_link', 'handle_removeNode', 'iter_begin', 'iter_end', 'iter_next', 'libc_sprintf', 'loot_spawnNamedItem', 'lump_load', 'model_buildHideMask', 'model_getAttachPoint', 'model_getBounds', 'model_getSamplePoint', 'model_getSamplePointCount', 'net_objOrGhost', 'obj_delete', 'obj_getPropInt', 'obj_getPropStr', 'obj_moveOffGrid', 'obj_runEvery60', 'obj_runEveryFrame', 'obj_updateGridCell', 'particleDef_ctor', 'particle_spawn', 'pathFollower_init', 'path_requestToPoint', 'path_requestType1', 'path_requestType2', 'path_requestType3', 'paths_findByName', 'player_getSkillRank', 'player_giveExp', 'player_speedScale', 'players_anyWithinRadius', 'pointList_getPoint', 'pointList_snapToNearest', 'pool_new', 'res_find', 'script_runByName', 'sound_playAtPos', 'sound_playOnObject', 'terrain_height', 'terrain_heightBelow', 'terrain_raycast', 'water_disturb', 'water_test', 'world_collectInSphere', 'world_collectObjectsInArc', 'world_collectObjectsInBox', 'world_findFreeSpot', 'world_findObjectByName', 'world_notifyObjectsNear', 'world_raycast', 'world_slideMove']
NAMES += [name for name in ABI_NAMES if name not in NAMES]

# Declared units only: imported/inherited methods are not added to this count.
PORTED_UNITS = {
    'vcall','anim_flags','see','start','swing','dead_update','drop_loot','events','str_prefix_differs',
    '_shed_chunk','_badger_pick','_badger_chase','_scorpion_attack','_badger_attack',
    'smallspider_ctor','gnaw_ctor','mosquito_ctor','scorpion_ctor','badger_ctor',
    'smallspider_set_state','gnaw_set_state','mosquito_set_state','scorpion_set_state','badger_set_state',
    'smallspider_on_touch','mosquito_lose_part','scorpion_sever_part','shape_result','call',
    'smallspider_run','gnaw_run','mosquito_run','scorpion_run','badger_run',
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
        cb=callbacks()+[[i,[0],[],-1] for i in range(16,56)]
        cb[25][1]=[[0,0,0,0,0,0]]
        for i in range(33,56):cb[i][1]=[[1,0]]
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

def abi_recipes(ref):
    if sorted(ref.EXTERNALS)!=ABI_NAMES:raise ValueError('ABI name inventory changed')
    out=[]
    for name in ABI_NAMES:
        ident=NAMES.index(name);ext=ref.EXTERNALS[name]
        # Extra arguments deliberately exercise only the shape walk, which does
        # not validate argument count or float-register layout in the reference.
        for null in (0,1):
            args=[[1,null] for _ in range(24)]
            supplied=[[2,1,0x80000000,3],[1,17],[3,[1,7],[1,8]],
                      [0],[4,[1,-2],[1,99]],[1,255],[1,6],[1,7]]
            for outs in (None,[],supplied[:1],supplied):
                out.append(('abi_shape',[[],[],[],[[21,ident,args,0x12345678,0x7fc12345,0 if outs is None else outs]]]))
            out.append(('abi_signed',[[],[],[],[[21,ident,args,-9,-11,0]]]))
            out.append(('abi_missing',[[],[],[],[[22,ident,args]]]))
            for value in ([0],[1,-9],[2,1,2,3],[3,[1,4],[1,-5]],
                          [4,[1,6],[3,[1,7]]],[5,0,65,255]):
                # New callback IDs use generic structured handlers; original IDs
                # preserve their legacy callback transport convention.
                if ident>=56:
                    out.append(('abi_callback',[[],[[ident,[value],[],-1]],[],[[22,ident,args]]]))
        # Buffer bytes and untyped supplied output payloads are both preserved.
        if ext.ret=='s':
            for value in ([5],[5,0,65,255],[1,9],[3,[1,8]],[0]):
                out.append(('abi_text',[[],[],[],[[21,ident,[],0,0,[value]]]]))
        if ext.ret!='n':
            out.append(('abi_raw64',[[],[],[],[[21,ident,[[1,1]]*24,0xfedcba9876543210,0xfedcba9876543210,0]]]))
    for ident in range(56):
        if ident==25:returns=[[1,2,3,4,5,6]]
        elif ident<33:returns=[0xfedcba9876543210]
        elif ident in (40,43,45,52):returns=[[4,[1,7],[2,1,2,3]]]
        else:returns=[[1,-9]]
        out.append(('abi_existing_handler',[[],[[ident,returns,[],-1]],[],[[22,ident,[[1,-1]]]]]))
    # Python truth tests use the original unnormalized argument in shape_result.
    for name,position in (('world_slideMove',3),('model_getSamplePoint',6),('world_raycast',4)):
        for test in ([0],[1,-1],[1,0x100000000],[3],[4],[5],[2,0,0,0],[3,[1,0]]):
            args=[[1,0] for _ in range(8)];args[position]=test
            out.append(('abi_truth',[[],[],[],[[21,NAMES.index(name),args,0,0,[[1,17],[1,19]]]]]))
    for ident in range(56,len(NAMES)):
        for fail in (-1,0,1):
            out.append(('abi_failure',[setup(),[[ident,[[4,[1,5],[1,-6]]],[[C+0x44,4,9]],fail]],[],
                [[22,ident,[[1,-1],[1,0x100000001],[4,[1,-1],[1,0x100000001]],[5,65]]]]*2]))
    return out

def run_recipes(ref=None):
    """Complete ordinary ticks: state decisions and inherited lifecycle branches."""
    out=[]
    bodies=[0x2ab704,0x267a0c,0x297f54,0x2a55ec,0x244aa4]
    def cb_all():
        cb=callbacks()+[[i,[0],[],-1] for i in range(16,len(NAMES))]
        cb[25][1]=[[0,0,0,0,0,0]]
        for i in range(33,len(NAMES)):cb[i][1]=[[1,0]]
        for name in ('world_collectObjectsInBox','world_collectInSphere','world_collectObjectsInArc'):cb[NAMES.index(name)][1]=[[3]]
        cb[43][1]=[[4,[1,0],[2,0,0,0]]];cb[40][1]=[[5,65,0,66]]
        cb[52][1]=[[4,[2,0,0,0],[1,0]]]
        cb[NAMES.index('model_getBounds')][1]=[[4]+[[1,v] for v in (-10,-20,-8,10,20,24)]]
        cb[NAMES.index('model_getAttachPoint')][1]=[[2,0x3f800000,0x40000000,0x40400000]]
        cb[NAMES.index('player_speedScale')][1]=[[1,0x3f800000]]
        cb[NAMES.index('terrain_raycast')][1]=[[1,0x3f800000]]
        return cb
    def add(f=0,extra=(),cb=None,state=2,ops=None,external=None,seed=0x12345678):
        pokes=setup(seed)+[[C+0x232,2,state],[C+0xc,4,0x20],[C+0xa8,8,0x8000000000000000],
            [C+0x231,1,8],[C+0x38,2,0],[C+0x300,4,100],[C+0x228,4,0x42c80000],
            [C+0x22c,4,0x42c80000],[C+0xdc,4,0x3f800000],[C+0x20,4,0x42400000],
            [C+0x1c,4,0x41200000],[C+0x3c4,2,600],[0x403a08,1,1],[0x44fdcc,4,1],
            [C+0x23c,4,0xffffffff],[C+0x240,4,0xffffffff],[C+0x2bc,4,16],
            [C+0x2c8,4,0x3f800000],[C+0x2cc,4,0xbf800000],[C+0x2d4,4,100],
            [T+8*27+4,4,bodies[f]],[T+8*26+4,4,0x10bb08],
            [T+8*16+4,4,0x3c2738],[T+8*17+4,4,0x11a868],[T+8*23+4,4,0x10ded0],
            [T+8*25+4,4,0x10e658],[T+8*37+4,4,0x3c2730],[T+8*20+4,4,0x119328],
            [O+0x48,4,0x71000],[0x71000+8*37+4,4,0x3c2730],[O+0x310,1,1],
            [B+12,4,O],[O+0x10,4,0x43480000],[O+0x1c,4,0x41200000]]
        out.append(('ordinary_run',[pokes+list(extra),cb_all() if cb is None else cb,external or [],ops or [[23+f,C]]]))
    # Every signed/unsigned state, sight branch, animation completion and event.
    for f in range(5):
        for state in list(range(26))+[0x7fff,0x8000,0xffff]:
            for target,anim,event,sight in ((0,0,0,0),(B,8,1,1),(B,0x10,4,0),(B,0,3,0),(B,0x20,7,1)):
                cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,event]];cb[33][1]=[[1,sight]]
                add(f,[[C+0x234,4,target],[C+0x11a,2,anim],[C+0xf4,4,101]],cb,state)
        # Gate changes, exact signed clocks, inert/non-inert target selection.
        for active,camera,radius,pos in ((0,0,600,0),(1,0,0,0),(1,1,600,0),(0,1,600,0x44fa0000)):
            add(f,[[C+0xa8,8,active<<63],[0x4039c1,1,camera],[C+0x3c4,2,radius],[C+0x10,4,pos],[C+4,4,B],[C+0xc,4,0xc00020]])
        for clock in (0,0x7fffffff,0x80000000,0xffffffff):
            for slow in (0,1,30,60,61,255):
                cb=cb_all();cb[NAMES.index('anim_advance')][2]=[[C+0xbc,4,0x40000000],[C+0xc0,4,0xbf800000],[C+0xc4,2,0x8001]]
                add(f,[[C+0x300,4,clock],[C+0x2fc,1,slow],[C+0xf4,4,clock]],cb,0)
        for timer in (0,1,2,0x7fff,0x8000,0xffff):
            add(f,[[C+off,2,timer] for off in (0x314,0x316,0x318,0x31a,0x31c,0x312)]+[[0x44fdcc,4,0],[C+0x90,2,1],[C+0x92,2,0xffff],[C+0x98,2,0x8000],[C+0x9a,2,0x7fff]])
        for name in ('iter_begin','iter_end','Creature_applyKnockback','anim_advance','anim_releaseCaches','handle_link'):
            cb=cb_all();cb[NAMES.index(name)][3]=0
            cb[NAMES.index(name)][2]=[[C+0x3c8,4,77]]
            add(f,[[C+0xa8,8,0],[0x403a08,1,0 if name=='anim_releaseCaches' else 1]],cb)
    # Damage effects / survivors / deaths / award branches, all knockback lanes.
    for f in range(5):
        for cf in (0,1,0x400,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x20000000,0x100000000):
            for hp,pending,damage in ((0x42c80000,0x3f800000,13),(0,0x3f800000,0x38080d),(0,0x3f800000,0x10),(0,0x3f800000,0x20),(0,0x3f800000,0x2000000),(0x3f800000,0,0x2000)):
                cb=cb_all();cb[8][1]=[0x80000]
                add(f,[[C+0xa8,8,(1<<63)|cf],[C+0x228,4,hp],[C+0xc8,4,pending],[C+0x2dc,4,damage],
                    [0x44fdc0,4,1],[0x404468,4,123]],cb,2)
        for bit in (0,0x40000,0x80000,0x100000,0x200000,0x3c0000):
            add(f,[[C+0x2dc,4,bit],[C+0xc8,4,0x3f800000]],state=18)
        for players in (0,1,2,3):
            for killer in (0,B):
                cb=cb_all();cb[NAMES.index('SkillLifeTap_getStat0Fraction')][1]=[[1,0x3f800000]];cb[8][1]=[0x80000]
                extra=[[0x4d8ae0,4,players],[C+0xd8,4,0x42c80000],[C+0xc8,4,0x3f800000],[C+0x228,4,0],[C+0x2e4,4,killer]]
                for i in range(3):
                    rec=0x4d8af0+i*0x4c30
                    extra += [[rec+0x43d8,4,O],[rec+0x59,1,1+i],[rec+0xac,4,0 if i==1 else 0x3f800000],[rec+0xc4,4,0x3f800000],[0x41b558+4*(1+i),4,100*i],[0x41b558+4*(2+i),4,100*(i+1)]]
                add(f,extra,cb)
    # Lifecycle effects, status counters, boss meter and scavenger shared RNG.
    for f in range(5):
        for timer in (0x3c888888,0x3d888888,0x3f800000,0x42700000,0x7fffffff):
            for object_flags in (0x20,0x40,0x20020,0x4000020,8):
                cb=cb_all();cb[NAMES.index('net_objOrGhost')][1]=[[1,C]]
                add(f,[[C+0xc,4,object_flags],[0x44fdcc,4,0]]+[[C+off,4,timer] for off in (0x340,0x344,0x348,0x34c,0x350,0x354)],cb)
        for counter in (0xffffffff,1,30,31,60):
            for frame in (0,30,31,0x80000000):add(f,[[C+0x388,4,counter],[0x44fdcc,4,frame],[C+0x6c,4,0x3f000000],[0x44fe08,4,0x100]])
        for shown in (0,0x80000000):
            for near in (0,0x44fa0000):
                add(f,[[C+0xa8,8,(1<<63)|shown|1],[C+0x231,1,0],[0x4f8424,4,1],[0x4f8430,4,0],[0x4d8af0+0x43d8,4,O],[O+0x10,4,near]])
        for kind in (0,1,2,3):
            cb=cb_all();cb[8][1]=[0x80000]
            add(f,[[C+0xa8,8,(1<<63)|0x1000000],[C+0x11a,2,0x80],[0x402768,4,kind]],cb,20)
    # Follow path/player, activity follow scripts and spell list dispatch.
    for f in range(5):
        for point in (0,0x80000):
            for index in (0,0xffff):
                cb=cb_all();cb[10][1]=[0x90000];cb[NAMES.index('pointList_getPoint')][1]=[[1,point]]
                add(f,[[C+0xa8,8,(1<<63)|0x8000000],[C+0x58,2,index],[0x80000,4,0x43480000],[0x80000+0x3c,2,3]],cb)
        for cf in (0x400000,0x800000):
            for active in (0,1):
                for leader in (0,O):
                    add(f,[[C+0xa8,8,(active<<63)|cf],[C+0x312,2,0xffff],[C+0x5a,1,0],[C+0x310,1,0xff],[0x4d8ae0,4,1],[0x4d8af0+0x43d8,4,leader],[0x40278c,4,0x42c80000]])
        for spellbody in (0,0x2e177c,0x30cbc8):
            for phase in (0,1,2,3,4):
                cb=cb_all();cb[NAMES.index('iter_begin')][1]=[[1,0x80000]];cb[NAMES.index('iter_next')][1]=[[1,0]]
                cb[NAMES.index('Skill.v12')][1]=[[1,4]];cb[NAMES.index('world_collectObjectsInArc')][1]=[[3,[1,O]]]
                extra=[[0x80000+12,4,0x90000],[0x90000+0x14,4,0xa0000],[0xa0000+20,4,spellbody],
                    [0x90000+12,4,C],[0x90000+0x10,4,123],[0x90000+0x7c,4,0x41200000],[0x90000+0x80,2,0xffff]]
                if phase==0:extra += [[0x90000+0x7c,4,0]]
                if phase==1:extra += [[0x90000+0x82,2,8]]
                if phase>=2:extra += [[0x90000+0x80,2,0],[0x90000+0x18,4,B],[0x90000+0x38,4,0x41200000]]
                if phase==3:extra += [[0x90000+0x82,2,1],[0x90000+0x58,4,0]]
                if phase==4:extra += [[O+0x10,4,0x3f800000],[O+0x14,4,0],[0x90000+0x38,4,0],[0x90000+0x82,2,0]];cb[8][1]=[0xb0000]
                add(f,extra,cb)
        for rays in ((0x3f800000,0), (0x3e800000,O), (0xbf800000,0), (0x7fc12345,O)):
            cb=cb_all();cb[50][1]=[[4,[1,rays[0]],[1,rays[1]]]]
            add(f,[[C+0x234,4,B],[C+0x318,2,20],[C+0xf8,4,0xbf800000],[C+0xfc,4,0xbf800000],[0x44fdcc,4,0]],cb,16)
    # Explicit overrides keep their signed this adjustment and raw64 return.
    for f in range(1,5):
        cb=cb_all();cb[4][1]=[0xfedcba9876543210]
        add(f,[[T+8*27,2,0xfffe]],cb,3,external=[27])
    for f in range(5):
        for slow in (1,30,60):
            cb=cb_all();cb[54][1]=[[1,0x80000]];cb[55][1]=[[1,0x3f000000]]
            cb[NAMES.index('anim_advance')][2]=[[C+0xbc,4,0x40000000],[C+0xc0,4,0xbf800000],[C+0xc4,2,0x7fff]]
            add(f,[[C+0x31a,2,2],[C+0x2fc,1,slow],[C+0x11a,2,8]],cb,3)
        for found in (0,0x90000):
            cb=cb_all();cb[10][1]=[found]
            add(f,[[C+0xc,4,0xc00020],[C+0xa8,8,0]],cb)
        for side in (0,1,2,3):
            cb=cb_all();cb[50][1]=[[4,[1,0x3e800000],[1,O]]]
            add(f,[[C+0x234,4,B],[C+0x318,2,20],[C+0x2f8,1,side],[C+0x2bc,4,0],[C+0x23c,4,5],[C+0x240,4,7],[C+0x244,4,0xffffffff],[0x44fdcc,4,0]],cb,16)
    # Target scans, forced priority, failures retain temporary exclude mask.
    for f in range(1,5):
        for fail in (-1,0):
            cb=cb_all();cb[45][1]=[[3,[1,O],[1,0x60000]]];cb[45][3]=fail
            add(f,[[C+0x231,1,0],[C+0x234,4,B],[0x44fdcc,4,0],[0x60000+12,4,0x20],[0x60000+0x48,4,0x71000],[0x60000+0x310,1,2],[0x60000+0x10,4,0x42c80000]],cb,3)
    # Generic Creature burn table calls must never become arena skill dispatch,
    # even if a host table happens to contain a recognized cast-body address.
    for f in range(5):
        for body in (0,0x2e2368,0x2e18a4,0x30c708):
            for adjust in (0,0xfffe):
                cb=cb_all();cb[NAMES.index('Skill.v11')][1]=[[1,100]]
                add(f,[[C+12,4,0x40],[C+0x340,4,0x3f800000],
                    [0x44fdcc,4,0],[0x41b1a4,4,0x80000],
                    [0x80000+0x60,4,0x90000],[0x90000+8*11,2,adjust],
                    [0x90000+8*11+4,4,body]],cb)
        # Timers, root motion and animation events persist across ticks. Include
        # cold-level and NaN inputs without replacing the reference arithmetic.
        for raw in (0,0x80000000,0x3f800000,0x7f800000,0x7fc12345):
            cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,1],[1,4],[1,0]]
            add(f,[[C+0x354,4,raw],[C+0x344,4,raw],[C+0xdc,4,raw],
                [C+0x318,2,3],[C+0x31c,2,3],[C+0x312,2,3],
                [C+0xbc,4,raw],[C+0xc0,4,raw]],cb,state=18,
                ops=[[23+f,C]]*3)
    if ref is not None:
        observed={}
        for label,inputs in out:
            observation=execute(ref,0,inputs)
            for name,_args in observation[1][-1][2][3]:
                if name not in observed:observed[name]=inputs
        for name,original in observed.items():
            ident=NAMES.index(name)
            for fail in (-1,0):
                inputs=copy.deepcopy(original)
                inputs[1][ident][2]=[[C+0x3c8,4,0x12345678],[C+0x11a,2,0],[C+0x228,4,0]]
                inputs[1][ident][3]=fail
                out.append(('run_service_prefix',inputs))
    return out

def skelight_recipes(ref=None):
    """Complete inherited Skelight lifecycle, kept separate from legacy inputs."""
    out=[]
    def cb_all():
        cb=callbacks()+[[i,[0],[],-1] for i in range(16,len(NAMES))]
        cb[25][1]=[[0,0,0,0,0,0]]
        for i in range(33,len(NAMES)):cb[i][1]=[[1,0]]
        for name in ('world_collectObjectsInBox','world_collectInSphere','world_collectObjectsInArc'):
            cb[NAMES.index(name)][1]=[[3]]
        cb[43][1]=[[4,[1,0],[2,0,0,0]]];cb[40][1]=[[5,65]]
        cb[52][1]=[[4,[2,0,0,0],[1,0]]]
        cb[NAMES.index('model_getBounds')][1]=[[4]+[[1,v] for v in (-10,-20,-8,10,20,24)]]
        cb[NAMES.index('model_getSamplePoint')][1]=[[4,[2,0x3f800000,0x40000000,0x40400000],[0]]]
        cb[NAMES.index('model_getSamplePointCount')][1]=[[1,7]]
        cb[NAMES.index('Holder.v15')][1]=[[1,2]]
        cb[NAMES.index('Holder.v17')][1]=[[4,[1,17],[4,[1,18],[1,19]]]]
        return cb
    def base(seed=0x12345678,state=2):
        return setup(seed)+[[D+off,4,0x60000+off*256] for off in range(0x1c,0xa0,8)]+[
            [C+0x232,2,state],[C+12,4,0x20],[C+0xa8,8,0x8000000000000000],
            [C+0x231,1,8],[C+0x300,4,100],[C+0x228,4,0x42c80000],[C+0x22c,4,0x42c80000],
            [C+0xdc,4,0x3f800000],[C+0x20,4,0x42400000],[C+0x1c,4,0x41400000],
            [C+0x3c4,2,600],[C+0xb0,4,0x3fc00000],[C+0x38,2,0],
            [0x403a08,1,1],[0x44fdcc,4,1],[C+0x23c,4,0xffffffff],[C+0x240,4,0xffffffff],
            [C+0x2bc,4,16],[C+0x2c8,4,0x3f800000],[C+0x2cc,4,0xbf800000],[C+0x2d4,4,100],
            [T+8*27+4,4,0x2a91b8],[T+8*26+4,4,0x10bb08],[T+8*16+4,4,0x3c2738],
            [T+8*17+4,4,0x11a868],[T+8*23+4,4,0x10ded0],[T+8*25+4,4,0x10e658],
            [T+8*37+4,4,0x3c2730],[T+8*20+4,4,0x119328],
            [B+12,4,O],[O+0x48,4,0x71000],[0x71000+8*37+4,4,0x3c2730],
            [O+0x310,1,1],[O+0x10,4,0x43480000],[O+0x1c,4,0x41200000],
            [C+0x50,4,0x80000],[0x80000,4,0x90000],[0x3e9ce0+40,4,0x76543210],
            [0x8e4560,8,0xfedcba9876543210]]
    def add(label,extra=(),cb=None,ops=None,state=2,seed=0x12345678,external=None):
        out.append((label,[base(seed,state)+list(extra),cb_all() if cb is None else cb,
                           external or [],ops or [[29,C]]]))
    # Every state/filter combination, timer redirection, signed state and repeated calls.
    for state in list(range(36))+[0x7fff,0x8000,0xffff,0x10000,0x80000000,0xffffffff]:
        for current in (0,6,18,20,23):
            for root,stun,cool,target in ((0,0,0,0),(0,0,0,B),(1,0,0,B),(0,1,0,B),(0,0,1,B),(1,1,1,B)):
                add('skelight_state_filter',[[C+0x314,2,root],[C+0x31c,2,stun],[C+0x318,2,cool],[C+0x234,4,target]],
                    state=current,ops=[[30,C,state]])
    for state in list(range(36))+[0x7fff,0x8000,0xffff]:
        for target,anim,event,sight in ((0,0,0,0),(B,8,1,1),(B,0x10,4,0),(B,0,3,0),(B,0x80,1,1)):
            cb=cb_all();cb[33][1]=[[1,sight]];cb[NAMES.index('anim_advance')][1]=[[1,event]]
            add('skelight_run_state',[[C+0x234,4,target],[C+0x11a,2,anim],[C+0xf4,4,1]],cb,state=state)
    # Swing latch/countdown re-reads, holder presence, signed table row and this adjustment.
    for timer in (0,1,2,0x7fffffff,0x80000000,0xffffffff):
        for target in (0,B):
            for anim in (0,8,0x10,0x80):
                for end in (0,8):
                    cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,1]]
                    cb[NAMES.index('Creature_hitTargetInArc')][2]=[[C+0x11a,2,end]]
                    add('skelight_swing',[[C+0x234,4,target],[C+0x11a,2,anim],[C+0xf4,4,timer]],cb,state=6)
    for holder in (0,0x80000):
        for row in (-2,-1,0,2,0x7fffffff,0x80000000):
            for adjust in (0,0xfffe,0x7fff,0x8000):
                cb=cb_all();cb[NAMES.index('Holder.v15')][1]=[[1,row]]
                add('skelight_holder',[[C+0x50,4,holder],[0x90000+8*15,2,adjust],
                    [0x90000+8*22,2,adjust],[0x3e9ce0+((row*16)&0xffffffff)+8 & 0xffffffff,4,0x11223344]],cb)
    for state in (0,1,3,5,6,16,18,19,20,23):
        for raw in (0,0x80000000,0x3f800000,0x7f800000,0x7fc12345):
            cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,1],[1,4],[1,0]]
            add('skelight_ticks',[[C+0x231,1,0],[C+0x234,4,B],[C+0x11a,2,8],
                [C+0xc8,4,raw],[C+0xdc,4,raw],[C+0xb8,4,raw],[C+0x31c,2,3]],cb,state=state,ops=[[29,C]]*3)
    for active,camera,radius,pos in ((0,0,600,0),(1,0,0,0),(1,1,600,0),(0,1,600,0x44fa0000)):
        add('skelight_inactive_still_runs',[[C+0xa8,8,0x8000000000000000 if active else 0],
            [0x4039c1,1,camera],[C+0x3c4,2,radius],[C+0x10,4,pos]],state=23)
    # Deferred rising damage versus ordinary hit processing and lethal shattering.
    for state in (2,6,23):
        for hp in (0,0x3f800000,0xbf800000):
            for flag in (0,0x2000,0x80000,0x400000,0x200):
                add('skelight_damage',[[C+0xc8,4,0x3f800000],[C+0x228,4,hp],[C+0x2dc,4,flag]],state=state)
    for count in (-1,0,1,2,3,4,7,12):
        cb=cb_all();cb[NAMES.index('model_getSamplePointCount')][1]=[[1,count]]
        cb[NAMES.index('model_getSamplePoint')][2]=[[C+0x38,2,0x8000],[D+12,4,0x12345678],[C+0x10,4,0x40000000]]
        add('skelight_shatter_samples',cb=cb,ops=[[30,C,20],[31,0xffffffff,0xffffffff],[30,C,20]])
    for body in (0x2a8b84,0x2a91b8):
        for slot in (4,27,35):
            for adjust in (0,0xfc00):
                for override in (False,True):
                    cb=cb_all();ident=NAMES.index('Creature.v'+str(slot)) if slot!=4 else 4
                    cb[ident][1]=[19] if ident<33 else [[1,19]]
                    # Slot 4 is absent from the external ABI; no override recipe
                    # invents a new callback name. Direct body dispatch still covers it.
                    if slot==4 and override:continue
                    extra=[[T+8*slot,2,adjust],[T+8*slot+4,4,body]]
                    if adjust:extra += [[a-0x400,n,v] for a,n,v in base() if C<=a<C+0x464]
                    add('skelight_virtual_body',extra,cb,ops=[[32,C,slot,[] if body==0x2a8b84 else [[1,6]]]],external=[slot] if override else [])
    # Constructor: temporary level global, byte-wise case folding with 64-byte cap,
    # holder/event write order, no balancing call for the null type name.
    ctor_inputs=[]
    for level in (0,1,10,0x7fffffff,0x80000000,0xffffffff):
        for facing in (0,0x7fff,0x8000,0xffff,0x12348001):
            for name,other in ((b'alpha',b'ALPHA'),(b'alpha',b'other'),(b'',b''),(b'A'*64+b'x',b'a'*64+b'y'),(bytes([0xc0]),bytes([0xe0]))):
                cb=cb_all();cb[22][2]=[[C+0x24,4,D],[C+0x10,4,0x3f800000],[C+0x14,4,0xbf800000],[C+0x18,4,0x7fffffff]]
                extra=[[0x4f714c,4,0x7fc12345],[0x4f85c4,1,255]]
                extra += [[0x44fe18+i,1,b] for i,b in enumerate(name+b'\0')]
                extra += [[0x54c080+i,1,b] for i,b in enumerate(other+b'\0')]
                op=[28,C,[0x3f800001,0x80000001,0xffc12345],facing,0x70000,level]
                add('skelight_constructor',extra,cb,ops=[op]);ctor_inputs.append(copy.deepcopy(out[-1][1]))
    # Exercise replacement of holder/definition pointers between callbacks.
    for name in ('Holder.v15','Holder.v17','Holder.v22','Creature_hitTargetInArc','Creature.v26','anim_advance'):
        cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,1]]
        cb[NAMES.index(name)][2]=[[C+0x50,4,0xa0000],[0xa0000,4,0xb0000],[C+0x11a,2,0],
            [C+0x38,2,0x8000],[C+0x234,4,B],[C+0xf4,4,0],[C+0x232,2,6]]
        add('skelight_callback_rereads',[[C+0x234,4,B],[C+0x11a,2,8],[T+8*26+4,4,0x1234]],cb,state=6)
    if ref is not None:
        observed={}
        for label,inputs in out:
            observation=execute(ref,0,inputs)
            for name,_args in observation[1][-1][2][3]:
                observed.setdefault(name,inputs)
        for name,original in observed.items():
            ident=NAMES.index(name)
            for fail in (-1,0,1):
                inputs=copy.deepcopy(original)
                inputs[1][ident][2]=[[C+0x11a,2,0],[C+0x38,2,0x8000],[C+0x228,4,0]]
                inputs[1][ident][3]=fail
                out.append(('skelight_service_prefix',inputs))
        for original in ctor_inputs[:5]:
            for name in ('GameObject_ctor','Holder.v1','Creature_setStats','model_buildHideMask','anim_clear','anim_push','obj_runEveryFrame','Creature_init'):
                inputs=copy.deepcopy(original);ident=NAMES.index(name);inputs[1][ident][3]=0
                inputs[1][ident][2]+=[[0x4f714c,4,0x40000000]]
                out.append(('skelight_constructor_failure',inputs))
    return out

def arena_cast_recipes(ref=None):
    """Arena casts and all constructor/sampling dependencies, synthetic only."""
    out=[];skill=0x70000;stable=0x71000;spell=0x80000
    def cb_all():
        cb=callbacks()+[[i,[[1,0]],[],-1] for i in range(16,len(NAMES))]
        for i in range(16,33):cb[i][1]=[0]
        cb[25][1]=[[0,0,0,0,0,0]]
        cb[22][2]=[[B+0x24,4,D],[B+0x10,4,0x3f800000],[B+0x14,4,0xbf800000],[B+0x18,4,0x40000000]]
        cb[NAMES.index('model_getSamplePoint')][1]=[[2,0x3f800000,0x40000000,0x40400000]]
        cb[NAMES.index('world_findFreeSpot')][1]=[[4,[2,0x41200000,0xc1a00000,0x41f00000],[1,1]]]
        cb[NAMES.index('Skill.v12')][1]=[[1,10],[1,100]]
        cb[NAMES.index('Skill.v14')][1]=[[1,3]]
        return cb
    # Keep all names/resources as supplied addresses, no retail strings.
    def add(label,extra=(),cb=None,ops=None,external=None):
        pokes=setup()+[[T+8*16+4,4,0x3c2738],[T+8*37+4,4,0x3c2730],
            [skill+0x60,4,stable],[C+0xc,4,0],[C+0x3c8,4,0],[C+0x310,1,0xff],
            [0x503788,4,0x12345678]]+[[0x503778+4*k,4,0x23456700+k] for k in range(4)]
        out.append((label,[pokes+list(extra),cb_all() if cb is None else cb,external or [],ops or [[34,skill,1,C,[0x3f800000,0xbf800000,0x40000000],0]]]))
    for flag in (0,0x40,0xffffffff):
        for slot in (0,1,2,3,4,0xff,0xffffffff):
            for found in (0,1,0xffffffff):
                cb=cb_all();cb[NAMES.index('world_findObjectByName')][1]=[[1,found]]
                add('summon_can_use',[[C+12,4,flag],[C+0x3c8,4,slot]],cb,[[33,skill,C]])
    for level in (0,1,255,256,0x80000000,0xffffffff):
        for facing in (0,0x7fff,0x8000,0xffff,0x12348001):
            for seed in (0,0xffffffff):
                ops=[[36,spell,level,C,skill,0x76543210],[37,spell+0x100,level,C,skill,0],
                     [37,spell+0x200,level,C,skill,B],[38,spell+0x300,level,C,skill]]
                add('spell_constructors',[[C+0x38,2,facing],[0x404464,4,seed]],ops=ops)
    for raw in (0,0x80000000,0x00000001,0x7f800000,0xff800000,0x7fc12345,0x3f800001):
        add('tagar_constructor_arithmetic',[[C+0x10,4,raw],[C+0x14,4,raw],[C+0x18,4,raw]],ops=[[38,spell,256,C,skill]])
    for flag,slot in ((0,0),(0x40,0),(0x40,3),(0x40,4),(0x40,0xffffffff)):
        for level in (0,1,0x80000000,0xffffffff):
            for allocation in ([0],[B,0],[B,spell]):
                for chance in (0,100,0xffffffff):
                    cb=cb_all();cb[8][1]=allocation;cb[NAMES.index('Skill.v12')][1]=[[1,0x80000001],[1,chance]]
                    add('summon_allocation_charm',[[C+12,4,flag],[C+0x3c8,4,slot]],cb,
                        [[34,skill,level,C,[0x3f800000,0xbf800000,0x40000000],0xffff]])
    for seed in range(12):
        cb=cb_all();cb[8][1]=[B,spell]
        add('summon_elemental_pick',[[0x404464,4,seed]],cb)
    for height in (0x41bfffff,0x41c00000,0x41c00001,0xc1c00001,0x7f800000,0xff800000,0x7fc12345):
        for found in (0,1,0x100,0xff,0x80000000):
            cb=cb_all();cb[NAMES.index('terrain_height')][1]=[[1,height]]
            cb[NAMES.index('world_findFreeSpot')][1]=[[4,[2,0x41200000,0xc1a00000,0x41f00000],[1,found]]]
            add('summon_height_fallback',[[C+0x18,4,0]],cb)
    for count in (1,-1,7,0x80000000):
        for seed in (0,1,0xffffffff):
            cb=cb_all();cb[NAMES.index('model_getSamplePointCount')][1]=[[1,count]]
            cb[NAMES.index('model_getSamplePoint')][2]=[[C+0x38,2,0x8000],[C+0x10,4,0x40000000],[D+12,4,0x34567890]]
            add('summon_cosmetic_particles',[[0x404460,4,seed]],cb)
    for level in (0,1,0xffffffff,0x80000000):
        for allocation in (0,spell):
            cb=cb_all();cb[8][1]=[allocation];cb[NAMES.index('Skill.v14')][1]=[[1,level]]
            add('tagar_cast',cb=cb,ops=[[35,skill,0x12345678,C,[0x7fc12345,0x80000000,1],0x8001]])
    for body,argv in ((0x2e2368,[[1,C]]),(0x2e18a4,[[1,1],[1,C],[2,0,0,0],[1,0]]),
                      (0x30c708,[[1,1],[1,C],[2,0,0,0],[1,0]])):
        for slot in (1,2,8):
            for adjust in (0,0xfffe,0x8000):
                for override in (False,True):
                    add('arena_skill_body_lookup',[[stable+8*slot,2,adjust],[stable+8*slot+4,4,body]],
                        ops=[[39,skill,slot,argv]],external=[100+slot] if override else [])
    if ref is not None:
        observed={}
        for label,inputs in out:
            observation=execute(ref,0,inputs)
            for name,_args in observation[1][-1][2][3]:observed.setdefault(name,inputs)
        for name,original in observed.items():
            for fail in (-1,0,1):
                inputs=copy.deepcopy(original);ident=NAMES.index(name)
                inputs[1][ident][2]+=[[C+0xc,4,0x40],[C+0x3c8,4,3],[C+0x38,2,0x8000],[C+0x18,4,0x42400000]]
                inputs[1][ident][3]=fail;out.append(('arena_cast_service_prefix',inputs))
        original=next(inputs for label,inputs in out if label=='summon_cosmetic_particles')
        for fail in (31,32,231):
            inputs=copy.deepcopy(original);inputs[1][NAMES.index('particle_spawn')][3]=fail
            out.append(('summon_particle_failure_prefix',inputs))
    return out

def horsemen_recipes(ref=None):
    """Whole mounted/on-foot lifecycle and cast dependencies, authored inputs."""
    out=[];holder=0x80000;htable=0x90000;skill=0xa0000;stable=0xa1000;mountnode=0xb0000;mount=0xb1000
    def cb_all():
        cb=callbacks()+[[i,[0] if i<33 else [[1,0]],[],-1] for i in range(16,len(NAMES))]
        cb[25][1]=[[0,0,0,0,0,0]]
        for name in ('world_collectObjectsInBox','world_collectInSphere','world_collectObjectsInArc'):
            cb[NAMES.index(name)][1]=[[3]]
        cb[43][1]=[[4,[1,0],[2,0,0,0]]];cb[40][1]=[[5,65]]
        cb[52][1]=[[4,[2,0,0,0],[1,0]]]
        cb[NAMES.index('model_getBounds')][1]=[[4]+[[1,v] for v in (-10,-20,-8,10,20,24)]]
        cb[NAMES.index('Holder.v17')][1]=[[4,[1,17],[4,[1,18],[1,19]]]]
        cb[NAMES.index('world_findFreeSpot')][1]=[[4,[2,0,0,0],[1,1]]]
        return cb
    def add(label,extra=(),cb=None,ops=None,state=2,external=None,seed=0x12345678):
        pokes=setup(seed)+[[D+off,4,0x60000+off*256] for off in range(0x1c,0xdc,8)]+[
            [C+0x232,2,state],[C+12,4,0x20],[C+0xa8,8,0x8000000000000200],
            [C+0x231,1,8],[C+0x300,4,100],[C+0x228,4,0x42c80000],[C+0x22c,4,0x42c80000],
            [C+0xdc,4,0x3f800000],[C+0x20,4,0x42400000],[C+0x1c,4,0x41a00000],
            [C+0x3c4,2,600],[C+0xb0,4,0x3f999999],[C+0x38,2,0],
            [0x403a08,1,1],[0x44fdcc,4,1],[C+0x23c,4,0xffffffff],[C+0x240,4,0xffffffff],
            [C+0x2bc,4,16],[C+0x2c8,4,0x3f800000],[C+0x2cc,4,0xbf800000],[C+0x2d4,4,100],
            [T+8*27+4,4,0x272f4c],[T+8*26+4,4,0x10bb08],[T+8*16+4,4,0x3c2738],
            [T+8*17+4,4,0x11a868],[T+8*23+4,4,0x10ded0],[T+8*25+4,4,0x10e658],
            [T+8*35+4,4,0x118c50],[T+8*37+4,4,0x3c2730],[T+8*20+4,4,0x119328],
            [B+12,4,O],[O+0x48,4,0x71000],[0x71000+8*37+4,4,0x3c2730],
            [O+0x310,1,1],[O+0x10,4,0x43480000],[O+0x1c,4,0x41200000],
            [C+0x50,4,holder],[holder,4,htable],[mountnode+12,4,mount],
            [mount+0x10,4,0x41200000],[mount+0x14,4,0x41a00000],[mount+0x18,4,0x41f00000],[mount+0x38,2,0x8001],
            [C+0xd0,2,0xfffc],[C+0xd2,2,5],[C+0xd4,2,0x8000],
            [0x41b180,4,skill],[0x41b18c,4,skill],[0x41b19c,4,skill],[0x41b2cc,4,skill],
            [skill+0x60,4,stable],[0x4f714c,4,0x41200000]]
        out.append((label,[pokes+list(extra),cb_all() if cb is None else cb,external or [],ops or [[40,C]]]))
    states=list(range(40))+[0x7fff,0x8000,0xffff,0x10000,0x80000000,0xffffffff]
    for state in states:
        for mounted in (0,mountnode):
            for root,stun,cool,target in ((0,0,0,0),(0,0,0,B),(1,1,1,B),(0,1,1,0),(0,0,1,B)):
                add('horsemen_state_filter',[[C+0x3c8,4,mounted],[C+0x314,2,root],[C+0x31c,2,stun],
                    [C+0x318,2,cool],[C+0x234,4,target],[C+0x11a,2,8]],ops=[[41,C,state]],state=6)
    for state in states:
        for mounted in (0,mountnode):
            for target,anim,event in ((0,0,0),(B,8,1),(B,0,2),(B,0x88,3),(B,0x90,0)):
                cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,event]]
                add('horsemen_run_state',[[C+0x3c8,4,mounted],[C+0x234,4,target],[C+0x11a,2,anim],
                    [C+0x472,2,2],[mount+0x234,4,B]],cb,state=state)
    for variant in (0,1,2,3,4,127,128,255):
        for body in (0,0x30c708,0x2e18a4):
            add('horsemen_cast_variant',[[C+0x470,1,variant],[C+0x234,4,B],[stable+8*2+4,4,body]],ops=[[42,C]])
    for variant in range(4):
        for allocation in (0,0xc0000):
            cb=cb_all();cb[8][1]=[allocation]
            add('horsemen_local_cast_allocation',[[C+0x470,1,variant],[C+0x234,4,B],
                [stable+8*2+4,4,0x30c708 if variant==2 else 0x2e18a4]],cb,ops=[[42,C]])
    for seed in range(20):
        for blocked in (0,1):
            cb=cb_all();cb[33][1]=[[1,1]];cb[NAMES.index('creature_isBlockedBy')][1]=[[1,blocked]]
            add('horsemen_between_attacks',[[C+0x234,4,B]],cb,state=5,seed=seed)
    for seen in ([0,1],[0,0,1],[0,0,0]):
        for speed in (0x42c80000,0x42c80001,0x7f800000,0x7fc12345):
            for facing in (0,0x3fff,0x4000,0x8000,0xffff):
                cb=cb_all();cb[33][1]=[[1,v] for v in seen];cb[NAMES.index('creature_countAlliesNear')][1]=[[1,1]]
                add('horsemen_charging_allies',[[C+0x234,4,B],[O+0x308,4,speed],[O+0x38,2,facing]],cb,state=3)
    for timer in (99,100,101,0x80000000,0xffffffff):
        for variant in (0,2,3,255):
            cb=cb_all();cb[49][1]=[[1,1]]
            add('horsemen_cast_selection',[[C+0x234,4,B],[C+0x474,4,timer],[C+0x470,1,variant],
                [stable+8*1+4,4,0x2e2368]],cb,state=3)
    for flags in (0,0x88,0x90,0x98):
        add('horsemen_dismount_animation',[[C+0x11a,2,flags]],state=35)
    for same in (0,1):
        for mine in (0,B):
            add('horsemen_mount_target',[[C+0x3c8,4,mountnode],[mount+0x234,4,0xb2000],
                [0xb2000+12,4,O if same else O+0x1000],[C+0x234,4,mine]],state=27)
    for petflags in (0,0x20):
        cb=cb_all();cb[NAMES.index('world_findObjectByName')][1]=[[1,0xc0000]]
        add('horsemen_pet_death',[[C+0x470,1,3],[0xc0000+12,4,petflags],
            [0xc0000+0x48,4,0xc1000],[0xc1000+8*27+4,4,0x2a91b8]],cb,ops=[[41,C,20]])
    for body,args in ((0x27236c,[]),(0x272f4c,[[1,6]])):
        for slot in (34,35,36):
            for adjust in (0,0xfffe,0x8000):
                for override in (False,True):
                    add('horsemen_virtual_body',[[T+8*slot,2,adjust],[T+8*slot+4,4,body]],
                        ops=[[43,C,slot,args]],external=[slot] if override else [])
    for variant in (0,1,2,3,31,32,255,0xffffffff):
        for mounted in (0,1):
            for allocated in (0,0xc0000):
                cb=cb_all();cb[8][1]=[allocated]
                cb[22][2]=[[C+0x24,4,D]]
                add('horsemen_constructor',[[0x5006e8,4,0x123],[0x5006f4,4,0xabc]],cb,
                    ops=[[44,C,0,[0x3f800000,0xbf800000,0],0x8001,0,variant,mounted]])
                out[-1][1][0]+=[[0x5006e8+off,4,0xd0000+off*256] for off in range(0x1c,0xdc,8)]
    for linked in (False,True):
        for type_name in (0,0x12345678):
            cb=cb_all();cb[8][1]=[0xc0000];cb[25][1]=[[1,0x3f800000,0x40000000,0x40400000,0x40800000,0x40a00000]]
            if linked:cb[NAMES.index('handle_link')][2]=[[C+0x3c8,4,mountnode]]
            add('horsemen_constructor_linked',[[0x5006e8+off,4,0xd0000+off*256] for off in range(0x1c,0xdc,8)]+
                [[0x50cda0+8*1,2,0xfffe]],cb,ops=[[44,C,type_name,[0x7fc12345,0x80000000,1],0xffff,0,3,1]])
    for raw in (0,0x80000000,1,0x3f800001,0x7f800000,0xff800000,0x7fc12345):
        for variant in range(4):
            add('horsemen_cast_arithmetic',[[C+0x470,1,variant],[C+0x234,4,B],[C+0x10,4,raw],
                [C+0x18,4,raw],[C+0x20,4,raw],[O+0x10,4,raw],[0x4f714c,4,raw]],ops=[[42,C]])
    for seed in (0,1,0xffffffff):
        for state in (0,5,16,23,26,35):
            cb=cb_all();cb[NAMES.index('anim_advance')][1]=[[1,1],[1,2],[1,0]]
            add('horsemen_repeated_ticks',[[C+0x234,4,B],[C+0x318,2,3],[C+0x31c,2,3],
                [C+0x472,2,3],[C+0x470,1,2]],cb,ops=[[40,C]]*3,state=state,seed=seed)
    if ref is not None:
        observed={}
        for label,inputs in out:
            observation=execute(ref,0,inputs)
            for name,_args in observation[1][-1][2][3]:observed.setdefault(name,inputs)
        for name,original in observed.items():
            for fail in (-1,0,1):
                inputs=copy.deepcopy(original);ident=NAMES.index(name)
                inputs[1][ident][2]+=[[C+0x50,4,0],[C+0x38,2,0x8000],[C+0x300,4,0x80000000]]
                inputs[1][ident][3]=fail;out.append(('horsemen_service_prefix',inputs))
    return out

def zek_recipes(ref=None):
    """Complete Zek lifecycle, sparks, wave and inherited damage, synthetic only."""
    out=[]
    base=horsemen_recipes()[0][1]
    def add(label,extra=(),cb=None,ops=None,state=2,seed=0x12345678,external=None):
        pokes=copy.deepcopy(base[0])+[[C+0x44,4,seed],[C+0x232,2,state],[C+0x231,1,0],
            [C+0xa8,8,0x8000000000000000],[C+0x3c8,4,0],[C+0x464,4,0],
            [T+8*27+4,4,0x2c382c],[T+8*35+4,4,0x2c3c54],[C+0x300,4,1000],
            [C+0x478,4,0],[C+0x47c,4,0],[O+12,4,0x20]]
        out.append((label,[pokes+list(extra),copy.deepcopy(base[1]) if cb is None else cb,external or [],ops or [[46,C]]]))
    def callbacks_for(name,values):
        cb=copy.deepcopy(base[1]);cb[NAMES.index(name)][1]=values;return cb
    states=list(range(24))+[0x7fff,0x8000,0xffff,0x10000,0x80000000,0xffffffff]
    for state in states:
        for root,stun,cool,target in ((0,0,0,0),(0,0,0,B),(1,1,1,B),(0,1,1,0),(0,0,1,B)):
            add('zek_state_filter',[[C+0x314,2,root],[C+0x31c,2,stun],[C+0x318,2,cool],
                [C+0x234,4,target]],ops=[[47,C,state]],state=6)
        for target,playing,event in ((0,0,0),(B,8,1),(B,0,2),(B,0x18,3),(B,0x90,0)):
            cb=callbacks_for('anim_advance',[[1,event]])
            add('zek_run_state',[[C+0x234,4,target],[C+0x11a,2,playing]],cb,state=state)
    for seed in range(20):
        for last in (0,8,0x80000000):
            cb=callbacks_for('creature_canSeeObj',[[1,1]])
            add('zek_front_attack_pick',[[C+0x234,4,B],[C+0x47c,4,last]],cb,state=5,seed=seed)
    for seen in ([0,1],[0,0,1],[0,0,0,1],[0,0,0,0]):
        for cones in ([0,0,0,0],[1],[0,1],[0,0,1],[0,0,0,1]):
            for clock in (179,180,181,1000,0x80000000,0xffffffff):
                cb=callbacks_for('creature_canSeeObj',[[1,v] for v in seen]);cb[49][1]=[[1,v] for v in cones]
                add('zek_priority_specials',[[C+0x234,4,B],[C+0x300,4,clock]],cb,state=5)
    for state in (3,5):
        for face in (0,0x4000,0x8000,0xffff):
            for speed in (0x42c80000,0x42c80001,0x7f800000,0x7fc12345):
                cb=callbacks_for('creature_canSeeObj',[[1,1]])
                add('zek_charging',[[C+0x234,4,B],[O+0x38,2,face],[O+0x308,4,speed]],cb,state=state)
    for name1,name2 in (([],[]),([65],[65]),([65],[97]),([65,66],[65]),([65]*65,[65]*64),([65]*63+[66],[65]*64)):
        for holder in (0,0x80000):
            add('zek_death_text',[[0x44fe18+k,1,v] for k,v in enumerate(name1)]+
                [[0x54e5f8+k,1,v] for k,v in enumerate(name2)]+[[C+0x50,4,holder]],ops=[[47,C,20]])
    for actor in (0,O):
        for flags in (0,0x2000,0x10000000,0x10002000):
            for inert,ignore in ((0,0),(1,0),(255,0),(0,0x1000000000000)):
                for attacker_flags in (0x20,0x80,0xa0):
                    add('zek_damage_route',[[C+0x231,1,inert],[C+0xa8,8,ignore],[O+12,4,attacker_flags],
                        [O+0x4c,4,0xb0000],[0xb0000+12,4,0xc0000],[C+0x360,4,0xb1000],
                        [0xb1000+12,4,0xc1000]],ops=[[48,C,actor,flags,0x3f800000]])
    for seed in (0,1,2,3,0x12345678,0xffffffff):
        for ready in (0,1):
            for raw in (0,0x80000000,1,0x7f800000,0x7fc12345):
                add('zek_spark_arithmetic',[[0x503748,4,ready],[C+0x20,4,raw],[C+0x1c,4,raw],
                    [C+0x228,4,raw],[C+0x10,4,raw]],ops=[[48,C,O,0x2000,raw]],seed=seed)
    for terrain in (0,0x41400000,0x41400001,0xc1500000,0x7f800000,0xff800000,0x7fc12345):
        for enemy in (0,B):
            for distance in (0,0x42100000,0x44480000,0x44480001):
                cb=callbacks_for('terrain_height',[[1,terrain]]);cb[8][1]=[0xc0000]
                add('zek_wave_termination',[[C+0x464,4,1],[C+0x460,4,enemy],[C+0x468,2,0],
                    [C+0x46c,4,distance],[C+0x470,4,0],[C+0x474,4,0x3f800000]],cb,ops=[[50,C]])
    for heading in (0,0x4000,0x7fff,0x8000,0xffff):
        for raw in (0,0x80000000,1,0x3f800001,0x7f800000,0xff800000,0x7fc12345):
            add('zek_wave_steering',[[C+0x464,4,1],[C+0x460,4,B],[C+0x468,2,heading],
                [C+0x46c,4,raw],[C+0x470,4,raw],[C+0x474,4,raw],[O+0x10,4,raw]],ops=[[50,C]])
    for active in (0,0x8000000000000000):
        for nearby in (0,1):
            add('zek_activity_gates_wave',[[C+0xa8,8,active],[0x403a08,1,nearby],
                [C+0x464,4,1],[C+0x46c,4,0x43000000]],state=0)
    for raw in (0,0x80000000,1,0x41200000,0xbf800000,0x7f800000,0x7fc12345):
        add('zek_level',[[0x4f714c,4,raw]],ops=[[49,C]])
    for body,args in ((0x2c2440,[]),(0x2c382c,[[1,6]]),(0x2c34e4,[[1,O],[1,0x2000],[1,0x3f800000]]),
                      (0x2c3c54,[]),(0x116638,[[1,O],[1,0],[1,0x3f800000]])):
        for slot in (2,34,35):
            for adjust in (0,0xfffe,0x8000):
                for override in (False,True):
                    add('zek_virtual_body',[[T+8*slot,2,adjust],[T+8*slot+4,4,body]],ops=[[51,C,slot,args]],external=[slot] if override else [])
    for type_name in (0,0x12345678):
        for facing in (0,0x8001,0x1234ffff):
            for raw in (0,0x80000000,0x7fc12345):
                cb=copy.deepcopy(base[1]);cb[22][2]=[[C+0x24,4,D]]
                add('zek_constructor',[[0x503688+off,4,0xd0000+off*256] for off in range(0x1c,0xdc,8)],
                    cb,ops=[[45,C,type_name,[raw,raw,raw],facing,0]])
    for seed in (0,1,0xffffffff):
        for state in (0,5,8,13,14,16,20):
            cb=callbacks_for('anim_advance',[[1,1],[1,2],[1,0]])
            add('zek_repeated_ticks',[[C+0x234,4,B],[C+0x318,2,3],[C+0x31c,2,3]],cb,ops=[[46,C]]*3,state=state,seed=seed)
    if ref is not None:
        observed={}
        for label,inputs in out:
            observation=execute(ref,0,inputs)
            for name,_args in observation[1][-1][2][3]:observed.setdefault(name,inputs)
        for name,original in observed.items():
            for fail in (-1,0,1):
                inputs=copy.deepcopy(original);ident=NAMES.index(name)
                inputs[1][ident][2]+=[[C+0x50,4,0],[C+0x38,2,0x8000],[C+0x300,4,0x80000000],
                    [C+0xd0,2,0x8001],[C+0x460,4,B]]
                inputs[1][ident][3]=fail;out.append(('zek_service_prefix',inputs))
        # Late particle failures and rereads after prior successful calls.
        for fail in (0,1,24,49,50):
            cb=copy.deepcopy(base[1]);cb[NAMES.index('particle_spawn')][3]=fail
            cb[NAMES.index('particle_spawn')][2]=[[C+0x228,4,0x41200000]]
            add('zek_spark_late_failure',[],cb,ops=[[48,C,O,0x2000,0x3f800000]])
    return out

def vcall_recipes(ref=None):
    """Final dispatcher: base answers, every recognized body, and typed returns."""
    out=[]
    base=horsemen_recipes()[0][1]
    def add(label,slot,body,args=(),extra=(),cb=None,adjust=0,override=False):
        pokes=copy.deepcopy(base[0])+[[T+slot*8,2,adjust],[T+slot*8+4,4,body]]+list(extra)
        out.append((label,[pokes,copy.deepcopy(base[1]) if cb is None else cb,[slot] if override else [],[[52,C,slot,list(args)]]]))
    for found in (0,1,0xffffffff):
        for failures in (0,1,0x7fffffff,0xffffffff):
            for adjust in (0,2,0xfffe,0x8000):
                add('path_answer',7,0x11a260,[[1,found]],[[C+(adjust if adjust<0x8000 else adjust-0x10000)+0x2c4,4,failures]],adjust=adjust)
    for frames in (0,1,0xffff,0x10000,0x80000000,0xffffffff):
        for rank in (0,1,2,0xffffffff):
            for state in (0,6,20,0x8000):
                for flash in (0,1):
                    cb=copy.deepcopy(base[1]);cb[NAMES.index('creature_bossRank')][1]=[[1,rank]]
                    add('stun',30,0x118a58,[[1,frames],[1,flash]],[[C+0x232,2,state],[T+27*8+4,4,0x3c2688]],cb)
            for pvp in (0,1):
                cb=copy.deepcopy(base[1]);cb[NAMES.index('creature_bossRank')][1]=[[1,rank]]
                add('no_target',31,0x118af8,[[1,frames]],[[0x4f711c,4,pvp],[C+0x234,4,B]],cb)
    ordinary=[(0x2ab5d8,[]),(0x267654,[]),(0x297b30,[]),(0x2a5068,[]),(0x244380,[]),
       (0x2ab704,[[1,5]]),(0x267a0c,[[1,5]]),(0x297f54,[[1,5]]),(0x2a55ec,[[1,5]]),(0x244aa4,[[1,5]]),
       (0x2ab8e0,[[1,O]]),(0x244a9c,[]),(0x3c2738,[]),(0x3c2708,[[1,65535]]),(0x10beb8,[[1,1]]),(0x10e658,[[1,0]]),
       (0x2a8b84,[]),(0x2a91b8,[[1,5]]),(0x27236c,[]),(0x272f4c,[[1,5]]),(0x2c2440,[]),(0x2c382c,[[1,5]]),
       (0x2c34e4,[[1,0],[1,0],[1,0]]),(0x2c3c54,[])]
    if ref is not None:
        if {body for body,_args in ordinary} != set(ref.CLASS_BODIES) | set(ref.AM.CLASS_BODIES):
            raise ValueError('derived virtual body inventory changed')
    for body,args in ordinary:
        for slot in (4,27,35):
            for adjust in ((0,) if body in (0x2ab5d8,0x267654,0x297b30,0x2a5068,0x244380,0x2a8b84,0x27236c,0x2c2440) else (0,0xfffe,0x8000)):
                for override in ((False,) if slot==4 else (False,True)):add('derived_lookup',slot,body,args,extra=[[C+(adjust if adjust<0x8000 else adjust-0x10000)+0x232,2,19],[C+(adjust if adjust<0x8000 else adjust-0x10000)+0x31c,2,32767]],adjust=adjust,override=override)
    bases=[(2,0x116638,[[1,0],[1,0],[1,0]]),(6,0x11a418,[[1,O]]),(7,0x11a260,[[1,0]]),
       (10,0x11a3f0,[]),(11,0x11a448,[]),(17,0x11a868,[[2,0,0,0]]),(20,0x119328,[[1,O]]),
       (23,0x10ded0,[[1,0]]),(26,0x10bb08,[[1,0],[2,0,0,0]]),(27,0x3c2688,[[1,5]]),
       (30,0x118a58,[[1,1],[1,1]]),(31,0x118af8,[[1,1]]),(35,0x118c50,[]),(36,0x118c78,[]),(37,0x3c2730,[])]
    if ref is not None:
        if {(slot,body) for slot,body,_args in bases} != {(slot,pair[0]) for slot,pair in ref.RC.BASE_BODIES.items()}:
            raise ValueError('base virtual body inventory changed')
    for slot,body,args in bases:
        for adjust in (0,0xfffe,0x8000):
            for override in (False,True):add('base_lookup',slot,body,args,adjust=adjust,override=override)
        # Base bodies are slot keyed; an unexpected slot delegates.
        add('base_wrong_slot',35 if slot!=35 else 34,body,args)
    for slot in (2,6,7,10,11,16,17,19,20,23,25,27,28,30,31,34,35,36,37):
        ident=NAMES.index('Creature.v'+str(slot))
        if ident<33:continue
        for value in ([0],[1,0],[1,-1],[1,0xffffffff],[2,1,2,3],[3],[3,[1,7]],[4,[0],[1,-5]],[5,65,0,255]):
            for adjust in (0,0xfffe):
                cb=copy.deepcopy(base[1]);cb[ident][1]=[value]
                add('typed_fallback',slot,0x12345678,[[1,0xffffffff],[4,[1,-1],[0]]],cb=cb,adjust=adjust)
    # Runs forward final set-state returns, including None and compound values.
    for body in (0x2ab5d8,0x267654,0x297b30,0x2a5068,0x244380,0x2a8b84,0x27236c):
        for value in ([0],[1,0],[4,[1,7],[0]],[3]):
            cb=copy.deepcopy(base[1]);cb[NAMES.index('Creature.v27')][1]=[value]
            add('run_return',4,body,extra=[[C+0x232,2,19],[C+0x31c,2,0],[T+27*8+4,4,0x12345678]],cb=cb)
    # Exercise existing lifecycle/path/cast bodies through the complete owner domain.
    mapping={23:0x2ab5d8,24:0x267654,25:0x297b30,26:0x2a5068,27:0x244380,
             29:0x2a8b84,40:0x27236c,46:0x2c2440}
    for recipes_for in (run_recipes,skelight_recipes,horsemen_recipes,zek_recipes):
        candidates=[i for _label,i in recipes_for() if len(i[3])==1 and i[3][0][0] in mapping]
        for i in candidates[::max(1,len(candidates)//80)]:
            i=copy.deepcopy(i);op=i[3][0];body=mapping[op[0]]
            i[0].append([T+4*8+4,4,body]);i[3]=[[52,op[1],4,[]]];out.append(('full_lifecycle_domain',i))
    if ref is not None:
        # Complete mutation and failure prefixes on the three new base bodies.
        for label,inputs in list(out):
            if label not in ('stun','no_target'):continue
            if inputs[3][0][3][0]!=[1,1]:continue
            for name in ('creature_bossRank','handle_clear','Creature.v27'):
                cb=copy.deepcopy(inputs);ident=NAMES.index(name)
                cb[1][ident][2]+=[[C+0x232,2,20],[C+0x234,4,B],[C+0x90,2,0x1234]]
                cb[1][ident][3]=0;out.append(('base_failure_prefix',cb))
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
    model=Trace(memory,external=[('Skill.v'+str(i-100) if i>=100 else 'Creature.v'+str(i)) for i in inputs[2]])
    for ident,returns,mutations,fail in inputs[1]:
        def cb(*args,ident=ident,returns=returns,mutations=mutations,fail=fail):
            index=counts[ident];counts[ident]+=1
            for row in mutations:memory.store(*row)
            if index==fail:raise ValueError('authored callback failure')
            value=returns[min(index,len(returns)-1)]
            return decode_structured(value) if ident>=33 or (ident in (4,5) and isinstance(value,list)) else value
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
            if op[0]==21:
                _,ident,argv,ret,fret,outs=op
                args=tuple(decode_structured(a) for a in argv)
                given=None if outs==0 else [decode_structured(a)[1] if a[0]==5 else decode_structured(a) for a in outs]
                result=ref.shape_result(NAMES[ident],args,ret,fret,given)
            elif 23<=op[0]<=27:
                result=(model.smallspider_run,model.gnaw_run,model.mosquito_run,model.scorpion_run,model.badger_run)[op[0]-23](op[1])
            elif op[0]==28:
                result=model.skelight_ctor(op[1],tuple(op[2]),*op[3:])
            elif op[0]==29:
                result=model.skelight_run(op[1])
            elif op[0]==30:
                result=model.skelight_set_state(*op[1:])
            elif op[0]==31:
                result=model.skelight_shatter(*op[1:])
            elif op[0]==32:
                result=model.vcall(op[1],op[2],*(decode_structured(a) for a in op[3]))
            elif op[0]==33:
                result=model.summon_can_use(*op[1:])
            elif op[0] in (34,35):
                result=(model.summon_cast if op[0]==34 else model.tagar_cast)(*op[1:4],tuple(op[4]),op[5])
            elif op[0] in (36,37,38):
                result=(model.spell_base_ctor,model.summon_spell_ctor,model.tagar_spell_ctor)[op[0]-36](*op[1:])
            elif op[0]==39:
                result=model.skill_call(op[1],op[2],*(decode_structured(a) for a in op[3]))
            elif op[0]==40:
                result=model.horsemen_run(op[1])
            elif op[0]==41:
                result=model.horsemen_set_state(*op[1:])
            elif op[0]==42:
                result=model._horsemen_cast(op[1])
            elif op[0]==43:
                result=model.vcall(op[1],op[2],*(decode_structured(a) for a in op[3]))
            elif op[0]==44:
                result=model.horsemen_ctor(op[1],op[2],tuple(op[3]),*op[4:])
            elif op[0]==45:
                result=model.zek_ctor(op[1],op[2],tuple(op[3]),*op[4:])
            elif op[0]==46:
                result=model.zek_run(op[1])
            elif op[0]==47:
                result=model.zek_set_state(*op[1:])
            elif op[0]==48:
                result=model.zek_take_damage(*op[1:])
            elif op[0]==49:
                result=model.zek_slot35(op[1])
            elif op[0]==50:
                result=model._zek_wave(op[1])
            elif op[0]==51:
                result=model.vcall(op[1],op[2],*(decode_structured(a) for a in op[3]))
                if result is not None:result &= (1<<64)-1
            elif op[0]==52:
                result=model.vcall(op[1],op[2],*(decode_structured(a) for a in op[3]))
                if isinstance(result,int):result &= (1<<64)-1
            elif op[0]==22:
                result=model.call(NAMES[op[1]],*(decode_structured(a) for a in op[2]))
            elif op[0]!=5:
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
        families=(recipes()+state_recipes()+constructor_recipes()+attack_recipes()+chase_recipes()+
                  abi_recipes(ref)+run_recipes(ref)+skelight_recipes(ref)+arena_cast_recipes(ref)+horsemen_recipes(ref)+zek_recipes(ref)+vcall_recipes(ref))
        for config in range(16):
            for label,inputs in families:
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
