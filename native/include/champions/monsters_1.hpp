#pragma once
#include "champions/creature_hp.hpp"
#include <array>
#include <set>
#include <variant>

namespace champions::simulation::rta::monsters_1 {
using Memory = creature_hp::Memory;
using Address = creature_hp::Address;
using Write = creature_hp::Write;
using Error = creature_hp::Error;
using CallbackPolicy = creature_hp::CallbackPolicy;
using Vec3 = std::array<Raw, 3>;
using Argument = std::variant<std::uint32_t, Vec3>;
struct Unsupported : Error { using Error::Error; };
struct Call { std::string name; std::vector<Argument> args; };
struct Event { bool is_call = false; Write write{}; Call call{}; };
// Bounded standalone subset, explicitly Return to Arms. Storage is borrowed
// and must outlive this nonmovable model. Missing handlers refuse by default.
class Model {
  std::size_t depth_ = 0;
  void record_call(const Call &);
  void creature_ctor(Address, Raw, Vec3, Raw, Raw, Raw, Raw);
  void event_params(Address, Raw, std::array<Raw,4>, std::array<Raw,4>);
  void write(Address, std::size_t, std::uint64_t);
  std::uint64_t call(const std::string &, std::vector<Argument>);
  Raw u32(Address) const;
  Vec3 vec(Address) const;
  void wvec(Address, Vec3);
  std::uint32_t random(Address);
  std::pair<bool, std::int32_t> filter(Address, std::uint32_t);
  void clear_path(Address);
  void wander(Address);
  void start(Address, std::uint32_t, std::uint32_t, Raw blend = 0x3dcccccc);
  void swing(Address, std::uint32_t, std::uint32_t);
  void drop_loot(Address, Raw item, Raw, Raw, Raw);
  void virtual_call(Address, unsigned, std::uint32_t);
  void on_death(Address, std::uint32_t);
  void shed(Address);
  void spider_state_body(Address, std::uint32_t);
public:
  using Callback = std::function<std::uint64_t(Model &, const std::vector<Argument> &)>;
  using Balance = std::array<Raw,6>; // found, HP, armour, damage, attack, XP
  std::function<Balance(Model &, Raw)> balance_lookup;
  Memory &mem;
  CallbackPolicy policy;
  std::map<std::string, Callback> callbacks;
  // Only these two slots may force a modelled virtual body into a callback.
  std::set<std::string> external;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0; // Shared RNG: this subset never draws from it.
  explicit Model(Memory &, CallbackPolicy = CallbackPolicy::RequireHandlers);
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // Callback mutation, replacement and exceptions retain partial state; no
  // rollback. Integer callback arguments are raw32; returns retain raw64.
  Raw anim_flags(Address c, Raw mask) const;
  void smallspider_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  void gnaw_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  // Reference order differs from the other constructors.
  void mosquito_ctor(Address, Vec3, Raw facing, Raw props, Raw type_name);
  void scorpion_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  void badger_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props, Raw variant);
  void smallspider_set_state(Address, std::uint32_t state);
  void gnaw_set_state(Address, std::uint32_t state);
  void mosquito_set_state(Address, std::uint32_t state);
  void scorpion_set_state(Address, std::uint32_t state);
  void badger_set_state(Address, std::uint32_t state);
  void smallspider_on_touch(Address, std::uint32_t other);
  void mosquito_lose_part(Address, std::uint32_t part);
  void scorpion_sever_part(Address, std::uint32_t part);
  // Uses caller-supplied signed-character classification bytes at 0x5628e1.
  // A prefix longer than 4096 bytes refuses instead of looping indefinitely.
  std::uint32_t str_prefix_differs(Address text, Address prefix);
};
} // namespace champions::simulation::rta::monsters_1
