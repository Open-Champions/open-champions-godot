"""Integrity failures across synthetic platform artifacts, not actual CI proof."""
import ast
import copy
import gzip
import hashlib
import json
import pathlib
import unittest
import test_compare_core_traces as artifact_tests
from compare_core_traces import validate, PLATFORMS
from simulation_contracts import (CONTRACTS, CREATURE_TURN_REFERENCE,
    CREATURE_TURN_REFERENCES, CREATURE_TURN_SPECIFICATION, CREATURE_TURN_GENERATOR,
    CREATURE_TURN_PROTOCOL)

class TurnContract(unittest.TestCase):
    def setUp(self):
        self.fixture = artifact_tests.CoreArtifacts()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.manifests = self.fixture.manifests
        self.traces = self.fixture.traces
        self.manifest = self.manifests / 'creature_turn.json'
        # Use one complete original-Python observation for omission probes.
        source=pathlib.Path(__file__).resolve().parent.parent/'native/tests/vectors/creature_turn.tsv.gz'
        data=gzip.decompress(source.read_bytes())
        self.original_row=next(row for row in data.splitlines() if row and not row.startswith(b'#'))
        vector=b'# creature_turn_vectors_v1\n'+self.original_row+b'\n'
        (self.manifests/'creature_turn.tsv.gz').write_bytes(gzip.compress(vector,mtime=0))
        m=json.loads(self.manifest.read_text());m['vector_sha256']=hashlib.sha256(vector).hexdigest()
        self.manifest_value(m)
        for platform in PLATFORMS:
            folder=self.traces/platform
            actual=self.original_row+b'\n'
            (folder/'creature-turn.tsv.gz').write_bytes(gzip.compress(actual,mtime=0))
            path=folder/'creature-turn.json';r=json.loads(path.read_text())
            r['vector_sha256']=m['vector_sha256'];r['trace_sha256']=hashlib.sha256(actual).hexdigest()
            path.write_text(json.dumps(r))

    def check_all(self):
        validate(self.traces, self.manifests, 'commit')
    def manifest_value(self, value):
        self.manifest.write_text(json.dumps(value))
        digest = hashlib.sha256(self.manifest.read_bytes()).hexdigest()
        for platform in PLATFORMS:
            p = self.traces / platform / 'creature-turn.json'
            r = json.loads(p.read_text()); r['manifest_sha256'] = digest
            p.write_text(json.dumps(r))
    def test_registry_requires_36_suites_and_turn(self):
        self.assertEqual(len(CONTRACTS), 39)
        self.assertEqual(CONTRACTS['creature-turn'], ('creature_turn', CREATURE_TURN_REFERENCE))
        self.check_all()
    def test_missing_host_or_suite_refused(self):
        p = self.traces / sorted(PLATFORMS)[0] / 'creature-turn.json'
        p.unlink()
        with self.assertRaisesRegex(ValueError, 'missing required'): self.check_all()
    def test_reference_pin_changes_rejected_with_refreshed_reports(self):
        original = json.loads(self.manifest.read_text())
        for key in ('core', 'creature'):
            for field in ('commit', 'sha256', 'path'):
                with self.subTest(key=key,field=field):
                    changed = copy.deepcopy(original)
                    changed['references'][key][field] = 'changed'
                    self.manifest_value(changed)
                    with self.assertRaisesRegex(ValueError, 'unsupported creature-turn'): self.check_all()
    def test_specification_helpers_generator_policy_changes_refused(self):
        original = json.loads(self.manifest.read_text())
        changes = {'specification_evidence': {}, 'helpers': {'hidden': 'untracked'},
                   'generator_sha256': '0'*64, 'composition_policy': 'different-order'}
        for field,value in changes.items():
            with self.subTest(field=field):
                changed=copy.deepcopy(original);changed[field]=value;self.manifest_value(changed)
                with self.assertRaisesRegex(ValueError, 'unsupported creature-turn'): self.check_all()
    def test_protocol_default_or_observation_omission_refused(self):
        original=json.loads(self.manifest.read_text())
        for field in ('input','commands','snapshot','observation','defaults','configuration'):
            with self.subTest(field=field):
                changed=copy.deepcopy(original);changed['input_protocol'].pop(field)
                self.manifest_value(changed)
                with self.assertRaisesRegex(ValueError, 'unsupported creature-turn'): self.check_all()
    def test_all_hosts_agreeing_on_changed_state_refused(self):
        from generate_creature_turn_vectors import sc
        columns=self.original_row.split(b'\t');self.assertEqual(len(columns),5)
        def parse(text):
            index=0
            def value():
                nonlocal index
                if text[index]=='[':
                    index+=1;out=[]
                    if text[index]==']':index+=1;return out
                    while True:
                        out.append(value())
                        if text[index]==']':index+=1;return out
                        self.assertEqual(text[index],',');index+=1
                start=index
                while index<len(text) and text[index] not in ',]':index+=1
                atom=text[start:index]
                if atom=='_':return None
                if atom.startswith('s'):return bytes.fromhex(atom[1:]).decode('utf-8')
                if atom.startswith('b'):return bytes.fromhex(atom[1:])
                return int(atom)
            result=value();self.assertEqual(index,len(text));return result
        original=parse(columns[4].decode('ascii'))
        for field,replacement in ((0,[]),(1,[]),(2,[]),(4,[]),(5,1),(9,0),(10,1)):
            with self.subTest(snapshot_field=field):
                observation=copy.deepcopy(original)
                snapshot=observation[2][-1][2];self.assertEqual(len(snapshot),11)
                self.assertNotEqual(snapshot[field],replacement)
                snapshot[field]=replacement
                changed=columns[:];changed[4]=sc(observation).encode('ascii')
                actual=b'\t'.join(changed)+b'\n'
                for platform in PLATFORMS:
                    folder=self.traces/platform
                    (folder/'creature-turn.tsv.gz').write_bytes(gzip.compress(actual,mtime=0))
                    p=folder/'creature-turn.json';r=json.loads(p.read_text())
                    r['trace_sha256']=hashlib.sha256(actual).hexdigest();p.write_text(json.dumps(r))
                with self.assertRaisesRegex(ValueError, 'creature-turn:.*reference expectation'): self.check_all()
    def test_generator_source_and_literal_provenance_are_bound(self):
        p=pathlib.Path(__file__).with_name('generate_creature_turn_vectors.py')
        source=p.read_bytes();self.assertEqual(hashlib.sha256(source).hexdigest(),CREATURE_TURN_GENERATOR)
        values={}
        for node in ast.parse(source).body:
            if isinstance(node,ast.Assign) and len(node.targets)==1 and isinstance(node.targets[0],ast.Name):
                name=node.targets[0].id
                if name in ('REFERENCES','SPECIFICATION_EVIDENCE','INPUT_PROTOCOL'):
                    values[name]=ast.literal_eval(node.value)
        self.assertEqual(values['REFERENCES'],CREATURE_TURN_REFERENCES)
        self.assertEqual(values['SPECIFICATION_EVIDENCE'],CREATURE_TURN_SPECIFICATION)
        self.assertEqual(values['INPUT_PROTOCOL'],CREATURE_TURN_PROTOCOL)
    def test_whole_record_width_is_required(self):
        from core_trace_contract import expected_trace
        for row in (b'modes\t0\t0;[]\tok',b'modes\t0\t0;[]\tok\t[]\textra'):
            with self.assertRaisesRegex(ValueError,'malformed creature-turn'):
                expected_trace('creature-turn',[row])

if __name__ == '__main__': unittest.main()
