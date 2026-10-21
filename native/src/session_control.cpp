#include "champions/detail/checkpoint_control_anim_cache.hpp"
// Authored diagnostic control policy over the already pinned Session, object,
// terrain and HP contracts. This is not a port of original Player_move.
#include "champions/session_control.hpp"
#include <algorithm>
#include <utility>

namespace champions::simulation::rta::session_control {
namespace {
constexpr std::uint32_t TASK = 0xf4000001u;
// Whole-run bound for the fixed dry singleton controller: movement contributes
// at most10 writes*3 histories +4 calls*3 histories =42; base HP contributes
// at most6 writes*3 +2 calls*2 +1 combat event =23; two scheduler events add2.
// There are no remote peers/inbox, scripts, custom callbacks, shared draws or
// network error logs in this owned controller. World graph/stamps have no
// append-only event history. Setup and scalar publication use unlogged pokes.
// Thus67 entries/ready tick is covered by96, including retained failed
// prefixes. At512 ticks this leaves ample room for the128-entry movement
// preflight and the existing2048-entry Session network tail. No history is
// cleared here.
static_assert(MAX_CREATURE_READY_TICKS * 96 + 128 < session::MAX_EVENT_HISTORY);
// Recovery adds at most14 body stores (42 shared-log/write/event entries) and
// two calls (four call/event entries). Together with the legacy67-entry bound,
// 128 entries per ready tick conservatively covers the closed composition.
static_assert(MAX_RECOVERY_READY_TICKS * 128 + 128 <
              session::MAX_EVENT_HISTORY);
// Turning adds <=3 stores *3 histories +1 operation =10. Combined
// movement/damage/recovery <=128 plus10 fits160; retain all histories.
static_assert(MAX_FACING_READY_TICKS * 160 + 2048 + 128 <
              session::MAX_EVENT_HISTORY);
void require(bool condition, const char *message) {
  if (!condition)
    throw Error(message);
}
void validate(const Config &config) {
  require(config.profile == "rta",
          "controls support only explicit RTA profile");
  require(config.model == Fpu::Model::Ps2 ||
              config.model == Fpu::Model::Ps2g1 ||
              config.model == Fpu::Model::Chop ||
              config.model == Fpu::Model::Nearest,
          "unknown control arithmetic model");
  require(config.movement == MovementPolicy::Isolated ||
              config.movement == MovementPolicy::DryCreature,
          "unknown control movement policy");
  if (config.play_window) {
    require(
        config.play_window <= play_session::MAX_WINDOW &&
            config.movement == MovementPolicy::DryCreature &&
            !config.recovery && !config.facing && !config.animation_cache &&
            config.max_ready_ticks == MAX_CREATURE_READY_TICKS,
        "play requires dry singleton, window 1..128, no optional diagnostics");
  }
  if (config.animation_cache) {
    validate_control_animation(*config.animation_cache);
    require(config.max_ready_ticks <= MAX_ANIM_READY_TICKS,
            "animation policy requires at most 128 ready ticks");
  }
  if (config.facing) {
    require(config.movement == MovementPolicy::DryCreature &&
                config.facing->policy == FacingPolicy::DirectionalTarget,
            "facing requires explicit DryCreature directional policy");
    require(config.facing->mode == 0 || config.facing->mode == 1,
            "facing mode must be ordinary or opposite");
    require(config.max_ready_ticks <= MAX_FACING_READY_TICKS,
            "facing ready-tick limit must be between one and 256");
  }
  if (config.recovery) {
    session_recovery::validate(*config.recovery);
    require(config.max_ready_ticks <= MAX_RECOVERY_READY_TICKS,
            "recovery ready-tick limit must be between one and 256");
  }
  const auto limit = config.movement == MovementPolicy::DryCreature
                         ? MAX_CREATURE_READY_TICKS
                         : MAX_READY_TICKS;
  require(config.max_ready_ticks > 0 && config.max_ready_ticks <= limit,
          config.movement == MovementPolicy::DryCreature
              ? "dry creature ready-tick limit must be between one and 512"
              : "control ready-tick limit must be between one and 2000");
}
session::Config session_config(const Config &config) {
  session::Config out;
  out.profile = config.profile;
  out.model = config.model;
  out.mul_bias = config.mul_bias;
  out.div_nearest = config.div_nearest;
  out.peers = 1;
  out.local = 0;
  out.network_mode = 1;
  out.network_seed = config.shared_seed;
  return out;
}
world_queries::LevelData default_terrain() {
  world_queries::LevelData data;
  data.tg_x0 = data.tg_y0 = -2;
  data.tg_w = data.tg_h = 4;
  data.grid.assign(16, {0});
  world_queries::HeightField field;
  field.x0 = field.y0 = -64;
  field.w = field.h = 129;
  field.samples.assign(129 * 129, 0);
  data.heightfields.push_back(std::move(field));
  data.elements.push_back(world_queries::make_element(0, -256, -256, 256, 256,
                                                      0, 0, 0, 0, 0, 32767, 0));
  return data;
}
Raw axis(std::uint32_t held, std::uint32_t positive, std::uint32_t negative) {
  const auto p = (held & positive) != 0, n = (held & negative) != 0;
  if (p == n)
    return 0;
  return p ? SPEED : SPEED | 0x80000000u;
}
} // namespace
struct Controller::State {
  Config config;
  play_session::History history;
  std::uint64_t next_animation_command = 1;
  std::vector<AnimationCommand> animation_commands;
  animation_runtime::TrackId selected_animation_track = 0;
  world_queries::Snapshot initial_terrain;
  bool supplied_terrain = false;
  std::unique_ptr<session::Session> session;
  std::uint64_t actor_serial = 0;
  std::uint32_t actor_address = 0;
  session::MoverId mover = 0;
  session::HpId hp = 0;
  std::uint32_t held_input = 0;
  std::vector<std::int64_t> pending_damage_frames;
  std::int64_t minimum_input_frame = 1;
  std::size_t submissions = 0, ready_ticks = 0, damage_events = 0;
  bool paused = false, stopped = false, failed = false, busy = false;
  std::string phase = "idle", error;

  State(Config cfg, const world_queries::LevelData &data, bool supplied,
        world_queries::Limits limits)
      : config(std::move(cfg)), supplied_terrain(supplied) {
    validate(config);
    require(!config.start || supplied,
            "a client start position requires supplied terrain");
    if (config.start)
      for (const auto word : *config.start)
        require((word & 0x7f800000u) != 0x7f800000u,
                "client start position is not finite");
    initial_terrain = world_queries::Level(data, limits).snapshot();
    session = std::make_unique<session::Session>(session_config(config));
    session::ObjectBindings bindings;
    // The actor is deliberately the published base GameObject, with no claimed
    // derived gameplay callback. All movement is the explicit task below.
    bindings.policy = objects::CallbackPolicy::ReferenceBase;
    session->enable_objects(config.shared_seed, std::move(bindings));
    session->with_world([&](objects::World &world) {
      auto *actor =
          world.new_object(config.recovery ? 0x500 : 0x400,
                           config.start ? *config.start
                                        : objects::Vec3{0, 0, 0});
      require(actor != nullptr, "control actor allocation failed");
      if (config.recovery && config.recovery->copy_at_setup)
        actor->flags |= 0x20000;
      actor->radius = RADIUS;
      actor->height = 0x41000000u;
      if (config.movement == MovementPolicy::DryCreature)
        actor->flags |= 0x20u;
      actor_serial = actor->serial;
      actor_address = actor->addr;
      world.run_every_frame(*actor);
    });
    session->enable_hp();
    const bool creature = config.movement == MovementPolicy::DryCreature;
    hp = session->bind_hp(
        actor_serial, actor_address,
        {config.recovery ? config.recovery->health : INITIAL_HP,
         creature ? 0x100000u : 0u,
         {}});
    session->enable_terrain(initial_terrain.data, limits);
    if (creature) {
      session->enable_creature_movement();
      session::CreatureInitial initial;
      initial.base_table = CREATURE_BASE_TABLE;
      session->bind_creature_mover(hp, initial);
    } else {
      mover = session->bind_isolated_mover(actor_serial, actor_address);
    }
    if (config.facing) {
      session->enable_turning({1, 1});
      session->bind_turner(hp, config.facing->initial);
    }
    if (config.recovery)
      session->enable_recovery(hp, *config.recovery);
    if (config.animation_cache) {
      const auto &a = *config.animation_cache;
      session->enable_animation_cache(a.slots, control_animation_limits(a));
      for (const auto &clip : a.clips)
        session->add_animation_cache_clip(clip);
      session->bind_animation_cache(hp, a.capacity);
      animation_commands.reserve(MAX_ANIM_COMMANDS);
    }
    session->register_object_task(
        TASK, "authored_control", 20, 0,
        [this](session::Session &running) { ready(running); });
  }
  void ready(session::Session &running) {
    phase = "task";
    require(ready_ticks < (config.play_window ? play_session::MAX_TICKS
                                              : config.max_ready_ticks),
            "control ready-tick budget exhausted");
    ++ready_ticks;
    std::int64_t frame = 0;
    std::uint32_t held = 0;
    running.with_world([&](objects::World &world) {
      frame = world.frame;
      held = std::uint32_t(world.pads[0].held);
    });
    if (config.animation_cache) {
      // Reserve the whole scheduled command prefix plus advance/decode before
      // any task mutation. Runtime operational refusals still retain prefixes.
      const auto operations = running.control_animation_operations();
      std::size_t work = 2;
      for (const auto &c : animation_commands)
        if (!c.consumed && c.frame == frame)
          work += c.kind == AnimationCommandKind::Restart ? 2 : 1;
      require(work <= config.animation_cache->calls_per_tick &&
                  work <= config.animation_cache->history - operations,
              "control animation task operation/history budget");
    }
    // Releasing presentation input never rewrites earlier wire records. Old
    // records can still become ready, but are neutral under this authored
    // epoch.
    if (frame < minimum_input_frame)
      held = 0;
    const world_queries::Vec3 velocity{axis(held, POS_X, NEG_X),
                                       axis(held, POS_Y, NEG_Y), 0};
    // Authored host target: current XY plus existing axis velocity, using
    // native FPU arithmetic. No normalization or original player mapping.
    if (config.facing && (velocity[0] || velocity[1])) {
      const auto actor = running.observe_actor(hp);
      const creature_movement::Vec3 target{add(actor.position[0], velocity[0]),
                                           add(actor.position[1], velocity[1]),
                                           actor.position[2]};
      running.turn_toward(hp, config.facing->mode, target);
    }
    if (config.movement == MovementPolicy::DryCreature)
      running.move_creature(hp, velocity);
    else
      running.move_isolated(mover, velocity);
    bool damage = false;
    auto end = pending_damage_frames.begin();
    while (end != pending_damage_frames.end() && *end <= frame) {
      if (*end == frame && frame >= minimum_input_frame && (held & DAMAGE))
        damage = true;
      ++end;
    }
    // Consumption precedes the effect: a failed HP operation is retained and
    // stopped, never replayed automatically.
    pending_damage_frames.erase(pending_damage_frames.begin(), end);
    if (damage) {
      running.take_damage(hp, DAMAGE_AMOUNT);
      ++damage_events;
    }
    if (config.recovery)
      running.recover_player(hp);
    if (config.animation_cache) {
      for (auto &c : animation_commands) {
        if (c.consumed || c.frame != frame)
          continue;
        c.consumed = true; // No automatic retry of an accepted failure.
        if (c.kind != AnimationCommandKind::Start)
          running.stop_animation_cache(hp, c.clip, ZERO);
        if (c.kind != AnimationCommandKind::Stop)
          running.start_animation_cache(hp, c.clip, ZERO,
                                        animation_runtime::LOOP |
                                            animation_runtime::KEEP);
        c.completed = true;
      }
      running.advance_animation_cache(hp, ONE);
      selected_animation_track = running.control_animation_track(hp);
      if (selected_animation_track)
        running.decode_animation_cache(hp, selected_animation_track);
    }
    phase = "submit";
  }
  void available() const {
    require(!busy, "control operation is not reentrant");
  }
  void release() {
    held_input = 0;
    pending_damage_frames.clear();
    minimum_input_frame = session->network().lif;
  }
};
Controller::Controller(Config config,
                       std::optional<world_queries::LevelData> terrain,
                       world_queries::Limits limits) {
  validate(config);
  ScopedFpu context(Fpu(config.model, config.mul_bias, config.div_nearest));
  const bool supplied = terrain.has_value();
  auto data = terrain ? std::move(*terrain) : default_terrain();
  state_ = std::make_unique<State>(std::move(config), data, supplied, limits);
}
Controller::~Controller() = default;
Observation Controller::advance(std::uint32_t command_mask) {
  auto &state = *state_;
  state.available();
  require((command_mask & ~COMMAND_MASK) == 0, "unknown control command bit");
  if (state.paused || state.stopped)
    return observation();
  state.busy = true;
  state.phase = "submit";
  try {
    if (state.config.play_window &&
        state.ready_ticks - state.history.through_ready ==
            state.config.play_window) {
      // Prior successful frame is idle and observable until this next command.
      const auto counts = state.session->play_retained().records;
      state.session
          ->retire_play_history(); // validates before clearing anything
      for (std::size_t i = 0; i < counts.size(); ++i)
        state.history.retired[i] += counts[i];
      state.history.through_ready = state.ready_ticks;
      ++state.history.retirements;
    }
    require(state.pending_damage_frames.size() < 4,
            "control edge queue budget exhausted");
    const auto frame = state.session->network().lif;
    if ((command_mask & DAMAGE) && !(state.held_input & DAMAGE))
      state.pending_damage_frames.push_back(frame);
    state.held_input = command_mask;
    ++state.submissions;
    const netwire::ByteVector sample{
        std::uint8_t(command_mask), 0, 128, 128, 128, 128, 0, 0};
    const auto result = state.session->submit(sample);
    require(result.steps >= 0 && result.steps <= 1,
            "control submission produced an unexpected frame count");
    if (state.ready_ticks >= (state.config.play_window
                                  ? play_session::MAX_TICKS
                                  : state.config.max_ready_ticks)) {
      state.stopped = true;
      state.error = "ready_tick_limit";
    } else if (!state.config.play_window &&
               state.submissions >= MAX_SUBMISSIONS) {
      state.stopped = true;
      state.error = "submission_limit";
    }
    state.phase = "idle";
  } catch (const std::exception &error) {
    state.failed = state.stopped = true;
    state.error = error.what();
    state.phase = "failed";
  }
  state.busy = false;
  return observation();
}
std::uint64_t Controller::queue_animation(AnimationCommandKind kind,
                                          animation_runtime::ClipId clip,
                                          std::int64_t frame) {
  auto &s = *state_;
  s.available();
  require(bool(s.config.animation_cache) && !s.stopped && !s.failed,
          "animation command requires live opted-in Controller");
  require(unsigned(kind) <= unsigned(AnimationCommandKind::Restart) && clip &&
              clip <= s.config.animation_cache->clips.size(),
          "animation command kind/clip domain");
  if (!frame)
    frame = s.session->network().lif;
  require(frame >= s.session->network().lif &&
              frame <= std::int64_t(MAX_SUBMISSIONS),
          "animation command frame domain");
  require(s.animation_commands.size() < MAX_ANIM_COMMANDS,
          "animation command history budget");
  std::size_t count = 0,
              work = 2 + (kind == AnimationCommandKind::Restart ? 2 : 1);
  for (const auto &c : s.animation_commands) {
    if (c.frame == frame) {
      ++count;
      work += c.kind == AnimationCommandKind::Restart ? 2 : 1;
    }
  }
  require(count < MAX_ANIM_COMMANDS_PER_FRAME &&
              work <= s.config.animation_cache->calls_per_tick,
          "animation command per-frame budget");
  const auto id = s.next_animation_command;
  s.animation_commands.push_back({id, frame, kind, clip, false, false});
  ++s.next_animation_command;
  return id;
}
void Controller::set_paused(bool paused) {
  auto &state = *state_;
  state.available();
  if (state.stopped)
    return;
  if (paused && !state.paused)
    state.release();
  state.paused = paused;
}
void Controller::release_input() {
  auto &state = *state_;
  state.available();
  if (!state.stopped)
    state.release();
}
void Controller::stop() {
  auto &state = *state_;
  state.available();
  if (state.stopped)
    return;
  state.release();
  state.stopped = true;
  state.error = "stopped";
}
void Controller::reset() {
  const auto &state = *state_;
  state.available();
  ScopedFpu context(
      Fpu(state.config.model, state.config.mul_bias, state.config.div_nearest));
  auto next = std::make_unique<State>(state.config, state.initial_terrain.data,
                                      state.supplied_terrain,
                                      state.initial_terrain.limits);
  state_ = std::move(next);
}
std::uint32_t Controller::shared_seed() const {
  std::uint32_t seed = 0;
  state_->session->with_world([&](objects::World &world) { seed = world.shared.state; });
  return seed;
}
Observation Controller::observation() const {
  const auto &state = *state_;
  auto out = state.session->control_observation(state.actor_serial, state.hp);
  out.submissions = state.submissions;
  out.ready_ticks = state.ready_ticks;
  out.damage_events = state.damage_events;
  out.paused = state.paused;
  out.stopped = state.stopped;
  out.failed = state.failed || out.failed;
  out.error = state.error;
  out.movement = state.config.movement;
  out.ready_tick_limit = state.config.play_window
                             ? play_session::MAX_TICKS
                             : state.config.max_ready_ticks;
  out.play_window = state.config.play_window;
  out.history = state.history;
  if (state.config.play_window)
    out.retained = state.session->play_retained();
  if (state.config.animation_cache)
    out.animation_track = state.selected_animation_track;
  if (state.config.facing)
    out.turn_mode = state.config.facing->mode;
  return out;
}
Setup Controller::setup() const {
  const auto &state = *state_;
  return {state.config, state.initial_terrain, state.supplied_terrain};
}
Snapshot Controller::snapshot() const {
  require(!state_->config.play_window,
          "play history is windowed; use window_snapshot explicitly");
  return window_snapshot();
}
Snapshot Controller::window_snapshot() const {
  const auto &state = *state_;
  Snapshot out;
  out.history = state.history;
  out.next_animation_command = state.next_animation_command;
  out.animation_commands = state.animation_commands;
  out.selected_animation_track = state.selected_animation_track;
  out.config = state.config;
  out.initial_terrain = state.initial_terrain;
  out.supplied_terrain = state.supplied_terrain;
  out.session = state.session->snapshot();
  out.actor_serial = state.actor_serial;
  out.actor_address = state.actor_address;
  out.mover = state.mover;
  out.hp = state.hp;
  out.held_input = state.held_input;
  out.pending_damage_frames = state.pending_damage_frames;
  out.minimum_input_frame = state.minimum_input_frame;
  out.submissions = state.submissions;
  out.ready_ticks = state.ready_ticks;
  out.damage_events = state.damage_events;
  out.paused = state.paused;
  out.stopped = state.stopped;
  out.failed = state.failed;
  out.busy = state.busy;
  out.phase = state.phase;
  out.error = state.error;
  return out;
}
world_queries::LevelData Controller::terrain_data() const {
  return state_->session->snapshot().terrain->level.data;
}
void Controller::checkpoint(detail::Checkpoint &a) {
  auto &s = *state_;
  using A = detail::Checkpoint;
  A::require(bool(s.config.play_window) == a.play_continuation,
             "checkpoint play policy mismatch");
  if (a.play_continuation) {
    a.fields(s.history.through_ready, s.history.retirements, s.history.retired);
  }
  A::require(!s.busy && !s.failed,
             "checkpoint requires idle healthy Controller");
  a.fields(s.held_input, s.pending_damage_frames, s.minimum_input_frame,
           s.submissions, s.ready_ticks, s.damage_events, s.paused, s.stopped,
           s.phase, s.error);
  A::require(
      s.phase == "idle" &&
          s.submissions <= (a.play_continuation ? play_session::MAX_TICKS + 1
                                                : MAX_SUBMISSIONS) &&
          s.ready_ticks <= (a.play_continuation ? play_session::MAX_TICKS
                                                : s.config.max_ready_ticks) &&
          s.damage_events <= s.ready_ticks && !(s.held_input & ~COMMAND_MASK) &&
          s.pending_damage_frames.size() <= 4,
      "checkpoint Controller counters");
  A::require(!s.config.animation_cache || a.control_anim_cache,
             "legacy Controller checkpoint refuses animation cache");
  if (s.config.animation_cache) {
    a.fields(s.next_animation_command, s.selected_animation_track);
    detail::cache_vector(a, s.animation_commands, MAX_ANIM_COMMANDS);
    A::require(s.next_animation_command == s.animation_commands.size() + 1,
               "checkpoint animation command sequence");
    std::uint64_t id = 0;
    for (const auto &c : s.animation_commands) {
      A::require(c.id == ++id && unsigned(c.kind) <= 2 && c.clip &&
                     c.clip <= s.config.animation_cache->clips.size() &&
                     c.frame >= 1 && c.frame <= std::int64_t(MAX_SUBMISSIONS) &&
                     c.consumed == c.completed,
                 "checkpoint animation command domain");
      std::size_t count = 0, work = 2;
      for (const auto &other : s.animation_commands)
        if (other.frame == c.frame) {
          ++count;
          work += other.kind == AnimationCommandKind::Restart ? 2 : 1;
        }
      A::require(count <= MAX_ANIM_COMMANDS_PER_FRAME &&
                     work <= s.config.animation_cache->calls_per_tick,
                 "checkpoint animation command budget");
    }
  }
  detail::CheckpointAccess::run(*s.session, a);
  if (s.config.animation_cache) {
    const auto view = s.session->snapshot();
    const auto &cache = *view.animation_cache;
    const auto &st = cache.runtime.states.front().value;
    const auto &descriptor = *s.config.animation_cache;
    A expected_clips, actual_clips;
    auto clips = descriptor.clips;
    std::vector<animation_runtime::Clip> current_clips;
    for (const auto &c : cache.runtime.decoder.clips)
      current_clips.push_back(c.value);
    expected_clips.value(clips);
    actual_clips.value(current_clips);
    A::require(expected_clips.bytes == actual_clips.bytes &&
                   st.capacity == descriptor.capacity &&
                   cache.runtime.decoder.tracks.size() ==
                       std::size_t(descriptor.capacity),
               "checkpoint cache immutable clips/cells");
    for (std::size_t i = 0; i < st.tracks.size(); ++i)
      A::require(st.tracks[i] == i + 1,
                 "checkpoint cache fixed cell membership");
    A::require(
        !(st.flags & ~(animation_runtime::LOOP | animation_runtime::KEEP)),
        "checkpoint cache fixed state flags");
    for (const auto &t : cache.runtime.decoder.tracks)
      A::require(!(t.value.flags &
                   ~(animation_runtime::LOOP | animation_runtime::KEEP)) &&
                     t.value.time <= 0x43000000u &&
                     t.value.weight <= 0x43000000u &&
                     (t.value.rate == ZERO || t.value.rate == ONE) &&
                     (t.value.life == ZERO || t.value.life == ONE) &&
                     t.value.life_rate == ZERO && !t.value.sound_idx,
                 "checkpoint cache fixed track arithmetic domain");
    for (std::int32_t i = 0; i < st.count; ++i)
      A::require(cache.runtime.decoder.tracks[st.tracks[std::size_t(i)] - 1]
                         .value.clip != 0,
                 "checkpoint cache live null clip");
    for (const auto &slot : cache.runtime.decoder.slots) {
      A::require(slot.tick >= 0 &&
                     slot.tick <= std::int64_t(MAX_ANIM_READY_TICKS),
                 "checkpoint cache decoded tick domain");
      for (const auto &joint : slot.joints) {
        A::require(
            joint.pt >= 0 && joint.pt <= std::int64_t(MAX_ANIM_READY_TICKS) &&
                joint.rt >= 0 && joint.rt <= std::int64_t(MAX_ANIM_READY_TICKS),
            "checkpoint cache joint tick domain");
        auto finite = [&](const auto &values) {
          for (auto raw : values)
            A::require((raw & 0x7f800000u) != 0x7f800000u,
                       "checkpoint cache nonfinite joint value");
        };
        finite(joint.pos);
        finite(joint.vel);
        finite(joint.quat);
        finite(joint.qvel);
      }
    }
    A::require(view.world && view.world->frame == std::int64_t(s.ready_ticks) &&
                   view.network.sim_frame == view.world->frame &&
                   st.flags == (st.count ? animation_runtime::LOOP |
                                               animation_runtime::KEEP
                                         : 0),
               "checkpoint cache ready frame/state flags");
    for (std::int32_t i = 0; i < st.count; ++i) {
      const auto &track = cache.runtime.decoder.tracks[std::size_t(i)].value;
      A::require(track.life == ONE && track.rate == ONE &&
                     track.flags ==
                         (animation_runtime::LOOP | animation_runtime::KEEP) &&
                     track.weight != ZERO,
                 "checkpoint cache live lifecycle scalars");
    }
    if (!a.deferred()) {
      // Inspect the declared ID topology only. Do not invoke lifecycle,
      // arithmetic, decoder or gameplay bodies to reconstruct current state.
      std::vector<animation_runtime::ClipId> live_clips;
      std::size_t op_at = 0;
      for (std::size_t tick = 1; tick <= s.ready_ticks; ++tick) {
        // Closed one-peer Controller executes frames in order, one per task.
        auto consume = [&](const char *kind, animation_runtime::ClipId clip) {
          A::require(op_at < cache.operations.size(),
                     "checkpoint missing cache operation");
          const auto &op = cache.operations[op_at++];
          A::require(op.kind == kind && op.clip == clip && op.frame == tick,
                     "checkpoint cache command/operation composition");
          if (op.kind == "start_animation_cache") {
            const auto index = std::min(live_clips.size(),
                                        std::size_t(descriptor.capacity - 1));
            A::require(op.track == st.tracks[index],
                       "checkpoint cache start cell identity");
            if (live_clips.size() < std::size_t(descriptor.capacity))
              live_clips.push_back(clip);
            else
              live_clips.back() = clip;
          } else if (op.kind == "stop_animation_cache") {
            live_clips.erase(
                std::remove(live_clips.begin(), live_clips.end(), clip),
                live_clips.end());
          } else if (op.kind == "decode_animation_cache") {
            A::require(
                !live_clips.empty() &&
                    op.track == st.tracks[live_clips.size() - 1] &&
                    op.decoded &&
                    op.decoded->joints.size() ==
                        std::size_t(descriptor.clips[live_clips.back() - 1]
                                        .joint_count),
                "checkpoint cache historical selected cell/sample shape");
          }
        };
        for (const auto &c : s.animation_commands)
          if (c.consumed && c.frame == std::int64_t(tick)) {
            if (c.kind != AnimationCommandKind::Start)
              consume("stop_animation_cache", c.clip);
            if (c.kind != AnimationCommandKind::Stop)
              consume("start_animation_cache", c.clip);
          }
        consume("advance_animation_cache", 0);
        if (!live_clips.empty())
          consume("decode_animation_cache", 0);
      }
      A::require(op_at == cache.operations.size() &&
                     live_clips.size() == std::size_t(st.count),
                 "checkpoint extra cache operations/current count");
      for (std::size_t i = 0; i < live_clips.size(); ++i)
        A::require(cache.runtime.decoder.tracks[i].value.clip == live_clips[i],
                   "checkpoint cache current clip identity/history mismatch");
    }
    A::require(s.selected_animation_track ==
                   (st.count ? st.tracks[std::size_t(st.count - 1)] : 0),
               "checkpoint selected animation track");
    for (const auto &c : s.animation_commands)
      A::require(c.consumed ? c.frame < view.network.net_frame
                            : c.frame >= view.network.net_frame,
                 "checkpoint animation command ready-frame ownership");
  }
  if (s.config.facing) {
    const auto view = s.session->snapshot();
    A::require(view.turning && view.turning->operations.size() <= s.ready_ticks,
               "checkpoint facing operation count");
    for (const auto &op : view.turning->operations)
      A::require(op.mode == s.config.facing->mode,
                 "checkpoint facing operation policy");
  }
  const auto n = s.session->network();
  if (a.play_continuation) {
    A::require(s.history.through_ready <= s.ready_ticks &&
                   s.ready_ticks - s.history.through_ready <=
                       s.config.play_window &&
                   s.history.through_ready % s.config.play_window == 0 &&
                   s.history.retirements ==
                       s.history.through_ready / s.config.play_window &&
                   n.lif == std::int64_t(s.submissions) + 1 &&
                   n.net_frame == std::int64_t(s.ready_ticks) + 1 &&
                   n.sim_frame == std::int64_t(s.ready_ticks) &&
                   s.submissions == s.ready_ticks + (s.submissions ? 1 : 0),
               "play checkpoint absolute counters");
    std::uint64_t total = 0;
    for (auto count : s.history.retired) {
      A::require(count <= s.history.through_ready * 96,
                 "play checkpoint retired stream count");
      total += count;
    }
    A::require(total <= s.history.through_ready * 96,
               "play checkpoint retired aggregate count");
    total = 0;
    for (auto count : s.session->play_retained().records)
      total += count;
    A::require(total <= (s.ready_ticks - s.history.through_ready) * 96,
               "play checkpoint retained history budget");
  }
  if (s.config.animation_cache)
    A::require(n.lif == std::int64_t(s.submissions) + 1 &&
                   n.net_frame == std::int64_t(s.ready_ticks) + 1,
               "checkpoint animation Controller/network counters");
  A::require(s.minimum_input_frame >= 1 && s.minimum_input_frame <= n.lif,
             "checkpoint input cutoff");
  std::int64_t previous = 0;
  for (auto f : s.pending_damage_frames) {
    A::require(f > previous && f >= n.net_frame && f < n.lif,
               "checkpoint pending input frame");
    previous = f;
  }
}
} // namespace champions::simulation::rta::session_control
