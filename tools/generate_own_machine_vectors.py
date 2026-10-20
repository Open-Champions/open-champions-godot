#!/usr/bin/env python3
"""Write the locked own-machine ticks as a synthetic text fixture.

The tuples come from the research reference. This script does not copy
retail bytes. Run it with the research tools directory on disk; the committed
fixture is what the native test reads.
"""
import sys
import unittest

unittest  # imported before the research tools directory can shadow stdlib dis


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_own_machine_vectors.py RESEARCH_TOOLS OUT")
    sys.path.insert(0, sys.argv[1])
    import test_ref_own_machines as T

    bools = {
        "seen", "away", "see", "enemy", "ally", "rooted", "ready", "stunned",
        "arrived", "demon", "path", "target", "owner", "idle", "move",
        "fidget_a", "fidget_b", "flee_clip", "snap",
    }
    lines = [
        "# Synthetic own-machine ticks. Defaults match the reference tick:",
        "# clock 1000, queue 1, other fields 0. 253 cases.",
    ]
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
