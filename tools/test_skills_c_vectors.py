#!/usr/bin/env python3
"""Verify complete native observations against immutable synthetic Python vectors."""
import argparse
import gzip
import hashlib
import json
import pathlib
import subprocess


# Independently retained coverage and data identity prevent a changed manifest
# from accepting a truncated or replaced expected-state corpus.
EXPECTED_ROWS = 41152
EXPECTED_SHA256 = '28eb229c855144e45f54ab154f269f70d136011e1b714e8a2f32cf35350f3c2c'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe',required=True,type=pathlib.Path)
    p.add_argument('--vectors',required=True,type=pathlib.Path)
    p.add_argument('--manifest',required=True,type=pathlib.Path)
    args=p.parse_args()
    pin=json.loads(args.manifest.read_text())
    root=pathlib.Path(__file__).resolve().parents[1]
    evidence=json.loads((root/'native/tests/vectors/skills_c_evidence.json').read_text())
    if pin['evidence']!=evidence or evidence['commit']!='356add27d50e50972b2a671b31e8f4ec93e6778c':
        raise ValueError('reference evidence mismatch')
    if pin['rows']!=EXPECTED_ROWS or pin['vector_sha256']!=EXPECTED_SHA256:
        raise ValueError('pinned corpus identity mismatch')
    if pin['generator_sha256']!=hashlib.sha256((root/'tools/generate_skills_c_vectors.py').read_bytes()).hexdigest():
        raise ValueError('generator identity mismatch')
    data=gzip.decompress(args.vectors.read_bytes())
    if hashlib.sha256(data).hexdigest()!=pin['vector_sha256']:raise ValueError('vector hash mismatch')
    lines=data.decode('ascii').splitlines()
    if lines[0]!='# skills_c_vectors_v1':raise ValueError('bad vector header')
    rows=[line.split('\t') for line in lines[1:]]
    if len(rows)!=pin['rows'] or any(len(row)!=3 for row in rows):raise ValueError('incomplete vectors')
    legacy = [line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 120]
    original = ('# skills_c_vectors_v1\n' + '\n'.join(legacy) + '\n').encode('ascii')
    if hashlib.sha256(original).hexdigest() != 'c75d1d4959637932f4073cb018b76b0641d54abe280a50b8ba4c440719c5a4cf':
        raise ValueError('original vector compatibility lost')
    previous = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 168) + '\n').encode('ascii')
    if hashlib.sha256(previous).hexdigest() != '2d2c32f668fe869b3db2a22511475b8c86dc20ae664d3b4f9c8a6c3fb7bbbb6a':
        raise ValueError('shared-runtime vector compatibility lost')
    protection = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 274) + '\n').encode('ascii')
    if hashlib.sha256(protection).hexdigest() != '4e674c59477c73c879536a39516ad8112ee27ed3cee592a915a9a8c1155dffd4':
        raise ValueError('protection vector compatibility lost')
    bloodlust = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 417) + '\n').encode('ascii')
    if hashlib.sha256(bloodlust).hexdigest() != '96fdf5384ea18d7d7b710c455c71a1860ce7cc23a15560960f540156406b29d3':
        raise ValueError('Blood Lust vector compatibility lost')
    shared_classes = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 504) + '\n').encode('ascii')
    if hashlib.sha256(shared_classes).hexdigest() != 'f63ff2c4925a8ce92b6cf3ced1fee9060ad3ee9fdabea8af72e75573152f41d3':
        raise ValueError('SharedClasses vector compatibility lost')
    summon_items = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 656) + '\n').encode('ascii')
    if hashlib.sha256(summon_items).hexdigest() != '391e7d5dd67673786bf9a090974c7a061f9aa2a0da7bb8f5149dcfdf66af7d2d':
        raise ValueError('SummonItems vector compatibility lost')
    critical = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 797) + '\n').encode('ascii')
    if hashlib.sha256(critical).hexdigest() != '4b98c7488aa3d6aabf73d553da55b6fc7d07c7cd6cbcdb6e777342fdb84c4d2e':
        raise ValueError('CriticalHit vector compatibility lost')
    thrown_axes = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 919) + '\n').encode('ascii')
    if hashlib.sha256(thrown_axes).hexdigest() != 'ba8736bceee122349fdc86a170e3c89dbcb4afbd414b35b4fbf5ebdd5077d5c8':
        raise ValueError('ThrownAxes vector compatibility lost')
    battlecry = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 1184) + '\n').encode('ascii')
    if hashlib.sha256(battlecry).hexdigest() != '703f2b69695f08368d5dea2e2c3d1c5bdb8344e79971dfddf28058208f423d18':
        raise ValueError('BattleCry vector compatibility lost')
    storms = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 1541) + '\n').encode('ascii')
    if hashlib.sha256(storms).hexdigest() != '404e5460b5cba1b05098883869f0ac515cae7213858a7614ac2b47602e407848':
        raise ValueError('Storms vector compatibility lost')
    diseaseblade = ('# skills_c_vectors_v1\n' + '\n'.join(
        line for i,line in enumerate(lines[1:]) if i % pin['scenario_count'] < 2089) + '\n').encode('ascii')
    if hashlib.sha256(diseaseblade).hexdigest() != 'f585dcfbb4baf2b472791af13200e8167df82c335d938272015d917de07ab526':
        raise ValueError('Disease Blade vector compatibility lost')
    submitted=''.join(row[0]+'\t'+row[1]+'\n' for row in rows)
    result=subprocess.run([str(args.exe.resolve())],input=submitted,text=True,capture_output=True,check=True,timeout=180)
    actual=result.stdout.splitlines()
    if len(actual)!=len(rows):raise ValueError('incomplete native trace')
    for i,(row,observed) in enumerate(zip(rows,actual)):
        if row[2]!=observed:
            a,b=row[2].split(),observed.split()
            at=next((k for k,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
            raise ValueError(f'row {i}, config {row[0]}, observation token {at}: expected {a[at:at+6]}, actual {b[at:at+6]}')
    # Input tampering must change the actual observation, since the runner never
    # receives expected state. Malformed and empty transport must fail.
    row=rows[0];tokens=row[1].split();tokens[5]=str(int(tokens[5])^1)
    changed=subprocess.run([str(args.exe.resolve())],input=row[0]+'\t'+' '.join(tokens)+'\n',text=True,capture_output=True)
    if changed.returncode==0 and changed.stdout.strip()==row[2]:raise ValueError('native ignored changed memory input')
    for malformed in ('','16\t'+row[1]+'\n', '0\t4 0\n',row[0]+'\t'+row[1]+' 7\n'):
        bad=subprocess.run([str(args.exe.resolve())],input=malformed,text=True,capture_output=True)
        if bad.returncode==0:raise ValueError('malformed transport accepted')
    print(f'skills-c: {len(rows)} complete-state vectors passed; transport checks passed')
    print('native observation SHA-256: '+hashlib.sha256(result.stdout.encode('ascii')).hexdigest())


if __name__=='__main__':main()
