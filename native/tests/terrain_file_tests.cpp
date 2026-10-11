#include "champions/terrain_file.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace R = champions::simulation::rta;
namespace W = R::world_queries;
namespace T = R::terrain_file;
namespace {
using Bytes = std::vector<std::uint8_t>;
unsigned checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
template <class E = T::Error, class F>
void rejects(F action, const char *message) {
  bool failed = false;
  try {
    action();
  } catch (const E &) {
    failed = true;
  }
  check(failed, message);
}
void put(Bytes &b, std::size_t offset, std::uint64_t value,
         unsigned width = 4) {
  if (offset > b.size() || width > b.size() - offset)
    throw std::runtime_error("authored fixture write outside buffer");
  for (unsigned i = 0; i < width; ++i)
    b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
// Authored layout: one chunk, two elements sharing a 2x2 heightfield,
// repeated piece fixups, duplicate cell references, and two path words.
Bytes file() {
  Bytes b(0x220, 0);
  put(b, 0, 1);
  put(b, 0x10, 1);
  put(b, 0x14, 1);
  put(b, 0x18, 0x170);
  put(b, 0x1c, 2);
  put(b, 0x20, 0xc0);
  put(b, 0x24, 0x84);
  put(b, 0x28, 0xfffffffe);
  put(b, 0x2c, 3);
  put(b, 0x30, 2);
  put(b, 0x34, 1);
  put(b, 0x38, 0x1e0);
  put(b, 0x50, 2);
  put(b, 0x54, 0x1a0);
  put(b, 0x64, 0x1f0);
  put(b, 0x68, 0x44340000);
  put(b, 0x78, 0x180);
  put(b, 0x84, 0x210); // No geometry payload traversal is promised.
  for (unsigned i = 0; i < 2; ++i) {
    const auto e = 0xc0 + i * 0x24;
    put(b, e, 0xfff8, 2);
    put(b, e + 2, 0xfff8, 2);
    put(b, e + 4, 8, 2);
    put(b, e + 6, 8, 2);
    put(b, e + 8, 0xdeadbeef);
    put(b, e + 12, 0x110);
    put(b, e + 16, i ? 0x12340004 : 0xffff0006);
    put(b, e + 28, i ? 9 : 5);
    put(b, e + 32, 1234, 2);
    put(b, e + 34, static_cast<std::uint16_t>(-2345), 2);
    const auto p = 0x1a0 + i * 0x1c;
    put(b, p + 4, 1, 2);
    put(b, p + 24, i, 2);
  }
  put(b, 0x118, 2);
  put(b, 0x11c, 2);
  put(b, 0x120, static_cast<std::uint16_t>(-3), 2);
  put(b, 0x122, 7, 2);
  for (unsigned i = 0; i < 4; ++i)
    put(b, 0x124 + 2 * i,
        static_cast<std::uint16_t>(std::array<int, 4>{-31, 32, -16, 7}[i]), 2);
  put(b, 0x180, 1, 2);
  put(b, 0x182, 0, 2);
  put(b, 0x184, 1, 2);
  put(b, 0x186, 0xffff, 2);
  put(b, 0x1e0, 0x12345678);
  put(b, 0x1e4, 0xffffffff);
  return b;
}
void decoded_values() {
  auto b = file();
  const auto original = b;
  auto d = T::decode(b);
  check(b == original,
        "decoder does not rewrite source bytes during piece fixup");
  check(d.stamp == 0 && d.elements.size() == 2 && d.heightfields.size() == 1,
        "shared offsets form one field and file stamps do not become runtime "
        "stamps");
  check(d.elements[0].hf == 0 && d.elements[1].hf == 0 &&
            d.elements[0].stamp == 0,
        "both elements preserve one shared field identity");
  check(d.elements[0].flags == 6 && d.elements[1].flags == 4,
        "element flags are exactly narrowed to unsigned low16");
  check(d.elements[0].cos == 1234 && d.elements[0].sin == -2345,
        "non-unit signed rotation words remain raw values");
  check(d.elements[0].f_min_x == 0xc1000000 &&
            d.elements[0].f_max_x == 0x41000000,
        "cached element bounds use exact signed16 float conversion");
  check(d.heightfields[0].samples ==
                std::vector<std::int16_t>({32001, 32, -16, 32007}) &&
            d.heightfields[0].hi == 2000 && d.heightfields[0].lo == -3 &&
            d.heightfields[0].offset == 0x110,
        "ordered repeated fixups preserve low flags and only replace bit0 "
        "heights");
  check(d.grid == std::vector<std::vector<std::int64_t>>{{1, 0, 1}} &&
            d.pieces == std::vector<std::int64_t>{0, 1},
        "list duplicates/order and piece element references are retained");
  check(d.pg_x0 == -2 && d.pg_y0 == 3 && d.pg_w == 2 && d.pg_h == 1 &&
            d.path_cells == std::vector<std::uint32_t>{0x12345678, 0xffffffff},
        "unused path metadata and all path word bits survive");
  b.assign(b.size(), 0);
  check(d.heightfields[0].samples[0] == 32001,
        "decoded values outlive and do not alias caller input");
  W::Level level(d);
  auto old = level.snapshot();
  level.set_piece_collision(0, true);
  level.set_piece_height(0, 0x40f00000);
  check(level.data().elements[0].flags == 4 && level.get_piece_height(0) == 7 &&
            old.data.elements[0].flags == 6 && old.data.elements[0].z == 5,
        "piece operations work on decoded data and earlier snapshots are "
        "deep-owned");
  d.heightfields[0].samples[0] = 0;
  check(level.data().heightfields[0].samples[0] == 32001,
        "Level owns a deep copy of decoded sample storage");
  auto sorted = file();
  put(sorted, 0x50, 0);
  put(sorted, 0xcc, 0x140);
  for (unsigned i = 0; i < 28; ++i)
    sorted[0x140 + i] = sorted[0x110 + i];
  auto order = T::decode(sorted);
  check(order.heightfields.size() == 2 &&
            order.heightfields[0].offset == 0x110 &&
            order.heightfields[1].offset == 0x140 &&
            order.elements[0].hf == 1 && order.elements[1].hf == 0,
        "field identity is sorted by physical offset, not element encounter "
        "order");
  // Packed formats are byte-addressed; a valid unaligned field is not a host
  // pointer.
  auto odd = file();
  put(odd, 0x50, 0);
  for (unsigned i = 0; i < 28; ++i)
    odd[0x141 + i] = odd[0x110 + i];
  put(odd, 0xcc, 0x141);
  put(odd, 0xf0, 0x141);
  check(T::decode(odd).heightfields[0].offset == 0x141,
        "valid unaligned field reads do not rely on host pointer alignment");
}
void malformed() {
  const auto original = file();
  for (std::size_t length = 0; length < 0x84; ++length) {
    Bytes b(original.begin(), original.begin() + length);
    rejects([&] { T::decode(b); }, "all incomplete fixed headers refuse");
  }
  const std::vector<std::pair<std::size_t, std::uint32_t>> invalid{
      {0, 0},
      {0, 65536},
      {0x24, 0x83},
      {0x24, 0xffffffff},
      {4, 0x83},
      {4, 0x21c},
      {0x10, 4097},
      {0x14, 4097},
      {0x18, 0xffffffff},
      {0x18, 0},
      {0x78, 0x83},
      {0x78, 0x220},
      {0x1c, 1000001},
      {0x20, 0xffffffff},
      {0x30, 8193},
      {0x34, 8193},
      {0x38, 0xffffffff},
      {0x38, 0},
      {0x50, 10001},
      {0x54, 0xffffffff},
      {0x58, 2},
      {0x5c, 20000},
      {0x64, 0xffffffff},
      {0x68, 0x7f800000},
      {0x68, 0x7fc00001},
      {0x68, 0xbf800000},
      {0x6c, 0x220},
      {0x80, 0x220},
      {0x84, 0x220},
      {0xcc, 0x83},
      {0xcc, 0xffffffff},
      {0x118, 0},
      {0x11c, 4097},
      {0x118, 4096},
      {0x170, 0xffffffff}};
  for (const auto &change : invalid) {
    auto b = original;
    put(b, change.first, change.second);
    const auto malformed_input = b;
    rejects([&] { T::decode(b); }, "malformed header/count/offset must refuse");
    check(b == malformed_input, "failure does not rewrite malformed input");
  }
  for (unsigned lane = 0; lane < 6; ++lane) {
    auto b = original;
    put(b, 0x8c + 4 * lane, lane & 1 ? 0xff800000 : 0x7fc00001);
    rejects([&] { T::decode(b); },
            "all six chunk bound words require finite values");
  }
  auto tree = original;
  put(tree, 4, 0x200);
  rejects([&] { T::decode(tree); },
          "present tree requires marker even when geometry unparsed");
  put(tree, 0x200, 0x4567, 2);
  check(T::decode(tree).elements.size() == 2,
        "valid tree marker suffices for terrain-only projection");
  for (std::uint32_t raw : {2u, 0xfffeu}) {
    auto b = original;
    put(b, 0x180, raw, 2);
    rejects([&] { T::decode(b); },
            "cell list rejects outside or negative nonterminator indices");
  }
  auto no_end = original;
  put(no_end, 0x170, 0x9e);
  put(no_end, 0x21e, 0, 2);
  rejects([&] { T::decode(no_end); },
          "list must encounter terminator before file end");
  auto half = original;
  put(half, 0x170, 0x9f);
  rejects([&] { T::decode(half); },
          "last-byte list address does not permit a two-byte read");
  for (const auto &change : std::vector<std::pair<std::size_t, std::uint32_t>>{
           {0x1a0, 0xffffffff}, {0x1a4, 0xffff}, {0x1b8, 2}, {0x1b8, 0xffff}}) {
    auto b = original;
    put(b, change.first, change.second, change.first == 0x1a0 ? 4 : 2);
    rejects([&] { T::decode(b); },
            "piece signed references/counts stay bounded");
  }
  auto bit = original;
  put(bit, 0x4c, 0x210);
  put(bit, 0x44, 65536);
  put(bit, 0x48, 65536);
  rejects([&] { T::decode(bit); },
          "bit-grid product cannot overflow or escape file");
  auto minimal = original;
  put(minimal, 0x10, 0);
  put(minimal, 0x14, 0);
  put(minimal, 0x18, 0);
  put(minimal, 0x30, 0);
  put(minimal, 0x34, 0);
  put(minimal, 0x38, 0);
  check(T::decode(minimal).grid.empty() &&
            T::decode(minimal).path_cells.empty(),
        "zero-sized absent topology is a supported empty domain");
}
void budgets() {
  const auto b = file();
  T::ParseLimits l;
  l.max_input_bytes = b.size() - 1;
  rejects<T::ResourceLimit>([&] { T::decode(b, l); },
                            "input cap applies before decode");
  l = {};
  l.max_chunks = 0;
  rejects<T::ResourceLimit>([&] { T::decode(b, l); },
                            "chunk workload is bounded");
  l = {};
  l.max_pieces = 1;
  rejects<T::ResourceLimit>([&] { T::decode(b, l); },
                            "piece count budget precedes traversal");
  l = {};
  l.max_list_steps = 3;
  rejects<T::ResourceLimit>([&] { T::decode(b, l); },
                            "list terminator consumes one aggregate visit");
  l.max_list_steps = 4;
  check(T::decode(b, l).grid[0].size() == 3,
        "exact list budget includes terminator");
  l = {};
  l.max_fixup_samples = 7;
  rejects<T::ResourceLimit>([&] { T::decode(b, l); },
                            "repeated alias fixups each consume visits");
  l.max_fixup_samples = 8;
  check(T::decode(b, l).heightfields[0].hi == 2000,
        "exact aggregate fixup budget succeeds");
  for (unsigned which = 0; which < 6; ++which) {
    l = {};
    switch (which) {
    case 0:
      l.level_limits.heightfields = 0;
      break;
    case 1:
      l.level_limits.elements = 1;
      break;
    case 2:
      l.level_limits.grid_cells = 0;
      break;
    case 3:
      l.level_limits.samples = 3;
      break;
    case 4:
      l.level_limits.references = 2;
      break;
    default:
      l.level_limits.path_cells = 1;
      break;
    }
    rejects<T::ResourceLimit>([&] { T::decode(b, l); },
                              "decoded allocation budget is enforced");
  }
  l = {};
  l.max_input_bytes++;
  rejects<T::Error>([&] { T::decode(b, l); },
                    "callers cannot raise parser hard caps");
}
void contexts() {
  for (unsigned config = 0; config < 16; ++config) {
    auto &context = R::set_fpu(static_cast<R::Fpu::Model>(config / 4),
                               (config & 2) != 0, (config & 1) != 0);
    auto b = file();
    put(b, 0x50, 0);
    put(b, 0x1c, 1);
    put(b, 0x180, 0, 2);
    put(b, 0x182, 0xffff, 2);
    put(b, 0xd0, 0);
    put(b, 0xdc, 0);
    put(b, 0xe0, 32767, 2);
    put(b, 0xe2, 0, 2);
    for (unsigned i = 0; i < 4; ++i)
      put(b, 0x124 + 2 * i, 32, 2);
    W::Level level(T::decode(b));
    check(
        W::terrain_height_at_point(level, {0, 0, 0x41200000}) == 0x40000000,
        "decoded field yields literal height2 under every arithmetic context");
    check(level.data().stamp == 0 && level.data().elements[0].stamp == 0,
          "point-height query reads decoded data without advancing stamps");
    level.next_stamp();
    check(W::collect_elements(level, 0, 0, 0, 0, 64) ==
                  std::vector<std::size_t>{0} &&
              level.data().elements[0].stamp == 1,
          "collection uses caller stamp and mutates decoded element visits");
    check(context.model == static_cast<R::Fpu::Model>(config / 4) &&
              context.mul_bias == ((config & 2) != 0) &&
              context.div_nearest == ((config & 1) != 0),
          "decode and queries preserve chosen arithmetic model");
  }
}
} // namespace
int main() {
  try {
    decoded_values();
    malformed();
    budgets();
    contexts();
    std::cout << checks << " terrain file checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "terrain file check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
