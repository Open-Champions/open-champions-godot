#pragma once
#include "champions/amx.hpp"
#include "champions/objects.hpp"
#include <memory>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <variant>

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
  // External services remain explicit; integer arguments/results are raw low32.
  std::function<std::uint32_t(const std::string &, const std::vector<std::uint32_t> &)> player_service;
  std::function<std::uint32_t(const std::string &, const std::string &)> find_resource;
  using ServiceValue = std::variant<std::uint32_t, std::string, std::array<std::uint32_t, 3>>;
  // Strings are arbitrary owned bytes (including dialog packets), not host pointers.
  std::function<std::uint32_t(const std::string &, const std::vector<ServiceValue> &)> scene_service;
  // Address registry projection used by level transitions and prop animation. Missing address
  // and absent property list are distinct reference faults; zero means no object.
  std::map<std::uint32_t, std::optional<std::vector<std::string>>> object_properties;
  std::optional<amx::Cell> dialog_string_slot, cutscene_string_slot;
  // Reuse the tested object name table. Owners keep named objects alive; only
  // Address identity and type_name are used; fields are read live from Memory.
  objects::NameTable object_names;
  std::map<std::uint32_t, std::shared_ptr<objects::GameObject>> named_objects;
  std::map<unsigned, amx::Cell> menu_string_slots;
  // Grid enumeration belongs to the host. Returned objects must stay alive during
  // dispatch; their fields are read live from Memory, in the supplied order.
  std::function<std::vector<objects::GameObject *>(const std::string &,
      const std::vector<ServiceValue> &)> object_query;
  // The pinned model represents only the initialized portions of the temporary
  // item record. Other item contents belong to the host, as in the reference.
  struct ItemBlank {
    std::array<std::uint8_t, 18> bytes{};
    std::array<std::uint16_t, 4> words{{65535,65535,65535,65535}};
    ItemBlank() { bytes[16]=bytes[17]=255; }
  };
  using InventoryValue = std::variant<std::uint32_t, std::string,
      std::array<std::uint32_t,3>, ItemBlank>;
  using InventoryResult = std::variant<std::uint32_t, std::array<std::uint32_t,2>>;
  // findItemBySubstring returns {index, bag}; every other service returns a word.
  std::function<InventoryResult(const std::string &, const std::vector<InventoryValue> &)> inventory_service;
  std::uint64_t draws = 0;
  std::uint32_t random_draw();
  objects::GameObject *player_object(amx::Cell) const;
  void empty_bag(amx::Cell, unsigned);
  void drop_at(std::function<objects::GameObject *()>);
  bool give(amx::Cell, const std::string &, amx::Cell, amx::Cell, bool, bool);
  void give_or_drop(amx::Cell);
  objects::GameObject *obj_at(std::uint32_t) const;
  void apply_monster_val(objects::GameObject *, amx::Cell, amx::Cell);
  void apply_cflag(objects::GameObject *, amx::Cell, bool);
  void apply_oflag(objects::GameObject *, amx::Cell, bool);
  void follow_path(objects::GameObject *);
  void stop_path(objects::GameObject *);
  amx::Cell alive(amx::Cell) const;
  std::vector<objects::GameObject *> find(const std::string &, std::int64_t) const;
  objects::GameObject *find_first(const std::string &) const;
  objects::GameObject *need(objects::GameObject *, const std::string &) const;
  amx::Cell medal_gate();
  std::uint32_t medal_prompt();
  amx::Cell medal_pad();
  amx::Cell net_gate();
  void set_next_world(const std::string &);
  void dialog_start(const std::string &, amx::Cell, amx::Cell);
  amx::Cell new_world_no_gate(amx::Machine &);
  void player_set_flag(amx::Cell, amx::Cell, amx::Cell);
  amx::Cell player_get_flag(amx::Cell, amx::Cell) const;
  amx::Cell count_players_with_hp() const;
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
  // Supported bindings only. The Variables instance must outlive bindings.
  amx::NativeMap natives();
  // A fresh/nested run has a different handler stack. Unported calls must not
  // accidentally preserve a known slot; their frame is outside this subset.
  void forget_string_slot() noexcept { string_slot.reset(); quest_string_slot.reset(); dialog_string_slot.reset(); cutscene_string_slot.reset(); menu_string_slots.clear(); }
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
  amx::Cell effects_dispatch(const std::string &, amx::Machine &);
  static std::uint32_t particle_definition(amx::Cell);
  InventoryResult inventory_call(const std::string &, const std::vector<InventoryValue> & = {});
  std::uint32_t inventory_word(const std::string &, const std::vector<InventoryValue> & = {});
  amx::Cell inventory_dispatch(const std::string &, amx::Machine &);
  amx::Cell creature_dispatch(const std::string &, amx::Machine &);
  std::vector<objects::GameObject *> creatures(const std::string &, std::int64_t = 0x200) const;
  void scale(objects::GameObject *, amx::Machine &, unsigned);
  void start_anim(std::uint32_t, const std::function<std::uint32_t()> &, amx::Cell);
  amx::Cell menu_dispatch(const std::string &, amx::Machine &);
  const std::vector<std::string> *selected_properties() const;
  std::uint32_t scene_call(const std::string &, const std::vector<ServiceValue> & = {});
  amx::Cell scene_dispatch(const std::string &, amx::Machine &);
  void refill(bool);
  std::uint32_t service(const std::string &, const std::vector<std::uint32_t> &);
  amx::Cell player_dispatch(const std::string &, amx::Machine &);
  std::string string_argument(amx::Machine &, amx::Address, std::optional<amx::Cell> &);
  std::pair<std::uint32_t, bool> slot(std::uint32_t, const std::string &) const;
  amx::Cell get(std::uint32_t, const std::string &);
};
} // namespace champions::simulation::rta::builtins
