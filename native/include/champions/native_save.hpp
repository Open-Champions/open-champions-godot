#pragma once
#include "champions/session_control.hpp"

namespace champions::simulation::rta::native_save {
// Authored native replay checkpoint, never a console save format.
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
using Bytes = std::vector<std::uint8_t>;
inline constexpr std::uint32_t VERSION = 1, RTA_PROFILE = 1;
inline constexpr std::size_t MAX_BYTES = 32u * 1024u * 1024u;
inline constexpr std::size_t MAX_ACTIONS = 10000;
// Closed mutation surface: all authoritative state is reproducible from the
// initial config/terrain and this bounded operation stream. No mutable Session
// or Controller reference escapes. Reload builds a fresh owned Session.
class Scene {
public:
  explicit Scene(session_control::Config config = {},
                 std::optional<world_queries::LevelData> terrain = {},
                 world_queries::Limits limits = {});
  session_control::Observation advance(std::uint32_t command);
  void set_paused(bool);
  void release_input();
  void stop();
  void reset();
  session_control::Observation observation() const;
  session_control::Snapshot snapshot() const;
  Bytes save() const;
  // Separate direct state checkpoint; replay v1 remains byte-for-byte stable.
  Bytes save_snapshot() const;
  // Atomic publication by ownership: failure never modifies an existing scene.
  // Profile is required from the caller, independently of the file header.
  static std::unique_ptr<Scene> load(const Bytes &, const std::string &profile);

private:
  static std::unique_ptr<Scene> load_replay(const Bytes &, const std::string &,
                                            bool);
  struct Action {
    std::uint8_t kind, value;
  };
  std::unique_ptr<session_control::Controller> controller_;
  session_control::Config config_;
  std::optional<world_queries::LevelData> terrain_;
  world_queries::Limits limits_;
  std::vector<Action> actions_;
  void reserve_action();
};
} // namespace champions::simulation::rta::native_save
