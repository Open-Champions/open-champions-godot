#pragma once
// Adapters that connect one decoded RTA WORLD to the merged water services and
// level-flow moving pieces. They read only the supplied bytes, allocate no
// ripple grids and invent no constants. Collision changes made here apply to
// this adapter's own terrain copy; a separate session keeps its own.
#include "champions/creature_water.hpp"
#include "champions/level_flow.hpp"
#include "champions/world_geometry.hpp"
#include "champions/world_queries.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace champions::real_world {
namespace wg = presentation::rta::world_geometry;
namespace cw = simulation::rta::creature_water;
namespace lf = simulation::rta::level_flow;
namespace wq = simulation::rta::world_queries;
using Raw = simulation::rta::Raw;

struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Read-only relocated image of a WORLD for creature_water::Service. Pointer
// fields read by the services (world +0x04 root node past the 0x4567 marker,
// +0x24 chunk table, water chunk +0x00 payload) hold BASE + file offset. Each
// payload's +0x08 ripple-grid pointer reads 0. The service therefore returns
// the chunk count and writes nothing. The drawn field keeps the cosmetic grid
// and receives the same disturb when that count is not zero.
class WaterImage {
public:
  static constexpr cw::Address BASE = 0x01000000;
  WaterImage(const std::vector<std::uint8_t> &world, const wg::Geometry &);
  WaterImage(const WaterImage &) = delete;
  WaterImage &operator=(const WaterImage &) = delete;
  cw::Address world() const noexcept { return BASE; }
  cw::Service &service() noexcept { return service_; }
  struct Test {
    bool in_water = false;
    Raw height = 0; // chunk minimum z (raw32) of the first traversal hit
  };
  Test test(Raw radius, const cw::Vec3 &position);
  std::uint32_t disturb(Raw strength, Raw radius, const cw::Vec3 &position);
  // Chunk-table indices of the water chunks water_disturb's query admits, in
  // query order (water-draw section 6). Empty when the cut byte is set.
  std::vector<std::uint32_t> disturb_chunks(Raw radius, const cw::Vec3 &position);

private:
  std::vector<std::uint8_t> image_;
  cw::Memory memory_;
  cw::Service service_;
  std::uint64_t load(cw::Address, std::size_t) const;
};

// The player's wading on this WORLD's water: Player_move step (f)
// (champions/player_wading.hpp, research player-wading.md). The in-water bit
// lives here, not in the session: the diagnostic control walk the real-level
// scenes run has no Player_move, stick, jump, hit points or shared seed, so
// the caller states what it can observe:
//   frame     the ready tick, used as g_simFrame for the & 3 / & 15 gates;
//   position  the feet, raw floats;
//   moving    the stick speed stand-in: s = 1.0 when the feet moved since the
//             previous call, else 0.0 (only s >= 0.01 matters to the step).
// Not applied, only counted: the entry's shared-seed draw (`draws`) and the
// hurting-water damage (`hurts`). The entry splash sound and burst are
// presentation and are not played. `airborne` is always false here.
// The Godot scenes REFUSE to run this by default (no host frame counter or
// stick speed); it runs only behind the explicit --wade-diagnostic opt-in.
// The disturb callback receives each ripple's strength, radius and point and
// returns the chunks touched; the Godot binding also feeds the drawn field.
class Wader {
public:
  using Disturb = std::function<std::uint32_t(Raw strength, Raw radius, const cw::Vec3 &point)>;
  struct Totals {
    std::uint64_t steps = 0, ticks_in_water = 0, entries = 0, exits = 0, draws = 0,
                  hurts = 0, disturbs = 0, touched = 0, splashes = 0;
  };
  Wader(WaterImage &image, Disturb disturb) : image_(image), disturb_(std::move(disturb)) {}
  bool in_water() const noexcept { return in_water_; }
  const Totals &totals() const noexcept { return totals_; }
  // Returns the bit after the step.
  bool step(Raw frame, std::uint32_t level_flags, const cw::Vec3 &position, bool moving);

private:
  WaterImage &image_;
  Disturb disturb_;
  bool in_water_ = false;
  Totals totals_;
};

// Pieces of the table, element from +0x18, mirroring from chunk flag 0x40.
std::vector<lf::Piece> pieces(const wg::Geometry &);

// Which records claim a piece in their constructor (level-flow 11.4, 13.6):
// a non-zero `AutoDoor` property (AutoProp family), or a `DoorSwing`
// (WorldPart). Keys and class names compare case-insensitively.
bool claims_piece(const lf::Record &);
// Documented initial passability of a claiming AutoDoor (monsters-2 AutoProp
// section 3): with `StartState` n < states, state n's `Passable` flag, or with
// `States` absent, state 1 is passable. nullopt: not determinable or state 0.
std::optional<bool> autodoor_start_open(const lf::Record &);

struct PieceDraw {
  int piece = -1;
  std::uint32_t chunk = 0;
  // Row-major 3x4 affine in native Z-up world units that maps the chunk's
  // decoded rest-pose vertices to its current drawn pose. Presentation only.
  std::array<double, 12> affine{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
};

// One level's moving pieces driven through level_flow::Pieces: claims in
// record order, initial AutoDoor state, then world_disableUnclaimedPieces.
class PieceScene {
public:
  PieceScene(const std::vector<std::uint8_t> &world, const wg::Geometry &,
             const std::vector<lf::Record> &records);
  std::size_t size() const noexcept { return pieces_->data().size(); }
  std::size_t claimed() const;
  std::size_t claim_attempts() const noexcept { return attempts_; }
  std::size_t started_open() const noexcept { return started_open_; }
  int claimer(std::size_t piece) const { return claimer_.at(piece); }
  bool collision(std::size_t piece) const;
  // AutoProp_setPassable for the claimed piece: open drops it 256 and turns
  // its collision off; closed restores both. Refuses an unclaimed piece.
  void set_door(std::size_t piece, bool open);
  std::vector<PieceDraw> draws() const;
  const wq::Level &terrain() const noexcept { return *terrain_; }

private:
  const wg::Geometry &geometry_;
  std::unique_ptr<wq::Level> terrain_;
  std::unique_ptr<lf::Pieces> pieces_;
  std::vector<int> claimer_;
  std::size_t attempts_ = 0, started_open_ = 0;
};

// Affine from a chunk's file pose to another drawn pose (world.md drawing
// rule, translation before rotation). Presentation binary64 only.
std::array<double, 12> pose_affine(const wg::Chunk &rest,
                                   const lf::ChunkTransform &pose);
} // namespace champions::real_world
