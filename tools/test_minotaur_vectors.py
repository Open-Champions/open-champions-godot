#!/usr/bin/env python3
"""Replay authored Minotaur statistics fixtures without original game data."""
import argparse
import gzip
import pathlib
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--native", required=True)
parser.add_argument("--vectors", type=pathlib.Path, required=True)
args = parser.parse_args()
with gzip.open(args.vectors, "rb") as source:
    result = subprocess.run([args.native, "--stats"], input=source.read(), check=False)
raise SystemExit(result.returncode)
