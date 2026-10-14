#include "champions/skills_c.hpp"
#include <iostream>
#include <limits>
#include <sstream>
using namespace champions::simulation::rta;
namespace sk = champions::simulation::rta::skills_c;
namespace {
struct Reader {
  std::istringstream stream;
  explicit Reader(const std::string &s) : stream(s) {}
  std::uint64_t number() {
    std::string token;
    if (!(stream >> token) || token.empty() || token[0] == '-' ||
        token[0] == '+' || (token.size() > 1 && token[0] == '0'))
      throw std::runtime_error("invalid integer");
    for (char c : token)
      if (c < '0' || c > '9')
        throw std::runtime_error("invalid integer");
    std::size_t used = 0;
    const auto value = std::stoull(token, &used);
    if (used != token.size())
      throw std::runtime_error("invalid integer");
    return value;
  }
  std::size_t count() {
    auto n = number();
    if (n > 65536)
      throw std::runtime_error("list limit");
    return static_cast<std::size_t>(n);
  }
  void shape(std::size_t n) {
    if (count() != n)
      throw std::runtime_error("invalid shape");
  }
  std::uint32_t raw() {
    auto n = number();
    if (n > UINT32_MAX)
      throw std::runtime_error("raw32 overflow");
    return static_cast<std::uint32_t>(n);
  }
  std::vector<std::uint32_t> vector() {
    std::vector<std::uint32_t> v;
    auto n = count();
    for (std::size_t i = 0; i < n; ++i)
      v.push_back(raw());
    return v;
  }
  void end() {
    std::string s;
    if (stream >> s)
      throw std::runtime_error("trailing input");
  }
};
struct Output {
  std::vector<std::uint64_t> words;
  void n(std::uint64_t v) { words.push_back(v); }
  void vector(const std::vector<std::uint32_t> &v) {
    n(v.size());
    for (auto x : v)
      n(x);
  }
  std::string text() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < words.size(); ++i) {
      if (i)
        out << ' ';
      out << words[i];
    }
    return out.str();
  }
};
struct Callback {
  unsigned slot;
  std::vector<std::uint32_t> result, poke;
  bool throwing;
};
void configs(Output &out, const std::vector<Callback> &c) {
  out.n(c.size());
  for (const auto &v : c) {
    out.n(4);
    out.n(v.slot);
    out.vector(v.result);
    out.vector(v.poke);
    out.n(v.throwing);
  }
}
void writes(Output &out, const std::vector<sk::Write> &v) {
  out.n(v.size());
  for (const auto &x : v) {
    out.n(3);
    out.n(x.address);
    out.n(x.size);
    out.n(x.value);
  }
}
void snapshot(Output &out, const sk::Model &m, const std::vector<Callback> &c) {
  out.n(7);
  out.n(m.mem.bytes().size());
  for (const auto &v : m.mem.bytes()) {
    out.n(2);
    out.n(v.first);
    out.n(v.second);
  }
  writes(out, m.mem.log());
  writes(out, m.writes);
  out.n(m.calls.size());
  for (const auto &v : m.calls) {
    out.n(2);
    out.n(v.slot);
    out.vector(v.args);
  }
  out.n(m.draws);
  out.n(m.external.size());
  for (auto v : m.external)
    out.n(v);
  configs(out, c);
}
std::string execute(unsigned config, const std::string &input) {
  if (config > 15)
    throw std::runtime_error("invalid config");
  set_fpu(static_cast<Fpu::Model>(config / 4), bool(config & 2),
          bool(config & 1));
  Reader r(input);
  r.shape(4);
  sk::Model m;
  auto count = r.count();
  for (std::size_t i = 0; i < count; ++i) {
    r.shape(3);
    auto a = r.raw();
    auto w = r.count();
    auto v = r.number();
    m.mem.poke(a, w, v);
  }
  count = r.count();
  for (std::size_t i = 0; i < count; ++i)
    m.external.insert(r.raw());
  std::vector<Callback> callbacks;
  count = r.count();
  for (std::size_t i = 0; i < count; ++i) {
    r.shape(4);
    auto slot = r.raw();
    auto result = r.vector();
    auto poke = r.vector();
    auto throwing = r.raw();
    if (slot > sk::ModelBuildHideMask || (poke.size() != 0 && poke.size() != 3) || throwing > 1)
      throw std::runtime_error("callback shape");
    callbacks.push_back({slot, result, poke, bool(throwing)});
    m.callbacks[slot] = [result, poke,
                         throwing](sk::Model &model,
                                   const std::vector<std::uint32_t> &) {
      if (!poke.empty())
        model.mem.poke(poke[0], poke[1], poke[2]);
      if (throwing)
        throw std::runtime_error("authored callback exception");
      return result;
    };
  }
  const auto commands = r.count();
  Output out;
  out.n(commands + 1);
  snapshot(out, m, callbacks);
  for (std::size_t i = 0; i < commands; ++i) {
    r.shape(2);
    const auto op = r.raw();
    const auto args = r.vector();
    std::vector<std::uint32_t> result;
    unsigned status = 0;
    try {
      result = m.execute(static_cast<sk::Operation>(op), args);
    } catch (const std::runtime_error &) {
      status = 1;
    }
    out.n(3);
    out.n(status);
    out.vector(result);
    snapshot(out, m, callbacks);
  }
  r.end();
  return out.text();
}
} // namespace
int main() {
  try {
    std::string line;
    std::size_t rows = 0;
    while (std::getline(std::cin, line)) {
      if (line.empty() || line[0] == '#')
        continue;
      const auto tab = line.find('\t');
      if (tab == std::string::npos)
        throw std::runtime_error("missing tab");
      Reader config(line.substr(0, tab));
      auto c = config.raw();
      config.end();
      // Transport contains inputs only. Expected state never enters this
      // runner.
      std::cout << execute(c, line.substr(tab + 1)) << '\n';
      ++rows;
    }
    if (!rows)
      throw std::runtime_error("empty input");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
