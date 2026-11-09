#include "champions/vendor_stock.hpp"
#include "champions/item_generation.hpp"
#include <algorithm>
#include <cstring>

// research-vendor-stock@fdf6752:docs/spec/vendor-stock.md:225-342.
// Observed session stock, prices, and storeLevel. Resource completion,
// the two non-literal hub stores, which literal is current, unbounded
// generation failure, rendered UI, and save I/O stay refused.
namespace champions::simulation::rta::items {
namespace {
constexpr Raw kBuy = 0x3f199999u, kBuyArmour = 0x3ee66666u;
constexpr Raw kSellStack = 0x41a00000u, kSellFraction = 0x3d4cccccu;
constexpr std::uint64_t kFullFlags = 0x30000000000ull;
constexpr std::uint64_t kStackFlags = 0x1c0000000000ull;
constexpr std::uint64_t kZeroFlags = 0x400000000000ull;
constexpr std::uint64_t kBit40 = 1ull << 40;
constexpr std::int32_t kGoldCap = 99999999;
constexpr std::uint32_t kListStride = 0x1208u;

std::int32_t wrap(std::int64_t value) {
  return s32(static_cast<std::uint32_t>(value));
}
std::array<std::uint8_t, ITEM_SIZE> zeros() { return {}; }
bool same_raw(const std::array<std::uint8_t, ITEM_SIZE> &a,
              const std::array<std::uint8_t, ITEM_SIZE> &b) {
  // 0x108924. The unwritten halfword +0x22 participates. Quantity does not.
  // docs/spec/vendor-stock.md:142-146,296-297.
  if (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0)
    return false;
  return std::memcmp(a.data(), b.data(), 4) == 0 &&
         std::memcmp(a.data() + 8, b.data() + 8, 8) == 0 &&
         std::memcmp(a.data() + 0x20, b.data() + 0x20, 4) == 0;
}
std::uint32_t cosmetic_draw(std::uint32_t seed) {
  return static_cast<std::uint32_t>(seed * 0x19660du + 0x3c6ef35fu);
}
const std::vector<std::int32_t> &literal_row(const std::string &level) {
  // docs/spec/vendor-stock.md:313-317. Counts only; no other expression.
  static const std::vector<std::int32_t> hub{0, 8, 7, 6, 5, 7, 6};
  static const std::vector<std::int32_t> stobonus{7};
  static const std::vector<std::int32_t> islgnome{3};
  if (level == "hub" || level == "hub2")
    return hub;
  if (level == "stobonus")
    return stobonus;
  if (level == "islgnome")
    return islgnome;
  throw VendorRefused("storeLevel literal setv is not authored for this level");
}
} // namespace

std::int32_t vendor_buy_price(std::int32_t value, std::int32_t category,
                              std::uint64_t record_flags, std::int32_t slot) {
  // docs/spec/vendor-stock.md:236-244. Full-price override wins.
  Raw factor = category == 1 ? kBuyArmour : kBuy;
  if ((record_flags & kFullFlags) || slot == 8)
    factor = F_ONE;
  return ftrunc(fmul(fint(static_cast<std::uint32_t>(value)), factor));
}
std::int32_t vendor_sell_credit(std::int32_t value, std::int32_t count,
                                std::int32_t slot, bool name_has_stem,
                                std::uint64_t definition_flags) {
  // docs/spec/vendor-stock.md:246-254. Slot 8 is scaled, then forced to 0.
  Raw amount = fint(static_cast<std::uint32_t>(value));
  if (slot == 8 || name_has_stem || (definition_flags & kStackFlags))
    amount = fmul(amount, kSellStack);
  std::int32_t paid = ftrunc(fmul(
      fdiv(amount, fint(static_cast<std::uint32_t>(count))), kSellFraction));
  if (paid <= 0)
    paid = 1;
  if (slot == 8 || (definition_flags & kZeroFlags))
    paid = 0;
  return paid;
}
std::pair<std::int32_t, bool> vendor_buy_gold(std::int32_t gold,
                                              std::int32_t price) {
  // docs/spec/vendor-stock.md:240-244. Debit is stored before giveItem.
  gold = wrap(gold);
  price = wrap(price);
  if (gold < price)
    return {gold, false};
  return {wrap(std::int64_t(gold) - price), true};
}
std::int32_t vendor_sell_gold(std::int32_t gold, std::int32_t paid) {
  // docs/spec/vendor-stock.md:252-254. Credit is stored before dropItem.
  gold = wrap(std::int64_t(wrap(gold)) + wrap(paid));
  if (gold > kGoldCap)
    gold = kGoldCap;
  return gold;
}
VendorBudget vendor_budget(std::int32_t store_level, std::int32_t difficulty,
                           std::int32_t second_read) {
  // docs/spec/vendor-stock.md:67-88,330-332.
  if (difficulty < 1 || difficulty > 5 || second_read < 0 || second_read > 12)
    throw VendorRefused("unestablished budget input");
  const std::int32_t q =
      store_level >= 0 && store_level <= 11 ? store_level : -1;
  const Raw scale =
      fmax(F_ONE, fadd(fmul(fint(static_cast<std::uint32_t>(q)), 0x3eaaaaaau),
                       F_ONE));
  const std::int32_t base_integer =
      1600 * (second_read + 11 * (difficulty - 1));
  const Raw powered =
      powf(fint(static_cast<std::uint32_t>(base_integer)), 0x3f999999u);
  const Raw base = fadd(powered, 0x45dac000u);
  VendorBudget out;
  out.scale = scale;
  out.low = ftrunc(fsub(fmul(base, 0x3f199999u), 0x453b8000u));
  out.high = ftrunc(fadd(fmul(base, 0x3fc00000u), 0x45bb8000u));
  const std::int32_t delta = wrap(std::int64_t(out.high) - out.low);
  const int divisors[3] = {22, 17, 6};
  for (int i = 0; i < 3; ++i)
    out.steps[static_cast<std::size_t>(i)] = delta / divisors[i];
  const Raw scaled = fmul(scale, 0x40800000u);
  out.caps[0] = ftrunc(fadd(fint(static_cast<std::uint32_t>(24 * difficulty - 24)),
                            scaled)) +
                2;
  out.caps[1] = out.caps[0];
  out.caps[2] =
      ftrunc(fadd(fint(static_cast<std::uint32_t>(24 * difficulty - 18)), scaled));
  out.special_cap = out.caps[0];
  // docs/spec/vendor-stock.md:99-102. Byte offsets, not item names.
  const int tier = !flt(0x3fc00000u, scale) ? 44 : !flt(0x40600000u, scale) ? 40 : 36;
  const auto offset = static_cast<std::uint32_t>(tier - 12 * std::min(difficulty, 3));
  out.other_pointer_cells = {0x4f4280u + offset, 0x4f4258u + offset};
  return out;
}
void sort_stock_by_value(const Tables &tables, std::vector<Item> &items) {
  // docs/spec/vendor-stock.md:148-152. Ascending signed value, stable ties.
  std::stable_sort(items.begin(), items.end(), [&](const Item &a, const Item &b) {
    return item_value(tables, a) < item_value(tables, b);
  });
}

VendorStock::VendorStock(Tables tables) : tables_(std::move(tables)) {
  game_.num_players = 1;
  game_.classes = {0, 0, 0, 0};
  game_.difficulty = 1;
}
void VendorStock::set_seeds(std::uint32_t shared, std::uint32_t cosmetic) {
  shared_seed_ = shared;
  cosmetic_seed_ = cosmetic;
}
void VendorStock::set_difficulty(std::int32_t difficulty) { difficulty_ = difficulty; }
void VendorStock::set_party(std::int32_t num_players,
                            std::vector<std::int32_t> classes) {
  game_.num_players = num_players;
  game_.classes = std::move(classes);
}
void VendorStock::set_fixed(VendorFixedText fixed) { fixed_ = std::move(fixed); }
void VendorStock::set_destinations(std::array<std::string, 4> identifiers) {
  for (const auto &name : identifiers)
    if (name.empty())
      throw VendorRefused("empty shop destination identifier");
  destinations_ = std::move(identifiers);
  destinations_set_ = true;
}
constexpr const char *kStoreLevel = "storeLevel";
void VendorStock::bind_quest_values(std::map<std::string, std::int32_t> *values) {
  quest_values_ = values;
}
bool VendorStock::store_level_present() const {
  return quest_values_ ? quest_values_->count(kStoreLevel) != 0 : store_present_;
}
void VendorStock::store(std::int32_t value) {
  if (quest_values_)
    (*quest_values_)[kStoreLevel] = value;
  else {
    store_present_ = true;
    store_level_ = value;
  }
}
std::int32_t VendorStock::read_store_level() {
  // docs/spec/vendor-stock.md:309-311. The creating read stores 0.
  if (quest_values_)
    return quest_values_->try_emplace(kStoreLevel, 0).first->second;
  if (!store_present_) {
    store_present_ = true;
    store_level_ = 0;
  }
  return store_level_;
}
void VendorStock::set_retry_watchdog(std::uint64_t attempts) {
  if (attempts == 0)
    throw Error("retry watchdog must allow one attempt");
  retry_watchdog_ = attempts;
}
std::vector<std::int32_t> VendorStock::literals(const std::string &level) {
  return literal_row(level);
}
void VendorStock::apply_literal_setv(const std::string &level, std::size_t index) {
  const auto &row = literal_row(level);
  if (index >= row.size())
    throw VendorRefused("storeLevel literal index is outside the authored list");
  store(row[index]);
  non_literal_ = false;
}
void VendorStock::apply_non_literal_setv(const std::string &level) {
  // docs/spec/vendor-stock.md:315-319,341. The expressions are unknown.
  if (level != "hub" && level != "hub2")
    throw VendorRefused("non-literal storeLevel setv is evidenced only in hub and hub2");
  non_literal_ = true;
  throw VendorRefused("non-literal storeLevel setv in hub/hub2 is unknown");
}
void VendorStock::literal_current_on_visit(const std::string &) const {
  // docs/spec/vendor-stock.md:318-319,225-226.
  throw VendorRefused("which storeLevel literal is current on a visit is unknown");
}
void VendorStock::plant_store_level(std::int32_t value) {
  store(value);
  non_literal_ = false;
}
void VendorStock::on_act_change(bool act_changed, bool card_latch, bool death_task) {
  // docs/spec/vendor-stock.md:45-47,320. A latch or a death task skips the clear.
  // With bound quest values the level-flow clear zeroes every variable;
  // repeating it here is idempotent. An absent variable stays absent.
  if (!act_changed || card_latch || death_task)
    return;
  if (store_level_present())
    store(0);
  non_literal_ = false;
}
void VendorStock::render_shop() const {
  // docs/spec/vendor-stock.md:223-224,341.
  throw VendorRefused("rendered shop UI is not executed");
}
void VendorStock::save_stock() const {
  // docs/spec/vendor-stock.md:220-222,341.
  throw VendorRefused("save I/O for vendor stock is not executed");
}
void VendorStock::fill_storage(std::uint8_t byte) {
  for (auto &list : slots_) {
    list.resize(kSlots);
    for (auto &slot : list) {
      slot.raw.fill(byte);
      slot.occupied = true;
    }
  }
}
void VendorStock::plant_record(int category, std::size_t index, const Item &item) {
  if (category < 0 || category > 2 || index >= kSlots)
    throw Error("stock plant is outside the three lists");
  auto &list = slots_[static_cast<std::size_t>(category)];
  if (list.size() <= index)
    list.resize(index + 1);
  auto &slot = list[index];
  slot.item = item;
  slot.raw = item.pack();
  slot.occupied = true;
}
void VendorStock::plant_name_record(
    std::size_t index, const std::array<std::uint8_t, kRecord> &raw) {
  if (names_.size() <= index)
    names_.resize(index + 1);
  names_[index] = raw;
}
const std::vector<Item> &VendorStock::candidates(int category) const {
  if (category < 0 || category > 2)
    throw Error("stock list is outside 0..2");
  return seen_[static_cast<std::size_t>(category)];
}
std::uint32_t VendorStock::count(int category) const {
  if (category < 0 || category > 2)
    throw Error("stock list is outside 0..2");
  return counts_[static_cast<std::size_t>(category)];
}
const Item &VendorStock::item(int category, std::size_t index) const {
  if (index >= count(category))
    throw Error("stock index is past the list count");
  return slots_[static_cast<std::size_t>(category)][index].item;
}
const std::array<std::uint8_t, VendorStock::kRecord> &
VendorStock::record_bytes(int category, std::size_t index) const {
  if (index >= count(category))
    throw Error("stock index is past the list count");
  return slots_[static_cast<std::size_t>(category)][index].raw;
}
VendorStock::Snapshot VendorStock::capture() const {
  return {slots_, counts_, shared_seed_, cosmetic_seed_, seen_};
}
void VendorStock::restore(const Snapshot &saved) {
  slots_ = saved.slots;
  counts_ = saved.counts;
  shared_seed_ = saved.shared;
  cosmetic_seed_ = saved.cosmetic;
  seen_ = saved.seen;
}
std::array<std::uint8_t, ITEM_SIZE>
VendorStock::scan_record(int category, std::size_t index) const {
  // Category 1 scans the previous third list. Category 2 scans the name
  // buffer at 0x86b698, which is not a fourth stock list.
  // docs/spec/vendor-stock.md:124-140,287-301.
  if (category == 2) {
    if (index >= names_.size())
      return zeros();
    return names_[index];
  }
  const int source = category == 1 ? 2 : 0;
  const auto &list = slots_[static_cast<std::size_t>(source)];
  if (index >= list.size() || !list[index].occupied)
    return zeros();
  return list[index].raw;
}
void VendorStock::write_record(int category, std::uint32_t index, const Item &item) {
  if (index >= kSlots)
    throw VendorRefused("unbounded generation failure");
  auto &list = slots_[static_cast<std::size_t>(category)];
  if (list.size() <= index)
    list.resize(index + 1);
  // Parser and generator bytes replace the destination. A stale 0xa5 image
  // is not kept. docs/spec/vendor-stock.md:280-285.
  list[index].item = item;
  list[index].raw = item.pack();
  list[index].occupied = true;
}
bool VendorStock::duplicate(int category, std::uint32_t count,
                            const std::array<std::uint8_t, ITEM_SIZE> &raw,
                            int &rejections) {
  // Scan origin is 2*category list strides: list 0, the previous third list,
  // or the name buffer at STOCK+4*0x1208 (0x86b698). kListStride is that stride.
  (void)kListStride;
  for (std::uint32_t n = 0; n < count; ++n) {
    if (!same_raw(scan_record(category, n), raw))
      continue;
    if (category == 1)
      ++rejections;
    return true;
  }
  return false;
}
VendorLoad VendorStock::regenerate(int mode) {
  // Modes 1, 2 and 3 share one body. docs/spec/vendor-stock.md:29-30,326-329.
  VendorLoad out;
  out.mode = mode;
  out.resource_requested = true;
  out.resource_completed = false;
  out.resource_reason =
      "resource completion unavailable: shop resource 0x538678 is requested and not completed";
  const Snapshot saved = capture();
  for (auto &seen : seen_)
    seen.clear();
  try {
    if (non_literal_)
      throw VendorRefused("storeLevel was last stored by the non-literal setv in "
                          "hub/hub2; that expression is unknown");
    const std::int32_t first = read_store_level();
    const std::int32_t second = read_store_level();
    game_.difficulty = difficulty_;
    const VendorBudget budget = vendor_budget(first, difficulty_, second);
    if (!fixed_.cell)
      throw VendorRefused("fixed descriptor unavailable");
    for (const auto &text : fixed_.weapons)
      if (text.empty())
        throw VendorRefused("fixed descriptor unavailable");
    if (fixed_.other.empty())
      throw VendorRefused("fixed descriptor unavailable");
    Rng rng(shared_seed_);
    const int attempts[3] = {22, 17, 6};
    const int special_n[3] = {22, 17, 10};
    for (int category = 0; category < 3; ++category) {
      auto &count = counts_[static_cast<std::size_t>(category)];
      count = 0;
      std::vector<std::string> fixed;
      if (category == 0)
        fixed.assign(fixed_.weapons.begin(), fixed_.weapons.end());
      else if (category == 2) {
        for (auto address : budget.other_pointer_cells) {
          auto text = fixed_.cell(address);
          if (text.empty())
            throw VendorRefused("fixed descriptor unavailable");
          fixed.push_back(std::move(text));
        }
        fixed.push_back(fixed_.other);
      }
      for (const auto &text : fixed) {
        // Count increments before the parser. The return is ignored.
        // docs/spec/vendor-stock.md:90-97.
        const auto index = count++;
        auto item = parse_descriptor(tables_, text, -1, false, &rng);
        write_record(category, index, item);
      }
      // 0x1c7d90-0x1c7e94, 0x1c7fd8-0x1c80b4, 0x1c8398-0x1c8488, as
      // ref_vendor_stock.py:107-134. The generator writes slot `count`
      // before the outer tests, so a rejected candidate stays there until
      // the next write. A failed call preserves the destination.
      int retries = 0;
      int progress = 0;
      std::uint64_t idle = 0;
      while (progress < attempts[category]) {
        const std::int32_t attempt_budget = wrap(
            std::int64_t(budget.low) +
            std::int64_t(progress + 1) * budget.steps[static_cast<std::size_t>(category)]);
        auto made = generate_with_affixes(
            tables_, game_, rng, attempt_budget, category, category == 2 ? 1 : -1,
            budget.caps[static_cast<std::size_t>(category)]);
        bool accept = false;
        if (made) {
          write_record(category, count, *made);
          seen_[static_cast<std::size_t>(category)].push_back(*made);
          const auto &raw = slots_[static_cast<std::size_t>(category)][count].raw;
          accept = true;
          // Six duplicate rejections; the seventh duplicate can pass.
          if (duplicate(category, count, raw, out.armour_scan_rejections) &&
              retries < 6) {
            ++retries;
            accept = false;
          }
          // Bit 40 rejects even after the duplicate allowance is spent.
          if (category == 0 && (made->flags & kBit40)) {
            accept = false;
            ++out.bit40_rejected[0];
          }
          if (category == 2 && raw[0x21] == 0xff)
            accept = false;
          if (!accept)
            ++out.rejected[static_cast<std::size_t>(category)];
        } else {
          ++out.failed[static_cast<std::size_t>(category)];
        }
        if (accept) {
          ++count;
          retries = 0;
        }
        // Weapons and armour retry the same progress and budget; the third
        // list advances even on failure. docs/spec/vendor-stock.md:117-131.
        if (accept || category == 2) {
          ++progress;
          idle = 0;
          continue;
        }
        // The original has no overall bound (docs/spec/vendor-stock.md:
        // 121-122,217-219). This is a port watchdog, not an observed value:
        // reaching it refuses the whole pass and never publishes stock.
        if (++idle >= retry_watchdog_) {
          restore(saved);
          out.refused = true;
          out.reason = "unbounded generation failure: no record accepted at one "
                       "progress step within the port watchdog";
          out.generated = false;
          out.counts = counts_;
          return out;
        }
      }
      cosmetic_seed_ = cosmetic_draw(cosmetic_seed_);
      const Raw sample = fmul(fint(cosmetic_seed_ >> 16), 0x37800000u);
      out.special_roll[static_cast<std::size_t>(category)] =
          flt(sample, 0x3dccccccu);
      if (!out.special_roll[static_cast<std::size_t>(category)])
        continue;
      const std::int32_t subtotal = wrap(
          std::int64_t(budget.low) +
          std::int64_t(special_n[category]) *
              budget.steps[static_cast<std::size_t>(category)]);
      const std::int32_t special_budget = wrap(std::int64_t(subtotal) * 2);
      out.special_budget[static_cast<std::size_t>(category)] = special_budget;
      // Failure preserves the destination. A nonzero pointer there is kept.
      // docs/spec/vendor-stock.md:173-179. Stack residue is not invented: only
      // a record already in that slot can be adopted.
      auto preset = generate_preset(tables_, game_, rng, special_budget, category,
                                    budget.special_cap);
      if (preset)
        write_record(category, count, *preset);
      const auto &list = slots_[static_cast<std::size_t>(category)];
      if (count < list.size() && list[count].occupied &&
          list[count].raw[0] | list[count].raw[1] | list[count].raw[2] |
              list[count].raw[3])
        ++count;
    }
    // 0x1c8588-0x1c8724: ascending signed value, equal values keep their
    // order, and a swap copies all 0x48 bytes. docs/spec/vendor-stock.md:148-152.
    for (int category = 0; category < 3; ++category) {
      auto &list = slots_[static_cast<std::size_t>(category)];
      const auto n = counts_[static_cast<std::size_t>(category)];
      if (list.size() < n)
        throw Error("stock count exceeds stored records");
      std::vector<std::pair<std::int32_t, Slot>> rows;
      rows.reserve(n);
      for (std::uint32_t i = 0; i < n; ++i) {
        if (!list[i].item.definition)
          throw VendorRefused("a counted stock record is residue without a "
                              "definition; its value is unknown");
        rows.emplace_back(item_value(tables_, list[i].item), list[i]);
      }
      std::stable_sort(rows.begin(), rows.end(),
                       [](const auto &a, const auto &b) { return a.first < b.first; });
      for (std::uint32_t i = 0; i < n; ++i)
        list[i] = std::move(rows[i].second);
    }
    shared_seed_ = rng.seed;
    out.counts = counts_;
    out.shared_seed = shared_seed_;
    out.cosmetic_seed = cosmetic_seed_;
    out.generated = true;
    ++generations_;
    return out;
  } catch (const VendorRefused &error) {
    restore(saved);
    out.refused = true;
    out.reason = error.what();
    out.generated = false;
    out.counts = counts_;
    return out;
  } catch (...) {
    restore(saved);
    throw;
  }
}
VendorLoad VendorStock::open_or_stock(int mode) {
  // docs/spec/vendor-stock.md:29-30,326-329. No freshness gate.
  VendorLoad out;
  out.mode = mode;
  if (mode == 0) {
    out.kept = true;
    out.counts = counts_;
    out.shared_seed = shared_seed_;
    out.cosmetic_seed = cosmetic_seed_;
    return out;
  }
  if (mode < 1 || mode > 3)
    throw VendorRefused("unestablished stock mode");
  return regenerate(mode);
}
VendorLoad VendorStock::on_load(const std::string &destination) {
  // docs/spec/vendor-stock.md:17-24,326-329. Every matching load regenerates.
  // A non-match does nothing. Mode 0 is not this caller.
  if (!destinations_set_)
    throw VendorRefused("shop destination identifiers unavailable");
  VendorLoad out;
  int mode = 0;
  bool matched = false;
  for (std::size_t i = 0; i < destinations_.size(); ++i) {
    if (destination != destinations_[i])
      continue;
    matched = true;
    mode = kShopDestinationModes[i];
  }
  if (!matched)
    return out;
  out = open_or_stock(mode);
  out.matched = true;
  return out;
}
} // namespace champions::simulation::rta::items
