// The wading step inside the composed combat runtime: a Full-composition player
// ticked over a synthetic water chunk whose services are the real
// creature_water::Service. Cases mirror research player-wading.md: entry,
// the ripple per sixteenth frame of a still wader, the deep band, leaving, the
// refusal of a missing service, and "wading does not change the move" (the
// A/B of `wadeNoSpeed`, here over whole ticks).
#include "champions/combat_runtime.hpp"
#include "champions/creature_water.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace C = champions::simulation::rta::combat_runtime;
namespace W = champions::simulation::rta::creature_water;
using namespace champions::simulation::rta;
namespace {
constexpr std::uint32_t WORLD = 0x10000, TREE = 0x10100, CHUNKS = 0x10200,
                        PAYLOAD = 0x10400, GRID = 0x11000, HEIGHT = 0x9000;
constexpr std::uint64_t WATER_BIT = UINT64_C(1) << 61;
std::size_t checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    std::exit(1);
  }
}
struct Fixture {
  W::Memory mem;
  W::Service service{mem};
  bool water = true, disturb_service = true;
  Fixture(Raw surface) {
    mem.poke(WORLD + 4, 4, TREE);
    mem.poke(WORLD + 0x24, 4, CHUNKS);
    mem.poke(TREE, 2, 1);
    mem.poke(TREE + 2, 2, 0);
    mem.poke(CHUNKS, 4, PAYLOAD);
    mem.poke(CHUNKS + 0x24, 4, 0x80);
    const Raw lo[3] = {0xc3400000, 0xc3400000, surface}, hi[3] = {0x43400000, 0x43400000, surface};
    for (unsigned k = 0; k < 3; ++k) {
      mem.poke(CHUNKS + 8 + 4 * k, 4, lo[k]);
      mem.poke(CHUNKS + 0x14 + 4 * k, 4, hi[k]);
    }
    mem.poke(PAYLOAD + 8, 4, GRID);
  }
  C::Services services() {
    auto s = C::flat_fixture_services();
    s.water_test = [this](const Vec3 &p, Raw r) {
      return Raw(water && service.water_test(r, WORLD, {p[0], p[1], p[2]}, HEIGHT));
    };
    // Authored animation stand-ins (no clip data): nothing starts, nothing ends.
    s.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
    s.animation_advance = [](C::Address, Raw) { return Raw(0); };
    s.water_height = [this](const Vec3 &, Raw) { return Raw(mem.load(HEIGHT, 4)); };
    if (disturb_service)
      s.water_disturb = [this](const Vec3 &p, Raw strength, Raw r) {
        return water ? Raw(service.water_disturb(strength, r, WORLD, {p[0], p[1], p[2]})) : Raw(0);
      };
    else
      s.water_disturb = nullptr;
    return s;
  }
  int cell(int x, int y) const {
    return s16(std::uint32_t(mem.load(GRID + 0xa90 + 2 * (32 * x + y), 2)));
  }
  int total() const {
    int sum = 0;
    for (int x = 0; x < 32; ++x)
      for (int y = 0; y < 32; ++y) sum += std::abs(cell(x, y));
    return sum;
  }
};
struct Game {
  std::unique_ptr<C::World> w;
  C::Address p = 0;
  explicit Game(C::Services s) {
    w = std::make_unique<C::World>(std::move(s), 1, C::Composition::Full);
    C::Spawn v;
    v.team = 0;
    p = w->spawn_player(0, v, 40);
  }
  bool wet() const { return (w->read(p + 0xa8, 8) & WATER_BIT) != 0; }
  std::size_t count(const char *name, std::size_t from = 0) const {
    std::size_t n = 0;
    for (auto i = from; i < w->events().size(); ++i) n += w->events()[i].name == name;
    return n;
  }
};
} // namespace

int main() {
  const Raw dz10 = 0x41200000, dz50 = 0x42480000, dz55 = 0x425c0000;
  {
    // Entry on foot: bit, one 0.4 splash, one 1000 ring at the centre cell; the
    // standing wader then adds 300 on its sixteenth frames only.
    Fixture f(dz10);
    Game g(f.services());
    std::size_t entered_at = 0;
    for (unsigned t = 0; t < 4 && !g.wet(); ++t) {
      const auto before = g.w->events().size();
      g.w->tick();
      if (g.wet()) entered_at = before;
    }
    check(g.wet(), "a dry player over water enters within four frames");
    check(g.count("presentation:sound_playOnObject", entered_at) == 1, "one splash on entry");
    check(f.cell(16, 16) == 1000 && f.cell(15, 16) == 749 && f.cell(15, 15) == 499,
          "the entry ring: 1000 centre, 749 and 499 around it");
    const int after_entry = f.total();
    std::size_t ripples = 0;
    for (unsigned t = 0; t < 33; ++t) {
      const auto before = g.w->events().size();
      g.w->tick();
      ripples += g.count("presentation:water_disturb", before);
    }
    check(ripples == 2, "a still wader ripples on frames with frame & 15 == 0 only");
    check(f.total() == after_entry + 300 * int(ripples), "each wading ripple adds 300 to one cell");
    check(g.wet(), "the wader stays in water");
  }
  {
    // Deep band (48 < dz <= 60): enters, leaves on the next wading ripple, no ring.
    Fixture f(dz55);
    Game g(f.services());
    g.w->stick(0, 0x3f800000, 0); // moving: the wader's ripple runs every frame
    std::size_t splashes = 0;
    for (unsigned t = 0; t < 12; ++t) {
      const auto before = g.w->events().size();
      g.w->tick();
      splashes += g.count("presentation:sound_playOnObject", before);
    }
    check(splashes >= 2, "the deep band keeps entering");
    check(f.total() == 0, "the deep band writes no grid cell");
  }
  {
    // A wader whose ripple finds no water leaves.
    Fixture f(dz50);
    Game g(f.services());
    g.w->write(g.p + 0xa8, 8, WATER_BIT);
    g.w->stick(0, 0x3f800000, 0);
    g.w->tick();
    check(!g.wet(), "dz 50 is above the wading band: the bit clears");
  }
  {
    // A wader needs the disturb service: it refuses rather than read as dry.
    Fixture f(dz10);
    f.disturb_service = false;
    Game g(f.services());
    g.w->write(g.p + 0xa8, 8, WATER_BIT);
    g.w->stick(0, 0x3f800000, 0);
    bool refused = false;
    try {
      g.w->tick();
    } catch (const std::exception &e) {
      refused = std::string(e.what()).find("water_disturb service missing") != std::string::npos;
    }
    check(refused, "missing water_disturb refuses");
  }
  {
    // Wading does not change the move: the same walk in water and on dry land.
    Fixture wet_world(dz10), dry_world(dz10);
    dry_world.water = false;
    Game a(wet_world.services()), b(dry_world.services());
    a.w->write(a.p + 0xa8, 8, WATER_BIT);
    a.w->stick(0, 0x3f800000, 0);
    b.w->stick(0, 0x3f800000, 0);
    for (unsigned t = 0; t < 40; ++t) {
      a.w->tick();
      b.w->tick();
    }
    check(a.wet(), "the walker stayed in water for the whole run");
    check(!b.wet(), "the control stayed dry");
    for (C::Address off : {0x10u, 0x14u, 0x18u, 0x38u, 0x468u, 0x46cu, 0x4bcu, 0x4c0u})
      check(a.w->read(a.p + off) == b.w->read(b.p + off), "wading leaves +0x" + std::to_string(off) + " unchanged");
  }
  std::cout << "player-wading-runtime: " << checks << " checks passed\n";
}
