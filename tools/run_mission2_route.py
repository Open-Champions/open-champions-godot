#!/usr/bin/env python3
"""Counts-only mission-2 acceptance, with an explicitly synthetic hub offer.

The menu fixture controls availability; it does not prove live hub dialog.
The owner folder is read-only. All output belongs under ignored work/.
"""
import argparse
import importlib
import json
import pathlib
import subprocess
import sys
import tempfile

import generate_script_host_vectors as fixtures

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=pathlib.Path, required=True)
    parser.add_argument("--game-folder", type=pathlib.Path, required=True)
    parser.add_argument("--research-dir", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, default=ROOT / "work/mission2/acceptance")
    args = parser.parse_args()
    out = args.out.resolve()
    out.relative_to(ROOT / "work")
    out.mkdir(parents=True, exist_ok=True)
    disc = args.game_folder.resolve(strict=True)
    godot = args.godot.resolve(strict=True)
    # Existing generator verifies ref_amx and its dependencies against 9649e6b.
    with tempfile.TemporaryDirectory(dir=out) as temp:
        fixtures.stage(args.research_dir, pathlib.Path(temp))
        sys.path.insert(0, temp)
        amx = importlib.import_module("ref_amx")
        for available in (False, True):
            calls = [("setMissionAvailable", [1, 1]), ("grantMission", [1]),
                     ("setMissionAvailable", [2, int(available)]),
                     ("addQuest", [2000, 2100]), ("activateMissionMenu", [])]
            case = {"strings": {2000: "authored-title", 2100: "authored-body"},
                    "publics": [("start", "zero", calls)]}
            (out / ("offered.scr" if available else "locked.scr")).write_bytes(fixtures.image_of(amx, case))
    before = sum(p.is_file() for p in disc.rglob("*"))
    results = {}
    for variant, flags in (("headless", ["--headless"]), ("rendered", [])):
        dest = out / variant
        dest.mkdir(exist_ok=True)
        (dest / "result.json").unlink(missing_ok=True)
        command = [str(godot), *flags, "--path", str(ROOT / "godot"), "--script",
                   "res://tests/mission2_route.gd", "--", f"--game-folder={disc}",
                   f"--out={dest}", f"--fixture-dir={out}"]
        with (dest / "godot.log").open("w") as log:
            try:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=900).returncode
            except subprocess.TimeoutExpired:
                code = 124
        report = dest / "result.json"
        results[variant] = {"exit": code, "result_written": report.is_file()}
        if report.is_file():
            results[variant]["result"] = json.loads(report.read_text())
        print(variant, "exit", code, "result", report.is_file(), flush=True)
    after = sum(p.is_file() for p in disc.rglob("*"))
    (out / "summary.json").write_text(json.dumps({"disc_before": before, "disc_after": after,
                                                  "variants": results}, indent=2) + "\n")
    print("disc files", before, after)
    return int(before != after or any(r["exit"] or not r["result_written"] for r in results.values()))


if __name__ == "__main__":
    raise SystemExit(main())
