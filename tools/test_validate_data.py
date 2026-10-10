"""Public CLI checks without disc data. Real file validation is opt-in."""
import pathlib
import struct
import subprocess
import sys
import tempfile


def main():
    exe = pathlib.Path(sys.argv[1]).resolve()

    def run(*args):
        return subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True)

    assert run("--help").returncode == 0
    assert run("--game", "norrath", ".").returncode == 2
    with tempfile.TemporaryDirectory(prefix="Champions 测试 ") as temporary:
        root = pathlib.Path(temporary)
        assert run("--game", "rta", root).returncode == 1
        # An authored empty LMP is accepted; malformed inputs count as refusals.
        (root / "empty.LMP").write_bytes(struct.pack("<I", 0))
        sub = root / "nested"
        sub.mkdir()
        for extension in ("ANM", "tex", "ETEX", "VIF"):
            (sub / f"bad.{extension}").write_bytes(b"\x00")
        (sub / "ignored.bin").write_bytes(b"\x00")
        # Never follow linked files or directories into unrelated private data.
        outside = root.parent / (root.name + "-outside")
        outside.mkdir()
        try:
            (outside / "linked.anm").write_bytes(b"\x00")
            try:
                (root / "link").symlink_to(outside, target_is_directory=True)
                (root / "linked.vif").symlink_to(sub / "bad.VIF")
            except OSError:
                pass  # Windows symlink privileges are not needed by this test.
            result = run("--game", "rta", root)
            assert result.returncode == 0, result.stderr
            for line in ("archives\t1\t0\t1", "animations\t0\t1\t1",
                         "models\t0\t1\t1", "textures\t0\t2\t2"):
                assert line in result.stdout, result.stdout
            assert str(root) not in result.stdout
            assert "bad." not in result.stdout
            assert run("--game", "rta", sub / "bad.VIF").returncode == 1
        finally:
            (outside / "linked.anm").unlink()
            outside.rmdir()
    print("Private-directory validation CLI checks passed")


if __name__ == "__main__":
    main()
