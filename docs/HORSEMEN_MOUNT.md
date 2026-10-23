# Mounted Horsemen: the Nightmare mount, rider link and lifecycle (Return to Arms)

Authored from research `t3/research-horsemen-mount` at `338d459` (the 1,101
synthetic behavior vectors remain pinned to its earlier `397c35c`): `docs/spec/horsemen-mount.md`, `tools/ref_horsemen_mount.py`,
`tools/difftest_horsemen_mount.py`, `tools/test_ref_horsemen_mount.py`. Contract revision: Return to Arms
SLUS_209.73 US v2.00. Nothing here has run on a console. No research source, decompiler output or game
bytes are copied; the tests use the research rig's blank synthetic memory.

## What is ported

`champions::simulation::rta::arena_monsters::Model` (`native/src/arena_monsters.cpp`) gains, from the
spec's sections 2-5 and 8:

| Native | Original | Spec |
|---|---|---|
| `nightmare_ctor` | `Nightmare_ctor` 0x270968 | 2.3 |
| `nightmare_set_state` | `Nightmare_setState` 0x27155c, with the rider table | 5 |
| `nightmare_run` | `Nightmare_run` 0x270bbc, every state | 3.2, 4 |
| `nightmare_on_hit_terrain`, `nightmare_on_touch_list` | slots 10 / 11 | 4 |
| `nightmare_pick_charge_target` | 0x271a6c | 4 |
| `nightmare_draw`, `nightmare_seat_transform` | 0x270b4c, 0x2719d4 | 7 |
| `angle_to_target` | `Creature_angleToTarget` 0x118840 | 4 |

`horsemen_ctor` builds the real mount when `Model::nightmare_native` is set (off by default: the old
`Nightmare_ctor` call-out is kept for existing callers). Virtual dispatch (`vcall` / tick) resolves the mount's
table functions by identity, as for the other arena classes. The rider side (`Horsemen_ctor`, `_run`,
`_setState` with its mounted branches) was already native.

## Level load (script session)

`Host::build_mounted` (`native/src/script_commands_objects.cpp`) handles the four rider rows
(name tests 0x532cc8, 0x532cd8, 0x532ce8, 0x532cf8 = variants 0-3; constructor 0x271c70, mounted 1) and the
`Nightmare` row (0x532c98, 0x270968):

- A rider record takes **two pool blocks and two shared-seed draws** (the rider's `GameObject_ctor`, then the
  mount's; spec 1.1). Before this change the record counted one block and one unmodelled draw.
- The four **Observed** property lists at `0x500848 + 12 v` each contain `miniboss=1` and a distinct
  name: `horsedisease`, `horsewar`, `horsepestilence`, `horsedeath`. The host builds `RiderDisease`,
  `RiderWar`, `RiderPestilence`, and `RiderDeath` with their named MiniBoss mounts and reciprocal
  handles. The MiniBoss path of `Creature_init` reaches the inherited empty Creature slot 18.
- A placed `Nightmare` passes the record's own list and is built.
- Rows 0x532dc8-0x532df8 call the same constructor but are not documented by the research: they keep the
  previous accounting (one block, one unmodelled draw).

## Refused or host-bounded, on purpose

| Item | Why |
|---|---|
| Run-list order of rider and mount | Unknown in the game (spec 3). This host registers the rider, then mount into successive every-60 buckets; activation can change their later scheduling. The play scene does not tick them. |
| Triggers for mount states 0x19 / 0x1a | no known caller (spec 6): bodies are ported, no trigger invented |
| Whether a mounted rider can carry a stun / root / cooldown | Unknown (spec 6): the stranded-rider outcome is reproduced, not "fixed" |
| Charge-target picker table capacity / maximum player count | Unknown (spec 4): the host chooses `history_entries` as its bound and refuses any slot at or above it (`player scan limit`) |
| `Horsemen_draw` matrix chain (spec 7, rank 5) | not ported; only the mount's draw and the seat transform are |
| Ticking the pair inside the play scene | no scene runtime ticks Horsemen-class objects (on-foot included) |
| Mount pool exhaustion | the on-foot rider is labelled as a stub; no level-load exception escapes |

## Evidence

`tools/generate_horsemen_mount_vectors.py RESEARCH_DIR OUT.tsv OUT.json` replays the memory rig of the
research unit tests through `ref_horsemen_mount.py` (SHA-256 pins in the script) and writes
a local TSV, committed as `native/tests/vectors/horsemen_mount.tsv.gz` (1,101 cases: every set-state value linked and unlinked, filter
cases, constructors, picker, contact slots, draw, seat, angles, every run state against seven answer sets,
the rider's mounted run). The committed gzip and JSON case count are required by CTest. `champions-horsemen-mount-tests` replays them and compares the ordered stores,
call-outs, shared draws and results exactly; it adds the native-only refusals above. The unit tests of the
research are a regression guard there, not evidence about the original; the evidence is its difftest.

`champions-script-commands-tests` covers the factory rows on the authored synthetic executable.
