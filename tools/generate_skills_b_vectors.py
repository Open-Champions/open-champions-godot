#!/usr/bin/env python3
"""Generate authored inputs and full expected state from immutable research Python."""
import argparse
import ast
import contextlib
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
        if a is None: return "n"
        if isinstance(a, tuple) and a and a[0] == 'desc':
            return 'd' + ''.join(':'+str(v) for e in a[1] for v in e)
        if isinstance(a, tuple): return 'v:' + ':'.join(str(v & MASK) for v in a)
        if isinstance(a, str): return 's:' + a
        return str(a & MASK)
    calls = ';'.join(name + ',' + ','.join(arg(a) for a in args) for name,args in model.calls)
    return '|'.join((status, ','.join(map(str,results)), str(model.draws), memory, writes, calls))

class CallbackFailure(Exception): pass
class Missing(Exception): pass

METHODS = {
 'f32':'f32', 'player':'is_player', 'half':'half_toward_zero', 'stat14':'_stat_via_slot14',
 'lifecast':'skill_cast_base', 'stancecast':'skill_cast_base',
 'stancetrigger':'strongstance_trigger', 'stuneffect':'stunningstrike_effect',
 'shadowcast':'shadowstep_cast', 'shadowctor':'shadowstep_spell_ctor', 'shadowrun':'shadowstep_run',
 'windcast':'planarwind_cast', 'windctor':'planarwind_spell_ctor', 'windrun':'planarwind_run',
 'sparkcast':'shockoffire_cast', 'sparkctor':'sparks_ctor', 'sparkrun':'sparks_run', 'sparktouch':'sparks_on_touch',
 'darkcast':'darkness_cast', 'darkctor':'darkness_ctor', 'darkrun':'darkness_run',
 'harmcan':'harmtouch_can_use', 'harmcast':'harmtouch_cast', 'harmctor':'harmtouch_spell_ctor',
 'harmtint':'_harmtouch_tint', 'harmrun':'harmtouch_run', 'harmhit':'harmtouch_apply_to_hit',
 'poundcan':'groundpound_can_use', 'poundcast':'groundpound_cast',
 'poundctor':'groundpound_effect_ctor', 'poundstat':'_groundpound_stat', 'poundrun':'groundpound_effect_run',
 'bowcan':'bow_can_use', 'multican':'multifire_can_use', 'multicast':'multifire_cast',
 'conecast':'coneoffrost_cast', 'conector':'coneoffrost_spell_ctor', 'conerun':'coneoffrost_run',
 'dummyctor':'dummydamager_ctor', 'dummyrun':'dummydamager_run', 'dummytouch':'dummydamager_on_touch',
 'cyclonecan':'cyclone_can_use', 'cyclonecast':'cyclone_cast',
 'cyclonector':'cyclone_spell_ctor', 'cyclonerun':'cyclone_run',
 'cyclonefx':'cyclone_update_fx',
 'revivecan':'resurrect_can_use', 'revivetarget':'resurrect_find_target',
 'reviveraised':'resurrect_is_being_raised', 'revivecast':'resurrect_cast',
 'revivector':'resurrect_spell_ctor', 'reviverun':'resurrect_run',
 'revivedeath':'resurrect_on_owner_death',
 'bashcan':'bash_can_use',
 'bashcast':'bash_cast',
 'bashctor':'bash_spell_ctor',
 'bashwind':'bash_windup_fx',
 'bashrun':'bash_run',
 'bashdone':'_bash_done',
 'critcan':'criticalhit_can_use',
 'critcast':'criticalhit_cast',
 'critctor':'criticalhit_spell_ctor',
 'crittint':'criticalhit_set_tint',
 'critrun':'criticalhit_run',
 'critbonus':'criticalhit_bonus_for',
 'chargecast':'charge_cast',
 'chargector':'charge_spell_ctor',
 'chargerun':'charge_run',
 'chargestat':'charge_stat3_scaled',
 'chargecontact':'charge_on_contact',
 'uintfloat':'from_uint',

 'ancestralcan':'ancestralcall_can_use',
 'ancestralcast':'ancestralcall_cast',
 'ancestralctor':'ancestralcall_spell_ctor',
 'ancestralrun':'ancestralcall_run',
 'ancestraltint':'ancestralcall_tint_and_glow',
 'ancestralstat0':'ancestralcall_stat0_fraction',
 'ancestralstat1':'ancestralcall_stat1_fraction',
 'ancestralquery':'ancestralcall_query_for',

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
 'enemycan':'convertenemy_can_use', 'undeadcan':'convertundead_can_use',
 'enemycast':'convertenemy_cast', 'undeadcast':'convertundead_cast',
 'enemyctor':'convertenemy_spell_ctor', 'undeadctor':'convertundead_spell_ctor',
 'enemyrun':'convertenemy_run', 'undeadrun':'convertundead_run',
 'convertcast':'convert_cast', 'convertctor':'convert_spell_ctor', 'convertrun':'convert_run',
 'convertword':'_convert_word',
 'growthcan':'giantgrowth_can_use',
 'growthcast':'giantgrowth_cast',
 'growthctor':'giantgrowth_spell_ctor',
 'growthrun':'giantgrowth_run',
 'growthstep':'giantgrowth_step_scale',
 'growthemit':'giantgrowth_emit',
 'growthscale':'giantgrowth_owner_scale',
 'record':'record',
 'unit':'unit',
 'signedunit':'signed_unit',
 'helperstat':'stat',
 'helperfor':'stat_for',

 'frostcast':'froststorm_cast', 'frostctor':'froststorm_ctor', 'frostrun':'froststorm_run',
 'shardparticle':'_iceshard_particle', 'shardrun':'iceshard_run',
 'shockcast':'shock_cast', 'shockctor':'shock_ctor', 'shockchain':'shock_build_chain',
 'shockin':'shock_in_chain', 'shockwants':'_shock_wants', 'shockrun':'shock_run',

}

def run(SB, config, pokes, hooks, commands):
    SB.C.set_fpu(('ps2','ps2g1','chop','nearest')[config//4], bool(config&2), bool(config&1))
    initial = SB.RC.Memory()
    for addr,size,value in pokes: initial.poke(addr,size,value)
    model = SB.Model(initial)
    for name,ret,mutation,throws,*action in hooks:
        sequence = str(ret).split(':')[1:] if str(ret).startswith('q:') else None
        cursor = [0]
        def hook(*args, ret=ret, mutation=mutation, throws=throws, action=action, sequence=sequence, cursor=cursor):
            value = ret
            if sequence:
                value = int(sequence[min(cursor[0],len(sequence)-1)])
                cursor[0] += 1
            elif str(ret).startswith('l:'): value = [int(x) for x in str(ret)[2:].split(':') if x]
            elif str(ret).startswith('g:'):
                parts=[int(x) for x in str(ret)[2:].split(':')]
                value=(tuple(parts[:3]),tuple(parts[3:4]),tuple(parts[4:]))
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
            if name == 'factory':
                slot,n,skill,level=args
                result=SB._mana_cost_method(slot,n)(model,skill,level)
            elif name == 'cstrlen': result=len(model.cstr(args[0],SB.s32(args[1])))
            elif name == 'cstrbyte':
                text=model.cstr(args[0],SB.s32(args[1]))
                result=text[args[2]] if args[2]<len(text) else 256
            elif name == 'route':
                cls,obj,table,slot,*a=args
                result=model._virtual(('Skill','Spell','Creature')[cls],(SB.SKILL_BODIES,SB.SPELL_BODIES,SB.OBJECT_BODIES)[cls],obj,table,slot,a)
            elif name == 'table':
                cls,obj,off,slot,*a=args
                result=model.table_call(('Skill','Spell','Creature','Other')[cls],obj,off,slot,*a)
            elif name == 'extskill': model.external.add('Skill.v'+str(args[0])); result=0
            elif name == 'emptyname': model.addr_names[args[0]]=''; result=0
            elif name == 'call':
                result=model.call(('handle_link','handle_clear','handle_removeNode','synthetic','typed')[args[0]],*args[1:])
            elif name == 'external':
                model.external.add(('Spell','Creature')[args[0]]+'.v'+str(args[1])); result=0
            elif name == 'namebody': model.addr_names[args[0]]='named'; result=0
            elif name == 'objects': result=len(model.list_objects(args[0]))
            elif name == 'queryinto':
                model.query_into(args[0],'synthetic_query',*args[1:]); result=0
            elif name == 'querylist': result=model.query_list('synthetic_query',*args)
            elif name == 'fill':
                SB.list_fill(model.mem.load,model.mem.poke,args[0],args[1:]); result=0
            elif name == 'ambient': result=model.ambient_bytes()[args[0]]
            elif name == 'windstrike': model._planarwind_strike(args[0],args[1],tuple(args[2:])); result=0
            elif name == 'darkbox': result=len(model._darkness_box(*args))
            elif name == 'bashrange': result=model._bash_range(args[0])[args[1]]
            elif name == 'rangehelper': result=model.stat_range(*args[:-1])[args[-1]]
            elif name == 'trailpoint': result=model._charge_trail_point(args[0],args[1:3],args[3])[args[4]]
            elif name == 'down': result=SB.down(*args)
            elif name == 'crem': result=SB.c_rem(*args)
            elif name == 'frostpoint': result=model._froststorm_point(*args[:-1])[args[-1]]
            elif name == 'uintfloat': result=SB.from_uint(*args)
            elif name == 'localvec': result=model.local_vec(model.vec(args[0]))
            elif name == 'objball': result=model.obj_ball(args[0])[args[1]]
            elif name in ('arrowcan','arrowcast','arrowctor','arrowctor2','arrowrun','arrowhit','arrowend','arrowparticles','arrowcommon'):
                sid,*a=args
                prefix={43:'firearrow',44:'coldarrow',45:'poisonarrow',47:'explodearrow'}[sid]
                suffix={'arrowcan':'can_use','arrowcast':'cast','arrowctor':'ctor','arrowctor2':'ctor2','arrowrun':'run','arrowhit':'on_hit','arrowend':'end','arrowparticles':'particles','arrowcommon':'common'}[name]
                if sid==47 and suffix=='end': suffix='explode'
                if name in ('arrowparticles','arrowcommon'): prefix='_'+prefix
                if name=='arrowcommon' and sid==43: a.insert(3,0) # unused level
                result=getattr(model,prefix+'_'+suffix)(*a)
            elif name=='arrowlight': result=model._arrow_light(args[0],model.vec(args[1]),args[2],args[3],args[4:7])
            elif name=='arrowvf': result=model._arrow_velocity_facing(*args)
            elif name=='arrowvd': result=model._arrow_velocity_dir(args[0],model.vec(args[1]),args[2])
            elif name=='arrowblocked': result=model._arrow_blocked_for_player(*args)
            elif name=='firestats': result=model._firearrow_stats(*args)
            elif name=='arrowfinish': result=model._arrow_finish(*args)
            elif name=='monsterspread': result=model._monster_spread(*args[:2])[args[2]]
            elif name=='flightparticle': result=model._flight_particle(*args)
            elif name=='arrowflight':
                sid,obj,reason=args
                prefix={44:'coldarrow',45:'poisonarrow',47:'explodearrow'}[sid]
                result=model._arrow_flight(obj,getattr(model,prefix+('_explode' if sid==47 else '_end')),getattr(model,'_'+prefix+'_particles'),reason)
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


    # Conversion family: append to preserve every older byte-for-byte row.
    for config in range(16):
      common=[(S+0x60,4,T),(B+8,4,S),(B+0xc,4,O),(B,1,5),
              (SB.G_RAND_SEED,4,0x12345678),(SB.G_SIM_FRAME,4,0x87654321),
              (O+0x10,4,0x3f800000),(O+0x14,4,0x80000000),(O+0x18,4,0x40400000),
              (O+0x38,2,0xffff),(O+0x3c8,4,0xffffffff),
              (0x4d3ec0+0x4c24,1,0xfe),(SB.G_HANDLE_FREE,4,pool)]
      for i in range(96): common += [(pool+32*i,4,pool+32*(i+1) if i<95 else 0)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for stat,value in enumerate((4,7,75,0,2)):
        common += [(S+0xe+2*stat,2,1),(S+0x18+2*stat,2,value),(S+0x24+4*stat,4,0)]
      hooks=[('res_find',17,0,0),('sound_playOnObject',0,0,0),('creature_bossRank',0,0,0),
             ('particleGroup_create','q:41:42:43:44',0,0),('particleGroup_setMatrix',0,0,0),
             ('particleGroup_begin',0,0,0),('particle_spawn',0,0,0),('particleGroup_end',0,0,0),
             ('Creature_setCharm',0,0,0),('pool_new',H,0,0),('text_lookup',123,0,0),
             ('hud_showMessage',0,0,0),('FlyingText_ctor',0,0,0,5,0),('Spell.v1',0,0,0)]
      targets=[H+i*0x500 for i in range(8)]
      data=[]
      for i,target in enumerate(targets):
        data += [(target+0xc,4,(0,0x1000000,0x40,0,0,0,0,0)[i]),
                 (target+0x312,2,1 if i==3 else 0),(target+0xb4,1,0x10 if i%2==0 else 0),
                 (target+0x354,4,0x3f800000 if i==4 else 0),
                 (target+0x10,4,0x40000000),(target+0x14,4,0xbf800000),(target+0x18,4,0x40400000),
                 (target+0x38,2,0x8001),(target+0x1c,4,0x41a00001 if i%2 else 0x41a00000),
                 (target+0xa8,4,0xabcdef01),(target+0xac,4,0x87654321)]
      arc=('world_collectObjectsInArc','l:'+':'.join(map(str,targets+[targets[0]])),0,0)
      box=('world_collectObjectsInBox','l:'+':'.join(map(str,targets)),0,0)
      # Eligibility, short-circuiting, callback changes and failure prefixes.
      for prefix in ('enemy','undead'):
        for player in (0,0x40):
          for pvp in (0,1):
            for vote in (0,1):
              yield config,common+data+[(O+0xc,4,player),(SB.G_PVP_MODE_A,4,pvp)],hooks+[box,('Creature.v20',vote,0,0)],[(prefix+'can',S,O)]
        for fail in ('text_lookup','hud_showMessage','world_collectObjectsInBox','Creature.v20'):
          hs=[h for h in hooks+[box,('Creature.v20',1,0,0)] if h[0]!=fail]+[(fail,0,0,1)]
          yield config,common+data+[(O+0xc,4,0x40),(SB.G_PVP_MODE_A,4,int(fail in ('text_lookup','hud_showMessage')))],hs,[(prefix+'can',S,O)]
      for id,prefix,table,castbody,runbody,canbody in ((39,'enemy',0x5189b0,0x2cff1c,0x2d0354,0x2cffb8),(40,'undead',0x518a68,0x2d09bc,0x2d0edc,0x2d0a58)):
        # Cast and ctor: signed levels/counts, allocations, filtered/duplicate
        # query order, exhausted pools, callbacks before target filtering.
        for lv in (0,5,128,255):
          for most in (0,1,4,0xffff):
            for block in (0,B):
              hs=[h for h in hooks if h[0]!='pool_new']+[('pool_new',block,0,0),arc]
              yield config,common+data+[(S+0x18,2,most)],hs,[(prefix+'cast',S,lv,O),('objects',B+0x18)]
        for fail in ('res_find','sound_playOnObject','world_collectObjectsInArc','creature_bossRank','particleGroup_create'):
          hs=[h for h in hooks+[arc] if h[0]!=fail]+[(fail,0,0,1)]
          yield config,common+data,hs,[(prefix+'ctor',B,5,O,S)]
        for rank in (0,1,0xffffffff):
          hs=[h for h in hooks if h[0]!='creature_bossRank']+[arc,('creature_bossRank',rank,targets[0]+0x312,0)]
          yield config,common+data,hs,[('convertctor',id,B,5,O,S),('objects',B+0x18)]
        for available in (0,1,2):
          yield config,common+data+[(SB.G_HANDLE_FREE,4,pool if available else 0),(pool,4,pool+32 if available==2 else 0)],hooks+[('world_collectObjectsInArc','l:'+str(H),0,0)],[(prefix+'ctor',B,5,O,S)]
        # Full lifetime, exact threshold, angle wrap, wide rounding (including
        # negative odd chances), immutable target snapshot, null entries.
        for clock in (0,0x40700000,0x40800000,0x80000000):
          for chance in (0,1,75,201,0xffff):
            for player in (0,0x40):
              pokes=common+data+[(O+0xc,4,player),(S+0x1c,2,chance),
                    (B+0x70,2,65530),(B+0x74,4,clock),(B+0x14,4,table),
                    (table+12,4,0x777100),(B+(0x20 if id==39 else 0x1c),4,44)]
              commands=[('push',B+0x18,H),('push',B+0x18,0),('push',B+0x18,H+0x500),('push',B+0x18,H)]
              yield config,pokes,hooks,commands+[(prefix+'run',B)]
        for seed in (0,1,0xffffffff,0x12345678):
          for radius in (0x41a00000,0x41a00001,0x42c80000):
            pokes=common+data+[(SB.G_RAND_SEED,4,seed),(H+0x1c,4,radius),(H+0xb4,1,0),
                             (B+0x74,4,0x40800000),(S+0x1c,2,101),(S+0x1a,2,0xffff)]
            yield config,pokes,hooks,[('push',B+0x18,H),('convertrun',id,B)]
        for fail in ('particleGroup_setMatrix','particleGroup_begin','particle_spawn','particleGroup_end',
                     'Creature_setCharm','pool_new','text_lookup','FlyingText_ctor','Spell.v1'):
          hs=[h for h in hooks if h[0]!=fail]+[(fail,0,0,1)]
          final=fail not in ('particleGroup_setMatrix','particleGroup_begin','particle_spawn','particleGroup_end')
          pokes=common+data+[(H+0xb4,1,0),(H+0x1c,4,0x42c80000),(B+0x74,4,0x40800000 if final else 0),
                            (S+0x1c,2,201 if fail=='Creature_setCharm' else 0)]
          yield config,pokes,hs,[('push',B+0x18,H),(prefix+'run',B)]
        # Callback re-reads after matrix/rolls and snapshots after removals.
        for cb,ret,field in (('particleGroup_setMatrix',0x41200000,H+0x1c),
                             ('Creature_setCharm',O,B+0xc),('particleGroup_end',0,B+0x74)):
          hs=[h for h in hooks if h[0]!=cb]+[(cb,ret,field,0)]
          yield config,common+data+[(B+0x74,4,0x40800000 if cb=='Creature_setCharm' else 0),(S+0x1c,2,201)],hs,[('push',B+0x18,H),('push',B+0x18,H+0x500),(prefix+'run',B)]
        # Virtual dispatch with signed this adjustment; cast and can-use too.
        for adj in (0,16,65520):
          origin=B+(-16 if adj==16 else 16 if adj else 0)
          yield config,common+[(origin+0x14,4,table),(table+16,2,adj),(table+20,4,runbody)],hooks,[('spellvirtual',origin,2,L,0)]
        yield config,common+data+[(T+20,4,castbody)],hooks+[arc],[('virtual',S,2,5,O,L,0)]
        yield config,common+data+[(T+12,4,canbody)],hooks+[box,('Creature.v20',1,0,0)],[('virtual',S,1,O)]
        yield config,common+data,hooks+[arc],[('convertcast',id,S,5,O)]+[(prefix+'run',B)]*35
      # Query helpers preserve order, aliases, node-zero writes and no log.
      for objects in ((),(0,H,H,N),(H,N)):
        hs=[('synthetic_query','l:'+':'.join(map(str,objects)),0,0)]
        yield config,common,hs,[('querylist',),('queryinto',L),('objects',L),('fill',L,*objects),('objects',L)]
      for block in (0,H):
        hs=[h for h in hooks if h[0]!='pool_new']+[('pool_new',block,0,0)]
        yield config,common+data,hs,[('convertword',H,0x54edb8)]

    # Giant Growth family, appended without altering preceding recipes.
    for config in range(16):
      common=[(S+0x60,4,T),(B+8,4,S),(B+0xc,4,O),(B,1,5),
              (O+0xc,4,0x40),(O+0xdc,4,0x3f800000),(O+0x3c8,4,0xffffffff),
              (O+0x10,4,0x3f800000),(O+0x14,4,0x80000000),(O+0x18,4,0x40400000),
              (O+0x38,2,0xffff),(O+0x544,4,N),(SB.G_RAND_SEED,4,0x12345678),
              (SB.G_HANDLE_FREE,4,pool),(pool,4,pool+32),(pool+32,4,0),
              (B+0x14,4,0x51b6c0),(0x51b6c0+12,4,0x777100)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for stat,value in enumerate((50,1,0,0,2)):
        common += [(S+0xe+2*stat,2,1),(S+0x18+2*stat,2,value),(S+0x24+4*stat,4,0)]
      sample='g:'+':'.join(map(str,(0x40000000,0xbf800000,0x40800000,0x41200000,
                       0x3f800000,0x3f000000,0,0,0x3f800000,0,0,0,0x3f800000)))
      hooks=[('SpellGiantGrowth_typeinfo',17,0,0),('creature_findSpellByType',0,0,0),
             ('pool_new',B,0,0),('light_link',0,0,0),('light_relink',0,0,0),
             ('particleGroup_create',88,0,0),('model_getSamplePointCount',7,0,0),
             ('res_find',19,0,0),('sound_playAtPos',0,0,0),('particleGroup_begin',0,0,0),
             ('particleGroup_setMatrix',0,0,0),('model_getSamplePoint',sample,0,0),
             ('particle_spawn',0,0,0),('particleGroup_end',0,0,0),('Spell.v1',0,0,0)]
      for found in (0,1,0xffffffff):
        hs=[h for h in hooks if h[0]!='creature_findSpellByType']+[('creature_findSpellByType',found,0,0)]
        yield config,common,hs,[('growthcan',S,O)]
      for level in (0,5,128,255,0xffffffff):
        for flags in (0,0x20,0x40):
          for block in (0,B):
            hs=[h for h in hooks if h[0]!='pool_new']+[('pool_new',block,0,0)]
            yield config,common+[(O+0xc,4,flags)],hs,[('growthcast',S,level,O),('growthscale',B)]
      for count in (1,7,257,0xfffffff9):
        hs=[h for h in hooks if h[0]!='model_getSamplePointCount']+[('model_getSamplePointCount',count,0,0)]
        yield config,common,hs,[('growthctor',B,5,O,S)]
      for scale,aim in ((0x3f800000,0x3fc00000),(0x3fc00000,0x3f800000),
                        (0x3f800000,0x3f800001),(0x3f800001,0x3f800000),
                        (0,0),(0x80000000,0),(0x3fa00000,0x3fa00000)):
        for frames in (0,1,29,30,31,0xffffffff,0x80000000):
          yield config,common+[(O+0xdc,4,scale),(B+0x1c,4,aim),(B+0x18,4,0x3f800000),
                              (B+0x20,4,frames)],hooks,[('growthstep',B,O),('growthscale',B)]
      for aim,scale in ((0x3f800000,0x3f800000),(0x3fc00000,0x3fc00000),(0x3fc00000,0x3f800000)):
        for age in (0,40,42,45,255):
          for seed in (0,0xffffffff):
            pokes=common+[(B+0x18,4,0x3f800000),(B+0x1c,4,aim),(O+0xdc,4,scale),
                          (B+0x28,4,7),(B+0x10,4,seed)]
            for k in range(50): pokes += [(B+0x30+4*k,4,age|(3<<8)|(65530<<16))]
            yield config,pokes,hooks,[('growthemit',B,O)]
      for fail in [h[0] for h in hooks]:
        hs=[h for h in hooks if h[0]!=fail]+[(fail,0,0,1)]
        op='growthcan' if fail in ('SpellGiantGrowth_typeinfo','creature_findSpellByType') else 'growthcast'
        yield config,common,hs,[(op,S,5,O) if op=='growthcast' else (op,S,O)]
        if fail in ('light_relink','particleGroup_begin','particleGroup_setMatrix','model_getSamplePoint','particle_spawn','particleGroup_end','Spell.v1'):
          yield config,common+[(B+0xf8,4,H),(H+0xc,4,O),(B+0x28,4,7),(B+0x24,4,int(fail=='Spell.v1'))],hs,[('growthrun',B)]
      for cb,field,val in (('light_link',B+0xc,H),('particleGroup_create',B+8,S),
                            ('particleGroup_setMatrix',O+0xdc,0x3fc00000),('particle_spawn',B+0x2c,99),
                            ('particleGroup_end',B+0x24,1)):
        hs=[h for h in hooks if h[0]!=cb]+[(cb,val,field,0)]
        yield config,common+[(H+0xdc,4,0x40000000),(B+0xf8,4,N),(N+0xc,4,O),(B+0x28,4,7)],hs,[('growthctor',B,5,O,S),('growthrun',B)]
      for adj in (0,16,65520):
        origin=B+(-16 if adj==16 else 16 if adj else 0)
        yield config,common+[(origin+0x14,4,0x51b6c0),(0x51b6c0+16,2,adj),
                             (0x51b6c0+20,4,0x309420)],hooks,[('spellvirtual',origin,2,L,0)]
      yield config,common+[(T+12,4,0x308f2c)],hooks,[('virtual',S,1,O)]
      yield config,common+[(T+20,4,0x308f60)],hooks,[('virtual',S,2,5,O,L,0)]
      for node,obj in ((0,O),(H,0),(H,O)):
        yield config,common+[(B+0xf8,4,node),(H+0xc,4,obj),(B+0x28,4,7)],hooks,[('growthrun',B)]
      yield config,common+[(SB.G_HANDLE_FREE,4,0)],hooks,[('growthctor',B,5,O,S),('growthrun',B)]
      # Complete grow/hold/shrink/end lifecycle with small authored target.
      yield config,common+[(S+0x18,2,1)],hooks,[('growthcast',S,5,O)]+[('growthrun',B)]*66

    # Complete Ancestral Call family; append-only corpus extension.
    for config in range(16):
      common=[(S+0x60,4,T),(B+8,4,S),(B+0xc,4,O),(B+0x18,4,O),(B,1,5),
              (B+0x20,4,0x41200000),(B+0x80,4,88),(O+0xc,4,0x40),(O+0x3c8,4,0),
              (O+0x10,4,0x3f800000),(O+0x14,4,0),(O+0x18,4,0x40400000),
              (O+0x38,2,65535),(O+0x544,4,17),(O+0x4a8,4,MASK),(O+0x24,4,H),(H,4,N),
              (SB.G_RAND_SEED,4,0x12345678),(SB.G_SIM_FRAME,4,40),
              (0x54fc60,4,0),(0x54fc64,4,0),(0x54fc68,4,0),
              (L,4,0x40000000),(L+4,4,0x40400000),(L+8,4,0x40800000),
              (N+0x10,4,0x41400000),(N+0x14,4,0),(N+0x18,4,0x447a0000),
              (SB.G_ALIVE_COUNT,4,3),(SB.G_ALIVE_LIST,4,0),(SB.G_ALIVE_LIST+4,4,1),
              (SB.G_ALIVE_LIST+8,4,2),(SB.G_PLAYERS+SB.REC_OBJ,4,O),
              (SB.G_PLAYERS+SB.REC_SIZE+SB.REC_OBJ,4,N),
              (SB.G_PLAYERS+2*SB.REC_SIZE+SB.REC_OBJ,4,0),
              (B+0x14,4,0x5197d8),(0x5197d8+12,4,0x777100)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for stat,value in enumerate((125,130,10,1,2)):
        common += [(S+0xe+2*stat,2,1),(S+0x18+2*stat,2,value),(S+0x24+4*stat,4,0)]
      hooks=[('SpellAncestralCall_typeinfo',17,0,0),('creature_findSpellByType',0,0,0),
             ('pool_new','q:'+str(B)+':'+str(H),0,0),('res_find',19,0,0),
             ('libc_sprintf','s:synthetic_clip',0,0),('sound_playOnObject',22,0,0),
             ('anim_startBlendInOut',0,0,0),('LightEffect_ctor',0,0,0),
             ('particleGroup_create',88,0,0),('Player_animStep',0,0,0),('sound_stop',0,0,0),
             ('model_getAttachPoint','l:1065353216:1073741824:1077936128',0,0),
             ('sound_release',0,0,0),('beam_create',42,0,0),('beam_update',0,0,0),
             ('fx_shockModel',0,0,0),('particleGroup_begin',0,0,0),('net_objOrGhost',O,0,0),
             ('particleGroup_setMatrix',0,0,0),('terrain_heightAtPoint',0x40400000,0,0),
             ('particle_spawn',0,0,0),('particleGroup_end',0,0,0),('Creature.v22',1,0,0),('Spell.v1',0,0,0)]
      def change(name,ret,mutation=0,throws=0):
        return [h for h in hooks if h[0]!=name]+[(name,ret,mutation,throws)]
      for found in (0,1,MASK):
        yield config,common,change('creature_findSpellByType',found),[('ancestralcan',S,O)]
      for level in (0,5,127,128,255,MASK):
        for flags in (0,0x40,0x20000,0x20040):
          for poolret in ('q:'+str(B)+':0','q:'+str(B)+':'+str(H),'q:0'):
            yield config,common+[(O+0xc,4,flags)],change('pool_new',poolret),[('ancestralcast',S,level,O)]
      for weapon in (0,1,2,3,5,7,4,255):
        yield config,common+[(SB.G_PLAYERS+0x4a86,1,weapon)],hooks,[('ancestralctor',B,5,O,S)]
      for value in (0,1,32767,65535,32768):
        yield config,common+[(S+0x18,2,value),(S+0x1a,2,value)],hooks,[('ancestralstat0',B),('ancestralstat1',B)]
      for target in (0,O,N):
        for out0,out1 in ((0,0),(H,H+4),(H,H),(B+0xc,B+8)):
          for pvp,vote in ((0,1),(1,0),(1,1)):
            yield config,common+[(SB.G_PVP_MODE_A,4,pvp)],change('Creature.v22',vote),[('ancestralquery',B,target,out0,out1)]
      for distance in (0x413fffff,0x41400000,0x41400001,0x41500000):
        # Relative distance from caster at x=1 is exactly 12 at x=13.
        yield config,common+[(O+0x10,4,0),(N+0x10,4,distance)],hooks,[('ancestralquery',B,N,H,H+4),('ancestralrun',B,L)]
      for radius in (0,1,65535,32767,32768):
        yield config,common+[(S+0x1e,2,radius)],hooks,[('ancestralquery',B,N,H,H+4),('ancestralrun',B,L)]
      for level in (0,5,127,128,241,242,255):
        for done,anim in ((0,0),(0,0x20),(1,0x20)):
          yield config,common+[(B,1,level),(B+0x7c,4,done),(O+0x11a,2,anim)],hooks,[('ancestralrun',B,L)]
      for frame in (0,39,40,41,MASK):
        for handle in (0,13):
          yield config,common+[(SB.G_SIM_FRAME,4,frame),(O+0x11a,2,0x20),
                              (B+0x2c,4,handle),(B+0x30,4,handle),(B+0x34,4,handle)],hooks,[('ancestralrun',B,L)]
      yield config,common+[(O+0x11a,2,0x20)],change('beam_create',0),[('ancestralrun',B,L)]
      for ground in (0xc28a0001,0xc28a0000,0x42960000,0x42960001):
        yield config,common,change('terrain_heightAtPoint',ground),[('ancestralrun',B,L)]
      for duration,clock in ((0,0),(0x3c888888,0),(0x3c888887,0),(0x3c888889,0),(0x41200000,0x41200000)):
        yield config,common+[(B+0x20,4,duration),(B+0x1c,4,clock)],hooks,[('ancestralrun',B,L)]
      for pvp,vote in ((0,1),(1,0),(1,1)):
        yield config,common+[(SB.G_PVP_MODE_A,4,pvp),(O+0x11a,2,0x20)],change('Creature.v22',vote),[('ancestralrun',B,L)]
      for fail in [h[0] for h in hooks]:
        hs=change(fail,0,0,1)
        if fail in ('SpellAncestralCall_typeinfo','creature_findSpellByType'):
          yield config,common,hs,[('ancestralcan',S,O)]
        elif fail in ('pool_new','res_find','libc_sprintf','sound_playOnObject','anim_startBlendInOut','LightEffect_ctor','particleGroup_create'):
          yield config,common,hs,[('ancestralcast',S,5,O)]
        yield config,common+[(SB.G_SIM_FRAME,4,0),(O+0x11a,2,0x20),(SB.G_PVP_MODE_A,4,1),(B+0x20,4,0)],hs,[('ancestralrun',B,L)]
        if fail=='sound_stop':
          yield config,common+[(O+0x4a8,4,13)],hs,[('ancestralrun',B,L)]
      for cb,field,value in (('anim_startBlendInOut',B+0xc,N),('fx_shockModel',B+0x18,N),
                             ('net_objOrGhost',B+0xc,N),('Creature.v22',B+0xc,N),
                             ('beam_update',B+0x18,N),('sound_stop',B+0x18,N),
                             ('particleGroup_end',B+0x20,0),('terrain_heightAtPoint',O+0x18,0),
                             ('Player_animStep',O+0x11a,0)):
        hs=change(cb,value,field)
        yield config,common+[(O+0x11a,2,0x20),(SB.G_PVP_MODE_A,4,1)],hs,[('ancestralcast',S,5,O),('ancestralrun',B,L)]
        if cb=='sound_stop': yield config,common+[(O+0x4a8,4,13)],hs,[('ancestralrun',B,L)]
        if cb=='Creature.v22': yield config,common+[(SB.G_PVP_MODE_A,4,1)],hs,[('ancestralquery',B,N,H,H+4)]
      # Callback mutation/throw through statistic dispatch, signed adjustment,
      # and live list edits. Duplicate entries deliberately remain observable.
      for ret,mutation,throws in (('q:1:125:130',0,0),('q:1:125:130',B+0xc,0),
                                  (0,0,1),(0,0,2)):
        hs=hooks+[('Skill.v10',ret,mutation,throws)]
        if mutation: hs=hooks+[('Skill.v10',N,mutation,throws)]
        yield config,common+[(T+84,4,0x777200)],hs,[('ancestralquery',B,N,H,H)]
      for count in (0,0xffffffff,4):
        yield config,common+[(SB.G_ALIVE_COUNT,4,count),(SB.G_ALIVE_LIST+12,4,1)],hooks,[('ancestralrun',B,L)]
      for vote,field in ((1,B+0x18),(0,SB.G_ALIVE_COUNT)):
        hs=change('Creature.v22',vote,field)
        yield config,common+[(SB.G_PVP_MODE_A,4,1)],hs,[('ancestralrun',B,L)]
      # Owner and caster copy have distinct roles in query versus run.
      yield config,common+[(B+0xc,4,N)],hooks,[('ancestralquery',B,N,H,H+4),('ancestralrun',B,L)]
      yield config,common+[(B+0x18,4,N)],hooks,[('ancestralquery',B,O,H,H+4),('ancestralrun',B,L)]
      yield config,common+[(O+0x3c8,4,MASK)],hooks,[('ancestralctor',B,5,O,S)]
      yield config,common,hooks,[('ancestraltint',B,N)]
      # Full short lifetime: animation completion, ring, aura, expiry.
      yield config,common+[(S+0x1c,2,0)],hooks,[('ancestralcast',S,5,O),('ancestralrun',B,L)]
      for adj in (0,16,65520):
        origin=B+(-16 if adj==16 else 16 if adj else 0)
        yield config,common+[(origin+0x14,4,0x5197d8),(0x5197d8+16,2,adj),
                             (0x5197d8+20,4,0x2e0d10)],hooks,[('spellvirtual',origin,2,L,0)]
      yield config,common+[(T+12,4,0x2e060c)],hooks,[('virtual',S,1,O)]
      yield config,common+[(T+20,4,0x2e0750)],hooks,[('virtual',S,2,5,O,L,0)]

    # Melee buffs and strikes: complete sections 49, 53 and 35; append only.
    for config in range(16):
      victim=H+0x1000; ally=H+0x2000; ghost=H+0x3000; classdef=H+0x4000; clip=H+0x5000
      common=[(S+0x60,4,T),(S+0x64,4,53),(B,1,5),(B+8,4,S),(B+0xc,4,O),(B+0x10,4,0x12345678),
              (B+0x18,4,0x40800000),(O+0xc,4,0x40),(O+0x3c8,4,0),(O+0x11a,2,0x10),
              (O+0x10,4,0x3f800000),(O+0x14,4,0x80000000),(O+0x18,4,0x40400000),(O+0x38,2,0xffff),
              (O+0x24,4,classdef),(classdef,4,77),(classdef+0x7c,4,clip),(O+0x544,4,N),
              (SB.G_RAND_SEED,4,0x98765432),(SB.G_SIM_FRAME,4,40),(SB.G_HANDLE_FREE,4,pool),
              (SB.G_PLAYERS+0x43fc,4,1),(SB.G_PLAYERS+0x4a86,1,0),
              (SB.G_ALIVE_COUNT,4,3),(SB.G_ALIVE_LIST,4,0),(SB.G_ALIVE_LIST+4,4,1),(SB.G_ALIVE_LIST+8,4,2),
              (SB.G_PLAYERS+SB.REC_OBJ,4,O),(SB.G_PLAYERS+SB.REC_SIZE+SB.REC_OBJ,4,ally),
              (SB.G_PLAYERS+2*SB.REC_SIZE+SB.REC_OBJ,4,victim),
              (victim+0xc,4,0x20),(victim+0x228,4,0x42c80000),(victim+0x10,4,0x40800000),
              (ally+0xc,4,0x20),(ally+0x10,4,0x41000000),(ghost+0x10,4,0x40000000)]
      for k in range(12): common += [(pool+32*k,4,pool+32*(k+1) if k<11 else 0)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for index,value in enumerate((10,100,2,2,5)):
        common += [(S+0xe+2*index,2,1),(S+0x18+2*index,2,value),(S+0x24+4*index,4,0)]
      hooks=[('pool_new',B,0,0),('res_find',clip,0,0),('sound_playOnObject',0,0,0),
             ('text_lookup',77,0,0),('hud_showMessage',0,0,0),('anim_startBlendInOut',0,0,0),
             ('world_collectObjectsInArc','l:'+str(victim)+':'+str(ally),0,0),
             ('world_collectInSphere','l:'+str(ally),0,0),('world_collectObjectsInBox','l:'+str(O)+':'+str(victim)+':'+str(ally),0,0),
             ('Player_animStep',2,0,0),('Player_move',0,0,0),('Creature.v20',1,0,0),('Creature.v22',1,0,0),
             ('Creature.v26',0,0,0),('Creature.v30',0,0,0),('Creature.v16',N,0,0),
             ('net_objOrGhost',ghost,0,0),('net_isLocalSlot',1,0,0),('model_getSamplePointCount',7,0,0),
             ('model_getSamplePoint','p:1065353216:1073741824:1077936128:0',0,0),
             ('model_getAttachPoint','l:1065353216:1073741824:1077936128',0,0),
             ('particle_spawn',0,0,0),('LightEffect_ctor',0,0,0,5,0),('damage_apply',0,0,0),
             ('light_link',0,0,0),('light_relink',0,0,0),('light_unlink',0,0,0),('particleGroup_create',88,0,0),
             ('particleGroup_begin',0,0,0),('particleGroup_setMatrix',0,0,0),('particleGroup_end',0,0,0),
             ('terrain_heightAtPoint',0x40400000,0,0),('SpellCriticalHit_typeinfo',19,0,0),
             ('creature_findSpellByType',1,0,0),('player_getSkillRank','q:3:5:2:8:9:10',0,0),
             ('trail_create','q:1:2:3:0:5:6:7:8:9:10',0,0),('trail_addPoint',0,0,0),
             ('player_voice',0,0,0),('camera_shake',0,0,0),('fx_spawn',0,0,0),('Spell.v1',0,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [h for h in hooks if h[0]!=name]+[(name,value,mutation,throws,*action)]
      # Eligibility, null allocation, signed byte levels and constructors.
      for flags in (0,0x20,0x40):
        for equipped in (0,1):
          for weapon in (0,4,255):
            yield config,common+[(O+0xc,4,flags),(SB.G_PLAYERS+0x43fc,4,equipped),(SB.G_PLAYERS+0x4a86,1,weapon)],hooks,[('bashcan',S,O)]
      for found in (0,1,MASK):
        yield config,common,change('creature_findSpellByType',found),[('critcan',S,O)]
      for prefix in ('bash','crit','charge'):
        for level in (0,1,5,6,15,16,127,128,255,MASK):
          for block in (0,B):
            yield config,common,change('pool_new',block),[(prefix+'cast',S,level,O)]
        yield config,common,hooks,[(prefix+'ctor',B,5,O,S)]
      # Bash's exact first-object target bug, including null and touch-only entries.
      for objects in ((),(0,),(victim,),(victim,ally),(victim,ally,victim)):
        for flags in (0,0x1000000):
          yield config,common+[(victim+0xc,4,flags)],change('world_collectObjectsInArc','l:'+':'.join(map(str,objects))),[('bashctor',B,5,O,S)]
      yield config,common,hooks,[('bashwind',B)]
      for count in (1,257,0xfffffff9):
        yield config,common,change('model_getSamplePointCount',count),[('bashwind',B)]
      for level in (0,5,6,15,16,255):
        for most in (0,1,2):
          for hp in (0,0x80000000,0x42c80000):
            yield config,common+[(B,1,level),(B+0x18,4,0),(B+0x24,4,victim),(S+0x1e,2,most),(victim+0x228,4,hp)],hooks,[('bashrun',B)]
      for chance in (0,1,50,100):
        yield config,common+[(B+0x18,4,0),(S+0x1a,2,chance)],hooks,[('bashrun',B)]
      for flags in (0,0x20,0x40,0x1000000,0x1000060):
        for vote in (0,1):
          yield config,common+[(B+0x18,4,0),(victim+0xc,4,flags),(ally+0xc,4,flags)],change('Creature.v20',vote),[('bashrun',B)]
      for ev in (0,1,2):
        for done in (0,1):
          for status,latch,local in ((0,0,1),(0x10,0,0),(0x10,0,1),(0x10,1,1)):
            hs=change('Player_animStep',ev); hs=[h for h in hs if h[0]!='net_isLocalSlot']+[('net_isLocalSlot',local,0,0)]
            yield config,common+[(B+0x18,4,done),(O+0x11a,2,status),(O+0x450,4,latch)],hs,[('bashrun',B)]
      yield config,common,hooks,[('bashdone',B,L),('bashrange',B,0),('bashrange',B,1),('rangehelper',S,0,5,0,0x3e4ccccc,0),('rangehelper',S,0,5,0,0x3e4ccccc,1)]
      # Critical countdown, fading boundaries, three IDs, ring rejection and aura.
      for seconds in (0,0x3c888888,0x3c888889,0x3f800000,0x40000000,0x40400000,0x40800000):
        for id in (33,53,81,0):
          yield config,common+[(B+0x18,4,seconds),(S+0x64,4,id),(B+0x58,4,88)],hooks,[('critrun',B)]
      for reach in (0,1,0xffff,0x8000):
        for ground in (0xc2900001,0xc2900000,0,0x42900000,0x42900001):
          yield config,common+[(S+0x1a,2,reach),(ghost+0x18,4,0)],change('terrain_heightAtPoint',ground),[('critrun',B)]
      for pvp in (0,1):
        for vote in (0,1):
          for own in (0,1,5,0xffffffff):
            hs=change('Creature.v22',vote)
            yield config,common+[(SB.G_PVP_MODE_A,4,pvp)],hs,[('critbonus',S,O,own),('critrun',B)]
      for found in (0,1):
        for rank in ('q:0:0:0','q:1:3:2','q:4294967293:4294967294:4294967295'):
          hs=change('creature_findSpellByType',found); hs=[h for h in hs if h[0]!='player_getSkillRank']+[('player_getSkillRank',rank,0,0)]
          yield config,common,hs,[('critbonus',S,O,0)]
      for position in (0x4479c000,0x447a0000,0x447a4000,0x44960000):
        yield config,common+[(ally+0x10,4,position),(victim+0x10,4,position)],hooks,[('critbonus',S,O,0)]
      for count in (0,MASK,4):
        yield config,common+[(SB.G_ALIVE_COUNT,4,count),(SB.G_ALIVE_LIST+12,4,1)],hooks,[('critrun',B),('critbonus',S,O,0)]
      yield config,common,hooks,[('crittint',B,victim)]
      # Charge duration is an unsigned difference; null ghosts still get stores in run.
      for elapsed in (0,44,45,46,0x7fffffff,0x80000000,MASK):
        for contact in (0,1):
          for flags in (0,0x40):
            yield config,common+[(B+0x44,4,(40-elapsed)&MASK),(B+0x48,4,contact),(O+0xc,4,flags)],hooks,[('chargerun',B),('chargestat',B)]
      for ghostvalue in (0,ghost):
        for weapon in (0,4,255):
          yield config,common+[(B+0x48,4,1),(SB.G_PLAYERS+0x4a86,1,weapon)],change('net_objOrGhost',ghostvalue),[('chargerun',B)]
          yield config,common,change('net_objOrGhost',ghostvalue),[('chargecast',S,5,O)]
      for target in (0,victim):
        for flags in (0,8,0x20,0x28):
          for radius in (0,1,0xffff):
            yield config,common+[(victim+0xc,4,flags),(S+0x1c,2,radius)],hooks,[('chargecontact',B,target),('chargecontact',B,target)]
      for vote in (0,1):
        yield config,common,change('Creature.v20',vote),[('chargecontact',B,victim)]
      for objects in ((),(0,O,victim,ally,ally),(ally,victim)):
        yield config,common,change('world_collectObjectsInBox','l:'+':'.join(map(str,objects))),[('chargecontact',B,victim)]
      for value in (0,1,45,46,0x7fffffff,0x80000000,0x80000001,MASK):
        yield config,common,hooks,[('uintfloat',value)]
      for index in (0,9,MASK):
        yield config,common,hooks,[('trailpoint',O,0x41200000,0xc1200000,index,k) for k in range(3)]
      # Each relevant callback is tested on a reachable failure path, with writes retained.
      routes={
        'bashcan':('text_lookup','hud_showMessage'),
        'bashcast':('res_find','sound_playOnObject','pool_new','anim_startBlendInOut','world_collectObjectsInArc'),
        'bashwind':('net_objOrGhost','model_getSamplePointCount','model_getSamplePoint','particle_spawn'),
        'bashrun':('Player_animStep','Player_move','Creature.v26','model_getAttachPoint','world_collectObjectsInArc','Creature.v20','damage_apply','Creature.v30','pool_new','LightEffect_ctor','world_collectInSphere','net_isLocalSlot','Spell.v1'),
        'critcan':('SpellCriticalHit_typeinfo','creature_findSpellByType'),
        'critcast':('pool_new','light_link','particleGroup_create'),
        'critrun':('light_relink','light_unlink','net_objOrGhost','particleGroup_begin','particleGroup_setMatrix','terrain_heightAtPoint','particle_spawn','particleGroup_end','Creature.v22','Spell.v1'),
        'critbonus':('SpellCriticalHit_typeinfo','creature_findSpellByType','player_getSkillRank','Creature.v22'),
        'chargecast':('pool_new','light_link','res_find','sound_playOnObject','trail_create','trail_addPoint','net_objOrGhost'),
        'chargerun':('trail_addPoint','Creature.v16','model_getSamplePointCount','model_getSamplePoint','particle_spawn','light_relink','net_objOrGhost','player_voice','anim_startBlendInOut','damage_apply','Spell.v1'),
        'chargecontact':('res_find','sound_playOnObject','camera_shake','fx_spawn','Creature.v20','damage_apply','world_collectObjectsInBox','particle_spawn')}
      for op,names in routes.items():
        for cb in names:
          data=[]
          if op=='bashcan': data=[(SB.G_PLAYERS+0x43fc,4,0)]
          if op=='bashrun':
            data=[(B+0x18,4,0),(B+0x24,4,victim)]
            if cb=='world_collectInSphere': data += [(S+0x1e,2,0)]
            if cb=='Spell.v1': data += [(O+0x11a,2,0)]
          if op=='critrun':
            data=[(SB.G_PVP_MODE_A,4,1)]
            if cb=='light_unlink': data += [(B+0x18,4,0x3f800000)]
            if cb=='Spell.v1': data += [(B+0x18,4,0)]
          if op=='critbonus': data=[(SB.G_PVP_MODE_A,4,1)]
          if op=='chargerun' and cb in ('net_objOrGhost','player_voice','anim_startBlendInOut','damage_apply','Spell.v1'): data=[(B+0x48,4,1)]
          args=(S,O) if op.endswith('can') else (S,5,O) if op.endswith('cast') else (S,O,0) if op=='critbonus' else (B,0 if cb=='fx_spawn' else victim) if op=='chargecontact' else (B,)
          for throws in (1,2): yield config,common+data,change(cb,0,0,throws),[(op,*args)]
      # Mutations exercise re-read timing and live list state; callbacks are scalar.
      for op,cb,value,field in (
          ('bashrun','Player_move',1,B+0x18),('bashrun','damage_apply',0,victim+0x228),
          ('bashrun','Creature.v20',1,B+0xc),('bashrun','LightEffect_ctor',0,B+0x18),
          ('critrun','light_relink',0,B+0xc),('critrun','particleGroup_end',0,SB.G_ALIVE_COUNT),
          ('critbonus','Creature.v22',1,SB.G_ALIVE_LIST+4),('critbonus','player_getSkillRank',5,SB.G_ALIVE_COUNT),
          ('chargecast','trail_create',ghost,B+0xc),('chargerun','trail_addPoint',ghost,B+0xc),
          ('chargecontact','damage_apply',ghost,B+0xc),('chargecontact','camera_shake',0,B+0x48)):
        data=[(B+0x18,4,0)] if op=='bashrun' else [(SB.G_PVP_MODE_A,4,1)] if op.startswith('crit') else []
        args=(S,5,O) if op.endswith('cast') else (S,O,0) if op=='critbonus' else (B,victim) if op=='chargecontact' else (B,)
        yield config,common+data,change(cb,value,field),[(op,*args)]
      # Virtual run/cast/eligibility preserve signed this adjustments.
      for prefix,table,runbody,castbody,canbody in (
          ('bash',0x5195b0,0x2de7dc,0x2de3bc,0x2de334),
          ('crit',0x519c28,0x2e7acc,0x2e74b0,0x2e7224),
          ('charge',0x5183a8,0x2c8f2c,0x2c8864,0)):
        for adj in (0,16,65520):
          origin=B+(-16 if adj==16 else 16 if adj else 0)
          yield config,common+[(origin+0x14,4,table),(table+16,2,adj),(table+20,4,runbody)],hooks,[('spellvirtual',origin,2,L,0)]
        yield config,common+[(T+20,4,castbody)],hooks,[('virtual',S,2,5,O,L,0)]
        if canbody: yield config,common+[(T+12,4,canbody)],hooks,[('virtual',S,1,O)]
      # Short complete lifetimes and a single authoritative strike.
      yield config,common,hooks,[('bashcast',S,5,O),('bashrun',B),('poke',O+0x11a,2,0),('bashrun',B)]
      yield config,common+[(S+0x1c,2,0)],hooks,[('critcast',S,5,O),('critrun',B)]
      yield config,common,hooks,[('chargecast',S,5,O),('chargerun',B),('chargecontact',B,victim),('chargerun',B)]

      # Wide statistic returns, callback mutation and strict plane-range boundaries.
      for value in (0x7fffffff,0x80000000,0xffffffff,1,100):
        hs=hooks+[('Skill.v10',value,0,0)]
        yield config,common+[(T+84,4,0x777999)],hs,[('critrun',B),('critbonus',S,O,1),('chargestat',B)]
      for throws in (0,1,2):
        for field in (B+8,B+0xc,SB.G_ALIVE_COUNT):
          hs=hooks+[('Skill.v10',0,field,throws)]
          yield config,common+[(T+84,4,0x777999)],hs,[('bashrun',B),('critrun',B)]
      for x in (0x4495dfff,0x44960000,0x44960001):
        yield config,common+[(O+0x10,4,0),(O+0x14,4,0),(O+0x18,4,0),
                              (ally+0x10,4,x),(victim+0x10,4,x),(S+0x1a,2,100)],hooks,[('critrun',B)]
      for cb in ('Player_move','Creature.v26','net_isLocalSlot'):
        yield config,common+[(O+0x3c8,4,MASK),(B+0x24,4,victim)],change(cb,0),[('bashrun',B)]
      yield config,common+[(O+0x3c8,4,MASK),(B+0x48,4,1)],hooks,[('chargerun',B)]

    # Section 68: complete resurrection lifecycle, with authored player records.
    for config in range(16):
      target, other, replacement = 0x70000, 0x72000, 0x74000
      existing, phantom, light = 0x76000, 0x78000, 0x7a000
      recs=[SB.G_PLAYERS+i*SB.REC_SIZE for i in range(3)]
      common=[(S+0x60,4,T),(SB.G_NUM_PLAYERS,4,3),(SB.G_ALIVE_COUNT,4,1),
              (SB.G_ALIVE_LIST,4,0),(SB.G_PVP_MODE_A,4,0),(SB.G_SIM_FRAME,4,7),
              (SB.G_RAND_SEED,4,0x12345678),(B,1,5),(B+8,4,S),(B+0xc,4,target),
              (B+0x18,4,target),(B+0x68,4,O),(B+0x60,4,0x3e000000),
              (B+0x64,4,0x3dcccccd),(B+0x1c,4,0),(B+0x14,4,SB.Model.VT_SPELL_RESURRECT),
              (SB.Model.VT_SPELL_RESURRECT+12,4,0x777888),
              (SB.Model.VT_SPELL_RESURRECT+20,4,0x312b30),
              (existing+0x18,4,target)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for index,base in enumerate((1,50,2,100,1)):
        common += [(S+0xe+index*2,2,0),(S+0x18+index*2,2,base),(S+0x24+index*4,4,0)]
      for slot,obj in enumerate((O,target,other)):
        common += [(recs[slot]+SB.REC_OBJ,4,obj),(recs[slot]+0xb0,4,0x42c80000),
                   (obj+0x3c8,4,slot),(obj+0xc,4,0x40),(obj+0x24,4,0x80000+slot*0x100),
                   (obj+0x10,4,(0,0x41800000,0x42000000)[slot]),(obj+0x14,4,0),
                   (obj+0x18,4,0),(obj+0x38,2,0),
                   (0x80000+slot*0x100+0x44,4,0x81800+slot*0x100),
                   (0x80000+slot*0x100+8,4,0x82800+slot*0x100),
                   (SB.Model.G_DEAD_COUNTDOWN+2*slot,2,240 if slot==0 else 60)]
      common += [(replacement+0x3c8,4,1),(replacement+0x10,4,0x41800000)]
      hooks=[(name,0,0,0) for name in ('hud_showMessage','light_link','light_relink',
              'sound_playAtPos','anim_start','fx_emitFromModelVerts','player_resetIdleAnim',
              'arena_addLife','Player_animStep','Spell.v1','light_unlink','StrongStancePhantom_ctor','LightEffect_ctor')]
      hooks += [('text_lookup',0x84000,0,0),('SpellResurrect_typeinfo',0x85000,0,0),
                ('creature_findSpellByType',0,0,0),('pool_new',B,0,0),('res_find',0x86000,0,0),
                ('player_respawn',replacement,0,0,4,recs[1]+SB.REC_OBJ)]
      def change(name,value=0,mutation=0,throws=0,action=None):
        result=[h for h in hooks if h[0]!=name]
        h=(name,value,mutation,throws)
        result.append(h if action is None else h+action)
        return result
      # All seven bodies, single/multiple/negative player-count branches and null alloc.
      for count in (-1,0,1,2,3):
        data=common+[(SB.G_NUM_PLAYERS,4,count&MASK)]
        for op,args in (('revivecan',(S,O)),('revivetarget',(S,O)),('reviveraised',(S,1)),
                        ('revivecast',(S,5,O)),('revivector',(B,5,target,S,O)),
                        ('reviverun',(B,)),('revivedeath',(B,))):
          yield config,data,hooks,[(op,*args)]
        for allocation in (0,B):
          yield config,data,change('pool_new',allocation),[('revivecast',S,5,O)]
      # Strict forward and sideways bounds, tie preference, signed countdowns/angles.
      for angle in (0,0x4000,0x8000,0xc000,0x7fff,0xffff):
        for axis in (0x10,0x14):
          for value in (0xc1400001,0xc1400000,0xc13fffff,0x4307ffff,0x43080000,0x43080001,
                        0x425fffff,0x42600000,0x42600001,0xc7800000,0,0x80000000):
            data=common+[(O+0x38,2,angle),(target+axis,4,value),(recs[2]+SB.REC_OBJ,4,0)]
            yield config,data,hooks,[('revivetarget',S,O)]
      for countdown in (0,60,61,62,240,32767,32768,65535):
        for obj in (0,target):
          yield config,common+[(SB.Model.G_DEAD_COUNTDOWN+2,2,countdown),(recs[1]+SB.REC_OBJ,4,obj),
                              (recs[2]+SB.REC_OBJ,4,0)],hooks,[('revivecan',S,O),('revivetarget',S,O),('revivecast',S,5,O)]
      yield config,common+[(other+0x10,4,0x41800000)],hooks,[('revivetarget',S,O)]
      for raised in (0,existing):
        for slot in (0,1,2,0xffffffff):
          yield config,common,change('creature_findSpellByType',raised),[('reviveraised',S,slot),('revivetarget',S,O)]
      for player in (0,0x40):
        yield config,common+[(SB.G_PVP_MODE_A,4,1),(O+0xc,4,player)],hooks,[('revivecan',S,O)]
        yield config,common+[(O+0xc,4,player),(recs[1]+SB.REC_OBJ,4,0),(recs[2]+SB.REC_OBJ,4,0)],hooks,[('revivecan',S,O)]
      # Dynamic alive traversal re-reads count/list after callbacks, signed slots.
      for count in (-1,0,1,2):
        data=common+[(SB.G_ALIVE_COUNT,4,count&MASK),(SB.G_ALIVE_LIST+4,4,0)]
        yield config,data,change('creature_findSpellByType','q:0:'+str(existing)),[('reviveraised',S,1)]
      for field,value in ((SB.G_ALIVE_COUNT,0),(SB.G_ALIVE_LIST,1),(existing+0x18,other)):
        cb='SpellResurrect_typeinfo' if field==SB.G_ALIVE_LIST else 'creature_findSpellByType'
        yield config,common,change(cb,value,field),[('reviveraised',S,1)]
      for slot in (-1,0x7fffffff,0x80000000):
        yield config,common+[(SB.G_ALIVE_LIST,4,slot&MASK)],hooks,[('reviveraised',S,slot&MASK)]
      # Constructor exact stores/double rate, signed level byte, mode changed by service.
      for seconds in (-32768,-1,0,1,2,32767):
        for count in (0,1,2):
          for level in (0,127,128,255):
            data=common+[(SB.G_NUM_PLAYERS,4,count),(S+0x18,2,seconds&65535),(S+0x1c,2,seconds&65535)]
            yield config,data,hooks,[('revivector',B,level,target,S,O)]
      for value in (0,0xffffffff,0x80000000,0x7fffffff):
        yield config,common+[(T+84,4,0x777999)],change('Skill.v10',value),[('revivector',B,5,target,S,O)]
      for cb,value,field in (('light_link',O,B+0xc),('light_link',1,SB.G_NUM_PLAYERS),
                             ('res_find',1,SB.G_NUM_PLAYERS),('sound_playAtPos',other,B+0x18),
                             ('pool_new',B,recs[1]+SB.REC_OBJ)):
        op='revivecast' if cb=='pool_new' else 'revivector'
        args=(S,5,O) if op=='revivecast' else (B,5,target,S,O)
        yield config,common,change(cb,value,field),[(op,*args)]
      # Run threshold, once-only flag, pulse wrapped frames and both mode switches.
      for left in (0,0x80000000,0xbc888888,0x3c888887,0x3c888888,0x3c888889,
                   0x3e7fffff,0x3e800000,0x3e800001,0x3f800000):
        for count in (0,1,2):
          for flag in (0,1):
            yield config,common+[(B+0x60,4,left),(B+0x1c,4,flag),(SB.G_NUM_PLAYERS,4,count)],hooks,[('reviverun',B)]
      for frame in (0,1,0xffffffff,0x7fffffff,0x80000000,65536,0xffff0000):
        yield config,common+[(SB.G_SIM_FRAME,4,frame)],hooks,[('reviverun',B)]
      for coordinate in (0,0x80000000,0x3f800000,0x7f800000):
        yield config,common+[(target+0x10,4,coordinate)],hooks,[('reviverun',B)]
      for cb,value,field in (('light_relink',2,SB.G_NUM_PLAYERS),('fx_emitFromModelVerts',other,B+0x18),
                             ('fx_emitFromModelVerts',1,B+0x1c),('player_resetIdleAnim',other,recs[1]+SB.REC_OBJ),
                             ('arena_addLife',other,B+0xc),('Player_animStep',0,B+0x60)):
        data=common+([(SB.G_NUM_PLAYERS,4,1)] if cb=='light_relink' else [])
        yield config,data,change(cb,value,field),[('reviverun',B)]
      # Chance is signed; allocations and delegated constructors preserve draw count.
      for chance in (-1,0,1,50,99,100,101,32767):
        for seed in (0,1,0xffffffff,0x12345678):
          for alloc in ('q:0:0','q:'+str(phantom)+':0','q:0:'+str(light),'q:'+str(phantom)+':'+str(light)):
            data=common+[(S+0x1e,2,chance&65535),(SB.G_RAND_SEED,4,seed)]
            yield config,data,change('pool_new',alloc),[('revivedeath',B)]
      for cb,value,field in (('StrongStancePhantom_ctor',other,B+0x18),('LightEffect_ctor',other,B+0x18),
                             ('pool_new',light,B+0x68),('light_unlink',other,B+0x18)):
        data=common+([(S+0x1e,2,0)] if cb=='light_unlink' else [])
        yield config,data,change(cb,value,field),[('revivedeath',B)]
      # Callback failures at every external boundary, earlier prefixes retained.
      for op,args,names in (
          ('revivecan',(S,O),('SpellResurrect_typeinfo','creature_findSpellByType')),
          ('revivector',(B,5,target,S,O),('light_link','res_find','sound_playAtPos','anim_start')),
          ('reviverun',(B,),('fx_emitFromModelVerts','player_respawn','player_resetIdleAnim','arena_addLife','Player_animStep','Spell.v1')),
          ('revivedeath',(B,),('pool_new','StrongStancePhantom_ctor','LightEffect_ctor','light_unlink'))):
        for cb in names:
          for throws in (1,2):
            data=common+([(B+0x60,4,0)] if cb=='Spell.v1' else [(S+0x1e,2,0)] if cb=='light_unlink' else [])
            yield config,data,change(cb,0,0,throws),[(op,*args)]
      for cb in ('text_lookup','hud_showMessage'):
        for throws in (1,2):
          yield config,common+[(SB.G_PVP_MODE_A,4,1)],change(cb,0,0,throws),[('revivecan',S,O)]
      for op,args in (('revivecast',(S,5,O)),('revivector',(B,5,target,S,O)),('revivedeath',(B,))):
        for slot in (10,14):
          for throws in (0,1,2):
            data=common+[(T+8*slot+4,4,0x777999)]
            yield config,data,change('Skill.v'+str(slot),5,SB.G_NUM_PLAYERS,throws),[(op,*args)]
      # Internal virtual dispatch preserves signed self adjustment and body names.
      for adjustment in (0,16,65520):
        origin=B+(-16 if adjustment==16 else 16 if adjustment else 0)
        yield config,common+[(origin+0x14,4,0x88000),(0x88010,2,adjustment),
                            (0x88014,4,0x312b30)],hooks,[('spellvirtual',origin,2,L,0)]
        origin=S+(-16 if adjustment==16 else 16 if adjustment else 0)
        for slot,body,args in ((1,0x312168,(O,)),(2,0x3123e4,(5,O,L,0))):
          yield config,common+[(0x89000+8*k,2,0) for k in BODIES]+[(0x89004+8*k,4,v) for k,v in BODIES.items()]+[(origin+0x60,4,0x89000),(0x89000+slot*8,2,adjustment),
                              (0x89004+slot*8,4,body)],hooks,[('virtual',origin,slot,*args)]
      # Complete synthetic one/multiple-player lifetimes; mode join/leave re-read.
      for count in (1,2):
        commands=[('revivecast',S,5,O)]+[('reviverun',B)]*65
        yield config,common+[(SB.G_NUM_PLAYERS,4,count),(S+0x1c,2,1)],hooks,commands
      yield config,common+[(SB.G_NUM_PLAYERS,4,1)],hooks,[('revivecast',S,5,O),('reviverun',B),
        ('poke',SB.G_NUM_PLAYERS,4,2),('reviverun',B),('poke',SB.G_NUM_PLAYERS,4,1),('reviverun',B)]

    # Cyclone 41/84: full standalone family, appended without changing prior rows.
    for config in range(16):
      clip,klass,ghost,pool,other=0x71000,0x72000,0x73000,0x90000,0x74000
      common=[(S+0x60,4,T),(B,1,9),(B+8,4,S),(B+0xc,4,O),(B+0x14,4,SB.Model.VT_SPELL_CYCLONE),
              (B+0x10,4,0x12345678),(B+0x38,4,0x42100000),(B+0x64,2,65500),(B+0x66,2,32760),
              (B+0x68,4,0x3f800000),(B+0x78,4,clip),(O+0x24,4,klass),(klass,4,0x777),
              (O+0xc,4,0x40),(O+0x11a,2,0x30),(O+0x58c,4,6),(O+0x544,4,0x778),
              (O+0x3c8,4,0),(SB.G_PLAYERS+0x4a86,1,1),(clip+4,4,60),(clip+0x14,4,0x3f700000),
              (SB.G_RAND_SEED,4,0x76543210),(SB.G_SIM_FRAME,4,0),(SB.G_HANDLE_FREE,4,pool),
              (L,4,0x3f800000),(L+4,4,0x40000000),(L+8,4,0x40400000)]
      for i in range(3):
        common += [(O+0x10+4*i,4,(0x3f800000,0x40000000,0x40400000)[i]),
                   (B+0x6c+4*i,4,(0x3f800000,0x40000000,0x40400000)[i])]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for index,value in enumerate((30,2,40,0,24)):
        common += [(S+0xe+index*2,2,1),(S+0x18+index*2,2,value),(S+0x24+index*4,4,0)]
      for i in range(32): common += [(pool+i*32,4,pool+(i+1)*32 if i<31 else 0)]
      hooks=[('pool_new',B,0,0),('text_lookup',77,0,0),('hud_showMessage',0,0,0),
             ('light_link',0,0,0),('res_find',clip,0,0),('anim_push',0,0,0),('player_voice',0,0,0),
             ('sound_playOnObject',0,0,0),('world_collectInSphere','l:'+str(other)+':'+str(O)+':'+str(other),0,0),
             ('Player_animStep',0,0,0),('net_objOrGhost',ghost,0,0),('Spell.v1',0,0,0),
             ('anim_startBlendInOut',0,0,0),('net_getCamYaw',0,0,0),('Player_move',0,0,0),
             ('light_relink',0,0,0),('particle_spawn',0,0,0),('model_getAttachPoint','l:1077936128:1073741824:1077936128',0,0),
             ('Creature.v20',1,0,0),('Player_meleeHit',0,0,0),('net_isLocalSlot',1,0,0)]
      def change(name,ret=0,mutation=0,throws=0,*action):
        return [h for h in hooks if h[0]!=name]+[(name,ret,mutation,throws,*action)]
      for flags in (0,0x40,0x20040):
        for weapon in (0,4,6,255):
          yield config,common+[(O+0xc,4,flags),(SB.G_PLAYERS+0x4a86,1,weapon)],hooks,[('cyclonecan',S,O)]
      for flags in (0,0x40,0x20000,0x20040):
        for level in (0,1,9,127,128,255):
          for length in (0,1,60,0xffffffff,0x7fffffff):
            yield config,common+[(O+0xc,4,flags),(clip+4,4,length)],hooks,[('cyclonector',B,level,O,S)]
      yield config,common,change('pool_new',0),[('cyclonecast',S,9,O,L,65535)]
      # Constructor services may mutate state consumed later in the same body.
      for cb,value,field in (('light_link',other,B+0xc),('res_find',clip,B+0x78),
                             ('anim_push',0x20000,O+0xc),('player_voice',7,O+0x3c8),
                             ('sound_playOnObject',0,B+0x18),('world_collectInSphere','l:',0)):
        yield config,common,change(cb,value,field),[('cyclonecast',S,9,O,L,0)]
      # Spin/stop and animation event boundaries, signed frame and both periods.
      for phase in (0,1):
        for mark in (0,1):
          for event in (0,1,4,5):
            for left in (0,0x3d937089,0x3d93708a,0x3d93708b,0x3f800000):
              yield config,common+[(B+0x5c,4,phase),(B+0x60,4,mark),(B+0x68,4,left)],change('Player_animStep',event),[('cyclonerun',B,L,65535)]
      for frame in (1,3,4,5,12,0xffffffff,0xfffffffc,0x7fffffff,0x80000000,65536,0xffff0000):
        for flag in (0,0x100):
          for level in (8,9,127,128):
            yield config,common+[(SB.G_SIM_FRAME,4,frame),(SB.G_LEVEL_FLAGS,4,flag),(B,1,level)],hooks,[('cyclonerun',B,L,0x8000)]
      for status in (0,0x10,0x20,0x30):
        for action in (2,6,7):
          for net in (0,1,255):
            yield config,common+[(O+0x11a,2,status),(O+0x58c,4,action),(SB.G_NETWORK_MODE,1,net),(B+0x68,4,0)],hooks,[('cyclonerun',B,L,0)]
      for slot in (0,1,0xffffffff):
        for track in (0,1,127,128,255):
          for yaw in (0,0x4000,0x8000,0xffffffff,0x80000000):
            pad=(SB.G_PADS+slot*0x58)&MASK
            data=common+[(O+0x3c8,4,slot),(O+0x118,1,track),(B+0x68,4,0),
                         (pad+0x14,4,0x3f800000),(pad+0x18,4,0xbf800000)]
            hs=change('net_getCamYaw',yaw)+[('Player_move',0x40800000,O+0x10,0)]
            yield config,data,hs,[('cyclonerun',B,L,0)]
      # Live candidate traversal, duplicates, strict distance, dynamic next/target.
      targets=[other,other+0x100,other+0x200]
      for distance in (0x41c00000,0x41e4acfd,0x424e5666,0x43000000):
        for vote in (0,1):
          data=common+[(B+0x18,4,N)]
          for i,obj in enumerate((targets[0],0,targets[1],targets[0],targets[2])):
            data += [(N+i*32,4,N+(i+1)*32 if i<4 else 0),(N+i*32+0xc,4,obj)]
          for obj in targets: data += [(obj+0x10,4,distance),(obj+0x14,4,0x40000000),(obj+0x1c,4,0)]
          yield config,data,change('Creature.v20',vote),[('cyclonerun',B,L,0)]
          yield config,data,change('Creature.v20',vote,N,0),[('cyclonerun',B,L,0)]
          yield config,data,change('Player_meleeHit',0,N+0xc,0),[('cyclonerun',B,L,0)]
      # FX direct API: signed frame remainder, angles, spark threshold and zero signs.
      for frame in (0,1,3,0xffffffff,0xfffffffd,0x80000000,0xffff0001,65535,65536):
        for angle in (0,32767,32768,65535):
          for level in (8,9,255):
            yield config,common+[(SB.G_SIM_FRAME,4,frame),(B+0x64,2,angle),(B+0x66,2,angle),(B,1,level)],hooks,[('cyclonefx',B,L)]
      # Every external call can fail, retaining its preceding writes and draws.
      for op,args,names in (
        ('cyclonecan',(S,O),('text_lookup','hud_showMessage')),
        ('cyclonecast',(S,9,O,L,0),('pool_new','light_link','res_find','anim_push','player_voice','sound_playOnObject','world_collectInSphere')),
        ('cyclonerun',(B,L,0),('net_objOrGhost','Player_animStep','Spell.v1','anim_startBlendInOut','net_getCamYaw','Player_move','light_relink','particle_spawn','model_getAttachPoint','Creature.v20','Player_meleeHit','net_isLocalSlot'))):
        for cb in names:
          for throws in (1,2):
            data=common+[(SB.G_PLAYERS+0x4a86,1,4),(SB.G_NETWORK_MODE,1,1),(B+0x68,4,0),
                         (B+0x18,4,N),(N+0xc,4,other),(other+0x10,4,0x41e4acfd),(other+0x14,4,0x40000000)]
            if cb=='Spell.v1': data += [(O+0x11a,2,0)]
            yield config,data,change(cb,0,0,throws),[(op,*args)]
      for cb,value,field in (('Player_animStep',5,B+0x78),('Player_move',0x40800000,O+0x10),
                             ('light_relink',0,B+0x60),('particle_spawn',0,B+0x64),
                             ('model_getAttachPoint','l:0:0:0',0),('net_isLocalSlot',0,O+0x3c8)):
        yield config,common,change(cb,value,field),[('cyclonerun',B,L,0)]
      # Callback-visible shared header replacement, virtual self adjustment.
      for adjustment in (0,16,65520):
        origin=B+(-16 if adjustment==16 else 16 if adjustment else 0)
        yield config,common+[(origin+0x14,4,0x88000),(0x88010,2,adjustment),(0x88014,4,0x2d1a94)],hooks,[('spellvirtual',origin,2,L,0)]
        origin=S+(-16 if adjustment==16 else 16 if adjustment else 0)
        yield config,common+[(origin+0x60,4,0x88000),(0x88008,2,adjustment),(0x8800c,4,0x2d1484)],hooks,[('virtual',origin,1,O)]
      for net in (0,1):
        yield config,common+[(SB.G_NETWORK_MODE,1,net)],change('Player_animStep','q:1:1:1:4:0'),[
          ('cyclonecast',S,9,O,L,0),('cyclonerun',B,L,0),('poke',B+0x68,4,0),('cyclonerun',B,L,0),
          ('cyclonerun',B,L,0),('cyclonerun',B,L,0),('poke',O+0x11a,2,0),('cyclonerun',B,L,0)]


    # Cone of Frost and DummyDamager: append only, synthetic tables and services.
    for config in range(16):
      D,V,other,pool=0x70000,0x71000,0x72000,0x90000
      common=[(S+0x60,4,T),(B,1,9),(B+8,4,S),(B+0xc,4,O),(B+0x10,4,0x12345678),
              (B+0x14,4,SB.Model.VT_SPELL_CONEOFFROST),(SB.G_SIM_FRAME,4,0),
              (SB.G_RAND_SEED,4,0x76543210),(SB.G_HANDLE_FREE,4,pool),
              (D+0x48,4,SB.Model.VT_DUMMYDAMAGER),(D+0x44,4,0x76543210),
              (D+0x4c,4,N),(N+0xc,4,O),(D+0x60,4,0x12),(D+0x64,4,0x80c040),
              (D+0x68,4,0x4239aabb),(D+0x6c,2,45),(D+0x1c,4,0x41200000),
              (other+0xc,4,0x21),(other+0x1c,4,0x41200000)]
      common += [(SB.Model.VT_DUMMYDAMAGER+36,4,0x1ee330),
                 (SB.Model.VT_DUMMYDAMAGER+52,4,0x1ee7b0)]
      for slot,body in BODIES.items(): common += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for i in range(5): common += [(S+0xe+2*i,2,1),(S+0x18+2*i,2,(100,7,13,0,5)[i]),(S+0x24+4*i,4,0x3f000000)]
      for i in range(3):
        for addr in (L,O+0x10,D+0x10,other+0x10): common += [(addr+4*i,4,(0x3f800000,0x40000000,0x40400000)[i])]
        common += [(V+4*i,4,(0x41200000,0xc1000000,0)[i]),(D+0x54+4*i,4,(0x41200000,0xc1000000,0)[i])]
      for i in range(12): common += [(pool+i*24,4,pool+(i+1)*24 if i<11 else 0)]
      hooks=[('pool_new',D,0,0),('light_link',0,0,0),('res_find',0x778,0,0),
             ('sound_playAtPos',0x779,0,0),('particle_spawn',0,0,0),('light_relink',0,0,0),
             ('GameObject_ctor',0,0,0,5,0),('obj_runEveryFrame',0,0,0),
             ('obj_updateGridCell',0,0,0),('terrain_height',0,0,0),('water_test',0,0,0),
             ('world_collectObjectsInBox','l:',0,0),('Creature.v1',0,0,0),('Creature.v20',1,0,0),
             ('Creature.v6',0,0,0),('StaticFire_ctor',0,0,0,5,0),('damage_apply',0,0,0),('named',0,0,0)]
      def change(name,value=0,mutation=0,throws=0):
        return [(n,value,mutation,throws,*a) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      for lv in (0,1,9,127,128,255,65535,0x80000000):
        yield config,common,hooks,[('conector',B,lv,O,S)]
        for allocated in (0,B):
          yield config,common,change('pool_new',allocated),[('conecast',S,lv,O,L,65535)]
      for seed in (0,1,2,3,4,5,0xffff,0x12345678,0xffffffff):
        for frame in (0,1,7,8,0xffffffff,0x80000000):
          yield config,common+[(B+0x10,4,seed),(SB.G_SIM_FRAME,4,frame)],hooks,[('conerun',B,L,32768)]
      for st in (0,1,65535,32768,32767):
        for facing in (0,65535,32767,32768):
          yield config,common+[(S+0x1c,2,st),(S+0x1a,2,st)],hooks,[('conerun',B,L,facing)]
      for bits in range(32):
        for owner in (0,O):
          yield config,common,hooks,[('dummyctor',D,L,V,45,10,bits,0x80c040,owner,0x4239aabb,65535)]
        yield config,common+[(D+0x60,4,bits)],hooks,[('dummytouch',D,other),('dummytouch',D,other)]
        yield config,common+[(D+0x60,4,bits)],change('terrain_height',0x42c80000),[('dummyrun',D)]
        yield config,common+[(D+0x60,4,bits)],change('world_collectObjectsInBox','l:'+str(other)),[('dummyrun',D)]
      for life in (0,1,2,32767,32768,65535):
        yield config,common+[(D+0x6c,2,life)],hooks,[('dummyrun',D)]
        yield config,common,hooks,[('dummyctor',D,L,V,life,0xffffffff,0x1f,0,0,0xbf800000,0x8000)]
      for damage in (0,0x80000000,0x3f000000,0xbf800000,0x477fff00,0x47800000,0x4f000000):
        for bits in (0,8,16,24):
          yield config,common+[(D+0x68,4,damage),(D+0x60,4,bits)],hooks,[('dummytouch',D,other)]
      for flags in (0,0x20):
        for owner in (0,N):
          for vote in (0,1):
            yield config,common+[(D+0x4c,4,owner),(other+0xc,4,flags)],change('Creature.v20',vote),[('dummytouch',D,other)]
      for op,args,services in (
        ('conecast',(S,9,O,L,0),('pool_new','light_link','res_find','sound_playAtPos')),
        ('conerun',(B,L,0),('particle_spawn','pool_new','GameObject_ctor','obj_runEveryFrame','light_relink')),
        ('dummyctor',(D,L,V,45,10,0x12,0x80c040,O,0x4239aabb,1),('GameObject_ctor','obj_runEveryFrame')),
        ('dummyrun',(D,),('obj_updateGridCell','terrain_height','world_collectObjectsInBox')),
        ('dummytouch',(D,other),('Creature.v20','damage_apply'))):
        for cb in services:
          for throws in (1,2): yield config,common,change(cb,0,0,throws),[(op,*args)]
      # Live mutation re-reads: skill/level, owner, position, bits and damage.
      for cb,value,field,op,args in (
        ('particle_spawn',255,B,'conerun',(B,L,0)),
        ('particle_spawn',0x80000000,SB.G_SIM_FRAME,'conerun',(B,L,0)),
        ('pool_new',0,0,'conerun',(B,L,0)),
        ('Creature.v20',0x1f,D+0x60,'dummytouch',(D,other)),
        ('Creature.v20',0,D+0x68,'dummytouch',(D,other)),
        ('res_find',other,B+0xc,'conecast',(S,9,O,L,0))):
        yield config,common,change(cb,value,field),[(op,*args)]
      for cb,value,field in (('StaticFire_ctor',2,D+0x60),('particle_spawn',1,D+0x60),
                             ('particle_spawn',0x42c80000,D+0x10)):
        hs=change('terrain_height',0x42c80000)
        hs=[(n,value,field,t,*a) if n==cb else (n,r,m,t,*a) for n,r,m,t,*a in hs]
        yield config,common+[(D+0x60,4,0x18)],hs,[('dummyrun',D)]
      # Internal adjusted virtual dispatch, plus explicit external/named routes.
      for adjustment in (0,16,65520):
        origin=B+(-16 if adjustment==16 else 16 if adjustment else 0)
        data=common+[(origin+0x14,4,0x88000),(0x88010,2,adjustment),(0x88014,4,0x2e9d10)]
        yield config,data,hooks,[('spellvirtual',origin,2,L,0)]
        for slot,body,args in ((4,0x1ee330,()),(6,0x1ee7b0,(other,))):
          origin=D+(-16 if adjustment==16 else 16 if adjustment else 0)
          data=common+[(origin+0x48,4,0x89000),(0x89000+slot*8,2,adjustment),(0x89004+slot*8,4,body)]
          yield config,data,hooks,[('objvirtual',origin,slot,*args)]
          yield config,data,hooks,[('external',1,slot),('namebody',body),('objvirtual',origin,slot,*args)]
      yield config,common+[(T+20,4,0x2e9930)],change('pool_new',B),[('virtual',S,2,9,O,L,0)]
      yield config,common,hooks,[('localvec',L),('objball',D,0),('objball',D,1),('objball',D,2)]
      # Full projectile lifetime and several channel frames (no automatic attachment).
      yield config,common,hooks,[('dummyctor',D,L,V,45,10,0x12,0x80c040,O,0x4239aabb,1)]+[('dummyrun',D)]*45
      commands=[('conecast',S,9,O,L,0)]
      for frame in range(9): commands += [('poke',SB.G_SIM_FRAME,4,frame),('conerun',B,L,frame*4000)]
      yield config,common,change('pool_new','q:'+str(B)+':'+str(D)),commands

    # Elemental arrows / Multi Fire; append without consuming the earlier RNG.
    for config in range(16):
      V,other,prop,pool,light,cloud,bow=0x71000,0x72000,0x73000,0x90000,0x74000,0x75000,0x76000
      base=[(S+0x60,4,T),(O+0xc,4,0x40),(O+0x3c8,4,0),(B+0x4c,4,N),(N+0xc,4,O),
            (B+0x44,4,0x12345678),(B+0x1c,4,0x40e00000),(B+0x20,4,0x42900000),
            (B+0x68,4,2),(B+0x6c,4,2),(B+0xac,4,S),(B+0x70,4,S),(B+0x64,2,32768),
            (other+0xc,4,0x20),(other+0x1c,4,0x41200000),(other+0x20,4,0x42900000),
            (other+0x344,4,0x3f000000),(prop+0xc,4,0),(prop+0x1c,4,0x41200000),
            (prop+0x20,4,0x42900000),(SB.G_RAND_SEED,4,0x76543210),(SB.G_COSMETIC_SEED,4,0x12345678),
            (SB.G_HANDLE_FREE,4,pool),(SB.G_PLAYERS+0x4a86,1,4),(SB.G_PLAYERS+0xd0,4,13),
            (SB.G_PLAYERS+0xd4,4,29),(SB.G_PLAYERS+0x4434,4,bow),(bow+8,4,0x89),
            (SB.Model.G_ITEM_MODELS+8,4,0xabcdef),(SB.Model.G_ITEM_MODELS+12,4,0x987654)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for st in range(5):
        base += [(S+0xe+st*2,2,1),(S+0x18+st*2,2,(100,7,3,3,5)[st]),(S+0x24+st*4,4,0),
                 (S+0x38+st*2,2,1),(S+0x42+st*2,2,(9,11,4,-3,7)[st]&65535),(S+0x4c+st*4,4,0)]
      for i in range(3):
        for addr in (L,B+0x10,O+0x10,other+0x10,prop+0x10): base += [(addr+4*i,4,(0x3f800000,0x40000000,0x42300000)[i])]
        base += [(V+4*i,4,(0x3f800000,0xbf800000,0x3f000000)[i]),(B+0x54+4*i,4,(0x41200000,0xc1000000,0)[i])]
      for i in range(16): base += [(pool+i*24,4,pool+(i+1)*24 if i<15 else 0)]
      hooks=[('pool_new',B,0,0),('GameObject_ctor',0,0,0,5,0),('light_link',0,0,0),
             ('obj_runEveryFrame',0,0,0),('res_find',0x77,0,0),('sound_playOnObject',0,0,0),
             ('obj_updateGridCell',0,0,0),('terrain_height',0,0,0),('water_test',0,0,0),
             ('world_collectObjectsInBox','l:',0,0),('Creature.v6',0,0,0),('Creature.v20',1,0,0),
             ('particle_spawn',0,0,0),('light_relink',0,0,0),('damage_apply',0,0,0),
             ('Creature_setEffectTimer',0,0,0),('obj_delete',0,0,0),('fx_spawn',0,0,0),
             ('fx_ringBurst',0,0,0),('camera_shake',0,0,0),('fx_impactPuffNearCamera',0,0,0),
             ('LightEffect_ctor',0,0,0),('poisonGasSpell_ctor',0,0,0),('MissileWeapon_ctor',0,0,0),
             ('libc_sprintf','s:synthetic-impact',0,0),('item_getModifiedField1a',7,0,0),
             ('text_lookup',0x99,0,0),('hud_showMessage',0,0,0),('named',0,0,0)]
      def change(name,value=0,mutation=0,throws=0):
        return [(n,value,mutation,throws,*a) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      specs=((43,0x518de0,0x2d5fc4,0x2d603c,0x2d66ac,0x2d685c),
             (44,0x518ef8,0x2d6d40,0x2d6db8,0x2d73c8,0x2d7aac),
             (45,0x519010,0x2d877c,0x2d87f4,0x2d8e80,0x2d932c),
             (47,0x519240,0x2dac30,0x2daca8,0x2db17c,0x2db4f0))
      for sid,table,can,cast,runbody,hitbody in specs:
        common=base+[(B+0x48,4,table),(table+36,4,runbody),(table+52,4,hitbody)]
        for player in (0,0x40):
          for weapon in (4,0,255):
            yield config,common+[(O+0xc,4,player),(SB.G_PLAYERS+0x4a86,1,weapon)],hooks,[('arrowcan',sid,S,O)]
          for lv in (0,1,5,6,10,11,12,15,16,255,65535,0x80000000):
            data=common+[(O+0xc,4,player),(B+0x68,4,lv),(B+0x6c,4,lv)]
            # Negative levels have empty cold bursts; large positive cold levels
            # remain within documented host history bounds by testing expiry.
            yield config,data,hooks,[('arrowcast',sid,S,lv,O,L,65535)]
            yield config,data,hooks,[('arrowctor',sid,B,L,32768,O,lv,S)]
            yield config,data,hooks,[('arrowctor2',sid,B,L,V,O,lv,S)]
            if sid!=44 or lv in (0,1,5,6,10,11,12,15,16,0x80000000):
              yield config,data,change('pool_new',0),[('arrowhit',sid,B,other),('arrowhit',sid,B,prop)]
        for seed in (0,1,0xffff,0xffffffff):
          for age in (0,599,600,601,0x7fffffff,0xffffffff):
            data=common+[(B+0x44,4,seed),(B+(0x68 if sid==43 else 0x6c),4,age)]
            yield config,data,hooks,[('arrowrun',sid,B)]
        for flags in (0,0x20,0x1000000,0x1000020):
          for vote in (0,1):
            for player in (0,0x40):
              yield config,common+[(O+0xc,4,player),(other+0xc,4,flags),(other+0xa8,8,0x4000000 if flags==0x20 else 0)],change('Creature.v20',vote),[('arrowhit',sid,B,other)]
        for reason in (0,1,2,0xffffffff):
          for ended in (0,1):
            yield config,common+[(B+0x60,4,ended)],change('pool_new',0),[('arrowend',sid,B,reason),('arrowend',sid,B,reason)]
        for owner in (0,N):
          yield config,common+[(B+0x4c,4,owner)],change('pool_new',0),[('arrowhit',sid,B,other)]
          yield config,common+[(B+0x4c,4,owner)],hooks,[('arrowctor',sid,B,L,0,0,2,S)]
        # Collision calls back through the newly ported object's real slot 6.
        yield config,common,change('world_collectObjectsInBox','l:'+':'.join(map(str,(other,prop,other)))),[('arrowrun',sid,B)]
        yield config,common,change('terrain_height',0x42400000),[('arrowrun',sid,B)]
        for direction in ((0,0,0),(0x80000000,0,0),(0x7f800000,0,0),(0x00800000,0x80800000,0),(0xbf800000,0,0x3f800000)):
          yield config,common+[(V+4*i,4,v) for i,v in enumerate(direction)],hooks,[('arrowctor2',sid,B,L,V,O,2,S)]
        # Exact signed adjustment and explicit forced/named object dispatch.
        for adjustment in (0,16,65520):
          origin=B+(-16 if adjustment==16 else 16 if adjustment else 0)
          for slot,body,args in ((4,runbody,()),(6,hitbody,(other,))):
            data=common+[(origin+0x48,4,0x88000),(0x88000+8*slot,2,adjustment),(0x88004+8*slot,4,body)]
            yield config,data,hooks,[('objvirtual',origin,slot,*args)]
            yield config,data,hooks,[('external',1,slot),('namebody',body),('objvirtual',origin,slot,*args)]
          origin=S+(-16 if adjustment==16 else 16 if adjustment else 0)
          for slot,body,args in ((1,can,(O,)),(2,cast,(2,O,L,0))):
            data=common+[(0x89000+8*k,2,0) for k in BODIES]+[(0x89004+8*k,4,v) for k,v in BODIES.items()]+[(origin+0x60,4,0x89000),(0x89000+8*slot,2,adjustment),(0x89004+8*slot,4,body)]
            yield config,data,hooks,[('virtual',origin,slot,*args)]
        # Fail each reached service; earlier writes and draws remain observable.
        cases=(('arrowcast',(sid,S,2,O,L,0),('pool_new','GameObject_ctor','light_link','obj_runEveryFrame','res_find','sound_playOnObject')),
               ('arrowrun',(sid,B),('obj_updateGridCell','terrain_height','world_collectObjectsInBox','particle_spawn','light_relink','fx_impactPuffNearCamera')),
               ('arrowhit',(sid,B,other),('Creature.v20','damage_apply','Creature_setEffectTimer','fx_spawn','obj_delete')),
               ('arrowend',(sid,B,1),('libc_sprintf','fx_spawn','res_find','sound_playOnObject','particle_spawn','pool_new','LightEffect_ctor','camera_shake','obj_delete')))
        for op,args,names in cases:
          for cb in names:
            for throws in (1,2): yield config,common,change(cb,0,0,throws),[(op,*args)]
        # Mutate live position, owner, ended flag, skill level and weapon record.
        for cb,value,field in (('Creature.v20',1,B+0x68),('Creature.v20',1,N+0xc),
                               ('damage_apply',0,B+0x4c),('damage_apply',0x40,O+0xc),
                               ('particle_spawn',0,B+0x68),('light_relink',0,B+0x60),
                               ('res_find',0x42a00000,B+0x10),('GameObject_ctor',0,O+0xc)):
          yield config,common,change(cb,value,field),[('arrowcast',sid,S,2,O,L,0),('arrowrun',sid,B),('arrowhit',sid,B,other)]
        if sid in (43,44,45):
          yield config,common,hooks,[('arrowcommon',sid,B,L,O,S)]
        if sid in (44,45,47):
          yield config,common,hooks,[('arrowparticles',sid,B),('arrowflight',sid,B,2)]
      # Explosion damage visits creatures and props in query order (including ties).
      for flags in (0,0x40000):
        for player in (0,0x40):
          for lv in (5,6,11,12,16,0xffffffff):
            data=base+[(SB.G_LEVEL_FLAGS,4,flags),(O+0xc,4,player),(B+0x68,4,lv)]
            hs=change('world_collectObjectsInBox','l:'+':'.join(map(str,(O,other,prop,other))))
            hs=[(n,light if n=='pool_new' else r,m,t,*a) for n,r,m,t,*a in hs]
            yield config,data,hs,[('arrowend',47,B,2)]
      for lv in (0,1,2,3,0xffffffff):
        for allocated in (0,cloud):
          yield config,base+[(B+0x68,4,lv)],change('pool_new',allocated),[('arrowend',45,B,0)]
      # Each shared helper has a direct input-driven invocation as well.
      yield config,base,hooks,[('bowcan',O,0x99),('arrowlight',B,L,0x42c00000,0x38e38e38,1,2,65535),('firestats',B,O),
                               ('arrowfinish',B,0x99,0x98),('flightparticle',B,0x99,0xbf800000,0x3f000000)]
      for speed in (0,1,0xffffffff,0x7fffffff,0x80000000):
        yield config,base,hooks,[('arrowvf',B,speed),('arrowvd',B,V,speed)]
      for amount in (0,1,0xffffffff,100,0xffffff9c):
        yield config,base,hooks,[('monsterspread',amount,65535,0),('monsterspread',amount,65535,1),('arrowblocked',other)]
      # Multi Fire uses the original level for damage, adjusted level for count/speed.
      for player in (0,0x40):
        for count in (0,1,2,7,65535):
          for seed in (0,0xffffffff):
            data=base+[(O+0xc,4,player),(S+0x1c,2,count),(S+0x46,2,count),(SB.G_COSMETIC_SEED,4,seed)]
            yield config,data,change('pool_new','q:'+':'.join(map(str,(light,B,0,B)))),[('multicast',S,2,O,L,32768)]
        for setbyte in (0,1,255,128):
          yield config,base+[(O+0xc,4,player),(SB.G_PLAYERS+0x4a7c,1,setbyte)],hooks,[('multican',S,O),('multicast',S,2,O,L,65535)]
      for cb in ('pool_new','LightEffect_ctor','MissileWeapon_ctor','item_getModifiedField1a','res_find','sound_playOnObject'):
        for throws in (1,2): yield config,base,change(cb,0,0,throws),[('multicast',S,2,O,L,0)]
      for cb,value,field in (('item_getModifiedField1a',13,bow+8),('pool_new',0,L+8),('res_find',0x99,O+0x3c8)):
        yield config,base,change(cb,value,field),[('multicast',S,2,O,L,0)]
      # Natural Fire lifetime: 601 movements, followed by an inert run.
      data=base+[(B+0x48,4,0x518de0),(0x518de0+36,4,0x2d66ac),(0x518de0+52,4,0x2d685c)]
      yield config,data,hooks,[('arrowctor',43,B,L,0,O,2,S)]+[('arrowrun',43,B)]*602
      # Short complete lifetimes for every other projectile, starting at age 599.
      for sid,table,_,_,runbody,hitbody in specs[1:]:
        data=base+[(B+0x48,4,table),(table+36,4,runbody),(table+52,4,hitbody)]
        yield config,data,change('pool_new',0),[('arrowctor',sid,B,L,0,O,2,S),('poke',B+0x6c,4,599)]+[('arrowrun',sid,B)]*3

    # Shock lifecycle and recursive chain: independent appended synthetic corpus.
    for config in range(16):
      pool=0x90000
      victims=(0x71000,0x72000,0x73000,0x74000)
      base=[(S+0x60,4,T),(B+0x48,4,SB.Model.VT_SHOCK),(SB.Model.VT_SHOCK+36,4,0x2c42e4),
            (B+0x4c,4,S),(B+0x44,4,0x12345678),(B+0x190,4,0x43aa8000),
            (B+0x188,1,9),(B+0x18a,1,4),(B+0x18c,4,0x472de900),
            (B+0x194,4,N),(N+0xc,4,O),(O+0xc,4,0x40),(SB.G_HANDLE_FREE,4,pool),
            (SB.G_RAND_SEED,4,0x76543210),(SB.G_SIM_FRAME,4,8),(B+0x58,4,0)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for st in range(5):
        base += [(S+0xe+st*2,2,1),(S+0x18+st*2,2,(100,3,0,0,5)[st]),(S+0x24+st*4,4,0),
                 (S+0x38+st*2,2,1),(S+0x42+st*2,2,(10,2,0,0,7)[st]),(S+0x4c+st*4,4,0)]
      for i in range(64): base += [(pool+24*i,4,pool+24*(i+1) if i<63 else 0)]
      for a in (L,B+0x10,O+0x10):
        for i,v in enumerate((0,0,0x42300000)): base += [(a+4*i,4,v)]
      for a,x,y in zip(victims,(0,0x42c80000,0xc2c80000,0x43960000),(0,0,0,0)):
        base += [(a+0xc,4,0x20),(a+0x10,4,x),(a+0x14,4,y),(a+0x18,4,0x42300000),(a+0x1c,4,0x41200000)]
      listing='l:'+':'.join(map(str,victims))
      hooks=[('pool_new',B,0,0),('GameObject_ctor',0,0,0,5,0),('light_link',0,0,0),
             ('res_find',0x77,0,0),('sound_playOnObject',0,0,0),('obj_runEveryFrame',0,0,0),
             ('sound_setVolume',0,0,0),('Skill.v13',3,0,0),('obj_updateGridCell',0,0,0),('terrain_height',0,0,0),
             ('world_collectObjectsInBox',listing,0,0),('Creature.v20',1,0,0),('damage_apply',0,0,0),
             ('beam_create',0x99,0,0),('beam_update',0,0,0),('particle_spawn',0,0,0),
             ('light_relink',0,0,0),('Creature.v1',0,0,0),('named',0,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      for caster in (0,O):
        for flags in (0,0x40):
          for level in (0,1,9,10,20,127,128,255,65535,0x80000000):
            for facing in (0,16384,32768,65535):
              data=base+[(O+0xc,4,flags)]
              yield config,data,hooks,[('shockctor',B,L,facing,caster,S,level)]
              yield config,data,hooks,[('shockcast',S,level,caster,L,facing)]
      yield config,base,change('pool_new',0),[('shockcast',S,9,O,L,0)]
      for cb in ('pool_new','GameObject_ctor','light_link','res_find','sound_playOnObject','obj_runEveryFrame'):
        for throws in (1,2): yield config,base,change(cb,0,0,throws),[('shockcast',S,9,O,L,0)]
      # Null/dead candidates, strict ties, range boundaries, zero/signed maxima.
      for most in (0,1,2,4,8,127,128,255):
        for distance in (0,0x472de900,0x472de901,0x7f800000):
          data=base+[(B+0x18a,1,most),(B+0x18c,4,distance)]
          commands=[('fill',B+0x50,0,*victims,victims[0]),('shockchain',B,L)]
          commands += [('shockin',B,a) for a in (0,*victims)]
          yield config,data,hooks,commands
      for flags in (0,8,0x20,0x28,0xffffffff):
        for owner in (0,N):
          data=base+[(victims[0]+0xc,4,flags),(B+0x194,4,owner)]
          yield config,data,hooks,[('shockwants',B,victims[0],0),('shockwants',B,victims[0],victims[0]),('shockwants',B,B,0)]
          yield config,data,hooks,[('fill',B+0x50,*victims),('shockchain',B,L)]
      # Equal planar distances retain the first candidate, regardless of height.
      yield config,base+[(victims[0]+0x10,4,0x42c80000),(victims[0]+0x18,4,0x7f800000),(B+0x18a,1,1)],hooks,[('fill',B+0x50,*victims),('shockchain',B,L)]
      for state in (0,1,2,128,255):
        for frame in (0,7,8,270,299,300,301,0xffffffff):
          for owner in (0,N):
            data=base+[(B+0x189,1,state),(SB.G_SIM_FRAME,4,frame),(B+0x194,4,owner)]
            yield config,data,hooks,[('shockrun',B)]
      for ground in (0x42300000,0x42300001,0x7fc01234):
        for speed in (0,0x80000000,0x43aa8000):
          yield config,base+[(B+0x190,4,speed)],change('terrain_height',ground),[('shockrun',B)]
      for most in (0,1,4,8,128,255):
        for damage in (0,1,32767,32768,0x80000000,0xffffffff):
          yield config,base+[(B+0x18a,1,most),(T+108,4,0)],change('Skill.v13',damage),[('shockrun',B)]
      for seed in (0,1,0x12345678,0xffffffff):
        data=base+[(B+0x44,4,seed)]
        yield config,data,hooks,[('shockrun',B),('poke',SB.G_SIM_FRAME,4,9),('shockrun',B)]
      for cb in ('sound_setVolume','obj_updateGridCell','terrain_height','world_collectObjectsInBox','Creature.v20','damage_apply','beam_create','particle_spawn','light_relink','Creature.v1'):
        for throws in (1,2):
          yield config,base+[(B+0x58,4,(-292)&MASK)],change(cb,0,0,throws),[('shockrun',B)]
      for cb,value,addr in (('terrain_height',0,SB.G_SIM_FRAME),('obj_updateGridCell',1,B+0x189),
                            ('Creature.v20',1,B+0x194),('damage_apply',0,B+0x194),
                            ('beam_create',0,B+0x189),('light_relink',0,B+0x44)):
        yield config,base,change(cb,value,addr),[('shockrun',B)]
      # Beam traversal re-reads the live next pointer after callback changes.
      commands=[('fill',B+0x54,*victims),('shockrun',B)]
      for handle in (0,0xffffffff,0x99):
        data=base+[(B+0x189,1,1)]+[(B+0x12c+4*i,4,handle) for i in range(4)]
        yield config,data,hooks,commands
        yield config,data,change('beam_update',0,0,0,6,B+0x54),commands
      # Signed virtual self adjustment and explicit overrides.
      origin=B+16
      yield config,base+[(origin+0x48,4,SB.Model.VT_SHOCK),(SB.Model.VT_SHOCK+32,2,65520)],hooks,[('objvirtual',origin,4)]
      yield config,base,hooks,[('external',1,4),('namebody',0x2c42e4),('objvirtual',B,4)]
      yield config,base+[(T+20,4,0x2c3d68)],hooks,[('virtual',S,2,9,O,L,65535)]
      # Empty queries, rejected enemy votes and the strict radius boundary.
      for result in (0,'l:'):
        yield config,base,change('world_collectObjectsInBox',result),[('shockrun',B)]
      yield config,base,change('Creature.v20',0),[('shockrun',B)]
      for radius in (0x41200000,0x411fffff,0x41200001):
        data=base+[(victims[0]+0x10,4,0x41e00000),(victims[0]+0x1c,4,radius)]
        yield config,data,change('world_collectObjectsInBox','l:'+str(victims[0])),[('shockrun',B)]
      # Zero maximum with unequal count still links once, but does not recurse.
      yield config,base+[(B+0x18a,1,0),(B+0x18b,1,1)],hooks,[('fill',B+0x50,*victims),('shockchain',B,L)]
      for cb,value,addr in (('light_link',0,B+0xc),('sound_playOnObject',0xffffffff,SB.G_SIM_FRAME),('obj_runEveryFrame',0,B+0x194)):
        yield config,base,change(cb,value,addr),[('shockctor',B,L,65535,O,S,255)]
      for throws in (1,2):
        data=base+[(B+0x189,1,1)]+[(B+0x12c+4*i,4,0x99) for i in range(4)]
        yield config,data,change('beam_update',0,0,throws),[('fill',B+0x54,*victims),('shockrun',B)]
      # Complete terrain-stop lifetime, including its final particles and deletion.
      commands=[('shockrun',B)]
      for f in range(9,39): commands += [('poke',SB.G_SIM_FRAME,4,f),('shockrun',B)]
      yield config,base,change('terrain_height',0x42400000),commands

    # Complete Frost Storm / Ice Shard family; append only, independent seeds.
    for config in range(16):
      pool, shard, warning = 0x90000, 0x80000, 0x81000
      victims=(0x71000,0x72000,0x73000)
      base=[(S+0x60,4,T),(B+0x48,4,SB.Model.VT_FROSTSTORM),
            (SB.Model.VT_FROSTSTORM+36,4,0x1fe950),(SB.Model.VT_ICESHARD+36,4,0x1fdef8),
            (B+0x44,4,0x12345678),(B+0x50,4,0),(B+0x4c,4,60),
            (B+0x54,4,0x41c00000),(B+0x58,4,0x3f800000),(B+0x5c,4,101),
            (B+0x8c,4,0x429e0000),(B+0x6c,4,N),(N+4,4,B+0x6c),(N+0xc,4,O),(O+0xc,4,0x40),
            (SB.G_HANDLE_FREE,4,pool),(SB.G_RAND_SEED,4,0x76543210),(SB.G_SIM_FRAME,4,30),
            (SB.G_WORLD,4,0x123400),(shard+0x44,4,0x98765432),(shard+0x48,4,SB.Model.VT_ICESHARD),
            (shard+0x50,4,0x3f800000),(shard+0x60,4,120),(shard+0x1c,4,0x41400000)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for st in range(5):
        base += [(S+0xe+st*2,2,1),(S+0x18+st*2,2,(101,2,1,3,5)[st]),(S+0x24+st*4,4,0),
                 (S+0x38+st*2,2,1),(S+0x42+st*2,2,(9,2,1,1,7)[st]),(S+0x4c+st*4,4,0)]
      for i in range(16): base += [(pool+24*i,4,pool+24*(i+1) if i<15 else 0)]
      for a in (L,B+0x10,O+0x10,shard+0x10):
        for i,v in enumerate((0x41200000,0xc1200000,0x42300000)): base += [(a+4*i,4,v)]
      for a in (L+0x20,B+0x60,shard+0x54):
        for i,v in enumerate((0x3f800000,0xbf800000,0x3f000000)): base += [(a+4*i,4,v)]
      hooks=[('pool_new','q:'+':'.join(map(str,(B,warning,shard))),0,0),
             ('GameObject_ctor',0,0,0,5,0),('light_link',0,0,0),('light_relink',0,0,0),
             ('res_find',0x77,0,0),('sound_playOnObject',0,0,0),('obj_runEveryFrame',0,0,0),
             ('obj_updateGridCell',0,0,0),('terrain_height',0,0,0),('terrain_heightAtPoint',0,0,0),
             ('world_collectObjectsInBox','l:'+':'.join(map(str,victims)),0,0),('Creature.v20',1,0,0),
             ('damage_apply',0,0,0),('particle_spawn',0,0,0),('Creature.v1',0,0,0),('named',0,0,0),
             ('Skill.v12',2,0,0),('Skill.v14',1,0,0),('Skill.v2',0,0,0),('Creature.v4',0,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      for value in (0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc01234,0xffc01234):
        yield config,base,hooks,[('down',value)]
      for a in (0,1,0x7fffffff,0x80000000,0xffffffff):
        for b in (0,1,10,30,0xffffffff,0x80000000):
          yield config,base,hooks,[('crem',a,b)]
      for caster in (0,O):
        for flags in (0,0x20,0x40,0x60):
          for level in (0,1,20,255,0x80000000):
            for facing in (0,16384,32768,65535):
              yield config,base+[(O+0xc,4,flags)],hooks,[('frostcast',S,level,caster,L,facing)]
      for value in (0,1,16,0x7fffffff,0x80000000,0xffffffff):
        data=base+[(T+100,4,0),(T+116,4,0)]
        yield config,data,change('Skill.v12',value),[('frostcast',S,20,O,L,65535)]
      for caster in (0,O):
        for flags in (0,0x20,0x40):
          for radius in (0,0x80000000,0x00800000,0x41400000,0x43340000,0x43960000,0xc1c00000,0x7f800000):
            for frame in (0,100,0x7fffffff,0x80000000):
              data=base+[(O+0xc,4,flags),(SB.G_SIM_FRAME,4,frame)]
              yield config,data,change('pool_new',warning),[('frostctor',B,L,0xffffffff,0x80000000,caster,L+0x20,radius)]
      for cb in ('pool_new','GameObject_ctor','light_link','obj_runEveryFrame','res_find','sound_playOnObject'):
        for throws in (1,2):
          yield config,base+[(O+0xc,4,0x20)],change(cb,0,0,throws),[('frostcast',S,1,O,L,0)]
      yield config,base,change('pool_new',0),[('frostcast',S,1,O,L,0)]
      yield config,base+[(O+0xc,4,0x20)],change('pool_new','q:'+str(B)+':0'),[('frostcast',S,1,O,L,0)]
      for frame in (0,1,9,10,29,30,60,61,0x80000000,0xffffffff):
        for factor in (0x3f800000,0x3f733333,0,0x80000000):
          for owner in (0,N):
            yield config,base+[(SB.G_SIM_FRAME,4,frame),(B+0x58,4,factor),(B+0x6c,4,owner)],change('pool_new',shard),[('frostrun',B)]
      for radius in (0,1,0x00800000,0xc1c00000,0x413fffff,0x41400000,0x41400001,0x41c00000,0x42700000,0x43040000):
        yield config,base+[(B+0x54,4,radius)],change('pool_new',shard),[('frostrun',B)]
      for objects in ('l:','l:0:'+str(B)+':'+str(victims[0])+':'+str(victims[0]),0):
        for vote in (0,1):
          hh=change('world_collectObjectsInBox',objects)
          hh=[(n,vote,m,t,*a) if n=='Creature.v20' else (n,r,m,t,*a) for n,r,m,t,*a in hh]
          yield config,base+[(B+0x58,4,0x3f733333)],hh,[('frostrun',B)]
      for damage in (0,1,101,0xffffffff,0x7fffffff,0x80000000):
        yield config,base+[(B+0x5c,4,damage),(B+0x58,4,0x3f733333)],hooks,[('frostrun',B)]
      for cb in ('light_relink','particle_spawn','pool_new','GameObject_ctor','obj_updateGridCell','obj_runEveryFrame',
                 'terrain_heightAtPoint','world_collectObjectsInBox','Creature.v20','damage_apply','Creature.v1'):
        for throws in (1,2):
          data=base if cb!='Creature.v1' else base+[(B+0x58,4,0x3f733333),(B+0x54,4,0),(B+0x4c,4,0)]
          yield config,data,change(cb,shard if cb=='pool_new' else 0,0,throws),[('frostrun',B)]
      for cb,field,value in (('light_relink',B+0x58,0x3f733333),('particle_spawn',B+0x54,0x41400000),
                             ('Creature.v20',B+0x5c,1),('damage_apply',B+0x6c,0),('pool_new',SB.G_SIM_FRAME,60)):
        yield config,base,change(cb,value,field),[('frostrun',B)]
      yield config,base,change('Creature.v20',1,0,0,6,B+0x6c),[('frostrun',B)]
      for height in (0xc3960000,0xc2d00000,0x42c80000,0x7f800000):
        yield config,base,change('terrain_heightAtPoint',height),[('frostrun',B)]
      for seed in (0,1,0xffffffff,0x12345678):
        for spread in (0,0x3f800000,0x3f800032,0xbf800000):
          yield config,base+[(B+0x44,4,seed)],hooks,[('frostpoint',B,spread,i) for i in range(3)]
      for landed in (0,1,0xffffffff):
        for frame in (0,119,120,121,0xffffffff,0x80000000):
          for scale in (0,0x80000000,1,0x3f800000,0x40000000,0xbf800000):
            yield config,base+[(shard+0x4c,4,landed),(SB.G_SIM_FRAME,4,frame),(shard+0x50,4,scale)],hooks,[('shardrun',shard)]
      for ground in (0,0x42000000,0x42300000,0x7f800000):
        yield config,base,change('terrain_height',ground),[('shardrun',shard),('shardrun',shard)]
      for cb in ('particle_spawn','terrain_height','Creature.v1'):
        for throws in (1,2):
          yield config,base+[(shard+0x4c,4,int(cb=='Creature.v1'))],change(cb,0,0,throws),[('shardrun',shard)]
      for speed in (0,0x3f89fc08,0x412c7b0a,0xbf800000):
        yield config,base,hooks,[('shardparticle',shard,speed)]
      # Virtual adjustment, external forcing, named dispatch and scalar/list query ABI.
      yield config,base+[(T+20,4,0x2ed9a0)],hooks,[('virtual',S,2,1,O,L,0)]
      yield config,base+[(T+16,2,16),(T+20,4,0x2ed9a0),(S+0x70,4,T)],hooks,[('virtual',S,2,1,O,L,0)]
      for obj,body in ((B,0x1fe950),(shard,0x1fdef8)):
        yield config,base,hooks,[('objvirtual',obj,4)]
        table=SB.Model.VT_FROSTSTORM if obj==B else SB.Model.VT_ICESHARD
        yield config,base+[(table+32,2,16)],hooks,[('objvirtual',obj,4)]
        yield config,base,hooks,[('external',1,4),('objvirtual',obj,4)]
        yield config,base,hooks,[('external',1,4),('namebody',body),('objvirtual',obj,4)]
      # Full active -> shrinking -> delete lifetime and shard fall -> landing -> burst.
      commands=[('frostrun',B)]
      for f in range(31,49): commands += [('poke',SB.G_SIM_FRAME,4,f),('frostrun',B)]
      yield config,base+[(B+0x4c,4,30)],change('pool_new',shard),commands
      commands=[('shardrun',shard)]
      for f in range(31,38): commands += [('poke',SB.G_SIM_FRAME,4,f),('shardrun',shard)]
      yield config,base+[(shard+0x18,4,0x42000000),(shard+0x5c,4,0xc0000000)],change('terrain_height',0x41a00000),commands

    # Complete Ground Pound family, appended without changing preceding scenarios.
    for config in range(16):
      pool, light, clip, item, definition = 0x90000, 0x80000, 0x81000, 0x82000, 0x83000
      victims=(0x71000,0x72000,0x73000)
      rec=SB.G_PLAYERS
      base=[(S+0x60,4,T),(B+0x48,4,SB.Model.VT_GROUNDPOUND),
            (SB.Model.VT_GROUNDPOUND+36,4,0x2c7808),(B+0x44,4,0x12345678),
            (B+0x4c,1,0),(B+0x4d,1,20),(B+0x4f,1,0),(B+0x50,1,0),
            (B+0x54,4,0x42d80000),(B+0x58,4,S),(B+0x98,4,N),
            (N+4,4,B+0x98),(N+0xc,4,O),(O+0xc,4,0x40),(O+0x3c8,4,0),
            (rec+0x4a86,1,2),(rec+0x4a7c,1,0),(rec+0x4434,4,item),
            (item,4,definition),(definition+0x20,4,1),(O+0x24,4,H),(H,4,0x123400),
            (SB.G_HANDLE_FREE,4,pool),(SB.G_RAND_SEED,4,0x76543210),
            (SB.G_SEED3,4,0x98765432),(SB.G_WORLD,4,0x123400)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for st in range(5):
        base += [(S+0xe+st*2,2,1),(S+0x18+st*2,2,(101,2,100,3,5)[st]),(S+0x24+st*4,4,0),
                 (S+0x38+st*2,2,1),(S+0x42+st*2,2,(9,3,100,1,7)[st]),(S+0x4c+st*4,4,0)]
      for i in range(16): base += [(pool+24*i,4,pool+24*(i+1) if i<15 else 0)]
      for a in (L,B+0x10,O+0x10):
        for i,v in enumerate((0x41200000,0xc1200000,0x42300000)): base += [(a+4*i,4,v)]
      for i,a in enumerate(victims):
        base += [(a+0xc,4,0x20),(a+0x10,4,0x41200000),(a+0x14,4,0xc1200000),
                 (a+0x18,4,0x42300000),(a+0x1c,4,0x41400000),(a+0x228,4,0x42c80000)]
      hooks=[('pool_new',light,0,0),('GameObject_ctor',0,0,0,5,0),
             ('LightEffect_ctor',0,0,0,5,0),('light_link',0,0,0),('light_unlink',0,0,0),
             ('light_relink',0,0,0),('res_find',clip,0,0),('sound_playOnObject',0,0,0),
             ('obj_runEveryFrame',0,0,0),('terrain_height',0x42300000,0,0),('water_test',0,0,0),
             ('particle_spawn',0,0,0),('decal_add',0,0,0),('camera_shake',0,0,0),('water_disturb',0,0,0),
             ('world_collectObjectsInBox','l:'+':'.join(map(str,victims)),0,0),('Creature.v20',1,0,0),
             ('Creature.v30',0,0,0),('damage_apply',0,0,0),('obj_delete',0,0,0),
             ('text_lookup',0x77,0,0),('hud_showMessage',0,0,0),('anim_startBlendInOut',0,0,0),
             ('Skill.v10',101,0,0),('Skill.v12',2,0,0),('Skill.v1',1,0,0),('Skill.v2',0,0,0),
             ('Creature.v4',0,0,0),('named',0,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      for flags in (0,0x20,0x40,0x60):
        for weapon in (0,1,2,255):
          for active in (0,1,255):
            for held in (0,item):
              data=base+[(O+0xc,4,flags),(rec+0x4a86,1,weapon),(rec+0x4a7c,1,active),
                         ((rec+0x4434+4*(active if active<128 else active-256))&MASK,4,held)]
              yield config,data,hooks,[('poundcan',S,O)]
      yield config,base+[(definition+0x20,4,2)],hooks,[('poundcan',S,O)]
      yield config,base,hooks,[('poundcan',S,0)]
      for caster in (0,O):
        for flags in (0,0x20,0x40):
          yield config,base+[(O+0xc,4,flags)],hooks,[('poundcast',S,255,caster,L,65535)]
      for cb in ('res_find','anim_startBlendInOut','sound_playOnObject'):
        for throws in (1,2):
          yield config,base,change(cb,0,0,throws),[('poundcast',S,1,O,L,0)]
      for cb in ('text_lookup','hud_showMessage'):
        for throws in (1,2):
          yield config,base+[(rec+0x4a86,1,0)],change(cb,0,0,throws),[('poundcan',S,O)]
      # Ground-distance and water-byte thresholds, signed facing/level, absent owner.
      for owner in (0,O):
        for facing in (0,16384,32768,65535):
          for water in (0,1,128,256):
            for height in (0x42300000,0x433bffff,0x433c0000,0x433c0001):
              hh=change('terrain_height',height)
              hh=[(n,water,m,t,*a) if n=='water_test' else (n,r,m,t,*a) for n,r,m,t,*a in hh]
              yield config,base,hh,[('poundctor',B,L,facing,S,255,owner)]
      for cb in ('GameObject_ctor','light_link','res_find','sound_playOnObject','obj_runEveryFrame',
                 'terrain_height','water_test','light_unlink','light_relink'):
        for throws in (1,2):
          data=base if cb!='light_unlink' else base+[(B+0x18,4,0x43c80000)]
          yield config,data,change(cb,0,0,throws),[('poundctor',B,L,65535,S,20,O)]
      for cb,field,value in (('light_link',B+0x38,16384),('sound_playOnObject',B+0x38,32768),
                             ('obj_runEveryFrame',B+0x18,0x43c80000),('water_test',B+0x10,0)):
        yield config,base,change(cb,value,field),[('poundctor',B,L,0,S,20,O)]
      for owner in (0,N):
        for flags in (0,0x20,0x40):
          for index,cap in ((0,0),(1,10)):
            yield config,base+[(B+0x98,4,owner),(O+0xc,4,flags)],hooks,[('poundstat',B,index,cap)]
      for counter in (0,1,3,4,29,30,31,127,128,255):
        for wet in (0,1,128):
          for ledge in (0,1):
            yield config,base+[(B+0x4c,1,counter),(B+0x4f,1,wet),(B+0x50,1,ledge)],hooks,[('poundrun',B)]
      # Ring size thresholds and wrapped radius, HP boundary and creature filtering.
      for reach in (0,12,13,19,20,0xffff,0x7fff,0x8000):
        yield config,base+[(S+0x1a,2,reach)],hooks,[('poundrun',B)]
      for amount in (0,1,2,3,7,0xffff,0x7fff,0x8000):
        yield config,base+[(S+0x18,2,amount)],hooks,[('poundrun',B)]
      for chance in (0,1,50,99,100,101,0xffff,0x7fff,0x8000):
        yield config,base+[(S+0x1c,2,chance)],hooks,[('poundrun',B)]
      for hp in (0,0x80000000,0xbf800000,0x7fc00001):
        for flags in (0,0x20):
          yield config,base+[(victims[0]+0xc,4,flags),(victims[0]+0x228,4,hp)],hooks,[('poundrun',B)]
      for x in (0x4207ffff,0x42080000,0x42080001):
        yield config,base+[(victims[0]+0x10,4,x),(victims[0]+0x1c,4,0)],hooks,[('poundrun',B)]
      for found in (0,'l:','l:0:'+str(victims[0])+':'+str(victims[0])):
        for vote in (0,1):
          hh=change('world_collectObjectsInBox',found)
          hh=[(n,vote,m,t,*a) if n=='Creature.v20' else (n,r,m,t,*a) for n,r,m,t,*a in hh]
          yield config,base,hh,[('poundrun',B)]
      yield config,base+[(B+0x98,4,0)],hooks,[('poundrun',B)]
      yield config,base+[(N+0xc,4,0)],hooks,[('poundrun',B)]
      for value in (0,0x80000000,1,0x00800000,0x40795444,0x40795445,0x40795446,0xbf800000,0x7f800000):
        yield config,base+[(B+0x4c,1,4),(B+0x54,4,value)],hooks,[('poundrun',B)]
      for cb in ('particle_spawn','pool_new','LightEffect_ctor','decal_add','camera_shake',
                 'world_collectObjectsInBox','Creature.v20','damage_apply','Creature.v30','water_disturb','obj_delete'):
        for throws in (1,2):
          data=base+([(B+0x4f,1,1)] if cb=='water_disturb' else [(B+0x4c,1,30)] if cb=='obj_delete' else [])
          yield config,data,change(cb,0,0,throws),[('poundrun',B)]
      yield config,base,change('pool_new',0),[('poundrun',B)]
      for cb,field,value in (('particle_spawn',B+0x50,1),('LightEffect_ctor',B+0x4f,1),
                             ('decal_add',B+0x50,1),('camera_shake',B+0x4c,4),
                             ('Creature.v20',B+0x98,0),('damage_apply',B+0x98,0),
                             ('damage_apply',victims[0]+0x228,0xbf800000),('obj_delete',B+0x4f,1)):
        yield config,base+([(B+0x4c,1,30)] if cb=='obj_delete' else []),change(cb,value,field),[('poundrun',B)]
      yield config,base,change('Creature.v20',1,0,0,6,B+0x98),[('poundrun',B)]
      # Adjusted virtual dispatch and named/external boundaries.
      yield config,base+[(T+12,4,0x2c7140)],hooks,[('virtual',S,1,O)]
      yield config,base+[(T+20,4,0x2c71ec)],hooks,[('virtual',S,2,1,O,L,0)]
      yield config,base+[(T+16,2,65520),(T+20,4,0x2c71ec)],hooks,[('virtual',S,2,1,O,L,0)]
      yield config,base,hooks,[('objvirtual',B,4)]
      yield config,base+[(SB.Model.VT_GROUNDPOUND+32,2,16)],hooks,[('objvirtual',B,4)]
      yield config,base,hooks,[('external',1,4),('objvirtual',B,4)]
      yield config,base,hooks,[('external',1,4),('namebody',0x2c7808),('objvirtual',B,4)]
      for level in (0,1,127,128,255):
        yield config,base+[(B+0x4d,1,level)],hooks,[('poundrun',B)]
      for seed in (0,1,0xffffffff,0x80000000):
        yield config,base+[(B+0x44,4,seed),(SB.G_SEED3,4,seed),(S+0x1c,2,50)],hooks,[('poundrun',B)]
      for value in (0,0x7fffffff,0x80000000,0xffffffff):
        data=base+[(T+84,4,0),(T+100,4,0)]
        hh=change('Skill.v10',value)
        hh=[(n,value,m,t,*a) if n=='Skill.v12' else (n,r,m,t,*a) for n,r,m,t,*a in hh]
        yield config,data,hh,[('poundrun',B)]
      yield config,base,change('res_find',0),[('poundcast',S,1,O,L,0)]
      yield config,base,change('terrain_height',0x7fc00001),[('poundctor',B,L,0,S,1,0)]
      yield config,base,hooks,[('poundctor',B,B+0x10,65535,S,20,O)]
      # Dry and wet complete lifetimes, with damage exclusively on run zero.
      for wet in (0,1):
        yield config,base+[(B+0x4f,1,wet)],hooks,[('poundrun',B)]*31


    # Complete Harm Touch family (42), append-only synthetic fixtures.
    for config in range(16):
      target, light, table = 0x71000, 0x80000, SB.Model.VT_SPELL_HARMTOUCH
      base=[(S+0x60,4,T),(B,1,20),(B+8,4,S),(B+0xc,4,O),
            (B+0x10,4,0x12345678),(B+0x14,4,table),(B+0x18,4,0x3f29372a),
            (table+20,4,0x2d2c0c),(O+0xc,4,0x40),(O+0x3c8,4,0),
            (SB.G_PLAYERS+0x4a86,1,2),(target+0xc,4,0x20),(target+0x38,2,65535),
            (SB.G_RAND_SEED,4,0x76543210),(L,4,0xdeadbeef),(L+4,4,0xabcdef01)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      base += [(S+0xe,2,1),(S+0x18,2,101),(S+0x24,4,0x3f000000),
               (S+0x38,2,1),(S+0x42,2,9),(S+0x4c,4,0x3f800000)]
      for obj in (O,target):
        for i,v in enumerate((0x41200000,0xc1200000,0x42300000)): base += [(obj+0x10+4*i,4,v)]
      hooks=[('pool_new',light,0,0),('res_find',0x77,0,0),('sound_playOnObject',0,0,0),
             ('text_lookup',0x88,0,0),('hud_showMessage',0,0,0),('Spell.v1',0,0,0),
             ('Spell.v2',0,0,0),('named',0,0,0),('Creature.v16',0x90000,0,0),
             ('model_getSamplePointCount',7,0,0),
             ('model_getSamplePoint','p:1101004800:1106247680:1115684864:0',0,0),
             ('particle_spawn',0,0,0),('LightEffect_ctor',0,0,0,5,0),('Skill.v12',101,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      for caster in (0,O):
        for flags in (0,0x20,0x40,0x60):
          for weapon in (0,2,4,6,128,255):
            yield config,base+[(O+0xc,4,flags),(SB.G_PLAYERS+0x4a86,1,weapon)],hooks,[('harmcan',S,caster)]
      for owner in (0,O):
        for level in (0,1,127,128,255,0xffffffff):
          for seed in (0,0xffffffff):
            data=base+[(SB.G_RAND_SEED,4,seed)]
            yield config,data,change('pool_new',B),[('harmcast',S,level,owner,L,65535)]
            yield config,data,hooks,[('harmctor',B,level,owner,S)]
      yield config,base,change('pool_new',0),[('harmcast',S,20,O,L,0)]
      for value in (0,0x80000000,1,0x00800000,0x3c888888,0x3c888889,0x3c88888a,
                    0x3f29372a,0x3f800000,0x40000000,0xbf800000,0x7f800000,0xff800000,0x7fc01234):
        for owner in (0,O):
          data=base+[(B+0xc,4,owner),(B+0x18,4,value)]
          yield config,data,hooks,[('harmrun',B,L,65535)]
          yield config,data,hooks,[('harmtint',B,value)]
      for obj in (0,target):
        for flags in (0,0x20,0x40,0x60):
          yield config,base+[(target+0xc,4,flags)],hooks,[('harmhit',B,obj,L,L+4)]
      for count in (1,2,7,0xffffffff,0x80000000):
        for seed in (0,1,0xffffffff,0x80000000):
          for owner in (0,O):
            data=base+[(B+0x10,4,seed),(B+0xc,4,owner)]
            yield config,data,change('model_getSamplePointCount',count),[('harmhit',B,target,L,L+4)]
      for level in (0,1,127,128,255):
        for flags in (0,0x40):
          yield config,base+[(B,1,level),(O+0xc,4,flags)],hooks,[('harmhit',B,target,L,L+4)]
      for point in ('p:1092616192:3240099840:1110441984:0',
                    'p:0:0:0:0','p:2143289345:2139095040:0:0'):
        yield config,base,change('model_getSamplePoint',point),[('harmhit',B,target,L,L+4)]
      yield config,base,change('pool_new',0),[('harmhit',B,target,L,L+4)]
      # Output aliases can overwrite the private seed, owner, skill or timer.
      for low,high in ((L,L),(B+0x10,L),(L,B+0x10),(B+0xc,L),(B+8,L),(B+0x18,B+0x18)):
        yield config,base,hooks,[('harmhit',B,target,low,high)]
      for value in (0,1,0xffffffff,0x7fffffff,0x80000000):
        yield config,base+[(T+100,4,0)],change('Skill.v12',value),[('harmhit',B,target,L,L+4)]
      for cb in ('pool_new','res_find','sound_playOnObject'):
        for throws in (1,2):
          yield config,base,change(cb,0,0,throws),[('harmcast',S,20,O,L,0)]
      for cb in ('text_lookup','hud_showMessage'):
        for throws in (1,2):
          yield config,base+[(SB.G_PLAYERS+0x4a86,1,4)],change(cb,0,0,throws),[('harmcan',S,O)]
      for cb in ('Creature.v16','model_getSamplePointCount','model_getSamplePoint','particle_spawn',
                 'pool_new','LightEffect_ctor','Skill.v12','Spell.v1'):
        for throws in (1,2):
          data=base+([(T+100,4,0)] if cb=='Skill.v12' else [])
          yield config,data,change(cb,0,0,throws),[('harmhit',B,target,L,L+4)]
      # Return values are also stored into live raw32 fields by these callbacks.
      for cb,field in (('Creature.v16',target+0x38),('model_getSamplePointCount',B+0xc),
                       ('particle_spawn',B+0x10),('pool_new',target+0x10),('LightEffect_ctor',B+8)):
        ret=next(r for n,r,*_ in hooks if n==cb)
        yield config,base,change(cb,ret,0,0,4,field),[('harmhit',B,target,L,L+4)]
      yield config,base,change('res_find',0x77,B+0xc),[('harmctor',B,20,O,S)]
      yield config,base+[(SB.G_PLAYERS+0x4a86,1,4)],change('text_lookup',1,O+0x3c8),[('harmcan',S,O)]
      for body,slot,args in ((0x2d2984,1,(O,)),(0x2d2a04,2,(20,O,L,0))):
        for adjustment in (0,16,65520):
          yield config,base+[(T+8*slot,2,adjustment),(T+8*slot+4,4,body)],change('pool_new',B),[('virtual',S,slot,*args)]
      for adjustment in (0,16,65520):
        yield config,base+[(table+16,2,adjustment)],hooks,[('spellvirtual',B,2,L,65535)]
      yield config,base,hooks,[('external',0,2),('spellvirtual',B,2,L,0)]
      yield config,base,hooks,[('external',0,2),('namebody',0x2d2c0c),('spellvirtual',B,2,L,0)]
      # Complete armed countdown, and cast -> attached -> landed-hit lifetimes.
      yield config,base,change('pool_new',B),[('harmcast',S,20,O,L,0)]+[('harmrun',B,L,0)]*40
      pool=0xa0000
      data=base+[(SB.G_HANDLE_FREE,4,pool),(pool,4,pool+24),(pool+24,4,0)]
      yield config,data,change('pool_new','q:'+str(B)+':'+str(light)),[
        ('harmcast',S,20,O,L,0),('attach',O,B),('update',O,L),('harmhit',B,target,L,L+4)]


    # Complete Darkness family (36). Every fixture is authored, append-only.
    for config in range(16):
      target, second, pool, vt = 0x71000, 0x72000, 0xa0000, SB.Model.VT_DARKNESS
      base=[(S+0x60,4,T),(B+0x48,4,vt),(vt+36,4,0x2cf690),
            (B+0xac,4,S),(B+0x98,4,20),(B+0xa8,4,N),(N+4,4,B+0xa8),(N+0xc,4,O),
            (B+0x94,4,108),(B+0x9c,4,1),(B+0xa0,1,0),(B+0x44,4,0x12345678),
            (SB.G_RAND_SEED,4,0x76543210),(SB.G_WORLD,4,0x90000),
            (SB.G_SIM_FRAME,4,30),(SB.G_HANDLE_FREE,4,pool),(pool,4,pool+24),
            (pool+24,4,0),(O,4,0)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for i,value in enumerate((101,35,1,3,10)):
        base += [(S+0xe+2*i,2,1),(S+0x18+2*i,2,value),(S+0x24+4*i,4,0)]
      for obj in (O,B,target,second,L):
        for i,v in enumerate((0x41200000,0xc1200000,0x42300000)):
          base += [(obj+(0 if obj==L else 0x10)+4*i,4,v)]
      base += [(L+12,4,0x43ba8000),(L+16,4,0),(L+20,4,0),
               (B+0x4c,4,0x40c6eeed),(B+0x50,4,0),(B+0x54,4,0),
               (target+0x228,4,0x3f800000),(second+0x228,4,0x3f800000)]
      for i,v in enumerate((0,1,128,255)): base += [(SB.Model.G_AMBIENT_BYTES+i,1,v)]
      hooks=[('pool_new',B,0,0),('GameObject_ctor',0,0,0,5,0),
             ('light_link',0,0,0),('light_relink',0,0,0),('light_unlink',0,0,0),
             ('obj_runEveryFrame',0,0,0),('res_find',0x77,0,0),('sound_playOnObject',0,0,0),
             ('terrain_height',0,0,0),('world_collectObjectsInBox','l:'+str(target)+':'+str(second),0,0),
             ('Creature.v20',1,0,0),('Creature.v31',0,0,0),('Creature.v4',0,0,0),('named',0,0,0),
             ('creature_bossRank',0,0,0),('damage_apply',0,0,0),('particle_spawn',0,0,0),
             ('obj_delete',0,0,0),('Skill.v10',101,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      for values in ((0,0,0,0),(255,254,128,1),(1,2,3,4)):
        data=base+[(SB.Model.G_AMBIENT_BYTES+i,1,v) for i,v in enumerate(values)]
        yield config,data,hooks,[('ambient',i) for i in range(4)]
        yield config,data,hooks,[('darkctor',B,L,L+12,S,20,O)]
      for caster in (0,O):
        for facing in (0,1,16384,32767,32768,65535,0xffffffff):
          for level in (0,1,20,0xffffffff):
            yield config,base,hooks,[('darkcast',S,level,caster,L,facing)]
      yield config,base,change('pool_new',0),[('darkcast',S,20,O,L,0)]
      for level in (0,1,127,128,255,0xffffffff):
        for caster in (0,O):
          yield config,base,hooks,[('darkctor',B,L,L+12,S,level,caster)]
      yield config,base,hooks,[('darkctor',B,B+0x10,B+0x4c,S,20,O)]
      # Signed attached byte: only 1 takes attached branch, only 0 attaches.
      for attached in (0,1,2,127,128,255):
        for moving in (0,1,0xffffffff):
          for owner in (0,N):
            yield config,base+[(B+0xa0,1,attached),(B+0x9c,4,moving),(B+0xa8,4,owner)],hooks,[('darkrun',B)]
      for frame in (0,1,29,30,31,60,0xffffffff,0xffffffe2,0x80000000):
        for seed in (0,1,0xffffffff,0x80000000):
          yield config,base+[(B+0xa0,1,1),(SB.G_SIM_FRAME,4,frame),(B+0x44,4,seed)],hooks,[('darkrun',B)]
      for life in (0,1,108,0xffffffff,0x80000000,0x7fffffff):
        yield config,base+[(B+0xa8,4,0),(B+0x94,4,life)],hooks,[('darkrun',B)]
      for ground in (0x42300000,0x42300001,0x422fffff,0x7f800000,0xff800000,0x7fc01234):
        yield config,base+[(B+0xa8,4,0)],change('terrain_height',ground),[('darkrun',B)]
      for objects in ('l:',0,'l:0','l:'+str(target),'l:'+str(target)+':'+str(target)):
        for attached in (0,1):
          data=base+[(B+0xa0,1,attached)]
          yield config,data,change('world_collectObjectsInBox',objects),[('darkrun',B)]
          yield config,data,change('world_collectObjectsInBox',objects),[('darkbox',B)]
      for votes in (0,1,'q:1:0:1:1','q:0:1:1','q:1:1:0:1'):
        for attached in (0,1):
          yield config,base+[(B+0xa0,1,attached)],change('Creature.v20',votes),[('darkrun',B)]
      for hp in (0,0x80000000,0xbf800000,0x3f800000,0x7f800000,0x7fc01234):
        for timer in (0,1,32767,32768,65535):
          yield config,base+[(B+0xa0,1,1),(target+0x228,4,hp),(target+0x316,2,timer)],hooks,[('darkrun',B)]
      # Signed stats and wrapped frame counts; boss adjustment uses arithmetic shift.
      for index in (0,1,2,3):
        for value in (0,1,2,50,100,101,32767,0x7fffffff,0x80000000,0xffffffff):
          for attached in (0,1):
            data=base+[(B+0xa0,1,attached),(T+84,4,0)]
            hh=change('Skill.v10',value)
            yield config,data,hh,[('darkrun',B)]
      for boss in (0,1,2,0xffffffff):
        yield config,base+[(B+0xa0,1,1)],change('creature_bossRank',boss),[('darkrun',B)]
      # Exact failure prefixes at every service reached in construction/run.
      for cb in ('pool_new','GameObject_ctor','light_link','res_find','sound_playOnObject','obj_runEveryFrame'):
        for throws in (1,2):
          yield config,base,change(cb,0,0,throws),[('darkcast',S,20,O,L,0)]
      for cb in ('terrain_height','world_collectObjectsInBox','Creature.v20','damage_apply',
                 'light_relink','particle_spawn','obj_delete','light_unlink'):
        for throws in (1,2):
          data=base+([(B+0xa8,4,0),(B+0x94,4,0)] if cb in ('obj_delete','light_unlink') else [])
          yield config,data,change(cb,0,0,throws),[('darkrun',B)]
      for cb in ('Skill.v10','creature_bossRank','Creature.v31'):
        for throws in (1,2):
          yield config,base+[(B+0xa0,1,1),(T+84,4,0)],change(cb,0,0,throws),[('darkrun',B)]
      # Callback writes can change subsequent owner/stat/particle reads.
      for cb,field,value in (('Creature.v20',N+0xc,1),('Creature.v20',B+0xa8,0),
                             ('Creature.v20',B+0xa0,2),('damage_apply',B+0x98,0),
                             ('damage_apply',B+0x94,0),('particle_spawn',B+0x44,0),
                             ('light_relink',B+0x10,0x7fc01234)):
        hh=change(cb,value,0,0,4,field)
        yield config,base,hh,[('darkrun',B)]
      yield config,base,change('Creature.v20',1,0,0,6,B+0xa8),[('darkrun',B)]
      for adjustment in (0,16,65520):
        yield config,base+[(T+16,2,adjustment),(T+20,4,0x2cf1e0)],hooks,[('virtual',S,2,20,O,L,65535)]
        adjusted=B+(adjustment if adjustment<32768 else adjustment-65536)
        data=base+[(vt+32,2,adjustment),(adjusted+0xa8,4,0),(adjusted+0xa0,1,1),
                   (adjusted+0x98,4,20),(adjusted+0x94,4,108)]
        yield config,data,hooks,[('objvirtual',B,4)]
      yield config,base,hooks,[('external',1,4),('objvirtual',B,4)]
      yield config,base,hooks,[('external',1,4),('namebody',0x2cf690),('objvirtual',B,4)]
      # Full 109-frame empty flight and full 61-frame attached lifecycle.
      yield config,base+[(B+0xa8,4,0)],hooks,[('darkrun',B)]*109
      yield config,base,hooks,[('darkrun',B)]+[cmd for i in range(1,61) for cmd in [('poke',SB.G_SIM_FRAME,4,i),('darkrun',B)]]


    # Complete Shock of Fire / Sparks family (57), append-only authored inputs.
    for config in range(16):
      target, second, pool, vt = 0x71000, 0x72000, 0xa0000, SB.Model.VT_SPARKS
      base=[(S+0x60,4,T),(B+0x48,4,vt),(vt+36,4,0x28fa3c),(vt+52,4,0x2900c8),
            (B+0x4c,4,N),(N+4,4,B+0x4c),(N+0xc,4,O),(B+0x54,4,700),
            (B+0x44,4,0x12345678),(B+0xa4,4,0x9876),(B+0xa0,4,0xfeedbeef),
            (B+0xa8,4,120),(B+0xac,4,80),(B+0xb0,4,35),(B+0xb4,4,24),
            (B+0x1c,4,0x41400000),(B+0x20,4,0x42900000),
            (O+0xc,4,0x40),(SB.G_SIM_FRAME,4,100),(SB.G_WORLD,4,0x90000),
            (SB.G_RAND_SEED,4,0x76543210),(SB.G_HANDLE_FREE,4,pool),(pool,4,pool+24),
            (pool+24,4,0),(O,4,0)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for i,value in enumerate((101,35,2,243,10)):
        base += [(S+0xe+2*i,2,1),(S+0x18+2*i,2,value),(S+0x24+4*i,4,0x3f000000),
                 (S+0x38+2*i,2,1),(S+0x42+2*i,2,value+7),(S+0x4c+4*i,4,0xbf000000)]
      for obj in (B,target,second,L):
        for i,v in enumerate((0x41200000,0xc1200000,0x42300000)):
          base += [(obj+(0 if obj==L else 0x10)+4*i,4,v)]
      base += [(L+12,4,0x43730000),(L+16,4,0),(L+20,4,0),
               (B+0x58,4,0x43730000),(B+0x5c,4,0),(B+0x60,4,0)]
      for obj in (target,second): base += [(obj+0x1c,4,0x41400000),(obj+0x20,4,0x42900000)]
      hooks=[('pool_new',B,0,0),('GameObject_ctor',0,0,0,5,0),
             ('LightEffect_ctor',0,0,0,5,0),('light_link',0,0,0),('light_relink',0,0,0),
             ('obj_runEveryFrame',0,0,0),('res_find',0x77,0,0),('sound_playOnObject',0,0,0),
             ('obj_updateGridCell',0,0,0),('terrain_height',0,0,0),('water_test',0,0,0),
             ('world_collectObjectsInBox','l:',0,0),('voice_setPosition',0,0,0),
             ('Creature.v20',1,0,0),('Creature.v6',0,0,0),('Creature.v1',0,0,0),
             ('Creature.v4',0,0,0),('named',0,0,0),('particle_spawn',0,0,0),
             ('damage_apply',0,0,0),('fx_ringBurst',0,0,0),('world_damageInRadius',0,0,0),
             ('Skill.v14',20,0,0),('Skill.v12',24,0,0),
             ('libc_sprintf','s:synthetic-water',0,0),('water_disturb',0,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      cast=[('sparkcast',S,20,O,L,65535)]
      ctor=[('sparkctor',B,L,L+12,O,0xdead,0xabcdef01,80,120,35,24)]
      # Tier-1 direct damage for everyone; other stats use player/tier-2 path.
      for owner in (0,O):
        for flags in (0,0x20,0x40,0x60):
          for level in (0,1,4,5,10,20,127,128,255,0xffffffff):
            yield config,base+[(O+0xc,4,flags)],hooks,[('sparkcast',S,level,owner,L,65535)]
      for facing in (0,1,16384,32767,32768,65535,0xffffffff):
        yield config,base,hooks,[('sparkcast',S,20,O,L,facing)]
      yield config,base,change('pool_new',0),cast
      for owner in (0,O):
        for frame in (0,1,0x7ffffda7,0x7ffffda8,0x7fffffff,0x80000000,0xffffffff):
          yield config,base+[(SB.G_SIM_FRAME,4,frame)],hooks,[('sparkctor',B,L,L+12,owner,0,0,80,120,35,24)]
      yield config,base,hooks,[('sparkctor',B,B+0x10,B+0x58,O,1,0xffffffff,0x80000000,0xffffffff,0x7fffffff,0x80000000)]
      # Signed timing includes equality, negative frames and a zeroed end at negative frame.
      for frame,end in ((0,0),(0,1),(1,0),(699,700),(700,700),(701,700),
                        (0x7fffffff,0x80000000),(0x80000000,0x7fffffff),
                        (0xffffffff,0),(0xffffffff,0xffffffff),(0,0xffffffff)):
        for splash in (0,1,0xffffffff,0x80000000):
          yield config,base+[(SB.G_SIM_FRAME,4,frame),(B+0x54,4,end),(B+0xb0,4,splash)],hooks,[('sparkrun',B)]
      for seed in (0,1,2,0x12345678,0xffffffff,0x80000000):
        for end in (0,700):
          yield config,base+[(B+0x44,4,seed),(B+0x54,4,end)],hooks,[('sparkrun',B)]
      for motion in (0,0x80000000,1,0x00800000,0xbf800000,0x7f800000,0xff800000,0x7fc01234):
        yield config,base+[(B+0x58,4,motion),(B+0x5c,4,motion)],hooks,[('sparkrun',B)]
      for radius in (0,1,0x7fffffff,0x80000000,0xffffffff):
        for head,owner in ((0,O),(N,0),(N,O)):
          yield config,base+[(B+0x54,4,0),(B+0xb4,4,radius),(B+0x4c,4,head),(N+0xc,4,owner)],hooks,[('sparkrun',B)]
      # Terrain-only, object-only, and combined results; duplicate queries hit once.
      for found in ('l:','l:'+str(target),'l:'+str(target)+':'+str(second)+':'+str(target),
                    'l:0:'+str(O)+':'+str(B)):
        for ground in (0,0x422fffff,0x42300000,0x42300001,0x7fc01234):
          for vote in (0,1,'q:0:1:1'):
            hh=change('world_collectObjectsInBox',found)
            hh=[(n,ground if n=='terrain_height' else vote if n=='Creature.v20' else r,m,t,*a) for n,r,m,t,*a in hh]
            yield config,base,hh,[('sparkrun',B),('poke',SB.G_SIM_FRAME,4,101),('sparkrun',B)]
      # Complete water-service branch, with one extra private draw.
      for wet in (0,1,128,255):
        hh=change('terrain_height',0x42300001)
        hh=[(n,1 if n=='water_test' else r,m,t,*a) for n,r,m,t,*a in hh]
        yield config,base+[(B+0x50,1,wet)],hh,[('sparkrun',B)]
      for head,owner in ((0,O),(N,0),(N,O)):
        for end in (0,700):
          for vote in (0,1):
            yield config,base+[(B+0x4c,4,head),(N+0xc,4,owner),(B+0x54,4,end)],change('Creature.v20',vote),[('sparktouch',B,target),('sparktouch',B,second)]
      for low,high in ((0,0),(1,2),(0xffffffff,0x80000000),(0x8000,0x7fff),(0x12345678,0xabcdef01)):
        yield config,base+[(B+0xac,4,low),(B+0xa8,4,high)],change('pool_new',0x80000),[('sparktouch',B,target)]
      yield config,base,change('pool_new',0),[('sparktouch',B,0)]
      # Failures preserve all reached writes, draws and callbacks.
      for cb in ('Skill.v14','Skill.v12','pool_new','GameObject_ctor','light_link','obj_runEveryFrame','res_find','sound_playOnObject'):
        for throws in (1,2):
          data=base+([(T+116,4,0)] if cb=='Skill.v14' else [(T+100,4,0)] if cb=='Skill.v12' else [])
          yield config,data,change(cb,0,0,throws),cast
      for cb in ('obj_updateGridCell','terrain_height','world_collectObjectsInBox','voice_setPosition','particle_spawn','light_relink'):
        for throws in (1,2): yield config,base,change(cb,0,0,throws),[('sparkrun',B)]
      for cb in ('water_test','libc_sprintf','water_disturb'):
        for throws in (1,2):
          hh=change(cb,0,0,throws)
          hh=[(n,0x42300001 if n=='terrain_height' else 1 if n=='water_test' and cb!='water_test' else r,m,t,*a) for n,r,m,t,*a in hh]
          yield config,base,hh,[('sparkrun',B)]
      for cb in ('particle_spawn','fx_ringBurst','world_damageInRadius','Creature.v1'):
        for throws in (1,2):
          yield config,base+[(B+0x54,4,0)],change(cb,0,0,throws),[('sparkrun',B)]
      for cb in ('Creature.v20','pool_new','LightEffect_ctor','res_find','sound_playOnObject','damage_apply'):
        for throws in (1,2):
          hh=change(cb,0,0,throws)
          hh=[(n,0x80000 if n=='pool_new' and cb!='pool_new' else r,m,t,*a) for n,r,m,t,*a in hh]
          yield config,base,hh,[('sparktouch',B,target)]
      # Live callback mutations: capture owner/start early, reread damage/position/frame later.
      for cb,field,value,commands in (
          ('Skill.v14',L,20,cast),('pool_new',L,B,cast),
          ('GameObject_ctor',L,0,ctor),('light_link',SB.G_SIM_FRAME,0xffffffff,ctor),
          ('obj_runEveryFrame',B+0xa4,0x77,ctor),('sound_playOnObject',B+0xa0,0,ctor),
          ('voice_setPosition',B+0x58,0,[('sparkrun',B)]),
          ('particle_spawn',B+0x44,1,[('sparkrun',B)]),
          ('particle_spawn',B+0x10,0x7fc01234,[('sparkrun',B)]),
          ('light_relink',B+0x54,0,[('sparkrun',B)]),
          ('Creature.v20',N+0xc,0,[('sparktouch',B,target)]),
          ('LightEffect_ctor',B+0xac,0xffffffff,[('sparktouch',B,target)]),
          ('damage_apply',B+0x54,700,[('sparktouch',B,target)]),
          ('fx_ringBurst',B+0xb0,0,[('poke',B+0x54,4,0),('sparkrun',B)]),
          ('fx_ringBurst',B+0xb4,0x80000000,[('poke',B+0x54,4,0),('sparkrun',B)])):
        ret=next(r for n,r,*_ in hooks if n==cb)
        # Keep acceptance true while clearing the owner through the callback action.
        hh=change(cb,ret,0,0,4,field) if cb=='Creature.v20' else change(cb,value,0,0,4,field)
        if cb=='pool_new': hh=change(cb,B,0,0,4,field)
        if cb=='LightEffect_ctor': hh=[(n,0x80000 if n=='pool_new' else r,m,t,*a) for n,r,m,t,*a in hh]
        yield config,base,hh,commands
      yield config,base,change('Creature.v20',1,0,0,6,B+0x4c),[('sparktouch',B,target)]
      # Internal adjusted dispatch, plus named and forced external service dispatch.
      for adjustment in (0,16,65520):
        adjusted_skill=S+(adjustment if adjustment<32768 else adjustment-65536)
        yield config,base+[(adjusted_skill+0x60,4,T),(T+16,2,adjustment),(T+20,4,0x2ed4e4)],hooks,[('virtual',S,2,20,O,L,65535)]
        adjusted=B+(adjustment if adjustment<32768 else adjustment-65536)
        data=base+[(vt+32,2,adjustment),(adjusted+0x54,4,0),(adjusted+0xb0,4,0)]
        yield config,data,hooks,[('objvirtual',B,4)]
        data=base+[(vt+48,2,adjustment),(adjusted+0x54,4,700),(adjusted+0x4c,4,N)]
        yield config,data,hooks,[('objvirtual',B,6,target)]
      for slot,body,args in ((4,0x28fa3c,()),(6,0x2900c8,(target,))):
        yield config,base,hooks,[('external',1,slot),('objvirtual',B,slot,*args)]
        yield config,base,hooks,[('external',1,slot),('namebody',body),('objvirtual',B,slot,*args)]
      # Cast, hit during movement, complete delayed burst on the next frame.
      hh=change('world_collectObjectsInBox','l:'+str(target)+':'+str(second))
      hh=[(n,'q:'+str(B)+':'+str(0x80000) if n=='pool_new' else r,m,t,*a) for n,r,m,t,*a in hh]
      yield config,base,hh,cast+[('sparkrun',B),('poke',SB.G_SIM_FRAME,4,101),('sparkrun',B)]
      # Last ten flying ticks followed by expiry; full state/history stays bounded.
      yield config,base,hooks,ctor+[cmd for frame in range(690,701) for cmd in [('poke',SB.G_SIM_FRAME,4,frame),('sparkrun',B)]]


    # Complete Planar Wind family (66); preserve every preceding scenario.
    for config in range(16):
      target, second = 0x71000, 0x72000
      base=[(S+0x60,4,T),(B,1,6),(B+8,4,S),(B+0xc,4,O),
            (B+0x10,4,0x12345678),(B+0x14,4,SB.Model.VT_SPELL_PLANARWIND),
            (SB.Model.VT_SPELL_PLANARWIND+20,4,0x30a0b0),
            (B+0x18,4,0),(B+0x1c,4,0x80000000),(B+0x20,4,0x41400000),
            (B+0x24,4,0x3f800000),(B+0x30,4,0x40800000),(B+0x34,4,0x44700000),
            (B+0x5c,4,3),(B+0x60,4,50),(B+0x64,4,0x100000),
            (SB.G_RAND_SEED,4,0x76543210),(SB.G_WORLD,4,0x99000),(O+0x38,2,0),
            (L,4,0x3f800000),(L+4,4,0x80000000),(L+8,4,0x41a00000),
            (SB.G_HANDLE_FREE,4,0xa0000)]
      for i in range(32): base += [(0xa0000+0x20*i,4,0xa0000+0x20*(i+1) if i<31 else 0)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for index,value in enumerate((101,4,960,50,20)):
        base += [(S+0xe+2*index,2,1),(S+0x18+2*index,2,value),(S+0x24+4*index,4,0)]
      for i,value in enumerate((0x3f800000,0,0,0,0x3f800000,0,0,0,0x3f800000)):
        base += [(B+0x38+4*i,4,value)]
      for obj in (O,target,second):
        base += [(obj+0x10,4,0x3f800000),(obj+0x14,4,0),(obj+0x18,4,0x41a00000)]
      hooks=[('pool_new',B,0,0),('light_link',0,0,0),('light_relink',0,0,0),
             ('res_find',0x77,0,0),('sound_playAtPos',0,0,0),
             ('world_collectObjectsInArc','l:'+str(target)+':'+str(second),0,0),
             ('damage_apply',0,0,0),('Creature_setEffectTimer',0,0,0),('Creature.v16',0,0,0),
             ('model_getSamplePointCount',7,0,0),('model_getSamplePoint','p:1065353216:0:1101004800:0',0,0),
             ('particle_spawn',0,0,0),('terrain_height',0,0,0),('Spell.v1',0,0,0),
             ('Spell.v2',0,0,0),('named',0,0,0),('Skill.v12',101,0,0),('Skill.v14',6,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      cast=[('windcast',S,6,O,L,65535)]
      ctor=[('windctor',B,6,O,S,L)]
      strike=[('windstrike',B,target,0x3f800000,0x80000000,0)]
      for owner in (0,O):
        for rank in (0,1,2,4,5,6,9,10,11,127,128,255,0xffffffff):
          for facing in (0,16384,32768,65535):
            data=base+[(O+0x38,2,facing)]
            yield config,data,hooks,[('windcast',S,rank,owner,L,1234)]
            yield config,data,hooks,[('windctor',B,rank,owner,S,L)]
      yield config,base,change('pool_new',0),cast
      yield config,base,hooks,[('windctor',B,6,O,S,B+0x18)]
      for width in (0,1,4,40,72,107,201,32767,0xffffffff):
        yield config,base+[(S+0x1a,2,width)],hooks,ctor
      for objects in ('l:','l:0','l:'+str(target),'l:'+str(target)+':'+str(target)+':0:'+str(second)):
        yield config,base,change('world_collectObjectsInArc',objects),ctor+[('windrun',B)]
      for owner in (0,O):
        for flags in (0,0x40):
          for pvp in (0,1):
            for chance in (0,1,21,50,100,101,0x80000000,0xffffffff):
              data=base+[(B+0xc,4,owner),(O+0xc,4,flags),(target+0xc,4,flags),
                         (SB.G_PVP_MODE_A,4,pvp),(B+0x60,4,chance)]
              yield config,data,hooks,strike
      for model in (0,0x90000):
        for count in (1,7,0xffffffff,0x80000000):
          hh=change('Creature.v16',model)
          hh=[(n,count if n=='model_getSamplePointCount' else r,m,t,*a) for n,r,m,t,*a in hh]
          yield config,base,hh,strike
      for rank in (0,1,2,4,5,9,10,127,128,255):
        for seed in (0,1,0xffffffff):
          yield config,base+[(B,1,rank),(B+0x10,4,seed)],hooks,[('windrun',B)]
      for runs in (0,52,53,54,0x7fffffff,0x80000000,0xffffffff):
        for ground in (0,0x41400000,0x41400001,0x7fc01234):
          yield config,base+[(B+0x68,4,runs)],change('terrain_height',ground),[('windrun',B)]
      for amount in (0,1,3,0xffffffff,0x80000000):
        yield config,base+[(B+0x5c,4,amount)],hooks,[('windrun',B)]
      # The exact plane boundary, duplicates, removed targets and next-before-body.
      for x in (0,0x417fffff,0x41800000,0x41800001,0x7fc01234):
        yield config,base+[(target+0x10,4,x)],hooks,[('link',B+0x6c,target,target),('windrun',B)]
      yield config,base,hooks,ctor+[('clear',target),('windrun',B)]
      for cb in ('pool_new','light_link','res_find','sound_playAtPos','world_collectObjectsInArc'):
        for throws in (1,2): yield config,base,change(cb,0,0,throws),cast
      for cb in ('damage_apply','Creature_setEffectTimer','res_find','sound_playAtPos','Creature.v16',
                 'model_getSamplePointCount','model_getSamplePoint','particle_spawn'):
        for throws in (1,2):
          hh=change(cb,0,0,throws)
          if cb in ('model_getSamplePointCount','model_getSamplePoint','particle_spawn'):
            hh=[(n,0x90000 if n=='Creature.v16' else r,m,t,*a) for n,r,m,t,*a in hh]
          yield config,base+[(B+0x60,4,101)],hh,strike
      for cb in ('light_relink','particle_spawn','terrain_height','Spell.v1'):
        for throws in (1,2): yield config,base+[(B+0x68,4,53)],change(cb,0,0,throws),[('windrun',B)]
      for cb,field,value,commands in (
          ('Skill.v14',L,6,cast),('pool_new',L,B,cast),('light_link',B+0xc,0,ctor),
          ('sound_playAtPos',O+0x38,16384,ctor),('world_collectObjectsInArc',B+0x64,0,ctor),
          ('damage_apply',B+0xc,0,strike),('damage_apply',B+0x60,0,strike),
          ('Creature_setEffectTimer',target+0x10,0,strike),('sound_playAtPos',target+0x38,32768,strike),
          ('light_relink',B+0x34,0,[('windrun',B)]),('particle_spawn',B+0x5c,0,[('windrun',B)]),
          ('terrain_height',B+0x68,53,[('windrun',B)])):
        data=base+[(T+116,4,0)] if cb=='Skill.v14' else base
        yield config,data,change(cb,value,0,0,4,field),commands
      for amount in (0,1,0x7fffffff,0x80000000,0xffffffff):
        data=base+[(T+100,4,0)]
        yield config,data,change('Skill.v12',amount),strike
      yield config,base+[(T+20,4,0x309acc)],hooks,[('virtual',S,2,6,O,L,0)]
      yield config,base+[(T+16,2,16),(T+20,4,0x309acc),(S+16+0x60,4,T)],hooks,[('virtual',S,2,6,O,L,0)]
      yield config,base,hooks,[('spellvirtual',B,2,L,0)]
      yield config,base+[(SB.Model.VT_SPELL_PLANARWIND+16,2,16)],hooks,[('spellvirtual',B,2,L,0)]
      yield config,base,hooks,[('external',0,2),('spellvirtual',B,2,L,0)]
      yield config,base,hooks,[('external',0,2),('namebody',0x30a0b0),('spellvirtual',B,2,L,0)]
      # Complete 54-run lifecycle, and immediate terrain expiry after strikes.
      yield config,base,hooks,ctor+[('windrun',B)]*54
      yield config,base,change('terrain_height',0x43000000),ctor+[('windrun',B)]


    # Complete Shadow Step family (67), appended after the immutable prefix.
    for config in range(16):
      target,second,handle=0x71000,0x72000,0x73000
      vt=SB.Model.VT_SPELL_SHADOWSTEP
      base=[(S+0x60,4,T),(B,1,6),(B+8,4,S),(B+0xc,4,O),(B+0x10,4,0x12345678),
            (B+0x14,4,vt),(vt+20,4,0x311e3c),(B+0x1c,4,0x43b90000),
            (B+0x20,4,0x43b90000),(B+0x24,4,0),(B+0x28,4,0),(B+0x2c,4,O),(B+0x30,2,32768),
            (SB.G_RAND_SEED,4,0x76543210),(SB.G_WORLD,4,0x99000),(SB.G_NETWORK_MODE,1,1),
            (O+0x38,2,1234),(O+0xa8,4,0x12345678),(O+0xac,4,0x87654321),(target+0xc,4,0x20),
            (target+0x234,4,handle),(handle+0xc,4,O),(second+0xc,4,0x20),(second+0x234,4,handle)]
      for slot,body in BODIES.items(): base += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
      for index,value in enumerate((101,0,3,50,20)):
        base += [(S+0xe+2*index,2,1),(S+0x18+2*index,2,value),(S+0x24+4*index,4,0)]
      for obj in (O,target,second):
        base += [(obj+0x10,4,0x3f800000),(obj+0x14,4,0x41200000),(obj+0x18,4,0x41a00000)]
      hooks=[('pool_new','q:'+str(B)+':'+str(0x80000)+':'+str(0x81000),0,0),
             ('res_find',0x77,0,0),('sound_playAtPos',0,0,0),('terrain_heightAtPoint',0x41a00000,0,0),
             ('StaticFire_ctor',0,0,0),('world_collectObjectsInBox','l:'+str(target)+':'+str(second),0,0),
             ('Creature.v20',1,0,0),('Creature.v31',0,0,0),('Creature.v16',0x90000,0,0),
             ('model_getSamplePointCount',7,0,0),('model_getSamplePoint','p:1065353216:0:1101004800:0',0,0),
             ('particle_spawn',0,0,0),('LightEffect_ctor',0,0,0),
             ('Player_clampToScreen','l:1120403456:2147483648:0',0,0),('obj_moveWithCollision',0,0,0),
             ('Spell.v1',0,0,0),('Spell.v2',0,0,0),('named',0,0,0),('Skill.v14',6,0,0)]
      def change(name,value=0,mutation=0,throws=0,*action):
        return [(n,value,mutation,throws,*action) if n==name else (n,r,m,t,*a) for n,r,m,t,*a in hooks]
      ctor=[('shadowctor',B,6,O,S)]
      cast=[('shadowcast',S,6,O,0xffffffff,65535)]
      run=[('shadowrun',B,0,0)]
      for owner in (0,O):
        for rank in (0,1,5,6,20,127,128,255,0xffffffff):
          for facing in (0,16384,32768,65535):
            yield config,base+[(owner+0x38,2,facing)],hooks,[('shadowctor',B,rank,owner,S)]
            yield config,base+[(owner+0x38,2,facing)],hooks,[('shadowcast',S,rank,owner,0xffffffff,0xffffffff)]
      for fires in (0,1,2,3,6,0xffff,0x8000):
        # Tier-one stat fields are signed halfwords; negative ring still enables knock-back.
        data=base+[(S+0x1a,2,fires)]
        if fires==0xffff: data += [(S+0x1a,2,0xffff)]
        if fires==0x8000: data += [(S+0x1a,2,0x8000)]
        # Avoid a 65,534-object fixture while still covering signed table extremes through failure prefixes.
        hh=change('terrain_heightAtPoint',0,0,2) if fires==0xffff else hooks
        yield config,data,hh,ctor
      for fires in (0,1,2,4):
        for height in (0,0x41a00000,0x42b80000,0x42b80001,0x42c80000,0xc2500000,0x7fc00000):
          yield config,base+[(S+0x1a,2,fires)],change('terrain_heightAtPoint',height),ctor
      for allocation in (0,0x80000,'q:393216:0:0','q:393216:524288:0'):
        for fires in (0,1,3):
          yield config,base+[(S+0x1a,2,fires)],change('pool_new',allocation),cast
      for objects in ('l:','l:0','l:'+str(target),'l:'+str(second)+':'+str(target)+':'+str(target)):
        yield config,base+[(S+0x1a,2,3)],change('world_collectObjectsInBox',objects),ctor
      for field,size,value in ((target+0xc,4,0),(target+0x234,4,0),(handle+0xc,4,0),
                               (handle+0xc,4,second),(O+0xc,4,0x40),(B+0xc,4,second)):
        yield config,base+[(S+0x1a,2,3),(field,size,value)],hooks,ctor
      yield config,base,change('Creature.v20',0),ctor
      for count in (1,7,0xffffffff,0x80000000):
        yield config,base,change('model_getSamplePointCount',count),ctor
      for net in (0,1,128,255):
        for progress in (0,0x3f000000,0x3effffff,0x3f000001,0x3f7fffff,0x3f800000,0xbf800000,0x7fc00000,0x7f800000):
          yield config,base+[(SB.G_NETWORK_MODE,1,net),(B+0x18,4,progress)],hooks,run
      for value in (0,0x80000000,0x7fc00000,0x7f800000):
        yield config,base+[(B+0x20,4,value)],hooks,run
      # Prefix failures at every external service reached by construction and run.
      for fires in (0,1,3):
        for cb in ('pool_new','res_find','sound_playAtPos','StaticFire_ctor','terrain_heightAtPoint',
                   'world_collectObjectsInBox','Creature.v20','Creature.v31','Creature.v16',
                   'model_getSamplePointCount','model_getSamplePoint','particle_spawn','LightEffect_ctor'):
          for throws in (1,2):
            yield config,base+[(S+0x1a,2,fires)],change(cb,0,0,throws),ctor
      for cb in ('Player_clampToScreen','obj_moveWithCollision','Spell.v1'):
        for throws in (1,2):
          yield config,base+[(SB.G_NETWORK_MODE,1,0),(B+0x18,4,0x3f800000)],change(cb,0,0,throws),run
      # Live callback writes: no snapshot of mover/owner/skill survives a delegated call.
      for cb,field,value,commands in (
          ('res_find',B+0xc,second,ctor),('sound_playAtPos',B+0x2c,second,ctor),
          ('StaticFire_ctor',B+0x2c,second,ctor),('terrain_heightAtPoint',B+0xc,second,ctor),
          ('Creature.v20',B+0x2c,second,ctor),('Creature.v31',B+0x2c,second,ctor),
          ('particle_spawn',B+0x2c,second,ctor),('obj_moveWithCollision',B+0x2c,second,run),
          ('obj_moveWithCollision',B+0xc,second,run),('obj_moveWithCollision',B+0x18,0x3f800000,run)):
        ret=next(r for n,r,*_ in hooks if n==cb)
        # Mutation uses poke to retain each callback's independently typed return.
        yield config,base+[(S+0x1a,2,3)],change(cb,value,field),commands
      for adjustment in (0,16,65520):
        adjusted=S+(adjustment if adjustment<32768 else adjustment-65536)
        yield config,base+[(adjusted+0x60,4,T),(T+16,2,adjustment),(T+20,4,0x311340)],hooks,[('virtual',S,2,6,O,0xffffffff,65535)]
        adjusted=B+(adjustment if adjustment<32768 else adjustment-65536)
        yield config,base+[(vt+16,2,adjustment),(adjusted+0x2c,4,O)],hooks,[('spellvirtual',B,2,0,0)]
      yield config,base,hooks,[('external',0,2),('spellvirtual',B,2,0,0)]
      yield config,base,hooks,[('external',0,2),('namebody',0x311e3c),('spellvirtual',B,2,0,0)]
      for net in (0,1):
        for fires in (0,1,3):
          yield config,base+[(SB.G_NETWORK_MODE,1,net),(S+0x1a,2,fires)],hooks,cast+run*25


    # Complete remaining registered passive family, after the immutable prefix.
    for config in range(16):
      vt=0x71000
      base=[(O+0xc,4,0x60),(O+0x10,4,0x80000000),(O+0x14,4,0xbf800000),
            (O+0x18,4,0x41200000),(O+0x38,2,65535),(O+0x544,4,0x90000),
            (O+0x48,4,vt),(vt+240,2,0),(vt+244,4,0x123456),
            (S+0x60,4,T),(SB.G_RAND_SEED,4,0x12345678),
            (SB.G_COSMETIC_SEED,4,0xabcdefff),(SB.G_SEED3,4,0x76543210)]
      hooks=[('model_getSamplePointCount',7,0,0),('pool_new','q:'+str(B)+':'+str(B+0x1000),0,0),
             ('LightEffect_ctor',0,0,0,5,0),
             ('model_getSamplePoint','p:1073741824:3212836864:1082130432:0',0,0),
             ('particle_spawn',0,0,0),('StrongStancePhantom_ctor',0,0,0,5,0),
             ('res_find',0x777,0,0),('sound_playAtPos',0,0,0),('Creature.v30',0,0,0),('named',0,0,0)]
      trigger=[('stancetrigger',S,O,0xdeadbeef,0xffffffff)]
      effect=[('stuneffect',S,O)]
      def change(name,value=None,mutation=0,throws=0,*action):
        return [(n,r if value is None else value,mutation,throws,*action) if n==name else (n,r,m,t,*a)
                for n,r,m,t,*a in hooks]
      for seed in (0,1,2,0xffff,0x80000000,0xffffffff,0x12345678,0xabcdef01):
        for count in (1,2,7,65535,0x7fffffff,0xffffffff,0x80000000):
          data=base+[(SB.G_RAND_SEED,4,seed),(SB.G_COSMETIC_SEED,4,seed),(SB.G_SEED3,4,seed)]
          yield config,data,change('model_getSamplePointCount',count),trigger
      for light in (0,B):
        for phantom in (0,B+0x1000):
          hh=change('pool_new','q:'+str(light)+':'+str(phantom))
          yield config,base,hh,trigger
          yield config,base,hh,effect
      for target in (0,O):
        for flags in (0,8,0x20,0x40,0x60,0xffffffff):
          yield config,base+[(O+0xc,4,flags)],hooks,[('stuneffect',S,target)]
      for seed in (0,1,0xffff,0x80000000,0xffffffff,0x12345678,0xabcdef01):
        for light in (0,B):
          yield config,base+[(SB.G_RAND_SEED,4,seed)],change('pool_new',light),effect
      for field in (O+0x10,O+0x14,O+0x18):
        for pattern in (0,0x80000000,0x7f800000,0xff800000,0x7fc00001,0x00800000,0xffffffff):
          yield config,base+[(field,4,pattern)],hooks,trigger
          yield config,base+[(field,4,pattern)],hooks,effect
      for facing in (0,32767,32768,65535):
        yield config,base+[(O+0x38,2,facing)],hooks,trigger
      for cb in ('model_getSamplePointCount','pool_new','LightEffect_ctor','model_getSamplePoint',
                 'particle_spawn','StrongStancePhantom_ctor'):
        for throws in (1,2):
          yield config,base,change(cb,throws=throws),trigger
      # Phantom constructor failures occur after all 25 particles.
      for throws in (1,2):
        yield config,base,change('StrongStancePhantom_ctor',throws=throws),trigger
      for cb in ('res_find','sound_playAtPos','pool_new','LightEffect_ctor','Creature.v30'):
        for throws in (1,2): yield config,base,change(cb,throws=throws),effect
      for cb,field,value,commands in (
          ('model_getSamplePointCount',O+0x544,1,trigger),
          ('pool_new',O+0x10,0,trigger),('LightEffect_ctor',O+0x544,0x90001,trigger),
          ('particle_spawn',O+0x38,32768,trigger),('particle_spawn',SB.G_SEED3,0xffffffff,trigger),
          ('particle_spawn',SB.G_COSMETIC_SEED,0,trigger),
          ('StrongStancePhantom_ctor',O+0xc,0,trigger),
          ('res_find',O+0x10,0, effect),('sound_playAtPos',O+0x18,0,effect),
          ('pool_new',O+0xc,0,effect),('LightEffect_ctor',SB.G_RAND_SEED,0,effect),
          ('LightEffect_ctor',vt+240,65520,effect),('Creature.v30',O+0xc,0,effect)):
        # Preserve independently scripted return/constructor draw with an unlogged mutation.
        yield config,base,change(cb,value,field),commands
      for adjustment in (0,16,65520):
        yield config,base+[(vt+240,2,adjustment)],hooks,effect
      yield config,base,hooks,[('namebody',0x123456)]+effect
      yield config,base,hooks,[('external',1,30)]+effect
      for body,op in ((0x2f8d54,'lifecast'),(0x308434,'stancecast')):
        for adjustment in (0,16,65520):
          yield config,base+[(T+16,2,adjustment),(T+20,4,body)],hooks,[('virtual',S,2,0xffffffff,O,L,65535)]
          yield config,base,hooks,[(op,S,0xffffffff,O,L,65535)]
      yield config,base,hooks,trigger*2+effect*2


    # Complete virtual/callback routing family. All tables and services are authored.
    for config in range(16):
      for cls,op,off in ((0,'virtual',0x60),(1,'spellvirtual',0x14),(2,'objvirtual',0x48)):
        for slot in (0,1,2,5,10,14,15):
          args=([0xffffffff,O,L,65535] if cls==0 and slot==2 else
                [L,65535] if cls==1 and slot==2 else [7,0x80000000])
          for adjustment in (0,16,65520,32768,32767):
            base=[(S+off,4,T),(T+8*slot,2,adjustment),(T+8*slot+4,4,0x777777),
                  (L,4,0x7fc12345),(L+4,4,0x80000000),(L+8,4,0x7f800000)]
            for named in (False,True):
              cb='named' if named else ('Skill','Spell','Creature')[cls]+'.v'+str(slot)
              setup=[('namebody',0x777777)] if named else []
              for throws in (0,1,2):
                hooks=[(cb,0xffffffff,L,throws)]
                # Three public wrappers, explicit table path and class table path agree.
                for command in [(op,S,slot,*args),('route',cls,S,T,slot,*args)]+([('table',cls,S,0x90,slot,*args)] if cls!=2 else []):
                  yield config,base,hooks,setup+[command]
            # Empty aliases fall back to the original virtual name.
            yield config,base,[(('Skill','Spell','Creature')[cls]+'.v'+str(slot),9,0,0)],[('emptyname',0x777777),(op,S,slot,*args)]
      # Known Skill bodies execute regardless of slot; forced dispatch snapshots cast position.
      for adjustment in (0,16,65520):
        for body in (0x3c8220,0x2f8d54,0x308434):
          base=[(S+0x60,4,T),(T+16,2,adjustment),(T+20,4,body),(L,4,1),(L+4,4,2),(L+8,4,3)]
          for command in (('virtual',S,2,5,O,L,7),('route',0,S,T,2,5,O,L,7),('table',0,S,0,2,5,O,L,7)):
            yield config,base,[],[command]
            yield config,base,[('Skill.v2',42,L,0)],[('extskill',2),command]
            yield config,base,[('named',42,L,0)],[('namebody',body),('extskill',2),command]
      # Non-Skill/Spell tables use only adjustment; body names and inventories do not apply.
      for cls in (2,3):
        for adjustment in (0,65520,32768):
          for off in (0,0x48,0x90):
            for slot in (0,2,30,64):
              cb=('Creature','Other')[cls-2]+'.v'+str(slot)
              base=[(S+off,4,T),(T+slot*8,2,adjustment),(T+slot*8+4,4,0x3c8220)]
              yield config,base,[(cb,23,0,0)],[('namebody',0x3c8220),('table',cls,S,off,slot,1,2,3)]
      # Internal handle helpers ignore callback overrides and do not emit call records.
      pool=0x864ea8
      base=[(pool,4,N),(N,4,N+0x20),(N+0x20,4,N+0x40),(H,4,0),(H+4,4,0)]
      hooks=[('handle_link',9,0,1),('handle_clear',9,0,1),('handle_removeNode',9,0,1)]
      yield config,base,hooks,[('call',0,H,H+4,O),('call',2,N),('call',1,H)]
      yield config,base,hooks,[('call',0,H,H+4,O),('call',1,H)]
      yield config,[],hooks,[('call',1,H)]
      for throws in (0,1,2):
        for args in ((),(0,),(0xffffffff,0x80000000,7)):
          yield config,[],[('synthetic',17,L,throws)],[('call',3,*args)]


    # Remaining setup/scalar/statistic/factory family. Model receives already
    # populated synthetic memory above, as the native constructor does.
    for config in range(16):
      patterns=(0,1,0xffffffff,0x80000000,0x7fffffff,0x7fc12345,0xff800000,0x00800000)
      for pattern in patterns:
        for address in (0,L,L+1,0xfffffffc):
          yield config,[(address,4,pattern)],[],[('f32',address),('half',pattern)]
      values=(0,1,2,3,0x7ffffffe,0x7fffffff,0x80000000,0x80000001,0xfffffffd,0xfffffffe,0xffffffff)
      yield config,[],[],[('half',v) for v in values]+[('f32',L),('alloc',0),('alloc',1),('alloc',17)]
      for flags in (0,0x20,0x40,0x20040,0xffffffbf,0xffffffff):
        for obj in (0,O,0xfffffff0):
          yield config,[(obj+0xc,4,flags)],[],[('player',obj),('poke',obj+0xc,4,flags^0x40),('player',obj)]
      # Full returned bytes, not just length: UTF-8 decoding is never implied.
      for data in (b'',b'abc',bytes(range(1,256)),b'ABC\0ignored',b'\xff\x80\x01'*24):
        for limit in (0,1,2,3,63,64,65,255,256,0xffffffff,0x80000000):
          pokes=[(L+i,1,b) for i,b in enumerate(data)]
          count=min(len(data.split(b'\0')[0]),max(0,SB.s32(limit)))
          yield config,pokes,[],[('cstrlen',L,limit)]+[('cstrbyte',L,limit,i) for i in range(count+1)]
      yield config,[(0xffffffff,1,255)],[],[('cstrlen',0xffffffff,1),('cstrbyte',0xffffffff,1,0)]
      for stat in range(5):
        for level in (0,1,20,0xffffffff,0x80000000):
          for n in (0,1,20,0xffffffff):
            base=[(S+0x60,4,T),(T+14*8,2,65520),(T+14*8+4,4,0x7714),
                  (T+10*8,2,16),(T+10*8+4,4,0x7710)]
            hooks=[('Skill.v14',17,L,0),('Skill.v10',0x80000001,L+4,0)]
            yield config,base,hooks,[('stat14',S,stat,level,n)]
            for fail in (1,2):
              yield config,base,[('Skill.v14',17,L,fail),hooks[1]],[('stat14',S,stat,level,n)]
              yield config,base,[hooks[0],('Skill.v10',0x80000001,L+4,fail)],[('stat14',S,stat,level,n)]
      # Slot 14 can rewrite the table before stat() performs its live slot 10 read.
      base=[(S+0x60,4,T),(T+14*8+4,4,0x7714),(B+10*8,2,65528),(B+10*8+4,4,0x7710)]
      yield config,base,[('Skill.v14',B,0,0,4,S+0x60),('Skill.v10',91,0,0)],[('stat14',S,2,9,3)]
      # Internally routed base statistics and adjustment; synthetic stat table.
      base=[(S+0x60,4,T),(T+14*8+4,4,BODIES[14]),(T+10*8+4,4,BODIES[10])]
      for stat in range(5):
        data=base+[(S+0xe+2*stat,2,1),(S+2*stat,2,13),(S+0x18+4*stat,4,0x3f000000)]
        yield config,data,[],[('stat14',S,stat,lv,20) for lv in (0,1,2,20)]
      for slot in (0,14,15,16,0xffffffff):
        for n in (0,1,20,0xffffffff,0x80000000):
          for level in (0,1,0xffffffff,0x80000000):
            target=15 if slot==15 else 14
            base=[(S+0x60,4,T),(T+target*8,2,65520),(T+target*8+4,4,0x7700)]
            for fail in (0,1,2):
              yield config,base,[('Skill.v'+str(target),0x7fc12345,L,fail)],[('factory',slot,n,S,level)]
      # Two closures with distinct captures and repeated invocation.
      base=[(S+0x60,4,T),(T+14*8+4,4,0x7714),(T+15*8+4,4,0x7715)]
      yield config,base,[('Skill.v14',7,0,0),('Skill.v15','q:1:2:3',0,0)],[('factory',15,1,S,2),('factory',14,20,S,3),('factory',15,99,S,4)]


# Harness/inventory glue has no portable gameplay body. The runtime mana alias
# factory counts once; registered-body accounting remains separate below.
ACCOUNTING_EXCLUDED = {'shape_result', '_register'}
PORTED_UNITS = set(('Model._node_take Model._node_push Model._node_unlink Model._node_free Model._partner_drop\nModel.handle_link Model.list_push Model.handle_removeNode Model.handle_clear Model.handle_obj Model.list_objects\nModel._iter_skip Model._iter_park Model.iter_begin Model.iter_next Model.iter_end\nModel.skill_can_use_base Model.skill_cast_base Model.skill_mana_cost_base Model._stat Model.skill_get_stat\nModel.skill_get_stat_tier2 Model.skill_get_stat_for Model.skill_get_stat_for_handle Model.skill_adjust_level\nModel.skill_mana_from_stat4 Model.skill_stat_range Model.archery_fraction Model.lifetap_fraction\nModel.lifetap_stat1 Model.strongstance_stat0 Model.naturalresistance_fraction Model.stunningstrike_stat0_for\nModel.spell_rand Model.cosmetic_draw Model.seed3_draw Model.spell_base_ctor Model.creature_add_spell\nModel.alloca Model.creature_update_spells Model.spell_end Model.game_object_ctor Model.projectile_ctor Model.obj_move_and_collide').split())

PORTED_UNITS.update(('Model.minorhealing_cast Model.minorhealing_spell_ctor Model.minorhealing_run Model.holyshield_cast Model.holyshield_spell_ctor Model.holyshield_run Model.holyshield_absorb Model.healing_cast Model.healing_spell_ctor Model.healing_run Model._heal_share Model._sample_point Model.record Model.stat Model.stat_for Model.unit Model.signed_unit Model.ball Model.spell_ball Model.object_ctor Model.light_effect').split())

PORTED_UNITS.update(('list_fill Model.query_into Model.query_list Model.convertenemy_can_use Model.convertundead_can_use Model.convert_cast Model.convert_spell_ctor Model.convert_run Model._convert_word Model.convertenemy_cast Model.convertundead_cast Model.convertenemy_run Model.convertundead_run Model.convertenemy_spell_ctor Model.convertundead_spell_ctor').split())

PORTED_UNITS.update('Model.giantgrowth_can_use Model.giantgrowth_cast Model.giantgrowth_spell_ctor Model.giantgrowth_run Model.giantgrowth_step_scale Model.giantgrowth_emit Model.giantgrowth_owner_scale'.split())

PORTED_UNITS.update('Model.ancestralcall_can_use Model.ancestralcall_cast Model.ancestralcall_spell_ctor Model.ancestralcall_run Model.ancestralcall_tint_and_glow Model.ancestralcall_stat0_fraction Model.ancestralcall_stat1_fraction Model.ancestralcall_query_for'.split())

PORTED_UNITS.update('Model.bash_can_use Model.bash_cast Model.bash_spell_ctor Model.bash_windup_fx Model.bash_run Model._bash_done Model.criticalhit_can_use Model.criticalhit_cast Model.criticalhit_spell_ctor Model.criticalhit_set_tint Model.criticalhit_run Model.criticalhit_bonus_for Model.charge_cast Model.charge_spell_ctor Model.charge_run Model.charge_stat3_scaled Model.charge_on_contact Model._bash_range Model._charge_trail_point Model.stat_range'.split())
PORTED_UNITS.add('from_uint')

PORTED_UNITS.update('Model.resurrect_can_use Model.resurrect_find_target Model.resurrect_is_being_raised Model.resurrect_cast Model.resurrect_spell_ctor Model.resurrect_run Model.resurrect_on_owner_death'.split())

PORTED_UNITS.update('Model.cyclone_can_use Model.cyclone_cast Model.cyclone_spell_ctor Model.cyclone_run Model.cyclone_update_fx'.split())
PORTED_UNITS.update('Model.coneoffrost_cast Model.coneoffrost_spell_ctor Model.coneoffrost_run Model.dummydamager_ctor Model.dummydamager_run Model.dummydamager_on_touch Model.local_vec Model.obj_ball Model.descriptor'.split())

PORTED_UNITS.update('Model.firearrow_can_use Model.firearrow_cast Model.firearrow_ctor Model.firearrow_ctor2 Model.firearrow_run Model.firearrow_on_hit Model.firearrow_end Model.coldarrow_can_use Model.coldarrow_cast Model.coldarrow_ctor Model.coldarrow_ctor2 Model.coldarrow_run Model.coldarrow_on_hit Model.coldarrow_end Model.poisonarrow_can_use Model.poisonarrow_cast Model.poisonarrow_ctor Model.poisonarrow_ctor2 Model.poisonarrow_run Model.poisonarrow_on_hit Model.poisonarrow_end Model.explodearrow_can_use Model.explodearrow_cast Model.explodearrow_ctor Model.explodearrow_ctor2 Model.explodearrow_run Model.explodearrow_on_hit Model.explodearrow_explode Model.multifire_can_use Model.multifire_cast Model.bow_can_use Model._arrow_light Model._arrow_velocity_facing Model._arrow_velocity_dir Model._arrow_blocked_for_player Model._firearrow_common Model._firearrow_stats Model._monster_spread Model._arrow_flight Model._coldarrow_common Model._arrow_finish Model._flight_particle Model._coldarrow_particles Model._poisonarrow_common Model._poisonarrow_particles Model._explodearrow_particles'.split())

PORTED_UNITS.update('Model.shock_cast Model.shock_ctor Model.shock_in_chain Model.shock_build_chain Model._shock_wants Model.shock_run'.split())

PORTED_UNITS.update('down c_rem Model.froststorm_cast Model.froststorm_ctor Model._froststorm_point Model.froststorm_run Model._iceshard_particle Model.iceshard_run'.split())

PORTED_UNITS.update('Model.groundpound_can_use Model.groundpound_cast Model.groundpound_effect_ctor Model._groundpound_stat Model.groundpound_effect_run'.split())
PORTED_UNITS.update('Model.harmtouch_can_use Model.harmtouch_cast Model.harmtouch_spell_ctor Model._harmtouch_tint Model.harmtouch_run Model.harmtouch_apply_to_hit'.split())

PORTED_UNITS.update('Model.darkness_cast Model.ambient_bytes Model.darkness_ctor Model._darkness_box Model.darkness_run'.split())
PORTED_UNITS.update('Model.shockoffire_cast Model.sparks_ctor Model.sparks_run Model.sparks_on_touch'.split())

PORTED_UNITS.update('Model.planarwind_cast Model.planarwind_spell_ctor Model._planarwind_strike Model.planarwind_run'.split())

PORTED_UNITS.update('Model.shadowstep_cast Model.shadowstep_spell_ctor Model.shadowstep_run'.split())
PORTED_UNITS.update('Model.strongstance_trigger Model.stunningstrike_effect'.split())
PORTED_UNITS.update('Model.call Model._virtual Model.skill_call Model.spell_call Model.vcall Model.table_call'.split())

PORTED_UNITS.update('Model.__init__ Model._stat_via_slot14 Model.cstr Model.f32 Model.half_toward_zero Model.is_player _mana_cost_method'.split())

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
    p.add_argument('--reuse-prefix',type=pathlib.Path,help='reuse a hash-checked pinned fixture after matching every input row')
    p.add_argument('--verbose',action='store_true',help='print each newly generated row for diagnosis')
    args = p.parse_args()
    prefix_manifest=None
    if args.reuse_prefix:
        prefix_manifest=json.loads(args.reuse_prefix.with_suffix('').with_suffix('.json').read_text())
        if prefix_manifest['research_commit']!=COMMIT or prefix_manifest['references']!=PINS:
            raise ValueError('prefix uses different evidence')
        if not set(prefix_manifest['reference_progress']['ported_units']) <= PORTED_UNITS:
            raise ValueError('prefix has unsupported units')
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
        args.output.parent.mkdir(parents=True,exist_ok=True)
        pathlib.Path('work').mkdir(exist_ok=True)
        digest, prefix_digest, count = hashlib.sha256(), hashlib.sha256(), 0
        prefix_count=prefix_manifest['rows'] if prefix_manifest else 0
        opener=gzip.open if args.reuse_prefix and args.reuse_prefix.suffix=='.gz' else open
        with tempfile.TemporaryDirectory(prefix='skills-b-output-',dir='work') as spool:
            staged=pathlib.Path(spool)/'vectors'
            with (opener(args.reuse_prefix,'rb') if args.reuse_prefix else contextlib.nullcontext(None)) as prior:
                with staged.open('wb') as file:
                    with (gzip.GzipFile(fileobj=file,mode='wb',mtime=0,filename='')
                          if args.output.suffix=='.gz' else contextlib.nullcontext(file)) as archive:
                        for config,pokes,hooks,commands in scenarios(SB):
                            inputs='\t'.join((str(config),flat(pokes),flat(hooks),flat(commands)))
                            if count < prefix_count:
                                row=prior.readline()
                                fields=row.rstrip(b'\n').split(b'\t')
                                if len(fields)!=5 or b'\t'.join(fields[:4])!=inputs.encode('ascii') or not row.endswith(b'\n'):
                                    raise ValueError('prefix input mismatch at row '+str(count))
                                prefix_digest.update(row)
                                if count+1==prefix_count:
                                    if prior.read(1) or prefix_digest.hexdigest()!=prefix_manifest['vectors_sha256']:
                                        raise ValueError('prefix hash/count mismatch')
                                    print('Verified and reused',prefix_count,'preceding rows',flush=True)
                            else:
                                if args.verbose: print('Generating',count,commands,flush=True)
                                row=(inputs+'\t'+run(SB,config,pokes,hooks,commands)+'\n').encode('ascii')
                            archive.write(row); digest.update(row); count+=1
                            if count%5000==0: print('Processed',count,'rows',flush=True)
                if count < prefix_count: raise ValueError('prefix exceeds generated scenarios')
            staged.replace(args.output)
    supported = set('Skill_canUse_base Skill_cast_base Skill_manaCost_base Skill_getStatRange Skill_getStat Skill_getStatTier2 Skill_getStatFor Skill_getStatForHandle Skill_adjustLevel Skill_manaFromStat4 Spell_rand GameObject_rand SkillArchery_getStat0Fraction SkillLifeTap_getStat0Fraction SkillLifeTap_getStat1 SkillStrongStance_getStat0 SkillNaturalResistance_getStat0Fraction SkillStunningStrike_getStat0ForTarget'.split())
    supported.update('Skill%d_manaCost'%sid for sid in SB.MANA_COST)
    supported.update('Creature_updateSpells Projectile_ctor obj_moveAndCollide'.split())
    supported.update('handle_link handle_clear handle_removeNode list_push iter_begin iter_next iter_end creature_addSpell'.split())
    supported.update('SkillMinorHealing_cast SpellMinorHealing_ctor SpellMinorHealing_run SkillHolyShield_cast SpellHolyShield_ctor SpellHolyShield_run SpellHolyShield_absorb SkillHealing_cast SpellHealing_ctor SpellHealing_run'.split())
    supported.update('SkillConvertEnemy_canUse SkillConvertEnemy_cast SpellConvertEnemy_ctor SpellConvertEnemy_run SkillConvertUndead_canUse SkillConvertUndead_cast SpellConvertUndead_ctor SpellConvertUndead_run'.split())
    supported.update('SkillGiantGrowth_canUse SkillGiantGrowth_cast SpellGiantGrowth_ctor SpellGiantGrowth_run SpellGiantGrowth_stepScale SpellGiantGrowth_emitParticles SpellGiantGrowth_getOwnerField0dc'.split())
    supported.update('SkillAncestralCall_canUse SkillAncestralCall_cast SpellAncestralCall_ctor SpellAncestralCall_run SpellAncestralCall_getStat0Fraction SpellAncestralCall_getStat1Fraction SpellAncestralCall_queryFor SpellAncestralCall_tintAndGlow'.split())
    supported.update('SkillBash_canUse SkillBash_cast SpellBash_ctor SpellBash_run SpellBash_windupFx SkillCriticalHit_canUse SkillCriticalHit_cast SkillCriticalHit_critBonusFor SpellCriticalHit_ctor SpellCriticalHit_run SpellCriticalHit_setTint SkillCharge_cast SpellCharge_ctor SpellCharge_run SpellCharge_getStat3Scaled SpellCharge_onContact'.split())
    supported.update('SkillResurrect_canUse SkillResurrect_findTarget SkillResurrect_cast SkillResurrect_isSlotBeingRaised SpellResurrect_ctor SpellResurrect_run SpellResurrect_onOwnerDeath'.split())
    supported.update('SkillCyclone_canUse SkillCyclone_cast SpellCyclone_ctor SpellCyclone_run SpellCyclone_updateFx'.split())
    supported.update('SkillConeOfFrost_cast SpellConeOfFrost_ctor SpellConeOfFrost_run DummyDamager_ctor DummyDamager_run DummyDamager_onTouch'.split())
    supported.update('SkillFireArrow_canUse SkillFireArrow_cast FireArrowProjectile_ctor FireArrowProjectile_ctor2 FireArrowProjectile_run FireArrowProjectile_onHit FireArrowProjectile_end SkillColdArrow_canUse SkillColdArrow_cast ColdArrowProjectile_ctor ColdArrowProjectile_ctor2 ColdArrowProjectile_run ColdArrowProjectile_onHit ColdArrowProjectile_end SkillPoisonArrow_canUse SkillPoisonArrow_cast PoisonArrowProjectile_ctor PoisonArrowProjectile_ctor2 PoisonArrowProjectile_run PoisonArrowProjectile_onHit PoisonArrowProjectile_end SkillExplodeArrow_canUse SkillExplodeArrow_cast ExplodeArrowProjectile_ctor ExplodeArrowProjectile_ctor2 ExplodeArrowProjectile_run ExplodeArrowProjectile_onHit ExplodeArrowProjectile_explode SkillMultiFire_canUse SkillMultiFire_cast'.split())
    supported.update('SkillShock_cast ShockProjectile_ctor ShockProjectile_run ShockProjectile_buildChain ShockProjectile_inChain'.split())
    supported.update('SkillGroundPound_canUse SkillGroundPound_cast GroundPoundEffect_ctor GroundPoundEffect_run'.split())
    supported.update('SkillFrostStorm_cast FrostStormSpell_ctor FrostStormSpell_run FrostStormIceShard_run'.split())
    supported.update('SkillHarmTouch_canUse SkillHarmTouch_cast SpellHarmTouch_ctor SpellHarmTouch_run SpellHarmTouch_applyToHit'.split())
    supported.update('SkillDarkness_cast DarknessProjectile_ctor DarknessProjectile_run ambientBytes_14a028'.split())
    supported.update('SkillShockOfFire_cast Sparks_ctor Sparks_run Sparks_onTouch'.split())
    supported.update('SkillPlanarWind_cast SpellPlanarWind_ctor SpellPlanarWind_run'.split())
    supported.update('SkillShadowStep_cast SpellShadowStep_ctor SpellShadowStep_run'.split())
    supported.update('SkillLifeTap_cast SkillStrongStance_cast SkillStrongStance_trigger SkillStunningStrike_spawnEffect'.split())
    unknown = supported - set(SB.FUNCTIONS)
    if unknown: raise ValueError('unknown supported functions: '+str(unknown))
    catalog = {str(key): value for key,value in SB.SKILLS.items()}

    manifest = {'schema':1,'reference_progress':progress,'rows':count,'research_commit':COMMIT,'references':PINS,
                'vectors_sha256':digest.hexdigest(),
                'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                'supported_functions':sorted(supported),'unsupported_functions':sorted(set(SB.FUNCTIONS)-supported),
                'skill_catalog':catalog}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print('Generated',count,'complete-state cases')
if __name__=='__main__': main()
