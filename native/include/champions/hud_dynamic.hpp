#pragma once
#include "champions/core_math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace champions::hud_dynamic {
using Raw = simulation::rta::Raw;
struct Sprite {
  std::string resource;
  int x{}, y{}, layer{};
  std::uint32_t tint{};
  // Clipped sprites (0x17fb88). clipped is false for a plain sprite. An edge
  // the research does not give stays empty rather than 0.
  bool clipped = false;
  std::optional<int> clip_left{}, clip_top{};
  int clip_right = 0;
  std::optional<int> clip_bottom{};
  // False when only the alpha byte of tint is documented (RGB unknown).
  bool tint_rgb_known = true;
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
  // Nonzero values mark one text_begin/text_end packet. Painter order keeps
  // those records forward; each zero is its own packet.
  int batch = 0;
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

// Round 3 (research t3/research-ui 1ed740a, tools/ref_hud_dynamic.py).
// These functions are the documented arithmetic. They do not load art.
// minimapProjector, compassMesh, arenaHermitePoint, textureFlag0x1000 and
// fontHeaderPlus2 throw: those meanings are still undocumented.
[[noreturn]] void minimapProjector();
[[noreturn]] void compassMesh();
[[noreturn]] void arenaHermitePoint();
[[noreturn]] void textureFlag0x1000();
[[noreturn]] void fontHeaderPlus2();

Raw bustFadeStep(Raw fade, bool hide);
std::pair<std::uint32_t, std::uint32_t> bustAlphas(Raw fade);
struct BustMeter {
  std::string resource;
  int clip_right = 0;
};
BustMeter bustMeter(int value, int maximum, int width, int tick);
// hud_drawBust's submissions are champions::ui_text_gaps::hudDrawBust (research
// t3/research-ui-text-gaps docs/ui-text-gaps.md section 3); the old bustPanel
// refused a maximum and guessed the font, so it is gone.

int flyingTextLife(int kind);
int flyingTextDrift(std::uint32_t rand_word);
struct FlyingStep {
  std::uint16_t life = 0, alpha = 0;
  bool erase = false;
};
// One simulation tick of FlyingText_run. Drawing does not call this.
FlyingStep flyingTextStep(std::uint16_t life, std::uint16_t alpha);
struct FlyingStyle {
  std::uint32_t colour = 0;
  bool force_x = false;
  Raw lift = 0;
};
FlyingStyle flyingTextStyle(int kind, bool crit, int alpha);
Raw flyingTextAnchorZ(Raw world_z, Raw extra);
// Local draw. x_off/y_off are the raw bit patterns stored at +0x78/+0x7c
// (a negative offset is the low 16 bits sign-extended by the caller to 32).
std::pair<std::uint32_t, std::uint32_t> flyingTextScreen(std::uint32_t x_off,
                                                         std::uint32_t y_off,
                                                         Raw clip_x, Raw clip_y,
                                                         Raw w);
// Simulated lifetime plus the local draw inputs. step() is the only mutator
// of life and alpha. draw() does not write them; when the style forces x it
// reports 5 without changing the simulated drift.
struct FlyingNumber {
  int kind = 0;
  bool crit = false;
  std::uint16_t life = 0;
  std::uint16_t alpha = 0x80;
  std::uint32_t x_off = 0, y_off = 0;
  Raw world_z = 0;
  std::string text;
  bool alive = true;
  static FlyingNumber make(int kind, bool crit, std::uint32_t rand_word,
                           Raw world_z, std::string text = {});
  bool step();
  struct Draw {
    std::uint32_t x = 0, y = 0, colour = 0;
    int layer = 5;
    Raw anchor_z = 0;
    std::uint32_t x_off = 0;
  };
  Draw draw(Raw clip_x, Raw clip_y, Raw w) const;
};

struct MinimapParams {
  std::array<Raw, 5> words{};
};
MinimapParams minimapParams(int mode, int players);
std::pair<Raw, Raw> minimapExtra(int mode);
Raw minimapApproach(Raw current, Raw target);
Raw minimapZoom(Raw scale, Raw stick);
std::optional<std::array<int, 3>> minimapPlayerBlip(int kind);
bool minimapPlayerDrawn(int health, std::uint32_t sim_frame);
// Non-player arm of minimap_blipColor. scale is set only for the grey arm.
struct ObjectBlip {
  std::array<int, 3> rgb{};
  std::optional<Raw> scale;
};
ObjectBlip minimapObjectBlip(std::uint32_t flags_c, std::uint64_t flags_a8);

std::array<Raw, 3> mat4Xyz(const std::array<Raw, 16> &m,
                           const std::array<Raw, 3> &v);
std::array<Raw, 3> compassAnchor(const std::array<Raw, 16> &view,
                                 const std::array<Raw, 3> &camera);
struct CompassLabel {
  int x = 0, y = 0, width = 0, top = 0, bottom = 0, line = 0;
  std::uint32_t colour = 0;
};
CompassLabel compassLabel();
// The label call. Mesh drawing is compassMesh(). text is caller-owned.
Text compassLabelText(const std::string &text);

int helpButtonAlpha(int close_delay);
std::uint32_t helpButtonColour(int close_delay);
struct HelpBox {
  int button_x = 0, button_y = 0, label_x = 0, label_y = 0;
  int frame_x = 0x74, frame_y = 0x5e, frame_width = 0x1a0, frame_height = 0;
  int inner_x = 0x7c, inner_y = 0x68, inner_width = 0x1a0, inner_height = 0;
  int title_x = 0x144, title_y = 0x64, body_x = 0x84, body_y = 0x82;
};
HelpBox helpBox(int wrap_height, int label_width);
int noticeColumn(int word);
struct NoticeBox {
  int title_x = 0, title_y = 0;
  int body_x = 0, body_y = 0, body_width = 0, body_top = 0, body_bottom = 0,
      body_line = 0;
  int button_x = 0, button_y = 0, label_x = 0, label_y = 0;
  int frame_x = 0, frame_y = 0, frame_width = 0, frame_height = 0;
  int rect_x0 = 0, rect_y0 = 0, rect_x1 = 0, rect_y1 = 0;
};
NoticeBox noticeBox(int column, int wrap_height);

struct MessagePlacement {
  std::uint32_t x = 0, y = 0, width = 0, layer = 0;
};
MessagePlacement messageBoxPlacement(std::int64_t t0, std::int64_t t2,
                                     std::int64_t t3, std::int64_t stack0);
struct MedalPrompt {
  bool mission_menu = false;
  std::string title_key, body_key;
  MessagePlacement box;
  int list_x = 0, list_y = 0;
};
std::optional<MedalPrompt> medalPrompt(bool pause_menu, bool armed,
                                       bool mission_menu, bool network,
                                       bool first_active);
bool netCamShow(bool network, bool other_slot, bool health_zero,
                bool has_object, int countdown, int alive);
struct NetCamText {
  int name_x = 0, name_y = 0, prompt_x = 0, prompt_y = 0;
  std::uint32_t prompt_colour = 0;
};
NetCamText netCamText();
int netCamPad(int slot);

bool arenaPlayerActive(Raw timer);
bool arenaStepShown(Raw timer, int step);
std::pair<int, int> arenaPulseArgs(int step);
} // namespace champions::hud_dynamic
