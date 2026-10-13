// Authored translation of ref_player.py at
// 356add27d50e50972b2a671b31e8f4ec93e6778c. SHA256
// ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d. MIT, Open
// Champions contributors. Only documented entry/scripted and helpers.
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
                                          "anim_startBlendInOut"};
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
std::uint32_t Tick::call(Service s, std::vector<Argument> args) {
  service_name(s);
  if (calls.size() >= 65536 || events.size() >= 65536 || callbacks.size() > 19)
    throw ResourceLimit("player callback/history cap");
  Call c{s, std::move(args)};
  calls.push_back(c);
  events.push_back({true, {}, c});
  auto found = callbacks.find(s);
  if (found == callbacks.end() || !found->second) {
    if (policy == Policy::ReferenceZero)
      return 0;
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
} // namespace champions::simulation::rta::player
