"""Shared Godot log gate for development, CI and exported-app checks."""

import re


ANSI_ESCAPE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ERROR = re.compile(
    r"\b(?:SCRIPT ERROR|USER ERROR|ERROR|Parse Error|Compile Error|Shader Error):"
    r"|<C\+\+ Error>"
    r'|\bCondition "[^"\n]+" is true'
    r'|\bParameter "[^"\n]+" is null'
    r"|\bIndex .+ out of bounds"
)

# No errors are currently proven benign. Entries must be (compiled full-line
# pattern, evidence/reason); never exempt whole error categories or scenes.
# See docs/GODOT_MATRIX.md before adding an entry.
ALLOW_LIST = ()


def error_lines(output):
    """Return (1-based line number, diagnostic), preserving the original log."""
    found = []
    for number, raw in enumerate(output.splitlines(), 1):
        line = ANSI_ESCAPE.sub("", raw)
        if ERROR.search(line) and not any(pattern.fullmatch(line) for pattern, _ in ALLOW_LIST):
            found.append((number, line))
    return found


def require_clean_log(output, log):
    errors = error_lines(output)
    if errors:
        details = "\n".join(f"  {log}:{number}: {line}" for number, line in errors[:12])
        raise ValueError(f"{len(errors)} Godot error diagnostic(s); see {log}\n{details}")
