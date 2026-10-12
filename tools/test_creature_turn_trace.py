"""Exercise creature turn trace transport refusals and filesystem safety."""
import argparse
import gzip
import pathlib
import subprocess
import tempfile
import unittest

NATIVE = None

class CreatureTurnTransport(unittest.TestCase):
    def setUp(self):
        if NATIVE is None:
            self.skipTest("native creature turn trace path is supplied by its dedicated CTest case")
        self.temp = tempfile.TemporaryDirectory(prefix="Chämpions 测试 ")
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        fixture = pathlib.Path(__file__).resolve().parent.parent / "native/tests/vectors/creature_turn.tsv.gz"
        with gzip.open(fixture, "rb") as stream:
            self.data = stream.read()
        self.input = self.root / "vectors.tsv"
        self.output = self.root / "trace.tsv"

    def run_native(self, data, output=None):
        self.input.write_bytes(data)
        return subprocess.run([str(NATIVE), str(self.input), str(output or self.output)],
                              capture_output=True, timeout=600)

    def refused(self, data):
        self.output.write_bytes(b"previous verified trace")
        result = self.run_native(data)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.output.read_bytes(), b"previous verified trace")
        self.assertFalse(list(self.root.glob(".creature-turn-trace-*")))
        self.assertEqual(self.input.read_bytes(), data)

    def test_unicode_path_complete(self):
        self.output.write_bytes(b"previous verified trace")
        result = self.run_native(self.data)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
        self.assertIn(b"exact", result.stdout)
        self.assertEqual(self.output.read_bytes(), self.data.split(b"\n", 1)[1])
        self.assertFalse(list(self.root.glob(".creature-turn-trace-*")))

    def test_empty_and_malformed(self):
        for data in (b"", b"invalid\n", b"# creature_turn_vectors_v1\nwrong\n"):
            with self.subTest(data=data):
                self.refused(data)

    def test_truncated_and_duplicate(self):
        lines = self.data.splitlines(keepends=True)
        for data in (lines[0]+lines[1][:-1], lines[0]+lines[1], lines[0]+lines[1]+lines[1]):
            with self.subTest(size=len(data)):
                self.refused(data)

    def test_wrong_expected_result(self):
        lines = self.data.splitlines(keepends=True)
        columns = lines[1].rstrip(b"\n").split(b"\t")
        columns[4] = b"[wrong]"
        self.refused(lines[0]+b"\t".join(columns)+b"\n")

    def test_ascii_lf_required(self):
        for data in (self.data.replace(b"\n", b"\r\n", 1), b"# creature_turn_vectors_v1\n\xff\n"):
            with self.subTest(data=data[:30]):
                self.refused(data)

    def test_same_path_does_not_overwrite(self):
        result = self.run_native(self.data, self.input)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

    def test_hardlink_does_not_overwrite(self):
        self.input.write_bytes(self.data)
        self.output.hardlink_to(self.input)
        result = subprocess.run([str(NATIVE), str(self.input), str(self.output)],
                                capture_output=True, timeout=600)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

    def test_directory_output_refused(self):
        result = self.run_native(self.data, self.root)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

    def test_limited_output_file_refused(self):
        try:
            import resource
            import signal
        except ImportError:
            self.skipTest("POSIX file-size limit is unavailable on this host")
        self.input.write_bytes(self.data)
        self.output.write_bytes(b"previous verified trace")
        def limit_output():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))
        result = subprocess.run([str(NATIVE), str(self.input), str(self.output)],
                                capture_output=True, timeout=600,
                                preexec_fn=limit_output, restore_signals=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

        self.assertEqual(self.output.read_bytes(), b"previous verified trace")
        self.assertFalse(list(self.root.glob(".creature-turn-trace-*")))

    def test_output_write_failure_refused(self):
        destination = pathlib.Path("/dev/full")
        if not destination.exists():
            self.skipTest("write-failure device is unavailable on this host")
        result = self.run_native(self.data, destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

    def test_noncanonical_and_inconsistent_identity(self):
        lines = self.data.splitlines(keepends=True)
        base = lines[1].rstrip(b"\n").split(b"\t")
        for index, value in ((0, b"unknown"), (1, b"00"), (1, b"16"),
                             (2, b"0;g0"), (2, b"1;00"), (3, b"failed")):
            with self.subTest(index=index, value=value):
                columns = list(base)
                columns[index] = value
                self.refused(lines[0] + b"\t".join(columns) + b"\n")

    def changed_input(self, inputs):
        import json
        lines = self.data.splitlines(keepends=True)
        cols = lines[1].rstrip(b"\n").split(b"\t")
        cols[2] = b"0;" + json.dumps(inputs, separators=(",", ":")).encode("ascii")
        return self.run_native(lines[0] + b"\t".join(cols) + b"\n")

    def first_input(self):
        import json
        return json.loads(self.data.splitlines()[1].split(b"\t")[2].split(b";", 1)[1])

    def test_canonical_dsl_and_depth(self):
        lines = self.data.splitlines(keepends=True)
        cols = lines[1].rstrip(b"\n").split(b"\t")
        for source in (b"[+1]", b"[-0]", b"[00]", b"[1,]", b"[1]x", b"[_]", b"[ 1]",
                       b"["*12+b"0"+b"]"*12, b"["+b"0,"*2048+b"0]"):
            with self.subTest(prefix=source[:30]):
                changed = list(cols)
                changed[2] = b"0;" + source
                result = self.run_native(lines[0] + b"\t".join(changed) + b"\n")
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn(b"mismatch at row", result.stderr)

    def test_turn_dsl_shapes_and_ranges(self):
        malformed = (
            [[], [[9, 0, 0, 0]]],
            [[], [[0, 0, 0, [0, 0]]]],
            [[], [[0, 0, 2147483648, [0, 0, 0]]]],
            [[], [[0, 0, -2147483649, [0, 0, 0]]]],
            [[], [[0, 0, 0, [4294967296, 0, 0]]]],
            [[], [[0, -1, 0, [0, 0, 0]]]],
            [[], [[1, 0, 9, 0]]],
            [[], [[2, 4294967295, 2, 0]]],
            [[[0, 1, -1]], []],
            [[[0, 1, 18446744073709551616]], []],
            [[[0, 0, 0]] * 513, []],
            [[], [[1, 0, 0, 0]] * 129],
            [[], [], []],
        )
        for value in malformed:
            with self.subTest(value=str(value)[:100]):
                result = self.changed_input(value)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn(b"mismatch at row", result.stderr)

    def test_input_drives_actual_state(self):
        inputs = self.first_input()
        # Add a previously absent byte. A runner copying expected snapshots would
        # miss this; the actual model must expose it in initial and final state.
        inputs[0].append([0x70000000, 1, 123])
        result = self.changed_input(inputs)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"mismatch at row", result.stderr)

    def test_coherently_changed_input_reaches_digest_guard(self):
        import json
        rows = self.data.splitlines(keepends=True)
        columns = rows[1].rstrip(b"\n").split(b"\t")
        inputs = json.loads(columns[2].split(b";", 1)[1])
        old = json.dumps(inputs, separators=(",", ":")).encode("ascii")
        # Zero-width poke changes the submitted input, but no model byte or log.
        inputs[0].append([0, 0, 0])
        new = json.dumps(inputs, separators=(",", ":")).encode("ascii")
        columns[2] = b"0;" + new
        self.assertTrue(columns[4].startswith(b"[" + old + b","))
        columns[4] = b"[" + new + columns[4][len(old)+1:]
        rows[1] = b"\t".join(columns) + b"\n"
        self.output.write_bytes(b"previous verified trace")
        result = self.run_native(b"".join(rows))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"incomplete or changed", result.stderr)
        self.assertEqual(self.output.read_bytes(), b"previous verified trace")

    def test_symlink_to_unrelated_trace_preserved(self):
        target = self.root / "unrelated.tsv"
        target.write_bytes(b"unrelated verified trace")
        try:
            self.output.symlink_to(target)
        except OSError as error:
            self.skipTest("symlinks unavailable: " + str(error))
        result = self.run_native(self.data)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(target.read_bytes(), b"unrelated verified trace")

    def test_symlink_alias_does_not_overwrite(self):
        self.input.write_bytes(self.data)
        try:
            self.output.symlink_to(self.input)
        except OSError as error:
            self.skipTest("symlinks unavailable: " + str(error))
        result = subprocess.run([str(NATIVE), str(self.input), str(self.output)],
                                capture_output=True, timeout=600)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

    def test_fixture_size_budget(self):
        with self.input.open("wb") as stream:
            stream.truncate(16 * 1024 * 1024 + 1)
        self.output.write_bytes(b"keep existing trace")
        result = subprocess.run([str(NATIVE), str(self.input), str(self.output)],
                                capture_output=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"budget", result.stderr)
        self.assertEqual(self.output.read_bytes(), b"keep existing trace")
        self.assertEqual(self.input.stat().st_size, 16 * 1024 * 1024 + 1)

    def test_line_size_budget(self):
        result = self.run_native(b"# creature_turn_vectors_v1\n" +
                                 b"x" * (64 * 1024 + 1) + b"\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"line exceeds budget", result.stderr)

    def test_missing_input(self):
        result = subprocess.run([str(NATIVE), str(self.input), str(self.output)],
                                capture_output=True, timeout=600)
        self.assertNotEqual(result.returncode, 0)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True, type=pathlib.Path)
    args, rest = parser.parse_known_args()
    NATIVE = args.native.resolve()
    unittest.main(argv=["test_creature_turn_trace.py", *rest])
