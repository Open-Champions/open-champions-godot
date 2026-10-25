# Play-scene audio composition

Wave 9 composes the released [native audio](NATIVE_AUDIO.md) implementation with
play-session ready ticks and [creature animation](PLAYABLE_ANIMATION.md).
Reference-function coverage unchanged; client integration only.

## Clock and authority

`ChampionsSessionController.play_audio_tick(packet)` emits exactly once after
successful ready advancement in ordinary, combat and level play, including every
step of a catch-up batch. Warm-up submissions, pause, stopped/refused steps and
legacy diagnostics emit nothing. Each packet owns its tick, level generation
key and animation observations. No mutable World handles or RNG are exposed.

`godot/audio/playable_audio.gd` routes events before calling native `sound_tick(0)`.
That method polls level policy then mixes exactly 800 stereo frames at 48 kHz.
The constant cosmetic draw is an authored client choice; game RNG is untouched.
The main thread drains the bounded 12,800-frame FIFO into a Godot
AudioStreamGenerator (`ChampionsNativeAudio.pump_frames`, called from `_process`); the
audio thread only mixes that generator's ring buffer. The native `Playback` is not
synchronised: it must never be called from the audio thread. Accelerated/headless soaks can drop old presentation PCM
when the device cannot consume it; native audio clocks/events still run once per
ready tick. This is not an audio latency or real-time performance claim.

The entry menu has its own capped 60 Hz presentation accumulator while no session
has started. Once play begins, audio only advances from ready ticks, including
when returning to selection with a retained paused session. Pause lets already
queued device PCM drain but freezes all further mixer/queue progress.

The client keeps one native mixer per process. `g_soundFrame`, `g_drawFrame`
and the duck words are process-lifetime words (research
`docs/audio-playback.md` "Executed frame counters"), so new play, reset and a
successful continuation load call `end_session()` instead of restoring the
process images: streams are stopped as `audio_streamStop` does (the stream
volume words stay), voices and sequences end, and the level directory is
dropped. This session boundary is client policy. The loading-screen
`g_drawFrame` advance and `game_loadLevel`'s two `sound_update` calls were not
executed, so the counter phase at level entry is not the original's and no
advance is applied. The saved ready tick becomes the client's new origin. No PCM/voice/queue state is serialized,
and native continuation bytes and deterministic state remain unchanged. Invalid
loads do not replace audio. The read-only native `get_observation()` bridge
reports clock, stream names/flags/queues, voices, FIFO/drop/error counts for checks.

## Typed cues and fixtures

ClipRole is explicit: 0 = attack, 1 = locomotion, 2 = other. It is never inferred
from a mask. The existing stationary animated combat fixture selects attack
clips for its two creatures and idle for the player. Its real native mask-1 events
allocate two synthetic hit voices on ready tick 37, alongside exact combat damage.
Stale poses are not re-emitted. No invented player movement/action dispatch was
added. The existing optional native footstep callback remains available.

The scene receiver accepts locomotion 0x4/0x8 and passes them to the native typed
classifier. Attack 0x4 is a combo event, not a footstep. A separate headless native
composition test drives the real generated FatCyclops state-1 walk dispatch,
entry 16, over authored clips: 0x4 at tick 13 and 0x8 at tick 49 each allocate a
footstep voice and produce PCM on that same tick. This is dispatch/sound evidence,
not reconstructed FatCyclops AI or movement in the stationary combat fixture.

All VAG/VAT bytes are authored at runtime in `synthetic_bank.gd`. Test tones are
substitutes, including entries named for documented menu/boss selectors. The
step/hit sample associations, fixture countdown of 1 (the process image is 0x2a30;
the level-load store was not re-executed), zero cosmetic draw and
-18 dB preview output volume are explicit client fixture choices. No retail
PCM, sound banks, music or original animation sound-table associations are present.

## Music and retained gates

`g_musicVolume` starts at save_init's 0.5 (`0x3f000000`, research
`docs/spec/music-volume.md`), the value of a boot with no options file, and
`playable_audio.gd` mirrors it in `music_volume_bits`. Its owner may call
`load_options_file_music(bits)` with the options file's +0x08 word (copied with
no clamp) or `set_music_volume(volume)` with a slider-range value. The scene
sets no value, so level and boss music play at 0.5; a value below `0x3d4ccccc`
closes the gate for both. Menu music does not read the volume.

The real native selector queues `mx_maintheme_l.vag` on entry. Play installs an
explicit synthetic level directory; its first `mx_theme` entry is selected on the
first poll. Its fade bit waits until the kept stream-2 volume word reads 0, and
it starts on the next 4-frame queue service after that. `amb_` selection uses `g_drawFrame`'s low four bits. Level-generation changes
reinstall the synthetic level bank without guessing asset names from the WORLD path.

The scene's explicit “Synthetic boss music (Broon)” control calls
`boss_enter("FatCyclops", "Broon")`, which uses the documented native lookup and
0x123 flags. A second call replaces the queued span. `boss_leave()` queues the
native stop marker. These are also service hooks for future callers with documented
boss identities. Current combat carriers are not silently classified as bosses.

Flags 0x40 and 0x100 are stored. Recall slices fade-stop stream 2 when status bit
0x40 is set, and a cutscene gate leaves a stream-2 entry alone when bit 0x100 is
set. VAT loop-restart spans follow the executed contract. Music ducking is
not run by the client: the call site and order of `music_updateDucking` are not
documented, and its channel word is not applied to the mix.
Intro movie drift, the cutscene ±0.1 threshold, the 48 kHz labelling question,
mute-for, delayed starts, original ANM sound tables, group-volume table values,
SPU exhaustion, interpolation and final hardware mixing remain unimplemented.
No reverb is added. No playback-heard, speaker capture, original-sound or retail
parity claim is made.

## Checks

`playable-audio` is a native CTest over real dispatch and audio modules (163 checks).
The released `native-audio` suite and generator smoke remain intact.
`playable_wave_smoke_audio.gd` extends all earlier combined checks and captures:
entry/library/load, 36,000+ ready ticks, pause/save/load/continue, manual animation,
art/HUD/inventory, fight/death/removal, gate travel/saved-level return, animated
hit timing, FX and textured native lighting. Added checks cover menu/level/boss
music, same-tick attack voices, typed footstep receiver, pause, catch-up batches,
continuation origin and bounded PCM. UI is driven through real control signals
and native methods; captures are from the actual exported app viewport.

Godot 4.5.2 intermittently reports `AudioStreamGeneratorPlayback` references
at immediate shutdown, also present in the released owner's isolated smoke.
The bridge detaches playback and stops the player before teardown. Verbose local
shutdown evidence identifies one playback object in the isolated smoke and three
after the combined reset/load flow; this warning is retained as
an engine/bridge teardown limitation, not silently reported as a clean exit log.
The assertions and process exit status pass; actual heard output is unverified.
