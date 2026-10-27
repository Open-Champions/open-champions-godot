STATUS: IN PROGRESS — pending full CTest.

NOPT-7: preserved the interrupted fix after reviewing native-options-r1-fix.md fully. State 14 now submits no modal or instruction draws; bindings and unchanged-choice refusal use the existing host diagnostic channel. Query/choice logic is unchanged. Native and Godot regressions assert empty draws and visible diagnostics.

Evidence: research-options-menu @603b2b7 docs/options-menu.md:34-40 establishes query/choice calls but leaves modal art unknown; research-menus-layout @dc458d5 tools/ref_frontend_screens.py:731-741 is a generic message-box fixture with no state-14 binding. Music-volume @e9777eb config change/commit contract remains untouched. No new geometry, game constants or flow conditions introduced.

## Gate results

- Release configure (`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`) and build (`cmake --build build/native -j 4`): passed.
- Godot extension `champions-archive-extension` rebuilt in Release at -j 4: passed.
- Full CTest (`ctest --test-dir build/native --output-on-failure -j 3`): pending; running against committed fix ff00480c.
- Frontend Godot smoke: exit 0, 144 checks, zero failures, 21 screens. Includes changed-choice bindings and unchanged-choice refusal assertions with no state-14 draws.
- Rendered frontend capture: exit 0, 25 screenshots. Inspected state-14 capture at 1280x960: host diagnostic bindings visible, no modal or instruction artwork. Private input read only; screenshots and logs remain ignored.
- Synthetic full-combat regression: exit 0, 2,620 checks, zero failures. FULL_TRACE 31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, unchanged from the preceding report.
- Godot runs emit an ObjectDB-leak warning on exit; no script errors or failed assertions in these runs. These are bounded fixture results, not retail pixel parity or a full playthrough.

NOPT-1 through NOPT-6 code paths remain untouched. Unchanged state-14 accept/cancel still refuse without write/exit; controls, communications, non-audio application and state-specific artwork remain unverified/refused.
