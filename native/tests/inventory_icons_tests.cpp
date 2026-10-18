#include "champions/inventory_icons.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace icons = champions::inventory_icons;
namespace {

using Bytes = std::vector<std::uint8_t>;
int checks = 0;

void expect(bool value, const char *message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template <class Action> void rejects(Action action, const char *message) {
    bool caught = false;
    try { action(); } catch (const std::invalid_argument &) { caught = true; }
    expect(caught, message);
}
void put16(Bytes &bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = std::uint8_t(value);
    bytes[at + 1] = std::uint8_t(value >> 8);
}
void put32(Bytes &bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = std::uint8_t(value >> (i * 8));
}
void put64(Bytes &bytes, std::size_t at, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) bytes[at + i] = std::uint8_t(value >> (i * 8));
}
void append64(Bytes &bytes, std::uint64_t value) {
    const auto at = bytes.size();
    bytes.resize(at + 8);
    put64(bytes, at, value);
}
void tag(Bytes &bytes, std::uint64_t low, std::uint64_t regs) {
    append64(bytes, low);
    append64(bytes, regs);
}
void upload(Bytes &packet, std::uint32_t width, std::uint32_t height, const Bytes &pixels) {
    tag(packet, 4 | (1ULL << 60), 14);
    tag(packet, 1ULL << 48, 0x50);
    tag(packet, 0, 0x51);
    tag(packet, std::uint64_t(width) | (std::uint64_t(height) << 32), 0x52);
    tag(packet, 0, 0x53);
    tag(packet, (pixels.size() / 16) | (2ULL << 58), 0);
    packet.insert(packet.end(), pixels.begin(), pixels.end());
}
Bytes fixture(std::uint16_t flags = 0x44) {
    Bytes palette;
    for (unsigned i = 0; i < 256; ++i) {
        palette.insert(palette.end(), {std::uint8_t(i), std::uint8_t(i ^ 0x55), 19, std::uint8_t(i % 129)});
    }
    Bytes pixels(8192);
    pixels[0] = 8; pixels[2] = 17; pixels[17] = 16; pixels[273] = 24;
    Bytes packet;
    upload(packet, 16, 16, palette);
    upload(packet, 64, 32, pixels);
    Bytes data(128);
    put16(data, 0, 9); put16(data, 2, 6);
    put16(data, 6, std::uint16_t(packet.size() / 16));
    put16(data, 8, flags);
    put32(data, 12, 2 | (19 << 6) | (7 << 12) | (6 << 16));
    put32(data, 16, 128);
    data.insert(data.end(), packet.begin(), packet.end());
    return data;
}

void test_pixels_and_flags() {
    const auto data = fixture();
    const auto picture = icons::decode(data.data(), data.size());
    expect(picture.width == 9 && picture.height == 6 && picture.rgba.size() == 216, "crop dimensions");
    expect(picture.flags == 0x44 && picture.packet_offset == 128, "header identity");
    const std::array<std::uint8_t, 6> rows{{16, 24, 0, 0, 8, 0}};
    for (std::size_t y = 0; y < rows.size(); ++y) {
        expect(picture.rgba[y * 9 * 4] == rows[y], "field weave and CLUT permutation");
    }
    const std::array<std::uint8_t, 4> last{{9, std::uint8_t(9 ^ 0x55), 19, 9}};
    expect(std::equal(last.begin(), last.end(), picture.rgba.begin() + 8 * 4), "row crop landmark");
    for (std::uint16_t flag : {0x844, 0x1044, 0x1844}) {
        const auto other = fixture(flag);
        expect(icons::decode(other.data(), other.size()).rgba == picture.rgba, "flag variant pixels");
    }
    expect(icons::bottom_anchor_y(100, 30, 0x44) == 70, "plain bottom anchor");
    expect(icons::bottom_anchor_y(100, 30, 0x844) == 71, "bottom padding row");
    expect(icons::bottom_anchor_y(100, 30, 0x1044) == 70, "0x1000 does not move art");
    Bytes with_labels = data;
    tag(with_labels, 8 | (1ULL << 60), 14);
    for (unsigned i = 0; i < 8; ++i) tag(with_labels, i, 0x62);
    put16(with_labels, 6, std::uint16_t(picture.qwords + 9));
    expect(icons::decode(with_labels.data(), with_labels.size()).rgba == picture.rgba,
           "optional LABEL tail does not change pixels");
}

void test_addresses() {
    std::vector<bool> words(2048), bytes(8192);
    for (std::size_t y = 0; y < 32; ++y) for (std::size_t x = 0; x < 64; ++x) {
        const auto at = icons::gs32_word(x, y);
        expect(at < words.size() && !words[at], "GS32 page bijection");
        words[at] = true;
    }
    for (std::size_t y = 0; y < 64; ++y) for (std::size_t x = 0; x < 128; ++x) {
        const auto at = icons::gs8_byte(x, y);
        expect(at < bytes.size() && !bytes[at], "GS8 page bijection");
        bytes[at] = true;
    }
    expect(std::all_of(words.begin(), words.end(), [](bool seen) { return seen; }), "GS32 page coverage");
    expect(std::all_of(bytes.begin(), bytes.end(), [](bool seen) { return seen; }), "GS8 page coverage");
    const std::array<std::size_t, 6> landmarks{{0, 8, 33, 41, 96, 104}};
    for (std::size_t y = 0; y < landmarks.size(); ++y) expect(icons::gs8_byte(0, y) == landmarks[y], "GS8 landmark");
    expect(icons::gs32_word(0, 32) == 2048 && icons::gs8_byte(0, 64) == 8192, "page boundary");
}

void test_chunks_and_alpha() {
    const auto data = fixture();
    const auto span = icons::packet_span(data.data(), data.size());
    const auto chunks = icons::dma_chunks(data.data(), data.size());
    Bytes joined;
    for (const auto &chunk : chunks) {
        expect((chunk.tag_word & 0xffff) * 16 == chunk.bytes.size() && chunk.bytes.size() <= 1264, "DMA block size");
        joined.insert(joined.end(), chunk.bytes.begin(), chunk.bytes.end());
    }
    expect(joined == Bytes(data.begin() + span.offset, data.begin() + span.offset + span.qwords * 16), "DMA bytes");
    expect(chunks.front().tag_word == 0x2000004f && chunks.back().tag_word >> 28 == 6, "NEXT/RET tags");
    const Bytes rgba{10,20,30,0, 1,2,3,64, 4,5,6,128, 7,8,9,255};
    const Bytes shown{10,20,30,0, 1,2,3,128, 4,5,6,255, 7,8,9,255};
    expect(icons::display_rgba(rgba) == shown, "conventional alpha and hidden RGB");
    rejects([] { icons::display_rgba(Bytes{1,2,3}); }, "malformed RGBA refusal");
}

void test_refusals() {
    const auto original = fixture();
    Bytes short_data(original.begin(), original.begin() + 30);
    rejects([&] { icons::decode(short_data.data(), short_data.size()); }, "short header");
    Bytes truncated = original;
    truncated.pop_back();
    rejects([&] { icons::decode(truncated.data(), truncated.size()); }, "truncated packet");
    for (const auto &change : std::array<std::array<std::uint32_t, 3>, 6>{{
             {{0, 0, 2}}, {{2, 5, 2}}, {{8, 1, 2}}, {{10, 0xa7fb, 2}},
             {{16, 129, 4}}, {{16, 0xfffffff0U, 4}},
         }}) {
        Bytes data = original;
        if (change[2] == 2) put16(data, change[0], std::uint16_t(change[1]));
        else put32(data, change[0], change[1]);
        rejects([&] { icons::decode(data.data(), data.size()); }, "bad header refusal");
    }
    Bytes bad_psm = original;
    put64(bad_psm, 144, (1ULL << 48) | (19ULL << 56));
    rejects([&] { icons::decode(bad_psm.data(), bad_psm.size()); }, "unsupported upload PSM");
    Bytes bad_direction = original;
    put64(bad_direction, 128 + 4 * 16, 1);
    rejects([&] { icons::decode(bad_direction.data(), bad_direction.size()); }, "non-host transfer refusal");
    Bytes bad_register = original;
    put64(bad_register, 128 + 4 * 16 + 8, 0x70);
    rejects([&] { icons::decode(bad_register.data(), bad_register.size()); }, "unsupported GS register refusal");
    Bytes missing = original;
    put16(missing, 2, 128);
    put32(missing, 12, 2 | (19 << 6) | (7 << 12) | (7 << 16));
    rejects([&] { icons::decode(missing.data(), missing.size()); }, "unsupplied visible pixel");
}

} // namespace

int main() {
    try {
        test_pixels_and_flags();
        test_addresses();
        test_chunks_and_alpha();
        test_refusals();
        std::cout << "PASS inventory-icons " << checks << " checks\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL inventory-icons after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
