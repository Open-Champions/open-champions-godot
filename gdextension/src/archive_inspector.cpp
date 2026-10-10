#include "champions/archive.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <algorithm>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>

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

class ChampionsArchiveInspector : public RefCounted {
    GDCLASS(ChampionsArchiveInspector, RefCounted)
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("inspect_archive", "path"), &ChampionsArchiveInspector::inspect_archive);
    }
public:
    Dictionary inspect_archive(const String &path) const {
        Dictionary result;
        result["ok"] = false;
        result["error"] = String();
        result["rows"] = Array();
        result["file_size"] = int64_t(0);
        try {
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
            // This owning buffer lives until every nested parser and copy has finished.
            const PackedByteArray data = file->get_buffer(size);
            if (static_cast<std::uint64_t>(data.size()) != size) throw champions::FormatError("Could not read the complete archive.");
            const champions::Bytes bytes{data.ptr(), static_cast<std::size_t>(data.size())};
            Array rows;
            if (gob) {
                bound_gob(bytes);
                const auto entries = champions::readGob(bytes);
                for (const auto &entry : entries) {
                    add_row(rows, "GOB", "", entry, 0);
                    if (champions::hasExtension(entry.name, ".lmp")) {
                        const auto nested = champions::payload(bytes, entry);
                        try {
                            bound_lmp(nested, max_rows - static_cast<std::size_t>(rows.size()));
                            for (const auto &child : champions::readLmp(nested)) {
                                add_row(rows, "LMP", entry.name, child, entry.offset);
                            }
                        } catch (const champions::FormatError &error) {
                            throw champions::FormatError("Nested LMP '" + champions::escapeTsv(entry.name) + "': " + error.what());
                        }
                    }
                }
            } else {
                bound_lmp(bytes, max_rows);
                for (const auto &entry : champions::readLmp(bytes)) add_row(rows, "LMP", "", entry, 0);
            }
            // Publish rows atomically after all nested input has passed validation.
            result["rows"] = rows;
            result["file_size"] = static_cast<int64_t>(size);
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
