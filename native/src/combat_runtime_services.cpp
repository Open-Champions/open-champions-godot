#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
void World::State::tables() {
  constexpr Raw base[] = {
      0x3c2608, 0x117128, 0x116638, 0x11a5f0, 0x3c7e98, 0x3c7ea0, 0x11a418,
      0x11a260, 0x116920, 0x116780, 0x11a3f0, 0x11a448, 0x11a290, 0x116d50,
      0x116bd8, 0x11c298, 0x3c2738, 0x11a868, 0x118410, 0x118418, 0x119328,
      0x119400, 0x10de10, 0x10ded0, 0x10e2c0, 0x10e658, 0x10bb08, 0x3c2688,
      0x10beb8, 0x11a668, 0x118a58, 0x118af8, 0x10bfc8, 0x3c2700, 0x3c2708,
      0x118c50, 0x118c78, 0x3c2730, 0x3c2748};
  for (auto table : {VT_CREATURE, VT_PLAYER, VT_SKELIGHT})
    for (unsigned k = 0; k < 39; ++k) {
      w(table + 8 * k, 2, 0);
      w(table + 8 * k + 4, base[k]);
    }
  for (auto p : std::map<unsigned, Raw>{{0, 0x3c3ab8},
                                        {1, 0x1a1bb8},
                                        {2, 0x1b5330},
                                        {4, 0x1a5030},
                                        {5, 0x1a46c0},
                                        {6, 0x1beaf0},
                                        {15, 0x11e870},
                                        {16, 0x3c3b38},
                                        {20, 0x1b5ea8},
                                        {27, 0x1b5d68},
                                        {29, 0x3c3b08},
                                        {30, 0x3c3b10},
                                        {37, 0x3c3b18},
                                        {39, 0x3c3b40}})
    w(VT_PLAYER + 8 * p.first + 4, p.second);
  for (auto p : std::map<unsigned, Raw>{{0, 0x3cc6dc},
                                        {1, 0x3cc67c},
                                        {4, 0x2a8b84},
                                        {5, 0x2a8a64},
                                        {8, 0x2a90b0},
                                        {9, 0x2a8fdc},
                                        {15, 0x126b58},
                                        {27, 0x2a91b8}})
    w(VT_SKELIGHT + 8 * p.first + 4, p.second);
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
Value World::State::vcall(Address c, unsigned slot, std::vector<Value> args) {
  actor(c);
  note("virtual", {c, slot});
  auto fn = u(u(c + 0x48) + 8 * slot + 4);
  auto ar = [&](unsigned k) { return number(args.at(k)); };
  if (fn == 0x1b5330) {
    player_take_damage(c, ar(0), ar(1), ar(2));
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
  return model.vcall(c, slot, std::move(args));
}
void World::State::install_services() {
  const char *names[] = {"handle_clear",
                         "handle_link",
                         "handle_removeNode",
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
                         "water_test",
                         "obj_getPropStr",
                         "obj_getPropInt",
                         "net_objOrGhost",
                         "net_isLocalSlot",
                         "net_getCamFlag",
                         "player_speedScale",
                         "creature_bossRank",
                         "creature_findSpellByType",
                         "SpellDrowsy_typeinfo",
                         "SpellAncestralCall_typeinfo",
                         "SpellBattleCry_typeinfo",
                         "player_getSkillRank",
                         "SkillLifeTap_getStat0Fraction",
                         "player_giveExp",
                         "world_notifyObjectsNear",
                         "model_getBounds",
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
                         "Creature_hitTargetInArc",
                         "Creature_hitAllEnemiesInArc",
                         "creature_findEnemyInCone",
                         "damage_apply",
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
                         "pad_rumble"};
  for (auto name : names) {
    std::string n = name;
    model.callbacks[n] = [this, n](A::Model &, const std::vector<Value> &a) {
      return service(n, a);
    };
  }
  for (unsigned slot : {2, 6, 12, 20, 26, 27, 28, 29, 30, 37, 39}) {
    auto n = "Creature.v" + std::to_string(slot);
    model.callbacks[n] = [this, slot](A::Model &, const std::vector<Value> &a) {
      std::vector<Value> rest(a.begin() + 1, a.end());
      return vcall(number(a.at(0)), slot, std::move(rest));
    };
  }
  model.external_slot26 = true;
  model.external.insert("Creature.v27");
  model.external.insert("Creature.v28");
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
      note("touch", {c, other});
      vcall(Raw(c), 6, {iv(other)});
      return M::integer(0);
    }
    throw Unsupported("movement virtual slot");
  };
}
Value World::State::service(const std::string &n, const std::vector<Value> &a) {
  auto ar = [&](unsigned k) { return number(a.at(k)); };
  auto v = [&](unsigned k) { return std::get<Vec3>(a.at(k)); };
  if (n == "handle_clear") {
    clear(ar(0));
    return iv(0);
  }
  if (n == "handle_link") {
    link(ar(0), ar(1), ar(2));
    return iv(0);
  }
  if (n == "handle_removeNode") {
    remove(ar(0));
    return iv(0);
  }
  if (n == "iter_begin") {
    if (head(ar(0)).next)
      throw Unsupported("nonempty spell iterator");
    return iv(0);
  }
  if (n == "iter_next" || n == "iter_end")
    return iv(0);
  if (n == "obj_updateGridCell") {
    grid(ar(0));
    return iv(0);
  }
  if (n == "obj_runEveryFrame" || n == "obj_runEvery60") {
    schedule(ar(0), n == "obj_runEveryFrame" ? 1 : 60);
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
  if (n == "terrain_heightBelow") {
    if (!services.height_below)
      throw Unsupported("height service missing");
    return iv(services.height_below(v(1), ar(2)));
  }
  if (n == "water_test") {
    if (!services.water_test)
      throw Unsupported("water query service missing");
    if (services.water_test(v(1), ar(3)))
      throw Unsupported("water");
    return iv(0);
  }
  if (n == "obj_getPropStr")
    return iv(0);
  if (n == "obj_getPropInt")
    return a.at(2);
  if (n == "net_objOrGhost")
    return a.at(0);
  if (n == "net_isLocalSlot" || n == "net_isSlotActive")
    return iv(1);
  if (n == "net_getCamFlag")
    return iv(0);
  if (n == "player_speedScale") {
    Raw s = 0x3f5ca86d;
    if (ar(0) != UINT32_MAX)
      s = mul(s, u(PLAYERS + ar(0) * RECORD_SIZE + 0x4c20));
    s = mul(s, u(0x4f8418));
    return iv(lt(0x3f999999, s) ? 0x3f999999 : s);
  }
  if (n == "creature_bossRank")
    return iv(q(ar(0) + 0xa8) & 1);
  if (n == "creature_findSpellByType") {
    if (target(ar(0) + 0x364))
      throw Unsupported("attached spells");
    return iv(0);
  }
  if (n == "SpellDrowsy_typeinfo")
    return iv(0x3d1234);
  if (n == "SpellAncestralCall_typeinfo")
    return iv(0x3d0001);
  if (n == "SpellBattleCry_typeinfo")
    return iv(0x3d0002);
  if (n == "player_getSkillRank")
    return iv(skill_rank(ar(0), ar(1)));
  if (n == "SkillLifeTap_getStat0Fraction") {
    if (!services.skill_stat)
      throw Unsupported("skill statistic service missing");
    return iv(services.skill_stat(n, ar(0), ar(1)));
  }
  if (n == "player_giveExp") {
    note(n, {ar(0), ar(1)});
    return iv(0);
  }
  if (n == "world_notifyObjectsNear") {
    auto p = v(1);
    auto r = ar(3);
    auto list = box({sub(p[0], r), sub(p[1], r), sub(p[2], r)},
                    {add(p[0], r), add(p[1], r), add(p[2], r)}, 0x80);
    for (auto o : list)
      vcall(o, 12, a);
    return iv(0);
  }
  if (n == "model_getBounds")
    return A::Bounds{};
  if (n == "model_getSamplePointCount")
    return iv(0);
  if (n == "dialog_isOpen") {
    if (!services.dialog_is_open)
      throw Unsupported("dialog state service missing");
    return iv(services.dialog_is_open());
  }
  if (n == "game_isFrozen" || n == "task_isRunning" || n == "res_find" ||
      n == "text_lookup")
    return iv(0);
  if (n == "libc_sprintf")
    return A::CallbackResult::tuple(
        {A::CallbackResult::text("buf"), A::CallbackResult::bytes("")});
  if (n == "anim_start" || n == "anim_startBlendInOut") {
    start_anim(ar(0) - 0x114, ar(1), ar(2), ar(3), a.size() > 4 ? ar(4) : 0);
    return iv(0);
  }
  if (n == "anim_advance")
    return iv(advance_anim(ar(0) - 0x114, ar(4)));
  if (n == "Creature_applyKnockback") {
    knockback(ar(0), ar(1));
    return iv(0);
  }
  if (n == "Creature_deadUpdate") {
    dead_update(ar(0), ar(1), ar(3), ar(4));
    return iv(0);
  }
  if (n == "creature_canSeeObj")
    return iv(can_see(ar(0), ar(1), ar(2), ar(3), ar(4), ar(5), ar(6), ar(7)));
  if (n == "Creature_hitTargetInArc" || n == "Creature_hitAllEnemiesInArc") {
    if (ar(7))
      throw Unsupported("weapon descriptor");
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
  if (n == "damage_apply") {
    if (ar(6))
      throw Unsupported("weapon descriptor");
    return iv(damage(ar(0), ar(1), ar(8), ar(9), ar(2), ar(5)));
  }
  static const std::set<std::string> presentation = {"fx_spawn",
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
                                                     "pad_rumble"};
  if (presentation.count(n)) {
    note("presentation:" + n);
    return iv(0);
  }
  throw Unsupported("combat service: " + n);
}
void World::State::run(Address a) {
  actor_domain(a);
  auto table = u(a + 0x48);
  auto fn = u(table + 36);
  note("run", {a});
  if (fn == 0x2a8b84)
    skelight_run(a);
  else if (fn == 0x1a5030)
    player_run(a);
  else if (fn != 0x3c7e98)
    throw Unsupported("unimplemented actor run table");
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
    else if (model.is_away_from_home(c))
      throw Unsupported("walking home");
    break;
  case 1:
    throw Unsupported("walking/wander");
  case 3:
    if (!target(c + TARGET))
      st(0);
    else if (see(h(c + 0x38), 6000, 0x42400000))
      st(6);
    else if (see(0, 0x7fff, 0x42100000))
      model.turn_to_target(c);
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
    dead_update(c, ev, 1, 0x41a00000);
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
