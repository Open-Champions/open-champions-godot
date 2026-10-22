#!/usr/bin/env python3
"""Verify provenance of the wading corpus, then run the native contract test on it."""
import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
sys.dont_write_bytecode = True

COMMIT = '942a6bac3543ad14ab350b210da65dc51e9c695a'


def main():
    exe, path = sys.argv[1:]
    path = Path(path)
    data = path.read_bytes()
    manifest = json.loads(path.with_suffix('').with_suffix('.json').read_text())
    raw = gzip.decompress(data)
    if (manifest['schema'] != 'player-wading-v1' or manifest['research_commit'] != COMMIT
            or manifest['vector_sha256'] != hashlib.sha256(data).hexdigest()
            or manifest['content_sha256'] != hashlib.sha256(raw).hexdigest()
            or manifest['cases'] != raw.count(b'\n')):
        raise ValueError('player wading fixture provenance mismatch')
    with tempfile.TemporaryDirectory(prefix='player-wading-') as d:
        p = Path(d) / 'vectors.txt'
        p.write_bytes(raw)
        subprocess.run([exe, str(p)], check=True)


if __name__ == '__main__':
    main()
