#!/usr/bin/env python3
"""Verify complete native observations against immutable synthetic Python vectors."""
import argparse
import gzip
import hashlib
import json
import pathlib
import subprocess
import tempfile
from itertools import zip_longest


# Independently retained coverage and data identity prevent a changed manifest
# from accepting a truncated or replaced expected-state corpus.
EXPECTED_ROWS = 149648
EXPECTED_SHA256 = '1881bfb823b26fa03e971f20a4698fe0f329c0fbdbac60a6db21c335de0aad46'


LEGACY_CORPORA = (
    (9032, '05460150d5c3d09771b6250a021ed464df494c6b3c56ff3850d911c9f9d2ff07'),
    (8486, '37d08ee71e99486fed0d02f13d82fb5e3bf5007b861fdac7c60be7a0ad4b2138'),
    (7900, 'f8b45b46f85c6fef684481d085199e21abf0c316ba4b2bf9eee511a5fab4d773'),
    (6430, '1a11411a2d52c108b3f6de566f33ce4ae2ea73a28aa3ccc047dc0e4f5a285d55'),
    (6357, '36a4a5b17b5c11637a59f7186f61592168a80e2d2135ab4ba1b89c38e77e96ec'),
    (120, 'c75d1d4959637932f4073cb018b76b0641d54abe280a50b8ba4c440719c5a4cf'),
    (168, '2d2c32f668fe869b3db2a22511475b8c86dc20ae664d3b4f9c8a6c3fb7bbbb6a'),
    (274, '4e674c59477c73c879536a39516ad8112ee27ed3cee592a915a9a8c1155dffd4'),
    (417, '96fdf5384ea18d7d7b710c455c71a1860ce7cc23a15560960f540156406b29d3'),
    (504, 'f63ff2c4925a8ce92b6cf3ced1fee9060ad3ee9fdabea8af72e75573152f41d3'),
    (656, '391e7d5dd67673786bf9a090974c7a061f9aa2a0da7bb8f5149dcfdf66af7d2d'),
    (797, '4b98c7488aa3d6aabf73d553da55b6fc7d07c7cd6cbcdb6e777342fdb84c4d2e'),
    (919, 'ba8736bceee122349fdc86a170e3c89dbcb4afbd414b35b4fbf5ebdd5077d5c8'),
    (1184, '703f2b69695f08368d5dea2e2c3d1c5bdb8344e79971dfddf28058208f423d18'),
    (1541, '404e5460b5cba1b05098883869f0ac515cae7213858a7614ac2b47602e407848'),
    (2089, 'f585dcfbb4baf2b472791af13200e8167df82c335d938272015d917de07ab526'),
    (2572, '28eb229c855144e45f54ab154f269f70d136011e1b714e8a2f32cf35350f3c2c'),
    (2877, '85a9fdafae504c7ccface4b1afa8552603a18e7e85728fc8f8f1f2b9fb1f08cd'),
    (3210, '311b3a1b1093aff89af2dac9552f38ff651fa486e405964c72a33d856c599e12'),
    (3769, '065b6686be3f44419fc539934b6d33eace1befdb3eeba929056004fd3376d0e8'),
    (4346, 'f3bfb9d8c7b277a1d43d16a00ae55aa90597279800be663396a249a844081d85'),
    (4908, '8d6fefbb859175c46299107623977e9f3ca9b96fe41a146ca319cb7aacfccf49'),
    (5377, '758278a0f77f8d4f177f31660831a6c198b9f35b026569c48fb6e3bcfca8df81'),
    (5824, '1be850f8b2fbcaa8c4c575cd7f8f4eb29ae1172da8d735771d1417e8b64b8844'),
)


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
    # Stream the corpus and all retained-prefix hashes. Holding a separate
    # multi-gigabyte byte string for each historical corpus needlessly retains
    # all those copies. Expected state never enters the runner.
    header=b'# skills_c_vectors_v1\n'
    whole=hashlib.sha256(header)
    legacy=[hashlib.sha256(header) for _ in LEGACY_CORPORA]
    first=None
    count=0
    (root/'work').mkdir(exist_ok=True)
    with tempfile.TemporaryFile(mode='w+t',encoding='ascii',newline='',dir=root/'work') as submitted, \
         tempfile.TemporaryFile(mode='w+t',encoding='ascii',newline=None,dir=root/'work') as observed:
        with gzip.open(args.vectors,'rb') as source:
            if source.readline()!=header:raise ValueError('bad vector header')
            for index,line in enumerate(source):
                whole.update(line)
                row=line.decode('ascii').rstrip('\n').split('\t')
                if len(row)!=3:raise ValueError('incomplete vectors')
                if first is None:first=row[:2]
                submitted.write(row[0]+'\t'+row[1]+'\n')
                for (limit,_),digest in zip(LEGACY_CORPORA,legacy):
                    if index % pin['scenario_count'] < limit:digest.update(line)
                count+=1
        if count!=pin['rows']:raise ValueError('incomplete vectors')
        if whole.hexdigest()!=pin['vector_sha256']:raise ValueError('vector hash mismatch')
        for (limit,expected),digest in zip(LEGACY_CORPORA,legacy):
            if digest.hexdigest()!=expected:raise ValueError(f'previous {limit}-scenario vector compatibility lost')
        submitted.seek(0)
        subprocess.run([str(args.exe.resolve())],stdin=submitted,stdout=observed,
                       stderr=subprocess.PIPE,text=True,check=True,timeout=1200)
        observed.seek(0)
        actual_hash=hashlib.sha256()
        with gzip.open(args.vectors,'rt',encoding='ascii',newline='') as source:
            next(source)
            for i,(line,actual) in enumerate(zip_longest(source,observed)):
                if line is None or actual is None:raise ValueError('incomplete native trace')
                actual_hash.update(actual.encode('ascii'))
                row=line.rstrip('\n').split('\t')
                if row[2]!=actual.rstrip('\n'):
                    a,b=row[2].split(),actual.split()
                    at=next((k for k,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
                    raise ValueError(f'row {i}, config {row[0]}, observation token {at}: expected {a[at:at+6]}, actual {b[at:at+6]}')
    # Input tampering must change actual state; malformed/empty transport fails.
    row=first;tokens=row[1].split();tokens[5]=str(int(tokens[5])^1)
    changed=subprocess.run([str(args.exe.resolve())],input=row[0]+'\t'+' '.join(tokens)+'\n',text=True,capture_output=True)
    with gzip.open(args.vectors,'rt',encoding='ascii') as source:
        next(source);expected_first=next(source).rstrip('\n').split('\t')[2]
    if changed.returncode==0 and changed.stdout.strip()==expected_first:raise ValueError('native ignored changed memory input')
    for malformed in ('','16\t'+row[1]+'\n', '0\t4 0\n',row[0]+'\t'+row[1]+' 7\n'):
        bad=subprocess.run([str(args.exe.resolve())],input=malformed,text=True,capture_output=True)
        if bad.returncode==0:raise ValueError('malformed transport accepted')
    print(f'skills-c: {count} complete-state vectors passed; transport checks passed')
    print('native observation SHA-256: '+actual_hash.hexdigest())


if __name__=='__main__':main()
