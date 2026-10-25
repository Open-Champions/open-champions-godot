#!/usr/bin/env python3
"""Verify provenance and run the exact round-2 native comparisons without research or game data.

CLI (CTest combat-runtime-2-exact): test_combat_runtime_2_vectors.py EXE VECTORS.txt.gz
Import-safe: `python3 -m unittest discover -s tools -p 'test_*.py'` imports this module and
runs only the provenance check of the committed vectors; the native comparison needs EXE.
"""
import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_combat_runtime_2_vectors import COMMIT, PINS

VECTORS = Path(__file__).resolve().parents[1] / 'native' / 'tests' / 'vectors' / 'combat_runtime_2.txt.gz'


def verified(path):
    """The decompressed vectors, after the manifest provenance check."""
    path = Path(path)
    data = path.read_bytes()
    m = json.loads(path.with_suffix('').with_suffix('.json').read_text())
    raw = gzip.decompress(data)
    if (m['schema'] != 'combat-runtime-2-v1' or m['research_commit'] != COMMIT or m['references'] != PINS
            or m['vector_sha256'] != hashlib.sha256(data).hexdigest()
            or m['content_sha256'] != hashlib.sha256(raw).hexdigest() or m['cases'] != raw.count(b'\nCASE ')):
        raise ValueError('combat round-2 fixture provenance mismatch')
    return raw


def main(argv):
    exe, path = argv
    raw = verified(path)
    with tempfile.TemporaryDirectory(prefix='combat-runtime-2-') as d:
        p = Path(d) / 'vectors.txt'
        p.write_bytes(raw)
        subprocess.run([exe, str(p)], check=True)


class Provenance(unittest.TestCase):
    @unittest.skipUnless(VECTORS.exists(), 'combat_runtime_2.txt.gz is not committed (over 5 MB); '
                         'regenerate it locally, see native/tests/vectors/README.md')
    def test_committed_vectors(self):
        self.assertIn(b'\nCASE ', verified(VECTORS))


if __name__ == '__main__':
    main(sys.argv[1:])
