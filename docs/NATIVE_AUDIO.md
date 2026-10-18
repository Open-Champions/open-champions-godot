# Native audio (Return to Arms)

`champions-native-audio` (`native/include/champions/native_audio.hpp`) is a
Godot-independent C++17 port of the Champions: Return to Arms (SLUS-20973 v2.00)
audio contracts in the research branch `t3/research-audio`, commits
`a9593d2..834806e`: `docs/formats/audio.md`, `docs/audio-playback.md` and
`tools/ref_audio.py`. Gameplay hook masks come from `t3/research-anim`
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
| `decode_vat_entry(Bytes, entry)` | Mono blocks are sequential. Stereo is 0x400 bytes left, then 0x400 bytes right, with separate decoder history per channel. A nonzero loop-restart value is rejected (gated). |
| `stream_duration(bytes, stereo, rate=24000)` | `bytes/16*28/rate`, halved for stereo. |
| `pan_atten(pos, camera, right, near=360, far=840)` | `atten = clamp((far-d)/(far-near), 0, 1)`, `pan = clamp(dot(delta, right)/far, -1, 1)`, `left = atten*(1-pan)`, `right = atten*(1+pan)`. |

## Playback model

`Playback` is deterministic. Its state changes only through owner calls and `tick()`.
One `tick()` is one displayed 60 Hz sound frame and mixes exactly
`frames_per_tick = 800` stereo frames at 48 kHz into a FIFO (bounded at 16 ticks; the
oldest frames are dropped and counted in `dropped_frames()`). `drain(n)` removes up
to `n` frames and never changes model state, so the device's pull size cannot change
what is heard.

Order inside `tick()`: reclaim ended voices when `frame % 8 == 0`, service the stream
queues when `frame % 4 == 0`, then mix 800 frames. The stream-driver timer steps the
volumes at output sample 480 (10 ms after init), then every 288 samples (6 ms).
Counting starts when `Playback` is constructed. The phase of the 4-, 8- and 16-frame
cadences relative to the original's frame counter is not documented. The model
uses multiples of the frame number.

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
| 0x1 loop | Restart at sample 0 (all observed loop-restart values are zero). |
| 0x2 fade current | While the current entry plays: target 0 every service round until the volume is 0, then clear the stream, set the target back to 0x3fff and start the entry. Flag 2 with an empty name is the stop marker (`queue_stop`). A full fade takes 512 steps ≈ 3.07 s. |
| 0x4 wait | Start after the current entry ends. |
| 0x8 hold | The entry starts silent and paused until `release_stream`. |
| 0x20, 0x80 | Stored. 0x20 suppresses the level music poll. |
| 0x40, 0x100 | **Gated.** Stored, never interpreted, counted in `gated_flag_entries()`. |
| other bits | `std::invalid_argument`. |

An entry with neither 0x2 nor 0x4 also waits for the current entry. The contract does
not say whether it cuts the current entry, so this choice is unverified. Streams play
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
| `music_play_global(name, flags)` | `music_playGlobal` 0x2110e0: stream 2, `flags \| 0x22`. |
| `music_play_level(name, flags)` | `music_playLevel` 0x215f48: stream 2, `flags \| 0x22`. Its volume gate is not traced and is left to the caller. |
| `play_menu_music(wait)` | `frontend_run`: global `mx_maintheme_l.vag`, flags 5 if waiting, else 1. |
| `boss_music_name(type, object)` / `play_boss_music` | The 11-row `creature_bossMusicName` table (the type is compared without case; the cyclops object names are compared the same way). Queued with `music_playGlobal(name, 0x103)` → 0x123. |
| `stop_boss_music()` | Stream 2 stop marker. |
| `poll_level(draw)` | `mainloop_pollAmbientAudio`, once per displayed frame. The countdown is decremented; at zero it resets to `14400 + draw` (draw 0..7199 from the cosmetic RNG, supplied by the caller), and unless stream 2's current or queued flags contain 0x20, the first level entry containing `_env_` or `mx_theme` is queued with `music_playLevel(name, 0x102)`. Every 16th call, if stream 0 is idle, the first `amb_` entry is queued on stream 0 with flags 3. The initial countdown is passed to `set_level_vat`, because its original value is not documented. |

Voice lines (stream 1, `0x28` then `release_stream` at the dialog key) and the intro
soundtrack (`porko_l.vag` on stream 0, held) use `queue_stream` and
`release_stream` directly.

### Gated or not modeled

- **Boss override order:** `play_boss_music` refuses a second boss track while one plays
  or is queued. The separate stream-2 stop that `creature_startBossMusic` sends before
  queuing is not modeled beyond flag 2.
- **Queue flags 0x40 and 0x100:** carried but not interpreted (above).
- **Intro movie drift:** only the held start is available. Video-to-audio resync and the
  cutscene ±0.1 track correction threshold are not implemented.
- Not modeled: music ducking (the target is recorded only as "about 0.2"), the
  `sound_muteFor` countdown, delayed sample starts, `.anm` multi-event sound tables (a
  sequence here is one sample), group volume table 0x41bac8, SPU memory exhaustion
  (silent address 0x5000), SPU four-tap interpolation and the final hardware mix.
  Reverb is off in every observed level row, and none is applied.

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
audio.sound_tick(cosmetic_draw_0_to_7199)               # poll_level, then tick()
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
| `play_menu_music(wait)`, `play_boss_music(type, object_name)`, `stop_boss_music()` | bool |
| `get_stream_time(stream)` | seconds at 24 kHz |
| `get_gated_flag_entries()` | count |
| `sound_tick(cosmetic_draw)` | true if the level poll queued an entry |
| `pump_frames(max_frames)` | frames pushed |
| `get_last_error()` | String; cleared by successful registration calls |

The order of `mainloop_pollAmbientAudio` relative to `sound_update` within a frame is
not documented; `sound_tick` polls first.

## Verification

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R native-audio --output-on-failure
godot --headless --path godot --script res://tests/native_audio_smoke.gd
```

The `native-audio` CTest case runs 185 checks, including ADPCM, stereo and gain
vectors produced by `tools/ref_audio.py` from the same authored bytes, exact
driver-timer fade timing, queue flags, music selection and determinism across drain
sizes. The Godot smoke test pushes PCM through a real `AudioStreamGenerator` headless.
No sound has been heard or captured from this client. Acoustic parity, private-disc
acceptance and a scene-level integration (level change, speech, 3-D motion, boss
transition) remain unverified.
