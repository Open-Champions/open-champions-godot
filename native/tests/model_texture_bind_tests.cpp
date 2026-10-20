// Authored synthetic model and texture (the same shapes as model_tests.cpp
// make() and texture_tests.cpp fixture()); no game bytes.
#include "champions/model_texture_bind.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Buffer = std::vector<std::uint8_t>;
using Row = std::array<int, 4>;
unsigned checks = 0;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
void put(Buffer &b, std::size_t at, std::uint64_t value, unsigned size) {
  for (unsigned i = 0; i < size; ++i)
    b.at(at + i) = static_cast<std::uint8_t>(value >> (8 * i));
}
void word(Buffer &b, std::uint32_t value) {
  const auto at = b.size();
  b.resize(at + 4);
  put(b, at, value, 4);
}
std::size_t unpack(Buffer &b, unsigned cmd, unsigned addr, bool uns,
                   const std::vector<Row> &rows) {
  const auto at = b.size();
  word(b, (cmd << 24) | (unsigned(rows.size()) << 16) | 0x8000 |
              (uns ? 0x4000 : 0) | addr);
  const unsigned width = 4U >> (cmd & 3), components = ((cmd >> 2) & 3) + 1;
  for (const auto &row : rows)
    for (unsigned c = 0; c < components; ++c) {
      const auto out = b.size();
      b.resize(out + width);
      put(b, out, static_cast<std::uint32_t>(row[c]), width);
    }
  while (b.size() % 4)
    b.push_back(0);
  return at;
}
struct Fixture {
  Buffer bytes;
  std::size_t pos, norm, slots, weights, copies, tag, mscal, uv, shell;
};
Fixture make(unsigned strip = 3, bool copied = false,
             const std::vector<Row> &weight_rows = {{0, 0, 255, 3}}) {
  Fixture f{};
  auto &b = f.bytes;
  b.resize(0x80, 0);
  put(b, 0, 1, 8);
  put(b, 0x12, 1, 1);
  put(b, 0x13, 255, 1);
  put(b, 0x25, 4, 1);
  put(b, 0x26, 1, 1);
  put(b, 0x28, 0x80, 4);
  word(b, 0x04000003);
  f.pos = unpack(b, 0x69, 0, false,
                 {{0, 0, 0, 0}, {160, 0, 0, 0}, {0, 160, 80, 0}});
  f.norm = unpack(b, 0x6a, 3, false,
                  {{0, 0, 127, 0}, {0, 0, 127, 0}, {0, 0, 127, 0}});
  f.slots = unpack(
      b, 0x69, 6, true,
      {{3, 0, 3, 0}, {3, 0, 3, 0}, {0, 0, 0, 0}, {3, 0, 0, 0}, {6, 0, 0, 0}});
  f.weights = unpack(b, 0x6e, 11, true, weight_rows);
  std::vector<Row> copies{{0, 0, 0, 0}};
  if (copied) {
    copies[0][0] = 1;
    copies.push_back({0, 9, 3, 12});
  }
  if (strip == 83) {
    copies = {{40, 0, 0, 0}};
    for (unsigned i = 3; i < 83; i += 2)
      copies.push_back({int((i % 3) * 3), int(i * 3), int(((i + 1) % 3) * 3),
                        int((i + 1) * 3)});
  }
  const auto copy_address = 11 + unsigned(weight_rows.size());
  f.copies = unpack(b, 0x6d, copy_address, true, copies);
  const std::uint64_t low =
      strip | 0x8000ULL | (1ULL << 46) | (0x15cULL << 47) | (3ULL << 60);
  f.tag = unpack(b, 0x6c, copy_address + unsigned(copies.size()), false,
                 {{int(low & 0xffffffffU), int(low >> 32), 0x413, 0}});
  f.mscal = b.size();
  word(b, 0x14000042);
  word(b, 0x01000103);
  std::vector<Row> uv;
  for (unsigned i = 0; i < strip; ++i)
    uv.push_back({int(i * 16), int(i * 32), 0, 0});
  f.uv = unpack(b, 0x65, 0x81, true, uv);
  word(b, 0x01000101);
  while (b.size() % 16)
    b.push_back(0);
  put(b, 0x2c, b.size(), 4);
  return f;
}
constexpr std::array<std::uint8_t, 64> pixels{{
    255,0,0,0, 0,255,0,32, 0,0,255,64, 255,255,0,128,
    255,0,255,255, 0,255,255,1, 127,63,31,127, 11,22,33,129,
    20,40,60,16, 80,100,120,48, 140,160,180,96, 200,220,240,112,
    9,8,7,128, 6,5,4,64, 3,2,1,32, 255,255,255,0
}};
Buffer texture_fixture() {
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
} // namespace
int main() {
  namespace MT = champions::model_texture;
  try {
    check(MT::companion_texture_name("Beast.VIF") == "Beast.tex" &&
              MT::companion_texture_name("beast.anm").empty(),
          "the companion texture keeps the model's base name");
    const auto model = make();
    const auto tex = texture_fixture();
    const auto bound = MT::bind({model.bytes.data(), model.bytes.size()},
                                {tex.data(), tex.size()});
    check(bound.texture.width == 4 && bound.texture.height == 4,
          "the texture decodes through the released decoder");
    check(bound.meshes.size() == 1 && bound.meshes[0].uv.size() % 3 == 0 &&
              !bound.meshes[0].uv.empty(),
          "every corner has a coordinate");
    check(bound.meshes[0].blend == MT::Blend::On,
          "PRIM with ABE set turns blending on");
    const auto decoded = champions::decodeRtaModel({model.bytes.data(), model.bytes.size()});
    bool same = true;
    for (std::size_t i = 0; i < decoded.meshes[0].corners.size(); ++i) {
      const auto &c = decoded.meshes[0].corners[i];
      same = same && std::abs(bound.meshes[0].uv[i][0] - c.uv_texels[0] / 4.0) < 1e-12 &&
             std::abs(bound.meshes[0].uv[i][1] - c.uv_texels[1] / 4.0) < 1e-12;
    }
    check(same, "UVs are texel coordinates divided by the texture size");
    check(bound.alpha1 == decoded.alpha1, "header +0x18 is reported unchanged");
    bool refused = false;
    try {
      (void)MT::bind({model.bytes.data(), model.bytes.size()}, {tex.data(), 16});
    } catch (const champions::FormatError &) {
      refused = true;
    }
    check(refused, "a malformed texture refuses");
    std::cout << "model texture bind checks: " << checks << "; failures: 0\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "model texture bind failure: " << e.what() << '\n';
    return 1;
  }
}
