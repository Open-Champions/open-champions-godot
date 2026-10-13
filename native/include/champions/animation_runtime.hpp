#pragma once

#include "champions/animation_decoder.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace champions::simulation::rta::animation_runtime {

using StateId = std::uint64_t;
using ClipId = animation_decoder::ClipId;
using TrackId = animation_decoder::TrackId;
using SlotId = animation_decoder::SlotId;
using Clip = animation_decoder::Clip;
using Track = animation_decoder::Track;
using Slot = animation_decoder::Slot;
using JointValues = animation_decoder::JointValues;
using ValueError = animation_decoder::ValueError;
using IndexError = animation_decoder::IndexError;
using HostFault = animation_decoder::HostFault;
using Unsupported = animation_decoder::Unsupported;
using ResourceLimit = animation_decoder::ResourceLimit;

inline constexpr std::uint32_t LOOP = 0x0001, PURGE_BELOW = 0x0002,
                               REPORT_END = 0x0004, TALK = 0x0020,
                               FIXED_STEP = 0x0040, EASE = 0x0100,
                               KEEP = 0x0200, ROOT = 0x0800,
                               PERSISTENT = 0x1000, PAUSED = 0x2000,
                               YAW = 0x4000, PROTECTED = ROOT | YAW,
                               STATUS_ENDED = 0x0080, STATUS_TOP_ENDED = 0x0400;
inline constexpr std::int32_t NOT_FOUND = -1000000000;
inline constexpr std::size_t MAX_STATES = 1024, MAX_STATE_CELLS = 65536;
inline constexpr std::uint32_t MAX_LOOP_SUBTRACTIONS = 1048576;

struct Limits {
  animation_decoder::Limits decoder{};
  std::size_t states = MAX_STATES;
  std::size_t state_cells = MAX_STATE_CELLS;
  std::uint32_t max_loop_subtractions = 65536;
};

// Cell membership never changes. Field copies change the canonical decoder
// cells, not these identities. Raw supplied flags remain raw32 until a modeled
// masking store. Pose tokens are observable supplied input, never executable.
struct AnimState {
  std::uint64_t pose = 0;
  std::int32_t count = 0, capacity = 0;
  std::uint32_t flags = 0;
  std::vector<TrackId> tracks{};
};
struct StateSnapshot {
  StateId id = 0;
  AnimState value;
};
struct Snapshot {
  Limits limits;
  std::vector<StateSnapshot> states;
  animation_decoder::Snapshot decoder;
};

// One owned decoder Context is the sole clip/track/slot/frame/seed authority.
// No clock-track mirrors, external lifetimes, pose or sound execution. All
// operations use the caller's core FPU. IDs are monotonic, zero is null.
class Runtime {
public:
  explicit Runtime(std::size_t slot_count = 165, Limits limits = {});
  ~Runtime();
  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;
  Runtime(Runtime &&) = delete;
  Runtime &operator=(Runtime &&) = delete;

  std::uint32_t sim_frame() const noexcept;
  void set_sim_frame(std::uint32_t value) noexcept;
  std::uint32_t cosmetic_seed() const noexcept;
  void set_cosmetic_seed(std::uint32_t value) noexcept;
  ClipId add_clip(const std::vector<std::uint8_t> &data,
                  const std::string &name = {});
  ClipId add_clip(const Clip &value);
  TrackId add_track(const Track &value = {});
  StateId add_state(std::int32_t capacity = 8);
  const AnimState &state(StateId id) const;
  // Supplied-state setter: valid count only; stores raw flags and pose tokens
  // verbatim. Executing operations refuse unsupported pose/ROOT/YAW inputs.
  // Capacity and cell membership cannot be changed through this API.
  void set_state_scalars(StateId id, std::uint64_t pose, std::int32_t count,
                         std::uint32_t flags);
  Clip &clip(ClipId id);
  const Clip &clip(ClipId id) const;
  Track &track(TrackId id);
  const Track &track(TrackId id) const;
  Slot &slot(SlotId id);
  const Slot &slot(SlotId id) const;
  std::vector<SlotId> &order() noexcept;
  const std::vector<SlotId> &order() const noexcept;
  Snapshot snapshot() const;

  SlotId alloc();
  void touch(SlotId id);
  SlotId animtrack_decode(TrackId id);
  std::array<Raw, 3> joint_pos(SlotId id, std::int64_t joint, Raw time) const;
  std::array<Raw, 4> joint_quat(SlotId id, std::int64_t joint, Raw time) const;
  std::vector<JointValues> decode_clip(ClipId id, Raw time,
                                       std::uint32_t flags = 0);

  void anim_clear(StateId id);
  TrackId anim_push(StateId id, ClipId clip, std::uint32_t flags);
  TrackId anim_push_with_weights(StateId id, ClipId clip, std::uint32_t flags,
                                 Raw weight, Raw rate, Raw life, Raw life_rate);
  TrackId anim_push_chained(StateId id, ClipId clip, std::uint32_t flags);
  TrackId anim_start(Raw blend_in, StateId id, ClipId clip,
                     std::uint32_t flags);
  TrackId anim_start_blend_in_out(Raw blend_in, Raw blend_out, StateId id,
                                  ClipId clip, std::uint32_t flags);
  void anim_release_caches(StateId id);
  // Models release only; neither this state nor any cell identity is erased.
  void anim_state_dtor(StateId id);
  void anim_stop(Raw blend_out, StateId id, ClipId clip);
  void anim_drop_non_persistent(StateId id);
  void anim_sync_to_sim_frame(StateId id);
  void anim_randomize_top_time(StateId id);
  void anim_sync_top_to_below(StateId id);
  void anim_jump_top_to_end(StateId id);
  void anim_snap_top_to_end(StateId id);
  TrackId anim_find_track(StateId id, ClipId clip) const;
  std::int32_t anim_find_track_by_flag(StateId id, std::uint32_t mask) const;
  void anim_fade_in_track(Raw blend_in, StateId id, ClipId clip);
  void anim_fade_out_track(Raw blend_out, StateId id, ClipId clip);
  void anim_set_track_weight(Raw weight, StateId id, ClipId clip);
  StateId anim_state_copy(StateId source);
  // Signed-byte event result widened to int32. All feature switches must be
  // false. LOOP work is aggregate across the call; refusal retains every
  // already executed model store, including completed wrap subtractions.
  std::int32_t anim_advance(Raw speed, StateId id, bool sound = false,
                            bool want_root = false, bool want_yaw = false);

private:
  struct Storage;
  std::unique_ptr<Storage> storage_;
  void validate() const;
  AnimState &checked_state(StateId id);
  const AnimState &checked_state(StateId id) const;
  void check_clip(ClipId id) const;
  Raw length(const Track &value) const;
  void release_slot(Track &value);
  TrackId new_track(AnimState &value, std::int32_t index, ClipId clip,
                    std::uint32_t flags, bool reset_sound = true,
                    Raw time = ZERO);
  std::int32_t start_index(AnimState &value);
  void release_caches(AnimState &value);
  void compact(AnimState &value, std::int32_t boundary);
  void remove_track(AnimState &value, std::int32_t index);
};

} // namespace champions::simulation::rta::animation_runtime
