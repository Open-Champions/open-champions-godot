# Mission 2 hostile actor boundary

RTA US v2.00 only. Evidence: `research-mission2` **ffff9a7**,
`docs/mission2-enemies.md`, `tools/ref_mission2_enemies.py`, and the
addressed Clockwork, Droid and IronGuardian descriptions in `docs/monsters-2.md`.
The hostile audit and `research-mission2-fix2.md` both read **PASS** when
implementation began. The inferred four-level route is not a proven campaign
progression path.

`mission2_actors::{ClockworkCutter,ClockworkHammer,Droid,IronGuardian}` finish
the class-owned numeric constructor over the script host's base Creature
constructor and its existing World allocation. The factory selects the
documented constructor, allocation size and definition. The base constructor
uses the selected level's balance table through the existing stat service;
the class part does not allocate, draw the shared seed or roll stats again.
Final native stats are a composition of existing services, not an original
loaded-level stat comparison.

Definition names are read from the selected executable at runtime, then
resolved in order through the resource manager (26/26/10/34 slots across the
four classes). No extracted resource-entry names are compiled into this port.
Resolved words are native opaque IDs, not captured original pointers. Both
Clockwork variants share their definition and retain separate radius, scale,
subkind and melee flags. Droid retains its frame-based clock and 3600-frame
expiry; IronGuardian retains its home/activity ranges and zero spawn counter.

The class run prefix, active gate and the 16 measured synthetic states
(0, 3, 6, 20 per class) dispatch through explicit lower services. State 6
retains cutter flags `0x40008`, hammer flags `1`, and guardian flags `8`.
Droid's measured state 23 explosion dispatch retains inner/outer radii
60/144, signed maximum damage from `+0xd2`, type `0x40840`, and ten prop
requests. The inherited damage body writes the same shared HP/pending-damage
bytes as the combat World. Synthetic service bindings also exercise actual
shared-World melee and radius damage; this does not establish landed attacks
on an original level.

Direct death entry retains the filter, Clockwork own draws and eight-part
route, exact six raw loot arguments, animation-slot request and on-death call.
Droid has no class loot request. Death entry is tracked so a refused lower
service cannot repeat an item roll on later damage ticks. The ordinary loot
adapter refuses ranked/tinted cases, including IronGuardian, before rolling.

The play session installs these actors alongside the WarBoar family, adopts
them into the same combat World as the Player, and exposes their actual class
identities in observations. The boar presenter skips the new class identities;
their loaded models and animation are not presented as verified.

## Remaining refusals

- Loaded constructor results: Clockwork Weapon2/mesh masks, initial clip state,
  complete per-entry masks/times for all classes, Creature_init model bounds,
  and IronGuardian's extra archive-load service. The archive itself is made
  available by the loader; that does not execute the original lump service.
- The live scene stops active animation before producing events from those
  unknown constructor results. AI refusal preserves processDamage and
  Creature_run; it does not silently run a guessed idle, attack or movement.
- Non-death state entry, live-target AI, cutter missiles, hammer steam with
  sampled velocities, corpse explosion events, Droid fuse entry and killed
  props, IronGuardian missiles/junk and its four-handle/fifty-total spawn path.
  Unexecuted states are refused even where static prose suggests later work.
- Shed-part bodies, loaded death animation, ranked/tinted loot, ground loot
  construction/landing/pickup, named boss drop, quest credit and boss portal.
  The scene records requests/refusals for void effects; it creates no fake prop.
- Clockwork's unique slot 36 and the unique Droid/guardian destructor slots
  remain unported. Their vtable addresses are retained so these calls cannot
  silently fall through to an inherited answer. Guardian slot 36 is likewise
  retained as its own unbound function.
- Hostile save/load and original-client, other-platform and cross-architecture
  parity are unverified. The existing save spine does not establish them.

Public tests use only authored ELF/LMP fixtures and explicit synthetic service
answers. Private acceptance reports contain counts only.
