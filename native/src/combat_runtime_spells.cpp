#include "combat_runtime_internal.hpp"
// Round-2 spells in combat (research ref_combat_spells QueryModel and
// SpellRuntime, executed by difftest_combat_spells), composing the executed
// skill bodies of 16 Holy Strike (ref_skills_a), 57 Shock of Fire
// (ref_skills_b) and 95 Wave of Flame (ref_skills_c) with the shared world.
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Raw F_100 = 0x42c80000, F_DEG = 0x43360b60, F_THIRD = 0x3eaa7ef9,
              F_2M16 = 0x37800000, F_02 = 0x3e4ccccc, F_12 = 0x41400000,
              F_36 = 0x42100000, F_60 = 0x42700000, DT = 0x3c888888,
              VT_SPELL_BASE = 0x515ba0, WAVE_GETTER = 0x3d1c4c,
              HOLY_SHIELD_DTOR = 0x3cf0c4, HOLY_SHIELD_RUN = 0x2e8428;
// Object bodies of the composed skill modules.
constexpr Raw HOLYSTRIKE_RUN = 0x2ebc50, HOLYSTRIKE_HIT = 0x2ecae8,
              DISEASEBOLT_RUN = 0x2ca1a8, DISEASEBOLT_HIT = 0x2caec8,
              DISEASEBOLT_CAST = 0x2cb1c0, DISEASEBOLT_CAN_USE = 0x2cb368,
              DISEASEBOLT_COST = 0x2cb33c,
              SPARKS_RUN = 0x28fa3c, SPARKS_TOUCH = 0x2900c8,
              HOLYSTRIKE_CAST = 0x2ecfcc, HOLYSTRIKE_CAN_USE = 0x2ed20c,
              SHOCKOFFIRE_CAST = 0x2ed4e4, WAVE_CAN_USE = 0x316b1c,
              WAVE_CAST = 0x316b24, WAVE_MANA = 0x316cdc, WAVE_RUN = 0x317000,
              WAVE_DTOR = 0x3d1bd8;
// Observed Skill tables of the three skills (slots 0-15).
const std::map<int, std::pair<Address, std::array<Raw, 16>>> SKILL_TABLES = {
    {16,
     {0x51a108,
      {0x3cf4c0, 0x2ed20c, 0x2ecfcc, 0x3c8248, 0x2ed08c, 0x2ed1e4, 0x3c82b8,
       0x3cf510, 0x3c82e4, 0x2ecf74, 0x3c85fc, 0x3c8690, 0x3c87f0, 0x3c8864,
       0x3c88dc, 0x3c88e4}}},
    {57,
     {0x51a190,
      {0x3cf60c, 0x3c8218, 0x2ed4e4, 0x3c8248, 0x2ed6a4, 0x2ed80c, 0x3c82b8,
       0x3cf65c, 0x3c82e4, 0x2ed3f8, 0x3c85fc, 0x3c8690, 0x3c87f0, 0x3c8864,
       0x3c88dc, 0x3c88e4}}},
    {95,
     {0x51c2a8,
      {0x3d1c9c, 0x316b1c, 0x316b24, 0x3c8248, 0x316bdc, 0x316cdc, 0x3c82b8,
       0x3d1cec, 0x3c82e4, 0x316a90, 0x3c85fc, 0x3c8690, 0x3c87f0, 0x3c8864,
       0x3c88dc, 0x3c88e4}}}};
Raw frac(Raw d) { return mul(from_int(d), F_2M16); }
Raw unit(Raw d) {
  auto m = frac(d);
  return sub(add(m, m), ONE);
}
Raw spread_max(Raw base, Raw draw) {
  return add(base, mul(mul(base, frac(draw)), F_02));
}
std::int32_t round_le(Raw v) {
  return to_int(le(0, v) ? add(v, HALF) : sub(v, HALF));
}
std::int32_t round_lt(Raw v) {
  return to_int(lt(v, 0) ? sub(v, HALF) : add(v, HALF));
}
combat::Descriptor descriptor(std::initializer_list<std::array<Raw, 4>> e) {
  combat::Descriptor d(4, std::vector<std::int64_t>{-1, 0, 0, 0});
  unsigned k = 0;
  for (auto &x : e) {
    d[k] = {s16(x[0] & 0xffff), s16(x[1] & 0xffff), s16(x[2] & 0xffff),
            s16(x[3] & 0xffff)};
    ++k;
  }
  return d;
}
} // namespace
int damage_with_descriptor(World::State &, Address, Address, Raw, Raw,
                           const combat::Descriptor &, Raw, Raw);
void construct_effect(World::State &s, Address block, const Vec3 &pos) {
  s.construct(block, pos, World::State::Kind::Effect);
  s.w(block + 0x24, 0);
  s.w(block + 0x28, 1, 0x19);
  s.w(block + 0x29, 1, 0x49);
  s.w(block + 0x2c, 0);
  s.w(block + 0x34, 0);
}
// ---- handle lists of the composition (SpellRuntime) ----------------------
Cell &World::State::list_head(Address addr) {
  return head(addr ? addr : STACK_HEAD);
}
Object *World::State::link_target(Address obj) {
  if (auto *o = find(obj))
    return o;
  return &spell_record(obj);
}
World::State::Actor &World::State::spell_record(Address addr) {
  auto i = spells.find(addr);
  if (i != spells.end())
    return *i->second;
  if (objects.count(addr))
    throw Error("an object is not a spell");
  if (!addr || addr < Pool::BASE)
    throw Error("not a live object or spell");
  pool.class_of(addr);
  auto p = std::make_unique<Actor>();
  p->id = addr;
  p->kind = Kind::Spell;
  auto *rec = p.get();
  spells[addr] = std::move(p);
  head(addr + 4);
  return *rec;
}
Address World::State::handle_link(Address head_addr, Address back, Address obj) {
  auto *target = link_target(obj);
  Cell *back_cell = nullptr;
  // A script-host factory creature keeps its own back-reference heads (the
  // same rule State::link applies, for its three list heads at +0, +4, +8); a
  // combat actor's heads are in `heads`.
  if (authority->is_factory(obj) && back >= obj && back - obj <= 8 && !((back - obj) & 3)) {
    auto external = authority->objects->objects.find(obj);
    if (external != authority->objects->objects.end())
      back_cell = &external->second->heads[(back - obj) / 4];
  }
  if (!back_cell) {
    auto i = heads.find(back);
    if (i != heads.end()) back_cell = i->second;
  }
  if (!back_cell)
    throw Error("handle_link back list is not a head");
  auto *n = handles.link(list_head(head_addr), *back_cell, target);
  return ptr(n);
}
Address World::State::handle_insert(Address head_addr, Address after,
                                    Address obj) {
  Link *node = nullptr;
  if (after) {
    if (after < HandlePool::NODE_BASE || (after - HandlePool::NODE_BASE) % 20)
      throw Error("not a node");
    node = handles.nodes().at((after - HandlePool::NODE_BASE) / 20);
  }
  return ptr(handles.insert(list_head(head_addr), node, link_target(obj)));
}
Address World::State::handle_add(Address head_addr, Address obj) {
  return ptr(handles.add(list_head(head_addr), link_target(obj)));
}
std::vector<Address> World::State::list(Address head_addr) const {
  std::vector<Address> out;
  for (auto node = u(head_addr); node; node = u(node))
    out.push_back(u(node + 12));
  return out;
}
std::vector<Address> World::State::attached(Address creature) const {
  std::vector<Address> out;
  auto i = heads.find(creature + 0x364);
  if (i == heads.end())
    return out;
  for (auto *n = i->second->next; n; n = n->next)
    if (n->obj)
      out.push_back(n->obj->id);
  return out;
}
Raw World::State::find_spell_by_type(Address c, Raw type) {
  if (!full()) {
    if (target(c + 0x364))
      throw Unsupported("attached spells");
    return 0;
  }
  Raw found = 0;
  for (auto spell : attached(c)) {
    auto getter = u(u(spell + 0x14) + 4);
    if (getter == WAVE_GETTER)
      continue;
    if (getter == DISEASE_GETTER) {
      if (type == DISEASE_GETTER)
        found = spell;
      continue;
    }
    // SpellHolyShield_typeinfo. The absorb body is round 4; the lookup is the
    // one Player_takeDamage already makes.
    if (getter == 0x3cf124) {
      if (type == 0x3cf124)
        found = spell;
      continue;
    }
    throw Unsupported("creature_findSpellByType over an unknown spell class");
  }
  return found;
}
// ---- the victim finders (QueryModel, executed) ----------------------------
std::vector<Address> World::State::collect_in_arc(const Vec3 &pos, Raw facing,
                                                  Address out, Address filter,
                                                  Raw radius, Raw arc,
                                                  Raw width) {
  auto found = box({sub(pos[0], radius), sub(pos[1], radius),
                    sub(pos[2], radius)},
                   {add(pos[0], radius), add(pos[1], radius),
                    add(pos[2], radius)},
                   0x200);
  auto f16 = Raw(s16(facing));
  Raw fwd[2] = {cos16(f16), sin16(f16)}, n1[2] = {}, n2[2] = {}, side[2] = {};
  auto dot2 = [&](const Raw *v, const Vec3 &p) {
    return add(mul(v[0], sub(p[0], pos[0])), mul(v[1], sub(p[1], pos[1])));
  };
  bool use_arc = !eq(arc, MINUS_ONE);
  if (use_arc) {
    auto hh = mul(mul(arc, HALF), F_DEG);
    hh = le(0, hh) ? add(hh, HALF) : sub(hh, HALF);
    auto n = s32(Raw(to_int(hh)));
    auto a = Raw(s16(Raw(s16(f16) + n - 0x4000))),
         b = Raw(s16(Raw(s16(f16) - n + 0x4000)));
    n1[0] = cos16(a);
    n1[1] = sin16(a);
    n2[0] = cos16(b);
    n2[1] = sin16(b);
  }
  bool use_width = !eq(width, 0);
  if (use_width) {
    side[0] = cos16(Raw(s16(f16) + 0x4000));
    side[1] = sin16(Raw(s16(f16) + 0x4000));
  }
  for (auto o : found) {
    auto flags = u(o + 12);
    if (!(flags & 0x20) || (flags & 0xd))
      continue;
    if (filter && !number(vcall(filter, 20, {iv(o)})))
      continue;
    auto p = vec(o + 16);
    if (use_arc && (lt(dot2(n1, p), 0) || lt(dot2(n2, p), 0)))
      continue;
    if (use_width && lt(width, fabs(dot2(side, p))))
      continue;
    if (!lt(0, dot2(fwd, p)))
      continue;
    auto dx = sub(pos[0], p[0]), dy = sub(pos[1], p[1]);
    auto d2 = add(mul(dx, dx), mul(dy, dy));
    Address after = 0;
    for (auto node = u(out ? out : STACK_HEAD); node; node = u(node)) {
      auto qobj = u(node + 12);
      auto qx = sub(pos[0], u(qobj + 16)), qy = sub(pos[1], u(qobj + 20));
      if (le(add(mul(qx, qx), mul(qy, qy)), d2)) {
        after = node;
        break;
      }
    }
    handle_insert(out, after, o);
  }
  auto result = list(out ? out : STACK_HEAD);
  note("arc", {result.begin(), result.end()});
  return result;
}
std::vector<Address> World::State::collect_in_sphere(const Vec3 &c,
                                                     Address out, Raw radius) {
  auto found = box({sub(c[0], radius), sub(c[1], radius), sub(c[2], radius)},
                   {add(c[0], radius), add(c[1], radius), add(c[2], radius)},
                   0x200);
  for (auto o : found) {
    if (u(o + 12) & 0xd)
      continue;
    auto p = vec(o + 16);
    auto dx = sub(c[0], p[0]), dy = sub(c[1], p[1]), dz = sub(c[2], p[2]);
    auto d = sqrt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
    if (lt(d, add(radius, u(o + 0x1c))))
      handle_add(out, o);
  }
  auto result = list(out ? out : STACK_HEAD);
  note("sphere", {result.begin(), result.end()});
  return result;
}
std::vector<Address> World::State::damage_in_radius(Address attacker,
                                                    const Vec3 &c, Raw most,
                                                    Raw flags, Raw least,
                                                    Raw skip, Raw inner,
                                                    Raw outer) {
  auto most_f = from_int(s32(most));
  auto found = box({sub(c[0], outer), sub(c[1], outer), sub(c[2], outer)},
                   {add(c[0], outer), add(c[1], outer), add(c[2], outer)},
                   0x80);
  auto span = sub(outer, inner);
  auto k = lt(ONE, span) ? div(most_f, span) : F_100;
  bool is_player = attacker && (u(attacker + 12) & 0x40);
  std::vector<Address> hit;
  for (auto o : found) {
    if (o == attacker)
      continue;
    if (is_player && !number(vcall(attacker, 20, {iv(o)})))
      continue;
    if (u(o + 12) & skip)
      continue;
    auto ax = fabs(sub(u(o + 16), c[0])), ay = fabs(sub(u(o + 20), c[1]));
    auto d = sub(sub(add(ax, ay), mul(fmin(ax, ay), F_THIRD)), u(o + 0x1c));
    if (le(outer, d))
      continue;
    auto amount = mul(sub(outer, d), k);
    auto low = from_int(s32(least));
    if (lt(amount, low))
      amount = low;
    amount = fmin(amount, most_f);
    auto dmg = from_int(s32(Raw(to_int(amount))));
    damage_full(o, attacker, 100000, 0, 0, flags, 0, dmg, dmg);
    hit.push_back(o);
  }
  return hit;
}
// ---- destructors, pool, objects -------------------------------------------
void World::State::effect_dtor(Raw fn, Address self, Raw flags) {
  struct Part {
    Raw table;
    std::vector<std::pair<bool, unsigned>> steps; // (light, offset)
  };
  Address table_off = 0x48;
  std::vector<Part> parts;
  bool gameobject = true;
  switch (fn) {
  case 0x3cf534: // HolyStrike_dtor
    parts = {{VT_HOLYSTRIKE, {{false, 0x58}, {false, 0x54}}},
             {VT_PROJECTILE, {{false, 0x4c}}}};
    break;
  case 0x3c5b90: // Sparks_dtor
    parts = {{VT_SPARKS, {{true, 0x64}}}, {VT_PROJECTILE, {{false, 0x4c}}}};
    break;
  case 0x3c2940: // Projectile_dtor
    parts = {{VT_PROJECTILE, {{false, 0x4c}}}};
    break;
  case 0x3c6888: // LightEffect_dtor
    parts = {{VT_LIGHTEFFECT, {{false, 0x88}, {true, 0x4c}}}};
    break;
  case DISEASE_CLOUD_DTOR:
    throw Unsupported(DISEASE_CLOUD_DTOR_REFUSAL);
  case HOLY_SHIELD_DTOR: // SpellHolyShield_dtor 0x3cf0c4 = the base Spell
    // destructor (skills-35-68 section 54). Its steps are the base tail of the
    // executed SpellWaveOfFlame_dtor (combat-spells 6.1).
    table_off = 0x14;
    parts = {{VT_SPELL_BASE, {{false, 4}}}};
    gameobject = false;
    break;
  case WAVE_DTOR: // SpellWaveOfFlame_dtor
    table_off = 0x14;
    parts = {{VT_SPELL_WAVE, {{false, 0x18}}}, {VT_SPELL_BASE, {{false, 4}}}};
    gameobject = false;
    break;
  default:
    throw Unsupported("destructor without a body");
  }
  for (auto &part : parts) {
    w(self + table_off, part.table);
    for (auto &step : part.steps)
      if (step.first)
        note("presentation:light_dtor");
      else {
        list_head(self + step.second);
        clear(self + step.second);
      }
  }
  if (gameobject)
    gameobject_dtor_call(self, 0);
  if (flags & 1)
    pool_free(self);
}
Address World::State::pool_new(Raw size) {
  auto a = pool.allocate(size);
  note("pool_new", {size, a});
  if (a) {
    auto extent = Pool::CLASSES[pool.class_of(a)][0];
    for (auto i = spells.lower_bound(a); i != spells.end() && i->first < a + extent;)
      i = spells.erase(i);
    forget_heads(a, a + extent);
    for (unsigned k = 0; k < extent; ++k)
      mem.poke(a + k, 1, 0);
  }
  return a;
}
void World::State::pool_free(Address a) {
  auto extent = Pool::CLASSES[pool.class_of(a)][0];
  auto i = objects.find(a);
  if (i != objects.end()) {
    i->second->alive = false;
    authority->unregister_object(*i->second);
    retired.push_back(std::move(i->second));
    objects.erase(i);
  }
  auto j = spells.find(a);
  if (j != spells.end()) {
    retired.push_back(std::move(j->second));
    spells.erase(j);
  }
  pool.free(a);
  forget_heads(a, a + extent);
}
Address World::State::light_effect(const Vec3 &pos, Address follow, Raw life) {
  auto block = pool_new(0xbc);
  if (block)
    light_effect_ctor(block, pos, follow, life);
  return block;
}
// LightEffect_ctor 0x201bd8, its simulation part (research SpellRuntime): one
// shared draw, not solid, a follow handle, the end frame, the run list.
void World::State::light_effect_ctor(Address block, const Vec3 &pos,
                                     Address follow, Raw life) {
  construct_effect(*this, block, pos);
  w(block + 0x48, VT_LIGHTEFFECT);
  w(block + 12, u(block + 12) | 3);
  if (follow)
    handle_link(block + 0x88, follow, follow);
  auto frames = s32(Raw(to_int(mul(life, F_60))));
  w(block + 0xb4, u(FRAME) + Raw(frames > 0 ? frames : 1));
  schedule(block, 1);
  note("light", {block});
}
void World::State::light_effect_run(Address obj) {
  if (!(s32m(FRAME) < s32m(obj + 0xb4)))
    dispatch_virtual(obj, 1, {iv(3)});
}
// ---- Skill objects, casts and spells (SpellRuntime) ------------------------
Address World::State::cast(Address caster, const Cast &c) {
  auto skill = skill_objects.at(c.skill);
  auto pos = c.point ? *c.point : vec(caster + 16);
  Raw facing = c.facing ? *c.facing : Raw(s16m(caster + 0x38));
  note("cast", {caster, Raw(c.skill), Raw(c.level)});
  auto spell = number(skill_virtual(skill, 2, {iv(c.level), iv(caster), pos,
                                               iv(facing)}));
  if (spell) {
    handle_link(caster + 0x364, spell + 4, spell);
    if (s32m(skill + 8) == 1)
      handle_link(caster + 0x588, spell + 4, spell);
  }
  return spell;
}
void World::State::run_queued_casts(Address caster) {
  auto i = queued_casts.find(caster);
  if (i == queued_casts.end())
    return;
  auto casts = std::move(i->second);
  queued_casts.erase(i);
  for (auto &c : casts)
    if (objects.count(caster))
      cast(caster, c);
}
namespace {
// The Skill base bodies of ref_skills_a (slots 8, 10-15).
struct SkillA {
  World::State &s;
  Raw stat(Address skill, Raw stat_index, Raw level) {
    auto st = s32(stat_index), lv = s32(level);
    if (lv == 0)
      return 0;
    auto first = s.s16m(skill + 0xe + 2 * Raw(st));
    if (lv < first)
      return 0;
    auto value = add(from_int(s.s16m(skill + 0x18 + 2 * Raw(st))),
                     mul(s.u(skill + 0x24 + 4 * Raw(st)), from_int(lv - first)));
    return Raw(round_le(value));
  }
  Raw tier2(Address skill, Raw stat_index, Raw level, Raw cap) {
    auto st = s32(stat_index), lv = s32(level), cp = s32(cap);
    if (lv == 0)
      return 0;
    if (cp > 0 && cp < lv)
      lv = cp;
    auto first = s.s16m(skill + 0x38 + 2 * Raw(st));
    Address base = 0x42, step = 0x4c;
    if (first < 0) {
      first = s.s16m(skill + 0xe + 2 * Raw(st));
      base = 0x18;
      step = 0x24;
    }
    if (lv < first)
      return 0;
    auto value =
        add(from_int(s.s16m(skill + base + 2 * Raw(st))),
            mul(s.u(skill + step + 4 * Raw(st)), from_int(lv - first)));
    return Raw(round_le(value));
  }
};
} // namespace
bool World::State::skill_body(Raw fn) const {
  return fn == DISEASEBOLT_RUN || fn == DISEASEBOLT_HIT ||
         fn == HOLYSTRIKE_RUN || fn == HOLYSTRIKE_HIT || fn == SPARKS_RUN ||
         fn == SPARKS_TOUCH;
}
Value World::State::skill_virtual(Address skill, unsigned slot,
                                  std::vector<Value> args) {
  auto table = u(skill + 0x60);
  auto fn = u(table + 8 * slot + 4);
  auto self = Raw(skill + Raw(s16m(table + 8 * slot)));
  auto ar = [&](unsigned k) { return number(args.at(k)); };
  SkillA a{*this};
  // Top-level dispatch: the module whose tables hold the function (A first).
  switch (fn) {
  case 0x3c85fc:
    return iv(a.stat(self, ar(0), ar(1)));
  case 0x3c8690:
    return iv(a.tier2(self, ar(0), ar(1), ar(2)));
  // Skill_getStatFor 0x3c87f0 and Skill_getStatForHandle 0x3c8864
  // (ref_skills_a). Slot 10 for a player, slot 11 otherwise. The handle form
  // reads the object through the node and does not test that object for null.
  case 0x3c87f0:
    if (ar(2) && (u(ar(2) + 0xc) & 0x40))
      return skill_virtual(skill, 10, {iv(ar(0)), iv(ar(1)), iv(ar(3))});
    return skill_virtual(skill, 11, {iv(ar(0)), iv(ar(1)), iv(ar(3))});
  case 0x3c8864: {
    auto node = u(ar(2));
    Address caster = node ? u(node + 0xc) : 0;
    if (node && (u(caster + 0xc) & 0x40))
      return skill_virtual(skill, 10, {iv(ar(0)), iv(ar(1)), iv(ar(3))});
    return skill_virtual(skill, 11, {iv(ar(0)), iv(ar(1)), iv(ar(3))});
  }
  case 0x3c88dc:
    return iv(ar(0));
  case 0x3c8218:
    return iv(1);
  case HOLYSTRIKE_CAN_USE:
  case DISEASEBOLT_CAN_USE:
  case WAVE_CAN_USE:
    return iv(1);
  default:
    break;
  }
  if (slot == 5 && fn == DISEASEBOLT_COST) {
    // SkillDiseaseBolt_manaCost 0x2cb33c. The documented step 2.0000129...
    // is float32 0x40000036. Keep the PS2 model's separate mul and add:
    // IEEE host arithmetic gives different low bits at ranks 2, 5 and 20.
    if (ar(0) < 1 || ar(0) > 20)
      throw Unsupported("Disease Bolt cost outside probed player ranks 1..20");
    return iv(add(from_int(9), mul(from_int(ar(0) - 1), 0x40000036)));
  }
  if (slot != 2)
    throw Unsupported("Skill slot " + std::to_string(slot) + " reaches " +
                      std::to_string(fn) + ", which has no model");
  auto level = ar(0), caster = ar(1);
  auto pos = std::get<Vec3>(args.at(2));
  auto facing = ar(3);
  if (fn == HOLYSTRIKE_CAST)
    return iv(holystrike_cast(self, level, caster, pos, facing));
  if (fn == DISEASEBOLT_CAST)
    return iv(diseasebolt_cast(self, level, caster, pos, facing));
  if (fn == SHOCKOFFIRE_CAST) {
    auto at = b_alloca(0x10);
    for (unsigned i = 0; i < 3; ++i)
      poke(at + 4 * i, 4, pos[i]);
    return iv(shockoffire_cast(self, level, caster, at, facing));
  }
  if (fn == WAVE_CAST)
    return iv(waveofflame_cast(self, level, caster));
  if (fn == WAVE_MANA)
    throw Unsupported("mana cost is not composed");
  throw Unsupported("Skill slot 2 reaches a skill without a composed model");
}
Value World::State::spell_virtual(Address spell, unsigned slot,
                                  std::vector<Value> args) {
  auto table = u(spell + 0x14);
  auto fn = u(table + 8 * slot + 4);
  auto self = Raw(spell + Raw(s16m(table + 8 * slot)));
  if (slot == 1 && fn == WAVE_DTOR) {
    note("destroy_spell", {spell});
    effect_dtor(fn, spell, number(args.at(0)));
    return iv(0);
  }
  if (fn == WAVE_RUN) {
    waveofflame_spell_run(self);
    return iv(0);
  }
  if (fn == DISEASE_RUN)
    throw Unsupported("SpellDiseaseEffect_run (remaining skill path)");
  // SpellHolyShield (skills-35-68 section 54, table 0x519ce0): slot 1 is the
  // base Spell destructor 0x3cf0c4, slot 2 the executed run 0x2e8428
  // (ref_skills_b.holyshield_run).
  if (slot == 1 && fn == HOLY_SHIELD_DTOR) {
    note("destroy_spell", {spell});
    effect_dtor(fn, spell, number(args.at(0)));
    return iv(0);
  }
  if (fn == HOLY_SHIELD_RUN) {
    holy_shield_run(self);
    return iv(0);
  }
  // Round 6 runs, reached by function when a table holds them. Creature slot
  // 17 calls spell slot 2 with the position. Nothing here plants these tables.
  if (fn == 0x305bbc || fn == 0x2fc684 || fn == 0x2fb9b4) {
    if (args.empty() || !std::holds_alternative<Vec3>(args[0]))
      throw Unsupported("a round-6 spell run without its position");
    auto pos = std::get<Vec3>(args[0]);
    if (fn == 0x305bbc)
      arena_run(self, pos);
    else if (fn == 0x2fc684)
      hornomen_run(self, pos);
    else
      planarsteel_run(self, pos);
    return iv(0);
  }
  throw Unsupported("Spell slot " + std::to_string(slot) + " reaches " +
                    std::to_string(fn) + ", which has no model");
}
Value World::State::object_body(Address obj, Raw fn, unsigned slot,
                                std::vector<Value> args) {
  switch (fn) {
  case DISEASEBOLT_RUN:
    diseasebolt_run(obj);
    return iv(0);
  case DISEASEBOLT_HIT:
    diseasebolt_hit(obj, number(args.at(0)));
    return iv(0);
  case HOLYSTRIKE_RUN:
    holystrike_run(obj);
    return iv(0);
  case HOLYSTRIKE_HIT:
    holystrike_hit(obj, number(args.at(0)));
    return iv(0);
  case SPARKS_RUN:
    sparks_run(obj);
    return iv(0);
  case SPARKS_TOUCH:
    sparks_on_touch(obj, number(args.at(0)));
    return iv(0);
  default:
    throw Unsupported("object slot " + std::to_string(slot) + " has no body");
  }
}
// ---- 16 Holy Strike (ref_skills_a) ----------------------------------------
namespace {
const Raw HOLYSTRIKE_DEFS[3] = {0x8e61d0, 0x8e6220, 0x8e6270};
} // namespace
Vec3 World::State::obj_ball(Address obj) {
  for (unsigned attempt = 0; attempt < 65536; ++attempt) {
    auto x = unit(own(obj)), y = unit(own(obj)), z = unit(own(obj));
    if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z))))
      return {x, y, z};
  }
  throw Error("ball rejection budget");
}
void World::State::puff(Address obj, Raw definition, const Vec3 &centre,
                        Raw spread, Raw speed) {
  auto b = obj_ball(obj);
  Vec3 point;
  for (unsigned i = 0; i < 3; ++i)
    point[i] = add(mul(b[i], spread), centre[i]);
  b = obj_ball(obj);
  note("presentation:particle_spawn",
       {definition, point[0], point[1], point[2], mul(b[0], speed),
        mul(b[1], speed), mul(b[2], speed)});
}
Raw World::State::holystrike_cast(Address skill, Raw level, Address caster,
                                  const Vec3 &pos, Raw facing) {
  for (unsigned base : {0x90u, 0x98u}) {
    const Raw v[4] = {0xff, 200, 0x87, 0x40};
    for (unsigned i = 0; i < 4; ++i)
      w(caster + base + 2 * i, 2, v[i]);
  }
  auto obj = pool_new(0x80);
  if (!obj)
    return 0;
  // HolyStrike_ctor 0x2eba48: Projectile_ctor through the shared world.
  construct_effect(*this, obj, pos);
  w(obj + 0x4c, 0);
  w(obj + 0x1c, F_12);
  w(obj + 0x48, VT_PROJECTILE);
  w(obj + 12, u(obj + 12) | 0x4081);
  w(obj + 0x50, 1, 0);
  if (caster)
    handle_link(obj + 0x4c, caster, caster);
  w(obj + 0x54, 0);
  w(obj + 0x38, 2, facing);
  w(obj + 0x48, VT_HOLYSTRIKE);
  w(obj + 0x58, 0);
  if (caster)
    handle_link(obj + 0x54, caster, caster);
  w(obj + 0x5c, 2, level);
  auto frame = u(FRAME);
  w(obj + 0x68, skill);
  w(obj + 0x64, frame + 0xf);
  w(obj + 0x60, frame + 0xb4);
  Raw speed = 0x43e10000;
  w(obj + 0x6c, mul(cos16(Raw(s16m(obj + 0x38))), speed));
  w(obj + 0x74, 0);
  w(obj + 0x78, 0);
  w(obj + 0x70, mul(sin16(Raw(s16m(obj + 0x38))), speed));
  note("presentation:trail_create");
  w(obj + 0x7c, 0);
  note("presentation:trail_addPoint");
  Address who = caster && (u(caster + 12) & 0x20) ? caster : 0;
  collect_in_arc(vec(obj + 16), Raw(s16m(obj + 0x38)), obj + 0x58, who,
                 0x43b40000, 0x42960000, 0);
  schedule(obj, 1);
  resource_at(0x5506c0, 0x5506c8);
  note("presentation:sound_playOnObject");
  return 0;
}
void World::State::holystrike_run(Address obj) {
  constexpr std::array<std::array<Raw, 3>, 3> rows = {
      {{5, 0x40efffdc, 0x41800000},
       {10, 0x411fff59, 0x4147ff03},
       {9, 0x411fffca, 0x419fffcf}}};
  for (unsigned k = 0; k < 3; ++k)
    for (unsigned n = 0; n < rows[k][0]; ++n)
      puff(obj, HOLYSTRIKE_DEFS[k], vec(obj + 16), rows[k][1], rows[k][2]);
  note("presentation:trail_addPoint");
  if (u(obj + 0x58) && s32m(obj + 0x64) < s32m(FRAME)) {
    SkillA a{*this};
    auto rate =
        s32(Raw(s32(a.stat(u(obj + 0x68), 2, Raw(s16m(obj + 0x5c)))) * 0xb6));
    auto target = u(u(obj + 0x58) + 12);
    auto diff = atan2_16(sub(u(target + 20), u(obj + 20)),
                         sub(u(target + 16), u(obj + 16))) -
                s16m(obj + 0x38);
    auto heading = h(obj + 0x38);
    if (std::abs(diff) < rate)
      heading += Raw(diff);
    else if (diff <= 0)
      heading -= Raw(rate);
    else
      heading += Raw(rate);
    w(obj + 0x38, 2, heading);
    Raw speed = 0x43e10000;
    w(obj + 0x6c, mul(cos16(Raw(s16m(obj + 0x38))), speed));
    w(obj + 0x74, 0);
    w(obj + 0x70, mul(sin16(Raw(s16m(obj + 0x38))), speed));
  }
  auto hit = move_and_collide(obj, vec(obj + 0x6c));
  if ((hit & 2) && u(obj + 0x78))
    w(obj + 0x60, 0);
  if (hit & 1)
    w(obj + 0x60, 0);
  if (s32m(FRAME) < s32m(obj + 0x60))
    return;
  resource_at(0x5506c0, 0x5506e0);
  note("presentation:sound_playOnObject");
  light_effect(vec(obj + 16), 0, ONE);
  for (unsigned n = 0; n < 25; ++n) {
    puff(obj, HOLYSTRIKE_DEFS[0], vec(obj + 16), 0x40efffdc, 0x42c00000);
    puff(obj, HOLYSTRIKE_DEFS[1], vec(obj + 16), 0x411fff59, 0x4295ff42);
    puff(obj, HOLYSTRIKE_DEFS[2], vec(obj + 16), 0x411fffca, 0x42efffb6);
  }
  dispatch_virtual(obj, 1, {iv(3)});
}
void World::State::holystrike_hit(Address obj, Address other) {
  if (u(obj + 0x78))
    return;
  auto owner = u(obj + 0x54) ? u(u(obj + 0x54) + 12) : 0;
  if (u(obj + 0x54) && !number(vcall(owner, 20, {iv(other)})))
    return;
  auto flags = u(other + 12);
  if (!(flags & 0x40) && (flags & 0x20) && (q(other + 0xa8) & 0x200))
    return;
  Raw stat_index = s8m(other + 0xb4) == 0x10 ? 1 : 0;
  SkillA a{*this};
  auto amount = from_int(s32(a.stat(u(obj + 0x68), stat_index,
                                    Raw(s16m(obj + 0x5c)))));
  Raw how = u(G_PVP_A) ? 0 : 0x80000;
  auto probe = add(spread_max(amount, own(obj)), HALF);
  auto rolled = spread_max(amount, own(obj));
  std::int32_t high = le(0, probe) ? to_int(add(rolled, ONE))
                                   : to_int(sub(add(rolled, HALF), HALF));
  auto d = descriptor({});
  damage_with_descriptor(*this, other, owner, 100000, how, d, amount,
                         from_int(high));
  w(obj + 0x78, 1);
  for (unsigned base : {0x90u, 0x98u}) {
    const Raw v[4] = {0xff, 200, 0x87, 0x40};
    for (unsigned i = 0; i < 4; ++i)
      w(other + base + 2 * i, 2, v[i]);
  }
}
// ---- 57 Shock of Fire (ref_skills_b) ---------------------------------------
Address World::State::b_alloca(Raw size) {
  b_stack -= (size + 15) & ~15u;
  return b_stack;
}
Address World::State::b_local_vec(const Vec3 &v) {
  auto at = b_alloca(12);
  for (unsigned i = 0; i < 3; ++i)
    poke(at + 4 * i, 4, v[i]);
  return at;
}
namespace {
// The Skill base bodies of ref_skills_b, reached from B's own code.
struct SkillB {
  World::State &s;
  Raw stat_impl(Address skill, Address first_off, Address base_off,
                Address step_off, std::int32_t st, std::int32_t lv) {
    auto first = s.s16m(skill + first_off + 2 * Raw(st));
    if (first > lv)
      return 0;
    auto value = add(from_int(s.s16m(skill + base_off + 2 * Raw(st))),
                     mul(s.u(skill + step_off + 4 * Raw(st)),
                         from_int(lv - first)));
    return Raw(round_lt(value));
  }
  Raw stat(Address skill, Raw st, Raw level) {
    if (s32(level) == 0)
      return 0;
    return stat_impl(skill, 0xe, 0x18, 0x24, s32(st), s32(level));
  }
  Raw tier2(Address skill, Raw st, Raw level, Raw cap) {
    auto lv = s32(level), cp = s32(cap);
    if (lv == 0)
      return 0;
    if (cp > 0)
      lv = std::min(lv, cp);
    if (s.s16m(skill + 0x38 + 2 * Raw(s32(st))) < 0)
      return stat_impl(skill, 0xe, 0x18, 0x24, s32(st), lv);
    return stat_impl(skill, 0x38, 0x42, 0x4c, s32(st), lv);
  }
  Raw stat_for(Address skill, Raw st, Raw level, Address caster, Raw cap) {
    if (caster && (s.u(caster + 12) & 0x40))
      return stat(skill, st, level);
    return tier2(skill, st, level, cap);
  }
  // Slot 8 with its two outputs in the frame (B writes both, then clamps).
  std::pair<std::int32_t, std::int32_t> stat_range(Address skill, Raw st,
                                                   Raw level, Raw spread) {
    auto out = s.b_alloca(8);
    auto value = from_int(s32(stat(skill, st, level)));
    for (unsigned k = 0; k < 2; ++k) {
      auto bound = k == 0 ? sub(value, mul(value, spread))
                          : add(value, mul(value, spread));
      bool negative = lt(bound, 0);
      auto number = Raw(to_int(negative ? sub(bound, HALF) : add(bound, HALF)));
      s.w(out + 4 * k, number);
      if (s32(number) < 1)
        s.w(out + 4 * k, 1);
    }
    return {s.s32m(out), s.s32m(out + 4)};
  }
};
} // namespace
Raw World::State::shockoffire_cast(Address skill, Raw level, Address caster,
                                   Address pos, Raw facing) {
  SkillB sb{*this};
  auto start = vec(pos);
  // slot 14 (level, 20): the level unchanged
  auto lr = sb.stat_range(skill, 0, level, F_02);
  auto splash = sb.stat_for(skill, 1, level, caster, 0);
  auto radius = s32(sb.stat_for(skill, 2, level, caster, 10));
  auto speed = from_int(s32(sb.stat_for(skill, 3, level, caster, 10)));
  auto f = Raw(s16(facing));
  Vec3 velocity{mul(cos16(f), speed), mul(sin16(f), speed), 0};
  auto obj = pool_new(0xb8);
  if (obj) {
    auto p = b_local_vec(start);
    auto v = b_local_vec(velocity);
    sparks_ctor(obj, p, v, caster, 0, Raw(lr.first), Raw(lr.second), splash,
                Raw(radius * 12));
  }
  return 0;
}
void World::State::sparks_ctor(Address obj, Address pos, Address velocity,
                               Address owner, Raw tag, Raw low, Raw high,
                               Raw splash, Raw radius) {
  auto start = vec(pos), speed = vec(velocity);
  auto at = b_local_vec(start);
  construct_effect(*this, obj, vec(at));
  w(obj + 0x4c, 0);
  w(obj + 0x48, VT_PROJECTILE);
  w(obj + 0x1c, F_12);
  w(obj + 0x50, 1, 0);
  w(obj + 12, u(obj + 12) | 0x4081);
  if (owner)
    handle_link(obj + 0x4c, owner, owner);
  w(obj + 0x48, VT_SPARKS);
  w(obj + 0x6d, 1, 0);
  wvec(obj + 0x94, start);
  for (auto p : {std::pair<unsigned, Raw>{0x88, 0x42f00000},
                 {0x8c, 0x3891a2b3},
                 {0x80, 0x42f00000},
                 {0x84, 0x3891a2b3},
                 {0x90, 0}})
    w(obj + p.first, p.second);
  for (auto p : {std::pair<unsigned, Raw>{0x76, 0x13e},
                 {0x78, 0xc1},
                 {0x7c, 0xffe0},
                 {0x6e, 0x13e},
                 {0x70, 0xc1},
                 {0x74, 0xffe0},
                 {0x72, 0},
                 {0x7a, 0}})
    w(obj + p.first, 2, p.second);
  w(obj + 0x6c, 1, 0);
  note("presentation:light_link");
  wvec(obj + 0x58, speed);
  w(obj + 0xb0, splash);
  w(obj + 0xb4, radius);
  w(obj + 0x54, u(FRAME) + 600);
  w(obj + 0xa8, high);
  w(obj + 0xac, low);
  schedule(obj, 1);
  resource_at(0x549088, 0x549458);
  note("presentation:sound_playOnObject");
  w(obj + 0xa0, tag);
}
void World::State::sparks_run(Address obj) {
  if (s32m(FRAME) < s32m(obj + 0x54)) {
    auto vel = b_local_vec(vec(obj + 0x58));
    if (move_and_collide(obj, vec(vel)) == 1)
      w(obj + 0x54, 0);
    note("presentation:voice_setPosition");
    for (unsigned n = 0; n < 3; ++n) {
      auto b = obj_ball(obj);
      note("presentation:particle_spawn", {mul(b[0], 0x41199999)});
    }
    for (unsigned n = 0; n < 5; ++n) {
      obj_ball(obj);
      auto b = obj_ball(obj);
      note("presentation:particle_spawn", {mul(b[0], 0x41199999)});
    }
    if (own(obj) & 1) {
      auto b = obj_ball(obj);
      note("presentation:particle_spawn", {mul(b[0], 0x41200000)});
    }
    wvec(obj + 0x94, vec(obj + 16));
    note("presentation:light_relink");
    return;
  }
  for (unsigned n = 0; n < 50; ++n)
    obj_ball(obj);
  if (s32m(obj + 0xb0) > 0) {
    auto attacker = u(obj + 0x4c) ? u(u(obj + 0x4c) + 12) : obj;
    note("presentation:fx_ringBurst");
    damage_in_radius(attacker, vec(obj + 16), u(obj + 0xb0), 0x40860,
                     u(obj + 0xb0), 0, 0x42200000, from_int(s32m(obj + 0xb4)));
  }
  dispatch_virtual(obj, 1, {iv(3)});
}
void World::State::sparks_on_touch(Address obj, Address other) {
  auto owner = u(obj + 0x4c) ? u(u(obj + 0x4c) + 12) : 0;
  if (!u(obj + 0x54) || !owner || !number(vcall(owner, 20, {iv(other)})))
    return;
  if (auto block = pool_new(0xbc))
    light_effect_ctor(block, vec(obj + 16), 0, ONE);
  resource_at(0x549088, 0x549470);
  note("presentation:sound_playOnObject");
  auto low = u(obj + 0xac), high = u(obj + 0xa8);
  auto d = descriptor({{0x1f, 100, low, high}});
  damage_with_descriptor(*this, other, owner, 10000000, 0xc060, d, 0, 0);
  w(obj + 0x54, 0);
}
// ---- 95 Wave of Flame (ref_skills_c) ----------------------------------------
namespace {
// The Skill base bodies of ref_skills_c, reached from C's own code.
struct SkillC {
  World::State &s;
  Raw tier(Address skill, Address first_at, Address base_at, Address step_at,
           std::int32_t st, std::int32_t lv) {
    auto first = s.s16m(skill + first_at + 2 * Raw(st));
    if (lv < first)
      return 0;
    auto step = mul(s.u(skill + step_at + 4 * Raw(st)), from_int(lv - first));
    return Raw(round_le(add(from_int(s.s16m(skill + base_at + 2 * Raw(st))), step)));
  }
  Raw stat(Address skill, Raw st, Raw level) {
    if (s32(level) == 0)
      return 0;
    return tier(skill, 0xe, 0x18, 0x24, s32(st), s32(level));
  }
  Raw tier2(Address skill, Raw st, Raw level, Raw cap) {
    auto lv = s32(level), cp = s32(cap);
    if (lv == 0)
      return 0;
    if (cp > 0 && cp < lv)
      lv = cp;
    if (s.s16m(skill + 0x38 + 2 * Raw(s32(st))) < 0)
      return tier(skill, 0xe, 0x18, 0x24, s32(st), lv);
    return tier(skill, 0x38, 0x42, 0x4c, s32(st), lv);
  }
  Raw stat_for(Address skill, Raw st, Raw level, Address caster, Raw cap) {
    if (caster && (s.u(caster + 12) & 0x40))
      return stat(skill, st, level);
    return tier2(skill, st, level, cap);
  }
  std::pair<std::int32_t, std::int32_t> spread(Address skill, Raw st,
                                               Raw level, Address owner) {
    if (owner && (s.u(owner + 12) & 0x40)) {
      auto value = from_int(s32(stat(skill, st, level)));
      std::int32_t out[2];
      for (unsigned k = 0; k < 2; ++k) {
        auto b = round_le(k == 0 ? sub(value, mul(value, F_02))
                                 : add(value, mul(value, F_02)));
        out[k] = b > 0 ? b : 1;
      }
      return {out[0], out[1]};
    }
    auto value = from_int(s32(stat_for(skill, st, level, owner, 0)));
    auto high = to_int(add(value, mul(value, F_02)));
    auto low = to_int(sub(value, mul(value, F_02)));
    return {low, high};
  }
};
} // namespace
Raw World::State::waveofflame_cast(Address skill, Raw level, Address caster) {
  auto spell = pool_new(0x2c);
  if (spell) {
    // SpellWaveOfFlame_ctor 0x316d10 with a zero offset.
    w(spell, 1, level);
    w(spell + 8, skill);
    w(spell + 0xc, caster);
    w(spell + 0x14, VT_SPELL_BASE);
    w(spell + 4, 0);
    w(spell + 0x10, draw());
    w(spell + 0x14, VT_SPELL_WAVE);
    w(spell + 0x18, 0);
    auto x = u(caster + 16), y = u(caster + 20);
    if (!eq(u(caster + 0xe0), 0)) {
      x = add(x, mul(cos16(Raw(s16m(caster + 0x38))), u(caster + 0xe0)));
      y = add(y, mul(sin16(Raw(s16m(caster + 0x38))), u(caster + 0xe0)));
    }
    w(spell + 0x1c, add(x, 0));
    w(spell + 0x20, add(y, 0));
    w(spell + 0x24, add(u(caster + 0x3c), 0));
    w(spell + 0x28, 0);
    auto centre = vec(spell + 0x1c);
    resource_at(0x552f40, 0x552f48);
    note("presentation:sound_playAtPos");
    light_effect({add(centre[0], 0), centre[1], add(centre[2], F_36)}, 0, ONE);
    SkillC sc{*this};
    auto steps = from_int(s32(sc.stat_for(u(spell + 8), 0, Raw(s8m(spell)),
                                          caster, 10)));
    auto reach = add(mul(steps, F_12), mul(steps, F_12));
    auto found = collect_in_sphere(centre, STACK_HEAD, reach);
    for (auto obj : found)
      if (obj && !(u(obj + 12) & 0xd))
        handle_link(spell + 0x18, obj, obj);
    clear(STACK_HEAD);
  }
  return spell;
}
void World::State::waveofflame_spell_run(Address spell) {
  auto clock = add(u(spell + 0x28), DT);
  w(spell + 0x28, clock);
  auto ring = add(mul(mul(clock, 0x40c00000), F_12), 0x41a0c8b4);
  SkillC sc{*this};
  auto steps = sc.stat_for(u(spell + 8), 0, Raw(s8m(spell)), u(spell + 0xc), 10);
  if (lt(mul(from_int(s32(steps)), F_12), ring)) {
    spell_virtual(spell, 1, {iv(3)});
    return;
  }
  // SpellWaveOfFlame_hitTest 0x317570.
  std::vector<std::pair<Address, Address>> items;
  for (auto node = u(spell + 0x18); node; node = u(node))
    items.push_back({node, u(node + 12)});
  for (auto &item : items) {
    auto node = item.first, obj = item.second;
    if (!obj) {
      remove(node);
      continue;
    }
    auto dx = sub(u(spell + 0x1c), u(obj + 16)),
         dy = sub(u(spell + 0x20), u(obj + 20));
    if (!le(sub(add(mul(dx, dx), mul(dy, dy)), u(obj + 0x1c)), mul(ring, ring)))
      continue;
    if (u(obj + 12) & 0x20)
      if ((u(obj + 12) & 9) || !number(vcall(u(spell + 0xc), 20, {iv(obj)}))) {
        remove(node);
        continue;
      }
    auto owner = u(spell + 0xc);
    auto range = sc.spread(u(spell + 8), 1, Raw(s8m(spell)), owner);
    auto d = descriptor({{0x15, 10, 10, 0}});
    damage_with_descriptor(*this, obj, u(spell + 0xc), 100000, 0x160, d,
                           from_int(range.first), from_int(range.second));
    resource_at(0x552f40, 0x552f60);
    note("presentation:sound_playAtPos");
    remove(node);
  }
  auto share = mul(ring, 0x3ce56041);
  if (!le(share, ONE))
    share = ONE;
  for (Raw scale : {0x40a00000u, 0x42820000u, 0x40c00000u})
    for (int n = 0; n < std::max(to_int(mul(share, scale)), 0); ++n) {
      spell_rand(spell);
      spell_ball(spell);
      note("presentation:particle_spawn");
    }
}
Raw World::State::spell_rand(Address spell) {
  auto v = u(spell + 0x10) * 0x19660dU + 0x3c6ef35fU;
  w(spell + 0x10, v);
  return v >> 16;
}
Vec3 World::State::spell_ball(Address spell) {
  auto unit_of = [&] {
    return sub(mul(mul(from_int(spell_rand(spell)), F_2M16), TWO), ONE);
  };
  for (unsigned attempt = 0; attempt < 65536; ++attempt) {
    auto x = unit_of(), y = unit_of(), z = unit_of();
    if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z))))
      return {x, y, z};
  }
  throw Error("ball rejection budget");
}
// Disease Bolt's outgoing damage path is observed; expiry is deliberately
// refused because the audited research does not include its destructor.
Raw World::State::diseasebolt_cast(Address skill, Raw level, Address caster,
                                   const Vec3 &pos, Raw facing) {
  if (level < 1 || level > 20)
    throw Unsupported("Disease Bolt cast outside probed player ranks 1..20");
  if (!caster || !(u(caster + 12) & 0x40))
    throw Unsupported("Disease Bolt nonplayer statistics are not installed in this fixture");
  for (unsigned i = 0; i < 3; ++i)
    w(caster + 0x98 + 2 * i, 2, Raw(-100));
  w(caster + 0x9e, 2, 0);
  auto obj = pool_new(0x7c);
  if (!obj) return 0;
  projectile_ctor(obj, pos, caster);
  w(obj + 0x54, 0);
  w(obj + 0x48, VT_DISEASEBOLT);
  w(obj + 0x58, 0);
  w(obj + 0x38, 2, facing);
  w(obj + 0x6c, 0);
  w(obj + 0x64, mul(cos16(Raw(s16m(obj + 0x38))), 0x43af0000));
  w(obj + 0x68, mul(sin16(Raw(s16m(obj + 0x38))), 0x43af0000));
  w(obj + 0x60, 2, level);
  if (caster) handle_link(obj + 0x54, caster, caster);
  auto frame = u(FRAME);
  w(obj + 0x5c, skill);
  w(obj + 0x70, frame + 300);
  w(obj + 0x74, frame + 10);
  w(obj + 0x78, 0);
  if (caster) {
    for (auto other : collect_in_arc(vec(obj + 16), Raw(s16m(obj + 0x38)), 0,
                                     0, 0x438bfffc, 0x42700000, 0)) {
      if (number(vcall(caster, 20, {iv(other)}))) {
        handle_link(obj + 0x58, other, other);
        break;
      }
    }
    clear(STACK_HEAD);
  }
  schedule(obj, 1);
  resource_at(0x54e988, 0x54e990);
  note("presentation:sound_playOnObject");
  note("diseasebolt_spawn", {obj, caster, level});
  return 0;
}
void World::State::diseasebolt_run(Address obj) {
  constexpr Raw defs[3] = {0x8e50a0, 0x8e50f0, 0x8e5140};
  for (unsigned i = 0; i < 3; ++i)
    for (unsigned n = 0; n < (i == 2 ? 6u : 5u); ++n)
      puff(obj, defs[i], vec(obj + 16), i ? 0x41700000 : 0x41200000,
           i ? 0x40a00000 : 0x41800000);
  if (u(obj + 0x58) && s32m(obj + 0x74) < s32m(FRAME)) {
    SkillA a{*this};
    auto skill = u(obj + 0x5c);
    auto owner = u(obj + 0x4c) ? u(u(obj + 0x4c) + 12) : 0;
    auto rank = Raw(s16m(obj + 0x60));
    auto rate = (owner && (u(owner + 12) & 0x40)
                     ? a.stat(skill, 1, rank)
                     : a.tier2(skill, 1, rank, 10)) * 182u;
    auto target = u(u(obj + 0x58) + 12);
    auto diff = std::int64_t(atan2_16(sub(u(target + 20), u(obj + 20)),
                                     sub(u(target + 16), u(obj + 16)))) -
                s16m(obj + 0x38);
    auto step = diff < 0 ? -diff : diff;
    auto heading = Raw(s16m(obj + 0x38));
    heading += Raw(step < s32(rate) ? diff : diff < 1 ? -std::int64_t(rate)
                                                    : std::int64_t(rate));
    w(obj + 0x38, 2, heading);
    w(obj + 0x64, mul(cos16(Raw(s16m(obj + 0x38))), 0x43af0000));
    w(obj + 0x6c, 0);
    w(obj + 0x68, mul(sin16(Raw(s16m(obj + 0x38))), 0x43af0000));
  }
  auto hit = move_and_collide(obj, vec(obj + 0x64));
  if ((hit & 2) && u(obj + 0x78)) w(obj + 0x70, 0);
  if (hit & 1) w(obj + 0x70, 0);
  if (s32m(FRAME) < s32m(obj + 0x70)) return;
  throw Unsupported("Disease Bolt expiry needs its unverified destructor");
}
void World::State::diseasebolt_hit(Address obj, Address other) {
  if (u(obj + 0x78)) return;
  auto owner = u(obj + 0x54) ? u(u(obj + 0x54) + 12) : 0;
  if (!owner) {
    if (!(u(other + 12) & 0x20)) w(obj + 0x78, 1);
  } else if (!number(vcall(owner, 20, {iv(other)}))) return;
  auto flags = u(other + 12);
  if (!(flags & 0x40) && (flags & 0x20) && (q(other + 0xa8) & 0x200)) return;
  SkillA a{*this};
  auto skill = u(obj + 0x5c), rank = Raw(s16m(obj + 0x60));
  auto damage = owner && (u(owner + 12) & 0x40)
                    ? a.stat(skill, 0, rank) : a.tier2(skill, 0, rank, 0);
  auto amount = from_int(damage);
  auto high = spread_max(amount, own(obj));
  auto desc = descriptor({{0x26, damage, damage, 0}});
  damage_with_descriptor(*this, other, owner, 100000, 0, desc, amount, high);
  w(obj + 0x78, 1);
  note("diseasebolt_hit", {obj, other, damage});
}
} // namespace champions::simulation::rta::combat_runtime
