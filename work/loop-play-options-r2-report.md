STATUS: IN PROGRESS — pending full CTest.

NOPT-7: preserved the interrupted fix after reviewing native-options-r1-fix.md fully. State 14 now submits no modal or instruction draws; bindings and unchanged-choice refusal use the existing host diagnostic channel. Query/choice logic is unchanged. Native and Godot regressions assert empty draws and visible diagnostics.

Evidence: research-options-menu @603b2b7 docs/options-menu.md:34-40 establishes query/choice calls but leaves modal art unknown; research-menus-layout @dc458d5 tools/ref_frontend_screens.py:731-741 is a generic message-box fixture with no state-14 binding. Music-volume @e9777eb config change/commit contract remains untouched. No new geometry, game constants or flow conditions introduced.

Gates: pending Release build, full CTest (-j 3), frontend Godot smoke/capture, and FULL_TRACE comparison. Build concurrency limited to -j 4. NOPT-1 through NOPT-6 preserved.
