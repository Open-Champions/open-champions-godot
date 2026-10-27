STATUS: FIXED — NA-R3-1, NA-R3-2 and NA-R3-3 addressed; live mode-B match remains BLOCKED.

# Native arena round 4

Reviewed on 2026-10-27 in `codex/native-arena`, starting clean at `4693246c`.
Fixes: `190ffce6`; discriminating regressions: `cba64f24`;
recorded full native gate: `4693246c`. All commits are local only.
Research authority: research-arena `17810bb` and research-arena-match
`d4f58da6377f1011068ba80d3608dbe1e3149370`. The named audit was read in full.
The final review required no further native or Godot source changes.

## Findings and regression evidence

- **NA-R3-1 FIXED.** World player death branches on last-attacker handle
  presence. A present handle routes its object to `on_player_killed`; an absent
  handle routes the player slot to `lose_life`, with prediction exclusion.
  This preserves mode-A kill decrement when both modes are enabled, rather than
  treating an absent handle as the standalone scorer's null-killer input.
  Authority: pinned `tools/ref_combat_ptick.py:437-442`,
  `tools/ref_arena_modes.py:113-168`; research-arena `docs/arena-modes.md:10-18`
  and `41-64` establish the combined-mode domain, score layout and starting lives.
  The regression creates a Full World with one and two players, applies lethal
  unattributed damage through `take_damage` / `process_damage`, checks actual
  death and zero HP, and independently checks kills wrap from 0 to `0xffff`,
  lives stay 2, and the other player's kills stay 0. The old caller would credit
  slot 0 with one player and omit the decrement with two. These are World
  behaviour checks, separate from the standalone reference scoring vectors.
- **NA-R3-2 FIXED.** Saved-object walking returns before state access unless
  flag `0x20` is present; empty-wave walking reads state only when
  `(flags & 0x68) == 0x20`. The remaining saved-object classification is unchanged.
  Authority: pinned `tools/ref_arena_match_core.py:45-67` and
  `docs/spec/arena-match-core.md:28-34,48-60`; the all-object walker domain is
  documented in `tools/ref_objects.py:905-916`. The bounded synthetic non-creature
  fixture uses the documented `0xc0` allocation and `0x220c` flags from
  `docs/spec/gold-run.md:323-335`. Its underlying-memory delegate throws on the
  former state overread at `+0x312`. It runs both actual callbacks, verifies no
  save or wave block, checks intermission begins, and records zero rejected reads.
  Thus sparse zero-fill cannot hide the audited overread.
- **NA-R3-3 FIXED.** The tracked round-3 report commands now use caller-supplied
  `$RESEARCH_ARENA_MATCH`, `$GODOT_CPP_DIR`, `$GODOT` and `$GAME_FOLDER`.
  Machine-specific research, dependency, runtime and private-disc roots were
  removed in `190ffce6`. Historical round-3 gate outcomes remain historical;
  the resolved native gate is recorded below.

## Gate results (macOS arm64)

- Release native configure/build and three Release `template_debug` extension
  builds completed in the prior continuation; retained logs are
  `work/native-arena-r4-configure.log`, `work/native-arena-r4-build.log`, and
  `work/native-arena-r4-champions-*.log`. Dependencies and runtime are local to
  this worktree; no integration worktree is needed.
- Focused native checks rerun 2026-10-27: **4/4 PASS, 0.12 s**:
  `ctest --test-dir build/native -j 3 --output-on-failure -R
  '^(arena-match-host|arena-match-exact|combat-runtime-2-host|combat-runtime-3-host)$'`.
- Full CTest: **216/216 PASS, 840.58 s**, including `player-exact` (109.72 s).
  Command: `ctest --test-dir build/native --output-on-failure -j 3` on
  `cba64f24`. Recorded in `4693246c`; inspected retained log
  `work/native-arena-r4-ctest-full.log`. This gate was not rerun because no native
  code changed during this review. The earlier round-3 timeouts are superseded
  by this complete pass, not reclassified as passes retroactively.
- Godot dev matrix rerun 2026-10-27: **45/45 PASS**, exit 0,
  129.75 s summed command time. Command:
  `python3 work/native-arena-r4/run-godot.py`; results in
  `work/native-arena-r4/matrix/godot-results.json`, runner log
  `work/native-arena-r4/matrix-review-run.log`. Zero script errors. The UI-layout
  negative test emits the expected wrong-arity diagnostic; it passes.
  Headless checks and the matrix's rendered FX/camera/material smoke commands
  passed; this is no claim of visual live-arena acceptance.
- `FULL_TRACE`: **UNCHANGED**, **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
  Parsed and asserted against the round-3 baseline from the freshly executed
  `godot-combined-full-combat.log`. No script-dispatch, audio-mixer or trace
  implementation changed.

## Remaining live-match blockers and refusals

These fixes establish bounded native contracts, not a playable arena match.
The World frame runner still refuses PvP. Original player/creature/holder and
resource construction, player equipment/records, the full scheduler, wave
construction and Godot mode-B session wiring remain incomplete. No dedicated
arena Godot test exists; the existing dev matrix tests surrounding integration.
The unchanged FULL_TRACE fixture does not enter mode B.

Round 11 original evidence proves assisted kills and one bounded wave handoff;
it does not prove normal-input kills, later creature ticks, a natural completed
match, displayed results, collected rewards, or a successful next-level load.
Saved-record, engine, level and wave services remain explicit. Missing handlers,
nonzero purchase-candidate counts and unestablished qsort tie order refuse.
Destructor flags, shipped recipe/class tables, fixed awards and a final mode-B
victory condition have not been guessed. The round-3 private level sweep is
historical level coverage, not rerun or promoted into live-match evidence.
The loop playthrough runner and Godot script remain absent in this worktree.

No private game bytes, decoded content or absolute private paths were added.
No push, PR, sibling edit or process-name kill was performed.
