#include "champions/script_effects.hpp"
#include "champions/amx.hpp"
#include "champions/core_arithmetic.hpp"

namespace champions::simulation::rta::script_effects {
namespace {
using champions::simulation::rta::Fpu;
const Fpu kFpu(Fpu::Model::Ps2);

std::uint32_t trunc_half(builtins::Memory &memory, std::uint32_t address, std::uint32_t scale,
                         bool floor) {
  const auto n = Fpu::to_int(kFpu.mul(kFpu.from_int(static_cast<std::uint32_t>(memory.s16(address))), scale));
  auto low = static_cast<std::uint32_t>(n) & 0xffffu;
  if (floor && (low == 0 || low >= 0x8000u))
    low = 1;
  return low;
}
std::uint32_t trunc_float(std::uint32_t pattern, std::uint32_t scale) {
  return kFpu.from_int(static_cast<std::uint32_t>(Fpu::to_int(kFpu.mul(pattern, scale))));
}
} // namespace

Effects::Effects(builtins::Memory &image) : memory(image) {}

std::uint32_t Effects::reply(const std::string &name, const std::vector<std::uint32_t> &args,
                             const std::vector<std::uint32_t> &) {
  if (name == "net_isLocalSlot" && local_slot)
    return local_slot(args.empty() ? 0u : args[0]) ? 1u : 0u;
  const auto stub = stubs.find(name);
  if (stub != stubs.end())
    return stub->second;
  if (discarded.count(name))
    return 0;
  if (default_return)
    return *default_return;
  throw amx::HostFault(name + " was not entered; its return is not in the contract");
}

std::uint32_t Effects::call(const std::string &name, const std::vector<std::uint32_t> &args,
                            const std::vector<std::uint32_t> &fargs) {
  calls.push_back(Call{name, args, fargs});
  if (name == "ui_drawRect" && args.size() >= 6)
    rects.push_back(Rect{static_cast<int>(args[0]), static_cast<int>(args[1]),
                         static_cast<int>(args[2]), static_cast<int>(args[3]),
                         static_cast<int>(args[4]), args[5]});
  return reply(name, args, fargs);
}

std::uint32_t Effects::scale_stats(std::uint32_t creature, const std::uint32_t scales[5]) {
  const auto hp = scales[0], armour = scales[1], damage = scales[2], rating = scales[3], xp = scales[4];
  memory.w16(creature + 0xe4, trunc_half(memory, creature + 0xe4, armour, true));
  memory.w32(creature + 0x228, trunc_float(memory.u32(creature + 0x228), hp));
  memory.w32(creature + 0x22c, trunc_float(memory.u32(creature + 0x22c), hp));
  memory.w16(creature + 0xd0, trunc_half(memory, creature + 0xd0, damage, true));
  memory.w16(creature + 0xd2, trunc_half(memory, creature + 0xd2, damage, true));
  memory.w16(creature + 0xd4, trunc_half(memory, creature + 0xd4, rating, false));
  memory.w32(creature + 0xd8, trunc_float(memory.u32(creature + 0xd8), xp));
  return 0;
}

std::uint32_t Effects::set_charm(std::uint32_t creature, std::uint32_t frames, std::uint32_t team) {
  if (memory.s16(creature + 0x312) == -1)
    return 0xffffffffu;
  const auto flags = memory.u32(creature + 0xc);
  if ((flags & 0x20u) == 0)
    return 0;
  if (flags & 0x40u)
    return flags & 0x20u;
  memory.w8(creature + 0x310, team & 0xffu);
  memory.w16(creature + 0x312, frames & 0xffffu);
  // handle_clear of a null target stores nothing (executed). A live handle is
  // research's Unknown; the script host refuses before it gets here.
  const auto ret = call("handle_clear", {creature + 0x234, 0xffffffffu});
  memory.w32(creature + 0x234, 0);
  return ret;
}

void Effects::slot27_set_state(std::uint32_t creature, std::uint32_t state) {
  memory.w16(creature + 0x232, state & 0xffffu);
}

void Effects::slot34_set_home_range(std::uint32_t creature, std::uint32_t value) {
  memory.w32(creature + 0x3c0, kFpu.from_int(value & 0xffffu));
}

std::uint32_t Effects::show_message(std::uint32_t text, std::uint32_t frames, std::uint32_t slot,
                                   std::uint32_t force) {
  const auto force_byte = static_cast<std::uint32_t>(static_cast<std::int8_t>(force & 0xffu));
  if (static_cast<std::int32_t>(slot) != -1 && call("net_isLocalSlot", {slot}) == 0)
    return 0;
  const auto menu = static_cast<std::uint32_t>(memory.s8(G_LOCAL_MENU)) & 0xffu;
  if (force_byte == 0 && menu != 0)
    return menu;
  call("wstr_copy", {G_MESSAGE_TEXT, text});
  memory.w16(G_MESSAGE_FRAMES, frames & 0xffffu);
  memory.w8(G_MESSAGE_FORCE, force_byte & 0xffu);
  return 0x4f0000u;
}

std::uint32_t Effects::show_help(std::uint32_t slot, std::uint32_t title, std::uint32_t body) {
  if (static_cast<std::int32_t>(slot) != -1 && call("net_isLocalSlot", {slot}) == 0)
    return 0;
  bool opened = memory.s8(G_NETWORK_MODE) == 0;
  if (!opened) {
    const auto count = memory.u32(G_ALIVE_COUNT);
    for (std::uint32_t index = 0; index < count; ++index) {
      const auto live = memory.u32(G_ALIVE_LIST + 4 * index);
      if (call("net_isLocalSlot", {live})) {
        call("hudmsg_sendMenuState", {live & 0xffu, 1u, 0u});
        opened = true;
        break;
      }
    }
  }
  if (!opened)
    return (memory.u32(G_ALIVE_COUNT) << 2) & 0xffffffffu;
  const auto found = call("res_find", {HELP_RES_LUMP, HELP_RES_NAME});
  const auto handle = call("sound_playOnObject", {found, 0u}, {0x3f800000u});
  call("sound_setPlaysWhileFrozen", {handle});
  call("ui_startPulse", {0u});
  memory.w8(G_HELP_OPEN, 1);
  memory.w32(G_HELP_TITLE, title);
  memory.w32(G_HELP_BODY, body);
  memory.w32(G_HELP_DELAY, 90);
  return 90;
}

std::optional<std::uint32_t> Effects::hud_blocked() {
  const auto suppressed = static_cast<std::uint32_t>(memory.s8(G_HUD_SUPPRESSED)) & 0xffu;
  if (suppressed)
    return suppressed;
  const auto menu = static_cast<std::uint32_t>(memory.s8(G_LOCAL_MENU)) & 0xffu;
  if (menu)
    return menu;
  const auto camera = memory.u32(G_CAMERA);
  if (camera == 0)
    throw amx::HostFault("hud setter has a null camera");
  const auto mode = (memory.u32(camera + 0x34) ^ 1u) & 0xffffffffu;
  if (mode)
    return mode;
  const auto loading = static_cast<std::uint32_t>(memory.s8(G_LOADSCREEN)) & 0xffu;
  if (loading)
    return loading;
  const auto flag = memory.u32(G_MENU_OPEN_FLAG);
  if (flag)
    return flag;
  return std::nullopt;
}

std::uint32_t Effects::set_compass(const std::uint32_t pos[3], std::uint32_t text) {
  if (const auto blocked = hud_blocked())
    return *blocked;
  memory.w32(G_COMPASS_POS, pos[0]);
  memory.w32(G_COMPASS_POS + 4, pos[1]);
  memory.w32(G_COMPASS_POS + 8, pos[2]);
  call("wstr_copy", {G_COMPASS_TEXT, text});
  memory.w8(G_COMPASS_COUNTER, 5);
  return 0x870000u;
}

std::uint32_t Effects::set_bust(std::uint32_t texture, std::uint32_t text, std::uint32_t left,
                               std::uint32_t right) {
  if (const auto blocked = hud_blocked())
    return *blocked;
  call("strcpy", {G_BUST_TEXTURE, texture});
  call("wstr_copy", {G_BUST_TEXT, text});
  memory.w8(G_BUST_COUNTER, 5);
  memory.w32(G_BUST_A, left);
  memory.w32(G_BUST_B, right);
  return 0x870000u;
}

std::optional<std::uint32_t> Effects::set_fade(std::uint32_t on) {
  const bool enable = static_cast<std::int32_t>(on) != 0;
  memory.w32(G_FADE, enable ? 128u : 0u);
  if (!enable)
    return std::nullopt;
  return 128u;
}

std::uint32_t Effects::step_fade(std::uint32_t delta) {
  auto value = static_cast<std::int64_t>(static_cast<std::int32_t>(memory.u32(G_FADE))) +
               static_cast<std::int32_t>(delta);
  if (value < 0)
    value = 0;
  else if (value > 128)
    value = 128;
  memory.w32(G_FADE, static_cast<std::uint32_t>(value));
  if (value != 0)
    call("ui_drawRect", {0u, 0u, 640u, 448u, 6u, static_cast<std::uint32_t>(value) << 24});
  return static_cast<std::uint32_t>(value);
}

std::uint32_t Effects::set_plays_while_frozen(std::uint32_t handle) {
  const auto signed_handle = static_cast<std::int32_t>(handle);
  if (signed_handle == -1 || (handle & 0x8000u) == 0)
    return 0xffffffffu;
  const auto slot = handle & 0xffu;
  const auto offset = slot * 0x4cu;
  memory.w8(G_SOUND_SEQS + offset + 0x3b, 1);
  return offset;
}

std::uint32_t Effects::queue_stop(std::uint32_t stream) {
  stream &= 0xffffffffu;
  auto scaled = (((stream * 7u) & 0xffffffffu) << 4) & 0xffffffffu;
  scaled = (scaled + stream) & 0xffffffffu;
  const auto stride = (scaled << 2) & 0xffffffffu;
  const auto count_at = (G_STREAMS + stride + 448u) & 0xffffffffu;
  auto count = memory.u32(count_at);
  if (count >= 8)
    return (count * 3u) & 0xffffffffu;
  const auto base = (G_STREAMS + stride) & 0xffffffffu;
  const auto entry = (base + (count * 48u)) & 0xffffffffu;
  memory.w8(entry + 64, 0);
  memory.w32(entry + 0x6c, 2);
  memory.w32(entry + 80, 0);
  memory.w32(entry + 88, 0);
  memory.w32(entry + 92, 0);
  memory.w32(entry + 84, 0);
  count = (count + 1u) & 0xffffffffu;
  memory.w32(count_at, count);
  return count;
}

std::uint32_t Effects::music_play_level(std::uint32_t name, std::uint32_t flags) {
  channel_volume.reset();
  const auto volume = memory.u32(G_MUSIC_VOLUME);
  if (Fpu::compare(volume, MUSIC_GATE) < 0)
    return 0x500000u;
  const auto level = kFpu.mul(kFpu.mul(memory.u32(G_MUSIC_SCALAR), volume), MUSIC_SCALE);
  channel_volume = level;
  call("audio_setChannelVolume", {2u}, {level});
  return call("stream_playLevel", {2u, name, (flags | 0x22u) & 0xffffffffu});
}

} // namespace champions::simulation::rta::script_effects
