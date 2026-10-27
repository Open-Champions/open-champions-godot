STATUS: MORE — bounded act progression implemented; full campaign progression remains REFUSED.

2026-10-27. Assigned `play-progression`, branch `codex/play-progression`, clean
wave-34 base `a68504da`. Local commits only; no push/PR or sibling edits.

## Survey of wave 34

Read the local wave-34 report, level-up r1 and fix report, quests r1, save r1/r3,
and mission2 r1 reports. Audits reviewed: native-levelup-r1-fix4, play-quests-fix,
play-save-fix2, play-mission2-r1; research-levelup-fix, research-mission2-fix2,
research-story-route-r3, research-save-screen-r2-fix, research-save-world-r2-fix.
These are evidence boundaries, not claims of a completed campaign.

- XP, earned/acknowledged level, stats and point spending already use the native
  live-record/menu paths. `observed_player()` reads live XP/level/gold/base and
  effective stats/resources; `bind_player_to_scripts()` installs those into the
  replacement host. The menu companion additionally retains point balances and
  sender-half state. Research-levelup@576b41c `docs/spec/levelup.md:28-65` establishes
  the XP and stat rules. This change adds no XP award, threshold or recalculation.
- `enable_level_script()` already saves/restores the authored campaign, binds
  the live record and copies existing raw inventory/equipment spans at stable
  addresses. Mission completion/availability, medals, decision/evil bits, quests
  and quest variables use the existing campaign bridge. This is bounded state
  carry, not proof of arbitrary imported records or every record byte.
- The menu already consumes runtime executable mission rows and native button /
  fade / availability checks. Completion never automatically offers another
  mission. Mission2's controlled offer is explicitly synthetic. The host story
  selection helper supports only the first two missions; later real objectives
  and links have no newly authorized implementation here.
- Native continuation, player/menu, campaign, level association and world-field
  companions already exist. They are authored formats, not retail memory-card
  compatibility. Dead Creature state and unknown load effects refuse.
- The bug fixed here: only the mission2 special route used the native act-change
  reset. Other table-proved cross-act carried entries retained the source act.

## Implemented, with pinned authority

Research-quests@7efd3ce `docs/quests.md:266-277` executes case-sensitive prefix
matching, current act first, with a table miss leaving the current act unchanged.
`docs/quests.md:285-298` establishes that the guarded act reset depends on the
act change, not the mission number. `docs/quests.md:163-172` establishes the
flag-39 keep/remove decision; it does not authorize silently dropping inventory.

`campaign_entry_requires_transfer()` now shares this table-based decision between
native entry and the pending-world bridge. Every table-proved cross-act carried
entry uses the existing native `Host::change_level()` path. The bridge preflights
unsupported inventory before loading the destination presentation. No mission
order, unlock table, address, reward or game-content payload was added.

The supported domain remains one local player, a bound source act, and known
empty live/projected inventory for cross-act entry; imported carry refuses.
Mission2 retains its stricter existing guard even for same-act entry. Ordinary
same-act carry keeps the existing raw inventory path and does not clear quests.
The default offline change options are preserved; this does not add card-latch,
death-task or network transition support. Same-act and table-miss behavior remain
as researched; neither invents a new act or a mission destination.

On an accepted cross-act entry the native reset clears journal/quest-variable
values, preserves their names and mission flags, and leaves the carried player
XP/level/gold state intact. Quests therefore do NOT all carry unchanged across
act boundaries. Existing stats and inventory carry code is unchanged.

Authored regressions in `native/tests/mission2_host_tests.hpp` use synthetic
act tables/destinations and numeric player values, not retail mission ordering.
They require non-mission2 cross-act clearing, same-act preservation, explicit
XP/level/gold/mission expectations, replacement-host save/load, no manufactured
mission-3 unlock, and inventory refusal before candidate campaign mutation.
The pre-change implementation would fail the later cross-act/reset assertions;
this sensitivity is a static code comparison, not an executed old-binary result.
The fixture proves native progression/continuation, not an actual mission finish.

## Explicit refusals and missing research handoff

REFUSE full campaign completion/unlock sequencing, arbitrary carried records,
nonempty/imported cross-act inventory, live mission2 completion, and campaign
save/load through an unproved physical exit. Existing visible/exception refusal
paths are retained; there is no guessed fallback or auto-unlock.

1. **Full mission unlock predicates:** research-quests@7efd3ce
   `docs/quests.md:87-131,320-323,348-350` is a call-site census, not an executed
   prerequisite graph. Need per-entry containment, dialog choices, quest-variable,
   alignment, difficulty, completion and repeat/replay conditions for every offer
   and turn-in, including branches with variable mission arguments. Do not derive
   adjacency or act order from mission numbers or filenames.
2. **Live first-to-second offer:** research-quest-log@03c2304
   `docs/spec/quest-log.md:129-137` identifies separate availability action entries
   but explicitly leaves their live-dialog reachability unproved. Need original
   execution of those paths with positive and suppressing conditions. A forced
   byte or synthetic offer cannot close this gap.
3. **Mission2 route and completion:** research-mission2@047adfe
   `docs/mission2.md:3-10,23-61` leaves individual record-to-entry mappings, call
   containment, null-world destination resolution, dialog/objective branches,
   deferred completion, boss/drop/portal gates and actual travel Unknown. Need
   those executed before automatic travel or turn-in completion can be exposed.
4. **Inventory reset composition:** the flag-39 keep decision is established in
   research-quests@7efd3ce `docs/quests.md:163-172`. That pin stubbed removal fixups.
   Related research-items@49bc578 `docs/script-items.md:9-15,39-47,92-102`
   separately establishes the removal primitive; this is NOT a claim that the
   primitive is still unresearched. The client still lacks the composed live-bag
   migration/reset binding. Need original act-entry comparisons for mixed bags,
   equipped/active removed items and retained items, plus imported ownership and
   all resulting pointer/load/stat state, before widening this bounded entry.
   This includes implementation work as well as composition evidence.
5. **Exact entire record across travel:** existing observed-player and raw-span
   carry does not prove every skill/ability/passive/resource/ownership byte of an
   imported character. Research-saves@8d43286 `docs/save-format.md:83-122` establishes
   snapshot/pointer fixups, not the client's entire live transition composition.
   Need an original before/after record comparison through real completion,
   hub arrival, next entry and reload; do not equate companion roundtrip with it.
6. **World/exit restore:** research-save-world@e7d8972
   `docs/save-world.md:109-112,141-142` leaves class load effects/reconstruction
   Unknown. Need live/dead Creature, item/chest, trigger/gate and portal effects
   with the world present. Those existing refusals block the ordinary journey's
   departure and successful saved-level reload; no save fixture was weakened.

No research job was launched: this implementation sub-agent records the exact
handoff for its owner. Research files were read only. No new decoded names,
private input paths, game bytes or assets are included in these changes.

## Gates

A pending-full-CTest note was committed in e8c83918 and c948a935 before the full
CTest invocation. No baseline, fixture, harness or pickup assertion was changed.

- Release native configure/build: PASS (`-j 4`); focused script-commands 1/1 PASS,
  0.39 s. A redundant overlapping native build was accidentally started; both
  completed, then a sequential verified build and focused test passed. This is
  not reported as two independent validations. Logs under ignored
  `work/progression/`; the initial build log will be copied there at completion.
- Full CTest `-j 3`: PENDING. Exact count/time and isolated timeout reruns follow.
- Three development GDExtensions: Release / template_debug built at `-j 4`.
- Strict Godot dev matrix, FULL_SAVE_LOAD and FULL_TRACE: PENDING.
- Headless/rendered standard journey versus wave34 80/81 steps: PENDING.
