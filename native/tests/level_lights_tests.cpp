// Synthetic tests for the per-level light record (docs/lighting.md section 10).
// The cases are the research tests in tools/test_fx_level_lights.py and the explicit
// level-light cases in tools/difftest_lighting.py. No retail value is in this file.
#include "champions/fx_rz.hpp"
#include "champions/fx_skills.hpp"
#include "champions/level_lights.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace L = champions::rta::lighting;
namespace fx = champions::rta::fx::skills;
using champions::rta::fx::rounding::rz;

namespace {
int checks = 0, failures = 0;

void check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << what << '\n';
    }
}

std::string read_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool throws(const std::string &json) {
    try {
        L::load_level_lights(json);
        return false;
    } catch (const L::LevelLightsError &) {
        return true;
    }
}

const char *kTable = R"json({
 "executable_sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
 "level_table": 5206544,
 "row_bytes": 48,
 "flag_offset": 32,
 "marker_block": 4521440,
 "runtime_record": 6108232,
 "defaults": {"ambient": [32, 32, 32, 0], "sun": [68, 68, 68, 0], "direction": [0.0, -1.0, 1.0], "base_level": 128},
 "levels": [
  {"index": 0, "name": "alpha", "flags": 67108864, "base_level": 64,
   "ambient": [50, 30, 20], "sun": [150, 100, 50], "direction": [0.0, -1.0, 1.0]},
  {"index": 1, "name": "alpha_extra", "flags": 1, "base_level": 128},
  {"index": 2, "name": "beta", "flags": 536870912, "base_level": 32,
   "ambient": [10, 20, 30], "sun": [40, 50, 60], "direction": [0.0, -2.0, 2.0]}
 ],
 "default": {"index": 3, "flags": 67108864, "base_level": 64},
 "files": [
  {"source": "alpha.lmp", "level_index": 0, "ambient": [50, 30, 20], "sun": [150, 100, 50], "direction": [0.0, -1.0, 1.0]}
 ]
})json";

// 0x04000000 = 67108864, 0x20000000 = 536870912, 0x4f7210 = 5206544,
// 0x44fde0 = 4521440, 0x5d3448 = 6108232.

void test_lookup_and_markers() {
    const auto table = L::load_level_lights(kTable);
    check(table.levels.size() == 3 && table.default_index == 3, "three named rows and the null row");
    check(L::lookup_level(table, "alpha", "extra")->index == 1, "exact name_sub wins");
    check(L::lookup_level(table, "alpha")->index == 0, "the plain name hits the first prefix");
    check(L::lookup_level(table, "zz_beta_zz")->index == 2, "a row name inside the query");
    check(L::lookup_level(table, "missing") == nullptr, "no hit is the null-name row");

    const L::Vec3 ambient_pos{100, 60, 41}, sun_pos{300, 200, 100}, direction{0, -1, 1};
    const auto via = L::Level::from_markers(&ambient_pos, &sun_pos, &direction, 0x04000000u | 0x20000000u);
    // Both flags: 0x04000000 wins, so this row's 0x04000000 alone is also base 0x40.
    check(L::Level::base_level(0x04000000u | 0x20000000u) == 0x40, "both light flags yield 0x40");
    const auto got = L::resolve_level_light(table, "alpha");
    check(got.index == 0 && !got.default_row && got.marker_ambient && got.marker_sun && got.marker_direction,
          "alpha uses its markers");
    check(got.level.base == 0x40 && got.level.ambient[0] == 50 && got.level.ambient[1] == 30 &&
              got.level.ambient[2] == 20 && got.level.sun[0] == 150 && got.level.ambient[3] == 0 &&
              got.level.sun[3] == 0,
          "marker colours and the flag base");
    check(got.level.ambient == via.ambient && got.level.sun[0] == 150, "colours match from_markers where they fit a byte");
    check(std::fabs(got.level.sun_direction.y - via.sun_direction.y) < 1e-12 &&
              std::fabs(got.level.sun_direction.z - (-got.level.sun_direction.y)) < 1e-12,
          "direction (0, -1, 1) is normalised");
    const auto fill = L::Object::reset(got.level);
    check(got.object.fill == fill.fill && got.object.sun == fill.sun && got.object.ambient == fill.ambient,
          "fill comes from light_resetObjectLight");
    check(got.object.fill[0] == static_cast<int>(rz(50.0 * L::fill_factor())) &&
              got.object.fill[1] == static_cast<int>(rz(30.0 * L::fill_factor())) &&
              got.object.fill[2] == static_cast<int>(rz(20.0 * L::fill_factor())) && got.object.fill[3] == 0,
          "fill is trunc(0.79999995 x ambient as set)");

    const auto bare = L::resolve_level_light(table, "alpha_extra");
    check(!bare.marker_ambient && bare.level.ambient[0] == 0x20 && bare.level.sun[0] == 0x44 && bare.level.base == 0x80,
          "a row with no markers keeps the documented defaults");
    const auto missed = L::resolve_level_light(table, "missing");
    check(missed.default_row && missed.index == 3 && missed.level.base == 0x40 && missed.level.ambient[0] == 0x20,
          "a miss uses the null row's base and the marker defaults");
}

void test_reference_cases() {
    // The four explicit difftest cases, stored as the extractor would store them
    // (truncated marker colours, not the positions).
    const char *json = R"json({
 "executable_sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
 "level_table": 5206544, "row_bytes": 48, "flag_offset": 32, "marker_block": 4521440, "runtime_record": 6108232,
 "defaults": {"ambient": [32, 32, 32, 0], "sun": [68, 68, 68, 0], "direction": [0.0, -1.0, 1.0], "base_level": 128},
 "levels": [
  {"index": 0, "name": "none", "flags": 0, "base_level": 128},
  {"index": 1, "name": "both", "flags": 603979776, "base_level": 64,
   "ambient": [10, 20, 30], "sun": [40, 50, 60], "direction": [0.0, -2.0, 2.0]},
  {"index": 2, "name": "low", "flags": 536870912, "base_level": 32,
   "ambient": [50, 30, 20], "sun": [150, 100, 50], "direction": [0.0, 0.0, 5.0]},
  {"index": 3, "name": "partial", "flags": 67108864, "base_level": 64, "ambient": [0, 0, 20]}
 ],
 "default": {"index": 4, "flags": 0, "base_level": 128}
})json";
    // 0x04000000|0x20000000 = 603979776.
    const auto table = L::load_level_lights(json);
    const auto none = L::resolve_level_light(table, "none");
    const auto defaults = L::Level::from_markers();
    check(none.level.ambient == defaults.ambient && none.level.sun == defaults.sun && none.level.base == 0x80,
          "absent markers are the load-record defaults");
    check(std::fabs(none.level.sun_direction.y - defaults.sun_direction.y) < 1e-12, "default direction");

    const L::Vec3 a{20, 40, 60}, s{80, 100, 120}, d{0, -2, 2};
    const auto both_ref = L::Level::from_markers(&a, &s, &d, 0x04000000u | 0x20000000u);
    const auto both = L::resolve_level_light(table, "both");
    check(both.level.ambient == both_ref.ambient && both.level.sun == both_ref.sun && both.level.base == both_ref.base,
          "both-flags case matches from_markers");
    check(std::fabs(both.level.sun_direction.x - both_ref.sun_direction.x) < 1e-12 &&
              std::fabs(both.level.sun_direction.y - both_ref.sun_direction.y) < 1e-12 &&
              std::fabs(both.level.sun_direction.z - both_ref.sun_direction.z) < 1e-12,
          "ramped direction matches from_markers");
    check(both.object.fill == L::Object::reset(both_ref).fill, "both-flags fill");

    const L::Vec3 a2{100, 60, 41}, s2{300, 200, 100}, d2{0, 0, 5};
    const auto low_ref = L::Level::from_markers(&a2, &s2, &d2, 0x20000000u);
    const auto low = L::resolve_level_light(table, "low");
    check(low.level.base == 0x20 && low.level.sun_direction.z == 1.0 && low.level.ambient == low_ref.ambient &&
              low.level.sun == low_ref.sun,
          "0x20000000 case");

    const L::Vec3 a3{0.1, 1.0, 41.0};
    const auto part_ref = L::Level::from_markers(&a3, nullptr, nullptr, 0x04000000u);
    const auto part = L::resolve_level_light(table, "partial");
    check(part.level.ambient == part_ref.ambient && part.level.sun == part_ref.sun && part.level.base == 0x40 &&
              !part.marker_sun && !part.marker_direction,
          "a partial marker keeps the other defaults");
}

void test_low_byte() {
    const char *json = R"json({
 "executable_sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
 "level_table": 5206544, "row_bytes": 48, "flag_offset": 32, "marker_block": 4521440, "runtime_record": 6108232,
 "defaults": {"ambient": [32, 32, 32, 0], "sun": [68, 68, 68, 0], "direction": [0.0, -1.0, 1.0], "base_level": 128},
 "levels": [
  {"index": 0, "name": "wide", "flags": 0, "base_level": 128, "ambient": [256, -1, 300], "sun": [511, 44, -256]}
 ],
 "default": {"index": 1, "flags": 0, "base_level": 128}
})json";
    const auto got = L::resolve_level_light(L::load_level_lights(json), "wide");
    check(L::stored_channel(256) == 0 && L::stored_channel(-1) == 255 && L::stored_channel(300) == 44, "low 8 bits");
    check(got.level.ambient[0] == 0 && got.level.ambient[1] == 255 && got.level.ambient[2] == 44, "ambient byte store");
    check(got.level.sun[0] == 255 && got.level.sun[1] == 44 && got.level.sun[2] == 0, "sun byte store");
    check(got.object.fill[1] == static_cast<int>(rz(255.0 * L::fill_factor())), "fill uses the stored ambient byte");
}

void test_refusals() {
    std::string bad = kTable;
    const auto sha = bad.find("0123456789");
    bad.replace(sha, 10, "0123");
    check(throws(bad), "a short hash is refused");
    bad = kTable;
    const auto table = bad.find("5206544");
    bad.replace(table, 7, "5206545");
    check(throws(bad), "a different level table is refused");
    bad = kTable;
    const auto base = bad.find("\"base_level\": 64");
    bad.replace(base, std::string("\"base_level\": 64").size(), "\"base_level\": 128");
    check(throws(bad), "a base level that disagrees with the flags is refused");
    check(throws(std::string(kTable) + "x"), "trailing text is refused");
    bad = kTable;
    const auto amb = bad.find("[50, 30, 20]");
    // The files[] copy must agree. Change only the row and the file still says 50.
    bad.replace(amb, std::string("[50, 30, 20]").size(), "[51, 30, 20]");
    check(throws(bad), "a file marker that differs from its row is refused");
}

int count_levels(const std::string &path) {
    const auto table = L::load_level_lights(read_file(path));
    int lit = 0;
    for (const auto &row : table.levels) {
        const auto got = L::resolve_level_light(table, row.name);
        if (got.index == row.index && got.level.base == row.base_level) ++lit;
    }
    const auto miss = L::resolve_level_light(table, "\x01no-such-level");
    if (miss.default_row) ++lit;
    std::cout << "levels_lit " << lit << " named_rows " << table.levels.size() << "\n";
    return 0;
}

int count_recipes(const std::string &path) {
    try {
        const auto constants = fx::RecipeConstants::from_json(read_file(path));
        std::set<std::string> classes;
        int lights = 0;
        for (const auto &call : constants.calls()) classes.insert(call.cls);
        for (const char *name : {"StaticFire_ctor", "StaticFire_ctor2", "SpellPlanarSteelEffect_ctor"})
            lights += constants.light_record(name) != nullptr;
        std::cout << "recipes_accepted 1 classes " << classes.size() << " calls " << constants.calls().size()
                  << " light_records " << lights << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cout << "recipes_accepted 0\n";
        std::cerr << e.what() << '\n';
        return 1;
    }
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--count-levels") return count_levels(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--count-recipes") return count_recipes(argv[2]);
        test_lookup_and_markers();
        test_reference_cases();
        test_low_byte();
        test_refusals();
    } catch (const std::exception &e) {
        std::cerr << "FAIL exception " << e.what() << '\n';
        return 1;
    }
    std::cout << "level-lights: " << checks << " checks, " << failures << " failed\n";
    return failures ? 1 : 0;
}
