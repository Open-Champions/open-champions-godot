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
            return 0;
        }
        if (!args.empty()) throw std::runtime_error("unexpected test arguments");
        tests(); std::cout << checks << " texture checks passed\n"; return 0;
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
