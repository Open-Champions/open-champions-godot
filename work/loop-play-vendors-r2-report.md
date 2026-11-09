STATUS: PARTIAL — observed session vendor stock implemented and all gates PASS (2026-11-08/09). Native CTest 260/260, strict Godot 54/54, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged, journeys 80/81 with pickup 10→11, identical to wave 37. Live retail stock stays refused for the listed research gates.

# Play vendors round 2 — session vendor stock

Branch `codex/play-vendors-r2` from wave 37 `70d2de19`. Source: PASS-audited
`research-vendor-stock` at `fdf6752` (audits `research-vendor-stock-r2.md`,
`research-vendor-stock-r2-fix.md`). Citations below are
`docs/spec/vendor-stock.md` (`spec`) and `tools/ref_vendor_stock.py` (`ref`)
at that commit. Builds on the first vendor round (`codex/play-vendors`
`061eadae..9c6e23d8`, already in wave 37). Only synthetic fixtures; no retail
names, bytes or values are stored.

## Commits

- `eb28505e`, `1057226a` (earlier agent, cut off by a provider limit): the
  `VendorStock` model, budgets, prices, storeLevel literals, refusals, Host
  and session-flow hooks, and tests.
- `38af1b9d` (this round): makes the generation loop match `ref:107-145`, and
  adds the bound quest variables and the non-literal taint.
- Gate records `8c65c0c3`, `aa41d1a9`, `5d5e6cd8`, then this report.

## What is implemented (Observed only)

| Behaviour | Code | Research |
| --- | --- | --- |
| The four destinations at 0x554678/80/88/98 dispatch modes 1, 1, 2, 3. Every matching load regenerates, a non-match does nothing, mode 0 keeps the lists. No freshness gate. | `vendor_stock.hpp` `kShopDestination*`; `VendorStock::begin_load`/`on_load`, `begin`/`open_or_stock`; `Host::stock_on_load` (the ordered stock point, for `SessionHooks::shop_stock`); `Host::loading_readiness` only reads the recorded result | spec:17-30, 326-329 |
| Budget from storeLevel (first read for scale, second for budget), difficulty, raw PS2 float steps, the `(progress+1)` attempt budget, caps and the 44/40/36 pointer-cell offsets. Out-of-contract inputs are refused. | `vendor_budget` | spec:36-43, 67-102; ref:15-44 |
| Shared seed for parser/generators, three cosmetic-seed draws (one after each ordinary loop), special budgets `2*(low+n*step)` with n = 22, 17, 10. | `step_ordinary`, `step_special` | spec:156-185; ref:47-58, 135-145 |
| Class-4 armour fraction through the party passed to the existing generators. | `set_party` → `Game::classes` | spec:40-43 |
| Seven fixed records from `parse_descriptor` (4 / 0 / 2+1). The count increments before parsing and the return is ignored. The parser's own bytes replace the destination; no old bytes are kept. | `step_fixed` | spec:90-97, 280-285 |
| Ordinary records from `generate_with_affixes`, specials from `generate_preset`. The generator writes slot `count` before the outer tests, so a rejected candidate stays there. Six duplicate rejections, then the seventh passes. List 2 advances every attempt; 0xff suffix byte rejects in list 2. A weapon/armour generator failure or a weapon bit-40 rejection refuses the pass (see Refused). A special is appended when the slot pointer is nonzero, whatever the generator returned. | `step_ordinary`, `step_special` | spec:106-131, 173-179; ref:107-145 |
| Duplicate scans: list 0 scans itself; list 1 scans the **previous third list**, which is kept until armour has finished; list 2 scans the name region at 0x86b698, where deferred names never match. The `same_raw` identity includes +0x22 and not quantity. | `scan`, `same_raw` (three-valued: a decision that depends on unknown bytes refuses) | spec:124-146, 287-301; ref:61-64, 116-120 |
| Stable ascending signed-value sort that swaps whole 0x48-byte records. | `step_sort` (the only sort; `sort_stock_by_value` was deleted) | spec:148-152; ref:149-158 |
| No restock by gold and no 121-tick daemon (0x1c8a00 is never called). | none by design | spec:54-63, 256-276, 334-336 |
| storeLevel: the creating read stores 0. Literal `setv` values are `hub`/`hub2` 0, 8, 7, 6, 5, 7, 6, `stobonus` 7 and `islgnome` 3. The guarded act change zeroes it (card latch and death task skip). It always lives in the session's quest-variable table; there is no private copy. | `read_store_level`, `apply_literal_setv`, `on_act_reset`, `bind_quest_variables`/`bind_quest_values`; the Host binds its 0x86da80 table; `vendor_stock_tests.cpp` runs the real `level_flow::begin_change` | spec:45-52, 303-322, 330 |
| Prices: buy factor 0x3f199999 (100→59), category 1 0x3ee66666 (100→44), and full price for flag bits 0x30000000000 or slot 8. Sell 100/1→4, ×20 stack cases, minimum 1, forced 0 for slot 8 / 0x400000000000. Gold is debited before `giveItem` with its result ignored, and credited before `dropItem`. Cap 99,999,999. | `vendor_buy_price`, `vendor_sell_credit`, `vendor_buy_gold`, `vendor_sell_gold`; the existing `Machine::shop_buy`/`shop_sell` (`messages.cpp:974-1001`) with `messages_tests.cpp` 44/full-price/4 cases | spec:228-254, 337-338; ref:161-220 |

## Corrections made in `38af1b9d`

The earlier commit refused the whole pass on the first weapon/armour
generator failure and on any bit-40 rejection. It also never stored rejected
candidates, and it repacked records during the sort. The reference retries
(`ref:128-134`), writes every returned candidate (`ref:110-113`) and swaps
raw records (spec:148-150). The two tests that encoded the old refusals now
use a small port watchdog and still require a refusal with no published
stock. That is the same unbounded case, now reached the way the original
reaches it, so no test was weakened. New tests cover:

- a bit-40 subset being rejected and retried, with only fixed records keeping the bit;
- a category-2 name-region duplicate staying in its slot and being adopted by a failed special (cosmetic seed 12 rolls only that special);
- storeLevel creation, literal writes and act clears in a bound quest map;
- the non-literal taint.

## Refused, with reasons

- **Non-literal storeLevel `setv` in `hub`/`hub2`**: its expression is not a pushed constant and is not resolved (spec:315-319). `apply_non_literal_setv` throws and marks storeLevel unestablished. The next pass refuses and keeps the lists until a literal `setv` or an act clear (`vendor_stock.cpp:345`).
- **Which literal is current on a visit**: unknown (spec:225-226, 318-319). `literal_current_on_visit` throws. The live play client therefore does not bind a `VendorStock`, and the menu's live buy refusal from round 1 is unchanged.
- **Unbounded generation failure**: the original has no overall bound and its termination is Unknown (spec:121-122, 217-219). The research does not bound the retry after a weapon/armour generator failure, or after a weapon bit-40 rejection. The pass therefore refuses at the first such attempt, publishes nothing and keeps the previous lists and seeds (`step_ordinary`). Duplicate rejections are bounded (six, then the seventh passes; spec:119-121) and keep retrying. No numeric limit is used. The research probe's instruction limit belongs to its harness and says nothing about game behaviour; the round-2 report was wrong to cite it.
- **Undefined record bytes**: a generated record's +0x06, +0x22, +0x44 and empty-modifier values are never written; in a generator they hold an earlier candidate's float temporaries (items.md:83-95, I1 at items.md:724). The port does not invent them. A duplicate decision (`same_raw` includes +0x22) whose answer depends on them, a scan of a stock slot that was never written, or a special adoption of such a slot refuses the pass. Only the seven fixed parser records are known to be zero there (spec:280-285). An executed probe can supply the bytes through `set_executed_residue`, as `stock_plan` takes executed raw records (spec:197-202); production never does. So without residue a pass refuses whenever a candidate matches an earlier record in definition, flags and affixes, and the first pass of a session refuses at the armour scan of the never-written third list.
- **Script stores of storeLevel**: the VM's `setv`/`addv`/`vars_set` call does not show whether its value was a pushed constant, so it cannot be classified as one of the authored literals (spec:313-319). Such a store marks storeLevel unestablished; the next pass refuses until the act clear.
- **Missing session inputs**: seeds, difficulty, party and the quest table start unset in `VendorStock`. The Host refuses the pass until it has a bound shared seed that is the original's state, a bound cosmetic seed, published 0x4f719c, published 0x4d8ae0 (1..4) and each player's published class word. Nothing is defaulted.
- **Resource completion**: 0x538678 is requested and not completed (spec:31-33, 220-222). Every generation reports `resource_requested` without completion, and `loading_readiness` adds that refusal.
- **Rendered shop UI** (spec:223-224) and **save I/O** (spec:220-222): `render_shop` / `save_stock` throw.
- A counted record that is residue with no definition (only reachable from a synthetic fill) refuses at the sort, because its value is unknown (spec:200-202).

## Gates (mini, `~/malik-champions/open-champions/worktrees/play-vendors-r2`)

Evidence is in ignored `work/vendors-r2/` (mini home path rewritten to `~/`).
Godot 4.5.2.stable from `~/malik-champions/runtime`. All runs were serial and
had standard input closed.

1. Release native build, `--clean-first -j 4`: **PASS**, exit 0, 113 s.
   GDExtensions `mac-template_debug`, `mac-template_release`,
   `level-texture-template_debug`, `level-texture-template_release` and
   `creature-template_debug`: **PASS**, every library `x86_64 arm64`.
   Evidence `build.log`, `*-build.log`, `build-status.txt`.
2. Focused CTest (`vendor-stock-exact`, `items-exact`, `messages-exact`,
   `messages-api`, `menus-exact`, `level-flow-exact`, `script-commands`,
   `script-items`), `-j 3`: **PASS 8/8**. Evidence `ctest-focused.out`.
3. Full CTest `-j 3`: **PASS 260/260**, 845.38 s, exit 0. Wave 37 was 259;
   the added test is `vendor-stock-exact`. Evidence `ctest.out`.
4. Headless editor import (`--import`, exit 0), then the strict Godot dev
   matrix, unchanged `tools/godot_matrix.py matrix --captures --timeout 1200`:
   **PASS 54/54**, exit 0. Evidence `matrix/results.json`, `matrix-run.log`.
5. FULL_SAVE_LOAD: **PASS 300/300**. Continued and uninterrupted presentation
   SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`,
   identical. Full synthetic combat 2,645 checks, 0 failures.
6. FULL_TRACE: **PASS, unchanged**, 36,402 bytes, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
   Evidence `matrix/playable_wave_smoke_full_combat.log`.
7. Journeys, unchanged `tools/run_loop_playthrough.py` against the read-only
   disc: headless **80** steps and rendered **81** (38 captures), pickup
   **10→11**, 52 walk ticks, 0 log errors, 6 refusal kinds, disc files
   994/994. Step/result pairs and refusal maps are identical to wave 37 at
   every index (27 / 28 `refused` rows). Wrapper exit 1 is the retained
   death-reload refusal, as in wave 37. Evidence `playthrough.out`,
   `playthrough/*/result.json`.

The mini `build/` was deleted afterwards.

## Incident

My first sync used the brief's rsync line without excluding `.godot`. Its
`--delete` removed the mini copy's untracked `godot/.godot`, `godot/bin` and
`godot/creature_anim/bin`, which the MacBook worktree does not contain. Only
this branch's synced copy was affected. Gate 1 rebuilt the extension
libraries and gate 4 re-imported the project. Later syncs exclude those
directories.

## Boundary

Live retail stock is not generated in play: the descriptors are private, and
the current storeLevel on a visit and the non-literal store are unknown. The
`Host` binding is opt-in and the shipped client does not set it, which is
why every Godot result is unchanged. No baselines, fixtures or tests were
weakened. No push, no PR, and no edits to `~/champions-re` or
playable-integration (its wave 37 evidence was only read for comparison). No
process was killed by name.

## Fix round (audit `play-vendors-r2.md`, VERDICT FIX)

Commit `76758a35` fixes VS-1..VS-7. Research is still only
`research-vendor-stock@fdf6752` (`spec`, `ref`) and the `items.md` rows it cites.

- **VS-1**: the 2^20 watchdog, `set_retry_watchdog` and the idle counter are gone. The pass refuses at the first weapon/armour generator failure and at the first weapon bit-40 rejection (spec:121-122, 217-219), restores nothing because nothing was published, and keeps retrying the bounded duplicate rejections (six, then the seventh passes). Cycle detection was not added: under this rule every progress step ends within seven attempts. The pass runs in steps (`begin`/`step`, `Host::step_vendor_stock`), so loading reports "not ready" while it runs.
- **VS-2**: every stock slot carries a known-byte mask. Generated records leave +0x06, +0x22, +0x44 and empty-modifier values unknown (items.md:83-95, 724) unless an executed probe supplies them (`set_executed_residue`). `same_raw` is three-valued; a duplicate decision that depends on unknown bytes, a scan of a never-written slot, or a special adoption of one refuses. Only the seven fixed parser records are zero there (spec:280-285). The stale-bytes test now claims only the fixed records.
- **VS-3**: `loading_readiness` is a pure status query. Generation happens once, at `Host::stock_on_load` (the ordered stock point after the peer barrier, wired from `SessionHooks::shop_stock`). A test polls readiness repeatedly and sees one generation and an unchanged shared seed.
- **VS-4**: `VendorStock` has no defaults. Seeds, difficulty, party and the quest table start unset and refuse. The Host supplies both seeds (and refuses if either moved between steps), difficulty 0x4f719c, 0x4d8ae0 and each class word, and storeLevel through its own 0x86da80 table with creating-read semantics, then writes both seeds back. A VM `setv`/`addv`/`vars_set` of storeLevel cannot be classified as a literal and blocks the next pass until the act clear (`Host::change_level` calls `on_act_reset`).
- **VS-5**: the test binds a `VendorStock` to a real `level_flow::State::quest_values` and runs `begin_change`: card latch and death task keep storeLevel 7 (budget low 44572), the act clear zeroes it and the next pass uses the storeLevel-0 budget (low 1199) and lifts the non-literal block.
- **VS-6**: `sort_stock_by_value` is deleted; `step_sort` is the only sort. A tagged pass checks ascending signed value, that ties keep generation order (with at least one generated tie), that each published record's bytes belong to its item and come from the matching `candidates()` entry, and that generation order was not already sorted. A separate test replays the duplicate allowance independently from the generator results and requires a seventh duplicate that passes.
- **VS-7**: the price check is renamed ("a covered price of 59 leaves 41 gold and buys"); the watchdog wording above is corrected. The optional call-order test in `messages_tests` was not added.

### Fix-round gates (mini, evidence in ignored `work/vendors-r2-fix/`)

1. Release native build `--clean-first -j 4`: **PASS**, exit 0, 829 s (the mini's load average was about 88). GDExtensions `mac-template_debug`, `mac-template_release`, `level-texture-template_debug`, `level-texture-template_release`, `creature-template_debug`: **PASS**, every library `x86_64 arm64`, total 2,578 s.
2. Focused CTest (`vendor-stock-exact`, `items-exact`, `messages-exact`, `messages-api`, `menus-exact`, `level-flow-exact`, `script-commands`, `script-items`), `-j 3`: **PASS 8/8**.
4. Headless editor import (exit 0), then the unchanged strict Godot dev matrix (`tools/godot_matrix.py matrix --captures --timeout 1200`): **PASS 54/54**, exit 0. It ran next to the full CTest on a heavily loaded mini.
5. FULL_SAVE_LOAD: **PASS 300/300** presents. Continued and uninterrupted presentation SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical. Full synthetic combat 2,645 checks, 0 failures.
6. FULL_TRACE: **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
7. Journeys, unchanged `tools/run_loop_playthrough.py` against the read-only disc: headless **80** steps and rendered **81** (38 captures), pickup **10→11**, 52 walk ticks, 0 log errors, 6 refusal kinds, disc files 994/994. Step/result pairs and refusal maps are identical to round 2 (and so to wave 37) at every index. Wrapper exit 1 is the retained death-reload refusal, as before.
