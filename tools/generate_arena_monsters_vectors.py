#!/usr/bin/env python3
"""Synthetic arena base services and Skelight states from immutable reference sources."""
import argparse
import ast
import hashlib
import importlib
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'docs/spec/arena-monsters.md': 'd9a1a7b81b79c293b64ce5911112456ddd177df15fb8d0ffa0c09230887cd87b',
    'tools/difftest_arena_monsters.py': 'd342f5ca8b2fa7edde61c05350481900fa758c80d8a4df300da63ae8804dc911',
}
# Count only units whose entire behavior is implemented and covered. The inherited
# filter_state is exercised but does not belong to this pinned file. Dispatch
# adapters and the partially supported Skelight_set_state are not counted.
PORTED_UNITS = [
    'Model.'+name for name in (
        'is_not_rooted','is_attack_ready','is_not_stunned','slot16_model',
        'slot34_set_home_range','turn_to_target','can_see_target_in',
        'is_target_charging','count_allies_near_target','is_blocked_by_target',
        'slot25_can_see_target','slot28_on_death','set_def_entry_params','creature_ctor',
        'skelight_shatter','spell_rand','_unit','_spell_ball','spell_base_ctor',
        'tagar_cast','tagar_spell_ctor','tagar_is_valid','tagar_pick_target',
        'tagar_tick_victims','tagar_spell_run','cosmetic_draw','_cosmetic_ball',
        'summon_can_use','summon_cast','_summon_sample','summon_spell_ctor',
        'summon_spell_run','skelight_ctor','holder_call','cstr','class_model','clip',
        'weapon_descriptor','_prop_int','_init_holder')
] + ['scaled_time','half_toward_zero','third_toward_zero'] + ['ArenaScript.'+name for name in (
    '__init__','defer','frame','start','incoming','nosave','setFlag','knight_death',
    'knightDefeatedPause','knightDefeatedDelay','knightDefeated','avatar_death',
    'bossDefeatedDelay','endmission','levelChange')]

def reference_units(source):
    units=[]
    for node in ast.parse(source).body:
        if isinstance(node,ast.FunctionDef): units.append(node.name)
        elif isinstance(node,ast.ClassDef):
            units.extend(node.name+'.'+m.name for m in node.body if isinstance(m,ast.FunctionDef))
    # This contract file contains no test entry points or harness functions.
    # shape_result is its callback result marshaller, part of the model API.
    if len(units)!=78 or len(set(PORTED_UNITS))!=58 or not set(PORTED_UNITS)<=set(units):
        raise ValueError('reference unit accounting changed')
    return units

C, D, T = 0x10000, 0x20000, 0x30000
MASK = (1 << 64)-1

def packed(rows):
    return ';'.join(','.join(str(x & MASK) for x in row) for row in rows) or '-'

def value(v):
    if isinstance(v, int): return 'i'+str(v)
    if isinstance(v, str): return 's'+v
    return 'v'+','.join(map(str, v))

def calltext(name, args):
    return 'c'+name+'('+ ';'.join(map(value,args))+')'

def writetext(w):
    return 'w('+','.join(map(str,w))+')'

def snapshot(m, events, results):
    return '|'.join([','.join(f'{a}:{b}' for a,b in sorted(m.mem.bytes.items())),
                     ';'.join(map(writetext,m.mem.log)), ';'.join(map(writetext,m.writes)),
                     ';'.join(calltext(n,a) for n,a in m.calls), ';'.join(events),
                     str(m.draws), ';'.join(results)])

SCRIPT_KEYS = ('sawknights','sawcaz','killedAv','endsound','firstmissionever',
               'knight1','knight2','knight3','knight4','extra')
SCRIPT_NAMES = ('start','incoming','nosave','setFlag','knightDefeatedPause',
                'knightDefeatedDelay','knightDefeated','avatar_death','bossDefeatedDelay',
                'endmission','levelChange','__init__')

def script_snapshot(m):
    def arg(v): return 'i'+str(v) if isinstance(v,int) else 's'+v
    logs = ';'.join(n+'('+ ';'.join(arg(v) for v in a)+')' for n,*a in m.log)
    return '|'.join([','.join(k+':'+str(v) for k,v in sorted(m.q.items())),
        ','.join(str(k)+':'+str(v) for k,v in sorted(m.g.items())),
        ','.join(m.front), ','.join(m.back), str(m.paused), logs])

def script_vectors(R):
    cases=[]
    # Every entry, seeded quests, repeated idempotent actions, and arbitrary knight index.
    for mask in range(32):
        init=[[k, -3 if mask & (1<<k) else 0] for k in range(5)]+[[9,17]]
        cases.append((init,[[102,0],[103,0],[103,0],[104,0],[105,0],
            [106,1],[106,2],[106,3],[106,4],[107,0],[108,0],[109,0],
            [110,0],[111,0],[112,0],[112,0],[113,0]]))
    for index in (-1,0,1,2,3,4,5,9223372036854775807):
        cases.append(([],[[106,index],[107,0],[101,0,2]]))
    # Thresholds including repeated queued copies and the promotion frame.
    for frames in (0,1,251,252,253,254,876,877,878,879,1000):
        cases.append(([],[[110,0],[101,0,frames],[101,1,2],[113,0]]))
    for frames in (0,1,300,301,302,303,305):
        cases.append(([],[[106,k] for k in (1,2,3,4)]+[[101,0,frames],[101,1,2]]))
    for g0 in (249,250,251,252,1499,1500,1501,1502,-1,0x7fffffff,0x80000000,0xffffffff):
        for paused in (0,1,-1):
            cases.append(([[10,g0],[11,g0]],[[101,paused,1],[108,0],[111,0],[111,0]]))
    # Queue saturation, duplicates, queue promotion and discarded name 65.
    for n in (0,1,63,64,65,70):
        cases.append(([],[[100,3]]*n+[[100,0],[101,0,1],[101,-7,1],[101,0,1]]))
        cases.append(([],[[100,8]]*n+[[101,0,1],[101,0,1],[101,1,1]]))
    for name in range(len(SCRIPT_NAMES)):
        cases.append(([],[[100,name],[101,0,1],[101,-1,1],[101,0,1]]))
    cases.append(([[2,1],[9,7]],[[100,0],[100,11],[100,10],[101,0,1],[101,-1,1]]))
    for seed in (0,1):
        cases.append(([[3,seed]],[[110,0],[101,1,400],[101,0,252],
                                      [101,1,40],[101,0,626],[101,1,2]]))
    for count in list(range(-5,5))+[250,251,252,300,301,302,1500,1501,1502,
                                    0x7fffffff,0x80000000,0xffffffff,0x100000000,
                                    -9223372036854775808,9223372036854775807]:
        cases.append(([],[[114,count]]))
    for init,commands in cases:
        quest={SCRIPT_KEYS[k]:v for k,v in init if k<10}
        m=R.ArenaScript(quest)
        for k,v in init:
            if k>=10: m.g[(k-10)*4]=v
        observations=[]
        for cmd in commands:
            op,arg=cmd[:2]
            result='ok'
            if op==100: m.defer(SCRIPT_NAMES[arg])
            elif op==101:
                for _ in range(cmd[2]): m.frame(arg)
            elif op==106: m.knight_death(arg)
            elif op==114: result='r'+str(R.scaled_time(arg))
            else: getattr(m,{102:'start',103:'incoming',104:'nosave',105:'setFlag',
                107:'knightDefeatedPause',108:'knightDefeatedDelay',109:'knightDefeated',
                110:'avatar_death',111:'bossDefeatedDelay',112:'endmission',113:'levelChange'}[op])()
            observations.append(result+'~'+script_snapshot(m))
        yield init,commands,'#'.join(observations)


def tagar_vectors(R):
    S,O,K,V,W,N,M = 0x60000,0x61000,0x62000,0x63000,0x64000,0x65000,0x66000
    base=[[S,1,255],[S+8,4,K],[S+12,4,O],[S+16,4,0x12345678],[S+20,4,R.VT_SPELL_TAGAR],
          [S+0x7c,4,0x4000101f],[S+0x80,2,0xffff],[O+0x48,4,0x67000],[K+0x60,4,0x68000],
          [V+0x10,4,0x41200000],[W+0x10,4,0xc1200000],[V+0x1c,4,0x41400000],
          [V+0x20,4,0x42000000],[W+0x20,4,0x42000000],[0x404464,4,0x89abcdef],
          [R.G_SIM_FRAME,4,0xfffffff0],[N+12,4,V],[N+0x20+12,4,W]]
    cases=[]
    # Fully cover seed, raw32 draw boundaries and signed-byte constructor levels.
    for seed in (0,1,0xffffffff,0x80000000,0x12345678,0x89abcdef):
        cases.append(([[S+16,4,seed]],[[200,0],[200,0],[201,0]],0,0))
    for draw in list(range(0,65536,257))+[32767,32768,65535,0x7fffffff,0x80000000,0xffffffff]:
        cases.append(([],[[209,draw]],0,0))
    for level in (0,1,127,128,255,256,0xffffffff):
        for heading in (0,1,0x3fff,0x4000,0x7fff,0x8000,0xffff):
            cases.append(([[O+0x38,2,heading],[O+0x10,4,0x3f800001],
                           [O+0x14,4,0xbf800000],[O+0x18,4,0x7f800000]],[[202,level]],0,0))
    cases.append(([],[[210,0x1ff,O,K,0x12345678],[210,2,0,0,R.VT_SPELL_TAGAR]],0,0))
    for scenario in (0,1,2,3):
        cases.append(([],[[203,255]],scenario,0))
        for fail in range(1,6): cases.append(([],[[203,255]],scenario,fail))
    for flags in (0,1,2,8,9,0xffffffff):
        for scenario in (0,4,8):
            for marker in (0,1,0xffffffff):
                cases.append(([[V+12,4,flags]],[[204,V,marker],[204,0,marker]],scenario,0))
    for later in (0,1,0xffffffff):
        for scenario in (0,4,8,16,32):
            for count in (0,1,2,0xffff):
                cases.append(([[S+0x82,2,count],[S+0x18,4,N],[S+0x1c,4,N+0x20]],[[205,later]],scenario,0))
        for fail in range(1,9): cases.append(([],[[205,later]],0,fail))
    # nearest strict tie, invalid/marked entries, distance cap and signed adjustments.
    for coord in (0,0x3f800000,0x447a0000,0x7fffffff,0x80000000):
        cases.append(([[V+0x10,4,coord],[W+0x10,4,coord]],[[205,1]],0,0))
    for adjust in (0,4,0xfffc,0x8000,0x7fff):
        cases.append(([[0x67000+20*8,2,adjust],[0x68000+14*8,2,adjust]],[[204,V,1],[203,1]],0,0))
    victim=[[S+0x82,2,1],[S+0x18,4,N],[S+0x38,4,0x41200000]]
    for expiry in (0,0xbf800000,0x3f800000,0x41200000,0x7fffffff):
        for tick in (0,0x3f800000,0xbf800000):
            for scenario in (0,4,8,64):
                cases.append((victim+[[S+0x38,4,expiry],[S+0x58,4,tick],[S+0x78,4,0x3f800000],
                                     [O+12,4,0x40 if scenario&64 else 0]],[[206,0]],scenario,0))
    for fail in range(1,11): cases.append((victim,[[206,0]],0,fail))
    for scenario in (0,4,8,16,32,128,256,512,1024,2048,4096,8192):
        cases.append(([],[[207,0]],scenario,0))
    # Flight direction reflection, arrival (including zero-distance division),
    # current handle loss, all eight victims, and old-clock expiry/limit exits.
    for heading in (0,0x3ffe,0x3fff,0x4000,0x7fff,0x8000,0xc000,0xffff):
        for xy in ((0,0),(0x3f800000,0),(0x42c80000,0),(0,0x42c80000),(0xc2c80000,0),(0,0xc2c80000)):
            cases.append(([[S+0x84,2,heading],[V+0x10,4,xy[0]],[V+0x14,4,xy[1]]],[[207,0]],0,0))
    for current in (0,1,7,8,0x7fff):
        cases.append(([[S+0x80,2,current],[S+0x18+4*current,4,N],[S+0x38+4*current,4,0x41200000]],[[207,0]],0,0))
    for count in (0,1,7,8,9,0xffff,0x8000):
        for scenario in (0,128,256):
            cases.append(([[S+0x82,2,count]],[[207,0]],scenario,0))
    for end in (0,0xbf800000,0x3c888887,0x3c888888,0x3c888889):
        cases.append(([[S+0x7c,4,end]],[[207,0]],0,0))
        cases.append((victim+[[S+0x7c,4,end]],[[207,0]],0,0))
    for fail in range(1,61): cases.append(([],[[207,0]],0,fail))
    cases.append(([],[[202,2],[207,0],[207,0],[207,0],[206,0]],0,0))
    cases.append(([[V+0x10,4,0]],[[207,0],[207,0],[207,0]],0,0))
    # Mutation at each call forces re-reads of owner, clock, count, heading and
    # victim dimensions. Changes are supplied memory stores, never inferred.
    for scenario in (512,1024,2048,4096,8192):
        cases.append((victim+[[O+12,4,0x40]],[[206,0],[207,0]],scenario,0))
    for extra,commands,scenario,fail in cases:
        pokes=base+extra
        mem=R.RC.Memory()
        for row in pokes:mem.poke(*row)
        m=R.Model(mem);events=[];results=[];calls=0
        original=m._w
        def write(addr,size,v):
            events.append(writetext((addr,size,v & ((1<<(8*size))-1))))
            original(addr,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def callback(name):
            def fn(*args):
                nonlocal calls
                calls+=1
                if fail and calls==fail:raise RuntimeError('synthetic callback failure')
                if scenario&512:mem.store(S+0x78,4,0x3f000000)
                if scenario&1024:mem.store(S+0xc,4,O+0x100)
                if scenario&2048:mem.store(S+0x82,2,0)
                if scenario&4096:mem.store(S+0x84,2,0x8765);mem.store(R.G_SIM_FRAME,4,calls)
                if scenario&8192:mem.store(V+0x1c,4,0x41a00000);mem.store(V+0x20,4,0x42a00000)
                if name=='Skill.v14':return args[1]+1
                if name=='Skill.v12':return (0 if scenario&128 else -1 if scenario&256 else 8) if args[1]==0 else (100 if args[1]==1 else 3)
                if name=='Skill.v8':return (-3,0x7fffffff)
                if name=='res_find':return 0x77700
                if name=='pool_new':return 0 if scenario&1 else S if args[0]==0x94 else M
                if name=='Creature.v20':return 0 if scenario&4 else -3
                if name=='creature_findSpell':return M if scenario&8 else 0
                if name.startswith('world_collect'):
                    return [] if scenario&16 else [W,V] if scenario&32 else [0,V,W]
                if name=='handle_link':
                    mem.store(args[0],4,N);mem.store(N+12,4,args[1])
                return 0
            return fn
        for name in ('Skill.v14','Skill.v12','Skill.v8','Creature.v20','Spell.v1','res_find',
                     'sound_playAtPos','pool_new','creature_findSpell','world_collectInSphere',
                     'world_collectObjectsInArc','handle_clear','handle_link','damage_apply','particle_spawn','creature_addSpell'):
            m.cb[name]=callback(name)
        for cmd in commands:
            op,arg=cmd[:2]
            try:
                if op==200:results.append('r'+str(m.spell_rand(S)))
                elif op==201:results.append(value(m._spell_ball(S)))
                elif op==202:m.tagar_spell_ctor(S,arg,O,K);results.append('ok')
                elif op==203:results.append('r'+str(m.tagar_cast(K,arg,O,(0x3f800000,0x80000000,0x7fc12345),0xffff)))
                elif op==204:results.append('r'+str(m.tagar_is_valid(S,arg,cmd[2])))
                elif op==205:results.append('r'+str(m.tagar_pick_target(S,arg)))
                elif op==206:results.append('r'+str(m.tagar_tick_victims(S)))
                elif op==207:m.tagar_spell_run(S);results.append('ok')
                elif op==209:results.append('r'+str(m._unit(arg)))
                elif op==210:m.spell_base_ctor(S,*cmd[1:]);results.append('ok')
                else:raise ValueError('tagar operation')
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,[scenario,fail],snapshot(m,events,results)

def summon_vectors(R):
    S,O,K,N,B=0x60000,0x61000,0x62000,0x65000,0x70000
    base=[[O+0x48,4,0x67000],[0x67000+132,4,0x3c2738],[O+0x24,4,0x68000],
        [0x6800c,4,0x77700],[O+0x10,4,0x3f800000],[O+0x14,4,0x80000000],
        [O+0x18,4,0],[O+0x38,2,0x8123],[O+0x310,1,255],
        [K+0x60,4,0x69000],[0x404464,4,0x12345678],[0x404460,4,0x87654321],
        [R.G_LEVEL_MONSTER_LEVEL,4,0x41200000],[R.G_SIM_FRAME,4,0xfffffff0],
        [R.DEF_SKELIGHT+0xc,4,0x77700],[R.DEF_SKELIGHT+0x1c,4,0x71000],
        [R.DEF_SKELIGHT+0x24,4,0x72000],[R.VT_SKELIGHT+34*8+4,4,0x3c2708],
        [0x67000+37*8+4,4,0x3c2730],[R.G_SUMMON_MONSTER_NAME,4,0x78000]]
    for k in range(4):base.append([R.G_SUMMON_PLAYER_NAMES+4*k,4,0x78100+k])
    # Authored bytes, not retail names. Mixed ASCII case and early NULs.
    for a,t in ((R.G_LEVEL_NAME,b'SyntHETic'),(R.S_BLOMEDAL,b'synthetic')):
        base += [[a+k,1,v] for k,v in enumerate(t+b'\0')]
    cases=[]
    for flags in (0,0x40,0xffffffff):
        for slot in (0,1,2,3,4,0xffffffff):
            for found in (0,1,0xffffffff):
                cases.append(([[O+12,4,flags],[O+0x3c8,4,slot]],[[300,0]],found<<16,0))
    for seed in (0,1,0xffffffff,0x87654321,0x80000000):
        cases.append(([[0x404460,4,seed]],[[301,0],[302,0],[301,0]],0,0))
    for count in (-2147483648,-7,-1,1,7,2147483647):
        for adj in (0,4,0xfffc,0x8000,0x7fff):
            signed=adj if adj<0x8000 else adj-0x10000
            cases.append(([[0x67000+128,2,adj],[O+signed+0x24,4,0x68000]],[[303,count]],0,0))
    for level in (0,1,255,256,0xffffffff,0x80000000):
        for skeleton in (0,B):
            cases.append(([],[[304,level,skeleton],[305,0]],0,0))
    cases.append(([],[[305,0]],0,0));cases.append(([[S+0x18,4,N]],[[305,0]],0,0))
    for scenario in (0,1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192):
        for level in (0,1,0xffffffff):
            cases.append(([[O+12,4,0x40 if scenario&4 else 0],[O+0x3c8,4,scenario%6]],[[306,level]],scenario,0))
    # Count-positive and signed-negative sampling; all 232 particles in full cases.
    for count in (0,1,-1,-2147483648,7):
        cases.append(([],[[306,2]],32 if count else 0,0)) if count in (0,7) else cases.append(([],[[306,2]],(count&0xffffffff)<<32,0))
    # Terrain float comparison at strict +/-24 boundaries, infinity and NaNs.
    for z in (0,0x41bfffff,0x41c00000,0x41c00001,0xc1c00000,0xc1c00001,0x7f800000,0xff800000,0x7fc12345):
        for found in (0,1,128,256):
            cases.append(([],[[306,1]],(z<<64)|(found<<96)|8|16384|32768,0))
    # Shared draw choices and strict elemental chance boundaries; facing narrowing.
    for seed in range(32):
        for chance in (0,1,50,100,-1):
            cases.append(([[0x404464,4,seed]],[[306,1]],(chance&0xffffffff)<<128|65536,0))
    for facing in (0,0x3fff,0x4000,0x7fff,0x8000,0xffff,0x10000,0xffffffff):
        cases.append(([],[[306,1,facing]],0,0))
    # Named strings compare as bounded byte strings; bytes.lower is ASCII only.
    for text in (b'',b'SYNTHETIC',b'different',b'synthetic\0ignored',bytes([0xc0])+b'synthetic',b'A'*64):
        extra=[[R.G_LEVEL_NAME+k,1,v] for k,v in enumerate(text+b'\0')]
        cases.append((extra,[[307,3]],0,0))
    for adjustment in (4,0xfffc,0x8000,0x7fff):
        cases.append(([[R.VT_WEAPON1+8,2,adjustment],[0x69000+12*8,2,adjustment],
                       [0x67000+37*8,2,adjustment],[R.VT_SKELIGHT+34*8,2,adjustment]],
                      [[306,1]],4,0))
    for failure in range(1,75):cases.append(([[O+12,4,0x40]],[[306,1]],4,failure))
    for failure in (1,2,3,4,10,35,36,65,66,200,233,234,235,250,280,300):cases.append(([],[[306,1]],32,failure))
    # Standalone constructor, timer filtering, truncation, case-sensitive mismatch,
    # callbacks that change fields, and reuse with retained globals/serials.
    for scenario in (0,64,128,256,512,1024,2048,4096,8192):
        for level in (0,1,0xffffffff,0x80000000):
            cases.append(([[B+0x314,2,1] if scenario&128 else [B+0x31c,2,1] if scenario&256 else [B+0x318,2,1]],[[307,level]],scenario,0))
    for failure in range(1,38):cases.append(([],[[307,0xffffffff]],0,failure))
    cases.append(([],[[307,1],[307,2],[306,2],[305,0]],4,0))
    for extra,commands,scenario,fail in cases:
        # Options have fixed-width words; no precision-dependent text parsing.
        opts=[scenario&0xffffffff,(scenario>>32)&0xffffffff,(scenario>>64)&0xffffffff,(scenario>>96)&0xffffffff,fail,(scenario>>128)&0xffffffff]
        pokes=base+extra
        mem=R.RC.Memory()
        for row in pokes:mem.poke(*row)
        m=R.Model(mem);events=[];results=[];calls=0
        original=m._w
        def write(addr,size,v):
            events.append(writetext((addr,size,v&((1<<(8*size))-1))));original(addr,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def callback(name):
            def fn(*args):
                nonlocal calls
                calls+=1
                if fail and calls==fail:raise RuntimeError('synthetic callback failure')
                if scenario&512:mem.store(O+0x38,2,calls+0x8123)
                if scenario&1024:mem.store(R.G_SIM_FRAME,4,calls)
                if scenario&2048:mem.store(B+0x50,4,0)
                if scenario&4096:mem.store(O+12,4,0)
                if scenario&8192:mem.store(R.G_LEVEL_NAME,1,ord('!'))
                if name=='world_findObjectByName':return (scenario>>16)&0xffffffff
                if name=='GameObject_ctor':
                    mem.store(args[0]+0x24,4,args[2])
                    for k,v in enumerate(args[1]):mem.store(args[0]+0x10+4*k,4,v)
                if name=='model_getSamplePointCount':return R.s32(opts[1]) if opts[1] else (7 if scenario&32 else 0)
                if name=='model_getSamplePoint':return (0x3f800000,0x80000000,0x7fc12345)
                if name=='terrain_height':return opts[2] if scenario&16384 else (0x42000000 if scenario&8 else 0)
                if name=='world_findFreeSpot':return ((0x3f800000,0x40000000,0x40400000),opts[3] if scenario&32768 else (0 if scenario&16 else 1))
                if name=='Skill.v12':return (-1 if scenario&64 else 3) if args[1]==0 else (R.s32(opts[5]) if scenario&65536 else 0 if scenario&128 else -1 if scenario&256 else 100)
                if name=='pool_new':return (0 if scenario&1 else B) if args[0]==0x468 else (0 if scenario&2 else S)
                if name=='model_buildHideMask':return 0xfedcba9876543210
                if name=='res_find':return 0x77700
                if name=='handle_link':mem.store(args[0],4,N);mem.store(N+12,4,args[1])
                return 0
            return fn
        for name in ('world_findObjectByName','GameObject_ctor','obj_getPropInt','obj_getPropStr',
            'model_getSamplePointCount','model_getSamplePoint','particle_spawn','terrain_height','world_findFreeSpot',
            'Skill.v12','pool_new','Holder.v1','Holder.v2','Holder.v4','Holder.v6','Creature_setStats',
            'model_buildHideMask','anim_clear','anim_push','anim_start','anim_syncToSimFrame','handle_clear',
            'obj_runEveryFrame','Creature_init','Creature_setCharm','res_find','sound_playAtPos','handle_link','Spell.v1'):
            m.cb[name]=callback(name)
        for cmd in commands:
            op,arg=cmd[:2]
            try:
                if op==300:results.append('r'+str(m.summon_can_use(K,O)))
                elif op==301:results.append('r'+str(m.cosmetic_draw()))
                elif op==302:results.append(value(m._cosmetic_ball()))
                elif op==303:results.append(value(m._summon_sample(O,R.s32(arg))))
                elif op==304:m.summon_spell_ctor(S,arg,O,K,cmd[2]);results.append('ok')
                elif op==305:m.summon_spell_run(S);results.append('ok')
                elif op==306:results.append('r'+str(m.summon_cast(K,arg,O,(0x3f800000,0x80000000,0x7fc12345),cmd[2] if len(cmd)>2 else 0xffff)))
                elif op==307:m.skelight_ctor(B,(0x3f800000,0x80000000,0x7fc12345),0xffff,0x78000,arg);results.append('ok')
                else:raise ValueError('summon op')
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,opts,snapshot(m,events,results)


def helper_vectors(R):
    # Full signed-16 domain, including upper bits ignored by each scaling helper.
    cases=[]
    for start in range(0,65536,128):
        cases.append(([], [[op,v | 0xabcd0000] for v in range(start,start+128) for op in (407,408)], [0,0,0]))
    for blob in (b'',b'A\0ignored',bytes(range(1,256)),b'\xff\x80ByteString'):
        for limit in (0,1,3,64,256):
            cases.append(([[0x40000+k,1,v] for k,v in enumerate(blob)],[[400,0x40000,limit]], [0,0,0]))
    for ptr in (0,D):
        for offset in (0,12,0x1c,0x94,0x40000):
            cases.append(([[C+0x24,4,ptr],[ptr+12,4,0x87654321]],[[401,C],[402,C,offset]], [0,0,0]))
    for slot in (0,1,11,15,17,21,22,255):
        for adj in (0,1,0x7fff,0x8000,0xffff):
            for holder in (0,0x40000,0xfffffff0):
                table=0x50000
                cases.append(([[holder,4,table],[table+8*slot,2,adj]],
                              [[403,holder,slot]], [0x87654321,0,0]))
    for holder in (0,0x40000):
        for adj in (0,0xffff):
            for scenario in (0,1,2):
                for fail in (0,1):
                    cases.append(([[C+0x50,4,holder],[holder,4,0x50000],[0x50000+136,2,adj]],
                        [[404,C],[404,C],[405,C,0xffffffff]], [0xfedcba98,scenario,fail]))
    for final in (None,0,1,0x50cda0,0xffffffff):
        # All nonzero prior fields make unintended clears or missing stores visible.
        extra=[[0x40000+k,8,0xfedcba9876543210] for k in range(0,0xa0,8)]
        cases.append((extra,[[406,C,0x40000,final or 0,int(final is not None)],[406,0,0x40000,final or 0,int(final is not None)]], [0,0,0]))
    for failure in (0,1):
        cases.append(([],[[405,C,0x87654321],[405,C,0]], [0xffffffff,1,failure]))
    for pokes,commands,opts in cases:
        mem=R.RC.Memory()
        for row in pokes:mem.poke(*row)
        m=R.Model(mem); events=[];results=[];calls=0
        original=m._w
        def write(addr,size,v):
            events.append(writetext((addr,size,v&((1<<(8*size))-1))));original(addr,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def callback(*args):
            nonlocal calls
            calls+=1
            if opts[2] and calls==opts[2]:raise RuntimeError('synthetic callback failure')
            if opts[1]==1:mem.store(C+0x50,4,0)
            if opts[1]==2:
                mem.store(0x40000,4,0x51000);mem.store(0x51000+136,2,0xfffe)
            return opts[0]
        for slot in (0,1,11,15,17,21,22,255):m.cb['Holder.v%d'%slot]=callback
        m.cb['obj_getPropInt']=callback
        for cmd in commands:
            op,arg=cmd[:2]
            try:
                if op==400:results.append('b'+m.cstr(arg,cmd[2]).hex())
                elif op==401:results.append('r'+str(m.class_model(arg)))
                elif op==402:results.append('r'+str(m.clip(arg,cmd[2])))
                elif op==403:results.append('r'+str(m.holder_call(arg,cmd[2],0xffffffff,0x100000001,(0,0x80000000,0x7fc12345))))
                elif op==404:results.append('r'+str(m.weapon_descriptor(arg)))
                elif op==405:results.append('r'+str(m._prop_int(arg,cmd[2])))
                elif op==406:m._init_holder(arg,cmd[2],cmd[3] if cmd[4] else None);results.append('ok')
                elif op==407:results.append('r'+str(R.half_toward_zero(arg)))
                elif op==408:results.append('r'+str(R.third_toward_zero(arg)))
                else:raise ValueError('helper op')
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,opts,snapshot(m,events,results)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--research-dir',type=Path,required=True)
    p.add_argument('--output',type=Path,default=Path('native/tests/vectors/arena_monsters.tsv'))
    a=p.parse_args()
    sys.dont_write_bytecode=True
    with tempfile.TemporaryDirectory(prefix='arena-pinned-') as tmp:
        for path,sha in PINS.items():
            data=subprocess.check_output(['git','-C',str(a.research_dir),'show',COMMIT+':'+path])
            if hashlib.sha256(data).hexdigest()!=sha: raise ValueError('pin mismatch: '+path)
            if path.startswith('tools/ref_'): (Path(tmp)/Path(path).name).write_bytes(data)
        sys.path.insert(0,tmp)
        R=importlib.import_module('ref_arena_monsters')
        units=reference_units((Path(tmp)/'ref_arena_monsters.py').read_text())
        rng=random.Random(0x4152454e)
        cases=[]
        states=list(range(40))+[0xffffffff,0x80000000,0x7fffffff,0x10000,0x10006]
        # Exhaust priorities, signed timer reads, PvP gate and inert rejection.
        for target in (0,0x40000):
            for root,stun,cooldown in ((0,0,0),(1,0,0),(0,1,0),(0,0,1),(0xffff,0x8000,0xffff),(1,1,1)):
                for state in states:
                    extra=[[C+0x234,4,target],[C+0x314,2,root],[C+0x31c,2,stun],[C+0x318,2,cooldown]]
                    # State 1 can still be filtered/refused: omit only a resolved wander.
                    for op in (0,1): cases.append((extra,[[op,state]],[7,0,0,0]))
        for state in states:
            cases.append(([[0x4f7120,4,1],[0x503890,4,1]],[[0,state]],[0,0,0,0]))
            cases.append(([[C+0x231,1,255]],[[0,state]],[0,0,0,0]))
        for count in (-1,0,1,2,3,4,7,12,0x80000000,0xffffffff):
            for external in (0,1):
                for failure in range(0,12):
                    cases.append(([],[[0,20]],[count,failure,external,external]))
        # Slot16 signed this adjustment, unknown virtual body, and PvP bypasses.
        for adjustment in (4,0xfffc,0x8000,0x7fff):
            signed = adjustment if adjustment < 0x8000 else adjustment-0x10000
            cases.append(([[T+128,2,adjustment],[C+signed+0x24,4,D]],[[0,20]],[4,0,0,0]))
        cases.append(([[T+132,4,0x12345678]],[[0,20]],[4,0,0,0]))
        for a_mode,b_mode,gate,finished in ((1,1,1,0),(0,0,1,0),(0,1,0,0),(0,1,1,1)):
            cases.append(([[0x4f711c,4,a_mode],[0x4f7120,4,b_mode],[0x503890,4,gate],
                           [0x503894,4,finished]],[[0,6]],[0,0,0,0]))
        for _ in range(128):
            cases.append(([[C+0x44,4,rng.getrandbits(32)],[C+0x300,4,rng.getrandbits(32)],
                           [C+0x232,2,rng.getrandbits(16)]],[[0,6],[0,18],[0,23],[0,0],[2,0]],[0,0,0,0]))
        # Append the base family: the original 1,544 rows remain byte-identical.
        target=[[C+0x234,4,0x40000],[0x4000c,4,0x41000],
                [0x41010,4,0x41200000],[0x41014,4,0],[0x41018,4,0x80000000]]
        for scenario in range(32):
            for failure in range(36):
                cases.append(([[0x4f85c4,1,255]],
                    [[3,0 if scenario&1 and not scenario&16 else 0x60000,0x3f800000,0xc0200000,0x7fc12345,
                      0xffff8123,D,0x70000,0x41200000]], [0, failure,0,scenario&2,scenario]))
        for scenario in (0,1,4,8,15,31):
            ctor=[3,0x60000,0,0x80000000,0x3f800000,0x1234,D,0,0x41200000]
            cases.append(([],[ctor,ctor,[8,0],[9,0x12345678],[15,0]], [0,0,0,scenario&2,scenario]))
        for _ in range(128):
            entry=rng.randrange(32)
            cases.append(([[D+8*entry+4,4,0x60000]],
                [[4,entry]+[rng.getrandbits(32) for _ in range(8)]],[0,0,0,0,0]))
        for node in ([],target):
            for timer in (0,1,0x8000,0xffff):
                cases.append((node+[[C+0x314,2,timer],[C+0x318,2,timer],[C+0x31c,2,timer]],
                              [[5,0],[6,0],[7,0],[8,0],[9,0xffffffff],[10,0],
                               [11,0xffff8123,0x18000,0xffffffff,0x80000000,0x7fc12345,0x42c80000],
                               [13,0x42100000],[14,0]], [0xffffffff,0,0,0,0]))
        for scenario in (0,1,2,4,8,15):
            for quiet in (0,1,0xffffffff):
                for failure in range(10):
                    cases.append(([[C+0xc,4,2],[C+0xa8,8,MASK],[0x4f7120,4,1],
                                  [C+0x2e4,4,0x40000],[0x4000c,4,0x41000]],
                                  [[16,quiet]], [1,failure,0,scenario&2,scenario]))
        for arc in (0,0x7fff,0x8000,0xffff):
            for result in (0,1,0xffffffff):
                for scenario in (0,2,16,32):
                    for failure in (0,1,2):
                        cases.append((target+[[C+0x3ac,2,65535],[C+0x3ae,2,65534],
                                            [C+0x3b0,2,arc]],[[15,scenario&2]],
                                      [result,failure,0,0,scenario]))
        for adjustment in (0,4,0xfffc,0x8000,0x7fff):
            cases.append((target+[[T+26*8,2,adjustment]],[[10,0]],[0,0,0,0,0]))
        cases.append((target+[[T+26*8+4,4,0x10bb08],[T+26*8,2,0xfffc]],[[10,0]],[0,0,0,0,64]))
        for facing in (0,0x3ffe,0x3fff,0x4000,0x8000,0xffff):
            for speed in (0,0x42c7ffff,0x42c80000,0x42c80001,0x43000000,0x7f800000):
                cases.append((target+[[0x41038,2,facing],[0x41308,4,speed]],[[12,0]],[0,0,0,0,0]))
        for _ in range(256):
            cases.append((target+[[a,4,rng.getrandbits(32)] for a in
                                  (C+0x10,C+0x14,0x41010,0x41014,0x41308,0x4130c)]+
                                  [[0x41038,2,rng.getrandbits(16)]],[[12,0]],[0,0,0,0,0]))
        rows=[];counts={}
        for extra,commands,options in cases:
            pokes=[[C+0x24,4,D],[C+0x48,4,T],[T+132,4,0x3c2738],
                   [D+12,4,0x77770000],[D+0x1c,4,0x11110000],[D+0x24,4,0x22220000],
                   [D+0x34,4,0x33330000],[D+0x44,4,0x44440000],
                   [C+0x10,8,0x400000003f800000],[C+0x18,4,0x7fc12345],
                   [C+0x38,2,0xc123],[C+0x44,4,0x12345678],[C+0x300,4,0xfffffff0],
                   [C+0xf4,4,0xcafebabe],[C+0xa8,8,0x8000000000000001],
                   [0x8e4560,8,0xfedcba9876543210],[0x404464,4,0x89abcdef]]+extra
            mem=R.RC.Memory()
            for row in pokes: mem.poke(*row)
            external=['Creature.v16'] if options[2] else []
            if len(options)>4 and options[4]&64: external.append('Creature.v26')
            m=R.Model(mem,external=external)
            events=[];results=[];calls=0
            original=m._w
            def write(addr,size,v):
                events.append(writetext((addr,size,v & ((1<<(8*size))-1))))
                original(addr,size,v)
            m._w=write
            original_call=m.call
            def call(name,*args):
                args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
                events.append(calltext(name,args))
                return original_call(name,*args)
            m.call=call
            def callback(name):
                def fn(*args):
                    nonlocal calls
                    calls+=1
                    if options[1] and calls==options[1]: raise RuntimeError('synthetic callback failure')
                    if options[3]:
                        mem.poke(C+0x24,4,D)
                        mem.poke(D+12,4,0x77770000+calls)
                        mem.poke(C+0x38,2,0xc123+calls)
                        mem.store(0x50000,4,calls)
                    scenario=options[4] if len(options)>4 else 0
                    if name=='GameObject_ctor':
                        mem.store(C+0x24,4,args[2])
                        for k,v in enumerate(args[1]): mem.store(C+0x10+4*k,4,v)
                    if name=='obj_getPropInt': return (0xffff8001+calls) if scenario&1 else 0
                    if name=='obj_getPropStr':
                        if args[1] in (0x52fbf8,0x52fc08,0x52fa00): return 0x60000+calls if scenario&4 else 0
                        return 0x61000 if args[1]==(0x52fa80,0x52fa88,0x52fa90)[scenario%3] and scenario&8 else 0
                    if name=='balance_lookup': return (scenario&8,0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000)
                    if name=='Creature_setStats': mem.store(C+0x38,2,0x4321)
                    if name=='creature_canSeeObj':
                        if scenario&16: mem.store(C+0x234,4,0)
                        if scenario&32:
                            mem.store(C+0x3ae,2,7);mem.store(C+0x38,2,0x8765)
                            return 0 if calls==1 else options[0]
                        return options[0]
                    if name in ('creature_countAlliesNear','creature_isBlockedBy'): return options[0]
                    if name=='Creature.v16': return 0x99990000+calls
                    if name=='model_getSamplePointCount': return options[0]
                    if name=='model_getSamplePoint': return ((0x3f800000,0x80000000,0x7fc12345),(0,1,2))
                    return 0
                return fn
            for name in ('handle_clear','anim_start','anim_startBlendInOut','anim_syncToSimFrame',
                         'Creature.v16','model_getSamplePointCount','model_getSamplePoint','fx_spawn','obj_delete',
                         'GameObject_ctor','obj_getPropInt','obj_getPropStr','obj_moveOffGrid','balance_lookup',
                         'Creature_setStats','Creature.v26','creature_canSeeObj','creature_countAlliesNear',
                         'creature_isBlockedBy','script_runByName','loot_spawnNamedItem','anim_jumpTopToEnd',
                         'arena_addKill','creature_stopBossMusic'):
                m.cb[name]=callback(name)
            unsupported=False
            for command in commands:
                op,state=command[:2]
                # Inspect filtered state on independent memory to exclude only unported wander.
                if op==0:
                    probe=R.Model(R.RC.Memory());probe.mem.bytes=mem.bytes.copy()
                    refused,filtered=probe.filter_state(C,state)
                    if not refused and filtered==1: unsupported=True;break
                try:
                    if op==0: m.skelight_set_state(C,state);results.append('ok')
                    elif op==1:
                        refused,state=m.filter_state(C,state);results.append(f'f{refused},{state}')
                    elif op==2: m.skelight_shatter(C,state);results.append('ok')
                    elif op==3:
                        m.creature_ctor(C,state,tuple(command[2:5]),*command[5:]);results.append('ok')
                    elif op==4:
                        m.set_def_entry_params(C,state,command[2:6],command[6:10]);results.append('ok')
                    elif op in (9,16):
                        getattr(m,{9:'slot34_set_home_range',16:'slot28_on_death'}[op])(C,state);results.append('ok')
                    else:
                        methods={5:'is_not_rooted',6:'is_attack_ready',7:'is_not_stunned',8:'slot16_model',
                                 10:'turn_to_target',11:'can_see_target_in',12:'is_target_charging',
                                 13:'count_allies_near_target',14:'is_blocked_by_target',15:'slot25_can_see_target'}
                        args=command[1:] if op in (11,13,15) else []
                        results.append('r'+str(getattr(m,methods[op])(C,*args)))
                except RuntimeError: results.append('callback_error');break
            if unsupported: continue
            rows.append('\t'.join([str(len(rows)),packed(pokes),packed(commands),packed([options]),snapshot(m,events,results)]))
            counts[str(commands[0][0])]=counts.get(str(commands[0][0]),0)+1
        for init,commands,expected in script_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'script:'+packed(init), packed(commands), '-', expected]))
            counts['script']=counts.get('script',0)+1
        for pokes,commands,options,expected in tagar_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'tagar:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['tagar']=counts.get('tagar',0)+1
        for pokes,commands,options,expected in summon_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'summon:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['summon']=counts.get('summon',0)+1
        for pokes,commands,options,expected in helper_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'helper:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['helper']=counts.get('helper',0)+1
        data=('# arena-monsters-v1\n'+'\n'.join(rows)+'\n').encode()
        a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data)
        a.output.with_suffix('.json').write_text(json.dumps({'schema':'arena-monsters-v1','research_commit':COMMIT,
            'references':PINS,'reference_function_count':len(units),'ported_units':PORTED_UNITS,'cases':len(rows),'operations':counts,'vector_sha256':hashlib.sha256(data).hexdigest(),
            'scope':'Skelight set-state excluding resolved wander; filter_state; shatter; complete arena creature base family; complete ArenaScript and scaled_time; complete Tagar cast/spell family; complete Summon cast/spell family and Skelight constructor; complete shared data/holder and signed attack-scaling helpers',
            'observations':'all sparse bytes, memory store log, model writes/calls/events, shared draw count, results'},indent=2)+'\n')
        print(f'{len(rows)} exact reference cases')
if __name__=='__main__': main()
