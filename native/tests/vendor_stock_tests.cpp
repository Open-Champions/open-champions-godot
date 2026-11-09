#include "vendor_stock_fixture.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <string>

namespace it = champions::simulation::rta::items;
namespace fx = it::vendor_fixture;

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
std::vector<std::array<std::uint8_t, it::ITEM_SIZE>> packs(const it::VendorStock &stock) {
  std::vector<std::array<std::uint8_t, it::ITEM_SIZE>> out;
  for (int category = 0; category < 3; ++category)
    for (std::uint32_t i = 0; i < stock.count(category); ++i)
      out.push_back(stock.record_bytes(category, i));
  return out;
}
it::Definition priced(int price, std::uint8_t suffix) {
  it::Definition d;
  d.addr = 0x1000u + suffix;
  d.price = price;
  d.count = 1;
  d.category = 2;
  return d;
}
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
  check(paid.first == 41 && paid.second, "debit is 59 and happens before giveItem");

  auto fresh = fx::ready(7, 7);
  check(!fresh.store_level_present(), "storeLevel starts absent");
  check(fresh.read_store_level() == 0 && fresh.store_level_present(),
        "the creating read stores storeLevel at 0");
  check(it::VendorStock::literals("hub") == std::vector<std::int32_t>({0, 8, 7, 6, 5, 7, 6}) &&
            it::VendorStock::literals("hub2") == it::VendorStock::literals("hub") &&
            it::VendorStock::literals("stobonus") == std::vector<std::int32_t>({7}) &&
            it::VendorStock::literals("islgnome") == std::vector<std::int32_t>({3}),
        "authored literal setv values");
  fresh.apply_literal_setv("hub", 3);
  check(fresh.read_store_level() == 6, "hub literal index 3 stores 6");
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
  fresh.plant_store_level(9);
  fresh.on_act_change(true, true, false);
  check(fresh.read_store_level() == 9, "a card latch keeps storeLevel");
  fresh.on_act_change(true, false, true);
  check(fresh.read_store_level() == 9, "a death task keeps storeLevel");
  fresh.on_act_change(false, false, false);
  check(fresh.read_store_level() == 9, "same-act travel keeps storeLevel");
  fresh.on_act_change(true, false, false);
  check(fresh.read_store_level() == 0 && fresh.store_level_present(),
        "a guarded act change zeros storeLevel");
  check(threw_reason([&] { fresh.render_shop(); }, "rendered shop UI"),
        "rendered shop UI stays refused");
  check(threw_reason([&] { fresh.save_stock(); }, "save I/O"),
        "save I/O stays refused");

  fresh.plant_store_level(0);
  fresh.set_seeds(7, 7);
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
  check(!(first.counts[0] == 1 && first.counts[1] == 0 && first.counts[2] == 1),
        "session stock does not force the restock-by-gold counts");
  const auto generated = packs(fresh);
  const auto generations = fresh.generations();
  auto kept = fresh.open_or_stock(0);
  check(kept.kept && !kept.generated && !kept.resource_requested &&
            fresh.generations() == generations && packs(fresh) == generated,
        "mode 0 keeps the three lists");
  fresh.set_seeds(7, 7);
  fresh.plant_store_level(0);
  auto repeated = fresh.open_or_stock(1);
  check(repeated.generated && fresh.generations() == generations + 1 && packs(fresh) == generated,
        "a populated shop regenerates; there is no freshness gate");
  fresh.set_seeds(7, 7);
  fresh.plant_store_level(7);
  auto scaled = fresh.open_or_stock(1);
  check(scaled.generated && packs(fresh) != generated, "storeLevel changes the generated lists");

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

  auto stale = fx::ready(7, 7);
  stale.fill_storage(0xa5);
  check(stale.open_or_stock(1).generated, "generation replaces a stale destination");
  bool stale_cleared = true;
  for (int category = 0; category < 3 && stale_cleared; ++category) {
    for (std::uint32_t i = 0; i < stale.count(category); ++i) {
      const auto &raw = stale.record_bytes(category, i);
      if (raw != stale.item(category, i).pack() || raw[6] == 0xa5)
        stale_cleared = false;
    }
  }
  check(stale_cleared, "fixed and generated records do not keep the old destination bytes");

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

  // ref_vendor_stock.py:107-134: weapon and armour failures and outer
  // rejections retry the same progress with no overall bound. The port
  // refuses only when its watchdog is reached, and publishes nothing.
  // docs/spec/vendor-stock.md:117-123,217-219,341.
  fx::Options no_ordinary;
  no_ordinary.ordinary = false;
  auto blocked = fx::ready(99, 99, no_ordinary);
  blocked.plant_store_level(0);
  blocked.set_retry_watchdog(64);
  auto blocked_result = blocked.open_or_stock(1);
  check(blocked_result.refused &&
            blocked_result.reason.find("unbounded generation failure") != std::string::npos &&
            blocked_result.failed[0] == 64 &&
            blocked.generations() == 0 && blocked.shared_seed() == 99 && blocked.count(0) == 0,
        "weapon generation failure retries and refuses only at the port watchdog");
  check(threw_reason([&] { blocked.set_retry_watchdog(0); }, "retry watchdog"),
        "a zero watchdog is rejected");
  fx::Options bit40;
  bit40.bit40 = true;
  auto flagged = fx::ready(7, 7, bit40);
  flagged.set_retry_watchdog(64);
  auto flagged_result = flagged.open_or_stock(1);
  check(flagged_result.refused &&
            flagged_result.reason.find("unbounded generation failure") != std::string::npos &&
            flagged_result.bit40_rejected[0] > 0 && flagged.generations() == 0,
        "bit 40 on every weapon rejects without bound and is refused at the watchdog");
  fx::Options some_bit40;
  some_bit40.bit40_names = {"w0", "w1"};
  auto partial = fx::ready(7, 7, some_bit40);
  auto partial_result = partial.open_or_stock(1);
  int fixed_flagged = 0;
  for (std::uint32_t i = 0; i < partial.count(0); ++i)
    if (partial.item(0, i).flags & (1ull << 40))
      ++fixed_flagged;
  check(partial_result.generated && partial_result.counts[0] == 26 &&
            partial_result.bit40_rejected[0] > 0 && fixed_flagged == 3 &&
            partial.shared_seed() != first.shared_seed,
        "a bit-40 weapon is rejected and retried; only the fixed records keep it");

  // A rejected category-2 candidate stays in its slot; a failed special then
  // adopts that nonzero pointer. ref_vendor_stock.py:110-113,135-145.
  // Cosmetic seed 12 rolls only the third special.
  auto plain = fx::ready(7, 12, no_presets);
  auto plain_result = plain.open_or_stock(1);
  check(plain_result.generated &&
            plain_result.special_roll == std::array<bool, 3>{false, false, true} &&
            plain_result.rejected[2] == 0 && plain.candidates(2).size() == 6,
        "cosmetic seed 12 rolls only the category-2 special");
  auto shadow = fx::ready(7, 12, no_presets);
  for (std::size_t i = 0; i < plain.candidates(2).size(); ++i)
    shadow.plant_name_record(i, plain.candidates(2)[i].pack());
  auto shadow_result = shadow.open_or_stock(1);
  const auto last = shadow.candidates(2).back().pack();
  bool adopted = false;
  for (std::uint32_t i = 0; i < shadow.count(2); ++i)
    adopted = adopted || shadow.record_bytes(2, i) == last;
  check(shadow_result.generated && shadow_result.rejected[2] > 0 &&
            shadow_result.counts[2] ==
                3u + std::uint32_t(6 - shadow_result.rejected[2]) + 1u &&
            adopted,
        "a duplicate in the name region is rejected, kept in its slot, and adopted by a failed special");

  // storeLevel bound to the session quest variables.
  // docs/spec/vendor-stock.md:305-322.
  std::map<std::string, std::int32_t> quest;
  auto bound = fx::ready(7, 7);
  bound.bind_quest_values(&quest);
  check(!bound.store_level_present(), "bound storeLevel starts absent");
  check(bound.open_or_stock(1).generated && quest.at("storeLevel") == 0 &&
            packs(bound) == generated,
        "the creating read stores storeLevel 0 in the quest variables");
  bound.apply_literal_setv("hub", 1);
  check(quest.at("storeLevel") == 8, "a literal setv writes the quest variable");
  for (auto &value : quest)
    value.second = 0;
  check(bound.read_store_level() == 0, "the level-flow act clear zeroes the bound variable");
  check(threw_reason([&] { bound.apply_non_literal_setv("hub2"); }, "non-literal"),
        "a non-literal hub2 store stays refused");
  auto tainted = bound.open_or_stock(1);
  check(tainted.refused && tainted.reason.find("non-literal setv") != std::string::npos &&
            bound.generations() == 1 && packs(bound) == generated,
        "stock after a non-literal store refuses and keeps the lists");
  bound.on_act_change(true, true, false);
  check(bound.store_level_unestablished(), "a card latch does not clear the unknown store");
  bound.on_act_change(true, false, false);
  check(!bound.store_level_unestablished() && quest.at("storeLevel") == 0 &&
            bound.open_or_stock(1).generated,
        "an act clear replaces the unknown store with 0");
  check(threw_reason([&] { bound.apply_non_literal_setv("hub"); }, "non-literal") &&
            bound.store_level_unestablished(),
        "the hub store also marks storeLevel unknown");
  bound.apply_literal_setv("stobonus", 0);
  check(!bound.store_level_unestablished() && quest.at("storeLevel") == 7,
        "a later literal store replaces the unknown value");

  const std::array<std::string, 4> names{"shop-a", "shop-b", "shop-c", "shop-d"};
  auto loaded = fx::ready(7, 7);
  loaded.set_destinations(names);
  check(!loaded.on_load("elsewhere").matched && loaded.generations() == 0,
        "a non-matching destination does not generate");
  const int modes[4] = {1, 1, 2, 3};
  for (int i = 0; i < 4; ++i) {
    loaded.set_seeds(7, 7);
    loaded.plant_store_level(0);
    auto result = loaded.on_load(names[static_cast<std::size_t>(i)]);
    check(result.matched && result.generated && result.mode == modes[i],
          std::string("load mode for ") + names[static_cast<std::size_t>(i)]);
  }
  const auto loads = loaded.generations();
  loaded.set_seeds(7, 7);
  loaded.plant_store_level(0);
  check(loaded.on_load("shop-a").generated && loaded.generations() == loads + 1,
        "the same shop destination regenerates on the next load");
  check(threw_reason([&] { loaded.open_or_stock(4); }, "unestablished stock mode"),
        "an unknown mode is refused");

  auto low = std::make_shared<it::Definition>(priced(1, 1));
  auto low_again = std::make_shared<it::Definition>(priced(1, 2));
  auto high_item = std::make_shared<it::Definition>(priced(5, 3));
  std::vector<it::Item> rows{it::Item(high_item), it::Item(low), it::Item(low_again)};
  rows[0].count = rows[1].count = rows[2].count = 1;
  rows[0].suffix = 3;
  rows[1].suffix = 1;
  rows[2].suffix = 2;
  it::sort_stock_by_value(fx::tables(), rows);
  check(rows[0].suffix == 1 && rows[1].suffix == 2 && rows[2].suffix == 3,
        "equal values keep their order under the stable value sort");

  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "vendor-stock-exact ok\n";
  return 0;
}
