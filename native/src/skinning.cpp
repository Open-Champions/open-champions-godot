#include "champions/skinning.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

// Original implementation of the documented direct model-space influence rule:
// champions-return-to-arms-re45668f7 docs/formats/model-texture.md, weights/joints
// (VU consumer0x3e7e38); parent-attached parts require a different transform path.
namespace champions {
namespace {
void need(bool value,const char* message) {if(!value)throw FormatError(message);}
void validate(const RtaModel& model,const RtaAnimationTransforms& transforms) {
    need(model.attachment_slot==-1,"direct skinning does not support parent attachment models");
    need(model.meshes.size()<=64 && transforms.joints.size()<=64,"skinning supports at most64 meshes and transforms");
    need(std::isfinite(transforms.tick) && transforms.tick>=0,"skinning tick must be finite and nonnegative");
    for(const auto& joint:transforms.joints) {
        need(joint.drives_joint,"direct skinning requires a complete pose, not partial-body transforms");
        for(const auto* matrix:{&joint.model,&joint.skin})for(double value:*matrix) {
            need(std::isfinite(value),"skinning transform contains nonfinite values");
            need(joint.valid || value==0,"invalid unused skinning transform must have zero matrices");
        }
    }
    std::array<bool,64> indices{};
    std::size_t corners=0;
    for(const auto& mesh:model.meshes) {
        need(mesh.index<64 && !indices[mesh.index],"skinning mesh indices must be unique and below64");
        indices[mesh.index]=true;
        need(mesh.corners.size()%3==0 && mesh.corners.size()<=600000-corners,"skinning triangle output budget or triple count is invalid");
        corners+=mesh.corners.size();
        for(const auto& corner:mesh.corners) {
            for(auto value:corner.position)need(std::isfinite(value),"skinning source position must be finite");
            const auto& skin=corner.skin;
            need(skin.count>=1 && skin.count<=4,"skinning influence count must be1..4");
            unsigned total=0;
            for(unsigned i=0;i<4;++i) {
                if(i<skin.count) {
                    need(skin.joints[i]<transforms.joints.size(),"skinning influence joint exceeds transform count");
                    need(transforms.joints[skin.joints[i]].valid,"skinning influence references invalid transform");
                    total+=skin.weights[i];
                } else need(skin.joints[i]==0 && skin.weights[i]==0,"unused skinning influence slots must be zero");
            }
            need(total==255,"skinning byte weights must sum to255");
        }
    }
}
} // namespace
RtaSkinnedPositions skinRtaModelPositions(const RtaModel& model,const RtaAnimationTransforms& transforms) {
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits==53 &&
                  std::numeric_limits<double>::max_exponent==1024,"skinning requires binary64 double");
    validate(model,transforms);
    RtaSkinnedPositions result{};result.tick=transforms.tick;result.meshes.reserve(model.meshes.size());
    for(const auto& mesh:model.meshes) {
        RtaSkinnedMeshPositions output{mesh.index,{}};output.positions.reserve(mesh.corners.size());
        for(const auto& corner:mesh.corners) {
            std::array<double,3> position{};
            for(unsigned influence=0;influence<corner.skin.count;++influence) {
                const auto& matrix=transforms.joints[corner.skin.joints[influence]].skin;
                const double weight=static_cast<double>(corner.skin.weights[influence])/255.0;
                for(unsigned row=0;row<3;++row) {
                    double transformed=0;
                    for(unsigned axis=0;axis<3;++axis) {
                        const double product=matrix[row*4+axis]*static_cast<double>(corner.position[axis]);
                        transformed=transformed+product;
                    }
                    transformed=transformed+matrix[row*4+3];
                    need(std::isfinite(transformed),"skinning matrix arithmetic is nonfinite");
                    const double contribution=weight*transformed;
                    position[row]=position[row]+contribution;
                    need(std::isfinite(position[row]),"skinning weighted sum is nonfinite");
                }
            }
            output.positions.push_back(position);
        }
        result.meshes.push_back(std::move(output));
    }
    return result;
}
} // namespace champions
