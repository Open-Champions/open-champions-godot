#!/usr/bin/env python3
"""Run the private, counts-only journey in headless and rendered Godot.

Example:
  python3 tools/run_loop_playthrough.py --godot /path/to/Godot \
      --game-folder /path/to/read-only/rta-disc

Outputs stay under ignored work/playthrough. The game folder is only read.
"""

import argparse
import json
import pathlib
import subprocess
import sys
from godot_log import error_lines


ROOT = pathlib.Path(__file__).resolve().parents[1]


def file_count(folder: pathlib.Path) -> int:
    return sum(1 for path in folder.rglob("*") if path.is_file())


def pickup_verified(steps: list) -> bool:
    return any(row.get("step") == "loot_pickup" and row.get("result") == "passed"
               and row.get("drop_produced") is True and row.get("picked_up") is True
               and row.get("walked") is True
               and row.get("inventory_units_after", 0) > row.get("inventory_units_before", 0)
               for row in steps)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=pathlib.Path, required=True)
    parser.add_argument("--game-folder", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, default=ROOT / "work/playthrough")
    parser.add_argument("--blood1-only", action="store_true",
                        help="stop after the first story destination branch")
    parser.add_argument("--blood1-choice-evil", action="store_true",
                        help="follow the researched alternate blood1 dialog edges")
    args = parser.parse_args()
    godot = args.godot.resolve(strict=True)
    disc = args.game_folder.resolve(strict=True)
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    before = file_count(disc)
    results = {}
    for name, flags in (("headless", ["--headless"]), ("rendered", [])):
        dest = output / name
        dest.mkdir(exist_ok=True)
        (dest / "result.json").unlink(missing_ok=True)
        for screenshot in dest.glob("*.png"):
            screenshot.unlink()
        command = [str(godot), *flags, "--path", str(ROOT / "godot"),
                   "--script", "res://tests/loop_playthrough.gd", "--",
                   f"--game-folder={disc}", f"--out={dest}"]
        if name == "rendered":
            command.append("--rendered")
        if args.blood1_only:
            command.append("--blood1-only")
        if args.blood1_choice_evil:
            command.append("--blood1-choice-evil")
        try:
            with (dest / "godot.log").open("w", encoding="utf-8") as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=900, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
        report = dest / "result.json"
        results[name] = {"exit_code": code,
                         "log_errors": len(error_lines((dest / "godot.log").read_text(encoding="utf-8", errors="replace"))),
                         "result_written": report.is_file(), "pickup_verified": False}
        if report.is_file():
            data = json.loads(report.read_text(encoding="utf-8"))
            results[name]["screenshots"] = data.get("screenshots", 0)
            results[name]["steps"] = len(data.get("steps", []))
            results[name]["refusal_kinds"] = len(data.get("refusals", {}))
            results[name]["pickup_verified"] = pickup_verified(data.get("steps", []))
            for row in data.get("steps", []):
                if row.get("step") == "loot_pickup":
                    results[name]["loot_pickup"] = row
        print(f"{name}: {results[name]}")
    after = file_count(disc)
    summary = {"disc_files_before": before, "disc_files_after": after,
               "variants": results}
    (output / "run-summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                               encoding="utf-8")
    print(f"disc files: {before} before, {after} after")
    return int(before != after or any(row["exit_code"] != 0 or row["log_errors"] != 0 or not row["result_written"]
                                      or not row["pickup_verified"]
                                      for row in results.values()))


if __name__ == "__main__":
    sys.exit(main())
