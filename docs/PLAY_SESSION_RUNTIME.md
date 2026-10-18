# Play-session runtime

The opt-in `rta-play-session-v1` runtime supports the existing singleton dry
movement/health pipeline with bounded observation windows and independent
continuation saves. This is authored client policy, not original RTA behavior.
The default diagnostic constructor, its limits, complete histories and existing
save formats remain unchanged. The 60 Hz simulation and numerical bodies are
unchanged.

Baseline: `60def02263f2bcb9685cb06c5a0c7409ac5ccab4`, branch
`codex/session-play-runtime`. The coordinator authorized the narrow
`objects_lifetime.cpp` checkpoint extension on 2026-10-17 after round 1 found
its legacy 2,000-frame / 1,000,000-iterator limits. The brief records that scope.
The default-false `Checkpoint::play_continuation` selector enables separate
range validation; it does not change legacy validation or field bytes. Frame
and iterator serial are live state and never reset during history retirement.

## Closed retention policy (specified before runtime edits)

The intended `rta-play-session-v1` domain is exactly one existing dry base
creature, one local peer, the existing movement/health input task and explicit
RTA arithmetic. Recovery, facing, animation cache, ordinary animation, scripts,
items, external callbacks, remote peers, dynamic contacts and multi-actor combat
are refused in this mode. All old diagnostic configurations remain available.
The fixed simulation step stays 60 Hz.

Use a configurable 1..128 successful-ready-frame window, default 128. These are
authored retention bounds. At an idle successful ready boundary, a full window's
observation records may retire together, after all consumers have completed.
Never retire on a failed or partially executed frame, paused submission or
reentrant call. Allocation/validation failure latches stopped and preserves the
remaining reportable prefix. A failure while growing a retained stream stops the operation; the surviving
prefix remains observable and cannot be saved as a successful continuation.

Expose absolute submissions, ready ticks, damage events, retired-through frame,
retirement count and per-stream retired-record counters as checked unsigned
64-bit values. An explicit overflow/resource refusal is preferable to wraparound.
Preserve the original raw32 frame projection limit; the new mode must stop before
that live numeric domain is exhausted, even with bounded histories. Snapshot
metadata must say `windowed`, distinguish complete retained suffix from
unavailable prefix, and give absolute ranges. An empty suffix after retirement
must never be described as an empty complete history. Legacy snapshot APIs must
refuse the new mode rather than present partial histories as legacy complete data.

## Collection-by-collection audit

| Owner / retained collection | Role and consumer | Policy for the closed play domain |
|---|---|---|
| Controller `pending_damage_frames` | Deferred rising edges indexed by genuine input frame; `ready()` consumes matching edges | Keep all pending frames, maximum four; never retire with logs. |
| Controller `animation_commands` | Deferred commands and consumed diagnostic history | Optional cache policy refused; require empty. |
| Controller `initial_terrain` | Immutable setup used by explicit Reset | Preserve exactly, with existing terrain resource caps. |
| Session `events` | Ordered task observations; counted by `history()` and snapshot/trace | Retire completed-window records; preserve errors and absolute offsets. |
| Session `seed.draws` | Observations of seed draws | Expected empty here. Keep seed state and absolute draw count; refuse a domain mismatch. |
| Session `object_tasks`, callback bindings | Frozen scheduler dispatch, including Controller's captured owner | Keep one authored task and known callback identities; never serialize function pointers. |
| Session `tasks`, `messages`, `definitions`, `items`, `scripts`, item arena | Other runtime configurations, some owning live state | Require absent/empty; do not retire them as diagnostics. |
| Network peers, record/sync/message rings, pads, previous pad words, codecs, input locks, positions | Fixed-capacity live input/protocol state, including not-yet-ready input | Preserve every field and slot. No wire-codec or online behavior changes. |
| Network `handlers`, `direct_handlers`, callback fields | Dispatch authority | Preserve closed constructor-owned callbacks; reject arbitrary replacements. |
| Network `outbox`, `log` | Outbound delivery records and errors | Expected empty for the healthy singleton. Unexpected contents block retirement; do not discard unsent packets or errors. |
| World owned object storage, live map, owned-object map, pool, handle nodes/free list, heads, grid cells, name tables, task list | Current allocation, identity, generation and scheduler membership | Preserve all state and pointer relationships; exactly one live retained actor. Fixed grids/pool remain bounded. |
| World `iterators_`, `completed_iterators_`, extra cells | Graph identities, including stale references to ended iterators | Keep all referenced storage. Only existing `prune_iterators()` proves unreferenced ended storage removable. Preserve `iterator_serial_`. |
| World spawn arrays, centres, pads and callback captures | Current world/query state | Preserve; no new memberships or mutable external captures. |
| HP bindings/object pointers and raw-handle registry/overlay | Shared World-to-Memory identity and generation checks | Preserve singleton binding, reservations, weak lifetime and overlay ownership. Never reset at a window boundary. |
| HP sparse Memory `bytes_` | Authoritative current bytes and explicit backing | Preserve every byte and existing memory cap; do not reconstruct from recent writes. |
| HP Memory `log_` | Attempted ordered stores, including failures | Retire only a successful completed window; reset retained-budget prefix only for the named play policy, and count absolute retired stores separately. |
| HP model `writes`, `calls`, `events` | Ordered observations of the original HP body | Retire together at the boundary; preserve model scalars, draw count, limits, callback table and borrowed relationships. |
| Combat Adapter `entries`, `regions`, handle callbacks | Typed object ownership, generations, raw spans and service identity | Preserve; fixed singleton constructor domain. |
| Combat Adapter callback-frame stack | Active projection/reentrancy expectations | Must be empty at retirement; depth zero. Never clear a live stack. |
| Combat World events and seed/cosmetic draw histories | HP/combat observations and separate random streams | Retire event records; draw histories expected empty. Preserve RNG states and counters; no reseeding or shared-random substitution. |
| Creature movement `writes`, `calls`, `events`, `callback_results` | Ordered movement observations; result records refer to call indices | Retire as one window; retain separate absolute call/result offsets and validate each suffix reference. Never independently clear one side. |
| Creature model callbacks, binding, initial dispatch table, locals/draw count, last velocity/result | Live services and continuation state | Preserve all; borrowed Memory remains the HP owner's Memory. |
| Terrain Level arrays, grid references, samples, path cells | Immutable selected geometry and query inputs | Preserve existing hard caps and supplied-terrain identity. |
| Terrain stamp, element stamps/cached rectangles, last result | Mutable query state | Preserve exactly, including wrap behavior already implemented. |
| Recovery/turn/animation/cache collections | Optional live state plus independent histories | Refuse these configurations in initial play policy. Never silently disable them. |
| Save `actions_`, deferred Source, prefix history counts | Legacy complete replay/journal and deferred history reconstruction | Keep old behavior in diagnostic mode. Play uses a separate direct continuation envelope and no growing replay journal or deferred source. |
| Bridge current/pending Scene, prepared terrain/visual/save candidates, interpolation poses | Atomic host publication and presentation | Preserve existing transaction ownership; replace only a fully validated candidate. Clear input and snap presentation after successful publication. |

Per-ready-frame retained observations in the closed dry policy are bounded by the
existing conservative 96-entry allowance. A 128-frame window plus the existing
operation/network reserves is below the 65,536 Session history cap. Capacities,
not only vector lengths, must be measured after warm-up. Small-window tests must
compare ordered emitted records against the pinned preexisting Python recipe,
normalizing only the explicit history offsets. Output capture is bounded by the
test; runtime must not silently spill to disk.

## Continuation format and validation

Reserve a distinct magic (`OCPLAY\0\0`) and independent version 1; do not consume
or reinterpret replay/snapshot version numbers. The implemented reader/writer keeps the 32 MiB file cap and existing
terrain/Memory/graph resource caps. Encode explicit little-endian values:
profile and arithmetic configuration, selected initial terrain, window policy,
absolute counters/ranges, current state and the bounded retained suffix. No
action journal or replay is needed. Include checked section lengths and CRCs;
CRCs detect corruption and do not authenticate input.

Load into a fresh candidate with the known callback graph, validate all lengths,
profile/version/policy, collection capacities, current-state/HP projection,
network/Controller/World frame relationships, graph identities, callback indices
and history ranges, then require canonical re-encoding before publication.
Failures preserve the running scene and prepared terrain. Saving a failed prefix
is refused; the prefix remains observable. A continuation is never a complete
historical snapshot. The exact byte layout is described below.

The bridge API is `start_play_session() -> Dictionary` plus
`get_play_capabilities() -> Dictionary`, with explicit mode/history fields and
save-format support. A successful start preserves committed terrain/visuals,
starts fresh state, releases input and snaps interpolation. Reset in play keeps
the selected terrain. Capabilities reject the unsupported optional diagnostics. New observation fields
identify mode, window, retained counts/capacities and absolute retired offsets.

## Source evidence

The existing `session_control_creature` manifest pins the immutable public
reference bodies below. They were read with `git show` and SHA-256 verified in
the read-only research checkout. Original numeric behavior is unchanged.

| Source | Commit | SHA-256 |
|---|---|---|
| `ref_core.py` | `41e847af5360724ab62d208594f8027ea3c1b686` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `ref_objects.py` | `0dd02ce416389dd440540119727d946636507703` | `541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2` |
| `ref_creature.py` | `a0d89502ce1431375a6fc045b91a73039114e6b9` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |
| `ref_world.py` | `6831458ee7ee12bb166d1dce4eb8e4340095bc48` | `30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8` |
| `ref_netwire.py` | `54258f9d7e281ef07f0675040e82e6264f9386cf` | `4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42` |

Private retail validation remains unavailable; no alternate assets were sought.
Reference-function coverage unchanged; client integration only.

## Round 1 local measurements

Apple M5 Pro, 48 GiB RAM, arm64 macOS 27.2 (26B5112a), Release, four
build jobs; other workers were actively testing on the same machine. The
new probe passed 527 checks. Its standalone World scheduler reached frame
2,001 with iterator serial 8,004; the checkpoint refused that frame with the
exact message above after a successful canonical roundtrip at frame 2,000.

The unchanged dry Controller workload alternates direction every 32 submissions
and raises damage every other submission. The first submission is not ready.

| Ready ticks | Retained history records | Sparse backing bytes | World links | Iterator serial | Cumulative workload microseconds |
|---:|---:|---:|---:|---:|---:|
| 128 | 6,656 | 124 | 34,981 | 512 | 1,052,299 |
| 256 | 13,312 | 124 | 34,981 | 1,024 | 2,222,585 |
| 512 | 26,624 | 124 | 34,981 | 2,048 | 3,956,792 |

Counts include all history containers in the existing creature-control test
recipe; shared-random draws are absent in this closed policy. World links are
current graph state, not disposable history. The 1,162-byte complete replay
roundtripped identically and reconstructed all 26,624 history records. Total
probe elapsed time was 7.94 seconds; process peak RSS was 49,807,360 bytes,
including the World boundary probes, snapshots and replay reload. These
measurements are an unchanged diagnostic baseline, not evidence of a bounded
play runtime, a 36,000-tick soak or a before/after improvement.

The same Release probe also passed 527 checks in an x86_64 binary under
Rosetta on this Apple Silicon host, with identical retained counts, frame
boundary and 1,162-byte replay length. Its 512-tick workload took 6,207,147 us;
whole-probe elapsed time was 13.72 seconds, peak RSS 112,930,816 bytes. This
is not physical Intel execution or a complete cross-architecture state-trace
comparison. The independent pinned Python World scheduler also passed the
2,000-to-2,001 transition using its original `tasks_run`/`run_objects` bodies.

Release configure/full native build, the registered boundary CTest, the
simulation-boundary validator, its 37 unit tests and formatting/whitespace
checks passed. The full 149-test native suite was not run: its local wait queue
was cancelled before CTest started when stopping at the scope blocker.
No extension was built and no export was produced for this blocked audit.
Those acceptance gates remain open; no sibling test result is borrowed.

## Round 2 implementation decisions

Retirement occurs on the next active submission after a complete successful
window, while the controller is still idle. Thus every emitted window's final
frame remains inspectable before the next submission. Pause/save do not retire.
Per-stream retained indices are window-relative, with checked absolute retired
offsets carried separately. All 15 observation streams retire together. Failed
frames never retire. `snapshot()` and legacy save entrypoints refuse play mode;
`window_snapshot()`/`continuation_snapshot()` explicitly return partial history.
The existing public diagnostic Config defaults remain unchanged.

Round 2 now implements the opt-in `Config::play_window` (zero means legacy),
`Controller::window_snapshot`, `Scene::continuation_snapshot`, and
`Scene::save_continuation`. The new World checkpoint gate is
`Checkpoint::play_continuation`, default false. Legacy World validation and
byte order are unchanged. Absolute frames are limited to `0x7ffffffd` to stop
before signed/raw frame overflow; iterator identities retain their existing
63-bit identity domain. Neither counter is retired or reset.

The implemented continuation-v1 envelope is explicit little-endian: 8-byte
`OCPLAY\0\0`, u32 version 1, u32 RTA profile 1, u32 total length, u32 CRC32
(checksum bytes zeroed), u32 window, u32 setup byte length + canonical legacy
zero-action setup envelope, u32 checkpoint byte length + eager complete current
state and bounded suffix. Checkpoint starts with u64 retired-through-ready,
u64 retirement count and 15 u64 retired stream counts, followed by the existing
explicit Controller/Session/World field traversal. No continuation journal or
deferred source is retained. Legacy save/snapshot entrypoints refuse play.
Setup must leave 4 MiB within the 32 MiB cap for retained history; every encoder
and loader still checks its total bound. Canonical re-encoding, CRC, version,
profile, counter relationships, graph links, memory projection, history ranges
and callback indices validate before publishing a loaded owner.

`native/tools/play_session_trace.cpp` adapts the preexisting independent
151-case creature-control recipe. Windows 1 and 2 collect emitted suffixes
only inside the bounded test, translate callback indices by explicit retired
offsets, and compare every reconstructed full state and ordered observation
against the unchanged Python fixture. The single diagnostic-cap recipe stays
in diagnostic mode; the other 150 cases use play. A continuation load at ready
tick 2 is inserted without resetting the Python expected state. This tests the
new path against original expectations, not expectations generated by new C++.

## Round 3 bridge and export evidence

The locally built Release extension passes `play_session_runtime.gd` (23 checks),
including 600 ready ticks, preserved terrain/visual owners, input release,
interpolation snap, pause, continuation save/load and equal future commands,
atomic bad-save/unsupported-diagnostic refusal, terrain replacement and Reset.
The actual exported macOS app passes the same 23 checks headlessly and 27 checks
with rendered captures. Four captures were inspected: movement at tick 600,
paused HP 98, loaded moving HP 94 and selected-terrain Reset HP 100. Interactions
were driven by the test through existing scene input handlers and bridge calls,
not physical keyboard/mouse or native file-dialog interaction. The unchanged
baseline shell still labels its save selector Replay; scene-play-flow owns the
play UI and must label the new format using the capability response. No scene
script was edited by this job.

The export used pinned Godot 4.5.2, locally compiled godot-cpp bindings at
`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`, template_debug ABI with optimized
Release native code, and an isolated staged project. It ran arm64 on Apple M5
Pro/macOS 27.2. A universal Godot template is not proof of a universal extension.
The bridge implementation and exported verification are specific to macOS;
no Windows, physical Intel or retail gameplay claim is made.

The failure-only native probe passes 20 checks: forced retirement allocation
failure preserves the complete pre-retirement suffix and latches stopped;
failed candidate allocation leaves the current owner unchanged; refreshed-CRC
unknown version/profile/window, impossible absolute counters and truncation are
refused. Full matrix/parity, soak and final regression evidence follows in round 4.
