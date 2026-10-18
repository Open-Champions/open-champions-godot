#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Champions: Return to Arms (SLUS-20973 v2.00) audio, authored from the
// t3/research-audio contracts docs/formats/audio.md and docs/audio-playback.md
// (commits a9593d2..834806e). Local presentation state only: nothing here may
// feed shared gameplay state. See docs/NATIVE_AUDIO.md.
namespace champions::audio {

struct Bytes {
  const std::uint8_t *data = nullptr;
  std::size_t size = 0;
};

struct VagHeader {
  std::array<std::uint8_t, 4> magic{};
  std::uint32_t version = 0, data_size = 0, rate = 0;
  std::string name;
};

// Decoded PS2 ADPCM. Loop end is exclusive; loop points are in samples.
struct Pcm {
  std::vector<std::int16_t> left, right;
  std::optional<std::size_t> loop_start, loop_end;
  std::uint32_t rate = 0;
  bool stereo() const { return !right.empty(); }
  bool loops() const {
    return loop_start && loop_end && *loop_start < *loop_end;
  }
};

struct VatEntry {
  std::string name;
  std::uint32_t offset = 0, length = 0, declared_rate = 0, loop_restart = 0;
  bool stereo = false;
  std::int16_t language = 0;
};

constexpr std::uint32_t spu_rate = 48000;
constexpr std::uint32_t stream_pitch = 0x7fe;   // HELLO+0x1018
constexpr double game_stream_rate = 24000.0;    // dialog_syncMouth 0x22fdd8

VagHeader parse_vag_header(Bytes bytes);
// honor_flags=false is the stream driver's view: every block is played.
Pcm decode_adpcm(Bytes bytes, bool honor_flags = true);
Pcm decode_vag(Bytes bytes);
std::vector<VatEntry> parse_vat_directory(Bytes directory, std::size_t vat_size);
Pcm decode_vat_entry(Bytes vat, const VatEntry &entry);
// Exact-case name; the current language is preferred over language 0, and
// within either the last record wins.
std::optional<VatEntry> find_vat_entry(const std::vector<VatEntry> &entries,
                                       const std::string &name,
                                       std::int16_t language);
std::uint32_t base_pitch(std::uint32_t rate);
double stream_duration(std::size_t bytes, bool stereo, double rate = game_stream_rate);

struct Vec3 {
  double x = 0, y = 0, z = 0;
};
struct Gains {
  double left = 0, right = 0;
};
Gains pan_atten(Vec3 position, Vec3 camera, Vec3 right, double near = 360.0,
                double far = 840.0);

// Gameplay hooks from t3/research-anim docs/spec/creature-animation-dispatch.md
// section 4.3. Mask 0x4 is a combo strike on attack clips and a footstep on
// walk/run clips, so the caller must state which kind of clip fired it.
enum class ClipRole { attack, locomotion, other };
enum class GameplayCue { none, hit, footstep };
GameplayCue classify_animation_event(std::uint32_t mask, ClipRole role);

// Stream queue flags (stream_startQueued 0x211548, stream_serviceQueues 0x211df8).
namespace stream_flag {
constexpr std::uint32_t loop = 0x1, fade_current = 0x2, wait_for_end = 0x4,
                        hold = 0x8, suppress = 0x20, ambience = 0x80;
// Passed by callers, meaning not traced: stored and counted, never interpreted.
constexpr std::uint32_t gated = 0x40 | 0x100;
} // namespace stream_flag

enum class StreamSource { registered, level, global };

// Deterministic sound model. One tick() is one displayed 60 Hz sound frame and
// mixes exactly frames_per_tick stereo frames at 48 kHz into an output FIFO.
// Voice allocation, reclaim, queues and fades change only inside calls made by
// the owner and tick(), never from the audio device's pull.
class Playback {
public:
  static constexpr std::size_t effect_voice_limit = 43, sequence_limit = 32,
                               stream_limit = 3, stream_queue_limit = 8;
  static constexpr std::size_t frames_per_tick = spu_rate / 60;
  static constexpr std::size_t fifo_limit = frames_per_tick * 16;
  static constexpr std::uint16_t full_volume = 0x3fff;

  struct Voice {
    bool active = false, ended = false, plays_while_frozen = false;
    std::string sample;
    const Pcm *pcm = nullptr; // registered effects are never removed
    std::uint32_t pitch = 0;  // SPU pitch register; 0x1000 is 48 kHz
    std::uint64_t phase = 0;  // sample position with 12 fractional bits
    double volume = 1;
    std::optional<Vec3> position;
    double near = 360, far = 840;
    Gains gains{1, 1};
    int sequence = -1;
  };
  struct QueueEntry {
    std::string name;
    std::uint32_t flags = 0;
    StreamSource source = StreamSource::registered;
  };
  struct Stream {
    bool active = false, held = false;
    std::string name;
    std::uint32_t flags = 0;
    std::uint64_t phase = 0;
    std::uint16_t volume = 0, target = full_volume, scale = 0x100;
    Pcm pcm;
  };
  struct Sequence {
    bool active = false, repeat = false;
    int group = 0, voice = -1;
    std::uint8_t generation = 0;
  };

  // Effect samples are matched without case; a duplicate keeps the first.
  bool add_effect(const std::string &name, Pcm pcm);
  void add_stream(const std::string &name, Pcm pcm);
  void set_global_vat(std::vector<std::uint8_t> vat, std::vector<VatEntry> entries,
                      std::int16_t language);
  void set_level_vat(std::vector<std::uint8_t> vat, std::vector<VatEntry> entries,
                     std::int16_t language, std::uint32_t initial_countdown);

  // sound_playSample 0x2134e8. Returns the voice index or -1.
  int play_sample(const std::string &name, double pitch_scale, double volume,
                  std::optional<Vec3> position, bool plays_while_frozen = false);
  // A single-sample sequence in one of 32 slots with its group cap. Returns a
  // handle (bit 15, generation << 8, slot) or -1.
  int start_sequence(const std::string &name, double volume, Vec3 position,
                     int group, bool plays_while_frozen = false);
  bool stop_sequence(int handle);
  bool release_sequence(int handle);
  bool set_sequence_position(int handle, Vec3 position);
  int animation_event(std::uint32_t mask, ClipRole role, const std::string &sample,
                      Vec3 position);
  void set_camera(Vec3 position, Vec3 right);
  void set_frozen(bool frozen) { frozen_ = frozen; }
  void set_sfx_volume(double volume);

  bool queue_stream(std::size_t stream, const std::string &name, std::uint32_t flags,
                    StreamSource source = StreamSource::registered);
  bool queue_stop(std::size_t stream);
  void release_stream(std::size_t stream);
  void set_stream_scale(std::size_t stream, std::uint16_t scale);
  void set_two_channel_output(bool enabled) { two_channel_ = enabled; }
  std::size_t queued_count(std::size_t stream) const;
  std::uint64_t stream_samples_played(std::size_t stream) const;
  double stream_time(std::size_t stream) const;

  bool music_play_global(const std::string &name, std::uint32_t flags);
  bool music_play_level(const std::string &name, std::uint32_t flags);
  bool play_menu_music(bool wait_for_current);
  static std::optional<std::string> boss_music_name(const std::string &creature_type,
                                                    const std::string &object_name);
  bool play_boss_music(const std::string &creature_type,
                       const std::string &object_name = {});
  bool stop_boss_music() { return queue_stop(2); }
  // mainloop_pollAmbientAudio, once per displayed frame; draw is 0..7199.
  bool poll_level(std::uint32_t cosmetic_draw);

  void tick();
  std::vector<std::array<float, 2>> drain(std::size_t max_frames);
  std::size_t buffered_frames() const { return fifo_.size(); }

  static int group_limit(int group);
  std::size_t active_group(int group) const;
  const std::array<Voice, effect_voice_limit> &voices() const { return voices_; }
  const std::array<Stream, stream_limit> &streams() const { return streams_; }
  const std::array<Sequence, sequence_limit> &sequences() const { return sequences_; }
  std::uint64_t frame() const { return frame_; }
  std::uint32_t level_countdown() const { return countdown_; }
  std::uint64_t gated_flag_entries() const { return gated_flag_entries_; }
  std::uint64_t dropped_frames() const { return dropped_frames_; }
  std::uint64_t failed_stream_entries() const { return failed_stream_entries_; }

private:
  struct Vat {
    std::vector<std::uint8_t> bytes;
    std::vector<VatEntry> entries;
    std::int16_t language = 0;
  };
  std::map<std::string, Pcm> effects_, registered_;
  Vat global_, level_;
  bool level_active_ = false;
  std::array<Voice, effect_voice_limit> voices_{};
  std::array<Sequence, sequence_limit> sequences_{};
  std::array<Stream, stream_limit> streams_{};
  std::array<std::deque<QueueEntry>, stream_limit> queues_{};
  std::deque<std::array<float, 2>> fifo_;
  Vec3 camera_{}, right_{1, 0, 0};
  std::size_t next_voice_ = 0;
  std::uint64_t frame_ = 0, output_samples_ = 0, next_timer_ = 480;
  std::uint64_t level_frames_ = 0, gated_flag_entries_ = 0, dropped_frames_ = 0;
  std::uint64_t failed_stream_entries_ = 0;
  std::uint32_t countdown_ = 0;
  double sfx_volume_ = 1;
  bool frozen_ = false, two_channel_ = true;

  const Pcm *effect(const std::string &name) const;
  bool stream_exists(const std::string &name, StreamSource source) const;
  Pcm load_stream(const QueueEntry &entry) const;
  int sequence_slot(int handle) const;
  void refresh_gains(Voice &voice);
  void service_streams();
  void step_stream_volumes();
  void mix(std::array<float, 2> &out);
};
} // namespace champions::audio
