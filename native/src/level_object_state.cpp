#include "champions/level_object_state.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace champions::simulation::rta::level_object_state {
namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
void append(Bytes &out, std::uint32_t value) {
  for (unsigned i = 0; i != 4; ++i) out.push_back(std::uint8_t(value >> (8 * i)));
}
void append_string(Bytes &out, const std::string &value) {
  append(out, std::uint32_t(value.size()));
  out.insert(out.end(), value.begin(), value.end());
}
struct Reader {
  const Bytes &data;
  std::size_t at = 0;
  std::uint32_t word() {
    require(at <= data.size() && data.size() - at >= 4, "truncated level-state directory");
    std::uint32_t result = 0;
    for (unsigned i = 0; i != 4; ++i) result |= std::uint32_t(data[at++]) << (8 * i);
    return result;
  }
  std::string string(std::size_t limit) {
    const auto length = word();
    require(length > 0 && length <= limit && at <= data.size() && length <= data.size() - at,
            "invalid level-state string");
    std::string result(data.begin() + at, data.begin() + at + length);
    at += length;
    require(result.find('\0') == std::string::npos, "NUL in level-state string");
    return result;
  }
};
void validate(const std::string &key, const std::vector<Record> &records) {
  require(!key.empty() && key.size() < 0x14 && key.find('\0') == std::string::npos,
          "invalid qualified level key");
  require(records.size() <= 65536, "too many saved object records");
  std::uint32_t previous = 0;
  bool first = true;
  for (const auto &record : records) {
    require(!record.cls.empty() && record.cls.size() <= 255 &&
                record.cls.find('\0') == std::string::npos,
            "invalid saved object class");
    require(record.fields.size() <= 1024, "saved object fields exceed native limit");
    require(first || record.ordinal > previous, "saved object ordinals are not unique and ordered");
    first = false;
    previous = record.ordinal;
  }
}
} // namespace

bool Directory::store(const std::string &key, std::vector<Record> records) {
  validate(key, records);
  for (auto &slot : slots_)
    if (slot.key == key) { slot.records = std::move(records); return true; }
  if (slots_.size() == READABLE) {
    // The first overflow names the gap; later misses replace its native body
    // without changing the name. Original act-row sizes are not in this schema.
    if (ghost_.key.empty()) ghost_.key = key;
    ghost_.records = std::move(records);
    ++overflow_writes_;
    return false;
  }
  slots_.push_back({key, std::move(records)});
  return true;
}
const Slot *Directory::lookup(const std::string &key) const {
  for (const auto &slot : slots_)
    if (slot.key == key) return &slot;
  return nullptr;
}
Bytes Directory::encode() const {
  Bytes out{'O', 'C', 'L', 'S', 1, 0, 0, 0};
  append(out, std::uint32_t(slots_.size()));
  auto write_slot = [&](const Slot &slot) {
    validate(slot.key, slot.records);
    append_string(out, slot.key);
    append(out, std::uint32_t(slot.records.size()));
    for (const auto &record : slot.records) {
      append(out, record.ordinal);
      append_string(out, record.cls);
      for (const auto component : record.position) append(out, component);
      append(out, record.angle);
      append(out, std::uint32_t(record.fields.size()));
      out.insert(out.end(), record.fields.begin(), record.fields.end());
      require(out.size() <= MAX_BYTES, "level-state directory exceeds native limit");
    }
  };
  for (const auto &slot : slots_) write_slot(slot);
  append(out, ghost_.key.empty() ? 0u : 1u);
  if (!ghost_.key.empty()) write_slot(ghost_);
  require(out.size() <= MAX_BYTES, "level-state directory exceeds native limit");
  return out;
}
Directory Directory::decode(const Bytes &data) {
  require(data.size() >= 16 && data.size() <= MAX_BYTES &&
              std::equal(data.begin(), data.begin() + 8,
                         Bytes{'O', 'C', 'L', 'S', 1, 0, 0, 0}.begin()),
          "unsupported level-state directory");
  Reader read{data, 8};
  const auto count = read.word();
  require(count <= READABLE, "level-state directory has more than ten readable slots");
  Directory result;
  auto read_slot = [&]() {
    auto key = read.string(0x13);
    const auto n = read.word();
    require(n <= 65536, "too many saved object records");
    std::vector<Record> records;
    records.reserve(n);
    for (std::uint32_t j = 0; j < n; ++j) {
      Record record;
      record.ordinal = read.word();
      record.cls = read.string(255);
      for (auto &component : record.position) component = read.word();
      const auto angle = read.word();
      require(angle <= 0xffffu, "invalid saved object angle");
      record.angle = std::uint16_t(angle);
      const auto length = read.word();
      require(length <= 1024 && read.at <= data.size() && length <= data.size() - read.at,
              "truncated saved object fields");
      record.fields.assign(data.begin() + read.at, data.begin() + read.at + length);
      read.at += length;
      records.push_back(std::move(record));
    }
    return Slot{std::move(key), std::move(records)};
  };
  for (std::uint32_t i = 0; i < count; ++i) {
    auto slot = read_slot();
    require(!result.lookup(slot.key), "duplicate level-state key");
    require(result.store(slot.key, std::move(slot.records)), "duplicate level-state key");
  }
  const auto ghost = read.word();
  require(ghost <= 1 && (!ghost || count == READABLE), "invalid overflow gap");
  if (ghost) {
    auto slot = read_slot();
    require(!result.lookup(slot.key), "overflow key duplicates a readable slot");
    result.store(slot.key, std::move(slot.records));
  }
  require(read.at == data.size(), "trailing level-state directory data");
  return result;
}
} // namespace champions::simulation::rta::level_object_state
