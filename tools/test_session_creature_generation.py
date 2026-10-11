"""Publication failure tests use tiny authored rows, never original model runs."""
import gzip
import hashlib
import json
import pathlib
import tempfile
import types
import unittest
from unittest import mock

import generate_session_creature_vectors as generator


class AtomicCreatureGeneration(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="creature generation ")
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)
        self.fixture = self.root / "session_creature.tsv.gz"
        self.manifest = self.root / "session_creature.json"
        self.previous_fixture = gzip.compress(b"previous complete fixture\n", mtime=0)
        self.previous_manifest = b'{"previous":"complete manifest"}\n'
        self.fixture.write_bytes(self.previous_fixture)
        self.manifest.write_bytes(self.previous_manifest)
        patches = (
            mock.patch.object(generator.BASE, "load_references", return_value={}),
            mock.patch.object(generator.BASE, "load_helpers", return_value={
                "session_objects": types.SimpleNamespace(sc=lambda value: "[7]")}),
            mock.patch.object(generator, "run_scenario", return_value=[7]),
            mock.patch.object(generator, "check_case", return_value=None),
            mock.patch.object(generator, "SCENARIOS", ("probe",)),
            mock.patch.object(generator, "FPU_SCENARIOS", set()),
            mock.patch.object(generator, "VARIANTS", {"probe": (0,)}),
        )
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)
        self.replace = generator.os.replace

    def generate(self):
        return generator.generate(self.root / "unused-research", self.fixture)

    def assert_previous(self):
        self.assertEqual(self.fixture.read_bytes(), self.previous_fixture)
        self.assertEqual(self.manifest.read_bytes(), self.previous_manifest)
        self.assertEqual(set(self.root.iterdir()), {self.fixture, self.manifest})

    def test_interrupted_oracle_preserves_previous_pair(self):
        with mock.patch.object(generator, "run_scenario", side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                self.generate()
        self.assert_previous()

    def test_failed_gzip_write_preserves_previous_pair(self):
        original_write = generator.gzip.GzipFile.write
        writes = 0

        def failing_write(stream, data):
            nonlocal writes
            writes += 1
            if writes == 2:
                raise OSError("interrupted fixture write")
            return original_write(stream, data)

        with mock.patch.object(generator.gzip.GzipFile, "write", failing_write):
            with self.assertRaisesRegex(OSError, "fixture write"):
                self.generate()
        self.assertEqual(writes, 2)
        self.assert_previous()

    def test_failed_flush_before_either_replacement_preserves_pair(self):
        for fail_at in (1, 2):
            with self.subTest(fsync=fail_at):
                calls = 0

                def fail_sync(_descriptor):
                    nonlocal calls
                    calls += 1
                    if calls == fail_at:
                        raise OSError("disk sync failed")

                with mock.patch.object(generator.os, "fsync", side_effect=fail_sync):
                    with self.assertRaisesRegex(OSError, "disk sync"):
                        self.generate()
                self.assert_previous()

    def test_failed_fixture_replace_preserves_previous_pair(self):
        with mock.patch.object(generator.os, "replace", side_effect=OSError("replace failed")):
            with self.assertRaisesRegex(OSError, "replace failed"):
                self.generate()
        self.assert_previous()

    def test_failed_manifest_write_preserves_previous_pair(self):
        def fail_dump(value, stream, **_kwargs):
            stream.write('{"incomplete":')
            raise OSError("manifest write failed")

        with mock.patch.object(generator.json, "dump", side_effect=fail_dump):
            with self.assertRaisesRegex(OSError, "manifest write failed"):
                self.generate()
        self.assert_previous()

    def test_success_replaces_complete_fixture_then_complete_manifest(self):
        events = []

        def replace(source, target):
            source, target = pathlib.Path(source), pathlib.Path(target)
            self.assertEqual(source.parent, self.root)
            self.assertNotEqual(source, target)
            self.assertEqual(events[:2], ["fsync", "fsync"])
            if target == self.fixture:
                self.assertEqual(gzip.decompress(source.read_bytes()),
                                 b"# session_creature_vectors_v1\nprobe\t0\t0;0\tok\t[7]\n")
                self.assertEqual(self.manifest.read_bytes(), self.previous_manifest)
            else:
                data = json.loads(source.read_text())
                self.assertEqual(data["compressed_sha256"],
                                 hashlib.sha256(self.fixture.read_bytes()).hexdigest())
            events.append(target.name)
            self.replace(source, target)

        with mock.patch.object(generator.os, "fsync", side_effect=lambda _fd: events.append("fsync")), \
                mock.patch.object(generator.os, "replace", side_effect=replace):
            result = self.generate()
        self.assertEqual(events, ["fsync", "fsync", self.fixture.name, self.manifest.name])
        self.assertEqual(json.loads(self.manifest.read_text()), result)
        self.assertEqual(result["vector_count"], 1)
        self.assertEqual(set(self.root.iterdir()), {self.fixture, self.manifest})

    def test_second_replace_failure_leaves_complete_fixture_and_old_manifest(self):
        # A pair of files is not a filesystem transaction. The old manifest must
        # remain intact, so its hash disagreement exposes an interrupted publish.
        def replace(source, target):
            if pathlib.Path(target) == self.manifest:
                raise OSError("manifest replace failed")
            self.replace(source, target)

        with mock.patch.object(generator.os, "replace", side_effect=replace):
            with self.assertRaisesRegex(OSError, "manifest replace failed"):
                self.generate()
        self.assertEqual(gzip.decompress(self.fixture.read_bytes()),
                         b"# session_creature_vectors_v1\nprobe\t0\t0;0\tok\t[7]\n")
        self.assertEqual(self.manifest.read_bytes(), self.previous_manifest)
        self.assertEqual(set(self.root.iterdir()), {self.fixture, self.manifest})


if __name__ == "__main__":
    unittest.main()
