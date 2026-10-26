STATUS: MORE — MINOP-1, MINOP-2 and MINOP-3 repaired; full gameplay acceptance remains blocked by explicit unsupported paths.

# Minotaur audit repair, round 2

## Scope and ownership

Work remains on `codex/port-minotaur`, starting at `a9929b68`. The interrupted
five-file patch was inspected, retained and finished, not discarded. No sibling
checkout was edited, no processes were killed, and nothing was pushed or opened
as a pull request. Private validation inputs remain external and read-only.

## MINOP-1: authoritative missing-service boundaries

- `Creature_init` now records its refusal and throws `combat_runtime::Unsupported`.
  Ordinary campaign construction preflights this unbound service before the
  Minotaur class constructor, combat adoption or actor registration. It no longer
  claims successful construction with default model bounds.
- A sight-approved `Creature_hitTargetInArc` request still counts, then throws
  when damage delivery is unbound. No target or failed sight remains the
  researched no-hit path, not an invented zero-damage outcome.
- Required holder publication still counts, then throws from the injected drop
  service. The existing holder clears its hand bit only after successful return;
  regression coverage verifies that failure preserves the equipped bit.
- A contained Minotaur service failure freezes subsequent authoritative class
  ticks. The adoption regression verifies a single attempted run, a recorded
  refusal and no subsequent shared-memory writes.
- The boundary installer takes the already-installed Creature service model.
  Calling `World::creature_services()` again reinstalls constructor callbacks;
  the retained interrupted patch initially reset the very overrides being tested.
  This is corrected without changing the shared World's service-install policy.

Evidence: `research-minotaur` at
`27eb6ae309c72e15a31fc4b04d012b833f15031f`,
`docs/spec/minotaur.md` sections 3, 7, 8 and remaining-service boundaries;
`tools/ref_minotaur.py`; existing `combat_weapon_holder.cpp` slot 11 ordering.
The same pinned class arguments and sight gate are retained. No replacement
gameplay constants, offsets, damage formulas or bounds are invented. Fixture
positions, resource labels and observation handles are authored synthetic data.

## MINOP-2: shared actor-lifetime reconciliation

The starting commit already incorporates the audited boar
`owns_live_object` / `register_actor` helpers, guarded constructor cleanup and
const registry-access policy. These are reused for both adoption branches, not
duplicated. Incoming retired or foreign/same-address objects are rejected before
constructor/shared-memory effects. Registration also checks the Actor's saved
object serial. Retirement checks map membership, pointer, serial and live state
without first dereferencing a stale object pointer.

The pinned integration commit `3550e1bf71bb89504593084d08c380404e70393a`
was merged locally into this feature branch. Existing integration changes are
retained, including death-flow diagnostics and test registration. Conflicts in
the play-mode label, shared header includes, test registration and combined
Minotaur/death probe diagnostics were resolved to preserve both sides. No new
actor-lifetime subsystem was introduced.

Evidence: the audit's `research-campaign-combat` pin `328b072`,
`tools/ref_objects.py` construction/destruction and `tools/ref_core.py` pool
reuse contracts; audited boar helper ancestor `7cb5bee` and pinned integration.
Synthetic adoption regressions cover both foreign same-address and retired
Minotaurs, real destroy/reallocate/readopt, shared HP and boar ownership.

## MINOP-3: presentation lifetime and resource identity

- Both model caching and instance binding use the same identity tuple: runtime
  archive, model entry, visibility mask and texture entry.
- A changed identity retires/detaches the old MeshInstance3D and binds a fresh
  mesh/material instance. A previous untextured fallback cannot override the
  replacement's textured materials.
- Absent actors are retired, not hidden indefinitely. An unavailable new model
  also retires the old body instead of leaving an earlier occupant visible.
- The authored Godot regression observes independent archive/model/mask/texture
  changes, unchanged-key reuse, absence, same-address replacement-class reuse,
  fallback-to-textured material rebinding and unavailable-model retirement.

Evidence: the pinned object/pool-reuse contracts above, plus
`research-minotaur` section 3 variant selection and section 5 resource/visibility
selection. This is presentation cache/lifetime logic, not a new game mechanic.

## Gates

- Release native configure/build (`cmake -S native -B build/native
  -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j 4`): PASS after
  integration reconciliation. Logs: `configure-integrated.log`,
  `build-integrated.log` under the evidence directory.
- Full `ctest --test-dir build/native --output-on-failure -j 3`: **250/250 PASS**,
  817.90 seconds, no failed tests or timeouts. Log: `ctest-full.log`.
- Pre-integration focused Minotaur/actor/fight suite: **5/5 PASS**. The new
  boundary case initially exposed the interrupted patch's repeated service
  installation and failed, then passed after the installer correction. The
  initial build also exposed a call to a private Model method; preflight now
  invokes the required installed callback directly. These are fixed failures,
  not unexecuted or waived gates.
- Release Godot extension configure/build (`cmake -S gdextension -B
  build/godot-extension -DCMAKE_BUILD_TYPE=Release`, then build `-j 4`): PASS.
  This is the optimized development/editor-ABI extension, not a release export
  or a claim of cross-platform validation. Log: `build-godot.log`.
- Godot editor `--headless --editor --import`: exit **0**, no error/warning
  entries in `godot-import-complete.log`. This replaces the initial bounded
  `--quit-after 3` import check, which warned that scanning was aborted on exit.
- Post-integration Godot 4.5.2 enemy-identity regression: **22 checks / 0
  failures**, exit **0**, separately headless and rendered. The rendered run
  uses the Compatibility renderer; it checks live mesh/material bindings, not
  original-game pixels or Minotaur animation.
- Godot `scene_play_flow_smoke.gd`: **65 checks / 0 failures**, exit **0**.
  `playable_wave_smoke_play.gd --ready-ticks=640`: **1,341 checks / 0 failures**,
  exit **0**. No script errors occur in these affected checks. The continuation
  fixture logs an ObjectDB exit-cleanup warning; this is not relabelled as an
  entirely warning-free run.
- A private native construction probe completes, exit **0**: six attempted
  class placements each report the required `Creature_init` refusal, **zero
  Minotaurs registered**, and no fight startup error. This checks the actual
  factory path, beyond the synthetic callback-only boundary regression.
- `python3 tools/run_loop_playthrough.py --godot <Godot> --game-folder
  <read-only disc> --out work/minotaur-r2/playthrough`: both variants complete
  and write their results, but both exit **1** and the wrapper exits **1**.
  These are **blocked gameplay gates, not passes**. Headless: **53 steps**;
  rendered: **54 steps / 37 PNG captures**; **5 refusal kinds** in each. The
  wrapper observes **994 files before / 994 after** in the private input.
- Both journeys end with the same incomplete rows: `walk_in_exit_trigger:
  refused`, `enemy_fight: blocked_unported_combat` (missing
  `auto_container_gold_source` and `gold_gain`), and `player_death:
  blocked_death_reload` (no valid checkpoint/reload service; no respawn).
  Both record a real native damage kill and verified diagnostic death sequence.
  These same three outcomes are recorded in the pinned integration's
  `work/loop-playable-integration-r32-report.md`; no unrelated port is attempted
  and no expected-refusal rule is weakened to make this gate return success.
- Inspected the actual rendered approach, post-fight and reload-refused captures
  (`04b-fight-approach.png`, `05-fight-attempt.png`,
  `13-player-reload-refused.png`). The updated construction-refusal text wraps
  visibly, rest-pose bodies are displayed, and reload remains visibly refused
  after the full fade. No rendered live Minotaur is claimed: ordinary
  construction is intentionally refused. Both journey logs contain ObjectDB
  exit-cleanup warnings, but no `SCRIPT ERROR` or `ERROR` entries.
- `git merge-tree --write-tree HEAD 3550e1bf`: exit **0**, no conflicts;
  the pinned integration commit is an ancestor of this branch.

Local implementation checkpoints: `013b35d6` (missing-service refusals and
presentation replacement), `915953fb` (pinned integration merge), `ba3c58be`
(remaining probe reconciliation), and `72da0688` (fixture UID and probe
formatting). The final native build/probe also runs after the formatting change.

Detailed private logs and screenshots stay under ignored `work/minotaur-r2/`;
only this counts-only report is staged for publication.

## Remaining gameplay boundaries

These repairs are not a complete Minotaur encounter. Required bounds-derived
initialization remains unbound, so ordinary campaign Minotaurs refuse
construction. Live damage descriptors/delivery, held-part physical publication,
animated appearance/root trajectories and complete loot-to-pickup-to-credit
remain unverified or unported. Diagnostic class/unit fixtures do not establish
retail gameplay, rendered Minotaur animation or PCSX2 parity. A successful
playthrough process can still truthfully report `blocked_unported_combat`.
