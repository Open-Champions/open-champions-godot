#pragma once
#include "champions/animation.hpp"
#include "champions/model.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace champions {
struct RtaSkinnedMeshPositions {
    std::uint32_t index;
    std::vector<std::array<double,3>> positions; // Original corner order, Z-up model space.
};
struct RtaSkinnedPositions {
    double tick = 0;
    std::vector<RtaSkinnedMeshPositions> meshes; // Original mesh order, including empty/hidden meshes.
};
// Direct position skinning only; caller must establish model/clip compatibility.
// Rejects attachment models and any partial-body transform set. Validates finite
// matrices, referenced validity (even zero-weight influences), weights and unused
// influence tails. Unused invalid transforms must have zero matrices. Limits:
// 64 meshes/transforms, unique mesh indices0..63,200000 triangle triples total.
// Uses ordered binary64 sum(weight/255 * (skin.R * position + skin.t)); rejects
// nonfinite arithmetic. Compile without fast-math or FP contraction. Normal/UV,
// material and visibility metadata are not consumed or deformed. No pairing,
// original-renderer equivalence, playback or Norrath compatibility is established.
RtaSkinnedPositions skinRtaModelPositions(const RtaModel& model,
                                        const RtaAnimationTransforms& transforms);
struct RtaSkinnedMeshNormals {
    std::uint32_t index;
    std::vector<std::array<double,3>> normals; // Same corner order as positions.
};
struct RtaSkinnedNormals {
    std::vector<RtaSkinnedMeshNormals> meshes;
};
// Rotates each stored normal (decoder s8/127) by the blended skin 3x3:
// n' = sum (weight/255 * R_j * n). Same validation and limits as positions,
// and stored normals must be finite. Translation is not applied and the
// result is not renormalized; a zero sum stays zero. The environment pass
// normalizes in camera space (render-pipeline section 9).
RtaSkinnedNormals skinRtaModelNormals(const RtaModel& model,
                                      const RtaAnimationTransforms& transforms);
} // namespace champions
