#!/usr/bin/env python3
"""Read-only check of the pinned, authored WarBoar probe. Prints counts only.

Optional private acceptance, never a public-test dependency. All memory and
resource tokens in these probes are synthetic; base services remain recording
stubs. No whole-tick or live gameplay equivalence is asserted.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--research", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    pin = subprocess.check_output(["git", "-C", str(args.research), "rev-parse", "HEAD"], text=True).strip()
    assert pin == "3f81c89215af3a59337709f98c75385370b7cea0", "research pin changed"
    sys.path.insert(0, str(args.research / "tools"))
    import difftest_warboar_ai as probe
    assert hashlib.sha256(args.executable.read_bytes()).hexdigest() == probe.EXPECTED_SHA256
    # These complete callback signatures fill the class-entry details omitted
    # from the handoff's prose. They are checked against the same original
    # entry point used by check_set_state, with no translation imported.
    entries = {
        0: (0x84, 3, 0x3DCCCCCC, 0, False),
        1: (0x94, 0x803, 0x3DCCCCCC, 0, True),
        3: (0x4C, 0x803, 0x3E4CCCCC, 0, True),
        5: (0x84, 3, 0x3E4CCCCC, 0, True),
        6: (0x24, 8, 0x3E999999, 0x3E999999, False),
        7: (0x54, 0x808, 0x3E999999, 0x3E999999, False),
        8: (0x2C, 0x4008, 0x3E999999, 0x3E999999, False),
        9: (0x3C, 0x4008, 0x3E999999, 0x3E999999, False),
        10: (0x34, 0x808, 0x3E999999, 0x3E999999, False),
        16: (0x44, 0x803, 0x3DCCCCCC, 0, True),
        18: (0x6C, 0x10, 0x3DCCCCCC, 0x3DCCCCCC, False),
        19: (0x84, 3, 0x3DCCCCCC, 0, False),
    }
    for seed in (20973, 7):
        cases = 0
        for check in (probe.check_base_defaults, probe.check_constructor_events,
                      probe.check_idle, probe.check_choices, probe.check_attack,
                      probe.check_set_state, probe.check_recovery, probe.check_geometry,
                      probe.check_sight_wrapper, probe.check_multi_hit):
            cases += check(args.executable, seed)
        assert cases == 530
        for state, (offset, flags, blend, out, sync) in entries.items():
            rig = probe.Rig(args.executable, seed, 3)
            rig.emu.call(probe.SET_STATE, rig.obj, state, max_steps=20000)
            calls = rig.emu.calls
            animation = [c for c in calls if c.target in (0x16CFE0, 0x16CEB0)]
            assert len(animation) == 1
            call = animation[0]
            assert call.target == (0x16CEB0 if out else 0x16CFE0)
            assert call.args[:3] == (rig.obj + 0x114, 0x700000 + offset, flags)
            assert call.fargs[:2] == (blend, out)
            assert sum(c.target == 0x1709F8 for c in calls) == int(sync)
            assert sum(c.target == 0x11A1E0 for c in calls) == int(state in (1, 3, 16))
            cases += 1
        rig = probe.Rig(args.executable, seed, 6, True, 2)
        rig.emu.stub(probe.SEE, ret=1)
        assert rig.run() == [5]  # continuation wins even when flag 8 is clear
        cases += 1
        print(f"PASS research AI cases={cases} seed={seed}")


if __name__ == "__main__":
    main()
