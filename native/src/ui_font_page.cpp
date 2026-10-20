#include "champions/ui_font_page.hpp"
#include <algorithm>
#include <cstring>

namespace champions {
namespace {
constexpr std::size_t kPaletteDbp = 0x14;
constexpr std::size_t kImageDbp8 = 0x474;

std::uint16_t u16(Bytes b, std::size_t o) {
    if (!b.data || o + 2 > b.size)
        throw FormatError("truncated font page field");
    return std::uint16_t(b.data[o] | (b.data[o + 1] << 8));
}
std::uint32_t u32(Bytes b, std::size_t o) {
    if (!b.data || o + 4 > b.size)
        throw FormatError("truncated font page field");
    return std::uint32_t(b.data[o]) | (std::uint32_t(b.data[o + 1]) << 8) |
           (std::uint32_t(b.data[o + 2]) << 16) | (std::uint32_t(b.data[o + 3]) << 24);
}
std::uint64_t u64(Bytes b, std::size_t o) {
    return std::uint64_t(u32(b, o)) | (std::uint64_t(u32(b, o + 4)) << 32);
}
void put32(std::vector<std::uint8_t> &b, std::size_t o, std::uint32_t v) {
    b[o] = v & 255;
    b[o + 1] = (v >> 8) & 255;
    b[o + 2] = (v >> 16) & 255;
    b[o + 3] = (v >> 24) & 255;
}

struct Transfer {
    int buffer = -1;
    std::uint32_t dbw = 0, dpsm = 0, dsax = 0, dsay = 0, rrw = 0, rrh = 0;
};

std::vector<Transfer> walkPacket(Bytes data, std::size_t begin, std::size_t end) {
    std::vector<Transfer> out;
    int buffer = -1;
    Transfer state;
    std::size_t cursor = begin;
    while (cursor < end) {
        if (end - cursor < 16)
            throw FormatError("truncated font page GIF tag");
        const auto low = u64(data, cursor);
        const auto high = u64(data, cursor + 8);
        cursor += 16;
        const auto loops = static_cast<std::size_t>(low & 0x7fffU);
        const int pre = (low >> 46) & 1;
        const int mode = (low >> 58) & 3;
        int nreg = (low >> 60) & 15;
        if (nreg == 0)
            nreg = 16;
        if (mode == 0) {
            if (nreg != 1 || (high & 0xfU) != 0xeU || pre)
                throw FormatError("font page GIF tag is not a register list");
            for (std::size_t i = 0; i < loops; ++i) {
                if (end - cursor < 16)
                    throw FormatError("font page register list runs past the packet");
                const auto value = u64(data, cursor);
                const auto reg = data.data[cursor + 8];
                if (reg == 0x50) {
                    ++buffer;
                    state.dbw = static_cast<std::uint32_t>((value >> 48) & 0x3fU);
                    state.dpsm = static_cast<std::uint32_t>((value >> 56) & 0x3fU);
                    if ((value & 0xffffffffU) != 0 || ((value >> 32) & 0x3fffU) != 0)
                        throw FormatError("font page BITBLTBUF has a base pointer");
                } else if (reg == 0x51) {
                    state.dsax = static_cast<std::uint32_t>((value >> 32) & 0x7ffU);
                    state.dsay = static_cast<std::uint32_t>((value >> 48) & 0x7ffU);
                    if ((value & 0xffffffffU) != 0 || (value >> 59) != 0)
                        throw FormatError("font page TRXPOS has a source or direction");
                } else if (reg == 0x52) {
                    state.rrw = static_cast<std::uint32_t>(value & 0xfffU);
                    state.rrh = static_cast<std::uint32_t>((value >> 32) & 0xfffU);
                } else if (reg == 0x53) {
                    if ((value & 3U) != 0)
                        throw FormatError("font page TRXDIR is not host to local");
                } else if (reg != 0x62) {
                    throw FormatError("font page GS register is not used by this format");
                }
                cursor += 16;
            }
        } else if (mode == 2) {
            if (loops > (end - cursor) / 16)
                throw FormatError("font page image runs past the packet");
            const auto bytes = loops * 16;
            if (buffer < 0)
                throw FormatError("font page image has no destination");
            const unsigned bpp = state.dpsm == 0 ? 32U : state.dpsm == 0x14U ? 4U : 0U;
            if (!bpp)
                throw FormatError("font page upload format is not handled");
            const auto want = (static_cast<std::uint64_t>(state.rrw) * state.rrh * bpp + 7) / 8;
            if (want > bytes || bytes >= want + 16)
                throw FormatError("font page image byte count does not match its rectangle");
            Transfer row = state;
            row.buffer = buffer;
            out.push_back(row);
            cursor += bytes;
        } else {
            throw FormatError("font page GIF tag uses REGLIST mode");
        }
    }
    if (cursor != end)
        throw FormatError("font page packet overruns its size");
    return out;
}

Texture32 weaveFields(const Texture32 &stored) {
    const auto half = stored.height / 2;
    Texture32 out{stored.width, half * 2, stored.flags, {}};
    const auto stride = std::size_t(stored.width) * 4;
    out.rgba.resize(stride * out.height);
    for (std::uint32_t k = 0; k < half; ++k) {
        std::copy_n(stored.rgba.data() + k * stride, stride, out.rgba.data() + (2 * k) * stride);
        std::copy_n(stored.rgba.data() + (half + k) * stride, stride,
                    out.rgba.data() + (2 * k + 1) * stride);
    }
    return out;
}
} // namespace

int fontPageImageDbp(std::uint16_t flags) {
    if (flags & 0x0004)
        return static_cast<int>(kImageDbp8);
    if (flags & 0x2000)
        return 0xb4;
    return -1;
}

std::vector<std::uint8_t> applyTexRelocatePrefix(std::uint32_t base, Bytes bytes) {
    if (!bytes.data || bytes.size < 0x20)
        throw FormatError("short texture header");
    if (u16(bytes, 8) & 0x0101)
        throw FormatError("flags take the block-table walk");
    std::vector<std::uint8_t> out(bytes.data, bytes.data + bytes.size);
    if (u32({out.data(), out.size()}, 0x1c) != 0)
        return out;
    const auto offset = u32({out.data(), out.size()}, 0x10);
    out[0x0a] = 0xfb;
    out[0x0b] = 0xa7;
    out[0x1c] = out[0x1d] = out[0x1e] = out[0x1f] = 0xff;
    put32(out, 0x10, static_cast<std::uint32_t>((static_cast<std::uint64_t>(base) + offset) & 0xffffffffU));
    return out;
}

FontPageInfo parseFontPage(Bytes data) {
    if (!data.data || data.size < 0x40)
        throw FormatError("short font page");
    FontPageInfo page;
    page.width = u16(data, 0);
    page.height = u16(data, 2);
    page.blocks = u16(data, 4);
    page.qwords = u16(data, 6);
    page.flags = u16(data, 8);
    const auto magic = u16(data, 0x0a);
    page.tex0 = u32(data, 0x0c);
    page.packet_offset = u32(data, 0x10);
    if (magic)
        throw FormatError("font page is already relocated");
    if (page.packet_offset < 0x40)
        throw FormatError("packet offset overlaps the runtime header");
    if (page.packet_offset > data.size ||
        std::any_of(data.data + 0x14, data.data + page.packet_offset,
                    [](std::uint8_t v) { return v != 0; }))
        throw FormatError("gap before the packet is not zero");
    if ((page.flags & ~0x0044U) != 0 || (page.flags & 0x0004U) == 0)
        throw FormatError("flags are not an 8-bit font page");
    if (!page.qwords || page.packet_offset > data.size ||
        std::size_t(page.qwords) > (data.size - page.packet_offset) / 16 ||
        page.packet_offset + std::size_t(page.qwords) * 16 != data.size)
        throw FormatError("packet does not end at the end of the page");
    if (page.packet_offset + kImageDbp8 + 8 > data.size)
        throw FormatError("packet is shorter than the image BITBLTBUF");
    page.tbw = page.tex0 & 0x3fU;
    const auto psm = (page.tex0 >> 6) & 0x3fU;
    const auto log_w = (page.tex0 >> 12) & 15U;
    const auto log_h = (page.tex0 >> 16) & 15U;
    page.surface_width = 1U << log_w;
    page.surface_height = 1U << log_h;
    if ((page.tex0 >> 23) != 0 || psm != 0x13 || !page.tbw || (page.tbw & 1U) != 0)
        throw FormatError("TEX0 is not a PSMT8 page");
    if (!(page.width > 0 && page.width <= page.surface_width && page.height > 0 &&
          page.height <= page.surface_height && page.surface_width <= 1024 &&
          page.surface_height <= 1024))
        throw FormatError("size does not fit the surface");
    if ((u64(data, page.packet_offset + 0x18) & 0xffU) != 0x50)
        throw FormatError("palette BITBLTBUF is not at packet+0x10");
    if (page.packet_offset + kImageDbp8 + 4 + 8 > data.size ||
        (u64(data, page.packet_offset + kImageDbp8 + 4) & 0xffU) != 0x50)
        throw FormatError("image BITBLTBUF is not at packet+0x470");
    const auto transfers = walkPacket(data, page.packet_offset, data.size);
    const Transfer *palette = nullptr;
    const Transfer *image = nullptr;
    for (const auto &row : transfers) {
        if (row.buffer == 0) {
            if (palette)
                throw FormatError("font page is not one palette and one image");
            palette = &row;
        } else if (row.buffer == 1) {
            if (image)
                throw FormatError("font page is not one palette and one image");
            image = &row;
        } else {
            throw FormatError("font page is not one palette and one image");
        }
    }
    if (transfers.size() != 2 || !palette || !image)
        throw FormatError("font page is not one palette and one image");
    if (palette->rrw != 16 || palette->rrh != 16 || palette->dpsm != 0)
        throw FormatError("palette is not 16x16 PSMCT32");
    if (image->dpsm != 0 || image->dsax != 0 || image->dsay != 0)
        throw FormatError("image is not one PSMCT32 rectangle at the origin");
    if (image->dbw * 2 != page.tbw)
        throw FormatError("upload width does not match TBW");
    static_assert(kPaletteDbp == 0x14, "palette DBP halfword");
    return page;
}

DecodedFontPage decodeFontPage(Bytes bytes, bool weave) {
    const auto page = parseFontPage(bytes);
    auto decoded = decodeRtaPsmt8Packet(bytes, page.packet_offset);
    if (decoded.image.width != page.width || decoded.image.height != page.height)
        throw FormatError("decoded font page size does not match the header");
    DecodedFontPage out;
    out.uncovered = decoded.uncovered;
    out.packet_offset = page.packet_offset;
    out.flags = page.flags;
    out.image = weave ? weaveFields(decoded.image) : std::move(decoded.image);
    return out;
}

void refuseFntHeaderLineHeight() {
    throw FormatError("FNT header +2 has no reader; line height is a caller argument");
}
} // namespace champions
