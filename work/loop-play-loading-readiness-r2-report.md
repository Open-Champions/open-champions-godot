STATUS: MORE

Round 2 implementation in progress. Assigned branch verified clean at 3278e618.
Research pin: research-loading-readiness@d6f035fab6bb5cbe66f5c72a55a1a6aef05ee8df,
`docs/loading-arrival-r2.md`; research and round-1 audits PASS.

Pending full CTest: Release build and full `ctest -j 3` have not run for round 2.
Godot matrix, unchanged FULL_TRACE and both journey comparisons pending.
No completion or playable-arrival claim.

Implementation checkpoint: production deferred queue already has documented
VM-error continuation; regression executes HALT, bounds/division errors, missing
entry and native-fault stop through Host. Added memory-backed equipment lifecycle,
seven routes, bounded publication, explicit I/O/fixup services and matching model
pair output. Shared World explicit stores now count as runtime publication, while
sparse defaults and executable backing do not. Actual transformation services
remain unbound; no live model-readiness claim. Focused script-commands PASS (0.46 s).
Release native build PASS; initial added shared-World test needed its combat-runtime
link dependency, fixed without changing assertions. Full CTest still pending.

## Per-condition implementation and research evidence

All round-2 references below are to `research-loading-readiness` at exact pin
`d6f035fab6bb5cbe66f5c72a55a1a6aef05ee8df`. These are addressed Observed/Executed
contracts with synthetic original-function execution, not live arrival proof.

- Deferred (`docs/loading-arrival-r2.md:11-35`): retained the real Variables queue's
  result-discard behavior and added pinned source comments. It terminates the
  failing VM entry, continues to the following entry, promotes captured back count
  once, clears back and increments parity on return. Tests execute authored HALT
  4/27/-1, bounds/division errors and a missing entry, observe the following quest
  write and a self-deferred nonempty front after exactly one promotion. Existing
  loading observation requires the returned pass, not queue exhaustion.
- Fault/refusal (`docs/loading-arrival-r2.md:37-54`): an unbound SYSREQ native
  faults the real Host, does not reach the following quest write, does not promote
  counts/parity and cannot certify loading. Existing discarded-command refusal
  counters still invalidate loading completion. No actual arrival command census
  is invented; no unsupported command is granted a successful effect.
- Player/slots (`docs/loading-arrival-r2.md:58-73`): memory-backed publication
  stores player index +0x3c8; non-display-only (+0x462 == 0) publishes into
  record +0x43d8 at 0x4d8af0 + index*0x4c30. Selected-slot publication writes
  +0x43f0 + 4*slot. These bounded slices do not implement inventory selection,
  construction, conflicts, hooks or reconstruction. Client publication accepts
  only exercised indices 0/1, and the inherited 17-slot domain
  (`docs/loading-readiness.md:41-53`). Sparse missing bytes refuse. The shared
  World regression spawns an actual native combat player; explicit combat stores
  are visible to readiness without copying a fabricated player or empty inventory.
- Specialized routes (`docs/loading-arrival-r2.md:75-97`): all seven slot/cache
  routes and index*0x58 stride are implemented; tests exercise actual cache memory
  for both indices, pending and validated pointer outputs. Host scanner consults
  these routes under the documented environment and definition guards and trims
  four characters only from explicitly supplied formatter output. Formatter is
  unbound in ordinary play. Slot-3 selection still refuses. Generic lookup is
  unchanged/refused. The four-draw-frame controls use the actual draw counter
  (`tools/difftest_loading_arrival_r2.py:98-100,134-138`), not a guessed cache age.
- Submission (`docs/loading-arrival-r2.md:101-118`): explicit service entry accepts
  resolved metadata/file location and age/busy observations; neither metadata
  search nor a device result is synthesized. Implements three source-address
  selections, sector offset/size arithmetic, submitted short, model offset,
  completion-pointer storage/clear, request words and active byte. Actual transfer
  callback is mandatory and absent by default. Test observes all request fields,
  all source routes, young/busy suppression and no duplicate submission.
- I/O completion (`docs/loading-arrival-r2.md:120-128`): ordinary busy waits;
  error requests a bound retry and stays pending; idle/no-error writes completion
  and clears active. Streaming requires observed remaining bytes, waits above zero
  and completes at zero. Tests drive real runtime fields; missing retry/streaming
  observations refuse. Offline never completes local media work.
- Cache publication (`docs/loading-arrival-r2.md:130-138`): pending completion
  leaves both outputs zero. Completion requires both fixup services in documented
  order and usable resource validation before identity, valid short and outstanding
  clear are published. Completing call still has zero outputs; next matching
  lookup produces (base+offset,base). Default fixups/validation are unbound and
  refuse even with a completion byte or valid bit. Authored regression services
  transform owned fixture bytes; invalidated bytes revoke readiness. They are not
  evidence of actual resource transformations. Outstanding-cache identity check,
  publication coverage, supported-domain and pointer-overflow checks are explicit
  client safety restrictions, not new claimed original branch conditions.
- Header (`docs/loading-arrival-r2.md:140-144`): count/override selects the two
  documented identifier addresses; metadata/header contents remain unknown.

## Remaining refusals

Destination-specific deferred commands, slot-3 selection, generic model resolution,
actual formatter/header metadata, inventory/constructor effects, media transfer,
resource fixups and usable live models remain unverified/unbound. No empty-table
workaround, placeholder pair, offline I/O completion, ready-bit shortcut or success
fallback was added. Peer/store/save behavior is inherited from audited round 1.

## Verification in progress

Pending full CTest was committed first in `00107865`; implementation checkpoint
`5334d44e` retained the pending note before aggregate execution. Release native
configure/build and Release extension configure/build PASS (`-j 4`). Focused
builtins/commands: 5/5 PASS, 69.52 s; final commands after route controls: 1/1 PASS,
0.46 s. Full CTest uses `-j 3`; exact count/time and timeout reruns pending.
Initial shared-World test link failure was fixed by adding its actual combat-runtime
link dependency, preserving every assertion. Godot and journey gates running.

### Godot and trace checkpoint

Strict isolated aggregate: 58/61 PASS. Same three inherited/setup failures as
round 1: private loading-disc case without required input argument (exit 2),
native-FX capture's absent relative work directory (exit 0 plus PNG-save ERROR),
and `ui_layout_smoke` calling `hud_text_glyphs` with four arguments instead of five
(exit 0 plus engine ERROR). No diagnostic was suppressed. Supplied private input
and capture directory in isolated reruns: both PASS. Three additional rendered
identity/death/loading-present cases PASS. Final public total: **62/63 clean**;
private disc case separately PASS. No test/fixture/baseline was changed.
Evidence: `work/loading-r2-matrix/results.json`,
`work/loading-r2-additional/results.json`, associated logs.

`FULL_TRACE`: **36,402 bytes**, SHA-256
`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged.
`FULL_SAVE_LOAD`: 300/300, identical continued/uninterrupted presentation hashes.
Evidence: `work/loading-r2-matrix/playable_wave_smoke_full_combat.log:17-18`.

Headless journey writes result JSON at **67 steps**, unchanged from round 1.
Arrival step **37** has attached script, two loading frames, zero audio-handoff
errors, `loading_ready=false`, with the same deferred/equipment refusal. Final
step 67 is `player_death: blocked_death_reload`, 239 native clip steps, no respawn.
Six refusal kinds, zero captures. Rendered journey/full CTest still pending.
