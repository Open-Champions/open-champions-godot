#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace champions::inventory_icons {

struct PacketSpan {
    std::size_t offset = 0;
    std::uint16_t qwords = 0;
};

struct DmaChunk {
    std::uint32_t tag_word = 0;
    std::vector<std::uint8_t> bytes;
};

struct Picture {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t flags = 0;
    std::uint16_t qwords = 0;
    std::size_t packet_offset = 0;
    std::vector<std::uint8_t> rgba;
};

// Sector payload only, starting at DAT + first_sector * 2048. HDR/name
// association is exposed separately in inventory_icon_assoc.hpp.
PacketSpan packet_span(const std::uint8_t *data, std::size_t size);
std::vector<DmaChunk> dma_chunks(const std::uint8_t *data, std::size_t size);
std::size_t gs32_word(std::size_t x, std::size_t y, std::size_t dbw = 1);
std::size_t gs8_byte(std::size_t x, std::size_t y, std::size_t tbw = 2);
Picture decode(const std::uint8_t *data, std::size_t size);
std::vector<std::uint8_t> display_rgba(const std::vector<std::uint8_t> &rgba);
// The gold-pile bottom alignment uses the 0x0800 transparent padding row.
// Inventory grid placement does not.
std::int32_t bottom_anchor_y(std::int32_t anchor, std::int32_t height, std::uint16_t flags);

} // namespace champions::inventory_icons
