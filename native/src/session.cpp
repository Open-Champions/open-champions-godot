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
  struct HpState {
    HpLimits limits;
    std::unique_ptr<combat_creature::Adapter> adapter;
    std::unique_ptr<raw_handles::RawHandles> handles;
    std::vector<HpBinding> bindings;
    std::vector<objects::GameObject *> objects;
    std::string phase = "idle", publication_error;
    std::size_t depth = 0, calls_this_tick = 0;
    std::optional<BaseCombatSnapshot> base_combat;
  };
  std::unique_ptr<HpState> hp;
  struct TerrainState {
    world_queries::Level level;
    std::optional<TerrainBinding> binding;
    std::string phase = "idle", publication_error;
    std::size_t depth = 0, calls_this_tick = 0;
    std::optional<world_queries::Vec3> last_velocity;
    std::optional<world_queries::MoveResult> last_result;
    TerrainState(const world_queries::LevelData &data,
                 world_queries::Limits limits)
        : level(data, limits) {}
  };
  std::unique_ptr<TerrainState> terrain;
  struct CreatureState {
    std::unique_ptr<creature_movement::Model> model;
    std::optional<HpBinding> binding;
    CreatureInitial initial;
    std::string phase = "idle", publication_error;
    std::size_t depth = 0, calls_this_tick = 0;
    std::optional<world_queries::Vec3> last_velocity;
    std::optional<std::uint32_t> last_bits;
    std::vector<CreatureCallbackResult> callback_results;
  };
  // Declared after HP: borrowed model dies before its Memory owner.
  std::unique_ptr<CreatureState> creature;
  struct AnimationState {
    struct Binding {
      HpBinding binding;
      animation_clock::AnimState state;
      std::size_t calls_this_tick = 0;
      bool advanced_this_tick = false;
      std::optional<std::int32_t> last_event;
      Binding(HpBinding value, std::int32_t capacity)
          : binding(value), state(capacity) {}
    };
    AnimationLimits limits;
    animation_clock::AnimSystem system;
    std::vector<animation_clock::ClipPtr> clips;
    std::vector<Binding> bindings;
    std::string phase = "idle";
    std::size_t depth = 0, calls_this_tick = 0, total_cells = 0;
    std::vector<AnimationOperation> operations;
  };
  std::unique_ptr<AnimationState> animation;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
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
  std::size_t hp_index(HpId id) const {
    require(bool(hp) && id && id <= hp->bindings.size(), "unknown HP binding");
    return std::size_t(id - 1);
  }
  void hp_preflight() const {
    require(bool(hp) && bool(world), "HP runtime is not enabled");
    for (std::size_t i = 0; i < hp->bindings.size(); ++i) {
      const auto &b = hp->bindings[i];
      const auto found = world->objects.find(b.address);
      require(found == world->objects.end() || found->second == hp->objects[i],
              "a bound HP address was reused by another World generation");
      require(hp->objects[i]->serial == b.world_serial &&
                  hp->objects[i]->addr == b.address,
              "HP binding identity changed");
    }
  }
  void base_combat_record(std::size_t i, bool configured) const {
    require(bool(hp->base_combat), "base combat is not enabled");
    const auto id = hp->bindings[i].id;
    if (configured)
      require(std::find(hp->base_combat->configured.begin(),
                        hp->base_combat->configured.end(),
                        id) != hp->base_combat->configured.end(),
              "base combat record requires explicit configuration");
    const auto &object = *hp->objects[i];
    require(object.alive, "base combat binding is retired");
    // These flags are read from the authoritative World before importing any
    // scalar. A setup-qualified creature may have changed since the last tick.
    require((object.flags & (0x20u | 0x40u | 0x80u | 0x10000000u)) == 0x20u,
            "base combat requires a nonplayer, nonprojectile creature");
    const auto &record = *hp->adapter->lookup(hp->bindings[i].address);
    const auto binding = hp->adapter->binding(record);
    require(binding.projection == combat_creature::Projection::CreatureFields &&
                binding.dispatch == combat_creature::Dispatch::BaseCreature &&
                record.cls == U"Creature" && !record.owner_link &&
                !record.owner && !record.status_source &&
                !record.last_attacker && record.spells.empty() &&
                !record.counter2 && !record.counter4 && !record.counter11 &&
                !record.counter12,
            "base combat does not support derived records, owners, spells or "
            "counters");
    require(record.armor >= INT16_MIN && record.armor <= INT16_MAX,
            "base combat armor must fit signed 16 bits");
  }
  void hp_import_scalars() {
    hp_preflight();
    for (std::size_t i = 0; i < hp->bindings.size(); ++i) {
      auto &to = *hp->adapter->lookup(hp->bindings[i].address);
      const auto &from = *hp->objects[i];
      to.flags = from.flags;
      to.pos.assign(from.pos.begin(), from.pos.end());
    }
    hp->adapter->world().frame = world->frame;
  }
  void hp_publish_scalars() {
    for (std::size_t i = 0; i < hp->bindings.size(); ++i) {
      const auto &from = *hp->adapter->lookup(hp->bindings[i].address);
      require(from.flags <= UINT32_MAX && from.pos.size() == 3,
              "invalid HP scalar result");
      auto &to = *hp->objects[i];
      to.flags = std::uint32_t(from.flags);
      std::copy(from.pos.begin(), from.pos.end(), to.pos.begin());
    }
    world->frame = hp->adapter->world().frame;
  }
  objects::GameObject &isolated_object(std::uint64_t serial,
                                       std::uint32_t address) const {
    require(bool(world), "terrain movement requires the object runtime");
    // The current map alone is insufficient: a caller may have removed a live
    // retained actor from it. Snapshot observes every retained generation.
    const auto state = world->snapshot();
    std::size_t alive = 0;
    for (const auto &object : state.objects) {
      if (!object.alive)
        continue;
      ++alive;
      require(object.serial == serial && object.addr == address,
              "isolated movement forbids any other live retained object");
    }
    require(alive == 1 && state.live.size() == 1 && state.live.count(address) &&
                state.live.at(address) == serial,
            "mover requires the sole current live World generation");
    const auto found = world->objects.find(address);
    require(found != world->objects.end(), "mover is not in the live registry");
    const auto identity = world->handle_object_identity(found->second);
    require(identity && identity->first == serial &&
                identity->second == address,
            "mover registry does not identify its retained World object");
    const auto cls = world->pool.class_of(address);
    require(cls < Pool::CLASSES.size() && address >= world->pool.base[cls] &&
                (address - world->pool.base[cls]) % Pool::CLASSES[cls][0] ==
                    0 &&
                !world->pool.link.count(address),
            "mover must own an aligned allocated World pool block");
    return *found->second;
  }
  void terrain_hp_preflight() const {
    if (!hp)
      return;
    hp_preflight();
    require(!hp->depth && !hp->adapter->operation_depth() &&
                !hp->adapter->raw_callback_depth(),
            "movement cannot enter an active HP operation");
    require(world->frame >= 0 && std::uint64_t(world->frame) <= UINT32_MAX,
            "movement HP publication requires an unsigned 32-bit frame");
    const auto &backing = hp->adapter->model().mem.bytes();
    std::size_t missing = 0;
    const auto count_missing = [&](std::uint32_t address, std::size_t count) {
      for (std::size_t k = 0; k < count; ++k)
        missing += !backing.count(address + std::uint32_t(k));
    };
    count_missing(creature_hp::G_SIM_FRAME, 4);
    for (const auto &binding : hp->bindings) {
      const auto *record = hp->adapter->lookup(binding.address);
      require(record && record->status.size() == 5,
              "movement HP publication requires five status values");
      for (const auto &span :
           std::array<std::pair<std::uint32_t, std::size_t>, 4>{
               {{0xc, 16}, {0xa8, 8}, {0x228, 4}, {0x340, 20}}})
        count_missing(binding.address + span.first, span.second);
    }
    if (backing.size() > hp->limits.memory.memory_bytes ||
        missing > hp->limits.memory.memory_bytes - backing.size())
      throw ResourceLimit("movement HP publication exceeds memory capacity");
  }
  objects::GameObject &creature_object() const {
    require(creature && creature->binding, "creature mover is not bound");
    const auto &b = *creature->binding;
    auto &o = isolated_object(b.world_serial, b.address);
    hp_preflight();
    require((o.flags & (0x20u | 0x40u | 0x80u | 0x10000000u)) == 0x20u,
            "movement requires a base nonplayer creature");
    require(o.angle <= UINT16_MAX, "creature angle must fit unsigned 16 bits");
    require(world->frame >= 0 && std::uint64_t(world->frame) <= UINT32_MAX,
            "creature frame must fit unsigned 32 bits");
    const auto *record = hp->adapter->lookup(b.address);
    require(record, "creature HP record is missing");
    const auto binding = hp->adapter->binding(*record);
    require(binding.generation == b.adapter_generation &&
                binding.projection ==
                    combat_creature::Projection::CreatureFields &&
                binding.dispatch == combat_creature::Dispatch::BaseCreature &&
                record->cls == U"Creature",
            "creature requires its original base HP binding");
    require((record->cflags & 0x100000) != 0,
            "creature movement requires disabled water effects");
    require(o.vtable == creature->initial.base_table &&
                creature->model->mem.load(std::uint64_t(o.addr) + 0x48, 4) ==
                    o.vtable,
            "creature dispatch table identity changed");
    for (const auto pair : std::array<std::pair<unsigned, std::uint32_t>, 3>{
             {{6, 0x11a418}, {10, 0x11a3f0}, {11, 0x11a448}}}) {
      const auto a = std::uint64_t(o.vtable) + 8 * pair.first;
      require(creature->model->mem.load(a, 2) == 0 &&
                  creature->model->mem.load(a + 4, 4) == pair.second,
              "creature base dispatch entry changed");
    }
    return o;
  }
  void creature_import(objects::GameObject &o) {
    hp_import_scalars();
    hp->adapter->project();
    auto &mem = creature->model->mem;
    const std::uint64_t c = o.addr;
    mem.poke(c + 0x1c, 4, o.radius);
    mem.poke(c + 0x20, 4, o.height);
    mem.poke(c + 0x30, 4, o.vz);
    mem.poke(c + 0x38, 2, o.angle);
  }
  void creature_publish() {
    // Raw memory is authoritative from entry until this publication. Never
    // project the stale typed HP position over an in-progress movement write.
    hp->adapter->publish();
    hp_publish_scalars();
    const auto &b = *creature->binding;
    auto &o = *hp->objects[hp_index(b.id)];
    const auto &mem = creature->model->mem;
    const std::uint64_t c = b.address;
    o.radius = Raw(mem.load(c + 0x1c, 4));
    o.height = Raw(mem.load(c + 0x20, 4));
    o.vz = Raw(mem.load(c + 0x30, 4));
    o.angle = std::uint32_t(mem.load(c + 0x38, 2));
  }
  void animation_identity(HpId id) const {
    hp_preflight();
    const auto index = hp_index(id);
    const auto &binding = hp->bindings[index];
    const auto found = world->objects.find(binding.address);
    require(found != world->objects.end(), "animation actor is not current");
    // Check ownership before dereferencing a caller-mutable registry pointer.
    const auto identity = world->handle_object_identity(found->second);
    require(identity && identity->first == binding.world_serial &&
                identity->second == binding.address &&
                found->second == hp->objects[index],
            "animation actor registry identity changed");
    require(found->second->alive, "animation actor is retired");
    const auto cls = world->pool.class_of(binding.address);
    require(cls < Pool::CLASSES.size() && Pool::CLASSES[cls][0] >= 0x3b2 &&
                binding.address >= world->pool.base[cls] &&
                (binding.address - world->pool.base[cls]) %
                        Pool::CLASSES[cls][0] ==
                    0 &&
                !world->pool.link.count(binding.address),
            "animation actor requires its allocated Creature pool block");
    const auto *record = hp->adapter->lookup(binding.address);
    require(record && hp->adapter->binding(*record).generation ==
                          binding.adapter_generation,
            "animation HP generation changed");
  }
  AnimationState::Binding &animation_binding(HpId id) {
    require(bool(animation), "animation runtime is not enabled");
    const auto found =
        std::find_if(animation->bindings.begin(), animation->bindings.end(),
                     [&](const auto &entry) { return entry.binding.id == id; });
    require(found != animation->bindings.end(), "unknown animation binding");
    animation_identity(id);
    return *found;
  }
  animation_clock::ClipPtr animation_clip(AnimationClipId id,
                                          bool allow_null) const {
    require(bool(animation), "animation runtime is not enabled");
    if (!id && allow_null)
      return {};
    require(id && id <= animation->clips.size(), "unknown animation clip");
    return animation->clips[std::size_t(id - 1)];
  }
  void animation_ready() const {
    require(bool(animation), "animation runtime is not enabled");
    hp_preflight();
    require(submitting && world_depth == 1 && world_phase == "tasks" &&
                object_run_completed && task_priority && *task_priority > 10,
            "animation requires an explicit post-runObjects ready task");
    require(!animation->depth && !hp->depth &&
                !hp->adapter->operation_depth() &&
                !hp->adapter->raw_callback_depth(),
            "nested animation or HP operations are unsupported");
    require(world->frame >= 0 && std::uint64_t(world->frame) <= UINT32_MAX,
            "animation frame must fit unsigned 32 bits");
    if (animation->calls_this_tick >= animation->limits.calls_per_tick)
      throw ResourceLimit("animation ready-tick call limit exceeded");
  }
  template <class Fn>
  std::optional<std::int32_t> animation_invoke(AnimationOperation operation,
                                               AnimationState::Binding *binding,
                                               Fn &&function) {
    auto &a = *animation;
    // One retained operation is the only history growth performed here. The
    // clock stores bounded cells and has no callbacks or additional logs.
    if (history() >= MAX_EVENT_HISTORY)
      throw ResourceLimit("animation aggregate history reserve exhausted");
    operation.frame = std::uint32_t(world->frame);
    a.operations.push_back(std::move(operation));
    auto &entry = a.operations.back();
    a.system.sim_frame = entry.frame;
    ++a.calls_this_tick;
    if (binding)
      ++binding->calls_this_tick;
    a.phase = entry.kind;
    a.depth = 1;
    if (entry.kind == "advance_animation") {
      binding->advanced_this_tick = true;
      binding->last_event.reset();
    }
    try {
      entry.result = function();
      if (entry.kind == "advance_animation")
        binding->last_event = entry.result;
      entry.completed = true;
      a.depth = 0;
      a.phase = "idle";
      return entry.result;
    } catch (...) {
      // The original clock already made its exact partial stores. No rewind,
      // frame writeback, retry, event dispatch, or RNG publication is added.
      a.depth = 0;
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
    std::size_t result =
        events.size() + seed.draws.size() + net.log.size() + net.outbox.size();
    if (hp) {
      const auto &m = hp->adapter->model();
      result += m.mem.log().size() + m.writes.size() + m.calls.size() +
                m.events.size() + hp->adapter->world().events.size() +
                hp->adapter->world().seed.draws.size() +
                hp->adapter->world().cosmetic.draws.size() +
                hp->adapter->world().shared_random_draw_count();
    }
    if (creature)
      result += creature->model->writes.size() + creature->model->calls.size() +
                creature->model->events.size() +
                creature->callback_results.size();
    if (animation)
      result += animation->operations.size();
    return result;
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
void Session::enable_terrain(const world_queries::LevelData &data,
                             world_queries::Limits limits) {
  auto &s = *state_;
  s.guard("enable_terrain", [&] {
    s.setup();
    require(bool(s.world), "terrain requires the existing object runtime");
    s.validate_world();
    require(!s.terrain, "terrain is already enabled");
    s.terrain = std::make_unique<State::TerrainState>(data, limits);
  });
}
MoverId Session::bind_isolated_mover(std::uint64_t serial,
                                     std::uint32_t address) {
  auto &s = *state_;
  return s.guard("bind_isolated_mover", [&]() -> MoverId {
    s.setup();
    require(bool(s.terrain), "terrain is not enabled");
    require(!s.terrain->binding, "isolated mover is already bound");
    require(!s.creature, "creature movement excludes the isolated mover");
    (void)s.isolated_object(serial, address);
    s.terrain->binding = TerrainBinding{1, serial, address};
    return 1;
  });
}
world_queries::MoveResult
Session::move_isolated(MoverId id, const world_queries::Vec3 &velocity) {
  auto &s = *state_;
  return s.guard("move_isolated", [&]() -> world_queries::MoveResult {
    require(s.terrain && s.terrain->binding && s.terrain->binding->id == id,
            "unknown isolated mover binding");
    require(s.submitting && s.world_depth == 1 && s.world_phase == "tasks" &&
                s.object_run_completed && s.task_priority &&
                *s.task_priority > 10,
            "movement requires an explicit post-runObjects ready task");
    auto &t = *s.terrain;
    require(!t.depth, "nested terrain movement is unsupported");
    if (t.calls_this_tick)
      throw ResourceLimit("isolated mover has already run this ready tick");
    auto &object =
        s.isolated_object(t.binding->world_serial, t.binding->address);
    s.terrain_hp_preflight();
    ++t.calls_this_tick;
    t.last_velocity = velocity;
    t.last_result.reset();
    t.publication_error.clear();
    t.phase = "terrain_query";
    t.depth = 1;
    bool committed = false;
    try {
      const auto result = world_queries::object_move_isolated(
          t.level, object.radius, object.pos, velocity, object.vz,
          object.flags);
      object.pos = result.position;
      object.vz = result.vz;
      t.last_result = result;
      committed = true;
      t.phase = "grid_update";
      // Reuse the original grid link even for a same-cell or off-grid move.
      // No replacement pair is allocated, including when the pool is full.
      s.world->update_grid_cell(object);
      if (s.hp) {
        t.phase = "hp_publish";
        s.hp_import_scalars();
        s.hp->adapter->project();
      }
      t.phase = "idle";
      t.depth = 0;
      return result;
    } catch (...) {
      const auto primary = std::current_exception();
      if (committed && s.hp) {
        // World owns the moved position; never restore a stale typed HP
        // position. Keep the primary phase/error if publication also fails.
        try {
          s.hp_import_scalars();
          s.hp->adapter->project();
        } catch (const std::exception &secondary) {
          t.publication_error = secondary.what();
        } catch (...) {
          t.publication_error =
              "nonstandard terrain scalar publication failure";
        }
      }
      t.depth = 0;
      std::rethrow_exception(primary);
    }
  });
}
void Session::enable_creature_movement() {
  auto &s = *state_;
  s.guard("enable_creature_movement", [&] {
    s.setup();
    require(s.world && s.hp && s.terrain,
            "creature movement requires existing World, HP and terrain");
    require(!s.creature && !s.terrain->binding,
            "creature movement excludes another movement binding");
    auto c = std::make_unique<State::CreatureState>();
    c->model = std::make_unique<creature_movement::Model>(
        s.hp->adapter->model().mem,
        creature_movement::CallbackPolicy::RequireHandlers,
        s.hp->limits.memory);
    using V = creature_movement::Value;
    using K = creature_movement::ValueKind;
    using Args = std::vector<V>;
    const auto bind = [&](const std::string &name, const std::string &phase,
                          creature_movement::Model::Callback callback) {
      c->model->callbacks[name] = [&s, phase, callback](auto &model,
                                                        const Args &args) {
        require(s.creature && s.creature->depth == 1,
                "creature service outside its movement call");
        s.creature->phase = phase;
        auto result = callback(model, args);
        if (s.creature->callback_results.size() >=
            s.hp->limits.memory.history_entries)
          throw ResourceLimit("creature callback result history exhausted");
        s.creature->callback_results.push_back({model.calls.size(), result});
        s.creature->phase = "move_by";
        return result;
      };
    };
    bind("world_slideMove", "slide", [&s](auto &model, const Args &args) {
      require(args.size() == 5 && args[0].kind == K::Integer &&
                  args[1].kind == K::Vector && args[2].kind == K::Integer &&
                  args[3].kind == K::Integer && args[4].kind == K::Integer &&
                  args[0].scalar ==
                      std::int64_t(s.creature->binding->address) + 0x10 &&
                  args[2].scalar == 3 && args[3].scalar == 1,
              "unsupported creature slide service arguments");
      s.creature_publish();
      const auto &o = *s.hp->objects[s.hp_index(s.creature->binding->id)];
      const auto result = world_queries::world_slide_move(
          s.terrain->level, Raw(args[4].scalar), o.pos, args[1].vec, 3);
      const auto a = std::uint64_t(s.creature->binding->address) + 0x10;
      // Service side effects are backing pokes, not creature-body stores.
      for (std::size_t i = 0; i < 3; ++i)
        model.mem.poke(a + 4 * i, 4, result.position[i]);
      s.creature_publish();
      return V::tuple({V::integer(result.hit),
                       result.normal ? V::vector(*result.normal) : V{}});
    });
    bind("obj_updateGridCell", "grid_update", [&s](auto &, const Args &args) {
      require(args.size() == 1 && args[0].kind == K::Integer &&
                  args[0].scalar == s.creature->binding->address,
              "unsupported creature grid service arguments");
      s.creature_publish();
      s.world->update_grid_cell(
          *s.hp->objects[s.hp_index(s.creature->binding->id)]);
      return V{};
    });
    bind("world_collectObjectsInBox", "collect",
         [&s](auto &, const Args &args) {
           require(args.size() == 3 && args[0].kind == K::Vector &&
                       args[1].kind == K::Vector &&
                       args[2].kind == K::Integer && args[2].scalar == 16,
                   "unsupported creature collection arguments");
           s.creature_publish();
           const auto found =
               s.world->collect_in_box(args[0].vec, args[1].vec, 16);
           std::vector<V> result;
           for (const auto *o : found) {
             const auto identity = s.world->handle_object_identity(o);
             require(identity &&
                         identity->first == s.creature->binding->world_serial &&
                         identity->second == s.creature->binding->address,
                     "singleton creature collection found another object");
             result.push_back(V::integer(identity->second));
           }
           return V::list(std::move(result));
         });
    bind("terrain_heightBelow", "height", [&s](auto &, const Args &args) {
      require(args.size() == 3 && args[0].kind == K::Integer &&
                  args[1].kind == K::Vector && args[2].kind == K::Integer,
              "unsupported creature height arguments");
      s.creature_publish();
      // The captured Level is the service identity. Preserve the raw world
      // token in the call observation without treating it as a host pointer.
      return V::integer(world_queries::terrain_height_below(
          s.terrain->level, Raw(args[2].scalar), args[1].vec));
    });
    s.creature = std::move(c);
  });
}
HpId Session::bind_creature_mover(HpId id, CreatureInitial initial) {
  auto &s = *state_;
  return s.guard("bind_creature_mover", [&]() -> HpId {
    s.setup();
    require(s.creature && !s.creature->binding && !s.terrain->binding,
            "creature binding must be unique");
    const auto index = s.hp_index(id);
    const auto b = s.hp->bindings[index];
    auto &o = s.isolated_object(b.world_serial, b.address);
    s.terrain_hp_preflight();
    require((o.flags & (0x20u | 0x40u | 0x80u | 0x10000000u)) == 0x20u &&
                o.angle <= UINT16_MAX,
            "creature binding requires base flags and a 16-bit angle");
    const auto &record = *s.hp->adapter->lookup(b.address);
    require((record.cflags & 0x100000) && record.cls == U"Creature",
            "creature binding requires dry base HP state");
    require(initial.base_table && std::uint64_t(initial.base_table) + 128 <=
                                      UINT64_C(0x100000000),
            "creature table must be a nonzero raw32 span");
    const auto overlaps = [&](std::uint64_t a, std::uint64_t size) {
      return initial.base_table < a + size &&
             a < std::uint64_t(initial.base_table) + 128;
    };
    for (std::size_t i = 0; i < Pool::CLASSES.size(); ++i)
      require(
          !overlaps(s.world->pool.base[i],
                    std::uint64_t(Pool::CLASSES[i][0]) * Pool::CLASSES[i][1]),
          "creature table intersects a World allocation arena");
    require(!overlaps(HandlePool::NODE_BASE,
                      HandlePool::NODE_COUNT * HandlePool::NODE_SIZE) &&
                !overlaps(0x864ea8, 4) &&
                !overlaps(creature_movement::G_SIM_FRAME, 4) &&
                !overlaps(creature_movement::G_WORLD, 4) &&
                !overlaps(0x404464, 4),
            "creature table intersects managed memory or a shared global");
    for (const auto &cell : s.world->handle_cells())
      require(!overlaps(cell.address, 4),
              "creature table intersects a World head");
    for (const auto &entry : s.hp->adapter->bindings())
      require(!overlaps(entry.address, entry.extent),
              "creature table intersects HP record");
    for (const auto &entry : s.hp->adapter->raw_regions())
      require(!overlaps(entry.address, entry.size),
              "creature table intersects raw reservation");
    auto &mem = s.creature->model->mem;
    const auto existing = mem.bytes().lower_bound(initial.base_table);
    require(existing == mem.bytes().end() ||
                existing->first >= std::uint64_t(initial.base_table) + 128,
            "creature table intersects existing backing bytes");
    std::vector<creature_hp::Write> setup;
    const auto push = [&](std::uint64_t a, std::size_t size,
                          std::uint64_t value) {
      require(a <= UINT32_MAX && a + size <= UINT64_C(0x100000000),
              "creature setup span exceeds raw32");
      setup.push_back({std::uint32_t(a), size, value});
    };
    const std::uint64_t address = b.address;
    push(address + 0x1c, 4, o.radius);
    push(address + 0x20, 4, o.height);
    push(address + 0x30, 4, o.vz);
    push(address + 0x38, 2, o.angle);
    push(address + 0x48, 4, initial.base_table);
    push(address + 0xbc, 8,
         std::uint64_t(initial.root[0]) |
             (std::uint64_t(initial.root[1]) << 32));
    push(address + 0xc4, 2, initial.root_yaw);
    push(address + 0x314, 2, initial.root_timer);
    push(address + 0x230, 1, initial.phase);
    push(address + 0x231, 1, initial.inert);
    for (const auto pair : std::array<std::pair<unsigned, std::uint32_t>, 3>{
             {{6, 0x11a418}, {10, 0x11a3f0}, {11, 0x11a448}}}) {
      push(std::uint64_t(initial.base_table) + 8 * pair.first, 2, 0);
      push(std::uint64_t(initial.base_table) + 8 * pair.first + 4, 4,
           pair.second);
    }
    std::vector<std::uint32_t> missing;
    const auto overlay = mem.overlay();
    for (const auto &w : setup)
      for (std::size_t k = 0; k < w.size; ++k) {
        const auto a = w.address + std::uint32_t(k);
        require(!overlay || !overlay->covers(a),
                "creature setup intersects read-only graph");
        if (!mem.bytes().count(a))
          missing.push_back(a);
      }
    // The full reservation may not alias a managed byte, including padding
    // between the six explicitly materialized virtual entries.
    for (std::uint64_t k = 0; k < 128; ++k)
      require(!overlay ||
                  !overlay->covers(std::uint32_t(initial.base_table + k)),
              "creature table overlaps read-only graph");
    std::sort(missing.begin(), missing.end());
    missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
    if (mem.bytes().size() > s.hp->limits.memory.memory_bytes ||
        missing.size() > s.hp->limits.memory.memory_bytes - mem.bytes().size())
      throw ResourceLimit("creature setup exceeds backing memory capacity");
    s.hp->adapter->reserve_raw(initial.base_table, 128,
                               "session.creature.base_table");
    for (const auto &w : setup)
      mem.poke(w.address, w.size, w.value);
    o.vtable = initial.base_table;
    s.creature->binding = b;
    s.creature->initial = initial;
    return id;
  });
}
std::uint32_t Session::move_creature(HpId id,
                                     const world_queries::Vec3 &velocity) {
  auto &s = *state_;
  return s.guard("move_creature", [&]() -> std::uint32_t {
    require(s.creature && s.creature->binding && s.creature->binding->id == id,
            "unknown creature mover");
    require(s.submitting && s.world_depth == 1 && s.world_phase == "tasks" &&
                s.object_run_completed && s.task_priority &&
                *s.task_priority > 10,
            "creature movement requires a post-runObjects ready task");
    auto &c = *s.creature;
    require(!c.depth && !s.terrain->depth,
            "nested creature movement is unsupported");
    if (c.calls_this_tick)
      throw ResourceLimit("creature mover has already run this ready tick");
    auto &o = s.creature_object();
    s.terrain_hp_preflight();
    (void)s.hp->handles->snapshot();
    // Ten possible body writes, four service calls/results, and bounded
    // publication are well below this reserve. Shared Memory.log counts once.
    if (s.history() > MAX_EVENT_HISTORY - 128)
      throw ResourceLimit("creature movement history reserve exhausted");
    ++c.calls_this_tick;
    c.last_velocity = velocity;
    c.last_bits.reset();
    c.publication_error.clear();
    c.phase = "import";
    c.depth = 1;
    bool imported = false;
    try {
      s.creature_import(o);
      imported = true;
      c.phase = "move_by";
      const auto bits = c.model->move_by(c.binding->address, velocity);
      c.last_bits = bits;
      c.phase = "publish";
      s.creature_publish();
      c.phase = "idle";
      c.depth = 0;
      return bits;
    } catch (...) {
      const auto primary = std::current_exception();
      if (imported)
        try {
          s.creature_publish();
        } catch (const std::exception &secondary) {
          c.publication_error = secondary.what();
        } catch (...) {
          c.publication_error = "nonstandard creature publication failure";
        }
      c.depth = 0;
      std::rethrow_exception(primary);
    }
  });
}

void Session::enable_animation(AnimationLimits limits) {
  auto &s = *state_;
  s.guard("enable_animation", [&] {
    s.setup();
    s.hp_preflight();
    require(!s.animation, "animation runtime is already enabled");
    if (!limits.clips || limits.clips > MAX_ANIMATION_CLIPS ||
        !limits.bindings || limits.bindings > MAX_ANIMATION_BINDINGS ||
        !limits.cells || limits.cells > MAX_ANIMATION_CELLS ||
        !limits.calls_per_tick || limits.calls_per_tick > MAX_ANIMATION_CALLS ||
        limits.clock.max_loop_subtractions > MAX_ANIMATION_LOOP_SUBTRACTIONS)
      throw ResourceLimit("animation limits exceed supported capacity");
    auto animation = std::make_unique<State::AnimationState>();
    animation->limits = limits;
    animation->system.limits = limits.clock;
    s.animation = std::move(animation);
  });
}

AnimationClipId
Session::add_animation_clip(const animation_clock::Clip &metadata) {
  auto &s = *state_;
  return s.guard("add_animation_clip", [&]() -> AnimationClipId {
    s.setup();
    require(bool(s.animation), "animation runtime is not enabled");
    s.hp_preflight();
    if (s.animation->clips.size() >= s.animation->limits.clips)
      throw ResourceLimit("animation clip registry limit exceeded");
    auto clip = std::make_shared<animation_clock::Clip>(metadata);
    s.animation->clips.push_back(std::move(clip));
    return s.animation->clips.size();
  });
}

HpId Session::bind_animation(HpId id, std::int32_t capacity) {
  auto &s = *state_;
  return s.guard("bind_animation", [&]() -> HpId {
    s.setup();
    require(bool(s.animation), "animation runtime is not enabled");
    s.animation_identity(id);
    if (capacity < 1 || capacity > 127)
      throw animation_clock::ValueError("capacity is a positive signed byte");
    auto &a = *s.animation;
    for (const auto &entry : a.bindings)
      require(entry.binding.id != id, "duplicate animation binding");
    if (a.bindings.size() >= a.limits.bindings ||
        std::size_t(capacity) > a.limits.cells - a.total_cells)
      throw ResourceLimit("animation binding or aggregate cell limit exceeded");
    a.bindings.emplace_back(s.hp->bindings[s.hp_index(id)], capacity);
    a.total_cells += std::size_t(capacity);
    return id;
  });
}

std::optional<std::int32_t> Session::start_animation(HpId id,
                                                     AnimationClipId clip_id,
                                                     Raw blend_in,
                                                     std::uint32_t flags) {
  auto &s = *state_;
  return s.guard("start_animation", [&]() -> std::optional<std::int32_t> {
    s.animation_ready();
    auto &binding = s.animation_binding(id);
    auto clip = s.animation_clip(clip_id, true);
    if (flags & animation_clock::PROTECTED)
      throw animation_clock::Unsupported("Session animation refuses ROOT/YAW");
    AnimationOperation operation;
    operation.kind = "start_animation";
    operation.binding = id;
    operation.clip = clip_id;
    operation.parameter = blend_in;
    operation.flags = flags;
    return s.animation_invoke(
        std::move(operation), &binding, [&]() -> std::optional<std::int32_t> {
          const auto *track =
              animation_clock::anim_start(blend_in, binding.state, clip, flags);
          if (!track)
            return {};
          return std::int32_t(track - binding.state.tracks.data());
        });
  });
}

void Session::stop_animation(HpId id, AnimationClipId clip_id, Raw blend_out) {
  auto &s = *state_;
  s.guard("stop_animation", [&] {
    s.animation_ready();
    auto &binding = s.animation_binding(id);
    auto clip = s.animation_clip(clip_id, true);
    AnimationOperation operation;
    operation.kind = "stop_animation";
    operation.binding = id;
    operation.clip = clip_id;
    operation.parameter = blend_out;
    (void)s.animation_invoke(
        std::move(operation), &binding, [&]() -> std::optional<std::int32_t> {
          animation_clock::anim_stop(blend_out, binding.state, clip);
          return {};
        });
  });
}

std::int32_t Session::advance_animation(HpId id, Raw speed) {
  auto &s = *state_;
  return s.guard("advance_animation", [&]() -> std::int32_t {
    s.animation_ready();
    auto &binding = s.animation_binding(id);
    if (binding.advanced_this_tick)
      throw ResourceLimit(
          "animation binding has already advanced this ready tick");
    AnimationOperation operation;
    operation.kind = "advance_animation";
    operation.binding = id;
    operation.parameter = speed;
    const auto result = s.animation_invoke(
        std::move(operation), &binding, [&]() -> std::optional<std::int32_t> {
          return animation_clock::anim_advance(s.animation->system, speed,
                                               binding.state);
        });
    return *result;
  });
}

void Session::set_animation_event(AnimationClipId clip_id, std::size_t slot,
                                  animation_clock::Event event) {
  auto &s = *state_;
  s.guard("set_animation_event", [&] {
    s.animation_ready();
    auto clip = s.animation_clip(clip_id, false);
    require(slot < clip->events.size(),
            "animation event slot is outside four slots");
    AnimationOperation operation;
    operation.kind = "set_animation_event";
    operation.clip = clip_id;
    operation.event_index = slot;
    operation.event = event;
    (void)s.animation_invoke(std::move(operation), nullptr,
                             [&]() -> std::optional<std::int32_t> {
                               clip->events[slot] = event;
                               return {};
                             });
  });
}

void Session::enable_hp(HpLimits limits) {
  auto &s = *state_;
  s.guard("enable_hp", [&] {
    s.setup();
    require(bool(s.world), "HP requires the existing object runtime");
    require(!s.hp, "HP runtime is already enabled");
    if (!limits.records || limits.records > 64 || !limits.calls_per_tick ||
        limits.calls_per_tick > 64)
      throw ResourceLimit("HP record/call limits exceed supported capacity");
    auto h = std::make_unique<State::HpState>();
    h->limits = limits;
    combat_creature::Limits l;
    l.entries = limits.records;
    l.memory = limits.memory;
    h->adapter = std::make_unique<combat_creature::Adapter>(
        0, 0, creature_hp::CallbackPolicy::RequireHandlers, l);
    h->handles = std::make_unique<raw_handles::RawHandles>(
        *s.world, h->adapter->model().mem);
    auto *raw = h->handles.get();
    h->adapter->set_handle_callback("handle_clear", "session.raw_handles.clear",
                                    [raw](auto &, const auto &args) {
                                      require(args.size() == 1,
                                              "invalid clear callback arity");
                                      raw->clear(args[0]);
                                    });
    h->adapter->set_handle_callback(
        "handle_link", "session.raw_handles.link",
        [raw](auto &, const auto &args) {
          require(args.size() == 3, "invalid link callback arity");
          (void)raw->link(args[0], args[1], args[2]);
        });
    s.hp = std::move(h);
  });
}
HpId Session::bind_hp(std::uint64_t serial, std::uint32_t address,
                      HpInitial initial) {
  auto &s = *state_;
  return s.guard("bind_hp", [&]() -> HpId {
    s.setup();
    s.hp_preflight();
    const auto found = s.world->objects.find(address);
    require(found != s.world->objects.end() &&
                found->second->serial == serial && found->second->alive,
            "HP binding requires a current owned World generation");
    const auto *object = found->second;
    const auto cls = s.world->pool.class_of(address);
    require(cls < Pool::CLASSES.size() && Pool::CLASSES[cls][0] >= 0x3b2 &&
                address >= s.world->pool.base[cls] &&
                (address - s.world->pool.base[cls]) % Pool::CLASSES[cls][0] ==
                    0,
            "World allocation is too small or not aligned for Creature fields");
    require(!s.world->pool.link.count(address),
            "World object does not own an allocated pool block");
    for (const auto &b : s.hp->bindings)
      require(b.address != address, "duplicate HP address");
    if (s.hp->bindings.size() >= s.hp->limits.records)
      throw ResourceLimit("HP record limit exceeded");
    // Validate all fixed setup spans/capacity before publishing a binding.
    const auto cells = s.world->handle_cells();
    std::size_t retained = 0;
    for (const auto &c : cells)
      if (!c.object_serial && c.identity >= 0x90000000u)
        ++retained;
    if (retained > objects::MAX_RETAINED - 3)
      throw ResourceLimit("HP retained head capacity exceeded");
    const auto registry = s.hp->handles->registry_snapshot();
    if (registry.heads.size() > 65536 - 6)
      throw ResourceLimit("HP head registry capacity exceeded");
    for (const auto off : {0u, 4u, 8u, 0x4cu, 0x2e4u, 0x360u}) {
      const auto a = std::uint64_t(address) + off;
      require(a + 4 <= UINT64_C(0x100000000), "HP head span exceeds raw32");
      for (const auto &h : registry.heads)
        require(!(a < std::uint64_t(h.address) + 4 && h.address < a + 4),
                "HP head already has a binding");
      for (const auto &c : cells) {
        if (off < 12 && c.cell == &object->heads[off / 4])
          continue;
        require(!(a < std::uint64_t(c.address) + 4 && c.address < a + 4),
                "HP head aliases an existing World cell");
      }
    }
    const auto &backing = s.hp->adapter->model().mem.bytes();
    std::size_t missing = 0;
    const auto count_missing = [&](std::uint32_t a, std::size_t n) {
      for (std::size_t k = 0; k < n; ++k)
        missing += !backing.count(a + std::uint32_t(k));
    };
    count_missing(creature_hp::G_SIM_FRAME, 4);
    for (const auto &span :
         std::array<std::pair<std::uint32_t, std::size_t>, 4>{
             {{0xc, 16}, {0xa8, 8}, {0x228, 4}, {0x340, 20}}})
      count_missing(address + span.first, span.second);
    if (backing.size() > s.hp->limits.memory.memory_bytes ||
        missing > s.hp->limits.memory.memory_bytes - backing.size())
      throw ResourceLimit("HP scalar setup exceeds memory capacity");
    combat::Obj value;
    value.flags = object->flags;
    value.pos.assign(object->pos.begin(), object->pos.end());
    value.hp = initial.hp;
    value.cflags = initial.cflags;
    value.status.assign(initial.status.begin(), initial.status.end());
    auto &record = s.hp->adapter->create(address, std::move(value));
    HpBinding b{s.hp->bindings.size() + 1, serial, address,
                s.hp->adapter->binding(record).generation};
    s.hp->bindings.push_back(b);
    s.hp->objects.push_back(found->second);
    s.hp->handles->bind_object(*found->second);
    for (const auto off : {0x4cu, 0x2e4u, 0x360u})
      s.hp->handles->intern_raw_head(address + off,
                                     "hp:" + std::to_string(b.id) + ":" +
                                         std::to_string(off));
    s.hp_import_scalars();
    s.hp->adapter->project();
    return b.id;
  });
}
void Session::seed_hp_backing(HpId id,
                              const std::vector<creature_hp::Write> &writes) {
  auto &s = *state_;
  s.guard("seed_hp_backing", [&] {
    s.setup();
    s.hp_preflight();
    const auto i = s.hp_index(id);
    const auto address = s.hp->bindings[i].address;
    if (writes.size() > 256)
      throw ResourceLimit("HP setup write count exceeded");
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 7>
        protected_spans{{{0, 0x1c},
                         {0x4c, 4},
                         {0xa8, 8},
                         {0x228, 4},
                         {0x2e4, 4},
                         {0x340, 20},
                         {0x360, 4}}};
    for (const auto &w : writes) {
      if (s.creature && s.creature->binding && s.creature->binding->id == id)
        for (const auto p : std::array<std::pair<unsigned, unsigned>, 5>{
                 {{0x1c, 8}, {0x30, 4}, {0x38, 2}, {0x44, 4}, {0x48, 4}}})
          require(
              !(std::uint64_t(w.address) < std::uint64_t(p.first) + p.second &&
                p.first < std::uint64_t(w.address) + w.size),
              "HP backing write intersects creature World authority");
      require(w.size > 0 && w.size <= 8 &&
                  std::uint64_t(w.address) + w.size <= 0x3b2,
              "HP backing write out of record");
      for (const auto &p : protected_spans)
        require(
            !(std::uint64_t(w.address) < std::uint64_t(p.first) + p.second &&
              p.first < std::uint64_t(w.address) + w.size),
            "HP backing write intersects projected or managed fields");
    }
    const auto &backing = s.hp->adapter->model().mem.bytes();
    std::vector<std::uint32_t> missing;
    for (const auto &w : writes)
      for (std::size_t k = 0; k < w.size; ++k) {
        auto a = address + w.address + std::uint32_t(k);
        if (!backing.count(a))
          missing.push_back(a);
      }
    std::sort(missing.begin(), missing.end());
    missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
    if (backing.size() > s.hp->limits.memory.memory_bytes ||
        missing.size() > s.hp->limits.memory.memory_bytes - backing.size())
      throw ResourceLimit("HP setup writes exceed memory capacity");
    for (const auto &w : writes)
      s.hp->adapter->model().mem.poke(std::uint64_t(address) + w.address,
                                      w.size, w.value);
  });
}
void Session::link_hp_head(HpId id, std::uint32_t offset,
                           std::uint32_t target) {
  auto &s = *state_;
  s.guard("link_hp_head", [&] {
    s.setup();
    s.hp_preflight();
    auto i = s.hp_index(id);
    require(offset == 0x4c || offset == 0x2e4 || offset == 0x360,
            "unsupported HP head offset");
    (void)s.hp->handles->link(s.hp->bindings[i].address + offset, target,
                              target);
  });
}
void Session::take_damage(HpId victim, Raw amount, std::optional<HpId> attacker,
                          std::uint32_t flags) {
  auto &s = *state_;
  s.guard("take_damage", [&] {
    s.hp_preflight();
    require(s.submitting && s.world_depth && s.world_phase == "tasks" &&
                s.object_run_completed && s.task_priority &&
                *s.task_priority > 10,
            "HP requires an explicit post-runObjects ready task");
    require(!s.hp->depth, "nested Session HP calls are unsupported");
    const auto vi = s.hp_index(victim);
    std::optional<std::size_t> ai;
    if (attacker)
      ai = s.hp_index(*attacker);
    require(s.hp->objects[vi]->alive && (!ai || s.hp->objects[*ai]->alive),
            "HP target binding is retired");
    if (s.hp->calls_this_tick >= s.hp->limits.calls_per_tick ||
        s.history() > MAX_EVENT_HISTORY - 32)
      throw ResourceLimit("HP call or aggregate history budget exceeded");
    // Refuse unencodable/active iterator graphs before any HP projection/write.
    (void)s.hp->handles->snapshot();
    ++s.hp->calls_this_tick;
    s.hp->phase = "take_damage";
    s.hp->depth = 1;
    try {
      s.hp_import_scalars();
      auto &a = *s.hp->adapter;
      a.take_damage(*a.lookup(s.hp->bindings[vi].address), amount,
                    ai ? a.lookup(s.hp->bindings[*ai].address) : nullptr,
                    flags);
      s.hp_publish_scalars();
      s.hp->depth = 0;
      s.hp->phase = "idle";
    } catch (...) {
      const auto error = std::current_exception();
      // Adapter already publishes its raw partial state on HP exceptions.
      try {
        s.hp_publish_scalars();
      } catch (const std::exception &secondary) {
        s.hp->publication_error = secondary.what();
      } catch (...) {
        s.hp->publication_error = "nonstandard scalar publication failure";
      }
      s.hp->depth = 0;
      std::rethrow_exception(error);
    }
  });
}
void Session::enable_base_combat(BaseCombatConfig config) {
  auto &s = *state_;
  s.guard("enable_base_combat", [&] {
    s.setup();
    s.hp_preflight();
    require(!s.hp->base_combat, "base combat is already enabled");
    auto &w = s.hp->adapter->world();
    // These services are outside the selected base path. Unexpected
    // reachability must fail explicitly, never acquire an implicit zero/no-op
    // implementation.
    w.stat_fn = [](auto, auto, auto) -> std::int64_t {
      throw Error("base combat stat service is unsupported");
    };
    w.stat_for_fn = [](auto, auto, auto, auto *, auto) -> std::int64_t {
      throw Error("base combat stat_for service is unsupported");
    };
    w.adjust_fn = [](auto, auto, auto) -> std::int64_t {
      throw Error("base combat adjust service is unsupported");
    };
    w.pool_fn = [](auto) -> bool {
      throw Error("base combat effect allocation is unsupported");
    };
    w.enemy_fn = [](auto *, auto *) -> std::int64_t {
      throw Error("base combat enemy service is unsupported");
    };
    w.focus.assign(config.focus.begin(), config.focus.end());
    w.cutscene = config.cutscene;
    w.network_mode = 1;
    w.bind_shared_random(s.world->shared, "objects.shared");
    s.hp->base_combat = BaseCombatSnapshot{config, {}};
  });
}
void Session::configure_base_combat(HpId id, BaseCombatInitial initial) {
  auto &s = *state_;
  s.guard("configure_base_combat", [&] {
    s.setup();
    s.hp_preflight();
    const auto i = s.hp_index(id);
    s.base_combat_record(i, false);
    auto &configured = s.hp->base_combat->configured;
    require(std::find(configured.begin(), configured.end(), id) ==
                configured.end(),
            "base combat record is already configured");
    configured.push_back(id);
    s.hp->adapter->lookup(s.hp->bindings[i].address)->armor = initial.armor;
  });
}
int Session::apply_base_damage(HpId victim, HpId attacker, Raw minimum,
                               Raw maximum, std::int32_t attack,
                               std::uint32_t flags) {
  auto &s = *state_;
  return s.guard("apply_base_damage", [&] {
    s.hp_preflight();
    require(s.submitting && s.world_depth && s.world_phase == "tasks" &&
                s.object_run_completed && s.task_priority &&
                *s.task_priority > 10,
            "base combat requires an explicit post-runObjects ready task");
    require(!s.hp->depth, "nested Session combat calls are unsupported");
    require(s.world->frame >= 0 && std::uint64_t(s.world->frame) <= UINT32_MAX,
            "base combat simulation frame must fit unsigned 32 bits");
    require(!(flags & ~(0x40u | 0x4000u | 0x8000u)),
            "base combat flags are outside the supported subset");
    const auto vi = s.hp_index(victim), ai = s.hp_index(attacker);
    s.base_combat_record(vi, true);
    s.base_combat_record(ai, true);
    auto &a = *s.hp->adapter;
    auto &v = *a.lookup(s.hp->bindings[vi].address);
    auto &source = *a.lookup(s.hp->bindings[ai].address);
    require(!(v.cflags & (combat::CF_BLOCKING | combat::CF_THORNS)),
            "base combat blocking and thorns are unsupported");
    // At most 7 attributed draws + 22 HP memory/model entries + one
    // take_damage event + one sound event = 31. Reserve 64 and keep the
    // existing separate network-tail reserve. EMPTY_DESC still consumes four
    // zero-chance rolls after its first hit test. Cosmetic paths are excluded.
    if (s.hp->calls_this_tick >= s.hp->limits.calls_per_tick ||
        s.history() > MAX_EVENT_HISTORY - 64)
      throw ResourceLimit(
          "base combat call or aggregate history budget exceeded");
    (void)s.hp->handles->snapshot();
    ++s.hp->calls_this_tick;
    s.hp->phase = "damage_apply";
    s.hp->depth = 1;
    try {
      s.hp_import_scalars();
      const auto result = a.damage(v, &source, minimum, maximum, attack, flags,
                                   &combat::EMPTY_DESC, 1);
      s.hp_publish_scalars();
      s.hp->depth = 0;
      s.hp->phase = "idle";
      return result;
    } catch (...) {
      const auto error = std::current_exception();
      try {
        s.hp_publish_scalars();
      } catch (const std::exception &secondary) {
        s.hp->publication_error = secondary.what();
      } catch (...) {
        s.hp->publication_error = "nonstandard scalar publication failure";
      }
      s.hp->depth = 0;
      std::rethrow_exception(error);
    }
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
        s.object_run_completed = false;
        if (s.hp)
          s.hp->calls_this_tick = 0;
        if (s.terrain)
          s.terrain->calls_this_tick = 0;
        if (s.creature)
          s.creature->calls_this_tick = 0;
        if (s.animation) {
          s.animation->calls_this_tick = 0;
          for (auto &entry : s.animation->bindings) {
            entry.calls_this_tick = 0;
            entry.advanced_this_tick = false;
          }
        }
        s.world->tasks_run(1, [&](std::uint32_t function) {
          s.validate_world();
          if (function == OBJECT_RUN_TASK) {
            s.event("task", "runObjects");
            s.world->run_objects();
            s.object_run_completed = true;
          } else {
            const auto found = s.object_tasks.find(function);
            require(found != s.object_tasks.end(), "unregistered World task");
            auto handler = found->second.second;
            s.event("task", found->second.first);
            const auto current = std::find_if(
                s.world->tasks.tasks.begin(), s.world->tasks.tasks.end(),
                [&](const auto &t) { return t.fn == function; });
            require(current != s.world->tasks.tasks.end(),
                    "running task is missing from scheduler");
            s.task_priority = current->priority;
            struct ResetTask {
              std::optional<std::int64_t> &p;
              ~ResetTask() { p.reset(); }
            } reset_task{s.task_priority};
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
  if (s.animation) {
    const auto &a = *s.animation;
    AnimationSnapshot value;
    value.limits = a.limits;
    value.system = a.system;
    for (std::size_t i = 0; i < a.clips.size(); ++i)
      value.clips.push_back({i + 1, *a.clips[i]});
    for (const auto &entry : a.bindings) {
      AnimationBindingSnapshot binding;
      binding.binding = entry.binding;
      binding.pose = entry.state.pose;
      binding.count = entry.state.count;
      binding.capacity = entry.state.capacity;
      binding.flags = entry.state.flags;
      for (const auto &cell : entry.state.tracks) {
        AnimationClipId clip_id = 0;
        if (cell.clip) {
          const auto found =
              std::find(a.clips.begin(), a.clips.end(), cell.clip);
          require(found != a.clips.end(), "animation cell has an unowned clip");
          clip_id = std::uint64_t(found - a.clips.begin()) + 1;
        }
        binding.tracks.push_back({clip_id, cell.slot, cell.weight, cell.rate,
                                  cell.life, cell.life_rate, cell.time,
                                  cell.flags, cell.sound_idx});
      }
      binding.calls_this_tick = entry.calls_this_tick;
      binding.advanced_this_tick = entry.advanced_this_tick;
      binding.last_event = entry.last_event;
      value.bindings.push_back(std::move(binding));
    }
    value.phase = a.phase;
    value.depth = a.depth;
    value.calls_this_tick = a.calls_this_tick;
    value.total_cells = a.total_cells;
    value.operations = a.operations;
    value.object_run_completed = s.object_run_completed;
    value.task_priority = s.task_priority;
    out.animation = std::move(value);
  }
  if (s.terrain) {
    TerrainSnapshot t;
    t.level = s.terrain->level.snapshot();
    t.binding = s.terrain->binding;
    t.phase = s.terrain->phase;
    t.publication_error = s.terrain->publication_error;
    t.depth = s.terrain->depth;
    t.calls_this_tick = s.terrain->calls_this_tick;
    t.last_velocity = s.terrain->last_velocity;
    t.last_result = s.terrain->last_result;
    t.object_run_completed = s.object_run_completed;
    t.task_priority = s.task_priority;
    out.terrain = std::move(t);
  }
  if (s.hp) {
    HpSnapshot h;
    h.limits = s.hp->limits;
    h.bindings = s.hp->bindings;
    h.phase = s.hp->phase;
    h.depth = s.hp->depth;
    h.calls_this_tick = s.hp->calls_this_tick;
    h.publication_error = s.hp->publication_error;
    h.object_run_completed = s.object_run_completed;
    h.task_priority = s.task_priority;
    h.base_combat = s.hp->base_combat;
    h.adapter = s.hp->adapter->snapshot();
    h.registry = s.hp->handles->registry_snapshot();
    try {
      h.physical = s.hp->handles->snapshot();
    } catch (const std::exception &e) {
      h.physical_error = e.what();
    }
    out.hp = std::move(h);
  }
  if (s.creature) {
    const auto &c = *s.creature;
    CreatureMovementSnapshot m;
    m.binding = c.binding;
    m.initial = c.initial;
    m.phase = c.phase;
    m.publication_error = c.publication_error;
    m.depth = c.depth;
    m.calls_this_tick = c.calls_this_tick;
    m.last_velocity = c.last_velocity;
    m.last_bits = c.last_bits;
    m.limits = s.hp->limits.memory;
    m.callback_policy = c.model->policy;
    m.borrowed_memory = c.model->borrows_memory();
    for (const auto &entry : c.model->callbacks) {
      const auto label =
          entry.first == "world_slideMove"      ? "session.creature.slide"
          : entry.first == "obj_updateGridCell" ? "session.creature.grid"
          : entry.first == "world_collectObjectsInBox"
              ? "session.creature.collect"
              : "session.creature.height";
      m.callbacks.push_back({entry.first, label, bool(entry.second)});
    }
    m.external.assign(c.model->external.begin(), c.model->external.end());
    m.writes = c.model->writes;
    m.calls = c.model->calls;
    m.events = c.model->events;
    m.callback_results = c.callback_results;
    m.draws = c.model->draws;
    m.object_run_completed = s.object_run_completed;
    m.task_priority = s.task_priority;
    out.creature_movement = std::move(m);
  }
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
