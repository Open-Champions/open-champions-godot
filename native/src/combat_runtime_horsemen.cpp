#include "combat_runtime_internal.hpp"

// Synthetic placement only. The fixture supplies HP and damage statistics;
// the executed Horsemen/Nightmare run and set-state bodies own all later work.
// Class-definition clip words here are opaque animation tokens supplied by
// this fixture, not claims about loaded game resources.
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Address VT_HORSEMEN = 0x515730, VT_NIGHTMARE = 0x5155e8;
constexpr Raw HORSEMEN_RUN = 0x27236c, HORSEMEN_STATE = 0x272f4c,
              NIGHTMARE_RUN = 0x270bbc, NIGHTMARE_STATE = 0x27155c;
}

std::array<Address, 2> World::State::spawn_mounted_horseman(
    unsigned variant, const Spawn &rider, const Spawn &mount) {
  if (!full())
    throw Unsupported("mounted Horsemen need the Full combat composition");
  if (variant >= 4)
    throw Error("Horsemen variant outside documented 0..3 range");
  install_ctor_services();
  auto bridge = [this](const char *name) {
    model.callbacks[name] = [this, name](A::Model &,
        const std::vector<Value> &args) { return service(name, args); };
  };
  for (auto name : {"anim_syncToSimFrame", "anim_pushChained",
                    "creature_inKnockdownAnim"})
    bridge(name);

  auto place = [&](Raw size, const Spawn &v, Address table,
                   const std::map<unsigned, Raw> &slots) {
    auto a = pool_new(size);
    if (!a)
      throw Unsupported("mounted Horsemen pool allocation failed");
    construct(a, v.position, Kind::Creature);
    for (auto off : {0u, 4u, 8u, 0x234u, 0x238u, 0x2e4u,
                     0x360u, 0x364u, 0x3c8u})
      head(a + off);
    auto def = alloc_fixture(table == VT_HORSEMEN ? 0xe0 : 0x90);
    for (Raw off = 0x1c; off < (table == VT_HORSEMEN ? 0xe0u : 0x90u);
         off += 8)
      w(def + off, def + off);
    w(a + 0x24, def);
    w(a + 0x48, table);
    write_table(table, &slots);
    w(a + 12, 0x20);
    w(a + 0x38, 2, v.facing);
    w(a + 0x228, v.hp);
    w(a + 0x22c, v.hp);
    w(a + 0xd0, 2, Raw(v.minimum));
    w(a + 0xd2, 2, Raw(v.maximum));
    w(a + 0xd4, 2, Raw(v.attack));
    w(a + 0xe4, 2, Raw(v.armor));
    w(a + 0x3c0, v.home_range);
    vec(a + 0x3b4, v.position);
    w(a + 0xdc, ONE);
    w(a + 0x310, 1, Raw(v.team));
    w(a + 0x5a, 1, 0xff);
    w(a + 0x23c, UINT32_MAX);
    w(a + 0x240, UINT32_MAX);
    w(a + 0x3c4, 2, 600);
    w(a + 0x3ae, 2, 500);
    w(a + 0x3b0, 2, 0x4000);
    w(a + 0xa8, 8, v.creature_flags);
    w(a + 0xec, u(FRAME));
    schedule(a, 60);
    return a;
  };

  auto r = place(0x478, rider, VT_HORSEMEN,
                 {{4, HORSEMEN_RUN}, {5, 0x2721f8}, {27, HORSEMEN_STATE}});
  auto m = place(0x3d8, mount, VT_NIGHTMARE,
                 {{4, NIGHTMARE_RUN}, {5, 0x270b4c}, {10, 0x27142c},
                  {11, 0x271484}, {27, NIGHTMARE_STATE}});
  // Horsemen_ctor embeds WeaponShield at +0x3d0 and equips both hands.
  // Its slot 17 supplies the descriptor used by mounted swing states.
  const Address holder = r + 0x3d0;
  constexpr Raw holder_table = 0x50cda0;
  model.init_holder(r, holder, &holder_table);
  w(r + 0x50, holder);
  model.holder_call(holder, 1, {iv(1), iv(2 * variant + 0x1b)});
  model.holder_call(holder, 1, {iv(2), iv(2 * variant + 0x23)});
  w(r + 0x470, 1, variant);
  // The executed Creature_setStats constructor consumes one own-RNG draw
  // before Horsemen_ctor's first cast timer. Fixture stats are supplied above.
  (void)model.own_draw(r);
  w(r + 0xa8, 8, q(r + 0xa8) | UINT64_C(0x2020020000));
  w(r + 0x1c, 0x41a00000); // 20.0, Horsemen_ctor.
  w(r + 0x28, 1, 0x30);
  w(r + 0xb0, 0x3f999999);
  w(r + 0xb4, 1, 9);
  w(r + 0xdc, 0x3f999999);
  w(r + 0x5c, 0x3e4ccccc);
  w(r + 0x474, u(FRAME) + 480 + model.own_draw(r) % 120);
  w(m + 0xa8, 8, q(m + 0xa8) | UINT64_C(0x20200040000));
  w(m + 0x1c, 0x41f00000); // 30.0, Nightmare_ctor 0x270a20.
  w(m + 0x28, 1, 50);
  w(m + 0xb0, 0x3f199999); // 0.6, Nightmare_ctor 0x270a0c.
  w(m + 0xe0, 0xc1a00000); // -20.0 seat offset, 0x270a64.
  w(m + 0xdc, 0x3f999999); // 1.2, Nightmare_ctor 0x270a74.
  w(m + 0xb4, 1, 10);
  w(m + 0x5c, ONE);
  w(m + 0x3c0, from_int(2400));
  w(m + 0x3ae, 2, 2400);
  (void)model.own_draw(m); // Creature_setStats HP roll, before state 0.
  model.nightmare_set_state(m, 0);
  link(r + 0x3c8, m, m);
  link(m + 0x3c8, r, r);
  model.horsemen_set_state(r, 0x1b);
  return {r, m};
}

std::array<Address, 2> World::spawn_mounted_horseman(
    unsigned variant, const Spawn &rider, const Spawn &mount) {
  return s_->spawn_mounted_horseman(variant, rider, mount);
}
void World::set_mounted_state(Address mount, Raw state) {
  s_->actor(mount);
  if (s_->u(mount + 0x48) != VT_NIGHTMARE)
    throw Error("mounted state target is not a Nightmare");
  s_->model.nightmare_set_state(mount, state);
}
} // namespace champions::simulation::rta::combat_runtime
