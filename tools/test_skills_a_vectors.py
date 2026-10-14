#!/usr/bin/env python3
"""Check complete Python state against input-only native execution; no research needed."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

PIN = '6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae'
COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native', type=Path, required=True)
    p.add_argument('--vectors', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--trace-dir', type=Path, required=True)
    a = p.parse_args()
    a.trace_dir.mkdir(parents=True, exist_ok=True)
    actual = a.trace_dir / 'skills_a_actual.tsv'
    report = a.trace_dir / 'skills_a_report.json'
    actual.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    manifest = json.loads(a.manifest.read_text())
    if manifest['schema'] != 'rta-skills-a-shared-v1' or manifest['reference_commit'] != COMMIT or manifest['reference_sha256'] != PIN:
        raise ValueError('reference identity mismatch')
    plain = gzip.decompress(a.vectors.read_bytes())
    if hashlib.sha256(plain).hexdigest() != manifest['vector_sha256']:
        raise ValueError('vector SHA mismatch')
    legacy_hash = '3dd4d5fd40bde30ae99a6795cf543aa76db3efb7db2ec6b3f654676a18ec6f1c'
    legacy = b''.join(plain.splitlines(keepends=True)[:15311])
    if manifest['legacy_vector_count'] != 15310 or manifest['legacy_vector_sha256'] != legacy_hash or hashlib.sha256(legacy).hexdigest() != legacy_hash:
        raise ValueError('original Skill corpus changed')
    if manifest['families'] != ['shared-skill', 'weapon-buffs-12-14-26', 'undead-shields-15', 'root-critical-support-31-32-33', 'shield-bash-34', 'fire-storm-7', 'disease-trail-6', 'shock-of-frost-23', 'fire-flares-8', 'homing-projectiles-4-16']:
        raise ValueError('family coverage mismatch')
    previous_hash = '7189fbbc67e939725e4a452e6f63654b7f4d339a109f84d1e376df2ead4ddec2'
    previous = b''.join(plain.splitlines(keepends=True)[:28400])
    if manifest['previous_vector_count'] != 28399 or manifest['previous_vector_sha256'] != previous_hash or hashlib.sha256(previous).hexdigest() != previous_hash:
        raise ValueError('previous weapon-buff corpus changed')
    if manifest['reference_function_count'] != 196 or len(set(manifest['ported_functions'])) != 115:
        raise ValueError('reference function accounting mismatch')
    shield_hash = '94170ab12e4094409aceb36fae8aa244bf34ddc892efd29e925479f57c11ff3a'
    shield = b''.join(plain.splitlines(keepends=True)[:36029])
    if manifest['shield_vector_count'] != 36028 or manifest['shield_vector_sha256'] != shield_hash or hashlib.sha256(shield).hexdigest() != shield_hash:
        raise ValueError('previous shield corpus changed')
    support_hash = 'ebaf48a42ea47ed6f037e41c5bb0afb988db2b0f0c1a02c0726c1ea59745a65c'
    support = b''.join(plain.splitlines(keepends=True)[:44869])
    if manifest['support_vector_count'] != 44868 or manifest['support_vector_sha256'] != support_hash or hashlib.sha256(support).hexdigest() != support_hash:
        raise ValueError('previous support corpus changed')
    bash_hash = 'e01b19da02da2b8c9aa14908ef03c7daa7475538f49f5ccd5516bde61fb9e13d'
    bash = b''.join(plain.splitlines(keepends=True)[:49137])
    if manifest['bash_vector_count'] != 49136 or manifest['bash_vector_sha256'] != bash_hash or hashlib.sha256(bash).hexdigest() != bash_hash:
        raise ValueError('previous Shield Bash corpus changed')
    storm_hash = 'edd6413ae031ec3d5e7f40f1a094ed139e253b44ab3f5bc994962efb81ac2fd0'
    storm = b''.join(plain.splitlines(keepends=True)[:56227])
    if manifest['storm_vector_count'] != 56226 or manifest['storm_vector_sha256'] != storm_hash or hashlib.sha256(storm).hexdigest() != storm_hash:
        raise ValueError('previous Fire Storm corpus changed')
    trail_hash = '1d3bc3b81f8355d66b4c424e39b3997b49c5f0e711862d5cbf3912720fe8af07'
    trail = b''.join(plain.splitlines(keepends=True)[:64433])
    if manifest['trail_vector_count'] != 64432 or manifest['trail_vector_sha256'] != trail_hash or hashlib.sha256(trail).hexdigest() != trail_hash:
        raise ValueError('previous Disease Trail corpus changed')
    frost_hash = '249b5ed5fbe45377b04d444506829a6192b82a22c9a46d7cc5e8c590b8f898cc'
    frost = b''.join(plain.splitlines(keepends=True)[:68848])
    if manifest['frost_vector_count'] != 68847 or manifest['frost_vector_sha256'] != frost_hash or hashlib.sha256(frost).hexdigest() != frost_hash:
        raise ValueError('previous Frost corpus changed')
    flares_hash = 'fb9192cfa2cfbfc77f679a9dc65fa87c6b2956ffb823b285b0422c663f7285d3'
    flares = b''.join(plain.splitlines(keepends=True)[:75874])
    if manifest['flares_vector_count'] != 75873 or manifest['flares_vector_sha256'] != flares_hash or hashlib.sha256(flares).hexdigest() != flares_hash:
        raise ValueError('previous Fire Flares corpus changed')
    inputs, expected, operations = [], [], {}
    for line in plain.decode('ascii').splitlines():
        if not line or line.startswith('#'):
            continue
        fields = line.split('\t')
        if len(fields) != 8:
            raise ValueError('vector field count')
        inputs.append('\t'.join(fields[:6]) + '\n')
        expected.append('\t'.join(fields[6:]) + '\n')
        operations[fields[0]] = operations.get(fields[0], 0) + 1
    if len(expected) != manifest['vector_count'] or operations != manifest['operations']:
        raise ValueError('coverage mismatch')
    required = {'firestorm_can_use', 'firestorm_cast', '_firestorm_base', 'firestorm_ctor', 'firestorm_ctor2', 'firestorm_init_common', 'firestorm_run', 'obj_ball', 'delete_self', 'gameobject_ctor', 'object_call', 'shieldbash_can_use', 'shieldbash_cast', 'shieldbash_ctor', 'shieldbash_windup_fx', 'shieldbash_run', 'shieldbash_done', 'light_effect', 'weaponbuff_can_use', 'weaponbuff_cast', 'weaponbuff_ctor', 'weaponbuff_run',
                'undeadshield_can_use', 'undeadshield_cast', 'undeadshield_ctor', 'undeadshield_run',
                'undeadshield_damage_attacker', 'secondary_shield_ctor', 'secondary_shield_run',
                'secondary_shield_damage_attacker', '_shield_glow', '_shield_reflect', 'spell_ball',
                'player_object', 'root_can_use', 'root_cast', 'root_spell_ctor', 'root_spell_run', 'crit_can_use', 'crit_cast', 'crit_spell_ctor', 'crit_set_tint', 'crit_spell_run', 'crit_bonus_for',
                'spell_call', 'spell_base', 'spell_rand', 'spell_type', 'weapons_of', 'enchant', 'add_spell'}
    required |= {'diseasetrail_can_use', 'diseasetrail_cast', 'diseasetrail_spell_ctor', 'diseasetrail_spell_run',
                 'diseasetrail_ctor', 'diseasetrail_run', '_trail_puffs', 'flash2', 'box_around'}
    required |= {'move_and_collide', 'shockoffrost_run', '_frost_hit', 'frostshard_run', 'shockoffrost_can_use', '_frost_debris', 'projectile_ctor', 'shockoffrost_hit', 'frostshard_ctor', 'shockoffrost_cast', 'puff', 'frostshard_hit', 'shockoffrost_ctor'}
    required |= {'fireflares_can_use', 'fireflares_cast', 'fireflares_ctor', 'fireflares_dtor', 'fireflares_run', 'fireflare_ctor', 'fireflare_run', 'spell_dtor_call'}
    required |= {family+suffix for family in ('diseasebolt', 'holystrike')
                 for suffix in ('_can_use', '_cast', '_ctor', '_run', '_hit')}
    if not required.issubset(operations):
        raise ValueError('missing weapon-buff operation coverage')
    with tempfile.TemporaryDirectory(prefix='skills-a-') as tmp:
        source = Path(tmp) / 'input.tsv'
        source.write_text(''.join(inputs), encoding='ascii')
        subprocess.run([str(a.native.resolve()), str(source), str(actual.resolve())], check=True)
    lines = actual.read_text(encoding='ascii').splitlines(keepends=True)
    if len(lines) != len(expected):
        raise ValueError('actual trace count mismatch')
    for index, (got, want) in enumerate(zip(lines, expected)):
        if got != want:
            raise ValueError(f'exact state mismatch at row {index+1}: {inputs[index][:160]}\nactual {got[:400]}\nexpected {want[:400]}')
    digest = hashlib.sha256(''.join(lines).encode('ascii')).hexdigest()
    report.write_text(json.dumps({'rows': len(lines), 'trace_sha256': digest, 'reference_commit': COMMIT,
                                  'reference_sha256': PIN, 'vector_sha256': manifest['vector_sha256']}, indent=2) + '\n')
    print(f'{len(lines)} exact complete states PASS; SHA256 {digest}')


if __name__ == '__main__':
    main()
