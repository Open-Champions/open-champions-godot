"""Regression checks for false-green Godot logs; no game data or engine needed."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import godot_isolation
import godot_log
import godot_matrix


class LogTests(unittest.TestCase):
    def test_script_engine_and_api_diagnostics(self):
        # Godot 4.5.2 headless probe: push_error and a Nil method call both
        # emit these diagnostics while the process still exits zero.
        for message in (
            "ERROR: synthetic diagnostic probe",
            "SCRIPT ERROR: Invalid call. Nonexistent function 'missing' in base 'Nil'.",
            "SCRIPT ERROR: Invalid access to property 'loaded'",
            "ERROR: Parameter m is null.",
            "USER ERROR: test assertion failed",
            "Parse Error: Could not resolve class",
            "Compile Error: Identifier not found",
            "Shader Error: Unknown identifier",
            '<C++ Error> Condition "!is_inside_tree()" is true.',
            'Condition "!is_inside_tree()" is true. Returning: Transform3D()',
            'Parameter "m" is null.',
            "Index p_surface = 0 is out of bounds (surfaces.size() = 0).",
            '   <C++ Error>   Parameter "m" is null.',
            '\tERROR: Condition "!is_inside_tree()" is true. Returning: Transform3D()',
            '  [00:00:01.123] ERROR: Index p_surface = 0 is out of bounds (surfaces.size() = 0).',
            "[00:01] \x1b[31;1mSCRIPT ERROR:\x1b[0m Invalid call",
        ):
            with self.subTest(message=message):
                errors = godot_log.error_lines("startup\n" + message + "\nPASS\n")
                self.assertEqual(len(errors), 1)
                self.assertEqual(errors[0][0], 2)

    def test_normal_output_and_supported_refusals(self):
        self.assertEqual(godot_log.error_lines(
            'Godot Engine v4.5.2\nPASS malformed input refused\n'
            '{"ok":false,"error":"unsupported format"}\n'
            'WARNING: test warning\nWARNING: ObjectDB instances leaked at exit\n'
            'SCENE_PLAY_FLOW_PASS checks=65 failures=0\n'), [])

    def test_quoted_diagnostics_refusals_and_paths_are_not_diagnostics(self):
        for message in (
            'WARNING: fixture deliberately rejects ERROR: input',
            '{"ok":false,"error":"unsupported token ERROR: in input"}',
            '{"error":"SCRIPT ERROR: rejected input"}',
            '"ERROR: quoted diagnostic"',
            "'SCRIPT ERROR: quoted diagnostic'",
            'example: <C++ Error> Condition "x" is true',
            'res://tests/ERROR: fixture.gd',
            '/tmp/USER ERROR: example/output.log',
            'C:\\fixtures\\ERROR: sample.txt',
            'PASS Parameter "m" is null.',
            'payload Index p_surface = 0 is out of bounds',
            '[not a timestamp] ERROR: payload',
        ):
            with self.subTest(message=message):
                self.assertEqual(godot_log.error_lines(message), [])

    def test_allow_list_is_full_line_and_does_not_hide_nearby_errors(self):
        exception = ((re.compile(r"ERROR: synthetic benign diagnostic"), "test-only evidence"),)
        with patch.object(godot_log, "ALLOW_LIST", exception):
            self.assertEqual(godot_log.error_lines("ERROR: synthetic benign diagnostic"), [])
            self.assertEqual(len(godot_log.error_lines(
                "ERROR: synthetic benign diagnostic changed\nERROR: real regression")), 2)


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.log = self.root / "nested/godot.log"

    def tearDown(self):
        self.temporary.cleanup()

    def run_python(self, source, **kwargs):
        return godot_matrix.run_logged([sys.executable, "-c", source], self.log, **kwargs)

    def test_zero_exit_and_pass_marker_cannot_override_either_stream(self):
        for stream in ("sys.stdout", "sys.stderr"):
            for message in ("SCRIPT ERROR: Invalid call", "ERROR: mesh is null"):
                with self.subTest(stream=stream, message=message):
                    with self.assertRaisesRegex(ValueError, "Godot error diagnostic"):
                        self.run_python(f"import sys; print('PASS'); print({message!r}, file={stream})",
                                        expect=["PASS"])
                    self.assertIn(message, self.log.read_text())

    def test_clean_success_and_completion_marker(self):
        self.assertEqual(self.run_python("print('PASS')", expect=["PASS"]), "PASS\n")
        with self.assertRaisesRegex(ValueError, "missing completion marker"):
            self.run_python("print('prefix PASS')", expect=["PASS"])

    def test_nonzero_exit_and_timeout_are_failures(self):
        with self.assertRaisesRegex(ValueError, "exited 7"):
            self.run_python("raise SystemExit(7)")
        with self.assertRaisesRegex(ValueError, "timed out"):
            self.run_python("import time; print('started', flush=True); time.sleep(30)", timeout=0.2)
        self.assertIn("started", self.log.read_text())
        self.assertIn("TIMEOUT", self.log.read_text())

    def test_matrix_preserves_failure_and_continues_other_scenes(self):
        args = argparse.Namespace(godot=Path(sys.executable), out=self.root, captures=False,
                                  project=self.root, case=["archive_smoke", "scene_play_flow_smoke"], timeout=2)
        def command(command, log, timeout):
            if "archive_smoke" in str(log):
                raise ValueError("synthetic log regression")
        with patch.object(godot_matrix, "run_logged", side_effect=command):
            self.assertEqual(godot_matrix.matrix(args), 1)
        results = json.loads((self.root / "results.json").read_text())
        self.assertEqual([row["status"] for row in results], ["FAIL", "PASS"])
        self.assertIn("synthetic log regression", results[0]["error"])

    def test_isolation_across_cases_invocations_and_nested_drivers(self):
        source = self.root / "source"
        source.mkdir()
        (source / "project.godot").write_text('config_version=5\n[application]\nconfig/name="fixture"\n')
        args = argparse.Namespace(godot=Path(sys.executable), out=self.root / "logs", captures=False,
                                  project=source, case=["archive_smoke", "scene_play_flow_smoke",
                                                        "creature_animation", "creature_dispatch",
                                                        "creature_freed_callable"], timeout=2)
        seen = []
        staged_paths = []
        def command(command, log, timeout):
            staged = Path(command[command.index("--project" if "--project" in command else "--path") + 1])
            staged_paths.append(staged)
            if "--user-dir-name" in command:
                name = command[command.index("--user-dir-name") + 1]
                godot_isolation.configure_user_dir(staged, name)
            text = (staged / "override.cfg").read_text()
            name = json.loads(text.split("config/custom_user_dir_name=")[1].splitlines()[0])
            self.assertNotIn(name, seen)
            seen.append(name)
            user = godot_isolation.data_home() / name
            self.assertFalse(user.exists())
            user.mkdir(parents=True)
            self.assertFalse((user / "missing-save.json").exists())
            (user / "fixed-fixture").write_text("previous case must never see this")
        with patch.object(godot_matrix, "run_logged", side_effect=command):
            for _ in range(2):
                self.assertEqual(godot_matrix.matrix(args), 0)
        self.assertEqual(len(seen), 10)
        self.assertTrue(all(not path.exists() for path in staged_paths))
        self.assertTrue(all(not (godot_isolation.data_home() / name).exists() for name in seen))
        self.assertFalse((source / "override.cfg").exists())

    def test_user_data_cleanup_on_failure(self):
        with self.assertRaisesRegex(ValueError, "failed engine"):
            with godot_isolation.isolated_project() as (project, name, user):
                user.mkdir(parents=True)
                (user / "fixture").write_text("synthetic")
                raise ValueError("failed engine")
        self.assertFalse(user.exists())
        self.assertFalse(project.exists())

    def test_existing_log_cli_rejects_errors(self):
        self.log.parent.mkdir()
        self.log.write_text("PASS\nERROR: engine API misuse\n")
        result = subprocess.run([sys.executable, str(Path(godot_matrix.__file__)), "check-log", str(self.log)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("engine API misuse", result.stderr)

    def test_private_playthrough_rejects_zero_exit_errors(self):
        disc = self.root / "synthetic-empty-folder"
        disc.mkdir()
        executable = self.root / "fake-godot"
        for diagnostic, pickup, expected in (("", True, 0), ("SCRIPT ERROR: Invalid call", True, 1), ("", False, 1)):
            with self.subTest(diagnostic=diagnostic, pickup=pickup):
                executable.write_text(
                    f"#!{sys.executable}\nimport json, pathlib, sys\n"
                    "out = pathlib.Path(next(a.split('=', 1)[1] for a in sys.argv if a.startswith('--out=')))\n"
                    f"steps = [dict(step='loot_pickup', result='passed', drop_produced=True, picked_up=True, walked=True, inventory_units_before=10, inventory_units_after=11)] if {pickup!r} else []\n"
                    "(out / 'result.json').write_text(json.dumps({'steps': steps, 'refusals': {}}))\n"
                    f"print({diagnostic!r}, file=sys.stderr)\n", encoding="utf-8")
                executable.chmod(0o755)
                output = self.root / ("clean" if not diagnostic else "errors")
                result = subprocess.run([
                    sys.executable, str(Path(godot_matrix.__file__).with_name("run_loop_playthrough.py")),
                    "--godot", str(executable), "--game-folder", str(disc), "--out", str(output),
                ], capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, expected, result.stderr)
                summary = json.loads((output / "run-summary.json").read_text())
                self.assertTrue(all(row["exit_code"] == 0 for row in summary["variants"].values()))
                self.assertEqual(summary["variants"]["headless"]["log_errors"], int(bool(diagnostic)))
                self.assertEqual(summary["variants"]["headless"]["pickup_verified"], pickup)


if __name__ == "__main__":
    unittest.main()
