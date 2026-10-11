# Shared random stream for persistent base combat

This restricted RTA composition lets a registered post-`runObjects` task apply
the published combat damage body to Session's persistent base-creature health.
Combat draws directly from the existing object World's `SharedRandom` instance.
The object, combat and wire models identify that original shared domain as
`g_randSeed` at `0x404464`. No seed is copied into or out of a second generator.

It is an authored integration policy over pinned Python contracts. Original
attack controls, player skills, derived dispatch, movement, collision, death,
loot, presentation and a playable scene remain work. Norrath behavior and
original-console networking need independent evidence.

## Supported setup and calls

Enable objects and health, then call `enable_base_combat` during setup with
explicit focus coordinates and cutscene state. Configure each participating
health binding with signed 16-bit armor. Both victim and attacker must be
configured, live nonplayer base creatures. Existing allocation serials,
addresses and generations retain their separate identities.

Only a ready scheduler task after priority-10 `runObjects` may call
`apply_base_damage`. Inputs use integer raw arithmetic patterns and signed
32-bit attack. The fixed descriptor is empty and critical hits are disabled.
Permitted call flags are `0x40`, `0x4000` and `0x8000`. Blocking, thorns,
player/projectile/effect forms and unconnected owner, skill, spell, reflection
or damage-over-time paths refuse before combat effects. Unexpected service
branches raise an error rather than supply invented game behavior.

The existing health call budget also bounds combat calls. The restricted path
has at most seven shared draws, one base-health dispatch and one sound event;
the integration reserves 64 aggregate history entries before entry. The
existing network bookkeeping reserve remains separate.

## Exact ordering and owned observation

For fixed guaranteed-hit damage, the original body consumes six shared draws:
two independent hit tests and four zero-probability element checks. A ranged
damage roll adds a seventh. The two hit tests can disagree. Misses still reach
the original zero-amount base-health operation and its bookkeeping. Music,
sounds, handles, pending damage and failure prefixes follow the published body.

Snapshots retain the direct authority's semantic identity, current state and
ordered combat-attributed returned draws and resulting states. The dormant
standalone combat seed, cosmetic seed and legacy authored Session seed remain
distinct observations. Existing object operations see combat's changes
immediately. Network synchronization observes the actual shared authority at
the established publication boundary.

The binding routes the modeled combat draw sites through `draw_shared`.
Standalone code that explicitly accesses `World.seed` still accesses its
fallback storage. Session keeps the Adapter private and installs fixed
services, so its accepted combat path cannot bypass the shared authority.

The new trace schema includes complete Session, World, network, health,
registry and backing state plus combat configuration and authority state.
Legacy schemas explicitly reject an active extension; their frozen helpers
and fixture bytes stay unchanged. No callback or host pointer escapes through
an owned snapshot.

Execution failures preserve completed draws, stores and handle effects, publish
the retained scalar prefix and latch Session against retry. Allocation failure
after combat draws does not restore an earlier seed or health graph.

## Verification

The independent generator verifies immutable core, object, network, combat and
creature models before executing their original bodies. Its direct Python view
delegates to the original object's shared generator. It does not implement a
second RNG or use native results as expected values. Existing snapshot helpers
are independently hash pinned. Numeric recipes cover all sixteen arithmetic
configurations; literal anchors independently check interleaved draws and both
hit-test orders. Full state records, not wire hashes alone, gate native parity.

Local validation passed all 59 CTest cases (56 existing and three new), 443
API checks and all 276 complete contract cases in both the optimized build and
ASan/UBSan with all 23 linked simulation runtime modules instrumented. The
computed trace bytes are identical to the Python expectation:
`f16fc61ac4c61c175bc61b0f5feaf9ee61c5bd846e3413d94b87f7fec63bc08f`.
Leak detection was disabled because macOS LeakSanitizer is unsupported; these
results establish address and undefined-behavior checks only.

Three independent full generator processes produced identical compressed vectors
and manifests. The Python suite ran 234 tests with 108 explicit environment/native
skips; the native transport cases ran through CTest. All 17 owned C++ files pass
LLVM 20 formatting; affected runtime units and both new entrypoints pass GCC 15
syntax checks. The simulation boundary check covers 42 runtime files.

The existing native CI compares all complete traces on macOS arm64/x64, Windows
arm64/x64 and Linux x64. Completed platform results belong to the implementation
pull request; this local evidence does not establish unexecuted hosts or a
playable game.
