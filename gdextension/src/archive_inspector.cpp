#include "session_controller.hpp"
#include "fx_bridge.hpp"
#include "native_audio_bridge.hpp"
#include "champions/archive.hpp"
#include "champions/texture.hpp"
#include "champions/ui_font.hpp"
#include "champions/ui_font_page.hpp"
#include "champions/ui_art_shape.hpp"
#include "champions/texture_upscale.hpp"
#include "champions/inventory_icons.hpp"
#include "champions/inventory_icon_assoc.hpp"
#include "champions/model.hpp"
#include "champions/animation.hpp"
#include "champions/skinning.hpp"
#include "champions/arena_executable.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/hashing_context.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

void register_ui_layout_service();
void register_level_lights_service();
void register_ps2_save_import();
namespace {
using namespace godot;
constexpr std::uint64_t max_file_bytes = 64ULL * 1024 * 1024;
constexpr std::size_t max_rows = 20000;
constexpr std::size_t max_name_bytes = 1024;

std::uint32_t little32(champions::Bytes bytes, std::size_t offset) {
    if (offset > bytes.size || bytes.size - offset < 4) {
        throw champions::FormatError("truncated LMP table");
    }
    return std::uint32_t(bytes.data[offset]) | (std::uint32_t(bytes.data[offset + 1]) << 8) |
           (std::uint32_t(bytes.data[offset + 2]) << 16) | (std::uint32_t(bytes.data[offset + 3]) << 24);
}

// Resource limits precede the core parser's allocations. Format validity still
// comes from the existing parser; these limits intentionally reject huge inputs.
void bound_lmp(champions::Bytes bytes, std::size_t available_rows) {
    const auto count = little32(bytes, 0);
    if (count > available_rows) throw champions::FormatError("archive exceeds the 20,000-entry browser limit");
    if (count > (bytes.size - 4) / 12) throw champions::FormatError("truncated LMP table");
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = little32(bytes, 4 + i * 12);
        if (offset >= bytes.size) throw champions::FormatError("LMP name offset exceeds its payload");
        const auto size = std::min(bytes.size - offset, max_name_bytes + 1);
        const auto *begin = bytes.data + offset;
        if (std::find(begin, begin + size, std::uint8_t(0)) == begin + size) {
            throw champions::FormatError("LMP name is unterminated or exceeds the 1,024-byte browser limit");
        }
    }
}

void bound_gob(champions::Bytes bytes) {
    for (std::size_t offset = 0, count = 0; offset < bytes.size; offset += 40) {
        if (bytes.data[offset] == 0) return;
        if (++count > max_rows) throw champions::FormatError("archive exceeds the 20,000-entry browser limit");
    }
}

void add_row(Array &rows, const char *kind, const std::string &parent,
             const champions::Entry &entry, std::uint64_t base) {
    if (static_cast<std::size_t>(rows.size()) >= max_rows) {
        throw champions::FormatError("archive exceeds the 20,000-entry browser limit");
    }
    Dictionary row;
    row["kind"] = kind;
    row["parent"] = String::utf8(champions::escapeTsv(parent).c_str());
    row["name"] = String::utf8(champions::escapeTsv(entry.name).c_str());
    row["offset"] = static_cast<int64_t>(base + entry.offset);
    row["size"] = static_cast<int64_t>(entry.size);
    rows.push_back(row);
}

struct ArchiveInput {
    PackedByteArray data;
    bool gob;
};

ArchiveInput read_archive(const String &path, bool allow_texture = false) {
    if (path.is_empty()) throw champions::FormatError("Choose a .GOB or .LMP file first.");
    const auto encoded = path.utf8();
    const std::string filename(encoded.get_data(), static_cast<std::size_t>(encoded.length()));
    const bool gob = champions::hasExtension(filename, ".gob");
    if (!gob && !champions::hasExtension(filename, ".lmp") &&
        !(allow_texture && (champions::hasExtension(filename, ".tex") || champions::hasExtension(filename, ".etex") || champions::hasExtension(filename, ".fnt")))) {
        throw champions::FormatError("Unsupported file type. Choose a .GOB or .LMP archive.");
    }
    const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) throw champions::FormatError("Cannot open this file. Check its location and read permission.");
    const auto size = file->get_length();
    if (size > max_file_bytes) throw champions::FormatError("This archive exceeds the browser's 64 MiB file limit. Use the native inspector for larger archives.");
    PackedByteArray data = file->get_buffer(size);
    if (static_cast<std::uint64_t>(data.size()) != size) throw champions::FormatError("Could not read the complete archive.");
    return {data, gob};
}

String fingerprint(const PackedByteArray &data) {
    Ref<HashingContext> hash;
    hash.instantiate();
    if (hash->start(HashingContext::HASH_SHA256) != OK || hash->update(data) != OK) {
        throw champions::FormatError("Could not identify the archive. Reopen it to try again.");
    }
    return hash->finish().hex_encode();
}

// Listing and preview use exactly the same flattened row order. Every container
// is validated even when the selected row was found before a later malformed row.
template <typename Visitor>
void walk_archive(champions::Bytes bytes, bool gob, Visitor visitor) {
    std::size_t row_index = 0;
    auto visit = [&](const char *kind, const std::string &parent,
                     const champions::Entry &entry, std::uint64_t base,
                     champions::Bytes container) {
        if (row_index >= max_rows) throw champions::FormatError("archive exceeds the 20,000-entry browser limit");
        visitor(row_index++, kind, parent, entry, base, container);
    };
    if (gob) {
        bound_gob(bytes);
        for (const auto &entry : champions::readGob(bytes)) {
            visit("GOB", "", entry, 0, bytes);
            if (champions::hasExtension(entry.name, ".lmp")) {
                const auto nested = champions::payload(bytes, entry);
                try {
                    bound_lmp(nested, max_rows - row_index);
                    for (const auto &child : champions::readLmp(nested)) {
                        visit("LMP", entry.name, child, entry.offset, nested);
                    }
                } catch (const champions::FormatError &error) {
                    throw champions::FormatError("Nested LMP '" + champions::escapeTsv(entry.name) + "': " + error.what());
                }
            }
        }
    } else {
        bound_lmp(bytes, max_rows);
        for (const auto &entry : champions::readLmp(bytes)) visit("LMP", "", entry, 0, bytes);
    }
}

struct SelectedInput {
    ArchiveInput archive;
    champions::Bytes bytes;
    std::string name;
    champions::Bytes container;
    bool in_lmp;
};

// Resolve one or more explicit row identities from one immutable file snapshot.
// Each selection retains the same PackedByteArray owner. Shared references do not
// reread/copy the file, and the complete walk still validates later containers.
template <std::size_t Count>
std::array<SelectedInput,Count> read_selected_entries(const String &path,
        const std::array<int64_t,Count> &row_indices, const String &expected_fingerprint) {
    for(auto row:row_indices) {
        if(row<0 || row>=static_cast<int64_t>(max_rows)) throw champions::FormatError("Choose a valid archive entry.");
    }
    auto input=read_archive(path, true);
    if(expected_fingerprint.is_empty() || fingerprint(input.data)!=expected_fingerprint) {
        throw champions::FormatError("The archive changed since it was opened. Reopen it before previewing an entry.");
    }
    const champions::Bytes bytes{input.data.ptr(),static_cast<std::size_t>(input.data.size())};
    const auto extension = path.get_extension().to_lower();
    if (extension == "tex" || extension == "etex" || extension == "fnt") {
        std::array<SelectedInput,Count> loose{};
        for (std::size_t i=0; i<Count; ++i) {
            if (row_indices[i] != 0) throw champions::FormatError("Loose inputs have only row zero.");
            loose[i] = {input, bytes, path.get_file().utf8().get_data(), bytes, false};
        }
        return loose;
    }
    std::array<SelectedInput,Count> selected{};
    std::array<bool,Count> found{};
    for(auto &entry:selected) entry.archive=input;
    walk_archive(bytes,input.gob,[&](std::size_t index,const char *kind,const std::string &,
                 const champions::Entry &entry,std::uint64_t,champions::Bytes container) {
        for(std::size_t i=0;i<Count;++i)if(index==static_cast<std::size_t>(row_indices[i])) {
            selected[i].bytes=champions::payload(container,entry);
            selected[i].name=entry.name;
            selected[i].container=container;
            selected[i].in_lmp=std::string(kind)=="LMP";
            found[i]=true;
        }
    });
    for(bool exists:found)if(!exists) throw champions::FormatError("The selected archive entry no longer exists. Reopen the archive.");
    return selected;
}

SelectedInput read_selected(const String &path,int64_t row_index,const String &expected_fingerprint) {
    auto selected=read_selected_entries(path,std::array<int64_t,1>{{row_index}},expected_fingerprint);
    return std::move(selected[0]);
}

// Archive names are byte strings, not filesystem paths. Fold ASCII only; keep
// every path/stem byte, and reject ambiguity even if one spelling matches exactly.
bool same_companion_name(const std::string &left, const std::string &right) {
    if (left.size() != right.size()) return false;
    auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (fold(static_cast<unsigned char>(left[i])) != fold(static_cast<unsigned char>(right[i]))) return false;
    }
    return true;
}

Dictionary texture_status(const char *status, const String &error, const std::string &name = {}) {
    Dictionary result;
    result["ok"] = false;
    result["status"] = status;
    result["error"] = error;
    result["width"] = int64_t(0);
    result["height"] = int64_t(0);
    result["rgba"] = PackedByteArray();
    result["name"] = String::utf8(champions::escapeTsv(name).c_str());
    result["flags"] = int64_t(0);
    return result;
}

Dictionary model_texture(const SelectedInput &selected) {
    std::string candidate_name;
    try {
        if (!selected.in_lmp) {
            return texture_status("missing", "A model texture must be a companion entry in the same LMP.");
        }
        // read_selected retained this exact LMP within the fingerprinted owner and
        // validated the entire archive. Never resolve by the display parent name:
        // different GOB entries can have identical names or basename collisions.
        const auto expected = selected.name.substr(0, selected.name.size() - 4) + ".tex";
        champions::Bytes candidate{nullptr, 0};
        std::size_t matches = 0;
        for (const auto &entry : champions::readLmp(selected.container)) {
            if (same_companion_name(entry.name, expected)) {
                ++matches;
                candidate = champions::payload(selected.container, entry);
                candidate_name = entry.name;
            }
        }
        if (matches == 0) {
            return texture_status("missing", "No same-name .tex companion was found in this LMP.");
        }
        if (matches != 1) {
            return texture_status("ambiguous", "More than one .tex companion matches this model in the same LMP. No texture was selected.");
        }
        const auto texture = champions::decodeRtaTexture(candidate);
        if (texture.width == 0 || texture.height == 0 || texture.width > 1024 || texture.height > 1024 ||
            texture.rgba.size() != static_cast<std::size_t>(texture.width) * texture.height * 4) {
            throw champions::FormatError("The companion texture exceeds the supported preview dimensions.");
        }
        PackedByteArray rgba;
        if (rgba.resize(static_cast<int64_t>(texture.rgba.size())) != OK) throw std::bad_alloc();
        std::copy(texture.rgba.begin(), texture.rgba.end(), rgba.ptrw());
        // Publish owned pixels atomically. Stored alpha is metadata; the model
        // viewer intentionally displays opaque RGB, not an inferred game blend.
        auto result = texture_status("ready", String(), candidate_name);
        result["width"] = static_cast<int64_t>(texture.width);
        result["height"] = static_cast<int64_t>(texture.height);
        result["flags"] = static_cast<int64_t>(texture.flags);
        result["rgba"] = rgba;
        result["ok"] = true;
        return result;
    } catch (const std::bad_alloc &) {
        return texture_status("unavailable", "Not enough memory to preview the companion texture.", candidate_name);
    } catch (const std::exception &error) {
        return texture_status("unavailable", String::utf8(error.what()), candidate_name);
    } catch (...) {
        return texture_status("unavailable", "The companion texture could not be inspected.", candidate_name);
    }
}

class ChampionsArchiveInspector : public RefCounted {
    GDCLASS(ChampionsArchiveInspector, RefCounted)
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("inspect_archive", "path"), &ChampionsArchiveInspector::inspect_archive);
        ClassDB::bind_method(D_METHOD("inspect_ui_font", "path", "row_index", "profile", "archive_fingerprint"), &ChampionsArchiveInspector::inspect_ui_font);
        ClassDB::bind_method(D_METHOD("read_fnt_entry", "path", "row_index", "profile", "archive_fingerprint"), &ChampionsArchiveInspector::read_fnt_entry);
        ClassDB::bind_method(D_METHOD("shape_ui_art", "width", "height", "rgba", "field_rows", "crop"), &ChampionsArchiveInspector::shape_ui_art);
        ClassDB::bind_method(D_METHOD("inspect_ui_candidates", "path", "profile"), &ChampionsArchiveInspector::inspect_ui_candidates);
        ClassDB::bind_method(D_METHOD("inspect_texture", "path", "row_index", "profile", "archive_fingerprint"),
                            &ChampionsArchiveInspector::inspect_texture);
        ClassDB::bind_method(D_METHOD("decode_inventory_icon", "sector_payload", "profile", "display_alpha"),
                            &ChampionsArchiveInspector::decode_inventory_icon, DEFVAL(false));
        ClassDB::bind_method(D_METHOD("lookup_inventory_icon", "index_bytes", "picture_name", "profile"),
                            &ChampionsArchiveInspector::lookup_inventory_icon);
        ClassDB::bind_method(D_METHOD("inventory_icon_grid_placement", "width", "height", "panel_x", "column", "row_y", "profile"),
                            &ChampionsArchiveInspector::inventory_icon_grid_placement);
        ClassDB::bind_method(D_METHOD("texture_fidelity", "width", "height", "source", "output_width", "output_height", "output", "factor"),
                            &ChampionsArchiveInspector::texture_fidelity);
        ClassDB::bind_method(D_METHOD("inspect_texture_upscaled", "path", "row_index", "profile", "archive_fingerprint", "filter", "factor", "opaque_rgb"),
                            &ChampionsArchiveInspector::inspect_texture_upscaled, DEFVAL(false));
        ClassDB::bind_method(D_METHOD("inspect_model", "path", "row_index", "profile", "archive_fingerprint"),
                            &ChampionsArchiveInspector::inspect_model);
        ClassDB::bind_method(D_METHOD("sample_animation", "path", "row_index", "profile", "archive_fingerprint", "tick"),
                            &ChampionsArchiveInspector::sample_animation);
        ClassDB::bind_method(D_METHOD("sample_skinned_model", "path", "model_row_index", "animation_row_index", "profile", "archive_fingerprint", "tick"),
                            &ChampionsArchiveInspector::sample_skinned_model);
        ClassDB::bind_method(D_METHOD("sample_skinned_resources", "model_bytes", "animation_bytes", "tick"),
                            &ChampionsArchiveInspector::sample_skinned_resources);
        ClassDB::bind_method(D_METHOD("read_player_resource", "path", "row_index", "archive_fingerprint", "expected_name"),
                            &ChampionsArchiveInspector::read_player_resource);
        ClassDB::bind_method(D_METHOD("player_model_resources", "executable_bytes", "model_index"),
                            &ChampionsArchiveInspector::player_model_resources);
    }
public:
    Dictionary player_model_resources(const PackedByteArray &executable_bytes,
            int64_t model_index) const {
        Dictionary failure;
        failure["ok"] = false;
        failure["error"] = String();
        try {
            // Player_ctor's 12 model definitions, research-player-model
            // 7f13899 docs/spec/player-model-resources.md. The table strings
            // themselves remain in the owner's executable at run time.
            constexpr std::array<std::uint32_t, 12> definitions{{
                0x4ecc58, 0x4ec8d0, 0x4ee188, 0x4ede00,
                0x4eda78, 0x4ed6f0, 0x4ee898, 0x4ee510,
                0x4ecfe0, 0x4ed368, 0x4eec20, 0x4eefa8}};
            constexpr std::array<const char *, 12> body_codes{{
                "bm", "bf", "wem", "wef", "hem", "hef",
                "em", "ef", "dem", "def", "vm", "im"}};
            constexpr std::array<int, 12> classes{{0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6}};
            if (model_index < 0 || model_index >= static_cast<int64_t>(definitions.size()))
                throw champions::FormatError("Player model index is outside the traced 0..11 table.");
            if (executable_bytes.is_empty() || executable_bytes.size() > 16 * 1024 * 1024)
                throw champions::FormatError("Owner executable is missing or exceeds the 16 MiB limit.");
            champions::simulation::rta::arena_modes::Executable exe;
            if (!exe.load(std::vector<std::uint8_t>(executable_bytes.ptr(),
                    executable_bytes.ptr() + executable_bytes.size())))
                throw champions::FormatError("Owner executable could not be mapped as RTA ELF32.");
            const auto index = static_cast<std::size_t>(model_index);
            const auto table = definitions[index];
            const auto animation_lump = exe.string(exe.word(table));
            if (!champions::hasExtension(animation_lump, ".lmp"))
                throw champions::FormatError("Player animation definition does not name an LMP.");
            Array slots;
            for (std::uint32_t slot = 1; slot <= 111; ++slot) {
                const auto name = exe.string(exe.word(table + slot * 8));
                if (!champions::hasExtension(name, ".anm") &&
                    !champions::hasExtension(name, ".vag"))
                    throw champions::FormatError("Player animation slot names an unsupported resource.");
                slots.push_back(String::utf8(name.c_str()));
            }
            if (exe.word(table + 112 * 8) != 0)
                throw champions::FormatError("Player animation definition has no traced terminator.");
            Dictionary result;
            result["ok"] = true;
            result["body_code"] = String(body_codes[index]);
            result["class_index"] = classes[index];
            result["animation_lump"] = String::utf8(animation_lump.c_str());
            result["clip_slots_1_to_111"] = slots;
            return result;
        } catch (const std::exception &error) {
            failure["error"] = String::utf8(error.what());
        }
        return failure;
    }

    // Copy a selected player resource from the same fingerprinted snapshot used
    // to validate its row. The caller never reopens saved offsets after listing.
    Dictionary read_player_resource(const String &path, int64_t row_index,
            const String &archive_fingerprint, const String &expected_name) const {
        Dictionary result;
        result["ok"] = false;
        result["bytes"] = PackedByteArray();
        result["error"] = String();
        try {
            const auto selected = read_selected(path, row_index, archive_fingerprint);
            const std::string expected = expected_name.utf8().get_data();
            if (selected.name != expected ||
                (!champions::hasExtension(expected, ".vif") &&
                 !champions::hasExtension(expected, ".anm") &&
                 !champions::hasExtension(expected, ".tex")))
                throw champions::FormatError("The selected player resource name changed or is unsupported.");
            if (selected.bytes.size == 0 || selected.bytes.size > 16 * 1024 * 1024)
                throw champions::FormatError("The selected player resource size is invalid.");
            PackedByteArray bytes;
            if (bytes.resize(static_cast<int64_t>(selected.bytes.size)) != OK)
                throw std::bad_alloc();
            std::copy(selected.bytes.data, selected.bytes.data + selected.bytes.size, bytes.ptrw());
            result["bytes"] = bytes;
            result["ok"] = true;
        } catch (const std::bad_alloc &) {
            result["error"] = "Not enough memory to read the selected player resource.";
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        }
        return result;
    }

    // The player constructor fetches its body from SELECT and its clip from a
    // class-specific LMP. Keep both source archives outside the project and
    // pass only bounded, read-only entry bytes to the existing native decoder.
    Dictionary sample_skinned_resources(const PackedByteArray &model_bytes,
            const PackedByteArray &animation_bytes, double tick) const {
        Dictionary failure;
        failure["ok"] = false;
        failure["error"] = String();
        try {
            if (model_bytes.is_empty() || animation_bytes.is_empty() ||
                model_bytes.size() > 16 * 1024 * 1024 || animation_bytes.size() > 16 * 1024 * 1024)
                throw champions::FormatError("Player model or clip entry is empty or exceeds the 16 MiB limit.");
            if (!std::isfinite(tick) || tick < 0)
                throw champions::FormatError("Choose a finite animation tick at or above zero.");
            const champions::Bytes model_input{model_bytes.ptr(), static_cast<std::size_t>(model_bytes.size())};
            const champions::Bytes animation_input{animation_bytes.ptr(), static_cast<std::size_t>(animation_bytes.size())};
            const auto model = champions::decodeRtaModel(model_input);
            if (model.attachment_slot != -1)
                throw champions::FormatError("The body model unexpectedly requires a parent attachment.");
            const auto animation = champions::decodeRtaAnimationKeys(animation_input);
            if (animation.skeleton.joint_mask != 0)
                throw champions::FormatError("The selected player clip is partial-body.");
            const auto pose = champions::sampleRtaAnimationPose(animation, tick);
            const auto transforms = champions::buildRtaAnimationTransforms(animation.skeleton, pose);
            const auto skinned = champions::skinRtaModelPositions(model, transforms);
            const auto skinned_normals = champions::skinRtaModelNormals(model, transforms);
            Array meshes;
            std::size_t corners = 0;
            for (std::size_t i = 0; i < skinned.meshes.size(); ++i) {
                const auto &mesh = skinned.meshes[i];
                const auto &normal_mesh = skinned_normals.meshes[i];
                PackedVector3Array positions, normals;
                if (positions.resize(static_cast<int64_t>(mesh.positions.size())) != OK ||
                    normals.resize(static_cast<int64_t>(normal_mesh.normals.size())) != OK) throw std::bad_alloc();
                for (std::size_t corner = 0; corner < mesh.positions.size(); ++corner) {
                    const auto &point = mesh.positions[corner];
                    const auto &normal = normal_mesh.normals[corner];
                    const Vector3 p(point[0], point[1], point[2]);
                    const Vector3 n(normal[0], normal[1], normal[2]);
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                        !std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z))
                        throw champions::FormatError("Player pose contains nonfinite positions or normals.");
                    positions.set(static_cast<int64_t>(corner), p);
                    normals.set(static_cast<int64_t>(corner), n);
                }
                Dictionary item;
                item["index"] = static_cast<int64_t>(mesh.index);
                item["visible_by_default"] = model.meshes[i].visible_by_default;
                item["positions"] = positions;
                item["normals"] = normals;
                meshes.push_back(item);
                corners += mesh.positions.size();
            }
            Dictionary result;
            result["ok"] = true;
            result["pairing_verified"] = false;
            result["duration_ticks"] = static_cast<int64_t>(animation.skeleton.duration_ticks);
            result["tick"] = pose.tick;
            result["meshes"] = meshes;
            result["triangle_count"] = static_cast<int64_t>(corners / 3);
            return result;
        } catch (const std::bad_alloc &) {
            failure["error"] = "Not enough memory to sample the player model.";
        } catch (const std::exception &error) {
            failure["error"] = String::utf8(error.what());
        }
        return failure;
    }
    // The scene supplies a picture filename and HDR bytes; caller controls
    // the per-frame streaming cache and slices DAT using the returned sector
    // range before passing that payload to decode_inventory_icon.
    Dictionary lookup_inventory_icon(const PackedByteArray &index_bytes, const String &picture_name,
                                     const String &profile) const {
        Dictionary result;
        result["ok"] = false;
        try {
            if (profile != "rta") throw champions::FormatError("Inventory icons are supported only for Return to Arms.");
            const auto index = champions::inventory_icons::parse_index(
                index_bytes.ptr(), static_cast<std::size_t>(index_bytes.size()));
            const auto encoded = picture_name.utf8();
            const auto *entry = champions::inventory_icons::find_entry(
                index, std::string(encoded.get_data(), encoded.length()));
            result["found"] = entry != nullptr;
            if (entry) {
                result["number"] = static_cast<int64_t>(entry->number);
                result["first_sector"] = static_cast<int64_t>(entry->first_sector);
                result["sectors"] = static_cast<int64_t>(entry->sectors);
                result["width"] = static_cast<int64_t>(entry->width);
                result["height"] = static_cast<int64_t>(entry->height);
            }
            result["ok"] = true;
        } catch (const std::bad_alloc &) {
            result["error"] = "Not enough memory to read the inventory icon index.";
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "The inventory icon index could not be read.";
        }
        return result;
    }

    Dictionary inventory_icon_grid_placement(int32_t width, int32_t height, int32_t panel_x,
                                             int32_t column, float row_y, const String &profile) const {
        Dictionary result;
        result["ok"] = false;
        try {
            if (profile != "rta") throw champions::FormatError("Inventory icons are supported only for Return to Arms.");
            const auto p = champions::inventory_icons::grid_placement(width,height,panel_x,column,row_y);
            result["columns"] = p.columns; result["rows"] = p.rows;
            result["footprint_width"] = p.footprint_width;
            result["footprint_height"] = p.footprint_height;
            result["x"] = p.x; result["y"] = p.y;
            result["ok"] = true;
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "The inventory icon could not be placed.";
        }
        return result;
    }

    // Caller supplies an explicitly selected PICCACHE.DAT sector payload. The
    // item-to-entry lookup and HDR index are separate contracts.
    Dictionary decode_inventory_icon(const PackedByteArray &sector_payload, const String &profile,
                                     bool display_alpha = false) const {
        Dictionary result;
        result["ok"] = false;
        try {
            if (profile != "rta") throw champions::FormatError("Inventory icons are supported only for Return to Arms.");
            const auto picture = champions::inventory_icons::decode(
                sector_payload.ptr(), static_cast<std::size_t>(sector_payload.size()));
            const auto pixels = display_alpha ? champions::inventory_icons::display_rgba(picture.rgba) : picture.rgba;
            PackedByteArray rgba;
            if (rgba.resize(static_cast<int64_t>(pixels.size())) != OK) throw std::bad_alloc();
            std::copy(pixels.begin(), pixels.end(), rgba.ptrw());
            result["width"] = static_cast<int64_t>(picture.width);
            result["height"] = static_cast<int64_t>(picture.height);
            result["flags"] = static_cast<int64_t>(picture.flags);
            result["rgba"] = rgba;
            result["ok"] = true;
        } catch (const std::bad_alloc &) {
            result["error"] = "Not enough memory to decode this inventory icon.";
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "The inventory icon could not be decoded.";
        }
        return result;
    }

    // Metadata only: flag 0x0040 is a candidate signal, never a frame identity.
    // Refused decodes stay discoverable; pixels are decoded only on selection.
    Dictionary inspect_ui_candidates(const String &path, const String &profile) const {
        Dictionary result;
        result["ok"] = false;
        result["rows"] = Array();
        try {
            if (profile != "rta") throw champions::FormatError("UI candidates are supported only for Return to Arms.");
            const auto input = read_archive(path, true);
            const champions::Bytes bytes{input.data.ptr(), static_cast<std::size_t>(input.data.size())};
            Array rows;
            auto add = [&](std::size_t index, const std::string &name, const std::string &parent, champions::Bytes payload) {
                if (champions::hasExtension(name, ".fnt")) {
                    Dictionary row;
                    row["row_index"] = static_cast<int64_t>(index);
                    row["name"] = String::utf8(champions::escapeTsv(name).c_str());
                    row["parent"] = String::utf8(champions::escapeTsv(parent).c_str());
                    row["kind"] = "font";
                    row["width"] = int64_t(0); row["height"] = int64_t(0);
                    row["flags"] = int64_t(0); row["psm"] = int64_t(0);
                    rows.push_back(row);
                    return;
                }
                if ((!champions::hasExtension(name, ".tex") && !champions::hasExtension(name, ".etex")) || payload.size < 0x80) return;
                const auto header = champions::readRtaTextureHeader(payload);
                if (!header.isUiCandidate()) return;
                Dictionary row;
                row["row_index"] = static_cast<int64_t>(index);
                row["name"] = String::utf8(champions::escapeTsv(name).c_str());
                row["parent"] = String::utf8(champions::escapeTsv(parent).c_str());
                row["width"] = static_cast<int64_t>(header.width);
                row["height"] = static_cast<int64_t>(header.height);
                row["flags"] = static_cast<int64_t>(header.flags);
                row["psm"] = static_cast<int64_t>(header.psm);
                rows.push_back(row);
            };
            const auto extension = path.get_extension().to_lower();
            if (extension == "tex" || extension == "etex" || extension == "fnt") add(0, path.get_file().utf8().get_data(), "", bytes);
            else walk_archive(bytes, input.gob, [&](std::size_t index, const char *, const std::string &parent,
                        const champions::Entry &entry, std::uint64_t, champions::Bytes container) {
                add(index, entry.name, parent, champions::payload(container, entry));
            });
            result["fingerprint"] = fingerprint(input.data);
            result["rows"] = rows;
            result["ok"] = true;
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "UI candidates could not be inspected.";
        }
        return result;
    }

    Dictionary inspect_archive(const String &path) const {
        Dictionary result;
        result["ok"] = false;
        result["error"] = String();
        result["rows"] = Array();
        result["file_size"] = int64_t(0);
        result["fingerprint"] = String();
        try {
            const auto input = read_archive(path);
            const champions::Bytes bytes{input.data.ptr(), static_cast<std::size_t>(input.data.size())};
            Array rows;
            walk_archive(bytes, input.gob, [&](std::size_t, const char *kind, const std::string &parent,
                         const champions::Entry &entry, std::uint64_t base, champions::Bytes) {
                add_row(rows, kind, parent, entry, base);
            });
            // Publish rows atomically after all nested input has passed validation.
            result["fingerprint"] = fingerprint(input.data);
            result["rows"] = rows;
            result["file_size"] = input.data.size();
            result["ok"] = true;
        } catch (const std::bad_alloc &) {
            result["error"] = "Not enough memory to inspect this archive.";
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "The archive could not be inspected.";
        }
        return result;
    }

    Dictionary inspect_texture(const String &path, int64_t row_index, const String &profile,
                               const String &archive_fingerprint) const {
        Dictionary result;
        result["ok"] = false;
        result["error"] = String();
        result["rgba"] = PackedByteArray();
        result["width"] = int64_t(0);
        result["height"] = int64_t(0);
        try {
            if (profile != "rta") throw champions::FormatError("Texture preview is supported only for Return to Arms. Norrath texture compatibility is unverified.");
            const auto selected = read_selected(path, row_index, archive_fingerprint);
            if (!champions::hasExtension(selected.name, ".tex") && !champions::hasExtension(selected.name, ".etex")) {
                throw champions::FormatError("Select a .tex or .etex entry. Preview supports bounded 32-bit, plain 4-bit and 8-bit lump textures.");
            }
            const auto texture = champions::decodeRtaTexture(selected.bytes);
            if (texture.width == 0 || texture.height == 0 || texture.width > 1024 || texture.height > 1024 ||
                texture.rgba.size() != static_cast<std::size_t>(texture.width) * texture.height * 4) {
                throw champions::FormatError("The texture exceeds the supported preview dimensions.");
            }
            PackedByteArray rgba;
            if (rgba.resize(static_cast<int64_t>(texture.rgba.size())) != OK) throw std::bad_alloc();
            std::copy(texture.rgba.begin(), texture.rgba.end(), rgba.ptrw());
            result["width"] = static_cast<int64_t>(texture.width);
            result["height"] = static_cast<int64_t>(texture.height);
            result["flags"] = static_cast<int64_t>(texture.flags);
            result["name"] = String::utf8(champions::escapeTsv(selected.name).c_str());
            result["rgba"] = rgba;
            result["ok"] = true;
        } catch (const std::bad_alloc &) {
            result["error"] = "Not enough memory to preview this texture.";
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "The texture could not be previewed.";
        }
        return result;
    }

    Dictionary read_fnt_entry(const String &path, int64_t row, const String &profile, const String &hash) const {
        Dictionary result;
        result["ok"] = false;
        try {
            if (profile != "rta") throw champions::FormatError("Font data is RTA-only.");
            const auto selected = read_selected(path, row, hash);
            if (!champions::hasExtension(selected.name, ".fnt"))
                throw champions::FormatError("Select an explicit FNT entry.");
            PackedByteArray bytes;
            if (bytes.resize(static_cast<int64_t>(selected.bytes.size)) != OK) throw std::bad_alloc();
            if (selected.bytes.size)
                std::copy(selected.bytes.data, selected.bytes.data + selected.bytes.size, bytes.ptrw());
            result["bytes"] = bytes;
            result["ok"] = true;
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "Font entry could not be read.";
        }
        return result;
    }

    Dictionary inspect_ui_font(const String &path, int64_t row, const String &profile, const String &hash) const {
        Dictionary result;
        result["ok"] = false;
        try {
            if (profile != "rta") throw champions::FormatError("Font data is RTA-only.");
            const auto selected = read_selected(path,row,hash);
            if (!champions::hasExtension(selected.name,".fnt")) throw champions::FormatError("Select an explicit FNT entry.");
            const auto font = champions::readRtaUiFont(selected.bytes);
            Array glyphs, kerns;
            for (const auto &g:font.glyphs) { Array values; for (auto v:g) values.push_back(int64_t(v)); glyphs.push_back(values); }
            for (const auto &k:font.kerns) { Array values; for (auto v:k) values.push_back(int64_t(v)); kerns.push_back(values); }
            result["glyphs"] = glyphs; result["kerns"] = kerns;
            result["glyph_offset"] = int64_t(font.glyph_offset);
            result["kerning_offset"] = int64_t(font.kerning_offset);
            result["texture_offset"] = int64_t(font.texture_offset);
            auto page = texture_status("unavailable", "Embedded texture unsupported.");
            auto publish = [&](const champions::Texture32 &texture) {
                PackedByteArray pixels;
                if (pixels.resize(static_cast<int64_t>(texture.rgba.size())) != OK) throw std::bad_alloc();
                std::copy(texture.rgba.begin(),texture.rgba.end(),pixels.ptrw());
                page["width"] = int64_t(texture.width); page["height"] = int64_t(texture.height);
                page["flags"] = int64_t(texture.flags); page["rgba"] = pixels; page["ok"] = true;
                page["status"] = "ready"; page["error"] = String();
            };
            try {
                publish(champions::decodeFontPage({font.texture.data(),font.texture.size()}).image);
            } catch (const std::exception &font_error) {
                try {
                    publish(champions::decodeRtaTexture({font.texture.data(),font.texture.size()}));
                } catch (const std::bad_alloc &) {
                    throw;
                } catch (const std::exception &) {
                    page["error"] = String::utf8(font_error.what());
                }
            }
            result["page"] = page;
            result["ok"] = true; // Metadata success does not depend on texture support.
        } catch (const std::exception &error) { result["error"] = String::utf8(error.what()); }
        catch (...) { result["error"] = "Font inspection failed."; }
        return result;
    }

    Dictionary shape_ui_art(int64_t width, int64_t height, const PackedByteArray &rgba, int64_t rows, const Array &crop) const {
        Dictionary result; result["ok"] = false;
        try {
            if (width<1 || width>1024 || height<1 || height>1024 || rows<0 || rows>height)
                throw champions::FormatError("Explicit shape dimensions/rows outside supported domain.");
            if (rgba.size()!=width*height*4) throw champions::FormatError("Shape requires exact RGBA8 bytes.");
            std::optional<champions::UiArtCrop> region;
            if (!crop.is_empty()) {
                if (crop.size()!=4) throw champions::FormatError("Crop needs four integers.");
                std::array<uint32_t,4> values{};
                for (int i=0;i<4;++i) {
                    if (crop[i].get_type()!=Variant::INT) throw champions::FormatError("Crop needs integer coordinates.");
                    const int64_t v=crop[i];
                    if (v<0 || v>2048) throw champions::FormatError("Crop value outside supported domain.");
                    values[i]=static_cast<uint32_t>(v);
                }
                region=champions::UiArtCrop{values[0],values[1],values[2],values[3]};
            }
            champions::Texture32 page{static_cast<uint32_t>(width),static_cast<uint32_t>(height),0,{}};
            page.rgba.assign(rgba.ptr(),rgba.ptr()+rgba.size());
            const auto shaped=champions::shapeRtaUiArt(page,static_cast<uint32_t>(rows),region);
            PackedByteArray pixels;
            if (pixels.resize(static_cast<int64_t>(shaped.rgba.size()))!=OK) throw std::bad_alloc();
            std::copy(shaped.rgba.begin(),shaped.rgba.end(),pixels.ptrw());
            result["width"]=int64_t(shaped.width); result["height"]=int64_t(shaped.height);
            result["rgba"]=pixels; result["ok"]=true;
        } catch (const std::exception &error) { result["error"]=String::utf8(error.what()); }
        catch (...) { result["error"]="Shape preview failed."; }
        return result;
    }

    Dictionary texture_fidelity(int64_t width, int64_t height, const PackedByteArray &source,
                                int64_t output_width, int64_t output_height, const PackedByteArray &output, int64_t factor) const {
        Dictionary result;
        result["accepted"] = false;
        try {
            if(width<1 || width>1024 || height<1 || height>1024 || output_width<1 || output_width>4096 ||
               output_height<1 || output_height>4096 || factor<2 || factor>4 ||
               source.size()!=width*height*4 || output.size()!=output_width*output_height*4)
                throw champions::FormatError("Invalid bounded fidelity input.");
            champions::Texture32 a{uint32_t(width),uint32_t(height),0,std::vector<uint8_t>(source.ptr(),source.ptr()+source.size())};
            champions::Texture32 b{uint32_t(output_width),uint32_t(output_height),0,std::vector<uint8_t>(output.ptr(),output.ptr()+output.size())};
            const auto m=champions::measureTextureFidelity(a,b,unsigned(factor));
            result["accepted"]=m.accepted();
            result["aspect_preserved"]=m.aspect_preserved;
            result["error_numerator"]=int64_t(m.error_numerator);
            result["error_denominator"]=int64_t(m.error_denominator);
            result["max_error_numerator"]=int64_t(m.max_error_numerator);
            result["new_palette_pixels"]=int64_t(m.new_palette_pixels);
            result["flat_region_changes"]=int64_t(m.flat_region_changes);
            result["hard_alpha_changes"]=int64_t(m.hard_alpha_changes);
        } catch(const std::exception &error) { result["error"]=String::utf8(error.what()); }
        return result;
    }

    Dictionary inspect_texture_upscaled(const String &path, int64_t row_index, const String &profile,
                                        const String &archive_fingerprint, const String &filter, int64_t factor, bool opaque_rgb) const {
        auto result = inspect_texture(path, row_index, profile, archive_fingerprint);
        if (!bool(result["ok"])) return result;
        try {
            if (factor < 2 || factor > 4) throw champions::FormatError("Upscaling factor must be 2, 3 or 4.");
            champions::TextureFilter mode;
            if (filter == "nearest") mode = champions::TextureFilter::Nearest;
            else if (filter == "pixel-art") mode = champions::TextureFilter::PixelArt;
            else if (filter == "bicubic") mode = champions::TextureFilter::Bicubic;
            else throw champions::FormatError("Unknown texture filter.");
            const PackedByteArray original = result["rgba"];
            champions::Texture32 source{static_cast<uint32_t>(int64_t(result["width"])),
                static_cast<uint32_t>(int64_t(result["height"])), static_cast<uint16_t>(int64_t(result["flags"])),
                std::vector<uint8_t>(original.ptr(), original.ptr() + original.size())};
            if (opaque_rgb) for (std::size_t i = 3; i < source.rgba.size(); i += 4) source.rgba[i] = 255;
            const auto scaled = champions::upscaleTexture(source, static_cast<unsigned>(factor), mode);
            PackedByteArray pixels;
            if (pixels.resize(static_cast<int64_t>(scaled.rgba.size())) != OK) throw std::bad_alloc();
            std::copy(scaled.rgba.begin(), scaled.rgba.end(), pixels.ptrw());
            PackedByteArray measured_source = original;
            if(opaque_rgb) for(int64_t i=3;i<measured_source.size();i+=4) measured_source.set(i,255);
            result["fidelity"] = texture_fidelity(source.width,source.height,measured_source,scaled.width,scaled.height,pixels,factor);
            result["upscaled_rgba"] = pixels;
            result["upscaled_width"] = int64_t(scaled.width);
            result["upscaled_height"] = int64_t(scaled.height);
        } catch (const std::exception &error) {
            result["ok"] = false;
            result["error"] = String::utf8(error.what());
        }
        return result;
    }

    Dictionary sample_animation(const String &path, int64_t row_index, const String &profile,
                                const String &archive_fingerprint, double tick) const {
        Dictionary failure;
        failure["ok"] = false;
        failure["error"] = String();
        failure["name"] = String();
        failure["duration_ticks"] = int64_t(0);
        failure["tick"] = 0.0;
        failure["parents"] = PackedInt32Array();
        failure["origins"] = PackedVector3Array();
        failure["valid"] = PackedByteArray();
        failure["drives_joint"] = PackedByteArray();
        try {
            if (profile != "rta") throw champions::FormatError("Animation inspection is supported only for Return to Arms. Norrath compatibility is unverified.");
            if (!std::isfinite(tick) || tick < 0) throw champions::FormatError("Choose a finite animation tick at or above zero.");
            // The owned selected archive is fingerprinted and every nested
            // container is revalidated for each explicit sample. No model pairing.
            const auto selected = read_selected(path, row_index, archive_fingerprint);
            if (!champions::hasExtension(selected.name, ".anm")) throw champions::FormatError("Select an .anm entry for joint-only animation inspection.");
            const auto animation = champions::decodeRtaAnimationKeys(selected.bytes);
            const auto pose = champions::sampleRtaAnimationPose(animation, tick);
            const auto transforms = champions::buildRtaAnimationTransforms(animation.skeleton, pose);
            const auto count = static_cast<int64_t>(transforms.joints.size());
            PackedInt32Array parents;
            PackedVector3Array origins;
            PackedByteArray valid, drives;
            if (parents.resize(count) != OK || origins.resize(count) != OK || valid.resize(count) != OK || drives.resize(count) != OK) throw std::bad_alloc();
            for (int64_t i = 0; i < count; ++i) {
                const auto index = static_cast<std::size_t>(i);
                const auto &joint = transforms.joints[index];
                // Standard Godot uses single-precision Vector3. Copy native Z-up
                // origins; axis presentation belongs to the UI, not this decoder.
                const Vector3 origin(joint.model[3], joint.model[7], joint.model[11]);
                if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)) {
                    throw champions::FormatError("Animation joint origin exceeds the viewer's numeric range.");
                }
                parents.set(i, animation.skeleton.parents[index]);
                origins.set(i, origin);
                valid.set(i, joint.valid ? 1 : 0);
                drives.set(i, joint.drives_joint ? 1 : 0);
            }
            // Publish only completed owned arrays. Any decoding/allocation error
            // returns the separate empty result, never a partially sampled pose.
            Dictionary result;
            result["ok"] = true;
            result["error"] = String();
            result["name"] = String::utf8(champions::escapeTsv(selected.name).c_str());
            result["duration_ticks"] = static_cast<int64_t>(animation.skeleton.duration_ticks);
            result["tick"] = pose.tick;
            result["parents"] = parents;
            result["origins"] = origins;
            result["valid"] = valid;
            result["drives_joint"] = drives;
            return result;
        } catch (const std::bad_alloc &) {
            failure["error"] = "Not enough memory to inspect this animation.";
        } catch (const std::exception &error) {
            failure["error"] = String::utf8(error.what());
        } catch (...) {
            failure["error"] = "The animation could not be inspected.";
        }
        return failure;
    }

    Dictionary sample_skinned_model(const String &path, int64_t model_row_index,
            int64_t animation_row_index, const String &profile,
            const String &archive_fingerprint, double tick) const {
        Dictionary failure;
        failure["ok"] = false;
        failure["error"] = String();
        failure["model_name"] = String();
        failure["animation_name"] = String();
        failure["duration_ticks"] = int64_t(0);
        failure["tick"] = 0.0;
        failure["pairing_verified"] = false;
        failure["meshes"] = Array();
        failure["triangle_count"] = int64_t(0);
        try {
            if(profile!="rta") throw champions::FormatError("Skinned position inspection is supported only for Return to Arms. Norrath compatibility is unverified.");
            if(!std::isfinite(tick) || tick<0) throw champions::FormatError("Choose a finite animation tick at or above zero.");
            const auto selected=read_selected_entries(path,
                std::array<int64_t,2>{{model_row_index,animation_row_index}},archive_fingerprint);
            if(!champions::hasExtension(selected[0].name,".vif") || !champions::hasExtension(selected[1].name,".anm")) {
                throw champions::FormatError("Explicitly select a .vif model and an .anm animation entry.");
            }
            const auto model=champions::decodeRtaModel(selected[0].bytes);
            if(model.attachment_slot!=-1) throw champions::FormatError("Direct skinning does not support parent attachment models.");
            const auto animation=champions::decodeRtaAnimationKeys(selected[1].bytes);
            if(animation.skeleton.joint_mask!=0) throw champions::FormatError("Direct skinning requires a complete animation, not a partial-body clip.");
            const auto pose=champions::sampleRtaAnimationPose(animation,tick);
            const auto transforms=champions::buildRtaAnimationTransforms(animation.skeleton,pose);
            const auto skinned=champions::skinRtaModelPositions(model,transforms);
            const auto skinned_normals=champions::skinRtaModelNormals(model,transforms);
            Array meshes;
            std::size_t corners=0;
            for(std::size_t i=0;i<skinned.meshes.size();++i) {
                const auto &mesh=skinned.meshes[i];
                const auto &normal_mesh=skinned_normals.meshes[i];
                PackedVector3Array positions, normals;
                if(positions.resize(static_cast<int64_t>(mesh.positions.size()))!=OK ||
                   normals.resize(static_cast<int64_t>(normal_mesh.normals.size()))!=OK) throw std::bad_alloc();
                for(std::size_t corner=0;corner<mesh.positions.size();++corner) {
                    const auto &point=mesh.positions[corner];
                    const Vector3 converted(point[0],point[1],point[2]);
                    if(!std::isfinite(converted.x) || !std::isfinite(converted.y) || !std::isfinite(converted.z)) {
                        throw champions::FormatError("Skinned position exceeds the viewer's numeric range.");
                    }
                    positions.set(static_cast<int64_t>(corner),converted);
                    const auto &normal=normal_mesh.normals[corner];
                    const Vector3 converted_normal(normal[0],normal[1],normal[2]);
                    if(!std::isfinite(converted_normal.x) || !std::isfinite(converted_normal.y) || !std::isfinite(converted_normal.z)) {
                        throw champions::FormatError("Skinned normal exceeds the viewer's numeric range.");
                    }
                    normals.set(static_cast<int64_t>(corner),converted_normal);
                }
                Dictionary item;
                item["index"]=static_cast<int64_t>(mesh.index);
                item["visible_by_default"]=model.meshes[i].visible_by_default;
                item["positions"]=positions; // Native Z-up.
                item["normals"]=normals; // Blended skin 3x3, same corner order, not unit length.
                meshes.push_back(item);
                corners+=mesh.positions.size();
            }
            // No name, lump membership, joint count, or successful arithmetic
            // proves semantic pairing. Materials and textures are not returned.
            Dictionary result;
            result["ok"]=true;
            result["error"]=String();
            result["model_name"]=String::utf8(champions::escapeTsv(selected[0].name).c_str());
            result["animation_name"]=String::utf8(champions::escapeTsv(selected[1].name).c_str());
            result["duration_ticks"]=static_cast<int64_t>(animation.skeleton.duration_ticks);
            result["tick"]=pose.tick;
            result["pairing_verified"]=false;
            result["meshes"]=meshes;
            result["triangle_count"]=static_cast<int64_t>(corners/3);
            return result;
        } catch(const std::bad_alloc &) {
            failure["error"]="Not enough memory to sample this model and animation.";
        } catch(const std::exception &error) {
            failure["error"]=String::utf8(error.what());
        } catch(...) {
            failure["error"]="The skinned model positions could not be inspected.";
        }
        return failure;
    }

    Dictionary inspect_model(const String &path, int64_t row_index, const String &profile,
                             const String &archive_fingerprint) const {
        Dictionary result;
        result["ok"] = false;
        result["error"] = String();
        result["meshes"] = Array();
        result["has_variants"] = false;
        result["texture"] = texture_status("unavailable", "No model has been decoded.");
        try {
            if (profile != "rta") throw champions::FormatError("Model preview is supported only for Return to Arms. Norrath compatibility is unverified.");
            const auto selected = read_selected(path, row_index, archive_fingerprint);
            if (!champions::hasExtension(selected.name, ".vif")) throw champions::FormatError("Select a .vif entry for supported rest-pose model inspection.");
            const auto model = champions::decodeRtaModel(selected.bytes);
            Array meshes;
            std::array<std::uint64_t,32> variant_masks{};
            if (model.has_variants) {
                for (std::size_t i = 0; i < variant_masks.size(); ++i) {
                    variant_masks[i] = champions::modelVisibilityMask(model,static_cast<int>(i));
                }
            }
            std::size_t total_corners = 0;
            for (const auto &mesh : model.meshes) {
                if (mesh.index >= 64 || mesh.corners.size() % 3 || mesh.corners.size() > 600000 - total_corners) {
                    throw champions::FormatError("Model exceeds the 200,000-triangle preview limit.");
                }
                total_corners += mesh.corners.size();
                PackedVector3Array positions, normals;
                PackedVector2Array uv;
                const auto count = static_cast<int64_t>(mesh.corners.size());
                if (positions.resize(count) != OK || normals.resize(count) != OK || uv.resize(count) != OK) throw std::bad_alloc();
                for (int64_t i = 0; i < count; ++i) {
                    const auto &corner = mesh.corners[static_cast<std::size_t>(i)];
                    positions.set(i, Vector3(corner.position[0], corner.position[1], corner.position[2]));
                    normals.set(i, Vector3(corner.normal[0], corner.normal[1], corner.normal[2]));
                    uv.set(i, Vector2(corner.uv_texels[0], corner.uv_texels[1]));
                }
                Dictionary item;
                item["index"] = static_cast<int64_t>(mesh.index);
                item["visible_by_default"] = mesh.visible_by_default;
                if (model.has_variants) {
                    PackedByteArray visibility;
                    if (visibility.resize(32) != OK) throw std::bad_alloc();
                    for (std::size_t i = 0; i < variant_masks.size(); ++i) {
                        visibility.set(static_cast<int64_t>(i),static_cast<std::uint8_t>((variant_masks[i] >> mesh.index) & 1U));
                    }
                    item["variant_visibility"] = visibility;
                }
                item["joint"] = static_cast<int64_t>(mesh.joint);
                item["positions"] = positions;
                item["normals"] = normals;
                item["uv_texels"] = uv;
                meshes.push_back(item);
            }
            result["texture"] = model_texture(selected);
            result["meshes"] = meshes;
            result["has_variants"] = model.has_variants;
            result["triangle_count"] = static_cast<int64_t>(total_corners / 3);
            result["flags"] = static_cast<int64_t>(model.flags);
            // Decimal strings retain every bit of unsigned 64-bit masks.
            result["hidden_mask"] = String::utf8(std::to_string(model.hidden_mask).c_str());
            result["part_mask"] = String::utf8(std::to_string(model.part_mask).c_str());
            result["name"] = String::utf8(champions::escapeTsv(selected.name).c_str());
            result["ok"] = true;
        } catch (const std::bad_alloc &) {
            result["error"] = "Not enough memory to preview this model.";
        } catch (const std::exception &error) {
            result["error"] = String::utf8(error.what());
        } catch (...) {
            result["error"] = "The model could not be inspected.";
        }
        if (!static_cast<bool>(result["ok"])) {
            result["meshes"] = Array();
            result["has_variants"] = false;
            result["texture"] = texture_status("unavailable", "No model has been decoded.");
        }
        return result;
    }

};

void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(ChampionsArchiveInspector);
        GDREGISTER_CLASS(ChampionsSessionController);
        register_ui_layout_service();
        register_level_lights_service();
        register_ps2_save_import();
        GDREGISTER_CLASS(ChampionsFxBridge);
        GDREGISTER_CLASS(ChampionsNativeAudio);
    }
}
void terminate(ModuleInitializationLevel) {}
} // namespace

extern "C" {
GDExtensionBool GDE_EXPORT champions_archive_init(GDExtensionInterfaceGetProcAddress get_proc_address,
        GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(terminate);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
}
