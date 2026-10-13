#!/usr/bin/env python3
"""Compare every native state with Python expectations; fail closed on stale fixtures."""
import argparse
import gzip
import hashlib
import json
import pathlib
import subprocess
import tempfile


def load_vectors(path):
    manifest = json.loads(path.with_suffix('').with_suffix('.json').read_text())
    data = gzip.decompress(path.read_bytes())
    if hashlib.sha256(data).hexdigest() != manifest['vectors_sha256']:
        raise ValueError('vector hash mismatch')
    generator = pathlib.Path(__file__).with_name('generate_skills_b_vectors.py')
    if hashlib.sha256(generator.read_bytes()).hexdigest() != manifest['generator_sha256']:
        raise ValueError('generator hash mismatch')
    rows = data.decode('ascii').splitlines()
    if len(rows)!=manifest['rows'] or not rows: raise ValueError('vector count mismatch')
    inputs,expected=[],[]
    for row in rows:
        fields=row.split('\t')
        if len(fields)!=5: raise ValueError('vector field count')
        inputs.append('\t'.join(fields[:4]))
        expected.append(fields[4])
    return inputs,expected


def execute(exe, inputs, directory):
    source,target=directory/'input.tsv',directory/'actual.tsv'
    source.write_text('\n'.join(inputs)+'\n')
    target.unlink(missing_ok=True)
    subprocess.run([str(exe),str(source),str(target)],check=True,timeout=120)
    actual=target.read_text().splitlines()
    if len(actual)!=len(inputs): raise ValueError('incomplete native output')
    return actual


def compare(actual,expected):
    if len(actual)!=len(expected): raise ValueError('state count mismatch')
    for i,(a,b) in enumerate(zip(actual,expected)):
        if a!=b:
            # Full reports remain in the caller-selected scratch directory.
            raise ValueError('complete-state mismatch at row '+str(i))


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--exe',type=pathlib.Path,required=True)
    p.add_argument('--vectors',type=pathlib.Path,required=True)
    p.add_argument('--transport',action='store_true')
    args=p.parse_args()
    inputs,expected=load_vectors(args.vectors)
    with tempfile.TemporaryDirectory(prefix='skills-b-exact-') as d:
        directory=pathlib.Path(d)
        if args.transport:
            baseline=execute(args.exe,inputs[:1],directory)
            compare(baseline,expected[:1])
            mutated=inputs[0].replace('shared;', 'shared;shared;',1)
            if mutated==inputs[0]: raise ValueError('mutation did not change input')
            different=execute(args.exe,[mutated],directory)
            if different==baseline: raise ValueError('native runner ignored changed commands')
            for changed in ([expected[0]+'x'],[]):
                try: compare(baseline,changed)
                except ValueError: pass
                else: raise ValueError('wrong/truncated expectations accepted')
            for data in (args.vectors.read_bytes()[:-8], gzip.compress(b'0\t\t\tshared\twrong\n')):
                bad=directory/'bad.tsv.gz'
                bad.write_bytes(data)
                bad.with_suffix('').with_suffix('.json').write_bytes(args.vectors.with_suffix('').with_suffix('.json').read_bytes())
                try: load_vectors(bad)
                except (ValueError,EOFError,OSError): pass
                else: raise ValueError('corrupt fixture accepted')
            print('skills-b transport checks passed')
        else:
            actual=execute(args.exe,inputs,directory)
            compare(actual,expected)
            print(len(actual),'complete-state cases passed; trace SHA-256',
                  hashlib.sha256(('\n'.join(actual)+'\n').encode()).hexdigest())
if __name__=='__main__': main()
