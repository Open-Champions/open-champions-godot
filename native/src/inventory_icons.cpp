#include "champions/inventory_icons.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

namespace champions::inventory_icons {
namespace {

using Byte = std::uint8_t;

std::uint16_t u16(const Byte *data, std::size_t at) {
    return std::uint16_t(data[at]) | (std::uint16_t(data[at + 1]) << 8);
}
std::uint32_t u32(const Byte *data, std::size_t at) {
    return std::uint32_t(u16(data, at)) | (std::uint32_t(u16(data, at + 2)) << 16);
}
std::uint64_t u64(const Byte *data, std::size_t at) {
    return std::uint64_t(u32(data, at)) | (std::uint64_t(u32(data, at + 4)) << 32);
}

constexpr std::array<std::array<std::size_t, 8>, 4> block{{
    {{0, 1, 4, 5, 16, 17, 20, 21}},
    {{2, 3, 6, 7, 18, 19, 22, 23}},
    {{8, 9, 12, 13, 24, 25, 28, 29}},
    {{10, 11, 14, 15, 26, 27, 30, 31}},
}};
constexpr std::array<std::size_t, 8> word{{0, 1, 4, 5, 8, 9, 12, 13}};

struct Transfer {
    int buffer = -1;
    std::uint32_t dbw = 0, psm = 0, x = 0, y = 0, w = 0, h = 0;
    const Byte *pixels = nullptr;
    std::size_t size = 0;
};

std::vector<Transfer> transfers(const Byte *data, std::size_t size) {
    const auto span = packet_span(data, size);
    std::size_t at = span.offset;
    const std::size_t end = at + std::size_t(span.qwords) * 16;
    Transfer state;
    unsigned fields = 0;
    std::vector<Transfer> result;
    while (at < end) {
        const std::uint64_t low = u64(data, at);
        const std::uint64_t regs = u64(data, at + 8);
        at += 16;
        const auto count = std::size_t(low & 0x7fff);
        const auto mode = (low >> 58) & 3;
        const auto nr_bits = (low >> 60) & 15;
        const auto nr = nr_bits ? nr_bits : 16;
        if (mode == 0) {
            if (nr != 1 || regs != 14 || count > (end - at) / 16) {
                throw std::invalid_argument("unsupported/truncated A+D tag");
            }
            for (std::size_t i = 0; i < count; ++i, at += 16) {
                const std::uint64_t value = u64(data, at);
                const std::uint64_t reg = u64(data, at + 8);
                if (reg == 0x50) {
                    ++state.buffer;
                    state.dbw = (value >> 48) & 63;
                    state.psm = (value >> 56) & 63;
                    fields |= 1;
                } else if (reg == 0x51) {
                    state.x = (value >> 32) & 2047;
                    state.y = (value >> 48) & 2047;
                    fields |= 2;
                } else if (reg == 0x52) {
                    state.w = value & 4095;
                    state.h = (value >> 32) & 4095;
                    fields |= 4;
                } else if (reg == 0x53) {
                    if (value != 0) throw std::invalid_argument("not a host-to-local transfer");
                    fields |= 8;
                } else if (reg != 0x62) {
                    throw std::invalid_argument("unsupported GS register");
                }
            }
        } else if (mode == 2) {
            if (fields != 15) throw std::invalid_argument("incomplete transfer state");
            const std::size_t bytes = std::size_t(state.w) * state.h * 4;
            if (state.buffer < 0 || state.psm != 0 || bytes == 0 ||
                bytes != count * 16 || bytes > end - at) {
                throw std::invalid_argument("unsupported/truncated image transfer");
            }
            state.pixels = data + at;
            state.size = bytes;
            result.push_back(state);
            at += bytes;
        } else {
            throw std::invalid_argument("unsupported GIF mode");
        }
    }
    return result;
}

} // namespace

PacketSpan packet_span(const Byte *data, std::size_t size) {
    if (size < 0x40 || data == nullptr) throw std::invalid_argument("short picture header");
    const std::uint16_t qwc = u16(data, 6);
    const std::size_t offset = u32(data, 0x10);
    const std::size_t packet_bytes = std::size_t(qwc) * 16;
    if (qwc == 0 || offset < 0x40 || offset % 16 || offset > size || packet_bytes > size - offset) {
        throw std::invalid_argument("invalid upload span");
    }
    return {offset, qwc};
}

std::vector<DmaChunk> dma_chunks(const Byte *data, std::size_t size) {
    auto span = packet_span(data, size);
    std::vector<DmaChunk> result;
    std::size_t remaining = span.qwords;
    while (remaining) {
        const std::size_t count = std::min<std::size_t>(remaining, 79);
        remaining -= count;
        result.push_back({std::uint32_t((remaining ? 0x20000000U : 0x60000000U) | count),
                          std::vector<Byte>(data + span.offset, data + span.offset + count * 16)});
        span.offset += count * 16;
    }
    return result;
}

std::size_t gs32_word(std::size_t x, std::size_t y, std::size_t dbw) {
    return (y / 32 * dbw + x / 64) * 2048 +
           block[(y >> 3) & 3][(x >> 3) & 7] * 64 +
           ((y & 7) >> 1) * 16 + (y & 1) * 2 + word[x & 7];
}

std::size_t gs8_byte(std::size_t x, std::size_t y, std::size_t tbw) {
    const auto yy = y & 15;
    const auto xi = (x & 7) ^ ((((yy + 2) >> 2) & 1) ? 4U : 0U);
    const auto within_word = (yy >> 2) * 16 + (yy & 1) * 2 + word[xi];
    return (y / 64 * (tbw / 2) + x / 128) * 8192 +
           block[(y >> 4) & 3][(x >> 4) & 7] * 256 +
           within_word * 4 + ((yy & 3) >> 1) + (((x >> 3) & 1) << 1);
}

Picture decode(const Byte *data, std::size_t size) {
    const auto span = packet_span(data, size);
    const auto w = u16(data, 0), h = u16(data, 2), flags = u16(data, 8);
    const auto marker = u16(data, 10);
    const auto tex0 = u32(data, 12);
    const auto tbw = tex0 & 63, psm = (tex0 >> 6) & 63;
    const auto nw = 1U << ((tex0 >> 12) & 15), nh = 1U << ((tex0 >> 16) & 15);
    if ((flags != 0x44 && flags != 0x844 && flags != 0x1044 && flags != 0x1844) ||
        marker != 0 || psm != 19 || tbw != 2 || w == 0 || w > std::min(nw, 128U) ||
        h == 0 || h > std::min(nh, 256U) || h % 2) {
        throw std::invalid_argument("not the observed inventory-icon profile");
    }
    const auto parts = transfers(data, size);
    if (parts.size() != 2) throw std::invalid_argument("expected one palette and one image");
    const auto &pal = parts[0], &img = parts[1];
    if (pal.buffer != 0 || pal.dbw != 1 || pal.x != 0 || pal.y != 0 ||
        pal.w != 16 || pal.h != 16) {
        throw std::invalid_argument("unsupported palette");
    }
    if (img.buffer != 1 || img.dbw != 1 || img.x != 0 || img.y != 0 ||
        img.w > 64 || img.h > 128) {
        throw std::invalid_argument("unsupported image upload");
    }
    std::array<Byte, 32768> memory{};
    std::array<Byte, 8192> present{};
    for (std::size_t y = 0; y < img.h; ++y) {
        for (std::size_t x = 0; x < img.w; ++x) {
            const auto address = gs32_word(x, y);
            if (address >= present.size()) throw std::invalid_argument("image upload exceeds GS memory");
            const auto source = (y * img.w + x) * 4;
            std::copy_n(img.pixels + source, 4, memory.data() + address * 4);
            present[address] = 1;
        }
    }
    Picture picture{w, h, flags, span.qwords, span.offset, {}};
    picture.rgba.reserve(std::size_t(w) * h * 4);
    for (std::size_t y = 0; y < h; ++y) {
        const auto stored_y = y / 2 + ((y & 1) ? h / 2 : 0);
        for (std::size_t x = 0; x < w; ++x) {
            const auto at = gs8_byte(x, stored_y);
            if (at >= memory.size() || !present[at / 4]) {
                throw std::invalid_argument("logical picture reads unsupplied GS memory");
            }
            const auto index = memory[at];
            const auto clut = (index & ~0x18) | ((index & 8) << 1) | ((index & 16) >> 1);
            picture.rgba.insert(picture.rgba.end(), pal.pixels + 4 * clut, pal.pixels + 4 * clut + 4);
        }
    }
    return picture;
}

std::vector<Byte> display_rgba(const std::vector<Byte> &rgba) {
    if (rgba.size() % 4) throw std::invalid_argument("not RGBA");
    std::vector<Byte> output = rgba;
    for (std::size_t i = 3; i < output.size(); i += 4) {
        output[i] = Byte(std::min<unsigned>(255, unsigned(output[i]) * 2));
    }
    return output;
}

std::int32_t bottom_anchor_y(std::int32_t anchor, std::int32_t height, std::uint16_t flags) {
    const auto y = std::int64_t(anchor) - height + ((flags & 0x0800) ? 1 : 0);
    if (y < std::numeric_limits<std::int32_t>::min() ||
        y > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument("bottom anchor outside signed32");
    }
    return static_cast<std::int32_t>(y);
}

} // namespace champions::inventory_icons
