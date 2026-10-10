#include "champions/archive.hpp"
#include "champions/texture.hpp"
#include "champions/model.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/hashing_context.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

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

ArchiveInput read_archive(const String &path) {
    if (path.is_empty()) throw champions::FormatError("Choose a .GOB or .LMP file first.");
    const auto encoded = path.utf8();
    const std::string filename(encoded.get_data(), static_cast<std::size_t>(encoded.length()));
    const bool gob = champions::hasExtension(filename, ".gob");
    if (!gob && !champions::hasExtension(filename, ".lmp")) {
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
};

// Retain the exact owned archive used for its fingerprint and complete validation.
SelectedInput read_selected(const String &path, int64_t row_index, const String &expected_fingerprint) {
    if (row_index < 0 || row_index >= static_cast<int64_t>(max_rows)) throw champions::FormatError("Choose a valid archive entry.");
    auto input = read_archive(path);
    if (expected_fingerprint.is_empty() || fingerprint(input.data) != expected_fingerprint) {
        throw champions::FormatError("The archive changed since it was opened. Reopen it before previewing an entry.");
    }
    const champions::Bytes bytes{input.data.ptr(), static_cast<std::size_t>(input.data.size())};
    champions::Bytes selected{nullptr, 0};
    std::string selected_name;
    bool found = false;
    walk_archive(bytes, input.gob, [&](std::size_t index, const char *, const std::string &,
                 const champions::Entry &entry, std::uint64_t, champions::Bytes container) {
        if (index == static_cast<std::size_t>(row_index)) {
            selected = champions::payload(container, entry);
            selected_name = entry.name;
            found = true;
        }
    });
    if (!found) throw champions::FormatError("The selected archive entry no longer exists. Reopen the archive.");
    return {std::move(input), selected, std::move(selected_name)};
}

class ChampionsArchiveInspector : public RefCounted {
    GDCLASS(ChampionsArchiveInspector, RefCounted)
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("inspect_archive", "path"), &ChampionsArchiveInspector::inspect_archive);
        ClassDB::bind_method(D_METHOD("inspect_texture", "path", "row_index", "profile", "archive_fingerprint"),
                            &ChampionsArchiveInspector::inspect_texture);
        ClassDB::bind_method(D_METHOD("inspect_model", "path", "row_index", "profile", "archive_fingerprint"),
                            &ChampionsArchiveInspector::inspect_model);
    }
public:
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

    Dictionary inspect_model(const String &path, int64_t row_index, const String &profile,
                             const String &archive_fingerprint) const {
        Dictionary result;
        result["ok"] = false;
        result["error"] = String();
        result["meshes"] = Array();
        try {
            if (profile != "rta") throw champions::FormatError("Model preview is supported only for Return to Arms. Norrath compatibility is unverified.");
            const auto selected = read_selected(path, row_index, archive_fingerprint);
            if (!champions::hasExtension(selected.name, ".vif")) throw champions::FormatError("Select a .vif entry for supported rest-pose model inspection.");
            const auto model = champions::decodeRtaModel(selected.bytes);
            Array meshes;
            std::size_t total_corners = 0;
            for (const auto &mesh : model.meshes) {
                if (mesh.corners.size() % 3 || mesh.corners.size() > 600000 - total_corners) {
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
                item["joint"] = static_cast<int64_t>(mesh.joint);
                item["positions"] = positions;
                item["normals"] = normals;
                item["uv_texels"] = uv;
                meshes.push_back(item);
            }
            result["meshes"] = meshes;
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
        if (!static_cast<bool>(result["ok"])) result["meshes"] = Array();
        return result;
    }

};

void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) GDREGISTER_CLASS(ChampionsArchiveInspector);
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
