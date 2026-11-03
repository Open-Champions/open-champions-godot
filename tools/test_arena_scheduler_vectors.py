#!/usr/bin/env python3
"""Validate the synthetic R5 oracle before driving the actual native runtime."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from generate_arena_scheduler_vectors import COMMIT, PINS, RECIPES

exe, filename = sys.argv[1:]
path = Path(filename)
data = path.read_bytes()
meta = json.loads(path.with_suffix(".json").read_text())
if (meta["research_commit"] != COMMIT or meta["sources"] != PINS or
        meta["recipes"] != RECIPES or meta["cases"] != 240 or
        meta["cases"] != len(data.splitlines()) - 1 or
        meta["vector_sha256"] != hashlib.sha256(data).hexdigest()):
    raise ValueError("arena scheduler oracle provenance mismatch")
subprocess.run([exe, str(path)], check=True)
