# Cache-bearing animation lifecycle

This standalone RTA runtime connects original animation lifecycle and timing to the existing raw32 decoder through one owned registry. State membership holds fixed track-cell IDs. Moving track contents does not move their identity; a decoder slot can therefore keep an observable stale owner in an inactive cell.

The behavior contract is executable Python `ref_anim.py` at `f516ac798bf28375554fe53ffb31b74802844136` (SHA256 `6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8`) and `ref_core.py` at `41e847af5360724ab62d208594f8027ea3c1b686` (SHA256 `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`). Animation prose is separately pinned at `f2b996dded898f1b0b558b901cab875f775cfdc7`, SHA256 `be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab`. The executable formula controls behavior where prose differs.

## Ownership and domain

One Runtime owns one decoder Context. Clips, track cells, slots, frame and cosmetic seed live only there. Each state owns a fixed ordered list of canonical track IDs. State copies allocate new cells and share clip identities, including their four mutable event entries. Equal clip bytes parsed twice retain distinct identities. State destruction models cache release; state and cell IDs remain observable until Runtime destruction. Temporary tracks created by supplied-context `decode_clip` remain owned after either success or failure.

Clearing changes only count. Reusing a cell releases its stale cache. Zero-blend stopping releases every active cache before shifting fields; compaction transfers owners and clears moved-from links; removal repairs moved owners and clears only the vacated top slot. Inactive fields and all 64 joint records per slot remain observable. Stale nonreciprocal owner links are permitted by the contract and are never repaired during validation or snapshotting.

Execution admits non-ROOT/non-YAW tracks and null pose tokens. Pose tokens can be supplied and observed, but execution refuses them. Sound, root and yaw output requests refuse. This gate does not integrate Session, scene playback, poses, sound, animation-generated root motion, original player controls or automatic AI. The existing cache-free clock and standalone decoder remain separate APIs with unchanged fixtures.

## Native limits and failure prefixes

State capacity is 1 through 127; supplied count is 0 through capacity. Limits expose decoder limits, at most 1,024 states, 65,536 state cells and an aggregate subtraction budget per advance call (default 65,536; hard maximum 1,048,576). Configured state allocation limits are checked before allocating capacity cells. A true host allocation failure may retain an unassociated decoder-cell prefix, with no incomplete StateID published.

Every LOOP subtraction stores track time in original order. Budget exhaustion keeps every executed store and prevents later stores, including final state flags; it does not replace the loop with remainder arithmetic or roll back. Decoder key-work limits retain their existing charge boundary and failure prefixes. Caller-selected integer-backed FPU behavior remains authoritative; cosmetic randomization uses the single Context seed and no operation implicitly increments frame.

## Local validation

All 237 original-Python cases in 23 groups match every computed optimized and ASan/UBSan observation byte. Fourteen arithmetic variants run under all 16 settings; thirteen structural cases run once. The input-driven corpus exercises all 38 DSL opcodes, including the 22 original lifecycle methods, with nine finite original HostFault results and their retained prefixes. Trace SHA256 is `3199fc2cc97fe8e16c98893b79a85c41824493c70f8a2ac3c7c17d92096e28d0`; FNV is `3534a91e3532094d`.

All five linked runtime units were instrumented in the sanitizer build. Leak detection was disabled on macOS, so no leak result is claimed. Thirty-four independent API checks pass in both builds, including active null-clip clear/release/copy/destructor behavior and a LOOP budget failure after its first published subtraction. Operations that read a missing clip have an explicit native HostFault guard; that safety policy is separate from finite original-Python outcomes.

Three fresh final-source generations reproduce both canonical files byte for byte; seven atomic publisher tests pass. The strict 38-suite registry binds immutable model/specification references, generator hash, helper map and every protocol field. Sixty-two whole-state omission probes change every platform's synthetic output and refresh its hash while leaving the reference fixture unchanged; all are rejected. These synthetic integrity fixtures are not actual platform artifacts. Independent runtime, oracle and integration reviews found no admitted-domain blocker.

The 103-test local CTest run passed in 620.58 seconds; the 21-test transport suite passed with one unavailable macOS facility skip and a real RLIMIT_FSIZE failure check. Its initial core/Python checks preceded the final boundary census extension. CI then exposed two stale assertions expecting 56 paths instead of 58. The correction updates both assertions and adds explicit runtime census and forbidden-arithmetic/dependency regressions. Fresh final-source full Python regression passes 647 tests with 364 explicit skips, and both affected CTest gates (simulation boundary and core trace integrity) pass in 8.35 seconds. Numerical runtime, oracle and replay sources are unchanged, so their completed parity, sanitizer and transport evidence is retained. GCC15 strict syntax, LLVM20 formatting and a fresh 58-path simulation boundary check pass. Fresh Universal2 debug/release builds and the actual selected source extension pass 70 scene and 283 browser checks. The local 38 actual trace/report pairs contain 1,428,801 records and independently match original expectations. Those original local results preceded publication; the follow-up below records independently inspected remote artifacts and fresh local checks.


## PR #73 review follow-up (2026-10-13)

The two failed comparison jobs in [run 38240574302](https://github.com/Open-Champions/open-champions-godot/actions/runs/38240574302) never downloaded or compared traces. Their first prerequisite step received `NATIVE_RESULT: cancelled`. The macOS Intel check annotation states “The run was canceled by @TrustlessMike.” Its CTest log stops during `session-turn-transport`, after 77 passing cases; artifact uploads were skipped. The workflow has a 90-minute limit and the job was cancelled after about 60 minutes. This is incomplete-run evidence, not a numerical mismatch or a timeout; the fail-closed prerequisite is retained.

[Push run 38240568635](https://github.com/Open-Champions/open-champions-godot/actions/runs/38240568635) passed every native job and both comparisons at the identical source commit `758c2d63be19c99d01d866abc4063080ef943d88`. Fresh downloads of its actual artifacts pass the current comparators: all 38 suites on all five platforms match committed original-model expectations, including runtime SHA256 `3199fc2cc97fe8e16c98893b79a85c41824493c70f8a2ac3c7c17d92096e28d0`. The pose comparator passes 36,248 components under its documented tolerance; that result remains separate from exact runtime proof.

Review against the hash-verified pinned Python lifecycle found no demonstrated defect in cache release, compaction, copying, event order, stale fields or failure prefixes. Production runtime, workflow and vector bytes are unchanged. Eight additional handwritten synthetic API checks cover the aggregate budget across two tracks, descending execution and retained weight/cache/flag state on refusal, a fresh budget on the next call, the cosmetic seed prefix before a null-clip length failure, shared mutable events after copying, inclusive/exclusive event bounds on a paused fixed-step track, and speed-based events with fixed-step timing. The null-clip failure check exercises explicit native safety policy, not an original finite Python result.

Fresh local Apple Silicon configure/build and full CTest pass all 103 cases (300.82 seconds), including 42 runtime API checks, 237 exact runtime cases and transport tests. `python3 tools/check_simulation_boundary.py .` passes. Generator-publication tests pass 7 cases; runtime provenance/omission tests pass 4 cases. The runtime API, exact and transport cases also pass under fully instrumented ASan/UBSan (3/3, 191.25 seconds). Leak detection was disabled on macOS; no leak result is claimed. See commands below; actual downloaded artifacts are kept under ignored `work/`.

```sh
cmake -S native -B build/native
cmake --build build/native -j 8
ctest --test-dir build/native --output-on-failure -j 6
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p 'test_animation_runtime_generation.py'
python3 -m unittest discover -s tools -p 'test_core_animation_runtime_contract.py'
python3 tools/compare_core_traces.py --trace-dir work/finish-review-core --source-commit 758c2d63be19c99d01d866abc4063080ef943d88
python3 tools/compare_animation_traces.py --trace-dir work/finish-review-pose --source-commit 758c2d63be19c99d01d866abc4063080ef943d88
cmake -S native -B build/review-sanitize -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/review-sanitize --target champions-animation-runtime-api-tests champions-animation-runtime-trace -j 8
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/review-sanitize --output-on-failure -R '^animation-runtime-'
```

Setup refusals were corrected before these passing checks: the boundary CLI needs its root argument; the decoder requires at least two slots in a fixture; pose and core comparators reject a mixed artifact directory. None required a production change.

The local follow-up adds tests and documentation only. These additions have not run on remote platforms. No Session, scene, ROOT/YAW, pose, sound, player or AI integration is claimed.
