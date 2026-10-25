#pragma once

#include "champions/archive.hpp"
#include <cstdint>
#include <vector>

namespace champions {

// Header-only RTA inspection, including unsupported packet/storage variants.
// This does not validate a GIF packet, identify UI art or promise a decode.
struct RtaTextureHeader {
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t flags;
    std::uint8_t psm;
    bool isUiCandidate() const { return (flags & 0x0040U) != 0; }
};
RtaTextureHeader readRtaTextureHeader(Bytes bytes);

// Owned RGBA8 output; the name describes the output, not the source storage.
struct Texture32 {
    std::uint32_t width;
    std::uint32_t height;
    std::uint16_t flags;
    std::vector<std::uint8_t> rgba;
};

// Narrow RTA lump .tex/.etex decoder: one PSMCT32 upload, no palette or runtime
// relocation. Output is owned RGBA8 in stored row order, cropped to used width
// and height. Alpha is min(255, raw * 255 / 128), not a recovered blend rule.
// No interface-field weaving, swizzling, mipmaps or Norrath compatibility claim.
// Unsupported or malformed data raises FormatError; dimensions are <= 1024.
Texture32 decodeRtaTexture32(Bytes bytes);

// General narrow RTA lump decoder: the same 32-bit form above.
// The general API also permits PSMCT32 packets to omit initial TRXPOS,
// using the documented zero destination origin; the strict API requires it.
// Also accepts one 16-entry
// RGBA palette (8x2) followed by one plain-row PSMT4 image, low nibble first.
// Both return the same owned RGBA8 result and alpha conversion. These forms
// must cover all used pixels; unused power-of-two surface padding is cropped.
// Also accepts a 256-entry RGBA palette followed by PSMCT32 rectangles sampled
// as PSMT8 through bounded 4 MiB GS memory. CLUT bits 3/4 are exchanged; unwritten
// indices are zero. Overlapping rectangles apply in upload order.
// Also accepts the documented 16-entry palette with PSMCT32 rectangles sampled
// as PSMT4 nibbles (plain palette order), using the same bounded GS memory.
// Rejects partial 32-bit/plain 4-bit images and runtime/world forms.
// Stored row order is preserved; no field weaving or Norrath support is implied.
Texture32 decodeRtaTexture(Bytes bytes);

// PSMT8 pixel decode of a GIF packet that begins at packet_offset.
// The lump rule that the packet starts at 0x80 is not applied. Sampling,
// CLUT exchange and alpha match decodeRtaTexture. uncovered counts used
// texels whose GS byte the image upload did not write.
struct Indexed8Packet {
    Texture32 image;
    std::size_t uncovered = 0;
};
Indexed8Packet decodeRtaPsmt8Packet(Bytes bytes, std::size_t packet_offset);

} // namespace champions
