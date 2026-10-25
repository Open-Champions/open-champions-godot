#!/usr/bin/env python3
"""Exact round-6 fixtures from research commit faa7033, never from native output.

Each case runs one method of tools/ref_combat_round6.py on an authored memory.
The answers of the calls the method makes (the box, the enemy test, the skill
statistics, the near-view test, the ground, the sample points, the resource
entry, the particle group) are chosen so that the native world's own bodies
give the same answers on the matching fixture; the native replay checks those
answers where it can (the enemy relation, the box order) before it compares.

Compared: particle rows, damage_apply arguments (victim by tag), box
arguments, DiseaseCloud_ctor arguments, stored words, sound, effect, trail,
glow and sample-point arguments, and the refusal at an unmodelled slot 1.
Object addresses and handle words are not compared.
"""
import gzip
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = 'faa70331492e91084f0910071089ae3669e3e8fd'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat'
PINS = {
    'docs/spec/combat-round6.md': '66e1018224590eae1b9306121aaebbb3f3b08be7b3af2d5e372fd44eaa57d21e',
    'docs/combat-runtime.md': '47d2ed6da5d244858ddb9dd9d31380fd92bad2e7ef35d469a2014646c44e0713',
    'tools/ref_combat_round6.py': 'd03838ea1f31d3799c9c5a17c6037aa3400ac2a4d83291e4b944e041f787969b',
    'tools/test_ref_combat_round6.py': 'aa2f6371d8653020d70443b19cc2f78bea47a299a1202019cadb138a0b9145d6',
    'tools/difftest_combat_round6.py': '14c076bf5eefb1fcf351ab74cada7544e3297a9fb7e1b1ac2068402750c93eb2',
    'tools/ref_skills_a.py': '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
}

# Reference-side addresses. Only the memory of the reference model; the native
# replay builds its own objects and maps them by tag.
OBJ, NODE, OWNER, SKILL, SVT, VT, OVT = 0x2000, 0x4000, 0x3000, 0x5000, 0x7200, 0x7000, 0x7100
VICTIMS = (0x9000, 0x9400, 0x9800, 0x9C00)
CREATURE = 0xA000
FN_DTOR, FN_ENEMY, FN_STAT10, FN_STAT13 = 0x3E0100, 0x3E0120, 0x3E0140, 0x3E0160
TAGS = {OBJ: 'obj', OWNER: 'owner'}
TAGS.update({addr: 'v%d' % (i + 1) for i, addr in enumerate(VICTIMS)})
TAG_NUM = {'obj': 0, 'owner': 1, 'v1': 2, 'v2': 3, 'v3': 4, 'v4': 5}
# Object flags the native fixture leaves on what it spawns (spawn_player and
# spawn_class): a player 0x60, a Skelight 0x20, a fixture object 0.
PLAYER_FLAGS, SKELIGHT_FLAGS = 0x60, 0x20
KIND = {'player': 1, 'skelight': 2}

C = RC = SA = R6 = None


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


def fi(n):
    return C.from_int(n)


class World:
    """The answers of the calls a method makes."""

    def __init__(self, found=(), enemies=(), stat0=0, stat2=1, near=None, ground=0,
                 samples=0, sample=(0, 0, 0), entry=0, group=0, model=0x3600):
        self.found, self.enemies = tuple(found), set(enemies)
        self.stat0, self.stat2, self.near, self.ground = stat0, stat2, near, ground
        self.samples, self.sample, self.entry, self.group = samples, sample, entry, group
        self.model = model

    def get(self, name):
        def answer(*args):
            if name == 'world_collectObjectsInBox':
                return self.found
            if name == 'pool_new':
                return 0x8000
            if name == 'Skill.v13':
                return self.stat0
            if name == 'Skill.v10':
                return self.stat2
            if name == 'Creature.v20':
                return 1 if args[1] in self.enemies else 0
            if name == 'Creature.v16':
                return self.model
            if name == 'isNearView':
                return self.near
            if name == 'libc_sprintf':
                return ('buf', b'')
            if name == 'res_find':
                return self.entry
            if name in ('terrain_height', 'terrain_heightAtPoint'):
                return self.ground
            if name == 'model_getSamplePointCount':
                return self.samples
            if name == 'model_getSamplePoint':
                return (self.sample, (0, 0, 0)) if args[5] else self.sample
            if name == 'particleGroup_create':
                return self.group
            return 0
        return answer


def memory():
    mem = RC.Memory()
    for table in (VT, OVT):
        mem.poke(table + 8, 2, 0)
        mem.poke(table + 12, 4, FN_DTOR)
        mem.poke(table + 0xA0, 2, 0)
        mem.poke(table + 0xA4, 4, FN_ENEMY)
    mem.poke(SVT + 0x50, 2, 0)
    mem.poke(SVT + 0x54, 4, FN_STAT10)
    mem.poke(SVT + 0x68, 2, 0)
    mem.poke(SVT + 0x6C, 4, FN_STAT13)
    mem.poke(SKILL + 0x60, 4, SVT)
    for addr in (OBJ, OWNER, CREATURE) + VICTIMS:
        mem.poke(addr + 0x48, 4, VT)
    return mem


def put_vec(mem, addr, vec):
    for j, value in enumerate(vec):
        mem.poke(addr + 4 * j, 4, value & 0xFFFFFFFF)


class Case:
    def __init__(self, name, entry):
        self.name, self.entry = name, entry
        self.lines = [f'CASE {name}', f'ENTRY {entry}']
        self.env = {'frame': 0, 'cull': 1, 'focus': (0, 0), 'level': 0, 'ground': 0,
                    'samples': 0, 'sample': (0, 0, 0), 'entry': 0, 'group': 0,
                    'counter': 0, 'seed': 1, 'pos': (0, 0, 0)}

    def obj(self, mem, base, off, size, value, tag='OBJ'):
        mem.poke(base + off, size, value & ((1 << (8 * size)) - 1))
        self.lines.append(f'{tag} ' + words([off, size, value]))

    def finish_env(self):
        e = self.env
        self.lines.insert(2, 'ENV ' + words([e['frame'], e['cull'], *e['focus'], e['level'],
                                              e['ground'], e['samples'], *e['sample'],
                                              e['entry'], e['group'], e['counter'], e['seed'],
                                              *e['pos']]))


def emit_calls(case, model, keep=()):
    for name, args in model.calls:
        if name == 'particle_spawn':
            definition, point, velocity, back = args
            case.lines.append('P ' + words((definition, *point, *velocity, back)))
        elif name == 'damage_apply':
            victim, attacker, _a, _z0, _z1, flags, desc, _z2, low, high = args
            row = [0, 0, 0, 0, 0]
            if desc:
                slot = desc[1]
                row = [1, slot[0], slot[1], slot[2], slot[3]]
            case.lines.append('DMG ' + ' '.join([TAGS[victim], TAGS[attacker]]) + ' ' +
                              words([flags, *row, low, high]))
        elif name == 'world_collectObjectsInBox':
            low, high, limit = args
            case.lines.append('BOX ' + words([*low, *high, limit]))
        elif name == 'DiseaseCloud_ctor':
            _block, pos, level, owner = args
            case.lines.append('CLOUD ' + words([*pos, level, 1 if owner else 0]))
        elif name == 'sound_playAtPos':
            entry, pos, volume = args
            case.lines.append('SOUND ' + words([entry, *pos, volume]))
        elif name == 'sound_playOnObject':
            entry, _obj, volume = args
            case.lines.append('SOUNDOBJ ' + words([entry, volume]))
        elif name == 'fx_spawn':
            ident, pos = args
            case.lines.append('FX ' + words([ident, *pos]))
        elif name == 'libc_sprintf':
            fmt, number = args
            case.lines.append('SPRINTF ' + words([fmt, number]))
        elif name == 'trail_release':
            case.lines.append('TRAIL ' + words([args[0]]))
        elif name == 'glow_add':
            point, r, g, b, a, tex, z0, two, zero, size = args
            case.lines.append('GLOW ' + words([*point, r, g, b, a, tex, z0, two, zero, size]))
        elif name == 'model_getSamplePoint':
            _model, _anim, pos, facing, index = args[:5]
            case.lines.append('SAMPLE ' + words([index, facing, *pos]))
        elif name == 'isNearView':
            case.lines.append('NEAR ' + words([args[1]]))
        elif name in ('Creature.v1', 'Spell.v1'):
            case.lines.append('REFUSE')
        elif name in keep:
            case.lines.append('CALL ' + name)


def model_for(mem, world):
    return R6.Model(mem, world)


# ---- poison gas -----------------------------------------------------------------
def gas_case(name, frame, end, target, radius, value, seed, owner=True, victims=(), pos=(0, 0, 0)):
    """victims: (tag, kind, offset, enemy). The native box also returns the gas
    object and the owner player; neither is an enemy of the owner."""
    case = Case(name, 'gas')
    mem = memory()
    case.env.update(frame=frame, pos=tuple(fi(p) for p in pos))
    mem.poke(SA.G_SIM_FRAME, 4, frame)
    put_vec(mem, OBJ + 0x10, case.env['pos'])
    case.obj(mem, OBJ, 0x44, 4, seed)
    case.obj(mem, OBJ, 0x5C, 4, end)
    case.obj(mem, OBJ, 0x60, 4, target)
    case.obj(mem, OBJ, 0x64, 4, radius)
    case.obj(mem, OBJ, 0x6C, 4, value)
    found, enemies = [OBJ], []
    if owner:
        mem.poke(OBJ + 0x4C, 4, NODE)
        mem.poke(NODE + 0xC, 4, OWNER)
        mem.poke(OWNER + 0xC, 4, PLAYER_FLAGS)
        case.lines.append('OWNER ' + words([KIND['player'], 0, 0, 0, 0]))
        found.append(OWNER)
    for tag, kind, offset, enemy in victims:
        addr = VICTIMS[TAG_NUM[tag] - 2]
        case.lines.append('VICTIM ' + words([TAG_NUM[tag], KIND[kind], *offset, 0, 0, 0]))
        found.append(addr)
        if enemy:
            enemies.append(addr)
        if owner:
            case.lines.append(f'ENEMY {tag} {word(enemy)}')
    model = model_for(mem, World(found=found, enemies=enemies))
    model.poison_gas_run(OBJ)
    case.finish_env()
    emit_calls(case, model)
    case.lines.append('OW ' + words([0x64, 4, mem.load(OBJ + 0x64, 4)]))
    case.lines.append('OW ' + words([0x44, 4, mem.load(OBJ + 0x44, 4)]))
    return case.lines


# ---- disease corpse -------------------------------------------------------------
def corpse_case(name, reach, word50, owner, victims, seed=1):
    """victims: (tag, offset, dead). The native box order is listed in ORDER and
    checked by the replay before it compares."""
    case = Case(name, 'corpse')
    mem = memory()
    case.obj(mem, OBJ, 0x44, 4, seed)
    case.obj(mem, OBJ, 0x50, 4, word50)
    case.obj(mem, OBJ, 0x54, 4, reach)
    found = []
    if owner:
        mem.poke(OBJ + 0x4C, 4, NODE)
        mem.poke(NODE + 0xC, 4, OWNER)
        case.lines.append('OWNER ' + words([KIND['player'], fi(400), 0, 0, 0]))
    for tag, offset, dead in victims:
        addr = VICTIMS[TAG_NUM[tag] - 2]
        flags = SKELIGHT_FLAGS | (8 if dead else 0)
        mem.poke(addr + 0xC, 4, flags)
        put_vec(mem, addr + 0x10, tuple(fi(o) for o in offset))
        case.lines.append('VICTIM ' + words([TAG_NUM[tag], KIND['skelight'],
                                             *(fi(o) for o in offset), 8 if dead else 0, 0, 0]))
    # Native box order: every victim stands in the fixture object's grid cell
    # (non-negative offsets below 256), where the newest link comes first.
    order = [VICTIMS[TAG_NUM[t] - 2] for t, _o, _d in reversed(victims)] + [OBJ]
    found = order
    case.lines.append('ORDER ' + ' '.join(TAGS[a] for a in order))
    model = model_for(mem, World(found=found))
    model.disease_corpse_run(OBJ)
    case.finish_env()
    emit_calls(case, model)
    return case.lines


# ---- multi-fire hit and end -----------------------------------------------------
def hit_case(name, owner_kind, other_kind, stat0=12, stat2=4, d0=10, d4=20, seed=0x1234,
             ended=0, level_ice=0, entry=0x77, other_a8=0, enemy=1, trail=0xAB, frame=5):
    case = Case(name, 'hit')
    mem = memory()
    case.env.update(level=LEVEL(level_ice), entry=entry, frame=frame)
    mem.poke(RC.G_LEVEL_FLAGS, 4, case.env['level'])
    case.obj(mem, OBJ, 0x44, 4, seed)
    case.obj(mem, OBJ, 0x60, 4, ended)
    case.obj(mem, OBJ, 0x68, 4, 1)
    case.obj(mem, OBJ, 0x74, 4, trail)
    mem.poke(OBJ + 0x70, 4, SKILL)
    mem.poke(OBJ + 0x4C, 4, NODE)
    mem.poke(NODE + 0xC, 4, OWNER)
    other = VICTIMS[0]
    if owner_kind == 'player':
        mem.poke(OWNER + 0xC, 4, PLAYER_FLAGS)
        mem.poke(OWNER + 0x3C8, 4, 0)
        mem.poke(RC.G_PLAYERS + 0xD0, 4, d0)
        mem.poke(RC.G_PLAYERS + 0xD4, 4, d4)
        case.lines.append('REC ' + words([d0, d4]))
    else:
        mem.poke(OWNER + 0xC, 4, SKELIGHT_FLAGS)
    case.lines.append('OWNER ' + words([KIND[owner_kind], fi(40), 0, 0, 0]))
    mem.poke(other + 0xC, 4, PLAYER_FLAGS if other_kind == 'player' else SKELIGHT_FLAGS)
    mem.poke(other + 0xA8, 4, other_a8)
    # Team bytes +0x310: the owner 0, the other 1 for an enemy and 0 for a
    # friend, which is what Creature_isEnemy compares (slot 37).
    case.lines.append('VICTIM ' + words([2, KIND[other_kind], fi(-40), 0, 0, 0, other_a8,
                                         1 if enemy else 0]))
    case.lines.append('SKILL ' + words([stat0, stat2]))
    if owner_kind == 'skelight':
        case.lines.append(f'ENEMY v1 {word(enemy)}')
    model = model_for(mem, World(enemies=(other,) if enemy else (), stat0=stat0, stat2=stat2,
                                 entry=entry))
    model.multifire_on_hit(OBJ, other)
    case.finish_env()
    emit_calls(case, model)
    case.lines.append('OW ' + words([0x60, 4, mem.load(OBJ + 0x60, 4)]))
    case.lines.append('OW ' + words([0x44, 4, mem.load(OBJ + 0x44, 4)]))
    deleted = any(n == 'obj_delete' for n, _a in model.calls)
    case.lines.append('DELETED ' + word(1 if deleted else 0))
    return case.lines


def LEVEL(ice):
    return R6.LEVEL_ICE if ice else 0


def end_case(name, reason, ended=0, level_ice=0, seed=9, entry=0x55, trail=0x1110):
    case = Case(name, 'end')
    mem = memory()
    case.env.update(level=LEVEL(level_ice), entry=entry, pos=(fi(3), fi(-4), fi(5)))
    mem.poke(RC.G_LEVEL_FLAGS, 4, case.env['level'])
    put_vec(mem, OBJ + 0x10, case.env['pos'])
    case.obj(mem, OBJ, 0x44, 4, seed)
    case.obj(mem, OBJ, 0x60, 4, ended)
    case.obj(mem, OBJ, 0x74, 4, trail)
    case.lines.append('REASON ' + word(reason))
    model = model_for(mem, World(entry=entry))
    model.multifire_end(OBJ, reason)
    case.finish_env()
    emit_calls(case, model)
    case.lines.append('OW ' + words([0x60, 4, mem.load(OBJ + 0x60, 4)]))
    case.lines.append('OW ' + words([0x44, 4, mem.load(OBJ + 0x44, 4)]))
    deleted = any(n == 'obj_delete' for n, _a in model.calls)
    case.lines.append('DELETED ' + word(1 if deleted else 0))
    return case.lines


# ---- smoke ---------------------------------------------------------------------
def smoke_case(name, frame, amount, radius, cull, focus, pos=(0, 0, 0), victims=(),
               player_at=(500, 0, 0)):
    """victims: (tag, offset as floats, extra flags). The fixture player stands at
    player_at, outside the damage box but inside the view square when cull is 0."""
    case = Case(name, 'smoke')
    mem = memory()
    case.env.update(frame=frame, cull=cull, focus=tuple(fi(f) for f in focus),
                    pos=tuple(fi(p) for p in pos))
    mem.poke(SA.G_SIM_FRAME, 4, frame)
    mem.poke(RC.G_CULL_BY_CAMERA, 1, cull)
    put_vec(mem, RC.G_CAMERA_FOCUS, case.env['focus'] + (0,))
    mem.poke(RC.G_NUM_PLAYERS, 4, 1)
    mem.poke(RC.G_PLAYERS + RC.REC_OBJ, 4, OWNER)
    put_vec(mem, OWNER + 0x10, tuple(fi(p) for p in player_at))
    case.lines.append('OWNER ' + words([KIND['player'], *(fi(p) for p in player_at), 0]))
    put_vec(mem, OBJ + 0x10, case.env['pos'])
    case.obj(mem, OBJ, 0x478, 4, radius)
    case.obj(mem, OBJ, 0x48C, 4, amount)
    near = RC.Model(mem, {}).is_near_view(OBJ + 0x10, R6.F_1000)
    found = [OBJ]
    for tag, offset, extra in victims:
        addr = VICTIMS[TAG_NUM[tag] - 2]
        mem.poke(addr + 0xC, 4, SKELIGHT_FLAGS | extra)
        found.append(addr)
        case.lines.append('VICTIM ' + words([TAG_NUM[tag], KIND['skelight'], *offset, extra, 0, 0]))
    model = model_for(mem, World(found=found, near=near))
    model.smoke_gen_run(OBJ)
    case.finish_env()
    # isNearView is a callback in the model; its answer is the ref_creature body above.
    case.lines.append('NEARVIEW ' + word(near))
    emit_calls(case, model, keep=('SmokeGen_stepParticles',))
    return case.lines


# ---- pustule -------------------------------------------------------------------
def pustule_case(name, timer, burst, z, floor, vz=0xC0000000, seed=0x2468, ground=0x40000000,
                 entry=0x99, drops=None):
    case = Case(name, 'pustule')
    mem = memory()
    case.env.update(ground=ground, entry=entry, pos=(fi(10), fi(-20), z))
    put_vec(mem, OBJ + 0x10, case.env['pos'])
    case.obj(mem, OBJ, 0x44, 4, seed)
    case.obj(mem, OBJ, 0x54, 4, floor)
    case.obj(mem, OBJ, 0x58, 4, 0x3F800000)
    case.obj(mem, OBJ, 0x5C, 4, 0xBF800000)
    case.obj(mem, OBJ, 0x60, 4, vz)
    case.obj(mem, OBJ, 0x64, 4, timer)
    case.obj(mem, OBJ, 0x6C, 4, burst)
    for i, value in enumerate(drops or ()):
        case.obj(mem, OBJ, 0x70 + 4 * i, 4, value)
    model = model_for(mem, World(ground=ground, entry=entry))
    model.pustule_run(OBJ)
    case.finish_env()
    emit_calls(case, model)
    for off in [0x10, 0x14, 0x18, 0x44, 0x60, 0x64, 0x6C] + [0x70 + 4 * i for i in range(60)]:
        case.lines.append('OW ' + words([off, 4, mem.load(OBJ + off, 4)]))
    return case.lines


# ---- spells --------------------------------------------------------------------
def spell_setup(case, mem, spell_seed, creature_pos=(0, 0, 0), facing=0):
    mem.poke(OBJ + 0xC, 4, CREATURE)
    case.obj(mem, OBJ, 0x10, 4, spell_seed)
    pos = tuple(fi(p) for p in creature_pos)
    put_vec(mem, CREATURE + 0x10, pos)
    mem.poke(CREATURE + 0xC, 4, SKELIGHT_FLAGS)
    mem.poke(CREATURE + 0x38, 2, facing)
    case.lines.append('CREATURE ' + words([*pos, facing]))


def creature_words(case, mem, offs):
    for off, size in offs:
        case.lines.append('CW ' + words([off, size, mem.load(CREATURE + off, size)]))


def arena_case(name, counter, seed=0x1357, samples=5, sample=(0x41200000, 0x41A00000, 0x41F00000)):
    case = Case(name, 'arena')
    mem = memory()
    case.env.update(counter=counter & 0xFFFF, samples=samples, sample=sample)
    mem.poke(R6.G_ARENA_COUNTER, 2, counter & 0xFFFF)
    spell_setup(case, mem, seed, (30, -10, 4), 0x2345)
    model = model_for(mem, World(samples=samples, sample=sample))
    try:
        model.arena_run(OBJ, (fi(7), fi(8), fi(9)))
    except ZeroDivisionError:
        case.lines.append('REFUSE')
    case.finish_env()
    emit_calls(case, model)
    case.lines.append('OW ' + words([0x10, 4, mem.load(OBJ + 0x10, 4)]))
    creature_words(case, mem, [(0xC, 4)] + [(0x90 + 2 * i, 2) for i in range(8)])
    return case.lines


def horn_ctor_case(name, frame, seed, kind=1, extra=9, group=0xB100):
    case = Case(name, 'hornctor')
    mem = memory()
    case.env.update(frame=frame, seed=seed, group=group)
    mem.poke(SA.G_SIM_FRAME, 4, frame)
    mem.poke(RC.G_RAND_SEED, 4, seed)
    case.lines.append('CTOR ' + words([kind, extra]))
    model = model_for(mem, World(group=group))
    model.hornomen_ctor(OBJ, kind, CREATURE, extra)
    case.finish_env()
    emit_calls(case, model)
    for off, size in ((0, 1), (4, 4), (8, 4), (0x10, 4), (0x14, 4), (0x18, 4), (0x1C, 4)):
        case.lines.append('OW ' + words([off, size, mem.load(OBJ + off, size)]))
    case.lines.append('SEED ' + word(mem.load(RC.G_RAND_SEED, 4)))
    return case.lines


def horn_run_case(name, frame, end, seed=0x7531, samples=3, sample=(0x40000000, 0x40400000, 0x40800000)):
    case = Case(name, 'horn')
    mem = memory()
    case.env.update(frame=frame, samples=samples, sample=sample, group=0xB100)
    mem.poke(SA.G_SIM_FRAME, 4, frame)
    spell_setup(case, mem, seed, (5, 6, 7), 0x1111)
    case.obj(mem, OBJ, 0x18, 4, end)
    case.obj(mem, OBJ, 0x1C, 4, 0xB100)
    mem.poke(OBJ + 0x14, 4, VT)
    model = model_for(mem, World(samples=samples, sample=sample))
    model.hornomen_run(OBJ, (fi(1), fi(2), fi(3)))
    case.finish_env()
    emit_calls(case, model)
    case.lines.append('OW ' + words([0x10, 4, mem.load(OBJ + 0x10, 4)]))
    creature_words(case, mem, [(0x90 + 2 * i, 2) for i in range(8)])
    return case.lines


def planar_ctor_case(name, frame, seed, size_float, kind=1, extra=9, entry=0x66):
    case = Case(name, 'planarctor')
    mem = memory()
    case.env.update(frame=frame, seed=seed, entry=entry)
    mem.poke(SA.G_SIM_FRAME, 4, frame)
    mem.poke(RC.G_RAND_SEED, 4, seed)
    pos = (fi(11), fi(-12), fi(13))
    put_vec(mem, CREATURE + 0x10, pos)
    mem.poke(CREATURE + 0x20, 4, size_float)
    case.lines.append('CREATURE ' + words([*pos, 0]))
    case.lines.append('CW0 ' + words([0x20, 4, size_float]))
    case.lines.append('CTOR ' + words([kind, extra]))
    model = model_for(mem, World(entry=entry))
    model.planarsteel_ctor(OBJ, kind, CREATURE, extra)
    case.finish_env()
    emit_calls(case, model)
    for off, size in ([(0, 1), (4, 4), (8, 4), (0x10, 4), (0x14, 4), (0x18, 2), (0x1A, 2), (0x1C, 2),
                       (0x20, 4), (0x24, 4), (0x38, 1), (0x39, 1)]
                      + [(0x3A + 2 * i, 2) for i in range(8)]
                      + [(0x4C, 4), (0x50, 4), (0x54, 4), (0x58, 4), (0x5C, 4),
                         (0x60, 4), (0x64, 4), (0x68, 4)]):
        case.lines.append('OW ' + words([off, size, mem.load(OBJ + off, size)]))
    case.lines.append('SEED ' + word(mem.load(RC.G_RAND_SEED, 4)))
    return case.lines


def planar_run_case(name, frame, end, countdown, fired=0, kind=1, seed=0x8642, angles=(0, 0x7FFF),
                    ground=0x40800000, entry=0x44):
    case = Case(name, 'planar')
    mem = memory()
    case.env.update(frame=frame, ground=ground, entry=entry, pos=(fi(-3), fi(4), fi(-5)))
    mem.poke(SA.G_SIM_FRAME, 4, frame)
    spell_setup(case, mem, seed, (20, 30, 2), 0)
    case.obj(mem, OBJ, 0, 1, kind)
    case.obj(mem, OBJ, 0x18, 2, angles[0])
    case.obj(mem, OBJ, 0x1A, 2, angles[1])
    case.obj(mem, OBJ, 0x1C, 2, countdown)
    case.obj(mem, OBJ, 0x20, 4, fired)
    case.obj(mem, OBJ, 0x24, 4, end)
    mem.poke(OBJ + 0x14, 4, VT)
    model = model_for(mem, World(ground=ground, entry=entry))
    model.planarsteel_run(OBJ, case.env['pos'])
    case.finish_env()
    emit_calls(case, model)
    for off, size in ((0x10, 4), (0x18, 2), (0x1A, 2), (0x1C, 2), (0x20, 4), (0x4C, 4), (0x50, 4),
                      (0x54, 4), (0x58, 4), (0x60, 4), (0x64, 4), (0x68, 4)):
        case.lines.append('OW ' + words([off, size, mem.load(OBJ + off, size)]))
    creature_words(case, mem, [(0x90 + 2 * i, 2) for i in range(8)])
    return case.lines


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    global C, RC, SA, R6
    import ref_core as C_
    import ref_creature as RC_
    import ref_skills_a as SA_
    import ref_combat_round6 as R6_
    C, RC, SA, R6 = C_, RC_, SA_, R6_
    C.set_fpu('ps2')
    lines = ['# combat-runtime-6']
    # Gas: growth, the frame-30 damage, a negative target, no owner, the end.
    lines += gas_case('gas-grow', 7, 100, 25, fi(4), 0x10005, 0x1234, victims=(('v1', 'skelight', (fi(-5), 0, 0), 1),))
    lines += gas_case('gas-damage', 30, 100, 20, 0x40980000, 0x20007, 0x55,
                      victims=(('v1', 'skelight', (fi(-3), 0, 0), 1), ('v2', 'skelight', (0, fi(3), 0), 1)))
    lines += gas_case('gas-negative', 60, 100, -15, fi(2), 3, 0x99, owner=False)
    lines += gas_case('gas-ownerless', 60, 100, 11, fi(11), 3, 0x77, owner=False,
                      victims=(('v1', 'skelight', (fi(2), 0, 0), 1),))
    lines += gas_case('gas-end', 100, 100, 20, fi(4), 3, 0x11, owner=False)
    # Corpse: half 1 with two dead, half 2, one alive, an odd negative word, ownerless, empty.
    lines += corpse_case('corpse-half', 16, 3, True, (('v1', (4, 0, 0), True), ('v2', (8, 0, 0), True)))
    lines += corpse_case('corpse-two', 16, 4, True,
                         (('v1', (4, 0, 0), True), ('v2', (8, 0, 0), False), ('v3', (0, 4, 0), True)))
    lines += corpse_case('corpse-negative', 16, -3, True, (('v1', (4, 0, 0), True),))
    lines += corpse_case('corpse-ownerless', 16, 6, False, (('v1', (4, 0, 0), True),))
    lines += corpse_case('corpse-alone', 16, 6, True, ())
    # Multi-fire hits: player owner, player victim skip, flagged creature skip,
    # monster owner with an enemy and with a friend, an already-ended bolt.
    lines += hit_case('hit-player', 'player', 'skelight', stat0=12, stat2=4, d0=10, d4=21)
    lines += hit_case('hit-player-ice', 'player', 'skelight', stat0=7, stat2=3, d0=-9, d4=33, level_ice=1)
    lines += hit_case('hit-player-skip', 'player', 'skelight', other_a8=0x04000000)
    lines += hit_case('hit-player-on-player', 'player', 'player')
    lines += hit_case('hit-monster', 'skelight', 'skelight', stat0=15, seed=0xBEEF)
    lines += hit_case('hit-monster-friend', 'skelight', 'skelight', enemy=0)
    lines += hit_case('hit-ended', 'player', 'skelight', ended=1)
    # The end on its own.
    lines += end_case('end-zero', 0)
    lines += end_case('end-stone', 1)
    lines += end_case('end-ice', 2, level_ice=1, seed=0x3333)
    lines += end_case('end-done', 2, ended=1)
    # Smoke: far, near with damage, a non-damage frame, an empty amount, by player.
    lines += smoke_case('smoke-far', 8, 5, fi(4), 1, (5000, 0))
    lines += smoke_case('smoke-damage', 16, 5, fi(4), 1, (100, -100),
                        victims=(('v1', (fi(3), 0, 0), 0), ('v2', (fi(-3), 0, 0), 1)))
    lines += smoke_case('smoke-odd', 9, 5, fi(4), 1, (0, 0), victims=(('v1', (fi(3), 0, 0), 0),))
    lines += smoke_case('smoke-empty', 8, 0, fi(4), 1, (0, 0), victims=(('v1', (fi(3), 0, 0), 0),))
    lines += smoke_case('smoke-player', 24, 2, 0x40200000, 0, (5000, 5000),
                        victims=(('v1', (0, fi(5), 0), 0),))
    # Pustule: falling, landing (the burst tick), bursting, the age limit, the life limit.
    lines += pustule_case('pus-fall', 3, 0, fi(50), fi(0))
    lines += pustule_case('pus-land', 7, 0, fi(1), fi(0))
    drops = [fi(i) for i in range(30)] + [0x3F000000 + i for i in range(30)]
    lines += pustule_case('pus-burst', 12, 1, fi(0), fi(0), drops=drops)
    lines += pustule_case('pus-age', 36, 1, fi(0), fi(0))
    lines += pustule_case('pus-life', 301, 0, fi(50), fi(0))
    # Arena: below 30, 30, 45, beyond 61, zero samples with particles due.
    lines += arena_case('arena-low', 10)
    lines += arena_case('arena-30', 30)
    lines += arena_case('arena-45', 45)
    lines += arena_case('arena-high', 70)
    lines += arena_case('arena-nosamples', 40, samples=0)
    # Horn omen.
    lines += horn_ctor_case('horn-ctor', 12, 1)
    lines += horn_run_case('horn-early', 100, 100 + 600 - 20)
    lines += horn_run_case('horn-late', 500, 900)
    lines += horn_run_case('horn-nosamples', 500, 900, samples=0)
    lines += horn_run_case('horn-end', 900, 900)
    # Planar steel.
    lines += planar_ctor_case('planar-ctor', 40, 0x2222, 0x42900000)
    lines += planar_run_case('planar-run', 41, 280, 200)
    lines += planar_run_case('planar-burst', 77, 280, 0, kind=2)
    lines += planar_run_case('planar-fired', 78, 280, 0xFFFF, fired=1)
    lines += planar_run_case('planar-end', 280, 280, 5)
    text = ('\n'.join(lines) + '\n').encode()
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text, mtime=0)
    (out / 'combat_runtime_6.txt.gz').write_bytes(blob)
    meta = {
        'schema': 'combat-runtime-6-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'cases': text.count(b'\nCASE '),
    }
    (out / 'combat_runtime_6.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'{meta["cases"]} cases, {len(text)} bytes')


if __name__ == '__main__':
    main()
