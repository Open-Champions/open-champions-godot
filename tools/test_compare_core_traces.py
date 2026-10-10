import gzip
import hashlib
import json
import pathlib
import tempfile
import unittest
from compare_core_traces import PLATFORMS, SUITES, validate
from simulation_contracts import CONTRACTS
from generate_session_vectors import REFERENCES, COMPOSITION_POLICY


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
            vectors, data = {
                "arithmetic": (b"ps2\t0\t0\tadd\t00000000\t00000000\t00000000\n", b"1\tadd\t0000000000000000\n"),
                "math": (b"sin16\tps2\t0\t0\t00000000\t34800000\n", b"# rta-core-math-trace-v1\nsin16\tps2:0:0\t00000000\t34800000\n"),
                "state": (b"rng\tdraw\tshared\t0\tshared=0\n", b"rng\tdraw\tshared\t0\tshared=0\n"),
            }.get(suite, (b"items\top\targs\tresult\tstate\n", b"items\top\targs\tresult\tstate\n"))
            vector_hash = hashlib.sha256(vectors).hexdigest()
            (self.manifests / f"{CONTRACTS[suite][0]}.tsv.gz").write_bytes(gzip.compress(vectors, mtime=0))
            source = {"vector_sha256": vector_hash, "reference_sha256": CONTRACTS[suite][1], "vector_count": 1}
            if suite == "session-replay":
                source.update(references=REFERENCES, composition_policy=COMPOSITION_POLICY)
            manifest = self.manifests / f"{CONTRACTS[suite][0]}.json"
            manifest.write_text(json.dumps(source))
            for platform in PLATFORMS:
                folder = self.traces / platform
                folder.mkdir(exist_ok=True)
                (folder / f"{suite}.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
                report = dict(schema_version=1, suite=suite, platform=platform, source_commit="commit",
                              vector_sha256=vector_hash, reference_sha256=CONTRACTS[suite][1], trace_rows=1,
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

    def test_unsupported_item_contract(self):
        path = self.manifests / "items.json"
        manifest = json.loads(path.read_text())
        manifest["reference_sha256"] = "unverified"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "unsupported reference"):
            self.check()

    def test_changed_session_component(self):
        path = self.manifests / "session_replay.json"
        manifest = json.loads(path.read_text())
        manifest["references"]["combat"]["commit"] = "other"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "reference map"):
            self.check()

    def test_unrecognized_session_policy(self):
        path = self.manifests / "session_replay.json"
        manifest = json.loads(path.read_text())
        manifest["composition_policy"] = "invented"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "composition policy"):
            self.check()

    def test_identical_wrong_item_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"items\top\targs\twrong\tstate\n"
            (folder / "items.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "items.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
            self.check()

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
        with self.assertRaisesRegex(ValueError, "reference expectation"): self.check()

    def test_identical_wrong_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"1\tadd\t0000000000000001\n"
            (folder / "arithmetic.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "arithmetic.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"): self.check()


if __name__ == "__main__":
    unittest.main()
