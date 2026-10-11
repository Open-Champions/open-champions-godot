#include "champions/animation_clock.hpp"
#include "champions/core_math.hpp"
#include "champions/core_state.hpp"
#include <utility>

// Authored from tools/ref_anim.py at f516ac798bf28375554fe53ffb31b74802844136
// SHA256 6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8
// and docs/spec/animation.md at f2b996dded898f1b0b558b901cab875f775cfdc7
// SHA256 be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab.
// This is only the supplied-metadata, cache-free lifecycle/clock contract.

namespace champions::simulation::rta::animation_clock {
namespace {

void supported_flags(std::uint32_t flags) {
  if (flags & PROTECTED)
    throw Unsupported(
        "ROOT and YAW tracks require the excluded decoder contract");
}

void validate(const AnimState &state) {
  if (state.capacity < 1 || state.capacity > 127 || state.count < 0 ||
      state.count > state.capacity ||
      state.tracks.size() != static_cast<std::size_t>(state.capacity))
    throw HostFault("invalid animation state count, capacity, or cell array");
  if (state.pose != 0)
    throw Unsupported("pose caches are outside the animation clock contract");
  supported_flags(state.flags);
  for (std::size_t i = 0; i < state.tracks.size(); ++i) {
    const auto &track = state.tracks[i];
    if (track.slot != 0)
      throw Unsupported(
          "decoder caches are outside the animation clock contract");
    supported_flags(track.flags);
    if (i < static_cast<std::size_t>(state.count) && !track.clip)
      throw HostFault("active animation track has no clip");
  }
}

Raw clip_length(const Track &track) {
  return from_int(static_cast<std::uint32_t>(track.clip->length));
}

Raw blend_rate(Raw seconds) {
  return lt(seconds, K60) ? ONE : div(ONE, mul(seconds, SIXTY));
}

Track &new_track(AnimState &state, std::int32_t index, ClipPtr clip,
                 std::uint32_t flags, bool reset_sound = true,
                 Raw time = ZERO) {
  state.count = index + 1;
  state.flags = static_cast<std::uint16_t>(state.flags | flags);
  auto &track = state.tracks[static_cast<std::size_t>(index)];
  track.clip = std::move(clip);
  track.time = time;
  if (reset_sound)
    track.sound_idx = 0;
  track.slot = 0;
  track.flags = static_cast<std::uint16_t>(flags);
  return track;
}

std::int32_t start_index(const AnimState &state) {
  return state.count < state.capacity ? state.count : state.count - 1;
}

// A negative boundary means retain only PERSISTENT; otherwise remove only
// nonpersistent cells below the boundary. Source cells remain stale copies.
void compact(AnimState &state, std::int32_t boundary) {
  std::int32_t out = 0;
  for (std::int32_t k = 0; k < state.count; ++k) {
    auto &source = state.tracks[static_cast<std::size_t>(k)];
    if ((boundary < 0 || k < boundary) && !(source.flags & PERSISTENT))
      continue;
    if (out != k)
      state.tracks[static_cast<std::size_t>(out)] = source;
    ++out;
  }
  state.count = out;
}

void remove_track(AnimState &state, std::int32_t index) {
  for (auto j = index; j < state.count - 1; ++j)
    state.tracks[static_cast<std::size_t>(j)] =
        state.tracks[static_cast<std::size_t>(j + 1)];
  // In the admitted domain this is already zero, but the reference explicitly
  // clears only this field of the vacated cell, retaining everything else.
  state.tracks[static_cast<std::size_t>(state.count - 1)].slot = 0;
  --state.count;
}

} // namespace

AnimState::AnimState(std::int32_t requested_capacity)
    : capacity(requested_capacity) {
  if (capacity < 1 || capacity > 127)
    throw ValueError("capacity is a positive signed byte");
  tracks.resize(static_cast<std::size_t>(capacity));
}

Track *AnimState::top() {
  validate(*this);
  return count > 0 ? &tracks[static_cast<std::size_t>(count - 1)] : nullptr;
}

const Track *AnimState::top() const {
  validate(*this);
  return count > 0 ? &tracks[static_cast<std::size_t>(count - 1)] : nullptr;
}

void anim_clear(AnimState &state) {
  validate(state);
  state.count = 0;
}

Track *anim_push(AnimState &state, ClipPtr clip, std::uint32_t flags) {
  validate(state);
  supported_flags(flags);
  if (!clip || state.count >= state.capacity)
    return nullptr;
  auto &track = new_track(state, state.count, std::move(clip), flags);
  track.rate = ZERO;
  track.weight = ONE;
  track.life = ONE;
  track.life_rate = ZERO;
  return &track;
}

Track *anim_push_with_weights(AnimState &state, ClipPtr clip,
                              std::uint32_t flags, Raw weight, Raw rate,
                              Raw life, Raw life_rate) {
  validate(state);
  supported_flags(flags);
  if (!clip || state.count >= state.capacity)
    return nullptr;
  auto &track = new_track(state, state.count, std::move(clip), flags);
  track.weight = weight;
  track.rate = rate;
  track.life = life;
  track.life_rate = life_rate;
  return &track;
}

Track *anim_push_chained(AnimState &state, ClipPtr clip, std::uint32_t flags) {
  validate(state);
  supported_flags(flags);
  if (!clip)
    return nullptr;
  if (state.count < 1)
    throw HostFault(
        "anim_pushChained on an empty state reads before the array");
  const auto &below = state.tracks[static_cast<std::size_t>(state.count - 1)];
  const auto length = clip_length(below);
  if (state.count >= state.capacity)
    return nullptr;
  // Preserve the executable +2 formula, regardless of the prose's 'after'.
  const auto start = add(sub(below.time, length), TWO);
  auto &track =
      new_track(state, state.count, std::move(clip), flags, false, start);
  track.life_rate = ZERO;
  track.rate = ZERO;
  track.life = ONE;
  track.weight = ONE;
  return &track;
}

Track *anim_start(Raw blend_in, AnimState &state, ClipPtr clip,
                  std::uint32_t flags) {
  validate(state);
  supported_flags(flags);
  if (!clip)
    return nullptr;
  const auto index = start_index(state);
  if (index == 0)
    blend_in = ZERO;
  auto &track = new_track(state, index, std::move(clip), flags);
  track.weight = ZERO;
  track.rate = blend_rate(blend_in);
  track.life = ONE;
  track.life_rate = ZERO;
  return &track;
}

Track *anim_start_blend_in_out(Raw blend_in, Raw blend_out, AnimState &state,
                               ClipPtr clip, std::uint32_t flags) {
  validate(state);
  supported_flags(flags);
  if (!clip)
    return nullptr;
  auto &track = new_track(state, start_index(state), std::move(clip), flags);
  track.weight = ZERO;
  track.rate = blend_rate(blend_in);
  const auto out_rate = blend_rate(blend_out);
  track.life_rate = neg(out_rate);
  track.life = mul(mul(mul(clip_length(track), K60), SIXTY), out_rate);
  return &track;
}

void anim_release_caches(AnimState &state) { validate(state); }
void anim_state_dtor(AnimState &state) { validate(state); }

void anim_stop(Raw blend_out, AnimState &state, ClipPtr clip) {
  validate(state);
  if (!clip || state.count <= 0)
    return;
  std::int32_t i = 0;
  while (i < state.count) {
    auto &track = state.tracks[static_cast<std::size_t>(i)];
    if (track.clip == clip) {
      if (eq(blend_out, ZERO)) {
        // All caches are already null. Unlike advance's removal, stop does not
        // explicitly clear the vacated cell; that distinction is inert here.
        for (auto j = i; j < state.count - 1; ++j)
          state.tracks[static_cast<std::size_t>(j)] =
              state.tracks[static_cast<std::size_t>(j + 1)];
        --state.count;
        continue;
      }
      if (lt(ONE, track.weight))
        track.weight = ONE;
      const auto life = track.life;
      track.rate = neg(blend_rate(blend_out));
      if (lt(ONE, life))
        track.life = ONE;
      track.life_rate = ZERO;
      track.flags &= 0xeffbu;
    }
    ++i;
  }
}

void anim_drop_non_persistent(AnimState &state) {
  validate(state);
  compact(state, -1);
}

void anim_sync_to_sim_frame(AnimSystem &system, AnimState &state) {
  validate(state);
  for (std::int32_t i = 0; i < state.count; ++i) {
    auto &track = state.tracks[static_cast<std::size_t>(i)];
    const auto frame = mul(mul(from_int(system.sim_frame), K60), SIXTY);
    track.time = fmodf(frame, clip_length(track));
  }
}

void anim_randomize_top_time(AnimSystem &system, AnimState &state) {
  validate(state);
  if (state.count == 0)
    return;
  auto &track = state.tracks[static_cast<std::size_t>(state.count - 1)];
  CosmeticRandom random(system.cosmetic_seed);
  const auto draw = random.draw();
  system.cosmetic_seed = random.state;
  track.time = mul(mul(from_int(draw), K_RAND), clip_length(track));
}

void anim_sync_top_to_below(AnimSystem &, AnimState &state) {
  validate(state);
  if (state.count < 2)
    return;
  auto &track = state.tracks[static_cast<std::size_t>(state.count - 1)];
  track.time = state.tracks[static_cast<std::size_t>(state.count - 2)].time;
  if (le(clip_length(track), track.time))
    track.time = ZERO;
}

void anim_jump_top_to_end(AnimSystem &, AnimState &state) {
  validate(state);
  if (state.count > 0) {
    auto &track = state.tracks[static_cast<std::size_t>(state.count - 1)];
    track.time = clip_length(track);
  }
}

void anim_snap_top_to_end(AnimState &state) {
  validate(state);
  if (state.count > 0) {
    auto &track = state.tracks[static_cast<std::size_t>(state.count - 1)];
    track.life = ONE;
    track.weight = ONE;
    track.time = clip_length(track);
  }
}

Track *anim_find_track(AnimState *state, ClipPtr clip) {
  if (!state)
    return nullptr;
  validate(*state);
  if (!clip)
    return nullptr;
  for (std::int32_t i = 0; i < state->count; ++i) {
    auto &track = state->tracks[static_cast<std::size_t>(i)];
    if (track.clip == clip)
      return &track;
  }
  return nullptr;
}

std::int32_t anim_find_track_by_flag(const AnimState &state,
                                     std::uint32_t mask) {
  validate(state);
  for (std::int32_t i = 0; i < state.count; ++i)
    if (static_cast<std::uint32_t>(
            s16(state.tracks[static_cast<std::size_t>(i)].flags)) &
        mask)
      return i;
  return NOT_FOUND;
}

void anim_fade_in_track(Raw blend_in, AnimState &state, ClipPtr clip) {
  auto *track = anim_find_track(&state, std::move(clip));
  if (!track)
    return;
  const auto weight = fmin(track->weight, track->life);
  track->weight = lt(weight, ZERO) ? ZERO : weight;
  track->rate = blend_rate(blend_in);
  track->life = ONE;
}

void anim_fade_out_track(Raw blend_out, AnimState &state, ClipPtr clip) {
  auto *track = anim_find_track(&state, std::move(clip));
  if (!track)
    return;
  const auto weight = fmin(track->weight, track->life);
  track->weight = lt(ONE, weight) ? ONE : weight;
  track->life = ZERO;
  track->rate = neg(blend_rate(blend_out));
}

void anim_set_track_weight(Raw weight, AnimState &state, ClipPtr clip) {
  auto *track = anim_find_track(&state, std::move(clip));
  if (!track)
    return;
  track->weight = weight;
  track->life = ONE;
  track->life_rate = ZERO;
  track->rate = ZERO;
}

AnimState anim_state_copy(AnimSystem &, const AnimState &source) {
  validate(source);
  return source;
}

std::int32_t anim_advance(AnimSystem &system, Raw speed, AnimState &state,
                          bool sound, bool want_root, bool want_yaw) {
  validate(state);
  if (sound || want_root || want_yaw)
    throw Unsupported(
        "sound, root and yaw outputs are outside the clock contract");
  if (system.limits.max_loop_subtractions > MAX_LOOP_SUBTRACTIONS)
    throw ResourceLimit("LOOP subtraction limit exceeds the native hard cap");
  std::uint32_t remaining = system.limits.max_loop_subtractions;
  const auto step = mul(speed, K_STEP);
  Raw left = ONE;
  std::uint32_t events = 0, flag_union = 0;
  for (std::int32_t i = state.count - 1; i >= 0; --i) {
    auto &track = state.tracks[static_cast<std::size_t>(i)];
    const auto clip = track.clip;
    const auto flags = track.flags;
    const auto old_time = track.time;
    flag_union |= static_cast<std::uint32_t>(s16(flags));
    const auto low = mul(old_time, K60);
    const auto high = add(low, mul(step, K60));
    for (const auto &event : clip->events)
      if (le(low, event.seconds) && lt(event.seconds, high))
        events |= event.mask;
    auto time = old_time;
    if (!(flags & PAUSED)) {
      time = add(old_time, flags & FIXED_STEP ? K_STEP : step);
      track.time = time;
    }
    if (lt(time, ZERO))
      continue;

    const auto weight = track.weight;
    auto life = track.life;
    const auto raw = fmin(weight, life);
    // The root weight itself has no observable consumer in this boundary.
    // Preserve the reference's remaining-weight arithmetic and operation order.
    auto w = fmin(raw, left);
    if (lt(ONE, w))
      w = ONE;
    if (lt(w, ZERO))
      w = ZERO;
    (void)w;
    const auto used = lt(raw, ZERO) ? ZERO : raw;
    if (clip->joint_mask == 0) {
      left = sub(left, used);
      if (lt(left, ZERO))
        left = ZERO;
    }

    const auto length = clip_length(track);
    if (le(length, time)) {
      if (flags & LOOP) {
        if (clip->length <= 0)
          throw HostFault("looping clip has nonpositive length");
        do {
          if (remaining == 0)
            throw ResourceLimit("LOOP subtraction work exhausted");
          --remaining;
          time = sub(time, length);
          track.time = time;
        } while (le(length, time));
        life = track.life;
      } else if (flags & REPORT_END) {
        track.time = length;
        flag_union |= STATUS_ENDED;
        if (i == state.count - 1)
          flag_union |= STATUS_TOP_ENDED;
      } else {
        track.life = ZERO;
        track.life_rate = ZERO;
        life = ZERO;
      }
    }
    auto rate = track.rate;
    auto life_rate = track.life_rate;
    if (!(flags & FIXED_STEP)) {
      rate = mul(rate, speed);
      life_rate = mul(life_rate, speed);
    }
    track.weight = add(weight, rate);
    track.life = life = add(life, life_rate);
    if (le(life, ZERO) && !(flags & KEEP)) {
      remove_track(state, i);
    } else if ((flags & PURGE_BELOW) && le(ONE, fmin(track.weight, life)) &&
               i > 0) {
      compact(state, i);
      break;
    }
  }
  state.flags = static_cast<std::uint16_t>(flag_union);
  const auto byte = events & 0xffu;
  return byte < 128 ? static_cast<std::int32_t>(byte)
                    : static_cast<std::int32_t>(byte) - 256;
}

} // namespace champions::simulation::rta::animation_clock
