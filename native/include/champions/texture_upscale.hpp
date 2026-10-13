#pragma once
#include "champions/texture.hpp"
namespace champions {
enum class TextureFilter { Nearest, PixelArt, Bicubic };
// Presentation only. Exact RGBA comparisons; clamped borders. 2, 3 or 4 only.
// Refuses malformed input, dimensions >1024, output >4096 or >64 MiB.
// Integer arithmetic includes premultiplied Catmull-Rom bicubic interpolation.
Texture32 upscaleTexture(const Texture32 &input, unsigned factor, TextureFilter filter);
}
