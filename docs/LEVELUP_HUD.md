# Level-up and live HUD producers

Research pins (round 3): `t3/research-levelup` `8d38a0b`
(`docs/spec/levelup.md`, `tools/difftest_levelup.py`,
`tools/difftest_progression.py`) and `t3/research-hud-inputs` `8f06b2d`
(`docs/spec/hud-live-producers.md`, `docs/issues/claims-audit.md`,
`FINDINGS.md`). Both descend from the round-1 and round-2 pins `98a8a47` and
`fb24f5e`; every line number below is at the round-3 pins. Only rules those
notes mark Observed or Executed are ported. The experience table
and the skill trees are supplied by the caller; no retail word is embedded.
Tests use authored tables (`i * 100`) and planted values only.

## Experience and level steps

`levelup_hud::give_experience` (`native/src/levelup_hud.cpp`) is
`player_giveExp` `0x1d3150`, driven by `Ledger::give_xp`.

- The award is added with 32-bit wrap. `table[80] - 1` is stored when that
  ceiling is below the sum (hud-live-producers.md:97-103, levelup.md:28-31).
- The loop runs while experience is not below `table[level]`; one below the
  word does not level and the exact word does (levelup.md:47-50).
- The skill word `+0xe8` gains 1, 2, 3, 4 or 5 by the level being left; old
  levels 5, 6, 15, 16, 25, 26, 35, 36 and 79 give 1, 2, 2, 3, 3, 4, 4, 5 and 5
  (levelup.md:33-34, 49-53).
- The stat byte `+0xec` gains 3 as a byte; 254 becomes 1 (levelup.md:50-51).
- `player_recalcStats` runs after the level increment. Current health and
  mana become the new maxima only when health is above 0 (levelup.md:40-41,
  60-61; hud-live-producers.md:119-122). In a multi-level award each step's
  refill is on the record (`+0xac` / `+0xc0`) before the next recalc, so that
  recalc's clamps see the refilled currents (levelup.md:31-41).
- Acknowledged level `+0x58` is not written (levelup.md:42, 61-62).
- Each step asks `hud_showMessage` for 360 frames (`0x168`) and force 0. The
  ledger is the owning machine; with `g_localMenuOpen` set the zero force keeps
  the previous message (hud-live-producers.md:131-133, 184-186). The ledger
  records the calls, frames and force (`level_message_*` in the menu service).
  The message text is not known, so nothing draws it yet.
- The writers of `g_localMenuOpen` `0x41b69d` are `menu_openInventory`
  `0x1c15c8`, `shop_openOrStock` and `shop_close`, and `inventory_close`
  `0x1c5318` clears it (claims-audit.md:103, :287; FINDINGS K2 "set at
  0x1c15c8"). `menu_openInventory` on the owner sets it to 1
  (hud-live-producers.md:273). The pause menu `0x44f208` is a separate word
  (native-hud.md:130). In play the host menu (`godot/ui/game_menus.gd`)
  states the flag for those writers only: open when it shows the inventory,
  shop, shop bag, skills, stats or level-up screen (the `menu_openInventory`
  pages and the shop open), closed when it closes one of them. Pause, Save
  index and any other host screen do not write it. Switching from an
  inventory or shop screen straight to one of those has no original path, so
  the flag becomes unknown there (`forget_local_menu`) until the next
  inventory or shop open and close. **Inferred**: which host screen stands
  for which original opener, and the closed state at start before any writer
  ran. The menu service keeps the state across a new ledger. Until something
  states it, the ledger does not decide the store and counts the calls as
  `level_message_undecided`.

A pending level survives `Ledger::save` / `Ledger::load` unapplied: both level
bytes, experience and both point balances come back unchanged
(levelup.md:326-339; `observed_level_vectors` in `native/tests/menus_tests.cpp`).

## Skill screen sender

`levelup_hud::skill_screen` is the `skills_screen` `0x1d4660` sender.
`Ledger::skill_screen` fills the record side (class `+0x50`, points `+0xe8`,
acknowledged `+0x58`, maximum mana `+0xc4`, ranks) and the panel side: the
page is the ledger's own `+0x50`, which must be one of the two executed pages,
0 (the host Skills screen; no latch, a preset word suppresses) or the
level-up page 5 (tools/difftest_levelup.py:240-243, levelup.md:155-159). Any
other page, and a caller page that differs from the ledger's, refuse. The
latch word `0x4f4878` is kept in the ledger between calls. The caller supplies the tree row, cursor and pad. The
Godot `skill_screen(row)` binding requires every row, panel and pad field
(`kind`, `need`, `ident`, `pre_kind`, `pre`, `page`, `resource`, `in_table`,
`cursor`, `edge`, `held`, `panel30`) and refuses a row without one; only the
confirm mask `0x40` is a constant.

- A missing picture resource, a point outside the tree, an edge without the
  confirm mask `0x40`, or the page-5 held-bit-2 latch leave without sending
  (levelup.md:129-160). The notes order none of the three exits against the
  latch store at `0x1d5010`; the one missing-resource probe
  (tools/difftest_levelup.py:244) runs with held 0 and gate 0 and does not
  read the word. So a call where that order would change the stored word
  refuses, for all three exits alike, and the other calls leave the word.
- The row box is inclusive on all four edges: `panel+0 + rowX <= trunc(x) <=
  that + 0x32` and `rowY + 0x9e <= trunc(y) + panel+0x28 <= rowY + 0xd0`
  (levelup.md:133-151, tools/difftest_progression.py:362-391).
  `levelup_hud::cursor_box` is that test on the raw cursor floats
  (`cvt.w.s` truncation, the interpreter's model); `Cursor::Edge` sends like
  `Cursor::Inside`.
- Prerequisite value `-1` (all 64 bits) passes, whatever its kind; value 0
  passes on skill 60 or 61; otherwise a kind-0 prerequisite needs that passive
  mask's rank 1 and a kind-1 prerequisite that skill id's rank 1
  (levelup.md:162-167). Any other kind refuses. The notes give no compare
  width for `-1` or 0, so a `-1` low word with a nonzero high word and a 0 low
  word with a nonzero high word both refuse.
- Only row kinds 0 and 1 have a send (levelup.md:198-204); any other row kind
  refuses, in `levelup_hud::skill_screen` and in the Godot binding.
- The nine executed alternatives are embedded by class, row kind and ident,
  including class 4 ident 95 from skill 18 and class 6 ident 90 from skill 86
  (levelup.md:169-186). Each applies only on the prerequisite chain it was
  executed on: the two passive-`0x40` rows with a passive prerequisite, the
  seven skill rows with a skill prerequisite. A passive-`0x40` row with a
  skill prerequisite is not unlocked by its alternative (the `0x40` compare is
  only on the passive chain, levelup.md:181-184). A skill alternative with a
  passive prerequisite, any alternative after a failed prerequisite-0 check,
  and an alternative ident on a row of the other kind refuse. Class 1 ident 55
  is not unlocked by passive `0x40`.
- Rank 20 does not send; acknowledged `< need + rank` does not send
  (levelup.md:188-191).
- From there `player_recalcStats` runs even when the point word (`<= 0`) or the
  float compare refuses (levelup.md:193-195, 214-218).
- A passive row sends `(slot, 0, low 32 bits of the mask)`. A skill row sends
  `(slot, 1, id)` only when the planted vtable+0x28 float is `<=` maximum mana
  (`c.ole.s`; levelup.md:198-203).

The ledger runs the screen's recalc, then applies the existing message-7
handler `inventory_onSkillPoint`, which does not recalculate
(levelup.md:220-232). Without the executable class formulas the recalc
refuses. `Ledger::spend_skill` stays refused: the handler is not the sender.

## HUD

These producers already existed and now follow the notes:

- Bars: `ui_layout::barClipRight` is `trunc(S + current / maximum * 99)`,
  `S = origin + 15`, offline slot 0 `x = 27`. No clamp. A zero maximum
  saturates (40/0 gives `0x7fffffff`, -5/0 gives `0x80000000`)
  (hud-live-producers.md:51-66). The Godot HUD view still omits a bar whose
  maximum is not above 0 or whose current is negative: the play scene uses a
  zero maximum to mean that the value is not available (for example play mana),
  so drawing it would paint a value the scene does not have. The play
  observation carries no flag that tells a real zero maximum from a missing
  one, so in play a real zero maximum is not drawn and never saturates; the
  saturation is reached only from native callers.
- Green bar: `hudBaseLayers` draws it from acknowledged level only, from
  `T[ack-1]..T[ack]`, with both differences wrapped as 32-bit integers and
  converted with the core `from_int` (`cvt.s.w`), not the host cast
  (hud-live-producers.md:125-129). The Godot HUD view draws it only when
  `experience_next > experience_base`, so in play the equal-threshold
  saturation and wrapped (negative) spans are not drawn either.
- Regen: `Tick::regen_health` / `Tick::regen_mana` (`native/src/player.cpp`)
  are `0x1b3b90` / `0x1b3900`: `maximum * 0.0001`, the passive, the object
  byte, the 17 equipment modifiers, the cap, class 0 without natural mana, and
  the pending drain (hud-live-producers.md:71-93). Unchanged; cites added.
- Message: `strings::Hud::show_message` / `tick_message` and
  `hud_dynamic::messageFrame`. The alpha step now reads the timer as a signed
  halfword like the tick does (hud-live-producers.md:191-203).

`levelup_hud::reveal_world` is the automap reveal `0x16a4b0` for any world
position: `cell = origin + world * 0.25 * 0x3d088888`, radius-8 disk, round
half away from zero of `(1 - r/8) * 254`, high nibble only, greater nibble wins,
bits 0..11 kept, loop bounded by the loaded size (hud-live-producers.md:156-173,
`tools/ref_hud_inputs.py:77-132`). It is a native producer; the play view does
not draw an automap yet.

## Still refused

- Per-skill virtual bodies: the client has no port of the slot-5 mana-cost
  bodies, so a skill row needs a planted float (levelup.md:198-210). The
  round-3 note executes them for the 16 skills of rows 1–2
  (levelup.md:257-276); porting them is not part of this branch.
- Cursor-box points that no caller can place yet: the play skills page does
  not read the class trees (gap `skill-confirm-input`), so nothing in play
  calls `cursor_box` or the sender.
- `save_load` header, camera, quest and memory-card steps: the round-3 note
  executes the steps without the card (levelup.md:342-398); not ported here.
  Memory-card I/O stays unknown there.
- VU1 tiles `0x403e20`: the round-3 note gives the data contract
  (hud-live-producers.md:409-453); the `world+0x6c` atlas texels stay Unknown
  (hud-live-producers.md:513-514). Not ported here.
- Scripted map icons: the `vars_get` / `script_runByName` gates are executed
  in hud-live-producers.md:358-408; not ported here.
- Ammunition and thrown notices: starters executed in
  hud-live-producers.md:337-357; not ported here.
- Other `hud_showMessage` callers: the ordinary-mission ones are executed in
  hud-live-producers.md:297-336; the rest keep an unexecuted condition
  (hud-live-producers.md:516-517). Not ported here.
- Acknowledged level `+0x58`: raised in play only through inventory message 9
  (hud-live-producers.md:228-296, levelup.md:400-451). The client keeps the
  existing inventory message 9 path (levelup.md:42-44).
- Level bytes outside 0..80 and a negative bar maximum: not executed.
