#!/usr/bin/env python3
"""Verify provenance and run the exact g_musicVolume native comparisons."""
import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_music_volume_vectors import COMMIT, PINS, SEED

def main():
    exe, path = sys.argv[1:]
    path = Path(path)
    data = path.read_bytes()
    meta_path = path.with_suffix('').with_suffix('.json')
    m = json.loads(meta_path.read_text())
    raw = gzip.decompress(data)
    records = sum(1 for line in raw.splitlines() if line and not line.startswith(b'#'))
    if (m['schema'] != 'music-volume-v1' or m['research_commit'] != COMMIT or m['references'] != PINS
            or m['seed'] != SEED or m['vector_sha256'] != hashlib.sha256(data).hexdigest()
            or m['content_sha256'] != hashlib.sha256(raw).hexdigest() or m['records'] != records):
        raise ValueError('music volume fixture provenance mismatch')
    with tempfile.TemporaryDirectory(prefix='music-volume-') as d:
        p = Path(d) / 'vectors.txt'
        p.write_bytes(raw)
        out = subprocess.run([exe, str(p)], check=True, capture_output=True, text=True).stdout
        print(out, end='')
        if f'{records} records' not in out:
            raise ValueError('the native replay did not consume every record')


if __name__ == "__main__":
    main()
