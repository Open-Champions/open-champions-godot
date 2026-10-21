#include "combat_runtime_internal.hpp"
// The endings of thrown potions 1 and 2 (research t3/research-combat-ctors
// 98c081a, docs/spec/potion-ctors.md, tools/ref_potion_ctors.py; executed by
// tools/difftest_potion_ctors.py, 0 mismatches): LooseFire_ctor 0x202b70 with
// fx_spawn id 5 0x1f0ae0 and Fire_ctor 0x2020e0, Fire_run 0x1f34e0,
// LooseFire_run 0x202c48, LooseFire_onTouch 0x1fcb70 and poisonGasSpell_ctor
// 0x202de8. The fire entry of a touch (descriptor type 0x15 through the real
// damage_apply, and the burn it leaves) is research-combat-ctors-2
// docs/spec/fire-touch.md (ref_fire_touch.py, executed by
// difftest_fire_touch.py). The destructors are combat_ctors_2.cpp's;
// poisonGasSpell_run is combat_runtime_round6.cpp's (combat round 6).
namespace champions::simulation::rta::combat_runtime {
int damage_with_descriptor(World::State &, Address, Address, Raw, Raw,
                           const combat::Descriptor &, Raw, Raw);
void construct_effect(World::State &, Address, const Vec3 &);
namespace {
constexpr Raw F_SIX = 0x40c00000, F_TEN = 0x41200000, F_FIVE = 0x40a00000,
              F_GLOW_SIZE = 0x45fa0000, F_GLOW_SWING = 0x437a0000,
              F_RISE = 0x40999999;
constexpr Address G_DIFFICULTY = 0x4f719c, G_GLOW_TEXTURE = 0x4f848c, FIRE_PARTICLE = 0x870548;
constexpr Raw LOOSEFIRE_LIFE = 120, FIRE_LIFE = 100, TOUCH_PERIOD = 20,
              TOUCH_RATING = 10000000, TOUCH_FLAGS = 0x100c040,
              FIRE_ENTRY = 0x15, CREATURE_STRENGTH = 8, GAS_RADIUS_FLOOR = 10;
// The machine's signed div remainder (ref_skills_a.rem).
std::int32_t rem(Raw a, Raw b) {
  auto x = s32(a), y = s32(b);
  if (y == 0)
    return x;
  std::int64_t q = std::abs(std::int64_t(x)) / std::abs(std::int64_t(y));
  if ((x < 0) != (y < 0))
    q = -q;
  return std::int32_t(std::int64_t(x) - q * std::int64_t(y));
}
} // namespace
// The table words a constructor installs enter the image with the first
// object of the class (as the DiseaseCloud's), with the type-info descriptor's
// name word in mangled form (symbols/runtime.tsv getters and descriptors).
// Slots 7, 10, 11 and 12 are the GameObject defaults 0x3c7eb0, 0x3c7ec8,
// 0x3c7ef0 and 0x3c7ef8, which all 156 classes outside the Creature branch
// hold (research docs/runtime.md section 4.4, Observed).
void World::State::potion_table(Address table, Raw getter, Address desc,
                                const char *name,
                                std::initializer_list<std::pair<unsigned, Raw>> slots) {
  if (u(table + 4) == getter)
    return;
  w(table, 2, 0);
  w(table + 4, getter);
  for (auto p : {std::pair<unsigned, Raw>{7, 0x3c7eb0}, {10, 0x3c7ec8}, {11, 0x3c7ef0},
                 {12, 0x3c7ef8}}) {
    w(table + 8 * p.first, 2, 0);
    w(table + 8 * p.first + 4, p.second);
  }
  for (auto &p : slots) {
    w(table + 8 * p.first, 2, 0);
    w(table + 8 * p.first + 4, p.second);
  }
  if (!u(desc)) {
    std::string text = name;
    auto at = alloc_fixture(unsigned(text.size() + 1));
    for (unsigned k = 0; k <= text.size(); ++k)
      w(at + k, 1, k < text.size() ? Raw(std::uint8_t(text[k])) : 0);
    w(desc, at);
  }
}
// fx_spawn 0x1ef270 id 5 (the arm at 0x1f0ae0): no camera test; one shared
// draw d gives 3 + (d & 1) fires; each takes three shared draws for its point
// ((x - 6) + d % 12, (y - 6) + d % 12, z + d % 18), then pool_new(0x50) and
// Fire_ctor when the pool gives a block. The 24 particle definitions it builds
// on first use (0x4f87bc + 4i, research-fx effects.md 14.1) are presentation
// and are not kept by the World.
void World::State::fx_spawn_fire(const Vec3 &pos) {
  note("fx_spawn", {5, pos[0], pos[1], pos[2]});
  auto count = 3 + (draw() & 1);
  for (Raw fire = 0; fire < count; ++fire) {
    auto dx = draw(), dy = draw(), dz = draw();
    Vec3 point{add(sub(pos[0], F_SIX), from_int(dx % 12)),
               add(sub(pos[1], F_SIX), from_int(dy % 12)),
               add(pos[2], from_int(dz % 18))};
    auto block = pool_new(0x50);
    if (block)
      fire_ctor(block, point);
  }
}
// Fire_ctor 0x2020e0 (block, position): GameObject_ctor (one shared draw),
// table 0x50f208, flags |= 0x4001, byte +0x28 = 0xf0, obj_runEveryFrame, the
// life halfword +0x4c = 100.
void World::State::fire_ctor(Address obj, const Vec3 &pos) {
  potion_table(VT_FIRE, 0x3c50b8, 0x908690, "4Fire",
               {{1, FIRE_DTOR}, {4, FIRE_RUN}});
  construct_effect(*this, obj, pos);
  w(obj + 0x48, VT_FIRE);
  w(obj + 0x28, 1, 0xf0);
  w(obj + 0xc, u(obj + 0xc) | 0x4001);
  schedule(obj, 1);
  w(obj + 0x4c, 2, FIRE_LIFE);
}
// Fire_run 0x1f34e0: the glow and one particle from the fire's own generator,
// then the life drops by one; at 0 or below slot 1 with 3. No shared draw.
void World::State::fire_run(Address obj) {
  actor(obj);
  auto life = s16m(obj + 0x4c);
  std::int32_t alpha = life < 20 ? 60 - 3 * (20 - life) : life < 0x51 ? 60 : 3 * (100 - life);
  auto swing = sin16(Raw(u(FRAME) << 10));
  auto size = add(mul(swing, F_GLOW_SWING), F_GLOW_SIZE);
  auto at = vec(obj + 0x10);
  note("presentation:glow_add", {at[0], at[1], at[2], 0xff, 0x96, 0, Raw(alpha),
                                 u(G_GLOW_TEXTURE), 0, 0, F_FIVE, size});
  auto ball = obj_ball(obj);
  Vec3 velocity{add(ball[0], ball[0]), add(ball[1], ball[1]),
                add(add(ball[2], ball[2]), F_RISE)};
  auto spot = obj_ball(obj);
  auto up = add(spot[2], spot[2]);
  if (lt(up, ZERO))
    up = neg(up);
  auto pos = vec(obj + 0x10);
  Vec3 point{add(add(spot[0], spot[0]), pos[0]), add(add(spot[1], spot[1]), pos[1]),
             add(up, pos[2])};
  note("presentation:particle_spawn", {FIRE_PARTICLE, point[0], point[1], point[2],
                                       velocity[0], velocity[1], velocity[2], ZERO});
  auto left = (h(obj + 0x4c) - 1) & 0xffff;
  w(obj + 0x4c, 2, left);
  if (s16(left) <= 0 && obj)
    vcall(obj, 1, {iv(3)});
}
// LooseFire_ctor 0x202b70 (block, position, owner or 0).
void World::State::loosefire_ctor(Address obj, const Vec3 &pos, Address owner) {
  potion_table(VT_LOOSEFIRE, 0x3c67e8, 0x908200, "9LooseFire",
               {{1, LOOSEFIRE_DTOR}, {4, LOOSEFIRE_RUN}, {6, LOOSEFIRE_TOUCH}});
  construct_effect(*this, obj, pos);
  w(obj + 0x48, VT_LOOSEFIRE);
  w(obj + 0x4c, 0);
  if (owner) {
    actor(owner);
    handle_link(obj + 0x4c, owner, owner);
  }
  w(obj + 0x50, u(FRAME) + LOOSEFIRE_LIFE);
  fx_spawn_fire(vec(obj + 0x10));
  schedule(obj, 1);
  w(obj + 0xc, u(obj + 0xc) | 0x420c);
  w(obj + 0x1c, F_TEN);
}
// LooseFire_run 0x202c48: slot 1 with 3 once the signed end frame +0x50 is
// strictly below g_simFrame, so a LooseFire made on frame F is deleted by the
// run of frame F + 121.
void World::State::loosefire_run(Address obj) {
  actor(obj);
  if (s32m(obj + 0x50) < s32m(FRAME) && obj)
    vcall(obj, 1, {iv(3)});
}
// LooseFire_onTouch 0x1fcb70 (fire, toucher), slot 6: only on frames
// divisible by 20; the owner's slot 20 must call the toucher an enemy; a
// player's entry is (0x15, v, v, 0) with v from g_difficultyIdx (2..5, else
// 1), a creature's (0x15, 8, 8, 0), anything else an empty descriptor; then
// damage_apply(toucher, owner or the fire, 10000000, 0, 0, 0x100c040, ...).
void World::State::loosefire_on_touch(Address obj, Address other) {
  actor(obj);
  if (rem(u(FRAME), TOUCH_PERIOD))
    return;
  Address owner = 0;
  if (auto node = u(obj + 0x4c)) {
    auto candidate = u(node + 0xc);
    if (u(candidate + 0xc) & 0x20)
      owner = candidate;
  }
  if (owner && !number(vcall(owner, 20, {iv(other)})))
    return;
  auto flags = u(other + 0xc);
  combat::Descriptor desc(4, std::vector<std::int64_t>{-1, 0, 0, 0});
  if (flags & 0x60) {
    std::int32_t value = CREATURE_STRENGTH;
    if (flags & 0x40) {
      auto level = s32m(G_DIFFICULTY);
      value = level >= 2 && level <= 5 ? level : 1;
    }
    note("LooseFire_onTouch", {obj, other, FIRE_ENTRY, Raw(value)});
    desc[0] = {FIRE_ENTRY, value, value, 0};
  }
  // damage_apply(toucher, owner or the fire, 10000000, 0, 0, 0x100c040,
  // descriptor, 0; 0.0, 0.0); its result is dropped (fire-touch.md section 2).
  note("damage_apply", {other, owner ? owner : obj, TOUCH_RATING, TOUCH_FLAGS});
  damage_with_descriptor(*this, other, owner ? owner : obj, TOUCH_RATING, TOUCH_FLAGS,
                         desc, ZERO, ZERO);
}
// poisonGasSpell_ctor 0x202de8 (block, position, life, radius, word, owner,
// damage): a GameObject with one shared draw, no Spell base.
void World::State::poison_gas_ctor(Address obj, const Vec3 &pos, Raw life, Raw radius,
                                   Raw word, Address owner, Raw damage) {
  potion_table(VT_POISON_GAS, 0x3c78e8, 0x908700, "14poisonGasSpell",
               {{1, POISON_GAS_DTOR}, {4, POISON_GAS_RUN}, {5, 0x202f00}});
  construct_effect(*this, obj, pos);
  w(obj + 0x4c, 0);
  w(obj + 0x48, VT_POISON_GAS);
  wvec(obj + 0x50, pos);
  w(obj + 0x60, radius);
  w(obj + 0x68, word);
  w(obj + 0x5c, u(FRAME) + life);
  w(obj + 0x6c, damage);
  if (owner) {
    actor(owner);
    handle_link(obj + 0x4c, owner, owner);
  }
  if (s32m(obj + 0x60) < s32(GAS_RADIUS_FLOOR))
    w(obj + 0x60, GAS_RADIUS_FLOOR);
  w(obj + 0xc, u(obj + 0xc) | 3);
  w(obj + 0x64, F_TEN);
  schedule(obj, 1);
}
} // namespace champions::simulation::rta::combat_runtime
