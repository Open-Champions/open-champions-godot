#pragma once
#include "champions/texture.hpp"
namespace champions {
enum class TextureFilter { Nearest, PixelArt, Bicubic };
// Presentation only. Exact RGBA comparisons; clamped borders. 2, 3 or 4 only.
// Refuses malformed input, dimensions >1024, output >4096 or >64 MiB.
// Integer arithmetic includes premultiplied Catmull-Rom bicubic interpolation.
// Exact box-downsample error numerator; denominator is factor squared.
// Palette counts output RGBA tuples absent from source, flat counts output
// pixels differing in a clamped uniform 3x3 source region, hard alpha counts
// changes to a source 0/255 alpha block. No tolerances or floating point.
struct TextureFidelity {
    std::uint64_t error_numerator = 0, error_denominator = 1;
    std::uint64_t max_error_numerator = 0, new_palette_pixels = 0;
    std::uint64_t flat_region_changes = 0, hard_alpha_changes = 0;
    bool aspect_preserved = false;
    bool accepted() const {
        return aspect_preserved && !error_numerator && !new_palette_pixels &&
               !flat_region_changes && !hard_alpha_changes;
    }
};
TextureFidelity measureTextureFidelity(const Texture32 &source, const Texture32 &result, unsigned factor);
// Ranking measurements select nearest; factor ties prefer the least allocation.
constexpr TextureFilter faithfulTextureFilter = TextureFilter::Nearest;
constexpr unsigned faithfulTextureFactor = 2;
Texture32 upscaleTexture(const Texture32 &input, unsigned factor, TextureFilter filter);
}
