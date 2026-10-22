STATUS: COMPLETE — audit fixes committed after native arm64 CTest 228/228, Godot dev matrix 54/54, 111/111 sweep (scripts on and off), FULL_TRACE unchanged.

# Items gaps round 1 — ground item, host bit, generators, hide/show, givePlayerRandom

Branch `codex/items-gaps` (worktree `~/Projects/open-champions/worktrees/items-gaps`), from `codex/playable-integration` `b75c174` = wave 23 (its report exists, uncommitted, in the integration worktree; I used its commands and baseline sweeps `w23on` / `w23off`). Source: `t3/research-items-gaps` `ecf9742`. Local commits only: nothing pushed, no PR, `~/champions-re` untouched, the disc read only (counts only, in ignored `work/private/`).

## Audit fixes (2026-10-22)

The final Claude audit identified IG-1–IG-7. This section supersedes the
ground-item and random-giver acceptance claims in the original round-1 text
below. Pinned research is `research-items-gaps` `ecf9742`; see
`docs/ITEMS_GAPS.md` for the resulting bounded contract.

| Finding | Fix and evidence |
|---|---|
| IG-1 MEDIUM | Removed special-level `+0x20/+0x21` stores from shared `item_parseDescriptor`; `docs/spec/item-drops.md` section 2 places them only in `givePlayerRandom`. The random handler now refuses, so no other give may acquire those bytes. |
| IG-2 MEDIUM | Refuse a ground ItemProp before `pool_new(0x190)` allocates or draws, and refuse direct `ItemProp_ctor2` calls. `docs/spec/item-drops.md` sections 1.2–1.3 and `ref_item_drops.py:186-206` require resource names, `res_find` pointers and model-bounds fields; the host has no bound source. The former zero-stub vectors could not prove those fields. All 400 authored cases now assert refusal/no published object; the underlying pool capacity test remains. |
| IG-3 MEDIUM | Refuse `givePlayerRandom` before its first draw, even with other inputs bound. `docs/spec/item-drops.md` sections 5–6 leave the stack record after ten failed generations unmodelled and recommend refusal. A direct service call also refuses. The 120 random-giver vectors are retained as research fixtures, not native acceptance evidence. |
| IG-4 LOW | Removed the untraced `campaign_->set_difficulty` from `receive_launch`; `docs/spec/items-gaps.md` section 2 pins the receiving memory writes only. |
| IG-5 LOW | Cited pinned `docs/monsters-2.md:7660-7667` and `symbols/monsters.tsv:262` for the ScriptGenerator constructor's size, table, flags and facing. |
| IG-6 LOW | A creature stub with `hide=` or `wait=` refuses hide/show because its constructor's load effects have not run (`docs/spec/hide-monster.md` section 3.4). The build-refusal counter records this case. |
| IG-7 LOW | `bind_level_name` refuses because the allocation capacity at `0x41b721` is not pinned. `pool_new` now propagates a corrupt-pool exception; a full pool's normal zero return remains independently tested. |

**New refusal categories: 4** (ground ItemProp, random giver, creature stub
with hide/wait load property, and unbounded level-name write). These are
contract-level categories, not counts of private-disc occurrences. No inferred
constant was added. The previous 264 ground builds / 100 random completions
were zero-stub or stale-record contracts and are withdrawn as native claims.

### Audit verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: pass; native test binary is arm64.
- `ctest --test-dir build/native --output-on-failure -j 3`: **228/228 pass** on
  the final tree, 670.14 s (`work/items-gaps/verify/ctest-audit-final-arm3.log`).
  Focused `script-commands`, `script-items`, `item-drops`, `net-launch` and
  `level-factory`: **5/5 pass**.
- Universal Godot debug and release extensions rebuilt; Godot dev matrix via
  `python3 work/items-gaps/run-godot.py`: **54/54 pass** on the final tree.
- `bash work/run-slices-r21.sh ig-final-on --scripts` and
  `bash work/run-slices-r21.sh ig-final-off`: each **111/111 tried, loaded and
  played**, 230 checks, 0 failures, empty sweep refusal lists. Scripts-off
  deterministic counts match the prior `igoff` run. Scripts-on identifies 56
  creature stubs with hide/wait load properties; `refused.hideMonster` rises
  45→49, `refused.showMonster` rises 1→4, and total stub command refusals
  rise 156→163. No successful item give or ground-item path is claimed.
- FULL_TRACE from `combined-full-combat`: 31,622 bytes, SHA-256
  `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`,
  unchanged. Private-disc results are counts only; sweep logs remain ignored.

## What was ported (see `docs/ITEMS_GAPS.md`)

| Gap | Result |
|---|---|
| `0x4d8ae8` writer | The research proves there is none. `recalc_load_limits` now uses the constant 0 and never reads the word (`LOAD_BONUS_VALUE`); `load_limit()` keeps the branch for parity. |
| `+0xf4` flag | New `net_launch` (lobby/menu host bit, host-only fill of record `+0xa0..+0xa3`/`+0xf4`, receive half) and `Host::receive_launch`. 1,612 vectors from `ref_items_gaps.py`. Nothing in the play session launches a net game yet. |
| `ItemProp_ctor2` and ground-drop draws | `pool_new` takes a pool block; `ItemProp_ctor2` at launch 0 draws once from the shared seed (which seeds the prop's own generator) and once from the prop's own generator (facing), writes the block, registers the object. 400 vectors from `ref_item_drops.Ground`, whole 0x190-byte block compared (264 builds, 136 refusals). |
| `givePlayerRandom` | Runs with its real callees (blank record, special-level bytes, `item_generateWithAffixes`, weapon/quality tables read from the image, stale-record refusal). 120 vectors from `ref_item_drops.RandomGiver`: 100 completed (54 ground drops, 66 special-level items), 20 refused faults; every byte and the shared seed match. Still gated in the host until the session binds the level name, monster level and executable (0 shipped calls). |
| Generators | `ScriptGenerator` records are built (`Host::build_generator`); `generateMonster` spawns through `Host::create_object` = factory row + a ported constructor. |
| `hideMonster` / `showMonster` | Stubs of factory rows the research probe decided run (flag `0x8000`, off-grid list, `+0x231` bit 3 for creatures only). `tools/generate_creature_class_table.py` runs the research's `objects_census.probe_class`: 201 rows, 97 creature, 100 non-creature, 4 undecided (those 4 are shadowed rows no record can reach). |
| Non-literal `generateMonster` names | No special case, as the research says: the bytecode runs, the same factory classifies the name. |

Kept refused (Unknown): classes whose constructors are not ported (spawn refuses before taking a pool block); the Skeleton rise; hide/show on stubs of property-decided classes (see below); `givePlayerRandom` without bound inputs; `+0xf4` consumers beyond the contract.

Provenance note: the `ScriptGenerator` constructor body (table `0x514e60`, flags `|= 9`, facing at `+0x38`) is read from the pinned research repository's disassembly `0x265f54-0x265fbc`, not from a spec document (`hide-monster.md` gives only the class and size). It is flagged in `docs/ITEMS_GAPS.md`.

## Verification

- **Native arm64 Release CTest:** first full run 226/228 (see below), then both failures resolved: `script-builtins-exact` failed because I had added two calls to the binder that the research's pinned call log does not contain; I moved those stores into the item service (commit `0aec41f`) and it passes (128.8 s). `player-exact` hit its 240 s timeout under load from other agents' jobs (as in wave 22). The final full rerun on the final tree passed **228/228** in 1,064 s (`work/items-gaps/verify/ctest-arm3.log`); wave 23 had 226 tests, the two new are `net-launch` and `item-drops`.
- **New tests:** `net-launch`, `item-drops`, the `givePlayerRandom` cases in `script-items`, `level-factory` creature table, and new `script-commands` tests for hide/show on stubs, generators/spawn, launch receive and the `givePlayerRandom` gate. Every contract test was generated from the pinned research models (hashes in `native/tests/*_contract.json`).
- **Godot dev matrix:** 54/54 (`work/items-gaps/run-godot.py` = wave 23's list; the three creature-anim runs needed `gdextension/creature_anim` built first, which I did). Four universal GDExtension configure/build pairs pass.
- **111-level sweep** (4 slices, 60 ready ticks, scripts on and off): **111/111 tried, loaded and played, 230 checks, 0 failures, empty refusal lists**, both runs. Scripts-off: every deterministic count identical to wave 23. Scripts-on changes versus wave 23 (`w23on`), excluding time/audio/text-frame noise:

| Key | w23 | now | Why |
|---|---:|---:|---|
| `executed.hideMonster` / `refused.hideMonster` | 8 / 114 | 75 / 45 | stubs of decided rows run |
| `executed.showMonster` / `refused.showMonster` | 1 / 14 | 14 / 1 | same |
| `scripts.objects_built` | 324 | 429 | +105: every `ScriptGenerator` record (matches the research's 105) |
| `scripts.objects_unbuilt`, `load_draws_unmodelled` | 15,577 | 15,472 | −105: those records now make their one shared draw |
| `scripts.stub_refusals` | 266 | 156 | −110 (the hide/show lifts) |
| `refused.generateMonster` | 6 | 5 | still 0 executed: the five reach a class whose constructor is not ported and refuse |
| `scripts.refusal_stops` / `reached_wait` | 14 / 30 | 7 / 37 | scripts that stopped at hide/show now run on |
| `refused.saveCheckPoint` | — | 8 | newly reached by those scripts |
| `executed.getRand`, `addDeferedCall`, `getv`, `addParticleOnObject`, `sin1000`/`cos1000`, `countQuestItems`, `getScaledTime`, `setv`, `setSaveInhibit`, `campaign.vars` | see `work/items-gaps/compare.out` | higher | more of each script runs |
| `executed.callScript`, `countPlayersAlive`, `findDistance`, `closestPlayerDist`, `gamePaused` | 104, 345, 9, 2, 2,672 | 84, 325, 3, 0, 2,630 | lower. Inferred, not isolated: the 105 generator draws shift the shared seed stream, which changes random-dependent branches |
| `refused.followPath`, `revealMap`, `setCreatureParams`, `setMonsterVal`, `showOnMap` | 14, 3, 37, 63, 24 | 13, 2, 31, 57, 14 | scripts reach them less (fewer) or more; the full table is in `compare.out` |

  No `executed.generateMonster`, `givePlayerItem`, `givePlayerRandom` or ground-item count appears: no sweep level reaches a successful path (the first-round gives refuse without bound item tables in the play session). The audit fix withdraws the ground-item and random-giver native acceptance claims above; their vectors remain research fixtures.
- **Residual hide refusals (45) and show (1)** are all stubs of *property-decided* classes (no name row: constructors `0x23d93c` 34, `0x23ad18` 10 + 1 show, `0x2547fc` 1). The research states 53 property-decided probes stopped before `pool_new` and are Unknown, so they stay refused; a new per-command counter (`scripts.stub_refusal.<command>: <reason>`) now reports this.
- **FULL_TRACE:** the dev headless Full run (`combined-full-combat`, part of the matrix) prints the same trace hash as wave 23; `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes, identical to wave 23.

## Not verified

x86_64 CTest and exports were not rerun for this branch. The play session does not bind the item tables, level name, monster level or a net launch, so those new paths run only in tests. Windows/Linux builds not rebuilt.
