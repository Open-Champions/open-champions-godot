#!/usr/bin/env python3
"""Vectors for native/src/net_launch.cpp from the research reference model.

Reads tools/ref_items_gaps.py from t3/research-items-gaps at ecf9742
(docs/spec/items-gaps.md section 2). No disc and no executable: every value is
seeded here, and the expected output comes only from the reference functions.

  python3 tools/generate_net_launch_vectors.py --research-dir DIR \
      --out native/tests/vectors/net_launch.txt --contract native/tests/net_launch_contract.json
"""
import argparse
import hashlib
import json
import random
import subprocess
import sys
from pathlib import Path

COMMIT = "ecf9742"
FILES = ("docs/spec/items-gaps.md", "tools/ref_items_gaps.py", "tools/difftest_items_gaps.py",
         "tools/test_ref_items_gaps.py")
SEED = 20973


class Mem:
    def __init__(self):
        self.b = {}

    def load(self, a, n):
        return int.from_bytes(bytes(self.b.get(a + i, 0) for i in range(n)), "little")

    def store(self, a, n, v):
        for i, x in enumerate((v & ((1 << (8 * n)) - 1)).to_bytes(n, "little")):
            self.b[a + i] = x

    def u8(self, a): return self.load(a, 1)
    def u32(self, a): return self.load(a, 4)
    def w8(self, a, v): self.store(a, 1, v)
    def w32(self, a, v): self.store(a, 4, v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--research-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--contract", required=True)
    args = ap.parse_args()
    research = Path(args.research_dir)
    head = subprocess.run(["git", "-C", str(research), "rev-parse", "--short", "HEAD"], capture_output=True,
                          text=True, check=True).stdout.strip()
    if head != COMMIT:
        sys.exit("research HEAD is %s, not %s" % (head, COMMIT))
    sys.path.append(str(research / "tools"))
    import ref_items_gaps as R
    rng = random.Random(SEED)
    lines = []
    S = R.SETTINGS

    def settings_bytes(m):
        return [m.u8(S + o) for o in (R.S_FLAGS, R.S_DIFFICULTY, R.S_MODE, R.S_MISSION, R.S_KILL_LIMIT,
                                        R.S_COUNT, R.S_OWN, R.S_0A)]

    def fmt(values):
        return " ".join("%x" % v for v in values)

    for _ in range(300):                                  # lobby_task
        own, count, flags, kind, b0a = (rng.randrange(256) for _ in range(5))
        if rng.random() < 0.4:
            own = rng.choice((0, 0, 1, 2, 3))
        room = [rng.randrange(256) for _ in range(4)]
        diff, mode, kill, mission = (rng.randrange(256) for _ in range(4))
        m = Mem()
        m.w32(R.G_LOBBY_CTX, 0x1000)
        m.w8(0x1000 + R.LOBBY_OWN, own)
        for off, v in zip((R.ROOM_DIFFICULTY, R.ROOM_MODE, R.ROOM_KILL_LIMIT, R.ROOM_MISSION), room):
            m.w8(0x1000 + R.LOBBY_ROOM + off, v)
        for off, v in zip((R.S_FLAGS, R.S_DIFFICULTY, R.S_MODE, R.S_MISSION, R.S_KILL_LIMIT, R.S_COUNT, R.S_0A),
                          (flags, diff, mode, mission, kill, count, b0a)):
            m.w8(S + off, v)
        m.w8(S + R.S_OWN, rng.randrange(256))
        m.w8(R.G_CHAR_KIND, kind)
        before = settings_bytes(m)
        R.lobby_set_host(m, )
        lines.append("lobby %s | %x %s %x | %s" % (fmt(before), own, fmt(room), kind, fmt(settings_bytes(m))))
    for _ in range(100):                                  # frontend_run host
        sel = rng.choice((0, 1, 2, 0xFFFFFFFF, 0x100000000))
        m = Mem()
        for o in (R.S_FLAGS, R.S_DIFFICULTY, R.S_MODE):
            m.w8(S + o, rng.randrange(256))
        R.frontend_set_host(m, sel)
        lines.append("menu %x | %s" % (1 if sel & 0xFFFFFFFFFFFFFFFF else 0, fmt(settings_bytes(m))))
    for _ in range(100):                                  # frontend_run kind bits
        m = Mem()
        flags, kind, b0a = (rng.randrange(256) for _ in range(3))
        m.w8(S + R.S_FLAGS, flags)
        m.w8(S + R.S_0A, b0a)
        m.w8(R.G_CHAR_KIND, kind)
        R.frontend_set_kind(m)
        lines.append("kind %x %x %x | %x %x" % (flags, kind, b0a, m.u8(S + R.S_FLAGS), m.u8(S + R.S_0A)))
    for own in range(256):                                # own record, every own number
        lines.append("own %x | %x" % (own, R.own_record(own)))
    for _ in range(300):                                  # net_launchPrepare fill
        m = Mem()
        flags = rng.choice((0, 1, 2, 3, 4, 5, rng.randrange(256)))
        vals = [rng.randrange(256) for _ in range(4)]
        for o, v in zip((R.S_FLAGS, R.S_DIFFICULTY, R.S_MODE, R.S_MISSION), (flags, *vals[:3])):
            m.w8(S + o, v)
        m.w8(S + R.S_KILL_LIMIT, vals[3])
        rec = 0x2000
        old = [rng.randrange(256) for _ in range(4)]
        old_cm1 = rng.randrange(1 << 32)
        for o, v in zip((R.R_DIFFICULTY, R.R_MODE, R.R_KILL_LIMIT, R.R_MISSION), old):
            m.w8(rec + o, v)
        m.w32(rec + R.R_CM1, old_cm1)
        scan = rng.randrange(-1, 10) & 0xFFFFFFFF
        calls = R.launch_fill_host_fields(m, rec, scan)
        after = [m.u8(rec + o) for o in (R.R_DIFFICULTY, R.R_MODE, R.R_KILL_LIMIT, R.R_MISSION)]
        lines.append("fill %x %s | %s %x | %s %x %d" % (flags, fmt(vals), fmt(old), old_cm1, fmt(after),
                                                          m.u32(rec + R.R_CM1), 1 if calls else 0) + " %x" % scan)
    for _ in range(300):                                  # net_launchUpdate receive
        m = Mem()
        mode = rng.choice((0, 1, 2, 3, 4, 0x80, 0x82, 0x83, 0xFF, rng.randrange(256)))
        diff, kill = rng.randrange(256), rng.randrange(256)
        cm1 = rng.randrange(1 << 32)
        m.w8(R.LAUNCH_RECS + R.R_MODE, mode)
        m.w8(R.LAUNCH_RECS + R.R_DIFFICULTY, diff)
        m.w8(R.LAUNCH_RECS + R.R_KILL_LIMIT, kill)
        m.w32(R.LAUNCH_RECS + R.R_CM1, cm1)
        calls = R.launch_receive(m)
        got = [m.u32(R.G_DIFFICULTY), m.u8(R.G_KILL_LIMIT), m.u32(R.G_PVP_MODE_A), m.u32(R.G_PVP_MODE_B),
               m.u32(R.G_MODE_3), 1 if calls else 0, calls[0][1] if calls else 0]
        lines.append("receive %x %x %x %x | %s" % (mode, diff, kill, cm1, fmt(got)))
    for mode in range(256):                               # composition
        lines.append("compose %x | %x %x" % (mode, R.cm1_word_after_launch(4, 9, mode),
                                             R.cm1_word_after_launch(0xFFFFFFFF, 3, mode) & 0xFFFFFFFF))
    Path(args.out).write_text("\n".join(lines) + "\n")
    contract = {"research_commit": COMMIT, "policy": "rta-net-launch-v1", "seed": SEED, "cases": len(lines),
                "files": {f: hashlib.sha256((research / f).read_bytes()).hexdigest() for f in FILES}}
    Path(args.contract).write_text(json.dumps(contract, indent=2) + "\n")
    print(len(lines), "cases")


if __name__ == "__main__":
    main()
