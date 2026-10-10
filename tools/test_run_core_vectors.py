import gzip
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch
import run_core_vectors


class FreshEvidence(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        vectors = b"one vector\n"
        (self.root / "data.gz").write_bytes(gzip.compress(vectors, mtime=0))
        manifest = {"reference_sha256": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
                    "vector_sha256": hashlib.sha256(vectors).hexdigest(), "vector_count": 1}
        (self.root / "manifest.json").write_text(json.dumps(manifest))
        (self.root / "arithmetic").mkdir()
        for name in ("arithmetic/actual.tsv", "arithmetic.json", "arithmetic.tsv.gz"):
            (self.root / name).write_bytes(b"stale success")
        argv = ["test", "--suite", "arithmetic", "--exe", sys.executable,
                "--vectors", str(self.root / "data.gz"), "--manifest", str(self.root / "manifest.json"),
                "--trace-dir", str(self.root), "--platform", "test", "--source-commit", "test"]
        self.patch = patch.object(sys, "argv", argv)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def test_success_without_new_trace_cannot_reuse_old_trace(self):
        with patch.object(run_core_vectors.subprocess, "run"):
            with self.assertRaises(FileNotFoundError):
                run_core_vectors.main()
        self.assertFalse((self.root / "arithmetic.json").exists())

    def test_bad_fixture_removes_old_success(self):
        (self.root / "data.gz").write_bytes(gzip.compress(b"changed\n", mtime=0))
        with self.assertRaisesRegex(ValueError, "manifest"):
            run_core_vectors.main()
        self.assertFalse((self.root / "arithmetic.json").exists())
        self.assertFalse((self.root / "arithmetic/actual.tsv").exists())

    def test_fresh_trace_publishes_only_new_results(self):
        def emit(*args, **kwargs):
            (self.root / "arithmetic/actual.tsv").write_bytes(b"new result\r\n")
        with patch.object(run_core_vectors.subprocess, "run", side_effect=emit):
            run_core_vectors.main()
        report = json.loads((self.root / "arithmetic.json").read_text())
        self.assertEqual(report["trace_rows"], 1)
        self.assertEqual(report["trace_sha256"], hashlib.sha256(b"new result\n").hexdigest())


if __name__ == "__main__":
    unittest.main()
