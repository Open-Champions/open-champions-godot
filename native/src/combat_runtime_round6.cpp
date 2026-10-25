#include "champions/fx_trig.hpp"
#include "combat_runtime_internal.hpp"
// Round 6 (research ref_combat_round6 at faa7033, docs/spec/combat-round6.md).
// The eleven handlers that difftest executes. Every constant below is an
// immediate from tools/ref_combat_round6.py or an address ref_creature already
// names. The round-5 MultiFireProjectile_run now reaches the end body here.
//
// The destructors of these classes (slot 1) are not in the research: section 8
// leaves every spell destructor but two unmodelled, and no object destructor of
// the gas cloud, the corpse or the blob is read. Each "slot 1 with 3" therefore
// records the call and refuses unless the object's slot 1 is a destructor this
// runtime already models. The spell vtables 0x51ae58 and 0x51ae28 are stored
// as the ctors store them; their slot words were not read and are not planted.
//
// Still refused, because this difftest does not execute them: StaticFire_run
// 0x2c595c, deflection, the bow release inside the player tick, monster death
// states past the bodies already composed, Skeleton and AntQueen past state 1,
// Soul's thrown-state hits, a nonzero silent passed to Player_blockSparks,
// haze kind 3, SpellDiseaseEffect_run, SpellHolyShield_ctor,
// SkillStrongStance_trigger, GutChunk's run, and Fire from fx_spawn id 5.
// SmokeGen_stepParticles 0x31cac8 is called and not given a body; it stays a
// recorded call, as in the research.
// The seven descriptor-search instructions past the first empty slot are dead
// on a fresh record; the descriptor written here is that first slot.
namespace champions::simulation::rta::combat_runtime {
namespace {
namespace trig = champions::rta::fx::trig;
// Addresses and immediates from tools/ref_combat_round6.py.
constexpr Raw GAS_PARTICLE = 0x871088, GAS_SPEED = 0x40a00000, GAS_FLAGS = 0x01000040;
constexpr Raw F_SCALE = 0x3c23d70a, F_SPREAD = 0x3e4ccccc, HIT_FLAGS = 0x40000;
constexpr Raw PLAYER_SKIP = 0x01000040, CREATURE_SKIP = 0x04000000;
constexpr Address FMT_ICE = 0x54f598, FMT_STONE = 0x54f5b0, FX_LUMP = 0x54f560;
constexpr Raw LEVEL_ICE = 0x40000;
constexpr Raw F_1000 = 0x447a0000, F_3 = 0x40400000, SMOKE_FLAGS = 0x0100c040;
constexpr Address PUS_FALL = 0x8e7548, PUS_DROP = 0x8e74f8;
constexpr Address PUS_LUMP = 0x552328, PUS_SOUND = 0x552388;
constexpr Raw PUS_GRAVITY = 0xbe4ccab3, PUS_DROP_GRAVITY = 0xbee1c9f7;
constexpr Raw PUS_AGE = 0x420eccfb;
constexpr int PUS_LIFE = 0x12d;
constexpr Address G_ARENA_COUNTER = 0x5038c8;
constexpr Address ARENA_A = 0x8e71e0, ARENA_B = 0x8e7190;
constexpr Raw ARENA_TINT[4] = {0x56, 0x35, 0x21, 0x3d};
constexpr Address HO_PARTICLE = 0x8e6c40;
constexpr Address VT_SPELL = 0x515ba0, HO_VT = 0x51ae58, PS_VT = 0x51ae28;
constexpr Address PS_BURST = 0x8e6ba0, PS_SPARK = 0x8e6b00, PS_SPARK2 = 0x8e6b50,
                  PS_FALL = 0x8e6bf0;
constexpr Address PS_LUMP = 0x551320, PS_SOUND_START = 0x551328, PS_SOUND_BURST = 0x551340;
constexpr Address G_GLOW_TEX_C = 0x4f8498;
// ref_creature: g_cullByCamera, the camera focus, the record's object word,
// and 2^-16.
constexpr Address G_CULL = 0x4039c1, G_FOCUS = 0x44f048, REC_OBJ = 0x43d8;
constexpr Raw F_2M16 = 0x37800000;
// The difftest's spell block. The script built-ins that allocate these spells
// (hornOmen 0x146628, planarSteel 0x1466d4) were not read, so this is a
// fixture size: every field the two ctors store lies below 0x70.
constexpr Raw FIXTURE_SPELL_SIZE = 0x80;

std::int32_t round_half_away(Raw v) {
  return to_int(le(ZERO, v) ? add(v, HALF) : sub(v, HALF));
}
Raw frac(Raw draw) { return mul(from_int(draw), F_2M16); }
void particle(World::State &s, Raw def, const Vec3 &p, const Vec3 &v) {
  s.note("presentation:particle_spawn",
         {def, p[0], p[1], p[2], v[0], v[1], v[2], ZERO});
}
combat::Descriptor one_row(Raw type, Raw a, Raw b, Raw c) {
  return {{s16(type & 0xffff), s16(a & 0xffff), s16(b & 0xffff), s16(c & 0xffff)},
          {-1, 0, 0, 0},
          {-1, 0, 0, 0},
          {-1, 0, 0, 0}};
}
void damage_row(World::State &s, Address victim, Address attacker, Raw flags,
                bool row, Raw type, Raw a, Raw b, Raw c, Raw lo, Raw hi) {
  // damage_apply(victim, attacker, 100000, 0, 0, flags, entry or 0, 0; lo, hi)
  // as the round-6 method passes it. The damage body notes its own
  // "damage_apply" event.
  s.note("round6_damage", {victim, attacker, flags, row ? Raw(1) : Raw(0), type, a,
                          b, c, lo, hi});
  if (row)
    damage_with_descriptor(s, victim, attacker, 100000, flags,
                           one_row(type, a, b, c), lo, hi);
  else
    s.damage_full(victim, attacker, 100000, 0, 0, flags, 0, lo, hi);
}
void damage_plain(World::State &s, Address victim, Address attacker, Raw flags,
                  Raw lo, Raw hi) {
  damage_row(s, victim, attacker, flags, false, 0, 0, 0, 0, lo, hi);
}
struct Pair {
  Raw low, high;
};
Pair player_bounds(World::State &s, Address obj, Address owner) {
  auto skill = s.u(obj + 0x70);
  auto level = s.u(obj + 0x68);
  auto stat0 = number(s.skill_virtual(
      skill, 13, {iv(0), iv(level), iv(obj + 0x4c), iv(10)}));
  auto stat2 = number(s.skill_virtual(skill, 10, {iv(2), iv(level)}));
  auto scale = div(mul(from_int(stat0), F_SCALE), from_int(stat2));
  auto rec = s.rec_of(owner);
  auto low = from_int(static_cast<std::uint32_t>(
      round_half_away(mul(from_int(s.u(rec + 0xd0)), scale))));
  auto high = from_int(static_cast<std::uint32_t>(
      round_half_away(mul(from_int(s.u(rec + 0xd4)), scale))));
  return {low, high};
}
Pair monster_bounds(World::State &s, Address obj) {
  auto skill = s.u(obj + 0x70);
  auto level = s.u(obj + 0x68);
  auto base = number(s.skill_virtual(
      skill, 13, {iv(0), iv(level), iv(obj + 0x4c), iv(10)}));
  auto amount = from_int(base);
  auto spread = mul(amount, F_SPREAD);
  auto sign = mul(spread, frac(s.own(obj)));
  auto magnitude = mul(spread, frac(s.own(obj)));
  auto extra = to_int(le(ZERO, sign) ? add(magnitude, HALF) : sub(magnitude, HALF));
  auto high_bits = static_cast<std::uint32_t>(
      static_cast<std::int64_t>(s32(base)) + extra);
  return {amount, from_int(high_bits)};
}
// world_collectObjectsInBox(low, high, 0x40), recorded with its arguments.
std::vector<Address> collect(World::State &s, const Vec3 &lo, const Vec3 &hi) {
  s.note("world_collectObjectsInBox", {lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], Raw(0x40)});
  return s.box(lo, hi, 0x40);
}
// Slot 1 with 3 on an object or a spell. The call is recorded first. Only a
// destructor this runtime already models runs; anything else refuses.
bool modelled_object_dtor(Raw fn) {
  switch (fn) {
  case 0x151454: case 0x3c6468: case 0x3cf534: case 0x3c5b90: case 0x3c2940:
  case 0x3c6888: case 0x117128: case 0x3cc67c:
    return true;
  default:
    return false;
  }
}
void delete_self(World::State &s, Address obj, const char *cls) {
  s.note("delete_self", {obj, Raw(3)});
  auto fn = s.u(s.u(obj + 0x48) + 12);
  if (!modelled_object_dtor(fn) && fn != DISEASE_CLOUD_DTOR &&
      !combat_ctors::is_class_dtor(fn))
    throw Unsupported(std::string(cls) +
                      " ends through its slot 1, whose destructor is not in the "
                      "research");
  s.destroy(obj);
}
void delete_spell(World::State &s, Address spell, const char *cls) {
  s.note("delete_spell", {spell, Raw(3)});
  if (!s.u(s.u(spell + 0x14) + 12))
    throw Unsupported(std::string(cls) +
                      " ends through spell slot 1, whose destructor is not in "
                      "the research");
  s.spell_virtual(spell, 1, {iv(3)});
}
// A sample count that is negative is not exercised: the machine's signed
// remainder and the reference's would differ. 0 with work to do divides by 0.
void sample_count_ok(std::int32_t count, bool used, const char *who) {
  if (count < 0)
    throw Unsupported(std::string(who) + " sees a negative sample count");
  if (used && count == 0)
    throw Unsupported(std::string(who) + " divides by a sample count of 0");
}
} // namespace

int World::State::is_near_view(Address pos, Raw radius) {
  // isNearView 0x1e77c0, ref_creature.is_near_view. Square test in x and y.
  auto px = u(pos), py = u(pos + 4);
  auto inside = [&](Raw cx, Raw cy) {
    return le(px, add(cx, radius)) && le(sub(cx, radius), px) &&
           le(py, add(cy, radius)) && le(sub(cy, radius), py);
  };
  if (s8m(G_CULL))
    return inside(u(G_FOCUS), u(G_FOCUS + 4)) ? 1 : 0;
  auto count = s32m(NUM_PLAYERS);
  int slot = 0;
  while (slot < count) {
    auto obj = u(PLAYERS + static_cast<Raw>(slot) * RECORD_SIZE + REC_OBJ);
    if (obj && inside(u(obj + 0x10), u(obj + 0x14)))
      break;
    ++slot;
  }
  return slot != s32m(NUM_PLAYERS) ? 1 : 0;
}
Address World::State::spell_block() {
  // A fixture block for the round-6 spells (see FIXTURE_SPELL_SIZE).
  auto spell = pool_new(FIXTURE_SPELL_SIZE);
  if (!spell)
    throw Unsupported("the pool refused a spell");
  spell_record(spell);
  return spell;
}
Address World::State::spawn_effect_fixture(Raw run, Raw size, const Vec3 &pos,
                                          Address owner) {
  auto obj = pool_new(size);
  if (!obj)
    throw Unsupported("the pool refused a fixture object");
  construct(obj, pos, Kind::Effect);
  auto table = alloc_fixture(0x30);
  for (unsigned k = 0; k < 0x30; k += 4)
    w(table + k, 0);
  w(table + 8 * 4 + 4, run);
  w(obj + 0x48, table);
  w(obj + 0x4c, 0);
  if (owner) {
    actor(owner);
    handle_link(obj + 0x4c, owner, owner);
  }
  return obj;
}
void World::State::poison_gas_run(Address obj) {
  // poisonGasSpell_run 0x1fd0f0.
  actor(obj);
  if (!(s32m(FRAME) < s32m(obj + 0x5c))) {
    delete_self(*this, obj, "poisonGasSpell");
    return;
  }
  auto target = s32m(obj + 0x60);
  if (lt(u(obj + 0x64), from_int(static_cast<std::uint32_t>(target))))
    w(obj + 0x64, add(u(obj + 0x64), HALF));
  auto count = target / 10;
  for (int made = 0; made < count; ++made) {
    auto ball = obj_ball(obj);
    auto radius = u(obj + 0x64);
    auto pos = vec(obj + 0x10);
    Vec3 point, velocity;
    for (unsigned k = 0; k < 3; ++k)
      point[k] = add(mul(ball[k], radius), pos[k]);
    ball = obj_ball(obj);
    for (unsigned k = 0; k < 3; ++k)
      velocity[k] = mul(ball[k], GAS_SPEED);
    particle(*this, GAS_PARTICLE, point, velocity);
  }
  if (s32m(FRAME) % 30)
    return;
  if (!u(obj + 0x4c))
    return;
  auto radius = from_int(static_cast<std::uint32_t>(to_int(u(obj + 0x64))));
  auto pos = vec(obj + 0x10);
  Vec3 lo, hi;
  for (unsigned k = 0; k < 3; ++k) {
    lo[k] = sub(pos[k], radius);
    hi[k] = add(pos[k], radius);
  }
  for (auto victim : collect(*this, lo, hi)) {
    auto node = u(obj + 0x4c);
    if (!node)
      continue;
    auto owner = u(node + 0xc);
    if (!number(vcall(owner, 20, {iv(victim)})))
      continue;
    auto value = u(obj + 0x6c) & 0xffff;
    damage_row(*this, victim, owner, GAS_FLAGS, true, 0x17, value, value, 0, ZERO,
               ZERO);
  }
}
void World::State::disease_corpse_run(Address obj) {
  // DiseaseCorpse_run 0x2ce4a4. DiseaseCloud_ctor is the round-3 body.
  actor(obj);
  auto radius = from_int(u(obj + 0x54));
  auto pos = vec(obj + 0x10);
  Vec3 lo, hi;
  for (unsigned k = 0; k < 3; ++k) {
    lo[k] = sub(pos[k], radius);
    hi[k] = add(pos[k], radius);
  }
  auto found = collect(*this, lo, hi);
  if (!found.empty()) {
    if (s32m(obj + 0x50) / 2 > 0) {
      int spawned = 0;
      std::size_t index = 0;
      while (index < found.size()) {
        auto victim = found[index];
        if ((u(victim + 0xc) & 0x28) == 0x28) {
          auto node = u(obj + 0x4c);
          if (node) {
            ++spawned;
            auto block = pool_new(DISEASE_CLOUD_SIZE);
            auto owner = u(node + 0xc);
            auto at = vec(victim + 0x10);
            if (block) {
              note("DiseaseCloud_ctor", {block, at[0], at[1], at[2], u(obj + 0x50), owner});
              diseasecloud_ctor(block, at, u(obj + 0x50), owner);
            }
          }
        }
        ++index;
        if (index >= found.size())
          break;
        if (!(spawned < s32m(obj + 0x50) / 2))
          break;
      }
    }
  }
  delete_self(*this, obj, "DiseaseCorpse");
}
void World::State::multifire_on_hit(Address obj, Address other) {
  // MultiFireProjectile_onHit 0x2da610. No already-ended test. A stat-2 result
  // of 0 is not exercised; div(ONE, ZERO) is the FPU's result if it happens.
  actor(obj);
  actor(other);
  auto node = u(obj + 0x4c);
  if (!node)
    return;
  auto owner = u(node + 0xc);
  Pair bounds;
  if (u(owner + 0xc) & 0x40) {
    auto flags = u(other + 0xc);
    if (flags & PLAYER_SKIP)
      return;
    if ((flags & 0x20) && (u(other + 0xa8) & CREATURE_SKIP))
      return;
    bounds = player_bounds(*this, obj, owner);
  } else {
    if (!number(vcall(owner, 20, {iv(other)})))
      return;
    bounds = monster_bounds(*this, obj);
  }
  damage_plain(*this, other, owner, HIT_FLAGS, bounds.low, bounds.high);
  multifire_end(obj, 2);
}
void World::State::multifire_end(Address obj, Raw reason) {
  // MultiFireProjectile_end 0x2da928.
  actor(obj);
  if (u(obj + 0x60))
    return;
  note("presentation:trail_release", {u(obj + 0x74)});
  if (reason) {
    auto ice = (u(G_LEVEL_FLAGS) & LEVEL_ICE) != 0;
    auto number_drawn = (own(obj) & 3) + 1;
    auto format = ice ? FMT_ICE : FMT_STONE;
    // The format text is an executable string and is not in this image, so the
    // printed name is recorded as (format, number) and res_find sees the lump
    // with an empty entry.
    note("libc_sprintf", {format, number_drawn});
    auto id = ice ? Raw(8) : Raw(2);
    auto pos = vec(obj + 0x10);
    note("presentation:fx_spawn", {id, pos[0], pos[1], pos[2]});
    auto entry = resource(FX_LUMP, "");
    note("presentation:sound_playOnObject", {entry, obj, ONE});
  }
  erase_later(obj);
  w(obj + 0x60, 1);
}
void World::State::smoke_gen_run(Address obj) {
  // SmokeGen_run 0x31c898. stepParticles stays a note.
  actor(obj);
  auto near = is_near_view(obj + 0x10, F_1000);
  note("isNearView", {obj + 0x10, F_1000, near ? Raw(1) : Raw(0)});
  if (!near)
    return;
  note("SmokeGen_stepParticles", {obj});
  auto amount = s32m(obj + 0x48c);
  if (amount <= 0 || (u(FRAME) & 7))
    return;
  auto reach = mul(u(obj + 0x478), F_3);
  auto pos = vec(obj + 0x10);
  Vec3 lo, hi;
  for (unsigned k = 0; k < 3; ++k) {
    lo[k] = sub(pos[k], reach);
    hi[k] = add(pos[k], reach);
  }
  auto harm = from_int(static_cast<std::uint32_t>(amount));
  for (auto victim : collect(*this, lo, hi)) {
    if (victim == obj)
      continue;
    auto flags = u(victim + 0xc);
    if ((flags & 1) || !(flags & 0x20))
      continue;
    damage_row(*this, victim, obj, SMOKE_FLAGS, true, 0x26, 1, 1, 0, harm, harm);
  }
}
void World::State::pustule_run(Address obj) {
  // Pustule_run 0x30bde0. The object's own generator is the only one drawn.
  actor(obj);
  auto timer = s32m(obj + 0x64);
  if (u(obj + 0x6c) &&
      lt(PUS_AGE, from_int(static_cast<std::uint32_t>(timer)))) {
    delete_self(*this, obj, "Pustule");
    return;
  }
  if (!(timer < PUS_LIFE)) {
    delete_self(*this, obj, "Pustule");
    return;
  }
  if (!u(obj + 0x6c)) {
    auto old = vec(obj + 0x10);
    w(obj + 0x60, add(u(obj + 0x60), PUS_GRAVITY));
    Vec3 step{};
    for (unsigned j = 0; j < 3; ++j) {
      auto now = add(u(obj + 0x10 + 4 * j), u(obj + 0x58 + 4 * j));
      w(obj + 0x10 + 4 * j, now);
      step[j] = sub(old[j], now);
    }
    for (int n = 0; n < 3; ++n) {
      auto share = frac(own(obj));
      auto pos = vec(obj + 0x10);
      Vec3 point;
      for (unsigned j = 0; j < 3; ++j)
        point[j] = add(pos[j], mul(step[j], share));
      particle(*this, PUS_FALL, point, {ZERO, ZERO, ZERO});
    }
    if (le(u(obj + 0x18), u(obj + 0x54))) {
      w(obj + 0x64, 0);
      w(obj + 0x6c, 1);
      auto entry = resource_at(PUS_LUMP, PUS_SOUND);
      auto at = vec(obj + 0x10);
      note("presentation:sound_playAtPos", {entry, at[0], at[1], at[2], ONE});
      auto base = at;
      // terrain_height(g_world, position; 8.0) is the height_below service.
      if (!services.height_below)
        throw Unsupported("terrain_height service missing");
      auto ground = services.height_below(vec(obj + 0x10), 0x41000000);
      base[2] = add(ground, 0x41400000);
      for (int i = 0; i < 10; ++i) {
        auto radius = frac(own(obj));
        auto angle = own(obj);
        auto x = add(base[0], mul(mul(radius, trig::cos16(angle)), 0x40c3693c));
        auto y = add(base[1], mul(mul(radius, trig::sin16(angle)), 0x40c3693c));
        auto angle2 = own(obj);
        auto spin = mul(frac(own(obj)), 0x3f49fb1e);
        auto vx = mul(mul(trig::cos16(angle2), spin), 0x408daf39);
        auto vy = mul(mul(trig::sin16(angle2), spin), 0x408daf39);
        vec(obj + 0x70 + 12 * Raw(i), {x, y, base[2]});
        vec(obj + 0xe8 + 12 * Raw(i), {vx, vy, mul(ONE, 0x408daf39)});
      }
    }
    if (!u(obj + 0x6c)) {
      w(obj + 0x64, (u(obj + 0x64) + 1) & 0xffffffffu);
      return;
    }
  }
  for (int i = 0; i < 10; ++i) {
    auto place = obj + 0x70 + 12 * Raw(i);
    auto velocity = obj + 0xe8 + 12 * Raw(i);
    for (unsigned j = 0; j < 3; ++j)
      w(place + 4 * j, add(u(place + 4 * j), u(velocity + 4 * j)));
    w(velocity + 8, add(u(velocity + 8), PUS_DROP_GRAVITY));
    auto scale = mul(frac(own(obj)), 0x40a729f7);
    Vec3 speed;
    for (unsigned j = 0; j < 3; ++j)
      speed[j] = mul(u(velocity + 4 * j), scale);
    auto ball = obj_ball(obj);
    Vec3 point;
    for (unsigned j = 0; j < 3; ++j)
      point[j] = add(mul(ball[j], 0x403fee34), u(place + 4 * j));
    particle(*this, PUS_DROP, point, speed);
  }
  w(obj + 0x64, (u(obj + 0x64) + 1) & 0xffffffffu);
}
void World::State::arena_run(Address spell, const Vec3 &) {
  // SpellArena_run 0x305bbc. The position argument is not read again.
  // A positive particle count with a sample count of 0 divides by 0. That is
  // not exercised; the reference raises after the first draw, and so does this.
  auto counter = s16m(G_ARENA_COUNTER);
  auto creature = u(spell + 0xc);
  auto flags = u(creature + 0xc);
  w(creature + 0xc, counter < 0x3d ? (flags & ~Raw(2)) : (flags | 2));
  if (counter < 0x1e)
    return;
  auto fade = sub(ONE, mul(sub(from_int(static_cast<std::uint32_t>(counter)),
                               0x41f00000),
                           0x3bda740d));
  if (!services.sample_point_count)
    throw Unsupported("model sample point service missing");
  auto count = s32(services.sample_point_count(number(vcall(u(spell + 0xc), 16))));
  auto limit = to_int(mul(fade, 0x41b80000));
  sample_count_ok(count, false, "SpellArena_run");
  for (int index = 0; index < limit; ++index) {
    auto model = number(vcall(u(spell + 0xc), 16));
    auto draw = spell_rand(spell);
    creature = u(spell + 0xc);
    // The reference raises here, after the draw, on a count of 0.
    sample_count_ok(count, true, "SpellArena_run");
    auto sample = static_cast<Raw>(s32(draw) % count);
    if (!services.sample_point)
      throw Unsupported("model sample point service missing");
    auto point = services.sample_point(model, creature + 0x114, vec(creature + 0x10),
                                       Raw(s16m(creature + 0x38)), sample, ONE);
    auto speed = mul(sub(ONE, fade), 0x41200000);
    auto angle = spell_rand(spell);
    Vec3 velocity{mul(trig::cos16(angle), speed), mul(trig::sin16(angle), speed),
                  mul(ZERO, speed)};
    Vec3 place;
    for (unsigned j = 0; j < 3; ++j)
      place[j] = sub(point[j], mul(velocity[j], 0x40f25fdc));
    particle(*this, (index & 3) == 0 ? ARENA_A : ARENA_B, place, velocity);
  }
  creature = u(spell + 0xc);
  for (unsigned slot = 0; slot < 8; ++slot)
    w(creature + 0x90 + 2 * slot, 2, ARENA_TINT[slot & 3]);
}
Address World::State::hornomen_ctor(Raw kind, Address owner, Raw extra) {
  // SpellHornOmenEffect_ctor 0x2fc574. One shared draw, inlined.
  auto spell = spell_block();
  w(spell, 1, kind);
  w(spell + 0x14, VT_SPELL);
  w(spell + 0xc, owner);
  w(spell + 8, extra);
  w(spell + 4, 0);
  w(spell + 0x14, HO_VT);
  w(spell + 0x10, draw());
  w(spell, 1, kind);
  w(spell + 0xc, owner);
  w(spell + 8, extra);
  if (!services.particle_group)
    throw Unsupported("particleGroup_create service missing");
  auto group = services.particle_group();
  note("particleGroup_create", {group});
  w(spell + 0x1c, group);
  w(spell + 0x18, (u(FRAME) + 600) & 0xffffffffu);
  return spell;
}
void World::State::hornomen_run(Address spell, const Vec3 &) {
  // SpellHornOmenEffect_run 0x2fc684. mat34_setPosRotZ's matrix is what the
  // original leaves on the stack. The model passes that stack slot through and
  // does not read it back, so the note records the group handle only.
  if (!(s32m(FRAME) < s32m(spell + 0x18))) {
    delete_spell(*this, spell, "SpellHornOmenEffect");
    return;
  }
  auto age = from_int(static_cast<std::uint32_t>(s32m(FRAME) - s32m(spell + 0x18) + 600));
  auto creature = u(spell + 0xc);
  auto early = lt(age, 0x430c0000);
  std::int32_t red, green, blue;
  if (early) {
    auto scale = mul(age, 0x3bea0ea0);
    red = to_int(mul(scale, 0x43610000));
    green = to_int(mul(scale, 0x41a00000));
    blue = to_int(mul(scale, 0x42c80000));
  } else {
    red = 0xe1;
    green = 0x14;
    blue = 100;
  }
  for (Address base : {Address(0x90), Address(0x98)}) {
    w(creature + base, 2, static_cast<std::uint32_t>(red));
    w(creature + base + 2, 2, static_cast<std::uint32_t>(green));
    w(creature + base + 4, 2, 0);
    w(creature + base + 6, 2, static_cast<std::uint32_t>(blue));
  }
  note("particleGroup_setMatrix", {u(spell + 0x1c)});
  note("particleGroup_begin", {u(spell + 0x1c)});
  if (!services.sample_point_count)
    throw Unsupported("model sample point service missing");
  auto count = s32(services.sample_point_count(number(vcall(creature, 16))));
  sample_count_ok(count, false, "SpellHornOmenEffect_run");
  if (count) {
    int made = 0;
    while (true) {
      if (early) {
        if (!lt(from_int(static_cast<std::uint32_t>(made)),
                mul(mul(age, 0x3bea0ea0), 0x43160000)))
          break;
      } else if (made > 0x95) {
        break;
      }
      auto model = number(vcall(u(spell + 0xc), 16));
      auto sample = spell_rand(spell) % static_cast<Raw>(count);
      if (!services.sample_point)
        throw Unsupported("model sample point service missing");
      auto point = services.sample_point(model, u(spell + 0xc) + 0x114,
                                         {ZERO, ZERO, ZERO}, 0, sample, ONE);
      auto ball = spell_ball_add(spell);
      Vec3 place;
      for (unsigned j = 0; j < 3; ++j)
        place[j] = add(point[j], mul(ball[j], 0x40400ce4));
      ball = spell_ball_add(spell);
      Vec3 velocity;
      for (unsigned j = 0; j < 3; ++j)
        velocity[j] = mul(ball[j], 0x41f00099);
      particle(*this, HO_PARTICLE, place, velocity);
      ++made;
    }
  }
  note("particleGroup_end", {u(spell + 0x1c)});
}
Address World::State::planarsteel_ctor(Raw kind, Address owner, Raw extra) {
  // SpellPlanarSteelEffect_ctor 0x2fb728.
  auto spell = spell_block();
  w(spell, 1, kind);
  w(spell + 0x14, VT_SPELL);
  w(spell + 4, 0);
  auto seed = draw();
  w(spell + 0xc, owner);
  w(spell + 8, extra);
  w(spell + 0x10, seed);
  w(spell + 0x14, PS_VT);
  auto light = spell + 0x30;
  auto inverse = div(ONE, ZERO);
  w(light + 9, 1, 0);
  vec(spell + 0x60, vec(owner + 0x10));
  w(light + 0x28, inverse);
  w(light + 0x2c, 0x3f266666);
  constexpr Raw colour[] = {0x4b, 0x4b, 200, 0xff, 0x4b, 0x4b, 200, 0xff};
  for (unsigned i = 0; i < 8; ++i)
    w(light + 0xa + 2 * i, 2, colour[i]);
  w(light + 0x20, inverse);
  w(light + 0x1c, ZERO);
  w(light + 0x24, ZERO);
  w(light + 8, 1, 0);
  note("presentation:light_link", {light});
  w(spell, 1, kind);
  w(spell + 8, extra);
  w(spell + 0xc, owner);
  w(spell + 0x20, 0);
  w(spell + 0x24, (u(FRAME) + 0xf0) & 0xffffffffu);
  auto ticks = to_int(mul(u(owner + 0x20), 0x40400000));
  w(spell + 0x1c, 2, static_cast<std::uint32_t>(ticks) & 0xffff);
  w(spell + 0x18, 2, 0);
  w(spell + 0x1a, 2, 0x7fff);
  auto entry = resource_at(PS_LUMP, PS_SOUND_START);
  auto at = vec(owner + 0x10);
  note("presentation:sound_playAtPos", {entry, at[0], at[1], at[2], ONE});
  return spell;
}
void World::State::planarsteel_run(Address spell, const Vec3 &pos) {
  // SpellPlanarSteelEffect_run 0x2fb9b4.
  if (!(s32m(FRAME) < s32m(spell + 0x24))) {
    delete_spell(*this, spell, "SpellPlanarSteelEffect");
    return;
  }
  auto sway = trig::sin16(static_cast<std::uint32_t>(s32m(FRAME)) * 1000u);
  auto creature = u(spell + 0xc);
  vec(spell + 0x60, vec(creature + 0x10));
  note("presentation:light_relink", {spell + 0x30});
  auto radius = add(from_int(static_cast<std::uint32_t>(s8m(spell) + 0x42)),
                    mul(sway, 0x41200000));
  auto share = mul(sway, 0x42440000);
  auto high = to_int(add(share, 0x43480000));
  auto low = to_int(add(mul(sway, 0x40a00000), 0x41700000));
  auto mid = to_int(add(share, 0x42960000));
  w(spell + 0x4c, radius);
  w(spell + 0x54, radius);
  auto inverse = div(ONE, mul(radius, radius));
  creature = u(spell + 0xc);
  for (Address base : {Address(0x90), Address(0x98)}) {
    w(creature + base, 2, static_cast<std::uint32_t>(mid));
    w(creature + base + 2, 2, static_cast<std::uint32_t>(mid));
    w(creature + base + 4, 2, static_cast<std::uint32_t>(high));
    w(creature + base + 6, 2, static_cast<std::uint32_t>(low));
  }
  w(spell + 0x58, inverse);
  w(spell + 0x50, inverse);
  auto countdown = (h(spell + 0x1c) - 1) & 0xffff;
  w(spell + 0x1c, 2, countdown);
  if ((countdown & 0x8000) && !u(spell + 0x20)) {
    w(spell + 0x20, 1);
    int angle = 0;
    for (int n = 0; n < 60; ++n) {
      auto bits = static_cast<std::uint32_t>(angle);
      auto vx = mul(trig::cos16(bits), 0x42c80000);
      auto vy = mul(trig::sin16(bits), 0x42c80000);
      auto vz = mul(frac(spell_rand(spell)), 0x41a00000);
      creature = u(spell + 0xc);
      Vec3 place{add(u(creature + 0x10), ZERO), u(creature + 0x14),
                 add(u(creature + 0x18), 0x41900000)};
      particle(*this, PS_BURST, place, {vx, vy, vz});
      angle = s16(static_cast<std::uint32_t>(angle + 0x444));
    }
    auto entry = resource_at(PS_LUMP, PS_SOUND_BURST);
    auto at = vec(u(spell + 0xc) + 0x10);
    note("presentation:sound_playAtPos", {entry, at[0], at[1], at[2], ONE});
  }
  for (int slot = 0; slot < 2; ++slot) {
    auto angle = (h(spell + 0x18 + 2 * Raw(slot)) - 0x5dc) & 0xffff;
    w(spell + 0x18 + 2 * Raw(slot), 2, angle);
    auto x = mul(trig::cos16(static_cast<std::uint32_t>(s16(angle))), 0x42200000);
    auto y = mul(trig::sin16(static_cast<std::uint32_t>(s16m(spell + 0x18 + 2 * Raw(slot)))),
                 0x42200000);
    auto lifted = mul(sway, 0x42200000);
    creature = u(spell + 0xc);
    auto height = from_int(static_cast<std::uint32_t>(s16m(spell + 0x1c)));
    auto colour = to_int(add(lifted, 0x42480000));
    auto blue = to_int(add(lifted, 0x42c80000));
    Vec3 point{add(x, u(creature + 0x10)), add(y, u(creature + 0x14)),
               add(height, u(creature + 0x18))};
    note("presentation:glow_add", {point[0], point[1], point[2],
                     static_cast<Raw>(static_cast<std::uint32_t>(colour)),
                     static_cast<Raw>(static_cast<std::uint32_t>(colour)),
                     static_cast<Raw>(static_cast<std::uint32_t>(blue)),
                     static_cast<Raw>(static_cast<std::uint32_t>(colour)),
                     u(G_GLOW_TEX_C), 0, 2, ZERO, 0x457a0000});
    // Six 0x8e6b00 at 0x3ecccccc, then four 0x8e6b50 at 0x40c00000.
    for (auto spark : {std::pair<Address, std::pair<int, Raw>>{PS_SPARK, {6, 0x3ecccccc}},
                       std::pair<Address, std::pair<int, Raw>>{PS_SPARK2, {4, 0x40c00000}}}) {
      for (int n = 0; n < spark.second.first; ++n) {
        auto ball = spell_ball_add(spell);
        Vec3 velocity;
        for (unsigned j = 0; j < 3; ++j)
          velocity[j] = mul(ball[j], spark.second.second);
        ball = spell_ball_add(spell);
        Vec3 place;
        for (unsigned j = 0; j < 3; ++j)
          place[j] = add(add(ball[j], ball[j]), point[j]);
        particle(*this, spark.first, place, velocity);
      }
    }
  }
  for (int n = 0; n < 10; ++n) {
    auto angle = static_cast<std::uint32_t>(s16(spell_rand(spell)));
    auto radius = mul(frac(spell_rand(spell)), 0x42200000);
    auto x = mul(trig::cos16(angle), radius);
    auto y = mul(trig::sin16(angle), radius);
    auto z = add(add(0x43480000, from_int(spell_rand(spell) % 200)), 0xc2c80000);
    Vec3 place{add(x, pos[0]), add(y, pos[1]), add(z, pos[2])};
    particle(*this, PS_FALL, place, {ZERO, ZERO, 0xc2a00000});
  }
  for (int n = 0; n < 10; ++n) {
    auto angle = static_cast<std::uint32_t>(s16(spell_rand(spell)));
    auto radius = mul(frac(spell_rand(spell)), 0x42200000);
    auto x = mul(trig::cos16(angle), radius);
    auto y = mul(trig::sin16(angle), radius);
    auto vx = add(from_int(spell_rand(spell) & 1), MINUS_ONE);
    auto vy = add(from_int(spell_rand(spell) & 1), MINUS_ONE);
    Vec3 place{add(x, pos[0]), add(y, pos[1]), add(ZERO, pos[2])};
    if (!services.height_at_point)
      throw Unsupported("terrain_heightAtPoint service missing");
    auto ground = services.height_at_point(place);
    place[2] = add(ground, 0x40a00000);
    particle(*this, PS_SPARK, place, {vx, vy, ZERO});
  }
}
} // namespace champions::simulation::rta::combat_runtime
