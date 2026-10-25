#include "combat_runtime_internal.hpp"
// Round 4 (research ref_combat_round4 at 95c17ae, docs/spec/combat-round4.md).
// playerProjectile_run for kinds 1, 2 and 3, SpellHolyShield_absorb, and
// Player_blockSparks when silent is not 0. Kinds 0 and 4 and above stay in
// thrown_run. Silent 0 stays in block_sparks. Deflection, the bow release,
// further death states and the remaining skill paths stay refused.
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Raw F_INV65536 = 0x37800000, F_FIVE = 0x40a00000, F_GRAVITY = 0x410fffff,
              F_FLAME_B = 0x3fa3d70a, F_FLAME_BZ = 0x41c5c28f, F_TWENTY = 0x41a00000,
              F_SIX = 0x40c00000, F_THIRTY_SIX = 0x42100000, F_RISE = 0x416b3333,
              F_FORTY = 0x42200000, F_ONE_EIGHTY = 0x43340000, F_FORTY_EIGHT = 0x42400000,
              F_ONE_HUNDRED = 0x42c80000, F_SEVENTY_EIGHT = 0x429c0000,
              F_MINUS_THIRTY_TWO = 0xc2000000, F_ONE_SIXTY_EIGHT = 0x43280000,
              F_SIXTY = 0x42700000, F_ONE_TWENTY_EIGHT = 0x43000000, F_THIRD = 0x3eb33333,
              F_THIRTY_SEVEN = 0x42140000, F_SIXTY_FOUR = 0x42800000,
              F_ONE_TWENTY_FIVE = 0x42fa0000, F_ONE_TWENTY = 0x42f00000,
              F_COOLDOWN = 0x3e86a832, F_HUNDREDTH = 0x3c23d70a,
              F_SPARK_SCALE = 0x3eb2b020, F_SPARK_JITTER = 0x4098a3d7,
              F_SPARK_SPEED = 0x3dcccccc;
constexpr Address P_FLAME_A = 0x8658b0, P_FLAME_B = 0x865900, P_HOLY = 0x8e5ff0,
                  P_BLOCK_A = 0x865810, P_BLOCK_B = 0x865860, S_FX_LUMP_THROWN = 0x535ef0,
                  S_STONE_HIT_THROWN = 0x537b70, S_FIRE = 0x537b90,
                  S_BLAST = 0x537bc8, S_GAS = 0x537bb0, G_LEVEL_TEXT = 0x44fe18, S_INNPIT = 0x535ed8;
constexpr std::uint64_t D_FOUR = 0x4010000000000000ULL;
constexpr Address HOLY_TYPEINFO = 0x3cf124, VT_HOLY_SHIELD = 0x519ce0;
Raw signed_unit(Raw draw) {
  auto unit = mul(from_int(draw), F_INV65536);
  return sub(add(unit, unit), ONE);
}
Raw upward(Raw z) { return lt(z, ZERO) ? neg(z) : z; }
std::string lower_ascii(std::string s) {
  for (auto &c : s)
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  return s;
}
} // namespace
Vec3 World::State::shared_ball_round4() {
  for (unsigned attempt = 0; attempt < 65536; ++attempt) {
    auto x = signed_unit(draw()), y = signed_unit(draw()), z = signed_unit(draw());
    if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z))))
      return {x, y, z};
  }
  throw Error("ball rejection budget");
}
void World::State::potion_run(Address self) {
  // playerProjectile_run 0x1ba940 while +0x1ce is 1, 2 or 3. A potion keeps
  // the mover's result whole: bit 1 is not cleared and +0xcc is not folded in.
  actor(self);
  auto kind = s8m(self + 0x1ce);
  auto pos = [&] { return vec(self + 0x10); };
  if (kind == 1) {
    auto at = pos();
    for (int n = 0; n < 2; ++n) {
      auto ball = shared_ball_round4();
      auto z = upward(ball[2]);
      note("presentation:particle_spawn",
           {P_FLAME_A, at[0], at[1], at[2], mul(ball[0], F_FIVE), mul(ball[1], F_FIVE),
            mul(mul(z, F_FIVE), F_FIVE), ZERO});
    }
    auto ball = shared_ball_round4();
    auto z = upward(ball[2]);
    note("presentation:particle_spawn",
         {P_FLAME_B, at[0], at[1], at[2], mul(ball[0], F_FLAME_B), mul(ball[1], F_FLAME_B),
          mul(mul(z, F_FLAME_B), F_FLAME_BZ), ZERO});
  }
  auto spin = s16m(self + 0x1cc);
  if (!(spin < -0x1387))
    w(self + 0x1cc, 2, Raw(std::uint32_t(spin - 0xc8)));
  w(self + 0x1b8, sub(u(self + 0x1b8), F_GRAVITY));
  auto res = move_and_collide(self, vec(self + 0x1b0));
  auto stone = [&] {
    auto hit = resource_text(cstr(S_FX_LUMP_THROWN), cstr(S_STONE_HIT_THROWN));
    note("presentation:sound_playOnObject", {hit, self, ONE});
  };
  if (res) {
    w(self + 0xec, 0);
    if (res == 2) {
      if (!s8m(self + 0xcc))
        stone();
    } else if (res == 1) {
      if (!services.height_below)
        throw Unsupported("height service missing");
      services.height_below(pos(), u(self + 0x1c));
      stone();
    }
  }
  if (s32m(FRAME) < s32m(self + 0xec))
    return;
  Address owner = target(self + 0x1c8);
  if (kind == 1) {
    auto lump = resource_text(cstr(S_FX_LUMP_THROWN), cstr(S_FIRE));
    note("presentation:sound_playOnObject", {lump, self, ONE});
    auto origin = pos();
    for (int n = 0; n < 5; ++n) {
      auto ball = shared_ball_round4();
      Vec3 place{add(origin[0], mul(ball[0], F_TWENTY)),
                 add(origin[1], mul(ball[1], F_TWENTY)),
                 add(origin[2], mul(ball[2], F_TWENTY))};
      if (!services.height_below)
        throw Unsupported("height service missing");
      auto ground = services.height_below(origin, F_SIX);
      place[2] = add(ground, ONE);
      // LooseFire_ctor 0x202b70 (block, place, owner): research-combat-ctors
      // 98c081a (potion-ctors.md). A zero pool return skips it.
      auto block = pool_new(LOOSEFIRE_SIZE);
      if (block)
        loosefire_ctor(block, place, owner);
      place[2] = add(place[2], ONE);
      auto angle = Raw(std::uint32_t(s16(draw())));
      note("presentation:decal_add",
           {place[0], place[1], place[2], angle, 9, 0, F_THIRTY_SIX});
    }
  } else if (kind == 2) {
    // poisonGasSpell_ctor 0x202de8 (block, position, 300, 20, 0, owner, 5):
    // research-combat-ctors 98c081a (potion-ctors.md), then the gas sound on
    // the cloud. A zero pool return skips the constructor and the sound.
    auto block = pool_new(POISON_GAS_SIZE);
    if (block) {
      poison_gas_ctor(block, pos(), 0x12c, 0x14, 0, owner, 5);
      auto gas = resource_text(cstr(S_FX_LUMP_THROWN), cstr(S_GAS));
      note("presentation:sound_playOnObject", {gas, block, ONE});
    }
  } else {
    auto at = pos();
    auto raised = at;
    raised[2] = add(raised[2], F_RISE);
    auto lump = resource_text(cstr(S_FX_LUMP_THROWN), cstr(S_BLAST));
    note("presentation:sound_playOnObject", {lump, self, ONE});
    note("presentation:fx_ringBurst", {raised[0], raised[1], raised[2], 0x40000000});
    if (lower_ascii(cstr(G_LEVEL_TEXT, 64)) != lower_ascii(cstr(S_INNPIT, 64))) {
      note("world_damageInRadius",
           {owner, at[0], at[1], at[2], 0x32, 0x40840, 0, 0, F_FORTY, F_ONE_EIGHTY});
      damage_in_radius(owner, at, 0x32, 0x40840, 0, 0, F_FORTY, F_ONE_EIGHTY);
    }
    at = pos();
    if (!services.height_below)
      throw Unsupported("height service missing");
    auto ground = services.height_below(at, F_SIX);
    auto spot = at;
    spot[2] = add(spot[2], ONE);
    auto gap = f2d(sub(ground, u(self + 0x18)));
    if (dcmp(gap, 0) < 0)
      gap = dsub(0, gap);
    if (dcmp(gap, D_FOUR) < 0) {
      auto angle = Raw(std::uint32_t(s16(draw())));
      note("presentation:decal_add",
           {spot[0], spot[1], spot[2], angle, 9, 0, F_FORTY_EIGHT});
    }
    auto block = pool_new(0xbc);
    if (block) {
      auto where = pos();
      note("LightEffect_ctor",
           {block, where[0], where[1], where[2], F_ONE_TWENTY_EIGHT, F_ONE_HUNDRED,
            F_SEVENTY_EIGHT, F_MINUS_THIRTY_TWO, ZERO, ZERO, ZERO, ZERO,
            F_ONE_SIXTY_EIGHT, F_SIXTY, ONE, F_THIRD});
      light_effect_ctor(block, where, 0, F_THIRD);
    }
  }
  vcall(self, 1, {iv(3)});
}
Vec3 World::State::spell_ball_add(Address spell) {
  // SpellHolyShield_absorb's unit ball: add(unit, unit) - 1, not mul by two.
  for (unsigned attempt = 0; attempt < 65536; ++attempt) {
    auto x = signed_unit(spell_rand(spell)), y = signed_unit(spell_rand(spell)),
         z = signed_unit(spell_rand(spell));
    if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z))))
      return {x, y, z};
  }
  throw Error("ball rejection budget");
}
Raw World::State::holy_shield_absorb(Address spell) {
  auto caster = u(spell + 0xc);
  actor(caster);
  auto counted = number(vcall(caster, 16));
  if (!services.sample_point_count)
    throw Unsupported("model sample point service missing");
  auto count = s32(services.sample_point_count(counted));
  if (count == 0)
    throw Unsupported("SpellHolyShield_absorb divides by a sample count of 0");
  if (count < 0)
    throw Unsupported("SpellHolyShield_absorb sample count is below 0");
  auto model = number(vcall(caster, 16));
  auto group = u(spell + 0x24);
  note("particleGroup_begin", {group});
  auto facing = Raw(std::uint32_t(s16m(caster + 0x38)));
  auto pos = vec(caster + 0x10);
  if (!services.sample_point)
    throw Unsupported("model_getSamplePoint service missing");
  auto sample = services.sample_point;
  for (int n = 0; n < 0xf1; ++n) {
    auto index = spell_rand(spell) % Raw(std::uint32_t(count));
    auto point = sample(model, caster + 0x114, pos, facing, index, ONE);
    note("model_getSamplePoint", {model, index});
    auto ball = spell_ball_add(spell);
    note("presentation:particle_spawn",
         {P_HOLY, point[0], point[1], point[2], mul(ball[0], F_THIRTY_SEVEN),
          mul(ball[1], F_THIRTY_SEVEN), mul(ball[2], F_THIRTY_SEVEN), ZERO});
  }
  note("particleGroup_end", {group});
  if (le(u(spell + 0x1c), ZERO)) {
    auto block = pool_new(0xbc);
    if (block) {
      auto spot = pos;
      spot[2] = add(spot[2], F_SIXTY);
      note("LightEffect_ctor",
           {block, spot[0], spot[1], spot[2], F_SIXTY_FOUR, F_SIXTY_FOUR,
            F_ONE_TWENTY_FIVE, ZERO, ZERO, ZERO, ZERO, ZERO, F_ONE_TWENTY, F_SIXTY,
            HALF, F_COOLDOWN});
      light_effect_ctor(block, spot, 0, F_COOLDOWN);
    }
    w(spell + 0x1c, F_COOLDOWN);
  }
  auto skill = u(spell + 8);
  auto stat = number(skill_virtual(skill, 10, {iv(0), iv(Raw(std::uint32_t(s8m(spell))))}));
  return mul(from_int(s32(stat)), F_HUNDREDTH);
}
Address World::State::attach_holy_shield(Address caster, Address skill, Raw level) {
  // Authored plant of the words SpellHolyShield_absorb reads. Not the
  // constructor: that skill path is still refused.
  // Size and table are skills-35-68 section 54 (SkillHolyShield_cast
  // allocates 0x28; table 0x519ce0: slot 0 the getter 0x3cf124
  // (runtime.tsv), slot 1 the base destructor 0x3cf0c4, slot 2 the run
  // 0x2e8428). Slots 3 and 4 are "base" with no documented address and stay
  // 0, so a call there refuses in spell_virtual. Like the cloud's table, the
  // slot words enter the image when the first shield is planted.
  actor(caster);
  auto block = pool_new(0x28);
  if (!block)
    throw Unsupported("the pool refused a Holy Shield spell");
  spell_record(block);
  for (unsigned slot = 0; slot < 3; ++slot)
    w(VT_HOLY_SHIELD + 8 * slot, 2, 0);
  w(VT_HOLY_SHIELD + 4, HOLY_TYPEINFO);
  w(VT_HOLY_SHIELD + 12, 0x3cf0c4);
  w(VT_HOLY_SHIELD + 20, 0x2e8428);
  w(block, 1, level & 0xff);
  w(block + 8, skill);
  w(block + 0xc, caster);
  // +0x10 is the constructor's shared draw (the spell's own generator). The
  // constructor is not run, so this is an authored fixture value.
  w(block + 0x10, 1);
  w(block + 0x1c, 0);
  w(block + 0x24, 0);
  w(block + 0x14, VT_HOLY_SHIELD);
  handle_link(caster + 0x364, block + 4, block);
  return block;
}
void World::State::block_sparks_silent(Address p, Raw silent) {
  // Player_blockSparks 0x1b9658 for silent != 0. The plate sound and the 50
  // own-seed sparks are the silent == 0 arm. Both executed callers pass 0;
  // who passes a nonzero silent is not established.
  if (!services.attach_point)
    throw Unsupported("attach point service missing");
  auto hands = services.attach_point;
  auto model = u(p + 0x544);
  auto pos = vec(p + 0x10);
  auto facing = Raw(std::uint32_t(s16m(p + 0x38)));
  for (unsigned index = 0; index < 2; ++index) {
    note("model_getAttachPoint", {model, index});
    hands(p, model, pos, facing, index);
  }
  if (!services.sample_point_count)
    throw Unsupported("model sample point service missing");
  auto count = s32(services.sample_point_count(model));
  if (count <= 0)
    return;
  if (!services.sample_point)
    throw Unsupported("model_getSamplePoint service missing");
  auto obj = u(rec_of(p) + 0x43d8);
  auto sample = services.sample_point;
  Vec3 offset{pos[0], pos[1], add(pos[2], F_THIRTY_SIX)};
  auto particle = silent == 1 ? P_BLOCK_A : P_BLOCK_B;
  for (int index = 0; index < count; ++index) {
    auto point = sample(model, obj + 0x114, pos, facing, Raw(std::uint32_t(index)), ONE);
    note("model_getSamplePoint", {model, Raw(std::uint32_t(index))});
    for (int spark = 0; spark < 5; ++spark) {
      Vec3 delta{mul(sub(point[0], offset[0]), F_SPARK_SCALE),
                 mul(sub(point[1], offset[1]), F_SPARK_SCALE),
                 mul(sub(point[2], offset[2]), F_SPARK_SCALE)};
      auto ball = shared_ball_round4();
      Vec3 direction{add(delta[0], mul(ball[0], F_SPARK_JITTER)),
                     add(delta[1], mul(ball[1], F_SPARK_JITTER)),
                     add(delta[2], mul(ball[2], F_SPARK_JITTER))};
      if (silent == 1 || silent == 2) {
        auto speed = mul(mul(from_int(draw()), F_INV65536), F_SPARK_SPEED);
        note("presentation:particle_spawn",
             {particle, point[0], point[1], point[2], direction[0], direction[1],
              direction[2], speed});
      }
    }
  }
}
} // namespace champions::simulation::rta::combat_runtime
