"""Prevent presentation arithmetic and dependencies from entering game rules."""
import pathlib
import re
import sys


def simulation_paths(root):
    paths = sorted((root / "native/src").glob("core_*.cpp"))
    paths += sorted((root / "native/include/champions").glob("core_*.hpp"))
    for stem in ("items", "item_generation", "item_names"):
        paths.extend((root / "native/src" / (stem + ".cpp"),
                      root / "native/include/champions" / (stem + ".hpp")))
    paths.append(root / "native/include/champions/combat.hpp")
    paths.append(root / "native/src/combat_common.cpp")
    for stem in ("combat_damage", "combat_stats", "combat_items"):
        paths.extend((root / "native/src" / (stem + ".cpp"),
                      root / "native/include/champions" / (stem + ".hpp")))
    paths.append(root / "native/include/champions/combat_unicode_data.hpp")
    if len(paths) != 21 or not all(path.is_file() for path in paths):
        raise ValueError("expected the ten simulation sources and eleven public headers")
    return paths


def check(root):
    paths = simulation_paths(root)
    allowed = {"champions/" + path.name for path in paths if path.suffix == ".hpp"}
    allowed.update({"algorithm", "array", "cstddef", "cstdint", "deque",
                    "functional", "initializer_list", "iterator", "limits",
                    "map", "memory", "optional", "stdexcept", "string",
                    "type_traits", "utility", "variant", "vector"})
    for path in paths:
        source = path.read_text()
        for header in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', source, re.M):
            if header not in allowed:
                raise ValueError(f"{path.name}: simulation dependency {header}")
        # Ignore documentation and strings; examine the remaining C++ tokens.
        tokens = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', "", source, flags=re.S)
        if re.search(r"\b(?:float|double|__int128)\b", tokens):
            raise ValueError(f"{path.name}: host floating-point or nonportable integer type")
    print("Simulation has no host floating-point types or presentation dependencies")


if __name__ == "__main__":
    check(pathlib.Path(sys.argv[1]))
