#!/usr/bin/env python3
"""Authored synthetic statistics vectors from research-minotaur 27eb6ae.

Only the published Python arithmetic model is imported. No game input is read.
"""
import argparse
import gzip
import importlib.util
import pathlib
import random
import subprocess

PIN = "27eb6ae309c72e15a31fc4b04d012b833f15031f"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--research", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    head = subprocess.check_output(["git", "-C", str(args.research), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise SystemExit("research pin mismatch")
    spec = importlib.util.spec_from_file_location("minotaur_reference", args.research / "tools/ref_combat.py")
    model = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(model)
    tables = model.Tables()
    tables.fixed_levels = ("stomedal", "innpit")
    factors = tuple(model.fmul(model.itof(value), 0x3C23D70A) for value in (600, 200, 250, 100, 250))
    rows = []
    for seed in (20973, 7):
        rng = random.Random(seed)
        levels = [model.f32(float(level)) for level in range(1, 161)]
        levels += [model.level_scale(tables, 20, difficulty, 0, "valor") for difficulty in range(1, 6)]
        for level in levels:
            for difficulty in range(1, 6):
                for residue in range(5):
                    own = rng.getrandbits(32)
                    while model.Seed(own).draw() % 5 != residue:
                        own = rng.getrandbits(32)
                    stream = model.Seed(own)
                    draw = stream.draw()
                    world = model.World(seed=seed)
                    world.monster_level, world.difficulty = level, difficulty
                    world.num_players, world.level_name = 1, "valor"
                    result = model.monster_stats(tables, world, factors, 0, "Minotaur", True, draw)
                    values = (level, difficulty, own, draw, stream.state,
                              *(result[k] for k in ("hp", "max_hp", "armor", "dmg_low", "dmg_high", "attack", "exp_value")))
                    rows.append(" ".join(map(str, values)) + "\n")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("wb") as output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=output, mtime=0) as compressed:
            compressed.write("".join(rows).encode("ascii"))
    print(f"{len(rows)} synthetic statistics cases")


if __name__ == "__main__":
    main()
