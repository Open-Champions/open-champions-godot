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
void require(bool value, const char *message) {
  if (!value)
    throw Error(message);
}
std::uint32_t crc(const Bytes &bytes) {
  std::uint32_t result = 0xffffffffu;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    // The checksum field itself is treated as four zero bytes.
    result ^= i >= 20 && i < 24 ? 0 : bytes[i];
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
}
} // namespace
Scene::Scene(C::Config config, std::optional<W::LevelData> data, W::Limits l)
    : controller_(std::make_unique<C::Controller>(config, data, l)),
      config_(std::move(config)), terrain_(std::move(data)), limits_(l) {
  // Enforce the serialization budget before exposing a live scene.
  require(save().size() <= MAX_BYTES - 2 * MAX_ACTIONS - 128,
          "initial terrain leaves insufficient action budget");
  actions_.reserve(MAX_ACTIONS);
}
void Scene::reserve_action() {
  require(actions_.size() < MAX_ACTIONS, "native save action budget exhausted");
}
C::Observation Scene::advance(std::uint32_t command) {
  require((command & ~C::COMMAND_MASK) == 0, "unknown control command bit");
  reserve_action();
  // Capacity and command validation precede mutation. Keep the input even if
  // copying the returned observation throws after a completed submission.
  actions_.push_back({0, std::uint8_t(command)});
  return controller_->advance(command);
}
void Scene::set_paused(bool value) {
  reserve_action();
  controller_->set_paused(value);
  actions_.push_back({1, std::uint8_t(value)});
}
void Scene::release_input() {
  reserve_action();
  controller_->release_input();
  actions_.push_back({2, 0});
}
void Scene::stop() {
  reserve_action();
  controller_->stop();
  actions_.push_back({3, 0});
}
void Scene::reset() {
  controller_->reset();
  actions_.clear();
}
C::Observation Scene::observation() const { return controller_->observation(); }
C::Snapshot Scene::snapshot() const { return controller_->snapshot(); }
Bytes Scene::save() const {
  const auto o = observation();
  require(!o.failed,
          "failed operational prefix is not a reloadable checkpoint");
  Writer w;
  for (auto v : MAGIC)
    w.u(v, 1);
  w.u(VERSION, 4);
  w.u(RTA_PROFILE, 4);
  w.u(0, 4);
  w.u(0, 4);
  w.u(unsigned(config_.model), 1);
  w.u(config_.mul_bias, 1);
  w.u(config_.div_nearest, 1);
  w.u(unsigned(config_.movement), 1);
  w.u(config_.shared_seed, 4);
  w.count(config_.max_ready_ticks);
  limits(w, limits_);
  w.u(terrain_.has_value(), 1);
  if (terrain_)
    terrain(w, *terrain_);
  w.count(actions_.size());
  for (const auto &a : actions_) {
    w.u(a.kind, 1);
    w.u(a.value, 1);
  }
  witness(w, o);
  patch(w.bytes, 16, std::uint32_t(w.bytes.size()));
  patch(w.bytes, 20, crc(w.bytes));
  return std::move(w.bytes);
}
std::unique_ptr<Scene> Scene::load(const Bytes &bytes,
                                   const std::string &profile) {
  require(profile == "rta",
          "native save requires explicit supported RTA profile");
  require(bytes.size() >= HEADER && bytes.size() <= MAX_BYTES,
          "native save size outside bounds");
  require(std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin()),
          "native save magic mismatch");
  require(field(bytes, 8) == VERSION, "unsupported native save version");
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
  const auto cap = movement ? C::MAX_CREATURE_READY_TICKS : C::MAX_READY_TICKS;
  require(cfg.max_ready_ticks > 0 && cfg.max_ready_ticks <= cap,
          "invalid ready tick bound");
  const auto l = limits(r);
  std::optional<W::LevelData> data;
  if (r.boolean())
    data = terrain(r, l);
  const auto count = r.count(MAX_ACTIONS, 2);
  std::vector<Action> actions;
  actions.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const auto kind = r.u(1), value = r.u(1);
    require(kind <= 3 && (kind == 0   ? value <= C::COMMAND_MASK
                          : kind == 1 ? value <= 1
                                      : value == 0),
            "invalid native save action");
    actions.push_back({std::uint8_t(kind), std::uint8_t(value)});
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
  require(r.at == bytes.size(), "trailing native save bytes");
  std::unique_ptr<Scene> next;
  try {
    next = std::make_unique<Scene>(cfg, std::move(data), l);
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
      }
    }
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
  Writer actual;
  witness(actual, next->observation());
  require(actual.bytes.size() == bytes.size() - witness_at &&
              std::equal(actual.bytes.begin(), actual.bytes.end(),
                         bytes.begin() + witness_at),
          "native save continuation witness mismatch");
  return next;
}
} // namespace champions::simulation::rta::native_save
