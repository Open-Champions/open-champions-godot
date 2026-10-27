STATUS: DONE — PSD-R3-1 fix reviewed; requested gates completed with expected journey refusal.

Reviewed fix 3a695b08 and regression expansion 6c6b9747 on starting HEAD
53ccb13f, branch codex/play-strife-drundar. No source changes were necessary.

PSD-R3-1: the addressed door diagnostic reads persistence at 0xa68. Zero
records an explicit unsupported-sequence refusal before dispatching any mutating
effect: no clear at 0xaa0, cutscene at 0xacc, or persistent write at 0xb00.
Incoming save inhibit and stored flag remain unchanged; there is no rollback or
fabricated completion. Nonzero skips all effects and completes.

Native regressions cover incoming inhibit 0 and 1 for saved inputs 0, 1, 2, -1,
and minimum signed 32-bit. Production signed-byte persistence clamps inputs;
the tests independently require preserved zero/nonzero class and unchanged stored
value, not an unclamped minimum-integer VM-input claim. They require persistent
read, correct refusal/completion, unchanged inhibit/flag, and absent effect sites.
Godot requires unchanged inhibit/flag and no clear/cutscene/write dispatch.

NOT validation at 0xa78 and JZER validation remain intact, including replacement
and authored-PROC-padding negative controls. PSD-R1-1 preStart refusal still
prevents automatic object adoption/restoration and further attachment execution;
its Godot checks remain intact. Research pin remains
research-strife-drundar@3216e7b0d9a779060c51648637646e6b49019732;
contract citations are docs/strife-drundar.md:214-235 and
tools/difftest_strife_drundar.py:399-402,415-461 relative to that pin.

Native evidence reused, not rerun: the orchestrator recorded full CTest on the
fix in 53ccb13f: 252/252 PASS, 657.59 s, no timeout,
`ctest --test-dir build/native --output-on-failure -j 3`.
Verified its retained log work/mission1/r3-fix-ctest-full.log. Existing Release
native/extension build logs end successfully; focused tests recorded 4/4 PASS,
0.51 s. No native code changed during this verification; full CTest was not rerun.

Fresh gates executed 2026-10-27, serially using the existing ignored gate driver:
- Mission 1 headless: exit 0, 47 checks, zero failures, 5.21 s.
- Mission 1 rendered: exit 0, 49 checks, zero failures, 5.56 s.
- Full-combat headless: exit 0, 2,641 checks, zero failures, 20.29 s.
- Full-combat rendered: exit 0, 2,771 checks, zero failures, 24.06 s.
- FULL_TRACE unchanged in both modes: 36,402 bytes, SHA-256
  9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659.
- tools/run_loop_playthrough.py: headless 67 steps; rendered 68 steps and
  43 captures; six refusal kinds each. Both wrote results and exited 1 at
  blocked_death_reload, with reload_refused true and respawn_completed false.
  Wrapper exit 1 is the expected preserved boundary, not full playable acceptance.
  Wrapper elapsed 451.57 s. Read-only input file count: 994 before and after.
- Final Godot logs: no SCRIPT ERROR or ERROR envelopes. Existing ObjectDB
  exit-leak warnings remain in all six Godot runs.

Evidence stays ignored under work/mission1/r3-fix/: gates.json, Mission 1 and
full-combat logs, journey run-summary.json, per-variant JSON/logs and captures.
Fresh driver output: work/mission1/r3-fix-gates-current.out.
Rendered Mission 1 destination capture inspected: terrain, HUD, capsule fallback.
No visual-parity, door-passability, actual cutscene-completion, or complete
playable-journey claim is made. Unsupported effects remain explicitly refused.

Coordination 2026-10-27: requested review and gates complete; report-only local
commit. Baselines, fixtures and journey tools unchanged. No private game data,
decoded identifiers or private absolute paths added to this report. Stray
untracked UID untouched; no push, PR, prohibited worktree changes or process-name
termination. Final git diff --check passed before commit.
