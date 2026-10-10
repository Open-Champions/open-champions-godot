#include "champions/amx.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
using namespace champions::simulation::rta::amx;
namespace {
std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    auto end = s.find(delimiter, start);
    out.push_back(s.substr(start, end == s.npos ? end : end - start));
    if (end == s.npos)
      return out;
    start = end + 1;
  }
}
Address number(const std::string &s) {
  std::size_t end = 0;
  auto value = std::stoll(s, &end);
  if (end != s.size())
    throw ValueError("invalid vector integer");
  return value;
}
std::vector<Address> numbers(const std::string &s) {
  std::vector<Address> out;
  if (!s.empty())
    for (auto &v : split(s, ','))
      out.push_back(number(v));
  return out;
}
std::string enc(Address x) { return std::to_string(x); }
std::string enc(const std::optional<Address> &x) { return x ? enc(*x) : "n"; }
std::string pack(const std::vector<std::string> &items) {
  std::string s = "[";
  for (auto &item : items) {
    if (s.size() > 1)
      s += ',';
    s += item;
  }
  return s + "]";
}
std::string enc(const std::vector<Cell> &v) {
  std::vector<std::string> fields;
  for (auto x : v)
    fields.push_back(enc(x));
  return pack(fields);
}
std::string enc(const ExecuteResult &r) {
  return pack({enc(r.first), enc(r.second)});
}
unsigned digit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  throw ValueError("invalid vector hex");
}
ByteVector unhex(const std::string &s) {
  if (s.size() % 2)
    throw ValueError("odd hex");
  ByteVector bytes;
  for (std::size_t i = 0; i < s.size(); i += 2)
    bytes.push_back(digit(s[i]) * 16 + digit(s[i + 1]));
  return bytes;
}
std::string hex(const ByteVector &bytes) {
  const char *d = "0123456789abcdef";
  std::string s;
  s.reserve(bytes.size() * 2);
  for (auto b : bytes) {
    s += d[b >> 4];
    s += d[b & 15];
  }
  return s;
}
struct Result {
  std::string result, state;
  std::set<Cell> executed;
  std::set<std::string> errors;
  std::set<Address> codes;
};
Result run(const ByteVector &image, const std::string &op,
           const std::vector<Address> &args, const std::string &setup) {
  Machine m(image);
  std::vector<std::string> events;
  Address mode = 0, repeat = 1;
  Native native =
      [&](Machine &machine,
          const std::vector<Cell> &values) -> std::optional<Address> {
    events.push_back(
        pack({"0", enc(values), enc(machine.params), enc(machine.cip),
              enc(machine.frm), enc(machine.hea), enc(machine.stk)}));
    Address sum = 0;
    for (auto v : values)
      sum += v;
    machine.set_cell(0, std::uint64_t(sum) + 7);
    if (mode == 1) {
      machine.hea += 4;
      machine.stk -= 4;
      machine.raise_error(37);
    } else if (mode == 2) {
      auto result = machine.execute(1, {5}, 123);
      events.push_back(
          pack({"2", enc(result.first), enc(result.second), enc(machine.stk)}));
      return result.second;
    } else if (mode == 3)
      throw HostFault("authored native exception");
    else if (mode == 4) {
      machine.profile = std::make_shared<Profile>();
      machine.decoded = std::make_shared<Decoded>(*machine.decoded);
    } else if (mode == 5)
      machine.callback = {};
    else if (mode == 6)
      machine.budget = 0;
    return sum + 0x100000001LL;
  };
  m.load();
  m.register_natives({{"native", native}});
  m.profile = std::make_shared<Profile>();
  m.budget = 200;
  for (auto &command : split(setup, ';')) {
    if (command.empty())
      continue;
    auto fields = split(command, '=');
    if (fields.size() != 2)
      throw ValueError("bad vector setup");
    const auto &key = fields[0];
    const auto value = number(fields[1]);
    if (key == "mode")
      mode = value;
    else if (key == "repeat")
      repeat = value;
    else if (key == "callback") {
      if (!value)
        m.callback = {};
      else
        m.callback = [&](Address index, Address params) {
          events.push_back(pack({"3", enc(index), enc(params)}));
          return std::make_pair<Address, Address>(41, 99);
        };
    } else if (key == "budget")
      m.budget = value == -999 ? std::nullopt : std::optional<Address>(value);
    else if (key == "profile")
      m.profile.reset();
    else if (key == "decoded")
      m.decoded.reset();
    else if (key == "inject_none") {
      (*m.decoded)[0] = {0, std::monostate{}, 4};
      m.flags |= FLAG_RELOC;
    } else if (key == "parsed")
      m.parsed = bool(value);
#define FIELD(field) else if (key == #field) m.field = value
    FIELD(cip);
    FIELD(frm);
    FIELD(hea);
    FIELD(stk);
    FIELD(stp);
    FIELD(flags);
    FIELD(curline);
    FIELD(curfile);
    FIELD(dbgcode);
    FIELD(dbgaddr);
    FIELD(dbgparam);
    FIELD(error);
    FIELD(hdr_stp);
#undef FIELD
    else throw ValueError("unknown setup field");
  }
  Result out;
  for (Address i = 0; i < repeat; ++i) {
    if (i)
      out.result += ':';
    try {
      if (op == "call")
        out.result += enc(m.call(args.at(0) ? "missing" : "entry", args.at(1)));
      else if (op == "divide") {
        auto result = floored_div(static_cast<Cell>(args.at(0)),
                                  static_cast<Cell>(args.at(1)));
        out.result += pack({enc(result.first), enc(result.second)});
      } else if (op == "execute") {
        std::vector<Cell> params;
        for (std::size_t j = 2; j < args.size(); ++j)
          params.push_back(s32(args[j]));
        const auto result = m.execute(args.at(0), params, args.at(1));
        out.result += enc(result);
        out.codes.insert(result.first);
      } else
        throw ValueError("unknown operation");
    } catch (const HostFault &) {
      out.result += "!HostFault";
      out.errors.insert("HostFault");
    } catch (const Limit &) {
      out.result += "!Limit";
      out.errors.insert("Limit");
    } catch (const ZeroDivisionError &) {
      out.result += "!ZeroDivisionError";
      out.errors.insert("ZeroDivisionError");
    }
  }
  std::string profile = "n";
  if (m.profile) {
    std::vector<std::string> fields;
    for (auto entry : *m.profile) {
      fields.push_back(pack({enc(entry.first), std::to_string(entry.second)}));
      out.executed.insert(entry.first);
    }
    profile = pack(fields);
  }
  out.state = pack({enc(m.cip), enc(m.frm), enc(m.hea), enc(m.stk), enc(m.stp),
                    std::to_string(m.flags), enc(m.curline), enc(m.curfile),
                    enc(m.dbgcode), enc(m.dbgaddr), enc(m.dbgparam),
                    enc(m.dbgname), enc(m.error), enc(m.hdr_stp), enc(m.params),
                    enc(m.budget), profile, pack(events)}) +
              ":" + hex(m.image);
  return out;
}
void anchors() {
  bool zero_divide = false;
  try {
    (void)floored_div(7, 0);
  } catch (const ZeroDivisionError &) {
    zero_divide = true;
  }
  if (!zero_divide)
    throw ValueError("direct division exception anchor");
  if (floored_div(-7, 3) != std::make_pair<Cell, Cell>(-3, 2))
    throw ValueError("floor divide anchor");
  if (floored_div(-2147483647 - 1, -1) !=
      std::make_pair<Cell, Cell>(-2147483647 - 1, 0))
    throw ValueError("divide overflow anchor");
  if (floored_div(7, -3) != std::make_pair<Cell, Cell>(-3, -2))
    throw ValueError("negative divisor anchor");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw ValueError("usage: amx-execute-tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
      throw ValueError("missing fixture");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw ValueError("cannot create trace");
    }
    std::string line;
    if (!std::getline(input, line) || line != "# amx_execute_vectors_v1")
      throw ValueError("missing fixture schema");
    std::uint64_t hash = 14695981039346656037ULL;
    std::size_t count = 0;
    std::set<Cell> executed;
    std::set<std::string> errors;
    std::set<Address> codes;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      auto columns = split(line, '\t');
      if (columns.size() != 5 || columns[0] != "amx")
        throw ValueError("malformed fixture row");
      auto args = split(columns[2], '|');
      if (args.size() != 3)
        throw ValueError("malformed fixture args");
      auto result = run(unhex(args[0]), columns[1], numbers(args[1]), args[2]);
      executed.insert(result.executed.begin(), result.executed.end());
      errors.insert(result.errors.begin(), result.errors.end());
      codes.insert(result.codes.begin(), result.codes.end());
      if (result.result != columns[3] || result.state != columns[4]) {
        std::cerr << "Mismatch row " << count + 1 << " " << columns[1]
                  << " setup " << args[2] << "\nExpected result " << columns[3]
                  << " actual " << result.result << "\nExpected state "
                  << columns[4] << "\nActual state " << result.state << "\n";
        return 1;
      }
      const auto actual = columns[0] + "\t" + columns[1] + "\t" + columns[2] +
                          "\t" + result.result + "\t" + result.state + "\n";
      for (unsigned char c : actual) {
        hash ^= c;
        hash *= 1099511628211ULL;
      }
      if (trace)
        trace << actual;
      ++count;
    }
    if (input.bad())
      throw ValueError("fixture read failure");
    for (Cell op = 0; op < 131; ++op)
      if (!executed.count(op))
        throw ValueError("missing opcode coverage");
    if (!errors.count("HostFault") || !errors.count("Limit") ||
        !errors.count("ZeroDivisionError"))
      throw ValueError("missing error-class coverage");
    for (Address code : {0, 3, 4, 5, 6, 7, 8, 9, 11, 20, 22, 37, 41})
      if (!codes.count(code))
        throw ValueError("missing returned error code");
    constexpr std::size_t expected_count = 6412;
    constexpr std::uint64_t expected_hash = 0xc13d8d22e548a881ULL;
    if (count != expected_count || hash != expected_hash)
      throw ValueError("incomplete or changed execute corpus");
    if (trace) {
      trace.flush();
      if (!trace)
        throw ValueError("trace write failure");
    }
    std::cout << count
              << " exact AMX execute vectors /131 opcodes passed; FNV1a64 "
              << std::hex << hash << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
