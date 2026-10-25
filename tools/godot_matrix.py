#!/usr/bin/env python3
"""Run Godot checks with exit status AND script/engine log diagnostics as gates."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from godot_log import require_clean_log


ROOT = Path(__file__).resolve().parents[1]

# Explicit public fixture matrix. Helper scripts are not executable tests.
SMOKES = (
    "archive_smoke", "control_anim_cache_smoke",
    "frame_rate_smoke", "frontend_smoke", "fx_bridge_refusal_smoke",
    "fx_shade_batch_smoke", "inventory_icons_smoke", "level_library_smoke",
    "level_lights_smoke",
    "level_texture_preview_smoke", "level_texture_stream_smoke",
    "level_textured_session_smoke", "menu_smoke", "native_audio_smoke",
    "native_fx_smoke", "play_hud_smoke", "play_session_runtime",
    "playable_fx_camera_smoke", "playable_wave_smoke",
    "playable_wave_smoke_play", "playable_wave_smoke_ui",
    "playable_wave_smoke_combat", "playable_wave_smoke_level",
    "playable_wave_smoke_animation", "playable_wave_smoke_fx",
    "playable_wave_smoke_textures", "playable_wave_smoke_audio",
    "playable_wave_smoke_full_combat", "playable_wave_smoke_wave12",
    "ps2_save_import_smoke", "real_water_texture_smoke",
    "scene_animation_preview_smoke", "scene_original_art_smoke",
    "scene_play_flow_smoke", "script_commands_smoke", "session_facing_smoke",
    "session_music_smoke", "session_recovery_smoke", "session_sandbox_smoke",
    "texture_upscale_smoke", "ui_art_gallery_smoke", "ui_layout_dynamic_smoke",
    "ui_layout_smoke",
)
CAPTURES = (
    "creature_blend_smoke", "level_texture_material_smoke",
    "scene_animation_preview_visual", "scene_original_art_visual",
    "session_recovery_visual", "native_fx_visual",
)


def run_logged(command, log, timeout=180, expect=()):
    """Capture both streams; a PASS marker or exit 0 cannot override an error."""
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    try:
        with log.open("w", encoding="utf-8") as sink:
            result = subprocess.run([str(part) for part in command], cwd=ROOT,
                                    stdout=sink, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
    except subprocess.TimeoutExpired as error:
        with log.open("a", encoding="utf-8") as sink:
            sink.write(f"\nTIMEOUT after {timeout}s\n")
        raise ValueError(f"command timed out after {timeout}s; see {log}") from error
    output = log.read_text(encoding="utf-8", errors="replace")
    # Inspect the log even on nonzero exit, so diagnostics name the actual cause.
    require_clean_log(output, log)
    if result.returncode != 0:
        raise ValueError(f"command exited {result.returncode}; see {log}")
    for marker in expect:
        if marker not in output.splitlines():
            raise ValueError(f"missing completion marker {marker!r}; see {log}")
    return output


def matrix(args):
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    godot = args.godot.resolve()
    project = args.project.resolve()
    cases = {"editor": [godot, "--headless", "--path", project, "--editor",
                         "--quit-after", "120", "--frame-delay", "50"]}
    cases["default_scene"] = [godot, "--headless", "--path", project, "--quit-after", "120"]
    for name in SMOKES:
        cases[name] = [godot, "--headless", "--path", project, "--script",
                       f"res://tests/{name}.gd", "--", "--ready-ticks=640"]
    for name, flags in (("creature_animation", []), ("creature_dispatch", ["--dispatch"]),
                        ("creature_freed_callable", ["--freed"])):
        cases[name] = [sys.executable, ROOT / "tools/run_creature_anim_smoke.py",
                       "--godot", godot, *flags]
    if args.captures:
        for name in CAPTURES:
            directory = output / name
            directory.mkdir(exist_ok=True)
            cases[name] = [godot, "--path", project, "--script",
                           f"res://tests/{name}.gd", "--", f"--capture-dir={directory}"]
    for name, command in cases.items():
        if command[0] == godot:
            command[1:1] = ["--log-file", output / f"{name}-engine.log"]
    selected = args.case or list(cases)
    unknown = set(selected) - cases.keys()
    if unknown:
        raise ValueError(f"unknown cases: {', '.join(sorted(unknown))}; captures need --captures")
    results = []
    for name in selected:
        started = time.monotonic()
        row = {"case": name, "log": str(output / f"{name}.log")}
        try:
            run_logged(cases[name], row["log"], args.timeout)
            row["status"] = "PASS"
        except (ValueError, OSError) as error:
            row.update(status="FAIL", error=str(error))
        row["seconds"] = round(time.monotonic() - started, 2)
        results.append(row)
        print(f"{name}: {row['status']} ({row['seconds']}s)", flush=True)
        if "error" in row:
            print(row["error"], file=sys.stderr, flush=True)
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    failed = sum(row["status"] == "FAIL" for row in results)
    print(f"Godot matrix: {len(results) - failed}/{len(results)} passed; {failed} failed")
    if not args.captures:
        print("Graphical checks not selected (use --captures): " + ", ".join(CAPTURES))
    return int(failed != 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    suite = commands.add_parser("matrix", help="run the public synthetic scene matrix")
    suite.add_argument("--godot", required=True, type=Path)
    suite.add_argument("--out", type=Path, default=ROOT / "work/godot-matrix")
    suite.add_argument("--project", type=Path, default=ROOT / "godot", help="project directory; defaults to this checkout's godot/")
    suite.add_argument("--case", action="append", help="select a case (repeatable); default: all")
    suite.add_argument("--captures", action="store_true", help="also run graphical synthetic captures")
    suite.add_argument("--timeout", type=float, default=180)
    run = commands.add_parser("run", help="gate one Godot command, including private-data scenes")
    run.add_argument("--godot", required=True, type=Path)
    run.add_argument("--log", required=True, type=Path)
    run.add_argument("--timeout", type=float, default=180)
    run.add_argument("--expect", action="append", default=[], help="required exact completion line")
    run.add_argument("arguments", nargs=argparse.REMAINDER, help="-- followed by Godot arguments")
    check = commands.add_parser("check-log", help="reject errors in an existing log")
    check.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        if args.action == "matrix":
            return matrix(args)
        if args.action == "check-log":
            require_clean_log(args.log.read_text(encoding="utf-8", errors="replace"), args.log)
        else:
            arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
            sys.stdout.write(run_logged([args.godot.resolve(), *arguments], args.log, args.timeout, args.expect))
        return 0
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
