#!/usr/bin/env python3
"""Execute pinned blood1 class runs on synthetic targeted state-6 frames.

Only synthetic inputs, call identities and resulting synthetic positions are
written. Original executable bytes stay read-only and are never exported.
Downstream services, including turning, grid and hit dispatch, are stubs.
"""
import argparse
import hashlib
import pathlib
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--research", type=pathlib.Path, required=True)
    parser.add_argument("--executable", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    revision = subprocess.check_output(
        ["git", "-C", str(args.research), "rev-parse", "HEAD"], text=True).strip()
    assert revision == "5107fcb1aa7fead20cdeca8693feaabf4cc2c4f7"
    sys.path.insert(0, str(args.research / "tools"))
    import difftest_blood1_enemies as original
    from ee_emu import float_to_raw

    assert hashlib.sha256(args.executable.read_bytes()).hexdigest() == original.SHA256
    names = ("Kobold", "Koboldtough", "koboldshaman", "WarWraith")
    calls = {0x110D30: "damage", 0x112948: "run", 0x1162B0: "step",
             0x115EB8: "knockback", 0x10DED0: "target", 0x10E2C0: "ally",
             0x119E38: "settle", 0x1187E0: "turn", 0x119938: "hitArc",
             0x11B2A8: "holder", 0x11B180: "descriptor",
             0x1196E8: "hitReach", 0x14F028: "grid",
             0x27FF6C: "setState", 0x2C1648: "setState"}
    rows = []
    for seed in (20973, 7):
        for kind, name in enumerate(names):
            family = "WarWraith" if kind == 3 else "Kobold"
            for hit in (0, 1):
                for complete in (0, 1):
                    for fade in (98, 100):
                        for height, target_height in ((0., 42.), (6., 5.),
                                                      (6., 6.), (6., 6.0625),
                                                      (-6., -42.)):
                            emu, obj, _ = original.make(str(args.executable), name,
                                                        seed, base=False)
                            emu.follow = {original.R.RUN[family]}
                            emu.stub(0x10C0F0, ret=1)
                            emu.stub(0x1162B0, ret=hit)
                            target, node = emu.alloc(0x500), emu.alloc(20)
                            emu.mem.w32(node + 12, target)
                            emu.mem.w32(obj + 0x234, node)
                            emu.mem.w16(obj + 0x232, 6)
                            emu.mem.w16(obj + 0x11A, 0 if complete else 8)
                            for offset, value in ((0x10, 12.), (0x14, 24.),
                                                  (0x18, height), (0x228, 100.),
                                                  (0x22C, 100.)):
                                emu.mem.wf32(obj + offset, value)
                            emu.mem.wf32(target + 0x18, target_height)
                            emu.mem.w8(obj + 0x3C8, fade)
                            grid_positions = []
                            emu.stub(0x14F028, function=lambda current:
                                     grid_positions.append(current.mem.u32(obj + 0x18)) or 0)
                            emu.reset_log()
                            emu.call(original.R.RUN[family], obj, max_steps=200000)
                            observed = [calls[call.target] for call in emu.calls
                                        if call.target != 0x10C0F0]
                            assert observed.count("setState") == complete, (name, hit, complete, observed)
                            transitions = [call.args[1] for call in emu.calls
                                           if call.target == original.R.SET_STATE[family]]
                            assert transitions == ([3 if kind == 3 else 5] if complete else [])
                            hits = [call for call in emu.calls
                                    if call.target in (0x119938, 0x1196E8)]
                            assert len(hits) == hit
                            if hit:
                                assert hits[0].args[4 if kind == 3 else 6] == (
                                    8 if kind == 3 else original.R.KOBOLD_HIT_FLAGS[name])
                            result = emu.mem.u32(obj + 0x18)
                            assert grid_positions == ([result] if kind == 3 else [])
                            assert [emu.mem.u32(obj + offset) for offset in (0x10, 0x14)] == [
                                float_to_raw(12.), float_to_raw(24.)]
                            rows.append(" ".join(map(str, (seed, kind, hit, complete, fade,
                                float_to_raw(height), float_to_raw(target_height), result,
                                ",".join(observed)))))
    args.output.write_text("\n".join(rows) + "\n")
    print(f"PASS: {len(rows)} original targeted frames; positions, grid snapshots, calls, hits and transitions")


if __name__ == "__main__":
    main()
