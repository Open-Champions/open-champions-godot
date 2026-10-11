#pragma once
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace champions::simulation::rta::world_queries {
using Vec3 = std::array<Raw, 3>;
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : Error {
  using Error::Error;
};
inline constexpr Raw NO_FLOOR = 0xc9742400u;
inline constexpr Raw NO_STAND = 0xc7c35000u;
struct Limits {
  std::size_t heightfields = 65536, elements = 65536, grid_cells = 1048576;
  std::size_t samples = 4194304, references = 4194304, path_cells = 1048576;
  std::size_t query_work = 16777216;
};
struct HeightField {
  std::int64_t x0 = 0, y0 = 0, w = 0, h = 0, lo = 0, hi = 0;
  std::vector<std::int16_t> samples{};
  // Provenance label only, never a host pointer or an alias identity.
  std::int64_t offset = 0;
};
struct Element {
  std::int64_t index = 0, min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  std::int64_t hf = 0, flags = 0, x = 0, y = 0, z = 0, cos = 32767, sin = 0;
  std::uint32_t stamp = 0;
  Raw f_min_x = 0, f_min_y = 0, f_max_x = 0, f_max_y = 0;
};
// Construct the cached rectangle exactly as the reference Element constructor.
Element make_element(std::int64_t index, std::int64_t min_x, std::int64_t min_y,
                     std::int64_t max_x, std::int64_t max_y, std::int64_t hf,
                     std::int64_t flags, std::int64_t x, std::int64_t y,
                     std::int64_t z, std::int64_t cosine, std::int64_t sine);
struct LevelData {
  std::uint32_t stamp = 0;
  std::int64_t tg_x0 = 0, tg_y0 = 0, tg_w = 0, tg_h = 0;
  std::vector<std::vector<std::int64_t>> grid{};
  std::vector<Element> elements{};
  std::vector<HeightField> heightfields{};
  std::vector<std::int64_t> pieces{};
  // Retained, uninterpreted path metadata; no pathfinding in this module.
  std::int64_t pg_x0 = 0, pg_y0 = 0, pg_w = 0, pg_h = 0;
  std::vector<std::uint32_t> path_cells{};
};
struct Snapshot {
  LevelData data;
  Limits limits;
};
namespace detail {
struct Access;
}
class Level {
  LevelData data_;
  Limits limits_;
  friend struct detail::Access;

public:
  // Supplied POST-fixup data: no file parser or automatic piece sample rewrite.
  // Field sharing is by hf vector index, independent of the offset label.
  explicit Level(const LevelData &data = {}, Limits limits = {});
  const LevelData &data() const noexcept { return data_; }
  Snapshot snapshot() const { return {data_, limits_}; }
  // Advance before a new standalone collection; compound queries advance their
  // own stamp. Unsigned wrap matches the reference _next_stamp operation.
  void next_stamp() noexcept { ++data_.stamp; }
  void set_piece_collision(std::int64_t piece, bool on);
  void set_piece_height(std::int64_t piece, Raw height);
  std::int64_t get_piece_height(std::int64_t piece) const;
};
struct SweepResult {
  Raw time;
  std::optional<Vec3> normal;
};
struct SlideResult {
  int hit;
  Vec3 position;
  std::optional<Vec3> normal;
};
struct GroundResult {
  Vec3 position;
  Raw vz;
  int landed;
};
struct MoveResult {
  Vec3 position;
  Raw vz;
  int bits;
};
// collect does NOT increment the stamp; callers use Level::next_stamp() for a
// new collection pass. Results are storage indices, allowing
// duplicate Element.index labels without losing object identity.
std::vector<std::size_t> collect_elements(Level &, Raw min_x, Raw min_y,
                                          Raw max_x, Raw max_y,
                                          std::int64_t limit);
Raw terrain_height(Level &, Raw radius, const Vec3 &);
Raw terrain_height_below(Level &, Raw radius, const Vec3 &);
Raw terrain_height_at_point(Level &, const Vec3 &);
Raw terrain_test_flag1(Level &, Raw radius, const Vec3 &);
SweepResult terrain_sweep_circle(Level &, Raw radius, const Vec3 &position,
                                 const Vec3 &delta, std::uint32_t mask,
                                 bool want_normal = true);
SlideResult world_slide_move(Level &, Raw radius, const Vec3 &position,
                             const Vec3 &delta, std::uint32_t mask);
std::uint32_t mover_mask(std::uint32_t object_flags) noexcept;
GroundResult ground_step(Level &, Raw radius, const Vec3 &position, Raw vz,
                         Raw stand = NO_STAND);
MoveResult object_move_isolated(Level &, Raw radius, const Vec3 &position,
                                const Vec3 &velocity, Raw vz,
                                std::uint32_t object_flags);
} // namespace champions::simulation::rta::world_queries
