#pragma once
#include "champions/amx.hpp"
#include "champions/animation_clock.hpp"
#include "champions/combat_creature.hpp"
#include "champions/combat_items.hpp"
#include "champions/combat_stats.hpp"
#include "champions/creature_movement.hpp"
#include "champions/netwire.hpp"
#include "champions/objects.hpp"
#include "champions/play_session.hpp"
#include "champions/raw_handles.hpp"
#include "champions/session_anim_cache.hpp"
#include "champions/session_recovery.hpp"
#include "champions/world_queries.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace champions::simulation::rta::detail {
struct Checkpoint;
struct CheckpointAccess;
} // namespace champions::simulation::rta::detail
namespace champions::simulation::rta::session_control {
class Controller;
struct Observation;
} // namespace champions::simulation::rta::session_control
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
struct BaseCombatConfig {
  std::array<Raw, 2> focus{};
  bool cutscene = false;
};
struct BaseCombatInitial {
  std::int16_t armor = 0;
};
struct BaseCombatSnapshot {
  BaseCombatConfig config;
  std::vector<HpId> configured;
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
  std::optional<BaseCombatSnapshot> base_combat;
};
using MoverId = std::uint64_t;
struct TerrainBinding {
  MoverId id = 0;
  std::uint64_t world_serial = 0;
  std::uint32_t address = 0;
};
struct TerrainSnapshot {
  world_queries::Snapshot level;
  std::optional<TerrainBinding> binding;
  std::string policy = "rta-session-terrain-v1", phase = "idle",
              publication_error;
  std::size_t depth = 0, calls_this_tick = 0;
  std::optional<world_queries::Vec3> last_velocity;
  std::optional<world_queries::MoveResult> last_result;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
struct CreatureInitial {
  std::array<Raw, 2> root{};
  std::uint16_t root_yaw = 0, root_timer = 0;
  std::uint8_t phase = 0, inert = 0;
  // Explicit authored dispatch storage, not an inferred original table.
  std::uint32_t base_table = 0;
};
struct CreatureCallbackResult {
  std::size_t call = 0;
  creature_movement::Value value;
};
struct CreatureMovementSnapshot {
  std::optional<HpBinding> binding;
  CreatureInitial initial;
  std::string policy = "rta-session-creature-v1", phase = "idle",
              publication_error, memory_owner = "hp.adapter.model.mem";
  std::size_t depth = 0, calls_this_tick = 0;
  std::optional<world_queries::Vec3> last_velocity;
  std::optional<std::uint32_t> last_bits;
  creature_movement::Limits limits;
  creature_movement::CallbackPolicy callback_policy =
      creature_movement::CallbackPolicy::RequireHandlers;
  bool borrowed_memory = true;
  std::vector<ObjectBindingSnapshot> callbacks;
  std::vector<std::string> external;
  std::vector<creature_movement::Write> writes;
  std::vector<creature_movement::Call> calls;
  std::vector<creature_movement::Event> events;
  std::vector<CreatureCallbackResult> callback_results;
  std::uint64_t draws = 0;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
inline constexpr std::size_t MAX_TURN_BINDINGS = 64, MAX_TURN_CALLS = 64;
struct TurnLimits {
  std::size_t bindings = MAX_TURN_BINDINGS, calls_per_tick = MAX_TURN_CALLS;
};
struct TurnInitial {
  Raw rate = ZERO;
  std::uint16_t angular_velocity = 0;
  std::uint8_t slow = 0;
};
struct TurnBindingSnapshot {
  HpBinding binding;
  TurnInitial initial;
  std::size_t calls_this_tick = 0;
  bool turned_this_tick = false;
  std::optional<std::int16_t> last_result;
};
struct TurnOperation {
  std::string kind = "turn_toward";
  HpId binding = 0;
  std::int32_t mode = 0;
  creature_movement::Vec3 point{};
  std::uint32_t frame = 0;
  std::optional<std::int16_t> result;
  bool completed = false;
};
struct TurningSnapshot {
  TurnLimits limits;
  std::vector<TurnBindingSnapshot> bindings;
  std::string policy = "rta-session-turn-v1", phase = "idle", publication_error,
              memory_owner = "hp.adapter.model.mem";
  std::size_t depth = 0, calls_this_tick = 0;
  creature_movement::Limits model_limits;
  creature_movement::CallbackPolicy callback_policy =
      creature_movement::CallbackPolicy::RequireHandlers;
  bool borrowed_memory = true;
  std::vector<ObjectBindingSnapshot> callbacks;
  std::vector<std::string> external;
  std::vector<creature_movement::Write> writes;
  std::vector<creature_movement::Call> calls;
  std::vector<creature_movement::Event> events;
  std::uint64_t draws = 0;
  std::vector<TurnOperation> operations;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
struct RecoverySnapshot {
  HpBinding binding;
  session_recovery::Initial initial;
  std::string policy = session_recovery::POLICY, phase = "idle",
              publication_error;
  std::string memory_owner = "hp.adapter.model.mem";
  std::size_t depth = 0, calls_this_tick = 0;
  std::vector<player::Write> writes;
  std::vector<player::Call> calls;
  std::vector<player::Event> events;
  std::uint64_t draws = 0;
  player::Address R0 = 0;
  player::Tick::Policy callback_policy = player::Tick::Policy::RequireHandlers;
  std::vector<ObjectBindingSnapshot> callbacks;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
using AnimationClipId = std::uint64_t;
inline constexpr std::size_t MAX_ANIMATION_CLIPS = 64,
                             MAX_ANIMATION_BINDINGS = 16,
                             MAX_ANIMATION_CELLS = 512,
                             MAX_ANIMATION_CALLS = 64;
inline constexpr std::uint32_t MAX_ANIMATION_LOOP_SUBTRACTIONS = 65536;
struct AnimationLimits {
  std::size_t clips = MAX_ANIMATION_CLIPS, bindings = MAX_ANIMATION_BINDINGS,
              cells = MAX_ANIMATION_CELLS, calls_per_tick = MAX_ANIMATION_CALLS;
  animation_clock::Limits clock{};
};
struct AnimationClipSnapshot {
  AnimationClipId id = 0;
  animation_clock::Clip value;
};
struct AnimationTrackSnapshot {
  AnimationClipId clip = 0;
  std::uint64_t slot = 0;
  Raw weight = ZERO, rate = ZERO, life = ZERO, life_rate = ZERO, time = ZERO;
  std::uint32_t flags = 0;
  std::int16_t sound_idx = 0;
};
struct AnimationBindingSnapshot {
  HpBinding binding;
  std::uint64_t pose = 0;
  std::int32_t count = 0, capacity = 0;
  std::uint32_t flags = 0;
  std::vector<AnimationTrackSnapshot> tracks;
  std::size_t calls_this_tick = 0;
  bool advanced_this_tick = false;
  std::optional<std::int32_t> last_event;
};
struct AnimationOperation {
  std::string kind;
  HpId binding = 0;
  AnimationClipId clip = 0;
  std::uint32_t frame = 0;
  Raw parameter = ZERO;
  std::uint32_t flags = 0;
  std::size_t event_index = 0;
  animation_clock::Event event{};
  std::optional<std::int32_t> result;
  bool completed = false;
};
struct AnimationSnapshot {
  AnimationLimits limits;
  animation_clock::AnimSystem system;
  std::vector<AnimationClipSnapshot> clips;
  std::vector<AnimationBindingSnapshot> bindings;
  std::string policy = "rta-session-animation-v1", phase = "idle";
  std::size_t depth = 0, calls_this_tick = 0, total_cells = 0;
  std::vector<AnimationOperation> operations;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
struct AnimationCacheBindingSnapshot {
  HpBinding binding;
  animation_runtime::StateId state = 0;
  std::size_t calls_this_tick = 0;
  bool advanced_this_tick = false;
  std::optional<std::int32_t> last_event;
  std::optional<AnimationCacheDecoded> last_decoded;
};
struct AnimationCacheSnapshot {
  AnimationCacheLimits limits;
  animation_runtime::Snapshot runtime;
  std::vector<AnimationCacheBindingSnapshot> bindings;
  std::string policy = ANIMATION_CACHE_POLICY, phase = "idle";
  std::size_t depth = 0, calls_this_tick = 0;
  std::vector<AnimationCacheOperation> operations;
  bool object_run_completed = false;
  std::optional<std::int64_t> task_priority;
};
struct Snapshot {
  Config config;
  std::optional<HpSnapshot> hp;
  std::optional<TerrainSnapshot> terrain;
  std::optional<CreatureMovementSnapshot> creature_movement;
  std::optional<AnimationSnapshot> animation;
  std::optional<AnimationCacheSnapshot> animation_cache;
  std::optional<TurningSnapshot> turning;
  std::optional<RecoverySnapshot> recovery;
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
// Callback-free scalar display projection. Histories remain owned and intact.
struct ActorObservation {
  world_queries::Vec3 position{};
  Raw vz = 0, radius = 0, hp = 0;
  std::uint32_t facing = 0;
  std::int64_t frame = 0;
};
class Session {
  friend struct detail::CheckpointAccess;
  void checkpoint(detail::Checkpoint &);

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
  // Supplied post-fixup terrain only; no file loading or inferred controls.
  // Setup freezes on submit. The binding must be the sole live retained actor.
  void enable_terrain(const world_queries::LevelData &,
                      world_queries::Limits = {});
  MoverId bind_isolated_mover(std::uint64_t world_serial,
                              std::uint32_t address);
  // Explicit raw velocity, once per actual post-runObjects ready tick. Every
  // call rechecks global isolation; no dynamic-object contact is omitted.
  world_queries::MoveResult move_isolated(MoverId,
                                          const world_queries::Vec3 &velocity);
  // Original dry singleton Creature_moveBy, sharing HP backing and terrain.
  // Requires enabled HP/terrain and no isolated mover. Dispatch table storage
  // must be nonzero/disjoint, and the creature must keep cflags bit0x100000.
  void enable_creature_movement();
  HpId bind_creature_mover(HpId, CreatureInitial initial = {});
  std::uint32_t move_creature(HpId, const world_queries::Vec3 &velocity);
  // Original scalar turning over the same HP Memory, without terrain, a mover
  // or a virtual table. Setup freezes on submit. Initial raw fields are
  // explicit.
  // Closed recovery over the existing HP Memory. Setup freezes on submit.
  // The initial health must match the authoritative base HP at binding.
  void enable_recovery(HpId, const session_recovery::Initial &);
  // Once per ready tick in an explicit task after runObjects. Base HP imports
  // to record HP, health then mana execute, completed record HP publishes back.
  void recover_player(HpId);
  void enable_turning(TurnLimits limits = {});
  HpId bind_turner(HpId, TurnInitial);
  // Once per binding per actual post-runObjects ready tick (priority > 10).
  // Imports World X/Y into selected typed/raw HP X/Y and imports World angle;
  // preserves each existing Z. Publishes only facing, including on failure.
  std::int16_t turn_toward(HpId, std::int32_t mode,
                           const creature_movement::Vec3 &point);
  // Base slot2 only, fixed shared-pool callbacks. Setup freezes on submit.
  void enable_hp(HpLimits limits = {});
  HpId bind_hp(std::uint64_t world_serial, std::uint32_t address,
               HpInitial initial = {});
  // Setup copies clip metadata. Shared identity exists only through returned
  // IDs; snapshots contain values/IDs, never mutable aliases to live tracks.
  // No raw creature animation-memory projection or cosmetic RNG binding.
  void enable_animation(AnimationLimits limits = {});
  AnimationClipId add_animation_clip(const animation_clock::Clip &);
  HpId bind_animation(HpId, std::int32_t capacity = 8);
  // Explicit authored post-runObjects ready tasks only (priority > 10).
  // Start/stop accept clip ID zero as the original null-clip no-op. Each
  // binding advances at most once per ready tick; all four methods share the
  // aggregate call budget. Existing World frame is read, never changed.
  std::optional<std::int32_t> start_animation(HpId, AnimationClipId,
                                              Raw blend_in = ZERO,
                                              std::uint32_t flags = 0);
  void stop_animation(HpId, AnimationClipId, Raw blend_out = ZERO);
  std::int32_t advance_animation(HpId, Raw speed = ONE);
  void set_animation_event(AnimationClipId, std::size_t slot,
                           animation_clock::Event);
  // Separately selected cache policy; mutually exclusive with enable_animation.
  // Owns exactly one Runtime. Setup freezes on first submit. All execution is
  // explicit post-runObjects ready work (priority > 10), never a render getter.
  void enable_animation_cache(std::size_t slots = 2,
                              AnimationCacheLimits limits = {});
  animation_runtime::ClipId
  add_animation_cache_clip(const std::vector<std::uint8_t> &,
                           const std::string &name = {});
  animation_runtime::ClipId
  add_animation_cache_clip(const animation_runtime::Clip &);
  animation_runtime::StateId bind_animation_cache(HpId,
                                                  std::int32_t capacity = 8);
  animation_runtime::TrackId start_animation_cache(HpId,
                                                   animation_runtime::ClipId,
                                                   Raw blend_in = ZERO,
                                                   std::uint32_t flags = 0);
  void stop_animation_cache(HpId, animation_runtime::ClipId,
                            Raw blend_out = ZERO);
  std::int32_t advance_animation_cache(HpId, Raw speed = ONE);
  void set_animation_cache_event(animation_runtime::ClipId, std::size_t,
                                 animation_decoder::Event);
  // Selected cell must be in this binding's live range. Decode and sample every
  // declared joint at that cell's current raw32 time; returned values are
  // owned.
  AnimationCacheDecoded decode_animation_cache(HpId,
                                               animation_runtime::TrackId);
  // Write.address is a relative record offset; projected/managed spans refuse.
  void seed_hp_backing(HpId, const std::vector<creature_hp::Write> &);
  // offset must be 0x4c,0x2e4,0x360. Target backhead must already be
  // registered.
  void link_hp_head(HpId, std::uint32_t offset, std::uint32_t raw_target);
  // Only a post-runObjects task (priority>10) can invoke this operation.
  void take_damage(HpId victim, Raw amount, std::optional<HpId> attacker = {},
                   std::uint32_t flags = 0);
  // Restricted nonplayer base combat, with direct World.shared draw authority.
  // Both records require explicit setup. No skills, descriptors, player or
  // projectile dispatch, reflection or DOT. Only 0x40|0x4000|0x8000 flags.
  void enable_base_combat(BaseCombatConfig config = {});
  void configure_base_combat(HpId, BaseCombatInitial initial = {});
  int apply_base_damage(HpId victim, HpId attacker, Raw minimum, Raw maximum,
                        std::int32_t attack, std::uint32_t flags = 0x40);
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
  ActorObservation observe_actor(HpId) const;

private:
  friend class session_control::Controller;
  play_session::Retained play_retained() const;
  void retire_play_history();
  // Closed Controller projection: no histories, callbacks or mutable aliases.
  session_control::Observation control_observation(std::uint64_t actor_serial,
                                                   HpId) const;
  animation_runtime::TrackId control_animation_track(HpId) const;
  std::size_t control_animation_operations() const;
  struct State;
  std::unique_ptr<State> state_;
  void ready_tick();
};
} // namespace champions::simulation::rta::session
