#include "champions/level_commands.hpp"

#include <cstdio>
#include <stdexcept>

namespace champions::simulation::rta::level_commands {
namespace {
constexpr std::uint64_t M64 = UINT64_MAX;
std::uint64_t sext32(std::uint32_t value) {
  return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));
}
std::string hex(std::uint32_t value) {
  char text[16];
  std::snprintf(text, sizeof text, "0x%x", value);
  return text;
}
} // namespace

std::uint64_t model_build_hide_mask(const Memory &mem, std::uint32_t model, std::uint64_t bits) {
  // ~(model + 8); with the model's u16 +0x10 bit 2 and bits != 0, each set bit k
  // (low first) ORs record k's first u64 and clears its second; records are 16
  // bytes from model + 16 * u8(+0x21); bits shift as a 32-bit value (ref:265-281).
  std::uint64_t result = ~mem.u64(model + 8) & M64;
  if (!(mem.u16(model + 0x10) & 2) || bits == 0)
    return result;
  std::uint32_t record = model + std::uint32_t(mem.u8(model + 0x21)) * 16u;
  while (true) {
    if (bits & 1)
      result = (result | mem.u64(record)) & ~mem.u64(record + 8);
    bits = sext32(static_cast<std::uint32_t>(bits & 0xffffffffu) >> 1);
    record += 16u;
    if (bits == 0)
      return result;
  }
}

void anim_push(Memory &mem, std::uint32_t state, std::uint32_t clip, std::uint32_t mode) {
  // ref:284-311: nothing for a null clip or a full state (s8 +4 >= s8 +5).
  if (clip == 0)
    return;
  const auto count = mem.s8(state + 4), capacity = mem.s8(state + 5);
  const std::uint32_t track = state + std::uint32_t(count) * 32u + 8u;
  if (!(count < capacity))
    return;
  mem.w8(state + 4, (mem.u8(state + 4) + 1u) & 0xffu);
  const auto flags = (mem.u16(state + 6) | mode) & 0xffffu;
  const auto old = mem.u32(track + 4);
  mem.w32(track, clip);
  mem.w16(state + 6, flags);
  mem.w32(track + 0x18, 0);
  mem.w16(track + 0x1e, 0);
  if (old) {
    mem.w32(old + 0x14, 0);
    mem.w32(track + 4, 0);
  }
  mem.w16(track + 0x1c, mode);
  mem.w32(track + 0x0c, ZERO);
  mem.w32(track + 0x08, ONE);
  mem.w32(track + 0x10, ONE);
  mem.w32(track + 0x14, ZERO);
}

void anim_jump_top_to_end(Memory &mem, std::uint32_t state) {
  // ref:313-331: a cache pointer at +0 is dropped (it and its +0x610 zeroed);
  // the top track's time +0x18 = float(clip length at clip + 4).
  const auto cache = mem.u32(state);
  if (cache) {
    mem.w32(state, 0);
    mem.w32(cache + 0x610, 0);
  }
  const auto count = mem.s8(state + 4);
  if (count == 0)
    return;
  const std::uint32_t track = state + std::uint32_t(count - 1) * 32u + 8u;
  const auto clip = mem.u32(track);
  if ((clip + 4u) & 3u)
    throw std::logic_error("anim_jumpTopToEnd: unaligned clip reached the port");
  const Fpu fpu;
  const auto length = fpu.from_int(mem.u32(clip + 4));
  if (mem.u16(track + 0x1c) & 0x800)
    throw std::logic_error("anim_jumpTopToEnd: a track with flag 0x800 reached the port");
  mem.w32(track + 0x18, length);
}

void anim_start(Memory &mem, std::uint32_t state, std::uint32_t clip, std::uint32_t flags, Raw blend) {
  // ref:333-370: fade-in rate 1 / (blend x 60), 1.0 below 1/60; the first track
  // has no blend; a full state replaces its top track.
  if (clip == 0)
    return;
  const auto count = mem.s8(state + 4), capacity = mem.s8(state + 5);
  auto index = count;
  if (!(count < capacity)) {
    index = count - 1;
    const std::uint32_t link = state + 0x0cu + std::uint32_t(index) * 32u;
    const auto held = mem.u32(link);
    if (held) {
      mem.w32(held + 0x14, 0);
      mem.w32(link, 0);
    }
  }
  if (index == 0)
    blend = ZERO;
  const std::uint32_t track = state + std::uint32_t(index) * 32u + 8u;
  mem.w8(state + 4, std::uint32_t(index + 1) & 0xffu);
  const auto old_flags = mem.u16(state + 6);
  const auto old = mem.u32(track + 4);
  mem.w32(track, clip);
  mem.w16(state + 6, (old_flags | flags) & 0xffffu);
  mem.w32(track + 0x18, 0);
  mem.w16(track + 0x1e, 0);
  if (old) {
    mem.w32(old + 0x14, 0);
    mem.w32(track + 4, 0);
  }
  mem.w16(track + 0x1c, flags);
  mem.w32(track + 8, 0);
  const Fpu fpu;
  Raw rate = ONE;
  if (!lt(blend, F_1_60))
    rate = fpu.div(ONE, fpu.mul(blend, F_60));
  mem.w32(track + 0x0c, rate);
  mem.w32(track + 0x10, ONE);
  mem.w32(track + 0x14, 0);
}

std::optional<std::string> autoprop_refusal(const Memory &mem, std::uint32_t obj, std::uint32_t state) {
  const auto table = mem.u32(obj + O_VTABLE);
  if (table != VT_AUTOPROP && table != VT_AUTOPROP_LP)
    return "slot 18 of class table " + hex(table) +
           " was not run (research ran AutoProp and AutoPropLP only, docs/level-commands.md:169-170)";
  const auto n = static_cast<std::int32_t>(state);
  if (!(n < static_cast<std::int32_t>(mem.u32(obj + AP_STATES))))
    return std::nullopt; // nothing stored, nothing called (:135-136)
  if (n < 0)
    return "a negative state indexes below the clip table; not run (docs/level-commands.md:161)";
  if (n >= AP_TABLE_STATES)
    return "the state indexes the clip table past +0x178 (hazard, docs/level-commands.md:157-162)";
  if (mem.u32(obj + AP_KIND) & AK_DOOR)
    return "AutoDoor setPassable moves its level piece; world_movePiece was a stub (docs/level-commands.md:147, :167-168)";
  if (mem.u32(obj + AP_MODEL) == 0)
    return "no model block for model_buildHideMask (docs/level-commands.md:143)";
  const auto clip = mem.u32(obj + AP_ANIMS + 8u * std::uint32_t(n));
  if (clip != 0 && ((clip + 4u) & 3u))
    return "an unaligned clip traps on its length load (ref_level_commands.py:168-172)";
  const auto flags = mem.u32(obj + AP_STATE_FLAGS + 8u * std::uint32_t(n));
  if ((flags & SF_ACTION_AT_END) && !(flags & SF_DONE))
    return "ActionAtEnd with the done bit clear: the next AutoProp run would run the state's actions at the "
           "animation's end; Inferred, not run, and the run is not ported (docs/level-commands.md:153, :209-210; "
           "audit LC-6)";
  return std::nullopt;
}

bool autoprop_set_state(Memory &mem, std::uint32_t o, std::uint32_t state) {
  // AutoProp_setState 0x23d680 (:131-147, ref:189-227).
  if (const auto refused = autoprop_refusal(mem, o, state))
    throw std::logic_error("autoprop_set_state on a refused case: " + *refused);
  const auto n = static_cast<std::int32_t>(state);
  if (!(n < static_cast<std::int32_t>(mem.u32(o + AP_STATES))))
    return false;
  mem.w32(o + AP_STATE, std::uint32_t(n));
  mem.w32(o + AP_COUNTER, 0);
  mem.w64(o + AP_REACHED, 0);
  if (n != 0) {
    std::uint64_t bits = 0;
    for (std::int32_t k = 0; k <= n; ++k)
      bits |= sext32(1u << (std::uint32_t(k) & 31u));
    mem.w64(o + AP_REACHED, bits);
  }
  const auto mask = model_build_hide_mask(mem, mem.u32(o + AP_MODEL), sext32(mem.u32(o + AP_REACHED)));
  mem.w8(o + AP_TRACK_COUNT, 0);
  mem.w64(o + AP_MASK, mask);
  const auto clip = mem.u32(o + AP_ANIMS + 8u * std::uint32_t(n));
  if (clip != 0) {
    const bool looping = (mem.u32(o + AP_STATE_FLAGS + 8u * std::uint32_t(n)) & SF_LOOPING) != 0;
    anim_push(mem, o + AP_ANIM, clip, looping ? 3u : 6u);
    anim_jump_top_to_end(mem, o + AP_ANIM);
    const auto length = mem.u32(mem.u32(o + AP_ANIMS + 8u * mem.u32(o + AP_STATE)) + 4u);
    mem.w32(o + AP_COUNTER, mem.u32(o + AP_COUNTER) + length + 1u);
  }
  // Slot 19, AutoProp_setPassable 0x23d7ac (:147, ref:229-250); not an AutoDoor.
  const bool on = (mem.u32(o + AP_STATE_FLAGS + 8u * mem.u32(o + AP_STATE)) & SF_PASSABLE) != 0;
  const auto flags = mem.u32(o + O_FLAGS);
  mem.w32(o + O_FLAGS, on ? flags | OF_NOCOLLIDE : flags & ~OF_NOCOLLIDE);
  return true;
}

bool arena_gate_refuses(const Memory &mem) {
  // 0x10a9c0 for state 0x14 (:254-257, ref:380-397).
  return mem.u32(G_ARENA) != 0 && mem.u32(G_ARENA_B) == 0 && mem.u32(G_GATE_C) != 0 && mem.u32(G_GATE_D) == 0;
}

std::uint32_t game_object_rand(Memory &mem, std::uint32_t o) {
  const std::uint32_t value = mem.u32(o + C_RAND) * 0x19660du + 0x3c6ef35fu;
  mem.w32(o + C_RAND, value);
  return value >> 16;
}

std::optional<std::string> gnome_kill_refusal(const Memory &mem, std::uint32_t o, const Death &death) {
  const auto table = mem.u32(o + O_VTABLE);
  if (table != VT_GNOME)
    return "slot 27 of class table " + hex(table) +
           " is Unknown (research ran Gnome only, docs/level-commands.md:321-322)";
  if (arena_gate_refuses(mem))
    return std::nullopt; // the arena-only refusal: only the handler's stores remain (:254-257)
  if (mem.u32(o + C_CLASS) == 0)
    return "no class definition block for the death animation (docs/level-commands.md:261)";
  if (death.drop_item)
    return "a dropitem calls loot_spawnNamedItem, a stub in the research (docs/level-commands.md:269-270)";
  if (mem.u32(o + O_FLAGS) & OF_DELETE_ON_DEATH)
    return "object flag 0x2 calls obj_delete, a stub in the research (docs/level-commands.md:271-272)";
  if (mem.u32(G_ARENA) != 0)
    return "the arena flag calls arena_addKill, a stub in the research (docs/level-commands.md:273)";
  if (death.boss_music)
    return "a boss-music name queues a stream-2 stop, a stub in the research (docs/level-commands.md:274-275)";
  return std::nullopt;
}

int gnome_set_state(Memory &mem, std::uint32_t o, const Death &death,
                    const std::function<void(const std::string &)> &run_death_script) {
  if (const auto refused = gnome_kill_refusal(mem, o, death))
    throw std::logic_error("gnome_set_state on a refused case: " + *refused);
  // 1. The state gate (:254-258, ref:380-397).
  if (arena_gate_refuses(mem))
    return 1;
  for (const auto field : {C_STUN, C_ATTACK, C_ROOT, C_T31A})
    mem.w16(o + field, 0);
  if (mem.u32(o + C_TARGET) == 0) // Creature_isAttackReady 0x1195f0
    mem.w16(o + C_ATTACK, 0);
  // 2-3. One own draw picks death animation (draw & 3) (:259-262, ref:412-416).
  const auto pick = game_object_rand(mem, o) & 3u;
  const auto clip = mem.u32(mem.u32(o + C_CLASS) + CLASS_DEATH_ANIMS + 8u * pick);
  anim_start(mem, o + C_ANIM, clip, 6, F_0_1);
  // 4. Creature_onDeath(0) (:263-275, ref:433-462).
  mem.w64(o + C_FLAGS64, mem.u64(o + C_FLAGS64) & DEATH_CFLAGS_KEEP);
  if (death.script) {
    mem.w32(G_SELECTED_OBJ, o);
    run_death_script(*death.script);
    mem.w32(G_SELECTED_OBJ, 0);
  }
  // 5. AI state 0x14 (:276).
  mem.w16(o + C_AI_STATE, STATE_DEAD);
  return 0;
}
} // namespace champions::simulation::rta::level_commands
