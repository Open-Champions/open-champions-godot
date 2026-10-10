#pragma once
#include "champions/archive.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace champions {
struct ModelSkinInfluences {
  std::array<std::uint8_t, 4>
      joints{}; // Joint indices0..63, not VU matrix addresses.
  std::array<std::uint8_t, 4>
      weights{};          // Exact byte weights; active entries sum to255.
  std::uint8_t count = 0; // 1..4, including any explicitly stored zero weights.
};
struct ModelCorner {
  std::array<float, 3> position; // Model-space rest pose, Z up; file units /16.
  std::array<float, 3> normal;   // Signed source normal /127; not renormalized.
  std::array<float, 2> uv_texels; // Top-left texel coordinates, not normalized.
  ModelSkinInfluences
      skin; // Owned source-vertex influences, copied through strip seams.
  std::array<std::int16_t, 3> position_raw{};
  std::array<std::int8_t, 3> normal_raw{};
  std::array<std::uint16_t, 2> uv_raw{};
  // Renderer shell displacement uses these signed values /32768, not /127.
  std::optional<std::array<std::int8_t, 3>> shell_normal_raw{};
};
struct ModelPacket {
  std::uint8_t program = 0, stride = 0;
  std::uint16_t vertex_count = 0, strip_count = 0;
  std::size_t first_corner = 0, corner_count = 0;
  // Exact low/high GIF words; first and second pass remain distinct metadata.
  std::vector<std::array<std::uint64_t, 2>> gif_tags{};
};
struct ModelMesh {
  std::uint32_t index;
  bool visible_by_default;
  std::uint8_t joint;
  std::vector<ModelCorner>
      corners; // Consecutive triples, winding follows normals.
  std::vector<ModelPacket> packets{};
  std::array<std::int8_t, 6> bounds_raw{};
  std::uint16_t declared_triangles = 0;
  std::array<std::array<std::int8_t, 3>, 8> hull_points{};
};
struct ModelVariant {
  std::uint64_t show;
  std::uint64_t hide;
};
struct ModelFacingPoint {
  std::array<std::uint8_t, 16> raw{};
  std::array<std::int16_t, 3>
      position_raw{}; // /16, no evaluation or transform.
  std::array<std::int8_t, 3>
      normal_raw{}; // /128, not ordinary mesh normal /127.
  std::int8_t joint = 0;
};
struct ModelAttachPoint {
  std::int32_t joint = 0;
  std::array<std::uint32_t, 12>
      matrix_raw{}; // Rotation9 then origin3, raw f32.
};
struct ModelSamplePoint {
  std::array<std::int16_t, 3> position_raw{};
  std::uint16_t joint_size_raw = 0; // Preserve both bytes; low byte is joint*4.
};
struct RtaModel {
  std::uint16_t flags;
  std::uint64_t hidden_mask;
  std::uint64_t
      part_mask; // Affects a parent body, never this model's visibility.
  std::vector<ModelMesh> meshes;
  bool has_variants = false;
  std::array<ModelVariant, 32> variants{};
  std::int8_t attachment_slot =
      -1; // Header0x13: parent attachment slot0..15, or -1.
  std::uint64_t alpha1 = 0;
  std::uint8_t fix2 = 0;
  std::vector<ModelFacingPoint> facing_points{};
  std::optional<std::array<std::uint8_t, 16>> facing_terminator_raw{};
  std::vector<ModelAttachPoint> attach_points{};
  std::optional<std::array<std::uint32_t, 6>> effect_segment_raw{};
  std::vector<ModelSamplePoint> sample_points{};
  // Exact bounded header+packet bytes, retaining unexecuted sample VIF
  // metadata.
  std::vector<std::uint8_t> sample_block_raw{};
};
// Bounded RTA lump .vif rest-pose geometry, programs0x42 and0x44. Reflection
// tags, shell normals and ancillary tables are metadata, not rendered effects.
// No animation, original lighting/blending, world0x46 or Norrath claim.
// Preserves weights and validates tables; does not apply joint transforms.
// Rejects unsupported forms with FormatError. Limits:16MiB input,64
// meshes,32768 packets,200000 triangles.
RtaModel decodeRtaModel(Bytes bytes);
// Default (-1) or exactly one variant bit (0..31). Hide wins a show/hide
// conflict. All 64 mask bits are preserved; part_mask is never applied here.
// Throws FormatError for invalid selection or a variant without a table.
std::uint64_t modelVisibilityMask(const RtaModel &model, int selection = -1);
} // namespace champions
