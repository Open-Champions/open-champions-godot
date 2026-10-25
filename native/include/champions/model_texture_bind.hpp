#pragma once
// A model's texture and per-corner texture coordinates for display.
// Association (research docs/formats/model-texture.md): a model file holds no
// texture name; creature class definitions name `x.vif` then `x.tex`, and
// every .vif on the disc has a .tex of the same base name in the same lump.
// UVs are GS texels x 16 per strip position, divided by 16 and the texture
// size. Blending follows header +0x18 (the GS ALPHA_1 register) and each
// packet's GIF PRIM. Flag 0x10 adds the context-2 environment pass, drawn
// before the base pass (model-texture.md "Passes, blending, shell").
// Evidence pin: research t3/research-textures 1528708.
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
// A FIX above 0x80 is refused (equation 0): the table gives F = FIX / 128 and
// says nothing about F > 1 on the GS, so such a register is not drawn as if
// it were documented.
AlphaMode alpha_mode(std::uint64_t alpha1);

// Largest FIX the ALPHA table is taken to cover (0x80 = 1).
constexpr std::uint8_t kMaxDocumentedFix = 0x80;

// One channel of the documented equation. `as` and `fix` are already divided
// by 128 (0x80 = 1). `blend_enabled` is the packet PRIM ABE bit: clear means
// the source replaces the destination. An undocumented equation returns empty.
std::optional<std::array<double, 3>>
blend_rgb(std::uint32_t equation, double fix, const std::array<double, 3> &src,
          const std::array<double, 3> &dst, double as, bool blend_enabled);

// Model environment UV, per vertex, divided by the environment image size.
// research docs/spec/render-pipeline.md 4.7 (V): program 0x44 writes
// (192 + 64 c.x, 192 + 64 c.y) texels with c = the camera-space unit normal
// x 0.99; the GS interpolates that UV across the triangle. `right` and `down`
// are the unit normal (x right, y down); this function applies the 0.99.
// 0.99 is the figure 4.7 states; the research gives no further digits.
constexpr double kEnvironmentNormalScale = 0.99;
std::array<double, 2> environment_uv(double right, double down, double width,
                                     double height);

struct MeshUv {
  std::uint32_t index = 0;
  bool visible_by_default = false;
  std::vector<std::array<double, 2>> uv; // corner order, 0..1
  Blend blend = Blend::Unstated;         // first GIF tag: context 1, base
  // Second GIF tag: context 2, the environment pass. Unstated when the mesh
  // has no environment pass or the tags disagree.
  Blend environment_blend = Blend::Unstated;
  bool environment_tag = false; // every packet has the second tag
  // Drawn with the environment pass: header flag 0x10 and environment_tag.
  // model-texture.md: ALPHA_2 is written only when flag 0x10 is set, and flag
  // 0x10 is program 0x44, the only program with the second tag.
  bool environment = false;
};
struct Bound {
  Texture32 texture;
  std::uint64_t alpha1 = 0; // header +0x18, unchanged
  std::uint8_t fix2 = 0;    // header +0x20, ALPHA_2 FIX
  bool two_pass = false;    // header flag 0x10
  AlphaMode mode{};
  std::vector<MeshUv> meshes;
  // Tag/flag disagreements, refused rather than drawn: a packet carries the
  // second tag while flag 0x10 is clear (no environment pass is drawn), or
  // flag 0x10 is set and some mesh's packets do not all carry it (that mesh
  // stays on the single pass).
  bool env_tag_without_two_pass = false;
  bool two_pass_mesh_without_env_tag = false;
  // Flag 0x10 with header +0x20 above 0x80: the environment pass is refused.
  bool fix2_above_documented = false;
};
// "x.vif" -> "x.tex" (same base name, same lump); empty if not a .vif name.
std::string companion_texture_name(const std::string &vif_name);
Bound bind(Bytes vif, Bytes tex);
} // namespace champions::model_texture
