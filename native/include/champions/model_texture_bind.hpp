#pragma once
// A model's texture and per-corner texture coordinates for display.
// Association (research docs/formats/model-texture.md): a model file holds no
// texture name; creature class definitions name `x.vif` then `x.tex`, and
// every .vif on the disc has a .tex of the same base name in the same lump.
// UVs are GS texels x 16 per strip position, divided by 16 and the texture
// size. Blending follows header +0x18 (the GS ALPHA_1 register) and each
// packet's GIF PRIM. Flag 0x10 is the second, environment-map pass.
#include "champions/model.hpp"
#include "champions/texture.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace champions::model_texture {
enum class Blend { Off, On, Mixed, Unstated };

// Documented ALPHA equations (research docs/spec/render-pipeline.md section 6
// and docs/formats/level-draw.md section 1). `equation` is the register's low
// word: 1, 2, 9, 0x2a, 0x44, 0x48, 0x64 or 0x68. 0 means the register is not
// one of those rows. For 0x64 and 0x68, `fix` is the FIX byte (0x80 = 1).
struct AlphaMode {
  std::uint32_t equation = 0;
  std::uint8_t fix = 0;
};

// Header +0x18. 0x64 and 0x68 accept a FIX byte in bits 32-39 and nothing above.
AlphaMode alpha_mode(std::uint64_t alpha1);

// One channel of the documented equation. `as` and `fix` are already divided
// by 128 (0x80 = 1). `blend_enabled` is the packet PRIM ABE bit: clear means
// the source replaces the destination. An undocumented equation returns empty.
std::optional<std::array<double, 3>>
blend_rgb(std::uint32_t equation, double fix, const std::array<double, 3> &src,
          const std::array<double, 3> &dst, double as, bool blend_enabled);

// Model environment UV in texels, then divided by the environment image size.
// research docs/spec/render-pipeline.md section 9: (192 + 64 n.x, 192 + 64 n.y)
// / 128 for a 128-texel image. `right` and `down` are the camera-space unit
// normal (x right, y down). Section 4.7's VU cosine carries an extra x0.99;
// the renderer paragraph uses the unit normal, and this function does too.
std::array<double, 2> environment_uv(double right, double down, double width,
                                     double height);

struct MeshUv {
  std::uint32_t index = 0;
  bool visible_by_default = false;
  std::vector<std::array<double, 2>> uv; // corner order, 0..1
  Blend blend = Blend::Unstated;         // first GIF tag: context 1, base
  // Second GIF tag: context 2, the environment pass. Unstated when the mesh
  // has no second tag or the tags disagree.
  Blend environment_blend = Blend::Unstated;
  bool environment = false; // every packet has the second tag
};
struct Bound {
  Texture32 texture;
  std::uint64_t alpha1 = 0; // header +0x18, unchanged
  std::uint8_t fix2 = 0;    // header +0x20, ALPHA_2 FIX
  bool two_pass = false;    // header flag 0x10
  AlphaMode mode{};
  std::vector<MeshUv> meshes;
};
// "x.vif" -> "x.tex" (same base name, same lump); empty if not a .vif name.
std::string companion_texture_name(const std::string &vif_name);
Bound bind(Bytes vif, Bytes tex);
} // namespace champions::model_texture
