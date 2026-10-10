#pragma once
#include "champions/archive.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace champions {
struct ModelSkinInfluences {
    std::array<std::uint8_t, 4> joints{}; // Joint indices0..63, not VU matrix addresses.
    std::array<std::uint8_t, 4> weights{}; // Exact byte weights; active entries sum to255.
    std::uint8_t count = 0; //1..4, including any explicitly stored zero weights.
};
struct ModelCorner {
    std::array<float, 3> position; // Model-space rest pose, Z up; file units /16.
    std::array<float, 3> normal; // Signed source normal /127; not renormalized.
    std::array<float, 2> uv_texels; // Top-left texel coordinates, not normalized.
    ModelSkinInfluences skin; // Owned source-vertex influences, copied through strip seams.
};
struct ModelMesh {
    std::uint32_t index;
    bool visible_by_default;
    std::uint8_t joint;
    std::vector<ModelCorner> corners; // Consecutive triples, winding follows normals.
};
struct ModelVariant {
    std::uint64_t show;
    std::uint64_t hide;
};
struct RtaModel {
    std::uint16_t flags;
    std::uint64_t hidden_mask;
    std::uint64_t part_mask; // Affects a parent body, never this model's visibility.
    std::vector<ModelMesh> meshes;
    bool has_variants = false;
    std::array<ModelVariant, 32> variants{};
};
// Bounded RTA lump .vif rest-pose geometry, program0x42 only. No animation,
// original lighting/blending, shell, reflection, effects, world or Norrath claim.
// Preserves weights and validates tables; does not apply joint transforms. Rejects unsupported
// forms with FormatError. Limits:16MiB input,64 meshes,32768 packets,200000 triangles.
RtaModel decodeRtaModel(Bytes bytes);
// Default (-1) or exactly one variant bit (0..31). Hide wins a show/hide
// conflict. All 64 mask bits are preserved; part_mask is never applied here.
// Throws FormatError for invalid selection or a variant without a table.
std::uint64_t modelVisibilityMask(const RtaModel& model, int selection = -1);
} // namespace champions
