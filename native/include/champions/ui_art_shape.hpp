#pragma once
#include "champions/texture.hpp"
#include <optional>
namespace champions {
struct UiArtCrop { std::uint32_t x, y, width, height; };
// Explicit RTA research _shape operation on decoded RGBA8; every byte preserved.
// Input dimensions 1..1024, exact RGBA length; rows 0..height (0=no weave).
// Woven output height=2*rows; odd row uses min(rows+k,height-1).
// Optional nonempty crop must be wholly inside the resulting page. No inference.
Texture32 shapeRtaUiArt(const Texture32 &page, std::uint32_t field_rows,
                      std::optional<UiArtCrop> crop = std::nullopt);
}
