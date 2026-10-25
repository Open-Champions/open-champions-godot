#include "champions/hud_dynamic.hpp"
#include <climits>
#include <stdexcept>

namespace champions::hud_dynamic {
namespace F = simulation::rta;
namespace {
[[noreturn]] void undocumented(const char *what) {
  throw std::runtime_error(what);
}
int checked(long long n) {
  if (n < INT_MIN || n > INT_MAX)
    throw std::out_of_range("HUD coordinate overflow");
  return static_cast<int>(n);
}
std::int32_t as32(long long n, const char *what) {
  if (n < INT32_MIN || n > INT32_MAX)
    throw std::out_of_range(what);
  return static_cast<std::int32_t>(n);
}
std::uint32_t low32(std::int64_t v) { return static_cast<std::uint32_t>(v); }
std::uint32_t addu(std::uint32_t a, std::uint32_t b) { return a + b; }
int sx16(unsigned v) {
  v &= 0xffffu;
  return (v & 0x8000u) ? static_cast<int>(v) - 0x10000 : static_cast<int>(v);
}
void modeRange(int mode) {
  if (mode < 0 || mode > 2)
    throw std::out_of_range("minimap mode outside 0..2");
}
Raw row4(const std::array<Raw, 16> &m, int row, const std::array<Raw, 3> &v) {
  Raw s = F::mul(m[static_cast<std::size_t>(row)], v[0]);
  s = F::add(s, F::mul(m[static_cast<std::size_t>(row + 1)], v[1]));
  s = F::add(s, F::mul(m[static_cast<std::size_t>(row + 2)], v[2]));
  return F::add(s, m[static_cast<std::size_t>(row + 3)]);
}
// Byte-offset tables of minimap_applyMode. Keys are the documented offsets.
Raw minimapA(int offset) {
  switch (offset) {
  case 0:
  case 4:
  case 24:
  case 28:
  case 0x38:
  case 0x3c:
    return 0;
  case 8:
    return 0x43480000;
  case 12:
  case 20:
    return 0xc2f00000;
  case 16:
    return 0x43340000;
  case 32:
  case 40:
    return 0xc3700000;
  case 36:
  case 44:
    return 0x430c0000;
  case 0x30:
    return 0x3f666666;
  case 0x34:
    return 0x3f333333;
  default:
    throw std::out_of_range("minimap offset");
  }
}
Raw minimapB(int offset) {
  switch (offset) {
  case 0:
  case 24:
    return 0x43a00000;
  case 4:
  case 28:
    return 0x43600000;
  case 8:
  case 32:
    return 0x42f00000;
  case 12:
  case 36:
    return 0x42c80000;
  case 16:
  case 20:
  case 40:
  case 44:
    return 0;
  default:
    throw std::out_of_range("minimap offset");
  }
}
} // namespace

void minimapProjector() {
  undocumented("minimap projector 0x168cac is undocumented");
}
void compassMesh() { undocumented("compass mesh extents are undocumented"); }
void arenaHermitePoint() {
  undocumented("arena hermite point is undocumented");
}
void textureFlag0x1000() { undocumented("TEX flag 0x1000 has no reader"); }
void fontHeaderPlus2() { undocumented("FNT header +2 is not interpreted"); }

Raw bustFadeStep(Raw fade, bool hide) {
  fade = hide ? F::sub(fade, 0x3dacf3df) : F::add(fade, 0x3dacf3df);
  if (F::lt(F::ONE, fade))
    fade = F::ONE;
  if (F::lt(fade, F::ZERO))
    fade = F::ZERO;
  return fade;
}
std::pair<std::uint32_t, std::uint32_t> bustAlphas(Raw fade) {
  return {F::to_uint(F::mul(fade, 0x42dc0000)),
          F::to_uint(F::mul(fade, 0x43000000))};
}
BustMeter bustMeter(int value, int maximum, int width, int tick) {
  const int q = tick / 2;
  const int n = q - (q / 4) * 4;
  const Raw fill = F::from_int(static_cast<std::uint32_t>(as32(value, "bust value")));
  const Raw span = F::from_int(static_cast<std::uint32_t>(as32(maximum, "bust maximum")));
  const Raw wide = F::from_int(static_cast<std::uint32_t>(as32(width, "bust width")));
  const int rounded = F::round_half_away(F::mul(F::div(fill, span), wide));
  return {"bar_yellow0" + std::to_string(n) + ".tex", 521 + rounded - 2};
}
int flyingTextLife(int kind) { return kind ? 0xb4 : 0x1e; }
int flyingTextDrift(std::uint32_t rand_word) {
  return static_cast<int>(rand_word % 60u) - 30;
}
FlyingStep flyingTextStep(std::uint16_t life, std::uint16_t alpha) {
  int life_s = sx16(life);
  unsigned a = alpha;
  if (life_s < 31) {
    a = (a - 3u) & 0xffffu;
    if (sx16(a) < 0)
      a = 0;
  }
  life_s -= 1;
  return {static_cast<std::uint16_t>(life_s & 0xffff),
          static_cast<std::uint16_t>(a), life_s <= 0};
}
FlyingStyle flyingTextStyle(int kind, bool crit, int alpha) {
  const std::uint32_t a = (static_cast<std::uint32_t>(alpha) & 0xffu) << 24;
  if (kind == 1)
    return {a | 0x602060, true, 0x42700000};
  if (kind == 2 && !crit)
    return {a | 0x406e40, true, 0x41400000};
  if (crit)
    return {a | 0x008080, false, 0};
  return {a | 0x323c3c, false, 0};
}
Raw flyingTextAnchorZ(Raw world_z, Raw extra) {
  return F::add(F::add(world_z, 0x42900000), extra);
}
std::pair<std::uint32_t, std::uint32_t>
flyingTextScreen(std::uint32_t x_off, std::uint32_t y_off, Raw clip_x, Raw clip_y,
                 Raw w) {
  const Raw inv = F::div(F::ONE, w);
  const auto x = static_cast<std::uint32_t>(
      F::to_int(F::mul(F::mul(clip_x, inv), 0x3f000000)));
  const auto y = static_cast<std::uint32_t>(F::to_int(F::mul(clip_y, inv)));
  return {addu(addu(x_off, x), 0x140), addu(addu(y_off, y), 0xe0)};
}
FlyingNumber FlyingNumber::make(int kind, bool crit, std::uint32_t rand_word,
                                Raw world_z, std::string text) {
  FlyingNumber n;
  n.kind = kind;
  n.crit = crit;
  n.life = static_cast<std::uint16_t>(flyingTextLife(kind));
  n.alpha = 0x80;
  const auto drift = static_cast<std::uint32_t>(flyingTextDrift(rand_word));
  n.x_off = drift;
  n.y_off = drift;
  n.world_z = world_z;
  n.text = std::move(text);
  return n;
}
bool FlyingNumber::step() {
  const FlyingStep next = flyingTextStep(life, alpha);
  life = next.life;
  alpha = next.alpha;
  alive = !next.erase;
  return alive;
}
FlyingNumber::Draw FlyingNumber::draw(Raw clip_x, Raw clip_y, Raw w) const {
  const FlyingStyle style = flyingTextStyle(kind, crit, alpha);
  const std::uint32_t x = style.force_x ? 5u : x_off;
  const auto screen = flyingTextScreen(x, y_off, clip_x, clip_y, w);
  return {screen.first, screen.second, style.colour, 5,
          flyingTextAnchorZ(world_z, style.lift), x};
}

MinimapParams minimapParams(int mode, int players) {
  modeRange(mode);
  const bool single = players < 2;
  const int off_lo = (mode << 3) + (single ? 0 : 0x18);
  const int off_hi = (mode << 3) + (single ? 4 : 0x1c);
  return {{minimapA(0x30 + (mode << 2)), minimapA(off_lo), minimapA(off_hi),
           minimapB(off_lo), minimapB(off_hi)}};
}
std::pair<Raw, Raw> minimapExtra(int mode) {
  modeRange(mode);
  static const Raw f18[] = {0x41800000, 0x41200000, 0x41200000};
  static const Raw f17[] = {0x40c00000, 0x40a00000, 0x40a00000};
  return {f18[mode], f17[mode]};
}
Raw minimapApproach(Raw current, Raw target) {
  if (F::eq(current, target))
    return current;
  const Raw diff = F::sub(target, current);
  if (F::lt(diff & 0x7fffffffu, 0x3ba3d70a))
    return target;
  return F::add(current, F::mul(diff, 0x3e800000));
}
Raw minimapZoom(Raw scale, Raw stick) { return F::mul(scale, stick); }
std::optional<std::array<int, 3>> minimapPlayerBlip(int kind) {
  switch (kind) {
  case 0:
    return std::array<int, 3>{0xb6, 0x2f, 0x3a};
  case 1:
    return std::array<int, 3>{0x00, 0xbf, 0x00};
  case 2:
    return std::array<int, 3>{0xb4, 0xb2, 0x00};
  case 3:
    return std::array<int, 3>{0x1d, 0x20, 0xa1};
  default:
    return std::nullopt;
  }
}
bool minimapPlayerDrawn(int health, std::uint32_t sim_frame) {
  if (health <= 0 && (sim_frame & 0x20u) == 0)
    return false;
  return true;
}
ObjectBlip minimapObjectBlip(std::uint32_t flags_c, std::uint64_t flags_a8) {
  if ((flags_c & 0x28u) == 0x20u && (flags_a8 & (std::uint64_t{1} << 43)) != 0)
    return {{0xe7, 0xc1, 0x52}, std::nullopt};
  return {{0xc8, 0xc8, 0xc8}, Raw{0x3f000000}};
}

std::array<Raw, 3> mat4Xyz(const std::array<Raw, 16> &m,
                           const std::array<Raw, 3> &v) {
  return {row4(m, 0, v), row4(m, 4, v), row4(m, 8, v)};
}
std::array<Raw, 3> compassAnchor(const std::array<Raw, 16> &view,
                                 const std::array<Raw, 3> &camera) {
  const std::array<Raw, 3> offset{0xbde977c8, 0x3e20c38f, 0xc0b8273d};
  const auto p = mat4Xyz(view, offset);
  return {F::add(p[0], camera[0]), F::add(p[1], camera[1]),
          F::add(p[2], camera[2])};
}
CompassLabel compassLabel() { return {0x41, 0x18a, 0x83, -0x64, 0x1f4, 0x12, 0x632f6159}; }
Text compassLabelText(const std::string &text) {
  const CompassLabel label = compassLabel();
  return Text{"wrapped_centered", label.x, label.y, 5, label.colour, text, 0};
}

int helpButtonAlpha(int close_delay) {
  int a = 0x80 - close_delay * 10;
  if (a < 0)
    a = 0;
  if (a >= 0x81)
    a = 0x80;
  return a;
}
std::uint32_t helpButtonColour(int close_delay) {
  return (static_cast<std::uint32_t>(helpButtonAlpha(close_delay)) << 24) | 0x464646;
}
HelpBox helpBox(int wrap_height, int label_width) {
  const std::int32_t width = as32(label_width, "help label width");
  // Arithmetic shifts, matching the signed 32-bit halfWidth at the help call.
  const std::int32_t sign = width < 0 ? -1 : 0;
  const std::uint32_t widened = static_cast<std::uint32_t>(width + sign);
  const int half = static_cast<std::int32_t>(
      width < 0 ? ~(~widened >> 1) : widened >> 1);
  const int y = checked(static_cast<long long>(wrap_height) + 0x8b);
  HelpBox box;
  box.button_x = 0x144 - half - 0x19;
  box.button_y = y;
  box.label_x = 0x144 - half;
  box.label_y = y;
  box.frame_height = checked(static_cast<long long>(wrap_height) + 0x4c);
  // Corrected by research t3/research-ui-text-gaps (executed): the inner frame is as
  // tall as the outer frame's argument.
  box.inner_height = checked(static_cast<long long>(wrap_height) + 0x4c);
  return box;
}
int noticeColumn(int word) { return checked(static_cast<long long>(word) * 320); }
NoticeBox noticeBox(int column, int wrap_height) {
  NoticeBox b;
  b.title_x = checked(static_cast<long long>(column) + 0xa0);
  b.title_y = 0x64;
  b.body_x = checked(static_cast<long long>(column) + 0x24);
  b.body_y = 0x82;
  b.body_width = 0x100;
  b.body_top = 0;
  b.body_bottom = 0x1c0;
  b.body_line = 0x15;
  b.button_x = checked(static_cast<long long>(column) + 0x87);
  b.button_y = checked(static_cast<long long>(wrap_height) + 0x87);
  b.label_x = checked(static_cast<long long>(column) + 0xa0);
  b.label_y = b.button_y;
  b.frame_x = checked(static_cast<long long>(column) + 0x1a);
  b.frame_y = 0x5e;
  b.frame_width = 0x10c;
  b.frame_height = checked(static_cast<long long>(wrap_height) + 0x4c);
  b.rect_x0 = checked(static_cast<long long>(column) + 0x22);
  b.rect_y0 = 0x66;
  b.rect_x1 = checked(static_cast<long long>(column) + 0x12e);
  b.rect_y1 = checked(static_cast<long long>(wrap_height) + 0xb2);
  return b;
}

MessagePlacement messageBoxPlacement(std::int64_t t0_in, std::int64_t t2_in,
                                     std::int64_t t3_in, std::int64_t stack0_in) {
  const std::uint32_t t0 = low32(t0_in), t2 = low32(t2_in), t3 = low32(t3_in),
                      stack0 = low32(stack0_in);
  MessagePlacement p;
  p.width = t0 ? t0 : 0x208;
  p.x = addu(t2, 0x140);
  p.y = addu(stack0, 0xdc);
  p.layer = t3 == 0xffffffffu ? 6u : t3;
  return p;
}
std::optional<MedalPrompt> medalPrompt(bool pause_menu, bool armed,
                                       bool mission_menu, bool network,
                                       bool first_active) {
  if (pause_menu || !armed)
    return std::nullopt;
  MedalPrompt p;
  p.mission_menu = mission_menu;
  if (mission_menu) {
    p.title_key = "MedalRound";
    p.body_key = (network && !first_active) ? "MedalRoundPickNet" : "MedalRoundPick";
  }
  p.box = messageBoxPlacement(0, 0, -1, -0x1c);
  p.list_x = 0x140;
  p.list_y = 0xdc;
  return p;
}
bool netCamShow(bool network, bool other_slot, bool health_zero, bool has_object,
                int countdown, int alive) {
  if (!network)
    return false;
  bool show = other_slot;
  if (health_zero && (!has_object || countdown == 0) && alive != 0)
    show = true;
  return show;
}
NetCamText netCamText() { return {0x140, 0x160, 0x140, 0x174, 0x804d4d4d}; }
int netCamPad(int slot) {
  if (slot < 0)
    throw std::out_of_range("net camera slot");
  return checked(static_cast<long long>(slot) * 0x58);
}

bool arenaPlayerActive(Raw timer) { return F::lt(timer, 0x40000000); }
bool arenaStepShown(Raw timer, int step) {
  if (step < 0 || step > 4)
    throw std::out_of_range("arena step outside 0..4");
  Raw t = F::sub(timer, F::mul(F::from_int(static_cast<std::uint32_t>(step)), 0x3cf5c28f));
  if (F::lt(t, F::ZERO))
    t = F::ZERO;
  return F::le(t, F::ONE);
}
std::pair<int, int> arenaPulseArgs(int step) {
  if (step < 0 || step > 4)
    throw std::out_of_range("arena step outside 0..4");
  return {0x80 - step * 0x10, 0x7d0 - step * 0xc8};
}
} // namespace champions::hud_dynamic
