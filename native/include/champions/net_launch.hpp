#pragma once
// The host bit of the net settings block and the launch record's +0xf4
// (research t3/research-items-gaps ecf9742, docs/spec/items-gaps.md section 2,
// tools/ref_items_gaps.py, executed against the original by
// tools/difftest_items_gaps.py suites lobby, menu, record, fill, receive).
//
// Only the machine that is player 0 has bit 0 of the settings flags, and only
// it fills launch-record fields +0xa0..+0xa3 and +0xf4 (its own
// game_scanCm1Vars(1)). Every machine then applies record 0 at the end of the
// launch; the word is copied to 0x4f7110 only in modes 2 and 3 (PvP mode A).
// These are pure functions on the documented bytes: nothing here talks to a
// network, and the session has no net launch yet.
#include <cstdint>
#include <functional>
#include <optional>

namespace champions::simulation::rta::net_launch {

// The net settings block 0x402a30 (docs/ui.md), the bytes this contract reads.
struct Settings {
  std::uint8_t flags = 0;       // +0x00: bit 0 host, bit 1 or 2 the kind of character
  std::uint8_t difficulty = 0;  // +0x02
  std::uint8_t mode = 0;        // +0x03
  std::uint8_t mission = 0;     // +0x04
  std::uint8_t kill_limit = 0;  // +0x05
  std::int8_t count = 0;        // +0x07 player count
  std::int8_t own = 0;          // +0x09 this machine's player number
  std::uint8_t byte_0a = 0;     // +0x0a, 0xff with the kind bits (meaning unknown)
};
inline constexpr std::uint8_t FLAG_HOST = 1, FLAG_KIND_A = 2, FLAG_KIND_B = 4;
inline constexpr std::uint32_t SETTINGS_ADDRESS = 0x402a30, LAUNCH_RECORDS = 0x402b90, LAUNCH_RECORD_SIZE = 0x144;
inline constexpr std::uint32_t RECORD_DIFFICULTY = 0xa0, RECORD_MODE = 0xa1, RECORD_KILL_LIMIT = 0xa2,
                               RECORD_MISSION = 0xa3, RECORD_CM1 = 0xf4;

// The lobby context's room bytes (+0x13e, +0x140, +0x142, +0x144) and own number (+0xc80).
struct Room {
  std::uint8_t difficulty = 0, mode = 0, kill_limit = 0, mission = 0;
};

// lobby_task 0x3276a4-0x327728. char_kind is the byte at 0x4fdba9: non-zero
// in its low two bits gives kind bit 4, otherwise kind bit 2.
void lobby_set_host(Settings &, std::int8_t lobby_own, const Room &, std::uint8_t char_kind);
// frontend_run 0x1e1b6c-0x1e1b90: selection is the menu routine's result.
void frontend_set_host(Settings &, bool selection);
// frontend_run 0x1e0d2c-0x1e0d5c.
void frontend_set_kind(Settings &, std::uint8_t char_kind);

// net_launchPrepare 0x1351d8-0x135208: the record this machine fills.
std::uint32_t own_record(std::int8_t own);

// The launch record fields this contract touches.
struct Record {
  std::uint8_t difficulty = 0, mode = 0, kill_limit = 0, mission = 0;  // +0xa0..+0xa3
  std::uint32_t cm1 = 0;                                               // +0xf4
};
// net_launchPrepare 0x1353f4-0x135430. Without the host bit nothing is written
// and game_scanCm1Vars is not called. With it the four fields are copied from
// the settings and +0xf4 is scan(1). Returns whether the scan was called.
bool launch_fill_host_fields(const Settings &, Record &, const std::function<std::int32_t()> &scan);

// net_launchUpdate 0x137464-0x137518, from record 0 (the host's).
struct Received {
  std::uint32_t difficulty = 0;  // g_difficultyIdx 0x4f719c = s8(+0xa0)
  std::uint8_t kill_limit = 0;   // 0x4f8408
  bool pvp_mode_a = false;       // 0x4f711c: mode 2 or 3
  bool pvp_mode_b = false;       // 0x4f7120: mode 1, 2 or 3
  bool mode_3 = false;           // 0x4f7124
  // game_set4f7110(+0xf4), called only when pvp_mode_a.
  std::optional<std::int32_t> set_cm1;
};
Received launch_receive(const Record &record0);

// The 0x4f7110 word a machine holds when the level starts: the host's scan in
// modes 2 and 3, this machine's own word otherwise (Inferred in the research
// from the executed fragments).
std::int32_t cm1_word_after_launch(std::int32_t host_scan, std::int32_t own_word, std::uint8_t mode);

} // namespace champions::simulation::rta::net_launch
