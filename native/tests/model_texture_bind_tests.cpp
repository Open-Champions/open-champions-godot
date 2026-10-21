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
// One flag-0x10 packet with both GIF tags and program 0x44 (model_tests.cpp
// makeForm(true, false): research model-texture.md, program 0x44 row of the
// GIF tag table). Header +0x18 = 9, +0x20 = 0x80.
Fixture make_two_pass() {
  Fixture f{};
  auto &b = f.bytes;
  b.resize(0x80, 0);
  put(b, 0, 1, 8);
  put(b, 0x10, 0x10, 2);
  put(b, 0x12, 1, 1);
  put(b, 0x13, 255, 1);
  put(b, 0x18, 9, 8);
  put(b, 0x20, 0x80, 1);
  put(b, 0x25, 4, 1);
  put(b, 0x26, 1, 1);
  put(b, 0x28, 0x80, 4);
  word(b, 0x04000003);
  f.pos = unpack(b, 0x69, 0, false,
                 {{0, 0, 0, 0}, {160, 0, 0, 0}, {0, 160, 80, 0}});
  f.norm = unpack(b, 0x6a, 3, false,
                  {{0, 0, 127, 0}, {0, 0, 127, 0}, {0, 0, 127, 0}});
  const unsigned strip = 3, stride = 4, base = 1;
  f.slots = unpack(b, 0x69, 6, true,
                   {{int(base + stride), 0, int(base + stride), 0},
                    {int(base + stride), 0, int(base + stride), 0},
                    {int(base), 0, 0, 0},
                    {int(base + stride), 0, 0, 0},
                    {int(base + 2 * stride), 0, 0, 0}});
  f.weights = unpack(b, 0x6e, 11, true,
                     {{0, 0, 255, 1}, {4, 0, 255, 1}, {252, 0, 255, 1}});
  f.copies = unpack(b, 0x6d, 14, true, {{0, 0, 0, 0}});
  auto giftag = [&](unsigned prim, unsigned regs) {
    const std::uint64_t lo = strip | 0x8000ULL | (1ULL << 46) |
                             (std::uint64_t(prim) << 47) |
                             (std::uint64_t(stride) << 60);
    return Row{int(lo & 0xffffffffU), int(lo >> 32), int(regs), 0};
  };
  f.tag = unpack(b, 0x6c, 15, false,
                 {giftag(0x15c, 0x41f3), giftag(0x35c, 0x413f)});
  f.mscal = b.size();
  word(b, 0x14000044);
  word(b, 0x01000100 | stride);
  std::vector<Row> uv;
  for (unsigned i = 0; i < strip; ++i)
    uv.push_back({int(i * 16), int(i * 32 + 8), 0, 0});
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
    auto mode = MT::alpha_mode(1);
    check(mode.equation == 1 && mode.fix == 0, "ALPHA 1 is the reverse mix");
    check(MT::alpha_mode(2).equation == 2, "ALPHA 2 drops the destination");
    check(MT::alpha_mode(9).equation == 9, "ALPHA 9 adds destination times As");
    check(MT::alpha_mode(0x2a).equation == 0x2a, "ALPHA 0x2a replaces");
    check(MT::alpha_mode(0x44).equation == 0x44, "ALPHA 0x44 is the usual mix");
    check(MT::alpha_mode(0x48).equation == 0x48, "ALPHA 0x48 adds source times As");
    mode = MT::alpha_mode((0x80ULL << 32) | 0x64);
    check(mode.equation == 0x64 && mode.fix == 0x80,
          "0x64 plus FIX keeps the FIX byte");
    mode = MT::alpha_mode((40ULL << 32) | 0x68);
    check(mode.equation == 0x68 && mode.fix == 40, "0x68 plus FIX is the add form");
    check(MT::alpha_mode(0x8000000029ULL).equation == 0 &&
              MT::alpha_mode(0xac00000029ULL).equation == 0 &&
              MT::alpha_mode((0x100ULL << 32) | 0x64).equation == 0,
          "registers outside the documented rows stay undocumented");
    check(MT::alpha_mode((0x81ULL << 32) | 0x64).equation == 0 &&
              MT::alpha_mode((0xffULL << 32) | 0x68).equation == 0 &&
              MT::alpha_mode((0x80ULL << 32) | 0x68).equation == 0x68,
          "a FIX above 0x80 is refused, 0x80 is accepted");
    const std::array<double, 3> src{{10, 20, 30}}, dst{{1, 2, 3}};
    auto blended = MT::blend_rgb(1, 0, src, dst, 0.25, true);
    check(blended && (*blended)[0] == 10 * 0.75 + 1 * 0.25 &&
              (*blended)[2] == 30 * 0.75 + 3 * 0.25,
          "equation 1 is src (1-As) + dst As");
    blended = MT::blend_rgb(9, 0, src, dst, 0.5, true);
    check(blended && (*blended)[1] == 20 + 2 * 0.5, "equation 9 is src + dst As");
    blended = MT::blend_rgb(0x48, 0, src, dst, 0.5, true);
    check(blended && (*blended)[0] == 1 + 10 * 0.5, "equation 0x48 is dst + src As");
    blended = MT::blend_rgb(2, 0, src, dst, 0.25, true);
    check(blended && (*blended)[0] == 7.5 && (*blended)[1] == 15,
          "equation 2 does not add the destination");
    blended = MT::blend_rgb(0x44, 0, src, dst, 0.25, true);
    check(blended && (*blended)[0] == 10 * 0.25 + 1 * 0.75,
          "equation 0x44 is src As + dst (1-As)");
    blended = MT::blend_rgb(0x2a, 0, src, dst, 0.9, true);
    check(blended && *blended == src, "equation 0x2a is the source");
    blended = MT::blend_rgb(0x64, 0.25, src, dst, 0.9, true);
    check(blended && (*blended)[0] == 10 * 0.25 + 1 * 0.75,
          "equation 0x64 weights by FIX");
    blended = MT::blend_rgb(0x68, 40.0 / 128.0, src, dst, 0.9, true);
    check(blended && (*blended)[0] == 1 + 10 * (40.0 / 128.0),
          "equation 0x68 adds source times FIX");
    blended = MT::blend_rgb(9, 0, src, dst, 1, false);
    check(blended && *blended == src, "a clear PRIM blend bit replaces");
    check(!MT::blend_rgb(0x29, 1, src, dst, 0, true),
          "an undocumented equation is not evaluated");
    const auto centre = MT::environment_uv(0, 0, 128, 128);
    const auto right = MT::environment_uv(1, 0, 128, 128);
    const auto down = MT::environment_uv(0, 1, 128, 128);
    const double edge = (192.0 + 64.0 * 0.99) / 128.0;
    check(centre[0] == 1.5 && centre[1] == 1.5 && right[0] == edge &&
              right[1] == 1.5 && down[0] == 1.5 && down[1] == edge,
          "environment UV is (192 + 64 x 0.99 n) / image size");
    auto header = model.bytes;
    put(header, 0x10, 0x10, 2);
    put(header, 0x18, 9, 8);
    put(header, 0x20, 0x80, 1);
    const auto flagged = MT::bind({header.data(), header.size()}, {tex.data(), tex.size()});
    check(flagged.two_pass && flagged.fix2 == 0x80 && flagged.mode.equation == 9 &&
              !flagged.meshes[0].environment_tag &&
              !flagged.meshes[0].environment &&
              flagged.two_pass_mesh_without_env_tag &&
              !flagged.env_tag_without_two_pass,
          "a flag-0x10 mesh with one tag stays on the single pass and is counted");
    auto two = make_two_pass();
    const auto both = MT::bind({two.bytes.data(), two.bytes.size()}, {tex.data(), tex.size()});
    check(both.two_pass && both.meshes.size() == 1 &&
              both.meshes[0].environment_tag && both.meshes[0].environment &&
              both.meshes[0].environment_blend == MT::Blend::On &&
              both.meshes[0].blend == MT::Blend::On &&
              !both.two_pass_mesh_without_env_tag &&
              !both.env_tag_without_two_pass && !both.fix2_above_documented,
          "flag 0x10 with both tags draws the environment pass");
    put(two.bytes, 0x20, 0x81, 1);
    const auto heavy = MT::bind({two.bytes.data(), two.bytes.size()}, {tex.data(), tex.size()});
    check(heavy.fix2_above_documented && heavy.meshes[0].environment_tag &&
              !heavy.meshes[0].environment &&
              heavy.meshes[0].environment_blend == MT::Blend::Unstated,
          "header +0x20 above 0x80 refuses the environment pass");
    put(two.bytes, 0x20, 0x80, 1);
    put(two.bytes, 0x10, 0, 2);
    refused = false;
    try {
      (void)MT::bind({two.bytes.data(), two.bytes.size()}, {tex.data(), tex.size()});
    } catch (const champions::FormatError &) {
      refused = true;
    }
    check(refused, "a second tag without flag 0x10 is refused by the decoder");
    put(header, 0x10, 0, 2);
    put(header, 0x18, 0x48, 8);
    const auto additive = MT::bind({header.data(), header.size()}, {tex.data(), tex.size()});
    check(!additive.two_pass && additive.mode.equation == 0x48,
          "header 0x48 is the documented add equation");
    std::cout << "model texture bind checks: " << checks << "; failures: 0\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "model texture bind failure: " << e.what() << '\n';
    return 1;
  }
}
