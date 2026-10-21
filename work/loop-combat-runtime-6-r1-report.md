STATUS: DONE

# combat-runtime-6, round 1 (I took this over after the Cursor Grok run stopped)

- **Branch:** `codex/combat-runtime-6`, from `codex/combat-runtime-5` `99f22ef`.
- **Research pin:** `t3/research-combat` `faa7033` (`tools/ref_combat_round6.py`, `docs/spec/combat-round6.md`, `docs/combat-runtime.md` §19).
- **Scope:** local commits only. Nothing was pushed, and nothing was written in `~/champions-re`.

## What Grok left, and what happened to it

Grok left 10 uncommitted files and no commits. I checked every value against the round-6 research:

- **Correct as written.** The eleven bodies were mostly a faithful transcription of `ref_combat_round6.py`. The constants and immediates match the reference. So do:
  - the `isNearView` constants (`ref_creature`);
  - `Skill_getStatFor` `0x3c87f0` and `Skill_getStatForHandle` `0x3c8864` (`ref_skills_a` slots 12 and 13);
  - the `DiseaseCloud_ctor` split of the round-3 body.

  I kept these.
- **Invented, now removed.** `plant_spell` wrote the base Spell destructor `0x3cf0c4` into slot 1 of tables `0x51ae58` and `0x51ae28`, and the runs into slot 2. The research does not read those tables, and §8 says that every spell destructor except two stays unmodelled. Nothing is planted in those tables now.
- **Ends now refuse.** The previous code called a generic `destroy()` for the gas cloud, the corpse and the blob. That falls back to the base GameObject destructor, which is a guess. Every "slot 1 with 3" in these bodies is now recorded (`delete_self` / `delete_spell`). It then refuses unless the slot holds a destructor that the runtime already models.
- **Missing position now refuses.** A spell run without its position used (0, 0, 0). It now refuses.
- **`res_find`.** It used an empty entry. It now takes the address pair (`resource_at`), as the other bodies do. The multi-fire end keeps an empty entry, because its printed name comes from an executable format string that is not in this repository. The note records (format, number).
- **Notes.** Every particle and sound was noted twice. Also, the damage note reused the damage body's own `damage_apply` event name, which `combat_play` counts. The note is now `round6_damage`.
- **Sample counts.** Arena and horn omen refuse a negative sample count. Arena refuses a count of 0 after the first draw, which is where the reference raises.
- **Not done by Grok:** no tests, no vectors and no docs. I added all three.

## Added

- **The eleven executed bodies:**
  - `poisonGasSpell_run`;
  - `DiseaseCorpse_run`;
  - `MultiFireProjectile_onHit` and `_end`;
  - `SmokeGen_run`;
  - `Pustule_run`;
  - `SpellArena_run`;
  - `SpellHornOmenEffect_ctor` and `_run`;
  - `SpellPlanarSteelEffect_ctor` and `_run`.

  Object runs dispatch by function through slot 4, and the hit through `full_vcall`.
- **The round-5 volley.** `MultiFireProjectile_run` still records the reason it passes, and then runs the executed end body. The round-5 vectors still pass.
- **New services:**
  - `height_at_point` (`terrain_heightAtPoint`);
  - `particle_group` (`particleGroup_create`).
- **Fixtures, labelled as fixtures:**
  - `World::spell_block` is 0x80 bytes, the difftest's size. The built-ins that allocate these spells were not read.
  - `World::spawn_effect_fixture` builds a pool block with a fixture table, where slot 4 is the run and every other slot is 0.
- **Vectors.** `tools/generate_combat_runtime_6_vectors.py` checks the pinned file hashes and runs the reference itself to produce `combat_runtime_6.txt.gz`: 46 cases.
  - The native replay first checks the enemy relation and the box order of each fixture.
  - It then compares particles, damage arguments, the box, cloud constructor arguments, stored words, sounds, effects, the printed name, the trail, glows, sample-point arguments, the shared seed and each refusal: 6,830 checks.
  - Mutating a constant or a branch makes it fail. I tried four mutations, and all four were caught.
- **Docs.** A round-6 section in `COMBAT_RUNTIME.md` and a line in `ROADMAP.md`.

## FULL_TRACE

The hash is unchanged: `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, 36,402 bytes. No object or spell from round 6 exists in the live Full fight, and no volley does either. Code that the fight did reach previously would have refused at the getStatFor slots, and it did not refuse. So the trace stays the same.

## Still refused or unverified

- **Not in the research, so they refuse:**
  - the destructors of these classes. In practice the gas cloud at its end frame, every corpse after its walk, the blob at its limits, and both spells at their end frames all refuse;
  - the spell table words;
  - `SmokeGen_stepParticles` (a recorded call);
  - the built-ins that create these spells. Creature slot 17 does not reach these runs.
- **Still refused from earlier rounds:**
  - `StaticFire_run`;
  - deflection;
  - the bow release in the tick;
  - further death states;
  - Skeleton and AntQueen past state 1;
  - Soul's thrown hits;
  - a nonzero silent passed to block sparks;
  - haze kind 3.
- **Vector answers are chosen, not measured.** The vectors set the reference's call answers (box contents, enemy test, statistics, ground, sample points) to what the native fixture's own bodies give. Damage internals are not compared, because the reference leaves `damage_apply` as a call.
- **Private disc:** 994 files, counted read-only. No acceptance measurement applies to this job.

## Tests

- **arm64 Release:** `ctest -j 3`, **200/200**. Another agent's load killed the first ctest process at 124/200, all passing. I reran the remaining 76 tests with `-I`, and all passed: 77 of 77, because test 136 (create-fixtures) ran in both.
- **x86_64:** `build/native-x64`, `CMAKE_OSX_ARCHITECTURES=x86_64`. `ctest -j 3`, **200/200**, 1,280 s.
- **Targeted while iterating:**
  - `combat-runtime-5`: 2 of 2;
  - the 29 combat, arena-monsters, monsters-1 and fx-exact tests: 29 of 29;
  - `combat-runtime-6-host`: 15 checks;
  - `combat-runtime-6-exact`: 6,830 checks.
- **Godot:** `work/combat-runtime-6/run-godot.py`, the 43-command matrix, **43/43** after one rerun.
  - The archive, level-texture and creature-anim extensions were built from this branch against godot-cpp `e83fd09`.
  - `combined-full-combat` failed twice while the x86_64 ctest and other worktrees loaded the machine. Each time it was the known "stale clip clears cosmetic owner" archive and animation check, not a combat check.
  - Rerun alone after the load dropped: 2,616 checks, 0 failures.
  - wave12: 40 checks, 0 failures.
- **Vectors reproduce:** `python3 tools/generate_combat_runtime_6_vectors.py` writes the committed file (gzip mtime 0).

## Commits

- `bda2e93` Execute the round-6 skill and spell combat handlers
- `6538faa` Document combat runtime round 6
- (this report)
