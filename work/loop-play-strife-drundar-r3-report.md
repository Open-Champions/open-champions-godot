STATUS: IMPLEMENTED — pending focused verification and full CTest.

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
