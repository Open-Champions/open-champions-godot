// Godot adapter for the PS2 memory-card character import (native/src/ps2_save.cpp).
// All parsing, refusals and native state stay in native/. This class owns one
// imported character and reports it as a Dictionary.
#include "champions/ps2_save.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <algorithm>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace {
using namespace godot;
namespace P = champions::simulation::rta::ps2_save;

// Names on the card are single bytes; report them as Latin-1.
String latin1(const std::string &text) {
    std::string utf8;
    for (unsigned char c : text) {
        if (c < 0x80) {
            utf8.push_back(char(c));
        } else {
            utf8.push_back(char(0xC0 | (c >> 6)));
            utf8.push_back(char(0x80 | (c & 0x3F)));
        }
    }
    return String::utf8(utf8.c_str(), int64_t(utf8.size()));
}
Array pairs(const P::Quests &values) {
    Array out;
    for (const auto &[name, value] : values) {
        Array row;
        row.push_back(latin1(name));
        row.push_back(int64_t(value));
        out.push_back(row);
    }
    return out;
}
Dictionary refused(const String &error) {
    Dictionary out;
    out["ok"] = false;
    out["error"] = error;
    return out;
}

class ChampionsPs2SaveImport : public RefCounted {
    GDCLASS(ChampionsPs2SaveImport, RefCounted)

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("import_file", "path", "index"), &ChampionsPs2SaveImport::import_file, DEFVAL(0));
        ClassDB::bind_method(D_METHOD("import_bytes", "data", "index"), &ChampionsPs2SaveImport::import_bytes, DEFVAL(0));
        ClassDB::bind_method(D_METHOD("has_character"), &ChampionsPs2SaveImport::has_character);
        ClassDB::bind_method(D_METHOD("quest_value", "name"), &ChampionsPs2SaveImport::quest_value);
        ClassDB::bind_method(D_METHOD("record_word", "offset"), &ChampionsPs2SaveImport::record_word);
        ClassDB::bind_method(D_METHOD("record_bytes"), &ChampionsPs2SaveImport::record_bytes);
        ClassDB::bind_method(D_METHOD("replace_record", "bytes"), &ChampionsPs2SaveImport::replace_record);
        ClassDB::bind_static_method("ChampionsPs2SaveImport", D_METHOD("synthetic_slot", "name", "gold"),
                                    &ChampionsPs2SaveImport::synthetic_slot);
        ClassDB::bind_static_method("ChampionsPs2SaveImport", D_METHOD("file_size"), &ChampionsPs2SaveImport::file_size);
    }

public:
    static int64_t file_size() { return int64_t(P::FILE_SIZE); }

    Dictionary import_file(const String &path, int64_t index) {
        const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
        if (file.is_null())
            return refused("cannot open the selected file");
        if (file->get_length() != P::FILE_SIZE)
            return refused(String("a slot file is exactly ") + String::num_int64(int64_t(P::FILE_SIZE)) +
                           " bytes; this one is " + String::num_int64(int64_t(file->get_length())));
        return import_bytes(file->get_buffer(int64_t(P::FILE_SIZE)), index);
    }

    Dictionary import_bytes(const PackedByteArray &data, int64_t index) {
        try {
            const P::Bytes bytes(data.ptr(), data.ptr() + data.size());
            auto imported = std::make_unique<P::ImportedCharacter>(P::import_into(bytes, int(index)));
            Dictionary out = describe(*imported);
            imported_ = std::move(imported);
            return out;
        } catch (const std::exception &error) {
            // A refusal keeps the previously imported character.
            return refused(String::utf8(error.what()));
        }
    }

    bool has_character() const { return imported_ != nullptr; }

    int64_t quest_value(const String &name) const {
        if (!imported_)
            return 0;
        const CharString text = name.utf8();
        return imported_->variables->quest_get(std::string(text.get_data(), size_t(text.length())));
    }

    // A word of the native live record, for verification.
    int64_t record_word(int64_t offset) const {
        if (!imported_ || offset < 0 || size_t(offset) + 4 > P::RECORD_SIZE)
            return -1;
        return int64_t(imported_->memory->word(imported_->record_address + uint32_t(offset)));
    }
    PackedByteArray record_bytes() const {
        PackedByteArray out;
        if (!imported_) return out;
        const auto bytes = imported_->memory->read(imported_->record_address, P::RECORD_SIZE);
        out.resize(int64_t(bytes.size()));
        std::memcpy(out.ptrw(), bytes.data(), bytes.size());
        return out;
    }
    Dictionary replace_record(const PackedByteArray &bytes) {
        if (!imported_) return refused("no imported character");
        if (bytes.size() != int64_t(P::RECORD_SIZE)) return refused("player record size changed");
        try {
            imported_->memory->write(imported_->record_address,
                                     std::vector<std::uint8_t>(bytes.ptr(), bytes.ptr() + bytes.size()));
            Dictionary out;
            out["ok"] = true;
            return out;
        } catch (const std::exception &error) {
            return refused(String::utf8(error.what()));
        }
    }

    // An authored synthetic slot image for smokes. Not a save from a card.
    static PackedByteArray synthetic_slot(const String &name, int64_t gold) {
        const CharString text = name.utf8();
        P::Bytes record(P::RECORD_SIZE, 0);
        std::memcpy(record.data(), text.get_data(), std::min<size_t>(size_t(text.length()), 0x1F));
        const auto put = [&](size_t at, uint32_t v) {
            for (int i = 0; i < 4; ++i) record[at + size_t(i)] = uint8_t(v >> (8 * i));
        };
        put(0x50, 1);
        record[0x59] = 4;
        put(0x5C, 5000);
        put(0x78, 46);
        put(0x7C, 37);
        put(0x80, 24);
        put(0x84, 28);
        put(0xE4, uint32_t(gold));
        put(P::BAG_COUNT_AT, 1);
        put(P::BAG_ITEM_AT, P::definition_pointer(3));
        record[P::BAG_ITEM_AT + 4] = 4;
        put(P::OBJECT_POINTER_AT, 0x01100000);
        put(P::EQUIPMENT_AT, P::PLAYERS + uint32_t(P::BAG_ITEM_AT));
        put(0x4B08, 7 | (2u << 16));
        put(0x4B0C, 0xFFFF);
        P::SlotInputs in;
        in.level_name = "crypt";
        in.play_time = 12345;
        in.difficulty = 2;
        in.players = {P::LiveCharacter{record, P::PLAYERS, 0x1234, {{0x3F800000, 0x40000000, 0x40400000}}}};
        in.quests = {{"opened", 7}, {"door", -2}};
        in.acts = {{{"crypt", 0x2000}}};
        const auto image = P::build_slot(in);
        PackedByteArray out;
        out.resize(int64_t(image.size()));
        std::memcpy(out.ptrw(), image.data(), image.size());
        return out;
    }

private:
    static Dictionary describe(const P::ImportedCharacter &imported) {
        const auto &c = imported.character;
        const auto &s = imported.slot;
        Dictionary out;
        out["ok"] = true;
        out["profile"] = P::PROFILE;
        out["index"] = int64_t(imported.index);
        out["name"] = latin1(c.name);
        out["class_index"] = int64_t(c.class_index);
        out["model_index"] = int64_t(c.model_index);
        Array appearance;
        for (auto value : c.appearance) appearance.push_back(int64_t(value));
        out["appearance"] = appearance;
        out["level"] = int64_t(c.level);
        out["acknowledged"] = int64_t(c.acknowledged_level);
        out["experience"] = int64_t(c.experience);
        out["gold"] = int64_t(c.gold);
        // These are the saved record's documented +0xac/+0xb0 and +0xc0/+0xc4
        // raw float words. The character's equipped items can contribute to
        // its stored maxima, so the base stats alone must not replace them.
        const auto raw_word = [&](std::size_t at) -> int64_t {
            return int64_t(std::uint32_t(c.raw[at]) |
                           std::uint32_t(c.raw[at + 1]) << 8 |
                           std::uint32_t(c.raw[at + 2]) << 16 |
                           std::uint32_t(c.raw[at + 3]) << 24);
        };
        out["hp_raw"] = raw_word(0xac);
        out["max_hp_raw"] = raw_word(0xb0);
        out["mana_raw"] = raw_word(0xc0);
        out["max_mana_raw"] = raw_word(0xc4);
        out["imported"] = true;
        Array effective;
        for (std::size_t n = 0; n < 4; ++n)
            effective.push_back(int64_t(static_cast<std::int32_t>(raw_word(0x90 + n * 4))));
        out["effective_stats"] = effective;
        Array stats;
        for (auto v : c.base_stats) stats.push_back(int64_t(v));
        out["base_stats"] = stats;
        out["skill_points"] = int64_t(c.skill_points);
        out["stat_points"] = int64_t(c.stat_points);
        Array bags;
        for (const auto &bag : c.bags) bags.push_back(int64_t(bag.size()));
        out["bag_counts"] = bags;
        Array skills;
        for (const auto &[id, rank] : c.skills) {
            Array row;
            row.push_back(int64_t(id));
            row.push_back(int64_t(rank));
            skills.push_back(row);
        }
        out["skills"] = skills;
        Array names;
        for (const auto &other : s.characters) names.push_back(latin1(other.name));
        out["characters"] = names;
        out["level_name"] = latin1(s.level_name);
        out["subname"] = latin1(s.subname);
        out["act"] = int64_t(s.act);
        out["difficulty"] = int64_t(s.difficulty);
        out["play_time"] = int64_t(s.play_time);
        out["player_count"] = int64_t(s.player_count);
        out["quests"] = pairs(imported.live_quests);
        out["checksum"] = int64_t(imported.checksum);
        out["record_address"] = int64_t(imported.record_address);
        Array refusals;
        for (const auto &r : imported.refusals) refusals.push_back(String::utf8(r.c_str()));
        out["refusals"] = refusals;
        return out;
    }

    std::unique_ptr<P::ImportedCharacter> imported_;
};
} // namespace

void register_ps2_save_import() { GDREGISTER_CLASS(ChampionsPs2SaveImport); }
