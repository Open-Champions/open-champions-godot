#include "champions/texture.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Buffer = std::vector<std::uint8_t>;
unsigned checks = 0;
void check(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}
void put(Buffer& b, std::size_t offset, std::uint64_t value, unsigned length) {
    for (unsigned i = 0; i < length; ++i) b.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
// Entirely authored pixels: distinct corners, rows and alpha values reveal crop,
// orientation, byte-order and alpha scaling errors without any game assets.
constexpr std::array<std::uint8_t, 64> pixels{{
    255,0,0,0, 0,255,0,32, 0,0,255,64, 255,255,0,128,
    255,0,255,255, 0,255,255,1, 127,63,31,127, 11,22,33,129,
    20,40,60,16, 80,100,120,48, 140,160,180,96, 200,220,240,112,
    9,8,7,128, 6,5,4,64, 3,2,1,32, 255,255,255,0
}};
Buffer fixture() {
    Buffer b(0x120, 0);
    put(b, 0, 4, 2); put(b, 2, 4, 2); put(b, 4, 1, 2);
    put(b, 6, 10, 2); put(b, 8, 0x420, 2);
    put(b, 12, 1 | (2 << 12) | (2 << 16), 4); put(b, 16, 0x80, 4);
    put(b, 0x80, (1ULL << 60) | 4, 8); put(b, 0x88, 0xe, 8);
    put(b, 0x90, 1ULL << 48, 8); put(b, 0x98, 0x50, 8);
    put(b, 0xa0, 0, 8); put(b, 0xa8, 0x51, 8);
    put(b, 0xb0, 4 | (4ULL << 32), 8); put(b, 0xb8, 0x52, 8);
    put(b, 0xc0, 0, 8); put(b, 0xc8, 0x53, 8);
    put(b, 0xd0, (2ULL << 58) | 0x8004, 8);
    std::copy(pixels.begin(), pixels.end(), b.begin() + 0xe0);
    return b;
}
champions::Texture32 decode(const Buffer& b) { return champions::decodeRtaTexture32({b.data(), b.size()}); }
void rejected(const Buffer& b, const char* description) {
    bool failed = false;
    try { (void)decode(b); } catch (const champions::FormatError&) { failed = true; }
    check(failed, description);
}
void mutation(std::size_t offset, std::uint64_t value, unsigned length, const char* description) {
    auto b = fixture(); put(b, offset, value, length); rejected(b, description);
}
void tests() {
    auto b = fixture();
    auto result = decode(b);
    check(result.width == 4 && result.height == 4 && result.flags == 0x420, "metadata");
    bool exact = result.rgba.size() == pixels.size();
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const unsigned expected = i % 4 == 3 ? std::min(255U, unsigned(pixels[i]) * 255U / 128U) : pixels[i];
        exact = exact && result.rgba[i] == expected;
    }
    check(exact, "all asymmetric pixels and scaled/clamped alpha");
    std::fill(b.begin(), b.end(), 0);
    check(result.rgba[0] == 255 && result.rgba[4] == 0 && result.rgba[5] == 255, "owned result survives input mutation");
    b = fixture(); b.resize(1616, 0);
    check(decode(b).rgba == result.rgba, "zero-padded small lump entry");
    b.back() = 1; rejected(b, "nonzero trailing bytes");
    b = fixture(); put(b, 0, 3, 2); put(b, 2, 2, 2);
    auto crop = decode(b);
    Buffer expected;
    for (unsigned y = 0; y < 2; ++y) expected.insert(expected.end(), result.rgba.begin() + y * 16, result.rgba.begin() + y * 16 + 12);
    check(crop.width == 3 && crop.height == 2 && crop.rgba == expected, "crop full surface to used dimensions without weaving");
    b = fixture();
    for (std::size_t size = 0; size < b.size(); ++size) rejected(Buffer(b.begin(), b.begin() + size), "every truncated prefix rejected");
    bool null_rejected = false;
    try { (void)champions::decodeRtaTexture32({nullptr, 0x120}); } catch (const champions::FormatError&) { null_rejected = true; }
    check(null_rejected, "null bytes rejected");
    rejected(Buffer(0x80 + 65535U * 16U + 1, 0), "input work bound");
    mutation(0, 0, 2, "zero width"); mutation(2, 1025, 2, "oversized height");
    mutation(4, 0, 2, "zero allocation"); mutation(4, 16385, 2, "allocation bound");
    mutation(6, 0, 2, "empty packet"); mutation(6, 65535, 2, "packet exceeds input");
    for (auto flags : {0x4, 0x2000, 0x80, 0x24, 0x21, 0x120, 0x8020}) mutation(8, flags, 2, "unsupported flags");
    mutation(10, 0xa7fb, 2, "runtime magic"); mutation(16, 0x40, 4, "world header offset");
    mutation(20, 1, 4, "runtime pointer"); mutation(127, 1, 1, "runtime header tail");
    mutation(12, 1 | (0x13 << 6) | (2 << 12) | (2 << 16), 4, "paletted TEX0");
    mutation(12, 0x80000001, 4, "unknown TEX0 fields");
    mutation(12, 1 | (11 << 12) | (2 << 16), 4, "surface dimension bound");
    mutation(12, 1 | (1 << 12) | (2 << 16), 4, "used width exceeds surface");
    mutation(12, (2 << 12) | (2 << 16), 4, "zero TBW");
    mutation(12, 1 | (7 << 12) | (2 << 16), 4, "TBW too narrow");
    mutation(0x80, (1ULL << 60) | (1ULL << 46) | 4, 8, "GIF PRE unsupported");
    mutation(0x80, (2ULL << 60) | 4, 8, "multiple registers unsupported");
    mutation(0x88, 0xf, 8, "not A+D");
    mutation(0x80, (1ULL << 60) | 0x7fff, 8, "packed payload bound");
    mutation(0x98, 0x54, 8, "unknown register");
    mutation(0xa8, 0x50, 8, "repeated buffer");
    mutation(0x98, 0x53, 8, "direction before setup");
    mutation(0xc8, 0x62, 8, "missing direction");
    mutation(0xc0, 1, 8, "wrong transfer direction");
    mutation(0x90, (1ULL << 48) | (1ULL << 32), 8, "relocated DBP");
    mutation(0x90, 2ULL << 48, 8, "DBW mismatch");
    mutation(0x90, (1ULL << 48) | 1, 8, "source buffer forbidden");
    mutation(0xa0, 1, 8, "source coordinates forbidden");
    mutation(0xa0, 1ULL << 32, 8, "destination outside surface");
    mutation(0xb0, 4ULL << 32, 8, "zero transfer width");
    mutation(0xb0, 1025 | (4ULL << 32), 8, "transfer dimension bound");
    mutation(0xb0, 4 | (4ULL << 32) | (1ULL << 12), 8, "TRXREG reserved fields");
    mutation(0xd0, (1ULL << 58) | 4, 8, "REGLIST unsupported");
    mutation(0xd0, (3ULL << 58) | 4, 8, "disabled mode unsupported");
    mutation(0xd0, (2ULL << 58) | (1ULL << 60) | 4, 8, "IMAGE register count");
    mutation(0xd8, 0xe, 8, "IMAGE register list");
    mutation(0xd0, (2ULL << 58) | 3, 8, "IMAGE count mismatch");
    mutation(0xd0, (2ULL << 58) | 5, 8, "IMAGE payload bound");
    b = fixture(); b.resize(0xd0); put(b, 6, 5, 2); rejected(b, "missing image");
    b = fixture(); put(b, 0xb0, 2 | (2ULL << 32), 8); put(b, 0xd0, (2ULL << 58) | 1, 8); b.resize(0xf0); put(b, 6, 7, 2);
    rejected(b, "partial upload must not invent uninitialized pixels");
    put(b, 0, 2, 2); put(b, 2, 2, 2); put(b, 0xa0, (1ULL << 32) | (1ULL << 48), 8);
    rejected(b, "offset upload must cover used origin");
    b = fixture(); b.resize(0x170, 0); put(b, 6, 15, 2);
    put(b, 0x120, (2ULL << 58) | 4, 8); rejected(b, "second image forbidden");
    b = fixture(); b.resize(0x140, 0); put(b, 6, 12, 2);
    put(b, 0x120, (1ULL << 60) | 1, 8); put(b, 0x128, 0xe, 8); put(b, 0x138, 0x50, 8);
    rejected(b, "transfer state after image forbidden");
    put(b, 0x138, 0x62, 8);
    check(decode(b).rgba == result.rgba, "harmless trailing LABEL accepted");
    b = fixture(); put(b, 0, 1, 2); put(b, 2, 1, 2); put(b, 0xb0, 1 | (1ULL << 32), 8);
    put(b, 0xd0, (2ULL << 58) | 1, 8); b.resize(0xf0); put(b, 6, 7, 2);
    check(decode(b).rgba == Buffer({255,0,0,0}), "single pixel ignores final IMAGE qword padding");
}
// A palette uses the authored RGBA colours above; a deliberately nonsequential
// index pattern exposes nibble reversal and accidental 8-bit palette swizzling.
constexpr std::array<unsigned, 16> indices{{0,15,1,14,2,13,3,12,4,11,5,10,6,9,7,8}};
Buffer paletteFixture() {
    auto b = fixture(); b.resize(0x190, 0);
    put(b, 4, 2, 2); put(b, 6, 17, 2); put(b, 8, 0x2010, 2);
    put(b, 12, 1 | (0x14 << 6) | (2 << 12) | (2 << 16), 4);
    put(b, 0xb0, 8 | (2ULL << 32), 8);
    put(b, 0xd0, (2ULL << 58) | 4, 8);
    put(b, 0x120, (1ULL << 60) | 4, 8); put(b, 0x128, 0xe, 8);
    put(b, 0x130, (1ULL << 48) | (0x14ULL << 56), 8); put(b, 0x138, 0x50, 8);
    put(b, 0x148, 0x51, 8);
    put(b, 0x150, 4 | (4ULL << 32), 8); put(b, 0x158, 0x52, 8);
    put(b, 0x168, 0x53, 8); put(b, 0x170, (2ULL << 58) | 0x8001, 8);
    for (std::size_t i = 0; i < indices.size(); i += 2) b[0x180 + i / 2] = static_cast<std::uint8_t>(indices[i] | (indices[i + 1] << 4));
    return b;
}
champions::Texture32 decodeAny(const Buffer& b) { return champions::decodeRtaTexture({b.data(), b.size()}); }
void rejectedAny(const Buffer& b, const char* description) {
    bool failed = false;
    try { (void)decodeAny(b); } catch (const champions::FormatError&) { failed = true; }
    check(failed, description);
}
void paletteMutation(std::size_t offset, std::uint64_t value, unsigned length, const char* description) {
    auto b = paletteFixture(); put(b, offset, value, length); rejectedAny(b, description);
}
Buffer paletteExpected(const std::vector<unsigned>& order) {
    Buffer output;
    for (auto index : order) {
        for (unsigned c = 0; c < 4; ++c) {
            const auto raw = unsigned(pixels[index * 4 + c]);
            output.push_back(static_cast<std::uint8_t>(c == 3 ? std::min(255U, raw * 255U / 128U) : raw));
        }
    }
    return output;
}
void paletteTests() {
    auto b = paletteFixture();
    auto result = decodeAny(b);
    const auto expected = paletteExpected(std::vector<unsigned>(indices.begin(), indices.end()));
    check(result.width == 4 && result.height == 4 && result.flags == 0x2010, "plain4 metadata");
    check(result.rgba == expected, "all16 palette entries, low nibble first, plain palette order, scaled/clamped alpha");
    rejected(b, "strict32 API still rejects supported plain4");
    check(decodeAny(fixture()).rgba == decode(fixture()).rgba, "general API preserves32 pixels");
    std::fill(b.begin(), b.end(), 0);
    check(result.rgba == expected, "palette result owns its pixels");
    b = paletteFixture(); b.resize(1616, 0);
    check(decodeAny(b).rgba == expected, "plain4 padded small entry");
    b.back() = 1; rejectedAny(b, "plain4 nonzero trailing data");
    b = paletteFixture(); put(b, 0, 3, 2); put(b, 2, 2, 2);
    check(decodeAny(b).rgba == paletteExpected({0,15,1,2,13,3}), "plain4 crop respects upload stride");
    // Odd-width transfers pack continuously across row boundaries, including a
    // last unused high nibble. Palette size does not constrain image dimensions.
    put(b, 2, 3, 2); put(b, 0x150, 3 | (3ULL << 32), 8);
    check(decodeAny(b).rgba == paletteExpected({0,15,1,14,2,13,3,12,4}), "odd row width nibble continuity");
    b = paletteFixture(); put(b, 0, 1, 2); put(b, 2, 1, 2);
    put(b, 12, 1 | (0x14 << 6), 4); put(b, 0x150, 1 | (1ULL << 32), 8); b[0x180] = 0xf7;
    check(decodeAny(b).rgba == paletteExpected({7}), "one texel ignores final high nibble and IMAGE padding");
    b = paletteFixture(); put(b, 0x90, 2ULL << 48, 8);
    check(decodeAny(b).rgba == expected, "palette buffer stride independent of image TBW");
    b = paletteFixture();
    for (std::size_t size = 0; size < b.size(); ++size) rejectedAny(Buffer(b.begin(), b.begin() + size), "all plain4 truncated prefixes rejected");
    bool null_rejected = false;
    try { (void)champions::decodeRtaTexture({nullptr, b.size()}); } catch (const champions::FormatError&) { null_rejected = true; }
    check(null_rejected, "general API null input");
    rejectedAny(Buffer(0x80 + 65535U * 16U + 1, 0), "plain4 input work bound");
    for (auto flags : {0x2004,0x2020,0x2080,0x2001,0x2100,0xa000,0x4}) paletteMutation(8, flags, 2, "plain4 unsupported storage flags");
    paletteMutation(10, 0xa7fb, 2, "plain4 relocated magic");
    paletteMutation(16, 0x40, 4, "plain4 world packet offset");
    paletteMutation(20, 1, 4, "plain4 runtime state");
    paletteMutation(12, 1 | (0x13 << 6) | (2 << 12) | (2 << 16), 4, "4-bit flags with8-bit format");
    paletteMutation(12, 1 | (2 << 12) | (2 << 16), 4, "4-bit flags with32-bit format");
    paletteMutation(0x90, (1ULL << 48) | (0x14ULL << 56), 8, "palette must be32-bit");
    paletteMutation(0x90, 0, 8, "palette zero buffer width");
    paletteMutation(0x90, (1ULL << 48) | (1ULL << 32), 8, "palette relocated base");
    paletteMutation(0xa0, 1ULL << 32, 8, "palette offset origin");
    paletteMutation(0xb0, 16 | (1ULL << 32), 8, "palette exact8x2 geometry");
    paletteMutation(0xc8, 0x62, 8, "palette missing direction");
    paletteMutation(0xd0, (2ULL << 58) | 3, 8, "palette short payload count");
    paletteMutation(0xd0, (2ULL << 58) | 5, 8, "palette long payload count");
    paletteMutation(0xd0, (2ULL << 58) | (1ULL << 60) | 4, 8, "palette IMAGE register fields");
    paletteMutation(0x130, 1ULL << 48, 8, "4-bit sent-as32-bit unsupported");
    paletteMutation(0x130, (1ULL << 48) | (0x13ULL << 56), 8, "8-bit image unsupported");
    paletteMutation(0x130, (2ULL << 48) | (0x14ULL << 56), 8, "4-bit image buffer mismatch");
    paletteMutation(0x130, (1ULL << 48) | (0x14ULL << 56) | (1ULL << 32), 8, "4-bit image relocated base");
    paletteMutation(0x138, 0x62, 8, "image cannot reuse palette setup");
    b = paletteFixture(); put(b, 0x148, 0x62, 8);
    check(decodeAny(b).rgba == expected, "image inherits validated palette position when TRXPOS is omitted");
    b = paletteFixture(); b.erase(b.begin() + 0x140, b.begin() + 0x150);
    put(b, 0x120, (1ULL << 60) | 3, 8); put(b, 6, 16, 2);
    check(decodeAny(b).rgba == expected, "three-register image setup reuses persistent GS position");
    paletteMutation(0x158, 0x62, 8, "image must provide fresh extent");
    paletteMutation(0x168, 0x62, 8, "image must provide fresh direction");
    paletteMutation(0x148, 0x50, 8, "repeated image buffer before IMAGE");
    paletteMutation(0x138, 0x51, 8, "position before new buffer");
    paletteMutation(0x140, 1, 8, "4-bit source coordinate");
    paletteMutation(0x140, 1ULL << 32, 8, "4-bit destination overflow");
    paletteMutation(0x150, 2 | (4ULL << 32), 8, "4-bit partial used coverage");
    paletteMutation(0x150, 1025 | (4ULL << 32), 8, "4-bit dimension bound");
    paletteMutation(0x160, 1, 8, "4-bit wrong direction");
    paletteMutation(0x168, 0x54, 8, "4-bit unknown GS register");
    paletteMutation(0x170, (2ULL << 58), 8, "4-bit empty IMAGE");
    paletteMutation(0x170, (2ULL << 58) | 2, 8, "4-bit IMAGE exceeds packet");
    paletteMutation(0x170, (1ULL << 58) | 1, 8, "4-bit REGLIST unsupported");
    // The sampling extent may be wider than TBW. Only the used/uploaded
    // rectangle must fit the buffer stride; the absent padding is not read.
    b = paletteFixture(); put(b, 0, 3, 2); put(b, 2, 2, 2);
    put(b, 12, 1 | (0x14 << 6) | (7 << 12) | (2 << 16), 4);
    check(decodeAny(b).rgba == paletteExpected({0,15,1,2,13,3}), "plain4 sampling surface may exceed actual buffer width");
    put(b, 0, 65, 2); rejectedAny(b, "used width must fit actual buffer stride");
    put(b, 0, 3, 2); put(b, 0x150, 65 | (2ULL << 32), 8);
    rejectedAny(b, "upload width must fit actual buffer stride");
    b = paletteFixture(); b.resize(0x120); put(b, 6, 10, 2);
    rejectedAny(b, "palette-only packet rejected");
    b = paletteFixture(); b.erase(b.begin() + 0x80, b.begin() + 0x120); put(b, 6, 7, 2);
    rejectedAny(b, "image without palette rejected");
    b = paletteFixture(); b.resize(0x1b0, 0); put(b, 6, 19, 2);
    put(b, 0x190, (2ULL << 58) | 1, 8);
    rejectedAny(b, "third IMAGE rejected");
    put(b, 0x190, (1ULL << 60) | 1, 8); put(b, 0x198, 0xe, 8); put(b, 0x1a8, 0x50, 8);
    rejectedAny(b, "third destination buffer rejected");
    put(b, 0x1a8, 0x62, 8);
    check(decodeAny(b).rgba == expected, "trailing palette LABEL is harmless");
}
void write(const fs::path& path, const Buffer& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("cannot write authored texture fixture");
}
int run(const std::vector<fs::path>& args) {
    try {
        if (args.size() == 2 && args[0] == "--write-fixtures") {
            fs::create_directories(args[1]);
            auto b = fixture(); write(args[1] / "asymmetric32.tex", b);
            put(b, 8, 4, 2); put(b, 12, 1 | (0x13 << 6) | (2 << 12) | (2 << 16), 4);
            write(args[1] / "unsupported-paletted.tex", b);
            write(args[1] / "asymmetric4.tex", paletteFixture());
            return 0;
        }
        if (!args.empty()) throw std::runtime_error("unexpected test arguments");
        tests(); paletteTests(); std::cout << checks << " texture checks passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<fs::path> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return run(args);
}
#else
int main(int argc, char** argv) {
    std::vector<fs::path> args;
    for (int i = 1; i < argc; ++i) args.push_back(fs::u8path(argv[i]));
    return run(args);
}
#endif
