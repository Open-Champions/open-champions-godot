#include "champions/session.hpp"
#include "champions/combat_damage.hpp"
#include <algorithm>
#include <exception>
#include <type_traits>
#include <utility>

namespace champions::simulation::rta::session {
namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw Error(message);
}
void name_ok(const std::string &name) {
  require(
      !name.empty() && name.size() <= MAX_NAME &&
          std::all_of(name.begin(), name.end(),
                      [](unsigned char c) { return c >= 0x21 && c <= 0x7e; }),
      "name must be bounded printable ASCII without spaces");
}
ItemRecord item_record(const items::Item &i) {
  return {i.count, i.flags, i.a, i.b, i.prefix, i.suffix, i.mods};
}
// Copy value state explicitly: copying std::function and then clearing it can
// itself invoke a caller-supplied functor's copy constructor.
netwire::Machine copy_network(const netwire::Machine &source) {
  netwire::Machine out(1, 0, source.fpu);
  out.network_mode = source.network_mode;
  out.num_peers = source.num_peers;
  out.peers = source.peers;
  out.lif = source.lif;
  out.last_sent = source.last_sent;
  out.net_frame = source.net_frame;
  out.cycle = source.cycle;
  out.phase = source.phase;
  out.modem = source.modem;
  out.last_hash_frame = source.last_hash_frame;
  out.stall = source.stall;
  out.ended = source.ended;
  out.eq = source.eq;
  out.min_ready = source.min_ready;
  out.win = source.win;
  out.long_stall = source.long_stall;
  out.kicked = source.kicked;
  out.kick_state = source.kick_state;
  out.kick_acks = source.kick_acks;
  out.cam_yaw = source.cam_yaw;
  out.msg_attached = source.msg_attached;
  out.input_lock = source.input_lock;
  out.sim_enc = source.sim_enc;
  out.sim_dec = source.sim_dec;
  out.send_cycles = source.send_cycles;
  out.sim_frame = source.sim_frame;
  out.rand_seed = source.rand_seed;
  out.sync_acc = source.sync_acc;
  out.level_pending = source.level_pending;
  out.num_players = source.num_players;
  out.positions = source.positions;
  out.pads = source.pads;
  out.pad_globals = source.pad_globals;
  out.outbox = source.outbox;
  out.log = source.log;
  return out;
}
// Typed, adapter-owned indirection lets scopes detect unsupported callback
// replacement without comparing opaque std::function captures.
template <class Result, class... Args> struct WorldCallback {
  const std::function<Result(Args...)> *source;
  Result operator()(Args... args) const {
    auto copy = *source;
    return copy(args...);
  }
};
template <class Result, class... Args>
void install_callback(std::function<Result(Args...)> &target,
                      const std::function<Result(Args...)> &source) {
  if (source)
    target = WorldCallback<Result, Args...>{&source};
}
template <class Result, class... Args>
bool same_callback(const std::function<Result(Args...)> &target,
                   const std::function<Result(Args...)> &source) {
  if (!source)
    return !target;
  const auto *wrapper =
      target.template target<WorldCallback<Result, Args...>>();
  return wrapper && wrapper->source == &source;
}
std::uint32_t pad_word(const netwire::Pad &pad, std::size_t offset) noexcept {
  return std::uint32_t(pad[offset]) | (std::uint32_t(pad[offset + 1]) << 8) |
         (std::uint32_t(pad[offset + 2]) << 16) |
         (std::uint32_t(pad[offset + 3]) << 24);
}
void write_pad_word(netwire::Pad &pad, std::size_t offset,
                    std::int64_t value) noexcept {
  const auto bits = std::uint32_t(value);
  for (unsigned i = 0; i < 4; ++i)
    pad[offset + i] = std::uint8_t(bits >> (8 * i));
}
void set_record(items::Item &i, const ItemRecord &r) {
  i.count = r.count;
  i.flags = r.flags;
  i.a = r.a;
  i.b = r.b;
  i.prefix = r.prefix;
  i.suffix = r.suffix;
  i.mods = r.mods;
}
} // namespace
struct Session::State {
  Config config;
  Fpu fpu;
  netwire::Machine net;
  combat::Seed seed;
  std::unique_ptr<objects::World> world;
  ObjectBindings object_bindings;
  std::map<std::uint32_t, std::pair<std::string, Task>> object_tasks;
  std::string world_phase = "disabled";
  unsigned world_depth = 0;
  bool started = false, failed = false, submitting = false;
  unsigned executing = 0;
  std::string failure_operation;
  std::vector<std::pair<std::string, Task>> tasks;
  std::vector<std::pair<std::int64_t, std::string>> messages;
  std::map<std::string, std::shared_ptr<items::Definition>> definitions;
  combat::ItemArena arena;
  struct OwnedItem {
    std::string definition;
    items::Item value;
    combat::Item combat_value;
  };
  struct Script {
    std::unique_ptr<amx::Machine> machine;
    std::vector<std::string> names, declared_names;
  };
  std::map<std::string, OwnedItem> items;
  std::map<std::string, Script> scripts;
  std::vector<Event> events;
  explicit State(Config c)
      : config(std::move(c)),
        fpu(config.model, config.mul_bias, config.div_nearest),
        net(config.peers, config.local,
            netwire::Fpu(config.model, config.mul_bias, config.div_nearest)),
        seed(config.seed) {
    net.network_mode = config.network_mode;
    net.num_players = config.peers;
    net.rand_seed = config.network_seed;
  }
  void import_pads() noexcept {
    if (!world)
      return;
    for (std::size_t i = 0; i < 4; ++i) {
      world->pads[i].held = pad_word(net.pads[i], 8);
      world->pads[i].pressed = pad_word(net.pads[i], 12);
      world->pads[i].edge2 = pad_word(net.pads[i], 16);
    }
  }
  void publish_world() noexcept {
    if (!world)
      return;
    net.rand_seed = world->shared.state;
    net.sim_frame = world->frame;
    for (std::size_t i = 0; i < 4; ++i) {
      write_pad_word(net.pads[i], 8, world->pads[i].held);
      write_pad_word(net.pads[i], 12, world->pads[i].pressed);
      write_pad_word(net.pads[i], 16, world->pads[i].edge2);
    }
  }
  void validate_world() const {
    if (!world)
      return;
    require(world->network_mode == 1 &&
                world->callback_policy == object_bindings.policy,
            "World network mode and callback policy are fixed by Session");
    require(
        same_callback(world->on_run, object_bindings.on_run.function) &&
            same_callback(world->on_destroy,
                          object_bindings.on_destroy.function) &&
            same_callback(world->is_frozen,
                          object_bindings.is_frozen.function) &&
            same_callback(world->res_find, object_bindings.res_find.function) &&
            same_callback(world->end_of_tick,
                          object_bindings.end_of_tick.function),
        "World callback replacement is outside the registered binding "
        "contract");
    for (const auto &pad : world->pads)
      require(pad.held >= 0 && std::uint64_t(pad.held) <= 0xffffffffu &&
                  pad.pressed >= 0 &&
                  std::uint64_t(pad.pressed) <= 0xffffffffu && pad.edge2 >= 0 &&
                  std::uint64_t(pad.edge2) <= 0xffffffffu,
              "Session World pad words must fit unsigned 32 bits");
    if (world->tasks.tasks.size() > objects::MAX_TASKS)
      throw ResourceLimit("World task table budget exceeded");
    for (std::size_t i = 0; i < world->tasks.tasks.size(); ++i) {
      const auto &task = world->tasks.tasks[i];
      require(task.fn == OBJECT_RUN_TASK || object_tasks.count(task.fn),
              "World task requires a registered numeric handler");
      name_ok(task.name);
      for (std::size_t j = 0; j < i; ++j)
        require(world->tasks.tasks[j].fn != task.fn,
                "duplicate World task function");
    }
  }
  template <class Fn> void world_scope(const char *phase, Fn &&function) {
    require(bool(world), "object runtime is not enabled");
    validate_world();
    if (world_depth >= 16)
      throw ResourceLimit("nested World scope budget exceeded");
    if (!world_depth)
      import_pads();
    auto previous = world_phase;
    world_phase = phase;
    ++world_depth;
    try {
      function();
      available();
      validate_world();
      publish_world();
      world_phase = std::move(previous);
      --world_depth;
    } catch (...) {
      // Explicit nonthrowing publication, not rollback. Keep the failing phase.
      publish_world();
      --world_depth;
      throw;
    }
  }
  void available() const {
    if (failed)
      throw Failed("session failure is latched; construct a new session");
  }
  void setup() const {
    require(!started, "session registration is frozen after first submission");
  }
  void latch(const std::string &operation) noexcept {
    if (!failed) {
      failed = true;
      // Allocation failure must not replace the original exception.
      try {
        failure_operation = operation;
      } catch (...) {
      }
    }
  }
  std::size_t history() const {
    return events.size() + seed.draws.size() + net.log.size() +
           net.outbox.size();
  }
  std::size_t script_bytes() const {
    std::size_t result = 0;
    for (const auto &entry : scripts) {
      const auto size = entry.second.machine->image.size();
      if (size > MAX_SCRIPT_BYTES - result)
        throw ResourceLimit("aggregate script image budget exceeded");
      result += size;
    }
    return result;
  }
  void budgets() const {
    (void)script_bytes();
    if (history() > MAX_HISTORY)
      throw ResourceLimit("aggregate session history budget exceeded");
    std::size_t decoded = 0, bindings = 0;
    for (const auto &entry : scripts) {
      const auto &m = *entry.second.machine;
      if (m.decoded) {
        if (m.decoded->size() > amx::MAX_DECODED - decoded)
          throw ResourceLimit("aggregate decoded instruction budget exceeded");
        decoded += m.decoded->size();
      }
      const auto n = m.bound ? m.bound->size() : 0;
      if (n > MAX_HISTORY - bindings ||
          m.pending_natives.size() > MAX_HISTORY - bindings - n)
        throw ResourceLimit("aggregate native binding budget exceeded");
      bindings += n + m.pending_natives.size();
    }
  }
  void event(std::string kind, std::string name,
             std::vector<std::int64_t> values = {}) {
    if (history() >= MAX_EVENT_HISTORY || values.size() > MAX_EVENT_VALUES)
      throw ResourceLimit("session event history or value budget exceeded");
    events.push_back(
        {std::move(kind), std::move(name), net.net_frame, std::move(values)});
  }
  template <class Fn> auto guard(const std::string &operation, Fn &&fn) {
    available();
    ScopedFpu scope(fpu);
    try {
      budgets();
      if constexpr (std::is_void_v<decltype(fn())>) {
        fn();
        available();
        budgets();
      } else {
        auto result = fn();
        available();
        budgets();
        return result;
      }
    } catch (...) {
      if (world) {
        if (!world_depth && world_phase == "idle")
          import_pads();
        publish_world();
      }
      latch(operation);
      throw;
    }
  }
  OwnedItem &item(const std::string &name) {
    const auto found = items.find(name);
    require(found != items.end(), "unknown item");
    return found->second;
  }
};
Session::Session(Config config) {
  require(config.profile == "rta",
          "only the explicit rta profile is supported");
  require(config.model == Fpu::Model::Ps2 ||
              config.model == Fpu::Model::Ps2g1 ||
              config.model == Fpu::Model::Chop ||
              config.model == Fpu::Model::Nearest,
          "unknown arithmetic configuration");
  require(config.peers >= 1 && config.peers <= 4 && config.local >= 0 &&
              config.local < config.peers,
          "session requires one to four peers and a valid local slot");
  require(config.network_mode == 0 || config.network_mode == 1,
          "session network mode must be zero or one");
  state_ = std::make_unique<State>(std::move(config));
  state_->net.run_tasks = [this](netwire::Machine &) { ready_tick(); };
}
Session::~Session() = default;
void Session::add_definition(const std::string &name,
                             const items::Definition &definition) {
  auto &s = *state_;
  s.guard("add_definition", [&] {
    s.setup();
    name_ok(name);
    require(!s.definitions.count(name), "duplicate definition name");
    if (s.definitions.size() >= MAX_DEFINITIONS ||
        definition.name.size() > MAX_NAME ||
        (definition.picture && definition.picture->size() > MAX_NAME))
      throw ResourceLimit("definition count or text budget exceeded");
    s.definitions.emplace(name,
                          std::make_shared<items::Definition>(definition));
  });
}
void Session::add_item(const std::string &name, const std::string &definition,
                       const ItemRecord &record) {
  auto &s = *state_;
  s.guard("add_item", [&] {
    s.setup();
    name_ok(name);
    require(!s.items.count(name), "duplicate item name");
    const auto found = s.definitions.find(definition);
    require(found != s.definitions.end(), "unknown item definition");
    if (s.items.size() >= MAX_ITEMS)
      throw ResourceLimit("item count budget exceeded");
    items::Item item(found->second);
    set_record(item, record);
    auto converted = s.arena.convert(item);
    s.items.emplace(name, State::OwnedItem{definition, std::move(item),
                                           std::move(converted)});
  });
}
void Session::add_script(const std::string &name, const amx::ByteVector &image,
                         const amx::NativeMap &natives) {
  auto &s = *state_;
  s.guard("add_script", [&] {
    s.setup();
    name_ok(name);
    require(!s.scripts.count(name), "duplicate script name");
    if (s.scripts.size() >= MAX_SCRIPTS ||
        image.size() > MAX_SCRIPT_BYTES - s.script_bytes() ||
        natives.size() > MAX_TASKS)
      throw ResourceLimit(
          "script count, image or native registration budget exceeded");
    // Bound declaration-driven allocations before the AMX loader binds names.
    if (image.size() >= amx::STRUCT_SIZE + amx::HEADER_SIZE) {
      const auto at = amx::STRUCT_SIZE + 0x28;
      const auto native_count =
          unsigned(image[at]) | (unsigned(image[at + 1]) << 8);
      if (native_count > MAX_NATIVE_BINDINGS)
        throw ResourceLimit("script native declaration budget exceeded");
    }
    amx::NativeMap wrapped;
    std::vector<std::string> names;
    for (const auto &entry : natives) {
      name_ok(entry.first);
      require(bool(entry.second), "empty native handler");
      names.push_back(entry.first);
      const auto label = name + ":" + entry.first;
      wrapped.emplace(entry.first, [this, label, function = entry.second](
                                       amx::Machine &machine,
                                       const std::vector<amx::Cell> &params) {
        auto &state = *state_;
        return state.guard(
            "native:" + label, [&]() -> std::optional<amx::Address> {
              if (params.size() > 256)
                throw ResourceLimit("native parameter budget exceeded");
              state.event("native", label, {params.begin(), params.end()});
              auto result = function(machine, params);
              state.available();
              state.event("native_result", label,
                          {result ? 1 : 0, result.value_or(0)});
              return result;
            });
      });
    }
    auto machine = std::make_unique<amx::Machine>(image, wrapped);
    require(machine->load() == amx::ERR_NONE && machine->decoded,
            "script could not be loaded and relocated");
    auto declared_names = machine->native_names();
    for (const auto &required_name : declared_names)
      require(natives.count(required_name) != 0,
              "script requires an unregistered native handler");
    const auto &bindings = machine->natives();
    for (const auto &binding : bindings)
      require(binding.kind == amx::BindingKind::Bound && bool(binding.function),
              "script contains an unbound or foreign native handler");
    std::size_t decoded_count = machine->decoded->size();
    for (const auto &entry : s.scripts) {
      const auto &existing = entry.second.machine->decoded;
      if (existing) {
        if (existing->size() > amx::MAX_DECODED - decoded_count)
          throw ResourceLimit("aggregate decoded instruction budget exceeded");
        decoded_count += existing->size();
      }
    }
    machine->profile = std::make_shared<amx::Profile>();
    machine->budget = SCRIPT_BUDGET;
    s.scripts.emplace(name, State::Script{std::move(machine), std::move(names),
                                          std::move(declared_names)});
  });
}
void Session::register_task(const std::string &name, Task task) {
  auto &s = *state_;
  s.guard("register_task", [&] {
    s.setup();
    require(!s.world, "legacy Session tasks cannot be mixed with object tasks");
    name_ok(name);
    require(bool(task), "empty task handler");
    require(std::none_of(s.tasks.begin(), s.tasks.end(),
                         [&](const auto &p) { return p.first == name; }),
            "duplicate task name");
    if (s.tasks.size() >= MAX_TASKS)
      throw ResourceLimit("task registration budget exceeded");
    s.tasks.emplace_back(name, std::move(task));
  });
}
void Session::enable_objects(std::uint32_t shared_seed,
                             ObjectBindings bindings) {
  auto &s = *state_;
  s.guard("enable_objects", [&] {
    s.setup();
    require(s.config.network_mode == 1,
            "object runtime requires network mode one");
    require(!s.world && s.tasks.empty(),
            "object runtime must be enabled once without legacy tasks");
    require(bindings.policy == objects::CallbackPolicy::RequireHandlers ||
                bindings.policy == objects::CallbackPolicy::ReferenceBase,
            "unknown World callback policy");
    const auto check = [](const auto &binding) {
      if (binding.function)
        name_ok(binding.name);
      else
        require(binding.name.empty(),
                "absent World callback must have empty label");
    };
    check(bindings.on_run);
    check(bindings.on_destroy);
    check(bindings.is_frozen);
    check(bindings.res_find);
    check(bindings.end_of_tick);
    s.object_bindings = std::move(bindings);
    s.world = std::make_unique<objects::World>(s.object_bindings.policy);
    s.world->network_mode = 1;
    s.world->frame = s.net.sim_frame;
    s.world->shared.state = shared_seed;
    install_callback(s.world->on_run, s.object_bindings.on_run.function);
    install_callback(s.world->on_destroy,
                     s.object_bindings.on_destroy.function);
    install_callback(s.world->is_frozen, s.object_bindings.is_frozen.function);
    install_callback(s.world->res_find, s.object_bindings.res_find.function);
    install_callback(s.world->end_of_tick,
                     s.object_bindings.end_of_tick.function);
    // Published object scheduler registration; no renderer/draw task installed.
    s.world->tasks.add(OBJECT_RUN_TASK, 10, "runObjects", 0);
    s.import_pads();
    s.publish_world();
    s.world_phase = "idle";
    s.net.before_frame = [this](netwire::Machine &machine) {
      auto &state = *state_;
      state.guard("pre_frame", [&] {
        state.world_phase = "pre_frame";
        state.validate_world();
        // Observed mainloop ordering, separate from the Python wire contract.
        state.world->frame = machine.net_frame;
        machine.sim_frame = machine.net_frame;
        machine.rand_seed = state.world->shared.state;
        state.world_phase = "idle";
      });
    };
  });
}
void Session::register_object_task(std::uint32_t function,
                                   const std::string &name,
                                   std::int64_t priority, std::int64_t flags,
                                   Task handler) {
  auto &s = *state_;
  s.guard("register_object_task", [&] {
    s.setup();
    require(bool(s.world), "object runtime is not enabled");
    require(function != OBJECT_RUN_TASK && function != OBJECT_DRAW_TASK,
            "built-in object task IDs are reserved");
    require(!s.object_tasks.count(function), "duplicate object task handler");
    require(bool(handler), "empty object task handler");
    name_ok(name);
    if (s.object_tasks.size() >= MAX_TASKS - 1)
      throw ResourceLimit("object task handler budget exceeded");
    s.object_tasks.emplace(function, std::make_pair(name, std::move(handler)));
    require(bool(s.world->tasks.add(function, priority, name, flags)),
            "object task already exists in scheduler");
    s.validate_world();
  });
}
void Session::with_world(
    const std::function<void(objects::World &)> &function) {
  auto &s = *state_;
  s.guard("with_world", [&] {
    require(bool(function), "empty World operation");
    s.world_scope("world", [&] {
      auto copy = function;
      copy(*s.world);
    });
  });
}
void Session::register_message(std::int64_t kind, const std::string &name,
                               MessageHandler handler) {
  auto &s = *state_;
  s.guard("register_message", [&] {
    s.setup();
    name_ok(name);
    require(bool(handler), "empty message handler");
    require(kind >= 0 && kind <= 255, "message kind must fit its wire byte");
    require(!s.net.handlers.count(kind), "duplicate message kind");
    if (s.messages.size() >= MAX_TASKS)
      throw ResourceLimit("message registration budget exceeded");
    s.net.handlers.emplace(kind, [this, name, handler = std::move(handler)](
                                     const netwire::ByteVector &bytes) {
      auto &state = *state_;
      state.guard("message:" + name, [&] {
        state.event("message", name, {bytes.begin(), bytes.end()});
        if (state.world)
          state.world_scope("message", [&] { handler(*this, bytes); });
        else
          handler(*this, bytes);
      });
    });
    s.messages.emplace_back(kind, name);
  });
}
void Session::ready_tick() {
  auto &s = *state_;
  s.guard("task", [&] {
    if (s.world) {
      s.world_scope("tasks", [&] {
        s.world->tasks_run(1, [&](std::uint32_t function) {
          s.validate_world();
          if (function == OBJECT_RUN_TASK) {
            s.event("task", "runObjects");
            s.world->run_objects();
          } else {
            const auto found = s.object_tasks.find(function);
            require(found != s.object_tasks.end(), "unregistered World task");
            auto handler = found->second.second;
            s.event("task", found->second.first);
            handler(*this);
          }
          s.available();
          s.validate_world();
        });
      });
      return;
    }
    require(!s.tasks.empty(), "no task handler registered");
    for (const auto &entry : s.tasks) {
      s.event("task", entry.first);
      entry.second(*this);
      s.available();
    }
  });
}
netwire::NetMainResult
Session::submit(const netwire::ByteVector &sample, std::int64_t yaw,
                std::int64_t camera_flag, std::int64_t frame_step, bool no_sim,
                bool hold_arg, std::int64_t render_frame,
                const std::vector<netwire::ByteVector> &inbox) {
  auto &s = *state_;
  return s.guard("submit", [&] {
    require(!s.submitting && !s.world_depth,
            "submission from a Session or World callback is unsupported");
    require(s.world || !s.tasks.empty(), "no task handler registered");
    s.validate_world();
    if (s.history() > MAX_EVENT_HISTORY)
      throw ResourceLimit("network bookkeeping history reserve exhausted");
    if (sample.size() > 12 || inbox.size() > 256 || frame_step < 0 ||
        frame_step > MAX_SUBMIT_STEPS)
      throw ResourceLimit(
          "submission sample, inbox or frame-step budget exceeded");
    std::size_t bytes = 0;
    for (const auto &packet : inbox) {
      if (packet.size() > netwire::MAX_BYTES - bytes)
        throw ResourceLimit("aggregate inbox byte budget exceeded");
      bytes += packet.size();
    }
    s.started = true;
    s.submitting = true;
    struct Reset {
      bool &value;
      ~Reset() { value = false; }
    } reset{s.submitting};
    return s.net.net_main(sample, yaw, camera_flag, frame_step, no_sim,
                          hold_arg, render_frame, inbox);
  });
}
void Session::queue_message(std::int64_t kind,
                            const netwire::ByteVector &bytes) {
  auto &s = *state_;
  s.guard("queue_message", [&] {
    require(s.net.handlers.count(kind) != 0,
            "message requires an explicit registered handler");
    if (bytes.size() > MAX_MESSAGE_BYTES)
      throw ResourceLimit("queued message payload must fit its wire byte");
    s.net.queue_message(kind, bytes);
  });
}
void Session::set_level_pending(bool value) {
  auto &s = *state_;
  s.guard("set_level_pending", [&] { s.net.level_pending = value; });
}
void Session::set_sync_state(
    std::int64_t frame, std::int64_t accumulator,
    const std::array<std::optional<netwire::Position>, 4> &positions,
    std::optional<std::uint32_t> network_seed) {
  auto &s = *state_;
  s.guard("set_sync_state", [&] {
    if (s.world)
      require(frame == s.world->frame && !network_seed,
              "object mode owns frame and shared seed; supply only sync "
              "observations");
    s.net.sim_frame = frame;
    s.net.sync_acc = accumulator;
    s.net.positions = positions;
    if (network_seed)
      s.net.rand_seed = *network_seed;
  });
}
amx::ExecuteResult Session::execute(const std::string &name, amx::Address index,
                                    const std::vector<amx::Cell> &params,
                                    amx::Address pri) {
  auto &s = *state_;
  return s.guard("execute", [&] {
    const auto found = s.scripts.find(name);
    require(found != s.scripts.end(), "unknown script");
    if (params.size() > 256 || s.executing >= 16)
      throw ResourceLimit("script parameter or nested call budget exceeded");
    std::vector<std::int64_t> values{index, pri, std::int64_t(params.size())};
    values.insert(values.end(), params.begin(), params.end());
    s.event("execute", name, std::move(values));
    ++s.executing;
    struct Reset {
      unsigned &value;
      ~Reset() { --value; }
    } reset{s.executing};
    auto &machine = *found->second.machine;
    machine.budget = SCRIPT_BUDGET;
    auto result = machine.execute(index, params, pri);
    s.event("result", name,
            {result.first, result.second ? 1 : 0, result.second.value_or(0)});
    require(result.first == amx::ERR_NONE,
            "script returned an execution error");
    return result;
  });
}
ItemEvaluation Session::evaluate_item(const std::string &name, bool elements) {
  auto &s = *state_;
  return s.guard("evaluate_item", [&] {
    auto &item = s.item(name);
    item.combat_value = s.arena.convert(item.value);
    const auto &i = item.value;
    const auto &c = item.combat_value;
    ItemEvaluation result{items::item_weight(i),
                          items::item_armor(i),
                          items::item_attack_rating(i),
                          items::item_damage_range(i, elements),
                          combat::item_armor_value(c),
                          combat::item_attack_rating(c),
                          combat::item_damage_range(c, elements)};
    s.event("item", name,
            {result.weight, result.armor, result.attack, result.damage.first,
             result.damage.second, result.combat_armor, result.combat_attack,
             result.combat_damage.first, result.combat_damage.second});
    return result;
  });
}
std::int32_t Session::set_modifier(const std::string &name, std::int32_t code,
                                   std::int32_t v0, std::int32_t v1,
                                   std::int32_t v2) {
  auto &s = *state_;
  return s.guard("set_modifier", [&] {
    auto &item = s.item(name);
    auto result = item.value.set_modifier(code, v0, v1, v2);
    item.combat_value = s.arena.convert(item.value);
    s.event("modifier", name, {code, v0, v1, v2, result});
    return result;
  });
}
std::uint32_t Session::draw() {
  auto &s = *state_;
  return s.guard("draw", [&] {
    if (s.history() + 2 > MAX_EVENT_HISTORY)
      throw ResourceLimit("draw history budget exceeded");
    auto result = s.seed.draw();
    s.event("draw", "", {result, s.seed.state});
    return result;
  });
}
Raw Session::roll_damage(Raw minimum, Raw maximum) {
  auto &s = *state_;
  return s.guard("roll_damage", [&] {
    auto random = draw();
    auto result = combat::base_roll(minimum, maximum, random);
    s.event("roll", "", {minimum, maximum, result});
    return result;
  });
}
netwire::Machine Session::network() const { return copy_network(state_->net); }

bool Session::failed() const noexcept { return state_->failed; }
Snapshot Session::snapshot() const {
  const auto &s = *state_;
  s.budgets();
  Snapshot out;
  out.config = s.config;
  out.started = s.started;
  out.failed = s.failed;
  out.submitting = s.submitting;
  out.executing = s.executing;
  out.failure_operation = s.failure_operation;
  for (const auto &task : s.tasks)
    out.tasks.push_back(task.first);
  out.message_handlers = s.messages;
  out.network = copy_network(s.net);
  out.network_task_callback = bool(s.net.run_tasks);
  out.network_before_frame_callback = bool(s.net.before_frame);
  out.world_phase = s.world_phase;
  out.world_depth = s.world_depth;
  if (s.world) {
    out.world = s.world->snapshot();
    out.object_task_handlers.emplace_back(OBJECT_RUN_TASK, "runObjects");
    for (const auto &entry : s.object_tasks)
      out.object_task_handlers.emplace_back(entry.first, entry.second.first);
    std::sort(out.object_task_handlers.begin(), out.object_task_handlers.end());
    const auto add_binding = [&](const char *kind, const auto &binding) {
      out.object_bindings.push_back(
          {kind, binding.name, bool(binding.function)});
    };
    add_binding("on_run", s.object_bindings.on_run);
    add_binding("on_destroy", s.object_bindings.on_destroy);
    add_binding("is_frozen", s.object_bindings.is_frozen);
    add_binding("res_find", s.object_bindings.res_find);
    add_binding("end_of_tick", s.object_bindings.end_of_tick);
  }
  for (const auto &entry : s.definitions)
    out.definitions.emplace(entry.first, *entry.second);
  for (const auto &entry : s.items) {
    const auto &i = entry.second;
    const auto &c = i.combat_value;
    out.items.emplace(entry.first,
                      ItemSnapshot{i.definition, item_record(i.value),
                                   c.defn ? *c.defn : combat::ItemDef{},
                                   c.count, c.flags, c.mods});
  }
  for (const auto &entry : s.scripts) {
    const auto &m = *entry.second.machine;
    ScriptSnapshot v;
    v.stored = m.snapshot();
    v.params = m.params;
    v.parsed = m.parsed;
    v.header_read = m.header_read;
    v.callback_present = bool(m.callback);
    v.size = m.size;
    v.cod = m.cod;
    v.dat = m.dat;
    v.hea0 = m.hea0;
    v.start = m.start;
    v.num_publics = m.num_publics;
    v.publics_off = m.publics_off;
    v.num_natives = m.num_natives;
    v.natives_off = m.natives_off;
    v.num_libraries = m.num_libraries;
    v.num_pubvars = m.num_pubvars;
    v.data_offset = m.data_offset;
    v.budget = m.budget;
    if (m.decoded)
      v.decoded = *m.decoded;
    if (m.profile)
      v.profile = *m.profile;
    if (m.bound) {
      v.bindings.emplace();
      v.binding_present.emplace();
      for (const auto &b : *m.bound) {
        v.bindings->push_back(b.kind);
        v.binding_present->push_back(bool(b.function));
      }
    }
    for (const auto &n : m.pending_natives) {
      v.pending_names.push_back(n.first);
      v.pending_present.push_back(bool(n.second));
    }
    v.declared_native_names = entry.second.declared_names;
    v.registered_names = entry.second.names;
    out.scripts.emplace(entry.first, std::move(v));
  }
  out.seed = s.seed.state;
  out.draws = s.seed.draws;
  out.events = s.events;
  return out;
}
} // namespace champions::simulation::rta::session
