# PS2 memory-card save import

`native/include/champions/ps2_save.hpp` reads the Champions: Return to Arms
(SLUS-20973, US v2.00) memory-card slot file and options file, and imports one
character into native state. It is not the authored OCNSAVE schema
([NATIVE_SAVE.md](NATIVE_SAVE.md)); that format stays the native session save.

## Evidence

Everything here is a port of the research branch `t3/research-saves` at
`8d43286`: `docs/save-format.md`, `tools/ref_save.py` (the reference reader),
`tools/test_ref_save.py` and `tools/difftest_save.py` (26 of 26 properties
compared with the original functions). `native/tests/ps2_save_contract.json`
pins the SHA-256 of those four files. Every address and constant in the header
appears in those files; the header comments name the original function each
piece follows. The research's own labels (Executed, Observed, Inferred,
Unknown) carry over: the native code implements what is Executed or Observed
and refuses what is Unknown.

## What is read

- **Slot file**: exactly 0x4bc78 bytes, the raw `g_saveBuffer`. Header (level
  name, subname, play time, record size, listed and loaded player counts,
  difficulty, save argument, act), camera bytes, jump gates, minimap mode, four
  player blocks, quest records from +0x13418, the ten-slot level directory and
  the level bodies.
- **Character record**: name, appearance, class, model, levels, experience,
  base stats, gold, skill and stat points, passives, three bags, equipment and
  weapon-set indices, skill ranks, mission flags, medals, evil/decision bits,
  kind, pose, and the whole 0x4c30 bytes.
- **Level bodies**: the framing (world length, decal length, automap). Only
  the documented empty world (`00 00 01 00 08 00 00 00`) and empty decal (eight
  zero bytes) count as known. Any other body is kept raw and marked refused.
- **Options file**: exactly 760 bytes, the raw `g_savedConfig`. Scalar options,
  four button rows, the five voice words (raw), the profile table at +0xf8.

The builders (`build_slot`, `config_set_defaults`, `pack_camera`,
`pack_automap`, the item index conversions) exist so tests can make synthetic
images. They are the same functions the research compared with the executable.

## Checksums

The research found **no file checksum**: `save_build` and `save_load` compute
none, and neither file has a magic. `file_checksum()` throws `Unknown` instead
of inventing one. `player_checksum` is the netplay mix (0x1bdc98). An importer
may supply the mix it expects for the record `save_load` would leave at
`g_players[index]`; `import_into` compares it and refuses before any native
write when it differs. Without a supplied value no checksum is assumed.

## Refusals

Reader rules (from `docs/save-format.md`, stricter than the original load):
wrong file size, record-size field other than 0x4c30, u32 player count outside
0..4, a quest name with no NUL in 0x20 bytes, a quest record that crosses
+0x16010, a level or directory name with no NUL, directory sizes past the end
of the file, a bag count outside 0..64, a character name with no NUL.

Native rules added here, each thrown as `ps2_save::Unknown`:

- The 0x18 bytes at +0x16100 must be zero. What an eleventh level name does
  there was not executed.
- `apply_load` refuses when the word at 0x4f85b4 is 1 (`save_load` would call
  `inventory_shareQuestItems`, which is not modelled), a negative difficulty
  byte (the width of its sign-extended store is not documented), and a
  qualified level name that would reach the pending-level marker at 0x41b720.
- `require_known_world` / `require_known_decal` refuse populated world and
  decal bodies (`world_writeRecords` and live decal fields were not executed).
- `apply_camera` (the camera load is not documented), `voice_global_pairing`,
  `config_open_name` (which file name the 760-byte open uses), and
  `config_field_name` for +0x00, +0x18, +0xc4..+0xd7 and +0xd8..+0xf7.

Norrath and every profile other than `rta-us-v2.00` refuse (`require_profile`).

## Native mapping

`apply_load(Variables&, image)` performs the stores `save_load` 0x221d60 makes,
in its order: header copy to 0x879e38; jump-gate destinations and count; minimap
mode; the u32 player count; for each loaded slot the spawn facing and position,
the record decoded by `save_fixupPlayer` into `g_players` (definition pointers
restored, equipment indices turned back into item addresses, object pointer
cleared); `script_clearDeferred`, `vars_clearAll` and `vars_set` per quest
record (through `Variables::clear_deferred`, `quest_clear_all` and
`quest_set`, which clamps and lower-cases as `vars_set` does); the loaded
flag, act, difficulty and play time; the pending level, its 0xfd marker and the
loaded name; the autosave countdown 99. The camera is listed as not applied.

`import_into(image, index)` parses strictly, checks the profile and an optional
expected mix, builds a fresh `builtins::Memory` and `Variables`, runs
`apply_load`, and returns the character with:

- the **native player record** at `g_players[index]`, which the existing
  script natives (`Variables::player_record`, missions, medals, inventory
  services) read directly;
- the **inventory**: bag words in that record; `inventory(tables, imported)`
  turns them into `items::Item` values through `Item::unpack` and refuses a
  definition the item tables do not hold;
- the **quest state**: the native quest table at 0x86da80 (`live_quests`);
- the `items::Player` view (class, level, base stats, skill ranks) used by
  `items::equip_restriction`.

`apply_config(memory, image)` writes the options image to `g_savedConfig`,
which places the profile table at `PROFILE_VARS` for `Variables::profile_get`.
Which other globals a loaded options image reaches is not documented, so
nothing else is written.

## Godot

`ChampionsPs2SaveImport` (`gdextension/src/ps2_save_import.cpp`) wraps
`import_into` and returns a Dictionary. The play scene's menu has **Import
character from PS2 save…**: it opens a file dialog, refuses a file that is not
exactly 0x4bc78 bytes before reading it, and shows the imported character, the
saved level and what was not applied. A refusal keeps any previous import. The
movement session does not consume the imported record; the import fills the
native script and item state only. `synthetic_slot` builds an authored image
for the smoke test.

## Tests

- `ctest -R ps2-save` (`native/tests/ps2_save_tests.cpp`): every
  `test_ref_save.py` case, the model side of every `difftest_save.py` image,
  29 image vectors and 51 checksum vectors generated from the pinned reference
  (`tools/generate_ps2_save_vectors.py`), the documented executed mixes
  0xbf50531a, 0x407d263f and 0x41331236, the `save_load` stores, and each
  refusal.
- `godot/tests/ps2_save_import_smoke.gd`: the bridge and the play-scene option
  on synthetic images.

All fixtures are synthetic. No save from a card or disc is read or committed.

## Not supported

Populated level bodies, live decals, the camera load, the eleventh directory
name, the options file name and voice-word pairing, `save_packLocalPlayer`, the
load screen's steps other than 4 and 5, and applying the import to the movement
session's player.
