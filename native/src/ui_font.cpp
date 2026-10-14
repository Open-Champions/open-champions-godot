#include "champions/ui_font.hpp"
namespace champions {
RtaUiFont readRtaUiFont(Bytes b) {
    if (!b.data || b.size < 0x214) throw FormatError("FNT requires at least 0x214 bytes.");
    auto u16 = [&](std::size_t o) { return std::uint32_t(b.data[o]) | (std::uint32_t(b.data[o+1]) << 8); };
    auto u32 = [&](std::size_t o) { return u16(o) | (u16(o+2) << 16); };
    auto s16 = [&](std::size_t o) { const auto v = u16(o); return v < 32768 ? std::int32_t(v) : std::int32_t(v) - 65536; };
    const auto n = u16(0);
    RtaUiFont out{u32(8), u32(12), u32(16), {}, {}, {}};
    // Subtraction/division validates without overflowing even on 32-bit hosts.
    if (out.glyph_offset > b.size || n > (b.size-out.glyph_offset)/16)
        throw FormatError("FNT glyph table is outside input.");
    if (out.texture_offset >= b.size || out.kerning_offset > b.size)
        throw FormatError("FNT texture or kerning offset is outside input.");
    out.glyphs.reserve(n);
    for (std::size_t i=0; i<n; ++i) {
        const auto o = std::size_t(out.glyph_offset) + i*16;
        out.glyphs.push_back({std::int32_t(u16(o)),std::int32_t(u16(o+2)),std::int32_t(u16(o+4)),
            std::int32_t(u16(o+6)),std::int32_t(u16(o+8)),s16(o+10),std::int32_t(u16(o+12))});
    }
    std::size_t o = out.kerning_offset;
    while (o <= out.texture_offset && out.texture_offset-o >= 6) {
        const auto left = u16(o);
        if (left == 0xffff) break;
        out.kerns.push_back({std::int32_t(left),std::int32_t(u16(o+2)),s16(o+4)});
        o += 6;
    }
    out.texture.assign(b.data+out.texture_offset,b.data+b.size);
    return out;
}
}
