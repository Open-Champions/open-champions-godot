#pragma once
// Callees of the level-script commands whose bodies research executed.
// Research: t3/research-script 52984db, tools/ref_script_effects.py and
// docs/script-effects.md. None of these bodies draw the shared seed.
#include "champions/script_builtins.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace champions::simulation::rta::script_effects {
namespace builtins = champions::simulation::rta::builtins;

inline constexpr std::uint32_t G_LOCAL_MENU = 0x41b69d;
inline constexpr std::uint32_t G_HUD_SUPPRESSED = 0x4f66a8;
inline constexpr std::uint32_t G_LOADSCREEN = 0x506094;
inline constexpr std::uint32_t G_CAMERA = 0x4f8450;
inline constexpr std::uint32_t G_MESSAGE_TEXT = 0x86ab78;
inline constexpr std::uint32_t G_MESSAGE_FRAMES = 0x4f66e0;
inline constexpr std::uint32_t G_MESSAGE_FORCE = 0x4f66e2;
inline constexpr std::uint32_t G_HELP_TITLE = 0x86aa64;
inline constexpr std::uint32_t G_HELP_BODY = 0x86aa68;
inline constexpr std::uint32_t G_HELP_OPEN = 0x41b928;
inline constexpr std::uint32_t G_HELP_DELAY = 0x4f66cc;
inline constexpr std::uint32_t G_COMPASS_POS = 0x86a8f0;
inline constexpr std::uint32_t G_COMPASS_COUNTER = 0x4f66d5;
inline constexpr std::uint32_t G_COMPASS_TEXT = 0x86aaf0;
inline constexpr std::uint32_t G_BUST_TEXTURE = 0x4f6b48;
inline constexpr std::uint32_t G_BUST_TEXT = 0x86aab0;
inline constexpr std::uint32_t G_BUST_COUNTER = 0x4f66d4;
inline constexpr std::uint32_t G_BUST_A = 0x86ab30;
inline constexpr std::uint32_t G_BUST_B = 0x86ab34;
inline constexpr std::uint32_t G_FADE = 0x4f66ac;
inline constexpr std::uint32_t G_MUSIC_VOLUME = 0x4f8944;
inline constexpr std::uint32_t G_MUSIC_SCALAR = 0x4f894c;
inline constexpr std::uint32_t G_SOUND_SEQS = 0x871a68;
inline constexpr std::uint32_t G_STREAMS = 0x876c10;
inline constexpr std::uint32_t G_NETWORK_MODE = 0x402791;
inline constexpr std::uint32_t G_ALIVE_COUNT = 0x4f8420;
inline constexpr std::uint32_t G_ALIVE_LIST = 0x4f8440;
inline constexpr std::uint32_t G_MENU_OPEN_FLAG = 0x41b4f8;
inline constexpr std::uint32_t MUSIC_GATE = 0x3d4ccccc;
inline constexpr std::uint32_t MUSIC_SCALE = 0x3f480000;
inline constexpr std::uint32_t HELP_RES_LUMP = 0x53c3b8;
inline constexpr std::uint32_t HELP_RES_NAME = 0x53c3c0;
inline constexpr std::uint32_t SCALE_DIVISOR = 0x3c23d70a; // 0.01

struct Call {
  std::string name;
  std::vector<std::uint32_t> args;
  std::vector<std::uint32_t> fargs;
};

struct Rect {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0, layer = 0;
  std::uint32_t colour = 0;
};

// One memory image and the calls a body makes. A call is recorded and
// answered; its own body does not run here. That includes wstr_copy, strcpy
// and sound_setPlaysWhileFrozen, which the research difftest stubs inside
// these bodies, so no string is copied and no string-length bound is assumed.
//
// A call's answer is, in order: local_slot for net_isLocalSlot, a named stub,
// a name in discarded (answered 0), then default_return. With none of these
// the call throws HostFault: research did not enter that callee, and its
// return would reach the caller. The vector replay sets default_return to 0,
// the default of ref_script_effects.Trace.call. The script host sets only
// discarded, for calls whose answer no script handler reads.
class Effects {
public:
  explicit Effects(builtins::Memory &memory);
  builtins::Memory &memory;
  std::vector<Call> calls;
  std::vector<Rect> rects;
  std::function<std::uint32_t(std::uint32_t slot)> local_slot;
  std::map<std::string, std::uint32_t> stubs;
  std::set<std::string> discarded;
  std::optional<std::uint32_t> default_return;
  // Set when music_play_level passes the volume gate. Absent when it does not.
  std::optional<std::uint32_t> channel_volume;

  std::uint32_t reply(const std::string &name, const std::vector<std::uint32_t> &args,
                      const std::vector<std::uint32_t> &fargs = {});

  std::uint32_t scale_stats(std::uint32_t creature, const std::uint32_t scales[5]);
  std::uint32_t set_charm(std::uint32_t creature, std::uint32_t frames, std::uint32_t team);
  void slot27_set_state(std::uint32_t creature, std::uint32_t state);
  void slot34_set_home_range(std::uint32_t creature, std::uint32_t value);
  std::uint32_t show_message(std::uint32_t text, std::uint32_t frames, std::uint32_t slot,
                             std::uint32_t force);
  std::uint32_t show_help(std::uint32_t slot, std::uint32_t title, std::uint32_t body);
  // nullopt means the setter runs. A null camera throws HostFault.
  std::optional<std::uint32_t> hud_blocked();
  std::uint32_t set_compass(const std::uint32_t pos[3], std::uint32_t text);
  std::uint32_t set_bust(std::uint32_t texture, std::uint32_t text, std::uint32_t left,
                         std::uint32_t right);
  // nullopt: turning the fade off leaves v0 unchanged. Otherwise 128.
  std::optional<std::uint32_t> set_fade(std::uint32_t on);
  std::uint32_t step_fade(std::uint32_t delta);
  std::uint32_t set_plays_while_frozen(std::uint32_t handle);
  std::uint32_t queue_stop(std::uint32_t stream);
  // Shut gate returns 0x500000 and makes no call. Otherwise the channel word
  // is scalar * volume * 0x3f480000 and stream 2 is queued with flags|0x22.
  std::uint32_t music_play_level(std::uint32_t name, std::uint32_t flags);

private:
  std::uint32_t call(const std::string &name, const std::vector<std::uint32_t> &args,
                     const std::vector<std::uint32_t> &fargs = {});
};
} // namespace champions::simulation::rta::script_effects
