#include "champions/hud_player.hpp"

namespace champions::hud_extras {
PlayerSnapshot observePlayer(int slot, const PlayerRead &read,
                             const PlayerPresent &present) {
  if (slot < 0 || slot >= 4 || !read || !present)
    throw Refusal("HUD Player needs a valid slot and live memory owner");
  const auto index = std::uint32_t(slot);
  // RTA's record, object and table addresses, not native host addresses.
  const auto record = 0x4d8af0u + 0x4c30u * index;
  PlayerSnapshot out;
  out.object = std::uint32_t(read(record + 0x43d8, 4));
  if (!out.object || !present(out.object))
    throw Refusal("HUD Player object is absent or no longer owned");
  out.flags = std::uint32_t(read(out.object + 0xc, 4));
  if (!(out.flags & 0x40) || (out.flags & 0x20000) ||
      read(out.object + 0x3c8, 4) != index)
    throw Refusal("HUD Player is not the real object for this slot");
  auto word = [&](std::uint32_t offset) {
    return std::uint32_t(read(record + offset, 4));
  };
  auto signed_value = [](std::uint64_t value, unsigned bits) {
    const auto mask = (std::uint64_t(1) << bits) - 1;
    value &= mask;
    return std::int32_t(value & (std::uint64_t(1) << (bits - 1))
                            ? std::int64_t(value) - std::int64_t(mask) - 1
                            : std::int64_t(value));
  };
  out.frame = std::uint32_t(read(0x44fdcc, 4));
  out.flash = std::uint32_t(read(0x41b930 + 4 * index, 4));
  out.hp = word(0xac); out.max_hp = word(0xb0); out.pending_hp = word(0xb4);
  out.mana = word(0xc0); out.max_mana = word(0xc4); out.pending_mana = word(0xc8);
  out.acknowledged = signed_value(read(record + 0x58, 1), 8);
  out.level = signed_value(read(record + 0x59, 1), 8);
  out.experience = signed_value(word(0x5c), 32);
  out.gold = signed_value(word(0xe4), 32);
  out.cooldown = signed_value(read(out.object + 0x4ae, 2), 16);
  out.language = signed_value(read(0x44fddc, 1), 8);
  for (std::uint32_t k = 0; k < 2; ++k) {
    out.quick_slots[k] = signed_value(read(record + 0x4a84 + k, 1), 8);
    out.potion_counts[k] = signed_value(read(0x41b940 + 0x10 * k + 4 * index, 4), 32);
  }
  for (std::uint32_t k = 0; k < 8; ++k) {
    out.ability_ids[k] = signed_value(read(record + 0x4440 + 0x68 * k + 0x20, 2), 16);
    out.notice_timers[k] = signed_value(read(0x41b828 + 0x20 * index + 4 * k, 4), 32);
  }
  return out;
}
GoldState observeGold(const PlayerRead &read) {
  if (!read) throw Refusal("HUD gold needs live memory");
  GoldState out;
  out.frame = std::uint32_t(read(0x44fdcc, 4));
  for (std::uint32_t k = 0; k < 4; ++k) {
    out.gold[k] = std::uint32_t(read(0x4d8af0 + 0x4c30 * k + 0xe4, 4));
    const auto word = std::uint32_t(read(0x86ace8 + 4 * k, 4));
    out.pending[k] = word <= INT32_MAX ? std::int32_t(word)
        : std::int32_t(std::int64_t(word) - 0x100000000LL);
  }
  for (std::uint32_t k = 0; k < FLOATER_COUNT; ++k) {
    auto &f = out.floaters[k];
    const auto at = 0x86a900u + 0x24u * k;
    f.amount = std::uint32_t(read(at + 0x1c, 4));
    if (!f.amount) continue;
    const auto slot = std::uint32_t(read(at + 0x20, 1));
    if (slot > 3) throw Refusal("live gold floater slot outside 0..3");
    f.slot = int(slot);
    f.start = std::uint32_t(read(at + 0x18, 4));
    for (std::uint32_t j = 0; j < 3; ++j) {
      f.pos[j] = std::uint32_t(read(at + 4 * j, 4));
      f.vel[j] = std::uint32_t(read(at + 0xc + 4 * j, 4));
    }
    f.offscreen = std::uint8_t(read(at + 0x21, 1));
  }
  return out;
}
void syncLiveGold(GoldState &view, const GoldState &source) {
  // A native continuation can rewind the ready-frame domain. Its display
  // history is not authoritative save data; discard the future local fades.
  if (source.frame < view.frame) view = GoldState{};
  for (std::size_t k = 0; k < view.floaters.size(); ++k) {
    const auto &incoming = source.floaters[k];
    auto &current = view.floaters[k];
    if (incoming.amount != current.amount || incoming.start != current.start ||
        incoming.slot != current.slot)
      current = incoming;
  }
  view.gold = source.gold;
  view.pending = source.pending;
  view.frame = source.frame;
}
} // namespace champions::hud_extras
