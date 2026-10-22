#include "champions/ui_text_gaps.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace champions::ui_text_gaps {
namespace F = simulation::rta;
namespace {
const F::Fpu kPs2{F::Fpu::Model::Ps2, false, false};
constexpr std::uint32_t M32 = 0xffffffffu;

std::int32_t s8(std::int32_t v) { return static_cast<std::int8_t>(v & 0xff); }
std::int32_t s16(std::int64_t v) { return static_cast<std::int16_t>(v & 0xffff); }
std::uint32_t u32(std::int64_t v) { return static_cast<std::uint32_t>(v); }
// (v + (v >> 31)) >> 1 on a signed word: C division by two.
std::int32_t halfTowardZero(std::int32_t v) { return (v + (v < 0 ? 1 : 0)) >> 1; }

Call call(std::string name, std::vector<Arg> args = {}) { return {std::move(name), std::move(args)}; }

// ref_ui_composite.scale_rgb (color_scale 0x19ecac): R, G and B are
// trunc(channel * factor) clamped to 0..255; the alpha byte is copied unchanged.
std::uint32_t scaleRgb(std::uint32_t colour, Raw factor) {
  std::uint32_t out = colour & 0xff000000u;
  for (int shift = 0; shift <= 16; shift += 8) {
    std::int32_t v = F::to_int(F::mul(F::from_int((colour >> shift) & 0xffu), factor));
    v = std::clamp(v, 0, 255);
    out |= static_cast<std::uint32_t>(v) << shift;
  }
  return out;
}
} // namespace

std::string canonical(const Call &c) {
  std::string out = c.name;
  char buf[16];
  for (const auto &a : c.args) {
    out += '\t';
    if (const auto *w = std::get_if<std::uint32_t>(&a)) {
      std::snprintf(buf, sizeof buf, "%08x", *w);
      out += buf;
    } else {
      out += "s:";
      for (const unsigned char c : std::get<std::string>(a)) {
        if (std::isalnum(c) || c == '.' || c == '_' || c == '<' || c == '>') {
          out += static_cast<char>(c);
        } else {
          std::snprintf(buf, sizeof buf, "%%%02x", c);
          out += buf;
        }
      }
    }
  }
  return out;
}

// 1 ---------------------------------------------------------------------------
Ramps rampsStep(const Ramps &in) {
  const F::ScopedFpu fpu{kPs2};
  Ramps r = in;
  for (std::size_t i = 0; i < 4; ++i) {
    const Raw p = r.pulse[i];
    if (F::lt(ONE, p))
      r.pulse[i] = F::add(F::mul(F::sub(p, ONE), PULSE_DECAY), ONE);
    const Raw f = r.fade[i];
    if (F::lt(f, ONE)) {
      const Raw n = F::add(f, FADE_STEP); // stored before the clamp test
      r.fade[i] = F::lt(ONE, n) ? ONE : n;
    }
  }
  return r;
}
std::vector<Raw> fadeRamp(Raw start, int limit) {
  std::vector<Raw> out;
  Ramps r;
  r.fade = {start, start, start, start};
  for (int i = 0; i < limit; ++i) {
    r = rampsStep(r);
    out.push_back(r.fade[0]);
    if (r.fade[0] == ONE)
      return out;
  }
  throw std::runtime_error("fade did not reach 1.0");
}
std::vector<Raw> pulseRamp(Raw start, int frames) {
  std::vector<Raw> out;
  Ramps r;
  r.pulse = {start, start, start, start};
  for (int i = 0; i < frames; ++i) {
    r = rampsStep(r);
    out.push_back(r.pulse[0]);
  }
  return out;
}

// 2 ---------------------------------------------------------------------------
std::vector<LayoutWord> buttonLayout(std::int8_t layout_byte) {
  // game_initCamera 0x1e6bd4..0x1e6ca4, sorted by address.
  if (layout_byte == 1)
    return {{0x41b960, 0x40}, {0x41b964, 0x7e}, {0x41b968, 0x10}, {0x41b96c, 0x7c},
            {0x41b970, 0x80}, {0x41b974, 0x7d}, {0x41b978, 0x7e}, {0x41b97c, 0x7b},
            {0x41bfb8, 0x20}, {0x41bfbc, 0x40}};
  if (layout_byte == 0)
    return {{0x41b960, 0x20}, {0x41b964, 0x7b}, {0x41b968, 0x80}, {0x41b96c, 0x7c},
            {0x41b978, 0x7b}, {0x41b97c, 0x7e}, {0x41bfb8, 0x140}, {0x41bfbc, 0x20}};
  return {};
}
std::optional<std::uint32_t> confirmGlyph(std::int8_t layout_byte) {
  if (layout_byte != LAYOUT_OBSERVED)
    return std::nullopt; // layout 0 is inferred from code, others write nothing
  for (const auto &w : buttonLayout(layout_byte))
    if (w.address == G_GLYPH_CONFIRM)
      return w.value;
  return std::nullopt;
}
std::uint32_t padMaskToGlyph(std::uint32_t mask) {
  switch (mask) {
  case 0x80: return 0x7d;
  case 0x20: return 0x7b;
  case 0x10: return 0x7c;
  case 0x40: return 0x7e;
  case 0x8000: return 0x7f;
  case 0x2000: return 0x80;
  case 0x1000: return 0x81;
  case 0x4000: return 0x82;
  case 0x4: return 0x83;
  case 0x1: return 0x84;
  case 0x8: return 0x85;
  case 0x2: return 0x86;
  default: return M32;
  }
}
std::string glyphText(std::uint32_t word) {
  const auto b = static_cast<char>(word & 0xffu);
  return b ? std::string(1, b) : std::string();
}
bool isButtonGlyph(std::uint8_t code) { return code >= 0x7b && code <= 0x88; }

// 3 ---------------------------------------------------------------------------
int helpButtonAlpha(std::int32_t delay) {
  int a = 0x80 - delay * 10;
  a = a < 0 ? 0 : a;
  return a >= 0x81 ? 0x80 : a;
}

HelpResult helpTask(const HelpState &in) {
  const F::ScopedFpu fpu{kPs2};
  HelpResult r;
  r.state = in;
  auto &st = r.state;
  auto &calls = r.calls;
  if (st.delay > 0)
    st.delay -= 1;
  if (!s8(st.open))
    return r;
  if (!s8(st.net)) {
    st.paused = 1;
    st.pause_byte = 1;
  }
  if (st.dl_free >= DL_MIN_FREE) {
    const Raw p = st.pulse;
    calls.push_back(call("text_begin", {st.font_med, 6u, scaleRgb(HELP_TITLE_COLOUR, p), M32}));
    if (st.title)
      calls.push_back(call("text_drawWideCentered", {0x144u, 0x64u, st.title, 1000000u}));
    calls.push_back(call("text_setColor", {scaleRgb(HELP_BODY_COLOUR, p), M32, 0u}));
    std::int32_t h = 0;
    if (st.body) {
      calls.push_back(call("text_drawWrapped", {0x84u, 0x82u, st.body, 0x194u, 0u, 0x1c0u, 0x15u}));
      h = st.wrap_height;
    }
    const std::string glyph = glyphText(st.glyph_word);
    calls.push_back(call("text_lookup", {std::string(HELP_LABEL_KEY)}));
    calls.push_back(call("text_widthWide", {st.font_med, std::string("label"), 1000000u}));
    const std::int32_t half = halfTowardZero(st.label_width);
    const std::uint32_t c3 = scaleRgb((static_cast<std::uint32_t>(helpButtonAlpha(st.delay)) << 24) | HELP_BUTTON_RGB, p);
    calls.push_back(call("text_setColor", {c3, M32, 0u}));
    calls.push_back(call("text_drawAscii", {u32(0x144 - half - 0x19), u32(h + 0x8b), glyph, 100000u}));
    calls.push_back(call("text_lookup", {std::string(HELP_LABEL_KEY)}));
    calls.push_back(call("text_drawWide", {u32(0x144 - half), u32(h + 0x8b), std::string("label"), 1000000u}));
    calls.push_back(call("text_end"));
    const std::uint32_t c4 = scaleRgb(HELP_FRAME_COLOUR, p);
    // wrapper 0x17fbb8: x-1, y-4, w+2, h+8, style forced to 2, then the inner ctile frame
    calls.push_back(call("ui_drawFrameImpl", {u32(0x74 - 1), u32(0x5e - 4), u32(0x1a0 + 2), u32(h + 0x4c + 8),
                                              6u, c4, 2u, 0u, 0u}));
    calls.push_back(call("ui_drawFrameImpl", {0x7cu, 0x68u, 0x1a0u, u32(h + 0x4c), 6u, HELP_INNER_TINT, 0u, 0u, 0u}));
  }
  if (st.delay != 0)
    return r;
  for (int pad = 0; pad < std::max(st.num_players, 0) && pad < 4; ++pad) {
    if (!(st.pads[static_cast<std::size_t>(pad)] & st.confirm_mask))
      continue;
    // no break: every pressing pad closes again
    st.open = 0;
    st.lockout = HELP_CLOSE_DELAY;
    if (!s8(st.net))
      calls.push_back(call("script_runPostAction"));
    st.delay = HELP_CLOSE_DELAY;
    st.pause_byte = 0;
    if (!s8(st.local_menu))
      st.paused = 0;
    for (const int slot : st.alive) {
      calls.push_back(call("net_isLocalSlot", {u32(slot)}));
      if (st.local.count(slot))
        calls.push_back(call("hudmsg_sendMenuState", {u32(s8(slot)), 0u, 0u}));
    }
  }
  return r;
}

Frame helpInnerFrame(int wrap_height) { return {0x7c, 0x68, 0x1a0, wrap_height + 0x4c, HELP_INNER_TINT, 0}; }

// 4 ---------------------------------------------------------------------------
std::uint32_t bustAlpha(Raw fade, Raw factor) { return F::to_uint(F::mul(fade, factor)) & 0xffu; }

BustResult hudDrawBust(const BustState &in) {
  const F::ScopedFpu fpu{kPs2};
  BustResult r;
  r.fade = in.fade;
  r.latch = in.latch;
  r.value = in.value;
  if (!s8(in.shown))
    return r;
  Raw f = s8(in.latch) ? F::sub(in.fade, BUST_FADE_STEP) : F::add(in.fade, BUST_FADE_STEP);
  if (F::lt(ONE, f))
    f = ONE;
  if (F::lt(f, ZERO))
    f = ZERO;
  r.fade = f;
  r.latch = 0;
  if (r.value < 0)
    r.value = 0;
  const std::int32_t value = r.value, maximum = in.maximum;
  auto &calls = r.calls;
  calls.push_back(call("text_begin", {in.font_small, 6u,
                                      (bustAlpha(f, BUST_TEXT_ALPHA) << 24) | BUST_TEXT_RGB, M32}));
  if (maximum == -1) {
    calls.push_back(call("text_drawWrapped", {0x20bu, 0x187u, 0x86aab0u, 0x70u, u32(-100), 0x1f4u, 0xeu}));
    calls.push_back(call("text_setColor", {(bustAlpha(f, BUST_SPRITE_ALPHA) << 24) | BUST_TEXT_RGB, M32, 0u}));
    calls.push_back(call("text_drawAsciiCentered", {0x21eu, 0x173u, std::to_string(value), 0x2710u}));
  } else {
    calls.push_back(call("text_drawWrapped", {0x20fu, 0x185u, 0x86aab0u, 0x70u, u32(-100), 0x1f4u, 0xeu}));
  }
  calls.push_back(call("text_end"));
  const std::uint32_t tint = (bustAlpha(f, BUST_SPRITE_ALPHA) << 24) | BUST_SPRITE_RGB;
  calls.push_back(call("res_find", {std::string("bust.lmp"), std::string("frame.tex")}));
  calls.push_back(call("res_find", {std::string("bust.lmp"), std::string("<portrait>")}));
  if (maximum != -1) {
    calls.push_back(call("res_find", {std::string("bust.lmp"), std::string("fieldmeter.tex")}));
    if (in.bar_frame_available) {
      const std::int32_t half = halfTowardZero(in.sim_frame);
      const std::int32_t q = (half < 0 ? -1 : 1) * (std::abs(half) % 4); // C remainder
      const std::string bar = "bar_yellow0" + std::to_string(q) + ".tex";
      calls.push_back(call("res_find", {std::string("bust.lmp"), bar}));
      const TexSize b = in.tex(bar);
      const Raw ratio = F::div(F::from_int(u32(value)), F::from_int(u32(maximum)));
      const std::int32_t fill = F::round_half_away(F::mul(ratio, F::from_int(u32(s16(b.w)))));
      calls.push_back(call("sprite_clipped", {bar, 0x209u, 0x179u, 6u, tint, 0x209u, 0x179u,
                                              u32(0x209 + fill - 2), u32(0x179 + s16(b.h)), 0u, 0u}));
    }
    calls.push_back(call("sprite", {std::string("fieldmeter.tex"), 0x1ffu, 0x173u, 6u, tint}));
  } else {
    calls.push_back(call("res_find", {std::string("bust.lmp"), std::string("fieldnumber.tex")}));
    calls.push_back(call("sprite", {std::string("fieldnumber.tex"), 0x1feu, 0x172u, 6u, tint}));
  }
  const TexSize p = in.tex("<portrait>"), fr = in.tex("frame.tex");
  calls.push_back(call("sprite", {std::string("<portrait>"), u32(0x1f3 - halfTowardZero(s16(p.w))),
                                  u32(s16(fr.h) - s16(p.h) + 0x16e), 6u, tint}));
  calls.push_back(call("sprite", {std::string("frame.tex"), u32(0x1f3 - halfTowardZero(s16(fr.w))), 0x171u, 6u, tint}));
  return r;
}

// 5 ---------------------------------------------------------------------------
namespace {
constexpr std::uint32_t REG_UV = 0x03, REG_XYZ2 = 0x05, REG_SCANMSK = 0x22;
constexpr std::uint64_t Z_WORD = static_cast<std::uint64_t>(0xa000) << 20; // Z = 10 in bits 32..63
std::uint64_t uv(std::int64_t u, std::int64_t v) {
  return (static_cast<std::uint64_t>(u) & 0xffffu) | ((static_cast<std::uint64_t>(v) & 0xffffu) << 16);
}
std::uint64_t xyz(std::int64_t x, std::int64_t y) {
  return (static_cast<std::uint64_t>(x) & 0xffffu) | ((static_cast<std::uint64_t>(y) & 0xffffu) << 16) | Z_WORD;
}
} // namespace

std::vector<Qword> spriteQuad(bool frame_mode, std::int8_t field_mode, std::int32_t tex_w, std::int32_t tex_h,
                              std::int32_t x, std::int32_t y, std::int32_t u0, std::int32_t v0, std::int32_t u1,
                              std::int32_t v1, std::array<int, 2> fields) {
  const std::int64_t w = s16(tex_w), h = s16(tex_h), half = h >> 1;
  std::int64_t Y = (static_cast<std::int64_t>(y) * 16 + 0x7200) & M32;
  if (frame_mode) {
    const std::int64_t X = (static_cast<std::int64_t>(x) * 32 + 0x5800) & M32;
    const std::int64_t X1 = X + w * 32, Y1 = Y + h * 16;
    const bool odd = (Y & 0x10) != 0;
    return {{odd ? 2u : 3u, REG_SCANMSK},
            {uv(u0 * 16 + 4, v0 * 8 + 4), REG_UV}, {xyz(X, Y), REG_XYZ2},
            {uv(u1 * 16 + 4, v1 * 8 + 4), REG_UV}, {xyz(X1, Y1), REG_XYZ2},
            {odd ? 3u : 2u, REG_SCANMSK},
            {uv(u0 * 16 + 4, half * 16 + v0 * 8), REG_UV}, {xyz(X, Y), REG_XYZ2},
            {uv(u1 * 16 + 4, half * 16 + v1 * 8), REG_UV}, {xyz(X1, Y1), REG_XYZ2},
            {0, REG_SCANMSK}};
  }
  if (s8(field_mode)) {
    const std::int64_t X = (static_cast<std::int64_t>(x) * 32 + 0x5800) & M32;
    std::int64_t base;
    if (Y & 0x10) {
      Y = (fields[0] & 1) ? Y + 0x10 : Y - 0x10;
      base = (fields[1] & 1) ? half * 16 : -4;
    } else {
      base = (fields[0] & 1) ? 0 : half * 16 - 4;
    }
    return {{uv(u0 * 16 + 4, v0 * 8 + base + 4), REG_UV}, {xyz(X, Y), REG_XYZ2},
            {uv(u1 * 16 + 4, v1 * 8 + base + 4), REG_UV}, {xyz(X + w * 32, Y + h * 16), REG_XYZ2}};
  }
  const std::int64_t X = (static_cast<std::int64_t>(x) * 16 + 0x6c00) & M32;
  return {{uv(u0 * 16 + 8, v0 * 16 + 8), REG_UV}, {xyz(X, Y), REG_XYZ2},
          {uv(u1 * 16 + 8, v1 * 16 + 8), REG_UV}, {xyz(X + w * 16, Y + h * 16), REG_XYZ2}};
}
int fieldCalls(bool frame_mode, std::int8_t field_mode, std::int32_t y) {
  if (frame_mode || !s8(field_mode))
    return 0;
  return (((static_cast<std::int64_t>(y) * 16 + 0x7200) & 0x10) != 0) ? 2 : 1;
}
std::optional<WovenLine> wovenLine(std::int32_t screen_line, std::int32_t y, std::int32_t h) {
  const std::int32_t k = screen_line - y;
  if (k < 0 || k >= h)
    return std::nullopt;
  return WovenLine{k / 2 + ((k & 1) ? h / 2 : 0), k};
}

// 6 ---------------------------------------------------------------------------
CompassWorld compassWorld(const F::Mat4 &view, const std::array<Raw, 3> &camera, const std::array<Raw, 2> &player_xy,
                          const std::array<Raw, 2> &target_xy) {
  const F::ScopedFpu fpu{kPs2};
  const F::Mat4 m = F::mat4_translate_local(ZERO, ZERO, ZERO, view);
  const auto t = F::mat4_transform_point_transposed(m, {COMPASS_OFFSET[0], COMPASS_OFFSET[1], COMPASS_OFFSET[2]});
  const F::Vec3 origin{F::add(t.point[0], camera[0]), F::add(t.point[1], camera[1]), F::add(t.point[2], camera[2])};
  F::Mat34 world = F::mat34_set_identity();
  world = F::mat34_translate_local(origin[0], origin[1], origin[2], world);
  const Raw dx = F::sub(player_xy[0], target_xy[0]);
  const Raw dy = F::sub(player_xy[1], target_xy[1]);
  const std::int32_t angle = s16(static_cast<std::int64_t>(F::atan2_16(dy, dx)) - 0x8000);
  world = F::mat34_rotate_axis(world, 2, static_cast<std::uint32_t>(angle));
  return {world, angle};
}

CompassResult hudDrawCompass(const CompassState &in) {
  CompassResult r;
  r.state = in;
  auto &st = r.state;
  if (!st.gate || st.player == -1 || !st.object)
    return r;
  r.calls.push_back(call("dialog_isOpen"));
  if (st.dialog_open || !s8(st.shown))
    return r;
  const auto w = compassWorld(st.view, st.camera, st.player_xy, st.target_xy);
  r.angle = w.angle;
  const auto draw = [&](std::uint32_t flags) {
    std::vector<Arg> a{5u, ZERO, ZERO, ONE};
    for (const Raw v : w.world)
      a.emplace_back(v);
    a.insert(a.end(), {0u, M32, 0u, 0u, flags, st.light_flag, st.light_depth});
    r.calls.push_back(call("model_draw", std::move(a)));
  };
  draw(st.flags | 0x20u); // pass 1: depth test ALWAYS (flag 0x20)
  st.flags = 0;
  st.light_flag = 1;
  st.light_depth += 10;
  draw(0); // pass 2: normal depth state, light depth + 10
  r.calls.push_back(call("light_resetObjectLight"));
  r.calls.push_back(call("text_begin", {st.font_small, 5u, COMPASS_LABEL_COLOUR, M32}));
  r.calls.push_back(call("text_drawWrappedCentered", {0x41u, 0x18au, 0x86aaf0u, 0x83u, u32(-100), 0x1f4u, 0x12u}));
  r.calls.push_back(call("text_end"));
  return r;
}

// 7 ---------------------------------------------------------------------------
std::vector<SeqEvent> soundSeqUpdateAll(std::vector<SeqSlotView> &slots, const std::vector<VoiceView> &voices,
                                        bool frozen, const std::array<Raw, 3> &camera,
                                        const std::function<void(std::size_t, SeqSlotView &)> &advance) {
  const F::ScopedFpu fpu{kPs2};
  std::vector<SeqEvent> events;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    auto &s = slots[i];
    if (s8(s.tracks) == 0)
      continue;
    if (!s.released) {
      bool free = false;
      if (!s.has_event_table) {
        free = true;
      } else {
        const std::int32_t idx = s16(s.sound_idx);
        if (idx < 0 || idx > static_cast<std::int32_t>(s.events.size()))
          throw std::out_of_range("sound cursor outside the event table");
        const bool terminator = idx == static_cast<std::int32_t>(s.events.size());
        free = terminator || (s.flags & STATE_ENDED) != 0;
      }
      if (free) {
        s.tracks = 0;
        for (std::size_t v = 0; v < voices.size(); ++v) {
          const auto &vo = voices[v];
          if ((vo.flags & 4) && (vo.flags & 0x10) && s16(vo.owner) == static_cast<std::int32_t>(i) &&
              s16(vo.generation) == s16(s.serial))
            events.push_back({"stop", static_cast<std::int32_t>(v), 0});
        }
        continue;
      }
    } else {
      const Raw dur = F::from_int(u32(s.duration));
      if (F::lt(dur, s.time))
        s.time = ZERO;
    }
    if (frozen && !s.while_frozen)
      continue;
    if (s.linked)
      s.pos = *s.linked; // the linked object's +0x10..+0x1b
    if (s.no_pos)
      s.pos = camera;    // tested after the object copy: the camera wins
    events.push_back({"advance", static_cast<std::int32_t>(i), s.volume});
    if (advance)
      advance(i, s);
  }
  return events;
}
} // namespace champions::ui_text_gaps
