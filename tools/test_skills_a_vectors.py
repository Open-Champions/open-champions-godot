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
    if manifest['families'] != ['shared-skill', 'weapon-buffs-12-14-26']:
        raise ValueError('family coverage mismatch')
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
    required = {'weaponbuff_can_use', 'weaponbuff_cast', 'weaponbuff_ctor', 'weaponbuff_run',
                'spell_call', 'spell_base', 'spell_rand', 'spell_type', 'weapons_of', 'enchant', 'add_spell'}
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
