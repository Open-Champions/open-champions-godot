# Original RTA creature turning

`creature_movement::Model::turn_toward` implements the original
`Creature_turnToward` spring and slow scaling over the module's existing owned
or borrowed sparse Memory. It returns the signed 16-bit angle to the target
before the turn. Mode 1 selects the opposite direction; other signed modes use
the ordinary target. Only target X/Y are consumed.

The body uses the original raw turn rate at +0xb0, angular velocity at +0x110,
facing at +0x38 and slow byte at +0x2fc. The spring stores angular velocity
before any limit clamp. Slow values 1..60 scale the facing step after the
spring; other values bypass scaling. The second slow-byte read and stored
velocity reload are preserved. Signed angle wrapping, negative rates, clamp
ordering and every executed store follow the published model without repair.
Local intermediate arithmetic uses portable integers to avoid C++ overflow;
all game numeric operations use the existing integer-backed core FPU.

Turning makes no callbacks or random draws, changes no frame counter and does
not establish object validity or a virtual-slot dispatcher. It adds no fields,
registry, wall-clock delta or presentation arithmetic. The borrowed Memory's
owner and optional overlay retain their existing lifetime and write policies.

## Immutable contract

- `ref_creature.py`: `a0d89502ce1431375a6fc045b91a73039114e6b9`, SHA-256
  `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7`.
- `ref_core.py`: `41e847af5360724ab62d208594f8027ea3c1b686`, SHA-256
  `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`.
- `docs/spec/creature-tick.md`: the creature revision above, SHA-256
  `bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e`.

The oracle executes the pinned original `turn_toward` and `slow_factor` bodies.
It loads Git objects with hash checks, not mutable research files or new
decompile interpretations. The reference-map identity is
`44e47186b857b04c9295b480cea89369af4697e2467e650aa6d20773f9ad8dfb`.
The independent fixture policy is `rta-creature-turn-v1`, with no imported
helpers. Its complete protocol and generator checksum are pinned by the suite
registry and vector manifest.

## Observations and refusals

Inputs explicitly supply initial pokes and turn/poke/store commands. Native
replay computes state from these commands before consulting expected columns.
Every step observes submitted input, every sparse byte including explicit
zeros, Memory store history, Model writes/calls/ordered events, draw count,
callback/external names, policy, borrowed ownership and overlay presence.

The existing host limits remain separate from original-model parity: unsigned
32-bit memory spans, bounded sparse bytes, histories and operation depth.
Late address failure preserves prior velocity writes. A Model history refusal
precedes its next write; an overlay or Memory store failure retains the attempted
Model write/event and Memory store log according to the existing Memory policy.
There is no rollback, repair, retry or new failure latch.

Replay separately caps fixture size at 16 MiB and line size at 64 KiB. Complete
output is published from a reserved sibling temporary directory only after
every result, final count, digest and write check passes. A refusal preserves a
prior output. The finite Python corpus is success-only; native guard failure
prefixes are authored API evidence, not claimed original Python cases.

## Verification and remaining work

The corpus has 1,568 original-Python cases: 98 explicit inputs across all 16 FPU
settings, in 15 groups. Three fresh-process generations reproduce both canonical
files; seven atomic-publication tests pass. Optimized and ASan/UBSan traces match
every original expected byte. Trace SHA-256 is
`e1d03097547e2d93bad39237fcd6a68bb89e9fad7b047d7b51c32a939109e0c5`;
FNV is `5b1957cda14e65f7`.

The 349 authored API checks cover literal spring/clamp/slow results, wrapping,
unchanged RNG/frame state, exact write order, borrowed lifetime, readonly
overlay refusal and late native failure prefixes. Both optimized and sanitizer
runs pass with all five linked runtime units instrumented. Leak detection is
disabled on this Mac, so no leak claim is made. The 20-test transport suite passes,
including an actual POSIX file-size failure; unavailable platform facilities
skip explicitly. Eight contract tests and the 75-test comparator suite reject
missing hosts, changed pins/protocols and omitted state even when five reports
agree and their hashes are refreshed.

All 97 CTest gates pass in one full 762.27-second regression run, including the
existing Session, movement, animation and asset checks. The final Python run
passes 578 tests with 323 explicit skips. GCC15 strict syntax for runtime/API/
replay, LLVM20 formatting of five C++ paths, the 56-path simulation boundary,
fresh universal2 extension build and actual source 70 scene/283 browser checks
pass. No new scene or visual behavior is claimed.

Local reports carry an explicit uncommitted source label. Exact-head remote
platform and actual-artifact gates remain required before merge. Session ready
tasks, controller/Godot integration and original player or AI dispatch are
separate follow-ups. This is RTA evidence; Norrath needs independent validation.
Turning parity does not establish retail playability or PS2 networking.
