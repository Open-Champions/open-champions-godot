#pragma once
#include "champions/core_math.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace champions::hud_dynamic {
using Raw = simulation::rta::Raw;
struct Sprite {
  std::string resource;
  int x{}, y{}, layer{};
  std::uint32_t tint{};
};
struct Rect {
  int x0{}, y0{}, x1{}, y1{}, layer{};
  std::uint32_t colour{};
};
struct Text {
  std::string kind;
  int x{}, y{}, layer{};
  std::uint32_t colour{};
  std::string text;
};
struct Frame {
  int x{}, y{}, width{}, height{}, layer{};
  std::uint32_t tint{};
  int style{};
};
using Record = std::variant<Sprite, Rect, Text, Frame>;
using Records = std::vector<Record>;
// Submission stages of HUD Draw. Gated stages are represented so callers keep
// the original order while supplying their own records only after research.
enum class Stage {
  Gold,
  Status,
  Arena,
  MessageBoss,
  Bust,
  Compass,
  NetCamPrompt,
  ControllerOut,
  MedalPrompt,
  NetStatus
};
const std::array<Stage, 10> &drawSequence();
const char *stageName(Stage);
const std::array<const char *, 11> &statusSequence();
using StageRecords = std::array<Records, 10>;
Records composeHud(const StageRecords &); // submissions in draw-task order
Records composeStatus(const std::array<Records, 11> &); // one slot
std::vector<std::size_t> painterIndices(const Records &);
int pulse(std::uint32_t tick);
std::uint32_t pendingLevelColour(int pulse);
std::uint32_t nameColour(int slot, int tick, std::uint32_t arena_colour = 0,
                         bool has_player_colour = false,
                         std::uint32_t player_colour = 0,
                         std::uint32_t blink_mask = 0);
int nameWidthBudget(bool pvp_a, bool pvp_b, int counter_width);
std::uint32_t pulseGrey(int pulse, int alpha);
int glowStep(int alpha);
Sprite glowSprite(int x, int alpha, int pulse);
int noticeBaseX(int slot, bool network = false);
int noticeAlphaStep(int timer, int alpha);
struct NoticeFrame {
  Records submissions;
  std::array<int, 8> timers{}, alphas{};
  std::vector<int> cleared;
};
NoticeFrame noticeFrame(int slot, std::array<int, 8> timers,
                        std::array<int, 8> alphas,
                        const std::array<int, 8> &counts, std::uint32_t tick,
                        bool network = false, bool hide_counts = false);
int countActiveNotices(const std::array<int, 8> &timers, int except_index);
int messageAlphaStep(int timer, int alpha);
struct MessageFrame {
  Records submissions;
  int alpha{};
};
MessageFrame messageFrame(int timer, int alpha, bool force, bool suppressed,
                          int text_width);
struct BossState {
  int hp{}, maximum{}, alpha{}, visible{}, previous{};
};
struct BossFrame {
  Records submissions;
  BossState state;
};
BossFrame bossMeterFrame(BossState, int language = 0);
struct GoldFloater {
  std::array<Raw, 3> position{}, velocity{};
};
GoldFloater goldFloaterStep(const GoldFloater &, int slot);
int goldFloaterAlpha(int tick, int start);
std::pair<int, int> goldCounterStep(int hold, int alpha);
Records goldCounter(int slot, int alpha, int text_width);
std::pair<int, int> selectorCentre(int slot, bool network = false,
                                   bool menu_mode = false);
int selectorDirection(int dx, int dy);
struct SelectorFade {
  int alpha{};
  Raw timer{};
  int speed{};
};
SelectorFade selectorFadeStep(SelectorFade, bool active);
std::pair<int, int> selectorRadii(int alpha);
struct SelectorIcon {
  int index{}, x{}, y{};
};
std::array<SelectorIcon, 8> selectorRing(int cx, int cy, int alpha,
                                         int page = 0);
std::pair<std::uint32_t, std::uint32_t> selectorHighlight(std::uint32_t tick,
                                                          int alpha);
} // namespace champions::hud_dynamic
