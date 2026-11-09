#pragma once

// The bodies two level-script built-ins reach below their handlers, ported for
// the story-route levels: autoPropSetState (virtual slot 18 of AutoProp and
// AutoPropLP) and killMonster (virtual slot 27 of Gnome). They act on the
// binder's memory image at an object address, with the field offsets the
// original uses. Research: t3/research-level-commands 8efa13d
// docs/level-commands.md (cited below as :line) and its reference model
// tools/ref_level_commands.py (cited as ref:line); audit
// cursor-runs/audits/research-level-commands.md (PASS, LC-1..LC-11 LOW).
//
// Each *_refusal function names the first case on an object that the research
// did not run, or nullopt when the port covers it. The host checks every target
// before the binder stores anything, so a refused command stores nothing.
#include "champions/core_arithmetic.hpp"
#include "champions/script_builtins.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace champions::simulation::rta::level_commands {
using Memory = builtins::Memory;

// Class tables at object +0x48 (ref:57, :127-129, :244-246).
inline constexpr std::uint32_t VT_AUTOPROP = 0x5133c0, VT_AUTOPROP_LP = 0x5130c0, VT_GNOME = 0x5103a8;
inline constexpr std::uint32_t O_FLAGS = 0x0c, O_VTABLE = 0x48;
inline constexpr std::uint32_t OF_NOCOLLIDE = 0x1, OF_DELETE_ON_DEATH = 0x2, OF_DEAD = 0x8, OF_CREATURE = 0x20;
inline constexpr std::uint32_t OF_KILLED = 0x2000, OF_AUTOPROP = 0x10000000;
// AutoProp fields (:138-147, ref:114-118).
inline constexpr std::uint32_t AP_MODEL = 0x90, AP_ANIM = 0x98, AP_TRACK_COUNT = 0x9c, AP_MASK = 0xc8;
inline constexpr std::uint32_t AP_KIND = 0x11c, AP_STATES = 0x120, AP_ANIMS = 0x128, AP_STATE_FLAGS = 0x12c;
inline constexpr std::uint32_t AP_STATE = 0x178, AP_COUNTER = 0x17c, AP_REACHED = 0x180;
inline constexpr std::uint32_t AK_DOOR = 0x1;
// Per-state flags from the Passable / LoopingAnimation / NoMessage / ActionAtEnd
// strings (ref:584), and the per-state "done" bit setState leaves alone (:153).
inline constexpr std::uint32_t SF_PASSABLE = 0x1, SF_LOOPING = 0x2, SF_ACTION_AT_END = 0x8, SF_DONE = 0x10;
// The clip and flag tables hold 10 states before +0x178 (:157-161).
inline constexpr std::int32_t AP_TABLE_STATES = 10;
// Creature fields (:235-276, ref:119-126).
inline constexpr std::uint32_t C_CLASS = 0x24, C_RAND = 0x44, C_FLAGS64 = 0xa8, C_ANIM = 0x114;
inline constexpr std::uint32_t C_HP = 0x228, C_AI_STATE = 0x232, C_TARGET = 0x234;
inline constexpr std::uint32_t C_ROOT = 0x314, C_ATTACK = 0x318, C_T31A = 0x31a, C_STUN = 0x31c;
inline constexpr std::uint32_t CLASS_DEATH_ANIMS = 0xa4;
inline constexpr std::uint64_t DEATH_CFLAGS_KEEP = UINT64_C(0xfffffffff73fffff);
inline constexpr std::uint32_t KILL_FLAGS = OF_KILLED | OF_DEAD | OF_NOCOLLIDE; // 0x2009
inline constexpr std::uint32_t STATE_DEAD = 0x14;
inline constexpr Raw F_0_1 = 0x3dcccccc, F_60 = 0x42700000, F_1_60 = 0x3c888888;
// Globals the death path reads (ref:81-85, :254-257, :273).
inline constexpr std::uint32_t G_SELECTED_OBJ = 0x4039f8, G_ARENA = 0x4f7120, G_ARENA_B = 0x4f711c;
inline constexpr std::uint32_t G_GATE_C = 0x503890, G_GATE_D = 0x503894;

// ---- shared animation and model helpers (executed bodies, ref:265-370) ----
// model_buildHideMask 0x179550 (:143, ref:265-281).
std::uint64_t model_build_hide_mask(const Memory &, std::uint32_t model, std::uint64_t bits);
// anim_push 0x170838 (:145, ref:284-311).
void anim_push(Memory &, std::uint32_t state, std::uint32_t clip, std::uint32_t mode);
// anim_jumpTopToEnd 0x170bc8 (:145, ref:313-331). The caller has refused a top
// track with flag 0x800 (animtrack_decode, not modelled, :62-63).
void anim_jump_top_to_end(Memory &, std::uint32_t state);
// anim_start 0x16cfe0 (:261-262, ref:333-370).
void anim_start(Memory &, std::uint32_t state, std::uint32_t clip, std::uint32_t flags, Raw blend);

// ---- autoPropSetState: AutoProp_setState 0x23d680 (slot 18) ----
// nullopt when the port covers setState(n) on this object. A state at or past
// States is covered (nothing happens, :135-136). Refused, each not run:
//  - a class other than AutoProp / AutoPropLP (its slot 18 was a stub, :169-170);
//  - a negative state (indexes below +0x128, :161), or one past the ten-entry
//    clip table (the hazard at 0x170c0c, :157-162);
//  - an AutoDoor (world_movePiece was a stub, :147, :167-168);
//  - no model block (the mask reads it, :143), or an unaligned clip (the CPU
//    traps on its length load, ref:168-172);
//  - a state with ActionAtEnd whose done bit is clear: setState jumps the top
//    animation to its end and leaves the done bit (:145, :153), so the next run
//    0x23cb54 would run that state's ActionAtEnd actions (0x23cb8c, 0x23cbbc,
//    0x23cc28, :209-210). That consequence is Inferred, not run (audit LC-6b),
//    and the client does not port the AutoProp run.
std::optional<std::string> autoprop_refusal(const Memory &, std::uint32_t obj, std::uint32_t state);
// True when it stored (n < States). Stores, in order (:138-147): state, frames in
// state 0, states-reached bits, mesh mask, track count 0, the state's animation
// pushed and jumped to its end, frames = clip length + 1, then slot 19
// setPassable(state flags & 1): object flag 0x1 set or cleared. No action
// script, no done bit, no generator draw (:153-155). It does not claim to reset
// a saved state on a revisit (:224-226, audit LC-6a).
bool autoprop_set_state(Memory &, std::uint32_t obj, std::uint32_t state);

// ---- killMonster: Gnome_setState 0x269bb4 (slot 27), Creature_onDeath (slot 28) ----
// The arena-only refusal of the state gate 0x10a9c0 (:254-257).
bool arena_gate_refuses(const Memory &);
// GameObject_rand 0x1509d0: the object's own generator at +0x44 (:259-260, ref:373-378).
std::uint32_t game_object_rand(Memory &, std::uint32_t obj);
struct Death {
  // The record's deathScript value (obj_getPropStr, :265-268), if any.
  std::optional<std::string> script;
  bool drop_item = false;           // a dropitem property (:269-270)
  bool boss_music = false;          // creature_bossMusicName answers a name (:274-275)
};
// nullopt when the port covers state 0x14 on this living creature. Refused, each
// not run (stubs in the research, :54, :269-275): a class other than Gnome
// (other classes' slot 27 is Unknown, :321-322); past the gate: no class
// definition block, a dropitem, object flag 0x2 (obj_delete), the arena flag
// (arena_addKill), or a boss-music name (stream_queueStop). The deathscript
// entry itself is the host's to check (its entry gate).
std::optional<std::string> gnome_kill_refusal(const Memory &, std::uint32_t obj, const Death &);
// After the handler's flags 0x2009 and hit points 0 (:235-237). Returns 1 when
// the gate refuses (nothing more happens) and 0 otherwise. Past the gate
// (:257-276): the four timers 0, one own draw picks death animation (draw & 3),
// anim_start(0.1 s), creature flags &= ~0x8c00000, the deathscript run with
// g_scriptSelectedObj = the creature and then 0 (not its old value), and AI
// state 0x14. The deathscript runs as the level's code: which counter it
// changes is not claimed here (audit LC-2).
int gnome_set_state(Memory &, std::uint32_t obj, const Death &,
                    const std::function<void(const std::string &)> &run_death_script);
} // namespace champions::simulation::rta::level_commands
