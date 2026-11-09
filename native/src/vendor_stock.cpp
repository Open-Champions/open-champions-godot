#include "champions/vendor_stock.hpp"
#include "champions/item_generation.hpp"
#include <algorithm>
#include <cstring>

// research-vendor-stock@fdf6752:docs/spec/vendor-stock.md:225-342.
// Observed session stock, prices, and storeLevel. Resource completion,
// the two non-literal hub stores, which literal is current, unbounded
// generation failure, rendered UI, and save I/O stay refused. A retry the
// research does not bound refuses at its first occurrence, and a duplicate
// decision that depends on bytes the generator never writes refuses.
namespace champions::simulation::rta::items {
namespace {
constexpr Raw kBuy = 0x3f199999u, kBuyArmour = 0x3ee66666u;
constexpr Raw kSellStack = 0x41a00000u, kSellFraction = 0x3d4cccccu;
constexpr std::uint64_t kFullFlags = 0x30000000000ull;
constexpr std::uint64_t kStackFlags = 0x1c0000000000ull;
constexpr std::uint64_t kZeroFlags = 0x400000000000ull;
constexpr std::uint64_t kBit40 = 1ull << 40;
constexpr std::int32_t kGoldCap = 99999999;

std::int32_t wrap(std::int64_t value) {
  return s32(static_cast<std::uint32_t>(value));
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

VendorStock::VendorStock(Tables tables) : tables_(std::move(tables)) {}
void VendorStock::set_seeds(std::uint32_t shared, std::uint32_t cosmetic) {
  shared_seed_ = shared;
  cosmetic_seed_ = cosmetic;
}
void VendorStock::set_difficulty(std::int32_t difficulty) { difficulty_ = difficulty; }
void VendorStock::set_party(std::int32_t num_players,
                            std::vector<std::int32_t> classes) {
  if (num_players < 1 || static_cast<std::size_t>(num_players) > classes.size())
    throw VendorRefused("party has no player or fewer classes than players");
  party_ = std::make_pair(num_players, std::move(classes));
}
void VendorStock::set_fixed(VendorFixedText fixed) { fixed_ = std::move(fixed); }
void VendorStock::set_destinations(std::array<std::string, 4> identifiers) {
  for (const auto &name : identifiers)
    if (name.empty())
      throw VendorRefused("empty shop destination identifier");
  destinations_ = std::move(identifiers);
  destinations_set_ = true;
}
void VendorStock::bind_quest_variables(VendorQuestVariables variables) {
  if (!variables.read || !variables.present || !variables.store)
    throw Error("quest-variable binding needs read, present and store");
  variables_ = std::move(variables);
}
void VendorStock::bind_quest_values(std::map<std::string, std::int32_t> *values) {
  if (!values) {
    variables_.reset();
    return;
  }
  VendorQuestVariables bound;
  bound.read = [values](const std::string &name) {
    return values->try_emplace(name, 0).first->second;
  };
  bound.present = [values](const std::string &name) { return values->count(name) != 0; };
  bound.store = [values](const std::string &name, std::int32_t value) {
    (*values)[name] = value;
  };
  variables_ = std::move(bound);
}
void VendorStock::set_executed_residue(VendorResidueSource source) {
  residue_ = std::move(source);
}
const VendorQuestVariables &VendorStock::variables() const {
  // docs/spec/vendor-stock.md:36-38,305-311: the selector reads the session
  // table. No private stand-in.
  if (!variables_)
    throw VendorRefused("storeLevel unavailable: session quest-variable table unbound");
  return *variables_;
}
bool VendorStock::store_level_present() const {
  return variables_ && variables_->present(kStoreLevelName);
}
std::int32_t VendorStock::read_store_level() {
  // docs/spec/vendor-stock.md:309-311. The creating read stores 0.
  return variables().read(kStoreLevelName);
}
std::vector<std::int32_t> VendorStock::literals(const std::string &level) {
  return literal_row(level);
}
void VendorStock::apply_literal_setv(const std::string &level, std::size_t index) {
  const auto &row = literal_row(level);
  if (index >= row.size())
    throw VendorRefused("storeLevel literal index is outside the authored list");
  variables().store(kStoreLevelName, row[index]);
  unestablished_.reset();
}
void VendorStock::apply_non_literal_setv(const std::string &level) {
  // docs/spec/vendor-stock.md:315-319,341. The expressions are unknown.
  if (level != "hub" && level != "hub2")
    throw VendorRefused("non-literal storeLevel setv is evidenced only in hub and hub2");
  unestablished_ = "storeLevel was last stored by the non-literal setv in hub/hub2; "
                   "that expression is unknown";
  throw VendorRefused("non-literal storeLevel setv in hub/hub2 is unknown");
}
void VendorStock::note_unclassified_store(const std::string &why) {
  // docs/spec/vendor-stock.md:315-319,341.
  unestablished_ = "storeLevel was last stored by a script setv that is not "
                   "classified as an authored literal: " + why;
}
void VendorStock::literal_current_on_visit(const std::string &) const {
  // docs/spec/vendor-stock.md:318-319,225-226.
  throw VendorRefused("which storeLevel literal is current on a visit is unknown");
}
void VendorStock::on_act_reset(bool reset_act) {
  // docs/spec/vendor-stock.md:45-47,320-321. The clear itself is the
  // session's (level_flow begin_change zeroes every quest value).
  if (reset_act)
    unestablished_.reset();
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
  if (pass_)
    throw Error("stock storage cannot be planted during a pass");
  for (auto &list : slots_) {
    list.resize(kSlots);
    for (auto &slot : list) {
      slot.item = Item();
      slot.raw.fill(byte);
      slot.known.set();
    }
  }
}
void VendorStock::plant_record(int category, std::size_t index, const Item &item) {
  if (pass_)
    throw Error("stock storage cannot be planted during a pass");
  if (category < 0 || category > 2 || index >= kSlots)
    throw Error("stock plant is outside the three lists");
  auto &list = slots_[static_cast<std::size_t>(category)];
  if (list.size() <= index)
    list.resize(index + 1);
  auto &slot = list[index];
  slot.item = item;
  slot.raw = item.pack();
  slot.known.set();
}
void VendorStock::plant_name_record(std::size_t index, const Bytes &raw) {
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
const VendorStock::Bytes &VendorStock::record_bytes(int category, std::size_t index) const {
  if (index >= count(category))
    throw Error("stock index is past the list count");
  return slots_[static_cast<std::size_t>(category)][index].raw;
}
const VendorStock::Known &VendorStock::record_known(int category, std::size_t index) const {
  if (index >= count(category))
    throw Error("stock index is past the list count");
  return slots_[static_cast<std::size_t>(category)][index].known;
}

namespace {
// 0x108924 as used by the stock loops: a non-null second definition, equal
// definition, flags, and the whole word at +0x20 (the unwritten +0x22
// participates; quantity does not). docs/spec/vendor-stock.md:142-146,
// ref_vendor_stock.py:61-64. With bytes of unknown value the result is
// known only when a known byte already differs.
template <typename Slot> int same_raw(const Slot &a, const Slot &b) {
  bool unknown = false, nonnull = false;
  for (std::size_t i = 0; i < 4; ++i) {
    if (!b.known[i])
      unknown = true;
    else if (b.raw[i])
      nonnull = true;
  }
  if (!nonnull && !unknown)
    return 0;
  if (!nonnull)
    unknown = true;
  static constexpr std::pair<std::size_t, std::size_t> ranges[3] = {
      {0, 4}, {8, 16}, {0x20, 0x24}};
  for (const auto &range : ranges)
    for (std::size_t o = range.first; o < range.second; ++o) {
      if (a.known[o] && b.known[o]) {
        if (a.raw[o] != b.raw[o])
          return 0;
      } else {
        unknown = true;
      }
    }
  return unknown ? 2 : 1;
}
constexpr const char *kUnknownBytes =
    "duplicate test depends on record bytes nothing establishes: a generated "
    "record's unwritten +0x22 (earlier-candidate float residue) or a stock "
    "slot that was never written (docs/spec/items.md:83-95,724)";
} // namespace

VendorStock::Same VendorStock::scan(const Pass &p, int category, std::uint32_t count,
                                    const Slot &candidate) const {
  // Scan origin is 2*category list strides (0x1208 each): list 0, the
  // previous third list, or the name buffer at 0x86b698, which is not a
  // fourth stock list. docs/spec/vendor-stock.md:124-140,287-301.
  bool unknown = false;
  for (std::uint32_t n = 0; n < count; ++n) {
    int result = 0;
    if (category == 2) {
      // docs/spec/vendor-stock.md:298-300,339: deferred names are a
      // non-match. Only a planted probe record is compared.
      if (n >= names_.size() || !names_[n])
        continue;
      Slot named;
      named.raw = *names_[n];
      named.known.set();
      result = same_raw(named, candidate);
    } else {
      const auto &list = p.slots[category == 1 ? 2u : 0u];
      const Slot never_written;
      result = same_raw(n < list.size() ? list[n] : never_written, candidate);
    }
    if (result == 1)
      return Same::Yes;
    if (result == 2)
      unknown = true;
  }
  return unknown ? Same::Unknown : Same::No;
}
void VendorStock::put(Pass &p, int category, std::uint32_t index, Slot slot) const {
  if (index >= kSlots)
    throw VendorRefused("stock count is past the 64-record list");
  auto &list = p.slots[static_cast<std::size_t>(category)];
  if (list.size() <= index)
    list.resize(index + 1);
  list[index] = std::move(slot);
}
VendorStock::Slot VendorStock::generated_slot(const Item &item,
                                              const VendorResidueCall &call) const {
  // Defined fields come from the generator. Its unwritten bytes are an
  // executed probe's when one is supplied, otherwise unknown.
  // docs/spec/items.md:83-95; docs/spec/vendor-stock.md:197-202.
  Slot slot;
  slot.item = item;
  slot.known.set();
  if (residue_) {
    slot.raw = item.pack(residue_(call, item));
  } else {
    slot.raw = item.pack();
    for (const auto offset : item.undefined_offsets()) {
      slot.raw[offset] = 0;
      slot.known.reset(offset);
    }
  }
  return slot;
}
VendorLoad VendorStock::refuse_pass(const std::string &reason) {
  VendorLoad out = pass_ ? pass_->out : VendorLoad{};
  pass_.reset();
  out.pending = false;
  out.refused = true;
  out.generated = false;
  out.reason = reason;
  out.counts = counts_;
  out.shared_seed = shared_seed();
  out.cosmetic_seed = cosmetic_seed();
  return out;
}
VendorLoad VendorStock::begin(int mode) {
  // docs/spec/vendor-stock.md:29-30,326-329. No freshness gate.
  if (pass_)
    throw VendorRefused("a stock pass is already in progress");
  VendorLoad out;
  out.mode = mode;
  if (mode == 0) {
    out.kept = true;
    out.counts = counts_;
    out.shared_seed = shared_seed();
    out.cosmetic_seed = cosmetic_seed();
    return out;
  }
  if (mode < 1 || mode > 3)
    throw VendorRefused("unestablished stock mode");
  out.resource_requested = true;
  out.resource_completed = false;
  out.resource_reason =
      "resource completion unavailable: shop resource 0x538678 is requested and not completed";
  pass_.emplace();
  pass_->out = out;
  try {
    // Port inputs first, so that a refused pass does not touch the table.
    // docs/spec/vendor-stock.md:36-43,165-171,330-332.
    if (!shared_seed_ || !cosmetic_seed_)
      throw VendorRefused("shared or cosmetic seed unavailable for the stock pass");
    if (!difficulty_)
      throw VendorRefused("difficulty 0x4f719c unavailable for the stock pass");
    if (!party_)
      throw VendorRefused("party count and classes unavailable for the stock pass");
    (void)variables();
    if (unestablished_)
      throw VendorRefused(*unestablished_);
    if (!fixed_.cell || fixed_.other.empty())
      throw VendorRefused("fixed descriptor unavailable");
    for (const auto &text : fixed_.weapons)
      if (text.empty())
        throw VendorRefused("fixed descriptor unavailable");
    auto &p = *pass_;
    const std::int32_t first = read_store_level();
    const std::int32_t second = read_store_level();
    p.out.store_level_reads = {first, second};
    p.out.budget = vendor_budget(first, *difficulty_, second);
    p.slots = slots_;
    p.counts = counts_;
    p.rng = Rng(*shared_seed_);
    p.cosmetic = *cosmetic_seed_;
    p.game.difficulty = *difficulty_;
    p.game.num_players = party_->first;
    p.game.classes = party_->second;
    p.category = 0;
    enter_category(p);
    p.out.pending = true;
    return p.out;
  } catch (const VendorRefused &error) {
    return refuse_pass(error.what());
  } catch (...) {
    pass_.reset();
    throw;
  }
}
void VendorStock::enter_category(Pass &p) {
  // Counts are reset individually; whole arrays are not cleared.
  // docs/spec/vendor-stock.md:106-108.
  const auto c = static_cast<std::size_t>(p.category);
  p.counts[c] = 0;
  p.fixed.clear();
  p.fixed_index = 0;
  p.progress = 0;
  p.retries = 0;
  if (p.category == 0)
    p.fixed.assign(fixed_.weapons.begin(), fixed_.weapons.end());
  else if (p.category == 2) {
    for (auto address : p.out.budget.other_pointer_cells) {
      auto text = fixed_.cell(address);
      if (text.empty())
        throw VendorRefused("fixed descriptor unavailable");
      p.fixed.push_back(std::move(text));
    }
    p.fixed.push_back(fixed_.other);
  }
  p.phase = p.fixed.empty() ? Phase::Ordinary : Phase::Fixed;
}
void VendorStock::step_fixed(Pass &p) {
  // Count increments before the parser. The return is ignored. The parser's
  // bytes replace the destination; for these descriptors they match a zero
  // stale image. docs/spec/vendor-stock.md:90-97,280-285.
  auto &count = p.counts[static_cast<std::size_t>(p.category)];
  const auto index = count++;
  auto item = parse_descriptor(tables_, p.fixed[p.fixed_index++], -1, false, &p.rng);
  Slot slot;
  slot.item = item;
  slot.raw = item.pack();
  slot.known.set();
  put(p, p.category, index, std::move(slot));
  if (p.fixed_index >= p.fixed.size())
    p.phase = Phase::Ordinary;
}
void VendorStock::step_ordinary(Pass &p) {
  // 0x1c7d90-0x1c7e94, 0x1c7fd8-0x1c80b4, 0x1c8398-0x1c8488, as
  // ref_vendor_stock.py:107-134. The generator writes slot `count` before
  // the outer tests, so a rejected candidate stays there until the next
  // write.
  static constexpr int attempts[3] = {22, 17, 6};
  const int c = p.category;
  const auto ci = static_cast<std::size_t>(c);
  auto &count = p.counts[ci];
  const auto &budget = p.out.budget;
  const std::int32_t attempt_budget =
      wrap(std::int64_t(budget.low) + std::int64_t(p.progress + 1) * budget.steps[ci]);
  auto made = generate_with_affixes(tables_, p.game, p.rng, attempt_budget, c,
                                    c == 2 ? 1 : -1, budget.caps[ci]);
  bool accept = false;
  if (!made) {
    ++p.out.failed[ci];
    // docs/spec/vendor-stock.md:117-123,217-219: weapons and armour retry the
    // same progress after a failure with no overall limit. The research does
    // not bound that retry, so the port refuses at the first one.
    if (c != 2)
      throw VendorRefused("unbounded generation failure: a generator failure in list " +
                          std::to_string(c) +
                          " retries the same progress step, which the research does not "
                          "bound (docs/spec/vendor-stock.md:121-122,217-219)");
  } else {
    const VendorResidueCall call{c, false, p.seen[ci].size()};
    p.seen[ci].push_back(*made);
    put(p, c, count, generated_slot(*made, call));
    const auto &slot = p.slots[ci][count];
    accept = true;
    const auto duplicate = scan(p, c, count, slot);
    if (duplicate == Same::Unknown)
      throw VendorRefused(kUnknownBytes);
    if (duplicate == Same::Yes && c == 1)
      ++p.out.armour_scan_rejections;
    // Six duplicate rejections; the seventh duplicate can pass.
    if (duplicate == Same::Yes && p.retries < 6) {
      ++p.retries;
      accept = false;
    }
    // Bit 40 rejects even after the duplicate allowance is spent, and the
    // retry it starts is not bounded by the research.
    if (c == 0 && (made->flags & kBit40)) {
      ++p.out.bit40_rejected[0];
      ++p.out.rejected[0];
      throw VendorRefused("unbounded generation failure: a bit-40 rejection in list 0 "
                          "retries the same progress step, which the research does not "
                          "bound (docs/spec/vendor-stock.md:119-122,217-219)");
    }
    if (c == 2 && slot.raw[0x21] == 0xff)
      accept = false;
    if (!accept)
      ++p.out.rejected[ci];
  }
  if (accept) {
    ++count;
    p.retries = 0;
  }
  // Weapons and armour retry the same progress and budget (only the bounded
  // duplicate retry reaches here); the third list advances on every attempt.
  if (accept || c == 2)
    ++p.progress;
  if (p.progress >= attempts[c])
    p.phase = Phase::Special;
}
void VendorStock::step_special(Pass &p) {
  // 0x1c7ea0, 0x1c80c4, 0x1c8498: one cosmetic draw after each ordinary
  // loop. docs/spec/vendor-stock.md:165-179.
  static constexpr int special_n[3] = {22, 17, 10};
  const int c = p.category;
  const auto ci = static_cast<std::size_t>(c);
  const auto &budget = p.out.budget;
  p.cosmetic = cosmetic_draw(p.cosmetic);
  const Raw sample = fmul(fint(p.cosmetic >> 16), 0x37800000u);
  p.out.special_roll[ci] = flt(sample, 0x3dccccccu);
  if (p.out.special_roll[ci]) {
    auto &count = p.counts[ci];
    const std::int32_t subtotal =
        wrap(std::int64_t(budget.low) + std::int64_t(special_n[c]) * budget.steps[ci]);
    const std::int32_t special_budget = wrap(std::int64_t(subtotal) * 2);
    p.out.special_budget[ci] = special_budget;
    // Failure preserves the destination. A nonzero pointer there is kept.
    auto preset = generate_preset(tables_, p.game, p.rng, special_budget, c,
                                  budget.special_cap);
    if (preset)
      put(p, c, count, generated_slot(*preset, VendorResidueCall{c, true, 0}));
    const auto &list = p.slots[ci];
    const Slot never_written;
    const Slot &slot = count < list.size() ? list[count] : never_written;
    bool nonzero = false;
    for (std::size_t i = 0; i < 4; ++i) {
      if (!slot.known[i])
        throw VendorRefused("special adoption reads a stock slot that was never written; "
                            "its definition pointer is unknown");
      nonzero = nonzero || slot.raw[i] != 0;
    }
    if (nonzero)
      ++count;
  }
  if (++p.category < 3)
    enter_category(p);
  else
    p.phase = Phase::Sort;
}
void VendorStock::step_sort(Pass &p) {
  // 0x1c8588-0x1c8724: ascending signed value, equal values keep their
  // order, and a swap copies all 0x48 bytes. docs/spec/vendor-stock.md:148-152.
  for (std::size_t c = 0; c < 3; ++c) {
    auto &list = p.slots[c];
    const auto n = p.counts[c];
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
}
VendorLoad VendorStock::finish_pass() {
  auto &p = *pass_;
  slots_ = std::move(p.slots);
  counts_ = p.counts;
  seen_ = std::move(p.seen);
  shared_seed_ = p.rng.seed;
  cosmetic_seed_ = p.cosmetic;
  ++generations_;
  VendorLoad out = p.out;
  pass_.reset();
  out.pending = false;
  out.generated = true;
  out.counts = counts_;
  out.shared_seed = *shared_seed_;
  out.cosmetic_seed = *cosmetic_seed_;
  return out;
}
VendorLoad VendorStock::step() {
  if (!pass_)
    throw Error("no stock pass in progress");
  ++steps_;
  try {
    auto &p = *pass_;
    switch (p.phase) {
    case Phase::Fixed:
      step_fixed(p);
      break;
    case Phase::Ordinary:
      step_ordinary(p);
      break;
    case Phase::Special:
      step_special(p);
      break;
    case Phase::Sort:
      step_sort(p);
      return finish_pass();
    }
    return p.out;
  } catch (const VendorRefused &error) {
    return refuse_pass(error.what());
  } catch (...) {
    pass_.reset();
    throw;
  }
}
VendorLoad VendorStock::open_or_stock(int mode) {
  auto out = begin(mode);
  while (out.pending)
    out = step();
  return out;
}
VendorLoad VendorStock::begin_load(const std::string &destination) {
  // docs/spec/vendor-stock.md:17-24,326-329. Every matching load regenerates.
  // A non-match does nothing. Mode 0 is not this caller.
  if (!destinations_set_)
    throw VendorRefused("shop destination identifiers unavailable");
  int mode = 0;
  bool matched = false;
  for (std::size_t i = 0; i < destinations_.size(); ++i) {
    if (destination != destinations_[i])
      continue;
    matched = true;
    mode = kShopDestinationModes[i];
  }
  if (!matched)
    return {};
  auto out = begin(mode);
  out.matched = true;
  if (pass_)
    pass_->out.matched = true;
  return out;
}
VendorLoad VendorStock::on_load(const std::string &destination) {
  auto out = begin_load(destination);
  while (out.pending)
    out = step();
  return out;
}
} // namespace champions::simulation::rta::items
