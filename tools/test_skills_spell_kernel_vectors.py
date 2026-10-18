#!/usr/bin/env python3
"""Check the separate exact spell corpus; historical fixtures are untouched."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
def digest(data): return hashlib.sha256(data).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--vectors',type=Path,default=ROOT/'native/tests/vectors/skills_spell_kernel.tsv.gz')
    args=parser.parse_args()
    manifest=json.loads(args.vectors.with_suffix('').with_suffix('.json').read_text())
    assert manifest['reference_commit']=='356add27d50e50972b2a671b31e8f4ec93e6778c'
    assert digest((ROOT/'tools/generate_skills_spell_kernel_vectors.py').read_bytes())==manifest['generator_sha256']
    assert digest(args.vectors.read_bytes())==manifest['compressed_sha256']
    plain=gzip.decompress(args.vectors.read_bytes())
    assert digest(plain)==manifest['vector_sha256']
    rows=[s.split('\t') for s in plain.decode('ascii').splitlines()]
    assert len(rows)==manifest['rows']
    with tempfile.TemporaryDirectory(prefix='skills-spell-kernel-') as folder:
        source=Path(folder)/'input';actual=Path(folder)/'actual'
        source.write_text('\n'.join(row[0] for row in rows)+'\n')
        subprocess.run([str(args.exe.resolve()),'--spell-trace',str(source),str(actual)],check=True)
        results=actual.read_text().splitlines()
        assert len(results)==len(rows)
        for i,(result,row) in enumerate(zip(results,rows)):
            if result!=row[1]: raise ValueError(f'row {i}: {row[0]}\nactual {result}\nexpect {row[1]}')
        print(f'{len(rows)} spell observations passed; trace SHA256 {digest(actual.read_bytes())}')
if __name__=='__main__': main()
