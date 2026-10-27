#pragma once
#include <cstddef>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace champions::simulation::rta::level_object_state {
using Bytes = std::vector<std::uint8_t>;

// Native, versioned companion data. A key is the qualified level name, not a
// resource path. The original reader examines only the first ten slots; the
// write-only overflow gap is represented by an unfindable ghost write.
struct Record {
  std::uint32_t ordinal = 0; // objects.ob file order, before factory filtering
  std::string cls;
  Bytes fields;
  std::array<std::uint32_t, 3> position{};
  std::uint16_t angle = 0;
};
struct Slot {
  std::string key;
  std::vector<Record> records;
};
class Directory {
public:
  static constexpr std::size_t READABLE = 10;
  static constexpr std::size_t MAX_BYTES = 32 * 1024 * 1024;
  // Returns false for the eleventh and later distinct key. As in the
  // original, those writes have no readable directory entry.
  bool store(const std::string &key, std::vector<Record> records);
  const Slot *lookup(const std::string &key) const;
  std::size_t count() const { return slots_.size(); }
  std::size_t overflow_writes() const { return overflow_writes_; }
  const Slot *overflow_gap() const { return ghost_.key.empty() ? nullptr : &ghost_; }
  Bytes encode() const;
  static Directory decode(const Bytes &);
private:
  std::vector<Slot> slots_;
  Slot ghost_;
  std::size_t overflow_writes_ = 0;
};
} // namespace champions::simulation::rta::level_object_state
