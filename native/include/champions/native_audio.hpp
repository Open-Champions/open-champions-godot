#pragma once
#include "champions/music_volume.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Champions: Return to Arms (SLUS-20973 v2.00) audio, authored from the
// t3/research-audio contracts docs/formats/audio.md, docs/audio-playback.md
// and tools/ref_audio_playback.py (commits a9593d2..6b08886). Local
// presentation state only: nothing here may feed shared gameplay state.
// See docs/NATIVE_AUDIO.md.
namespace champions::audio {

// Behaviour the research does not document. The call changes no state.
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};

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

// Image words and executed playback constants (docs/audio-playback.md,
// tools/ref_audio_playback.py). g_musicVolume 0x4f8944 is in music_volume.hpp.
constexpr std::uint32_t music_volume_gate = 0x3d4ccccc;
constexpr std::uint32_t music_volume_scale = 0x3f480000;
constexpr std::uint32_t duck_voice_level = 0x3e4cccd0;
constexpr std::uint32_t duck_creep = 0x3c888888;
constexpr std::uint32_t duck_ease = 0x3c23d70a;
constexpr std::uint32_t duck_current_image = 0x3f800000;
constexpr std::uint32_t duck_target_image = 0;
constexpr std::uint32_t float_one = 0x3f800000;
constexpr std::uint32_t sound_frame_image = 0;
constexpr std::uint32_t draw_frame_image = 0x400;
constexpr std::uint32_t level_countdown_image = 0x2a30;

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
// 0x40 and 0x100 are stored and returned by stream status. The service does not
// branch on them; the recall and cutscene callers do.
namespace stream_flag {
constexpr std::uint32_t loop = 0x1, fade_current = 0x2, wait_for_end = 0x4, hold = 0x8,
                        suppress = 0x20, recall = 0x40, ambience = 0x80,
                        cutscene_keep = 0x100;
constexpr std::uint32_t known = loop | fade_current | wait_for_end | hold | suppress |
                                recall | ambience | cutscene_keep;
} // namespace stream_flag

enum class StreamDecision { leave, stop, clear_queue };
enum class BossAction { noop, silence, replace };

struct SoundCadence {
  bool queues = false, end_flags = false, send_volumes = false;
  std::uint32_t next = 0;
};
struct StreamCursor {
  std::uint32_t position = 0, remaining = 0;
};
struct DuckStep {
  std::uint32_t current = 0, target = 0, channel = 0;
  bool sent = false;
};

// R5900 single-precision add/sub/mul used by music_updateDucking (ps2 model:
// six extra bits, truncate toward zero). Patterns are raw IEEE-layout words.
std::uint32_t ps2_add(std::uint32_t a, std::uint32_t b);
std::uint32_t ps2_sub(std::uint32_t a, std::uint32_t b);
std::uint32_t ps2_mul(std::uint32_t a, std::uint32_t b);
int ps2_cmp(std::uint32_t a, std::uint32_t b);

std::uint32_t loop_restart_span(std::uint32_t value);
StreamCursor after_completed_read(bool named, std::uint32_t flags, std::uint32_t position,
                                  std::uint32_t remaining, std::uint32_t offset,
                                  std::uint32_t length, std::uint32_t restart,
                                  std::uint32_t nbytes);
bool music_gate_open(std::uint32_t volume_bits);
DuckStep duck_step(std::uint32_t current, std::uint32_t target, std::uint32_t voice_status,
                   std::uint32_t stream2_status, bool actintro_other_language,
                   std::uint32_t music_volume);
std::uint32_t ducked_channel(std::uint32_t current, std::uint32_t music_volume);
std::uint32_t set_duck_target(std::uint32_t argument);
std::uint32_t stream_status_bits(std::uint32_t write_pos, std::uint32_t read_pos,
                                 std::uint32_t remaining, std::uint32_t active_flags,
                                 std::uint32_t queued_count, std::uint32_t first_flags);
bool recall_stops_music(std::uint32_t status);
StreamDecision cut_run_stream2(bool gate, std::uint32_t status);
StreamDecision cut_end_stream2(bool gate, std::uint32_t status);
SoundCadence sound_cadence(std::uint32_t frame);
bool draw_cadence(std::uint32_t frame);
BossAction boss_action(bool has_name, std::uint32_t volume_bits, std::uint32_t length);

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
    std::uint32_t offset = 0, length = 0, loop_restart = 0;
    bool stereo = false, from_vat = false;
  };
  struct Stream {
    bool active = false, held = false, audible = false, from_vat = false;
    std::string name;
    std::uint32_t flags = 0;
    std::uint64_t phase = 0;
    std::uint16_t volume = 0, target = full_volume, scale = 0x100;
    std::uint32_t file_offset = 0, file_length = 0, file_position = 0;
    std::uint32_t bytes_remaining = 0, loop_restart = 0;
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

  // Both read g_musicVolume and queue nothing below the gate 0x3d4ccccc.
  bool music_play_global(const std::string &name, std::uint32_t flags);
  bool music_play_level(const std::string &name, std::uint32_t flags);
  bool play_menu_music(bool wait_for_current);
  static std::optional<std::string> boss_music_name(const std::string &creature_type,
                                                    const std::string &object_name);
  // creature_startBossMusic: stop stream 2, then one mouth.vat entry, or silence.
  bool play_boss_music(const std::string &creature_type,
                       const std::string &object_name = {});
  // creature_stopBossMusic. No boss name leaves the queue unchanged.
  bool stop_boss_music(const std::string &creature_type, const std::string &object_name = {});
  // Explicit stream-2 stop marker for a caller that already decided to stop.
  bool stop_boss_music() { return queue_stop(2); }
  // g_musicVolume 0x4f8944 as a raw float pattern, stored as given with no
  // clamp. It starts at save_init's 0.5 (music_volume_default): no music request
  // can read the ELF word 1.0. The owner supplies the session value of the
  // documented boot chain (boot_music_volume in music_volume.hpp) or a slider value.
  void set_music_volume(std::uint32_t bits) { music_volume_ = bits; }
  std::uint32_t music_volume() const { return music_volume_; }
  // music_setDuck: target = 1 - argument. The current scalar is left alone.
  std::uint32_t set_duck(std::uint32_t argument);
  // One music_updateDucking call. tick() does not call it: its call site and
  // order inside the frame are not documented, so the owner decides when.
  DuckStep step_duck();
  std::uint32_t duck_current() const { return duck_current_; }
  std::uint32_t duck_target() const { return duck_target_; }
  bool duck_sent() const { return duck_sent_; }
  // The last audio_setChannelVolume(2, ...) argument. Recorded only: the driver's
  // mapping onto the output is not documented, so it does not change the mix.
  std::uint32_t music_channel() const { return music_channel_; }
  void set_level_name(const std::string &name) { level_name_ = name; }
  void set_language(std::int16_t language) { language_ = language; }

  // mainloop_pollAmbientAudio, once per displayed frame; draw is 0..7199.
  // The amb_ poll reads g_drawFrame and does not advance it.
  bool poll_level(std::uint32_t cosmetic_draw);
  void advance_draw_frame() { draw_frame_ = (draw_frame_ + 1) & 0xffffffffu; }
  // audio_streamStop: clear the queue and the active name. The volume word stays.
  void stream_stop(std::size_t stream);
  void clear_queue(std::size_t stream);
  // The five slices that fade-stop stream 2 when its status has bit 0x40.
  bool apply_recall_stop();
  // cut_runFrame also stops stream 1 before the stream-2 decision.
  StreamDecision cutscene_run(bool gate);
  StreamDecision cutscene_end(bool gate);
  std::uint32_t stream_status(std::size_t stream) const;

  void tick();
  // Client session boundary, not an original routine: the three streams are
  // stopped as audio_streamStop does, effect voices and sequences end, the level
  // directory is dropped and unread output is discarded. The process-lifetime
  // words stay: g_soundFrame, g_drawFrame, the duck words, the music volume, the
  // stream volume words and the driver timer (docs/audio-playback.md 260-265).
  // The loading-screen g_drawFrame advance and game_loadLevel's two sound_update
  // calls were not executed, so they are not applied.
  void end_session();
  std::vector<std::array<float, 2>> drain(std::size_t max_frames);
  std::size_t buffered_frames() const { return fifo_.size(); }

  static int group_limit(int group);
  std::size_t active_group(int group) const;
  const std::array<Voice, effect_voice_limit> &voices() const { return voices_; }
  const std::array<Stream, stream_limit> &streams() const { return streams_; }
  const std::array<Sequence, sequence_limit> &sequences() const { return sequences_; }
  std::uint64_t frame() const { return sound_frame_; }
  std::uint32_t draw_frame() const { return draw_frame_; }
  std::uint32_t level_countdown() const { return countdown_; }
  // Retained for the bridge. 0x40 and 0x100 are no longer counted here.
  std::uint64_t gated_flag_entries() const { return gated_flag_entries_; }
  std::uint64_t dropped_frames() const { return dropped_frames_; }
  std::uint64_t failed_stream_entries() const { return failed_stream_entries_; }
  std::uint64_t volume_sends() const { return volume_sends_; }

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
  std::uint32_t sound_frame_ = sound_frame_image, draw_frame_ = draw_frame_image;
  std::uint64_t output_samples_ = 0, next_timer_ = 480;
  std::uint64_t gated_flag_entries_ = 0, dropped_frames_ = 0, failed_stream_entries_ = 0;
  std::uint64_t volume_sends_ = 0;
  std::uint32_t countdown_ = level_countdown_image;
  std::uint32_t music_volume_ = music_volume_default; // save_init
  std::uint32_t duck_current_ = duck_current_image, duck_target_ = duck_target_image;
  std::uint32_t music_channel_ = float_one;
  std::int16_t language_ = 0;
  double sfx_volume_ = 1;
  bool frozen_ = false, two_channel_ = true, duck_sent_ = false;
  std::string level_name_;

  const Pcm *effect(const std::string &name) const;
  bool stream_exists(const std::string &name, StreamSource source) const;
  Pcm load_stream(const QueueEntry &entry) const;
  int sequence_slot(int handle) const;
  void refresh_gains(Voice &voice);
  void service_streams();
  void step_stream_volumes();
  void mix(std::array<float, 2> &out);
  void reclaim_ended();
  void promote(std::size_t stream);
  bool queue_music(const std::string &name, std::uint32_t flags, StreamSource source);
};
} // namespace champions::audio
