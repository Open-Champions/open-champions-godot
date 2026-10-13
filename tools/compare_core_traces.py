"""Require identical exact simulation traces across the five native CI hosts."""
import argparse
import gzip
import hashlib
import json
import pathlib
import sys
from core_trace_contract import expected_records, expected_trace
from trace_diagnostics import IncompleteTrace, require_producer, tsv_diagnostic
from simulation_contracts import CONTRACTS, reference_identity

PLATFORMS = {"macos-arm64", "macos-x64", "windows-arm64", "windows-x64", "linux-x64"}
SUITES = set(CONTRACTS)


def validate(directory, manifests, commit):
    reports = {}
    for path in sorted(directory.rglob("*.json")):
        report = json.loads(path.read_text())
        if not isinstance(report, dict):
            raise ValueError("trace report must be an object")
        key = report.get("suite"), report.get("platform")
        if key[0] not in SUITES or key[1] not in PLATFORMS:
            raise ValueError("unexpected trace suite or platform")
        if key in reports:
            raise ValueError("duplicate trace identity")
        reports[key] = (path, report)
    missing = {(suite, platform) for suite in SUITES for platform in PLATFORMS} - reports.keys()
    if missing:
        raise IncompleteTrace("missing required core suite or platform: " +
                              ", ".join(f"{s}/{p}" for s, p in sorted(missing)))
    absent = [str(path.with_suffix(".tsv.gz")) for path, _ in reports.values()
              if not path.with_suffix(".tsv.gz").is_file()]
    if absent:
        raise IncompleteTrace("missing required core trace payload: " + ", ".join(sorted(absent)))
    seen = {}
    coverage_errors = {}
    expectations = {}
    for suite in sorted(SUITES):
        manifest = json.loads((manifests / f"{CONTRACTS[suite][0]}.json").read_text())
        reference_hash = reference_identity(manifest, suite)
        fixture = manifests / f"{CONTRACTS[suite][0]}.tsv.gz"
        with gzip.open(fixture, "rb") as stream:
            vector_bytes = stream.read()
        if hashlib.sha256(vector_bytes).hexdigest() != manifest.get("vector_sha256", manifest.get("vectors_sha256")):
            raise ValueError("committed fixture differs from its manifest")
        expectations[suite] = expected_trace(suite, vector_bytes.splitlines())
    for (suite, platform), (path, report) in sorted(reports.items()):
        key = suite, platform
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
        if rows != report.get("trace_rows"):
            raise ValueError(f"{suite}/{platform}: trace coverage differs from report: "
                             f"actual={rows!r}, reported={report.get('trace_rows')!r}")
        expected_rows = manifest.get("vector_count", manifest.get("rows"))
        if rows != expected_rows:
            coverage_errors[suite, platform] = (rows, expected_rows)
        seen[key] = digest.hexdigest()
    for suite in sorted(SUITES):
        reference = reports[suite, "linux-x64"][0].with_suffix(".tsv.gz")
        divergences = []
        for platform in sorted(PLATFORMS - {"linux-x64"}):
            if seen[suite, platform] != seen[suite, "linux-x64"]:
                peer = reports[suite, platform][0].with_suffix(".tsv.gz")
                with gzip.open(reference, "rb") as left, gzip.open(peer, "rb") as right:
                    index, detail = tsv_diagnostic(suite, left, right, ("linux-x64", platform),
                                                   with_index=True)
                divergences.append((index, platform, detail))
        if divergences:
            _, _, detail = min(divergences)
            raise ValueError(f"{suite}: actual results differ from reference expectation; "
                             f"exact state diverges across platforms\n{detail}")
        if seen[suite, "linux-x64"] != expectations[suite][1]:
            fixture = manifests / f"{CONTRACTS[suite][0]}.tsv.gz"
            with gzip.open(fixture, "rb") as source, gzip.open(reference, "rb") as actual:
                detail = tsv_diagnostic(suite, expected_records(suite, source), actual,
                                        ("reference expectation", "linux-x64"))
            raise ValueError(f"{suite}: actual results differ from reference expectation\n{detail}")
        for platform in sorted(PLATFORMS):
            if (suite, platform) in coverage_errors:
                rows, expected_rows = coverage_errors[suite, platform]
                raise ValueError(f"{suite}/{platform}: trace coverage differs from committed fixture: "
                                 f"actual={rows}, expected={expected_rows}")
        print(f"{suite}: all five native targets match {seen[suite, 'macos-arm64']}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-dir", required=True, type=pathlib.Path)
    parser.add_argument("--manifests", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parent.parent / "native/tests/vectors")
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--native-result", default="success",
                        choices=("success", "failure", "cancelled", "skipped"))
    args = parser.parse_args(argv)
    try:
        require_producer(args.native_result)
        validate(args.trace_dir, args.manifests, args.source_commit)
        return 0
    except IncompleteTrace as error:
        print(f"INCOMPLETE: {error}", file=sys.stderr)
        return 2
    except (ValueError, OSError, EOFError) as error:
        print(f"MISMATCH: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
