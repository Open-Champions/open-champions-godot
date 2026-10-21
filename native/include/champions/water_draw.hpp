#pragma once
// Water tiles for Return to Arms. The executable form of the contract is
// tools/ref_waterdraw.py and docs/formats/water-draw.md at research
// t3/research-textures a9486c7. The height step and the drawer do not call
// math_cos16 or math_sin16.
//
// The claim-frame weights, the drawer splash point, the negative countdown
// splash, the unread integer 4, and neighbour slots 1 and 3 stay refused.
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace champions::presentation::rta::water_draw {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Unsupported : Error {
  using Error::Error;
};

inline constexpr int LAYER = 3;
inline constexpr std::uint32_t SET0_FIX = 102;
inline constexpr std::uint32_t SET0_COLOUR = 0x006d6b5fu; // R 95 G 107 B 109 A 0
inline constexpr std::uint32_t SET0_SCALE = 0x3b999999u;  // 0.0046875
inline constexpr std::uint32_t SET1_FIX = 98;
inline constexpr std::uint32_t SET1_COLOUR = 0x00808080u; // R 128 G 128 B 128 A 0
inline constexpr std::uint32_t SET1_SCALE = 0x3cc00000u;  // 0.0234375
inline constexpr std::uint32_t STEP_SCALE = 0x3d088888u;
inline constexpr std::uint32_t UV_SCALE = 0x3d4c78eau;
inline constexpr std::uint32_t ALPHA_BLEND = 0x64u;
inline constexpr std::uint32_t ALPHA_ADD = 0x68u;
inline constexpr std::uint32_t TEST = 0x00071001u;
inline constexpr std::uint32_t TEX1 = 0x61u;
inline constexpr std::uint32_t PRIM = 0x15cu;
inline constexpr int ROWS = 33;
inline constexpr int SAMPLES = 32;
inline constexpr int STRIPS = 30;
inline constexpr int VERTS_PER_STRIP = 62;
inline constexpr int KEEP_FRAMES = 200;
// Stored by water_setup. The countdown splash that would read them was not
// executed. Nothing documented reads SETUP_INTEGER.
inline constexpr float WAVE_STRENGTH = 41.f;
inline constexpr float WAVE_RADIUS = 10.f;
inline constexpr int SETUP_INTEGER = 4;
inline constexpr float SPLASH_STRENGTH = 1500.f;
inline constexpr float SPLASH_RADIUS = 6.f;
inline constexpr int SPLASH_MODULUS = 300;
inline constexpr std::uint32_t LCG_MUL = 0x0019660du;
inline constexpr std::uint32_t LCG_ADD = 0x3c6ef35fu;

struct GsState {
  int fix = 0;
  std::uint32_t alpha_low = 0;
  std::uint32_t alpha_high = 0;
  std::uint32_t colour = 0;
  std::uint32_t scale = 0;
  std::uint32_t test = TEST;
  std::uint32_t tex1 = TEX1;
  std::uint32_t prim = PRIM;
  int layer = LAYER;
};

struct Vertex {
  std::uint32_t x = 0, y = 0, u = 0, v = 0;
  int adc = 0;
};

struct Grid {
  std::array<std::array<std::int16_t, SAMPLES>, ROWS> heights{};
  std::array<std::array<std::int16_t, SAMPLES>, SAMPLES> velocity{};
  std::array<std::int16_t, SAMPLES> gap{};
};

struct Seam {
  Grid self;
  Grid other;
};

struct Splash {
  std::uint32_t seed = 0;
  bool fire = false;
};

// World-space sample before the view. u and v are the 12.4 slope texels.
struct Sample {
  std::uint32_t x_bits = 0, y_bits = 0, z_bits = 0;
  std::uint32_t u = 0, v = 0;
};

GsState gs_state(int constant_set);
std::array<int, 4> colour_channels(std::uint32_t word);
std::array<std::uint32_t, 2> alpha_words(int fix);
// Water stays on layer 3 when the chunk also has flag 0x200.
int draw_layer(std::uint32_t chunk_flags);

double floor_half(double value);
std::uint32_t tile_step_bits(double min_x, double max_x);
std::array<double, 3> origin(double min_x, double min_y, std::int32_t chunk_z);
std::array<std::array<std::uint32_t, 4>, 4>
upload_matrix(const std::array<std::array<double, 4>, 4> &view,
              const std::array<double, 3> &origin_xyz, std::uint32_t scale_bits);

// rows is the uploaded sequence (record 1, record 2, ...). Calls 0 and 1 are
// not kicked. clip nonzero enables the sticky screen test.
std::vector<std::vector<Vertex>>
project_strips(const std::vector<std::array<std::int16_t, SAMPLES>> &rows,
               const std::array<std::array<std::uint32_t, 4>, 4> &matrix,
               std::uint32_t step, int clip = 0);

// Slope lookup. A flat pair is 512. from is the neighbour, to is this sample.
std::uint32_t slope_uv(std::int16_t from, std::int16_t to);

// Uploaded row k (record k+1) and column position i. Height is rows[k][i+1].
Sample world_sample(const std::vector<std::array<std::int16_t, SAMPLES>> &rows,
                    int call, int column, std::uint32_t step,
                    const std::array<double, 3> &origin_xyz,
                    std::uint32_t scale_bits);

bool mask_allows(const std::array<std::uint8_t, 128> &mask, int column, int row);
std::array<std::uint8_t, 128> filled_mask();
Grid step_grid(const Grid &grid, const std::array<std::uint8_t, 128> &mask);
Seam seam_columns(const Grid &self, const Grid &other);
Seam seam_rows(const Grid &self, const Grid &other);
bool block_expired(std::uint32_t frame, std::uint32_t stamp);
Splash splash_seed(std::uint32_t seed);

[[noreturn]] void refuse_claim_weights();
[[noreturn]] void refuse_splash_point();
[[noreturn]] void refuse_countdown_splash();
[[noreturn]] void refuse_setup_integer();
[[noreturn]] void refuse_diagonal_slots();

// One level's drawn tiles. Grids start at zero: the claim-frame weights are
// refused, which is not a measurement that the original seed is zero.
struct TileSource {
  std::uint32_t chunk = 0;
  std::array<std::int16_t, 4> neighbours{}; // chunk indices, -1 for none
  std::array<std::uint8_t, 128> mask{};
  bool second_set = false;
  std::uint32_t min_x = 0, max_x = 0, min_y = 0; // raw float bits of the bounds
  std::int32_t chunk_z = 0;
  bool flag_0x200 = false;
};

struct DrawSurface {
  std::vector<std::array<double, 3>> positions; // native Z-up world units
  std::vector<std::array<float, 2>> uv_texels;
  std::vector<std::uint32_t> indices;
  float fix = 0;          // FIX/128, the blend weight
  std::uint32_t colour = 0;
  int layer = LAYER;
  int triangles = 0;
};

class Field {
public:
  explicit Field(std::vector<TileSource> tiles);
  // Simulation first, then the drawer's claim. Frozen skips the step and the
  // lifetime drop. A new block is zero because the claim weights are refused.
  void on_tick(std::uint32_t frame, bool frozen);
  bool dirty() const noexcept { return dirty_; }
  void clear_dirty() noexcept { dirty_ = false; }
  std::size_t tile_count() const noexcept { return tiles_.size(); }
  std::size_t live_blocks() const;
  bool claim_seed_refused() const noexcept { return true; }
  // Supplied samples for a live block. Not the claim-frame seed.
  void supply_grid(std::size_t index, Grid grid);
  const Grid &grid(std::size_t index) const;
  std::array<DrawSurface, 2> surfaces() const;

private:
  struct Block {
    bool live = false;
    std::uint32_t stamp = 0;
    Grid grid{};
  };
  std::vector<TileSource> tiles_;
  std::vector<Block> blocks_;
  std::vector<int> by_chunk_;
  bool dirty_ = true;
};
} // namespace champions::presentation::rta::water_draw
