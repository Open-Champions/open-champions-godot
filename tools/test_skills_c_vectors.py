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
EXPECTED_ROWS = 2688
EXPECTED_SHA256 = '2d2c32f668fe869b3db2a22511475b8c86dc20ae664d3b4f9c8a6c3fb7bbbb6a'


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
    legacy = [line for i,line in enumerate(lines[1:]) if i % 168 < 120]
    original = ('# skills_c_vectors_v1\n' + '\n'.join(legacy) + '\n').encode('ascii')
    if hashlib.sha256(original).hexdigest() != 'c75d1d4959637932f4073cb018b76b0641d54abe280a50b8ba4c440719c5a4cf':
        raise ValueError('original vector compatibility lost')
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


if __name__=='__main__':main()
