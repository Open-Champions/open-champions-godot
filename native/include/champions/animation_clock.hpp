#pragma once

#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace champions::simulation::rta::animation_clock {

inline constexpr std::uint16_t LOOP = 0x0001, PURGE_BELOW = 0x0002,
                               REPORT_END = 0x0004, TALK = 0x0020,
                               FIXED_STEP = 0x0040, EASE = 0x0100,
                               KEEP = 0x0200, ROOT = 0x0800,
                               PERSISTENT = 0x1000, PAUSED = 0x2000,
                               YAW = 0x4000, PROTECTED = ROOT | YAW,
                               STATUS_ENDED = 0x0080, STATUS_TOP_ENDED = 0x0400;
inline constexpr std::int32_t NOT_FOUND = -1000000000;
inline constexpr Raw SIXTY = 0x42700000u, K60 = 0x3c888888u,
                     K_STEP = 0x3f7fffffu, K_RAND = 0x37800000u;
inline constexpr std::uint32_t MAX_LOOP_SUBTRACTIONS = 1048576;

struct ValueError : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};
struct HostFault : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Event {
  Raw seconds = ZERO;
  std::uint8_t mask = 0;
};

// Supplied timing metadata, not a file decoder. Identity is the shared object,
// not its contents. Mutating an event slot affects every alias of this clip.
struct Clip {
  std::int32_t length = 0;
  std::uint64_t joint_mask = 0;
  std::array<Event, 4> events{};
  bool has_sound_table = false;
};
using ClipPtr = std::shared_ptr<Clip>;

struct Track {
  ClipPtr clip{};
  // Opaque cache tokens exist only to make unsupported input observable.
  // They are never dereferenced; every operation refuses nonzero tokens.
  std::uint64_t slot = 0;
  Raw weight = ZERO, rate = ZERO, life = ZERO, life_rate = ZERO, time = ZERO;
  // Physical storage is u16, but the supplied Python model permits raw32
  // assignments. Only modeled masking operations normalize these values.
  std::uint32_t flags = 0;
  std::int16_t sound_idx = 0;
};

struct AnimState {
  explicit AnimState(std::int32_t capacity = 8);
  std::uint64_t pose = 0;
  std::int32_t count = 0;
  std::int32_t capacity;
  // Preserve supplied raw32 flags until a modeled u16 write normalizes them.
  std::uint32_t flags = 0;
  // All cells, including inactive stale values, are part of the state. Caller
  // mutation is supported only with valid count/capacity and cache-free cells.
  std::vector<Track> tracks;
  Track *top();
  const Track *top() const;
};

struct Limits {
  std::uint32_t max_loop_subtractions = 65536;
};
struct AnimSystem {
  std::uint32_t sim_frame = 0;
  std::uint32_t cosmetic_seed = 0;
  Limits limits{};
};

// These functions use the caller's current core FPU context. No host floating
// point, decoder/pose pool, sound service, or gameplay event dispatch is used.
// Native policy rejects unsupported features and malformed public state before
// mutation. Model HostFault and LOOP work exhaustion retain the executed
// prefix. Returned Track pointers refer to fixed cells until caller
// resizes/destroys the state; their contents can move/change during later
// lifecycle operations.
void anim_clear(AnimState &state);
Track *anim_push(AnimState &state, ClipPtr clip, std::uint32_t flags);
Track *anim_push_with_weights(AnimState &state, ClipPtr clip,
                              std::uint32_t flags, Raw weight, Raw rate,
                              Raw life, Raw life_rate);
Track *anim_push_chained(AnimState &state, ClipPtr clip, std::uint32_t flags);
Track *anim_start(Raw blend_in, AnimState &state, ClipPtr clip,
                  std::uint32_t flags);
Track *anim_start_blend_in_out(Raw blend_in, Raw blend_out, AnimState &state,
                               ClipPtr clip, std::uint32_t flags);
void anim_release_caches(AnimState &state);
void anim_state_dtor(AnimState &state);
void anim_stop(Raw blend_out, AnimState &state, ClipPtr clip);
void anim_drop_non_persistent(AnimState &state);
void anim_sync_to_sim_frame(AnimSystem &system, AnimState &state);
void anim_randomize_top_time(AnimSystem &system, AnimState &state);
void anim_sync_top_to_below(AnimSystem &system, AnimState &state);
void anim_jump_top_to_end(AnimSystem &system, AnimState &state);
void anim_snap_top_to_end(AnimState &state);
Track *anim_find_track(AnimState *state, ClipPtr clip);
std::int32_t anim_find_track_by_flag(const AnimState &state,
                                     std::uint32_t mask);
void anim_fade_in_track(Raw blend_in, AnimState &state, ClipPtr clip);
void anim_fade_out_track(Raw blend_out, AnimState &state, ClipPtr clip);
void anim_set_track_weight(Raw weight, AnimState &state, ClipPtr clip);
AnimState anim_state_copy(AnimSystem &system, const AnimState &source);
// All three feature switches must be false. Events are returned as a signed
// byte widened to int32. One aggregate subtraction budget spans all tracks.
std::int32_t anim_advance(AnimSystem &system, Raw speed, AnimState &state,
                          bool sound = false, bool want_root = false,
                          bool want_yaw = false);

} // namespace champions::simulation::rta::animation_clock
