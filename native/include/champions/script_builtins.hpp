#pragma once
#include "champions/amx.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::builtins {
inline constexpr std::uint32_t QUEST_VARS = 0x86da80, PROFILE_VARS = 0x4f9030;
class StackGarbage : public amx::HostFault {
public:
  using amx::HostFault::HostFault;
};
// Zero-backed, owned byte storage at reference addresses. These are identities,
// never pointers into the host process. Writes retain their original order/size.
struct Memory {
  std::map<std::uint32_t, std::uint8_t> bytes;
  std::vector<std::pair<std::uint32_t, std::size_t>> writes;
  std::uint8_t read(std::uint32_t) const;
  std::string cstr(std::uint32_t) const;
  void write(std::uint32_t, const std::vector<std::uint8_t> &);
  void byte(std::uint32_t, std::uint8_t);
  std::uint32_t word(std::uint32_t) const;
  void word(std::uint32_t, std::uint32_t);
  void string(std::uint32_t, const std::string &);
};
class Variables {
public:
  Memory memory;
  // Required when profile_set actually requests a save. No persistence is
  // fabricated; absence refuses after the documented table write prefix.
  std::function<void()> request_config_save;
  std::optional<amx::Cell> string_slot;
  // Explicit boundary for script_runByName: host owns nested execution and slots.
  std::function<void(const std::string &)> run_reward_script;
  std::function<amx::Cell()> local_player_slot;
  std::optional<amx::Cell> quest_string_slot; // handler stack slot 0x30
  Variables() = default;
  Variables(const Variables &) = delete;
  Variables &operator=(const Variables &) = delete;
  Variables(Variables &&) = delete;
  Variables &operator=(Variables &&) = delete;
  amx::Cell quest_get(const std::string &);
  void quest_set(const std::string &, amx::Cell);
  void quest_clear_all();
  std::optional<std::pair<std::string, amx::Cell>> quest_by_index(amx::Address) const;
  amx::Cell profile_get(const std::string &);
  void profile_set(const std::string &, amx::Cell, bool save);
  amx::Cell dispatch(const std::string &, amx::Machine &);
  // Nineteen supported bindings only. The Variables instance must outlive bindings.
  amx::NativeMap natives();
  // A fresh/nested run has a different handler stack. Unported calls must not
  // accidentally preserve a known slot; their frame is outside this subset.
  void forget_string_slot() noexcept { string_slot.reset(); quest_string_slot.reset(); }
  std::uint32_t player_record(amx::Cell) const;
  std::uint32_t mission_byte(amx::Cell, amx::Cell, amx::Cell) const;
  amx::Cell mission_available(amx::Cell, amx::Cell) const;
  amx::Cell mission_complete(amx::Cell, amx::Cell) const;
  void mission_set_available(amx::Cell, amx::Cell, amx::Cell);
  void mission_set_complete(amx::Cell, amx::Cell, amx::Cell);
  void mission_grant_medal(amx::Cell, amx::Cell);
  amx::Cell mission_get_medal(amx::Cell, amx::Cell, amx::Cell);
  void quest_add(const std::string &, const std::string &);
  void quest_remove(const std::string &);
  amx::Cell choice_get(std::uint32_t) const;
  void choice_set(std::uint32_t, amx::Cell);
private:
  std::string string_argument(amx::Machine &, amx::Address, std::optional<amx::Cell> &);
  std::pair<std::uint32_t, bool> slot(std::uint32_t, const std::string &) const;
  amx::Cell get(std::uint32_t, const std::string &);
};
} // namespace champions::simulation::rta::builtins
