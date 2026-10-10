"""Authored transport anchors; never requires the research model or disc files."""
import hashlib
import io
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import concurrent.futures
from validate_amx_scripts import compare, read_line, run_script

NATIVE = None


def authored_image():
    # CONST.pri7, HALT0. No names, imported handlers or game content.
    code = struct.pack("<4i", 0x0b, 7, 0x78, 0)
    header = struct.pack("<iHbbHH5i", 80, 0xf1e0, 3, 1, 0, 24,
                         64, 80, 80, 336, 0)
    header += struct.pack("<hxxihxxihxxihxxi", 0, 64, 0, 64, 0, 64, 0, 64)
    return bytes(96) + header + code + bytes(256)


class Transport(unittest.TestCase):
    def test_exact_comparison_accepts_windows_separators(self):
        digest = hashlib.sha256()
        compare("R\t0\t7\r\n", ["R", "0", "7"], digest)
        self.assertEqual(digest.hexdigest(), hashlib.sha256(b"R\t0\t7\n").hexdigest())

    def test_mismatch_does_not_expose_private_values(self):
        with self.assertRaisesRegex(ValueError, r"^private AMX state mismatch at field 1$"):
            compare("R\tprivate script data\n", ["R", "expected private data"], hashlib.sha256())

    def test_loader_refusal_still_requires_successful_transport_exit(self):
        class Machine:
            _parsed = False
            def __init__(self, image): self.image = image
            def load(self): return 17
            def snapshot(self): return ()
        class Reference:
            HostFault = RuntimeError
            Limit = MemoryError
        Reference.Machine = Machine
        data = bytes(160)
        row = "L\t17\t-\t" + ",".join(["N"] * 15) + "\t" + data.hex() + "\n"
        process = mock.Mock(returncode=3, stdin=io.StringIO(), stdout=io.StringIO(row))
        process.poll.return_value = 3
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "authored.scr"
            path.write_bytes(data)
            with mock.patch("validate_amx_scripts.subprocess.Popen", return_value=process):
                with self.assertRaisesRegex(ValueError, "native transport refused"):
                    run_script(path, pathlib.Path("unused"), Reference, 0, 100,
                               hashlib.sha256(), {"load_compared": 0, "load_refused": 0})

    def test_stuck_transport_is_terminated(self):
        reader, process = mock.Mock(), mock.Mock()
        reader.submit.return_value.result.side_effect = concurrent.futures.TimeoutError
        with self.assertRaisesRegex(ValueError, "native transport timed out"):
            read_line(reader, process)
        process.kill.assert_called_once()
        process.wait.assert_called_once()

    def test_unicode_file_and_exact_state(self):
        if not NATIVE: self.skipTest("built transport supplied by CTest")
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "é 測試.scr"
            data = authored_image()
            path.write_bytes(data)
            run = subprocess.run([NATIVE, str(path), "0"], input="R 1 -1 0 99 0 100\n",
                                 text=True, capture_output=True, timeout=5)
            self.assertEqual(run.returncode, 0, run.stderr)
            rows = run.stdout.splitlines()
            self.assertEqual(len(rows), 2)
            expected_image = bytearray(data)
            struct.pack_into("<i", expected_image, 96 + 24, 332)
            saved = "0,0,0,252,252,40960,0,0,0,0,0,N,0,332,0"
            self.assertEqual(rows[0].split("\t"), ["L", "0,0",
                             "80,64,80,80,332,0,0,64,0,64,0,0,176;;",
                             saved, expected_image.hex()])
            self.assertEqual(rows[1].split("\t"), ["R", "0", "7", saved,
                             expected_image.hex(), "-", "11:1,120:1,", "98", "0"])

    def test_refused_magic_and_invalid_commands(self):
        if not NATIVE: self.skipTest("built transport supplied by CTest")
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "authored.scr"
            data = bytearray(authored_image())
            data[100] = 0
            path.write_bytes(data)
            run = subprocess.run([NATIVE, str(path), "0"], text=True,
                                 capture_output=True, timeout=5)
            self.assertEqual(run.returncode, 0)
            self.assertEqual(run.stdout.split("\t")[1], "17")
            path.write_bytes(authored_image())
            run = subprocess.run([NATIVE, str(path), "0"], input="not a command\n",
                                 text=True, capture_output=True, timeout=5)
            self.assertNotEqual(run.returncode, 0)


if __name__ == "__main__":
    if len(sys.argv) == 2 and not sys.argv[1].startswith("-"):
        NATIVE = str(pathlib.Path(sys.argv.pop()).resolve())
    unittest.main()
