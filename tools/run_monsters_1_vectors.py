#!/usr/bin/env python3
"""Compare complete input-driven native observations to the pinned reference."""
import argparse
import gzip
import hashlib
import json
import pathlib
import subprocess
import tempfile
from generate_monsters_1_vectors import COMMIT, PINS, NAMES

def run(exe,vectors,manifest,scratch):
    record=json.loads(manifest.read_text())
    if record['research_commit']!=COMMIT or record['references']!=PINS or record['callback_names']!=NAMES:
        raise ValueError('reference identity changed')
    if record['generator_sha256']!=hashlib.sha256(pathlib.Path(__file__).with_name('generate_monsters_1_vectors.py').read_bytes()).hexdigest():
        raise ValueError('generator identity changed')
    payload=gzip.decompress(vectors.read_bytes())
    if hashlib.sha256(payload).hexdigest()!=record['vector_sha256']:raise ValueError('fixture hash mismatch')
    lines=payload.decode('ascii').splitlines()
    if len(lines)!=record['rows']:raise ValueError('fixture coverage mismatch')
    scratch.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch,prefix='monsters-exact-') as tmp:
        input_path=pathlib.Path(tmp)/'input.tsv';output_path=pathlib.Path(tmp)/'actual.txt'
        # Strip the expected column entirely before native execution.
        input_path.write_text(''.join('\t'.join(line.split('\t')[:2])+'\t_\n' for line in lines))
        with output_path.open('w') as output:
            subprocess.run([str(exe.resolve()),str(input_path)],stdout=output,check=True,timeout=180)
        with output_path.open() as actual:
            for index,line in enumerate(lines):
                observed=actual.readline().rstrip('\r\n')
                expected=line.split('\t')[2]
                if observed!=expected:raise ValueError('complete state mismatch at row '+str(index))
            if actual.readline():raise ValueError('extra native records')
        digest=hashlib.sha256(output_path.read_bytes()).hexdigest()
    print(str(len(lines))+' complete monsters-1 observations match; trace SHA-256 '+digest)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','vectors','manifest','scratch'):p.add_argument('--'+name,type=pathlib.Path,required=True)
    a=p.parse_args();run(a.exe,a.vectors,a.manifest,a.scratch)
if __name__=='__main__':main()
