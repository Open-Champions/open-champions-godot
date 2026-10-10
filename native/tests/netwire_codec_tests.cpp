#include "champions/netwire.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace champions::simulation::rta::netwire;
namespace {
std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> v;
  std::size_t p = 0;
  for (;;) {
    auto q = s.find(d, p);
    v.push_back(s.substr(p, q - p));
    if (q == s.npos)
      return v;
    p = q + 1;
  }
}
std::string n(Integer x) {
  std::ostringstream s;
  s << std::hex << std::setfill('0') << std::setw(16) << std::uint64_t(x);
  return s.str();
}
Integer integer(const std::string &s) {
  if (s.size() != 16 || s.find_first_not_of("0123456789abcdef") != s.npos)
    throw ValueError("numeric syntax");
  auto v = std::stoull(s, nullptr, 16);
  return v <= INT64_MAX ? Integer(v) : -1 - Integer(~v);
}
std::vector<Integer> ints(const std::string &s) {
  std::vector<Integer> v;
  if (s != "-")
    for (auto p : split(s, ','))
      v.push_back(integer(p));
  return v;
}
std::string ns(const std::vector<Integer> &v) {
  std::string out;
  for (auto x : v) {
    if (!out.empty())
      out += ',';
    out += n(x);
  }
  return out.empty() ? "-" : out;
}
std::string raw(const ByteVector &v) {
  if (v.empty())
    return "-";
  std::string s;
  const char *h = "0123456789abcdef";
  for (auto x : v) {
    s += h[x >> 4];
    s += h[x & 15];
  }
  return s;
}
std::string raw(const WireRecord &r) {
  return raw(ByteVector(r.begin(), r.end()));
}
ByteVector unhex(const std::string &s) {
  if (s == "-")
    return {};
  if (s.empty() || s.size() % 2 ||
      s.find_first_not_of("0123456789abcdef") != s.npos)
    throw ValueError("byte syntax");
  ByteVector v;
  for (std::size_t i = 0; i < s.size(); i += 2)
    v.push_back(std::uint8_t(std::stoul(s.substr(i, 2), nullptr, 16)));
  return v;
}
Channel channel(const std::string &s) {
  auto v = ints(s);
  if (v.size() != 4)
    throw ValueError("channel fields");
  return {v[0], v[1], std::uint32_t(v[2]), std::uint32_t(v[3])};
}
std::string ch(const Channel &c) {
  return ns({c.index, c.pred, c.value, c.velocity});
}
CodecState codec(const std::string &s) {
  auto v = split(s, ';');
  if (v.size() != 5)
    throw ValueError("codec fields");
  CodecState c;
  for (std::size_t i = 0; i < 4; ++i)
    c.ch[i] = channel(v[i]);
  auto q = ints(v[4]);
  if (q.size() != 5)
    throw ValueError("codec scalars");
  c.yaw_prev = q[0];
  c.yaw_pred = q[1];
  c.yaw_delta = q[2];
  c.pad46 = q[3];
  c.count = q[4];
  return c;
}
std::string cs(const CodecState &c) {
  std::string s;
  for (const auto &x : c.ch) {
    if (!s.empty())
      s += ';';
    s += ch(x);
  }
  return s + ';' + ns({c.yaw_prev, c.yaw_pred, c.yaw_delta, c.pad46, c.count});
}
AxisSymbols symbols(const std::string &s) {
  AxisSymbols out;
  if (s != "-")
    for (auto part : split(s, ';')) {
      auto v = ints(part);
      if (v.size() != 2)
        throw ValueError("axis symbol");
      out.emplace_back(v[0], v[1]);
    }
  return out;
}
std::string sy(const AxisSymbols &s) {
  std::string out;
  for (auto [m, d] : s) {
    if (!out.empty())
      out += ';';
    out += ns({m, d});
  }
  return out.empty() ? "-" : out;
}
std::string fs(const FrameSymbols &s) {
  std::string out = ns({s.count, s.bits}) + '/' + ns(s.buttons);
  for (const auto &v : s.sticks)
    out += '/' + sy(v);
  AxisSymbols y;
  for (const auto &v : s.yaw)
    y.emplace_back(Integer(v.kind), v.value);
  return out + '/' + sy(y) + '/' + ns(s.flags);
}
FrameSymbols frame_symbols(const std::vector<std::string> &a, std::size_t p) {
  if (a.size() != p + 8)
    throw ValueError("frame symbol shape");
  FrameSymbols s;
  auto v = ints(a[p]);
  if (v.size() != 2)
    throw ValueError("frame count bits");
  s.count = v[0];
  s.bits = v[1];
  s.buttons = ints(a[p + 1]);
  for (std::size_t i = 0; i < 4; ++i)
    s.sticks[i] = symbols(a[p + 2 + i]);
  for (auto [kind, value] : symbols(a[p + 6])) {
    if (kind < 0 || kind > 2)
      throw ValueError("yaw kind");
    s.yaw.push_back({YawSymbol::Kind(kind), value});
  }
  s.flags = ints(a[p + 7]);
  return s;
}
WireRecord record(const std::string &s) {
  auto v = unhex(s);
  if (v.size() != 12)
    throw ValueError("wire length");
  WireRecord r;
  std::copy(v.begin(), v.end(), r.begin());
  return r;
}
std::vector<WireRecord> records(const std::string &s) {
  std::vector<WireRecord> v;
  if (s != "-")
    for (auto p : split(s, ','))
      v.push_back(record(p));
  return v;
}
std::string rs(const std::vector<WireRecord> &v) {
  std::string s;
  for (const auto &r : v) {
    if (!s.empty())
      s += ',';
    s += raw(r);
  }
  return s.empty() ? "-" : s;
}
Fpu fpu(Integer mode) {
  if (mode < 0 || mode > 15)
    throw ValueError("fpu mode");
  return Fpu(Fpu::Model(mode / 4), (mode & 2) != 0, (mode & 1) != 0);
}
std::pair<std::string, std::string> run(const std::string &op,
                                        const std::string &arg) {
  auto a = split(arg, '/');
  const std::map<std::string, std::size_t> shapes = {
      {"write", 3},         {"read", 3},          {"axis_symbols", 2},
      {"axis_read", 2},     {"axis_encode", 2},   {"axis_apply", 3},
      {"axis_decode", 4},   {"frames_encode", 2}, {"frames_read", 2},
      {"frames_apply", 11}, {"frames_decode", 4}, {"local", 4}};
  auto shape = shapes.find(op);
  if (a.size() != (shape == shapes.end() ? 1 : shape->second))
    throw std::runtime_error("operation argument shape");
  std::string result, state = "-";
  std::optional<Channel> c;
  std::optional<CodecState> s, d;
  std::optional<BitReader> r;
  bool reader_state = false, writer_state = false;
  BitWriter w;
  try {
    if (op == "add" || op == "sub" || op == "mul" || op == "div" ||
        op == "lt" || op == "sqrt" || op == "from_int" || op == "to_int") {
      auto v = ints(arg);
      const bool binary = op == "add" || op == "sub" || op == "mul" ||
                          op == "div" || op == "lt";
      if (v.size() != (binary ? 3u : 2u))
        throw std::runtime_error("arithmetic argument shape");
      for (std::size_t i = 1; i < v.size(); ++i)
        if (v[i] < 0 || v[i] > 0xffffffffLL)
          throw std::runtime_error("raw32 argument range");
      auto f = fpu(v.at(0));
      const auto x = std::uint32_t(v.at(1));
      auto y = v.size() > 2 ? std::uint32_t(v.at(2)) : 0;
      Integer out;
      if (op == "add")
        out = f.add(x, y);
      else if (op == "sub")
        out = f.sub(x, y);
      else if (op == "mul")
        out = f.mul(x, y);
      else if (op == "div")
        out = f.div(x, y);
      else if (op == "lt")
        out = f.lt(x, y);
      else if (op == "sqrt")
        out = f.sqrt(x);
      else if (op == "from_int")
        out = f.from_int(x);
      else
        out = f.to_int(x);
      result = n(out);
    } else if (op == "channel_pack") {
      c = channel(arg);
      result = raw(c->pack());
    } else if (op == "channel_unpack")
      result = ch(Channel::unpack(unhex(arg)));
    else if (op == "state_pack") {
      s = codec(arg);
      result = raw(s->pack());
    } else if (op == "state_unpack")
      result = cs(CodecState::unpack(unhex(arg)));
    else if (op == "record") {
      auto v = ints(arg);
      if (v.size() != 9)
        throw ValueError("record fields");
      result =
          raw(rec12(v[0], {v[1], v[2], v[3], v[4]}, {v[5], v[6]}, v[7], v[8]));
    } else if (op == "record_fields") {
      auto f = rec12_fields(unhex(arg));
      result = ns({f.buttons, f.sticks[0], f.sticks[1], f.sticks[2],
                   f.sticks[3], f.pressure[0], f.pressure[1], f.yaw});
    } else if (op == "write") {
      w.pos = integer(a.at(0));
      auto bytes = unhex(a.at(1));
      if (bytes.size() % 2)
        throw ValueError("writer words");
      for (std::size_t i = 0; i < bytes.size(); i += 2)
        w.words.push_back(
            std::uint16_t(bytes[i] | std::uint16_t(bytes[i + 1]) << 8));
      for (auto [value, bits] : symbols(a.at(2)))
        w.put(value, bits);
      result = raw(w.tobytes());
      state = ns({w.pos, Integer(w.written())});
    } else if (op == "read") {
      r.emplace(unhex(a.at(1)), integer(a.at(0)));
      reader_state = true;
      std::vector<Integer> v;
      for (auto bits : ints(a.at(2)))
        v.push_back(r->get(bits));
      result = ns(v);
    } else if (op == "axis_symbols") {
      c = channel(a.at(0));
      result = sy(axis_symbols(*c, ints(a.at(1))));
    } else if (op == "axis_write") {
      write_axis_symbols(w, symbols(arg));
      result = raw(w.tobytes());
      state = n(w.pos);
    } else if (op == "axis_read") {
      r.emplace(unhex(a.at(0)));
      reader_state = true;
      result = sy(read_axis_symbols(*r, integer(a.at(1))));
    } else if (op == "axis_encode") {
      c = channel(a.at(0));
      writer_state = true;
      encode_axis(w, *c, ints(a.at(1)));
      result = raw(w.tobytes());
    } else if (op == "axis_apply") {
      c = channel(a.at(1));
      result =
          raw(apply_axis_symbols(*c, symbols(a.at(2)), fpu(integer(a.at(0)))));
    } else if (op == "axis_decode") {
      c = channel(a.at(1));
      r.emplace(unhex(a.at(2)));
      reader_state = true;
      auto v = ints(a.at(3));
      result =
          raw(decode_axis(*r, *c, v.at(0), v.at(1), fpu(integer(a.at(0)))));
    } else if (op == "frames_encode") {
      s = codec(a.at(0));
      writer_state = true;
      w = encode_frames(*s, records(a.at(1)));
      result = raw(w.tobytes());
    } else if (op == "frames_read")
      result = fs(read_frames(unhex(a.at(0)), integer(a.at(1))));
    else if (op == "frames_apply") {
      s = codec(a.at(1));
      result = rs(apply_frames(*s, frame_symbols(a, 3), integer(a.at(2)),
                               fpu(integer(a.at(0)))));
    } else if (op == "frames_decode") {
      s = codec(a.at(1));
      auto v = ints(a.at(2));
      auto out = decode_frames(*s, unhex(a.at(3)), v.at(0), v.at(1),
                               fpu(integer(a.at(0))));
      result = rs(out.first) + '/' + n(out.second);
    } else if (op == "local") {
      s = codec(a.at(1));
      d = codec(a.at(2));
      result =
          raw(local_roundtrip(record(a.at(3)), *s, *d, fpu(integer(a.at(0)))));
    } else if (op == "sensitivity") {
      auto out = fpu_sensitivity(ints(arg));
      result.clear();
      for (auto model : {Fpu::Model::Ps2, Fpu::Model::Ps2g1, Fpu::Model::Chop,
                         Fpu::Model::Nearest}) {
        if (!result.empty())
          result += '/';
        result += raw(out.at(model));
      }
    } else
      throw ValueError("unknown operation");
  } catch (const ValueError &) {
    result = "value";
  } catch (const IndexError &) {
    result = "index";
  } catch (const MalformedPacket &) {
    result = "malformed";
  }
  if (c)
    state = ch(*c);
  if (s)
    state = cs(*s);
  if (d)
    state += '/' + cs(*d);
  if (reader_state) {
    if (c)
      state += '/' + n(r->pos);
    else
      state = n(r->pos);
  }
  if (writer_state && result != "value") {
    state += '/' + n(w.pos);
  }
  return {result, state};
}
void anchors() {
  bool refused = false;
  try {
    read_frames({}, 1);
  } catch (const ResourceLimit &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("empty frames must refuse looping reference input");
  refused = false;
  try {
    BitReader r({});
    read_axis_symbols(r, 1);
  } catch (const ResourceLimit &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("empty directions must refuse looping reference input");
  BitWriter w;
  w.start(0x1234);
  w.put(5, 3);
  if (raw(w.tobytes()) != "341200a00000" || w.pos != 19)
    throw ValueError("MSB/LE anchor");
  BitReader r(w.tobytes(), 16);
  if (r.get(3) != 5)
    throw ValueError("reader anchor");
  CodecState c;
  auto b = c.pack();
  if (b.size() != 76 || b[10] != 0 || b[11] != 0x43)
    throw ValueError("state reset");
  auto rr = rec12(0xabcd, {1, 2, 3, 4}, {5, 6}, -2, 1);
  if (raw(rr) != "cdab010203040506ffffffff")
    throw ValueError("wire layout");
  refused = false;
  try {
    checked_add(INT64_MAX, 1);
  } catch (const ResourceLimit &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("overflow refusal");
  if (python_index(-1, 4) != 3)
    throw ValueError("negative index");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw ValueError("usage: netwire_codec_tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream in(argv[1], std::ios::binary);
    if (!in)
      throw ValueError("missing vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw ValueError("trace open");
    }
    std::string line;
    if (!std::getline(in, line) || line != "# rta-netwire-codec-v1")
      throw ValueError("schema");
    std::size_t count = 0;
    std::map<std::string, std::size_t> counts;
    while (std::getline(in, line)) {
      auto f = split(line, '\t');
      if (f.size() != 5 || f[0] != "netwire-codec")
        throw ValueError("row shape");
      auto bar = f[2].find('|');
      if (bar == f[2].npos || f[2].substr(0, bar) != std::to_string(count))
        throw ValueError("row identity");
      auto out = run(f[1], f[2].substr(bar + 1));
      if (out.first != f[3] || out.second != f[4]) {
        std::cerr << "Mismatch row " << count << " " << f[1] << "\nexpected "
                  << f[3] << "\nactual " << out.first << "\nexpected state "
                  << f[4] << "\nactual state " << out.second << '\n';
        return 1;
      }
      if (trace)
        trace << f[0] << '\t' << f[1] << '\t' << f[2] << '\t' << out.first
              << '\t' << out.second << '\n';
      ++count;
      ++counts[f[1]];
    }
    // Exact case and operation cardinalities are generated from the pinned
    // corpus.
    const std::map<std::string, std::size_t> expected = {
        {"add", 13376},          {"axis_apply", 768},   {"axis_decode", 768},
        {"axis_encode", 768},    {"axis_read", 774},    {"axis_symbols", 788},
        {"axis_write", 768},     {"channel_pack", 192}, {"channel_unpack", 208},
        {"div", 13376},          {"frames_apply", 787}, {"frames_decode", 1056},
        {"frames_encode", 1057}, {"frames_read", 1062}, {"from_int", 8480},
        {"local", 768},          {"lt", 13376},         {"mul", 13376},
        {"read", 256},           {"record", 192},       {"record_fields", 204},
        {"sensitivity", 16},     {"sqrt", 8480},        {"state_pack", 192},
        {"state_unpack", 268},   {"sub", 13376},        {"to_int", 8480},
        {"write", 256}};
    if (!in.eof() || count != 103468 || counts != expected)
      throw ValueError("incomplete coverage");
    if (trace) {
      trace.flush();
      if (!trace)
        throw ValueError("trace write");
    }
    std::cout << count << " exact netwire codec vectors passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
