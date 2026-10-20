#include "champions/level_texture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace champions::presentation::rta::level_texture;
namespace {
int checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <typename F> void rejects(F f, const char *why) {
  bool thrown = false;
  try {
    f();
  } catch (const Error &) {
    thrown = true;
  }
  check(thrown, why);
}
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
} // namespace
int main() {
  try {
    const auto plain = chunk_gs(0);
    check(plain.alpha1 == 2 && !plain.alpha2_set && plain.test == 0x587f4 &&
              plain.layer == 0 && !plain.env,
          "default chunk darkens and depth-tests");
    const auto dynamic = chunk_light(0);
    check(dynamic.kind == ChunkLightKind::Dynamic && !dynamic.constant &&
              dynamic.flags_after == 0,
          "default chunk uses the dynamic light call");

    check(chunk_gs(0x10).alpha2 == 0x2a && chunk_gs(0x10).alpha2_set &&
              chunk_gs(0x10).alpha1 == 9,
          "environment chunk writes both alpha registers");
    check(chunk_gs(0x200).layer == 4 && chunk_gs(0x200).alpha1 == 1,
          "flag 0x200 is layer 4 and alpha 1");
    check(chunk_gs(0x400).test == 0x582f5 && chunk_gs(0x400).layer == 0,
          "flag 0x400 cuts alpha at or above 0x2f");
    check(chunk_gs(0x600).layer == 4 && chunk_gs(0x600).test == 0x582f5,
          "flags 0x200 and 0x400 together");
    check(chunk_gs(0x10000).alpha1 == 0x48 && !chunk_gs(0x10000).alpha2_set,
          "flag 0x10000 is alpha 0x48");
    check(chunk_gs(0x410).test == 0x582f5 && chunk_gs(0x410).alpha2 == 0x2a,
          "environment cutout keeps context 2");

    const auto flash = chunk_light(0x100);
    check(flash.kind == ChunkLightKind::Flash && flash.constant == -230 &&
              flash.flags_after == 0,
          "flag 0x100 flashes at -230 and clears itself");
    check(chunk_light(0x110).constant == -230 &&
              chunk_light(0x110).flags_after == 0x10,
          "the flash clear keeps every other bit");
    check(chunk_light(0x2000).kind == ChunkLightKind::Full &&
              chunk_light(0x2000).constant == 230 &&
              chunk_light(0x2000).flags_after == 0x2000,
          "flag 0x2000 forces +230");
    check(chunk_light(0x2200).constant == 230 &&
              chunk_light(0x2200).flags_after == 0x2200 &&
              chunk_gs(0x2200).alpha1 == 1 && chunk_gs(0x2200).layer == 4,
          "0x2000 wins over 0x200");
    check(chunk_light(0x2100).kind == ChunkLightKind::Flash &&
              chunk_light(0x2100).constant == -230 &&
              chunk_light(0x2100).flags_after == 0x2000,
          "0x100 wins over 0x2000");
    check(chunk_light(0x200).kind == ChunkLightKind::Flat &&
              chunk_light(0x200).constant == 0,
          "flag 0x200 is the flat constant 0");
    check(world_constant_channel(128, -230) == 52 &&
              world_constant_channel(64, -230) == 180 &&
              world_constant_channel(32, -230) == 116,
          "negative flash wraps as two times the sum mod 256");
    check(world_constant_channel(128, 230) == 255 &&
              world_constant_channel(128, 0) == 128,
          "non-negative constants clamp at 255");
    for (const auto bit : {0x80u, 0x800u, 0x4000u, 0x8000u}) {
      rejects([&] { chunk_gs(bit); }, "other chunk kinds are refused");
      rejects([&] { chunk_light(bit); }, "other chunk lights are refused");
    }

    const std::array<double, 9> camera{1, 2, 3, 4, 5, 6, 7, 8, 9};
    check(axis_xy(camera) == std::array<double, 6>{1, 2, 4, 5, 7, 8},
          "axis xy skips each axis z");
    const std::array<double, 6> axis{1, 2, 4, 5, 7, 8};
    const auto rows = env_rows(axis, 0, 0, std::nullopt);
    check(rows.first == std::array<double, 4>{6, 12, 24, 30} &&
              rows.second == std::array<double, 4>{42, 48, 192, 192},
          "rows scale by six and bias 192");
    const std::array<std::array<double, 4>, 8> turned{{
        {6, 12, 24, 30},
        {-6, -12, 24, 30},
        {6, 12, -24, -30},
        {-6, -12, -24, -30},
        {24, 30, 6, 12},
        {-24, -30, 6, 12},
        {24, 30, -6, -12},
        {-24, -30, -6, -12},
    }};
    for (int code = 0; code < 8; ++code) {
      const auto turned_rows = env_rows(axis, 0, 0, std::optional<int>(code));
      check(turned_rows.first == turned[static_cast<std::size_t>(code)] &&
                turned_rows.second[0] == 42 && turned_rows.second[1] == 48,
            "quarter turn permutes only the x and y pairs");
    }
    const std::array<double, 6> zero{};
    check(env_rows(zero, 100, 0, std::nullopt).second[2] == 192,
          "scroll stays off without level option bit 2");
    check(env_rows(zero, 100, 4, std::nullopt).second[2] == 217,
          "frame 100 scrolls by a quarter");
    check(env_rows(zero, 0x400, 4, std::nullopt).second[2] == 192,
          "frame mask clears at 0x400");
    check(env_rows(zero, 0x3ff, 4, std::nullopt).second[2] == 447.75,
          "frame 0x3ff scrolls to 447.75");
    check(env_rows(zero, 50, 5, std::nullopt).second[2] == 204.5,
          "option bit 2 is the value 4");
    const auto uv_rows = env_rows(std::array<double, 6>{1, 0, 0, 0, 0, 0}, 0, 0,
                                  std::nullopt);
    const auto uv = env_uv({16, 0, 0}, uv_rows.first, uv_rows.second, 10, -20);
    check(near(uv[0], 192 + 6 + 0.07 * 10) && near(uv[1], 192 + 0.07 * -20),
          "environment uv divides the stored normal by 16");

    const auto wide = automap_fields(0x0f1f);
    check(wide.col == 31 && wide.row == 15 && wide.orient == 0 &&
              wide.explored == 0 && wide.alpha == 0x0f,
          "automap fields of column 31 row 15");
    const auto marked_fields = automap_fields(0xf562);
    check(marked_fields.col == 2 && marked_fields.orient == 3 &&
              marked_fields.row == 5 && marked_fields.explored == 15 &&
              marked_fields.alpha == 0xf5,
          "automap fields pack the high byte as alpha");
    const std::uint16_t same =
        static_cast<std::uint16_t>(0xf000 | (2 << 8) | 1);
    const auto same_quad = automap_quad(same, same, same, same);
    const int same_uv[4][2] = {{160, 320}, {160, 478}, {318, 320}, {318, 478}};
    for (int i = 0; i < 4; ++i)
      check(same_quad[static_cast<std::size_t>(i)].u == same_uv[i][0] &&
                same_quad[static_cast<std::size_t>(i)].v == same_uv[i][1] &&
                same_quad[static_cast<std::size_t>(i)].alpha == 0xf2,
            "orientation 0 uses origin and origin+158");
    const std::uint16_t plain_cell = 0xf000;
    const std::uint16_t marked =
        static_cast<std::uint16_t>(0xf000 | (5 << 5) | 1 | (2 << 8));
    const auto tile = automap_quad(plain_cell, plain_cell, plain_cell, marked);
    const int tile_uv[4][2] = {{160, 478}, {318, 478}, {160, 320}, {318, 320}};
    const int tile_alpha[4] = {0xf0, 0xf0, 0xf0, 0xf2};
    for (int i = 0; i < 4; ++i)
      check(tile[static_cast<std::size_t>(i)].u == tile_uv[i][0] &&
                tile[static_cast<std::size_t>(i)].v == tile_uv[i][1] &&
                tile[static_cast<std::size_t>(i)].alpha == tile_alpha[i],
            "the tile and orientation come from the bottom-right cell");
    std::vector<std::array<std::pair<int, int>, 4>> seen;
    for (int orient = 0; orient < 8; ++orient) {
      const auto code = static_cast<std::uint16_t>(0xf000 | (orient << 5) | 1 |
                                                   (2 << 8));
      const auto quad = automap_quad(code, code, code, code);
      std::array<std::pair<int, int>, 4> key;
      for (int i = 0; i < 4; ++i)
        key[static_cast<std::size_t>(i)] = {
            quad[static_cast<std::size_t>(i)].u,
            quad[static_cast<std::size_t>(i)].v};
      check(std::find(seen.begin(), seen.end(), key) == seen.end(),
            "automap orientations are distinct");
      seen.push_back(key);
    }

    std::vector<std::uint8_t> grid(0x84 + 8 + 8);
    grid[0x80] = 0x84;
    auto put16 = [&](std::size_t at, int value) {
      grid[at] = static_cast<std::uint8_t>(value);
      grid[at + 1] = static_cast<std::uint8_t>(value >> 8);
    };
    put16(0x84, 1);
    put16(0x86, 2);
    put16(0x88, 2);
    put16(0x8a, 2);
    put16(0x8c, plain_cell);
    put16(0x8e, plain_cell);
    put16(0x90, plain_cell);
    put16(0x92, marked);
    const auto parsed = read_automap_grid(grid);
    check(parsed.origin_x == 1 && parsed.origin_y == 2 && parsed.width == 2 &&
              parsed.height == 2 && parsed.cells.size() == 4 &&
              parsed.cells[3] == marked,
          "automap grid is origin, size, then row-major cells");
    const auto from_file =
        automap_quad(parsed.cells[0], parsed.cells[2], parsed.cells[1],
                     parsed.cells[3]);
    // Row-major (c, r), (c+1, r), (c, r+1), (c+1, r+1) is not the corner
    // argument order. The quad arguments are (c, r), (c, r+1), (c+1, r),
    // (c+1, r+1): cells 0, 2, 1, 3 in a 2-wide row-major grid.
    check(from_file[3].u == 318 && from_file[3].v == 320 &&
              from_file[3].alpha == 0xf2,
          "a 2x2 grid quad uses the bottom-right cell");

    using Item = std::tuple<int, bool, std::int64_t, std::uint32_t, std::string>;
    const std::vector<Item> packets = {
        {0, true, 1, 0, "a0"}, {0, true, 2, 0, "b"},   {0, true, 1, 0, "a1"},
        {0, true, 3, 1, "front"}, {4, true, 1, 0, "later"},
        {0, false, 0, 0, "notex"},
    };
    const auto sent = sorted_draw_order(packets);
    const std::vector<std::string> layer0 = {"notex", "front", "b", "a0", "a1"};
    check(sent[0] == layer0 && sent[4] == std::vector<std::string>{"later"} &&
              sent[1].empty(),
          "texture groups stay together and flag 0 or no texture goes first");
    rejects([&] { sorted_draw_order(std::vector<Item>{{8, false, 0, 0, "x"}}); },
            "layer 8 is refused");

    std::cout << "level draw: " << checks << " checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << "level draw failure: " << e.what() << '\n';
    return 1;
  }
}
