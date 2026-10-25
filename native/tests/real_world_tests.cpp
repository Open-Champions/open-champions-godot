// Authored synthetic WORLD: one geometry chunk claimed as a moving piece, one
// water tile, a one-leaf tree, one terrain element/heightfield. No game bytes.
#include "champions/core_arithmetic.hpp"
#include "champions/real_world.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace R = champions::real_world;
namespace W = champions::presentation::rta::world_geometry;
namespace L = champions::simulation::rta::level_flow;
using champions::simulation::rta::from_int;
using champions::simulation::rta::Raw;
namespace {
std::size_t checks = 0;
void check(bool ok, const char *what) {
  ++checks;
  if (!ok)
    throw std::runtime_error(what);
}
struct Builder {
  std::vector<std::uint8_t> b = std::vector<std::uint8_t>(0x84, 0);
  std::size_t at(std::size_t n) {
    const auto p = b.size();
    b.resize(p + ((n + 15) & ~std::size_t(15)), 0);
    return p;
  }
  void u16(std::size_t p, std::uint16_t v) {
    b[p] = std::uint8_t(v);
    b[p + 1] = std::uint8_t(v >> 8);
  }
  void u32(std::size_t p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
      b[p + i] = std::uint8_t(v >> (8 * i));
  }
  void f32(std::size_t p, float v) {
    std::uint32_t u;
    std::memcpy(&u, &v, 4);
    u32(p, u);
  }
};
std::uint32_t command(unsigned cmd, unsigned num, unsigned imm) {
  return (cmd << 24) | (num << 16) | imm;
}
// Three-vertex program-0x46 strip with UVs (mirrors the public fixture shape).
std::vector<std::uint8_t> packet() {
  std::vector<std::uint8_t> p;
  auto w = [&](std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
      p.push_back(std::uint8_t(v >> (8 * i)));
  };
  auto h = [&](std::uint16_t v) {
    p.push_back(std::uint8_t(v));
    p.push_back(std::uint8_t(v >> 8));
  };
  auto pad = [&] {
    while (p.size() % 4)
      p.push_back(0);
  };
  w(command(0x69, 3, 0)); // positions s16 x3
  for (int v : {0, 0, 0, 40, 0, 0, 0, 40, 0})
    h(std::uint16_t(v));
  pad();
  w(command(0x6a, 3, 0)); // normals s8 x3
  for (int i = 0; i < 9; ++i)
    p.push_back(i % 3 == 2 ? 127 : 0);
  pad();
  w(command(0x69, 5, 0x4000)); // strip table u16 x3
  for (int v : {0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 6, 0, 0})
    h(std::uint16_t(v));
  pad();
  w(command(0x6d, 1, 0x4000)); // extra copies u16 x4 (count 0)
  for (int v : {0, 0, 0, 0})
    h(std::uint16_t(v));
  w(command(0x6c, 1, 0x4000)); // GIF tag u32 x4: 3 slots, nreg 3, strip
  w(3 | 0x8000);
  w((3u << 28) | (4u << 15));
  w(0);
  w(0);
  w(command(0x14, 0, 0x46));
  w(command(0x65, 3, 0x4000)); // UVs u16 x2
  for (int v : {0, 0, 16, 0, 0, 16})
    h(std::uint16_t(v));
  while (p.size() % 16)
    p.push_back(0);
  return p;
}
} // namespace
int main() {
  try {
    Builder w;
    const auto pk = packet();
    const auto pkt = w.at(pk.size());
    std::copy(pk.begin(), pk.end(), w.b.begin() + std::ptrdiff_t(pkt));
    const auto water = w.at(0x90);
    w.u32(water, 0xffffffff);
    w.u32(water + 4, 0xffffffff);
    w.u32(water + 8, 0x12345678); // stale run-time word must read as no grid
    const auto tree = w.at(8);
    w.u16(tree, 0x4567);
    w.u16(tree + 2, 2); // leaf: two chunks
    w.u16(tree + 4, 0);
    w.u16(tree + 6, 1);
    const auto hf = w.at(0x14 + 4 * 2);
    w.u32(hf + 8, 2);
    w.u32(hf + 12, 2);
    const auto pieces = w.at(0x1c);
    w.u32(pieces, 0);      // first chunk 0
    w.u16(pieces + 4, 1);  // count 1
    w.u32(pieces + 8, 0);  // rest translation 0,0,0
    w.u16(pieces + 0x14, 32767);
    w.u16(pieces + 0x18, 0); // element 0
    const auto cells = w.at(8);
    const auto elements = w.at(0x24);
    w.u32(elements + 0x0c, std::uint32_t(hf));
    const auto chunks = w.at(2 * 0x3c);
    // chunk 0: piece geometry, cell 1, flag 0x20
    w.u32(chunks, std::uint32_t(pkt));
    w.u32(chunks + 4, std::uint32_t(pk.size() / 16));
    for (int a = 0; a < 3; ++a) {
      w.f32(chunks + 8 + 4 * a, -100);
      w.f32(chunks + 0x14 + 4 * a, 100);
    }
    w.u32(chunks + 0x24, 0x20);
    w.u16(chunks + 0x34, 1);
    w.u16(chunks + 0x36, 32767);
    // chunk 1: water tile, x,y -60..60, z 8..8
    const auto c1 = chunks + 0x3c;
    w.u32(c1, std::uint32_t(water));
    w.f32(c1 + 8, -60);
    w.f32(c1 + 0xc, -60);
    w.f32(c1 + 0x10, 8);
    w.f32(c1 + 0x14, 60);
    w.f32(c1 + 0x18, 60);
    w.f32(c1 + 0x1c, 8);
    w.u32(c1 + 0x24, 0x80);
    w.u32(c1 + 0x30, 32); // translation z 32 quarter units = 8
    w.u32(0x00, 2);
    w.u32(0x04, std::uint32_t(tree));
    w.u32(0x1c, 1);
    w.u32(0x20, std::uint32_t(elements));
    w.u32(0x24, std::uint32_t(chunks));
    w.u32(0x50, 1);
    w.u32(0x54, std::uint32_t(pieces));
    w.u32(0x58, 1);
    w.u32(0x5c, 1);
    w.u32(0x64, std::uint32_t(cells));
    w.u32(0x6c, 0);
    w.u32(0x78, 0);
    w.u32(0x80, 0);
    const auto bytes = w.b;
    const auto g = W::decode(bytes);
    check(g.water.size() == 1 && g.pieces.size() == 1, "fixture decodes");
    check(g.chunks[0].packet_index.has_value(), "piece chunk is geometry");

    R::WaterImage image(bytes, g);
    const auto in = image.test(from_int(12), {from_int(0), from_int(0), from_int(0)});
    check(in.in_water && in.height == 0x41000000u,
          "water_test hits the tile and reports its minimum z (8)");
    const auto deep = image.test(from_int(12), {0, 0, from_int(-100)});
    check(!deep.in_water, "more than 60 below the surface is not in water");
    const auto far = image.test(from_int(12), {from_int(200), 0, 0});
    check(!far.in_water, "outside the tile bounds is dry");
    check(image.disturb(from_int(300), from_int(12), {0, 0, 0}) == 1,
          "disturb counts the tile even without a ripple grid");
    check(image.service().writes().empty(), "no ripple grid is written");
    check(image.disturb_chunks(from_int(12), {0, 0, 0}) == std::vector<std::uint32_t>{1},
          "the disturb query names the water chunk by its table index");
    check(image.disturb_chunks(from_int(12), {from_int(200), 0, 0}).empty(),
          "a disturb outside the tile admits no chunk");
    check(image.disturb_chunks(from_int(12), {0, 0, from_int(-100)}).empty(),
          "more than 60 below the surface admits no chunk");
    {
      // The player's wading on this tile (research player-wading.md): the tile
      // surface is 8 above the feet, inside the entry band [-1, 60].
      R::Wader wader(image, [&](std::uint32_t strength, std::uint32_t radius, const champions::simulation::rta::creature_water::Vec3 &p) {
        return image.disturb(strength, radius, p);
      });
      const champions::simulation::rta::creature_water::Vec3 feet{0, 0, 0}, away{from_int(200), 0, 0};
      check(!wader.step(1, 0, feet, true), "no water test on frames with frame & 3 != 0");
      check(wader.step(4, 0, feet, true) && wader.totals().entries == 1 && wader.totals().draws == 1 &&
                wader.totals().splashes == 1 && wader.totals().disturbs == 1,
            "frame 4: entry, one shared draw, one splash, one entry ripple");
      wader.step(5, 0, feet, true);
      check(wader.totals().disturbs == 2, "a moving wader ripples every frame");
      wader.step(6, 0, feet, false);
      check(wader.totals().disturbs == 2, "a still wader ripples only on sixteenth frames");
      wader.step(16, 0, feet, false);
      check(wader.totals().disturbs == 3 && wader.in_water(), "the sixteenth frame ripples");
      wader.step(32, 0x1000, feet, false);
      check(wader.totals().hurts == 1, "hurting water is counted on a sixteenth frame");
      check(!wader.step(33, 0, away, true) && wader.totals().exits == 1,
            "a wading ripple that touches no water clears the bit");
      check(wader.totals().draws == 1, "wading takes no shared draw");
    }
    check(bytes == w.b, "input bytes stay unchanged");

    L::Record door;
    door.cls = "SomeProp";
    door.props = {"autodoor=1"};
    L::Record plain;
    plain.cls = "SomeProp";
    plain.props = {"AutoDoor=0"};
    check(R::claims_piece(door) && !R::claims_piece(plain),
          "a non-zero AutoDoor property claims, case-insensitively");
    R::PieceScene scene(bytes, g, {plain, door});
    check(scene.size() == 1 && scene.claimed() == 1 && scene.claimer(0) == 1 &&
              scene.claim_attempts() == 1 && scene.collision(0),
          "the AutoDoor claims the nearest piece; collision stays on");
    check(scene.draws().size() == 1 &&
              scene.draws()[0].affine ==
                  std::array<double, 12>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0},
          "an unmoved piece draws at its file pose");
    scene.set_door(0, true);
    const auto open = scene.draws().at(0).affine;
    check(!scene.collision(0) && std::abs(open[11] + 256) < 1e-9,
          "an open AutoDoor drops 256 and loses collision");
    scene.set_door(0, false);
    check(scene.collision(0), "closing restores collision");
    R::PieceScene lonely(bytes, g, {plain});
    check(lonely.claimed() == 0 && !lonely.collision(0),
          "an unclaimed piece loses collision after the objects");
    bool refused = false;
    try {
      lonely.set_door(0, true);
    } catch (const R::Error &) {
      refused = true;
    }
    check(refused, "an unclaimed piece cannot be driven");
    L::Record start = door;
    start.props.push_back("StartState=1");
    check(R::autodoor_start_open(start).value_or(false),
          "StartState 1 of a default two-state AutoDoor is passable");
    R::PieceScene opened(bytes, g, {start});
    check(opened.started_open() == 1 && !opened.collision(0),
          "a passable start state opens the door at load");
    std::cout << "real WORLD adapter checks: " << checks << "; failures: 0\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "real WORLD adapter failure: " << e.what() << '\n';
    return 1;
  }
}
