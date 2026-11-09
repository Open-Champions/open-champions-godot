#include "vendor_stock_fixture.hpp"
#include "champions/level_flow.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

namespace it = champions::simulation::rta::items;
namespace fx = it::vendor_fixture;
namespace lf = champions::simulation::rta::level_flow;

namespace {
int failed = 0;
void check(bool ok, const std::string &what) {
  if (ok)
    return;
  std::cerr << "FAIL " << what << '\n';
  ++failed;
}
bool threw_reason(const std::function<void()> &fn, const std::string &part) {
  try {
    fn();
  } catch (const std::exception &error) {
    return std::string(error.what()).find(part) != std::string::npos;
  }
  return false;
}
bool has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}
using Record = std::array<std::uint8_t, it::ITEM_SIZE>;
std::vector<Record> packs(const it::VendorStock &stock) {
  std::vector<Record> out;
  for (int category = 0; category < 3; ++category)
    for (std::uint32_t i = 0; i < stock.count(category); ++i)
      out.push_back(stock.record_bytes(category, i));
  return out;
}
// Independent restatement of ref_vendor_stock.same_raw (ref:61-64).
bool ref_same_raw(const Record &a, const Record &b) {
  const bool nonnull = b[0] | b[1] | b[2] | b[3];
  return nonnull && std::memcmp(a.data(), b.data(), 4) == 0 &&
         std::memcmp(a.data() + 8, b.data() + 8, 8) == 0 &&
         std::memcmp(a.data() + 0x20, b.data() + 0x20, 4) == 0;
}
std::uint32_t tag(const Record &raw) { return raw[0x44] | (std::uint32_t(raw[0x45]) << 8); }
} // namespace

int main() {
  using it::vendor_budget;
  const auto base = vendor_budget(0, 1, 0);
  check(base.scale == 0x3f800000u && base.low == 1199 && base.high == 16500 &&
            base.steps == std::array<std::int32_t, 3>{695, 900, 2550} &&
            base.caps == std::array<std::int32_t, 3>{6, 6, 10} &&
            base.other_pointer_cells == std::array<std::uint32_t, 2>{0x4f42a0u, 0x4f4278u},
        "storeLevel 0 difficulty 1 budget");
  const auto seven = vendor_budget(7, 1, 7);
  check(seven.scale == 0x40555554u && seven.low == 44572 && seven.high == 124930 &&
            seven.steps == std::array<std::int32_t, 3>{3652, 4726, 13393} &&
            seven.caps == std::array<std::int32_t, 3>{15, 15, 19},
        "storeLevel 7 difficulty 1 budget");
  const auto high = vendor_budget(11, 3, 11);
  check(high.scale == 0x40955554u && high.low == 280012 && high.high == 713530 &&
            high.steps == std::array<std::int32_t, 3>{19705, 25501, 72253} &&
            high.caps == std::array<std::int32_t, 3>{68, 68, 72} &&
            high.other_pointer_cells == std::array<std::uint32_t, 2>{0x4f4280u, 0x4f4258u},
        "storeLevel 11 difficulty 3 budget and pointer cells");
  const auto reset = vendor_budget(99, 1, 0);
  check(reset.scale == it::F_ONE && reset.low == 1199 && reset.caps[0] == 6,
        "an out-of-range first read resets scale and keeps the second read");
  check(threw_reason([] { vendor_budget(0, 1, 13); }, "unestablished budget input"),
        "second read 13 is refused");
  check(threw_reason([] { vendor_budget(0, 0, 0); }, "unestablished budget input"),
        "difficulty 0 is refused");
  check(threw_reason([] { vendor_budget(0, 6, 0); }, "unestablished budget input"),
        "difficulty 6 is refused");

  check(it::vendor_buy_price(100, 0, 0, 0) == 59, "value 100 buys at 59");
  check(it::vendor_buy_price(100, 1, 0, 0) == 44, "category 1 buys at 44");
  check(it::vendor_buy_price(100, 0, 0x30000000000ull, 0) == 100, "record flags force full price");
  check(it::vendor_buy_price(100, 1, 0, 8) == 100, "slot 8 forces full price");
  check(it::vendor_sell_credit(100, 1, 0, false, 0) == 4, "value 100 sells at 4");
  check(it::vendor_sell_credit(100, 1, 8, false, 0) == 0, "slot 8 sell credit is forced to 0");
  check(it::vendor_sell_credit(100, 1, 0, false, 0x400000000000ull) == 0,
        "zero-flag sell credit is 0");
  check(it::vendor_sell_gold(99999998, 4) == 99999999, "sell gold caps at 99999999");
  check(it::vendor_sell_gold(10, 4) == 14, "sell credit is added before the cap");
  const auto short_gold = it::vendor_buy_gold(10, 59);
  check(short_gold.first == 10 && !short_gold.second, "short gold does not debit");
  const auto paid = it::vendor_buy_gold(100, 59);
  check(paid.first == 41 && paid.second, "a covered price of 59 leaves 41 gold and buys");

  // Every input starts unset and refuses; a refused pass does not touch the
  // quest table. docs/spec/vendor-stock.md:36-43,165-171,330-332.
  {
    it::VendorStock bare(fx::tables());
    bare.set_fixed(fx::fixed_text());
    bare.fill_storage(0);
    bare.set_executed_residue(fx::residue());
    std::map<std::string, std::int32_t> quest;
    auto r = bare.open_or_stock(1);
    check(r.refused && has(r.reason, "seed"), "unset seeds refuse");
    bare.set_seeds(7, 7);
    r = bare.open_or_stock(1);
    check(r.refused && has(r.reason, "difficulty"), "unset difficulty refuses");
    bare.set_difficulty(1);
    r = bare.open_or_stock(1);
    check(r.refused && has(r.reason, "party"), "unset party refuses");
    check(threw_reason([&] { bare.set_party(0, {}); }, "party"), "an empty party is refused");
    bare.set_party(1, {0, 0, 0, 0});
    r = bare.open_or_stock(1);
    check(r.refused && has(r.reason, "quest-variable table unbound"),
          "an unbound storeLevel table refuses; there is no private stand-in");
    check(threw_reason([&] { bare.read_store_level(); }, "unbound"),
          "reading storeLevel without the session table is refused");
    bare.bind_quest_values(&quest);
    check(quest.empty() && bare.generations() == 0, "refused passes did not create storeLevel");
    r = bare.open_or_stock(1);
    check(r.generated && quest.at("storeLevel") == 0 && r.store_level_reads == std::array<std::int32_t, 2>{0, 0},
          "with every input supplied the creating read stores 0 and the pass completes");
  }

  std::map<std::string, std::int32_t> table;
  auto fresh = fx::ready(7, 7);
  fresh.bind_quest_values(&table);
  check(!fresh.store_level_present(), "storeLevel starts absent");
  check(fresh.read_store_level() == 0 && fresh.store_level_present() && table.at("storeLevel") == 0,
        "the creating read stores storeLevel at 0 in the bound table");
  check(it::VendorStock::literals("hub") == std::vector<std::int32_t>({0, 8, 7, 6, 5, 7, 6}) &&
            it::VendorStock::literals("hub2") == it::VendorStock::literals("hub") &&
            it::VendorStock::literals("stobonus") == std::vector<std::int32_t>({7}) &&
            it::VendorStock::literals("islgnome") == std::vector<std::int32_t>({3}),
        "authored literal setv values");
  fresh.apply_literal_setv("hub", 3);
  check(table.at("storeLevel") == 6, "hub literal index 3 stores 6");
  fresh.apply_literal_setv("hub2", 1);
  check(fresh.read_store_level() == 8, "hub2 uses the same literal list");
  fresh.apply_literal_setv("stobonus", 0);
  check(fresh.read_store_level() == 7, "stobonus literal is 7");
  fresh.apply_literal_setv("islgnome", 0);
  check(fresh.read_store_level() == 3, "islgnome literal is 3");
  check(threw_reason([&] { fresh.apply_literal_setv("islgnome", 1); }, "outside the authored list") &&
            fresh.read_store_level() == 3,
        "an extra literal index is refused");
  check(threw_reason([&] { fresh.apply_non_literal_setv("hub"); }, "non-literal storeLevel setv in hub/hub2") &&
            fresh.read_store_level() == 3,
        "the hub non-literal setv is refused");
  check(threw_reason([&] { fresh.apply_non_literal_setv("hub2"); }, "non-literal storeLevel setv in hub/hub2"),
        "the hub2 non-literal setv is refused");
  check(threw_reason([&] { fresh.literal_current_on_visit("hub"); },
                     "which storeLevel literal is current on a visit"),
        "which literal is current stays refused");
  check(threw_reason([&] { fresh.render_shop(); }, "rendered shop UI"),
        "rendered shop UI stays refused");
  check(threw_reason([&] { fresh.save_stock(); }, "save I/O"),
        "save I/O stays refused");
  fresh.apply_literal_setv("hub", 0);

  auto first = fresh.open_or_stock(1);
  check(first.generated && !first.refused && first.resource_requested &&
            !first.resource_completed &&
            first.resource_reason.find("resource completion unavailable") != std::string::npos,
        "mode 1 requests the shop resource and does not complete it");
  check(first.special_roll == std::array<bool, 3>{false, false, false},
        "cosmetic seed 7 rolls no special");
  check(first.counts[0] == 26 && first.counts[1] == 17 && first.counts[2] >= 3 &&
            first.counts[2] <= 9,
        "seed 7 counts stay inside the no-special ranges");
  check(first.failed[0] == 0 && first.failed[1] == 0 && first.bit40_rejected[0] == 0,
        "the seed 7 pass needs no unbounded retry");
  check(!(first.counts[0] == 1 && first.counts[1] == 0 && first.counts[2] == 1),
        "session stock does not force the restock-by-gold counts");
  const auto generated = packs(fresh);
  const auto generations = fresh.generations();
  auto kept = fresh.open_or_stock(0);
  check(kept.kept && !kept.generated && !kept.resource_requested &&
            fresh.generations() == generations && packs(fresh) == generated,
        "mode 0 keeps the three lists");
  fresh.set_seeds(7, 7);
  auto repeated = fresh.open_or_stock(1);
  check(repeated.generated && fresh.generations() == generations + 1 && packs(fresh) == generated,
        "a populated shop regenerates; there is no freshness gate");
  fresh.set_seeds(7, 7);
  table["storeLevel"] = 7;
  auto scaled = fresh.open_or_stock(1);
  check(scaled.generated && scaled.budget.low == 44572 && packs(fresh) != generated,
        "storeLevel from the bound table changes the generated lists");

  // A pass runs in steps and publishes nothing until it completes.
  // docs/spec/vendor-stock.md:217-219 (resumable reference scheduler).
  {
    auto stepped = fx::ready(7, 7);
    auto started = stepped.begin(1);
    check(started.pending && !started.generated && stepped.pending() &&
              stepped.count(0) == 0 && stepped.generations() == 0,
          "begin starts a pass without publishing");
    check(threw_reason([&] { stepped.begin(1); }, "already in progress"),
          "a second pass cannot start while one is pending");
    std::uint64_t steps = 0;
    auto out = started;
    while (out.pending) {
      out = stepped.step();
      ++steps;
      if (out.pending)
        check(stepped.count(0) == 0 && stepped.shared_seed() == 7,
              "an unfinished pass has published nothing");
    }
    check(out.generated && steps > 26 && stepped.steps_taken() == steps &&
              packs(stepped) == generated && stepped.generations() == 1,
          "the stepped pass equals the synchronous pass");
    check(threw_reason([&] { stepped.step(); }, "no stock pass"), "stepping with no pass is refused");
    stepped.set_seeds(7, 7);
    stepped.begin(1);
    stepped.step();
    stepped.cancel();
    check(!stepped.pending() && packs(stepped) == generated && stepped.generations() == 1,
          "a cancelled pass publishes nothing");
  }

  auto mode2 = fx::ready(7, 7);
  auto mode3 = fx::ready(7, 7);
  check(mode2.open_or_stock(2).generated && mode3.open_or_stock(3).generated &&
            packs(mode2) == packs(mode3) && packs(mode2) == generated,
        "modes 1, 2 and 3 generate the same records");

  auto other_seed = fx::ready(20973, 20973);
  auto seed_20973 = other_seed.open_or_stock(1);
  check(seed_20973.generated && seed_20973.special_roll == std::array<bool, 3>{false, false, false} &&
            seed_20973.counts[0] == 26 && seed_20973.counts[1] == 17,
        "seed 20973 regenerates the weapon and armour counts");

  auto cloth = fx::ready(7, 7);
  cloth.set_party(1, {4, 0, 0, 0});
  check(cloth.open_or_stock(1).generated && cloth.shared_seed() != first.shared_seed,
        "class 4 changes the armour fraction and the shared seed");

  // The seven fixed records are the parser's own bytes and do not keep the
  // 0xa5 image (docs/spec/vendor-stock.md:280-285). Generated records are
  // told apart by the executed-residue tag at +0x44.
  auto stale = fx::ready(7, 7);
  stale.fill_storage(0xa5);
  stale.set_executed_residue(fx::residue(true));
  check(stale.open_or_stock(1).generated, "generation replaces a stale destination");
  bool fixed_clean = true;
  int fixed_seen = 0;
  for (int category = 0; category < 3; ++category) {
    for (std::uint32_t i = 0; i < stale.count(category); ++i) {
      const auto &raw = stale.record_bytes(category, i);
      if (tag(raw) != 0)
        continue;
      ++fixed_seen;
      if (raw != stale.item(category, i).pack() || raw[6] == 0xa5 || !stale.record_known(category, i).all())
        fixed_clean = false;
    }
  }
  check(fixed_clean && fixed_seen == 7, "the seven fixed records do not keep the old destination bytes");

  // VS-2: without executed residue the generator's unwritten bytes are
  // unknown (docs/spec/items.md:83-95,724). A duplicate decision that would
  // read them refuses, and nothing is published.
  {
    auto unknown = fx::ready(7, 7);
    unknown.set_executed_residue({});
    auto r = unknown.open_or_stock(1);
    check(r.refused && has(r.reason, "duplicate test depends on record bytes") &&
              unknown.generations() == 0 && unknown.count(0) == 0 && unknown.shared_seed() == 7,
          "a defined-field duplicate against unknown +0x22 refuses");
    // A scan of a previous third list that was never written refuses too.
    auto unwritten = it::VendorStock(fx::tables());
    unwritten.set_seeds(7, 7);
    unwritten.set_difficulty(1);
    unwritten.set_party(1, {0, 0, 0, 0});
    unwritten.set_fixed(fx::fixed_text());
    std::map<std::string, std::int32_t> quest;
    unwritten.bind_quest_values(&quest);
    unwritten.set_executed_residue(fx::residue());
    auto u = unwritten.open_or_stock(1);
    check(u.refused && has(u.reason, "never written") && u.counts[1] == 0 &&
              unwritten.generations() == 0,
          "the armour scan of an unwritten previous third list refuses");
  }

  std::array<std::uint8_t, it::ITEM_SIZE> name{};
  const char label[] = "entry-point-name-long";
  std::memcpy(name.data(), label, sizeof(label));
  auto named = fx::ready(7, 7);
  named.plant_name_record(0, name);
  check(named.open_or_stock(1).generated && packs(named) == generated,
        "a deferred name in the category-2 scan is not a duplicate");

  auto scan_a = fx::ready(7, 7);
  check(scan_a.open_or_stock(1).armour_scan_rejections == 0, "an empty previous third list rejects nothing");
  const auto candidate = scan_a.armour_candidates().at(1);
  auto scan_b = fx::ready(7, 7);
  scan_b.plant_record(2, 0, candidate);
  auto scanned = scan_b.open_or_stock(1);
  check(scanned.generated && scanned.armour_scan_rejections > 0 &&
            scan_b.shared_seed() != scan_a.shared_seed(),
        "armour generation scans the previous third list before replacing it");

  auto special = fx::ready(8, 8);
  auto special_result = special.open_or_stock(1);
  check(special_result.generated &&
            special_result.special_roll == std::array<bool, 3>{false, true, false} &&
            special_result.special_budget[1] == 32998 && special_result.counts[1] == 18,
        "seed 8 appends the category-1 special at the n=17 budget");

  auto residue_def = std::make_shared<it::Definition>();
  residue_def->addr = 0x4242;
  residue_def->name = "residue";
  residue_def->price = 4242;
  residue_def->count = 1;
  residue_def->category = 1;
  it::Item residue(residue_def);
  fx::Options no_presets;
  no_presets.presets = false;
  auto residue_stock = fx::ready(8, 8, no_presets);
  residue_stock.plant_record(1, 17, residue);
  auto residue_result = residue_stock.open_or_stock(1);
  bool found_residue = false;
  for (std::uint32_t i = 0; i < residue_stock.count(1); ++i)
    found_residue = found_residue || residue_stock.item(1, i).definition == residue_def;
  check(residue_result.generated && residue_result.counts[1] == 18 && found_residue,
        "a failed special keeps a nonzero destination pointer and does not invent residue");
  {
    // Special adoption of a slot that was never written refuses. The old
    // third list is a known planted image so armour can scan it.
    auto adopt = it::VendorStock(fx::tables(no_presets));
    adopt.set_seeds(8, 8);
    adopt.set_difficulty(1);
    adopt.set_party(1, {0, 0, 0, 0});
    adopt.set_fixed(fx::fixed_text());
    std::map<std::string, std::int32_t> quest;
    adopt.bind_quest_values(&quest);
    adopt.set_executed_residue(fx::residue());
    for (std::size_t i = 0; i < 17; ++i)
      adopt.plant_record(2, i, residue);
    auto a = adopt.open_or_stock(1);
    check(a.refused && has(a.reason, "special adoption reads a stock slot that was never written") &&
              a.special_roll[1] && adopt.generations() == 0,
          "a failed special over an unwritten slot refuses");
  }

  // VS-1: a weapon/armour generator failure or a weapon bit-40 rejection
  // retries the same progress step, and the research gives that retry no
  // bound (docs/spec/vendor-stock.md:117-123,217-219). The pass refuses at
  // the first one and publishes nothing.
  fx::Options no_ordinary;
  no_ordinary.ordinary = false;
  auto blocked = fx::ready(99, 99, no_ordinary);
  auto blocked_result = blocked.open_or_stock(1);
  check(blocked_result.refused &&
            has(blocked_result.reason, "unbounded generation failure: a generator failure in list 0") &&
            has(blocked_result.reason, "does not bound") && blocked_result.failed[0] == 1 &&
            blocked.generations() == 0 && blocked.shared_seed() == 99 && blocked.count(0) == 0,
        "the first weapon generator failure refuses the pass");
  fx::Options armour_only_fails;
  armour_only_fails.ordinary_names = {"w0", "w1", "w2", "wx0", "wx1", "wx2"};
  auto armour_blocked = fx::ready(7, 7, armour_only_fails);
  auto armour_result = armour_blocked.open_or_stock(1);
  check(armour_result.refused && has(armour_result.reason, "generator failure in list 1") &&
            armour_result.failed[1] == 1 && armour_result.failed[0] == 0 &&
            armour_blocked.generations() == 0,
        "the first armour generator failure refuses the pass");
  fx::Options bit40;
  bit40.bit40 = true;
  auto flagged = fx::ready(7, 7, bit40);
  auto flagged_result = flagged.open_or_stock(1);
  check(flagged_result.refused && has(flagged_result.reason, "bit-40 rejection in list 0") &&
            flagged_result.bit40_rejected[0] == 1 && flagged_result.failed[0] == 0 &&
            flagged.generations() == 0,
        "the first bit-40 weapon rejection refuses the pass");
  fx::Options some_bit40;
  some_bit40.bit40_names = {"w0", "w1"};
  auto partial = fx::ready(7, 7, some_bit40);
  auto partial_result = partial.open_or_stock(1);
  check(partial_result.refused == (partial_result.bit40_rejected[0] == 1) &&
            partial_result.bit40_rejected[0] <= 1,
        "a partly flagged weapon list refuses exactly when a bit-40 candidate appears");
  if (partial_result.generated) {
    int fixed_flagged = 0;
    for (std::uint32_t i = 0; i < partial.count(0); ++i)
      if (partial.item(0, i).flags & (1ull << 40))
        ++fixed_flagged;
    check(fixed_flagged == 3, "only the fixed records keep bit 40");
  } else {
    check(partial.generations() == 0 && partial.count(0) == 0, "the refused flagged pass publishes nothing");
  }

  // Bounded duplicate retries: six rejections, then the seventh passes
  // (docs/spec/vendor-stock.md:117-123). Only the three cheap weapon
  // definitions are eligible, which makes many candidates duplicates. The accepted list is replayed independently
  // from the generator results with ref_vendor_stock's outer control.
  {
    fx::Options one_weapon;
    one_weapon.ordinary_names = {"w0", "w1", "w2", "a0", "a1", "a2", "a3", "a4", "a5", "c0", "c1",
                                 "c2", "c3", "c4", "c5", "o0"};
    auto dup = fx::ready(7, 7, one_weapon);
    dup.set_executed_residue(fx::residue(true));
    auto r = dup.open_or_stock(1);
    check(r.generated && r.counts[0] == 26,
          "the one-weapon pass completes with 26 weapons: " + r.reason);
    const auto &cands = dup.candidates(0);
    std::vector<Record> list;
    // The fixed records are the published records without a residue tag.
    for (std::uint32_t i = 0; i < dup.count(0); ++i)
      if (tag(dup.record_bytes(0, i)) == 0)
        list.push_back(dup.record_bytes(0, i));
    check(list.size() == 4, "four fixed weapon records");
    int retries = 0, rejected = 0, seventh = 0;
    std::vector<std::uint32_t> accepted;
    for (std::size_t k = 0; k < cands.size(); ++k) {
      Record raw = cands[k].pack(fx::residue(true)({0, false, k}, cands[k]));
      bool duplicate = false;
      for (const auto &old : list)
        duplicate = duplicate || ref_same_raw(old, raw);
      if (duplicate && retries < 6) {
        ++retries;
        ++rejected;
        continue;
      }
      if (duplicate)
        ++seventh;
      list.push_back(raw);
      accepted.push_back(static_cast<std::uint32_t>(k + 1));
      retries = 0;
    }
    std::vector<std::uint32_t> published;
    for (std::uint32_t i = 0; i < dup.count(0); ++i)
      if (tag(dup.record_bytes(0, i)) != 0)
        published.push_back(tag(dup.record_bytes(0, i)));
    std::sort(published.begin(), published.end());
    check(published == accepted && r.rejected[0] == rejected,
          "accepted weapons match an independent replay of the duplicate allowance");
    check(seventh > 0, "the replay reaches a seventh duplicate that passes");
  }

  // VS-6: generation itself sorts by ascending signed value, keeps equal
  // values in generation order, and moves every record byte with its item.
  // docs/spec/vendor-stock.md:148-152; ref_vendor_stock.py:149-158.
  {
    auto sorted = fx::ready(7, 7);
    sorted.set_executed_residue(fx::residue(true));
    check(sorted.open_or_stock(1).generated, "the tagged pass completes");
    const auto values_tables = fx::tables();
    bool ascending = true, moved = true, tie_order = true, unsorted_input = false;
    int generated_ties = 0;
    for (int category = 0; category < 3; ++category) {
      const auto n = sorted.count(category);
      std::vector<std::pair<std::uint32_t, std::int32_t>> by_tag;
      for (std::uint32_t i = 0; i < n; ++i) {
        const auto &raw = sorted.record_bytes(category, i);
        const auto &item = sorted.item(category, i);
        const auto value = it::item_value(values_tables, item);
        if (item.pack(raw) != raw)
          moved = false;
        const auto t = tag(raw);
        if (t && raw[0x46] == 0) {
          const auto &cands = sorted.candidates(category);
          if (t > cands.size() || cands[t - 1].pack() != item.pack())
            moved = false;
          by_tag.emplace_back(t, value);
        }
        if (i + 1 < n) {
          const auto &next_raw = sorted.record_bytes(category, i + 1);
          const auto next_value = it::item_value(values_tables, sorted.item(category, i + 1));
          if (value > next_value)
            ascending = false;
          if (value == next_value) {
            const auto a = tag(raw), b = tag(next_raw);
            // Fixed records (tag 0) precede generated ones in generation order.
            if (a && b) {
              ++generated_ties;
              if (a >= b)
                tie_order = false;
            } else if (a && !b) {
              tie_order = false;
            }
          }
        }
      }
      std::sort(by_tag.begin(), by_tag.end());
      for (std::size_t k = 1; k < by_tag.size(); ++k)
        if (by_tag[k - 1].second > by_tag[k].second)
          unsorted_input = true;
    }
    check(ascending, "published lists ascend by signed value");
    check(moved, "each published record's bytes belong to its item");
    check(tie_order && generated_ties > 0, "equal values keep generation order");
    check(unsorted_input, "generation order was not already sorted");
  }

  // VS-5: storeLevel bound to the real level-flow quest values. The act
  // clear is level_flow::begin_change's, guarded by card latch and death
  // task. docs/spec/vendor-stock.md:45-47,305-322.
  {
    lf::State s;
    s.acts = {{{"hub", 100}, {"aa", 50}}, {{"hub", 100}, {"bb", 70}}};
    s.act = 0;
    s.level = "aa";
    s.quest_values["storeLevel"] = 7;
    auto shop = fx::ready(7, 7);
    shop.bind_quest_values(&s.quest_values);
    auto before = shop.open_or_stock(1);
    check(before.generated && before.store_level_reads == std::array<std::int32_t, 2>{7, 7} &&
              before.budget.low == 44572,
          "the level-flow storeLevel 7 drives the budget");
    s.pending.request("bb");
    lf::ChangeOptions latch;
    latch.card_latch = true;
    auto r = lf::begin_change(s, latch);
    shop.on_act_reset(r.reset_act);
    shop.set_seeds(7, 7);
    auto latched = shop.open_or_stock(1);
    check(r.act_changed && !r.reset_act && s.quest_values.at("storeLevel") == 7 &&
              latched.budget.low == 44572,
          "a card latch keeps storeLevel through the act change");
    s.pending.request("aa");
    lf::ChangeOptions death;
    death.death_task = true;
    r = lf::begin_change(s, death);
    shop.on_act_reset(r.reset_act);
    shop.set_seeds(7, 7);
    auto died = shop.open_or_stock(1);
    check(r.act_changed && !r.reset_act && s.quest_values.at("storeLevel") == 7 &&
              died.budget.low == 44572,
          "a death task keeps storeLevel through the act change");
    check(threw_reason([&] { shop.apply_non_literal_setv("hub2"); }, "non-literal") &&
              shop.store_level_unestablished(),
          "a non-literal hub2 store marks storeLevel unknown");
    auto tainted = shop.open_or_stock(1);
    check(tainted.refused && has(tainted.reason, "non-literal setv") && !shop.pending(),
          "stock after a non-literal store refuses");
    s.pending.request("bb");
    r = lf::begin_change(s, {});
    shop.on_act_reset(r.reset_act);
    shop.set_seeds(7, 7);
    auto cleared = shop.open_or_stock(1);
    check(r.reset_act && s.quest_values.at("storeLevel") == 0 && !shop.store_level_unestablished() &&
              cleared.generated && cleared.store_level_reads == std::array<std::int32_t, 2>{0, 0} &&
              cleared.budget.low == 1199 && packs(shop) == generated,
          "the level-flow act clear zeroes storeLevel and the next pass uses the storeLevel-0 budget");
    check(threw_reason([&] { shop.apply_non_literal_setv("hub"); }, "non-literal") &&
              shop.store_level_unestablished(),
          "the hub store also marks storeLevel unknown");
    shop.apply_literal_setv("stobonus", 0);
    check(!shop.store_level_unestablished() && s.quest_values.at("storeLevel") == 7,
          "a later literal store replaces the unknown value");
    shop.note_unclassified_store("synthetic");
    auto unclassified = shop.open_or_stock(1);
    check(unclassified.refused && has(unclassified.reason, "not classified as an authored literal"),
          "an unclassified script store blocks the next pass");
  }

  const std::array<std::string, 4> names{"shop-a", "shop-b", "shop-c", "shop-d"};
  auto loaded = fx::ready(7, 7);
  loaded.set_destinations(names);
  check(!loaded.on_load("elsewhere").matched && loaded.generations() == 0,
        "a non-matching destination does not generate");
  const int modes[4] = {1, 1, 2, 3};
  for (int i = 0; i < 4; ++i) {
    loaded.set_seeds(7, 7);
    auto result = loaded.on_load(names[static_cast<std::size_t>(i)]);
    check(result.matched && result.generated && result.mode == modes[i],
          std::string("load mode for ") + names[static_cast<std::size_t>(i)]);
  }
  const auto loads = loaded.generations();
  loaded.set_seeds(7, 7);
  check(loaded.on_load("shop-a").generated && loaded.generations() == loads + 1,
        "the same shop destination regenerates on the next load");
  check(threw_reason([&] { loaded.open_or_stock(4); }, "unestablished stock mode"),
        "an unknown mode is refused");

  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "vendor-stock-exact ok\n";
  return 0;
}
