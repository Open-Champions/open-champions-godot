// Authored translation of ref_player.py at
// 356add27d50e50972b2a671b31e8f4ec93e6778c. SHA256
// ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d. MIT, Open
// Champions contributors. Documented entry/scripted and independently callable helpers.
// Reference copyright (c) 2026 TrustlessMike and contributors, MIT license.
#include "champions/player.hpp"
#include <utility>
namespace champions::simulation::rta::player {
namespace {
constexpr Address PLAYERS = 0x4d8af0, REC = 0x4c30, PADS = 0x41bfd8, PAD = 0x58,
                  ZERO_PAD = 0x8667f0, FRAME = 0x44fdcc, FLAGS = 0x44fe08,
                  SEED = 0x404464, NETWORK = 0x402791, CUTSCENE = 0x44fcd0,
                  ALIVE = 0x4f8420, LIGHT = 0x4f83f4, MISSION = 0x4f713c,
                  MEDAL = 0x4f7114, LOCKOUT = 0x41b4fc;
// Signed pointer expressions refuse outside raw32 instead of wrapping
// implicitly.
Address rec(std::int32_t slot) {
  auto a = std::int64_t(PLAYERS) + std::int64_t(slot) * std::int64_t(REC);
  if (a < 0 || a > UINT32_MAX)
    throw ResourceLimit("player record outside raw32");
  return Address(a);
}
Address indexed(Address base, std::int64_t offset) {
  if (base > UINT32_MAX)
    throw ResourceLimit("player base address outside raw32");
  if (offset < -std::int64_t(base) || offset > std::int64_t(UINT32_MAX) - std::int64_t(base))
    throw ResourceLimit("player address outside raw32");
  auto a = std::int64_t(base) + offset;
  if (a < 0 || a > UINT32_MAX)
    throw ResourceLimit("player address outside raw32");
  return Address(a);
}
} // namespace
const char *service_name(Service s) {
  static constexpr const char *names[] = {"Creature_run",
                                          "Creature_applyKnockback",
                                          "light_relink",
                                          "light_unlink",
                                          "net_getCamFlag",
                                          "game_isFrozen",
                                          "player_voice",
                                          "memcard_isBusy",
                                          "pad_rumble",
                                          "Player_animStep",
                                          "Player_footstep",
                                          "Creature_moveToDest",
                                          "fx_spawn",
                                          "dialog_isOpen",
                                          "task_isRunning",
                                          "net_isSlotActive",
                                          "res_find",
                                          "anim_start",
                                          "anim_startBlendInOut",
                                          "SpellBlessed_typeinfo",
                                          "creature_findSpellByType",
                                          "SpellBlessed_getStat0",
                                          "SkillRegeneration_getStat0",
                                          "net_isLocalSlot",
                                          "sound_playOnObject",
                                          "invmsg_sendDrop",
                                          "inventory_removeItem",
                                          "player_countPotions",
                                          "player_itemWeight",
                                          "hud_countActiveNotices",
                                          "model_getAttachPoint",
                                          "Player.v17",
                                          "anim_findTrackByFlag",
                                          "Spell.v1",
                                          "inventory_equipItem",
                                          "Player_recalcDamage",
                                          "Skill.v1",
                                          "world_collectObjectsInArc",
                                          "Player.v20",
                                          "Player.v39",
                                          "Skill.v2",
                                          "creature_addSpell",
                                          "handle_link",
                                          "Skill.v5",
                                          "handle_clear", "Player_move", "sound_stop", "decal_add", "water_disturb",
      "pool_new", "LightEffect_ctor", "Vehicle.v17", "obj_updateGridCell",
      "world_damageInRadius", "GroundPoundEffect_ctor", "SpellBash_typeinfo",
      "SpellShieldBash_typeinfo", "Spell.v4", "particleDef_ctor", "particle_spawn", "text_lookup", "hud_showMessage", "libc_sprintf",
      "Player_findTarget", "Player.v26", "str_prefixDiffers", "pad_buttonValue", "SpellAncestralCall_typeinfo", "SpellAncestralCall_queryFor",
      "SpellBattleCry_typeinfo", "SpellBattleCry_affects", "SkillBerserkerRage_swingFactor",
      "world_collectObjectsInBox", "Creature.v12", "iter_begin", "iter_next", "iter_end",
      "Player_meleeHit", "handle_removeNode", "Player_pickAttackAnim",
      "player_recalcLoad", "item_getResourceIndex", "playerProjectile_ctor",
      "item_getModifiedField1a", "script_runByName", "Skill.v10",
      "item_getDamageRange", "MissileWeapon_ctor", "Root_ctor", "UnholyAura_ctor", "Player_weaponTrails", "dialog_22fbd0",
      "Player_processDamage", "Player_interact", "light_link2"};
  auto i = static_cast<unsigned>(s);
  if (i >= sizeof(names) / sizeof(*names))
    throw Error("unknown player service");
  return names[i];
}
Tick::Tick(Memory &m, std::uint32_t p, Policy selected,
           std::shared_ptr<NamedCallbacks> named, std::set<std::string> selected_external)
    : named_callbacks(named ? std::move(named) : std::make_shared<NamedCallbacks>()),
      external(std::move(selected_external)), mem(m), P(p), policy(selected) {
  if (P > UINT32_MAX - 0x58f)
    throw ResourceLimit("player object outside raw32");
  if (policy != Policy::RequireHandlers && policy != Policy::ReferenceZero)
    throw Error("unknown callback policy");
}
std::int32_t s8(std::uint64_t value) {
  auto low = std::uint32_t(value & 0xff);
  return low & 0x80 ? std::int32_t(low) - 256 : std::int32_t(low);
}
std::uint8_t Tick::u8(Address a) const { return std::uint8_t(mem.load(a, 1)); }
std::uint16_t Tick::u16(Address a) const { return std::uint16_t(mem.load(a, 2)); }
std::uint64_t Tick::u64(Address a) const { return mem.load(a, 8); }
void Tick::w8(Address a, std::uint64_t value) { write(a, 1, value); }
void Tick::w16(Address a, std::uint64_t value) { write(a, 2, value); }
void Tick::w32(Address a, std::uint64_t value) { write(a, 4, value); }
std::uint32_t Tick::u32(Address a) const {
  return std::uint32_t(mem.load(a, 4));
}
std::int32_t Tick::signed8(Address a) const {
  return player::s8(u8(a));
}
std::int32_t Tick::signed16(Address a) const {
  return s16(std::uint32_t(mem.load(a, 2)));
}
std::int32_t Tick::signed32(Address a) const { return s32(u32(a)); }
std::int32_t Tick::slot() const { return signed32(Address(P) + 0x3c8); }
std::uint32_t Tick::record() const {
  return std::uint32_t(std::int64_t(PLAYERS) +
                       std::int64_t(slot()) * std::int64_t(REC));
}
bool Tick::is_copy() const { return u32(Address(P) + 0xc) & 0x20000; }
void Tick::write(Address a, std::size_t size, std::uint64_t value) {
  if (a > UINT32_MAX || size > 8 || size > UINT64_C(0x100000000) - a)
    throw ResourceLimit("player write span");
  if (writes.size() >= 65536 || events.size() >= 65536)
    throw ResourceLimit("player history cap");
  if (size < 8)
    value &= (UINT64_C(1) << (size * 8)) - 1;
  Write w{std::uint32_t(a), size, value};
  writes.push_back(w);
  events.push_back({false, w, {}});
  mem.store(a, size, value);
}
Result Tick::invoke(Service s, std::vector<Argument> args) {
  service_name(s);
  if (calls.size() >= 65536 || events.size() >= 65536 || callbacks.size() > 95 || named_callbacks->size() > 4096)
    throw ResourceLimit("player callback/history cap");
  Call c{s, std::move(args), {}};
  calls.push_back(c);
  events.push_back({true, {}, c});
  auto named = named_callbacks->find(service_name(s));
  auto found = callbacks.find(s);
  if (named != named_callbacks->end() && named->second) {
    if (depth_ >= 64) throw ResourceLimit("player callback recursion cap");
    auto fn = named->second;
    ++depth_;
    try { auto result = fn(*this, c.args); --depth_; return result; }
    catch (...) { --depth_; throw; }
  }
  if (found == callbacks.end() || !found->second) {
    if (policy == Policy::ReferenceZero) {
      if (s == Service::AttachPoint) {
        if (c.args.size() == 7 && std::get<std::uint32_t>(c.args[5])) return AttachResult{};
        return Vec3{};
      }
      if (s == Service::AncestralQuery || s == Service::BattleCryAffects) return QueryResult{};
      if (s == Service::CollectArc || s == Service::CollectBox) return std::vector<std::uint32_t>{};
      return 0u;
    }
    throw MissingHandler(service_name(s));
  }
  if (depth_ >= 64)
    throw ResourceLimit("player callback recursion cap");
  auto fn = found->second;
  ++depth_;
  try {
    auto result = fn(*this, c.args);
    --depth_;
    return result;
  } catch (...) {
    --depth_;
    throw;
  }
}
Result Tick::call(const std::string &name, std::vector<Argument> args) {
  if (name.size() > 256 || named_callbacks->size() > 4096)
    throw ResourceLimit("player named callback budget");
  for (auto &arg : args) {
    if (auto v = std::get_if<std::int64_t>(&arg)) arg = std::uint32_t(*v);
    else if (auto v = std::get_if<std::uint64_t>(&arg)) arg = std::uint32_t(*v);
  }
  for (unsigned i = 0; i < static_cast<unsigned>(Service::Named); ++i)
    if (name == service_name(static_cast<Service>(i)))
      return invoke(static_cast<Service>(i), std::move(args));
  if (calls.size() >= 65536 || events.size() >= 65536)
    throw ResourceLimit("player history cap");
  Call c{Service::Named, std::move(args), name};
  calls.push_back(c); events.push_back({true, {}, c});
  auto found = named_callbacks->find(name);
  if (found == named_callbacks->end() || !found->second) throw MissingHandler(name);
  if (depth_ >= 64) throw ResourceLimit("player callback recursion cap");
  auto fn = found->second;
  ++depth_;
  try { auto value = fn(*this, c.args); --depth_; return value; }
  catch (...) { --depth_; throw; }
}
Result Tick::vcall(const std::string &cls, std::uint32_t obj,
                   unsigned table_offset, unsigned slot, std::vector<Argument> args) {
  auto table = u32(indexed(obj, table_offset));
  auto adjustment = signed16(indexed(table, std::int64_t(slot) * 8));
  auto self = obj + std::uint32_t(adjustment);
  args.insert(args.begin(), self);
  return call(cls + ".v" + std::to_string(slot), std::move(args));
}
Result Tick::helper(const std::string &name, Callback function, std::vector<Argument> args) {
  if (external.count(name)) return call(name, std::move(args));
  if (!function) throw MissingHandler("internal helper " + name);
  return function(*this, args);
}
std::uint32_t Tick::call(Service s, std::vector<Argument> args) {
  auto value = invoke(s, std::move(args));
  if (auto result = std::get_if<std::uint32_t>(&value)) return *result;
  throw Error("scalar callback must return a uint32");
}
std::uint32_t Tick::draw() {
  auto s = u32(SEED) * 0x19660du + 0x3c6ef35fu;
  write(SEED, 4, s);
  ++draws;
  return s >> 16;
}
std::uint32_t Tick::own_draw() {
  auto s = u32(Address(P) + 0x44) * 0x19660du + 0x3c6ef35fu;
  write(Address(P) + 0x44, 4, s);
  return s >> 16;
}
Address Tick::entry() {
  const Address p = P;
  if ((u32(FLAGS) & 0x100) &&
      !((u32(FRAME) ^ std::uint32_t(signed8(p + 0x230))) & 0xe0) &&
      !(own_draw() & 1)) {
    Vec3 v{};
    bool accepted = false;
    for (unsigned attempt = 0; attempt < 4096; ++attempt) {
      for (auto &x : v) {
        auto u = mul(from_int(own_draw()), TWO_M16);
        x = sub(add(u, u), ONE);
      }
      if (!lt(ONE,
              add(add(mul(v[0], v[0]), mul(v[1], v[1])), mul(v[2], v[2])))) {
        accepted = true;
        break;
      }
    }
    if (!accepted)
      throw ResourceLimit("breath rejection sampling cap");
    Vec3 position{add(u32(p + 0x10), add(v[0], v[0])),
                  add(u32(p + 0x14), add(v[1], v[1])),
                  add(u32(p + 0x18), add(v[2], v[2]))};
    auto r = mul(mul(from_int(own_draw()), TWO_M16), 0x41400000);
    position[2] = add(position[2], add(u32(p + 0x20), r));
    call(Service::FxSpawn, {0x11u, position});
  }
  call(Service::CreatureRun, {P, u32(p + 0x544)});
  call(Service::Knockback, {P, ONE});
  write(p + 0x228, 4, u32(Address(record()) + 0xac));
  auto range = mul(u32(LIGHT), 0x43700000);
  if (signed32(ALIVE) == 2)
    range = mul(range, 0x3f28f5c2);
  write(p + 0x4e4, 4, range);
  write(p + 0x4ec, 4, range);
  auto inv = div(ONE, mul(range, range));
  write(p + 0x4f0, 4, inv);
  write(p + 0x4e8, 4, inv);
  write(p + 0x4f8, 8, mem.load(p + 0x10, 8));
  write(p + 0x500, 4, u32(p + 0x18));
  write(p + 0x500, 4, add(u32(p + 0x500), 0x42800000));
  call(Service::LightRelink, {std::uint32_t(p + 0x4c8)});
  call(Service::LightUnlink, {std::uint32_t(p + 0x504)});
  if (!is_copy()) {
    write(Address(record()) + 0x4c27, 1,
          mem.load(Address(record()) + 0x4c27, 1) - 1);
    if (signed8(Address(record()) + 0x4c27) < 0)
      write(Address(record()) + 0x4c27, 1, 0);
  }
  if (signed16(p + 0x4b4) != 0) {
    auto v = std::uint32_t(mem.load(p + 0x4b4, 2) - 1) & 0xffff;
    write(p + 0x4b4, 2, v);
    if (s16(v) < 20)
      write(p + 0xc, 4, u32(p + 0xc) & ~2u);
  }
  const Address hp_rec = rec(slot());
  R0 = hp_rec;
  if (lt(ZERO, u32(hp_rec + 0xac))) {
    if (call(Service::CamFlag, {std::uint32_t(slot())}) || signed8(CUTSCENE) ||
        call(Service::Frozen) ||
        u32(indexed(PADS, std::int64_t(slot()) * std::int64_t(PAD)) + 8))
      write(p + 0x4b0, 2, 0);
    else {
      auto sl = slot();
      auto v = std::uint32_t(mem.load(p + 0x4b0, 2) + 1) & 0xffff;
      auto limit = signed16(p + 0x4b2);
      write(p + 0x4b0, 2, v);
      if (limit < s16(v)) {
        call(Service::Voice, {std::uint32_t(sl), 1u});
        write(p + 0x4b0, 2, 0);
        auto d = draw();
        write(p + 0x4b2, 2,
              (std::int64_t(d % 30) + std::int64_t(slot()) * 45) * 60 + 0xe10);
      }
    }
  }
  if (signed16(p + 0x4ae) > 0)
    write(p + 0x4ae, 2, mem.load(p + 0x4ae, 2) - 1);
  bool rumble = signed8(NETWORK) == 0;
  if (!rumble && !(u32(MISSION) != 0 && u32(MEDAL) != 0))
    rumble = !call(Service::MemcardBusy);
  if (rumble && lt(ZERO, u32(hp_rec + 0xac)) &&
      lt(div(u32(hp_rec + 0xac), u32(hp_rec + 0xb0)), 0x3ecccccc)) {
    auto phase = u32(FRAME) & 0x3f;
    if (phase == 0)
      call(Service::Rumble, {std::uint32_t(slot()), 0x3df5c28fu, ONE});
    if (phase == 20)
      call(Service::Rumble, {std::uint32_t(slot()), 0x3db851ebu, ONE});
  }
  return hp_rec;
}
std::string Tick::run_legacy_entry() {
  entry();
  const Address p = P;
  if (signed8(p + 0x231) == 0)
    throw Unsupported("ordinary Player_run after entry (0x1a5614) is unported");
  auto state = signed32(p + 0x58c);
  if (state == 0)
    call(Service::AnimStep, {P, ONE});
  else if (state == 1) {
    if (call(Service::AnimStep, {P, ONE}) & 1)
      call(Service::Footstep, {P});
    call(Service::MoveToDest, {P});
  }
  write(p + 0x488, 4, 0);
  return "scripted";
}
std::string Tick::run() {
  try {
    top();
    dispatch_state();
    tail();
  } catch (const Leave &leave) {
    return leave.what();
  }
  return "end";
}
void Tick::tail() {
  const Address p = P, light = p + 0x504;
  auto sl = slot();
  if (lt(ZERO, u32(indexed(0x4d8aa0, std::int64_t(sl) * 4)))) {
    call(Service::LightLink2, {std::uint32_t(light)});
    auto ability = selected_ability();
    if (!ability) throw Leave("ability button index");
    std::uint32_t y;
    if (strcmp(*ability, 0x536250) == 0 && !signed8(0x41b51d)) {
      auto c = cos16(signed16(p + 0x38));
      write(p + 0x534, 4, add(u32(p + 0x10), mul(c, 0x42200000)));
      auto s = sin16(signed16(p + 0x38));
      y = add(u32(p + 0x14), mul(s, 0x42200000));
    } else {
      auto x = u32(p + 0x10);
      y = u32(p + 0x14);
      write(p + 0x534, 4, x);
    }
    write(p + 0x538, 4, y);
    write(p + 0x53c, 4, add(u32(p + 0x18), 0x42100000));
    call(Service::LightRelink, {std::uint32_t(light)});
    auto wave = sin16(s16(u32(FRAME) * 6000u));
    auto radius = from_int(std::int64_t(to_int(mul(wave, 0x40800000))) + 0xb4);
    sl = slot();
    auto colour = indexed(0x8659a0, std::int64_t(sl) * 16);
    write(light + 0x1c, 4, radius);
    write(light + 0x24, 4, radius);
    auto inv = div(ONE, mul(radius, radius));
    write(light + 0x28, 4, inv);
    write(light + 0x20, 4, inv);
    auto fade = u32(indexed(0x4d8aa0, std::int64_t(sl) * 4));
    std::array<std::int32_t, 4> c{};
    for (unsigned k = 0; k < 4; ++k)
      c[k] = to_int(mul(from_int(signed32(colour + 4 * k)), fade));
    write(p + 0x514, 2, c[3]);
    write(p + 0x50e, 2, c[0]);
    write(p + 0x510, 2, c[1]);
    write(p + 0x512, 2, c[2]);
    write(p + 0x516, 2, c[0]);
    write(p + 0x518, 2, c[1]);
    write(p + 0x51a, 2, c[2]);
    write(p + 0x51c, 2, c[3]);
  }
  sl = slot();
  if (lt(ZERO, u32(rec(sl) + 0xac))) h_update_cast(P, pad);
  if (!is_copy()) {
    auto r = Address(record());
    auto count = mem.load(r + 0x4bca, 2);
    if (count) write(r + 0x4bca, 2, count - 1);
    count = mem.load(r + 0x4bc8, 1);
    if (count) {
      write(r + 0x4bc8, 1, count - 1);
      r = record();
      if (!mem.load(r + 0x4bc8, 1)) write(p + 0xc, 4, u32(p + 0xc) & 0xffbfffff);
    }
    r = record();
    count = mem.load(r + 0x4bc9, 1);
    if (count) write(r + 0x4bc9, 1, count - 1);
  }
  if (signed32(p + 0x58c) != 5) write(p + 0x488, 4, 0);
}
void Tick::top() {
  const Address hp_rec = entry(), p = P;
  if (signed8(p + 0x231)) {
    auto state = signed32(p + 0x58c);
    if (state == 0) call(Service::AnimStep, {P, ONE});
    else if (state == 1) {
      if (call(Service::AnimStep, {P, ONE}) & 1) call(Service::Footstep, {P});
      call(Service::MoveToDest, {P});
    }
    write(p + 0x488, 4, 0);
    throw Leave("scripted");
  }
  write(p + 0x4c4, 4, ONE);
  if (!signed8(p + 0x464) && !(u32(p + 0xc) & 8)) call(Service::WeaponTrails, {P});
  pad = choose_pad();
  const Address input = pad; // Python local survives callback mutation of self.pad.
  h_idle_fidget(P, input);
  if (call(Service::DialogOpen) && lt(ZERO, u32(Address(record()) + 0xac))) {
    auto target = call(Service::DialogTarget, {std::uint32_t(p + 0x10)});
    write(p + 0x38, 2, angle_approach(0x3e000000, MINUS_ONE, MINUS_ONE,
                                    signed16(p + 0x38), s32(target)));
  }
  auto s4 = std::int64_t(slot()) * 4;
  auto fade = add(u32(indexed(0x4d8aa0, s4)), u32(indexed(0x4d8a90, s4)));
  write(indexed(0x4d8aa0, s4), 4, fade);
  if (lt(ONE, fade)) write(indexed(0x4d8aa0, s4), 4, ONE);
  if (lt(u32(indexed(0x4d8aa0, s4)), ZERO)) write(indexed(0x4d8aa0, s4), 4, ZERO);
  write(indexed(0x4d8a40, s4), 4,
        std::int64_t(signed32(indexed(0x4d8a40, s4))) +
        std::int64_t(signed32(indexed(0x4d8ab0, s4))) * 3);
  s4 = std::int64_t(slot()) * 4;
  if (signed32(indexed(0x4d8a40, s4)) > 0x50) {
    write(indexed(0x4d8a40, s4), 4, 0x50);
    s4 = std::int64_t(slot()) * 4;
  }
  if (signed32(indexed(0x4d8a40, s4)) < 0) write(indexed(0x4d8a40, s4), 4, 0);
  write(p + 0x4c0, 4, 0); write(p + 0x4bc, 4, 0);
  auto weapon = u32(indexed(hp_rec, 0x4434 + 4 * signed8(hp_rec + 0x4a7c)));
  weapon0 = weapon;
  if (!weapon) write(p + 0x48c, 4, 0x3ea8f5c2);
  else {
    auto definition = u32(weapon);
    write(p + 0x48c, 4, signed8(Address(definition) + 0x1e) == 4 ? 0x3ea8f5c2 : 0x3e199999);
    if (signed8(Address(u32(weapon)) + 0x1e) == 4 && lt(u32(p + 0x48c), ZERO))
      write(p + 0x48c, 4, ZERO);
  }
  auto sl = slot();
  if (lt(ZERO, u32(rec(sl) + 0xac))) {
    auto pressed = u32(indexed(input, 0xc));
    if (pressed & u32(indexed(0x41ba10, std::int64_t(sl) * 4))) {
      use_potion(0); sl = slot(); pressed = u32(indexed(input, 0xc));
    }
    if ((pressed & u32(indexed(0x41ba20, std::int64_t(sl) * 4))) && !strstr(0x41b721, 0x535ed8))
      use_potion(1);
  }
  auto flags = u32(p + 0xc);
  if (!(flags & 0x20000)) {
    auto current = u32(hp_rec + 0x4a80);
    if (current) {
      s4 = std::int64_t(slot()) * 4;
      if (current != u32(indexed(0x41b990, s4)) && current != u32(indexed(0x41b9a0, s4)))
        write(hp_rec + 0x4a80, 4, 0);
    }
    if (signed32(p + 0x58c) != 5) {
      s4 = std::int64_t(slot()) * 4;
      auto pressed = u32(indexed(input, 0xc)), a = u32(indexed(0x41b990, s4));
      if (pressed & a) { write(hp_rec + 0x4a80, 4, a); write(hp_rec + 0x4a60, 2, 0); }
      else {
        auto b = u32(indexed(0x41b9a0, s4));
        if (pressed & b) { write(hp_rec + 0x4a80, 4, b); write(hp_rec + 0x4a60, 2, 1); }
      }
      flags = u32(p + 0xc);
    }
  }
  if (signed32(hp_rec + 0x50) == 0 && u32(p + 0x374) && signed32(p + 0x58c) != 12) {
    auto mana = sub(u32(hp_rec + 0xc0), mul(u32(hp_rec + 0xc4), 0x3ada740d));
    write(hp_rec + 0xc0, 4, mana);
    if (le(mana, ZERO)) { write(hp_rec + 0xc0, 4, ZERO); write(p + 0x374, 4, 0); }
  }
  if (!(flags & 8)) {
    h_regen_health(P); h_regen_mana(P); h_tick_enchant_timers(P);
    auto plane = u32(0x4f7188);
    if (lt(0xc7afc800, plane) && lt(u32(p + 0x18), plane)) {
      if (!u32(0x44f20c)) write(p + 0xc, 4, u32(p + 0xc) | 2);
      write(p + 0x2dc, 4, 0); write(p + 0xc8, 4, 0x49742400);
    }
    call(Service::ProcessDamage, {P});
    if (!u32(0x41b4f4) && !u32(LOCKOUT) && !call(Service::DialogOpen) &&
        !call(Service::CamFlag, {std::uint32_t(slot())}) && !is_copy()) {
      h_weapon_swap_input(P, input, 0);
      if (!u32(indexed(0x41b818, std::int64_t(slot()) * 4)))
        call(Service::Interact, {P, std::uint32_t(input), 0u});
    }
  }
  instant_abilities();
  sl = slot();
  write(p + 0x454, 4, 0);
  if ((u32(indexed(input, 8)) & u32(indexed(0x41b9d0, std::int64_t(sl) * 4))) &&
      !(u32(FLAGS) & 0x10000000) && h_can_block(std::uint32_t(sl), u32(p + 0x58c)))
    write(p + 0x454, 4, 1);
}
void Tick::instant_abilities() {
  const Address p = P, input = pad;
  auto forced = u32(p + 0x488);
  instant = 0;
  if (!forced && (u32(indexed(input, 8)) & 0xf000)) return;
  if (signed16(p + 0x4ae)) { write(p + 0x488, 4, 0); return; }
  for (unsigned i = 0; i < 2; ++i) {
    std::uint32_t ability;
    const Address button = i ? 0x41b9a0 : 0x41b990;
    if (forced) ability = forced;
    else {
      auto sl = slot();
      auto mask = u32(indexed(button, std::int64_t(sl) * 4));
      if (!(u32(indexed(input, 0xc)) & mask)) continue;
      auto offset = std::int64_t(sl) * std::int64_t(REC);
      auto index = signed8(indexed(PLAYERS, offset + 0x4a84 + i));
      ability = std::uint32_t(offset + std::int64_t(index) * 0x68 + PLAYERS + 0x4440);
    }
    if (!ability || signed8(Address(ability) + 0x25) != 7) {
      if (forced) return;
      continue;
    }
    if (strcmp(ability, 0x535e30)) return;
    auto skill = skill_of(ability);
    if (!vcall(Service::SkillCanUse, skill, 0x60, 1, {P})) {
      write(p + 0x488, 4, 0); return;
    }
    auto rank = vcall(Service::PlayerRank, P, 0x48, 39,
                      {std::uint32_t(signed16(Address(ability) + 0x20))});
    auto cost = vcall(Service::SkillCost, skill, 0x60, 5, {rank});
    auto sl = slot();
    if (!le(cost, u32(rec(sl) + 0xc0))) {
      call(Service::Voice, {std::uint32_t(sl), 4u});
      write(p + 0x488, 4, 0); return;
    }
    instant = UINT32_MAX;
    if (u32(p + 0x488)) return;
    instant = u32(indexed(button, std::int64_t(slot()) * 4));
    return;
  }
}

Address Tick::choose_pad() {
  auto sl = slot();
  const Address p = P;
  if (u32(LOCKOUT) != 0 && signed8(NETWORK) == 0)
    return ZERO_PAD;
  if (call(Service::DialogOpen) || signed16(p + 0x4b4) != 0 ||
      signed8(0x41b51d) != 0 || u32(MEDAL) != 0 ||
      call(Service::CamFlag, {std::uint32_t(sl)}) ||
      call(Service::TaskRunning, {0x535eb8u}) ||
      call(Service::TaskRunning, {0x535ec8u}) ||
      !call(Service::SlotActive, {std::uint32_t(sl)}) ||
      (u32(p + 0xc) & 0x400000) || u32(0x503890) != 0 ||
      signed8(0x4f66c8) != 0 || le(u32(rec(sl) + 0xac), ZERO))
    return ZERO_PAD;
  return indexed(PADS, std::int64_t(sl) * std::int64_t(PAD));
}
void Tick::idle_fidget(std::uint32_t player, Address pad) {
  const Address p = player;
  auto ly = u32(indexed(pad, 0x18)), lx = u32(indexed(pad, 0x14));
  auto length = sqrt(add(mul(lx, lx), mul(ly, ly)));
  if (lt(0x3c23d70a, length) && signed8(p + 0x463) != 0) {
    write(p + 0x463, 1, 0);
    if (signed8(p + 0x118) - 1 < 4)
      return;
    for (int i = 4, off = 0x1a0;; ++i, off += 0x20) {
      write(p + 0xc + off, 4, ONE);
      write(p + 0x10 + off, 4, 0xbdaaaaaa);
      if (signed8(p + 0x118) - 1 < i + 1)
        break;
    }
    return;
  }
  if (signed8(p + 0x118) != 4 || draw() >= 0xe2 || signed8(p + 0x463) != 0)
    return;
  auto r = rec(signed32(p + 0x3c8));
  auto cls = signed8(r + 0x4a86);
  std::uint32_t name = 0x537628;
  switch (cls) {
  case 1:
    name = 0x5375d8;
    break;
  case 2:
    name = 0x5375e8;
    break;
  case 3:
    name = 0x5375f8;
    break;
  case 4:
    name = 0x537608;
    break;
  case 5:
    name = 0x537bf8;
    break;
  case 7:
    name = 0x537618;
    break;
  }
  auto obj = u32(r + 0x43d8);
  auto anm = call(Service::ResFind, {u32(u32(Address(obj) + 0x24)), name});
  call(Service::AnimStart, {std::uint32_t(p + 0x114), anm, 1u, HALF});
  write(p + 0x463, 1, 1);
}
std::uint32_t Tick::can_block(std::uint32_t sl, std::uint32_t state) {
  auto r = rec(s32(sl));
  auto two = u32(r + 0x43f8);
  if (s32(state) == 11 || (two != 0 && signed8(Address(u32(two)) + 0x1e) == 4))
    return 0;
  if (u32(r + 0x43f8) == 0 && u32(r + 0x43f0) == 0)
    return u32(r + 0x43fc) != 0;
  return 1;
}
void Tick::try_block_move(std::uint32_t player, Address pad) {
  const Address p = player;
  if ((u32(FLAGS) & 0x10000000) || mem.load(p + 0x47f, 1) == 0)
    return;
  auto sl = signed32(p + 0x3c8);
  if (!(u32(indexed(pad, 0xc)) &
        u32(indexed(0x41ba30, std::int64_t(sl) * 4))) ||
      !(u32(indexed(pad, 8)) & u32(indexed(0x41b9d0, std::int64_t(sl) * 4))))
    return;
  write(p + 0x58c, 4, 11);
  auto cls = signed8(rec(sl) + 0x4a86);
  auto table = u32(p + 0x24);
  write(p + 0x481, 1, 0x2f);
  call(Service::AnimBlend,
       {std::uint32_t(p + 0x114),
        u32(indexed(table, std::int64_t(cls) * 0x68 + 0xa4)), 0x808u,
        0x3dccccccu, 0x3e999999u});
}
std::uint32_t Tick::has_anim(std::uint32_t player, std::uint32_t name) {
  const Address p = player;
  auto anm = call(Service::ResFind, {u32(u32(p + 0x24)), name});
  for (int i = 0; i < signed8(p + 0x118); ++i)
    if (u32(p + 0x11c + 0x20 * i) == anm)
      return 1;
  return 0;
}
std::int32_t Tick::skill_rank(std::uint32_t sl, std::uint32_t id) {
  auto a = rec(s32(sl)) + 0x4b08;
  for (unsigned i = 0; i < 4096; ++i, a += 4) {
    auto skill = signed16(a);
    if (skill == -1)
      return 0;
    if (skill == s16(id))
      return signed16(a + 2);
  }
  throw ResourceLimit("unterminated skill rank list exceeds 4096 pairs");
}
std::int32_t Tick::passive_level(std::uint32_t sl, std::uint64_t mask) {
  if (sl > 4)
    return 0;
  auto r = rec(s32(sl));
  auto table = indexed(0x4ef538, std::int64_t(signed32(r + 0x50)) * 0xa88);
  for (unsigned n = 0; n < 48; ++n)
    if (mem.load(table + n * 0x38, 8) == mask) {
      auto level = signed8(r + 0xef + n);
      return level < 0 ? 0 : level;
    }
  return 0;
}
std::int32_t Tick::item_modifier(Address item, std::int32_t wanted,
                                 unsigned field) const {
  for (unsigned k = 0; k < 4; ++k) {
    auto at = indexed(item, 0x24 + 8 * k);
    if (signed16(at) == wanted)
      return signed16(indexed(at, field));
  }
  return 0;
}
std::string Tick::cstr(Address a, std::int64_t limit) const {
  std::string out;
  while (std::int64_t(out.size()) < limit) {
    if (out.size() >= creature_hp::MAX_MEMORY_BYTES)
      throw ResourceLimit("player string output cap");
    auto b = u8(indexed(a, std::int64_t(out.size())));
    if (!b) break;
    out.push_back(static_cast<char>(b));
  }
  return out;
}
void Tick::hud_notice(unsigned index, std::uint32_t frames) {
  write(indexed(0x41b828, std::int64_t(slot()) * 0x20 + index * 4), 4, frames);
  auto row = call(Service::NoticeCount, {std::uint32_t(slot()), index});
  write(indexed(0x41b8a8, std::int64_t(slot()) * 0x20 + index * 4), 4, row);
}
void Tick::regen_health(std::uint32_t player) {
  const Address p = player;
  if (u32(p + 0xc) & 0x20000) return;
  auto sl = signed32(p + 0x3c8);
  auto r = rec(sl);
  auto hp = u32(r + 0xac);
  if (le(hp, ZERO)) return;
  auto top = u32(r + 0xb0);
  if (lt(hp, top)) {
    auto amount = mul(top, 0x38d1b717);
    auto level = from_int(h_passive_level(std::uint32_t(sl), 0x10));
    auto bonus = signed8(p + 0x477);
    if (lt(ZERO, level))
      amount = mul(amount, add(mul(level, 0x3dcccccc), 0x3f999999));
    amount = add(amount, mul(from_int(std::uint32_t(bonus)), 0x38d1b717));
    for (unsigned i = 0; i < 17; ++i) {
      auto item = u32(r + 0x43f0 + 4 * i);
      if (item && u32(item)) {
        auto v = item_modifier(item, 0xb, 2);
        amount = mul(amount, add(mul(from_int(std::uint32_t(v)), 0x3c23d70a), ONE));
      }
    }
    hp = add(u32(r + 0xac), amount);
    top = u32(r + 0xb0);
    write(r + 0xac, 4, hp);
    if (lt(top, hp)) write(r + 0xac, 4, top);
  }
  auto pending = u32(r + 0xb4);
  if (lt(ZERO, pending)) {
    auto rate = u32(r + 0xb8);
    hp = add(u32(r + 0xac), rate);
    write(r + 0xac, 4, hp);
    if (lt(top, hp)) {
      write(r + 0xac, 4, top);
      pending = ZERO;
      write(r + 0xb4, 4, ZERO);
    }
    auto left = sub(pending, rate);
    write(r + 0xb4, 4, left);
    if (lt(left, ZERO)) write(r + 0xb4, 4, ZERO);
  }
}
void Tick::regen_mana(std::uint32_t player) {
  const Address p = player;
  if (u32(p + 0xc) & 0x20000) return;
  auto sl = signed32(p + 0x3c8);
  auto r = rec(sl);
  if (le(u32(r + 0xac), ZERO)) return;
  auto mana = u32(r + 0xc0), top = u32(r + 0xc4);
  if (lt(mana, top)) {
    auto amount = mul(top, 0x38d1b717);
    auto level = from_int(h_passive_level(std::uint32_t(sl), 0x200));
    auto bonus = signed8(p + 0x475);
    if (lt(ZERO, level))
      amount = mul(amount, add(mul(level, 0x3e199999), 0x3f999999));
    amount = add(amount, mul(from_int(std::uint32_t(bonus)), 0x38d1b717));
    if (signed32(r + 0x50) == 0) amount = ZERO;
    for (unsigned i = 0; i < 17; ++i) {
      auto item = u32(r + 0x43f0 + 4 * i);
      if (item && u32(item)) {
        auto v = item_modifier(item, 0xc, 2);
        amount = mul(amount, add(mul(from_int(std::uint32_t(v)), 0x3c23d70a), ONE));
      }
    }
    auto type = call(Service::BlessedType);
    auto spell = call(Service::FindSpell, {player, type});
    if (spell) {
      auto v = call(Service::BlessedStat, {spell});
      amount = add(amount, mul(amount, mul(from_int(v), 0x3c23d70a)));
    }
    auto rank = h_skill_rank(std::uint32_t(signed32(p + 0x3c8)), 0x13);
    if (rank) {
      auto v = call(Service::RegenerationStat, {u32(0x41b170 + 4 * 0x13), std::uint32_t(rank)});
      amount = add(amount, mul(amount, mul(from_int(v), 0x3c23d70a)));
    }
    mana = add(u32(r + 0xc0), amount);
    top = u32(r + 0xc4);
    write(r + 0xc0, 4, mana);
    if (lt(top, mana)) write(r + 0xc0, 4, top);
  }
  auto pending = u32(r + 0xc8);
  if (lt(ZERO, pending)) {
    auto rate = u32(r + 0xbc);
    mana = add(u32(r + 0xc0), rate);
    write(r + 0xc0, 4, mana);
    if (lt(top, mana)) {
      write(r + 0xc0, 4, top);
      pending = ZERO;
      write(r + 0xc8, 4, ZERO);
    }
    auto left = sub(pending, rate);
    write(r + 0xc8, 4, left);
    if (lt(left, ZERO)) write(r + 0xc8, 4, ZERO);
  }
}
void Tick::tick_enchant_timers(std::uint32_t player) {
  const Address p = player;
  if (u32(p + 0xc) & 0x20000) return;
  auto r = rec(signed32(p + 0x3c8));
  for (unsigned i = 0; i < 17; ++i) {
    auto item = u32(r + 0x43f0 + 4 * i);
    if (!item || !u32(item)) continue;
    Address entry = 0x537c08;
    unsigned count = 0;
    while (signed32(entry) != -1) {
      if (++count > 4096) throw ResourceLimit("unterminated timed modifier list exceeds 4096 ids");
      auto wanted = signed32(entry);
      entry += 4;
      auto frames = item_modifier(item, wanted, 6);
      if (frames < 1) continue;
      auto left = frames - 1;
      auto value = item_modifier(item, wanted, 2);
      auto third = item_modifier(item, wanted, 4);
      for (unsigned k = 0; k < 4; ++k) {
        auto at = indexed(item, 0x24 + 8 * k);
        if (signed16(at) == -1 || signed16(at) == wanted) {
          write(at, 2, std::uint32_t(wanted));
          write(at + 2, 2, std::uint32_t(value));
          write(at + 4, 2, std::uint32_t(third));
          write(at + 6, 2, std::uint32_t(left));
          break;
        }
      }
      if (left < 1)
        for (unsigned k = 0; k < 4; ++k) {
          auto at = indexed(item, 0x24 + 8 * k);
          if (signed16(at) == wanted) write(at, 2, 0xffff);
        }
    }
  }
}
Result Tick::use_potion(std::uint32_t kind) {
  return h_use_potion_real(P, kind, 0, 0, UINT32_MAX);
}
void Tick::use_potion_real(std::uint32_t player, std::uint32_t kind,
                          std::uint32_t list_bag, std::uint32_t list_cell,
                          std::uint32_t list_panel) {
  if (kind > 1) throw Unsupported("potion kind must be health (0) or mana (1)");
  const Address p = player;
  if (u32(p + 0xc) & 0x20000) return;
  auto r = rec(signed32(p + 0x3c8));
  if (le(u32(r + 0xac), ZERO)) return;
  auto rate_hp = fmin(mul(u32(r + 0xb0), 0x3b088888), 0x3effffff);
  auto rate_mana = fmin(mul(u32(r + 0xc4), 0x3b088888), 0x3effffff);
  auto have = kind == 0 ? add(u32(r + 0xac), u32(r + 0xb4))
                        : add(u32(r + 0xc0), u32(r + 0xc8));
  write(r + 0xb8, 4, rate_hp);
  write(r + 0xbc, 4, rate_mana);
  unsigned top_at = kind == 0 ? 0xb0 : 0xc4;
  unsigned now_at = kind == 0 ? 0xac : 0xc0;
  unsigned pending_at = kind == 0 ? 0xb4 : 0xc8;
  if (le(u32(r + top_at), have)) return;
  std::array<int, 9> found;
  found.fill(-1);
  unsigned iterations = 0;
  for (unsigned size = 0; size < 9; ++size) {
    for (int i = 0; i < signed32(r + 0x3740); ++i) {
      if (++iterations > 65536) throw ResourceLimit("potion inventory scan cap");
      auto name = u32(u32(r + 0x2540 + i * 0x48));
      if (cstr(name) == cstr(u32((kind == 0 ? 0x4ef478 : 0x4ef4a0) + 4 * size)))
        found[size] = i;
    }
  }
  // Python // is floor division, including negative/nonfinite conversion results.
  auto missing = to_int(add(sub(u32(r + top_at), have), ONE));
  auto want = missing / 60;
  if (missing < 0 && missing % 60) --want;
  if (want > 8) want = 8;
  // Negative Python list indices -9..-1 are observable in pathological FPU inputs.
  auto index = want < 0 ? std::int64_t(want) + 9 : want;
  if (index < 0 || index > 8) throw Unsupported("potion size outside reference list domain");
  if (found[std::size_t(index)] < 0) {
    if (found[8] >= 0) want = 8;
    else {
      want = -1;
      for (int size = 7; size >= 0; --size)
        if (found[size] >= 0) { want = size; break; }
    }
  }
  if (want == -1) { hud_notice(kind == 0 ? 1 : 2, 0x78); return; }
  auto pending = add(u32(r + pending_at), mul(from_int(std::uint32_t(want + 1)), 0x42700000));
  auto now = u32(r + now_at), top = u32(r + top_at);
  write(r + pending_at, 4, pending);
  if (lt(top, add(now, pending))) write(r + pending_at, 4, sub(top, now));
  auto sl = signed32(p + 0x3c8);
  if (call(Service::LocalSlot, {std::uint32_t(sl)})) {
    auto seq = call(Service::ResFind, {0x535ef0u, kind == 0 ? 0x537aa0u : 0x537b60u});
    call(Service::SoundPlay, {seq, u32(rec(signed32(p + 0x3c8)) + 0x43d8), ONE});
  }
  if (signed8(0x41b69d) != 0 &&
      call(Service::LocalSlot, {std::uint32_t(signed32(p + 0x3c8))}) && u32(0x4ebbb0) != 0) {
    sl = signed32(p + 0x3c8);
    // This expression is explicitly wrapped by the reference, unlike record spans.
    auto cell = std::uint32_t(std::int64_t(sl) * 0x23988 + std::int64_t(u32(0x4ebbb0)) +
                             0x17bb0 + std::int64_t(s32(list_bag)) * 0x1518 +
                             std::int64_t(s32(list_cell)) * 0x48);
    call(Service::SendDrop, {std::uint32_t(sl), std::uint32_t(signed16(Address(cell) + 0x22)),
                            std::uint32_t(signed8(Address(u32(cell)) + 0x1d)),
                            list_bag, list_cell, list_panel, 2u});
  } else {
    index = want < 0 ? std::int64_t(want) + 9 : want;
    auto item = r + 0x2540 + found[std::size_t(index)] * 0x48;
    auto left = (mem.load(item + 4, 2) - 1) & 0xffff;
    write(item + 4, 2, left);
    if (left == 0)
      call(Service::RemoveItem, {std::uint32_t(signed32(p + 0x3c8)), std::uint32_t(found[std::size_t(index)]), 2u, 0u});
  }
  auto count = call(Service::CountPotions, {std::uint32_t(signed32(p + 0x3c8)), kind});
  if (std::uint32_t(count - 1) < 5) hud_notice(kind == 0 ? 4 : 5, 0xf0);
  sl = signed32(p + 0x3c8);
  auto rn = rec(sl);
  write(rn + 0x120, 4, 0);
  iterations = 0;
  for (unsigned bag = 0; bag < 3; ++bag)
    for (int i = 0; i < signed32(rn + 0x1330 + bag * 0x1208); ++i) {
      if (++iterations > 65536) throw ResourceLimit("potion load recount cap");
      auto weight = call(Service::ItemWeight, {std::uint32_t(sl), std::uint32_t(rn + 0x130 + bag * 0x1208 + i * 0x48)});
      write(rn + 0x120, 4, add(u32(rn + 0x120), weight));
    }
  Address counter = kind == 0 ? 0x4f85ca : 0x4f85c8;
  write(counter, 2, mem.load(counter, 2) + 1);
}
Vec3 Tick::vec(Address a) const {
  return {u32(indexed(a, 0)), u32(indexed(a, 4)), u32(indexed(a, 8))};
}
std::uint32_t Tick::vcall(Service service, std::uint32_t obj,
                          unsigned offset, unsigned slot,
                          std::vector<Argument> args) {
  auto table = u32(indexed(obj, offset));
  auto self = std::uint32_t(std::int64_t(obj) + signed16(indexed(table, 8 * slot)));
  args.insert(args.begin(), self);
  return call(service, std::move(args));
}
std::uint32_t Tick::skill_of(std::uint32_t ability) const {
  return u32(indexed(0x41b170, 4 * signed16(Address(ability) + 0x20)));
}
std::pair<Vec3, Vec3> Tick::hands_midpoint(std::uint32_t player, bool) {
  const Address p = player;
  auto attach = [&](unsigned hand) {
    auto result = invoke(Service::AttachPoint,
      {u32(p + 0x544), vec(p + 0x10), std::uint32_t(signed16(p + 0x38)),
       std::uint32_t(p + 0x114), hand, 0u, ONE});
    if (auto value = std::get_if<Vec3>(&result)) return *value;
    throw Error("attach point callback must return a vector");
  };
  auto a = attach(0);
  auto b = attach(1); // Read model, position and facing again after callback.
  return {a, b}; // Reference does not use limit_height or compute the midpoint here.
}
namespace {
Vec3 aim_point(const Vec3 &a, const Vec3 &b, Raw base) {
  Vec3 point{mul(add(a[0], b[0]), HALF), mul(add(a[1], b[1]), HALF),
             mul(add(a[2], b[2]), HALF)};
  auto diff = f2d(sub(point[2], base));
  if (dcmp(diff, 0) < 0) diff = dsub(0, diff);
  if (dcmp(diff, UINT64_C(0x404d000000000000)) > 0)
    point[2] = add(base, 0x42680000);
  return point;
}
}
void Tick::update_cast(std::uint32_t player, Address pad) {
  const Address p = player;
  auto [a, b] = hands_midpoint(player, true);
  auto point = aim_point(a, b, u32(p + 0x18));
  vcall(Service::PlayerAim, player, 0x48, 17, {point});
  if (u32(p + 0x488)) return;
  auto r = rec(signed32(p + 0x3c8));
  auto held = u32(indexed(pad, 8)) & u32(r + 0x4a80);
  if (held && signed32(p + 0x58c) == 5 && !le(u32(r + 0xc0), ZERO)) return;
  auto track = s32(call(Service::FindTrack, {std::uint32_t(p + 0x114), 0x8000u}));
  if (track >= 0 && !le(ONE, u32(indexed(p, 0x124 + std::int64_t(track) * 0x20)))) return;
  write(indexed(0x4d8ab0, std::int64_t(signed32(p + 0x3c8)) * 4), 4, UINT32_MAX);
  write(indexed(0x4d8a90, std::int64_t(signed32(p + 0x3c8)) * 4), 4, 0xbd4ccccc);
  if (track >= 0) {
    auto weight = indexed(p, 0x12c + std::int64_t(track) * 0x20);
    write(weight, 4, fmin(u32(weight), ONE));
    write(indexed(p, 0x130 + std::int64_t(track) * 0x20), 4, 0xbd6eeeee);
  }
  write(p + 0x47b, 1, 0);
  if (signed32(p + 0x58c) == 5) write(p + 0x58c, 4, 2);
  auto handle = u32(p + 0x588);
  if (handle) {
    auto spell = u32(Address(handle) + 0xc);
    if (spell) vcall(Service::SpellStop, spell, 0x14, 1, {3u});
  }
}
std::uint32_t Tick::weapon_swap_input(std::uint32_t player, Address pad,
                                       std::uint32_t force) {
  const Address p = player;
  std::uint32_t result = 0;
  auto sl = signed32(p + 0x3c8);
  auto r = rec(sl);
  if (signed32(p + 0x58c) == 11 || signed32(p + 0x58c) == 8) {
    write(r + 0x4a7d, 1, 0);
    sl = signed32(p + 0x3c8);
  }
  if (u32(indexed(pad, 8)) & u32(indexed(0x41b9d0, std::int64_t(sl) * 4)))
    if (can_block(std::uint32_t(sl), u32(p + 0x58c)) && force == 0) return 0;
  auto primary = [&] { return u32(indexed(r, 0x4434 + 4 * signed8(r + 0x4a7c))); };
  if (!(u32(FLAGS) & 0x10000000)) {
    if (signed32(p + 0x58c) != 2 && signed32(p + 0x58c) != 3)
      if ((u32(indexed(pad, 0xc)) & u32(indexed(0x41ba30, std::int64_t(sl) * 4))) || force)
        write(r + 0x4a7d, 1, 1);
    if (signed32(p + 0x58c) == 2) {
      unsigned tries = 0;
      auto step = [&](bool increment) {
        do {
          auto b = std::uint32_t(mem.load(r + 0x4a7c, 1) + (increment ? 1 : -1)) & 255;
          write(r + 0x4a7c, 1, b);
          auto signed_b = b & 128 ? int(b) - 256 : int(b);
          if (increment ? signed_b > 1 : signed_b < 0)
            write(r + 0x4a7c, 1, increment ? 0 : 1);
          ++tries;
        } while (primary() == 0 && tries < 3);
      };
      if (signed8(r + 0x4a7d) == 0) {
        if ((u32(indexed(pad, 0xc)) & u32(indexed(0x41ba30, std::int64_t(signed32(p + 0x3c8)) * 4))) || force)
          step(false);
      } else {
        write(r + 0x4a7d, 1, 0);
        step(true);
      }
      auto which = signed8(r + 0x4a7c);
      auto item = primary();
      if (item) {
        if (u32(r + 0x43f8) != item && u32(r + 0x43f0) != item) {
          result = 1;
          call(Service::EquipItem, {std::uint32_t(signed32(p + 0x3c8)), item, 0u});
          if (signed8(r + 0x4a7c) == 1 && signed8(Address(u32(primary())) + 0x1e) == 6)
            hud_notice(7, 0xf0);
        }
        if (which == 0) {
          auto second = u32(r + 0x443c);
          if (second && u32(r + 0x43f4) != second) {
            result = 1;
            call(Service::EquipItem, {std::uint32_t(signed32(p + 0x3c8)), second, 0u});
          }
        }
      }
    }
  }
  call(Service::RecalcDamage, {player, primary(), 1u});
  return result;
}
void Tick::cast_skill(std::uint32_t player, std::uint32_t ability) {
  const Address p = player;
  if (signed16(p + 0x4ae) > 0 || !ability) return;
  if (cstr(ability) != cstr(0x535e30)) return;
  auto skill = skill_of(ability);
  bool copy = u32(p + 0xc) & 0x20000;
  if (!(u32(Address(skill) + 4) & 1) && copy) return;
  if (!vcall(Service::SkillCanUse, skill, 0x60, 1, {player})) return;
  auto [a, b] = hands_midpoint(player, true);
  auto point = aim_point(a, b, u32(p + 0x18));
  auto collected = invoke(Service::CollectArc,
    {vec(p + 0x10), std::uint32_t(signed16(p + 0x38)), 0u, 0x43aa0000u, 0x41a00000u, ZERO});
  auto objects = std::get_if<std::vector<std::uint32_t>>(&collected);
  if (!objects) throw Error("arc callback must return an object list");
  if (objects->size() > 4096) throw ResourceLimit("arc object list cap");
  std::uint32_t target = 0;
  for (auto obj : *objects)
    if (vcall(Service::PlayerEnemy, player, 0x48, 20, {obj})) { target = obj; break; }
  auto facing = signed16(p + 0x38);
  if (target) {
    auto angle = atan2_16(sub(u32(Address(target) + 0x10), u32(p + 0x10)),
                          sub(u32(Address(target) + 0x14), u32(p + 0x14)));
    facing = s16(std::uint32_t(0x41c2 - angle));
  }
  auto rank = vcall(Service::PlayerRank, player, 0x48, 39,
                    {std::uint32_t(signed16(Address(ability) + 0x20))});
  auto spell = vcall(Service::SkillCast, skill, 0x60, 2,
                    {rank, player, point, std::uint32_t(facing)});
  if (spell) {
    if (u32(p + 0xc) & 0x20000) vcall(Service::SpellStop, spell, 0x14, 1, {3u});
    else {
      call(Service::AddSpell, {player, spell});
      if (signed32(Address(skill) + 8) == 1)
        call(Service::HandleLink, {std::uint32_t(p + 0x588), spell + 4u, spell});
    }
  }
  write(p + 0x4ae, 2, mem.load(Address(skill) + 0xc, 2));
  rank = vcall(Service::PlayerRank, player, 0x48, 39,
               {std::uint32_t(signed16(Address(ability) + 0x20))});
  auto cost = vcall(Service::SkillCost, skill, 0x60, 5, {rank});
  if (signed32(Address(skill) + 8) == 1) cost = mul(cost, 0x3c888888);
  auto r = rec(signed32(p + 0x3c8));
  write(r + 0xc0, 4, sub(u32(r + 0xc0), cost));
  call(Service::HandleClear, {StackHandle{}});
}
// Bounded action-state family. Services remain supplied synchronous callbacks.
std::uint32_t Tick::anim_step(std::uint32_t speed) { return call(Service::AnimStep, {P, speed}); }
std::uint32_t Tick::move(std::uint32_t mode, Argument ext) {
  return call(Service::Move, {P, mode, std::uint32_t(indexed(pad, 0)), ext});
}
std::int32_t Tick::frames() const { return s32(u32(FRAME) - u32(Address(P) + 0x448)); }
Address Tick::top_track(std::int64_t offset) const {
  // A raw32 field cannot be reached with an offset beyond this range, even
  // with the largest signed-byte track adjustment. Check before adding.
  if (offset < -INT64_C(0x100001000) || offset > INT64_C(0x100001000))
    throw ResourceLimit("track offset outside raw32");
  return indexed(P, std::int64_t(signed8(Address(P) + 0x118)) * 0x20 + offset);
}
void Tick::stop_loop_sound() {
  if (signed32(Address(P) + 0x4a8) != -1) {
    call(Service::SoundStop, {u32(Address(P) + 0x4a8)});
    write(Address(P) + 0x4a8, 4, UINT32_MAX);
  }
}
void Tick::clear_extra_tracks() {
  for (int i = 1; i < signed8(Address(P) + 0x119); ++i) {
    write(Address(P) + 0x144 + 0x20 * (i - 1), 4, 0);
    write(Address(P) + 0x14c + 0x20 * (i - 1), 4, 0);
  }
}
std::uint32_t Tick::slow_scale(std::uint32_t speed) const {
  auto slow = std::uint32_t(mem.load(Address(P) + 0x2fc, 1));
  if (slow - 1 < 0x3c) speed = mul(speed, mul(from_int(60 - signed8(Address(P) + 0x2fc)), 0x3c888888));
  return speed;
}
std::uint32_t Tick::haste_of(Address item) const {
  return mul(from_int(item_modifier(item, 0x14, 2)), 0x3d3851eb);
}
std::uint32_t Tick::anim_name(std::int64_t offset) const {
  if (offset < -INT64_C(0x100004000) || offset > INT64_C(0x100004000))
    throw ResourceLimit("animation offset outside raw32");
  return u32(indexed(u32(Address(P) + 0x24), std::int64_t(signed8(Address(record()) + 0x4a86)) * 0x68 + offset));
}
std::optional<std::uint32_t> Tick::selected_ability() const {
  auto r = record(); auto which = signed16(Address(r) + 0x4a60);
  if (which != 0 && which != 1) return std::nullopt;
  auto index = signed8(Address(r) + (which == 0 ? 0x4a84 : 0x4a85));
  return std::uint32_t(std::int64_t(r) + 0x4440 + std::int64_t(index) * 0x68);
}
void Tick::sound(std::uint32_t entry, std::uint32_t obj) {
  auto seq = call(Service::ResFind, {0x535ef0u, entry});
  call(Service::SoundPlay, {seq, obj, ONE});
}
Vec3 Tick::unit_sphere_shared() {
  for (unsigned attempt = 0; attempt < 4096; ++attempt) {
    Vec3 v;
    for (auto &x : v) { auto u = mul(from_int(draw()), TWO_M16); x = sub(add(u, u), ONE); }
    if (!lt(ONE, add(add(mul(v[0], v[0]), mul(v[1], v[1])), mul(v[2], v[2])))) return v;
  }
  throw ResourceLimit("shared unit sphere rejection cap");
}
void Tick::state_hit() {
  const Address p = P;
  if (signed8(p + 0x47b) == 0) {
    if (signed8(p + 0x47f)) { write(top_track(0xf0), 4, 0xbd6eeeee); write(p + 0x47f, 1, 0); }
  } else {
    stop_loop_sound();
    write(indexed(0x4d8ab0, std::int64_t(slot()) * 4), 4, UINT32_MAX);
    write(indexed(0x4d8a90, std::int64_t(slot()) * 4), 4, 0xbd4ccccc);
    write(top_track(0xf0), 4, 0xbd6eeeee); write(p + 0x47b, 1, 0);
  }
  anim_step();
  if (!(mem.load(p + 0x11a, 2) & 8)) {
    write(p + 0x480, 1, 0); write(p + 0x44c, 4, 0); write(p + 0x58c, 4, 2);
    write(p + 0x448, 4, u32(FRAME)); write(p + 0x3cc, 1, 0);
  }
}
void Tick::state_dead() {
  const Address p = P;
  stop_loop_sound(); auto pvp = u32(0x4f711c);
  write(indexed(0x4d8ab0, std::int64_t(slot()) * 4), 4, UINT32_MAX);
  write(indexed(0x4d8a90, std::int64_t(slot()) * 4), 4, 0xbd4ccccc);
  if (!pvp && dcmp(i2d(std::uint32_t(frames())), mem.load(0x5362a0, 8)) == 0) {
    auto [a,b] = hands_midpoint(P, false); auto z = u32(p + 0x18);
    auto angle = draw(); auto size = draw();
    call(Service::Decal, {Vec3{mul(add(a[0],b[0]),HALF),mul(add(a[1],b[1]),HALF),z},
      std::uint32_t(s16(angle)), 0u, 1u, from_int(size % 10 + 0x19)});
  }
  if (signed16(indexed(0x4f84a8, std::int64_t(slot()) * 2)) > 1) anim_step();
}
void Tick::state_jump() {
  const Address p = P;
  bool go = !lt(u32(top_track(0x114)), 0x4080a3d6) || signed8(p + 0x47d) != 0;
  if (go) {
    if (!signed8(p + 0x47e)) {
      if (mem.load(p + 0xa8, 8) & UINT64_C(0x2000000000000000))
        call(Service::Water, {u32(0x44fdc8), Vec3{sub(u32(p+0x10),ZERO),u32(p+0x14),sub(u32(p+0x18),0x41400000)},0x453b8000u,0x41c00000u});
      call(Service::Voice, {std::uint32_t(slot()), 13u});
      write(p+0x47e,1,1); write(p+0x30,4,0x43b40000); write(p+0x18,4,add(u32(p+0x18),0x3ca3d70a));
    }
    write(p+0x47d,1,1);
  }
  auto rise = u32(p+0x30); if (lt(ZERO,rise)) write(p+0x30,4,sub(rise,0x4157fffe));
  move(0);
}
void Tick::state_rush() {
  write(Address(P)+0x188,4,0x3dcccccc); write(Address(P)+0x18c,4,ONE); write(Address(P)+0x190,4,0);
  anim_step(0x40000000); move(2);
}
void Tick::state_block_move() {
  const Address p = P;
  if (signed8(0x41b51d)) throw Leave("recall during block move");
  if (anim_step() & 5) call(Service::Footstep,{P});
  if (mem.load(p+0xa8,8) & UINT64_C(0x2000000000000000))
    call(Service::Water,{u32(0x44fdc8),Vec3{sub(u32(p+0x10),ZERO),u32(p+0x14),sub(u32(p+0x18),0x41400000)},0x447a0000u,0x41400000u});
  write(p+0x55c,4,0x10);
  if (!(mem.load(p+0x11a,2)&8)) {
    if (!(u32(indexed(pad,8)) & u32(indexed(0x41b9d0,std::int64_t(slot())*4)))) {
      write(p+0x58c,4,2); write(top_track(0x110),4,0xbdaaaaaa); write(top_track(0xf0),4,0xbdaaaaaa); write(p+0x47f,1,0);
    } else { write(p+0x47f,1,1); write(p+0x58c,4,8); }
  }
  move(1);
}
void Tick::state_warcry() {
  const Address p = P; anim_step();
  if (signed32(p+0xf0) == s32(u32(FRAME)-30)) {
    auto block = call(Service::PoolNew,{0xbcu});
    if (block) call(Service::LightEffect,{block,Vec3{add(u32(p+0x10),ZERO),u32(p+0x14),add(u32(p+0x18),0x42100000)},P,0u,
      0x437f0000u,0x431b0000u,ZERO,0xc2c00000u,ZERO,ZERO,ZERO,ZERO});
  }
  if (!(mem.load(p+0x11a,2)&0x20)) {
    write(p+0x58c,4,2); write(top_track(0x10c),4,ONE); write(p+0xf0,4,u32(FRAME)); write(top_track(0x110),4,0xbdaaaaaa);
  }
}
void Tick::state_vehicle() {
  const Address p = P;
  if (is_copy() && (u32(indexed(pad,0xc)) & 0x10)) { write(p+0x58c,4,2); return; }
  anim_step(); auto vehicle=u32(p+0x540);
  vcall(Service::VehicleMove,vehicle,0x48,17,{std::uint32_t(slot()),std::uint32_t(p+0x10),std::uint32_t(p+0x38)});
  call(Service::UpdateGrid,{P});
  if (h_has_anim(P,0x535ee0) || cstr(0x41b721)==cstr(0x535e40)) return;
  auto lump=u32(u32(p+0x24)); auto name=call(Service::ResFind,{lump,0x535ee0u}) ? 0x535ee0u : 0x535808u;
  auto anm=call(Service::ResFind,{u32(u32(p+0x24)),name});
  call(Service::AnimStart,{std::uint32_t(p+0x114),anm,3u,0x3dccccccu});
}
void Tick::state_pound() {
  const Address p=P; auto ev=anim_step();
  if (ev&1) { call(Service::Voice,{std::uint32_t(slot()),10u}); sound(0x535ef8,u32(Address(record())+0x43d8)); }
  if (is_copy()) return;
  if (ev&7) {
    int turn=(ev&1) ? -0x4000 : ((ev&2) ? 0 : int((ev&4)<<12));
    auto angle=std::uint32_t(signed16(p+0x38)+turn);
    call(Service::DamageRadius,{P,Vec3{add(u32(p+0x10),mul(cos16(angle),0x41c00000)),add(u32(p+0x14),mul(sin16(angle),0x41c00000)),add(u32(p+0x18),0x42700000)},1u,0x40000u,1u,0u,ZERO,0x42400000u});
  }
  if (ev&8) { auto block=call(Service::PoolNew,{0x9cu}); if (block) {
    auto rank=h_skill_rank(std::uint32_t(slot()),0x26);
    call(Service::PoundEffect,{block,vec(p+0x10),std::uint32_t(signed16(p+0x38)),u32(0x41b208),std::uint32_t(rank),P});
  }}
  if (!(mem.load(p+0x11a,2)&8)) { write(p+0x58c,4,2); write(top_track(0x10c),4,ONE); write(p+0x448,4,u32(FRAME)); write(top_track(0x110),4,0xbdaaaaaa); }
}
void Tick::state_skill() {
  const Address p=P;
  if (is_copy()) anim_step();
  if (signed32(p+0x58c)==14) return;
  move(5,Vec3{}); if (is_copy()) return;
  auto r=record(); if (!(u32(indexed(pad,0xc)) & u32(Address(r)+0x4a80))) return;
  auto ability=selected_ability().value_or(0); if (signed16(Address(ability)+0x20)==-1) ability=0;
  if (!ability) return;
  if (signed16(Address(ability)+0x20)==0x31) {
    auto type=call(Service::BashType); auto spell=call(Service::FindSpell,{P,type});
    if (spell && vcall(Service::SpellActive,spell,0x14,4)) write(p+0x450,4,1);
  }
  if (signed16(Address(ability)+0x20)==0x22) {
    auto type=call(Service::ShieldBashType); auto spell=call(Service::FindSpell,{P,type});
    if (spell) { if (!vcall(Service::SpellActive,spell,0x14,4)) return; write(p+0x450,4,1); }
  }
}
void Tick::state_block() {
  const Address p=P, input=pad, r0=R0;
  auto row=std::int64_t(signed8(Address(record())+0x4a86))*0x68;
  bool restart=false;
  if (!h_has_anim(P,u32(indexed(u32(p+0x24),row+0xa0)))) restart=!h_has_anim(P,u32(indexed(u32(p+0x24),row+0x80)));
  anim_step();
  if (!h_has_anim(P,u32(indexed(u32(p+0x24),row+0x98)))) call(Service::AnimStart,{std::uint32_t(p+0x114),anim_name(0x9c),0x2801u,0x3dccccccu});
  auto sl=slot(); write(p+0x55c,4,0x10);
  if (!(u32(indexed(input,8)) & u32(indexed(0x41b9d0,std::int64_t(sl)*4)))) {
    if (le(ONE,u32(top_track(0x104))) && signed8(p+0x118)<7) {
      write(indexed(r0,0x4a5c),4,0); write(p+0x47f,1,0); write(p+0x58c,4,2); write(p+0x454,4,0);
      write(top_track(0x110),4,0xbdaaaaaa); write(top_track(0xf0),4,0xbdaaaaaa);
    }
  } else {
    if (!signed8(p+0x47f) || restart) {
      write(p+0x47f,1,1); call(Service::AnimStart,{std::uint32_t(p+0x114),anim_name(0x84),0x803u,0x3dccccccu}); write(p+0x58c,4,8); sl=slot();
    }
    write(indexed(r0,0x4a5c),4,sub(u32(indexed(r0,0x4a5c)),0x3c888888));
  }
  if (s32(std::uint32_t(h_skill_rank(std::uint32_t(sl),0x3b)))>0) {
    auto [a,b]=hands_midpoint(P,false); Vec3 mid; for (unsigned k=0;k<3;++k) mid[k]=mul(add(a[k],b[k]),HALF);
    auto velocity=unit_sphere_shared(); for(auto &x:velocity) x=mul(x,0x41200000);
    if (!u32(0x4ef3f4)) {
      call(Service::ParticleCtor,{0x864eb0u,0xf3u,0x11u,0xf0u,0xeau,0xe5u,0x1bu,0xdcu,0x41b5be76u,0x4119db22u,ZERO}); write(0x4ef3f4,4,1);
    }
    call(Service::ParticleSpawn,{0x864eb0u,mid,velocity,ZERO});
  }
  move(1); h_try_block_move(P,input);
}
// State 2 and its complete ability/attack subpaths. RTA contract only.
void Tick::old_arrow_notices() {
  auto ammo = u32(Address(record()) + 0x4410);
  if (ammo && (std::uint32_t(mem.load(Address(ammo) + 4, 2)) - 1u) < 0x14)
    hud_notice(6, 0xf0);
}
void Tick::state_neutral() {
  const Address p = P, input = pad, r0 = R0;
  write(p + 0x484, 4, 0);
  if (signed8(p + 0x482)) {
    auto sl = slot();
    auto forced = u32(p + 0x488);
    auto rs = rec(sl);
    auto ability = forced;
    if (!forced) {
      auto selected = selected_ability();
      if (!selected) return;
      ability = *selected;
    }
    const Address a = ability;
    bool start = false;
    if (signed8(a + 0x25) == 2) {
      if ((forced || (u32(indexed(input, 0xc)) & u32(rs + 0x4a80))) && signed8(p + 0x118) >= 4) {
        if (!(u32(FLAGS) & 0x10000000)) start = signed16(p + 0x4ae) == 0;
        else if (!signed16(p + 0x4ae)) call(Service::Voice, {std::uint32_t(sl), 6u});
      }
    } else if (((u32(indexed(input, 8)) & u32(rs + 0x4a80)) || u32(p + 0x450) || forced) && lt(ONE, u32(rs + 0xc0))) {
      if (!(u32(FLAGS) & 0x10000000)) start = signed16(p + 0x4ae) == 0;
      else if (!signed16(p + 0x4ae)) call(Service::Voice, {std::uint32_t(sl), 6u});
    }
    if (!signed8(NETWORK) && u32(indexed(0x41b818, std::int64_t(slot()) * 4)) && !u32(p + 0x488)) start = false;
    if (start) {
      if (signed16(a + 0x20) == -1) start = false;
      else {
        auto rn = Address(record());
        if (mem.load(rn + 0x4bc9, 1)) start = false;
        else if (u32(0x4f7128)) {
          start = false;
          auto text = call(Service::TextLookup, {0x535f10u});
          call(Service::HudMessage, {text, 0xb4u, UINT32_MAX, 0u});
        } else if (signed16(a + 0x20) == 0x25) {
          auto cls = signed8(rs + 0x4a86);
          if (cls == 4 || cls == 6) {
            start = false;
            auto text = call(Service::TextLookup, {0x535f20u});
            call(Service::HudMessage, {text, 0xb4u, std::uint32_t(slot()), 0u});
          }
        } else if (eq(u32(a + 0x2c), 0x4479c000) && lt(u32(rn + 0xc0), u32(rn + 0xc4))) start = false;
      }
    }
    write(p + 0x450, 4, 0);
    if (start && signed8(a + 0x24)) {
      std::uint32_t cost;
      if (cstr(a) == cstr(0x535e30)) {
        auto skill = skill_of(ability);
        auto rank = vcall(Service::PlayerRank, P, 0x48, 39, {std::uint32_t(signed16(a + 0x20))});
        cost = vcall(Service::SkillCost, skill, 0x60, 5, {rank});
        if (signed32(Address(skill) + 8) == 1) cost = mul(cost, HALF);
      } else if (signed16(a + 0x20) == 0x25) {
        auto level = s32(std::uint32_t(h_passive_level(std::uint32_t(slot()), 0x20000)));
        cost = add(mul(from_int(std::int64_t(level) - 1), 0x407fffb0), 0x41d00000);
      } else cost = u32(a + 0x2c);
      if (le(cost, u32(Address(record()) + 0xc0)) || eq(cost, 0x4479c000)) {
        if (neutral_start_ability(ability, cost, rs)) return;
      } else call(Service::Voice, {std::uint32_t(slot()), 4u});
    }
  }
  auto sl = slot();
  auto mask = u32(indexed(0x41b980, std::int64_t(sl) * 4));
  if (mem.load(p + 0x480, 1) || u32(p + 0x484)) mask = u32(Address(record()) + 0x4a80);
  if (!(u32(indexed(input, 8)) & u32(Address(record()) + 0x4a80)) && !u32(p + 0x488)) write(p + 0x480, 1, 0);
  bool no_combat = u32(FLAGS) & 0x10000000;
  if (no_combat && ((u32(indexed(input, 8)) & mask) || (u32(indexed(input, 0xc)) & u32(indexed(0x41b9d0, std::int64_t(sl) * 4))) || (u32(p + 0x488) && u32(p + 0x484))))
    call(Service::Voice, {std::uint32_t(sl), 6u});
  bool attack;
  if (!no_combat && instant == UINT32_MAX) attack = true;
  else if (u32(p + 0x488) && u32(p + 0x484)) attack = true;
  else attack = (u32(indexed(input, 8)) & (mask | instant)) != 0;
  if (attack) {
    if (neutral_attack(mask, std::uint32_t(sl))) return;
  } else {
    bool wants = u32(p + 0x454) == 1;
    if (!wants) {
      sl = slot();
      wants = (u32(indexed(input, 8)) & u32(indexed(0x41b9d0, std::int64_t(sl) * 4))) && !(u32(FLAGS) & 0x10000000);
    } else sl = slot();
    if (wants) {
      if (h_can_block(std::uint32_t(sl), u32(p + 0x58c))) write(p + 0x58c, 4, 8);
      else {
        auto text = call(Service::TextLookup, {0x535fe8u});
        call(Service::HudMessage, {text, 0xb4u, std::uint32_t(slot()), 0u});
      }
    }
  }
  h_try_block_move(P, input);
  auto v = add(u32(indexed(r0, 0x4a5c)), 0x3c888888);
  write(indexed(r0, 0x4a5c), 4, v);
  if (le(0x3e800000, v)) write(indexed(r0, 0x4a5c), 4, HALF);
  if (move(0) & 5) call(Service::Footstep, {P});
}
bool Tick::neutral_start_ability(std::uint32_t ability, std::uint32_t cost, Address rs) {
  const Address p = P, a = ability;
  bool cast = false;
  if (cstr(a) == cstr(0x535e30)) {
    auto kind = signed8(a + 0x25);
    if (kind == 7) h_cast_skill(P, ability);
    else if (kind == 8) {
      auto skill = skill_of(ability);
      if (vcall(Service::SkillCanUse, skill, 0x60, 1, {P})) write(p + 0x484, 4, ability);
      else { write(p + 0x484, 4, 0); write(p + 0x488, 4, 0); }
    } else cast = true;
  } else {
    auto sid = signed16(a + 0x20);
    if (sid == 0xe) {
      auto cls = signed8(Address(record()) + 0x4a86);
      std::uint32_t fmt = 0;
      switch (cls) {
        case 0: fmt = 0x535f30; break; case 5: fmt = 0x535f40; break;
        case 2: fmt = 0x535f50; break; case 1: fmt = 0x535f60; break;
        case 3: fmt = 0x535f70; break; case 7: fmt = 0x535f80; break;
      }
      Argument name = StackHandle{};
      if (fmt) name = call(Service::Sprintf, {fmt, NullArgument{}});
      if (!u32(p + 0x374)) {
        auto anm = call(Service::ResFind, {u32(u32(p + 0x24)), name});
        call(Service::AnimBlend, {std::uint32_t(p + 0x114), anm, 0x22u, 0x3dccccccu, 0x3dccccccu});
        write(p + 0x374, 4, 0x30c); write(p + 0x58c, 4, 0xc); write(p + 0xf0, 4, u32(FRAME));
        return true;
      }
    } else if (sid == 0x25) {
      if (!u32(p + 0x44c) && signed8(p + 0x118) < 6) {
        sound(0x535f90, P);
        write(p + 0xb0, 4, 0x40333333); write(p + 0x470, 4, 0x3f5b645a);
        write(p + 0x460, 1, 0); write(p + 0x46c, 4, 0); write(p + 0x448, 4, u32(FRAME));
        auto mana = u32(indexed(rs, 0xc0));
        write(p + 0x44c, 4, 2); write(indexed(rs, 0xc0), 4, sub(mana, cost));
        clear_extra_tracks();
        call(Service::AnimBlend, {std::uint32_t(p + 0x114), anim_name(0x64), 0x812u, 0x3dccccccu, 0x3dccccccu});
        if (!is_copy()) {
          auto block = call(Service::PoolNew, {0xbcu});
          if (block) {
            Vec3 pos{add(u32(p + 0x10), ZERO), u32(p + 0x14), add(u32(p + 0x18), 0x42300000)};
            call(Service::LightEffect, {block, pos, 0u, 0u, 0x437f0000u, 0x42ce0000u, 0x42e60000u, ZERO, ZERO, ZERO, ZERO, ZERO});
          }
        }
        write(p + 0x58c, 4, 1);
      }
    } else if (sid == 0xc) {
      write(p + 0x58c, 4, 10); write(p + 0x448, 4, u32(FRAME));
      auto anm = call(Service::ResFind, {u32(u32(p + 0x24)), 0x535fa8u});
      call(Service::AnimBlend, {std::uint32_t(p + 0x114), anm, 10u, 0x3dccccccu, 0x3dccccccu});
    } else if ((std::uint32_t(sid - 4) & 0xffff) < 3 || sid == 0x13 || sid == 0x12) {
      auto rn = Address(record());
      if (signed8(rn + 0x4a86) != 4 && u32(rn + 0x4438)) write(rn + 0x4a7c, 1, 1);
      rn = record();
      if (signed8(rn + 0x4a86) == 4) {
        if (u32(rn + 0x4410)) {
          write(p + 0x44c, 4, 1);
          write(p + 0x480, 1, sid == 0x13 ? 5 : (sid == 0x12 ? 6 : sid - 3));
        }
        old_arrow_notices();
        if (!is_copy() && !u32(Address(record()) + 0x4410)) {
          call(Service::Voice, {std::uint32_t(slot()), 5u}); hud_notice(3, 0x78);
        }
      }
      write(p + 0x4ae, 2, mem.load(a + 0x22, 2));
    } else cast = true;
  }
  if (cast) {
    auto sl = slot();
    write(p + 0x4ae, 2, mem.load(a + 0x22, 2));
    call(Service::Voice, {std::uint32_t(sl), 0xbu}); write(p + 0x58c, 4, 5); write(p + 0x448, 4, u32(FRAME));
    return true;
  }
  return false;
}
bool Tick::neutral_attack(std::uint32_t mask, std::uint32_t sl_bits) {
  const Address p = P, input = pad, r0 = R0;
  auto sl = s32(sl_bits);
  auto rc = signed32(indexed(r0, 0x50));
  std::int32_t cls = 0;
  if ((rc == 0 || rc == 2 || rc == 3) &&
      ((cls = signed8(Address(record()) + 0x4a86)) == 4 || cls == 6)) {
    auto ammo = u32(indexed(r0, 0x4410));
    bool empty = !ammo || signed16(Address(ammo) + 4) < 1;
    auto roll = mul(from_int(own_draw()), TWO_M16);
    if (le(roll, 0x3dcccccc) || empty) {
      call(Service::FindTarget, {P, 0x42900000u});
      auto node = u32(p + 0x234);
      if (node) {
        vcall(Service::PlayerTurn, P, 0x48, 26, {0u, vec(Address(u32(Address(node) + 0xc)) + 0x10)});
        auto aim = vec(Address(u32(Address(u32(p + 0x234)) + 0xc)) + 0x10);
        if (!h_has_anim(P, 0x535fb8) && !h_has_anim(P, 0x535fc8)) {
          auto here = vec(p + 0x10);
          write(p + 0x448, 4, u32(FRAME));
          Vec3 d{}; for (unsigned k = 0; k < 3; ++k) d[k] = sub(aim[k], here[k]);
          auto dist = sqrt(add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2])));
          if (lt(dist, 0x42700000)) {
            write(p + 0x58c, 4, 1);
            cls = signed8(indexed(r0, 0x4a86));
            if (cls == 4 || cls == 6) {
              auto anm = call(Service::ResFind, {u32(u32(p + 0x24)), cls == 4 ? 0x535fb8u : 0x535fc8u});
              call(Service::AnimBlend, {std::uint32_t(p + 0x114), anm, 0x802u, 0x3dccccccu, 0x3dccccccu});
              if (cls == 4) write(p + 0x470, 4, 0x3f666666);
            }
            return true;
          }
        }
      }
    }
  }
  bool go = true;
  if (u32(0x4f7128)) {
    go = false;
    if (signed8(Address(record()) + 0x4a86) == 6 && (u32(indexed(input, 8)) & mask)) {
      auto weapon = u32(indexed(r0, 0x4434 + 4 * signed8(indexed(r0, 0x4a7c))));
      if (weapon) go = call(Service::PrefixDiffers, {u32(u32(weapon)), 0x535fd8u}) == 0;
    }
    if (!go) {
      auto text = call(Service::TextLookup, {0x535f10u});
      call(Service::HudMessage, {text, 0xb4u, UINT32_MAX, 0u});
    }
  }
  if (go) {
    sl = slot(); auto rs = rec(sl); cls = signed8(rs + 0x4a86);
    if (cls == 6) {
      if ((u32(indexed(input, 8)) & mask) || (u32(p + 0x488) && u32(p + 0x484))) {
        write(p + 0x58c, 4, 0); write(p + 0x480, 1, 7);
      }
    } else if (cls == 4) {
      if (u32(p + 0x44c) || instant == UINT32_MAX || (u32(p + 0x488) && u32(p + 0x484)) || (u32(indexed(input, 8)) & (mask | instant))) {
        write(p + 0x44c, 4, 0);
        if (u32(rs + 0x4410)) {
          if (!u32(p + 0x488) || !u32(p + 0x484)) {
            if (lt(call(Service::ButtonValue, {std::uint32_t(indexed(input, 0)), u32(indexed(0x41b980, std::int64_t(slot()) * 4))}), 0x3e051eb8) &&
                lt(call(Service::ButtonValue, {std::uint32_t(indexed(input, 0)), u32(Address(record()) + 0x4a80)}), 0x3e051eb8)) {
              for (int i = 1; i < signed8(p + 0x119); ++i) write(p + 0x150 + 0x20 * (i - 1), 4, 0xbdeeeeee);
              move(1); anim_step(); return true;
            }
          }
          sl = slot();
          if (u32(indexed(input, 8)) & u32(indexed(0x41b980, std::int64_t(sl) * 4))) write(p + 0x480, 1, 0);
          write(p + 0x58c, 4, 0);
        } else if (!is_copy()) {
          for (unsigned i = 0; i < 64; ++i) {
            auto item = rs + 0x130 + i * 0x48;
            if (u32(item) && signed8(Address(u32(item)) + 0x1c) == 8) write(rs + 0x4410, 4, item);
          }
        }
        if (!is_copy()) {
          old_arrow_notices(); sl = slot();
          if (!u32(rec(sl) + 0x4410)) { call(Service::Voice, {std::uint32_t(sl), 5u}); hud_notice(3, 0x78); }
        }
      }
    } else if (le(mul(u32(p + 0x48c), 0x42700000), from_int(frames()))) {
      if (u32(p + 0x44c) || instant == UINT32_MAX || (u32(indexed(input, 0xc)) & u32(indexed(0x41b980, std::int64_t(sl) * 4))) || instant)
        write(p + 0x58c, 4, 1);
    }
  }
  auto state = u32(p + 0x58c);
  if (state < 2) {
    write(p + 0x448, 4, u32(FRAME)); write(p + 0x470, 4, 0x3fcccccc); write(p + 0x461, 1, 1);
    write(p + 0x44c, 4, 0); write(p + 0x460, 1, 0); write(p + 0x46c, 4, 0);
    clear_extra_tracks();
    call(Service::AnimBlend, {std::uint32_t(p + 0x114), anim_name(0x74), state == 0 ? 0x12u : 0x802u, 0x3dccccccu, 0x3dccccccu});
    return true;
  }
  return false;
}
std::uint32_t Tick::draw3() {
  auto seed = u32(0x404468) * 0x19660du + 0x3c6ef35fu;
  write(0x404468, 4, seed);
  return seed >> 16;
}
std::uint32_t Tick::melee_next_swing(Address rm, std::int32_t steps, std::uint32_t started) {
  const Address p = P;
  auto step = signed8(p + 0x460) + 1;
  step = (step & 128) ? (step & 255) - 256 : (step & 255);
  write(p + 0x460, 1, step);
  if (std::int64_t(steps) - 1 < step) write(p + 0x460, 1, std::int64_t(steps) - 1);
  if (u32(p + 0x44c) != 2) write(p + 0x44c, 4, 1);
  write(p + 0x454, 4, 0);
  call(Service::PickAttackAnim, {P, std::uint32_t(signed8(indexed(rm, 0x4a86)))});
  auto cls = signed8(indexed(rm, 0x4a86));
  std::uint32_t fmt = 0;
  switch (cls) {
    case 0: fmt = 0x536198; break; case 5: fmt = 0x5361b0; break;
    case 2: fmt = 0x5361c8; break; case 1: fmt = 0x5361e0; break;
    case 3: fmt = 0x5361f8; break; case 7: fmt = 0x536208; break;
  }
  Argument name = StackHandle{};
  if (fmt) name = call(Service::Sprintf, {fmt, std::uint32_t(signed8(p + 0x461))});
  else if (cls == 4 || cls == 6)
    name = call(Service::Sprintf, {cls == 4 ? 0x535fb8u : 0x535fc8u, NullArgument{}});
  cls = signed8(indexed(rm, 0x4a86));
  if (cls == 4 || cls == 6) {
    started = u32(p + 0x448);
    auto elapsed = from_int(u32(FRAME) - started);
    if (le(elapsed, 0x421e6665)) {
      if (lt(0x4257ffff, elapsed)) write(p + 0x58c, 4, 2);
      return started;
    }
    call(Service::FindTarget, {P, 0x42700000u});
    if (!u32(p + 0x234)) return u32(p + 0x448);
    if (signed8(indexed(rm, 0x4a86)) == 4) write(p + 0x470, 4, 0x3f666666);
  } else write(p + 0x470, 4, 0x3fcccccc);
  auto anm = call(Service::ResFind, {u32(u32(p + 0x24)), name});
  call(Service::AnimStart, {std::uint32_t(p + 0x114), anm, 0x802u, 0x3dccccccu});
  write(p + 0x448, 4, u32(FRAME));
  return u32(FRAME);
}
void Tick::state_melee() {
  const Address p = P, input = pad;
  auto frame = u32(FRAME);
  auto sl = slot();
  const Address rm = rec(sl);
  auto gap = s32(frame - u32(p + 0x45c));
  write(p + 0x45c, 4, frame);
  if (gap > 0x3c) write(p + 0x458, 4, frame);
  auto cls = signed8(rm + 0x4a86);
  bool heavy = u32(p + 0x44c) == 2;
  int steps = 7;
  if (heavy) {
    for (auto [off, value] : {std::pair<unsigned,unsigned>{0x90,0x8e}, {0x92,0x3a},
         {0x94,0x52}, {0x9e,0x40}, {0x98,0x8e}, {0x9a,0x3a}, {0x9c,0x52}, {0x96,0x40}})
      write(p + off, 2, value);
    steps = 1;
  } else {
    if (cls == 2) steps = 4; else if (cls == 3) steps = 9; else if (cls == 5) steps = 2;
  }
  int sweep_frame = cls == 2 && !heavy ? 0x7f : 0x5e;
  auto weapon = u32(indexed(rm, 0x4434 + 4 * signed8(rm + 0x4a7c)));
  Raw haste = ZERO;
  if (weapon) {
    Raw other = ZERO;
    auto second = u32(rm + 0x43f4);
    if (second) other = haste_of(second);
    haste = haste_of(weapon);
    if (lt(haste, other)) haste = other;
    haste = add(haste, ZERO);
  }
  if (cls == 7) haste = mul(haste, HALF);
  // range() captures the count, but the reference rereads each list entry
  // after callbacks before falling back from ancestral to battle cry.
  Raw best = ZERO;
  const auto alive_count = signed32(ALIVE);
  if (alive_count > 65536) throw ResourceLimit("melee alive-list cap");
  for (int i = 0; i < alive_count; ++i) {
    auto obj = u32(rec(signed32(0x4f8440 + 4 * Address(i))) + 0x43d8);
    auto type = call(Service::AncestralType);
    auto spell = call(Service::FindSpell, {obj, type});
    QueryResult query;
    if (spell) {
      auto result = invoke(Service::AncestralQuery, {spell, P, 0u});
      auto value = std::get_if<QueryResult>(&result);
      if (!value) throw Error("ancestral query must return a pair");
      query = *value;
    } else {
      obj = u32(rec(signed32(0x4f8440 + 4 * Address(i))) + 0x43d8);
      type = call(Service::BattleCryType);
      spell = call(Service::FindSpell, {obj, type});
      if (!spell) continue;
      auto result = invoke(Service::BattleCryAffects, {spell, P});
      auto value = std::get_if<QueryResult>(&result);
      if (!value) throw Error("battle-cry query must return a pair");
      query = *value;
    }
    if (query[0] && lt(best, query[1])) best = query[1];
  }
  Raw speed = ONE;
  if (!eq(best, ZERO)) speed = mul(speed, best);
  if (u32(p + 0x374)) haste = add(haste, 0x3e800000);
  if (u32(p + 0x37c)) haste = add(haste, 0x3dcccccc);
  if (signed32(rm + 0x50) == 5) {
    auto rank = s32(std::uint32_t(h_skill_rank(std::uint32_t(slot()), 0x47)));
    if (rank > 0) {
      auto factor = call(Service::BerserkerFactor, {u32(0x41b170 + 4 * 0x47),
        std::uint32_t(rank), std::uint32_t(rm), frame - u32(p + 0x458)});
      speed = mul(speed, factor);
    }
  }
  speed = add(speed, haste);
  anim_step(speed);
  speed = slow_scale(speed);
  if (cls == 2 && !heavy) sweep_frame = to_int(div(from_int(sweep_frame), speed));
  // The target list is a callback result; handles and virtual calls retain
  // their supplied-memory side effects and this-adjustments.
  auto started = u32(p + 0x448);
  bool turn = false;
  if (s32(frame - started) == 1 || !u32(p + 0x234)) {
    Raw reach = 0x42400000;
    auto fx = cos16(std::uint32_t(signed16(p + 0x38)));
    auto fy = sin16(std::uint32_t(signed16(p + 0x38)));
    auto kind = signed8(rm + 0x4a86);
    if (kind == 1 || kind == 3) reach = 0x42680000;
    auto pos = vec(p + 0x10);
    Vec3 high{add(pos[0], reach), add(pos[1], reach), add(pos[2], 0x42900000)};
    Vec3 low{sub(pos[0], reach), sub(pos[1], reach), sub(pos[2], 0x42900000)};
    auto result = invoke(Service::CollectBox, {low, high, 0x100u});
    auto objects = std::get_if<std::vector<std::uint32_t>>(&result);
    if (!objects) throw Error("box query must return an object list");
    if (objects->size() > 4096) throw ResourceLimit("melee box query cap");
    Raw nearest = 0x551184e7;
    call(Service::HandleClear, {std::uint32_t(p + 0x4b8)});
    std::optional<std::uint32_t> best_obj;
    for (auto obj32 : *objects) {
      const Address obj = obj32;
      if (obj == p) continue;
      auto flags = u32(obj + 0xc);
      bool ok = !(flags & 0x49) && (!(flags & 0x20) ||
        (signed16(obj + 0x312) == 0 && !(mem.load(obj + 0xa8, 8) & 0x4000000))) && !(flags & 0x1000000);
      if (!ok) ok = u32(0x4f711c) && (flags & 0x20) && vcall(Service::PlayerEnemy, P, 0x48, 20, {obj32});
      if (!ok) continue;
      auto dx = sub(u32(obj + 0x10), u32(p + 0x10));
      auto dy = sub(u32(obj + 0x14), u32(p + 0x14));
      auto dist = sqrt(add(mul(dx, dx), mul(dy, dy)));
      auto inv = div(ONE, dist);
      auto nx = mul(dx, inv), ny = mul(dy, inv);
      auto dot = add(mul(nx, fx), mul(ny, fy));
      if (!le(ZERO, dot)) continue;
      if (signed8(rm + 0x4a86) == 2 && signed8(p + 0x460) == 3)
        call(Service::HandleLink, {std::uint32_t(p + 0x4b8), obj32, obj32});
      dot = add(add(mul(nx, fx), mul(ny, fy)), mul(ZERO, ZERO));
      if (le(HALF, dot)) {
        auto d = sub(dist, u32(obj + 0x1c));
        if (lt(d, reach)) {
          d = sub(d, mul(dot, 0x42000000));
          if (le(d, nearest)) { best_obj = obj32; nearest = d; }
        }
      }
    }
    call(Service::HandleClear, {std::uint32_t(p + 0x234)});
    bool linked = true;
    if (best_obj) {
      call(Service::HandleLink, {std::uint32_t(p + 0x234), *best_obj, *best_obj});
      auto node = u32(p + 0x234);
      if (node) {
        auto target = u32(Address(node) + 0xc);
        if (u32(Address(target) + 0xc) & 0x20)
          vcall(Service::CreatureReact, target, 0x48, 12, {3u, vec(p + 0x10), P, ZERO});
      } else linked = false;
    }
    if (linked && u32(p + 0x234)) { started = u32(p + 0x448); turn = true; }
  } else turn = true;
  if (turn && le(from_int(frame - started), 0x41ed1eb7)) {
    auto node = u32(p + 0x234);
    vcall(Service::PlayerTurn, P, 0x48, 26, {0u, vec(Address(u32(Address(node) + 0xc)) + 0x10)});
  }
  started = u32(p + 0x448);
  auto lunge = u32(p + 0x44c) == 2 ? 0x3f1126e9u : 0x3eddb22du;
  move(le(from_int(frame - started), mul(lunge, 0x42700000)) ? 4 : 3);
  auto t = s32(u32(FRAME) - u32(p + 0x448));
  bool hit = false, swing = false, shout = false;
  std::uint32_t hit_flag = 0;
  if (signed8(NETWORK) == 0) {
    hit = t == to_int(div(0x418028f5, speed));
    if (hit) hit_flag = 1;
    swing = t == 6; shout = t == 9;
  } else {
    if (t == 1) hit = u32(p + 0x55c) == 0;
    if (!call(Service::LocalSlot, {std::uint32_t(slot())}) || is_copy()) { swing = t == 6; shout = t == 9; }
    if (signed32(p + 0x55c) > 0) write(p + 0x55c, 4, signed32(p + 0x55c) - 1);
  }
  if (signed8(rm + 0x4a86) == 2 && sweep_frame <= t) {
    auto past = std::int64_t(t) - sweep_frame;
    auto node = call(Service::IterBegin, {std::uint32_t(p + 0x4b8), 0u});
    unsigned visits = 0;
    while (node) {
      if (++visits > 65536) throw ResourceLimit("melee sweep iterator cap");
      auto target = u32(Address(node) + 0xc);
      auto angle = atan2_16(sub(u32(Address(target) + 0x10), u32(p + 0x10)),
                            sub(u32(Address(target) + 0x14), u32(p + 0x14)));
      if (s16(0u - std::uint32_t(mem.load(p + 0x38, 2)) - std::uint32_t(angle) + 0x4000u) <
          s16(std::uint32_t(past * -0x708 + 0x4000))) {
        call(Service::MeleeHit, {P, u32(Address(node) + 0xc), 0u, 0u, ZERO});
        call(Service::RemoveNode, {node});
      }
      node = call(Service::IterNext);
    }
    call(Service::IterEnd, {2u});
    write(p + 0x44c, 4, 0);
  } else if (hit) {
    if (u32(p + 0x234)) call(Service::MeleeHit, {P, u32(Address(u32(p + 0x234)) + 0xc), hit_flag, 0u, ZERO});
    write(p + 0x44c, 4, 0);
  }
  if (swing) {
    std::uint32_t fmt;
    if (!weapon) fmt = 0x536120;
    else if (u32(Address(u32(weapon)) + 0x20) & 8)
      fmt = mem.load(Address(weapon) + 8, 8) & 0x20000000 ? 0x536138 : 0x536150;
    else fmt = mem.load(Address(weapon) + 8, 8) & 0x20000000 ? 0x536168 : 0x536180;
    auto roll = draw3();
    auto name = call(Service::Sprintf, {fmt, (roll & 3) + 1});
    sound(name, P);
  }
  if (shout) {
    auto kind = signed8(rm + 0x4a86);
    int voice = kind == 5 ? 0x10 : kind == 0 ? 0x12 : kind == 2 ? 0xe : kind == 1 ? 0xf : kind == 3 ? 0x11 : -1;
    if (voice >= 0) call(Service::Voice, {std::uint32_t(slot()), std::uint32_t(voice)});
  }
  cls = signed8(rm + 0x4a86);
  auto trim = mul(mul(haste, 0x42700000), HALF);
  if (lt(0x418b3332, trim)) trim = 0x418b3332;
  Raw window = ZERO;
  if (cls >= 0 && cls <= 6) window = sub(0x41b028f5, trim);
  else if (cls == 7) window = sub(0x418ced91, trim);
  sl = slot();
  auto pressed = u32(indexed(input, 0xc));
  if (!(pressed & (u32(indexed(0x41b980, std::int64_t(sl) * 4)) | instant)) && instant != M32) {
    if (u32(p + 0x454) == 1 || ((pressed & u32(indexed(0x41b9d0, std::int64_t(sl) * 4))) && !(u32(FLAGS) & 0x10000000))) {
      if (h_can_block(std::uint32_t(sl), u32(p + 0x58c))) write(p + 0x58c, 4, 8);
      else {
        auto text = call(Service::TextLookup, {0x535fe8u});
        call(Service::HudMessage, {text, 0xb4u, M32, 0u});
      }
    }
    started = u32(p + 0x448);
  } else {
    if ((pressed & instant) || instant == M32) {
      Address ability = 0;
      if (instant == M32) { ability = u32(p + 0x488); write(p + 0x488, 4, 0); }
      else {
        auto rs = rec(sl);
        if (instant & u32(indexed(0x41b990, std::int64_t(sl) * 4))) ability = indexed(rs, 0x4440 + signed8(rs + 0x4a84) * 0x68);
        if (instant & u32(indexed(0x41b9a0, std::int64_t(sl) * 4))) ability = indexed(rs, 0x4440 + signed8(rs + 0x4a85) * 0x68);
      }
      if (cstr(ability) == cstr(0x535e30)) h_cast_skill(P, std::uint32_t(ability));
    }
    started = u32(p + 0x448);
    if (lt(div(window, speed), from_int(u32(FRAME) - started))) started = melee_next_swing(rm, steps, started);
  }
  frame = u32(FRAME);
  if (le(div(window, speed), from_int(frame - started))) {
    auto ability = selected_ability();
    if (ability && signed16(Address(*ability) + 0x20) == 0x25 && (u32(indexed(input, 0xc)) & u32(Address(record()) + 0x4a80)))
      write(p + 0x450, 4, 1);
  }
  auto lx = u32(indexed(input, 0x14)), ly = u32(indexed(input, 0x18));
  auto stick = sqrt(add(mul(lx, lx), mul(ly, ly)));
  auto elapsed = from_int(frame - started);
  bool end = le(stick, 0x3f4ccccc) || lt(elapsed, 0x421d3332) ?
    le(mul(u32(p + 0x470), 0x42700000), elapsed) || u32(p + 0x450) : true;
  if (end) {
    write(p + 0x44c, 4, 0); write(p + 0xb0, 4, 0x3fb33333);
    if (signed8(p + 0x118) - 1 > 3) {
      int i = 4; Address off = 0x1a0;
      do {
        write(p + 0xc + off, 4, ONE); ++i;
        write(p + 0x10 + off, 4, 0xbdaaaaaa); off += 0x20;
      } while (i <= signed8(p + 0x118) - 1);
    }
    write(p + 0x58c, 4, 2); write(p + 0x448, 4, u32(FRAME));
  }
  h_try_block_move(P, input);
}
std::uint32_t Tick::obj_rand(std::optional<std::uint32_t> object) {
  auto address = indexed(object.value_or(P), 0x44);
  auto seed = u32(address) * 0x19660du + 0x3c6ef35fu;
  write(address, 4, seed);
  return seed >> 16;
}
std::uint32_t Tick::strstr(Address haystack, Address needle) const {
  return cstr(haystack).find(cstr(needle)) != std::string::npos;
}
Vec3 Tick::aim_at_target(std::optional<std::uint32_t> divisor,
                         std::optional<std::uint32_t> /*scale*/) {
  const Address p = P;
  Vec3 aim{};
  call(Service::FindTarget, {P, 0x43960000u});
  auto node = u32(p + 0x234);
  if (node) {
    vcall(Service::PlayerTurn, P, 0x48, 26,
          {0u, vec(Address(u32(Address(node) + 0xc)) + 0x10)});
    auto target = u32(Address(u32(p + 0x234)) + 0xc);
    aim = vec(Address(target) + 0x10);
    if (h_passive_level(std::uint32_t(slot()), 0x200000)) {
      target = u32(Address(u32(p + 0x234)) + 0xc);
      auto dy = sub(u32(Address(target) + 0x14), u32(p + 0x14));
      auto dx = sub(u32(Address(target) + 0x10), u32(p + 0x10));
      auto distance = sqrt(add(mul(dx, dx), mul(dy, dy)));
      auto k = divisor ? div(distance, *divisor) : mul(mul(distance, 0x3e800000), 0x42c80000);
      aim[0] = add(aim[0], mul(u32(Address(target) + 0x308), k));
      aim[1] = add(aim[1], mul(u32(Address(target) + 0x30c), k));
    }
  }
  return aim;
}
void Tick::state_ranged() {
  const Address p = P;
  auto base = mem.load(p + 0x480, 1) == 7 ? 11 : 22;
  auto weapon = u32(indexed(R0, 0x4434 + 4 * signed8(indexed(R0, 0x4a7c))));
  if (signed8(NETWORK)) base >>= 1;
  auto speed = ZERO;
  if (weapon) { auto h = haste_of(weapon); speed = add(speed, add(h, h)); }
  speed = add(ONE, speed);
  anim_step(speed);
  speed = slow_scale(speed);
  auto release = to_int(div(from_int(base), speed));
  std::int32_t sl;
  if (mem.load(p + 0x480, 1) == 7 && weapon) {
    ranged_throw(weapon, release); sl = slot();
  } else {
    sl = slot();
    if (s32(u32(FRAME) - u32(p + 0x448)) == release && u32(rec(sl) + 0x4410)) {
      ranged_shoot(); sl = slot();
    }
  }
  call(Service::RecalcLoad, {std::uint32_t(sl)});
  if ((u32(indexed(pad, 0xc)) & u32(indexed(0x41b980, std::int64_t(slot()) * 4))) && frames() > 14)
    write(p + 0x44c, 4, 1);
  move(signed8(NETWORK) == 0 ? 1 : 6);
  if (!(mem.load(p + 0x11a, 2) & 0x10)) {
    write(p + 0x3cc, 1, 0);
    if (le(mul(u32(p + 0x48c), 0x42700000), from_int(frames()))) write(p + 0x58c, 4, 2);
  }
}
void Tick::ranged_throw(std::uint32_t weapon, std::int32_t release) {
  const Address p = P, w = weapon;
  auto velocity = mem.load(w + 8, 8) & 0x1000000 ? 0x43af0000u : 0x443b8000u;
  auto aim = aim_at_target(velocity);
  if (s32(u32(FRAME) - u32(p + 0x448)) != release || is_copy()) return;
  if (u32(p + 0x484)) {
    h_cast_skill(P, u32(p + 0x484)); write(p + 0x4ae, 2, 0); write(p + 0x3cc, 1, 1); return;
  }
  write(p + 0x3cc, 1, 1);
  auto attached = invoke(Service::AttachPoint,
    {u32(p + 0x544), vec(p + 0x10), std::uint32_t(signed16(p + 0x38)), std::uint32_t(p + 0x114), 0u, 1u, ONE});
  auto pair = std::get_if<AttachResult>(&attached);
  if (!pair) throw Error("throw attach callback must return position and direction");
  auto hand = (*pair)[0];
  auto angle = std::int64_t(mem.load(p + 0x38, 2)) + draw() % 0xa28 - 0x514;
  if (u32(p + 0x234)) angle = 0x4000 - atan2_16(sub(aim[0], hand[0]), sub(aim[1], hand[1]));
  angle = s16(std::uint32_t(angle));
  auto vx = mul(cos16(std::int32_t(angle)), velocity), vy = mul(sin16(std::int32_t(angle)), velocity);
  auto vz = add(from_int(draw() % 0x28), 0x42200000);
  if (u32(p + 0x234)) {
    Vec3 d{}; for (unsigned k = 0; k < 3; ++k) d[k] = sub(u32(p + 0x10 + 4 * k), aim[k]);
    vz = sub(sqrt(add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2]))), 0x42f00000);
  }
  auto item = u32(Address(record()) + 0x43f8);
  auto model_a = u32(indexed(0x3e9ce8, std::int64_t(call(Service::ResourceIndex, {item})) * 16));
  auto model_b = u32(indexed(0x3e9cec, std::int64_t(call(Service::ResourceIndex, {item})) * 16));
  auto name = call(Service::Sprintf, {0x535ff8u, (obj_rand() & 3) + 1});
  sound(name, P);
  auto flags = mem.load(w + 8, 8);
  bool make = true; std::int32_t turn = 0x4000;
  if (flags & 0x1000000) {
    turn = -0x4000;
    auto name_ptr = u32(u32(w));
    if (!strstr(0x536010, name_ptr) && !strstr(0x536020, name_ptr))
      make = call(Service::PrefixDiffers, {name_ptr, 0x535fd8u}) == 0;
  }
  if (make) {
    auto block = call(Service::PoolNew, {0x1d0u});
    if (block) call(Service::ProjectileCtor, {block, model_a, model_b, hand, Vec3{vx, vy, vz}, std::uint32_t(angle + turn), P, 0u});
  }
  auto count = std::uint32_t(mem.load(Address(item) + 4, 2));
  if (count - 1 < 0x14) { hud_notice(7, 0xf0); count = std::uint32_t(mem.load(Address(item) + 4, 2)); }
  auto left = s16(count);
  if (!is_copy()) { write(Address(item) + 4, 2, count - 1); left = s16(count - 1); }
  if (left < 1) {
    auto sl = slot();
    for (unsigned i = 0; i < 64; ++i) {
      auto r = rec(sl);
      if (r + 0x130 + i * 0x48 == u32(r + 0x43f8)) {
        call(Service::RemoveItem, {std::uint32_t(sl), i, 0u, 0u}); sl = slot();
      }
    }
  }
}
void Tick::ranged_shoot() {
  const Address p = P;
  auto attached = invoke(Service::AttachPoint,
    {u32(p + 0x544), vec(p + 0x10), std::uint32_t(signed16(p + 0x38)), std::uint32_t(p + 0x114), 0u, 0u, ONE});
  auto point = std::get_if<Vec3>(&attached);
  if (!point) throw Error("bow attach callback must return a position");
  auto spawn = *point;
  aim_at_target(std::nullopt);
  auto spread = std::int64_t(signed16(p + 0x38)) + draw() % 500 - 0xfa;
  auto collected = invoke(Service::CollectArc,
    {vec(p + 0x10), std::uint32_t(signed16(p + 0x38)), 0u, 0x43aa0000u, 0x41a00000u, ZERO});
  auto objects = std::get_if<std::vector<std::uint32_t>>(&collected);
  if (!objects) throw Error("bow arc callback must return an object list");
  if (objects->size() > 4096) throw ResourceLimit("bow arc object list cap");
  std::uint32_t target = 0;
  for (auto obj : *objects) if (vcall(Service::PlayerEnemy, P, 0x48, 20, {obj})) { target = obj; break; }
  std::int64_t aim;
  if (!target) aim = std::int64_t(mem.load(p + 0x38, 2)) + draw() % 500 - 0xfa;
  else {
    auto bearing = atan2_16(sub(u32(Address(target) + 0x10), u32(p + 0x10)), sub(u32(Address(target) + 0x14), u32(p + 0x14)));
    aim = std::int64_t(draw() % 500) - bearing + 0x40c8;
  }
  auto angle = s16(std::uint32_t(aim));
  auto c = cos16(angle), s = sin16(angle);
  Vec3 vel{mul(mul(c, 0x41200000), 0x42c80000), mul(mul(s, 0x41200000), 0x42c80000), from_int(std::int32_t(draw() % 0x3c) - 0x1e)};
  if (target) {
    const Address t = target;
    Vec3 d{sub(u32(t + 0x10), spawn[0]), sub(u32(t + 0x14), spawn[1]), sub(add(u32(t + 0x18), mul(u32(t + 0x20), HALF)), spawn[2])};
    auto inv = div(ONE, sqrt(add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2]))));
    auto nz = mul(d[2], inv);
    c = cos16(angle); s = sin16(angle);
    auto vz = add(mul(nz, 0x447a0000), from_int(std::int32_t(draw() % 0x3c) - 0x1e));
    if (lt(vz, 0xc3960000)) vz = 0xc3960000;
    else if (lt(0x43960000, vz)) vz = 0x43960000;
    vel = {mul(c, 0x447a0000), mul(s, 0x447a0000), vz};
  }
  if (!is_copy() && target && (u32(Address(target) + 0xc) & 0x20))
    vcall(Service::CreatureReact, target, 0x48, 12, {2u, vec(p + 0x10), P, ZERO});
  auto ability = u32(p + 0x488);
  if (!ability) ability = selected_ability().value_or(0);
  auto sl = slot();
  if (ability && signed16(Address(ability) + 0x20) == -1) write(p + 0x484, 4, 0);
  std::int64_t left = signed16(Address(u32(rec(sl) + 0x4410)) + 4);
  if (!u32(p + 0x484) || signed16(Address(ability) + 0x20) != 0x2e) {
    if (!is_copy()) --left;
  } else {
    auto rank = h_skill_rank(std::uint32_t(sl), 0x2e);
    auto used = s32(vcall(Service::SkillStat, u32(0x41b170 + 4 * 0x2e), 0x60, 10, {2u, std::uint32_t(rank)}));
    if (!is_copy()) left -= used;
    sl = slot();
  }
  if (left < 0) left = 0;
  auto rs = rec(sl);
  auto bow = u32(indexed(rs, 0x4434 + 4 * signed8(rs + 0x4a7c)));
  if (bow) {
    call(Service::ModifiedField, {bow}); call(Service::ScriptRun, {0x536030u});
    auto name = call(Service::Sprintf, {0x536048u, draw() % 5 + 1}); sound(name, P);
    if (!u32(p + 0x484)) {
      auto kind = signed8(p + 0x480);
      if (kind != 3 && !is_copy()) { missile(spawn, vel, spread - 0x4000); kind = signed8(p + 0x480); }
      if (kind == 3) {
        call(Service::DamageRange, {bow, 0u});
        auto count = signed16(Address(u32(Address(record()) + 0x4410)) + 4);
        if (count >= 1 && !is_copy()) missile(spawn, vel, spread - 0x4000);
        write(rs + 0xc0, 4, sub(u32(rs + 0xc0), u32(Address(ability) + 0x2c)));
      } else if ((std::uint32_t(kind - 1) & 0xff) < 2 || kind == 5 || kind == 6)
        write(rs + 0xc0, 4, sub(u32(rs + 0xc0), u32(Address(ability) + 0x2c)));
      write(p + 0x480, 1, 0);
    } else { h_cast_skill(P, u32(p + 0x484)); write(p + 0x4ae, 2, 0); }
    if (!is_copy()) {
      if (!left) {
        sl = slot();
        for (unsigned i = 0; i < 64; ++i) {
          auto r = rec(sl);
          if (r + 0x130 + i * 0x48 == u32(r + 0x4410)) {
            call(Service::RemoveItem, {std::uint32_t(sl), i, 0u, 0u}); sl = slot(); break;
          }
        }
        write(rec(sl) + 0x4410, 4, 0);
      } else write(Address(u32(Address(record()) + 0x4410)) + 4, 2, std::uint64_t(left));
    }
  }
  call(Service::HandleClear, {StackHandle{}});
}
void Tick::missile(const Vec3 &spawn, const Vec3 &velocity, std::int64_t facing) {
  auto block = call(Service::PoolNew, {0x1c8u});
  if (block) call(Service::MissileCtor, {block, std::uint32_t(mem.load(Address(P) + 0x480, 1)), u32(0x3e9ce8), u32(0x3e9cec), spawn, velocity, std::uint32_t(facing), P});
}
std::uint32_t Tick::strcmp(Address a, Address b) const {
  return cstr(a) == cstr(b) ? 0u : 1u;
}
void Tick::dispatch_state() {
  switch (u32(Address(P) + 0x58c)) {
  case 0: state_ranged(); break;
  case 1: state_melee(); break;
  case 2: state_neutral(); break;
  case 3: state_hit(); break;
  case 4: state_dead(); break;
  case 5: state_cast(); break;
  case 6: case 14: state_skill(); break;
  case 7: state_jump(); break;
  case 8: state_block(); break;
  case 9: state_rush(); break;
  case 10: state_pound(); break;
  case 11: state_block_move(); break;
  case 12: state_warcry(); break;
  case 13: state_vehicle(); break;
  default: break;
  }
}
void Tick::state_cast() {
  const Address p = P;
  auto ability = u32(p + 0x488);
  if (!ability) ability = selected_ability().value_or(0);
  if (ability && signed16(Address(ability) + 0x20) == -1) ability = 0;
  anim_step();
  move(signed8(NETWORK) == 0 ? 1 : 6);
  if (!ability || strcmp(ability, 0x535e30)) { cast_old_style(ability); return; }
  auto skill = skill_of(ability);
  auto phase = mem.load(p + 0x47b, 1);
  if (phase == 0) {
    if (!vcall(Service::SkillCanUse, skill, 0x60, 1, {P})) {
      write(p + 0x488, 4, 0); write(p + 0x58c, 4, 2); return;
    }
    phase = mem.load(p + 0x47b, 1);
  }
  if (phase < 2) {
    auto track = s32(call(Service::FindTrack, {std::uint32_t(p + 0x114), 0x8000u}));
    auto point = signed8(NETWORK) == 0 ? ONE : HALF;
    if (signed8(p + 0x47b) == 0 && track < 0) {
      if (signed32(Address(skill) + 8) != 6)
        call(Service::AnimStart, {std::uint32_t(p + 0x114), anim_name(0x6c), 0x8003u, 0x3dccccccu});
      write(p + 0x47b, 1, 1);
    } else if (signed8(p + 0x47b) == 1) {
      if (signed32(Address(skill) + 8) == 6 || le(point, u32(indexed(p, 0x124 + std::int64_t(track) * 0x20)))) {
        write(p + 0x47b, 1, 2);
        h_cast_skill(P, ability);
        write(p + 0x488, 4, 0);
        if (signed32(Address(skill) + 8) == 0) {
          auto weight = indexed(p, 0x12c + std::int64_t(track) * 0x20);
          write(weight, 4, fmin(u32(weight), ONE));
          write(indexed(p, 0x130 + std::int64_t(track) * 0x20), 4, 0xbd6eeeee);
        }
      }
    }
  } else {
    if (signed8(Address(ability) + 0x25) == 1) {
      auto rank = vcall(Service::PlayerRank, P, 0x48, 39, {std::uint32_t(signed16(Address(ability) + 0x20))});
      auto cost = vcall(Service::SkillCost, skill, 0x60, 5, {rank});
      auto r = record();
      write(Address(r) + 0xc0, 4, sub(u32(Address(r) + 0xc0), mul(cost, 0x3c888888)));
    }
    if (!(mem.load(p + 0x11a, 2) & 0x8000)) {
      write(p + 0x47b, 1, 0); write(p + 0x58c, 4, 2); write(p + 0x488, 4, 0);
    }
  }
}
void Tick::light_effect(std::uint32_t height, std::uint32_t owner,
                        const std::vector<std::uint32_t> &colours) {
  if (colours.size() > 64) throw ResourceLimit("light colour argument cap");
  auto block = call(Service::PoolNew, {0xbcu});
  if (!block) return;
  const Address p = P;
  Vec3 position{add(u32(p + 0x10), ZERO), u32(p + 0x14), add(u32(p + 0x18), height)};
  std::vector<Argument> args{block, position, owner, 0u};
  for (auto colour : colours) args.emplace_back(colour);
  for (auto i = colours.size(); i < 8; ++i) args.emplace_back(ZERO);
  call(Service::LightEffect, std::move(args));
}
void Tick::cast_old_style(std::uint32_t ability) {
  const Address p = P, a = ability;
  auto [left, right] = hands_midpoint(P, false);
  Vec3 mid;
  for (unsigned k = 0; k < 3; ++k) mid[k] = mul(add(left[k], right[k]), HALF);
  auto aura = [&](Address base, unsigned stride = 4, unsigned offset = 0) {
    return indexed(base, std::int64_t(slot()) * stride + offset);
  };
  auto colours = [&] {
    for (unsigned k = 0; k < 3; ++k) write(aura(0x4d8a50, 16, 4*k), 4, std::uint32_t(signed16(a + 0x30 + 2*k)));
    for (unsigned k = 0; k < 4; ++k) write(aura(0x8659a0, 16, 4*k), 4, std::uint32_t(signed16(a + 0x38 + 2*k)));
  };
  const std::vector<std::uint32_t> light{0x437f0000, 0x42c80000, 0x437f0000, 0xc2c00000};
  if (signed8(p + 0x47b) == 0) write(aura(0x4d8ab0), 4, 1);
  if (!ability) return;
  auto kind = signed8(a + 0x25);
  if (kind == 1) {
    if (signed8(p + 0x47b) == 0) {
      if (signed16(a + 0x20) == 9) {
        const std::uint32_t color[] = {100, 200, 255}, lc[] = {0, 0x136, 0x1fe, 0xffffffa0};
        for (unsigned k = 0; k < 3; ++k) write(aura(0x4d8a50, 16, 4*k), 4, color[k]);
        for (unsigned k = 0; k < 4; ++k) write(aura(0x8659a0, 16, 4*k), 4, lc[k]);
      }
      auto sl = slot();
      write(indexed(0x4d8a90, std::int64_t(sl)*4), 4, 0x3d4ccccc);
      write(indexed(0x4d8aa0, std::int64_t(sl)*4), 4, 0);
      call(Service::AnimStart, {std::uint32_t(p + 0x114), anim_name(0x6c), 0x8003u, 0x3dccccccu});
    }
    write(p + 0x47b, 1, 1);
    if (!(u32(indexed(pad, 8)) & u32(Address(record()) + 0x4a80))) {
      stop_loop_sound(); write(aura(0x4d8ab0), 4, UINT32_MAX); write(p + 0x58c, 4, 2);
      write(aura(0x4d8a90), 4, 0xbd4ccccc); write(top_track(0x110), 4, 0xbd6eeeee); write(p + 0x47b, 1, 0);
    }
  } else if (kind == 2) {
    if (signed8(p + 0x47b) == 0) {
      colours(); write(aura(0x4d8aa0), 4, 0); write(aura(0x4d8a90), 4, u32(a + 0x40));
      sound(0x536220, u32(Address(record()) + 0x43d8));
      call(Service::AnimStart, {std::uint32_t(p + 0x114), anim_name(0x6c), 0x8003u, 0x3dccccccu});
      write(p + 0x47b, 1, 1);
    }
    if (le(0x41200000, u32(top_track(0x104)))) {
      call(Service::SoundStop, {u32(p + 0x4a8)}); write(p + 0x4a8, 4, UINT32_MAX);
      write(aura(0x4d8ab0), 4, UINT32_MAX); write(p + 0x58c, 4, 2); write(p + 0x488, 4, 0);
      write(aura(0x4d8a90), 4, 0xbd4ccccc); write(top_track(0x110), 4, 0xbd6eeeee); write(p + 0x47b, 1, 0);
    }
  } else if (kind == 0) {
    auto sid = signed16(a + 0x20), phase = signed8(p + 0x47b);
    if (phase == 0) {
      if (sid == 0x1a) {
        write(p + 0xf0, 4, u32(p + 0x448));
        write(Address(u32(Address(record()) + 0x43d8)) + 0x398, 4, 0x29);
      } else if (sid == 0x1b) {
        write(p + 0xf0, 4, u32(p + 0x448)); light_effect(0x42100000, 0, light);
        write(Address(u32(Address(record()) + 0x43d8)) + 0x3a0, 4, std::uint32_t(signed16(a + 0x22)));
      }
      write(aura(0x4d8a90), 4, u32(a + 0x40)); colours(); write(p + 0x47b, 1, 1);
      call(Service::AnimBlend, {std::uint32_t(p + 0x114), anim_name(0x6c), 0x8022u, 0x3dccccccu, 0x3dccccccu});
    } else if (phase != 2 && le(ONE, u32(top_track(0x104)))) {
      write(p + 0x47b, 1, 2);
      if (sid == 0x19) {
        auto block = call(Service::PoolNew, {0xc0u});
        if (block) call(Service::RootCtor, {block, mid, std::uint32_t(signed16(p + 0x38)), P, 0u, 0x41f00000u});
      } else if (sid == 0x15) old_fire_nova(ability);
      else if (sid == 0x1a) {
        auto owner = u32(Address(record()) + 0x43d8);
        if (signed32(Address(owner) + 0x39c) < 1) {
          light_effect(0x42100000, 0, light);
          auto block = call(Service::PoolNew, {0xa0u});
          if (block) call(Service::UnholyAuraCtor, {block, vec(p + 0x10), P, 0u, 0x424a0000u});
        } else write(Address(u32(Address(record()) + 0x43d8)) + 0x39c, 4, 0);
      }
    }
    if (!(mem.load(p + 0x11a, 2) & 0x20)) {
      stop_loop_sound(); write(p + 0x488, 4, 0); write(p + 0x58c, 4, 2);
      write(aura(0x4d8ab0), 4, UINT32_MAX); write(p + 0x47b, 1, 0);
    }
  }
}
void Tick::old_fire_nova(std::uint32_t ability) {
  const Address p = P;
  if (!u32(0x4ef3f8)) {
    call(Service::ParticleCtor, {0x864f00u, 0x6eu, 0x6bu, 0x9au, 0x9fu, 0x6fu, 0x6bu, 0xa1u, 0x4222978du, 0x42a19604u, ZERO});
    write(0x4ef3f8, 4, 1);
  }
  for (unsigned angle = 0; angle < 0x10000; angle += 300) {
    auto vx = mul(cos16(angle), 0x42c80000), vy = mul(sin16(angle), 0x42c80000);
    Vec3 pos{add(u32(p + 0x10), ZERO), u32(p + 0x14), add(u32(p + 0x18), 0x42100000)};
    call(Service::ParticleSpawn, {0x864f00u, pos, Vec3{vx, vy, 0}, ZERO});
  }
  sound(0x536238, u32(Address(record()) + 0x43d8));
  light_effect(0x42100000, 0, {ZERO, 0x431b0000, 0x437f0000, 0xc2c00000});
  auto level = s32(std::uint32_t(h_passive_level(std::uint32_t(slot()), 0x8000)));
  auto extra = to_int(powf(from_int(level * 5), 0x3f8ccccc));
  call(Service::DamageRadius, {P, vec(p + 0x10), std::uint32_t(signed16(Address(ability) + 0x28)) + std::uint32_t(extra), 0x100900u, 0u, 0u, 0x42700000u, 0x43700000u});
}
} // namespace champions::simulation::rta::player
