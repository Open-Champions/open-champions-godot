STATUS: IN PROGRESS — implementation checkpoint; pending full CTest.

WIP preserved in d8d26769; verified wave 33 (66e52972) merged in e51783d0.
Only the assigned worktree changed. No push or PR.

Bounded addressed Mission 1 effects; continuous execution, physical objective
reachability, unsupported prop bodies, cutscenes and transitions remain refused.
Existing factory, shared actor authority, script binder/refusal exception and
loading-readiness checks are reused. No readiness bypass is introduced.

Research: audited PASS at research-strife-drundar 0d42d465276aeb768ad54316b34cb4dabe3a2c35.
Audited static calls do not establish the intervening bytecode bodies. The
adapter applies only addressed cells and stops restoration on the first refusal.
A new explicit diagnostic may inspect a different entry; ticks never resume it.

Release configure/build (-j 4): PASS. Focused mission1-scripts and script-commands:
2/2 PASS in 0.33 s. Full CTest (-j 3): PENDING; this note is committed before run.
Godot, fresh private Mission 1 checks, both journey variants and FULL_TRACE: pending.

## Implementation and evidence ledger

Evidence sources are the pinned research's docs/strife-drundar.md,
tools/ref_strife_drundar.py, tools/difftest_strife_drundar.py and the PASS fix
audit. Research worktree HEAD was verified at the full audited SHA above.
Numbers below are code offsets unless identified as a data slot or address.
Identifiers and argument strings are joined in memory from owner inputs;
no copied resource, switch, placement or quest-name inventories are committed.

- Recognition: 47 publics / 21 imports with start 0x638 (first branch),
  36 / 20 with start 0x150 (second branch): script_contract. Candidate
  mismatches refuse loading, never fall back to the unrestricted VM.
- Counter restore: getv 0x5e8 and STOR to data slot 0x14c at 0x5f8;
  dynamic persistence site 0x15b4 is checked but untraced counter callbacks
  are not executed. stage_effects is the addressed source.
- Stage predicates: getv sites 0x678, 0x760, 0x844, 0x928; first flag is
  negated, the remaining flags are direct. Branch skips are 0x744, 0x828,
  0x90c, 0x9f0. The four-by-four visibility sites (0x6b0..0x734,
  0x794..0x818, 0x878..0x8fc, 0x95c..0x9e0) join the same four targets;
  exactly one show and three hide per group, through existing host handlers.
  Each selected group runs in order, halting on the first refused effect.
- Strife objective links: entries 0x88/0xcc, 0x3dc/0x420, 0x138 join
  13 first-stage, two second-stage and two catapult callback records.
  Branch equality at 0x1668..0x167c uses local slot 0x14c and literal 2;
  0x1694 is a further predicate. Completion setv at 0x16f4 and callScript
  0x1720 -> entry 0x17f8 are checked, but the intervening predicate/body
  is not fabricated. The status exposes the count predicate only.
- Quest/bridge links: setv 0x2040, callScript 0x20a4 -> 0xc94, addQuest
  0x20d8, removeQuest 0x1f00 and trigger sites 0x1c90, 0x1df0, 0x1ed4
  are checked against stage_effects. These checks are not execution claims.
  Bridge requests refuse; no prop/cutscene behavior is invented.
- Eight second-branch switch pairs: SWITCH_OFFSETS in ref_strife_drundar.py
  supplies each key/action offset. switch_effects establishes key+0x2c
  getv followed by NOT at +0x3c, action+0x1c INC of slot 0, +0x48 setv
  to 1 and +0x74 callScript to the restoration effect. The adapter runs
  those addressed effects only. A saved key refuses another action, so it
  cannot increment twice. Effect bodies refuse; count and persistence
  already written are retained. Unknown bytecode between sites never runs.
- Restoration: eight getv/branch/call relationships at 0x47c..0x7dc;
  saved door flag 0x338, branch 0x348 -> 0x3b8, prop targets 0x374/0x3a8.
  No switch count is fabricated from persistent bits on revisit. Refusal
  at the first prop/callScript prevents following restoration work.
- Gate: 0x868..0x87c and 0x1c54..0x1c60 require exact equality with 8
  in data slot 0. setSaveInhibit 0x89c(1), addDeferedCall 0x8dc -> 0x934,
  callScript 0x9e4 -> 0xa3c; only the inhibit and deferred registration
  execute. Ready ticks never drain this untraced deferred body.
- Door diagnostic: getv 0xa68 and conditional branch 0xa7c -> 0xb10
  select the unsaved path; inhibit clears at 0xaa0(0), cutscene 0xacc
  refuses, so persistent completion at 0xb00(1) is never written.
  Save-inhibit memory address 0x50605c is the existing built-in state,
  not a new mission-specific global.
- Object census: COUNTS/FACTORY and script_contract establish 283/270
  records, 267/254 factory selections, 69/85 hostile placements, four
  scouts, three bridge targets, eight switch pairs, four door targets and
  one stairs/boss target. Constructor aggregation uses existing classify;
  no parallel constructor or actor lifetime implementation was added.
- Important source discrepancy: the mission research's FACTORY summary
  stubs propList_hasAnyLight and therefore lists only the unlit 0x23c44c
  path for all 24 second-branch generic props. The existing executed
  factory (research-script-session 6841b8a, level_factory.cpp fallback at
  selector address 0x5339a0, light keys 0x541ec8 onward) instead selects
  sixteen unlit props and eight light-bearing 0x2416c4 props on the owner
  input. The census now accepts that established classifier result. This
  is a documented use of integration's existing factory evidence; it is
  not a new constructor port or a claim that the narrower mission research
  executed the light-bearing branch.

## Refusals and scope

The original VM does not execute undocumented entry bodies in these two
levels, even when an entry offset is documented. Public runs route through
the bounded adapter; an injected unknown store between known cells cannot
execute. Unsupported callScript bodies, trigger/prop changes and cutscenes
stop with the existing RefusedResult/remember path, even for discarded return
values. Explicit diagnostic calls may start another bounded probe; ordinary
ticks do not resume a refusal. Continuous script execution, live hostile
placement/combat for these branches, physical switches, scout dialog, bridge
collision, door/stair passability, objective reachability and transitions
remain unverified/refused. Mission 1 completion and mission 2 availability are
not granted by census or diagnostic success. Actor identity/lifetime and the
wave-33 loading readiness refusals are retained unchanged.

## Gates in progress

- Native synthetic mission gate: 80 checks / 0 failures, including switch
  state effects, no double-count, persistence after refusal, fresh campaign
  restoration, exact predicate mutation refusal, ordered stop and unknown
  bytecode isolation. Existing script-commands gate also passes.
- Godot affected cases: script_commands, session_music, enemy identity,
  campaign moves, loading display/audio/present and scene_play_flow all
  exit 0 with no SCRIPT ERROR. Fresh mission1_real: headless 53 checks,
  rendered 57 checks, 0 failures, two destinations each.
- Rendered full-combat: 2,771 checks, 0 failures; FULL_TRACE 36,402 bytes,
  SHA-256 9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659,
  unchanged from wave 33. FULL_SAVE_LOAD presentation equality is checked
  when present counts agree (see final log).
- Rendered destination captures inspected: both show terrain and the
  bounded host/HUD; the actor is a capsule fallback. No original-client
  visual parity or mission combat acceptance is claimed.
- Corrections during validation: inherited synthetic public names exceeded
  the VM name limit; filler HALTs consumed following cells; synthetic
  executable light-format strings lacked percent-d placeholders. All were
  corrected without weakening the addressed contract. Rendered probe used
  obsolete advance_once and an unbounded post-draw wait; it now uses the
  integration advance_step/static-view force_draw capture route. Two owned
  failed probes were terminated by exact PID, never by process name.
  Initial full-combat invocation incorrectly requested captures headlessly;
  the corrected rendered run above passes. Preliminary failures are not
  counted as passes. Raw logs/captures remain ignored in work/mission1/.
