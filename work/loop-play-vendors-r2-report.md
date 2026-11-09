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
| The four destinations at 0x554678/80/88/98 dispatch modes 1, 1, 2, 3. Every matching load regenerates, a non-match does nothing, mode 0 keeps the lists. No freshness gate. | `vendor_stock.hpp` `kShopDestination*`; `VendorStock::on_load`, `open_or_stock`; `Host::loading_readiness`; `SessionHooks::shop_stock` | spec:17-30, 326-329 |
| Budget from storeLevel (first read for scale, second for budget), difficulty, raw PS2 float steps, the `(progress+1)` attempt budget, caps and the 44/40/36 pointer-cell offsets. Out-of-contract inputs are refused. | `vendor_budget` | spec:36-43, 67-102; ref:15-44 |
| Shared seed for parser/generators, three cosmetic-seed draws (one after each ordinary loop), special budgets `2*(low+n*step)` with n = 22, 17, 10. | `regenerate` | spec:156-185; ref:47-58, 135-145 |
| Class-4 armour fraction through the party passed to the existing generators. | `set_party` → `Game::classes` | spec:40-43 |
| Seven fixed records from `parse_descriptor` (4 / 0 / 2+1). The count increments before parsing and the return is ignored. The parser's own bytes replace the destination; no old bytes are kept. | `regenerate`, `write_record` | spec:90-97, 280-285 |
| Ordinary records from `generate_with_affixes`, specials from `generate_preset`. The generator writes slot `count` before the outer tests, so a rejected candidate stays there. A failure preserves the slot. Six duplicate rejections, then the seventh passes. Bit 40 rejects weapons even after that allowance. Lists 0/1 retry the same progress on failure or rejection; list 2 advances every attempt; 0xff suffix byte rejects in list 2. A special is appended when the slot pointer is nonzero, whatever the generator returned. | `regenerate` loop (`vendor_stock.cpp:384-442`) | spec:106-131, 173-179; ref:107-145 |
| Duplicate scans: list 0 scans itself; list 1 scans the **previous third list**, which is kept until armour has finished; list 2 scans the name region at 0x86b698, where deferred names never match. The `same_raw` identity includes +0x22 and not quantity. | `scan_record`, `duplicate`, `same_raw` | spec:124-146, 287-301; ref:61-64, 116-120 |
| Stable ascending signed-value sort that swaps whole 0x48-byte records. | `regenerate` sort, `sort_stock_by_value` | spec:148-152; ref:149-158 |
| No restock by gold and no 121-tick daemon (0x1c8a00 is never called). | none by design | spec:54-63, 256-276, 334-336 |
| storeLevel: the creating read stores 0. Literal `setv` values are `hub`/`hub2` 0, 8, 7, 6, 5, 7, 6, `stobonus` 7 and `islgnome` 3. The guarded act change zeroes it (card latch and death task skip). Optionally bound to the session quest-variable map, where the existing level-flow clear zeroes it. | `read_store_level`, `apply_literal_setv`, `on_act_change`, `bind_quest_values`; `level_flow_tests.cpp` storeLevel checks | spec:45-52, 303-322, 330 |
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
- **Unbounded generation failure**: the original has no overall bound (spec:121-122, 217-219). The port retries exactly. Only after 2^20 consecutive non-accepting attempts at one progress step does it refuse the pass, restore the previous state and publish nothing (`vendor_stock.cpp:433-439`). This watchdog is a port safety limit, not an observed value (the research probe likewise ran under an instruction limit, spec:218-219). The limit can be set with `set_retry_watchdog`.
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
