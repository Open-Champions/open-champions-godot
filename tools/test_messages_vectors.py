#!/usr/bin/env python3
"""Check complete synthetic message state against pinned Python-generated vectors."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def read_vectors(path, manifest_path, generator_path):
    manifest = json.loads(manifest_path.read_text())
    if manifest['schema'] != 'rta-messages-transport-v1':
        raise ValueError('unexpected messages schema')
    if hashlib.sha256(generator_path.read_bytes()).hexdigest() != manifest['generator_sha256']:
        raise ValueError('generator hash mismatch')
    plain = gzip.decompress(path.read_bytes())
    if hashlib.sha256(plain).hexdigest() != manifest['vectors_sha256']:
        raise ValueError('vector hash mismatch')
    rows = [line.split('\t') for line in plain.decode().splitlines()]
    if len(rows) != manifest['rows'] or not rows or any(len(row)!=3 for row in rows):
        raise ValueError('incomplete vectors')
    counts = {}
    for group,_,_ in rows:
        counts[group] = counts.get(group,0)+1
    if counts != manifest['groups']:
        raise ValueError('vector group mismatch')
    return rows


def compare(exe, rows):
    run = subprocess.run([str(exe)],input='\n'.join(row[1] for row in rows)+'\n',
                         text=True,capture_output=True,timeout=120)
    if run.returncode:
        raise ValueError('native trace failed: '+run.stderr[:1000])
    actual = run.stdout.splitlines()
    if len(actual) != len(rows):
        raise ValueError('native trace row count mismatch')
    for i, (row, observed) in enumerate(zip(rows, actual),1):
        if observed != row[2]:
            raise ValueError(f'case {i} ({row[0]}) differs\ninput: {row[1][:400]}\n'
                             f'expected: {row[2][:900]}\nactual: {observed[:900]}')


def integrity_checks(exe, rows, vectors, manifest, generator):
    # Verify corrupt/truncated fixtures and wrong traces cannot pass.
    with tempfile.TemporaryDirectory() as temp:
        bad = Path(temp)/'bad.gz'
        plain = gzip.decompress(vectors.read_bytes())
        bad.write_bytes(gzip.compress(plain[:-1],mtime=0))
        try:
            read_vectors(bad,manifest,generator)
        except ValueError:
            pass
        else:
            raise ValueError('truncated vector accepted')
    altered = [list(rows[0])]
    altered[0][2] += 'wrong'
    try:
        compare(exe,altered)
    except ValueError:
        pass
    else:
        raise ValueError('wrong expected state accepted')
    for script in ['\n','unknown\n','poke 0 3 1\n','send 0 f\n','encode 1 - 0\n']:
        run = subprocess.run([str(exe)],input=script,text=True,capture_output=True,timeout=10)
        if run.returncode == 0:
            raise ValueError('malformed transport accepted: '+repr(script))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native',type=Path,required=True)
    p.add_argument('--integrity',action='store_true')
    args = p.parse_args()
    root = Path(__file__).resolve().parent.parent
    vectors = root/'native/tests/vectors/messages.tsv.gz'
    manifest = root/'native/tests/vectors/messages.json'
    generator = root/'tools/generate_messages_vectors.py'
    rows = read_vectors(vectors,manifest,generator)
    if args.integrity:
        integrity_checks(args.native,rows,vectors,manifest,generator)
        print('messages fixture and transport integrity checks passed')
    else:
        compare(args.native,rows)
        print(f'{len(rows)} complete message states match the pinned reference')

if __name__ == '__main__':
    main()
