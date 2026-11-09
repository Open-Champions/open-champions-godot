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
//
// through_round 6 extends the same three prefixes with the audited round-5/6
// stages (research-loading-readiness@56a6760:docs/loading-arrival-r5.md and
// docs/loading-arrival-r6.md; audits r5r6-fix / r6-fix PASS). Round 4 stays
// the default and is unchanged. Every extra argument is the caller's
// synthetic control: names, quest values and the slot-19 value argument are
// never copied from a private script.
//
// through_round 7 extends round 6 with the PASS round-7 stages
// (research-loading-readiness@f8c780b:docs/loading-arrival-r7.md, audit
// research-loading-readiness-r7-fix2 PASS): the loader from 0x322e58 to
// loadscreen_stop or the hub shop's disc lookup (r7:199-263), setState with a
// clip and slot 19's value from +0x12c bit 0 (r7:270-351), and the delete
// that removes a name= value and frees the block (r7:353-387). Round 6 stays
// as its own pin; its "does not free" is withdrawn by r7:355-360.
struct ArrivalControl {
  int route = 0;
  int through_round = 4; // 4, 6 or 7; anything else is refused
  // Route 0 (r5:51-56, r6:37-54): 0x142320 argument, two 0x144160 names,
  // 17 removePlayerItem names. var_name/var_first are the read and write.
  std::string object_name;
  std::vector<std::string> deferred_names;
  std::vector<std::string> remove_items;
  // AutoProp slot 19 0x23d7ac value argument. Only 0 was executed
  // (r6:101-108, 181-185). Its source is not in the round-6 notes. Round 7
  // reads it from +0x12c bit 0 (r7:275-276), so this must stay 0 there.
  std::uint32_t slot19_value = 0;
  // Route 1 (r5:97-108, r6:115-125): 0x14222c lump argument, one show and
  // three hide names.
  std::string lump_name;
  std::string show_name;
  std::vector<std::string> hide_names;
  // Route 2 (r5:115-156, r6:127-149): 0x143f1c argument, the quest value the
  // post action writes after cut_start, the replacement post action and its
  // deleteMonster name. first_frame -1 runs the replacement; 0 or 1 runs the
  // first cut_runFrame with that argument instead.
  std::string cut_name;
  int var_after_cut = 0;
  std::string replacement_name;
  std::string delete_name;
  int first_frame = -1;
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
  // through_round 6 only: original callees the stages report reaching, in
  // order (grid, dtor, handle-clear, hide-mask, cut_start, frame calls).
  // Being listed means only that the research records the call being reached
  // and returning; it does not mean the callee's stores were applied.
  std::vector<std::uint32_t> callees;
  bool loader_promoted = false;
  // through_round 6 only: callees reached and returned from whose stores the
  // research does not record, so none of their stores are applied: slot 19
  // 0x23d7ac (56a6760:docs/loading-arrival-r6.md:107) and handle_clear
  // 0x1a1698 (r6:132). One entry per stage that reached them; the stage
  // continues only to the boundary the research gives after that call.
  // Round 7 also lists the capacity-0 anim_push 0x170838 (its +0x9e stores
  // were not checked, r7:303-306) and the six warm-up pass callees
  // (r7:215-222, 265-268).
  std::vector<std::uint32_t> unmodeled;
  // through_round 7 only: warm-up passes run, each storing the counter
  // g_warmupFrames 0x506048 one lower (r7:224-229).
  int warmup_passes = 0;
};
void run_arrival_prefix(builtins::Variables &, const ArrivalControl &, ArrivalPrefix &);
// 56a6760:docs/loading-arrival-r6.md:82-98 (model_buildHideMask 0x179550):
// complement of the double at model +8; only when the halfword at +0x10 has
// bit 0x2 do selected 16-byte records at (+0x21 << 4) or/clear it. A null
// model faults on the +0x10 load ("hide mask null model").
std::uint64_t arrival_hide_mask(const builtins::Memory &, std::uint32_t model,
                                std::uint32_t bits);
// Round-7 request selection (f8c780b:docs/loading-arrival-r7.md:131-197,
// 492-500). new_game_destination writes the destination 0x4fdc30, the
// request 0x41b6d0 and orientation 0xff at 0x41b720; the request is "hub",
// or "hub2" only when the decision, choice and mission-1 completion bits are
// all set at the current difficulty (1-5 executed). mission_one_start reads
// mission row 0 +8 and refuses a non-null +0x0c; it does not write the
// request (the menu copy is not executed). boot_request writes the front
// end's "intro" offline; online is refused. split_request copies the request
// into g_levelName and 0x44fe18, clears it, and names the route ordinal:
// hub 0, strife 1, blood1 2. Any other name is refused.
struct ArrivalRequest {
  std::string level;
  int route = -1;
};
std::string new_game_destination(builtins::Variables &);
std::string mission_one_start(const builtins::Memory &);
std::string boot_request(builtins::Memory &, bool offline);
ArrivalRequest split_request(builtins::Memory &);
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
