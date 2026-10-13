#pragma once
// Authored bounded integer/list transport. The explicit input describes state
// and commands; no corpus-specific recipe or expected value drives execution.
#include "champions/monsters_1.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace champions::test::monsters_1_transport {
namespace M = simulation::rta::monsters_1;
namespace R = simulation::rta;
inline std::string sequence(const std::vector<std::string> &v) {
  std::string out = "[";
  for (const auto &x : v) {
    if (out.size() > 1)
      out += ',';
    out += x;
  }
  return out + ']';
}
template <class T> std::string n(T value) { return std::to_string(value); }
struct Value {
  bool list = false, negative = false;
  std::uint64_t magnitude = 0;
  std::vector<Value> items;
  const std::vector<Value> &array(std::size_t count) const {
    if (!list || items.size() != count)
      throw std::runtime_error("invalid DSL shape");
    return items;
  }
  const std::vector<Value> &bounded_array(std::size_t maximum) const {
    if (!list || items.size() > maximum)
      throw std::runtime_error("DSL list budget or shape");
    return items;
  }
  template <class T> T integer() const {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
    if (list)
      throw std::runtime_error("expected DSL integer");
    if constexpr (std::is_signed_v<T>) {
      const auto max =
          static_cast<std::uint64_t>(std::numeric_limits<T>::max());
      if (magnitude > max + (negative ? 1ULL : 0ULL))
        throw std::runtime_error("DSL signed integer range");
      if (negative && magnitude == max + 1ULL)
        return std::numeric_limits<T>::min();
      const auto positive = static_cast<T>(magnitude);
      return negative ? static_cast<T>(-positive) : positive;
    } else {
      if (negative || magnitude > std::numeric_limits<T>::max())
        throw std::runtime_error("DSL unsigned integer range");
      return static_cast<T>(magnitude);
    }
  }
  std::string canonical() const {
    if (!list)
      return (negative ? "-" : "") + n(magnitude);
    std::vector<std::string> out;
    for (const auto &v : items)
      out.push_back(v.canonical());
    return sequence(out);
  }
};
class Parser {
  const std::string &text;
  std::size_t at = 0, nodes = 0;
  Value value(unsigned depth) {
    if (depth > 10 || ++nodes > 8192)
      throw std::runtime_error("DSL depth/node budget");
    if (at == text.size())
      throw std::runtime_error("truncated DSL");
    Value v;
    if (text[at] == '[') {
      v.list = true;
      ++at;
      if (at < text.size() && text[at] == ']') {
        ++at;
        return v;
      }
      for (;;) {
        if (v.items.size() == 2048)
          throw std::runtime_error("DSL list budget");
        v.items.push_back(value(depth + 1));
        if (at == text.size())
          throw std::runtime_error("truncated DSL list");
        if (text[at] == ']') {
          ++at;
          return v;
        }
        if (text[at++] != ',')
          throw std::runtime_error("invalid DSL separator");
      }
    }
    if (text[at] == '-') {
      v.negative = true;
      ++at;
    }
    const auto start = at;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
      const unsigned digit = static_cast<unsigned>(text[at++] - '0');
      if (v.magnitude >
          (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
        throw std::runtime_error("DSL integer overflow");
      v.magnitude = v.magnitude * 10 + digit;
    }
    if (at == start || (at - start > 1 && text[start] == '0') ||
        (v.negative && v.magnitude == 0))
      throw std::runtime_error("noncanonical DSL integer");
    return v;
  }

public:
  explicit Parser(const std::string &source) : text(source) {}
  Value parse() {
    auto result = value(0);
    if (at != text.size())
      throw std::runtime_error("trailing DSL bytes");
    return result;
  }
};
inline std::string hex(const std::string &s) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : s) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}
inline std::string text(const std::string &s) { return "s" + hex(s); }
template <class T> std::string integers(const T &values) {
  std::vector<std::string> out;
  for (auto v : values)
    out.push_back(n(v));
  return sequence(out);
}
} // namespace champions::test::monsters_1_transport
