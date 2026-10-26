STATUS: MORE

# Native loot, pickup, Gold and starting equipment

RTA US v2.00 only, on `codex/play-pickups` from `bcfebff6` / `252f34a6`.
Research: `research-loot-drop` **77c2f70073e037f5b160e295eab9d803340ee6d1**,
`docs/loot-drop.md`, its differential probes, and the PASS corrective audit
`research-loot-drop-fix.md`; upstream `research-pickups` **9911a6b**, its
`docs/pickups.md` and PASS corrective audit. The earlier native blockers report
is retained as historical evidence. No inventory branch was merged.

## Implemented boundaries

- The campaign loot generator now binds its successful pool allocation to a
  real shared-World PhProp. It uses allocation `0x248`, runtime resource row,
  mask 1, scale 1, launch `0x441`, source position plus the supplied z offset,
  one shared constructor draw and the existing eight-own-draw constructor.
  Equipment's shared angle draw follows successful construction, then overrides
  horizontal velocity and reinitializes physics with three more own draws.
  Allocation failure skips those construction/angle draws. No resting item is
  substituted at roll time.
- The existing native rigid-body run receives actual integer model bounds,
  hull bytes and the loaded WORLD's height/sweep services. Its signed timer,
  six momentum tests, `0x190` replacement allocation and unconditional flying
  deletion on replacement failure remain intact. Replacement identity reaches
  the campaign adapter before a later actor can pick it up in the same tick.
  Resting objects request no run list and have no invented age expiry.
- ItemProp ctor2 and ctor3 are distinct constructor bodies. Ctor2 uses named
  resources/bounds, facing and Prop launch (inventory's launch flags are zero);
  ctor3 takes the settled transform with no new bounds/run request. The separate
  inventory UI's live drop integration remains refused; that branch was read
  at `20a04f7` without merging or changing its footprint refusals.
- Player_run retains its K1 gate. Player_interact uses the existing x/y +/-48,
  z..z+60 box, capacity 256 and strict nearest-origin ranking. Slot 13 runs
  without a press; slot 14 requires pressed use (or status-2 bit `0x10`) and
  simulation mode. The host's E input queues that pad edge using the live use
  binding. No separate proximity pickup loop was added.
- Pickup uses the existing 72-byte bag records, stack handler, actual item
  weight and load limit. Count 63 refuses, equality at the load limit accepts,
  bit 39 tries every active slot in order, and one destructor removes the
  world object after any helper accepts. The helper deliberately does not turn
  inventory_giveItem's unique-item `-1` into a refusal.
- Gold uses the existing ctor/run/touch/HUD bodies, base radius/height 24/72,
  player radius 12, strict horizontal sum 36 and original broad-phase vertical
  gate. Countdown 41 does not despawn it. Touch clears its amount and queues
  deletion; HUD credit starts at age 43, pays `(pending+7)>>3`, and caps the
  balance at 99,999,999. A full eight-floater queue credits immediately.
  The traced broken/no-placed-item AutoContainer seam preserves its own draw
  threshold 24000 and allocation-before-amount draw. It does not register a
  fictitious live container. WarBoar's zero Gold chance is unchanged.
- Ordinary Chest construction binds the addressed descriptor/model/texture/
  opening clip, gold/trap selection and hide variant. Factory prefix precedence
  preserves reachable variants 0/4. Opening uses the actual clip with `0x204`;
  state 2 and run-list removal require completion bit `0x80`. Placed descriptor,
  potion and random-worth launches use z+12 and their recorded scatter/reinit;
  the final generator retains its zero Gold chance. Second-use gold requests
  the existing HUD floater rather than changing the bag or balance directly.
- All seven normal starting grant sequences resolve descriptors from the
  executable at runtime, preserving parser variants, byte-0x20 overrides,
  grant/equip order, quantities and flags. Starting balance 500 remains the
  new-character record's balance. Raw bag records and in-record equipment
  pointers carry across host replacement for the same character revision;
  session spawn preserves their weapon class. New characters receive grants
  once. Neither item names nor decoded descriptors are embedded in source.

## Verification

Audit repair verification (2026-10-25). Logs and private runtime artifacts stay
under ignored `work/pickups/verification/pp2-*`, `work/pickups/matrix/`,
`work/pickups/sweep/` and `work/pickups/pp2-playthrough/`.

- Release native and Godot extension configure/build: passed (`-j 4`).
- Native targeted pickup checks: 221 passed; `pickups` and `script-items`
  CTest regressions: 2/2 passed. Python pickup-runner regressions: 6/6 passed.
- Runtime starting parser/insertion probe: 7/7 classes passed; equip/stat/skill
  callbacks are recorded boundaries in this probe, matching the research.
- Full CTest (`ctest --test-dir build/native --output-on-failure -j 3`):
  242/242 passed, 777.19 seconds; no timeout reruns needed.
- Godot development matrix (`python3 work/pickups/run-godot.py`): 54/54 passed.
- 111-level sweep (`bash work/pickups/run-sweep.sh`): 111/111 loaded,
  zero failures across four slices, with existing bounded service refusals.
- FULL_TRACE unchanged: 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- Extended `tools/run_loop_playthrough.py`: headless and rendered FAILED
  (both exit 1). Each produced a drop after nine retry kills and walked 20 ticks
  toward it, but inventory units stayed 10 -> 10 and pickup remained false. The combat
  journey therefore remains blocked, not accepted; its post-pickup file and
  cold-load checks were not exercised. No missing service/value is guessed.
  It follows
  produced native drop positions using ordinary movement and pressed E, and
  verifies an inventory-quantity increase. It does not force a loot roll,
  teleport to a drop, manufacture an item or claim rendered pickup fidelity.
  Disc file counts stayed 994 before/after. Rendered run produced 13 screenshots;
  inspected its save screen and confirmed the explicit inventory/equipment
  refusal with current session retained. The refused save writes no scene file.

Commands: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`,
`cmake --build build/native -j 4`,
`cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release`,
`cmake --build build/godot-extension -j 4`,
`ctest --test-dir build/native -R 'pickups|script-items' --output-on-failure -j 3`,
`build/native/champions-pickups-tests`, and
`python3 -m unittest discover -s tools -p test_run_loop_playthrough.py` all passed.
The journey command uses `python3 tools/run_loop_playthrough.py --godot` with
the existing integration runtime, `--game-folder` with the read-only disc, and
`--out work/pickups/pp2-playthrough`; it failed as described above. Godot's
`--headless --path godot --script res://tests/loop_playthrough.gd --check-only`
also passed after updating the refusal-screen back route.

## Corrective audit PP2-1 through PP2-4

- PP2-1 (`9ef0968f`): adopted Chest destruction retires the exact host owner,
  handle heads, combat view, animation and Chest index together. Identity/class
  checks and checked insertion protect reuse. The synthetic regression opens,
  destroys and reuses one pool address as another Chest and a different object.
- PP2-2 (`dcf8f2cf`): action-bearing or trapped Chest branches throw before
  opening, dispensing or completion. Tests cover initial states 0/1, empty and
  nonempty action properties, trapped opening, and the supported no-action subset.
- PP2-4 (`df421ac9`): Host notification delegates propagate unsupported and
  faulted script results rather than completing equip/grant. Both outcomes have
  synthetic script-item regressions; successful callback coverage remains.
- PP2-3: no new inventory/world save format is invented. Both scene save modes
  and the scalar player companion preflight authoritative memory before any
  serialization, file opening, replacement or old-companion deletion. Nonempty
  bags, equipped pointers, pending Gold floaters, pending age-43 credit balances,
  opened/destroyed Chests and any produced drop (including picked-up/lost drops)
  refuse explicitly. The equipment range ends before ability data at `+0x4440`.
  Evidence: loot-drop `docs/loot-drop.md:280-294`, pickups `docs/pickups.md:71-78`,
  and existing inventory/equipment field contracts. Native tests cover pickup,
  equipment-only, changed-world-only, all floater ages before credit and all
  seven remaining credit ticks. Clean/fully credited scalar state stays allowed.
  The real journey checks all three save entrypoints against absent files and
  existing authored sentinels after pickup, including temporary-file absence.
  A fresh scene must refuse loading the absent output, not claim an inventory
  roundtrip. Save-dependent journey stages are explicitly unavailable, not
  reported as successful restores.

## Refusals and limits

Original live flight/settling visuals, rendered item pickup, source-container
integration beyond the traced Gold branch, and dropped-item save/reload remain
unverified. Saving with inventory/equipment, changed pickup/Chest world state or
outstanding Gold explicitly refuses before writing. Item models, original prompt text, sound presentation
and Chest lid rendering are not claimed as rendered gameplay acceptance.

The trapped Chest explosion and completion action service were not executed by
the cited loot probes, so those paths remain labelled refusals. Gold's animation
reference is unbound in this host; countdown behavior is supported and the
existing run body retains its animation-completion branch. Complete equipped
stat/skill recomputation and native inventory persistence are separate limits;
the scalar player/campaign save test does not establish an inventory roundtrip.
Existing campaign AI, model attachment and level-load RNG refusals remain.
No original-campaign seed, target-platform parity or PCSX2 visual equivalence
is inferred from native tests or a rendered host run.

All private disc access is read-only. Reports contain counts and public contract
terms only. No push, pull request or issue was created.
