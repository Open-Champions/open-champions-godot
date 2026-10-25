#pragma once
#include <cstdint>
#include <optional>

// The player's music volume g_musicVolume 0x4f8944 of Champions: Return to Arms
// (SLUS-20973 v2.00), authored from t3/research-combat-ctors 22408a1
// docs/spec/music-volume.md and tools/ref_music_volume.py (executed against the
// original by tools/difftest_music_volume.py, 0 mismatches). Floats are raw
// patterns under the R5900 'ps2' adder model of native_audio.hpp.
//
// Only the music word is modelled: the live global and options-image word
// g_savedConfig +0x08. The other 16 option scalars, the four button rows and
// image +0x00 are written by the same routines but are outside this model.
// Local presentation state: nothing here may feed shared gameplay state.
namespace champions::audio {

constexpr std::uint32_t music_volume_elf_image = 0x3f800000; // never read by music
constexpr std::uint32_t options_music_elf_image = 0;         // g_savedConfig +0x08
constexpr std::uint32_t music_volume_default = 0x3f000000;   // config_setDefaults(-1)
constexpr std::uint32_t music_slider_step = 0x3c088888;      // 1/120 per active call
constexpr std::uint32_t menu_repeat_step = 0x3e4fdef8;       // timer gain, capped at 1.0
constexpr std::uint32_t slider_lower_bits = 0x408000;        // held bits that lower
constexpr std::uint32_t slider_raise_bits = 0x802000;        // held bits that raise

// The two music words. The member functions are the writers of the spec's
// "Writers of 0x4f8944" and "Writers of image +0x08" tables.
struct MusicVolumeWords {
  std::uint32_t live = music_volume_elf_image;   // 0x4f8944
  std::uint32_t image = options_music_elf_image; // g_savedConfig 0x4f8f38 +0x08

  // config_setDefaults(which): -1 (save_init) stores 0.5 in both words; any
  // other value writes only button rows and leaves the music words.
  void set_defaults(std::int32_t which = -1);
  // memcard_browser step 29 after a good options-file read: a non-zero mode
  // copies the file over the image, +0x08 bit for bit with no clamp. The live
  // word is not touched here. Boot (memcard_configScreen0) passes mode 1.
  void browser_copy(std::uint32_t mode, std::uint32_t file_word);
  // game_addCoreTasks at every mainloop_runSession start: live = image, no clamp.
  void add_core_tasks() { live = image; }
  // memcard_optionsScreen: state 2 copies the load buffer's +0x08 into both
  // words; every other state writes neither music word.
  void options_screen(std::int32_t state, std::uint32_t buffer_word);
};

// One active slider step of menuList_handleInput on a type-4 entry: lower bits
// subtract 1/120 and clamp to 0 below 0, then raise bits add 1/120 and clamp to
// 1.0 above it. Both may apply in one call.
std::uint32_t slider_step(std::uint32_t value, std::uint32_t held);
struct SliderCall {
  std::uint32_t value = 0, timer = 0;
  bool active = false;
};
// One menuList_handleInput call with the cursor on the slider and no edge bits:
// the per-slot timer gains 0x3e4fdef8, capped at 1.0; below 1.0 nothing else
// happens (the first four calls after the cursor lands are idle).
SliderCall menu_slider_call(std::uint32_t value, std::uint32_t held, std::uint32_t timer);

// config_commitOptions(force, queryOnly, noCardSave) reduced, as the reference
// is, to a state where every other option already equals its image word: the
// count is 1 when the live music differs from the image (c.eq.s), a non-zero
// count copies the live value into the image unless queryOnly, and a save is
// requested when the count or force is non-zero, unless queryOnly or noCardSave.
struct MusicCommit {
  std::uint32_t count = 0, image = 0;
  bool save_requested = false;
};
MusicCommit config_commit_music(std::uint32_t live, std::uint32_t image, bool query_only,
                                bool no_card_save, bool force);

// music_applyVolume: with stream 2 status bit 0x20, audio_setChannelVolume gets
// (duck * volume) * 0.78125. It stores nothing.
struct ApplyVolume {
  bool sent = false;
  std::uint32_t channel = 0;
};
ApplyVolume music_apply_volume(std::uint32_t volume, std::uint32_t duck, std::uint32_t status);

// The value the first level or boss music request reads after the documented
// boot: save_init's config_setDefaults(-1), the boot card copy of the options
// file's +0x08 when there is a file (nullopt: no card or no file), then the
// first session's game_addCoreTasks. Before any slider input.
std::uint32_t boot_music_volume(std::optional<std::uint32_t> options_file_word);
} // namespace champions::audio
