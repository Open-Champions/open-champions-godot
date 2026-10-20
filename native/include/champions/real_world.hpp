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
// payload's +0x08 ripple-grid pointer reads 0: grids are allocated only when a
// tile is drawn and are cosmetic (level-flow section 12), so water_test and
// the water_disturb return value stay exact; no ripple is written.
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

private:
  std::vector<std::uint8_t> image_;
  cw::Memory memory_;
  cw::Service service_;
  std::uint64_t load(cw::Address, std::size_t) const;
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
