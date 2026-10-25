STATUS: MORE — six Minotaurs construct in the shared World and accept player
damage. Class-local state, animation/event, holder and loot argument contracts
are implemented, but delivered enemy damage, animated presentation, held-part
publication and the complete loot/pickup journey remain refused or unverified.

# Minotaur implementation continuation — 2026-10-25

Worktree `port-minotaur`, branch `codex/port-minotaur`, base `252f34a6`.
Only this checkout was written. No push, branch creation, PR, private-input
modification, or process-name termination was performed.

## Commits and evidence

- `1b431e95`: preserved the 23 recovered files after a Release build and the
  two Minotaur tests passed, within the first few minutes of this continuation.
- `0e9220af`: fixed the audited pooled-address actor lifetime defect, plus two
  incorrect recovered integration assertions (last attacker is `+0x2e4`, not
  the attached-spell head; removing a combat view does not unregister its owner).
- `23296f50`: selected the player-position activity mode; an unset origin-camera
  focus previously kept nearby Minotaurs inactive. Added counts-only activity
  and targeting diagnostics to the existing private probe.
- `4ec80fd0`: documented boundaries and strengthened direct reconstruct/adopt
  regressions so they do not rely on a preceding lookup to retire stale entries.

Primary evidence is `research-minotaur` at
`27eb6ae309c72e15a31fc4b04d012b833f15031f`: `docs/spec/minotaur.md`, its authored
reference and two executed difftests; `audits/research-minotaur-fix.md` PASS.
The shared rules, project instructions, contributing guide, roadmap and both
requested sibling-port audits were read. Unknowns in the research are not
relabelled as observed implementation behavior. No decompiler output, retail
bytes or extracted assets were imported.

See `docs/MINOTAUR.md` for implementation and field contracts. Native changes
cover the class module, campaign actor registry/factory/fight services, inherited
combat service bridge, holder drop seam and synthetic tests/vectors. Godot and
the GDExtension expose runtime model identities, masks, scale, HP and class;
the presenter draws the actual selected class body in a rest pose.

## Validation commands and results

All evidence below is local; ignored details are in `work/minotaur-final/`.

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: PASS. Rebuilt again after the activity fix.
- `cmake --build build/godot-extension -j 4`: PASS, Release development extension;
  final activity-mode rebuild also passed.
- `ctest --test-dir build/native -R '(minotaur|campaign-actors|campaign-fight)'
  --output-on-failure -j 3`: final 4/4 PASS. `minotaur-class`: 1,502 cases /
  19,557 checks; `minotaur-stats-exact`: 8,250 cases. Both seeds 20973 and 7.
- `ctest --test-dir build/native --output-on-failure -j 3`: the first run had
  242/243 pass and exposed the recovered integration assertions above; corrected
  assertions and expanded reuse regressions passed the focused rerun. A clean
  full-suite rerun is recorded in `ctest-final.log`; final result pending below.
- The same 54-command Godot development matrix as the base, with only its output
  directory changed to `current-matrix/`: **54/54 exit 0**. Combined Full combat
  has **2,620 checks / zero failures**, save/load **300/300**. The existing
  `scene_play_flow` fixture still emits two missing-`loaded` script errors, and
  `ui_layout_smoke` emits its existing four-versus-five-argument glyph error.
  Those files are unchanged; exit 0 does not mean their logs are error-free.
- `FULL_TRACE`: **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged.
- The base's four-slice `run-sweep.sh`, output redirected to `current-sweep/`:
  **111 tried / 111 loaded / 111 played, 230 checks, zero check failures**;
  all four commands exit 0; 111 script attachments, 76 script starts. These
  counts do not establish that all script commands or combat classes are ported.
- `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder
  "$RTA_DISC" --out work/minotaur-final/exclusive-playthrough`: final result
  pending below. A prior overlapping rendered probe caused the rendered journey
  to pause on focus loss (415 ticks, no swings); it is not counted as a combat
  success. The journey was rerun with no other rendered validation window opened.
- Native private probe: `--near-minotaur --ticks 900` constructs six Minotaurs,
  damages one through the ordinary player damage path and reports no World
  failure. Placement is diagnostic; HP and damage are not boosted in that run.
- Activity-only probe `--near-minotaur --attack-every 0 --ticks 120`: six built,
  two active, two targeted. This does not prove a delivered attack or live AI
  parity; the observed Minotaurs remained in state 0 in this bounded placement.
- **Diagnostic only:** `--near-minotaur --weak --ticks 900` starts nearby enemies
  at 1 HP to reach the death path cheaply: one Minotaur reaches state 20/dead,
  one loot call, two held-part requests, one XP award. It is not a natural kill.
  Subsequent corpse movement records `object has no grid node` and remains
  refused rather than being replaced with a guessed movement body.
- Rendered isolated encounter probe (`visual.gd` in ignored evidence): six
  Minotaurs, selected body drawn with five surfaces, one screenshot. Inspected
  `visual/minotaur-encounter.png`: the textured Minotaur body is visible next to
  the player in the actual viewport, in rest pose without held parts. Camera and
  placement are diagnostic; this is not animation/terrain/root-motion parity.
- In-memory scan of the 27 selected definition identifiers against newly added
  diff lines: **zero matches**. A whole-file scan also sees shared identifiers
  already present in unchanged boar code; no such identifier was newly added.
- Disc count in each playthrough: **994 before / 994 after**, read-only.

## Remaining refusals and missing evidence

1. Delivered Minotaur player damage and runtime descriptor contents remain
   unverified by the pinned class execution; the hit helper gate is retained and
   the missing final effect is counted, not invented. The target-attacking-me
   predicate refuses when its untraced branch is reached.
2. Bounds-derived init, live animated blend/root trajectory and held-part
   attachment transforms remain unavailable. The visible model is explicitly a
   rest pose. Held-part drop arguments and holder mutation are implemented;
   physical ground-part publication is not.
3. Gold scatter/pool/constructor, item ground objects, pickup and credit remain
   unbound. Gold chance is forwarded as raw `0x3f199999`; a seed reaching Gold
   must still refuse its missing services. The ordinary playthrough remains a
   boar kill, not Minotaur end-to-end kill-to-pickup evidence.
4. Host construction still refuses non-`name` Minotaur properties, non-default
   difficulty/multiplayer sessions and unsupported rank/tint/equipment paths.
   Formula fixtures across levels/difficulties do not enable those sessions.
5. No PCSX2 capture, cross-architecture replay or exact animation/loot parity
   claim is made. The isolated rendered probe also logs an exit-time ObjectDB
   leak warning; it exits early rather than running the normal journey cleanup.

No unrelated sibling port or branch was edited. The remaining work needs the
named shared-service contracts/captures, not arbitrary constants or no-op
implementations.
