#!/usr/bin/env python3
"""Generate authored inputs and full expected state from immutable research Python."""
import argparse
import ast
import gzip
import hashlib
import importlib
import json
import pathlib
import random
import subprocess
import sys
import tempfile

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
 'tools/ref_skills_b.py': 'b71cbdd9e82cd72abb855b9f6ab99d11fb78a58613cb69d314a5041457bbdd95',
 'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
 'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
 'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
 'tools/difftest_skills_b.py': '06778b17758b7b12e30df0a0640706f456f4df58a3e39dda67cf28f3d1c39b47',
 'docs/spec/skills-35-68.md': 'a7092cba387542fd7e926145f245ddfcb5339a374442b478e1d810f52049eb6b',
 'docs/spec/skills-35-68-executed.md': '698400358c3ca2f221c9e79fa19bf64703006069925f01f2e983a5706fd681f5',
}
S, T, O, H, N, L, B = 0x10000, 0x20000, 0x30000, 0x40000, 0x41000, 0x50000, 0x60000
BODIES = {1:0x3c8218, 2:0x3c8220, 5:0x3c82a8, 8:0x3c82e4, 10:0x3c85fc, 11:0x3c8690,
          12:0x3c87f0, 13:0x3c8864, 14:0x3c88dc, 15:0x3c88e4}
MASK = 0xffffffff

def flat(records):
    return ';'.join(','.join(map(str, r)) for r in records)

def state(model, results, status):
    memory = flat(sorted(model.mem.bytes.items()))
    writes = flat(model.mem.log)
    def arg(a):
        if isinstance(a, tuple): return 'v:' + ':'.join(str(v & MASK) for v in a)
        if isinstance(a, str): return 's:' + a
        return str(a & MASK)
    calls = ';'.join(name + ',' + ','.join(arg(a) for a in args) for name,args in model.calls)
    return '|'.join((status, ','.join(map(str,results)), str(model.draws), memory, writes, calls))

class CallbackFailure(Exception): pass
class Missing(Exception): pass

METHODS = {
 'stat':'skill_get_stat', 'tier':'skill_get_stat_tier2', 'for':'skill_get_stat_for',
 'handle':'skill_get_stat_for_handle', 'mana':'skill_mana_from_stat4', 'range':'skill_stat_range',
 'archery':'archery_fraction', 'lifetap':'lifetap_fraction', 'life1':'lifetap_stat1',
 'stance':'strongstance_stat0', 'resist':'naturalresistance_fraction', 'stun':'stunningstrike_stat0_for',
 'ctor':'spell_base_ctor', 'spellrand':'spell_rand', 'objectrand':'obj_rand',
 'shared':'draw', 'cosmetic':'cosmetic_draw', 'third':'seed3_draw',
 'adjust':'skill_adjust_level', 'can':'skill_can_use_base', 'cast':'skill_cast_base',
 'cost':'skill_mana_cost_base', 'virtual':'skill_call',
 'link':'handle_link', 'push':'list_push', 'remove':'handle_removeNode',
 'clear':'handle_clear', 'begin':'iter_begin', 'next':'iter_next', 'end':'iter_end',
 'attach':'creature_add_spell', 'headobj':'handle_obj',
 'update':'creature_update_spells', 'finish':'spell_end', 'projectile':'projectile_ctor',
 'gamector':'game_object_ctor', 'move':'obj_move_and_collide', 'spellvirtual':'spell_call', 'objvirtual':'vcall',
 'alloc':'alloca', 'objects':'list_objects',
 'minorcast':'minorhealing_cast',
 'minorctor':'minorhealing_spell_ctor',
 'minorrun':'minorhealing_run',
 'shieldcast':'holyshield_cast',
 'shieldctor':'holyshield_spell_ctor',
 'shieldrun':'holyshield_run',
 'absorb':'holyshield_absorb',
 'healcast':'healing_cast',
 'healctor':'healing_spell_ctor',
 'healrun':'healing_run',
 'healshare':'_heal_share',
 'record':'record',
 'unit':'unit',
 'signedunit':'signed_unit',
 'helperstat':'stat',
 'helperfor':'stat_for',

}

def run(SB, config, pokes, hooks, commands):
    SB.C.set_fpu(('ps2','ps2g1','chop','nearest')[config//4], bool(config&2), bool(config&1))
    model = SB.Model(SB.RC.Memory())
    for addr,size,value in pokes: model.mem.poke(addr,size,value)
    for name,ret,mutation,throws,*action in hooks:
        sequence = str(ret).split(':')[1:] if str(ret).startswith('q:') else None
        cursor = [0]
        def hook(*args, ret=ret, mutation=mutation, throws=throws, action=action, sequence=sequence, cursor=cursor):
            value = ret
            if sequence:
                value = int(sequence[min(cursor[0],len(sequence)-1)])
                cursor[0] += 1
            elif str(ret).startswith('l:'): value = [int(x) for x in str(ret)[2:].split(':') if x]
            elif str(ret).startswith('p:'):
                parts=[int(x) for x in str(ret)[2:].split(':')]
                value=(tuple(parts[:3]),parts[3])
            elif str(ret).startswith('s:'): value = str(ret)[2:]
            if mutation: model.mem.poke(mutation,4,value)
            if throws == 1: raise CallbackFailure()
            if throws == 2: raise Missing()
            if action:
                kind, operand = action
                if kind == 1: model.spell_end(args[0])
                elif kind == 2: model.handle_clear(args[0]+4)
                elif kind == 3: model.creature_add_spell(O,operand)
                elif kind == 4: model.mem.store(operand,4,value)
                elif kind == 5:
                    seed=(model.u32(SB.G_RAND_SEED)*1664525+1013904223)&MASK
                    model.w32(SB.G_RAND_SEED,seed)
                    model.w32(args[0]+0x44,seed>>16)
                elif kind == 6: model.handle_clear(operand)
            return value
        model.cb[name] = hook
    results, status = [], 'ok'
    try:
        for name,*args in commands:
            if name == 'external':
                model.external.add(('Spell','Creature')[args[0]]+'.v'+str(args[1])); result=0
            elif name == 'namebody': model.addr_names[args[0]]='named'; result=0
            elif name == 'objects': result=len(model.list_objects(args[0]))
            elif name == 'peek': result = model.u32(args[0])
            elif name == 'poke':
                model.mem.poke(*args)
                result = 0
            elif name == 'manacost':
                skill,sid,level=args
                fn=getattr(model,'mana_cost_'+str(sid),model.skill_mana_cost_base)
                result=fn(skill,level)
            else: result = getattr(model, METHODS[name])(*args)
            results.append(0 if result is None else result & MASK)
    except CallbackFailure: status = 'callback'
    except Missing: status = 'unsupported'
    return state(model, results, status)

def scenarios(SB):
    rng = random.Random(3568)
    steps = (0,0x3f000000,0xbf000000,0x3f800000,0x3c23d70a,0x4b800000,0x80000000,0x00800000)
    levels = (0,1,2,19,20,21,-1,-20,32767,65535,0x7fffffff,0x80000000)
    spreads = (0,0x3e4ccccc,0x3f000000,0x3f800000,0x40000000,0xbf800000,0x40400000)
    for config in range(16):
      for i in range(160):
        pokes = [(S+0x60,4,T),(O+0xc,4,(0,0x20,0x40,0x20040)[i%4]),
                 (H,4,0 if i%3==0 else N),(N+0xc,4,0 if i%5==0 else O),
                 (L,4,0xdeadbeef),(L+4,4,0xabcddcba),
                 (SB.G_RAND_SEED,4,rng.getrandbits(32)),(SB.G_COSMETIC_SEED,4,rng.getrandbits(32)),
                 (SB.G_SEED3,4,rng.getrandbits(32)),(SB.G_PVP_MODE_A,4,int(i%7==0)),
                 (B,4,0xaabbccdd),(B+0x10,4,rng.getrandbits(32)),(O+0x44,4,rng.getrandbits(32))]
        for slot,body in BODIES.items(): pokes += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
        for stat in range(5):
          # Int16 first/base and raw single steps are all authored, never retail tables.
          first = (-20,-1,0,1,10,20,32767)[(i+stat)%7]
          base = (-32768,-101,-1,0,1,5,6,101,32767)[(i+stat)%9]
          first2 = (-32768,-1,0,1,10,20)[(i+stat)%6]
          pokes += [(S+0xe+2*stat,2,first & 65535),(S+0x18+2*stat,2,base & 65535),
                    (S+0x24+4*stat,4,steps[(i+stat)%len(steps)]),
                    (S+0x38+2*stat,2,first2 & 65535),(S+0x42+2*stat,2,(-base)&65535),
                    (S+0x4c+4*stat,4,steps[(i+stat+2)%len(steps)])]
        lv = levels[i%len(levels)] & MASK
        # Avoid Python arbitrary-width conversion beyond the documented native domain.
        if lv == 0x7fffffff: lv = 0x7fff0000
        stat = i%5
        cap = (0,1,10,20,-1)[i%5] & MASK
        hooks = [('creature_bossRank',(-1,0,1,2,3)[i%5]&MASK,0,0)]
        commands = [('stat',S,stat,lv),('tier',S,stat,lv,cap),('for',S,stat,lv,0 if i%6==0 else O,cap),
                    ('handle',S,stat,lv,H,cap),('mana',S,lv,i),('archery',S,lv),('lifetap',S,lv),
                    ('life1',S,lv),('stance',S,lv),('resist',S,lv),('stun',S,0 if i%9==0 else O,lv),
                    ('range',S,stat,lv,i%2,L,L if i%11==0 else L+4,spreads[i%7]),
                    ('ctor',B,lv,O,S,T),('spellrand',B),('objectrand',O),('shared',),('cosmetic',),('third',),
                    ('adjust',S,lv,i),('can',S,O),('cast',S,lv,O,L,0),('cost',S,lv)]
        # Explicitly scripted virtual callback return, mutation, and failure cases.
        if i%10 in (1,2,3,4,5):
          slot = 14 if i%10==1 else 10
          pokes.append((T+8*slot+4,4,0x777000+slot))
          hooks.append(('Skill.v'+str(slot),(-12,0,1,5,6,9)[i%6]&MASK,
                        SB.G_RAND_SEED if i%10==3 else 0, 1 if i%10==4 else 2 if i%10==5 else 0))
        # Signed this adjustments must point at an authored duplicate table.
        if i%10==6:
          for slot in (10,11):
            pokes.append((T+8*slot,2,(-0x100)&65535))
          for a,size,v in list(pokes):
            if S+0xe <= a < S+0x60: pokes.append((a-0x100,size,v))
        sid=35+(i%34)
        body=SB.MANA_COST.get(sid,(BODIES[5],))[0]
        pokes.append((T+44,4,body))
        commands += [('manacost',S,sid,lv),('virtual',S,5,lv)]
        if i%10 in (4,5): commands.insert(0,('ctor',B,lv,O,S,T))
        commands += [('virtual',S,10,stat,lv),('virtual',S,11,stat,lv,cap),
                     ('virtual',S,12,stat,lv,O,cap),('virtual',S,13,stat,lv,H,cap),
                     ('virtual',S,14,lv,20),('virtual',S,15,lv,20),
                     ('virtual',S,8,stat,lv,i%2,L,L+4,spreads[i%7])]
        yield config,pokes,hooks,commands
    # Output pointers can alias the shared seed or Skill table. Ordered writes
    # must influence later draws/reads rather than computing both outputs first.
    for config in range(16):
      for lo,hi in ((SB.G_RAND_SEED,L),(L,SB.G_RAND_SEED),(SB.G_RAND_SEED,SB.G_RAND_SEED),
                    (S+0x24,S+0x18),(L,L)):
        pokes = [(S+0x60,4,T),(T+84,4,BODIES[10]),(S+0xe,2,1),(S+0x18,2,7),
                 (S+0x24,4,0),(SB.G_RAND_SEED,4,0x12345678)]
        yield config,pokes,[],[('range',S,0,1,1,lo,hi,0x40400000),('stat',S,0,2)]
    # Targeted sign-probe cases: the first random draw decides rounding even
    # when the second draw gives a bound with the opposite sign.
    for config in range(16):
      for seed in range(64):
        pokes = [(S+0x60,4,T),(T+84,4,BODIES[10]),(S+0xe,2,1),(S+0x18,2,7),
                 (S+0x24,4,0),(SB.G_RAND_SEED,4,seed)]
        yield config,pokes,[],[('range',S,0,1,1,L,L+4,0x40400000)]

    # Paired handles and mutation-safe iterator family. Append only: all old
    # rows and expectations remain byte-for-byte identical.
    pool, back, it = 0x70000, 0x80000, 0x90000
    for config in range(16):
      for count in range(7):
        for mode in range(8):
          pokes = [(SB.G_HANDLE_FREE,4,pool),(H,4,0),(back,4,0)]
          for i in range(40):
            node = pool + i*0x20
            pokes += [(node,4,node+0x20 if i<39 else 0),(node+0x10,2,0xffff)]
          for i in range(count):
            pokes.append((O+i*0x100+0xc,4,(0,1,2,3,0x40,0xffffffff)[i%6]))
          commands = []
          for i in range(count):
            obj = 0 if mode==1 and i%2==0 else O+i*0x100
            commands.append(('link',H,back,obj) if mode>=4 else ('push',H,obj))
          commands += [('headobj',H),('begin',it,H,0 if mode%2==0 else 3),
                       ('peek',it+4)]
          if count and mode==2:
            # Remove the currently visited node; the iterator remains linked.
            commands.append(('remove',pool+(count-1)*0x20))
          if mode==3:
            commands.append(('push',H,O+0x1000))
          if mode==4:
            # Destroy all paired back links while the iterator is parked.
            commands.append(('clear',back))
          if count>1 and mode==6:
            commands.append(('remove',pool+(count-2)*0x40))
          if mode!=7:
            commands += [('next',it),('peek',it+4)]*count
          commands += [('end',it,2),('clear',H),('clear',back),('headobj',H),('peek',SB.G_HANDLE_FREE)]
          yield config,pokes,[],commands
      # Spell attachment: null is a no-op, newest goes first, either end of
      # the paired relationship can be cleared and nodes reused immediately.
      pokes = [(SB.G_HANDLE_FREE,4,pool)]
      for i in range(12):
        pokes += [(pool+i*0x20,4,pool+(i+1)*0x20 if i<11 else 0),
                  (pool+i*0x20+0x10,2,65535)]
      commands = [('attach',O,0),('attach',O,B),('attach',O,B+0x100),
                  ('headobj',O+0x364),('clear',B+4),('headobj',O+0x364),
                  ('clear',O+0x364),('attach',O,B),('headobj',O+0x364),('clear',B+4)]
      yield config,pokes,[],commands
      # Exhausted pool behavior is preserved, including writes through node 0;
      # this model does not invent allocation refusal or roll back a half pair.
      for available in (0,1):
        pokes = [(SB.G_HANDLE_FREE,4,pool if available else 0),(pool,4,0)]
        yield config,pokes,[],[('link',H,back,O)]
        yield config,pokes,[],[('push',H,O)]
      # Aliased heads, partner/head overlap and unusual iterator storage.
      for head,other in ((H,H),(pool,pool+4),(H,pool+8)):
        pokes = [(SB.G_HANDLE_FREE,4,pool),(pool,4,pool+0x20),(pool+0x20,4,0)]
        yield config,pokes,[],[('link',head,other,O)]
      for iterator in (H,pool+4):
        pokes = [(SB.G_HANDLE_FREE,4,pool),(pool,4,0),(O+0xc,4,0)]
        yield config,pokes,[],[('push',H,O),('begin',iterator,H,0),('end',iterator,0)]

    # Shared lifecycle and projectile base: authored callback scripts and state.
    for config in range(16):
      for count in range(6):
       for mode in range(8):
        pokes=[(SB.G_HANDLE_FREE,4,pool),(O+0x38,2,65535),(L,4,0x80000000),(L+4,4,0x3f800000),(L+8,4,0x40400000)]
        for i in range(32): pokes.append((pool+i*0x20,4,pool+(i+1)*0x20 if i<31 else 0))
        commands=[]
        for i in range(count+1):
            spell=B+i*0x100
            pokes += [(spell+0x14,4,T),(T+8,2,(-16)&65535 if mode==7 else 0),(T+12,4,0x777100),(T+16,2,0),(T+20,4,0x777200)]
            if i<count: commands.append(('attach',O,spell))
        hooks=[('Spell.v1',0,0,0,2,0)]
        # Removal, insertion, mutation and exception prefixes during the walk.
        hooks.append(('Spell.v2',0,L if mode==4 else 0,1 if mode==5 else 2 if mode==6 else 0,
                      2 if mode==1 else 3 if mode==2 else 1 if mode==3 else 0,B+count*0x100))
        if mode==7:
            hooks[0]=('named',0,0,0)
            commands.append(('namebody',0x777100))
        commands += [('update',O,L),('objects',O+0x364),('update',O,L),('finish',0),('finish',B)]
        yield config,pokes,hooks,commands
      for owner in (0,O):
       for failure in (0,1,2):
        pokes=[(SB.G_HANDLE_FREE,4,pool),(pool,4,pool+0x20),(pool+0x20,4,0),
               (SB.G_RAND_SEED,4,0x12345678),(B+0xc,4,0x123400),(L,4,0xbf800000),(L+4,4,0),(L+8,4,0x3f800000)]
        hooks=[('GameObject_ctor',0,0,failure,5,0)]
        yield config,pokes,hooks,[('projectile',B,L,owner,0x888000),('gamector',B,L,0x888100,7)]
      # Geometry boundaries and full callback order; duplicate contacts are intentional.
      for mode in range(40):
        pokes=[(SB.G_WORLD,4,0x999000),(B+0x10,4,0),(B+0x14,4,0),(B+0x18,4,0x3f800000),
               (B+0x1c,4,0x3f800000),(B+0x20,4,0x3f800000),(B+0x44,4,1234),(B+0x50,1,mode%3),
               (B+0x48,4,T),(T+48,2,(-8)&65535 if mode%5==0 else 0),(T+52,4,0x777600),
               (L,4,(0,0x42700000,0xc2700000,0x80000000)[mode%4]),(L+4,4,0x41f00000),(L+8,4,0xc2700000)]
        for i in range(8):
            obj=O+i*0x100
            pokes += [(obj+0xc,4,(0,0x201,0x20000000,0,0,0,0,0)[i]),
                      (obj+0x10,4,(0,0,0,0x40000000,0,0,0x3f800000,0)[i]),
                      (obj+0x18,4,(0,0,0,0,0x41200000,0xc1200000,0,0)[i]),
                      (obj+0x1c,4,0x3f800000),(obj+0x20,4,0x3f800000),(obj+0x48,4,T)]
        pokes += [(B+0x4c,4,H),(H+0xc,4,O+0x700)]
        ground=0xc1200000 if mode%4==0 else 'q:1065353216:3212836864' if mode%4==1 else 0x41200000
        hooks=[('obj_updateGridCell',0,0,0),('terrain_height',ground,0,1 if mode==30 else 0),
               ('water_test',mode%2,0,0),('libc_sprintf','s:synthetic',0,0),('res_find',0xabc,0,0),
               ('sound_playOnObject',0,0,0),('water_disturb',0,0,0),
               ('world_collectObjectsInBox','l:'+':'.join(map(str,[B]+[O+i*0x100 for i in range(8)]+[O])),0,2 if mode==31 else 0),
               ('Creature.v6',0,B+0x10 if mode%7==0 else 0,1 if mode==32 else 0)]
        commands=[('move',B,L),('move',B,L)]
        yield config,pokes,hooks,commands
      # Scratch alias: word-by-word copying can consume earlier poke writes.
      yield config,[(0x1e0ffd4,4,1),(0x1e0ffd8,4,2),(0x1e0ffdc,4,3)],[],[('update',O,0x1e0ffd4),('alloc',1)]
      # Synthetic body refusal and forced callback delegation are distinct.
      for cls,body,slot in ((0,0x2d1a94,2),(1,0x2d66ac,6)):
        pokes=[(B+(0x14 if cls==0 else 0x48),4,T),(T+8*slot+4,4,body)]
        op=('spellvirtual',B,slot,L,0) if cls==0 else ('objvirtual',B,slot,O)
        # Refusals are native domain checks; only forced paths run in Python.
        yield config,pokes,[(('Spell' if cls==0 else 'Creature')+'.v'+str(slot),7,0,0)],[('external',cls,slot),op]


    # Append after the entire old corpus: healing/protection family. All values,
    # record slots, tables and callback outputs below are authored synthetic data.
    for config in range(16):
      rec=0x4d8af0
      common=[(S+0x60,4,T),(B+8,4,S),(B+0xc,4,O),(B,1,5),
              (B+0x10,4,0x12345678),(SB.G_RAND_SEED,4,0x76543210),
              (O+0x10,4,0x3f800000),(O+0x14,4,0x40000000),(O+0x18,4,0x40400000),
              (O+0x38,2,0x9876),(O+0x3c8,4,0),(O+0x22c,4,0x42c80000),
              (rec+0xac,4,0x42480000),(rec+0xb0,4,0x42c80000),(rec+0x43d8,4,O)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for stat in range(5):
        common += [(S+0xe+2*stat,2,1),(S+0x18+2*stat,2,(25,1,10,4,2)[stat]),
                   (S+0x24+4*stat,4,0),(S+0x38+2*stat,2,65535)]
      hooks=[('res_find',77,0,0),('sound_playOnObject',0,0,0),('GlowingShaft_ctor',0,0,0,5,0),
             ('particleGroup_create',88,0,0),('particleGroup_setMatrix',0,0,0),
             ('Spell.v1',0,0,0),('particleGroup_begin',0,0,0),('particleGroup_end',0,0,0),
             ('net_objOrGhost',O,0,0),('terrain_heightAtPoint',0x40400000,0,0),('particle_spawn',0,0,0)]
      # All casts: allocation success, no block, shaft success/failure, level
      # wrapping, player/nonplayer and negative record slots. Direct constructor
      # cases use the same hooks and cover every external failure prefix.
      for prefix,vt,body in (('minor',0x519720,0x2e022c),('shield',0x519ce0,0x2e80fc),('heal',0x519e20,0x2e8cf8)):
        for player in (0x20,0x40):
          for lv in (0,5,128,255):
            for allocation in ('q:0','q:'+str(B)+':0','q:'+str(B)+':'+str(H)):
              pokes=common+[(O+0xc,4,player),(T+20,4,body)]
              yield config,pokes,hooks+[('pool_new',allocation,0,0)],[(prefix+'cast',S,lv,O)]
        for failed in ('res_find','sound_playOnObject','pool_new','GlowingShaft_ctor','particleGroup_create'):
          # Minor healing has no particle group; omit that unused failure recipe.
          if prefix=='minor' and failed=='particleGroup_create': continue
          hs=[h for h in hooks if h[0]!=failed]+[('pool_new',H,0,0)] if failed!='pool_new' else hooks
          hs=[h for h in hs if h[0]!=failed]+[(failed,0,0,1)]
          yield config,common+[(O+0xc,4,0x40)],hs,[(prefix+'ctor',B,5,O,S)]
        # Constructor callbacks can rewrite spell owner, target, skill and seed.
        for name,field,value in (('sound_playOnObject',B+0xc,H),('pool_new',B+8,S),
                                 ('particleGroup_create',B+0xc,H)):
          if prefix=='minor' and name=='particleGroup_create': continue
          hs=[h for h in hooks if h[0]!=name]+[('pool_new',0,0,0)] if name!='pool_new' else hooks
          hs=[h for h in hs if h[0]!=name]+[(name,value,field,0)]
          yield config,common+[(O+0xc,4,0x40),(H+0xc,4,0x20),(H+0x22c,4,0x43480000)],hs,[(prefix+'ctor',B,5,O,S)]
        # Internal virtual dispatch and signed self adjustment for constructors' runs.
        clockfield=0x1c if prefix=='minor' else 0x18
        runbody={'minor':0x2e04f8,'shield':0x2e8428,'heal':0x2e90b8}[prefix]
        for adjust in (0,16,65520):
          origin=B+(-16 if adjust==16 else 16 if adjust==65520 else 0)
          yield config,common+[(origin+0x14,4,vt),(vt+16,2,adjust),(vt+20,4,runbody),
                               (B+clockfield,4,0x40000000),(B+0x18,4,O if prefix=='minor' else 0x40000000),
                               (B+0x20,4,0),(O+0xc,4,0)],hooks,[('spellvirtual',origin,2,L,0)]
      # Minor clock/HP boundaries and no-clamp behavior, including arbitrary
      # target record indexing with no player-flag requirement.
      for clock in (0,0x3effff77,0x3effff78,0x3fbe0000,0x3fbfffde,0x40000000):
        for hp in (0,0x42c60000,0x42c70000,0x42c80000,0x80000000):
          yield config,common+[(B+0x18,4,O),(B+0x1c,4,clock),(B+0x20,4,0x41200000),
                               (rec+0xac,4,hp)],hooks,[('minorrun',B)]
      # Shield duration strict comparison, matrix and owner re-read after callback.
      for clock,duration in ((0,0),(0,0x3c888889),(0,0x3f800000),(0x3f800000,0x3f800000)):
        for target in (0,H):
          hs=[h for h in hooks if h[0]!='particleGroup_setMatrix']+[('particleGroup_setMatrix',target,B+0xc if target else 0,0)]
          yield config,common+[(B+0x18,4,clock),(B+0x20,4,duration),(B+0x24,4,88)],hs,[('shieldrun',B)]
      # Complete absorb calls: exactly 241 particles, rejected ball samples,
      # negative sample counts (C remainder), positive/nonpositive flash timers,
      # allocation failure and success. Light constructor owns its shared draw.
      for seed,count,timer,light in ((0,7,0,0),(0x12345678,7,0,H),(0xffffffff,-7,0x3f800000,0),
                                    (0xabcdef01,1,0x80000000,H)):
        hs=hooks+[('Creature.v16',N,0,0),('model_getSamplePointCount',count&MASK,0,0),
                   ('model_getSamplePoint','p:1065353216:1073741824:1077936128:9',0,0),
                   ('pool_new',light,0,0),('LightEffect_ctor',0,0,0,5,0)]
        yield config,common+[(B+0x10,4,seed),(B+0x1c,4,timer),(B+0x24,4,88)],hs,[('absorb',B)]
      for failed in ('model_getSamplePointCount','model_getSamplePoint','particle_spawn','particleGroup_end','LightEffect_ctor'):
        hs=hooks+[('Creature.v16',N,0,0),('model_getSamplePointCount',7,0,0),
                  ('model_getSamplePoint','p:0:0:0:0',0,0),('pool_new',H,0,0),('LightEffect_ctor',0,0,0)]
        hs=[h for h in hs if h[0]!=failed]+[(failed,0,0,1)]
        yield config,common,hs,[('absorb',B)]
      # Healing players: 3D range, +1 guard, duplicate and reordered slots,
      # negative alive count, and PvP acceptance callbacks mutating the list.
      for pvp in (0,1):
        for slots in ((),(0,1,0),(1,0),(-1,0)):
          for height in (0x40400000,0x44800000):
            pokes=common+[(O+0xc,4,0x40),(B+0x1c,4,0x3f000000),(B+0x18,4,0),
                          (SB.G_PVP_MODE_A,4,pvp),(SB.G_ALIVE_COUNT,4,len(slots)),
                          (H+0x10,4,0x3f800000),(H+0x14,4,0x40000000),(H+0x18,4,height)]
            for i,slot in enumerate(slots):
              r=(rec+slot*0x4c30)&MASK
              pokes += [(SB.G_ALIVE_LIST+4*i,4,slot&MASK),(r+0x43d8,4,O if slot==0 else H),
                        (r+0xac,4,0x42c70000 if slot==1 else 0),(r+0xb0,4,0x42c80000)]
            yield config,pokes,hooks+[('Creature.v22',1,0,0)],[('healrun',B)]
      for vote in (0,1):
        pokes=common+[(O+0xc,4,0x40),(B+0x18,4,0),(SB.G_PVP_MODE_A,4,1),
                     (SB.G_ALIVE_COUNT,4,2),(SB.G_ALIVE_LIST,4,0),(SB.G_ALIVE_LIST+4,4,0)]
        yield config,pokes,hooks+[('Creature.v22',vote,SB.G_ALIVE_LIST,0)],[('healrun',B)]
      # Monster planar range includes vertical separation and preserves query
      # order/duplicates. Dead or noncreature owners skip the world query.
      for flags in (0,0x20,0x28):
        for vote in (0,1):
          for clock in (0,0x3e050ef8,0x40000000):
            pokes=common+[(O+0xc,4,flags),(O+0x228,4,0x42c80000),(B+0x18,4,clock),
                          (B+0x1c,4,0x3f000000),(H+0x10,4,0x3f800000),(H+0x14,4,0x40000000),
                          (H+0x18,4,0x44800000),(H+0x228,4,0x42c80000),(H+0x22c,4,0x42c80000),
                          (N+0x10,4,0x44800000)]
            hs=hooks+[('world_collectObjectsInBox','l:'+':'.join(map(str,(H,O,N,O))),0,0),('Creature.v21',vote,0,0)]
            yield config,pokes,hs,[('healrun',B)]
      # Ring particle terrain boundaries, negative radius and no particles from
      # negative level-first+5; ending runs after the presentation callbacks.
      for reachstat in (0,1,65535):
        for terrain in (0x42960000,0x42960001,0xc28a0000,0xc28a0001):
          hs=[h for h in hooks if h[0]!='terrain_heightAtPoint']+[('terrain_heightAtPoint',terrain,0,0)]
          yield config,common+[(O+0xc,4,0),(S+0x1a,2,reachstat),(B+0x18,4,0x40000000),
                               (SB.G_SIM_FRAME,4,0x87654321)],hs,[('healrun',B)]
      # Direct public helper APIs: record wrap and signed conversion forwarding.
      yield config,common+[(O+0x3c8,4,0xffffffff)],[],[('record',O),('unit',65535),('signedunit',0),
                                ('helperstat',S,0,5),('helperfor',S,2,5,O,20),('healshare',B,0x42c80000)]

# Harness/inventory glue has no portable gameplay body. The runtime mana alias
# factory counts once; registered-body accounting remains separate below.
ACCOUNTING_EXCLUDED = {'shape_result', '_register'}
PORTED_UNITS = set(('Model._node_take Model._node_push Model._node_unlink Model._node_free Model._partner_drop\nModel.handle_link Model.list_push Model.handle_removeNode Model.handle_clear Model.handle_obj Model.list_objects\nModel._iter_skip Model._iter_park Model.iter_begin Model.iter_next Model.iter_end\nModel.skill_can_use_base Model.skill_cast_base Model.skill_mana_cost_base Model._stat Model.skill_get_stat\nModel.skill_get_stat_tier2 Model.skill_get_stat_for Model.skill_get_stat_for_handle Model.skill_adjust_level\nModel.skill_mana_from_stat4 Model.skill_stat_range Model.archery_fraction Model.lifetap_fraction\nModel.lifetap_stat1 Model.strongstance_stat0 Model.naturalresistance_fraction Model.stunningstrike_stat0_for\nModel.spell_rand Model.cosmetic_draw Model.seed3_draw Model.spell_base_ctor Model.creature_add_spell\nModel.alloca Model.creature_update_spells Model.spell_end Model.game_object_ctor Model.projectile_ctor Model.obj_move_and_collide').split())

PORTED_UNITS.update(('Model.minorhealing_cast Model.minorhealing_spell_ctor Model.minorhealing_run Model.holyshield_cast Model.holyshield_spell_ctor Model.holyshield_run Model.holyshield_absorb Model.healing_cast Model.healing_spell_ctor Model.healing_run Model._heal_share Model._sample_point Model.record Model.stat Model.stat_for Model.unit Model.signed_unit Model.ball Model.spell_ball Model.object_ctor Model.light_effect').split())

def accounting(source):
    tree=ast.parse(source)
    units=[]
    for node in tree.body:
        if isinstance(node,ast.FunctionDef) and node.name not in ACCOUNTING_EXCLUDED:
            units.append(node.name)
        elif isinstance(node,ast.ClassDef):
            units.extend(node.name+'.'+m.name for m in node.body if isinstance(m,ast.FunctionDef))
    if len(units)!=len(set(units)): raise ValueError('ambiguous reference definitions')
    if PORTED_UNITS-set(units): raise ValueError('unknown ported units')
    return {'total':len(units),'ported':len(PORTED_UNITS),'ported_units':sorted(PORTED_UNITS),
            'remaining_units':sorted(set(units)-PORTED_UNITS),'excluded_glue':sorted(ACCOUNTING_EXCLUDED)}

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--research-dir',type=pathlib.Path,required=True)
    p.add_argument('--output',type=pathlib.Path,required=True)
    args = p.parse_args()
    with tempfile.TemporaryDirectory(prefix='skills-b-pinned-') as d:
        for path,digest in PINS.items():
            data = subprocess.run(['git','-C',str(args.research_dir),'show',COMMIT+':'+path],
                                  check=True,capture_output=True).stdout
            if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('pin mismatch: '+path)
            if path in ('tools/ref_core.py','tools/ref_creature.py','tools/ref_arena_monsters.py','tools/ref_skills_b.py'):
                (pathlib.Path(d)/pathlib.Path(path).name).write_bytes(data)
        sys.path.insert(0,d)
        SB = importlib.import_module('ref_skills_b')
        progress=accounting((pathlib.Path(d)/'ref_skills_b.py').read_text())
        rows = []
        for config,pokes,hooks,commands in scenarios(SB):
            rows.append('\t'.join((str(config),flat(pokes),flat(hooks),flat(commands),run(SB,config,pokes,hooks,commands))))
    data = ('\n'.join(rows)+'\n').encode('ascii')
    supported = set('Skill_canUse_base Skill_cast_base Skill_manaCost_base Skill_getStatRange Skill_getStat Skill_getStatTier2 Skill_getStatFor Skill_getStatForHandle Skill_adjustLevel Skill_manaFromStat4 Spell_rand GameObject_rand SkillArchery_getStat0Fraction SkillLifeTap_getStat0Fraction SkillLifeTap_getStat1 SkillStrongStance_getStat0 SkillNaturalResistance_getStat0Fraction SkillStunningStrike_getStat0ForTarget'.split())
    supported.update('Skill%d_manaCost'%sid for sid in SB.MANA_COST)
    supported.update('Creature_updateSpells Projectile_ctor obj_moveAndCollide'.split())
    supported.update('handle_link handle_clear handle_removeNode list_push iter_begin iter_next iter_end creature_addSpell'.split())
    supported.update('SkillMinorHealing_cast SpellMinorHealing_ctor SpellMinorHealing_run SkillHolyShield_cast SpellHolyShield_ctor SpellHolyShield_run SpellHolyShield_absorb SkillHealing_cast SpellHealing_ctor SpellHealing_run'.split())
    unknown = supported - set(SB.FUNCTIONS)
    if unknown: raise ValueError('unknown supported functions: '+str(unknown))
    catalog = {str(key): value for key,value in SB.SKILLS.items()}

    args.output.parent.mkdir(parents=True,exist_ok=True)
    if args.output.suffix == '.gz':
        with args.output.open('wb') as file:
            with gzip.GzipFile(fileobj=file,mode='wb',mtime=0,filename='') as archive: archive.write(data)
    else: args.output.write_bytes(data)
    manifest = {'schema':1,'reference_progress':progress,'rows':len(rows),'research_commit':COMMIT,'references':PINS,
                'vectors_sha256':hashlib.sha256(data).hexdigest(),
                'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                'supported_functions':sorted(supported),'unsupported_functions':sorted(set(SB.FUNCTIONS)-supported),
                'skill_catalog':catalog}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print('Generated',len(rows),'complete-state cases')
if __name__=='__main__': main()
