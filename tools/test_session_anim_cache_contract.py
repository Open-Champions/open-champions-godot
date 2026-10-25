"""Fail-closed provenance and complete-state omission probes, synthetic hosts only."""
import contextlib
import io
import copy
import gzip
import hashlib
import json
import pathlib
import re
import unittest
import ast
import test_compare_core_traces as artifacts
from compare_core_traces import PLATFORMS, validate
from simulation_contracts import CONTRACTS, SESSION_ANIM_CACHE_IDENTITY, SESSION_ANIMATION_REFERENCE


def parse(source):
    source = re.sub(r'(?<=[\[,])([bs][0-9a-f]*)(?=[,\]])', r"'\1'", source)
    return ast.literal_eval(source.replace('_', 'None'))


def encode(v):
    if v is None:return '_'
    if isinstance(v,list):return '['+','.join(map(encode,v))+']'
    return str(v)


def delta(a,b,path=()):
    if a==b:return []
    if isinstance(a,list) and isinstance(b,list) and len(a)==len(b):
        return [row for i,(x,y) in enumerate(zip(a,b)) for row in delta(x,y,path+(i,))]
    return [[list(path),copy.deepcopy(b)]]


def apply(a,patch):
    a=copy.deepcopy(a)
    for entry in patch:
        path=entry[0]
        value=entry[1]
        if len(entry)==3:
            target=a
            for i in path:target=target[i]
            data=bytearray.fromhex(target[1:]);payload=bytes.fromhex(entry[2][1:])
            data[entry[1]:entry[1]+len(payload)]=payload;value='b'+data.hex()
        if not path:a=copy.deepcopy(value);continue
        target=a
        for i in path[:-1]:target=target[i]
        target[path[-1]]=copy.deepcopy(value)
    return a

class SessionAnimCacheContract(unittest.TestCase):
    def setUp(self):
        f=artifacts.CoreArtifacts();f.setUp();self.addCleanup(f.doCleanups)
        self.manifests=f.manifests;self.traces=f.traces;self.path=self.manifests/'session_anim_cache.json'
        fixture=pathlib.Path(__file__).resolve().parents[1]/'native/tests/vectors/session_anim_cache.tsv.gz'
        with gzip.open(fixture,'rb') as stream:
            self.row=next(row.rstrip(b'\n') for row in stream if not row.startswith(b'#'))
        self.columns=self.row.decode().split('\t')
        self.original=parse(self.columns[4])
        self.assertEqual(encode(self.original),self.columns[4])
        vectors=b'# session_anim_cache_vectors_v1\n'+self.row+b'\n'
        (self.manifests/'session_anim_cache.tsv.gz').write_bytes(gzip.compress(vectors,mtime=0))
        m=json.loads(self.path.read_text());m['vector_sha256']=hashlib.sha256(vectors).hexdigest();self.manifest(m)
        for platform in PLATFORMS:
            path=self.traces/platform/'session-anim-cache.json';r=json.loads(path.read_text());r['vector_sha256']=m['vector_sha256'];path.write_text(json.dumps(r))
        self.actual(self.original)

    def manifest(self,m):
        self.path.write_text(json.dumps(m));digest=hashlib.sha256(self.path.read_bytes()).hexdigest()
        for platform in PLATFORMS:
            path=self.traces/platform/'session-anim-cache.json';r=json.loads(path.read_text());r['manifest_sha256']=digest;path.write_text(json.dumps(r))

    def actual(self,value):
        columns=self.columns.copy();columns[4]=encode(value);data=('\t'.join(columns)+'\n').encode()
        for platform in PLATFORMS:
            folder=self.traces/platform;(folder/'session-anim-cache.tsv.gz').write_bytes(gzip.compress(data,mtime=0))
            path=folder/'session-anim-cache.json';r=json.loads(path.read_text());r['trace_sha256']=hashlib.sha256(data).hexdigest();path.write_text(json.dumps(r))

    def validate(self):
        with contextlib.redirect_stdout(io.StringIO()):
            validate(self.traces,self.manifests,'commit')

    def test_registered_valid_full_observation(self):
        self.assertEqual(len(CONTRACTS),39)
        self.assertEqual(CONTRACTS['session-anim-cache'],('session_anim_cache',SESSION_ANIMATION_REFERENCE))
        self.validate()

    def test_every_provenance_and_protocol_binding(self):
        original=json.loads(self.path.read_text())
        for field in SESSION_ANIM_CACHE_IDENTITY:
            changed=copy.deepcopy(original);changed[field]={}
            with self.subTest(field=field):
                self.manifest(changed)
                with self.assertRaisesRegex(ValueError,'unsupported session-anim-cache'):self.validate()
        for key in original['references']:
            for field in ('commit','path','sha256'):
                changed=copy.deepcopy(original);changed['references'][key][field]='changed';self.manifest(changed)
                with self.assertRaisesRegex(ValueError,'unsupported session-anim-cache'):self.validate()
        for field in original['input_protocol']:
            changed=copy.deepcopy(original);del changed['input_protocol'][field];self.manifest(changed)
            with self.assertRaisesRegex(ValueError,'unsupported session-anim-cache'):self.validate()

    def test_all_hosts_omitting_initial_state_still_refused(self):
        paths=[[1,1,i] for i in range(10)]
        paths += [[1,1,0,i] for i in range(4)]
        paths += [[1,1,1,i] for i in range(3)]
        paths += [[1,1,1,0,i] for i in range(4)]
        paths += [[1,1,1,0,0,i] for i in range(6)]
        paths += [[1,1,1,1,0,1,i] for i in range(5)]
        paths += [[1,1,1,2,i] for i in range(6)]
        paths += [[1,1,1,2,2,0,i] for i in range(12)]
        paths += [[1,1,1,2,3,0,1,i] for i in range(9)]
        paths += [[1,1,1,2,4,0,i] for i in range(7)]
        paths += [[1,1,1,2,4,0,6,63,i] for i in range(16)]
        paths += [[1,1,2,0,i] for i in range(6)]
        for path in paths:
            with self.subTest(path=path):
                changed=copy.deepcopy(self.original);target=changed
                for i in path[:-1]:target=target[i]
                del target[path[-1]];self.actual(changed)
                with self.assertRaisesRegex(ValueError,'reference expectation'):self.validate()

    def test_complete_operations_decodes_and_failure_counter_fields(self):
        previous=self.original[1]
        for _,patch in self.original[2][:-1]:previous=apply(previous,patch)
        current=apply(previous,self.original[2][-1][1])
        paths=[[1,7,-1,i] for i in range(12)]
        paths += [[1,2,0,5,i] for i in range(4)]
        paths += [[1,2,0,5,3,0,i] for i in range(2)]
        paths += [[1,2,0,i] for i in range(6)]
        for path in paths:
            with self.subTest(path=path):
                changed_current=copy.deepcopy(current);target=changed_current
                for i in path[:-1]:target=target[i]
                del target[path[-1]]
                changed=copy.deepcopy(self.original);changed[2][-1][1]=delta(previous,changed_current)
                self.actual(changed)
                with self.assertRaisesRegex(ValueError,'reference expectation'):self.validate()

if __name__=='__main__':unittest.main()
