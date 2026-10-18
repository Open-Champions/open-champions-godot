#include "champions/inventory_icon_assoc.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace champions::inventory_icons {
namespace {

std::uint16_t u16(const std::uint8_t *bytes, std::size_t at) {
    return std::uint16_t(bytes[at]) | (std::uint16_t(bytes[at + 1]) << 8);
}
std::uint32_t u32(const std::uint8_t *bytes, std::size_t at) {
    return std::uint32_t(u16(bytes, at)) | (std::uint32_t(u16(bytes, at + 2)) << 16);
}
std::int16_t s16(const std::uint8_t *bytes, std::size_t at) {
    const auto bits = u16(bytes, at);
    return bits < 0x8000 ? std::int16_t(bits) : std::int16_t(std::int32_t(bits) - 0x10000);
}
void require(bool condition, const char *message) {
    if (!condition) throw std::invalid_argument(message);
}
std::string splice(const std::string &text, std::size_t cut, const std::string &suffix) {
    require(text.size() >= cut, "picture base too short");
    return text.substr(0, text.size() - cut) + suffix;
}
std::string circlet_sheet(const std::string &name) {
    char kind = '3';
    if (name.find("circlet1") != std::string::npos) kind = '1';
    else if (name.find("circlet2") != std::string::npos) kind = '2';
    const std::string head = std::string("circlet") + kind;
    require(name.compare(0, head.size(), head) == 0, "circlet number unreadable");
    std::size_t at = head.size();
    while (at < name.size() && (name[at] == ' ' || name[at] == '\t' ||
                                name[at] == '\n' || name[at] == '\r')) ++at;
    bool negative = false;
    if (at < name.size() && (name[at] == '-' || name[at] == '+')) {
        negative = name[at] == '-';
        ++at;
    }
    require(at < name.size() && name[at] >= '0' && name[at] <= '9',
            "circlet number unreadable");
    std::int64_t value = 0;
    while (at < name.size() && name[at] >= '0' && name[at] <= '9') {
        const auto digit = name[at++] - '0';
        const std::int64_t bound = std::int64_t(std::numeric_limits<std::int32_t>::max()) + (negative ? 1 : 0);
        require(value <= (bound - digit) / 10, "circlet number outside signed32");
        value = value * 10 + digit;
    }
    if (negative) value = -value;
    return "cloth" + std::string(1, kind) + "helmet6" + std::to_string(value) + ".tex";
}
std::int32_t checked_int(std::int64_t value) {
    require(value >= std::numeric_limits<std::int32_t>::min() &&
            value <= std::numeric_limits<std::int32_t>::max(), "grid placement outside signed32");
    return static_cast<std::int32_t>(value);
}

} // namespace

std::int32_t armour_body(std::int32_t body_type, bool cloth) {
    require(body_type >= 0 && body_type < 12, "body type outside documented range");
    const bool odd_low = (body_type & 1) && body_type < 10;
    if (cloth) return body_type == 6 || body_type == 7 ? body_type : (odd_low ? 7 : 6);
    if (body_type == 6 || body_type == 7) return odd_low ? 1 : 0;
    return body_type;
}

std::string picture_filename(const PictureItem &item, std::int32_t body_type,
                             const PictureTables &tables,
                             std::optional<std::int32_t> item_value) {
    require(body_type >= 0 && body_type < 12, "body type outside documented range");
    const auto &d = item.definition;
    std::string base;
    if (d.picture && !d.picture->empty()) base = *d.picture + ".tex";
    else {
        require(d.model_row < tables.model_textures.size(), "model texture row out of range");
        base = tables.model_textures[d.model_row];
    }
    const auto variant = std::int32_t(((item.flags >> 11) + 1) & 31);
    const auto prefix = item.prefix;
    if (d.category == 0 && !(item.flags & PLAIN_WEAPON)) {
        if (prefix != NO_PREFIX) require(prefix < tables.prefix_colours.size(), "prefix colour row out of range");
        std::int32_t colour = prefix != NO_PREFIX ? tables.prefix_colours[prefix][0] : 1;
        if (d.subtype == 4) colour = 1;
        if (prefix != NO_PREFIX && (item.flags & ALT_COLOUR)) colour = tables.prefix_colours[prefix][2];
        if (d.name.find('+') != std::string::npos) {
            colour = std::int32_t((std::int64_t(d.damage_min) + d.damage_max) % 18);
        }
        if (colour == 0) colour = 1;
        return splice(base, 5, std::to_string(variant) + std::to_string(colour) + ".tex");
    }
    if (d.category == 1) {
        if (d.slot == 3) return base;
        if (prefix != NO_PREFIX) require(prefix < tables.prefix_colours.size(), "prefix colour row out of range");
        std::int32_t body = body_type;
        if (d.name.find("circlet") != std::string::npos) {
            if (body == 6 || body == 7) base = circlet_sheet(d.name);
        } else body = armour_body(body_type, (item.flags & CLOTH) != 0);
        std::string name = tables.body_codes[static_cast<std::size_t>(body)] + "_" + base;
        std::int32_t colour = prefix != NO_PREFIX ? tables.prefix_colours[prefix][1] : 1;
        if (d.name.find('+') != std::string::npos) {
            require(item_value.has_value(), "preset armour colour needs item value");
            colour = *item_value % 4;
        }
        if (prefix == NO_PREFIX) colour = 1;
        return splice(name, 4, std::to_string(colour) + ".tex");
    }
    if (base.find("1.tex") != std::string::npos) {
        return splice(base, 5, std::to_string(variant) + ".tex");
    }
    return base;
}

std::optional<std::pair<std::string, std::int32_t>> split_picture_name(std::string name) {
    const auto dot = name.rfind('.');
    if (dot != std::string::npos) name.resize(dot);
    if (name.empty()) return std::nullopt;
    std::size_t at = name.size();
    while (at > 1 && name[at - 1] >= '0' && name[at - 1] <= '9') --at;
    std::int64_t number = -1;
    if (at < name.size()) {
        number = 0;
        for (std::size_t i = at; i < name.size(); ++i) {
            const auto digit = name[i] - '0';
            require(number <= (std::numeric_limits<std::int32_t>::max() - digit) / 10,
                    "picture suffix outside signed32");
            number = number * 10 + digit;
        }
    }
    name.resize(at);
    return std::make_pair(std::move(name), static_cast<std::int32_t>(number));
}

PictureIndex parse_index(const std::uint8_t *data, std::size_t size) {
    require(data != nullptr && size >= 4, "short picture index");
    const auto count = u32(data, 0);
    require(count <= (size - 4) / 12, "picture group table out of range");
    PictureIndex groups;
    groups.reserve(count);
    for (std::size_t g = 0; g < count; ++g) {
        const auto base = 4 + g * 12;
        const std::size_t entries_at = u32(data, base);
        const std::size_t stem_at = u32(data, base + 4);
        const std::size_t n = u32(data, base + 8);
        require(stem_at < size && entries_at <= size && n <= (size - entries_at) / 10,
                "picture group out of range");
        const void *end = std::memchr(data + stem_at, 0, size - stem_at);
        require(end != nullptr, "unterminated picture stem");
        IndexGroup group;
        group.stem.assign(reinterpret_cast<const char *>(data + stem_at),
                          static_cast<const std::uint8_t *>(end) - (data + stem_at));
        group.entries.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            const auto at = entries_at + i * 10;
            group.entries.push_back({s16(data, at), u16(data, at + 2), u16(data, at + 4),
                                     s16(data, at + 6), s16(data, at + 8)});
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

const IndexEntry *find_entry(const PictureIndex &groups, const std::string &name) {
    const auto split = split_picture_name(name);
    if (!split) return nullptr;
    for (const auto &group : groups) {
        if (group.stem != split->first) continue;
        for (const auto &entry : group.entries) {
            if (entry.number == split->second) return &entry;
        }
    }
    return nullptr;
}

PayloadRange payload_range(const IndexEntry &entry, std::size_t dat_size) {
    constexpr std::size_t sector_bytes = 2048;
    const std::size_t offset = std::size_t(entry.first_sector) * sector_bytes;
    const std::size_t bytes = std::size_t(entry.sectors) * sector_bytes;
    require(bytes > 0 && offset <= dat_size && bytes <= dat_size - offset,
            "picture sector range outside DAT");
    return {offset, bytes};
}

bool PictureCache::evict(std::int64_t frame) {
    // The original compares last use strictly with frame minus eight. Use
    // subtraction without signed overflow for a caller-supplied frame.
    require(frame >= std::numeric_limits<std::int64_t>::min() + IDLE_FRAMES,
            "display frame outside cache domain");
    std::int64_t limit = frame - IDLE_FRAMES;
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (loading_ && *loading_ == i) continue;
        if (slots_[i].last_use < limit) { best = i; limit = slots_[i].last_use; }
    }
    if (!best) return false;
    slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(*best));
    if (loading_ && *loading_ > *best) --*loading_;
    return true;
}

std::optional<IndexEntry> PictureCache::request(const std::string &name, std::int64_t frame) {
    for (auto &slot : slots_) {
        if (slot.name != name) continue;
        if (slot.state != CacheState::Loaded) return std::nullopt;
        slot.last_use = frame;
        return slot.entry;
    }
    if (slots_.size() == SLOT_COUNT && !evict(frame)) return std::nullopt;
    slots_.push_back({name, CacheState::Pending, frame, std::nullopt});
    return std::nullopt;
}

void PictureCache::service(std::int64_t frame, bool read_done) {
    if (loading_) {
        auto &slot = slots_[*loading_];
        slot.last_use = frame;
        if (!read_done) return;
        slot.state = CacheState::Loaded;
        loading_.reset();
    }
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        auto &slot = slots_[i];
        if (slot.state != CacheState::Pending) continue;
        const auto *entry = find_entry(groups_, slot.name);
        if (!entry) { slot.state = CacheState::Missing; continue; }
        slot.entry = *entry;
        slot.last_use = frame;
        slot.state = CacheState::Reading;
        loading_ = i;
        return;
    }
}

PictureSource picture_or_placeholder(std::optional<IndexEntry> loaded) {
    if (!loaded) return {};
    PictureSource result;
    result.placeholder = false;
    result.entry = *loaded;
    result.archive.clear();
    result.lump.clear();
    return result;
}

GridPlacement grid_placement(std::int32_t width, std::int32_t height,
                             std::int32_t panel_x, std::int32_t column,
                             float row_y) {
    require(width > 0 && height > 0 && column >= 0 && std::isfinite(row_y),
            "invalid grid picture or position");
    const std::int64_t columns = std::int64_t(width) / 32 + 1;
    std::uint32_t multiplier_bits = 0x3cf0f0f0;
    float multiplier;
    std::memcpy(&multiplier, &multiplier_bits, sizeof(multiplier));
    const float rows_f = std::ceil(float(height) * multiplier);
    require(std::isfinite(rows_f) && rows_f > 0 && rows_f < 2147483648.0f,
            "grid row span outside signed32");
    const std::int64_t rows = static_cast<std::int32_t>(rows_f);
    const std::int64_t foot_w = columns * 32, foot_h = rows * 34;
    const float base_y_f = row_y - 30.0f;
    require(std::isfinite(base_y_f) && base_y_f >= float(std::numeric_limits<std::int32_t>::min()) &&
            base_y_f < float(std::numeric_limits<std::int32_t>::max()),
            "grid row origin outside signed32");
    const auto base_y = static_cast<std::int32_t>(base_y_f);
    const std::int64_t x = std::int64_t(panel_x) + 15 + std::int64_t(column) * 32 + foot_w / 2 - width / 2;
    const std::int64_t y = std::int64_t(base_y) + (foot_h - height) / 2 + 6;
    return {checked_int(columns), checked_int(rows), checked_int(foot_w),
            checked_int(foot_h), checked_int(x), checked_int(y)};
}

} // namespace champions::inventory_icons
