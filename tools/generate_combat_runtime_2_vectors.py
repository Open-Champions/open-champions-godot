#!/usr/bin/env python3
"""Generate the public round-2 combat fixtures from immutable research, never native output.

Every case runs the research's composed runtime (ref_combat_full.FullRuntime, the six
round-2 models in one world with the real player tick) on authored synthetic set-ups, and
records after every operation: the events of the trace the runtime keeps, the bytes that
changed, a digest of every non-zero byte of the memory, and a digest of every non-empty
handle word.  A refused operation (Unsupported or CompositionError) ends its case and the
native runtime must refuse it too.

One AUTHORED adjustment of the reference is applied, and only one: ref_combat_full builds
FullServices.PRESENTATION before ref_combat_spells adds 'voice_setPosition' and
'fx_ringBurst' to the presentation names, so the composed runtime refuses Shock of Fire's
first run while ref_combat_spells.SpellRuntime (where the spell is composed) treats both as
presentation.  The generator adds the two names to FullServices.PRESENTATION before any
runtime is built.
"""
import argparse
import gzip
import hashlib
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '8b68a93658b840c34dffddfb662930c99ee931a2'
FIRST = 'd7251e7'
PINS = {
    'docs/combat-runtime.md': '4eaa5b510c8e56ca91b2c0753de8a06397d4f909e1a91da28d90d1313111fd63',
    'docs/spec/combat-movement.md': '30464bb083c90d92dbb27651476ed9049746752b1644dfd7accfad7538ba1740',
    'docs/spec/combat-player-melee.md': '9d1f82d2b0e64f5731912d0327e7d628412f7363c852aa9a188f8814ad9b4e29',
    'docs/spec/combat-player-defence.md': '94f6b07b4e139ce3d2967e33c68b360569c2c84a3e56e1ae5db5b423d46cdf0b',
    'docs/spec/combat-projectiles.md': '176304258f18fd5daaf584471d3fa9d02dea0c4cd64ac3d6d7db46a1bd930d11',
    'docs/spec/combat-spells.md': 'f311616b967398f58fdfd8e298733f1ac67588db232229e61280d4c67ec0c999',
    'docs/spec/combat-monster-death.md': 'c86fbad22eb9f40d6dea32138f5fa796efdf8df29d3094afac59c07aaa9ec8e1',
    'tools/ref_combat_move.py': '28fa523d3c1b4768ac1b147a0fb095dd4cfde8192a9f7db59b0c3d9a39f7cba0',
    'tools/ref_combat_melee.py': '3995b40c39c7c09478e2263b2b43adcfaee6dd30f75198238e5eabc7787c1888',
    'tools/ref_combat_ptick.py': 'b90cc565e1651c659248945d5e98695449e1f6de20138928b18ade69568fb79c',
    'tools/ref_combat_ranged.py': '7755ff203383b92d39a190475b5704fb3fc512abcc107924e09189913d42bda5',
    'tools/ref_combat_spells.py': 'b2cb2574f83f9c056a5cae729f37d29bc817ea18cd6566ddc626e0932aaaca3a',
    'tools/ref_combat_monsters.py': '1c9628ba39a4f6eda0db02f914e03e473f931a1850f6367291efdcf47e208d29',
    'tools/ref_combat_full.py': '57f3a3f2e19a9997b8c024a1f25f03ecfc2b1dfdc53190040682096006c3bfc4',
    'tools/test_ref_combat_move.py': '8c5bfbd0f11e0b0f079a1da296790630dc6dbbdfeebf464a3404c99e7d08a148',
    'tools/test_ref_combat_melee.py': 'a971d7347563cba2c9b530fc48bf6fad47351359e6f8c92e1831723aa5147980',
    'tools/test_ref_combat_ptick.py': '03103d8874fb646fd10ec79cba00d4967b5d83a867d54a370cc9c1c495c76a20',
    'tools/test_ref_combat_ranged.py': 'b21ce57a225ec0e6038e00e0b966cba60b82e23e3c5656a6512ae2a202060482',
    'tools/test_ref_combat_spells.py': '291a37a449726424987cd78a85c0d9b595de2420aad707132ba3675f6e1e6a17',
    'tools/test_ref_combat_monsters.py': 'ce0bb83df608d9db9f84a6d3f0259bc52f5f0bc4ad79ba320888b1ed6539c16b',
    'tools/test_ref_combat_full.py': 'cac7ede6e89919c7d1eed9ba218bc4e899005ab05c6c30d0d259c54e9abbba64',
    'tools/difftest_combat_move.py': 'bcbaa829a929e17af9fccfb849bb210df67e846bd5cf57be44aa79b0ca8967a1',
    'tools/difftest_combat_melee.py': 'fa80442ea1d05efad2a624b908dd1304b604f9bf8b593a6fe9804aa868c5d5ed',
    'tools/difftest_combat_ptick.py': '36aa9c6a410e9f7281a008bf40881a5594eef388a1f798d33f093f3a42bcc2ee',
    'tools/difftest_combat_ranged.py': 'add303919a467162c0b8fbdd894b2e7e653c21dba901847f805865bdfeb288ed',
    'tools/difftest_combat_spells.py': '9ac399fca630c7683d0240db14feb2fcc8539173f4d47a8242c6936cfb0b5788',
    'tools/difftest_combat_monsters.py': 'd8094414af90cd88610d9738ff93fbc7da2a8c445e8f49c2e59bf97121588cf4',
    'tools/ref_combat_runtime.py': '328fbc428e26d50ac5cdb455d1614cf35148216151ba7904bb2ef92984e25ddd',
    'tools/test_ref_combat_runtime.py': 'a462b1ce23611c246b8158eead09a8604a60d5dbe42f5f08f19a797feadce41d',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_objects.py': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_combat.py': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781',
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_player.py': 'ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d',
    'tools/ref_skills_a.py': '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae',
    'tools/ref_skills_b.py': 'b71cbdd9e82cd72abb855b9f6ab99d11fb78a58613cb69d314a5041457bbdd95',
    'tools/ref_skills_c.py': '9e673475a0b89292e483075d76b2f277d1251ce820a0956053e68c83faa81e91',
    'tools/ref_items.py': '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e',
}
EVENT_KINDS = ('run', 'damage_apply', 'damage_result', 'obj_delete', 'destroy', 'destroy_spell', 'cast',
               'player_tick', 'pool_new', 'light', 'player_giveExp', 'loot_generateDrops', 'loot_spawnNamedItem')
M32 = 0xFFFFFFFF
FNV_OFFSET, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3


def fnv(pairs, width):
    h = FNV_OFFSET
    for addr, value in pairs:
        for k in range(4):
            h = ((h ^ ((addr >> (8 * k)) & 0xFF)) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
        for k in range(width):
            h = ((h ^ ((value >> (8 * k)) & 0xFF)) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--research', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    root = Path(args.research)
    snapshot = tempfile.TemporaryDirectory(prefix='combat2-reference-')
    snap = Path(snapshot.name)
    (snap / 'tools').mkdir()
    (snap / 'docs' / 'spec').mkdir(parents=True)
    for name, digest in PINS.items():
        data = subprocess.check_output(['git', '-C', str(root), 'show', COMMIT + ':' + name])
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('research changed: ' + name)
        (snap / name).write_bytes(data)
    sys.path.append(str(snap / 'tools'))
    import ref_combat_runtime as T
    import ref_combat_full as F
    import ref_core as C
    F.FullServices.PRESENTATION = F.FullServices.PRESENTATION + ('voice_setPosition', 'fx_ringBurst')
    gen = Generator(T, F, C)
    gen.all_cases()
    out = Path(args.out)
    data = ('\n'.join(gen.lines) + '\n').encode()
    out.write_bytes(gzip.compress(data, mtime=0))
    manifest = {
        'schema': 'combat-runtime-2-v1', 'research_commit': COMMIT, 'research_range': FIRST + '..' + COMMIT[:7],
        'references': PINS, 'cases': len(gen.cases), 'case_names': gen.cases,
        'refused_cases': gen.refused, 'operations': gen.operations, 'checks': gen.checks,
        'vector_sha256': hashlib.sha256(out.read_bytes()).hexdigest(),
        'content_sha256': hashlib.sha256(data).hexdigest(),
        'adjustment': 'FullServices.PRESENTATION += voice_setPosition, fx_ringBurst (import-order artifact)',
        'scope': 'synthetic composed round-2 runtime: movement, real player tick, melee, defence, '
                 'projectiles, three spells, monster helpers, loot and Goblin death'}
    out.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(len(gen.cases), 'cases;', gen.operations, 'operations;', gen.checks, 'checks;',
          len(gen.refused), 'refused')


class Refused(Exception):
    pass


class Generator:
    def __init__(self, T, F, C):
        self.T, self.F, self.C = T, F, C
        self.lines = ['# combat-runtime-2-v1']
        self.cases, self.refused = [], []
        self.operations = self.checks = 0
        self.rng = random.Random(20973)

    def emit(self, *xs):
        self.lines.append(' '.join(str(x) for x in xs))

    # ---- the case machinery -----------------------------------------------------------
    def case(self, name, mode=1, planes=None, terrain_z=0.0, step=None, skill_stat=0):
        return Case(self, name, mode, planes, terrain_z, step, skill_stat)

    def all_cases(self):
        from combat_runtime_2_cases import build
        build(self)


class StepTerrain:
    """AUTHORED terrain: height `h` at and beyond x0, 0 before (test_ref_combat_ranged's)."""

    def __init__(self, T, x0, h, z=0.0):
        self.T, self.x0, self.h, self.z = T, x0, h, z

    def height_below(self, pos, radius):
        C = self.T.C
        return self.h if C.le(self.x0, pos[0]) else self.z

    def slide(self, pos, delta, mask, radius):
        C = self.T.C
        return [C.add(pos[i], delta[i]) for i in range(3)], 0, (0, 0, 0)

    def raycast(self, origin, direction, mask, factor):
        return self.T.MISS

    def water_test(self, pos, radius):
        return 0


class Case:
    def __init__(self, gen, name, mode, planes, terrain_z, step, skill_stat):
        T, F = gen.T, gen.F
        self.gen, self.T, self.F, self.C = gen, T, F, gen.C
        f32 = T.f32
        terrain = T.FlatTerrain(f32(terrain_z))
        if step is not None:
            terrain = StepTerrain(T, f32(step[0]), f32(step[1]), f32(terrain_z))
        plane_list = None
        if planes is not None:
            plane_list = planes
        value = f32(skill_stat) if isinstance(skill_stat, float) else skill_stat
        self.rt = F.FullRuntime(terrain=terrain, skill_stat=lambda name, *a: value, camera_mode=mode,
                                screen_planes=plane_list)
        self.name = name
        self.dead = False
        self.mark = 0
        self.prev = {}
        self.prev_heads = {}
        gen.cases.append(name)
        plane_words = []
        if planes is not None:
            for point, normal in planes:
                plane_words += [f32(v) for v in point] + [f32(v) for v in normal]
        gen.emit('CASE', name, mode, 1 if planes is not None else 0, *plane_words)
        gen.emit('TERRAIN', f32(terrain_z), 1 if step is not None else 0,
                 f32(step[0]) if step else 0, f32(step[1]) if step else 0)
        if skill_stat:
            gen.emit('SKILLSTAT', value)
        self.check()

    # ---- running one operation ----------------------------------------------------------
    def op(self, *line, fn):
        if self.dead:
            return None
        self.gen.operations += 1
        self.gen.emit(*line)
        self.mark = len(self.rt.trace)
        try:
            result = fn()
        except (self.T.Unsupported, self.T.CompositionError, RecursionError) as e:
            # RecursionError: the composed reference re-dispatches a virtual slot that no
            # model has a body for without end (no defined result); refused.
            self.gen.emit('REFUSED')
            reason = 'unbounded virtual re-dispatch' if isinstance(e, RecursionError) else str(e)
            self.gen.refused.append(self.name + ': ' + reason)
            self.dead = True
            return None
        self.check()
        return result

    def check(self):
        T = self.T
        rt, gen = self.rt, self.gen
        events = []
        for e in rt.trace[self.mark:]:
            kind = e[0]
            if kind not in EVENT_KINDS:
                continue
            if kind == 'run':
                events.append(('run', [e[1]]))
            elif kind == 'damage_apply':
                events.append(('damage_apply', [e[1], e[2], e[3] & M32, e[4] & M32]))
            elif kind == 'damage_result':
                events.append(('damage_result', [e[1], e[2] & M32]))
            elif kind in ('obj_delete', 'destroy', 'destroy_spell', 'light'):
                events.append((kind, [e[1]]))
            elif kind == 'cast':
                events.append(('cast', [e[1], e[2], e[3]]))
            elif kind == 'player_tick':
                events.append(('player_tick', [e[1], e[3]], e[2]))
            elif kind == 'pool_new':
                events.append(('pool_new', [e[1], e[2]]))
            elif kind == 'player_giveExp':
                events.append(('player_giveExp', [e[1] & M32, e[2] & M32]))
            elif kind in ('loot_generateDrops', 'loot_spawnNamedItem'):
                events.append((kind, [v & M32 for v in e[1:]]))
        self.mark = len(rt.trace)
        gen.emit('EV', len(events))
        for ev in events:
            text = ev[2] if len(ev) > 2 else None
            gen.emit('e', ev[0], len(ev[1]), *ev[1], *([text.replace(' ', '_')] if text else []))
        mem = rt.mem
        now = {a: v for a, v in mem.bytes.items() if v}
        delta = []
        for a, v in now.items():
            if self.prev.get(a) != v:
                delta.append((a, v))
        for a in self.prev:
            if a not in now:
                delta.append((a, 0))
        delta.sort()
        self.prev = now
        gen.emit('D', len(delta), *[x for pair in delta for x in pair])
        gen.emit('M', len(now), fnv(sorted(now.items()), 1))
        heads = {}
        for a, cell in mem.heads.items():
            v = mem.node_addr(cell.next)
            if v:
                heads[a] = v
        hd = []
        for a, v in heads.items():
            if self.prev_heads.get(a) != v:
                hd.append((a, v))
        for a in self.prev_heads:
            if a not in heads:
                hd.append((a, 0))
        hd.sort()
        self.prev_heads = heads
        gen.emit('HD', len(hd), *[x for pair in hd for x in pair])
        gen.emit('H', len(heads), fnv(sorted(heads.items()), 4))
        gen.checks += 1 + len(events) + len(delta) + len(hd)

    def end(self):
        """Node pool digest at the end of the case."""
        if self.dead:
            return
        pool = self.rt.world.handles
        mem = self.rt.mem
        pairs = []

        def link(x):
            if x is not None and not isinstance(x, (self.C.Cell, self.C.Node)):
                return 0xFFFFFFFF                       # a (finished) iterator left as a stale link
            return mem.node_addr(x)
        for i, n in enumerate(pool.nodes):
            words = (link(n.next), link(n.prev), link(n.partner),
                     n.obj.addr if n.obj is not None else 0, n.tag & 0xFFFF)
            for k, w in enumerate(words):
                if w:
                    pairs.append((i * 8 + k, w))
        free = link(pool.free)
        import os
        if os.environ.get('CR2_DUMP_NODES') and self.name == os.environ.get('CR2_DUMP_CASE'):
            with open(os.environ['CR2_DUMP_NODES'], 'w') as out:
                for i, n in enumerate(pool.nodes):
                    words = (link(n.next), link(n.prev), link(n.partner),
                             n.obj.addr if n.obj is not None else 0, n.tag & 0xFFFF)
                    out.write('%d %s\n' % (i, ' '.join('%x' % w for w in words)))
                out.write('free %x\n' % free)
        self.gen.emit('NODES', len(pairs), fnv(pairs, 4), free)

    # ---- authored set-up ----------------------------------------------------------------
    def spawn_skelight(self, pos, facing=0, hp=100.0, team=0xFF, cflags=0x100000, armor=0, damage=(4, 8),
                       attack=100, home_range=600.0):
        T, f32 = self.T, self.T.f32
        out = {}

        def run():
            out['a'] = self.rt.spawn_skelight(pos, facing=facing, hp=hp, armor=armor, damage=damage, attack=attack,
                                              team=team, home_range=home_range, cflags=cflags)
            return out['a']
        # the address is known after the call; emit it with the operation (S ... address)
        self.gen.operations += 1
        self.mark = len(self.rt.trace)
        a = run()
        self.gen.emit('S', *[f32(v) for v in pos], facing & 0xFFFF, f32(hp), team & M32, cflags, armor & M32,
                      damage[0] & M32, damage[1] & M32, attack & M32, f32(home_range), a)
        self.check()
        return a

    def spawn_player(self, slot, pos, facing=0, hp=100.0, level=1, strength=20, armor=0, team=0,
                     idle_voice_limit=0x7FFF, speed_mul=1.0):
        f32 = self.T.f32
        self.gen.operations += 1
        self.mark = len(self.rt.trace)
        P = self.rt.spawn_player(slot, pos, facing=facing, hp=hp, level=level, strength=strength, armor=armor,
                                 team=team, idle_voice_limit=idle_voice_limit, speed_mul=speed_mul)
        self.gen.emit('P', slot, *[f32(v) for v in pos], facing & 0xFFFF, f32(hp), level, strength, armor & M32,
                      team & M32, idle_voice_limit & 0xFFFF, f32(speed_mul), P)
        self.check()
        return P

    def link(self, head, target):
        return self.op('LINK', head, target, fn=lambda: self.rt.relink(head, target))

    def loot_class(self, name):
        """AUTHORED type identities for Creature_dropLoot: the creature is of class `name`
        (or of none); loot call-outs are recorded in the trace."""
        rt = self.rt
        token = ('class', name)
        rt.plug('Creature.v0', lambda *a: token)
        for cls in ('SpiderQueen', 'AntQueen', 'UndeadKnight', 'VampireLord', 'Arenabeast', 'SeaMonster',
                    'CloudGiant', 'Minion', 'MummyKing', 'Innoruuk', 'Zek'):
            rt.plug(cls + '_typeinfo', (lambda c: lambda *a: ('class', c))(cls))
        rt.plug('rtti_typeInfoEquals', lambda a, b: 1 if a == b and a[1] is not None else 0)
        rt.plug('loot_generateDrops',
                lambda pos, angle, filt, force, z, thr, gs, gc, q, ic:
                rt.trace.append(('loot_generateDrops',) + tuple(pos) + (angle, filt, force, z, thr, gs, gc, q, ic)))
        rt.plug('loot_spawnNamedItem',
                lambda pos, name: rt.trace.append(('loot_spawnNamedItem',) + tuple(pos) + (name,)))
        self.gen.emit('CLASS', name or '-')

    def write(self, a, size, value):
        value &= (1 << (8 * size)) - 1
        return self.op('W', a, size, value, fn=lambda: self.rt.mem.poke(a, size, value))

    def clips(self, token, length, events):
        """AUTHORED animation timeline of a clip token."""
        self.rt.anim.clips[token] = (length, dict(events))
        self.gen.emit('CLIP', token, length, len(events), *[x for k in sorted(events) for x in (k, events[k])])

    def swing_clips(self, c, length=30, hit=10):
        definition = self.rt.u32(c + 0x24)
        for off in (0x1C, 0x24):
            self.clips(self.rt.u32(definition + off), length, {hit: 1})

    def root(self, actor, rx, ry):
        f32 = self.T.f32
        self.rt.root_motion[actor + 0x114] = (rx, ry)
        self.gen.emit('ROOT', actor, f32(rx), f32(ry))

    def ray(self, t):
        self.rt.terrain.raycast = lambda *a: t
        self.gen.emit('RAY', t)

    def yaw(self, value):
        self.rt.svc_net_getCamYaw = lambda slot: value & M32
        self.gen.emit('YAW', value & M32)

    def dialog(self, on):
        self.rt.svc_dialog_isOpen = lambda: on
        self.gen.emit('DIALOG', on)

    def travel(self, on):
        self.rt.svc_jumpgate_getTravelState = lambda: on
        self.gen.emit('TRAVEL', on)

    def tick(self):
        return self.op('TICK', fn=self.rt.tick)

    def stick(self, slot, x, y):
        f32 = self.T.f32
        return self.op('STICK', slot, f32(x), f32(y), fn=lambda: self.rt.stick(slot, x, y))

    def attack(self, slot, held=True):
        return self.op('ATTACK', slot, int(held), fn=lambda: self.rt.press_attack(slot, held))

    def weapon(self, slot, dmg=(5, 9), attack=10, subtype=0, dmg_type=1, cls=0, mods=()):
        out = {}

        def run():
            out['i'] = self.rt.give_weapon(slot, dmg=dmg, attack=attack, subtype=subtype, dmg_type=dmg_type, cls=cls,
                                           mods=mods)
        line = ['WEAPON', slot, dmg[0] & M32, dmg[1] & M32, attack & M32, subtype & 0xFF, dmg_type & M32, cls & 0xFF,
                len(mods)] + [v & 0xFFFF for m in mods for v in m]
        self.op(*line, fn=run)
        return out.get('i')

    def skill(self, skill_id, stats, kind=0, recast=0, flags=0):
        f32 = self.T.f32
        words = []
        for i in range(5):
            first, base, step = stats.get(i, (1, 1, 0.0))
            words += [first & 0xFFFF, base & 0xFFFF, f32(step)]
        out = {}

        def run():
            out['s'] = self.rt.give_skill(skill_id, stats=stats, kind=kind, recast=recast, flags=flags)
        self.op('SKILL', skill_id, kind, recast & 0xFFFF, flags & M32, *words, fn=run)
        return out.get('s')

    def cast(self, caster, skill_id, level=1, point=None, facing=None):
        f32 = self.T.f32
        line = ['CASTQ', caster, skill_id, level]
        line += [1] + [f32(v) for v in point] if point is not None else [0]
        line += [1, facing & M32] if facing is not None else [0]
        pt = None if point is None else tuple(f32(v) for v in point)
        return self.op(*line, fn=lambda: self.rt.queue_cast(caster, skill_id, level, pt, facing))

    def missile(self, owner, pos, vel, facing=0, dmin=1, dmax=2, attack=100, dflags=4, item=0, kind=0, launch=0,
                impact=0, wrating=0, iflags=0, models=(0, 0)):
        f32 = self.T.f32
        out = {}

        def run():
            out['a'] = self.rt.spawn_missile(owner, pos, vel, facing=facing, dmin=dmin, dmax=dmax, attack=attack,
                                             dflags=dflags, item=item, kind=kind, launch=launch, impact=impact,
                                             wrating=wrating, iflags=iflags, models=models)
        self.op('MISSILE', owner, *[f32(v) for v in pos], *[f32(v) for v in vel], facing & 0xFFFF, dmin & M32,
                dmax & M32, attack & M32, dflags & M32, item, kind & M32, launch, impact, wrating & M32, iflags & M32,
                models[0], models[1], fn=run)
        return out.get('a')

    def thrown(self, owner, pos, vel, facing=0, dmin=1, dmax=2, attack=100000, kind=4, item=0, models=(0, 0)):
        f32 = self.T.f32
        out = {}

        def run():
            out['a'] = self.rt.spawn_thrown(owner, pos, vel, facing=facing, dmin=dmin, dmax=dmax, attack=attack,
                                            kind=kind, item=item, models=models)
        self.op('THROWN', owner, *[f32(v) for v in pos], *[f32(v) for v in vel], facing & 0xFFFF, dmin & M32,
                dmax & M32, attack & M32, kind & 0xFF, item, models[0], models[1], fn=run)
        return out.get('a')

    def address_of(self, key):
        return key

    def call(self, name, args, fn):
        """A direct call of an executed body: emits the result."""
        out = {}

        def run():
            out['r'] = fn()
        self.op('CALL', name, len(args), *[a & M32 for a in args], fn=run)
        if not self.dead:
            r = out['r']
            if isinstance(r, (list, tuple)):
                self.gen.emit('RL', len(r), *[x & M32 for x in r])
            else:
                self.gen.emit('R', (r or 0) & M32)
        return out.get('r')


if __name__ == '__main__':
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    main()
