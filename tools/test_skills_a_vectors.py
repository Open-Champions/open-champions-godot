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
    # Historical prefixes share the immutable plain corpus. Retaining joined
    # copies for every continuation can consume tens of GiB before native runs.
    # Scan line boundaries once and hash views, preserving each exact byte span.
    endings = [0]
    position = 0
    while position < len(plain):
        newline = plain.find(b'\n', position)
        position = len(plain) if newline < 0 else newline + 1
        endings.append(position)
    view = memoryview(plain)
    prefix_hashes = {}
    hasher = hashlib.sha256()
    position = 0
    for count in (15311, 28400, 36029, 44869, 49137, 56227, 64433, 68848, 75874, 90847, 97627, 99940, 102322, 105798, 109111, 116217, 120073, 126776, 133658, 140666, 143970, 145410):
        end = endings[min(count, len(endings)-1)]
        hasher.update(view[position:end])
        prefix_hashes[count] = hasher.hexdigest()
        position = end
    legacy_hash = '3dd4d5fd40bde30ae99a6795cf543aa76db3efb7db2ec6b3f654676a18ec6f1c'
    legacy=prefix_hashes[15311]
    if manifest['legacy_vector_count'] != 15310 or manifest['legacy_vector_sha256'] != legacy_hash or legacy != legacy_hash:
        raise ValueError('original Skill corpus changed')
    if manifest['families'] != ['shared-skill', 'weapon-buffs-12-14-26', 'undead-shields-15', 'root-critical-support-31-32-33', 'shield-bash-34', 'fire-storm-7', 'disease-trail-6', 'shock-of-frost-23', 'fire-flares-8', 'homing-projectiles-4-16', 'blinding-light-20', 'disease-shield-status-5-29', 'fire-weapons-cone-13-18', 'wizard-beam-1', 'blessed-10', 'offensive-undead-21-22', 'poison-rain-24', 'hammer-of-wrath-17', 'riposte-9-82', 'shared-can-use-mana-dispatch', 'summon-skeleton-11', 'item-image-rng-descriptor-adapters', 'model-generic-dispatch']:
        raise ValueError('family coverage mismatch')
    previous_hash = '7189fbbc67e939725e4a452e6f63654b7f4d339a109f84d1e376df2ead4ddec2'
    previous=prefix_hashes[28400]
    if manifest['previous_vector_count'] != 28399 or manifest['previous_vector_sha256'] != previous_hash or previous != previous_hash:
        raise ValueError('previous weapon-buff corpus changed')
    if manifest['reference_function_count'] != 196 or len(set(manifest['ported_functions'])) != 195:
        raise ValueError('reference function accounting mismatch')
    shield_hash = '94170ab12e4094409aceb36fae8aa244bf34ddc892efd29e925479f57c11ff3a'
    shield=prefix_hashes[36029]
    if manifest['shield_vector_count'] != 36028 or manifest['shield_vector_sha256'] != shield_hash or shield != shield_hash:
        raise ValueError('previous shield corpus changed')
    support_hash = 'ebaf48a42ea47ed6f037e41c5bb0afb988db2b0f0c1a02c0726c1ea59745a65c'
    support=prefix_hashes[44869]
    if manifest['support_vector_count'] != 44868 or manifest['support_vector_sha256'] != support_hash or support != support_hash:
        raise ValueError('previous support corpus changed')
    bash_hash = 'e01b19da02da2b8c9aa14908ef03c7daa7475538f49f5ccd5516bde61fb9e13d'
    bash=prefix_hashes[49137]
    if manifest['bash_vector_count'] != 49136 or manifest['bash_vector_sha256'] != bash_hash or bash != bash_hash:
        raise ValueError('previous Shield Bash corpus changed')
    storm_hash = 'edd6413ae031ec3d5e7f40f1a094ed139e253b44ab3f5bc994962efb81ac2fd0'
    storm=prefix_hashes[56227]
    if manifest['storm_vector_count'] != 56226 or manifest['storm_vector_sha256'] != storm_hash or storm != storm_hash:
        raise ValueError('previous Fire Storm corpus changed')
    trail_hash = '1d3bc3b81f8355d66b4c424e39b3997b49c5f0e711862d5cbf3912720fe8af07'
    trail=prefix_hashes[64433]
    if manifest['trail_vector_count'] != 64432 or manifest['trail_vector_sha256'] != trail_hash or trail != trail_hash:
        raise ValueError('previous Disease Trail corpus changed')
    frost_hash = '249b5ed5fbe45377b04d444506829a6192b82a22c9a46d7cc5e8c590b8f898cc'
    frost=prefix_hashes[68848]
    if manifest['frost_vector_count'] != 68847 or manifest['frost_vector_sha256'] != frost_hash or frost != frost_hash:
        raise ValueError('previous Frost corpus changed')
    flares_hash = 'fb9192cfa2cfbfc77f679a9dc65fa87c6b2956ffb823b285b0422c663f7285d3'
    flares=prefix_hashes[75874]
    if manifest['flares_vector_count'] != 75873 or manifest['flares_vector_sha256'] != flares_hash or flares != flares_hash:
        raise ValueError('previous Fire Flares corpus changed')
    homing_hash = '5e7d0d2fca9ac2eb845dcc656e39434977b0d58a808d93bde21629fa09baf51d'
    homing=prefix_hashes[90847]
    if manifest['homing_vector_count'] != 90846 or manifest['homing_vector_sha256'] != homing_hash or homing != homing_hash:
        raise ValueError('previous homing projectile corpus changed')
    blind_hash = '248ccb90d3eed7302a7acdedd3cc9ff2a51fd176ae16f25b37a4c7db116ab12b'
    blind=prefix_hashes[97627]
    if manifest['blind_vector_count'] != 97626 or manifest['blind_vector_sha256'] != blind_hash or blind != blind_hash:
        raise ValueError('previous Blinding Light corpus changed')
    disease_hash = 'e2fbadb97554a7c0195a2184b23b4b81ab0537c833583b6d7a73f3d95e25e7c4'
    disease=prefix_hashes[99940]
    if manifest['disease_vector_count'] != 99939 or manifest['disease_vector_sha256'] != disease_hash or disease != disease_hash:
        raise ValueError('previous Disease Shield corpus changed')
    fire_hash = '6ef047051d61fea147898038cd3d58db1ce53ad21b03eebeed001315c00f3019'
    fire=prefix_hashes[102322]
    if manifest['fire_vector_count'] != 102321 or manifest['fire_vector_sha256'] != fire_hash or fire != fire_hash:
        raise ValueError('previous Fire Weapons/Cone corpus changed')
    beam_hash = '05522993a7a9b52b174087335178c9b5734450636da63a686edee25aca340311'
    beam=prefix_hashes[105798]
    if manifest['beam_vector_count'] != 105797 or manifest['beam_vector_sha256'] != beam_hash or beam != beam_hash:
        raise ValueError('previous Wizard Beam corpus changed')
    blessed_hash = '90af1ec0bf135123768b61f1d0962a2a6209f413a77e3dca010c76309fd3efb4'
    blessed=prefix_hashes[109111]
    if manifest['blessed_vector_count'] != 109110 or manifest['blessed_vector_sha256'] != blessed_hash or blessed != blessed_hash:
        raise ValueError('previous Blessed corpus changed')
    undead_hash = 'ef937fd65f3d8200112f02a61b18e2009916effcff4bd8ef2f831246810948ed'
    undead=prefix_hashes[116217]
    if manifest['undead_vector_count'] != 116216 or manifest['undead_vector_sha256'] != undead_hash or undead != undead_hash:
        raise ValueError('previous offensive undead corpus changed')
    rain_hash='5f6cb7a0ebc536feca7f1e089655456e51f6892153fa3ef414b30504425b09f7'
    rain=prefix_hashes[120073]
    if manifest['rain_vector_count']!=120072 or manifest['rain_vector_sha256']!=rain_hash or rain!=rain_hash:
        raise ValueError('previous Poison Rain corpus changed')
    hammer_hash='99cf9f3ccaa8ce95d313cec6c7382f9194fcac75569ba498752677b8ab316576'
    hammer=prefix_hashes[126776]
    if manifest['hammer_vector_count']!=126775 or manifest['hammer_vector_sha256']!=hammer_hash or hammer!=hammer_hash:
        raise ValueError('previous Hammer corpus changed')
    old_hash='e72ca0521d268ad56fc6a699258a2cdaabae6742d7457ca7f60c8c01ef07ca57'
    old=prefix_hashes[133658]
    if manifest['dispatch_previous_vector_count']!=133657 or manifest['dispatch_previous_vector_sha256']!=old_hash or old!=old_hash:
        raise ValueError('previous Riposte corpus changed')
    summon_hash='ad32ad315f3daf02a77fe019856bcec7aa7c97989be82ca62d64463b086070e3'
    old=prefix_hashes[140666]
    if manifest['summon_previous_vector_count']!=140665 or manifest['summon_previous_vector_sha256']!=summon_hash or old!=summon_hash:
        raise ValueError('previous shared dispatch corpus changed')
    adapters_hash='ab44e6f3340ec0c21bfa4521730c7a183fb76c3131a3342c0129dcee5a3379cc'
    if (manifest['adapters_previous_vector_count']!=143969 or
        manifest['adapters_previous_vector_sha256']!=adapters_hash or prefix_hashes[143970]!=adapters_hash):
        raise ValueError('previous Summon Skeleton corpus changed')
    for method in ('descriptor','_ImageExe.__init__','_ImageExe.read','_ImageExe.cstr','_SharedDraws.__init__','_SharedDraws.next'):
        if method not in manifest['ported_functions']:
            raise ValueError('missing counted item adapter')
    for method in ('image_read','image_cstr','shared_draws','descriptor'):
        if manifest['operations'].get(method,0)<16:
            raise ValueError('missing item adapter coverage')
    if 'skelight_ctor' not in manifest['ported_functions']:
        raise ValueError('missing counted skeleton bridge')
    for method in ('summon_can_use','summon_cast','summon_spell_ctor','summon_spell_run','skelight_ctor','_summon_sample'):
        if manifest['operations'].get(method,0)<16:
            raise ValueError('missing Summon Skeleton family coverage')
    for method in ('carries_getter','can_use_slot','mana_cost','frame','is_player','first_skill_call'):
        if manifest['operations'].get(method,0)<16:
            raise ValueError('missing shared dispatch coverage')
    for method in ('riposte_counter_attack','lcg','unit','frac','round_half','spread_max','rem','quot','half_toward_zero'):
        if manifest['operations'].get(method,0)<16 or method not in manifest['ported_functions']:
            raise ValueError('missing Riposte family coverage')
    for method in ('item_tables','item_parse','item_resource_index','hammer_can_use','hammer_cast','hammer_ctor','hammer_spell_ctor','hammer_spell_run','_hammer_pull','_hammer_blow','hammer_run','seed3_draw'):
        if manifest['operations'].get(method,0)<16 or method not in manifest['ported_functions']:
            raise ValueError('missing Hammer family coverage')
    for method in ('poisonrain_can_use','poisonrain_cast','poisonrain_ctor','poisonrain_run'):
        if manifest['operations'].get(method,0) < 16 or method not in manifest['ported_functions']:
            raise ValueError('missing Poison Rain family coverage')
    for method in ('repulse_cast','damage_radial','dismissundead_cast','holybolt_ctor','holybolt_run','holybolt_hit'):
        if manifest['operations'].get(method,0) < 16 or method not in manifest['ported_functions']:
            raise ValueError('missing offensive undead family coverage')
    for method in ('blessed_can_use','blessed_cast','blessed_ctor','blessed_dtor','blessed_stat0','blessed_run'):
        if manifest['operations'].get(method,0) < 16 or method not in manifest['ported_functions']:
            raise ValueError('missing Blessed family coverage')
    for method in ('wizardbeam_cast','wizardbeam_ctor','wizardbeam_run','_beam_chain','_beam_flash'):
        if manifest['operations'].get(method,0) < 16 or method not in manifest['ported_functions']:
            raise ValueError('missing Wizard Beam family coverage')
    for family in ('fireweapons','coneoffire'):
        for method in ('can_use','cast','ctor','run'):
            if manifest['operations'].get(f'{family}_{method}',0) < 16:
                raise ValueError('missing fire lifecycle coverage')
    plumbing_hash='4b77d8478c3a800f44c93ff9db509c3a543cdca383e8736b3ac2dfc7f9c1c4d5'
    if manifest['plumbing_previous_vector_count']!=145409 or manifest['plumbing_previous_vector_sha256']!=plumbing_hash or prefix_hashes[145410]!=plumbing_hash:
        raise ValueError('previous adapter corpus changed')
    for op in ('model_init','named_call','generic_table','object_call','call','first_skill_call'):
        if manifest['operations'].get(op,0)<16: raise ValueError('missing generic dispatch coverage')
    def records():
        start = 0
        for end in endings[1:]:
            line = plain[start:end].decode('ascii').rstrip('\r\n')
            start = end
            if not line or line.startswith('#'):
                continue
            fields = line.split('\t')
            if len(fields) != 8:
                raise ValueError('vector field count')
            yield fields

    operations, case_count = {}, 0
    with tempfile.TemporaryDirectory(prefix='skills-a-') as tmp:
        source = Path(tmp) / 'input.tsv'
        with source.open('w', encoding='ascii') as stream:
            for fields in records():
                stream.write('\t'.join(fields[:6]) + '\n')
                operations[fields[0]] = operations.get(fields[0], 0) + 1
                case_count += 1
        if case_count != manifest['vector_count'] or operations != manifest['operations']:
            raise ValueError('coverage mismatch')
        subprocess.run([str(a.native.resolve()), str(source), str(actual.resolve())], check=True)
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
    required |= {'blindinglight_can_use', 'blindinglight_cast', 'blindinglight_effect_ctor',
                 'blindinglight_effect_run', 'blindinglight_spell_ctor', 'blindinglight_spell_run'}
    required |= {'diseaseshield_can_use', 'diseaseshield_cast', 'diseaseshield_ctor', 'diseaseshield_run', 'disease_spell_ctor', 'disease_spell_run'}
    if not required.issubset(operations):
        raise ValueError('missing weapon-buff operation coverage')
    digest = hashlib.sha256()
    with actual.open('r', encoding='ascii') as stream:
        for index, fields in enumerate(records()):
            got = stream.readline()
            if not got:
                raise ValueError('actual trace count mismatch')
            want = '\t'.join(fields[6:]) + '\n'
            if got != want:
                inputs = '\t'.join(fields[:6]) + '\n'
                raise ValueError(f'exact state mismatch at row {index+1}: {inputs[:160]}\nactual {got[:400]}\nexpected {want[:400]}')
            digest.update(got.encode('ascii'))
        if stream.readline():
            raise ValueError('actual trace count mismatch')
    digest = digest.hexdigest()
    report.write_text(json.dumps({'rows': case_count, 'trace_sha256': digest, 'reference_commit': COMMIT,
                                  'reference_sha256': PIN, 'vector_sha256': manifest['vector_sha256']}, indent=2) + '\n')
    print(f'{case_count} exact complete states PASS; SHA256 {digest}')


if __name__ == '__main__':
    main()
