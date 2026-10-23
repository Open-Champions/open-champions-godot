#pragma once

#include "champions/archive.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace champions::resources {

// A session-owned res_find catalog. The caller supplies bytes from the selected
// disc at run time; no archive data or derived names are embedded in the client.
// Replacing a lump invalidates its old handles by generation.
struct Handle {
  std::uint64_t generation = 0;
  std::string lump;
  std::string entry;
};

class Manager {
public:
  void load_lmp(const std::string &name, Bytes image);
  void unload(const std::string &name);
  std::optional<Handle> find(const std::string &lump, const std::string &entry) const;
  std::optional<Bytes> bytes(const Handle &handle) const;
  std::size_t loaded_lumps() const { return lumps_.size(); }

private:
  struct Lump {
    std::shared_ptr<std::vector<std::uint8_t>> image;
    std::map<std::string, Entry> entries;
    std::uint64_t generation = 0;
  };
  std::map<std::string, Lump> lumps_;
  std::uint64_t next_generation_ = 1;
};

} // namespace champions::resources
