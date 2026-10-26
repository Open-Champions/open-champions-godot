#include "combat_runtime_internal.hpp"
#include "champions/model.hpp"
// Public entry points of the Full composition (round 2). Set-up helpers are
// the research runtimes' AUTHORED fixtures, not original constructors.
namespace champions::simulation::rta::combat_runtime {
namespace {
void require_full(const World::State &s) {
  if (!s.full())
    throw Unsupported("a round-2 entry point needs the Full composition");
}
constexpr Address G_BUTTONS[] = {0x41b980, 0x41b990, 0x41b9a0, 0x41b9d0,
                                 0x41ba10, 0x41ba20, 0x41ba30};
} // namespace
World::State::Actor *World::State::find(Address a) const {
  auto i = objects.find(a);
  return i == objects.end() ? nullptr : i->second.get();
}
void World::stick(unsigned slot, Raw x, Raw y) {
  require_full(*s_);
  if (slot >= 4)
    throw Error("pad slot out of range");
  auto pad = PADS + slot * PAD_SIZE;
  s_->poke(pad + 0x14, 4, x);
  s_->poke(pad + 0x18, 4, y);
}
void World::press_attack(unsigned slot, bool held) {
  require_full(*s_);
  if (slot >= 4)
    throw Error("pad slot out of range");
  // research Runtime.press_attack: the authored attack mask 0x10.
  Raw mask = 0x10;
  s_->poke(G_BTN_ATTACK + 4 * slot, 4, mask);
  auto pad = PADS + slot * PAD_SIZE;
  s_->poke(pad + 0xc, 4, mask);
  s_->poke(pad + 8, 4, held ? mask : 0);
}
void World::bind_button(unsigned slot, Button which, Raw mask) {
  require_full(*s_);
  if (slot >= 4)
    throw Error("pad slot out of range");
  s_->poke(G_BUTTONS[unsigned(which)] + 4 * slot, 4, mask);
}
void World::pad(unsigned slot, Raw held, Raw pressed) {
  require_full(*s_);
  if (slot >= 4)
    throw Error("pad slot out of range");
  auto pad = PADS + slot * PAD_SIZE;
  s_->poke(pad + 8, 4, held);
  s_->poke(pad + 0xc, 4, pressed);
}
Address World::give_weapon(unsigned slot, const Weapon &v) {
  require_full(*s_);
  if (slot >= 4)
    throw Error("player slot out of range");
  auto &s = *s_;
  auto d = s.alloc_fixture(0x40);
  s.poke(d + 0xe, 2, Raw(v.minimum) & 0xffff);
  s.poke(d + 0x10, 2, Raw(v.maximum) & 0xffff);
  s.poke(d + 0x1a, 2, Raw(v.attack) & 0xffff);
  s.poke(d + 0x1e, 1, Raw(v.subtype) & 0xff);
  s.poke(d + 0x20, 4, v.damage_type);
  auto item = s.alloc_fixture(0x48);
  s.poke(item, 4, d);
  s.poke(item + 4, 2, 1);
  if (v.mods.size() > 4)
    throw Error("an item has four modifier slots");
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned j = 0; j < 4; ++j) {
      Raw value = i < v.mods.size() ? Raw(v.mods[i][j]) : (j == 0 ? 0xffff : 0);
      s.poke(item + 0x24 + 8 * i + 2 * j, 2, value & 0xffff);
    }
  auto r = PLAYERS + slot * RECORD_SIZE;
  s.poke(r + 0x4434, 4, item);
  s.poke(r + 0x43f0, 4, item);
  s.poke(r + 0x4a86, 1, Raw(v.cls) & 0xff);
  return item;
}
Address World::spawn_missile(Address owner, const Missile &m) {
  require_full(*s_);
  auto a = s_->pool.allocate(MISSILE_SIZE);
  if (!a)
    throw Unsupported("the pool refused the block");
  s_->kind_hint = State::Kind::Missile;
  Missile v = m;
  v.facing &= 0xffff;
  s_->missile_ctor(a, v, owner);
  return a;
}
Address World::spawn_thrown(Address owner, const Thrown &t) {
  require_full(*s_);
  auto a = s_->pool.allocate(THROWN_SIZE);
  if (!a)
    throw Unsupported("the pool refused the block");
  s_->kind_hint = State::Kind::Thrown;
  Thrown v = t;
  v.facing &= 0xffff;
  s_->thrown_ctor(a, v, owner);
  return a;
}
Address World::give_skill(int id, const SkillSetup &v) {
  require_full(*s_);
  auto &s = *s_;
  static const std::map<int, std::pair<Address, std::array<Raw, 16>>> tables = {
      // US v2.00 pointer values, checked read-only; unmodelled bodies still
      // refuse in skill_virtual.
      {4,
       {0x518468,
        {0x3ccf60, 0x2cb368, 0x2cb1c0, 0x3c8248, 0x2cb264, 0x2cb33c,
         0x3c82b8, 0x3ccfb0, 0x3c82e4, 0x2cb14c, 0x3c85fc, 0x3c8690,
         0x3c87f0, 0x3c8864, 0x3c88dc, 0x3c88e4}}},
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
  auto t = tables.find(id);
  if (t == tables.end())
    throw Unsupported("skill " + std::to_string(id) + " is not composed");
  auto table = t->second.first;
  for (unsigned k = 0; k < 16; ++k) {
    s.poke(table + 8 * k, 2, 0);
    s.poke(table + 8 * k + 4, 4, t->second.second[k]);
  }
  auto skill = s.alloc_fixture(0x68);
  s.poke(skill + 4, 4, v.flags);
  s.poke(skill + 8, 4, v.kind);
  s.poke(skill + 0xc, 2, v.recast & 0xffff);
  for (unsigned i = 0; i < 5; ++i) {
    s.poke(skill + 0xe + 2 * i, 2, Raw(v.stats[i].first) & 0xffff);
    s.poke(skill + 0x18 + 2 * i, 2, Raw(v.stats[i].base) & 0xffff);
    s.poke(skill + 0x24 + 4 * i, 4, v.stats[i].step);
    s.poke(skill + 0x38 + 2 * i, 2, 0xffff);
  }
  s.poke(skill + 0x60, 4, table);
  s.poke(skill + 0x64, 4, Raw(id));
  s.poke(G_SKILL_TABLE + 4 * Raw(id), 4, skill);
  s.skill_objects[id] = skill;
  return skill;
}
void World::select_disease_bolt(unsigned slot, Raw mana, Raw max_mana,
                                unsigned rank) {
  require_full(*s_);
  if (rank < 1 || rank > 20)
    throw Unsupported("Disease Bolt rank needs a probed player rank 1 to 20");
  if (slot >= 4 || !lt(0, mana) || !le(mana, max_mana) ||
      (mana & 0x7f800000u) == 0x7f800000u ||
      (max_mana & 0x7f800000u) == 0x7f800000u)
    throw Unsupported("Disease Bolt needs a player and supplied positive finite mana");
  auto &s = *s_;
  auto r = PLAYERS + slot * RECORD_SIZE;
  if (!s.u(r + 0x43d8) || s.skill_objects.count(4) ||
      s.s16m(r + 0x4b08) != -1)
    throw Unsupported("Disease Bolt requires an unused Full player skill record");
  SkillSetup bolt;
  bolt.kind = 0;
  bolt.recast = 30;
  bolt.stats[0] = {1, 30, 0x41102484};
  bolt.stats[1] = {5, 1, 0x3eaa7ef9};
  give_skill(4, bolt);
  // Keep the opt-in projectile table out of the default Full fixture's exact
  // memory image. Pointers are checked read-only against the US v2.00 table;
  // unmodelled destructor/drawing bodies still refuse when invoked.
  constexpr std::array<Raw, 17> projectile_methods = {
      0x3cd05c, 0x3ccfd4, 0x3c7e88, 0x3c7e90, 0x2ca1a8, 0x2ca09c,
      0x2caec8, 0x3c7eb0, 0x3c7eb8, 0x3c7ec0, 0x3c7ec8, 0x3c7ef0,
      0x3c7ef8, 0x3c7f20, 0x3c7f28, 0x11c7f8, 0x3c8120};
  for (unsigned k = 0; k < projectile_methods.size(); ++k) {
    s.poke(VT_DISEASEBOLT + 8 * k, 2, 0);
    s.poke(VT_DISEASEBOLT + 8 * k + 4, 4, projectile_methods[k]);
  }
  // Ability record and quick-slot A are observed at R+0x4440 and R+0x4a84.
  // Other quick slots remain untouched. This is a fixture assignment, not a
  // substitute for the unported local HUD selector/message producer.
  // The original compares these two C strings, but the research does not
  // establish the literal bytes. Match them only inside this authored fixture.
  constexpr char name[] = "fixture-skill";
  for (unsigned k = 0; k < sizeof(name); ++k) {
    s.poke(r + 0x4440 + k, 1, std::uint8_t(name[k]));
    s.poke(0x535e30 + k, 1, std::uint8_t(name[k]));
  }
  s.poke(r + 0x4440 + 0x20, 2, 4);
  s.poke(r + 0x4440 + 0x24, 1, rank);
  s.poke(r + 0x4440 + 0x25, 1, 0);
  s.poke(r + 0x4a84, 1, 0);
  s.poke(r + 0x4a60, 2, 0);
  s.poke(r + 0x4b08, 2, 4);
  s.poke(r + 0x4b0a, 2, rank);
  s.poke(r + 0x4b0c, 2, 0xffff);
  s.poke(r + 0xc0, 4, mana);
  s.poke(r + 0xc4, 4, max_mana);
}
void World::queue_cast(Address caster, int id, int level,
                       std::optional<Vec3> point, std::optional<Raw> facing) {
  require_full(*s_);
  s_->actor(caster);
  if (!s_->skill_objects.count(id))
    throw Error("give_skill first");
  s_->queued_casts[caster].push_back({id, level, point, facing});
}
Raw World::move_with_collision(Address a, const Vec3 &v, Address ignore,
                               Raw skip) {
  require_full(*s_);
  s_->actor(a);
  return s_->move_with_collision(a, v, ignore, skip);
}
Raw World::player_move(Address p, Raw mode, Address pad, const Vec3 &ext) {
  require_full(*s_);
  s_->record(p);
  return s_->player_move(p, mode, pad, ext);
}
Raw World::player_anim_step(Address p, Raw speed) {
  require_full(*s_);
  s_->record(p);
  return s_->player_anim_step(p, speed);
}
Raw World::clamp_to_screen(Address p, Address v, Address mask) {
  require_full(*s_);
  s_->record(p);
  return s_->clamp_to_screen(p, v, mask);
}
void World::pick_attack_anim(Address p, Raw cls) {
  require_full(*s_);
  s_->record(p);
  s_->pick_attack_anim(p, cls);
}
void World::hit_sparks(Address p, Address t) {
  require_full(*s_);
  s_->record(p);
  s_->hit_sparks(p, t);
}
void World::block_sparks(Address p, Raw silent) {
  require_full(*s_);
  s_->record(p);
  s_->block_sparks(p, silent);
}
void World::interact(Address p, Address pad, Raw display_only) {
  require_full(*s_);
  s_->record(p);
  s_->interact(p, pad, display_only);
}
int World::can_see_point(Address c, const Vec3 &p, Raw f, Raw arc, Raw near,
                         Raw far) {
  require_full(*s_);
  s_->actor(c);
  return s_->can_see_point(c, p, f, arc, near, far);
}
int World::can_see_edge(Address c, Address o, Raw f, Raw arc, Raw mode,
                        Raw near, Raw far, Raw lead) {
  require_full(*s_);
  s_->actor(c);
  return s_->can_see_edge(c, o, f, arc, mode, near, far, lead);
}
Address World::find_player_in_cone(Address c, Raw f, Raw arc, Raw near,
                                   Raw far) {
  require_full(*s_);
  s_->actor(c);
  return s_->find_player_in_cone(c, f, arc, near, far);
}
int World::hit_target_at_reach(Address c, Raw lo, Raw hi, Raw attack,
                               Raw flags, Raw reach) {
  require_full(*s_);
  s_->actor(c);
  return s_->hit_target_at_reach(c, lo, hi, attack, flags, 0, reach);
}
int World::hit_target_in_arc_edge(Address c, const Arc &a) {
  require_full(*s_);
  s_->actor(c);
  return s_->hit_target_in_arc_edge(c, a.facing, a.angle, a.minimum, a.maximum,
                                    a.attack, a.flags, 0, a.near, a.far);
}
int World::hit_target_or_player_in_arc(Address c, const Arc &a) {
  require_full(*s_);
  s_->actor(c);
  return s_->hit_target_or_player(c, a.facing, a.angle, a.minimum, a.maximum,
                                  a.attack, a.flags, 0, a.near, a.far);
}
void World::knockback_target(Address c, Raw speed) {
  require_full(*s_);
  s_->actor(c);
  s_->knockback_target(c, speed);
}
void World::set_cooldown(Address c, Raw frames) {
  require_full(*s_);
  s_->actor(c);
  s_->set_cooldown(c, frames);
}
void World::set_cooldown_on(Address c, Raw frames, Address o) {
  require_full(*s_);
  s_->actor(c);
  s_->set_cooldown_on(c, frames, o);
}
void World::notify_objects_near(Raw a, const Vec3 &p, Raw b, Raw radius) {
  require_full(*s_);
  s_->notify_objects_near(a, p, b, radius);
}
std::vector<Address> World::collect_in_arc(const Vec3 &p, Raw f, Address out,
                                           Address filter, Raw radius, Raw arc,
                                           Raw width) {
  require_full(*s_);
  return s_->collect_in_arc(p, f, out, filter, radius, arc, width);
}
std::vector<Address> World::collect_in_sphere(const Vec3 &c, Address out,
                                              Raw radius) {
  require_full(*s_);
  return s_->collect_in_sphere(c, out, radius);
}
std::vector<Address> World::damage_in_radius(Address a, const Vec3 &c,
                                             Raw most, Raw flags, Raw least,
                                             Raw skip, Raw inner, Raw outer) {
  require_full(*s_);
  return s_->damage_in_radius(a, c, most, flags, least, skip, inner, outer);
}
std::pair<Vec3, int> World::find_free_spot(const Vec3 &c, Raw spread,
                                           Raw radius, Raw any_height) {
  require_full(*s_);
  return s_->find_free_spot(c, spread, radius, any_height);
}
std::vector<Address> World::attached_spells(Address c) const {
  return s_->attached(c);
}
std::vector<std::pair<Address, Address>> World::handle_words() const {
  std::vector<std::pair<Address, Address>> out;
  for (auto &h : s_->heads)
    if (h.second->next)
      out.push_back({h.first, s_->ptr(h.second->next)});
  return out;
}
namespace {
Address link_word(const World::State &s, const Link *p) {
  if (p && p->kind == LinkKind::Iterator)
    return 0xffffffffu;
  return s.ptr(p);
}
} // namespace
Address World::free_handle_node() const {
  return link_word(*s_, s_->handles.free);
}
std::vector<std::array<Address, 5>> World::handle_nodes() const {
  std::vector<std::array<Address, 5>> out;
  for (auto *n : s_->handles.nodes())
    out.push_back({link_word(*s_, n->next), link_word(*s_, n->prev),
                   link_word(*s_, n->partner), n->obj ? n->obj->id : n->object_raw,
                   n->tag & 0xffff});
  return out;
}
const std::vector<std::pair<Raw, Raw>> &World::experience() const {
  return s_->exp_awards;
}
void World::drop_loot(Address c, const Vec3 &p, Raw angle, Raw filter, Raw z,
                      Raw thr, Raw gs, Raw gc, Raw q, Raw ic) {
  require_full(*s_);
  s_->actor(c);
  s_->drop_loot(c, p, angle, filter, z, thr, gs, gc, q, ic);
}
void World::goblin_set_state(Address c, Raw state) {
  require_full(*s_);
  s_->goblin_set_state(c, state);
}
void World::retain_round2_refusals() {
  s_->round3 = false;
  s_->round4 = false;
}
Address World::spawn_disease_cloud(const Vec3 &pos, Raw level, Address owner) {
  require_full(*s_);
  return s_->diseasecloud_spawn(pos, level, owner);
}
Address World::attach_disease(Address victim, Address attacker) {
  require_full(*s_);
  return s_->disease_attach(victim, attacker);
}
Address World::spawn_loose_fire(const Vec3 &pos, Address owner) {
  require_full(*s_);
  auto block = s_->pool_new(LOOSEFIRE_SIZE);
  if (!block)
    throw Unsupported("the pool refused a LooseFire");
  s_->loosefire_ctor(block, pos, owner);
  return block;
}
Address World::spawn_poison_gas(const Vec3 &pos, Raw life, Raw radius, Raw word,
                                Address owner, Raw damage) {
  require_full(*s_);
  auto block = s_->pool_new(POISON_GAS_SIZE);
  if (!block)
    throw Unsupported("the pool refused a poisonGasSpell");
  s_->poison_gas_ctor(block, pos, life, radius, word, owner, damage);
  return block;
}
void World::touch(Address object, Address other) {
  require_full(*s_);
  s_->vcall(object, 6, {iv(other)});
}
Address World::spawn_gut_chunk(const Vec3 &pos, Raw kind, Raw bits) {
  require_full(*s_);
  return s_->gutchunk_spawn(pos, kind, bits);
}
Address World::spawn_phantom(Address target, const std::string &name, Raw speed,
                            Raw flag) {
  require_full(*s_);
  return s_->phantom_spawn(target, name, speed, flag);
}
void World::run_actor(Address a) {
  require_full(*s_);
  s_->run(a);
}
Raw World::holy_shield_absorb(Address spell) {
  require_full(*s_);
  return s_->holy_shield_absorb(spell);
}
Address World::attach_holy_shield(Address caster, Address skill, Raw level) {
  require_full(*s_);
  return s_->attach_holy_shield(caster, skill, level);
}
void World::fade_bank_tracks(Address creature) {
  require_full(*s_);
  s_->fade_bank_tracks(creature);
}
void World::update_bank(Address creature) {
  require_full(*s_);
  s_->update_bank(creature);
}
int World::is_demon_near(Address creature) {
  require_full(*s_);
  return s_->is_demon_near(creature);
}
Raw World::holy_shield_sample(Address spell) {
  require_full(*s_);
  return s_->holy_shield_sample(spell);
}
void World::multifire_run(Address obj) {
  require_full(*s_);
  s_->multifire_run(obj);
}
Address World::arm_fixture_holy_shield(Address player) {
  require_full(*s_);
  return s_->arm_fixture_holy_shield(player);
}
Address World::spell_block() {
  require_full(*s_);
  return s_->spell_block();
}
Address World::spawn_effect_fixture(Raw run, Raw size, const Vec3 &pos,
                                    Address owner) {
  require_full(*s_);
  return s_->spawn_effect_fixture(run, size, pos, owner);
}
void World::poison_gas_run(Address obj) {
  require_full(*s_);
  s_->poison_gas_run(obj);
}
void World::disease_corpse_run(Address obj) {
  require_full(*s_);
  s_->disease_corpse_run(obj);
}
void World::multifire_on_hit(Address obj, Address other) {
  require_full(*s_);
  s_->multifire_on_hit(obj, other);
}
void World::multifire_end(Address obj, Raw reason) {
  require_full(*s_);
  s_->multifire_end(obj, reason);
}
void World::smoke_gen_run(Address obj) {
  require_full(*s_);
  s_->smoke_gen_run(obj);
}
void World::pustule_run(Address obj) {
  require_full(*s_);
  s_->pustule_run(obj);
}
void World::arena_run(Address spell, const Vec3 &pos) {
  require_full(*s_);
  s_->arena_run(spell, pos);
}
void World::begin_arena_intro() {
  require_full(*s_);
  if (!s_->u(G_PVP_B) || s_->u(G_PVP_A))
    throw Unsupported("arena intro requires mode B without mode A");
  s_->w(0x5038c8, 2, 180);
  s_->w(0x503890, 1);
}
void World::arena_intro_visit() {
  require_full(*s_);
  if (!s_->u(G_PVP_B) || s_->u(G_PVP_A))
    throw Unsupported("arena intro requires mode B without mode A");
  const auto counter = s_->mem.load(0x5038c8, 2);
  if (counter == 0xffff) return;
  if (counter > 180)
    throw Unsupported("arena intro counter outside executed 180-visit domain");
  if (counter) {
    s_->w(0x5038c8, 2, counter - 1);
    s_->w(0x41b500, 1);
    s_->w(0x503890, 1);
    s_->w(0x41b4fc, 1);
  } else {
    s_->w(0x5038c8, 2, 0xffff);
    s_->w(0x41b500, 0);
    s_->w(0x503890, 0);
    s_->w(0x41b4fc, 0);
  }
}
void World::bind_prop_model(Address prop, champions::Bytes vif) {
  require_full(*s_);
  s_->actor(prop);
  const auto model = s_->u(prop + 0x4c);
  const auto source = s_->prop_model_pointers.find(prop);
  const auto table = s_->u(prop + 0x48);
  if (source == s_->prop_model_pointers.end() || !model ||
      model != source->second || (table != VT_GUTCHUNK && table != 0x511ba8))
    throw Unsupported("PhProp model pointer or actor identity is not verified");
  if (!vif.data || !vif.size || vif.size > 16u * 1024u * 1024u)
    throw Unsupported("PhProp model resource is absent or exceeds the host limit");
  const auto mask = s_->q(prop + 0xe8);
  if (!mask)
    throw Unsupported("PhProp has no active mesh hull");
  for (unsigned mesh = 0; mesh < 64; ++mesh)
    if (mask & (UINT64_C(1) << mesh))
      (void)champions::rtaModelHull(vif, mesh);
  // The constructor's resource pointer is only a hint: the World does not
  // own its allocation. Reserve an exact private extent and publish it only
  // after the complete checked copy succeeds.
  const auto destination = s_->alloc_fixture(unsigned(vif.size));
  if (s_->fixture_extents.at(destination) < vif.size)
    throw Error("PhProp model destination extent is too small");
  s_->mem.write_bytes(destination, {vif.data, vif.data + vif.size});
  s_->w(prop + 0x4c, destination);
  source->second = destination;
}
Address World::hornomen_ctor(Raw kind, Address owner, Raw extra) {
  require_full(*s_);
  return s_->hornomen_ctor(kind, owner, extra);
}
void World::hornomen_run(Address spell, const Vec3 &pos) {
  require_full(*s_);
  s_->hornomen_run(spell, pos);
}
Address World::planarsteel_ctor(Raw kind, Address owner, Raw extra) {
  require_full(*s_);
  return s_->planarsteel_ctor(kind, owner, extra);
}
void World::planarsteel_run(Address spell, const Vec3 &pos) {
  require_full(*s_);
  s_->planarsteel_run(spell, pos);
}
} // namespace champions::simulation::rta::combat_runtime
