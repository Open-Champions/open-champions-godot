// Authored terrain replay: expected columns never drive native execution.
#include "champions/world_queries.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace R = champions::simulation::rta;
namespace W = R::world_queries;
namespace {
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> v;
  std::size_t start = 0;
  for (;;) {
    auto at = s.find(sep, start);
    v.push_back(s.substr(start, at == s.npos ? at : at - start));
    if (at == s.npos)
      return v;
    start = at + 1;
  }
}
std::int64_t integer(const std::string &s) {
  if (s.empty() || s == "-0" || s.front() == '+' ||
      (s.size() > 1 && s.front() == '0') ||
      (s.size() > 2 && s[0] == '-' && s[1] == '0'))
    throw std::runtime_error("noncanonical integer");
  for (std::size_t i = s.front() == '-' ? 1 : 0; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '9')
      throw std::runtime_error("invalid integer");
  std::size_t used = 0;
  auto n = std::stoll(s, &used);
  if (used != s.size())
    throw std::runtime_error("invalid integer");
  return n;
}
template <class T> std::string n(T value) { return std::to_string(value); }
std::string sequence(const std::vector<std::string> &v) {
  std::string s = "[";
  for (const auto &x : v) {
    if (s.size() > 1)
      s += ',';
    s += x;
  }
  return s + ']';
}
template <class V> std::string ints(const V &v) {
  std::vector<std::string> o;
  for (auto x : v)
    o.push_back(n(x));
  return sequence(o);
}
std::string normal(const std::optional<W::Vec3> &v) {
  return v ? ints(*v) : "_";
}
std::string snapshot(const W::Snapshot &s) {
  const auto &d = s.data;
  std::vector<std::string> grid, hfs, elements;
  for (const auto &row : d.grid)
    grid.push_back(ints(row));
  for (const auto &h : d.heightfields)
    hfs.push_back(sequence({n(h.x0), n(h.y0), n(h.w), n(h.h), n(h.lo), n(h.hi),
                            ints(h.samples), n(h.offset)}));
  for (const auto &e : d.elements)
    elements.push_back(sequence(
        {n(e.index), n(e.min_x), n(e.min_y), n(e.max_x), n(e.max_y), n(e.hf),
         n(e.flags), n(e.x), n(e.y), n(e.z), n(e.cos), n(e.sin), n(e.stamp),
         n(e.f_min_x), n(e.f_min_y), n(e.f_max_x), n(e.f_max_y)}));
  return sequence(
      {sequence({n(d.stamp),
                 sequence({n(d.tg_x0), n(d.tg_y0), n(d.tg_w), n(d.tg_h)}),
                 sequence(grid), sequence(hfs), sequence(elements),
                 ints(d.pieces),
                 sequence({n(d.pg_x0), n(d.pg_y0), n(d.pg_w), n(d.pg_h)}),
                 ints(d.path_cells)}),
       sequence({n(s.limits.heightfields), n(s.limits.elements),
                 n(s.limits.grid_cells), n(s.limits.samples),
                 n(s.limits.references), n(s.limits.path_cells),
                 n(s.limits.query_work)})});
}
const std::vector<std::pair<std::string, unsigned>> operations{
    {"collect", 16},      {"height", 24}, {"height_below", 24},
    {"height_point", 24}, {"flag1", 24},  {"sweep", 32},
    {"slide", 16},        {"ground", 16}, {"object_move", 16},
    {"piece", 12},        {"mask", 12}};
R::Raw I(std::int32_t v) { return R::from_int(static_cast<std::uint32_t>(v)); }
W::LevelData fixture(unsigned kind) {
  W::LevelData d;
  d.pg_x0 = -3;
  d.pg_y0 = 7;
  d.pg_w = 2;
  d.pg_h = 1;
  d.path_cells = {0x12345678, 0xfedcba98};
  if (!kind)
    return d;
  d.tg_x0 = d.tg_y0 = -1;
  d.tg_w = d.tg_h = 2;
  std::array<int, 25> heights{}, flags{};
  int lo = 0, hi = 0;
  if (kind == 2) {
    for (unsigned i = 0; i < 25; ++i)
      flags[i] = i % 5 == 3 ? 1 : i % 5 == 4 ? 2 : 0;
  } else if (kind == 3) {
    for (unsigned i = 0; i < 25; ++i) {
      heights[i] = std::array<int, 5>{-2000, 0, 11, 12, 30}[i % 5];
      flags[i] = i % 8;
    }
    lo = -2000;
    hi = 30;
  } else if (kind == 6) {
    for (unsigned i = 0; i < 25; ++i) {
      heights[i] = i % 6 == 0 ? 2000 : -4;
      flags[i] = i % 6 == 0 ? 1 : 0;
    }
    lo = -4;
    hi = 2000;
  } else {
    for (unsigned i = 0; i < 25; ++i)
      heights[i] = 4 + i % 5;
    lo = 4;
    hi = 8;
  }
  W::HeightField field;
  field.x0 = field.y0 = -2;
  field.w = field.h = 5;
  field.lo = lo;
  field.hi = hi;
  field.offset = 0x400;
  for (unsigned i = 0; i < 25; ++i)
    field.samples.push_back(
        static_cast<std::int16_t>(heights[i] * 16 + flags[i]));
  W::HeightField unused;
  unused.x0 = 11;
  unused.y0 = -12;
  unused.w = unused.h = 1;
  unused.lo = unused.hi = -2000;
  unused.samples = {-32000};
  unused.offset = 0x400;
  d.heightfields = {field, unused};
  auto cosine = kind == 4   ? 0
                : kind == 5 ? 23170
                            : 32767,
       sine = kind == 4   ? 32767
              : kind == 5 ? -23170
                          : 0;
  d.elements = {W::make_element(77, -16, -16, 16, 16, 0,
                                4 | (kind == 4 ? 1 : 0), 0, 0, 0, cosine, sine),
                W::make_element(77, -16, -16, 16, 16, 0, 4, 4, 0, 3, 32767, 0),
                W::make_element(-9, -16, -16, 16, 16, 1, 6, 0, 0, 0, 32767, 0)};
  d.grid = {{0, 1, 0, 2}, {-3, 1}, {1, 0}, {-1, 0, 1}};
  d.pieces = {0, 1, -2};
  if (kind == 5) {
    d.elements[0].f_max_x = I(1);
    d.elements[0].f_min_y = I(-1);
  }
  if (kind == 7) {
    d.stamp = UINT32_MAX;
    d.elements[0].stamp = 0;
    d.elements[1].stamp = UINT32_MAX;
    d.elements[2].stamp = 7;
  }
  return d;
}
std::pair<std::string, std::string>
scenario(const std::string &op, std::int64_t cfg, const std::string &args) {
  auto a = split(args, ';');
  if (a.size() != 2 || cfg < 0 || cfg > 15)
    throw std::runtime_error("invalid configuration/arguments");
  auto id = integer(a[0]), variant = integer(a[1]);
  std::int64_t counter = 0;
  bool valid = false;
  for (const auto &entry : operations)
    for (unsigned c = 0; c < (entry.first == "mask" ? 1u : 16u); ++c)
      for (unsigned v = 0; v < entry.second; ++v) {
        if (counter == id)
          valid = op == entry.first && cfg == c && variant == v;
        ++counter;
      }
  if (!valid)
    throw std::runtime_error("inconsistent scenario identity");
  R::ScopedFpu context(
      R::Fpu(static_cast<R::Fpu::Model>(cfg / 4), cfg & 2, cfg & 1));
  auto v = static_cast<unsigned>(variant), kind = v % 8, style = v / 8;
  auto data = fixture(op == "collect" && kind == 0 ? 1 : kind);
  if (op == "piece")
    data = fixture(6);
  if (op == "collect" && v != 0 && kind != 7)
    data.stamp = 1;
  W::Level level(data);
  W::Vec3 pos{0, 0, I(4)}, delta{I(16), I(3), 0};
  R::Raw radius = I(6);
  if (style == 1) {
    pos = {0xc0800001, I(4), I(12)};
    radius = I(4);
    delta = {I(-12), I(7), I(-3)};
  } else if (style == 2) {
    pos = op == "height_point" ? W::Vec3{I(160), I(-160), 0} : W::Vec3{};
    radius = 0;
    delta = {0, 0, 0};
  } else if (style == 3) {
    pos = {I(4), I(1), I(30)};
    radius = I(5);
    delta = {I(2), I(-6), I(13)};
  }
  std::string status = "ok", result = "_";
  try {
    if (op == "collect") {
      auto b = style == 0 ? std::array<R::Raw, 4>{I(-8), I(-8), I(8), I(8)}
                          : std::array<R::Raw, 4>{I(-161), I(-1), I(160), I(1)};
      auto limit = std::array<int, 8>{0, 1, 2, 3, 64, 256, 2, 3}[kind];
      result = ints(W::collect_elements(level, b[0], b[1], b[2], b[3], limit));
      if (v == 13) {
        const auto repeated =
            ints(W::collect_elements(level, b[0], b[1], b[2], b[3], limit));
        level.next_stamp();
        const auto refreshed =
            ints(W::collect_elements(level, b[0], b[1], b[2], b[3], limit));
        result = sequence({result, repeated, refreshed});
      }
    } else if (op == "height")
      result = n(W::terrain_height(level, radius, pos));
    else if (op == "height_below")
      result = n(W::terrain_height_below(level, radius, pos));
    else if (op == "height_point")
      result = n(W::terrain_height_at_point(level, pos));
    else if (op == "flag1")
      result = n(W::terrain_test_flag1(level, radius, pos));
    else if (op == "sweep") {
      auto r = W::terrain_sweep_circle(
          level, radius, pos, delta, std::array<unsigned, 4>{1, 3, 5, 0}[style],
          v & 1);
      result = sequence({n(r.time), normal(r.normal)});
    } else if (op == "slide") {
      auto r =
          W::world_slide_move(level, radius, pos, delta, style == 0 ? 3 : 5);
      result = sequence({n(r.hit), ints(r.position), normal(r.normal)});
    } else if (op == "ground") {
      auto r = W::ground_step(level, radius, pos, style == 0 ? I(-20) : I(60),
                              style == 0 ? W::NO_STAND : I(18));
      result = sequence({ints(r.position), n(r.vz), n(r.landed)});
    } else if (op == "object_move") {
      auto velocity = style == 0 ? W::Vec3{I(600), I(120), I(-10)} : W::Vec3{};
      auto r = W::object_move_isolated(
          level, radius, pos, velocity, I(-20),
          std::array<unsigned, 4>{0, 0x20, 0x40, 0x60}[kind % 4]);
      result = sequence({ints(r.position), n(r.vz), n(r.bits)});
    } else if (op == "piece") {
      auto piece = std::array<int, 12>{-1, 0, 1, 2, 3, -2, 0, 1, 2, 0, 1, 0}[v];
      auto value = std::array<R::Raw, 4>{0x3fc00000, 0xbfc00000, 0x7fffffff,
                                         0xffffffff}[v % 4];
      std::vector<std::string> out{n(level.get_piece_height(piece))};
      level.set_piece_collision(piece, v & 1);
      level.set_piece_height(piece, value);
      out.push_back(n(level.get_piece_height(piece)));
      out.push_back(n(W::terrain_height_at_point(level, {0, 0, 0})));
      if (v == 11) {
        level.set_piece_height(0, I(17));
        level.set_piece_collision(99, true);
      }
      result = sequence(out);
    } else if (op == "mask")
      result = n(W::mover_mask(std::array<std::uint32_t, 12>{
          0, 0x20, 0x40, 0x60, 0x80, 0x10000000, 0xffffffff, 0x20000, 0x21,
          0x41, 0x61, 0x80000000}[v]));
  } catch (const W::ResourceLimit &) {
    throw;
  } catch (const W::Error &) {
    status = op == "piece" ? "IndexError" : "RuntimeError";
    result = "_";
  }
  return {status, sequence({result, snapshot(level.snapshot())})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x2f9bead7eec395f6ULL;
constexpr std::size_t expected_rows = 3276;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: world-queries-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 8ULL * 1024 * 1024;
  constexpr std::size_t max_line = 4 * 1024;
  if (!std::filesystem::is_regular_file(args[0]) ||
      std::filesystem::file_size(args[0]) > max_fixture)
    throw std::runtime_error(
        "fixture is missing, not regular, or exceeds budget");
  if (args.size() == 2) {
    if (std::filesystem::weakly_canonical(args[0]) ==
            std::filesystem::weakly_canonical(args[1]) ||
        (std::filesystem::exists(args[1]) &&
         std::filesystem::equivalent(args[0], args[1])))
      throw std::runtime_error("trace must not overwrite fixture");
  }
  std::ifstream input(args[0], std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open fixture");
  std::ofstream output;
  if (args.size() == 2) {
    output.open(args[1], std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open trace");
  }
  auto line = [&]() {
    std::string value;
    for (char ch; input.get(ch);) {
      if (ch == '\n')
        return value;
      if (value.size() == max_line)
        throw std::runtime_error("fixture line exceeds budget");
      if (static_cast<unsigned char>(ch) > 127 || ch == '\r' || ch == '\0')
        throw std::runtime_error("fixture must be ASCII with LF separators");
      value += ch;
    }
    if (!value.empty())
      throw std::runtime_error("fixture ends without newline");
    return value;
  };
  if (line() != "# world_queries_vectors_v1")
    throw std::runtime_error("fixture schema mismatch");
  std::set<std::string> seen;
  std::size_t rows = 0;
  std::uint64_t digest = 14695981039346656037ULL;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto columns = split(line(), '\t');
    if (columns.size() != 5)
      throw std::runtime_error("fixture requires five columns");
    const auto key = columns[0] + '\t' + columns[1] + '\t' + columns[2];
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate scenario");
    const auto actual = scenario(columns[0], integer(columns[1]), columns[2]);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::cerr << "world-queries mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (args.size() == 2) {
      output << record;
      if (!output)
        throw std::runtime_error("trace write failed");
    }
    for (unsigned char ch : record) {
      digest ^= ch;
      digest *= 1099511628211ULL;
    }
    if (++rows > 65536)
      throw std::runtime_error("fixture row budget exceeded");
  }
  if (input.bad())
    throw std::runtime_error("fixture read failed");
  if (rows != expected_rows || digest != expected_digest)
    throw std::runtime_error("incomplete or changed world-queries corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact world-queries vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    return run(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return entry(args);
}
