#pragma once

#include "champions/lighting.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The per-level light record tools/fx_level_lights.py --json writes from the user's
// executable (docs/lighting.md section 10, research t3/research-fx 8e66747). Colours and
// the sun direction are the marker objects; the executable row supplies only the flag
// word. No retail value is compiled in. RTA only.
namespace champions::rta::lighting {

struct LevelLightsError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The load tail stores each marker int's low 8 bits (0x1e60e0). A value outside a byte
// and the truncated colour agree only when the int already fits in a byte.
int stored_channel(int value);

struct LevelRow {
    int index = 0;
    std::string name;
    unsigned flags = 0;
    int base_level = 0x80;
    bool has_ambient = false;
    bool has_sun = false;
    bool has_direction = false;
    std::array<int, 3> ambient{{0, 0, 0}};
    std::array<int, 3> sun{{0, 0, 0}};
    Vec3 direction{};
};

struct LevelLights {
    std::string executable_sha256;
    std::array<int, 4> default_ambient{{0x20, 0x20, 0x20, 0}};
    std::array<int, 4> default_sun{{0x44, 0x44, 0x44, 0}};
    Vec3 default_direction{0.0, -1.0, 1.0};
    int default_base = 0x80;
    unsigned default_flags = 0;
    int default_index = 0;
    std::vector<LevelRow> levels;
};

// game_loadLevel's three compares: name + "_" + sub, then a row name that starts with
// the plain name, then a row name that occurs inside it. No hit is the null-name row.
const LevelRow *lookup_level(const LevelLights &table, std::string_view name, std::string_view sub = {});

struct ResolvedLevelLight {
    Level level;    // runtime record 0x5d3448: normalised direction, low-8-bit colours, base
    Object object;  // light_resetObjectLight: fill is trunc(0.79999995 x ambient as set)
    int index = 0;
    bool default_row = false;
    bool marker_ambient = false;
    bool marker_sun = false;
    bool marker_direction = false;
};

// Parse the extractor JSON. Refuses a shape that is not that file, a base level that
// disagrees with the flag word, or a files[] marker that disagrees with its row.
LevelLights load_level_lights(std::string_view json);

// The row lookup selects, then the defaults fill any marker the row does not carry.
// The result is what light_setLevelLight stores and what Object::reset derives.
ResolvedLevelLight resolve_level_light(const LevelLights &table, std::string_view name,
                                       std::string_view sub = {});

} // namespace champions::rta::lighting
