#include "champions/detail/checkpoint_control_anim_cache.hpp"
// Handwritten little-endian authored native checkpoint. No struct dumps,
// console data, host pointers, callbacks or host floating-point conversion.
#include "champions/native_save.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace champions::simulation::rta::native_save {
namespace {
namespace C = session_control;
namespace W = world_queries;
constexpr std::array<std::uint8_t, 8> MAGIC{'O', 'C', 'N', 'S',
                                            'A', 'V', 'E', 0};
constexpr std::size_t HEADER = 24;
constexpr std::array<std::uint8_t, 8> PLAY_MAGIC{'O', 'C', 'P', 'L',
                                                 'A', 'Y', 0,   0};
constexpr std::uint32_t PLAY_VERSION = 1;
void require(bool value, const char *message) {
  if (!value)
    throw Error(message);
}
std::uint32_t crc(const Bytes &bytes, bool header_zero = true) {
  std::uint32_t result = 0xffffffffu;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    // The checksum field itself is treated as four zero bytes.
    result ^= header_zero && i >= 20 && i < 24 ? 0 : bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      result = (result >> 1) ^ (0xedb88320u & (0u - (result & 1u)));
  }
  return ~result;
}
struct Writer {
  Bytes bytes;
  void u(std::uint64_t value, unsigned width) {
    require(width <= MAX_BYTES - bytes.size(), "save exceeds byte budget");
    for (unsigned i = 0; i < width; ++i) {
      bytes.push_back(std::uint8_t(value));
      value >>= 8;
    }
  }
  void count(std::size_t value) {
    require(value <= UINT32_MAX, "save count exceeds u32");
    u(value, 4);
  }
};
struct Reader {
  const Bytes &bytes;
  std::size_t at = HEADER;
  std::uint64_t u(unsigned width) {
    require(width <= bytes.size() - at, "truncated native save");
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint64_t(bytes[at++]) << (8 * i);
    return value;
  }
  std::int64_t s(unsigned width) {
    const auto value = u(width);
    const auto sign = std::uint64_t(1) << (width * 8 - 1);
    if (!(value & sign))
      return std::int64_t(value);
    const auto mask = width == 8 ? UINT64_MAX : (sign * 2 - 1);
    return -1 - std::int64_t((~value) & mask);
  }
  bool boolean() {
    const auto v = u(1);
    require(v <= 1, "invalid save boolean");
    return v != 0;
  }
  std::size_t count(std::size_t cap, std::size_t stride) {
    const auto n = u(4);
    require(n <= cap && n <= (bytes.size() - at) / stride,
            "save collection exceeds bounds or remaining bytes");
    return std::size_t(n);
  }
};
void patch(Bytes &bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) {
    bytes[at + i] = std::uint8_t(value);
    value >>= 8;
  }
}
std::uint32_t field(const Bytes &bytes, std::size_t at) {
  std::uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i)
    value |= std::uint32_t(bytes[at + i]) << (8 * i);
  return value;
}
void limits(Writer &w, const W::Limits &l) {
  for (auto v : {l.heightfields, l.elements, l.grid_cells, l.samples,
                 l.references, l.path_cells, l.query_work})
    w.count(v);
}
W::Limits limits(Reader &r) {
  W::Limits l, hard;
  std::array<std::size_t *, 7> fields{
      &l.heightfields, &l.elements,   &l.grid_cells, &l.samples,
      &l.references,   &l.path_cells, &l.query_work};
  const std::array<std::size_t, 7> caps{
      hard.heightfields, hard.elements,   hard.grid_cells, hard.samples,
      hard.references,   hard.path_cells, hard.query_work};
  for (std::size_t i = 0; i < fields.size(); ++i) {
    auto v = r.u(4);
    require(v <= caps[i], "terrain limit exceeds native hard cap");
    *fields[i] = std::size_t(v);
  }
  return l;
}
void terrain(Writer &w, const W::LevelData &d) {
  w.u(d.stamp, 4);
  for (auto v : {d.tg_x0, d.tg_y0, d.tg_w, d.tg_h})
    w.u(std::uint64_t(v), 8);
  w.count(d.grid.size());
  for (const auto &cell : d.grid) {
    w.count(cell.size());
    for (auto v : cell)
      w.u(std::uint64_t(v), 8);
  }
  w.count(d.elements.size());
  for (const auto &e : d.elements) {
    for (auto v : {e.index, e.min_x, e.min_y, e.max_x, e.max_y, e.hf, e.flags,
                   e.x, e.y, e.z, e.cos, e.sin})
      w.u(std::uint64_t(v), 8);
    w.u(e.stamp, 4);
    for (auto v : {e.f_min_x, e.f_min_y, e.f_max_x, e.f_max_y})
      w.u(v, 4);
  }
  w.count(d.heightfields.size());
  for (const auto &h : d.heightfields) {
    for (auto v : {h.x0, h.y0, h.w, h.h, h.lo, h.hi, h.offset})
      w.u(std::uint64_t(v), 8);
    w.count(h.samples.size());
    for (auto v : h.samples)
      w.u(std::uint16_t(v), 2);
  }
  w.count(d.pieces.size());
  for (auto v : d.pieces)
    w.u(std::uint64_t(v), 8);
  for (auto v : {d.pg_x0, d.pg_y0, d.pg_w, d.pg_h})
    w.u(std::uint64_t(v), 8);
  w.count(d.path_cells.size());
  for (auto v : d.path_cells)
    w.u(v, 4);
}
W::LevelData terrain(Reader &r, const W::Limits &l) {
  W::LevelData d;
  d.stamp = std::uint32_t(r.u(4));
  d.tg_x0 = r.s(8);
  d.tg_y0 = r.s(8);
  d.tg_w = r.s(8);
  d.tg_h = r.s(8);
  d.grid.resize(r.count(l.grid_cells, 4));
  std::size_t refs = 0, samples = 0;
  for (auto &cell : d.grid) {
    cell.resize(r.count(l.references - refs, 8));
    refs += cell.size();
    for (auto &v : cell)
      v = r.s(8);
  }
  d.elements.resize(r.count(l.elements, 116));
  for (auto &e : d.elements) {
    e.index = r.s(8);
    e.min_x = r.s(8);
    e.min_y = r.s(8);
    e.max_x = r.s(8);
    e.max_y = r.s(8);
    e.hf = r.s(8);
    e.flags = r.s(8);
    e.x = r.s(8);
    e.y = r.s(8);
    e.z = r.s(8);
    e.cos = r.s(8);
    e.sin = r.s(8);
    e.stamp = std::uint32_t(r.u(4));
    e.f_min_x = Raw(r.u(4));
    e.f_min_y = Raw(r.u(4));
    e.f_max_x = Raw(r.u(4));
    e.f_max_y = Raw(r.u(4));
  }
  d.heightfields.resize(r.count(l.heightfields, 60));
  for (auto &h : d.heightfields) {
    h.x0 = r.s(8);
    h.y0 = r.s(8);
    h.w = r.s(8);
    h.h = r.s(8);
    h.lo = r.s(8);
    h.hi = r.s(8);
    h.offset = r.s(8);
    h.samples.resize(r.count(l.samples - samples, 2));
    samples += h.samples.size();
    for (auto &v : h.samples)
      v = std::int16_t(r.s(2));
  }
  d.pieces.resize(r.count(l.references - refs, 8));
  for (auto &v : d.pieces)
    v = r.s(8);
  d.pg_x0 = r.s(8);
  d.pg_y0 = r.s(8);
  d.pg_w = r.s(8);
  d.pg_h = r.s(8);
  d.path_cells.resize(r.count(l.path_cells, 4));
  for (auto &v : d.path_cells)
    v = std::uint32_t(r.u(4));
  // Existing native structural checks also validate dimensions and identities.
  try {
    (void)W::Level(d, l);
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
  return d;
}
} // namespace
namespace terrain_codec {
void encode_terrain(Bytes &out, const W::Snapshot &t) {
  Writer w;
  w.bytes = std::move(out);
  limits(w, t.limits);
  terrain(w, t.data);
  out = std::move(w.bytes);
}
W::Snapshot decode_terrain(const Bytes &bytes, std::size_t &at) {
  Reader r{bytes};
  r.at = at;
  W::Snapshot t;
  t.limits = limits(r);
  t.data = terrain(r, t.limits);
  at = r.at;
  return t;
}
} // namespace terrain_codec
namespace {
// Redundant continuation witness detects incompatible replay results. Complete
// authoritative histories are reconstructed, not discarded or summarized.
void witness(Writer &w, const C::Observation &o) {
  for (auto v : o.position)
    w.u(v, 4);
  w.u(o.vz, 4);
  w.u(o.radius, 4);
  w.u(o.hp, 4);
  w.u(std::uint64_t(o.frame), 8);
  w.count(o.submissions);
  w.count(o.ready_ticks);
  w.count(o.damage_events);
  w.u(o.paused, 1);
  w.u(o.stopped, 1);
  w.u(o.failed, 1);
  w.count(o.error.size());
  for (unsigned char v : o.error)
    w.u(v, 1);
  if (o.animation_cache_enabled) {
    detail::Checkpoint cache;
    auto track = o.animation_track;
    auto event = o.animation_event;
    auto decoded = o.animation_decoded;
    cache.fields(track, event, decoded);
    w.count(cache.bytes.size());
    for (auto v : cache.bytes)
      w.u(v, 1);
  }
  if (o.facing_enabled) {
    w.u(o.facing, 4);
    w.u(o.angular_velocity, 2);
  }
  if (o.recovery_enabled) {
    w.u(o.mana, 4);
    w.u(o.pending_health, 4);
    w.u(o.pending_mana, 4);
  }
}
} // namespace
Scene::Scene(C::Config config, std::optional<W::LevelData> data, W::Limits l)
    : controller_(std::make_unique<C::Controller>(config, data, l)),
      config_(std::move(config)), terrain_(std::move(data)), limits_(l) {
  if (config_.play_window) {
    require(save_continuation().size() <= MAX_BYTES - 4u * 1024u * 1024u,
            "play setup leaves insufficient bounded history budget");
    return;
  }
  // Enforce the serialization budget before exposing a live scene.
  const auto journal_reserve =
      config_.animation_cache ? 10 * MAX_ACTIONS + 4096 : 2 * MAX_ACTIONS + 128;
  require(save().size() <= MAX_BYTES - journal_reserve,
          "initial terrain leaves insufficient action budget");
  actions_.reserve(MAX_ACTIONS);
}
void Scene::reserve_action() {
  require(retained_actions() < MAX_ACTIONS,
          "native save action budget exhausted");
}
C::Observation Scene::advance(std::uint32_t command) {
  require((command & ~C::COMMAND_MASK) == 0, "unknown control command bit");
  if (config_.play_window)
    return controller_->advance(command);
  prepare_mutation();
  reserve_action();
  // Capacity and command validation precede mutation. Keep the input even if
  // copying the returned observation throws after a completed submission.
  actions_.push_back({0, std::uint8_t(command)});
  return controller_->advance(command);
}
std::uint64_t Scene::queue_animation(C::AnimationCommandKind kind,
                                     animation_runtime::ClipId clip,
                                     std::int64_t frame) {
  prepare_mutation();
  reserve_action();
  if (!frame)
    frame = controller_->snapshot().session.network.lif;
  const auto id = controller_->queue_animation(kind, clip, frame);
  actions_.push_back(
      {std::uint8_t(4 + unsigned(kind)), std::uint8_t(clip), frame});
  return id;
}
void Scene::set_paused(bool value) {
  if (config_.play_window) {
    controller_->set_paused(value);
    return;
  }
  prepare_mutation();
  reserve_action();
  controller_->set_paused(value);
  actions_.push_back({1, std::uint8_t(value)});
}
void Scene::release_input() {
  if (config_.play_window) {
    controller_->release_input();
    return;
  }
  prepare_mutation();
  reserve_action();
  controller_->release_input();
  actions_.push_back({2, 0});
}
void Scene::stop() {
  if (config_.play_window) {
    controller_->stop();
    return;
  }
  prepare_mutation();
  reserve_action();
  controller_->stop();
  actions_.push_back({3, 0});
}
void Scene::reset() {
  controller_->reset();
  actions_.clear();
  deferred_.reset();
}
C::Observation Scene::observation() const { return controller_->observation(); }
std::uint32_t Scene::shared_seed() const { return controller_->shared_seed(); }
C::Setup Scene::setup() const { return controller_->setup(); }
C::Snapshot Scene::snapshot() const {
  materialize();
  return controller_->snapshot();
}
C::Snapshot Scene::continuation_snapshot() const {
  require(config_.play_window != 0,
          "continuation observation requires play mode");
  return controller_->window_snapshot();
}
Bytes Scene::save_continuation() const {
  require(config_.play_window != 0,
          "continuation save requires explicit play mode");
  auto diagnostic = config_;
  diagnostic.play_window = 0;
  Scene initial(diagnostic, terrain_, limits_);
  const auto setup = initial.save();
  detail::Checkpoint state;
  state.play_continuation = true;
  detail::CheckpointAccess::run(*controller_, state);
  Writer w;
  for (auto b : PLAY_MAGIC)
    w.u(b, 1);
  w.u(PLAY_VERSION, 4);
  w.u(RTA_PROFILE, 4);
  w.u(0, 4);
  w.u(0, 4);
  w.count(config_.play_window);
  w.count(setup.size());
  for (auto b : setup)
    w.u(b, 1);
  w.count(state.bytes.size());
  for (auto b : state.bytes)
    w.u(b, 1);
  patch(w.bytes, 16, std::uint32_t(w.bytes.size()));
  patch(w.bytes, 20, crc(w.bytes));
  return std::move(w.bytes);
}
std::unique_ptr<Scene> Scene::load_continuation(const Bytes &bytes,
                                                const std::string &profile) {
  require(profile == "rta" && bytes.size() >= HEADER &&
              bytes.size() <= MAX_BYTES,
          "play continuation profile/size");
  require(field(bytes, 8) == PLAY_VERSION && field(bytes, 12) == RTA_PROFILE &&
              field(bytes, 16) == bytes.size() &&
              field(bytes, 20) == crc(bytes),
          "play continuation version/integrity");
  Reader r{bytes};
  const auto window = r.u(4);
  require(window > 0 && window <= play_session::MAX_WINDOW,
          "play continuation window");
  auto block = [&]() {
    const auto n = r.count(MAX_BYTES, 1);
    Bytes result(bytes.begin() + r.at, bytes.begin() + r.at + n);
    r.at += n;
    return result;
  };
  const auto setup = block();
  const auto state = block();
  require(r.at == bytes.size(), "play continuation trailing bytes");
  try {
    auto initial = load_replay(setup, profile, false);
    require(initial->actions_.empty() && initial->save() == setup,
            "play continuation requires canonical empty setup journal");
    auto config = initial->config_;
    config.play_window = std::size_t(window);
    auto candidate =
        std::make_unique<Scene>(config, initial->terrain_, initial->limits_);
    detail::Checkpoint input(state);
    input.play_continuation = true;
    detail::CheckpointAccess::run(*candidate->controller_, input);
    input.finish();
    require(candidate->save_continuation() == bytes,
            "noncanonical play continuation");
    return candidate;
  } catch (const std::exception &error) {
    throw Error(error.what());
  }
}
Bytes Scene::save() const {
  require(!config_.play_window, "play saves require the continuation format");
  materialize();
  const auto o = observation();
  require(!o.failed,
          "failed operational prefix is not a reloadable checkpoint");
  Writer w;
  for (auto v : MAGIC)
    w.u(v, 1);
  w.u(config_.animation_cache ? CONTROL_ANIM_CACHE_REPLAY_VERSION
      : config_.facing        ? FACING_REPLAY_VERSION
      : config_.recovery      ? RECOVERY_REPLAY_VERSION
                              : VERSION,
      4);
  w.u(RTA_PROFILE, 4);
  w.u(0, 4);
  w.u(0, 4);
  w.u(unsigned(config_.model), 1);
  w.u(config_.mul_bias, 1);
  w.u(config_.div_nearest, 1);
  w.u(unsigned(config_.movement), 1);
  w.u(config_.shared_seed, 4);
  w.count(config_.max_ready_ticks);
  if (config_.animation_cache) {
    detail::Checkpoint descriptor;
    auto cache = *config_.animation_cache;
    auto facing = config_.facing;
    auto recovery = config_.recovery;
    descriptor.fields(cache, facing, recovery);
    w.count(descriptor.bytes.size());
    for (auto v : descriptor.bytes)
      w.u(v, 1);
  } else if (config_.facing) {
    detail::Checkpoint descriptor;
    auto facing = *config_.facing;
    auto recovery = config_.recovery;
    descriptor.fields(facing, recovery);
    w.count(descriptor.bytes.size());
    for (auto v : descriptor.bytes)
      w.u(v, 1);
  } else if (config_.recovery) {
    detail::Checkpoint descriptor;
    auto recovery = *config_.recovery;
    descriptor.value(recovery);
    w.count(descriptor.bytes.size());
    for (auto v : descriptor.bytes)
      w.u(v, 1);
  }
  limits(w, limits_);
  // 0 no terrain, 1 terrain (historical bytes), 2 terrain plus the client
  // start position (session_control::Config::start), whose words follow.
  w.u(terrain_ ? (config_.start ? 2u : 1u) : 0u, 1);
  if (terrain_)
    terrain(w, *terrain_);
  if (config_.start)
    for (const auto word : *config_.start)
      w.u(word, 4);
  w.count(actions_.size());
  for (const auto &a : actions_) {
    w.u(a.kind, 1);
    w.u(a.value, 1);
    if (config_.animation_cache)
      w.u(std::uint64_t(a.frame), 8);
  }
  witness(w, o);
  patch(w.bytes, 16, std::uint32_t(w.bytes.size()));
  patch(w.bytes, 20, crc(w.bytes));
  return std::move(w.bytes);
}
std::unique_ptr<Scene> Scene::load_replay(const Bytes &bytes,
                                          const std::string &profile,
                                          bool replay) {
  require(profile == "rta",
          "native save requires explicit supported RTA profile");
  require(bytes.size() >= HEADER && bytes.size() <= MAX_BYTES,
          "native save size outside bounds");
  require(std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin()),
          "native save magic mismatch");
  require(field(bytes, 8) == VERSION || field(bytes, 8) == 2 ||
              field(bytes, 8) == FACING_REPLAY_VERSION ||
              field(bytes, 8) == CONTROL_ANIM_CACHE_REPLAY_VERSION,
          "unsupported native save version");
  require(field(bytes, 12) == RTA_PROFILE, "native save profile mismatch");
  require(field(bytes, 16) == bytes.size(), "native save length mismatch");
  require(field(bytes, 20) == crc(bytes), "native save checksum mismatch");
  Reader r{bytes};
  C::Config cfg;
  const auto model = r.u(1);
  require(model <= 3, "unknown arithmetic model");
  cfg.model = static_cast<Fpu::Model>(model);
  cfg.mul_bias = r.boolean();
  cfg.div_nearest = r.boolean();
  const auto movement = r.u(1);
  require(movement <= 1, "unknown movement policy");
  cfg.movement = static_cast<C::MovementPolicy>(movement);
  cfg.shared_seed = std::uint32_t(r.u(4));
  cfg.max_ready_ticks = std::size_t(r.u(4));
  if (field(bytes, 8) == CONTROL_ANIM_CACHE_REPLAY_VERSION) {
    const auto size = r.count(131072, 1);
    detail::Checkpoint descriptor(
        Bytes(bytes.begin() + r.at, bytes.begin() + r.at + size));
    r.at += size;
    cfg.animation_cache.emplace();
    descriptor.fields(*cfg.animation_cache, cfg.facing, cfg.recovery);
    descriptor.finish();
  } else if (field(bytes, 8) == FACING_REPLAY_VERSION) {
    const auto size = r.count(4096, 1);
    detail::Checkpoint descriptor(
        Bytes(bytes.begin() + r.at, bytes.begin() + r.at + size));
    r.at += size;
    cfg.facing.emplace();
    descriptor.fields(*cfg.facing, cfg.recovery);
    descriptor.finish();
  } else if (field(bytes, 8) == 2) {
    const auto size = r.count(4096, 1);
    detail::Checkpoint descriptor(
        Bytes(bytes.begin() + r.at, bytes.begin() + r.at + size));
    r.at += size;
    cfg.recovery.emplace();
    descriptor.value(*cfg.recovery);
    descriptor.finish();
  }
  const auto cap = movement ? C::MAX_CREATURE_READY_TICKS : C::MAX_READY_TICKS;
  require(cfg.max_ready_ticks > 0 && cfg.max_ready_ticks <= cap,
          "invalid ready tick bound");
  const auto l = limits(r);
  std::optional<W::LevelData> data;
  const auto supplied = r.u(1);
  require(supplied <= 2, "invalid save terrain marker");
  if (supplied)
    data = terrain(r, l);
  if (supplied == 2) {
    cfg.start.emplace();
    for (auto &word : *cfg.start)
      word = std::uint32_t(r.u(4));
  }
  const auto count = r.count(MAX_ACTIONS, 2);
  std::vector<Action> actions;
  actions.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const auto kind = r.u(1), value = r.u(1);
    const auto frame = cfg.animation_cache ? r.s(8) : 0;
    require((kind <= 3 && !frame &&
             (kind == 0   ? value <= C::COMMAND_MASK
              : kind == 1 ? value <= 1
                          : value == 0)) ||
                (cfg.animation_cache && kind >= 4 && kind <= 6 && value &&
                 value <= cfg.animation_cache->clips.size() && frame >= 1 &&
                 frame <= std::int64_t(C::MAX_SUBMISSIONS)),
            "invalid native save action");
    actions.push_back({std::uint8_t(kind), std::uint8_t(value), frame});
  }
  const auto witness_at = r.at;
  // Validate the entire tail before any replay, including canonical booleans.
  for (unsigned i = 0; i < 6; ++i)
    (void)r.u(4);
  (void)r.s(8);
  for (unsigned i = 0; i < 3; ++i)
    (void)r.u(4);
  (void)r.boolean();
  (void)r.boolean();
  require(!r.boolean(), "failed native save witness");
  const auto error_bytes = r.count(128, 1);
  r.at += error_bytes;
  if (cfg.animation_cache) {
    const auto n = r.count(4096, 1);
    r.at += n;
  }
  if (cfg.facing) {
    (void)r.u(4);
    (void)r.u(2);
  }
  if (cfg.recovery)
    for (unsigned k = 0; k < 3; ++k)
      (void)r.u(4);
  require(r.at == bytes.size(), "trailing native save bytes");
  std::unique_ptr<Scene> next;
  try {
    next = std::make_unique<Scene>(cfg, std::move(data), l);
    if (replay)
      for (const auto &a : actions) {
        switch (a.kind) {
        case 0:
          require(!next->advance(a.value).failed, "native save replay failed");
          break;
        case 1:
          next->set_paused(a.value != 0);
          break;
        case 2:
          next->release_input();
          break;
        case 3:
          next->stop();
          break;
        default:
          next->queue_animation(C::AnimationCommandKind(a.kind - 4), a.value,
                                a.frame);
          break;
        }
      }
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
  if (!replay) {
    next->actions_.assign(actions.begin(), actions.end());
    return next;
  }
  Writer actual;
  witness(actual, next->observation());
  require(actual.bytes.size() == bytes.size() - witness_at &&
              std::equal(actual.bytes.begin(), actual.bytes.end(),
                         bytes.begin() + witness_at),
          "native save continuation witness mismatch");
  return next;
}
namespace {
constexpr std::array<std::uint8_t, 8> SNAP_MAGIC{'O', 'C', 'N', 'S',
                                                 'N', 'A', 'P', 0};
constexpr std::size_t INDEXED_HEADER = 56;
struct Section {
  std::size_t offset = 0, size = 0;
  std::uint32_t checksum = 0;
};
Bytes read_section(const Source &source, const Section &s, std::size_t total) {
  require(source.size() == total, "snapshot source length changed");
  auto bytes = source.read(s.offset, s.size);
  require(bytes.size() == s.size && crc(bytes, false) == s.checksum,
          "snapshot section integrity mismatch");
  return bytes;
}
} // namespace
struct Scene::Deferred {
  std::shared_ptr<const Source> source;
  std::size_t total = 0;
  Section history, replay;
  Bytes setup, current;
  std::vector<std::uint32_t> counts;
  bool resumable = false;
  std::size_t action_count = 0;
};
void Scene::prepare_mutation() {
  if (deferred_ && !deferred_->resumable)
    materialize();
}
std::size_t Scene::retained_actions() const {
  return actions_.size() + (deferred_ ? deferred_->action_count : 0);
}
bool Scene::history_pending() const { return bool(deferred_); }
Bytes Scene::save_indexed_snapshot() const {
  const auto replay = save();
  Scene initial(config_, terrain_, limits_);
  const auto setup_bytes = initial.save();
  detail::Checkpoint state(detail::Checkpoint::Storage::Indexed);
  state.control_anim_cache = bool(config_.animation_cache);
  detail::CheckpointAccess::run(*controller_, state);
  Writer current;
  current.count(actions_.size());
  current.count(state.history_counts.size());
  for (auto n : state.history_counts)
    current.u(n, 4);
  current.count(state.bytes.size());
  for (auto v : state.bytes)
    current.u(v, 1);
  witness(current, observation());
  const std::array<const Bytes *, 4> sections{&setup_bytes, &current.bytes,
                                              &state.history_bytes, &replay};
  Writer w;
  for (auto v : SNAP_MAGIC)
    w.u(v, 1);
  w.u(config_.animation_cache ? CONTROL_ANIM_CACHE_SNAPSHOT_VERSION
      : config_.facing        ? FACING_SNAPSHOT_VERSION
      : config_.recovery      ? RECOVERY_SNAPSHOT_VERSION
                              : 3,
      4);
  w.u(RTA_PROFILE, 4);
  std::size_t total = INDEXED_HEADER;
  for (const auto *b : sections) {
    require(b->size() <= MAX_BYTES - total, "indexed snapshot byte budget");
    total += b->size();
  }
  w.count(total);
  w.u(0, 4);
  for (const auto *b : sections) {
    w.count(b->size());
    w.u(crc(*b, false), 4);
  }
  patch(w.bytes, 20, crc(w.bytes));
  for (const auto *b : sections)
    for (auto v : *b)
      w.u(v, 1);
  return std::move(w.bytes);
}
std::unique_ptr<Scene> Scene::load_source(std::shared_ptr<const Source> source,
                                          const std::string &profile) try {
  require(profile == "rta", "native snapshot requires explicit RTA profile");
  require(bool(source), "missing snapshot source");
  const auto total = source->size();
  require(total >= HEADER && total <= MAX_BYTES, "snapshot source size bounds");
  auto header = source->read(0, HEADER);
  require(header.size() == HEADER, "short snapshot source header");
  if (!std::equal(SNAP_MAGIC.begin(), SNAP_MAGIC.end(), header.begin()) ||
      (field(header, 8) != 2 && field(header, 8) != 3 &&
       field(header, 8) != 4 && field(header, 8) != 5 &&
       field(header, 8) != 6)) {
    auto all = source->read(0, total);
    require(all.size() == total, "short legacy save source");
    return load(all, profile);
  }
  require(total >= INDEXED_HEADER, "short indexed snapshot header");
  header = source->read(0, INDEXED_HEADER);
  require(
      header.size() == INDEXED_HEADER &&
          std::equal(SNAP_MAGIC.begin(), SNAP_MAGIC.end(), header.begin()) &&
          (field(header, 8) == 2 || field(header, 8) == 3 ||
           field(header, 8) == 4 || field(header, 8) == 5 ||
           field(header, 8) == 6) &&
          field(header, 12) == RTA_PROFILE && field(header, 16) == total &&
          field(header, 20) == crc(header),
      "indexed snapshot header integrity/version/profile");
  std::array<Section, 4> sections;
  auto offset = INDEXED_HEADER;
  for (std::size_t i = 0; i < sections.size(); ++i) {
    const auto n = field(header, 24 + 8 * i);
    require(n <= total - offset, "indexed snapshot section bounds");
    sections[i] = {offset, n, field(header, 28 + 8 * i)};
    offset += n;
  }
  require(offset == total, "trailing indexed snapshot bytes");
  try {
    auto setup_bytes = read_section(*source, sections[0], total);
    require(sections[3].size >= HEADER &&
                sections[3].size <=
                    setup_bytes.size() + 10 * MAX_ACTIONS + 4096,
            "indexed snapshot journal section bounds");
    auto next = load_replay(setup_bytes, profile, false);
    require(next->actions_.empty() && next->save() == setup_bytes,
            "indexed snapshot setup must be initial state");
    auto current_bytes = read_section(*source, sections[1], total);
    Reader r{current_bytes, 0};
    require(bool(next->config_.animation_cache) == (field(header, 8) == 6) &&
                (field(header, 8) == 6 ||
                 (bool(next->config_.facing) == (field(header, 8) == 5) &&
                  (field(header, 8) == 5 ||
                   bool(next->config_.recovery) == (field(header, 8) == 4)))),
            "snapshot version/recovery policy mismatch");
    const bool resumable = field(header, 8) >= 3;
    const auto action_count = resumable ? std::size_t(r.u(4)) : 0;
    require(action_count <= MAX_ACTIONS, "indexed action count budget");
    const auto count = r.count(32, 4);
    std::vector<std::uint32_t> counts;
    for (std::size_t i = 0; i < count; ++i)
      counts.push_back(std::uint32_t(r.u(4)));
    const auto n = r.count(MAX_BYTES, 1);
    Bytes current(current_bytes.begin() + r.at,
                  current_bytes.begin() + r.at + n);
    r.at += n;
    require(sections[2].size >= 4 * counts.size(),
            "indexed snapshot missing historical containers");
    detail::Checkpoint input(current, counts);
    input.control_anim_cache = bool(next->config_.animation_cache);
    detail::CheckpointAccess::run(*next->controller_, input);
    input.finish();
    Writer actual;
    witness(actual, next->observation());
    require(actual.bytes.size() == current_bytes.size() - r.at &&
                std::equal(actual.bytes.begin(), actual.bytes.end(),
                           current_bytes.begin() + r.at),
            "indexed snapshot current witness mismatch");
    auto deferred = std::make_shared<Deferred>();
    deferred->source = std::move(source);
    deferred->total = total;
    deferred->history = sections[2];
    deferred->replay = sections[3];
    deferred->setup = std::move(setup_bytes);
    deferred->current = std::move(current);
    deferred->counts = std::move(counts);
    deferred->resumable = resumable;
    deferred->action_count = action_count;
    next->deferred_ = std::move(deferred);
    // Recovery has a fixed256-ready-tick envelope. Validate and own its entire
    // checkpoint before scene replacement, including historical corruption.
    // No journal actions are replayed. Legacy v2/v3 retain their deferred,
    // bounded resumed-action path unchanged.
    if (field(header, 8) >= RECOVERY_SNAPSHOT_VERSION)
      next->materialize();
    return next;
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
} catch (const std::exception &e) {
  throw Error(e.what());
}
void Scene::materialize() const {
  if (!deferred_)
    return;
  const auto &d = *deferred_;
  try {
    auto history = read_section(*d.source, d.history, d.total);
    auto replay = read_section(*d.source, d.replay, d.total);
    auto next = load_replay(replay, "rta", false);
    Scene initial(next->config_, next->terrain_, next->limits_);
    require(initial.save() == d.setup, "deferred snapshot setup mismatch");
    detail::Checkpoint input(d.current, history);
    input.control_anim_cache = bool(next->config_.animation_cache);
    detail::CheckpointAccess::run(*next->controller_, input);
    input.finish();
    if (next->config_.animation_cache) {
      // Validate identity against the retained journal by inspection only.
      // Never replay gameplay or decoder work to recreate this state.
      const auto commands = next->controller_->snapshot().animation_commands;
      std::size_t index = 0;
      for (const auto &action : next->actions_)
        if (action.kind >= 4) {
          require(index < commands.size(), "cache journal has extra command");
          const auto &c = commands[index++];
          require(unsigned(c.kind) == action.kind - 4 &&
                      c.clip == action.value && c.frame == action.frame &&
                      c.id == index,
                  "cache journal/current command identity mismatch");
        }
      require(index == commands.size(), "cache journal missing command");
    }
    detail::Checkpoint canonical(detail::Checkpoint::Storage::Indexed);
    canonical.control_anim_cache = bool(next->config_.animation_cache);
    detail::CheckpointAccess::run(*next->controller_, canonical);
    require(
        canonical.bytes == d.current && canonical.history_bytes == history &&
            canonical.history_counts == d.counts && next->save() == replay &&
            (!d.resumable || next->actions_.size() == d.action_count),
        "deferred snapshot canonical state/history/journal mismatch");
    // Merge the new owned records and actual current values without replaying
    // any old or new operation. Callback indices already include the prefix.
    if (d.resumable && !actions_.empty()) {
      detail::Checkpoint suffix(detail::Checkpoint::Storage::Split);
      detail::CheckpointAccess::run(*controller_, suffix);
      detail::Checkpoint merged(suffix.bytes, suffix.history_bytes, true);
      detail::CheckpointAccess::run(*next->controller_, merged);
      merged.finish();
      detail::Checkpoint final_state(detail::Checkpoint::Storage::Split);
      detail::CheckpointAccess::run(*next->controller_, final_state);
      require(final_state.bytes == suffix.bytes,
              "deferred continuation current state mismatch");
      next->actions_.insert(next->actions_.end(), actions_.begin(),
                            actions_.end());
      Writer live_witness, merged_witness;
      witness(live_witness, observation());
      witness(merged_witness, next->observation());
      require(live_witness.bytes == merged_witness.bytes,
              "deferred continuation witness mismatch");
    }
    controller_ = std::move(next->controller_);
    actions_ = std::move(next->actions_);
    deferred_.reset();
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
}
Bytes Scene::save_snapshot() const {
  if (config_.recovery || config_.facing || config_.animation_cache)
    return save_indexed_snapshot();
  const auto replay = save();
  detail::Checkpoint state;
  detail::CheckpointAccess::run(*controller_, state);
  Writer w;
  for (auto v : SNAP_MAGIC)
    w.u(v, 1);
  w.u(1, 4);
  w.u(RTA_PROFILE, 4);
  w.u(0, 4);
  w.u(0, 4);
  w.count(replay.size());
  for (auto v : replay)
    w.u(v, 1);
  w.count(state.bytes.size());
  for (auto v : state.bytes)
    w.u(v, 1);
  patch(w.bytes, 16, std::uint32_t(w.bytes.size()));
  patch(w.bytes, 20, crc(w.bytes));
  return std::move(w.bytes);
}
std::unique_ptr<Scene> Scene::load(const Bytes &bytes,
                                   const std::string &profile) {
  if (bytes.size() >= 8 &&
      std::equal(PLAY_MAGIC.begin(), PLAY_MAGIC.end(), bytes.begin()))
    return load_continuation(bytes, profile);
  if (bytes.size() >= HEADER &&
      std::equal(SNAP_MAGIC.begin(), SNAP_MAGIC.end(), bytes.begin()) &&
      (field(bytes, 8) == 2 || field(bytes, 8) == 3 || field(bytes, 8) == 4 ||
       field(bytes, 8) == 5 || field(bytes, 8) == 6)) {
    class MemorySource final : public Source {
      Bytes bytes_;

    public:
      explicit MemorySource(const Bytes &b) : bytes_(b) {}
      std::size_t size() const override { return bytes_.size(); }
      Bytes read(std::size_t at, std::size_t n) const override {
        require(at <= bytes_.size() && n <= bytes_.size() - at,
                "snapshot source range");
        return Bytes(bytes_.begin() + at, bytes_.begin() + at + n);
      }
    };
    return load_source(std::make_shared<MemorySource>(bytes), profile);
  }
  if (bytes.size() < 8 ||
      !std::equal(SNAP_MAGIC.begin(), SNAP_MAGIC.end(), bytes.begin()))
    return load_replay(bytes, profile, true);
  require(profile == "rta",
          "native snapshot requires explicit supported RTA profile");
  require(bytes.size() >= HEADER && bytes.size() <= MAX_BYTES,
          "native snapshot size outside bounds");
  require(field(bytes, 8) == 1 && field(bytes, 12) == RTA_PROFILE,
          "unsupported native snapshot version/profile");
  require(field(bytes, 16) == bytes.size() && field(bytes, 20) == crc(bytes),
          "native snapshot length/checksum mismatch");
  Reader r{bytes};
  auto block = [&]() {
    const auto n = r.count(MAX_BYTES, 1);
    Bytes result(bytes.begin() + r.at, bytes.begin() + r.at + n);
    r.at += n;
    return result;
  };
  auto replay = block();
  auto state = block();
  require(r.at == bytes.size(), "trailing native snapshot bytes");
  try {
    // Decode the unchanged replay envelope/journal, but never execute it.
    auto next = load_replay(replay, profile, false);
    require(!next->config_.recovery && !next->config_.facing &&
                !next->config_.animation_cache,
            "legacy snapshot cannot contain recovery/facing");
    detail::Checkpoint input(state);
    detail::CheckpointAccess::run(*next->controller_, input);
    input.finish();
    require(next->save() == replay,
            "native snapshot continuation witness mismatch");
    detail::Checkpoint canonical;
    detail::CheckpointAccess::run(*next->controller_, canonical);
    require(canonical.bytes == state, "noncanonical native snapshot state");
    return next;
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
}
} // namespace champions::simulation::rta::native_save
