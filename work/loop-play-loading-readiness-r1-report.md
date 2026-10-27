STATUS: MORE — reviewed implementation; full gates in progress.

## Scope and pinned evidence

Assigned worktree/branch only: `play-loading-readiness` / `codex/play-loading-readiness`.
Initial commit `c8339a1a0f8d4f53e94b26c6de334d492a6f2f89`; verified base
`66e529727407dc7c0016cd286c36ac4b36649ba9` is an ancestor.
Research pin `research-loading-readiness@9dd4904603ae0d8a46e57454cf5c84b1f25c8fd9`;
`audits/research-loading-readiness.md` verdict PASS. References below are to
`docs/loading-readiness.md` at that exact pin. The pin was verified live.
No original-executable probes rerun here; those remain the research's evidence.

## Per-condition review and implementation

- Deferred: lines 11-37 document a single returned pass and queue promotion,
  not an empty-queue barrier. `Host::run_start` records completion only after
  start/post-action/deferred execution returns without fault/refusal. Script load
  resets the observation; current fault/refusal invalidates it. This boolean is
  an authored observation, not an invented original flag/address. Runtime AMX
  tests exercise self-deferral, actual queue promotion, absence of an extra pass,
  script replacement, unsupported-command refusal and absent-machine fault.
- Peer: lines 91-95 authorize only explicit offline mode with live byte
  `0x402791 == 0`. `start_single_player` grants that mode; `receive_launch`
  revokes it. Default sparse zero does not grant offline status. Native tests
  exercise absent grant, explicit grant, nonzero live mode, and launch revocation
  even while memory still contains zero. Online readiness remains unavailable;
  no peer-count/stall-counter success fallback is implemented (lines 97-113).
- Equipment PARTIAL: lines 41-53 supply count `0x4d8ae0`, player base
  `0x4dcec8`, player record index `+0x3c8`, record base `0x4d8af0`,
  equipment `+0x43f0`, 17 slots, definition category `+0x1c`, categories
  5-8 pending, and the category 0/2/3 environment skip using `0x41b69d`,
  `0x44fe08 & 0x10000000`. The adapter accepts only its supported explicitly
  offline one-player/record-zero domain, with every required byte explicitly
  published by real runtime memory writes. Sparse defaults/executable bytes
  cannot certify publication. Null players and all-empty slots can be observed;
  slot-3 selection, generic index/model lookup, specialized paths and I/O remain
  refused (lines 48-79). Pointer presence alone never establishes model readiness.
  These are conservative client restrictions, not claims about original limits.
  Tests mutate production Host memory and distinguish missing slot publication,
  all 17 empty slots, null player, all four direct-pending categories,
  environment skip/removal, and specialized/generic refusals.
- Warm-up: lines 81-87 establish the 1501-frame threshold, correcting the former
  1500 comparison in both helper and transition. Existing assertions now verify
  1500 still waits and 1501 reaches the documented escape. Terrain/equipment
  pending at escape remains an explicit client refusal, never resource success.
- Store PARTIAL: lines 117-138 supply four identifier addresses
  `0x554678`, `0x554680`, `0x554688`, `0x554698`. Values are read from the
  owner-selected executable at runtime. Only four exact nonmatches certify
  no applicable stock job. Any match refuses unknown stock generation/RNG;
  missing destination/executable, empty strings and read exceptions refuse.
  Native tests cover four match refusals, four nonmatches and read failure.
  There is no unconditional four-store job and no no-op generation success.
- Save PARTIAL: lines 140-152 establish a separate network-only pre-load gate.
  The same explicit offline/live-zero condition bypasses only that gate.
  The adapter does not claim a card save or persistence. Nonoffline state refuses
  unbound card services. The native test retains offline bypass with nonzero busy
  byte `0x4f8f2c`, distinguishing bypass from an invented save-complete condition.
- Bridge/scene: lines 154-163 require observable applicable prerequisites.
  Native optionals become unavailable fields, never guessed booleans. `ok`
  requires the deferred observation, known equipment pending state, peer/store/save
  satisfaction. Equipment pending itself is handled by the warm-up loop.
  Follow-up `5d61344c` also makes failed scene script attachment force `ok=false`;
  tests distinguish a ready controller from an actually attached scene.

## Remaining explicit refusals

Unpublished player/equipment state; slot-3 selection and model identity/publication;
actual specialized/I/O completion; matching transition stock generation and RNG;
onoffline peer/card services; save persistence; unsupported destination scripts.
Native synthetic state and successful fixtures do not establish live campaign
acceptance. No success fallback, test/fixture/baseline relaxation or private
resource identifier was added.

## Verification checkpoints

`c8339a1a` already committed a pending-full-CTest note. `5d61344c` committed the
review fixes and renewed pending-full-CTest note before the new aggregate run.
Release configure/build and main extension build PASS (`-j 4`). Focused CTest
script commands 3/3 PASS, 0.42 s. Auxiliary texture/animation Release builds PASS
(`-j 4`). Initial Godot launches failed because the texture library was absent;
local builds resolved setup and loading present then passed 19/19 headless.
Full CTest running with `-j 3`; exact final count/time and isolated reruns pending.
Godot matrix completed; journeys/full CTest final results pending.

### Godot, trace and private-art results

The 54 inherited public matrix cases plus wave-33's seven loading/identity/moves/
death cases, and loading-present headless/rendered, total 63 public cases.
**62/63 pass the strict diagnostic gate**. The sole remaining diagnostic is the
inherited `ui_layout_smoke` required-argument check (`:84`), which deliberately
calls `hud_text_glyphs` with four arguments when five are required. It emits one
engine ERROR while exiting 0. Do not represent this as a clean all-pass matrix;
no test, binding, allowlist or baseline was changed to suppress the diagnostic.

Two setup failures remain separately recorded: the initial private disc case
lacked its required folder argument (exit 2), and the isolated native-FX visual
case lacked the relative capture directory (exit 0 plus PNG-save ERROR). The
private case passed when given the selected read-only input. Native FX passed
with clean diagnostics when rerun in the assigned project, where its documented
relative `work/` output directory exists. The aggregate's original 58/61 result
is retained; the final public 62/63 includes these corrected setup results and
three additional rendered cases, excludes the private disc case, and includes
no suppressed errors. Summary: `work/loading-r1-godot-summary.json`.

Loading display passes its corrected 1500/1501 assertions; audio 10/10; present
19/19 headless and 21/21 rendered; identity 22/22 each; death 11/11 headless and
13/13 rendered; campaign moves 33/33. Private loading-art check: 111 levels,
111 selected, 12 documented fallback selections, zero refusals. No private
resource names or decoded content are reported here.

Full combat: 2,641 checks / zero failures. `FULL_SAVE_LOAD`: 300/300;
continued/uninterrupted presentation hashes match.
`FULL_TRACE`: **36,402 bytes**, SHA-256
**`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**,
exactly unchanged from wave 33. Evidence:
`work/loading-r1-matrix/playable_wave_smoke_full_combat.log:17-21`.

Headless journey completed 67 steps, matching wave 33, result written. Arrival
at step 37 refuses with the exact native diagnostic: deferred loading pass absent,
faulted or refused commands; equipment runtime player/slot/model publication or
specialized I/O unbound. `script_attached=true`, `loading_ready=false`, two loading
frames, zero audio-handoff errors; campaign/image carry is observed but complete
record carry is false. Peer/store/save no longer appear as unavailable barriers.
The driver continues explicit diagnostic branches and ends at step 67,
`player_death: blocked_death_reload` (239 native clip steps, no respawn).
Rendered journey completed **68 steps / exit 1**, 43 captures, six refusal
kinds; headless **67 steps / exit 1**, zero captures, six refusal kinds. These
exactly match wave 33's 67/68 totals. Both result JSON files were written; neither
run timed out. The unchanged wrapper exits 1 because both variants exit 1.
Rendered arrival is step **38** (headless **37**), both after two loading frames
with the same deferred/equipment diagnostic and no audio-handoff errors. Both
end at `player_death: blocked_death_reload`, not a completed campaign or respawn.
Logs have zero SCRIPT ERROR/engine error diagnostics and one ObjectDB exit-leak
warning per variant. Disc file counts are **994 before / 994 after** (a counts-only
check, not a claim of a full input-content hash comparison). Full aggregate CTest
still running; no native result is inferred from Godot or journey completion.

Visual inspection of actual rendered journey captures: the loading-labelled
capture shows the prior live view, while the destination capture shows the session
stopped and disabled resume. These do not prove a successful playable handoff or
loading-art pixel parity. Rendered present tests separately observe an actual draw
and preserve bounded refusal with drawing disabled.

Ignored raw evidence: `work/loading-r1-*.log`, `work/loading-r1-matrix/`,
`work/loading-r1-playthrough/`. Tooling is staged under ignored
`work/loading-r1-tools/`; matrix diagnostic/isolation helpers are read from the
existing matrix-errors tooling, with execution rooted in this assigned worktree.

2026-10-27: review and requested gates resumed locally; no push/PR, no process-name
termination, no edits to research, integration or other worktrees; private input
is read-only. No durable memory update requested or performed.

Full CTest (recorded by the orchestrator, 2026-10-27 ~06:15, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` — **251/251 passed, 803.08 s**, no timeouts (log `work/loading-r1-ctest.log`). With the Godot matrix, unchanged FULL_TRACE and the 67-headless / 68-rendered journeys already recorded above, all requested gates for this round are complete.
