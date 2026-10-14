#include "champions/detail/checkpoint.hpp"
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
    initial_terrain = world_queries::Level(data, limits).snapshot();
    session = std::make_unique<session::Session>(session_config(config));
    session::ObjectBindings bindings;
    // The actor is deliberately the published base GameObject, with no claimed
    // derived gameplay callback. All movement is the explicit task below.
    bindings.policy = objects::CallbackPolicy::ReferenceBase;
    session->enable_objects(config.shared_seed, std::move(bindings));
    session->with_world([&](objects::World &world) {
      auto *actor =
          world.new_object(config.recovery ? 0x500 : 0x400, {0, 0, 0});
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
    if (config.recovery)
      session->enable_recovery(hp, *config.recovery);
    session->register_object_task(
        TASK, "authored_control", 20, 0,
        [this](session::Session &running) { ready(running); });
  }
  void ready(session::Session &running) {
    phase = "task";
    require(ready_ticks < config.max_ready_ticks,
            "control ready-tick budget exhausted");
    ++ready_ticks;
    std::int64_t frame = 0;
    std::uint32_t held = 0;
    running.with_world([&](objects::World &world) {
      frame = world.frame;
      held = std::uint32_t(world.pads[0].held);
    });
    // Releasing presentation input never rewrites earlier wire records. Old
    // records can still become ready, but are neutral under this authored
    // epoch.
    if (frame < minimum_input_frame)
      held = 0;
    const world_queries::Vec3 velocity{axis(held, POS_X, NEG_X),
                                       axis(held, POS_Y, NEG_Y), 0};
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
    if (state.ready_ticks >= state.config.max_ready_ticks) {
      state.stopped = true;
      state.error = "ready_tick_limit";
    } else if (state.submissions >= MAX_SUBMISSIONS) {
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
  out.ready_tick_limit = state.config.max_ready_ticks;
  return out;
}
Setup Controller::setup() const {
  const auto &state = *state_;
  return {state.config, state.initial_terrain, state.supplied_terrain};
}
Snapshot Controller::snapshot() const {
  const auto &state = *state_;
  Snapshot out;
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
  A::require(!s.busy && !s.failed,
             "checkpoint requires idle healthy Controller");
  a.fields(s.held_input, s.pending_damage_frames, s.minimum_input_frame,
           s.submissions, s.ready_ticks, s.damage_events, s.paused, s.stopped,
           s.phase, s.error);
  A::require(s.phase == "idle" && s.submissions <= MAX_SUBMISSIONS &&
                 s.ready_ticks <= s.config.max_ready_ticks &&
                 s.damage_events <= s.ready_ticks &&
                 !(s.held_input & ~COMMAND_MASK) &&
                 s.pending_damage_frames.size() <= 4,
             "checkpoint Controller counters");
  detail::CheckpointAccess::run(*s.session, a);
  const auto n = s.session->network();
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
