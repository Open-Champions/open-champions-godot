STATUS: MORE — NHR6-1 and NHR6-2 implemented; pending full CTest and external gates.

# Native Horsemen round 7 — 2026-10-26

Base: e294193c. Assigned branch only; integration lifetime pattern inspected read-only. No actor lifetime ownership changes are required for these property callback fixes.

NHR6-1: the mounted fixture initializes the documented MiniBoss key address and the death-path key addresses reached by its existing run bodies. Unknown/empty key memory on a non-null property list refuses explicitly; a null list returns zero independently of key data. Registered callback regression uses the documented MiniBoss key address rather than a host string.

NHR6-2: original property adapters interpret argument three as occurrence index and return zero for absent entries. The audited lists contain each key once; validated nonzero occurrences return zero. The host-only integer fallback helper remains separate. Callback tests cover occurrences zero and one for both keys, missing keys at zero/one/seven, and unavailable key refusal.

Evidence: audited research pin 338d459c47d5e8e49dc4d1f393111910b2026f05, ref_arena_monsters.py:105-109,473-479,588-589; docs/spec/arena-monsters.md section 2.5; docs/objects.md:105-108; ref_objects.py:199-233; fixed lists from docs/spec/horsemen-mount.md section 2.2. No original executable difftest rerun or new original-data claim.

Release configure/build (-j 4): PASS. Final focused CTest: 5/5 PASS, 4.93 seconds. Initial iterations exposed missing authored animation service in the new callback test and uninitialized death-path keys; corrected using the same pinned research. A null-list key decode regression was caught by combat-play and fixed before this milestone.

Pending full CTest: this note is committed before running ctest -j 3. Exact aggregate count/time and all timeout retries, including player-exact, will be appended. Godot, headless/rendered playthrough and FULL_TRACE remain pending.
