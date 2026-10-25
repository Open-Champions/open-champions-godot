#include "champions/fx_trig.hpp"
#include "combat_runtime_internal.hpp"
#include <array>
// Round 5 (research ref_combat_round5 at 2980af1, docs/spec/combat-round5.md).
// AntQueen bank tracks, Soul's demon box, Holy Shield's sample points, and
// MultiFireProjectile_run. Round 6 (combat_runtime_round6.cpp) executes
// PlanarSteel, HornOmen, SpellArena, Pustule, SmokeGen, DiseaseCorpse and
// MultiFireProjectile_end; the run records the reason it passes and then runs
// that end body. The round-4 holy_shield_absorb stub is unchanged.
namespace champions::simulation::rta::combat_runtime {
namespace {
namespace trig = champions::rta::fx::trig;
constexpr Raw BANK_RATE = 0x3b000000, BANK_LIMIT = 0x3f800000, BANK_SPRING = 0x3db851eb,
              BANK_DAMP = 0x3f75c28f, BANK_ANGLE = 0x45081000, FADE_RATE = 0xbd088888,
              DEMON_REACH = 0x43960000, K_SAMPLE = 0x3d800000, K_QUAT = 0x39800000,
              K_POS = 0x3c800000, F_INV65536 = 0x37800000, F_EIGHT = 0x41000000,
              F_THIRTY_SEVEN = 0x42140000, F_SIXTY = 0x42700000, F_SIXTY_FOUR = 0x42800000,
              F_ONE_TWENTY_FIVE = 0x42fa0000, F_ONE_TWENTY = 0x42f00000,
              F_COOLDOWN = 0x3e86a832, F_HUNDREDTH = 0x3c23d70a;
constexpr Address P_HOLY = 0x8e5ff0, P_VOLLEY = 0x8e5910, DEMON_TYPEINFO = 0x3ca81c;
constexpr int DEMON_LIMIT = 128, VOLLEY_LIFE = 0x259, VOLLEY_PARTICLES = 10;
constexpr unsigned BANK_CLIP_OFF[4] = {0xb4, 0x44, 0xbc, 0x4c};

std::array<Raw, 4> bank_clips(World::State &s, Address creature) {
  auto defin = s.u(creature + 0x24);
  std::array<Raw, 4> clips{};
  for (unsigned i = 0; i < 4; ++i)
    clips[i] = s.u(defin + BANK_CLIP_OFF[i]);
  return clips;
}
Raw u32_of(std::int32_t v) { return static_cast<std::uint32_t>(v); }

struct SamplePoint {
  bool present = false;
  std::int32_t x = 0, y = 0, z = 0, word = 0;
};
SamplePoint sample_record(World::State &s, Address model, int index) {
  auto block = model + 16u * s.b(model + 0x24);
  auto count = s.s32m(block + 4);
  SamplePoint point;
  if (index < 0 || index >= count)
    return point;
  auto addr = block + 0x20 + 8 * Raw(index) + 16 * Raw(index / 45);
  point.present = true;
  point.x = s.s16m(addr);
  point.y = s.s16m(addr + 2);
  point.z = s.s16m(addr + 4);
  point.word = s.s16m(addr + 6);
  return point;
}
Mat34 joint_matrix(World::State &s, Address pose, int joint) {
  auto count = s.s16m(pose + 8);
  if (count <= 0)
    throw Unsupported("Holy Shield sample point pose has no joints");
  int j = joint < 0 ? 0 : joint;
  if (j > count - 1)
    j = count - 1;
  std::int32_t rows[3][4];
  for (int row = 0; row < 3; ++row) {
    auto base = pose + 0x10 + j * 24 + row * 8;
    for (int col = 0; col < 4; ++col)
      rows[row][col] = s.s16m(base + 2 * col);
  }
  Mat34 m{};
  for (int r = 0; r < 3; ++r)
    for (int k = 0; k < 3; ++k)
      m[r * 3 + k] = mul(from_int(u32_of(rows[k][r])), K_QUAT);
  for (int k = 0; k < 3; ++k)
    m[9 + k] = mul(from_int(u32_of(rows[k][3])), K_POS);
  return m;
}
Vec3 sample_position(World::State &s, Address caster, Address pose, Raw facing,
                     const SamplePoint &point) {
  auto angle = static_cast<std::uint32_t>(s.s16m(caster + 0x38) - 0x4000);
  auto c = trig::cos16(angle), sn = trig::sin16(angle);
  auto pos = s.vec(caster + 0x10);
  Vec3 p{0, 0, 0};
  if (point.present) {
    auto m = joint_matrix(s, pose, (point.word & 0xff) >> 2);
    Vec3 local{mul(from_int(u32_of(point.x)), K_SAMPLE), mul(from_int(u32_of(point.y)), K_SAMPLE),
               mul(from_int(u32_of(point.z)), K_SAMPLE)};
    p = mat34_transform_point(m, local);
  }
  (void)facing;
  auto nx = neg(p[0]);
  auto out_x = add(mul(nx, c), mul(p[1], sn));
  auto out_y = sub(mul(nx, sn), mul(p[1], c));
  return {add(mul(out_x, ONE), pos[0]), add(mul(out_y, ONE), pos[1]), add(mul(p[2], ONE), pos[2])};
}
} // namespace

void World::State::fade_bank_tracks(Address creature) {
  // AntQueen_fadeBankTracks 0x2385dc. A signed count at or below 0 writes nothing.
  actor(creature);
  auto count = s8m(creature + 0x118);
  if (count <= 0)
    return;
  auto clips = bank_clips(*this, creature);
  for (int index = 0; index < count; ++index) {
    auto base = creature + 32 * index;
    auto clip = u(base + 0x11c);
    bool match = false;
    for (auto word : clips)
      match = match || clip == word;
    if (match)
      w(base + 0x128, FADE_RATE);
  }
}
void World::State::update_bank(Address creature) {
  // AntQueen_updateBank 0x238448. The halfword at +0x3e4 is stored before the
  // track loop. A signed count at or below 0 leaves the tracks alone.
  actor(creature);
  auto rate = mul(from_int(u32_of(s16m(creature + 0x110))), BANK_RATE);
  if (lt(BANK_LIMIT, rate))
    rate = BANK_LIMIT;
  if (lt(rate, MINUS_ONE))
    rate = MINUS_ONE;
  auto vel = u(creature + 0x3e0);
  auto pos = u(creature + 0x3dc);
  auto delta = mul(sub(rate, vel), BANK_SPRING);
  pos = add(pos, delta);
  vel = add(vel, pos);
  auto scaled = mul(pos, BANK_DAMP);
  auto twice = add(vel, vel);
  w(creature + 0x3e0, vel);
  pos = sub(pos, scaled);
  auto angle = mul(twice, BANK_ANGLE);
  w(creature + 0x3dc, pos);
  w(creature + 0x3e4, 2, static_cast<std::uint32_t>(to_int(angle)) & 0xffffu);
  auto count = s8m(creature + 0x118);
  if (count <= 0)
    return;
  auto clips = bank_clips(*this, creature);
  bool positive = lt(ZERO, twice);
  for (int index = 0; index < count; ++index) {
    auto base = creature + 0x110 + 32 * index;
    auto clip = u(base + 12);
    if (clip == clips[0] || clip == clips[1])
      w(base + 20, positive ? twice : 0);
    else if (clip == clips[2] || clip == clips[3])
      w(base + 20, positive ? 0 : neg(twice));
  }
}
int World::State::is_demon_near(Address creature) {
  // Soul_isDemonNear 0x2ab2b4. The box is position ± 300, capacity 128. The
  // first entry whose type equals Demon's and whose object flag 8 is clear
  // returns 1. Demon's descriptor address is not in the research, so the
  // compare is made on the slot-0 getter: every typeinfo getter returns its
  // own class's descriptor, so an entry is a Demon exactly when its getter
  // is Demon_typeinfo 0x3ca81c.
  actor(creature);
  note("Soul_isDemonNear", {creature});
  auto pos = vec(creature + 0x10);
  Vec3 low{sub(pos[0], DEMON_REACH), sub(pos[1], DEMON_REACH), sub(pos[2], DEMON_REACH)};
  Vec3 high{add(pos[0], DEMON_REACH), add(pos[1], DEMON_REACH), add(pos[2], DEMON_REACH)};
  note("world_collectObjectsInBox", {low[0], low[1], low[2], high[0], high[1], high[2],
                                    static_cast<std::uint64_t>(DEMON_LIMIT)});
  for (auto obj : box(low, high, DEMON_LIMIT)) {
    auto getter = u(u(obj + 0x48) + 4);
    note("rtti_typeInfoEquals", {getter, DEMON_TYPEINFO});
    if (getter == DEMON_TYPEINFO && !(u(obj + 0xc) & 8))
      return 1;
  }
  return 0;
}
Raw World::State::holy_shield_sample(Address spell) {
  // SpellHolyShield_absorb 0x2e8534 with model_getSamplePoint followed at
  // scale 1. A zero pose is not built here. A count of 0 still refuses.
  auto caster = u(spell + 0xc);
  actor(caster);
  auto counted = number(vcall(caster, 16));
  auto block = counted + 16u * b(counted + 0x24);
  auto count = s32m(block + 4);
  if (count == 0)
    throw Unsupported("SpellHolyShield_absorb divides by a sample count of 0");
  if (count < 0)
    throw Unsupported("SpellHolyShield_absorb sample count is below 0");
  auto model = number(vcall(caster, 16));
  auto group = u(spell + 0x24);
  note("particleGroup_begin", {group});
  auto facing = Raw(static_cast<std::uint32_t>(s16m(caster + 0x38)));
  auto pos = vec(caster + 0x10);
  auto pose = u(caster + 0x114);
  if (!pose)
    throw Unsupported("Holy Shield sample point with no pose on the caster");
  if (s16m(pose + 8) <= 0)
    throw Unsupported("Holy Shield sample point pose has no joints");
  for (int n = 0; n < 0xf1; ++n) {
    auto index = static_cast<int>(spell_rand(spell) % Raw(static_cast<std::uint32_t>(count)));
    auto point = sample_record(*this, model, index);
    auto where = sample_position(*this, caster, pose, facing, point);
    note("model_getSamplePoint", {model, static_cast<std::uint64_t>(index)});
    auto ball = spell_ball_add(spell);
    note("presentation:particle_spawn",
         {P_HOLY, where[0], where[1], where[2], mul(ball[0], F_THIRTY_SEVEN),
          mul(ball[1], F_THIRTY_SEVEN), mul(ball[2], F_THIRTY_SEVEN), ZERO});
  }
  note("particleGroup_end", {group});
  if (le(u(spell + 0x1c), ZERO)) {
    auto light = pool_new(0xbc);
    if (light) {
      auto spot = pos;
      spot[2] = add(spot[2], F_SIXTY);
      note("LightEffect_ctor",
           {light, spot[0], spot[1], spot[2], F_SIXTY_FOUR, F_SIXTY_FOUR, F_ONE_TWENTY_FIVE, ZERO,
            ZERO, ZERO, ZERO, ZERO, F_ONE_TWENTY, F_SIXTY, HALF, F_COOLDOWN});
      light_effect_ctor(light, spot, 0, F_COOLDOWN);
    }
    w(spell + 0x1c, F_COOLDOWN);
  }
  auto skill = u(spell + 8);
  auto stat = number(skill_virtual(skill, 10, {iv(0), iv(Raw(static_cast<std::uint32_t>(s8m(spell))))}));
  return mul(from_int(s32(stat)), F_HUNDREDTH);
}
void World::State::multifire_run(Address obj) {
  // MultiFireProjectile_run 0x2da2bc. The run records the reason it passes to
  // MultiFireProjectile_end 0x2da928, then runs the round-6 end body. Once
  // +0x60 is set the run returns.
  actor(obj);
  if (u(obj + 0x60))
    return;
  auto at = vec(obj + 0x10);
  note("trail_addPoint", {u(obj + 0x74), at[0], at[1], at[2]});
  auto age = u(obj + 0x6c) + 1;
  w(obj + 0x6c, age);
  auto result = move_and_collide(obj, vec(obj + 0x54));
  if (result & 1) {
    note("MultiFireProjectile_end", {obj, 1});
    multifire_end(obj, 1);
    return;
  }
  if (!(s32(age) < VOLLEY_LIFE)) {
    note("MultiFireProjectile_end", {obj, 0});
    multifire_end(obj, 0);
    return;
  }
  auto pos = vec(obj + 0x10);
  for (int n = 0; n < VOLLEY_PARTICLES; ++n) {
    Vec3 ball{};
    bool inside = false;
    for (unsigned attempt = 0; attempt < 65536 && !inside; ++attempt) {
      for (auto &x : ball) {
        auto draw = mul(from_int(own(obj)), F_INV65536);
        x = sub(add(draw, draw), ONE);
      }
      inside = !lt(ONE, add(add(mul(ball[0], ball[0]), mul(ball[1], ball[1])), mul(ball[2], ball[2])));
    }
    if (!inside)
      throw Error("ball rejection budget");
    Vec3 velocity{mul(ball[0], F_EIGHT), mul(ball[1], F_EIGHT), mul(ball[2], F_EIGHT)};
    auto fraction = mul(from_int(own(obj)), F_INV65536);
    Vec3 spot{add(pos[0], mul(velocity[0], fraction)), add(pos[1], mul(velocity[1], fraction)),
              add(pos[2], mul(velocity[2], fraction))};
    note("presentation:particle_spawn",
         {P_VOLLEY, spot[0], spot[1], spot[2], velocity[0], velocity[1], velocity[2], ZERO});
  }
}
Address World::State::arm_fixture_holy_shield(Address player) {
  // Plants the sample model and the one-joint pose Holy Shield reads, then
  // attaches a shield on the skill already given to the player. Not the
  // constructor. give_skill(16) is not called again.
  actor(player);
  auto model = alloc_fixture(0x90);
  w(model + 0x24, 1, 6);
  w(model + 0x64, 4, 1);
  auto pose = alloc_fixture(0x40);
  w(pose + 8, 2, 1);
  w(pose + 0x10, 2, 0x1000);
  w(pose + 0x18, 2, 0x1000);
  w(pose + 0x20, 2, 0x1000);
  auto defin = u(player + 0x24);
  w(defin + 12, model);
  // Player slot 16 is 0x3c3b38: the model at +0x544. Creature slot 16 still
  // reads the class definition.
  w(player + 0x544, model);
  w(player + 0x114, pose);
  auto rec = record(player);
  if (!lt(ZERO, u(rec + 0xc0))) {
    if (!lt(ZERO, u(rec + 0xc4)))
      w(rec + 0xc4, 0x42c80000);
    w(rec + 0xc0, u(rec + 0xc4));
  }
  auto skill = u(G_SKILL_TABLE + 4 * 16);
  if (!skill)
    throw Error("give_skill first");
  // attach_holy_shield points the spell at table 0x519ce0 with slots 0-2
  // planted: the live fight calls slot 2 (run) every tick and slot 1 (the
  // destructor) when the shield ends. Slots 3 and 4 stay 0 and refuse.
  auto spell = attach_holy_shield(player, skill, 1);
  // SpellHolyShield_run reads the clock at +0x18 and the duration at +0x20.
  // A zero duration ends the spell on the first creature slot-17 tick.
  w(spell + 0x18, 0);
  auto seconds = s32(number(skill_virtual(skill, 10, {iv(1), iv(Raw(s8m(spell)))})));
  w(spell + 0x20, from_int(static_cast<std::uint32_t>(seconds)));
  return spell;
}
void World::State::holy_shield_run(Address spell) {
  // SpellHolyShield_run 0x2e8428, the skills-b body. Creature slot 17 calls it
  // with the owner's position and facing; this body does not read those.
  constexpr Raw DT = 0x3c888888;
  auto clock = add(u(spell + 0x18), DT);
  w(spell + 0x18, clock);
  w(spell + 0x1c, sub(u(spell + 0x1c), DT));
  if (lt(u(spell + 0x20), clock)) {
    spell_virtual(spell, 1, {iv(3)});
    return;
  }
  auto owner = u(spell + 0xc);
  auto matrix = mat34_set_pos_rot_z(vec(owner + 0x10),
                                    static_cast<std::uint32_t>(s16m(owner + 0x38)));
  std::vector<std::uint64_t> args{u(spell + 0x24)};
  args.insert(args.end(), matrix.begin(), matrix.end());
  note("particleGroup_setMatrix", std::move(args));
  owner = u(spell + 0xc);
  for (auto tint : {std::pair<unsigned, Raw>{0x9e, 0x37}, {0x90, 0x15}, {0x92, 0x1c},
                    {0x94, 0x73}, {0x98, 0x15}, {0x9a, 0x1c}, {0x9c, 0x73}, {0x96, 0x37}})
    w(owner + tint.first, 2, tint.second);
}
} // namespace champions::simulation::rta::combat_runtime
