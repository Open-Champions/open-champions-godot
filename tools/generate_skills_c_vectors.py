#!/usr/bin/env python3
"""Execute the immutable RTA statistics/passive contract on authored sparse memory.

No game files, retail tables, decompiler text, or native results are read.
"""
import argparse
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
           'diseaseblade_spell_ctor','diseaseblade_spell_run')
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
            'Creature.v1','LightningBolt_ctor','Holder.v7','Holder.v8')

def callback_name(slot):
    return 'Skill.v%d' % slot if slot < 16 else SERVICES[slot-16]

def callback_id(name):
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


def snapshot(model, configs):
    return copy.deepcopy([sorted(model.mem.bytes.items()), model.mem.log, model.writes,
            [[callback_id(name), call_words(args)] for name, args in model.calls],
            model.draws, sorted(callback_id(name) for name in model.external), configs])


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
            if slot == 36 and len(result) == 6:
                return tuple(result[:3]), tuple(result[3:])
            return result[0] if len(result) == 1 else tuple(result)
        callbacks[callback_name(slot)] = callback
    model = SK.Model(mem, callbacks, [callback_name(slot) for slot in externals])
    out = [snapshot(model, configs)]
    for operation, args in commands:
        try:
            arguments = list(args)
            if operation == 33: arguments = [args[0],tuple(args[1:4]),args[4]]
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
            elif operation == 120: arguments=[args[0],tuple(args[1:4]),args[4]]
            elif operation == 92: arguments=[args[0],tuple(args[1:5]),*args[5:8],tuple(args[8:11])]
            value = SK.item_record(*arguments) if operation == 84 else getattr(model, METHODS[operation])(*arguments)
            result = (call_words(value) if operation in (79,84) else [] if value is None else
                      [n for pair in value for n in pair] if operation in (29,56) else
                      list(value) if isinstance(value, tuple) else [value & M32])
            result = [v & M32 for v in result]
            status = 0
        except (RuntimeError, ZeroDivisionError):
            status, result = 1, []
        out.append([status, result, snapshot(model, configs)])
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
        sys.path.insert(0,tmp)
        SK=importlib.import_module('ref_skills_c'); RC=importlib.import_module('ref_creature'); C=importlib.import_module('ref_core')
        cases=list(scenarios(SK)) + list(runtime_scenarios(SK)) + list(protection_scenarios(SK)) + list(bloodlust_scenarios(SK)) + list(shared_classes_scenarios(SK)) + list(summon_items_scenarios(SK)) + list(criticalhit_scenarios(SK)) + list(thrown_axes_scenarios(SK)) + list(battlecry_scenarios(SK)) + list(storm_scenarios(SK)) + list(diseaseblade_scenarios(SK))
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
