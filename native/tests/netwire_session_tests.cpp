#include "champions/netwire.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
using namespace champions::simulation::rta::netwire;
namespace {
std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> out;
  std::size_t p = 0;
  for (;;) {
    auto q = s.find(d, p);
    out.push_back(s.substr(p, q == s.npos ? q : q - p));
    if (q == s.npos)
      return out;
    p = q + 1;
  }
}
Integer num(const std::string &s) {
  std::size_t p;
  auto n = std::stoll(s, &p);
  if (p != s.size())
    throw std::runtime_error("bad integer");
  return n;
}
unsigned digit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  throw std::runtime_error("bad hex");
}
ByteVector unhex(const std::string &s) {
  if (s.size() % 2)
    throw std::runtime_error("odd hex");
  ByteVector b;
  for (std::size_t i = 0; i < s.size(); i += 2)
    b.push_back(std::uint8_t(digit(s[i]) * 16 + digit(s[i + 1])));
  return b;
}
template <class C> std::string hex(const C &b) {
  std::string s;
  const char *d = "0123456789abcdef";
  for (auto v : b) {
    s += d[(unsigned(v) >> 4) & 15];
    s += d[unsigned(v) & 15];
  }
  return s;
}
std::string enc(Integer x) { return std::to_string(x); }
std::string enc(const ByteVector &b) { return hex(b); }
std::string enc(const WireRecord &b) { return hex(b); }
std::string pack(const std::vector<std::string> &xs) {
  std::string s = "[";
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (i)
      s += ',';
    s += xs[i];
  }
  return s + "]";
}
std::string enc(const FrameSymbols &);
template <class T> std::string enc(const std::vector<T> &v);
template <class T, std::size_t N> std::string enc(const std::array<T, N> &v);
template <class A, class B> std::string enc(const std::pair<A, B> &v);
template <class T> std::string enc(const std::optional<T> &v);
template <class T> std::string enc(const std::vector<T> &v) {
  std::vector<std::string> x;
  for (auto &a : v)
    x.push_back(enc(a));
  return pack(x);
}
template <class T, std::size_t N> std::string enc(const std::array<T, N> &v) {
  std::vector<std::string> x;
  for (auto &a : v)
    x.push_back(enc(a));
  return pack(x);
}
template <class A, class B> std::string enc(const std::pair<A, B> &v) {
  return pack({enc(v.first), enc(v.second)});
}
template <class T> std::string enc(const std::optional<T> &v) {
  return v ? enc(*v) : "n";
}
std::string enc(const Sent &s) {
  return pack(
      {enc(s.peer), hex(s.ip), enc(s.port), hex(s.data), enc(s.undefined)});
}
std::string enc(const FrameSymbols &s) {
  std::vector<std::string> yaw;
  for (auto &y : s.yaw)
    yaw.push_back(pack({enc(Integer(y.kind)), enc(y.value)}));
  return pack({enc(s.count), enc(s.buttons), enc(s.sticks), pack(yaw),
               enc(s.flags), enc(s.bits)});
}
std::string enc(const Packet &p) {
  return pack({enc(p.kind), enc(p.last), enc(p.slot), enc(p.mask), enc(p.byte9),
               enc(p.count), enc(p.ack_delta), enc(p.first()), enc(p.ack()),
               enc(p.sim_frame()), enc(p.symbols), enc(p.raw_records),
               enc(p.stream_bytes), enc(p.sync_offset), enc(p.sync),
               enc(p.sync_word), enc(p.blocks), enc(p.blocks_offset)});
}
const std::map<std::string, Integer Machine::*> global = {
#define M(n) {#n, &Machine::n}
    M(network_mode),    M(num_peers),    M(lif),        M(last_sent),
    M(net_frame),       M(cycle),        M(phase),      M(modem),
    M(last_hash_frame), M(stall),        M(ended),      M(eq),
    M(min_ready),       M(win),          M(long_stall), M(kicked),
    M(kick_state),      M(msg_attached), M(sim_frame),  M(rand_seed),
    M(sync_acc),        M(num_players)
#undef M
};
const std::vector<std::string> global_order = split(
    "network_mode num_peers lif last_sent net_frame cycle phase modem "
    "last_hash_frame stall ended eq min_ready win long_stall kicked kick_state "
    "msg_attached sim_frame rand_seed sync_acc level_pending num_players",
    ' ');
const std::map<std::string, Integer Peer::*> pfields = {
#define P(n) {#n, &Peer::n}
    P(port),         P(send_frame), P(msg_write), P(msg_read), P(newest),
    P(acked),        P(lead),       P(sim),       P(silence),  P(heard),
    P(local),        P(dropped),    P(kick),      P(oos),      P(drop_frame),
    P(kicked_notice)
#undef P
};
const std::vector<std::string> peer_order =
    split("port send_frame msg_write msg_read newest acked lead sim silence "
          "heard local dropped kick oos drop_frame kicked_notice",
          ' ');
Peer &at(Machine &m, Integer i) { return m.peers[python_index(i, 4)]; }
std::string snapshot(const Machine &m, const std::vector<std::string> &events) {
  std::vector<std::string> fields;
  for (auto &k : global_order)
    fields.push_back(
        enc(k == "level_pending" ? Integer(m.level_pending) : m.*global.at(k)));
  std::vector<std::string> peers, sent, logs;
  for (auto &p : m.peers) {
    std::vector<std::string> v;
    for (auto &k : peer_order)
      v.push_back(enc(p.*pfields.at(k)));
    v.push_back(enc(p.pad_prev));
    peers.push_back(pack(v));
  }
  for (auto &s : m.outbox)
    sent.push_back(enc(s));
  for (auto &s : m.log)
    logs.push_back(hex(s));
  std::string result =
      pack({pack(fields), enc(m.kick_acks), enc(m.cam_yaw), enc(m.input_lock),
            enc(m.send_cycles), enc(m.positions),
            pack({enc(m.pad_globals.start_block), enc(m.pad_globals.mute),
                  enc(m.pad_globals.allow)}),
            pack(peers), pack(sent), pack(logs), pack(events)});
  for (auto &p : m.peers)
    result += ':' + hex(p.pack());
  result += ':' + hex(m.sim_enc.pack());
  result += ':' + hex(m.sim_dec.pack());
  for (auto &p : m.pads)
    result += ':' + hex(p);
  return result;
}
std::pair<std::string, std::string> run(const std::string &script,
                                        std::set<std::string> &coverage) {
  std::unique_ptr<Machine> ptr;
  std::vector<std::string> outcomes, events;
  for (auto &cmd : split(script, ';')) {
    auto a = split(cmd, ',');
    auto &op = a[0];
    coverage.insert(op);
    auto n = [&](std::size_t i) { return num(a.at(i)); };
    auto b = [&](std::size_t i) { return unhex(a.at(i)); };
    try {
      if (op == "init") {
        auto models =
            std::map<std::string, Fpu::Model>{{"ps2", Fpu::Model::Ps2},
                                              {"ps2g1", Fpu::Model::Ps2g1},
                                              {"chop", Fpu::Model::Chop},
                                              {"nearest", Fpu::Model::Nearest}};
        ptr = std::make_unique<Machine>(
            n(1), n(2), Fpu(models.at(a[3]), n(4) != 0, n(5) != 0));
        continue;
      }
      if (!ptr)
        throw std::runtime_error("missing init");
      auto &m = *ptr;
      if (op == "m") {
        if (a[1] == "level_pending")
          m.level_pending = n(2) != 0;
        else
          m.*global.at(a[1]) = n(2);
        continue;
      }
      if (op == "p") {
        at(m, n(1)).*pfields.at(a[2]) = n(3);
        continue;
      }
      if (op == "fill") {
        auto &p = at(m, n(1));
        for (Integer i = 0; i < n(3); ++i) {
          auto frame = n(2) + i;
          WireRecord r{};
          for (Integer j = 0; j < 12; ++j)
            r[std::size_t(j)] = std::uint8_t(n(4) + frame * 17 + j * 31);
          p.set_record(frame, r, 0, 0);
        }
        continue;
      }
      if (op == "sync") {
        at(m, n(1)).set_sync(n(2), n(3), n(4));
        continue;
      }
      if (op == "record") {
        auto bytes = b(3);
        if (bytes.size() != 12)
          throw std::runtime_error("fixture record");
        WireRecord r{};
        std::copy(bytes.begin(), bytes.end(), r.begin());
        at(m, n(1)).set_record(n(2), r,
                               a[4] == "n" ? std::optional<Integer>{} : n(4),
                               a[5] == "n" ? std::optional<Integer>{} : n(5));
        continue;
      }
      if (op == "ring") {
        auto &p = at(m, n(1));
        auto bytes = b(3);
        for (std::size_t i = 0; i < bytes.size(); ++i)
          p.msg_ring[(std::uint64_t(n(2)) + i) & 8191] = bytes[i];
        continue;
      }
      if (op == "peerimage") {
        at(m, n(1)) = Peer::unpack(b(2));
        continue;
      }
      if (op == "hook") {
        auto index = n(1), mode = n(2);
        m.direct_handlers[python_index(index, 4)] =
            [&, index, mode](const ByteVector &payload) {
              events.push_back(pack({enc(index), hex(payload)}));
              ++m.cycle;
              if (mode == 1) {
                m.peers[1].dropped = 1;
                m.num_peers = 0;
                m.direct_handlers[python_index(index, 4)] = {};
              }
              if (mode == 2)
                throw ValueError("authored callback");
              if (mode == 3)
                m.queue_message(index, payload);
            };
        continue;
      }
      if (op == "stack") {
        auto mode = n(1);
        m.stack_bytes = [&, mode](Integer slot, Integer frame) {
          events.push_back(pack({enc(slot), enc(frame)}));
          ++m.phase;
          if (mode == 2 && m.phase == 2)
            throw ValueError("authored pressure callback");
          if (mode == 3)
            m.stack_bytes = {};
          return std::array<std::uint8_t, 2>{std::uint8_t(slot),
                                             std::uint8_t(frame)};
        };
        continue;
      }
      std::string result = "n";
      if (op == "header")
        result = enc(header(n(1), n(2), n(3), n(4), n(5), n(6), n(7)));
      else if (op == "parse")
        result = enc(parse_packet(b(1)));
      else if (op == "kick_request")
        result = enc(kick_request(n(1), n(2)));
      else if (op == "notice")
        result = enc(kicked_notice());
      else if (op == "lan")
        result = enc(lan_packet(n(1) != 0));
      else if (op == "direct")
        result = enc(direct_message(n(1), b(2)));
      else if (op == "queries")
        result = pack({enc(m.local_slot()), enc(m.active_mask()),
                       enc(m.remote_active()), enc(m.is_first_active())});
      else if (op == "pack")
        result = enc(at(m, n(1)).pack());
      else if (op == "unpack")
        result = enc(Peer::unpack(b(1)).pack());
      else if (op == "getrecord") {
        auto r = at(m, n(1)).record(n(2));
        result = pack({enc(std::get<0>(r)), enc(std::get<1>(r)),
                       enc(std::get<2>(r)), enc(std::get<3>(r))});
      } else if (op == "getsync")
        result = enc(at(m, n(1)).sync(n(2)));
      else if (op == "queue")
        m.queue_message(n(1), b(2));
      else if (op == "attach")
        result = enc(m.attach_messages());
      else if (op == "messages")
        result = enc(m.frame_messages(n(1), n(2)));
      else if (op == "build")
        result = enc(m.build_packet(n(1), n(2), n(3), n(4), n(5),
                                    n(6) ? nullptr : &at(m, n(1)).send_state,
                                    n(6) != 0));
      else if (op == "send")
        result = enc(m.send_input(n(1)));
      else if (op == "sendmutable") {
        auto payload = b(2);
        m.direct_handlers[0] = [&](const ByteVector &arg) {
          events.push_back(pack({hex(arg)}));
          payload.clear();
          events.push_back(pack({hex(arg)}));
        };
        m.send_direct(0, n(1), payload);
        m.direct_handlers[0] = {};
      } else if (op == "receive_mutate") {
        auto payload = b(1);
        m.stack_bytes = [&](Integer slot, Integer frame) {
          events.push_back(pack({enc(slot), enc(frame)}));
          payload.clear();
          return std::array<std::uint8_t, 2>{17, 23};
        };
        result = enc(m.receive(payload));
        m.stack_bytes = {};
      } else if (op == "senddirect")
        m.send_direct(n(1), n(2), b(3));
      else if (op == "kick")
        m.kick_peer(n(1));
      else if (op == "receive")
        result = enc(m.receive(b(2), n(1) != 0));
      else if (op == "dispatch")
        result = enc(m.dispatch(b(1)));
      else
        throw std::runtime_error("unknown operation");
      outcomes.push_back(result);
    } catch (const MalformedPacket &) {
      outcomes.push_back("!MalformedPacket");
    } catch (const IndexError &) {
      outcomes.push_back("!IndexError");
    } catch (const ValueError &) {
      outcomes.push_back("!ValueError");
    }
  }
  std::string out;
  for (auto &s : outcomes) {
    if (!out.empty())
      out += ':';
    out += s;
  }
  if (!ptr)
    throw std::runtime_error("no machine initialized");
  return {out, snapshot(*ptr, events)};
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error("usage vectors [actual]");
    if (header(0xbd3f, 0x12345678, 2, 5, 7, 3, -1) !=
        unhex("3fbd000078563412520703ff"))
      throw std::runtime_error("header anchor");
    Peer anchor;
    anchor.set_record(257, rec12(), 0x12345678, 0x87654321);
    auto packed = anchor.pack();
    if (packed.size() != PEER_SIZE || packed[RING_OFFSET + 24] != 1 ||
        packed[RING_OFFSET + 25] != 1 || packed[RING_OFFSET + 40] != 0x78 ||
        packed[RING_OFFSET + 44] != 0x21)
      throw std::runtime_error("peer offsets anchor");
    Machine attachment;
    attachment.lif = 4;
    attachment.queue_message(5, {0xaa, 0xbb});
    if (attachment.attach_messages() != 2 ||
        attachment.peers[0].msg_count(2) != 4 ||
        attachment.frame_messages(0, 2).first[0].second !=
            ByteVector({0xaa, 0xbb}))
      throw std::runtime_error("message anchor");
    Machine bounded;
    bounded.net_frame = 2;
    bounded.peers[0].set_record(0, rec12(), 0, Integer(MAX_BYTES));
    bool exhausted = false;
    try {
      bounded.build_packet(1, 0, 0, PKT_INPUT, 0, &bounded.peers[1].send_state);
    } catch (const ResourceLimit &) {
      exhausted = true;
    }
    if (!exhausted || bounded.last_hash_frame != 2 || !bounded.outbox.empty())
      throw std::runtime_error("retained budget state anchor");
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
      throw std::runtime_error("missing vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw std::runtime_error("cannot write trace");
    }
    std::string line;
    std::size_t count = 0;
    std::uint64_t fnv = 14695981039346656037ull;
    std::set<std::string> coverage;
    while (std::getline(input, line)) {
      ++count;
      if (input.eof())
        throw std::runtime_error("unterminated fixture row");
      auto a = split(line, '\t');
      if (a.size() != 5 || a[0] != "netwire-session")
        throw std::runtime_error("malformed vector");
      auto sep = a[2].find('|');
      if (sep == a[2].npos || num(a[2].substr(0, sep)) != Integer(count - 1))
        throw std::runtime_error("sequence mismatch");
      auto result = run(a[2].substr(sep + 1), coverage);
      if (result.first != a[3] || result.second != a[4]) {
        std::ofstream debug("work/netwire-session/mismatch.txt");
        debug << "RESULT\n"
              << result.first << "\nEXPECTED\n"
              << a[3] << "\nSTATE\n"
              << result.second << "\nEXPECTED\n"
              << a[4] << '\n';
        throw std::runtime_error("mismatch row " + std::to_string(count) + " " +
                                 a[1]);
      }
      auto actual = a[0] + '\t' + a[1] + '\t' + a[2] + '\t' + result.first +
                    '\t' + result.second + '\n';
      for (unsigned char c : actual)
        fnv = (fnv ^ c) * 1099511628211ull;
      if (trace)
        trace << actual;
    }
    const std::set<std::string> required = {
        "attach", "build",      "direct",       "dispatch",
        "fill",   "getrecord",  "getsync",      "header",
        "hook",   "kick",       "kick_request", "lan",
        "m",      "messages",   "notice",       "p",
        "pack",   "parse",      "peerimage",    "queries",
        "queue",  "receive",    "record",       "ring",
        "send",   "senddirect", "sendmutable",  "receive_mutate",
        "stack",  "sync",       "unpack"};
    for (auto &op : required)
      if (!coverage.count(op))
        throw std::runtime_error("missing operation coverage");
    if (!input.eof())
      throw std::runtime_error("read failure");
    if (count != 538 || fnv != 0xb85f4f53379dedf6ull)
      throw std::runtime_error("incomplete or altered corpus");
    if (argc == 3) {
      trace.flush();
      if (!trace)
        throw std::runtime_error("trace failure");
    }
    std::cout << count << " exact session vectors; FNV1a64 " << std::hex << fnv
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
