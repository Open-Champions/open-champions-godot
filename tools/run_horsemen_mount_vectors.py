#!/usr/bin/env python3
"""Run the committed compressed Horsemen vectors with a manifest case-count gate."""
import argparse
import gzip
import json
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--vectors", required=True)
    parser.add_argument("--manifest", required=True)
    args = parser.parse_args()
    with gzip.open(args.vectors, "rb") as source:
        raw = source.read()
    with open(args.manifest, encoding="utf-8") as source:
        expected = int(json.load(source)["cases"])
    lines = raw.splitlines()
    actual = sum(1 for line in lines if line and not line.startswith((b"#", b"L\t", b"U\t")))
    if not raw or actual != expected:
        raise SystemExit(f"horsemen vector count {actual} != manifest {expected}")
    return subprocess.run([args.exe, str(expected)], input=raw, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
