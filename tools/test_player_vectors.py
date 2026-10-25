#!/usr/bin/env python3
"""Compare complete native observations to independently pinned Python results."""
import argparse
from array import array
import gzip
import hashlib
import json
import pathlib
import subprocess
import tempfile
from generate_player_vectors import COMMIT, PINS

LEGACY_COUNT = 14448
LEGACY_SHA256 = "74fad4e6cf7e1e71472b3a0addd19c3f196701ce4fd3857293fab2a8ab5b82ac"
PREVIOUS_COUNT = 24384
PREVIOUS_SHA256 = "5d58e95c8b82432ac354c277cd592f0ed5ff980f9bc76382d19dd3a48fe99704"
CASTING_COUNT = 34128
CASTING_SHA256 = "7ab0008239aa8ec2d346fc8605d16f7e70e9edac5260cbe1b806ba50058ad0d6"
ACTION_COUNT = 59888
ACTION_SHA256 = "1c28ce889847e6504996f319f2626992c96cb77f894882fa0a8b79ed02d1e4d3"
NEUTRAL_COUNT = 87840
NEUTRAL_SHA256 = "1e5126fa7e007ce1e159fb96a44e650f87bfc25caef2440e072bb8cf167b6ee0"
MELEE_COUNT = 119888
MELEE_SHA256 = "fff6f0a283587380043b6b9229e76c242b4fb481e4336960ffe69389fc8b68d6"

RANGED_COUNT = 145840
RANGED_SHA256 = "cd3676b63e189e0995f95a2e4b4bcd4b9d3f749b05e848474f63d0f5a3cf948f"

LEGACYCAST_COUNT = 162912
LEGACYCAST_SHA256 = "ff58796f1f988c9c2f17d084c22ce4e14349fc40a481e307ff0d8ab8496b1ded"

PRESWITCH_COUNT = 177344
PRESWITCH_SHA256 = "cbab5e308a11d633743656df79a7acb0e759bdaf9d07c92be18e7635e4df3cc4"

TICK_COUNT = 218336
TICK_SHA256 = "ec38f0de6c2e42c46fae39b74c49b195a7491f1359e22d599229deca5b6dcb64"

UTILITY_COUNT = 231744
UTILITY_SHA256 = "8c9cec57f70df51086871514f26a06de032c5630436b1db82490f15c5adc3397"

MEMORY_COUNT = 248672
MEMORY_SHA256 = "d0d742a0e1d3acc00f31aa937098d7b765e9341bea227b7ecbe901d8904ca158"

EXPECTED_COUNT = 274304
EXPECTED_SHA256 = "b67371c3c22347948cf589d89a6bba66e66fb7ba39a57ef92d08c9bc62a67515"

DECODER = json.JSONDecoder()


def split_record(line):
    # The hash-pinned corpus is compact ASCII JSON. Decode only its input
    # fields, leaving the much larger expected observation unparsed. Offsets
    # in this ASCII text are also byte offsets in the original fixture.
    text = line.decode('ascii')
    assert text.startswith('[') and text.endswith(']\n'), 'record framing'
    label, at = DECODER.raw_decode(text, 1)
    assert text[at] == ',', 'label separator'
    config, at = DECODER.raw_decode(text, at + 1)
    assert text[at] == ',', 'configuration separator'
    start = at + 1
    _, end = DECODER.raw_decode(text, start)
    assert text[end] == ',', 'input separator'
    return label, config, text[start:end], end + 1


def compare_observations(actuals, vectors, offsets):
    observed = 0
    for i, line in enumerate(vectors):
        actual_line = actuals.readline()
        assert actual_line, 'missing observation'
        # Identical JSON bytes imply the original value equality. If the
        # formatting differs, retain that same semantic comparison and its
        # diagnostics rather than rejecting equivalent JSON representations.
        if actual_line.rstrip(b'\r\n') != line[offsets[i]:-2]:
            actual = json.loads(actual_line.decode('utf-8'))
            label, config, inputs, expected = json.loads(line)
            if actual != expected:
                raise AssertionError(f'row {i} {label} config={config} full state mismatch: ' +
                                     str(first_difference(actual, expected)))
        observed += 1
    assert not actuals.readline(), 'extra observation'
    return observed


def main():
    p=argparse.ArgumentParser();p.add_argument('--native',required=True);p.add_argument('--vectors',type=pathlib.Path,required=True)
    a=p.parse_args();manifest=json.loads(a.vectors.with_suffix('').with_suffix('.json').read_text())
    assert manifest['commit']==COMMIT and manifest['pins']==PINS,'evidence pins'
    assert manifest['count']==EXPECTED_COUNT and manifest['vector_sha256']==EXPECTED_SHA256,'frozen corpus'
    generator=pathlib.Path(__file__).with_name('generate_player_vectors.py')
    assert hashlib.sha256(generator.read_bytes()).hexdigest()==manifest['generator_sha256'],'generator hash'
    compressed=a.vectors.read_bytes()
    assert hashlib.sha256(compressed).hexdigest()==manifest['compressed_sha256'],'compressed hash'
    with tempfile.TemporaryDirectory(prefix='player-vectors-') as directory:
        input_path=pathlib.Path(directory)/'inputs';output_path=pathlib.Path(directory)/'actual'
        expected_offsets=array('I')
        memory_count=0;memory_digest=hashlib.sha256()
        utility_count=0;utility_digest=hashlib.sha256()
        tick_count=0;tick_digest=hashlib.sha256()
        preswitch_count=0;preswitch_digest=hashlib.sha256()
        legacycast_count=0;legacycast_digest=hashlib.sha256()
        count=expanded=legacy_count=previous_count=casting_count=action_count=neutral_count=melee_count=ranged_count=0;ranged_digest=hashlib.sha256();melee_digest=hashlib.sha256();neutral_digest=hashlib.sha256();action_digest=hashlib.sha256();casting_digest=hashlib.sha256();digest=hashlib.sha256();legacy_digest=hashlib.sha256();previous_digest=hashlib.sha256()
        with gzip.open(a.vectors,'rb') as vectors,input_path.open('w') as f:
            for line in vectors:
                digest.update(line);expanded+=len(line)
                label,config,inputs,offset=split_record(line)
                expected_offsets.append(offset)
                if not label.startswith('generic_'):
                    memory_digest.update(line);memory_count+=1
                if not label.startswith(('memory_', 'generic_')):
                    utility_digest.update(line);utility_count+=1
                if not label.startswith(('utility_', 'memory_', 'generic_')):
                    tick_digest.update(line);tick_count+=1
                if not label.startswith(('tick_', 'utility_', 'memory_', 'generic_')):
                    preswitch_digest.update(line);preswitch_count+=1
                if not label.startswith(('preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    legacycast_digest.update(line);legacycast_count+=1
                if not label.startswith(('legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    ranged_digest.update(line);ranged_count+=1
                if not label.startswith(('ranged_', 'legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    melee_digest.update(line);melee_count+=1
                if not label.startswith(('melee_', 'ranged_', 'legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    neutral_digest.update(line);neutral_count+=1
                if not label.startswith(('neutral_', 'melee_', 'ranged_', 'legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    action_digest.update(line);action_count+=1
                if not label.startswith(('action_', 'neutral_', 'melee_', 'ranged_', 'legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    casting_digest.update(line);casting_count+=1
                if not label.startswith(('casting_', 'action_', 'neutral_', 'melee_', 'ranged_', 'legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    previous_digest.update(line);previous_count+=1
                if not label.startswith(('recovery_', 'casting_', 'action_', 'neutral_', 'melee_', 'ranged_', 'legacycast_', 'preswitch_', 'tick_', 'utility_', 'memory_', 'generic_')):
                    legacy_digest.update(line);legacy_count+=1
                f.write(str(config)+';'+inputs+'\n')
                count+=1
        assert count==manifest['count'] and count,'row count'
        assert memory_count==MEMORY_COUNT and memory_digest.hexdigest()==MEMORY_SHA256,'unchanged Memory vectors'
        assert utility_count==UTILITY_COUNT and utility_digest.hexdigest()==UTILITY_SHA256,'unchanged scalar/string vectors'
        assert tick_count==TICK_COUNT and tick_digest.hexdigest()==TICK_SHA256,'unchanged full-tick vectors'
        assert preswitch_count==PRESWITCH_COUNT and preswitch_digest.hexdigest()==PRESWITCH_SHA256,'unchanged pre-switch vectors'
        assert legacy_count==LEGACY_COUNT and legacy_digest.hexdigest()==LEGACY_SHA256,'unchanged original vectors'
        assert previous_count==PREVIOUS_COUNT and previous_digest.hexdigest()==PREVIOUS_SHA256,'unchanged previous vectors'
        assert casting_count==CASTING_COUNT and casting_digest.hexdigest()==CASTING_SHA256,'unchanged casting vectors'
        assert action_count==ACTION_COUNT and action_digest.hexdigest()==ACTION_SHA256,'unchanged action vectors'
        assert melee_count==MELEE_COUNT and melee_digest.hexdigest()==MELEE_SHA256,'unchanged melee vectors'
        assert legacycast_count==LEGACYCAST_COUNT and legacycast_digest.hexdigest()==LEGACYCAST_SHA256,'unchanged casting/dispatch vectors'
        assert ranged_count==RANGED_COUNT and ranged_digest.hexdigest()==RANGED_SHA256,'unchanged ranged vectors'
        assert neutral_count==NEUTRAL_COUNT and neutral_digest.hexdigest()==NEUTRAL_SHA256,'unchanged neutral vectors'
        assert expanded==manifest['expanded_bytes'] and digest.hexdigest()==manifest['vector_sha256'],'vector hash'
        with input_path.open() as src,output_path.open('w') as dest:
            result=subprocess.run([a.native],stdin=src,stdout=dest,stderr=subprocess.PIPE,text=True,timeout=180)
        assert result.returncode==0,result.stderr
        with output_path.open('rb') as actuals,gzip.open(a.vectors,'rb') as vectors:
            observed=compare_observations(actuals,vectors,expected_offsets)
        assert observed==count,'missing observations'
    # Input transport is strict and generic, and expected columns aren't sent.
    for bad in ('16;[[],[],[]]\n','0;[[],[],[[99]]]\n','0;[[],[],[[0,1]]]\n','0;[[],[[95,0,[],0]],[]]\n','0;[[],[],[[8,4294967295,4,0]]]\n','00;[[],[],[]]\n','0;[[],[],[[13]]]\n','0;[[],[],[[14,65536,0,0,0]]]\n','0;[[],[],[[10,-1]]]\n','0;[[],[[30,[1,0,0],[],0]],[]]\n','0;[[],[[30,[3,[]],[],0]],[]]\n','0;[[],[],[[18,65536,2]]]\n','0;[[],[],[[16,65536,4294967295]]]\n','0;[[],[],[[20,10,0,0]]]\n','0;[[],[],[[21,12,0,0]]]\n','0;[[],[],[[20,0,0]]]\n','0;[[],[],[[22,4,0,0,0,0]]]\n','0;[[],[],[[22,0,0,0,0]]]\n','0;[[],[],[[22,0,0,0,0,-1]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        # Memory-span refusals are per-command observations; malformed transport
        # must terminate with nonzero. Both are explicit rejection states.
        assert result.returncode!=0 or json.loads(result.stdout)[1][0][0]==3,'accepted invalid transport'
    for bad in ('0;[[],[],[[23,3,0,0,0,0]]]\n', '0;[[],[],[[23,0,0,0,0]]]\n', '0;[[],[[68,[4,1],[],0]],[]]\n', '0;[[],[[68,[4,1,-1],[],0]],[]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid melee transport'
    for bad in ('0;[[],[],[[24,7,0,0,0,0]]]\n', '0;[[],[],[[24,4,0,2,0,0]]]\n', '0;[[],[],[[24,5,0,2,0,0]]]\n', '0;[[],[[30,[5,[0,0],[0,0,0]],[],0]],[]]\n', '0;[[],[],[[24,0,0,0,0]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid ranged transport'
    for bad in ('0;[[],[],[[25,6,0,0,0,0]]]\n', '0;[[],[],[[25,0,0,0,0]]]\n', '0;[[],[],[[25,2,0,0,65,0]]]\n', '0;[[],[],[[25,5,0,0,0,-1]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid casting transport'
    for bad in ('0;[[],[],[[26,0]]]\n', '0;[[],[],[[27]]]\n', '0;[[],[],[[28,0]]]\n', '0;[[],[],[[27,-1]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid pre-switch transport'
    for bad in ('0;[[],[],[[29]]]\n', '0;[[],[],[[29,-1]]]\n', '0;[[],[],[[30,0]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid full-tick transport'
    for bad in ('0;[[],[],[[31,8,0,0]]]\n', '0;[[],[],[[31,0,0]]]\n',
                '0;[[],[],[[31,1,0,1]]]\n', '0;[[],[],[[31,7,0,9223372036854775808]]]\n',
                '0;[[],[],[[31,4,0,-9223372036854775809]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid utility transport'
    for bad in ('0;[[],[],[[32,0]],[]]\n', '0;[[],[],[[33,0,[256]]],[]]\n',
                '0;[[],[],[],[256,0,[]]]\n', '0;[[],[],[],[0,0]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid memory transport'
    for bad in ('0;[[],[],[[34,[],[]]],[],[[],[[[],0,[],0],[[],1,[],0]]]]\n',
                '0;[[],[],[[34,[256],[]]]]\n', '0;[[],[],[[35,[],0,0,0]]]\n',
                '0;[[],[],[[36,[],[[],0,[],2],[]]]]\n', '0;[[],[],[[37]]]\n',
                '0;[[],[],[[39,-1]]]\n', '0;[[],[],[],[],[[],[],[]]]\n'):
        result=subprocess.run([a.native],input=bad,text=True,capture_output=True,timeout=10)
        assert result.returncode!=0,'accepted invalid callback transport'
    print(f'PASS player: {count} exact complete-state cases and transport refusals')

def first_difference(a,b,path=()):
    if type(a)!=type(b):return path,a,b
    if isinstance(a,list):
        if len(a)!=len(b):return path,'length',len(a),len(b)
        for i,(x,y) in enumerate(zip(a,b)):
            if x!=y:return first_difference(x,y,path+(i,))
    return path,a,b
if __name__=='__main__':main()
