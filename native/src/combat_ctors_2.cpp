// Authored from research t3/research-combat-ctors-2 at
// 5b928373a4bc31e5566312de598f861cc9f58071: tools/ref_combat_ctors2.py
// (docs/spec/combat-ctors-2.md) and tools/ref_antqueen_run.py
// (docs/spec/antqueen-run.md); the destructor chain beneath is
// tools/ref_combat_monsters.py's (Creature_dtor, GameObject_dtor), borrowed
// unchanged there; the bank helpers are tools/ref_combat_round5.py's. The SHA-256
// of every file is pinned in native/tests/vectors/combat_ctors_2.json.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
constexpr Address G_WORLD = 0x44fdc8, G_SKILL_POISON = 0x41b1d8,
                  G_LEVEL_MONSTER_LEVEL = 0x4f714c, G_CTYPE_TABLE = 0x5628e1;
// AntQueen_run constants (antqueen-run.md section 3 table, Observed).
constexpr Raw STATE_COUNT = 0x1a, F_10 = 0x41200000, F_12 = 0x41400000,
              F_20 = 0x41a00000, F_30 = 0x41f00000, F_36 = 0x42100000,
              F_40 = 0x42200000, F_150 = 0x43160000, F_300 = 0x43960000,
              F_500 = 0x43fa0000, F_15 = 0x41700000, F_2M16 = 0x37800000,
              POISON = 0x17, WAVE_SIZE = 0x3d8, WAVE_LIFE = 600;
// The bank helpers (ref_combat_round5: BANK_*, FADE_RATE, BANK_CLIPS).
constexpr Raw BANK_RATE = 0x3b000000, BANK_LIMIT = 0x3f800000,
              BANK_SPRING = 0x3db851eb, BANK_DAMP = 0x3f75c28f,
              BANK_ANGLE = 0x45081000, FADE_RATE = 0xbd088888;
constexpr Address BANK_CLIPS[4] = {0xb4, 0x44, 0xbc, 0x4c};
// Soul_takeDamage (combat-ctors-2.md section 5).
constexpr int SOUL_PARTICLES = 6;
constexpr Raw FACING_BIAS = 0x4000;
Value iv(std::uint64_t v) { return std::int64_t(v & 0xffffffffu); }
Value sv(std::int32_t v) { return std::int64_t(std::uint32_t(v)); }
std::int64_t integer(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v))
    return *p;
  if (auto p = std::get_if<A::CallbackResult>(&v))
    if (p->kind == A::CallbackResult::Kind::integer)
      return p->negative ? -std::int64_t(p->magnitude) : std::int64_t(p->magnitude);
  if (std::holds_alternative<std::monostate>(v))
    return 0;
  throw A::Error("combat ctors 2: integer call-out result required");
}
Vec3 sample_point(const Value &v) {
  if (auto p = std::get_if<A::Sample>(&v))
    return p->point;
  throw A::Error("combat ctors 2: model_getSamplePoint result required");
}
struct Body {
  A::Model &m;
  std::uint32_t u8(Address a) const { return std::uint32_t(m.mem.load(a, 1)); }
  std::int32_t s8(Address a) const {
    auto v = u8(a);
    return v & 0x80 ? std::int32_t(v) - 0x100 : std::int32_t(v);
  }
  std::uint32_t u16(Address a) const { return std::uint32_t(m.mem.load(a, 2)); }
  std::int32_t s16m(Address a) const { return s16(u16(a)); }
  Raw u32(Address a) const { return Raw(m.mem.load(a, 4)); }
  std::int32_t s32m(Address a) const { return s32(u32(a)); }
  Vec3 vec(Address a) const { return {u32(a), u32(a + 4), u32(a + 8)}; }
  void w8(Address a, std::uint64_t v) { m.store(a, 1, v); }
  void w32(Address a, std::uint64_t v) { m.store(a, 4, v); }
  Value call(const std::string &name, std::vector<Value> args) {
    return m.external_call(name, std::move(args));
  }
  // The signed word at off strictly below the creature clock +0x300.
  bool expired(Address c, Address off) const { return s32m(c + off) < s32m(c + 0x300); }
  bool tracks_below_4(Address c) const { return s8(c + 0x118) < 4; }
  std::uint32_t anim_flags(Address c, std::uint32_t mask) const { return u16(c + 0x11a) & mask; }
  // The facing halfword read unsigned, plus or minus 0x7fff, narrowed to s16.
  std::int32_t back_facing(Address c, int sign) const {
    return s16(Raw(std::int64_t(u16(c + 0x38)) + sign * 0x7fff));
  }
  // Creature_canSeeTargetIn around the own facing, ray mode 0, near 0, lead 0.
  bool see(Address c, Raw arc, Raw far) {
    return m.can_see_target_in(c, Raw(s16m(c + 0x38)), arc, 0, ZERO, far, ZERO) != 0;
  }
};
bool cbytes_equal(A::Model &m, Address a, Address b) {
  for (Address k = 0; k < 256; ++k) {
    auto x = std::uint32_t(m.mem.load(a + k, 1)), y = std::uint32_t(m.mem.load(b + k, 1));
    if (x != y)
      return false;
    if (!x)
      return true;
  }
  return true;
}
// The machine's signed remainder: the sign of the dividend (ref c_rem).
std::int32_t c_rem(Raw a, Raw b) {
  auto x = std::int64_t(s32(a)), y = std::int64_t(s32(b));
  auto r = std::abs(x) % std::abs(y);
  return std::int32_t(x < 0 ? -r : r);
}
// The poison descriptor states 7, 9 and 10 build (ref_antqueen_run
// poison_descriptor): ('desc', ((0x17, v, v, 0), None, None, None)), v the low
// 16 bits of the skill's value.
Value poison_descriptor(std::int64_t value) {
  using R = A::CallbackResult;
  auto v = std::int64_t(value & 0xffff);
  return R::tuple({R::text("desc"),
                   R::tuple({R::tuple({R::integer(POISON), R::integer(v), R::integer(v),
                                       R::integer(0)}),
                             R{}, R{}, R{}})});
}
// GameObject_dtor's name scan (ref_combat_monsters._lower_key, _strneq,
// _find_name). The key string is lower-cased in place before every scan.
void lower_key(A::Model &m) {
  Body b{m};
  for (Address a = S_NAME_KEY;; ++a) {
    auto ch = b.u8(a);
    if (!ch)
      return;
    std::int32_t sc = ch & 0x80 ? std::int32_t(ch) - 0x100 : std::int32_t(ch);
    if (b.u8(Address(Raw(G_CTYPE_TABLE + Raw(sc)))) & 1)
      sc += 0x20;
    b.w8(a, std::uint32_t(sc) & 0xff);
  }
}
bool strneq(A::Model &m, Address a, Address k, unsigned n) {
  for (unsigned i = 0; i < n; ++i) {
    auto x = std::uint32_t(m.mem.load(a + i, 1)), y = std::uint32_t(m.mem.load(k + i, 1));
    if (x != y)
      return false;
    if (!x)
      return true;
  }
  return true;
}
Raw find_name(A::Model &m, Address o, std::int32_t index) {
  Body b{m};
  lower_key(m);
  auto props = b.u32(o + 0x2c);
  for (Address k = 0;; ++k) {
    auto entry = b.u32(Address(props + 4 * k));
    if (!entry)
      return 0;
    if (strneq(m, entry, S_NAME_KEY, 4) && b.s8(entry + 4) == 0x3d) {
      if (--index == -1)
        return Raw(entry + 5);
    }
  }
}
} // namespace

const TypeInfo TYPE_INFOS[8] = {
    {TI_GAMEOBJECT, 0x564318, 0x532210, 0, "10GameObject"},
    {TI_CREATURE, 0x908130, 0x530150, TI_GAMEOBJECT, "8Creature"},
    {TI_SKELETON, 0x908e20, 0x54c168, TI_CREATURE, "8Skeleton"},
    {TI_SOUL, 0x908e30, 0x54c3e0, TI_CREATURE, "4Soul"},
    {TI_ANTQUEEN, 0x908680, 0x530a80, TI_CREATURE, "8AntQueen"},
    {TI_SKELIGHT, 0x908e10, 0x54c178, TI_CREATURE, "8Skelight"},
    {TI_PLAYER, 0x9083d0, 0x530a70, TI_CREATURE, "6Player"},
    {TI_DEMON, 0x908b80, 0x5454a0, TI_CREATURE, "5Demon"}};

const TypeInfo *find_type_info(Raw getter) {
  for (const auto &t : TYPE_INFOS)
    if (t.getter == getter)
      return &t;
  return nullptr;
}
Address type_info(A::Model &m, Raw getter) {
  Body b{m};
  auto t = find_type_info(getter);
  if (!t)
    throw A::Error("type-info getter not in combat-ctors-2.md section 3");
  if (!b.u32(t->descriptor)) {
    if (!t->base) {
      // rtti_initUserTypeInfo 0x3b12d8: +0 the name, +4 the __user_type_info table.
      b.w32(t->descriptor, t->name);
      b.w32(t->descriptor + 4, VT_USER_TYPEINFO);
    } else {
      // The base getter (result unused), then rtti_initSingleBaseTypeInfo
      // 0x3b12b8: +8 the base descriptor, +0 the name, +4 __si_type_info.
      type_info(m, t->base);
      b.w32(t->descriptor + 8, find_type_info(t->base)->descriptor);
      b.w32(t->descriptor, t->name);
      b.w32(t->descriptor + 4, VT_SI_TYPEINFO);
    }
  }
  return t->descriptor;
}
Address type_id(A::Model &m, Address obj) {
  Body b{m};
  if (auto cb = m.callbacks.find("CT_type_id"); cb != m.callbacks.end())
    return Address(integer(cb->second(m, {iv(obj)})));
  auto fn = b.u32(Address(b.u32(obj + 0x48) + 4));
  if (find_type_info(fn))
    return type_info(m, fn);
  auto body = m.class_bodies.find(fn);
  if (body != m.class_bodies.end())
    return Address(integer(body->second(m, obj, {})));
  return Address(Raw(integer(b.call("Creature.v0", {}))));
}
bool type_info_equals(A::Model &m, Address a, Address b) {
  if (a == b)
    return true;
  if (auto cb = m.callbacks.find("CT_type_info_equals"); cb != m.callbacks.end())
    return integer(cb->second(m, {iv(a),iv(b)})) != 0;
  Body x{m};
  return cbytes_equal(m, x.u32(a), x.u32(b));
}

// ---- Skeleton_isEnemy, the take-damage slots ---------------------------------
Raw skeleton_is_enemy(A::Model &m, Address c, Raw other) {
  Body b{m};
  auto r = s32(Raw(m.base_is_enemy(c, other)));
  Raw result = r ? Raw(r + 1) : 0;
  if (!other || result)
    return result;
  auto mine = type_id(m, Address(other));
  if (!type_info_equals(m, mine, type_info(m, TI_SKELETON)))
    return 0;
  if (b.u32(Address(other) + 0xc) & 8)
    return 0;
  if (b.s16m(c + 0x312))
    return 0;
  auto me = b.s8(c + 0x3d4), him = b.s8(Address(other) + 0x3d4);
  return (me == 2 && him == 1) || (me == 1 && him == 2) ? 1 : 0;
}
void soul_take_damage(A::Model &m, Address c, Raw attacker, Raw flags, Raw amount) {
  Body b{m};
  if (!attacker) {
    b.call("cxx_throwBadTypeid", {});
    throw A::Unsupported("Soul_takeDamage 0x2aa908 with no attacker: __throw_bad_typeid "
                         "(what catches it is Unknown, combat-ctors-2.md section 8)");
  }
  auto mine = type_id(m, Address(attacker));
  if (!type_info_equals(m, mine, type_info(m, TI_DEMON)))
    return;
  m.base_take_damage(c, attacker, flags, amount);
  auto count = Raw(integer(
      b.call("model_getSamplePointCount", {iv(Raw(integer(m.vcall(c, 16))))})));
  if (!count)
    return;
  for (int k = 0; k < SOUL_PARTICLES; ++k) {
    auto model = Raw(integer(m.vcall(c, 16)));
    auto index = c_rem(m.own_draw(c), count);
    auto facing = s16(Raw(b.u16(c + 0x38) - FACING_BIAS) & 0xffff);
    auto point = sample_point(b.call(
        "model_getSamplePoint", {iv(model), iv(c + 0x114), b.vec(c + 0x10), sv(facing),
                                 sv(index), iv(1), iv(0), iv(b.u32(c + 0xdc))}));
    Raw v[3];
    for (;;) {
      for (auto &e : v) {
        auto f = mul(from_int(m.own_draw(c)), F_2M16);
        e = sub(add(f, f), ONE);
      }
      auto size = add(add(mul(v[0], v[0]), mul(v[1], v[1])), mul(v[2], v[2]));
      if (!lt(ONE, size))
        break;
    }
    // The copy's z word is zeroed before the scale (0x2aaaec): flat.
    b.call("particle_spawn",
           {iv(P_SOUL), point, Vec3{mul(v[0], F_15), mul(v[1], F_15), mul(ZERO, F_15)},
            iv(ZERO)});
  }
}

// ---- Destructors ----------------------------------------------------------------
void gameobject_dtor(A::Model &m, Address o, Raw flags) {
  Body b{m};
  b.w32(o + 0x48, VT_GAMEOBJECT);
  for (std::int32_t n = 0; b.u32(o + 0x2c); ++n) {
    if (!find_name(m, o, n))
      break;
    b.call("objNames_remove", {iv(find_name(m, o, n)), iv(o)});
  }
  for (Address off : {8u, 4u, 0u})
    b.call("handle_clear", {iv(o + off)});
  if (flags & 1)
    b.call("pool_free", {iv(o)});
}
void creature_dtor(A::Model &m, Address c, Raw flags) {
  Body b{m};
  b.w32(c + 0x48, VT_CREATURE);
  auto node = Raw(integer(b.call("iter_begin", {iv(c + 0x364), iv(0)})));
  while (node) {
    auto spell = b.u32(Address(node) + 0xc);
    if (spell)
      m.table_call("Spell", spell, 0x14, 1, {iv(3)});
    node = Raw(integer(b.call("iter_next", {})));
  }
  b.call("iter_end", {iv(2)});
  b.call("handle_clear", {iv(c + 0x364)});
  b.call("handle_clear", {iv(c + 0x360)});
  b.call("sub_202500", {iv(c + 0x320), iv(2)});
  for (Address off : {0x2e4u, 0x238u, 0x234u})
    b.call("handle_clear", {iv(c + off)});
  b.call("AnimState_dtor", {iv(c + 0x114), iv(0)});
  gameobject_dtor(m, c, 0);
  if (flags & 1)
    b.call("pool_free", {iv(c)});
}
bool is_class_dtor(Raw fn) {
  return fn == ANTQUEEN_DTOR || fn == SKELETON_DTOR || fn == SOUL_DTOR || fn == FIRE_DTOR ||
         fn == LOOSEFIRE_DTOR || fn == POISON_GAS_DTOR;
}
// The inline shape (combat-ctors-2.md section 6): the class table at +0x48,
// handle_clear of each handle in order, the base destructor with 0, then a
// tail jump to pool_free when bit 0 of the flags is set. Bit 1 is not read.
void class_dtor(A::Model &m, Raw fn, Address o, Raw flags) {
  Body b{m};
  Address table = 0;
  std::vector<Address> handles;
  bool creature = true;
  switch (fn) {
  case ANTQUEEN_DTOR:
    table = VT_ANTQUEEN;
    break;
  case SKELETON_DTOR:
    table = VT_SKELETON, handles = {0x3cc, 0x3c8};
    break;
  case SOUL_DTOR:
    table = VT_SOUL, handles = {0x3cc, 0x3c8};
    break;
  case FIRE_DTOR:
    table = VT_FIRE, creature = false;
    break;
  case LOOSEFIRE_DTOR:
    table = VT_LOOSEFIRE, handles = {0x4c}, creature = false;
    break;
  case POISON_GAS_DTOR:
    table = VT_POISON_GAS, handles = {0x4c}, creature = false;
    break;
  default:
    throw A::Error("not a combat-ctors-2 destructor");
  }
  b.w32(o + 0x48, table);
  for (auto off : handles)
    b.call("handle_clear", {iv(o + off)});
  if (creature)
    creature_dtor(m, o, 0);
  else
    gameobject_dtor(m, o, 0);
  if (flags & 1)
    b.call("pool_free", {iv(o)});
}

// ---- AntQueen ---------------------------------------------------------------------
void antqueen_fade_bank_tracks(A::Model &m, Address c) {
  Body b{m};
  auto count = b.s8(c + 0x118);
  if (count <= 0)
    return;
  auto def = Address(b.u32(c + 0x24));
  Raw clips[4];
  for (unsigned k = 0; k < 4; ++k)
    clips[k] = b.u32(def + BANK_CLIPS[k]);
  for (std::int32_t index = 0; index < count; ++index) {
    auto base = c + 32 * Address(index);
    auto clip = b.u32(base + 0x11c);
    if (clip == clips[0] || clip == clips[1] || clip == clips[2] || clip == clips[3])
      b.w32(base + 0x128, FADE_RATE);
  }
}
void antqueen_update_bank(A::Model &m, Address c) {
  Body b{m};
  auto rate = mul(from_int(Raw(b.s16m(c + 0x110))), BANK_RATE);
  if (lt(BANK_LIMIT, rate))
    rate = BANK_LIMIT;
  if (lt(rate, MINUS_ONE))
    rate = MINUS_ONE;
  auto vel = b.u32(c + 0x3e0);
  auto pos = b.u32(c + 0x3dc);
  auto delta = mul(sub(rate, vel), BANK_SPRING);
  pos = add(pos, delta);
  vel = add(vel, pos);
  auto scaled = mul(pos, BANK_DAMP);
  auto twice = add(vel, vel);
  b.w32(c + 0x3e0, vel);
  pos = sub(pos, scaled);
  auto angle = mul(twice, BANK_ANGLE);
  b.w32(c + 0x3dc, pos);
  m.store(c + 0x3e4, 2, std::uint32_t(to_int(angle)) & 0xffffu);
  auto count = b.s8(c + 0x118);
  if (count <= 0)
    return;
  auto def = Address(b.u32(c + 0x24));
  Raw clips[4];
  for (unsigned k = 0; k < 4; ++k)
    clips[k] = b.u32(def + BANK_CLIPS[k]);
  bool positive = lt(ZERO, twice);
  for (std::int32_t index = 0; index < count; ++index) {
    auto base = c + 0x110 + 32 * Address(index);
    auto clip = b.u32(base + 12);
    if (clip == clips[0] || clip == clips[1])
      b.w32(base + 20, positive ? twice : 0);
    else if (clip == clips[2] || clip == clips[3])
      b.w32(base + 20, positive ? 0 : neg(twice));
  }
}
Vec3 antqueen_pick_fly_dest(A::Model &m, Address q) {
  Body b{m};
  if (!b.u32(q + 0x234))
    return b.vec(q + 0x10);
  for (int attempt = 0; attempt < 10; ++attempt) {
    auto angle = s16(m.own_draw(q));
    auto cos_a = cos16(Raw(angle));
    auto t = Address(b.u32(Address(b.u32(q + 0x234)) + 0xc));
    auto d = m.own_draw(q);
    auto x = add(b.u32(t + 0x10),
                 mul(add(mul(mul(from_int(d), F_2M16), F_300), F_300), cos_a));
    auto sin_a = sin16(Raw(angle));
    t = Address(b.u32(Address(b.u32(q + 0x234)) + 0xc));
    d = m.own_draw(q);
    auto y = add(b.u32(t + 0x14),
                 mul(add(mul(mul(from_int(d), F_2M16), F_300), F_300), sin_a));
    auto z = b.u32(q + 0x18);
    // 150 behind the queen's facing: s16 facing - 0x7fff, not narrowed; the
    // routines fold the angle themselves.
    auto behind = Raw(std::int64_t(b.s16m(q + 0x38)) - 0x7fff);
    auto px = add(x, mul(cos16(behind), F_150));
    auto py = add(y, mul(sin16(behind), F_150));
    auto h = Raw(integer(b.call("terrain_height",
                                {iv(b.u32(G_WORLD)), Vec3{px, py, z}, iv(b.u32(q + 0x1c))})));
    auto qz = b.u32(q + 0x18);
    if (lt(sub(qz, F_12), h) && lt(h, add(qz, F_12)))
      return {x, y, z};
  }
  return b.vec(q + 0x10);
}
void antqueen_run(A::Model &m, Address c) {
  Body b{m};
  auto st = [&](Raw state) { m.vcall(c, 27, {iv(state)}); };
  auto target = [&] { return b.u32(c + 0x234); };
  m.process_damage(c, m.class_model(c));
  if (m.creature_run(c, m.class_model(c)))
    return;
  m.update_active(c);
  auto ev = m.step_anim(c, ONE);
  b.call("Creature_applyKnockback", {iv(c), iv(HALF)});
  m.vcall(c, 23, {iv(0)});
  auto state = b.s16m(c + 0x232);
  if (state < 0 || state >= std::int32_t(STATE_COUNT))
    return;
  auto poison_value = [&] {
    return integer(m.table_call("Skill", b.u32(G_SKILL_POISON), 0x60, 11,
                                {iv(0), sv(to_int(b.u32(G_LEVEL_MONSTER_LEVEL))), iv(0)}));
  };
  auto sees = [&] { return integer(m.vcall(c, 25, {iv(0)})) != 0; };
  switch (state) {
  case 0:
    antqueen_fade_bank_tracks(m, c);
    if (sees() && b.tracks_below_4(c))
      return st(3);
    if (sees() && b.tracks_below_4(c))
      return st(3);
    if (b.expired(c, 0xf4))
      return st(1);
    break;
  case 1:
    antqueen_update_bank(m, c);
    if (sees())
      return st(3);
    m.move_to_dest(c);
    break;
  case 3:
    antqueen_update_bank(m, c);
    if (!target())
      return st(0);
    if (m.own_draw(c) & 3) {
      if (b.see(c, 6000, F_20))
        return st(6);
    } else if (integer(b.call("creature_findEnemyInCone",
                              {iv(c), sv(b.back_facing(c, -1)), iv(6000), iv(ZERO), iv(F_40)}))) {
      return st(7);
    }
    m.move_to_target(c);
    if (b.expired(c, 0x3d4) && b.tracks_below_4(c))
      return st(8);
    break;
  case 5: {
    auto roll = m.own_draw(c) & 3;
    if (roll) {
      if (target() && b.see(c, 6000, F_20))
        return st(6);
    } else if (target() &&
               integer(b.call("creature_findEnemyInCone",
                              {iv(c), sv(b.back_facing(c, -1)), iv(6000), iv(ZERO), iv(F_40)}))) {
      return st(7);
    }
    if (m.is_not_rooted(c))
      return st(3);
    break;
  }
  case 6:
    antqueen_fade_bank_tracks(m, c);
    if (target()) {
      if (ev & 1) {
        b.call("Creature_hitTargetAtReach",
               {iv(c), sv(b.s16m(c + 0xd0)), sv(b.s16m(c + 0xd2)), sv(b.s16m(c + 0xd4)),
                iv(0x40008), iv(0), iv(F_36)});
        b.w32(c + 0x3cc, 1);
      }
      if (!b.u32(c + 0x3cc))
        m.turn_to_target(c);
    }
    if (!b.anim_flags(c, 8))
      return st(3);
    break;
  case 7:
    antqueen_fade_bank_tracks(m, c);
    if (ev & 1) {
      auto descriptor = poison_descriptor(poison_value());
      b.call("Creature_hitTargetOrPlayerInArc",
             {iv(c), sv(b.back_facing(c, 1)), iv(3000), sv(b.s16m(c + 0xd0)),
              sv(b.s16m(c + 0xd2)), sv(b.s16m(c + 0xd4)), iv(0x40008), descriptor, iv(ZERO),
              iv(F_40)});
    }
    if (!b.anim_flags(c, 8))
      return st(3);
    break;
  case 8:
    antqueen_update_bank(m, c);
    if (!target())
      return st(0);
    if (b.expired(c, 0x3d4))
      return st(0x17);
    if (b.anim_flags(c, 8))
      return;
    if (b.see(c, 6000, F_20)) {
      st(9);
      if (m.own_draw(c) % 3 == 0)
        b.w32(c + 0x3d4, 0);
      return;
    }
    if (m.can_see_target_in(c, 0, 0x7fff, 0, ZERO, F_20, ZERO))
      m.turn_to_target(c);
    else
      m.move_to_target(c);
    break;
  case 9:
  case 10:
    antqueen_fade_bank_tracks(m, c);
    if (target()) {
      if (ev & 1) {
        auto descriptor = poison_descriptor(poison_value());
        b.call("Creature_hitTargetAtReach",
               {iv(c), sv(b.s16m(c + 0xd0)), sv(b.s16m(c + 0xd2)), sv(b.s16m(c + 0xd4)),
                iv(0x10004), descriptor, iv(F_40)});
      }
      m.turn_to_target(c);
    }
    if (b.anim_flags(c, 8))
      return;
    return st(b.s16m(c + 0x232) == 10 ? 0x17 : 8);
  case 0x12:
    antqueen_fade_bank_tracks(m, c);
    if (!b.anim_flags(c, 0x10) && b.tracks_below_4(c))
      return st(3);
    break;
  case 0x13:
    if (m.is_not_stunned(c))
      return st(0);
    break;
  case 0x14:
    antqueen_fade_bank_tracks(m, c);
    b.call("Creature_deadUpdate", {iv(c), iv(ev), Vec3{0, 0, 0}, iv(0), iv(F_30)});
    break;
  case 0x17:
    antqueen_update_bank(m, c);
    if (!target())
      return st(0);
    if (b.expired(c, 0x3d4)) {
      m.turn_to_target(c);
      if (b.see(c, 3000, F_500))
        return st(0x18);
      return;
    }
    if (b.anim_flags(c, 8))
      return;
    if (m.move_to(c, b.vec(c + 0x104)) || b.s32m(c + 0x2c4) >= 31)
      b.w32(c + 0x3d4, 0);
    break;
  case 0x18:
    antqueen_fade_bank_tracks(m, c);
    if (!b.anim_flags(c, 8) && b.tracks_below_4(c))
      return st(3);
    break;
  case 0x19:
    antqueen_fade_bank_tracks(m, c);
    if (target() && b.s32m(c + 0x300) == s32(Raw(b.u32(c + 0xf4) - 120))) {
      auto facing = b.s16m(c + 0x38);
      auto reach = add(b.u32(c + 0x1c), F_10);
      auto x = add(b.u32(c + 0x10), mul(cos16(Raw(facing)), reach));
      reach = add(b.u32(c + 0x1c), F_10);
      auto y = add(b.u32(c + 0x14), mul(sin16(Raw(facing)), reach));
      Vec3 point{x, y, b.u32(c + 0x18)};
      auto block = Raw(integer(b.call("pool_new", {iv(WAVE_SIZE)})));
      if (block)
        b.call("AntWave_ctor", {iv(block), iv(b.u32(Address(b.u32(c + 0x234)) + 0xc)), point,
                                sv(b.s16m(c + 0x38)), iv(WAVE_LIFE)});
    }
    if (b.expired(c, 0xf4) && b.tracks_below_4(c))
      return st(3);
    break;
  default:
    break;
  }
}

void install_round2(A::Model &m) {
  auto take = [](void (*body)(A::Model &, Address, Raw, Raw, Raw)) {
    return [body](A::Model &model, Address self, const std::vector<Value> &args) -> Value {
      if (args.size() != 3)
        throw A::Error("takeDamage takes three arguments");
      body(model, self, Raw(integer(args[0])), Raw(integer(args[1])), Raw(integer(args[2])));
      return std::monostate{};
    };
  };
  // Skeleton_takeDamage 0x2a774c and AntQueen_takeDamage 0x2378fc: a linked
  // call of Creature_takeDamage with the four arguments, nothing else.
  auto wrapper = [](A::Model &model, Address self, Raw a, Raw f, Raw n) {
    model.base_take_damage(self, a, f, n);
  };
  m.class_bodies[SKELETON_TAKE_DAMAGE] = take(wrapper);
  m.class_bodies[ANTQUEEN_TAKE_DAMAGE] = take(wrapper);
  m.class_bodies[SOUL_TAKE_DAMAGE] = take(soul_take_damage);
  m.class_bodies[SKELETON_IS_ENEMY] = [](A::Model &model, Address self,
                                         const std::vector<Value> &args) -> Value {
    if (args.size() != 1)
      throw A::Error("isEnemy takes one argument");
    return std::int64_t(skeleton_is_enemy(model, self, Raw(integer(args[0]))));
  };
  for (const auto &t : TYPE_INFOS) {
    auto getter = t.getter;
    m.class_bodies[getter] = [getter](A::Model &model, Address,
                                      const std::vector<Value> &) -> Value {
      return std::int64_t(type_info(model, getter));
    };
  }
  for (Raw fn : {ANTQUEEN_DTOR, SKELETON_DTOR, SOUL_DTOR, FIRE_DTOR, LOOSEFIRE_DTOR,
                 POISON_GAS_DTOR})
    m.class_bodies[fn] = [fn](A::Model &model, Address self,
                              const std::vector<Value> &args) -> Value {
      if (args.size() != 1)
        throw A::Error("a destructor takes one argument");
      class_dtor(model, fn, self, Raw(integer(args[0])));
      return std::monostate{};
    };
}
} // namespace champions::simulation::rta::combat_ctors
