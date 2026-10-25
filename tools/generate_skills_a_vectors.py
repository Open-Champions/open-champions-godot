#!/usr/bin/env python3
"""Generate shared Skill foundation state vectors from a verified external pin.
Only authored sparse memory, statistic values and callback scripts are used.
"""
import argparse
import ast
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_skills_a.py': '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_items.py': '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e',
    'tools/difftest_skills_a.py': 'd91f55a4d6ad75dc878e3fad279f865abb679cbc34d3d28b25aab29da8728b30',
    'docs/spec/skills-01-34.md': '3e7ea2c519e301d5a70633d04763a9fec2c198ace5969f86b6ef12a9280f80aa',
    'docs/spec/skills-01-34-executed.md': 'ad12fac09365575437c123d2d0c78ef364a7358a97d37fe5895fdf865a4bd60c',
}
MASK = 0xffffffff
BODIES = {1: 0x3c8218, 2: 0x3c8220, 5: 0x3c82a8, 8: 0x3c82e4,
          9: 0x3c85b0, 10: 0x3c85fc, 11: 0x3c8690, 12: 0x3c87f0,
          13: 0x3c8864, 14: 0x3c88dc, 15: 0x3c88e4}

PORTED_UNITS = ['skill_by_id', 'skill_can_use_base', 'skill_cast_base', 'skill_mana_base', 'skill_init_stats_base', 'skill_stat', 'skill_stat_tier2', 'skill_stat_for', 'skill_stat_for_handle', 'skill_adjust_level', 'skill_mana_from_stat4', 'skill_stat_range', 'mana_cost_at', 'passive_cast', 'dodge_fraction', 'regeneration_stat0', 'add_spell', 'find_spell_by_type', 'spell_type', 'carries', 'spell_base', 'weapons_of', 'enchant', 'weaponbuff_can_use', 'weaponbuff_cast', 'weaponbuff_ctor', 'weaponbuff_run', 'lightningweapons_can_use', 'lightningweapons_cast', 'lightningweapons_run', 'coldweapons_can_use', 'coldweapons_cast', 'coldweapons_run', 'poisonweapons_can_use', 'poisonweapons_cast', 'poisonweapons_run', 'undeadshield_can_use', 'undeadshield_cast', 'undeadshield_ctor', '_shield_glow', 'undeadshield_run', 'undeadshield_damage_attacker', '_shield_reflect', 'secondary_shield_ctor', 'secondary_shield_run', 'secondary_shield_damage_attacker', 'spell_ball', 'spell_delete', 'flash_both']

PORTED_UNITS += ['player_object', 'root_can_use', 'root_cast', 'root_spell_ctor', 'root_spell_run', 'crit_can_use', 'crit_cast', 'crit_spell_ctor', 'crit_set_tint', 'crit_spell_run', 'crit_bonus_for']

PORTED_UNITS += ['shieldbash_can_use', 'shieldbash_cast', 'shieldbash_ctor', 'shieldbash_windup_fx', 'shieldbash_run', 'shieldbash_done', 'light_effect']

PORTED_UNITS += ['firestorm_can_use', 'firestorm_cast', '_firestorm_base', 'firestorm_ctor', 'firestorm_ctor2', 'firestorm_init_common', 'firestorm_run', 'obj_ball', 'delete_self']
PORTED_UNITS += ['diseasetrail_can_use', 'diseasetrail_cast', 'diseasetrail_spell_ctor', 'diseasetrail_spell_run', 'diseasetrail_ctor', 'diseasetrail_run', '_trail_puffs', 'flash2', 'box_around']

PORTED_UNITS += ['shockoffrost_can_use', 'shockoffrost_cast', 'shockoffrost_ctor', '_frost_debris', 'shockoffrost_run', '_frost_hit', 'shockoffrost_hit', 'frostshard_ctor', 'frostshard_run', 'frostshard_hit', 'projectile_ctor', 'move_and_collide', 'puff']

PORTED_UNITS += ['fireflares_can_use', 'fireflares_cast', 'fireflares_ctor', 'fireflares_dtor', 'fireflares_run', 'fireflare_ctor', 'fireflare_run']

PORTED_UNITS += [name for family in ('diseasebolt', 'holystrike') for name in
                 (family+'_can_use', family+'_cast', family+'_ctor', family+'_run', family+'_hit')]

PORTED_UNITS += ['blindinglight_can_use', 'blindinglight_cast', 'blindinglight_effect_ctor',
                 'blindinglight_effect_run', 'blindinglight_spell_ctor', 'blindinglight_spell_run']

PORTED_UNITS += ['diseaseshield_can_use', 'diseaseshield_cast', 'diseaseshield_ctor', 'diseaseshield_run', 'disease_spell_ctor', 'disease_spell_run']

PORTED_UNITS += [f'{family}_{method}' for family in ('fireweapons','coneoffire') for method in ('can_use','cast','ctor','run')]

PORTED_UNITS += ['wizardbeam_cast', 'wizardbeam_ctor', '_beam_flash', '_beam_chain', 'wizardbeam_run']

PORTED_UNITS += ['blessed_can_use', 'blessed_cast', 'blessed_ctor', 'blessed_dtor', 'blessed_stat0', 'blessed_run']

PORTED_UNITS += ['repulse_cast', 'damage_radial', 'dismissundead_cast', 'holybolt_ctor', 'holybolt_run', 'holybolt_hit']

PORTED_UNITS += ['poisonrain_can_use', 'poisonrain_cast', 'poisonrain_ctor', 'poisonrain_run']

PORTED_UNITS += ['item_tables','item_parse','item_resource_index','hammer_can_use','hammer_cast','hammer_ctor','hammer_spell_ctor','hammer_spell_run','_hammer_pull','_hammer_blow','hammer_run','seed3_draw']

PORTED_UNITS += ['carries_getter','can_use_slot','mana_cost','frame','is_player']

PORTED_UNITS += ['riposte_counter_attack','lcg','unit','frac','round_half','spread_max','rem','quot','half_toward_zero']

PORTED_UNITS += ['skelight_ctor']

PORTED_UNITS += ['descriptor', '_ImageExe.__init__', '_ImageExe.read', '_ImageExe.cstr',
                 '_SharedDraws.__init__', '_SharedDraws.next']

PORTED_UNITS += ['__init__', 'call', 'vcall', 'table_call', 'skill_call.original', 'skill_call.override']

def nums(values):
    return ','.join(f'{v & MASK:08x}' for v in values) or '-'


def tuples(values):
    return ';'.join(nums(v) for v in values) or '-'


NAMES = {100: 'pool_new', 101: 'LightEffect_ctor', 102: 'res_find', 103: 'sound_playAtPos',
         104: 'world_collectObjectsInBox', 105: 'particle_spawn', 106: 'fx_shockModel',
         107: 'Creature.v16', 108: 'Creature.v22', 109: 'Holder.v4', 110: 'Holder.v5',
         111: 'Holder.v6', 112: 'Holder.v8', 113: 'Spell.v1', 114: 'Spell.v0',
         115: 'handle_link', 116: 'Spell.v2', 117: 'light_link', 118: 'light_relink',
         119: 'light_unlink', 120: 'particleGroup_create', 121: 'particleGroup_setMatrix',
         122: 'particleGroup_begin', 123: 'particleGroup_end', 124: 'model_getSamplePointCount',
         125: 'model_getSamplePoint', 126: 'sound_release', 127: 'damage_apply', 128: 'text_lookup', 129: 'hud_showMessage',
         130: 'sound_playOnObject', 131: 'world_collectObjectsInArc', 132: 'Creature.v20',
         133: 'creature_bossRank', 134: 'handle_clear', 135: 'net_objOrGhost',
         136: 'terrain_heightAtPoint', 137: 'player_getSkillRank', 138: 'anim_startBlendInOut', 139: 'Player_animStep',
         140: 'Player_move', 141: 'Creature.v26', 142: 'Creature.v30', 143: 'model_getAttachPoint',
         144: 'world_collectInSphere', 145: 'net_isLocalSlot', 146: 'terrain_height', 147: 'obj_getPropInt',
         148: 'obj_runEveryFrame', 149: 'FireStormFlare_ctor', 150: 'Creature.v1', 151: 'obj_updateGridCell', 152: 'water_test', 153: 'libc_sprintf', 154: 'water_disturb', 155: 'PhProp_ctor', 156: 'PhProp_initPhysics', 157: 'fx_spawn', 158: 'terrain_raycast', 159: 'Creature.v6', 160: 'sound_stop', 161: 'light_dtor', 162: 'pool_free', 163: 'trail_create', 164: 'trail_addPoint', 165: 'gfx_setColorOffset', 166: 'GlowingShaft_ctor', 167: 'Creature.v31', 168: 'glow_add', 169: 'Holder.v2', 170: 'trail_setPoints_1918a0', 171: 'trail_clear_191e58', 172: 'curve_sample_1911e8', 173: 'ring_draw_1920e0', 174: 'Creature_getVisualPos', 175: 'voice_setPosition', 176: 'LostSoul_ctor', 177: 'Creature.v2', 178: 'model_transformSamplePoint', 179: 'world_findObjectByName', 180: 'world_findFreeSpot', 181: 'Skelight_ctor', 182: 'Creature_setCharm', 183: 'Creature.v37', 184: 'Creature.v34', 185: 'Holder.v6'}


def flatten(values):
    if values == "stack":
        return [MASK]
    if isinstance(values, tuple) and values and values[0] == 'desc':
        return [lane for slot in values[1:] for lane in ([MASK] if slot is None else slot)]
    return [lane for value in values for lane in (flatten(value) if isinstance(value, (tuple, list, str)) else [value])]


def snapshot(model):
    return '|'.join([tuples(sorted(model.mem.bytes.items())), tuples(model.writes),
                     tuples(model.mem.log), ';'.join((str(int(name[7:])) if name.startswith('Skill.v') else name) + ':' + nums(flatten(args))
                                                    for name, args in model.calls) or '-',
                     nums([model.draws, model.hidden_draws])])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('native/tests/vectors/skills_a.tsv.gz'))
    a = parser.parse_args()
    for path, expected in PINS.items():
        data = (a.research_dir / path).read_bytes()
        committed = subprocess.check_output(['git', '-C', str(a.research_dir), 'show', COMMIT + ':' + path])
        if hashlib.sha256(data).hexdigest() != expected or hashlib.sha256(committed).hexdigest() != expected:
            parser.error('research pin mismatch: ' + path)
    tree = ast.parse((a.research_dir / 'tools/ref_skills_a.py').read_text())
    units = [node.name for node in tree.body if isinstance(node, ast.FunctionDef) and node.name != 'shape_result']
    units += [(cls.name+'.'+node.name if cls.name in ('_ImageExe','_SharedDraws') else node.name) for cls in tree.body if isinstance(cls, ast.ClassDef)
              for node in cls.body if isinstance(node, ast.FunctionDef)]
    dispatch_indices=[i for i,name in enumerate(units) if name=='skill_call']
    assert len(dispatch_indices)==2
    for i,label in zip(dispatch_indices,('skill_call.original','skill_call.override')): units[i]=label
    assert len(units) == 196 and len(PORTED_UNITS) == 195
    assert all(units.count(name) == 1 for name in PORTED_UNITS)
    sys.dont_write_bytecode = True
    sys.path.insert(0, str((a.research_dir / 'tools').resolve()))
    spec = importlib.util.spec_from_file_location('skills_reference', a.research_dir / 'tools/ref_skills_a.py')
    R = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(R)
    for module in (R.C, R.RC, R.AM, R.RI):
        if Path(module.__file__).resolve().parent != (a.research_dir / 'tools').resolve():
            parser.error('reference import came from another checkout')
    rng = random.Random(0x534b494c)
    rows, counts = [], {}
    porting_hammer = False

    def put(op, args, patches, config=0, scripts=(), external=(), strict=False):
        if porting_hammer:
            # The input schema carries raw32 lanes; supplied 64-bit table flags
            # must arrive as two adjacent raw32 patches, not one truncated lane.
            patches=[lane for addr,size,value in patches for lane in
                ([(addr,4,value & MASK),(addr+4,4,(value>>32) & MASK)] if size==8 else [(addr,size,value)])]
        R.C.set_fpu(('ps2', 'ps2g1', 'chop', 'nearest')[config // 4], bool(config & 2), bool(config & 1))
        mem = R.RC.Memory()
        for addr, size, value in patches:
            mem.poke(addr, size, value)
        model = R.Model(mem, external=['Skill.v%d' % slot for slot in external])
        if op.startswith("hammer") or op in ("item_tables","item_parse"):
            R.Model.item_tables_cache = None
        model.type_info = {} # avoid leaking the reference class-level table between cases
        if strict:
            # Default-zero external results are outside the native portable domain.
            def required_call(name, *params):
                params = tuple(v & MASK if isinstance(v, int) else v for v in params)
                model.calls.append((name, params))
                if name not in model.cb:
                    raise NotImplementedError('missing authored callback: ' + name)
                return model.cb[name](*params)
            model.call = required_call
        for slot, value, mode, addr, size, mutation in scripts:
            if mode == 6:
                model.type_info[value] = addr
                continue
            sequence = [0]
            def cb(*params, value=value, mode=mode, addr=addr, size=size, mutation=mutation, sequence=sequence):
                if mode in (1, 2, 8, 12):
                    mem.poke(addr, size, mutation)
                if mode == 2:
                    raise RuntimeError('authored callback failure')
                if mode == 8:
                    return [value]
                if mode == 12:
                    return (value,0x40000000,0x40400000)
                if mode == 13:
                    return (value,0x40000000,0x40400000),mutation
                if mode == 11:
                    return (value,addr,mutation),(0x3f800000,0,0)
                if mode == 10:
                    return (value, addr, mutation), None
                if mode == 9:
                    return [value, addr, mutation]
                if mode == 3:
                    return [value, mutation]
                if mode == 4:
                    result = value if sequence[0] == 0 else mutation
                    sequence[0] += 1
                    return result
                if mode == 7:
                    return []
                return R.s32(value)
            model.cb[NAMES.get(slot, 'Skill.v%d' % slot)] = cb
        try:
            if op == 'model_init':
                result=(model.draws,model.hidden_draws,len(model.calls),len(model.writes))
            elif op == 'named_call':
                result=model.call('Creature.v16',*args)
            elif op == 'generic_table':
                result=model.table_call('Spell' if args[0]==0 else 'Creature',*args[1:4],*args[4:])
            elif op in ('image_read','image_cstr'):
                image=R._ImageExe(mem.load)
                result=tuple(image.read(args[0],R.s32(args[1])) if op=='image_read' else image.cstr(args[0]))
            elif op == 'shared_draws':
                shared=R._SharedDraws(model)
                result=tuple(shared.next() for _ in range(args[0]))
            elif op == 'descriptor':
                entries=[]; index=0
                while index<len(args):
                    size=args[index]; index+=1
                    entries.append(tuple(args[index:index+size])); index+=size
                result=tuple(flatten(R.descriptor(*entries)))
            elif op in ('summon_cast',):
                result=model.summon_cast(*args[:3],tuple(args[3:6]),args[6])
            elif op == '_summon_sample':
                result=model._summon_sample(args[0],R.s32(args[1]))
            elif op == 'skelight_ctor':
                result=model.skelight_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op == 'carries_getter':
                result=R.carries_getter(model.u32,args[0])
            elif op == 'first_skill_call':
                first=next(cls for cls in reversed(R.Model.__mro__) if cls.__module__==R.__name__ and 'skill_call' in cls.__dict__)
                result=(first.skill_call(model,*args[:4],tuple(args[4:7]),args[7]) if len(args)==8 and args[1]==2
                        else first.skill_call(model,args[0],args[1],*args[2:]))
            elif op in ('lcg','unit','frac','round_half','spread_max','rem','quot','half_toward_zero'):
                result=getattr(R,op)(*args)
            elif op == 'item_tables':
                t=model.item_tables(); result=(len(t.defs),len(t.presets),len(t.prefixes),len(t.suffixes),len(t.formats))
            elif op == 'hammer_ctor':
                result=model.hammer_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op == '_hammer_pull':
                result=model._hammer_pull(args[0],tuple(args[1:4]),args[4])
            elif op in ('hammer_cast','repulse_cast','dismissundead_cast','poisonrain_cast'):
                result=getattr(model,op)(*args[:3],tuple(args[3:6]),args[6])
            elif op == 'poisonrain_ctor':
                result=model.poisonrain_ctor(*args[:3],tuple(args[3:6]),*args[6:])
            elif op == 'holybolt_ctor':
                result=model.holybolt_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op == 'damage_radial':
                result=model.damage_radial(args[0],tuple(args[1:4]),*args[4:])
            elif op == '_beam_chain':
                result=model._beam_chain(*args[:3],tuple(args[3:6]),args[6])
            elif op in ('wizardbeam_run','fireweapons_run','coneoffire_run'):
                result=getattr(model,op)(args[0],tuple(args[1:4]),args[4])
            elif op in ('diseaseshield_run', 'disease_spell_run'):
                result=getattr(model,op)(args[0],tuple(args[1:4]),args[4])
            elif op == 'blindinglight_cast':
                result=model.blindinglight_cast(*args[:3],tuple(args[3:6]),args[6])
            elif op == 'blindinglight_effect_ctor':
                result=model.blindinglight_effect_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op in ('diseasebolt_cast', 'holystrike_cast'):
                result=getattr(model,op)(*args[:3],tuple(args[3:6]),args[6])
            elif op in ('diseasebolt_ctor', 'holystrike_ctor'):
                result=getattr(model,op)(*args[:3],tuple(args[3:6]),*args[6:])
            elif op == 'fireflares_cast':
                result=model.fireflares_cast(*args[:3],tuple(args[3:6]),args[6])
            elif op == 'fireflares_ctor':
                result=model.fireflares_ctor(*args[:3],tuple(args[3:6]),args[6])
            elif op == 'fireflares_run':
                result=model.fireflares_run(args[0],tuple(args[1:4]),args[4])
            elif op == 'fireflare_ctor':
                result=model.fireflare_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op == 'spell_dtor_call':
                result=model.table_call('Spell',args[0],0x14,1,args[1])
            elif op == 'shockoffrost_cast':
                result=model.shockoffrost_cast(*args[:3],tuple(args[3:6]),args[6])
            elif op in ('shockoffrost_ctor', 'frostshard_ctor', 'projectile_ctor'):
                result=getattr(model,op)(args[0],tuple(args[1:4]),*args[4:])
            elif op == 'move_and_collide':
                result=model.move_and_collide(args[0],tuple(args[1:4]))
            elif op == 'puff':
                result=model.puff(lambda: model.obj_ball(args[0]),args[1],tuple(args[2:5]),args[6] if args[5] else None,args[7])
            elif op == 'diseasetrail_cast':
                result=model.diseasetrail_cast(*args[:3],tuple(args[3:6]),args[6])
            elif op == 'diseasetrail_spell_ctor':
                result=model.diseasetrail_spell_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op == 'diseasetrail_spell_run':
                result=model.diseasetrail_spell_run(args[0],tuple(args[1:4]),args[4])
            elif op == 'diseasetrail_ctor':
                result=model.diseasetrail_ctor(*args[:2],tuple(args[2:5]),tuple(args[5:8]),*args[8:])
            elif op == '_trail_puffs':
                obj=args[0]
                draw=(lambda: model.spell_rand(obj)) if args[1] else (lambda: model.obj_rand(obj))
                ball=(lambda: model.spell_ball(obj)) if args[1] else (lambda: model.obj_ball(obj))
                result=model._trail_puffs(draw,ball,tuple(args[2:5]),tuple(args[5:8]))
            elif op == 'box_around':
                result=tuple(model.box_around(tuple(args[:3]),*args[3:]))
            elif op == 'gameobject_ctor':
                result=model.gameobject_ctor(args[0],tuple(args[1:4]),*args[4:])
            elif op == 'object_call':
                result=model.vcall(args[0],args[1],*args[2:])
            elif op == 'firestorm_cast':
                result=model.firestorm_cast(*args[:3],tuple(args[3:6]),args[6])
            elif op == '_firestorm_base':
                result=model._firestorm_base(args[0],tuple(args[1:4]),args[4])
            elif op == 'firestorm_ctor':
                result=model.firestorm_ctor(*args[:3],tuple(args[3:6]),args[6])
            elif op == 'firestorm_ctor2':
                result=model.firestorm_ctor2(*args[:2],tuple(args[2:5]),*args[5:])
            elif op in ('lightningweapons_run', 'coldweapons_run', 'poisonweapons_run'):
                result = getattr(model, op)(args[0], tuple(args[1:4]), args[4])
            elif op == 'light_effect':
                result = model.light_effect(args[0], tuple(args[1:4]), *args[4:])
            elif op == 'root_cast':
                result = model.root_cast(*args[:3], tuple(args[3:6]), args[6])
            elif op == 'undeadshield_run':
                result = model.undeadshield_run(args[0], tuple(args[1:4]), args[4])
            elif op == 'weaponbuff_run':
                result = model.weaponbuff_run(args[0], args[1], tuple(args[2:5]), args[5])
            elif op == 'spell_call' and len(args) == 2:
                result = model.table_call('Spell', args[0], 0x14, args[1])
            elif op == 'spell_call':
                result = model.table_call('Spell', args[0], 0x14, args[1], tuple(args[2:5]), *args[5:])
            elif op == 'call':
                if len(args) == 8 and args[1] == 2:
                    result = model.skill_call(args[0], args[1], *args[2:4], tuple(args[4:7]), args[7])
                else:
                    result = model.skill_call(args[0], args[1], *args[2:])
            else:
                result = getattr(model, op)(*args)
            if isinstance(result,list): result=tuple(result)
            result = [] if result is None else list(result) if isinstance(result, tuple) else [result]
            encoded = nums(result)
        except (NotImplementedError, KeyError):
            encoded = 'unsupported'
        except RuntimeError:
            encoded = 'callback-error'
        rows.append('\t'.join([op, str(config), nums(args), tuples(patches), tuples(scripts), nums(external),
                               encoded, snapshot(model)]))
        counts[op] = counts.get(op, 0) + 1

    def base(seed=0):
        p = [(0x1060, 4, 0x2000), (R.G_RAND_SEED, 4, seed), (0x300c, 4, 0x40),
             (0x4000, 4, 0x5000), (0x500c, 4, 0x3000), (R.G_SKILL_TABLE + 4*17, 4, 0x1000)]
        for slot, body in BODIES.items():
            p += [(0x2000 + 8*slot, 2, 0), (0x2004 + 8*slot, 4, body)]
        return p

    firsts = [-32768, -8, -2, -1, 0, 1, 2, 20, 32767]
    levels = [0, 1, 2, 19, 20, 40, -1, -32768, 2147483647, -2147483648]
    steps = [0, 0x80000000, 0x3f000000, 0xbf000000, 0x40000000, 0xc0000000,
             0x00800000, 0x007fffff, 0x7fffffff, 0xffffffff, 0x3f800001, 0x4b800001]
    for config in range(16):
        for i in range(96):
            p = base(rng.getrandbits(32))
            for stat in range(5):
                for off, width, value in [(0xe, 2, rng.choice(firsts)), (0x18, 2, rng.randrange(-32768, 32768)),
                                          (0x24, 4, rng.choice(steps)), (0x38, 2, rng.choice(firsts)),
                                          (0x42, 2, rng.randrange(-32768, 32768)), (0x4c, 4, rng.choice(steps))]:
                    p.append((0x1000 + off + width*stat, width, value))
            stat, level, cap = i % 5, rng.choice(levels), rng.choice([0, 1, 10, 20, -1, 2147483647])
            for op, tail in [('skill_stat', [stat, level]), ('skill_stat_tier2', [stat, level, cap]),
                             ('skill_stat_for', [stat, level, [0, 0x3000, 0x3100][i % 3], cap]),
                             ('skill_stat_for_handle', [stat, level, 0x4000, cap]),
                             ('skill_mana_from_stat4', [level, cap]),
                             ('dodge_fraction', [level]), ('regeneration_stat0', [level]),
                             ('skill_stat_range', [stat, level, i % 3, rng.choice(steps)])]:
                put(op, [0x1000] + tail, p, config)
            put('call', [0x1000, 10, stat, level], p, config)
        for level in levels:
            # Synthetic immediates: no instruction bytes from any game file.
            p = base() + [(0x2004 + 8*5, 4, 0x6000), (0x6000, 4, 0x24a5ffff),
                          (0x6004, 4, 0x3c014060), (0x6008, 4, 0x44810800),
                          (0x600c, 4, 0x3c01bf00), (0x6010, 4, 0x34210001), (0x6014, 4, 0x44811000)]
            put('call', [0x1000, 5, level], p, config)
            for slot in (14, 15):
                for instruction in (0x24060014, 0x3021, 0x3025, 0x302d):
                    p = base() + [(0x2004 + 8*5, 4, 0x6000), (0x600c, 4, instruction),
                                  (0x6014, 4, 0x24420000 | slot*8)]
                    put('call', [0x1000, 5, level], p, config)
    for op, args in [('skill_can_use_base', [0x1000]), ('skill_cast_base', [0x1000]),
                     ('skill_mana_base', [0x1000]), ('passive_cast', [0x1000]), ('skill_init_stats_base', [0x1000]),
                     ('skill_adjust_level', [0x1000, 0xffffffff, 77]), ('skill_by_id', [17])]:
        put(op, args, base())
    for slot, args in [(1, []), (2, []), (5, []), (9, []), (14, [0xffffffff])]:
        put('call', [0x1000, slot] + args, base())
    for body in (0x2ddb90, 0x2f1440):
        put('call', [0x1000, 2], base() + [(0x2004 + 8*2, 4, body)])
    # Initialized first tier, second tier and sentinel bytes remain observable.
    p = base() + [(0x1000+i, 1, (i*73+11) & 255) for i in range(0x68)]
    put('skill_init_stats_base', [0x1000], p)
    # Handle node present/object absent still reads low memory's flags.
    for node, obj, flag in [(0, 0, 0x40), (0x5000, 0, 0x40), (0x5000, 0, 0), (0x5000, 0x3000, 0)]:
        p = base() + [(0x4000, 4, node), (0x500c, 4, obj), (0xc, 4, flag)]
        put('skill_stat_for_handle', [0x1000, 0, 40, 0x4000, 1], p)
    # Negative stat indices read adjacent supplied memory rather than typed-array guesses.
    for stat in (-1, 5, 8):
        put('skill_stat', [0x1000, stat, 40], base())
        put('skill_stat_tier2', [0x1000, stat, 40, 0], base())
    for delta in (-16, 16):
        p = base() + [(0x2000 + 8*10, 2, delta), (0x1000+delta+0xe, 2, 0),
                      (0x1000+delta+0x18, 2, 17), (0x1000+delta+0x24, 4, 0x40000000)]
        put('call', [0x1000, 10, 0, 2], p)
    # External overrides include writes, errors and virtual this offsets.
    for mode in range(3):
        p = base(0x12345678) + [(0x2000 + 8*10, 2, -16)]
        scripts = [(10, 0xfffffff9, mode, 0x100e, 2, 0xffff)]
        for op, tail in [('skill_stat_range', [0, 20, 1, 0x40000000]),
                         ('skill_stat_for', [0, 20, 0x3000, 1]), ('dodge_fraction', [20]),
                         ('regeneration_stat0', [20]), ('skill_mana_from_stat4', [20, 0])]:
            put(op, [0x1000] + tail, p, scripts=scripts, external=[10])
    put('skill_mana_from_stat4', [0x1000, 20, 3], base(), scripts=[(14, 7, 1, 0x100e, 2, 9)], external=[14])
    # Unknown callbacks and mana shape refusals, preserving exact state prefixes.
    p = base() + [(0x2004 + 8*10, 4, 0xdeadbeef)]
    put('call', [0x1000, 10, 0, 20], p, scripts=[(10, 5, 0, 0, 1, 0)])
    for p in [base() + [(0x2004 + 8*5, 4, 0x6000)],
              base() + [(0x2004 + 8*5, 4, 0x6000), (0x6000, 4, 0x24a5ffff)]]:
        put('call', [0x1000, 5, 1], p)
    legacy_count = len(rows)
    legacy_plain = ('# rta-skills-a-shared-v1\n' + '\n'.join(rows) + '\n').encode('ascii')
    assert legacy_count == 15310
    assert hashlib.sha256(legacy_plain).hexdigest() == '3dd4d5fd40bde30ae99a6795cf543aa76db3efb7db2ec6b3f654676a18ec6f1c'
    # One complete family: ids 12/14/26, synthetic tables/items and explicit services.
    # All older rows above remain in the same order and byte-for-byte identical.
    kinds = {12: 0x18, 14: 0x16, 26: 0x17}
    tables = {12: 0x5199d0, 14: 0x519b40, 26: 0x51c278}
    getters = {12: 0x3ceba4, 14: 0x3ced8c, 26: 0x3d1b14}
    can_bodies = {12: 0x2e34ac, 14: 0x2e616c, 26: 0x315bb0}
    cast_bodies = {12: 0x2e3184, 14: 0x2e5e44, 26: 0x315be4}
    run_bodies = {12: 0x2e2ac4, 14: 0x2e5578, 26: 0x3164fc}
    rec = R.G_PLAYERS

    def weapon_state(sid, flags=0x40, frame=30):
        p = base(0x13579bdf) + [(R.G_SIM_FRAME, 4, frame), (0x300c, 4, flags),
              (0x3010, 4, 0x41200000), (0x3014, 4, 0xc1a00000), (0x3018, 4, 0x41f00000),
              (0x3038, 2, -17), (0x30dc, 4, ONE), (0x30e0, 4, TWO),
              (0x3048, 4, 0x9000), (0x3050, 4, 0xa000), (0xa000, 4, 0xa100),
              (0x33c8, 4, 0), (0x600c, 4, 0x40), (0x63c8, 4, 1),
              (0x700c, 4, 0x40), (0x73c8, 4, 2),
              (rec+0x4a7c, 1, 0), (rec+0x4434, 4, 0xb000), (rec+0x443c, 4, 0xb100),
              (rec+R.REC_SIZE+0x4434, 4, 0xb200), (rec+R.REC_SIZE+0x443c, 4, 0xb300),
              (rec+2*R.REC_SIZE+0x4434, 4, 0xb400),
              (0x2004+8, 4, can_bodies[sid]), (0x2004+16, 4, cast_bodies[sid]),
              (0x8000, 1, 7), (0x8008, 4, 0x1000), (0x800c, 4, 0x3000),
              (0x8010, 4, 0x87654321), (0x8014, 4, tables[sid]),
              (0x8018, 4, frame+600), (0x801c, 4, 96),
              (0x8020, 4, 0x42c00000 if sid==26 else frame+22),
              (tables[sid]+4, 4, getters[sid]), (tables[sid]+20, 4, run_bodies[sid])]
        # Authored statistic tables, independent of any retail values.
        for k, value in enumerate((17, 15, 8, 12, 4)):
            p += [(0x100e+2*k, 2, 1), (0x1018+2*k, 2, value), (0x1024+4*k, 4, 0x3f000000),
                  (0x1038+2*k, 2, -1)]
        for item in range(0xb000, 0xb500, 0x100):
            p += [(item+0x24+8*k, 2, -1) for k in range(4)]
        return p

    def service_scripts(block=0xc000):
        return [(100, block, 0, 0, 1, 0), (101, 0, 7, 0, 1, 0),
                (102, 0xd000, 0, 0, 1, 0), (103, 0, 7, 0, 1, 0),
                (104, 0x6000, 3, 0, 1, 0x7000), (105, 0, 7, 0, 1, 0),
                (106, 0, 7, 0, 1, 0), (107, 0xe000, 0, 0, 1, 0),
                (108, 1, 0, 0, 1, 0), (109, 0, 7, 0, 1, 0),
                (110, 0, 7, 0, 1, 0), (111, 0, 7, 0, 1, 0), (112, 0, 7, 0, 1, 0),
                (113, 0, 7, 0, 1, 0)]

    ONE, TWO, HALF = R.ONE, R.TWO, R.HALF
    for config in range(16):
        for sid in (12, 14, 26):
            for level in (0, 1, 7, 127, 128, 255, 256, -1, -2147483648, 2147483647):
                for flags in (0x40, 0x20, 0):
                    p = weapon_state(sid, flags)
                    scripts = service_scripts()
                    put('weaponbuff_ctor', [sid, 0x8000, level, 0x3000, 0x1000], p, config, scripts, strict=True)
                scripts = service_scripts()
                scripts[0] = (100, 0x8000, 4, 0, 1, 0xc000)
                put('weaponbuff_cast', [sid, 0x1000, level, 0x3000], weapon_state(sid), config, scripts, strict=True)
            for frame in (-60, -30, -1, 0, 1, 29, 30, 31, 599, 600, 601):
                for flags in (0x40, 0x20, 0x60):
                    p = weapon_state(sid, flags, frame) + [(0x8018, 4, 600), (0x8020, 4, 0x42c00000 if sid==26 else 30)]
                    put('weaponbuff_run', [sid, 0x8000, 0x41200000, 0xc1a00000, 0x41f00000, -17],
                        p, config, service_scripts(), strict=True)
            # Exact enchant slot scanning, signed comparisons, low16 storage, FPU rounding.
            for slot in range(4):
                for current in (-1, 0, 30, 31, 32, 32767):
                    for value in (-1, 0, 1, 30, 31, 32767, 65536, 2147483647):
                        p = [(0xb024+8*k, 2, 2) for k in range(4)]
                        p += [(0xb024+8*slot, 2, kinds[sid]), (0xb026+8*slot, 2, current)]
                        put('enchant', [0xb000, kinds[sid], value, 0x1001f, 31], p, config, strict=True)
            put('spell_rand', [0x8000], weapon_state(sid), config, strict=True)

    for sid in (12, 14, 26):
        p = weapon_state(sid)
        scripts = service_scripts()
        mapping = [(0, getters[sid], 6, 0x123400+sid, 1, 0)]
        for present in (0, 1, 2):
            q = p + [(0x3364, 4, 0xf000 if present else 0),
                     (0xf000, 4, 0xf100), (0xf00c, 4, 0 if present==2 else 0x8000),
                     (0xf10c, 4, 0x8000)]
            put('weaponbuff_can_use', [sid, 0x3000], q, scripts=mapping, strict=True)
            put('call', [0x1000, 1, 0x3000], q, scripts=mapping, strict=True)
        put('weaponbuff_can_use', [sid, 0x3000], p, strict=True) # missing identity refuses
        for block in (0, 0xc000):
            put('weaponbuff_ctor', [sid, 0x8000, 7, 0x3000, 0x1000], p,
                scripts=service_scripts(block), strict=True)
        put('call', [0x1000, 2, 7, 0x3000], p,
            scripts=[(100, 0x8000, 4, 0, 1, 0xc000)]+scripts[1:], strict=True)
        put('weaponbuff_cast', [sid, 0x1000, 7, 0x3000], p,
            scripts=[(100, 0, 0, 0, 1, 0)], strict=True)
        # Signed virtual adjustments, inherited ownership rereads, failed callbacks.
        for mode in (1, 2):
            for entry in (100, 101, 102, 103, 109, 110, 111):
                ss = [v if v[0]!=entry else (entry,v[1],mode,0x800c,4,0x6000) for v in scripts]
                put('weaponbuff_ctor', [sid,0x8000,7,0x3000,0x1000], weapon_state(sid,0x20), scripts=ss, strict=True)
            for entry in (104, 105, 106, 107, 108, 112, 113):
                ss = [v if v[0]!=entry else (entry,v[1],mode,0x800c,4,0x6000) for v in scripts]
                # Query needs a list even when it mutates: separately handled below.
                if entry==104 and mode==1:
                    continue
                q=p+[(R.G_PVP_MODE_A,4,1), (0x9000+8*16,2,-16), (0x9000+8*22,2,16),
                     (0xa100+8*8,2,-8), (tables[sid]+8,2,8)]
                if entry in (112,113):
                    q=weapon_state(sid,0x20,600)+[(0x8018,4,600), (tables[sid]+8,2,8), (0xa100+64,2,-8)]
                put('weaponbuff_run',[sid,0x8000,ONE,TWO,0,0],q,scripts=ss,strict=True)
        for omitted in (100,101,102,103):
            put('weaponbuff_ctor',[sid,0x8000,7,0x3000,0x1000],p,
                scripts=[v for v in scripts if v[0]!=omitted],strict=True)
        for omitted in (104,105,106,107,108,112,113):
            q=p+[(R.G_PVP_MODE_A,4,1)]
            if omitted in (112,113):
                q=weapon_state(sid,0x20,600)+[(0x8018,4,600)]
            put('weaponbuff_run',[sid,0x8000,ONE,TWO,0,0],q,
                scripts=[v for v in scripts if v[0]!=omitted],strict=True)
        # Aura zero/nonplayer/unaligned frame exits; PvP rejects; duplicate/self/null objects.
        for radius in (0, -1, 1, 96):
            for pvp, same in ((0,0),(1,0),(1,1)):
                q=p+[(0x8020 if sid==26 else 0x801c,4,R.C.from_int(radius) if sid==26 else radius),
                     (R.G_PVP_MODE_A,4,pvp)]
                ss=[v if v[0]!=108 else (108,same,0,0,1,0) for v in scripts]
                put('weaponbuff_run',[sid,0x8000,ONE,TWO,0,0],q,scripts=ss,strict=True)
        # Query mutation rereads owner after collecting; callbacks may return an empty list.
        for mode in (7, 8):
            ss=[v if v[0]!=104 else (104,0x6000,mode,0x800c,4,0x7000) for v in scripts]
            put('weaponbuff_run',[sid,0x8000,ONE,TWO,0,0],p,scripts=ss,strict=True)
        # Aura's 31 threshold is the existing minimum, independent of remaining duration.
        for current in (-1,0,30,31,32):
            for value in (0,17,31,32):
                q=p+[(0xb224,2,kinds[sid]),(0xb226,2,current),(0xb22a,2,1)]
                ss=scripts+[(10,value,0,0,1,0)]
                put('weaponbuff_run',[sid,0x8000,ONE,TWO,0,0],q,scripts=ss,external=[10],strict=True)
        # Holder activation and Skill dispatch use independently adjusted this values.
        for delta in (-16,16):
            q=weapon_state(sid,0x20)+[(0xa100+8*R.Model.WEAPON_BUFFS[sid]['holder'],2,delta)]
            put('weaponbuff_ctor',[sid,0x8000,7,0x3000,0x1000],q,scripts=scripts,strict=True)
            q=p+[(0x2008,2,delta)]
            put('call',[0x1000,1,0x3000],q,scripts=mapping,strict=True)
        for found in ((0x3000,0), (0x6000,0x6000), (0x5000,0x7000)):
            ss=[v if v[0]!=104 else (104,found[0],3,0,1,found[1]) for v in scripts]
            put('weaponbuff_run',[sid,0x8000,ONE,TWO,0,0],p,scripts=ss,strict=True)
        put('spell_call',[0x8000,2,ONE,TWO,0,99],p,scripts=scripts,strict=True)
        # External spell run callback remains opt-in at the API level (tested there).
        for active in (-128,-1,0,1,127):
            put('weapons_of',[0x3000],p+[(rec+0x4a7c,1,active)],strict=True)
        for thrown in (0,0x1000000):
            for kind in (-1,2,kinds[sid]):
                put('enchant',[0xb000,kinds[sid],17,600,600],
                    [(0xb008,4,thrown),(0xb024,2,kind)],strict=True)
        put('enchant',[0,kinds[sid],17,600,600],[],strict=True)
        # First matching entry is read before selecting the first empty-or-kind slot.
        put('enchant',[0xb000,kinds[sid],17,600,600],
            [(0xb024,2,-1),(0xb02c,2,kinds[sid]),(0xb02e,2,10)],strict=True)

    for spell in (0,0x8000):
        put('add_spell',[0x3000,spell],[],scripts=[(115,0,7,0,1,0)],strict=True)
    put('add_spell',[0x3000,0x8000],[],strict=True)
    put('spell_base',[0x8000,0x123,0x3000,0x1000,0x2000],base(0xfeedcafe),strict=True)
    for getter in (0,0x1234):
        for mapped in (False,True):
            p=[(0x8014,4,0x2000),(0x2004,4,getter),(0x2000,2,-16)]
            ss=[(0,getter,6,0x12345678,1,0)] if mapped else [(114,17,0,0,1,0)]
            put('spell_type',[0x8000],p,scripts=ss,strict=True)
    put('spell_type',[0x8000],[],strict=True)
    previous_count = len(rows)
    previous_plain = ('# rta-skills-a-shared-v1\n' + '\n'.join(rows) + '\n').encode('ascii')
    assert previous_count == 28399
    previous_hash = hashlib.sha256(previous_plain).hexdigest()
    assert previous_hash == '7189fbbc67e939725e4a452e6f63654b7f4d339a109f84d1e376df2ead4ddec2'

    def shield_state(frame=30, level=7):
        return weapon_state(12, frame=frame) + [
            (0x8000,1,level), (0x8014,4,0x51a048), (0x8018,4,600),
            (0x801c,4,85), (0x805c,4,0xd100), (0x8020,4,0xd200),
            (0x51a048+4,4,0x3cf3ac), (0x51a048+20,4,0x2ea74c),
            (0x51a018+4,4,0x3cf3fc), (0x51a018+20,4,0x2eb170),
            (0x200c,4,0x2eb970), (0x2014,4,0x2eb7cc)]

    def shield_services(count=0):
        return [v if v[0]!=103 else (103,0xd300,0,0,1,0) for v in service_scripts()] + [(n,0,7,0,1,0) for n in (117,118,119,121,122,123,126,127)] + [
            (120,0xd100,0,0,1,0), (124,count,0,0,1,0),
            (125,ONE,9,TWO,1,0), (115,0,7,0,1,0),
            (0,0x3cf3ac,6,0x1111,1,0), (0,0x3cf3fc,6,0x2222,1,0)]

    for config in range(16):
        for level in (0,1,7,127,128,255,256,-1,-2147483648,2147483647):
            p=shield_state(level=level)
            put('undeadshield_ctor',[0x8000,level,0x3000,0x1000],p,config,shield_services(),strict=True)
            put('undeadshield_cast',[0x1000,level,0x3000],p,config,
                [(100,0x8000,0,0,1,0)]+[v for v in shield_services() if v[0]!=100],strict=True)
            for incoming in (-2147483648,-100,-1,0,1,99,2147483647):
                put('undeadshield_damage_attacker',[0x8000,0x3000,0x6000,incoming],p,config,shield_services(),strict=True)
        for frame in (-60,-1,0,1,15,29,30,31,59,60,599,600,601):
            for count in (0,1,3,-3):
                p=shield_state(frame)
                put('undeadshield_run',[0x8000,ONE,TWO,0,0],p,config,shield_services(count),strict=True)
                put('secondary_shield_run',[0x8000],p+[(0x8014,4,0x51a018)],config,shield_services(count),strict=True)
        # Exact light fade: the supplied duration is 18 seconds, end frame 1080.
        for frame in (-61,-60,-59,-45,-31,-30,-29,0,15,29,30,31,1079,1080):
            put('undeadshield_run',[0x8000,ONE,TWO,0,0],
                shield_state(frame)+[(0x8018,4,1080)],config,shield_services(),strict=True)
        for seconds in (-2147483648,-1,0,1,2147483647):
            put('undeadshield_run',[0x8000,ONE,TWO,0,0],shield_state(),config,
                shield_services()+[(10,seconds,0,0,1,0)],external=[10],strict=True)
        for seconds in (0,ONE,HALF,0xbf800000,0x7f7fffff,0x80000000):
            for percent in (-1,0,1,50,2147483647,-2147483648):
                put('secondary_shield_ctor',[0x8000,percent,0x3000,seconds],shield_state(),config,shield_services(),strict=True)
                for incoming in (-100,0,1,2147483647):
                    put('secondary_shield_damage_attacker',[0x8000,0x3000,0x6000,incoming],
                        shield_state()+[(0x801c,4,percent)],config,shield_services(),strict=True)
        # Reflection percentage and duration supplied independently through virtual callbacks.
        for percent in (-1,0,1,50,101,2147483647,-2147483648):
            put('_shield_reflect',[0x8000,0x3000,0x6000,12345,percent],shield_state(),config,shield_services(),strict=True)
        put('spell_ball',[0x8000],shield_state(),config,strict=True)

    for sid, prefix in ((12,'lightningweapons'),(14,'coldweapons'),(26,'poisonweapons')):
        put(prefix+'_can_use',[0x1000,0x3000],weapon_state(sid),scripts=[(0,getters[sid],6,0x123400+sid,1,0)],strict=True)
        put(prefix+'_cast',[0x1000,7,0x3000],weapon_state(sid),scripts=service_scripts(),strict=True)
        put(prefix+'_run',[0x8000,ONE,TWO,0,0],weapon_state(sid),scripts=service_scripts(),strict=True)
    for count in (0,3):
        for op,args in [('undeadshield_ctor',[0x8000,7,0x3000,0x1000]),
                        ('undeadshield_run',[0x8000,ONE,TWO,0,0]),
                        ('secondary_shield_ctor',[0x8000,37,0x3000,HALF]),
                        ('secondary_shield_run',[0x8000]),
                        ('undeadshield_damage_attacker',[0x8000,0x3000,0x6000,123]),
                        ('secondary_shield_damage_attacker',[0x8000,0x3000,0x6000,123]),
                        ('_shield_glow',[0x8000,0x5c])]:
            for service in range(100,128):
                scripts=shield_services(count)
                put(op,args,shield_state(),scripts=[v for v in scripts if v[0]!=service],strict=True)
                for mode in (1,2):
                    callback_mode = 8 if service == 104 and mode == 1 else mode
                    scripts=[v if v[0]!=service else (service,v[1],callback_mode,0x800c,4,0x6000) for v in scripts]
                    # Sample points must retain their vector shape when mutating.
                    if service==125 and mode==1: continue
                    put(op,args,shield_state(),scripts=scripts,strict=True)
    for present in (0,1,2):
        p=shield_state()+[(0x3364,4,0xf000 if present else 0), (0xf00c,4,0x8000),
                          (0x8014,4,0x51a048 if present!=2 else 0x51a018)]
        put('undeadshield_can_use',[0x1000,0x3000],p,scripts=shield_services(),strict=True)
        put('call',[0x1000,1,0x3000],p,scripts=shield_services(),strict=True)
    put('undeadshield_can_use',[0x1000,0x3000],shield_state(),strict=True)
    put('undeadshield_cast',[0x1000,7,0x3000],shield_state(),scripts=[(100,0,0,0,1,0)],strict=True)
    put('call',[0x1000,2,7,0x3000],shield_state(),scripts=shield_services(),strict=True)
    for body in (0x2ea74c,0x2eb170):
        for delta in (-16,0,16):
            put('spell_call',[0x8000,2,ONE,TWO,0,99],shield_state()+[(0x51a048+16,2,delta),(0x51a048+20,4,body)],scripts=shield_services(),strict=True)
    for pvp,same in ((0,0),(1,0),(1,1)):
        for allocate in (0,0xc000):
            ss=[v if v[0] not in (100,108) else (v[0],allocate if v[0]==100 else same,0,0,1,0) for v in shield_services()]
            for first,second in ((0,0),(1,0),(0,1),(1,1)):
                p=shield_state()+[(R.G_PVP_MODE_A,4,pvp), (0x6364,4,0xf000 if first or second else 0),
                    (0xf00c,4,0xf200), (0xf000,4,0xf100 if first and second else 0), (0xf10c,4,0xf300),
                    (0xf214,4,0x51a048 if first else 0x51a018), (0xf314,4,0x51a018)]
                put('undeadshield_run',[0x8000,ONE,TWO,0,0],p,scripts=ss,strict=True)
    put('flash_both',[0x3000,0x12345,-1,0x80000000,7],[],strict=True)
    for spell in (0,0x8000):
        put('spell_delete',[spell],shield_state(),scripts=shield_services(),strict=True)
    shield_count=len(rows)
    shield_plain=('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')
    assert shield_count==36028
    shield_hash=hashlib.sha256(shield_plain).hexdigest()
    assert shield_hash=='94170ab12e4094409aceb36fae8aa244bf34ddc892efd29e925479f57c11ff3a'

    def support_state(root=False, level=7):
        table=0x519b70 if root else 0x519c28
        p=weapon_state(12)+[(0x8000,1,level),(0x8014,4,table),
            (0x8018,4,0 if root else R.C.from_int(8)),(0x801c,4,0),
            (0x8020,4,5),(0x8024,4,0xd100),(0x8058,4,0xd100),
            (0x8028,4,ONE),(0x802c,4,TWO),(0x8030,4,0),
            (table+4,4,R.Model.CRIT_GETTER),(table+20,4,0x2e699c if root else 0x2e7acc),
            (0x200c,4,0x2e6534 if root else 0x2e7224),
            (0x2014,4,0x2e6598 if root else 0x2e74b0),
            (0x1064,4,33),(R.G_ALIVE_COUNT,4,3)]
        for i,obj in enumerate((0x3000,0x6000,0x7000)):
            p += [(R.G_ALIVE_LIST+4*i,4,i),(R.G_PLAYERS+R.REC_SIZE*i+R.RC.REC_OBJ,4,obj),
                  (obj+0x10,4,R.C.from_int(i*5)),(obj+0x14,4,R.C.from_int(i*7)),
                  (obj+0x18,4,R.C.from_int(i*3)),(obj+0x1c,4,R.C.from_int(13)),
                  (obj+0x20,4,R.C.from_int(21))]
        return p

    def support_services(root=False):
        ss=[v for v in shield_services() if v[0] not in (100,125)]
        ss += [(100,0xc000,0,0,1,0),(125,ONE,10,TWO,1,0),
               (0,R.Model.CRIT_GETTER,6,0x3333,1,0),(128,0xd400,0,0,1,0),
               (129,0,7,0,1,0),(130,0,7,0,1,0),
               (131,0x6000,9,0x7000,1,0x6000),(132,1,0,0,1,0),
               (133,0,0,0,1,0),(134,0,7,0,1,0),(135,0x7000,0,0,1,0),
               (136,0,0,0,1,0),(137,7,0,0,1,0)]
        return ss

    def replace_script(ss, service, value, mode=0, addr=0, size=1, mutation=0):
        return [v for v in ss if v[0]!=service]+[(service,value,mode,addr,size,mutation)]

    for config in range(16):
        for root in (False,True):
            prefix='root' if root else 'crit'
            ss=support_services(root)
            for level in (0,1,7,127,128,255,256,-1,-2147483648,2147483647):
                p=support_state(root,level)
                put(prefix+'_spell_ctor',[0x8000,level,0x3000,0x1000],p,config,ss,strict=True)
                args=[0x1000,level,0x3000]+([ONE,TWO,0,-32769] if root else [])
                put(prefix+'_cast',args,p,config,ss,strict=True)
            for flags in (0,0x40,0x60):
                for pvp in (0,1,0xffffffff):
                    put(prefix+'_can_use',[0x1000,0x3000],support_state(root)+[(0x300c,4,flags),(R.G_PVP_MODE_A,4,pvp)],config,ss,strict=True)
        # Root phases, delay/expiry boundaries, success/resistance, signed durations.
        for phase in (-1,0,1,2):
            for t in (0,0x3f000000,R.Model.ROOT_DELAY,0x3f04dcc9,0x3f04dccb,ONE,0x40c00000):
                for flags in (0,0x40):
                    p=support_state(True)+[(0x801c,4,phase),(0x8018,4,t),(0x300c,4,flags)]
                    put('root_spell_run',[0x8000],p,config,support_services(True),strict=True)
        for chance in (-1,0,1,50,100,101,2147483647):
            for count in (0,1,-1,3):
                p=support_state(True)+[(0x8018,4,R.Model.ROOT_DELAY),(0x101c,2,chance),(0x102c,4,0)]
                ss=replace_script(support_services(True),124,count)
                put('root_spell_run',[0x8000],p,config,ss,strict=True)
                put('root_spell_run',[0x8000],p+[(0x801c,4,1)],config,ss,strict=True)
        # Critical light branches, alias particle counts, terrain cutoffs, radius wrap.
        for sid in (33,53,81,999):
            for level in (0,1,7,127,128,255):
                for left in (0,HALF,ONE,TWO,0x40c00000,0x41000000,0x41200000):
                    put('crit_spell_run',[0x8000],support_state(level=level)+[(0x1064,4,sid),(0x8018,4,left)],config,support_services(),strict=True)
        for radius in (-1,0,1,6,0x15555555,0x40000000,0x7fffffff,0x80000000):
            for height in (0x42900000,0x42900001,0xc2900000,0xc2900001):
                ss=replace_script(support_services(),10,radius)
                ss=replace_script(ss,136,height)
                put('crit_spell_run',[0x8000],support_state()+[(0x8018,4,ONE)],config,ss,external=[10],strict=True)
        for rank in (0,1,7,-1):
            for present in (False,True):
                p=support_state()+[(0x3364,4,0xf000 if present else 0),(0xf00c,4,0x8000),
                    (0x6364,4,0xf100),(0xf10c,4,0x8000)]
                put('crit_bonus_for',[0x1000,0x3000,rank],p,config,support_services(),strict=True)
        put('crit_set_tint',[0x8000,0x3000],support_state(),config,strict=True)
        for slot in (0,1,2,-1,-2147483648,2147483647):
            put('player_object',[slot],support_state(),config,strict=True)

    # Cast target selection: duplicates, empty query, limit, hostility, boss, timer,
    # allocation failure, signed timer and original failed-allocation count.
    for limit in (-1,0,1,2,3,2147483647):
        for caster in (0,0x3000):
            for flags,timer,boss,enemy,allocation in ((0,0,0,1,0xc000),(0,0,0,1,0),
                    (0x1000000,0,0,1,0xc000),(0,1,0,1,0xc000),(0,0,1,1,0xc000),
                    (0,0,0,0,0xc000),(0,-32768,0,1,0xc000)):
                p=support_state(True)+[(0x1018,2,limit),(0x1024,4,0),
                    (0x600c,4,flags),(0x6314,2,timer)]
                ss=support_services(True)
                for service,value in ((100,allocation),(132,enemy),(133,boss)):
                    ss=replace_script(ss,service,value)
                put('root_cast',[0x1000,7,caster,ONE,TWO,0,-1],p,scripts=ss,strict=True)
    put('root_cast',[0x1000,7,0x3000,ONE,TWO,0,0],support_state(True),scripts=replace_script(support_services(True),131,0,7),strict=True)

    # Bonus: strict 3-D and planar radius tests, signed best ranks and team direction.
    for best in (-7,-1,0,1,20):
        for distance in (0,0x42c00000,0x4479ffff,0x447a0000,0x447a0001):
            for same in (0,1):
                p=support_state()+[(R.G_PVP_MODE_A,4,1),(0x6010,4,distance),
                    (0x6014,4,0),(0x6018,4,0),(0x6364,4,0xf100),(0xf10c,4,0x8000)]
                ss=replace_script(support_services(),137,best)
                ss=replace_script(ss,108,same)
                put('crit_bonus_for',[0x1000,0x3000,0],p,scripts=ss,strict=True)

    # Callback-visible field rereads and exact exception/missing-service prefixes.
    probes=[('root_can_use',[0x1000,0x3000],True,[(R.G_PVP_MODE_A,4,1)]),
        ('root_cast',[0x1000,7,0x3000,ONE,TWO,0,0],True,[]),
        ('root_spell_ctor',[0x8000,7,0x3000,0x1000],True,[]),
        ('root_spell_run',[0x8000],True,[(0x801c,4,1)]),
        ('root_spell_run',[0x8000],True,[(0x8018,4,R.Model.ROOT_DELAY)]),
        ('crit_cast',[0x1000,7,0x3000],False,[]),
        ('crit_spell_ctor',[0x8000,7,0x3000,0x1000],False,[]),
        ('crit_spell_run',[0x8000],False,[]),
        ('crit_bonus_for',[0x1000,0x3000,0],False,[(0x6364,4,0xf100),(0xf10c,4,0x8000)])]
    for op,args,root,patches in probes:
        p=support_state(root)+patches
        for service in range(100,138):
            ss=support_services(root)
            put(op,args,p,scripts=[v for v in ss if v[0]!=service],strict=True)
            for address,value in ((0x800c,0x6000),(0x8008,0x1000),(0x8018,ONE),
                                  (0x801c,2),(0x8020,-1),(0x8058,0xd555)):
                ss=replace_script(ss,service,0 if service not in (100,107,124,135) else 1,
                                  2,address,4,value)
                put(op,args,p,scripts=ss,strict=True)
        # Successful owner mutations during void calls.
        for service in (105,117,118,119,121,122,123,129,130,134):
            ss=replace_script(support_services(root),service,0,1,0x800c,4,0x6000)
            put(op,args,p,scripts=ss,strict=True)
    # Successful mutations distinguish captured alive counts from reread slot/owner fields.
    for op,args,patches in [('crit_spell_run',[0x8000],[]),
            ('crit_bonus_for',[0x1000,0x3000,0],[(0x6364,4,0xf100),(0xf10c,4,0x8000)])]:
        for address,value in ((R.G_ALIVE_COUNT,0),(R.G_ALIVE_COUNT,4),
                (R.G_ALIVE_LIST+8,1),(0x800c,0x6000),
                (R.G_PLAYERS+R.REC_SIZE+R.RC.REC_OBJ,0x7000)):
            for service,ret in ((108,1),(137,7),(136,0)):
                ss=replace_script(support_services(),service,ret,1,address,4,value)
                put(op,args,support_state()+patches+[(R.G_PVP_MODE_A,4,1)],scripts=ss,strict=True)
    # A terrain service can mutate the ghost height after placement was captured.
    put('crit_spell_run',[0x8000],support_state(),scripts=replace_script(support_services(),136,0,1,0x7018,4,ONE),strict=True)
    # Rank values differ by call order, including best-rank negative first-match exit.
    for first,later in ((-7,3),(7,1),(-1,-2),(0,7)):
        ss=replace_script(support_services(),137,first,4,0,1,later)
        put('crit_bonus_for',[0x1000,0x3000,0],support_state()+[(0x6364,4,0xf100),(0xf10c,4,0x8000)],scripts=ss,strict=True)
    for root in (False,True):
        for delta in (-16,16):
            p=support_state(root)+[(0x2008,2,delta),(0x2010,2,delta)]
            put('call',[0x1000,1,0x3000],p,scripts=support_services(root),strict=True)
            args=[0x1000,2,7,0x3000]+([ONE,TWO,0,0] if root else [])
            put('call',args,p,scripts=support_services(root),strict=True)
    # Direct virtual routes and signed this adjustments; ignored Spell pose arguments.
    for root in (False,True):
        p=support_state(root)
        ss=support_services(root)
        put('call',[0x1000,1,0x3000],p,scripts=ss,strict=True)
        args=[0x1000,2,7,0x3000]+([ONE,TWO,0,0] if root else [])
        put('call',args,p,scripts=ss,strict=True)
        for delta in (-16,0,16):
            table=0x519b70 if root else 0x519c28
            put('spell_call',[0x8000,2,ONE,TWO,0,99],p+[(table+16,2,delta)],scripts=ss,strict=True)
        for count in (-1,0,1,3):
            put('crit_spell_run',[0x8000],support_state()+[(R.G_ALIVE_COUNT,4,count)],scripts=ss,strict=True)

    support_count = len(rows)
    support_plain = ('# rta-skills-a-shared-v1\n' + '\n'.join(rows) + '\n').encode('ascii')
    assert support_count == 44868
    support_hash = hashlib.sha256(support_plain).hexdigest()
    assert support_hash == 'ebaf48a42ea47ed6f037e41c5bb0afb988db2b0f0c1a02c0726c1ea59745a65c'

    def bash_state(level=7):
        p=base(123)+[(0x8000,1,level),(0x8008,4,0x1000),(0x800c,4,0x3000),
            (0x8010,4,19),(0x8014,4,0x51ad70),(0x8018,4,0),(0x801c,4,0),
            (0x3024,4,0xf000),(0xf000,4,0x7777),(0x3544,4,0x7778),
            (0x311a,2,0x10),(0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0),
            (0x3038,2,0x8001),(0x51ad84,4,0x2fae80),(0x51ad94,4,0x2fb67c),
            (0x200c,4,0x2faa00),(0x2014,4,0x2faa88)]
        for stat,value in enumerate((25,50,3,2,5)):
            p += [(0x100e+2*stat,2,0),(0x1018+2*stat,2,value),(0x1024+4*stat,4,0)]
        for obj in (0x6000,0x6100,0x6200):
            p += [(obj+0xc,4,0x20),(obj+0x228,4,ONE),(obj+0x10,4,TWO),
                  (obj+0x14,4,ONE),(obj+0x18,4,0)]
        return p

    def bash_services(events=2):
        return [(100,0x9000,0,0,1,0),(101,0,0,0,1,0),(102,0xa000,0,0,1,0),
            (125,ONE,10,TWO,1,0),(124,3,0,0,1,0),(105,0,0,0,1,0),
            (113,0,0,0,1,0),(128,71,0,0,1,0),(129,0,0,0,1,0),
            (131,0x6000,9,0x6100,1,0x6200),(132,1,0,0,1,0),
            (134,0,0,0,1,0),(135,0x3000,0,0,1,0),
            (138,0,0,0,1,0),(139,events,0,0,1,0),(140,0,0,0,1,0),
            (141,0,0,0,1,0),(142,0,0,0,1,0),(143,ONE,9,TWO,1,0),
            (144,0x6200,8,0x620c,4,0),(145,1,0,0,1,0),(127,0,0,0,1,0)]

    for config in range(16):
        for level in (0,1,5,6,15,16,127,128,255,256,-1,-2147483648,2147483647):
            p=bash_state(level); ss=bash_services()
            for op,args in [('shieldbash_ctor',[0x8000,level,0x3000,0x1000]),
                ('shieldbash_cast',[0x1000,level,0x3000]),('shieldbash_run',[0x8000])]:
                put(op,args,p,config,ss,strict=True)
        for flags in (0,0x40,0x60):
            for equipped in (0,1):
                for kind in (0,4,128,255):
                    put('shieldbash_can_use',[0x1000,0x3000],bash_state()+[(0x300c,4,flags),
                        (R.G_PLAYERS+0x43fc,4,equipped),(R.G_PLAYERS+0x4a86,1,kind)],config,bash_services(),strict=True)
        for done in (0,1,0xffffffff):
            put('shieldbash_done',[0x8000],bash_state()+[(0x8018,4,done)],config,strict=True)
            put('spell_call',[0x8000,4],bash_state()+[(0x8018,4,done)],config,strict=True)
        for count in (0,1,3,-1,-2147483648):
            put('shieldbash_windup_fx',[0x8000],bash_state(),config,replace_script(bash_services(),124,count),strict=True)
        for events in (0,2):
            for done in (0,1):
                for anim in (0,0x10):
                    for dead in (0,1):
                        for local in (0,1):
                            put('shieldbash_run',[0x8000],bash_state()+[(0x8018,4,done),(0x311a,2,anim),
                                (0x3450,4,dead),(0x8024,4,0x6000)],config,
                                replace_script(bash_services(events),145,local),strict=True)
        # Target quotas and chance boundaries, dead targets skip the shared stun draw.
        for quota in (-1,0,1,2,3):
            for chance in (-1,0,1,50,100,101):
                for hp in (0,ONE,0xbf800000):
                    p=bash_state()+[(0x101e,2,quota),(0x101a,2,chance)]
                    p += [(o+0x228,4,hp) for o in (0x6000,0x6100,0x6200)]
                    put('shieldbash_run',[0x8000],p,config,bash_services(),strict=True)
        for flags in (0,0x20,0x40,0x1000000,0x1000060):
            for enemy in (0,1):
                p=bash_state()+[(o+0xc,4,flags) for o in (0x6000,0x6100,0x6200)]
                put('shieldbash_ctor',[0x8000,7,0x3000,0x1000],p,config,bash_services(),strict=True)
                put('shieldbash_run',[0x8000],p,config,replace_script(bash_services(),132,enemy),strict=True)
        for delta in (-32768,-1,0,1,32767):
            p=bash_state()+[(0x2008,2,delta),(0x2010,2,delta),(0x51ad80,2,delta),(0x51ad90,2,delta)]
            for op,args in [('call',[0x1000,1,0x3000]),('call',[0x1000,2,7,0x3000]),
                            ('spell_call',[0x8000,2,ONE,TWO,0,0]),('spell_call',[0x8000,4])]:
                put(op,args,p,config,bash_services(),strict=True)
        for size in (0,0xbc):
            for allocation in (0,0x9000):
                put('light_effect',[size,ONE,TWO,0,99,0],bash_state(),config,
                    replace_script(bash_services(),100,allocation),strict=True)

    probes=[('shieldbash_can_use',[0x1000,0x3000]),('shieldbash_cast',[0x1000,7,0x3000]),
        ('shieldbash_ctor',[0x8000,7,0x3000,0x1000]),('shieldbash_windup_fx',[0x8000]),
        ('shieldbash_run',[0x8000]),('light_effect',[0xbc,ONE,TWO,0])]
    for op,args in probes:
        for service in (100,101,102,105,113,124,125,127,128,129,131,132,134,135,138,139,140,141,142,143,144,145):
            ss=bash_services()
            p=bash_state()+[(0x8024,4,0x6000),(0x311a,2,0)]
            put(op,args,p,scripts=[v for v in ss if v[0]!=service],strict=True)
            put(op,args,p,scripts=replace_script(ss,service,0,2,0x7000,4,77),strict=True)
            if service not in (125,143): # point callbacks must retain their tuple shape
                put(op,args,p,scripts=replace_script(ss,service,0,1,0x800c,4,0x6100),strict=True)
    # Empty/duplicate lists; callback mutations must be visible to later rereads.
    for mode in (7,8,9):
        for op,args in probes[1:5]:
            ss=replace_script(bash_services(),131,0x6000,mode,0x6000,1,0x6000)
            put(op,args,bash_state(),scripts=ss,strict=True)
    for allocation in (0,0x8000):
        put('shieldbash_cast',[0x1000,7,0x3000],bash_state(),scripts=replace_script(bash_services(),100,allocation),strict=True)
    for service in (127,132,142,101,139,140):
        for at,width,value in ((0x8018,4,1),(0x8000,1,255),(0x8008,4,0),(0x6228,4,0),(0x3380,4,1)):
            put('shieldbash_run',[0x8000],bash_state(),scripts=replace_script(bash_services(),service,1,1,at,width,value),strict=True)

    # Preserve the entire preceding corpus, independently of regenerated manifests.
    bash_count=len(rows)
    bash_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert bash_count == 49136 and bash_hash == 'e01b19da02da2b8c9aa14908ef03c7daa7475538f49f5ccd5516bde61fb9e13d'

    ZERO=R.ZERO
    def storm_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(R.G_WORLD,4,0xa000),
            (0x300c,4,0x40),(0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0x43000000),
            (0x3038,2,12345),(0x33c8,4,0),
            (0x8048,4,0x518c98),(0x518cbc,4,0x2d470c),
            (0x8044,4,seed),(0x804c,4,0xf000),(0xf00c,4,0x3000),
            (0x8010,4,ONE),(0x8014,4,TWO),(0x8018,4,0),
            (0x808c,4,109),(0x8090,4,240),(0x8094,4,180),(0x8098,4,0),
            (0x809c,4,24),(0x80a0,4,0x41c00000),(0x80a4,4,ONE),(0x80a8,4,15),(0x80b0,4,0),
            (0x200c,4,0x2d532c),(0x2014,4,0x2d4f7c)]
        for i in range(5):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,[109,2,3,4,5][i]),(0x1024+4*i,4,ZERO),
                  (0x1038+2*i,2,1),(0x1042+2*i,2,[17,4,5,6,7][i]),(0x104c+4*i,4,ZERO)]
        return p

    def storm_services():
        # Handle linking remains external: authored node linkage for constructor stats.
        return [(100,0x9000,0,0,1,0),(101,0,0,0,1,0),(102,71,0,0,1,0),
            (104,0x6000,9,0x6100,1,0x6000),(115,0,1,0x904c,4,0xf000),
            (117,0,0,0,1,0),(118,0,0,0,1,0),(126,0,0,0,1,0),
            (127,0,0,0,1,0),(130,77,0,0,1,0),(132,1,0,0,1,0),
            (137,7,0,0,1,0),(146,ONE,0,0,1,0),(147,0,0,0,1,0),
            (148,0,0,0,1,0),(149,0,0,0,1,0),(150,0,0,0,1,0)]

    probes=[('firestorm_can_use',[0x1000,0x3000]),
        ('firestorm_cast',[0x1000,7,0x3000,ONE,TWO,0,12345]),
        ('_firestorm_base',[0x9000,ONE,TWO,0,0x3000]),
        ('firestorm_ctor',[0x9000,7,0x3000,ONE,TWO,0,0x1000]),
        ('firestorm_ctor2',[0x9000,0x3000,ONE,TWO,0,24,109,180,15,ONE]),
        ('firestorm_init_common',[0x8000,0x3000]),('firestorm_run',[0x8000])]
    for config in range(16):
        for level in (0,1,7,128,255,256,-1,-2147483648,2147483647):
            for owner in (0,0x3000):
                for flags in (0,0x20,0x40,0x60):
                    p=storm_state()+[(0xc,4,flags),(0x300c,4,flags)]
                    for op,args in [('firestorm_ctor',[0x9000,level,owner,ONE,TWO,0,0x1000]),
                                    ('firestorm_cast',[0x1000,level,owner,ONE,TWO,0,-32768])]:
                        put(op,args,p,config,storm_services(),strict=True)
        for flags in (0,0x20,0x40,0x60):
            for ground in (ZERO,0xbf800000,0xc1800000,0xc3100000,0x7fffffff,0xffffffff):
                put('firestorm_can_use',[0x1000,0x3000],storm_state()+[(0x300c,4,flags)],config,
                    replace_script(storm_services(),146,ground),strict=True)
        for radius in (-2147483648,-301,-1,0,1,149,150,300,301,2147483647):
            for flags in (0,0x20,0x40):
                put('firestorm_ctor2',[0x9000,0x3000,ONE,TWO,0,radius,-109,180,0,ONE],
                    storm_state()+[(0x300c,4,flags)],config,storm_services(),strict=True)
        for frame in (-2147483648,-31,-30,-1,0,1,15,30,31,60,179,180,181,239,240,2147483647):
            for variant in (0,1):
                put('firestorm_run',[0x8000],storm_state()+[(R.G_SIM_FRAME,4,frame),(0x80b0,4,variant)],
                    config,storm_services(),strict=True)
        for end in (60,61,90,120,121,240,-2147483648,2147483647):
            for period in (0,1,15,17,-1,-15,-2147483648):
                put('firestorm_run',[0x8000],storm_state()+[(0x8090,4,end),(0x80a8,4,period)],
                    config,storm_services(),strict=True)
        for damage in (-2147483648,-65537,-3,-1,0,1,3,65537,2147483647):
            for seed in (0,1,0xffffffff,0x80000000):
                put('firestorm_run',[0x8000],storm_state(seed)+[(0x808c,4,damage)],config,storm_services(),strict=True)
        for radius in (-24,0,1,24,301):
            for glow in (ZERO,0x80000000,ONE,0x41c00000,0x7fffffff):
                put('firestorm_run',[0x8000],storm_state()+[(0x809c,4,radius),(0x80a0,4,glow)],config,storm_services(),strict=True)
        for start in (-2147483648,0,59,60,61,2147483647):
            for handle in (0,0xf000):
                put('firestorm_run',[0x8000],storm_state()+[(0x8098,4,start),(0x804c,4,handle)],config,storm_services(),strict=True)
        for seed in (0,1,2,3,0xffff,0xffffffff,0x80000000):
            put('obj_ball',[0x8000],storm_state(seed),config,strict=True)
        for delta in (-32768,-1,0,1,32767):
            p=storm_state()+[(0x2008,2,delta),(0x2010,2,delta),(0x518cb8,2,delta)]
            for op,args in [('call',[0x1000,1,0x3000]),('call',[0x1000,2,7,0x3000,ONE,TWO,0,12345]),
                            ('object_call',[0x8000,4])]:
                put(op,args,p,config,storm_services(),strict=True)
    # Complete exception prefixes, state mutation and no fabricated service defaults.
    for op,args in probes:
        put(op,args,storm_state(),scripts=storm_services(),strict=True)
        for service in (100,101,102,104,115,117,118,126,127,130,132,137,146,147,148,149,150):
            put(op,args,storm_state(),scripts=[v for v in storm_services() if v[0]!=service],strict=True)
            put(op,args,storm_state(),scripts=replace_script(storm_services(),service,0,2,0x7000,4,77),strict=True)
            for at,width,value in ((R.G_SIM_FRAME,4,240),(0x804c,4,0),(0x808c,4,-7),(0x80b0,4,1),(0x8044,4,0xffffffff),(0x300c,4,0x20)):
                # Preserve the list return shape when mutating a box callback.
                if service==104:
                    ss=replace_script(storm_services(),service,0x6000,8,at,width,value)
                else:
                    original=next(v[1] for v in storm_services() if v[0]==service)
                    ss=replace_script(storm_services(),service,original,1,at,width,value)
                put(op,args,storm_state(),scripts=ss,strict=True)
    for allocation in (0,0x9000):
        for op,args in probes[1:]:
            put(op,args,storm_state(),scripts=replace_script(storm_services(),100,allocation),strict=True)
    for enemy in (0,1):
        for mode in (7,8,9):
            put('firestorm_run',[0x8000],storm_state(),scripts=replace_script(
                replace_script(storm_services(),132,enemy),104,0x6000,mode,0x6000,1,0x6000),strict=True)
    for obj in (0,0x8000):
        put('delete_self',[obj],storm_state(),scripts=storm_services(),strict=True)
    # Constructor's documented refusal prefixes and resource-generation paths.
    for x in (ZERO,0xc6700000,0x466fffff,0x46700000,0x7fffffff,0xffffffff):
        for props in (0,0x9000):
            put('gameobject_ctor',[0x8000,x,TWO,0,0,props],storm_state(),scripts=storm_services(),strict=True)
    for generation in (0,1):
        p=storm_state()+[(0x41bf78,4,1),(0x7000,4,17),(0x7004,4,generation),
            (0x7008,4,19),(0x7010,4,20),(0x7018,4,0)]
        put('gameobject_ctor',[0x8000,ONE,TWO,0,0x7000,0],p,scripts=storm_services(),strict=True)
    put('gameobject_ctor',[0x8000,ONE,TWO,0,0,0],storm_state(),scripts=replace_script(storm_services(),147,1),strict=True)

    storm_count=len(rows)
    storm_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert storm_count == 56226 and storm_hash == 'edd6413ae031ec3d5e7f40f1a094ed139e253b44ab3f5bc994962efb81ac2fd0'

    def trail_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(0x300c,4,0x40),
            (0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0),
            (0x200c,4,0x2cce28),(0x2014,4,0x2ccba0),
            (0x8000,1,7),(0x8008,4,0x1000),(0x800c,4,0x3000),(0x8010,4,seed),
            (0x8014,4,0x518608),(0x51861c,4,0x2cc248),
            (0x8018,4,0xb000),(0x801c,4,ONE),(0x8020,4,TWO),(0x8024,4,0),
            (0x802c,4,0x43700000),
            (0xa010,4,ONE),(0xa014,4,TWO),(0xa018,4,0),
            (0xa044,4,seed),(0xa048,4,0x5184f0),(0x518514,4,0x2cb65c),
            (0xa04c,4,0x42200000),(0xa050,4,0xc2200000),(0xa054,4,0x42000000),
            (0xa058,4,240),(0xa05c,4,0xf000),(0xf00c,4,0x3000),
            (0xa060,4,7),(0xa064,4,0x1000),
            (0x600c,4,0x20),(0x610c,4,0x20)]
        for i in range(5):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,[109,3,17,19,5][i]),(0x1024+4*i,4,ZERO)]
        return p

    def trail_services():
        return [(100,0xb000,4,0,1,0xc000),(102,71,0,0,1,0),(103,0,0,0,1,0),
            (104,0x6000,9,0x6100,1,0x6000),(105,0,0,0,1,0),
            (113,0,0,0,1,0),(115,0,0,0,1,0),(127,0,0,0,1,0),
            (130,0,0,0,1,0),(132,1,0,0,1,0),(134,0,0,0,1,0),
            (147,0,0,0,1,0),(148,0,0,0,1,0),(150,0,0,0,1,0),(151,0,0,0,1,0),
            (0,R.Model.DISEASETRAIL_GETTER,6,0x123406,1,0)]

    trail_probes=[('diseasetrail_can_use',[0x1000,0x3000]),
        ('diseasetrail_cast',[0x1000,7,0x3000,ONE,TWO,0,-32769]),
        ('diseasetrail_spell_ctor',[0x8000,ONE,TWO,0,7,0x3000,0x1000]),
        ('diseasetrail_spell_run',[0x8000,0x42200000,TWO,0,0]),
        ('diseasetrail_ctor',[0xa000,7,ONE,TWO,0,0x42200000,0xc2200000,0,0x3000,0x1000]),
        ('diseasetrail_run',[0xa000]),
        ('_trail_puffs',[0xa000,0,ONE,TWO,0,0x42200000,0xc2200000,0]),
        ('flash2',[0x3000,-100,-100,-100,0]),
        ('box_around',[ONE,TWO,0,0x41980000,64])]
    for config in range(16):
        for op,args in trail_probes:
            put(op,args,trail_state(),config,trail_services(),strict=True)
        for level in (0,1,7,127,128,255,256,-1,-32768,2147483647,-2147483648):
            for owner in (0,0x3000):
                for op,args in [('diseasetrail_cast',[0x1000,level,owner,ONE,TWO,0,0]),
                    ('diseasetrail_spell_ctor',[0x8000,ONE,TWO,0,level,owner,0x1000]),
                    ('diseasetrail_ctor',[0xa000,level,ONE,TWO,0,ONE,TWO,0,owner,0x1000])]:
                    put(op,args,trail_state(),config,trail_services(),strict=True)
        for frame in (-2147483648,-16,-15,-1,0,1,14,15,16,59,60,61,239,240,241,2147483647):
            put('diseasetrail_run',[0xa000],trail_state()+[(R.G_SIM_FRAME,4,frame)],config,trail_services(),strict=True)
            put('diseasetrail_spell_ctor',[0x8000,ONE,TWO,0,7,0x3000,0x1000],
                trail_state()+[(R.G_SIM_FRAME,4,frame)],config,trail_services(),strict=True)
            for expiry in (ZERO,0x42700000,0x42700001,0x43700000,0x7fffffff,0xffffffff):
                put('diseasetrail_spell_run',[0x8000,0x42200000,TWO,0,0],
                    trail_state()+[(R.G_SIM_FRAME,4,frame),(0x802c,4,expiry)],config,trail_services(),strict=True)
        # Both strict thresholds, planar-only failure, and signed/unusual raw coordinates.
        for x in (ZERO,ONE,0x42100000,0x42100001,0x4213ffff,0x42140000,0x42140001,0xc2200000,0x7fffffff,0xffffffff):
            for z in (ZERO,0x42100000,0x42200000):
                for allocation in (0,0xb000):
                    put('diseasetrail_spell_run',[0x8000,x,TWO,z,0],trail_state(),config,
                        replace_script(trail_services(),100,allocation),strict=True)
        for seed in (0,1,2,3,0xffff,0x80000000,0xffffffff):
            for op,args in [('diseasetrail_run',[0xa000]),
                ('diseasetrail_spell_run',[0x8000,0x42200000,TWO,0,0]),
                ('_trail_puffs',[0xa000,0,ONE,TWO,0,0x42200000,0xc2200000,0]),
                ('_trail_puffs',[0x8000,1,ONE,TWO,0,0x42200000,0xc2200000,0])]:
                put(op,args,trail_state(seed),config,trail_services(),strict=True)
        for owner_node in (0,0xf000):
            for owner in (0,0x3000):
                for flags in (0,8,0x20,0x28,0x40,0x60,0xffffffff):
                    for enemy in (0,1):
                        put('diseasetrail_run',[0xa000],trail_state()+[(0xa05c,4,owner_node),(0xf00c,4,owner),
                            (0x600c,4,flags),(0x610c,4,flags)],config,
                            replace_script(trail_services(),132,enemy),strict=True)
        for damage in (-32768,-1,0,1,32767):
            for level in (0,1,7,0xffffffff):
                put('diseasetrail_run',[0xa000],trail_state()+[(0x1018,2,damage),(0xa060,4,level)],config,trail_services(),strict=True)
        for delta in (-32768,-1,0,1,32767):
            for op,args,patch in [('call',[0x1000,1,0x3000],[(0x2008,2,delta)]),
                ('call',[0x1000,2,7,0x3000,ONE,TWO,0,0],[(0x2010,2,delta)]),
                ('spell_call',[0x8000,2,0x42200000,TWO,0,0],[(0x518618,2,delta)]),
                ('object_call',[0xa000,4],[(0x518510,2,delta)])]:
                put(op,args,trail_state()+patch,config,trail_services(),strict=True)
        for list_mode in (7,8,9):
            put('diseasetrail_run',[0xa000],trail_state(),config,
                replace_script(trail_services(),104,0x6000,list_mode,0x6000,1,0x6000),strict=True)
        # Attached-spell lookup, empty nodes, original getter identity and first match.
        for node in (0,0xe000):
            for spell in (0,0x8000):
                for getter in (0x123406,0x123407):
                    put('diseasetrail_can_use',[0x1000,0x3000],trail_state()+[(0x3364,4,node),
                        (0xe00c,4,spell)],config,trail_services()+[(114,getter,0,0,1,0)],strict=True)
        for half in (ZERO,0x80000000,ONE,0xbf800000,0x7fffffff,0xffffffff):
            put('box_around',[ONE,TWO,0,half,0xffffffff],trail_state(),config,trail_services(),strict=True)
        for v in (-65537,-32769,-1,0,32767,32768,65537):
            put('flash2',[0x3000,v,v,v,v],trail_state(),config,strict=True)

    # Every service failure/missing-service prefix and callback-visible mutations.
    for op,args in trail_probes:
        for service in (100,102,103,104,105,113,115,127,130,132,134,147,148,150,151):
            put(op,args,trail_state(),scripts=[v for v in trail_services() if v[0]!=service],strict=True)
            put(op,args,trail_state(),scripts=replace_script(trail_services(),service,0,2,0x7000,4,77),strict=True)
            for at,width,value in ((R.G_SIM_FRAME,4,240),(0x801c,4,0x42200000),
                (0x8000,1,255),(0x800c,4,0),(0xa05c,4,0),(0xf00c,4,0),
                (0xa060,4,0),(0xa044,4,0xffffffff),(0x6010,4,ONE),(0x600c,4,0x28)):
                original=next(v[1] for v in trail_services() if v[0]==service)
                mode=8 if service==104 else 1
                put(op,args,trail_state(),scripts=replace_script(trail_services(),service,original,mode,at,width,value),strict=True)
    for allocation in (0,0xb000):
        for op,args in trail_probes[1:6]:
            put(op,args,trail_state(),scripts=replace_script(trail_services(),100,allocation),strict=True)

    trail_count=len(rows)
    trail_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert trail_count == 64432 and trail_hash == '1d3bc3b81f8355d66b4c424e39b3997b49c5f0e711862d5cbf3912720fe8af07'

    def frost_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(R.G_WORLD,4,0x7000),
            (0x200c,4,0x2f8b24),(0x2014,4,0x2f8840),
            (0x800c,4,0x4081),(0x8010,4,ONE),(0x8014,4,TWO),(0x8018,4,0x41c00000),
            (0x801c,4,0x41400000),(0x8020,4,0x42900000),(0x8038,2,12345),
            (0x8044,4,seed),(0x8048,4,0x51a838),(0x804c,4,0xf000),(0x8054,4,0xf000),
            (0xf00c,4,0x3000),(0x8094,2,7),(0x8098,4,0xffffff00),
            (0x809c,4,0x43af0000),(0x80a0,4,0xc2a00000),(0x80a4,4,0),
            (0x80c0,4,60),(0x80c8,4,0x1000),(0x80cc,4,0x6000),
            (0x8058,2,7),(0x8060,4,0x43af0000),(0x8064,4,0xc2a00000),
            (0x8068,4,0),(0x806c,4,60),(0x8074,4,0x1000),(0x8078,4,0x6200),
            (0x51a85c,4,0x2f77c0),(0x51a86c,4,0x2f84e4),
            (0x51a8ec,4,0x2f6b3c),(0x51a8fc,4,0x2f71ac)]
        for other in (0x6000,0x6100):
            p += [(other+0xc,4,0x20),(other+0x10,4,ONE),(other+0x14,4,TWO),
                  (other+0x18,4,0x41c00000),(other+0x1c,4,0x41400000),(other+0x20,4,0x42900000)]
        for i in range(5):
            for off,width,value in ((0xe,2,1),(0x18,2,[109,2,37,7,5][i]),(0x24,4,ZERO),
                                    (0x38,2,1),(0x42,2,[31,1,17,9,5][i]),(0x4c,4,ZERO)):
                p.append((0x1000+off+width*i,width,value))
        return p

    def frost_services():
        return [(100,0xb000,0,0,1,0),(102,71,0,0,1,0),(103,0,0,0,1,0),
            (104,0,0,0,1,0),(105,0,0,0,1,0),(115,0,0,0,1,0),
            (117,0,0,0,1,0),(118,0,0,0,1,0),(127,0,0,0,1,0),
            (130,0,0,0,1,0),(132,1,0,0,1,0),(146,0,0,0,1,0),
            (147,0,0,0,1,0),(148,0,0,0,1,0),(150,0,0,0,1,0),(151,0,0,0,1,0),
            (152,0,0,0,1,0),(153,0x7100,0,0,1,0),(154,0,0,0,1,0),
            (155,0,0,0,1,0),(156,0,0,0,1,0),(157,0,0,0,1,0),
            (158,0,0,0,1,0),(159,0,0,0,1,0)]

    frost_probes=[('shockoffrost_can_use',[0x1000,0x3000]),
        ('shockoffrost_cast',[0x1000,7,0x3000,ONE,TWO,0,12345]),
        ('shockoffrost_ctor',[0x9000,ONE,TWO,0,12345,7,0x3000,0x1000]),
        ('frostshard_ctor',[0x9000,ONE,TWO,0,12345,7,0x3000,0x1000,0x6000]),
        ('projectile_ctor',[0x9000,ONE,TWO,0,0x3000,0]),
        ('move_and_collide',[0x8000,0x43af0000,0xc2a00000,0]),
        ('_frost_debris',[0x8000,0x809c]),('shockoffrost_run',[0x8000]),
        ('frostshard_run',[0x8000]),('_frost_hit',[0x8000,0x6000,0,7,0xc4]),
        ('shockoffrost_hit',[0x8000,0x6000]),('frostshard_hit',[0x8000,0x6000]),
        ('puff',[0x8000,0x1234,ONE,TWO,0,1,0x41100000,0x41a00000])]
    for config in range(16):
        for op,args in frost_probes:
            put(op,args,frost_state(),config,frost_services(),strict=True)
        for level,facing in ((0,0),(1,32767),(128,32768),(255,65535),(-1,-32769),(-32768,0x7fffffff),(0x7fffffff,-0x80000000)):
            for owner in (0,0x3000):
                for op,args in [('shockoffrost_ctor',[0x9000,ONE,TWO,0,facing,level,owner,0x1000]),
                                ('frostshard_ctor',[0x9000,ONE,TWO,0,facing,level,owner,0x1000,0x6000])]:
                    put(op,args,frost_state(),config,frost_services(),strict=True)
        for frame in (-0x80000000,-1,0,59,60,61,0x7fffffff):
            for op in ('shockoffrost_run','frostshard_run'):
                put(op,[0x8000],frost_state()+[(R.G_SIM_FRAME,4,frame)],config,frost_services(),strict=True)
        for seed in (0,1,2,3,7,31,0x12345678,0x80000000,0xffffffff):
            put('_frost_debris',[0x8000,0x809c],frost_state(seed),config,frost_services(),strict=True)
        for flags in (0,0x20,0x40,0x60):
            for owner in (0,0xf000):
                for immune in (0,0x200):
                    for op in ('shockoffrost_hit','frostshard_hit'):
                        put(op,[0x8000,0x6000],frost_state()+[(0x600c,4,flags),(0x60a8,4,immune),(0x804c,4,owner)],config,frost_services(),strict=True)
        for d in (-32768,-1,0,1,32767):
            put('_frost_hit',[0x8000,0x6000,0,7,0xc4],frost_state()+[(0x1018,2,d)],config,frost_services(),strict=True)
        # Query flags, vertical and exact planar-touch filtering; duplicate callback order.
        for flags,z,x in ((0x201,0,0),(0x20000000,0,0),(0x20,0x43800000,ONE),
                          (0x20,0xc3800000,ONE),(0x20,0x41c00000,0x41c80000),
                          (0x20,0x41c00000,0x41c7ffff),(0x20,0x41c00000,ONE)):
            p=frost_state()+[(0x600c,4,flags),(0x6018,4,z),(0x6010,4,x)]
            services=replace_script(frost_services(),104,0x6000,9,0x6100,1,0x6000)
            put('move_and_collide',[0x8000,0,0,0],p,config,services,strict=True)
        for water in (0,1):
            for splashed in (0,1,255):
                services=replace_script(frost_services(),146,0x42c80000)
                services=replace_script(services,152,water)
                put('move_and_collide',[0x8000,0x43af0000,0,0xc2c80000],frost_state()+[(0x8050,1,splashed)],config,services,strict=True)
        for terrain in (0x80000000,0x7fffffff,0xffffffff):
            put('move_and_collide',[0x8000,0,0,0],frost_state(),config,replace_script(frost_services(),146,terrain),strict=True)
        for spread in (0,1):
            put('puff',[0x8000,0x1234,ONE,TWO,0,spread,0xbf800000,0x41a00000],frost_state(),config,frost_services(),strict=True)
        for op,args in [('object_call',[0x8000,4]),('object_call',[0x8000,6,0x6000]),
                        ('call',[0x1000,1,0x3000]),('call',[0x1000,2,7,0x3000,ONE,TWO,0,12345])]:
            put(op,args,frost_state(),config,frost_services(),strict=True)
        # Enemy refusal, already-hit and excluded-object paths.
        put('shockoffrost_hit',[0x8000,0x6000],frost_state(),config,replace_script(frost_services(),132,0),strict=True)
        for off in (0x70,0x78):
            put('frostshard_hit',[0x8000,0x6000],frost_state()+[(0x8000+off,4,1 if off==0x70 else 0x6000)],config,frost_services(),strict=True)
        # Real shard hit dispatch and collision -> hit -> expiry chain.
        put('object_call',[0x8000,6,0x6000],frost_state()+[(0x8048,4,0x51a8c8)],config,frost_services(),strict=True)
        for op in ('shockoffrost_run','frostshard_run'):
            p=frost_state()+([] if op=='shockoffrost_run' else [(0x8048,4,0x51a8c8)])
            put(op,[0x8000],p,config,replace_script(frost_services(),104,0x6000,9,0x6100,1,0x6000),strict=True)

    # Every external boundary: failure prefixes, missing callback and callback-visible mutation.
    for op,args in frost_probes[1:]:
        for service in [v[0] for v in frost_services()]:
            put(op,args,frost_state(),scripts=[v for v in frost_services() if v[0]!=service],strict=True)
            put(op,args,frost_state(),scripts=replace_script(frost_services(),service,0,2,0x7004,4,77),strict=True)
            original=next(v[1] for v in frost_services() if v[0]==service)
            for at,width,value in ((0x8010,4,0x41c00000),(0x804c,4,0),(0x800c,4,0),
                                   (R.G_SIM_FRAME,4,61),(0x8094,2,255),(0x80cc,4,0x6100)):
                put(op,args,frost_state(),scripts=replace_script(frost_services(),service,original,1,at,width,value),strict=True)
    # Splash failures and ground backtracking exit on second terrain call.
    for service in (152,153,154,102,130):
        services=replace_script(frost_services(),146,0x42c80000)
        services=replace_script(services,152,1)
        put('move_and_collide',[0x8000,0x43af0000,0,0],frost_state(),scripts=[v for v in services if v[0]!=service],strict=True)
        put('move_and_collide',[0x8000,0x43af0000,0,0],frost_state(),scripts=replace_script(services,service,0,2,0x7004,4,77),strict=True)
    put('move_and_collide',[0x8000,0x43af0000,0,0],frost_state(),scripts=replace_script(frost_services(),146,0x42c80000,4,0,1,0),strict=True)
    for allocation in (0,0xb000):
        for op,args in frost_probes[1:9]:
            put(op,args,frost_state(7),scripts=replace_script(frost_services(),100,allocation),strict=True)
    for count in (-1,0,1,3):
        services=replace_script(frost_services(),13,count)
        for terrain in (ZERO,ONE,0xbf800000):
            put('shockoffrost_run',[0x8000],frost_state(),scripts=replace_script(services,158,terrain),external=(13,),strict=True)
    for adjust in (-1,1):
        for slot,body in ((4,0x2f77c0),(6,0x2f84e4),(4,0x2f6b3c),(6,0x2f71ac)):
            p=frost_state()+[(0x51a838+8*slot,2,adjust),(0x51a83c+8*slot,4,body)]
            put('object_call',[0x8000,slot]+([0x6000] if slot==6 else []),p,scripts=frost_services(),strict=True)

    frost_count=len(rows)
    frost_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert frost_count == 68847 and frost_hash == '249b5ed5fbe45377b04d444506829a6192b82a22c9a46d7cc5e8c590b8f898cc'

    def flare_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(0x200c,4,0x2dd8b8),(0x2014,4,0x2dd4e8),
            (0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0x41c00000),
            (0x8014,4,0x5194f8),(0x800c,4,0x3000),(0x8018,4,0xf000),
            (0x801c,4,0x3000),(0x8020,4,7),(0x8024,4,0x1000),(0x8028,4,120),
            (0x8068,4,79),(0x806c,4,750),(0x8070,4,24),(0x8074,4,77),
            (0x519504,4,0x2dd0c8),(0x51950c,4,0x2dd1ac),
            (0xf000,4,0xf100),(0xf00c,4,0x9000),(0xf100,4,0xf200),(0xf10c,4,0),
            (0xf200,4,0),(0xf20c,4,0xa000)]
        for obj in (0x9000,0xa000):
            p += [(obj+0xc,4,0x421d),(obj+0x10,4,ONE),(obj+0x14,4,TWO),(obj+0x18,4,0x41c00000),
                (obj+0x1c,4,0x41c00000),(obj+0x44,4,seed),(obj+0x48,4,0x5193e0),
                (obj+0x4c,4,0x3000),(obj+0x50,4,7),(obj+0x54,4,0x1000),(obj+0x58,4,0),
                (obj+0x5c,2,32760),(obj+0x5e,2,0),(obj+0x60,4,ONE),(obj+0x64,4,TWO),(obj+0x68,4,0x41c00000)]
        p += [(0x519404,4,0x2dbf10)]
        for other in (0x6000,0x6100):
            p += [(other+0x10,4,0x40a00000),(other+0x14,4,0xc0a00000),(other+0x18,4,0x41c00000)]
        for i in range(5):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,[109,38,8,3,5][i]),(0x1024+4*i,4,0)]
        return p

    def flare_services():
        return [(100,0xb000,0,0,1,0),(102,71,0,0,1,0),(103,77,0,0,1,0),
            (104,0,0,0,1,0),(105,0,0,0,1,0),(113,0,0,0,1,0),(115,0,0,0,1,0),
            (117,0,0,0,1,0),(118,0,0,0,1,0),(119,0,0,0,1,0),(126,0,0,0,1,0),
            (127,0,0,0,1,0),(130,0,0,0,1,0),(132,1,0,0,1,0),(134,0,0,0,1,0),
            (147,0,0,0,1,0),(148,0,0,0,1,0),(150,0,0,0,1,0),(151,0,0,0,1,0),
            (160,0,0,0,1,0),(161,0,0,0,1,0),(162,0,0,0,1,0),
            (0,R.Model.FIREFLARES_GETTER,6,0x123408,1,0)]

    flare_probes=[('fireflares_can_use',[0x1000,0x3000]),
        ('fireflares_cast',[0x1000,7,0x3000,ONE,TWO,0,12345]),
        ('fireflares_ctor',[0x8000,7,0x3000,ONE,TWO,0,0x1000]),
        ('fireflares_dtor',[0x8000,3]),('fireflares_run',[0x8000,ONE,TWO,0,0]),
        ('fireflare_ctor',[0x9000,ONE,TWO,0,7,0x3000,0x1000,32767]),
        ('fireflare_run',[0x9000]),('spell_dtor_call',[0x8000,3]),
        ('spell_call',[0x8000,2,ONE,TWO,0,0]),('object_call',[0x9000,4]),
        ('call',[0x1000,1,0x3000]),('call',[0x1000,2,7,0x3000,ONE,TWO,0,0])]
    for config in range(16):
        for op,args in flare_probes:
            put(op,args,flare_state(),config,flare_services(),strict=True)
        # Full word level, two integer divisions (including zero divisor), count cap,
        # unsuccessful individual allocation, signed duration/frame wrap.
        for level in (0,1,7,255,256,-1,-27,-32,-33,-32768,0x7fffffff,0x80000000):
            for count in (-1,0,1,16,17):
                services=replace_script(flare_services(),10,count)
                put('fireflares_ctor',[0x8000,level,0x3000,ONE,TWO,0,0x1000],flare_state(),config,services,external=(10,),strict=True)
        for allocation in (0,0xb000):
            put('fireflares_cast',flare_probes[1][1],flare_state(),config,replace_script(flare_services(),100,allocation),strict=True)
        # Sound, list cleanup and expiry signed boundaries. Null list objects in run
        # intentionally write to address 0 rather than being skipped.
        for frame in (-1,0,119,120,121,0x7fffffff,0x80000000):
            for radius in (-1,0,78,79,80,0x7fffffff):
                p=flare_state()+[(R.G_SIM_FRAME,4,frame),(0x8070,4,radius)]
                put('fireflares_run',flare_probes[4][1],p,config,flare_services(),strict=True)
        for flags in (0,1,2,3,0xffffffff):
            for head in (0,0xf000,0xf100):
                put('fireflares_dtor',[0x8000,flags],flare_state()+[(0x8018,4,head)],config,flare_services(),strict=True)
        put('fireflares_run',flare_probes[4][1],flare_state()+[(0x8018,4,0)],config,flare_services(),strict=True)
        # Carrying lookup: empty nodes, matching/nonmatching spell and getter.
        for identity in (0x123408,0x123409):
            p=flare_state()+[(0x3364,4,0xf300),(0xf300,4,0xf400),(0xf30c,4,0),
                (0xf400,4,0),(0xf40c,4,0x8000),(0x5194fc,4,0x1234)]
            ss=flare_services()+[(0,0x1234,6,identity,1,0)]
            put('fireflares_can_use',[0x1000,0x3000],p,config,ss,strict=True)
        # Hit queries preserve order and duplicates; first enemy alone triggers direct
        # damage, then all enemies receive explosion damage (including that victim).
        for frame in (-10,-1,0,9,10,11):
            for d in (-32768,-1,0,1,32767):
                for explosion in (0,38):
                    p=flare_state()+[(R.G_SIM_FRAME,4,frame),(0x1018,2,d),(0x101a,2,explosion)]
                    ss=replace_script(flare_services(),104,0x6000,9,0x6100,1,0x6000)
                    put('fireflare_run',[0x9000],p,config,ss,strict=True)
        put('fireflare_run',[0x9000],flare_state(),config,replace_script(replace_script(flare_services(),104,0x6000,8),132,0),strict=True)
        put('fireflare_run',[0x9000],flare_state(),config,replace_script(replace_script(flare_services(),104,0x6000,9,0x6100,1,0x6000),132,0,4,0,1,1),strict=True)
        for size in (-32768,-1,0,65,66,71,72,32767):
            for explosion in (0,38):
                p=flare_state()+[(0x9058,4,1),(0x905e,2,size),(0x101a,2,explosion)]
                put('fireflare_run',[0x9000],p,config,flare_services(),strict=True)
        for seed in (0,1,7,31,0x12345678,0x80000000,0xffffffff):
            put('fireflare_run',[0x9000],flare_state(seed)+[(0x9058,4,1)],config,flare_services(),strict=True)
        for angle in (-32768,-1,0,32767,65535):
            put('fireflare_ctor',[0x9000,ONE,TWO,0,255,0x3000,0x1000,angle],flare_state(),config,flare_services(),strict=True)

    # Callback failure/missing-service prefixes and field mutation. Exercise the
    # explosion and burst branches separately so no service is masked by an early exit.
    probes=flare_probes+[('fireflare_run',[0xa000])]
    for op,args in probes:
        p=flare_state()+([(0xa058,4,1)] if args==[0xa000] else [])
        services=replace_script(flare_services(),104,0x6000,9,0x6100,1,0x6000)
        for service in [v[0] for v in services if v[2]!=6]:
            put(op,args,p,scripts=[v for v in services if v[0]!=service],strict=True)
            put(op,args,p,scripts=replace_script(services,service,0,2,0x7004,4,77),strict=True)
            original=next(v[1] for v in services if v[0]==service)
            for address,width,value in ((0x8018,4,0),(0xf000,4,0),(0xf00c,4,0xa000),
                    (0x8070,4,-1),(0x9010,4,0x41c00000),(0x904c,4,0),
                    (0x9058,4,1),(0x905e,2,32767),(0x9054,4,0),(R.G_SIM_FRAME,4,61)):
                put(op,args,p,scripts=replace_script(services,service,original,8 if service==104 else 1,address,width,value),strict=True)
    for adjust in (-1,1):
        for op,args,row in [('call',[0x1000,1,0x3000],0x2008),
                ('call',[0x1000,2,7,0x3000,ONE,TWO,0,0],0x2010),
                ('spell_call',[0x8000,2,ONE,TWO,0,0],0x519508),
                ('spell_dtor_call',[0x8000,3],0x519500),('object_call',[0x9000,4],0x519400)]:
            put(op,args,flare_state()+[(row,2,adjust)],scripts=flare_services(),strict=True)

    # Homing attack projectiles: pin every previously committed complete-state row.
    flares_count=len(rows)
    flares_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert flares_count == 75873 and flares_hash == 'fb9192cfa2cfbfc77f679a9dc65fa87c6b2956ffb823b285b0422c663f7285d3'

    def homing_state(family, seed=1):
        holy=family=='holystrike'
        table=0x51a078 if holy else 0x5183d8
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(R.G_WORLD,4,0x7000),
            (0x200c,4,0x2ed20c if holy else 0x2cb368),(0x2014,4,0x2ecfcc if holy else 0x2cb1c0),
            (0x300c,4,0x60),(0x800c,4,0x4081),(0x8010,4,ONE),(0x8014,4,TWO),
            (0x8018,4,0x41c00000),(0x801c,4,0x41400000),(0x8020,4,0x42900000),
            (0x8038,2,12345),(0x8044,4,seed),(0x8048,4,table),
            (0x804c,4,0xf000),(0x8054,4,0xf000),(0x8058,4,0xf100),(0x8078,4,0),
            (0xf00c,4,0x3000),(0xf10c,4,0x6000),
            (table+0x24,4,0x2ebc50 if holy else 0x2ca1a8),
            (table+0x34,4,0x2ecae8 if holy else 0x2caec8)]
        p += ([(0x805c,2,7),(0x8060,4,240),(0x8064,4,50),(0x8068,4,0x1000),
                (0x806c,4,0x43e10000),(0x8070,4,0),(0x8074,4,0),(0x807c,4,17)] if holy else
              [(0x805c,4,0x1000),(0x8060,2,7),(0x8064,4,0x43af0000),(0x8068,4,0),
                (0x806c,4,0),(0x8070,4,360),(0x8074,4,50)])
        for other in (0x6000,0x6100):
            p += [(other+0xc,4,0x20),(other+0x10,4,0x42200000),(other+0x14,4,0xc2200000),
                (other+0x18,4,0x41c00000),(other+0x1c,4,0x41400000),(other+0x20,4,0x42900000)]
        for i in range(5):
            for off,width,value in ((0xe,2,1),(0x18,2,[109,173,2,7,5][i]),(0x24,4,ZERO),
                                    (0x38,2,1),(0x42,2,[31,1,17,9,5][i]),(0x4c,4,ZERO)):
                p.append((0x1000+off+width*i,width,value))
        return p

    def homing_services():
        return [(100,0xb000,0,0,1,0),(101,0,0,0,1,0),(102,71,0,0,1,0),
            (104,0,7,0,1,0),(105,0,0,0,1,0),(115,0,0,0,1,0),(127,0,0,0,1,0),
            (130,0,0,0,1,0),(131,0x6000,9,0x6100,1,0x6000),(132,1,0,0,1,0),
            (134,0,0,0,1,0),(146,0,0,0,1,0),(147,0,0,0,1,0),
            (148,0,0,0,1,0),(150,0,0,0,1,0),(151,0,0,0,1,0),
            (152,0,0,0,1,0),(153,0x7100,0,0,1,0),(154,0,0,0,1,0),(157,0,0,0,1,0),
            (159,0,0,0,1,0),(163,17,0,0,1,0),(164,0,0,0,1,0)]

    for family in ('diseasebolt','holystrike'):
        holy=family=='holystrike'
        expiry=0x8060 if holy else 0x8070
        arm=0x8064 if holy else 0x8074
        level_at=0x805c if holy else 0x8060
        stat_slot=10 if holy else 13
        table=0x51a078 if holy else 0x5183d8
        probes=[(family+'_can_use',[0x1000,0x3000]),
            (family+'_cast',[0x1000,7,0x3000,ONE,TWO,0,12345]),
            (family+'_ctor',[0x8000,12345,7,ONE,TWO,0,0x3000,0x1000]),
            (family+'_run',[0x8000]),(family+'_hit',[0x8000,0x6000])]
        for config in range(16):
            for op,args in probes:
                put(op,args,homing_state(family),config,homing_services(),strict=True)
            for level in (-65537,-32769,-32768,-1,0,1,7,32767,32768,65537):
                for owner in (0,0x3000):
                    for heading in (-32768,0,32767):
                        put(family+'_ctor',[0x8000,heading,level,ONE,TWO,0,owner,0x1000],
                            homing_state(family),config,homing_services(),strict=True)
                put(family+'_hit',[0x8000,0x6000],homing_state(family)+[(level_at,2,level)],config,homing_services(),strict=True)
            for allocation in (0,0xb000):
                for caster in (0,0x3000):
                    put(family+'_cast',[0x1000,7,caster,ONE,TWO,0,12345],homing_state(family),config,
                        replace_script(homing_services(),100,allocation),strict=True)
            # Signed expiry / arm boundaries, including wrapping frame additions.
            for frame in (-2147483648,-1,0,49,50,51,59,60,61,239,240,241,359,360,361,2147483647):
                put(family+'_run',[0x8000],homing_state(family)+[(R.G_SIM_FRAME,4,frame)],config,homing_services(),strict=True)
                put(family+'_ctor',probes[2][1],homing_state(family)+[(R.G_SIM_FRAME,4,frame)],config,homing_services(),strict=True)
            # Non-wrapped angle seam, equality at the rate threshold and negative /
            # overflowing signed32 rates. Null target nodes are deliberately followed.
            for heading,x,y in [(-32768,0xbf800000,0x80000000),(32767,0xbf800000,0),
                (-32768,ONE,0),(32767,ONE,0),(-182,ONE,0),(182,ONE,0),(0,ONE,0),
                (-1,0,ONE),(1,0,0xbf800000)]:
                for rate in (-2147483648,-1,0,1,2,32767,0x40000000,2147483647):
                    put(family+'_run',[0x8000],homing_state(family)+[(0x8038,2,heading),
                        (0x8010,4,0),(0x8014,4,0),(0x6010,4,x),(0x6014,4,y)],config,
                        homing_services()+[(stat_slot,rate,0,0,1,0)],external=(stat_slot,),strict=True)
            for node in (0,0xf100):
                for target in (0,0x6000):
                    put(family+'_run',[0x8000],homing_state(family)+[(0x8058,4,node),(0xf10c,4,target)],config,homing_services(),strict=True)
            # One-shot hit flags, both owner handles, target categories, immunity,
            # undead byte, PvP and signed statistic extremes.
            for node in (0,0xf000):
                for owner in (0,0x3000):
                    for flags in (0,8,0x20,0x40,0x60):
                        for immune in (0,0x200):
                            for enemy in (0,1):
                                put(family+'_hit',[0x8000,0x6000],homing_state(family)+[(0x8054,4,node),
                                    (0xf00c,4,owner),(0x600c,4,flags),(0x60a8,4,immune)],config,
                                    replace_script(homing_services(),132,enemy),strict=True)
            for damage in (-2147483648,-65537,-32768,-1,0,1,32767,65537,2147483647):
                for kind in (0,0x10,0xff):
                    for pvp in (0,1):
                        put(family+'_hit',[0x8000,0x6000],homing_state(family)+[(0x60b4,1,kind),(R.G_PVP_MODE_A,4,pvp)],config,
                            homing_services()+[(stat_slot,damage,0,0,1,0)],external=(stat_slot,),strict=True)
            for seed in (0,1,7,31,0x12345678,0x80000000,0xffffffff):
                for op,args in probes[3:]:
                    put(op,args,homing_state(family,seed),config,homing_services(),strict=True)
            for has_hit in (0,1,0xffffffff):
                put(family+'_hit',[0x8000,0x6000],homing_state(family)+[(0x8078,4,has_hit)],config,homing_services(),strict=True)
                # Ordered duplicate contacts invoke native onHit inside move.
                for terrain in (0,0x42c80000):
                    p=homing_state(family)+[(0x8078,4,has_hit),(0x6010,4,ONE),(0x6014,4,TWO),
                        (0x6110,4,ONE),(0x6114,4,TWO),(0x8058,4,0)]
                    ss=replace_script(homing_services(),104,0x6000,9,0x6100,1,0x6000)
                    ss=replace_script(ss,146,terrain)
                    put(family+'_run',[0x8000],p,config,ss,strict=True)
            for delta in (-32768,-1,0,1,32767):
                for op,args,row in [('call',[0x1000,1,0x3000],0x2008),
                    ('call',[0x1000,2,7,0x3000,ONE,TWO,0,12345],0x2010),
                    ('object_call',[0x8000,4],table+0x20),('object_call',[0x8000,6,0x6000],table+0x30)]:
                    put(op,args,homing_state(family)+[(row,2,delta)],config,homing_services(),strict=True)
        # Arc list order, first accepted target, no owner query, mutation and
        # service failures both during homing and expiry. Holy's arc mutates its
        # owned list via the callback; the returned query result is ignored.
        for mode in (7,8,9):
            for enemy_mode in (0,4):
                ss=replace_script(homing_services(),131,0x6000,mode,0x6100,1,0x6000)
                ss=replace_script(ss,132,0,enemy_mode,0,1,1)
                put(family+'_ctor',probes[2][1],homing_state(family),scripts=ss,strict=True)
        if holy:
            put(family+'_ctor',probes[2][1],homing_state(family),
                scripts=replace_script(homing_services(),131,0,1,0x8058,4,0xf100),strict=True)
        for op,args in probes+[ (family+'_run',[0xa000]) ]:
            p=homing_state(family)
            if args==[0xa000]:
                args=[0x8000]
                p += [(expiry,4,60)]
            ss=replace_script(homing_services(),104,0x6000,9,0x6100,1,0x6000)
            for service in [v[0] for v in ss]:
                put(op,args,p,scripts=[v for v in ss if v[0]!=service],strict=True)
                put(op,args,p,scripts=replace_script(ss,service,0,2,0x7004,4,77),strict=True)
                original=next(v[1] for v in ss if v[0]==service)
                for address,width,value in ((0x8058,4,0),(0xf10c,4,0),(0xf00c,4,0),
                    (0x8078,4,1),(0x8038,2,32767),(0x8010,4,0x41c00000),
                    (R.G_SIM_FRAME,4,61),(expiry,4,0),(arm,4,60),(level_at,2,65535)):
                    # Query mutation keeps its explicit list return shape.
                    put(op,args,p,scripts=replace_script(ss,service,original,8 if service in (104,131) else 1,
                        address,width,value),strict=True)


    homing_count=len(rows)
    homing_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert homing_count == 90846 and homing_hash == '5e7d0d2fca9ac2eb845dcc656e39434977b0d58a808d93bde21629fa09baf51d'

    # Authored area crowd-control family: distinct effect and victim spell storage.
    def blind_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,30),(0x200c,4,0x2f4b10),(0x2014,4,0x2f47dc),
            (0x8010,4,ONE),(0x8014,4,TWO),(0x8018,4,0x41c00000),(0x8044,4,seed),
            (0x8048,4,0x51a5e0),(0x804c,4,90),(0x8050,4,24),(0x51a604,4,0x2f42e4),
            (0x9000,1,7),(0x9008,4,0x1000),(0x900c,4,0x6000),(0x9010,4,seed),
            (0x9014,4,0x51a6f8),(0x9018,4,210),(0x901c,4,0x3000),(0x51a70c,4,0x2f4538)]
        for other in (0x6000,0x6100):
            p += [(other+0x10,4,ONE),(other+0x14,4,TWO),(other+0x18,4,0x41c00000),(other+0x228,4,0x42c80000)]
        for i,v in enumerate((101,35,3,2,5)):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,v),(0x1024+4*i,4,ZERO)]
        return p

    def blind_services():
        return [(100,0xb000,0,0,1,0),(101,0,0,0,1,0),(102,71,0,0,1,0),
            (104,0x6000,9,0x6100,1,0x6000),(105,0,0,0,1,0),(113,0,0,0,1,0),
            (115,0,0,0,1,0),(127,0,0,0,1,0),(130,0,0,0,1,0),
            (131,0x6000,9,0x6100,1,0x6000),(132,1,0,0,1,0),(133,0,0,0,1,0),
            (134,0,0,0,1,0),(147,0,0,0,1,0),(148,0,0,0,1,0),(150,0,0,0,1,0),
            (165,0,0,0,1,0),(166,0,0,0,1,0),(167,0,0,0,1,0)]

    blind_probes=[
        ('blindinglight_can_use',[0x1000,0x3000]),
        ('blindinglight_cast',[0x1000,7,0x3000,ONE,TWO,0,32768]),
        ('blindinglight_effect_ctor',[0x8000,ONE,TWO,0,7,0x3000,0x1000]),
        ('blindinglight_effect_run',[0x8000]),
        ('blindinglight_spell_ctor',[0x9000,7,0x3000,0x6000,0x1000]),
        ('blindinglight_spell_run',[0x9000])]
    for config in range(16):
        for op,args in blind_probes:
            put(op,args,blind_state(),config,blind_services(),strict=True)
        for level in (-2147483648,-65537,-129,-128,-1,0,1,127,128,255,256,65537,2147483647):
            put('blindinglight_cast',[0x1000,level,0x3000,ONE,TWO,0,0],blind_state(),config,blind_services(),strict=True)
            put('blindinglight_effect_ctor',[0x8000,ONE,TWO,0,level,0x3000,0x1000],blind_state(),config,blind_services(),strict=True)
            put('blindinglight_spell_ctor',[0x9000,level,0x3000,0x6000,0x1000],blind_state(),config,blind_services(),strict=True)
            put('blindinglight_spell_run',[0x9000],blind_state()+[(0x9000,1,level)],config,blind_services(),strict=True)
        for frame in (-2147483648,-61,-60,-31,-30,-1,0,1,29,30,31,59,60,89,90,209,210,2147483647):
            for op,args in blind_probes[2:]:
                put(op,args,blind_state()+[(R.G_SIM_FRAME,4,frame)],config,blind_services(),strict=True)
        for value in (-2147483648,-65537,-25,-3,-1,0,1,2,3,25,99,100,101,65537,2147483647):
            for boss in (0,1):
                put('blindinglight_effect_ctor',blind_probes[2][1],blind_state(),config,
                    replace_script(blind_services(),133,boss)+[(10,value,0,0,1,0)],external=(10,),strict=True)
            for hp in (0,0x80000000,ONE,0xbf800000,0x7fffffff,0xffffffff):
                put('blindinglight_spell_run',[0x9000],blind_state()+[(0x6228,4,hp)],config,
                    blind_services()+[(10,value,0,0,1,0)],external=(10,),strict=True)
            put('blindinglight_effect_run',[0x8000],blind_state()+[(0x8050,4,value)],config,blind_services(),strict=True)
            put('blindinglight_spell_ctor',blind_probes[4][1],blind_state(),config,
                blind_services()+[(10,value,0,0,1,0)],external=(10,),strict=True)
        for seed in (0,1,7,31,0x12345678,0x80000000,0xffffffff):
            for op,args in blind_probes[2:]:
                put(op,args,blind_state(seed),config,blind_services(),strict=True)
        for allocation in (0,0xb000):
            for enemy in (0,1):
                for boss in (0,1):
                    ss=replace_script(replace_script(replace_script(blind_services(),100,allocation),132,enemy),133,boss)
                    for op,args in blind_probes[1:3]:
                        put(op,args,blind_state(),config,ss,strict=True)
        for delta in (-32768,-1,0,1,32767):
            for op,args,row in [('call',[0x1000,1,0x3000],0x2008),
                ('call',[0x1000,2,7,0x3000,ONE,TWO,0,32768],0x2010),
                ('object_call',[0x8000,4],0x51a600),('spell_call',[0x9000,2],0x51a708)]:
                put(op,args,blind_state()+[(row,2,delta)],config,blind_services(),strict=True)
        for caster in (0,0x3000):
            for mode in (7,8,9):
                for enemy in (0,1):
                    ss=replace_script(replace_script(blind_services(),131,0x6000,mode,0x6100,1,0x6000),132,enemy)
                    put('blindinglight_cast',[0x1000,7,caster,ONE,TWO,0,0],blind_state(),config,ss,strict=True)
    # Independent radius/chance/duration values exercise wrapped radius and
    # boss rounding thresholds without coupling the statistics.
    for chance in (-3,-1,0,1,49,50,51,99,100,101):
        for boss in (0,1):
            p=blind_state()+[(0x1018,2,chance)]
            put('blindinglight_effect_ctor',blind_probes[2][1],p,scripts=replace_script(blind_services(),133,boss),strict=True)
    for op,args in blind_probes[1:]+[('blindinglight_effect_run',[0xa000]),('blindinglight_spell_run',[0xa100])]:
        p=blind_state()
        if args==[0xa000]: args=[0x8000]; p += [(0x804c,4,30)]
        if args==[0xa100]: args=[0x9000]; p += [(0x9018,4,30)]
        ss=blind_services()
        for service,original,*_ in ss:
            put(op,args,p,scripts=[v for v in ss if v[0]!=service],strict=True)
            put(op,args,p,scripts=replace_script(ss,service,0,2,0x7004,4,77),strict=True)
            for addr,width,value in ((0x8010,4,0x41c00000),(0x8050,4,0xffffffff),(0x900c,4,0x6100),
                (0x901c,4,0),(0x9008,4,0),(R.G_SIM_FRAME,4,60)):
                put(op,args,p,scripts=replace_script(ss,service,original,8 if service in (104,131) else 1,
                    addr,width,value),strict=True)

    # Preserve every existing row, including all Blinding Light vectors.
    blind_count=len(rows)
    blind_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert blind_count == 97626 and blind_hash == '248ccb90d3eed7302a7acdedd3cc9ff2a51fd176ae16f25b37a4c7db116ab12b'

    def disease_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,30),(0x200c,4,0x2ce2b0),(0x2014,4,0x2cdff8),
            (0x300c,4,0x60),(0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0x41c00000),
            (0x301c,4,0x41400000),(0x3020,4,0x42000000),
            (0x8000,1,7),(0x8008,4,0x1000),(0x800c,4,0x3000),(0x8010,4,seed),
            (0x8014,4,0x5186c0),(0x8198,4,0),(0x819c,4,0),(0x81a4,4,300),
            (0x81a8,4,60),(0x81ac,4,0x41f00000),(0x5186d4,4,0x2cd318),
            (0x9000,1,7),(0x9008,4,0),(0x900c,4,0x6000),(0x9010,4,seed),
            (0x9014,4,0x518db0),(0x9018,4,0xf000),(0xf00c,4,0x3000),
            (0x919c,4,0x6000),(0x91a0,4,300),(0x91a4,4,4),(0x91a8,4,7),(0x91ac,4,0x1000),
            (0x518dc4,4,0x2d5854),(R.G_GLOW_TEXTURE,4,0x4567)]
        for other in (0x6000,0x6100):
            p += [(other+0xc,4,0x20),(other+0x1c,4,0x41400000),(other+0x20,4,0x42000000),
                (other+0x68,4,0x3e800000)]
        for i,v in enumerate((35,5,4,2,5)):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,v),(0x1024+4*i,4,ZERO)]
        return p

    def disease_services():
        return [(100,0xb000,0,0,1,0),(102,71,0,0,1,0),(103,0,0,0,1,0),
            (104,0x6000,9,0x6100,1,0x6000),(105,0,0,0,1,0),(113,0,0,0,1,0),
            (115,0,0,0,1,0),(127,0,0,0,1,0),(132,1,0,0,1,0),(168,0,0,0,1,0),
            (999,0x3cd300,6,0x7711,1,0),(999,0x3cdd40,6,0x7722,1,0)]

    disease_probes=[('diseaseshield_can_use',[0x1000,0x3000]),
        ('diseaseshield_cast',[0x1000,7,0x3000]),
        ('diseaseshield_ctor',[0x8000,7,0x3000,0x1000]),
        ('diseaseshield_run',[0x8000,ONE,TWO,0,0]),
        ('disease_spell_ctor',[0x9000,4,7,0x6000,0x3000,0x1000]),
        ('disease_spell_run',[0x9000,ONE,TWO,0,0])]
    for config in range(16):
        for op,args in disease_probes:
            put(op,args,disease_state(),config,disease_services(),strict=True)
        for level in (0,1,127,128,255,256,65537,0xffffffff,0x80000000):
            for op,args in disease_probes[1:3]:
                put(op,[args[0],level]+args[2:],disease_state(),config,disease_services(),strict=True)
            put('disease_spell_ctor',[0x9000,4,level,0x6000,0x3000,0x1000],disease_state(),config,disease_services(),strict=True)
        for frame in (0,1,29,30,31,299,300,301,0xffffffff,0x80000000,0x7fffffff):
            for op,args in (disease_probes[3],disease_probes[5]):
                put(op,args,disease_state()+[(R.G_SIM_FRAME,4,frame)],config,disease_services(),strict=True)
        for radius in (0,0x80000000,ONE,0x41980000,0x41f00000,0x42480000,0xbf800000):
            put('diseaseshield_run',disease_probes[3][1],disease_state()+[(0x81ac,4,radius)],config,disease_services(),strict=True)
        for seconds in (-1,0,1,2,9):
            put('disease_spell_run',disease_probes[5][1],disease_state()+[(0x91a4,4,seconds)],config,disease_services(),strict=True)
            put('disease_spell_ctor',[0x9000,seconds,7,0x6000,0,0x1000],disease_state(),config,disease_services(),strict=True)
        for value in (0,1,0xffffffff,0x80000000,65537):
            for op,args in (disease_probes[2],disease_probes[3],disease_probes[5]):
                put(op,args,disease_state(),config,disease_services()+[(12,value,0,0,1,0)],external=[12],strict=True)
        # Both raw spell virtual run routes; signed this adjustments.
        for table,spell,body in ((0x5186c0,0x8000,0x2cd318),(0x518db0,0x9000,0x2d5854)):
            for adjust in (0,1,0xffff):
                p=disease_state()+[(table+16,2,adjust)]
                if adjust:
                    self=spell+(1 if adjust==1 else -1)
                    p += [(self+(0x1a4 if spell==0x8000 else 0x1a0),4,30)]
                put('spell_call',[spell,2,ONE,TWO,0,0],p,config,disease_services(),strict=True)
        for slot,args in ((1,[0x3000]),(2,[7,0x3000,ONE,TWO,0,0])):
            put('call',[0x1000,slot]+args,disease_state(),config,disease_services(),strict=True)
        for seed in (0,1,0xdeadbeef,0xffffffff):
            for op,args in (disease_probes[2],disease_probes[3],disease_probes[4],disease_probes[5]):
                p=disease_state(seed)
                for i in range(16):
                    for offset in (0x8018,0x901c):
                        p += [(offset+12*i+4*k,4,(0x42c80000,0xc2c80000,ONE)[k]) for k in range(3)]
                put(op,args,p,config,disease_services(),strict=True)
    # Existing attached status, absent/null/non-creature sources, friendly targets,
    # query duplicates/zeroes, failed allocation and disease resistance extremes.
    for op,args in (disease_probes[0],disease_probes[5]):
        other=0x3000 if op.endswith('can_use') else 0x6000
        for type_id in (0x7711,0x7722,0x7733):
            p=disease_state()+[(other+0x364,4,0xe000),(0xe00c,4,0xe100),(0xe114,4,0xe200),(0xe204,4,0x1234)]
            ss=disease_services()+[(999,0x1234,6,type_id,1,0)]
            put(op,args,p,scripts=ss,strict=True)
    for node,source,flags in ((0,0x3000,0x20),(0xf000,0,0),(0xf000,0x3000,0),(0xf000,0x3000,0x20)):
        for enemy in (0,1):
            for alloc in (0,0xb000):
                ss=replace_script(replace_script(disease_services(),132,enemy),100,alloc)
                put('disease_spell_run',disease_probes[5][1],disease_state()+[(0x9018,4,node),(0xf00c,4,source),(source+0xc,4,flags)],scripts=ss,strict=True)
    for resistance in (0,ONE,0xbf800000,0x41200000,0x7fffffff):
        for mode in (0,8,9):
            ss=replace_script(disease_services(),104,0 if mode==0 else 0x6000,mode,0x6100,1,0x6000)
            put('disease_spell_run',disease_probes[5][1],disease_state()+[(0x6068,4,resistance)],scripts=ss,strict=True)
    # Service failures and callback-visible mutations at each reachable boundary.
    for op,args in disease_probes[1:]:
        ss=disease_services()
        for service,original,mode,*_ in ss:
            if mode==6: continue
            put(op,args,disease_state(),scripts=[v for v in ss if v[0]!=service],strict=True)
            put(op,args,disease_state(),scripts=replace_script(ss,service,0,2,0x7004,4,77),strict=True)
            for addr,width,value in ((0x800c,4,0x6000),(0x8008,4,0),(0x81ac,4,0),
                (0x900c,4,0x6100),(0x919c,4,0x6100),(0x9018,4,0),(0x91ac,4,0),
                (R.G_SIM_FRAME,4,31)):
                put(op,args,disease_state(),scripts=replace_script(ss,service,original,8 if service==104 else 1,addr,width,value),strict=True)

    disease_count=len(rows)
    disease_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert disease_count==99939 and disease_hash=='e2fbadb97554a7c0195a2184b23b4b81ab0537c833583b6d7a73f3d95e25e7c4'

    def fire_state(sid=13,seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(0x200c,4,0x2e4df8 if sid==13 else 0x2f123c),
            (0x2014,4,0x2e4ad0 if sid==13 else 0x2f1078),
            (0x300c,4,0x60),(0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0x41c00000),
            (0x3050,4,0x4000),(0x600c,4,0x40),(0x63c8,4,1),
            (0x8000,1,7),(0x8008,4,0x1000),(0x800c,4,0x3000),(0x8010,4,seed),
            (0x8014,4,0x519a88 if sid==13 else 0x51a470),
            (0x8018,4,300),(0x801c,4,48),(0x805c,4,0),
            (0x519a9c,4,0x2e3b28),(0x51a484,4,0x2f02fc)]
        for i,v in enumerate((40,5,4,2,5)):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,v),(0x1024+4*i,4,ZERO)]
        for index in (0,1):
            rec=R.G_PLAYERS+index*R.REC_SIZE
            p += [(rec+R.REC_WEAPON_SETS,4,0xa000+index*0x200),(rec+R.REC_OFF_HAND,4,0xa100+index*0x200)]
        for item in (0xa000,0xa100,0xa200,0xa300):
            p += [(item+0x24+8*k,2,0xffff) for k in range(4)]
        return p

    def fire_services():
        return [(100,0x8000,0,0,1,0),(102,71,0,0,1,0),(103,73,0,0,1,0),
            (104,0x6000,9,0x3000,1,0x6000),(105,0,0,0,1,0),(113,0,0,0,1,0),
            (117,0,0,0,1,0),(118,0,0,0,1,0),(126,0,0,0,1,0),
            (127,0,0,0,1,0),(131,0x6000,9,0x3000,1,0x6000),(132,1,0,0,1,0),
            (108,1,0,0,1,0),(112,0,0,0,1,0),(169,0,0,0,1,0),(134,0,0,0,1,0),
            (999,0x3cec68,6,0x7711,1,0)]

    for sid,family in ((13,'fireweapons'),(18,'coneoffire')):
        probes=[(family+'_can_use',[0x1000,0x3000]),(family+'_cast',[0x1000,7,0x3000]),
            (family+'_ctor',[0x8000,7,0x3000,0x1000]),(family+'_run',[0x8000,ONE,TWO,0,0])]
        for config in range(16):
            for op,args in probes:
                put(op,args,fire_state(sid),config,fire_services(),strict=True)
            for level in (0,127,128,255,256,0xffffffff,0x80000000):
                for op,args in probes[1:3]:
                    put(op,[args[0],level]+args[2:],fire_state(sid),config,fire_services(),strict=True)
            for frame in (0,1,14,15,29,30,31,269,270,271,299,300,0xffffffff,0x80000000,0x7fffffff):
                put(probes[3][0],probes[3][1],fire_state(sid)+[(R.G_SIM_FRAME,4,frame)],config,fire_services(),strict=True)
            for facing in (1,16384,32767,32768,65535,0x80000000):
                put(probes[3][0],probes[3][1][:-1]+[facing],fire_state(sid),config,fire_services(),strict=True)
            for value in (0,1,4,31,0xffffffff,0x80000000):
                put(probes[3][0],probes[3][1],fire_state(sid),config,fire_services()+[(12,value,0,0,1,0)],external=[12],strict=True)
            for seed in (0,0xdeadbeef,0xffffffff):
                put(probes[3][0],probes[3][1],fire_state(sid,seed),config,fire_services(),strict=True)
            for slot,args in ((1,[0x3000]),(2,[7,0x3000,ONE,TWO,0,0])):
                put('call',[0x1000,slot]+args,fire_state(sid),config,fire_services(),strict=True)
            table=0x519a88 if sid==13 else 0x51a470
            for adjust in (0,1,0xffff):
                put('spell_call',[0x8000,2,ONE,TWO,0,32768],fire_state(sid)+[(table+16,2,adjust)],config,fire_services(),strict=True)
        # Missing callbacks, failures and synchronous mutations retain complete prefixes.
        for op,args in probes[1:]:
            for service,original,*_ in fire_services():
                if service==999: continue
                put(op,args,fire_state(sid),scripts=[v for v in fire_services() if v[0]!=service],strict=True)
                put(op,args,fire_state(sid),scripts=replace_script(fire_services(),service,0,2,0x7004,4,77),strict=True)
                for addr,value in ((0x800c,0x6000),(0x8008,0),(R.G_SIM_FRAME,31),(0x801c,0)):
                    # Preserve query shape while mutating memory.
                    mode=8 if service in (104,131) else 1
                    put(op,args,fire_state(sid),scripts=replace_script(fire_services(),service,original,mode,addr,4,value),strict=True)
        for flags in (0,0x20,0x28,0x40,0x48,0x60,0x68):
            for holder in (0,0x4000):
                for op,args in (probes[2],probes[3]):
                    put(op,args,fire_state(sid)+[(0x300c,4,flags),(0x3050,4,holder)],scripts=fire_services(),strict=True)
        for query in (104,131):
            for mode,value in ((0,0),(8,0),(8,0x3000),(9,0x6000)):
                for enemy in (0,1):
                    ss=replace_script(fire_services(),query,value,mode,0x6000,1,0x6000)
                    ss=replace_script(ss,132,enemy)
                    put(probes[3][0],probes[3][1],fire_state(sid)+[(R.G_PVP_MODE_A,4,1)],scripts=ss,strict=True)
        for alloc in (0,0x8000):
            put(probes[1][0],probes[1][1],fire_state(sid),scripts=replace_script(fire_services(),100,alloc),strict=True)
    # Fire-specific attached spell detection, enchantment minimum/full/thrown and PvP cases.
    for kind in (0x7711,0x7722):
        p=fire_state()+[(0x3364,4,0xe000),(0xe00c,4,0xe100),(0xe114,4,0xe200),(0xe204,4,0x1234)]
        put('fireweapons_can_use',[0x1000,0x3000],p,scripts=fire_services()+[(999,0x1234,6,kind,1,0)],strict=True)
    for current in (-1,0,1,30,31,40,300):
        for thrown in (0,0x1000000):
            p=fire_state()+[(0xa200+8,4,thrown),(0xa224,2,0x15),(0xa226,2,current)]
            put('fireweapons_run',[0x8000,ONE,TWO,0,0],p,scripts=fire_services(),strict=True)
    for same in (0,1):
        put('fireweapons_run',[0x8000,ONE,TWO,0,0],fire_state()+[(R.G_PVP_MODE_A,4,1)],scripts=replace_script(fire_services(),108,same),strict=True)

    fire_count=len(rows)
    fire_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert fire_count==102321 and fire_hash=='6ef047051d61fea147898038cd3d58db1ce53ad21b03eebeed001315c00f3019'

    # Wizard Beam: all stored chain state, tint, query/damage and presentation calls.
    def beam_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,40),(0x2014,4,0x2f3b0c),
            (0x8000,1,7),(0x8008,4,0x1000),(0x800c,4,0x3000),(0x8010,4,seed),
            (0x8014,4,0x51a5b0),(0x51a5c4,4,0x2f1a44),
            (0x8019,1,0),(0x801a,1,0),(0x80b8,4,2),
            (0x8344,4,0x41400000),(0x8348,4,0x41800000),(0x834c,4,0x41800000),
            (0x8378,4,0x80995533),(0x837c,4,80)]
        for i,v in enumerate((135,8,2,4,5)):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,v),(0x1024+4*i,4,ZERO)]
        for other,x,y in ((0x6000,72,0),(0x6100,68,4),(0x6200,64,-4)):
            p += [(other+0xc,4,0x20),(other+0x10,4,R.C.from_int(x)),
                (other+0x14,4,R.C.from_int(y)),(other+0x18,4,0),(other+0x20,4,TWO),
                (other+0x38,2,321),(other+0xdc,4,99)]
        for i in range(5): p += [(0x8350+8*i,4,100+i),(0x8354+8*i,4,32+i)]
        for k in range(6): p += [(0x80bc+4*k,4,200+k),(0x80d4+4*k,4,300+k),(0x832c+4*k,4,0x41400000)]
        for i in range(3): p += [(0x801c+4*i,4,400+i)]
        return p

    def beam_services():
        return [(100,0x8000,0,0,1,0),(102,71,0,0,1,0),(103,73,0,0,1,0),
            (104,0x6000,9,0x6100,1,0x6200),(107,51,0,0,1,0),
            (124,3,0,0,1,0),(125,0,0,0,1,0),(127,0,0,0,1,0),
            (132,1,0,0,1,0),(163,901,0,0,1,0),(170,0,0,0,1,0),
            (171,0,0,0,1,0),(172,0,0,0,1,0),(173,0,0,0,1,0),
            (174,ONE,9,TWO,1,0)]

    beam_probes=[('wizardbeam_cast',[0x1000,7,0x3000]),
                 ('wizardbeam_ctor',[0x8000,7,0x3000,0x1000]),
                 ('wizardbeam_run',[0x8000,0,0,0,0]),('_beam_flash',[0x8000,0x6000]),
                 ('_beam_chain',[0x8028,0x8070,6,ONE,0,0,0x8348])]
    for config in range(16):
        for seed in (0,1,0xdeadbeef,0xffffffff):
            for op,args in beam_probes:
                put(op,args,beam_state(seed),config,beam_services(),strict=True)
        for level in (0,1,127,128,255,256,-1,0x80000000):
            for op,args in beam_probes[:3]:
                args=list(args)
                if op.endswith(('cast','ctor')): args[1]=level
                put(op,args,beam_state()+[(0x8000,1,level)],config,beam_services(),strict=True)
        for frame in (-2147483648,-40,-32,-8,-1,0,7,8,19,20,29,30,31,39,40,2147483647):
            for initialized in (0,1):
                for op,args in (beam_probes[2],beam_probes[3]):
                    put(op,args,beam_state()+[(R.G_SIM_FRAME,4,frame),(0x8019,1,initialized),(0x801a,1,initialized)],config,beam_services(),strict=True)
        for beams in (-1,0,1,2,6,7):
            for op,args in beam_probes[:3]:
                put(op,args,beam_state()+[(0x101c,2,beams),(0x80b8,4,beams)],config,beam_services(),strict=True)
        for facing in (0,1,16384,32768,65535,0x80000000):
            for enemy in (0,1):
                ss=replace_script(beam_services(),132,enemy)
                put('wizardbeam_run',[0x8000,0,0,0,facing],beam_state(),config,ss,strict=True)
        for stat in (0,1,3):
            for value in (-32768,-1,0,1,32767):
                put('wizardbeam_run',beam_probes[2][1],beam_state()+[(0x1018+stat*2,2,value)],config,beam_services(),strict=True)
        # Both dispatch routes retain signed16 adjustments; direct adjusted state.
        for adjust in (0,1,0xffff):
            self=0x8000+(1 if adjust==1 else -1 if adjust else 0)
            p=beam_state()+[(0x51a5c0,2,adjust)]
            if adjust:
                p += [(self+8,4,0x1000),(self+0xc,4,0x3000),(self+0xb8,4,0),
                      (self+0x19,1,0),(self+0x348,4,0x41800000),(self+0x34c,4,0x41800000)]
            put('spell_call',[0x8000,2,0,0,0,0],p,config,beam_services(),strict=True)
        put('call',[0x1000,2,7,0x3000,ONE,TWO,0,0],beam_state(),config,beam_services(),strict=True)
        for n in (-1,0,1,2,4,6):
            for raw in (0,ONE,0x80000000,0x7fffffff,0xffffffff):
                p=beam_state()+[(0x8348,4,raw)]
                for i in range(6):
                    p += [(0x8028+12*i+4*a,4,v) for a,v in enumerate((raw,ONE,TWO))]
                    p += [(0x8070+12*i+4*a,4,v) for a,v in enumerate((TWO,raw,ONE))]
                put('_beam_chain',[0x8028,0x8070,n,raw,ONE,TWO,0x8348],p,config,strict=True)
    # Query order/ties/duplicates, sample count/divide, strict distance boundaries.
    for mode,value,addr,mutation in ((0,0,0,0),(8,0,0,0),(9,0x6000,0x6100,0x6100),(9,0x6100,0x6000,0x6200)):
        ss=replace_script(beam_services(),104,value,mode,addr,1,mutation)
        put('wizardbeam_run',beam_probes[2][1],beam_state(),scripts=ss,strict=True)
    for count in (0,1,-1,0x80000000,65536):
        put('wizardbeam_run',beam_probes[2][1],beam_state(),scripts=replace_script(beam_services(),124,count),strict=True)
    for x in (0,79,80,81,175,176):
        put('wizardbeam_run',beam_probes[2][1],beam_state()+[(0x6010,4,R.C.from_int(x))],scripts=replace_script(beam_services(),104,0x6000,8),strict=True)
    for op,args in beam_probes[:3]:
        for service,original,mode,*_ in beam_services():
            put(op,args,beam_state(),scripts=[v for v in beam_services() if v[0]!=service],strict=True)
            put(op,args,beam_state(),scripts=replace_script(beam_services(),service,0,2,0x7004,4,77),strict=True)
            for addr,width,value in ((0x800c,4,0x6100),(0x8008,4,0),(0x8000,1,255),
                    (0x80b8,4,0),(0x8348,4,ONE),(0x8378,4,0xff55aa),(R.G_SIM_FRAME,4,41),
                    (0x6038,2,65535),(0x6020,4,0)):
                if service==174: continue # scalar mutation mode would replace its vec3 shape
                ss=replace_script(beam_services(),service,original,8 if service==104 else 1,addr,width,value)
                put(op,args,beam_state(),scripts=ss,strict=True)
    put('wizardbeam_cast',beam_probes[0][1],beam_state(),scripts=replace_script(beam_services(),100,0),strict=True)

    beam_count=len(rows)
    beam_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert beam_count==105797 and beam_hash=='05522993a7a9b52b174087335178c9b5734450636da63a686edee25aca340311'

    # Blessed: the complete six-method player-support family, authored values only.
    def blessed_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,40),(R.G_GLOW_TEXTURE,4,73),
            (0x200c,4,0x2e000c),(0x2014,4,0x2dfeb8),
            (0x300c,4,0x40),(0x3010,4,ONE),(0x3014,4,TWO),(0x3018,4,0),
            (0x301c,4,0x41800000),(0x3020,4,0x42000000),
            (0x600c,4,0x20),(0x6010,4,TWO),(0x6014,4,ONE),
            (0x6018,4,ONE),(0x601c,4,TWO),(0x6020,4,0x41000000),
            (0x8000,1,7),(0x8008,4,0x1000),(0x800c,4,0x3000),(0x8010,4,seed),
            (0x8014,4,0x5196f0),(0x80b4,4,100),
            (0x5196fc,4,0x2df42c),(0x519704,4,0x2df4bc)]
        for stat,val in enumerate((135,8,3,4,5)):
            p += [(0x100e+2*stat,2,1),(0x1018+2*stat,2,val),(0x1024+4*stat,4,0)]
        for i in range(6):
            for k,val in enumerate((R.C.from_int(i*0x2aaa),ONE,0x41800000)):
                p.append((0x8018+12*i+4*k,4,val))
            for k,val in enumerate((R.C.from_int(160 if i&1 else -160),0,0)):
                p.append((0x8060+12*i+4*k,4,val))
        return p

    def blessed_services():
        return [(999,R.Model.BLESSED_GETTER,6,0x7711,1,0),
            (100,0x8000,0,0,1,0),(117,0,0,0,1,0),(118,0,0,0,1,0),
            (119,0,0,0,1,0),(161,0,0,0,1,0),(134,0,0,0,1,0),
            (162,0,0,0,1,0),(113,0,0,0,1,0),(168,0,0,0,1,0),(105,0,0,0,1,0)]
    blessed_probes=[('blessed_can_use',[0x1000,0x3000]),('blessed_cast',[0x1000,7,0x3000]),
        ('blessed_ctor',[0x8000,7,0x3000,0x1000]),('blessed_dtor',[0x8000,1]),
        ('blessed_stat0',[0x8000]),('blessed_run',[0x8000])]
    for config in range(16):
        for seed in (0,1,0xdeadbeef,0xffffffff):
            for op,args in blessed_probes:
                put(op,args,blessed_state(seed),config,blessed_services(),strict=True)
        for level in (0,1,66,127,128,190,255,256,-1,0x80000000):
            for op,args in blessed_probes[1:]:
                args=list(args)
                if op.endswith(('cast','ctor')): args[1]=level
                put(op,args,blessed_state()+[(0x8000,1,level)],config,blessed_services(),strict=True)
        for frame in (-2147483648,-100,-1,0,39,40,99,100,101,2147483647):
            for end in (-1,100,2147483647):
                put('blessed_run',[0x8000],blessed_state()+[(R.G_SIM_FRAME,4,frame),(0x80b4,4,end)],config,blessed_services(),strict=True)
        for n in (-32768,-1,0,1,5,6,7,32767):
            put('blessed_run',[0x8000],blessed_state()+[(0x101c,2,n)],config,blessed_services(),strict=True)
        for flags in (0,1,2,3,0xffffffff):
            put('blessed_dtor',[0x8000,flags],blessed_state(),config,blessed_services(),strict=True)
        # Radius/height and sparkle/rate raw extremes; clamps and signed angle conversion.
        for raw in (0,0x80000000,ONE,0xbf800000,0x7fffffff,0xffffffff):
            for addr in (0x301c,0x3020,0x8018,0x801c,0x8020,0x8060,0x8064,0x8068):
                put('blessed_run',[0x8000],blessed_state()+[(addr,4,raw)],config,blessed_services(),strict=True)
        for adjust in (0,1,0xffff):
            offset=1 if adjust==1 else -1 if adjust else 0
            p=blessed_state()+[(0x519700,2,adjust)]
            if adjust:
                p += [(0x8000+offset,1,7),(0x8008+offset,4,0x1000),(0x800c+offset,4,0x3000),
                      (0x80b4+offset,4,0)]
            put('spell_call',[0x8000,2,ONE,TWO,0,65535],p,config,blessed_services(),strict=True)
            p=blessed_state()+[(0x5196f8,2,adjust)]
            put('spell_dtor_call',[0x8000,1],p,config,blessed_services(),strict=True)
        put('call',[0x1000,1,0x3000],blessed_state(),config,blessed_services(),strict=True)
        put('call',[0x1000,2,7,0x3000,ONE,TWO,0,0],blessed_state(),config,blessed_services(),strict=True)
    # Empty/nonmatching/matching attached spell lists and allocation failure.
    for getter_kind in (0x7711,0x7722):
        p=blessed_state()+[(0x3364,4,0xe000),(0xe00c,4,0xe100),(0xe114,4,0xe200),(0xe204,4,0x1234)]
        for op,args in blessed_probes[:2]:
            put(op,args,p,scripts=blessed_services()+[(999,0x1234,6,getter_kind,1,0)],strict=True)
    put('blessed_cast',blessed_probes[1][1],blessed_state(),scripts=replace_script(blessed_services(),100,0),strict=True)
    # Every callback boundary, including destruction, and original-argument duration.
    for op,args in blessed_probes:
        for service,original,mode,*_ in blessed_services():
            if mode==6: continue
            put(op,args,blessed_state(),scripts=[v for v in blessed_services() if v[0]!=service],strict=True)
            put(op,args,blessed_state(),scripts=replace_script(blessed_services(),service,0,2,0x7004,4,77),strict=True)
            for addr,width,value in ((0x800c,4,0x6000),(0x8008,4,0),(0x8000,1,128),
                    (0x301c,4,0),(0x3020,4,ONE),(0x80b4,4,0),(R.G_SIM_FRAME,4,100)):
                put(op,args,blessed_state(),scripts=replace_script(blessed_services(),service,original,1,addr,width,value),strict=True)

    blessed_count=len(rows)
    blessed_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert blessed_count==109110 and blessed_hash=='90af1ec0bf135123768b61f1d0962a2a6209f413a77e3dca010c76309fd3efb4'

    # Complete offensive undead family: synthetic memory, no original skill tables.
    def undead_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(R.G_WORLD,4,0x7000),
            (0x2014,4,0x2f645c),(0x300c,4,0x60),(0x3038,2,0),
            (0x800c,4,0x4081),(0x8010,4,ONE),(0x8014,4,TWO),(0x8018,4,0x41c00000),
            (0x801c,4,0x41900000),(0x8020,4,0x42900000),(0x8038,2,12345),
            (0x8044,4,seed),(0x8048,4,0x50e908),(0x804c,4,0xf000),(0xf00c,4,0x3000),
            (0x8054,4,240),(0x8058,4,0x43700000),(0x805c,4,ZERO),(0x8060,4,ZERO),
            (0x8064,4,0x6000),(0x8068,4,TWO),(0x806c,4,ONE),(0x8070,4,0x42100000),
            (0x80b0,4,0xcafebabe),(0x80b4,4,17),(0x80b8,4,0x1000),(0x80bc,4,7),
            (0x50e92c,4,0x2f57b0),(0x50e93c,4,0x2f5fb4)]
        for other in (0x6000,0x6100,0x6200):
            p += [(other+0xc,4,0x20),(other+0xb4,1,0x10),(other+0x10,4,0x42200000),
                (other+0x14,4,0xc2200000),(other+0x18,4,0x41c00000),(other+0x1c,4,0x41400000),
                (other+0x20,4,0x42900000),(other+0x228,4,ONE)]
        for i,val in enumerate((109,8,3,7,5)):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,val),(0x1024+4*i,4,ZERO)]
        return p

    def undead_services():
        return [(100,0xb000,0,0,1,0),(101,0,0,0,1,0),(102,71,0,0,1,0),
            (103,0,0,0,1,0),(104,0,7,0,1,0),(105,0,0,0,1,0),
            (115,0,0,0,1,0),(117,0,0,0,1,0),(118,0,0,0,1,0),
            (127,0,0,0,1,0),(130,9,0,0,1,0),(132,1,0,0,1,0),
            (146,0,0,0,1,0),(147,0,0,0,1,0),(148,0,0,0,1,0),
            (150,0,0,0,1,0),(151,0,0,0,1,0),(152,0,0,0,1,0),
            (153,0x7100,0,0,1,0),(154,0,0,0,1,0),(158,0,0,0,1,0),
            (159,0,0,0,1,0),(163,17,0,0,1,0),(164,0,0,0,1,0),
            (175,0,0,0,1,0),(176,0,0,0,1,0),(177,0,0,0,1,0)]

    undead_probes=[('repulse_cast',[0x1000,7,0x3000,ONE,TWO,0,0]),
        ('damage_radial',[0x3000,ONE,TWO,0,109,0xa100900,0,0x40,0x42c00000,0x43400000]),
        ('dismissundead_cast',[0x1000,7,0x3000,ONE,TWO,0,12345]),
        ('holybolt_ctor',[0x8000,ONE,TWO,0,12345,7,0x3000,0x1000]),
        ('holybolt_run',[0x8000]),('holybolt_hit',[0x8000,0x6000])]
    for config in range(16):
        for seed in (0,1,0x80000000,0xffffffff,0x12345678):
            for op,args in undead_probes:
                put(op,args,undead_state(seed),config,undead_services(),strict=True)
        # Full signed level registers; Repulse levels restricted to terminating walks.
        for level in (0,1,4,20,255,0xffffffff,0xfffffffd):
            for op,args in undead_probes[:1]:
                aa=args.copy();aa[1]=level
                put(op,aa,undead_state(),config,undead_services(),strict=True)
        for level in (0,1,127,128,255,65535,0x7fffffff,0x80000000,0xffffffff):
            for op,args in undead_probes[2:4]:
                aa=args.copy();aa[1 if op=='dismissundead_cast' else 5]=level
                put(op,aa,undead_state(),config,undead_services(),strict=True)
            put('holybolt_hit',[0x8000,0x6000],undead_state()+[(0x80bc,4,level)],config,undead_services(),strict=True)
        for frame in (0,59,60,239,240,241,0x7fffffff,0x80000000,0xffffffff):
            for end in (0,60,240,0x80000000):
                put('holybolt_run',[0x8000],undead_state()+[(R.G_SIM_FRAME,4,frame),(0x8054,4,end)],config,undead_services(),strict=True)
        # Wrapped steering and exact signed16 vertical dead zone/conversion.
        for head in (0,1,0x443,0x444,0x4000,0x7fff,0x8000,0xffff):
            for target in (0,0x6000):
                put('holybolt_run',[0x8000],undead_state()+[(0x8038,2,head),(0x8064,4,target)],config,undead_services(),strict=True)
        for dz in (-65546,-32768,-10,-9,-1,0,9,10,32767,65546):
            put('holybolt_run',[0x8000],undead_state()+[(0x8064,4,0),(0x8070,4,R.C.from_int(dz)),(0x8018,4,ZERO)],config,undead_services(),strict=True)
        # Terrain-only bounce, raycasts, and collision+terrain (result 3: no bounce).
        for ray in (ZERO,ONE,0xbf800000,0x7fffffff):
            ss=replace_script(undead_services(),146,0x42c80000)
            ss=replace_script(ss,158,ray)
            for query in ([],[(104,0x6000,9,0x6100,1,0x6000)]):
                ss2=[v for v in ss if v[0]!=104]+query if query else ss
                put('holybolt_run',[0x8000],undead_state(),config,ss2,strict=True)
        # Ordered target list including duplicates, null owner, side selection, skips.
        for query in ((0x6000,0x6100,0x6200),(0x6200,0x6100,0x6000),(0x8000,0x3000,0x6000),(0,0x6000,0x6000)):
            ss=[v for v in undead_services() if v[0]!=104]+[(104,query[0],9,query[1],1,query[2])]
            for flags in (0x20,0x28,0x40,0):
                for back in (ZERO,0xc2200000):
                    p=undead_state()+[(0x600c,4,flags),(0x6110,4,back),(0x6210,4,0x42c80000)]
                    put('holybolt_ctor',undead_probes[3][1],p,config,ss,strict=True)
        for flags in (0,0x20,0x28,0x60,0x20000000):
            for kind in (0,0x10,0x80,0xff):
                p=undead_state()+[(0x600c,4,flags),(0x60b4,1,kind)]
                put('holybolt_hit',[0x8000,0x6000],p,config,undead_services(),strict=True)
        for owner in (0,0xf000):
            for hp in (ZERO,ONE,0xbf800000,0x80000000,0x7fffffff):
                for enemy in (0,1):
                    ss=replace_script(undead_services(),132,enemy)
                    put('holybolt_hit',[0x8000,0x6000],undead_state()+[(0x804c,4,owner),(0x6228,4,hp),(0x8054,4,240)],config,ss,strict=True)
        # Radial damage: octagonal boundary, radius, clamps, flags, duplicates, NaNs.
        query=[(104,0x6000,9,0x6100,1,0x6000)]
        ss=[v for v in undead_services() if v[0]!=104]+query
        for flags in (0,0xa100900):
            for kind in (0,0x10):
                for radius in (ZERO,0x41400000,0xbf800000):
                    p=undead_state()+[(0x60b4,1,kind),(0x601c,4,radius)]
                    for inner,outer in ((ZERO,ONE),(ONE,ONE),(0x42c00000,0x43400000),(0x43400000,0x42c00000),(0x7fffffff,0x7fffffff)):
                        aa=undead_probes[1][1].copy();aa[5]=flags;aa[-2:]=[inner,outer]
                        put('damage_radial',aa,p,config,ss,strict=True)
        for dmg in (-32768,-1,0,1,32767,0x7fffffff,0x80000000):
            for minimum in (-1,0,200):
                aa=undead_probes[1][1].copy();aa[4]=dmg;aa[6]=minimum
                put('damage_radial',aa,undead_state(),config,ss,strict=True)
        # Signed virtual adjustments for both new cast and object identities.
        for adjustment in (-16,16,-1,0x7fff,0x8000):
            for body in (0x2f4d34,0x2f645c):
                p=undead_state()+[(0x2010,2,adjustment),(0x2014,4,body)]
                put('call',[0x1000,2,7,0x3000,ONE,TWO,0,12345],p,config,undead_services(),strict=True)
            for slot,body in ((4,0x2f57b0),(6,0x2f5fb4)):
                p=undead_state()+[(0x50e908+8*slot,2,adjustment),(0x50e90c+8*slot,4,body)]
                put('object_call',[0x8000,slot]+([0x6000] if slot==6 else []),p,config,undead_services(),strict=True)

    for op,args in undead_probes:
        for service,original,mode,*_ in undead_services():
            if service==104: continue
            put(op,args,undead_state(),scripts=[v for v in undead_services() if v[0]!=service],strict=True)
            put(op,args,undead_state(),scripts=replace_script(undead_services(),service,0,2,0x7004,4,77),strict=True)
            for addr,width,value in ((0x8048,4,0),(0x804c,4,0),(0x8064,4,0),(0x8054,4,0),(0x8038,2,0xffff),
                    (0x8010,4,ONE),(0x80b8,4,0x1000),(0x80bc,4,128),(0x8044,4,1),
                    (0x600c,4,0x28),(0x6228,4,0),(R.G_SIM_FRAME,4,0xffffffff)):
                put(op,args,undead_state(),scripts=replace_script(undead_services(),service,original,1,addr,width,value),strict=True)
    for op,args in undead_probes:
        put(op,args,undead_state(),scripts=replace_script(undead_services(),100,0),strict=True)
    # Damage callback can kill the victim; LostSoul hidden count increments after success.
    for soul in (0,0xb000):
        for mode in (0,2):
            ss=replace_script(undead_services(),127,0,1,0x6228,4,0)
            ss=replace_script(ss,100,soul)
            ss=replace_script(ss,176,0,mode,0x7004,4,77)
            put('holybolt_hit',[0x8000,0x6000],undead_state(),scripts=ss,strict=True)

    undead_count=len(rows)
    undead_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert undead_count==116216 and undead_hash=='ef937fd65f3d8200112f02a61b18e2009916effcff4bd8ef2f831246810948ed'

    # Complete Poison Rain lifecycle. Synthetic statistics, no retail data.
    def rain_state(seed=1):
        p=base(seed)+[(R.G_SIM_FRAME,4,60),(R.G_WORLD,4,0x7000),
            (0x200c,4,0x2f9dec),(0x2014,4,0x2f9a3c),(0x300c,4,0x20),
            (0x800c,4,0x421b),(0x8010,4,ONE),(0x8014,4,TWO),(0x8018,4,0x41c00000),
            (0x8044,4,seed),(0x8048,4,0x51aa68),(0x804c,4,240),(0x8050,4,40),
            (0x8054,4,0x41c00000),(0x8058,4,0xbad),(0x805c,4,7),(0x8060,4,0x1000),
            (0x8064,4,0xf000),(0xf00c,4,0x3000),(0x8068,4,71),
            (0x51aa8c,4,0x2f9254),(0x600c,4,0x20),(0x620c,4,0x60)]
        for i,val in enumerate((109,2,3,7,5)):
            p += [(0x100e+2*i,2,1),(0x1018+2*i,2,val),(0x1024+4*i,4,ZERO),
                  (0x1038+2*i,2,1),(0x1042+2*i,2,val),(0x104c+4*i,4,ZERO)]
        return p

    def rain_services():
        return [(100,0x8000,4,0,1,0xb000),(101,0,0,0,1,0),
            (104,0x6000,3,0,1,0x6200),(105,0,0,0,1,0),
            (115,0,1,0x8064,4,0xf000),(127,0,0,0,1,0),
            (130,73,0,0,1,0),(132,1,0,0,1,0),(136,ONE,0,0,1,0),
            (146,TWO,0,0,1,0),(147,0,0,0,1,0),(148,0,0,0,1,0),
            (150,0,0,0,1,0),(160,0,0,0,1,0)]

    rain_probes=[('poisonrain_can_use',[0x1000,0x3000]),
        ('poisonrain_cast',[0x1000,7,0x3000,ONE,TWO,0,12345]),
        ('poisonrain_ctor',[0x8000,7,0x3000,ONE,TWO,0,0x1000,71]),
        ('poisonrain_run',[0x8000])]
    for config in range(16):
        for seed in (0,1,0x80000000,0xffffffff,0x12345678):
            for op,args in rain_probes:
                put(op,args,rain_state(seed),config,rain_services(),strict=True)
        for flags in (0,0x20,0x40,0x60):
            for level in (0,1,20,255,0xffffffff,0x80000000,0x7fffffff):
                p=rain_state()+[(0x300c,4,flags)]
                for op,args in rain_probes[1:3]:
                    aa=args.copy();aa[1]=level
                    put(op,aa,p,config,rain_services(),strict=True)
        # Signed delay/end, owner lifetime, periodic boundary and draw counts.
        for frame in (-61,-60,-1,0,39,40,41,59,60,61,239,240,241,0x7fffffff):
            for start,end,handle in ((40,240,0xf000),(-62,240,0xf000),(0,60,0xf000),(40,240,0)):
                put('poisonrain_run',[0x8000],rain_state()+[(R.G_SIM_FRAME,4,frame),
                    (0x8050,4,start),(0x804c,4,end),(0x8064,4,handle)],config,rain_services(),strict=True)
        # Loop counts around small radii, raw signed-zero/negative behavior.
        for radius in (ZERO,0x80000000,ONE,TWO,0xbf800000,0x41400000,0x41bfffff,
                       0x41c00000,0x41c00001,0x42100000):
            for frame in (59,60):
                put('poisonrain_run',[0x8000],rain_state()+[(0x8054,4,radius),(R.G_SIM_FRAME,4,frame)],
                    config,rain_services(),strict=True)
        # Damage values stored as descriptor low16, with negative and overflow cases.
        for amount in (0,1,32767,32768,65535,65536,0xffffffff,0x80000000,0x7fffffff):
            scripts=rain_services()+[(13,amount,0,0,1,0)]
            put('poisonrain_run',[0x8000],rain_state(),config,scripts,external=(13,),strict=True)
        for enemy in (0,1,0xffffffff):
            for query in ((104,0,7,0,1,0),(104,0x6000,8,0,1,0),
                          (104,0x6000,3,0,1,0x6000),(104,0x6200,3,0,1,0x6000)):
                ss=replace_script(rain_services(),132,enemy)
                ss=[v for v in ss if v[0]!=104]+[query]
                put('poisonrain_run',[0x8000],rain_state(),config,ss,strict=True)
        for allocation in (0,0xb000):
            ss=replace_script(rain_services(),100,allocation)
            for op,args in rain_probes[1:]:
                p=rain_state()+([(0x804c,4,60)] if op=='poisonrain_run' else [])
                put(op,args,p,config,ss,strict=True)
        for adjust in (-1,1,-32768,32767):
            for slot in (1,2):
                p=rain_state()+[(0x2000+8*slot,2,adjust)]
                args=[0x1000,slot,0x3000] if slot==1 else [0x1000,2,7,0x3000,ONE,TWO,0,65535]
                put('call',args,p,config,rain_services(),strict=True)
            p=rain_state()+[(0x51aa88,2,adjust)]
            put('object_call',[0x8000,4],p,config,rain_services(),strict=True)
        # Constructor argument and callback-visible level/handle/radius rereads.
        for stat in (0,1,2):
            for value in (0,1,0xffffffff,0x7fffffff,0x80000000):
                ss=rain_services()+[(13,value,0,0,1,0)]
                put('poisonrain_ctor',rain_probes[2][1],rain_state(),config,ss,external=(13,),strict=True)

    # Missing and throwing services retain the complete prefix at every boundary.
    for op,args in rain_probes[1:]+[('poisonrain_run',[0x8000])]:
        p=rain_state()+([(0x804c,4,60)] if op=='poisonrain_run' and args is not rain_probes[3][1] else [])
        for service,original,mode,*_ in rain_services():
            put(op,args,p,scripts=[v for v in rain_services() if v[0]!=service],strict=True)
            put(op,args,p,scripts=replace_script(rain_services(),service,0,2,0x7004,4,77),strict=True)
            # Small or zero radii ensure callback mutations still terminate.
            for addr,width,value in ((0x8054,4,ZERO),(0x8054,4,ONE),(0x8064,4,0),
                    (0x805c,4,0xffffffff),(0x8060,4,0),(R.G_SIM_FRAME,4,61),
                    (0x8010,4,0x42c80000),(0x300c,4,0x40)):
                put(op,args,p,scripts=replace_script(rain_services(),service,original,
                    (8 if service==104 else 1),addr,width,value),strict=True)

    porting_hammer = True
    rain_count=len(rows)
    rain_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert rain_count==120072 and rain_hash=='5f6cb7a0ebc536feca7f1e089655456e51f6892153fa3ef414b30504425b09f7'

    # Authored item table and descriptor strings, never original item data.
    def text_at(addr,text):
        return [(addr+i,1,c) for i,c in enumerate(text.encode('ascii')+'\0'.encode())]
    def hammer_state(seed=17):
        p=base(seed)+[(0x804c,4,0x5000),(0x8050,4,0x5100),(0x510c,4,0x6000),
            (0x600c,4,0x20),(0x6010,4,ONE),(0x6014,4,TWO),(0x6018,4,0x42700000),
            (0x300c,4,0x40),(0x3010,4,ZERO),(0x3014,4,ZERO),(0x3018,4,ZERO),
            (0x8044,4,seed),(R.G_SEED3,4,seed),(0x8038,2,12345),
            (0x8058,4,R.RI.A_DEFS),(0x8060,8,0x1000000000),(0x80a4,4,8),
            (0x80a8,4,240),(0x80c4,4,0x1000),(0x80c8,4,3),(0x8018,4,0x42700000),
            (0x80d8,4,ONE),(0x80dc,4,TWO),(0x80e0,4,0x42700000),(R.G_SIM_FRAME,4,100),
            (R.G_ITEM_MODELS+8,4,0x7770),(R.G_HAMMER_COUNT,1,255),
            (R.RI.A_DEFS,4,0x7400),(R.RI.A_DEFS+0xc,2,3),(R.RI.A_DEFS+0x28,8,0x1000001000)]
        p+=text_at(0x7400,'tool')
        for n,(addr,code) in enumerate(R.RI.TOKEN_FORMATS):
            p+=text_at(addr,('f%d,%d,%d' if code==0x20 else ('m%d' if code is None else f'x{n}%d')))
        for addr in (*R.Model.HAMMER_TEXTS.values(),0x550950): p+=text_at(addr,'tool+f17,21,35')
        for i in range(12): p.append((0x5509c0+4*i,4,ONE if i in (0,4,8) else ZERO))
        for stat,value in enumerate((40,15,60,7,5)):
            p += [(0x100e+2*stat,2,1),(0x1018+2*stat,2,value),(0x1024+4*stat,4,HALF)]
        return p
    def hammer_services():
        return [(0,0x3cf768,6,0xabc017,1,0),(100,0,0,0,0,0),(101,0,0,0,0,0),(102,71,0,0,0,0),
            (130,19,0,0,0,0),(115,0,0,0,0,0),(148,0,0,0,0,0),
            (147,0,0,0,0,0),(151,0,0,0,0,0),(150,0,0,0,0,0),(113,0,0,0,0,0),
            (166,0,0,0,0,0),(104,0x6000,8,0x7000,4,11),(158,0,0,0,0,0),
            (132,1,0,0,0,0),(127,0,0,0,0,0),(124,7,0,0,0,0),
            (178,ONE,11,TWO,0,ZERO),(105,0,0,0,0,0)]
    probes=[('item_tables',[]),('item_parse',[0x5508d0,0x8058]),('item_resource_index',[0x8058]),
        ('hammer_can_use',[0x1000,0x3000]),('hammer_cast',[0x1000,7,0x3000,ONE,TWO,ZERO,32769]),
        ('hammer_ctor',[0x8000,ONE,TWO,ZERO,32769,0x3000,7,0x1000]),
        ('hammer_spell_ctor',[0x9000,7,0x3000,0x1000,0x8000]),('hammer_spell_run',[0x9000]),
        ('_hammer_pull',[0x8000,0,0,0,ONE]),('_hammer_blow',[0x8000,0x6000,3000]),
        ('hammer_run',[0x8000]),('seed3_draw',[])]
    for config in range(16):
        for op,args in probes: put(op,args,hammer_state(),config,hammer_services(),strict=True)
        for state in (0,1,2,3,4,5,0xffffffff):
            for count in (0,29,30,41,42,46,47,59,60,79,80,127,128,0xffffffff,0x7fffffff):
                put('hammer_run',[0x8000],hammer_state()+[(0x80c8,4,state),(0x80a0,4,count)],config,hammer_services(),strict=True)
        for state in (0,3,4):
            for off,size,values in ((0x804c,4,(0,0x5000)),(0x8050,4,(0,0x5100)),
                (0x600c,4,(0,8,0x20,0x40,0x60)),(R.G_CUTSCENE_ACTIVE,1,(0,1,128)),
                (0x358c,4,(0,8)),(R.G_PVP_MODE_A,4,(0,1)),(0x80a8,4,(0,2,60,120,121,0xffffffff))):
                for v in values:
                    put('hammer_run',[0x8000],hammer_state()+[(0x80c8,4,state),(off,size,v)],config,hammer_services(),strict=True)
        for level in (0,1,2,3,4,255,0xffffffff,0x7fffffff,0x80000000):
            for owner in (0,0x3000):
                put('hammer_ctor',[0x8000,ONE,TWO,ZERO,0xffffffff,owner,level,0x1000],hammer_state(),config,hammer_services(),strict=True)
                put('hammer_spell_ctor',[0x9000,level,owner,0x1000,0x8000],hammer_state(),config,hammer_services(),strict=True)
        for attached in (0,0x9000):
            for identity in (0xabc017,0xabc018):
                p=hammer_state()+[(0x3364,4,0x5200),(0x520c,4,attached),(0x9014,4,0x2200),(0x2204,4,0x3cf768)]
                ss=replace_script(hammer_services(),0,0x3cf768,6,identity,1,0)
                put('hammer_can_use',[0x1000,0x3000],p,config,ss,strict=True)
        for seed in (0,1,0xffffffff,0x80000000,0x12345678):
            put('seed3_draw',[],hammer_state(seed),config,strict=True)
            put('_hammer_blow',[0x8000,0x6000,300000],hammer_state(seed),config,hammer_services(),strict=True)
        for flags in (0,0x1000000000,0x100000f800,0xf800,0x1000000800):
            for row in (-32768,-1,0,7,32767):
                put('item_resource_index',[0x8058],hammer_state()+[(0x8060,8,flags),(R.RI.A_DEFS+0x34,2,row)],config,strict=True)
        for variants in (0,1,2,31):
            for text in ('tool','missing','tool+f-1,32768,-32769','tool+m3','tool+x0-33+f2,3,4','tool+unknown','tool+f1,2,3+f4,5,6'):
                p=hammer_state()+text_at(0x7500,text)+[(R.RI.A_DEFS+0x28,8,0x1000000000|(variants<<11))]
                put('item_parse',[0x7500,0x8058],p,config,strict=True)
        for op,args in probes[4:8]:
            for pool in (0,0x9000):
                ss=replace_script(hammer_services(),100,pool,0)
                put(op,args,hammer_state(),config,ss,strict=True)
        # Exact virtual identities, signed adjustments, and forced callbacks.
        for adjust in (-4,0,4):
            p=hammer_state()+[(0x1060,4,0x2000),(0x2008,2,adjust),(0x200c,4,0x2efef8)]
            put('call',[0x1000-adjust,1,0x3000],p+[(0x1060-adjust,4,0x2000)],config,hammer_services(),strict=True)
            p=hammer_state()+[(0x8048-adjust,4,0x2100),(0x2120,2,adjust),(0x2124,4,0x2ee068)]
            put('object_call',[0x8000-adjust,4],p,config,hammer_services(),strict=True)
            p=hammer_state()+[(0x9014-adjust,4,0x2200),(0x2210,2,adjust),(0x2214,4,0x2efa04)]
            put('spell_call',[0x9000-adjust,2],p,config,hammer_services(),strict=True)
    # Missing/failing services and mutation at every external boundary.
    for op,args in probes[4:]+[('hammer_run',[0x8000])]:
        for service,value,mode,*rest in hammer_services():
            put(op,args,hammer_state(),scripts=[s for s in hammer_services() if s[0]!=service],strict=True)
            put(op,args,hammer_state(),scripts=replace_script(hammer_services(),service,value,2,0x7004,4,33),strict=True)
            if service in (178,124): continue
            for addr,size,val in ((0x804c,4,0),(0x8050,4,0),(0x80a0,4,79),(0x80a8,4,120),
                (0x80c4,4,0),(0x80a4,4,0xffffffff),(0x80d8,4,0x42c80000),(R.G_CUTSCENE_ACTIVE,1,1)):
                put(op,args,hammer_state(),scripts=replace_script(hammer_services(),service,value,
                    8 if service==104 else 1,addr,size,val),strict=True)
    # Explicit targeting ties, query order, walls, selection chance and state-4 blow.
    for config in range(16):
        for state,count in ((0,0),(4,79),(3,46)):
            for enemy in (0,1):
                for wall in (ZERO,ONE,0xbf800000):
                    ss=replace_script(hammer_services(),132,enemy,0)
                    ss=replace_script(ss,158,wall,0)
                    ss=replace_script(ss,104,0x6000,3,0,0,0x6100)
                    put('hammer_run',[0x8000],hammer_state()+[(0x80c8,4,state),(0x80a0,4,count),
                        (0x610c,4,0x20),(0x6110,4,ONE),(0x6114,4,TWO),(0x6118,4,0x42700000)],config,ss,strict=True)
    # The final-frame 3000-particle path is intentionally one complete case.
    put('hammer_run',[0x8000],hammer_state()+[(0x80a8,4,1)],scripts=hammer_services(),strict=True)

    hammer_count=len(rows)
    hammer_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    def riposte_state(seed=17, cosmetic=23):
        return base(seed)+[(0x404460,4,cosmetic),(0x3010,4,ONE),(0x3014,4,TWO),
            (0x3018,4,0x40400000),(0x3038,2,0x8001),(0x6010,4,TWO),
            (0x6014,4,ONE),(0x6018,4,0x40800000)]
    def riposte_services(chance=101,count=3):
        # Alternating statistic return: damage, then chance. Point is authored.
        return [(12,37,4,0,0,chance),(107,0xa000,0,0,0,0),
            (124,count,0,0,0,0),(125,ONE,9,TWO,0,0x40400000),
            (105,0,0,0,0,0),(127,0,0,0,0,0)]
    numeric=[0,1,2,0xffff,0x10000,0x7fffffff,0x80000000,0xfffffffe,MASK]
    patterns=[0,ONE,TWO,0x3f000000,0xbf000000,0x3fc00000,0xbfc00000,
        0x80000000,0x00000001,0x007fffff,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00001]
    for config in range(16):
        for value in numeric:
            for op in ('lcg','unit','frac','half_toward_zero'):
                put(op,[value],[],config)
            for divisor in numeric:
                for op in ('rem','quot'): put(op,[value,divisor],[],config)
        for value in patterns:
            put('round_half',[value],[],config)
            for draw in (0,1,0xffff,0x80000000,MASK):
                put('spread_max',[value,draw],[],config)
                put('spread_max',[value,draw,0xbf000000],[],config)
        for seed in (0,17,MASK,0x80000000):
            put('cosmetic_draw',[],riposte_state(cosmetic=seed),config,strict=True)
            put('_cosmetic_ball',[],riposte_state(cosmetic=seed),config,strict=True)
            for chance in (0,1,25,50,99,100,101,MASK,0x80000000):
                put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state(seed),config,riposte_services(chance,0),external=(12,),strict=True)
        for seed in (0,MASK,0x80000000):
            put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state(seed,seed),config,riposte_services(),external=(12,),strict=True)
        for damage in (0,MASK,0x80000000,0x7fffffff):
            put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state(),config,
                replace_script(riposte_services(),12,damage,4,0,0,101),external=(12,),strict=True)
        for count in (0,1,3,MASK,0x80000000):
            put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state(),config,riposte_services(count=count),external=(12,),strict=True)
        for level in (0,1,0x80000000,MASK):
            put('riposte_counter_attack',[0x1000,0x3000,0x6000,level],riposte_state(),config,riposte_services(),external=(12,),strict=True)
        # Callback-visible reads preserve signed16 virtual adjustment and rereads.
        for adjust in (0,7,0xfff9):
            put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state()+
                [(0x3048,4,0x5000),(0x5080,2,adjust)],config,riposte_services(),external=(12,),strict=True)
    # Refusal, exception and mutation prefixes at every engine boundary.
    for service,value,mode,addr,size,mutation in riposte_services():
        put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state(),
            scripts=[s for s in riposte_services() if s[0]!=service],external=(12,),strict=True)
        for off,n,val in ((0x6010,4,0x40a00000),(0x3014,4,0xbf800000),(0x404460,4,77),(0x404464,4,99),(0x3038,2,7)):
            for action in (1,2):
                put('riposte_counter_attack',[0x1000,0x3000,0x6000,7],riposte_state(),
                    scripts=replace_script(riposte_services(),service,value,12 if service==125 and action==1 else action,off,n,val),external=(12,),strict=True)

    # Shared can-use/mana entry points and instruction-shape family. Preserve
    # the complete prior corpus, then append independently authored instructions.
    dispatch_count=len(rows)
    dispatch_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    getter, function = 0x123400, 0x9000
    shape=[(function+0xc,4,0x0c000000 | (getter>>2)),(function+0x18,4,0x0c000000 | (0x11a710>>2))]
    constant=[(function,4,0x03e00008),(function+4,4,0x24020001)]
    for config in range(16):
        for value in (0,1,0x7fffffff,0x80000000,0xffffffff):
            put('frame',[],[(R.G_SIM_FRAME,4,value)],config,strict=True)
            for obj in (0,0x3000):
                put('is_player',[obj],[(obj+0xc,4,value)],config,strict=True)
        for first in (0,0x0c000000,0x0fffffff,0x08000000,0xffffffff):
            for second in (0,0x0c000000 | (0x11a710>>2),0x0c000001):
                put('carries_getter',[function],[(function+0xc,4,first),(function+0x18,4,second)],config,strict=True)
        for adjust in (0,0x7fff,0x8000,0xffff):
            p=base()+[(0x2008,2,adjust),(0x200c,4,function)]
            for instructions in ([],shape,constant,shape+constant):
                for known in (False,True):
                    scripts=[(0,getter,6,0xabcd,0,0)] if known else []
                    for attached in (False,True):
                        state=p+instructions+([(0x3364,4,0x4000),(0x400c,4,0x5000),(0x5014,4,0x6000),
                                              (0x6004,4,0x7000)] if attached else [])
                        callbacks=scripts+[(114,0xabcd,0,0,0,0)]
                        for op,args in (('can_use_slot',[0x1000,0x3000]),('first_skill_call',[0x1000,1,0x3000])):
                            put(op,args,state,config,callbacks,strict=True)
                        # Forced external slot must skip shape recognition and
                        # receive signed16-adjusted self, including failure prefixes.
                        for mode in (0,1,2):
                            put('can_use_slot',[0x1000,0x3000],state,config,
                                callbacks+[(1,7,mode,0x3364,4,0)],external=(1,),strict=True)
            for op,args in (('mana_cost',[0x1000,7]),('first_skill_call',[0x1000,5,7])):
                put(op,args,base()+[(0x2028,2,adjust)],config,strict=True)
        # Known bodies ignore fabricated instructions; slot-1 overrides do not
        # examine a function just because it has a matching supplied shape.
        for body in (0x3c8218,0x2e2368):
            put('can_use_slot',[0x1000,0x3000],base()+[(0x200c,4,body),
                (body+0xc,4,0x0c000000 | (getter>>2)),(body+0x18,4,0x0c000000 | (0x11a710>>2))],config,
                [(0,getter,6,0xabcd,0,0),(1,9,0,0,0,0)],external=(1,),strict=True)
        # Mana shape delegates level adjustment/statistic slots with full-register
        # arguments; zero and negative levels remain low32 exactly.
        for level in (0,1,0xffffffff,0x80000000):
            for slot in (14,15):
                for n in (0,1,0xffff):
                    p=base()+[(0x202c,4,function),(function+12,4,0x24060000|n),
                        (function+20,4,0x24420000 | (8*slot))]
                    for mode in (0,1,2):
                        put('mana_cost',[0x1000,level],p,config,
                            [(slot,0x3fc00000,mode,0x7000,4,99)],external=(slot,),strict=True)
        # Attached walk preserves empty nodes, duplicate classes and rereads
        # after getter callbacks. It compares type identities, not pointers.
        p=base()+[(0x200c,4,function)]+shape+[(0x3364,4,0x4000),(0x4000,4,0x4100),
            (0x4100,4,0x4200),(0x410c,4,0x5000),(0x420c,4,0x5100),
            (0x5014,4,0x6000),(0x5114,4,0x6000),(0x6004,4,0x7000)]
        for value in (0,0xabcd):
            for mode in (0,1,2):
                put('can_use_slot',[0x1000,0x3000],p,config,
                    [(0,getter,6,0xabcd,0,0),(114,value,mode,0x4100,4,0)],strict=True)

    # Complete Summon Skeleton family: inherited lifecycle + local callback bridge.
    summon_previous_count=len(rows)
    summon_previous_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    def summon_state(seed=17):
        return base(seed)+[(0x3038,2,0xfffd),(0x3010,4,ONE),(0x3014,4,TWO),
            (0x404460,4,23),(0x3048,4,0x5000),(0x6048,4,0x7000),
            (0x6050,4,0x9000),(0x9000,4,0xa000),(0x60a8,4,0x1234),(0x60ac,4,0xabcdef01),
            (0x44fdc8,4,0xb000),(0x8000+0x14,4,0x519918),
            (0x503778,4,0xd000),(0x50377c,4,0xd100),(0x503780,4,0xd200),
            (0x503784,4,0xd300),(0x503788,4,0xd400)]
    def summon_services(count=0,chance=101):
        return [(107,0xc000,0,0,0,0),(124,count,0,0,0,0),(125,ONE,12,0x7004,4,99),
                (146,0,0,0,0,0),(180,ONE,13,0,0,1),(100,0x6000,4,0,0,0x8000),
                (181,0,0,0,0,0),(12,13,4,0,0,chance),(169,0,0,0,0,0),
                (109,0,0,0,0,0),(185,0,0,0,0,0),(183,7,0,0,0,0),
                (182,0,0,0,0,0),(184,0,0,0,0,0),(102,0xe000,0,0,0,0),
                (103,0,0,0,0,0),(115,0,0,0,0,0),(113,0,0,0,0,0),
                (179,0,0,0,0,0),(105,0,0,0,0,0)]
    probes=[('summon_can_use',[0x1000,0x3000]),
            ('summon_cast',[0x1000,7,0x3000,ONE,TWO,0,0xfffd]),
            ('skelight_ctor',[0x6000,ONE,TWO,0,0xffffffff,0x503770,13]),
            ('summon_spell_ctor',[0x8000,0x1ff,0x3000,0x1000,0x6000]),
            ('summon_spell_run',[0x8000]),('_summon_sample',[0x3000,3])]
    for config in range(16):
        for op,args in probes:
            put(op,args,summon_state(),config,summon_services(),external=(12,),strict=True)
        for flags in (0,0x40):
            for slot in (0,1,2,3,4,0xffffffff):
                for exists in (0,0x6000):
                    put('summon_can_use',probes[0][1],summon_state()+[(0x300c,4,flags),(0x33c8,4,slot)],config,
                        replace_script(summon_services(),179,exists),strict=True)
        for seed in (0,1,17,99,0xffffffff):
            for chance in (0,1,50,100,101,0xffffffff):
                put('summon_cast',probes[1][1],summon_state(seed),config,summon_services(chance=chance),external=(12,),strict=True)
        for flags,slot,level in ((0,0,7),(0x40,0,0),(0x40,1,1),(0x40,3,0xffffffff),(0x40,4,7),(0x40,0xffffffff,7)):
            args=probes[1][1].copy();args[1]=level
            put('summon_cast',args,summon_state()+[(0x300c,4,flags),(0x33c8,4,slot)],config,summon_services(),external=(12,),strict=True)
        for height in (0x41bfffff,0x41c00000,0x41c00001,0xc1c00001,0x7f800000,0x7fc00000):
            for found in (0,1,0x80,0x100):
                ss=replace_script(summon_services(),146,height)
                ss=replace_script(ss,180,ONE,13,0,0,found)
                put('summon_cast',probes[1][1],summon_state(),config,ss,external=(12,),strict=True)
        for allocation in (0,0x6000):
            for spell in (0,0x8000):
                ss=replace_script(summon_services(),100,allocation,4,0,0,spell)
                put('summon_cast',probes[1][1],summon_state(),config,ss,external=(12,),strict=True)
        for count in (1,3,0xffffffff,0x80000000):
            put('_summon_sample',[0x3000,count],summon_state(),config,summon_services(),strict=True)
        for count in (1,0xffffffff):
            put('summon_cast',probes[1][1],summon_state(),config,summon_services(count=count),external=(12,),strict=True)
        for skeleton in (0,0x6000):
            put('summon_spell_ctor',[0x8000,0x80,0x3000,0x1000,skeleton],summon_state(),config,summon_services(),strict=True)
            put('summon_spell_run',[0x8000],summon_state()+[(0x8018,4,skeleton)],config,summon_services(),strict=True)
        for adjust in (0,0x7fff,0x8000,0xffff):
            # Both Skill declarations resolve the imported bodies. Constructor
            # writes keep original this adjustments and full register levels.
            for op in ('call','first_skill_call'):
                put(op,[0x1000,1,0x3000],summon_state()+[(0x2008,2,adjust),(0x200c,4,0x2e2368)],config,summon_services(),strict=True)
                put(op,[0x1000,2]+probes[1][1][1:],summon_state()+[(0x2010,2,adjust),(0x2014,4,0x2e18a4)],config,summon_services(),external=(12,),strict=True)
            put('spell_call',[0x8000,2],summon_state()+[(0x519928,2,adjust),(0x51992c,4,0x2e177c)],config,summon_services(),strict=True)
    for op,args in probes:
        for service,value,mode,addr,size,mutation in summon_services(count=1):
            put(op,args,summon_state(),scripts=[s for s in summon_services(count=1) if s[0]!=service],external=(12,),strict=True)
            for action in (1,2):
                for target,width,new in ((0x300c,4,0),(0x33c8,4,3),(0x6050,4,0),(0x8018,4,0x6000),(0x404464,4,77)):
                    ss=replace_script(summon_services(count=1),service,value,12 if service==125 and action==1 else action,target,width,new)
                    put(op,args,summon_state(),scripts=ss,external=(12,),strict=True)

    adapters_previous_count=len(rows)
    adapters_previous_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert adapters_previous_count==143969 and adapters_previous_hash=='ab44e6f3340ec0c21bfa4521730c7a183fb76c3131a3342c0129dcee5a3379cc'
    # Directly exercise every image/RNG adapter and canonical descriptor slot.
    # No game data: byte strings deliberately contain NUL and high byte values.
    for config in range(16):
        for address in (0,0x1000,0xfffffff0):
            patches=[(address+i,1,v) for i,v in enumerate((0x41,0xff,0x80,0,0x42,0,0xfe))]
            for size in (-7,-1,0,1,3,4,7,12):
                put('image_read',[address,size],patches,config,strict=True)
            for offset in (0,1,3,4,5,6,7):
                put('image_cstr',[address+offset],patches,config,strict=True)
        for seed in (0,1,0xffffffff,0x80000000,0x19660d):
            for count in (0,1,2,17,64):
                put('shared_draws',[count],[(R.G_RAND_SEED,4,seed)],config,strict=True)
        for count in range(5):
            for width in (0,1,4,6):
                entries=[[((i+1)*0x12345+j*0x80000001)&MASK for j in range(width)] for i in range(count)]
                args=[v for entry in entries for v in [len(entry),*entry]]
                put('descriptor',args,[],config,strict=True)

    plumbing_previous_count=len(rows)
    plumbing_previous_hash=hashlib.sha256(('# rta-skills-a-shared-v1\n'+'\n'.join(rows)+'\n').encode('ascii')).hexdigest()
    assert plumbing_previous_count==145409 and plumbing_previous_hash=='4b77d8478c3a800f44c93ff9db509c3a543cdca383e8736b3ac2dfc7f9c1c4d5'
    # Generic dispatch: unknown functions delegate, arbitrary table offsets,
    # signed this wrapping, empty/scalar/list callback results and failures.
    for config in range(16):
        put('model_init',[],[(0x1000,4,0xfeed1234)],config,strict=True)
        for adjust in (0,1,0x7fff,0x8000,0xffff):
            for mode in (0,1,2,7,9):
                services=[(107,0xffffffff,mode,0x9000,4,0x12345678)]
                put('named_call',[0xffffffff,0x80000000,17],[],config,services,strict=True)
                for offset in (0x14,0x48,0x70):
                    for cls,service in ((0,114),(1,107)):
                        slot=0 if cls==0 else 16
                        state=[(0x8000+offset,4,0x2000),(0x2000+8*slot,2,adjust),(0x2004+8*slot,4,0xabcdef)]
                        put('generic_table',[cls,0x8000,offset,slot,0xffffffff,17],state,config,
                            [(service,0xffffffff,mode,0x9000,4,0x12345678)],strict=True)
                state=[(0x8048,4,0x2000),(0x2080,2,adjust),(0x2084,4,0xabcdef)]
                put('object_call',[0x8000,16,17],state,config,services,strict=True)
                for op in ('call','first_skill_call'):
                    state=[(0x1060,4,0x2000),(0x2018,2,adjust),(0x201c,4,0xabcdef)]
                    put(op,[0x1000,3,17,0xffffffff],state,config,[(3,0xffffffff,mode,0x9000,4,0x12345678)],strict=True)
        # Known Spell bodies resolve even at caller-supplied nonstandard offsets;
        # a general Creature table call must always delegate, even a known body.
        for offset in (0x14,0x48,0x70):
            state=summon_state()+[(0x8000+offset,4,0x2000),(0x2010,2,0),(0x2014,4,0x2e177c),(0x8018,4,0)]
            put('generic_table',[0,0x8000,offset,2],state,config,summon_services(),strict=True)
            state=[(0x8000+offset,4,0x2000),(0x2080,2,0xffff),(0x2084,4,0x2f9254)]
            put('generic_table',[1,0x8000,offset,16],state,config,[(107,17,0,0,0,0)],strict=True)
        for op,args,state in [('named_call',[17],[]),('generic_table',[0,0x8000,0x70,0],[(0x8070,4,0x2000)]),
                              ('object_call',[0x8000,16],[(0x8048,4,0x2000)])]:
            put(op,args,state,config,strict=True)

    plain = ('# rta-skills-a-shared-v1\n' + '\n'.join(rows) + '\n').encode('ascii')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open('wb') as stream:
        with gzip.GzipFile(filename='', fileobj=stream, mode='wb', mtime=0) as zipped:
            zipped.write(plain)
    manifest = {'schema': 'rta-skills-a-shared-v1', 'reference_commit': COMMIT,
                'reference_sha256': PINS['tools/ref_skills_a.py'], 'evidence_sha256': PINS,
                'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'vector_sha256': hashlib.sha256(plain).hexdigest(), 'vector_count': len(rows), 'operations': counts,
                'legacy_vector_count': legacy_count, 'legacy_vector_sha256': hashlib.sha256(legacy_plain).hexdigest(),
                'previous_vector_count': previous_count, 'previous_vector_sha256': previous_hash,
                'support_vector_count': support_count, 'support_vector_sha256': support_hash,
                'bash_vector_count': bash_count, 'bash_vector_sha256': bash_hash,
                'trail_vector_count': trail_count, 'trail_vector_sha256': trail_hash,
                'storm_vector_count': storm_count, 'storm_vector_sha256': storm_hash,
                'shield_vector_count': shield_count, 'shield_vector_sha256': shield_hash,
                'frost_vector_count': frost_count, 'frost_vector_sha256': frost_hash,
                'homing_vector_count': homing_count, 'homing_vector_sha256': homing_hash,
                'flares_vector_count': flares_count, 'flares_vector_sha256': flares_hash,
                'disease_vector_count': disease_count, 'disease_vector_sha256': disease_hash,
                'blind_vector_count': blind_count, 'blind_vector_sha256': blind_hash,
                'dispatch_previous_vector_count': dispatch_count, 'dispatch_previous_vector_sha256': dispatch_hash,
                'summon_previous_vector_count': summon_previous_count, 'summon_previous_vector_sha256': summon_previous_hash,
                'adapters_previous_vector_count': adapters_previous_count, 'adapters_previous_vector_sha256': adapters_previous_hash,
                'plumbing_previous_vector_count': plumbing_previous_count, 'plumbing_previous_vector_sha256': plumbing_previous_hash,
                'reference_function_count': len(units), 'ported_functions': PORTED_UNITS,
                'hammer_vector_count': hammer_count, 'hammer_vector_sha256': hammer_hash,
                'rain_vector_count': rain_count, 'rain_vector_sha256': rain_hash,
                'undead_vector_count': undead_count, 'undead_vector_sha256': undead_hash,
                'blessed_vector_count': blessed_count, 'blessed_vector_sha256': blessed_hash,
                'beam_vector_count': beam_count, 'beam_vector_sha256': beam_hash,
                'fire_vector_count': fire_count, 'fire_vector_sha256': fire_hash,
                'families': ['shared-skill', 'weapon-buffs-12-14-26', 'undead-shields-15', 'root-critical-support-31-32-33', 'shield-bash-34', 'fire-storm-7', 'disease-trail-6', 'shock-of-frost-23', 'fire-flares-8', 'homing-projectiles-4-16', 'blinding-light-20', 'disease-shield-status-5-29', 'fire-weapons-cone-13-18', 'wizard-beam-1', 'blessed-10', 'offensive-undead-21-22', 'poison-rain-24', 'hammer-of-wrath-17', 'riposte-9-82', 'shared-can-use-mana-dispatch', 'summon-skeleton-11', 'item-image-rng-descriptor-adapters', 'model-generic-dispatch']}
    a.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'{len(rows)} Skill and weapon-buff state vectors')


if __name__ == '__main__':
    main()
