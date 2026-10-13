#pragma once
// Internal closed-controller checkpoint codec. Never encode host object bytes.
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
namespace champions::simulation::rta::detail {
struct Checkpoint {
  using Bytes = std::vector<std::uint8_t>;
  Bytes bytes;
  bool reading = false;
  std::size_t at = 0;
  unsigned value_depth = 0, value_nodes = 0, value_bytes = 0;
  bool value_group = false;
  static constexpr std::size_t budget = 32u * 1024u * 1024u;
  explicit Checkpoint() = default;
  explicit Checkpoint(Bytes input) : bytes(std::move(input)), reading(true) {
    require(bytes.size() <= budget, "checkpoint byte budget");
  }
  static void require(bool ok, const char *why) {
    if (!ok)
      throw std::runtime_error(why);
  }
  void word(std::uint64_t &v, unsigned width) {
    if (reading) {
      require(at <= bytes.size() && width <= bytes.size() - at,
              "truncated checkpoint");
      v = 0;
      for (unsigned i = 0; i < width; ++i)
        v |= std::uint64_t(bytes[at++]) << (8 * i);
    } else {
      require(width <= budget - bytes.size(), "checkpoint byte budget");
      for (unsigned i = 0; i < width; ++i)
        bytes.push_back(std::uint8_t(v >> (8 * i)));
    }
  }
  template <class T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
  void value(T &v) {
    constexpr unsigned width = std::is_same<T, bool>::value ? 1 : 8;
    std::uint64_t u = std::uint64_t(v);
    word(u, width);
    if (!reading)
      return;
    if constexpr (std::is_same<T, bool>::value) {
      require(u <= 1, "checkpoint boolean");
      v = u != 0;
    } else if constexpr (std::is_signed<T>::value) {
      const auto s = (u >> 63) ? -1 - std::int64_t(~u) : std::int64_t(u);
      require(s >= std::numeric_limits<T>::min() &&
                  s <= std::numeric_limits<T>::max(),
              "checkpoint signed range");
      v = T(s);
    } else {
      require(u <= std::numeric_limits<T>::max(), "checkpoint unsigned range");
      v = T(u);
    }
  }
  template <class T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
  void value(T &v) {
    auto u = std::uint32_t(v);
    value(u);
    if (reading) {
      using Underlying = std::underlying_type_t<T>;
      require(u <= std::uint64_t(std::numeric_limits<Underlying>::max()),
              "checkpoint enum range");
      v = T(Underlying(u));
    }
  }
  std::size_t count(std::size_t n, std::size_t cap = 65536) {
    std::uint64_t u = n;
    word(u, 4);
    require(u <= cap && (!reading || u <= bytes.size() - at),
            "checkpoint collection budget");
    return std::size_t(u);
  }
  template <class T> void value(std::vector<T> &v) {
    auto n = count(v.size());
    if (reading)
      v.resize(n);
    for (auto &x : v)
      value(x);
  }
  template <class T, std::size_t N> void value(std::array<T, N> &v) {
    for (auto &x : v)
      value(x);
  }
  template <class T> void value(std::optional<T> &v) {
    bool present = bool(v);
    value(present);
    if (reading) {
      if (present)
        v.emplace();
      else
        v.reset();
    }
    if (present)
      value(*v);
  }
  template <class A, class B> void value(std::pair<A, B> &v) {
    fields(v.first, v.second);
  }
  template <class K, class V> void value(std::map<K, V> &v) {
    auto n = count(v.size());
    if (reading) {
      v.clear();
      std::optional<K> previous;
      for (std::size_t i = 0; i < n; ++i) {
        K k{};
        V x{};
        fields(k, x);
        require(!previous || *previous < k, "checkpoint map order");
        previous = k;
        v.emplace(k, std::move(x));
      }
    } else
      for (auto &e : v) {
        auto k = e.first;
        fields(k, e.second);
      }
  }
  template <class C> void value(std::basic_string<C> &v) {
    auto n = count(v.size(), 4096);
    if (reading)
      v.resize(n);
    for (auto &c : v) {
      std::uint32_t u = std::make_unsigned_t<C>(c);
      value(u);
      if (reading) {
        require(u <= std::numeric_limits<std::make_unsigned_t<C>>::max(),
                "checkpoint character");
        if constexpr (std::is_signed<C>::value) {
          c = u <= std::uint64_t(std::numeric_limits<C>::max())
                  ? C(u)
                  : C(-1 -
                      std::int64_t(
                          std::numeric_limits<std::make_unsigned_t<C>>::max() -
                          u));
        } else {
          c = C(u);
        }
      }
    }
  }
  template <class T, std::enable_if_t<!std::is_integral<T>::value &&
                                          !std::is_enum<T>::value,
                                      int> = 0>
  void value(T &v) {
    checkpoint_fields(*this, v);
  }
  template <class... T> void fields(T &...v) { (value(v), ...); }
  void finish() const {
    require(!reading || at == bytes.size(), "trailing checkpoint bytes");
  }
};
struct CheckpointAccess {
  template <class T> static void run(T &object, Checkpoint &a) {
    object.checkpoint(a);
  }
};
} // namespace champions::simulation::rta::detail
