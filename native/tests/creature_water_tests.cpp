// Exact authored synthetic cases ported from research tools/test_ref_water_services.py
// at 2502e2bfac58203ac0174fccd7e0953b36d06439, SHA256
// 2895bbe1470b8ecbe6c16057353d1a9e41a8bcedf2fbbf21a2fcad73ce87f0c8.
#include "champions/creature_water.hpp"
#include "champions/core_arithmetic.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace W = champions::simulation::rta::creature_water;
namespace M = champions::simulation::rta::creature_movement;
using namespace champions::simulation::rta;
namespace {
constexpr std::uint32_t WORLD = 0x1000, TREE = 0x2000, CHUNKS = 0x3000,
                        PAYLOAD = 0x5000, GRID = 0x6000, HEIGHT = 0x9000;
void check(bool ok, const char *message) {
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
template <class Error, class F> void rejects(F &&body, const char *message) {
  try { body(); } catch (const Error &) { return; }
  check(false, message);
}
Raw f(std::int32_t value) { return from_int(std::uint32_t(value)); }
struct Fixture {
  W::Memory mem;
  W::Service service{mem};
  Fixture(std::vector<std::uint16_t> indices = {0}, bool grids = true) {
    mem.poke(WORLD + 4, 4, TREE);
    mem.poke(WORLD + 0x24, 4, CHUNKS);
    mem.poke(TREE, 2, indices.size());
    std::uint16_t maximum = 0;
    for (std::size_t i = 0; i < indices.size(); ++i) {
      mem.poke(TREE + 2 + 2 * i, 2, indices[i]);
      maximum = std::max(maximum, indices[i]);
    }
    for (std::uint32_t i = 0; i <= maximum; ++i) {
      const auto chunk = CHUNKS + 0x3c * i;
      const auto payload = PAYLOAD + 0x90 * i;
      mem.poke(chunk, 4, payload);
      mem.poke(chunk + 0x24, 4, 0x80);
      for (std::uint32_t axis = 0; axis < 3; ++axis) {
        mem.poke(chunk + 8 + 4 * axis, 4, f(axis < 2 ? -192 : 0));
        mem.poke(chunk + 0x14 + 4 * axis, 4, f(axis < 2 ? 192 : 0));
      }
      mem.poke(payload + 8, 4, grids ? GRID : 0);
    }
  }
  std::int32_t cell(int x, int y) const {
    return s16(std::uint32_t(mem.load(GRID + 0xa90 + 2 * (32 * x + y), 2)));
  }
};
void query_vectors() {
  Fixture a({0, 1, 0, 2});
  a.mem.poke(CHUNKS + 0x3c + 0x24, 4, 0);
  const auto hits = a.service.query(WORLD, {f(192), 0, 0}, {f(200), 0, 0}, 3, HEIGHT);
  check(hits == std::vector<std::uint32_t>{CHUNKS, CHUNKS}, "inclusive duplicate hits and capacity");
  const auto &writes = a.service.writes();
  check(writes.size() == 4 && writes[0].address == HEIGHT && writes[0].value == 0 &&
        writes[1].value == CHUNKS && writes[2].value == CHUNKS &&
        writes[3].address == HEIGHT + 8 && writes[3].value == 0,
        "ordered output clear, duplicate pointers, terminator");
  check(a.service.query(WORLD, {f(193), 0, 0}, {f(200), 0, 0}, 3).empty(),
        "outside inclusive boundary misses");
  rejects<W::Unsupported>([&] { a.service.query(WORLD, {}, {}, 1); }, "capacity refusal");

  Fixture b({0, 1});
  b.mem.poke(TREE, 2, 0xc000);
  b.mem.poke(TREE + 2, 4, 0);
  b.mem.poke(TREE + 6, 4, 0);
  b.mem.poke(TREE + 18, 2, 3);
  b.mem.poke(TREE + 20, 2, 1);
  b.mem.poke(TREE + 22, 2, 0);
  b.mem.poke(TREE + 24, 2, 1);
  b.mem.poke(TREE + 26, 2, 1);
  check(b.service.query(WORLD, {}, {}, 3) ==
            std::vector<std::uint32_t>{CHUNKS + 0x3c, CHUNKS},
        "both-child split visits right first");
  check(b.service.query(WORLD, {f(1), 0, 0}, {f(2), 0, 0}, 3) ==
            std::vector<std::uint32_t>{CHUNKS + 0x3c},
        "right-only split");
  check(b.service.query(WORLD, {f(-2), 0, 0}, {f(-1), 0, 0}, 3) ==
            std::vector<std::uint32_t>{CHUNKS}, "left-only split");
  b.mem.poke(TREE, 2, 0xc003);
  rejects<W::Unsupported>([&] { b.service.query(WORLD, {}, {}, 3, HEIGHT); }, "axis refusal");
  check(b.service.writes().size() == 1 && b.service.writes()[0].address == HEIGHT &&
            b.service.writes()[0].value == 0,
        "query output clear survives later refusal");
  b.mem.poke(TREE, 2, 0x8000);
  check(b.service.query(WORLD, {}, {}, 2).empty(), "inert internal node");
}
void test_vectors() {
  Fixture a({1, 0});
  a.mem.poke(CHUNKS + 0x3c + 0x10, 4, f(8));
  a.mem.poke(CHUNKS + 0x3c + 0x1c, 4, f(8));
  a.mem.poke(HEIGHT, 4, 0x12345678);
  check(a.service.water_test(f(1), WORLD, {f(194), 0, 0}, HEIGHT) == 1 &&
            a.mem.load(HEIGHT, 4) == f(8), "padded test uses first traversal hit height");
  check(a.service.water_test(f(1), WORLD, {f(195), 0, 0}, HEIGHT) == 0 &&
            a.mem.load(HEIGHT, 4) == f(8), "miss preserves height");
  check(a.service.water_test(0, WORLD, {0, 0, f(-61)}) == 0,
        "z upper boundary outside");
  check(a.service.water_test(0, WORLD, {0, 0, f(-60)}) == 1,
        "z upper boundary touches");
  Fixture empty(std::vector<std::uint16_t>{});
  check(empty.service.water_test(f(12), WORLD, {}) == 0, "empty leaf");
}
void disturb_vectors() {
  Fixture a;
  check(a.service.water_disturb(f(100), f(12), WORLD, {}) == 1,
        "center disturbance count");
  check(a.cell(16, 16) == 100 && a.cell(16, 15) == 0 &&
            a.cell(15, 15) == 0 && a.service.writes().size() == 1,
        "reciprocal boundary, center falloff and ordered single store");
  Fixture b({0, 0});
  const auto center = GRID + 0xa90 + 2 * (32 * 16 + 16);
  b.mem.poke(center, 2, 32760);
  check(b.service.water_disturb(f(100), f(12), WORLD, {}) == 2 &&
            b.cell(16, 16) == 32767 && b.service.writes().size() == 2,
        "duplicate count and upper clamp");
  b.mem.poke(center, 2, -32760);
  b.service.water_disturb(f(-100), f(12), WORLD, {});
  check(b.cell(16, 16) == 32576, "negative signed16 overflow wraps");
  Fixture c(std::vector<std::uint16_t>(20, 0), false);
  check(c.service.water_disturb(f(100), f(12), WORLD, {}) == 15 &&
            c.service.writes().empty(), "15 result cap includes absent grids");
  c.mem.poke(0x44fcec, 1, 0xff);
  check(c.service.water_disturb(f(100), 0, 0, {}) == 0,
        "cut-skip precedes world and radius reads");
  c.mem.poke(0x44fcec, 1, 0);
  rejects<W::Unsupported>([&] { c.service.water_disturb(f(100), 0, WORLD, {}); },
                          "nonpositive radius refuses");
  Fixture d({0}, false);
  check(d.service.water_test(f(1), WORLD, {f(194), 0, 0}) == 1 &&
            d.service.water_disturb(f(100), f(1), WORLD, {f(194), 0, 0}) == 0,
        "test padding is absent from disturb query");
  Fixture multiple({0, 1});
  constexpr std::uint32_t second_grid = 0xa000;
  multiple.mem.poke(PAYLOAD + 0x90 + 8, 4, second_grid);
  check(multiple.service.water_disturb(f(100), f(12), WORLD, {}) == 2 &&
            multiple.cell(16, 16) == 100 &&
            s16(std::uint32_t(multiple.mem.load(second_grid + 0xa90 + 2 * (32 * 16 + 16), 2))) == 100 &&
            multiple.service.writes().size() == 2 &&
            multiple.service.writes()[0].address < multiple.service.writes()[1].address,
        "two distinct chunks update supplied grids in query order");
  Fixture e;
  e.mem.poke(CHUNKS + 0x18, 4, f(384));
  for (std::uint32_t i = 0; i < 0x90; ++i)
    if (i < 8 || i >= 12)
      e.mem.poke(PAYLOAD + i, 1, 0xa5);
  e.service.water_disturb(f(100), f(12), WORLD, {});
  check(e.service.writes().size() == 1 &&
            e.service.writes()[0].address == center,
        "non-square chunk uses X width; mask and neighbors ignored");
  Fixture narrow;
  narrow.mem.poke(CHUNKS + 8, 4, 0);
  narrow.mem.poke(CHUNKS + 0x14, 4, 0);
  rejects<W::Unsupported>([&] { narrow.service.water_disturb(f(100), f(12), WORLD, {}); },
                          "rounded width refusal");
  Fixture partial({0, 1});
  partial.mem.poke(PAYLOAD + 0x90 + 8, 4, second_grid);
  partial.mem.poke(CHUNKS + 0x3c + 8, 4, 0);
  partial.mem.poke(CHUNKS + 0x3c + 0x14, 4, 0);
  rejects<W::Unsupported>([&] { partial.service.water_disturb(f(100), f(12), WORLD, {}); },
                          "later chunk width refusal");
  check(partial.cell(16, 16) == 100 && partial.service.writes().size() == 1,
        "first chunk ripple write survives later refusal");
}
void arithmetic_mode_vectors() {
  // Reference writes from the same 384-wide, radius-12 center fixture.
  const std::array<Fpu::Model, 4> modes{Fpu::Model::Ps2, Fpu::Model::Ps2g1,
                                        Fpu::Model::Chop, Fpu::Model::Nearest};
  for (const auto mode : modes) {
    set_fpu(mode);
    Fixture sample;
    check(sample.service.water_disturb(f(100), f(12), WORLD, {}) == 1,
          "arithmetic mode disturbance count");
    const auto &writes = sample.service.writes();
    if (mode == Fpu::Model::Nearest) {
      check(writes.size() == 5 && writes[0].address == 28272 &&
                writes[1].address == 28334 && writes[2].address == 28336 &&
                writes[3].address == 28338 && writes[4].address == 28400 &&
                writes[0].value == 0 && writes[1].value == 0 &&
                writes[2].value == 100 && writes[3].value == 0 &&
                writes[4].value == 0,
            "nearest mode writes zero increments on circle boundary");
    } else {
      check(writes.size() == 1 && writes[0].address == 28336 &&
                writes[0].value == 100,
            "ps2, ps2g1 and chop mode reciprocal boundary");
    }
  }
  set_fpu(Fpu::Model::Ps2);
}
void adapter_vector() {
  W::Memory backing;
  W::Service service(backing);
  M::Model creature(backing);
  constexpr std::uint32_t C = 0xa000;
  backing.poke(WORLD + 4, 4, TREE);
  backing.poke(WORLD + 0x24, 4, CHUNKS);
  backing.poke(TREE, 2, 1);
  backing.poke(TREE + 2, 2, 0);
  backing.poke(CHUNKS, 4, PAYLOAD);
  backing.poke(CHUNKS + 0x24, 4, 0x80);
  for (int k = 0; k < 3; ++k) {
    backing.poke(CHUNKS + 8 + 4 * k, 4, f(k < 2 ? -192 : 0));
    backing.poke(CHUNKS + 0x14 + 4 * k, 4, f(k < 2 ? 192 : 0));
  }
  backing.poke(PAYLOAD + 8, 4, GRID);
  backing.poke(M::G_WORLD, 4, WORLD);
  backing.poke(C + 0x44, 4, 1);
  creature.callbacks["libc_sprintf"] = [](M::Model &, const auto &) { return M::Value::buffer("splash"); };
  creature.callbacks["res_find"] = [](M::Model &, const auto &) { return M::Value::integer(1); };
  creature.callbacks["sound_playOnObject"] = [](M::Model &, const auto &) { return M::Value{}; };
  W::install_creature_callbacks(creature, service);
  creature.water(C);
  check((backing.load(C + 0xa8, 8) & UINT64_C(0x2000000000000000)) != 0 &&
            !service.writes().empty() &&
            std::any_of(service.writes().begin(), service.writes().end(), [](const auto &w) {
              return w.address == GRID + 0xa90 + 2 * (32 * 16 + 16);
            }),
        "opt-in creature callback uses supplied grid and enters water");
  check(creature.calls.size() == 5 && creature.calls[0].name == "water_test" &&
            creature.calls[4].name == "water_disturb", "creature caller service order");
  W::Service other(backing);
  M::Model different;
  rejects<W::Unsupported>([&] { W::install_creature_callbacks(different, other); },
                          "adapter refuses unshared memory");
}
} // namespace
int main() {
  set_fpu(Fpu::Model::Ps2);
  query_vectors();
  test_vectors();
  disturb_vectors();
  arithmetic_mode_vectors();
  adapter_vector();
  std::cout << "PASS creature-water synthetic contract and adapter\n";
}
