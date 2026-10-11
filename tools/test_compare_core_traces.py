import gzip
import hashlib
import json
import pathlib
import tempfile
import unittest
from compare_core_traces import PLATFORMS, SUITES, validate
from simulation_contracts import CONTRACTS, SESSION_OBJECTS_ORDERING, CREATURE_HP_SPECIFICATION, RAW_HANDLES_HELPERS, SESSION_HP_HELPERS, SESSION_COMBAT_HELPERS
from generate_session_vectors import REFERENCES, COMPOSITION_POLICY
from generate_objects_vectors import REFERENCES as OBJECT_REFERENCES, COMPOSITION_POLICY as OBJECT_POLICY


CREATURE_HP_REFERENCES = {
    "core": OBJECT_REFERENCES["core"],
    "creature": {
        "commit": "a0d89502ce1431375a6fc045b91a73039114e6b9",
        "path": "tools/ref_creature.py",
        "sha256": "debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7",
    },
}
COMBAT_CREATURE_REFERENCES = {**CREATURE_HP_REFERENCES, "combat": REFERENCES["combat"]}
RAW_HANDLES_REFERENCES = {**OBJECT_REFERENCES, "creature": CREATURE_HP_REFERENCES["creature"]}
SESSION_HP_REFERENCES = {**RAW_HANDLES_REFERENCES, "combat": REFERENCES["combat"], "netwire": REFERENCES["netwire"]}
SESSION_OBJECT_REFERENCES = {**OBJECT_REFERENCES, "netwire": REFERENCES["netwire"]}


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
            if suite == "objects-runtime":
                source.update(references=OBJECT_REFERENCES, composition_policy=OBJECT_POLICY)
            if suite == "session-objects":
                source.update(references=SESSION_OBJECT_REFERENCES,
                              composition_policy="rta-session-objects-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING)
            if suite == "creature-hp":
                source.update(references=CREATURE_HP_REFERENCES,
                              composition_policy="rta-creature-hp-v1",
                              specification_evidence=CREATURE_HP_SPECIFICATION)
            if suite == "combat-creature":
                source.update(references=COMBAT_CREATURE_REFERENCES,
                              composition_policy="rta-combat-creature-v1",
                              specification_evidence=CREATURE_HP_SPECIFICATION)
            if suite == "raw-handles":
                source.update(references=RAW_HANDLES_REFERENCES,
                              composition_policy="rta-raw-handles-v1",
                              specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=RAW_HANDLES_HELPERS)
            if suite == "session-hp":
                source.update(references=SESSION_HP_REFERENCES,
                              composition_policy="rta-session-hp-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=SESSION_HP_HELPERS)
            if suite == "session-combat":
                source.update(references=SESSION_HP_REFERENCES,
                              composition_policy="rta-session-combat-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=SESSION_COMBAT_HELPERS)
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

    def test_changed_creature_contract(self):
        for field in ("references", "composition_policy", "specification_evidence"):
            path = self.manifests / "creature_hp.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported creature"):
                self.check()
            path.write_text(original)

    def test_changed_combat_creature_contract(self):
        for field in ("references", "composition_policy", "specification_evidence"):
            path = self.manifests / "combat_creature.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported combat-creature"):
                self.check()
            path.write_text(original)

    def test_wrong_combat_creature_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"combat-creature\top\targs\twrong\tstate\n"
            (folder / "combat-creature.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "combat-creature.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
            self.check()

    def test_missing_combat_creature_host(self):
        (self.traces / "windows-arm64/combat-creature.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            self.check()

    def test_changed_raw_handle_contract(self):
        for field in ("references", "composition_policy", "specification_evidence", "helpers"):
            path = self.manifests / "raw_handles.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported raw-handles"):
                self.check()
            path.write_text(original)

    def test_wrong_raw_handle_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"raw-handles\top\targs\twrong\tstate\n"
            (folder / "raw-handles.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "raw-handles.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
            self.check()

    def test_missing_raw_handle_host(self):
        (self.traces / "windows-arm64/raw-handles.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            self.check()

    def test_changed_session_hp_contract(self):
        for field in ("references", "composition_policy", "ordering_evidence",
                      "specification_evidence", "helpers"):
            path = self.manifests / "session_hp.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported session-hp"):
                self.check()
            path.write_text(original)

    def test_wrong_session_hp_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"session-hp\top\targs\twrong\tstate\n"
            (folder / "session-hp.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-hp.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
            self.check()

    def test_missing_session_hp_host(self):
        (self.traces / "windows-arm64/session-hp.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            self.check()

    def test_complete(self):
        self.check()

    def test_changed_session_combat_contract(self):
        for field in ("references", "composition_policy", "ordering_evidence",
                      "specification_evidence", "helpers"):
            path = self.manifests / "session_combat.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported session-combat"):
                self.check()
            path.write_text(original)

    def test_wrong_session_combat_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"session-combat\top\targs\twrong\tstate\n"
            (folder / "session-combat.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-combat.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
            self.check()

    def test_missing_session_combat_host(self):
        (self.traces / "windows-arm64/session-combat.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
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

    def test_changed_object_dependency(self):
        path = self.manifests / "objects_runtime.json"
        manifest = json.loads(path.read_text())
        manifest["references"]["core"]["commit"] = "other"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "reference map"):
            self.check()

    def test_unrecognized_object_policy(self):
        path = self.manifests / "objects_runtime.json"
        manifest = json.loads(path.read_text())
        manifest["composition_policy"] = "invented"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "fixture policy"):
            self.check()

    def test_changed_session_object_dependency(self):
        path = self.manifests / "session_objects.json"
        manifest = json.loads(path.read_text())
        manifest["references"]["objects"]["sha256"] = "other"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "reference map"):
            self.check()

    def test_unrecognized_session_object_policy(self):
        path = self.manifests / "session_objects.json"
        manifest = json.loads(path.read_text())
        manifest["composition_policy"] = "inferred-gameplay"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "fixture policy"):
            self.check()

    def test_changed_session_object_ordering(self):
        path = self.manifests / "session_objects.json"
        manifest = json.loads(path.read_text())
        manifest["ordering_evidence"] = {**manifest["ordering_evidence"], "sha256": "other"}
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "ordering evidence"):
            self.check()

    def test_identical_wrong_session_object_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"session-objects\top\targs\twrong\tstate\n"
            (folder / "session-objects.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-objects.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
            self.check()

    def test_identical_wrong_creature_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"creature-hp\top\targs\twrong\tstate\n"
            (folder / "creature-hp.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "creature-hp.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "reference expectation"):
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

    def test_identical_wrong_object_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"objects\top\targs\twrong\tstate\n"
            (folder / "objects-runtime.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "objects-runtime.json"
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
