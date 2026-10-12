// Original implementation of the published executable Python contract:
// ref_anim.py f516ac798bf28375554fe53ffb31b74802844136, SHA256
// 6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8;
// animation.md f2b996dded898f1b0b558b901cab875f775cfdc7.
// Lifecycle fields live exclusively in the existing decoder Context cells.
#include "champions/animation_runtime.hpp"
#include "champions/core_math.hpp"
#include <utility>

namespace champions::simulation::rta::animation_runtime {
namespace {
constexpr Raw SIXTY = 0x42700000u, K60 = 0x3c888888u, K_STEP = 0x3f7fffffu,
              K_RAND = 0x37800000u;

void supported_flags(std::uint32_t flags) {
  if (flags & PROTECTED)
    throw Unsupported("ROOT/YAW execution is outside animation runtime");
}
void check_limits(const Limits &limits) {
  if (limits.states > MAX_STATES || limits.state_cells > MAX_STATE_CELLS ||
      limits.max_loop_subtractions > MAX_LOOP_SUBTRACTIONS)
    throw ResourceLimit("animation runtime limits exceed hard caps");
}
Raw blend_rate(Raw seconds) {
  return lt(seconds, K60) ? ONE : div(ONE, mul(seconds, SIXTY));
}
} // namespace

struct Runtime::Storage {
  Limits limits;
  animation_decoder::Context decoder;
  std::vector<std::unique_ptr<AnimState>> states;
  Storage(std::size_t slots, Limits supplied)
      : limits(supplied), decoder(slots, supplied.decoder) {}
};

Runtime::Runtime(std::size_t slot_count, Limits limits) {
  check_limits(limits);
  storage_ = std::make_unique<Storage>(slot_count, limits);
}
Runtime::~Runtime() = default;
std::uint32_t Runtime::sim_frame() const noexcept {
  return storage_->decoder.sim_frame;
}
void Runtime::set_sim_frame(std::uint32_t value) noexcept {
  storage_->decoder.sim_frame = value;
}
std::uint32_t Runtime::cosmetic_seed() const noexcept {
  return storage_->decoder.cosmetic_seed;
}
void Runtime::set_cosmetic_seed(std::uint32_t value) noexcept {
  storage_->decoder.cosmetic_seed = value;
}
Clip &Runtime::clip(ClipId id) { return storage_->decoder.clip(id); }
const Clip &Runtime::clip(ClipId id) const {
  return std::as_const(storage_->decoder).clip(id);
}
Track &Runtime::track(TrackId id) { return storage_->decoder.track(id); }
const Track &Runtime::track(TrackId id) const {
  return std::as_const(storage_->decoder).track(id);
}
Slot &Runtime::slot(SlotId id) { return storage_->decoder.slot(id); }
const Slot &Runtime::slot(SlotId id) const {
  return std::as_const(storage_->decoder).slot(id);
}
std::vector<SlotId> &Runtime::order() noexcept {
  return storage_->decoder.order();
}
const std::vector<SlotId> &Runtime::order() const noexcept {
  return std::as_const(storage_->decoder).order();
}
const AnimState &Runtime::state(StateId id) const {
  if (!id || id > storage_->states.size())
    throw Unsupported("unknown animation state identity");
  return *storage_->states[static_cast<std::size_t>(id - 1)];
}
void Runtime::set_state_scalars(StateId id, std::uint64_t pose,
                                std::int32_t count, std::uint32_t flags) {
  auto &value = const_cast<AnimState &>(state(id));
  if (count < 0 || count > value.capacity)
    throw ValueError("animation count outside fixed capacity");
  value.pose = pose;
  value.count = count;
  value.flags = flags;
}

void Runtime::validate() const {
  // Snapshot enforces decoder-owned allocation bounds without normalizing any
  // supplied references. Validate their domains here before lifecycle accesses.
  const auto context = storage_->decoder.snapshot();
  if (context.order.size() != context.slots.size())
    throw Unsupported("decoder LRU shape differs from pool");
  std::vector<bool> seen(context.slots.size(), false);
  for (const auto id : context.order) {
    if (!id || id > context.slots.size() || seen[id - 1])
      throw Unsupported("decoder LRU is not a pool permutation");
    seen[id - 1] = true;
  }
  for (std::size_t i = 0; i < context.slots.size(); ++i) {
    const auto &value = context.slots[i];
    if (value.index != i || value.clip > context.clips.size() ||
        value.owner > context.tracks.size())
      throw Unsupported("decoder slot has foreign identity");
  }
  for (const auto &entry : context.tracks) {
    supported_flags(entry.value.flags);
    if (entry.value.clip > context.clips.size() ||
        entry.value.slot > context.slots.size())
      throw Unsupported("decoder track has foreign identity");
  }
  for (const auto &entry : storage_->states) {
    if (entry->pose)
      throw Unsupported("pose execution is outside animation runtime");
    supported_flags(entry->flags);
  }
  // No reciprocity check: stale owner links and copied slot references are
  // original observable state, including cells outside every active range.
}
const AnimState &Runtime::checked_state(StateId id) const {
  validate();
  return state(id);
}
AnimState &Runtime::checked_state(StateId id) {
  return const_cast<AnimState &>(std::as_const(*this).checked_state(id));
}
void Runtime::check_clip(ClipId id) const {
  if (id)
    (void)clip(id);
}
ClipId Runtime::add_clip(const std::vector<std::uint8_t> &data,
                         const std::string &name) {
  validate();
  return storage_->decoder.add_clip(data, name);
}
ClipId Runtime::add_clip(const Clip &value) {
  validate();
  return storage_->decoder.add_clip(value);
}
TrackId Runtime::add_track(const Track &value) {
  validate();
  supported_flags(value.flags);
  return storage_->decoder.add_track(value);
}
StateId Runtime::add_state(std::int32_t capacity) {
  validate();
  if (capacity < 1 || capacity > 127)
    throw ValueError("capacity is a positive signed byte");
  const auto &limits = storage_->limits;
  const auto count = static_cast<std::size_t>(capacity);
  if (storage_->states.size() >= limits.states)
    throw ResourceLimit("animation state identities exhausted");
  std::size_t cells = 0;
  for (const auto &value : storage_->states)
    cells += value->tracks.size();
  if (cells > limits.state_cells || count > limits.state_cells - cells)
    throw ResourceLimit("animation state metadata cells exhausted");
  const auto context = storage_->decoder.snapshot();
  if (context.tracks.size() > limits.decoder.tracks ||
      count > limits.decoder.tracks - context.tracks.size())
    throw ResourceLimit("animation state needs more canonical track cells");
  // All deterministic capacity checks precede creation of the first cell.
  // Reserve metadata first. A real host allocation failure during add_track
  // retains any created Context cells; no partial StateID is published.
  auto value = std::make_unique<AnimState>();
  value->capacity = capacity;
  value->tracks.reserve(count);
  storage_->states.reserve(storage_->states.size() + 1);
  for (std::size_t i = 0; i < count; ++i)
    value->tracks.push_back(storage_->decoder.add_track());
  storage_->states.push_back(std::move(value));
  return storage_->states.size();
}
Snapshot Runtime::snapshot() const {
  Snapshot result;
  result.limits = storage_->limits;
  result.states.reserve(storage_->states.size());
  for (std::size_t i = 0; i < storage_->states.size(); ++i)
    result.states.push_back({i + 1, *storage_->states[i]});
  result.decoder = storage_->decoder.snapshot();
  return result;
}
SlotId Runtime::alloc() {
  validate();
  return storage_->decoder.alloc();
}
void Runtime::touch(SlotId id) {
  validate();
  storage_->decoder.touch(id);
}
SlotId Runtime::animtrack_decode(TrackId id) {
  validate();
  return storage_->decoder.animtrack_decode(id);
}
std::array<Raw, 3> Runtime::joint_pos(SlotId id, std::int64_t joint,
                                      Raw time) const {
  validate();
  return storage_->decoder.joint_pos(id, joint, time);
}
std::array<Raw, 4> Runtime::joint_quat(SlotId id, std::int64_t joint,
                                       Raw time) const {
  validate();
  return storage_->decoder.joint_quat(id, joint, time);
}
std::vector<JointValues> Runtime::decode_clip(ClipId id, Raw time,
                                              std::uint32_t flags) {
  validate();
  supported_flags(flags);
  return storage_->decoder.decode_clip(id, time, flags);
}

Raw Runtime::length(const Track &value) const {
  if (!value.clip)
    throw HostFault("animation track has no clip");
  return from_int(static_cast<std::uint32_t>(clip(value.clip).length));
}
void Runtime::release_slot(Track &value) {
  if (value.slot) {
    slot(value.slot).owner = 0;
    value.slot = 0;
  }
}
TrackId Runtime::new_track(AnimState &value, std::int32_t index,
                           ClipId selected, std::uint32_t flags,
                           bool reset_sound, Raw time) {
  value.count = index + 1;
  value.flags = (value.flags | flags) & 0xffffu;
  const auto id = value.tracks[static_cast<std::size_t>(index)];
  auto &cell = track(id);
  const auto stale = cell.slot;
  cell.clip = selected;
  cell.time = time;
  if (reset_sound)
    cell.sound_idx = 0;
  if (stale) {
    slot(stale).owner = 0;
    cell.slot = 0;
  }
  cell.flags = flags & 0xffffu;
  return id;
}
std::int32_t Runtime::start_index(AnimState &value) {
  auto index = value.count;
  if (index >= value.capacity) {
    --index;
    release_slot(track(value.tracks[static_cast<std::size_t>(index)]));
  }
  return index;
}
void Runtime::release_caches(AnimState &value) {
  for (std::int32_t i = 0; i < value.count; ++i)
    release_slot(track(value.tracks[static_cast<std::size_t>(i)]));
}
void Runtime::compact(AnimState &value, std::int32_t boundary) {
  std::int32_t out = 0;
  for (std::int32_t k = 0; k < value.count; ++k) {
    auto &source = track(value.tracks[static_cast<std::size_t>(k)]);
    if ((boundary < 0 || k < boundary) && !(source.flags & PERSISTENT)) {
      release_slot(source);
      continue;
    }
    const auto destination = value.tracks[static_cast<std::size_t>(out)];
    auto &cell = track(destination);
    if (out != k)
      cell = source;
    if (source.slot && out != k) {
      source.slot = 0;
      slot(cell.slot).owner = destination;
    }
    ++out;
  }
  value.count = out;
}
void Runtime::remove_track(AnimState &value, std::int32_t index) {
  release_slot(track(value.tracks[static_cast<std::size_t>(index)]));
  for (auto j = index; j < value.count - 1; ++j) {
    const auto destination = value.tracks[static_cast<std::size_t>(j)];
    auto &cell = track(destination);
    cell = track(value.tracks[static_cast<std::size_t>(j + 1)]);
    if (cell.slot)
      slot(cell.slot).owner = destination;
  }
  track(value.tracks[static_cast<std::size_t>(value.count - 1)]).slot = 0;
  --value.count;
}

void Runtime::anim_clear(StateId id) { checked_state(id).count = 0; }
TrackId Runtime::anim_push(StateId id, ClipId selected, std::uint32_t flags) {
  auto &value = checked_state(id);
  check_clip(selected);
  supported_flags(flags);
  if (!selected || value.count >= value.capacity)
    return 0;
  const auto cell_id = new_track(value, value.count, selected, flags);
  auto &cell = track(cell_id);
  cell.rate = ZERO;
  cell.weight = ONE;
  cell.life = ONE;
  cell.life_rate = ZERO;
  return cell_id;
}
TrackId Runtime::anim_push_with_weights(StateId id, ClipId selected,
                                        std::uint32_t flags, Raw weight,
                                        Raw rate, Raw life, Raw life_rate) {
  auto &value = checked_state(id);
  check_clip(selected);
  supported_flags(flags);
  if (!selected || value.count >= value.capacity)
    return 0;
  const auto cell_id = new_track(value, value.count, selected, flags);
  auto &cell = track(cell_id);
  cell.weight = weight;
  cell.rate = rate;
  cell.life = life;
  cell.life_rate = life_rate;
  return cell_id;
}
TrackId Runtime::anim_push_chained(StateId id, ClipId selected,
                                   std::uint32_t flags) {
  auto &value = checked_state(id);
  check_clip(selected);
  supported_flags(flags);
  if (!selected)
    return 0;
  if (value.count < 1)
    throw HostFault("anim_pushChained on an empty state reads before array");
  const auto &below =
      track(value.tracks[static_cast<std::size_t>(value.count - 1)]);
  const auto span = length(below);
  if (value.count >= value.capacity)
    return 0;
  const auto start = add(sub(below.time, span), TWO);
  const auto cell_id =
      new_track(value, value.count, selected, flags, false, start);
  auto &cell = track(cell_id);
  cell.life_rate = ZERO;
  cell.rate = ZERO;
  cell.life = ONE;
  cell.weight = ONE;
  return cell_id;
}
TrackId Runtime::anim_start(Raw blend_in, StateId id, ClipId selected,
                            std::uint32_t flags) {
  auto &value = checked_state(id);
  check_clip(selected);
  supported_flags(flags);
  if (!selected)
    return 0;
  const auto index = start_index(value);
  if (index == 0)
    blend_in = ZERO;
  const auto cell_id = new_track(value, index, selected, flags);
  auto &cell = track(cell_id);
  cell.weight = ZERO;
  cell.rate = blend_rate(blend_in);
  cell.life = ONE;
  cell.life_rate = ZERO;
  return cell_id;
}
TrackId Runtime::anim_start_blend_in_out(Raw blend_in, Raw blend_out,
                                         StateId id, ClipId selected,
                                         std::uint32_t flags) {
  auto &value = checked_state(id);
  check_clip(selected);
  supported_flags(flags);
  if (!selected)
    return 0;
  const auto cell_id = new_track(value, start_index(value), selected, flags);
  auto &cell = track(cell_id);
  cell.weight = ZERO;
  cell.rate = blend_rate(blend_in);
  const auto out_rate = blend_rate(blend_out);
  cell.life_rate = neg(out_rate);
  cell.life = mul(mul(mul(length(cell), K60), SIXTY), out_rate);
  return cell_id;
}
void Runtime::anim_release_caches(StateId id) {
  release_caches(checked_state(id));
}
void Runtime::anim_state_dtor(StateId id) {
  auto &value = checked_state(id);
  for (const auto cell : value.tracks)
    release_slot(track(cell));
}
void Runtime::anim_stop(Raw blend_out, StateId id, ClipId selected) {
  auto &value = checked_state(id);
  check_clip(selected);
  if (!selected || value.count <= 0)
    return;
  std::int32_t i = 0;
  while (i < value.count) {
    auto &cell = track(value.tracks[static_cast<std::size_t>(i)]);
    if (cell.clip == selected) {
      if (eq(blend_out, ZERO)) {
        release_caches(value);
        for (auto j = i; j < value.count - 1; ++j)
          track(value.tracks[static_cast<std::size_t>(j)]) =
              track(value.tracks[static_cast<std::size_t>(j + 1)]);
        --value.count;
        continue;
      }
      if (lt(ONE, cell.weight))
        cell.weight = ONE;
      const auto life = cell.life;
      cell.rate = neg(blend_rate(blend_out));
      if (lt(ONE, life))
        cell.life = ONE;
      cell.life_rate = ZERO;
      cell.flags &= 0xeffbu;
    }
    ++i;
  }
}
void Runtime::anim_drop_non_persistent(StateId id) {
  compact(checked_state(id), -1);
}
void Runtime::anim_sync_to_sim_frame(StateId id) {
  auto &value = checked_state(id);
  for (std::int32_t i = 0; i < value.count; ++i) {
    auto &cell = track(value.tracks[static_cast<std::size_t>(i)]);
    const auto frame = mul(mul(from_int(sim_frame()), K60), SIXTY);
    cell.time = fmodf(frame, length(cell));
  }
}
void Runtime::anim_randomize_top_time(StateId id) {
  auto &value = checked_state(id);
  if (!value.count)
    return;
  auto &cell = track(value.tracks[static_cast<std::size_t>(value.count - 1)]);
  auto &seed = storage_->decoder.cosmetic_seed;
  seed = seed * 0x19660du + 0x3c6ef35fu;
  const auto draw = seed >> 16;
  cell.time = mul(mul(from_int(draw), K_RAND), length(cell));
}
void Runtime::anim_sync_top_to_below(StateId id) {
  auto &value = checked_state(id);
  if (value.count < 2)
    return;
  auto &cell = track(value.tracks[static_cast<std::size_t>(value.count - 1)]);
  cell.time =
      track(value.tracks[static_cast<std::size_t>(value.count - 2)]).time;
  if (le(length(cell), cell.time))
    cell.time = ZERO;
}
void Runtime::anim_jump_top_to_end(StateId id) {
  auto &value = checked_state(id);
  if (value.count) {
    auto &cell = track(value.tracks[static_cast<std::size_t>(value.count - 1)]);
    cell.time = length(cell);
  }
}
void Runtime::anim_snap_top_to_end(StateId id) {
  auto &value = checked_state(id);
  if (value.count) {
    auto &cell = track(value.tracks[static_cast<std::size_t>(value.count - 1)]);
    cell.life = ONE;
    cell.weight = ONE;
    cell.time = length(cell);
  }
}
TrackId Runtime::anim_find_track(StateId id, ClipId selected) const {
  if (!id)
    return 0;
  const auto &value = checked_state(id);
  check_clip(selected);
  if (!selected)
    return 0;
  for (std::int32_t i = 0; i < value.count; ++i) {
    const auto cell_id = value.tracks[static_cast<std::size_t>(i)];
    if (track(cell_id).clip == selected)
      return cell_id;
  }
  return 0;
}
std::int32_t Runtime::anim_find_track_by_flag(StateId id,
                                              std::uint32_t mask) const {
  const auto &value = checked_state(id);
  for (std::int32_t i = 0; i < value.count; ++i)
    if (static_cast<std::uint32_t>(
            s16(track(value.tracks[static_cast<std::size_t>(i)]).flags)) &
        mask)
      return i;
  return NOT_FOUND;
}
void Runtime::anim_fade_in_track(Raw blend_in, StateId id, ClipId selected) {
  const auto cell_id = anim_find_track(id, selected);
  if (!cell_id)
    return;
  auto &cell = track(cell_id);
  const auto weight = fmin(cell.weight, cell.life);
  cell.weight = lt(weight, ZERO) ? ZERO : weight;
  cell.rate = blend_rate(blend_in);
  cell.life = ONE;
}
void Runtime::anim_fade_out_track(Raw blend_out, StateId id, ClipId selected) {
  const auto cell_id = anim_find_track(id, selected);
  if (!cell_id)
    return;
  auto &cell = track(cell_id);
  const auto weight = fmin(cell.weight, cell.life);
  cell.weight = lt(ONE, weight) ? ONE : weight;
  cell.life = ZERO;
  cell.rate = neg(blend_rate(blend_out));
}
void Runtime::anim_set_track_weight(Raw weight, StateId id, ClipId selected) {
  const auto cell_id = anim_find_track(id, selected);
  if (!cell_id)
    return;
  auto &cell = track(cell_id);
  cell.weight = weight;
  cell.life = ONE;
  cell.life_rate = ZERO;
  cell.rate = ZERO;
}
StateId Runtime::anim_state_copy(StateId source) {
  const auto &original = checked_state(source);
  const auto id = add_state(original.capacity);
  auto &value = const_cast<AnimState &>(state(id));
  value.count = original.count;
  value.flags = original.flags;
  for (std::size_t i = 0; i < original.tracks.size(); ++i) {
    auto &cell = track(value.tracks[i]);
    cell = track(original.tracks[i]);
    cell.slot = 0;
  }
  return id;
}

std::int32_t Runtime::anim_advance(Raw speed, StateId id, bool sound,
                                   bool want_root, bool want_yaw) {
  auto &value = checked_state(id);
  if (sound || want_root || want_yaw)
    throw Unsupported("sound/root/yaw output is outside animation runtime");
  auto remaining = storage_->limits.max_loop_subtractions;
  const auto step = mul(speed, K_STEP);
  Raw left = ONE;
  std::uint32_t events = 0, flag_union = 0;
  for (std::int32_t i = value.count - 1; i >= 0; --i) {
    auto &cell = track(value.tracks[static_cast<std::size_t>(i)]);
    // The original reads clip.events here; earlier lifecycle operations may
    // legitimately leave active cells with a null clip.
    if (!cell.clip)
      throw HostFault("animation advance track has no clip");
    const auto &selected = clip(cell.clip);
    const auto flags = cell.flags;
    const auto old_time = cell.time;
    flag_union |= static_cast<std::uint32_t>(s16(flags));
    const auto low = mul(old_time, K60);
    const auto high = add(low, mul(step, K60));
    for (const auto &event : selected.events)
      if (le(low, event.seconds) && lt(event.seconds, high))
        events |= event.mask;
    auto time = old_time;
    if (!(flags & PAUSED)) {
      time = add(old_time, flags & FIXED_STEP ? K_STEP : step);
      cell.time = time;
    }
    if (lt(time, ZERO))
      continue;
    const auto weight = cell.weight;
    auto life = cell.life;
    const auto raw = fmin(weight, life);
    auto w = fmin(raw, left);
    if (lt(ONE, w))
      w = ONE;
    if (lt(w, ZERO))
      w = ZERO;
    (void)w;
    const auto used = lt(raw, ZERO) ? ZERO : raw;
    if (!selected.joint_mask) {
      left = sub(left, used);
      if (lt(left, ZERO))
        left = ZERO;
    }
    const auto span = length(cell);
    if (le(span, time)) {
      if (flags & LOOP) {
        if (selected.length <= 0)
          throw HostFault("looping clip has nonpositive length");
        do {
          if (!remaining)
            throw ResourceLimit("LOOP subtraction work exhausted");
          --remaining;
          time = sub(time, span);
          cell.time = time;
        } while (le(span, time));
        life = cell.life;
      } else if (flags & REPORT_END) {
        cell.time = span;
        flag_union |= STATUS_ENDED;
        if (i == value.count - 1)
          flag_union |= STATUS_TOP_ENDED;
      } else {
        cell.life = ZERO;
        cell.life_rate = ZERO;
        life = ZERO;
      }
    }
    auto rate = cell.rate;
    auto life_rate = cell.life_rate;
    if (!(flags & FIXED_STEP)) {
      rate = mul(rate, speed);
      life_rate = mul(life_rate, speed);
    }
    cell.weight = add(weight, rate);
    cell.life = life = add(life, life_rate);
    if (le(life, ZERO) && !(flags & KEEP)) {
      remove_track(value, i);
    } else if ((flags & PURGE_BELOW) && le(ONE, fmin(cell.weight, life)) &&
               i > 0) {
      compact(value, i);
      break;
    }
  }
  value.flags = flag_union & 0xffffu;
  const auto byte = events & 0xffu;
  return byte < 128 ? static_cast<std::int32_t>(byte)
                    : static_cast<std::int32_t>(byte) - 256;
}

} // namespace champions::simulation::rta::animation_runtime
