#pragma once
#include "champions/script_builtins.hpp"
#include <functional>
#include <optional>

namespace champions::simulation::rta::loading_equipment {
// Every original field/condition in this API is pinned to
// research-loading-readiness@d6f035fa:docs/loading-arrival-r2.md, cited below.
// Reference addresses operate on the real binder memory, never host pointers.
class Runtime {
public:
  explicit Runtime(builtins::Memory &memory) : m(memory) {}
  // 58-73: bounded publication slices only, not construction/equip selection.
  void publish_player(std::uint32_t player, unsigned index);
  void publish_slot(unsigned index, unsigned selected_slot, std::uint32_t item);
  // 75-97: seven cache routes, exercised indices 0/1 only.
  static std::optional<std::uint32_t> cache(unsigned slot, unsigned index);
  std::uint32_t header() const; // 140-144, requires published inputs
  bool published(std::uint32_t, std::size_t) const;
  // 101-118: caller must resolve actual metadata/file location and submit real
  // device work. No formatter, header search, transfer or fixup stand-in exists.
  struct Read { std::uint32_t source, destination, completion, offset, size; };
  std::function<void(const Read &)> transfer;
  void submit(std::uint32_t cache, std::uint32_t metadata,
              std::uint32_t file_sector, bool four_frames_elapsed, bool disc_busy);
  // 120-128: device results are supplied by the actual service, never offline mode.
  // Streaming callers supply observed remaining bytes after their service call.
  void poll(bool busy, bool error, std::optional<std::uint32_t> remaining = {});
  std::function<void()> retry;
  // 130-138: transformations are required services; absent services refuse.
  // Validation checks usable transformed data, not pointer/valid-bit presence.
  std::function<void(std::uint32_t)> fixup_base, fixup_model;
  std::function<bool(std::uint32_t, std::uint32_t)> valid_resource;
  // Scanner formatter 87-97 is independently unverified; never synthesize a name.
  std::function<std::string(std::uint32_t)> format;
  struct Pair { bool ready = false; std::uint32_t model = 0, base = 0; };
  Pair lookup(std::uint32_t cache, const std::string &identity);
private:
  builtins::Memory &m;
  void require(std::uint32_t, std::size_t) const;
};
}
