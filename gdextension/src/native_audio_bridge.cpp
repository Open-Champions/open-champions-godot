#include "native_audio_bridge.hpp"
#include <cmath>
#include <cstring>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

using namespace godot;
namespace audio = champions::audio;

namespace {
std::string utf8(const String &value) {
  const auto bytes = value.utf8();
  return std::string(bytes.get_data(), std::size_t(bytes.length()));
}
audio::Vec3 vec(const Vector3 &v) { return {v.x, v.y, v.z}; }
// A non-finite position makes pan_atten's gains NaN, which the mixer's clamp passes
// on to the output (binding audit U3): refused at the boundary.
bool finite_value(const Vector3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
audio::Bytes view(const PackedByteArray &b) { return {b.ptr(), std::size_t(b.size())}; }
std::vector<std::uint8_t> copy(const PackedByteArray &b) {
  return std::vector<std::uint8_t>(b.ptr(), b.ptr() + b.size());
}
bool valid_stream(int32_t stream) {
  return stream >= 0 && std::size_t(stream) < audio::Playback::stream_limit;
}
} // namespace

void ChampionsNativeAudio::_bind_methods() {
  ClassDB::bind_method(D_METHOD("register_vag", "name", "bytes"), &ChampionsNativeAudio::register_vag);
  ClassDB::bind_method(D_METHOD("set_global_vat", "directory", "vat", "language"),
                       &ChampionsNativeAudio::set_global_vat);
  ClassDB::bind_method(D_METHOD("set_level_vat", "directory", "vat", "language", "initial_countdown"),
                       &ChampionsNativeAudio::set_level_vat);
  ClassDB::bind_method(D_METHOD("attach_playback", "playback"), &ChampionsNativeAudio::attach_playback);
  ClassDB::bind_method(D_METHOD("set_camera", "position", "right"), &ChampionsNativeAudio::set_camera);
  ClassDB::bind_method(D_METHOD("play_sample", "name", "pitch_scale", "volume", "position",
                                "positional", "plays_while_frozen"),
                       &ChampionsNativeAudio::play_sample);
  ClassDB::bind_method(D_METHOD("start_sequence", "name", "volume", "position", "group"),
                       &ChampionsNativeAudio::start_sequence);
  ClassDB::bind_method(D_METHOD("stop_sequence", "handle"), &ChampionsNativeAudio::stop_sequence);
  ClassDB::bind_method(D_METHOD("release_sequence", "handle"), &ChampionsNativeAudio::release_sequence);
  ClassDB::bind_method(D_METHOD("set_sequence_position", "handle", "position"),
                       &ChampionsNativeAudio::set_sequence_position);
  ClassDB::bind_method(D_METHOD("animation_event", "mask", "clip_role", "sample_name", "position"),
                       &ChampionsNativeAudio::animation_event);
  ClassDB::bind_method(D_METHOD("queue_level_entry", "stream", "name", "flags"),
                       &ChampionsNativeAudio::queue_level_entry);
  ClassDB::bind_method(D_METHOD("queue_global_entry", "stream", "name", "flags"),
                       &ChampionsNativeAudio::queue_global_entry);
  ClassDB::bind_method(D_METHOD("queue_stop", "stream"), &ChampionsNativeAudio::queue_stop);
  ClassDB::bind_method(D_METHOD("release_stream", "stream"), &ChampionsNativeAudio::release_stream);
  ClassDB::bind_method(D_METHOD("stream_stop", "stream"), &ChampionsNativeAudio::stream_stop);
  ClassDB::bind_method(D_METHOD("set_stream_scale", "stream", "scale"), &ChampionsNativeAudio::set_stream_scale);
  ClassDB::bind_method(D_METHOD("set_two_channel_output", "enabled"),
                       &ChampionsNativeAudio::set_two_channel_output);
  ClassDB::bind_method(D_METHOD("set_frozen", "frozen"), &ChampionsNativeAudio::set_frozen);
  ClassDB::bind_method(D_METHOD("set_sfx_volume", "volume"), &ChampionsNativeAudio::set_sfx_volume);
  ClassDB::bind_method(D_METHOD("set_music_volume", "volume"), &ChampionsNativeAudio::set_music_volume);
  ClassDB::bind_method(D_METHOD("set_options_file_music", "bits"),
                       &ChampionsNativeAudio::set_options_file_music);
  ClassDB::bind_method(D_METHOD("end_session"), &ChampionsNativeAudio::end_session);
  ClassDB::bind_method(D_METHOD("play_menu_music", "wait_for_current"), &ChampionsNativeAudio::play_menu_music);
  ClassDB::bind_method(D_METHOD("music_play_global", "name", "flags"), &ChampionsNativeAudio::music_play_global);
  ClassDB::bind_method(D_METHOD("play_boss_music", "creature_type", "object_name"),
                       &ChampionsNativeAudio::play_boss_music);
  ClassDB::bind_method(D_METHOD("stop_boss_music"), &ChampionsNativeAudio::stop_boss_music);
  ClassDB::bind_method(D_METHOD("get_stream_time", "stream"), &ChampionsNativeAudio::get_stream_time);
  ClassDB::bind_method(D_METHOD("get_gated_flag_entries"), &ChampionsNativeAudio::get_gated_flag_entries);
  ClassDB::bind_method(D_METHOD("get_observation"), &ChampionsNativeAudio::get_observation);
  ClassDB::bind_method(D_METHOD("sound_tick", "cosmetic_draw"), &ChampionsNativeAudio::sound_tick);
  ClassDB::bind_method(D_METHOD("pump_frames", "max_frames"), &ChampionsNativeAudio::pump_frames);
  ClassDB::bind_method(D_METHOD("get_last_error"), &ChampionsNativeAudio::get_last_error);
}

void ChampionsNativeAudio::fail(const char *message) { last_error_ = String::utf8(message); }

bool ChampionsNativeAudio::register_vag(const String &name, const PackedByteArray &bytes) {
  try {
    if (!model_.add_effect(utf8(name), audio::decode_vag(view(bytes)))) {
      fail("duplicate sample name; the first registration is kept");
      return false;
    }
    last_error_ = "";
    return true;
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::set_global_vat(const PackedByteArray &directory,
                                          const PackedByteArray &vat, int32_t language) {
  try {
    model_.set_global_vat(copy(vat), audio::parse_vat_directory(view(directory), std::size_t(vat.size())),
                          std::int16_t(language));
    last_error_ = "";
    return true;
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::set_level_vat(const PackedByteArray &directory, const PackedByteArray &vat,
                                         int32_t language, int64_t initial_countdown) {
  try {
    if (initial_countdown <= 0 || initial_countdown > INT32_MAX) {
      fail("initial countdown must be a positive frame count");
      return false;
    }
    model_.set_level_vat(copy(vat), audio::parse_vat_directory(view(directory), std::size_t(vat.size())),
                         std::int16_t(language), std::uint32_t(initial_countdown));
    last_error_ = "";
    return true;
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

void ChampionsNativeAudio::attach_playback(const Ref<AudioStreamGeneratorPlayback> &p) { playback_ = p; }

bool ChampionsNativeAudio::set_camera(const Vector3 &p, const Vector3 &r) {
  if (!finite_value(p) || !finite_value(r)) {
    fail("camera position and right vector must be finite");
    return false;
  }
  try {
    model_.set_camera(vec(p), vec(r));
    last_error_ = "";
    return true;
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

int32_t ChampionsNativeAudio::play_sample(const String &name, double pitch_scale, double volume,
                                          const Vector3 &position, bool positional,
                                          bool plays_while_frozen) {
  if (positional && !finite_value(position)) {
    fail("sample position must be finite");
    return -1;
  }
  try {
    return model_.play_sample(utf8(name), pitch_scale, volume,
                              positional ? std::optional<audio::Vec3>(vec(position)) : std::nullopt,
                              plays_while_frozen);
  } catch (const std::exception &e) {
    fail(e.what());
    return -1;
  }
}

int32_t ChampionsNativeAudio::start_sequence(const String &name, double volume,
                                             const Vector3 &position, int32_t group) {
  if (!finite_value(position)) {
    fail("sequence position must be finite");
    return -1;
  }
  try {
    return model_.start_sequence(utf8(name), volume, vec(position), group);
  } catch (const std::exception &e) {
    fail(e.what());
    return -1;
  }
}

bool ChampionsNativeAudio::stop_sequence(int32_t h) { return model_.stop_sequence(h); }
bool ChampionsNativeAudio::release_sequence(int32_t h) { return model_.release_sequence(h); }
bool ChampionsNativeAudio::set_sequence_position(int32_t h, const Vector3 &p) {
  if (!finite_value(p)) {
    fail("sequence position must be finite");
    return false;
  }
  try {
    return model_.set_sequence_position(h, vec(p));
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

int32_t ChampionsNativeAudio::animation_event(int64_t mask, int32_t role, const String &sample,
                                              const Vector3 &position) {
  if (mask < 0 || mask > 0xff || role < 0 || role > 2) {
    fail("animation mask must be 0..0xff and clip_role 0 (attack), 1 (locomotion) or 2 (other)");
    return -1;
  }
  if (!finite_value(position)) {
    fail("animation event position must be finite");
    return -1;
  }
  try {
    return model_.animation_event(std::uint32_t(mask), audio::ClipRole(role), utf8(sample), vec(position));
  } catch (const std::exception &e) {
    fail(e.what());
    return -1;
  }
}

bool ChampionsNativeAudio::queue_level_entry(int32_t stream, const String &name, int64_t flags) {
  try {
    if (!valid_stream(stream) || flags < 0 || flags > 0xffff)
      throw std::invalid_argument("stream must be 0..2 and flags 0..0xffff");
    return model_.queue_stream(std::size_t(stream), utf8(name), std::uint32_t(flags),
                               audio::StreamSource::level);
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::queue_global_entry(int32_t stream, const String &name, int64_t flags) {
  try {
    if (!valid_stream(stream) || flags < 0 || flags > 0xffff)
      throw std::invalid_argument("stream must be 0..2 and flags 0..0xffff");
    return model_.queue_stream(std::size_t(stream), utf8(name), std::uint32_t(flags),
                               audio::StreamSource::global);
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::queue_stop(int32_t stream) {
  return valid_stream(stream) && model_.queue_stop(std::size_t(stream));
}

void ChampionsNativeAudio::release_stream(int32_t stream) {
  if (valid_stream(stream))
    model_.release_stream(std::size_t(stream));
}
void ChampionsNativeAudio::stream_stop(int32_t stream) {
  if (valid_stream(stream))
    model_.stream_stop(std::size_t(stream));
}

void ChampionsNativeAudio::set_stream_scale(int32_t stream, int32_t scale) {
  if (valid_stream(stream) && scale >= 0 && scale <= 0xffff)
    model_.set_stream_scale(std::size_t(stream), std::uint16_t(scale));
}

void ChampionsNativeAudio::set_two_channel_output(bool enabled) { model_.set_two_channel_output(enabled); }
void ChampionsNativeAudio::set_frozen(bool frozen) { model_.set_frozen(frozen); }

void ChampionsNativeAudio::set_sfx_volume(double volume) {
  try {
    model_.set_sfx_volume(volume);
  } catch (const std::exception &e) {
    fail(e.what());
  }
}

// g_musicVolume starts at save_init's 0.5 (research 22408a1
// docs/spec/music-volume.md). This sets a slider-range value: the front-end
// slider clamps to 0..1, so a value outside that range is refused here. An
// options-file word, which is copied with no clamp, goes through
// set_options_file_music instead.
bool ChampionsNativeAudio::set_music_volume(double volume) {
  if (!std::isfinite(volume) || volume < 0.0 || volume > 1.0) {
    fail("music slider volume outside 0..1");
    return false;
  }
  const float value = float(volume);
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof bits);
  model_.set_music_volume(bits);
  last_error_ = String();
  return true;
}

// The options file's +0x08 word as a raw pattern: the boot card copy puts it in
// the options image unclamped and game_addCoreTasks copies it to g_musicVolume.
bool ChampionsNativeAudio::set_options_file_music(int64_t bits) {
  if (bits < 0 || bits > int64_t(UINT32_MAX)) {
    fail("options-file music word is not a 32-bit pattern");
    return false;
  }
  model_.set_music_volume(audio::boot_music_volume(std::uint32_t(bits)));
  last_error_ = String();
  return true;
}

// Client session boundary; the process-lifetime counters and duck words stay.
void ChampionsNativeAudio::end_session() { model_.end_session(); }

// music_playGlobal 0x2110e0: the gate on g_musicVolume, then a stream-2 queue
// with flags | 0x22. The options slider previews through it (flags 0x41).
bool ChampionsNativeAudio::music_play_global(const String &name, int64_t flags) {
  try {
    if (flags < 0 || flags > 0xffffffffLL)
      throw std::invalid_argument("flags are a 32-bit word");
    auto utf = name.utf8();
    return model_.music_play_global(utf.get_data() ? utf.get_data() : "", static_cast<std::uint32_t>(flags));
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::play_menu_music(bool wait) {
  try {
    return model_.play_menu_music(wait);
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::play_boss_music(const String &type, const String &object) {
  try {
    return model_.play_boss_music(utf8(type), utf8(object));
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

bool ChampionsNativeAudio::stop_boss_music() { return model_.stop_boss_music(); }

double ChampionsNativeAudio::get_stream_time(int32_t stream) const {
  return valid_stream(stream) ? model_.stream_time(std::size_t(stream)) : 0.0;
}

int64_t ChampionsNativeAudio::get_gated_flag_entries() const {
  return int64_t(model_.gated_flag_entries());
}

bool ChampionsNativeAudio::sound_tick(int32_t draw) {
  try {
    if (draw < 0)
      throw std::invalid_argument("cosmetic draw outside 0..7199");
    const bool queued = model_.poll_level(std::uint32_t(draw));
    model_.tick();
    // gfx_endFrame increments g_drawFrame after the poll and sound_update.
    model_.advance_draw_frame();
    return queued;
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}

int32_t ChampionsNativeAudio::pump_frames(int32_t maximum) {
  if (playback_.is_null() || maximum <= 0)
    return 0;
  const auto room = std::min<int64_t>(maximum, playback_->get_frames_available());
  const auto count = std::min<int64_t>(room, int64_t(model_.buffered_frames()));
  if (count <= 0)
    return 0;
  const auto frames = model_.drain(std::size_t(count));
  PackedVector2Array buffer;
  buffer.resize(int64_t(frames.size()));
  for (std::size_t i = 0; i < frames.size(); ++i)
    buffer.set(int64_t(i), Vector2(frames[i][0], frames[i][1]));
  if (!playback_->push_buffer(buffer)) {
    fail("AudioStreamGenerator buffer push failed");
    return 0;
  }
  return int32_t(frames.size());
}

Dictionary ChampionsNativeAudio::get_observation() const {
  Dictionary out; Array streams, voices;
  out["tick"]=int64_t(model_.frame());
  out["buffered_frames"]=int64_t(model_.buffered_frames());
  out["dropped_frames"]=int64_t(model_.dropped_frames());
  out["failed_stream_entries"]=int64_t(model_.failed_stream_entries());
  out["level_countdown"]=int64_t(model_.level_countdown());
  out["music_volume"]=int64_t(model_.music_volume()); // raw g_musicVolume pattern
  out["music_gate_open"]=champions::audio::music_gate_open(model_.music_volume());
  for(std::size_t i=0;i<model_.streams().size();++i) {
    const auto &s=model_.streams()[i]; Dictionary item;
    item["name"]=String::utf8(s.name.c_str()); item["active"]=s.active;
    item["flags"]=int64_t(s.flags); item["queued"]=int64_t(model_.queued_count(i));
    // audio_streamStatus, and the bytes played into the active entry.
    item["status"]=int64_t(model_.stream_status(i));
    // Bytes of the active entry played: 28 samples per 16-byte ADPCM block,
    // the conversion of dialog_syncMouth (research docs/dialog.md 5.4).
    item["entry_position"]=s.active ? int64_t(model_.stream_samples_played(i)*16/28) : int64_t(0);
    streams.push_back(item);
  }
  for(const auto &v:model_.voices()) if(v.active) {
    Dictionary item; item["sample"]=String::utf8(v.sample.c_str());
    item["ended"]=v.ended; voices.push_back(item);
  }
  out["streams"]=streams; out["voices"]=voices;
  return out;
}
