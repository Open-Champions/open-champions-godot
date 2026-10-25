#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace champions::inventory_icons {

// Supplied fields read by item_buildPictureFilename. No picture id is stored
// on an item; caller-owned game tables provide model names, prefix colours,
// and the viewing player's twelve body codes.
struct PictureDefinition {
    std::string name;
    std::int32_t category = 0, slot = 0, subtype = 0;
    std::int32_t damage_min = 0, damage_max = 0;
    std::optional<std::string> picture;
    std::size_t model_row = 0;
};
struct PictureItem {
    PictureDefinition definition;
    std::uint64_t flags = 0;
    std::uint8_t prefix = 0xff;
};
struct PictureTables {
    std::vector<std::string> model_textures;
    std::vector<std::array<std::int8_t, 3>> prefix_colours;
    std::array<std::string, 12> body_codes{};
};

inline constexpr std::uint64_t CLOTH = 1ULL << 19;
inline constexpr std::uint64_t ALT_COLOUR = 1ULL << 23;
inline constexpr std::uint64_t PLAIN_WEAPON = 1ULL << 34;
inline constexpr std::uint8_t NO_PREFIX = 0xff;
inline constexpr const char *PLACEHOLDER_ARCHIVE = "equipmnt.lmp";
inline constexpr const char *PLACEHOLDER_LUMP = "pic_dagger1.tex";

std::int32_t armour_body(std::int32_t body_type, bool cloth);
std::string picture_filename(const PictureItem &item, std::int32_t body_type,
                             const PictureTables &tables,
                             std::optional<std::int32_t> item_value = std::nullopt);
std::optional<std::pair<std::string, std::int32_t>> split_picture_name(std::string name);

struct IndexEntry {
    std::int16_t number = 0;
    std::uint16_t first_sector = 0, sectors = 0;
    std::int16_t width = 0, height = 0;
};
struct IndexGroup {
    std::string stem;
    std::vector<IndexEntry> entries;
};
using PictureIndex = std::vector<IndexGroup>;

PictureIndex parse_index(const std::uint8_t *data, std::size_t size);
const IndexEntry *find_entry(const PictureIndex &groups, const std::string &name);
struct PayloadRange { std::size_t offset = 0, size = 0; };
PayloadRange payload_range(const IndexEntry &entry, std::size_t dat_size);

enum class CacheState { Pending, Reading, Loaded, Missing };
struct CacheSlot {
    std::string name;
    CacheState state = CacheState::Pending;
    std::int64_t last_use = 0;
    std::optional<IndexEntry> entry;
};
class PictureCache {
public:
    explicit PictureCache(PictureIndex groups) : groups_(std::move(groups)) {}
    std::optional<IndexEntry> request(const std::string &name, std::int64_t frame);
    // Call once per displayed frame. read_done is completion of the outstanding
    // external read; this model owns no I/O or PS2 runtime data blocks.
    void service(std::int64_t frame, bool read_done);
    const std::vector<CacheSlot> &slots() const { return slots_; }
    bool reading() const { return loading_.has_value(); }
private:
    static constexpr std::size_t SLOT_COUNT = 64;
    static constexpr std::int64_t IDLE_FRAMES = 8;
    bool evict(std::int64_t frame);
    PictureIndex groups_;
    std::vector<CacheSlot> slots_;
    std::optional<std::size_t> loading_;
};

struct PictureSource {
    bool placeholder = true;
    std::optional<IndexEntry> entry;
    std::string archive = PLACEHOLDER_ARCHIVE;
    std::string lump = PLACEHOLDER_LUMP;
};
PictureSource picture_or_placeholder(std::optional<IndexEntry> loaded);

struct GridPlacement {
    std::int32_t columns = 0, rows = 0;
    std::int32_t footprint_width = 0, footprint_height = 0;
    std::int32_t x = 0, y = 0;
};
// Grid placement uses HDR dimensions even before pixels arrive and ignores
// the picture's 0x0800 bottom-padding flag, as itemList_drawGrid does.
GridPlacement grid_placement(std::int32_t width, std::int32_t height,
                             std::int32_t panel_x, std::int32_t column,
                             float row_y);

} // namespace champions::inventory_icons
