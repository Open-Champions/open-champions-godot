#pragma once
#include "champions/items.hpp"
#include <array>
#include <bitset>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Session stock for shop_openOrStock 0x1c7af8.
// research-vendor-stock@fdf6752:docs/spec/vendor-stock.md:225-342.
// Synthetic descriptors and definitions only. Retail bytes are not stored here.
namespace champions::simulation::rta::items {
class VendorRefused : public Error {
public:
  using Error::Error;
};

// shop_buy 0x1d1240 / sell arm of shop_onMsg 0x1c5d28.
// docs/spec/vendor-stock.md:230-254.
std::int32_t vendor_buy_price(std::int32_t value, std::int32_t category,
                              std::uint64_t record_flags, std::int32_t slot);
std::int32_t vendor_sell_credit(std::int32_t value, std::int32_t count,
                                std::int32_t slot, bool name_has_stem,
                                std::uint64_t definition_flags);
std::pair<std::int32_t, bool> vendor_buy_gold(std::int32_t gold,
                                              std::int32_t price);
std::int32_t vendor_sell_gold(std::int32_t gold, std::int32_t paid);

// 0x1c7b48-0x1c7cd8. Second read defaults to the first. Refuses a second
// read outside 0..12 or a difficulty outside 1..5.
struct VendorBudget {
  Raw scale = 0;
  std::int32_t low = 0, high = 0;
  std::array<std::int32_t, 3> steps{};
  std::array<std::int32_t, 3> caps{};
  std::int32_t special_cap = 0;
  std::array<std::uint32_t, 2> other_pointer_cells{};
};
VendorBudget vendor_budget(std::int32_t store_level, std::int32_t difficulty,
                           std::int32_t second_read);

inline constexpr std::array<std::uint32_t, 4> kShopDestinationAddresses{
    0x554678u, 0x554680u, 0x554688u, 0x554698u};
// stock(0,1,1), stock(0,1,1), stock(0,2,1), stock(0,3,1).
// docs/spec/vendor-stock.md:18-22. Slot and the third argument do not
// specialize items (docs/spec/vendor-stock.md:29-30).
inline constexpr std::array<int, 4> kShopDestinationModes{1, 1, 2, 3};
inline constexpr const char *kStoreLevelName = "storeLevel";

struct VendorFixedText {
  std::array<std::string, 4> weapons;
  std::string other;
  // Descriptor text already resolved from the pointer cell. Empty refuses.
  std::function<std::string(std::uint32_t address)> cell;
};

// The session's quest-variable table (0x86da80, selector 0x538750). read is
// the creating read: a missing name is created at 0 and 0 is returned
// (docs/spec/vendor-stock.md:305-311). VendorStock keeps no copy.
struct VendorQuestVariables {
  std::function<std::int32_t(const std::string &)> read;
  std::function<bool(const std::string &)> present;
  std::function<void(const std::string &, std::int32_t)> store;
};

// Bytes the item generator leaves in a record it never writes (+0x06, +0x22,
// +0x44, the values of empty modifier slots). docs/spec/items.md:83-95 says
// they hold an earlier candidate's float temporaries; nothing reconstructs
// them. A probe that executed the original can supply them, as
// ref_vendor_stock.stock_plan receives executed raw records
// (docs/spec/vendor-stock.md:197-202). Without a source they are unknown.
struct VendorResidueCall {
  int category = 0;
  bool special = false;
  // Position of this candidate in candidates(category); unused for specials.
  std::size_t ordinal = 0;
};
using VendorResidueSource = std::function<std::array<std::uint8_t, ITEM_SIZE>(
    const VendorResidueCall &, const Item &)>;

struct VendorLoad {
  bool matched = false;
  bool generated = false;
  bool kept = false;
  bool refused = false;
  // A stepped pass that has started and not finished. Nothing is published.
  bool pending = false;
  std::string reason;
  bool resource_requested = false;
  bool resource_completed = false;
  std::string resource_reason;
  int mode = -1;
  std::array<std::uint32_t, 3> counts{};
  int armour_scan_rejections = 0;
  // The two storeLevel reads and the budget they produced.
  std::array<std::int32_t, 2> store_level_reads{};
  VendorBudget budget{};
  // Diagnostics only: outer rejections and generator failures per list.
  std::array<int, 3> rejected{};
  std::array<int, 3> failed{};
  std::array<int, 3> bit40_rejected{};
  std::array<bool, 3> special_roll{};
  std::array<std::int32_t, 3> special_budget{};
  std::uint32_t shared_seed = 0;
  std::uint32_t cosmetic_seed = 0;
};

// Three stock lists. Mode 0 keeps them. Modes 1, 2 and 3 regenerate them.
// shop_restockByGold 0x1c8a00 is not called: its only sites are inside the
// unconstructed ShopDaemon (docs/spec/vendor-stock.md:256-276).
//
// A pass runs in steps (begin, then step until it is no longer pending), as
// the reference scheduler is a resumable generator
// (docs/spec/vendor-stock.md:217-219). Each step is one parser call, one
// generator attempt, one special decision, or the sort. Results are kept in a
// working copy and published only when the pass completes.
class VendorStock {
public:
  static constexpr std::uint32_t kResource = 0x538678u;
  static constexpr std::size_t kSlots = 64;
  static constexpr std::size_t kRecord = ITEM_SIZE;
  using Bytes = std::array<std::uint8_t, kRecord>;
  using Known = std::bitset<kRecord>;

  explicit VendorStock(Tables tables);

  // Every input below starts unset; a pass refuses until it is supplied.
  // docs/spec/vendor-stock.md:36-43,165-171,330-332.
  void set_seeds(std::uint32_t shared, std::uint32_t cosmetic);
  void set_difficulty(std::int32_t difficulty);
  void set_party(std::int32_t num_players, std::vector<std::int32_t> classes);
  void set_fixed(VendorFixedText fixed);
  void set_destinations(std::array<std::string, 4> identifiers);
  void bind_quest_variables(VendorQuestVariables variables);
  // Adapter for level_flow::State::quest_values and similar maps.
  void bind_quest_values(std::map<std::string, std::int32_t> *values);
  void set_executed_residue(VendorResidueSource source);

  // Creating read of storeLevel through the bound table.
  std::int32_t read_store_level();
  bool store_level_present() const;
  // Authored literal setv values only. docs/spec/vendor-stock.md:313-317.
  void apply_literal_setv(const std::string &level, std::size_t index);
  // Throws. Also marks storeLevel unestablished so that the next stock
  // pass refuses until a literal setv or an act clear replaces the value.
  void apply_non_literal_setv(const std::string &level);
  // A script store of storeLevel that was not classified as an authored
  // literal (the VM call does not show whether its value was a pushed
  // constant). Blocks the next pass like the non-literal store.
  void note_unclassified_store(const std::string &why);
  bool store_level_unestablished() const { return unestablished_.has_value(); }
  void literal_current_on_visit(const std::string &level) const;
  // Called after the session's own act clear (level_flow::begin_change with
  // reset_act). The variable itself is zeroed by that clear, not here; this
  // only lifts the block that a non-literal store left.
  // docs/spec/vendor-stock.md:45-47,320.
  void on_act_reset(bool reset_act);
  void render_shop() const;
  void save_stock() const;

  // Probe images: fully specified synthetic stock contents and name-region
  // records, as the research fills them before a pass.
  void fill_storage(std::uint8_t byte);
  void plant_record(int category, std::size_t index, const Item &item);
  void plant_name_record(std::size_t index, const Bytes &raw);

  VendorLoad begin(int mode);
  VendorLoad begin_load(const std::string &destination);
  VendorLoad step();
  bool pending() const { return pass_.has_value(); }
  // Discards an unfinished pass. Nothing it computed is published.
  void cancel() { pass_.reset(); }
  // begin/begin_load then step until finished. Every retry this port runs
  // is bounded (six duplicate rejections, then the seventh passes), so this
  // terminates; it is for callers that do not poll.
  VendorLoad open_or_stock(int mode);
  VendorLoad on_load(const std::string &destination);

  std::uint32_t count(int category) const;
  const Item &item(int category, std::size_t index) const;
  const Bytes &record_bytes(int category, std::size_t index) const;
  const Known &record_known(int category, std::size_t index) const;
  std::uint64_t generations() const { return generations_; }
  std::uint64_t steps_taken() const { return steps_; }
  std::uint32_t shared_seed() const { return shared_seed_.value_or(0); }
  std::uint32_t cosmetic_seed() const { return cosmetic_seed_.value_or(0); }
  const std::vector<Item> &armour_candidates() const { return seen_[1]; }
  // Every successful ordinary generator result of the last finished pass.
  const std::vector<Item> &candidates(int category) const;

  static std::vector<std::int32_t> literals(const std::string &level);

private:
  struct Slot {
    Item item;
    Bytes raw{};
    Known known{};
  };
  using Lists = std::array<std::vector<Slot>, 3>;
  enum class Phase { Fixed, Ordinary, Special, Sort };
  struct Pass {
    VendorLoad out;
    Lists slots;
    std::array<std::uint32_t, 3> counts{};
    std::array<std::vector<Item>, 3> seen;
    Rng rng;
    std::uint32_t cosmetic = 0;
    Game game;
    int category = 0;
    Phase phase = Phase::Fixed;
    std::vector<std::string> fixed;
    std::size_t fixed_index = 0;
    int progress = 0, retries = 0;
  };

  VendorLoad refuse_pass(const std::string &reason);
  void enter_category(Pass &);
  void step_fixed(Pass &);
  void step_ordinary(Pass &);
  void step_special(Pass &);
  void step_sort(Pass &);
  VendorLoad finish_pass();
  Slot generated_slot(const Item &, const VendorResidueCall &) const;
  enum class Same { No, Yes, Unknown };
  Same scan(const Pass &, int category, std::uint32_t count, const Slot &candidate) const;
  void put(Pass &, int category, std::uint32_t index, Slot slot) const;
  const VendorQuestVariables &variables() const;

  Tables tables_;
  VendorFixedText fixed_{};
  std::array<std::string, 4> destinations_{};
  bool destinations_set_ = false;
  std::optional<std::int32_t> difficulty_;
  std::optional<std::pair<std::int32_t, std::vector<std::int32_t>>> party_;
  std::optional<std::uint32_t> shared_seed_, cosmetic_seed_;
  std::optional<VendorQuestVariables> variables_;
  std::optional<std::string> unestablished_;
  VendorResidueSource residue_;
  Lists slots_{};
  std::array<std::uint32_t, 3> counts_{};
  std::vector<std::optional<Bytes>> names_;
  std::array<std::vector<Item>, 3> seen_;
  std::optional<Pass> pass_;
  std::uint64_t generations_ = 0, steps_ = 0;
};
} // namespace champions::simulation::rta::items
