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
# filter_state and inherited base bodies are exercised but do not belong to
# this pinned file. Every counted unit has a complete tested contract path.
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
        'weapon_descriptor','_prop_int','_init_holder','zek_ctor','zek_slot35',
        'zek_set_state','_zek_specials','_zek_wave','horsemen_ctor',
        'horsemen_set_state','_horsemen_cast','zek_take_damage','skelight_set_state','_zek_switch','horsemen_run','skelight_run','zek_run','call','vcall','skill_call','table_call','set_state')
] + ['shape_result','scaled_time','half_toward_zero','third_toward_zero'] + ['ArenaScript.'+name for name in (
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
    if len(units)!=78 or len(set(PORTED_UNITS))!=78 or not set(PORTED_UNITS)<=set(units):
        raise ValueError('reference unit accounting changed')
    return units

C, D, T = 0x10000, 0x20000, 0x30000
MASK = (1 << 64)-1

def packed(rows):
    return ';'.join(','.join(str(x & MASK) for x in row) for row in rows) or '-'

def value(v):
    if v is None: return 'n'
    if isinstance(v,tuple) and len(v)==3 and v[0]=='elem': return 'e'+str(v[1])+','+str(v[2])
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


def tagar_vectors(R, dispatch=False):
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
        if dispatch and any(cmd[0] not in (203,207) for cmd in commands):continue
        pokes=base+extra
        if dispatch:pokes += [[0x68000+8*2+4,4,0x30c708],[R.VT_SPELL_TAGAR+8*2+4,4,0x30cbc8]]
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
                elif op==203:results.append('r'+str(m.skill_call(K,2,arg,O,(0x3f800000,0x80000000,0x7fc12345),0xffff) if dispatch else m.tagar_cast(K,arg,O,(0x3f800000,0x80000000,0x7fc12345),0xffff)))
                elif op==204:results.append('r'+str(m.tagar_is_valid(S,arg,cmd[2])))
                elif op==205:results.append('r'+str(m.tagar_pick_target(S,arg)))
                elif op==206:results.append('r'+str(m.tagar_tick_victims(S)))
                elif op==207:
                    if dispatch:m.table_call('Spell',S,0x14,2)
                    else:m.tagar_spell_run(S)
                    results.append('ok')
                elif op==209:results.append('r'+str(m._unit(arg)))
                elif op==210:m.spell_base_ctor(S,*cmd[1:]);results.append('ok')
                else:raise ValueError('tagar operation')
            except RuntimeError:results.append('callback_error');break
        yield pokes,[[cmd[0]+1000]+cmd[1:] for cmd in commands] if dispatch else commands,[scenario,fail],snapshot(m,events,results)

def summon_vectors(R, dispatch=False):
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
        if dispatch and any(cmd[0] not in (300,305,306) for cmd in commands):continue
        # Options have fixed-width words; no precision-dependent text parsing.
        opts=[scenario&0xffffffff,(scenario>>32)&0xffffffff,(scenario>>64)&0xffffffff,(scenario>>96)&0xffffffff,fail,(scenario>>128)&0xffffffff]
        pokes=base+extra
        if dispatch:pokes += [[0x69000+8*1+4,4,0x2e2368],[0x69000+8*2+4,4,0x2e18a4],[S+20,4,R.VT_SPELL_SUMMON],[R.VT_SPELL_SUMMON+8*2+4,4,0x2e177c]]
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
                if op==300:results.append('r'+str(m.skill_call(K,1,O) if dispatch else m.summon_can_use(K,O)))
                elif op==301:results.append('r'+str(m.cosmetic_draw()))
                elif op==302:results.append(value(m._cosmetic_ball()))
                elif op==303:results.append(value(m._summon_sample(O,R.s32(arg))))
                elif op==304:m.summon_spell_ctor(S,arg,O,K,cmd[2]);results.append('ok')
                elif op==305:
                    if dispatch:m.table_call('Spell',S,0x14,2)
                    else:m.summon_spell_run(S)
                    results.append('ok')
                elif op==306:results.append('r'+str(m.skill_call(K,2,arg,O,(0x3f800000,0x80000000,0x7fc12345),cmd[2] if len(cmd)>2 else 0xffff) if dispatch else m.summon_cast(K,arg,O,(0x3f800000,0x80000000,0x7fc12345),cmd[2] if len(cmd)>2 else 0xffff)))
                elif op==307:m.skelight_ctor(B,(0x3f800000,0x80000000,0x7fc12345),0xffff,0x78000,arg);results.append('ok')
                else:raise ValueError('summon op')
            except RuntimeError:results.append('callback_error');break
        yield pokes,[[cmd[0]+1000]+cmd[1:] for cmd in commands] if dispatch else commands,opts,snapshot(m,events,results)


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


def zek_vectors(R):
    Z,H,E,N=0x10000,0x103c8,0x41000,0x40000
    base=[[Z+0x24,4,R.DEF_ZEK],[Z+0x48,4,R.VT_ZEK],[Z+0x50,4,H],
          [Z+0x44,4,0x12345678],[Z+0x38,2,0xc123],[Z+0x3c0,4,0x43960000],
          [Z+0x10,4,0x3f800000],[Z+0x14,4,0x40000000],[Z+0x18,4,0x40400000],
          [Z+0x3b4,4,0x3f800000],[Z+0x3b8,4,0x40000000],[Z+0x3bc,4,0x40400000],
          [Z+0xd0,2,0x8001],[Z+0xd2,2,0x7fff],[Z+0x464,4,1],
          [Z+0x46c,4,0x42f00000],[Z+0x470,4,0],[Z+0x474,4,0x40400000],
          [Z+0x300,4,1000],[Z+0x478,4,0],[R.G_WORLD,4,0x70000],
          [R.G_SIM_FRAME,4,0xfffffff0],[R.G_LEVEL_MONSTER_LEVEL,4,0x41200000],
          [N+12,4,E],[E+0x10,4,0x43960000],[E+0x14,4,0xc2c80000],
          [R.G_LEVEL_NAME,4,0x64636261],[R.S_ZEK_LEVEL,4,0x64636261]]
    for entry in range(3,22):
        base += [[R.DEF_ZEK+8*entry+4,4,0x80000+entry*0x100]]
    cases=[]
    for scenario in (0,1,2,4,8,16,32,64,128,256,512,1024):
        for fail in range(0,48):
            cases.append(([],[[500,0]], [scenario,fail,0]))
    for state in list(range(32))+[0xffffffff,0x80000000,0x10006]:
        for scenario in (0,4,8,16,32,64,128,256,512,1024):
            for fail in range(0,7):
                extra=[]
                if scenario&4:extra+=[[Z+0x50,4,0]]
                if scenario&8:extra+=[[R.G_LEVEL_NAME,1,65]]
                if scenario&16:extra+=[[Z+0xa8,8,0x8000000]]
                if scenario&32:extra+=[[Z+0xa8,8,0xc00000],[Z+0x10,4,0x43fa0000]]
                if scenario&64:extra+=[[Z+0xa8,8,0x400000],[Z+0x3c0,4,0x42a00000]]
                if scenario&128:extra+=[[Z+0x314,2,1],[Z+0x234,4,N]]
                if scenario&256:extra+=[[Z+0x31c,2,1]]
                if scenario&512:extra+=[[Z+0x48,4,0x30000],[0x30000+28*8+4,4,0x10beb8],[0x30000+28*8,2,0xfffc]]
                cases.append((extra,[[501,state]], [scenario,fail,0]))
    for clock in (0,1,179,180,181,0x7fffffff,0x80000000,0xffffffff):
        for timer in (0,1,0xffffff4c,0x7fffffff,0x80000000,0xffffffff):
            for scenario in (0,1,2,3,1024):
                for fail in (0,1,2,3):
                    cases.append(([[Z+0x300,4,clock],[Z+0x478,4,timer]],[[502,0]],[scenario,fail,0]))
    for active in (0,1):
        for heading in (0,0x4000,0x7fff,0x8000,0xc000,0xffff):
            for height in (0,0x40000000,0x415fffff,0x41600000,0x41600001,0xc1200000,0xc1400000,0x7f800000,0x7fc12345):
                for scenario in (0,1,2,1024):
                    for fail in range(0,10):
                        extra=[[Z+0x464,4,active],[Z+0x468,2,heading]]
                        if scenario&1:extra+=[[Z+0x460,4,N]]
                        if scenario&2:extra+=[[Z+0x46c,4,0x44480000],[Z+0x470,4,0x44480000]]
                        cases.append((extra,[[503,0],[503,0]], [scenario,fail,height]))
    # Exact enemy-distance, owner-distance and terrain discontinuity boundaries.
    for enemy_x in (0x420fffff,0x42100000,0x42100001,0xc2100000):
        for owner_x in (0x4447ffff,0x44480000,0x44480001):
            for fail in range(12):
                cases.append(([[Z+0x460,4,N],[Z+0x468,2,0],[Z+0x46c,4,0xc0a00000],
                               [Z+0x470,4,0],[Z+0x10,4,owner_x],[Z+0x14,4,0],
                               [E+0x10,4,enemy_x],[E+0x14,4,0]],[[503,0]],[0,fail,0x40000000]))
    for reach in (0,0x80000000,0xbf800000,0x42a00000,0x43960000,0x7fc12345):
        for home_x in (0,0x42f00000,0x43960000):
            for scenario in (0,16,32,64):
                cases.append(([[Z+0x3c0,4,reach],[Z+0x3b4,4,home_x],
                               [Z+0xa8,8,0xc00000 if scenario==32 else 0x8000000 if scenario==16 else 0x400000 if scenario==64 else 0]],
                              [[501,1]],[scenario,0,0]))
    for level in (0,0x80000000,0x3f800000,0x41200000,0x7f800000,0x7fc12345,0xcf000000):
        cases.append(([[R.G_LEVEL_MONSTER_LEVEL,4,level]],[[504,0]],[0,0,0]))
    for scenario in (0,1,2,1024):
        cases.append(([],[[501,6],[501,18],[502,0],[503,0],[501,20]], [scenario,0,0x40000000]))
    names=('GameObject_ctor','obj_getPropInt','obj_getPropStr','obj_moveOffGrid','balance_lookup',
           'Creature_setStats','Holder.v1','Holder.v11','handle_clear','anim_start',
           'anim_startBlendInOut','anim_syncToSimFrame','anim_clear','obj_runEvery60','Creature_init',
           'terrain_heightBelow','Creature_dropLoot','Creature.v28','creature_findEnemyInCone',
           'terrain_height','fx_ringBurst','res_find','sound_playOnObject','combat_applyAreaDamage',
           'decal_add','pool_new','LightEffect_ctor','fx_spawn','state_request','script_runByName',
           'loot_spawnNamedItem','anim_jumpTopToEnd','obj_delete','arena_addKill','creature_stopBossMusic')
    for extra,commands,opts in cases:
        pokes=base+extra;mem=R.RC.Memory()
        for p in pokes:mem.poke(*p)
        m=R.Model(mem);events=[];results=[];count=0;cone=0
        original=m._w
        def write(a,size,v):
            events.append(writetext((a,size,v&((1<<(size*8))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def cb(name):
            def fn(*args):
                nonlocal count,cone
                count+=1;scenario=opts[0]
                if opts[1] and count==opts[1]:raise RuntimeError('synthetic callback failure')
                if scenario&1024:
                    mem.store(Z+0x300,4,0x80000000+count);mem.store(Z+0x38,2,0x1234+count)
                    mem.store(Z+0x46c,4,0x44200000);mem.store(Z+0x24,4,R.DEF_ZEK)
                    mem.store(R.DEF_ZEK+0x7c,4,0x90000+count)
                if name=='GameObject_ctor':
                    mem.store(Z+0x24,4,args[2]);mem.store(Z+0x10,4,args[1][0]);mem.store(Z+0x14,4,args[1][1]);mem.store(Z+0x18,4,args[1][2])
                if name=='obj_getPropInt':return 1 if scenario&1 else 0
                if name=='balance_lookup':return (scenario&2,0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000)
                if name=='terrain_heightBelow':return 0x7fc12345 if scenario&64 else 0x40400000
                if name=='terrain_height':return opts[2]
                if name=='res_find':return 0x70001+count
                if name=='pool_new':return 0 if scenario&1 else 0x71000
                if name=='creature_findEnemyInCone':cone+=1;return (scenario>>(cone-1))&1
                return 0
            return fn
        for n in names:m.cb[n]=cb(n)
        for op,arg in commands:
            try:
                if op==500:m.zek_ctor(Z,0x60000,(0x3f800000,0x40000000,0x40400000),0xffff8123,0x70000);results.append('ok')
                elif op==501:m.zek_set_state(Z,arg);results.append('ok')
                elif op==502:results.append('r'+str(int(m._zek_specials(Z,lambda c,s:m.call('state_request',c,s)))))
                elif op==503:m._zek_wave(Z);results.append('ok')
                elif op==504:results.append('r'+str(m.zek_slot35(Z)))
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,opts,snapshot(m,events,results)


def horsemen_vectors(R):
    Z,H,N,E,K,T=0x10000,0x103d0,0x40000,0x41000,0x60000,0x30000
    base=[[Z+0x24,4,R.DEF_HORSEMEN],[Z+0x48,4,R.VT_HORSEMEN],[Z+0x50,4,H],
          [Z+0x44,4,0x12345678],[Z+0x38,2,0xc123],[Z+0x3c0,4,0x43960000],
          [Z+0x10,4,0x80000000],[Z+0x14,4,0x40000000],[Z+0x18,4,0x40400000],
          [Z+0x20,4,0x42800000],[Z+0x300,4,0xfffffff0],[Z+0xa8,8,0x200],
          [Z+0x234,4,N],[N+12,4,E],[E+0x10,4,0x43960000],[E+0x14,4,0xc2c80000],
          [E+0x18,4,0x7fc12345],[E+0xc,4,0x20],
          [R.VT_HORSEMEN+27*8+4,4,0x272f4c],[R.VT_HORSEMEN+35*8+4,4,0x118c50],
          [R.G_LEVEL_MONSTER_LEVEL,4,0x41200000],[R.G_WORLD,4,0x70000],
          [R.G_SIM_FRAME,4,0xfffffff0],[K+0x60,4,T],[T+16,2,0xfffc]]
    for g in (R.G_SKILL_DISEASEBOLT,R.G_SKILL_FIRESTORM,R.G_SKILL_TAGARSINSECTS,R.G_SKILL_SUMMONSKELETON):base+=[[g,4,K]]
    for entry in range(3,28):base+=[[R.DEF_HORSEMEN+8*entry+4,4,0x80000+entry*0x100]]
    cases=[]
    for variant in (0,1,2,3,127,128,255,256,0xffffffff,0x80000000):
        for mounted in (0,1):
            for scenario in (0,1,2,1024):
                for fail in (0,1,2,4,6,8,10,12,16,20,24,28,32,36,40):
                    cases.append(([],[[600,variant],[603,mounted]],[scenario,fail,0]))
    for state in list(range(44))+[0xffffffff,0x80000000,0x7fffffff,0x10006]:
        for mounted in (0,1):
            for scenario in (0,2,4,1024):
                for fail in range(8):
                    extra=[[Z+0x3c8,4,0x72000 if mounted else 0],[Z+0x470,1,3]]
                    if scenario&2:extra+=[[Z+0x232,2,6],[Z+0x11a,2,8]]
                    if scenario&4:extra+=[[Z+0x50,4,0]]
                    cases.append((extra,[[601,state]],[scenario,fail,0]))
    for timer in (0x314,0x318,0x31c):
        for state in (0,1,3,6,17,18,20,25,35,0xffffffff):
            for target in (0,N):
                cases.append(([[Z+timer,2,1],[Z+0x234,4,target]],[[601,state]],[0,0,0]))
    for gate in ([[R.G_PVP_MODE_B,4,1],[0x503890,4,1]],[[Z+0x231,1,255]]):
        for state in (0,1,20,35):cases.append((gate,[[601,state]],[0,0,0]))
    # Slot 27 nested fallback with signed adjustment and base/external dispatch.
    for fn in (0x272f4c,0x3c2688,0):
        for adjust in (0,4,0xfffc,0x8000,0x7fff):
            signed=adjust if adjust<0x8000 else adjust-0x10000
            extra=[[R.VT_HORSEMEN+216,2,adjust],[R.VT_HORSEMEN+220,4,fn],
                   [Z+signed+0x48,4,0x31000],[Z+signed+0x24,4,R.DEF_HORSEMEN]]
            cases.append((extra,[[601,9]],[0,0,0]))
    for variant in (0,1,2,3,4,127,128,255,256):
        for level in (0,0x80000000,0x3f800000,0x41200000,0x7f800000,0x7fc12345):
            for scenario in (0,1024):
                for fail in (0,1,2,3):
                    cases.append(([[Z+0x470,1,variant],[R.G_LEVEL_MONSTER_LEVEL,4,level]],[[602,0]],[scenario,fail,0]))
    for variant,fn in ((2,0x30c708),(3,0x2e18a4)):
        # Zero allocation: run the real skill cast, including terrain/RNG/sound.
        for fail in range(9):
            cases.append(([[Z+0x470,1,variant],[T+16,2,0],[T+20,4,fn]],[[602,0]],[1,fail,0x40400000]))
    # Internal pet death, base on-death and forced external selections.
    for fail in range(8):
        cases.append(([[Z+0x470,1,3],[E+0x48,4,0x32000],[0x32000+220,4,0x2a91b8],
                       [R.VT_HORSEMEN+228,4,0x10beb8]],[[601,20]],[0,fail,0]))
    for scenario in (0,2,4,1024):
        cases.append(([],[[601,6],[601,18],[601,9],[601,35],[602,0]], [scenario,0,0]))
    names=('GameObject_ctor','obj_getPropInt','obj_getPropStr','obj_moveOffGrid','balance_lookup',
           'Creature_setStats','Holder.v1','Holder.v11','handle_clear','handle_link','anim_start',
           'anim_startBlendInOut','anim_syncToSimFrame','anim_clear','anim_push','obj_runEvery60',
           'Creature_init','terrain_heightBelow','Creature_dropLoot','Creature.v28','Creature.v27',
           'Creature.v35','model_buildHideMask','lump_load','pool_new','Nightmare_ctor',
           'world_findObjectByName','Creature.v16','Skill.v2','creature_addSpell','model_getSamplePointCount',
           'terrain_height','res_find','sound_playAtPos','Skill.v12','Skill.v14',
           'obj_delete','creature_stopBossMusic')
    for extra,commands,opts in cases:
        pokes=base+extra;mem=R.RC.Memory()
        for p in pokes:mem.poke(*p)
        m=R.Model(mem);events=[];results=[];count=0
        original=m._w
        def write(a,size,v):
            events.append(writetext((a,size,v&((1<<(size*8))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def cb(name):
            def fn(*args):
                nonlocal count
                count+=1;scenario=opts[0]
                if opts[1] and count==opts[1]:raise RuntimeError('synthetic callback failure')
                if scenario&1024:
                    mem.store(Z+0x300,4,0x80000000+count);mem.store(Z+0x38,2,0x1234+count)
                    mem.store(Z+0x24,4,R.DEF_HORSEMEN);mem.store(R.DEF_HORSEMEN+0x54,4,0x90000+count)
                    if name=='Holder.v11':mem.store(Z+0x50,4,0)
                    if name=='anim_start':mem.store(Z+0x3c8,4,0x72000)
                if name=='GameObject_ctor':
                    mem.store(Z+0x24,4,args[2]);mem.store(Z+0x10,4,args[1][0]);mem.store(Z+0x14,4,args[1][1]);mem.store(Z+0x18,4,args[1][2])
                if name=='obj_getPropInt':return 1 if scenario&2 else 0
                if name=='balance_lookup':return (scenario&2,0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000)
                if name=='terrain_heightBelow':return 0x40400000
                if name=='terrain_height':return opts[2]
                if name=='model_buildHideMask':return 0x123456789abcdef0
                if name=='pool_new':return 0 if scenario&1 else 0x72000
                if name=='world_findObjectByName':return E
                if name=='handle_link':mem.store(args[0],4,args[1])
                if name=='Skill.v2':return 0x73000
                if name=='Skill.v12':return 0
                if name=='Skill.v14':return 1
                return 0
            return fn
        for n in names:m.cb[n]=cb(n)
        for index,(op,arg) in enumerate(commands):
            try:
                if op==600:
                    m.horsemen_ctor(Z,0x60000,(0x80000000,0x40000000,0x40400000),0xffff8123,0x70000,arg,commands[index+1][1]);results.append('ok')
                elif op==601:m.horsemen_set_state(Z,arg);results.append('ok')
                elif op==602:m._horsemen_cast(Z);results.append('ok')
                elif op==603:pass
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,opts,snapshot(m,events,results)


def zek_damage_vectors(R):
    A,N,W=0x41000,0x40000,0x42000
    base=[[C+0x44,4,0x12345678],[C+0x10,4,0x3f800000],
          [C+0x14,4,0x40000000],[C+0x18,4,0x40400000],
          [C+0x1c,4,0x41a00000],[C+0x20,4,0x42200000],
          [C+0x228,4,0x42c80000],[C+0xc8,4,0x3f800000],
          [C+0x2dc,4,0x80000001],[C+0x2e4,4,N],
          [C+0x348,4,1],[C+0x344,4,2],[C+0x340,4,3],
          [C+0x350,4,4],[C+0x34c,4,5],
          [A+0xc,4,0x20],[A+0x10,4,0x41a00000],[A+0x14,4,0xc1200000],
          [N+12,4,W],[R.G_SIM_FRAME,4,0xfffffff0]]
    cases=[]
    # Entire base damage accumulator, attacker/effect-source precedence, raw HP.
    for objflags in (0,0x20,0x80,0xa0,0xffffffff):
        for flags in (0,1,0x10000000,0x10002000,0xffffffff):
            for hp in (0,0x80000000,0xbf800000,0x42c80000,0x7f800000,0x7fc12345):
                for node in (0,N):
                    # No spark bit: exercise the accumulator without duplicate cosmetics.
                    f=flags&~0x2000
                    cases.append(([[A+0xc,4,objflags],[C+0x228,4,hp],
                        [A+0x4c,4,node],[C+0x360,4,node]],[[505,A,f,0x3fc00000]],[0,0,0]))
    for amount in (0,0x80000000,0x3f800001,0xbf800000,0x7f800000,0xff800000,0x7fc12345):
        for pending in (0,0x80000000,0x3f800001,0xbf800000,0x7f800000,0x7fc12345):
            cases.append(([[C+0xc8,4,pending]],[[505,0,0xffffffff&~0x2000,amount],
                [505,A,1,amount]],[0,0,0]))
    # Spark gates independently; inert/IgnoreDamage still receive the cosmetic prefix.
    for attacker in (0,A):
        for flags in (0,0x2000,0x10002000):
            for objflags in (0,0x20,0x80,0xa0):
                for inert,ignore in ((0,0),(1,0),(255,0),(0,0x1000000000000)):
                    cases.append(([[A+0xc,4,objflags],[C+0x231,1,inert],[C+0xa8,8,ignore]],
                                  [[505,attacker,flags,0x3f800000]],[0,0,0]))
    # Every service failure prefix (three setup calls, fifty particles, two handles).
    for ready in (0,1):
        for fail in range(56):
            cases.append(([[R.G_ZEK_SPARK_READY,4,ready]],[[505,A,0x2000,0x3f800000]],[0,fail,0]))
    for seed in (0,1,0xffffffff,0x80000000,0x89abcdef):
        cases.append(([[C+0x44,4,seed]],[[505,A,0x2000,0x3f800000],
            [505,A,0x2000,0x3f800000]],[0,0,0]))
    # Raw math, including unnormalized/NaN patterns and zero-length bearing.
    for x,y in ((0,0),(0x3f800000,0x40000000),(0x7f800000,0xff800000),
                (0x7fc12345,0x80000000),(0xc1200000,0x41a00000)):
        for size in (0,0x80000000,0xbf800000,0x7f800000,0x7fc12345):
            cases.append(([[A+0x10,4,x],[A+0x14,4,y],[C+0x1c,4,size],[C+0x20,4,size]],
                          [[505,A,0x2000,0x3f800000]],[0,0,0]))
    for scenario in (2048,4096,8192,16384,32768,65536,131072):
        for fail in (0,1,2,3,4,20,53,54,55):
            cases.append(([],[[505,A,0x10002000,0x3f800000]],[scenario,fail,0]))
    names=('res_find','sound_playOnObject','particleDef_ctor','particle_spawn','handle_clear','handle_link')
    for extra,commands,opts in cases:
        pokes=base+extra;mem=R.RC.Memory()
        for poke in pokes:mem.poke(*poke)
        m=R.Model(mem);events=[];results=[];count=0
        original=m._w
        def write(a,size,v):
            events.append(writetext((a,size,v&((1<<(size*8))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def cb(name):
            def fn(*args):
                nonlocal count
                count+=1;scenario=opts[0]
                if opts[1] and count==opts[1]:raise RuntimeError('synthetic callback failure')
                if scenario&2048:mem.store(C+0x231,1,1)
                if scenario&4096:mem.store(C+0xa8,8,0x1000000000000)
                if scenario&8192:
                    mem.store(C+0x10,4,0x42a00000+count);mem.store(A+0x14,4,0x41a00000)
                    mem.store(C+0x1c,4,0x3f800000);mem.store(C+0x20,4,0x40000000)
                if scenario&16384:mem.store(C+0x44,4,0x80000000+count)
                if scenario&32768:
                    mem.store(C+0x228,4,0x3f800000+count);mem.store(C+0xc8,4,0xbf800000)
                    mem.store(R.G_SIM_FRAME,4,count);mem.store(C+0x2dc,4,count)
                if scenario&65536 and name=='handle_clear':
                    mem.store(A+0xc,4,0xa0);mem.store(A+0x4c,4,N);mem.store(N+12,4,0)
                if scenario&131072 and name=='handle_clear':
                    mem.store(C+0x360,4,N);mem.store(N+12,4,W)
                if name=='res_find':return 0x70001+count
                if name=='handle_clear':mem.store(args[0],4,0)
                if name=='handle_link':mem.store(args[0],4,N)
                return 0
            return fn
        for name in names:m.cb[name]=cb(name)
        for op,attacker,flags,amount in commands:
            try:m.zek_take_damage(C,attacker,flags,amount);results.append('ok')
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,opts,snapshot(m,events,results)


def skelight_state_vectors(R):
    # Complete missing state entry; original non-wander rows stay untouched.
    base=[[C+0x24,4,D],[C+0x38,2,0x8123],[C+0x44,4,0x12345678],
          [D+0x44,4,0x80000],[D+0x34,4,0x81000],
          [C+0x10,4,0x3f800000],[C+0x14,4,0x40000000],[C+0x18,4,0x40400000],
          [C+0x3b4,4,0x3f800000],[C+0x3b8,4,0x40000000],[C+0x3bc,4,0x40400000],
          [C+0x3c0,4,0x43960000],[C+0x1c,4,0x41200000],
          [C+0x300,4,0xfffffff0],[C+0xf4,4,0xdeadbeef],[C+0x232,2,6],
          [R.G_WORLD,4,0x70000],[C+0x104,8,0x0123456789abcdef],[C+0x10c,4,0x76543210]]
    cases=[]
    for flags in (0,0x8000000,0x400000,0x800000,0xc00000,0x8c00000):
        for x in (0x3f800000,0x42f1ffff,0x42f20000,0x42f20001,0x43968000,0x7f800000,0x7fc12345):
            for reach in (0,0x80000000,0xbf800000,0x42a00000,0x43960000,0x7fc12345,0xcf000000):
                for fail in (0,1,2,3):
                    cases.append(([[C+0xa8,8,flags],[C+0x10,4,x],[C+0x3c0,4,reach]],
                                  [[506,1]],[0,fail,0]))
    # Strict terrain acceptance limits relative to z=3: -9 and 15.
    for ground in (0xc1100001,0xc1100000,0xc10fffff,0x416fffff,0x41700000,0x41700001,
                   0x40400000,0x7f800000,0xff800000,0x7fc12345):
        for seed in (0,1,0xffffffff,0x80000000,0x12345678):
            for fail in (0,1,2,5,10,11,12):
                cases.append(([[C+0x44,4,seed]],[[506,1]],[8192,fail,ground]))
    # Signed clock wrapping; callback-time definition/clock/home/seed/height rereads.
    for scenario in (0,2048,4096,6144,8192,10240,12288,14336):
        for clock in (0,0x7fffffff,0x80000000,0xffffffd4,0xffffffff):
            for fail in (0,1,2,3,10,11,12):
                cases.append(([[C+0x300,4,clock]],[[506,1],[506,1],[506,0],[506,1]],
                              [scenario,fail,0x7fc12345]))
    # Filter priorities, inert and PvP gates, including a signed non-wander request.
    for target in (0,0x40000):
        for root,stun,cool in ((0,0,0),(1,0,0),(0,1,0),(0,0,1),(1,1,1),(0xffff,0x8000,0xffff)):
            for inert,pvp in ((0,0),(1,0),(0,1)):
                extra=[[C+0x234,4,target],[C+0x314,2,root],[C+0x31c,2,stun],[C+0x318,2,cool],
                       [C+0x231,1,inert],[0x4f7120,4,pvp],[0x503890,4,pvp]]
                cases.append((extra,[[506,1],[506,0x10001]],[0,0,0]))
    names=('handle_clear','terrain_heightBelow','anim_start','anim_syncToSimFrame')
    for extra,commands,opts in cases:
        pokes=base+extra;mem=R.RC.Memory()
        for p in pokes:mem.poke(*p)
        m=R.Model(mem);events=[];results=[];count=0
        original=m._w
        def write(a,size,v):
            events.append(writetext((a,size,v&((1<<(size*8))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def cb(name):
            def fn(*args):
                nonlocal count
                count+=1;scenario=opts[0]
                if opts[1] and count==opts[1]:raise RuntimeError('synthetic callback failure')
                if scenario&2048:
                    mem.store(C+0x300,4,0xffffff00+count);mem.store(C+0x24,4,D)
                    mem.store(D+0x44,4,0x91000+count)
                if scenario&4096 and name=='terrain_heightBelow':
                    mem.store(C+0x3b4,4,0x41200000+count);mem.store(C+0x3b8,4,0xc1200000+count)
                    mem.store(C+0x3bc,4,0x3f800000);mem.store(C+0x18,4,0x40000000)
                    mem.store(C+0x44,4,0x80000000+count);mem.store(C+0xa8,8,0x8000000)
                if name=='terrain_heightBelow':return opts[2] if scenario&8192 else 0x40400000
                return 0
            return fn
        for name in names:m.cb[name]=cb(name)
        for op,arg in commands:
            try:m.skelight_set_state(C,arg);results.append('ok')
            except RuntimeError:results.append('callback_error');break
        yield pokes,commands,opts,snapshot(m,events,results)


def shape_value(v):
    if v is None: return 'n'
    if isinstance(v,int): return 'i'+str(v)+';'
    if isinstance(v,str): return 's'+v.encode().hex()+';'
    if isinstance(v,bytes): return 'b'+v.hex()+';'
    if isinstance(v,(list,tuple)):
        return ('l' if isinstance(v,list) else 't')+str(len(v))+':'+''.join(map(shape_value,v))
    raise ValueError('unsupported synthetic shape value')


def shape_vectors(R):
    # Cover every inherited and arena-added schema, including optional output
    # consumption even for a null pointer and exact None/list/tuple/bytes shapes.
    truths=(0,1,-1,None,b'',b'\x00',[],[0],(),(0,))
    outputs=(None,[],[0],[None],[b'\x00\xff'],[(1,0x80000000,0x7fc12345)],
             [[1,0xffffffff]], [11,22,33,44,55,66,77,88,99],
             [None,[],(),b'',b'\x00\xff',-1,0xffffffffffffffff])
    for name,ext in sorted(R.EXTERNALS.items()):
        for truth in truths:
            for outs in outputs:
                # All argument slots carry the selected truth value: optional
                # outputs must use the signature's canonical argument index.
                args=[truth]*16
                for ret,fret in ((0,0),(-1,0x7fc12345),(0xffffffffffffffff,0x80000000)):
                    expected=R.shape_result(name,args,ret,fret,outs)
                    yield name,args,(ret,fret,outs),shape_value(expected)
        # Single live optional pointer isolates its position from preceding
        # output-only arguments (which do not consume an argument slot).
        for slot in range(16):
            args=[0]*16;args[slot]=1
            outs=[11,22,33,44,55,66,77,88]
            yield name,args,(5,7,outs),shape_value(R.shape_result(name,args,5,7,outs))
        for args in ([],[1],[0],[1,2],[0,0]):
            try: expected=shape_value(R.shape_result(name,args,5,7,[11,22,33]))
            except IndexError: expected='argument_error'
            yield name,args,(5,7,[11,22,33]),expected


def navigation_vectors(R):
    # All inputs are authored sparse fixtures. Every expectation executes the
    # immutable arena/creature contracts, including inherited bodies internally.
    H,O,N,HOLDER=0x40000,0x50000,0x60000,0x70000
    base=[[C+0x24,4,D],[C+0x48,4,T],[D+12,4,0x81000],
          [C+0x10,4,0],[C+0x14,4,0],[C+0x18,4,0x40400000],
          [C+0x1c,4,0x41200000],[C+0x20,4,0x41a00000],
          [C+0x38,2,0x8123],[C+0x44,4,0x12345678],[C+0xb0,4,0x3f800000],
          [C+0xb8,4,0x42700000],[C+0xbc,4,0x3e800000],[C+0xc0,4,0xbe800000],
          [C+0xc4,2,123],[C+0x104,4,0x43480000],[C+0x108,4,0x42c80000],[C+0x10c,4,0x40400000],
          [C+0xa8,8,0x100000],[C+0xf8,4,0xbf800000],[C+0xfc,4,0xbf800000],
          [C+0x23c,4,0xffffffff],[C+0x240,4,0xffffffff],[C+0x244,4,0xffffffff],
          [C+0x2c4,4,0],[C+0x2d4,4,0],[C+0x2d8,4,0],[C+0x300,4,700],
          [C+0x3b4,4,0],[C+0x3b8,4,0],[C+0x3bc,4,0x40400000],[C+0x3c0,4,0x43960000],
          [C+0xd0,2,0x8001],[C+0xd2,2,0x7fff],[C+0xd4,2,0xffff],
          [H+12,4,O],[O+0x10,4,0x42c80000],[O+0x14,4,0x42480000],[O+0x18,4,0x40400000],
          [O+0x1c,4,0x41200000],[O+0x308,4,0],[O+0x30c,4,0],
          [N+0xc,4,0],[N+0x10,4,0x40a00000],[N+0x14,4,0],[N+0x18,4,0x40400000],
          [N+0x1c,4,0x41200000],[N+0x20,4,0x41a00000],[N+0x48,4,0x71000],
          [HOLDER,4,0x72000],[R.G_WORLD,4,0x73000],[R.G_SIM_FRAME,4,1],
          [R.G_LEVEL_MONSTER_LEVEL,4,0x41200000],[R.G_SKILL_GROUNDPOUND,4,0x74000]]
    default=[0,0,0,0,0,0x40400000,0,1]
    cases=[]
    def add(extra,ops,opts=None):cases.append((extra,ops,opts or default))
    for state in list(range(32))+[0xffff,0x8000,0x7fff]:
        for target in (0,H):
            for ev in (0,1,2,3,0x80000000,0xffffffff):
                for play in (0,8,16,0x88):
                    add([[C+0x232,2,state],[C+0x234,4,target],[C+0x11a,2,play]],[[700,ev]])
    for state in (0,1,3,5,16,18,19):
        for mask in (0,1,2,4,8,16,32,0xffffffff):
            for enemies in (0,1,2,4,8,0xffffffff):
                for scenario in (0,1,16,32):
                    add([[C+0x232,2,state],[C+0x234,4,H],[C+0x318,2,3]],[[700,3]],
                        [scenario,0,mask,enemies,1,0x40400000,0x8000,1])
    # Every outgoing service position, callback mutation/re-read and reuse via
    # multiple operations. State callbacks are either observers or real setters.
    for state in (0,1,3,5,6,7,8,9,10,11,12,13,14,16,18,19,20,0x7fff):
        for scenario in (0,1,4,8,16,32,64,128,256,512,1024,2048):
            for fail in range(0,14):
                extra=[[C+0x232,2,state],[C+0x234,4,H],[C+0x318,2,3],[C+0x50,4,HOLDER],
                       [C+0x31a,2,3],[C+0xa8,8,0],[R.G_SIM_FRAME,4,0]]
                add(extra,[[700,3],[700,1]],[scenario,fail,0,0,2,0x40400000,0xffff8123,1])
    # Internal navigation: arrival, path pending, malformed-looking but defined
    # signed indices, path advancement, 16-step cap, side step and teleport.
    rng=random.Random(0x5a454b)
    patterns=(0,0x80000000,0x3f800000,0xbf800000,0x41a00000,0x42c80000,0x7f800000,0x7fc12345)
    for op in (701,702,703,704,705,706,707):
        for scenario in (0,1,4,8,16,32,64,128,256,512,1024,2048):
            for ray in range(5):
                for fail in (0,1,2,3,4,5,8,12,18):
                    extra=[[C+0x234,4,H],[C+0x31a,2,3],[R.G_SIM_FRAME,4,0],[C+0xa8,8,0]]
                    add(extra,[[op,3]],[scenario,fail,0,0,ray,0x40400000,16001,1])
        for index in (-2,-1,0,1,14,15,16,17,0x7fffffff):
            for ray in range(5):
                extra=[[C+0x234,4,H],[C+0x2bc,4,index],[C+0x2c0,4,0],
                       [C+0x23c+8*index,4,30],[C+0x240+8*index,4,20]] if index<16 else [[C+0x234,4,H],[C+0x2bc,4,index]]
                for k in range(16):extra.extend([[C+0x23c+8*k,4,40+k],[C+0x240+8*k,4,20+k]])
                extra.extend([[C+0x2c4,4,5],[R.G_SIM_FRAME,4,64],[C+0x300,4,400],[C+0x2d8,4,400]])
                add(extra,[[op,3]],[512,0,0,0,ray,0x40400000,0,1])
        for _ in range(100):
            extra=[[C+off,4,rng.choice(patterns)] for off in (0x10,0x14,0x18,0x1c,0x30,0xb0,0xb8,0xbc,0xc0,0x3b4,0x3b8,0x3bc,0x3c0)]
            extra += [[C+0x234,4,H],[C+0x2fc,1,rng.choice((0,1,59,60,61,128,255))],
                      [C+0x38,2,rng.getrandbits(16)],[C+0x44,4,rng.getrandbits(32)],
                      [C+0x300,4,rng.choice((0,1,30,31,300,400,0x7fffffff,0x80000000,0xffffffff))]]
            add(extra,[[op,3]],[rng.choice((0,1,4,8,32,256,512)),0,0,0,rng.randrange(5),rng.choice(patterns),rng.getrandbits(32),1])
    # Physical contacts/standing/settling and callback-side pointer/flag changes.
    for cf in (0,0x10,0x400,0x401,0x8000000):
        for of in (0,1,0x10,0x200,0x210):
            for z in (0,0x40000000,0x40400000,0x40800000,0x41a00000):
                for target in (0,H):
                    add([[C+12,4,cf],[N+12,4,of],[N+0x18,4,z],[C+0x234,4,target],[H+12,4,N]],[[706,0],[707,0]],
                        [8|32|4,0,0,0,0,0x40400000,0,1])
    # Charging, home thresholds, cooldown/stun, no-allocation and scaled level.
    for state in (0,3,5,13,14,16,19):
        for x in (0x42c80000,0x42f00000,0x43960000,0x43c80000,0x7fc12345):
            for timer in (0,1,0xffff,0x8000):
                for allocation in (0,1):
                    add([[C+0x232,2,state],[C+0x234,4,H],[C+0x10,4,x],[C+0x314,2,timer],
                         [C+0x318,2,timer],[C+0x31c,2,timer],[O+0x308,4,0x42700000],[O+0x30c,4,0x42700000]],
                        [[700,3]],[32,0,1,1,0,0x40400000,0,allocation])
    # Exact spring turn and speed boundaries, charging threshold, signed attack
    # halving, re-resolved base visibility and virtual dispatch after callbacks.
    for state in (0,1,3,5,8,13):
        for scenario in (32,32|1,4096,8192):
            for speed in (0x42c7ffff,0x42c80000,0x42c80001,0x42f00000):
                for facing in (0,0x3fff,0x4000,0x8000,0xffff):
                    add([[C+0x232,2,state],[C+0x234,4,H],[O+0x38,2,facing],[O+0x308,4,speed],
                         [O+0x30c,4,0],[T+8*25+4,4,0x10e658],[C+0x3ac,2,200],
                         [C+0x3ae,2,400],[C+0x3b0,2,0x7fff]],[[700,3]],
                        [scenario,0,1,0,2,0x40400000,0,1])
    for off in (0,16000,16001,0xffffc180,0xffffc17f,0x8000,0x7fff):
        for slow in (0,1,59,60,61,255):
            for frame in (0,1,8,32,64):
                add([[C+0x2fc,1,slow],[C+0x234,4,H],[C+0x31a,2,1],[R.G_SIM_FRAME,4,frame]],
                    [[701,0],[704,0]], [128,0,0,0,4,0x40400000,off,1])
    for damage in (0,1,0xffff,0x8000,0x8001,0x7fff):
        add([[C+0x232,2,8],[C+0xd0,2,damage],[C+0xd2,2,damage],[C+0x234,4,H]],[[700,3]],
            [1,0,0,0,0,0x40400000,0,1])
    names=('state_request','Creature.v25','Creature.v26','Creature.v27','Creature.v35','Creature.v6','Creature.v10','Creature.v11',
           'Holder.v17','creature_canSeeObj','creature_findEnemyInCone','Creature_hitTargetInArc','Creature_hitAllEnemiesInArc',
           'pool_new','GroundPoundEffect_ctor','handle_link','handle_clear','Creature_deadUpdate',
           'world_slideMove','world_collectObjectsInBox','terrain_heightBelow','obj_updateGridCell','world_raycast',
           'world_findFreeSpot','water_test','water_disturb','libc_sprintf','res_find','sound_playOnObject',
           'path_requestType1','path_requestType2','path_requestType3','path_requestToPoint',
           'SpellDrowsy_typeinfo','creature_findSpellByType','SpellDrowsy_getStat3Fraction',
           'anim_start','anim_startBlendInOut','anim_syncToSimFrame','Creature_dropLoot','Creature.v28','creature_stopBossMusic')
    for extra,ops,opts in cases:
        pokes=base+extra
        if opts[0]&32:
            pokes += [[T+8*k+4,4,fn] for k,fn in ((6,0x11a418),(10,0x11a3f0),(11,0x11a448),(26,0x10bb08),(27,0x3c2688),(35,0x2c3c54))]
            pokes += [[0x71000+8*6+4,4,0x11a418]]
        if opts[0]&64:
            pokes += [[T+8*k,2,0xfffc] for k in (25,26,27,35)]+[[C-4+0xb0,4,0x3f800000]]
        mem=R.RC.Memory()
        for p in pokes:mem.poke(*p)
        m=R.Model(mem);events=[];results=[];count=0;per={}
        if opts[0]&64:m.external.update(('Creature.v25','Creature.v26','Creature.v27','Creature.v35'))
        original=m._w
        def write(a,size,v):events.append(writetext((a,size,v&((1<<(8*size))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def cb(name):
            def fn(*args):
                nonlocal count
                count+=1;per[name]=per.get(name,0)+1;n=per[name]-1;s=opts[0]
                if opts[1] and count==opts[1]:raise RuntimeError('synthetic callback failure')
                if s&1:
                    mem.store(C+0x38,2,0x8123+count);mem.store(C+0x300,4,700+count)
                    mem.store(C+0x11a,2,8 if count&1 else 0);mem.store(C+0xd0,2,0x8000+count)
                    mem.store(C+0x44,4,0x87654321+count);mem.store(C+0x47c,4,8 if count&1 else 6)
                if s&1024:
                    mem.store(R.G_SIM_FRAME,4,64+count);mem.store(C+0x230,1,count)
                    mem.store(C+0x2bc,4,16);mem.store(C+0x234,4,H if count&1 else 0)
                if s&4096:
                    mem.store(T+8*26+4,4,0x10bb08);mem.store(T+8*25+4,4,0x10e658)
                    mem.store(T+8*27+4,4,0x3c2688);mem.store(C+0x3ac,2,200)
                    mem.store(C+0x3ae,2,400);mem.store(C+0x3b0,2,0x7fff)
                if s&8192 and name=='terrain_heightBelow':
                    mem.store(C+0x3b4,4,0x41a00000);mem.store(C+0x3b8,4,0xc1a00000)
                    mem.store(C+0x18,4,0x40000000);mem.store(C+0x44,4,0x80000000+count)
                if name=='state_request':
                    if s&16:m.zek_set_state(*args)
                    else:mem.store(C+0x232,2,args[1])
                if name in ('creature_canSeeObj','Creature.v25'):return (opts[2]>>(n%32))&1
                if name=='creature_findEnemyInCone':return O if (opts[3]>>(n%32))&1 else 0
                if name=='Creature.v26':return R.s32(opts[6])
                if name=='Creature.v35':return 3
                if name=='Holder.v17':return 0x75000+count
                if name=='pool_new':return 0x76000 if opts[7] else 0
                if name=='world_collectObjectsInBox':return [N,C,N+0x100] if s&8 else []
                if name=='world_slideMove':
                    if s&4:
                        for k in range(3):mem.store(args[0]+4*k,4,R.C.add(mem.load(args[0]+4*k,4),args[1][k]))
                    return (1,(0x3f800000,0x80000000,0)) if args[3] else 0
                if name=='terrain_heightBelow':return opts[5]
                if name=='world_raycast':
                    t=(0x3f800000,0xbf800000,0x3e800000,0x40000000,(0x3e800000,0x3f000000,0x3f400000)[n%3])[opts[4]]
                    return (t,N if n&1 else 0) if args[4] else t
                if name=='world_findFreeSpot':return ((0x41200000,0xc1200000,0x40400000),0xff if s&512 else 0)
                if name in ('water_test','water_disturb'):return 1 if s&256 else 0
                if name=='libc_sprintf':return 'synthetic-water'
                if name=='res_find':return 0x77000
                if name in ('SpellDrowsy_typeinfo','creature_findSpellByType'):return 0x78000 if s&128 else 0
                if name=='SpellDrowsy_getStat3Fraction':return 0x3f000000
                if s&2048 and name=='Creature.v6':
                    mem.store(C+0x1c,4,0x41a00000);mem.store(C+12,4,0x8000000);mem.store(C+0x234,4,H);mem.store(H+12,4,N)
                return 0
            return fn
        for name in names:m.cb[name]=cb(name)
        for op,arg in ops:
            try:
                if op==700:m._zek_switch(C,arg,lambda c,st:m.call('state_request',c,st));result='ok'
                elif op==701:result='r'+str(m.move_to(C,(0x43480000,0x42c80000,0x40400000)))
                elif op==702:m.move_to_target(C);result='ok'
                elif op==703:result='r'+str(m.move_to_dest(C))
                elif op==704:m.flee_from_target(C,arg);result='ok'
                elif op==705:result='r'+str(m.is_away_from_home(C))
                elif op==706:result='r'+str(m.move_by(C,(0x42700000,0xc2700000,0)))
                elif op==707:m.settle(C);result='ok'
                results.append(result)
            except RuntimeError:results.append('callback_error');break
        yield pokes,ops,opts,snapshot(m,events,results)


TICK_NAMES = '''world_findObjectByName model_getSamplePointCount model_getSamplePoint Spell.v1 Spell.v2 terrain_height world_collectObjectsInArc Creature.v16 Creature.v17 Creature.v19 Creature.v20 Creature.v23 Creature.v25 Creature.v26 Creature.v27 Creature.v28 Creature.v35 Creature.v37
Holder.v15 Holder.v17 Holder.v21 Holder.v22 Holder.v11 Skill.v1 Skill.v2 Skill.v11 Skill.v12 Skill.v14 SkillLifeTap_getStat0Fraction
world_notifyObjectsNear model_getBounds fx_spawn libc_sprintf res_find sound_playOnObject model_getAttachPoint particle_spawn pool_new GutChunk_ctor PhProp_initPhysics decal_add fx_shatterToLooseIce fx_disintegrateModel
obj_getPropInt obj_getPropStr anim_snapTopToEnd player_getSkillRank player_giveExp OniBall_ctor
iter_begin iter_next iter_end pathFollower_init paths_findByName pointList_snapToNearest pointList_getPoint terrain_raycast players_anyWithinRadius obj_updateGridCell obj_runEveryFrame
net_objOrGhost fx_burnModel fx_shockModel damage_apply creature_bossRank Critter_ctor handle_clear handle_link handle_removeNode script_runByName anim_releaseCaches player_speedScale anim_advance Creature_applyKnockback
creature_canSeeObj creature_findEnemyInCone creature_countAlliesNear creature_isBlockedBy Creature_hitTargetInArc Creature_hitAllEnemiesInArc Creature_deadUpdate world_collectObjectsInBox world_slideMove terrain_heightBelow world_raycast world_findFreeSpot water_test water_disturb
path_requestType1 path_requestType2 path_requestType3 path_requestToPoint SpellDrowsy_typeinfo creature_findSpellByType SpellDrowsy_getStat3Fraction
anim_start anim_startBlendInOut anim_syncToSimFrame anim_push anim_jumpTopToEnd Creature_dropLoot creature_stopBossMusic GroundPoundEffect_ctor fx_ringBurst LightEffect_ctor
'''.split()

def tick_vectors(R, dispatch=False):
    H,O,MOUNT,HOLDER,SPELL,NODE=0x40000,0x50000,0x60000,0x70000,0x80000,0x90000
    base=[[C+0x24,4,D],[D+12,4,0xa0000],[C+0x48,4,T],[C+0x18,4,0x40400000],
          [C+0x1c,4,0x41200000],[C+0x20,4,0x41a00000],[C+0x3c,4,0x40400000],
          [C+0x38,2,0x8123],[C+0x44,4,0x12345678],[C+0xb0,4,0x3f800000],[C+0xb8,4,0x42700000],
          [C+0xbc,4,0x3e800000],[C+0xc0,4,0xbe800000],[C+0xc4,2,0x8123],[C+0xdc,4,0x3f800000],
          [C+0xe0,4,0x3f800000],[C+0xd0,2,0x8001],[C+0xd2,2,0x7fff],[C+0xd4,2,0xffff],
          [C+0xd8,4,0x42c80000],[C+0x228,4,0x42c80000],[C+0x22c,4,0x43480000],
          [C+0x300,4,700],[C+0xf4,4,800],[C+0x3b4,4,0],[C+0x3b8,4,0],[C+0x3bc,4,0x40400000],
          [C+0x3c0,4,0x43960000],[C+0x3c4,2,300],[C+0x3ac,2,100],[C+0x3ae,2,400],
          [C+0x3b0,2,0x7fff],[C+0x23c,4,0xffffffff],[C+0x240,4,0xffffffff],[C+0x2bc,4,16],
          [H+12,4,O],[O+12,4,0x40],[O+0x48,4,0xb0000],[O+0x10,4,0x42c80000],[O+0x14,4,0],
          [O+0x18,4,0x40400000],[O+0x1c,4,0x41200000],[O+0x20,4,0x41a00000],[O+0x310,1,1],
          [MOUNT+0x10,4,0x42480000],[MOUNT+0x14,4,0xc2480000],[MOUNT+0x18,4,0x40400000],
          [MOUNT+0x38,2,0x4000],[MOUNT+0x234,4,H],[H+0x100+12,4,MOUNT],[HOLDER,4,0xc0000],
          [R.G_WORLD,4,0xd0000],[R.G_SIM_FRAME,4,1],[R.RC.G_ACTIVITY_COUNT,1,1],
          [R.RC.G_ACTIVITY_CENTERS+8,4,0x40400000],[R.RC.G_NUM_PLAYERS,4,1],
          [R.RC.G_PLAYERS+R.RC.REC_OBJ,4,O],[R.RC.G_PLAYERS+0x59,1,10],
          [R.RC.G_PLAYERS+0xac,4,0x42c80000],[R.RC.G_PLAYERS+0xc4,4,0x42c80000],
          [R.RC.G_EXP_TABLE+40,4,1000],[R.RC.G_EXP_TABLE+44,4,2000],
          [R.RC.G_LOCAL_ALIVE_COUNT,4,1],[R.RC.G_LOCAL_ALIVE_LIST,4,0],
          [R.RC.G_FOLLOW_TELEPORT_DIST,4,0x42c80000],[R.G_LEVEL_MONSTER_LEVEL,4,0x41200000],
          [R.G_SKILL_SUMMONSKELETON,4,0xe0000],[0xe0000+0x60,4,0xe1000],
          [R.G_SKILL_GROUNDPOUND,4,0xe2000],[R.G_ITEM_MODELS+8,4,0xe3000],
          [0xb0000+8*37+4,4,0x3c2730]]
    base += [[T+8*k+4,4,fn] for k,(fn,_) in R.RC.BASE_BODIES.items()]
    base += [[T+8*16+4,4,0x3c2738],[T+8*25+4,4,0x10e658]]
    default=[0,0,3,0,0,0,0x3f800000,0]
    cases=[]
    def add(extra,ops,opts=None):cases.append((extra,ops,opts or default))
    # All AI branches, active/inactive, target and playing/event priorities.
    for op in (800,801,802):
        for state in list(range(40))+[0xffff,0x8000]:
            for target in (0,H):
                for play in (0,8,16,0x80,0x88,0x90):
                    for ev in (0,1,2,3):
                        add([[C+0x232,2,state],[C+0x234,4,target],[C+0x11a,2,play]],[[op,0]],[0,0,ev,0,0,0,0x3f800000,0])
        for state in (0,1,3,5,6,8,16,18,19,20,23,26,31,35):
            for scenario in (1,2,4,8,16,32,64,128,256,512,1024):
                for fail in (0,1,2,3,5,8,12,20,40):
                    add([[C+0x232,2,state],[C+0x234,4,H],[C+0x50,4,HOLDER],[C+0x470,1,state%4],
                         [C+0xc8,4,0x3f800000],[C+0x2dc,4,0x80001]],[[op,0],[op,0]],
                        [scenario,fail,3,0xaaaaaaaa,1,1,0x3f800000,0])
    # Mounted knight position/target handoff and all mounted states.
    for state in range(40):
        for mine in (0,H,H+0x200):
            for play in (0,8,0x88,0x90):
                add([[C+0x232,2,state],[C+0x3c8,4,H+0x100],[C+0x234,4,mine],[C+0x11a,2,play]],[[800,0]])
    # Pending-damage routes, material priorities, random streams, death/XP.
    for state in (0,18,20,23):
        for hp in (0,0xbf800000,0x42c80000):
            for amount in (0,0x3f800000,0xbf800000,0x7fc12345):
                for flags in (0,1,4,8,0x2000,0x400000,0x40000,0x80000,0x100000,0x200000,0x380000,0x2000000,0x10,0x20,0x80801):
                    add([[C+0x232,2,state],[C+0x228,4,hp],[C+0xc8,4,amount],[C+0x2dc,4,flags]],[[803,0]])
    for cf in (0,1,0x400,0x1000,0x2000,0x3000,0x4000,0x8000,0x10000,0x20000,0x40000,0x20000000,0x100000000):
        for seed in (0,1,0x12345678,0xffffffff):
            for hp in (0,0x42c80000):
                for scenario in (0,4,16,32,128):
                    add([[C+0xa8,8,cf],[C+0x44,4,seed],[C+0xc8,4,0x3f800000],[C+0x228,4,hp],
                         [C+0x2dc,4,0x80801],[R.RC.G_DECAL_DETAIL,4,1]],[[803,0]],
                        [scenario,0,3,0,0,1,0x3f800000,0])
    # Timers, status ramp, raw patterns, signed narrowing and clock pause.
    patterns=(0,1,0x80000000,0x3c888888,0x3d888888,0x3f800000,0x42700000,0x7f800000,0x7fc12345)
    for off in (0x340,0x344,0x348,0x34c,0x350,0x354,0x60,0x6c):
        for raw in patterns:
            for flags in (0,8,0x40,0x20000,0x4000000):
                for frame in (0,1,31,32,0xffffffff):
                    add([[C+off,4,raw],[C+12,4,flags],[R.G_SIM_FRAME,4,frame]],[[804,0]])
    for timer in (0,1,2,0x7fff,0x8000,0xfffe,0xffff):
        for slow in (0,1,59,60,61,128,255):
            for counter in (0,1,30,31,0xffffffff,0x80000000):
                add([[C+o,2,timer] for o in (0x312,0x314,0x316,0x318,0x31a,0x31c)]+
                    [[C+0x2fc,1,slow],[C+0x388,4,counter],[C+0xf0,4,0x80000000]],[[804,0]])
    # Activity boundaries and signed integer radius overflow, cursor, transitions.
    for radius in (0,1,100,46340,46341,65535):
        for cf in (0,0x4000000000000000,0x8000000000000000,0xc000000000000000):
            for scenario in (0,1,2,4,16):
                add([[C+o,2,radius] for o in (0x3c4,0x3ac,0x3ae)]+
                    [[C+0xa8,8,cf],[C+12,4,0xc00000],[C+4,4,NODE],[R.RC.G_RUN_LIST_60_CURSOR,4,59]],
                    [[805,0],[805,0]],[scenario,0,3,0,0,0,0x3f800000,1])
    # Animation world rotation, active/inactive stale root, slow/Drowsy, events.
    for state in (0,3,6):
        for slow in (0,1,30,59,60,61,128,255):
            for facing in (0,0x3fff,0x4000,0x8000,0xffff):
                for scenario in (0,1,4,8,16,64):
                    add([[C+0x232,2,state],[C+0x2fc,1,slow],[C+0x38,2,facing],[C+0x31a,2,2],
                         [C+0xa8,8,0 if scenario&1 else 0x8000000000000000]],[[806,0],[806,0]],
                        [scenario,0,3,0,0,0,0x3f800000,0])
    # Follow path/player, boss meter, scavenger and attached spell iteration.
    for cf in (0x8000000,0x400000,0x800000,0x8000000000400000,1,0x8000000080000001,0x8000000000000000):
        for state in (0,5,20):
            for scenario in (0,1,4,16,32,64,128,256,512,1024,2048):
                for fail in (0,1,3,7,12,20):
                    add([[C+0xa8,8,cf],[C+0x232,2,state],[C+0x312,2,0xffff],[C+0x11a,2,0x80],
                         [C+0x5a,1,0],[C+0x58,2,0xffff],[C+0x54,4,0],[C+0x310,1,0xff]],
                        [[804,0]],[scenario,fail,3,0,0,1,0x3f800000,1])
    # Spell bodies execute internally, never a fabricated Creature_run callback.
    for spell_fn in (0,0x2e177c,0x30cbc8):
        for fail in range(1,16):
            add([[NODE+12,4,SPELL],[SPELL+0x14,4,0x91000],[0x91000+20,4,spell_fn],
                 [SPELL+0xc,4,C],[SPELL+8,4,0xe0000],[SPELL+0x80,2,0xffff],[SPELL+0x7c,4,0x3f800000]],
                [[804,0]],[2048,fail,0,0,0,0,0x3f800000,0])
    # Several players: XP caps, dead shares, duplicate killer last-slot semantics.
    for players in (0,1,2,3):
        for killer in (0,H):
            for fail in range(0,16):
                extra=[[R.RC.G_NUM_PLAYERS,4,players],[C+0xc8,4,0x3f800000],[C+0x228,4,0],[C+0x2e4,4,killer]]
                for k in range(players):extra += [[R.RC.G_PLAYERS+k*R.RC.REC_SIZE+R.RC.REC_OBJ,4,O],
                    [R.RC.G_PLAYERS+k*R.RC.REC_SIZE+0x59,1,10],[R.RC.G_PLAYERS+k*R.RC.REC_SIZE+0xac,4,0 if k&1 else 0x3f800000]]
                add(extra,[[803,0]],[0,fail,0,0,0,1,0x3f800000,0])
    # Exact death ice gate, scavenger construction/ray search, real state setters,
    # path teleport, first boss display, and skill-v1's internal summon predicate.
    for seed in range(32):
        for model in (0,0xa0000):
            add([[C+0x44,4,seed],[D+12,4,model],[C+0xc8,4,0x3f800000],
                 [C+0x228,4,0],[C+0x344,4,0x3f800000]],[[803,0]])
    for kind in (1,2,3):
        for ray in (0,0x3e800000,0x3f800000,0xbf800000,0x40000000):
            for fail in (0,1,2,3,8,20,36):
                add([[C+0xa8,8,0x8000000000000000],[C+0x232,2,20],[C+0x11a,2,0x80],
                     [R.RC.G_SCAVENGER_KIND,4,kind]],[[804,0]],[0,fail,0,0,0,1,ray,0])
    for op,fn in ((800,0x272f4c),(801,0x2a91b8),(802,0x2c382c)):
        for state in (0,3,5,6,20,23):
            for target in (0,H):
                for fail in range(12):
                    add([[T+8*27+4,4,fn],[C+0x232,2,state],[C+0x234,4,target],
                         [C+0x11a,2,0x80]],[[op,0]],[0,fail,1,1,0,0,0x3f800000,0])
    for scenario in (0,1,2,4,16):
        add([[C+0xa8,8,0x8000000],[C+0x58,2,0],[C+0x54,4,0],
             [0xfc000,4,0x43c80000],[0xfc000+0x3c,2,0]],[[804,0]],
            [scenario,0,0,0,0,0,0x3f800000,1])
    add([[C+0xa8,8,0x8000000000000001]],[[804,0],[804,0]])
    add([[C+0x232,2,3],[C+0x234,4,H],[C+0x470,1,3],[0xe1000+12,4,0x2e2368]],[[800,0]])
    for players in (2,3):
        extra=[[R.RC.G_NUM_PLAYERS,4,players],[C+0xc8,4,0x3f800000],[C+0x228,4,0],[C+0x2e4,4,H]]
        for k in range(players):extra += [[R.RC.G_PLAYERS+k*R.RC.REC_SIZE+R.RC.REC_OBJ,4,O],
            [R.RC.G_PLAYERS+k*R.RC.REC_SIZE+0x59,1,80],[R.RC.G_EXP_TABLE+320,4,100],
            [R.RC.G_PLAYERS+k*R.RC.REC_SIZE+0xac,4,0x3f800000]]
        add(extra,[[803,0]],[8192,0,0,0,0,1,0x3f800000,0])
    for extra,ops,opts in cases:
        if dispatch and any(op not in (800,801,802) for op,_ in ops):continue
        pokes=base+extra
        if dispatch:opts=list(opts);opts[0]|=65536
        if dispatch:pokes += [[T+8*4+4,4,{800:0x27236c,801:0x2a8b84,802:0x2c2440}[ops[0][0]]]]
        if opts[0]&1:pokes += [[R.RC.G_ACTIVITY_COUNT,1,0],[R.RC.G_CULL_BY_CAMERA,1,0]]
        if opts[0]&2:pokes += [[R.RC.G_CULL_BY_CAMERA,1,1],[R.RC.G_CAMERA_FOCUS,4,0x42c80000]]
        if opts[0]&8:pokes += [[C+0xa8,8,0x8000000002000000]]
        if opts[0]&256:pokes += [[C+0x3c0,4,0],[C+0x5a,1,0xff],[R.RC.G_LEVEL_FLAGS,4,0x100]]
        if opts[0]&512:pokes += [[C+0x50,4,HOLDER]]
        if opts[0]&1024:pokes += [[T+8*27+4,4,0x2c382c],[C+0x31c,2,1]]
        mem=R.RC.Memory()
        for p in pokes:mem.poke(*p)
        m=R.Model(mem);events=[];results=[];count=0;per={}
        if opts[0]&16:m.external.update(('Creature.v16','Creature.v17','Creature.v20','Creature.v23','Creature.v25','Creature.v26','Creature.v27'))
        original=m._w
        def write(a,size,v):events.append(writetext((a,size,v&((1<<(8*size))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        def cb(name):
            def fn(*args):
                nonlocal count
                count+=1;per[name]=per.get(name,0)+1;n=per[name]-1;s=opts[0]
                if opts[1] and count==opts[1]:raise RuntimeError('synthetic callback failure')
                if s&4:
                    mem.store(C+0x38,2,0x8123+count);mem.store(C+0x300,4,700+count)
                    mem.store(C+0x11a,2,8 if count&1 else 0);mem.store(C+0x44,4,0x87654321+count)
                    mem.store(C+0x2dc,4,0x400000 if count&1 else 0x80000);mem.store(C+0x228,4,0x42c80000)
                if s&32:
                    mem.store(R.G_SIM_FRAME,4,count*32);mem.store(C+0x230,1,count)
                    mem.store(C+0x234,4,H if count&1 else 0);mem.store(C+0x3c8,4,H+0x100)
                if s&8192 and name=='player_giveExp':mem.store(R.RC.G_NUM_PLAYERS,4,1)
                if name=='Creature.v27':
                    mem.store(args[0]+0x232,2,args[1])
                    if s&65536:return (-7,b'\x00\xff',[0,None])
                if name=='Creature.v16':return 0xa0000
                if name in ('Creature.v20','Creature.v25','creature_canSeeObj'):return (opts[3]>>(n%32))&1
                if name=='Creature.v26':return 0
                if name=='creature_findEnemyInCone':return O if opts[4] else 0
                if name in ('creature_countAlliesNear','creature_isBlockedBy'):return opts[4]
                if name=='Creature.v35':return 3
                if name=='Creature.v37':return 1
                if name=='pool_new':return 0xf0000+n*0x1000 if opts[5] else 0
                if name=='model_getSamplePointCount':return 0
                if name=='model_getBounds':return (0,0,0xfffffff6,0,0,20)
                if name=='model_getAttachPoint':return (0x3f800000,0x40000000,0x40400000)
                if name=='libc_sprintf':return 'synthetic-effect'
                if name=='res_find':return 0xf8000
                if name=='obj_getPropInt':return opts[7]
                if name=='obj_getPropStr':return 0xfa000 if opts[7] else 0
                if name=='net_objOrGhost':return C
                if name=='creature_bossRank':return 1 if s&128 else 0
                if name=='player_getSkillRank':return 3
                if name=='SkillLifeTap_getStat0Fraction':return 0x3f000000
                if name in ('Skill.v11','Skill.v12'):return 10
                if name=='Skill.v14':return 3
                if name=='Skill.v1':return opts[4]
                if name=='iter_begin':return NODE if s&2048 else 0
                if name=='iter_next':return 0
                if name=='paths_findByName':return 0xfb000
                if name=='pointList_snapToNearest':return 0
                if name=='pointList_getPoint':return 0xfc000 if n==0 else 0
                if name=='terrain_raycast':return opts[6]
                if name=='players_anyWithinRadius':return 1 if s&64 else 0
                if name=='player_speedScale':return 0x3f000000
                if name=='anim_advance':
                    if s&64:
                        mem.store(C+0xbc,4,0x3f800000);mem.store(C+0xc0,4,0xbf800000);mem.store(C+0xc4,2,0xffff)
                        mem.store(C+0x2fc,1,60);mem.store(C+0xdc,4,0x40000000)
                    return opts[2]
                if name=='Holder.v15':return 0xffffffff
                if name=='Holder.v17':return 0xfd000
                if name=='world_collectObjectsInArc':return []
                if name=='world_collectObjectsInBox':return [O,0,O] if args[2]==0xc0 else []
                if name=='world_slideMove':return (0,(0,0,0)) if args[3] else 0
                if name=='terrain_heightBelow':return 0x40400000
                if name=='world_raycast':return (0x3f800000,0) if args[4] else 0x3f800000
                if name=='world_findFreeSpot':return ((0,0,0),0)
                if name in ('SpellDrowsy_typeinfo','creature_findSpellByType'):return 0xfe000
                if name=='SpellDrowsy_getStat3Fraction':return 0x3f000000
                return 0
            return fn
        for name in TICK_NAMES:m.cb[name]=cb(name)
        for op,arg in ops:
            try:
                if dispatch:result=shape_value(m.vcall(C,4))
                elif op==800:m.horsemen_run(C);result='ok'
                elif op==801:m.skelight_run(C);result='ok'
                elif op==802:m.zek_run(C);result='ok'
                elif op==803:m.process_damage(C,m.class_model(C));result='ok'
                elif op==804:result='r'+str(m.run(C,m.class_model(C)))
                elif op==805:result='r'+str(m.update_active(C))
                elif op==806:result='r'+str(m.step_anim(C,R.ONE))
                results.append(result)
            except RuntimeError:results.append('callback_error');break
        yield pokes,[[op+100,arg] for op,arg in ops] if dispatch else ops,opts,snapshot(m,events,results)


def dispatch_vectors(R):
    # Complete body/slot resolution with signed adjustments, redirects and
    # external overrides. Defaults are reference observations, never services.
    cases=[]
    specs=[]
    for fn,(_,name) in R.CLASS_BODIES.items():
        if name.endswith('_run'):continue # exercised with complete tick fixtures below
        args={'horsemen_set_state':[23],'skelight_set_state':[23],'zek_set_state':[23],
              'zek_take_damage':[0,0,0],'zek_slot35':[],'slot16_model':[],
              'slot25_can_see_target':[0],'slot28_on_death':[1],'slot34_set_home_range':[0xffff8001]}[name]
        specs.append((0,fn,R.CLASS_BODIES[fn][0],args))
    for slot,(fn,name) in R.RC.BASE_BODIES.items():
        args={2:[0,0,0],6:[0x50000],7:[0],10:[(1,2,3)],11:[[]],17:[(0,0,0)],
              20:[0],23:[0],26:[0,(0,0,0)],27:[0xffff8001],30:[2,1],31:[0xffff],35:[],36:[],37:[]}[slot]
        specs.append((0,fn,slot,args))
    specs += [(1,0x2e2368,1,[0x50000]),(1,0x2e18a4,2,[1,0x50000,(0,0,0),0]),
              (1,0x30c708,2,[1,0x50000,(0,0,0),0]),
              (2,0x2e177c,2,[]),(2,0x30cbc8,2,[]),
              (2,0x2e177c,2,[None,None]),(2,0x30cbc8,2,[None,None]),
              (1,0x30c708,2,[1,0x50000,(0,0,0),None])]
    for kind,fn,canonical,args in specs:
        for slot in sorted(set((canonical,16,27,42))):
            for adjust in (0,4,0xfffc,0x8000,0x7fff):
                for external in (0,1):
                    for callback in (0,1,2,3):
                        self=(C+(adjust if adjust<0x8000 else adjust-0x10000))&0xffffffff
                        offset=(0x48,0x60,0x14)[kind]
                        pokes=[[C+offset,4,T],[T+8*slot,2,adjust],[T+8*slot+4,4,fn],
                               [self+0x24,4,D],[D+12,4,0xabcdef01],[self+0x310,1,0xff],
                               [self+0xa8,8,0x100000],[self+0x3c0,4,0x43960000],
                               [self+0x48,4,T],[0x4f714c,4,0x41200000],[0x4f711c,4,0],
                               [0x50000+12,4,0x40],[0x50000+0x310,1,1]]
                        # Ensure table pointer when adjusted creature +0x48 aliases it.
                        pokes.append([C+offset,4,T])
                        cases.append((kind,pokes,slot,args,external,callback))
    # set_state is an exact forwarding wrapper, including unknown targets.
    for fn in (0x272f4c,0x2a91b8,0x2c382c,0x3c2688,0x12345678):
        for state in (0,5,23,0xffff8001,0xffffffff):
            for callback in (0,1,2,3):
                cases.append((3,[[C+0x48,4,T],[T+8*27+4,4,fn]],27,[state],0,callback))
    # Arbitrary class table callbacks never run creature or skill bodies.
    for kind in (4,5):
        for fn in (0,0x2e177c,0x30cbc8,0x3c2738,0x2e2368):
            for adjust in (0,0xffff,0x8000):
                for callback in (0,1,2,3):
                    cases.append((kind,[[C+0x20,4,T],[T+8*2,2,adjust],[T+8*2+4,4,fn]],2,[1,(0,0,0)],0,callback))
    for kind,pokes,slot,args,external,callback in cases:
        mem=R.RC.Memory()
        for p in pokes:mem.poke(*p)
        m=R.Model(mem);events=[]
        original=m._w
        def write(a,size,v):events.append(writetext((a,size,v&((1<<(8*size))-1))));original(a,size,v)
        m._w=write
        original_call=m.call
        def call(name,*args):
            args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
            events.append(calltext(name,args));return original_call(name,*args)
        m.call=call
        name=('Creature','Skill','Spell','Creature','Holder','Custom')[kind]+'.v'+str(slot)
        if external:m.external.add(name)
        if callback:
            def cb(*a):
                if callback==2:raise RuntimeError('synthetic callback failure')
                if callback==3:
                    mem.store(C+0x232,2,0x1234)
                    m.cb[name]=lambda *a: -7
                    return -9
                return -1
            m.cb[name]=cb
        try:
            if kind==0:result=m.vcall(C,slot,*args)
            elif kind==1:result=m.skill_call(C,slot,*args)
            elif kind==2:result=m.table_call('Spell',C,0x14,slot,*args)
            elif kind==3:result=m.set_state(C,*args)
            else:result=m.table_call('Holder' if kind==4 else 'Custom',C,0x20,slot,*args)
            result=shape_value(result)
        except (RuntimeError,ValueError,TypeError,KeyError,IndexError):result='error'
        yield pokes, [kind,slot,external,callback], args, snapshot(m,events,[result])


def call_vectors(R):
    values=(0,1,-1,0x100000001,None,b'',b'\x00\xff',[],[0],(),(0,))
    for name in sorted(R.EXTERNALS):
        for value in values:
            args=[value]*16
            for callback in (0,1,2):
                mem=R.RC.Memory();m=R.Model(mem)
                if callback==1:m.cb[name]=lambda *a:(-1,b'\x00\xff',[0,None])
                if callback==2:
                    def fail(*a):raise RuntimeError('synthetic callback failure')
                    m.cb[name]=fail
                try:out=shape_value(m.call(name,*args))
                except (RuntimeError,IndexError):out='error'
                calls=shape_value(m.calls)
                yield name,args,callback,out+'|'+calls


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
                    # Preserve the original prefix; resolved wander is appended below.
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
                # Retain the historical prefix exclusion; appended cases cover resolved wander.
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
        for pokes,commands,options,expected in zek_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'zek:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['zek']=counts.get('zek',0)+1
        for pokes,commands,options,expected in horsemen_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'horsemen:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['horsemen']=counts.get('horsemen',0)+1
        for pokes,commands,options,expected in zek_damage_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'zek:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['zek_damage']=counts.get('zek_damage',0)+1
        for pokes,commands,options,expected in skelight_state_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'zek:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['skelight_state']=counts.get('skelight_state',0)+1
        for name,args,options,expected in shape_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'shape:'+name, shape_value(args), shape_value(options), expected]))
            counts['shape_result']=counts.get('shape_result',0)+1
        for pokes,commands,options,expected in navigation_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'navigation:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['zek_switch_navigation']=counts.get('zek_switch_navigation',0)+1
        for pokes,commands,options,expected in tick_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'tick:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['monster_ticks']=counts.get('monster_ticks',0)+1
        for pokes,commands,options,expected in tick_vectors(R,dispatch=True):
            rows.append('\t'.join([str(len(rows)), 'tick:'+packed(pokes), packed(commands), packed([options]), expected]))
            counts['dispatch_ticks']=counts.get('dispatch_ticks',0)+1
        for pokes,options,args,expected in dispatch_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'dispatch:'+packed(pokes), packed([options]), shape_value(args), expected]))
            counts['dispatch_bodies']=counts.get('dispatch_bodies',0)+1
        for name,args,callback,expected in call_vectors(R):
            rows.append('\t'.join([str(len(rows)), 'dispatchcall:'+name, shape_value(args), str(callback), expected]))
            counts['dispatch_calls']=counts.get('dispatch_calls',0)+1
        for label,factory in (('tagar',tagar_vectors),('summon',summon_vectors)):
            for pokes,commands,options,expected in factory(R,dispatch=True):
                rows.append('\t'.join([str(len(rows)), label+':'+packed(pokes), packed(commands), packed([options]), expected]))
                counts['dispatch_'+label]=counts.get('dispatch_'+label,0)+1
        data=('# arena-monsters-v1\n'+'\n'.join(rows)+'\n').encode()
        a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data)
        a.output.with_suffix('.json').write_text(json.dumps({'schema':'arena-monsters-v1','research_commit':COMMIT,
            'references':PINS,'reference_function_count':len(units),'ported_units':PORTED_UNITS,'cases':len(rows),'operations':counts,'vector_sha256':hashlib.sha256(data).hexdigest(),
            'scope':'Complete shared dispatch/helper family; Complete Horsemen, Skelight and Zek ticks with inherited damage/status/activity/animation pipeline; Complete Zek AI switch and inherited navigation/movement; Complete callback result marshalling for all 139 EXTERNALS signatures; Complete Skelight state entry including wander; filter_state; shatter; complete arena creature base family; complete ArenaScript and scaled_time; complete Tagar cast/spell family; complete Summon cast/spell family and Skelight constructor; complete shared data/holder and signed attack-scaling helpers; complete Zek construction, state entry, specials and wave lifecycle; complete Horsemen construction, mounted state entry and variant casts; complete Zek damage intake and spark lifecycle',
            'observations':'all sparse bytes, memory store log, model writes/calls/events, shared draw count, results'},indent=2)+'\n')
        print(f'{len(rows)} exact reference cases')
if __name__=='__main__': main()
