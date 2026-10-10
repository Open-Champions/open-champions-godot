#include "champions/amx.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
using namespace champions::simulation::rta::amx;
namespace {
std::string join(const std::vector<std::string> &v, const char *sep) {
  std::string s;
  bool first = true;
  for (const auto &x : v) {
    if (!first)
      s += sep;
    first = false;
    s += x;
  }
  return s;
}
std::string list(const std::vector<std::string> &v) {
  return "[" + join(v, ",") + "]";
}
std::string hex(const ByteVector &v) {
  constexpr char d[] = "0123456789abcdef";
  std::string s;
  for (auto b : v) {
    s += d[b >> 4];
    s += d[b & 15];
  }
  return s;
}
ByteVector unhex(const std::string &s) {
  if (s.size() % 2)
    throw std::runtime_error("bad hex");
  ByteVector b;
  auto digit = [](char c) -> unsigned {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    throw std::runtime_error("bad hex digit");
  };
  for (std::size_t i = 0; i < s.size(); i += 2)
    b.push_back(std::uint8_t(digit(s[i]) * 16 + digit(s[i + 1])));
  return b;
}
std::string bytes(const ByteVector &v) { return "b" + hex(v); }
std::string bytes(const std::string &v) {
  return bytes(ByteVector(v.begin(), v.end()));
}
std::string txt(const std::string &v) {
  return "s" + hex(ByteVector(v.begin(), v.end()));
}
template <class T> std::string num(T v) { return std::to_string(v); }
std::string opt(const std::optional<Address> &v) { return v ? num(*v) : "_"; }
std::string cells(const std::vector<Cell> &v) {
  std::vector<std::string> out;
  for (auto x : v)
    out.push_back(num(x));
  return list(out);
}
std::string fields(const std::vector<std::optional<Address>> &v) {
  std::vector<std::string> out;
  for (auto x : v)
    out.push_back(opt(x));
  return list(out);
}
std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> v;
  std::size_t at = 0;
  for (;;) {
    auto next = s.find(delimiter, at);
    v.push_back(s.substr(at, next == std::string::npos ? next : next - at));
    if (next == std::string::npos)
      break;
    at = next + 1;
  }
  return v;
}
Address number(const std::string &s) {
  std::size_t n = 0;
  auto v = std::stoll(s, &n);
  if (n != s.size())
    throw std::runtime_error("bad integer");
  return v;
}
struct Context;
struct Handler {
  Context *context;
  std::string name;
  int variant;
  std::optional<Address> operator()(Machine &, const std::vector<Cell> &) const;
};
struct Context {
  std::vector<std::string> events;
  std::map<Address, Snapshot> saved;
  int callback_mode = 0;
  NativeMap handlers(Address mask, Address variant) {
    NativeMap out;
    const std::vector<std::string> names{"f0", "f1", std::string("\xffx", 2)};
    for (unsigned i = 0; i < 3; ++i)
      if (std::uint64_t(mask) & (1u << i))
        out.emplace(names[i], Handler{this, names[i],
                                      int(i == 0   ? variant
                                          : i == 1 ? 1
                                                   : 7)});
    return out;
  }
  void event(std::vector<std::string> v) { events.push_back(list(v)); }
};
std::optional<Address>
Handler::operator()(Machine &m, const std::vector<Cell> &args) const {
  auto &c = *context;
  c.event({txt("enter"), bytes(name), num(variant), cells(args), num(m.params),
           num(m.error)});
  std::optional<Address> result;
  switch (variant) {
  case 0: {
    Address sum = 100;
    for (auto v : args)
      sum += v;
    result = sum;
    break;
  }
  case 1:
    m.raise_error(77);
    break;
  case 2: {
    auto v = m.call_native(1, 32);
    c.event({txt("nested_native"), num(v.first), num(v.second), num(m.params),
             num(m.error)});
    result = v.second + 5;
    break;
  }
  case 3: {
    m.raise_error(45);
    auto v = m.execute(0);
    c.event({txt("nested_exec"), num(v.first), opt(v.second), num(m.params),
             num(m.error)});
    result = v.second.value_or(0) + 7;
    break;
  }
  case 4:
    m.bound.reset();
    m.error = 123;
    result = INT64_MAX;
    break;
  case 5:
    m.set_cell(12, 0xabcddcba);
    throw HostFault("authored native fault");
  case 6:
    result = INT64_MIN;
    break;
  default:
    break;
  }
  c.event({txt("exit"), bytes(name), opt(result), num(m.params), num(m.error)});
  return result;
}
void set_state(Machine &m, unsigned i, std::optional<Address> v) {
  if (i == 11) {
    m.dbgname = v;
    return;
  }
  if (i == 13) {
    m.hdr_stp = v;
    return;
  }
  if (!v)
    throw std::runtime_error("unsupported fixture null");
  switch (i) {
  case 0:
    m.cip = *v;
    break;
  case 1:
    m.frm = *v;
    break;
  case 2:
    m.hea = *v;
    break;
  case 3:
    m.stk = *v;
    break;
  case 4:
    m.stp = *v;
    break;
  case 5:
    m.flags = std::uint32_t(*v);
    break;
  case 6:
    m.curline = *v;
    break;
  case 7:
    m.curfile = *v;
    break;
  case 8:
    m.dbgcode = *v;
    break;
  case 9:
    m.dbgaddr = *v;
    break;
  case 10:
    m.dbgparam = *v;
    break;
  case 12:
    m.error = *v;
    break;
  default:
    throw std::runtime_error("bad state index");
  }
}
std::string operand(const Operand &v) {
  return std::visit(
      [](const auto &x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same<T, std::monostate>::value)
          return "_";
        else if constexpr (std::is_same<T, Cell>::value)
          return num(x);
        else if constexpr (std::is_same<T, std::pair<Cell, Cell>>::value)
          return list({num(x.first), num(x.second)});
        else if constexpr (std::is_same<T, DebugSymbol>::value)
          return list({num(x.address), num(x.flags), num(x.name)});
        else {
          std::vector<std::string> pairs;
          for (auto p : x.cases)
            pairs.push_back(list({num(p.first), num(p.second)}));
          return list({num(x.count),
                       x.default_target ? num(*x.default_target) : "_",
                       list(pairs)});
        }
      },
      v);
}
std::string state(Machine &m, Context &c) {
  auto snap = m.snapshot();
  std::string bound = "_", profile = "_", decoded = "_";
  std::vector<std::string> v;
  if (m.bound) {
    for (const auto &b : *m.bound) {
      std::string label = b.kind == BindingKind::Unbound ? "U" : "F";
      if (b.kind == BindingKind::Bound) {
        auto *h = b.function.target<Handler>();
        if (!h)
          throw std::runtime_error("unexpected native target");
        label = "H" + num(h->variant) + ":" +
                hex(ByteVector(h->name.begin(), h->name.end()));
      }
      v.push_back(txt(label));
    }
    bound = list(v);
    v.clear();
  }
  if (m.profile) {
    for (auto p : *m.profile)
      v.push_back(list({num(p.first), num(p.second)}));
    profile = list(v);
    v.clear();
  }
  if (m.decoded) {
    for (const auto &p : *m.decoded)
      v.push_back(list({num(p.first), num(p.second.opcode),
                        operand(p.second.operand), num(p.second.next)}));
    decoded = list(v);
    v.clear();
  }
  std::vector<std::string> pending;
  for (const auto &p : m.pending_natives) {
    auto *h = p.second.target<Handler>();
    if (!h)
      throw std::runtime_error("pending target");
    pending.push_back(
        list({bytes(p.first),
              txt("H" + num(h->variant) + ":" +
                  hex(ByteVector(h->name.begin(), h->name.end())))}));
  }
  for (const auto &p : c.saved)
    v.push_back(
        list({num(p.first), bytes(p.second.image), fields(p.second.state)}));
  auto saved = list(v);
  v.clear();
  for (Address value : {m.size, m.cod, m.dat, m.hea0, m.start, m.num_publics,
                        m.publics_off, m.num_natives, m.natives_off,
                        m.num_libraries, m.num_pubvars, m.data_offset})
    v.push_back(m.header_read ? num(value) : "_");
  return join({bytes(m.image), fields(snap.state), num(m.params), opt(m.budget),
               profile, bound, num(c.callback_mode), num(int(m.parsed)),
               list(v), decoded, list(c.events), list(pending), saved},
              "/");
}
std::pair<std::string, std::string> run(const std::string &args) {
  auto sections = split(args, ';');
  if (sections.size() != 3)
    throw std::runtime_error("bad args");
  number(sections[0]);
  auto image = unhex(sections[1]);
  Context c;
  Machine m(image);
  c.callback_mode = m.callback ? 2 : 0;
  std::vector<std::string> results;
  for (const auto &command : split(sections[2], '|')) {
    auto p = split(command, ',');
    auto a = [&](std::size_t i) { return number(p.at(i)); };
    const auto &name = p[0];
    std::string v = "_";
    try {
      if (name == "load") {
        v = num(m.load());
        if (!c.callback_mode && m.callback)
          c.callback_mode = 1;
      } else if (name == "cell")
        v = num(m.cell(a(1)));
      else if (name == "put")
        m.set_cell(a(1), std::uint64_t(a(2)));
      else if (name == "mem")
        v = bytes(m.memory());
      else if (name == "addr")
        v = num(m.get_addr(a(1)));
      else if (name == "get")
        v = bytes(m.get_string(a(1)));
      else if (name == "len")
        v = num(m.str_len(a(1)));
      else if (name == "set") {
        auto b = unhex(p.at(3));
        v = num(
            m.set_string(a(1), std::string(b.begin(), b.end()), bool(a(2))));
      } else if (name == "allot") {
        auto x = m.allot(a(1));
        v = list({num(x.first), opt(x.second)});
      } else if (name == "release")
        v = num(m.release(a(1)));
      else if (name == "error")
        v = num(m.raise_error(a(1)));
      else if (name == "reg")
        v = num(m.register_natives(c.handlers(a(1), a(2))));
      else if (name == "pending")
        m.pending_natives = c.handlers(a(1), a(2));
      else if (name == "parsed")
        m.parsed = bool(a(1));
      else if (name == "data")
        m.data_offset = a(1);
      else if (name == "native") {
        auto x = m.call_native(a(1), a(2));
        v = list({num(x.first), num(x.second)});
      } else if (name == "cb") {
        c.callback_mode = int(a(1));
        switch (c.callback_mode) {
        case 0:
          m.callback = {};
          break;
        case 1:
          m.callback = [&](Address i, Address p) {
            return m.call_native(i, p);
          };
          break;
        case 2:
          m.callback = [&](Address i, Address p) {
            return m.foreign_callback(i, p);
          };
          break;
        default:
          m.callback = [&](Address i, Address p) {
            c.event({txt("callback"), num(i), num(p)});
            m.error = 91;
            return std::pair<Address, Address>{23, INT64_C(0x1ffffffff)};
          };
          break;
        }
      } else if (name == "exec") {
        std::vector<Cell> params;
        for (std::size_t i = 3; i < p.size(); ++i)
          params.push_back(s32(std::uint64_t(a(i))));
        auto x = m.execute(a(1), params, a(2));
        v = list({num(x.first), opt(x.second)});
      } else if (name == "call") {
        auto b = unhex(p.at(1));
        v = num(m.call(std::string(b.begin(), b.end()), a(2)));
      } else if (name == "snap")
        c.saved[a(1)] = m.snapshot();
      else if (name == "restore") {
        auto copy = c.saved.at(a(1));
        copy.state.resize(
            std::min(copy.state.size(), static_cast<std::size_t>(a(2))));
        m.restore(copy);
      } else if (name == "state")
        set_state(m, unsigned(a(1)),
                  p.at(2) == "_" ? std::nullopt : std::optional<Address>(a(2)));
      else if (name == "params")
        m.params = a(1);
      else if (name == "budget")
        m.budget = p.at(1) == "_" ? std::nullopt : std::optional<Address>(a(1));
      else if (name == "profile") {
        m.profile = a(1) ? std::make_shared<Profile>() : nullptr;
        if (m.profile)
          (*m.profile)[11] = 9;
      } else if (name == "raw")
        m.image.at(static_cast<std::size_t>(a(1))) = std::uint8_t(a(2));
      else if (name == "clearbound")
        m.bound.reset();
      else if (name == "cleardecoded")
        m.decoded.reset();
      else if (name == "find") {
        auto b = unhex(p.at(1));
        auto x = m.find_public(std::string(b.begin(), b.end()));
        v = list({num(x.first), num(x.second)});
      } else
        throw std::runtime_error("unknown command");
      results.push_back(v);
    } catch (const HostFault &) {
      results.push_back("!HostFault");
    } catch (const Limit &) {
      results.push_back("!Limit");
    }
  }
  return {join(results, "~"), state(m, c)};
}
void invariants() {
  ImageOptions options;
  options.natives = {"f0"};
  options.data = ByteVector(16);
  Machine m(build_image({0x78, 0}, options));
  m.load();
  m.set_cell(0, 0);
  m.register_natives(
      {{"f0",
        [](Machine &x, const std::vector<Cell> &) -> std::optional<Address> {
          return x.call_native(0, 0).second;
        }}});
  bool fail = false;
  try {
    m.call_native(0, 0);
  } catch (const ResourceLimit &) {
    fail = true;
  }
  if (!fail)
    throw std::runtime_error("native recursion guard");
  m.bound.reset();
  m.register_natives(
      {{"f0",
        [](Machine &, const std::vector<Cell> &) -> std::optional<Address> {
          return 9;
        }}});
  if (m.call_native(0, 0).second != 9)
    throw std::runtime_error("native recursion depth did not unwind");
  auto s = m.snapshot();
  s.state[0] = std::nullopt;
  fail = false;
  try {
    m.restore(s);
  } catch (const HostFault &) {
    fail = true;
  }
  if (!fail)
    throw std::runtime_error("snapshot integer None guard");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error("usage: amx-host-tests TSV [TRACE]");
    invariants();
    std::ifstream in(argv[1]);
    if (!in)
      throw std::runtime_error("open fixture");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary | std::ios::trunc);
      if (!trace)
        throw std::runtime_error("open trace");
    }
    std::string line;
    std::map<std::string, std::size_t> counts;
    std::set<std::string> seen;
    std::size_t rows = 0;
    std::uint64_t hash = 14695981039346656037ULL;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      auto p = split(line, '\t');
      if (p.size() != 5 || p[0] != "amx-host" ||
          !seen.insert(p[1] + "\t" + p[2]).second)
        throw std::runtime_error("malformed or duplicate fixture");
      auto actual = run(p[2]);
      ++rows;
      ++counts[p[1]];
      if (actual.first != p[3] || actual.second != p[4]) {
        std::cerr << "case " << rows << " " << p[1] << "\nexpected " << p[3]
                  << "\nactual " << actual.first << "\n";
        std::ofstream("work/amx-host/expected-state.txt") << p[4];
        std::ofstream("work/amx-host/actual-state.txt") << actual.second;
        throw std::runtime_error("reference mismatch");
      }
      auto out = p[0] + "\t" + p[1] + "\t" + p[2] + "\t" + actual.first + "\t" +
                 actual.second + "\n";
      if (trace)
        trace << out;
      for (unsigned char ch : out) {
        hash ^= ch;
        hash *= 1099511628211ULL;
      }
    }
    if (!in.eof())
      throw std::runtime_error("fixture read failure");
    const std::map<std::string, std::size_t> expected{
        {"memory", 133},  {"string", 216}, {"detection", 10}, {"heap", 504},
        {"dispatch", 88}, {"binding", 40}, {"params", 7},     {"nested", 64},
        {"snapshot", 32}, {"unloaded", 2}, {"foreign", 2},    {"rebind", 1},
        {"pending", 8}};
    if (counts != expected)
      throw std::runtime_error("empty or incomplete corpus");
    if (trace) {
      trace.flush();
      if (!trace)
        throw std::runtime_error("trace write failure");
    }
    std::cout << rows << " exact AMX host traces passed; fnv1a64=" << std::hex
              << hash << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
