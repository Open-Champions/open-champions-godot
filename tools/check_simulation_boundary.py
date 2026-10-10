"""Prevent presentation arithmetic and dependencies from entering game rules."""
import pathlib
import re
import sys


def check(root):
    paths = sorted((root / "native/src").glob("core_*.cpp"))
    paths += sorted((root / "native/include/champions").glob("core_*.hpp"))
    if len(paths) != 6:
        raise ValueError("expected the three core sources and public headers")
    for path in paths:
        source = path.read_text()
        for header in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', source, re.M):
            if header in ("cmath", "math.h", "random") or (
                header.startswith("champions/") and not header.startswith("champions/core_")):
                raise ValueError(f"{path.name}: simulation dependency {header}")
        # Ignore documentation and strings; examine the remaining C++ tokens.
        tokens = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', "", source, flags=re.S)
        if re.search(r"\b(?:float|double|__int128)\b", tokens):
            raise ValueError(f"{path.name}: host floating-point or nonportable integer type")
    print("Simulation has no host floating-point types or presentation dependencies")


if __name__ == "__main__":
    check(pathlib.Path(sys.argv[1]))
