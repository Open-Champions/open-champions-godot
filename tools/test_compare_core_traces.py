import gzip
import hashlib
import json
import pathlib
import tempfile
import unittest
from compare_core_traces import PLATFORMS, SUITES, validate


class CoreArtifacts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.manifests = self.root / "manifests"
        self.manifests.mkdir()
        self.traces = self.root / "traces"
        self.traces.mkdir()
        for suite in SUITES:
            source = {"vector_sha256": "v", "reference_sha256": "r", "vector_count": 1}
            manifest = self.manifests / f"core_{suite}.json"
            manifest.write_text(json.dumps(source))
            for platform in PLATFORMS:
                folder = self.traces / platform
                folder.mkdir(exist_ok=True)
                data = b"1\t00000000\n"
                (folder / f"{suite}.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
                report = dict(schema_version=1, suite=suite, platform=platform, source_commit="commit",
                              vector_sha256="v", reference_sha256="r", trace_rows=1,
                              trace_sha256=hashlib.sha256(data).hexdigest(),
                              manifest_sha256=hashlib.sha256(manifest.read_bytes()).hexdigest())
                (folder / f"{suite}.json").write_text(json.dumps(report))

    def check(self):
        validate(self.traces, self.manifests, "commit")

    def edit(self, field, value):
        path = self.traces / "macos-arm64/arithmetic.json"
        report = json.loads(path.read_text())
        report[field] = value
        path.write_text(json.dumps(report))

    def test_complete(self):
        self.check()

    def test_missing(self):
        (self.traces / "windows-x64/state.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing"): self.check()

    def test_wrong_commit(self):
        self.edit("source_commit", "other")
        with self.assertRaisesRegex(ValueError, "commit"): self.check()

    def test_reduced_coverage(self):
        self.edit("trace_rows", 0)
        with self.assertRaisesRegex(ValueError, "coverage"): self.check()

    def test_false_provenance(self):
        self.edit("reference_sha256", "other")
        with self.assertRaisesRegex(ValueError, "provenance"): self.check()

    def test_actual_data_tampering(self):
        path = self.traces / "windows-arm64/math.tsv.gz"
        path.write_bytes(gzip.compress(b"1\t00000001\n", mtime=0))
        with self.assertRaisesRegex(ValueError, "actual trace"): self.check()

    def test_valid_but_divergent_trace(self):
        folder = self.traces / "linux-x64"
        data = b"1\t00000001\n"
        (folder / "state.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
        path = folder / "state.json"
        report = json.loads(path.read_text())
        report["trace_sha256"] = hashlib.sha256(data).hexdigest()
        path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "diverges"): self.check()


if __name__ == "__main__":
    unittest.main()
