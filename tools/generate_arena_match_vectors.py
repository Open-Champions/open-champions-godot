#!/usr/bin/env python3
"""Generate authored arena fixtures from pinned research references, never native output."""
import hashlib
import gzip
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

sys.dont_write_bytecode = True
COMMIT = "d4f58da6377f1011068ba80d3608dbe1e3149370"
PINS = {
    "tools/ref_arena_modes.py": "839f500b3e77b4e830b43d4f131177b43a54542a52335c181edc30f455a06cbe",
    "tools/ref_arena_match_core.py": "85280e831eaea93b7ab6bdf90c80d51c785f5f5fe5b29c930edbe3189000a1d2",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
    "tools/ref_objects.py": "541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2",
    "docs/arena-modes.md": "1dafe8b50e9d604ac387a87fe9d3c94490b6026f3eda4501cfd72da607c07a58",
    "docs/spec/arena-match-core.md": "f87fffdbd92aeb832257ff90d03cf1fb15ff92dc097a8e35436c88d1b2588c2a",
    "docs/spec/arena-match-probe.md": "4cc3db76cfa2cc06b5bcfb33ba513cc25477361964e1d72ff662fa3a2440f4d9",
}


def main():
    research = Path(sys.argv[1]).resolve()
    for path, digest in PINS.items():
        blob = subprocess.check_output(["git", "-C", str(research), "show", f"{COMMIT}:{path}"])
        if hashlib.sha256(blob).hexdigest() != digest or hashlib.sha256((research / path).read_bytes()).hexdigest() != digest:
            raise ValueError("arena reference pin mismatch: " + path)
    sys.path.insert(0, str(research / "tools"))
    import ref_arena_modes as R
    import ref_arena_match_core as M
    import ref_core as C
    C.set_fpu("ps2")
    lines = ["# authored synthetic arena fixtures; no executable or disc input"]

    def emit(kind, *values):
        lines.append(kind + " " + " ".join(f"{v & 0xffffffff:x}" for v in values))

    for flags in (0, 2, 8, 0x20, 0x21, 0x22, 0x28, 0x2a, 0x40, 0x60, 0x68, 0x612b):
        for state in (0, 1, 0x7fff, 0x8000, 0xffff):
            kind, _ = M.save_wave_visit(flags, state, 0)
            emit("VISIT", flags, state, M.empty_wave_visit(flags, state, 0) is not None,
                 {"skip": 0, "save": 1, "destroy": 2}[kind])
    for mode in (0, 1):
        for count in (1, 2):
            killers = [(None, 0, 0), (0, 0, 0), (0x40, 0, 0),
                       (0x20, 0, 0), (0x20, 0, 255), (0x60, 0, 0)]
            if count == 2:
                killers.extend([(0x40, 1, 0), (0x20, 0, 1)])
            for flags, slot, owner in killers:
                for amount in (0, 1, 9):
                    for before in (0, 0x7fff, 0xffff):
                        slots = [R.Slot(kills=before) for _ in range(2)]
                        killer = None if flags is None else SimpleNamespace(flags=flags, slot=slot, owner=owner)
                        R.add_kill(slots, killer, amount, mode, count)
                        emit("KILL", mode, count, 0xffffffff if flags is None else flags,
                             slot, owner, amount, before, slots[0].kills, slots[1].kills)
    for timer in (0, C.ONE, C.TWO, 0x3fffffff):
        slot = R.Slot(lives=0xffff, snapshot=2, timer=timer)
        R.tick_overlay([slot])
        emit("OVERLAY", timer, 0xffff, 2, slot.timer, slot.snapshot)
    for mode in (0, 1):
        for prediction in (0, 1):
            for flags, slot, owner in ((None, 0, 0), (0x20, 0, 255), (0x20, 0, 1),
                                      (0x40, 0, 0), (0x40, 1, 0), (0x60, 0, 0),
                                      (0x60, 1, 0), (0, 0, 0)):
                for before in (0, 2, 0xffff):
                    slots = [R.Slot(kills=before, lives=before) for _ in range(2)]
                    killer = None if flags is None else SimpleNamespace(flags=flags, slot=slot, owner=owner)
                    R.on_killed(slots, 0, killer, mode, 2, prediction)
                    emit("KILLED", mode, prediction, 0xffffffff if flags is None else flags,
                         slot, owner, before, slots[0].kills, slots[0].lives, slots[1].kills)
    # Authored class indices, never the shipped recipe. Keep every ring group
    # nonempty, as the audited reference's executed domain requires.
    recipe = bytes((0x30, 11, 12, 13, 14, 15))
    for wave in (0, 1, 2, 3, 5, 8, 30):
        for active in (1, 4):
            count, _quot, rem = R.spawn_budget(wave, active)
            classes = R.recipe_groups(recipe, rem, count)
            emit("WAVE", wave, active, R.monster_level(wave), len(classes), *recipe, *classes)
    data = ("\n".join(lines) + "\n").encode()
    out = Path(__file__).resolve().parents[1] / "native/tests/vectors/arena_match.tsv.gz"
    blob = gzip.compress(data, mtime=0)
    out.write_bytes(blob)
    out.with_suffix("").with_suffix(".json").write_text(json.dumps({
        "schema": "arena-match-v1", "research_commit": COMMIT, "references": PINS,
        "sha256": hashlib.sha256(blob).hexdigest(), "content_sha256": hashlib.sha256(data).hexdigest(),
        "cases": len(lines) - 1,
    }, indent=2) + "\n")
    print(f"{len(lines)-1} synthetic arena vectors")


if __name__ == "__main__":
    main()
