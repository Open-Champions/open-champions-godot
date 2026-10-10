#include "champions/amx.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
using namespace champions::simulation::rta::amx;
namespace {
std::string number(Address n) {
  std::ostringstream o;
  o << std::hex << std::setfill('0') << std::setw(16)
    << static_cast<std::uint64_t>(n);
  return o.str();
}
Address integer(const std::string &s) {
  if (s.size() != 16 ||
      s.find_first_not_of("0123456789abcdef") != std::string::npos)
    throw ValueError("invalid numeric token");
  auto u = std::stoull(s, nullptr, 16);
  return u <= INT64_MAX ? static_cast<Address>(u)
                        : -1 - static_cast<Address>(~u);
}
std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> out;
  std::size_t p = 0;
  for (;;) {
    auto q = s.find(d, p);
    out.push_back(s.substr(p, q - p));
    if (q == std::string::npos)
      break;
    p = q + 1;
  }
  return out;
}
std::string bytes(const ByteVector &v) {
  if (v.empty())
    return "-";
  static const char *h = "0123456789abcdef";
  std::string s;
  for (auto x : v) {
    s += h[x >> 4];
    s += h[x & 15];
  }
  return s;
}
ByteVector unhex(const std::string &s) {
  if (s == "-")
    return {};
  if (s.empty() || s.size() % 2 ||
      s.find_first_not_of("0123456789abcdef") != std::string::npos)
    throw ValueError("invalid bytes");
  ByteVector v;
  for (std::size_t i = 0; i < s.size(); i += 2)
    v.push_back(
        static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  return v;
}
std::string list(const std::vector<std::string> &v) {
  std::string s = "[";
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i)
      s += ';';
    s += v[i];
  }
  return s + "]";
}
std::string optional(const std::optional<Address> &v) {
  return v ? number(*v) : "N";
}
std::string operand(const Operand &o) {
  if (std::holds_alternative<std::monostate>(o))
    return "N";
  if (auto v = std::get_if<Cell>(&o))
    return number(*v);
  if (auto v = std::get_if<std::pair<Cell, Cell>>(&o))
    return list({number(v->first), number(v->second)});
  if (auto v = std::get_if<DebugSymbol>(&o))
    return list({number(v->address), number(v->flags), number(v->name)});
  const auto &t = std::get<CaseTable>(o);
  std::vector<std::string> pairs;
  for (auto p : t.cases)
    pairs.push_back(list({number(p.first), number(p.second)}));
  return list({number(t.count),
               t.default_target ? number(*t.default_target) : "N",
               list(pairs)});
}
std::string state(const Machine &m) {
  std::vector<std::string> fields;
  for (auto x :
       {m.cip, m.frm, m.hea, m.stk, m.stp, static_cast<Address>(m.flags),
        m.curline, m.curfile, m.dbgcode, m.dbgaddr, m.dbgparam})
    fields.push_back(number(x));
  fields.push_back(optional(m.dbgname));
  fields.push_back(number(m.error));
  fields.push_back(optional(m.hdr_stp));
  std::vector<std::string> headers;
  for (auto x : {m.size, m.cod, m.dat, m.hea0, m.start, m.num_publics,
                 m.publics_off, m.num_natives, m.natives_off, m.num_libraries,
                 m.num_pubvars, m.data_offset})
    headers.push_back(m.header_read ? number(x) : "N");
  std::string decoded = "N", bound = "N";
  if (m.decoded) {
    std::vector<std::string> d;
    for (const auto &[p, i] : *m.decoded)
      d.push_back(list(
          {number(p), number(i.opcode), operand(i.operand), number(i.next)}));
    decoded = list(d);
  }
  if (m.bound) {
    std::vector<std::string> b;
    for (const auto &v : *m.bound)
      b.push_back(number(v.kind == BindingKind::Unbound   ? 0
                         : v.kind == BindingKind::Foreign ? 1
                                                          : 2));
    bound = list(b);
  }
  return bytes(m.image) + "|" + list(fields) + "|" + list(headers) + "|" +
         number(m.parsed) + "|" + decoded + "|" + bound;
}
std::vector<Cell> cells(const std::string &s) {
  std::vector<Cell> v;
  if (s != "-")
    for (auto t : split(s, ',')) {
      auto x = integer(t);
      if (x < INT32_MIN || x > INT32_MAX)
        throw ValueError("cell out of range");
      v.push_back(static_cast<Cell>(x));
    }
  return v;
}
std::string numbers(const std::vector<Cell> &v) {
  std::string s;
  for (auto n : v) {
    if (!s.empty())
      s += ',';
    s += number(n);
  }
  return s.empty() ? "-" : s;
}
std::pair<std::string, std::string> run(const std::string &op,
                                        const std::vector<std::string> &a) {
  std::string result, st = "-";
  std::unique_ptr<Machine> machine;
  ByteVector expansion;
  try {
    if (op == "image") {
      if (a.size() != 4)
        throw ValueError("image args");
      auto mode = integer(a[1]);
      auto n = unhex(a[2]);
      std::string name(n.begin(), n.end());
      auto image = unhex(a[3]);
      NativeMap natives;
      if (mode == 3)
        natives["native"] =
            [](Machine &, const std::vector<Cell> &) -> std::optional<Address> {
          return 7;
        };
      machine = std::make_unique<Machine>(image, natives);
      auto &m = *machine;
      if (mode == 4)
        result =
            list({number(m.cip), number(m.frm), number(m.hea), number(m.stk),
                  number(m.stp), number(m.flags), number(m.error)});
      else {
        auto first = m.load();
        if (mode == 1) {
          auto second = m.load();
          result = list({number(first), number(second)});
        } else if (mode == 2) {
          auto f = m.find_public(name);
          result =
              list({number(first), list({number(f.first), number(f.second)})});
        } else
          result = number(first);
      }
    } else if (op == "encode") {
      if (a.size() != 2)
        throw ValueError("encode args");
      result = bytes(encode_compact(cells(a[1])));
    } else if (op == "expand") {
      if (a.size() != 3)
        throw ValueError("expand args");
      auto n = cells(a[1]);
      if (n.size() != 2)
        throw ValueError("expand counts");
      expansion = unhex(a[2]);
      expand_compact(expansion, 0, n[0], n[1]);
      result = "ok";
    } else if (op == "assemble") {
      if (a.size() != 2)
        throw ValueError("assemble args");
      auto i = static_cast<Cell>(integer(a[1]));
      auto out =
          assemble({Label{"start:"}, AsmInstruction{"FILE", {i}, "file", {}},
                    AsmInstruction{"LINE", {i, -i}, {}, {}},
                    AsmInstruction{"SYMBOL", {i, Cell{3}}, "symbol", {}},
                    AsmInstruction{"SRANGE", {i, Cell{4}}, {}, {}},
                    AsmInstruction{"SWITCH", {std::string("table")}, {}, {}},
                    AsmInstruction{"HALT", {i}, {}, {}}, Label{"table:"},
                    AsmInstruction{"CASETBL",
                                   {std::string("start")},
                                   {},
                                   {{i, std::string("start")},
                                    {-i, std::string("table")}}}});
      result = numbers(out.cells);
      st =
          number(out.labels.at("start")) + "," + number(out.labels.at("table"));
    } else if (op == "build") {
      if (a.size() != 2)
        throw ValueError("build args");
      auto i = static_cast<Cell>(integer(a[1]));
      ImageOptions o;
      for (auto x : {i, -i})
        for (unsigned j = 0; j < 4; ++j)
          o.data.push_back(static_cast<std::uint8_t>(
              static_cast<std::uint32_t>(x) >> (8 * j)));
      o.publics = {
          {"z", 0}, {std::string(1, static_cast<char>(0x80)), 4}, {"a", 8}};
      o.natives = {"native"};
      o.stack_cells = 8 + i % 9;
      o.start = 0;
      o.flags = static_cast<std::uint16_t>(i & 2);
      o.file_version = -1;
      o.amx_version = 3;
      o.compact = (i & 1) != 0;
      o.sort_publics = (i & 2) != 0;
      result = bytes(
          build_image({static_cast<Cell>(opcode_number("CONST.pri")), i - 128,
                       static_cast<Cell>(opcode_number("HALT")), 0},
                      o));
    } else
      throw ValueError("unknown operation");
  } catch (const HostFault &) {
    result = "host";
  } catch (const ValueError &) {
    result = "value";
  } catch (const Limit &) {
    result = "limit";
  }
  if (machine)
    st = state(*machine);
  if (op == "expand")
    st = bytes(expansion);
  return {result, st};
}
void anchors() {
  if (NUM_OPCODES != 131 || opcode_number("HALT") != 120 ||
      opcode_number("CASETBL") != 130)
    throw ValueError("opcode anchors");
  auto image = build_image({static_cast<Cell>(opcode_number("HALT")), 0});
  Machine m(image);
  if (m.load() != 0 || !m.parsed || !(m.flags & FLAG_RELOC) ||
      m.decoded->size() != 1)
    throw ValueError("minimal load");
  image[0] = 255;
  if (m.image[0] != 0)
    throw ValueError("image ownership");
  bool refused = false;
  try {
    ImageOptions o;
    o.stack_cells = MAX_IMAGE_BYTES;
    build_image({}, o);
  } catch (const ResourceLimit &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("allocation refusal");
  refused = false;
  try {
    ImageOptions o;
    o.natives = {std::string(20, 'a')};
    build_image({}, o);
  } catch (const ValueError &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("name limit");
  refused = false;
  try {
    m.cod = INT64_MAX;
    m.relocate();
  } catch (const ResourceLimit &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("relocation overflow refusal");
  refused = false;
  try {
    m.num_publics = INT64_MIN;
    m.find_public("x");
  } catch (const ResourceLimit &) {
    refused = true;
  }
  if (!refused)
    throw ValueError("lookup overflow refusal");
  auto encoded = encode_compact({0, -1, 63, -64, 64, -65});
  if (bytes(encoded) != "007f3f408040ff3f")
    throw ValueError("signed compact anchor");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw ValueError("usage: amx_image_tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream in(argv[1], std::ios::binary);
    if (!in)
      throw ValueError("cannot open vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw ValueError("cannot open trace");
    }
    std::string line;
    if (!std::getline(in, line) || line != "# rta-amx-image-v1")
      throw ValueError("schema");
    std::size_t count = 0;
    std::map<std::string, std::size_t> counts;
    while (std::getline(in, line)) {
      auto f = split(line, '\t');
      if (f.size() != 5 || f[0] != "amx-image")
        throw ValueError("row schema");
      auto args = split(f[2], '|');
      if (args.empty() || args[0] != std::to_string(count))
        throw ValueError("row identity");
      auto actual = run(f[1], args);
      if (actual.first != f[3] || actual.second != f[4]) {
        std::cerr << "Mismatch row " << count << " " << f[1] << "\nexpected "
                  << f[3] << "\nactual " << actual.first << "\nexpected state "
                  << f[4] << "\nactual state " << actual.second << "\n";
        return 1;
      }
      if (trace)
        trace << f[0] << '\t' << f[1] << '\t' << f[2] << '\t' << actual.first
              << '\t' << actual.second << '\n';
      ++count;
      ++counts[f[1]];
    }
    if (!in.eof() || count != 3779 ||
        counts != std::map<std::string, std::size_t>{{"image", 2237},
                                                     {"encode", 512},
                                                     {"expand", 518},
                                                     {"assemble", 256},
                                                     {"build", 256}})
      throw ValueError("incomplete fixture coverage");
    if (trace) {
      trace.flush();
      if (!trace)
        throw ValueError("trace write failed");
    }
    std::cout << count << " exact AMX image vectors passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
