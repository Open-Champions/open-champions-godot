#pragma once
#include "champions/arena_monsters.hpp"

// AntQueen, Skeleton and Soul: the constructors, the set-states 0x237918,
// 0x2a7768 and 0x2aac38, the run functions up to their state switch, and
// Soul_getModel (slot 16). Authored from research t3/research-combat-ctors at
// e9777eb (docs/spec/combat-ctors.md, tools/ref_combat_ctors.py, executed by
// tools/difftest_combat_ctors.py with 0 mismatches). The bodies run on an
// arena_monsters::Model, whose call-outs are its callbacks: the standalone
// replay answers them from the pinned vectors; the combat World answers them
// with its services. Explicitly Return to Arms.
namespace champions::simulation::rta::combat_ctors {
namespace A = arena_monsters;
using Address = A::Address;
using Vec3 = A::Vec3;

inline constexpr Address DEF_ANTQUEEN = 0x4feb10, VT_ANTQUEEN = 0x511078,
                         DEF_SKELETON = 0x5022e0, VT_SKELETON = 0x5175f8,
                         SKEL_COMMON = 0x5023e0, DEF_SOUL = 0x502778,
                         VT_SOUL = 0x517740, G_LEVEL_NAME = 0x41b721,
                         G_LUMP_GENERATION = 0x41bf78, S_AIRHUB = 0x54c2b8,
                         S_BONECHIP = 0x54bf60, P_SOUL = 0x8e4600,
                         G_CTYPE = 0x5628e1, VT_WEAPONSHIELD = 0x50cda0;
inline constexpr Raw ANTQUEEN_RUN = 0x236f78, ANTQUEEN_SET_STATE = 0x237918,
                     SKELETON_RUN = 0x2a6864, SKELETON_SET_STATE = 0x2a7768,
                     SOUL_RUN = 0x2a9870, SOUL_SET_STATE = 0x2aac38,
                     SOUL_GET_MODEL = 0x2ab298;
// The Skeleton type-name prefixes in the order Skeleton_ctor tests them
// (ref_combat_ctors.SKEL_PREFIXES): text address, weapon kind (-1 keeps the
// drawn one), side (-1 keeps 0), and the text the research names there.
struct SkeletonPrefix {
  Address text;
  int kind, side;
  const char *name;
};
extern const SkeletonPrefix SKELETON_PREFIXES[17];
// Documented virtual-table words that differ from the Creature base table
// (symbols/monsters.tsv, runtime.tsv, creatures.tsv; docs/monsters-2.md;
// combat-ctors.md section 7 lists every slot that differs).
struct Slot {
  unsigned slot;
  Raw function;
};
extern const Slot ANTQUEEN_SLOTS[8], SKELETON_SLOTS[11], SOUL_SLOTS[10];

// placed: the combat World has already placed the creature (its authored
// placement stands in for Creature_ctor, as for Skelight and Gnaw), so the
// hit-point draw and Creature_ctor are not made, and the event times of the
// class definition (creature_setDefEntryParams, anm_setEventSlots), which the
// World's animation fixture keeps itself, become the recorded call-outs of
// those names with the entry, the four masks and the four times.
struct Options {
  bool placed = false;
};
void antqueen_ctor(A::Model &, Address c, Raw type_name, const Vec3 &pos,
                   Raw facing, Raw props, const Options & = {});
void antqueen_set_state(A::Model &, Address c, Raw state);
void antqueen_run_prologue(A::Model &, Address c);
void skeleton_ctor(A::Model &, Address c, const Vec3 &pos, Raw facing,
                   Raw props, Raw type_name, Raw model_set,
                   const Options & = {});
void skeleton_set_state(A::Model &, Address c, Raw state);
// false: the creature is inactive and the run returned before the switch.
bool skeleton_run_prologue(A::Model &, Address c);
void soul_ctor(A::Model &, Address c, Raw type_name, const Vec3 &pos,
               Raw facing, Raw props, Raw owner, const Options & = {});
void soul_set_state(A::Model &, Address c, Raw state);
bool soul_run_prologue(A::Model &, Address c);
Raw soul_get_model(A::Model &, Address c);
// The three set-states (slot 27) and Soul_getModel (slot 16) as class bodies
// of the model's virtual calls.
void install(A::Model &);
// str_prefixDiffers 0x159c20 (text, prefix): 0 when the text starts with the
// prefix, letters compared through the C library class table at G_CTYPE.
int str_prefix_differs(A::Model &, Address text, Address prefix);
bool on_airhub(A::Model &);
} // namespace champions::simulation::rta::combat_ctors
