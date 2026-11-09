#!/usr/bin/env python3
"""Verify the creature_ai_m1_3 vector fixture against its manifest, expand it
into the build directory and run the native test on it."""
import argparse
import gzip
import hashlib
import json
import pathlib
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--vectors", type=pathlib.Path, required=True)
    parser.add_argument("--work", type=pathlib.Path, required=True)
    args = parser.parse_args()
    packed = args.vectors.read_bytes()
    manifest = json.loads(args.vectors.with_suffix("").with_suffix(".json").read_text())
    assert hashlib.sha256(packed).hexdigest() == manifest["compressed_sha256"], "fixture hash"
    text = gzip.decompress(packed)
    assert len(text) == manifest["expanded_bytes"], "expanded size"
    assert hashlib.sha256(text).hexdigest() == manifest["expanded_sha256"], "expanded hash"
    args.work.mkdir(parents=True, exist_ok=True)
    out = args.work / "creature_ai_m1_3.tsv"
    out.write_bytes(text)
    return subprocess.call([args.exe, str(out)])


if __name__ == "__main__":
    sys.exit(main())
