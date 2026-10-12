// Generic authored turn command replay. Expected columns never control
// native state, operations, or observed output.
#include "../tests/creature_turn_trace.hpp"
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
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
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
using namespace champions::test::creature_turn_trace;
std::pair<std::string, std::string> scenario(const std::string &op,
                                             std::int64_t config,
                                             const std::string &args,
                                             std::size_t row) {
  if (config < 0 || config > 15 ||
      std::find(operations().begin(), operations().end(), op) ==
          operations().end())
    throw std::runtime_error("unknown operation/configuration");
  const auto parts = split(args, ';');
  if (parts.size() != 2 || integer(parts[0]) != static_cast<std::int64_t>(row))
    throw std::runtime_error("invalid turn scenario identity");
  return {"ok", execute(static_cast<unsigned>(config), parts[1])};
}
// Independent original-model corpus coverage and full computed trace digest.
constexpr std::uint64_t expected_digest = 0x5b1957cda14e65f7ULL;
constexpr std::size_t expected_rows = 1568;
// Publish only a complete independently checked trace. Temporary files live in
// an exclusively created adjacent directory; failed runs preserve the target.
class TraceOutput {
  std::filesystem::path target, directory, temporary;
  std::ofstream stream;

public:
  explicit TraceOutput(const std::filesystem::path &destination)
      : target(destination) {
    auto parent = target.parent_path();
    if (parent.empty())
      parent = ".";
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(target)) ||
        (std::filesystem::exists(target) &&
         !std::filesystem::is_regular_file(target)))
      throw std::runtime_error(
          "trace destination must be a regular nonsymlink file");
    std::random_device random;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
      auto candidate =
          parent / (".creature-turn-trace-" + std::to_string(random()) + "-" +
                    std::to_string(random()));
      if (std::filesystem::create_directory(candidate)) {
        directory = std::move(candidate);
        break;
      }
    }
    if (directory.empty())
      throw std::runtime_error("cannot reserve trace temporary directory");
    temporary = directory / "trace.tsv";
    stream.open(temporary, std::ios::binary);
    if (!stream) {
      std::error_code ec;
      std::filesystem::remove(directory, ec);
      throw std::runtime_error("cannot open trace temporary file");
    }
  }
  TraceOutput(const TraceOutput &) = delete;
  TraceOutput &operator=(const TraceOutput &) = delete;
  ~TraceOutput() {
    stream.close();
    std::error_code ec;
    std::filesystem::remove(temporary, ec);
    std::filesystem::remove(directory, ec);
  }
  void append(const std::string &record) {
    stream << record;
    if (!stream)
      throw std::runtime_error("trace write failed");
  }
  void publish() {
    stream.flush();
    if (!stream)
      throw std::runtime_error("trace write failed");
    stream.close();
    if (!stream)
      throw std::runtime_error("trace close failed");
    std::filesystem::rename(temporary, target);
  }
};
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: creature-turn-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 16ULL * 1024 * 1024;
  constexpr std::size_t max_line = 64 * 1024;
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
  std::optional<TraceOutput> output;
  if (args.size() == 2)
    output.emplace(args[1]);
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
  if (line() != "# creature_turn_vectors_v1")
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
      std::cerr << "creature-turn mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (output)
      output->append(record);
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
    throw std::runtime_error("incomplete or changed creature-turn corpus");
  if (output)
    output->publish();
  std::cout << rows
            << " exact creature-turn vectors passed; FNV1a64=" << std::hex
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
