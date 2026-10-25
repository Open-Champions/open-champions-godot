#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
namespace {
// Spell classes damage_apply asks about (research ref_combat_spells
// DAMAGE_SPELL_GETTERS): a victim carrying one is outside the composition.
// 0x3cdd40 is SpellDiseaseEffect. Round 3 builds that object instead of
// refusing the victim for carrying it.
const std::set<Raw> DAMAGE_SPELL_GETTERS = {
    0x3cb1a0, 0x3d0a00, 0x3d19f0, 0x3d1d70, 0x3cf3ac, 0x3cf3fc,
    0x3d18a0, 0x3cf000, 0x3ce9bc, 0x3d0da8};
constexpr Raw WAVE_OF_FLAME_GETTER = 0x3d1c4c;
// Ephemeral call records for the existing exact damage body. They never survive
// a call. Publish only changed lanes before virtual dispatch; reload
// immediately afterward, so callbacks always observe the single memory's
// current values. The shared seed is borrowed for the call and written back to
// memory before every callback and at the end.
struct DamageCall {
  World::State &s;
  combat::World w;
  std::map<Address, combat::Obj> obj, before;
  std::map<Address, std::vector<combat::Spell>> spell_records;
  std::size_t seen_events = 0;
  Address disease_victim = 0, disease_attacker = 0, disease_block = 0;
  std::size_t disease_draws = 0;
  bool disease_asked = false;
  explicit DamageCall(World::State &state) : s(state) {
    s.shared.state = s.seed();
    w.bind_shared_random(s.shared, "combat-runtime:0x404464");
    w.frame = s.u(FRAME);
    w.numbers_option = s.u(0x44fcf8);
    w.network_mode = s.b(0x402791);
    w.cutscene = s.b(0x44fcd0);
    w.pvp_a = s.u(0x4f711c);
    w.focus = {s.u(0x44f048), s.u(0x44f04c)};
    w.num_players = s.u(NUM_PLAYERS);
    for (unsigned k = 0; k < 32; ++k)
      w.music[k] = s.music[k];
    for (auto &entry : s.objects)
      obj.emplace(entry.first, combat::Obj{});
    reload();
    for (unsigned k = 0; k < s.u(NUM_PLAYERS); ++k) {
      auto r = PLAYERS + k * RECORD_SIZE;
      combat::PlayerRec rec;
      rec.hp = s.u(r + 0xac);
      rec.max_hp = s.u(r + 0xb0);
      rec.cls = s32(s.u(r + 0x50));
      rec.level58 = byte(s.b(r + 0x58));
      for (unsigned n = 0; n < 4; ++n)
        rec.eff[n] = s32(s.u(r + 0x90 + 4 * n));
      auto a = s.u(r + 0x43d8);
      rec.obj = a && obj.count(a) ? &obj.at(a) : nullptr;
      for (unsigned j = 0; j < 64; ++j) {
        auto id = s16(s.h(r + 0x4b08 + 4 * j));
        if (id == -1)
          break;
        rec.skills.emplace_back(id, s16(s.h(r + 0x4b0a + 4 * j)));
        if (j == 63)
          throw Error("skill list unterminated");
      }
      w.players.push_back(rec);
    }
    for (unsigned k = 0; k < s.u(0x4f8420); ++k)
      w.alive.push_back(s.u(0x4f8440 + 4 * k));
    w.pool_fn = [&](std::int64_t size) -> bool {
      // SpellDiseaseEffect is 0x1b0. The shared draw that follows this return
      // is the Spell base generator; make_disease_effect stores it and does
      // not draw again. Every other damage allocation stays refused. The
      // round-2 corpus still records the refusal, so that replay skips this.
      if (size == DISEASE_EFFECT_SIZE && s.full() && s.round3) {
        disease_asked = true;
        disease_draws = w.shared_random_draw_count();
        disease_block = s.pool_new(DISEASE_EFFECT_SIZE);
        return disease_block != 0;
      }
      throw Unsupported("damage effect allocation, spells or FlyingText");
    };
    w.take_damage_fn = [&](combat::Obj &v, Raw amount, combat::Obj *a,
                           Raw flags) {
      Address victim = address(&v), attacker = a ? address(a) : 0;
      publish();
      s.take_damage(victim, attacker, flags, amount);
      reload();
    };
  }
  Address address(const combat::Obj *p) const {
    for (auto &e : obj)
      if (&e.second == p)
        return e.first;
    throw Error("foreign damage object");
  }
  void reload() {
    s.shared.state = s.seed();
    for (auto &entry : obj) {
      auto a = entry.first;
      auto &o = entry.second;
      o.flags = s.u(a + 12);
      o.pos.assign({s.u(a + 16), s.u(a + 20), s.u(a + 24)});
      o.facing = s16(s.h(a + 0x38));
      o.own_seed = s.u(a + 0x44);
      o.cflags = s.q(a + 0xa8);
      o.kind = byte(s.b(a + 0xb4));
      o.dot = s.u(a + 0xcc);
      o.exp_value = s.u(a + 0xd8);
      o.field_dc = s.u(a + 0xdc);
      o.armor = s.h(a + 0xe4);
      o.stamp_f0 = s.u(a + 0xf0);
      o.hp = s.u(a + 0x228);
      o.max_hp = s.u(a + 0x22c);
      o.poison_a = s.h(a + 0x358);
      o.poison_b = s.h(a + 0x35a);
      o.counter2 = s.u(a + 0x374);
      o.counter4 = s.u(a + 0x37c);
      o.counter11 = s.u(a + 0x398);
      o.counter12 = s.u(a + 0x39c);
      o.slot = s.u(a + 0x3c8);
      o.state = s.u(a + 0x58c);
      o.attack_start = s.u(a + 0x458);
      o.attack_chain = s.u(a + 0x45c);
      o.cls = s.actor(a).player ? U"Player" : U"Creature";
      o.tag = std::u32string(1, char32_t(a));
      for (unsigned k = 0; k < 5; ++k) {
        o.res[k] = s.u(a + 0x5c + 4 * k);
        o.status[k] = s.u(a + 0x340 + 4 * k);
      }
      for (unsigned k = 0; k < 8; ++k)
        o.flash[k] = s16(s.h(a + 0x90 + 2 * k));
      for (unsigned k = 0; k < 4; ++k)
        o.bonus[k] = byte(s.b(a + 0x474 + k));
      auto src = s.target(a + 0x360), last = s.target(a + LAST_ATTACKER);
      o.status_source = src && obj.count(src) ? &obj.at(src) : nullptr;
      o.last_attacker = last && obj.count(last) ? &obj.at(last) : nullptr;
      o.spells.clear();
      auto &records = spell_records[a];
      records.clear();
      if (s.full() && s.heads.count(a + 0x364)) {
        // SpellLiveObj: attached spells are visible unless damage_apply asks
        // about their class.
        for (auto spell : s.attached(a)) {
          auto getter = s.u(s.u(spell + 0x14) + 4);
          if (DAMAGE_SPELL_GETTERS.count(getter))
            throw Unsupported("a victim carrying a spell damage_apply asks "
                              "about");
          combat::Spell rec;
          if (getter == WAVE_OF_FLAME_GETTER)
            rec.kind = U"WaveOfFlame";
          else if (getter == DISEASE_GETTER)
            rec.kind = U"DiseaseEffect";
          else
            rec.kind = U"other";
          records.push_back(rec);
        }
        for (auto &rec : records)
          o.spells.push_back(&rec);
      } else if (s.target(a + 0x364))
        throw Unsupported("attached spells");
      if (!s.full() && (o.flags & 0x80))
        throw Unsupported("projectile owner damage");
    }
    before = obj;
  }
  void publish() {
    // Every status assignment re-links the status source (the LiveObj setter
    // of the reference clears and links the handle even for the same source).
    std::set<Address> assigned;
    for (; seen_events < w.events.size(); ++seen_events) {
      auto &e = w.events[seen_events];
      if (e.size() >= 2)
        if (auto name = std::get_if<combat::Text>(&e[0].value))
          if (*name == U"status_source")
            if (auto who = std::get_if<combat::Text>(&e[1].value))
              if (who->size() == 1)
                assigned.insert(Address((*who)[0]));
    }
    for (unsigned k = 0; k < 32; ++k)
      s.music[k] = std::uint8_t(w.music[k]);
    for (auto &e : obj) {
      auto a = e.first;
      auto &o = e.second;
      auto &p = before.at(a);
      auto lane = [&](Raw off, unsigned width, std::uint64_t old,
                      std::uint64_t value) {
        if (old != value)
          s.w(a + off, width, value);
      };
      lane(0xc, 4, std::uint64_t(p.flags), std::uint64_t(o.flags));
      lane(0x38, 2, std::uint64_t(p.facing), std::uint64_t(o.facing));
      lane(0x44, 4, std::uint64_t(p.own_seed), std::uint64_t(o.own_seed));
      lane(0xa8, 8, std::uint64_t(p.cflags), std::uint64_t(o.cflags));
      lane(0xb4, 1, std::uint64_t(p.kind), std::uint64_t(o.kind));
      lane(0xcc, 4, std::uint64_t(p.dot), std::uint64_t(o.dot));
      lane(0xd8, 4, std::uint64_t(p.exp_value), std::uint64_t(o.exp_value));
      lane(0xdc, 4, std::uint64_t(p.field_dc), std::uint64_t(o.field_dc));
      lane(0xe4, 2, std::uint64_t(p.armor), std::uint64_t(o.armor));
      lane(0xf0, 4, std::uint64_t(p.stamp_f0), std::uint64_t(o.stamp_f0));
      lane(0x228, 4, std::uint64_t(p.hp), std::uint64_t(o.hp));
      lane(0x22c, 4, std::uint64_t(p.max_hp), std::uint64_t(o.max_hp));
      lane(0x358, 2, std::uint64_t(p.poison_a), std::uint64_t(o.poison_a));
      lane(0x35a, 2, std::uint64_t(p.poison_b), std::uint64_t(o.poison_b));
      lane(0x374, 4, std::uint64_t(p.counter2), std::uint64_t(o.counter2));
      lane(0x37c, 4, std::uint64_t(p.counter4), std::uint64_t(o.counter4));
      lane(0x398, 4, std::uint64_t(p.counter11), std::uint64_t(o.counter11));
      lane(0x39c, 4, std::uint64_t(p.counter12), std::uint64_t(o.counter12));
      for (unsigned k = 0; k < 5; ++k)
        if (o.status[k] != p.status[k])
          s.w(a + 0x340 + 4 * k, o.status[k]);
      for (unsigned k = 0; k < 8; ++k)
        if (o.flash[k] != p.flash[k])
          s.w(a + 0x90 + 2 * k, 2, Raw(o.flash[k]));
      if (o.status_source != p.status_source || assigned.count(a))
        s.relink(a + 0x360, o.status_source ? address(o.status_source) : 0);
    }
    before = obj;
    s.set_seed(s.shared.state);
  }
  void commit_disease() {
    if (!disease_asked)
      return;
    disease_asked = false;
    Raw seed_value = 0;
    bool drawn = false;
    if (disease_block) {
      auto snap = w.shared_random_snapshot();
      if (snap && snap->draws.size() > disease_draws) {
        seed_value = snap->draws[disease_draws].value;
        drawn = true;
      }
    }
    if (disease_block && drawn)
      s.make_disease_effect(disease_block, disease_victim, disease_attacker,
                            seed_value);
    else
      disease_block = 0;
    s.note("creature_addSpell", {disease_victim, disease_block});
    if (disease_block)
      s.handle_link(disease_victim + 0x364, disease_block + 4, disease_block);
  }
};
combat::Descriptor item_mods(const World::State &s, Address item) {
  combat::Descriptor d(4, std::vector<std::int64_t>(4, 0));
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned j = 0; j < 4; ++j)
      d[i][j] = s16(s.h(item + 0x24 + 8 * i + 2 * j));
  return d;
}
} // namespace
// The item record (0x48 bytes) and its definition as ref_combat reads them
// (research ref_combat_melee.LiveItem / LiveItemDef).
struct LiveItem {
  combat::ItemDef def;
  combat::Item item;
  LiveItem(const World::State &s, Address addr) {
    auto d = s.u(addr);
    def.dmg_min = s16(s.h(d + 0xe));
    def.dmg_max = s16(s.h(d + 0x10));
    def.armor = s16(s.h(d + 0x18));
    def.attack = s16(s.h(d + 0x1a));
    def.category = s.b(d + 0x1d);
    def.subtype = s.b(d + 0x1e);
    def.dmg_type = s.u(d + 0x20);
    def.flags = s.q(d + 0x28);
    item.defn = &def;
    item.count = s16(s.h(addr + 4));
    item.flags = s.q(addr + 8);
    item.mods = item_mods(s, addr);
  }
  LiveItem(const LiveItem &) = delete;
};
void World::State::take_damage(Address v, Address a, Raw f, Raw amount) {
  actor(v);
  if (a)
    actor(a);
  note("take_damage", {v, a, f, amount});
  auto fn = u(u(v + 0x48) + 20);
  if (fn == 0x1b5330)
    take_damage_full(v, a, f, amount);
  else if (fn == 0x116638)
    model.vcall(v, 2, {iv(a), iv(f), iv(amount)});
  else if (full())
    vcall(v, 2, {iv(a), iv(f), iv(amount)});
  else
    throw Unsupported("take-damage virtual body");
}
namespace {
int run_damage(World::State &s, Address v, Address a, Raw lo, Raw hi,
               Raw attack, Raw flags, const combat::Descriptor *desc) {
  DamageCall call(s);
  call.disease_victim = v;
  call.disease_attacker = a;
  s.note("damage_apply", {v, a, attack, flags});
  auto finish = [&] {
    call.publish();
    auto snap = call.w.shared_random_snapshot();
    if (snap)
      for (auto &d : snap->draws)
        s.note("damage_draw", {d.value, d.state});
    call.commit_disease();
  };
  try {
    auto result = combat::damage_apply(call.w, call.obj.at(v),
                                       a ? &call.obj.at(a) : nullptr, lo, hi,
                                       s32(attack), flags, desc, 0);
    finish();
    s.note("damage_result", {v, Raw(result)});
    return result;
  } catch (...) {
    finish();
    throw;
  }
}
} // namespace
int World::State::damage(Address v, Address a, Raw lo, Raw hi, Raw attack,
                         Raw flags) {
  if (full())
    return damage_full(v, a, attack, 0, 0, flags, 0, lo, hi);
  domain();
  actor_domain(v);
  if (a)
    actor_domain(a);
  if (q(v + 0xa8) & combat::CF_BLOCKING)
    throw Unsupported("blocking combat victim");
  if (q(v + 0xa8) & combat::CF_THORNS)
    throw Unsupported("thorns skill");
  return run_damage(*this, v, a, lo, hi, attack, flags, nullptr);
}
// damage_apply 0x1e8230 (ref_combat, executed) as the composed runtime calls
// it: a projectile attacker refuses, a weapon item is the descriptor.
int World::State::damage_full(Address v, Address a, Raw attack, Raw, Raw,
                              Raw flags, Raw item, Raw lo, Raw hi) {
  domain();
  actor(v);
  if (a) {
    auto k = actor(a).kind;
    if (k == Kind::Missile || k == Kind::Thrown)
      throw Unsupported("a missile whose owner is gone is its own attacker");
  }
  if (!item)
    return run_damage(*this, v, a, lo, hi, attack, flags, nullptr);
  auto mods = item_mods(*this, item);
  return run_damage(*this, v, a, lo, hi, attack, flags, &mods);
}
int damage_with_descriptor(World::State &s, Address v, Address a, Raw attack,
                           Raw flags, const combat::Descriptor &d, Raw lo,
                           Raw hi) {
  s.domain();
  s.actor(v);
  if (a) {
    auto k = s.actor(a).kind;
    if (k == World::State::Kind::Missile || k == World::State::Kind::Thrown)
      throw Unsupported("a missile whose owner is gone is its own attacker");
  }
  return run_damage(s, v, a, lo, hi, attack, flags, &d);
}
void World::State::recalc(Address p, Raw flag) {
  auto r = record(p);
  if (u(r + 0x443c))
    throw Unsupported("second weapon");
  DamageCall call(*this);
  const combat::Tables unused_unarmed_tables;
  auto result = combat::recalc_damage(unused_unarmed_tables, call.w,
                                      call.obj.at(p), nullptr, flag != 0);
  w(r + 0xcc, Raw(result[0]));
  w(r + 0xd0, Raw(result[1]));
  w(r + 0xd4, Raw(result[2]));
}
// Player_recalcDamage 0x1b7158 with a weapon item: ref_combat.recalc_damage on
// live views, the subset that reads no skill statistics.
void World::State::recalc_weapon(Address p, Raw weapon, Raw flag) {
  if (!weapon)
    return recalc(p, flag);
  LiveItem item(*this, weapon);
  auto sub_type = byte(Raw(item.def.subtype));
  if (sub_type == 4 || sub_type == 6)
    throw Unsupported("a bow or thrown weapon (skill 0x30 / 0x4f statistics)");
  auto r = record(p);
  if (s32m(r + 0x50) == 4)
    throw Unsupported("record class 4 (skill 0x5c statistics)");
  if (u(r + 0x443c))
    throw Unsupported("a second weapon");
  DamageCall call(*this);
  const combat::Tables unused_tables;
  auto result = combat::recalc_damage(unused_tables, call.w, call.obj.at(p),
                                      &item.item, flag != 0);
  w(r + 0xcc, Raw(result[0]));
  w(r + 0xd0, Raw(result[1]));
  w(r + 0xd4, Raw(result[2]));
}
Raw World::State::item_rating(Address item) {
  LiveItem live(*this, item);
  return Raw(combat::item_attack_rating(live.item));
}
void World::State::process_damage(Address a) {
  actor_domain(a);
  if (actor(a).player)
    process_damage_full(a);
  else
    model.process_damage(a, model.class_model(a));
}
void World::take_damage(Address v, Address a, Raw f, Raw n) {
  s_->take_damage(v, a, f, n);
}
int World::damage(Address v, Address a, Raw l, Raw h, Raw ar, Raw f) {
  return s_->damage(v, a, l, h, ar, f);
}
void World::process_damage(Address a) { s_->process_damage(a); }
} // namespace champions::simulation::rta::combat_runtime
