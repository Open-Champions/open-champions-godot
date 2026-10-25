# Native audio (Return to Arms)

`champions-native-audio` (`native/include/champions/native_audio.hpp`) is a
Godot-independent C++17 port of the Champions: Return to Arms (SLUS-20973 v2.00)
audio contracts in the research branch `t3/research-audio`, commits
`a9593d2..6b08886`: `docs/formats/audio.md`, `docs/audio-playback.md`,
`tools/ref_audio.py` and `tools/ref_audio_playback.py`. Gameplay hook masks come from `t3/research-anim`
`docs/spec/creature-animation-dispatch.md` section 4.3. It is written from those
documents only. Tests build their own synthetic bytes, and no game sample, capture or
decompiler output is used. Norrath is not covered.

Everything here is **local presentation state**. Camera position, voice allocation,
stream position and the cosmetic random draw must not feed shared gameplay state or
network traces (`docs/audio-save.md` A3 in the research repository).

## Formats

| Function | Contract |
|---|---|
| `parse_vag_header(Bytes)` | 0x30-byte big-endian header: version +0x04, data size +0x0c (multiple of 16), rate +0x10 (nonzero), 16-byte name +0x20. The magic is informative and not checked. |
| `base_pitch(rate)` | `rate * 4096 / 48000`, integer. |
| `decode_adpcm(Bytes, honor_flags)` | 16-byte blocks to 28 samples; filters `(0,0) (60,0) (115,-52) (98,-55) (122,-60)`; `(nibble << 12) >> shift` plus `(c1*p + c2*pp + 32) >> 6` with floor shifts; clamps to s16; history carries across blocks. With `honor_flags`, flag 7 stops before the block, flag 4 marks `loop_start`, flag 1 ends after the block, and flag 3 also sets an exclusive `loop_end`. With `honor_flags=false` (the stream driver's view) every block is played. |
| `decode_vag(Bytes)` | Header plus body; `Pcm::rate` is the header rate. |
| `parse_vat_directory(Bytes, vat_size)` | 0x50-byte little-endian `dialog.bin` records. The next record's offset closes an entry; an empty-name terminal record must equal the VAT size; offsets must be 0x800-aligned and ordered; the channel flag must be 0 or 1. |
| `find_vat_entry(entries, name, language)` | Exact-case name. The last record in the current language wins; if there is none, the last language-0 record. |
| `decode_vat_entry(Bytes, entry)` | Mono blocks are sequential. Stereo is 0x400 bytes left, then 0x400 bytes right, with separate decoder history per channel. The directory's loop-restart word does not change the decoded samples. |
| `stream_duration(bytes, stereo, rate=24000)` | `bytes/16*28/rate`, halved for stereo. |
| `pan_atten(pos, camera, right, near=360, far=840)` | `atten = clamp((far-d)/(far-near), 0, 1)`, `pan = clamp(dot(delta, right)/far, -1, 1)`, `left = atten*(1-pan)`, `right = atten*(1+pan)`. |

## Playback model

`Playback` is deterministic. Its state changes only through owner calls and `tick()`.
One `tick()` is one displayed 60 Hz sound frame and mixes exactly
`frames_per_tick = 800` stereo frames at 48 kHz into a FIFO (bounded at 16 ticks; the
oldest frames are dropped and counted in `dropped_frames()`). `drain(n)` removes up
to `n` frames and never changes model state, so the device's pull size cannot change
what is heard.

`g_soundFrame` starts at its image, 0. `tick()` reads it before incrementing.
Queue service runs when the low two bits are clear, so the first tick services.
End-flag reads run when the low three bits are clear; a voice that ends during
the first mix is reclaimed on the tick that enters with the counter at 8.
`audio_sendVolumes` is due after the increment, when the new value's low four
bits are clear. The payload of that call is not documented, so the model counts
`volume_sends()` and does not change the mix. `tick()` does not run
`music_updateDucking`: one call is executed, but where it is called, how often
and in which order against the queue service (which changes the stream statuses
it reads) are not documented, so the owner calls `step_duck()` explicitly.
`g_drawFrame` starts at 0x400 and
moves only through `advance_draw_frame()`. The two counters are independent.
The stream-driver timer still steps volumes at output sample 480, then every
288 samples. That timer is not the frame cadence.

A new `Playback` holds the process-start images. `end_session()` is a client
session boundary, not an original routine: it stops the three streams as
`audio_streamStop` does, ends effect voices and sequences, drops the level
directory and discards unread output, and keeps the process-lifetime words
(both frame counters, the duck words, the music volume, the stream volume words
and the driver timer). The level-load advances of those counters were not
executed and are not applied.

### Effects

| Call | Behavior |
|---|---|
| `add_effect(name, Pcm)` | Mono samples matched without case; a duplicate keeps the first registration and returns `false`. |
| `play_sample(name, pitch_scale, volume, optional position, plays_while_frozen)` | `sound_playSample`. Unresolved name → −1 (silent). Pitch register `min(trunc(base_pitch*scale), 0x3fff)`, advanced as an integer 12-bit accumulator per 48 kHz frame. Loops come from the sample's own flags 6/3. A one-shot whose two gains times volume are zero is not started. Voices are taken round robin among the 43 free voices; there is no stealing. Returns the voice index. |
| `start_sequence(name, volume, position, group)` | One of 32 slots, group caps 2, except group 14 = 9 and groups 50/51 = 8; group 0 has no category cap; groups outside 0..98 throw. The handle is `0x8000 \| generation<<8 \| slot`, and the generation is 7 bits. |
| `stop_sequence(handle)` / `release_sequence(handle)` | A stale generation is rejected. Release makes the sequence repeat until stopped; it does not let it play out. |
| `set_sequence_position`, `set_camera` | Recompute the gains of moving and followed sounds. |
| `set_frozen(bool)` | Voices without plays-while-frozen stop advancing (pitch 0). |
| `animation_event(mask, ClipRole, sample, position)` | `classify_animation_event`: mask 0x1 on an `attack` clip is a **hit**; 0x4 or 0x8 on a `locomotion` clip is a **footstep**; everything else (combo strikes 0x2/0x4 on attacks, 0x80 death words) is `none`. The caller supplies the sample: in the original, sounds come from `.anm` sound tables, `sound_playHit` and `Player_footstep`/`sound_playFoley`, not from the masks. |

Non-positional samples use unit gains on both sides. Effect gain is
`volume × sfx_volume × side gain` on normalized PCM. The mapping of the game's float
volume to SPU register values is not documented and is not claimed.

### Streams

There are three streams. Each has an eight-entry queue, a driver volume and target, and
a `scale` (0x100 = 1, applied as `volume*scale >> 8`). At construction the volume is 0
and the target is 0x3fff, as in driver init command 0x20. Each timer step moves the
volume toward the target: −0x20, or +0x40 on stream 0 and +0xc0 on streams 1/2.

| Flag | Model |
|---|---|
| 0x1 loop | On a named entry, when a completed read leaves remaining at 0, the file position becomes `offset + span` and remaining becomes `length - span`. `span` is `(restart & 0x7ffffc00) << 1`: 0 rewinds to the entry offset, and a larger value skips in 0x800 steps and can wrap. A restart that lands outside the decoded entry keeps the cursor and plays no invented samples. A registered (non-VAT) loop still restarts at sample 0. |
| 0x2 fade current | Sets the volume target to 0 and stays queued until that word reads 0, including when the active name is already empty. Then the entry is cleared, the target returns to 0x3fff, and the queued entry starts. Flag 2 with an empty name is the stop marker (`queue_stop`). A full fade takes 512 steps ≈ 3.07 s. `stream_stop` clears the queue and the active name and does not write the volume word. |
| 0x4 wait | Start after the current entry ends. |
| 0x8 hold | Stays queued and `stream_status` reports 0 until `release_stream` clears the bit. It is not promoted while held. |
| 0x20 | Stored and returned. Suppresses the level music poll. While stream 1's status has this bit, ducking holds its target (below). |
| 0x40 | Stored and returned. The service does not branch on it. `apply_recall_stop` appends a stream-2 stop when the status has this bit. |
| 0x80 | Stored. `cutscene_run` / `cutscene_end` hard-stop stream 2 when the gate is set and this bit is set without 0x100. |
| 0x100 | Stored and returned. The service does not branch on it. With the cutscene gate set, `cut_runFrame` and `cut_end` leave that stream-2 entry alone. |
| other bits | `std::invalid_argument`. |

`gated_flag_entries()` stays 0. An entry with neither 0x2 nor 0x4 still waits while
one is active: that case was not settled. Streams play
at the fixed pitch 0x7fe (23,976.5625 Hz). `stream_samples_played` and `stream_time`
(samples / 24,000, the game's timing rate) give the position, which is 0 when the
stream is inactive. With two-channel output on (the default), stereo plays left/right.
With it off, both channels are mixed onto both sides. Mono is played on both sides.
Malformed media is dropped at start and counted in `failed_stream_entries()`.

Sources: `add_stream` (registered PCM), `set_global_vat` (`MOUTH.VAT`) and
`set_level_vat`. VAT entries are decoded only when they start.

### Music selection

| Call | Original |
|---|---|
| `set_music_volume(bits)` | `g_musicVolume` 0x4f8944 as a raw float pattern, stored as given with no clamp. It starts at 0.5 (`0x3f000000`): `save_init`'s `config_setDefaults(-1)` replaces the ELF word 1.0 before any session, so no music request reads 1.0. The owner supplies the session value: `boot_music_volume(file_word)` (below) or a slider value. Music reads it; nothing refuses. |
| `music_volume.hpp` | `t3/research-combat-ctors` 22408a1 `docs/spec/music-volume.md` and `tools/ref_music_volume.py` (executed against the original, 0 mismatches). Only the music word of the options image `g_savedConfig` +0x08 and the live global are modelled; the other 16 option scalars and the button rows are outside it. `MusicVolumeWords`: `set_defaults(-1)` stores 0.5 in both words (other `which` values leave them); `browser_copy(mode, word)` is `memcard_browser` step 29, where a non-zero mode (boot passes 1) copies the options file's +0x08 into the image bit for bit with no clamp; `add_core_tasks()` is `game_addCoreTasks` at every session start (live = image); `options_screen(state, word)` is `memcard_optionsScreen`, where only state 2 writes both words. `boot_music_volume(optional word)` runs that boot chain: 0.5 with no options file, else the file word. `slider_step` / `menu_slider_call` are `menuList_handleInput` type 4: the per-slot timer gains `0x3e4fdef8` capped at 1.0 and idles below it (four idle calls); held `0x408000` subtracts `0x3c088888` and clamps to 0, then held `0x802000` adds it and clamps to 1.0. From 0.5, down reaches 0 after 60 steps and is first below the gate at step 54 (`0x3d4ccc08`); up reaches 1.0 after 61. `config_commit_music` is `config_commitOptions` with every other option equal: a differing live value is copied into the image unless `queryOnly`, so a slider change survives the next session start. `music_apply_volume` is `music_applyVolume`. `music-volume-exact` replays `native/tests/vectors/music_volume.txt.gz` (`tools/generate_music_volume_vectors.py`, pinned blobs) bit for bit. |
| `music_play_global(name, flags)` | `music_playGlobal` 0x2110e0: stream 2, `flags \| 0x22`, only when `g_musicVolume` compares `>= 0x3d4ccccc` (`c.olt.s` skips; the pattern `0x3d4ccccc` itself is queued, as are `0x7fffffff` and 1.5; 0, -0 and every negative pattern are not) and the row's length is nonzero. |
| `music_play_level(name, flags)` | `music_playLevel` 0x215f48: the same gate, OR and length check. |
| `play_menu_music(wait)` | `frontend_run`: global `mx_maintheme_l.vag`, flags 5 if waiting, else 1. The first request now checks the documented music gate (`t3/research-menus-layout` 8f528ce, Codex-written and unaudited); it queues nothing below the gate. |
| `boss_music_name(type, object)` / `play_boss_music` | The 11-row `creature_bossMusicName` table (the type is compared without case; the cyclops object names are compared the same way). No boss name is a no-op. Otherwise stream 2 is stopped, then one `mouth.vat` span is queued with `music_playGlobal(name, 0x103)` → 0x123. A second boss replaces the queued span. A closed gate, or a missing or zero-length row, leaves the stream stopped and queues nothing. |
| `stop_boss_music(type, object)` | Appends the empty flags-2 marker only when that creature has a boss name. `stop_boss_music()` is the marker alone, for a caller that already decided to stop. |
| `set_duck(argument)` / `step_duck()` | `music_setDuck` writes `1 - argument` to the target. `step_duck()` is one `music_updateDucking` call, made only by the owner. Each step, while stream 1 status has bit 0x20, sets the target to `0x3e4cccd0` and adds `0x3c888888`, which holds at `0x3e5ddde1`. The current scalar eases by `0x3c23d70a` and snaps if one step would pass the target. `actintro` with a nonzero language byte stores the scalar as integer 0. A changed scalar is sent only when stream 2 status has bit 0x20; the channel word is `scalar * g_musicVolume * 0x3f480000`. Arithmetic is the R5900 truncate model. The channel word is recorded in `music_channel()` and does not change the mix: how the driver volume RPC 0x80 applies it to the output is not documented, so stream 2's gain stays `volume*scale`. |
| `poll_level(draw)` | `mainloop_pollAmbientAudio`. The countdown is decremented; at zero it resets to `14400 + draw` (draw 0..7199 from the cosmetic RNG, supplied by the caller), and unless stream 2's current or queued flags contain 0x20, the first level entry containing `_env_` or `mx_theme` is queued with `music_playLevel(name, 0x102)`. When `g_drawFrame`'s low four bits are clear and stream 0 is idle, the first `amb_` entry is queued on stream 0 with flags 3. The same draw frames are when the group-volume refresh is due; table `0x41bac8` is not specified, so no group volumes are written. The process image of the countdown is `0x2a30`. A level load's store of that word was not re-executed, so `set_level_vat` still takes the caller's countdown. The poll does not advance `g_drawFrame`. |

Voice lines (stream 1, `0x28` then `release_stream` at the dialog key) and the intro
soundtrack (`porko_l.vag` on stream 0, held) use `queue_stream` and
`release_stream` directly.

### Still gated or not modeled

- **Cutscene ±0.1 threshold** and **intro movie drift** (`movie_update`) are not
  implemented. The held intro start is still `queue_stream`.
- **48 kHz labelling:** a VAT record's declared rate is stored and is not used as
  the stream pitch. Whether that label means 48 kHz playback is not settled.
- Not modeled: the `sound_muteFor` countdown, delayed sample starts, `.anm`
  multi-event sound tables (a sequence here is one sample), the values in group
  volume table 0x41bac8, the `audio_sendVolumes` payload, how the duck's
  `audio_setChannelVolume(2, ...)` word reaches the output, SPU memory exhaustion
  (silent address 0x5000), SPU four-tap interpolation and the final hardware mix.
  Reverb is off in every observed level row, and none is applied. An entry with
  neither flag 2 nor flag 4 still waits while one is active.

## Godot binding

`ChampionsNativeAudio` (RefCounted, in `gdextension/src/native_audio_bridge.*`) wraps
one `Playback`. Use an `AudioStreamGenerator` with `mix_rate = 48000`; the model
does not resample.

```gdscript
var audio := ChampionsNativeAudio.new()
var generator := AudioStreamGenerator.new()
generator.mix_rate = 48000
$Player.stream = generator
$Player.play()
audio.attach_playback($Player.get_stream_playback())
audio.register_vag("hit_flesh.vag", vag_bytes)          # bool, see get_last_error()
audio.set_global_vat(mouth_dir, mouth_vat, language)
audio.set_level_vat(level_dir, level_vat, language, initial_countdown)
# Every displayed 60 Hz frame:
audio.set_camera(camera.global_position, camera.global_basis.x)
audio.sound_tick(cosmetic_draw_0_to_7199)               # poll, tick, then advance g_drawFrame
audio.pump_frames(4096)                                 # FIFO -> generator
```

| Method | Returns |
|---|---|
| `register_vag(name, bytes)` | bool |
| `set_global_vat(directory, vat, language)` / `set_level_vat(directory, vat, language, initial_countdown)` | bool |
| `attach_playback(AudioStreamGeneratorPlayback)`, `set_camera(position, right)` | — |
| `play_sample(name, pitch_scale, volume, position, positional, plays_while_frozen)` | voice or −1 |
| `start_sequence(name, volume, position, group)` | handle or −1 |
| `stop_sequence(handle)`, `release_sequence(handle)`, `set_sequence_position(handle, position)` | bool |
| `animation_event(mask, clip_role, sample_name, position)` with clip_role 0 attack, 1 locomotion, 2 other | voice or −1 |
| `queue_level_entry(stream, name, flags)`, `queue_global_entry(stream, name, flags)`, `queue_stop(stream)` | bool |
| `release_stream(stream)`, `set_stream_scale(stream, scale)`, `set_two_channel_output(bool)`, `set_frozen(bool)`, `set_sfx_volume(volume)` | — |
| `set_music_volume(volume)` | bool; a slider-range value: the slider clamps to 0..1, so others are refused. The mixer starts at save_init's 0.5 |
| `set_options_file_music(bits)` | bool; the options file's +0x08 word as a 32-bit pattern, copied with no clamp through `boot_music_volume` |
| `end_session()` | — ; keeps the process-lifetime counters and duck words |
| `play_menu_music(wait)`, `play_boss_music(type, object_name)`, `stop_boss_music()` | bool |
| `get_stream_time(stream)` | seconds at 24 kHz |
| `get_gated_flag_entries()` | count |
| `sound_tick(cosmetic_draw)` | true if the level poll queued an entry |
| `pump_frames(max_frames)` | frames pushed |
| `get_last_error()` | String; cleared by successful registration calls |

`sound_tick` follows the displayed-frame order: poll at the current `g_drawFrame`,
then `sound_update`, then `gfx_endFrame` increments `g_drawFrame`.

## Verification

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R native-audio --output-on-failure
godot --headless --path godot --script res://tests/native_audio_smoke.gd
```

The `native-audio` CTest case runs 211 checks, including ADPCM, stereo and gain
vectors produced by `tools/ref_audio.py` from the same authored bytes, exact
driver-timer fade timing, queue flags, music selection and determinism across drain
sizes. The Godot smoke test pushes PCM through a real `AudioStreamGenerator` headless.
No sound has been heard or captured from this client. Acoustic parity, private-disc
acceptance and a scene-level integration (level change, speech, 3-D motion, boss
transition) remain unverified.

## Playable integration

The released core is composed in [PLAYABLE_AUDIO.md](PLAYABLE_AUDIO.md). The
Godot bridge additionally exposes read-only `get_observation()` for native clock,
stream/voice and bounded-output checks. Playback algorithms and documented gates
are unchanged. The play scene uses synthetic banks and explicit ClipRole events.
