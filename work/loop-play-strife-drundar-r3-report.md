STATUS: DONE — researched predicate implemented; requested gates completed.

Research pin: research-strife-drundar@3216e7b0d9a779060c51648637646e6b49019732.
All citations below are relative to that research checkout at that revision.

Contract now requires NOT 0x54 at 0xa78, alongside getv 0xa68 and
JZER [0x35,0xb10] at 0xa7c/0xa80 (tools/difftest_strife_drundar.py:399-402,
415-419). NOT handler 0x1403e8 performs logical negation
(docs/strife-drundar.md:214-220). Zero falls through to 0xa84 and calls
clear-save-inhibit(0) at 0xaa0, cutScene at 0xacc, then setv(1) at 0xb00;
nonzero branches to 0xb10 (docs/strife-drundar.md:222-235;
tools/difftest_strife_drundar.py:453-461).

The real addressed diagnostic now reads the saved value and applies that
predicate. The existing documented save-inhibit binder runs for zero.
The existing cutscene effect-body refusal stops before the persistent write;
no effect completion or door passability is invented. Nonzero skips effects.
PSD-R1-1 attachment refusal/automatic-restoration protection is untouched.

Synthetic fixture encodes the pinned cells; it is not predicate evidence.
Independent expected branches come from original VM observations for saved
0, 1, 2, -1, minimum signed 32-bit
(tools/difftest_strife_drundar.py:421-461). Production diagnostic tests assert
read/effect sites, save inhibit, persistence and explicit cutscene refusal.
Changed opcode and removed NOT (replaced with authored PROC padding) must
fail inspection/load and cannot dispatch the diagnostic. These are replacement
controls, not a byte-deletion/shift experiment. Replacement values are authored
negative controls, not claims about game content.

Pending full CTest: this checkpoint is committed before invoking the full
ctest --test-dir build/native --output-on-failure -j 3 gate.
Release, focused, full suite, Godot, journey and trace results follow.

Focused first run exposed an assertion comparing minimum signed 32-bit input
with the persisted signed byte. Production quest_set clamps storage; the test
now independently checks that storage preserves the researched zero/nonzero
class and remains unchanged across the diagnostic. No persistence behavior or
predicate expectation changed; all five input cases retained.

Release native configure/build PASS (-j 4), extension Release build PASS (-j 4).
Focused mission1/script-command selection: 4/4 PASS, 0.32 seconds; Mission 1
fixture 127 checks. Initial full CTest invocation inadvertently overlapped the
rebuild and executed the stale 122-check binary (one already-fixed assertion
failure). It was interrupted by exact PID after verifying its cwd, preserved
as work/mission1/r3/ctest-stale-interrupted.log, then restarted after the
completed build/focused pass. No other worktree's process was interrupted.

Godot / trace / journey gates (work/mission1/r3/):
- Mission 1 headless PASS: 46 checks, zero failures, exit 0, 5.21 s.
- Mission 1 rendered PASS: 48 checks, zero failures, exit 0, 5.76 s.
  Production refused-preStart checks remain intact. Rendered destination
  capture inspected: terrain, HUD, capsule fallback. No visual parity claim;
  this probe does not visually establish door passability or cutscene completion.
- Full-combat headless PASS: 2,641 checks, zero failures, exit 0, 20.60 s.
- Full-combat rendered PASS: 2,771 checks, zero failures, exit 0, 22.69 s.
- FULL_TRACE unchanged in both: 36,402 bytes, SHA-256
  9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659.
- tools/run_loop_playthrough.py ran both variants: headless 67 steps,
  rendered 68 steps / 43 captures, six refusal kinds each; both exit 1 at
  blocked_death_reload, preserving the requested existing boundary. Wrapper
  elapsed 384.90 s. Input count 994 before and after. Not full playable acceptance.
- No SCRIPT ERROR or ERROR envelopes in final Godot logs. Existing ObjectDB
  exit leak warnings persist. Baseline and journey tools are unchanged.
- Full CTest remains running. No timeout observed at this checkpoint.


Final full CTest: 252/252 PASS, exact real elapsed 685.31 seconds,
ctest --test-dir build/native --output-on-failure -j 3. No timeout occurred;
no isolated timeout rerun required. Final log: work/mission1/r3/ctest-full.log.
The pending-full-CTest note was committed in 9c7cf913 before invocation.
Corrected fixture checkpoint: 6def088e. The interrupted stale-binary invocation
is disclosed above and is not counted as the final suite result.

Final review: git diff --check b1255ff2 PASS. Only five source/test files and
this forced-added report changed. PSD-R1-1 production attachment code remains
unchanged. No game data, decoded private names, private paths, baseline changes
or journey changes committed. The stray untracked UID is untouched. Work stayed
on codex/play-strife-drundar; no push, PR, prohibited worktree edit or process-name
termination. Cutscene completion, actual door persistence and passability remain
explicitly unestablished; zero stops at the cutscene refusal.

Coordination 2026-10-27: implementation and all requested bounded gates complete.
Final report commit is HEAD returned to the parent.
