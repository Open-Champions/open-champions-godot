#pragma once
// Water tiles for Return to Arms. The executable form of the contract is
// tools/ref_waterdraw.py and docs/formats/water-draw.md at research
// t3/research-textures 1528708. The height step and the drawer do not call
// math_cos16 or math_sin16.
//
// Still refused: the negative-countdown splash, any reader of the integer at
// the setup timer word, neighbour slots 1 and 3 in the per-frame exchange,
// Player_move's unpublished disturb immediates, the retail start value of the
// drawer splash seed, and splash particles whose definitions are zero in the
// executable.
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
// water_init. One ulp under 0.1. Ten of them stay strictly under 1.
inline constexpr std::uint32_t EDGE_WEIGHT = 0x3dccccccu;
inline constexpr std::uint32_t CLAIM_ONE = 0x3f800000u;
// water_disturb. Width times this, plus one half, truncates to the cell count.
inline constexpr std::uint32_t DISTURB_WIDTH = 0x3daaaaaau;
inline constexpr std::uint32_t SPLASH_UNIT = 0x37800000u; // 1/65536
inline constexpr std::uint32_t PARTICLE_RISE = 0x42a00000u; // 80
inline constexpr std::uint32_t PARTICLE_LIFT = 0x41c00000u; // 24
inline constexpr std::uint32_t PARTICLE_FIRST = 0x7d5d00u;
inline constexpr std::uint32_t PARTICLE_SECOND = 0x7d5cb0u;
// Level flag word 0x44fe08 (level table row +0x20). The difftest's "bit 8"
// is the value 0x8, which fills masks. 0x10 arms the drawer splash.
inline constexpr std::uint32_t LEVEL_OPTION_FILL = 0x8u;
inline constexpr std::uint32_t LEVEL_OPTION_SPLASH = 0x10u;

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

struct SplashPoint {
  std::uint32_t seed = 0;
  bool hit = false;
  std::array<std::uint32_t, 3> point{}; // raw bits; z is the minimum
};

// Post-load words. Reset zeros set 1, then setup replaces set 0 only.
// A level line can replace a word. Whether the object factory runs before or
// after setup was not executed: replacing() applies lines after setup, and
// before_setup() applies them between reset and setup.
struct Appearance {
  std::array<int, 2> fix{{0, 0}};
  std::array<std::uint32_t, 2> colour{{0, 0}};
  std::array<std::uint32_t, 2> scale{{0, 0}};
  std::uint32_t strength = 0;
  std::uint32_t radius = 0;
  int timer = 0;
};

struct Claim {
  std::array<std::array<std::int16_t, SAMPLES>, ROWS> heights{};
  std::array<std::array<std::int16_t, SAMPLES>, SAMPLES> velocity{};
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
// bounds is minimum x, y, z then maximum x, y, z, as raw float bits.
SplashPoint splash_point(std::uint32_t seed,
                         const std::array<std::uint32_t, 6> &bounds);
std::uint32_t second_particle_z(std::uint32_t terrain, std::uint32_t seed);

std::uint32_t colour_word(int red, int green, int blue);
std::uint32_t wavyness_scale(std::uint32_t parsed);
Appearance reset_appearance();
void setup_appearance(Appearance &appearance);
Appearance load_appearance();
// What one customVars line did. The float lines use the IEEE value of the
// decimal, as the difftest deposited it: the original sscanf did not return in
// the interpreter. A water key whose value is not the executed shape (four
// comma integers, one integer, one decimal) is refused and stores nothing.
enum class Property { Other, Applied, Refused };
Property apply_property(Appearance &appearance, std::string_view line);
struct Parsed {
  Appearance appearance;
  int applied = 0;
  int refused = 0;
};
// Lines applied after reset and setup (section 7 step 2 of the research note).
Parsed replacing(const std::vector<std::string> &lines);
// Lines applied between the reset and setup. Tested, not used by the scene.
Parsed before_setup(const std::vector<std::string> &lines);

// neighbours is payload order. A null slot is an absent or unlinked neighbour.
// Height row 0 is left zero. Every disturbance sample is cleared.
Claim claim_grid(const std::array<const std::array<std::array<std::int16_t, SAMPLES>, ROWS> *, 4> &neighbours);

// Writes the velocity grid. strength, radius, span (min x, max x) and pos
// (x, y) are raw float bits. origin is chunk +0x28 and +0x2c. z is not read.
void disturb_cells(std::array<std::array<std::int16_t, SAMPLES>, SAMPLES> &grid,
                   std::uint32_t strength, std::uint32_t radius,
                   const std::array<std::int32_t, 2> &origin,
                   const std::array<std::uint32_t, 2> &span,
                   const std::array<std::uint32_t, 2> &pos);

[[noreturn]] void refuse_countdown_splash();
[[noreturn]] void refuse_setup_integer();
[[noreturn]] void refuse_diagonal_slots();
[[noreturn]] void refuse_property_order();
[[noreturn]] void refuse_player_move_disturb();
[[noreturn]] void refuse_splash_particles();
[[noreturn]] void refuse_splash_start_seed();

// The water chunks water_disturb's query (0x189240) admits for this radius and
// point, in query order. The caller owns the WORLD tree. An empty list is a
// miss or the cut byte.
using ChunkQuery = std::function<std::vector<std::uint32_t>(
    std::uint32_t radius, const std::array<std::uint32_t, 3> &position)>;

// One level's drawn tiles. A fresh block is claimed from its live neighbours.
// Every tile counts as drawn every tick, in tile order; the view cull and the
// world draw order are not modelled.
struct TileSource {
  std::uint32_t chunk = 0;
  std::array<std::int16_t, 4> neighbours{}; // chunk indices, -1 for none
  std::array<std::uint8_t, 128> mask{};
  bool second_set = false;
  std::array<std::uint32_t, 6> bounds{}; // min xyz, max xyz, raw float bits
  std::array<std::int32_t, 2> origin{};  // chunk +0x28, +0x2c
  std::int32_t chunk_z = 0;
  bool flag_0x200 = false;
};

struct DrawSurface {
  std::vector<std::array<double, 3>> positions; // native Z-up world units
  std::vector<std::array<float, 2>> uv_texels;
  std::vector<std::uint32_t> indices;
  float fix = 0; // FIX/128. Negative selects the additive form.
  std::uint32_t colour = 0;
  int layer = LAYER;
  int triangles = 0;
};

class Field {
public:
  explicit Field(std::vector<TileSource> tiles);
  // Step, seams, then claim, then the drawer splash. A splash written here
  // stays in the velocity grid until the next tick's step. Frozen skips the
  // step, the drop, and the splash, and does not advance the splash seed.
  void on_tick(std::uint32_t frame, bool frozen);
  bool dirty() const noexcept { return dirty_; }
  void clear_dirty() noexcept { dirty_ = false; }
  std::size_t tile_count() const noexcept { return tiles_.size(); }
  std::size_t live_blocks() const;
  bool claim_seed_refused() const noexcept { return false; }
  // The retail start value of the word at 0x404460 is not in the pin. A
  // supplied seed is a test input.
  void set_splash_seed(std::uint32_t seed) noexcept { splash_seed_ = seed; }
  std::optional<std::uint32_t> splash_seed() const noexcept { return splash_seed_; }
  // Throws Unsupported when 0x10 is set and no seed was supplied.
  void set_level_flags(std::uint32_t flags);
  std::uint32_t level_flags() const noexcept { return level_flags_; }
  void set_appearance(Appearance appearance);
  const Appearance &appearance() const noexcept { return appearance_; }
  // Supplied samples for a live block.
  void supply_grid(std::size_t index, Grid grid);
  const Grid &grid(std::size_t index) const;
  void set_chunk_query(ChunkQuery query) { query_ = std::move(query); }
  // water_disturb into the live blocks of the queried chunks. Returns how
  // many chunks the query admitted, a block that is not live included.
  // Throws Unsupported without a chunk query.
  std::uint32_t disturb(std::uint32_t strength, std::uint32_t radius,
                        const std::array<std::uint32_t, 3> &position);
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
  Appearance appearance_ = load_appearance();
  std::uint32_t level_flags_ = 0;
  std::optional<std::uint32_t> splash_seed_;
  ChunkQuery query_;
  bool dirty_ = true;
};
} // namespace champions::presentation::rta::water_draw
