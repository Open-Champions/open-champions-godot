#include "champions/texture.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>

// Authored from the published RTA format contract, not decompiler output:
// Open-Champions/champions-return-to-arms-re commit c3d7264,
// docs/formats/model-texture.md (.tex header/pixel data).
// tools/model_info.py was consulted as the research reference for field meanings.
namespace champions {
namespace {
constexpr std::size_t header_size = 0x80;
constexpr std::size_t max_file_size = header_size + 65535U * 16U;
constexpr std::uint16_t storage_flags = 0x20a4; // palette8, rgba32, no-pixels, palette4
constexpr std::uint16_t allowed_flags = 0x1e70; // rgba32 + documented non-storage bits

void require(bool condition, const char* message) {
    if (!condition) throw FormatError(message);
}

std::uint64_t little(Bytes bytes, std::size_t offset, unsigned width) {
    require(offset <= bytes.size && width <= bytes.size - offset, "truncated texture field");
    std::uint64_t result = 0;
    for (unsigned i = 0; i < width; ++i) result |= std::uint64_t(bytes.data[offset + i]) << (8U * i);
    return result;
}

struct Transfer {
    bool buffer = false;
    bool position = false;
    bool inherited_position = false;
    bool extent = false;
    bool direction = false;
    bool image = false;
    std::uint32_t x = 0, y = 0, width = 0, height = 0;
};

Texture32 decodeTexture(Bytes bytes, bool allow_palette) {
    require(bytes.data != nullptr && bytes.size >= header_size + 16, "texture needs a complete 0x80-byte lump header and GIF packet");
    require(bytes.size <= max_file_size, "texture exceeds the bounded lump packet size");
    const auto width = static_cast<std::uint32_t>(little(bytes, 0, 2));
    const auto height = static_cast<std::uint32_t>(little(bytes, 2, 2));
    require(width > 0 && height > 0 && width <= 1024 && height <= 1024, "texture dimensions must be 1..1024");
    const auto blocks = little(bytes, 4, 2);
    require(blocks > 0 && blocks <= 16384, "invalid GS allocation block count");
    const auto qwords = static_cast<std::size_t>(little(bytes, 6, 2));
    require(qwords > 0 && qwords <= (bytes.size - header_size) / 16, "texture packet length exceeds input");
    const auto flags = static_cast<std::uint16_t>(little(bytes, 8, 2));
    require((flags & 0x0101U) == 0, "streamed/world texture forms are unsupported");
    const bool paletted = allow_palette && (flags & storage_flags) == 0x2000;
    require(paletted || (flags & storage_flags) == 0x0020,
            "only 32-bit or plain 4-bit textures are supported by this decoder");
    const auto permitted_flags = allowed_flags | (paletted ? 0x2000U : 0U);
    require((flags & ~permitted_flags) == 0, "unknown texture flag bits are unsupported");
    require(little(bytes, 0x0a, 2) == 0 && little(bytes, 0x10, 4) == header_size,
            "relocated or non-lump texture header is unsupported");
    require(std::all_of(bytes.data + 0x14, bytes.data + header_size, [](std::uint8_t v) { return v == 0; }),
            "texture runtime header fields must be zero");
    const auto tex0 = static_cast<std::uint32_t>(little(bytes, 0x0c, 4));
    require((tex0 >> 23) == 0 && ((tex0 >> 6) & 0x3fU) == (paletted ? 0x14U : 0U),
            "texture storage flags must match the PSMCT32 or PSMT4 TEX0 fragment");
    const auto buffer_width = tex0 & 0x3fU;
    const auto log_width = (tex0 >> 12) & 15U;
    const auto log_height = (tex0 >> 16) & 15U;
    require(log_width <= 10 && log_height <= 10, "texture surface dimensions exceed 1024");
    const std::uint32_t surface_width = 1U << log_width;
    const std::uint32_t surface_height = 1U << log_height;
    require(width <= surface_width && height <= surface_height && buffer_width > 0 && buffer_width * 64 >= (paletted ? width : surface_width),
            "used dimensions or buffer width do not fit the texture surface");
    const auto end = header_size + qwords * 16;
    require(std::all_of(bytes.data + end, bytes.data + bytes.size, [](std::uint8_t v) { return v == 0; }),
            "nonzero bytes follow the declared texture packet");

    Texture32 result{width, height, flags, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4, 0)};
    Transfer transfer;
    unsigned completed_uploads = 0;
    std::size_t palette_offset = 0;
    std::size_t cursor = header_size;
    while (cursor < end) {
        require(end - cursor >= 16, "truncated GIF tag");
        const auto low = little(bytes, cursor, 8);
        const auto registers = little(bytes, cursor + 8, 8);
        cursor += 16;
        const auto loops = static_cast<std::size_t>(low & 0x7fffU);
        const auto mode = (low >> 58) & 3U;
        const auto nreg = (low >> 60) & 15U;
        constexpr std::uint64_t tag_fields = 0xffffULL | (3ULL << 58) | (15ULL << 60);
        require((low & ~tag_fields) == 0, "GIF primitive or reserved tag fields are unsupported");
        require(loops <= (end - cursor) / 16, "GIF payload exceeds the declared packet");
        if (mode == 0) {
            require(nreg == 1 && registers == 0x0e, "only packed GIF A+D register tags are supported");
            for (std::size_t i = 0; i < loops; ++i, cursor += 16) {
                const auto value = little(bytes, cursor, 8);
                const auto reg = little(bytes, cursor + 8, 8);
                if (reg == 0x62) continue; // LABEL padding does not change transfer state.
                // GS position persists between destinations. Only the validated
                // palette origin can be inherited; extent and direction must be
                // supplied again for the image. A new TRXPOS may override it.
                const bool inherit_position = reg == 0x50 && paletted && completed_uploads == 1 && transfer.image;
                if (inherit_position) transfer = Transfer{};
                require(!transfer.image, "transfer registers after IMAGE are unsupported");
                if (reg == 0x50) {
                    require(!transfer.buffer && !transfer.position && !transfer.extent && !transfer.direction,
                            "texture must upload into exactly one fresh destination buffer");
                    if (paletted && completed_uploads == 0) {
                        // The 8-wide palette has its own PSMCT32 stride, independent
                        // of the image's TEX0. Base/source/reserved fields stay zero.
                        require((value & ~(0x3fULL << 48)) == 0 && ((value >> 48) & 0x3fU) > 0,
                                "palette BITBLTBUF must be unrelocated PSMCT32 with a nonzero stride");
                    } else {
                        const auto expected = (std::uint64_t(buffer_width) << 48) | (paletted ? (0x14ULL << 56) : 0);
                        require(value == expected,
                                "image BITBLTBUF must match the unrelocated texture format and buffer width; 4-bit-as-32-bit uploads are unsupported");
                    }
                    transfer.buffer = true;
                    if (inherit_position) {
                        transfer.position = true;
                        transfer.inherited_position = true;
                    }
                } else if (reg == 0x51) {
                    require(transfer.buffer && (!transfer.position || transfer.inherited_position) && !transfer.direction, "incoherent or repeated TRXPOS");
                    constexpr auto mask = (0x7ffULL << 32) | (0x7ffULL << 48);
                    require((value & ~mask) == 0, "TRXPOS source, direction or reserved fields are unsupported");
                    transfer.x = static_cast<std::uint32_t>((value >> 32) & 0x7ffU);
                    transfer.y = static_cast<std::uint32_t>((value >> 48) & 0x7ffU);
                    transfer.position = true;
                    transfer.inherited_position = false;
                } else if (reg == 0x52) {
                    require(transfer.buffer && !transfer.extent && !transfer.direction, "incoherent or repeated TRXREG");
                    require((value & ~(0xfffULL | (0xfffULL << 32))) == 0, "TRXREG reserved fields are unsupported");
                    transfer.width = static_cast<std::uint32_t>(value & 0xfffU);
                    transfer.height = static_cast<std::uint32_t>((value >> 32) & 0xfffU);
                    require(transfer.width > 0 && transfer.height > 0 && transfer.width <= 1024 && transfer.height <= 1024,
                            "transfer dimensions must be 1..1024");
                    transfer.extent = true;
                } else if (reg == 0x53) {
                    require(transfer.buffer && transfer.position && transfer.extent && !transfer.direction,
                            "TRXDIR requires one complete transfer setup");
                    require(value == 0, "only host-to-local TRXDIR is supported");
                    transfer.direction = true;
                } else {
                    throw FormatError("unknown GS register in texture upload");
                }
            }
        } else if (mode == 2) {
            require(nreg == 0 && registers == 0, "IMAGE tag register fields are unsupported");
            require(transfer.buffer && transfer.position && transfer.extent && transfer.direction && !transfer.image,
                    "IMAGE requires exactly one complete unconsumed transfer setup");
            if (paletted && completed_uploads == 0) {
                require(transfer.x == 0 && transfer.y == 0 && transfer.width == 8 && transfer.height == 2 && loops == 4,
                        "4-bit palette must be one 8x2 RGBA upload at the origin");
                palette_offset = cursor;
            } else {
                const auto destination_width = paletted ? std::min(surface_width, buffer_width * 64) : surface_width;
                require(transfer.x <= destination_width && transfer.width <= destination_width - transfer.x &&
                        transfer.y <= surface_height && transfer.height <= surface_height - transfer.y,
                        "IMAGE destination exceeds the bounded texture surface");
                require(transfer.x == 0 && transfer.y == 0 && transfer.width >= width && transfer.height >= height,
                        "one IMAGE upload must cover every used texture pixel");
                const auto texel_count = static_cast<std::size_t>(transfer.width) * transfer.height;
                const auto image_bytes = paletted ? (texel_count + 1) / 2 : texel_count * 4;
                require(loops == (image_bytes + 15) / 16, "IMAGE byte count does not match the transfer rectangle");
                for (std::uint32_t y = 0; y < height; ++y) {
                    for (std::uint32_t x = 0; x < width; ++x) {
                        const auto texel = static_cast<std::size_t>(y) * transfer.width + x;
                        std::size_t source = cursor + texel * 4;
                        if (paletted) {
                            // Nibbles form one continuous row-major stream; an odd
                            // row width does not insert a half-byte row pad.
                            const auto index = (bytes.data[cursor + texel / 2] >> (4U * (texel & 1U))) & 15U;
                            source = palette_offset + index * 4;
                        }
                        const auto destination = (static_cast<std::size_t>(y) * width + x) * 4;
                        result.rgba[destination] = bytes.data[source];
                        result.rgba[destination + 1] = bytes.data[source + 1];
                        result.rgba[destination + 2] = bytes.data[source + 2];
                        result.rgba[destination + 3] = static_cast<std::uint8_t>(std::min(255U, unsigned(bytes.data[source + 3]) * 255U / 128U));
                    }
                }
            }
            ++completed_uploads;
            cursor += loops * 16;
            transfer.image = true;
        } else {
            throw FormatError("GIF REGLIST or disabled tag modes are unsupported");
        }
    }
    require(transfer.image && completed_uploads == (paletted ? 2U : 1U),
            "texture packet must contain a complete image and, for 4-bit, a preceding palette");
    return result;
}
} // namespace

Texture32 decodeRtaTexture32(Bytes bytes) { return decodeTexture(bytes, false); }
Texture32 decodeRtaTexture(Bytes bytes) { return decodeTexture(bytes, true); }
} // namespace champions
