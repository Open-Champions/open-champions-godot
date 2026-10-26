#include "champions/model.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
namespace fs = std::filesystem;
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
// Authored two-pass/shell packets from the published layout. The second UV
// is computed by the vertex program; the file contains only the base UV list.
Fixture makeForm(bool two_pass, bool shell, unsigned strip = 3,
                 bool copied = false) {
  Fixture f{};
  auto &b = f.bytes;
  b.resize(0x80, 0);
  put(b, 0, 1, 8);
  put(b, 0x10, (two_pass ? 0x10 : 0) | (shell ? 0x20 : 0), 2);
  put(b, 0x12, 1, 1);
  put(b, 0x13, 255, 1);
  put(b, 0x18, 0x8000000029ULL, 8);
  put(b, 0x20, 58, 1);
  put(b, 0x25, 4, 1);
  put(b, 0x26, 1, 1);
  put(b, 0x28, 0x80, 4);
  word(b, 0x04000003);
  f.pos = unpack(b, 0x69, 0, false,
                 {{0, 0, 0, 0}, {160, 0, 0, 0}, {0, 160, 80, 0}});
  f.norm = unpack(b, 0x6a, 3, false,
                  {{0, 0, 127, 0}, {0, 0, 127, 0}, {0, 0, 127, 0}});
  const unsigned stride = two_pass ? 4 : 3, base = two_pass ? 1 : 0;
  std::vector<Row> copies{{copied ? 1 : 0, 0, 0, 0}};
  if (copied)
    copies.push_back({int(base), int(base + stride * 3), int(base + stride),
                      int(base + stride * 4)});
  if (strip == (two_pass ? 62U : 83U)) {
    copies = {{40, 0, 0, 0}};
    copies.resize(1);
    unsigned n = 0;
    for (unsigned i = 3; i < strip; i += 2) {
      copies.push_back({int(base + (i % 3) * stride), int(base + i * stride),
                        int(base + ((i + 1) % 3) * stride),
                        int(base + std::min(i + 1, strip - 1) * stride)});
      ++n;
    }
    copies[0][0] = int(n);
  }
  const unsigned tag_address = 12 + unsigned(copies.size()),
                 shell_address = tag_address + (two_pass ? 2 : 1);
  f.slots = unpack(b, 0x69, 6, true,
                   {{int(base + stride), shell ? int(shell_address) : 0,
                     int(base + stride), 0},
                    {int(base + stride), 0, int(base + stride), 0},
                    {int(base), 0, 0, 0},
                    {int(base + stride), 0, 0, 0},
                    {int(base + 2 * stride), 0, 0, 0}});
  f.weights = unpack(b, 0x6e, 11, true,
                     {{0, 0, 255, 1}, {4, 0, 255, 1}, {252, 0, 255, 1}});
  // Three weight rows move the copies and GIF tags two qwords forward.
  const unsigned actual_tag_address = tag_address + 2,
                 actual_shell_address = shell_address + 2;
  if (shell)
    put(b, f.slots + 6, actual_shell_address, 2);
  f.copies = unpack(b, 0x6d, 14, true, copies);
  auto giftag = [&](unsigned prim, unsigned regs) {
    const std::uint64_t lo = strip | 0x8000ULL | (1ULL << 46) |
                             (std::uint64_t(prim) << 47) |
                             (std::uint64_t(stride) << 60);
    return Row{int(lo & 0xffffffffU), int(lo >> 32), int(regs), 0};
  };
  std::vector<Row> tags{giftag(0x15c, two_pass ? 0x41f3 : 0x413)};
  if (two_pass)
    tags.push_back(giftag(0x35c, 0x413f));
  f.tag = unpack(b, 0x6c, actual_tag_address, false, tags);
  if (shell)
    f.shell = unpack(b, 0x6a, actual_shell_address, false,
                     {{127, 0, 0, 0}, {0, -128, 0, 0}, {0, 0, -64, 0}});
  f.mscal = b.size();
  word(b, two_pass ? 0x14000044 : 0x14000042);
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
// Forty-one source vertices and one rigid run place the GIF block at qword127.
// A single tag ends exactly at128; two tags exceed the input-memory boundary.
Fixture makeInputLimit(bool two_tags) {
  Fixture f{};
  auto &b = f.bytes;
  b.resize(0x80, 0);
  put(b, 0, 1, 8);
  put(b, 0x10, 0x10, 2);
  put(b, 0x12, 1, 1);
  put(b, 0x13, 255, 1);
  put(b, 0x25, 4, 1);
  put(b, 0x26, 1, 1);
  put(b, 0x28, 0x80, 4);
  word(b, 0x04000029);
  std::vector<Row> positions(41, {0, 160, 80, 0});
  positions[0] = {0, 0, 0, 0};
  positions[1] = {160, 0, 0, 0};
  f.pos = unpack(b, 0x69, 0, false, positions);
  f.norm = unpack(b, 0x6a, 41, false, std::vector<Row>(41, {0, 0, 127, 0}));
  const unsigned stride = two_tags ? 4 : 3, base = two_tags ? 1 : 0;
  std::vector<Row> slots(2, {int(base + stride), 0, int(base + stride), 0});
  for (unsigned i = 0; i < 41; ++i)
    slots.push_back({int(base + stride * std::min(i, 2U)), 0, 0, 0});
  f.slots = unpack(b, 0x69, 82, true, slots);
  f.weights = unpack(b, 0x6e, 125, true, {{0, 0, 255, 41}});
  f.copies = unpack(b, 0x6d, 126, true, {{0, 0, 0, 0}});
  auto tag = [&](unsigned prim, unsigned regs) {
    const std::uint64_t lo = 0x8003ULL | (1ULL << 46) |
                             (std::uint64_t(prim) << 47) |
                             (std::uint64_t(stride) << 60);
    return Row{int(lo & 0xffffffffU), int(lo >> 32), int(regs), 0};
  };
  std::vector<Row> tags{tag(0x15c, two_tags ? 0x41f3 : 0x413)};
  if (two_tags)
    tags.push_back(tag(0x35c, 0x413f));
  f.tag = unpack(b, 0x6c, 127, false, tags);
  f.mscal = b.size();
  word(b, two_tags ? 0x14000044 : 0x14000042);
  word(b, 0x01000100 | stride);
  f.uv = unpack(b, 0x65, 0x81, true,
                {{0, 8, 0, 0}, {16, 40, 0, 0}, {32, 72, 0, 0}});
  word(b, 0x01000101);
  while (b.size() % 16)
    b.push_back(0);
  put(b, 0x2c, b.size(), 4);
  return f;
}
Fixture withFacing(Fixture f, bool effect = false) {
  const unsigned extra = effect ? 80 : 48;
  f.bytes.insert(f.bytes.begin() + 0x80, extra, 0);
  for (auto *p : {&f.pos, &f.norm, &f.slots, &f.weights, &f.copies, &f.tag,
                  &f.mscal, &f.uv})
    *p += extra;
  if (f.shell)
    f.shell += extra;
  put(f.bytes, 0x10, (f.bytes[0x10] | 0x40), 2);
  put(f.bytes, 0x23, 8, 1);
  put(f.bytes, 0x28, 0x80 + extra, 4);
  put(f.bytes, 0x2c, f.bytes.size(), 4);
  for (unsigned i = 0; i < 32; ++i)
    f.bytes[0x80 + i] = std::uint8_t(3 * i + 1);
  f.bytes[0x80] = 3;
  f.bytes[0x90] = 0;
  put(f.bytes, 0x82, 0x8000, 2);
  put(f.bytes, 0x84, 0x7fff, 2);
  put(f.bytes, 0x86, 0xfff0, 2);
  f.bytes[0x88] = 0x80;
  f.bytes[0x89] = 0x7f;
  f.bytes[0x8a] = 0xff;
  f.bytes[0x8f] = 63;
  f.bytes[0x9f] = 0x80;
  for (unsigned i = 0; i < 16; ++i)
    f.bytes[0xa0 + i] = std::uint8_t(0xa0 + i);
  f.bytes[0xa0] = 255;
  if (effect) {
    put(f.bytes, 0x27, 11, 1);
    const std::array<std::uint32_t, 6> bits{0x80000000, 0x3f800000, 0xc0200000,
                                            0x00000001, 0x7f7fffff, 0x00800000};
    for (unsigned i = 0; i < 6; ++i)
      put(f.bytes, 0xb0 + i * 4, bits[i], 4);
  }
  return f;
}
champions::RtaModel decode(const Buffer &b) {
  return champions::decodeRtaModel({b.data(), b.size()});
}
void reject(const Buffer &b, const char *why) {
  bool failed = false;
  try {
    (void)decode(b);
  } catch (const champions::FormatError &) {
    failed = true;
  }
  check(failed, why);
}
void mutation(std::size_t at, std::uint64_t value, unsigned size,
              const char *why) {
  auto f = make();
  put(f.bytes, at, value, size);
  reject(f.bytes, why);
}
Buffer withVariants(Buffer b, std::size_t table_start = 0x40) {
  const auto count = b.at(0x12);
  b.insert(b.begin() + table_start, 512, 0);
  put(b, 0x10, 2, 2);
  put(b, 0x21, table_start / 16, 1);
  put(b, 0x25, b.at(0x25) + 32, 1);
  for (unsigned i = 0; i <= count; ++i) {
    const auto at = 0x28 + i * 4;
    std::uint32_t value = 0;
    for (unsigned j = 0; j < 4; ++j)
      value |= std::uint32_t(b[at + j]) << (8 * j);
    put(b, at, value + 512, 4);
  }
  return b;
}
void variantTests() {
  auto b = withVariants(make().bytes);
  put(b, 8, 1, 8);    // Own mesh0 hidden by default; part_mask is already1.
  put(b, 0x40, 1, 8); // Variant0 reveals it.
  put(b, 0x50, ~std::uint64_t(0), 8);
  put(b, 0x58, ~std::uint64_t(0), 8); // Hide wins every conflict.
  put(b, 0x40 + 31 * 16, std::uint64_t(1) << 63, 8);
  put(b, 0x48 + 31 * 16, 1, 8);
  auto m = decode(b);
  check(m.has_variants && m.variants[0].show == 1 &&
            m.variants[31].show == (std::uint64_t(1) << 63) &&
            m.variants[31].hide == 1,
        "all32 variant records retain unsigned64 values");
  check(champions::modelVisibilityMask(m) == ~std::uint64_t(1) &&
            !m.meshes[0].visible_by_default,
        "Default preserves hidden mask independently of variants");
  check(champions::modelVisibilityMask(m, 0) == ~std::uint64_t(0),
        "variant can reveal own mesh despite parent part mask");
  check(champions::modelVisibilityMask(m, 1) == 0,
        "hide wins show/hide conflict and permits all-hidden selection");
  check(champions::modelVisibilityMask(m, 2) ==
            champions::modelVisibilityMask(m),
        "unused record leaves Default unchanged");
  m.hidden_mask = std::uint64_t(1) << 63;
  check((champions::modelVisibilityMask(m, 31) & (std::uint64_t(1) << 63)) !=
                0 &&
            (champions::modelVisibilityMask(m, 31) & 1) == 0,
        "variant31 reveals mesh63 and hides mesh0 without signed shifts");
  m.hidden_mask = ~std::uint64_t(0);
  m.part_mask = ~std::uint64_t(0);
  check(champions::modelVisibilityMask(m) == 0 &&
            champions::modelVisibilityMask(m, 0) == 1,
        "all-hidden Default can reveal one mesh; part mask excluded");
  for (int selection : {-2, 32, 63}) {
    bool failed = false;
    try {
      (void)champions::modelVisibilityMask(m, selection);
    } catch (const champions::FormatError &) {
      failed = true;
    }
    check(failed, "variant selection must be Default or0..31");
  }
  const auto absent = decode(make().bytes);
  check(!absent.has_variants &&
            champions::modelVisibilityMask(absent) == ~std::uint64_t(0),
        "absent table still supports Default");
  for (int selection : {0, 31}) {
    bool failed = false;
    try {
      (void)champions::modelVisibilityMask(absent, selection);
    } catch (const champions::FormatError &) {
      failed = true;
    }
    check(failed, "variant selection without a table is rejected");
  }
  b = withVariants(make().bytes);
  put(b, 0x25, 0x23, 1);
  reject(b, "variant table must contain all32 records before next table");
  b = withVariants(make().bytes);
  put(b, 0x21, 0, 1);
  reject(b, "variant flag cannot name an absent table");
  b = withVariants(make().bytes);
  put(b, 0x10, 0, 2);
  reject(b, "variant table cannot omit its flag");
  b = withVariants(make().bytes);
  b.resize(0x230);
  reject(b, "truncated variant record region is rejected");
  Buffer many(0x9b0, 0);
  put(many, 0x12, 64, 1);
  put(many, 0x13, 255, 1);
  put(many, 0x26, 1, 1);
  put(many, 0x25, 0x3b, 1);
  for (unsigned i = 0; i <= 64; ++i)
    put(many, 0x28 + i * 4, many.size(), 4);
  many = withVariants(many, 0x3b0);
  put(many, 8, std::uint64_t(1) << 63, 8);
  put(many, 0x3b0 + 31 * 16, std::uint64_t(1) << 63, 8);
  const auto sixtyfour = decode(many);
  check(sixtyfour.meshes.size() == 64 &&
            !sixtyfour.meshes[63].visible_by_default &&
            ((champions::modelVisibilityMask(sixtyfour, 31) >> 63) & 1) == 1,
        "decoded64-mesh table supports variant31 affecting mesh63");
}
void skinTests() {
  using Skin = champions::ModelSkinInfluences;
  auto same = [](const Skin &a, const Skin &b) {
    return a.count == b.count && a.joints == b.joints && a.weights == b.weights;
  };
  const Skin rigid{{63, 0, 0, 0}, {255, 0, 0, 0}, 1};
  for (unsigned run : {3U, 255U}) {
    auto f = make(3, false, {{252, 19, 255, int(run)}, {0, 255, 255, 255}});
    const auto m = decode(f.bytes);
    check(m.meshes[0].corners.size() == 3,
          "rigid run emits only packet vertices, including overshoot");
    for (const auto &corner : m.meshes[0].corners)
      check(same(corner.skin, rigid),
            "rigid run ignores w1, maps joint63, and ignores terminal padding");
  }
  const std::vector<Row> blend{{0, 64, 252, 191},
                               {4, 100, 8, 100},
                               {12, 55, 255, 207},
                               {16, 0, 20, 55},
                               {24, 100, 28, 100}};
  const std::array<Skin, 3> expected{{{{0, 63, 0, 0}, {64, 191, 0, 0}, 2},
                                      {{1, 2, 3, 0}, {100, 100, 55, 0}, 3},
                                      {{4, 5, 6, 7}, {0, 55, 100, 100}, 4}}};
  auto f = make(3, false, blend);
  const auto original = decode(f.bytes).meshes[0].corners;
  for (unsigned i = 0; i < 3; ++i)
    check(same(original[i].skin, expected[i]),
          "two/three/four joint blends preserve order, byte weights, and "
          "zero-weight slots");
  std::fill(f.bytes.begin(), f.bytes.end(), 0);
  check(same(original[2].skin, expected[2]),
        "decoded influences own their storage after source mutation");
  f = make(3, false, blend);
  for (unsigned axis = 0; axis < 3; ++axis)
    put(f.bytes, f.pos + 4 + 6 + axis * 2, 0, 2);
  const auto coincident = decode(f.bytes).meshes[0].corners;
  check(coincident.size() == 3 &&
            coincident[0].position == coincident[1].position &&
            same(coincident[0].skin, expected[0]) &&
            same(coincident[1].skin, expected[1]),
        "coincident positions retain distinct source vertex skin influences");
  const auto runs =
      decode(make(3, false, {{0, 87, 255, 1}, {252, 0, 255, 2}}).bytes)
          .meshes[0]
          .corners;
  check(runs[0].skin.joints[0] == 0 && same(runs[1].skin, rigid) &&
            same(runs[2].skin, rigid),
        "consecutive rigid runs advance exactly across source vertices");
  f = make(3, false, blend);
  for (unsigned i = 0; i < 3; ++i)
    put(f.bytes, f.norm + 4 + i * 3 + 2, 129, 1);
  const auto reversed = decode(f.bytes).meshes[0].corners;
  check(reversed[0].position == original[1].position &&
            same(reversed[0].skin, expected[1]) &&
            same(reversed[1].skin, expected[0]),
        "winding reversal carries each source vertex's influences");
  f = make(5, true, blend);
  const auto copied = decode(f.bytes).meshes[0].corners;
  check(copied.size() == 9 && copied[5].uv_texels != original[0].uv_texels &&
            same(copied[5].skin, expected[0]),
        "copied strip UV seam retains original vertex influences");
  for (const auto &corner : copied) {
    bool matched = false;
    for (unsigned i = 0; i < 3; ++i)
      if (corner.position == original[i].position)
        matched = same(corner.skin, expected[i]);
    check(matched,
          "all copied strip corners follow their source vertex influences");
  }
  f = make(4, true, blend);
  put(f.bytes, f.copies + 4 + 8 + 6, 0, 2);
  const auto alias = decode(f.bytes).meshes[0].corners;
  check(alias.size() == 3 && same(alias.back().skin, expected[0]),
        "overlapping strip copy preserves snapshotted source influences");
  reject(make(3, false, {{0, 0, 255, 2}}).bytes,
         "weight run must cover all source vertices");
  reject(make(3, false, {{0, 0, 255, 0}}).bytes,
         "zero-length rigid run is rejected");
  reject(make(3, false, {{0, 100, 4, 100}}).bytes,
         "missing continuation is rejected");
  reject(
      make(3, false, {{0, 100, 4, 100}, {8, 54, 255, 0}, {0, 0, 255, 2}}).bytes,
      "incorrect blended total is rejected");
  reject(
      make(3, false, {{0, 200, 4, 100}, {8, 0, 255, 0}, {0, 0, 255, 2}}).bytes,
      "oversummed blended weights are rejected");
  reject(
      make(3, false, {{0, 100, 4, 100}, {9, 55, 255, 0}, {0, 0, 255, 2}}).bytes,
      "unaligned continuation joint is rejected");
  reject(make(3, false, {{0, 100, 4, 100}, {8, 25, 253, 30}, {0, 0, 255, 2}})
             .bytes,
         "out-of-range fourth joint is rejected");
  reject(make(3, false, {{0, 0, 255, 3}, {0, 254, 255, 255}}).bytes,
         "only the documented terminal padding is accepted");
  reject(
      make(3, false, {{0, 0, 255, 3}, {0, 255, 255, 255}, {0, 255, 255, 255}})
          .bytes,
      "extra terminal padding entries are rejected");
}
void tests() {
  auto f = make();
  auto model = decode(f.bytes);
  check(model.meshes.size() == 1 && model.meshes[0].visible_by_default &&
            model.part_mask == 1,
        "parent part mask does not hide own mesh");
  check(model.attachment_slot == -1, "signed attachment sentinel is retained");
  for (unsigned slot : {0U, 15U}) {
    auto part = make();
    put(part.bytes, 0x13, slot, 1);
    check(decode(part.bytes).attachment_slot == int(slot),
          "parent attachment slot is retained");
  }
  const auto original = model.meshes[0].corners;
  check(original.size() == 3 &&
            original[1].position == std::array<float, 3>{10, 0, 0} &&
            original[2].position == std::array<float, 3>{0, 10, 5},
        "triangle positions and scale");
  check(original[0].normal == std::array<float, 3>{0, 0, 1} &&
            original[2].uv_texels == std::array<float, 2>{2, 4},
        "normal and corner UV scale");
  std::fill(f.bytes.begin(), f.bytes.end(), 0);
  check(model.meshes[0].corners[1].position[0] == 10,
        "owned geometry survives input mutation");
  f = make();
  put(f.bytes, f.tag, 0xc000 + 13, 2);
  check(decode(f.bytes).meshes[0].corners.size() == 3,
        "GIF words accept unsigned V4-32 without numeric reinterpretation");
  f = make();
  put(f.bytes, 8, 1, 8);
  check(!decode(f.bytes).meshes[0].visible_by_default,
        "hidden-by-default mask");
  put(f.bytes, 0x38, 65535, 2);
  check(decode(f.bytes).meshes[0].corners.size() == 3,
        "record triangle hint is not authoritative");
  f = make();
  put(f.bytes, f.pos + 4, 0x8000, 2);
  put(f.bytes, f.pos + 6, 0x7fff, 2);
  auto signed_model = decode(f.bytes);
  bool extrema = false;
  for (const auto &c : signed_model.meshes[0].corners)
    extrema |= c.position[0] == -2048.0f && c.position[1] == 2047.9375f;
  check(extrema, "signed16 conversion is defined at extrema");
  f = make();
  for (unsigned i = 0; i < 3; ++i)
    put(f.bytes, f.norm + 4 + i * 3 + 2, 129, 1);
  const auto reversed = decode(f.bytes).meshes[0].corners;
  check(reversed[0].position == original[1].position &&
            reversed[0].uv_texels == original[1].uv_texels,
        "normal-directed winding swaps matching UV corners");
  f = make();
  put(f.bytes, f.slots + 4 + 4 * 6, 0x8006, 2);
  check(decode(f.bytes).meshes[0].corners.empty(),
        "terminal nodraw suppresses triangle");
  f = make(5, true);
  auto copies = decode(f.bytes).meshes[0].corners;
  check(copies.size() == 9 && copies[5].position == original[0].position &&
            copies[5].uv_texels == std::array<float, 2>{3, 6},
        "copy rows preserve per-strip UV seams");
  // Both sources are snapshotted before d2 then d1. Here d2 overwrites s1;
  // d1 must still receive the old vertex0 rather than vertex1.
  put(f.bytes, f.copies + 4 + 8, 0, 2);
  put(f.bytes, f.copies + 4 + 8 + 2, 9, 2);
  put(f.bytes, f.copies + 4 + 8 + 4, 3, 2);
  put(f.bytes, f.copies + 4 + 8 + 6, 0, 2);
  // Strip4 would be unfilled now: shorten to4 with a fresh synthetic packet.
  auto alias = make(4, true);
  put(alias.bytes, alias.copies + 4 + 8 + 6, 0, 2);
  const auto alias_corners = decode(alias.bytes).meshes[0].corners;
  check(alias_corners.size() == 3 &&
            alias_corners.back().position == original[0].position,
        "copy source snapshot survives overlapping destination");
  f = make(4, true);
  put(f.bytes, f.slots + 4 + 2 * 6, 0x8000, 2);
  put(f.bytes, f.slots + 4 + 3 * 6 + 4, 0x8000, 2);
  put(f.bytes, f.slots + 4 + 4 * 6 + 4, 0x8000, 2);
  put(f.bytes, f.copies + 12, 1, 2);
  put(f.bytes, f.copies + 14, 10, 2);
  check(decode(f.bytes).meshes[0].corners.size() == 3,
        "XYZ copied through lane1 retains its nodraw bit");
  f = make(5, true);
  put(f.bytes, f.copies + 4 + 8 + 2, 0x8009, 2);
  check(decode(f.bytes).meshes[0].corners.size() == 6,
        "copy destination owns nodraw flag");
  f = make();
  for (std::size_t n = 0; n < f.bytes.size(); ++n)
    reject(Buffer(f.bytes.begin(), f.bytes.begin() + n),
           "every truncated model prefix rejected");
  bool null_rejected = false;
  try {
    (void)champions::decodeRtaModel({nullptr, 128});
  } catch (const champions::FormatError &) {
    null_rejected = true;
  }
  check(null_rejected, "null input");
  reject(Buffer(16U * 1024U * 1024U + 16, 0), "input budget");
  for (unsigned count : {0U, 65U, 255U})
    mutation(0x12, count, 1, "signed mesh count bound");
  mutation(0x26, 0, 1, "zero mesh tables unsupported");
  mutation(0x26, 2, 1, "multiple mesh tables unsupported");
  for (unsigned flag : {0x20U, 0x40U, 0x80U})
    mutation(0x10, flag, 2,
             "form flags require matching packets/tables or remain unknown");
  mutation(0x14, 1, 4, "runtime model field");
  mutation(0x27, 4, 1, "effect table may not alias hull");
  mutation(0x13, 16, 1, "attachment bound");
  mutation(0x13, 254, 1, "unsupported negative attachment slot");
  mutation(0x28, 0x81, 4, "unaligned mesh start");
  mutation(0x28, 0x20, 4, "mesh overlaps header");
  mutation(0x2c, 0xfffffff0, 4, "mesh outside file");
  mutation(0x2c, 0x70, 4, "descending offsets");
  mutation(0x25, 0, 1, "missing hull");
  mutation(0x25, 7, 1, "hull exceeds its table extent");
  mutation(0x25, 2, 1, "hull overlaps mesh records");
  mutation(0x10, 2, 2, "variant flag mismatch");
  mutation(0x21, 4, 1, "variant offset mismatch");
  mutation(0x24, 4, 1, "aliased ancillary tables");
  mutation(0x36, 64, 1, "mesh joint bound");
  mutation(0x37, 1, 1, "mesh reserved byte");
  mutation(0x80, 0x04000000, 4, "zero vertices");
  mutation(0x80, 0x0400002a, 4, "vertex count exceeds41");
  mutation(0x80, 0x05000001, 4, "STMOD row transform unsupported");
  {
    auto world_mode = f.bytes;
    world_mode.insert(world_mode.begin() + f.pos, 16, 0);
    put(world_mode, f.pos, 0x05000001, 4);
    put(world_mode, 0x2c, world_mode.size(), 4);
    reject(world_mode, "STMOD1 after valid ITOP cannot silently ignore "
                       "external row transform");
  }
  mutation(f.pos + 3, 0xe9, 1, "IRQ unpack");
  mutation(f.pos + 3, 0x79, 1, "masked unpack");
  mutation(f.pos, 0, 2, "absolute unpack");
  mutation(f.pos, 0x8400, 2, "reserved unpack address bits");
  mutation(f.pos + 2, 0, 1, "num0 is256 and exceeds mesh payload");
  mutation(f.pos + 2, 4, 1, "position count mismatch");
  mutation(f.norm, 0x8004, 2, "normal address mismatch");
  mutation(f.weights + 4, 1, 1, "unaligned joint byte");
  mutation(f.weights + 7, 0, 1, "empty weight run");
  mutation(f.weights + 6, 4, 1, "incomplete blended weight");
  mutation(f.copies + 4, 1, 2, "copy count mismatch");
  mutation(f.copies + 6, 1, 2, "copy header reserved field");
  mutation(f.slots + 4 + 4 * 6, 254, 2, "slot address bound");
  mutation(f.slots + 4 + 4 * 6, 0, 2, "unfilled final strip slot");
  mutation(f.tag + 4, 0, 2, "zero strip count");
  mutation(f.tag + 4, 0x8054, 2, "strip exceeds83");
  mutation(f.tag + 12, 0x412, 8, "GIF register list");
  mutation(f.mscal, 0x14000044, 4,
           "reflection program requires two-tag packet/header");
  mutation(f.mscal, 0x14000046, 4, "world program unsupported");
  mutation(f.mscal + 4, 0x01000104, 4, "UV stride mismatch");
  mutation(f.uv, 0xc080, 2, "UV address mismatch");
  mutation(f.uv + 2, 2, 1, "UV count mismatch");
  mutation(f.uv + 16, 0, 4, "missing cycle reset");
  // A valid second mesh starts immediately after the first; a changed UNPACK
  // count may not borrow bytes from that next mesh.
  f = make();
  const auto mesh_end = f.bytes.size();
  f.bytes.resize(mesh_end + 256, 0);
  put(f.bytes, f.pos + 2, 80, 1);
  reject(f.bytes, "UNPACK bounded by mesh end, not whole file");
  // 64 meshes exercise visibility bit63 without shifting by64. Empty aligned
  //  mesh streams are permitted and consume no output budget.
  Buffer many(0x9b0, 0);
  put(many, 0x12, 64, 1);
  put(many, 0x13, 255, 1);
  put(many, 0x26, 1, 1);
  put(many, 0x25, 0x3b, 1);
  put(many, 8, 1ULL << 63, 8);
  for (unsigned i = 0; i <= 64; ++i)
    put(many, 0x28 + i * 4, many.size(), 4);
  auto sixtyfour = decode(many);
  check(sixtyfour.meshes.size() == 64 &&
            !sixtyfour.meshes[63].visible_by_default &&
            sixtyfour.meshes[62].visible_by_default,
        "64th visibility bit");
  f = make(83);
  const Buffer packet(f.bytes.begin() + 0x80, f.bytes.end());
  Buffer large(f.bytes.begin(), f.bytes.begin() + 0x80);
  for (unsigned i = 0; i < 2470; ++i)
    large.insert(large.end(), packet.begin(), packet.end());
  put(large, 0x2c, large.size(), 4);
  reject(large, "global triangle output budget");
  f = make(1);
  const Buffer tiny(f.bytes.begin() + 0x80, f.bytes.end());
  large.assign(f.bytes.begin(), f.bytes.begin() + 0x80);
  for (unsigned i = 0; i < 32769; ++i)
    large.insert(large.end(), tiny.begin(), tiny.end());
  put(large, 0x2c, large.size(), 4);
  reject(large, "global packet budget");
}
void ancillaryTests() {
  auto f = make();
  const unsigned table = 0x80, attach_bytes = 16 * 0x34,
                 sample = table + attach_bytes, sample_bytes = 0x1b0,
                 extra = attach_bytes + sample_bytes;
  f.bytes.insert(f.bytes.begin() + table, extra, 0);
  put(f.bytes, 0x10, 4, 2);
  put(f.bytes, 0x22, table / 16, 1);
  put(f.bytes, 0x24, sample / 16, 1);
  put(f.bytes, 0x28, table + extra, 4);
  put(f.bytes, 0x2c, f.bytes.size(), 4);
  for (unsigned i = 0; i < 16; ++i) {
    put(f.bytes, table + i * 0x34, i == 0 ? 0xffffffffU : i - 1, 4);
    for (unsigned j = 0; j < 12; ++j)
      put(f.bytes, table + i * 0x34 + 4 + j * 4, 0x80000000U + i * 0x100 + j,
          4);
  }
  put(f.bytes, sample, 26, 4);
  put(f.bytes, sample + 4, 46, 4);
  put(f.bytes, sample + 0x10, 0x13000000, 4);
  put(f.bytes, sample + 0x14, 0x6d2e0200, 4);
  put(f.bytes, sample + 0x18, 45, 2);
  put(f.bytes, sample + 0x188, 0x14000030, 4);
  put(f.bytes, sample + 0x18c, 0x6d020300, 4);
  put(f.bytes, sample + 0x190, 1, 2);
  put(f.bytes, sample + 0x1a0, 0x14000030, 4);
  put(f.bytes, sample + 0x1a4, 0x13000000, 4);
  for (unsigned i = 0; i < 46; ++i) {
    const auto at = sample + 0x20 + i * 8 + (i / 45) * 16;
    put(f.bytes, at, std::uint16_t(-int(i)), 2);
    put(f.bytes, at + 2, 0x7fff - i, 2);
    put(f.bytes, at + 4, 0x8000 + i, 2);
    put(f.bytes, at + 6, (i << 8) | ((i % 64) * 4), 2);
  }
  for (unsigned i = 0; i < 6; ++i)
    put(f.bytes, 0x30 + i, 0x80 + i, 1);
  put(f.bytes, 0x38, 0xbeef, 2);
  for (unsigned i = 0; i < 24; ++i)
    put(f.bytes, 0x40 + i, 0x80 + i, 1);
  const auto m = decode(f.bytes);
  check(m.attach_points.size() == 16 && m.attach_points[0].joint == -1 &&
            m.attach_points[15].joint == 14,
        "all16 attach slots retain signed raw joint values");
  check(m.attach_points[15].matrix_raw[11] == 0x80000f0bU &&
            m.attach_points[0].matrix_raw[0] == 0x80000000U,
        "attach matrix bits preserve all12 words without evaluation");
  check(m.sample_points.size() == 46 &&
            m.sample_block_raw.size() == sample_bytes,
        "sample header packet extent retained exactly");
  for (unsigned i : {0U, 44U, 45U})
    check(
        m.sample_points[i].position_raw ==
                std::array<std::int16_t, 3>{std::int16_t(-int(i)),
                                            std::int16_t(0x7fff - i),
                                            std::int16_t(-32768 + int(i))} &&
            m.sample_points[i].joint_size_raw == ((i << 8) | i * 4),
        "sample coordinate/size/joint fields cross45-point packet gap exactly");
  check(std::equal(m.sample_block_raw.begin(), m.sample_block_raw.end(),
                   f.bytes.begin() + sample),
        "sample VIF metadata block preserves every byte");
  check(m.meshes[0].bounds_raw == std::array<std::int8_t, 6>{-128, -127, -126,
                                                             -125, -124,
                                                             -123} &&
            m.meshes[0].declared_triangles == 0xbeef &&
            m.meshes[0].hull_points[7] ==
                std::array<std::int8_t, 3>{-107, -106, -105},
        "mesh bounds, nonauthoritative triangle hint and signed hull "
        "coordinates retained");
  const auto hull = champions::rtaModelHull({f.bytes.data(), f.bytes.size()}, 0);
  check(hull == m.meshes[0].hull_points,
        "runtime hull binding agrees with decoded signed mesh points");
  auto no_hull = f.bytes;
  no_hull[0x25] = 0;
  bool hull_refused = false;
  try {
    (void)champions::rtaModelHull({no_hull.data(), no_hull.size()}, 0);
  } catch (const champions::FormatError &) {
    hull_refused = true;
  }
  check(hull_refused, "runtime hull binding refuses a missing table");
  auto bad = f.bytes;
  put(bad, 0x24, (sample - 16) / 16, 1);
  reject(bad, "attach table must retain all16 records before sample table");
  bad = f.bytes;
  put(bad, sample, 25, 4);
  check(decode(bad).sample_points.size() == 46,
        "sample metadata reader does not claim to validate trailing VIF "
        "commands");
  // 26qwords are needed for complete packet metadata; the CPU-visible final
  // point
  //  ends at0x1a0. A length through0x1a0 is still enough for that point.
  bad = f.bytes;
  put(bad, sample, 24, 4);
  reject(bad, "sample45 requires post-gap bytes inside declared sample block");
  bad = f.bytes;
  put(bad, sample + 0x20 + 6, 1, 2);
  reject(bad, "sample matrix-address low bits require alignment");
}
void formTests() {
  const auto base = decode(make().bytes).meshes[0].corners;
  for (bool two : {false, true})
    for (bool shell : {false, true}) {
      auto f = makeForm(two, shell);
      const auto m = decode(f.bytes);
      const auto &mesh = m.meshes[0];
      check(mesh.corners.size() == 3,
            "new forms retain one geometry set rather than duplicating "
            "reflection pass");
      check(m.alpha1 == 0x8000000029ULL && m.fix2 == 58,
            "both model blend fields retain exact raw values");
      check(mesh.packets.size() == 1,
            "one authored packet creates one metadata record");
      const auto &packet = mesh.packets[0];
      check(packet.program == (two ? 0x44 : 0x42) &&
                packet.stride == (two ? 4 : 3) && packet.vertex_count == 3 &&
                packet.strip_count == 3 && packet.first_corner == 0 &&
                packet.corner_count == 3,
            "packet program/counts/geometry range exact");
      check(packet.gif_tags.size() == (two ? 2U : 1U) &&
                packet.gif_tags[0][1] == (two ? 0x41f3 : 0x413),
            "first GIF register list retained");
      if (two)
        check(packet.gif_tags[1][1] == 0x413f &&
                  ((packet.gif_tags[1][0] >> 47) & 0x7ff) == 0x35c,
              "second GIF tag remains separate and uses context2");
      const std::array<std::array<std::int8_t, 3>, 3> normals{
          {{127, 0, 0}, {0, -128, 0}, {0, 0, -64}}};
      for (unsigned i = 0; i < 3; ++i) {
        const auto &c = mesh.corners[i];
        check(c.position == base[i].position && c.normal == base[i].normal,
              "new forms preserve base rest position/normal");
        check(c.uv_raw ==
                      std::array<std::uint16_t, 2>{std::uint16_t(i * 16),
                                                   std::uint16_t(i * 32 + 8)} &&
                  c.uv_texels ==
                      std::array<float, 2>{float(i), float(i * 2) + 0.5f},
              "stored base UV qword retained with stride4 gaps");
        check(c.position_raw ==
                      std::array<std::int16_t, 3>{
                          std::int16_t(base[i].position[0] * 16),
                          std::int16_t(base[i].position[1] * 16),
                          std::int16_t(base[i].position[2] * 16)} &&
                  c.normal_raw == std::array<std::int8_t, 3>{0, 0, 127},
              "lossless signed position and ordinary normal metadata");
        check(c.skin.count == 1 && c.skin.joints[0] == (i == 2 ? 63 : i),
              "distinct source skin follows packet vertices");
        check(bool(c.shell_normal_raw) == shell,
              "shell normal presence reflects header and packet");
        if (shell)
          check(*c.shell_normal_raw == normals[i],
                "signed shell normal bytes preserved without ordinary-normal "
                "normalization");
      }
      auto owned = m;
      std::fill(f.bytes.begin(), f.bytes.end(), 0);
      check(owned.meshes[0].packets[0].gif_tags == packet.gif_tags &&
                owned.meshes[0].corners[2].shell_normal_raw ==
                    mesh.corners[2].shell_normal_raw,
            "new geometry/pass metadata owns input bytes");
    }
  // Header0x10 permits two-pass packets; the actual GIF count selects each
  // packet's stride and program, including mixed forms within one mesh.
  for (bool shell : {false, true}) {
    auto single = makeForm(false, shell);
    put(single.bytes, 0x10, shell ? 0x30 : 0x10, 2);
    auto m = decode(single.bytes);
    check(m.meshes[0].packets[0].program == 0x42 &&
              m.meshes[0].packets[0].stride == 3 &&
              m.meshes[0].packets[0].gif_tags.size() == 1 &&
              m.meshes[0].corners.size() == 3,
          "reflection-capable header permits ordinary single-tag packet");
    auto double_form = makeForm(true, shell, 5, true);
    single.bytes.insert(single.bytes.end(), double_form.bytes.begin() + 0x80,
                        double_form.bytes.end());
    put(single.bytes, 0x2c, single.bytes.size(), 4);
    m = decode(single.bytes);
    const auto &mesh = m.meshes[0];
    check(mesh.packets.size() == 2 && mesh.corners.size() == 12 &&
              mesh.packets[0].program == 0x42 && mesh.packets[0].stride == 3 &&
              mesh.packets[0].first_corner == 0 &&
              mesh.packets[0].corner_count == 3 &&
              mesh.packets[1].program == 0x44 && mesh.packets[1].stride == 4 &&
              mesh.packets[1].first_corner == 3 &&
              mesh.packets[1].corner_count == 9 &&
              mesh.packets[1].gif_tags.size() == 2,
          "one mesh preserves separate single/two-pass packet spans");
    check(mesh.corners[0].position_raw == mesh.corners[3].position_raw &&
              mesh.corners[8].skin.joints[0] == 0 &&
              mesh.corners[8].uv_raw == std::array<std::uint16_t, 2>{48, 104} &&
              bool(mesh.corners[8].shell_normal_raw) == shell,
          "mixed packet source identity, UV seam and shell metadata remain "
          "exact");
  }
  auto limit = makeInputLimit(false);
  auto edge = decode(limit.bytes).meshes[0];
  check(edge.packets[0].vertex_count == 41 && edge.packets[0].program == 0x42 &&
            edge.corners.size() == 3 &&
            edge.corners[2].position_raw ==
                std::array<std::int16_t, 3>{0, 160, 80},
        "single GIF tag at127 ends exactly at128 input qwords");
  put(limit.bytes, limit.mscal, 0x14000044, 4);
  reject(limit.bytes, "one GIF tag cannot dispatch two-pass program44");
  reject(makeInputLimit(true).bytes,
         "two GIF tags at127 exceed unchanged128-qword input limit");
  for (bool shell : {false, true}) {
    auto f = makeForm(true, shell, 5, true);
    auto m = decode(f.bytes);
    check(m.meshes[0].corners.size() == 9 &&
              m.meshes[0].corners[5].position == base[0].position &&
              m.meshes[0].corners[5].uv_raw ==
                  std::array<std::uint16_t, 2>{48, 104},
          "stride4 strip copies retain base UV seams");
    if (shell)
      check(m.meshes[0].corners[5].shell_normal_raw ==
                std::optional<std::array<std::int8_t, 3>>({127, 0, 0}),
            "shell normals follow copied source identity");
    put(f.bytes, f.copies + 14, 0x800d, 2);
    check(decode(f.bytes).meshes[0].corners.size() == 6,
          "stride4 copy destination ADC suppresses its terminal triangle");
    f = makeForm(true, shell, 4, true);
    put(f.bytes, f.copies + 18, 1, 2);
    auto alias = decode(f.bytes).meshes[0].corners;
    check(alias.size() == 3 && alias.back().position == base[0].position &&
              alias.back().skin.joints[0] == 0,
          "stride4 both copy sources snapshotted before overlapping stores");
    f = makeForm(true, shell, 3, true);
    put(f.bytes, f.slots + 4 + 4 * 6, 0x8009, 2);
    put(f.bytes, f.copies + 12, 9, 2);
    put(f.bytes, f.copies + 14, 9, 2);
    put(f.bytes, f.copies + 16, 1, 2);
    put(f.bytes, f.copies + 18, 1, 2);
    check(
        decode(f.bytes).meshes[0].corners.size() == 3,
        "stride4 self-copy rewrites ADC rather than skipping equal addresses");
  }
  auto overwritten = makeForm(true, false);
  put(overwritten.bytes, overwritten.slots + 4 + 4 * 6 + 2, 1, 2);
  put(overwritten.bytes, overwritten.slots + 4 + 4 * 6 + 4, 9, 2);
  check(decode(overwritten.bytes).meshes[0].corners.empty(),
        "stride4 row copy wins after its own XYZ store at overlapping "
        "destination");
  auto own_wins = makeForm(false, false);
  put(own_wins.bytes, own_wins.slots + 4 + 4 * 6 + 2, 0, 2);
  put(own_wins.bytes, own_wins.slots + 4 + 4 * 6 + 4, 6, 2);
  check(decode(own_wins.bytes).meshes[0].corners.size() == 3,
        "stride3 retains its distinct own-after-copy XYZ store order");
  auto f = makeForm(true, false, 62);
  check(decode(f.bytes).meshes[0].packets[0].strip_count == 62,
        "stride4 maximum strip count accepted");
  f = makeForm(true, false);
  put(f.bytes, f.tag + 4, 0x803f, 2);
  put(f.bytes, f.tag + 20, 0x803f, 2);
  reject(f.bytes, "stride4 count63 exceeds output memory budget");
  f = makeForm(true, true);
  const Buffer packet(f.bytes.begin() + 0x80, f.bytes.end());
  f.bytes.insert(f.bytes.end(), packet.begin(), packet.end());
  put(f.bytes, 0x2c, f.bytes.size(), 4);
  auto double_packet = decode(f.bytes).meshes[0];
  check(double_packet.packets.size() == 2 &&
            double_packet.packets[1].first_corner == 3 &&
            double_packet.packets[1].corner_count == 3,
        "packet corner ranges remain distinct across one mesh");
  for (bool two : {false, true}) {
    f = makeForm(two, true);
    for (std::size_t end = 0; end < f.bytes.size(); ++end)
      reject(Buffer(f.bytes.begin(), f.bytes.begin() + end),
             "every new-form truncated prefix rejected");
  }
  f = makeForm(true, true);
  auto bad = [&](std::size_t at, std::uint64_t value, unsigned size,
                 const char *why) {
    auto b = f.bytes;
    put(b, at, value, size);
    reject(b, why);
  };
  bad(f.slots + 6, 0, 2, "shell pointer required");
  bad(f.slots + 6, 127, 2, "shell pointer must name its own normal block");
  bad(f.shell, 0x8000, 2, "shell normal address mismatch");
  bad(f.shell + 2, 2, 1, "shell normal count mismatch");
  bad(f.shell + 3, 0x6e, 1, "shell normal kind mismatch");
  bad(f.shell, 0xc000 + 17, 2, "unsigned shell normals rejected");
  bad(f.tag + 2, 1, 1,
      "tag count cannot disagree with remaining packet layout");
  bad(f.tag + 20, 0x8004, 2, "second GIF loop count must agree");
  bad(f.tag + 28, 0x41f3, 8, "second GIF uses its distinct register list");
  bad(f.tag + 12, 0x413f, 8, "first GIF register order must not swap");
  bad(f.tag + 20 + 6, 0x20, 1,
      "second GIF context/primitive mutation rejected");
  bad(f.mscal, 0x14000042, 4, "two-tag packet and vertex program agree");
  bad(f.mscal + 4, 0x01000103, 4, "two-pass UV stride must be4");
  bad(f.uv, 0xc082, 2, "two-pass base UV qword address fixed");
  bad(f.copies + 4, 1, 2, "copy block count must agree");
  bad(0x10, 0x10, 2, "shell block cannot occur without shell flag");
  bad(0x10, 0x20, 2, "two-tag form cannot occur without two-pass flag");
  bad(0x10, 0xb0, 2, "unknown model flag remains rejected");
  f = withFacing(makeForm(true, true), true);
  auto m = decode(f.bytes);
  check(m.facing_points.size() == 2 && m.facing_terminator_raw &&
            (*m.facing_terminator_raw)[0] == 255 &&
            (*m.facing_terminator_raw)[15] == 0xaf,
        "facing sentinel is bounded and preserved as raw metadata");
  check(m.facing_points[0].position_raw ==
                std::array<std::int16_t, 3>{-32768, 32767, -16} &&
            m.facing_points[0].normal_raw ==
                std::array<std::int8_t, 3>{-128, 127, -1} &&
            m.facing_points[0].joint == 63,
        "facing signed position/normal/joint fields exact");
  check(m.facing_points[1].joint == -128,
        "facing signed joint metadata is not reinterpreted as a validated "
        "skeleton reference");
  check(m.facing_points[0].raw[1] == 4 && m.facing_points[0].raw[11] == 34,
        "uninterpreted facing fields retained");
  check(m.effect_segment_raw && *m.effect_segment_raw ==
                                    std::array<std::uint32_t, 6>{
                                        0x80000000, 0x3f800000, 0xc0200000,
                                        0x00000001, 0x7f7fffff, 0x00800000},
        "effect float bits retain negative zero/subnormal/extrema without host "
        "arithmetic");
  auto nonfinite = f.bytes;
  put(nonfinite, 0xb0, 0x7fc01234, 4);
  put(nonfinite, 0xb4, 0xff800000, 4);
  auto uninterpreted = decode(nonfinite);
  check((*uninterpreted.effect_segment_raw)[0] == 0x7fc01234 &&
            (*uninterpreted.effect_segment_raw)[1] == 0xff800000,
        "effect float metadata preserves nonfinite payloads without evaluating "
        "them");
  auto owned = m;
  std::fill(f.bytes.begin(), f.bytes.end(), 0);
  check(owned.facing_points[0].raw == m.facing_points[0].raw &&
            owned.effect_segment_raw == m.effect_segment_raw,
        "ancillary metadata owns storage");
  f = withFacing(makeForm(false, false));
  auto b = f.bytes;
  put(b, 0x80, 255, 1);
  auto empty = decode(b);
  check(empty.facing_points.empty() && empty.facing_terminator_raw.has_value(),
        "first-record sentinel permits empty facing table");
  b = f.bytes;
  put(b, 0xa0, 0, 1);
  reject(b, "facing table needs sentinel before mesh");
  b = f.bytes;
  put(b, 0x23, 0, 1);
  reject(b, "facing flag requires table");
  b = f.bytes;
  put(b, 0x10, 0, 2);
  reject(b, "facing table requires flag");
  b = f.bytes;
  put(b, 0x23, 4, 1);
  reject(b, "facing table cannot alias hull");
  b = f.bytes;
  put(b, 0x23, 11, 1);
  reject(b, "facing table cannot start at mesh");
  f = withFacing(makeForm(false, false), true);
  b = f.bytes;
  put(b, 0x27, 10, 1);
  reject(b, "effect table cannot overlap facing sentinel");
  b = f.bytes;
  put(b, 0x27, 12, 1);
  reject(b, "effect segment24bytes cannot cross mesh boundary");
}
void write(const fs::path &path, const Buffer &b) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()),
          static_cast<std::streamsize>(b.size()));
  if (!f)
    throw std::runtime_error("cannot write authored model fixture");
}
int run(const std::vector<fs::path> &args) {
  try {
    if (args.size() == 2 && args[0] == "--write-fixtures") {
      fs::create_directories(args[1]);
      write(args[1] / "triangle42.vif", make().bytes);
      write(args[1] / "copied-strip42.vif", make(5, true).bytes);
      write(args[1] / "two-pass44.vif", makeForm(true, false, 5, true).bytes);
      write(args[1] / "shell42.vif", makeForm(false, true).bytes);
      write(args[1] / "two-pass-shell44.vif",
            makeForm(true, true, 5, true).bytes);
      write(args[1] / "authored-facing.vif",
            withFacing(makeForm(true, true), true).bytes);
      return 0;
    }
    if (!args.empty())
      throw std::runtime_error("unexpected test arguments");
    tests();
    variantTests();
    skinTests();
    formTests();
    ancillaryTests();
    std::cout << checks << " model checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
  std::vector<fs::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return run(args);
}
#else
int main(int argc, char **argv) {
  std::vector<fs::path> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(fs::u8path(argv[i]));
  return run(args);
}
#endif
