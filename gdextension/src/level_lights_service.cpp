#include "champions/level_lights.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <exception>
#include <optional>
#include <string>

namespace {
using namespace godot;
namespace L = champions::rta::lighting;

PackedInt32Array channels(const L::Rgba &rgba) {
    PackedInt32Array out;
    out.push_back(rgba[0]);
    out.push_back(rgba[1]);
    out.push_back(rgba[2]);
    out.push_back(rgba[3]);
    return out;
}

class ChampionsLevelLights : public RefCounted {
    GDCLASS(ChampionsLevelLights, RefCounted)

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("load_json", "text"), &ChampionsLevelLights::load_json);
        ClassDB::bind_method(D_METHOD("resolve", "name", "sub"), &ChampionsLevelLights::resolve);
    }

public:
    Dictionary load_json(const String &text) {
        try {
            const CharString utf8 = text.utf8();
            table_ = L::load_level_lights(std::string(utf8.get_data(), static_cast<std::size_t>(utf8.length())));
            Dictionary out;
            out["ok"] = true;
            out["rows"] = static_cast<int64_t>(table_->levels.size());
            return out;
        } catch (const std::exception &error) {
            table_.reset();
            Dictionary out;
            out["ok"] = false;
            out["error"] = String::utf8(error.what());
            return out;
        }
    }

    Dictionary resolve(const String &name, const String &sub) const {
        if (!table_) {
            Dictionary out;
            out["ok"] = false;
            out["error"] = "level light JSON: no record is loaded";
            return out;
        }
        const CharString name_utf8 = name.utf8();
        const CharString sub_utf8 = sub.utf8();
        const auto got = L::resolve_level_light(
            *table_, std::string(name_utf8.get_data(), static_cast<std::size_t>(name_utf8.length())),
            std::string(sub_utf8.get_data(), static_cast<std::size_t>(sub_utf8.length())));
        const unsigned flags = got.default_row ? table_->default_flags : table_->levels[static_cast<std::size_t>(got.index)].flags;
        const L::Vec3 direction = got.marker_direction
                                      ? table_->levels[static_cast<std::size_t>(got.index)].direction
                                      : table_->default_direction;
        Dictionary out;
        out["ok"] = true;
        out["index"] = got.index;
        out["base"] = got.level.base;
        out["flags"] = static_cast<int64_t>(flags);
        out["ambient"] = channels(got.level.ambient);
        out["sun"] = channels(got.level.sun);
        out["fill"] = channels(got.object.fill);
        out["direction"] = Vector3(direction.x, direction.y, direction.z);
        out["default_row"] = got.default_row;
        out["marker_ambient"] = got.marker_ambient;
        out["marker_sun"] = got.marker_sun;
        out["marker_direction"] = got.marker_direction;
        return out;
    }

private:
    std::optional<L::LevelLights> table_;
};

} // namespace

void register_level_lights_service() { GDREGISTER_CLASS(ChampionsLevelLights); }
