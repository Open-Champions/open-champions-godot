#!/usr/bin/env python3
"""Generate authored raw-pattern tests from an explicitly supplied pinned research model.

No game executable or extracted assets are read. The checked-in vectors run without
Python or the research checkout. Reproduction intentionally requires the exact model.
"""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys

PIN = "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123"
COMMIT = "41e847af5360724ab62d208594f8027ea3c1b686"
SCHEMA = "rta-core-math-v1"
# Parameter shape: a plain integer is one word; lists/tuples flatten in order.
SHAPES = {
    "cos16": [1], "sin16": [1], "angle_from_radians": [1], "angle_from_degrees": [1],
    "atan2_16": [1, 1], "angle_turn_dir": [1, 1], "angle_approach": [1]*5,
    "hermite": [1]*5, "log2_floor": [1], "color_scale_clamped": [1, 1],
    "color_from_hsv": [1]*3, "vec3_hermite": [1,3,3,3,3],
    "vec3_hermite_derivative": [1,3,3,3,3], "vec3_dot_delta": [3]*3,
    "vec2_dot_delta": [2]*3, "ray_plane_distance": [3]*4,
    "segment_hits_plane_near_edge": [3]*5, "bbox_set_empty": [],
    "bbox_union": [6,6], "bbox_add_point": [6,3], "bbox_contains_point": [6,3],
    "bbox_overlaps": [6,6], "mat3_set_identity": [], "mat34_set_identity": [],
    "mat34_set_translation": [3], "mat3_mul": [9,9], "mat34_mul_mat3": [9,12],
    "mat34_mul": [12,12], "mat4_mul_mat34": [12,16], "mat4_mul": [16,16],
    "mat3_rotate_axis": [9,1,1], "mat34_rotate_axis": [12,1,1],
    "mat4_rotate_axis": [16,1,1], "mat3_rotate_axis_post": [9,1,1],
    "mat34_rotate_axis_post": [12,1,1], "mat3_set_rot_z": [1],
    "mat34_set_pos_rot_z": [3,1], "mat34_set_pos_rot_z_scale": [1,3,1],
    "mat34_set_pos_euler_scale": [1,3,1,1,1], "mat4_invert": [16],
    "mat34_invert": [12], "mat3_transpose": [9], "mat3_scale": [1,9],
    "mat3_scale_xyz": [1,1,1,9], "mat4_scale_rotation": [1,16],
    "mat34_translate_local": [1,1,1,12], "mat4_translate_local": [1,1,1,16],
    "mat3_mul_vec": [9,3], "mat34_rotate_vec": [12,3],
    "mat3_mul_vec_transposed": [9,3], "mat34_transform_point": [12,3],
    "mat4_transform_point": [16,3], "mat4_transform_point_transposed": [16,3],
    "mat4_set_projection": [1]*7, "mat4_set_projection_fixed": [1]*7,
    "mat4_from_mat3_pos": [9,3], "quat_from_axis_angle": [1]*4,
    "quat_mul": [4,4], "quat_normalize": [4], "quat_rotate_vec": [4,3],
    "quat_from_mat3": [9], "mat3_from_quat": [4], "mat34_from_quat": [4],
    "mat4_from_quat": [4,16], "noise": [256,1], "noise3": [256,1,1],
    "build_noise_table": [1,3,4], "atanf": [1], "atan2f": [1,1],
    "floorf": [1], "ceilf": [1], "copysignf": [1,1], "scalbnf": [1,1],
    "sqrtf": [1], "powf": [1,1], "fmodf": [1,1],
}
EDGES = [0,0x80000000,1,0x807fffff,0x00800000,0x80800000,0x3f800000,
         0xbf800000,0x3f000000,0xbf000000,0x40000000,0xc0000000,0x3f7fffff,
         0x3f800001,0x7fffffff,0xffffffff,0x7f800000,0xff800000,0x4f000000,
         0xcf000000,0x38d1b717,0x38d1b718,0x3edfffff,0x3ee00000,0x3f97ffff,
         0x3f980000,0x401bffff,0x401c0000,0x507fffff,0x50800000]


def flatten(value):
    if isinstance(value, (list, tuple)):
        return [x for v in value for x in flatten(v)]
    return [int(value) & 0xffffffff]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--research-dir", type=Path, required=True)
    ap.add_argument("--output", type=Path, default=Path("native/tests/vectors/core_math.tsv"))
    args = ap.parse_args()
    source = args.research_dir / "tools/ref_core.py"
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != PIN:
        ap.error("research model SHA256 differs from the pinned source; do not silently regenerate")
    head = subprocess.check_output(["git", "-C", str(args.research_dir), "rev-parse", "HEAD"], text=True).strip()
    pinned = subprocess.check_output(["git", "-C", str(args.research_dir), "show", COMMIT + ":tools/ref_core.py"])
    if hashlib.sha256(pinned).hexdigest() != PIN:
        ap.error("immutable research commit does not contain the pinned model")
    tree = ast.parse(raw)
    public = {n.name for n in tree.body if isinstance(n, ast.FunctionDef)
              and 377 <= n.lineno < 1627 and not n.name.startswith("_")}
    public |= {"mat34_rotate_axis", "mat34_rotate_vec"}
    if public != set(SHAPES):
        raise RuntimeError("modeled public API coverage differs")
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("ref_core_math_vectors", source)
    model = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(model)
    rng = random.Random(0x434f5245)
    counts = {}
    mode_counts = {}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="ascii", newline="\n") as out:
        out.write(f"# {SCHEMA}\n# op\tmode\tmul_bias\tdiv_nearest\tinput hex words\texpected hex words\n")
        config = ("ps2", False, False)
        def emit(op, values):
            if op == "libm_coefficients":
                result = [model.const(t) for t in model.LIBM_DECIMALS]
                inputs = []
            else:
                result = getattr(model, op)(*values)
                inputs = flatten(values) if op != "build_noise_table" else []
                if op == "log2_floor":
                    bits = values[0] & ((1 << 64)-1)
                    inputs = [bits & 0xffffffff, bits >> 32]
                elif op == "build_noise_table":
                    inputs = flatten(values[:2]) + [int(values[2] is not None)] + flatten(values[2] or [0]*4)
                elif op == "segment_hits_plane_near_edge":
                    hit, point = result
                    result = [hit, int(point is not None)] + (point or [0]*3)
            words = lambda seq: ",".join(f"{v:08x}" for v in flatten(seq)) or "-"
            out.write(f"{op}\t{config[0]}\t{int(config[1])}\t{int(config[2])}\t{words(inputs)}\t{words(result)}\n")
            counts[op] = counts.get(op, 0) + 1
            key = ":".join(map(str, config))
            mode_counts[key] = mode_counts.get(key, 0) + 1
        # Every 16-bit input, not sparse cardinal-angle sampling.
        for angle in range(65536):
            emit("cos16", [angle])
            emit("sin16", [angle])
        for mode in ("ps2", "ps2g1", "chop", "nearest"):
            for bias, rounding in ((False,False),(True,False),(False,True),(True,True)):
                config = (mode,bias,rounding)
                model.set_fpu(*config)
                emit("libm_coefficients", [])
                for op, shape in SHAPES.items():
                    trials = 12 if op == "build_noise_table" else 64
                    if op in {"atanf", "atan2f", "floorf", "ceilf", "copysignf", "scalbnf", "sqrtf", "powf", "fmodf"}:
                        trials = 1024
                    if not shape:
                        trials = 1
                    for trial in range(trials):
                        def lane():
                            if trial < 16:
                                return rng.choice(EDGES)
                            if trial >= 64:
                                return rng.getrandbits(32)
                            return model.f32(rng.uniform(-100,100))
                        values = [lane() if size==1 else [lane() for _ in range(size)] for size in shape]
                        if op in ("cos16","sin16","mat3_set_rot_z"):
                            values = [rng.getrandbits(32)]
                        if "rotate_axis" in op:
                            values[-2:] = [trial%5-1, rng.getrandbits(32)]
                        if op == "angle_turn_dir":
                            values = [rng.getrandbits(32),rng.getrandbits(32)]
                        if op == "angle_approach":
                            values[-2:] = [rng.getrandbits(32),rng.getrandbits(32)]
                        if op in ("mat34_set_pos_rot_z","mat34_set_pos_rot_z_scale","quat_from_axis_angle"):
                            values[-1] = rng.getrandbits(32)
                        if op == "mat34_set_pos_euler_scale":
                            values[-3:] = [rng.getrandbits(32) for _ in range(3)]
                        if op == "log2_floor":
                            values = [([-1,0,1,2,(1<<63)-1,-(1<<63)][trial%6]) if trial<16 else rng.randrange(-(1<<63),1<<63)]
                        if op == "color_scale_clamped":
                            values[1] = rng.getrandbits(32)
                        if op == "color_from_hsv" and trial>=16:
                            values = [model.f32(rng.uniform(-0.2,1.2)) for _ in range(3)]
                        if op == "scalbnf":
                            values[1] = [-2147483648,-50001,-256,-127,-1,0,1,255,50001,2147483647][trial%10]
                        if op in ("noise","noise3"):
                            values = [[rng.randrange(-32768,32768) for _ in range(256)]] + [rng.getrandbits(32) for _ in shape[1:]]
                        if op == "build_noise_table":
                            values = [rng.getrandbits(32),[lane() for _ in range(3)],None if trial%2==0 else [lane() for _ in range(4)]]
                        emit(op, values)
                # Geometric branch anchors supplement arbitrary raw edge patterns.
                f = model.f32
                v = lambda *xs: [f(x) for x in xs]
                for offset in (0,0.2,4):
                    emit("segment_hits_plane_near_edge", [v(0,0,0),v(2,0,0),v(0,0,1),v(offset,0,-1),v(offset,0,1)])
                emit("mat4_invert", [[0]*16])
                emit("quat_from_mat3", [model.mat3_set_identity()])
                emit("color_from_hsv", [f(1),f(1),f(1)])
    manifest = {
        "schema": SCHEMA,
        "reference": {"repository": "https://github.com/Open-Champions/champions-return-to-arms-re",
                      "commit": COMMIT, "checkout_head": head, "path": "tools/ref_core.py", "sha256": PIN},
        "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "vectors_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
        "seed": "0x434f5245", "rows": sum(counts.values()),
        "functions": dict(sorted(counts.items())), "configurations": mode_counts,
        "scope": "Exact pinned reference-model outputs; not console measurement or gameplay integration.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(f"Generated {sum(counts.values())} rows covering {len(counts)} APIs")


if __name__ == "__main__":
    main()
