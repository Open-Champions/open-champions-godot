// Fixture continuation v1: handwritten little-endian envelope, journal replay and
// redundant witness (docs/NATIVE_SAVE.md "Fixture continuation"). No struct
// dumps, host pointers, callbacks or host floating-point conversions.
#include "champions/play_fixture_save.hpp"
#include "champions/native_save.hpp"
#include <algorithm>
#include <array>
#include <type_traits>

namespace champions::simulation::rta::fixture_save {
namespace {
constexpr std::array<std::uint8_t, 8> MAGIC{'O', 'C', 'F', 'I', 'X', 'T', 0, 0};
constexpr std::size_t HEADER = 24, MAX_TEXT = 64;
// File kinds: the three combat fixtures, then the level fixture.
constexpr std::uint8_t KIND_COMBAT = 0, KIND_ANIMATED = 1, KIND_FULL = 2, KIND_LEVEL = 3;
// Journal operations. Combat and level share the session controls.
enum Op : std::uint8_t {
  ADVANCE = 0, PAUSED = 1, RELEASE_INPUT = 2, STOP = 3,
  HIT_EVENT = 4, PRESS_ATTACK = 5, RELEASE_ATTACK = 6, SET_BLOCK = 7, CAST_HOLY_STRIKE = 8,
  ATTACH = 9, DRAW_SLOTS = 10,
  OPEN_GATE = 11, CONFIRM_GATE = 12, REQUEST_LEVEL = 13,
};
void require(bool ok, const char *what) {
  if (!ok) throw Error(what);
}
// A malformed journal entry (as opposed to an operation the fixture refuses,
// which replays as the same refusal).
struct Corrupt : Error {
  using Error::Error;
};
void valid(bool ok, const char *what) {
  if (!ok) throw Corrupt(what);
}
struct Writer {
  Bytes b;
  void u(std::uint64_t v, unsigned n) {
    require(n <= MAX_BYTES - b.size(), "fixture save exceeds its byte budget");
    for (unsigned i = 0; i < n; ++i) b.push_back(std::uint8_t(v >> (8 * i)));
  }
  void u8(std::uint64_t v) { u(v, 1); }
  void u32(std::uint64_t v) { u(v, 4); }
  void u64(std::uint64_t v) { u(v, 8); }
  void count(std::size_t n) {
    require(n <= UINT32_MAX, "fixture save count exceeds u32");
    u32(n);
  }
  void text(const std::string &s) {
    count(s.size());
    for (unsigned char c : s) u8(c);
  }
  void block(const Bytes &x) {
    count(x.size());
    require(x.size() <= MAX_BYTES - b.size(), "fixture save exceeds its byte budget");
    b.insert(b.end(), x.begin(), x.end());
  }
};
struct Reader {
  const Bytes &b;
  std::size_t at = HEADER;
  std::uint64_t u(unsigned n) {
    require(n <= b.size() - at, "truncated fixture save");
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(b[at++]) << (8 * i);
    return v;
  }
  std::uint8_t u8() { return std::uint8_t(u(1)); }
  std::uint32_t u32() { return std::uint32_t(u(4)); }
  bool boolean() {
    const auto v = u8();
    require(v <= 1, "invalid fixture save boolean");
    return v != 0;
  }
  std::size_t count(std::size_t cap, std::size_t stride) {
    const auto n = u32();
    require(n <= cap && n <= (b.size() - at) / std::max<std::size_t>(stride, 1),
            "fixture save collection exceeds its bounds or the remaining bytes");
    return n;
  }
  std::string text(std::size_t cap) {
    const auto n = count(cap, 1);
    std::string s(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(at + n));
    at += n;
    return s;
  }
  Bytes block() {
    const auto n = count(MAX_BYTES, 1);
    Bytes x(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(at + n));
    at += n;
    return x;
  }
};
void patch(Bytes &b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) b[at + i] = std::uint8_t(v >> (8 * i));
}
std::uint32_t field(const Bytes &b, std::size_t at) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(b[at + i]) << (8 * i);
  return v;
}
Writer header(std::uint8_t kind) {
  Writer w;
  for (auto c : MAGIC) w.u8(c);
  w.u32(VERSION);
  w.u32(RTA_PROFILE);
  w.u32(0); // total length
  w.u32(0); // CRC32
  w.u8(kind);
  return w;
}
Bytes finish(Writer &w) {
  patch(w.b, 16, std::uint32_t(w.b.size()));
  patch(w.b, 20, crc32(w.b, 20));
  return std::move(w.b);
}
Reader open(const Bytes &bytes, const std::string &profile) {
  require(profile == "rta", "fixture continuation requires the explicit RTA profile");
  require(bytes.size() >= HEADER + 1 && bytes.size() <= MAX_BYTES, "fixture save size outside bounds");
  require(is_fixture_save(bytes), "not a fixture continuation");
  require(field(bytes, 8) == VERSION, "unsupported fixture continuation version");
  require(field(bytes, 12) == RTA_PROFILE, "unsupported fixture continuation profile");
  require(field(bytes, 16) == bytes.size() && field(bytes, 20) == crc32(bytes, 20),
          "fixture continuation length/checksum mismatch");
  return Reader{bytes};
}
void put_observation(Writer &w, const session_control::Observation &o) {
  for (auto v : o.position) w.u32(v);
  w.u32(o.hp);
  w.u32(o.max_health);
  w.u32(o.radius);
  w.u32(o.facing);
  w.u64(std::uint64_t(o.frame));
  w.count(o.submissions);
  w.count(o.ready_ticks);
  w.count(o.damage_events);
  w.u8(o.paused);
  w.u8(o.stopped);
  w.u8(o.failed);
  w.text(o.error);
}
std::uint8_t kind_of(combat_play::Fixture f) {
  return f == combat_play::Fixture::Full ? KIND_FULL
         : f == combat_play::Fixture::Animated ? KIND_ANIMATED : KIND_COMBAT;
}
} // namespace

bool is_fixture_save(const Bytes &bytes) {
  return bytes.size() >= MAGIC.size() && std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin());
}
std::uint32_t crc32(const Bytes &bytes, std::size_t skip_at) {
  // Reflected IEEE polynomial, as the OCNSAVE envelope; bytes skip_at..+3 read as 0.
  std::uint32_t r = 0xffffffffu;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    r ^= (skip_at != SIZE_MAX && i >= skip_at && i < skip_at + 4) ? 0 : bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit) r = (r >> 1) ^ (0xedb88320u & (0u - (r & 1u)));
  }
  return ~r;
}
ConstantsIdentity identify(const std::string &json) {
  require(json.size() <= UINT32_MAX, "recipe constants too large");
  return {std::uint32_t(json.size()), crc32(Bytes(json.begin(), json.end()))};
}

// ---- combat ----------------------------------------------------------------------------------
CombatScene::CombatScene(const world_queries::Snapshot &terrain, combat_play::Fixture which,
                         std::shared_ptr<const fxskills::RecipeConstants> constants, ConstantsIdentity identity)
    : terrain_(terrain), fixture_(which), identity_(identity),
      session_(std::make_unique<combat_play::Session>(terrain, which)) {
  if (which == combat_play::Fixture::Full) {
    require(bool(constants), "the Full fixture requires effect recipe constants");
    effects_ = std::make_unique<combat_play::Effects>(*session_, std::move(constants));
  } else {
    require(!constants && identity == ConstantsIdentity{}, "only the Full fixture takes recipe constants");
  }
}
CombatScene::~CombatScene() {
  effects_.reset(); // before the session it observes
}

template <class F> auto CombatScene::record(std::uint8_t op, std::uint32_t value, F &&f) -> decltype(f()) {
  if (!replaying_) {
    require(journal_.size() < MAX_ACTIONS, "fixture journal is full; save refused beyond this point");
    journal_.reserve(journal_.size() + 1);
  }
  Action a{op, 0, value};
  try {
    if constexpr (std::is_void_v<decltype(f())>) {
      f();
      if (!replaying_) journal_.push_back(a);
    } else {
      auto r = f();
      if (!replaying_) journal_.push_back(a);
      return r;
    }
  } catch (...) {
    a.refused = 1;
    if (!replaying_) journal_.push_back(a);
    throw;
  }
}

session_control::Observation CombatScene::advance(std::uint32_t command) {
  return record(ADVANCE, command, [&] { return session_->advance(command); });
}
void CombatScene::hit_event(unsigned slot, Raw mask) {
  require(slot <= 0xff && mask <= 0xffffff, "hit event outside the journal's range");
  record(HIT_EVENT, (slot << 24) | mask, [&] { session_->hit_event(slot, mask); });
}
void CombatScene::press_attack(bool held) {
  record(PRESS_ATTACK, held, [&] { session_->press_attack(held); });
}
void CombatScene::release_attack() {
  record(RELEASE_ATTACK, 0, [&] { session_->release_attack(); });
}
void CombatScene::set_block(bool held) {
  record(SET_BLOCK, held, [&] { session_->set_block(held); });
}
void CombatScene::cast_holy_strike() {
  record(CAST_HOLY_STRIKE, 0, [&] { session_->cast_holy_strike(); });
}
void CombatScene::release_input() {
  record(RELEASE_INPUT, 0, [&] { session_->release_input(); });
}
void CombatScene::set_paused(bool value) {
  record(PAUSED, value, [&] { session_->set_paused(value); });
}
void CombatScene::stop() {
  record(STOP, 0, [&] { session_->stop(); });
}
void CombatScene::attach(fxsim::EffectWorld *world) {
  record(ATTACH, world != nullptr, [&] {
    require(bool(effects_), "only the Full fixture has an effect simulation");
    effects_->attach(world);
  });
}
void CombatScene::draw_slots() {
  record(DRAW_SLOTS, 0, [&] {
    require(bool(effects_), "only the Full fixture has an effect simulation");
    effects_->draw_slots();
  });
}

void CombatScene::apply(const Action &a) {
  switch (a.op) {
  case ADVANCE: session_->advance(a.value); return;
  case PAUSED: valid(a.value <= 1, "journal pause value"); session_->set_paused(a.value != 0); return;
  case RELEASE_INPUT: session_->release_input(); return;
  case STOP: session_->stop(); return;
  case HIT_EVENT: session_->hit_event(a.value >> 24, a.value & 0xffffff); return;
  case PRESS_ATTACK: valid(a.value <= 1, "journal attack value"); session_->press_attack(a.value != 0); return;
  case RELEASE_ATTACK: session_->release_attack(); return;
  case SET_BLOCK: valid(a.value <= 1, "journal block value"); session_->set_block(a.value != 0); return;
  case CAST_HOLY_STRIKE: session_->cast_holy_strike(); return;
  case ATTACH:
    valid(a.value <= 1, "journal attach value");
    require(bool(effects_), "only the Full fixture has an effect simulation");
    // The per-machine world itself is the presentation block; during replay a
    // stand-in receives what the attached world received, so the effect half's
    // own state (draw-slot glows, model image, made flags) is reproduced.
    if (a.value && !stand_in_) stand_in_ = std::make_unique<fxsim::EffectWorld>();
    effects_->attach(a.value ? stand_in_.get() : nullptr);
    return;
  case DRAW_SLOTS:
    require(bool(effects_), "only the Full fixture has an effect simulation");
    effects_->draw_slots();
    return;
  default: throw Corrupt("unknown combat journal operation");
  }
}

Bytes CombatScene::witness() const {
  Writer w;
  put_observation(w, session_->observation());
  w.u32(session_->shared_seed());
  const auto actors = session_->actors();
  w.count(actors.size());
  for (const auto &a : actors) {
    w.u32(a.slot);
    w.u32(a.handle);
    w.u8(a.present);
    w.u8(a.dead);
    for (auto v : a.position) w.u32(v);
    w.u32(a.hp);
    w.u32(a.state);
  }
  w.u8(bool(effects_));
  if (effects_) {
    const auto &c = effects_->counts();
    for (auto v : {c.ticks, c.particle_spawns, c.fx_spawns, c.decals, c.glows, c.class_runs, c.game_damage,
                   c.game_queries})
      w.u64(v);
    w.count(c.unrouted.size());
    for (const auto &kv : c.unrouted) {
      w.text(kv.first);
      w.u64(kv.second);
    }
  }
  return std::move(w.b);
}

Bytes CombatScene::save(const Bytes *presentation) const {
  Writer w = header(kind_of(fixture_));
  native_save::terrain_codec::encode_terrain(w.b, terrain_);
  if (fixture_ == combat_play::Fixture::Full) {
    w.u32(identity_.length);
    w.u32(identity_.crc);
  }
  w.count(journal_.size());
  for (const auto &a : journal_) {
    w.u8(a.op);
    w.u8(a.refused);
    w.u32(a.value);
  }
  w.block(witness());
  w.u8(presentation != nullptr);
  if (presentation) w.block(*presentation);
  return finish(w);
}

CombatScene::Loaded CombatScene::load(const Bytes &bytes, const std::string &profile,
                                      std::shared_ptr<const fxskills::RecipeConstants> constants,
                                      ConstantsIdentity identity) {
  Reader r = open(bytes, profile);
  const auto kind = r.u8();
  require(kind <= KIND_FULL, "not a combat fixture continuation");
  const auto which = kind == KIND_FULL ? combat_play::Fixture::Full
                     : kind == KIND_ANIMATED ? combat_play::Fixture::Animated : combat_play::Fixture::Combat;
  Loaded out;
  try {
    const auto terrain = native_save::terrain_codec::decode_terrain(bytes, r.at);
    ConstantsIdentity saved{};
    if (which == combat_play::Fixture::Full) {
      saved.length = r.u32();
      saved.crc = r.u32();
      require(bool(constants), "this Full combat save needs the recipe constants it was made with");
      require(saved == identity, "the loaded recipe constants differ from the ones this save was made with");
    } else {
      constants.reset();
    }
    std::vector<Action> journal(r.count(MAX_ACTIONS, 6));
    for (auto &a : journal) {
      a.op = r.u8();
      a.refused = std::uint8_t(r.boolean());
      a.value = r.u32();
    }
    const Bytes recorded = r.block();
    if (r.boolean()) out.presentation = r.block();
    require(r.at == bytes.size(), "trailing bytes after the fixture continuation");

    auto scene = std::make_unique<CombatScene>(terrain, which, constants, saved);
    scene->replaying_ = true;
    for (std::size_t k = 0; k < journal.size(); ++k) {
      bool refused = false;
      try {
        scene->apply(journal[k]);
      } catch (const Corrupt &) {
        throw;
      } catch (const std::exception &) {
        refused = true;
      }
      if (refused != bool(journal[k].refused))
        throw Error("fixture journal replay diverged at operation " + std::to_string(k));
    }
    scene->replaying_ = false;
    scene->journal_ = std::move(journal);
    require(scene->witness() == recorded, "fixture continuation witness mismatch");
    const Bytes *p = out.presentation ? &*out.presentation : nullptr;
    require(scene->save(p) == bytes, "noncanonical fixture continuation");
    out.scene = std::move(scene);
  } catch (const Error &) {
    throw;
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
  return out;
}

// ---- level -----------------------------------------------------------------------------------
LevelScene::LevelScene() : session_(std::make_unique<level_play::Session>()) {}
LevelScene::~LevelScene() = default;

template <class F> auto LevelScene::record(Action a, F &&f) -> decltype(f()) {
  require(journal_.size() < MAX_ACTIONS, "fixture journal is full; save refused beyond this point");
  require(a.text.size() <= MAX_TEXT, "level name too long for the journal");
  journal_.reserve(journal_.size() + 1);
  try {
    if constexpr (std::is_void_v<decltype(f())>) {
      f();
      journal_.push_back(a);
    } else {
      auto r = f();
      journal_.push_back(a);
      return r;
    }
  } catch (...) {
    a.refused = 1;
    journal_.push_back(a);
    throw;
  }
}
session_control::Observation LevelScene::advance(std::uint32_t command) {
  return record({ADVANCE, 0, command, {}}, [&] { return session_->advance(command); });
}
void LevelScene::open_gate() {
  record({OPEN_GATE, 0, 0, {}}, [&] { session_->open_gate(); });
}
void LevelScene::confirm_gate(const std::string &d) {
  record({CONFIRM_GATE, 0, 0, d}, [&] { session_->confirm_gate(d); });
}
void LevelScene::request_level(const std::string &d) {
  record({REQUEST_LEVEL, 0, 0, d}, [&] { session_->request_level(d); });
}
void LevelScene::release_input() {
  record({RELEASE_INPUT, 0, 0, {}}, [&] { session_->release_input(); });
}
void LevelScene::set_paused(bool value) {
  record({PAUSED, 0, value, {}}, [&] { session_->set_paused(value); });
}
void LevelScene::stop() {
  record({STOP, 0, 0, {}}, [&] { session_->stop(); });
}
void LevelScene::apply(const Action &a) {
  switch (a.op) {
  case ADVANCE: session_->advance(a.value); return;
  case PAUSED: valid(a.value <= 1, "journal pause value"); session_->set_paused(a.value != 0); return;
  case RELEASE_INPUT: session_->release_input(); return;
  case STOP: session_->stop(); return;
  case OPEN_GATE: session_->open_gate(); return;
  case CONFIRM_GATE: session_->confirm_gate(a.text); return;
  case REQUEST_LEVEL: session_->request_level(a.text); return;
  default: throw Corrupt("unknown level journal operation");
  }
}
Bytes LevelScene::witness() const {
  Writer w;
  put_observation(w, session_->observation());
  w.u32(session_->shared_seed());
  const auto f = session_->flow();
  w.text(f.level);
  for (auto v : {f.generation, f.travel_ticks, f.restored}) w.u32(v);
  w.u8(f.gate_contact);
  w.u8(f.trigger_inside);
  w.u8(f.menu_open);
  for (auto v : f.marker) w.u32(v);
  for (auto v : f.resolved_spawn) w.u32(v);
  w.u64(f.saved_levels);
  w.u64(f.restored_bytes);
  return std::move(w.b);
}
Bytes LevelScene::save() const {
  Writer w = header(KIND_LEVEL);
  w.count(journal_.size());
  for (const auto &a : journal_) {
    w.u8(a.op);
    w.u8(a.refused);
    w.u32(a.value);
    if (a.op == CONFIRM_GATE || a.op == REQUEST_LEVEL) w.text(a.text);
  }
  w.block(witness());
  w.u8(0); // the level fixture has no per-machine presentation state
  return finish(w);
}
std::unique_ptr<LevelScene> LevelScene::load(const Bytes &bytes, const std::string &profile) {
  Reader r = open(bytes, profile);
  require(r.u8() == KIND_LEVEL, "not a level fixture continuation");
  try {
    std::vector<Action> journal(r.count(MAX_ACTIONS, 6));
    for (auto &a : journal) {
      a.op = r.u8();
      a.refused = std::uint8_t(r.boolean());
      a.value = r.u32();
      if (a.op == CONFIRM_GATE || a.op == REQUEST_LEVEL) a.text = r.text(MAX_TEXT);
    }
    const Bytes recorded = r.block();
    require(!r.boolean(), "the level fixture has no presentation block");
    require(r.at == bytes.size(), "trailing bytes after the fixture continuation");
    auto scene = std::make_unique<LevelScene>();
    for (std::size_t k = 0; k < journal.size(); ++k) {
      bool refused = false;
      try {
        scene->apply(journal[k]);
      } catch (const Corrupt &) {
        throw;
      } catch (const std::exception &) {
        refused = true;
      }
      if (refused != bool(journal[k].refused))
        throw Error("fixture journal replay diverged at operation " + std::to_string(k));
    }
    scene->journal_ = std::move(journal);
    require(scene->witness() == recorded, "fixture continuation witness mismatch");
    require(scene->save() == bytes, "noncanonical fixture continuation");
    return scene;
  } catch (const Error &) {
    throw;
  } catch (const std::exception &e) {
    throw Error(e.what());
  }
}
} // namespace champions::simulation::rta::fixture_save
