#!/usr/bin/env python3
"""Synthetic R5 request oracle; no private input, shipped recipe or game name.

Usage: python3 tools/generate_arena_scheduler_vectors.py $RESEARCH_ARENA_LIVE
Authority: research-arena-live@9a0c26a5 tools/ref_arena_modes.py:325-410.
The pre-existing reference, vectors and fixtures are not modified.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

pin = "9a0c26a5e6037043fa80fe55fa90a03dc0f619c4"
research = Path(sys.argv[1])
assert subprocess.check_output(["git", "-C", str(research), "rev-parse", "HEAD"], text=True).strip() == pin
sys.path.insert(0, str(research / "tools"))
spec = importlib.util.spec_from_file_location("arena_reference", research / "tools/ref_arena_modes.py")
ref = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ref)
# Authored records deliberately repeat class indices, while levels are shuffled.
recipes = [[0x34, 0, 0, 1, 0, 0], [0x31, 2, 3, 2, 0, 0],
           [0x36, 4, 5, 6, 0, 0], [0x30, 7, 7, 7, 0, 0],
           [0x32, 8, 1, 8, 0, 0]]
rows = ["# seed players wave monster_level final_seed classes_count classes... steps_count (count angle draw)..."]
for seed in (20973, 7):
    for players in range(1, 5):
        for wave in range(30):
            classes, final, steps = ref.wave_plan(wave, players, len(recipes), recipes, seed)
            fields = [seed, players, wave, ref.monster_level(wave), final, len(classes), *classes,
                      len(steps), *(v for step in steps for v in step)]
            rows.append(" ".join(str(v) for v in fields))
out = Path(__file__).resolve().parents[1] / "native/tests/vectors/arena_scheduler.tsv"
out.write_text("\n".join(rows) + "\n")
manifest = dict(research_commit=pin, cases=len(rows)-1, recipes=recipes,
                vector_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),
                sources={name: hashlib.sha256((research/name).read_bytes()).hexdigest()
                         for name in ("tools/ref_arena_modes.py", "tools/ref_core.py",
                                      "docs/spec/arena-live-scheduler.md")})
out.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n")
