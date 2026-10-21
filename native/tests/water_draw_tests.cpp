#include "champions/level_texture.hpp"
#include "champions/water_draw.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace W = champions::presentation::rta::water_draw;
namespace L = champions::presentation::rta::level_texture;

namespace {
int checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <typename F> void rejects(F fn, const char *why) {
  bool thrown = false;
  try {
    fn();
  } catch (const W::Unsupported &) {
    thrown = true;
  }
  check(thrown, why);
}
std::uint32_t raw_f(double value) {
  const float narrowed = static_cast<float>(value);
  std::uint32_t bits = 0;
  std::memcpy(&bits, &narrowed, 4);
  return bits;
}
W::Grid blank() { return {}; }
std::array<std::uint8_t, 128> ones() { return W::filled_mask(); }
std::array<std::uint8_t, 128> zeros() { return {}; }
} // namespace

int main() {
  try {
    const auto ordinary = W::gs_state(0);
    const auto other = W::gs_state(1);
    check(ordinary.alpha_low == 0x64 && ordinary.alpha_high == 102 &&
              ordinary.colour == 0x006d6b5fu && ordinary.scale == 0x3b999999u,
          "set 0 is RGB 95, 107, 109 at FIX 102");
    check(W::colour_channels(ordinary.colour) == std::array<int, 4>{95, 107, 109, 0},
          "set 0 alpha channel is 0 and is not the blend");
    check(other.alpha_low == 0x64 && other.alpha_high == 98 &&
              other.colour == 0x00808080u && other.scale == 0x3cc00000u,
          "flag 0x1000 selects set 1");
    check(ordinary.test == 0x71001 && ordinary.tex1 == 0x61 &&
              ordinary.prim == 0x15c && ordinary.layer == 3,
          "water tests colour-only, filters bilinear, and is one strip packet");
    check(W::alpha_words(-100) == std::array<std::uint32_t, 2>{0x68u, 100u} &&
              W::alpha_words(0) == std::array<std::uint32_t, 2>{0x64u, 0u},
          "a negative FIX switches to the additive form");
    check(W::draw_layer(0) == 3 && W::draw_layer(0x200u) == 3, "flag 0x200 stays on layer 3");
    check(L::chunk_gs(0).layer == 0 && L::chunk_gs(0x200u).layer == 4 &&
              W::draw_layer(0x80u) > L::chunk_gs(0).layer &&
              W::draw_layer(0x280u) < L::chunk_gs(0x200u).layer,
          "water is after ordinary WORLD chunks and before flag 0x200");

    check(W::floor_half(0.4) == 0.0 && W::floor_half(-1.2) == -1.0 &&
              W::floor_half(120.5) == 121.0 && W::floor_half(10.6) == 11.0,
          "origin and width use floor of half toward -infinity");
    check(W::tile_step_bits(0.0, 120.0) == 0x407fffffu &&
              W::tile_step_bits(0.4, 100.2) == 0x40555554u,
          "the grid step is the x extent times the documented scale");
    const auto org = W::origin(0.4, -1.2, 8);
    check(org[0] == 0.0 && org[1] == -1.0 && org[2] == 2.0,
          "the origin floors x and y and takes chunk z over 4");

    const std::array<std::array<double, 4>, 4> view{{
        {{2, 0, 0, 0}},
        {{0, 3, 0, 0}},
        {{0, 0, 4, 0}},
        {{5, 6, 7, 8}},
    }};
    const auto matrix = W::upload_matrix(view, {0.0, 0.0, 40.0}, W::SET0_SCALE);
    check(matrix[0][2] == 0x3c999999u && matrix[1][1] == raw_f(3) &&
              matrix[2][0] == raw_f(2),
          "height moves on view row 2");
    const auto z_term = matrix[3][2];
    float z_value = 0;
    std::memcpy(&z_value, &z_term, 4);
    check(z_value > 166.999f && z_value < 167.001f && matrix[3][0] == raw_f(5) &&
              matrix[3][1] == raw_f(6) && matrix[3][3] == raw_f(8),
          "the translation row carries the origin");

    auto grid = blank();
    grid.velocity[4][5] = 1000;
    auto stepped = W::step_grid(grid, ones());
    check(stepped.heights[5][5] == 993 && stepped.velocity[4][5] == 993 &&
              stepped.heights[4][5] == 0 &&
              stepped.velocity[31] == grid.velocity[31],
          "a velocity of 1000 is damped to 993 and added one row later");
    grid = blank();
    grid.heights[0][0] = 500;
    grid.heights[10][10] = 100;
    stepped = W::step_grid(grid, zeros());
    check(stepped.heights[0][0] == 0 && stepped.heights[10][10] == 100,
          "row 0 is replaced and a clear mask bit freezes the height");
    bool velocities_clear = true;
    for (const auto &row : stepped.velocity)
      for (const auto sample : row)
        velocities_clear = velocities_clear && sample == 0;
    check(velocities_clear, "a clear mask stores velocity 0");
    W::Grid flat;
    for (auto &row : flat.heights)
      row.fill(100);
    stepped = W::step_grid(flat, ones());
    check(stepped.heights[10][10] == 100 && stepped.velocity[10][10] == 0,
          "flat water does not move");
    std::array<std::uint8_t, 128> mask{};
    mask[0] = 0x00;
    mask[1] = 0x80;
    check(W::mask_allows(mask, 0, 0) && !W::mask_allows(mask, 0, 1),
          "bit 15 is the first row of a bank");
    mask = {};
    mask[64] = 1;
    check(W::mask_allows(mask, 0, 31), "bit 0 of the second bank is row 31");

    W::Grid self;
    W::Grid neighbour;
    for (int row = 0; row < 33; ++row)
      for (int col = 0; col < 32; ++col) {
        self.heights[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] =
            static_cast<std::int16_t>(row * 40 + col);
        neighbour.heights[static_cast<std::size_t>(row)]
                         [static_cast<std::size_t>(col)] =
            static_cast<std::int16_t>(1000 + row * 40 + col);
      }
    for (auto &row : self.velocity)
      row.fill(7);
    for (int row = 0; row < 32; ++row)
      for (int col = 0; col < 32; ++col)
        neighbour.velocity[static_cast<std::size_t>(row)]
                          [static_cast<std::size_t>(col)] =
            static_cast<std::int16_t>(3000 + row * 40 + col);
    auto columns = W::seam_columns(self, neighbour);
    check(columns.self.heights[4][0] == neighbour.heights[4][30] &&
              columns.other.heights[4][31] == self.heights[4][1] &&
              columns.self.velocity[4][0] == neighbour.velocity[4][30] &&
              columns.other.velocity[4][31] == self.velocity[4][1],
          "slot 0 copies the column seam both ways");
    auto rows = W::seam_rows(self, neighbour);
    check(rows.self.heights[1] == neighbour.heights[31] &&
              rows.other.heights[32] == self.heights[2] &&
              rows.self.velocity[0] == neighbour.velocity[30] &&
              rows.other.velocity[31] == self.velocity[1],
          "slot 2 copies the row seam both ways");
    check(!W::block_expired(199, 0) && W::block_expired(200, 0) &&
              !W::block_expired(10, 11),
          "a block lasts 199 frames and drops at 200");

    const std::array<std::array<std::uint32_t, 4>, 4> ident{{
        {{raw_f(0), raw_f(1), raw_f(0), raw_f(0)}},
        {{raw_f(0), raw_f(0), raw_f(1), raw_f(0)}},
        {{raw_f(1), raw_f(0), raw_f(0), raw_f(0)}},
        {{raw_f(0), raw_f(0), raw_f(0), raw_f(1)}},
    }};
    std::vector<std::array<std::int16_t, 32>> uploaded(3);
    uploaded[2][2] = 160;
    const auto strips = W::project_strips(uploaded, ident, raw_f(4));
    check(strips.size() == 1 && strips[0].size() == 62, "one kicked strip has 62 vertices");
    check(strips[0][0].x == 32896 && strips[0][0].y == 32768 &&
              strips[0][0].u == 512 && strips[0][0].v == 512 && strips[0][0].adc == 0,
          "a flat sample packs to 512 and the biased screen origin");
    check(strips[0][2].u == 504, "a sample 160 above its neighbour is UV 504");
    check(W::slope_uv(0, 0) == 512 && W::slope_uv(0, 160) == 504,
          "the slope helper matches the kicked UV");
    const auto placed =
        W::world_sample(uploaded, 2, 1, raw_f(4), {0.0, 0.0, 2.0}, W::SET0_SCALE);
    check(placed.u == 504 && placed.v == 504, "the world sample uses the same slope UV");
    float px = 0, py = 0;
    std::memcpy(&px, &placed.x_bits, 4);
    std::memcpy(&py, &placed.y_bits, 4);
    check(px == 8.f && py == 4.f, "row index steps in x and column index in y");

    const std::array<std::array<std::uint32_t, 4>, 4> clipped{{
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
        {{0, raw_f(100), 0, 0}},
        {{0, raw_f(-2000), 0, raw_f(1)}},
    }};
    std::vector<std::array<std::int16_t, 32>> four(4);
    const auto sticky = W::project_strips(four, clipped, raw_f(4), 1);
    check(sticky[1][0].adc == 0 && sticky[1][1].adc == 1 && sticky[1][2].adc == 1 &&
              sticky[1][3].adc == 1,
          "clip sticks for the rest of the strip and the next strip starts clear");
    const std::array<std::array<std::uint32_t, 4>, 4> edge{{
        {{0, 0, 0, 0}},
        {{0, raw_f(250), 0, 0}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, raw_f(1)}},
    }};
    const auto kept = W::project_strips(uploaded, edge, raw_f(4), 1);
    check(kept[0][2].adc == 0, "a vertex at exactly 1000 is kept");

    const auto splash = W::splash_seed(0);
    check(splash.seed == W::LCG_ADD &&
              splash.fire == ((splash.seed >> 16) % 300u == 0),
          "the drawer generator is the documented LCG");
    check(W::WAVE_STRENGTH == 41.f && W::WAVE_RADIUS == 10.f &&
              W::SPLASH_STRENGTH == 1500.f && W::SPLASH_RADIUS == 6.f &&
              W::SETUP_INTEGER == 4,
          "the stored splash constants stay with the setup");
    const std::array<std::uint32_t, 6> splash_bounds{
        raw_f(0.4), raw_f(-1.2), raw_f(3.5), raw_f(120.6), raw_f(10.0), raw_f(40.0)};
    check(W::splash_seed(324).fire, "seed 324 is a splash hit");
    const auto point = W::splash_point(324, splash_bounds);
    check(point.hit && point.seed == 0xaa3e5b7du &&
              point.point == std::array<std::uint32_t, 3>{0x42a10bbdu, 0x40c88098u, 0x40600000u},
          "the splash point shares one unit interval and keeps minimum z");
    check(W::second_particle_z(0x42f70000u, point.seed) == 0x4348b360u,
          "the second particle lifts off the terrain height");
    const auto missed = W::splash_point(325, splash_bounds);
    check(!missed.hit && missed.seed == W::splash_seed(325).seed,
          "seed 325 is one generator step and no disturb");
    rejects([] { W::refuse_countdown_splash(); }, "the countdown splash stays refused");
    rejects([] { W::refuse_setup_integer(); }, "the setup timer stays unread");
    rejects([] { W::refuse_diagonal_slots(); }, "the per-frame exchange still skips slots 1 and 3");
    rejects([] { W::refuse_property_order(); }, "the factory order versus setup stays unexecuted");
    rejects([] { W::refuse_player_move_disturb(); }, "Player_move disturb immediates stay unpublished");
    rejects([] { W::refuse_splash_particles(); }, "zero particle definitions stay refused");
    rejects([] { W::refuse_splash_start_seed(); }, "the retail splash seed start stays refused");

    check(W::colour_word(10, 20, 30) == 0x001e140au, "waterColor packs three bytes");
    check(W::wavyness_scale(0x3f800000u) == W::SET1_SCALE, "wavyness 1 stores the set-1 scale");
    check(W::wavyness_scale(0x3f000000u) == 0x3c400000u, "wavyness 0.5 is half of that scale");
    const auto loaded = W::load_appearance();
    check(loaded.fix[0] == 102 && loaded.colour[0] == W::SET0_COLOUR &&
              loaded.scale[0] == W::SET0_SCALE && loaded.strength == raw_f(41.0) &&
              loaded.radius == raw_f(10.0) && loaded.timer == 4 && loaded.fix[1] == 0 &&
              loaded.colour[1] == 0 && loaded.scale[1] == 0,
          "reset zeros set 1, then setup replaces set 0 only");
    const auto parsed = W::replacing(
        {"waterColor2=1,2,3,4", "waterColor=10,20,30,40", "waterWavyness2=0.5",
         "waterWavyness=1", "waterBumpAmount=12.5", "waterBumpRadius=3", "waterBumpTimer=7",
         "runSpeed=2"});
    check(parsed.applied == 7 && parsed.refused == 0, "seven water lines apply, others are not water");
    const auto replaced = parsed.appearance;
    check(replaced.colour[1] == W::colour_word(1, 2, 3) && replaced.fix[1] == 4 &&
              replaced.colour[0] == 0x001e140au && replaced.fix[0] == 40 &&
              replaced.scale[1] == 0x3c400000u && replaced.scale[0] == W::SET1_SCALE &&
              replaced.strength == raw_f(12.5) && replaced.radius == raw_f(3.0) &&
              replaced.timer == 7,
          "level lines replace colour, fix, wavyness, bump and the timer word");
    const auto earlier = W::before_setup({"waterColor2=1,2,3,4", "waterColor=10,20,30,40",
                                           "waterBumpTimer=7"}).appearance;
    check(earlier.colour[1] == W::colour_word(1, 2, 3) && earlier.fix[1] == 4 &&
              earlier.colour[0] == W::SET0_COLOUR && earlier.fix[0] == 102 &&
              earlier.timer == 4,
          "a set-1 line before setup survives and a set-0 line is replaced");

    {
      const auto refused = W::replacing({"waterColor=1,2,3", "waterColor=1, 2,3,4",
                                         "waterBumpTimer=7.5", "waterWavyness=1e2",
                                         "waterWavyness=", "waterBumpRadius=+3",
                                         "waterColor=1,2,3,256", "waterColor2=1,2,3,-300",
                                         "waterBumpTimer=7 "});
      check(refused.applied == 0 && refused.refused == 9 &&
                refused.appearance.colour == W::load_appearance().colour &&
                refused.appearance.scale == W::load_appearance().scale &&
                refused.appearance.timer == 4,
            "a water line outside the executed shapes is refused and stores nothing");
      const auto shapes = W::replacing({"waterWavyness=.5\r", "waterColor2=255,0,9,-100\r",
                                        "waterBumpTimer=7\r"});
      check(shapes.applied == 3 && shapes.appearance.timer == 7 && shapes.appearance.scale[0] == 0x3c400000u &&
                shapes.appearance.fix[1] == -100 &&
                shapes.appearance.colour[1] == W::colour_word(255, 0, 9),
            "a leading point decimal, a negative fix and a closing carriage return apply");
      check(W::colour_word(-1, -2, 3) == 0x0003feffu,
            "colour_word keeps the low byte of each part, as the reference does");
      check(W::alpha_words(-100) == std::array<std::uint32_t, 2>{W::ALPHA_ADD, 100u},
            "a negative level fix selects the additive blend");
    }
    using Heights = std::array<std::array<std::int16_t, 32>, 33>;
    Heights claim_edge{};
    claim_edge[1][31] = 1001;
    std::array<const Heights *, 4> one{{&claim_edge, nullptr, nullptr, nullptr}};
    const auto claimed = W::claim_grid(one);
    check(claimed.heights[0] == std::array<std::int16_t, 32>{} &&
              claimed.heights[1] ==
                  std::array<std::int16_t, 32>{1000, 900, 800, 700, 600, 500, 400, 300, 200, 100} &&
              claimed.heights[2] == std::array<std::int16_t, 32>{},
          "one edge is truncated against one");
    bool disturb_clear = true;
    for (const auto &row : claimed.velocity)
      for (const auto sample : row)
        disturb_clear = disturb_clear && sample == 0;
    check(disturb_clear, "a claim clears every disturbance sample");
    claim_edge[1][31] = -30000;
    const auto negative = W::claim_grid(one);
    check(negative.heights[1][0] == -29999 && negative.heights[1][9] == -2999,
          "a negative claim sample stays signed");
    Heights left{};
    Heights far{};
    left[1][31] = 30000;
    left[10][31] = 30000;
    far[32][9] = 30000;
    std::array<const Heights *, 4> both{{&left, nullptr, &far, nullptr}};
    std::array<const Heights *, 4> left_only{{&left, nullptr, nullptr, nullptr}};
    check(W::claim_grid(left_only).heights[1][0] == 29999 &&
              W::claim_grid(both).heights[1][0] == 14999 &&
              W::claim_grid(both).heights[10][9] == 5999,
          "two edges normalise only once the weight sum reaches one");

    std::array<std::array<std::int16_t, 32>, 32> cells{};
    W::disturb_cells(cells, raw_f(1500.0), raw_f(6.0), {0, 0}, {raw_f(-60.0), raw_f(60.0)},
                     {raw_f(0.0), raw_f(0.0)});
    check(cells[15][15] == 328 && cells[15][16] == 914 && cells[15][17] == 328 &&
              cells[16][15] == 914 && cells[16][16] == 1500 && cells[16][17] == 914 &&
              cells[17][15] == 328 && cells[17][16] == 914 && cells[17][17] == 328,
          "a centre splash is the documented 3 by 3");
    int splash_sum = 0;
    for (const auto &row : cells)
      for (const auto sample : row)
        splash_sum += sample;
    check(splash_sum == 328 * 4 + 914 * 4 + 1500, "the centre splash writes only that 3 by 3");
    std::array<std::array<std::int16_t, 32>, 32> shifted{};
    W::disturb_cells(shifted, raw_f(1500.0), raw_f(6.0), {0, 0}, {raw_f(-60.0), raw_f(60.0)},
                     {raw_f(30.0), raw_f(0.0)});
    int row_lo = 32, row_hi = -1, col_lo = 32, col_hi = -1;
    for (int r = 0; r < 32; ++r)
      for (int c = 0; c < 32; ++c)
        if (shifted[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]) {
          row_lo = std::min(row_lo, r);
          row_hi = std::max(row_hi, r);
          col_lo = std::min(col_lo, c);
          col_hi = std::max(col_hi, c);
        }
    check(row_lo == 23 && row_hi == 25 && col_lo == 15 && col_hi == 17,
          "moving the point in world x moves the rows");
    std::array<std::array<std::int16_t, 32>, 32> clamped{};
    for (auto &row : clamped)
      row.fill(-100);
    clamped[16][16] = 32000;
    W::disturb_cells(clamped, raw_f(1500.0), raw_f(6.0), {0, 0}, {raw_f(-60.0), raw_f(60.0)},
                     {raw_f(0.0), raw_f(0.0)});
    check(clamped[16][16] == 32767 && clamped[16][15] > -100 && clamped[0][0] == -100,
          "only a sum above 32767 is clamped");
    std::array<std::array<std::int16_t, 32>, 32> neg_strength{};
    W::disturb_cells(neg_strength, raw_f(-100.0), raw_f(6.0), {0, 0},
                     {raw_f(-60.0), raw_f(60.0)}, {raw_f(0.0), raw_f(0.0)});
    check(neg_strength[16][16] == -100, "a negative strength keeps the low 16 bits");

    W::TileSource a;
    a.chunk = 0;
    a.neighbours = {1, -1, -1, -1};
    a.mask = ones();
    a.bounds = {raw_f(0), raw_f(0), raw_f(0), raw_f(120), raw_f(120), raw_f(1)};
    W::TileSource b = a;
    b.chunk = 1;
    b.neighbours = {-1, -1, -1, -1};
    b.second_set = true;
    b.flag_0x200 = true;
    W::Field field({a, b});
    field.on_tick(0, false);
    check(field.live_blocks() == 2 && !field.claim_seed_refused() && field.dirty(),
          "drawing claims a block from the neighbour weights");
    W::Grid spike;
    spike.velocity[4][5] = 1000;
    field.supply_grid(0, spike);
    field.on_tick(1, false);
    check(field.grid(0).heights[5][5] == 993, "the field steps a supplied velocity");
    field.on_tick(1, true);
    check(field.grid(0).heights[5][5] == 993, "a frozen tick does not step");
    field.on_tick(199, false);
    check(field.live_blocks() == 2, "frame 199 keeps the block");
    field.on_tick(200, false);
    check(field.live_blocks() == 2 && field.grid(0).heights[5][5] == 0,
          "frame 200 drops the block and the new claim is zero again");
    const auto drawn = field.surfaces();
    check(drawn[0].layer == 3 && drawn[1].layer == 3 && drawn[0].triangles == 1800 &&
              drawn[1].triangles == 1800 && drawn[0].fix == 102.f / 128.f &&
              drawn[1].fix == 0.f && drawn[1].colour == 0 &&
              drawn[0].positions.size() == 31u * 31u,
          "a loaded field uses setup set 0 and the zeroed set 1");
    check(drawn[0].uv_texels[0][0] == 32.f && drawn[0].uv_texels[0][1] == 32.f,
          "a flat tile samples 32 texels");
    field.set_appearance(replaced);
    rejects([&] {
      W::Field unseeded({a});
      unseeded.set_level_flags(W::LEVEL_OPTION_SPLASH);
    }, "the drawer splash needs a supplied seed");
    const auto painted = field.surfaces();
    check(painted[1].fix == 4.f / 128.f && painted[1].colour == W::colour_word(1, 2, 3),
          "a level colour replaces the drawn second set");
    auto solo = a;
    solo.neighbours = {-1, -1, -1, -1};
    solo.bounds = {raw_f(-60), raw_f(-60), raw_f(0), raw_f(60), raw_f(60), raw_f(1)};
    W::Field ripples({solo});
    ripples.on_tick(0, false);
    rejects([&] { ripples.disturb(raw_f(1500.0), raw_f(6.0), {0, 0, 0}); },
            "water_disturb needs the WORLD chunk query");
    std::vector<std::uint32_t> asked;
    ripples.set_chunk_query([&](std::uint32_t radius, const std::array<std::uint32_t, 3> &) {
      asked.push_back(radius);
      return std::vector<std::uint32_t>{solo.chunk};
    });
    check(ripples.disturb(raw_f(1500.0), raw_f(6.0), {raw_f(0), raw_f(0), raw_f(0)}) == 1 &&
              ripples.grid(0).velocity[16][16] == 1500 && ripples.grid(0).velocity[16][15] == 914 &&
              ripples.grid(0).velocity[15][15] == 328 &&
              ripples.grid(0).heights[17][16] == 0,
          "a disturb writes the velocity grid and leaves the drawn heights");
    const auto carried = W::step_grid(ripples.grid(0), solo.mask);
    ripples.on_tick(1, false);
    check(ripples.grid(0).heights == carried.heights && carried.heights[17][16] != 0,
          "the next step carries that splash into the drawn heights");
    const auto seed = 324u;
    ripples.set_splash_seed(seed);
    ripples.set_level_flags(W::LEVEL_OPTION_SPLASH);
    const auto expected = W::splash_point(seed, solo.bounds);
    ripples.on_tick(2, false);
    check(expected.hit && ripples.splash_seed() == expected.seed &&
              asked.back() == raw_f(6.0),
          "the drawer splash rolls once per live tile when the option is set");
    ripples.set_splash_seed(seed);
    ripples.on_tick(3, true);
    check(ripples.splash_seed() == seed, "a frozen frame does not roll the splash generator");
    W::Field quiet({solo});
    quiet.set_splash_seed(seed);
    quiet.set_chunk_query([](std::uint32_t, const std::array<std::uint32_t, 3> &) {
      return std::vector<std::uint32_t>{};
    });
    quiet.on_tick(0, false);
    check(quiet.splash_seed() == seed, "the splash option has to be set before the generator rolls");

    std::cout << "PASS water draw: " << checks << " checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL water draw after " << checks << " checks: " << error.what()
              << '\n';
    return 1;
  }
}
