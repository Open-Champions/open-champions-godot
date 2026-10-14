"""Atomic pair publication tests use tiny authored stand-ins, never native data."""
import gzip
import hashlib
import json
import pathlib
import tempfile
import types
import unittest
from unittest import mock
import generate_session_anim_cache_vectors as G

class CacheGeneration(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);self.root=pathlib.Path(temp.name)
        self.fixture=self.root/'cache.tsv.gz';self.manifest=self.root/'cache.json'
        self.fixture.write_bytes(b'old fixture');self.manifest.write_bytes(b'old manifest')
        for patch in (
            mock.patch.object(G,'load_helpers',return_value={'session_animation':types.SimpleNamespace(load_references=lambda _: {},sc=lambda _: '[7]')}),
            mock.patch.object(G,'anchors'),mock.patch.object(G,'recipe',return_value=[]),
            mock.patch.object(G,'run_case',return_value=[]),mock.patch.object(G,'SCENARIOS',('probe',)),
            mock.patch.object(G,'CONFIGURATIONS',(0,))):
            patch.start();self.addCleanup(patch.stop)
    def generate(self):return G.generate(self.root,self.fixture)
    def unchanged(self):
        self.assertEqual(self.fixture.read_bytes(),b'old fixture');self.assertEqual(self.manifest.read_bytes(),b'old manifest')
        self.assertEqual(set(self.root.iterdir()),{self.fixture,self.manifest})
    def test_interrupted_oracle(self):
        with mock.patch.object(G,'run_case',side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):self.generate()
        self.unchanged()
    def test_failed_sync_preserves_pair(self):
        for at in (1,2):
            count=0
            def fail(_):
                nonlocal count
                count+=1
                if count==at:raise OSError('sync')
            with mock.patch.object(G.os,'fsync',side_effect=fail):
                with self.assertRaises(OSError):self.generate()
            self.unchanged()
    def test_failed_fixture_publish_preserves_pair(self):
        with mock.patch.object(G.os,'replace',side_effect=OSError('publish')):
            with self.assertRaises(OSError):self.generate()
        self.unchanged()
    def test_manifest_last_failure_is_detectable(self):
        original=G.os.replace
        def fail(source,dest):
            if dest==self.manifest:raise OSError('manifest')
            original(source,dest)
        with mock.patch.object(G.os,'replace',side_effect=fail):
            with self.assertRaises(OSError):self.generate()
        self.assertNotEqual(self.fixture.read_bytes(),b'old fixture')
        self.assertEqual(self.manifest.read_bytes(),b'old manifest')
        self.assertEqual(set(self.root.iterdir()),{self.fixture,self.manifest})
    def test_stable_pair(self):
        first=self.generate();a=self.fixture.read_bytes();b=self.manifest.read_bytes()
        second=self.generate();self.assertEqual(first,second);self.assertEqual(a,self.fixture.read_bytes());self.assertEqual(b,self.manifest.read_bytes())
        self.assertEqual(hashlib.sha256(gzip.decompress(a)).hexdigest(),first['vector_sha256'])
    def test_divergent_replica_preserves_pair(self):
        helper=types.SimpleNamespace(load_references=lambda _: {},sc=lambda x: str(x))
        with mock.patch.object(G,'load_helpers',return_value={'session_animation':helper}),mock.patch.object(G,'run_case',side_effect=[[1],[2]]):
            with self.assertRaises(AssertionError):self.generate()
        self.unchanged()
if __name__=='__main__':unittest.main()
