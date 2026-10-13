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
         125: 'model_getSamplePoint', 126: 'sound_release', 127: 'damage_apply'}


def flatten(values):
    if isinstance(values, tuple) and values == ('desc', None, None, None, None):
        return [MASK] * 4
    return [lane for value in values for lane in (flatten(value) if isinstance(value, (tuple, list)) else [value])]


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
    units += [node.name for cls in tree.body if isinstance(cls, ast.ClassDef)
              for node in cls.body if isinstance(node, ast.FunctionDef)]
    assert len(units) == 196 and len(PORTED_UNITS) == 49
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

    def put(op, args, patches, config=0, scripts=(), external=(), strict=False):
        R.C.set_fpu(('ps2', 'ps2g1', 'chop', 'nearest')[config // 4], bool(config & 2), bool(config & 1))
        mem = R.RC.Memory()
        for addr, size, value in patches:
            mem.poke(addr, size, value)
        model = R.Model(mem, external=['Skill.v%d' % slot for slot in external])
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
                if mode in (1, 2, 8):
                    mem.poke(addr, size, mutation)
                if mode == 2:
                    raise RuntimeError('authored callback failure')
                if mode == 8:
                    return [value]
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
            if op in ('lightningweapons_run', 'coldweapons_run', 'poisonweapons_run'):
                result = getattr(model, op)(args[0], tuple(args[1:4]), args[4])
            elif op == 'undeadshield_run':
                result = model.undeadshield_run(args[0], tuple(args[1:4]), args[4])
            elif op == 'weaponbuff_run':
                result = model.weaponbuff_run(args[0], args[1], tuple(args[2:5]), args[5])
            elif op == 'spell_call':
                result = model.table_call('Spell', args[0], 0x14, args[1], tuple(args[2:5]), *args[5:])
            elif op == 'call':
                result = model.skill_call(args[0], args[1], *args[2:])
            else:
                result = getattr(model, op)(*args)
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
                'reference_function_count': len(units), 'ported_functions': PORTED_UNITS,
                'families': ['shared-skill', 'weapon-buffs-12-14-26', 'undead-shields-15']}
    a.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'{len(rows)} Skill and weapon-buff state vectors')


if __name__ == '__main__':
    main()
