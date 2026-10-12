import gzip
import hashlib
import json
import pathlib
import tempfile
import unittest
from compare_core_traces import PLATFORMS, SUITES, validate
from simulation_contracts import ANIMATION_CLOCK_REFERENCE, ANIMATION_CLOCK_REFERENCES, ANIMATION_CLOCK_SPECIFICATION, WORLD_GEOMETRY_REFERENCES, WORLD_GEOMETRY_SPECIFICATION, WORLD_GEOMETRY_FORMAT, CONTRACTS, SESSION_OBJECTS_ORDERING, CREATURE_HP_SPECIFICATION, RAW_HANDLES_HELPERS, SESSION_HP_HELPERS, SESSION_COMBAT_HELPERS, WORLD_QUERIES_SPECIFICATION, SESSION_TERRAIN_HELPERS, SESSION_CONTROL_HELPERS, TERRAIN_FILE_HELPERS, TERRAIN_FILE_FORMAT, SESSION_CREATURE_HELPERS, SESSION_CONTROL_CREATURE_HELPERS
from simulation_contracts import (SESSION_ANIMATION_REFERENCE, SESSION_ANIMATION_REFERENCES,
                                  SESSION_ANIMATION_HELPERS, SESSION_ANIMATION_GENERATOR,
                                  SESSION_ANIMATION_PROTOCOL)
from simulation_contracts import (ANIMATION_DECODER_REFERENCE, ANIMATION_DECODER_REFERENCES,
                                  ANIMATION_DECODER_SPECIFICATION, ANIMATION_DECODER_GENERATOR,
                                  ANIMATION_DECODER_PROTOCOL)
from simulation_contracts import (CREATURE_TURN_REFERENCE, CREATURE_TURN_REFERENCES,
                                  CREATURE_TURN_SPECIFICATION, CREATURE_TURN_GENERATOR,
                                  CREATURE_TURN_PROTOCOL)
from simulation_contracts import (SESSION_TURN_REFERENCE, SESSION_TURN_REFERENCES,
                                  SESSION_TURN_SPECIFICATION, SESSION_TURN_ORDERING,
                                  SESSION_TURN_HELPERS, SESSION_TURN_GENERATOR,
                                  SESSION_TURN_PROTOCOL)
from core_trace_contract import expected_trace
from generate_session_vectors import REFERENCES, COMPOSITION_POLICY
from generate_objects_vectors import REFERENCES as OBJECT_REFERENCES, COMPOSITION_POLICY as OBJECT_POLICY
from generate_world_queries_vectors import REFERENCES as WORLD_REFERENCES
from generate_session_terrain_vectors import REFERENCES as TERRAIN_REFERENCES


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
                "animation-decoder": (b"field_width\t0\t0;[]\tok\t[[[1,2],[3],[],[[1,[2]]],[[0,1,2,3,4,5,[[6]]]],[1,2]],[],[]]\n",
                                      b"field_width\t0\t0;[]\tok\t[[[1,2],[3],[],[[1,[2]]],[[0,1,2,3,4,5,[[6]]]],[1,2]],[],[]]\n"),
                "session-animation": (b"events\t0\t0;[]\tok\t[[],[[],[1]],[],[]]\n",
                                      b"events\t0\t0;[]\tok\t[[],[[],[1]],[],[]]\n"),
                "state": (b"rng\tdraw\tshared\t0\tshared=0\n", b"rng\tdraw\tshared\t0\tshared=0\n"),
            }.get(suite, (b"items\top\targs\tresult\tstate\n", b"items\top\targs\tresult\tstate\n"))
            vector_hash = hashlib.sha256(vectors).hexdigest()
            (self.manifests / f"{CONTRACTS[suite][0]}.tsv.gz").write_bytes(gzip.compress(vectors, mtime=0))
            source = {"vector_sha256": vector_hash, "reference_sha256": CONTRACTS[suite][1], "vector_count": 1}
            if suite == "session-turn":
                source.update(references=SESSION_TURN_REFERENCES,
                              composition_policy="rta-session-turn-v1",
                              specification_evidence=SESSION_TURN_SPECIFICATION,
                              ordering_evidence=SESSION_TURN_ORDERING,
                              helpers=SESSION_TURN_HELPERS,
                              generator_sha256=SESSION_TURN_GENERATOR,
                              input_protocol=SESSION_TURN_PROTOCOL)
            if suite == "creature-turn":
                source.update(references=CREATURE_TURN_REFERENCES,
                              composition_policy="rta-creature-turn-v1",
                              specification_evidence=CREATURE_TURN_SPECIFICATION,
                              helpers={}, generator_sha256=CREATURE_TURN_GENERATOR,
                              input_protocol=CREATURE_TURN_PROTOCOL)
            if suite == "animation-decoder":
                source.update(references=ANIMATION_DECODER_REFERENCES,
                              composition_policy="rta-animation-decoder-v1",
                              specification_evidence=ANIMATION_DECODER_SPECIFICATION,
                              helpers={}, generator_sha256=ANIMATION_DECODER_GENERATOR,
                              input_protocol=ANIMATION_DECODER_PROTOCOL)
            if suite == "session-animation":
                source.update(references=SESSION_ANIMATION_REFERENCES,
                              composition_policy="rta-session-animation-v1",
                              specification_evidence=ANIMATION_CLOCK_SPECIFICATION,
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              helpers=SESSION_ANIMATION_HELPERS,
                              generator_sha256=SESSION_ANIMATION_GENERATOR,
                              input_protocol=SESSION_ANIMATION_PROTOCOL)
            if suite == "animation-clock":
                source.update(references=ANIMATION_CLOCK_REFERENCES,
                              composition_policy="rta-animation-clock-v1",
                              specification_evidence=ANIMATION_CLOCK_SPECIFICATION,
                              helpers={},
                              generator_sha256="8863600a495f0d104285b6029b6beb649d4de00604774fc469122f9011986cf4")
            if suite == "world-geometry":
                source.update(references=WORLD_GEOMETRY_REFERENCES,
                              composition_policy="rta-world-geometry-v1",
                              specification_evidence=WORLD_GEOMETRY_SPECIFICATION,
                              format_evidence=WORLD_GEOMETRY_FORMAT, helpers={})
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
            if suite == "creature-movement":
                source.update(references=CREATURE_HP_REFERENCES,
                              composition_policy="rta-creature-movement-v1",
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
            if suite == "world-queries":
                source.update(references=WORLD_REFERENCES,
                              composition_policy="rta-supplied-terrain-v1",
                              specification_evidence=WORLD_QUERIES_SPECIFICATION)
            if suite == "session-terrain":
                source.update(references=TERRAIN_REFERENCES,
                              composition_policy="rta-session-terrain-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              specification_evidence=WORLD_QUERIES_SPECIFICATION,
                              creature_specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=SESSION_TERRAIN_HELPERS)
            if suite == "session-creature":
                source.update(references=TERRAIN_REFERENCES,
                              composition_policy="rta-session-creature-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              specification_evidence=WORLD_QUERIES_SPECIFICATION,
                              creature_specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=SESSION_CREATURE_HELPERS)
            if suite == "session-control":
                source.update(references=TERRAIN_REFERENCES,
                              composition_policy="rta-session-control-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              specification_evidence=WORLD_QUERIES_SPECIFICATION,
                              creature_specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=SESSION_CONTROL_HELPERS)
            if suite == "session-control-creature":
                source.update(references=TERRAIN_REFERENCES,
                              composition_policy="rta-session-control-creature-v1",
                              ordering_evidence=SESSION_OBJECTS_ORDERING,
                              specification_evidence=WORLD_QUERIES_SPECIFICATION,
                              creature_specification_evidence=CREATURE_HP_SPECIFICATION,
                              helpers=SESSION_CONTROL_CREATURE_HELPERS)
            if suite == "terrain-file":
                source.update(references=WORLD_REFERENCES,
                              composition_policy="rta-terrain-file-v1",
                              specification_evidence=WORLD_QUERIES_SPECIFICATION,
                              format_evidence=TERRAIN_FILE_FORMAT,
                              helpers=TERRAIN_FILE_HELPERS)
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

    def replace_clock_manifest(self, manifest):
        # Refresh every host receipt too: rejection must come from the strict
        # contract, not merely from a stale outer manifest digest.
        path = self.manifests / "animation_clock.json"
        path.write_text(json.dumps(manifest))
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        for platform in PLATFORMS:
            report_path = self.traces / platform / "animation-clock.json"
            report = json.loads(report_path.read_text())
            report["manifest_sha256"] = digest
            report_path.write_text(json.dumps(report))

    def replace_decoder_manifest(self, manifest):
        path = self.manifests / "animation_decoder.json"
        path.write_text(json.dumps(manifest))
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        for platform in PLATFORMS:
            report_path = self.traces / platform / "animation-decoder.json"
            report = json.loads(report_path.read_text())
            report["manifest_sha256"] = digest
            report_path.write_text(json.dumps(report))

    def test_decoder_requires_all_five_hosts(self):
        self.assertEqual(len(SUITES), 37)
        self.assertEqual(CONTRACTS["animation-decoder"],
                         ("animation_decoder", ANIMATION_DECODER_REFERENCE))
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                path = self.traces / platform / "animation-decoder.json"
                original = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "missing required"):
                    self.check()
                path.write_bytes(original)

    def test_decoder_missing_or_altered_contract_is_refused(self):
        original = json.loads((self.manifests / "animation_decoder.json").read_text())
        for field in ("references", "composition_policy", "specification_evidence",
                      "helpers", "generator_sha256", "input_protocol"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    changed = json.loads(json.dumps(original))
                    if missing:
                        del changed[field]
                    else:
                        changed[field] = "unverified"
                    self.replace_decoder_manifest(changed)
                    with self.assertRaisesRegex(ValueError, "unsupported animation-decoder"):
                        self.check()
                    self.replace_decoder_manifest(original)

    def test_decoder_pins_all_source_and_specification_fields(self):
        original = json.loads((self.manifests / "animation_decoder.json").read_text())
        self.assertEqual(set(original["references"]), {"core", "anim"})
        self.assertEqual(original["helpers"], {})
        encoded = json.dumps(original["references"], sort_keys=True,
                             separators=(",", ":")).encode("ascii")
        self.assertEqual(hashlib.sha256(encoded).hexdigest(), ANIMATION_DECODER_REFERENCE)
        for group in ("core", "anim", "specification"):
            for field in ("commit", "path", "sha256"):
                with self.subTest(group=group, field=field):
                    changed = json.loads(json.dumps(original))
                    target = (changed["specification_evidence"] if group == "specification"
                              else changed["references"][group])
                    target[field] = "wrong-source"
                    self.replace_decoder_manifest(changed)
                    with self.assertRaisesRegex(ValueError, "unsupported animation-decoder"):
                        self.check()
                    self.replace_decoder_manifest(original)

    def test_decoder_rejects_extra_sources_helpers_and_each_protocol_change(self):
        original = json.loads((self.manifests / "animation_decoder.json").read_text())
        for group in ("references", "helpers", "input_protocol"):
            fields = original[group] if group == "input_protocol" else ["unexpected"]
            for field in fields:
                with self.subTest(group=group, field=field):
                    changed = json.loads(json.dumps(original))
                    changed[group][field] = "unverified"
                    self.replace_decoder_manifest(changed)
                    with self.assertRaisesRegex(ValueError, "unsupported animation-decoder"):
                        self.check()
                    self.replace_decoder_manifest(original)

    def test_decoder_generator_source_and_schema_match_pins(self):
        # Source verification does not execute the oracle or read research files.
        import ast
        path = pathlib.Path(__file__).resolve().parent / "generate_animation_decoder_vectors.py"
        source = path.read_bytes()
        self.assertEqual(hashlib.sha256(source).hexdigest(), ANIMATION_DECODER_GENERATOR)
        tree = ast.parse(source)
        values = {target.id: ast.literal_eval(node.value)
                  for node in tree.body if isinstance(node, ast.Assign)
                  for target in node.targets if isinstance(target, ast.Name)
                  and target.id in ("REFERENCES", "SPECIFICATION_EVIDENCE", "INPUT_PROTOCOL")}
        self.assertEqual(values["REFERENCES"], ANIMATION_DECODER_REFERENCES)
        self.assertEqual(values["SPECIFICATION_EVIDENCE"], ANIMATION_DECODER_SPECIFICATION)
        self.assertEqual(values["INPUT_PROTOCOL"], ANIMATION_DECODER_PROTOCOL)

    def test_identical_missing_or_altered_decoder_state_on_all_hosts_is_refused(self):
        fixture = gzip.decompress((self.manifests / "animation_decoder.tsv.gz").read_bytes())
        self.assertEqual(len(json.loads(fixture.split(b"\t")[-1])), 3)
        for old, new in ((b"[[1,[2]]]", b"[]"),  # lost stable track owner
                         (b"[[6]]", b"[]"),   # omitted stale joint cells
                         (b"[1,2]],[],[]]", b"[2,1]],[],[]]")):
            with self.subTest(old=old):
                self.assertIn(old, fixture)
                data = fixture.replace(old, new)
                for platform in PLATFORMS:
                    folder = self.traces / platform
                    (folder / "animation-decoder.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
                    path = folder / "animation-decoder.json"
                    report = json.loads(path.read_text())
                    report["trace_sha256"] = hashlib.sha256(data).hexdigest()
                    path.write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, "animation-decoder:.*reference expectation"):
                    self.check()

    def test_decoder_five_column_contract_rejects_malformed_rows(self):
        for row in ("decode\t0\t0;[]\tok", "decode\t0\t0;[]\tok\t[]\textra"):
            with self.subTest(row=row):
                with self.assertRaisesRegex(ValueError, "malformed animation-decoder"):
                    expected_trace("animation-decoder", [row])

    def replace_session_animation_manifest(self, manifest):
        path = self.manifests / "session_animation.json"
        path.write_text(json.dumps(manifest))
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        for platform in PLATFORMS:
            report_path = self.traces / platform / "session-animation.json"
            report = json.loads(report_path.read_text())
            report["manifest_sha256"] = digest
            report_path.write_text(json.dumps(report))

    def test_session_animation_requires_all_five_hosts(self):
        self.assertEqual(len(SUITES), 37)
        self.assertEqual(CONTRACTS["session-animation"],
                         ("session_animation", SESSION_ANIMATION_REFERENCE))
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                path = self.traces / platform / "session-animation.json"
                original = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "missing required"):
                    self.check()
                path.write_bytes(original)

    def test_session_animation_missing_or_changed_contract(self):
        original = json.loads((self.manifests / "session_animation.json").read_text())
        for field in ("references", "composition_policy", "specification_evidence",
                      "ordering_evidence", "helpers", "generator_sha256", "input_protocol"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    changed = json.loads(json.dumps(original))
                    if missing:
                        del changed[field]
                    else:
                        changed[field] = "unverified"
                    self.replace_session_animation_manifest(changed)
                    with self.assertRaisesRegex(ValueError, "unsupported session-animation"):
                        self.check()
                    self.replace_session_animation_manifest(original)

    def test_session_animation_each_source_helper_and_evidence_field_is_pinned(self):
        original = json.loads((self.manifests / "session_animation.json").read_text())
        self.assertEqual(set(original["references"]),
                         {"core", "objects", "netwire", "combat", "creature", "anim"})
        self.assertEqual(set(original["helpers"]),
                         {"session_hp", "session_objects", "combat_snapshot"})
        encoded = json.dumps(original["references"], sort_keys=True,
                             separators=(",", ":")).encode("ascii")
        self.assertEqual(hashlib.sha256(encoded).hexdigest(), SESSION_ANIMATION_REFERENCE)
        targets = [(group, name) for group in ("references", "helpers")
                   for name in original[group]]
        targets += [(group, None) for group in ("specification_evidence", "ordering_evidence")]
        for group, name in targets:
            target = original[group] if name is None else original[group][name]
            for field in target:
                with self.subTest(group=group, name=name, field=field):
                    changed = json.loads(json.dumps(original))
                    value = changed[group] if name is None else changed[group][name]
                    value[field] = "wrong-source"
                    self.replace_session_animation_manifest(changed)
                    with self.assertRaisesRegex(ValueError, "unsupported session-animation"):
                        self.check()
                    self.replace_session_animation_manifest(original)

    def test_session_animation_extra_sources_and_protocol_downgrade_refused(self):
        original = json.loads((self.manifests / "session_animation.json").read_text())
        for group in ("references", "helpers", "input_protocol"):
            with self.subTest(group=group):
                changed = json.loads(json.dumps(original))
                if group == "input_protocol":
                    changed[group]["observation"] = "[input,legacy_snapshot]"
                else:
                    changed[group]["unexpected"] = {"path": "unpinned.py", "sha256": "0" * 64}
                self.replace_session_animation_manifest(changed)
                with self.assertRaisesRegex(ValueError, "unsupported session-animation"):
                    self.check()
                self.replace_session_animation_manifest(original)

    def test_identical_omitted_animation_state_on_all_hosts_is_refused(self):
        # A minimal synthetic envelope isolates the gate: dropping the extension
        # must fail even when every host agrees and all receipt hashes are fresh.
        data = b"events\t0\t0;[]\tok\t[[],[[],_],[],[]]\n"
        for platform in PLATFORMS:
            folder = self.traces / platform
            (folder / "session-animation.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-animation.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "session-animation:.*reference expectation"):
            self.check()

    def test_session_animation_rejects_malformed_column_shapes(self):
        for row in ("events\t0\t0;[]\tok", "events\t0\t0;[]\tok\t[]\textra"):
            with self.subTest(row=row):
                with self.assertRaisesRegex(ValueError, "malformed session-animation"):
                    expected_trace("session-animation", [row])

    def test_animation_clock_requires_all_five_hosts(self):
        self.assertEqual(len(SUITES), 37)
        self.assertEqual(len(PLATFORMS), 5)
        self.assertEqual(CONTRACTS["animation-clock"],
                         ("animation_clock", ANIMATION_CLOCK_REFERENCE))
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                path = self.traces / platform / "animation-clock.json"
                original = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "missing required"):
                    self.check()
                path.write_bytes(original)

    def test_animation_clock_missing_or_changed_contract_is_refused(self):
        original = json.loads((self.manifests / "animation_clock.json").read_text())
        for field in ("references", "composition_policy", "specification_evidence",
                      "helpers", "generator_sha256"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    manifest = json.loads(json.dumps(original))
                    if missing:
                        del manifest[field]
                    else:
                        manifest[field] = "unverified"
                    self.replace_clock_manifest(manifest)
                    with self.assertRaisesRegex(ValueError, "unsupported animation-clock"):
                        self.check()
                    self.replace_clock_manifest(original)

    def test_animation_clock_pins_each_reference_and_specification_field(self):
        original = json.loads((self.manifests / "animation_clock.json").read_text())
        self.assertEqual(set(original["references"]), {"core", "anim"})
        encoded = json.dumps(original["references"], sort_keys=True,
                             separators=(",", ":")).encode("ascii")
        self.assertEqual(hashlib.sha256(encoded).hexdigest(), ANIMATION_CLOCK_REFERENCE)
        for group in ("core", "anim", "specification"):
            for field in ("commit", "path", "sha256"):
                with self.subTest(group=group, field=field):
                    manifest = json.loads(json.dumps(original))
                    target = (manifest["specification_evidence"] if group == "specification"
                              else manifest["references"][group])
                    target[field] = "wrong-immutable-source"
                    self.replace_clock_manifest(manifest)
                    with self.assertRaisesRegex(ValueError, "unsupported animation-clock"):
                        self.check()
                    self.replace_clock_manifest(original)

    def test_animation_clock_rejects_undeclared_helper_or_reference(self):
        original = json.loads((self.manifests / "animation_clock.json").read_text())
        self.assertEqual(original["helpers"], {})
        for group in ("helpers", "references"):
            with self.subTest(group=group):
                manifest = json.loads(json.dumps(original))
                manifest[group]["unexpected"] = {"path": "tools/unpinned.py", "sha256": "0" * 64}
                self.replace_clock_manifest(manifest)
                with self.assertRaisesRegex(ValueError, "unsupported animation-clock"):
                    self.check()
                self.replace_clock_manifest(original)

    def test_animation_clock_stale_generator_hash_is_refused(self):
        manifest = json.loads((self.manifests / "animation_clock.json").read_text())
        manifest["generator_sha256"] = "0" * 64
        self.replace_clock_manifest(manifest)
        with self.assertRaisesRegex(ValueError, "unsupported animation-clock"):
            self.check()

    def test_identical_wrong_animation_clock_results_are_refused(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"items\top\targs\tresult\twrong-clock-state\n"
            (folder / "animation-clock.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "animation-clock.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            report["trace_rows"] = 1
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "animation-clock:.*reference expectation"):
            self.check()

    def test_world_geometry_contract_is_pinned(self):
        path = self.manifests / "world_geometry.json"
        original = path.read_text()
        for field in ("references", "composition_policy", "specification_evidence",
                      "format_evidence", "helpers"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    data = json.loads(original)
                    if missing:
                        del data[field]
                    else:
                        data[field] = "unverified"
                    path.write_text(json.dumps(data))
                    with self.assertRaisesRegex(ValueError, "unsupported world-geometry"):
                        self.check()
                    path.write_text(original)
        for field in ("commit", "path", "sha256"):
            data = json.loads(original)
            data["references"]["world_info"][field] = "unverified"
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "unsupported world-geometry"):
                self.check()
            path.write_text(original)

    def test_world_geometry_requires_every_host(self):
        (self.traces / "windows-arm64/world-geometry.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing required"):
            self.check()

    def test_identical_wrong_world_geometry_results_are_refused(self):
        for platform in PLATFORMS:
            data = b"decode\t0\t0;00\tok\t[wrong-geometry]\n"
            folder = self.traces / platform
            (folder / "world-geometry.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "world-geometry.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            report["trace_rows"] = 1
            path.write_text(json.dumps(report))
        with self.assertRaises(ValueError):
            self.check()

    def test_session_creature_missing_host_is_refused(self):
        self.assertIn("session-creature", SUITES)
        (self.traces / "windows-arm64/session-creature.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing required"):
            self.check()

    def test_session_creature_contract_tampering_is_refused(self):
        for key, replacement in (("composition_policy", "wrong"),
                                 ("helpers", {}), ("ordering_evidence", {}),
                                 ("specification_evidence", {}),
                                 ("creature_specification_evidence", {}),
                                 ("references", {})):
            with self.subTest(key=key):
                path = self.manifests / "session_creature.json"
                before = path.read_bytes()
                source = json.loads(before)
                source[key] = replacement
                path.write_text(json.dumps(source))
                try:
                    with self.assertRaisesRegex(ValueError, "unsupported session-creature"):
                        self.check()
                finally:
                    path.write_bytes(before)

    def test_session_creature_false_consensus_is_refused(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"session-creature\t0\t0;0\tok\t[wrong-state]\n"
            (folder / "session-creature.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-creature.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "session-creature:.*reference expectation"):
            self.check()

    def test_creature_movement_required_on_each_host(self):
        self.assertIn("creature-movement", SUITES)
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                path = self.traces / platform / "creature-movement.json"
                original = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "missing required"):
                    self.check()
                path.write_bytes(original)

    def test_creature_movement_contract_is_pinned(self):
        path = self.manifests / "creature_movement.json"
        original = path.read_text()
        for field in ("references", "composition_policy", "specification_evidence"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    changed = json.loads(original)
                    if missing:
                        del changed[field]
                    else:
                        changed[field] = "unverified"
                    path.write_text(json.dumps(changed))
                    with self.assertRaisesRegex(ValueError, "unsupported creature-movement"):
                        self.check()
                    path.write_text(original)

    def test_creature_movement_pins_both_models(self):
        path = self.manifests / "creature_movement.json"
        original = path.read_text()
        for name in json.loads(original)["references"]:
            for missing in (False, True):
                with self.subTest(name=name, missing=missing):
                    changed = json.loads(original)
                    if missing:
                        del changed["references"][name]
                    else:
                        changed["references"][name]["sha256"] = "0" * 64
                    path.write_text(json.dumps(changed))
                    with self.assertRaisesRegex(ValueError, "unsupported creature-movement"):
                        self.check()
                    path.write_text(original)

    def test_creature_movement_stale_report(self):
        path = self.traces / "windows-arm64/creature-movement.json"
        original = path.read_text()
        for field, value, error in (("source_commit", "old", "commit"),
                                    ("manifest_sha256", "old", "manifest"),
                                    ("trace_rows", 0, "coverage"),
                                    ("reference_sha256", "old", "provenance")):
            with self.subTest(field=field):
                report = json.loads(original)
                report[field] = value
                path.write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, error):
                    self.check()
                path.write_text(original)

    def test_identical_wrong_creature_movement_results_are_rejected(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"creature-movement\t0\t0;0\tok\t[wrong-state]\n"
            (folder / "creature-movement.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "creature-movement.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "creature-movement:.*reference expectation"):
            self.check()

    def test_session_control_required_on_each_host(self):
        self.assertEqual(len(SUITES), 37)
        self.assertIn("session-control", SUITES)
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                path = self.traces / platform / "session-control.json"
                original = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "missing required"):
                    self.check()
                path.write_bytes(original)

    def test_changed_or_missing_creature_control_contract(self):
        path = self.manifests / "session_control_creature.json"
        original = path.read_text()
        for field in ("references", "composition_policy", "ordering_evidence",
                      "specification_evidence", "creature_specification_evidence", "helpers"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    manifest = json.loads(original)
                    if missing:
                        del manifest[field]
                    else:
                        manifest[field] = "unverified"
                    path.write_text(json.dumps(manifest))
                    with self.assertRaisesRegex(ValueError, "unsupported session-control-creature"):
                        self.check()
                    path.write_text(original)

    def test_creature_control_pins_every_model_and_helper(self):
        path = self.manifests / "session_control_creature.json"
        original = path.read_text()
        manifest = json.loads(original)
        self.assertEqual(len(manifest["references"]), 7)
        self.assertEqual(len(manifest["helpers"]), 7)
        for group in ("references", "helpers"):
            for name in manifest[group]:
                with self.subTest(group=group, name=name):
                    changed = json.loads(original)
                    changed[group][name]["sha256"] = "0" * 64
                    path.write_text(json.dumps(changed))
                    with self.assertRaisesRegex(ValueError, "unsupported session-control-creature"):
                        self.check()
                    path.write_text(original)

    def test_identical_wrong_creature_control_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"movement\t0\t0;0\tok\t[wrong-owned-state]\n"
            (folder / "session-control-creature.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-control-creature.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            report["trace_rows"] = 1
            path.write_text(json.dumps(report))
        with self.assertRaises(ValueError):
            self.check()

    def test_creature_control_requires_every_host(self):
        path = self.traces / "windows-arm64/session-control-creature.json"
        path.unlink()
        with self.assertRaisesRegex(ValueError, "missing required"):
            self.check()

    def test_changed_or_missing_session_control_contract(self):
        path = self.manifests / "session_control.json"
        original = path.read_text()
        for field in ("references", "composition_policy", "ordering_evidence",
                      "specification_evidence", "creature_specification_evidence", "helpers"):
            for missing in (False, True):
                with self.subTest(field=field, missing=missing):
                    manifest = json.loads(original)
                    if missing:
                        del manifest[field]
                    else:
                        manifest[field] = "unverified"
                    path.write_text(json.dumps(manifest))
                    with self.assertRaisesRegex(ValueError, "unsupported session-control"):
                        self.check()
                    path.write_text(original)

    def test_session_control_pins_every_model_and_helper(self):
        path = self.manifests / "session_control.json"
        original = path.read_text()
        manifest = json.loads(original)
        self.assertEqual(len(manifest["references"]), 7)
        self.assertEqual(len(manifest["helpers"]), 5)
        for group in ("references", "helpers"):
            for name in manifest[group]:
                with self.subTest(group=group, name=name):
                    changed = json.loads(original)
                    changed[group][name]["sha256"] = "0" * 64
                    path.write_text(json.dumps(changed))
                    with self.assertRaisesRegex(ValueError, "unsupported session-control"):
                        self.check()
                    path.write_text(original)

    def test_identical_wrong_control_results_on_every_host(self):
        for platform in PLATFORMS:
            folder = self.traces / platform
            data = b"movement\t0\t0;0\tok\t[wrong-owned-state]\n"
            (folder / "session-control.tsv.gz").write_bytes(gzip.compress(data, mtime=0))
            path = folder / "session-control.json"
            report = json.loads(path.read_text())
            report["trace_sha256"] = hashlib.sha256(data).hexdigest()
            path.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, "session-control:.*reference expectation"):
            self.check()

    def test_session_control_stale_report_and_incomplete_coverage(self):
        path = self.traces / "windows-arm64/session-control.json"
        original = path.read_text()
        for field, value, error in (("source_commit", "old", "commit"),
                                    ("manifest_sha256", "old", "manifest"),
                                    ("trace_rows", 0, "coverage"),
                                    ("reference_sha256", "old", "provenance")):
            with self.subTest(field=field):
                report = json.loads(original)
                report[field] = value
                path.write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, error):
                    self.check()
                path.write_text(original)

    def test_changed_terrain_file_contract(self):
        for field in ("references", "composition_policy", "specification_evidence",
                      "format_evidence", "helpers"):
            with self.subTest(field=field):
                path = self.manifests / "terrain_file.json"
                original = path.read_text()
                manifest = json.loads(original)
                manifest[field] = "unverified"
                path.write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError, "unsupported terrain-file"):
                    self.check()
                path.write_text(original)

    def test_changed_session_terrain_contract(self):
        for field in ("references", "composition_policy", "ordering_evidence",
                      "specification_evidence", "creature_specification_evidence", "helpers"):
            path = self.manifests / "session_terrain.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported session-terrain"):
                self.check()
            path.write_text(original)

    def test_changed_world_contract(self):
        for field in ("references", "composition_policy", "specification_evidence"):
            path = self.manifests / "world_queries.json"
            original = path.read_text()
            manifest = json.loads(original)
            manifest[field] = "unverified"
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "unsupported world-queries"):
                self.check()
            path.write_text(original)

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
