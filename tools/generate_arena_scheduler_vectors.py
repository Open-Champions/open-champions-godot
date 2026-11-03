#!/usr/bin/env python3
"""Synthetic R5 request oracle; no private input, shipped recipe or game name.

Usage: python3 tools/generate_arena_scheduler_vectors.py $RESEARCH_ARENA_LIVE
Authority: research-arena-live@9a0c26a5 tools/ref_arena_modes.py:325-410.
The pre-existing reference, vectors and fixtures are not modified.
"""
import hashlib
import types
import json
from pathlib import Path
import subprocess
import sys

COMMIT = "9a0c26a5e6037043fa80fe55fa90a03dc0f619c4"
PINS = {
    "tools/ref_arena_modes.py": "839f500b3e77b4e830b43d4f131177b43a54542a52335c181edc30f455a06cbe",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
    "docs/spec/arena-live-scheduler.md": "a72930dfbc7cb28e4a57243227ad3fc213be4e1f37a7e2753791807a243b1928",
}
# Authored records deliberately repeat class indices, while levels are shuffled.
RECIPES = [[0x34, 0, 0, 1, 0, 0], [0x31, 2, 3, 2, 0, 0],
           [0x36, 4, 5, 6, 0, 0], [0x30, 7, 7, 7, 0, 0],
           [0x32, 8, 1, 8, 0, 0]]


def main():
    research = Path(sys.argv[1])
    sources = {name: subprocess.check_output(["git", "-C", str(research), "show", COMMIT + ":" + name])
               for name in PINS}
    for name, data in sources.items():
        if hashlib.sha256(data).hexdigest() != PINS[name]:
            raise ValueError("pinned arena source digest mismatch: " + name)
    # Execute only checked committed blobs, never the mutable research working
    # tree, and never write bytecode or any other file in that research checkout.
    core = types.ModuleType("ref_core")
    sys.modules["ref_core"] = core
    exec(compile(sources["tools/ref_core.py"], "ref_core.py@" + COMMIT, "exec"), core.__dict__)
    ref = types.ModuleType("arena_reference")
    exec(compile(sources["tools/ref_arena_modes.py"], "ref_arena_modes.py@" + COMMIT, "exec"), ref.__dict__)
    recipes = RECIPES
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
    manifest = dict(research_commit=COMMIT, cases=len(rows)-1, recipes=recipes,
                    vector_sha256=hashlib.sha256(out.read_bytes()).hexdigest(), sources=PINS)
    out.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
