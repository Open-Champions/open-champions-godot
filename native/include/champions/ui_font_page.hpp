#pragma once
#include "champions/texture.hpp"
#include <cstdint>
#include <vector>

namespace champions {

// Embedded FNT page. The GIF packet starts at the u32 at TEX +0x10, which is
// not a fixed 0x80. One 16x16 PSMCT32 palette, then one PSMCT32 image.
// Palette DBP is the halfword at packet +0x14. The 8-bit image DBP is the
// halfword at packet +0x474. Header +2 of the FNT is not a field of this page.
struct FontPageInfo {
    std::uint16_t width = 0, height = 0, blocks = 0, qwords = 0, flags = 0;
    std::uint32_t tex0 = 0, packet_offset = 0, tbw = 0;
    std::uint32_t surface_width = 0, surface_height = 0;
};
struct DecodedFontPage {
    Texture32 image;
    std::size_t uncovered = 0;
    std::uint32_t packet_offset = 0;
    std::uint16_t flags = 0;
};

// Second DBP halfword written by gfx_dmaHandler, or -1 when only +0x14 is written.
// Flag 0x0004 is tested first, so an 8-bit page does not also take the 4-bit store.
int fontPageImageDbp(std::uint16_t flags);

// Prefix of tex_relocate when flags lack 0x0001 and 0x0100. Adds base to the
// packet offset, stores 0xa7fb at +0x0a and -1 at +0x1c, and leaves +0/+2 and
// the qword count unchanged. A later call sees +0x1c and changes nothing.
std::vector<std::uint8_t> applyTexRelocatePrefix(std::uint32_t base, Bytes bytes);

FontPageInfo parseFontPage(Bytes bytes);
// weave interleaves the two fields. Glyph rectangles are in that space.
// The default is stored order. Alpha follows the lump scale (0x80 becomes 255).
DecodedFontPage decodeFontPage(Bytes bytes, bool weave = false);

// FNT header +2 has no reader. Line height stays a caller argument.
[[noreturn]] void refuseFntHeaderLineHeight();

} // namespace champions
