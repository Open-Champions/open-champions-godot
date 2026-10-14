#pragma once
#include "champions/archive.hpp"
#include <array>
#include <vector>
namespace champions {
// RTA Fnt.parse subset only. Ordered raw fields: code,u0,u1,v0,v1,signed y,advance.
// No character-map, unused header or first-kerning-link interpretation.
struct RtaUiFont {
    std::uint32_t glyph_offset, kerning_offset, texture_offset;
    std::vector<std::array<std::int32_t, 7>> glyphs;
    std::vector<std::array<std::int32_t, 3>> kerns;
    std::vector<std::uint8_t> texture;
};
// Minimum 0x214 bytes; all offsets/ranges bounded; suffix is owned and nonempty.
// Metadata is independent of whether the existing texture decoder supports it.
RtaUiFont readRtaUiFont(Bytes bytes);
}
