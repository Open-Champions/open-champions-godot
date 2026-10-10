"""Prevent presentation arithmetic and dependencies from entering game rules."""
import pathlib
import re
import sys


def check(root):
    paths = sorted((root / "native/src").glob("core_*.cpp"))
    paths += sorted((root / "native/include/champions").glob("core_*.hpp"))
    for stem in ("items", "item_generation", "item_names"):
        paths.extend((root / "native/src" / (stem + ".cpp"),
                      root / "native/include/champions" / (stem + ".hpp")))
    if len(paths) != 12 or not all(path.is_file() for path in paths):
        raise ValueError("expected the six simulation sources and public headers")
    allowed = {"champions/" + path.name for path in paths if path.suffix == ".hpp"}
    for path in paths:
        source = path.read_text()
        for header in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', source, re.M):
            if header in ("cmath", "math.h", "random") or (
                header.startswith("champions/") and header not in allowed):
                raise ValueError(f"{path.name}: simulation dependency {header}")
        # Ignore documentation and strings; examine the remaining C++ tokens.
        tokens = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', "", source, flags=re.S)
        if re.search(r"\b(?:float|double|__int128)\b", tokens):
            raise ValueError(f"{path.name}: host floating-point or nonportable integer type")
    print("Simulation has no host floating-point types or presentation dependencies")


if __name__ == "__main__":
    check(pathlib.Path(sys.argv[1]))
