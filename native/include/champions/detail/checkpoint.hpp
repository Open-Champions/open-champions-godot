#pragma once
// Internal closed-controller checkpoint codec. Never encode host object bytes.
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace champions::simulation::rta::detail {
struct Checkpoint {
  using Bytes = std::vector<std::uint8_t>;
  enum class Storage { Inline, Split, Indexed };
  Bytes bytes;
  // Internal partition only, not a new on-disk schema. Inline remains the
  // default and retains the original checkpoint byte order exactly.
  Bytes history_bytes;
  std::vector<std::uint32_t> history_counts;
  // Explicit opt-in schema gate. Legacy callers continue to refuse cache state.
  bool control_anim_cache = false;
  // Independent authored continuation schema. Never enabled by legacy saves.
  bool play_continuation = false;
  bool reading = false;
  std::size_t at = 0;
  unsigned value_depth = 0, value_nodes = 0, value_bytes = 0;
  bool value_group = false;
  static constexpr std::size_t budget = 32u * 1024u * 1024u;
  explicit Checkpoint() = default;
  explicit Checkpoint(Storage storage)
      : split_(storage != Storage::Inline),
        indexed_(storage == Storage::Indexed) {}
  explicit Checkpoint(Bytes input) : bytes(std::move(input)), reading(true) {
    require(bytes.size() <= budget, "checkpoint byte budget");
  }
  Checkpoint(Bytes state, Bytes history, bool append = false)
      : bytes(std::move(state)), history_bytes(std::move(history)),
        reading(true), split_(true), append_(append) {
    require(bytes.size() <= budget &&
                history_bytes.size() <= budget - bytes.size(),
            "checkpoint byte budget");
  }
  // Restore only current values. Historical containers remain inaccessible
  // outside Scene until a full paired-stream restore succeeds.
  Checkpoint(Bytes state, std::vector<std::uint32_t> counts)
      : bytes(std::move(state)), history_counts(std::move(counts)),
        reading(true), split_(true), deferred_(true) {
    require(bytes.size() <= budget && history_counts.size() <= 32,
            "checkpoint deferred budget");
  }
  bool deferred() const { return deferred_; }
  std::size_t deferred_records() const { return deferred_records_; }
  template <class T> std::size_t records(const std::vector<T> &v) const {
    const auto it = counts_.find(&v);
    return it == counts_.end() ? v.size() : it->second;
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
      const auto used = bytes.size() + (split_ ? history_bytes.size() : 0);
      require(used <= budget && width <= budget - used,
              "checkpoint byte budget");
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
    auto n = count(v.size(), vector_cap_);
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
  // Mark retained historical records explicitly. Their counts and indices
  // still constrain continuation; splitting storage does not waive those rules.
  // Paired-stream restore decodes eagerly. Indexed current-only restore retains
  // validated counts. Scene must materialize before complete observations;
  // the closed owner may continue with reserved prefixes and owned suffix logs.
  // Nested histories are a codec error.
  template <class... T> void history(T &...v) {
    if (deferred_) {
      (defer(v), ...);
      return;
    }
    if (indexed_)
      (index(v), ...);
    if (!split_) {
      fields(v...);
      return;
    }
    require(!in_history_, "nested checkpoint history");
    in_history_ = true;
    bytes.swap(history_bytes);
    std::swap(at, history_at_);
    try {
      if (reading && append_)
        (append(v), ...);
      else
        fields(v...);
    } catch (...) {
      bytes.swap(history_bytes);
      std::swap(at, history_at_);
      in_history_ = false;
      throw;
    }
    bytes.swap(history_bytes);
    std::swap(at, history_at_);
    in_history_ = false;
  }
  // Cache histories have a smaller closed count cap than legacy histories.
  // Scope the outer vector count check before any resize; defaults are
  // unchanged.
  template <class T> void bounded_history(std::vector<T> &v, std::size_t cap) {
    const auto previous = vector_cap_;
    vector_cap_ = cap;
    try {
      history(v);
      require(records(v) <= cap, "checkpoint bounded history count");
    } catch (...) {
      vector_cap_ = previous;
      throw;
    }
    vector_cap_ = previous;
  }
  void finish() const {
    require(!reading || at == bytes.size(), "trailing checkpoint bytes");
    require(!reading || !split_ || deferred_ ||
                history_at_ == history_bytes.size(),
            "trailing checkpoint history bytes");
    require(!deferred_ || count_at_ == history_counts.size(),
            "trailing checkpoint history counts");
  }

private:
  std::size_t vector_cap_ = 65536;
  bool split_ = false, in_history_ = false;
  bool indexed_ = false, deferred_ = false, append_ = false;
  template <class T> void append(T &) {
    require(false, "appended history must be a vector");
  }
  template <class T> void append(std::vector<T> &v) {
    std::vector<T> suffix;
    value(suffix);
    require(v.size() <= 65536 && suffix.size() <= 65536 - v.size(),
            "checkpoint appended history budget");
    v.insert(v.end(), std::make_move_iterator(suffix.begin()),
             std::make_move_iterator(suffix.end()));
  }
  std::size_t history_at_ = 0;
  std::size_t count_at_ = 0, deferred_records_ = 0;
  std::map<const void *, std::size_t> counts_;
  template <class T> void index(const std::vector<T> &v) {
    require(v.size() <= 65536 && history_counts.size() < 32,
            "checkpoint history index budget");
    history_counts.push_back(std::uint32_t(v.size()));
  }
  template <class T> void index(const T &) {
    require(false, "indexed history must be a vector");
  }
  template <class T> void defer(T &) {
    require(false, "deferred history must be a vector");
  }
  template <class T> void defer(std::vector<T> &v) {
    require(count_at_ < history_counts.size(), "missing history count");
    const auto n = history_counts[count_at_++];
    require(n <= 65536 && n <= 65536 - deferred_records_,
            "checkpoint deferred record budget");
    deferred_records_ += n;
    counts_.emplace(&v, n);
    v.clear();
  }
};
struct CheckpointAccess {
  template <class T> static void history(T &object, Checkpoint &a) {
    object.checkpoint_history(a);
  }
  template <class T> static void run(T &object, Checkpoint &a) {
    object.checkpoint(a);
  }
};
} // namespace champions::simulation::rta::detail
