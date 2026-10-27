#!/usr/bin/env python3
"""Check arena fixture provenance, then run the native reference comparison."""
import hashlib
import gzip
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from generate_arena_match_vectors import COMMIT, PINS

exe, path = sys.argv[1:]
path = Path(path)
data = path.read_bytes()
raw = gzip.decompress(data)
meta = json.loads(path.with_suffix("").with_suffix(".json").read_text())
if (meta["schema"] != "arena-match-v1" or meta["research_commit"] != COMMIT or
        meta["references"] != PINS or meta["sha256"] != hashlib.sha256(data).hexdigest() or
        meta["content_sha256"] != hashlib.sha256(raw).hexdigest() or
        meta["cases"] != len(raw.splitlines()) - 1):
    raise ValueError("arena fixture provenance mismatch")
with tempfile.TemporaryDirectory(prefix="arena-match-") as folder:
    plain = Path(folder) / "vectors.tsv"
    plain.write_bytes(raw)
    subprocess.run([exe, str(plain)], check=True)
