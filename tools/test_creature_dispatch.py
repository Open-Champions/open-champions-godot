#!/usr/bin/env python3
import argparse,gzip,hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',required=True);a=p.parse_args()
    payload=gzip.decompress((ROOT/'native/tests/vectors/creature_dispatch.json.gz').read_bytes());meta=json.loads((ROOT/'native/tests/vectors/creature_dispatch.json').read_text())
    assert hashlib.sha256(payload).hexdigest()==meta['payload_sha256']
    assert hashlib.sha256((ROOT/'native/src/creature_anim/dispatch_data.inc').read_bytes()).hexdigest()==meta['data_sha256']
    for p,digest in meta['generator_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest
    assert hashlib.sha256((ROOT/'native/tests/vectors/creature_dispatch_sources.json').read_bytes()).hexdigest()==meta['sources_sha256']
    cases=json.loads(payload)
    r=subprocess.run([a.exe],input='\n'.join(c['input'] for c in cases)+'\n',text=True,capture_output=True,check=True)
    lines=r.stdout.splitlines();assert len(lines)==len(cases),(len(lines),len(cases),r.stderr)
    for i,(c,got) in enumerate(zip(cases,lines)):
     if got.split()!=c['expected'].split():raise AssertionError((i,c['input'],c['expected'],got))
    print('PASS',len(cases),'exact dispatch cases')

if __name__ == "__main__":
    main()
