#!/usr/bin/env python3
"""Group unchanged authored vector blocks for bounded Release compilation."""
import argparse
import hashlib
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    # Compiler granularity only, not a game value or a case-count limit.
    parser.add_argument("--batch-size", type=int, default=64)
    args = parser.parse_args()
    if args.batch_size <= 0:
        parser.error("batch size must be positive")
    source = args.input.read_bytes()
    lines = source.splitlines(keepends=True)
    if len(lines) < 2 or not lines[0].startswith(b"//"):
        parser.error("expected the authored header and CHECK/scoped cases")
    # The generated corpus has ordinary C++ strings/chars, standalone CHECKs
    # and independent brace scopes (some multiline). Keep whole scopes intact.
    quoted = rb'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\r\n]*'
    cases, current, stack = [], [], []
    first_line, mode = 0, b""
    opening = {ord("{"): ord("}"), ord("("): ord(")"), ord("["): ord("]")}
    for number, line in enumerate(lines[1:], 2):
        clean = re.sub(quoted, b"", line).strip()
        if b"/*" in clean or b"*/" in clean:
            parser.error("unsupported block comment in vector cases")
        if not current:
            first_line = number
            mode = b"block" if clean.startswith(b"{") else b"check"
            if mode == b"check" and not clean.startswith(b"CHECK("):
                parser.error(f"unsupported vector statement at line {number}")
        current.append(line)
        for char in clean:
            if char in opening:
                stack.append(opening[char])
            elif char in opening.values():
                if not stack or stack.pop() != char:
                    parser.error(f"unbalanced vector case at line {number}")
        if not stack:
            if not clean.endswith(b"}" if mode == b"block" else b";"):
                parser.error(f"incomplete vector statement at line {number}")
            cases.append((first_line, b"".join(current)))
            current = []
    if current or stack:
        parser.error("unterminated vector case")
    batches = [cases[i:i + args.batch_size] for i in range(0, len(cases), args.batch_size)]
    assert lines[0] + b"".join(case for batch in batches for _, case in batch) == source
    chunks = [lines[0], f"// Input SHA-256: {hashlib.sha256(source).hexdigest()}\n".encode()]
    for i, batch in enumerate(batches):
        chunks.append(f"void level_flow_vector_batch_{i}() {{\n"
                      f'#line {batch[0][0]} "level_flow_vectors.inc"\n'.encode())
        chunks.extend(case for _, case in batch)
        chunks.append(b"\n}\n")
    chunks.append(b'#line 1 "level_flow_vectors_batched.inc"\nvoid reference_vectors() {\n')
    chunks.extend(f"  level_flow_vector_batch_{i}();\n".encode() for i in range(len(batches)))
    chunks.append(b"}\n")
    output = b"".join(chunks)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_bytes() != output:
        args.output.write_bytes(output)
    print(f"level-flow vectors: {len(cases)} cases, {len(batches)} batches, payload unchanged")


if __name__ == "__main__":
    main()
