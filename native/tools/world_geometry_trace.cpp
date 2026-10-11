// Authored static WORLD geometry replay: input hex drives decoding; expected
// columns never influence native execution or query state.
#include "../tests/world_geometry_trace.hpp"
#include "champions/world_geometry.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
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
namespace G = champions::presentation::rta::world_geometry;
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
using namespace champions::test::world_geometry_trace;
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

std::pair<std::string, std::string> scenario(const std::string &op,
                                             std::int64_t config,
                                             const std::string &args,
                                             std::size_t row) {
  if (op != "decode" || config != 0)
    throw std::runtime_error("unknown geometry operation/configuration");
  const auto parts = split(args, ';');
  if (parts.size() != 2 || integer(parts[0]) != static_cast<std::int64_t>(row))
    throw std::runtime_error("invalid geometry scenario identity");
  const auto bytes = unhex(parts[1]);
  const G::Limits cap;
  try {
    const auto result = G::decode(bytes, cap);
    return {"ok", sequence({limits(cap), geometry(result)})};
  } catch (const G::ResourceLimit &) {
    throw;
  } catch (const G::Unsupported &) {
    return {"Unsupported", "_"};
  } catch (const G::Error &) {
    return {"FormatError", "_"};
  }
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x0999b159e2f70999ULL;
constexpr std::size_t expected_rows = 73;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: world-geometry-trace VECTORS.tsv [TRACE.tsv]");
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
  if (line() != "# world_geometry_vectors_v1")
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
      std::cerr << "world-geometry mismatch at row " << rows + 1 << " ("
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
    throw std::runtime_error("incomplete or changed world-geometry corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact world-geometry vectors passed; FNV1a64=" << std::hex
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
