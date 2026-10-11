// Authored terrain-file replay: input hex drives decoding; expected columns
// never influence native execution or query state.
#include "champions/terrain_file.hpp"
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
namespace T = R::terrain_file;
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
std::vector<std::uint8_t> unhex(const std::string &source) {
  if (source.size() % 2 || source.size() > 4 * 1024)
    throw std::runtime_error("invalid source hex length");
  auto nibble = [](char c) -> unsigned {
    if (c >= '0' && c <= '9')
      return unsigned(c - '0');
    if (c >= 'a' && c <= 'f')
      return unsigned(c - 'a' + 10);
    throw std::runtime_error("source hex must be lowercase canonical bytes");
  };
  std::vector<std::uint8_t> bytes;
  bytes.reserve(source.size() / 2);
  for (std::size_t i = 0; i < source.size(); i += 2)
    bytes.push_back(static_cast<std::uint8_t>(nibble(source[i]) * 16 +
                                              nibble(source[i + 1])));
  return bytes;
}
R::Raw I(std::int32_t value) {
  return R::from_int(static_cast<std::uint32_t>(value));
}
std::string parse_limits(const T::ParseLimits &p) {
  const auto &l = p.level_limits;
  return sequence({n(p.max_input_bytes), n(p.max_chunks), n(p.max_pieces),
                   n(p.max_list_steps), n(p.max_fixup_samples),
                   sequence({n(l.heightfields), n(l.elements), n(l.grid_cells),
                             n(l.samples), n(l.references), n(l.path_cells),
                             n(l.query_work)})});
}
std::string query_batch(W::Level &level) {
  const W::Vec3 position{I(0), I(0), I(4)}, delta{I(16), I(3), 0};
  const auto radius = I(6);
  std::vector<std::string> out;
  level.next_stamp();
  out.push_back(ints(W::collect_elements(level, I(-8), I(-8), I(8), I(8), 64)));
  out.push_back(ints(W::collect_elements(level, I(-8), I(-8), I(8), I(8), 64)));
  out.push_back(n(W::terrain_height(level, radius, position)));
  out.push_back(n(W::terrain_height_below(level, radius, position)));
  out.push_back(n(W::terrain_height_at_point(level, position)));
  out.push_back(n(W::terrain_test_flag1(level, radius, position)));
  const auto sweep =
      W::terrain_sweep_circle(level, radius, position, delta, 3, true);
  out.push_back(sequence({n(sweep.time), normal(sweep.normal)}));
  const auto slide = W::world_slide_move(level, radius, position, delta, 3);
  out.push_back(
      sequence({n(slide.hit), ints(slide.position), normal(slide.normal)}));
  const auto ground = W::ground_step(level, radius, position, I(-2));
  out.push_back(
      sequence({ints(ground.position), n(ground.vz), n(ground.landed)}));
  const auto move = W::object_move_isolated(level, radius, position,
                                            {I(120), I(30), 0}, I(-2), 32);
  out.push_back(sequence({ints(move.position), n(move.vz), n(move.bits)}));
  if (!level.data().pieces.empty()) {
    level.set_piece_collision(0, false);
    level.set_piece_height(0, I(-7));
    out.push_back(n(level.get_piece_height(0)));
    out.push_back(n(W::terrain_height(level, radius, position)));
    level.set_piece_collision(0, true);
    out.push_back(n(W::terrain_height(level, radius, position)));
  }
  return sequence(out);
}
std::pair<std::string, std::string> scenario(const std::string &op,
                                             std::int64_t config,
                                             const std::string &args,
                                             std::size_t row) {
  if (op != "parse" && op != "queries")
    throw std::runtime_error("unknown operation");
  if (config < 0 || config > 15 || (op == "parse" && config != 0))
    throw std::runtime_error("invalid configuration");
  const auto parts = split(args, ';');
  if (parts.size() != 2 || integer(parts[0]) < 0 ||
      static_cast<std::uint64_t>(integer(parts[0])) != row)
    throw std::runtime_error("inconsistent case identity");
  const auto bytes = unhex(parts[1]);
  R::ScopedFpu context(R::Fpu(static_cast<R::Fpu::Model>(config / 4),
                              (config & 2) != 0, (config & 1) != 0));
  T::ParseLimits limits;
  try {
    W::Level level(T::decode(bytes, limits), limits.level_limits);
    const auto initial = snapshot(level.snapshot());
    const auto result = op == "queries" ? query_batch(level) : "[]";
    return {"ok", sequence({parse_limits(limits), initial, result,
                            snapshot(level.snapshot())})};
  } catch (const T::ResourceLimit &) {
    throw;
  } catch (const T::Error &) {
    return {"FormatError", "_"};
  }
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x5e5007cf2088570fULL;
constexpr std::size_t expected_rows = 133;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: terrain-file-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 2ULL * 1024 * 1024;
  constexpr std::size_t max_line = 8 * 1024;
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
  if (line() != "# terrain_file_vectors_v1")
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
    const auto actual =
        scenario(columns[0], integer(columns[1]), columns[2], rows);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::cerr << "terrain-file mismatch at row " << rows + 1 << " ("
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
    throw std::runtime_error("incomplete or changed terrain-file corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact terrain-file vectors passed; FNV1a64=" << std::hex
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
