#include "champions/pickups.hpp"
#include <algorithm>
namespace champions::simulation::rta::pickups {
void require_supported_chest(const std::vector<std::string> &props, bool trapped) {
  if (trapped)
    throw combat_runtime::Unsupported("trapped Chest opening refused: timed explosion service is not evidenced");
  for (const auto &property : props)
    if (property.compare(0, 7, "action=") == 0)
      throw combat_runtime::Unsupported("Chest opening/completion refused: action script service is not composed");
}
std::vector<Grant> starting_grants(unsigned cls) {
  if (cls > 6) throw script_items::Refused("starting class outside 0..6");
  std::vector<Grant> grants;
  const std::uint32_t armor = cls == 4 ? 0x5373a0 : 0x537310;
  for (unsigned k = 0; k < 3; ++k)
    grants.push_back({armor + 0x30 * k, 0, std::uint8_t(cls == 4 ? 7 : 2), true, 1, 1});
  const std::array<std::uint32_t, 7> weapons{
    0x537430, 0x5374b8, 0x5374e0, 0x537458, 0x537510, 0x537540, 0x537570};
  const std::array<std::uint8_t, 7> variants{7, 7, 5, 4, 5, 9, 7};
  grants.push_back({weapons[cls], cls == 3 ? 1 : 0, variants[cls], true, 1, 1});
  if (cls == 3) {
    grants.push_back({0x537488, 0, std::uint8_t(9), true, 1, 1});
    // These are two parses and gives, unlike the parse-once consumables.
    grants.push_back({0x536ac8, 0, std::nullopt, false, 1, 1});
    grants.push_back({0x536ac8, 0, std::nullopt, false, 1, 1});
  }
  if (cls == 5) grants.push_back({0x537430, 0, std::uint8_t(7), true, 1, 1});
  for (auto p : {0x537598u, 0x5375a8u, 0x5375c0u})
    grants.push_back({p, -1, std::nullopt, false, 2, p == 0x537598 ? 0u : 1u});
  return grants;
}
void give_starting_items(script_items::Service &service, unsigned slot, unsigned cls,
                         const std::function<std::string(std::uint32_t)> &descriptor) {
  if (slot >= 4) throw script_items::Refused("starting slot outside 0..3");
  for (const auto &g : starting_grants(cls)) {
    service.call("item_parseDescriptor", {descriptor(g.descriptor), 0u, Raw(g.variant), 0u});
    auto record = service.record();
    if (g.byte20) record[0x20] = *g.byte20;
    service.set_record(record);
    for (unsigned copy = 0; copy < g.copies; ++copy) {
      const auto index = std::get<std::uint32_t>(service.call("inventory_giveItem", {slot, g.announce}));
      if (g.equip) {
        const auto category = g.descriptor >= 0x537430 ? 0 : 1;
        const auto entry = script_items::bag_entry(int(slot), category, std::int32_t(index));
        service.call("inventory_equipItem", {slot, entry, 1u});
      }
    }
  }
  service.call("player_recalcLoad", {slot});
}
std::vector<unsigned> take_item(script_items::Service &service, builtins::Memory &memory,
    const items::Tables &tables, const std::array<std::uint8_t, 0x48> &record, unsigned chosen) {
  const auto item = items::Item::unpack(tables, {record.begin(), record.end()});
  if (!item.definition) throw script_items::Refused("pickup item has no definition");
  const auto category = item.definition->category;
  if (category < 0 || category > 2) throw script_items::Refused("pickup category outside 0..2");
  const auto count = memory.word(script_items::G_NUM_PLAYERS);
  if (count > 4 || chosen >= count) throw script_items::Refused("pickup player slot outside active party");
  const bool shared = (item.flags & (UINT64_C(1) << 39)) != 0;
  std::vector<unsigned> accepted;
  for (unsigned slot = shared ? 0 : chosen; slot < (shared ? count : chosen + 1); ++slot) {
    const auto r = script_items::rec(int(slot));
    service.set_record(record);
    if (memory.s32(script_items::bag_count_at(int(slot), category)) >= 63) continue;
    const auto weight = std::get<Raw>(service.call("player_itemWeight", {slot}));
    if (lt(memory.word(r + 0x124), add(memory.word(r + 0x120), weight))) continue;
    if (const auto it = service.delegates.find("pickup_notify"); it != service.delegates.end())
      it->second({slot});
    service.call("inventory_giveItem", {slot, 1u});
    service.call("player_recalcLoad", {slot});
    accepted.push_back(slot);
  }
  return accepted;
}
std::array<Raw, 6> model_bounds(Bytes bytes, std::uint64_t mask) {
  if (bytes.size < 0x28) throw script_items::Refused("item model header is truncated");
  const auto meshes = std::int8_t(bytes.data[0x12]);
  const auto lods = std::int8_t(bytes.data[0x26]);
  if (meshes < 0 || lods < 0) throw script_items::Refused("negative item model mesh/LOD count");
  const auto start = std::size_t(0x28 + lods * (meshes + 1) * 4);
  if (start + std::size_t(meshes) * 10 > bytes.size)
    throw script_items::Refused("item model bounds are truncated");
  std::array<std::int32_t, 6> result{100000, 100000, 100000, -100000, -100000, -100000};
  for (int mesh = 0; mesh < meshes && mesh < 64; ++mesh) if (mask & (UINT64_C(1) << mesh))
    for (unsigned k = 0; k < 6; ++k) {
      const auto v = std::int8_t(bytes.data[start + 10 * mesh + k]);
      result[k] = k < 3 ? std::min(result[k], std::int32_t(v)) : std::max(result[k], std::int32_t(v));
    }
  std::array<Raw, 6> out{};
  for (unsigned k = 0; k < 6; ++k) out[k] = Raw(result[k]);
  return out;
}
} // namespace champions::simulation::rta::pickups
