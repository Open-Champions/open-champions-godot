#include "champions/water_draw.hpp"
#include "champions/core_arithmetic.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace champions::presentation::rta::water_draw {
namespace {
using simulation::rta::Fpu;
using simulation::rta::Raw;

Raw raw_f(double value) {
  const float narrowed = static_cast<float>(value);
  Raw bits = 0;
  static_assert(sizeof(float) == 4, "water draw expects IEEE-754 binary32");
  std::memcpy(&bits, &narrowed, 4);
  return bits;
}
float f32(Raw bits) {
  float value = 0;
  std::memcpy(&value, &bits, 4);
  return value;
}
double promoted(Raw bits) { return static_cast<double>(f32(bits)); }

Fpu cpu() { return Fpu(Fpu::Model::Ps2); }
Raw cpu_add(Raw a, Raw b) { return cpu().add(a, b); }
Raw cpu_sub(Raw a, Raw b) { return cpu().sub(a, b); }
Raw cpu_mul(Raw a, Raw b) { return cpu().mul(a, b); }
Raw cpu_div(Raw a, Raw b) { return cpu().div(a, b); }
Raw cpu_from_int(std::uint32_t value) { return cpu().from_int(value); }
int cpu_cmp(Raw a, Raw b) { return Fpu::compare(a, b); }
std::int32_t cpu_to_int(Raw value) { return Fpu::to_int(value); }
Raw from_i32(std::int32_t value) {
  return cpu_from_int(static_cast<std::uint32_t>(value));
}
std::int16_t low_s16(std::uint32_t bits) {
  return static_cast<std::int16_t>(bits & 0xffffu);
}
std::uint32_t raw_of(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, 4);
  return bits;
}

Fpu vu_fpu() { return Fpu(Fpu::Model::Chop); }
Raw vu_add(Raw a, Raw b) { return vu_fpu().add(a, b); }
Raw vu_mul(Raw a, Raw b) { return vu_fpu().mul(a, b); }
Raw vu_div(Raw a, Raw b) { return vu_fpu().div(a, b); }
Raw vu_from_int(std::uint32_t value) { return vu_fpu().from_int(value); }
std::int32_t vu_to_int(Raw value) { return Fpu::to_int(value); }

Raw itof4(std::int32_t sample) {
  return vu_mul(vu_from_int(static_cast<std::uint32_t>(sample)), raw_f(1.0 / 16.0));
}
std::uint32_t ftoi4(Raw value) {
  return static_cast<std::uint32_t>(vu_to_int(vu_mul(value, raw_f(16.0))));
}
std::uint32_t slope_from_delta(Raw delta) {
  return ftoi4(vu_add(raw_f(32.0), vu_mul(UV_SCALE, delta))) & 0x3fffu;
}

std::int16_t sat16(std::int32_t value) {
  if (value > 32767)
    return 32767;
  if (value < -32768)
    return -32768;
  return static_cast<std::int16_t>(value);
}
std::array<std::int16_t, 8> padd(const std::array<std::int16_t, 8> &a,
                                 const std::array<std::int16_t, 8> &b) {
  std::array<std::int16_t, 8> out{};
  for (int i = 0; i < 8; ++i)
    out[static_cast<std::size_t>(i)] =
        sat16(std::int32_t(a[static_cast<std::size_t>(i)]) +
              std::int32_t(b[static_cast<std::size_t>(i)]));
  return out;
}
std::array<std::int16_t, 8> psub(const std::array<std::int16_t, 8> &a,
                                 const std::array<std::int16_t, 8> &b) {
  std::array<std::int16_t, 8> out{};
  for (int i = 0; i < 8; ++i)
    out[static_cast<std::size_t>(i)] =
        sat16(std::int32_t(a[static_cast<std::size_t>(i)]) -
              std::int32_t(b[static_cast<std::size_t>(i)]));
  return out;
}
std::int16_t arithmetic_shift(std::int16_t value, int n) {
  const std::int32_t v = value;
  if (v < 0)
    return static_cast<std::int16_t>(~(~v >> n));
  return static_cast<std::int16_t>(v >> n);
}
std::array<std::int16_t, 8> psrah(const std::array<std::int16_t, 8> &a, int n) {
  std::array<std::int16_t, 8> out{};
  for (int i = 0; i < 8; ++i)
    out[static_cast<std::size_t>(i)] =
        arithmetic_shift(a[static_cast<std::size_t>(i)], n);
  return out;
}
std::array<std::int16_t, 8> psllh(const std::array<std::int16_t, 8> &a, int n) {
  std::array<std::int16_t, 8> out{};
  for (int i = 0; i < 8; ++i) {
    const auto v = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(a[static_cast<std::size_t>(i)]) << n);
    out[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(v);
  }
  return out;
}
std::array<std::int16_t, 8> pand(const std::array<std::int16_t, 8> &a,
                                 const std::array<std::int16_t, 8> &b) {
  std::array<std::int16_t, 8> out{};
  for (int i = 0; i < 8; ++i) {
    const auto v = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(a[static_cast<std::size_t>(i)]) &
        static_cast<std::uint16_t>(b[static_cast<std::size_t>(i)]));
    out[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(v);
  }
  return out;
}
// 16 halfwords, low group in the low bytes, then a byte shift down.
std::array<std::int16_t, 8> qfsrv(const std::array<std::int16_t, 8> &high,
                                  const std::array<std::int16_t, 8> &low,
                                  int sa_bytes) {
  std::uint8_t bytes[32] = {};
  for (int i = 0; i < 8; ++i) {
    const auto lo = static_cast<std::uint16_t>(low[static_cast<std::size_t>(i)]);
    const auto hi = static_cast<std::uint16_t>(high[static_cast<std::size_t>(i)]);
    bytes[i * 2] = static_cast<std::uint8_t>(lo & 0xffu);
    bytes[i * 2 + 1] = static_cast<std::uint8_t>(lo >> 8);
    bytes[16 + i * 2] = static_cast<std::uint8_t>(hi & 0xffu);
    bytes[16 + i * 2 + 1] = static_cast<std::uint8_t>(hi >> 8);
  }
  std::array<std::int16_t, 8> out{};
  for (int i = 0; i < 8; ++i) {
    const int at = sa_bytes + i * 2;
    std::uint16_t v = 0;
    if (at < 32)
      v = bytes[at];
    if (at + 1 < 32)
      v = static_cast<std::uint16_t>(v | (std::uint16_t(bytes[at + 1]) << 8));
    out[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(v);
  }
  return out;
}
using Groups = std::array<std::array<std::int16_t, 8>, 4>;
Groups groups_of(const std::array<std::int16_t, SAMPLES> &row) {
  Groups out{};
  for (int g = 0; g < 4; ++g)
    for (int i = 0; i < 8; ++i)
      out[static_cast<std::size_t>(g)][static_cast<std::size_t>(i)] =
          row[static_cast<std::size_t>(g * 8 + i)];
  return out;
}
std::array<std::int16_t, SAMPLES> ungroups(const Groups &groups) {
  std::array<std::int16_t, SAMPLES> out{};
  for (int g = 0; g < 4; ++g)
    for (int i = 0; i < 8; ++i)
      out[static_cast<std::size_t>(g * 8 + i)] =
          groups[static_cast<std::size_t>(g)][static_cast<std::size_t>(i)];
  return out;
}
std::array<std::int16_t, SAMPLES> row_of(const std::int16_t *src) {
  std::array<std::int16_t, SAMPLES> out{};
  for (int i = 0; i < SAMPLES; ++i)
    out[static_cast<std::size_t>(i)] = src[i];
  return out;
}

std::array<Raw, 4> transform(const std::array<std::array<Raw, 4>, 4> &matrix,
                             Raw hx, Raw column, Raw row) {
  std::array<Raw, 4> acc{};
  const Raw lanes[4] = {hx, column, row, raw_f(1.0)};
  for (int lane = 0; lane < 4; ++lane)
    for (int i = 0; i < 4; ++i)
      acc[static_cast<std::size_t>(i)] =
          vu_add(acc[static_cast<std::size_t>(i)],
                 vu_mul(matrix[static_cast<std::size_t>(lane)]
                              [static_cast<std::size_t>(i)],
                        lanes[lane]));
  return acc;
}
std::array<std::uint32_t, 2> pack_xy(const std::array<Raw, 4> &projected) {
  const auto inv_w = vu_div(raw_f(1.0), projected[3]);
  const auto bias = raw_f(526336.0);
  const auto x = ftoi4(vu_add(vu_mul(projected[0], inv_w), bias)) & 0xffffu;
  const auto y = ftoi4(vu_add(vu_mul(projected[1], inv_w), bias)) & 0xffffu;
  return {x, y};
}
bool outside(std::uint32_t x, std::uint32_t y) {
  const double sx = (x & 0xffffu) / 16.0 - 2048.0;
  const double sy = (y & 0xffffu) / 16.0 - 2048.0;
  return std::fabs(sx) > 1000.0 || std::fabs(sy) > 1000.0;
}

bool quiescent(const Grid &grid) {
  for (const auto &row : grid.heights)
    for (const auto sample : row)
      if (sample)
        return false;
  for (const auto &row : grid.velocity)
    for (const auto sample : row)
      if (sample)
        return false;
  for (const auto sample : grid.gap)
    if (sample)
      return false;
  return true;
}
void copy_column(Grid &dest, const Grid &src, int column, int from_column) {
  for (int row = 1; row < ROWS; ++row)
    dest.heights[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] =
        src.heights[static_cast<std::size_t>(row)]
                   [static_cast<std::size_t>(from_column)];
  for (int row = 0; row < SAMPLES; ++row)
    dest.velocity[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] =
        src.velocity[static_cast<std::size_t>(row)]
                    [static_cast<std::size_t>(from_column)];
}

float bits_to_float(std::uint32_t bits) { return f32(bits); }
} // namespace

GsState gs_state(int constant_set) {
  GsState state;
  if (constant_set == 0) {
    state.fix = static_cast<int>(SET0_FIX);
    state.colour = SET0_COLOUR;
    state.scale = SET0_SCALE;
  } else if (constant_set == 1) {
    state.fix = static_cast<int>(SET1_FIX);
    state.colour = SET1_COLOUR;
    state.scale = SET1_SCALE;
  } else {
    throw Error("water has two constant sets");
  }
  const auto words = alpha_words(state.fix);
  state.alpha_low = words[0];
  state.alpha_high = words[1];
  return state;
}
std::array<int, 4> colour_channels(std::uint32_t word) {
  return {int(word & 0xffu), int((word >> 8) & 0xffu), int((word >> 16) & 0xffu),
          int((word >> 24) & 0xffu)};
}
std::array<std::uint32_t, 2> alpha_words(int fix) {
  if (fix < 0)
    return {ALPHA_ADD, static_cast<std::uint32_t>(-fix)};
  return {ALPHA_BLEND, static_cast<std::uint32_t>(fix)};
}
int draw_layer(std::uint32_t) { return LAYER; }

double floor_half(double value) {
  const auto summed = cpu_add(raw_f(value), raw_f(0.5));
  return std::floor(promoted(summed));
}
std::uint32_t tile_step_bits(double min_x, double max_x) {
  const auto sum = cpu_add(raw_f(max_x), raw_f(min_x) ^ 0x80000000u);
  const auto extent = floor_half(promoted(sum));
  if (extent < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
      extent > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
    throw Error("water tile extent does not fit a signed word");
  return cpu_mul(cpu_from_int(static_cast<std::uint32_t>(
                     static_cast<std::int32_t>(extent))),
                 STEP_SCALE);
}
std::array<double, 3> origin(double min_x, double min_y, std::int32_t chunk_z) {
  const auto zbits =
      cpu_mul(cpu_from_int(static_cast<std::uint32_t>(chunk_z)), raw_f(0.25));
  return {floor_half(min_x), floor_half(min_y), promoted(zbits)};
}
std::array<std::array<Raw, 4>, 4>
upload_matrix(const std::array<std::array<double, 4>, 4> &view,
              const std::array<double, 3> &origin_xyz, Raw scale_bits) {
  std::array<std::array<Raw, 4>, 4> out{};
  for (int i = 0; i < 4; ++i) {
    out[0][static_cast<std::size_t>(i)] =
        cpu_mul(scale_bits, raw_f(view[2][static_cast<std::size_t>(i)]));
    out[1][static_cast<std::size_t>(i)] =
        raw_f(view[1][static_cast<std::size_t>(i)]);
    out[2][static_cast<std::size_t>(i)] =
        raw_f(view[0][static_cast<std::size_t>(i)]);
  }
  const double coefs[4] = {origin_xyz[0], origin_xyz[1], origin_xyz[2], 1.0};
  for (int term = 0; term < 4; ++term) {
    const auto bits = raw_f(coefs[term]);
    const auto &row = view[static_cast<std::size_t>(term)];
    for (int i = 0; i < 4; ++i)
      out[3][static_cast<std::size_t>(i)] = cpu_add(
          out[3][static_cast<std::size_t>(i)],
          cpu_mul(bits, raw_f(row[static_cast<std::size_t>(i)])));
  }
  return out;
}

std::uint32_t slope_uv(std::int16_t from, std::int16_t to) {
  const auto delta = vu_add(itof4(from), itof4(to) ^ 0x80000000u);
  return slope_from_delta(delta);
}

std::vector<std::vector<Vertex>>
project_strips(const std::vector<std::array<std::int16_t, SAMPLES>> &rows,
               const std::array<std::array<Raw, 4>, 4> &matrix, Raw step,
               int clip) {
  std::vector<std::vector<std::array<Raw, 4>>> made(rows.size());
  std::vector<std::vector<std::array<std::uint32_t, 2>>> uvs(rows.size());
  for (std::size_t k = 0; k < rows.size(); ++k) {
    const auto z = vu_mul(raw_f(static_cast<double>(k)), step);
    made[k].resize(31);
    uvs[k].resize(31);
    for (int i = 0; i < 31; ++i) {
      const int column = i + 1;
      const auto here = rows[k][static_cast<std::size_t>(column)];
      const auto left = rows[k][static_cast<std::size_t>(column - 1)];
      const auto prev = k ? rows[k - 1][static_cast<std::size_t>(column)] : 0;
      const auto hx = itof4(here);
      made[k][static_cast<std::size_t>(i)] =
          transform(matrix, hx, vu_mul(raw_f(static_cast<double>(i)), step), z);
      uvs[k][static_cast<std::size_t>(i)] = {slope_uv(prev, here),
                                             slope_uv(left, here)};
    }
  }
  std::vector<std::vector<Vertex>> strips;
  for (std::size_t k = 2; k < rows.size(); ++k) {
    std::vector<Vertex> verts;
    verts.reserve(VERTS_PER_STRIP);
    bool sticky = false;
    for (int i = 0; i < 31; ++i) {
      const auto current = pack_xy(made[k][static_cast<std::size_t>(i)]);
      const auto previous = pack_xy(made[k - 1][static_cast<std::size_t>(i)]);
      const std::array<std::uint32_t, 2> packed[2] = {current, previous};
      const std::array<std::uint32_t, 2> coord[2] = {
          uvs[k][static_cast<std::size_t>(i)],
          uvs[k - 1][static_cast<std::size_t>(i)]};
      for (int which = 0; which < 2; ++which) {
        Vertex vertex;
        vertex.x = packed[which][0];
        vertex.y = packed[which][1];
        vertex.u = coord[which][0];
        vertex.v = coord[which][1];
        if (clip && (sticky || outside(vertex.x, vertex.y))) {
          sticky = true;
          vertex.adc = 1;
        }
        verts.push_back(vertex);
      }
    }
    strips.push_back(std::move(verts));
  }
  return strips;
}

Sample world_sample(const std::vector<std::array<std::int16_t, SAMPLES>> &rows,
                    int call, int column, Raw step,
                    const std::array<double, 3> &origin_xyz, Raw scale_bits) {
  if (call < 0 || column < 0 || column > 30 ||
      static_cast<std::size_t>(call) >= rows.size())
    throw Error("water sample is outside the uploaded grid");
  const int index = column + 1;
  const auto here = rows[static_cast<std::size_t>(call)][static_cast<std::size_t>(index)];
  const auto left =
      rows[static_cast<std::size_t>(call)][static_cast<std::size_t>(index - 1)];
  const auto prev =
      call ? rows[static_cast<std::size_t>(call - 1)][static_cast<std::size_t>(index)]
           : std::int16_t{0};
  const auto along = vu_mul(raw_f(static_cast<double>(call)), step);
  const auto across = vu_mul(raw_f(static_cast<double>(column)), step);
  // Scale meets the world's up axis on the CPU, then the VU multiplies by height/16.
  const auto lifted = vu_mul(itof4(here), cpu_mul(scale_bits, raw_f(1.0)));
  Sample sample;
  sample.x_bits = vu_add(raw_f(origin_xyz[0]), along);
  sample.y_bits = vu_add(raw_f(origin_xyz[1]), across);
  sample.z_bits = vu_add(raw_f(origin_xyz[2]), lifted);
  sample.u = slope_uv(prev, here);
  sample.v = slope_uv(left, here);
  return sample;
}

bool mask_allows(const std::array<std::uint8_t, 128> &mask, int column, int row) {
  if (column < 0 || column > 31 || row < 0 || row > 31)
    throw Error("water mask cell is outside 32 by 32");
  const int base = row < 16 ? 0 : 64;
  const int at = base + column * 2;
  const auto word = static_cast<std::int16_t>(
      mask[static_cast<std::size_t>(at)] |
      (std::uint16_t(mask[static_cast<std::size_t>(at + 1)]) << 8));
  return ((word >> (15 - (row & 15))) & 1) == 1;
}
std::array<std::uint8_t, 128> filled_mask() {
  std::array<std::uint8_t, 128> mask{};
  mask.fill(0xff);
  return mask;
}

Grid step_grid(const Grid &grid, const std::array<std::uint8_t, 128> &mask) {
  Grid field = grid;
  const std::array<std::int16_t, 8> zero{};
  Groups held{};
  Groups older{};
  Groups previous{};
  int row_index = 1;
  int velocity_row = -1;
  for (int phase = 0; phase < 2; ++phase) {
    const int base = phase == 0 ? 0 : 64;
    std::array<std::int16_t, SAMPLES> halfs{};
    for (int c = 0; c < SAMPLES; ++c) {
      const int at = base + c * 2;
      halfs[static_cast<std::size_t>(c)] = static_cast<std::int16_t>(
          mask[static_cast<std::size_t>(at)] |
          (std::uint16_t(mask[static_cast<std::size_t>(at + 1)]) << 8));
    }
    auto gates = groups_of(halfs);
    const int end = row_index + 16;
    while (row_index != end) {
      const Groups saved = held;
      held = groups_of(row_of(field.heights[static_cast<std::size_t>(row_index)].data()));
      Groups right{};
      Groups sums{};
      for (int i = 0; i < 4; ++i) {
        const auto &nxt = i < 3 ? held[static_cast<std::size_t>(i + 1)] : zero;
        right[static_cast<std::size_t>(i)] =
            padd(held[static_cast<std::size_t>(i)],
                 qfsrv(nxt, held[static_cast<std::size_t>(i)], 2));
      }
      for (int i = 0; i < 4; ++i) {
        const auto &prv = i == 0 ? zero : held[static_cast<std::size_t>(i - 1)];
        sums[static_cast<std::size_t>(i)] =
            padd(right[static_cast<std::size_t>(i)],
                 qfsrv(held[static_cast<std::size_t>(i)], prv, 14));
      }
      const auto source =
          velocity_row < 0
              ? row_of(field.gap.data())
              : row_of(field.velocity[static_cast<std::size_t>(velocity_row)].data());
      const auto source_g = groups_of(source);
      Groups out_v{};
      Groups out_h{};
      Groups new_gates{};
      for (int i = 0; i < 4; ++i) {
        auto acc = padd(sums[static_cast<std::size_t>(i)],
                        previous[static_cast<std::size_t>(i)]);
        acc = padd(acc, older[static_cast<std::size_t>(i)]);
        acc = psub(acc, saved[static_cast<std::size_t>(i)]);
        acc = psrah(acc, 3);
        acc = psub(acc, saved[static_cast<std::size_t>(i)]);
        acc = psrah(acc, 2);
        auto vel = padd(source_g[static_cast<std::size_t>(i)], acc);
        const auto bit = psrah(gates[static_cast<std::size_t>(i)], 15);
        vel = pand(vel, bit);
        new_gates[static_cast<std::size_t>(i)] =
            psllh(gates[static_cast<std::size_t>(i)], 1);
        vel = psub(vel, psrah(vel, 7));
        out_v[static_cast<std::size_t>(i)] = vel;
        out_h[static_cast<std::size_t>(i)] =
            padd(saved[static_cast<std::size_t>(i)], vel);
      }
      gates = new_gates;
      const auto flat_v = ungroups(out_v);
      if (velocity_row < 0)
        field.gap = flat_v;
      else
        field.velocity[static_cast<std::size_t>(velocity_row)] = flat_v;
      field.heights[static_cast<std::size_t>(row_index - 1)] = ungroups(out_h);
      older = previous;
      previous = sums;
      ++velocity_row;
      ++row_index;
    }
  }
  return field;
}

Seam seam_columns(const Grid &self_in, const Grid &other_in) {
  Seam out{self_in, other_in};
  for (int row = 1; row < ROWS; ++row) {
    out.self.heights[static_cast<std::size_t>(row)][0] =
        other_in.heights[static_cast<std::size_t>(row)][30];
    out.other.heights[static_cast<std::size_t>(row)][31] =
        self_in.heights[static_cast<std::size_t>(row)][1];
  }
  for (int row = 0; row < SAMPLES; ++row) {
    out.self.velocity[static_cast<std::size_t>(row)][0] =
        other_in.velocity[static_cast<std::size_t>(row)][30];
    out.other.velocity[static_cast<std::size_t>(row)][31] =
        self_in.velocity[static_cast<std::size_t>(row)][1];
  }
  return out;
}
Seam seam_rows(const Grid &self_in, const Grid &other_in) {
  Seam out{self_in, other_in};
  out.self.heights[1] = other_in.heights[31];
  out.other.heights[32] = self_in.heights[2];
  out.self.velocity[0] = other_in.velocity[30];
  out.other.velocity[31] = self_in.velocity[1];
  return out;
}
bool block_expired(std::uint32_t frame, std::uint32_t stamp) {
  auto diff = static_cast<std::uint32_t>(frame - stamp);
  std::int32_t signed_diff = static_cast<std::int32_t>(diff);
  return signed_diff >= KEEP_FRAMES;
}
Splash splash_seed(std::uint32_t seed) {
  const auto next =
      static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * LCG_MUL + LCG_ADD);
  return {next, (next >> 16) % static_cast<std::uint32_t>(SPLASH_MODULUS) == 0};
}
Raw unit_interval(std::uint32_t seed) {
  return cpu_mul(cpu_from_int(seed >> 16), SPLASH_UNIT);
}
Raw along(Raw lo, Raw hi, Raw unit) {
  return cpu_add(lo, cpu_mul(cpu_sub(hi, lo), unit));
}
SplashPoint splash_point(std::uint32_t seed, const std::array<std::uint32_t, 6> &bounds) {
  const auto first = splash_seed(seed);
  SplashPoint out;
  if (!first.fire) {
    out.seed = first.seed;
    return out;
  }
  const auto second = splash_seed(first.seed);
  const auto unit = unit_interval(second.seed);
  out.hit = true;
  out.seed = splash_seed(second.seed).seed;
  out.point = {along(bounds[0], bounds[3], unit), along(bounds[1], bounds[4], unit),
               bounds[2]};
  return out;
}
std::uint32_t second_particle_z(std::uint32_t terrain, std::uint32_t seed) {
  const auto lift = cpu_mul(unit_interval(seed), PARTICLE_RISE);
  return cpu_add(cpu_add(terrain, lift), PARTICLE_LIFT);
}

std::uint32_t colour_word(int red, int green, int blue) {
  return (static_cast<std::uint32_t>(red) & 0xffu) |
         ((static_cast<std::uint32_t>(green) & 0xffu) << 8) |
         ((static_cast<std::uint32_t>(blue) & 0xffu) << 16);
}
std::uint32_t wavyness_scale(std::uint32_t parsed) {
  return cpu_mul(parsed, SET1_SCALE);
}
Appearance reset_appearance() {
  Appearance out;
  out.fix[0] = static_cast<int>(SET1_FIX);
  out.colour[0] = SET1_COLOUR;
  out.scale[0] = SET1_SCALE;
  out.strength = raw_f(WAVE_STRENGTH);
  out.radius = raw_f(4.0);
  out.timer = 6;
  return out;
}
void setup_appearance(Appearance &appearance) {
  appearance.fix[0] = static_cast<int>(SET0_FIX);
  appearance.colour[0] = SET0_COLOUR;
  appearance.scale[0] = SET0_SCALE;
  appearance.strength = raw_f(WAVE_STRENGTH);
  appearance.radius = raw_f(WAVE_RADIUS);
  appearance.timer = SETUP_INTEGER;
}
Appearance load_appearance() {
  auto out = reset_appearance();
  setup_appearance(out);
  return out;
}
// The executed shapes only: an optional minus and digits, or a decimal with
// one optional point. No space, sign plus, or exponent was executed.
std::optional<std::int32_t> strict_int(std::string_view text) {
  std::size_t i = text.size() > 0 && text[0] == '-' ? 1u : 0u;
  if (i == text.size())
    return std::nullopt;
  std::int64_t value = 0;
  for (; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9')
      return std::nullopt;
    value = value * 10 + (text[i] - '0');
    if (value > 0x80000000ll)
      return std::nullopt;
  }
  if (text[0] == '-')
    value = -value;
  if (value > std::numeric_limits<std::int32_t>::max())
    return std::nullopt;
  return static_cast<std::int32_t>(value);
}
std::optional<std::uint32_t> strict_decimal(std::string_view text) {
  std::size_t i = text.size() > 0 && text[0] == '-' ? 1u : 0u;
  int digits = 0;
  int points = 0;
  for (std::size_t j = i; j < text.size(); ++j) {
    if (text[j] == '.')
      ++points;
    else if (text[j] >= '0' && text[j] <= '9')
      ++digits;
    else
      return std::nullopt;
  }
  if (digits == 0 || points > 1)
    return std::nullopt;
  // Decimal to double, then to float, as the difftest packed float(text).
  const auto owned = std::string(text);
  errno = 0;
  char *end = nullptr;
  const double value = std::strtod(owned.c_str(), &end);
  if (end != owned.c_str() + owned.size() || errno == ERANGE ||
      std::fabs(value) > static_cast<double>(std::numeric_limits<float>::max()))
    return std::nullopt;
  return raw_of(static_cast<float>(value));
}
Property apply_property(Appearance &appearance, std::string_view line) {
  const auto eq = line.find('=');
  if (eq == std::string_view::npos)
    return Property::Other;
  const auto key = line.substr(0, eq);
  auto value = line.substr(eq + 1);
  // Level lines end in a carriage return. sscanf stops at the first
  // character a conversion cannot take, so it does not change the stored word.
  while (!value.empty() && value.back() == '\r')
    value.remove_suffix(1);
  const bool colour = key == "waterColor" || key == "waterColor2";
  const bool wave = key == "waterWavyness" || key == "waterWavyness2";
  const bool amount = key == "waterBumpAmount";
  const bool radius = key == "waterBumpRadius";
  const bool timer = key == "waterBumpTimer";
  if (!colour && !wave && !amount && !radius && !timer)
    return Property::Other;
  const std::size_t set = (key.back() == '2') ? 1u : 0u;
  if (colour) {
    std::array<std::int32_t, 4> parts{};
    std::string_view rest = value;
    for (std::size_t i = 0; i < 4; ++i) {
      const auto comma = i < 3 ? rest.find(',') : std::string_view::npos;
      if (i < 3 && comma == std::string_view::npos)
        return Property::Refused;
      const auto got = strict_int(rest.substr(0, comma));
      if (!got)
        return Property::Refused;
      parts[i] = *got;
      rest = i < 3 ? rest.substr(comma + 1) : std::string_view{};
    }
    // The fix becomes the high word of ALPHA_1. Which bits of that word the
    // GS keeps is not in the pin, so a fix that one byte cannot hold is refused.
    if (parts[3] > 255 || parts[3] < -255)
      return Property::Refused;
    // tools/ref_waterdraw.py colour_word keeps the low byte of each part.
    appearance.colour[set] = colour_word(parts[0], parts[1], parts[2]);
    appearance.fix[set] = parts[3];
    return Property::Applied;
  }
  if (timer) {
    const auto got = strict_int(value);
    if (!got)
      return Property::Refused;
    appearance.timer = *got;
    return Property::Applied;
  }
  const auto bits = strict_decimal(value);
  if (!bits)
    return Property::Refused;
  if (wave)
    appearance.scale[set] = wavyness_scale(*bits);
  else if (amount)
    appearance.strength = *bits;
  else
    appearance.radius = *bits;
  return Property::Applied;
}
void count_lines(Parsed &out, const std::vector<std::string> &lines) {
  for (const auto &line : lines) {
    const auto got = apply_property(out.appearance, line);
    out.applied += got == Property::Applied ? 1 : 0;
    out.refused += got == Property::Refused ? 1 : 0;
  }
}
Parsed replacing(const std::vector<std::string> &lines) {
  Parsed out{load_appearance()};
  count_lines(out, lines);
  return out;
}
Parsed before_setup(const std::vector<std::string> &lines) {
  Parsed out{reset_appearance()};
  count_lines(out, lines);
  setup_appearance(out.appearance);
  return out;
}

Claim claim_grid(const std::array<const std::array<std::array<std::int16_t, SAMPLES>, ROWS> *, 4> &slots) {
  Claim out;
  for (int row = 0; row < SAMPLES; ++row) {
    for (int col = 0; col < SAMPLES; ++col) {
      Raw hsum = 0;
      Raw wsum = 0;
      const auto add = [&](std::int16_t sample, int distance) {
        const auto weight = cpu_mul(cpu_from_int(static_cast<std::uint32_t>(distance)), EDGE_WEIGHT);
        hsum = cpu_add(hsum, cpu_mul(from_i32(sample), weight));
        wsum = cpu_add(wsum, weight);
      };
      if (col < 10 && slots[0] != nullptr)
        add((*slots[0])[static_cast<std::size_t>(row + 1)][31], 10 - col);
      if (row < 10 && slots[2] != nullptr)
        add((*slots[2])[32][static_cast<std::size_t>(col)], 10 - row);
      if (col >= 22 && slots[1] != nullptr)
        add((*slots[1])[static_cast<std::size_t>(row + 1)][0], col - 21);
      if (row >= 22 && slots[3] != nullptr)
        add((*slots[3])[1][static_cast<std::size_t>(col)], row - 21);
      if (cpu_cmp(wsum, CLAIM_ONE) < 0)
        wsum = CLAIM_ONE;
      out.heights[static_cast<std::size_t>(row + 1)][static_cast<std::size_t>(col)] =
          low_s16(static_cast<std::uint32_t>(cpu_to_int(cpu_div(hsum, wsum))));
    }
  }
  return out;
}

void disturb_cells(std::array<std::array<std::int16_t, SAMPLES>, SAMPLES> &grid,
                   std::uint32_t strength, std::uint32_t radius,
                   const std::array<std::int32_t, 2> &origin,
                   const std::array<std::uint32_t, 2> &span,
                   const std::array<std::uint32_t, 2> &pos) {
  const auto ox = cpu_mul(from_i32(origin[0]), raw_f(0.25));
  const auto oy = cpu_mul(from_i32(origin[1]), raw_f(0.25));
  const auto width = cpu_sub(span[1], span[0]);
  const auto count = cpu_to_int(cpu_add(cpu_mul(width, DISTURB_WIDTH), raw_f(0.5)));
  if (count <= 0)
    throw Unsupported("water rounded chunk width must be positive");
  const auto inv = cpu_div(CLAIM_ONE, cpu_mul(cpu_from_int(static_cast<std::uint32_t>(count)), raw_f(12.0)));
  const auto scale = raw_f(32.0);
  const auto gx = cpu_add(cpu_mul(cpu_mul(cpu_sub(pos[0], ox), scale), inv), raw_f(16.0));
  const auto gy = cpu_add(cpu_mul(cpu_mul(cpu_sub(pos[1], oy), scale), inv), raw_f(16.0));
  const auto reach = cpu_mul(cpu_mul(radius, scale), inv);
  const auto limit = cpu_mul(reach, reach);
  const auto gain = cpu_div(strength, limit);
  const auto window = [&](Raw centre) {
    const auto lo = cpu_to_int(cpu_sub(cpu_sub(centre, radius), CLAIM_ONE));
    const auto hi = cpu_to_int(cpu_add(cpu_add(centre, radius), CLAIM_ONE));
    return std::array<int, 2>{std::max(0, lo), std::min(31, hi)};
  };
  const auto xw = window(gx);
  const auto yw = window(gy);
  if (xw[1] < xw[0] || yw[1] < yw[0])
    return;
  for (int row = xw[0]; row <= xw[1]; ++row) {
    for (int col = yw[0]; col <= yw[1]; ++col) {
      const auto gap_x = cpu_sub(cpu_from_int(static_cast<std::uint32_t>(row)), gx);
      const auto gap_y = cpu_sub(cpu_from_int(static_cast<std::uint32_t>(col)), gy);
      const auto dist = cpu_add(cpu_mul(gap_x, gap_x), cpu_mul(gap_y, gap_y));
      if (cpu_cmp(limit, dist) < 0)
        continue;
      const auto amount = cpu_to_int(cpu_sub(strength, cpu_mul(dist, gain)));
      const auto total = static_cast<std::uint32_t>(
          static_cast<std::uint32_t>(grid[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)]) +
          static_cast<std::uint32_t>(amount));
      const auto signed_total = simulation::rta::s32(total);
      grid[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] =
          signed_total > 32767 ? 32767 : low_s16(total);
    }
  }
}
void refuse_countdown_splash() {
  throw Unsupported(
      "the simulation splash taken when the countdown is negative was not executed");
}
void refuse_setup_integer() {
  throw Unsupported("nothing documented reads the integer stored by water_setup");
}
void refuse_diagonal_slots() {
  throw Unsupported(
      "neighbour slots 1 and 3 are not read by the per-frame exchange");
}
void refuse_property_order() {
  throw Unsupported(
      "whether the object factory runs before or after water_setup was not executed");
}
void refuse_player_move_disturb() {
  throw Unsupported(
      "Player_move's water_disturb immediates are not in the water-draw pin");
}
void refuse_splash_particles() {
  throw Unsupported(
      "the splash particle definitions are zero in the executable and their names are not in the image");
}
void refuse_splash_start_seed() {
  throw Unsupported(
      "the retail start value of the drawer splash seed at 0x404460 is not in the pin");
}

Field::Field(std::vector<TileSource> tiles) : tiles_(std::move(tiles)) {
  blocks_.resize(tiles_.size());
  std::uint32_t highest = 0;
  for (const auto &tile : tiles_)
    highest = std::max(highest, tile.chunk);
  by_chunk_.assign(static_cast<std::size_t>(highest) + 1u, -1);
  for (std::size_t i = 0; i < tiles_.size(); ++i) {
    if (by_chunk_[tiles_[i].chunk] != -1)
      throw Error("two water tiles share a chunk");
    by_chunk_[tiles_[i].chunk] = static_cast<int>(i);
    for (const auto neighbour : tiles_[i].neighbours) {
      if (neighbour < -1)
        throw Error("water neighbour is below -1");
      if (neighbour >= 0 &&
          static_cast<std::uint32_t>(neighbour) >= by_chunk_.size())
        by_chunk_.resize(static_cast<std::size_t>(neighbour) + 1u, -1);
    }
  }
}
void Field::on_tick(std::uint32_t frame, bool frozen) {
  if (!frozen) {
    for (auto &block : blocks_) {
      if (!block.live)
        continue;
      if (block_expired(frame, block.stamp)) {
        if (!quiescent(block.grid))
          dirty_ = true;
        block.live = false;
        block.grid = {};
      }
    }
    std::vector<Grid> stepped(blocks_.size());
    std::vector<char> ran(blocks_.size(), 0);
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
      if (!blocks_[i].live)
        continue;
      // A zero grid stays zero for every mask, so the step is the same.
      auto next = quiescent(blocks_[i].grid)
                      ? blocks_[i].grid
                      : step_grid(blocks_[i].grid, tiles_[i].mask);
      if (next.heights != blocks_[i].grid.heights ||
          next.velocity != blocks_[i].grid.velocity ||
          next.gap != blocks_[i].grid.gap)
        dirty_ = true;
      stepped[i] = std::move(next);
      ran[i] = 1;
    }
    for (std::size_t i = 0; i < blocks_.size(); ++i)
      if (ran[i])
        blocks_[i].grid = stepped[i];
    auto snapshot = blocks_;
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
      if (!ran[i])
        continue;
      const auto apply = [&](int slot, bool columns) {
        const auto neighbour = tiles_[i].neighbours[static_cast<std::size_t>(slot)];
        if (neighbour < 0)
          return;
        const auto other = by_chunk_.at(static_cast<std::size_t>(neighbour));
        if (other < 0 || !ran[static_cast<std::size_t>(other)])
          return;
        const auto &src_self = snapshot[i].grid;
        const auto &src_other = snapshot[static_cast<std::size_t>(other)].grid;
        const auto seam =
            columns ? seam_columns(src_self, src_other) : seam_rows(src_self, src_other);
        if (columns) {
          copy_column(blocks_[i].grid, seam.self, 0, 0);
          copy_column(blocks_[static_cast<std::size_t>(other)].grid, seam.other, 31,
                      31);
        } else {
          blocks_[i].grid.heights[1] = seam.self.heights[1];
          blocks_[i].grid.velocity[0] = seam.self.velocity[0];
          blocks_[static_cast<std::size_t>(other)].grid.heights[32] =
              seam.other.heights[32];
          blocks_[static_cast<std::size_t>(other)].grid.velocity[31] =
              seam.other.velocity[31];
        }
      };
      apply(0, true);
      apply(2, false);
    }
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
      if (!ran[i])
        continue;
      if (blocks_[i].grid.heights != snapshot[i].grid.heights)
        dirty_ = true;
    }
  }
  for (std::size_t i = 0; i < blocks_.size(); ++i) {
    if (blocks_[i].live)
      continue;
    std::array<const std::array<std::array<std::int16_t, SAMPLES>, ROWS> *, 4> slots{};
    for (int slot = 0; slot < 4; ++slot) {
      const auto neighbour = tiles_[i].neighbours[static_cast<std::size_t>(slot)];
      if (neighbour < 0)
        continue;
      const auto other = by_chunk_.at(static_cast<std::size_t>(neighbour));
      if (other < 0 || !blocks_[static_cast<std::size_t>(other)].live)
        continue;
      slots[static_cast<std::size_t>(slot)] =
          &blocks_[static_cast<std::size_t>(other)].grid.heights;
    }
    const auto claimed = claim_grid(slots);
    if (claimed.heights != blocks_[i].grid.heights)
      dirty_ = true;
    blocks_[i].grid = {};
    blocks_[i].grid.heights = claimed.heights;
    blocks_[i].live = true;
    blocks_[i].stamp = frame;
  }
  if (!frozen && (level_flags_ & LEVEL_OPTION_SPLASH) != 0) {
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
      if (!blocks_[i].live)
        continue;
      const auto splash = splash_point(*splash_seed_, tiles_[i].bounds);
      splash_seed_ = splash.seed;
      if (splash.hit)
        disturb(raw_f(SPLASH_STRENGTH), raw_f(SPLASH_RADIUS), splash.point);
    }
  }
}
std::size_t Field::live_blocks() const {
  std::size_t n = 0;
  for (const auto &block : blocks_)
    n += block.live ? 1u : 0u;
  return n;
}
void Field::set_level_flags(std::uint32_t flags) {
  if ((flags & LEVEL_OPTION_SPLASH) != 0 && !splash_seed_)
    refuse_splash_start_seed();
  level_flags_ = flags;
  if ((flags & LEVEL_OPTION_FILL) == 0)
    return;
  const auto filled = filled_mask();
  for (auto &tile : tiles_)
    tile.mask = filled;
}
void Field::set_appearance(Appearance appearance) {
  if (appearance.colour != appearance_.colour || appearance.fix != appearance_.fix ||
      appearance.scale != appearance_.scale)
    dirty_ = true;
  appearance_ = appearance;
}
std::uint32_t Field::disturb(std::uint32_t strength, std::uint32_t radius,
                             const std::array<std::uint32_t, 3> &position) {
  if (!query_)
    throw Unsupported("water_disturb needs the WORLD chunk query");
  const auto chunks = query_(radius, position);
  for (const auto chunk : chunks) {
    const auto index = chunk < by_chunk_.size() ? by_chunk_[chunk] : -1;
    if (index < 0)
      throw Error("the chunk query returned a chunk that is not a water tile");
    auto &block = blocks_[static_cast<std::size_t>(index)];
    // A chunk with no simulation block is counted and not written.
    if (!block.live)
      continue;
    const auto &tile = tiles_[static_cast<std::size_t>(index)];
    const auto before = block.grid.velocity;
    disturb_cells(block.grid.velocity, strength, radius, tile.origin,
                  {tile.bounds[0], tile.bounds[3]}, {position[0], position[1]});
    if (block.grid.velocity != before)
      dirty_ = true;
  }
  return static_cast<std::uint32_t>(chunks.size());
}
void Field::supply_grid(std::size_t index, Grid grid) {
  if (!blocks_.at(index).live)
    throw Error("a water block is supplied before it is claimed");
  if (grid.heights != blocks_[index].grid.heights)
    dirty_ = true;
  blocks_[index].grid = std::move(grid);
}
const Grid &Field::grid(std::size_t index) const {
  return blocks_.at(index).grid;
}
std::array<DrawSurface, 2> Field::surfaces() const {
  std::array<DrawSurface, 2> out{};
  for (int set = 0; set < 2; ++set) {
    out[static_cast<std::size_t>(set)].fix =
        static_cast<float>(appearance_.fix[static_cast<std::size_t>(set)]) / 128.f;
    out[static_cast<std::size_t>(set)].colour =
        appearance_.colour[static_cast<std::size_t>(set)];
    out[static_cast<std::size_t>(set)].layer = LAYER;
  }
  for (std::size_t t = 0; t < tiles_.size(); ++t) {
    if (!blocks_[t].live)
      continue;
    const auto &tile = tiles_[t];
    const int set = tile.second_set ? 1 : 0;
    auto &surface = out[static_cast<std::size_t>(set)];
    const float min_x = bits_to_float(tile.bounds[0]);
    const float max_x = bits_to_float(tile.bounds[3]);
    const float min_y = bits_to_float(tile.bounds[1]);
    const auto step = tile_step_bits(min_x, max_x);
    const auto org = origin(min_x, min_y, tile.chunk_z);
    const auto scale = appearance_.scale[static_cast<std::size_t>(set)];
    std::vector<std::array<std::int16_t, SAMPLES>> rows;
    rows.reserve(32);
    for (int record = 1; record <= 32; ++record)
      rows.push_back(blocks_[t].grid.heights[static_cast<std::size_t>(record)]);
    const auto base = static_cast<std::uint32_t>(surface.positions.size());
    for (int call = 1; call <= 31; ++call) {
      for (int column = 0; column <= 30; ++column) {
        const auto sample =
            world_sample(rows, call, column, step, org, scale);
        surface.positions.push_back(
            {promoted(sample.x_bits), promoted(sample.y_bits),
             promoted(sample.z_bits)});
        surface.uv_texels.push_back(
            {static_cast<float>(sample.u) / 16.f,
             static_cast<float>(sample.v) / 16.f});
      }
    }
    for (int call = 2; call <= 31; ++call) {
      for (int column = 0; column <= 29; ++column) {
        const auto at = [&](int row_call, int col) {
          return base + static_cast<std::uint32_t>((row_call - 1) * 31 + col);
        };
        const auto c0 = at(call, column);
        const auto p0 = at(call - 1, column);
        const auto c1 = at(call, column + 1);
        const auto p1 = at(call - 1, column + 1);
        surface.indices.insert(surface.indices.end(), {c0, p0, c1, p0, c1, p1});
      }
    }
  }
  for (auto &surface : out)
    surface.triangles = static_cast<int>(surface.indices.size() / 3u);
  return out;
}
} // namespace champions::presentation::rta::water_draw
