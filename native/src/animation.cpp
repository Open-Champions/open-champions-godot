#include "champions/animation.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

// Original implementation from the published RTA format contract:
// Open-Champions/champions-return-to-arms-re 45668f7,
// docs/formats/anim-audio.md sections2.1,2.2,2.5,2.8.
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
} // namespace champions
