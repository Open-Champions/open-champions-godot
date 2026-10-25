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


ROOT = pathlib.Path(__file__).resolve().parents[1]


def file_count(folder: pathlib.Path) -> int:
    return sum(1 for path in folder.rglob("*") if path.is_file())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=pathlib.Path, required=True)
    parser.add_argument("--game-folder", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, default=ROOT / "work/playthrough")
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
        try:
            with (dest / "godot.log").open("w", encoding="utf-8") as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=900, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
        report = dest / "result.json"
        results[name] = {"exit_code": code,
                         "result_written": report.is_file()}
        if report.is_file():
            data = json.loads(report.read_text(encoding="utf-8"))
            results[name]["screenshots"] = data.get("screenshots", 0)
            results[name]["steps"] = len(data.get("steps", []))
            results[name]["refusal_kinds"] = len(data.get("refusals", {}))
        print(f"{name}: {results[name]}")
    after = file_count(disc)
    summary = {"disc_files_before": before, "disc_files_after": after,
               "variants": results}
    (output / "run-summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                               encoding="utf-8")
    print(f"disc files: {before} before, {after} after")
    return int(before != after or any(row["exit_code"] != 0 or not row["result_written"]
                                      for row in results.values()))


if __name__ == "__main__":
    sys.exit(main())
