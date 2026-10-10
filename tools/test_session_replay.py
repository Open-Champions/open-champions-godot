"""Exercise authored replay transport refusals and filesystem safety."""
import argparse
import gzip
import pathlib
import subprocess
import tempfile
import unittest

NATIVE = None

class ReplayTransport(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="Chämpions 测试 ")
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        fixture = pathlib.Path(__file__).resolve().parent.parent / "native/tests/vectors/session_replay.tsv.gz"
        with gzip.open(fixture, "rb") as stream:
            self.data = stream.read()
        self.input = self.root / "vectors.tsv"
        self.output = self.root / "trace.tsv"

    def run_native(self, data, output=None):
        self.input.write_bytes(data)
        return subprocess.run([str(NATIVE), str(self.input), str(output or self.output)],
                              capture_output=True, timeout=60)

    def refused(self, data):
        result = self.run_native(data)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), data)

    def test_unicode_path_complete(self):
        result = self.run_native(self.data)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
        self.assertIn(b"768 exact", result.stdout)

    def test_empty_and_malformed(self):
        for data in (b"", b"invalid\n", b"# session_vectors_v1\nwrong\n"):
            with self.subTest(data=data):
                self.refused(data)

    def test_truncated_and_duplicate(self):
        lines = self.data.splitlines(keepends=True)
        for data in (self.data[:-1], b"".join(lines[:-1]), lines[0]+lines[1]+lines[1]):
            with self.subTest(size=len(data)):
                self.refused(data)

    def test_wrong_expected_result(self):
        lines = self.data.splitlines(keepends=True)
        columns = lines[1].rstrip(b"\n").split(b"\t")
        columns[4] = b"[wrong]"
        self.refused(lines[0]+b"\t".join(columns)+b"\n")

    def test_ascii_lf_required(self):
        for data in (self.data.replace(b"\n", b"\r\n", 1), b"# session_vectors_v1\n\xff\n"):
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
                                capture_output=True, timeout=60)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.input.read_bytes(), self.data)

    def test_missing_input(self):
        result = subprocess.run([str(NATIVE), str(self.input), str(self.output)],
                                capture_output=True, timeout=60)
        self.assertNotEqual(result.returncode, 0)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True, type=pathlib.Path)
    args, rest = parser.parse_known_args()
    NATIVE = args.native.resolve()
    unittest.main(argv=["test_session_replay.py", *rest])
