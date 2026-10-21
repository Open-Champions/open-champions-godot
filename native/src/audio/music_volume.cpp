#include "champions/music_volume.hpp"
#include "champions/native_audio.hpp"

// Original implementation of t3/research-combat-ctors 22408a1
// docs/spec/music-volume.md and tools/ref_music_volume.py. No game bytes.
namespace champions::audio {

void MusicVolumeWords::set_defaults(std::int32_t which) {
  if (which != -1)
    return;
  live = music_volume_default;
  image = music_volume_default;
}

void MusicVolumeWords::browser_copy(std::uint32_t mode, std::uint32_t file_word) {
  if (mode != 0)
    image = file_word;
}

void MusicVolumeWords::options_screen(std::int32_t state, std::uint32_t buffer_word) {
  if (state != 2)
    return;
  live = buffer_word;
  image = buffer_word;
}

std::uint32_t slider_step(std::uint32_t value, std::uint32_t held) {
  if (held & slider_lower_bits) {
    value = ps2_sub(value, music_slider_step);
    if (ps2_cmp(value, 0) < 0)
      value = 0;
  }
  if (held & slider_raise_bits) {
    value = ps2_add(value, music_slider_step);
    if (ps2_cmp(float_one, value) < 0)
      value = float_one;
  }
  return value;
}

SliderCall menu_slider_call(std::uint32_t value, std::uint32_t held, std::uint32_t timer) {
  timer = ps2_add(timer, menu_repeat_step);
  if (ps2_cmp(float_one, timer) <= 0)
    timer = float_one;
  if (ps2_cmp(float_one, timer) > 0)
    return {value, timer, false};
  return {slider_step(value, held), timer, true};
}

MusicCommit config_commit_music(std::uint32_t live, std::uint32_t image, bool query_only,
                                bool no_card_save, bool force) {
  MusicCommit out;
  out.count = ps2_cmp(live, image) == 0 ? 0 : 1;
  out.image = image;
  if (query_only)
    return out;
  if (out.count)
    out.image = live;
  out.save_requested = (out.count || force) && !no_card_save;
  return out;
}

ApplyVolume music_apply_volume(std::uint32_t volume, std::uint32_t duck, std::uint32_t status) {
  constexpr std::uint32_t playing = 0x20; // audio_streamStatus(2) bit
  if (!(status & playing))
    return {};
  return {true, ducked_channel(duck, volume)};
}

std::uint32_t boot_music_volume(std::optional<std::uint32_t> options_file_word) {
  MusicVolumeWords words;
  words.set_defaults(-1);
  if (options_file_word)
    words.browser_copy(1, *options_file_word);
  words.add_core_tasks();
  return words.live;
}
} // namespace champions::audio
