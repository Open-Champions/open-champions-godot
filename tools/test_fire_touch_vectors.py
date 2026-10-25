#!/usr/bin/env python3
"""Verify provenance and run the fire-touch rows through the native combat-damage harness."""
import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_fire_touch_vectors import COMMIT, PINS

def main():
    exe, path = sys.argv[1:]
    path = Path(path)
    data = path.read_bytes()
    m = json.loads(path.with_suffix('').with_suffix('.json').read_text())
    raw = gzip.decompress(data)
    if (m['schema'] != 'fire-touch-v1' or m['research_commit'] != COMMIT or m['references'] != PINS
            or m['gzip_sha256'] != hashlib.sha256(data).hexdigest()
            or m['vector_sha256'] != hashlib.sha256(raw).hexdigest()
            or m['rows'] != raw.count(b'\ndamage\t') + raw.startswith(b'damage\t')
            or not raw.startswith(b'# fire_touch_vectors_v1\n')):
        raise ValueError('fire touch fixture provenance mismatch')
    with tempfile.TemporaryDirectory(prefix='fire-touch-') as d:
        p = Path(d) / 'vectors.tsv'
        p.write_bytes(raw)
        out = subprocess.run([exe, str(p)], check=True, capture_output=True, text=True).stdout
        print(out.strip())
        if not out.startswith(f"{m['rows']} exact damage vectors passed"):
            raise SystemExit('fire touch row count mismatch')


if __name__ == "__main__":
    main()
