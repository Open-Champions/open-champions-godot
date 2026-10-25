// Original implementation of the published executable Python contract:
// ref_anim.py f516ac798bf28375554fe53ffb31b74802844136, SHA-256
// 6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8,
// sections 1/2; animation.md f2b996dded898f1b0b558b901cab875f775cfdc7.
// No pose construction, clock integration, sound execution or asset data.
#include "champions/animation_decoder.hpp"
#include "champions/core_math.hpp"
#include "champions/detail/checkpoint_control_anim_cache.hpp"
#include <algorithm>
#include <limits>
#include <utility>

namespace champions::simulation::rta::animation_decoder {
namespace {
constexpr Raw K_POS = 0x3c800000u, K_QUAT = 0x39800000u, K_PVEL = 0x3b800000u,
              K_QVEL = 0x37000000u;
constexpr std::size_t MAX_NAME_BYTES = 1024;

void check_limits(const Limits &l) {
  if (l.input_bytes > MAX_INPUT_BYTES || l.owned_bytes > MAX_OWNED_BYTES ||
      l.clips > MAX_CLIPS || l.tracks > MAX_TRACKS || l.slots > MAX_SLOTS ||
      l.key_records > MAX_KEY_RECORDS)
    throw ResourceLimit("decoder limits exceed hard caps");
}
void charge(std::size_t &used, std::size_t count, std::size_t limit) {
  if (used > limit || count > limit - used)
    throw ResourceLimit("decoder owned bytes exceeded");
  used += count;
}
void charge_array(std::size_t &used, std::size_t count, std::size_t width,
                  std::size_t limit) {
  if (count > limit / width)
    throw ResourceLimit("decoder owned array exceeded");
  charge(used, count * width, limit);
}
std::size_t clip_size(const Clip &c, const Limits &l) {
  if (c.data.size() > l.input_bytes || c.name.size() > MAX_NAME_BYTES)
    throw ResourceLimit("decoder input or name exceeded");
  std::size_t n = 0;
  charge(n, c.name.size(), l.owned_bytes);
  charge(n, c.data.size(), l.owned_bytes);
  charge_array(n, c.words.size(), 2, l.owned_bytes);
  charge_array(n, c.bind.size(), 8, l.owned_bytes);
  charge(n, c.depth.size(), l.owned_bytes);
  if (c.sound_times)
    charge_array(n, c.sound_times->size(), 4, l.owned_bytes);
  return n;
}
std::uint16_t u16(const std::vector<std::uint8_t> &v, std::size_t p) {
  return static_cast<std::uint16_t>(v[p] | (std::uint16_t(v[p + 1]) << 8));
}
std::uint32_t u32(const std::vector<std::uint8_t> &v, std::size_t p) {
  return std::uint32_t(v[p]) | (std::uint32_t(v[p + 1]) << 8) |
         (std::uint32_t(v[p + 2]) << 16) | (std::uint32_t(v[p + 3]) << 24);
}
bool extent(std::size_t offset, std::size_t count, std::size_t size) {
  return offset <= size && count <= size - offset;
}
std::int64_t add_tick(std::int64_t a, std::uint32_t b) {
  if (a > std::numeric_limits<std::int64_t>::max() - b)
    throw ResourceLimit("decoder tick exceeds signed64 domain");
  return a + b;
}
std::int64_t subtract_tick(std::int64_t a, std::int64_t b) {
  if ((b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) ||
      (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b))
    throw ResourceLimit("decoder tick difference exceeds signed64 domain");
  return a - b;
}
std::uint32_t field(const Clip &c, std::uint64_t bit, unsigned width) {
  const auto index = bit >> 4;
  const auto bits = std::uint64_t(c.words.size()) * 16;
  std::uint32_t pair = 0;
  if (index < c.words.size() && c.words.size() - index >= 2) {
    pair = (std::uint32_t(c.words[static_cast<std::size_t>(index)]) << 16) |
           c.words[static_cast<std::size_t>(index + 1)];
  } else {
    if (bit > bits || width > bits - bit)
      throw HostFault("key stream read extends beyond supplied words");
    // The last word's missing successor is zero, exactly as Clip.word.
    if (index < c.words.size())
      pair = std::uint32_t(c.words[static_cast<std::size_t>(index)]) << 16;
  }
  return pair << (bit & 15);
}
std::uint32_t unsigned_field(const Clip &c, std::uint64_t bit, unsigned width) {
  return field(c, bit, width) >> (32 - width);
}
std::int32_t signed_field(const Clip &c, std::uint64_t bit, unsigned width) {
  const auto value = unsigned_field(c, bit, width);
  const auto result = (value & (std::uint32_t(1) << (width - 1)))
                          ? std::int64_t(value) - (std::int64_t(1) << width)
                          : std::int64_t(value);
  return static_cast<std::int32_t>(result);
}
Raw scaled_field(const Clip &c, std::uint64_t bit, unsigned width, Raw scale) {
  return mul(from_int(static_cast<std::uint32_t>(signed_field(c, bit, width))),
             scale);
}
std::int32_t yaw(const std::array<Raw, 4> &q) {
  const Raw side = add(mul(q[0], q[1]), mul(q[2], q[3]));
  return atan2_16(
      add(side, side),
      add(sub(sub(mul(q[0], q[0]), mul(q[1], q[1])), mul(q[2], q[2])),
          mul(q[3], q[3])));
}
std::size_t joint_index(std::int64_t index) {
  if (index < -static_cast<std::int64_t>(MAX_JOINTS) ||
      index >= static_cast<std::int64_t>(MAX_JOINTS))
    throw IndexError("joint index outside fixed decoder slot");
  return static_cast<std::size_t>(index < 0 ? index + MAX_JOINTS : index);
}
} // namespace

std::uint16_t Clip::word(std::int64_t index) const noexcept {
  return index >= 0 && static_cast<std::uint64_t>(index) < words.size()
             ? words[static_cast<std::size_t>(index)]
             : 0;
}

Clip parse_clip(const std::vector<std::uint8_t> &data, const std::string &name,
                Limits limits) {
  check_limits(limits);
  if (data.size() > limits.input_bytes || name.size() > MAX_NAME_BYTES)
    throw ResourceLimit("decoder input or name exceeded");
  if (data.size() < 0x70)
    throw ValueError("an animation clip needs its 0x70-byte header");
  Clip c;
  c.joint_count = s32(u32(data, 0));
  c.length = s32(u32(data, 4));
  const std::size_t key = u32(data, 8), bind = u32(data, 12),
                    depth = u32(data, 16), sound = u32(data, 0x38);
  for (std::size_t i = 0; i < 4; ++i)
    c.events[i] = {u32(data, 0x14 + 8 * i), data[0x18 + 8 * i]};
  c.sound_group = data[0x3c];
  c.joint_mask =
      std::uint64_t(u32(data, 0x48)) | (std::uint64_t(u32(data, 0x4c)) << 32);
  if (c.joint_count < 0 ||
      c.joint_count > static_cast<std::int32_t>(MAX_JOINTS))
    throw HostFault("joint count outside fixed decoder slot");
  const auto count = static_cast<std::size_t>(c.joint_count);
  if (!extent(bind, count * 8, data.size()) ||
      !extent(depth, count, data.size()) || key > data.size())
    throw ValueError("skeleton tables outside the file");
  std::size_t size = 0;
  charge(size, name.size(), limits.owned_bytes);
  charge(size, data.size(), limits.owned_bytes);
  charge_array(size, count, 8, limits.owned_bytes);
  charge(size, count, limits.owned_bytes);
  const auto words = (data.size() - key + 1) / 2;
  charge_array(size, words, 2, limits.owned_bytes);
  std::size_t sounds = 0;
  if (sound) {
    auto p = sound;
    for (;;) {
      if (!extent(p, 4, data.size()))
        throw ValueError("sound event list without terminator");
      charge(size, 4, limits.owned_bytes);
      ++sounds;
      if (u32(data, p) == 0x7fffffffu)
        break;
      // p is input bounded, and the hard byte cap is far below SIZE_MAX.
      p += 0x1c;
    }
  }
  c.name = name;
  c.data = data;
  c.bind.resize(count);
  c.depth.resize(count);
  for (std::size_t j = 0; j < count; ++j) {
    for (std::size_t k = 0; k < 4; ++k)
      c.bind[j][k] =
          static_cast<std::int16_t>(s16(u16(data, bind + 8 * j + 2 * k)));
    const auto b = data[depth + j];
    c.depth[j] = static_cast<std::int8_t>(b < 128 ? b : std::int32_t(b) - 256);
  }
  c.words.reserve(words);
  for (std::size_t p = key; p < data.size(); p += 2)
    c.words.push_back(static_cast<std::uint16_t>(
        data[p] | (p + 1 < data.size() ? std::uint16_t(data[p + 1]) << 8 : 0)));
  if (sound) {
    c.sound_times.emplace();
    c.sound_times->reserve(sounds);
    for (std::size_t i = 0; i < sounds; ++i)
      c.sound_times->push_back(s32(u32(data, sound + 0x1c * i)));
  }
  return c;
}

struct Context::State {
  Limits limits;
  std::vector<std::unique_ptr<Clip>> clips;
  std::vector<std::unique_ptr<Track>> tracks;
  std::vector<Slot> slots;
  std::vector<SlotId> order;
};
Context::Context(std::size_t slot_count, Limits limits)
    : state_(std::make_unique<State>()) {
  check_limits(limits);
  if (slot_count < 2)
    throw ValueError("decoder list needs at least two slots");
  if (slot_count > limits.slots)
    throw ResourceLimit("decoder slot limit exceeded");
  state_->limits = limits;
  state_->slots.resize(slot_count);
  state_->order.reserve(slot_count);
  for (std::size_t i = 0; i < slot_count; ++i) {
    state_->slots[i].index = static_cast<std::uint32_t>(i);
    state_->order.push_back(static_cast<SlotId>(slot_count - i));
  }
}
Context::~Context() = default;
Clip &Context::clip(ClipId id) {
  return const_cast<Clip &>(static_cast<const Context &>(*this).clip(id));
}
const Clip &Context::clip(ClipId id) const {
  if (!id || id > state_->clips.size())
    throw Unsupported("unknown decoder clip identity");
  return *state_->clips[static_cast<std::size_t>(id - 1)];
}
Track &Context::track(TrackId id) {
  return const_cast<Track &>(static_cast<const Context &>(*this).track(id));
}
const Track &Context::track(TrackId id) const {
  if (!id || id > state_->tracks.size())
    throw Unsupported("unknown decoder track identity");
  return *state_->tracks[static_cast<std::size_t>(id - 1)];
}
Slot &Context::slot(SlotId id) {
  return const_cast<Slot &>(static_cast<const Context &>(*this).slot(id));
}
const Slot &Context::slot(SlotId id) const {
  if (!id || id > state_->slots.size())
    throw Unsupported("unknown decoder slot identity");
  return state_->slots[id - 1];
}
std::vector<SlotId> &Context::order() noexcept { return state_->order; }
const std::vector<SlotId> &Context::order() const noexcept {
  return state_->order;
}

void Context::validate() const {
  const auto &s = *state_;
  std::size_t bytes = 0;
  for (const auto &c : s.clips)
    charge(bytes, clip_size(*c, s.limits), s.limits.owned_bytes);
  if (s.order.size() != s.slots.size())
    throw Unsupported("decoder LRU shape differs from pool");
  std::vector<bool> seen(s.slots.size(), false);
  for (const auto id : s.order) {
    if (!id || id > s.slots.size() || seen[id - 1])
      throw Unsupported("decoder LRU is not a pool permutation");
    seen[id - 1] = true;
  }
  for (std::size_t i = 0; i < s.slots.size(); ++i) {
    const auto &p = s.slots[i];
    if (p.index != i || p.clip > s.clips.size() || p.owner > s.tracks.size())
      throw Unsupported("decoder slot has foreign identity");
  }
  for (const auto &t : s.tracks)
    if (t->clip > s.clips.size() || t->slot > s.slots.size())
      throw Unsupported("decoder track has foreign identity");
}
ClipId Context::add_clip(const std::vector<std::uint8_t> &data,
                         const std::string &name) {
  validate();
  if (state_->clips.size() >= state_->limits.clips)
    throw ResourceLimit("decoder clip identities exhausted");
  auto available = state_->limits;
  for (const auto &c : state_->clips)
    available.owned_bytes -= clip_size(*c, state_->limits);
  return add_clip(parse_clip(data, name, available));
}
ClipId Context::add_clip(const Clip &value) {
  validate();
  if (state_->clips.size() >= state_->limits.clips)
    throw ResourceLimit("decoder clip identities exhausted");
  std::size_t bytes = clip_size(value, state_->limits);
  for (const auto &c : state_->clips)
    charge(bytes, clip_size(*c, state_->limits), state_->limits.owned_bytes);
  state_->clips.push_back(std::make_unique<Clip>(value));
  return state_->clips.size();
}
TrackId Context::add_track(const Track &value) {
  validate();
  if (state_->tracks.size() >= state_->limits.tracks)
    throw ResourceLimit("decoder track identities exhausted");
  if (value.clip > state_->clips.size() || value.slot > state_->slots.size())
    throw Unsupported("new decoder track has foreign identity");
  state_->tracks.push_back(std::make_unique<Track>(value));
  return state_->tracks.size();
}
Snapshot Context::snapshot() const {
  // Observation does not dereference graph IDs or repair supplied malformed
  // graphs. Resource bounds still apply before making owned copies.
  std::size_t bytes = 0;
  for (const auto &c : state_->clips)
    charge(bytes, clip_size(*c, state_->limits), state_->limits.owned_bytes);
  if (state_->order.size() > state_->limits.slots)
    throw ResourceLimit("decoder observation LRU size exceeded");
  Snapshot out;
  out.sim_frame = sim_frame;
  out.cosmetic_seed = cosmetic_seed;
  out.limits = state_->limits;
  for (std::size_t i = 0; i < state_->clips.size(); ++i)
    out.clips.push_back({i + 1, *state_->clips[i]});
  for (std::size_t i = 0; i < state_->tracks.size(); ++i)
    out.tracks.push_back({i + 1, *state_->tracks[i]});
  out.slots = state_->slots;
  out.order = state_->order;
  return out;
}
// Internal direct restore only. No allocator/LRU/decoder operation is replayed.
void Context::checkpoint(detail::Checkpoint &a) {
  using A = detail::Checkpoint;
  A::require(a.control_anim_cache, "decoder checkpoint requires cache schema");
  auto value = snapshot();
  const auto original_limits = value.limits;
  const auto original_slots = value.slots.size();
  a.value(value);
  A left, right;
  auto expected = original_limits;
  left.value(expected);
  right.value(value.limits);
  A::require(left.bytes == right.bytes && value.slots.size() == original_slots,
             "decoder checkpoint immutable limits/pool");
  if (!a.reading) {
    validate();
    return;
  }
  Context next(original_slots, original_limits);
  A::require(value.clips.size() <= original_limits.clips &&
                 value.tracks.size() <= original_limits.tracks,
             "decoder checkpoint identity counts");
  for (const auto &entry : value.clips) {
    A::require(entry.id == next.state_->clips.size() + 1,
               "decoder checkpoint canonical clip ID");
    const auto &c = entry.value;
    A::require(c.joint_count >= 1 && c.joint_count <= 4 && c.length > 0 &&
                   c.length <= 65536 &&
                   c.bind.size() == std::size_t(c.joint_count) &&
                   c.depth.size() == std::size_t(c.joint_count),
               "decoder checkpoint clip shape");
    next.state_->clips.push_back(std::make_unique<Clip>(c));
  }
  for (const auto &entry : value.tracks) {
    A::require(entry.id == next.state_->tracks.size() + 1,
               "decoder checkpoint canonical track ID");
    next.state_->tracks.push_back(std::make_unique<Track>(entry.value));
  }
  next.state_->slots = std::move(value.slots);
  next.state_->order = std::move(value.order);
  next.validate(); // Membership, not reciprocity: stale links are valid.
  for (const auto &slot : next.state_->slots)
    A::require(!slot.clip ||
                   slot.cursor <= next.clip(slot.clip).words.size() * 16,
               "decoder checkpoint stream cursor");
  next.sim_frame = value.sim_frame;
  next.cosmetic_seed = value.cosmetic_seed;
  state_.swap(next.state_);
  sim_frame = next.sim_frame;
  cosmetic_seed = next.cosmetic_seed;
}
void Context::touch(SlotId id) {
  validate();
  (void)slot(id);
  auto &o = state_->order;
  if (o.back() != id) {
    const auto it = std::find(o.begin(), o.end(), id);
    o.erase(it);
    o.push_back(id);
  }
}
SlotId Context::alloc() {
  validate();
  auto &o = state_->order;
  std::size_t tries = 0;
  while (slot(o.front()).owner &&
         (track(slot(o.front()).owner).flags & (ROOT | YAW))) {
    std::rotate(o.begin(), o.begin() + 1, o.end());
    if (++tries > o.size())
      throw HostFault(
          "all decoder slots are protected; original allocator loops");
  }
  const auto id = o.front();
  auto &p = slot(id);
  o.erase(o.begin());
  if (p.owner)
    track(p.owner).slot = 0;
  o.push_back(id);
  p.tick = SLOT_NEW_TICK;
  p.joints[0].pos[0] = ZERO;
  p.joints[0].pos[1] = ZERO;
  p.owner = 0;
  return id;
}
SlotId Context::animtrack_decode(TrackId id) {
  validate();
  auto &t = track(id);
  auto selected = t.slot;
  if (!selected) {
    selected = alloc();
    slot(selected).owner = id;
    t.slot = selected;
  }
  touch(selected);
  auto &p = slot(selected);
  const auto target = to_int(t.time);
  if (target < p.tick) {
    // Null is an explicit supplied-state refusal. A mutated joint count still
    // follows the original loop, including its partial writes before joint64.
    if (!t.clip)
      throw Unsupported("restart requires a clip");
    const auto &c = clip(t.clip);
    p.tick = 0;
    std::uint64_t bit = 0;
    for (std::int32_t j = 0; j < c.joint_count; ++j) {
      if (j >= static_cast<std::int32_t>(MAX_JOINTS))
        throw HostFault("joint outside fixed decoder slot");
      auto &r = p.joints[static_cast<std::size_t>(j)];
      r.pt = r.rt = 0;
      auto width = unsigned_field(c, bit, 4) + 1;
      bit += 4;
      if ((t.flags & ROOT) && j == 0) {
        bit += 2 * width;
      } else {
        r.pos[0] = scaled_field(c, bit, width, K_POS);
        r.pos[1] = scaled_field(c, bit + width, width, K_POS);
        bit += 2 * width;
      }
      r.pos[2] = scaled_field(c, bit, width, K_POS);
      bit += width;
      width = unsigned_field(c, bit, 4) + 1;
      bit += 4;
      r.quat[3] = scaled_field(c, bit, width, K_QUAT);
      r.quat[0] = scaled_field(c, bit + width, width, K_QUAT);
      r.quat[1] = scaled_field(c, bit + 2 * width, width, K_QUAT);
      r.quat[2] = scaled_field(c, bit + 3 * width, width, K_QUAT);
      bit += 4 * width;
      r.vel.fill(ZERO);
      r.qvel.fill(ZERO);
    }
    p.cursor = bit;
    p.clip = t.clip;
    p.yaw = yaw(p.joints[0].quat);
  }
  if (!p.clip)
    throw HostFault("decoder slot without a stream");
  const auto &c = clip(p.clip);
  std::size_t work = 0;
  for (;;) {
    // A work unit begins before the next original delta read. If refused,
    // every store from previously executed records remains visible.
    if (work++ >= state_->limits.key_records)
      throw ResourceLimit("decoder key work exceeded");
    auto bit = p.cursor;
    const auto tick = add_tick(p.tick, unsigned_field(c, bit, 8));
    if (target < tick)
      return selected;
    p.tick = tick;
    const auto position = unsigned_field(c, bit + 8, 1);
    auto &r = p.joints[unsigned_field(c, bit + 9, 6)];
    const auto width = unsigned_field(c, bit + 15, 4) + 1;
    bit += 19;
    if (position) {
      const auto dt =
          from_int(static_cast<std::uint32_t>(subtract_tick(tick, r.pt)));
      const std::array<Raw, 3> d{mul(r.vel[0], dt), mul(r.vel[1], dt),
                                 mul(r.vel[2], dt)};
      for (std::size_t k = 0; k < 3; ++k)
        r.pos[k] = add(r.pos[k], d[k]);
      r.vel[0] = scaled_field(c, bit, width, K_PVEL);
      r.vel[1] = scaled_field(c, bit + width, width, K_PVEL);
      r.vel[2] = scaled_field(c, bit + 2 * width, width, K_PVEL);
      r.pt = s16(static_cast<std::uint32_t>(tick));
      bit += 3 * width;
    } else {
      const auto dt =
          from_int(static_cast<std::uint32_t>(subtract_tick(tick, r.rt)));
      for (std::size_t k = 0; k < 4; ++k)
        r.quat[k] = add(r.quat[k], mul(r.qvel[k], dt));
      r.qvel[3] = scaled_field(c, bit, width, K_QVEL);
      r.qvel[0] = scaled_field(c, bit + width, width, K_QVEL);
      r.qvel[1] = scaled_field(c, bit + 2 * width, width, K_QVEL);
      r.qvel[2] = scaled_field(c, bit + 3 * width, width, K_QVEL);
      r.rt = s16(static_cast<std::uint32_t>(tick));
      bit += 4 * width;
    }
    p.cursor = bit;
  }
}
std::array<Raw, 3> Context::joint_pos(SlotId id, std::int64_t joint,
                                      Raw time) const {
  validate();
  const auto &r = slot(id).joints[joint_index(joint)];
  const auto dt = sub(time, from_int(static_cast<std::uint32_t>(r.pt)));
  std::array<Raw, 3> result{};
  for (std::size_t k = 0; k < 3; ++k)
    result[k] = add(r.pos[k], mul(r.vel[k], dt));
  return result;
}
std::array<Raw, 4> Context::joint_quat(SlotId id, std::int64_t joint,
                                       Raw time) const {
  validate();
  const auto &r = slot(id).joints[joint_index(joint)];
  const auto dt = sub(time, from_int(static_cast<std::uint32_t>(r.rt)));
  std::array<Raw, 4> result{};
  for (std::size_t k = 0; k < 4; ++k)
    result[k] = add(r.quat[k], mul(r.qvel[k], dt));
  return result;
}
std::vector<JointValues> Context::decode_clip(ClipId id, Raw time,
                                              std::uint32_t flags) {
  (void)clip(id);
  Track t;
  t.clip = id;
  t.flags = flags;
  t.time = time;
  const auto temporary = add_track(t);
  const auto decoded = animtrack_decode(temporary);
  std::vector<JointValues> result;
  const auto count = clip(id).joint_count;
  if (count > 0)
    result.reserve(static_cast<std::size_t>(
        std::min(count, static_cast<std::int32_t>(MAX_JOINTS))));
  for (std::int32_t j = 0; j < count; ++j)
    result.push_back(
        {joint_pos(decoded, j, time), joint_quat(decoded, j, time)});
  return result;
}
std::vector<JointValues> decode_clip(const Clip &value, Raw time,
                                     std::uint32_t flags, Limits limits) {
  Context context(2, limits);
  return context.decode_clip(context.add_clip(value), time, flags);
}
} // namespace champions::simulation::rta::animation_decoder
