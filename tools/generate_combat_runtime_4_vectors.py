#!/usr/bin/env python3
"""Exact round-4 fixtures from research commit 95c17ae, never from native output.

The compared words are the ones both sides compute from the same seed. Potion
cases that read the position after the move plant a z velocity equal to the
gravity term, so the real move and the research collide stub leave the
projectile where it was planted. Handle addresses are not compared.
"""
import gzip
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '95c17ae49f04b99626a3db856fd3b66764442caa'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat'
PINS = {
    'docs/spec/combat-round4.md': '58ef1a4f0561551624f68e9042c068de611376a6c28f3eab7426a1553c0fffda',
    'docs/combat-runtime.md': '31030e6cd3d722db3bcf76443e8548ab68efdc74b30b63342bb3f1b07c92b73c',
    'tools/ref_combat_round4.py': 'e4b83af89ac4d06d12be58b74259dc56664c8d1cf59cdb83098c15ace3c1e3dd',
    'tools/test_ref_combat_round4.py': '516f697c19c1a52775a8373567a2f575aba9aac829e90c5701e320a858c1e46e',
    'tools/difftest_combat_round4.py': '16d3735e3ea0cbf69fa9141adf3292cf7652e97cfbec57fc1c93b0383cf4713a',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_combat_ranged.py': '7755ff203383b92d39a190475b5704fb3fc512abcc107924e09189913d42bda5',
    'tools/ref_combat_ptick.py': 'b90cc565e1651c659248945d5e98695449e1f6de20138928b18ade69568fb79c',
    'tools/ref_combat_runtime.py': '328fbc428e26d50ac5cdb455d1614cf35148216151ba7904bb2ef92984e25ddd',
}


def pin_ok():
    for path, want in PINS.items():
        blob = subprocess.check_output(['git', '-C', str(RESEARCH), 'show', f'{COMMIT}:{path}'])
        got = hashlib.sha256(blob).hexdigest()
        if got != want:
            raise SystemExit(f'{path} at {COMMIT} hashed {got}, pin is {want}')
        if path.startswith('tools/'):
            disk = (RESEARCH / path).read_bytes()
            if hashlib.sha256(disk).hexdigest() != want:
                raise SystemExit(f'{path} on disk is not {COMMIT}')


def word(value):
    return f'{value & 0xffffffff:08x}'


def words(values):
    return ' '.join(word(v) for v in values)


class CB:
    def __init__(self, collide=0, pool=0x4000, ground=0, points=4, stat=100,
                 sample=(0x3f800000, 0x40000000, 0x40400000)):
        self.collide, self.pool, self.ground = collide, pool, ground
        self.points, self.stat, self.sample, self.calls = points, stat, sample, []

    def get(self, name):
        def answer(*args):
            self.calls.append((name, args))
            if name == 'obj_moveAndCollide':
                return self.collide
            if name == 'pool_new':
                return self.pool
            if name == 'terrain_height':
                return self.ground
            if name == 'res_find':
                return 0xA10000
            if name == 'Creature.v16':
                return 0x8000
            if name == 'model_getSamplePointCount':
                return self.points
            if name == 'model_getSamplePoint':
                return (self.sample, 0)
            if name == 'model_getAttachPoint':
                return (0, 0, 0)
            if name == 'Skill.v10':
                return self.stat
            return 0
        return answer


def plant_potion(mem, kind, frame=100, expiry=None, spin=0, splash=0, vz=None):
    import ref_core as C
    import ref_creature as RC
    import ref_combat_ranged as RR
    import ref_combat_round4 as R4
    mem.store(RC.G_SIM_FRAME, 4, frame)
    mem.store(RC.G_RAND_SEED, 4, 1)
    mem.store(RR.G_WORLD, 4, 0x1000)
    obj = 0x2000
    mem.store(obj + 0x48, 4, RR.VT_THROWN)
    mem.store(obj + 0x10, 4, C.f32(10.0))
    mem.store(obj + 0x14, 4, C.f32(20.0))
    mem.store(obj + 0x18, 4, C.f32(30.0))
    mem.store(obj + 0x1B0, 4, 0)
    mem.store(obj + 0x1B4, 4, 0)
    mem.store(obj + 0x1B8, 4, C.f32(40.0) if vz is None else vz)
    mem.store(obj + 0x1C, 4, C.f32(12.0))
    mem.store(obj + 0xEC, 4, frame + 80 if expiry is None else expiry)
    mem.poke(obj + 0x1CC, 2, spin & 0xFFFF)
    mem.poke(obj + 0x1CE, 1, kind)
    mem.poke(obj + 0xCC, 1, splash)
    mem.store(obj + 0x1C8, 4, 0x3100)
    mem.store(0x3100 + 0xC, 4, 0x3000)
    for i, byte in enumerate(b'town\0'):
        mem.poke(R4.G_LEVEL_TEXT + i, 1, byte)
    return obj


def particles(calls):
    rows = []
    for name, args in calls:
        if name != 'particle_spawn':
            continue
        definition, point, direction, speed = args
        rows.append((definition, *point, *direction, speed))
    return rows


def potion_case(name, kind, **kw):
    import ref_creature as RC
    import ref_combat_round4 as R4
    import ref_core as C
    C.set_fpu('ps2')
    mem = RC.Memory()
    held = {k: kw.pop(k) for k in list(kw) if k in ('frame', 'expiry', 'spin', 'splash', 'vz')}
    obj = plant_potion(mem, kind, **held)
    if kw.pop('innpit', False):
        for i, byte in enumerate(b'InNpIt\0'):
            mem.poke(R4.G_LEVEL_TEXT + i, 1, byte)
        for i, byte in enumerate(b'innpit\0'):
            mem.poke(R4.S_INNPIT + i, 1, byte)
    cb = CB(**kw)
    R4.PotionModel(mem, cb).thrown_run(obj)
    lines = [f'CASE {name}', f'VZ {mem.load(obj + 0x1B8, 4):08x}',
             f'SPIN {mem.load(obj + 0x1CC, 2):08x}', f'EXPIRY {mem.load(obj + 0xEC, 4):08x}']
    for row in particles(cb.calls):
        lines.append('PARTICLE ' + words(row))
    for call, args in cb.calls:
        if call == 'LooseFire_ctor':
            lines.append('FIRE ' + words(args[1]))
        elif call == 'decal_add':
            lines.append('DECAL ' + words((*args[0], args[1] & 0xffffffff, args[2], args[3], args[4])))
        elif call == 'poisonGasSpell_ctor':
            _block, place, life, radius, zero, _owner, damage = args
            lines.append('GAS ' + words((*place, life, radius, zero, damage)))
        elif call == 'fx_ringBurst':
            lines.append('RING ' + words((*args[0], args[1])))
        elif call == 'world_damageInRadius':
            lines.append('DAMAGE ' + words((args[2], args[3], args[6], args[7])))
        elif call == 'LightEffect_ctor':
            lines.append('LPLACE ' + words(args[1]))
            lines.append('LIGHT ' + words(args[4:]))
    return lines


def shield_case(name, cooldown, stat):
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round4 as R4
    C.set_fpu('ps2')
    spell, caster, skill = 0x5000, 0x6000, 0x7000
    mem = RC.Memory()
    mem.store(spell + 0x8, 4, skill)
    mem.store(spell + 0xC, 4, caster)
    mem.store(spell + 0x10, 4, 1)
    mem.store(spell + 0x1C, 4, cooldown)
    mem.store(spell + 0x24, 4, 0xD0)
    mem.poke(spell, 1, 1)
    mem.store(caster + 0x10, 4, C.f32(10.0))
    mem.store(caster + 0x14, 4, C.f32(20.0))
    mem.store(caster + 0x18, 4, C.f32(30.0))
    # Slot 10's this-adjustment is zero; the call returns the stub statistic.
    mem.store(skill + 0x60, 4, 0x7100)
    mem.store(0x7100 + 8 * 10, 2, 0)
    cb = CB(stat=stat, points=4)
    got = R4.ShieldModel(mem, cb).holy_shield_absorb(spell)
    lines = [f'CASE {name}', f'COOLDOWN {mem.load(spell + 0x1C, 4):08x}', f'RESULT {got & 0xffffffff:08x}']
    for row in particles(cb.calls):
        lines.append('PARTICLE ' + words(row))
    for call, args in cb.calls:
        if call == 'LightEffect_ctor':
            lines.append('LPLACE ' + words(args[1]))
            lines.append('LIGHT ' + words(args[4:]))
    return lines


def spark_case(name, silent, points):
    import ref_core as C
    import ref_creature as RC
    import ref_combat_round4 as R4
    C.set_fpu('ps2')
    player = 0x9000
    mem = RC.Memory()
    mem.store(player + 0x544, 4, 0x8000)
    mem.store(player + 0x10, 4, C.f32(1.0))
    mem.store(player + 0x14, 4, C.f32(2.0))
    mem.store(player + 0x18, 4, C.f32(3.0))
    mem.store(RC.G_RAND_SEED, 4, 1)
    # The record object is the player, so the sample reads player+0x114.
    mem.store(RC.G_PLAYERS + 0x43D8, 4, player)
    cb = CB(points=points, sample=(0x40800000, 0x40a00000, 0x40c00000))
    R4.SparkModel(mem, cb).block_sparks(player, silent)
    lines = [f'CASE {name}', f'SEED {mem.load(RC.G_RAND_SEED, 4):08x}']
    for row in particles(cb.calls):
        lines.append('PARTICLE ' + words(row))
    return lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    import ref_combat_ranged as RR
    lines = ['# combat-runtime-4']
    lines += potion_case('potion-flight', 1, collide=0)
    # Expiry 0 reaches the ending without a collision, so the real mover and the
    # research collide stub leave the same position when z velocity cancels gravity.
    lines += potion_case('potion-fire', 1, collide=0, expiry=0, ground=0x40800000,
                         vz=RR.F_GRAVITY)
    lines += potion_case('potion-gas', 2, collide=0, expiry=0, vz=RR.F_GRAVITY)
    lines += potion_case('potion-blast', 3, collide=0, expiry=0, ground=0x41f00000,
                         vz=RR.F_GRAVITY, innpit=True)
    lines += spark_case('spark-silent2', 2, 1)
    lines += shield_case('shield-due', 0, 100)
    lines += shield_case('shield-held', 0x3f800000, 0)
    lines += spark_case('spark-silent1', 1, 2)
    lines += spark_case('spark-silent3', 3, 1)
    text = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text)
    (out / 'combat_runtime_4.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-runtime-4-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'cases': text.count(b'\nCASE '),
    }
    (out / 'combat_runtime_4.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'{meta["cases"]} cases, {len(text)} bytes')


if __name__ == '__main__':
    main()
