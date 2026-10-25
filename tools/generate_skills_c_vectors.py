#!/usr/bin/env python3
"""Execute the immutable RTA statistics/passive contract on authored sparse memory.

No game files, retail tables, decompiler text, or native results are read.
"""
import argparse
import ast
import copy
import gzip
import hashlib
import importlib
import json
import pathlib
import random
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'native/tests/vectors/skills_c_evidence.json'
METHODS = ('skill_get_stat', 'skill_get_stat_tier2', 'skill_stat_for',
           'skill_stat_for_handle', 'skill_adjust_level', 'skill_mana_from_stat4',
           'skill_stat_range', 'skill_can_use_default', 'skill_cast_default',
           'skill_mana_default', 'skill_mana_cost', 'statistic_spread',
           'berserkerrage_swing_factor', 'berserkerrage_damage_factor',
           'passive_complement', 'throwing_fraction', 'battlemage_fraction',
           'penetratingarrows_fractions', 'spell_base', 'spell_rand', 'obj_rand',
           'spell_unit', 'obj_unit', 'spell_ball', 'obj_ball', 'visual_pos',
           'handle_link', 'handle_remove', 'handle_clear', 'handle_items',
           'retribution_can_use','retribution_cast','retribution_spell_ctor',
           'retribution_spell_run','retribution_heal_owner',
           'spiritshield_can_use','spiritshield_cast','spiritshield_spell_ctor',
           'spiritshield_spell_run','_spiritshield_particles','spiritshield_roll_absorb',
           'frostarmor_can_use','frostarmor_cast','frostarmor_spell_ctor',
           'frostarmor_spell_run','frostarmor_on_hit',
           'find_spell_by_type','is_player','record','flash','light_effect',
           'model_sample','sound_at','spell_destroy',
           'bloodlust_can_use','bloodlust_cast','ghost_hands','bloodlust_spell_ctor',
           'hand_streamers','bloodlust_spell_run','_bloodlust_glow',
           'bloodlust_complement','bloodlust_multiplier','shout_animation',
           'shout_interrupted','callee_store',
           'riposte_cast','riposte_counter_attack','dodge_mana_cost','dodge_fraction',
           'healing_cast','healing_spell_ctor','_healing_amount','_healing_flash','healing_spell_run',
           'summonarrows_can_use','summonaxe_cast','summonarrows_cast','_summon_cast',
           'summonaxe_build_item','_summon_ctor','summonaxe_spell_run','summonarrows_spell_run',
           '_summon_run','item_record',
           'criticalhit_can_use','criticalhit_cast','criticalhit_mana_cost','criticalhit_spell_ctor',
           'criticalhit_set_tint','criticalhit_spell_run','criticalhit_bonus_for','light_record',
           'fireaxe_can_use','coldaxe_can_use','_axe_can_use','fireaxe_cast','coldaxe_cast','_axe_cast',
           'battlecry_can_use','battlecry_cast','battlecry_spell_ctor','_battlecry_streamers',
           'battlecry_set_tint','battlecry_spell_run','battlecry_affects','attach_point',
           'lightningstrike_cast','pusstorm_cast','_storm_cast','_storm_ctor',
           'lightningstrike_run','pusstorm_run','_storm_run','pustule_ctor',
           'diseaseblade_can_use','diseaseblade_cast','player_weapons','write_disease',
           'diseaseblade_spell_ctor','diseaseblade_spell_run',
           'raisearmy_can_use','raisearmy_cast','raisearmy_is_raisable','raisearmy_spell_ctor',
           'raisearmy_spell_run','raisearmy_raise_in_ring','_raisearmy_dress',
           'drowsy_cast','drowsy_spell_ctor','_drowsy_burst','drowsy_spell_run','drowsy_fraction',
           'waveofflame_can_use','waveofflame_cast','waveofflame_spell_ctor',
           'waveofflame_spell_run','waveofflame_hit_test',
           'roar_can_use','roar_cast','roar_spell_ctor','_roar_breath','roar_spell_run','roar_hit_ring',
           'unspokenword_cast','unspokenword_spell_ctor','_unspokenword_phase',
           'unspokenword_spell_run','unspokenword_hit_ring',
           'poisonbolt_cast','poisonbolt_ctor','poisonbolt_run','poisonbolt_on_hit','poisonbolt_burst',
           'summonbadger_can_use','summonbadger_cast','summonbadger_spell_ctor','summonbadger_spell_run',
           'root_can_use','root_cast','root_spell_ctor','root_spell_run',
           'coneofshock_cast','coneofshock_spell_ctor','coneofshock_spell_run',
           'handle_link','handle_clear','shape_result','c_div','c_rem','desc','call','skill_call','table_call',
           'tagar_cast','tagar_spell_ctor','tagar_is_valid','tagar_pick_target','tagar_tick_victims','tagar_spell_run',
           'summon_can_use','summon_cast','summon_spell_ctor','summon_spell_run',
           '_summon_sample','cosmetic_draw','_cosmetic_ball','skelight_ctor')
SERVICES = ('typeinfo','creature_findSpellByType','pool_new','light_link','res_find',
            'sound_playAtPos','particleGroup_create','particleGroup_begin',
            'fx_emitFromModelVerts','particleGroup_end','light_relink','Spell.v1',
            'LightEffect_ctor','Creature.v16','model_getSamplePointCount',
            'model_getSamplePoint','particle_spawn','damage_apply','fx_spawn','net_objOrGhost','model_getAttachPoint',
            'libc_sprintf','anim_startBlendInOut','net_isLocalSlot','Player_animStep',
            'glow_add','sound_stop','sound_playOnObject','GlowingShaft_ctor',
            'world_collectObjectsInBox','Creature.v21','Creature.v22',
            'particleGroup_setMatrix','terrain_heightAtPoint','item_parseDescriptor','player_itemWeight',
            'inventory_giveItem','player_recalcLoad','inventory_equipItem','player_resetIdleAnim',
            'player_voice','particleGroup_free','light_unlink','player_getSkillRank',
            'text_lookup','hud_showMessage','item_getResourceIndex','item_copyMutableFields',
            'playerProjectile_ctor','hud_countActiveNotices','inventory_removeItem','Creature.v20','Creature.v30',
            'GameObject_ctor','terrain_height','obj_runEveryFrame','obj_updateGridCell',
            'Creature.v1','LightningBolt_ctor','Holder.v7','Holder.v8','creature_bossRank',
            'world_createObject','Creature_setCharm','Creature.v37','anim_pushWithWeights',
            'Creature.v27','Holder.v15','Holder.v16','Holder.v1','str_prefixDiffers','model_buildHideMask',
            'world_collectObjectsInArc','creature_addSpell','handle_clear','world_collectInSphere','camera_shake','water_test','water_disturb',
            'trail_create','trail_setLength','trail_setColor','trail_setPoints',
            'Projectile_ctor','obj_moveAndCollide','poisonGasSpell_ctor','Creature_setEffectTimer',
            'world_findObjectByName','world_findFreeSpot','CastBadger_ctor','Creature.v34','LightningBolt_ctor2','handle_link')

SKELETON_SERVICES={115:'obj_getPropInt',116:'obj_getPropStr',117:'obj_moveOffGrid',
                   118:'Creature_setStats',119:'anim_clear',120:'anim_push',121:'Creature_init',
                   122:'anim_start',123:'anim_syncToSimFrame',125:'Holder.v4',126:'Holder.v6'}

NAMED_NAMES=('authored','Spell.v2','Holder.v2','Other.v3','Skill.v16')

def callback_name(slot):
    if slot>=115:return SKELETON_SERVICES[slot]
    if slot==114:return 'creature_findSpell'
    if slot>=109:return NAMED_NAMES[slot-109]
    return 'Skill.v%d' % slot if slot < 16 else SERVICES[slot-16]

def callback_id(name):
    if name in SKELETON_SERVICES.values():return next(id for id,value in SKELETON_SERVICES.items() if value==name)
    if name=='creature_findSpell':return 114
    if name in NAMED_NAMES:return 109+NAMED_NAMES.index(name)
    return int(name.split('v')[1]) if name.startswith('Skill.v') else 16+SERVICES.index(name)

def call_words(value):
    # Fixed signatures retain vector field boundaries. Damage descriptors expand
    # the constant header and all four slots, including canonical empty slots.
    if isinstance(value, tuple) and value and value[0] == 'item':
        return [value[1],value[2]] + list(bytes.fromhex(value[3])) + [len(value)-4] + [
            n for slot in value[4:] for n in [len(slot),*slot]]
    if isinstance(value, tuple) and value and value[0] == 'desc':
        return list(bytes.fromhex(value[1])) + [n for slot in value[2:]
                for n in (list(slot)+[0]*(4-len(slot)))]
    if isinstance(value, (list,tuple)):
        return [n for item in value for n in call_words(item)]
    if value == "stack": return [0] # explicit stack marker, never a fabricated address
    return [0 if value is None else value & M32]

MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')
S, TABLE, RECORD, OBJ, HANDLE, NODE = 0x1000, 0x2000, 0x10000, 0x3000, 0x4000, 0x4100
M32 = 0xffffffff


def flatten(value):
    if isinstance(value, (list, tuple)):
        return [len(value)] + [n for v in value for n in flatten(v)]
    return [int(value)]


def encode(value):
    return ' '.join(map(str, flatten(value)))


def snapshot(model, configs, extended=False):
    result=copy.deepcopy([sorted(model.mem.bytes.items()), model.mem.log, model.writes,
            [[callback_id(name), call_words(args)] for name, args in model.calls if not extended or name not in NAMED_NAMES],
            model.draws, sorted(callback_id(name) for name in model.external), configs])
    if extended:
        result.append([[order,list(name.encode('ascii')),[value_words(arg) for arg in args]]
                       for order,(name,args) in enumerate(model.calls) if name in NAMED_NAMES])
    return result


def value_words(value):
    if value is None: return [0]
    if isinstance(value,int): return [1,value & M32]
    if isinstance(value,bytes): return [4,len(value)]+[n for v in value for n in [1,v]]
    if isinstance(value,tuple) and value and value[0]=='buf':
        return [5,1]+value_words(value[1])
    return [3 if isinstance(value,tuple) else 2,len(value)]+[n for v in value for n in value_words(v)]


def read_value(words,at):
    kind=words[at];at+=1
    if kind==0:return None,at
    if kind==1:return words[at],at+1
    count=words[at];at+=1;values=[]
    for _ in range(count):
        value,at=read_value(words,at);values.append(value)
    return (bytes(values) if kind==4 else ('buf',values[0]) if kind==5 else
            tuple(values) if kind==3 else values),at


def helper_result(SK,operation,args):
    if operation in (168,169):
        value=(SK.c_div if operation==168 else SK.c_rem)(SK.s32(args[0]),SK.s32(args[1]))
        return [value & M32,(value>>32)&M32]
    if operation==170:
        slots=[];at=1
        for _ in range(args[0]):
            count=args[at];at+=1;slots.append(args[at:at+count]);at+=count
        value=SK.desc(*slots)
        return list(bytes.fromhex(value[1]))+[len(value)-2]+[n for slot in value[2:] for n in [len(slot),*slot]]
    ret=chr(args[0]);length=args[1];ints=''.join(map(chr,args[2:2+length]));at=2+length
    count=args[at];at+=1;arguments=[]
    for _ in range(count):
        value,at=read_value(args,at);arguments.append(value)
    integer,at=read_value(args,at);floating,at=read_value(args,at)
    count=args[at];at+=1;outs=[]
    for _ in range(count):
        value,at=read_value(args,at);outs.append(value)
    name=next(name for name,ext in SK.EXTERNALS.items() if ext.ints==ints and ext.ret==ret)
    return value_words(SK.shape_result(name,arguments,integer,floating,outs))


def observe(SK, RC, C, config, scenario):
    C.set_fpu(MODELS[config // 4], bool(config & 2), bool(config & 1))
    pokes, externals, configs, commands = scenario
    mem = RC.Memory()
    for address, width, value in pokes:
        mem.poke(address, width, value)
    callbacks = {}
    for slot, result, poke, throwing in configs:
        def callback(*args, result=result, poke=poke, throwing=throwing, slot=slot):
            if poke:
                mem.poke(*poke)
            if throwing:
                raise RuntimeError('authored callback exception')
            if 109<=slot<114:return read_value(result,0)[0]
            if slot == 31 and any(op == 161 for op, _ in commands):
                return tuple(result), (0,0,0)
            if slot == 104: return tuple(result[:3]), result[3]
            if slot in (88,91): return tuple(result)
            if slot == 87 and len(result) == 2:
                return result[0] | result[1]<<32
            if slot == 36 and len(result) == 6:
                return tuple(result[:3]), tuple(result[3:])
            return result[0] if len(result) == 1 else tuple(result)
        callbacks[callback_name(slot)] = callback
    model = SK.Model(mem, callbacks, [callback_name(slot) for slot in externals])
    extended=any(op>=171 for op,_ in commands)
    out = [snapshot(model, configs,extended)]
    for operation, args in commands:
        try:
            arguments = list(args)
            if operation==171:
                at=1;name=bytes(args[at:at+args[0]]).decode('ascii');at+=args[0]
                length=args[at];at+=1;ints=bytes(args[at:at+length]).decode('ascii');at+=length
                ret=chr(args[at]);at+=1;count=args[at];at+=1;values=[]
                for _ in range(count):
                    value,at=read_value(args,at);values.append(value)
                old=SK.EXTERNALS.get(name)
                SK.EXTERNALS[name]=SK.Ext(0,ints,0,ret)
                try:value=model.call(name,*values)
                finally:
                    if old is None:del SK.EXTERNALS[name]
                    else:SK.EXTERNALS[name]=old
                out.append([0,value_words(value),snapshot(model,configs,extended)])
                continue
            if operation==172:
                arguments=list(args)
                table=model.u32(args[0]+0x60);body=SK.SKILL_BODIES.get(model.u32(table+8*args[1]+4)) or SK.AM.SKILL_BODIES.get(model.u32(table+8*args[1]+4))
                if body and body[0]==2 and len(arguments)==6:
                    ptr=arguments[4];arguments[4]=tuple(model.u32(ptr+4*k) for k in range(3))
            elif operation==173:
                count=args[0];cls=bytes(args[1:1+count]).decode('ascii');arguments=[cls,*args[1+count:]]
                if cls=='Spell' and len(arguments)==6:
                    ptr=arguments[4];arguments[4]=tuple(model.u32(ptr+4*k) for k in range(3))
            elif operation in (174,181): arguments=[*args[:3],tuple(args[3:6]),args[6]]
            elif operation==184: arguments=[args[0],SK.s32(args[1])]
            elif operation==187: arguments=[args[0],tuple(args[1:4]),*args[4:]]
            elif operation in (185,186): arguments=[]
            elif operation in (165,166): arguments = ['stack',*args[1:]]
            elif operation == 33: arguments = [args[0],tuple(args[1:4]),args[4]]
            elif operation == 50:
                arguments = [args[0],tuple(args[1:4]),args[4],tuple(args[6:10]),
                             tuple(args[14:18]),args[5],tuple(args[10:14])]
            elif operation == 51: arguments = [args[0],args[1],tuple(args[2:5]),args[5]]
            elif operation == 52: arguments = [args[0],args[1],tuple(args[2:5])]
            elif operation == 55: arguments = [args[0],args[1],args[2],tuple(args[3:6]),args[6]]
            elif operation == 58: arguments = [args[0],args[1],(tuple(args[2:5]),tuple(args[5:8])),args[8],args[9]]
            elif operation == 60: arguments = [args[0],(tuple(args[1:4]),tuple(args[4:7]))]
            elif operation == 63:
                end = 4+2*args[3]
                names = {SK.s32(args[k]):args[k+1] for k in range(4,end,2)}
                events = tuple((SK.s32(args[k]),args[k+1],args[k+2]) for k in range(end+1,len(args),3))
                arguments = [args[0],names,args[1],events,args[2]]
            elif operation == 64: arguments = [args[0],args[1],args[3] if args[2] else None]
            elif operation == 65: arguments = [args[0],args[1],args[2] | args[3]<<32]
            elif operation in (76,77): arguments = [args[0],args[1],args[2],tuple(args[3:6]),args[6]]
            elif operation == 78: arguments = [SK.SummonItems.SI_ARROWS if args[0] else SK.SummonItems.SI_AXE,
                                               args[1],args[2],args[3],tuple(args[4:7])]
            elif operation == 80: arguments = [SK.SummonItems.SI_ARROWS if args[0] else SK.SummonItems.SI_AXE,*args[1:]]
            elif operation == 83: arguments = [SK.SummonItems.SI_ARROWS if args[0] else SK.SummonItems.SI_AXE,args[1]]
            elif operation == 84:
                slots=[]; at=3
                for _ in range(args[2]):
                    length=args[at]; at+=1; slots.append(tuple(args[at:at+length])); at+=length
                arguments=[args[0],args[1],tuple(slots)]
            elif operation == 95: arguments=[SK.ThrownAxes.TA_COLD if args[0] else SK.ThrownAxes.TA_FIRE,args[1]]
            elif operation in (96,97): arguments=[*args[:3],tuple(args[3:6]),args[6]]
            elif operation == 98: arguments=[SK.ThrownAxes.TA_COLD if args[0] else SK.ThrownAxes.TA_FIRE,*args[1:4],tuple(args[4:7])]
            elif operation == 102: arguments=[args[0],args[2] if args[1] else None]
            elif operation == 104: arguments=[args[0],tuple(args[1:4]),args[4]]
            elif operation == 106: arguments=[args[0],args[1],args[3] if args[2] else None]
            elif operation in (107,108): arguments=[*args[:3],tuple(args[3:6]),SK.s32(args[6])]
            elif operation == 109: arguments=[SK.Storms.STORM_PS if args[0] else SK.Storms.STORM_LS,*args[1:4],tuple(args[4:7]),SK.s32(args[7])]
            elif operation == 110: arguments=[SK.Storms.STORM_PS if args[0] else SK.Storms.STORM_LS,args[1],tuple(args[2:5]),tuple(args[5:8]),*map(SK.s32,args[8:12]),args[12],args[13]]
            elif operation == 113: arguments=[SK.Storms.STORM_PS if args[0] else SK.Storms.STORM_LS,args[1]]
            elif operation == 114: arguments=[args[0],tuple(args[1:4]),tuple(args[4:7]),tuple(args[7:10]),SK.s32(args[10])]
            elif operation == 118: arguments=[args[0],args[1],SK.s32(args[2]),SK.s32(args[3])]
            elif operation in (155,159,162): arguments=[*args[:3],tuple(args[3:6]),SK.s32(args[6])]
            elif operation == 164: arguments=[args[0],tuple(args[1:4]),SK.s32(args[4])]
            elif operation == 149: arguments=[*args[:3],tuple(args[3:6]),SK.s32(args[6])]
            elif operation == 150: arguments=[args[0],tuple(args[1:4]),SK.s32(args[4]),*args[5:]]
            elif operation == 144: arguments=[*args[:3],tuple(args[3:6]),SK.s32(args[6])]
            elif operation == 146: arguments=[args[0],SK.s32(args[1])]
            elif operation == 141: arguments=[args[0],SK.s32(args[1]),tuple(args[2:5]),*args[5:]]
            elif operation == 135: arguments=[*args[:3],tuple(args[3:6]),args[6]]
            elif operation in (122,128): arguments=[*args[:3],tuple(args[3:6]),args[6]]
            elif operation == 125: arguments=[args[0],args[1],args[2]]
            elif operation == 120: arguments=[args[0],tuple(args[1:4]),args[4]]
            elif operation == 92: arguments=[args[0],tuple(args[1:5]),*args[5:8],tuple(args[8:11])]
            value = helper_result(SK,operation,args) if 167<=operation<=170 else SK.item_record(*arguments) if operation == 84 else getattr(model, METHODS[operation])(*arguments)
            result = (value if 167<=operation<=170 else call_words(value) if operation in (79,84) else [] if value is None else
                      [n for pair in value for n in pair] if operation in (29,56) else
                      list(value) if isinstance(value, tuple) else [value & M32])
            result = [v & M32 for v in result]
            status = 0
        except (RuntimeError, ZeroDivisionError):
            status, result = 1, []
        out.append([status, result, snapshot(model, configs,extended)])
    return encode(out)


def setup(SK, rng, variant):
    pokes = [[S+0x60, 4, TABLE], [SK.G_RAND_SEED, 4, rng.getrandbits(32)],
             [RECORD+0xac, 4, (0,0x3f000000,0x3f800000,0xbf800000,0x7fffffff)[variant%5]],
             [RECORD+0xb0, 4, (0x3f800000,0,0xbf800000)[variant%3]],
             [RECORD+0x43d8,4, OBJ if variant%4 else 0], [OBJ+0xc,4,0x40 if variant%2 else 0x20],
             [HANDLE,4,NODE if variant%3 else 0], [NODE+0xc,4,OBJ if variant%5 else 0], [0xc,4,0x40 if variant%7==0 else 0]]
    for slot, body in SK.SKILL_BASE.items():
        pokes += [[TABLE+slot*8,2,0],[TABLE+slot*8+4,4,body]]
    steps = [0, 0x3f000000,0xbf000000,0x40400fb6,0x40001034,0x40800fe6,0x3e800000,0x7fffffff,0x80000000]
    for i in range(5):
        pokes += [[S+0x0e+2*i,2, rng.choice([-3,0,1,5,20])&0xffff],
                  [S+0x18+2*i,2,rng.choice([-32768,-120,-1,0,1,20,100,32767])&0xffff],
                  [S+0x24+4*i,4,rng.choice(steps)],
                  [S+0x38+2*i,2,rng.choice([-1,0,1,3,20])&0xffff],
                  [S+0x42+2*i,2,rng.choice([-100,0,10,32767])&0xffff],
                  [S+0x4c+4*i,4,rng.choice(steps)]]
    for i in range(8):
        pokes += [[OBJ+0x90+2*i,2,rng.randrange(65536)]]
    return pokes


def scenarios(SK):
    rng = random.Random(69101)
    levels = [0,1,2,5,20,40,41,42,60,100,127,M32,M32-2,0x80000000,0x7fffffff]
    for variant in range(96):
        pokes = setup(SK,rng,variant)
        commands = []
        for stat in range(5):
            level=levels[(variant+stat)%len(levels)]
            cap=[0,10,20,25,M32][(variant+stat)%5]
            commands += [[0,[S,stat,level]], [1,[S,stat,level,cap]],
                         [2,[S,stat,level,OBJ,cap]], [2,[S,stat,level,0,cap]],
                         [3,[S,stat,level,HANDLE,cap]],
                         [6,[S,stat,level,variant%2,[0,0x3e4ccccc,0x3f800000,0xbf800000,0x40000000][variant%5]]],
                         [11,[S,stat,level,OBJ]], [11,[S,stat,level,0]]]
        rank = levels[variant%len(levels)]
        commands += [[4,[S,rank,0x14]], [5,[S,rank,2]], [7,[S,OBJ]],
                     [8,[S,rank,OBJ,0,0]], [9,[S,rank]], [10,[S,rank]],
                     [12,[S,rank,RECORD,[0,1,30,59,60,61,120,M32,0x80000000][variant%9]]],
                     [13,[S,rank,RECORD,30]], [14,[S,rank]], [15,[S,rank]],
                     [16,[S,rank]], [17,[S,rank]]]
        yield [pokes,[],[],commands]
    # All delegated shapes, forced and unknown-body dispatch, and mutation before
    # failure. Adjustment receives its own signed16 this pointer on each route.
    for slot in (8,10,11,12,14,15):
        for throwing in (0,1):
            for forced in (0,1):
                pokes=setup(SK,rng,slot)
                pokes += [[TABLE+8*slot,2,0xfff0]]
                if not forced: pokes += [[TABLE+8*slot+4,4,0xdeadbeef]]
                configs=[[slot,[23,31] if slot==8 else [0xffffffed], [OBJ+0x94,2,0x3456],throwing]]
                commands=[[10,[S,20]],[6,[S,0,20,1,0x40000000]],
                          [11,[S,0,20,OBJ]],[11,[S,0,20,0]],
                          [12,[S,20,RECORD,30]],[13,[S,20,RECORD,30]],
                          [14,[S,20]],[15,[S,20]],[16,[S,20]],[17,[S,20]]]
                yield [pokes,[slot] if forced else [],configs,commands]


def runtime_scenarios(SK):
    rng = random.Random(16964)
    for i in range(48):
        spell, obj, head, back = 0x6000, 0x7000, 0x8000, 0x8100
        nodes = [0x9000+20*n for n in range(8)]
        pokes = [[SK.G_RAND_SEED,4,rng.getrandbits(32)],
                 [spell+0x10,4,rng.getrandbits(32)],
                 [obj+0x44,4,rng.getrandbits(32)],
                 [obj+0x10,4,rng.getrandbits(32)],
                 [obj+0x14,4,rng.getrandbits(32)],
                 [obj+0x3c,4,rng.getrandbits(32)],
                 [obj+0x38,2,[0,0x4000,0x8000,0xffff,0x2345][i%5]],
                 [obj+0xe0,4,[0,0x80000000,0x3f800000,0xbf800000,0x7fffffff][i%5]],
                 [SK.G_HANDLE_FREE,4,nodes[0] if i%4 else 0]]
        for j,node in enumerate(nodes):
            pokes += [[node,4,nodes[j+1] if j+1<len(nodes) else 0],
                      [node+0x10,2,0xffff]]
        commands = [[19,[spell]],[20,[obj]],[21,[spell]],[22,[obj]],
                    [23,[spell]],[24,[obj]],[25,[obj]],
                    [18,[spell,[0,255,256,M32][i%4],obj,S,0x12345678]]]
        commands += [[23,[spell]],[24,[obj]]] * 3
        # Empty and full front/back lists, paired removals, clear and reuse.
        commands += [[29,[head]],[26,[head,back,obj]],[29,[head]],
                     [26,[head,back,obj+0x100]],[29,[head]],[29,[back]]]
        if i%4:
            commands += [[27,[nodes[0]]],[29,[head]],[29,[back]]]
        commands += [[28,[head]],[29,[head]],[29,[back]],
                     [26,[head,back,obj]],[28,[back]],[29,[head]],[29,[back]]]
        yield [pokes,[],[],commands]


def protection_scenarios(SK):
    rng=random.Random(699496)
    spell,owner,attacker,ctable,stable=0x6000,0x7000,0x8000,0x9000,0xa000
    for i in range(72):
        pokes=setup(SK,rng,i)
        # Uniform authored tiers make duration, chance and fraction independently
        # controllable; no retail skill tables enter these scenarios.
        for stat in range(5):
            value=([0,1,20,100,150,-30][i%6] if stat else [0,1,3,100][i%4])
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value&0xffff],
                      [S+0x24+4*stat,4,0],[S+0x38+2*stat,2,0xffff]]
        rank=[0,1,5,127,128,255,256,M32][i%8]
        slot=[0,1,M32,0x80000000][i%4]
        rec=(SK.G_PLAYERS+SK.s32(slot)*SK.REC_SIZE)&M32
        pokes += [[owner+0xc,4,[0x20,0x40,0x42,0][i%4]],
                  [owner+0x3c8,4,slot],[rec+0xac,4,[0,0x3f800000,0xbf800000,0x42c80000][i%4]],
                  [rec+0xb0,4,0x42c80000],[owner+0x48,4,ctable],
                  [ctable+128,2,[0,0xfff0,16][i%3]], [ctable+132,4,0xdeadbeef],
                  [stable+8,2,[0,0xfff0,16][i%3]], [spell+0x14,4,stable],
                  [spell+8,4,S],[spell+0xc,4,owner],[spell,1,rank&255],
                  [spell+0x10,4,rng.getrandbits(32)],
                  [spell+0x58,4,[0,0x3f800000,0x80000000][i%3]],
                  [SK.G_SIM_FRAME,4,[0,1,0x7fffffff,0x80000000,M32][i%5]],
                  [owner+0xdc,4,0x3fc00000],[owner+0xe0,4,[0,0x80000000,0x3f800000][i%3]],
                  [owner+0x38,2,[0,0x8000,0xffff][i%3]],
                  [owner+0x3c,4,0x41200000]]
        for obj in (owner,attacker):
            for k,raw in enumerate((0x3f800000,0xc0000000,0x40400000)):
                pokes += [[obj+0x10+4*k,4,raw]]
        configs=[]
        for service,name in enumerate(SERVICES[:19],16):
            result=([0xdead0001] if name=='typeinfo' else
                    [0 if i%2 else spell] if name=='creature_findSpellByType' else
                    [0 if i%3==0 else spell] if name=='pool_new' else
                    [0xface] if name=='res_find' else
                    [0x1234] if name=='particleGroup_create' else
                    [0xabc0] if name=='Creature.v16' else
                    [[7,-7,1,0][i%4]&M32] if name=='model_getSamplePointCount' else
                    [0x3f800000,0xc0000000,0x40400000] if name=='model_getSamplePoint' else [])
            configs.append([service,result,[],0])
        commands=[[30,[S,owner]],[35,[S,owner]],[41,[S,owner]],
                  [47,[owner]],[47,[0]],[48,[owner]],
                  [46,[owner,0x1234]],[49,[owner,1,1,0x10000,M32,0x8000]],
                  [49,[owner,0,5,6,7,8]],
                  [50,[0x8000,0,0x80000000,0x3f800000,owner,1]+list(range(12))],
                  [51,[owner,0xabc0,0,ONE:=0x3f800000,0x80000000,M32]],
                  [52,[0x100,0x200,0,ONE,0]], [53,[spell]]]
        for can,cast,ctor,run in ((30,31,32,33),(35,36,37,38),(41,42,43,44)):
            commands += [[cast,[S,rank,owner,0,0]],[ctor,[spell,rank,owner,S]]]
            if run==33:
                commands += [[run,[spell,0,ONE,0x80000000,0]],[34,[spell,0x42c80000]],
                             [34,[spell,0xbf800000]]]
            else:
                commands += [[run,[spell,0,0]]]
            if run==38:
                commands += [[40,[spell]],[40,[spell]],
                             [39,[spell,[0,0x3e800000,0x3f000000,0x3f400000,ONE,0x3fa00000,0xbe800000][i%7],i%2]]]
            if run==44:
                commands += [[45,[spell,0,0x42c80000]],[45,[spell,attacker,0x42c80000]]]
        yield [pokes,[],configs,commands]
    # Each Spirit Shield phase, before/at/after its deadline; separate run input
    # starts prevent constructors from erasing phase coverage.
    for phase in (0,1,2,3,4):
        for end in (0,0x3d000000,0x3f800000):
            pokes=[[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],
                   [spell+0x18,4,phase],[spell+0x1c,4,0],[spell+0x20,4,end],
                   [spell+0x10,4,123],[S+0x60,4,TABLE],[owner+0xc,4,0x42],
                   [owner+0x48,4,ctable],[spell+0x14,4,stable]]
            configs=[[12,[0],[],0],[29,[0xabc],[],0],[30,[7],[],0],
                     [31,[0,0,0],[],0],[32,[],[],0],[27,[],[],0]]
            yield [pokes,[12],configs,[[38,[spell,0,0]],[38,[spell,0,0]],[40,[spell]]]]
    # Failure prefixes: mutation followed by exception at each external boundary.
    for fail in range(16,35):
        pokes=setup(SK,rng,1)+[[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],
                [spell+0x10,4,123],[owner+0xc,4,0x40],[owner+0x48,4,ctable],
                [spell+0x14,4,stable],[spell+0x18,4,0],[spell+0x1c,4,0]]
        for stat,value in enumerate((100,100,100,0,1)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],
                      [S+0x24+4*stat,4,0]]
        configs=[]
        for id in range(16,35):
            result=([0,0,0] if id==31 else [7] if id==30 else
                    [spell] if id in (16,17,18,20,22,29) else [])
            configs.append([id,result,[owner+0x94,2,0x3456] if id==fail else [],int(id==fail)])
        commands=[[30,[S,owner]],[31,[S,5,owner,0,0]],[32,[spell,5,owner,S]],
                  [33,[spell,0,0,0,0]],[37,[spell,5,owner,S]],
                  [39,[spell,0x3f000000,1]],[40,[spell]],
                  [43,[spell,5,owner,S]],[44,[spell,0,0]],[45,[spell,attacker,0x42c80000]],
                  [53,[spell]]]
        yield [pokes,[],configs,commands]


def bloodlust_scenarios(SK):
    rng=random.Random(78303040)
    spell,owner,ghost,clip,definition,stable=0x6000,0x7000,0x8000,0x9000,0xa000,0xb000
    hands=(0x41200000,0xc0000000,0x40400000)*2
    def case(i):
        pokes=setup(SK,rng,i)
        slot=[0,1,M32,0x80000000][i%4]
        rec=(SK.G_PLAYERS+SK.s32(slot)*SK.REC_SIZE)&M32
        pokes += [[owner+0x3c8,4,slot],[rec+0x4a86,1,[0,1,2,3,7,4,255,128][i%8]],
                  [owner+0xc,4,[0,0x40][i%2]],[owner+0x24,4,definition],
                  [definition,4,0x1234],[owner+0x58c,4,6],
                  [owner+0x11a,2,0x20],[rec+0xc0,4,[0,0x42c60000,0xbf800000][i%3]],
                  [rec+0xc4,4,0x42c80000],
                  [spell,1,[0,1,127,128,255][i%5]],[spell+8,4,S],[spell+0xc,4,owner],
                  [spell+0x18,4,owner],[spell+0x14,4,stable],
                  [stable+8,2,[0,16,0xfff0][i%3]],
                  [spell+0x10,4,rng.getrandbits(32)],
                  [spell+0x1c,4,0x3f000000],[spell+0x20,4,0],[spell+0x24,4,0x41200000],
                  [spell+0x28,4,0],[spell+0x2c,4,1],[spell+0x30,4,0],
                  [SK.G_NETWORK_MODE,1,[0,1,255][i%3]],
                  [SK.G_SIM_FRAME,4,[0,1,2,3,4,31,511,512,1023,1024,0x7fffffff,M32][i%12]],
                  [SK.G_FORCE_GHOST_REBASE,1,0],
                  [0x44fdc0,4,[0,1,2,M32,0x80000000][i%5]],
                  [0x551b70,8,[0x401199999999999a,0xbff0000000000000,0][i%3]],
                  [0x551b78,8,[0x4053cccccccccccd,0x3ff0000000000000,0][i%3]],
                  [0x4f8498,4,0x123],[0x4f8490,4,0x456]]
        for obj in (owner,ghost):
            pokes += [[obj+0x544,4,0xabc0],[obj+0x38,2,[0,0x8000,0xffff][i%3]],
                      [obj+0xe0,4,[0,0x3f800000,0xbf800000][i%3]],
                      [obj+0x3c,4,0x40400000]]
            for k,raw in enumerate((0x3f800000,0xc0000000,0x40400000)):
                pokes.append([obj+0x10+4*k,4,raw])
        for k,raw in enumerate(hands):
            pokes.append([spell+0x34+4*k,4,raw])
        # Authored tier values; percent above 100 must remain unclamped.
        for stat,value in enumerate(([1,10,150,-30][i%4],100,150,0,200)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value&0xffff],
                      [S+0x24+4*stat,4,0],[S+0x38+2*stat,2,0x7fff]]
        results={16:[8],17:[spell if i%2 else 0],18:[spell if i%3 else 0],20:[clip],
                 35:[ghost if i%2 else owner],36:list(hands[:3]),37:[0xdead],
                 39:[i%2],40:[2 if i%2 else 0]}
        configs=[[id,results.get(id,[]),[],0] for id in (16,17,18,20,21,27,32,*range(35,43))]
        return pokes,[],configs
    # All entry points, signed rank narrowing, class styles, spell construction,
    # prediction-copy and owner hand paths, both factors and callee stores.
    for i in range(32):
        pokes,external,configs=case(i)
        rank=[0,1,127,128,255,256,M32,0x80000000][i%8]
        commands=[[54,[S,owner]],[55,[S,rank,owner,0,0x80000000,0x3f800000,0]],
                  [56,[owner]],[57,[spell,rank,owner,S]],
                  [61,[spell]],[62,[spell]],[59,[spell,0,0]],
                  [61,[spell]],[62,[spell]],
                  [64,[spell,owner,1,0x34]],
                  [65,[0xc000,[0,1,2,3,4,5,7,8][i%8],rng.getrandbits(32),rng.getrandbits(32)]]]
        yield [pokes,external,configs,commands]
    # Lifecycle branch product: active/inactive interruption, events, animation
    # flag loss, expiry equality, network-local flag and refund clamp.
    for i in range(48):
        pokes,external,configs=case(i)
        active=i%2
        pokes += [[spell+0x2c,4,active],[spell+0x28,4,(i//2)%2],
                  [owner+0x58c,4,[0,1,2,6][(i//4)%4]],
                  [owner+0x11a,2,0 if i%3==0 else 0x20],
                  [spell+0x1c,4,0],[spell+0x24,4,[0,0x3c888888,0x3e4ccccc,0x3f800000][i%4]]]
        yield [pokes,external,configs,[[61,[spell]],[62,[spell]],
                                     [59,[spell,0,0]],[59,[spell,0,0]],[61,[spell]],[62,[spell]]]]
    # Glow's trail thresholds, fourth-frame gating, drip path, both fade ramps,
    # negative fade and separate private RNG. Larger displacements are authored.
    for i in range(40):
        pokes,external,configs=case(i)
        delta=[0,0x3f800000,0x40000000,0x40a00000,0x41200000,0x41300000][i%6]
        h=(delta,0,0)*2
        pokes += [[spell+0x1c,4,[0,0x3dcccccc,0x3e4ccccc,0x3f000000,0xbf800000][i%5]],
                  [spell+0x24,4,[0x3f800000,0x3f0ccccd,0][i%3]],
                  [owner+0x58c,4,[0,1,2,6][i%4]],[0x44fdc0,4,1]]
        for k in range(6): pokes.append([spell+0x34+4*k,4,0])
        yield [pokes,external,configs,[[60,[spell,*h]],[58,[spell,0x8e7140,*h,0x34,0x40]]]]
    # General shout helper supports arbitrary style keys and multiple events:
    # masks for every event precede every time word. Empty lists are valid.
    for i in range(8):
        pokes,external,configs=case(i)
        names=[M32,0x1234,128,0x5678,0,0x9abc]
        events=[] if i%2 else [0,0x3f800000,0x102,2,0x80000000,0xff]
        yield [pokes,external,configs,[[63,[owner,0xface,0x22,3,*names,len(events)//3,*events]],
                                     [64,[spell,owner,0,0]]]]
    # Mutation then failure at every service Blood Lust reaches. Running
    # independent operations ensures later service failures are actually reached.
    for fail in (16,17,18,20,21,27,32,*range(35,43)):
        pokes,external,configs=case(1)
        pokes += [[spell+0x1c,4,0],[spell+0x24,4,0],[spell+0x28,4,0]]
        for config in configs:
            if config[0]==fail: config[2:]=[[owner+0x94,2,0x3456],1]
        yield [pokes,external,configs,[[54,[S,owner]],[55,[S,5,owner,0,0,0,0]],
             [57,[spell,5,owner,S]],[56,[owner]],
             [63,[owner,0xface,0x22,0,1,1,0x3f000000,2]],
             [64,[spell,owner,1,0x34]],[60,[spell,*hands]],
             [58,[spell,0x8e7140,*hands,0x34,0x40]],[59,[spell,0,0]]]]


def shared_classes_scenarios(SK):
    rng=random.Random(828386)
    spell,owner,other,attacker,ctable=0x6000,0x7000,0x7100,0x8000,0x9000
    def case(i):
        pokes=setup(SK,rng,i)
        for stat,value in enumerate((100,[0,1,2,-1][i%4],150,0,30)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value&0xffff],
                      [S+0x24+4*stat,4,0],[S+0x38+2*stat,2,0xffff]]
        pokes += [[SK.G_RAND_SEED,4,12345],[0x404460,4,rng.getrandbits(32)],
                  [spell,1,[0,1,5,127,128,255][i%6]],[spell+8,4,S],[spell+0xc,4,owner],
                  [spell+0x10,4,rng.getrandbits(32)],[spell+0x14,4,0xa000],
                  [0xa008,2,0xfff0],[spell+0x18,4,[0,0x3e050ef8,0x3f90a1df,0xbf800000][i%4]],
                  [spell+0x1c,4,0x40000000],[spell+0x20,4,99],
                  [SK.G_ALIVE_COUNT,4,3],[SK.G_ALIVE_LIST,4,0],[SK.G_ALIVE_LIST+4,4,1],
                  [SK.G_ALIVE_LIST+8,4,2],[SK.G_PVP_MODE_A,4,i%2],
                  [SK.G_WORLD,4,0xabcdef],[SK.G_SIM_FRAME,4,[0,1,0x80000000,M32][i%4]]]
        for j,obj in enumerate((owner,other,attacker)):
            rec=SK.G_PLAYERS+j*SK.REC_SIZE
            pokes += [[obj+0xc,4,[0x40,0x20,0x28,0][i%4]],[obj+0x48,4,ctable],
                      [ctable+128,2,0xfff0],[ctable+168,2,16],[ctable+176,2,0xfff0],
                      [obj+0x3c8,4,j],[rec+0x43d8,4,obj],
                      [rec+0xac,4,[0,0x42c80000,0x42c70000,0xbf800000][(i+j)%4]],
                      [rec+0xb0,4,0x42c80000],[obj+0x228,4,0x42c80000],[obj+0x22c,4,0x42c80000],
                      [obj+0x38,2,0xffff],[obj+0xdc,4,0x40000000]]
            for k,v in enumerate(([0,0,0],[0x3f800000,0x40000000,0x42c80000],[0x42c80000,0,0])[j]):
                pokes.append([obj+0x10+4*k,4,v])
        configs=[[18,[spell],[],0],[20,[77],[],0],[22,[99],[],0],[23,[],[],0],
                 [25,[],[],0],[27,[],[],0],[29,[42],[],0],[30,[[0,7,-7][i%3]&M32],[],0],
                 [31,[0x3f800000,0x40000000,0x40400000],[],0],[32,[],[],0],[33,[],[],0],
                 [35,[owner],[],0],[43,[],[],0],[44,[],[],0],[45,[owner,other,attacker,owner],[],0],
                 [46,[i%2],[],0],[47,[i%2],[],0],[48,[],[],0],
                 [49,[[0,0x42900000,0x42920000,0xc2900000,0xc2920000][i%5]],[],0]]
        return pokes,[],configs
    # Every entry/helper, signed narrowing, player/nonplayer construction and null allocation.
    for i in range(24):
        pokes,external,configs=case(i)
        if i%3==0: configs[0][1]=[0]
        rank=[0,1,127,128,255,256,M32,0x80000000][i%8]
        yield [pokes,external,configs,[[66,[S,rank,owner,0,0]], [68,[S,rank]],[69,[S,rank]],
            [72,[0x42c80000,rank]],[73,[spell,owner]], [70,[S,rank,owner,0,0]],
            [71,[spell,rank,owner,S]],[74,[spell,0,0]]]]
    # Run branches: reach sign, HP eligibility, 2-D vs 3-D, PvP teams, first flash and expiry.
    for i in range(32):
        pokes,external,configs=case(i)
        yield [pokes,external,configs,[[74,[spell,0,0]],[74,[spell,0,0]]]]
    # Riposte: rank-zero, chance failure, zero/negative/positive sample counts,
    # all 200 cosmetic steps and a second shared draw only after completion.
    for i in range(8):
        pokes,external,configs=case(i)
        external=[12]
        configs += [[12,[[0,100,150,-1][i%4]&M32],[],0]]
        yield [pokes,external,configs,[[67,[S,owner,attacker,0 if i==0 else M32]]]]
    # Failure with mutation at each delegated boundary. Independent entry points
    # reach services even when an earlier operation fails.
    for fail in (18,20,22,23,25,27,29,30,31,32,33,35,43,44,45,46,47,48,49):
        pokes,external,configs=case(1)
        # Live nonplayer Healing uses ally; an independent player run uses team.
        pokes += [[owner+0xc,4,0x20],[spell,1,5],[spell+0x18,4,0x3f90a1df]]
        external=[12]; configs += [[12,[100],[],0]]
        for cfg in configs:
            if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],1]
        yield [pokes,external,configs,[[67,[S,owner,attacker,1]],[70,[S,5,owner,0,0]],
            [71,[spell,5,owner,S]],[74,[spell,0,0]]]]
    # Team refusal must reach the player path, plus above-max flash without HP store.
    for i in range(4):
        pokes,external,configs=case(0)
        pokes += [[SK.G_PVP_MODE_A,4,1],[spell,1,5]]
        for cfg in configs:
            if cfg[0]==47: cfg[1:]=[[1],[owner+0x94,2,0x3456],i%2]
        yield [pokes,external,configs,[[74,[spell,0,0]]]]


def summon_items_scenarios(SK):
    rng=random.Random(7275)
    spell,owner,ghost,definition,ctable=0x6000,0x7000,0x7100,0x8000,0x9000
    def case(i):
        pokes=setup(SK,rng,i)
        for stat,value in enumerate((70000,0,0,0,30)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value&0xffff],
                      [S+0x24+4*stat,4,0],[S+0x38+2*stat,2,0xffff]]
        pokes += [[SK.G_SIM_FRAME,4,15],[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],
                  [spell+0x10,4,rng.getrandbits(32)],[spell+0x14,4,0xa000],
                  [0xa008,2,0xfff0],[spell+0x18,4,0],[spell+0x1c,4,99],
                  [owner+0xc,4,0x20 if i%2 else 0x40],[owner+0x3c8,4,0],
                  [owner+0x48,4,ctable],[ctable+128,2,0xfff0],[owner+0x38,2,0xffff],
                  [owner+0xdc,4,0x40000000],[definition+0x1d,1,[0,1,127,128,255][i%5]],
                  [SK.G_PLAYERS+0x120,4,0x41200000],[SK.G_PLAYERS+0x124,4,[0x41a00000,0x41a80000,0x41100000][i%3]],
                  [SK.G_PLAYERS+0xc0,4,[0x42800000,0x42c80000,0xbf800000][i%3]],
                  [SK.G_PLAYERS+0xc4,4,0x42c80000],[ghost+0xe0,4,0x40000000],[ghost+0x38,2,0x8000]]
        for obj in (owner,ghost):
            for k,v in enumerate((0x3f800000,0x80000000,0x40000000)):
                pokes += [[obj+0x10+4*k,4,v]]
            pokes += [[obj+0x3c,4,0x40400000]]
        configs=[[18,[spell],[],0],[20,[77],[],0],[21,[],[],0],[22,[99],[],0],
                 [23,[],[],0],[25,[],[],0],[27,[],[],0],[28,[],[],0],[29,[42],[],0],
                 [30,[[0,7,-7][i%3]&M32],[],0],[31,[0x3f800000,0x40000000,0x40400000],[],0],
                 [32,[],[],0],[35,[ghost],[],0],[48,[],[],0],[50,[definition],[],0],
                 [51,[0x41200000],[],0],[52,[[0,1,-1,0x80000000][i%4]&M32],[],0],
                 [53,[],[],0],[54,[],[],0],[55,[],[],0],[56,[],[],0],[57,[],[],0]]
        return pokes,[],configs
    # Full wrapper and shared helper coverage; capacities equal/below/above,
    # nonplayer flags, signed rank/quantity and signed bag/index narrowing.
    for i in range(30):
        pokes,external,configs=case(i)
        rank=[0,1,4,5,8,9,12,13,16,17,20,21,127,128,255,256,M32][i%17]
        external=[10,5]
        configs += [[10,[[0,1,65535,65536,70000,M32][i%6]],[],0],[5,[0x42100000],[],0]]
        yield [pokes,external,configs,[[75,[S,owner]], [79,[rank,M32,i%2]],
            [80,[i%2,spell,rank,owner,S]],[81+i%2,[spell,0,0]]]]
    # Cast allocation and light path, positions with negative zero, both configs.
    for i in range(12):
        pokes,external,configs=case(i)
        for cfg in configs:
            if cfg[0]==18: cfg[1]=[[0,spell][i%2]]
        rank=[1,128,255,256,M32,5][i%6]
        pos=[0x80000000,0x3f800000,0x40000000]
        yield [pokes,external,configs,[[76+i%2,[S,rank,owner,*pos,0]],
            [78,[i%2,S,rank,owner,*pos]]]]
    # Independently execute generic run and wrappers at age/deadline boundaries,
    # frame wrapping, zero/negative sample counts and expired particle groups.
    for i,age in enumerate((-1,0,1,7,15,29,30,31,120,121,0x7fffffff,-2147483648)):
        for arrows in (0,1):
            pokes,external,configs=case(i)
            pokes += [[SK.G_SIM_FRAME,4,age&M32]]
            yield [pokes,external,configs,[[83,[arrows,spell]],[81+arrows,[spell,0,0]]]]
    for arrows in (0,1):
        pokes,external,configs=case(1)
        pokes += [[spell+0x18,4,M32]]
        yield [pokes,external,configs,[[83,[arrows,spell]]]]
    # Item canonicalization preserves arbitrary slot lengths (including empty
    # slots and more than four), not just the summoning caller's fixed layout.
    for slots in ((),((),),((M32,),),((1,2,3,4),),
                  ((M32,65536,32768,1),(2,),(3,4),()),
                  ((1,),(2,),(3,),(4,),(5,6,7,8,9))):
        pokes,external,configs=case(0)
        args=[M32,M32,len(slots),*[v for slot in slots for v in (len(slot),*slot)]]
        yield [pokes,external,configs,[[84,args]]]
    # Failure/mutation prefixes at every delegated boundary, in both configs.
    # Separate operations reach construction, successful inventory, refund and
    # particle paths even if the first operation raises.
    for arrows in (0,1):
        for fail in (5,10,14,18,20,21,22,23,25,27,28,29,30,31,32,35,48,50,51,52,53,54,55,56,57):
            pokes,external,configs=case(1)
            external=[5,10,14]
            configs += [[5,[0x42100000],[],0],[10,[10],[],0],[14,[5],[],0]]
            for cfg in configs:
                if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],1]
            # Weight-service mutations enter the refund path for PlayerVoice failure.
            if fail==56:
                for cfg in configs:
                    if cfg[0]==51: cfg[2]=[SK.G_PLAYERS+0x124,4,0]
            if fail==57: pokes += [[SK.G_SIM_FRAME,4,121]]
            yield [pokes,external,configs,[[76+arrows,[S,5,owner,0,0,0,0]],
                [80,[arrows,spell,5,owner,S]],[83,[arrows,spell]]]]
    # Run-only failure cases keep a positive particle age; construction resets
    # the start frame, so chaining a constructor cannot reach sample/spawn failures.
    for arrows in (0,1):
        for fail in (23,25,27,29,30,31,32,35,48,57):
            pokes,external,configs=case(1)
            if fail==57: pokes += [[spell+0x18,4,M32]]
            for cfg in configs:
                if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],1]
            yield [pokes,external,configs,[[83,[arrows,spell]]]]
    # Refund mutations: force overweight, change the slot during voice, and
    # change mana during the cost callback to prove evaluation/store order.
    for arrows in (0,1):
        for i in range(4):
            pokes,external,configs=case(0)
            pokes += [[SK.G_PLAYERS+0x124,4,0],
                      [SK.G_PLAYERS+SK.REC_SIZE+0xc0,4,0x42800000],
                      [SK.G_PLAYERS+SK.REC_SIZE+0xc4,4,0x42c80000]]
            external=[5]; configs += [[5,[0x42100000],[SK.G_PLAYERS+(i%2)*SK.REC_SIZE+0xc0,4,0],i//2]]
            for cfg in configs:
                if cfg[0]==56: cfg[2]=[owner+0x3c8,4,i%2]
            yield [pokes,external,configs,[[80,[arrows,spell,5,owner,S]],[83,[arrows,spell]]]]


def criticalhit_scenarios(SK):
    rng=random.Random(335381)
    spell,owner,other,ghost,ctable=0x6000,0x7000,0x7100,0x8000,0x9000
    def case(i):
        pokes=setup(SK,rng,i)
        for stat,value in enumerate((150,6,8,0,30)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],
                      [S+0x24+4*stat,4,0],[S+0x38+2*stat,2,0xffff]]
        pokes += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],
                  [spell+0x10,4,rng.getrandbits(32)],[spell+0x14,4,0x519c28],
                  [0x519c30,2,0xfff0],[spell+0x18,4,0x41000000],[spell+0x58,4,99],
                  [S+0x64,4,[33,53,81,999][i%4]],
                  [SK.G_ALIVE_COUNT,4,3],[SK.G_ALIVE_LIST,4,0],
                  [SK.G_ALIVE_LIST+4,4,1],[SK.G_ALIVE_LIST+8,4,2],
                  [SK.G_PVP_MODE_A,4,i%2],[SK.G_SIM_FRAME,4,[0,1,0x80000000,M32][i%4]],
                  [SK.G_WORLD,4,0xabcdef]]
        for j,obj in enumerate((owner,other,ghost)):
            pokes += [[obj+0x3c8,4,j],[SK.G_PLAYERS+j*SK.REC_SIZE+0x43d8,4,obj],
                      [obj+0x48,4,ctable],[ctable+176,2,0xfff0]]
            for k,v in enumerate(([0,0,0],[0x3f800000,0x40000000,0x42c80000],[0x447a0000,0,0])[j]):
                pokes += [[obj+0x10+4*k,4,v]]
        results={16:[77],17:[spell],18:[spell],22:[99],35:[ghost],47:[1],49:[0],59:[5]}
        configs=[[id,results.get(id,[]),[],0] for id in (16,17,18,19,22,26,27,23,25,32,35,47,48,49,58,59)]
        return pokes,[],configs
    # Every body, direct rank without adjustment, narrowing, allocation and type lookup.
    for i,rank in enumerate((0,1,5,127,128,255,256,M32,0x80000000)):
        pokes,external,configs=case(i)
        for cfg in configs:
            if cfg[0] in (17,18): cfg[1]=[spell if i%2 else 0]
        yield [pokes,external,configs,[[85,[S,owner]],[86,[S,rank,owner,0,0]],
            [87,[S,rank]],[88,[spell,rank,owner,S]],[89,[spell,other]],
            [90,[spell,0,0]],[91,[S,owner,rank]]]]
    # Lifecycle light ages: <=1, fading (1,2], >2, expiry and signed levels.
    for i,left in enumerate((0,0x3c888888,0x41000000,0x40e00000,0x40c00000,
                            0x40bfffff,0x40c00001,0xbf800000,0x80000000)):
        for ident in (33,53,81,999):
            pokes,external,configs=case(i)
            pokes += [[spell+0x18,4,left],[S+0x64,4,ident],
                      [spell,1,[0,1,5,127,128,255][i%6]]]
            yield [pokes,external,configs,[[90,[spell,0,0]],[90,[spell,0,0]]]]
    # Strict planar gates even for nonpositive reach, wrapped integer square,
    # ghost terrain drop endpoints and alive-list count / null slot behavior.
    for i,radius in enumerate((0,1,-1,6,0x7fffffff,-2147483648)):
        for height in (0x42900000,0xc2900000,0x42920000,0xc2920000):
            pokes,external,configs=case(i)
            pokes += [[SK.G_ALIVE_COUNT,4,[0,1,3,M32][i%4]]]
            external=[10]; configs += [[10,[radius&M32],[],0]]
            for cfg in configs:
                if cfg[0]==49: cfg[1]=[height]
            yield [pokes,external,configs,[[90,[spell,0,0]]]]
    # Bonus query: own spell and rank, absent spell, aura, negative best,
    # PvP acceptance, 3D 1000 boundary and strict planar radius boundary.
    for i in range(24):
        pokes,external,configs=case(i)
        rank=[0,1,M32,0x80000000][i%4]
        distance=[0,0x428e0000,0x42900000,0x42920000,0x4479c000,0x447a0000][i%6]
        pokes += [[other+0x10,4,distance],[other+0x14,4,0],[other+0x18,4,0],
                  [ghost+0x10,4,0x44800000]]
        for cfg in configs:
            if cfg[0]==17: cfg[1]=[spell if i%3 else 0]
            if cfg[0]==47: cfg[1]=[i%2]
            if cfg[0]==59: cfg[1]=[[0,1,5,M32,0x80000000][i%5]]
        yield [pokes,external,configs,[[91,[S,owner,rank]]]]
    # Callback side effects and failure prefixes at each reachable service and
    # delegated Skill slot. Independent operations reach both relink and unlink.
    for fail in (10,14,15,16,17,18,19,22,23,25,26,27,32,35,47,48,49,58,59):
        pokes,external,configs=case(1)
        external=[10,14,15]
        configs += [[10,[6],[],0],[14,[5],[],0],[15,[0x42100000],[],0]]
        pokes += [[spell+0x18,4,0x40000000]]
        for cfg in configs:
            if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],1]
        yield [pokes,external,configs,[[85,[S,owner]],[86,[S,5,owner,0,0]],
            [87,[S,5]],[88,[spell,5,owner,S]],[90,[spell,0,0]],[91,[S,owner,0]]]]
        pokes,external,configs=case(1)
        external=[10,14]; configs += [[10,[8],[],0],[14,[5],[],0]]
        pokes += [[spell+0x18,4,0x40000000]]
        for cfg in configs:
            if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],1]
        yield [pokes,external,configs,[[90,[spell,0,0]],[91,[S,owner,0]]]]
    # Successful callback mutations prove terrain reads happen after its return
    # and bonus queries re-fetch the player object after rank/radius services.
    for i in range(4):
        pokes,external,configs=case(0)
        for cfg in configs:
            if cfg[0]==49: cfg[2]=[ghost+0x18,4,[0,0x42900000,0x42920000,0xc2920000][i]]
            if cfg[0]==59: cfg[2]=[SK.G_PLAYERS+SK.REC_SIZE+0x43d8,4,ghost]
        yield [pokes,external,configs,[[90,[spell,0,0]],[91,[S,owner,0]]]]
    # General light record preserves arbitrary halfwords and explicit falloff.
    for i in range(6):
        pokes,external,configs=case(i)
        yield [pokes,external,configs,[[92,[0xa000,M32,65536,32768,1,
            [0,0x80000000,0xbf800000,0x7fffffff,0x3f800000,0x42f00000][i],
            0x12345678,0x80000000,0x3f800000,0x80000000,0xbf800000]]]]


def thrown_axes_scenarios(SK):
    rng=random.Random(9899)
    owner,target,handle,main=0x7000,0x8000,0x9000,0xa000
    rec=SK.G_PLAYERS
    stack=rec+0x130+3*0x48
    def case(i):
        pokes=setup(SK,rng,i)
        for stat,value in enumerate((80,17,0,0,30)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],
                      [S+0x24+4*stat,4,0]]
        pokes += [[owner+0xc,4,0x40],[owner+0x3c8,4,0],[owner+0x544,4,0xabcd],
                  [owner+0x38,2,[0,32767,32768,65535][i%4]],
                  [owner+0x234,4,handle if i%2 else 0],[handle+0xc,4,target],
                  [rec+0x43f8,4,stack],[rec+0x4434,4,main],[rec+0x4a7c,1,0],
                  [rec+0xd0,4,M32-10],[rec+0xd4,4,27],
                  [stack+4,2,1],[stack+8,8,1<<42],
                  [SK.ThrownAxes.G_ITEM_MODELS+8,4,0x1111],
                  [SK.ThrownAxes.G_ITEM_MODELS+12,4,0x2222]]
        for obj,pos in ((owner,(0x80000000,0x3f800000,0x40000000)),
                        (target,(0x42c80000,0xc3480000,0x43480000))):
            pokes += [[obj+0x10+4*k,4,v] for k,v in enumerate(pos)]
        for k in range(4): pokes += [[S+0x68+0x24+8*k,2,0xffff]]
        results={18:[0xb000],20:[77],36:[0x3f800000,0xc0000000,0x40000000,0,0,0],60:[0xc000],62:[0],65:[3]}
        configs=[[id,results.get(id,[]),[],0] for id in (18,20,21,28,36,60,61,62,63,64,65,66)]
        return pokes,[],configs
    # Every family method, player/nonplayer, item-null and exact bit-42 gating.
    for i in range(12):
        pokes,external,configs=case(i)
        pokes += [[owner+0xc,4,[0,0x40,0x20040][i%3]],
                  [rec+0x43f8,4,0 if i%4==0 else stack],[stack+8,8,[0,1<<41,1<<42,1<<43][i%4]]]
        yield [pokes,external,configs,[[93,[S,owner]],[94,[S,owner]],[95,[i%2,owner]]]]
    # Both direct casts and the two configurations of their shared body; signed
    # rank narrowing is delegated to adjustment, not imposed by the cast.
    for i,rank in enumerate((0,1,2,20,127,128,255,256,M32,0x80000000)):
        pokes,external,configs=case(i)
        yield [pokes,external,configs,[[96+i%2,[S,rank,owner,0x80000000,0x3f800000,0x40000000,M32]],
                                     [98,[i%2,S,rank,owner,0x3f800000,0x80000000,0]]]]
    # Low-ammunition endpoints, signed quantity removal, prediction copies,
    # and main-weapon absence (after both resource queries and heading draws).
    for count in (0,1,2,20,21,32767,32768,65535):
        for flags in (0x40,0x20040):
            pokes,external,configs=case(count)
            pokes += [[stack+4,2,count],[owner+0xc,4,flags]]
            yield [pokes,external,configs,[[96,[S,5,owner,0,0,0,0]]]]
    for i in range(6):
        pokes,external,configs=case(i)
        pokes += [[rec+0x4a7c,1,[0,1,127,128,255,2][i]],
                  [rec+0x4434+4*SK.s8([0,1,127,128,255,2][i]),4,0 if i%2 else main]]
        for cfg in configs:
            if cfg[0]==18: cfg[1]=[0 if i%3==0 else 0xb000]
            if cfg[0]==62: cfg[1]=[[0,1,M32,M32-1][i%4]]
        yield [pokes,external,configs,[[97,[S,5,owner,0,0,0,0]]]]
    # All modifier positions, already matching slots, full slots, narrowing and
    # signed stat extrema. Empty slots have priority over later same elements.
    for cold in (0,1):
        for k in range(6):
            pokes,external,configs=case(k)
            for j in range(4):
                kind=(0xffff if k==j else 0x15+cold if k==4 and j==3 else 7)
                pokes += [[S+0x68+0x24+8*j,2,kind]]
            external=[10]; configs += [[10,[[0,1,65535,65536,M32,0x80000000][k]],[],0]]
            yield [pokes,external,configs,[[96+cold,[S,5,owner,0,0,0,0]]]]
    # Throw/failure prefixes at every delegated service and both Skill slots.
    for cold in (0,1):
        for fail in (10,14,18,20,21,28,36,60,61,62,63,64,65,66):
            pokes,external,configs=case(0)
            external=[10,14]; configs += [[10,[17],[],0],[14,[5],[],0]]
            for cfg in configs:
                if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],1]
            yield [pokes,external,configs,[[93+cold,[S,owner]],
                [96+cold,[S,5,owner,0,0,0,0]]]]
            # The warning path and removal are otherwise bypassed above.
            pokes += [[stack+8,8,0]]
            yield [pokes,external,configs,[[93+cold,[S,owner]]]]
    # Successful callback mutations lock down re-reads: slot changes after copy,
    # statistics and projectile; count after notice; equipment after resource query.
    for service,address,width,value in (
            (63,owner+0x3c8,4,1),(10,owner+0x3c8,4,1),(64,owner+0x3c8,4,1),
            (65,stack+4,2,7),(62,rec+0x4434,4,0),
            (36,owner+0x234,4,handle),(36,owner+0x38,2,0x8000),
            (63,rec+0xd0,4,0x80000000),(64,rec+0x43f8,4,0),
            (65,owner+0x3c8,4,2)):
        pokes,external,configs=case(0)
        if service==10: external=[10]; configs += [[10,[17],[],0]]
        for cfg in configs:
            if cfg[0]==service: cfg[2]=[address,width,value]
        yield [pokes,external,configs,[[96,[S,5,owner,0,0,0,0]]]]


def battlecry_scenarios(SK):
    rng=random.Random(77301464)
    spell,owner,other,ghost,enemy,table=0x6000,0x7000,0x7100,0x8000,0x7200,0x9000
    def case(i):
        pokes=setup(SK,rng,i)
        for stat,value in enumerate((101,20,8,6,30)):
            pokes += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],
                      [S+0x24+4*stat,4,0],[S+0x38+2*stat,2,0xffff]]
        pokes += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],
                  [spell+0x10,4,rng.getrandbits(32)],[spell+0x14,4,SK.BattleCry.BC_VT],
                  [SK.BattleCry.BC_VT+8,2,0xfff0],[spell+0x18,4,owner],
                  [spell+0x20,4,0x41000000],[spell+0x24,4,0xbf800000],
                  [spell+0x28,4,0xbf800000],[spell+0x2c,4,77],[spell+0x3c,4,99],
                  [owner+0x58c,4,6],[owner+0x11a,2,0x20],
                  [owner+0x24,4,0x9500],[0x9500,4,0xabcd],[owner+0x544,4,0xcafe],
                  [owner+0x38,2,0x8001],[SK.G_WORLD,4,0xabcdef],
                  [SK.G_NETWORK_MODE,1,i%2],[SK.G_SIM_FRAME,4,[0,1,0x80000000,M32][i%4]],
                  [SK.G_PVP_MODE_A,4,i%2],[SK.G_ALIVE_COUNT,4,4],
                  [SK.G_ALIVE_LIST,4,0],[SK.G_ALIVE_LIST+4,4,1],
                  [SK.G_ALIVE_LIST+8,4,2],[SK.G_ALIVE_LIST+12,4,3],
                  [enemy+0xc,4,0x20],[enemy+0x1c,4,0x3f800000],[enemy+0x228,4,0]]
        for j,obj in enumerate((owner,other,ghost,enemy)):
            pokes += [[obj+0x3c8,4,j],[SK.G_PLAYERS+j*SK.REC_SIZE+0x43d8,4,obj],
                      [obj+0x48,4,table],[table+160,2,0xfff0],[table+176,2,0xfff8],
                      [table+240,2,0xfff4]]
            for k,v in enumerate(([0,0,0],[0x3f800000,0,0],[0x447a0000,0,0],[0,0,0])[j]):
                pokes += [[obj+0x10+4*k,4,v]]
        results={16:[77],17:[0],18:[0],20:[0x9800],21:[123],22:[456],35:[ghost],
                 36:[0x3f800000,0x40000000,0x40400000],37:[0x9600],39:[1],40:[8],
                 45:[enemy,other,enemy],47:[1],49:[0],67:[1]}
        ids=(16,17,18,20,21,22,23,25,27,28,32,35,36,37,38,39,40,42,45,47,48,49,67,68)
        configs=[[id,results.get(id,[]),[],0] for id in ids]
        return pokes,[],configs
    # All seven bodies, signed rank and allocation paths; attach helper overrides.
    for i,rank in enumerate((0,1,5,127,128,255,256,M32,0x80000000)):
        p,e,c=case(i)
        p += [[SK.G_PLAYERS+0x4a86,1,[0,1,2,3,7,128,255,4,0][i]],
              [owner+0xc,4,0x40 if i%2 else 0x20]]
        for cfg in c:
            if cfg[0] in (17,18): cfg[1]=[spell if i%2 else 0]
        yield [p,e,c,[[99,[S,owner]],[100,[S,rank,owner,0,0]],
                      [101,[spell,rank,owner,S]],[102,[spell,0,0]],
                      [102,[spell,1,0]],[103,[spell,other]],
                      [105,[spell,owner,0]],[106,[owner,1,0,0]],
                      [106,[owner,M32,1,0]]]]
    # Full event paths, animation flag removal, interruption before/after cry,
    # expiry exact boundaries, network gating and streamer activation order.
    for event in (0,2,4,8,M32):
        for mode in range(8):
            p,e,c=case(mode)
            p += [[spell+0x30,4,1],[spell+0x34,4,mode%2],
                  [owner+0x58c,4,2 if mode in (2,3) else 6],
                  [owner+0x11a,2,0 if mode==4 else 0x20],
                  [spell+0x24,4,0 if mode==3 else 0xbf800000],
                  [spell+0x20,4,0 if mode==5 else 0x3c888888 if mode==6 else 0x41000000],
                  [SK.G_PLAYERS+0xc0,4,0x41a00000],[SK.G_PLAYERS+0xc4,4,0x41f00000]]
            for cfg in c:
                if cfg[0]==40: cfg[1]=[event]
            yield [p,e,c,[[104,[spell,0,0,0,M32]],[104,[spell,0,0,0,0]]]]
    # Aura growth clamp, min radius, signed particle counts, terrain endpoints.
    for rank in (0,5,127,128,243,255):
        for clock in (0,0x3f000000,0x3f800000):
            for terrain in (0x42900000,0xc2900000,0x42900001,0xc2900001):
                p,e,c=case(rank)
                p += [[spell,1,rank],[spell+0x24,4,0],[spell+0x1c,4,clock]]
                for cfg in c:
                    if cfg[0]==49: cfg[1]=[terrain]
                yield [p,e,c,[[104,[spell,0,0,0,0]]]]
    # Aura query always computes fraction, own owner is covered before the cry,
    # distinct aura-position owner, 3D inclusive radius and ignored output flag.
    for target in (0,owner,other):
        for distance in (0,0x42100000,0x42100001,0x447a0000):
            for cry in (0xbf800000,0):
                p,e,c=case(1)
                p += [[spell+0x28,4,0x42100000],[spell+0x24,4,cry],
                      [other+0x18,4,distance]]
                yield [p,e,c,[[105,[spell,target,0]],[105,[spell,target,1]]]]
    # First cry stun filters, duplicate query ordering, allocation success,
    # negative HP, noncreature, radius and enemy/team rejection.
    for mode in range(12):
        p,e,c=case(mode)
        p += [[spell+0x30,4,1],[enemy+0x228,4,0xbf800000 if mode==1 else 0],
              [enemy+0xc,4,0 if mode==2 else 0x20],
              [enemy+0x18,4,[0,0x42920000,0x42920001][mode%3]],
              [spell,1,243 if mode==3 else 5]]
        for cfg in c:
            if cfg[0]==18: cfg[1]=[0xa000 if mode%2 else 0]
            if cfg[0] in (47,67): cfg[1]=[0 if mode==4 else 1]
        e=[10]; c += [[10,[[0,1,100,101,M32][mode%5]],[],0]]
        yield [p,e,c,[[104,[spell,0,0,0,0]]]]
    # Failure and successful mutation prefixes across every delegated boundary.
    for fail in (5,10,14,16,17,18,20,21,22,23,25,27,28,32,35,36,37,38,39,40,42,45,47,48,49,67,68):
        for throwing in (0,1):
            p,e,c=case(1)
            p += [[spell+0x30,4,1],[spell+0x34,4,1]]
            e=[5,10,14]; c += [[5,[0x42100000],[],0],[10,[101],[],0],[14,[5],[],0]]
            for cfg in c:
                if cfg[0]==fail: cfg[2:]=[[owner+0x94,2,0x3456],throwing]
            yield [p,e,c,[[99,[S,owner]],[101,[spell,5,owner,S]],
                          [102,[spell,1,0x8e6f60]],[104,[spell,0,0,0,0]],
                          [105,[spell,other,1]]]]
            # Independent expiry and interrupted branches reach destroy/refund.
            p,e,c=case(1); e=[5,10,14]
            p += [[spell+0x30,4,1],[owner+0x58c,4,2],[spell+0x20,4,0],
                  [spell+0x24,4,0xbf800000]]
            c += [[5,[0x42100000],[],0],[10,[101],[],0],[14,[5],[],0]]
            for cfg in c:
                if cfg[0]==fail: cfg[2:]=[[spell+0x20,4,0],throwing]
            yield [p,e,c,[[104,[spell,0,0,0,0]],[100,[S,5,owner,0,0]]]]


def storm_scenarios(SK):
    rng=random.Random(8031)
    obj,owner,other,table,node=0x6000,0x7000,0x7100,0x9000,0xa000
    def case(pus, mode=0):
        cfg=SK.Storms.STORM_PS if pus else SK.Storms.STORM_LS
        p=setup(SK,rng,mode)
        for stat,value in enumerate((4,21,3,7,9)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[obj+0x48,4,cfg['vt']],[cfg['vt']+8,2,0xfff0],
              [obj+0xc,4,0x200],[obj+0x44,4,rng.getrandbits(32)],
              [obj+cfg['handle'],4,node],[node+0xc,4,owner],[owner+0xc,4,0x40],
              [owner+0x48,4,table],[table+160,2,0xfff8],
              [obj+0x58,4,10],[obj+0x5c,4,10],[obj+0x60,4,200],[obj+0x64,4,250],
              [obj+0x68,4,17],[obj+0x6c,4,M32],[obj+0x70,4,21],[obj+0x74,4,45],
              [obj+0x78,4,0x42400000],[obj+0x7c,4,0x4243f780] if pus else [obj+0x28,1,255],
              [SK.G_WORLD,4,0xabcdef],[SK.G_SIM_FRAME,4,10],
              [SK.G_HANDLE_FREE,4,0xb000],[0xb000+4,4,0xb020],[0xb020+4,4,0xb040]]
        for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):
            p += [[obj+0x10+4*k,4,v],[obj+0x4c+4*k,4,(0x3e800000,0xbe000000,0)[k]]]
        results={18:[0],20:[0x9800],21:[123],45:[other,other,0],67:[1],70:[0x40800000]}
        ids=(18,19,20,21,26,28,33,45,67,69,70,71,72,73,74)
        c=[[i,results.get(i,[]),[],0] for i in ids]
        return p,[],c
    def allocation(c,value):
        for v in c:
            if v[0]==18:v[1]=[value]
    # Every entry/body; allocation, caster-kind, rank and facing endpoints.
    for pus in (0,1):
        for i,rank in enumerate((0,1,20,39,40,41,42,127,128,255,256,M32,0x80000000)):
            p,e,c=case(pus,i)
            p += [[owner+0xc,4,(0,0x20,0x40,0x60)[i%4]]]
            allocation(c,0x8000 if i%2 else 0)
            pos=[0x3f800000,0x80000000,0x40a00000]; facing=(0,0x4000,0x8000,0xffff,M32)[i%5]
            yield [p,e,c,[[107+pus,[S,rank,owner,*pos,facing]],
                          [109,[pus,S,rank,owner,*pos,facing]]]]
        for i in range(24):
            p,e,c=case(pus,i); allocation(c,0xc000 if i%3==0 else 0)
            p += [[owner+0xc,4,(0x20,0x40,0x60,0)[i%4]],[SK.G_SIM_FRAME,4,(0,1,M32,0x7fffffff,0x80000000)[i%5]]]
            frames=(0,1,60,M32,0x80000000)[i%5]; interval=(0,1,2,M32,0xfffffffb,0x80000000)[i%6]
            steps=(0,0x3f800000,0xbf800000,0x43480000,0x80000000,0x7fffffff)[i%6]
            yield [p,e,c,[[110,[pus,obj,0x3f800000,0xc0000000,0x40400000,
                              0x40e00000,0xc0800000,0x80000000,frames,interval,
                              (0,1,3,M32,0x80000000)[i%5],0x7fffffff,owner,steps]]]]
        # Tick boundaries, disappearance, zero/negative scheduling, RNG seeds,
        # signed odd damage and integer extremes; with and without decoration.
        for i in range(72):
            p,e,c=case(pus,i)
            frame=(9,10,11,39,40,41,199,200,201,250,251,M32)[i%12]
            interval=(0,1,2,17,M32,0xfffffffd,0x80000000)[i%7]
            p += [[SK.G_SIM_FRAME,4,frame],[obj+0x68,4,interval],
                  [obj+0x6c,4,(M32,10,200)[i%3]],
                  [obj+0x70,4,(1,3,0xfffffffd,0x80000000)[i%4]],
                  [obj+0x74,4,(M32,45,0x7fffffff)[i%3]],
                  [obj+cfg_handle(pus),4,0 if i%13==0 else node],
                  [obj+0x44,4,(0,1,M32,0x80000000)[i%4]],
                  [obj+0x78,4,(0,0x42400000,0xbf800000,0x80000000)[i%4]]]
            allocation(c,0x8000 if i%2 else 0)
            for cb in c:
                if cb[0]==67: cb[1]=[0 if i%5==0 else 1]
                if cb[0]==45 and i%6==0: cb[1]=[]
            yield [p,e,c,[[111+pus,[obj]],[113,[pus,obj]]]]
        # Every service and Skill boundary: mutation before return or exception.
        for slot in (8,11,12,14,18,19,20,21,26,28,33,45,67,69,70,71,72,73,74):
            for throwing in (0,1):
                p,e,c=case(pus); allocation(c,0x8000)
                p += [[owner+0xc,4,0x20]]
                if slot<16:
                    e=[slot]; c += [[slot,[21,43] if slot==8 else [4],[],0]]
                for cb in c:
                    if cb[0]==slot: cb[2:]=[[SK.G_SIM_FRAME,4,40],throwing]
                yield [p,e,c,[[107+pus,[S,40,owner,0,0,0,0]],
                              [110,[pus,obj,0,0,0,0x40e00000,0,0,180,0,21,45,owner,0x40800000]],
                              [111+pus,[obj]],
                              [114,[0x8000,0,0,0,0,0,0,0,0,0,M32]]]]
        # Re-read frame after callbacks, retained centre after collection, adjusted owner.
        for slot in (26,45,67,70,72,74):
            for address,value in ((SK.G_SIM_FRAME,40),(obj+0x70,M32),(node+0xc,other),(obj+0x10,0x447a0000)):
                p,e,c=case(pus); allocation(c,0x8000)
                for cb in c:
                    if cb[0]==slot:cb[2]=[address,4,value]
                yield [p,e,c,[[111+pus,[obj]]]]
    for i,angle in enumerate((0,1,0x7fff,0x8000,0xffff,M32,0x80000000)):
        p,e,c=case(1,i)
        yield [p,e,c,[[114,[obj,0x80000000,0x3f800000,0x43700000,1,2,3,0x3f800000,0xbf800000,0,angle]]]]
    # Zero object, frame extremes, null owner and signed lifetime comparisons.
    for pus in (0,1):
        for frame in (0,0x7fffffff,0x80000000,M32):
            p,e,c=case(pus)
            p += [[SK.G_SIM_FRAME,4,frame],[obj+0x58,4,0x80000000],[obj+0x64,4,0x7fffffff],
                  [obj+0x60,4,0x80000000],[obj+0x5c,4,0x80000000],[obj+0x6c,4,0x7fffffff]]
            yield [p,e,c,[[113,[pus,obj]],[113,[pus,0]]]]


def cfg_handle(pus):
    return 0x80 if pus else 0x7c


def diseaseblade_scenarios(SK):
    rng=random.Random(97318)
    spell,owner,other,third,item,second,holder,table=0x6000,0x7000,0x7100,0x7200,0x8000,0x8100,0x9000,0xa000
    def case(mode=0):
        p=setup(SK,rng,mode)
        for stat,value in enumerate((21,15,3,17,50)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x14,4,SK.DiseaseBlade.DB_VT],
              [spell+0x18,4,900],[spell+0x20,4,0x42100000],
              [SK.DiseaseBlade.DB_VT+8,2,0xfff0],[SK.G_SIM_FRAME,4,30],
              [owner+0xc,4,0x40],[owner+0x3c8,4,0],[owner+0x48,4,table],
              [other+0xc,4,0x40],[other+0x3c8,4,1],[third+0xc,4,0x20],
              [holder,4,table],[table+56,2,0xfff8],[table+64,2,8],[table+176,2,0xfff0]]
        for slot in (0,1):
            rec=SK.G_PLAYERS+slot*SK.REC_SIZE
            p += [[rec+0x4a7c,1,0],[rec+0x4434,4,item],[rec+0x443c,4,second]]
        for weapon in (item,second):
            for k in range(4):p += [[weapon+0x24+8*k,2,0xffff]]
        c=[[i,v,[],0] for i,v in ((16,[123]),(17,[0]),(18,[0]),(20,[99]),(21,[456]),
                                  (28,[]),(27,[]),(45,[owner,other,third,other,0]),(47,[1]),(75,[]),(76,[]))]
        return p,[],c
    # Both availability results, adjusted ranks and allocation/null branches.
    for rank in (0,1,5,20,40,41,42,127,128,255,256,M32,0x80000000):
        for kind in (0,0x20,0x40,0x60):
            p,e,c=case(rank%96)
            p += [[owner+0xc,4,kind],[owner+0x50,4,holder]]
            for cb in c:
                if cb[0]==17:cb[1]=[rank%2]
                if cb[0]==18:cb[1]=[spell if rank%2 else 0]
            yield [p,e,c,[[115,[S,owner]],[116,[S,rank,owner,0xffffffff,M32]],
                          [119,[spell,rank,owner,S]]]]
    # Weapon selection uses the signed active-set byte, signed player slot and raw32 record wrap.
    for slot in (0,1,M32,0x80000000):
        for active in (0,1,2,127,128,255):
            p,e,c=case();rec=(SK.G_PLAYERS+SK.s32(slot)*SK.REC_SIZE)&M32
            p += [[owner+0x3c8,4,slot],[rec+0x4a7c,1,active],
                  [rec+0x4434+4*SK.s8(active),4,item]]
            yield [p,e,c,[[117,[owner]]]]
    # Every modifier location; first matching value, first writable slot, flags,
    # full arrays, signed value/old/limit and narrowing/rounding endpoints.
    for position in range(4):
        for old in (-32768,-1,0,20,30,31,32767):
            for value in (0,20,31,32768,0x7fffffff,0x80000000,M32):
                p,e,c=case();p += [[item+0x24+8*position,2,0x26],[item+0x26+8*position,2,old&0xffff]]
                # Earlier nonmatching slots are occupied, except an intentional early empty.
                for k in range(position):p += [[item+0x24+8*k,2,0xffff if value==31 else 0x18]]
                yield [p,e,c,[[118,[item,value,0x10001,31]]]]
    for flags in (0,0x1000000,0x100000000,0x100000000000000):
        for limit in (0,1,31,M32,0x7fffffff,0x80000000):
            p,e,c=case();p += [[item+8,8,flags]]
            for k in range(4):p += [[item+0x24+8*k,2,0x18]]
            yield [p,e,c,[[118,[item,50,0xffff,limit]],[118,[0,50,30,limit]]]]
    # Constructor duration arithmetic retains Python's wide product for limit,
    # while stored expiry and modifier fields wrap to their actual widths.
    for seconds in (0,1,M32,0x7fffffff,0x80000000):
        for frame in (0,0x7fffffff,0x80000000,M32):
            p,e,c=case();p += [[SK.G_SIM_FRAME,4,frame]];e=[12]
            c += [[12,[seconds],[],0]]
            yield [p,e,c,[[119,[spell,128,owner,S]]]]
    # Aura/expiry gates include signed frames, signed zero radius, nonplayers,
    # PvP acceptance and repeated/empty collection. No sphere filter is invented.
    for frame in (0,29,30,31,60,899,900,901,0x7fffffff,0x80000000,0xffffffe2,M32):
        for kind in (0,0x20,0x40,0x60):
            for radius in (0,0x80000000,0x42100000):
                p,e,c=case();p += [[SK.G_SIM_FRAME,4,frame],[owner+0xc,4,kind],
                    [owner+0x50,4,holder],[spell+0x20,4,radius],
                    [SK.G_PVP_MODE_A,4,frame%2]]
                for cb in c:
                    if cb[0]==47:cb[1]=[0 if frame==0 else 1]
                    if cb[0]==45 and frame==60:cb[1]=[]
                yield [p,e,c,[[120,[spell,0x3f800000,0xc0000000,0x40400000,0]]]]
    # Every reachable service and delegated Skill boundary, retaining mutation
    # before successful returns and throwing callbacks. Pokes can retarget owners,
    # ranks, weapons and frames to verify reference evaluation/re-read order.
    for slot in (10,12,14,16,17,18,20,21,27,28,45,47,75,76):
        for throwing in (0,1):
            for address,value in ((spell+0xc,other),(SK.G_SIM_FRAME,900),(spell,128)):
                p,e,c=case();p += [[owner+0x50,4,holder],[SK.G_PVP_MODE_A,4,1]]
                if slot<16:e=[slot];c += [[slot,[5],[],0]]
                for cb in c:
                    if cb[0]==18:cb[1]=[spell]
                    if cb[0]==slot:cb[2:]=[[address,4,value],throwing]
                yield [p,e,c,[[115,[S,owner]],[116,[S,5,owner,0,0]],
                     [119,[spell,5,owner,S]],[120,[spell,0,0,0,0]]]]
    # Monster holder activation and expiry callbacks have their own prefixes.
    for slot in (75,76):
        for throwing in (0,1):
            p,e,c=case();p += [[owner+0xc,4,0x20],[owner+0x50,4,holder],[spell+0x18,4,0]]
            for cb in c:
                if cb[0]==slot:cb[2:]=[[owner+0x98,2,0x1234],throwing]
            yield [p,e,c,[[119,[spell,5,owner,S]],[120,[spell,0,0,0,0]],
                           [120,[0,0,0,0,0]]]]


def raisearmy_scenarios(SK):
    rng=random.Random(913134)
    spell,owner,corpse,new,other=0x6000,0x7000,0x8000,0x9000,0xa000
    table,holder,newholder,ht,node=0xb000,0xc000,0xc100,0xd000,0xe000
    def case():
        p=setup(SK,rng,0)
        for stat,value in enumerate((4,2,25,7,9)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x14,4,SK.RaiseArmy.RA_VT],
              [SK.RaiseArmy.RA_VT+8,2,0xfff8],[spell+0x10,4,rng.getrandbits(32)],
              [spell+0x28,4,0x3e800000],[spell+0x2c,4,0x3f800000],
              [owner+0x48,4,table],[owner+0x3c8,4,0x123456ab],[table+37*8,2,0xfff8],
              [new+0x48,4,table],[table+27*8,2,8],[table+16*8,2,0xfff0],
              [corpse+0xc,4,0x28],[corpse+0x11a,2,0x80],[corpse+0x34,4,0x23456],
              [corpse+0x22c,4,0x42c80000],[corpse+0x118,1,1],[corpse+0xe8,4,0xabcdef],
              [corpse+0x3e0,4,2],[corpse+0x38,2,0x8000],[corpse+0x50,4,holder],
              [new+0xc,4,0x20],[new+0x34,4,0x34567],[new+0x50,4,newholder],
              [new+0xa8,8,0x12345678],[new+0xe8,4,0x123],[holder,4,ht],[newholder,4,ht],
              [ht+15*8,2,0xfff0],[ht+16*8,2,8],[ht+8,2,0xfff8],[ht+64,2,16],
              [spell+0x18,4,node],[node+0xc,4,corpse],[SK.G_HANDLE_FREE,4,0xf000]]
        for k in range(12):p += [[0xf000+k*0x20,4,0xf020+k*0x20 if k<11 else 0]]
        for off,v in ((0xfc,0x123abc),(0x118,0xfffe),(0x114,0x3f800000),(0x104,0x3e800000),
                      (0x108,0x3f000000),(0x10c,0x3fc00000),(0x110,0x40000000)):
            p += [[corpse+0x20+off,2 if off==0x118 else 4,v]]
        # Default points are coincident; negative-zero and nonzero deltas below
        # exercise inclusive reach and sqrt rounding without original content.
        for target in (spell+0x1c,owner+0x10,corpse+0x10):
            for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):
                p += [[target+4*k,4,v]]
        results={16:[123],17:[0],18:[0],20:[99],21:[456],27:[],28:[],
                 29:[321],32:[],45:[corpse,0],71:[],76:[],77:[0],78:[new],79:[],80:[7],
                 81:[],82:[],83:[0xabcdef],84:[0x26],85:[],86:[1],87:[0x89abcdef,0xfedcba98]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    # All seven methods and both active availability outcomes. Construction
    # covers signed byte ranks and both allocation paths, with/without light.
    for rank in (0,1,5,20,40,41,42,127,128,255,256,M32,0x80000000):
        for allocation in (0,spell):
            p,e,c=case()
            for cb in c:
                if cb[0]==18:cb[1]=[allocation]
                if cb[0]==17:cb[1]=[rank%2]
            yield [p,e,c,[[121,[S,owner]],[122,[S,rank,owner,0x3f800000,0xc0000000,0x40400000,M32]],
                          [124,[spell,rank,owner,S]]]]
    # Eligibility gates test every documented bit including high-half flags,
    # death-animation readiness, null and post-callback flags re-read.
    for flags in (0,8,0x28,0x48,0xa,0x1000028,0x68):
        for anim in (0,0x80,0xffff):
            for creatureflags in (0,1,0x800,0x1000000,0x4000000,0x100000000,0x200,0x8000000000000000):
                p,e,c=case();p += [[corpse+0xc,4,flags],[corpse+0x11a,2,anim],[corpse+0xa8,8,creatureflags]]
                yield [p,e,c,[[123,[spell,corpse]],[123,[spell,0]]]]
    for rank in (0,1,M32):
        p,e,c=case()
        for cb in c:
            if cb[0]==77:cb[1]=[rank];cb[2]=[corpse+0xc,4,0x1000028]
        yield [p,e,c,[[123,[spell,corpse]],[124,[spell,5,owner,S]]]]
    # Constructor query order/reversal, duplicates, stale entries and signed
    # halfword counter wrap, signed stat narrowing and early termination.
    for found in ([],[0,0],[corpse,corpse,0],[corpse,other,corpse]):
        p,e,c=case();p += [[other+0xc,4,0x28],[other+0x11a,2,0x80]]
        for cb in c:
            if cb[0]==45:cb[1]=found
        yield [p,e,c,[[124,[spell,5,owner,S]],[126,[spell,0x447a0000]]]]
    for count in (0,1,0x7fff,0x8000,0xffff):
        for limit in (0,1,2,0x7fff,0x8000,0xffff,0x10000):
            p,e,c=case();p += [[spell+0x30,2,count],[S+0x1a,2,limit&0xffff]]
            yield [p,e,c,[[126,[spell,0]]]]
    # Ring reach (radius subtracted after sqrt), creature/noncreature filters,
    # missing names, null/noncreature factory results, and each sound/removal path.
    for delta in (0,0x80000000,0x3f800000,0xbf800000,0x40000000,0x7fffffff):
        for radius in (0,0x3f800000,0xbf800000):
            p,e,c=case();p += [[corpse+0x10,4,delta],[corpse+0x1c,4,radius]]
            yield [p,e,c,[[126,[spell,0x3f800000]]]]
    for target in (0,corpse,other):
        for name in (0,0x23456):
            for created in (0,new,other):
                p,e,c=case();p += [[node+0xc,4,target],[corpse+0x34,4,name]]
                for cb in c:
                    if cb[0]==78:cb[1]=[created]
                yield [p,e,c,[[126,[spell,0x447a0000]]]]
    # Every lifecycle boundary including duration zero, negative/NaN values,
    # inclusive endpoint and private generator seed extremes. Ignored run args.
    for duration in (0,0x80000000,0x3f800000,0xbf800000,0x7fffffff):
        for clock in (0,0x3f000000,0x3f800000,0x3f800001,0xbf800000):
            p,e,c=case();p += [[spell+0x2c,4,duration],[spell+0x28,4,clock],[spell+0x18,4,0]]
            yield [p,e,c,[[125,[spell,0xffffffff,0x80000000]]]]
    # Full dress: HP floor and NaN/rounding, source signed track indices,
    # optional holders, animation args, variant short circuit, ant tint kinds.
    for hp in (0,0x3f000000,0x3f800000,0xbf800000,0x7fffffff,0x7f7fffff):
        for amount in (0,1,25,0xffff):
            p,e,c=case();p += [[corpse+0x22c,4,hp],[S+0x1c,2,amount]]
            yield [p,e,c,[[127,[spell,corpse,new]]]]
    for top in (0,1,2,127,128,255):
        for holders in (0,1,2,3):
            p,e,c=case();p += [[corpse+0x118,1,top],[corpse+0x50,4,holder if holders&1 else 0],
                               [new+0x50,4,newholder if holders&2 else 0]]
            yield [p,e,c,[[127,[spell,corpse,new]]]]
    for prefix in (0,1):
        for kind in (0,1,2,3,M32):
            p,e,c=case();p += [[corpse+0x3e0,4,kind]]
            for cb in c:
                if cb[0]==86:cb[1]=[prefix]
            yield [p,e,c,[[127,[spell,corpse,new]]]]
    # Mutation before success/failure at every reachable delegated boundary;
    # separate operation cases ensure an early exception cannot hide later ones.
    operations={121:[S,owner],122:[S,5,owner,0,0,0,0],123:[spell,corpse],
                124:[spell,5,owner,S],125:[spell,0,0],126:[spell,0],127:[spell,corpse,new]}
    boundaries={121:(16,17),122:(14,18,20,21,28),123:(77,),124:(10,45,77),
                125:(10,32,27),126:(10,20,21,77,78),
                127:(10,29,71,76,79,80,81,82,83,84,85,86,87)}
    for op,slots in boundaries.items():
        for slot in slots:
            for throwing in (0,1):
                for address,value in ((spell+0xc,other),(corpse+0x118,255)):
                    p,e,c=case()
                    if slot<16:e=[slot];c += [[slot,[5],[],0]]
                    if op==125 and slot==27:p += [[spell+0x28,4,0x40000000]]
                    for cb in c:
                        if op==122 and cb[0]==18:cb[1]=[spell]
                        if cb[0]==slot:cb[2:]=[[address,4,value],throwing]
                    yield [p,e,c,[[op,operations[op]]]]


def drowsy_scenarios(SK):
    rng=random.Random(8830)
    spell,owner,other,table=0x6000,0x7000,0x8000,0x9000
    def case():
        p=setup(SK,rng,0)
        for stat,value in enumerate((2,4,100,75,3)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x14,4,SK.Drowsy.DR_VT],
              [SK.Drowsy.DR_VT+8,2,0xfff8],[spell+0x10,4,rng.getrandbits(32)],
              [spell+0x18,4,0],[spell+0x1c,4,0],[spell+0x20,4,4],[spell+0x24,4,0],
              [spell+0x28,4,0],[spell+0x2c,4,0xc187b1cf],[spell+0x30,4,0xc207b1cf],
              [owner+0x48,4,table],[table+160,2,0xfff0],[owner+0xc,4,0x40],
              [owner+0x3c8,4,M32],[owner+0x1c,4,0x41400000],[owner+0x20,4,0x42100000],
              [owner+0x3c,4,0x40000000],[owner+0xe0,4,0x3f800000],[owner+0x38,2,0x8000],
              [SK.G_NETWORK_MODE,1,1],[SK.G_FORCE_GHOST_REBASE,1,0]]
        for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):p += [[owner+0x10+4*k,4,v]]
        results={18:[0],20:[123],43:[],28:[],39:[1],32:[],27:[],67:[1],77:[0],88:[owner,other],89:[],90:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    # Cast: allocation, count, caster and every victim filter, with signed facing/rank.
    for rank in (0,1,5,127,128,255,256,M32,0x80000000):
        for caster in (0,owner):
            for allocated in (0,spell):
                p,e,c=case()
                for cb in c:
                    if cb[0]==18:cb[1]=[allocated]
                yield [p,e,c,[[128,[S,rank,caster,0x80000000,0x3f800000,0xc0000000,0xffff8001]],
                              [129,[spell,rank,owner,S]],[132,[spell]]]]
    for count in (0,1,2,0x7fff,0x8000,0xffff):
        for found in ([],[owner],[0,owner,other],[owner,owner,other]):
            p,e,c=case();p += [[S+0x18,2,count]]
            for cb in c:
                if cb[0]==88:cb[1]=found
            yield [p,e,c,[[128,[S,5,owner,0,0,0,0]]]]
    for flags in (0,8,0x40,0x1000000,0x1000040):
        for timer in (0,1,0x7fff,0x8000,0xffff):
            for boss in (0,1):
                p,e,c=case();p += [[owner+0xc,4,flags],[owner+0x31a,2,timer]]
                for cb in c:
                    if cb[0]==77:cb[1]=[boss]
                    if cb[0]==88:cb[1]=[owner]
                yield [p,e,c,[[128,[S,5,owner,0,0,0,0]]]]
    for enemy in (0,1):
        p,e,c=case()
        for cb in c:
            if cb[0]==67:cb[1]=[enemy]
        yield [p,e,c,[[128,[S,5,owner,0,0,0,0]]]]
    # All phases, strict/inclusive time edges, invalid phase, dead and null spell.
    for phase in (0,1,2,3,M32):
        for clock in (0,0x3ef77777,0x3f000000,0x3e3645a1,0x40800000,0xbf800000,0x7fffffff):
            for duration in ((0,4,0xffffffff) if phase==2 else (4,)):
                p,e,c=case();p += [[spell+0x24,4,phase],[spell+0x18,4,clock],[spell+0x20,4,duration]]
                yield [p,e,c,[[131,[spell,0xffffffff,0x80000000]]]]
    for phase in (0,1,2,3):
        p,e,c=case();p += [[spell+0x24,4,phase],[owner+0xc,4,8]]
        yield [p,e,c,[[131,[spell,0,0]],[131,[0,0,0]]]]
    # Deterministic private draws precede chance: unclamped chance and timer wrap.
    for seed in (0,1,M32,0x80000000):
        for chance in (0,1,50,100,101,0xffff):
            duration=(0,546,547,0xffffffff)[(seed+chance)%4]
            p,e,c=case();p += [[spell+0x18,4,0x3f000000],[spell+0x10,4,seed],
                               [S+0x1c,2,chance],[spell+0x20,4,duration]]
            yield [p,e,c,[[131,[spell,0,0]]]]
    # Height wrap, network/player/local gates and fraction signed-byte narrowing.
    for network in (0,1,128,255):
        for flags in (0,0x20,0x40,0x48):
            for local in (0,1):
                p,e,c=case();p += [[spell+0x24,4,2],[owner+0xc,4,flags],[SK.G_NETWORK_MODE,1,network],
                    [spell+0x28,4,0x424e7202],[spell+0x2c,4,0x424e7203],[spell+0x30,4,0x7fffffff]]
                for cb in c:
                    if cb[0]==39:cb[1]=[local]
                yield [p,e,c,[[131,[spell,0,0]],[132,[spell]]]]
    for flat in (0,1):
        for speed in (0,0x80000000,0x40800000,0xc1400000,0x7fffffff):
            p,e,c=case()
            yield [p,e,c,[[130,[spell,0x123abc,speed,flat]]]]
    # Repeated ticks cross the half-second boundary with the exact preceding RNG.
    for chance in (0,100):
        p,e,c=case();p += [[S+0x1c,2,chance]]
        yield [p,e,c,[[131,[spell,0,0]]]*32]
    # Mutation and failure prefixes at every reachable service or statistic call.
    ops={128:[S,5,owner,0,0,0,0],129:[spell,5,owner,S],130:[spell,0x123abc,0x40800000,0],
         131:[spell,0,0],132:[spell]}
    boundaries={128:(14,20,43,18,28,10,88,67,77,89,90),129:(10,),130:(32,),131:(32,10,39,27),132:(10,)}
    for op,slots in boundaries.items():
        for slot in slots:
            for throwing in (0,1):
                for address,value in ((spell+0xc,other),(owner+0xc,8)):
                    p,e,c=case()
                    if slot<16:e=[slot];c += [[slot,[5],[],0]]
                    if slot==28:
                        for cb in c:
                            if cb[0]==18:cb[1]=[spell]
                    if op==131:
                        p += [[spell+0x18,4,0x3f000000]]
                        if slot==39:p += [[spell+0x24,4,2]]
                        if slot==27:p += [[owner+0xc,4,8]]
                    for cb in c:
                        if cb[0]==slot:cb[2:]=[[address,4,value],throwing]
                    yield [p,e,c,[[op,ops[op]]]]


def waveofflame_scenarios(SK):
    rng=random.Random(95317)
    spell,owner,other,table=0x6000,0x7000,0x8000,0x9000
    def case():
        p=setup(SK,rng,0)
        for stat,value in enumerate((5,100,2,3,60)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x14,4,SK.WaveOfFlame.WF_VT],
              [SK.WaveOfFlame.WF_VT+8,2,0xfff8],[spell+0x10,4,rng.getrandbits(32)],
              [spell+0x18,4,0],[spell+0x1c,4,0],[spell+0x20,4,0],[spell+0x24,4,0],
              [spell+0x28,4,0],[owner+0x48,4,table],[table+160,2,0xfff0],
              [owner+0xc,4,0x40],[owner+0x38,2,0x8001],[owner+0x3c,4,0x40000000],
              [other+0xc,4,0x20],[other+0x1c,4,0x40000000],[0x864ea8,4,0xb000]]
        for obj in (owner,other):
            for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):p += [[obj+0x10+4*k,4,v]]
        for k in range(32):p += [[0xb000+0x20*k,4,0xb000+0x20*(k+1) if k<31 else 0]]
        results={18:[0],20:[123],21:[456],28:[],91:[other],90:[],67:[1],33:[],32:[],27:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    def candidates(p,objects):
        p += [[spell+0x18,4,0xa000 if objects else 0]]
        for k,obj in enumerate(objects):
            node=0xa000+0x20*k
            p += [[node,4,node+0x20 if k+1<len(objects) else 0],
                  [node+4,4,spell+0x18 if not k else node-0x20],
                  [node+8,4,0],[node+0xc,4,obj]]
    # Can-use and cast ignore their position/facing; allocation failure is exact.
    for rank in (0,1,5,20,127,128,255,256,M32,0x80000000):
        for allocated in (0,spell):
            p,e,c=case()
            for cb in c:
                if cb[0]==18:cb[1]=[allocated]
            yield [p,e,c,[[133,[S,owner]],[134,[S,rank,owner,0xffffffff,0xffff8001]]]]
    # Complete constructor: offsets, narrowed rank, visual offset/facing, query gates
    # and front insertion with duplicates, nulls, single and empty query lists.
    for rank in (0,5,127,128,255,256,M32):
        for caster in (0,owner):
            for offset in ((0,0,0),(0x80000000,0x3f800000,0xc0000000)):
                p,e,c=case()
                yield [p,e,c,[[135,[spell,rank,caster,*offset,S]]]]
    for flags in (0,1,4,8,0x20,0x40,0x1000000,0xffffffff):
        for found in ([],[other],[0,other,owner,other]):
            p,e,c=case();p += [[other+0xc,4,flags]]
            for cb in c:
                if cb[0]==91:cb[1]=found
            yield [p,e,c,[[135,[spell,5,owner,0,0,0,S]],[137,[spell,0x42c80000]]]]
    for visual in (0,0x80000000,0x3f800000,0xc0000000,0x7fffffff):
        for facing in (0,0x7fff,0x8000,0xffff):
            p,e,c=case();p += [[owner+0xe0,4,visual],[owner+0x38,2,facing]]
            yield [p,e,c,[[135,[spell,5,owner,0,0,0,S]]]]
    # Inclusive ring test and the UNSQUARED radius subtraction, NaN, +/-zero.
    for ring in (0,0x80000000,0x3f800000,0xbf800000,0x40000000,0x7fffffff):
        for radius in (0,0x3f800000,0x40000000,0xbf800000,0x7fffffff):
            p,e,c=case();candidates(p,[other]);p += [[other+0x10,4,0x3f800000],[other+0x14,4,0],
                [other+0x1c,4,radius]]
            yield [p,e,c,[[137,[spell,ring]],[137,[spell,ring]]]]
    for flags in (0,1,8,9,0x20,0x21,0x28,0x40,0x1000020):
        for enemy in (0,1):
            for player in (0,0x40):
                p,e,c=case();candidates(p,[0,other,owner,other]);p += [[other+0xc,4,flags],[owner+0xc,4,player]]
                for cb in c:
                    if cb[0]==67:cb[1]=[enemy]
                yield [p,e,c,[[137,[spell,0x42c80000]],[137,[spell,0x42c80000]]]]
    # Run: expiry strict comparison, signed statistic/rank, private RNG, wobble,
    # saturating share and three particle definitions, repeated ticks to expiry.
    for clock in (0,0x3ecccccd,0x3f000000,0x3f0ccccd,0x3f800000,0xbf800000,0x80000000,0x7fffffff):
        for steps in (0,1,2,5,0x7fff,0x8000,0xffff):
            p,e,c=case();p += [[spell+0x28,4,clock],[S+0x18,2,steps]]
            yield [p,e,c,[[136,[spell,0xffffffff,0xffff8001]]]]
    for seed in (0,1,M32,0x80000000):
        for frame in (0,1,19,0x7fffffff,0x80000000,M32):
            p,e,c=case();p += [[spell+0x10,4,seed],[SK.G_SIM_FRAME,4,frame]];candidates(p,[other])
            yield [p,e,c,[[136,[spell,0,0]]]]
    for rank in (0,127,128,255,256,M32):
        p,e,c=case();p += [[spell,1,rank]]
        yield [p,e,c,[[136,[spell,0,0]]]]
    p,e,c=case();candidates(p,[other]);p += [[spell+0x28,4,0x3f000000]]
    yield [p,e,c,[[136,[spell,0,0]]]*5]
    # Success and failure mutations at every reachable callback boundary. The
    # surviving clock, candidate list, RNG and sound prefixes are all observed.
    operations={134:[S,5,owner,0,0],135:[spell,5,owner,0,0,0,S],
                136:[spell,0,0],137:[spell,0x42c80000]}
    boundaries={134:(14,18),135:(20,21,18,28,12,91,90),
                136:(12,67,8,33,20,21,32,27),137:(67,8,33,20,21)}
    for op,slots in boundaries.items():
        for slot in slots:
            for throwing in (0,1):
                for address,value in ((spell+0xc,other),(spell+0x1c,0x3f800000)):
                    p,e,c=case();candidates(p,[other])
                    if slot<16:
                        e=[slot];c += [[slot,[10,20] if slot==8 else [5],[],0]]
                    if slot==28:
                        for cb in c:
                            if cb[0]==18:cb[1]=[0xc000]
                    if slot==27:p += [[spell+0x28,4,0x3f800000]]
                    for cb in c:
                        if cb[0]==slot:cb[2:]=[[address,4,value],throwing]
                    yield [p,e,c,[[op,operations[op]]]]


def roar_scenarios(SK):
    rng=random.Random(76317)
    spell,owner,other,table=0x6000,0x7000,0x8000,0x9000
    def case():
        p=setup(SK,rng,0)
        for stat,value in enumerate((5,100,2,3,60)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x14,4,SK.Roar.ROAR_VT],
              [SK.Roar.ROAR_VT+8,2,0xfff8],[spell+0x10,4,rng.getrandbits(32)],
              [spell+0x18,4,owner],[spell+0x1c,4,0],[spell+0x2c,4,0],
              [spell+0x30,4,0xbf800000],[spell+0x34,4,0xbf800000],[spell+0x3c,4,456],
              [spell+0x40,4,1],[owner+0x48,4,table],[table+160,2,0xfff0],
              [owner+0x11a,2,0x20],[owner+0x58c,4,6],[owner+0x3c8,4,0],
              [owner+0x24,4,0x9800],[0x9800,4,0x9900],[owner+0x38,2,0x8001],
              [SK.G_PLAYERS+0xc0,4,0x3f800000],[SK.G_PLAYERS+0xc4,4,0x42c80000],
              [other+0xc,4,0x20],[other+0x1c,4,0x40000000],[0x864ea8,4,0xb000]]
        for obj in (owner,other):
            for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):p += [[obj+0x10+4*k,4,v]]
        for k in range(32):p += [[0xb000+0x20*k,4,0xb000+0x20*(k+1) if k<31 else 0]]
        results={16:[123],17:[0],18:[spell],20:[0xc000],21:[456],36:[0x3f800000,0xc0000000,0x40400000],
                 37:[0xd000],38:[],39:[1],40:[0],42:[],70:[0x40000000],91:[other],90:[],
                 67:[1],33:[],32:[],27:[],92:[],93:[1],94:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    def setcb(c,slot,result):
        for cb in c:
            if cb[0]==slot:cb[1]=result
    def candidates(p,objects):
        p += [[spell+0x1c,4,0xa000 if objects else 0]]
        for k,obj in enumerate(objects):
            node=0xa000+0x20*k
            p += [[node,4,node+0x20 if k+1<len(objects) else 0],
                  [node+4,4,spell+0x1c if not k else node-0x20],[node+8,4,0],[node+0xc,4,obj]]
    for found in (0,spell,M32):
        p,e,c=case();setcb(c,17,[found]);yield [p,e,c,[[138,[S,owner]]]]
    for rank in (0,1,5,10,127,128,255,256,M32,0x80000000):
        for allocated in (0,spell):
            p,e,c=case();setcb(c,18,[allocated]);yield [p,e,c,[[139,[S,rank,owner,M32,0xffff8001]]]]
    for rank in (0,5,10,127,128,255,256,M32):
        for caster in (0,owner):
            p,e,c=case();yield [p,e,c,[[140,[spell,rank,caster,S]]]]
    for style in (0,1,2,3,7,4,127,128,255):
        p,e,c=case();p += [[SK.G_PLAYERS+SK.Roar.STYLE_BYTE,1,style]];yield [p,e,c,[[140,[spell,5,owner,S]]]]
    # Standalone mouth geometry: one private draw, signed integer facing, no
    # implicit signed16 narrowing until the run calls it, both share modes.
    for seed in (0,1,M32,0x80000000):
        for facing in (0,0x7fff,0x8000,0xffff8001,M32,0x7fffffff):
            for centred in (0,1):
                p,e,c=case();p += [[spell+0x10,4,seed]]
                yield [p,e,c,[[141,[spell,facing,0x3f800000,0xc0000000,0x40400000,0x8e6dd0,0x4129b986,centred]]]]
    # Animation events and loss of state/flags, signed network/local gating.
    for event in (0,2,4,6,M32):
        for flags in (0,0x20):
            for state in (2,6):
                p,e,c=case();setcb(c,40,[event]);p += [[owner+0x11a,2,flags],[owner+0x58c,4,state]]
                yield [p,e,c,[[142,[spell,M32,0xffff8001]]]]
    for network in (0,1,128,255):
        for local in (0,1):
            p,e,c=case();p += [[SK.G_NETWORK_MODE,1,network]];setcb(c,39,[local]);yield [p,e,c,[[142,[spell,0,0]]]]
    for objects in ([],[other],[0,other,owner,other]):
        for flags in (0,1,4,8,0x20,0x40,0xffffffff):
            p,e,c=case();setcb(c,40,[2]);setcb(c,91,objects);p += [[other+0xc,4,flags]]
            yield [p,e,c,[[142,[spell,0,0]]]]
    # Hit uses sqrt distance minus radius and the signed rank flag bands;
    # no descriptor is substituted for the reference's None argument.
    for rank in (0,4,5,9,10,127,128,255):
        for flags in (0,1,8,9,0x20,0x21,0x28,0x40):
            for enemy in (0,1):
                p,e,c=case();candidates(p,[0,other,other]);setcb(c,67,[enemy]);p += [[spell,1,rank],[other+0xc,4,flags]]
                yield [p,e,c,[[143,[spell,0x42c80000]],[143,[spell,0x42c80000]]]]
    for ring in (0,0x80000000,0x3f800000,0x40000000,0xbf800000,0x7fffffff):
        for radius in (0,0x3f800000,0x40000000,0xbf800000,0x7fffffff):
            p,e,c=case();candidates(p,[other]);p += [[other+0x10,4,0x3f800000],[other+0x14,4,0],[other+0x1c,4,radius]]
            yield [p,e,c,[[143,[spell,ring]]]]
    # Wave equality/endpoints, rank particle counts, zero/null spell and private seeds.
    for clock in (0,0x3effffff,0x3f000000,0x3f000001,0x40bfffff,0x40c00000,0x40c00001,0xbf800000,0x80000000,0x7fffffff):
        for rank in (9,10,128):
            p,e,c=case();p += [[spell+0x30,4,0],[spell+0x2c,4,clock],[spell,1,rank]]
            yield [p,e,c,[[142,[spell,0,0]]]]
    for clock in (0,0x3effffff,0x3f000000,0x3f000001,0x7fffffff):
        for interrupted in (0,1):
            p,e,c=case();p += [[spell+0x40,4,0],[spell+0x44,4,interrupted],[spell+0x34,4,0x3f000000],[spell+0x2c,4,clock]]
            yield [p,e,c,[[142,[spell,0,0]]]]
    for wet in (0,1,M32):
        p,e,c=case();setcb(c,40,[2]);setcb(c,93,[wet]);yield [p,e,c,[[142,[spell,0,0]]]]
    for seed in (0,1,M32,0x80000000):
        for rank in (9,10):
            p,e,c=case();p += [[spell+0x30,4,0],[spell+0x10,4,seed],[spell,1,rank]];yield [p,e,c,[[142,[spell,0,0]]]]
    p,e,c=case();p += [[0x30,4,0xbf800000]];yield [p,e,c,[[142,[0,0,0]]]]
    # Short repeated traces include wave damage/removal and interruption expiry.
    p,e,c=case();candidates(p,[other]);p += [[spell+0x30,4,0],[spell+0x2c,4,0x3f000000]]
    yield [p,e,c,[[142,[spell,0,0]]]*3]
    p,e,c=case();p += [[owner+0x58c,4,2],[spell+0x2c,4,0x3effffff]]
    yield [p,e,c,[[142,[spell,0,0]]]*2]
    # Refund upper clamp, pre/post-shout interruption, linger particle rolls,
    # both mouth modes and owner-slot wrapping remain reference behavior.
    for mana,maximum in ((0,0x42c80000),(0x42c80000,0x3f800000),(0xbf800000,0),(0x7fffffff,0x3f800000)):
        for shout in (0xbf800000,0,0x7fffffff):
            p,e,c=case();p += [[owner+0x58c,4,2],[spell+0x30,4,shout],
                [SK.G_PLAYERS+0xc0,4,mana],[SK.G_PLAYERS+0xc4,4,maximum]]
            yield [p,e,c,[[142,[spell,0,0]]]]
    for seed in (0,1,M32,0x80000000,1000,12345):
        for interrupted in (0,1):
            p,e,c=case();p += [[spell+0x10,4,seed],[spell+0x44,4,interrupted],[spell+0x48,4,1]]
            yield [p,e,c,[[142,[spell,0,0xffff8001]]]]
    for slot in (0,1,M32,0x80000000):
        p,e,c=case();p += [[owner+0x3c8,4,slot]]
        rec=(SK.G_PLAYERS+SK.s32(slot)*SK.REC_SIZE)&M32
        p += [[rec+SK.Roar.STYLE_BYTE,1,7]]
        yield [p,e,c,[[140,[spell,5,owner,S]]]]
    operations={138:[S,owner],139:[S,5,owner,0,0],140:[spell,5,owner,S],
                141:[spell,0,0,0,0,0x8e6dd0,0x4129b986,1],142:[spell,0,0],143:[spell,0x42c80000]}
    boundaries={138:(16,17),139:(14,18),140:(37,20,38,21),141:(32,),
                142:(39,40,92,36,70,93,10,91,90,67,8,33,32,94,5,42,27),143:(67,8,33)}
    for op,slots in boundaries.items():
        for slot in slots:
            for throwing in (0,1):
                for address,value in ((spell+0x18,other),(spell+0x2c,0x3f800000),(spell,128)):
                    p,e,c=case();candidates(p,[other]);setcb(c,40,[2]);p += [[SK.G_NETWORK_MODE,1,1]]
                    if slot<16:
                        e=[slot];c += [[slot,[10,20] if slot==8 else [5],[],0]]
                    if slot in (5,42):p += [[owner+0x58c,4,2]]
                    if slot in (67,8,33):p += [[other+0x10,4,0],[other+0x14,4,0],[other+0x1c,4,0x42c80000]]
                    if slot==27:p += [[spell+0x2c,4,0x40c00000]];setcb(c,40,[0])
                    for cb in c:
                        if cb[0]==slot:cb[2:]=[[address,4 if address!=spell else 1,value],throwing]
                    yield [p,e,c,[[op,operations[op]]]]


def unspokenword_scenarios(SK):
    rng=random.Random(702614)
    spell,owner,other=0x6000,0x7000,0x8000
    def case():
        p=setup(SK,rng,0)
        for stat,value in enumerate((20,2,100,3,60)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[spell,1,5],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x14,4,SK.UnspokenWord.UW_VT],
              [SK.UnspokenWord.UW_VT+8,2,0xfff8],[spell+0x10,4,rng.getrandbits(32)],
              [spell+0x18,4,owner],[spell+0x1c,4,0],[spell+0x2c,4,0],
              [spell+0x30,4,0xd000],[spell+0x34,4,0xd100],
              [SK.G_SIM_FRAME,4,20],[other+0xc,4,0x20],[other+0x228,4,0x3f800000],
              [other+0x1c,4,0x40000000],[other+0x48,4,0x9000],[0x90f0,2,0xfff0],
              [0x864ea8,4,0xb000]]
        for obj in (owner,other):
            for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):p += [[obj+0x10+4*k,4,v]]
        for k in range(32):p += [[0xb000+0x20*k,4,0xb000+0x20*(k+1) if k<31 else 0]]
        results={18:[spell],20:[0xc000],21:[456],91:[other],90:[],67:[1],68:[],33:[],32:[],27:[],
                 28:[],95:[0xd000],96:[],97:[],98:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    def setcb(c,slot,result):
        for cb in c:
            if cb[0]==slot:cb[1]=result
    def candidates(p,objects):
        p += [[spell+0x1c,4,0xa000 if objects else 0]]
        for k,obj in enumerate(objects):
            node=0xa000+0x20*k
            p += [[node,4,node+0x20 if k+1<len(objects) else 0],
                  [node+4,4,spell+0x1c if not k else node-0x20],[node+8,4,0],[node+0xc,4,obj]]
    for rank in (0,1,5,127,128,255,256,M32,0x80000000):
        for allocated in (0,spell):
            p,e,c=case();setcb(c,18,[allocated])
            yield [p,e,c,[[144,[S,rank,owner,0x80000000,0x3f800000,0xc0000000,0xffff8001]]]]
    for rank in (0,1,127,128,255,256,M32):
        for caster in (0,owner):
            p,e,c=case();yield [p,e,c,[[145,[spell,rank,caster,S]]]]
    for objects in ([],[other],[0,other,owner,other]):
        for flags in (0,1,4,8,0x20,0x21,0x24,0x28,0x40,M32):
            p,e,c=case();setcb(c,91,objects);p += [[other+0xc,4,flags]]
            yield [p,e,c,[[145,[spell,5,owner,S]]]]
    for start in (0,1,0x7fffffff,0x80000000,M32,5368709,5368710):
        for age in (0,15,16,29,30,M32,0x7fffffff,0x80000000):
            p,e,c=case();p += [[spell+0x2c,4,start]];yield [p,e,c,[[146,[spell,age]]]]
    for age in (0,1,14,15,16,29,30,31,M32,0x80000000,0x7fffffff):
        for steps in (0,1,2,0xffff):
            p,e,c=case();p += [[SK.G_SIM_FRAME,4,age],[S+0x1a,2,steps]]
            yield [p,e,c,[[147,[spell,M32,0xffff8001]]]]
    for start in (M32,0x80000000,0x7fffffff):
        p,e,c=case();p += [[spell+0x2c,4,start],[SK.G_SIM_FRAME,4,(start+20)&M32]]
        yield [p,e,c,[[147,[spell,0,0]]]]
    # Damage flag thresholds, every creature gate, surviving HP and private rolls.
    for rank in (0,5,6,10,11,15,16,127,128,255):
        for flags in (0,1,8,0x20,0x21,0x28):
            for enemy in (0,1):
                p,e,c=case();candidates(p,[0,other,other]);setcb(c,67,[enemy]);p += [[spell,1,rank],[other+0xc,4,flags]]
                yield [p,e,c,[[148,[spell,0x42c80000]],[148,[spell,0x42c80000]]]]
    for hp in (0,0x80000000,0x3f800000,0xbf800000,0x7fffffff):
        for chance in (0,1,50,99,100,101,0xffff):
            for seed in (0,1,M32):
                p,e,c=case();candidates(p,[other]);p += [[other+0x228,4,hp],[S+0x1c,2,chance],[spell+0x10,4,seed]]
                yield [p,e,c,[[148,[spell,0x42c80000]]]]
    for ring in (0,0x80000000,0x3f800000,0x40000000,0xbf800000,0x7fffffff):
        for radius in (0,0x3f800000,0x40000000,0xbf800000,0x7fffffff):
            p,e,c=case();candidates(p,[other]);p += [[other+0x10,4,0x3f800000],[other+0x14,4,0],[other+0x1c,4,radius]]
            yield [p,e,c,[[148,[spell,ring]]]]
    # Same private stream supplies particle rejection and the later stun roll.
    for age in (15,16,29):
        for seed in (0,1,M32,0x80000000):
            p,e,c=case();candidates(p,[other]);p += [[SK.G_SIM_FRAME,4,age],[spell+0x10,4,seed],[S+0x1c,2,50]]
            yield [p,e,c,[[147,[spell,0,0]]]]
    p,e,c=case();candidates(p,[other]);yield [p,e,c,[[147,[spell,0,0]]]*3]
    # Both spirals: first centre captured at entry, second reloaded each particle.
    operations={144:[S,5,owner,0x3f800000,0xc0000000,0x40400000,M32],
                145:[spell,5,owner,S],147:[spell,0,0],148:[spell,0x42c80000]}
    boundaries={144:(14,20,21,18,28),145:(95,10,91,90),
                147:(10,96,97,98,32,67,8,33,68,27),148:(67,8,33,10,68)}
    for op,slots in boundaries.items():
        for slot in slots:
            for throwing in (0,1):
                for address,value in ((spell+0x20,0x40000000),(spell+0x2c,5),(spell,128)):
                    p,e,c=case();candidates(p,[other])
                    if slot<16:
                        e=[slot];c += [[slot,[10,20] if slot==8 else [2],[],0]]
                    if slot==27:p += [[SK.G_SIM_FRAME,4,30]]
                    for cb in c:
                        if cb[0]==slot:cb[2:]=[[address,1 if address==spell else 4,value],throwing]
                    yield [p,e,c,[[op,operations[op]]]]


def poisonbolt_scenarios(SK):
    rng=random.Random(892614)
    obj,owner,other,target=0x6000,0x7000,0x8000,0x9000
    def case():
        p=setup(SK,rng,0)
        for stat,value in enumerate((20,2,100,3,60)):
            p += [[S+0xe+2*stat,2,0],[S+0x18+2*stat,2,value],[S+0x24+4*stat,4,0],
                  [S+0x38+2*stat,2,0xffff]]
        p += [[obj+0x48,4,SK.PoisonBolt.PB_VT],[SK.PoisonBolt.PB_VT+8,2,0xfff8],
              [obj+0x44,4,rng.getrandbits(32)],[obj+0x90,4,0xa000],[0xa00c,4,owner],
              [obj+0x4c,4,0xa040],[0xa04c,4,target],[obj+0x94,4,0xa020],[0xa02c,4,other],
              [obj+0x98,4,S],[obj+0x9c,2,5],[obj+0xac,4,113],[obj+0xb0,4,11],[obj+0xb4,4,0],
              [SK.G_SIM_FRAME,4,20],[owner+0x48,4,0xd000],[0xd0a0,2,0xfff0],
              [other+0xc,4,0x20],[other+0xa8,8,0],
              [0x552658,8,0x4008000000000000],[0x552660,8,0x4031000000000000],
              [0x864ea8,4,0xb000]]
        for address in (obj,owner,other):
            for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):p += [[address+0x10+4*k,4,v]]
        p += [[obj+0xa0,4,0x43af0000],[obj+0xa4,4,0],[obj+0xa8,4,0]]
        for k in range(16):p += [[0xb000+0x20*k,4,0xb000+0x20*(k+1) if k<15 else 0]]
        results={18:[0xc000],19:[],20:[456],26:[],32:[],33:[],43:[],67:[1],73:[],
                 8:[10,20],10:[3],71:[],
                 88:[other,target],90:[],99:[],100:[0],101:[],102:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    def setcb(c,slot,result,poke=None,throws=0):
        for cb in c:
            if cb[0]==slot:cb[1:]=[result,poke or [],throws];return
        c.append([slot,result,poke or [],throws])
    for rank in (0,1,10,11,15,16,32767,32768,65535,65536,M32,0x80000000):
        for block in (0,obj):
            p,e,c=case();setcb(c,18,[block])
            yield [p,e,c,[[149,[S,rank,owner,0x80000000,0x3f800000,0xc0000000,0xffff8001]]]]
    for caster in (0,owner):
        for facing in (0,1,0x222,0x4000,0x7fff,0x8000,0xffff,0xffff8001,M32):
            p,e,c=case();yield [p,e,c,[[150,[obj,0x80000000,0x3f800000,0xc0000000,facing,65535,caster,S]]]]
    for objects in ([],[0],[other],[other,target],[other,other,0,target]):
        for enemy in (0,1):
            p,e,c=case();setcb(c,88,objects);setcb(c,67,[enemy])
            yield [p,e,c,[[150,[obj,0,0,0,0,5,owner,S]]]]
    for frame in (0,10,11,12,112,113,114,0x7fffffff,0x80000000,M32):
        for result in (0,1,2,3):
            for hit in (0,1):
                p,e,c=case();p += [[SK.G_SIM_FRAME,4,frame],[obj+0xb4,4,hit]];setcb(c,100,[result])
                yield [p,e,c,[[151,[obj]]]]
    # Nonwrapped turn, just below/equal/above the 0x222 clamp in both directions,
    # and signed heading endpoints. Position along +x gives atan2=0 exactly.
    for heading in (0,1,0x221,0x222,0x223,0xfffffddd,0xfffffdde,0xfffffddf,0x7fff,0x8000,0xffff):
        for position in ((0x40000000,0xc0000000),(0,0xc0000000),(0x3f800000,0),(0x3f800000,0xc0400000)):
            p,e,c=case();p += [[obj+0x38,2,heading],[other+0x10,4,position[0]],[other+0x14,4,position[1]]]
            yield [p,e,c,[[151,[obj]]]]
    for start in (0,0x7fffffff,0x80000000,M32):
        p,e,c=case();p += [[SK.G_SIM_FRAME,4,start]]
        yield [p,e,c,[[150,[obj,0,0,0,0,5,owner,S]],[151,[obj]]]]
    for rank in (0,10,11,15,16,32767,32768,65535):
        for flags in (0,0x20,0x40,0x21):
            for blocked in (0,0x200,0x10000000200):
                p,e,c=case();p += [[obj+0x9c,2,rank],[other+0xc,4,flags],[other+0xa8,8,blocked]]
                yield [p,e,c,[[152,[obj,other]],[152,[obj,other]]]]
    for handle in (0,0xa000):
        for flags in (0,0x20):
            for enemy in (0,1):
                p,e,c=case();p += [[obj+0x90,4,handle],[other+0xc,4,flags]];setcb(c,67,[enemy])
                yield [p,e,c,[[152,[obj,other]]]]
    for handle in (0,0xa000):
        for steps in (0,1,0xffff,32767):
            for block in (0,0xc000):
                p,e,c=case();p += [[obj+0x90,4,handle],[S+0x1a,2,steps]];setcb(c,18,[block])
                yield [p,e,c,[[153,[obj]]]]
    for seed in (0,1,0x80000000,M32):
        for d0,d1 in ((0,0),(0x8000000000000000,0x3ff0000000000000),
                      (0xbff0000000000000,0x4031000000000000),
                      (0x0000000000000001,0x7fefffffffffffff),
                      (0x7ff0000000000000,0x3ff0000000000000),
                      (0x7ff8000000000001,0x3ff0000000000000)):
            p,e,c=case();p += [[obj+0x44,4,seed],[0x552658,8,d0],[0x552660,8,d1]]
            yield [p,e,c,[[153,[obj]]]]
    # Repeated movement and collision mutation prove old position capture and
    # post-callback frame/hit/expiry reloads. The inherited constructor is external.
    for slot,poke in ((99,[obj+0x10,4,0x41200000]),(100,[obj+0x10,4,0x41200000]),
                      (100,[SK.G_SIM_FRAME,4,113]),(100,[obj+0xb4,4,1]),(100,[obj+0xac,4,0]),
                      (32,[obj+0x10,4,0x41200000])):
        p,e,c=case();setcb(c,slot,[2] if slot==100 else [],poke)
        yield [p,e,c,[[150,[obj,0,0,0,0,5,0,S]],[151,[obj]],[151,[obj]]]]
    operations={149:[S,5,owner,0,0,0,0],150:[obj,0,0,0,0,5,owner,S],
                151:[obj],152:[obj,other],153:[obj]}
    boundaries={149:(14,18,99,19),150:(99,19,88,67,90,71,20,43),
                151:(100,32,26,20,43,18,101,73),152:(67,8,33,10,102),153:(10,18,101,32)}
    for op,slots in boundaries.items():
        for slot in slots:
            for throws in (0,1):
                for address,value in ((obj+0x90,0),(obj+0x98,S+8),(obj+0x9c,16),(other+0xc,0)):
                    p,e,c=case()
                    if slot<16:
                        e=[slot];setcb(c,slot,[10,20] if slot==8 else [2])
                    if op==151 and slot in (20,43,18,101,73):p += [[obj+0xac,4,0]]
                    for cb in c:
                        if cb[0]==slot:cb[2:]=[[address,2 if address==obj+0x9c else 4,value],throws]
                    yield [p,e,c,[[op,operations[op]]]]


def summonbadger_scenarios(SK):
    rng=random.Random(902614)
    spell,owner,badger,node=0x6000,0x7000,0x8000,0xa000
    def case():
        p=setup(SK,rng,0)
        p += [[SK.G_WORLD,4,0xdead],[spell+0x14,4,SK.SummonBadger.SB_VT],
              [SK.SummonBadger.SB_VT+8,2,0xfff8],
              [spell+0x10,4,rng.getrandbits(32)],[spell+0x18,4,node],[node+0xc,4,badger],
              [spell+0x1c,4,0x3f000000],[badger+0xc,4,0x4002],[badger+0xa8,8,0x8002000000000000],
              [owner+0xc,4,0x40],[owner+0x3c8,4,1],[owner+0x48,4,0xd000],
              [0xd000+37*8,2,0xfff0],[badger+0x48,4,0xe000],
              [0xe000+16*8,2,0xfff8],[0xe000+34*8,2,0xfffc],[badger+0xdc,4,0x40000000],
              [badger+0x38,2,0xffff],[badger+0x3c,4,0x3f800000],
              [0x503908,4,0x1122],[0x864ea8,4,0xb000]]
        for i in range(4):p += [[0x5038f8+4*i,4,0x1130+i]]
        for address in (owner,badger):
            for k,v in enumerate((0x3f800000,0xc0000000,0x40400000)):p += [[address+0x10+4*k,4,v]]
        for k in range(12):p += [[0xb000+0x20*k,4,0xb000+0x20*(k+1) if k<11 else 0]]
        results={12:[5],18:[0xc000],20:[456],21:[],27:[],28:[],29:[0x3333],30:[17],
                 31:[0x41200000,0x41a00000,0x41f00000],32:[],70:[0x40400000],
                 79:[],80:[7],103:[0],104:[0x40000000,0x40800000,0x41000000,1],105:[],106:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    def cb(c,slot,result,poke=None,throws=0):
        for row in c:
            if row[0]==slot:row[1:]=[result,poke or [],throws];return
        c.append([slot,result,poke or [],throws])
    for flags in (0,0x40):
        for slot in (0,1,2,3,4,255,M32):
            for found in (0,1,M32):
                p,e,c=case();p += [[owner+0xc,4,flags],[owner+0x3c8,4,slot]];cb(c,103,[found])
                yield [p,e,c,[[154,[S,owner]]]]
    for flags in (0,0x40):
        for rank in (0,1,10,11,255,M32):
            for block in (0,0xc000):
                p,e,c=case();p += [[owner+0xc,4,flags]];cb(c,18,[block])
                yield [p,e,c,[[155,[S,rank,owner,0,0,0,0xffff8001]]]]
    for facing in (0,1,0x3fff,0x4000,0x7fff,0x8000,0xffff,0xffff8001,M32):
        for slot in (0,3,4,M32):
            p,e,c=case();p += [[owner+0x3c8,4,slot]]
            yield [p,e,c,[[155,[S,5,owner,0x80000000,0xc0000000,0x40400000,facing]]]]
    for height in (0xc1a80001,0xc1a80000,0xc1a7ffff,0x41d7ffff,0x41d80000,0x41d80001,
                   0,0x80000000,0x7f800000,0xff800000,0x7fc00001,0x7f800001):
        for found in (0,1,128,256,M32):
            p,e,c=case();cb(c,70,[height]);cb(c,104,[0x40000000,0x40800000,0x41000000,found])
            yield [p,e,c,[[155,[S,5,owner,0,0,0,0]]]]
    for rank in (0,1,127,128,255,256,M32):
        for pet in (0,badger):
            p,e,c=case()
            yield [p,e,c,[[156,[spell,rank,owner,S,pet]],[157,[spell,0,0]]]]
    for clock in (0,0x80000000,0xbf800000,0x3cc00000,0x3ef00000,0x3f7fffff,0x3f800000,
                  0x3f800001,0x3fc00000,0x3fffffff,0x40000000,0x40000001,
                  0x7f800000,0xff800000,0x7fc00001,0x7f800001,1):
        for points in (0,1,8,17,0xffffffff,0x80000000):
            p,e,c=case();p += [[spell+0x1c,4,clock]];cb(c,30,[points])
            yield [p,e,c,[[157,[spell,0,0]]]]
    for handle,pet in ((0,badger),(node,0),(node,badger)):
        p,e,c=case();p += [[spell+0x18,4,handle],[node+0xc,4,pet]]
        yield [p,e,c,[[157,[spell,0,0]]]]
    for seed in (0,1,0x80000000,M32):
        p,e,c=case();p += [[spell+0x10,4,seed]]
        yield [p,e,c,[[157,[spell,0,0]] for _ in range(4)]]
    # Every reachable external boundary: successful mutation and exception prefix.
    ops={154:[S,owner],155:[S,5,owner,0,0,0,0],156:[spell,1,owner,S,badger],157:[spell,0,0]}
    gates={154:(103,),155:(14,70,104,12,18,105,80,79,106,28,20,21),157:(29,30,31,32,27)}
    for op,slots in gates.items():
        for slot in slots:
            for throws in (0,1):
                for address,value in ((owner+0xc,0),(owner+0x3c8,4),(spell+0x1c,0x40000001),
                                      (badger+0xc,0),(badger+0x10,0x41200000)):
                    p,e,c=case()
                    if slot<16:e=[slot];cb(c,slot,[5])
                    if slot==104:cb(c,70,[0x42c80000])
                    if slot==27:p += [[spell+0x18,4,0]]
                    for row in c:
                        if row[0]==slot:row[2:]=[[address,4,value],throws]
                    yield [p,e,c,[[op,ops[op]]]]
    # A callback reloads the owner only where the reference does; per-tick centre
    # remains captured while facing/scale are supplied for every sample call.
    for slot,addr,value in ((29,badger+0x10,0x41200000),(31,badger+0x38,0x8000),
                           (32,badger+0xdc,0x3f000000),(32,spell+0x18,0)):
        p,e,c=case();cb(c,slot,[0x3333] if slot==29 else [1,2,3] if slot==31 else [],[addr,2 if addr==badger+0x38 else 4,value])
        yield [p,e,c,[[157,[spell,0,0]],[157,[spell,0,0]]]]


def root_scenarios(SK):
    rng=random.Random(852614)
    spell,victim,caster=0x6000,0x7000,0x8000
    def case():
        p=setup(SK,rng,0)
        p += [[spell,1,1],[spell+8,4,S],[spell+0xc,4,victim],
              [spell+0x10,4,rng.getrandbits(32)],[spell+0x14,4,SK.Root.ROOT_VT],
              [SK.Root.ROOT_VT+8,2,0xfff8],[spell+0x18,4,0x3f000000],
              [spell+0x1c,4,0],[spell+0x20,4,5],[spell+0x24,4,0x1234],
              [spell+0x28,4,0],[spell+0x2c,4,0],[victim+0xc,4,0x20],
              [victim+0x1c,4,0x42000000],[victim+0x20,4,0x42800000],
              [victim+0x38,2,0xffff],[victim+0x48,4,0xd000],
              [0xd000+16*8,2,0xfff8],[caster+0xc,4,0x40],
              [caster+0x3c8,4,2],[caster+0x48,4,0xe000],[0xe000+20*8,2,0xfff0],
              [S+0x0e,2,1],[S+0x18,2,2],[S+0x24,4,0],
              [S+0x10,2,1],[S+0x1a,2,5],[S+0x28,4,0],
              [S+0x12,2,1],[S+0x1c,2,101],[S+0x2c,4,0]]
        for k,v in enumerate((0x40400000,0x40800000,0x40a00000)):p += [[victim+0x10+4*k,4,v]]
        results={18:[0xa000],20:[456],22:[0x1234],23:[],25:[],27:[],29:[0x3333],30:[17],
                 31:[0x41200000,0x41a00000,0x41f00000],32:[],43:[],60:[456],61:[],
                 67:[1],77:[0],88:[victim],89:[],90:[],48:[]}
        return p,[],[[i,v,[],0] for i,v in results.items()]
    def cb(c,slot,result,poke=None,throws=0):
        for row in c:
            if row[0]==slot:row[1:]=[result,poke or [],throws];return
        c.append([slot,result,poke or [],throws])
    for mode in (0,1,0x80000000,M32):
        for flags in (0,0x20,0x40,0x60):
            p,e,c=case();p += [[SK.G_PVP_MODE_A,4,mode],[caster+0xc,4,flags]]
            yield [p,e,c,[[158,[S,caster]]]]
    for rank in (0,1,20,127,128,255,256,M32):
        for block in (0,0xa000):
            for owner in (0,caster):
                p,e,c=case();cb(c,18,[block]);cb(c,88,[victim,victim+0x1000,victim])
                yield [p,e,c,[[159,[S,rank,owner,1,2,3,0xffff8001]]]]
    for most in (0,1,2,0xffff,0x8000):
        for flags,timer,boss,enemy in ((0,0,0,1),(0x1000000,0,0,1),(0,1,0,1),
                                     (0,0xffff,0,1),(0,0x8000,0,1),(0,0,1,1),(0,0,0,0)):
            p,e,c=case();p += [[S+0x18,2,most],[victim+0xc,4,flags],[victim+0x314,2,timer]]
            cb(c,77,[boss]);cb(c,67,[enemy]);cb(c,88,[victim,victim,victim])
            yield [p,e,c,[[159,[S,1,caster,0,0,0,0x8000]]]]
    for rank in (0,1,127,128,255,256,M32):
        p,e,c=case();yield [p,e,c,[[160,[spell,rank,victim,S]]]]
    clocks=(0,0x80000000,0xbf800000,0x3f004442,0x3f004443,0x3f04dcca,0x3f800000,
            0x40b00000,0x7f800000,0xff800000,0x7fc00001,0x7f800001,1)
    for phase in (0,1,2,M32):
        for clock in clocks:
            for flags in (0x20,0x40):
                p,e,c=case();p += [[spell+0x1c,4,phase],[spell+0x18,4,clock],[victim+0xc,4,flags]]
                yield [p,e,c,[[161,[spell,0,0]]]]
    for chance in (0,1,2,50,100,101,0xffff,0x8000):
        for seed in (0,1,0x80000000,M32):
            p,e,c=case();p += [[S+0x1c,2,chance],[SK.G_RAND_SEED,4,seed],[spell+0x18,4,0x3f04dcca]]
            yield [p,e,c,[[161,[spell,0,0]]]]
    for points in (0,1,17,M32,0x80000000):
        for duration in (0,1,5,M32,0x80000000):
            p,e,c=case();p += [[spell+0x1c,4,1],[spell+0x20,4,duration]];cb(c,30,[points])
            yield [p,e,c,[[161,[spell,0,0]]]]
    for seed in (0,1,0x80000000,M32):
        p,e,c=case();p += [[spell+0x10,4,seed]]
        yield [p,e,c,[[161,[spell,0,0]] for _ in range(4)]]
    # All reachable callback boundaries preserve ordered failure/mutation prefixes.
    ops={158:[S,caster],159:[S,1,caster,0,0,0,0],160:[spell,1,victim,S],161:[spell,0,0]}
    gates={158:(60,61),159:(14,20,43,10,88,67,77,18,22,89,90),
           160:(22,10),161:(23,48,32,25,10,27,29,30,31)}
    for op,slots in gates.items():
        for slot in slots:
            for throws in (0,1):
                for address,value in ((spell+0xc,caster),(spell+0x18,0x41000000),
                                      (spell+0x20,M32),(victim+0xc,0x40)):
                    p,e,c=case()
                    if op==158:p += [[SK.G_PVP_MODE_A,4,1]]
                    if slot<16:e=[slot];cb(c,slot,[5])
                    if op==161 and slot in (29,30,31):p += [[spell+0x1c,4,1]]
                    if op==161 and slot==27:p += [[spell+0x1c,4,2],[spell+0x20,4,M32]]
                    for row in c:
                        if row[0]==slot:row[2:]=[[address,4,value],throws]
                    yield [p,e,c,[[op,ops[op]]]]


def coneofshock_scenarios(SK):
    rng=random.Random(1012614)
    spell,owner,other=0x6000,0x7000,0x8000
    def case():
        p=setup(SK,rng,0)+[[spell,1,8],[spell+8,4,S],[spell+0xc,4,owner],
            [spell+0x10,4,rng.getrandbits(32)],[owner+0x48,4,0xd000],
            [0xd000+160,2,0xfff0],[SK.G_SIM_FRAME,4,0]]
        for i,v in enumerate((0x40400000,0x40800000,0x40a00000)):p.append([owner+0x10+4*i,4,v])
        c=[[i,v,[],0] for i,v in {12:[40],14:[8],18:[0xa000],19:[],20:[456],21:[789],26:[],33:[],67:[1],88:[other,other+0x100],90:[],107:[]}.items()]
        return p,[12,14],c
    def cb(c,slot,result,poke=None,throws=0):
        for row in c:
            if row[0]==slot:row[1:]=[result,poke or [],throws];return
        c.append([slot,result,poke or [],throws])
    for rank in (0,1,4,20,127,128,255,256,M32):
        for block in (0,0xa000):
            p,e,c=case();cb(c,14,[rank]);cb(c,18,[block])
            yield [p,e,c,[[162,[S,rank,owner,0,0,0,M32]]]]
        p,e,c=case();yield [p,e,c,[[163,[spell,rank,owner,S]]]]
    for rank in (0,1,3,4,7,8,20,24,28,32,100,119,120,123,124,127,128,132,136,200,248,249,252,255):
        for frame in (0,1,7,10,14,15,29,30,60,0x7fffffff,0x80000000,M32):
            p,e,c=case();p += [[spell,1,rank],[SK.G_SIM_FRAME,4,frame]]
            yield [p,e,c,[[164,[spell,0x40400000,0xc0800000,0x80000000,0x1ffff]]]]
    for facing in (0,1,16384,32767,32768,65535,65536,M32):
        for amount in (0,1,3,4,0x7fffffff,0x80000000,M32):
            p,e,c=case();cb(c,12,[amount])
            yield [p,e,c,[[164,[spell,0,0,0,facing]]]]
    for seed in (0,1,0x80000000,M32):
        for objects in ([],[other],[other,other,other+0x100]):
            for enemy in (0,1,M32):
                p,e,c=case();p += [[spell+0x10,4,seed]];cb(c,88,objects);cb(c,67,[enemy]);cb(c,18,[0])
                yield [p,e,c,[[164,[spell,0,0,0,0]] for _ in range(3)]]
    ops={162:[S,8,owner,0,0,0,0],163:[spell,8,owner,S],164:[spell,0,0,0,0]}
    gates={162:(14,18,19,20,21),163:(19,20,21),164:(12,18,107,88,67,33,20,21,90,26)}
    for op,slots in gates.items():
        for slot in slots:
            for failure in (0,1):
                p,e,c=case()
                row=next(row for row in c if row[0]==slot);row[3]=1
                if failure:row[2]=[spell+0x10,4,M32]
                yield [p,e,c,[[op,ops[op]]]]
    # Callback mutations exercise captures versus reloads, signed division, and
    # the second frame gate after a decoration callback changes the global clock.
    for slot in (12,18,107,88,67,33,20,21,90,26):
        for addr,value in ((SK.G_SIM_FRAME,1),(SK.G_SIM_FRAME,15),(spell,248),
                           (spell+8,S+0x100),(spell+0xc,owner+0x100),(spell+0x10,M32)):
            p,e,c=case();row=next(row for row in c if row[0]==slot);row[2]=[addr,4,value]
            yield [p,e,c,[[164,[spell,0x7f800000,0x7fc00000,0x80000000,32768]]]]
    for slot in (19,20,21):
        for addr in (owner+0x10,spell+0xc):
            p,e,c=case();row=next(row for row in c if row[0]==slot);row[2]=[addr,4,0x40000000]
            yield [p,e,c,[[163,[spell,8,owner,S]]]]
    # Exercise built-in statistics independently of forced callbacks.
    for variant in range(24):
        p,e,c=case();p += setup(SK,rng,variant);e=[]
        p += [[spell,1,variant],[SK.G_SIM_FRAME,4,variant*15]]
        yield [p,e,c,[[164,[spell,0,0,0,variant*997]]]]


def stack_handle_scenarios(SK):
    # The reference delegates stack lists. Authored services retain return words,
    # mutations and exception prefixes; they never allocate invented stack nodes.
    for op,slot in ((165,108),(166,90)):
        for result in ([],[0],[1],[M32],[0x80000000],[1,2,3]):
            for throwing in (0,1):
                for poke in ([],[SK.G_HANDLE_FREE,4,0x1234],[OBJ,2,0xffff]):
                    commands = [[op,[0,OBJ,M32] if op==165 else [0]]]*2
                    yield [[[SK.G_HANDLE_FREE,4,NODE],[NODE,4,0]],[],
                           [[slot,result,poke,throwing]],commands]
    # Explicit stack selection must coexist with memory-backed address zero.
    yield [[[SK.G_HANDLE_FREE,4,0]],[],[[108,[0x5678],[],0],[90,[],[],0]],
           [[165,[0,0,0]],[166,[0]],[26,[0,0,0]],[28,[0]]]]


def general_helper_scenarios(SK):
    # Every pinned EXTERNALS signature; outputs consumed even when nullable
    # arguments suppress them, including buffers and output-only fields.
    signatures=sorted({(ext.ints,ext.ret) for ext in SK.EXTERNALS.values()})
    for ints,ret in signatures:
        for flag in (0,1,M32):
            for outs in ([],[7],[(1,M32,0),[9,8],b'abc',0,[],(7,)],
                         [0]*20,[b'\x00\xff']*20):
                arguments=[flag]*64
                words=[ord(ret),len(ints),*map(ord,ints),len(arguments)]
                words += [n for value in arguments for n in value_words(value)]
                words += value_words(M32)+value_words(0x80000000)+[len(outs)]
                words += [n for value in outs for n in value_words(value)]
                yield [[],[],[],[[167,words]]]
    values=(0,1,M32,2,0xfffffffe,0x7fffffff,0x80000000,0x80000001,17,0xfffffff1)
    for a in values:
        for b in values:
            yield [[],[],[],[[168,[a,b]],[169,[a,b]]]]
    slots=([],[0],[M32],[0xffff,0x10000,0x80000000,M32],list(range(7)))
    for count in range(7):
        for offset in range(len(slots)):
            selected=[slots[(offset+k)%len(slots)] for k in range(count)]
            words=[count]+[n for slot in selected for n in [len(slot),*slot]]
            yield [[],[],[],[[170,words]]]


def text_words(text):return [len(text),*text.encode('ascii')]

def base_dispatch_scenarios(SK,previous):
    # General call: every distinct EXTERNALS result signature, both the exact
    # no-handler defaults and complete typed callback values and failure prefixes.
    signatures=sorted({(ext.ints,ext.ret) for ext in SK.EXTERNALS.values()})
    for ints,ret in signatures:
        for flag in (0,1,M32):
            values=[flag]*64
            words=text_words('authored')+text_words(ints)+[ord(ret),len(values)]
            words += [n for v in values for n in value_words(v)]
            yield [[],[],[],[[171,words]]]
    values=(None,0,M32,(1,M32),[0,(3,4)],b'\x00\xff',('buf',b'abc'))
    for value in values:
        for throwing in (0,1):
            for poke in ([],[OBJ,4,M32]):
                words=text_words('authored')+text_words('')+[ord('i'),3]+value_words(M32)+value_words((M32,7))+value_words(None)
                yield [[],[],[[109,value_words(value),poke,throwing]],[[171,words]]*2]
    # Preserve every earlier case; select existing complete lifecycle scenarios
    # and route their run commands through all 18 locally owned spell bodies.
    routes={33:0x314618,38:0x3151b4,44:0x317ec0,136:0x317000,147:0x2fd5d4,
            164:0x31bcd0,131:0x30e6f0,142:0x3003d8,104:0x301a34,120:0x319068,
            157:0x310cd0,125:0x31399c,59:0x3034dc,81:0x2feddc,82:0x2ffad4,
            74:0x2e90b8,90:0x2e7acc,161:0x2e699c}
    counts={op:0 for op in routes}
    for target,body in routes.items():
        for p,e,c,commands in previous:
            selected=[a for op,a in commands if op==target]
            if not selected or len({a[0] for a in selected})!=1:continue
            if counts[target]>=4:break
            counts[target]+=1;table=0x71000000
            p=copy.deepcopy(p)+[[selected[0][0]+0x20000000,4,table],[table+16,2,0],[table+20,4,body]]
            translated=[]
            for index,(op,a) in enumerate(commands):
                if op!=target:
                    translated.append([op,a]);continue
                pos=0x70000000+index*16
                coords=a[1:4] if len(a)==5 else [0,0,0]
                p += [[pos+4*k,4,v] for k,v in enumerate(coords)]
                facing=a[4] if len(a)==5 else a[2]
                translated.append([173,text_words('Spell')+[a[0],0x20000000,2,pos,facing]])
            yield [p,e,c,translated]
    assert all(counts.values()),counts
    # Unknown and forced table dispatch: signed16 adjustments, arbitrary class
    # names, offsets, raw32 wrapping, callback mutations and throwing prefixes.
    for cls,slot,id in (('Spell',2,110),('Holder',2,111),('Other',3,112)):
        for obj in (0,1,0x6000,0xfffffff0):
            for adjustment in (0,1,32767,32768,65535):
                for throwing in (0,1):
                    offset=0 if cls=='Holder' else 0x14 if obj<0xfffffff0 else 0
                    p=[[obj+offset,4,TABLE],[TABLE+8*slot,2,adjustment],[TABLE+8*slot+4,4,0xdeadbeef]]
                    args=[0x70000000,M32] if cls=='Spell' else [M32,0x80000000,7]
                    yield [p,[],[[id,value_words((M32,7)),[OBJ,4,M32],throwing]],
                           [[173,text_words(cls)+[obj,offset,slot,*args]]]*2]
    for body in routes.values():
        p=[[0x6014,4,TABLE],[TABLE+16,2,65535],[TABLE+20,4,body]]
        yield [p,[110],[[110,value_words(None),[],0]],[[173,text_words('Spell')+[0x6000,0x14,2,0,M32]]]]
    # skill_call: every base/local body at an unusual slot, plus unknown slots
    # beyond the legacy 0..15 wrapper. Known bodies never call Skill.v16.
    for body,(slot,method) in SK.SKILL_BODIES.items():
        p=setup(SK,random.Random(body),0)
        p += [[TABLE+16*8,2,0],[TABLE+16*8+4,4,body]]
        args={1:[0],2:[0,0,0,0],5:[0],8:[0,0,0,0],10:[0,0],11:[0,0,0],
              12:[0,0,0,0],13:[0,0,0,0],14:[0,0],15:[0,0]}[slot]
        # all external services are explicit synthetic callbacks; casts often
        # stop at the allocation boundary, while no-op/default routes run.
        callbacks=[[id,[0]*6 if id==36 else [0],[],0] for id in range(16,109)]
        callbacks += [[k,[0,0] if k==8 else [0],[],0] for k in (8,10,12,14,15)]
        yield [p,[8,10,12,14,15],callbacks,[[172,[S,16,*args]]]]
    for adjustment in (0,32767,32768,65535):
        p=[[S+0x60,4,TABLE],[TABLE+16*8,2,adjustment],[TABLE+16*8+4,4,0xdeadbeef]]
        for throwing in (0,1):
            yield [p,[],[[113,value_words((M32,0)),[OBJ,4,7],throwing]],[[172,[S,16,M32,0x80000000]]]*2]


def tagar_scenarios(SK):
    spell,owner,target,other,node,marker=0x6000,0x7000,0x8000,0x9000,0xa000,0xb000
    def case(seed=1):
        p=[[spell,1,8],[spell+8,4,S],[spell+0xc,4,owner],[spell+0x10,4,seed],
           [spell+0x14,4,0xc000],[0xc008,2,0],[0xc00c,4,0xdeadbeef],
           [spell+0x80,2,0xffff],[spell+0x7c,4,0x4000101f],
           [owner+0xc,4,0x40],[owner+0x48,4,0xd000],[owner+0x310,1,255],
           [0xd000+20*8+4,4,0x119328],[0xd000+37*8+4,4,0x3c2730],
           [target+0xc,4,0x20],[target+0x48,4,0xe000],[target+0x310,1,1],
           [other+0xc,4,0x20],[other+0x48,4,0xe000],[other+0x310,1,1],
           [0xe000+37*8+4,4,0x3c2730],[target+0x10,4,0x41200000],
           [other+0x10,4,0x41a00000],[target+0x1c,4,0x41400000],[target+0x20,4,0x42400000],
           [node+0xc,4,target],[marker+0x14,4,0xf000],[0xf008,2,0xffff],
           [S+0x60,4,TABLE],[SK.G_RAND_SEED,4,seed],[SK.G_SIM_FRAME,4,seed]]
        c=[[12,[8],[],0],[14,[8],[],0],[8,[3,7],[],0],
           [18,[marker],[],0],[20,[123],[],0],[21,[],[],0],[32,[],[],0],
           [33,[],[],0],[27,[],[],0],[114,[0],[],0],
           [88,[target,other],[],0],[91,[other,target],[],0],[90,[],[],0],
           [108,[node],[spell+0x18,4,node],0],[67,[1],[],0],[80,[2],[],0],[89,[],[],0]]
        return p,[8,12,14],c
    # Cast/constructor: allocation boundaries, signed adjustment, levels, raw angles,
    # zero and nonzero owner, stale fields and callback failures in call order.
    for seed in (0,1,0x80000000,M32):
        for rank in (0,127,128,256,M32):
            for facing in (0,0x4000,0x8000,0xffff):
                p,e,c=case(seed);p += [[owner+0x38,2,facing],[owner+0x10,4,0x3f800000],
                                     [owner+0x14,4,0xbf800000],[owner+0x18,4,0x40400000],
                                     [spell+0x34,4,M32],[spell+0x24,4,M32]]
                yield [p,e,c,[[175,[spell,rank,owner,S]],[179,[spell,0,0]]]]
    for allocated in (0,spell):
        for virtual in (False,True):
            p,e,c=case();c[0][1]=[0];c[3][1]=[allocated]
            p += [[TABLE+16,2,0xfff0],[TABLE+20,4,0x30c708],[S-16+0x60,4,TABLE]]
            args=[S,5,owner,0x3f800000,0xbf800000,0x40400000,M32]
            command=[174,args] if not virtual else [172,[S,2,5,owner,0x100000,M32]]
            if virtual:p += [[0x100000+4*k,4,v] for k,v in enumerate(args[3:6])]
            yield [p,e,c,[command]]
    # Validity: every early exit and priority result, base team bytes, forced and
    # unknown virtual callbacks, and signed16 this adjustment.
    for flags in (0,1,8,9,0x20,0x40,0x60,0x20020,0x20008):
        for cflags in (0,0x800,0x4000000,0x100000000000):
            for team in (1,255):
                p,e,c=case();p += [[target+0xc,4,flags],[target+0xa8,8,cflags],[target+0x310,1,team]]
                yield [p,e,c,[[176,[spell,target,0]],[176,[spell,target,1]]]]
    for obj in (0,owner,target):
        for marked in (0,marker):
            p,e,c=case();c[9][1]=[marked]
            yield [p,e,c,[[176,[spell,obj,1]]]]
    for slot,id,body in ((20,67,0x119328),(37,80,0x3c2730)):
        for forced in (0,1):
            for adjustment in (0,32767,32768,65535):
                p,e,c=case();p += [[0xd000+8*slot,2,adjustment]]
                if not forced:p += [[0xd000+8*slot+4,4,0xdeadbeef]]
                else:e += [id]
                yield [p,e,c,[[176,[spell,target,1]]]]
    # First target uses list order; later uses strict 3D distance, duplicate
    # handles and ties; both clear delegated stack lists even when empty.
    for later in (0,1):
        for found in ([],[target],[other,target],[target,other],[0,owner,target], [target,target,other]):
            for duplicate in (0,1):
                p,e,c=case();c[10][1]=found;c[11][1]=found
                if duplicate:p += [[spell+0x82,2,1],[spell+0x18,4,node]]
                yield [p,e,c,[[177,[spell,later]]]]
        for x,y,z in ((0,0,0),(0x41200000,0,0),(0x49742400,0,0),
                      (0,0x41200000,0),(0,0,0x41200000),(0x7fffffff,0,0)):
            p,e,c=case();p += [[other+0x10,4,x],[other+0x14,4,y],[other+0x18,4,z]]
            yield [p,e,c,[[177,[spell,later]]]]
    # Victim tick: empty/missing handles, negative/expiry/equal/active times,
    # markers, overdue damage, player and nonplayer ranges, multiple victims.
    for count in (0,1,2,8,65535):
        for expiry in (0xbf800000,0,0x3f800000,0x40000000):
            for clock in (0,0x3f800000):
                for player in (0,1):
                    p,e,c=case(12345);p += [[spell+0x82,2,count],[spell+0x78,4,clock],
                                          [owner+0xc,4,0x40 if player else 0x20]]
                    for k in range(min(count,8)):
                        p += [[spell+0x18+4*k,4,node if k%3!=2 else 0],
                              [spell+0x38+4*k,4,expiry],[spell+0x58+4*k,4,0 if k%2==0 else 0x40000000]]
                    yield [p,e,c,[[178,[spell]]]]
    for marked in (0,marker):
        p,e,c=case();c[9][1]=[marked]
        p += [[spell+0x82,2,1],[spell+0x18,4,node],[spell+0x38,4,0]]
        yield [p,e,c,[[178,[spell]]]]
    # Full runs: early hunt expiry, limit, acquisition, lost/invalid current
    # target, zero-distance arrival, marker allocation, heading reflection,
    # grow clamp and continued ticks. Fixed seeds cover both wander branches.
    for seed in (0,1,3,999,0x80000000,M32):
        for current in (0xffff,0):
            for distance in (0,0x3f000000,0x41200000,0x42c80000,0xc2c80000):
                for alloc in (0,marker):
                    p,e,c=case(seed);c[3][1]=[alloc]
                    p += [[spell+0x80,2,current],[spell+0x18,4,node],
                          [spell+0x38,4,0x40000000],[target+0x10,4,distance],
                          [target+0x14,4,distance],[spell+0x84,2,seed&65535]]
                    yield [p,e,c,[[179,[spell,0,M32]]]*2]
    for clock,hunt,limit,count in ((0,0,8,0),(0,0,8,1),(0,0x40000000,0,0),
                                  (0,0x40000000,M32,0),(0x3f800000,0x40800000,8,8),
                                  (0x3f000000,0x40800000,8,0)):
        p,e,c=case();c[0][1]=[limit];c[10][1]=[];c[11][1]=[]
        p += [[spell+0x78,4,clock],[spell+0x7c,4,hunt],[spell+0x82,2,count]]
        yield [p,e,c,[[179,[spell,0,0]]]]
    # Virtual run at unusual slot with adjustment and distinct table offset.
    p,e,c=case();p += [[spell+0x100,4,0xc000],[0xc000+8*3,2,1],
                     [0xc000+8*3+4,4,0x30cbc8],[spell+1+0x7c,4,0]]
    yield [p,e,c,[[173,text_words('Spell')+[spell,0x100,3,0,0]]]]
    # Forced inherited Skill body delegates with the adjusted this; no cast stores.
    for adjustment in (0,32767,32768,65535):
        for throwing in (0,1):
            p,e,c=case();p += [[TABLE+16,2,adjustment],[TABLE+20,4,0x30c708]]
            e += [2];c += [[2,[M32],[owner+0x10,4,1],throwing]]
            yield [p,e,c,[[172,[S,2,8,owner,0,M32]]]]
    for throwing in (0,1):
        p,e,c=case();p += [[TABLE+16*8,2,0xffff],[TABLE+16*8+4,4,0x30c708],
                         [0x100000,4,0x3f800000],[0x100004,4,0xbf800000],[0x100008,4,0x40400000]]
        e += [113];c += [[113,value_words(M32),[owner+0x10,4,1],throwing]]
        yield [p,e,c,[[172,[S,16,8,owner,0x100000,M32]]]]
    # More than one active victim, signed durations/ranges, invalid current
    # handles, expiry with a live victim and the steering quarter-turn boundary.
    for heading in (0,0x3fff,0x4000,0x7fff,0x8000,0xbfff,0xc000,0xffff):
        for flags in (0x20,1,8):
            p,e,c=case(321);p += [[spell+0x84,2,heading],[spell+0x80,2,0],
                                [spell+0x18,4,node],[spell+0x38,4,0x40000000],
                                [target+0xc,4,flags]]
            c[10][1]=[];c[11][1]=[]
            yield [p,e,c,[[179,[spell,0,0]]]]
    for value in (M32,0x80000000,0x7fffffff,0):
        p,e,c=case(999);c[0][1]=[value];c[2][1]=[value,value]
        p += [[spell+0x82,2,2],[spell+0x18,4,node],[spell+0x1c,4,node],
              [spell+0x38,4,0x40000000],[spell+0x3c,4,0x40000000],[spell+0x7c,4,0]]
        yield [p,e,c,[[178,[spell]],[179,[spell,0,0]]]]
    # Throwing services retain full preceding memory/call/RNG observations.
    for operation,args in ((174,[S,5,owner,0,0,0,0]),(176,[spell,target,1]),
                           (177,[spell,0]),(178,[spell]),(179,[spell,0,0])):
        for id in (12,14,18,20,21,32,33,27,114,88,91,90,108,67,80,89,8):
            p,e,c=case();p += [[spell+0x82,2,1],[spell+0x18,4,node],[spell+0x38,4,0x40000000]]
            for row in c:
                if row[0]==id:row[2]=[owner+0x10,4,M32];row[3]=1
            yield [p,e,c,[[operation,args]]]


def skeleton_scenarios(SK):
    obj,owner,spell=0x6000,0x7000,0x8000
    def case(seed=1):
        p=[[S+0x60,4,TABLE],[SK.G_RAND_SEED,4,seed],[0x404460,4,seed],
           [owner+0xc,4,0x40],[owner+0x48,4,0x9000],[owner+0x310,1,1],
           [0x9000+37*8+4,4,0x3c2730],[owner+0x10,4,0x3f800000],
           [owner+0x18,4,0x41c00000],[owner+0x38,2,0xffff],
           [obj+0x24,4,0xa000],[obj+0x18,4,0x41c00000],
           [0xa00c,4,0xabc],[0xa01c,4,0xb000],[0xa024,4,0xc000],
           [0xa034,4,0xd000],[0xa03c,4,0xe000],[0xa044,4,0xf000],
           [0x503788,4,17],[0x503778,4,19],[0x4f714c,4,0x42280000],
           [0x44fdcc,4,seed],[0x4f85c4,1,255],
           [spell+0x14,4,0x11000],[0x11008,2,0xffff]]
        c=[[18,[obj],[],0],[20,[123],[],0],[21,[],[],0],[27,[],[],0],
           [29,[321],[],0],[30,[0],[],0],[31,[0x3f800000,0xbf800000,0x40400000],[],0],
           [32,[],[],0],[70,[0x41c00000],[],0],[103,[0],[],0],[104,[1,2,3,1],[],0],
           [12,[5],[],0],[115,[0],[],0],[116,[0],[],0],[117,[],[],0],[118,[],[],0],
           [119,[],[],0],[120,[],[],0],[121,[],[],0],[122,[],[],0],[123,[],[],0],
           [85,[],[],0],[87,[0x89abcdef,0xfedcba98],[],0],[71,[],[],0],[90,[],[],0],
           [80,[2],[],0],[106,[],[],0],[108,[0],[],0],[111,value_words(None),[],0],
           [125,[],[],0],[126,[],[],0],[69,[],[],0],[79,[],[],0]]
        return p,[12],c
    # Name lookup preserves every early exit and signed-slot-as-unsigned check.
    for flags in (0,0x20,0x40,0x60):
        for slot in (0,1,2,3,4,M32):
            for found in (0,obj):
                p,e,c=case();p += [[owner+0xc,4,flags],[owner+0x3c8,4,slot]]
                c[9][1]=[found]
                yield [p,e,c,[[180,[S,owner]]]]
    # Separate cosmetic stream, rejection sampling and signed point counts.
    for seed in (0,1,3,321,999,0x80000000,M32):
        p,e,c=case(seed);yield [p,e,c,[[185,[0]],[186,[0]]]*2]
        for count in (0,1,2,7,M32,0x80000000):
            p,e,c=case(seed);yield [p,e,c,[[184,[owner,count]]]]
    # Construction: stale memory, rank narrowing, flags, properties, case-insensitive
    # bounded strings, temporary global, callback failure prefixes and filter outcomes.
    for rank in (0,1,127,128,256,M32,0x80000000):
        for angle in (0,0x4000,0x8000,0xffff):
            p,e,c=case();p += [[obj+off,8,0xdeadbeefcafebabe] for off in (0xa8,0x220,0x360,0x450)]
            yield [p,e,c,[[187,[obj,0x3f800000,0xbf800000,0x40400000,angle,0x503770,rank]]]]
    for value in (1,M32,0x80000000,32768):
        p,e,c=case();c[12][1]=[value];c[13][1]=[value]
        yield [p,e,c,[[187,[obj,1,2,3,M32,0,5]]]]
    for first,second in ((b'Alpha',b'aLPHA'),(b'A',b'B'),(b'X'*64,b'x'*64),(b'A\0B',b'a')):
        p,e,c=case();p += [[addr+k,1,b] for addr,txt in ((0x44fe18,first),(0x54c080,second)) for k,b in enumerate(txt)]
        yield [p,e,c,[[187,[obj,0,0,0,0,0,5]]]]
    for offset,value in ((0x314,1),(0x31c,1),(0x318,1),(0x234,1),(0x24,0x502728)):
        for target in (0,1):
            p,e,c=case();c[16][2]=[obj+0x234,4,target];c[17][2]=[obj+offset,4,value]
            yield [p,e,c,[[187,[obj,0,0,0,0,0,5]]]]
    p,e,c=case();p += [[0x4f7120,4,1],[0x503890,4,1]]
    yield [p,e,c,[[187,[obj,0,0,0,0,0,5]]]]
    for id in (69,115,116,117,85,118,87,119,120,122,123,71,121):
        p,e,c=case();
        for row in c:
            if row[0]==id:row[2]=[obj+0x450,4,M32];row[3]=1
        # GameObject_ctor is a real callback, no seed/name effects fabricated.
        if id==69:c += [[69,[],[obj+0x450,4,M32],1]]
        else:c += [[69,[],[],0]]
        yield [p,e,c,[[187,[obj,0,0,0,0,0,5]]]]
    # Spell owns one shared draw and preserves handle callee mutations.
    for rank in (0,127,128,256,M32):
        for skel in (0,obj):
            for head in (0,0x123abc):
                p,e,c=case();c[27][2]=[spell+0x18,4,head]
                yield [p,e,c,[[182,[spell,rank,owner,S,skel]],[183,[spell,0,0]]]]
    for ptr in (0,spell):
        p,e,c=case();yield [p,e,c,[[183,[ptr,0,M32]]]]
    # Cast: allocation failures and success, monster/player/invalid slot, ranks,
    # both terrain threshold sides, signed-byte free-spot return and RNG chance.
    for allocated in (0,obj):
        for flags in (0x20,0x40):
            for slot in (0,3,4,M32):
                for rank in (0,1,M32):
                    p,e,c=case(123);c += [[69,[],[],0]];c[0][1]=[allocated]
                    p += [[owner+0xc,4,flags],[owner+0x3c8,4,slot]]
                    yield [p,e,c,[[181,[S,rank,owner,0,0,0,0x8000]]]]
    for height in (0,0x423fffff,0x42400000,0x42400001,0xc2c80000,0x7fffffff):
        for found in (0,1,128,256):
            p,e,c=case();c += [[69,[],[],0]];c[8][1]=[height];c[10][1]=[1,2,3,found]
            yield [p,e,c,[[181,[S,5,owner,0x3f800000,0xbf800000,0x40400000,M32]]]]
    for seed in (0,1,3,321,999,0x80000000,M32):
        p,e,c=case(seed);c += [[69,[],[],0]];c[11][1]=[100]
        yield [p,e,c,[[181,[S,1,owner,0,0,0,0]]]]
    # Full particle branches run all 232 particles for positive and negative counts.
    for seed in (0,1,M32):
        for count in (1,7,M32):
            p,e,c=case(seed);c[5][1]=[count];c[0][1]=[0]
            yield [p,e,c,[[181,[S,5,owner,0,0,0,0xffff]]]]
    for id in (29,30,31,32,70,104,12,18,20,21,108,80,106):
        p,e,c=case();c += [[69,[],[],0]];c[5][1]=[1] if id in (31,32) else [0]
        for row in c:
            if row[0]==id:row[2]=[obj+0x450,4,M32];row[3]=1
        yield [p,e,c,[[181,[S,5,owner,0,0,0,0]]]]
    # Inherited routes select by body address, with signed adjustment and arbitrary slot.
    for body,slot,args in ((0x2e2368,1,[owner]),(0x2e18a4,2,[5,owner,0x12000,0xffff])):
        for adj in (0,0xffff,0x8000,0x7fff):
            for forced in (0,1):
                p,e,c=case();c += [[69,[],[],0]];p += [[TABLE+slot*8,2,adj],[TABLE+slot*8+4,4,body]]
                if forced:e += [slot];c += [[slot,[7],[],0]]
                yield [p,e,c,[[172,[S,slot,*args]]]]
    p,e,c=case();p += [[spell+0x100,4,0x11000],[0x11000+24,2,0xffff],[0x11000+28,4,0x2e177c]]
    yield [p,e,c,[[173,text_words('Spell')+[spell,0x100,3,0,0]]]]

    for adjustment in (0,32767,32768,65535):
        for forced in (0,1):
            p,e,c=case();p += [[0x9000+16*8,2,adjustment],[0x9000+16*8+4,4,0x3c2738],
                             [(owner+SK.s16(adjustment))&M32,4,0],
                             [((owner+SK.s16(adjustment))&M32)+0x24,4,0xa000]]
            if forced:e += [29]
            yield [p,e,c,[[184,[owner,7]]]]
            p,e,c=case();p += [[0x5174b0+34*8,2,adjustment],[0x5174b0+34*8+4,4,0x3c2708]]
            if forced:e += [106]
            yield [p,e,c,[[181,[S,1,owner,0,0,0,0]]]]
            p,e,c=case();p += [[TABLE+16*8,2,adjustment],[TABLE+16*8+4,4,0x2e18a4],
                             [0x12000,4,0x3f800000],[0x12004,4,0xbf800000],[0x12008,4,0x40400000]]
            if forced:e += [113];c += [[113,value_words(M32),[],0]]
            yield [p,e,c,[[172,[S,16,5,owner,0x12000,M32]]]]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=pathlib.Path)
    parser.add_argument('--output',required=True,type=pathlib.Path)
    args=parser.parse_args()
    pin=json.loads(EVIDENCE.read_text())
    # Scratch files are authored Python contracts retrieved from exact Git
    # objects, never copied from the research working tree or committed here.
    (ROOT/'work').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT/'work',prefix='skills-c-pin-') as tmp:
        for path,digest in pin['files'].items():
            data=subprocess.check_output(['git','-C',str(args.research_dir),'show',pin['commit']+':'+path])
            if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('reference hash mismatch: '+path)
            if path.startswith('tools/ref_'): pathlib.Path(tmp,pathlib.Path(path).name).write_bytes(data)
        tree=ast.parse(pathlib.Path(tmp,'ref_skills_c.py').read_text())
        units={node.name for node in tree.body if isinstance(node,ast.FunctionDef)}
        units.update(method.name for node in tree.body if isinstance(node,ast.ClassDef)
                     for method in node.body if isinstance(method,ast.FunctionDef))
        ported=set(METHODS)&units
        ported.update({'_round','_tier_value','_rage','_h_take','_h_unlink_free'})
        assert len(units)==175 and len(ported)==175 and ported<=units
        print(f'PROGRESS: {len(ported)} of {len(units)} reference functions ported',flush=True)
        sys.path.insert(0,tmp)
        SK=importlib.import_module('ref_skills_c'); RC=importlib.import_module('ref_creature'); C=importlib.import_module('ref_core')
        cases=list(scenarios(SK)) + list(runtime_scenarios(SK)) + list(protection_scenarios(SK)) + list(bloodlust_scenarios(SK)) + list(shared_classes_scenarios(SK)) + list(summon_items_scenarios(SK)) + list(criticalhit_scenarios(SK)) + list(thrown_axes_scenarios(SK)) + list(battlecry_scenarios(SK)) + list(storm_scenarios(SK)) + list(diseaseblade_scenarios(SK)) + list(raisearmy_scenarios(SK)) + list(drowsy_scenarios(SK)) + list(waveofflame_scenarios(SK)) + list(roar_scenarios(SK)) + list(unspokenword_scenarios(SK)) + list(poisonbolt_scenarios(SK)) + list(summonbadger_scenarios(SK)) + list(root_scenarios(SK)) + list(coneofshock_scenarios(SK)) + list(stack_handle_scenarios(SK)) + list(general_helper_scenarios(SK))
        cases += list(base_dispatch_scenarios(SK,cases))
        cases += list(tagar_scenarios(SK))
        cases += list(skeleton_scenarios(SK))
        lines=['# skills_c_vectors_v1\n']
        for config in range(16):
            for case in cases:
                lines.append(f'{config}\t{encode(case)}\t{observe(SK,RC,C,config,case)}\n')
    data=''.join(lines).encode('ascii')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('wb') as f:
        with gzip.GzipFile(filename='',fileobj=f,mode='wb',mtime=0) as z:z.write(data)
    manifest={'schema_version':1,'suite':'skills-c','rows':len(lines)-1,'scenario_count':len(cases),
              'vector_sha256':hashlib.sha256(data).hexdigest(), 'evidence':pin,
              'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
              'protocol':'config TAB recursive count-prefixed integer input TAB complete observation; see docs/SKILLS_C.md'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(lines)-1} exact complete-state vectors')


if __name__=='__main__':main()
