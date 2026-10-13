"""New-suite provenance and whole-state omission rejection; synthetic artifacts only."""
import ast
import copy
import gzip
import hashlib
import json
import pathlib
import unittest
import test_compare_core_traces as artifact_tests
from compare_core_traces import validate, PLATFORMS
from simulation_contracts import CONTRACTS, ANIMATION_RUNTIME_REFERENCE

class AnimationRuntimeContract(unittest.TestCase):
    def setUp(self):
        self.fixture = artifact_tests.CoreArtifacts()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.manifests = self.fixture.manifests
        self.traces = self.fixture.traces
        self.manifest = self.manifests / 'animation_runtime.json'
        source = pathlib.Path(__file__).resolve().parent.parent / 'native/tests/vectors/animation_runtime.tsv.gz'
        data = gzip.decompress(source.read_bytes())
        self.row = next(row for row in data.splitlines() if row and not row.startswith(b'#'))
        vector = b'# animation_runtime_vectors_v1\n' + self.row + b'\n'
        (self.manifests / 'animation_runtime.tsv.gz').write_bytes(gzip.compress(vector, mtime=0))
        m = json.loads(self.manifest.read_text())
        m['vector_sha256'] = hashlib.sha256(vector).hexdigest()
        self.set_manifest(m)
        for platform in PLATFORMS:
            path = self.traces / platform / 'animation-runtime.json'
            r = json.loads(path.read_text())
            r['vector_sha256'] = m['vector_sha256']
            path.write_text(json.dumps(r))
        self.set_actual(self.row)

    def set_manifest(self, value):
        self.manifest.write_text(json.dumps(value))
        digest = hashlib.sha256(self.manifest.read_bytes()).hexdigest()
        for platform in PLATFORMS:
            path = self.traces / platform / 'animation-runtime.json'
            r = json.loads(path.read_text())
            r['manifest_sha256'] = digest
            path.write_text(json.dumps(r))

    def set_actual(self, row):
        data = row + b'\n'
        for platform in PLATFORMS:
            folder = self.traces / platform
            (folder / 'animation-runtime.tsv.gz').write_bytes(gzip.compress(data, mtime=0))
            path = folder / 'animation-runtime.json'
            r = json.loads(path.read_text())
            r['trace_sha256'] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(r))

    def check_all(self):
        validate(self.traces, self.manifests, 'commit')

    def test_registry_and_valid_whole_observation(self):
        self.assertEqual(len(CONTRACTS), 38)
        self.assertEqual(CONTRACTS['animation-runtime'], ('animation_runtime', ANIMATION_RUNTIME_REFERENCE))
        self.check_all()

    def test_every_reference_pin_is_bound(self):
        original = json.loads(self.manifest.read_text())
        for model in ('anim', 'core'):
            for field in ('commit', 'path', 'sha256'):
                with self.subTest(model=model, field=field):
                    changed = copy.deepcopy(original)
                    changed['references'][model][field] = 'changed'
                    self.set_manifest(changed)
                    with self.assertRaisesRegex(ValueError, 'unsupported animation-runtime'):
                        self.check_all()

    def test_policy_spec_helper_generator_and_every_protocol_field_are_bound(self):
        original = json.loads(self.manifest.read_text())
        for field in ('composition_policy', 'specification_evidence', 'helpers', 'generator_sha256'):
            with self.subTest(field=field):
                changed = copy.deepcopy(original)
                changed[field] = 'changed'
                self.set_manifest(changed)
                with self.assertRaisesRegex(ValueError, 'unsupported animation-runtime'):
                    self.check_all()
        for field in original['input_protocol']:
            with self.subTest(protocol=field):
                changed = copy.deepcopy(original)
                del changed['input_protocol'][field]
                self.set_manifest(changed)
                with self.assertRaisesRegex(ValueError, 'unsupported animation-runtime'):
                    self.check_all()

    def test_unanimous_refreshed_artifact_omissions_still_refused(self):
        columns = self.row.decode('ascii').split('\t')
        original = ast.literal_eval(columns[4].replace('_', 'None'))
        # Every top-level field, lifecycle limit, state scalar/membership,
        # decoder authority, clip/track/slot field and all sixteen joint cells.
        paths = [[1, i] for i in range(3)]
        paths += [[1, 0, i] for i in range(4)]
        paths += [[1, 1, 0, 1, i] for i in range(5)]
        paths += [[1, 2, i] for i in range(6)]
        paths += [[1, 2, 2, 0, i] for i in range(12)]
        paths += [[1, 2, 3, 0, 1, i] for i in range(9)]
        paths += [[1, 2, 4, 0, i] for i in range(7)]
        paths += [[1, 2, 4, 0, 6, 63, i] for i in range(16)]
        for path in paths:
            with self.subTest(path=path):
                changed = copy.deepcopy(original)
                parent = changed
                for index in path[:-1]:
                    parent = parent[index]
                del parent[path[-1]]
                cols = list(columns)
                cols[4] = repr(changed).replace(' ', '').replace('None', '_')
                self.set_actual('\t'.join(cols).encode('ascii'))
                with self.assertRaisesRegex(ValueError, 'reference expectation'):
                    self.check_all()

if __name__ == '__main__':
    unittest.main()
