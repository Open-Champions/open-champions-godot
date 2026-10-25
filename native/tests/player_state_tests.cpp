#include "champions/player_state.hpp"
#include "champions/combat.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace p = champions::simulation::rta::player_state;
namespace c = champions::simulation::rta::combat;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
namespace {
void put16(std::vector<std::uint8_t> &b, std::size_t at, std::uint16_t value) {
  b[at] = std::uint8_t(value);
  b[at + 1] = std::uint8_t(value >> 8);
}
void put32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t value) {
  for (int n = 0; n < 4; ++n) b[at + std::size_t(n)] = std::uint8_t(value >> (8 * n));
}
// An authored ELF segment maps the two documented addresses. The MIPS words
// are only the bounded instruction kinds read_tables accepts; no retail bytes.
std::vector<std::uint8_t> synthetic_elf() {
  constexpr std::uint32_t base = 0x1b0000, chain = 0x1b6740, xp = 0x41b558;
  constexpr std::size_t offset = 0x100;
  std::vector<std::uint8_t> bytes(offset + xp + 81 * 4 - base, 0);
  bytes[0] = 0x7f; bytes[1] = 'E'; bytes[2] = 'L'; bytes[3] = 'F'; bytes[4] = 1; bytes[5] = 1;
  put32(bytes, 0x1c, 0x34); put16(bytes, 0x2a, 0x20); put16(bytes, 0x2c, 1);
  put32(bytes, 0x34, 1); put32(bytes, 0x38, offset); put32(bytes, 0x3c, base);
  put32(bytes, 0x44, std::uint32_t(bytes.size() - offset));
  for (int level = 0; level <= 80; ++level)
    put32(bytes, offset + xp - base + std::size_t(level * 4), std::uint32_t(level * 100));
  std::size_t at = offset + chain - base;
  auto emit = [&](std::uint32_t word) { put32(bytes, at, word); at += 4; };
  const std::array<int, 6> fp = {23, 24, 25, 22, 20, 21};
  const std::array<int, 6> constants = {13, 2, 6, 14, 3, 65};
  for (std::size_t n = 0; n < fp.size(); ++n) {
    const auto bits = c::itof(constants[n]);
    emit(0x3c080000u | (bits >> 16)); // lui t0, high half
    emit(0x35080000u | (bits & 0xffffu)); // ori t0, low half
    emit(0x44880000u | (std::uint32_t(fp[n]) << 11)); // mtc1 t0, fN
  }
  emit(0x8c080050u); // lw t0, +0x50(a0): class selector
  emit(0x24090002u); // addiu t1, zero, 2
  emit(0x15090003u); // bne t0,t1 over class-2 override
  emit(0); // delay slot
  emit(0x3c084180u); // class 2: hp per level 16.0
  emit(0x4488b000u); // mtc1 t0, f22
  return bytes;
}
void test_tables() {
  auto bytes = synthetic_elf();
  champions::simulation::rta::arena_modes::Executable exe;
  check(exe.load(bytes), "synthetic ELF loads");
  const auto table = p::read_tables(exe);
  check(table.experience[1] == 100 && table.experience[80] == 8000,
        "synthetic experience table read");
  check(table.formulas[0][3] == c::itof(14) && table.formulas[2][3] == c::itof(16) &&
        table.formulas[6][5] == c::itof(65), "synthetic class branch and formula walk");
  // The executable reader rejects a malformed curve before creation.
  constexpr std::size_t xp_at = 0x100 + 0x41b558 - 0x1b0000;
  put32(bytes, xp_at + 4, 0);
  check(exe.load(bytes), "malformed synthetic ELF loads");
  bool refused = false;
  try { (void)p::read_tables(exe); } catch (const p::Refusal &) { refused = true; }
  check(refused, "nonincreasing synthetic XP threshold refused");
}
} // namespace
int main() {
  try {
    test_tables();
    p::Tables t;
    for (int n = 0; n < 80; ++n) t.experience[std::size_t(n)] = n * n * 1000;
    t.experience[80] = 99999999;
    for (auto &f : t.formulas)
      f = {c::itof(13), c::itof(2), c::itof(6), c::itof(14), c::itof(3), c::itof(65)};
    const auto made = p::create("Mira", 1, 4, {{46, 37, 24, 28}}, t);
    check(made.level == 1 && made.acknowledged == 1 && made.experience == 0 && made.gold == 500,
          "new character's documented starting fields");
    check(made.max_mana == c::itof(54) && made.max_hp == c::itof(33),
          "new character's maximums use class formula and base statistics");
    check(!made.current_known, "undocumented current values remain unset");
    auto imported = made;
    imported.experience = 4000;
    imported.gold = 8765;
    imported.effective_stats[0] += 7;
    imported.hp = c::itof(20); imported.mana = c::itof(15);
    imported = p::import(imported, t);
    check(imported.imported && imported.current_known && imported.gold == 8765 && imported.mana == c::itof(15),
          "import retains record fields");
    // player_importCharacter's caps (level 50, table[50]); table[51] is fine for
    // a level reached in native play, but not for an import.
    auto too_high = made; too_high.level = 51; too_high.acknowledged = 51;
    bool capped = false;
    try { (void)p::import(too_high, t); } catch (const p::Refusal &) { capped = true; }
    check(capped, "import above level 50 refused");
    auto too_much = made; too_much.experience = t.experience[50] + 1;
    capped = false;
    try { (void)p::import(too_much, t); } catch (const p::Refusal &) { capped = true; }
    check(capped, "import above table[50] experience refused");
    auto at_cap = made; at_cap.level = at_cap.acknowledged = 50; at_cap.experience = t.experience[50];
    at_cap.hp = c::itof(1); at_cap.mana = c::itof(1);
    check(p::import(at_cap, t).imported, "import at the documented caps accepted");
    auto leveled = p::import(at_cap, t); leveled.level = 60; leveled.acknowledged = 60;
    leveled.experience = t.experience[59];
    check(p::load(p::save(leveled, t), t).level == 60, "imported record that levelled past 50 in native play still saves");
    const auto saved = p::save(imported, t);
    const auto restored = p::load(saved, t);
    check(restored.name == imported.name && restored.experience == imported.experience &&
          restored.effective_stats == imported.effective_stats &&
          restored.max_mana == imported.max_mana && restored.imported,
          "native companion round trip");
    bool refused = false;
    try { auto bad = saved; bad[0] ^= 1; (void)p::load(bad, t); }
    catch (const p::Refusal &) { refused = true; }
    check(refused, "unknown native companion version refused");
    refused = false;
    try { (void)p::create("", 1, 4, {{46, 37, 24, 28}}, t); }
    catch (const p::Refusal &) { refused = true; }
    check(refused, "empty name refused");
    refused = false;
    try { (void)p::create("Mira", 0, 4, {{46, 37, 24, 28}}, t); }
    catch (const p::Refusal &) { refused = true; }
    check(refused, "model and class mismatch refused");
    std::cout << "PASS player-state 16 checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL player-state: " << error.what() << '\n';
    return 1;
  }
}
