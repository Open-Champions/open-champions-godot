#include "champions/texture.hpp"
#include "champions/ui_font_page.hpp"
#include "champions/ui_layout.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace champions;
using namespace champions::ui_layout;
namespace {
int checks = 0;
void expect(bool ok, const char *name) {
    ++checks;
    if (!ok)
        throw std::runtime_error(name);
}
template <class Fn> void reject(Fn fn, const char *name) {
    bool threw = false;
    try {
        fn();
    } catch (const std::exception &) {
        threw = true;
    }
    expect(threw, name);
}
void append64(std::vector<std::uint8_t> &b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void tag(std::vector<std::uint8_t> &b, std::uint64_t nloop, std::uint64_t flg, bool packed) {
    append64(b, (nloop & 0x7fffU) | (flg << 58) | (packed ? (1ULL << 60) : 0));
    append64(b, packed ? 0xe : 0);
}
void reg(std::vector<std::uint8_t> &b, std::uint64_t value, std::uint64_t register_id) {
    append64(b, value);
    append64(b, register_id);
}
constexpr unsigned kBlocks[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21},
                                    {2, 3, 6, 7, 18, 19, 22, 23},
                                    {8, 9, 12, 13, 24, 25, 28, 29},
                                    {10, 11, 14, 15, 26, 27, 30, 31}};
constexpr unsigned kWords[8] = {0, 1, 4, 5, 8, 9, 12, 13};
std::size_t address32(std::uint32_t x, std::uint32_t y, std::uint32_t pages) {
    const auto page = std::size_t(y / 32) * pages + x / 64;
    const auto block = kBlocks[(y / 8) % 4][(x / 8) % 8];
    const auto word = ((y % 8) / 2) * 16 + (y % 2) * 2 + kWords[x % 8];
    return page * 8192 + block * 256 + word * 4;
}
std::size_t address8(std::uint32_t x, std::uint32_t y, std::uint32_t pages) {
    const auto page = std::size_t(y / 64) * pages + x / 128;
    const auto block = kBlocks[(y / 16) % 4][(x / 16) % 8];
    const auto row = y % 16;
    const auto column = (x % 8) ^ (((row + 2) / 4) % 2 ? 4U : 0U);
    const auto word = (row / 4) * 16 + (row % 2) * 2 + kWords[column];
    return page * 8192 + block * 256 + word * 4 + (row % 4) / 2 + 2 * ((x / 8) % 2);
}
unsigned clutOrder(unsigned i) {
    return (i & ~0x18U) | ((i & 8U) << 1) | ((i & 16U) >> 1);
}
void put16(std::vector<std::uint8_t> &b, std::size_t o, int v) {
    b[o] = v & 255;
    b[o + 1] = (v >> 8) & 255;
}

// Synthetic PSMT8 page from the font-page reference. Used size 16x4.
// pixels are {(0,0):1, (3,1):8, (8,2):1}. No original game bytes.
std::vector<std::uint8_t> buildPage(std::size_t data_off, bool packed_image = false) {
    constexpr std::uint32_t dbw = 1, tbw = 2, image_w = 64, image_h = 32;
    std::uint8_t words[64][4] = {};
    bool have[64] = {};
    const std::pair<std::pair<int, int>, int> pixels[] = {{{0, 0}, 1}, {{3, 1}, 8}, {{8, 2}, 1}};
    for (auto pixel : pixels) {
        const auto addr = address8(pixel.first.first, pixel.first.second, dbw);
        const auto slot = addr >> 2;
        words[slot][addr & 3] = static_cast<std::uint8_t>(pixel.second);
        have[slot] = true;
    }
    std::vector<std::uint8_t> image(std::size_t(image_w) * image_h * 4);
    for (std::uint32_t y = 0; y < image_h; ++y) {
        for (std::uint32_t x = 0; x < image_w; ++x) {
            const auto addr = address32(x, y, dbw);
            const auto slot = addr >> 2;
            auto *dest = image.data() + (std::size_t(y) * image_w + x) * 4;
            if (slot < 64 && have[slot])
                std::memcpy(dest, words[slot], 4);
        }
    }
    std::uint8_t palette[1024] = {};
    const std::uint8_t stored[][5] = {{0, 0, 0, 0, 0}, {1, 1, 2, 3, 0x80}, {8, 9, 8, 7, 0x40}};
    for (auto colour : stored) {
        const auto at = clutOrder(colour[0]) * 4;
        std::memcpy(palette + at, colour + 1, 4);
    }
    std::vector<std::uint8_t> packet;
    tag(packet, 4, 0, true);
    reg(packet, std::uint64_t(dbw) << 48, 0x50);
    reg(packet, 0, 0x51);
    reg(packet, 16 | (16ULL << 32), 0x52);
    reg(packet, 0, 0x53);
    tag(packet, 64, 2, packed_image);
    packet.insert(packet.end(), palette, palette + sizeof palette);
    tag(packet, 5, 0, true);
    reg(packet, std::uint64_t(dbw) << 48, 0x50);
    reg(packet, 0, 0x51);
    reg(packet, image_w | (std::uint64_t(image_h) << 32), 0x52);
    reg(packet, 0, 0x62);
    reg(packet, 0, 0x53);
    tag(packet, image.size() / 16, 2, packed_image);
    packet.insert(packet.end(), image.begin(), image.end());
    std::vector<std::uint8_t> page(data_off + packet.size());
    std::memcpy(page.data() + data_off, packet.data(), packet.size());
    const auto tex0 = tbw | (0x13U << 6) | (7U << 12) | (2U << 16) | (1U << 20);
    put16(page, 0, 16);
    put16(page, 2, 4);
    put16(page, 4, 8);
    put16(page, 6, static_cast<int>(packet.size() / 16));
    put16(page, 8, 0x0044);
    page[0x0c] = tex0 & 255;
    page[0x0d] = (tex0 >> 8) & 255;
    page[0x0e] = (tex0 >> 16) & 255;
    page[0x0f] = (tex0 >> 24) & 255;
    page[0x10] = data_off & 255;
    page[0x11] = (data_off >> 8) & 255;
    page[0x12] = (data_off >> 16) & 255;
    page[0x13] = (data_off >> 24) & 255;
    return page;
}
std::uint16_t read16(const std::vector<std::uint8_t> &b, std::size_t o) {
    return std::uint16_t(b[o] | (b[o + 1] << 8));
}
std::uint32_t read32(const std::vector<std::uint8_t> &b, std::size_t o) {
    return std::uint32_t(b[o]) | (std::uint32_t(b[o + 1]) << 8) |
           (std::uint32_t(b[o + 2]) << 16) | (std::uint32_t(b[o + 3]) << 24);
}
bool samePixels(const Texture32 &a, const Texture32 &b) {
    return a.width == b.width && a.height == b.height && a.rgba == b.rgba;
}
} // namespace

int main() {
    try {
        const auto standard = buildPage(0x80);
        const auto lump = decodeRtaTexture({standard.data(), standard.size()});
        for (std::size_t offset : {std::size_t{0x80}, std::size_t{0x44}, std::size_t{0xb4}}) {
            const auto page = buildPage(offset);
            const auto decoded = decodeFontPage({page.data(), page.size()});
            expect(samePixels(decoded.image, lump), "font page matches the lump decoder");
            const auto info = parseFontPage({page.data(), page.size()});
            expect(info.packet_offset == offset && info.width == 16 && info.height == 4 && info.tbw == 2,
                   "packet offset and used size");
            expect(decoded.uncovered == 0, "synthetic used texels sit inside the upload");
            expect(decoded.packet_offset == offset, "decoded packet offset");
        }
        const auto packed = buildPage(0x44, true);
        const auto packed_page = decodeFontPage({packed.data(), packed.size()});
        expect(samePixels(packed_page.image, lump), "reference IMAGE tag still decodes");
        const auto sample = buildPage(0x44);
        const auto pixels = decodeFontPage({sample.data(), sample.size()});
        expect(pixels.image.width == 16 && pixels.image.height == 4, "used rectangle");
        const auto at = [&](int x, int y) {
            const auto i = (std::size_t(y) * pixels.image.width + x) * 4;
            return std::vector<int>{pixels.image.rgba[i], pixels.image.rgba[i + 1], pixels.image.rgba[i + 2],
                                    pixels.image.rgba[i + 3]};
        };
        expect((at(0, 0) == std::vector<int>{1, 2, 3, 255}), "palette index 1 scales 0x80 to 255");
        expect((at(3, 1) == std::vector<int>{9, 8, 7, 64 * 255 / 128}), "palette index 8 keeps the 0x40 scale");
        const auto woven = decodeFontPage({sample.data(), sample.size()}, true);
        expect(woven.image.width == pixels.image.width && woven.image.height == pixels.image.height &&
                   woven.image.rgba != pixels.image.rgba,
               "field weave changes stored order");
        const auto stride = std::size_t(pixels.image.width) * 4;
        expect(std::equal(woven.image.rgba.begin(), woven.image.rgba.begin() + stride, pixels.image.rgba.begin()) &&
                   std::equal(woven.image.rgba.begin() + stride, woven.image.rgba.begin() + 2 * stride,
                              pixels.image.rgba.begin() + 2 * stride),
               "weave takes even rows from the first field");

        const auto good = buildPage(0xb4);
        auto moved = good;
        moved[0x0a] = 1;
        auto short_off = good;
        short_off[0x10] = 0x20;
        short_off[0x11] = short_off[0x12] = short_off[0x13] = 0;
        auto gap = good;
        gap[0x14] = 1;
        auto trailing = good;
        trailing.push_back(0);
        auto truncated = good;
        truncated.pop_back();
        truncated.resize(good.size() - 16);
        put16(truncated, 6, read16(truncated, 6) - 1);
        auto flags = good;
        put16(flags, 8, 0x0020);
        auto bad_reg = good;
        bad_reg[0xb4 + 0x478] = 0;
        for (const auto &blob : {moved, short_off, gap, trailing, truncated, flags, bad_reg})
            reject([&] { parseFontPage({blob.data(), blob.size()}); }, "malformed font page rejected");

        const auto raw = buildPage(0x44);
        const auto once = applyTexRelocatePrefix(0x1000, {raw.data(), raw.size()});
        const auto twice = applyTexRelocatePrefix(0x1000, {once.data(), once.size()});
        expect(twice == once, "relocate prefix is idempotent");
        expect(read16(once, 0x0a) == 0xa7fb && read32(once, 0x1c) == 0xffffffffU && read32(once, 0x10) == 0x1044,
               "relocate stores the packet pointer");
        expect(read16(once, 0) == read16(raw, 0) && read16(once, 2) == read16(raw, 2) && read16(once, 6) == read16(raw, 6),
               "relocation does not change the drawn size or qword count");
        auto flagged = raw;
        put16(flagged, 8, 0x0100);
        reject([&] { applyTexRelocatePrefix(0x1000, {flagged.data(), flagged.size()}); }, "block-table flags refused");
        expect(fontPageImageDbp(0x0044) == 0x474 && fontPageImageDbp(0x2000) == 0xb4 &&
                   fontPageImageDbp(0x2004) == 0x474 && fontPageImageDbp(0x0020) == -1,
               "DMA image DBP sites");
        reject([] { refuseFntHeaderLineHeight(); }, "FNT header +2 is not line height");
        std::vector<std::uint8_t> fnt(0x250 + sample.size());
        fnt[0] = 2;
        fnt[2] = 8;
        fnt[8] = 0x14;
        fnt[9] = 0x02;
        fnt[12] = 0x34;
        fnt[13] = 0x02;
        fnt[16] = 0x50;
        fnt[17] = 0x02;
        for (int i = 0; i < 256; ++i) {
            fnt[0x14 + i * 2] = 0xff;
            fnt[0x15 + i * 2] = 0xff;
        }
        fnt[0x14 + 'A' * 2] = 1;
        const int glyphs[2][8] = {{0, 0, 0, 0, 0, 0, 3, -1}, {65, 0, 1, 0, 1, 0, 4, -1}};
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 8; ++j) {
                const auto v = glyphs[i][j];
                fnt[0x214 + i * 16 + j * 2] = v & 255;
                fnt[0x215 + i * 16 + j * 2] = (v >> 8) & 255;
            }
        std::memcpy(fnt.data() + 0x250, sample.data(), sample.size());
        const auto file = decodeFontFile({fnt.data(), fnt.size()}, false);
        expect(samePixels(file.image, pixels.image) && file.uncovered == 0, "FNT suffix uses the font-page decoder");
        const auto woven_file = decodeFontFile({fnt.data(), fnt.size()}, true);
        expect(samePixels(woven_file.image, woven.image), "FNT weave follows the page");
        auto other = fnt;
        other[2] = 99;
        expect(fontMetrics({fnt.data(), fnt.size()}).widthAscii({'A'}) ==
                   fontMetrics({other.data(), other.size()}).widthAscii({'A'}),
               "header +2 does not change measured width");
        std::cout << "ui-font-page exact synthetic checks: " << checks << " passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
