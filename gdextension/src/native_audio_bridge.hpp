#pragma once
#include "champions/native_audio.hpp"
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {
// Presentation-only bridge from champions::audio::Playback to an
// AudioStreamGenerator whose mix_rate is 48000. See docs/NATIVE_AUDIO.md.
class ChampionsNativeAudio : public RefCounted {
  GDCLASS(ChampionsNativeAudio, RefCounted)
  champions::audio::Playback model_;
  Ref<AudioStreamGeneratorPlayback> playback_;
  String last_error_;
  static void _bind_methods();
  void fail(const char *message);

public:
  bool register_vag(const String &name, const PackedByteArray &bytes);
  bool set_global_vat(const PackedByteArray &directory, const PackedByteArray &vat,
                      int32_t language);
  bool set_level_vat(const PackedByteArray &directory, const PackedByteArray &vat,
                     int32_t language, int64_t initial_countdown);
  void attach_playback(const Ref<AudioStreamGeneratorPlayback> &playback);
  bool set_camera(const Vector3 &position, const Vector3 &right);
  int32_t play_sample(const String &name, double pitch_scale, double volume,
                      const Vector3 &position, bool positional, bool plays_while_frozen);
  int32_t start_sequence(const String &name, double volume, const Vector3 &position,
                         int32_t group);
  bool stop_sequence(int32_t handle);
  bool release_sequence(int32_t handle);
  bool set_sequence_position(int32_t handle, const Vector3 &position);
  int32_t animation_event(int64_t mask, int32_t clip_role, const String &sample_name,
                          const Vector3 &position);
  bool queue_level_entry(int32_t stream, const String &name, int64_t flags);
  bool queue_global_entry(int32_t stream, const String &name, int64_t flags);
  bool queue_stop(int32_t stream);
  void release_stream(int32_t stream);
  // audio_streamStop: clear the queue and the active name (dialog lines).
  void stream_stop(int32_t stream);
  void set_stream_scale(int32_t stream, int32_t scale);
  void set_two_channel_output(bool enabled);
  void set_frozen(bool frozen);
  void set_sfx_volume(double volume);
  bool set_music_volume(double volume);
  bool set_options_file_music(int64_t bits);
  void end_session();
  bool play_menu_music(bool wait_for_current);
  bool music_play_global(const String &name, int64_t flags);
  bool play_boss_music(const String &creature_type, const String &object_name);
  bool stop_boss_music();
  double get_stream_time(int32_t stream) const;
  int64_t get_gated_flag_entries() const;
  Dictionary get_observation() const;
  bool sound_tick(int32_t cosmetic_draw);
  int32_t pump_frames(int32_t max_frames);
  String get_last_error() const { return last_error_; }
};
} // namespace godot
