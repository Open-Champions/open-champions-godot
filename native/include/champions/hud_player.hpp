#pragma once
#include "champions/hud_extras.hpp"

namespace champions::hud_extras {
// Read-only view of one attached RTA Player, not player_state::Record and not
// an isolated session-control actor. Addresses/widths: research-hud-gaps
// f7c3964 hud-status.md/hud-xp.md/hud-quickslots.md/hud-gold.md, and audited
// research-hud-inputs 44c6720 docs/spec/hud-input-producers.md.
struct PlayerSnapshot {
  std::uint32_t object{}, flags{}, frame{}, flash{};
  Raw hp{}, max_hp{}, pending_hp{}, mana{}, max_mana{}, pending_mana{};
  std::int32_t acknowledged{}, level{}, experience{}, gold{}, cooldown{}, language{};
  std::array<std::int32_t, 2> quick_slots{};
  std::array<std::int32_t, 8> ability_ids{}, notice_timers{};
  std::array<std::int32_t, 2> potion_counts{};
};
using PlayerRead = std::function<std::uint64_t(std::uint32_t, unsigned)>;
using PlayerPresent = std::function<bool(std::uint32_t)>;
// The owner must supply actual live storage and object membership. Missing,
// stale, prediction-copy and nonplayer objects refuse before any HUD values
// are read. No writes, RNG draws, constructor or current-value clamp.
PlayerSnapshot observePlayer(int slot, const PlayerRead &, const PlayerPresent &);
GoldState observeGold(const PlayerRead &);
// Adopt authoritative payout/floater state without running tickGold again.
// Only hold/alpha and continuing floater motion belong to this display copy.
void syncLiveGold(GoldState &presentation, const GoldState &authoritative);
} // namespace champions::hud_extras
