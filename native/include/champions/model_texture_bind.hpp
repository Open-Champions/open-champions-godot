#pragma once
// A model's texture and per-corner texture coordinates for display.
// Association (research docs/formats/model-texture.md): a model file holds no
// texture name; creature class definitions name `x.vif` then `x.tex`, and
// every .vif on the disc has a .tex of the same base name in the same lump.
// UVs are GS texels x 16 per strip position, divided by 16 and the texture
// size. Blending follows the model header +0x18 and each packet's GIF PRIM.
#include "champions/model.hpp"
#include "champions/texture.hpp"
#include <array>
#include <string>
#include <vector>

namespace champions::model_texture {
enum class Blend { Off, On, Mixed, Unstated };
struct MeshUv {
  std::uint32_t index = 0;
  bool visible_by_default = false;
  std::vector<std::array<double, 2>> uv; // corner order, 0..1
  Blend blend = Blend::Unstated;         // from each packet's first GIF tag
};
struct Bound {
  Texture32 texture;
  std::uint64_t alpha1 = 0; // header +0x18: 1 = src + (dst - src) x As
  std::vector<MeshUv> meshes;
};
// "x.vif" -> "x.tex" (same base name, same lump); empty if not a .vif name.
std::string companion_texture_name(const std::string &vif_name);
Bound bind(Bytes vif, Bytes tex);
} // namespace champions::model_texture
