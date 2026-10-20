#include "champions/level_texture.hpp"
#include <cstdint>

namespace champions::presentation::rta::level_texture {
namespace {
constexpr std::uint32_t kOtherDrawer = 0x80u | 0x800u | 0x4000u | 0x8000u;
constexpr double kEnvScale = 6.0;
constexpr double kEnvBias = 192.0;
constexpr double kEnvScreen = 0.07;
constexpr int kAutomapStep = 160;
constexpr int kAutomapSpan = 158;

void geometry_only(std::uint32_t flags) {
  if (flags & kOtherDrawer)
    throw Error("flags select water, a model, an emitter or a light");
}

int positive_mod256(int value) {
  int wrapped = value % 256;
  if (wrapped < 0)
    wrapped += 256;
  return wrapped;
}

// (u edge, v edge) for orientations 0..7. 0 is the tile origin, 1 is origin+158.
constexpr int kOrient[8][4][2] = {
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {{1, 0}, {1, 1}, {0, 0}, {0, 1}},
    {{0, 1}, {0, 0}, {1, 1}, {1, 0}}, {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {{0, 1}, {1, 1}, {0, 0}, {1, 0}},
    {{1, 0}, {0, 0}, {1, 1}, {0, 1}}, {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
};

std::uint16_t u16_at(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  if (at + 2 > bytes.size())
    throw Error("automap grid is truncated");
  return std::uint16_t(bytes[at] | (std::uint16_t(bytes[at + 1]) << 8));
}
std::int16_t i16_at(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  const auto n = u16_at(bytes, at);
  return n < 0x8000 ? std::int16_t(n) : std::int16_t(std::int32_t(n) - 0x10000);
}
std::uint32_t u32_at(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  if (at + 4 > bytes.size())
    throw Error("automap grid offset is truncated");
  return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) |
         (std::uint32_t(bytes[at + 2]) << 16) |
         (std::uint32_t(bytes[at + 3]) << 24);
}
} // namespace

ChunkGs chunk_gs(std::uint32_t flags) {
  geometry_only(flags);
  ChunkGs gs;
  gs.env = (flags & 0x10u) != 0;
  if (gs.env) {
    gs.alpha1 = 9;
    gs.alpha2_set = true;
    gs.alpha2 = 0x2a;
  } else if (flags & 0x10000u) {
    gs.alpha1 = 0x48;
  } else if (flags & 0x600u) {
    gs.alpha1 = 1;
  } else {
    gs.alpha1 = 2;
  }
  gs.test = (flags & 0x400u) ? 0x582f5u : 0x587f4u;
  gs.layer = (flags & 0x200u) ? 4 : 0;
  return gs;
}

ChunkLight chunk_light(std::uint32_t flags) {
  geometry_only(flags);
  ChunkLight light;
  if (flags & 0x100u) {
    light.kind = ChunkLightKind::Flash;
    light.constant = -230;
    light.flags_after = flags & ~0x100u;
  } else if (flags & 0x2000u) {
    light.kind = ChunkLightKind::Full;
    light.constant = 230;
    light.flags_after = flags;
  } else if (flags & 0x200u) {
    light.kind = ChunkLightKind::Flat;
    light.constant = 0;
    light.flags_after = flags;
  } else {
    light.kind = ChunkLightKind::Dynamic;
    light.flags_after = flags;
  }
  return light;
}

int world_constant_channel(int base, int added) {
  const long long sum = static_cast<long long>(base) + added;
  if (sum < 0)
    return positive_mod256(static_cast<int>(sum * 2));
  if (sum > 255)
    return 255;
  return static_cast<int>(sum);
}

std::array<double, 6> axis_xy(const std::array<double, 9> &camera) {
  return {camera[0], camera[1], camera[3], camera[4], camera[6], camera[7]};
}

std::array<double, 6> quarter_turn(const std::array<double, 6> &axis, int code) {
  const double x0 = axis[0], x1 = axis[1], y0 = axis[2], y1 = axis[3],
               z0 = axis[4], z1 = axis[5];
  const int bit0 = code & 1, bit1 = (code >> 1) & 1, bit2 = (code >> 2) & 1;
  double a0 = bit2 ? y0 : x0, a1 = bit2 ? y1 : x1;
  double b0 = bit2 ? x0 : y0, b1 = bit2 ? x1 : y1;
  if (bit0) {
    a0 = -a0;
    a1 = -a1;
  }
  if (bit1) {
    b0 = -b0;
    b1 = -b1;
  }
  return {a0, a1, b0, b1, z0, z1};
}

std::pair<std::array<double, 4>, std::array<double, 4>>
env_rows(const std::array<double, 6> &axis, std::uint32_t frame,
         std::uint32_t level_options, const std::optional<int> &quarter) {
  const auto turned = quarter ? quarter_turn(axis, *quarter & 7) : axis;
  const double scroll =
      (level_options & 4u) ? static_cast<double>(frame & 0x3ffu) / 4.0 : 0.0;
  return {{kEnvScale * turned[0], kEnvScale * turned[1], kEnvScale * turned[2],
           kEnvScale * turned[3]},
          {kEnvScale * turned[4], kEnvScale * turned[5], kEnvBias + scroll,
           kEnvBias}};
}

std::array<double, 2> env_uv(const std::array<double, 3> &normal,
                             const std::array<double, 4> &row4,
                             const std::array<double, 4> &row5, double sx,
                             double sy) {
  const double nx = normal[0] / 16.0, ny = normal[1] / 16.0,
               nz = normal[2] / 16.0;
  return {row5[2] + nx * row4[0] + ny * row4[2] + nz * row5[0] + kEnvScreen * sx,
          row5[3] + nx * row4[1] + ny * row4[3] + nz * row5[1] + kEnvScreen * sy};
}

AutomapFields automap_fields(std::uint16_t cell) {
  AutomapFields fields;
  fields.col = cell & 0x1f;
  fields.orient = (cell >> 5) & 7;
  fields.row = (cell >> 8) & 0xf;
  fields.explored = (cell >> 12) & 0xf;
  fields.alpha = (cell >> 8) & 0xff;
  return fields;
}

std::array<int, 4> automap_uv_rect(int col, int row) {
  const int u0 = col * kAutomapStep, v0 = row * kAutomapStep;
  return {u0, v0, u0 + kAutomapSpan, v0 + kAutomapSpan};
}

std::array<AutomapCorner, 4> automap_quad(std::uint16_t cell_c_r,
                                          std::uint16_t cell_c_r1,
                                          std::uint16_t cell_c1_r,
                                          std::uint16_t cell_c1_r1) {
  const std::uint16_t corners[4] = {cell_c_r, cell_c_r1, cell_c1_r, cell_c1_r1};
  const auto src = automap_fields(cell_c1_r1);
  const auto rect = automap_uv_rect(src.col, src.row);
  const int edges[2][2] = {{rect[0], rect[1]}, {rect[2], rect[3]}};
  std::array<AutomapCorner, 4> out;
  if (src.orient < 0 || src.orient > 7)
    throw Error("automap orientation is outside 0..7");
  for (int i = 0; i < 4; ++i) {
    const int ue = kOrient[src.orient][i][0], ve = kOrient[src.orient][i][1];
    out[static_cast<std::size_t>(i)] = {edges[ue][0], edges[ve][1],
                                        automap_fields(corners[i]).alpha};
  }
  return out;
}

AutomapGrid read_automap_grid(const std::vector<std::uint8_t> &world) {
  if (world.size() < 0x84)
    throw Error("WORLD header has no automap grid offset");
  const auto offset = std::uint64_t(u32_at(world, 0x80));
  if (offset > world.size() || world.size() - static_cast<std::size_t>(offset) < 8)
    throw Error("automap grid header is outside the WORLD");
  AutomapGrid grid;
  const auto at = static_cast<std::size_t>(offset);
  grid.origin_x = i16_at(world, at);
  grid.origin_y = i16_at(world, at + 2);
  grid.width = i16_at(world, at + 4);
  grid.height = i16_at(world, at + 6);
  if (grid.width < 0 || grid.height < 0)
    throw Error("automap grid size is negative");
  const auto count = std::uint64_t(grid.width) * std::uint64_t(grid.height);
  if (count > (world.size() - at - 8) / 2)
    throw Error("automap grid cells are outside the WORLD");
  grid.cells.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t i = 0; i < count; ++i)
    grid.cells.push_back(u16_at(world, at + 8 + static_cast<std::size_t>(i) * 2));
  return grid;
}
} // namespace champions::presentation::rta::level_texture
