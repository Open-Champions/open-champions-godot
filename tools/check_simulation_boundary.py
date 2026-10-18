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
    paths += [root / "native/include/champions/world_queries.hpp",
              root / "native/src/world_terrain.cpp", root / "native/src/world_movement.cpp",
              root / "native/src/world_queries_internal.hpp"]
    paths += [root / "native/include/champions/terrain_file.hpp", root / "native/src/terrain_file.cpp"]
    paths += [root / "native/include/champions/session_control.hpp", root / "native/src/session_control.cpp"]
    paths += [root / "native/src/creature_movement.cpp", root / "native/include/champions/creature_movement.hpp"]
    paths += [root / "native/src/creature_water.cpp", root / "native/include/champions/creature_water.hpp"]
    paths += [root / "native/src/animation_clock.cpp", root / "native/include/champions/animation_clock.hpp"]
    paths += [root / "native/src/animation_decoder.cpp", root / "native/include/champions/animation_decoder.hpp"]
    paths += [root / "native/src/animation_runtime.cpp", root / "native/include/champions/animation_runtime.hpp"]
    paths += [root / "native/src/native_save.cpp", root / "native/include/champions/native_save.hpp",
              root / "native/include/champions/detail/checkpoint.hpp",
              root / "native/include/champions/detail/checkpoint_fields.hpp"]
    for stem in ("player", "session_recovery"):
        paths += [root / "native/src" / (stem + ".cpp"),
                  root / "native/include/champions" / (stem + ".hpp")]
    paths.append(root / "native/include/champions/detail/skill_kernel.hpp")
    paths.append(root / "native/include/champions/session_anim_cache.hpp")
    paths += [root / "native/include/champions/control_anim_cache.hpp",
              root / "native/src/control_anim_cache.cpp",
              root / "native/include/champions/detail/checkpoint_control_anim_cache.hpp"]
    paths.append(root / "native/include/champions/play_session.hpp")
    paths += [root / "native/src/level_play.cpp", root / "native/include/champions/level_play.hpp"]
    paths += [root / "native/src/combat_play.cpp", root / "native/include/champions/combat_play.hpp"]
    paths += [root / "native/src/play_animation.cpp", root / "native/include/champions/play_animation.hpp"]
    if len(paths) != 80 or not all(path.is_file() for path in paths):
        raise ValueError("expected 80 simulation sources and headers")
    return paths


def check(root):
    paths = simulation_paths(root)
    allowed = {"champions/" + path.name for path in paths if path.suffix == ".hpp"}
    allowed.update({"champions/creature_anim/dispatch.hpp", "champions/session_level_flow.hpp", "champions/level_flow_water_adapter.hpp", "champions/combat_runtime.hpp", "champions/session_combat_runtime_adapter.hpp", "objects_internal.hpp", "world_queries_internal.hpp",
                    "champions/detail/checkpoint.hpp", "champions/detail/checkpoint_fields.hpp",
                    "champions/detail/skill_kernel.hpp",
                    "champions/detail/checkpoint_control_anim_cache.hpp"})
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
