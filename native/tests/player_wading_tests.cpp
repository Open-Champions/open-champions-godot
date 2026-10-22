// Contract test for champions/player_wading.hpp. It replays the cases that
// tools/generate_player_wading_vectors.py computed from t3/research-wading
// (tools/ref_wading.py, 942a6ba): the executed depth-band sweep, the call
// record of Player_move step (f) over widths, heights, frames, stick speeds,
// level flags and the cut-skip byte, the 32-frame sequences and the
// sound_playFoley choice. The water services are the real
// creature_water::Service on a synthetic chunk world.
#include "champions/creature_water.hpp"
#include "champions/player_wading.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace W = champions::simulation::rta::creature_water;
namespace PW = champions::simulation::rta::player_wading;
using namespace champions::simulation::rta;
namespace {
constexpr std::uint32_t WORLD = 0x10000, TREE = 0x10100, CHUNKS = 0x10200,
                        PAYLOAD = 0x10400, GRID = 0x11000, HEIGHT = 0x9000;
constexpr Raw SEED0 = 0x12345678;
std::size_t checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    if (++failures <= 20)
      std::cerr << "FAIL: " << what << '\n';
  }
}
using Fields = std::map<std::string, std::string>;
Raw hex(const std::string &s) { return Raw(std::stoul(s, nullptr, 16)); }
struct Case {
  std::string tag;
  Fields in, out;
};
std::vector<Case> read(const char *path) {
  std::ifstream f(path);
  std::vector<Case> cases;
  std::string text;
  while (std::getline(f, text)) {
    std::istringstream ss(text);
    Case c;
    ss >> c.tag;
    std::string tok;
    Fields *into = &c.in;
    while (ss >> tok) {
      if (tok == "->") {
        into = &c.out;
        continue;
      }
      const auto eq = tok.find('=');
      (*into)[tok.substr(0, eq)] = tok.substr(eq + 1);
    }
    cases.push_back(std::move(c));
  }
  return cases;
}

// One chunk, flag 0x80, spanning [x0, x0+width] x [-192, 192] at the surface z.
struct World {
  W::Memory mem;
  W::Service service{mem};
  World(Raw surface, Raw x0, Raw x1, bool cut) {
    mem.poke(WORLD + 4, 4, TREE);
    mem.poke(WORLD + 0x24, 4, CHUNKS);
    mem.poke(TREE, 2, 1);
    mem.poke(TREE + 2, 2, 0);
    mem.poke(CHUNKS, 4, PAYLOAD);
    mem.poke(CHUNKS + 0x24, 4, 0x80);
    const Raw lo[3] = {x0, 0xc3400000, surface}, hi[3] = {x1, 0x43400000, surface};
    for (unsigned k = 0; k < 3; ++k) {
      mem.poke(CHUNKS + 8 + 4 * k, 4, lo[k]);
      mem.poke(CHUNKS + 0x14 + 4 * k, 4, hi[k]);
    }
    mem.poke(PAYLOAD + 8, 4, GRID);
    mem.poke(0x44fcec, 1, cut ? 1 : 0);
  }
  std::string cells() const {
    std::string out;
    for (int x = 0; x < 32; ++x)
      for (int y = 0; y < 32; ++y) {
        const auto v = s16(std::uint32_t(mem.load(GRID + 0xa90 + 2 * (32 * x + y), 2)));
        if (v) {
          if (!out.empty()) out += ',';
          out += std::to_string(x) + ':' + std::to_string(y) + ':' + std::to_string(v);
        }
      }
    return out.empty() ? "-" : out;
  }
};
std::string h8(Raw v) {
  char b[16];
  std::snprintf(b, sizeof b, "%08x", v);
  return b;
}
struct Run {
  explicit Run(World &w) : world(w) {}
  World &world;
  Raw seed = SEED0;
  std::vector<std::string> events;
  int draws = 0;
  PW::Host host() {
    PW::Host h;
    h.water_test = [this](const PW::Vec3 &p, Raw radius) {
      PW::TestResult r;
      r.hit = world.service.water_test(radius, WORLD, {p[0], p[1], p[2]}, HEIGHT) != 0;
      if (r.hit) {
        r.has_height = true;
        r.height = Raw(world.mem.load(HEIGHT, 4));
      }
      return r;
    };
    h.water_disturb = [this](const PW::Vec3 &p, Raw strength, Raw radius) {
      const auto n = world.service.water_disturb(strength, radius, WORLD, {p[0], p[1], p[2]});
      events.push_back("disturb:" + h8(strength) + ":" + h8(radius) + ":" + h8(p[0]) + ":" +
                       h8(p[1]) + ":" + h8(p[2]) + "=" + std::to_string(n));
      return Raw(n);
    };
    h.hurt = [this] { events.push_back("hurt"); };
    h.draw = [this] {
      events.push_back("draw");
      seed = Raw(seed * 0x19660dU + 0x3c6ef35fU);
      ++draws;
    };
    h.splash = [this](Raw volume) { events.push_back("splash:" + h8(volume)); };
    h.burst = [this](const PW::Vec3 &p, const PW::TestResult &t) {
      events.push_back("burst:" + h8(p[0]) + ":" + h8(p[1]) + ":" + h8(t.height));
    };
    return h;
  }
  bool step(Raw frame, std::uint32_t level_flags, Raw s, const PW::Vec3 &pos, bool wet, bool air) {
    PW::Input in;
    in.frame = frame;
    in.level_flags = level_flags;
    in.speed = s;
    in.position = pos;
    in.in_water = wet;
    in.airborne = air;
    return PW::step(in, host());
  }
};
std::string join(const std::vector<std::string> &v) {
  std::string out;
  for (const auto &s : v) out += (out.empty() ? "" : ",") + s;
  return out.empty() ? "-" : out;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: player-wading-tests VECTORS\n";
    return 2;
  }
  std::size_t counts[4] = {0, 0, 0, 0};
  for (const auto &c : read(argv[1])) {
    if (c.tag == "band") {
      const bool wet = c.in.at("wet") == "1";
      const Raw x0 = hex(c.in.at("x0"));
      World w(hex(c.in.at("surf")), x0, add(x0, 0x43c00000), false);
      Run run(w);
      const bool after = run.step(wet ? 4 : 0, 0, 0x3f800000, {0, 0, hex(c.in.at("pz"))}, wet, false);
      check(std::to_string(after ? 1 : 0) == c.out.at("bit"),
            "band pz=" + c.in.at("pz") + " surf=" + c.in.at("surf") + " x0=" + c.in.at("x0") + " wet=" +
                c.in.at("wet"));
      ++counts[0];
    } else if (c.tag == "step") {
      const Raw width = hex(c.in.at("width"));
      // x0 = -width / 2: exact for the three widths of the corpus.
      const Raw x0 = width == 0x43c00000 ? 0xc3400000 : width == 0x42f00000 ? 0xc2700000 : 0xc3c00000;
      World w(hex(c.in.at("surf")), x0, add(x0, width), c.in.at("cut") == "1");
      Run run(w);
      const bool wet = c.in.at("wet") == "1", air = c.in.at("air") == "1";
      const bool after = run.step(Raw(std::stoul(c.in.at("frame"))), std::uint32_t(std::stoul(c.in.at("lf"), nullptr, 16)),
                                  hex(c.in.at("s")), {0, 0, 0}, wet, air);
      std::string what = "step";
      for (const auto &kv : c.in) what += " " + kv.first + "=" + kv.second;
      check(std::to_string(after ? 1 : 0) == c.out.at("bit"), what + " bit");
      check(h8(run.seed) == c.out.at("seed"), what + " seed");
      check(join(run.events) == c.out.at("events"), what + " events " + join(run.events) + " vs " + c.out.at("events"));
      check(w.cells() == c.out.at("grid"), what + " grid");
      ++counts[1];
    } else if (c.tag == "seq") {
      const bool wet = c.in.at("wet") == "1";
      World w(hex(c.in.at("surf")), 0xc3400000, 0x43400000, false);
      Run run(w);
      bool in_water = wet;
      std::string bits;
      for (Raw k = 0; k < 32; ++k) {
        in_water = run.step(16 + k, 0, hex(c.in.at("s")), {0, 0, 0}, in_water, false);
        bits += in_water ? '1' : '0';
      }
      std::size_t disturbs = 0, touched = 0;
      for (const auto &e : run.events)
        if (e.rfind("disturb", 0) == 0) {
          ++disturbs;
          touched += e.substr(e.rfind('=') + 1) != "0";
        }
      const std::string what = "seq surf=" + c.in.at("surf") + " s=" + c.in.at("s") + " wet=" + c.in.at("wet");
      check(bits == c.out.at("bits"), what + " bits");
      check(std::to_string(run.draws) == c.out.at("draws"), what + " draws");
      check(std::to_string(disturbs) == c.out.at("disturbs"), what + " disturbs");
      check(std::to_string(touched) == c.out.at("touched"), what + " touched");
      check(h8(run.seed) == c.out.at("seed"), what + " seed");
      check(w.cells() == c.out.at("grid"), what + " grid");
      ++counts[2];
    } else if (c.tag == "foley") {
      const bool item = c.in.at("item") == "1", wet = c.in.at("wet") == "1";
      Raw cosmetic = hex(c.in.at("seed"));
      const auto flags = PW::footstep_flags(item, hex(c.in.at("flags")));
      const auto choice = PW::foley(flags, wet, [&] { return PW::cosmetic_draw(cosmetic); });
      std::string name = "-";
      if (choice.audible) {
        if (choice.format == PW::FOLEY_WATER_FORMAT) name = "W";
        else if (choice.format == PW::FOLEY_PLAIN_FORMAT) name = "P";
        else if (choice.format == 0x53dde8) name = "F400000_";
        else if (choice.format == 0x53de00) name = "F200000_";
        else if (choice.format == 0x53de18) name = "F100000_";
        name += std::to_string(choice.n);
      }
      const std::string what = "foley item=" + c.in.at("item") + " flags=" + c.in.at("flags") + " wet=" +
                               c.in.at("wet") + " seed=" + c.in.at("seed");
      check(name == c.out.at("name"), what + " name " + name + " vs " + c.out.at("name"));
      check((c.out.at("result") == "audible") == choice.audible, what + " audible");
      check(h8(cosmetic) == c.out.at("cosmetic"), what + " cosmetic seed");
      ++counts[3];
    } else {
      check(false, "unknown case " + c.tag);
    }
  }
  // The constants are exercised by the replay above (every ripple, splash and
  // hurt argument is compared with the research model's value).
  // x - 0.0 is computed as written: -0.0 stays -0.0, no frame changes it.
  check(PW::ripple_point({0x80000000, 0, 0})[0] == 0x80000000, "x - 0.0 keeps -0.0");
  // A missing callback refuses; it never reads as dry.
  bool refused = false;
  try {
    PW::Input in;
    PW::step(in, PW::Host{});
  } catch (const std::bad_function_call &) {
    refused = true;
  }
  check(refused, "missing water_test refuses");
  std::cout << "player-wading: " << checks << " checks, " << failures << " failures (band " << counts[0] << ", step "
            << counts[1] << ", seq " << counts[2] << ", foley " << counts[3] << ")\n";
  return failures ? 1 : 0;
}
