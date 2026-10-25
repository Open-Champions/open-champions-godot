#!/usr/bin/env python3
"""Check the pinned fixture manifest before executing native exact-state tests."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from generate_arena_monsters_vectors import COMMIT, PINS, PORTED_UNITS

def main():
    executable, path = sys.argv[1:]
    vector = Path(path)
    data = vector.read_bytes()
    manifest = json.loads(vector.with_suffix('.json').read_text())
    if (manifest['schema'] != 'arena-monsters-v1' or manifest['research_commit'] != COMMIT
            or manifest['references'] != PINS
            or manifest['reference_function_count'] != 78
            or manifest['ported_units'] != PORTED_UNITS
            or manifest['vector_sha256'] != hashlib.sha256(data).hexdigest()
            or not data.startswith(b'# arena-monsters-v1\n')
            or len(data.splitlines())-1 != manifest['cases']):
        raise ValueError('arena vector provenance, count or integrity mismatch')
    subprocess.run([executable, str(vector)], check=True)

if __name__ == '__main__':
    main()
