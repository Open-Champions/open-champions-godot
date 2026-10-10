#pragma once

#include "champions/archive.hpp"
#include <cstdint>
#include <vector>

namespace champions {

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

} // namespace champions
