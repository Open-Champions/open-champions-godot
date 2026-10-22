// net_launch against tools/ref_items_gaps.py (t3/research-items-gaps ecf9742),
// by the vectors tools/generate_net_launch_vectors.py wrote, plus the contract's
// own statements (docs/spec/items-gaps.md section 2).
#include "champions/net_launch.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace nl = champions::simulation::rta::net_launch;
namespace {
unsigned checks = 0, failed = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failed;
    std::cerr << "FAIL " << what << '\n';
  }
}
// "name a b c | d e | f" -> groups of hex numbers.
std::vector<std::vector<std::uint64_t>> groups(const std::string &line, std::string &name) {
  std::vector<std::vector<std::uint64_t>> out(1);
  std::istringstream in(line);
  std::string word;
  in >> name;
  while (in >> word) {
    if (word == "|")
      out.emplace_back();
    else
      out.back().push_back(std::stoull(word, nullptr, 16));
  }
  return out;
}
nl::Settings settings_of(const std::vector<std::uint64_t> &v) {
  nl::Settings s;
  s.flags = static_cast<std::uint8_t>(v[0]);
  s.difficulty = static_cast<std::uint8_t>(v[1]);
  s.mode = static_cast<std::uint8_t>(v[2]);
  s.mission = static_cast<std::uint8_t>(v[3]);
  s.kill_limit = static_cast<std::uint8_t>(v[4]);
  s.count = static_cast<std::int8_t>(v[5]);
  s.own = static_cast<std::int8_t>(v[6]);
  s.byte_0a = static_cast<std::uint8_t>(v[7]);
  return s;
}
std::vector<std::uint64_t> bytes_of(const nl::Settings &s) {
  return {s.flags, s.difficulty, s.mode, s.mission, s.kill_limit,
          static_cast<std::uint8_t>(s.count), static_cast<std::uint8_t>(s.own), s.byte_0a};
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: net-launch-tests VECTORS\n";
    return 2;
  }
  std::ifstream in(argv[1]);
  check(static_cast<bool>(in), "vectors open");
  std::string line, name;
  std::size_t lobby = 0, menu = 0, kind = 0, own = 0, fill = 0, receive = 0, compose = 0, hosts = 0, calls = 0;
  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    const auto g = groups(line, name);
    if (name == "lobby") {
      auto s = settings_of(g[0]);
      nl::Room room{static_cast<std::uint8_t>(g[1][1]), static_cast<std::uint8_t>(g[1][2]),
                    static_cast<std::uint8_t>(g[1][3]), static_cast<std::uint8_t>(g[1][4])};
      nl::lobby_set_host(s, static_cast<std::int8_t>(g[1][0]), room, static_cast<std::uint8_t>(g[1][5]));
      check(bytes_of(s) == g[2], "lobby_task: " + line);
      hosts += (s.flags & nl::FLAG_HOST) ? 1 : 0;
      ++lobby;
    } else if (name == "menu") {
      nl::Settings s;
      s.flags = 0xff; // the model overwrites flags, difficulty and mode on every path
      nl::frontend_set_host(s, g[0][0] != 0);
      check(s.flags == g[1][0] && s.difficulty == g[1][1] && s.mode == g[1][2], "frontend host: " + line);
      ++menu;
    } else if (name == "kind") {
      nl::Settings s;
      s.flags = static_cast<std::uint8_t>(g[0][0]);
      s.byte_0a = static_cast<std::uint8_t>(g[0][2]);
      nl::frontend_set_kind(s, static_cast<std::uint8_t>(g[0][1]));
      check(s.flags == g[1][0] && s.byte_0a == g[1][1], "frontend kind: " + line);
      ++kind;
    } else if (name == "own") {
      check(nl::own_record(static_cast<std::int8_t>(g[0][0])) == g[1][0], "own record: " + line);
      ++own;
    } else if (name == "fill") {
      nl::Settings s;
      s.flags = static_cast<std::uint8_t>(g[0][0]);
      s.difficulty = static_cast<std::uint8_t>(g[0][1]);
      s.mode = static_cast<std::uint8_t>(g[0][2]);
      s.mission = static_cast<std::uint8_t>(g[0][3]);
      s.kill_limit = static_cast<std::uint8_t>(g[0][4]);
      nl::Record r;
      r.difficulty = static_cast<std::uint8_t>(g[1][0]);
      r.mode = static_cast<std::uint8_t>(g[1][1]);
      r.kill_limit = static_cast<std::uint8_t>(g[1][2]);
      r.mission = static_cast<std::uint8_t>(g[1][3]);
      r.cm1 = static_cast<std::uint32_t>(g[1][4]);
      const auto scan = static_cast<std::int32_t>(g[2][6]);
      unsigned scans = 0;
      const bool called = nl::launch_fill_host_fields(s, r, [&] {
        ++scans;
        return scan;
      });
      check(r.difficulty == g[2][0] && r.mode == g[2][1] && r.kill_limit == g[2][2] && r.mission == g[2][3] &&
                r.cm1 == g[2][4] && (called ? 1u : 0u) == g[2][5] && scans == (called ? 1u : 0u),
            "net_launchPrepare fill: " + line);
      check(called == ((s.flags & nl::FLAG_HOST) != 0), "the scan is called exactly with the host bit");
      calls += called ? 1 : 0;
      ++fill;
    } else if (name == "receive") {
      nl::Record r;
      r.mode = static_cast<std::uint8_t>(g[0][0]);
      r.difficulty = static_cast<std::uint8_t>(g[0][1]);
      r.kill_limit = static_cast<std::uint8_t>(g[0][2]);
      r.cm1 = static_cast<std::uint32_t>(g[0][3]);
      const auto got = nl::launch_receive(r);
      check(got.difficulty == g[1][0] && got.kill_limit == g[1][1] && got.pvp_mode_a == (g[1][2] != 0) &&
                got.pvp_mode_b == (g[1][3] != 0) && got.mode_3 == (g[1][4] != 0) &&
                got.set_cm1.has_value() == (g[1][5] != 0) &&
                (!got.set_cm1 || static_cast<std::uint32_t>(*got.set_cm1) == g[1][6]),
            "net_launchUpdate: " + line);
      ++receive;
    } else if (name == "compose") {
      const auto mode = static_cast<std::uint8_t>(g[0][0]);
      check(static_cast<std::uint32_t>(nl::cm1_word_after_launch(4, 9, mode)) == g[1][0] &&
                static_cast<std::uint32_t>(nl::cm1_word_after_launch(-1, 3, mode)) == g[1][1],
            "cm1 word after launch: " + line);
      ++compose;
    } else {
      check(false, "unknown vector kind " + name);
    }
  }
  check(lobby == 300 && menu == 100 && kind == 100 && own == 256 && fill == 300 && receive == 300 && compose == 256,
        "vector counts");
  check(hosts > 30 && calls > 100, "both host and non-host cases are covered");
  // The contract's statements.
  nl::Settings s;
  nl::lobby_set_host(s, 0, {3, 2, 7, 4}, 0);
  check(s.flags == (nl::FLAG_HOST | nl::FLAG_KIND_A) && s.mission == 5, "player 0 is the host and takes the room");
  nl::lobby_set_host(s, 2, {3, 2, 7, 4}, 1);
  check(s.flags == nl::FLAG_KIND_B, "another number is not the host");
  check(nl::own_record(0) == 0x402b90 && nl::own_record(-1) == 0x402b90 - 0x144, "record addresses");
  nl::Record client;
  client.cm1 = 0x55555555;
  check(!nl::launch_fill_host_fields({}, client, [] { return 3; }) && client.cm1 == 0x55555555,
        "a client writes nothing and does not scan");
  check(nl::cm1_word_after_launch(4, 9, 2) == 4 && nl::cm1_word_after_launch(4, 9, 1) == 9,
        "only modes 2 and 3 take the host's word");
  if (failed) {
    std::cerr << failed << " of " << checks << " checks failed\n";
    return 1;
  }
  std::cout << "net-launch ok (" << checks << " checks)\n";
  return 0;
}
