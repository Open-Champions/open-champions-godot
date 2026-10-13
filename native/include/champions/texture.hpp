#pragma once

#include "champions/archive.hpp"
#include <cstdint>
#include <vector>

namespace champions {

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

// General narrow RTA lump decoder: the same 32-bit form above, or one 16-entry
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

} // namespace champions
