#include "combat_runtime_internal.hpp"
// Round 3 (research ref_combat_round3 at 22649cd, docs/spec/combat-round3.md).
// DiseaseCloud, the DiseaseEffect object damage_apply builds for a type-0x26
// entry, GutChunk, the Strong Stance phantom, and MissileWeapon kinds 1, 2, 5
// and 6. Kinds 0 and 3 stay in missile_run. Potions, the Holy Shield light and
// silent block sparks are the round-4 bodies. Deflection, the bow release,
// GutChunk's run, the Strong Stance trigger's light and particles, Fire
// construction, and the remaining skill paths stay refused. So do the
// call-outs the research leaves unexecuted: GutChunk's PhProp_ctor, the
// phantom's Creature_ctor, ParticleProp_ctor, fx_spawn 5, DiseaseCloud_dtor,
// and the cloud's damage_apply on itself (2026-10-21 audit CR3-1..CR3-6).
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Raw F_5 = 0x40a00000, F_10 = 0x41200000, F_24 = 0x41c00000,
              F_INV65536 = 0x37800000, F_RING2 = 0x40000000,
              F_LIGHT_A = 0x437f0000, F_LIGHT_B = 0x431b0000,
              F_LIGHT_C = 0xc2000000, F_LIGHT_D = 0x43580000,
              F_LIGHT_E = 0x42f00000, F_OUTER72 = 0x42900000,
              F_KIND6_SCALE = 0x41accccc, F_KIND6_BIAS = 0x3fd10624,
              F_KIND1_LEAD = 0x3d75c28f, F_KIND1_SMALL = 0x3c23d70a,
              F_KIND1_STEP = 0x3dcccccc, F_KIND1_DIR = 0x40800000,
              F_KIND5_DRAG = 0x3da3d70a, F_KIND5_DIR = 0x41421062;
constexpr Address G_SKILL_DISEASE = 0x41b1e4, S_FX_LUMP = 0x532478,
                  S_KIND12_IMPACT = 0x5324e0, S_KIND5_IMPACT = 0x5324f8,
                  S_KIND6_IMPACT = 0x532518, P_KIND5 = 0x6e00c0,
                  P_KIND1_A = 0x6dffd0, P_KIND1_B = 0x6e0020, P_KIND6 = 0x6e0070;
constexpr Address DC_PARTICLES[3] = {0x8e5320, 0x8e5370, 0x8e53c0};
constexpr Raw DC_SPEEDS[3] = {0x40c00000, 0x40c00000, 0x41400000};
Raw unit_coord(Raw draw) {
  auto m = mul(from_int(draw), F_INV65536);
  return sub(add(m, m), ONE);
}
combat::Descriptor disease_row(Raw value) {
  combat::Descriptor d(4, std::vector<std::int64_t>{-1, 0, 0, 0});
  auto v = s16(value & 0xffff);
  d[0] = {s16(0x26), v, v, 0};
  return d;
}
} // namespace
Address World::State::diseasecloud_spawn(const Vec3 &pos, Raw level,
                                         Address owner) {
  auto obj = pool_new(DISEASE_CLOUD_SIZE);
  if (!obj)
    throw Unsupported("the pool refused a DiseaseCloud");
  diseasecloud_ctor(obj, pos, level, owner);
  return obj;
}
void World::State::diseasecloud_ctor(Address obj, const Vec3 &pos, Raw level,
                                     Address owner) {
  // DiseaseCloud_ctor 0x2ce610 (round 3). GameObject_ctor (one shared draw),
  // then the cloud fields. +0x28/+0x29 are the constructor's draw-size bytes;
  // construct() leaves them clear. Round 6's corpse passes this the pool block.
  construct(obj, pos, Kind::Effect);
  // Slot words stay out of the image until a cloud exists. Other slots stay 0.
  w(VT_DISEASE_CLOUD + 4, 0x3cd4ec);
  w(VT_DISEASE_CLOUD + 12, DISEASE_CLOUD_DTOR);
  w(VT_DISEASE_CLOUD + 36, DISEASE_CLOUD_RUN);
  w(obj + 0x28, 1, 0x19);
  w(obj + 0x29, 1, 0x49);
  w(obj + 0x48, VT_DISEASE_CLOUD);
  w(obj + 0x4c, 0);
  if (owner) {
    actor(owner);
    handle_link(obj + 0x4c, owner, owner);
  }
  w(obj + 0x50, level);
  w(obj + 0x58, 0x30);
  w(obj + 0x54, u(FRAME) + 0xf0);
  w(obj + 0x60, 0x14);
  w(obj + 0x5c, F_10);
  w(obj + 12, u(obj + 12) | 3);
  note("obj_runEveryFrame", {obj});
  schedule(obj, 1);
}
void World::State::diseasecloud_run(Address obj) {
  actor(obj);
  if (!(s32m(FRAME) < s32m(obj + 0x54))) {
    vcall(obj, 1, {iv(3)});
    return;
  }
  auto cap = s32m(obj + 0x58);
  if (lt(u(obj + 0x5c), from_int(cap)))
    w(obj + 0x5c, add(u(obj + 0x5c), HALF));
  auto count = cap / 10;
  for (unsigned stream = 0; stream < 3; ++stream) {
    for (int i = 0; i < count; ++i) {
      auto ball = obj_ball(obj);
      auto radius = u(obj + 0x5c);
      auto pos = vec(obj + 0x10);
      Vec3 point, velocity;
      for (unsigned k = 0; k < 3; ++k)
        point[k] = add(mul(ball[k], radius), pos[k]);
      ball = obj_ball(obj);
      for (unsigned k = 0; k < 3; ++k)
        velocity[k] = mul(ball[k], DC_SPEEDS[stream]);
      note("presentation:particle_spawn",
           {DC_PARTICLES[stream], point[0], point[1], point[2], velocity[0],
            velocity[1], velocity[2]});
    }
  }
  if (u(FRAME) % 30)
    return;
  auto radius = from_int(to_int(u(obj + 0x5c)));
  auto pos = vec(obj + 0x10);
  Vec3 lo, hi;
  for (unsigned k = 0; k < 3; ++k) {
    lo[k] = sub(pos[k], radius);
    hi[k] = add(pos[k], radius);
  }
  for (auto victim : box(lo, hi, 0x40)) {
    // ref_combat_round3.diseasecloud_run skips only the owner and two players.
    // The cloud is in grid A (GameObject_ctor) inside its own box, so it is
    // handed to damage_apply too (non-creature: one shared draw, then slot 2
    // of 0x5186f0). That slot is not documented; refuse there.
    auto node = u(obj + 0x4c);
    if (!node)
      continue;
    auto cloud_owner = u(node + 0xc);
    if (cloud_owner == victim)
      continue;
    if ((u(cloud_owner + 12) & 0x40) && (u(victim + 12) & 0x40))
      continue;
    if (victim == obj)
      throw Unsupported("DiseaseCloud damage_apply on itself: slot 2 of table "
                        "0x5186f0 is not documented");
    auto value = u(obj + 0x60) & 0xffff;
    damage_with_descriptor(*this, victim, 0, 100000, 0x1000040,
                           disease_row(value), 0, 0);
  }
}
void World::State::make_disease_effect(Address block, Address victim,
                                       Address attacker, Raw seed_value) {
  // SpellDiseaseEffect_ctor 0x2d5594. seed_value is the shared draw the Spell
  // base stores (the draw return, not the full generator state). The damage
  // path has already drawn it; this function does not draw again.
  actor(victim);
  spell_record(block);
  w(block, 1, 0);
  w(block + 4, 0);
  w(block + 8, 0);
  w(block + 0xc, victim);
  w(block + 0x10, seed_value);
  w(VT_DISEASE_SPELL + 4, DISEASE_GETTER);
  w(VT_DISEASE_SPELL + 20, DISEASE_RUN);
  w(block + 0x14, VT_DISEASE_SPELL);
  w(block + 0x18, 0);
  if (attacker) {
    actor(attacker);
    handle_link(block + 0x18, attacker, attacker);
  }
  auto seconds =
      to_int(sub(F_5, mul(u(victim + 0x68), F_5)));
  w(block + 0x1a4, Raw(std::uint32_t(seconds)));
  w(block + 0x19c, victim);
  w(block + 0x1a0,
    u(FRAME) + Raw(std::uint32_t(std::int64_t(seconds) * 60)));
  auto radius = u(u(block + 0xc) + 0x1c);
  for (int i = 0; i < 16; ++i) {
    auto b = spell_ball(block);
    for (unsigned k = 0; k < 3; ++k)
      w(block + 0x1c + 12 * Raw(i) + 4 * k, mul(b[k], radius));
    b = spell_ball(block);
    for (unsigned k = 0; k < 3; ++k)
      w(block + 0xdc + 12 * Raw(i) + 4 * k, b[k]);
  }
  w(block + 0x1ac, u(G_SKILL_DISEASE));
  w(block + 0x1a8, 0);
}
Address World::State::disease_attach(Address victim, Address attacker) {
  // The disease branch on its own: one pool, one shared draw when a block
  // comes back, creature_addSpell even when the pool returned nothing.
  actor(victim);
  auto block = pool_new(DISEASE_EFFECT_SIZE);
  if (block)
    make_disease_effect(block, victim, attacker, draw());
  note("creature_addSpell", {victim, block});
  if (block)
    handle_link(victim + 0x364, block + 4, block);
  return block;
}
void World::State::gutchunk_ctor(Address obj, const Vec3 &pos, Raw kind, Raw bits) {
  // Direct host spawning enters here without the service-level constructor
  // dispatch used by the AntQueen bleed and Skeleton shatter paths.
  for (auto pair:{std::pair<unsigned,Raw>{1,0x3c4d48},{4,0x1edb18}}) {
    w(0x50f3b8+8*pair.first,2,0);
    w(0x50f3b8+8*pair.first+4,pair.second);
  }
  for (auto pair:{std::pair<unsigned,Raw>{1,0x3c2fc8},{4,0x155fcc}}) {
    w(0x511ba8+8*pair.first,2,0);
    w(0x511ba8+8*pair.first+4,pair.second);
  }
  combat_ctors::gutchunk_ctor(model,obj,pos,kind,bits);
  prop_model_pointers[obj] = u(obj + 0x4c);
}
Address World::State::gutchunk_spawn(const Vec3 &pos, Raw kind, Raw bits) {
  // Direct host entry has no earlier creature constructor to register the
  // composed GameObject preparation callback used by the PhProp chain.
  install_ctor_services();
  auto obj = pool_new(GUTCHUNK_SIZE);
  if (!obj)
    throw Unsupported("the pool refused a GutChunk");
  gutchunk_ctor(obj, pos, kind, bits);
  return obj;
}
void World::State::phantom_ctor(Address, Address target, const std::string &,
                               Raw, Raw) {
  // StrongStancePhantom_ctor 0x1618d4 begins with Creature_ctor 0x109d68
  // (phantom, 0, the target's position and facing, its class definition +0x24,
  // 0, 0), a callback of ref_combat_round3. The research body
  // (ref_arena_monsters.creature_ctor) starts with GameObject_ctor on that
  // class definition (names registered, one shared draw), which the combat
  // World does not compose (the GameObject_ctor service refuses a
  // definition). The executed tail of the phantom constructor is not reached.
  actor(target);
  throw Unsupported("Creature_ctor 0x109d68 (StrongStancePhantom_ctor 0x1618d4): "
                    "GameObject_ctor with a class definition is not composed");
}
Address World::State::phantom_spawn(Address target, const std::string &name,
                                   Raw speed, Raw flag) {
  auto obj = pool_new(PHANTOM_SIZE);
  if (!obj)
    throw Unsupported("the pool refused a StrongStancePhantom");
  phantom_ctor(obj, target, name, speed, flag);
  return obj;
}
void World::State::phantom_run(Address obj) {
  actor(obj);
  advance_anim(obj, ONE);
  knockback(obj, HALF);
  move(obj, Vec3{0, 0, 0});
  auto left = sub(u(obj + 0x3cc), ONE);
  w(obj + 0x3cc, left);
  if (lt(left, ONE))
    erase_later(obj);
}
void World::State::missile_arrow_run(Address self, std::int32_t kind) {
  // MissileWeapon_run 0x15153c for kinds 1, 2, 5 and 6. The carried-arrow and
  // already-splashed exits are the shared prefix in missile_run.
  auto spin = h(self + 0xfa);
  if (s16(spin))
    w(self + 0xfc, 2, h(self + 0xfc) + spin);
  auto result = move_and_collide(self, vec(self + 0x1b0));
  auto res = result & ~2u;
  if (s8m(self + 0xcc))
    res |= 2;
  auto ball = [&] {
    for (unsigned attempt = 0; attempt < 65536; ++attempt) {
      auto x = unit_coord(own(self)), y = unit_coord(own(self)),
           z = unit_coord(own(self));
      if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z))))
        return Vec3{x, y, z};
    }
    throw Error("ball rejection budget");
  };
  auto light_of = [&](const Vec3 &pos, bool kind12) {
    auto block = pool_new(0xbc);
    if (!block)
      return;
    Raw c0 = kind12 ? F_LIGHT_A : 0, c1 = F_LIGHT_B,
        c2 = kind12 ? 0 : F_LIGHT_A, c3 = F_LIGHT_C;
    // research-combat-ctors-3 combat-round3.md:111-119 pins these colours and
    // stack floats. This missile call's follow and tag arguments were not
    // executed there; zero is inferred from the prior World path and the
    // zero-initialized block. arrow-hit.md:3 pins zero only for Explode.
    note("LightEffect_ctor", {block, pos[0], pos[1], pos[2], c0, c1, c2, c3, 0,
                              0, 0, 0, F_LIGHT_D, F_LIGHT_E, ONE, ONE});
    combat_ctors::light_effect_ctor(model,block,pos,0,0,
        {c0,c1,c2,c3,0,0,0,0,F_LIGHT_D,F_LIGHT_E,ONE,ONE});
  };
  if (res) {
    Raw sample = kind == 5 ? S_KIND5_IMPACT
                           : kind == 6 ? S_KIND6_IMPACT : S_KIND12_IMPACT;
    resource_at(S_FX_LUMP, sample);
    note("presentation:sound_playOnObject", {sample, self, ONE});
    w(self + 0xec, 0);
    auto pos = vec(self + 0x10);
    // fx_spawn id 5 builds 3 or 4 Fire objects on the shared seed
    // (research-fx docs/effects.md row 5 and section 17). No Fire body is
    // executed, so a World-only host refuses here as the playable observer does.
    if (res == 1 && kind == 1)
      throw Unsupported("fx_spawn id 5 (Fire construction) on a kind-1 arrow hit");
    if (res == 1) {
      auto impact = u(self + 0x1bc);
      if (impact)
        note("presentation:sound_playSample",
             {impact, pos[0], pos[1], pos[2], ONE, ONE, 0});
    }
    pos = vec(self + 0x10);
    if (kind == 1) {
      auto n = int(own(self) & 3) + 3;
      for (int step = 0; step < n; ++step) {
        // ParticleProp_ctor 0x1549e8 (block, position, 8) is a call-out of
        // ref_combat_round3 and "body not read" in symbols/objects.tsv. A
        // ParticleProp is a 0x148-byte Prop (creatures.md), so its
        // GameObject_ctor draws the shared seed; refuse instead of dropping it.
        auto block = pool_new(0x148);
        if (block)
          throw Unsupported("ParticleProp_ctor 0x1549e8 (kind-1 arrow hit): body not read");
      }
      light_of(pos, true);
    } else if (kind == 2) {
      note("presentation:fx_ringBurst", {pos[0], pos[1], pos[2], F_RING2});
      light_of(pos, true);
      auto node = u(self + 0x4c);
      Address attacker = node ? u(node + 0xc) : 0;
      damage_in_radius(attacker, pos, u(self + 0xd8), 0, 0, 0, F_24, F_OUTER72);
    } else if (kind == 5) {
      note("presentation:fx_spawn", {10, pos[0], pos[1], pos[2]});
      light_of(pos, false);
    } else {
      for (int n = 0; n < 100; ++n) {
        auto d = ball();
        note("presentation:particle_spawn",
             {P_KIND6, pos[0], pos[1], pos[2], mul(d[0], F_KIND6_SCALE),
              mul(d[1], F_KIND6_SCALE),
              add(mul(d[2], F_KIND6_SCALE), F_KIND6_BIAS)});
      }
      light_of(pos, false);
    }
  }
  if (kind == 1) {
    auto pos = vec(self + 0x10);
    auto vel = vec(self + 0x1b0);
    Vec3 base;
    for (unsigned i = 0; i < 3; ++i)
      base[i] = add(pos[i], mul(vel[i], F_KIND1_LEAD));
    for (int n = 0; n < 24; ++n) {
      auto spread = ball();
      Vec3 place;
      for (unsigned i = 0; i < 3; ++i) {
        auto jitter_i = std::int32_t(own(self) % 40) - 20;
        auto jitter = mul(from_int(Raw(std::uint32_t(jitter_i))), F_KIND1_STEP);
        auto drag = mul(mul(vel[i], F_KIND1_SMALL), spread[i]);
        place[i] = add(add(base[i], jitter), drag);
      }
      auto d = ball();
      Vec3 direction{mul(d[0], F_KIND1_DIR), mul(d[1], F_KIND1_DIR),
                     mul(d[2], F_KIND1_DIR)};
      note("presentation:particle_spawn",
           {P_KIND1_A, place[0], place[1], place[2], direction[0], direction[1],
            direction[2]});
      Vec3 shifted;
      for (unsigned i = 0; i < 3; ++i)
        shifted[i] = sub(place[i], mul(vel[i], F_KIND1_LEAD));
      note("presentation:particle_spawn",
           {P_KIND1_B, shifted[0], shifted[1], shifted[2], direction[0],
            direction[1], direction[2]});
    }
  } else if (kind == 6) {
    auto node = u(self + 0x4c);
    Address owner = node ? u(node + 0xc) : 0;
    if (owner && (u(owner + 12) & 0x40))
      note("player_getPassiveLevel",
           {u(owner + 0x3c8), 0x1000,
            Raw(std::uint32_t(passive_level(u(owner + 0x3c8), 0x1000)))});
  } else if (kind == 5) {
    auto pos = vec(self + 0x10);
    auto vel = vec(self + 0x1b0);
    Vec3 shifted;
    for (unsigned i = 0; i < 3; ++i)
      shifted[i] = add(pos[i], mul(vel[i], F_KIND5_DRAG));
    for (int n = 0; n < 32; ++n) {
      auto p = ball();
      Vec3 centre{add(shifted[0], mul(p[0], F_10)),
                  add(shifted[1], mul(p[1], F_10)),
                  add(shifted[2], mul(p[2], F_10))};
      auto d = ball();
      note("presentation:particle_spawn",
           {P_KIND5, centre[0], centre[1], centre[2], mul(d[0], F_KIND5_DIR),
            mul(d[1], F_KIND5_DIR), mul(d[2], F_KIND5_DIR)});
    }
  }
  wvec(self + 0x1a4, vec(self + 0x10));
  note("presentation:light_relink", {self + 0x174});
  if (!(s32m(FRAME) < s32m(self + 0xec)))
    vcall(self, 1, {iv(3)});
}
} // namespace champions::simulation::rta::combat_runtime
