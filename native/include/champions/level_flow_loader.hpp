#pragma once
#include "champions/level_flow.hpp"

namespace champions::simulation::rta::level_flow {
struct Lump {
  Blob bytes;
  std::vector<Entry> entries;
  int keep = 0;
};
// The application supplies an explicitly selected disc directory. No filesystem
// search, extracted assets, or platform rendering are part of this library.
struct LoadServices {
  // nullopt means absent. Requests use uppercase ISO names, including ;1.
  std::function<std::optional<Blob>(const std::string &)> disc;
  std::function<Blob(const std::string &, std::size_t offset, std::size_t size)>
      range;
  // Resolve the 12 race/select lumps from the owner's player model tables.
  std::function<bool()> player_lumps; // true if any player lump reloaded
  // WORLD metadata projection owned by terrain/level-textures integration.
  std::function<std::vector<std::string>(Bytes)> model_lumps;
  // Notification of documented service calls, in order. Throw on unsupported
  // work. Real integration must implement all events; tests record exact event
  // order.
  std::function<void(const std::string &)> event;
};
struct LoadedLevel {
  LevelRow row;
  Raw monster_level = 0;
  std::uint32_t base_light_level = 0x80; // g_worldLightLevel from row flags
  std::string world_entry;
  Blob world, texture_header, script, objects;
  std::vector<Resource> manifest;
};
class Loader {
  LoadServices services_;
  std::map<std::string, Lump> lumps_;
  std::vector<Resource> opened_;
  Lump parse_lump(Blob, int keep);
  const Lump &need_lump(const std::string &, int keep, const std::string &step);

public:
  explicit Loader(LoadServices);
  void resident(const std::string &, Blob, int keep);
  const std::map<std::string, Lump> &lumps() const { return lumps_; }
  // Required service callbacks run synchronously, including script/object
  // creation in SessionAdapter. Failure leaves partial resources, never reports
  // a level ready.
  LoadedLevel load(const std::string &base, const std::string &sub,
                   const std::vector<LevelRow> &, int difficulty,
                   bool recall = false, bool pvp = false);
  std::optional<Blob> entry(const std::string &lump,
                            const std::string &name) const;
};
} // namespace champions::simulation::rta::level_flow
