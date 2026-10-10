#!/usr/bin/env python3
"""Compare authored local-pose and model/skin transform and skinned-position traces across five native targets, with tolerance.

This establishes agreement for these samples only, not bit-exact simulation or
retail animation fidelity. No game files, third-party modules or network needed.
"""
import argparse
import json
import math
from pathlib import Path
import re
import sys

PLATFORMS = frozenset({"macos-arm64", "macos-x64", "windows-arm64", "windows-x64", "linux-x64"})
FIXTURE = "rta-skinning-authored-v3"
ABS_TOL = 1e-12
REL_TOL = 1e-12
MAX_FILE_BYTES = 5 * 1024 * 1024
# The coverage contract is independent of any one producer's supplied manifest.
CASES = (
    ("channels", 3, (0, 1.5, 4, 4.25, 8, 16, 5, 0)),
    ("mask63", 64, (0, 255, 65536.25, 4294967295)),
    ("zero_quaternion", 1, (0, 1)),
    ("tiny_rotation", 1, (1e-200,)),
    ("rotated_hierarchy", 3, (0,)),
    ("invalid_ancestor", 4, (0,)),
)

# Independent model-space anchors: Rz90(parent)*Rx90(child), then add the
# already-signed inverse-bind translation after rotation. Values are row-major.
MODEL_ANCHORS = {
    ("rotated_hierarchy", 0): [0,-1,0,1, 1,0,0,2, 0,0,1,3],
    ("rotated_hierarchy", 1): [0,0,1,1, 1,0,0,4, 0,1,0,4],
    ("rotated_hierarchy", 2): [1,0,0,-2, 0,1,0,1, 0,0,1,-1],
    ("invalid_ancestor", 3): [1,0,0,3, 0,1,0,-2, 0,0,1,1],
}
SKIN_ANCHORS = {
    ("rotated_hierarchy", 0): [0,-1,0,3, 1,0,0,3, 0,0,1,3.5],
    ("rotated_hierarchy", 1): [0,0,1,7, 1,0,0,8, 0,1,0,9],
    ("rotated_hierarchy", 2): [1,0,0,-3, 0,1,0,3, 0,0,1,2],
    ("invalid_ancestor", 3): [1,0,0,4, 0,1,0,-4, 0,0,1,1.5],
}

# Fixed independent position anchors, calculated from the authored rotations /
# translations before dividing integer byte-weight numerators by255. Seam
# corners0/3 share position/influences but carry different source UVs.
SKIN_CASES = (
    ("rigid", 0, ((1,4,6.5),(3,1,4),(4,3,7.5))),
    ("weighted2", 7, ((64/255,447/255,1180/255),(-381/255,732.5/255,638/255),(65/255,192/255,671/255))),
    ("weighted3", 2, ((947/255,151/255,1062.5/255),(267/255,-2.5/255,466.5/255),(760/255,-414/255,985.5/255))),
    ("weighted4", 9, ((448/255,257/255,455.5/255),(66/255,34/255,165/255),(323/255,-378/255,328.5/255))),
    ("joint63", 63, ((-5,3,-5),(-2,1,-2.5),(-4,0,-6))),
    ("weighted_zero", 5, ((890/255,512/255,1340/255),(3,-126/255,702.5/255),(1147/255,3/255,1595/255))),
    ("seam", 6, ((64/255,447/255,1180/255),(-381/255,732.5/255,638/255),(65/255,192/255,671/255),
                 (64/255,447/255,1180/255),(65/255,192/255,671/255),(-381/255,732.5/255,638/255))),
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def keys(value, expected, where):
    require(isinstance(value, dict) and set(value) == set(expected), f"{where}: wrong fields")


def number(value, where):
    require(type(value) in (int, float), f"{where}: expected number, not boolean/string")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    require(finite, f"{where}: nonfinite number")
    return value


def vector(value, length, where):
    require(isinstance(value, list) and len(value) == length, f"{where}: wrong vector length")
    for index, item in enumerate(value):
        number(item, f"{where}[{index}]")


def validate(trace, expected_commit):
    keys(trace, ("schema", "fixture", "platform", "source_commit", "records", "skinning_records"), "trace")
    require(type(trace["schema"]) is int and trace["schema"] == 3, "unsupported schema")
    require(trace["fixture"] == FIXTURE, "unexpected fixture")
    require(isinstance(trace["platform"], str) and trace["platform"] in PLATFORMS, "unexpected platform")
    require(trace["source_commit"] == expected_commit, "wrong source commit")
    records = trace["records"]
    require(isinstance(records, list) and len(records) == sum(len(c[2]) for c in CASES), "missing/extra records")
    offset = 0
    for case, count, ticks in CASES:
        for sample, tick in enumerate(ticks):
            row = records[offset]
            where = f"{trace['platform']}/{case}/{sample}"
            keys(row, ("case", "sample", "tick", "joints"), where)
            require(row["case"] == case, f"{where}: case order/coverage differs")
            require(type(row["sample"]) is int and row["sample"] == sample, f"{where}: sample order/coverage differs")
            require(number(row["tick"], where + "/tick") == tick, f"{where}: query time differs")
            require(isinstance(row["joints"], list) and len(row["joints"]) == count, f"{where}: joint coverage differs")
            for index, joint in enumerate(row["joints"]):
                at = f"{where}/joint{index}"
                keys(joint, ("joint", "rotation_valid", "drives_joint", "position", "quaternion_wxyz",
                             "transform_valid", "transform_drives_joint", "model_matrix", "skin_matrix"), at)
                require(type(joint["joint"]) is int and joint["joint"] == index, f"{at}: joint index differs")
                require(type(joint["rotation_valid"]) is bool and joint["rotation_valid"] == (case != "zero_quaternion" and not (case == "invalid_ancestor" and index == 0)), f"{at}: rotation validity differs")
                drives = not ((case in ("channels", "invalid_ancestor") and index == 1)
                              or (case == "mask63" and index == 63) or (case == "rotated_hierarchy" and index == 0))
                require(type(joint["drives_joint"]) is bool and joint["drives_joint"] == drives, f"{at}: joint mask differs")
                valid_transform = case != "zero_quaternion" and not (case == "invalid_ancestor" and index < 3)
                require(type(joint["transform_valid"]) is bool and joint["transform_valid"] == valid_transform,
                        f"{at}: transform validity/ancestor propagation differs")
                require(type(joint["transform_drives_joint"]) is bool and joint["transform_drives_joint"] == drives,
                        f"{at}: transform joint mask differs")
                for field, anchors in (("model_matrix", MODEL_ANCHORS), ("skin_matrix", SKIN_ANCHORS)):
                    matrix = joint[field]
                    vector(matrix, 12, at + "/" + field)
                    if not valid_transform:
                        require(all(v == 0 for v in matrix), f"{at}: invalid transform must be all zero")
                    if (case, index) in anchors:
                        for value, expected in zip(matrix, anchors[case, index]):
                            require(abs(value - expected) <= ABS_TOL + REL_TOL * abs(expected),
                                    f"{at}/{field}: authored matrix anchor differs")
                vector(joint["position"], 3, at + "/position")
                q = joint["quaternion_wxyz"]
                vector(q, 4, at + "/quaternion_wxyz")
                # Prevent all-zero placeholder quaternion traces from succeeding.
                require(all(abs(v) <= 1 + 1e-12 for v in q), f"{at}: quaternion component out of range")
                norm = sum(v * v for v in q)
                require(math.isfinite(norm) and abs(norm - (1 if joint["rotation_valid"] else 0)) <= 1e-10,
                        f"{at}: invalid normalized quaternion")
            offset += 1
    # Independent numeric anchors reject common identical placeholder traces.
    expected_positions = ((1, -2, .5), (2.5, -2.75, .875), (5, -4, 1.5),
                          (5.125, -3.9375, 1.5), (7, -3, 1.5), (11, -1, 1.5),
                          (5.5, -3.75, 1.5), (1, -2, .5))
    for row, expected in zip(records[:8], expected_positions):
        require(row["joints"][0]["position"] == list(expected), "authored channel position anchor differs")
    require(records[8]["joints"][63]["position"] == [-512, 32767 / 64, -1 / 64],
            "authored masked-joint initial anchor differs")
    skin_records = trace["skinning_records"]
    require(isinstance(skin_records, list) and len(skin_records) == len(SKIN_CASES), "missing/extra skinning cases")
    for record, (case, mesh, expected) in zip(skin_records, SKIN_CASES):
        where = f"{trace['platform']}/skinning/{case}"
        keys(record, ("case", "mesh_index", "tick", "positions"), where)
        require(record["case"] == case, f"{where}: case order/coverage differs")
        require(type(record["mesh_index"]) is int and record["mesh_index"] == mesh, f"{where}: mesh index differs")
        require(number(record["tick"], where + "/tick") == 12.5, f"{where}: sample time differs")
        positions = record["positions"]
        require(isinstance(positions, list) and len(positions) == len(expected), f"{where}: corner coverage differs")
        for index, (position, anchor) in enumerate(zip(positions, expected)):
            vector(position, 3, f"{where}/corner{index}")
            for value, target in zip(position, anchor):
                require(abs(value - target) <= ABS_TOL + REL_TOL * abs(target), f"{where}/corner{index}: authored skinning anchor differs")
    # Returning to tick zero must produce the same owned pose as the first query.
    first, repeated = records[0], records[7]
    require(first["joints"] == repeated["joints"], "backward/repeated query changed pose")


def compare(traces, expected_commit):
    require(isinstance(expected_commit, str) and re.fullmatch(r"[0-9a-f]{40}", expected_commit), "source commit must be a full lowercase SHA1")
    require(len(traces) == len(PLATFORMS), "exactly five platform traces are required")
    by_platform = {}
    for trace in traces:
        validate(trace, expected_commit)
        platform = trace["platform"]
        require(platform not in by_platform, f"duplicate platform: {platform}")
        by_platform[platform] = trace
    require(set(by_platform) == PLATFORMS, "missing platform")
    reference = by_platform["linux-x64"]
    compared = 0
    for platform in sorted(PLATFORMS - {"linux-x64"}):
        for left, right in zip(reference["records"], by_platform[platform]["records"]):
            for a, b in zip(left["joints"], right["joints"]):
                for field in ("position", "quaternion_wxyz", "model_matrix", "skin_matrix"):
                    for component, (x, y) in enumerate(zip(a[field], b[field])):
                        allowed = ABS_TOL + REL_TOL * max(abs(x), abs(y))
                        require(abs(x - y) <= allowed,
                                f"{platform}/{left['case']}/{left['sample']}/joint{a['joint']}/{field}[{component}]: "
                                f"{y!r} differs from linux-x64 {x!r}; allowed {allowed:.3g}")
                        compared += 1
        for left, right in zip(reference["skinning_records"], by_platform[platform]["skinning_records"]):
            for corner, (a, b) in enumerate(zip(left["positions"], right["positions"])):
                for component, (x, y) in enumerate(zip(a, b)):
                    allowed = ABS_TOL + REL_TOL * max(abs(x), abs(y))
                    require(abs(x - y) <= allowed,
                            f"{platform}/skinning/{left['case']}/corner{corner}[{component}]: "
                            f"{y!r} differs from linux-x64 {x!r}; allowed {allowed:.3g}")
                    compared += 1
    return compared


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON field: {key}")
        result[key] = value
    return result


def load_trace(path):
    require(path.stat().st_size <= MAX_FILE_BYTES, f"trace too large: {path.name}")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"nonfinite JSON: {value}")))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-dir", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args(argv)
    try:
        require(args.trace_dir.is_dir(), "trace directory is missing")
        paths = sorted(args.trace_dir.rglob("*.json"))
        require(len(paths) == 5, "trace directory must contain exactly five JSON files")
        count = compare([load_trace(p) for p in paths], args.source_commit)
        print(f"PASS: all five platforms, 17 pose samples + 7 skinning cases each, {count} double components compared; "
              f"absolute tolerance {ABS_TOL:g} + relative tolerance {REL_TOL:g}. Not bit-exact runtime proof.")
        return 0
    except (ValueError, OSError, RecursionError) as error:
        print(f"Animation trace comparison failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
