#!/usr/bin/env python3
"""The fire entry of a LooseFire touch and the burn tick it leaves, from research commit
5b92837 (docs/spec/fire-touch.md, tools/ref_fire_touch.py, executed against the original
by tools/difftest_fire_touch.py), never from native output.

Each case is one damage_apply call with the arguments ref_fire_touch composes:

- the touch: damage_apply(toucher, owner or the fire, 0, 0, 10000000, 0x0100c040,
  (0x15, v, v, 0) or the empty descriptor, 0), v from touch_entry (a player's from
  g_difficultyIdx, a creature's 8);
- the burn tick: damage_apply(c, c, 0, 0, 100000, 0x1180c040, (0x15, lo, hi, 0), 0),
  lo = hi = 1 for a monster, a player's range from burn_range (its shared draw is made
  first, as Creature_run makes it);
- a re-touch of a victim that is already burning.

The arguments are captured from ref_fire_touch.loosefire_touch and burn_tick themselves
(a recorder around ref_combat.damage_apply), then the row runs through
tools/generate_combat_damage_vectors.run on the pinned ref_combat (the same file the
combat-damage corpus pins), whose authored world the native combat-damage harness
rebuilds. The rows use that harness's format; tools/test_fire_touch_vectors.py checks
the provenance and runs them. A Frost Armor carrier's burn tick with a strike-back
chance of 100 or more recurses without end in the original (fire-touch.md section 3);
it is not a row: the native port refuses it, and the harness's anchors check that.
"""
import gzip
import hashlib
import json
import random
import subprocess
import sys
import types
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '5b928373a4bc31e5566312de598f861cc9f58071'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat-ctors-2'
PINS = {
    'docs/spec/fire-touch.md': '11feb9072f6d3c17d5b4bc816cd94f2854b70922e9699530d0296dcc73802426',
    'tools/ref_fire_touch.py': '82ac8c6ff727c90aef86a1f1c96f9fc6c86528d7bc66d17b71da1a15d04e73f4',
    'tools/test_ref_fire_touch.py': '6d4df0274f52455b6cd6b5803c86b12024ce6f9a78571d4dc41b15354e9f0510',
    'tools/difftest_fire_touch.py': '38d95e62b0377c550b93bdaabcf81564fbfb67b3805b7d4337e7b6f933bce2d9',
    'tools/ref_combat.py': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781',
}
M32 = 0xFFFFFFFF
ROOT = Path(__file__).resolve().parents[1]


def pin_ok():
    for path, want in PINS.items():
        blob = subprocess.check_output(['git', '-C', str(RESEARCH), 'show', f'{COMMIT}:{path}'])
        if hashlib.sha256(blob).hexdigest() != want:
            raise SystemExit(f'{path} at {COMMIT} is not the pin')
        if path.startswith('tools/') and hashlib.sha256((RESEARCH / path).read_bytes()).hexdigest() != want:
            raise SystemExit(f'{path} on disk is not {COMMIT}')


def lcg(state):
    state = (state * 0x19660D + 0x3C6EF35F) & M32
    return state, state >> 16


def main():
    pin_ok()
    sys.path.insert(0, str(RESEARCH / 'tools'))
    sys.path.insert(0, str(ROOT / 'tools'))
    import ref_combat as R
    import ref_fire_touch as F
    import generate_combat_damage_vectors as G
    raw = (RESEARCH / 'tools/ref_combat.py').read_bytes()
    r = types.ModuleType('external_combat')
    r.__file__ = str(RESEARCH / 'tools/ref_combat.py')
    exec(compile(raw, r.__file__, 'exec'), r.__dict__)
    config = r.Fpu.MODELS.index('ps2') * 4          # ps2, no multiply bias, chopped divide
    r.set_fpu('ps2', False, False)
    R.set_fpu('ps2', False, False)

    captured = []
    real_damage_apply = R.damage_apply

    def recorder(w, victim, attacker, minimum, maximum, attack, flags, desc, no_crit=0):
        captured.append((victim.tag, attacker.tag if attacker else None, minimum, maximum, attack, flags,
                         [tuple(e) for e in desc] if desc is not None else None, no_crit))
        return 1
    R.damage_apply = recorder

    def reference_args(step, victim_flags, attacker_kind, difficulty, frame, seed, skill_number=40):
        """The damage_apply arguments ref_fire_touch passes for this step."""
        captured.clear()
        w = R.World(seed=seed)
        w.frame, w.difficulty = frame, difficulty
        victim = R.Obj(tag='v', flags=victim_flags, cls='Creature' if victim_flags & 0x60 else 'Third')
        if step == 'burn':
            F.burn_tick(w, victim, skill_number)
            return captured[0], w.seed.state
        owner = None
        if attacker_kind == 'creature':
            owner = R.Obj(tag='a', flags=0x20, cls='Creature')
        elif attacker_kind == 'player':
            owner = R.Obj(tag='a', flags=0x60, cls='Creature')
        fire = R.Obj(tag='fire', flags=0x420C, cls='Third')
        if owner is not None:
            fire.owner_link, fire.owner = True, owner
        F.loosefire_touch(w, fire, victim, lambda o, t: 1)
        return captured[0], w.seed.state

    rng = random.Random(0x15F1E)
    rows, counts = [], {}
    victims = (('creature', 0x20), ('player', 0x60), ('object', 0x1))
    attackers = ('fire', 'creature', 'player')
    resist = (0, 0x3E800000, 0x3F000000, 0x3F800000)
    for step in ('touch', 'burn', 'retouch'):
        for vname, vflags in victims:
            if step != 'touch' and not vflags & 0x60:
                continue
            for attacker in attackers if step != 'burn' else ('self',):
                for k in range(18):
                    res = rng.choice(resist)
                    difficulty = rng.choice((0, 1, 2, 3, 4, 5, 6, -1))
                    frame = rng.choice((600, 640, 660)) if step != 'burn' else rng.choice((608, 640, 672))
                    seed = rng.getrandbits(32)
                    armor = rng.choice((0, 0, 10, 3000))
                    pvp = rng.choice((0, 0, 1))
                    frost = vflags & 0x60 and rng.randrange(5) == 0
                    burning = 0x3F000000 if step == 'retouch' else (0x40000000 if step == 'burn' else 0)
                    args, _ = reference_args('burn' if step == 'burn' else 'touch', vflags,
                                             attacker, difficulty, frame, seed)
                    victim_tag, attacker_tag, lo, hi, attack, flags, desc, no_crit = args
                    row_seed = seed
                    if step == 'burn' and vflags & 0x40:
                        row_seed, _d = lcg(seed)      # burn_range's draw, made before damage_apply
                    attacker_index = 0 if attacker_tag == 'v' else 1
                    entries = desc if desc is not None else [(-1, 0, 0, 0)] * 4
                    a_flags = {'fire': 0x420C, 'creature': 0x20, 'player': 0x60, 'self': 0x60}[attacker]
                    a_cls = 'Third' if attacker == 'fire' else 'Creature'
                    setup = [f'seed={row_seed}', f'cosmetic={rng.getrandbits(32)}', f'v.flags={vflags}',
                             f'v.res={res},0,0,0,0', f'v.armor={armor}', f'v.status={burning},0,0,0,0',
                             f'a.flags={a_flags}', f'a.cls={a_cls}', f'w.frame={frame}', f'w.pvp_a={pvp}',
                             'stats=0,25,30']
                    if not vflags & 0x60:
                        setup.append('v.cls=Third')
                    if frost:
                        setup.insert(0, 'spell=0:FrostArmor:0:7:1:0:0:0:0')
                    setup = ';'.join(setup)
                    params = [lo, hi, attack, flags, no_crit, attacker_index]
                    for e in entries:
                        params += list(e)
                    result, state = G.run(r, 'damage', params, setup)
                    rows.append('\t'.join(['damage', 'damage', f'{config}|' + ','.join(map(str, params)) + '|' + setup,
                                           result, state]))
                    key = f'{step}-{vname}-{attacker}'
                    counts[key] = counts.get(key, 0) + 1
    R.damage_apply = real_damage_apply
    body = ('\n'.join(rows) + '\n').encode('ascii')
    content = b'# fire_touch_vectors_v1\n' + body
    out = ROOT / 'native/tests/vectors/fire_touch.tsv.gz'
    blob = gzip.compress(content, mtime=0)
    out.write_bytes(blob)
    manifest = {
        'schema': 'fire-touch-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'harness_generator_sha256': hashlib.sha256((ROOT / 'tools/generate_combat_damage_vectors.py').read_bytes()).hexdigest(),
        'vector_sha256': hashlib.sha256(content).hexdigest(),
        'gzip_sha256': hashlib.sha256(blob).hexdigest(),
        'rows': len(rows),
        'cases': counts,
    }
    (ROOT / 'native/tests/vectors/fire_touch.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(len(rows), 'rows', json.dumps(counts))


if __name__ == '__main__':
    main()
