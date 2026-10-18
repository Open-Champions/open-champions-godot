// Exact port of tools/ref_particles.py (research 19f80a6). The implementation is
// header-only in champions/fx_exact.hpp so that the round-1 library champions-native-fx
// can delegate to it without linking champions-fx-exact (champions-fx-sim links both, so
// out-of-line definitions here would collide). This translation unit compiles the
// header under the champions-fx-exact warning flags and checks its layout constants.
#include "champions/fx_exact.hpp"

namespace champions::rta::fx::exact {
static_assert(CELLS_PER_PAGE == 8 * 30, "a page holds 8 blocks of 30 particles");
static_assert(CURSOR_FULL == 0x101, "cursor value meaning take a new page");
static_assert(LIFE_TICKS[3] == 90 && SLOT_COUNT == 32, "life table and slot count");
static_assert(sizeof(CellRecord) == 16, "a particle record is 16 bytes");
}  // namespace champions::rta::fx::exact
