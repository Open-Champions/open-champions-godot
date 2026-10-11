# Raw32 animation decoding and owned cache lifetime

The standalone RTA decoder parses supplied clip bytes, allocates and touches
decoder slots, decodes animation tracks and returns joint positions/quaternions
using the simulation core's integer-backed arithmetic. It is separate from the
binary64 presentation sampler, animation clock and Session adapter. It has no
Godot, scene, sound, pose-cache or animation-root consumer.

## Contract and lifetime

The executable Python bodies are the contract: `ref_anim.py` at
`f516ac798bf28375554fe53ffb31b74802844136` (SHA-256
`6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8`),
with `ref_core.py` at `41e847af5360724ab62d208594f8027ea3c1b686`
(SHA-256 `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`).
The published animation specification is pinned at
`f2b996dded898f1b0b558b901cab875f775cfdc7`; its checksum and all protocol,
generator and reference bindings are in the canonical vector manifest.

Each nonmoving Context owns stable clip, track and slot identities. Equal clip
bytes retain distinct identities. A supplied-context `decode_clip` retains its
temporary track because the original slot owner still points to that cell.
Snapshots include complete clip metadata and bytes, every track, every slot's
64 joint cells, all stale fields, cursor/tick/yaw/owner values and LRU order.
Allocation preserves old fields except the original documented resets. An
all-protected pool rotates N+1 times before HostFault; those rotations survive
the failure. A track that does not restart uses the slot's prior clip, including
when its own clip identity changed. Failed decoding retains executed stores.

ROOT and YAW flags preserve decoder-local baseline and protected-slot behavior;
they do not establish root/yaw extraction. The caller selects the core FPU
configuration. Simulation never uses presentation floating-point operations.

The published prose recommends stopping at closing delta255. The executable
model treats 255 as ordinary data and continues until its actual bit bounds
fault. This implementation preserves that behavior without clamping time or
repairing streams. The discrepancy is reported; protected research is unchanged.

## Host limits

The native policy separately refuses foreign identities, malformed ownership/LRU
graphs and bounded resource exhaustion. Defaults allow 64 MiB per input,
128 MiB retained clip payload, 1,024 clips, 65,536 tracks, 1,024 slots and
65,536 key records per decode (hard maximum 1,048,576). Key work is charged before
each delta read, including lookahead, preserving the stores already executed.
These host refusals are distinct from original-model HostFault/IndexError and
do not claim original behavior outside the admitted finite domain.

## Optional private files

After building the native targets, explicitly select your own extracted RTA
animation directory:

```sh
build/native/champions-animation-decoder-validate --game rta --input-dir /private/extracted/anm
```

The tool recursively counts `.anm` files, refuses symlink inputs and reports
decoded/refused counts with three requested times per parsed clip: zero, half
length and max(length-1, zero). Empty input reports zero coverage. Optional
`--fpu-config 0..15` selects arithmetic. The default report contains no paths or
asset values. `--observations` emits derived private metadata and cache state;
keep that stream private. No assets or real-file observations are committed.
File acceptance does not prove playback, semantic model pairing or retail
playability. Norrath requires its own independently validated contract.

## Local verification

The input-driven oracle contains 769 original-Python cases in 22 groups, with
arithmetic cases across all 16 core configurations and structural probes counted
separately. Three fresh generators reproduce both canonical files; seven atomic
publisher tests pass. Optimized and ASan/UBSan traces match every original
expected byte, including identities, stale cells and failure prefixes. Their
expanded SHA-256 is
`6e5f81991b038f5d740bde35acf1b2b9a82de46cde6dc7b589abde3e7642528c`
and FNV is `17ab671a8f9e835f`.

The authored API has 182 passing checks, including bounds, stable references,
copied snapshots and retained temporary owners. All four linked runtime units
are instrumented in the sanitizer trace and API runs; leak detection is disabled
on this Mac, so no leak claim is made. The 21-test replay transport suite includes
an actual POSIX output-size failure; the unavailable Mac `/dev/full` case skips.
All 93 then-registered CTest cases pass in one 264.08-second run; the subsequently
registered private-input test is verified separately. Strict comparator tests
reject omitted state even when five hosts agree and refreshed hashes are supplied.
The final Python run passes 543 tests with 303 explicit skips; the dedicated
native private-input run passes eight tests with only unavailable Mac `/dev/full`
skipped. A real POSIX file-size limit verifies final aggregate-output failure.
LLVM20 formatting of six C++ paths, GCC15 strict syntax of runtime/API/replay/
validator, the 56-path simulation boundary, universal2 extension build and actual
source 70 scene/283 browser checks pass. The scene is unchanged.

Optional owner-file comparison covers 4,087 unique private RTA clips at config0:
4,085 succeed at all three requested times, and two retain the original matching
refusals. All 12,261 sample results, including three HostFault prefixes, match
complete Python observations. A deterministic 64-clip sample also matches across
all 16 configurations (1,024 clip/config cases, 3,072 results). Inputs remain
unchanged. These unique counts are separate from the earlier 4,510 animation
occurrences. This check used the same frozen decoder before validator-only
flush, bounded-reserve and private-error-output fixes; the final validator has
separate authored regressions. No original asset values or observations are
published.

Local traces carry an explicit uncommitted source label and are not final-head
remote platform artifacts. Cross-platform release evidence requires the actual
commit's native and Godot workflows and independently inspected artifacts.
