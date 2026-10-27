STATUS: IN PROGRESS — PSD-R1 refusal fixes; pending full CTest.

Pinned research remains 0d42d465276aeb768ad54316b34cb4dabe3a2c35.
PSD-R1-1: attachment checks Mission 1 preStart stop; refusal/fault/wait
prevents adoption, dialog, start and automatic restoration. Missing preStart
is not a refusal. Restoration remains an explicit native diagnostic only.
PSD-R1-2: door entry refuses the unknown predicate at 0xa78 before any
addressed dispatch or save-inhibit mutation. No polarity is inferred and
Contract::inspect does not claim to establish that unknown cell. Mutation
fixtures verify both changed and removed operations are refused by the
entry diagnostic (not rejected by the wider bounded static contract).

Regression: Godot production attachment checks no adoption/start/restoration
sites or counter changes after refused preStart and subsequent ticks.
Native mutation tests preserve save inhibit and all reached-site counts.

Pending full CTest (-j 3) checkpoint committed before invocation.
Release build and focused results, Godot headless/rendered, full journeys
and unchanged FULL_TRACE verification will be recorded after completion.
No push/PR; private inputs remain read-only and untracked.
