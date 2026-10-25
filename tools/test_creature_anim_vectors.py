#!/usr/bin/env python3
"""Exact observation comparison; uses committed synthetic fixtures, no research checkout."""
import argparse,gzip,hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',required=True);a=p.parse_args()
    base=ROOT/'native/tests/vectors';manifest=json.loads((base/'creature_anim.json').read_text())
    assert hashlib.sha256((base/'creature_anim_sources.json').read_bytes()).hexdigest()==manifest['source_manifest_sha256'],'source pin manifest hash'
    payload=gzip.decompress((base/'creature_anim.json.gz').read_bytes())
    assert hashlib.sha256(payload).hexdigest()==manifest['sha256'],'fixture hash'
    assert hashlib.sha256((ROOT/'tools/generate_creature_anim_vectors.py').read_bytes()).hexdigest()==manifest['generator_sha256'],'generator hash'
    cases=json.loads(payload);assert len(cases)==manifest['cases']
    for case in cases:
        result=subprocess.run([a.exe],input=case['input'],text=True,capture_output=True,timeout=120)
        if result.returncode:raise AssertionError(case['name']+': '+result.stderr)
        actual=result.stdout.split();expected=case['expected'].split()
        if actual!=expected:
            i=next((i for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),min(len(actual),len(expected)))
            raise AssertionError(f"{case['name']} component {i}: actual={actual[i:i+8]} expected={expected[i:i+8]}; lengths={len(actual)}/{len(expected)}")
    print(f'{len(cases)} exact full-state creature animation cases pass')
if __name__=='__main__':main()
