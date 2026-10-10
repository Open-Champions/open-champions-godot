#include "champions/animation.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>
#include <cstdint>

// Original implementation from the published RTA format contract:
// Open-Champions/champions-return-to-arms-re 45668f7,
// docs/formats/anim-audio.md sections2.1,2.2,2.3,2.5,2.8.
namespace champions {
namespace {
constexpr std::size_t header_size = 0x70;
constexpr std::size_t max_input = 16U * 1024U * 1024U;
void need(bool condition, const char* message) { if (!condition) throw FormatError(message); }
struct Reader {
    Bytes bytes;
    void range(std::size_t offset, std::size_t count) const {
        need(offset <= bytes.size && count <= bytes.size - offset, "animation metadata range exceeds file");
    }
    std::uint64_t u(std::size_t offset, unsigned count) const {
        range(offset,count);
        std::uint64_t value = 0;
        for (unsigned i = 0; i < count; ++i) value |= std::uint64_t(bytes.data[offset+i]) << (i*8);
        return value;
    }
    std::int16_t s16(std::size_t offset) const {
        const auto raw = static_cast<std::int32_t>(u(offset,2));
        return static_cast<std::int16_t>(raw >= 32768 ? raw - 65536 : raw);
    }
};
struct KeyBits {
    const Reader& reader;
    std::size_t start, bits, position = 0;
    unsigned bit(std::size_t at) const {
        return unsigned((reader.u(start+(at/16)*2,2) >> (15-at%16)) & 1U);
    }
    unsigned get(unsigned width) {
        need(width <= 16 && position <= bits && width <= bits-position, "animation key stream is truncated");
        unsigned value = 0;
        for(unsigned i=0;i<width;++i)value=(value<<1)|bit(position++);
        return value;
    }
    std::int16_t signedField(unsigned width) {
        const auto raw=static_cast<std::int32_t>(get(width));
        const auto sign=std::int32_t(1)<<(width-1);
        return static_cast<std::int16_t>(raw >= sign ? raw-(sign*2) : raw);
    }
    bool terminalPadding() const {
        // Called just after the delta. At most31 trailing bits belong to a
        // canonical terminal; never scan or silently accept an external tail.
        if(((position+31)/32)*32 != bits)return false;
        for(auto at=position;at<bits;++at)if(bit(at)!=0)return false;
        return true;
    }
};
void validatePoseInput(const RtaAnimationKeys& animation) {
    const auto count=animation.skeleton.depths.size();
    need(count<=64 && animation.skeleton.parents.size()==count &&
         animation.skeleton.bind_offsets.size()==count && animation.initial.size()==count,
         "animation pose input has inconsistent joint arrays or exceeds64 joints");
    need(animation.keys.size()<=max_animation_keys, "animation pose key budget exceeded");
    std::array<std::int16_t,64> ancestors{}; ancestors.fill(-1);
    auto width = [](unsigned value) { need(value>=1 && value<=16, "animation pose input has invalid signed width"); };
    auto raw = [](std::int16_t value,unsigned bits) {
        const auto limit=std::int32_t(1)<<(bits-1);
        need(value>=-limit && value<limit, "animation pose raw value does not fit its signed width");
    };
    for(std::size_t joint=0;joint<count;++joint) {
        const auto depth=animation.skeleton.depths[joint];
        need(depth<64 && (joint!=0 || depth==0) &&
             (joint==0 || depth<=unsigned(animation.skeleton.depths[joint-1])+1),
             "animation pose input has invalid skeleton depth transitions");
        const auto parent=depth==0 ? std::int16_t(-1) : ancestors[depth-1];
        need((depth==0 || parent>=0) && animation.skeleton.parents[joint]==parent,
             "animation pose parent does not match its depth hierarchy");
        ancestors[depth]=static_cast<std::int16_t>(joint);
        const auto& initial=animation.initial[joint];
        width(initial.position_width); width(initial.rotation_width);
        for(auto value:initial.position_raw)raw(value,initial.position_width);
        for(auto value:initial.quaternion_wxyz_raw)raw(value,initial.rotation_width);
    }
    std::uint64_t previous=0;
    for(const auto& key:animation.keys) {
        // Both operands of the later addition are bounded before evaluating it.
        need(key.tick<=animation.skeleton.duration_ticks && key.tick>=previous &&
             key.tick-previous==key.delta_ticks, "animation pose key ticks or deltas are inconsistent");
        previous=key.tick;
        need(key.joint<count, "animation pose key joint is outside the skeleton");
        need(key.channel==AnimationKeyChannel::Position || key.channel==AnimationKeyChannel::Rotation,
             "animation pose key has invalid channel");
        width(key.width);
        const unsigned components=key.channel==AnimationKeyChannel::Position ? 3 : 4;
        for(unsigned i=0;i<components;++i)raw(key.values_raw[i],key.width);
        need(components==4 || key.values_raw[3]==0, "animation position key has a nonzero unused component");
    }
}
struct PoseState {
    std::array<std::int64_t,3> position{};
    std::array<std::int64_t,4> rotation{};
    std::array<std::int16_t,3> position_velocity{};
    std::array<std::int16_t,4> rotation_velocity{};
    std::uint64_t position_tick=0, rotation_tick=0;
};
void validateTransformInput(const RtaAnimationSkeleton& skeleton, const RtaAnimationPose& pose) {
    const auto count=skeleton.depths.size();
    need(count<=64 && skeleton.parents.size()==count && skeleton.bind_offsets.size()==count && pose.joints.size()==count,
         "animation transforms require matching joint arrays of0..64 joints");
    need(std::isfinite(pose.tick) && pose.tick>=0 && pose.tick<=skeleton.duration_ticks,
         "animation transform tick must be finite and within duration");
    std::array<std::int16_t,64> ancestors{}; ancestors.fill(-1);
    for(std::size_t joint=0;joint<count;++joint) {
        const auto depth=skeleton.depths[joint];
        need(depth<64 && (joint!=0 || depth==0) && (joint==0 || depth<=unsigned(skeleton.depths[joint-1])+1),
             "animation transforms have invalid depth transitions");
        const auto parent=depth==0 ? std::int16_t(-1) : ancestors[depth-1];
        need((depth==0 || parent>=0) && skeleton.parents[joint]==parent,
             "animation transform parent does not match depth");
        ancestors[depth]=static_cast<std::int16_t>(joint);
        const auto& input=pose.joints[joint];
        need(input.drives_joint==(((skeleton.joint_mask>>joint)&1U)==0), "animation transform pose mask flag disagrees with skeleton");
        for(auto component:input.position)need(std::isfinite(component), "animation transform position must be finite");
        double squared_norm=0;
        for(auto component:input.quaternion_wxyz) {
            need(std::isfinite(component) && std::abs(component)<=1.0+1e-12, "animation transform quaternion must be finite and normalized");
            if(!input.rotation_valid)need(component==0, "invalid animation rotation must have zero quaternion");
            const double square=component*component; squared_norm=squared_norm+square;
        }
        if(input.rotation_valid)need(std::abs(squared_norm-1.0)<=1e-12, "animation transform quaternion is not normalized");
    }
}
} // namespace
RtaAnimationSkeleton readRtaAnimationSkeleton(Bytes bytes) {
    need(bytes.data && bytes.size >= header_size && bytes.size <= max_input,
         "animation metadata requires a file of112 bytes to16MiB");
    const Reader r{bytes};
    const auto count = static_cast<unsigned>(r.u(0,4));
    need(count <= 64, "animation joint count must be0..64");
    const auto keys = static_cast<std::size_t>(r.u(8,4));
    const auto bind = static_cast<std::size_t>(r.u(12,4));
    const auto depth = static_cast<std::size_t>(r.u(16,4));
    need(depth == header_size, "unsupported animation layout: depth list must start at0x70");
    r.range(depth,count);
    need(bind >= depth + count, "animation bind records overlap depth list or header");
    r.range(bind,std::size_t(count)*8);
    need(keys >= bind + std::size_t(count)*8, "animation keys overlap bind records");
    r.range(keys,4); // The key stream itself is deliberately not decoded here.
    const auto names = static_cast<std::size_t>(r.u(0x34,4));
    const auto sounds = static_cast<std::size_t>(r.u(0x38,4));
    for (auto offset : {names,sounds}) if (offset != 0) {
        need(offset >= keys + 4, "animation sound pointer overlaps skeleton or key prefix");
        r.range(offset,4); // Only validate the header pointer, not its target contents.
    }
    if (names && sounds) need(sounds >= names + 4, "animation sound pointers are out of supported order");
    RtaAnimationSkeleton result{};
    result.duration_ticks = static_cast<std::uint32_t>(r.u(4,4));
    result.joint_mask = r.u(0x48,8);
    result.depths.reserve(count); result.parents.reserve(count); result.bind_offsets.reserve(count);
    std::array<std::int16_t,64> ancestors{};
    ancestors.fill(-1);
    for (unsigned joint = 0; joint < count; ++joint) {
        const auto level = static_cast<unsigned>(r.u(depth+joint,1));
        need(level < 64 && (joint != 0 || level == 0), "animation first joint must be a root and depth must be below64");
        need(joint == 0 || level <= unsigned(result.depths.back()) + 1, "animation depth skips a parent level");
        const auto parent = level == 0 ? std::int16_t(-1) : ancestors[level-1];
        need(level == 0 || parent >= 0, "animation joint has no preceding parent");
        ancestors[level] = static_cast<std::int16_t>(joint);
        result.depths.push_back(static_cast<std::uint8_t>(level));
        result.parents.push_back(parent);
        const auto record = bind + std::size_t(joint)*8;
        need(r.u(record+6,2) == 0, "unsupported nonzero animation bind padding");
        result.bind_offsets.push_back({r.s16(record),r.s16(record+2),r.s16(record+4)});
    }
    return result;
}
RtaAnimationKeys decodeRtaAnimationKeys(Bytes bytes) {
    RtaAnimationKeys result{};
    result.skeleton=readRtaAnimationSkeleton(bytes);
    const Reader r{bytes};
    const auto start=static_cast<std::size_t>(r.u(8,4));
    const auto names=static_cast<std::size_t>(r.u(0x34,4));
    const auto sounds=static_cast<std::size_t>(r.u(0x38,4));
    auto end=bytes.size;
    if(names || sounds) {
        need(names && sounds && names >= 0x28, "animation keys require a complete sound block boundary");
        const auto block=names-0x28;
        need(block >= start && block-start >= 4, "animation sound block overlaps key prefix");
        r.range(block,0x28);
        need(r.u(block,4)==0x28 && r.u(block+4,4)==sounds-block,
             "animation sound block relative-offset signature is unsupported");
        end=block;
    }
    need(end >= start && end-start >= 4 && (end-start)%4==0,
         "animation key section must contain complete4-byte stream units");
    KeyBits input{r,start,(end-start)*8};
    const auto joints=result.skeleton.depths.size();
    result.initial.reserve(joints);
    for(std::size_t joint=0;joint<joints;++joint) {
        RtaAnimationInitial initial{};
        initial.position_width=static_cast<std::uint8_t>(input.get(4)+1);
        for(auto& component:initial.position_raw)component=input.signedField(initial.position_width);
        initial.rotation_width=static_cast<std::uint8_t>(input.get(4)+1);
        for(auto& component:initial.quaternion_wxyz_raw)component=input.signedField(initial.rotation_width);
        result.initial.push_back(initial);
    }
    result.keys.reserve(std::min(max_animation_keys,(end-start)/3));
    std::uint64_t tick=0;
    while(true) {
        const auto delta=input.get(8);
        if(delta==255 && input.terminalPadding())break;
        need(tick+delta <= result.skeleton.duration_ticks, "animation key occurs after clip duration or has invalid terminal padding");
        need(result.keys.size()<max_animation_keys, "animation key output budget exceeded");
        RtaAnimationKey key{};
        tick+=delta;
        key.tick=tick; key.delta_ticks=static_cast<std::uint8_t>(delta);
        key.channel=input.get(1) ? AnimationKeyChannel::Position : AnimationKeyChannel::Rotation;
        key.joint=static_cast<std::uint8_t>(input.get(6));
        need(key.joint<joints, "animation key joint is outside the skeleton");
        key.width=static_cast<std::uint8_t>(input.get(4)+1);
        const unsigned components=key.channel==AnimationKeyChannel::Position ? 3 : 4;
        for(unsigned i=0;i<components;++i)key.values_raw[i]=input.signedField(key.width);
        result.keys.push_back(key);
    }
    return result;
}
RtaAnimationPose sampleRtaAnimationPose(const RtaAnimationKeys& animation, double tick) {
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits==53 &&
                  std::numeric_limits<double>::max_exponent==1024, "pose sampling requires binary64 double");
    need(std::isfinite(tick) && tick>=0 && tick<=animation.skeleton.duration_ticks,
         "animation sample tick must be finite and within0..duration");
    validatePoseInput(animation);
    const auto count=animation.initial.size();
    std::array<PoseState,64> states{};
    for(std::size_t joint=0;joint<count;++joint) {
        for(unsigned i=0;i<3;++i)states[joint].position[i]=std::int64_t(animation.initial[joint].position_raw[i])*4;
        for(unsigned i=0;i<4;++i)states[joint].rotation[i]=std::int64_t(animation.initial[joint].quaternion_wxyz_raw[i])*32;
    }
    const auto whole_tick=static_cast<std::uint64_t>(std::floor(tick));
    for(const auto& key:animation.keys) {
        if(key.tick>whole_tick)break;
        auto& state=states[key.joint];
        if(key.channel==AnimationKeyChannel::Position) {
            const auto elapsed=static_cast<std::int64_t>(key.tick-state.position_tick);
            for(unsigned i=0;i<3;++i) {
                state.position[i]+=std::int64_t(state.position_velocity[i])*elapsed;
                state.position_velocity[i]=key.values_raw[i];
            }
            state.position_tick=key.tick;
        } else {
            const auto elapsed=static_cast<std::int64_t>(key.tick-state.rotation_tick);
            for(unsigned i=0;i<4;++i) {
                state.rotation[i]+=std::int64_t(state.rotation_velocity[i])*elapsed;
                state.rotation_velocity[i]=key.values_raw[i];
            }
            state.rotation_tick=key.tick;
        }
    }
    // After validation, all nonnegative segment lengths for a channel sum to
    // <=UINT32_MAX, and |velocity|<=32768. Thus settled magnitudes are below
    // 32768*UINT32_MAX+1048576 (<2^48), safely inside int64 and exact in binary64.
    RtaAnimationPose result{}; result.tick=tick; result.joints.resize(count);
    for(std::size_t joint=0;joint<count;++joint) {
        const auto& state=states[joint]; auto& sampled=result.joints[joint];
        const double position_elapsed=tick-static_cast<double>(state.position_tick);
        for(unsigned i=0;i<3;++i) {
            const double delta=static_cast<double>(state.position_velocity[i])*position_elapsed;
            const double value=static_cast<double>(state.position[i])+delta;
            sampled.position[i]=value/256.0;
        }
        const double rotation_elapsed=tick-static_cast<double>(state.rotation_tick);
        double squared_norm=0;
        for(unsigned i=0;i<4;++i) {
            const double delta=static_cast<double>(state.rotation_velocity[i])*rotation_elapsed;
            const double value=static_cast<double>(state.rotation[i])+delta;
            sampled.quaternion_wxyz[i]=value/131072.0;
            const double square=sampled.quaternion_wxyz[i]*sampled.quaternion_wxyz[i];
            squared_norm=squared_norm+square;
        }
        // Ordinary sums preserve the stated operation order. For subnormal or
        // underflowed squared norms, rescale first so a nonzero tiny sample is
        // not mistaken for the zero quaternion (nor normalized by rounded zero).
        double scale=0;
        for(auto component:sampled.quaternion_wxyz)scale=std::max(scale,std::abs(component));
        sampled.rotation_valid=scale>0;
        if(sampled.rotation_valid) {
            if(squared_norm<std::numeric_limits<double>::min()) {
                squared_norm=0;
                for(auto& component:sampled.quaternion_wxyz) {
                    component=component/scale;
                    const double square=component*component;
                    squared_norm=squared_norm+square;
                }
            }
            const double norm=std::sqrt(squared_norm);
            for(auto& component:sampled.quaternion_wxyz)component=component/norm;
        } else sampled.quaternion_wxyz={0,0,0,0};
        sampled.drives_joint=((animation.skeleton.joint_mask>>joint)&1U)==0;
    }
    return result;
}
RtaAnimationTransforms buildRtaAnimationTransforms(const RtaAnimationSkeleton& skeleton,
                                                  const RtaAnimationPose& pose) {
    validateTransformInput(skeleton,pose);
    RtaAnimationTransforms result{}; result.tick=pose.tick; result.joints.resize(pose.joints.size());
    for(std::size_t joint=0;joint<pose.joints.size();++joint) {
        const auto& input=pose.joints[joint]; auto& output=result.joints[joint];
        output.drives_joint=input.drives_joint;
        const auto parent=skeleton.parents[joint];
        if(!input.rotation_valid || (parent>=0 && !result.joints[std::size_t(parent)].valid))continue;
        const auto& q=input.quaternion_wxyz;
        const double w=q[0],x=q[1],y=q[2],z=q[3];
        const double xx=x*x,yy=y*y,zz=z*z,xy=x*y,xz=x*z,yz=y*z,xw=x*w,yw=y*w,zw=z*w;
        const std::array<double,12> local{{
            1-2*yy-2*zz, 2*xy-2*zw, 2*xz+2*yw, input.position[0],
            2*xy+2*zw, 1-2*xx-2*zz, 2*yz-2*xw, input.position[1],
            2*xz-2*yw, 2*yz+2*xw, 1-2*xx-2*yy, input.position[2]
        }};
        if(parent<0)output.model=local;
        else {
            const auto& ancestor=result.joints[std::size_t(parent)].model;
            for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col) {
                double value=0;
                for(unsigned axis=0;axis<3;++axis) {
                    const double product=ancestor[row*4+axis]*local[axis*4+col];
                    value=value+product;
                }
                if(col==3)value=value+ancestor[row*4+3];
                output.model[row*4+col]=value;
            }
        }
        output.skin=output.model;
        for(unsigned row=0;row<3;++row) {
            double translation=0;
            for(unsigned axis=0;axis<3;++axis) {
                const double bind=static_cast<double>(skeleton.bind_offsets[joint][axis])/64.0;
                const double product=output.model[row*4+axis]*bind;
                translation=translation+product;
            }
            output.skin[row*4+3]=translation+output.model[row*4+3];
        }
        for(auto component:output.model)need(std::isfinite(component), "animation model transform arithmetic is nonfinite");
        for(auto component:output.skin)need(std::isfinite(component), "animation skin transform arithmetic is nonfinite");
        output.valid=true;
    }
    return result;
}
} // namespace champions
