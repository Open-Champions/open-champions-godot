#include "combat_runtime_internal.hpp"
#include "champions/core_math.hpp"
namespace champions::simulation::rta::combat_runtime {
namespace {
Value matrix_result(const Mat34 &matrix) {
  std::vector<A::CallbackResult> words;
  for (auto word:matrix) words.push_back(A::CallbackResult::unsigned_integer(word));
  return A::CallbackResult::tuple(std::move(words));
}
// The stack descriptor AntQueen_run states 7, 9 and 10 build
// (combat_ctors_2.cpp poison_descriptor): ("desc", ((0x17, v, v, 0), None,
// None, None)). Every other argument is not one.
std::optional<combat::Descriptor> stack_descriptor(const Value &v) {
  using K = A::CallbackResult::Kind;
  auto p = std::get_if<A::CallbackResult>(&v);
  if (!p || p->kind != K::tuple || p->elements.size() != 2 ||
      p->elements[0].kind != K::text || p->elements[0].data != "desc")
    return std::nullopt;
  combat::Descriptor d(4, std::vector<std::int64_t>{-1, 0, 0, 0});
  const auto &slots = p->elements[1].elements;
  for (std::size_t k = 0; k < slots.size() && k < 4; ++k) {
    if (slots[k].kind != K::tuple)
      continue;
    std::vector<std::int64_t> e;
    for (const auto &x : slots[k].elements)
      e.push_back(x.negative ? -std::int64_t(x.magnitude) : std::int64_t(x.magnitude));
    if (e.size() != 4)
      throw Unsupported("stack descriptor entry shape");
    // The halfwords are stored signed (ref_combat descriptors hold s16 values).
    for (std::size_t f = 1; f < 4; ++f)
      e[f] = s16(Raw(e[f]));
    d[k] = e;
  }
  return d;
}
// WeaponShield slot 17 returns its embedded four-row damage descriptor.
combat::Descriptor holder_descriptor(const World::State &s, Address address) {
  combat::Descriptor d;
  for (unsigned k = 0; k < 4; ++k) {
    std::vector<std::int64_t> row;
    for (unsigned f = 0; f < 4; ++f)
      row.push_back(s16(Raw(s.model.mem.load(address + 0x24 + 8 * k + 2 * f, 2))));
    d.push_back(std::move(row));
  }
  return d;
}
struct DescriptorScope {
  World::State &s;
  const combat::Descriptor *saved;
  DescriptorScope(World::State &s_, const combat::Descriptor *d) : s(s_), saved(s_.hit_descriptor) {
    s.hit_descriptor = d;
  }
  ~DescriptorScope() { s.hit_descriptor = saved; }
};
} // namespace
namespace {
// FullServices.PRESENTATION of the composed runtime (traced, ignored).
const std::set<std::string> FULL_PRESENTATION = {
    "fx_spawn",          "particle_spawn",       "sound_playOnObject",
    "sound_playAtPos",   "decal_add",            "anim_releaseCaches",
    "anim_snapTopToEnd", "anim_jumpTopToEnd",    "anim_syncToSimFrame",
    "creature_stopBossMusic", "fx_burnModel",    "fx_shockModel",
    "player_voice",      "gfx_setColorOffset",   "pad_rumble",
    "light_relink",      "light_unlink",         "light_link2",
    "sound_stop",        "Player_weaponTrails",  "Player_footstep",
    "hud_showMessage",   "fx_disintegrateModel", "light_link",
    "light_dtor",        "trail_addPoint",       "trail_release",
    "mat3_transpose",    "mat3_setIdentity",     "mat3_rotateAxis",
    "mat3_mul",          "Player_blockSparks",   "water_disturb",
    "fx_modelStatusEffects", "model_drawPart", "model_draw"};
const std::set<std::string> ROUND1_PRESENTATION = {
    "fx_spawn",          "particle_spawn",      "sound_playOnObject",
    "sound_playAtPos",   "decal_add",           "anim_releaseCaches",
    "anim_snapTopToEnd", "anim_jumpTopToEnd",   "anim_syncToSimFrame",
    "creature_stopBossMusic", "fx_burnModel",   "fx_shockModel",
    "player_voice",      "gfx_setColorOffset",  "pad_rumble"};
// Virtual-table functions some model has a body for (research
// RangedModel.knows over the merged tables).
bool knows(Raw fn) {
  switch (fn) {
  case 0x152a18: case 0x153314: case 0x15153c: case 0x1ba940: // ranged
  case 0x3c2738: case 0x3c3b38: case 0x3c8120:                // slot 16
  case 0x3c2608: case 0x3c9558: case 0x3c3308: case 0x3cc62c: // type-info
  case 0x3c7bc8: case 0x3c62c0: case 0x3c7e88:
  case 0x27236c: case 0x272f4c: case 0x2a8b84: case 0x2a91b8: // classes
  case 0x2c2440: case 0x2c382c: case 0x2c34e4: case 0x2c3c54:
  case 0x10e658: case 0x10beb8: case 0x3c2708:
  case 0x267a0c:                         // Gnaw_setState (creature-ai)
  case 0x237918: case 0x2a7768: case 0x2aac38: // combat-ctors set-states
  case 0x2ab298:                               // Soul_getModel
  case 0x2378fc: case 0x2378f4: case 0x2aab60: // AntQueen slot 2; slot 3s
  case 0x2a774c: case 0x2aa908: case 0x2a7e04: // refused recording stubs
  case 0x2a7688:
  case 0x3c3c78: case 0x3cc7a0:                // AntQueen, Soul type-info
  case 0x1fcb70:                               // LooseFire_onTouch
  case 0x1b5330: case 0x1beaf0: case 0x1b5ea8: case 0x3c3b18: // combat
  case 0x3c3b40:
  case 0x2da610:                                         // MultiFireProjectile_onHit
  case 0x11a290: case 0x3c7ef8: case 0x3c3b10: case 0x3c3b08: // empty
  case 0x116638: case 0x11a418: case 0x11a260: case 0x11a3f0: // base
  case 0x11a448: case 0x11a868: case 0x119328: case 0x10ded0:
  case 0x10bb08: case 0x3c2688: case 0x118a58: case 0x118af8:
  case 0x118c50: case 0x118c78: case 0x3c2730:
    return true;
  default:
    return false;
  }
}
// Object destructors and default empty bodies of the composed tables.
bool empty_slot(Raw fn) {
  switch (fn) {
  case 0x3c7e88: case 0x3c7e90: case 0x3c7e98: case 0x3c7ea0: case 0x3c7ea8:
  case 0x3c7eb0: case 0x3c7eb8: case 0x3c7ec0: case 0x3c7ec8: case 0x3c7ef0:
  case 0x3c7ef8: case 0x3c7f20: case 0x3c7f28: case 0x3c8120:
    return true;
  default:
    return false;
  }
}
} // namespace
bool World::State::presentation(const std::string &n) const {
  return full() ? FULL_PRESENTATION.count(n) != 0
                : ROUND1_PRESENTATION.count(n) != 0;
}
void World::State::tables() {
  for (auto table : {VT_CREATURE, VT_PLAYER, VT_SKELIGHT})
    write_table(table);
}
void World::State::start_anim(Address a, Raw clip, Raw flags, Raw blend,
                              Raw out) {
  note("anim_start", {a, clip, flags, blend, out});
  if (!services.animation_start)
    throw Unsupported("animation start service missing");
  auto fn = services.animation_start;
  fn(a, clip, flags, blend, out);
}
Raw World::State::advance_anim(Address a, Raw speed) {
  Raw ev = 0;
  if (services.animation_advance) {
    auto fn = services.animation_advance;
    ev = fn(a, speed);
  } else
    throw Unsupported("animation advance service missing");
  for (auto it = queued_events.begin(); it != queued_events.end();) {
    auto p = target(it->first);
    if (!p || p == a) {
      if (p)
        ev |= it->second.second;
      clear(it->first);
      it = queued_events.erase(it);
    } else
      ++it;
  }
  note("anim_advance", {a, speed, ev});
  return ev;
}
bool World::State::run_class_hook(Raw fn, Address self, const std::vector<Value> &args) {
  auto hook = class_hooks.find(fn);
  if (hook == class_hooks.end()) return false;
  std::vector<std::uint64_t> numbers;
  for (const auto &value : args)
    if (auto n = std::get_if<std::int64_t>(&value)) numbers.push_back(std::uint64_t(*n));
  auto body = hook->second;
  body(self, numbers);
  return true;
}
Value World::State::vcall(Address c, unsigned slot, std::vector<Value> args) {
  if (full())
    return full_vcall(c, slot, std::move(args));
  actor(c);
  note("virtual", {c, slot});
  auto fn = u(u(c + 0x48) + 8 * slot + 4);
  auto ar = [&](unsigned k) { return number(args.at(k)); };
  if (fn == 0x1b5330) {
    take_damage_full(c, ar(0), ar(1), ar(2));
    return iv(0);
  }
  if (fn == 0x1b5ea8)
    return iv(enemy(c, ar(0)));
  if (fn == 0x3c3b18)
    return iv(Raw(byte(b(record(c) + 0x4c24))));
  if (fn == 0x3c3b40)
    return iv(skill_rank(u(c + 0x3c8), ar(0)));
  if (fn == 0x1beaf0) {
    if (!(u(c + 12) & 0x20000) && u(c + 0x58c) == 9)
      throw Unsupported("Player_onTouch rush");
    return iv(0);
  }
  if (fn == 0x11a290 || fn == 0x3c3b10 || fn == 0x3c3b08)
    return iv(0);
  if (slot == 26) {
    return iv(movement.turn_toward(c, s32(ar(0)), std::get<Vec3>(args.at(1))));
  }
  if (slot == 28)
    throw Unsupported("other classes death and loot");
  if (slot == 27 && fn == 0x2a91b8) {
    if (ar(0) == 1)
      throw Unsupported("Skelight walking/wander");
    model.skelight_set_state(c, ar(0));
    return iv(0);
  }
  if (slot == 27)
    if (const char *cls = own_set_class(fn)) {
      apply_own_state(c, cls, ar(0));
      return iv(0);
    }
  return model.vcall(c, slot, std::move(args));
}
// FullModel.vcall: projectile destructors and bodies, the merged combat
// bodies, empty bodies, then the creature/arena model.
Value World::State::full_vcall(Address obj, unsigned slot,
                               std::vector<Value> args) {
  auto table = u(obj + 0x48);
  auto fn = u(table + 8 * slot + 4);
  auto self = Raw(obj + Raw(s16(h(table + 8 * slot))));
  auto ar = [&](unsigned k) { return number(args.at(k)); };
  if (slot == 6 && !args.empty())
    note("touch", {self, ar(0)});
  note("virtual", {self, slot});
  if (fn == 0x27142c) {
    model.nightmare_on_hit_terrain(self);
    return iv(0);
  }
  if (fn == 0x271484) {
    std::vector<Raw> touched;
    if (!args.empty()) {
      if (auto objects = std::get_if<A::Objects>(&args[0]))
        touched = objects->values;
      else if (auto contacts = std::get_if<A::Contacts>(&args[0]))
        touched = contacts->values;
    }
    model.nightmare_on_touch_list(self, touched);
    return iv(0);
  }
  if (fn == 0x27155c) {
    model.nightmare_set_state(self, ar(0));
    return iv(0);
  }
  if (fn == 0x272f4c) {
    model.horsemen_set_state(self, ar(0));
    return iv(0);
  }
  if (slot == 1 && (fn == 0x151454 || fn == 0x3c6468)) {
    projectile_destroy(self, ar(0));
    return iv(0);
  }
  if (slot == 1 && fn == DISEASE_CLOUD_DTOR)
    throw Unsupported(DISEASE_CLOUD_DTOR_REFUSAL);
  if (slot == 1 && combat_ctors::is_class_dtor(fn)) {
    note("destroy", {self});
    class_dtor(self, fn, ar(0));
    return iv(0);
  }
  if (slot == 1 && combat_ctors::is_prop_dtor(fn)) {
    note("destroy",{self});
    combat_ctors::prop_dtor(model,fn,self,ar(0));
    return iv(0);
  }
  if (slot == 1 && fn == 0x238750) {
    note("destroy",{self});
    combat_ctors::antwave_dtor(model,self,ar(0));
    return iv(0);
  }
  if (slot == 5 && fn == 0x2387d0) {
    combat_ctors::antwave_draw(model,self);
    return iv(0);
  }
  if (slot == 6 && fn == LOOSEFIRE_TOUCH) {
    loosefire_on_touch(self, ar(0));
    return iv(0);
  }
  if (slot == 6 && fn == 0x157a9c) {
    combat_ctors::gold_on_touch(model,self,ar(0));
    return iv(0);
  }
  if (run_class_hook(fn, self, args))
    return iv(0);
  switch (fn) {
  case 0x2da610:
    multifire_on_hit(self, ar(0));
    return iv(0);
  case 0x152a18:
    missile_on_touch(self, ar(0));
    return iv(0);
  case 0x153314:
    return iv(missile_nearest_sample(self, ar(0)));
  case 0x15153c:
    missile_run(self);
    return iv(0);
  case 0x1ba940:
    thrown_run(self);
    return iv(0);
  case 0x1b5330:
    take_damage_full(self, ar(0), ar(1), ar(2));
    return iv(0);
  case 0x1beaf0:
    if (!(u(self + 12) & 0x20000) && s32m(self + 0x58c) == 9)
      throw Unsupported("Player_onTouch in state 9 (SpellCharge_onContact)");
    return iv(0);
  case 0x1b5ea8:
    return iv(enemy(self, ar(0)));
  case 0x3c3b18:
    return iv(Raw(s8m(rec_of(self) + 0x4c24)));
  case 0x3c3b40:
    return iv(skill_rank(u(self + 0x3c8), ar(0)));
  case 0x3c2738:
    return iv(u(u(self + 0x24) + 12));
  case 0x3c3b38:
    return iv(u(self + 0x544));
  case 0x11a290:
  case 0x3c7ef8:
  case 0x3c3b10:
  case 0x3c3b08:
    return iv(0);
  default:
    break;
  }
  actor(obj);
  if (slot == 26 && fn == 0x10bb08)
    return iv(movement.turn_toward(self, s32(ar(0)), std::get<Vec3>(args.at(1))));
  if (slot == 27 && fn == 0x2a91b8) {
    model.skelight_set_state(self, ar(0));
    return iv(0);
  }
  if (slot == 27)
    if (const char *cls = own_set_class(fn)) {
      apply_own_state(self, cls, ar(0));
      return iv(0);
    }
  // Empty table slots (LightEffect and MissileWeapon take-damage is 0x3c7e88)
  // are known addresses with no body. Returning here is the empty function;
  // falling through would call the external dispatcher again. The round-2
  // corpus still records that re-dispatch as a refusal.
  if (round3 && empty_slot(fn))
    return iv(0);
  arena_stack.push_back({self, slot});
  try {
    auto out = model.vcall(obj, slot, std::move(args));
    arena_stack.pop_back();
    return out;
  } catch (...) {
    arena_stack.pop_back();
    throw;
  }
}
// SpellRuntime.dispatch_virtual: a virtual call-out of any model.
Value World::State::dispatch_virtual(Address self, unsigned slot,
                                     std::vector<Value> args) {
  auto fn = u(u(self + 0x48) + 8 * slot + 4);
  if (fn == 0x27142c || fn == 0x271484 || fn == 0x27155c ||
      fn == 0x272f4c)
    return full_vcall(self, slot, std::move(args));
  if (run_class_hook(fn, self, args))
    return iv(0);
  if (knows(fn)) {
    for (auto &d : arena_stack)
      if (d.first == self && d.second == slot)
        throw Unsupported("virtual slot " + std::to_string(slot) +
                          " has no body (unbounded re-dispatch)");
    return full_vcall(self, slot, std::move(args));
  }
  if (slot == 1 && fn == DISEASE_CLOUD_DTOR)
    throw Unsupported(DISEASE_CLOUD_DTOR_REFUSAL);
  if (slot == 1 && combat_ctors::is_class_dtor(fn)) {
    note("destroy", {self});
    class_dtor(self, fn, number(args.at(0)));
    return iv(0);
  }
  if (slot == 1 && (fn == 0x3cf534 || fn == 0x3c5b90 || fn == 0x3c2940 ||
                    fn == 0x3c6888)) {
    note("destroy", {self});
    effect_dtor(fn, self, number(args.at(0)));
    return iv(0);
  }
  if (empty_slot(fn))
    return iv(0);
  if (skill_body(fn))
    return object_body(self, fn, slot, std::move(args));
  throw Unsupported("virtual slot " + std::to_string(slot) + " reaches " +
                    std::to_string(fn) + ", which has no model");
}
// The call-outs of the AntQueen, Skeleton and Soul constructors and
// set-states (combat_ctors), registered with the first such creature so other
// worlds keep their callback table.
void World::State::install_ctor_services() {
  for (const char *name :
       {"anim_clear", "anim_push", "anim_pushWithWeights", "anim_pushChained",
        "anim_findTrack", "model_buildHideMask", "Creature_init",
        "creature_setDefEntryParams", "anm_setEventSlots", "Creature_setCharm",
        "Holder.v1", "Holder.v2", "Holder.v3", "Holder.v4", "Holder.v11",
        "Skeleton_shatter", "GutChunk_ctor", "PhProp_ctor", "PhProp_ctor2", "Gold_ctor2",
        "MissileWeapon_ctor",
        "model_getSamplePoint", "AntWave_ctor", "StaticFire_ctor",
        "creature_getAttachMatrix0", "model_getAttachMatrix",
        "cxx_throwBadTypeid", "Soul_isDemonNear", "CT_type_id",
        "CT_type_info_equals"}) {
    std::string n = name;
    model.callbacks[n] = [this, n](A::Model &, const std::vector<Value> &a) {
      return service(n, a);
    };
  }
  for (unsigned slot=1;slot<=24;++slot) {
    auto n="Holder.v"+std::to_string(slot);
    model.callbacks[n]=[this,n](A::Model &,const std::vector<Value> &a) {
      return service(n,a);
    };
  }
  model.callbacks["World_gameobject_prepare"]=[this](A::Model &,const std::vector<Value> &a)->Value {
    auto obj=number(a.at(0));
    auto p=std::make_unique<Actor>();
    // The World's own bookkeeping (audit C3-5): the research's
    // gameobject_ctor only zeroes the list heads and links the grid, so
    // Kind::Effect is not an original class, only how the World tracks
    // constructed props, chunks and gold.
    p->id=obj;p->kind=Kind::Effect;
    objects[obj]=std::move(p);
    authority->register_object(*objects.at(obj));
    for (unsigned off:{0u,4u,8u}) head(obj+off);
    return iv(0);
  };
}
void World::State::install_services() {
  const char *names[] = {"handle_clear",
                         "handle_link",
                         "handle_removeNode",
                         "handle_insertNode",
                         "handle_addNode",
                         "iter_begin",
                         "iter_next",
                         "iter_end",
                         "obj_updateGridCell",
                         "obj_runEveryFrame",
                         "obj_runEvery60",
                         "obj_delete",
                         "world_collectObjectsInBox",
                         "terrain_raycast",
                         "world_raycast",
                         "world_slideMove",
                         "terrain_heightBelow",
                         "terrain_height",
                         "water_test",
                         "obj_getPropStr",
                         "obj_getPropInt",
                         "net_objOrGhost",
                         "net_isLocalSlot",
                         "net_getCamFlag",
                         "net_getCamYaw",
                         "player_speedScale",
                         "creature_bossRank",
                         "creature_findSpellByType",
                         "creature_addSpell",
                         "SpellDrowsy_typeinfo",
                         "SpellAncestralCall_typeinfo",
                         "SpellBattleCry_typeinfo",
                         "SpellHolyShield_typeinfo",
                         "SpellResurrect_typeinfo",
                         "SpellHarmTouch_typeinfo",
                         "SpellCharge_typeinfo",
                         "SpellBlessed_typeinfo",
                         "SpellBash_typeinfo",
                         "SpellShieldBash_typeinfo",
                         "player_getSkillRank",
                         "SkillLifeTap_getStat0Fraction",
                         "SkillStrongStance_getStat0",
                         "player_giveExp",
                         "world_notifyObjectsNear",
                         "model_getBounds",
                         "model_getAttachPoint",
                         "model_getAttachWorldMatrix",
                         "isNearView",
                         "fx_modelStatusEffects",
                         "model_drawPart",
                         "model_draw",
                         "model_getMeshJoint",
                         "anim_getJointMatrix",
                         "anim_buildPose",
                         "anim_advanceUnitSpeed",
                         "AnimState_dtor",
                         "sub_202500",
                         "objNames_remove",
                         "rigidbody_init",
                         "quat_fromMat3",
                         "quat_normalize",
                         "rigidbody_updateDerived",
                         "obj_stopRunning",
                         "resref_get",
                         "model_getSamplePointCount",
                         "dialog_isOpen",
                         "game_isFrozen",
                         "task_isRunning",
                         "net_isSlotActive",
                         "libc_sprintf",
                         "res_find",
                         "text_lookup",
                         "anim_start",
                         "anim_startBlendInOut",
                         "anim_advance",
                         "Creature_applyKnockback",
                         "Creature_deadUpdate",
                         "creature_canSeeObj",
                         "creature_canSeeObjEdge",
                         "creature_canSeePoint",
                         "creature_findPlayerInCone",
                         "Creature_hitTargetInArc",
                         "Creature_hitAllEnemiesInArc",
                         "Creature_hitTargetAtReach",
                         "Creature_hitTargetInArcEdge",
                         "Creature_hitTargetOrPlayerInArc",
                         "Creature_knockbackTarget",
                         "Creature_setAttackCooldown",
                         "Creature_setAttackCooldownOn",
                         "Creature_dropLoot",
                         "creature_findEnemyInCone",
                         "creature_moveSimple",
                         "obj_moveWithCollision",
                         "obj_moveAndCollide",
                         "world_collectObjectsInArc",
                         "world_collectInSphere",
                         "world_damageInRadius",
                         "world_findFreeSpot",
                         "damage_apply",
                         "pool_new",
                         "pool_free",
                         "GameObject_ctor",
                         "GameObject_dtor",
                         "LightEffect_ctor",
                         "Creature_setEffectTimer",
                         "obj_addToAlwaysDraw",
                         "path_blockNodes",
                         "StaticFire_typeinfo",
                         "sound_playSample",
                         "trail_create",
                         "obj_updateActive",
                         "Creature.v0",
                         "fx_spawn",
                         "particle_spawn",
                         "sound_playOnObject",
                         "sound_playAtPos",
                         "decal_add",
                         "anim_releaseCaches",
                         "anim_snapTopToEnd",
                         "anim_jumpTopToEnd",
                         "anim_syncToSimFrame",
                         "creature_stopBossMusic",
                         "fx_burnModel",
                         "fx_shockModel",
                         "player_voice",
                         "gfx_setColorOffset",
                         "pad_rumble",
                         "light_link",
                         "light_relink",
                         "light_unlink",
                         "light_dtor",
                         "trail_addPoint",
                         "water_disturb"};
  for (auto name : names) {
    std::string n = name;
    model.callbacks[n] = [this, n](A::Model &, const std::vector<Value> &a) {
      return service(n, a);
    };
  }
  if (full()) {
    // Slot 26 (turning) is the creature-movement port's body.
    model.external_slot26 = true;
    for (unsigned slot = 1; slot < 48; ++slot) {
      auto n = "Creature.v" + std::to_string(slot);
      model.callbacks[n] = [this, slot](A::Model &,
                                        const std::vector<Value> &a) {
        std::vector<Value> rest(a.begin() + 1, a.end());
        return dispatch_virtual(number(a.at(0)), slot, std::move(rest));
      };
    }
    for (unsigned slot = 0; slot < 48; ++slot) {
      model.callbacks["Spell.v" + std::to_string(slot)] =
          [this, slot](A::Model &, const std::vector<Value> &a) {
            std::vector<Value> rest(a.begin() + 1, a.end());
            return spell_virtual(number(a.at(0)), slot, std::move(rest));
          };
      model.callbacks["Skill.v" + std::to_string(slot)] =
          [this, slot](A::Model &, const std::vector<Value> &a) {
            std::vector<Value> rest(a.begin() + 1, a.end());
            return skill_virtual(number(a.at(0)), slot, std::move(rest));
          };
    }
  } else {
    for (unsigned slot : {2, 6, 12, 20, 26, 27, 28, 29, 30, 37, 39}) {
      auto n = "Creature.v" + std::to_string(slot);
      model.callbacks[n] = [this, slot](A::Model &,
                                        const std::vector<Value> &a) {
        std::vector<Value> rest(a.begin() + 1, a.end());
        return vcall(number(a.at(0)), slot, std::move(rest));
      };
    }
    model.external_slot26 = true;
    model.external.insert("Creature.v27");
    model.external.insert("Creature.v28");
  }
  using M = creature_movement::Value;
  movement.callbacks["world_slideMove"] = [this](creature_movement::Model &,
                                                 const std::vector<M> &a) {
    if (!services.slide)
      throw Unsupported("slide service missing");
    auto addr = Raw(a.at(0).scalar);
    auto fn = services.slide;
    auto out =
        fn(vec(addr), a.at(1).vec, Raw(a.at(2).scalar), Raw(a.at(4).scalar));
    vec(addr, out.position);
    return a.at(3).scalar
               ? M::tuple({M::integer(out.hit), M::vector(out.normal)})
               : M::integer(out.hit);
  };
  movement.callbacks["obj_updateGridCell"] = [this](creature_movement::Model &,
                                                    const std::vector<M> &a) {
    grid(Raw(a.at(0).scalar));
    return M::integer(0);
  };
  movement.callbacks["world_collectObjectsInBox"] =
      [this](creature_movement::Model &, const std::vector<M> &a) {
        auto found = box(a.at(0).vec, a.at(1).vec, int(a.at(2).scalar));
        std::vector<M> out;
        for (auto p : found)
          out.push_back(M::integer(p));
        return M::list(out);
      };
  movement.callbacks["terrain_heightBelow"] = [this](creature_movement::Model &,
                                                     const std::vector<M> &a) {
    if (!services.height_below)
      throw Unsupported("height service missing");
    auto fn = services.height_below;
    return M::integer(fn(a.at(1).vec, Raw(a.at(2).scalar)));
  };
  movement.callbacks["water_test"] = [this](creature_movement::Model &,
                                            const std::vector<M> &a) -> M {
    if (!services.water_test)
      throw Unsupported("water query service missing");
    if (services.water_test(a.at(1).vec, Raw(a.at(3).scalar)))
      throw Unsupported("water movement");
    return M::integer(0);
  };
  movement.virtual_dispatch = [this](creature_hp::Address c, std::uint32_t slot,
                                     std::vector<M> a) {
    if (slot == 10 || slot == 11) {
      note("virtual", {c, slot});
      return M::integer(0);
    }
    if (slot == 6) {
      if (a.size() != 1)
        throw Error("touch arguments");
      auto other = Raw(a[0].scalar);
      if (!full())
        note("touch", {c, other});
      vcall(Raw(c), 6, {iv(other)});
      return M::integer(0);
    }
    throw Unsupported("movement virtual slot");
  };
}
Value World::State::service(const std::string &n, const std::vector<Value> &a) {
  auto ar = [&](unsigned k) { return number(a.at(k)); };
  // Effects that are not cosmetic (research t3/research-fx 19f80a6,
  // docs/effects.md 15.2 and 17): fx_spawn id 5 draws the shared seed and
  // builds Fire objects; the ice shatter draws the shared seed. Their
  // simulation side belongs to the FX API (native-fx-2): refused, never
  // treated as presentation.
  if (n == "fx_spawn" && !a.empty() && std::holds_alternative<std::int64_t>(a[0]) &&
      ar(0) == 5)
    throw Unsupported("fx_spawn id 5 (shared-seed Fire objects; effects 17)");
  if (n == "fx_shatterToLooseIce")
    throw Unsupported("fx_shatterToLooseIce (shared-seed ice shatter; effects 17)");
  auto v = [&](unsigned k) { return std::get<Vec3>(a.at(k)); };
  // Call-outs of the AntQueen, Skeleton and Soul constructors and set-states
  // (combat_ctors; research-combat-ctors e9777eb). The animation player's push
  // calls, Creature_init and the class-definition event times are recorded:
  // the World's animation service is the authored fixture that starts clips
  // and keeps its own timeline (as Gnaw_ctor's anim_clear and event times).
  if (n == "anim_clear" || n == "anim_push" || n == "anim_pushWithWeights" ||
      n == "anim_pushChained" || n == "Creature_init" ||
      n == "creature_setDefEntryParams" || n == "anm_setEventSlots") {
    std::vector<std::uint64_t> args;
    for (const auto &x : a)
      if (auto p = std::get_if<std::int64_t>(&x))
        args.push_back(std::uint64_t(*p));
    note(n, std::move(args));
    return iv(0);
  }
  if (n == "anim_findTrack")
    throw Unsupported("anim_findTrack: the World's animation fixture keeps no "
                      "tracks to search (AntQueen_setState 0x14)");
  if (n == "model_buildHideMask") {
    if (!services.hide_mask)
      throw Unsupported("model_buildHideMask service missing");
    return std::int64_t(services.hide_mask(ar(0), ar(1)));
  }
  if (n == "Creature_setCharm") {
    // Executed body: research-combat-ctors-3 soul-run.md section 4.
    auto c = ar(0);
    actor(c);
    note(n, {c, ar(1), ar(2)});
    if (full())
      list_head(c + 0x234);
    combat_ctors::creature_set_charm(model,c,ar(1),ar(2));
    return iv(0);
  }
  if (n == "Soul_isDemonNear")
    return iv(is_demon_near(ar(0)));
  if (n == "CT_type_id")
    return iv(type_of(ar(0)));
  // StaticFire's getter 0x3c7318 and descriptor 0x9083c0 are pinned by
  // research-combat-ctors-3 symbols/runtime.tsv:500. Descriptor identity here
  // is a World adapter; the RTTI equality body was not executed in research.
  if (n == "CT_type_info_equals")
    return iv((ar(0)==0x9083c0 || ar(1)==0x9083c0)
        ? (ar(0)==ar(1)) : same_type(ar(0),ar(1)));
  if (n.rfind("Holder.v",0)==0) {
    auto slot=unsigned(std::stoul(n.substr(8)));
    if (slot>=1&&slot<=24) {
      std::vector<Value> rest(a.begin()+1,a.end());
      return iv(combat_ctors::holder_slot(model,ar(0),slot,rest));
    }
  }
  if (n == "Skeleton_shatter")
    { combat_ctors::skeleton_shatter(model,ar(0),ar(1));return iv(0); }
  // The executed prop and gold table slots are installed when an
  // instance is constructed. Older Full reference cases compare all memory
  // and have none of these objects.
  auto prop_slots = [&](Address table,Raw dtor,Raw run) {
    for (auto pair:{std::pair<unsigned,Raw>{1,dtor},{4,run}}) {
      w(table+8*pair.first,2,0);
      w(table+8*pair.first+4,pair.second);
    }
  };
  if (n == "GutChunk_ctor") {
    if (a.size()!=5 || ar(4)!=ONE)
      throw Unsupported("GutChunk_ctor outside the executed unit-scale chain");
    gutchunk_ctor(ar(0),v(1),ar(2),ar(3));
    return iv(0);
  }
  if (n == "PhProp_ctor") {
    prop_slots(0x511ba8,0x3c2fc8,0x155fcc);
    auto parts=std::uint64_t(std::get<std::int64_t>(a.at(6)));
    combat_ctors::phprop_ctor(model,ar(1),v(2),ar(3),ar(4),ar(5),parts,ar(7),ar(8),ar(0));
    prop_model_pointers[ar(1)] = u(ar(1) + 0x4c);
    return iv(0);
  }
  if (n == "PhProp_ctor2")
    { prop_slots(0x511ba8,0x3c2fc8,0x155fcc);
      combat_ctors::phprop_ctor2(model,ar(1),v(2),ar(3),ar(4),ar(5),ar(6),ar(7),ar(8),
        std::uint64_t(std::get<std::int64_t>(a.at(9))),ar(10),ar(11),ar(0));
      prop_model_pointers[ar(1)] = u(ar(1) + 0x4c);
      return iv(0); }
  if (n == "Gold_ctor2")
    { for (const auto &entry:std::map<unsigned,Raw>{{1,0x3c31d8},{4,0x157b58},
                                                     {6,0x157a9c}}) {
        w(0x511a88+8*entry.first,2,0);
        w(0x511a88+8*entry.first+4,entry.second);
      }
      combat_ctors::gold_ctor2(model,ar(0),v(1),ar(2));return iv(0); }
  if (n == "model_getSamplePoint")
    throw Unsupported("model_getSamplePoint for a Soul's particles (Soul_setState "
                      "0x21, Soul_takeDamage with sample points) is not a World "
                      "service");
  if (n == "AntWave_ctor") {
    // Install the executed table when the wave first exists. Older Full
    // reference cases have no AntWave and compare their entire memory image.
    const std::map<unsigned,Raw> slots{{1,0x238750},{4,0x238938},
                                        {5,0x2387d0},{15,0x11fcd0}};
    write_table(0x50d298,&slots);
    combat_ctors::antwave_ctor(model,ar(0),ar(1),v(2),ar(3),ar(4));
    return iv(0);
  }
  if (n == "StaticFire_ctor") {
    const std::map<unsigned,Raw> slots{{0,0x3c7318},{1,0x2c5770},{4,0x2c595c}};
    write_table(0x50da38,&slots);
    combat_ctors::staticfire_ctor(model,ar(0),v(1),ar(2),ar(3),ar(4),ar(5),ar(6));
    return iv(0);
  }
  if (n == "cxx_throwBadTypeid")
    return iv(0);
  if (n == "handle_clear") {
    if (full())
      list_head(ar(0));
    clear(ar(0) ? ar(0) : (full() ? STACK_HEAD : 0));
    return iv(0);
  }
  if (n == "handle_link") {
    if (full())
      return iv(handle_link(ar(0), ar(1), ar(2)));
    link(ar(0), ar(1), ar(2));
    return iv(0);
  }
  if (n == "handle_removeNode") {
    remove(ar(0));
    return iv(0);
  }
  if (full() && n == "handle_insertNode")
    return iv(handle_insert(ar(0), ar(1), ar(2)));
  if (full() && n == "handle_addNode")
    return iv(handle_add(ar(0), ar(1)));
  if (n == "iter_begin") {
    if (!full()) {
      if (head(ar(0)).next)
        throw Unsupported("nonempty spell iterator");
      return iv(0);
    }
    reserve_iterator();
    iterators.push_back(std::make_unique<Iterator>(list_head(ar(0)), ar(1)));
    auto *it = iterators.back().get();
    spell_iterators.push_back(it);
    return iv(it->done() ? 0 : ptr(it->prev));
  }
  if (n == "iter_next") {
    if (!full())
      return iv(0);
    if (spell_iterators.empty())
      throw Error("iter_next without iter_begin");
    auto *it = spell_iterators.back();
    it->advance();
    return iv(it->done() ? 0 : ptr(it->prev));
  }
  if (n == "iter_end") {
    if (!full())
      return iv(0);
    if (spell_iterators.empty())
      throw Error("iter_end without iter_begin");
    end_iterator(*spell_iterators.back());
    spell_iterators.pop_back();
    return iv(0);
  }
  if (n == "obj_updateGridCell") {
    grid(ar(0));
    return iv(0);
  }
  if (n == "obj_runEveryFrame" || n == "obj_runEvery60") {
    schedule(ar(0), n == "obj_runEveryFrame" ? 1 : 60);
    return iv(0);
  }
  if (n == "obj_addToAlwaysDraw") {
    if (!full()) throw Unsupported("always-draw service outside Full");
    link(0x5db4e8,ar(0),ar(0));
    // research-combat-5 tools/ref_objects.py:add_to_always_draw:
    // the list insertion also marks the object as not grid drawn.
    w(ar(0)+0xc,u(ar(0)+0xc)|2u);
    return iv(0);
  }
  if (n == "path_blockNodes")
    throw Unsupported("path_blockNodes: a path-grid service is not supplied");
  if (n == "obj_stopRunning") {
    auto obj=ar(0);
    if (head(obj+4).next) handles.remove(head(obj+4).next);
    return iv(0);
  }
  if (n == "obj_delete") {
    erase_later(ar(0));
    return iv(0);
  }
  if (n == "world_collectObjectsInBox")
    return A::Objects{box(v(0), v(1), s32(ar(2)))};
  if (n == "terrain_raycast")
    return iv(ray(v(1), v(2), ar(3), ar(4)));
  if (n == "world_raycast") {
    auto t = ray(v(1), v(2), ar(5), ar(7));
    if (ar(4))
      return A::RayHit{t, 0};
    return iv(t);
  }
  if (n == "world_slideMove") {
    if (!services.slide)
      throw Unsupported("slide service missing");
    auto fn = services.slide;
    auto out = fn(vec(ar(0)), v(1), ar(2), ar(4));
    vec(ar(0), out.position);
    if (ar(3))
      return A::Slide{out.hit, out.normal};
    return iv(out.hit);
  }
  // terrain_height 0x183820 (AntQueen_pickFlyDest's probe, and the arena
  // bodies): the Full World's terrain_height service when it has one.
  if (full() && n == "terrain_height" && services.terrain_height)
    return iv(services.terrain_height(v(1), ar(2)));
  if (n == "terrain_heightBelow" || (full() && n == "terrain_height")) {
    if (!services.height_below)
      throw Unsupported("height service missing");
    return iv(services.height_below(v(1), ar(2)));
  }
  if (n == "water_test") {
    if (!services.water_test)
      throw Unsupported("water query service missing");
    if (services.water_test(v(1), ar(3)))
      throw Unsupported("water");
    if (full() && ar(2))
      return A::CallbackResult::tuple(
          {A::CallbackResult::integer(0), A::CallbackResult::integer(0)});
    return iv(0);
  }
  if (n == "obj_getPropStr")
    return iv(0);
  if (n == "obj_getPropInt") {
    if (full()) {
      auto *o = find(ar(0));
      if (!o || o->kind == Kind::Effect)
        return iv(0);
    }
    return a.at(2);
  }
  if (n == "net_objOrGhost")
    return a.at(0);
  if (n == "net_isLocalSlot" || n == "net_isSlotActive")
    return iv(1);
  if (n == "net_getCamFlag")
    return iv(camera_flag(ar(0)));
  if (full() && n == "net_getCamYaw")
    return iv(camera_yaw(ar(0)));
  if (n == "player_speedScale") {
    if (s8m(G_NETWORK))
      throw Unsupported("player_speedScale in a net game");
    Raw s = 0x3f5ca86d;
    if (ar(0) != UINT32_MAX)
      s = mul(s, u(Raw(PLAYERS + ar(0) * RECORD_SIZE + 0x4c20)));
    s = mul(s, u(0x4f8418));
    return iv(lt(0x3f999999, s) ? 0x3f999999 : s);
  }
  if (n == "creature_bossRank")
    return iv(q(ar(0) + 0xa8) & 1);
  if (n == "creature_findSpellByType")
    return iv(find_spell_by_type(ar(0), ar(1)));
  if (full() && n == "creature_addSpell") {
    if (ar(1))
      handle_link(ar(0) + 0x364, ar(1) + 4, ar(1));
    return iv(0);
  }
  static const std::map<std::string, Raw> types = {
      {"SpellDrowsy_typeinfo", 0x3d1234},
      {"SpellAncestralCall_typeinfo", SPELL_ANCESTRAL_CALL_GETTER},
      {"SpellBattleCry_typeinfo", SPELL_BATTLE_CRY_GETTER},
      {"SpellHolyShield_typeinfo", 0x3cf124},
      {"SpellResurrect_typeinfo", 0x3d162c},
      {"SpellHarmTouch_typeinfo", SPELL_HARM_TOUCH_GETTER},
      {"SpellCharge_typeinfo", 0x3cce9c},
      {"SpellBlessed_typeinfo", 0x3ce700},
      {"SpellBash_typeinfo", 0x3ce6b0},
      {"SpellShieldBash_typeinfo", 0x3d0258}};
  if (auto t = types.find(n); t != types.end())
    return iv(t->second);
  if (n == "player_getSkillRank")
    return iv(skill_rank(ar(0), ar(1)));
  if (n == "SkillLifeTap_getStat0Fraction" ||
      (n == "SkillStrongStance_getStat0")) {
    if (!services.skill_stat)
      throw Unsupported("skill statistic service missing");
    return iv(services.skill_stat(n, ar(0), ar(1)));
  }
  if (n == "player_giveExp") {
    note(n, {ar(0), ar(1)});
    exp_awards.push_back({ar(0), ar(1)});
    if (services.give_exp) services.give_exp(ar(0), ar(1));
    return iv(0);
  }
  if (n == "world_notifyObjectsNear") {
    notify_objects_near(ar(0), v(1), ar(2), ar(3));
    return iv(0);
  }
  if (n == "model_getBounds")
    return A::Bounds{};
  if (n == "MissileWeapon_ctor") {
    // Skeleton_run state 8's plain-arrow argument vector, executed in
    // research-combat-ctors-4 6baf4cd, arrow-run.md section 4.1.
    if (!full() || a.size()!=17 || ar(1)!=0 || ar(8)!=0 || ar(9)!=0 ||
        ar(12)!=0 || ar(13)!=0 || ar(15)!=4 || ar(16)!=0)
      throw Unsupported("MissileWeapon_ctor outside the Skeleton bow release");
    Missile arrow;
    arrow.position=v(4);arrow.velocity=v(5);arrow.facing=ar(6);
    arrow.models={ar(2),ar(3)};
    arrow.minimum=ar(10);arrow.maximum=ar(11);arrow.attack=ar(14);
    arrow.damage_flags=4;
    missile_ctor(ar(0),arrow,ar(7));
    return iv(0);
  }
  if (n == "creature_getAttachMatrix0")
    return matrix_result(combat_ctors::creature_get_attach_matrix0(model,ar(0)));
  if (n == "model_getAttachMatrix")
    return matrix_result(combat_ctors::model_get_attach_matrix(model,ar(3),ar(0),ar(1),ar(2)));
  if (n == "model_getAttachWorldMatrix")
    return matrix_result(combat_ctors::model_get_attach_world_matrix(
        model,ar(0),ar(1),v(2),ar(3),ar(4),ar(5)));
  if (n == "anim_buildPose") {
    note(n,{ar(0),ar(1)});
    return iv(0);
  }
  if (n == "anim_advanceUnitSpeed") {
    // The end bit is supplied by the loaded clip at runtime. Synthetic
    // fixtures have no clip and preserve their anim-state bytes.
    note(n,{ar(0),ar(1),ar(2),ar(3)});
    return iv(0);
  }
  if (n == "AnimState_dtor") {
    note(n,{ar(0),ar(1)});
    return iv(0);
  }
  if (n == "sub_202500") {
    note(n,{ar(0),ar(1)});
    return iv(0);
  }
  if (n == "objNames_remove")
    throw Unsupported("objNames_remove: the World keeps no object names");
  if (n == "isNearView")
    return iv(is_near_view(ar(0), ar(2)));
  if (n == "model_getAttachPoint") {
    if (!services.attach_point)
      throw Unsupported("model_getAttachPoint service missing");
    return services.attach_point(ar(4)-0x114,ar(1),v(2),ar(3),ar(5));
  }
  if (n == "model_getMeshJoint") {
    // The joint byte of the loaded mesh record (model-texture.md); the
    // research gives no modelled result, so there is no stand-in.
    if (!services.mesh_joint)
      throw Unsupported("model_getMeshJoint: no loaded mesh record service");
    return iv(services.mesh_joint(ar(0), ar(1)));
  }
  if (n == "anim_getJointMatrix") {
    return matrix_result(combat_ctors::anim_get_joint_matrix(model,ar(0),ar(1)));
  }
  if (n == "rigidbody_init" || n == "quat_fromMat3" || n == "quat_normalize" ||
      n == "rigidbody_updateDerived") {
    note(n,{ar(0)});
    if (n == "rigidbody_init")
      combat_ctors::rigidbody_init(model, ar(0), ar(1), ar(2), ar(3), ar(4), ar(5));
    else if (n == "quat_fromMat3")
      combat_ctors::rigidbody_quat_from_mat3(model, ar(0), ar(1));
    else if (n == "quat_normalize")
      combat_ctors::rigidbody_quat_normalize(model, ar(0));
    else
      combat_ctors::rigidbody_update_derived(model, ar(0));
    return iv(0);
  }
  if (n == "resref_get") {
    // The cached pointer of a resource reference (engine.md); loaded data.
    if (!services.resref)
      throw Unsupported("resref_get: no loaded resource reference service");
    return iv(services.resref(ar(0)));
  }
  if (n == "model_getSamplePointCount") {
    if (!full())
      return iv(0);
    if (!services.sample_point_count)
      throw Unsupported("model sample point service missing");
    return iv(services.sample_point_count(ar(0)));
  }
  if (n == "dialog_isOpen")
    return iv(dialog_open());
  if (n == "game_isFrozen" || n == "task_isRunning" || n == "text_lookup")
    return iv(0);
  if (n == "res_find") {
    if (!full())
      return iv(0);
    if (auto t = std::get_if<std::string>(&a.at(0)))
      return iv(resource_text(*t, std::get<std::string>(a.at(1))));
    return iv(resource_at(ar(0), ar(1)));
  }
  if (n == "libc_sprintf") {
    if (full())
      return iv(0);
    return A::CallbackResult::tuple(
        {A::CallbackResult::text("buf"), A::CallbackResult::bytes("")});
  }
  if (n == "anim_start" || n == "anim_startBlendInOut") {
    start_anim(ar(0) - 0x114, ar(1), ar(2), ar(3), a.size() > 4 ? ar(4) : 0);
    return iv(0);
  }
  if (n == "anim_advance")
    return iv(anim_advance_root(ar(0) - 0x114, ar(4), ar(2)));
  if (n == "Creature_applyKnockback") {
    knockback(ar(0), ar(1));
    return iv(0);
  }
  if (n == "Creature_deadUpdate") {
    dead_update(ar(0), ar(1), ar(3), ar(4),
                std::holds_alternative<Vec3>(a.at(2)) ? v(2) : Vec3{});
    return iv(0);
  }
  if (n == "creature_canSeeObj")
    return iv(can_see(ar(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6), ar(7)));
  if (n == "Creature_hitTargetInArc" || n == "Creature_hitAllEnemiesInArc") {
    std::optional<combat::Descriptor> descriptor;
    if (ar(7)) {
      const auto holder = u(ar(0) + 0x50);
      if (!full() || actor(ar(0)).kind != Kind::Creature || !holder ||
          holder + 0x50 != ar(7) || u(holder) != 0x50cda0)
        throw Unsupported("weapon descriptor outside actor holder");
      descriptor = holder_descriptor(*this, ar(7));
    }
    DescriptorScope scope(*this, descriptor ? &*descriptor : nullptr);
    Arc x;
    x.facing = ar(1);
    x.angle = ar(2);
    x.minimum = ar(3);
    x.maximum = ar(4);
    x.attack = ar(5);
    x.flags = ar(6);
    x.near = ar(8);
    x.far = ar(9);
    return iv(hit_arc(ar(0), x, n == "Creature_hitAllEnemiesInArc"));
  }
  if (n == "creature_findEnemyInCone")
    return iv(cone(ar(0), ar(1), ar(2), ar(3), ar(4)));
  if (n == "creature_inKnockdownAnim") {
    if (!services.in_knockdown_anim)
      throw Unsupported("Nightmare contact needs knockdown animation service");
    return iv(services.in_knockdown_anim(ar(0)) ? 1 : 0);
  }
  if (n == "damage_apply") {
    if (!full()) {
      if (ar(6))
        throw Unsupported("weapon descriptor");
      return iv(damage(ar(0), ar(1), ar(8), ar(9), ar(2), ar(5)));
    }
    if (std::holds_alternative<A::Element>(a.at(6))) {
      auto e = std::get<A::Element>(a.at(6));
      combat::Descriptor d{{0x15, s16(e.low), s16(e.high), 0},
                           {-1, 0, 0, 0},
                           {-1, 0, 0, 0},
                           {-1, 0, 0, 0}};
      return iv(damage_with_descriptor(*this, ar(0), ar(1), ar(2), ar(5), d,
                                       ar(8), ar(9)));
    }
    return iv(damage_full(ar(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6),
                          ar(8), ar(9)));
  }
  if (full()) {
    if (n == "creature_canSeeObjEdge")
      return iv(can_see_edge(ar(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6),
                             ar(7)));
    if (n == "creature_canSeePoint")
      return iv(can_see_point(ar(0), v(1), ar(2), ar(3), ar(4), ar(5)));
    if (n == "creature_findPlayerInCone")
      return iv(find_player_in_cone(ar(0), ar(1), ar(2), ar(3), ar(4)));
    if (n == "Creature_hitTargetAtReach") {
      if (auto d = stack_descriptor(a.at(5))) {
        DescriptorScope scope(*this, &*d);
        return iv(hit_target_at_reach(ar(0), ar(1), ar(2), ar(3), ar(4), 0, ar(6)));
      }
      if (ar(5))
        throw Unsupported("weapon descriptor");
      return iv(hit_target_at_reach(ar(0), ar(1), ar(2), ar(3), ar(4), 0,
                                    ar(6)));
    }
    if (n == "Creature_hitTargetOrPlayerInArc") {
      if (auto d = stack_descriptor(a.at(7))) {
        DescriptorScope scope(*this, &*d);
        return iv(hit_target_or_player(ar(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6), 0,
                                       ar(8), ar(9)));
      }
    }
    if (n == "Creature_hitTargetInArcEdge" ||
        n == "Creature_hitTargetOrPlayerInArc") {
      if (ar(7))
        throw Unsupported("weapon descriptor");
      auto f = n == "Creature_hitTargetInArcEdge"
                   ? &World::State::hit_target_in_arc_edge
                   : &World::State::hit_target_or_player;
      return iv((this->*f)(ar(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6), 0,
                           ar(8), ar(9)));
    }
    if (n == "Creature_knockbackTarget") {
      knockback_target(ar(0), ar(1));
      return iv(0);
    }
    if (n == "Creature_setAttackCooldown") {
      set_cooldown(ar(0), ar(1));
      return iv(0);
    }
    if (n == "Creature_setAttackCooldownOn") {
      set_cooldown_on(ar(0), ar(1), ar(2));
      return iv(0);
    }
    if (n == "Creature_dropLoot") {
      if (u(ar(0) + 0x48) != 0x515730)
        throw Unsupported("Creature_dropLoot: loot generation and type-info "
                          "tests are outside the composed runtime");
      drop_loot(ar(0), v(1), ar(2), ar(3), ar(4), ar(5), ar(6), ar(7),
                ar(8), ar(9));
      return iv(0);
    }
    if (n == "creature_moveSimple")
      return iv(move_with_collision(ar(0), v(1), 0, 0));
    if (n == "obj_moveWithCollision")
      return iv(move_with_collision(ar(0), v(1), ar(2), ar(3)));
    if (n == "obj_moveAndCollide")
      return iv(move_and_collide(ar(0), v(1)));
    if (n == "world_collectObjectsInArc") {
      // Player_castSkill supplies the six-argument form with no filter;
      // projectile constructors also use the explicit seven-argument form.
      if (a.size() == 6)
        return A::Objects{collect_in_arc(v(0), ar(1), ar(2), 0, ar(3), ar(4), ar(5))};
      if (a.size() == 7)
        return A::Objects{collect_in_arc(v(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6))};
      throw Unsupported("world_collectObjectsInArc argument count");
    }
    if (n == "world_collectInSphere") {
      if (a.size() == 4)
        return A::Objects{collect_in_sphere(v(0), ar(2), ar(3))};
      return A::Objects{collect_in_sphere(v(0), STACK_HEAD, ar(2))};
    }
    if (n == "world_damageInRadius")
      return A::Objects{damage_in_radius(ar(0), v(1), ar(2), ar(3), ar(4),
                                         ar(5), ar(6), ar(7))};
    if (n == "world_findFreeSpot") {
      auto spot = find_free_spot(v(0), ar(1), ar(2), ar(3));
      return A::FreeSpot{spot.first, Raw(spot.second)};
    }
    if (n == "pool_new")
      return iv(pool_new(ar(0)));
    if (n == "pool_free") {
      pool_free(ar(0));
      return iv(0);
    }
    if (n == "GameObject_dtor") {
      gameobject_dtor_call(ar(0), ar(1));
      return iv(0);
    }
    if (n == "sound_playSample" || n == "trail_create") {
      note("presentation:" + n);
      return iv(0);
    }
    if (n == "obj_updateActive")
      return iv(obj_update_active(ar(0), ar(1), ar(2)));
    if (n == "Creature.v0") {
      if (!has_typeid_target)
        throw Unsupported("virtual slot 0 without a known object");
      return iv(type_of(typeid_target));
    }
    // Call-outs of the real player tick (TickRuntime / FullRuntime).
    if (n == "Player_processDamage") {
      process_damage_full(ar(0));
      return iv(0);
    }
    if (n == "Player_takeDamage") {
      take_damage_full(ar(0), ar(1), ar(2), ar(3));
      return iv(0);
    }
    if (n == "Player_move") {
      Vec3 ext{};
      if (std::holds_alternative<Vec3>(a.at(3)))
        ext = v(3);
      else if (s32(ar(1)) == 5)
        throw Unsupported("Player_move mode 5 without a vector");
      return iv(player_move(ar(0), ar(1), ar(2), ext));
    }
    if (n == "Player_animStep")
      return iv(player_anim_step(ar(0), ar(1)));
    if (n == "Player_meleeHit") {
      melee_hit_full(ar(0), ar(1), ar(2), ar(3), ar(4));
      return iv(0);
    }
    if (n == "Player_findTarget") {
      player_find_target(ar(0), ar(1));
      return iv(0);
    }
    if (n == "Player_pickAttackAnim") {
      pick_attack_anim(ar(0), ar(1));
      return iv(0);
    }
    if (n == "Player_recalcDamage") {
      recalc_weapon(ar(0), ar(1), ar(2));
      return iv(0);
    }
    if (n == "Player_interact") {
      interact(ar(0), ar(1), ar(2));
      return iv(0);
    }
    if (n == "Player_hitSparks") {
      hit_sparks(ar(0), ar(1));
      return iv(0);
    }
    if (n == "Player_tryBlockMove")
      throw Unsupported("Player_tryBlockMove as a call-out");
    if (n == "anim_findTrackByFlag") {
      auto state = ar(0), mask = ar(1);
      for (int i = 0; i < std::max(s8m(state + 4), 0); ++i)
        if (Raw(s16m(state + 8 + 0x20 * Raw(i) + 0x1c)) & mask)
          return iv(i);
      return iv(Raw(-1000000000));
    }
    if (n == "memcard_isBusy" || n == "hud_countActiveNotices" ||
        n == "player_getPassiveLevel" || n == "pad_buttonValue")
      return iv(0);
    if (n == "item_getModifiedField1a")
      return iv(item_rating(ar(0)));
    if (n == "item_parseDescriptor")
      throw Unsupported("a deflected missile (item_parseDescriptor)");
    if (n == "game_rebuildAlivePlayers") {
      alive();
      return iv(0);
    }
    if (n == "jumpgate_getTravelState") {
      if (!services.jumpgate_travel_state)
        throw Unsupported("jumpgate state service missing");
      return iv(services.jumpgate_travel_state());
    }
    if (n == "net_closeLocalMenus") {
      note("presentation:" + n);
      return iv(0);
    }
    if (n == "AutoProp_typeinfo" || n == "AutoContainer_typeinfo" ||
        n == "Container_typeinfo" || n == "Skeleton_typeinfo" ||
        n == "PushPhysicsProp_typeinfo")
      return iv(static_type(n));
    if (n == "SkillPenetratingArrows_getStats01Fraction") {
      if (ar(1))
        throw Unsupported("Penetrating Arrows statistics at a nonzero rank");
      return A::CallbackResult::tuple(
          {A::CallbackResult::integer(0), A::CallbackResult::integer(0)});
    }
    if (n == "SkillDodge_getStat0Fraction") {
      if (ar(1))
        throw Unsupported("Dodge statistics at a nonzero rank");
      return iv(0);
    }
    // Descriptor address: research-combat-ctors-3 symbols/runtime.tsv:500.
    // The equality rule using it is the World adapter documented above.
    if (n == "StaticFire_typeinfo") return iv(0x9083c0);
    if (n == "LightEffect_ctor") {
      if (a.size()!=16)
        throw Unsupported("LightEffect_ctor requires its twelve executed float arguments");
      std::array<Raw,12> floats{};
      for (unsigned k=0;k<12;++k) floats[k]=ar(4+k);
      combat_ctors::light_effect_ctor(model,ar(0),v(1),ar(2),ar(3),floats);
      return iv(0);
    }
    if (n == "Creature_setEffectTimer") {
      combat_ctors::creature_set_effect_timer(model,ar(0),ar(1),ar(2),ar(3));
      return iv(0);
    }
    if (n == "GameObject_ctor") {
      if (ar(2) || ar(3))
        throw Unsupported("an object with a class definition or properties");
      construct_effect(*this, ar(0), v(1));
      return iv(0);
    }
  }
  if (presentation(n)) {
    note("presentation:" + n);
    return iv(0);
  }
  throw Unsupported("combat service: " + n);
}
void World::State::run(Address a) {
  if (!full()) {
    actor_domain(a);
    auto table = u(a + 0x48);
    auto fn = u(table + 36);
    note("run", {a});
    if (fn == 0x2a8b84)
      skelight_run(a);
    else if (fn == 0x1a5030)
      player_run(a);
    else if (!own_run(a, fn) && fn != 0x3c7e98)
      throw Unsupported("unimplemented actor run table");
    return;
  }
  // FullRuntime._slot4.
  auto fn = u(u(a + 0x48) + 36);
  if (services.external_run) {
    auto external = services.external_run;
    if (external(a)) {
      note("run", {a});
      return;
    }
  }
  // StaticFire_run 0x2c595c still deals damage from a view test this round
  // did not take. DiseaseCloud_run 0x2ce6e0 is the round-3 body.
  if (fn == 0x2c595c || (fn == DISEASE_CLOUD_RUN && !round3))
    throw Unsupported(fn == 0x2c595c
                          ? "a damaging effect class run (StaticFire; effects 17)"
                          : "a damaging effect class run (DiseaseCloud/StaticFire; "
                            "effects 17)");
  if (fn == DISEASE_CLOUD_RUN && round3) {
    note("run", {a});
    diseasecloud_run(a);
    return;
  }
  if (fn == PHANTOM_RUN) {
    note("run", {a});
    phantom_run(a);
    return;
  }
  if (fn == 0x2da2bc) {
    note("run", {a});
    multifire_run(a);
    return;
  }
  // Round 6 object runs. StaticFire 0x2c595c stays refused above.
  if (fn == 0x1fd0f0 || fn == 0x2ce4a4 || fn == 0x31c898 || fn == 0x30bde0) {
    note("run", {a});
    if (fn == 0x1fd0f0)
      poison_gas_run(a);
    else if (fn == 0x2ce4a4)
      disease_corpse_run(a);
    else if (fn == 0x31c898)
      smoke_gen_run(a);
    else
      pustule_run(a);
    return;
  }
  if (fn == 0x15153c || fn == 0x1ba940) {
    note("run", {a});
    if (fn == 0x15153c)
      missile_run(a);
    else
      thrown_run(a);
    return;
  }
  if (fn == 0x1ee050) {
    note("run", {a});
    light_effect_run(a);
    return;
  }
  if (fn == 0x157b58) {
    note("run",{a});
    combat_ctors::gold_run(model,a);
    return;
  }
  if (fn == 0x1edb18 || fn == 0x155fcc) {
    note("run", {a});
    if (fn == 0x1edb18) combat_ctors::gutchunk_run(model, a);
    else combat_ctors::phprop_run(model, a);
    return;
  }
  if (fn == 0x27236c || fn == 0x270bbc) {
    note("run", {a});
    if (fn == 0x27236c)
      model.horsemen_run(a);
    else
      model.nightmare_run(a);
    return;
  }
  if (fn == 0x238938) {
    note("run",{a});
    combat_ctors::antwave_run(model,a);
    return;
  }
  if (fn == FIRE_RUN || fn == LOOSEFIRE_RUN) {
    note("run", {a});
    if (fn == FIRE_RUN)
      fire_run(a);
    else
      loosefire_run(a);
    return;
  }
  if (fn == POISON_GAS_RUN)
    throw Unsupported("poisonGasSpell_run 0x1fd0f0 (combat round 6) is not ported");
  if (skill_body(fn)) {
    note("run", {a});
    object_body(a, fn, 4, {});
    return;
  }
  if (u(a + 12) & 0x20000)
    throw Unsupported("prediction copy runtime");
  note("run", {a});
  if (fn == 0x2a8b84) {
    if (u(a + 0x50))
      throw Unsupported("weapon holder");
    skelight_run(a);
  } else if (fn == 0x1a5030)
    player_tick(a);
  else if (!own_run(a, fn) && fn != 0x3c7e98)
    throw Unsupported("unimplemented actor run table");
  if (fn != 0x1a5030)
    run_queued_casts(a);
}
void World::State::skelight_run(Address c) {
  if (s16(h(c + 0x232)) != 23)
    model.process_damage(c, model.class_model(c));
  if (model.creature_run(c, model.class_model(c)))
    return;
  model.update_active(c);
  if (!(q(c + 0xa8) >> 63))
    for (auto it = queued_events.begin(); it != queued_events.end();) {
      auto a = target(it->first);
      if (!a || a == c) {
        clear(it->first);
        it = queued_events.erase(it);
      } else
        ++it;
    }
  if (u(c + 0x50))
    throw Unsupported("weapon holder");
  auto ev = model.step_anim(c, ONE);
  knockback(c, 0x3f333333);
  model.vcall(c, 23, {iv(0)});
  auto state = s16(h(c + 0x232));
  auto st = [&](Raw n) { vcall(c, 27, {iv(n)}); };
  auto see = [&](Raw face, Raw arc, Raw far) {
    return model.can_see_target_in(c, face, arc, 0, 0, far, 0);
  };
  switch (state) {
  case 0:
    if (number(model.vcall(c, 25, {iv(1)})))
      st(3);
    else if (model.is_away_from_home(c)) {
      if (!full())
        throw Unsupported("walking home");
      st(1);
    }
    break;
  case 1:
    if (!full())
      throw Unsupported("walking/wander");
    if (number(model.vcall(c, 25, {iv(1)})))
      st(3);
    else
      model.move_to_dest(c);
    break;
  case 3:
    if (!target(c + TARGET))
      st(0);
    else if (see(h(c + 0x38), 6000, 0x42400000))
      st(6);
    else if (see(0, 0x7fff, 0x42100000))
      model.turn_to_target(c);
    else if (full())
      model.move_to_target(c);
    else
      throw Unsupported("walking chase");
    break;
  case 5:
    if (see(h(c + 0x38), 6000, 0x42400000))
      st(6);
    else if (model.is_not_rooted(c))
      st(3);
    break;
  case 6: {
    bool down = true;
    if (target(c + TARGET)) {
      if (ev & 1) {
        Arc ar;
        ar.facing = h(c + 0x38);
        ar.minimum = Raw(s16(h(c + 0xd0)));
        ar.maximum = Raw(s16(h(c + 0xd2)));
        ar.attack = Raw(s16(h(c + 0xd4)));
        hit_arc(c, ar, false);
      }
      if (h(c + 0x11a) & 8) {
        model.turn_to_target(c);
        down = !(h(c + 0x11a) & 8);
      }
    } else
      down = !(h(c + 0x11a) & 8);
    if (down)
      w(c + 0xf4, u(c + 0xf4) - 1);
    if (s32(u(c + 0xf4)) <= 0)
      st(3);
    break;
  }
  case 16:
    if (model.is_attack_ready(c))
      st(0);
    else
      throw Unsupported("flee walking");
    break;
  case 18:
    st(3);
    break;
  case 19:
    if (model.is_not_stunned(c))
      st(0);
    break;
  case 20:
    dead_update(c, ev, 1, 0x41a00000, {});
    break;
  case 23:
    if (h(c + 0x11a) & 0x80)
      st(0);
    note("presentation:fx_spawn", {7});
    break;
  default:
    throw Unsupported("Skelight state outside combat domain");
  }
}
} // namespace champions::simulation::rta::combat_runtime
