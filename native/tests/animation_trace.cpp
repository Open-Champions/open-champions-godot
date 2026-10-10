#include "champions/animation.hpp"
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
    return {{"channels", channels, {0, 1.5, 4, 4.25, 8, 16, 5, 0}},
            {"mask63", masked, {0, 255, 65536.25, 4294967295.0}},
            {"zero_quaternion", zero, {0, 1}}, {"tiny_rotation", tiny, {1e-200}}};
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
        require(j.rotation_valid == (c.name != "zero_quaternion"), "rotation validity differs");
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
    } else {
        for (double p : pose.joints[0].position) near(p,0);
        for (double q : pose.joints[0].quaternion_wxyz) near(q,0);
    }
}
template<std::size_t N> void array(std::ostream& out, const std::array<double,N>& values) {
    out << '[';
    for (std::size_t i=0; i<N; ++i) { if(i) out << ','; out << values[i]; }
    out << ']';
}
std::string trace(const std::string& platform, const std::string& commit) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << std::boolalpha;
    out << "{\"schema\":1,\"fixture\":\"rta-pose-authored-v1\",\"platform\":\"" << platform
        << "\",\"source_commit\":\"" << commit << "\",\"records\":[";
    bool first=true;
    for (const auto& c : fixtures()) {
        for (std::size_t sample=0; sample<c.ticks.size(); ++sample) {
            const auto pose = champions::sampleRtaAnimationPose(c.clip,c.ticks[sample]);
            verify(c,sample,pose); // The producer rejects common wrong results, too.
            if(!first) out << ',';
            first=false;
            out << "{\"case\":\"" << c.name << "\",\"sample\":" << sample << ",\"tick\":" << pose.tick << ",\"joints\":[";
            for (std::size_t i=0; i<pose.joints.size(); ++i) {
                if(i) out << ',';
                const auto& j=pose.joints[i];
                out << "{\"joint\":" << i << ",\"rotation_valid\":" << j.rotation_valid
                    << ",\"drives_joint\":" << j.drives_joint << ",\"position\":";
                array(out,j.position); out << ",\"quaternion_wxyz\":"; array(out,j.quaternion_wxyz); out << '}';
            }
            out << "]}";
        }
    }
    out << "]}\n";
    return out.str();
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc==2 && std::string(argv[1])=="--self-test") {
            (void)trace("linux-x64",std::string(40,'0'));
            std::cout << "PASS: authored pose trace expectations, 15 samples\n";
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
