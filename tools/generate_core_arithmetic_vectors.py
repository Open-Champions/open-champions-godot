#!/usr/bin/env python3
"""Generate exact raw-pattern vectors from the pinned, authored RTA reference.
No game data, host arithmetic expectations, or research worktree modifications.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import random
import subprocess
import types

REVISION = "41e847af5360724ab62d208594f8027ea3c1b686"
SEED = 0x52415441


def generate(research_dir, output, revision):
    source = subprocess.run(["git", "-C", str(research_dir), "show", f"{revision}:tools/ref_core.py"], check=True, capture_output=True).stdout
    commit = subprocess.run(["git", "-C", str(research_dir), "rev-parse", f"{revision}^{{commit}}"], check=True, capture_output=True, text=True).stdout.strip()
    ref = types.ModuleType("pinned_ref_core")
    exec(compile(source, "pinned_ref_core.py", "exec"), ref.__dict__)
    rng = random.Random(SEED)
    lines = ["# model\tmul_bias\tdiv_nearest\top\ta\tb\texpected\n"]
    counts = Counter()
    trace_digest = 14695981039346656037
    def emit(model, bias, nearest, op, a, b=0):
        nonlocal trace_digest
        fun = getattr(ref, op)
        value = fun(a, b) if op in binary else fun(a)
        mask = (1 << 64) - 1 if op in double_output else (1 << 32) - 1
        lines.append(f"{model}\t{bias}\t{nearest}\t{op}\t{a:016x}\t{b:016x}\t{value & mask:016x}\n")
        counts[op] += 1
        for shift in range(0,64,8):
            trace_digest ^= ((value & mask) >> shift) & 255
            trace_digest = (trace_digest * 1099511628211) & ((1 << 64)-1)
    binary = set("add sub mul div rsqrt fmax fmin lt le eq dadd dsub dmul ddiv dcmp fmod".split())
    double_output = set("dadd dsub dmul ddiv i2d f2d fmod".split())
    edges = [0, 1, 0x7fffff, 0x800000, 0x800001, 0x3effffff, 0x3f000000,
             0x3f000001, 0x3f7fffff, 0x3f800000, 0x3f800001, 0x40000000,
             0x4affffff, 0x4b000000, 0x4b000001, 0x4effffff, 0x4f000000,
             0x4f000001, 0x7f000000, 0x7f7fffff, 0x7f800000, 0x7fffffff]
    edges += [x | 0x80000000 for x in edges]
    pairs = [(a,b) for a in edges for b in edges]
    # Cancellation and exponent alignments around each truncation boundary.
    for gap in [0,1,2,6,23,24,25,26,63,64,127,254]:
        for mant in [0,1,0x3fffff,0x7fffff]:
            for sign in [0,0x80000000]:
                pairs.append((0x7fffffff, sign | ((255-gap) << 23) | mant))
    random_pairs = [(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(256)]
    for model in ref.Fpu.MODELS:
        for bias in [0,1]:
            for nearest in [0,1]:
                ref.set_fpu(model, bool(bias), bool(nearest))
                for a,b in pairs + random_pairs:
                    for op in "add sub mul div rsqrt fmax fmin lt le eq".split():
                        emit(model,bias,nearest,op,a,b)
                for a in edges + [a for a,b in random_pairs]:
                    for op in "sqrt from_int to_int to_uint round_half_away neg fabs s16 s32".split():
                        emit(model,bias,nearest,op,a)
    dedges = [0,1,0xfffffffffffff,0x10000000000000,0x10000000000001,
              0x3fdfffffffffffff,0x3fe0000000000000,0x3fefffffffffffff,
              0x3ff0000000000000,0x3ff0000000000001,0x4000000000000000,
              0x41dfffffffffffff,0x41e0000000000000,0x47f0000000000000,
              0x4800000000000000,0x7fefffffffffffff,0x7ff0000000000000,
              0x7ff0000000000001,0x7ff8000000000000,0x7fffffffffffffff]
    dedges += [a | (1 << 63) for a in dedges]
    dpairs = [(a,b) for a in dedges for b in dedges]
    dpairs += [(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(2048)]
    for gap in [1,8,52,53,60,63,64,65,1022]:
        dpairs.append((0x3ff0000000000000, (1 << 63) | ((1023-gap) << 52) | 0xfffffffffffff))
    for a,b in dpairs:
        for op in "dadd dsub dmul ddiv dcmp fmod".split():
            emit("ps2",0,0,op,a,b)
    for a in dedges + [a for a,b in dpairs[-2048:]]:
        for op in ["d2i","d2f"]: emit("ps2",0,0,op,a)
    for a in edges + [a for a,b in random_pairs]:
        for op in ["i2d","f2d"]: emit("ps2",0,0,op,a)
    data = "".join(lines).encode("ascii")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    manifest = {
        "schema_version": 1, "reference_commit": commit,
        "reference_path": "tools/ref_core.py", "reference_sha256": hashlib.sha256(source).hexdigest(),
        "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "seed": SEED, "settings": {"models": list(ref.Fpu.MODELS), "mul_bias": [False,True], "div_nearest": [False,True]},
        "vector_sha256": hashlib.sha256(data).hexdigest(), "vector_count": sum(counts.values()),
        "expected_trace_fnv1a64": f"{trace_digest:016x}",
        "operations": dict(sorted(counts.items())),
        "scope": "RTA published reference model parity; not measured console or Norrath parity",
        "excluded_input_helpers": ["f32", "to_float", "d64", "to_double", "const", "dconst"],
        "literal_policy": "Model tables use generated explicit raw patterns; no runtime decimal parser or host floating-point conversion."
    }
    output.with_suffix(".json").write_text(json.dumps(manifest, indent=2, sort_keys=True)+"\n", encoding="utf-8")
    print(f"Wrote {sum(counts.values())} exact-bit vectors to {output}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--research-dir", required=True, type=Path)
    parser.add_argument("--revision", default=REVISION)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1]/"native/tests/vectors/core_arithmetic.tsv")
    args = parser.parse_args()
    generate(args.research_dir, args.output, args.revision)
