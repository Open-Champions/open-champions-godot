#pragma once
#include "champions/items.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
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

struct VendorFixedText {
  std::array<std::string, 4> weapons;
  std::string other;
  // Descriptor text already resolved from the pointer cell. Empty refuses.
  std::function<std::string(std::uint32_t address)> cell;
};

struct VendorLoad {
  bool matched = false;
  bool generated = false;
  bool kept = false;
  bool refused = false;
  std::string reason;
  bool resource_requested = false;
  bool resource_completed = false;
  std::string resource_reason;
  int mode = -1;
  std::array<std::uint32_t, 3> counts{};
  int armour_scan_rejections = 0;
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
class VendorStock {
public:
  static constexpr std::uint32_t kResource = 0x538678u;
  static constexpr std::size_t kSlots = 64;
  static constexpr std::size_t kRecord = ITEM_SIZE;

  explicit VendorStock(Tables tables);

  void set_seeds(std::uint32_t shared, std::uint32_t cosmetic);
  void set_difficulty(std::int32_t difficulty);
  void set_party(std::int32_t num_players, std::vector<std::int32_t> classes);
  void set_fixed(VendorFixedText fixed);
  void set_destinations(std::array<std::string, 4> identifiers);
  // Optional session quest variables (the getv/setv table). When bound,
  // storeLevel lives in that map, so script setv and the level-flow act
  // clear reach it directly. Unbound, a private variable is used.
  void bind_quest_values(std::map<std::string, std::int32_t> *values);
  // Creating read of storeLevel. A missing variable is stored at 0.
  // docs/spec/vendor-stock.md:305-311.
  std::int32_t read_store_level();
  bool store_level_present() const;
  // Authored literal setv values only. docs/spec/vendor-stock.md:313-317.
  void apply_literal_setv(const std::string &level, std::size_t index);
  // Throws. Also marks storeLevel unestablished so that the next stock
  // pass refuses until a literal setv or an act clear replaces the value.
  void apply_non_literal_setv(const std::string &level);
  bool store_level_unestablished() const { return non_literal_; }
  // Consecutive non-accepting attempts at one weapon/armour progress step
  // before the pass is refused. Port watchdog only; the original has no
  // bound (docs/spec/vendor-stock.md:121-122,217-219).
  void set_retry_watchdog(std::uint64_t attempts);
  void literal_current_on_visit(const std::string &level) const;
  // Probe plant, not a script store. Act clear reads the planted value back as 0.
  void plant_store_level(std::int32_t value);
  // vars_clearAll on a guarded act change. docs/spec/vendor-stock.md:45-47,320.
  void on_act_change(bool act_changed, bool card_latch, bool death_task);
  void render_shop() const;
  void save_stock() const;

  void fill_storage(std::uint8_t byte);
  void plant_record(int category, std::size_t index, const Item &item);
  void plant_name_record(std::size_t index,
                         const std::array<std::uint8_t, kRecord> &raw);

  VendorLoad open_or_stock(int mode);
  VendorLoad on_load(const std::string &destination);

  std::uint32_t count(int category) const;
  const Item &item(int category, std::size_t index) const;
  const std::array<std::uint8_t, kRecord> &
  record_bytes(int category, std::size_t index) const;
  std::uint64_t generations() const { return generations_; }
  std::uint32_t shared_seed() const { return shared_seed_; }
  std::uint32_t cosmetic_seed() const { return cosmetic_seed_; }
  const std::vector<Item> &armour_candidates() const { return seen_[1]; }
  // Every successful ordinary generator result of the last pass, in order.
  const std::vector<Item> &candidates(int category) const;

  static std::vector<std::int32_t> literals(const std::string &level);

private:
  struct Slot {
    Item item;
    std::array<std::uint8_t, kRecord> raw{};
    bool occupied = false;
  };
  struct Snapshot {
    std::array<std::vector<Slot>, 3> slots;
    std::array<std::uint32_t, 3> counts{};
    std::uint32_t shared = 0, cosmetic = 0;
    std::array<std::vector<Item>, 3> seen;
  };

  VendorLoad regenerate(int mode);
  Snapshot capture() const;
  void restore(const Snapshot &);
  std::array<std::uint8_t, kRecord> scan_record(int category,
                                               std::size_t index) const;
  void write_record(int category, std::uint32_t index, const Item &item);
  bool duplicate(int category, std::uint32_t count,
                 const std::array<std::uint8_t, kRecord> &raw, int &rejections);

  Tables tables_;
  Game game_{};
  VendorFixedText fixed_{};
  std::array<std::string, 4> destinations_{};
  bool destinations_set_ = false;
  std::int32_t difficulty_ = 1;
  std::uint32_t shared_seed_ = 0, cosmetic_seed_ = 0;
  bool store_present_ = false;
  std::int32_t store_level_ = 0;
  std::map<std::string, std::int32_t> *quest_values_ = nullptr;
  bool non_literal_ = false;
  std::uint64_t retry_watchdog_ = std::uint64_t(1) << 20;
  void store(std::int32_t value);
  std::array<std::vector<Slot>, 3> slots_{};
  std::array<std::uint32_t, 3> counts_{};
  std::vector<std::array<std::uint8_t, kRecord>> names_;
  std::array<std::vector<Item>, 3> seen_;
  std::uint64_t generations_ = 0;
};

void sort_stock_by_value(const Tables &, std::vector<Item> &);
} // namespace champions::simulation::rta::items
