#!/usr/bin/env python3
"""Verify provenance and run the exact AntQueen, Skeleton and Soul native comparisons."""
import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_combat_ctors_vectors import COMMIT, PINS

def main():
    exe, path = sys.argv[1:]
    path = Path(path)
    data = path.read_bytes()
    meta_path = path.with_suffix('').with_suffix('.json')
    m = json.loads(meta_path.read_text())
    raw = gzip.decompress(data)
    if (m['schema'] != 'combat-ctors-v1' or m['research_commit'] != COMMIT or m['references'] != PINS
            or m['vector_sha256'] != hashlib.sha256(data).hexdigest()
            or m['content_sha256'] != hashlib.sha256(raw).hexdigest()
            or sum(m['cases'].values()) != raw.count(b'\nCASE ')
            or m['operations'] != raw.count(b'\nOP ') or m['calls'] != raw.count(b'\nCALL ')):
        raise ValueError('combat ctors fixture provenance mismatch')
    with tempfile.TemporaryDirectory(prefix='combat-ctors-') as d:
        p = Path(d) / 'vectors.txt'
        p.write_bytes(raw)
        subprocess.run([exe, str(p)], check=True)


if __name__ == "__main__":
    main()
