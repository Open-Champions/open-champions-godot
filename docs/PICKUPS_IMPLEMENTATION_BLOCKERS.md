# Real-level chest and pickup implementation handoff

STATUS: BLOCKED

Scope: RTA US v2.00, `codex/play-pickups` from `252f34a`. The worktree was
clean on inspection: there were no partial edits to retain or discard. No
simulation, input, inventory, rendering, or RNG behavior was changed. This is
an evidence review, not a chest or pickup implementation.

## Evidence reviewed

- Shared `rules.txt`, repository `AGENTS.md`, `CONTRIBUTING.md`, and
  `docs/ROADMAP.md`.
- Research worktree `research-pickups`: `FINDINGS.md`, `docs/pickups.md`,
  `docs/spec/items.md` section 8, and `docs/monsters.md` Chest and container
  sections. Handoff commits: `24f4598`, `9911a6b`.
- Audits `research-pickups.md` (FIX) and `research-pickups-fix.md` (PASS).
  PASS resolves the prompt/generator-argument and shared-recipient coverage
  findings; it does not establish an integrated level path. Its remaining LOW
  finding concerns the absent post-fix execution log. No research difftest was
  rerun here, and its reported counts are not native acceptance results.
- Inventory commit `20a04f7` and its branch report, inspected without merging.
- Existing `docs/CAMPAIGN_FIGHT_REWARDS.md`, `Fight::Impl::roll_loot`,
  `script_items::Service::ground_item`, and the combat `Player_interact` port.

## Blocking contracts and existing refusals

1. **Loot roll to flying world item.** The items spec documents allocation,
   initial position, the constructor's shared draw, and horizontal launch
   velocity. It explicitly leaves `PhProp_ctor` outside that model; the
   difftest substitutes a constructor. It does not supply the complete
   loot-call resource/model/texture arguments, part mask, launch flags and
   scale, or full `PhProp_initPhysics` inputs. The existing fight diagnostic
   names these missing inputs precisely. Preserve it, the generated record,
   and `GroundDrop::spawned == false`; do not substitute a resting item for
   the documented flight/settle/allocation-loss path.
2. **Chest creation and execution in a real level.** The audited synthetic
   prompt/open/run cases establish useful branches, but stub the called
   services and omit placed item/potion/random properties. They do not give a
   complete ordinary Chest construction/resource/animation binding and its
   full placed-loot launch path. The general monster notes list that loot's
   order but do not close the missing construction/physics inputs above.
   Supply those contracts before registering a live Chest. Do not treat
   AutoChest or AutoContainer as aliases, or replace animation completion
   with a guessed timer.
3. **Resting world items and inventory drops.** The existing ground-item
   service refuses `ItemProp_ctor2: resource and model-bounds services are
   unbound`. A typed free-spot result alone does not bind those services or
   establish a fully registered item. The research handoff explicitly calls
   for concrete construction, registry removal, and level save/reload tests;
   its pickup tests stub world removal and inventory mutation.
4. **Gold reachability and contact.** Gold constructor/run/touch/HUD-credit
   bodies already exist. The handoff explicitly leaves the numeric pickup
   radius unknown: contact must come from original movement/object geometry,
   not an authored proximity sphere. The current boar death has zero Gold
   chance; its captured container Gold is not boar loot. A live source object
   and the movement-to-touch binding remain necessary. Chest second-use Gold
   must request the existing HUD floater path, not directly increment a bag
   or the player's Gold word.

The shared rules require stopping when exact behavior is insufficiently
documented and prohibit invented constants. Consequently no placeholder chest,
fabricated constructor arguments, diagnostic-only spawned item, automatic
manual-item pickup, or instant Gold credit was added.

## Established rules to preserve when unblocked

- Keep the real `Player_interact` nearest-object query and K1 decision. Its
  existing refusal for a nearby interactable must not be bypassed with a
  separate Godot proximity loop.
- Pickup refuses a destination bag already containing 63 records, even
  though `inventory_giveItem` itself can fill cell 64. Check load plus item
  weight against the actual limit before calling the inventory handler.
- Bit 39 attempts players in slot order, rather than only the selected
  player. Delete the world object once if any helper accepts. The helper's
  acceptance and `inventory_giveItem`'s unique-item failure are distinct.
- Use the existing inventory bag/stack/item representation. The inventory
  audit's unverified footprint refusals are presentation constraints, not
  permission to invent icon dimensions or change inventory capacity.
- Preserve shared/own draw ordering, pool failure losses, flight-to-rest
  conversion, HUD-credit delay, and level teardown/save ownership.

## Gates and disposition

Only read-only source/evidence inspection and the report's whitespace check
were performed. No private game files were read or modified.

| Requested gate | This handoff |
| --- | --- |
| Release configure/build | Not run: implementation stopped at the evidence gate |
| Full `ctest -j 3` | Not run; no new pass count claimed |
| Godot development matrix | Not run; no new pass count claimed |
| 111-level sweep | Not run; no new acceptance count claimed |
| FULL_TRACE | Not regenerated or hashed here |
| `tools/run_loop_playthrough.py` | Not run; real-level pickup remains unsupported |

The requested FULL_TRACE baseline is 36,402 bytes, SHA-256
`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
It is a supplied baseline, not a verification result from this worktree.
Running the unchanged client's old journey would not verify the missing
chest-to-world-to-inventory path. Once the construction contracts are supplied,
implement that path, test the branches above with synthetic fixtures, then run
every requested gate and inspect real rendered interactions before claiming
completion. Commit this report locally only; do not push.
