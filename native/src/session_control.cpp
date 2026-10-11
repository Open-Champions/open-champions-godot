// Authored diagnostic control policy over the already pinned Session, object,
// terrain and HP contracts. This is not a port of original Player_move.
#include "champions/session_control.hpp"
#include <algorithm>
#include <utility>

namespace champions::simulation::rta::session_control {
namespace {
constexpr std::uint32_t TASK = 0xf4000001u;
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
  require(config.max_ready_ticks > 0 &&
              config.max_ready_ticks <= MAX_READY_TICKS,
          "control ready-tick limit must be between one and 2000");
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
      auto *actor = world.new_object(0x400, {0, 0, 0});
      require(actor != nullptr, "control actor allocation failed");
      actor->radius = RADIUS;
      actor->height = 0x41000000u;
      actor_serial = actor->serial;
      actor_address = actor->addr;
      world.run_every_frame(*actor);
    });
    session->enable_hp();
    hp = session->bind_hp(actor_serial, actor_address, {INITIAL_HP, 0, {}});
    session->enable_terrain(initial_terrain.data, limits);
    mover = session->bind_isolated_mover(actor_serial, actor_address);
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
  const auto full = state.session->snapshot();
  require(full.world && full.hp, "control owned state is unavailable");
  const auto actor = std::find_if(
      full.world->objects.begin(), full.world->objects.end(),
      [&](const auto &value) { return value.serial == state.actor_serial; });
  require(actor != full.world->objects.end() &&
              full.hp->adapter.objects.size() == 1,
          "control actor observation is unavailable");
  Observation out;
  out.position = actor->pos;
  out.vz = actor->vz;
  out.radius = actor->radius;
  out.hp = full.hp->adapter.objects[0].value.hp;
  out.frame = full.world->frame;
  out.submissions = state.submissions;
  out.ready_ticks = state.ready_ticks;
  out.damage_events = state.damage_events;
  out.paused = state.paused;
  out.stopped = state.stopped;
  out.failed = state.failed || full.failed;
  out.error = state.error;
  return out;
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
} // namespace champions::simulation::rta::session_control
