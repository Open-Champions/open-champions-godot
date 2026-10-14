#pragma once

#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::detail {
struct Checkpoint;
struct CheckpointAccess;
} // namespace champions::simulation::rta::detail
namespace champions::simulation::rta::animation_decoder {

using ClipId = std::uint64_t;
using TrackId = std::uint64_t;
using SlotId = std::uint32_t;
inline constexpr std::uint32_t ROOT = 0x0800, YAW = 0x4000;
inline constexpr std::size_t MAX_JOINTS = 64;
inline constexpr std::int64_t SLOT_NEW_TICK = 1000000;
inline constexpr std::size_t MAX_INPUT_BYTES = 64 * 1024 * 1024;
inline constexpr std::size_t MAX_OWNED_BYTES = 128 * 1024 * 1024;
inline constexpr std::size_t MAX_CLIPS = 1024, MAX_TRACKS = 65536,
                             MAX_SLOTS = 1024, MAX_KEY_RECORDS = 1048576;

struct ValueError : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};
struct IndexError : std::out_of_range {
  using std::out_of_range::out_of_range;
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

struct Limits {
  std::size_t input_bytes = MAX_INPUT_BYTES;
  std::size_t owned_bytes = MAX_OWNED_BYTES;
  std::size_t clips = MAX_CLIPS;
  std::size_t tracks = MAX_TRACKS;
  std::size_t slots = MAX_SLOTS;
  std::size_t key_records = 65536;
};
struct Event {
  Raw seconds = ZERO;
  std::uint8_t mask = 0;
};
struct Clip {
  std::string name;
  std::vector<std::uint8_t> data;
  std::int32_t joint_count = 0, length = 0;
  std::vector<std::array<std::int16_t, 4>> bind;
  std::vector<std::int8_t> depth;
  std::array<Event, 4> events{};
  std::optional<std::vector<std::int32_t>> sound_times;
  std::uint8_t sound_group = 0;
  std::uint64_t joint_mask = 0;
  std::vector<std::uint16_t> words;
  std::uint16_t word(std::int64_t index) const noexcept;
};
Clip parse_clip(const std::vector<std::uint8_t> &data,
                const std::string &name = {}, Limits limits = {});

// Full fixed track cell. IDs are null at zero; all nonzero IDs must belong to
// this Context. Field copies do not repair a slot's owner, matching the model.
struct Track {
  ClipId clip = 0;
  SlotId slot = 0;
  Raw weight = ZERO, rate = ZERO, life = ZERO, life_rate = ZERO, time = ZERO;
  std::uint32_t flags = 0;
  std::int16_t sound_idx = 0;
};
struct Joint {
  std::int64_t pt = 0, rt = 0;
  std::array<Raw, 3> pos{}, vel{};
  std::array<Raw, 4> quat{}, qvel{};
};
struct Slot {
  std::uint32_t index = 0;
  ClipId clip = 0;
  std::uint64_t cursor = 0;
  std::int64_t tick = 0;
  TrackId owner = 0;
  std::int32_t yaw = 0;
  std::array<Joint, MAX_JOINTS> joints{};
};
struct ClipSnapshot {
  ClipId id = 0;
  Clip value;
};
struct TrackSnapshot {
  TrackId id = 0;
  Track value;
};
struct Snapshot {
  std::uint32_t sim_frame = 0, cosmetic_seed = 0;
  Limits limits;
  std::vector<ClipSnapshot> clips;
  std::vector<TrackSnapshot> tracks;
  std::vector<Slot> slots;
  std::vector<SlotId> order;
};
struct JointValues {
  std::array<Raw, 3> position{};
  std::array<Raw, 4> quaternion{};
};

// Stable owned identities; no pointers escape snapshots. Mutable references
// are supplied-state APIs, not permission to change vector sizes/IDs outside
// the documented limits. The next operation validates owned references and
// budgets. This is synchronous and uses the caller's selected core FPU.
class Context {
  friend struct detail::CheckpointAccess;
  void checkpoint(detail::Checkpoint &);

public:
  explicit Context(std::size_t slot_count = 165, Limits limits = {});
  ~Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
  Context(Context &&) = delete;
  Context &operator=(Context &&) = delete;

  std::uint32_t sim_frame = 0, cosmetic_seed = 0;
  ClipId add_clip(const std::vector<std::uint8_t> &data,
                  const std::string &name = {});
  ClipId add_clip(const Clip &value);
  TrackId add_track(const Track &value = {});
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
  // Retains the temporary original-model track, including on decode failure;
  // a slot owner must never point to a destroyed stack cell.
  std::vector<JointValues> decode_clip(ClipId id, Raw time,
                                       std::uint32_t flags = 0);

private:
  struct State;
  std::unique_ptr<State> state_;
  void validate() const;
};

// A fresh two-slot context, with no persistent owner escaping the operation.
std::vector<JointValues> decode_clip(const Clip &clip, Raw time,
                                     std::uint32_t flags = 0,
                                     Limits limits = {});

} // namespace champions::simulation::rta::animation_decoder
