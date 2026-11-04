#pragma once
#include "champions/script_builtins.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::loading_equipment {
// Every original field/condition in this API is pinned to
// research-loading-readiness@d6f035fa:docs/loading-arrival-r2.md, except
// resource fixups at 2550d34:docs/loading-arrival-r3.md:11-55 and the round-4
// selection/prefix below, cited at each operation.
// Reference addresses operate on the real binder memory, never host pointers.
// research-loading-readiness@366b65a:docs/starting-equipment-r4.md:43-56.
// Executed initializer controls only: bodies 0-11, seeds 7 and 20973, kinds
// -1 and 1. Other inputs stay refused. These are definition addresses, not
// item-instance bytes and not a destination's saved inventory.
struct StartingSelection {
  int body = 0;
  int class_index = 0;
  unsigned weapon_slot = 0;
  std::uint32_t weapon_definition = 0;
  unsigned armor_slots[3] = {4, 10, 12};
  std::uint32_t armor_definitions[3] = {};
  std::uint32_t armor_header_deltas[3] = {};
  std::uint32_t armor_caches[3] = {};
};
StartingSelection starting_selection(int body, std::uint32_t seed, int kind);
// 366b65a:docs/starting-equipment-r4.md:76-83. Player counts 1 and 2 with
// override 0 or 1 are the executed combinations. Secondary is always present.
struct HeaderChoice {
  std::uint32_t primary = 0;
  std::uint32_t primary_groups = 0;
  std::uint32_t secondary = 0;
  std::uint32_t secondary_groups = 0;
};
HeaderChoice header_choice(std::uint32_t player_count, int override_byte);
std::uint32_t header_group_count(std::uint32_t identifier);
// 366b65a:docs/starting-equipment-r4.md:104-124. Single-player primary header,
// slots 4, 10 and 12 only.
std::uint32_t armor_header_delta(int body, unsigned slot);
// 366b65a:docs/starting-equipment-r4.md:134-135. Same span the scanner submits.
struct CacheSpan { std::uint32_t offset = 0, size = 0; };
CacheSpan cache_span(std::uint32_t begin, std::uint32_t split, std::int16_t tail);
// 366b65a:docs/starting-equipment-r4.md:156-160. The caller supplies the
// observed short, the flag-bit-36 condition and the variant. No item bytes.
std::int32_t generic_weapon_index(std::int16_t definition_short, bool flag_bit_36,
                                  std::int32_t variant);
constexpr std::uint32_t GENERIC_ROW_BASE = 0x3e9ce0;
constexpr std::uint32_t GENERIC_SOURCE = 0x520030;
// 366b65a:docs/loading-arrival-r4.md:78-91. Clears queue counts, parity, the
// save-inhibit word and the post-action first byte. Record bytes stay.
void reset_arrival_queue(builtins::Memory &);
// 366b65a:docs/loading-arrival-r4.md:154-179. route is an explicit ordinal.
// Arguments are the caller's synthetic controls, not a private script.
// The call fills `out`, then throws the unexecuted boundary. Route ordinals
// outside 0..2 throw "route state" and do not invent an order.
struct ArrivalControl {
  int route = 0;
  bool completion_set = false;
  bool choice_bit = false;
  bool quest_list_full = false;
  int mission = 0;
  int save_inhibit = 0;
  int available_on = 1;
  std::string quest_title;
  std::string quest_text;
  std::string var_name;
  int var_first = 0;
  int var_second = 0;
  std::string post_name;
};
struct ArrivalPrefix {
  int missing_entry = 0;
  std::vector<std::uint32_t> executed;
  std::uint32_t boundary = 0;
  const char *refusal = "";
  bool post_action = false;
  int quest_count = 0;
};
void run_arrival_prefix(builtins::Variables &, const ArrivalControl &, ArrivalPrefix &);
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
  // device work. No formatter, header search or transfer stand-in exists.
  struct Read { std::uint32_t source, destination, completion, offset, size; };
  std::function<void(const Read &)> transfer;
  void submit(std::uint32_t cache, std::uint32_t metadata,
              std::uint32_t file_sector, bool four_frames_elapsed, bool disc_busy);
  // 120-128: device results are supplied by the actual service, never offline mode.
  // Streaming callers supply observed remaining bytes after their service call.
  void poll(bool busy, bool error, std::optional<std::uint32_t> remaining = {});
  std::function<void()> retry;
  // 2550d34:docs/loading-arrival-r3.md:11-55: built-in bounded fixups.
  // Optional paired service overrides retained for explicit adapters/tests.
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
