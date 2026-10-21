#!/usr/bin/env python3
"""Check the strings vectors against their manifest and pipe them to the C++ test.

    python3 tools/run_strings_vectors.py --exe build/.../champions-strings-tests \\
        --vectors native/tests/vectors/strings.tsv.gz --manifest native/tests/vectors/strings.json
"""
import argparse
import gzip
import hashlib
import json
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--exe", required=True)
    parser.add_argument("--vectors", required=True)
    parser.add_argument("--manifest", required=True)
    args = parser.parse_args()
    raw = gzip.decompress(open(args.vectors, "rb").read())
    manifest = json.load(open(args.manifest))
    got = hashlib.sha256(raw).hexdigest()
    if got != manifest["vectors_sha256"]:
        print("vectors hash %s, manifest %s" % (got, manifest["vectors_sha256"]))
        return 1
    print("vectors %s from research %s, %d bytes" % (got[:16], manifest["reference_commit"], len(raw)))
    done = subprocess.run([args.exe], input=raw)
    return done.returncode


if __name__ == "__main__":
    sys.exit(main())
