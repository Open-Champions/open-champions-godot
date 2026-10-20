#pragma once

#include "champions/core_arithmetic.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

// Exact port of the Return to Arms floor-decal reference tools/ref_decals.py (research
// commit 19f80a6, round 2, executed bit for bit there): decal_add 0x19f110 and the
// growth step of decal_drawTask 0x19f4e8. Float work goes through the champions-core
// port of tools/ref_core.py (raw single-precision patterns). The 64 blocks are held as
// fixed block memory, as in the original, so a decal handle stays valid until a reset.
namespace champions::rta::fx::sim {
struct WorldCodec;
}
namespace champions::rta::fx::decals {

using Raw = champions::simulation::rta::Raw;

inline constexpr int GRID = 30;              // cells per side, 256 units each
inline constexpr int GRID_ORIGIN = 0xF00;    // -3840
inline constexpr int BLOCKS = 64;            // 0x840450, 0x180 bytes each
inline constexpr int PER_BLOCK = 15;
inline constexpr int GROW_SLOTS = 16;        // 0x840310
inline constexpr int TEX_BASE = 0x3D3;
inline constexpr Raw STEP_FAST = 0x3E4CCCCCu;  // 0.19999999: grow modes other than 2
inline constexpr Raw STEP_SLOW = 0x3D4CCCCCu;  // 0.049999997: grow mode 2

// ref_core.f32: the pattern of the IEEE single nearest to a binary64 (infinity on
// overflow) and to_float: the IEEE reading of a pattern.
Raw f32(double value) noexcept;
double to_float(Raw raw) noexcept;

// The gate of decal_add (g_decalDetail 0x44fdc0); the type comparison is unsigned.
bool allowed(std::int64_t kind, std::int64_t detail) noexcept;
// (cx, cy) of the grid or nothing: ((trunc(x) + 3840) >> 8, (trunc(y) + 3840) >> 8).
std::optional<std::pair<int, int>> cell_of(double x, double y) noexcept;

struct Decal {
    std::array<int, 3> rgb{};
    std::int32_t x4 = 0, y4 = 0, z4 = 0, counted = 0, c4 = 0, s4 = 0, tex = 0;
    std::int64_t kind = 0;
    // The three qwords of s16 as decal_add writes them; the last s16 is not written by
    // decal_add (left as the block held it: 0 here).
    std::array<std::int16_t, 12> record() const;
};

struct Block {
    int index = 0;
    int count = 0;                    // +0x178
    std::int32_t zmin = 0, zmax = 0;  // +0x174 / +0x176 (start at 0)
    int counted = 0;                  // +0x17c: decals of types 0-2
    std::array<Decal, PER_BLOCK> decals{};
};

struct Grower {
    int mode = 0;
    int block = 0, index = 0;  // the decal it grows, as block memory
    std::uint32_t angle = 0;
    Raw size = 0, final_size = 0;
};

class DecalStore {
    friend struct ::champions::rta::fx::sim::WorldCodec;   // fx_world_save.cpp
public:
    // The state after the level reset: all blocks free in index order, cells empty, no
    // growing decal, total 0.
    explicit DecalStore(std::int64_t detail = 2);

    std::int64_t detail;

    // decal_add(size f12, pos a0, angle16 a1, type a2, grow a3). `tint` is what
    // light_sampleFloor 0x148ff0 returns for the position. Returns the decal (valid until
    // the next reset), or nullptr when the gate, the grid, the blocks or the cell drop it.
    const Decal *add(double size, const std::array<double, 3> &pos, std::int64_t angle,
                     std::int64_t kind, std::int64_t grow = 0,
                     const std::array<std::int64_t, 3> &tint = {0, 0, 0});
    // The head of decal_drawTask: one call per rendered frame.
    void grow_step(bool frozen = false);

    // decal_reset 0x19f018 as docs/effects.md 16.2 describes it (I from the code, not
    // executed; not part of the reference): the grid and the blocks are cleared and the
    // free list rebuilt in index order, but the 16 growing slots are left alone, so a
    // decal still growing keeps writing its corner vector into the recycled block memory.
    // The research does not say what the reset does to the decal total (0x4d8a00), so this
    // leaves it unchanged; set_total() lets a caller that knows apply it.
    void reset();
    void set_total(std::int32_t total) noexcept { total_ = total; }

    // --- read access
    std::int32_t total() const noexcept { return total_; }
    const std::deque<int> &free_blocks() const noexcept { return free_; }
    int cell_block(int cx, int cy) const;  // block index or -1
    const Block &block(int index) const;
    const std::array<std::optional<Grower>, GROW_SLOTS> &growers() const noexcept { return growers_; }
    // Every decal by cell (cells ordered by y then x), in block order.
    std::vector<std::pair<std::pair<int, int>, const Decal *>> decals() const;

private:
    std::deque<int> free_;
    std::array<int, GRID * GRID> grid_{};
    std::array<Block, BLOCKS> blocks_{};
    std::array<std::optional<Grower>, GROW_SLOTS> growers_{};
    std::int32_t total_ = 0;
};

}  // namespace champions::rta::fx::decals
