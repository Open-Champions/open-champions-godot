// Optional private-file differential transport. No game bytes are distributed.
#include "champions/amx.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

namespace A = champions::simulation::rta::amx;
static std::string hex(const std::string &s) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(s.size() * 2);
  for (unsigned char c : s) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out.empty() ? "-" : out;
}
static std::string image(const A::Machine &m) {
  return hex(std::string(m.image.begin(), m.image.end()));
}
static std::string state(const A::Machine &m) {
  std::ostringstream s;
  s << m.cip << ',' << m.frm << ',' << m.hea << ',' << m.stk << ',' << m.stp
    << ',' << m.flags << ',' << m.curline << ',' << m.curfile << ','
    << m.dbgcode << ',' << m.dbgaddr << ',' << m.dbgparam << ',';
  if (m.dbgname)
    s << *m.dbgname;
  else
    s << 'N';
  s << ',' << m.error << ',';
  if (m.hdr_stp)
    s << *m.hdr_stp;
  else
    s << 'N';
  s << ',' << m.params;
  return s.str();
}
static std::string metadata(const A::Machine &m) {
  if (!m.parsed)
    return "-";
  std::ostringstream s;
  s << m.size << ',' << m.cod << ',' << m.dat << ',' << m.hea0 << ','
    << *m.hdr_stp << ',' << m.start << ',' << m.num_publics << ','
    << m.publics_off << ',' << m.num_natives << ',' << m.natives_off << ','
    << m.num_libraries << ',' << m.num_pubvars << ',' << m.data_offset << ';';
  for (const auto &p : m.publics())
    s << hex(p.first) << ':' << p.second << ',';
  s << ';';
  for (const auto &n : m.native_names())
    s << hex(n) << ',';
  return s.str();
}
struct Stubs {
  std::uint32_t rng = 0;
  int mode = 0;
  std::vector<std::string> calls;
  std::uint64_t nested = 0;
  std::string record(A::Machine &m, const std::string &name,
                     const std::vector<A::Cell> &args) {
    std::ostringstream s;
    s << hex(name) << ':';
    for (auto a : args)
      s << a << ',';
    s << ':';
    for (auto a : args) {
      if (a >= 0 && !(a & 3) && m.get_addr(a) == 0) {
        try {
          s << hex(m.get_string(a));
        } catch (const A::HostFault &) {
          s << 'N';
        }
      } else
        s << 'N';
      s << ',';
    }
    return s.str();
  }
  A::Native native(const std::string &name) {
    return [this, name](
               A::Machine &m,
               const std::vector<A::Cell> &args) -> std::optional<A::Address> {
      calls.push_back(record(m, name, args));
      if (name == "callScript") {
        ++nested;
        const auto address = m.cell(m.params + 4);
        if (m.get_addr(address) != 0)
          throw A::HostFault("callScript address refused");
        return m.call(m.get_string(address), 0x5eed0000 + calls.size());
      }
      if (mode == 0)
        return 0;
      if (mode == 1)
        return 1;
      rng = rng * 1664525u + 1013904223u;
      // Mixed authored return sequence; avoids a dependency on a host RNG.
      switch (rng & 7) {
      case 0:
      case 1:
      case 2:
        return 0;
      case 3:
      case 4:
        return 1;
      case 5:
        return 2 + ((rng >> 8) % 11);
      case 6:
        return -1 - A::Address((rng >> 8) % 8);
      default:
        return A::s32(rng);
      }
    };
  }
  std::string trace() const {
    std::string out;
    for (const auto &c : calls)
      out += c + ';';
    return out.empty() ? "-" : out;
  }
};
static int run(const std::vector<std::filesystem::path> &args) {
  try {
    if (args.size() != 2)
      return 2;
    const auto padding = std::stoull(args[1].string());
    if (padding > 4096)
      return 2;
    std::ifstream input(args[0], std::ios::binary | std::ios::ate);
    if (!input)
      return 2;
    const auto size = input.tellg();
    if (size < 0 || std::uint64_t(size) + padding > A::MAX_IMAGE_BYTES)
      return 2;
    input.seekg(0);
    A::ByteVector bytes{std::istreambuf_iterator<char>(input), {}};
    bytes.resize(bytes.size() + padding, 0);
    A::Machine m(bytes);
    Stubs stubs;
    std::string load_result;
    try {
      const auto e = m.load();
      load_result = std::to_string(e);
      if (!e) {
        A::NativeMap table;
        for (const auto &name : m.native_names())
          table[name] = stubs.native(name);
        load_result += "," + std::to_string(m.register_natives(table));
      }
    } catch (const A::HostFault &) {
      load_result = "H";
    } catch (const A::Limit &) {
      load_result = "L";
    }
    std::string meta;
    try {
      meta = metadata(m);
    } catch (const A::HostFault &) {
      meta = "H";
    }
    std::cout << "L\t" << load_result << '\t' << meta << '\t' << state(m)
              << '\t' << image(m) << std::endl;
    const auto saved = m.snapshot();
    std::string line;
    while (std::getline(std::cin, line)) {
      std::istringstream row(line);
      char command;
      A::Address index, pri, budget;
      unsigned seed;
      int fresh, mode;
      if (!(row >> command >> fresh >> index >> seed >> pri >> mode >>
            budget) ||
          command != 'R' || mode < 0 || mode > 2 || budget < 0)
        return 2;
      std::string excess;
      if (row >> excess)
        return 2;
      if (fresh)
        m.restore(saved);
      stubs.calls.clear();
      stubs.nested = 0;
      stubs.rng = seed;
      stubs.mode = mode;
      m.budget = budget;
      m.profile = std::make_shared<A::Profile>();
      std::string outcome, result = "N";
      try {
        const auto r = m.execute(index, {}, pri);
        outcome = std::to_string(r.first);
        if (r.second)
          result = std::to_string(*r.second);
      } catch (const A::HostFault &) {
        outcome = "H";
      } catch (const A::Limit &) {
        outcome = "L";
      }
      std::ostringstream profile;
      for (const auto &p : *m.profile)
        profile << p.first << ':' << p.second << ',';
      std::cout << "R\t" << outcome << '\t' << result << '\t' << state(m)
                << '\t' << image(m) << '\t' << stubs.trace() << '\t'
                << profile.str() << '\t' << *m.budget << '\t' << stubs.nested
                << std::endl;
    }
    return 0;
  } catch (const std::exception &) {
    // Do not print filenames, script contents or proprietary names on failure.
    std::cerr << "private script validation transport refused\n";
    return 2;
  }
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return run(args);
}
#else
int main(int argc, char **argv) {
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(std::filesystem::u8path(argv[i]));
  return run(args);
}
#endif
