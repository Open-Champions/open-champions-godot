#pragma once
#include "champions/arena_monsters.hpp"
#include "champions/core_math.hpp"

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
bool skeleton_run_prologue(A::Model &, Address c, Raw *events = nullptr);
void soul_ctor(A::Model &, Address c, Raw type_name, const Vec3 &pos,
               Raw facing, Raw props, Raw owner, const Options & = {});
void soul_set_state(A::Model &, Address c, Raw state);
bool soul_run_prologue(A::Model &, Address c, Raw *events = nullptr);
Raw soul_get_model(A::Model &, Address c);
// The three set-states (slot 27) and Soul_getModel (slot 16) as class bodies
// of the model's virtual calls.
void install(A::Model &);
// str_prefixDiffers 0x159c20 (text, prefix): 0 when the text starts with the
// prefix, letters compared through the C library class table at G_CTYPE.
int str_prefix_differs(A::Model &, Address text, Address prefix);
bool on_airhub(A::Model &);

// ---- Round 2: research t3/research-combat-ctors-2 at 5b92837 ------------------
// docs/spec/combat-ctors-2.md (tools/ref_combat_ctors2.py, executed by
// tools/difftest_combat_ctors2.py) and docs/spec/antqueen-run.md
// (tools/ref_antqueen_run.py, executed by tools/difftest_antqueen_run.py), each
// with 0 mismatches on seeds 20973 and 7. combat_ctors_2.cpp holds the bodies.
inline constexpr Raw SKELETON_IS_ENEMY = 0x2a7e04, SKELETON_TAKE_DAMAGE = 0x2a774c,
                     ANTQUEEN_TAKE_DAMAGE = 0x2378fc, SOUL_TAKE_DAMAGE = 0x2aa908,
                     ANTQUEEN_DTOR = 0x3c3c10, SKELETON_DTOR = 0x3cc5b8,
                     SOUL_DTOR = 0x3cc72c, FIRE_DTOR = 0x3c5050,
                     LOOSEFIRE_DTOR = 0x3c6778, POISON_GAS_DTOR = 0x3c7878,
                     ANTQUEEN_PICK_FLY_DEST = 0x238150;
inline constexpr Address VT_CREATURE = 0x50cc58, VT_GAMEOBJECT = 0x5122a8,
                         VT_FIRE = 0x50f208, VT_LOOSEFIRE = 0x50e2b8,
                         VT_POISON_GAS = 0x50d500, VT_SI_TYPEINFO = 0x51df48,
                         VT_USER_TYPEINFO = 0x51df78, S_NAME_KEY = 0x5321b0;
// A type-info getter (combat-ctors-2.md section 3): its descriptor, the name
// string it stores, and its base getter (0: GameObject's, which has none).
struct TypeInfo {
  Raw getter;
  Address descriptor, name;
  Raw base;
  const char *text;
};
extern const TypeInfo TYPE_INFOS[8];
inline constexpr Raw TI_GAMEOBJECT = 0x3c7e10, TI_CREATURE = 0x3c2608,
                     TI_SKELETON = 0x3cc62c, TI_SOUL = 0x3cc7a0,
                     TI_ANTQUEEN = 0x3c3c78, TI_SKELIGHT = 0x3cc6dc,
                     TI_PLAYER = 0x3c3ab8, TI_DEMON = 0x3ca81c;
const TypeInfo *find_type_info(Raw getter);
// A getter of TYPE_INFOS: the descriptor, built on first use.
Address type_info(A::Model &, Raw getter);
// typeid(*obj): slot 0 of obj's table. A getter of TYPE_INFOS runs here; a
// getter the model has as a class body runs that (not recorded); anything
// else is the call-out "Creature.v0" with no argument, as the research's.
Address type_id(A::Model &, Address obj);
// rtti_typeInfoEquals 0x3b1250: the same descriptor, or equal name strings.
bool type_info_equals(A::Model &, Address a, Address b);
// Skeleton_isEnemy 0x2a7e04 (slot 20) -> 0, 1, 2 or 5.
Raw skeleton_is_enemy(A::Model &, Address c, Raw other);
// Soul_takeDamage 0x2aa908 (slot 2). A null attacker makes the call-out
// cxx_throwBadTypeid and then throws Unsupported (what catches the throw is
// Unknown, combat-ctors-2.md section 8).
void soul_take_damage(A::Model &, Address c, Raw attacker, Raw flags, Raw amount);
// The six inline destructors (slot 1, flags), with Creature_dtor 0x117128 and
// GameObject_dtor 0x14cf90 (research ref_combat_monsters, borrowed unchanged)
// beneath them. Their call-outs: iter_begin, iter_next, iter_end, Spell.v1,
// handle_clear, sub_202500, AnimState_dtor, objNames_remove and pool_free.
bool is_class_dtor(Raw fn);
void class_dtor(A::Model &, Raw fn, Address object, Raw flags);
void creature_dtor(A::Model &, Address c, Raw flags);
void gameobject_dtor(A::Model &, Address o, Raw flags);
// AntQueen_run 0x236f78, every state of the switch.
void antqueen_run(A::Model &, Address c);
// AntQueen_pickFlyDest 0x238150 (queen) -> the position it writes to out.
Vec3 antqueen_pick_fly_dest(A::Model &, Address q);
// AntQueen_fadeBankTracks 0x2385dc and AntQueen_updateBank 0x238448
// (research combat round 5 2980af1; ref_combat_round5.QueenModel).
void antqueen_fade_bank_tracks(A::Model &, Address c);
void antqueen_update_bank(A::Model &, Address c);
// The round-2 class bodies (isEnemy, the three takeDamage slots, the type-info
// getters and the six destructors) as class bodies of the model.
void install_round2(A::Model &);

// Executed round-3 run bodies and helpers: research-combat-ctors-3 9ff6ae0.
void skeleton_run(A::Model &, Address c);
void soul_run(A::Model &, Address c);
// Soul_isDemonNear 0x2ab2b4 as ref_soul_run.py soul_is_demon_near: the +-300 box,
// typeid of every object, no skipping; refuses when a getter has no resolution.
bool soul_is_demon_near(A::Model &, Address c);
void creature_set_charm(A::Model &, Address c, Raw frames, Raw team);
Raw skeleton_has_shield(A::Model &, Address c);
void skeleton_on_nearby_event(A::Model &, Address c, Raw code,
                              const Vec3 &position, Address other);
Raw ally_in_line_of_fire(A::Model &, Address c, Address target, Raw previous,
                         Raw reach, Raw distance);
Raw count_living_nonplayer_in_box(A::Model &, const Vec3 &centre, Raw half_size);
Raw creature_obj_in_arc_range(A::Model &, Address c, Address object, Raw arc,
                              Raw range);
Raw lead_target_point(A::Model &, Address c, Vec3 *out, Raw speed);
void install_round3(A::Model &);
// Weapon1/WeaponShield slots 1–17 at 9ff6ae0; slots 18–24 and Weapon2
// element access at research-combat-ctors-4 6baf4cd.
Raw holder_slot(A::Model &, Address holder, unsigned slot,
                const std::vector<A::Value> &args);
void draw_held_part(A::Model &, Address x, Raw part, Address creature);
void build_element_damage(A::Model &, Address descriptor, Raw kind);
Raw damage_desc_get_element(A::Model &, Address descriptor);
void drop_held_part(A::Model &, Raw part, Address creature, Address definition,
                    Raw flags);
void skeleton_shatter(A::Model &, Address creature, Raw group);
void gutchunk_ctor(A::Model &, Address chunk, const Vec3 &position, Raw kind,
                   Raw bits);
void phprop_ctor(A::Model &, Address prop, const Vec3 &position, Address lump,
                 Address model_name, Address texture_name, std::uint64_t parts,
                 Raw launch, Address item, Raw scale);
void phprop_ctor2(A::Model &, Address prop, const Vec3 &position, Raw facing,
                  Address model, Address anim, Address lump, Address model_name,
                  Address texture_name, std::uint64_t parts, Raw launch,
                  Address item, Raw scale);
void gold_ctor2(A::Model &, Address gold, const Vec3 &position, Raw amount);
void phprop_run(A::Model &, Address prop);
void gutchunk_run(A::Model &, Address chunk);
// research-combat-5 f33d1a3, rigidbody-init.md: executed prop body services.
void rigidbody_init(A::Model &, Address body, Raw mass, Raw factor,
                    Raw sx, Raw sy, Raw sz);
void rigidbody_update_derived(A::Model &, Address body);
void rigidbody_quat_from_mat3(A::Model &, Address out, Address matrix);
void rigidbody_quat_normalize(A::Model &, Address quaternion);
// arrow-hit.md: the cold timer and exploding arrow's complete light record.
void creature_set_effect_timer(A::Model &, Address creature, Raw which,
                               Address source, Raw seconds);
void light_effect_ctor(A::Model &, Address block, const Vec3 &position,
                       Address follow, Raw tag, const std::array<Raw, 12> &floats);
// staticfire-ctor.md: constructor, shared init and ring velocity push. Its
// run/draw/destructor remain refused.
void staticfire_ctor(A::Model &, Address fire, const Vec3 &position, Raw life,
                     Address owner, Raw flag, Raw size, Raw scale);
void staticfire_init_common(A::Model &, Address fire);
void staticfire_push(A::Model &, Address fire, Raw facing);
// minion-place.md: only the executed prefix and held-object placement.
// A state below 0x18 refuses at the unexecuted state dispatch.
bool minion_run_prefix(A::Model &, Address minion);
void gold_run(A::Model &, Address gold);
void gold_on_touch(A::Model &, Address gold, Address toucher);
void hud_add_gold_floater(A::Model &, Raw slot, const Vec3 &position, Raw amount);
void hud_tick_gold(A::Model &);
void hud_flush_gold(A::Model &);
bool is_prop_dtor(Raw fn);
void prop_dtor(A::Model &, Raw fn, Address prop, Raw flags);
A::Value anim_advance_unit_speed(A::Model &, Address state, Address sound,
                                 Address root, Address yaw);
Mat34 anim_get_joint_matrix(A::Model &, Address state, Raw joint);
Mat34 model_get_attach_matrix(A::Model &, Raw scale, Address model,
                              Address state, Raw index);
Mat34 model_get_attach_world_matrix(A::Model &, Raw scale, Address model,
                                    const Vec3 &position, Raw facing,
                                    Address state, Raw index);
Mat34 creature_get_attach_matrix0(A::Model &, Address creature);
void antwave_ctor(A::Model &, Address wave, Address target,
                  const Vec3 &position, Raw facing, Raw lifetime);
void antwave_run(A::Model &, Address wave);
void antwave_draw(A::Model &, Address wave);
void antwave_dtor(A::Model &, Address wave, Raw flags);
} // namespace champions::simulation::rta::combat_ctors
