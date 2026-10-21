#include "champions/water_draw.hpp"
#include "champions/core_arithmetic.hpp"
#include <algorithm>
#include <cmath>
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

Raw cpu_add(Raw a, Raw b) {
  return Fpu(Fpu::Model::Ps2).add(a, b);
}
Raw cpu_mul(Raw a, Raw b) {
  return Fpu(Fpu::Model::Ps2).mul(a, b);
}
Raw cpu_from_int(std::uint32_t value) {
  return Fpu(Fpu::Model::Ps2).from_int(value);
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

void refuse_claim_weights() {
  throw Unsupported(
      "water_init weights on the frame a block is claimed were not executed");
}
void refuse_splash_point() {
  throw Unsupported(
      "the drawer splash point inside the chunk bounds was not reduced");
}
void refuse_countdown_splash() {
  throw Unsupported(
      "the simulation splash taken when the countdown is negative was not executed");
}
void refuse_setup_integer() {
  throw Unsupported("nothing documented reads the integer 4 stored by water_setup");
}
void refuse_diagonal_slots() {
  throw Unsupported(
      "neighbour slots 1 and 3 are not read by the per-frame exchange");
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
  for (auto &block : blocks_) {
    if (block.live)
      continue;
    // A refused claim seed is a zero grid. The first mesh uses the
    // constructor's dirty flag; reclaiming zeros does not rebuild it.
    block.live = true;
    block.stamp = frame;
    block.grid = {};
  }
}
std::size_t Field::live_blocks() const {
  std::size_t n = 0;
  for (const auto &block : blocks_)
    n += block.live ? 1u : 0u;
  return n;
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
    const auto state = gs_state(set);
    out[static_cast<std::size_t>(set)].fix =
        static_cast<float>(state.fix) / 128.f;
    out[static_cast<std::size_t>(set)].colour = state.colour;
    out[static_cast<std::size_t>(set)].layer = LAYER;
  }
  for (std::size_t t = 0; t < tiles_.size(); ++t) {
    if (!blocks_[t].live)
      continue;
    const auto &tile = tiles_[t];
    const int set = tile.second_set ? 1 : 0;
    auto &surface = out[static_cast<std::size_t>(set)];
    const auto state = gs_state(set);
    const float min_x = bits_to_float(tile.min_x);
    const float max_x = bits_to_float(tile.max_x);
    const float min_y = bits_to_float(tile.min_y);
    const auto step = tile_step_bits(min_x, max_x);
    const auto org = origin(min_x, min_y, tile.chunk_z);
    std::vector<std::array<std::int16_t, SAMPLES>> rows;
    rows.reserve(32);
    for (int record = 1; record <= 32; ++record)
      rows.push_back(blocks_[t].grid.heights[static_cast<std::size_t>(record)]);
    const auto base = static_cast<std::uint32_t>(surface.positions.size());
    for (int call = 1; call <= 31; ++call) {
      for (int column = 0; column <= 30; ++column) {
        const auto sample =
            world_sample(rows, call, column, step, org, state.scale);
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
