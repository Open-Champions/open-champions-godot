#include "champions/level_lights.hpp"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <utility>

namespace champions::rta::lighting {
namespace {

constexpr unsigned kLevelTable = 0x4F7210u;
constexpr unsigned kRowBytes = 0x30u;
constexpr unsigned kFlagOffset = 0x20u;
constexpr unsigned kMarkerBlock = 0x44FDE0u;
constexpr unsigned kRuntimeRecord = 0x5D3448u;

struct Json {
    enum class Kind : std::uint8_t { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool flag = false;
    bool is_float = false;
    std::string text;
    double number = 0;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;
    const Json *get(std::string_view key) const {
        for (const auto &m : members)
            if (m.first == key) return &m.second;
        return nullptr;
    }
};

[[noreturn]] void refuse(const std::string &why) { throw LevelLightsError("level light JSON: " + why); }

struct Reader {
    std::string_view t;
    std::size_t p = 0;
    unsigned depth = 0;
    void space() {
        while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\r' || t[p] == '\t')) ++p;
    }
    bool word(std::string_view w) {
        if (t.substr(p, w.size()) == w) {
            p += w.size();
            return true;
        }
        return false;
    }
    [[noreturn]] void fail(const char *what) const { refuse(std::string(what) + " at offset " + std::to_string(p)); }
    static void put_utf8(std::string &out, unsigned cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    unsigned hex4() {
        if (p + 4 > t.size()) fail("short \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = t[p++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("bad \\u escape");
        }
        return v;
    }
    std::string string() {
        if (p >= t.size() || t[p] != '"') fail("string expected");
        ++p;
        std::string out;
        for (;;) {
            if (p >= t.size()) fail("unterminated string");
            const char c = t[p++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (p >= t.size()) fail("bad escape");
            const char e = t[p++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned cp = hex4();
                if (cp >= 0xD800 && cp < 0xDC00) {
                    if (!word("\\u")) fail("lone surrogate");
                    const unsigned lo = hex4();
                    if (lo < 0xDC00 || lo >= 0xE000) fail("bad surrogate pair");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                put_utf8(out, cp);
                break;
            }
            default: fail("bad escape");
            }
        }
    }
    Json value() {
        if (++depth > 64) fail("nesting too deep");
        space();
        if (p >= t.size()) fail("value expected");
        Json out;
        const char c = t[p];
        if (c == '"') {
            out.kind = Json::Kind::String;
            out.text = string();
        } else if (c == '{') {
            out.kind = Json::Kind::Object;
            ++p;
            space();
            if (p < t.size() && t[p] == '}') {
                ++p;
            } else {
                for (;;) {
                    space();
                    const std::string key = string();
                    space();
                    if (p >= t.size() || t[p] != ':') fail("':' expected");
                    ++p;
                    for (const auto &m : out.members)
                        if (m.first == key) fail("duplicate key");
                    out.members.emplace_back(key, value());
                    space();
                    if (p < t.size() && t[p] == ',') {
                        ++p;
                        continue;
                    }
                    if (p < t.size() && t[p] == '}') {
                        ++p;
                        break;
                    }
                    fail("object not closed");
                }
            }
        } else if (c == '[') {
            out.kind = Json::Kind::Array;
            ++p;
            space();
            if (p < t.size() && t[p] == ']') {
                ++p;
            } else {
                for (;;) {
                    out.items.push_back(value());
                    space();
                    if (p < t.size() && t[p] == ',') {
                        ++p;
                        continue;
                    }
                    if (p < t.size() && t[p] == ']') {
                        ++p;
                        break;
                    }
                    fail("array not closed");
                }
            }
        } else if (word("true")) {
            out.kind = Json::Kind::Bool;
            out.flag = true;
        } else if (word("false")) {
            out.kind = Json::Kind::Bool;
        } else if (word("null")) {
            out.kind = Json::Kind::Null;
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            const std::size_t start = p;
            if (t[p] == '-') ++p;
            if (p >= t.size() || t[p] < '0' || t[p] > '9') fail("bad number");
            if (t[p] == '0') ++p;
            else
                while (p < t.size() && t[p] >= '0' && t[p] <= '9') ++p;
            bool flt = false;
            if (p < t.size() && t[p] == '.') {
                flt = true;
                ++p;
                if (p >= t.size() || t[p] < '0' || t[p] > '9') fail("bad fraction");
                while (p < t.size() && t[p] >= '0' && t[p] <= '9') ++p;
            }
            if (p < t.size() && (t[p] == 'e' || t[p] == 'E')) {
                flt = true;
                ++p;
                if (p < t.size() && (t[p] == '+' || t[p] == '-')) ++p;
                if (p >= t.size() || t[p] < '0' || t[p] > '9') fail("bad exponent");
                while (p < t.size() && t[p] >= '0' && t[p] <= '9') ++p;
            }
            out.kind = Json::Kind::Number;
            out.is_float = flt;
            out.text = std::string(t.substr(start, p - start));
            char *end = nullptr;
            out.number = std::strtod(out.text.c_str(), &end);
            if (end != out.text.c_str() + out.text.size() || !std::isfinite(out.number)) fail("bad number");
        } else {
            fail("value expected");
        }
        --depth;
        return out;
    }
};

const Json &member(const Json &obj, const char *key, const std::string &where) {
    const Json *v = obj.get(key);
    if (!v) refuse(where + ": missing " + key);
    return *v;
}

void only_keys(const Json &obj, const std::string &where, std::initializer_list<const char *> keys) {
    for (const auto &m : obj.members) {
        bool known = false;
        for (const char *k : keys)
            if (m.first == k) known = true;
        if (!known) refuse(where + ": unexpected key " + m.first);
    }
}

int json_i32(const Json &v, const std::string &where) {
    if (v.kind != Json::Kind::Number || v.is_float) refuse(where + ": integer expected");
    errno = 0;
    char *end = nullptr;
    const long long n = std::strtoll(v.text.c_str(), &end, 10);
    if (end != v.text.c_str() + v.text.size() || errno == ERANGE || n < std::numeric_limits<int>::min() ||
        n > std::numeric_limits<int>::max())
        refuse(where + ": integer out of range");
    return static_cast<int>(n);
}

unsigned json_u32(const Json &v, const std::string &where) {
    if (v.kind != Json::Kind::Number || v.is_float || v.text.empty() || v.text[0] == '-')
        refuse(where + ": unsigned integer expected");
    errno = 0;
    char *end = nullptr;
    const unsigned long long n = std::strtoull(v.text.c_str(), &end, 10);
    if (end != v.text.c_str() + v.text.size() || errno == ERANGE || n > 0xFFFFFFFFull)
        refuse(where + ": unsigned integer out of range");
    return static_cast<unsigned>(n);
}

double json_f64(const Json &v, const std::string &where) {
    if (v.kind != Json::Kind::Number || !std::isfinite(v.number)) refuse(where + ": finite number expected");
    return v.number;
}

std::array<int, 3> json_i3(const Json &v, const std::string &where) {
    if (v.kind != Json::Kind::Array || v.items.size() != 3) refuse(where + ": three integers expected");
    return {json_i32(v.items[0], where), json_i32(v.items[1], where), json_i32(v.items[2], where)};
}

Vec3 json_vec3(const Json &v, const std::string &where) {
    if (v.kind != Json::Kind::Array || v.items.size() != 3) refuse(where + ": three numbers expected");
    return {json_f64(v.items[0], where), json_f64(v.items[1], where), json_f64(v.items[2], where)};
}

bool same_i3(const std::array<int, 3> &a, const std::array<int, 3> &b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

bool same_vec(const Vec3 &a, const Vec3 &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

void require_base(unsigned flags, int base, const std::string &where) {
    if (base != Level::base_level(flags))
        refuse(where + ": base_level does not match the flag word");
}

std::array<int, 3> triple(const std::array<int, 4> &c) { return {c[0], c[1], c[2]}; }

} // namespace

int stored_channel(int value) { return static_cast<int>(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value))); }

const LevelRow *lookup_level(const LevelLights &table, std::string_view name, std::string_view sub) {
    const std::string exact = std::string(name) + "_" + std::string(sub);
    for (const auto &row : table.levels)
        if (row.name == exact) return &row;
    if (!name.empty()) {
        for (const auto &row : table.levels)
            if (row.name.size() >= name.size() && row.name.compare(0, name.size(), name) == 0) return &row;
        for (const auto &row : table.levels)
            if (!row.name.empty() && std::string_view(name).find(row.name) != std::string_view::npos) return &row;
    }
    return nullptr;
}

LevelLights load_level_lights(std::string_view text) {
    Reader reader{text};
    const Json root = reader.value();
    reader.space();
    if (reader.p != text.size()) refuse("trailing text");
    if (root.kind != Json::Kind::Object) refuse("the top level is not an object");
    only_keys(root, "top level", {"executable_sha256", "level_table", "row_bytes", "flag_offset", "marker_block",
                                  "runtime_record", "defaults", "levels", "default", "files"});

    LevelLights out;
    const Json &sha = member(root, "executable_sha256", "top level");
    if (sha.kind != Json::Kind::String) refuse("executable_sha256: string expected");
    out.executable_sha256 = sha.text;
    if (out.executable_sha256.size() != 64 ||
        out.executable_sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
        refuse("executable_sha256 is not 64 lowercase hex digits");
    if (json_u32(member(root, "level_table", "top level"), "level_table") != kLevelTable)
        refuse("level_table is not g_levelTable 0x4f7210");
    if (json_u32(member(root, "row_bytes", "top level"), "row_bytes") != kRowBytes)
        refuse("row_bytes is not 0x30");
    if (json_u32(member(root, "flag_offset", "top level"), "flag_offset") != kFlagOffset)
        refuse("flag_offset is not 0x20");
    if (json_u32(member(root, "marker_block", "top level"), "marker_block") != kMarkerBlock)
        refuse("marker_block is not 0x44fde0");
    if (json_u32(member(root, "runtime_record", "top level"), "runtime_record") != kRuntimeRecord)
        refuse("runtime_record is not 0x5d3448");

    const Json &defaults = member(root, "defaults", "top level");
    if (defaults.kind != Json::Kind::Object) refuse("defaults: an object expected");
    only_keys(defaults, "defaults", {"ambient", "sun", "direction", "base_level"});
    const Json &damb = member(defaults, "ambient", "defaults");
    const Json &dsun = member(defaults, "sun", "defaults");
    if (damb.kind != Json::Kind::Array || damb.items.size() != 4) refuse("defaults.ambient: four integers expected");
    if (dsun.kind != Json::Kind::Array || dsun.items.size() != 4) refuse("defaults.sun: four integers expected");
    for (int i = 0; i < 4; ++i) {
        out.default_ambient[i] = json_i32(damb.items[static_cast<std::size_t>(i)], "defaults.ambient");
        out.default_sun[i] = json_i32(dsun.items[static_cast<std::size_t>(i)], "defaults.sun");
    }
    if (out.default_ambient[3] != 0 || out.default_sun[3] != 0) refuse("defaults: alpha is 0");
    out.default_direction = json_vec3(member(defaults, "direction", "defaults"), "defaults.direction");
    const int marker_base = json_i32(member(defaults, "base_level", "defaults"), "defaults.base_level");
    // world_loadRecords writes these before any marker or flag word (section 10).
    if (out.default_ambient != std::array<int, 4>{0x20, 0x20, 0x20, 0} ||
        out.default_sun != std::array<int, 4>{0x44, 0x44, 0x44, 0} || out.default_direction.x != 0.0 ||
        out.default_direction.y != -1.0 || out.default_direction.z != 1.0 || marker_base != 0x80)
        refuse("defaults are not the marker block written by world_loadRecords");

    const Json &levels = member(root, "levels", "top level");
    if (levels.kind != Json::Kind::Array) refuse("levels: an array expected");
    if (levels.items.size() > 127) refuse("levels: more rows than the table walk allows");
    out.levels.reserve(levels.items.size());
    for (std::size_t i = 0; i < levels.items.size(); ++i) {
        const Json &row = levels.items[i];
        const std::string where = "levels[" + std::to_string(i) + "]";
        if (row.kind != Json::Kind::Object) refuse(where + ": an object expected");
        only_keys(row, where, {"index", "name", "flags", "base_level", "ambient", "sun", "direction"});
        LevelRow level;
        level.index = json_i32(member(row, "index", where), where + ".index");
        if (level.index != static_cast<int>(i)) refuse(where + ": index is not its position");
        const Json &name = member(row, "name", where);
        if (name.kind != Json::Kind::String || name.text.empty()) refuse(where + ": a name is required");
        level.name = name.text;
        level.flags = json_u32(member(row, "flags", where), where + ".flags");
        level.base_level = json_i32(member(row, "base_level", where), where + ".base_level");
        require_base(level.flags, level.base_level, where);
        if (const Json *amb = row.get("ambient")) {
            level.ambient = json_i3(*amb, where + ".ambient");
            level.has_ambient = true;
        }
        if (const Json *sun = row.get("sun")) {
            level.sun = json_i3(*sun, where + ".sun");
            level.has_sun = true;
        }
        if (const Json *dir = row.get("direction")) {
            level.direction = json_vec3(*dir, where + ".direction");
            level.has_direction = true;
        }
        out.levels.push_back(std::move(level));
    }

    const Json &def = member(root, "default", "top level");
    if (def.kind != Json::Kind::Object) refuse("default: an object expected");
    only_keys(def, "default", {"index", "flags", "base_level"});
    out.default_index = json_i32(member(def, "index", "default"), "default.index");
    out.default_flags = json_u32(member(def, "flags", "default"), "default.flags");
    out.default_base = json_i32(member(def, "base_level", "default"), "default.base_level");
    require_base(out.default_flags, out.default_base, "default");
    if (out.default_index != static_cast<int>(out.levels.size()))
        refuse("default.index is not the row after the named rows");

    if (const Json *files = root.get("files")) {
        if (files->kind != Json::Kind::Array) refuse("files: an array expected");
        for (std::size_t i = 0; i < files->items.size(); ++i) {
            const Json &file = files->items[i];
            const std::string where = "files[" + std::to_string(i) + "]";
            if (file.kind != Json::Kind::Object) refuse(where + ": an object expected");
            only_keys(file, where, {"source", "ambient", "sun", "direction", "level_index"});
            if (member(file, "source", where).kind != Json::Kind::String) refuse(where + ": source string expected");
            const bool has_a = file.get("ambient") != nullptr;
            const bool has_s = file.get("sun") != nullptr;
            const bool has_d = file.get("direction") != nullptr;
            const std::array<int, 3> amb = has_a ? json_i3(*file.get("ambient"), where + ".ambient") : std::array<int, 3>{};
            const std::array<int, 3> sun = has_s ? json_i3(*file.get("sun"), where + ".sun") : std::array<int, 3>{};
            const Vec3 dir = has_d ? json_vec3(*file.get("direction"), where + ".direction") : Vec3{};
            if (const Json *idx = file.get("level_index")) {
                const int index = json_i32(*idx, where + ".level_index");
                if (index < 0 || index >= static_cast<int>(out.levels.size())) refuse(where + ": level_index out of range");
                const LevelRow &row = out.levels[static_cast<std::size_t>(index)];
                if (has_a != row.has_ambient || (has_a && !same_i3(amb, row.ambient)) || has_s != row.has_sun ||
                    (has_s && !same_i3(sun, row.sun)) || has_d != row.has_direction ||
                    (has_d && !same_vec(dir, row.direction)))
                    refuse(where + ": markers differ from the level row they name");
            }
        }
    }
    return out;
}

ResolvedLevelLight resolve_level_light(const LevelLights &table, std::string_view name, std::string_view sub) {
    const LevelRow *row = lookup_level(table, name, sub);
    ResolvedLevelLight out;
    const int base = row ? row->base_level : table.default_base;
    const std::array<int, 3> amb = row && row->has_ambient ? row->ambient : triple(table.default_ambient);
    const std::array<int, 3> sun = row && row->has_sun ? row->sun : triple(table.default_sun);
    const Vec3 dir = row && row->has_direction ? row->direction : table.default_direction;
    const Rgba ambient{stored_channel(amb[0]), stored_channel(amb[1]), stored_channel(amb[2]), 0};
    const Rgba sun_rgba{stored_channel(sun[0]), stored_channel(sun[1]), stored_channel(sun[2]), 0};
    out.level = Level::make(dir, sun_rgba, ambient, base);
    out.object = Object::reset(out.level);
    out.index = row ? row->index : table.default_index;
    out.default_row = row == nullptr;
    out.marker_ambient = row && row->has_ambient;
    out.marker_sun = row && row->has_sun;
    out.marker_direction = row && row->has_direction;
    return out;
}

} // namespace champions::rta::lighting
