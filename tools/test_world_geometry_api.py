"""Exercise geometry ownership and allocation limits with authored public bytes."""
import gzip
import pathlib
import subprocess
import sys
import tempfile


def main():
    exe = pathlib.Path(sys.argv[1]).resolve()
    corpus = pathlib.Path(__file__).resolve().parents[1] / "native/tests/vectors/world_geometry.tsv.gz"
    with gzip.open(corpus, "rt", encoding="ascii") as source:
        next(source)
        row = next(line.rstrip("\n").split("\t") for line in source
                   if line.split("\t")[3] == "ok")
    data = bytes.fromhex(row[2].split(";", 1)[1])
    with tempfile.TemporaryDirectory(prefix="Champions geometry ") as temporary:
        # The C++ test reads argv paths through the platform's ordinary stream.
        # Unicode filesystem transport is exercised by the dedicated trace gate.
        fixture = pathlib.Path(temporary) / "authored.world"
        fixture.write_bytes(data)
        result = subprocess.run([str(exe), str(fixture)], capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stderr
        assert fixture.read_bytes() == data
        print(result.stdout, end="")


if __name__ == "__main__":
    main()
