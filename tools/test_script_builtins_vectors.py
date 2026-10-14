#!/usr/bin/env python3
"""Verify pinned fixtures and record the complete native trace, without research/game inputs."""
import argparse
import gzip
import hashlib
import json
import platform
import subprocess
from pathlib import Path
from generate_script_builtins_vectors import COMMIT, PINS, NATIVES, CAMPAIGN, PLAYERS, PORTED_HELPERS, SCENE, SCENE_UNITS, MENUS, MENU_UNITS, CREATURES, CREATURE_UNITS, ITEMS, ITEM_UNITS, EFFECTS, EFFECT_UNITS, VEHICLES, VEHICLE_UNITS, INTERACTIONS, INTERACTION_UNITS, UTILITIES, SCRIPT_CONTROL, SCRIPT_UNITS, MEMORY_UNITS, OBJECT_UNITS, HOST_UNITS, ARGUMENT_UNITS, SETUP_UNITS, DIVISION_UNITS


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--vectors', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--trace-dir', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--platform', default=platform.system() + '-' + platform.machine())
    args = parser.parse_args()
    args.trace_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.trace_dir / 'script-builtins.json'
    actual_path = args.trace_dir / 'script-builtins-actual.tsv'
    compressed_path = args.trace_dir / 'script-builtins.tsv.gz'
    for path in (report_path, actual_path, compressed_path):
        path.unlink(missing_ok=True)
    manifest = json.loads(args.manifest.read_text())
    if manifest['reference_commit'] != COMMIT or manifest['reference_files'] != PINS:
        raise ValueError('reference identity mismatch')
    if manifest['schema'] != 'rta-script-builtins-variables-v1':
        raise ValueError('fixture schema mismatch')
    if digest(Path(__file__).with_name('generate_script_builtins_vectors.py').read_bytes()) != manifest['generator_sha256']:
        raise ValueError('generator identity mismatch')
    ported = PORTED_HELPERS + ['Game.n_' + name for name in NATIVES + CAMPAIGN + PLAYERS] + SCENE_UNITS + MENU_UNITS + CREATURE_UNITS + ITEM_UNITS + EFFECT_UNITS + VEHICLE_UNITS + INTERACTION_UNITS + ['Game.n_' + name for name in UTILITIES] + SCRIPT_UNITS + MEMORY_UNITS + OBJECT_UNITS + HOST_UNITS + ARGUMENT_UNITS + SETUP_UNITS + DIVISION_UNITS
    units = manifest['reference_units']
    if (manifest['ported_units'] != ported or len(units) != len(set(units)) or
            not set(ported) <= set(units) or
            manifest['progress'] != {'ported': len(ported), 'total': len(units)}):
        raise ValueError('reference function accounting mismatch')
    if any(name not in manifest['operations'] for name in NATIVES + CAMPAIGN + PLAYERS + SCENE + MENUS + CREATURES + ITEMS + EFFECTS + VEHICLES + INTERACTIONS + UTILITIES + SCRIPT_CONTROL):
        raise ValueError('missing supported native coverage')
    plain = gzip.decompress(args.vectors.read_bytes())
    if digest(plain) != manifest['vectors_sha256']:
        raise ValueError('fixture digest mismatch')
    records = [line for line in plain.splitlines() if line and not line.startswith(b'#')]
    counts, errors = {}, {}
    for record in records:
        fields = record.decode('ascii').split('\t')
        if len(fields) != 4:
            raise ValueError('fixture record width')
        op, _, result, _ = fields
        counts[op] = counts.get(op, 0) + 1
        if result in ('HostFault', 'StackGarbage', 'CallbackFault', 'IndexError', 'ZeroDivisionError'):
            errors[result] = errors.get(result, 0) + 1
    if len(records) != manifest['rows'] or counts != manifest['operations'] or errors != manifest['errors']:
        raise ValueError('fixture coverage mismatch')
    plain_path = args.trace_dir / 'script-builtins-vectors.tsv'
    plain_path.write_bytes(plain)
    try:
        subprocess.run([str(args.native.resolve()), str(plain_path), str(actual_path)], check=True)
    finally:
        plain_path.unlink(missing_ok=True)  # owned decoded fixture, not the retained trace
    actual = actual_path.read_text(encoding='ascii').encode('ascii')
    expected = b'\n'.join(records) + b'\n'
    if actual != expected:
        raise ValueError('native complete trace differs from reference expectations')
    with compressed_path.open('wb') as output:
        with gzip.GzipFile(filename='', fileobj=output, mode='wb', mtime=0) as stream:
            stream.write(actual)
    report = {
        'schema': manifest['schema'], 'rows': len(records), 'operations': counts,
        'errors': errors, 'source_commit': args.source_commit,
        'platform': args.platform, 'host_platform': platform.system() + '-' + platform.machine(),
        'reference_commit': COMMIT, 'reference_files': PINS,
        'vectors_sha256': digest(plain), 'trace_sha256': digest(actual),
    }
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(f"{len(records)} exact transitions; trace SHA-256 {digest(actual)}")


if __name__ == '__main__':
    main()
