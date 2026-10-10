#pragma once
#include "champions/amx.hpp"
#include "champions/combat_items.hpp"
#include "champions/combat_stats.hpp"
#include "champions/netwire.hpp"
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
struct Snapshot {
  Config config;
  bool started = false, failed = false, submitting = false;
  unsigned executing = 0;
  std::string failure_operation;
  std::vector<std::string> tasks;
  std::vector<std::pair<std::int64_t, std::string>> message_handlers;
  // All value state is copied, including rings/pads/logs/outbox. Executable
  // callbacks are cleared; only the session's run_tasks hook is installed live.
  netwire::Machine network;
  bool network_task_callback = false;
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
