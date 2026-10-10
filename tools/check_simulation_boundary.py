"""Prevent presentation arithmetic and dependencies from entering game rules."""
import pathlib
import re
import sys


def simulation_paths(root):
    # Check precisely the sources built into game-rule targets. Untracked backup
    # copies are not compiled and must neither weaken nor break this boundary.
    paths = [root / "native/src" / ("core_" + name + ".cpp")
             for name in ("arithmetic", "math", "state")]
    paths += [root / "native/include/champions" / ("core_" + name + ".hpp")
              for name in ("arithmetic", "math", "state")]
    for stem in ("items", "item_generation", "item_names"):
        paths.extend((root / "native/src" / (stem + ".cpp"),
                      root / "native/include/champions" / (stem + ".hpp")))
    paths.append(root / "native/include/champions/combat.hpp")
    paths.append(root / "native/src/combat_common.cpp")
    for stem in ("combat_damage", "combat_stats", "combat_items"):
        paths.extend((root / "native/src" / (stem + ".cpp"),
                      root / "native/include/champions" / (stem + ".hpp")))
    paths.append(root / "native/include/champions/combat_unicode_data.hpp")
    paths.append(root / "native/include/champions/amx.hpp")
    paths += [root / "native/src" / ("amx_" + name + ".cpp")
              for name in ("image", "execute", "host")]
    paths.append(root / "native/include/champions/netwire.hpp")
    paths += [root / "native/src" / ("netwire_" + name + ".cpp")
              for name in ("codec", "session", "tick")]
    paths += [root / "native/src/session.cpp", root / "native/include/champions/session.hpp"]
    paths += [root / "native/include/champions/objects.hpp"]
    paths += [root / "native/src" / ("objects_" + name + ".cpp")
              for name in ("lifetime", "scheduler", "queries")]
    paths += [root / "native/src/objects_internal.hpp"]
    paths += [root / "native/src/creature_hp.cpp", root / "native/include/champions/creature_hp.hpp"]
    paths += [root / "native/src/combat_creature.cpp", root / "native/include/champions/combat_creature.hpp"]
    paths += [root / "native/src/raw_handles.cpp", root / "native/include/champions/raw_handles.hpp"]
    if len(paths) != 42 or not all(path.is_file() for path in paths):
        raise ValueError("expected the twenty-three simulation sources, eighteen public headers and one internal header")
    return paths


def check(root):
    paths = simulation_paths(root)
    allowed = {"champions/" + path.name for path in paths if path.suffix == ".hpp"}
    allowed.add("objects_internal.hpp")
    allowed.update({"algorithm", "array", "cstddef", "cstdint", "deque",
                    "exception", "functional", "initializer_list", "iterator", "limits",
                    "map", "memory", "optional", "set", "stdexcept", "string",
                    "tuple", "type_traits", "utility", "variant", "vector"})
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
