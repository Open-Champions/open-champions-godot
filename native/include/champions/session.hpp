#pragma once
#include "champions/amx.hpp"
#include "champions/combat_creature.hpp"
#include "champions/combat_items.hpp"
#include "champions/combat_stats.hpp"
#include "champions/netwire.hpp"
#include "champions/objects.hpp"
#include "champions/raw_handles.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace champions::simulation::rta::session {
// Experimental composition of the pinned module contracts. Task/native names
// are supplied by the caller, never inferred original engine commands.
class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class Failed : public Error {
public:
  using Error::Error;
};
class ResourceLimit : public Error {
public:
  using Error::Error;
};
inline constexpr std::size_t MAX_SCRIPTS = 8, MAX_TASKS = 16,
                             MAX_DEFINITIONS = 256, MAX_ITEMS = 256,
                             MAX_NATIVE_BINDINGS = 256, MAX_MESSAGE_BYTES = 255,
                             MAX_EVENT_VALUES = 259,
                             MAX_SCRIPT_BYTES = 16u * 1024u * 1024u,
                             MAX_HISTORY = 65536, MAX_NAME = 128;
inline constexpr amx::Address SCRIPT_BUDGET = 100000;
inline constexpr std::int64_t MAX_SUBMIT_STEPS = 32;
// Reserve for bounded net_main bookkeeping after the last task/message event:
// at most256 inbox packets, four peers and32 ticks per submission.
inline constexpr std::size_t HISTORY_RESERVE = 2048,
                             MAX_EVENT_HISTORY = MAX_HISTORY - HISTORY_RESERVE;
struct Config {
  std::string profile = "rta";
  Fpu::Model model = Fpu::Model::Ps2;
  bool mul_bias = false, div_nearest = false;
  std::int64_t peers = 1, local = 0, network_mode = 0;
  std::uint32_t seed = 0, network_seed = 0;
};
// Supplied callbacks are registered before submission. Captures are outside
// snapshots; keep mutable state in World or explicitly owned referenced state.
template <class Function> struct NamedObjectCallback {
  std::string name;
  Function function;
};
struct ObjectBindings {
  objects::CallbackPolicy policy = objects::CallbackPolicy::RequireHandlers;
  NamedObjectCallback<std::function<void(objects::GameObject &)>> on_run;
  NamedObjectCallback<std::function<void(objects::GameObject &, std::int64_t)>>
      on_destroy;
  NamedObjectCallback<std::function<bool()>> is_frozen;
  NamedObjectCallback<
      std::function<std::uint32_t(const std::string &, const std::string &)>>
      res_find;
  NamedObjectCallback<std::function<void()>> end_of_tick;
};
struct ObjectBindingSnapshot {
  std::string kind, name;
  bool present = false;
};
inline constexpr std::uint32_t OBJECT_RUN_TASK = 0x14b910,
                               OBJECT_DRAW_TASK = 0x14b168;
struct ItemRecord {
  std::int32_t count = 0;
  std::uint64_t flags = 0;
  std::array<std::int16_t, 4> a{}, b{};
  std::uint8_t prefix = 0xff, suffix = 0xff;
  std::array<std::optional<items::Modifier>, 4> mods{};
};
struct ItemEvaluation {
  Raw weight = 0;
  std::int32_t armor = 0, attack = 0;
  std::pair<std::int32_t, std::int32_t> damage{};
  std::int64_t combat_armor = 0;
  std::int32_t combat_attack = 0;
  std::pair<std::int32_t, std::int32_t> combat_damage{};
};
struct Event {
  std::string kind, name;
  std::int64_t frame = 0;
  std::vector<std::int64_t> values;
};
struct ItemSnapshot {
  std::string definition;
  ItemRecord record;
  combat::ItemDef combat_definition;
  std::int64_t combat_count = 0;
  std::uint64_t combat_flags = 0;
  combat::Descriptor combat_mods;
};
struct ScriptSnapshot {
  amx::Snapshot stored;
  amx::Address params = 0;
  bool parsed = false, header_read = false, callback_present = false;
  amx::Address size = 0, cod = 0, dat = 0, hea0 = 0, start = 0, num_publics = 0,
               publics_off = 0, num_natives = 0, natives_off = 0,
               num_libraries = 0, num_pubvars = 0, data_offset = 0;
  std::optional<amx::Address> budget;
  std::optional<amx::Decoded> decoded;
  std::optional<amx::Profile> profile;
  std::optional<std::vector<amx::BindingKind>> bindings;
  std::optional<std::vector<bool>> binding_present;
  std::vector<bool> pending_present;
  // Declarations captured at load, not reinterpreted from mutable VM bytes.
  std::vector<std::string> pending_names, declared_native_names,
      registered_names;
};
using HpId = std::uint64_t;
struct HpInitial {
  Raw hp = 0;
  std::uint64_t cflags = 0;
  std::array<Raw, 5> status{};
};
struct HpLimits {
  std::size_t records = 64, calls_per_tick = 64;
  creature_hp::Limits memory{};
};
struct HpBinding {
  HpId id = 0;
  std::uint64_t world_serial = 0;
  std::uint32_t address = 0;
  std::uint64_t adapter_generation = 0;
};
struct HpSnapshot {
  HpLimits limits;
  std::vector<HpBinding> bindings;
  std::string phase = "idle";
  std::size_t depth = 0, calls_this_tick = 0;
  combat_creature::Snapshot adapter;
  raw_handles::RegistrySnapshot registry;
  std::optional<raw_handles::Snapshot> physical;
  std::string physical_error, publication_error;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
struct Snapshot {
  Config config;
  std::optional<HpSnapshot> hp;
  bool started = false, failed = false, submitting = false;
  unsigned executing = 0;
  std::string failure_operation;
  std::vector<std::string> tasks;
  std::vector<std::pair<std::int64_t, std::string>> message_handlers;
  // All value state is copied, including rings/pads/logs/outbox. Executable
  // callbacks are cleared. Live hooks are run_tasks and optional before_frame.
  netwire::Machine network;
  bool network_task_callback = false;
  bool network_before_frame_callback = false;
  std::optional<objects::WorldSnapshot> world;
  std::vector<std::pair<std::uint32_t, std::string>> object_task_handlers;
  std::vector<ObjectBindingSnapshot> object_bindings;
  std::string world_phase = "disabled";
  unsigned world_depth = 0;
  std::map<std::string, items::Definition> definitions;
  std::map<std::string, ItemSnapshot> items;
  std::map<std::string, ScriptSnapshot> scripts;
  std::uint32_t seed = 0;
  std::vector<std::uint32_t> draws;
  std::vector<Event> events;
};
class Session {
public:
  using Task = std::function<void(Session &)>;
  using MessageHandler =
      std::function<void(Session &, const netwire::ByteVector &)>;
  explicit Session(Config config = {});
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) = delete;
  Session &operator=(Session &&) = delete;
  void add_definition(const std::string &, const items::Definition &);
  void add_item(const std::string &, const std::string &definition,
                const ItemRecord & = {});
  void add_script(const std::string &, const amx::ByteVector &,
                  const amx::NativeMap & = {});
  void register_task(const std::string &, Task);
  // Explicit seed initializes the World/network shared stream only. The combat
  // Seed remains separate. Requires RTA, network_mode=1 and no legacy tasks.
  void enable_objects(std::uint32_t shared_seed, ObjectBindings bindings = {});
  void register_object_task(std::uint32_t function, const std::string &name,
                            std::int64_t priority, std::int64_t flags, Task);
  // Pads in this adapter are unsigned 32-bit wire words; wider/negative writes
  // refuse after retaining partial World state. Frame writes publish as
  // supplied. Reference valid only during this call. Never retain it or replace
  // callback bindings/policy/network mode; scheduler table mutations may use
  // registered function IDs. Nested access retains in-progress World pads and
  // seed.
  void with_world(const std::function<void(objects::World &)> &);
  // Base slot2 only, fixed shared-pool callbacks. Setup freezes on submit.
  void enable_hp(HpLimits limits = {});
  HpId bind_hp(std::uint64_t world_serial, std::uint32_t address,
               HpInitial initial = {});
  // Write.address is a relative record offset; projected/managed spans refuse.
  void seed_hp_backing(HpId, const std::vector<creature_hp::Write> &);
  // offset must be 0x4c,0x2e4,0x360. Target backhead must already be
  // registered.
  void link_hp_head(HpId, std::uint32_t offset, std::uint32_t raw_target);
  // Only a post-runObjects task (priority>10) can invoke this operation.
  void take_damage(HpId victim, Raw amount, std::optional<HpId> attacker = {},
                   std::uint32_t flags = 0);
  void register_message(std::int64_t kind, const std::string &, MessageHandler);
  // No receive bypass: inbox/ready-frame ordering stays in modeled net_main.
  netwire::NetMainResult
  submit(const netwire::ByteVector &sample, std::int64_t yaw = 0,
         std::int64_t camera_flag = 0, std::int64_t frame_step = 1,
         bool no_sim = false, bool hold_arg = false,
         std::int64_t render_frame = 0,
         const std::vector<netwire::ByteVector> &inbox = {});
  void queue_message(std::int64_t kind, const netwire::ByteVector &);
  void set_level_pending(bool);
  // Caller-supplied synchronization observation; never an invented movement
  // rule.
  void set_sync_state(std::int64_t sim_frame, std::int64_t accumulator,
                      const std::array<std::optional<netwire::Position>, 4> &,
                      std::optional<std::uint32_t> network_seed = {});
  amx::ExecuteResult execute(const std::string &, amx::Address index,
                             const std::vector<amx::Cell> &params = {},
                             amx::Address pri = 0);
  ItemEvaluation evaluate_item(const std::string &, bool elements = true);
  std::int32_t set_modifier(const std::string &, std::int32_t code,
                            std::int32_t v0 = 0, std::int32_t v1 = 0,
                            std::int32_t v2 = 0);
  Raw roll_damage(Raw minimum, Raw maximum);
  std::uint32_t draw();
  // Callback-free owned observation; cannot invoke live hooks through a const
  // view.
  netwire::Machine network() const;
  bool failed() const noexcept;
  Snapshot snapshot() const;

private:
  struct State;
  std::unique_ptr<State> state_;
  void ready_tick();
};
} // namespace champions::simulation::rta::session
