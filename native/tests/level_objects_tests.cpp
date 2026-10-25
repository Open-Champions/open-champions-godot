// Built level creatures against the research reference composition
// (tools/generate_level_objects_vectors.py, research t3/research-script-session
// 6841b8a): each authored record is adopted by script_commands::Host with an
// authored executable, and the object's bytes and the shared seed must be what
// the pinned reference constructors produced. Plus the host rules around them:
// the factory decides what is registered, built creatures are found by name and
// readable by hideMonster; hit points need the selected level's balance data.
#include "champions/objects.hpp"
#include "champions/script_commands.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/session_combat_runtime_adapter.hpp"
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
// Authored WORLD payload: the decode-only layout of terrain_file_tests (one
// chunk, two elements, one heightfield). Only its decode is relied on here.
std::vector<std::uint8_t> world_payload() {
  std::vector<std::uint8_t> b(0x220, 0);
  auto put = [&](std::size_t offset, std::uint64_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i)
      b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
  };
  put(0, 1); put(0x10, 1); put(0x14, 1); put(0x18, 0x170); put(0x1c, 2);
  put(0x20, 0xc0); put(0x24, 0x84); put(0x28, 0xfffffffe); put(0x2c, 3);
  put(0x30, 2); put(0x34, 1); put(0x38, 0x1e0); put(0x50, 2); put(0x54, 0x1a0);
  put(0x64, 0x1f0); put(0x68, 0x44340000); put(0x78, 0x180); put(0x84, 0x210);
  for (unsigned i = 0; i < 2; ++i) {
    const auto e = 0xc0 + i * 0x24;
    put(e, 0xfff8, 2); put(e + 2, 0xfff8, 2); put(e + 4, 8, 2); put(e + 6, 8, 2);
    put(e + 8, 0xdeadbeef); put(e + 12, 0x110);
    put(e + 16, i ? 0x12340004 : 0xffff0006);
    put(e + 28, i ? 9 : 5); put(e + 32, 1234, 2);
    put(e + 34, static_cast<std::uint16_t>(-2345), 2);
    const auto q = 0x1a0 + i * 0x1c;
    put(q + 4, 1, 2); put(q + 24, i, 2);
  }
  put(0x118, 2); put(0x11c, 2); put(0x120, static_cast<std::uint16_t>(-3), 2);
  put(0x122, 7, 2);
  for (unsigned i = 0; i < 4; ++i)
    put(0x124 + 2 * i, static_cast<std::uint16_t>(std::array<int, 4>{-31, 32, -16, 7}[i]), 2);
  put(0x180, 1, 2); put(0x182, 0, 2); put(0x184, 1, 2); put(0x186, 0xffff, 2);
  put(0x1e0, 0x12345678); put(0x1e4, 0xffffffff);
  return b;
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
    bool shared_authority_checked = false;
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
        if (!shared_authority_checked) {
          namespace combat = rta::combat_runtime;
          auto authority = std::make_shared<rta::WorldAuthority>();
          combat::World combat_world(authority, combat::Services{});
          combat::SessionAdapter adapter(combat_world);
          combat::Address handle = 0, address = 0;
          {
            sc::Host factory(authority);
            rta::SharedRandom factory_seed(seed0);
            factory.bind_seed(&factory_seed);
            factory.set_executable(image.view());
            check(factory.adopt_objects(bytes.data(), bytes.size()), "shared factory adoption");
            check(factory.counts().objects_built == 1, "ported factory constructor");
            check(authority->objects->objects.size() == 1, "one shared object graph entry");
            address = authority->objects->objects.begin()->first;
            const auto free_nodes = authority->objects->handles.count_free();
            handle = combat_world.retain(address);
            check(authority->objects->handles.count_free() < free_nodes,
                  "combat retain uses the host's handle pool");
            check(combat_world.target(handle) == address &&
                      adapter.factory_object(handle) == authority->objects->objects.at(address),
                  "factory object reachable by the combat adapter handle");
            check(combat_world.read(address + 0x44) == factory.variables().memory.word(address + 0x44),
                  "factory bytes visible in combat memory");
            combat_world.write(address + 0x34, 4, 0x1234);
            check(factory.variables().memory.word(address + 0x34) == 0x1234,
                  "combat write visible in script memory");
          }
          check(!combat_world.target(handle) && !adapter.factory_object(handle) &&
                    !authority->find(address), "host removal invalidates shared handle");
          authority->objects->pool.free(address); // Force the same pool slot's next generation.
          {
            sc::Host replacement(authority);
            rta::SharedRandom replacement_seed(seed0);
            replacement.bind_seed(&replacement_seed);
            replacement.set_executable(image.view());
            check(replacement.adopt_objects(bytes.data(), bytes.size()), "replacement adoption");
            check(authority->objects->objects.count(address) == 1,
                  "factory allocation reuses the shared pool address");
            check(!combat_world.target(handle) && !adapter.factory_object(handle),
                  "retired handle does not alias replacement generation");
            // The factory identity guard: a retained handle whose node is still
            // linked, but whose address is now registered to a different Object.
            const auto live_handle = combat_world.retain(address);
            check(adapter.factory_object(live_handle) == authority->objects->objects.at(address),
                  "fresh handle resolves the replacement factory object");
            auto *original = authority->find(address);
            authority->unregister_object(*original);
            rta::Object stand_in;
            stand_in.id = address;
            authority->register_object(stand_in);
            authority->register_factory(address);
            check(combat_world.target(live_handle) == address &&
                      !adapter.factory_object(live_handle),
                  "factory_object rejects a registry object that is not the graph object");
            authority->unregister_object(stand_in);
            authority->register_object(*original);
            authority->register_factory(address);
            check(adapter.factory_object(live_handle) == authority->objects->objects.at(address),
                  "factory identity restored");
            // Script writes to bytes the combat overlay covers are not refused
            // (script memory keeps its original store semantics).
            bool refused = false;
            try {
              replacement.variables().memory.write(address, {0x5a});
              replacement.variables().memory.write(address + 0x34, {0x12, 0x34});
            } catch (const std::exception &) {
              refused = true;
            }
            check(!refused && replacement.variables().memory.u8(address + 0x34) == 0x12,
                  "script write to a combat-overlay byte is not refused");
          }
          {
            // A combat actor is freed, then a factory object reuses its pool
            // block: a retained handle must link to the object's own head.
            auto reuse_authority = std::make_shared<rta::WorldAuthority>();
            combat::World reuse_world(reuse_authority, combat::Services{});
            const auto actor = reuse_world.spawn_skelight();
            reuse_world.destroy(actor);
            auto reuse = std::make_unique<sc::Host>(reuse_authority);
            rta::SharedRandom reuse_seed(seed0);
            reuse->bind_seed(&reuse_seed);
            reuse->set_executable(image.view());
            check(reuse->adopt_objects(bytes.data(), bytes.size()), "reuse adoption");
            check(reuse_authority->objects->objects.size() == 1, "one reused object graph entry");
            const auto reused = reuse_authority->objects->objects.begin()->first;
            check(reused == actor, "factory object reuses the freed combat actor's block");
            const auto reuse_handle = reuse_world.retain(reused);
            check(reuse_world.target(reuse_handle) == reused &&
                      reuse_world.factory_object(reuse_handle) ==
                          reuse_authority->objects->objects.at(reused),
                  "handle on a reused block links the factory object's own head");
            // Host teardown must unlink the node from the factory object's own
            // head; a node left in the stale combat head would still resolve
            // (audit WA-F2).
            reuse.reset();
            check(reuse_world.target(reuse_handle) == 0 && !reuse_world.factory_object(reuse_handle),
                  "host teardown invalidates the retained handle");
          }
          shared_authority_checked = true;
        }
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
    check(built > 200 && stubs > 0 && shared_authority_checked, "case coverage");
    rta::SharedRandom gate_seed(20973), expected_gate_seed(20973);
    sc::Host gate_host;
    gate_host.bind_seed(&gate_seed);
    gate_host.advance_first_level_items("intro", false);
    gate_host.advance_first_level_items("testlevel", true);
    gate_host.advance_first_level_items("testlevel");
    check(gate_seed.state == expected_gate_seed.state &&
          gate_host.counts().item_load_gate_refusals == 1 &&
          gate_host.counts().item_load_draws_inferred == 223,
          "unknown HUD residency refuses inferred item draws");
    gate_host.advance_first_level_items("", false);
    check(gate_seed.state == expected_gate_seed.state &&
          gate_host.counts().item_load_gate_refusals == 2 &&
          gate_host.counts().item_load_draws_inferred == 446,
          "missing world identity also refuses inferred item draws");
    gate_host.advance_first_level_items("testlevel", false);
    for (unsigned i = 0; i < 223; ++i) expected_gate_seed.draw();
    check(gate_seed.state == expected_gate_seed.state,
          "observed absent HUD applies 223 item draws");
    // Authored level table and balance row. Expected patterns are reproduced
    // by research tools/check_creature_stats_vector.py (commit a684d7d):
    // ref_combat.monster_stats on shared seed 20973, five factors of
    // fmul(itof(100), 0x3c23d70a), Gnaw at level 2 and Skelight's own factors
    // at its supplied level 1. Run: python3 tools/check_creature_stats_vector.py
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
    // Authored class labels occupy the executed factory's test addresses.
    for (const auto &entry : std::array<std::pair<std::uint32_t, std::string>, 3>{{
             {0x532cc8, "MountedVariant"}, {0x532dc8, "FootVariant"},
             {0x532e80, "ZekVariant"}}}) {
      for (std::size_t i = 0; i < entry.second.size(); ++i)
        put(entry.first + static_cast<std::uint32_t>(i),
            static_cast<std::uint8_t>(entry.second[i]), 1);
      put(entry.first + static_cast<std::uint32_t>(entry.second.size()), 0, 1);
    }
    // Row hash 0xd61c for "Gnaw": the formula of research data-tables.md:231
    // (executed against balance_hashName, difftest_balance_lookup.py), recomputed
    // independently with that tool's hash_name. No research-authored vector
    // for a game name exists.
    const std::vector<std::uint8_t> balance{1, 0, 0x1c, 0xd6,
                                            100, 0, 100, 0, 100, 0, 100, 0, 100, 0};
    {
      sc::Host long_name_host;
      long_name_host.set_executable(stats_image.view());
      bool refused = false;
      try {
        long_name_host.set_level_stats(level_name + "-outside-table", balance.data(), balance.size());
      } catch (const std::invalid_argument &) { refused = true; }
      check(refused, "external level name cannot overrun known table-name domain");
      refused = false;
      try {
        long_name_host.set_level_stats("other", balance.data(), balance.size());
      } catch (const std::invalid_argument &) { refused = true; }
      check(refused, "an unlisted short name also refuses unknown global capacity");
    }
    {
      // The original stores only the base name: a long sub-name is not a
      // domain violation, and the statistics equal the base-name request's.
      rta::SharedRandom base_seed(20973), sub_seed(20973);
      sc::Host base_host, sub_host;
      base_host.bind_seed(&base_seed);
      sub_host.bind_seed(&sub_seed);
      for (auto *h : {&base_host, &sub_host}) {
        h->start_single_player();
        h->set_executable(stats_image.view());
      }
      base_host.set_level_stats(level_name, balance.data(), balance.size());
      sub_host.set_level_stats(level_name + "_a-sub-name-longer-than-any-table-name",
                               balance.data(), balance.size());
      const auto one = blob({{"Gnaw", {0, 0, 0}, 0, {"name=sub-gnaw"}}});
      check(base_host.adopt_objects(one.data(), one.size()) &&
            sub_host.adopt_objects(one.data(), one.size()), "sub-name records adopt");
      rta::Pool pool;
      const auto at = pool.allocate(0x3c8);
      check(sub_host.is_built(at) && base_host.is_built(at) &&
            sub_host.variables().memory.u32(at + 0x228) ==
                base_host.variables().memory.u32(at + 0x228) &&
            sub_seed.state == base_seed.state, "sub-name request uses the base name");
    }
    {
      // CS4-1: the front end loads no monsters.bal, so hit points stay refused.
      for (const bool stats_first : {true, false}) {
        rta::SharedRandom intro_seed(20973);
        sc::Host intro_host;
        intro_host.bind_seed(&intro_seed);
        intro_host.start_single_player();
        intro_host.set_executable(stats_image.view());
        if (stats_first) intro_host.set_level_stats("intro", balance.data(), balance.size());
        intro_host.advance_first_level_items("intro", false);
        if (!stats_first) intro_host.set_level_stats("intro", balance.data(), balance.size());
        const auto g = blob({{"Gnaw", {0, 0, 0}, 0, {"name=intro-gnaw"}}});
        check(intro_host.adopt_objects(g.data(), g.size()), "intro creature adopted");
        rta::Pool intro_pool;
        const auto at = intro_pool.allocate(0x3c8);
        check(intro_host.is_built(at), "intro creature built");
        check(intro_host.counts().build_refusals.count("Front-end level has no balance table") == 1,
              "intro refusal counted");
        check(intro_host.variables().memory.u32(at + 0x228) != 0x41edee1au,
              "intro leaves no balance-scaled hit points");
      }
    }
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
    {
      rta::SharedRandom unsupported_seed(20973);
      sc::Host unsupported_host;
      unsupported_host.bind_seed(&unsupported_seed);
      unsupported_host.start_single_player();
      unsupported_host.set_executable(stats_image.view());
      unsupported_host.set_level_stats(level_name, balance.data(), balance.size());
      unsupported_host.variables().memory.word(0x4d8ae0, 2);
      check(unsupported_host.adopt_objects(gnaw.data(), gnaw.size()) &&
            unsupported_host.counts().objects_built == 0 &&
            unsupported_host.counts().build_refusals.count(
                "stats require one player at difficulty 1") == 1,
            "multiplayer creature statistics are explicitly refused");
    }
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
    check(!skel_host.is_built(skel_at) && skel_host.counts().objects_unbuilt == 1 &&
          skel_host.counts().build_refusals.count("Holder.v1 table slot 1 is untraced") == 1,
          "Skelight refuses before publishing holder-dependent bytes or statistics");
    {
      // A fully built creature still copies its traced collision radius.
      const auto gnaw_found = stat_host.variables().find("stat-gnaw", 0x200);
      check(gnaw_found.size() == 1 &&
            gnaw_found[0]->radius == stat_mem.u32(stat_at + 0x1c),
            "Gnaw host radius follows its constructor");
    }
    {
      // Player1_ctor 0x158928 placement (research ref_objects.py
      // player1_marker). The gate, WORLD and caller-supplied level-entry
      // inputs are all required; a refusal leaves the rank and spawn slots.
      Image p1_image = stats_image;
      const std::string p1_class = "Player1";
      for (std::size_t i = 0; i <= p1_class.size(); ++i)
        p1_image.bytes[0x532998 + static_cast<std::uint32_t>(i)] =
            i < p1_class.size() ? static_cast<std::uint8_t>(p1_class[i]) : 0;
      const auto record = blob({{p1_class, {0, 0, 0}, 0x1234, {"name=start", "from=prior"}}});
      const auto world = world_payload();
      struct Setup {
        rta::SharedRandom seed{20973};
        sc::Host host;
      };
      auto make = [&](bool intro, bool with_world) {
        auto setup = std::make_unique<Setup>();
        setup->host.bind_seed(&setup->seed);
        setup->host.start_single_player();
        setup->host.set_executable(p1_image.view());
        if (intro) setup->host.advance_first_level_items("intro", false);
        if (with_world) setup->host.set_level_world(world.data(), world.size());
        return setup;
      };
      rta::SharedRandom untouched(20973);
      auto refused = [&](Setup &s, const char *reason) {
        check(s.host.adopt_objects(record.data(), record.size()), "Player1 adopted");
        check(s.host.counts().build_refusals.count(reason) == 1 &&
              s.host.counts().objects_built == 0 && s.seed.state == untouched.state &&
              s.host.variables().world->start_rank == 0,
              std::string("Player1 refusal: ") + reason);
      };
      auto no_gate = make(false, true);
      no_gate->host.set_player1_entry({"prior", "start", false, false});
      refused(*no_gate, "Player1 placement needs faithful seed");
      auto no_world = make(true, false);
      no_world->host.set_player1_entry({"prior", "start", false, false});
      refused(*no_world, "Player1 needs session seed and WORLD");
      auto no_entry = make(true, true);
      refused(*no_entry, "Player1 needs caller-supplied level entry");
      // The start rank after a save load is 3: the marker only draws once.
      auto saved = make(true, true);
      saved->host.set_player1_entry({"prior", "start", false, true});
      check(saved->host.adopt_objects(record.data(), record.size()), "saved Player1 adopted");
      rta::SharedRandom one_draw(20973);
      one_draw.draw();
      check(saved->host.counts().objects_built == 1 && saved->seed.state == one_draw.state &&
            saved->host.variables().world->start_rank == 3 &&
            saved->host.variables().world->spawn_pos[0] == rta::objects::Vec3{} &&
            saved->host.counts().load_draws_unmodelled == 0,
            "rank 3 Player1 takes no spawn slots and draws once");
      // Smoke coverage only: the pinned marker difftest establishes the
      // algorithm, but this authored WORLD fixture has no independent exact
      // position/facing/seed vector. Do not claim exact placement from it.
      // A taken marker fills all four slots and the rank.
      auto taken = make(true, true);
      taken->host.set_player1_entry({"prior", "start", false, false});
      check(taken->host.adopt_objects(record.data(), record.size()), "taken Player1 adopted");
      {
        const auto &state = *taken->host.variables().world;
        const auto &counts = taken->host.counts();
        check(counts.objects_built == 1 && counts.build_refusals.empty() &&
              state.start_rank == 3 && taken->seed.state != one_draw.state,
              "taken Player1 ranks by name equal to the entrance and searches");
        for (unsigned slot = 0; slot < 4; ++slot)
          check(state.spawn_facing[slot] != 0, "taken Player1 fills every facing slot");
      }
    }
    for (const auto &[cls, size, reason] :
         std::array<std::tuple<std::string, std::uint32_t, std::string>, 3>{{
             {"FootVariant", 0x478, ""},
             {"ZekVariant", 0x480, "Holder.v1 table slot 1 is untraced"},
             {"MountedVariant", 0x478, ""}}}) {
      rta::SharedRandom class_seed(20973);
      sc::Host class_host;
      class_host.bind_seed(&class_seed);
      class_host.start_single_player();
      class_host.set_executable(stats_image.view());
      class_host.set_level_stats(level_name, balance.data(), balance.size());
      const auto record = blob({{cls, {0, 0, 0}, 0, {}}});
      check(class_host.adopt_objects(record.data(), record.size()), cls + " adopted");
      rta::Pool class_pool;
      const auto built = class_host.is_built(class_pool.allocate(size));
      if (cls == "MountedVariant" || cls == "FootVariant")
        check(built && class_host.counts().build_refusals.empty(),
              cls + " builds through its audited Horseman constructor");
      else
        check(!built && class_host.counts().build_refusals.count(reason) == 1,
              cls + " has a counted constructor refusal");
    }
    std::cout << "PASS level-objects: " << checks << " checks, " << built << " built, " << stubs
              << " stubs\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL level-objects: " << e.what() << '\n';
    return 1;
  }
}
