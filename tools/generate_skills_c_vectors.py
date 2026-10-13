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
           'shout_interrupted','callee_store')
SERVICES = ('typeinfo','creature_findSpellByType','pool_new','light_link','res_find',
            'sound_playAtPos','particleGroup_create','particleGroup_begin',
            'fx_emitFromModelVerts','particleGroup_end','light_relink','Spell.v1',
            'LightEffect_ctor','Creature.v16','model_getSamplePointCount',
            'model_getSamplePoint','particle_spawn','damage_apply','fx_spawn','net_objOrGhost','model_getAttachPoint',
            'libc_sprintf','anim_startBlendInOut','net_isLocalSlot','Player_animStep',
            'glow_add','sound_stop')

def callback_name(slot):
    return 'Skill.v%d' % slot if slot < 16 else SERVICES[slot-16]

def callback_id(name):
    return int(name.split('v')[1]) if name.startswith('Skill.v') else 16+SERVICES.index(name)

def call_words(value):
    # Fixed signatures retain vector field boundaries. Damage descriptors expand
    # the constant header and all four slots, including canonical empty slots.
    if isinstance(value, tuple) and value and value[0] == 'desc':
        return list(bytes.fromhex(value[1])) + [n for slot in value[2:]
                for n in (list(slot)+[0]*(4-len(slot)))]
    if isinstance(value, (list,tuple)):
        return [n for item in value for n in call_words(item)]
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
        def callback(*args, result=result, poke=poke, throwing=throwing):
            if poke:
                mem.poke(*poke)
            if throwing:
                raise RuntimeError('authored callback exception')
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
            value = getattr(model, METHODS[operation])(*arguments)
            result = ([] if value is None else
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
        cases=list(scenarios(SK)) + list(runtime_scenarios(SK)) + list(protection_scenarios(SK)) + list(bloodlust_scenarios(SK))
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
