"""Run pinned Python-generated fixtures through a native exact-result runner."""
import argparse
import gzip
import hashlib
import json
import pathlib
import subprocess
from core_trace_contract import expected_trace
from simulation_contracts import CONTRACTS, reference_identity


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", required=True, choices=tuple(CONTRACTS))
    for name in ("exe", "vectors", "manifest", "trace-dir"):
        parser.add_argument("--" + name, required=True, type=pathlib.Path)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    destination = args.trace_dir / args.suite
    destination.mkdir(parents=True, exist_ok=True)
    actual = destination / "actual.tsv"
    compressed = args.trace_dir / (args.suite + ".tsv.gz")
    report_path = args.trace_dir / (args.suite + ".json")
    # A failed rerun must never leave an earlier result looking current.
    for path in (actual, compressed, report_path):
        path.unlink(missing_ok=True)
    manifest = json.loads(args.manifest.read_text())
    vector_hash = manifest.get("vector_sha256", manifest.get("vectors_sha256"))
    reference_hash = reference_identity(manifest, args.suite)
    plain = destination / "vectors.tsv"
    with gzip.open(args.vectors, "rb") as source, plain.open("wb") as target:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            target.write(chunk)
    if sha256(plain) != vector_hash:
        raise ValueError("decompressed vectors do not match pinned manifest")
    with plain.open(encoding="ascii") as source:
        expected_count, expected_hash = expected_trace(args.suite, source)
    subprocess.run([str(args.exe.resolve()), str(plain), str(actual)], check=True)
    # C++ text streams use CRLF on Windows. Compare the actual value records,
    # with one LF separator, rather than platform text-stream conventions.
    digest = hashlib.sha256()
    count = 0
    with actual.open(encoding="ascii", newline=None) as source, compressed.open("wb") as output:
        with gzip.GzipFile(filename="", fileobj=output, mode="wb", mtime=0) as target:
            for line in source:
                data = line.encode("ascii")
                digest.update(data)
                target.write(data)
                if line.strip() and not line.startswith("#"):
                    count += 1
    if count != manifest.get("vector_count", manifest.get("rows")):
        raise ValueError("runner trace coverage differs from the pinned fixture")
    if count != expected_count or digest.hexdigest() != expected_hash:
        raise ValueError("actual trace differs from Python-generated expected results")
    report = {
        "schema_version": 1, "suite": args.suite, "platform": args.platform,
        "source_commit": args.source_commit,
        "reference_sha256": reference_hash,
        "vector_sha256": vector_hash,
        "manifest_sha256": sha256(args.manifest),
        "trace_rows": count, "trace_sha256": digest.hexdigest(),
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"{args.suite}: {count} exact result records; SHA-256 {digest.hexdigest()}")


if __name__ == "__main__":
    main()
