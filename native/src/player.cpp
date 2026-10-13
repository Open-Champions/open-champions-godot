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
                                          "handle_clear"};
  auto i = static_cast<unsigned>(s);
  if (i >= sizeof(names) / sizeof(*names))
    throw Error("unknown player service");
  return names[i];
}
Tick::Tick(Memory &m, std::uint32_t p, Policy selected)
    : mem(m), P(p), policy(selected) {
  if (P > UINT32_MAX - 0x58f)
    throw ResourceLimit("player object outside raw32");
  if (policy != Policy::RequireHandlers && policy != Policy::ReferenceZero)
    throw Error("unknown callback policy");
}
std::uint32_t Tick::u32(Address a) const {
  return std::uint32_t(mem.load(a, 4));
}
std::int32_t Tick::signed8(Address a) const {
  auto v = mem.load(a, 1);
  return v & 128 ? int(v) - 256 : int(v);
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
  if (calls.size() >= 65536 || events.size() >= 65536 || callbacks.size() > 45)
    throw ResourceLimit("player callback/history cap");
  Call c{s, std::move(args)};
  calls.push_back(c);
  events.push_back({true, {}, c});
  auto found = callbacks.find(s);
  if (found == callbacks.end() || !found->second) {
    if (policy == Policy::ReferenceZero) {
      if (s == Service::AttachPoint) return Vec3{};
      if (s == Service::CollectArc) return std::vector<std::uint32_t>{};
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
void Tick::entry() {
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
  R0 = rec(slot());
  if (lt(ZERO, u32(R0 + 0xac))) {
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
  if (rumble && lt(ZERO, u32(R0 + 0xac)) &&
      lt(div(u32(R0 + 0xac), u32(R0 + 0xb0)), 0x3ecccccc)) {
    auto phase = u32(FRAME) & 0x3f;
    if (phase == 0)
      call(Service::Rumble, {std::uint32_t(slot()), 0x3df5c28fu, ONE});
    if (phase == 20)
      call(Service::Rumble, {std::uint32_t(slot()), 0x3db851ebu, ONE});
  }
}
std::string Tick::run() {
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
std::string Tick::cstr(Address a) const {
  std::string out;
  for (unsigned i = 0; i < 256; ++i) {
    auto b = mem.load(indexed(a, i), 1);
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
    auto level = from_int(passive_level(std::uint32_t(sl), 0x10));
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
    auto level = from_int(passive_level(std::uint32_t(sl), 0x200));
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
    auto rank = skill_rank(std::uint32_t(signed32(p + 0x3c8)), 0x13);
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
void Tick::use_potion(std::uint32_t kind) {
  use_potion_real(P, kind, 0, 0, UINT32_MAX);
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
} // namespace champions::simulation::rta::player
