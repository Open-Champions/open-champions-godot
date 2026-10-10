#include "champions/animation.hpp"
#include "champions/skinning.hpp"
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using champions::AnimationKeyChannel;
using champions::RtaAnimationKeys;
using champions::RtaAnimationPose;
struct Case { std::string name; RtaAnimationKeys clip; std::vector<double> ticks; };
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
RtaAnimationKeys base(std::size_t joints, std::uint32_t duration) {
    RtaAnimationKeys a;
    a.skeleton.duration_ticks = duration;
    a.skeleton.joint_mask = 0;
    a.skeleton.depths.assign(joints, 0);
    a.skeleton.parents.assign(joints, -1);
    a.skeleton.bind_offsets.assign(joints, {64, -128, 32}); // Must not be added to local pose.
    a.initial.resize(joints);
    for (auto& value : a.initial) {
        value.position_width = 16;
        value.rotation_width = 16;
        value.quaternion_wxyz_raw = {4096, 0, 0, 0};
    }
    return a;
}
void key(RtaAnimationKeys& a, std::uint8_t delta, std::uint8_t joint,
         AnimationKeyChannel channel, std::array<std::int16_t, 4> values) {
    champions::RtaAnimationKey k;
    k.tick = (a.keys.empty() ? 0 : a.keys.back().tick) + delta;
    k.delta_ticks = delta; k.joint = joint; k.channel = channel; k.width = 16; k.values_raw = values;
    a.keys.push_back(k);
}
std::vector<Case> fixtures() {
    auto channels = base(3, 16);
    channels.skeleton.depths = {0, 1, 1}; channels.skeleton.parents = {-1, 0, 0};
    channels.skeleton.joint_mask = std::uint64_t(1) << 1;
    channels.initial[0].position_raw = {64, -128, 32};
    channels.initial[1].position_raw = {-64, 64, 0};
    channels.initial[1].quaternion_wxyz_raw = {2896, 2896, 0, 0};
    channels.initial[2].quaternion_wxyz_raw = {4096, 1024, -512, 256};
    key(channels, 0, 0, AnimationKeyChannel::Position, {256, -128, 64, 0});
    key(channels, 4, 0, AnimationKeyChannel::Position, {-64, 512, -256, 0});
    key(channels, 0, 0, AnimationKeyChannel::Position, {128, 64, 0, 0});
    key(channels, 4, 0, AnimationKeyChannel::Rotation, {0, 4096, 0, 0});
    key(channels, 8, 1, AnimationKeyChannel::Position, {16, -16, 32, 0});
    auto masked = base(64, std::numeric_limits<std::uint32_t>::max());
    masked.skeleton.joint_mask = std::uint64_t(1) << 63;
    masked.initial[63].position_raw = {-32768, 32767, -1};
    masked.initial[63].quaternion_wxyz_raw = {32767, -32768, 1, -1};
    key(masked, 0, 63, AnimationKeyChannel::Position, {32767, -32768, 12345, 0});
    key(masked, 255, 63, AnimationKeyChannel::Rotation, {32767, -32768, 1, -1});
    auto zero = base(1, 1); zero.initial[0].quaternion_wxyz_raw = {0, 0, 0, 0};
    auto tiny = base(1, 1); tiny.initial[0].quaternion_wxyz_raw = {0, 0, 0, 0};
    key(tiny, 0, 0, AnimationKeyChannel::Rotation, {1, 0, 0, 0});
    auto hierarchy = base(3, 0);
    hierarchy.skeleton.depths = {0, 1, 0}; hierarchy.skeleton.parents = {-1, 0, -1};
    hierarchy.skeleton.joint_mask = 1; // A masked parent still composes into its child.
    hierarchy.skeleton.bind_offsets = {{{64,-128,32}}, {{256,320,384}}, {{-64,128,192}}};
    hierarchy.initial[0].position_raw = {64,128,192};
    hierarchy.initial[0].quaternion_wxyz_raw = {2896,0,0,2896}; // +90 degrees Z.
    hierarchy.initial[1].position_raw = {128,0,64};
    hierarchy.initial[1].quaternion_wxyz_raw = {2896,2896,0,0}; // +90 degrees X.
    hierarchy.initial[2].position_raw = {-128,64,-64};
    auto invalid = base(4, 0);
    invalid.skeleton.depths = {0,1,2,0}; invalid.skeleton.parents = {-1,0,1,-1};
    invalid.skeleton.joint_mask = std::uint64_t(1) << 1;
    invalid.initial[0].quaternion_wxyz_raw = {0,0,0,0};
    invalid.initial[1].position_raw = {64,0,0};
    invalid.initial[2].position_raw = {0,64,0};
    invalid.initial[3].position_raw = {192,-128,64};
    return {{"channels", channels, {0, 1.5, 4, 4.25, 8, 16, 5, 0}},
            {"mask63", masked, {0, 255, 65536.25, 4294967295.0}},
            {"zero_quaternion", zero, {0, 1}}, {"tiny_rotation", tiny, {1e-200}},
            {"rotated_hierarchy", hierarchy, {0}}, {"invalid_ancestor", invalid, {0}}};
}
void near(double value, double expected) {
    require(std::isfinite(value) && std::abs(value - expected) <= 1e-12 + 1e-12 * std::abs(expected),
            "authored pose expectation failed");
}
void verify(const Case& c, std::size_t sample, const RtaAnimationPose& pose) {
    require(pose.tick == c.ticks.at(sample) && pose.joints.size() == c.clip.initial.size(), "pose coverage differs");
    for (std::size_t i = 0; i < pose.joints.size(); ++i) {
        const auto& j = pose.joints[i];
        require(j.drives_joint == (((c.clip.skeleton.joint_mask >> i) & 1U) == 0), "mask policy differs");
        require(j.rotation_valid == (c.name != "zero_quaternion" && !(c.name == "invalid_ancestor" && i == 0)), "rotation validity differs");
        for (double p : j.position) require(std::isfinite(p), "nonfinite position");
        double norm = 0;
        for (double q : j.quaternion_wxyz) { require(std::isfinite(q), "nonfinite quaternion"); norm += q*q; }
        near(norm, j.rotation_valid ? 1 : 0);
    }
    if (c.name == "channels") {
        // Independently calculated positions anchor order at duplicate tick4,
        // fractional sampling, past-key integration, and backward/repeated calls.
        const std::array<std::array<double, 3>, 8> expected{{
            {{1,-2,0.5}}, {{2.5,-2.75,0.875}}, {{5,-4,1.5}}, {{5.125,-3.9375,1.5}},
            {{7,-3,1.5}}, {{11,-1,1.5}}, {{5.5,-3.75,1.5}}, {{1,-2,0.5}}}};
        for (std::size_t axis=0; axis<3; ++axis) near(pose.joints[0].position[axis], expected[sample][axis]);
        near(pose.joints[1].position[0], -1); near(pose.joints[1].position[1], 1);
        near(pose.joints[1].position[2], 0);
        const double x = sample == 5 ? 0.25 : 0.0;
        near(pose.joints[0].quaternion_wxyz[0], 1.0/std::sqrt(1+x*x));
        near(pose.joints[0].quaternion_wxyz[1], x/std::sqrt(1+x*x));
    } else if (c.name == "mask63") {
        const std::array<double,3> initial{{-512, 32767.0/64.0, -1.0/64.0}};
        const std::array<double,3> velocity{{32767.0/256.0,-128,12345.0/256.0}};
        for (std::size_t axis=0; axis<3; ++axis)
            near(pose.joints[63].position[axis], initial[axis] + velocity[axis]*pose.tick);
        for (std::size_t i=0; i<63; ++i) {
            for (double p : pose.joints[i].position) near(p,0);
            near(pose.joints[i].quaternion_wxyz[0],1);
        }
    } else if (c.name == "tiny_rotation") {
        for (double p : pose.joints[0].position) near(p,0);
        near(pose.joints[0].quaternion_wxyz[0],1);
        for (std::size_t i=1; i<4; ++i) near(pose.joints[0].quaternion_wxyz[i],0);
    } else if (c.name == "zero_quaternion") {
        for (double p : pose.joints[0].position) near(p,0);
        for (double q : pose.joints[0].quaternion_wxyz) near(q,0);
    }
}
void verifyTransforms(const Case& c, const champions::RtaAnimationTransforms& transforms) {
    require(transforms.joints.size() == c.clip.initial.size(), "transform joint coverage differs");
    for (std::size_t i=0; i<transforms.joints.size(); ++i) {
        const auto& j=transforms.joints[i];
        const bool valid = c.name != "zero_quaternion" && !(c.name == "invalid_ancestor" && i < 3);
        require(j.valid == valid, "transform validity or ancestor propagation differs");
        require(j.drives_joint == (((c.clip.skeleton.joint_mask >> i) & 1U) == 0), "transform drive mask differs");
        for (double v : j.model) { require(std::isfinite(v), "nonfinite model matrix"); if(!valid) near(v,0); }
        for (double v : j.skin) { require(std::isfinite(v), "nonfinite skin matrix"); if(!valid) near(v,0); }
    }
    if(c.name == "rotated_hierarchy") {
        // Explicit noncommuting Z(parent)*X(child) matrices, column vectors.
        // Bind values already carry their inverse-bind sign: APPLY, do not negate.
        const std::array<std::array<double,12>,3> models{{
            {{0,-1,0,1, 1,0,0,2, 0,0,1,3}},
            {{0,0,1,1, 1,0,0,4, 0,1,0,4}},
            {{1,0,0,-2, 0,1,0,1, 0,0,1,-1}}}};
        const std::array<std::array<double,12>,3> skins{{
            {{0,-1,0,3, 1,0,0,3, 0,0,1,3.5}},
            {{0,0,1,7, 1,0,0,8, 0,1,0,9}},
            {{1,0,0,-3, 0,1,0,3, 0,0,1,2}}}};
        for(std::size_t i=0; i<3; ++i) for(std::size_t k=0; k<12; ++k) {
            near(transforms.joints[i].model[k], models[i][k]);
            near(transforms.joints[i].skin[k], skins[i][k]);
        }
    } else if(c.name == "invalid_ancestor") {
        const std::array<double,12> model{{1,0,0,3, 0,1,0,-2, 0,0,1,1}};
        const std::array<double,12> skin{{1,0,0,4, 0,1,0,-4, 0,0,1,1.5}};
        for(std::size_t k=0; k<12; ++k) {
            near(transforms.joints[3].model[k],model[k]);
            near(transforms.joints[3].skin[k],skin[k]);
        }
    }
}
template<std::size_t N> void array(std::ostream& out, const std::array<double,N>& values) {
    out << '[';
    for (std::size_t i=0; i<N; ++i) { if(i) out << ','; out << values[i]; }
    out << ']';
}
struct SkinCase {
    const char* name;
    std::uint32_t mesh_index;
    std::uint8_t count;
    std::array<std::uint8_t,4> joints, weights;
    std::vector<std::array<double,3>> expected;
};
std::vector<SkinCase> skinFixtures() {
    // Expected positions were calculated from each rotated/translated point,
    // then integer byte-weight numerators /255. No runtime skinning helper here.
    return {
        {"rigid",0,1,{{0,0,0,0}},{{255,0,0,0}},
            {{{1,4,6.5}},{{3,1,4}},{{4,3,7.5}}}},
        {"weighted2",7,2,{{0,1,0,0}},{{64,191,0,0}},
            {{{64.0/255,447.0/255,1180.0/255}},{{-381.0/255,732.5/255,638.0/255}},{{65.0/255,192.0/255,671.0/255}}}},
        {"weighted3",2,3,{{0,1,2,0}},{{17,83,155,0}},
            {{{947.0/255,151.0/255,1062.5/255}},{{267.0/255,-2.5/255,466.5/255}},{{760.0/255,-414.0/255,985.5/255}}}},
        {"weighted4",9,4,{{0,1,2,63}},{{1,64,127,63}},
            {{{448.0/255,257.0/255,455.5/255}},{{66.0/255,34.0/255,165.0/255}},{{323.0/255,-378.0/255,328.5/255}}}},
        {"joint63",63,1,{{63,0,0,0}},{{255,0,0,0}},
            {{{-5,3,-5}},{{-2,1,-2.5}},{{-4,0,-6}}}},
        {"weighted_zero",5,3,{{0,1,2,0}},{{128,0,127,0}},
            {{{890.0/255,512.0/255,1340.0/255}},{{3,-126.0/255,702.5/255}},{{1147.0/255,3.0/255,1595.0/255}}}},
        {"seam",6,2,{{0,1,0,0}},{{64,191,0,0}},
            {{{64.0/255,447.0/255,1180.0/255}},{{-381.0/255,732.5/255,638.0/255}},{{65.0/255,192.0/255,671.0/255}},
             {{64.0/255,447.0/255,1180.0/255}},{{65.0/255,192.0/255,671.0/255}},{{-381.0/255,732.5/255,638.0/255}}}}
    };
}
void skinTrace(std::ostream& out) {
    champions::RtaAnimationTransforms transforms;
    transforms.tick=12.5; transforms.joints.resize(64);
    const std::array<double,12> identity{{1,0,0,0,0,1,0,0,0,0,1,0}};
    for(auto& joint:transforms.joints) {
        joint.model=identity; joint.skin=identity; joint.valid=true; joint.drives_joint=true;
    }
    transforms.joints[0].skin={{0,-1,0,3,1,0,0,3,0,0,1,3.5}}; // Rz90 + translation.
    transforms.joints[1].skin={{1,0,0,-1,0,0,-1,4,0,1,0,2}}; // Rx90 + translation.
    transforms.joints[2].skin={{1,0,0,5,0,1,0,-2,0,0,1,1}};
    transforms.joints[63].skin={{-1,0,0,-4,0,1,0,1,0,0,-1,-2}};
    const std::array<std::array<float,3>,3> positions{{{{1,2,3}},{{-2,0,0.5f}},{{0,-1,4}}}};
    const std::array<std::size_t,6> seam_order{{0,1,2,0,2,1}};
    bool first=true;
    for(const auto& c:skinFixtures()) {
        champions::RtaModel model{};
        champions::ModelMesh mesh{}; mesh.index=c.mesh_index; mesh.visible_by_default=true;
        mesh.joint=c.joints[0];
        for(std::size_t i=0;i<c.expected.size();++i) {
            champions::ModelCorner corner{};
            corner.position=positions[seam_order[i]];
            corner.normal={{0,0,1}};
            corner.uv_texels={{static_cast<float>(i*7),static_cast<float>(i*3)}};
            corner.skin.count=c.count; corner.skin.joints=c.joints; corner.skin.weights=c.weights;
            mesh.corners.push_back(corner);
        }
        model.meshes.push_back(mesh);
        const auto output=champions::skinRtaModelPositions(model,transforms);
        require(output.tick==12.5 && output.meshes.size()==1, "skinning output time/mesh coverage differs");
        const auto& result=output.meshes[0];
        require(result.index==c.mesh_index && result.positions.size()==c.expected.size(), "skinning output corner coverage differs");
        for(std::size_t i=0;i<c.expected.size();++i) for(std::size_t axis=0;axis<3;++axis)
            near(result.positions[i][axis],c.expected[i][axis]);
        if(!first)out<<',';
        first=false;
        out<<"{\"case\":\""<<c.name<<"\",\"mesh_index\":"<<result.index<<",\"tick\":"<<output.tick<<",\"positions\":[";
        for(std::size_t i=0;i<result.positions.size();++i) {if(i)out<<',';array(out,result.positions[i]);}
        out<<"]}";
    }
}
std::string trace(const std::string& platform, const std::string& commit) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << std::boolalpha;
    out << "{\"schema\":3,\"fixture\":\"rta-skinning-authored-v3\",\"platform\":\"" << platform
        << "\",\"source_commit\":\"" << commit << "\",\"records\":[";
    bool first=true;
    for (const auto& c : fixtures()) {
        for (std::size_t sample=0; sample<c.ticks.size(); ++sample) {
            const auto pose = champions::sampleRtaAnimationPose(c.clip,c.ticks[sample]);
            verify(c,sample,pose); // The producer rejects common wrong results, too.
            const auto transforms = champions::buildRtaAnimationTransforms(c.clip.skeleton,pose);
            require(transforms.tick == pose.tick, "transform sample time differs");
            verifyTransforms(c,transforms);
            if(!first) out << ',';
            first=false;
            out << "{\"case\":\"" << c.name << "\",\"sample\":" << sample << ",\"tick\":" << pose.tick << ",\"joints\":[";
            for (std::size_t i=0; i<pose.joints.size(); ++i) {
                if(i) out << ',';
                const auto& j=pose.joints[i];
                out << "{\"joint\":" << i << ",\"rotation_valid\":" << j.rotation_valid
                    << ",\"drives_joint\":" << j.drives_joint << ",\"position\":";
                array(out,j.position); out << ",\"quaternion_wxyz\":"; array(out,j.quaternion_wxyz);
                const auto& t=transforms.joints[i];
                out << ",\"transform_valid\":" << t.valid << ",\"transform_drives_joint\":" << t.drives_joint
                    << ",\"model_matrix\":"; array(out,t.model);
                out << ",\"skin_matrix\":"; array(out,t.skin); out << '}';
            }
            out << "]}";
        }
    }
    out << "],\"skinning_records\":[";
    skinTrace(out);
    out << "]}\n";
    return out.str();
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc==2 && std::string(argv[1])=="--self-test") {
            (void)trace("linux-x64",std::string(40,'0'));
            std::cout << "PASS: authored pose, transform and skinning trace expectations: 17 pose samples, 7 skinning cases\n";
            return 0;
        }
        require(argc==7 && std::string(argv[1])=="--platform" && std::string(argv[3])=="--source-commit" &&
                std::string(argv[5])=="--output", "usage: champions-animation-trace --platform NAME --source-commit SHA1 --output FILE");
        const std::string platform=argv[2],commit=argv[4];
        require(platform=="macos-arm64" || platform=="macos-x64" || platform=="windows-arm64" ||
                platform=="windows-x64" || platform=="linux-x64", "unsupported trace platform");
        require(commit.size()==40 && commit.find_first_not_of("0123456789abcdef")==std::string::npos, "expected full source SHA1");
        const auto content=trace(platform,commit);
        std::ofstream file(argv[6],std::ios::binary|std::ios::trunc);
        require(bool(file),"cannot open trace output"); file << content; file.close();
        require(bool(file),"cannot finish trace output");
        std::cout << "PASS: authored pose trace written for " << platform << '\n';
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
