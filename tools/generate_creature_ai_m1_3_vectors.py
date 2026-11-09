#!/usr/bin/env python3
"""Write synthetic decision vectors for native creature_ai_m1_3 tests.

Runs only the research decision MODELS (tools/models_creature_ai_m1_3.py at
t3/research-campaign-combat fbb2e1d), which the research difftest executed
against the original run functions on both seeds (docs/creature-ai-m1-3.md
section 5). No executable, level or disc input is read: every context is the
research's own synthetic grid (difftest contexts()), sampled, and every answer
path of each sampled context is enumerated like the difftest walk.

Usage: python3 -I tools/generate_creature_ai_m1_3_vectors.py --research <wt> \
           --output native/tests/vectors/creature_ai_m1_3.tsv.gz
Writes a deterministic gzip fixture (mtime 0) and a .json manifest beside it.
"""
import argparse
import gzip
import hashlib
import json
import pathlib
import random
import struct
import subprocess
import sys

REVISION = "fbb2e1db4598fa3d2e48d070d1ddda78a954fb55"
CLASSES = ("Kobold", "WarWraith", "Orc", "Goblin", "Ogre", "Warrior", "Clockwork",
           "Droid", "IronGuardian", "Froglock", "Amphian", "SeaMonster")
# Contexts kept per (kind, variant, state) group; HP and exit contexts are
# all kept for Amphian (its HP rule) and sampled elsewhere.
KEEP = {"grid": 5, "draw": 4, "hp": 3, "exit": 2}


def raw(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def num(value):
    return repr(float(value))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--research", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--seed", type=int, default=20973)
    args = parser.parse_args()
    revision = subprocess.check_output(
        ["git", "-C", str(args.research), "rev-parse", "HEAD"], text=True).strip()
    assert revision == REVISION, revision
    sys.path.insert(0, str(args.research / "tools"))
    import difftest_creature_ai_m1_3 as D
    import models_creature_ai_m1_3 as MODELS
    import ref_creature_ai_m1_3 as R

    class Rec(R.M):
        def __init__(self, ctx, answers):
            super().__init__(ctx, answers)
            self.asked = []

        def ask(self, *event):
            self.asked.append(event[0])
            return super().ask(*event)

    def run(model, ctx, answers):
        m = Rec(ctx, answers)
        try:
            model(m)
        except MODELS.TickEnd:
            pass
        return m

    def paths(model, ctx, limit=4000):
        stack, out = [()], []
        while stack:
            prefix = stack.pop()
            m = run(model, ctx, prefix)
            used = [prefix[i] if i < len(prefix) else 0 for i in range(len(m.asked))]
            out.append((used, m.events, m.rng.state))
            assert len(out) <= limit
            for j in range(len(prefix), len(used)):
                for alt in D.domain(m.asked[j]):
                    if alt != 0:
                        stack.append(tuple(used[:j]) + (alt,))
        return out

    lines = []
    for name in CLASSES:
        spec = MODELS.SPECS[name]
        rng = random.Random("vectors:%s:%d" % (name, args.seed))
        groups = {}
        for kind, _group, ctx in D.contexts(name, spec, args.seed):
            key = (kind, ctx["state"], tuple(sorted(ctx["fields"].items())),
                   ctx["alive"], tuple(sorted(ctx["holder"].items())))
            groups.setdefault(key, []).append(ctx)
        for key in sorted(groups, key=repr):
            ctxs = groups[key]
            keep = KEEP[key[0]]
            if name == "Amphian" and key[0] in ("hp", "exit"):
                keep = len(ctxs)
            for ctx in rng.sample(ctxs, min(keep, len(ctxs))):
                hp, top = ctx["hp"]
                fields = ",".join("%d=%d" % (k, v) for k, v in sorted(ctx["fields"].items()))
                holder = ctx["holder"].get(0x7C, 0)
                for used, events, final in paths(spec["model"], ctx):
                    ev = ";".join(e[0] + "|" + ",".join(num(a) for a in e[1:]) for e in events)
                    lines.append("\t".join(str(x) for x in (
                        name, ctx["state"], int(bool(ctx["target"])), ctx["ev"], ctx["anim"],
                        ctx["clock"], ctx["deadline"], fields or "-", "%08x" % raw(hp),
                        "%08x" % raw(top), ctx["alive"], int(bool(ctx["ally"])), holder,
                        ctx["base"], ctx["active"], "%08x" % ctx["seed"],
                        ",".join(str(a) for a in used) or "-", ev or "-", "%08x" % final)))
    header = ("# creature_ai_m1_3 decision vectors: research models at %s, seed %d,"
              " %d answer paths; synthetic contexts only\n" % (REVISION, args.seed, len(lines)))
    text = (header + "\n".join(lines) + "\n").encode()
    packed = gzip.compress(text, compresslevel=9, mtime=0)
    args.output.write_bytes(packed)
    manifest = {
        "research_revision": REVISION, "seed": args.seed, "paths": len(lines),
        "classes": list(CLASSES), "expanded_bytes": len(text),
        "expanded_sha256": hashlib.sha256(text).hexdigest(),
        "compressed_sha256": hashlib.sha256(packed).hexdigest(),
        "generator_sha256": hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        "scope": "Research decision models only, on a sample of the research's synthetic context grid; "
                 "no executable, level or disc input.",
    }
    args.output.with_suffix("").with_suffix(".json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print("wrote", len(lines), "paths to", args.output)


if __name__ == "__main__":
    main()
