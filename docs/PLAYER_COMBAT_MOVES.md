# Campaign player combat inputs (RTA US v2.00)

The campaign client uses the existing Full `combat_runtime::World` Player,
not a second damage implementation. J feeds the attack pad mask, K the held
block mask, and U the swap mask (hold an established guard first). Host command
bits are 16/32/64; these are authored keyboard bindings, not original pad words.
L (128) requests heavy and emits a refusal, never a substituted light hit.
1/2 (256/512) request the two ability slots; their selected learned records
are unbound and both emit diagnostics without entering the pad.
Only the campaign controller accepts these extra bits; fixture/save command
formats and their historical 0..31 domain are unchanged.

## Evidence and boundaries

Source: research `research-combat-moves` at `0798896`,
`docs/spec/player-combat-moves.md` sections 1–5,
with `research-combat-moves.md` FIX and `research-combat-moves-fix.md` PASS
audits. The corrected class-tree assignment puts Throwing 79 on record class
5, not class 6. Class labels in that table are inferred, not new port inputs.

The observed Player state machine already implements the weapon-specific combo
lengths (7/7/4/9/7/2/7/7), swing/press windows, target selection, class-2 sweep,
dual-wield selection, guard states 8/11, and weapon-row animation selection.
Campaign pad edges now drive that same tick; releases produce no press, and
holding a key does not generate additional edges. Damage remains
`Player_meleeHit` -> recalculation -> `damage_apply` in the shared World.
No combo damage multiplier, universal heavy, stamina bar or free dodge is added.
The existing animation start/advance services resolve the selected original
clip through the player's model definition and weapon row.
The campaign also binds `Player_run`'s name formatter to the selected
executable's literal/`%d` strings so later combo variants resolve their clips.
The Full fixture's null-name formatter remains its explicit default. Unknown
conversions or names beyond the host resource-reader bound refuse. Formatting
uses the end of the existing host string area, after the definition names.
The observations count successful resolved animation starts and advances;
these are native hooks, not proof of rendered skinning. The campaign body
still displays its labelled rest pose.

Campaign observations expose combo step, attack variant/kind and blocking byte
from the real Player. Guard/guard-move refusals name equipment, level-veto or
established-guard prerequisites; the native Player still decides the transition.

Starting equipment remains the labelled unarmed stand-in (weapon animation
class 5). Its combo has two steps. Guard requires an equipped weapon/shield; a bow in slot 2 vetoes
it. No item or shield is synthesized to make K succeed. The observed heavy is
old-inline ability 0x25 with a usable learned record, sufficient mana, ground
contact and track limit. Campaign learned inline records/cost binding are not
established, so L refuses before changing authoritative state. New-style
skill id 37 is not that heavy. Bow/throw, armed combos and skill attacks remain
unavailable in the campaign until real equipment/ammunition/learned records
are bound (selected ability indices `R+0x4a84/+0x4a85`, inline records at
`R+0x4440`, and the campaign Skill objects); the native state machine is not
evidence of a playable equipped campaign character. Unknown live damage tables and normal-route combat
acceptance remain open.
The composed World's bow/throw damage recalculation also still refuses its
Archery/Throwing statistic dependencies; the bow guard rule is checked in the
documented `player_canBlock` helper without claiming a complete ranged tick.

## Checks

`combat-runtime-2-host` exercises the campaign edge mapper through a synthetic
Full World: two-step unarmed combo, no unarmed guard, equipped guard state 8,
and guard-swap state 11 with its observed 47-frame lock, weapon-row animation
hooks, the isolated bow guard helper and the in-World level veto. Existing exact vectors
continue to cover the native move bodies and resolver independently.
`res://tests/campaign_moves_smoke.gd` checks keyboard press/release, OS repeat,
combined guard/swap, pause, picker/focus release and no-player command gating.
All fixture values are authored. The private `loop_playthrough.gd` adds a
counts-only `combat_moves` stage (also available with `--moves-only`): real
keyboard inputs, a held attack without repeat edges, a second combo step,
unarmed guard/move refusals, heavy/ability refusals without mana use or attack
substitution, resume edges, and a newly resolved animation start on the second
combo step. It supplies
no equipment, learned ability, damage or health writes.
