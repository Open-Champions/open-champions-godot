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

Final gate figures are recorded below when execution completes. Logs and
private runtime artifacts stay under ignored `work/pickups/`.

- Release native and Godot builds: pending final gate record.
- Native targeted pickup checks: 136 passed before final full CTest.
- Runtime starting parser/insertion probe: 7/7 classes passed; equip/stat/skill
  callbacks are recorded boundaries in this probe, matching the research.
- Full CTest, Godot matrix, 111-level sweep and FULL_TRACE: running.
- Extended `tools/run_loop_playthrough.py`: pending final run. It follows
  produced native drop positions using ordinary movement and pressed E, and
  verifies an inventory-quantity increase. It does not force a loot roll,
  teleport to a drop, manufacture an item or claim rendered pickup fidelity.

## Refusals and limits

Original live flight/settling visuals, rendered item pickup, source-container
integration beyond the traced Gold branch, and dropped-item save/reload remain
unverified. Saving while a generated ground item remains live explicitly
refuses before writing. Item models, original prompt text, sound presentation
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
