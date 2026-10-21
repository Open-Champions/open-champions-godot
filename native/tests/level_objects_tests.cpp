// Built level creatures against the research reference composition
// (tools/generate_level_objects_vectors.py, research t3/research-script-session
// 6841b8a): each authored record is adopted by script_commands::Host with an
// authored executable, and the object's bytes and the shared seed must be what
// the pinned reference constructors produced. Plus the host rules around them:
// the factory decides what is registered, built creatures are found by name and
// readable by hideMonster, their hit points are not.
#include "champions/objects.hpp"
#include "champions/script_commands.hpp"
#include <array>
#include <cstring>
#include <tuple>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sc = champions::simulation::rta::script_commands;
namespace rta = champions::simulation::rta;
namespace {
unsigned checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok)
    throw std::runtime_error(what);
}
std::vector<std::string> split(const std::string &line, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else
      cur += c;
  }
  out.push_back(cur);
  return out;
}
std::uint32_t hex(const std::string &s) { return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)); }

struct Image {
  std::map<std::uint32_t, std::uint8_t> bytes;
  std::uint8_t byte(std::uint32_t a) const {
    auto it = bytes.find(a);
    if (it != bytes.end())
      return it->second;
    const auto c = std::int64_t(a) - 0x5628e1;
    if (c >= -128 && c <= 127)
      return std::uint8_t((c >= 'A' && c <= 'Z' ? 1 : 0) | (c >= 'a' && c <= 'z' ? 2 : 0) |
                          ((c == ' ' || (c >= 9 && c <= 13)) ? 8 : 0));
    return 0;
  }
  rta::level_factory::Executable view() const {
    rta::level_factory::Executable exe;
    exe.byte = [this](std::uint32_t a) { return byte(a); };
    exe.string = [this](std::uint32_t a) {
      std::string out;
      while (auto b = byte(a + static_cast<std::uint32_t>(out.size())))
        out += static_cast<char>(b);
      return out;
    };
    return exe;
  }
};

// One objects.ob blob (research docs/objects.md, level-object records).
std::vector<std::uint8_t> blob(const std::vector<std::tuple<std::string, std::array<std::uint32_t, 3>,
                                                            std::uint16_t, std::vector<std::string>>> &records) {
  std::vector<std::uint8_t> strtab{0};
  auto intern = [&](const std::string &s) {
    const auto at = static_cast<std::uint32_t>(strtab.size());
    strtab.insert(strtab.end(), s.begin(), s.end());
    strtab.push_back(0);
    return at;
  };
  std::vector<std::uint8_t> body;
  auto put = [&](std::vector<std::uint8_t> &v, std::uint64_t value, int n) {
    for (int k = 0; k < n; ++k)
      v.push_back(static_cast<std::uint8_t>(value >> (8 * k)));
  };
  for (const auto &r : records) {
    const auto &props = std::get<3>(r);
    const auto size = 20 + 4 * (props.size() + 1);
    put(body, intern(std::get<0>(r)), 4);
    put(body, size, 2);
    put(body, std::get<2>(r), 2);
    for (auto w : std::get<1>(r))
      put(body, w, 4);
    for (const auto &p : props)
      put(body, intern(p), 4);
    put(body, 0, 4);
  }
  std::vector<std::uint8_t> out;
  put(out, records.size(), 2);
  put(out, 0, 2);
  put(out, 8 + body.size(), 4);
  out.insert(out.end(), body.begin(), body.end());
  out.insert(out.end(), strtab.begin(), strtab.end());
  return out;
}

std::string hexs(std::uint32_t v) {
  std::ostringstream out;
  out << std::hex << v;
  return out.str();
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("usage: level_objects_tests VECTORS");
    std::ifstream in(argv[1]);
    if (!in)
      throw std::runtime_error("vectors are not readable");
    Image image;
    std::uint32_t seed0 = 0;
    std::vector<std::vector<std::string>> cases;
    for (std::string line; std::getline(in, line);) {
      if (line.empty() || line[0] == '#')
        continue;
      const auto f = split(line, '\t');
      if (f[0] == "B")
        image.bytes[hex(f[1])] = static_cast<std::uint8_t>(hex(f[2]));
      else if (f[0] == "SEED")
        seed0 = static_cast<std::uint32_t>(std::stoul(f[1]));
      else if (f[0] == "R")
        cases.push_back(f);
    }
    check(!cases.empty() && seed0, "vectors read");
    rta::SharedRandom seed(seed0);
    unsigned built = 0, stubs = 0;
    for (const auto &c : cases) {
      const auto pos_words = split(c[2], ',');
      const std::array<std::uint32_t, 3> pos{hex(pos_words[0]), hex(pos_words[1]), hex(pos_words[2])};
      const auto facing = static_cast<std::uint16_t>(hex(c[3]));
      std::vector<std::string> props;
      if (!c[4].empty())
        props = split(c[4], '|');
      const auto bytes = blob({{c[1], pos, facing, props}});
      sc::Host host;
      host.bind_seed(&seed);
      host.set_executable(image.view());
      check(host.adopt_objects(bytes.data(), bytes.size()), "adopt " + c[1]);
      // The pool's first block of this size class is where the host put it.
      rta::Pool pool;
      const auto &counts = host.counts();
      const std::string where = "record " + c[1] + " " + c[4];
      check(counts.objects_registered == 1, where + " registered");
      if (c[5] == "built") {
        // Size, then the non-zero bytes as offset:value.
        const auto semi = c[6].find(';');
        const auto n = hex(c[6].substr(0, semi));
        std::map<std::uint32_t, std::uint8_t> nonzero;
        if (semi + 1 < c[6].size())
          for (const auto &pair : split(c[6].substr(semi + 1), ',')) {
            const auto colon = pair.find(':');
            nonzero[hex(pair.substr(0, colon))] = static_cast<std::uint8_t>(hex(pair.substr(colon + 1)));
          }
        const auto address = pool.allocate(n);
        check(host.is_built(address) && counts.objects_built == 1, where + " built");
        for (std::uint32_t i = 0; i < n; ++i) {
          const auto found = nonzero.find(i);
          const std::uint8_t want = found == nonzero.end() ? 0 : found->second;
          const auto got = host.variables().memory.u8(address + i);
          if (got != want)
            throw std::runtime_error(where + " byte +0x" + hexs(i) + " " + std::to_string(got) +
                                     " != " + std::to_string(want));
        }
        check(seed.state == hex(c[7]), where + " shared seed");
        ++built;
      } else {
        check(c[5] == "stub" && counts.objects_unbuilt == 1 && counts.objects_built == 0,
              where + " stub");
        check(counts.build_refusals.count(c[6]) == 1, where + " refusal " + c[6]);
        ++stubs;
      }
      check(counts.load_draws_unmodelled == (c[5] == "built" ? 0u : 1u), where + " draws counted");
      for (const auto &p : props)
        if (p.compare(0, 5, "name=") == 0) {
          const auto name = rta::objects::strip_quotes(p.substr(5));
          check(!host.variables().find(name, 0x200).empty(), where + " found by name");
        }
    }
    check(built > 200 && stubs > 0, "case coverage");
    // Authored level table and balance row. Expected patterns come from the
    // executed ref_combat.monster_stats contract on seed 20973, level 2 and
    // five stored percentages of 100 (which look up as 0x3f7fffff each).
    Image stats_image = image;
    auto put = [&](std::uint32_t at, std::uint32_t value, unsigned n) {
      for (unsigned k = 0; k < n; ++k)
        stats_image.bytes[at + k] = static_cast<std::uint8_t>(value >> (8 * k));
    };
    constexpr std::uint32_t NAME = 0x7f0000;
    const std::string level_name = "testlevel";
    for (std::size_t i = 0; i < level_name.size(); ++i)
      put(NAME + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(level_name[i]), 1);
    put(NAME + static_cast<std::uint32_t>(level_name.size()), 0, 1);
    put(0x4f7210, NAME, 4);
    put(0x4f7214, 0, 1); // signed level byte 0 -> monster level 2
    put(0x4f7240, 0, 4); // default/terminator row
    const std::string skelight_class = "Skelight";
    for (std::size_t i = 0; i < skelight_class.size(); ++i)
      put(0x532d58 + static_cast<std::uint32_t>(i),
          static_cast<std::uint8_t>(skelight_class[i]), 1);
    put(0x532d58 + static_cast<std::uint32_t>(skelight_class.size()), 0, 1);
    for (const auto &entry : std::array<std::pair<std::uint32_t, std::string>, 3>{{
             {0x532cc8, "RiderDisease"}, {0x532dc8, "UndeadKnight1"},
             {0x532e80, "raloszek"}}}) {
      for (std::size_t i = 0; i < entry.second.size(); ++i)
        put(entry.first + static_cast<std::uint32_t>(i),
            static_cast<std::uint8_t>(entry.second[i]), 1);
      put(entry.first + static_cast<std::uint32_t>(entry.second.size()), 0, 1);
    }
    const std::vector<std::uint8_t> balance{1, 0, 0x1c, 0xd6,
                                            100, 0, 100, 0, 100, 0, 100, 0, 100, 0};
    rta::SharedRandom stat_seed(20973);
    sc::Host stat_host;
    stat_host.bind_seed(&stat_seed);
    stat_host.start_single_player();
    stat_host.set_executable(stats_image.view());
    stat_host.set_level_stats(level_name, balance.data(), balance.size());
    const auto gnaw = blob({{"Gnaw", {0, 0, 0}, 0, {"name=stat-gnaw"}}});
    check(stat_host.adopt_objects(gnaw.data(), gnaw.size()), "stat creature adopted");
    rta::Pool stat_pool;
    const auto stat_at = stat_pool.allocate(0x3c8);
    check(stat_host.is_built(stat_at), "stat creature built");
    const auto &stat_mem = stat_host.variables().memory;
    check(stat_mem.u32(stat_at + 0x228) == 0x41edee1au &&
          stat_mem.u32(stat_at + 0x22c) == 0x41edee1au, "exact hit points");
    check(stat_mem.u16(stat_at + 0xe4) == 27 &&
          stat_mem.u16(stat_at + 0xd0) == 3 && stat_mem.u16(stat_at + 0xd2) == 6 &&
          stat_mem.u16(stat_at + 0xd4) == 27 &&
          stat_mem.u32(stat_at + 0xd8) == 0x41a00000u, "exact combat statistics");
    rta::SharedRandom skel_seed(20973);
    sc::Host skel_host;
    skel_host.bind_seed(&skel_seed);
    skel_host.start_single_player();
    skel_host.set_executable(stats_image.view());
    skel_host.set_level_stats(level_name, balance.data(), balance.size());
    const auto skel = blob({{"Skelight", {0, 0, 0}, 0, {"name=stat-skelight"}}});
    check(skel_host.adopt_objects(skel.data(), skel.size()), "Skelight adopted");
    rta::Pool skel_pool;
    const auto skel_at = skel_pool.allocate(0x468);
    check(skel_host.is_built(skel_at),
          "Skelight built (built=" + std::to_string(skel_host.counts().objects_built) +
          ", unbuilt=" + std::to_string(skel_host.counts().objects_unbuilt) +
          ", absent=" + std::to_string(skel_host.counts().objects_not_created) + ")");
    const auto &skel_mem = skel_host.variables().memory;
    check(skel_mem.u32(skel_at + 0x228) == 0x41a8355bu &&
          skel_mem.u32(skel_at + 0x22c) == 0x41a8355bu,
          "Skelight's level override reaches hit points");
    check(skel_mem.u32(skel_at + 0x460) == 1u &&
          skel_mem.u8(skel_at + 0x3c8 + 8) == 4u &&
          (skel_mem.u8(skel_at + 0x3c8 + 4) & 1u) == 1u,
          "Skelight's level and embedded weapon holder");
    for (const auto &[cls, size, should_build] :
         std::array<std::tuple<std::string, std::uint32_t, bool>, 3>{{
             {"UndeadKnight1", 0x478, true}, {"raloszek", 0x480, true},
             {"RiderDisease", 0x478, false}}}) {
      rta::SharedRandom class_seed(20973);
      sc::Host class_host;
      class_host.bind_seed(&class_seed);
      class_host.start_single_player();
      class_host.set_executable(stats_image.view());
      class_host.set_level_stats(level_name, balance.data(), balance.size());
      const auto record = blob({{cls, {0, 0, 0}, 0, {}}});
      check(class_host.adopt_objects(record.data(), record.size()), cls + " adopted");
      rta::Pool class_pool;
      check(class_host.is_built(class_pool.allocate(size)) == should_build,
            cls + (should_build ? " built" : " refused"));
      if (!should_build)
        check(class_host.counts().build_refusals.count(
                  "mounted Horsemen needs Nightmare_ctor") == 1,
              "mounted constructor refusal is explicit");
    }
    std::cout << "PASS level-objects: " << checks << " checks, " << built << " built, " << stubs
              << " stubs\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL level-objects: " << e.what() << '\n';
    return 1;
  }
}
