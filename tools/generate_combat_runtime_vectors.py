#!/usr/bin/env python3
"""Generate public synthetic combat fixtures from immutable research, never native output."""
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
COMMIT = '9289d564acadcdd8270b48ed9155ac6289cf17f5'
PINS = {'docs/combat-runtime.md': 'f5aa058d5e47e988dc63a6c366c4bd1de1ad2191fedbaa3c763a0f7e6643aee3',
 'tools/ref_combat_runtime.py': '328fbc428e26d50ac5cdb455d1614cf35148216151ba7904bb2ef92984e25ddd',
 'tools/test_ref_combat_runtime.py': 'a462b1ce23611c246b8158eead09a8604a60d5dbe42f5f08f19a797feadce41d',
 'tools/difftest_combat_runtime.py': '1f5cf47aae712fa15010e26ea9eed413906a53559d9e8a6ccde0d8eb837e8a6b',
 'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
 'tools/ref_combat.py': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781',
 'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
 'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
 'tools/ref_player.py': 'ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d',
 'tools/ref_objects.py': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--research', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    root = Path(args.research)
    # Never import the mutable checkout: verify and read the requested Git blobs.
    snapshot = tempfile.TemporaryDirectory(prefix='combat-reference-')
    snap = Path(snapshot.name)
    (snap / 'tools').mkdir()
    (snap / 'docs').mkdir()
    for name, digest in PINS.items():
        data = subprocess.check_output(['git', '-C', str(root), 'show', COMMIT + ':' + name])
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('research changed: ' + name)
        (snap / name).write_bytes(data)
    sys.path.append(str(snap / 'tools'))
    import ref_combat_runtime as T
    f = T.f32
    rng = random.Random(20973)
    lines = ['# combat-runtime-v1']
    cases = []

    def emit(*xs):
        lines.append(' '.join(map(str, xs)))

    class Case:

        def __init__(self, name, mode=1):
            self.rt = T.Runtime(camera_mode=mode)
            self.actors = []
            self.records = []
            self.name = name
            emit('CASE', name, mode)
            cases.append(name)

        def spawn(self, pos=(0, 0, 0), player=None, **kw):
            face = kw.get('facing', 0)
            hp = kw.get('hp', 100)
            team = kw.get('team', 0 if player is not None else -1)
            cf = kw.get('cflags', 1048576)
            armor = kw.get('armor', 0)
            if player is None:
                a = self.rt.spawn_skelight(pos, facing=face, hp=hp, team=team, cflags=cf, armor=armor)
                emit('S', *[f(v) for v in pos], face, f(hp), team & 4294967295, cf, armor & 4294967295, a)
            else:
                a = self.rt.spawn_player(player, pos, facing=face, hp=hp, team=team, armor=armor)
                self.records.append(T.G_PLAYERS + player * T.REC_SIZE)
                emit('P', player, *[f(v) for v in pos], face, f(hp), team & 4294967295, cf, armor & 4294967295, a)
            self.actors.append((a, 1424 if player is not None else 968))
            return a

        def write(self, a, z, v):
            self.rt.mem.poke(a, z, v)
            emit('W', a, z, v & (1 << 8 * z) - 1)

        def link(self, a, b):
            self.rt.relink(a, b)
            emit('LINK', a, b)

        def ray(self, t):
            self.rt.terrain.raycast = lambda *a: t
            emit('RAY', t)

        def snap(self):
            # Full effective actor words include subword state and handle identities.
            for a, size in self.actors:
                if a not in self.rt.world.objects:
                    continue
                for off in range(0, size, 4):
                    emit('R', a + off, 4, self.rt.mem.load(a + off, 4))
            for r in self.records:
                for off, z in ((80, 4), (88, 2), (144, 4), (172, 4), (176, 4), (204, 4), (208, 4), (212, 4), (220, 4), (17368, 4), (19141, 1), (19142, 1), (19492, 1), (19495, 1)):
                    emit('R', r + off, z, self.rt.mem.load(r + off, z))
            for a, z in ((T.G_RAND_SEED, 4), (T.G_SIM_FRAME, 4), (T.G_ALIVE_COUNT, 4), (T.G_LOCAL_ALIVE_COUNT, 4), (T.G_FORCE_GHOST_REBASE, 1), (T.G_HUD_HIT_FLASH, 4), (T.G_HUD_HIT_FLASH + 4, 4)):
                emit('R', a, z, self.rt.mem.load(a, z))

        def order(self, n):
            emit('ORDER', n, len([e for e in self.rt.trace if e[0] == n]), *[e[1] for e in self.rt.trace if e[0] == n])
    c = Case('shared-image-reuse')
    a = c.spawn(team=1)
    b = c.spawn((40, 0, 0), team=2)
    c.link(a + T.H_TARGET, b)
    c.link(a + T.H_LAST_ATTACKER, b)
    c.snap()
    c.rt.world.on_destroy(c.rt.world.obj(b), 3)
    emit('DESTROY', b)
    d = c.spawn((80, 0, 0))
    assert d == b
    c.snap()
    for k in range(96):
        c = Case('visibility-cone-' + str(k))
        a = c.spawn(team=1, facing=rng.randrange(65536))
        b = c.spawn((rng.randrange(-100, 101), rng.randrange(-100, 101), rng.randrange(-15, 16)), team=2)
        for off in (776, 780):
            c.write(b + off, 4, f(rng.randrange(-20, 21)))
        t = rng.choice([-1, 0, 0.5, 1, 1.1])
        c.ray(f(t))
        face = rng.randrange(65536)
        arc = rng.choice([0, 1, 6000, 8192, 32767])
        mode = k % 2
        near = f(rng.choice([0, 10, 40]))
        far = f(rng.choice([20, 48, 100]))
        lead = f(rng.choice([0, 0.5, 1]))
        want = c.rt.model.can_see_obj(a, b, face, arc, mode, near, far, lead)
        emit('SEE', a, b, face, arc, mode, near, far, lead, want)
        want = c.rt.model.find_enemy_in_cone(a, face, arc, near, far)
        emit('CONE', a, face, arc, near, far, want)
        c.snap()
    c = Case('cone-no-hard-range')
    a = c.spawn(team=1)
    b = c.spawn((60, 0, 0), team=2)
    want = c.rt.model.find_enemy_in_cone(a, 0, 8192, 0, f(48))
    assert want == b
    emit('CONE', a, 0, 8192, 0, f(48), want)
    c.snap()
    for k in range(64):
        c = Case('enemy-and-target-' + str(k))
        a = c.spawn(player=0) if k % 2 else c.spawn(team=1)
        b = c.spawn((30, 0, 0), team=rng.choice([-1, 0, 1, 2]))
        d = c.spawn((45, 10, 0), team=2)
        c.write(b + 12, 4, c.rt.u32(b + 12) | rng.choice([0, 8, 16777216, 131072]))
        c.write(b + 786, 2, rng.choice([0, 1]))
        c.write(b + 168, 8, rng.choice([0, 2048, 67108864, 17592186044416]))
        want = c.rt.model.vcall(a, 20, b)
        emit('ENEMY', a, b, want)
        c.rt.model.vcall(a, 23, 1)
        emit('PICK', a, 1)
        c.snap()
    for k in range(64):
        c = Case('player-find-' + str(k))
        p = c.spawn(player=0)
        a = c.spawn((rng.randrange(1, 75), rng.randrange(-30, 31), 0))
        b = c.spawn((-40, 10, 0))
        c.ray(f(rng.choice([-1, -1, 0, 1.1])))
        if k % 8 == 0:
            for i, v in enumerate(b'foostomedalbar\x00'):
                c.write(T.G_LEVEL_NAME + i, 1, v)
        c.rt.model.player_find_target(p, f(60))
        emit('FIND', p, f(60))
        c.snap()
    for k in range(96):
        c = Case('damage-seeds-' + str(k))
        p = c.spawn(player=0, armor=rng.choice([0, 10, 50]))
        m = c.spawn((30, 0, 0), armor=rng.choice([0, 20, 100]))
        v, a = (p, m) if k % 2 else (m, p)
        lo = f(rng.choice([0, 4, 10]))
        hi = f(rng.choice([8, 15, 30]))
        attack = rng.choice([1, 5, 100, 100000])
        flags = rng.choice([1, 8, 64, 262152, 67108872])
        seed = rng.getrandbits(32)
        c.write(T.G_RAND_SEED, 4, seed)
        want = c.rt.svc_damage_apply(v, a, attack, 0, 0, flags, 0, 0, lo, hi)
        emit('DAMAGE', v, a, lo, hi, attack, flags, want)
        c.snap()
    for k in range(80):
        c = Case('player-take-process-' + str(k))
        p = c.spawn(player=0, hp=100)
        m = c.spawn((30, 10, 0))
        flags = rng.choice([8, 262152, 4194312, 67108872, 8388616])
        amount = f(rng.choice([0, 1, 10, 200]))
        c.write(T.G_RAND_SEED, 4, rng.getrandbits(32))
        if k % 7 == 0:
            c.write(T.G_PLAYERS + 19495, 1, 3)
        if k % 11 == 0:
            c.write(p + 1204, 2, 2)
        c.rt.model.player_take_damage(p, m, flags, amount)
        emit('TAKE', p, m, flags, amount)
        c.snap()
        c.rt.model.player_process_damage(p)
        emit('PROCESS', p)
        c.snap()
    for k in range(48):
        c = Case('unarmed-melee-' + str(k))
        p = c.spawn(player=0)
        m = c.spawn((30, 0, 0))
        c.write(T.G_PLAYERS + 144, 4, rng.randrange(0, 100))
        c.write(p + 884, 4, k % 2)
        c.write(p + 892, 4, k // 2 % 2)
        c.write(T.G_RAND_SEED, 4, rng.getrandbits(32))
        c.rt.model.player_melee_hit(p, m, 0, 0, 0)
        emit('MELEE', p, m, 0)
        c.snap()
    for k in range(40):
        c = Case('arc-order-' + str(k))
        a = c.spawn(team=1)
        b = c.spawn((30, 5, 0), team=2)
        d = c.spawn((30, -5, 0), team=2)
        c.spawn((25, 0, 0), team=1)
        c.link(a + T.H_TARGET, b)
        c.ray(f(rng.choice([-1, -1, 0])))
        all_ = k % 2
        params = (a, 0, 8192, 4, 8, 100000, 8, 0, 0, f(48))
        fn = c.rt.model.hit_all_enemies_in_arc if all_ else c.rt.model.hit_target_in_arc
        want = fn(*params)
        emit('ARC', a, all_, 0, 8192, 4, 8, 100000, 8, 0, f(48), want)
        c.order('damage_apply')
        c.snap()
    for k in range(48):
        c = Case('knockback-' + str(k))
        a = c.spawn(cflags=0 if k % 2 else 1048576)
        c.write(a + 744, 4, f(rng.choice([-2, 0, 1, 100])))
        c.write(a + 736, 2, rng.randrange(65536))
        c.write(a + 188, 4, f(3))
        c.write(a + 192, 4, f(5))
        c.write(a + 196, 2, 100)
        c.write(a + 68, 4, rng.getrandbits(32))
        scale = f(rng.choice([0, 0.7, 1]))
        c.rt.model.apply_knockback(a, scale)
        emit('KNOCK', a, scale)
        c.snap()
    for k in range(32):
        c = Case('corpse-' + str(k))
        a = c.spawn(cflags=16777216 if k % 2 else 0)
        c.write(a + 282, 2, rng.choice([0, 8, 128]))
        c.write(a + 280, 1, 1)
        c.write(a + 756, 4, f(rng.choice([0, 71.9, 73])))
        hold = k % 3 == 0
        c.rt.model.dead_update(a, 128, (0, 0, 0), int(hold), f(20))
        emit('DEAD', a, 128, int(hold), f(20))
        c.snap()
    c = Case('creature-contact-order')
    p = c.spawn(player=0)
    a = c.spawn((20, 0, 0))
    want = c.rt.model.move_by(a, (0, 0, 0))
    emit('MOVE', a, 0, 0, 0, want)
    c.snap()
    for active in (False, True):
        c = Case('composed-skelight-' + str(active), 0 if active else 1)
        a = c.spawn(team=1, hp=60)
        b = c.spawn((30, 0, 0), team=2, hp=60, facing=32768)
        for actor in (a, b):
            for off in (28, 36):
                c.rt.anim.clips[c.rt.clip_token(actor, off)] = (30, {10: 1})
        for tick in range(80):
            c.rt.tick()
            emit('TICK')
            c.snap()
        c.order('run')
        c.order('damage_apply')
        c.order('obj_delete')
        c.order('destroy')
    # Callback-gated player damage paths; flags can bypass dialog suppression.
    for k in range(12):
        c = Case('player-dialog-travel-' + str(k))
        p = c.spawn(player=0)
        m = c.spawn((30, 0, 0))
        travel = k % 3 == 0
        c.rt.svc_jumpgate_getTravelState = lambda: int(travel)
        emit('TRAVEL', int(travel))
        flags = 8 | (0x20000000 if k % 2 else 0)
        c.rt.model.player_take_damage(p, m, flags, f(10))
        emit('TAKE', p, m, flags, f(10))
        c.rt.svc_dialog_isOpen = lambda: 1
        emit('DIALOG', 1)
        if k % 4 == 0:
            c.write(T.G_CUTSCENE_ACTIVE, 1, 1)
        c.rt.model.player_process_damage(p)
        emit('PROCESS', p)
        c.snap()
    out = Path(args.out)
    data = ('\n'.join(lines) + '\n').encode()
    out.write_bytes(gzip.compress(data, mtime=0))
    manifest = {'schema': 'combat-runtime-v1', 'research_commit': COMMIT, 'references': PINS, 'cases': len(cases), 'case_names': cases, 'checks': sum((s.startswith(('R ', 'SEE ', 'CONE ', 'ENEMY ', 'DAMAGE ', 'ARC ', 'ORDER ', 'MOVE ')) for s in lines)), 'vector_sha256': hashlib.sha256(out.read_bytes()).hexdigest(), 'content_sha256': hashlib.sha256(data).hexdigest(), 'scope': 'synthetic executed helpers, shared image, handles, seeds and Skelight composition; observed-only player movement refused'}
    out.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(len(cases), 'cases;', manifest['checks'], 'comparisons')
if __name__ == '__main__':
    main()
