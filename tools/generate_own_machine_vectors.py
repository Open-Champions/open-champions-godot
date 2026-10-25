#!/usr/bin/env python3
"""Write the locked own-machine ticks as a synthetic text fixture.

The tuples come from the research reference at the commit pinned in
native/tests/vectors/creature_dispatch_sources.json; the two research files
are read with `git show <pin>:<path>` and checked against their sha256 there.
This script does not copy retail bytes. The committed fixture is what the
native test reads.

usage: generate_own_machine_vectors.py RESEARCH_REPO OUT
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

unittest  # imported before the research tools directory can shadow stdlib dis

ROOT = pathlib.Path(__file__).resolve().parents[1]
PINS = ROOT / "native/tests/vectors/creature_dispatch_sources.json"
FILES = ("tools/ref_own_machines.py", "tools/test_ref_own_machines.py")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_own_machine_vectors.py RESEARCH_REPO OUT")
    pins = json.loads(PINS.read_text())
    tmp = tempfile.mkdtemp(prefix="own-machines-")
    shas = {}
    for name in FILES:
        data = subprocess.check_output(
            ["git", "-C", sys.argv[1], "show", pins["commit"] + ":" + name])
        digest = hashlib.sha256(data).hexdigest()
        if digest != pins["sha256"][name]:
            raise SystemExit("pin mismatch " + name)
        shas[name] = digest
        (pathlib.Path(tmp) / pathlib.Path(name).name).write_bytes(data)
    sys.path.insert(0, tmp)
    import test_ref_own_machines as T

    bools = {
        "seen", "away", "see", "enemy", "ally", "rooted", "ready", "stunned",
        "arrived", "demon", "path", "target", "owner", "idle", "move",
        "fidget_a", "fidget_b", "flee_clip", "snap",
    }
    lines = [
        "# Synthetic own-machine ticks. Defaults match the reference tick:",
        "# clock 1000, queue 1, other fields 0. 253 cases.",
        "# research-anim %s" % pins["commit"],
    ] + ["# %s sha256 %s" % (name, shas[name]) for name in FILES]
    if len(T.LOCKED) != 253:
        raise SystemExit("expected 253 locked ticks, got %s" % len(T.LOCKED))
    for name, state, ctx, req, calls, words in T.LOCKED:
        lines.append("CASE %s %s" % (name, state))
        for key, value in ctx.items():
            if key == "seq":
                for seq_name, vals in value.items():
                    lines.append(
                        "SEQ %s %s" % (seq_name, " ".join(str(int(v)) for v in vals))
                    )
            elif key in bools:
                lines.append("F %s %d" % (key, 1 if value else 0))
            else:
                lines.append("F %s %d" % (key, int(value)))
        for n in req:
            lines.append("REQ %d" % int(n))
        for call in calls:
            lines.append("CALL %s" % call)
        for off, val in words.items():
            lines.append("WORD %d %d" % (int(off), int(val)))
        lines.append("END")
    with open(sys.argv[2], "w", encoding="utf-8") as out:
        out.write("\n".join(lines) + "\n")
    print("wrote %d cases" % len(T.LOCKED))


if __name__ == "__main__":
    main()
