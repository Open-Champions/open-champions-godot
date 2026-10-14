#include "champions/session_recovery.hpp"
#include <algorithm>
namespace champions::simulation::rta::session_recovery {
void validate(const Initial &i) {
  if (i.slot != 0 || i.record != RECORD)
    throw player::Unsupported("recovery requires explicit slot 0 and record 0");
  if (i.player_class < 0 || i.player_class > 4)
    throw player::Unsupported(
        "recovery supports supplied class indices 0 through 4");
  if (std::any_of(i.equipment.begin(), i.equipment.end(),
                  [](auto v) { return v != 0; }))
    throw player::Unsupported("recovery requires empty equipment");
  if (i.ranks != std::vector<std::pair<std::int16_t, std::int16_t>>{{-1, 0}})
    throw player::Unsupported(
        "recovery requires supplied empty terminated rank list");
  if (i.spells != SpellPolicy::NoActiveSpells || !i.blessed_type_token)
    throw player::Unsupported(
        "recovery requires explicit NoActiveSpells and nonzero query token");
}
std::uint32_t passive_table(const Initial &i) {
  return 0x4ef538 + std::uint32_t(i.player_class) * 0xa88;
}
std::vector<creature_hp::Write> setup(std::uint32_t p, const Initial &i) {
  validate(i);
  if (!p || std::uint64_t(p) + 0x478 > UINT64_C(0x100000000))
    throw player::Unsupported("recovery actor span exceeds raw32");
  std::vector<creature_hp::Write> out;
  const auto put = [&](std::uint32_t a, std::size_t n, std::uint64_t v) {
    out.push_back({a, n, v});
  };
  put(p + 0x3c8, 4, i.slot);
  put(p + 0x475, 1, std::uint8_t(i.mana_bonus));
  put(p + 0x477, 1, std::uint8_t(i.health_bonus));
  put(i.record + 0x50, 4, std::uint32_t(i.player_class));
  for (const auto &v :
       std::array<std::pair<unsigned, Raw>, 8>{{{0xac, i.health},
                                                {0xb0, i.max_health},
                                                {0xb4, i.pending_health},
                                                {0xb8, i.health_rate},
                                                {0xbc, i.mana_rate},
                                                {0xc0, i.mana},
                                                {0xc4, i.max_mana},
                                                {0xc8, i.pending_mana}}})
    put(i.record + v.first, 4, v.second);
  for (unsigned k = 0; k < 48; ++k) {
    put(passive_table(i) + k * 0x38, 8, i.passive_masks[k]);
    put(i.record + 0xef + k, 1, std::uint8_t(i.passive_levels[k]));
  }
  for (unsigned k = 0; k < 17; ++k)
    put(i.record + 0x43f0 + k * 4, 4, i.equipment[k]);
  put(i.record + 0x4b08, 2, 0xffff);
  put(i.record + 0x4b0a, 2, 0);
  return out;
}
void bind_services(player::Tick &t, const Initial &i) {
  validate(i);
  const auto token = i.blessed_type_token;
  const auto actor = t.P;
  t.callbacks[player::Service::BlessedType] =
      [token](auto &, const auto &args) -> player::Result {
    if (!args.empty())
      throw player::Unsupported("NoActiveSpells type query arity");
    return token;
  };
  t.callbacks[player::Service::FindSpell] =
      [token, actor](auto &, const auto &args) -> player::Result {
    if (args.size() != 2 || !std::holds_alternative<std::uint32_t>(args[0]) ||
        !std::holds_alternative<std::uint32_t>(args[1]) ||
        std::get<std::uint32_t>(args[0]) != actor ||
        std::get<std::uint32_t>(args[1]) != token)
      throw player::Unsupported(
          "NoActiveSpells only accepts configured actor/type query");
    return std::uint32_t(0);
  };
}
} // namespace champions::simulation::rta::session_recovery
