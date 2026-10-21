#!/usr/bin/env python3
"""Vectors for the ground item (ItemProp_ctor2) from the research reference model.

Reads tools/ref_item_drops.py (class Ground) from t3/research-items-gaps at
ecf9742 (docs/spec/item-drops.md section 1). No disc and no executable: the item
records, positions, seeds and blocks are seeded here, the leaf call-outs (grid
link, property lookup, resource lookup, model bounds) are stubs that answer 0, and
the expected bytes come only from the reference. Each case stores the whole
0x190-byte block after the call, the shared seed after it, and the number of
shared draws.

  python3 tools/generate_item_drops_vectors.py --research-dir DIR \
      --out native/tests/vectors/item_drops.txt --contract native/tests/item_drops_contract.json
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

COMMIT = "ecf9742"
FILES = ("docs/spec/item-drops.md", "docs/spec/items-gaps.md", "tools/ref_item_drops.py",
         "tools/difftest_item_drops.py", "tools/test_ref_item_drops.py", "tools/ref_script_items.py",
         "tools/ref_items.py", "tools/ref_core.py")
SEED = 20973


class Mem:
    def __init__(self):
        self.b = {}

    def read(self, a, n):
        return bytes(self.b.get(a + i, 0) for i in range(n))

    def write(self, a, data):
        for i, x in enumerate(data):
            self.b[a + i] = x

    def load(self, a, n):
        return int.from_bytes(self.read(a, n), "little")

    def store(self, a, n, v):
        self.write(a, (v & ((1 << (8 * n)) - 1)).to_bytes(n, "little"))


class Host:
    """The leaf call-outs the model leaves to the world: all answer 0 (bounds: six zeros)."""

    def call(self, name, *args):
        if name == "model_getBounds":
            return (0, 0, 0, 0, 0, 0)
        return 0


def f_bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


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
    import dis  # noqa: F401,E402  (tools/dis.py shadows the standard library)
    import ref_item_drops as D  # noqa: E402
    import ref_script_items as R  # noqa: E402
    rng = random.Random(SEED)
    lines = []
    shared = 0
    for case in range(400):
        mem = Mem()
        seed = rng.getrandbits(32)
        mem.store(R.G_RAND_SEED, 4, seed)
        mem.store(D.G_PROP_OPTIONS, 4, rng.choice((0, 0x100)))
        block = 0x01300000 + 0x400 * case
        fault = ""
        if case % 20 == 19:
            block += 4                      # (block + 0x148) not 8-aligned: ld/sd of the original faults
            fault = "misaligned"
        item = bytearray(rng.getrandbits(8) for _ in range(D.ITEM_SIZE))
        definition = rng.choice((0x00100000 + 0x100 * rng.randrange(40), 0x00100000 + 0x100 * rng.randrange(40), 0))
        item[0:4] = definition.to_bytes(4, "little")
        flags = int.from_bytes(item[8:16], "little")
        flags = (flags & ~(1 << 39)) | (rng.randrange(2) << 39)
        flags = (flags & ~(1 << 36)) | (rng.randrange(2) << 36)
        item[8:16] = flags.to_bytes(8, "little")
        item_at = 0x01200000 + 0x100 * case
        mem.write(item_at, bytes(item))
        pos = tuple(f_bits(rng.choice((rng.uniform(-3000, 3000), rng.uniform(-9000, 9000), 0.0))) for _ in range(3))
        ground = D.Ground(mem, None, Host())
        outcome = "ok"
        try:
            ground.itemprop_ctor2(block, pos, item_at, 0)
        except D.Fault:
            outcome = "fault"
        if outcome == "fault":
            expected_fault = "fault"
            lines.append("groundfault %x %x %s" % (seed, block, bytes(item).hex()))
            continue
        after = mem.load(R.G_RAND_SEED, 4)
        # one shared draw: the seed advanced exactly one step
        step = (seed * 0x19660D + 0x3C6EF35F) & 0xFFFFFFFF
        assert after == step, "ground item made other than one shared draw"
        shared += 1
        lines.append("ground %x %x %s %s %s | %x | %s" % (seed, block, " ".join("%x" % p for p in pos),
                                                          bytes(item).hex(), "%x" % definition, after,
                                                          mem.read(block, 0x190).hex()))
    Path(args.out).write_text("\n".join(lines) + "\n")
    contract = {"research_commit": COMMIT, "policy": "rta-item-drops-v1", "seed": SEED, "cases": len(lines),
                "one_shared_draw_cases": shared,
                "files": {f: hashlib.sha256((research / f).read_bytes()).hexdigest() for f in FILES}}
    Path(args.contract).write_text(json.dumps(contract, indent=2) + "\n")
    print(len(lines), "cases,", shared, "with one shared draw")


if __name__ == "__main__":
    main()
