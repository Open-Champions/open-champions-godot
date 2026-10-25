// level_factory against research tools/ref_level_factory.py (t3/research-script-session
// 6841b8a): the compiled test table, and every case the generator ran through the
// reference with authored strings at the reference's addresses.
#include "champions/level_factory.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace L = champions::simulation::rta::level_factory;
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
std::string unescape(const std::string &s) {
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      const char n = s[++i];
      out += n == 't' ? '\t' : n == 'p' ? '|' : n;
    } else
      out += s[i];
  }
  return out;
}
std::vector<std::uint32_t> words(const std::string &s) {
  std::vector<std::uint32_t> out;
  if (s.empty())
    return out;
  for (const auto &w : split(s, ','))
    out.push_back(static_cast<std::uint32_t>(std::stoul(w, nullptr, 16)));
  return out;
}
const char *kind_name(L::Kind k) {
  switch (k) {
  case L::Kind::Object: return "object";
  case L::Kind::Call: return "call";
  case L::Kind::ClearColor: return "color";
  default: return "nothing";
  }
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("usage: level_factory_tests VECTORS");
    std::ifstream in(argv[1]);
    if (!in)
      throw std::runtime_error("vectors are not readable");
    std::map<std::uint32_t, std::string> strings;
    std::vector<std::vector<std::string>> table, cases;
    for (std::string line; std::getline(in, line);) {
      if (line.empty() || line[0] == '#')
        continue;
      auto f = split(line, '\t');
      if (f[0] == "S")
        strings[static_cast<std::uint32_t>(std::stoul(f[1], nullptr, 16))] = unescape(f[2]);
      else if (f[0] == "T")
        table.push_back(f);
      else if (f[0] == "C")
        cases.push_back(f);
    }
    // An authored executable: the strings above, an ASCII class table (bit 0
    // upper case, bit 3 space).
    L::Executable exe;
    exe.string = [&](std::uint32_t addr) {
      auto it = strings.find(addr);
      if (it == strings.end())
        throw std::runtime_error("no authored string at that address");
      return it->second;
    };
    exe.byte = [](std::uint32_t addr) -> std::uint8_t {
      const auto c = std::int64_t(addr) - std::int64_t(L::CTYPE);
      return std::uint8_t((c >= 'A' && c <= 'Z' ? 1 : 0) |
                          ((c == ' ' || (c >= 9 && c <= 13)) ? 8 : 0));
    };
    const auto &rows = L::tests();
    check(rows.size() == table.size() && rows.size() == 206, "test table size");
    {
      // creature_kind: the research probe's per-row answer (creature_class_table.inc).
      // Every object-building name-test row has an entry; four are undecided.
      unsigned yes = 0, no = 0, unknown = 0, building = 0;
      for (const auto &r : rows) {
        if (!r.size || r.clear_color)
          continue;
        ++building;
        L::Outcome o;
        o.kind = L::Kind::Object;
        o.test = r.name;
        const auto kind = L::creature_kind(o);
        (kind == L::Creature::Yes ? yes : kind == L::Creature::No ? no : unknown) += 1;
      }
      check(building == 201 && yes == 97 && no == 100 && unknown == 4, "creature table covers every building row");
      L::Outcome probe;
      probe.kind = L::Kind::Object;
      probe.test = 0x5323f0;
      check(L::creature_kind(probe) == L::Creature::No, "a GameObject-chain row is not a creature");
      probe.test = 0x532870;
      check(L::creature_kind(probe) == L::Creature::Yes, "a Creature_ctor-chain row is a creature");
      probe.test = 0x532f78;
      check(L::creature_kind(probe) == L::Creature::Unknown, "an undecided row stays unknown");
      probe.test = 0;
      check(L::creature_kind(probe) == L::Creature::Unknown, "a property fall-back has no row");
      probe.test = 0x5323c0; // the Line row (a WaveMaker by property)
      check(L::creature_kind(probe) == L::Creature::Unknown, "the Line row is unknown");
      probe.test = 0x5323f0;
      probe.kind = L::Kind::Call;
      check(L::creature_kind(probe) == L::Creature::Unknown, "a call row builds no object");
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const auto &r = rows[i];
      const auto &t = table[i];
      const auto calls = words(t[4]);
      check((t[1] == "prefix") == r.prefix, "row kind");
      check(std::stoul(t[2], nullptr, 16) == r.name, "row string address");
      check(std::stoul(t[3], nullptr, 16) == r.size, "row size");
      check(calls.size() == r.call_count, "row call count");
      for (std::size_t k = 0; k < calls.size(); ++k)
        check(calls[k] == r.calls[k], "row call");
      check((t[5] == "1") == r.clear_color, "row clear colour");
    }
    unsigned objects = 0, nothing = 0;
    for (const auto &c : cases) {
      const auto name = unescape(c[1]);
      std::vector<std::string> props;
      const bool null = c[2] == "-";
      if (!null && !c[2].empty())
        for (const auto &p : split(c[2], '|'))
          props.push_back(unescape(p));
      const auto got = L::classify(name, null ? nullptr : &props, exe);
      const auto sizes = words(c[4]);
      const auto calls = words(c[5]);
      const std::string where = "case '" + name + "'";
      check(c[3] == kind_name(got.kind), where + " kind");
      check(sizes.empty() ? got.kind != L::Kind::Object : (sizes.size() == 1 && got.size == sizes[0]),
            where + " size");
      check(calls == got.calls, where + " calls");
      objects += got.kind == L::Kind::Object;
      nothing += got.kind == L::Kind::Nothing;
    }
    check(cases.size() > 1000 && objects > 0 && nothing > 0, "case coverage");
    // atoi: newlib strtol with a 64-bit long, the low 32 bits.
    check(L::atoi("\t-3", exe) == 0xfffffffdu, "atoi sign and space");
    check(L::atoi("4294967296", exe) == 0, "atoi low bits");
    check(L::atoi("99999999999999999999", exe) == 0xffffffffu, "atoi clamp high");
    check(L::atoi("-99999999999999999999", exe) == 0, "atoi clamp low");
    std::cout << "PASS level-factory: " << checks << " checks, " << cases.size()
              << " reference cases\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL level-factory: " << e.what() << '\n';
    return 1;
  }
}
