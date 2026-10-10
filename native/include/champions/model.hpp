#pragma once
#include "champions/archive.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace champions {
struct ModelCorner {
    std::array<float, 3> position; // Model-space rest pose, Z up; file units /16.
    std::array<float, 3> normal; // Signed source normal /127; not renormalized.
    std::array<float, 2> uv_texels; // Top-left texel coordinates, not normalized.
};
struct ModelMesh {
    std::uint32_t index;
    bool visible_by_default;
    std::uint8_t joint;
    std::vector<ModelCorner> corners; // Consecutive triples, winding follows normals.
};
struct RtaModel {
    std::uint16_t flags;
    std::uint64_t hidden_mask;
    std::uint64_t part_mask; // Affects a parent body, never this model's visibility.
    std::vector<ModelMesh> meshes;
};
// Bounded RTA lump .vif rest-pose geometry, program0x42 only. No animation,
// original lighting/blending, shell, reflection, effects, world or Norrath claim.
// Validates weights/tables but does not apply joint transforms. Rejects unsupported
// forms with FormatError. Limits:16MiB input,64 meshes,32768 packets,200000 triangles.
RtaModel decodeRtaModel(Bytes bytes);
} // namespace champions
