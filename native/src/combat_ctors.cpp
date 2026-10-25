// Authored from ref_combat_ctors.py at e9777eb25a150123e48452f25968e8ee263d0003
// SHA256 118fb30578b872b871b49b62038941739fd32ff869dc9cf14ed381e8a5e793cc
// (docs/spec/combat-ctors.md SHA256 4ad864cf...4040).
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cctype>

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
constexpr Address FRAME = 0x44fdcc, SKEL_ROW = 0x70;
constexpr Raw F_0_1 = 0x3dcccccc, F_0_2 = 0x3e4ccccc, F_0_3 = 0x3e999999,
              F_0_4 = 0x3ecccccc, F_0_5 = 0x3f000000, F_THIRD = 0x3eaaaaab,
              F_0_6 = 0x3f199999, F_0_7 = 0x3f333333, F_0_9 = 0x3f666666,
              F_1_2 = 0x3f999999, F_1_3 = 0x3fa66666, F_1_6 = 0x3fcccccc,
              F_2 = 0x40000000, F_5 = 0x40a00000, F_10 = 0x41200000,
              F_15 = 0x41700000, F_18 = 0x41900000, F_36 = 0x42100000,
              F_40 = 0x42200000, F_45 = 0x42340000, F_140 = 0x430c0000,
              F_200 = 0x43480000, F_240 = 0x43700000, F_1000 = 0x447a0000,
              F_2M16 = 0x37800000, F_0_25 = 0x3e800000;
// The element chance of each Skeleton weapon kind: (float)draw x 2^-16 below.
constexpr Raw SKEL_ELEMENT_CHANCE[5] = {0x3dccca2d, 0x3dccca2d, 0x3dcccab3,
                                        0x3dcccccc, 0x3dcccbc0};
Value iv(std::uint64_t v) { return std::int64_t(v & 0xffffffffu); }
Value sv(std::int32_t v) { return std::int64_t(std::uint32_t(v)); }
std::int64_t integer(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v))
    return *p;
  if (auto p = std::get_if<A::CallbackResult>(&v))
    if (p->kind == A::CallbackResult::Kind::integer)
      return p->negative ? -std::int64_t(p->magnitude) : std::int64_t(p->magnitude);
  if (std::holds_alternative<std::monostate>(v))
    return 0;
  throw A::Error("combat ctors: integer call-out result required");
}
Vec3 vector_of(const Value &v) {
  if (auto p = std::get_if<Vec3>(&v))
    return *p;
  if (auto p = std::get_if<A::CallbackResult>(&v))
    if (p->elements.size() == 3) {
      Vec3 out{};
      for (unsigned k = 0; k < 3; ++k)
        out[k] = Raw(p->elements[k].magnitude);
      return out;
    }
  throw A::Error("combat ctors: vector call-out result required");
}
Vec3 sample_point(const Value &v) {
  if (auto p = std::get_if<A::Sample>(&v))
    return p->point;
  if (auto p = std::get_if<A::CallbackResult>(&v))
    if (p->elements.size() == 2)
      return vector_of(Value(p->elements[0]));
  throw A::Error("combat ctors: model_getSamplePoint result required");
}
// The model's memory, read without recording; stores go through the model.
struct Body {
  A::Model &m;
  std::uint32_t u8(Address a) const { return std::uint32_t(m.mem.load(a, 1)); }
  std::int32_t s8(Address a) const {
    auto v = u8(a);
    return v & 0x80 ? std::int32_t(v) - 0x100 : std::int32_t(v);
  }
  std::uint32_t u16(Address a) const { return std::uint32_t(m.mem.load(a, 2)); }
  std::int32_t s16m(Address a) const { return s16(u16(a)); }
  Raw u32(Address a) const { return Raw(m.mem.load(a, 4)); }
  std::uint64_t u64(Address a) const { return m.mem.load(a, 8); }
  Vec3 vec(Address a) const { return {u32(a), u32(a + 4), u32(a + 8)}; }
  void w8(Address a, std::uint64_t v) { m.store(a, 1, v); }
  void w16(Address a, std::uint64_t v) { m.store(a, 2, v); }
  void w32(Address a, std::uint64_t v) { m.store(a, 4, v); }
  void w64(Address a, std::uint64_t v) { m.store(a, 8, v); }
  // The twelve-byte copy the compiler emits for a vector assignment.
  void wvec(Address a, const Vec3 &v) {
    w64(a, std::uint64_t(v[0]) | (std::uint64_t(v[1]) << 32));
    w32(a + 8, v[2]);
  }
  Value call(const std::string &name, std::vector<Value> args) {
    return m.external_call(name, std::move(args));
  }
  Raw draw(Address c) { return m.own_draw(c); }
  Raw clip(Address c, Raw off) { return m.clip(c, off); }
  void start(Address c, Raw off, Raw flags, Raw blend = F_0_1) {
    call("anim_start", {iv(c + 0x114), iv(clip(c, off)), iv(flags), iv(blend)});
  }
  void blend(Address c, Raw clip_word, Raw flags, Raw time = F_0_1) {
    call("anim_startBlendInOut",
         {iv(c + 0x114), iv(clip_word), iv(flags), iv(time), iv(time)});
  }
  // anim_pushWithWeights with f12 = weight 0, the rate, life 1.0, life rate 0.
  void weights(Address c, Raw clip_word, Raw flags, Raw rate) {
    call("anim_pushWithWeights", {iv(c + 0x114), iv(clip_word), iv(flags),
                                  iv(ZERO), iv(ZERO), iv(rate), iv(ONE), iv(ZERO)});
  }
  void fly_clips(Address c) {
    weights(c, clip(c, 0x3c), 0x203, F_THIRD);
    weights(c, clip(c, 0x44), 0x201, ZERO);
    weights(c, clip(c, 0x4c), 0x201, ZERO);
  }
  void event_times(Address c, Raw entry, std::array<Raw, 4> masks,
                   std::array<Raw, 4> times, const Options &o) {
    if (o.placed)
      call("creature_setDefEntryParams",
           {iv(c), iv(entry), iv(masks[0]), iv(masks[1]), iv(masks[2]),
            iv(masks[3]), iv(times[0]), iv(times[1]), iv(times[2]), iv(times[3])});
    else
      m.set_def_entry_params(c, entry, masks, times);
  }
  bool own_chance(Address c, Raw threshold) {
    return lt(mul(from_int(draw(c)), F_2M16), threshold);
  }
  Raw skel_clip(Address c, Raw off) {
    return u32(SKEL_COMMON + SKEL_ROW * u32(c + 0x3d0) + off);
  }
  // anm_setEventSlots 0x1183d0 (block, entry, four masks; four times):
  // creature_setDefEntryParams on a resource block.
  void event_slots(Address block, Raw entry, std::array<Raw, 4> masks,
                   std::array<Raw, 4> times, const Options &o) {
    if (o.placed) {
      call("anm_setEventSlots",
           {iv(block), iv(entry), iv(masks[0]), iv(masks[1]), iv(masks[2]),
            iv(masks[3]), iv(times[0]), iv(times[1]), iv(times[2]), iv(times[3])});
      return;
    }
    for (unsigned k = 0; k < 4; ++k) {
      auto record = Address(u32(block + 8 * entry + 4));
      w8(record + 0x18 + 8 * k, masks[k]);
      w32(record + 0x14 + 8 * k, times[k]);
    }
  }
  std::string lower_cstr(Address a) {
    auto s = m.cstr(a);
    for (auto &ch : s)
      if (ch >= 'A' && ch <= 'Z')
        ch = char(ch + 0x20);
    return s;
  }
};
Value class_value(std::int64_t v) { return v; }
} // namespace

const SkeletonPrefix SKELETON_PREFIXES[17] = {
    {0x54bd70, -1, 2, "skeletonred"},          {0x54bd80, -1, 1, "skeletonblue"},
    {0x54bd90, 0, 2, "skeletonswordohwred"},   {0x54bda8, 0, 1, "skeletonswordohwblue"},
    {0x54bdc0, 1, 2, "skeletonsworddhwred"},   {0x54bdd8, 1, 1, "skeletonsworddhwblue"},
    {0x54bdf0, 2, 2, "skeletonaxered"},        {0x54be00, 2, 1, "skeletonaxeblue"},
    {0x54be10, 3, 2, "skeletonspearred"},      {0x54be28, 3, 1, "skeletonspearblue"},
    {0x54be40, 4, 2, "skeletonbowred"},        {0x54be50, 4, 2, "skeletonbowblue"},
    {0x54be60, 0, -1, "skeletonswordohw"},     {0x54be78, 1, -1, "skeletonsworddhw"},
    {0x54be90, 2, -1, "skeletonaxe"},          {0x54bea0, 3, -1, "skeletonspear"},
    {0x54beb0, 4, -1, "skeletonbow"}};
const Slot ANTQUEEN_SLOTS[8] = {{0, 0x3c3c78}, {1, 0x3c3c10}, {2, 0x2378fc},
                                {3, 0x2378f4}, {4, ANTQUEEN_RUN}, {5, 0x236f10},
                                {15, 0x11f728}, {27, ANTQUEEN_SET_STATE}};
const Slot SKELETON_SLOTS[11] = {
    {0, 0x3cc62c}, {1, 0x3cc5b8},  {2, 0x2a774c},  {4, SKELETON_RUN},
    {5, 0x2a67c4}, {8, 0x2a75d4},  {9, 0x2a7508},  {12, 0x2a7688},
    {15, 0x1264b8}, {20, 0x2a7e04}, {27, SKELETON_SET_STATE}};
const Slot SOUL_SLOTS[10] = {{0, 0x3cc7a0}, {1, 0x3cc72c}, {2, 0x2aa908},
                             {3, 0x2aab60}, {4, SOUL_RUN},  {5, 0x2a9678},
                             {8, 0x2aabd0}, {9, 0x2aab68},  {16, SOUL_GET_MODEL},
                             {27, SOUL_SET_STATE}};

int str_prefix_differs(A::Model &m, Address text, Address prefix) {
  Body b{m};
  for (Address index = 0;; ++index) {
    auto want = b.s8(prefix + index);
    if (want == 0)
      return 0;
    auto have = b.s8(text + index);
    if (b.u8(Address(Raw(G_CTYPE + Raw(want)))) & 1)
      want += 0x20;
    if (b.u8(Address(Raw(G_CTYPE + Raw(have)))) & 1)
      have += 0x20;
    if (have != want)
      return 1;
  }
}
bool on_airhub(A::Model &m) {
  Body b{m};
  return b.lower_cstr(G_LEVEL_NAME) == b.lower_cstr(S_AIRHUB);
}

// ---- AntQueen -------------------------------------------------------------
// AntQueen_ctor 0x236c70 (object, type name, position, facing, property list).
void antqueen_ctor(A::Model &m, Address c, Raw type_name, const Vec3 &pos,
                   Raw facing, Raw props, const Options &o) {
  Body b{m};
  if (!o.placed) {
    // One own draw from the block's word +0x44 before GameObject_ctor seeds it;
    // the hit points it gives are replaced by Creature_setStats.
    auto hp = from_int(b.draw(c) % 14 + 4);
    m.creature_ctor(c, type_name, pos, Raw(s16(facing)), DEF_ANTQUEEN, props, hp);
  }
  b.w32(c + 0xb0, F_1_2);
  b.w32(c + 0x1c, F_45);
  b.w32(c + 0x48, VT_ANTQUEEN);
  b.w32(c + 0xb8, F_140);
  b.w8(c + 0x28, 0x37);
  b.w16(c + 0x232, 0);
  b.w32(c + 0x5c, F_0_4);
  b.w8(c + 0xb4, 0x13);
  b.w32(c + 0xec, b.u32(FRAME));
  b.w32(c + 0x3d4, 180);
  b.w64(c + 0xa8, b.u64(c + 0xa8) | UINT64_C(0x8020001000));
  auto d = b.draw(c);
  b.w32(c + 0xe8, 0);
  b.w32(c + 0x3d8, b.u32(c + 0x300) + d % 180 + 180);
  b.w64(c + 0x220, std::uint64_t(integer(b.call(
                       "model_buildHideMask", {iv(m.class_model(c)), iv(0)}))));
  b.call("anim_clear", {iv(c + 0x114)});
  b.call("anim_push", {iv(c + 0x114), iv(b.clip(c, 0x7c)), iv(1)});
  b.call("obj_runEvery60", {iv(c)});
  b.event_times(c, 3, {1, 0, 0, 0}, {F_0_2, 0, 0, 0}, o);
  for (Raw entry : {4u, 5u, 6u})
    b.event_times(c, entry, {1, 0, 0, 0}, {F_0_3, 0, 0, 0}, o);
  b.event_times(c, 0x13, {1, 0, 0, 0}, {F_0_2, 0, 0, 0}, o);
  b.call("Creature_init", {iv(c)});
}
// AntQueen_run 0x236f78 up to its switch: Creature_updateActive's result is
// not tested.
void antqueen_run_prologue(A::Model &m, Address c) {
  Body b{m};
  m.process_damage(c, m.class_model(c));
  if (m.creature_run(c, m.class_model(c)))
    return;
  m.update_active(c);
  m.step_anim(c, ONE);
  b.call("Creature_applyKnockback", {iv(c), iv(HALF)});
  m.vcall(c, 23, {iv(0)});
}
// AntQueen_setState 0x237918 (creature, state), slot 27.
void antqueen_set_state(A::Model &m, Address c, Raw requested) {
  Body b{m};
  auto filtered = m.filter_state(c, requested);
  if (filtered.refused)
    return;
  auto state = filtered.state;
  const Address anim = c + 0x114;
  const auto old = b.s16m(c + 0x232);
  auto clock = [&] { return b.u32(c + 0x300); };
  switch (state) {
  case 0: {
    auto d = b.draw(c);
    b.w32(c + 0xf4, clock() + d % 120);
    b.start(c, 0x7c, 3, F_0_3);
    break;
  }
  case 1:
    m.clear_path(c);
    m.pick_wander_dest(c);
    b.w32(c + 0xb8, F_140);
    b.start(c, 0xac, 3, F_0_3);
    break;
  case 3:
    if (old != 0x12 && old != 6 && old != 7) {
      m.clear_path(c);
      b.w32(c + 0xb8, F_140);
      b.weights(c, b.clip(c, 0xac), 0x203, F_THIRD);
      b.weights(c, b.clip(c, 0xb4), 0x201, ZERO);
      b.weights(c, b.clip(c, 0xbc), 0x201, ZERO);
    }
    b.w16(c + 0x3c4, 0xabe0);
    break;
  case 5:
    b.start(c, 0x84, 3, F_0_1);
    break;
  case 6:
    b.blend(c, b.clip(c, 0x1c), 8);
    b.w32(c + 0x3cc, 0);
    break;
  case 7:
    if (b.u32(c + 0x234)) {
      Raw entry = 4;
      auto found = Raw(integer(b.call(
          "creature_findEnemyInCone",
          {iv(c), sv(s16(b.u16(c + 0x38) - 0x7fff)), iv(6000), iv(ZERO), iv(F_40)})));
      if (found) {
        auto bearing = atan2_16(sub(b.u32(found + 0x14), b.u32(c + 0x14)),
                                sub(b.u32(found + 0x10), b.u32(c + 0x10)));
        auto diff = s16(Raw(bearing - std::int32_t(b.u16(c + 0x38)) + 0x7fff));
        entry = diff > 2000 ? 6 : diff < -2000 ? 5 : 4;
      }
      b.blend(c, b.clip(c, 8 * entry + 4), 8);
    }
    break;
  case 8:
    b.w32(c + 0xb8, F_240);
    m.clear_path(c);
    if (old != 0x17 && old != 9) {
      auto d = b.draw(c);
      b.w32(c + 0x3d4, clock() + d % 180 + 300);
      b.fly_clips(c);
      b.blend(c, b.clip(c, 0xa4), 8);
    }
    break;
  case 9:
  case 10:
    b.blend(c, b.clip(c, 0x9c), 8);
    b.w32(c + 0x3cc, 0);
    break;
  case 0x12:
    if (old == 8 || old == 0x17 || old == 9 || (old == 6 && (b.u16(c + 0x11a) & 8))) {
      state = old;
    } else {
      auto hit = b.u16(c + 0x2e0);
      Raw off;
      if (((hit + 0x1fff) & 0xffff) < 0x3fff)
        off = 0x54;
      else if (((hit - 0x2001) & 0xffff) < 0x3fff)
        off = 0x6c;
      else if (-0x5fff <= s16(hit) && s16(hit) < -0x2000)
        off = 0x64;
      else
        off = 0x5c;
      b.blend(c, b.clip(c, off), 0x10);
    }
    break;
  case 0x13:
    b.start(c, 0x7c, 3, F_0_3);
    break;
  case 0x14: {
    auto pos = b.vec(c + 0x10);
    b.call("Creature_dropLoot",
           {iv(c), pos, sv(b.s16m(c + 0x38)), iv(0), iv(F_36), iv(F_2), iv(F_5),
            iv(ONE), iv(F_5), iv(ONE)});
    b.start(c, 0xc4, 0x16, F_0_4);
    if (!integer(b.call("anim_findTrack", {iv(anim), iv(b.clip(c, 0x3c))})))
      b.blend(c, b.clip(c, 0xa4), 8);
    m.vcall(c, 28, {iv(0)});
    break;
  }
  case 0x17: {
    auto d = b.draw(c);
    b.w32(c + 0xb8, F_240);
    b.w32(c + 0x3d4, clock() + d % 120 + 180);
    m.clear_path(c);
    // AntQueen_pickFlyDest 0x238150, executed in research-combat-ctors-2
    // (combat_ctors_2.cpp). A model that answers it as a call-out (the
    // round-1 vectors pin e9777eb, where it was a recording stub) gets that
    // answer instead.
    auto dest = m.callbacks.count("AntQueen_pickFlyDest")
                    ? vector_of(b.call("AntQueen_pickFlyDest", {iv(c)}))
                    : antqueen_pick_fly_dest(m, c);
    b.wvec(c + 0x104, dest);
    if (((b.u16(c + 0x232) - 8) & 0xffff) >= 2) {
      b.fly_clips(c);
      b.blend(c, b.clip(c, 0xa4), 8);
    }
    break;
  }
  case 0x18: {
    auto d = b.draw(c);
    b.w32(c + 0x3d4, clock() + d % 180 + 180);
    b.start(c, 0x84, 3, F_0_1);
    b.blend(c, b.clip(c, 0x94), 8);
    break;
  }
  case 0x19: {
    auto d = b.draw(c);
    b.w32(c + 0x3d8, clock() + d % 180 + 180);
    b.start(c, 0x84, 3, F_0_1);
    b.blend(c, b.clip(c, 0x8c), 8);
    b.w32(c + 0xf4, clock() + b.draw(c) % 60 + 120);
    break;
  }
  default:
    break;
  }
  b.w16(c + 0x232, Raw(state));
}

// ---- Skeleton ------------------------------------------------------------
namespace {
struct EventRow {
  std::initializer_list<Raw> entries;
  std::array<Raw, 4> masks, times;
};
const Raw SKEL_EVENT_ORDER[] = {1,    2,    3,    0xf,  0x10, 0x11, 0x1d, 0x1e,
                                0x1f, 0x2b, 0x2c, 0x2d, 0x39, 0x3a, 0x3b};
void skel_events(Body &b, Raw entry, const Options &o) {
  switch (entry) {
  case 0x1d:
  case 0x1e:
    b.event_slots(SKEL_COMMON, entry, {1, 1, 0, 0}, {F_0_3, F_0_9, 0, 0}, o);
    break;
  case 0x39:
  case 0x3a:
  case 0x3b: // the bow row: bit 1, then bit 0
    b.event_slots(SKEL_COMMON, entry, {2, 1, 0, 0}, {F_0_2, F_1_3, 0, 0}, o);
    break;
  default:
    b.event_slots(SKEL_COMMON, entry, {1, 0, 0, 0}, {F_0_3, 0, 0, 0}, o);
    break;
  }
}
} // namespace
// Skeleton_ctor 0x2a5af4 (object, position, facing, property list, type name,
// model set).
void skeleton_ctor(A::Model &m, Address c, const Vec3 &pos, Raw facing,
                   Raw props, Raw type_name, Raw model_set, const Options &o) {
  Body b{m};
  const Address holder = c + 0x3d8;
  if (!o.placed)
    m.creature_ctor(c, type_name, pos, Raw(s16(facing)),
                    Raw(DEF_SKELETON + (model_set << 6)), props, F_10);
  b.w32(c + 0x48, VT_SKELETON);
  b.w32(c + 0x3c8, 0);
  b.w32(c + 0x3cc, 0);
  const Raw shield = VT_WEAPONSHIELD;
  m.init_holder(c, holder, &shield);
  if (b.u32(SKEL_COMMON + 4) != b.u32(G_LUMP_GENERATION)) {
    for (Address k = 8; b.u32(SKEL_COMMON + k); k += 8)
      b.w32(SKEL_COMMON + k + 4,
            Raw(integer(b.call("res_find", {iv(b.u32(SKEL_COMMON)),
                                            iv(b.u32(SKEL_COMMON + k))}))));
    b.w32(SKEL_COMMON + 4, b.u32(G_LUMP_GENERATION));
  }
  b.w8(c + 0x28, 0x14);
  b.w32(c + 0xb0, F_1_2);
  b.w32(c + 0x1c, F_18);
  b.w32(c + 0xb8, 0);
  b.w16(c + 0x232, 0);
  b.w64(c + 0xa8, b.u64(c + 0xa8) | 0x10000);
  b.w8(c + 0xb4, 0x10);
  b.w32(c + 0xec, b.u32(FRAME));
  b.w32(c + 0x64, ONE);
  b.w32(c + 0x50, holder);
  b.w32(c + 0x68, ONE);
  b.w32(c + 0x3d0, b.draw(c) % 5);
  b.w8(c + 0x3d4, 0);
  for (const auto &p : SKELETON_PREFIXES)
    if (!str_prefix_differs(m, type_name, p.text)) {
      if (p.kind >= 0)
        b.w32(c + 0x3d0, Raw(p.kind));
      if (p.side >= 0)
        b.w8(c + 0x3d4, Raw(p.side));
      break;
    }
  const auto kind = b.u32(c + 0x3d0);
  Raw element = 0;
  auto hand = [&](Raw which, Raw part) {
    if (b.u32(c + 0x50))
      m.holder_call(b.u32(c + 0x50), 1, {iv(which), iv(part)});
  };
  if (kind < 5) {
    if (b.own_chance(c, SKEL_ELEMENT_CHANCE[kind]))
      element = (kind == 4 ? b.draw(c) % 3 : b.draw(c) & 1) + 1;
    if (kind == 0) {
      auto part = b.draw(c) & 1 ? 0x52u : 0x54u;
      hand(1, part);
      auto second = 0x60u + 2 * (b.draw(c) & 1);
      hand(2, second);
    } else if (kind == 1) {
      hand(1, 0x5e);
    } else if (kind == 2) {
      auto part = b.draw(c) & 1 ? 0x5au : 0x5cu;
      hand(1, part);
    } else if (kind == 3) {
      auto part = b.draw(c) & 1 ? 0x64u : 0x66u;
      hand(1, part);
    } else {
      hand(1, 0x58);
    }
  }
  b.w32(c + 0xe8, 0);
  auto d = b.draw(c);
  if (d & 1)
    b.w32(c + 0xe8, b.u32(c + 0xe8) | (1u << (b.draw(c) % 3)));
  if (kind == 4)
    b.w32(c + 0xe8, b.u32(c + 0xe8) | 0x80);
  else if (d & 2)
    b.w32(c + 0xe8, b.u32(c + 0xe8) | (1u << (b.draw(c) % 12 + 9)));
  if (!(b.u32(c + 0xe8) & 0x4000))
    b.w32(c + 0xe8, b.u32(c + 0xe8) | (1u << (b.draw(c) % 5 + 21)));
  b.w32(c + 0xe8, b.u32(c + 0xe8) | (1u << ((b.draw(c) & 1) + 5)));
  b.w64(c + 0x220, std::uint64_t(integer(b.call(
                       "model_buildHideMask",
                       {iv(m.class_model(c)), iv(b.u32(c + 0xe8))}))));
  unsigned slot = element == 1 ? 4 : element == 2 ? 2 : element == 3 ? 3 : 0;
  if (slot && b.u32(c + 0x50))
    m.holder_call(b.u32(c + 0x50), slot, {iv(1)});
  const auto bits = b.u32(c + 0xe8);
  const std::uint64_t flag = bits & 0x21e00    ? UINT64_C(0x800000000)
                             : bits & 0x1de000 ? UINT64_C(0x2000000000)
                                               : UINT64_C(0x10000000000);
  b.w64(c + 0xa8, b.u64(c + 0xa8) | flag);
  b.w32(c + 0x3c0, b.u8(c + 0x3d4) ? F_200 : F_1000);
  b.call("anim_clear", {iv(c + 0x114)});
  b.call("anim_push", {iv(c + 0x114), iv(b.skel_clip(c, 0x44)), iv(1)});
  skeleton_set_state(m, c, 0);
  b.call("obj_runEvery60", {iv(c)});
  for (auto entry : SKEL_EVENT_ORDER)
    skel_events(b, entry, o);
  b.call("Creature_init", {iv(c)});
}
// Skeleton_run 0x2a6864 up to its switch: an inactive skeleton returns.
bool skeleton_run_prologue(A::Model &m, Address c, Raw *events) {
  Body b{m};
  m.process_damage(c, m.class_model(c));
  if (m.creature_run(c, m.class_model(c)))
    return false;
  if (!m.update_active(c))
    return false;
  if (b.u32(c + 0x50))
    m.holder_call(b.u32(c + 0x50), 21, {iv(SKEL_COMMON), iv(c), iv(HALF)});
  auto ev = m.step_anim(c, ONE);
  if (events)
    *events = ev;
  b.call("Creature_applyKnockback", {iv(c), iv(F_0_7)});
  m.vcall(c, 23, {iv(0)});
  return true;
}
// Skeleton_setState 0x2a7768 (creature, state), slot 27.
void skeleton_set_state(A::Model &m, Address c, Raw requested) {
  Body b{m};
  auto filtered = m.filter_state(c, requested);
  if (filtered.refused)
    return;
  auto state = filtered.state;
  const Address anim = c + 0x114;
  auto cf = b.u64(c + 0xa8) & ~UINT64_C(0x200);
  b.w64(c + 0xa8, cf);
  if (state != 0x17)
    b.w64(c + 0xa8, cf & UINT64_C(0xffffffffffdff7ff));
  auto clip = [&](Raw off) { return b.skel_clip(c, off); };
  auto start = [&](Raw off, Raw flags) {
    b.call("anim_start", {iv(anim), iv(clip(off)), iv(flags), iv(F_0_1)});
  };
  switch (state) {
  case 0:
    start(0x44, 3);
    b.w32(c + 0xf4, b.u32(c + 0x300) + 120);
    break;
  case 1:
    m.clear_path(c);
    b.w32(c + 0xf4, b.u32(c + 0x300) + 300);
    start(0x54, 0x803);
    b.call("anim_syncToSimFrame", {iv(anim)});
    break;
  case 3:
  case 0x10:
    start(0x6c, 0x803);
    b.call("anim_syncToSimFrame", {iv(anim)});
    break;
  case 5:
    start(0x2c, 3);
    break;
  case 6:
  case 8: {
    start(state == 6 ? 0x2c : 0x3c, 3);
    auto swing = clip(0xc + 8 * (b.draw(c) & 1));
    b.blend(c, swing, 0x808);
    b.w32(c + 0xf4, state == 6 ? b.draw(c) % 20 + 20 : 1);
    break;
  }
  case 7:
    start(0x2c, 3);
    b.blend(c, clip(0x1c), 0x808);
    b.w32(c + 0xf4, b.draw(c) % 20 + 20);
    break;
  case 0x11:
    start(0x2c, 3);
    b.w32(c + 0xf4, b.u32(c + 0x300) + 120);
    b.w64(c + 0xa8, b.u64(c + 0xa8) | 0x200);
    break;
  case 0x12: {
    // The +0x24 branch for a blocking skeleton tests flag 0x200 after the
    // clear above, so it is never taken (combat-ctors.md section 5).
    auto old = b.s16m(c + 0x232);
    if (old == 0x17)
      state = old;
    else if (b.u64(c + 0xa8) & 0x200)
      b.blend(c, clip(0x24), 0x10);
    else
      b.blend(c, clip(0x34), 0x10);
    break;
  }
  case 0x13:
    start(0x44, 3);
    break;
  case 0x14: {
    for (Raw which : {1u, 2u})
      if (b.u32(c + 0x50))
        m.holder_call(b.u32(c + 0x50), 11,
                      {iv(which), iv(c), iv(SKEL_COMMON), iv(0)});
    b.w32(c + 0xc, b.u32(c + 0xc) | 0x4002);
    b.w64(c + 0xa8, b.u64(c + 0xa8) | UINT64_C(0x20000000000));
    b.call("Skeleton_shatter", {iv(c), iv(8)});
    if (!(b.u64(c + 0xa8) & 0x1000000)) {
      auto pos = b.vec(c + 0x10);
      b.call("Creature_dropLoot",
             {iv(c), pos, sv(b.s16m(c + 0x38)), iv(S_BONECHIP), iv(F_36), iv(F_2),
              iv(F_1_6), iv(F_0_6), iv(F_1_6), iv(F_0_25)});
    }
    b.call("obj_delete", {iv(c)});
    m.vcall(c, 28, {iv(0)});
    break;
  }
  case 0x17:
    b.w64(c + 0xa8, b.u64(c + 0xa8) | 0x200800);
    b.call("anim_push", {iv(anim), iv(clip(0x74)), iv(0x10)});
    b.call("anim_pushChained", {iv(anim), iv(clip(0x44)), iv(3)});
    break;
  default:
    break;
  }
  b.w16(c + 0x232, Raw(state));
}

// ---- Soul -------------------------------------------------------------------
namespace {
void soul_burst(A::Model &m, Address c) {
  Body b{m};
  auto count = s32(Raw(integer(b.call(
      "model_getSamplePointCount", {iv(Raw(integer(m.vcall(c, 16))))}))));
  for (std::int32_t index = 0; index < std::max(count, 0); ++index) {
    auto model = Raw(integer(m.vcall(c, 16)));
    auto point = sample_point(b.call(
        "model_getSamplePoint", {iv(model), iv(c + 0x114), Vec3{0, 0, 0},
                                 iv(0xffffc000), iv(Raw(index)), iv(1), iv(0),
                                 iv(b.u32(c + 0xdc))}));
    if (b.u32(c + 0x3c8))
      throw A::Unsupported("Soul_setState 0x21 with an owner: not reachable, not modelled");
    for (int k = 0; k < 2; ++k) {
      Raw v[3];
      for (;;) {
        for (auto &e : v) {
          auto f = mul(from_int(b.draw(c)), F_2M16);
          e = sub(add(f, f), ONE);
        }
        auto size = add(add(mul(v[0], v[0]), mul(v[1], v[1])), mul(v[2], v[2]));
        if (!lt(ONE, size))
          break;
      }
      // The copy's z word is zeroed before the scale (0x2ab1f0): the burst is flat.
      b.call("particle_spawn",
             {iv(P_SOUL), point, Vec3{mul(v[0], F_15), mul(v[1], F_15), mul(ZERO, F_15)},
              iv(ZERO)});
    }
  }
  b.w32(c + 0xc, b.u32(c + 0xc) | 0x4002);
  b.call("obj_delete", {iv(c)});
}
} // namespace
// Soul_ctor 0x2a9478 (object, type name, position, facing, property list,
// owner).
void soul_ctor(A::Model &m, Address c, Raw type_name, const Vec3 &pos,
               Raw facing, Raw props, Raw owner, const Options &o) {
  Body b{m};
  if (!o.placed)
    m.creature_ctor(c, type_name, pos, Raw(s16(facing)), DEF_SOUL, props, F_10);
  b.w32(c + 0x3c8, 0);
  b.w32(c + 0x48, VT_SOUL);
  b.w32(c + 0xb0, F_1_2);
  b.w32(c + 0x1c, F_15);
  b.w8(c + 0x28, 0x1e);
  b.w32(c + 0x3cc, 0);
  b.w32(c + 0xb8, 0);
  b.w16(c + 0x232, 0);
  b.w32(c + 0xf4, 0);
  b.w32(c + 0xec, b.u32(FRAME));
  b.w64(c + 0xa8, b.u64(c + 0xa8) | UINT64_C(0x200080000));
  b.call("Creature_setCharm", {iv(c), iv(M32), iv(0)});
  for (Address off : {0x5c, 0x60, 0x64, 0x68, 0x6c})
    b.w32(c + off, ONE);
  b.call("anim_clear", {iv(c + 0x114)});
  if (owner) {
    b.w32(c + 0xc, b.u32(c + 0xc) | 9);
    b.call("handle_link", {iv(c + 0x3c8), iv(owner), iv(owner)});
    b.call("anim_push", {iv(c + 0x114), iv(b.clip(c, 0x6c)), iv(1)});
  } else if (on_airhub(m)) {
    b.call("anim_push", {iv(c + 0x114), iv(b.clip(c, 0x34)), iv(1)});
    soul_set_state(m, c, 0x22);
  } else {
    b.call("anim_push", {iv(c + 0x114), iv(b.clip(c, 0x3c)), iv(1)});
  }
  b.w64(c + 0xa8, b.u64(c + 0xa8) | UINT64_C(0x8000000000000000));
  b.call("obj_runEveryFrame", {iv(c)});
  b.event_times(c, 0xb, {1, 0, 0, 0}, {F_0_7, 0, 0, 0}, o);
  b.call("Creature_init", {iv(c)});
}
// Soul_getModel 0x2ab298 (slot 16): the class block's +0x1c while owned, else +0xc.
Raw soul_get_model(A::Model &m, Address c) {
  Body b{m};
  return b.u32(Address(b.u32(c + 0x24) + (b.u32(c + 0x3c8) ? 0x1c : 0xc)));
}
// Soul_run 0x2a9870 up to its switch: an inactive soul returns.
bool soul_run_prologue(A::Model &m, Address c, Raw *events) {
  Body b{m};
  m.process_damage(c, Raw(integer(m.vcall(c, 16))));
  if (m.creature_run(c, Raw(integer(m.vcall(c, 16)))))
    return false;
  if (!m.update_active(c))
    return false;
  auto ev = m.step_anim(c, ONE);
  if (events)
    *events = ev;
  b.call("Creature_applyKnockback", {iv(c), iv(ZERO)});
  m.vcall(c, 23, {iv(0)});
  if (!b.u32(c + 0x3c8) && b.s16m(c + 0x232) != 0x1b)
    m.move_by(c, {ZERO, ZERO, ZERO});
  return true;
}
// Soul_setState 0x2aac38 (creature, state), slot 27.
void soul_set_state(A::Model &m, Address c, Raw requested) {
  Body b{m};
  if (on_airhub(m))
    requested = 0x22;
  auto filtered = m.filter_state(c, requested);
  if (filtered.refused)
    return;
  auto state = filtered.state;
  const auto old = b.s16m(c + 0x232);
  if (b.u32(c + 0x3c8) && !(0x1a <= state && state <= 0x20))
    return;
  if (old == 0x1b)
    return;
  const Address anim = c + 0x114;
  auto clock = [&] { return b.u32(c + 0x300); };
  switch (state) {
  case 0: {
    auto d = b.draw(c);
    b.w32(c + 0xf4, clock() + d % 120 + 60);
    b.start(c, 0x3c, 3, F_0_3);
    break;
  }
  case 1:
    m.clear_path(c);
    m.pick_wander_dest(c);
    b.start(c, 0x4c, 0x803, F_0_3);
    b.call("anim_syncToSimFrame", {iv(anim)});
    b.w32(c + 0xf4, clock() + b.draw(c) % 120 + 300);
    break;
  case 5:
  case 0x17:
    b.start(c, 0x34, 3, F_0_3);
    break;
  case 0x10:
  case 0x13:
    state = old;
    break;
  case 0x12:
    m.vcall(c, 27, {iv(old == 0x18 ? 0x19 : 0x18)});
    break;
  case 0x14:
    b.start(c, 0x94, 6, F_0_1);
    b.w32(c + 0xf4, clock() + 180);
    b.w32(c + 0xc, b.u32(c + 0xc) | 0x4000);
    m.vcall(c, 28, {iv(0)});
    break;
  case 0x18:
    b.start(c, 0x44, 3, F_0_3);
    b.w32(c + 0xf4, clock() + b.draw(c) % 120);
    break;
  case 0x19:
    if (old == 0x18)
      b.blend(c, b.clip(c, 0x2c), 0x10);
    else
      state = old;
    break;
  case 0x1a:
    b.blend(c, b.clip(c, 0x54), 8);
    break;
  case 0x1b:
    b.call("handle_clear", {iv(c + 0x3c8)});
    b.w32(c + 0x3c8, 0);
    b.w32(c + 0xf4, clock() + 240);
    break;
  case 0x1c:
    b.blend(c, b.clip(c, 0x74), 0xc);
    break;
  case 0x1d:
    b.blend(c, b.clip(c, 0x5c), 8, F_0_5);
    break;
  case 0x1e:
    b.start(c, 0x6c, 3, F_0_1);
    break;
  case 0x1f:
    b.start(c, 0x7c, 3, F_0_3);
    break;
  case 0x20:
    b.blend(c, b.clip(c, 0x84), 8);
    break;
  case 0x21:
    soul_burst(m, c);
    break;
  case 0x22:
    b.start(c, 0x94, 6, F_0_1);
    b.w32(c + 0xf4, clock() + 300);
    b.w32(c + 0xc, b.u32(c + 0xc) | 9);
    break;
  default:
    break;
  }
  b.w16(c + 0x232, Raw(state));
}

void install(A::Model &m) {
  auto state = [](void (*body)(A::Model &, Address, Raw)) {
    return [body](A::Model &model, Address self, const std::vector<Value> &args) -> Value {
      if (args.size() != 1)
        throw A::Error("set-state takes one argument");
      body(model, self, Raw(integer(args[0])));
      return std::monostate{};
    };
  };
  m.class_bodies[ANTQUEEN_SET_STATE] = state(antqueen_set_state);
  m.class_bodies[SKELETON_SET_STATE] = state(skeleton_set_state);
  m.class_bodies[SOUL_SET_STATE] = state(soul_set_state);
  m.class_bodies[SOUL_GET_MODEL] = [](A::Model &model, Address self,
                                      const std::vector<Value> &args) -> Value {
    if (!args.empty())
      throw A::Error("Soul_getModel takes no argument");
    return class_value(std::int64_t(soul_get_model(model, self)));
  };
}
} // namespace champions::simulation::rta::combat_ctors
