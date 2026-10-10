#include "champions/netwire.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
using namespace champions::simulation::rta::netwire;
namespace {
std::string seq(const std::vector<std::string> &v) {
  std::string s = "[";
  for (const auto &x : v) {
    if (s.size() > 1)
      s += ',';
    s += x;
  }
  return s + "]";
}
std::string n(Integer v) { return std::to_string(v); }
template <class C> std::string bytes(const C &v) {
  const char *d = "0123456789abcdef";
  std::string s = "b";
  for (auto x : v) {
    s += d[std::uint8_t(x) >> 4];
    s += d[std::uint8_t(x) & 15];
  }
  return s;
}
std::string text(const std::string &s) { return "s" + bytes(s).substr(1); }
template <class C> std::string ints(const C &v) {
  std::vector<std::string> x;
  for (auto a : v)
    x.push_back(n(a));
  return seq(x);
}
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::size_t p = 0;
  for (;;) {
    auto end = s.find(sep, p);
    out.push_back(s.substr(p, end == s.npos ? end : end - p));
    if (end == s.npos)
      return out;
    p = end + 1;
  }
}
Integer integer(const std::string &s) {
  std::size_t end;
  auto v = std::stoll(s, &end);
  if (end != s.size())
    throw std::runtime_error("bad integer");
  return v;
}
ByteVector unhex(const std::string &s) {
  if (s.size() % 2)
    throw std::runtime_error("odd hex");
  auto digit = [](char c) -> unsigned {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    throw std::runtime_error("bad hex");
  };
  ByteVector out;
  for (std::size_t i = 0; i < s.size(); i += 2)
    out.push_back(digit(s[i]) * 16 + digit(s[i + 1]));
  return out;
}
Fpu fpu(Integer config) {
  if (config < 0 || config > 15)
    throw std::runtime_error("bad config");
  const Fpu::Model models[] = {Fpu::Model::Ps2, Fpu::Model::Ps2g1,
                               Fpu::Model::Chop, Fpu::Model::Nearest};
  return Fpu(models[config / 4], config & 2, config & 1);
}
std::string sent(const Sent &s) {
  std::vector<std::string> u;
  for (auto x : s.undefined)
    u.push_back(n(Integer(x)));
  return seq({n(s.peer), bytes(s.ip), n(s.port), bytes(s.data), seq(u)});
}
std::string sends(const std::vector<Sent> &s) {
  std::vector<std::string> v;
  for (const auto &x : s)
    v.push_back(sent(x));
  return seq(v);
}
std::string state(const Machine &m, const std::vector<std::string> &events) {
  std::vector<std::string> peers, locks, positions, pads, logs, handlers,
      direct;
  for (const auto &p : m.peers)
    peers.push_back(
        seq({bytes(p.pack()),
             ints(std::vector<Integer>{
                 p.port, p.send_frame, p.msg_write, p.msg_read, p.newest,
                 p.acked, p.lead, p.sim, p.silence, p.heard, p.local, p.dropped,
                 p.kick, p.oos, p.drop_frame, p.kicked_notice}),
             ints(p.pad_prev)}));
  for (const auto &x : m.input_lock)
    locks.push_back(ints(x));
  for (const auto &p : m.positions)
    positions.push_back(p ? ints(*p) : "_");
  for (const auto &p : m.pads)
    pads.push_back(bytes(p));
  for (const auto &x : m.log)
    logs.push_back(text(x));
  for (const auto &x : m.handlers)
    handlers.push_back(n(x.first));
  for (const auto &x : m.direct_handlers)
    direct.push_back(n(bool(x)));
  return seq(
      {ints(std::vector<Integer>{
           m.network_mode,    m.num_peers,     m.lif,        m.last_sent,
           m.net_frame,       m.cycle,         m.phase,      m.modem,
           m.last_hash_frame, m.stall,         m.ended,      m.eq,
           m.min_ready,       m.win,           m.long_stall, m.kicked,
           m.kick_state,      m.msg_attached,  m.sim_frame,  m.rand_seed,
           m.sync_acc,        m.level_pending, m.num_players}),
       seq(peers),
       ints(m.kick_acks),
       ints(m.cam_yaw),
       seq(locks),
       ints(m.send_cycles),
       bytes(m.sim_enc.pack()),
       bytes(m.sim_dec.pack()),
       seq(positions),
       seq(pads),
       ints(std::vector<Integer>{m.pad_globals.start_block, m.pad_globals.mute,
                                 m.pad_globals.allow}),
       sends(m.outbox),
       seq(logs),
       seq(events),
       seq(handlers),
       seq(direct),
       n(bool(m.run_tasks)),
       n(bool(m.on_kill)),
       n(bool(m.first_active_hook)),
       n(bool(m.sample_garbage)),
       n(bool(m.stack_bytes))});
}
std::pair<std::string, std::string> machine_case(Integer config,
                                                 Integer scenario) {
  Machine m((scenario == 3 || scenario == 6 || scenario == 10 ||
             scenario == 15 || scenario == 16 || scenario == 17)
                ? 4
                : 2,
            0, fpu(config));
  std::vector<std::string> events, results;
  std::vector<ByteVector> *active_inbox = nullptr;
  for (Integer i = 0; i < 4; ++i) {
    auto &p = m.peers[std::size_t(i)];
    p.ip = {10, 0, 0, std::uint8_t(i + 1)};
    p.port = 1000 + i;
    p.unknown8.fill(std::uint8_t(i + 31));
    p.lead = 2;
    p.sim = 1;
    p.newest = 20;
    for (Integer frame = 1; frame < 21; ++frame)
      p.set_record(frame,
                   rec12((frame * 257 + i * 19) & 65535,
                         {(frame * 13 + i) & 255, (frame * 29 + 64) & 255,
                          (i * 57 + 127) & 255, (frame * 3) & 255},
                         {frame & 255, (i * 63) & 255}, frame * 313 + i,
                         frame & 1),
                   0, 0);
    m.positions[std::size_t(i)] =
        Position{std::uint32_t(0x3f800000 + i * 0x10000),
                 std::uint32_t(0xc0200000 + i), i == 3 ? 0x7f800000u : 0u};
    m.pads[std::size_t(i)].fill(std::uint8_t(0xa0 + i));
  }
  m.run_tasks = [&](Machine &x) {
    events.push_back(seq({text("tasks"), n(x.net_frame), n(x.num_peers)}));
    x.rand_seed =
        std::uint32_t(std::uint64_t(x.rand_seed) * 1664525 + 1013904223);
    x.sync_acc = std::uint32_t(x.sync_acc + 17);
    ++x.sim_frame;
    if (scenario == 6) {
      x.level_pending = true;
      x.run_tasks = [&](Machine &again) {
        events.push_back(seq({text("replacement_tasks"), n(again.net_frame)}));
      };
    }
    if (scenario == 12)
      throw ValueError("authored task exception");
  };
  m.on_kill = [&](Integer i) {
    events.push_back(seq({text("kill"), n(i)}));
    m.sync_acc += 7;
  };
  m.first_active_hook = [&](Integer i) {
    events.push_back(seq({text("first"), n(i)}));
    m.queue_message(4, {std::uint8_t(i), 0x55});
  };
  m.sample_garbage = [](Integer frame) {
    return std::uint32_t(std::uint64_t(frame) * 0x1020304);
  };
  m.handlers[7] = [&](const ByteVector &p) {
    events.push_back(
        seq({text("message"), bytes(p), n(m.net_frame), n(m.num_peers)}));
    m.sync_acc += Integer(p.size());
    if (scenario == 6)
      m.handlers[7] = [&](const ByteVector &q) {
        events.push_back(seq({text("replacement_message"), bytes(q)}));
      };
    if (scenario == 15)
      m.num_peers = 1;
  };
  m.direct_handlers[0] = [&](const ByteVector &p) {
    events.push_back(seq({text("direct"), bytes(p)}));
    m.input_lock[0] = {2, 0xffff, 1};
    if (scenario == 22 && p == ByteVector{'a', 'p', 'p', 'e', 'n', 'd'}) {
      active_inbox->clear();
      active_inbox->push_back(lan_packet());
      active_inbox->push_back(
          direct_message(0, {'s', 'e', 'c', 'o', 'n', 'd'}));
    }
  };
  for (auto &p : m.peers) {
    ByteVector msg = {7, 2, 0x41, 0x42, 7, 1, 0x43};
    std::copy(msg.begin(), msg.end(), p.msg_ring.begin());
    p.set_record(1, p.wire_record(1), 0, Integer(msg.size()));
    p.msg_write = Integer(msg.size());
  }
  m.msg_attached = 7;
  if (scenario == 1) {
    m.modem = 1;
    m.input_lock[0] = {3, 0x1234, 1};
  }
  if (scenario == 2) {
    m.peers[1].silence = 901;
    m.peers[1].newest = 0;
  }
  if (scenario == 3) {
    m.peers[1].kick = 2;
    m.peers[1].drop_frame = 1;
    m.peers[2].kicked_notice = 120;
  }
  if (scenario == 4)
    for (std::size_t i = 1; i < 4; ++i)
      for (Integer frame = 2; frame < 8; ++frame)
        m.peers[i].set_sync(frame, 99, 0xdeadbeef);
  if (scenario == 5)
    m.level_pending = true;
  if (scenario == 7) {
    m.lif = 10;
    m.eq = 30;
    m.win = 61;
    m.min_ready = 4;
    m.peers[1].lead = 0;
  }
  if (scenario == 8) {
    m.stall = 301;
    m.long_stall = 9;
  }
  if (scenario == 9)
    for (auto &p : m.peers)
      p.local = 0;
  if (scenario == 10) {
    m.network_mode = 0;
    m.pad_globals = {3, 1, 0x401};
  }
  if (scenario == 11)
    m.peers[1].set_record(1, m.peers[1].wire_record(1), 0, 1);
  if (scenario == 14)
    m.peers[1].kicked_notice = 10;
  if (scenario == 16) {
    m.kick_state = 1;
    m.peers[1].kick = 1;
    m.peers[1].drop_frame = -1;
  }
  if (scenario == 17) {
    m.kick_state = 1;
    m.peers[1].kick = 1;
    m.peers[1].newest = 1;
    m.kick_acks.fill(1);
  }
  if (scenario == 18) {
    m.stall = 301;
    m.long_stall = 9;
    m.ended = 1;
  }
  if (scenario == 20) {
    m.send_cycles = {-7, -3, -5, -9};
    m.modem = 1;
  }
  if (scenario == 21) {
    m.net_frame = 0xffffffff;
    m.num_players = -1;
    for (auto &p : m.peers) {
      p.set_record(0xffffffff, rec12(1), 0, 0);
      p.set_record(0x100000000LL, rec12(2), 0, 0);
      p.set_sync(0, 9, 0x1234);
    }
  }
  if (scenario == 24) {
    m.num_peers = 5;
    for (auto &p : m.peers)
      p.kicked_notice = 10;
  }
  if (scenario == 23)
    m.num_peers = 5;
  if (scenario == 4 || scenario == 5 || scenario == 6 || scenario == 11 ||
      scenario == 12 || scenario == 15 || scenario == 21 || scenario == 23) {
    try {
      m.tick(scenario == 21 ? 2 : 3);
      results.push_back("_");
    } catch (const ValueError &) {
      results.push_back(text("!ValueError"));
    } catch (const MalformedPacket &) {
      results.push_back(text("!MalformedPacket"));
    } catch (const IndexError &) {
      results.push_back(text("!IndexError"));
    }
  } else
    for (Integer call = 0; call < (scenario == 24 ? 1 : 6); ++call) {
      auto raw = rec12(0xf835 ^ (call * 17), {0, 255, 127 + call, 40 + call},
                       {33, 220});
      ByteVector sample(raw.begin(), raw.begin() + 8);
      if (scenario == 14)
        sample = {'1', '2', '3', '4', '5', '6', '7'};
      std::vector<ByteVector> inbox;
      active_inbox = &inbox;
      if (scenario == 22 && call == 0)
        inbox.push_back(direct_message(0, {'a', 'p', 'p', 'e', 'n', 'd'}));
      if (scenario == 13 && call == 0)
        inbox.push_back(direct_message(0, {'l', 'o', 'c', 'k'}));
      try {
        auto result = m.net_main(
            sample, -32768 + call * 511, call & 1,
            scenario == 19 ? (call < 3 ? 0 : -1) : (scenario == 1 ? 2 : 1),
            scenario == 8 || scenario == 18 || (scenario == 1 && call == 2),
            scenario == 1 && call == 3, call * 30, inbox);
        results.push_back(
            seq({sends(result.sent), n(result.held), n(result.steps)}));
      } catch (const ValueError &) {
        results.push_back(text("!ValueError"));
      } catch (const MalformedPacket &) {
        results.push_back(text("!MalformedPacket"));
      } catch (const IndexError &) {
        results.push_back(text("!IndexError"));
      }
    }
  return {seq(results), state(m, events)};
}
std::string parsed(const std::optional<LaunchPacket> &p) {
  return p ? seq({n(p->player_id), n(p->kind), n(p->argument),
                  bytes(p->payload)})
           : "_";
}
std::pair<std::string, std::string> run(const std::string &op,
                                        const std::string &args) {
  auto a = split(args, ',');
  auto at = [&](std::size_t i) { return integer(a.at(i)); };
  if (op == "stick")
    return {ints(decode_stick(at(1), at(2), fpu(at(0)))), "_"};
  if (op == "pad") {
    Pad pad;
    pad.fill(std::uint8_t(at(1) + 64));
    std::array<Integer, 2> prev{at(2), at(3)};
    PadGlobals g{at(4), at(5), at(6)};
    auto record = unhex(a.at(8));
    auto f = fpu(at(0));
    for (Integer i = 0; i < at(7); ++i)
      pad_apply(pad, record, prev, &g, f);
    return {bytes(pad),
            seq({ints(prev),
                 ints(std::vector<Integer>{g.start_block, g.mute, g.allow})})};
  }
  if (op == "machine")
    return machine_case(at(0), at(1));
  if (op == "hash") {
    auto fields = split(args, ';'), head = split(fields.at(0), ',');
    std::vector<std::optional<Position>> p;
    for (std::size_t i = 1; i < fields.size(); ++i) {
      if (fields[i].empty())
        continue;
      if (fields[i] == "_")
        p.push_back({});
      else {
        auto x = split(fields[i], ',');
        p.push_back(Position{std::uint32_t(integer(x.at(0))),
                             std::uint32_t(integer(x.at(1))),
                             std::uint32_t(integer(x.at(2)))});
      }
    }
    return {n(sync_hash(integer(head.at(0)), integer(head.at(1)), p)), "_"};
  }
  if (op == "launch") {
    auto payload = unhex(a.at(4));
    try {
      auto packet =
          launch_packet(at(0), at(1), at(2), payload,
                        at(3) == -999 ? std::optional<Integer>{}
                                      : std::optional<Integer>{at(3)});
      auto data = launch_data_packet(at(0), at(2), payload);
      return {
          seq({bytes(packet), n(launch_checksum(payload)), bytes(data.first),
               n(data.second), parsed(parse_launch_packet(packet)),
               n(launch_data_ok(data.second, payload)),
               n(launch_data_ok(data.second ^ 0x10000, payload))}),
          "_"};
    } catch (const ValueError &) {
      return {"!ValueError", "_"};
    }
  }
  if (op == "parse")
    return {parsed(parse_launch_packet(unhex(args))), "_"};
  throw std::runtime_error("unknown operation");
}
void anchors() {
  Pad pad;
  pad.fill(0xa5);
  auto before = pad;
  std::array<Integer, 2> prev{};
  bool failed = false;
  try {
    pad_apply(pad, ByteVector(7), prev);
  } catch (const ValueError &) {
    failed = true;
  }
  if (!failed || pad != before)
    throw std::runtime_error("short pad atomicity");
  if (launch_checksum({}) != 0x217 || parse_launch_packet(ByteVector(15)))
    throw std::runtime_error("launch boundaries");
  Machine m(1);
  m.tick(-1);
  if (m.net_frame != 1)
    throw std::runtime_error("negative tick no-op");
  failed = false;
  try {
    m.tick(MAX_WORK + 1);
  } catch (const ResourceLimit &) {
    failed = true;
  }
  if (!failed)
    throw std::runtime_error("tick budget");
  PadGlobals globals{std::numeric_limits<Integer>::min(), 0, 0};
  failed = false;
  try {
    pad_apply(pad, rec12(0x800), prev, &globals);
  } catch (const ResourceLimit &) {
    failed = true;
  }
  if (!failed || pad != before)
    throw std::runtime_error("start-block overflow guard");
  Machine overflow(2);
  overflow.peers[1].silence = std::numeric_limits<Integer>::max();
  failed = false;
  try {
    overflow.net_main(ByteVector(8));
  } catch (const ResourceLimit &) {
    failed = true;
  }
  if (!failed)
    throw std::runtime_error("silence overflow guard");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error("usage: netwire-tick-tests TSV [TRACE]");
    anchors();
    std::ifstream in(argv[1], std::ios::binary);
    if (!in)
      throw std::runtime_error("fixture open");
    std::ofstream out;
    if (argc == 3) {
      out.open(argv[2], std::ios::binary);
      if (!out)
        throw std::runtime_error("trace open");
    }
    std::string line;
    if (!std::getline(in, line) || line != "# netwire_tick_vectors_v1")
      throw std::runtime_error("fixture schema");
    std::map<std::string, std::size_t> counts;
    std::size_t rows = 0;
    std::uint64_t hash = 14695981039346656037ULL;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      auto c = split(line, '\t');
      if (c.size() != 5 || c[0] != "netwire-tick")
        throw std::runtime_error("malformed fixture");
      auto result = run(c[1], c[2]);
      if (result.first != c[3] || result.second != c[4]) {
        std::cerr << "Mismatch row " << rows + 1 << " " << c[1] << " " << c[2]
                  << "\nExpected result " << c[3] << "\nActual result "
                  << result.first << "\n";
        std::ofstream("work/netwire-tick/expected-state.txt") << c[4];
        std::ofstream("work/netwire-tick/actual-state.txt") << result.second;
        return 1;
      }
      auto actual = c[0] + "\t" + c[1] + "\t" + c[2] + "\t" + result.first +
                    "\t" + result.second + "\n";
      if (out)
        out << actual;
      for (unsigned char ch : actual) {
        hash ^= ch;
        hash *= 1099511628211ULL;
      }
      ++counts[c[1]];
      ++rows;
    }
    if (!in.eof())
      throw std::runtime_error("fixture read failure");
    const std::map<std::string, std::size_t> expected{
        {"stick", 6224}, {"pad", 1024},   {"machine", 400},
        {"hash", 512},   {"launch", 256}, {"parse", 40}};
    if (counts != expected || hash != 0x2e36efc8ec5adb86ULL)
      throw std::runtime_error("incomplete corpus");
    if (out) {
      out.flush();
      if (!out)
        throw std::runtime_error("trace write failure");
    }
    std::cout << rows
              << " exact netwire tick vectors passed; FNV1a64=" << std::hex
              << hash << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
