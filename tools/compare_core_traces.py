"""Require identical exact simulation traces across the five native CI hosts."""
import argparse
import gzip
import hashlib
import json
import pathlib
from core_trace_contract import expected_trace
from simulation_contracts import CONTRACTS, reference_identity

PLATFORMS = {"macos-arm64", "macos-x64", "windows-arm64", "windows-x64", "linux-x64"}
SUITES = set(CONTRACTS)


def validate(directory, manifests, commit):
    seen = {}
    expectations = {}
    for suite in SUITES:
        manifest = json.loads((manifests / f"{CONTRACTS[suite][0]}.json").read_text())
        reference_hash = reference_identity(manifest, suite)
        fixture = manifests / f"{CONTRACTS[suite][0]}.tsv.gz"
        with gzip.open(fixture, "rb") as stream:
            vector_bytes = stream.read()
        if hashlib.sha256(vector_bytes).hexdigest() != manifest.get("vector_sha256", manifest.get("vectors_sha256")):
            raise ValueError("committed fixture differs from its manifest")
        expectations[suite] = expected_trace(suite, vector_bytes.splitlines())
    for path in sorted(directory.rglob("*.json")):
        report = json.loads(path.read_text())
        suite, platform = report.get("suite"), report.get("platform")
        if suite not in SUITES or platform not in PLATFORMS:
            raise ValueError("unexpected trace suite or platform")
        key = suite, platform
        if key in seen:
            raise ValueError("duplicate trace identity")
        manifest_path = manifests / f"{CONTRACTS[suite][0]}.json"
        manifest = json.loads(manifest_path.read_text())
        vector_hash = manifest.get("vector_sha256", manifest.get("vectors_sha256"))
        reference_hash = reference_identity(manifest, suite)
        if report.get("schema_version") != 1 or report.get("source_commit") != commit:
            raise ValueError("trace schema or source commit mismatch")
        if report.get("manifest_sha256") != hashlib.sha256(manifest_path.read_bytes()).hexdigest():
            raise ValueError("trace manifest differs from committed fixture")
        if report.get("vector_sha256") != vector_hash or report.get("reference_sha256") != reference_hash:
            raise ValueError("trace contract provenance mismatch")
        digest = hashlib.sha256()
        rows = 0
        with gzip.open(path.with_suffix(".tsv.gz"), "rb") as stream:
            for line in stream:
                digest.update(line)
                if line.strip() and not line.startswith(b"#"):
                    rows += 1
        if digest.hexdigest() != report.get("trace_sha256"):
            raise ValueError("actual trace bytes differ from report")
        if rows != report.get("trace_rows") or rows != manifest.get("vector_count", manifest.get("rows")):
            raise ValueError("trace coverage differs from committed fixture")
        if (rows, digest.hexdigest()) != expectations[suite]:
            raise ValueError(f"{suite}: actual results differ from reference expectation")
        seen[key] = digest.hexdigest()
    if set(seen) != {(suite, platform) for suite in SUITES for platform in PLATFORMS}:
        raise ValueError("missing required core suite or platform")
    for suite in sorted(SUITES):
        if len({seen[suite, platform] for platform in PLATFORMS}) != 1:
            raise ValueError(f"exact {suite} state diverges across platforms")
        print(f"{suite}: all five native targets match {seen[suite, 'macos-arm64']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-dir", required=True, type=pathlib.Path)
    parser.add_argument("--manifests", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parent.parent / "native/tests/vectors")
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    validate(args.trace_dir, args.manifests, args.source_commit)
