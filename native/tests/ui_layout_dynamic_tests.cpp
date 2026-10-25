#include "champions/hud_dynamic.hpp"
#include "champions/ui_composite.hpp"
#include "champions/ui_text_layout.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace champions;
namespace H = champions::hud_dynamic;
namespace T = champions::ui_text_layout;
namespace C = champions::ui_composite;
namespace F = champions::simulation::rta;
namespace {
int checks = 0;
void expect(bool ok, const char *name) {
  ++checks;
  if (!ok)
    throw std::runtime_error(name);
}
template <class Fn> void reject(Fn fn, const char *name) {
  bool threw = false;
  try {
    fn();
  } catch (const std::exception &) {
    threw = true;
  }
  expect(threw, name);
}
void put16(std::vector<std::uint8_t> &b, std::size_t o, int v) {
  b.at(o) = v & 255;
  b.at(o + 1) = (v >> 8) & 255;
}
void put32(std::vector<std::uint8_t> &b, std::size_t o, int v) {
  put16(b, o, v & 65535);
  put16(b, o + 2, v >> 16);
}
ui_layout::FontMetrics fixture() {
  constexpr int n = 0x60, go = 0x214, ko = go + n * 16, to = ko + 12;
  std::vector<std::uint8_t> b(to + 0x40);
  put16(b, 0, n);
  put16(b, 2, 8);
  put32(b, 4, 2);
  put32(b, 8, go);
  put32(b, 12, ko);
  put32(b, 16, to);
  for (int i = 0; i < 256; ++i)
    put16(b, 0x14 + 2 * i, -1);
  for (auto [c, index] : std::array<std::pair<int, int>, 5>{
           {{0x20, 0}, {'A', 1}, {'B', 2}, {45, 3}, {'{', 0x5b}}})
    put16(b, 0x14 + 2 * c, index);
  for (int i = 0; i < n; ++i) {
    std::array<int, 8> g{0x1000 + i, 0, 9, 16, 26, 1, 9, -1};
    if (i == 0)
      g = {0x20, 0, 0, 0, 0, 0, 3, -1};
    if (i == 1)
      g = {65, 0, 6, 0, 10, 2, 7, -1};
    if (i == 2)
      g = {66, 6, 13, 0, 12, 0, 8, 0};
    if (i == 3)
      g = {45, 13, 18, 0, 4, 5, 5, -1};
    if (i == 0x5b)
      g = {0x7b, 20, 30, 0, 10, 0, 10, -1};
    for (int k = 0; k < 8; ++k)
      put16(b, go + i * 16 + k * 2, g[k]);
  }
  put16(b, ko, 1);
  put16(b, ko + 2, 2);
  put16(b, ko + 4, -3);
  put16(b, ko + 6, -1);
  put16(b, ko + 8, -1);
  put16(b, ko + 10, 0);
  return ui_layout::fontMetrics({b.data(), b.size()});
}
C::Texture solid(int w, int h, C::Pixel color) {
  C::Texture t{w, h, {}};
  for (int i = 0; i < w * h; ++i)
    t.rgba.insert(t.rgba.end(), color.begin(), color.end());
  return t;
}
C::Pixel pixel(const std::vector<std::uint8_t> &fb, int x, int y) {
  std::size_t at = 4 * (y * C::WIDTH + x);
  return {fb[at], fb[at + 1], fb[at + 2], fb[at + 3]};
}
} // namespace
std::uint32_t hex32(const std::string &s) {
  return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16));
}
int dec(const std::string &s) { return std::stoi(s); }
std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> out;
  std::stringstream in(line);
  std::string field;
  while (std::getline(in, field, '\t'))
    out.push_back(field);
  return out;
}
void checkHudRound3(const std::string &path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("open hud_r3.tsv");
  std::string line;
  int rows = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    const auto f = split(line);
    const auto &k = f.at(0);
    if (k == "minimap_params") {
      const auto p = H::minimapParams(dec(f[1]), dec(f[2]));
      for (int i = 0; i < 5; ++i)
        expect(p.words[static_cast<std::size_t>(i)] == hex32(f[static_cast<std::size_t>(3 + i)]),
               "minimap params");
    } else if (k == "minimap_extra") {
      const auto e = H::minimapExtra(dec(f[1]));
      expect(e.first == hex32(f[2]) && e.second == hex32(f[3]), "minimap extra");
    } else if (k == "minimap_approach") {
      expect(H::minimapApproach(hex32(f[1]), hex32(f[2])) == hex32(f[3]),
             "minimap approach");
    } else if (k == "minimap_zoom") {
      expect(H::minimapZoom(hex32(f[1]), hex32(f[2])) == hex32(f[3]), "minimap zoom");
    } else if (k == "minimap_blip") {
      const auto got = H::minimapPlayerBlip(dec(f[1]));
      if (f[2] == "-")
        expect(!got, "minimap blip none");
      else
        expect(got && (*got)[0] == dec(f[2]) && (*got)[1] == dec(f[3]) &&
                   (*got)[2] == dec(f[4]),
               "minimap blip");
    } else if (k == "minimap_drawn") {
      expect(H::minimapPlayerDrawn(dec(f[1]), static_cast<std::uint32_t>(dec(f[2]))) ==
                 (f[3] == "1"),
             "minimap drawn");
    } else if (k == "flying_life") {
      expect(H::flyingTextLife(dec(f[1])) == dec(f[2]), "flying life");
    } else if (k == "flying_drift") {
      expect(H::flyingTextDrift(hex32(f[1])) == dec(f[2]), "flying drift");
    } else if (k == "flying_step") {
      static std::uint16_t seq_life = 0, seq_alpha = 0;
      static int seq_start = -1, seq_n = -1;
      std::uint16_t in_life = 0, in_alpha = 0;
      if (f[2] == "alpha2") {
        in_life = 5;
        in_alpha = 2;
      } else {
        const int start = dec(f[1]);
        const int n = dec(f[2]);
        if (n == 0 || start != seq_start || n != seq_n + 1) {
          seq_start = start;
          seq_life = static_cast<std::uint16_t>(start);
          seq_alpha = 0x80;
        }
        in_life = seq_life;
        in_alpha = seq_alpha;
        seq_n = n;
      }
      const auto step = H::flyingTextStep(in_life, in_alpha);
      if (f[2] != "alpha2") {
        seq_life = step.life;
        seq_alpha = step.alpha;
      }
      expect(step.life == static_cast<std::uint16_t>(dec(f[3])) &&
                 step.alpha == static_cast<std::uint16_t>(dec(f[4])) &&
                 step.erase == (f[5] == "1"),
             "flying step");
    } else if (k == "flying_style") {
      const auto s = H::flyingTextStyle(dec(f[1]), f[2] == "1", dec(f[3]));
      expect(s.colour == hex32(f[4]) && s.force_x == (f[5] == "1") && s.lift == hex32(f[6]),
             "flying style");
    } else if (k == "flying_z") {
      expect(H::flyingTextAnchorZ(hex32(f[1]), hex32(f[2])) == hex32(f[3]), "flying z");
    } else if (k == "flying_screen") {
      const auto s = H::flyingTextScreen(hex32(f[1]), hex32(f[2]), hex32(f[3]), hex32(f[4]),
                                          hex32(f[5]));
      expect(s.first == hex32(f[6]) && s.second == hex32(f[7]), "flying screen");
    } else if (k == "mat4_xyz") {
      std::array<F::Raw, 16> m{};
      std::array<F::Raw, 3> v{};
      for (int i = 0; i < 16; ++i)
        m[static_cast<std::size_t>(i)] = hex32(f[static_cast<std::size_t>(1 + i)]);
      for (int i = 0; i < 3; ++i)
        v[static_cast<std::size_t>(i)] = hex32(f[static_cast<std::size_t>(17 + i)]);
      const auto g = H::mat4Xyz(m, v);
      expect(g[0] == hex32(f[20]) && g[1] == hex32(f[21]) && g[2] == hex32(f[22]),
             "mat4 xyz");
    } else if (k == "compass_anchor") {
      std::array<F::Raw, 16> m{};
      std::array<F::Raw, 3> c{};
      for (int i = 0; i < 16; ++i)
        m[static_cast<std::size_t>(i)] = hex32(f[static_cast<std::size_t>(1 + i)]);
      for (int i = 0; i < 3; ++i)
        c[static_cast<std::size_t>(i)] = hex32(f[static_cast<std::size_t>(17 + i)]);
      const auto g = H::compassAnchor(m, c);
      expect(g[0] == hex32(f[20]) && g[1] == hex32(f[21]) && g[2] == hex32(f[22]),
             "compass anchor");
    } else if (k == "compass_label") {
      const auto l = H::compassLabel();
      expect(l.x == dec(f[1]) && l.y == dec(f[2]) && l.width == dec(f[3]) &&
                 l.top == dec(f[4]) && l.bottom == dec(f[5]) && l.line == dec(f[6]) &&
                 l.colour == hex32(f[7]),
             "compass label");
    } else if (k == "help_alpha") {
      expect(H::helpButtonAlpha(dec(f[1])) == dec(f[2]) &&
                 H::helpButtonColour(dec(f[1])) == hex32(f[3]),
             "help alpha");
    } else if (k == "help_box") {
      const auto b = H::helpBox(dec(f[1]), dec(f[2]));
      expect(b.button_x == dec(f[3]) && b.button_y == dec(f[4]) && b.label_x == dec(f[5]) &&
                 b.label_y == dec(f[6]) && b.frame_height == dec(f[7]) &&
                 b.inner_height == dec(f[8]),
             "help box");
    } else if (k == "notice_box") {
      const auto b = H::noticeBox(H::noticeColumn(dec(f[1])), dec(f[2]));
      expect(b.title_x == dec(f[4]) && b.title_y == dec(f[5]) && b.body_x == dec(f[6]) &&
                 b.body_y == dec(f[7]) && b.body_width == dec(f[8]) && b.body_top == dec(f[9]) &&
                 b.body_bottom == dec(f[10]) && b.body_line == dec(f[11]) &&
                 b.button_x == dec(f[12]) && b.button_y == dec(f[13]) &&
                 b.label_x == dec(f[14]) && b.label_y == dec(f[15]) &&
                 b.frame_x == dec(f[16]) && b.frame_y == dec(f[17]) &&
                 b.frame_width == dec(f[18]) && b.frame_height == dec(f[19]) &&
                 b.rect_x0 == dec(f[20]) && b.rect_y0 == dec(f[21]) &&
                 b.rect_x1 == dec(f[22]) && b.rect_y1 == dec(f[23]) &&
                 H::noticeColumn(dec(f[1])) == dec(f[3]),
             "notice box");
    } else if (k == "message_box") {
      const auto p = H::messageBoxPlacement(dec(f[1]), dec(f[2]), dec(f[3]), dec(f[4]));
      expect(p.x == hex32(f[5]) && p.y == hex32(f[6]) && p.width == static_cast<std::uint32_t>(dec(f[7])) &&
                 p.layer == static_cast<std::uint32_t>(dec(f[8])),
             "message box");
    } else if (k == "medal") {
      const auto g = H::medalPrompt(f[1] == "1", f[2] == "1", f[3] == "1", f[4] == "1", f[5] == "1");
      if (f[6] == "0")
        expect(!g, "medal hidden");
      else {
        expect(g && (g->title_key.empty() ? f[7] == "-" : g->title_key == f[7]) &&
                   (g->body_key.empty() ? f[8] == "-" : g->body_key == f[8]) &&
                   g->box.x == hex32(f[9]) && g->box.y == hex32(f[10]) &&
                   g->box.width == static_cast<std::uint32_t>(dec(f[11])) &&
                   g->box.layer == static_cast<std::uint32_t>(dec(f[12])) &&
                   g->list_x == dec(f[13]) && g->list_y == dec(f[14]),
               "medal");
      }
    } else if (k == "net_cam") {
      expect(H::netCamShow(f[1] == "1", f[2] == "1", f[3] == "1", f[4] == "1", dec(f[5]),
                           dec(f[6])) == (f[7] == "1"),
             "net cam");
    } else if (k == "net_pad") {
      expect(H::netCamPad(dec(f[1])) == dec(f[2]), "net pad");
    } else if (k == "net_cam_text") {
      const auto t = H::netCamText();
      expect(t.name_x == dec(f[1]) && t.name_y == dec(f[2]) && t.prompt_x == dec(f[3]) &&
                 t.prompt_y == dec(f[4]) && t.prompt_colour == hex32(f[5]),
             "net cam text");
    } else if (k == "arena_active") {
      expect(H::arenaPlayerActive(hex32(f[1])) == (f[2] == "1"), "arena active");
    } else if (k == "arena_step") {
      expect(H::arenaStepShown(hex32(f[1]), dec(f[2])) == (f[3] == "1"), "arena step");
    } else if (k == "arena_pulse") {
      const auto p = H::arenaPulseArgs(dec(f[1]));
      expect(p.first == dec(f[2]) && p.second == dec(f[3]), "arena pulse");
    } else if (k == "bust_fade") {
      expect(H::bustFadeStep(hex32(f[1]), f[2] == "1") == hex32(f[3]), "bust fade");
    } else if (k == "bust_alpha") {
      const auto a = H::bustAlphas(hex32(f[1]));
      expect(a.first == static_cast<std::uint32_t>(dec(f[2])) &&
                 a.second == static_cast<std::uint32_t>(dec(f[3])),
             "bust alpha");
    } else if (k == "bust_meter") {
      const auto m = H::bustMeter(dec(f[1]), dec(f[2]), dec(f[3]), dec(f[4]));
      expect(m.resource == f[5] && m.clip_right == dec(f[6]), "bust meter");
    } else if (k == "dimx") {
      const auto m = C::dimxMatrix();
      expect(m[static_cast<std::size_t>(dec(f[1]))][static_cast<std::size_t>(dec(f[2]))] ==
                 dec(f[3]),
             "dimx");
    } else if (k == "dither5") {
      expect(C::dither5(dec(f[1]), dec(f[2]), dec(f[3])) == dec(f[4]), "dither5");
    } else if (k == "scale_rgb") {
      expect(C::scaleRgb(hex32(f[1]), hex32(f[2])) == hex32(f[3]), "scale rgb");
    } else if (k == "present") {
      expect(C::presentColour(dec(f[1]), dec(f[2]), dec(f[3]), hex32(f[4])) == hex32(f[5]) &&
                 C::presentGain(hex32(f[4])) == hex32(f[6]),
             "present");
    } else if (k == "present_step") {
      expect(C::presentChannelStep(dec(f[1])) == dec(f[2]), "present step");
    } else {
      throw std::runtime_error("unknown hud_r3 row " + k);
    }
    ++rows;
  }
  expect(rows == 371, "hud_r3 row count");
}
void checkRound3Extras() {
  auto number = H::FlyingNumber::make(0, false, 0, F::from_int(3), "12");
  const auto before_life = number.life;
  const auto before_off = number.x_off;
  const auto drawn = number.draw(0, 0, F::ONE);
  expect(number.life == before_life && number.x_off == before_off && drawn.layer == 5 &&
             drawn.x_off == before_off,
         "flying draw leaves simulation state");
  expect(number.step() && number.life == before_life - 1, "flying step is simulation");
  auto crit = H::FlyingNumber::make(1, true, 0, 0, "9");
  const auto forced = crit.draw(0, 0, F::ONE);
  expect(forced.x_off == 5 && crit.x_off != 5, "forced draw x does not write drift");
  H::Records batch;
  batch.emplace_back(H::Text{"wrapped", 523, 391, 6, 0, "a", 1});
  batch.emplace_back(H::Text{"centered", 542, 371, 6, 0, "b", 1});
  batch.emplace_back(H::Sprite{"frame.tex", 0, 369, 6, 0});
  const auto order = H::painterIndices(batch);
  expect(order.size() == 3 && order[0] == 2 && order[1] == 0 && order[2] == 1,
         "bust text batch stays forward");
  const auto gold = H::minimapObjectBlip(0x20, std::uint64_t{1} << 43);
  const auto grey = H::minimapObjectBlip(0, 0);
  expect(gold.rgb[0] == 0xe7 && gold.rgb[1] == 0xc1 && gold.rgb[2] == 0x52 && !gold.scale &&
             grey.rgb[0] == 0xc8 && grey.scale && *grey.scale == 0x3f000000u,
         "object blip colours");
  reject([] { H::minimapProjector(); }, "projector refused");
  reject([] { H::compassMesh(); }, "compass mesh refused");
  reject([] { H::arenaHermitePoint(); }, "arena point refused");
  reject([] { H::textureFlag0x1000(); }, "tex flag refused");
  reject([] { H::fontHeaderPlus2(); }, "font header refused");
  reject([] { H::minimapParams(3, 1); }, "minimap mode refused");
  reject([] { H::arenaPulseArgs(5); }, "arena step refused");
  // hud_drawBust itself is champions::ui_text_gaps::hudDrawBust (ui_text_gaps_tests).
}
int main(int argc, char **argv) {
  try {
    auto font = fixture();
    T::Codes ab{'A', 'B'}, az{'A', 10, 'B'};
    expect(T::glyphIndexWide(font, 'Z') == 3 &&
               T::glyphIndexWide(font, 0x1004) == 4 &&
               T::glyphIndexWide(font, 0x2222) == 0,
           "wide lookup fallbacks");
    expect(T::measureWide(font, ab) == 12 &&
               T::measureWide(font, ab, false) == 13 &&
               T::measureWide(font, az) == 12,
           "wide measure kerning and newline");
    expect(T::measureWide(font, ab, true, 1) == 7 &&
               T::measureWide(font, ab, true, 0) == 0,
           "measure count");
    expect(T::centeredXWide(font, 100, {'Z', 'Z'}) == 95 &&
               T::centeredXAscii(font, 100, {'Z', 'Z'}) == 97,
           "centering mismatch");
    auto g = T::glyphRun(font, 10, 40, ab);
    expect(g.glyphs.size() == 2 && g.glyphs[0].x_half == 20 &&
               g.glyphs[0].y == 42 && g.glyphs[1].x_half == 31 &&
               g.end_x_half == 47,
           "half pixel draw kerning");
    g = T::glyphRun(font, 0, 0, {'A', 10, 'B', 0x2222});
    expect(g.glyphs.size() == 2 && g.glyphs[1].x_half == 14 &&
               g.end_x_half == 36,
           "newline draw and fallback advance");
    expect(T::glyphRun(font, 0, 801, {'A'}).glyphs.empty() &&
               T::glyphRun(font, 0, -100, {'A'}).glyphs.size() == 1,
           "vertical draw reject");
    T::TextState state{0x40ffcc10, 0x1234, -1};
    g = T::glyphRun(font, 0, 0, {'{', 'A', '{'}, -1, &state);
    expect(g.glyphs[0].tint == 0x34606060 && g.glyphs[1].tint == 0x40ffcc10 &&
               g.glyphs[2].tint == 0x40606060 && state.icon_alpha == -1,
           "icon tint restore");
    auto w = T::drawWrapped(
        font, 5, 100, {'A', 'B', ' ', 'A', 'B', ' ', 'A', 'B'}, 32, 0, 448, 17);
    expect(w.runs.size() == 2 && w.runs[0].count == 6 && w.runs[1].start == 6 &&
               w.height == 34 && w.shown == 8,
           "left word wrap");
    w = T::drawWrappedCentered(font, 200, 0,
                               {'A', 'B', ' ', 'A', 'B', ' ', 'A', 'B'}, 32, -1,
                               448, 21);
    expect(w.runs.size() == 2 && w.runs[0].x == 185 && w.runs[1].x == 194 &&
               w.height == 42 && !w.shown,
           "center word wrap");
    w = T::drawWrapped(font, 0, 0, {'A', 'A', 'A', 'A', 'A', 'A'}, 20, -1, 448,
                       10);
    expect(w.runs.size() == 3 && w.runs[0].count == 2 && w.runs[2].start == 4,
           "split long word");
    w = T::drawWrappedCentered(font, 50, 0, {'A', 'A', 'A', 'A', 'A', 'A'}, 20,
                               -1, 448, 10);
    expect(w.runs.size() == 2 && w.runs[0].count == 0 && w.runs[1].x == 29,
           "center long word");
    w = T::drawWrapped(font, 0, 50, {'A', 'B', ' ', 'A', 'B', ' ', 'A', 'B'},
                       32, -1, 448, 10, true, 0, 4);
    expect(w.runs.size() == 1 && w.runs[0].count == 4 && w.shown == 4,
           "typewriter budget");
    w = T::drawWrapped(font, 0, 50, {'A', 'B', ' ', 'A', 'B', ' ', 'A', 'B'},
                       32, 50, 448, 10);
    expect(w.runs.size() == 1 && w.runs[0].y == 60 && w.shown == 8,
           "clip origin and budget");
    reject([&] { T::drawWrapped(font, 0, 0, {'A', 'A'}, 5, -1, 448, 10); },
           "unbreakable word reject");
    expect(T::fitAscii(&font, {'A', 'B', 'A', 'B'}, 20) ==
               std::vector<std::uint8_t>({'A', 'B', 'A', '.', '.', '.'}),
           "pixel fit");
    expect(T::fitAscii(nullptr, {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'}, 6) ==
               std::vector<std::uint8_t>({'A', 'B', 'C', '.', '.', '.'}),
           "character fit");
    auto sc = T::textScissor(10, 20, 30, 40);
    expect(sc.gs_left == 20 && sc.gs_right == 59 && sc.logical.bottom == 41,
           "text scissor");
    expect(std::string(H::stageName(H::Stage::MessageBoss)) == "message_boss" &&
               H::drawSequence()[0] == H::Stage::Gold &&
               H::drawSequence()[9] == H::Stage::NetStatus &&
               H::statusSequence()[10] == std::string("spell selector"),
           "submission sequences");
    expect(H::pulse(16) == 25 && H::pulse(0) == 0 && H::pulse(48) == -25 &&
               H::pulse(80) == 25,
           "pulse period");
    expect(H::pendingLevelColour(-3) == 0x80313b3b &&
               H::pendingLevelColour(3) == 0x80333d3d,
           "pending level truncation");
    expect(H::nameColour(2, 32, 0, true, 0x80a0b0c1, 4) == 0x80505860 &&
               H::nameColour(2, 31, 0, true, 0x80a0b0c1, 4) == 0x80a0b0c1,
           "name blink");
    expect(H::nameWidthBudget(false, true, 20) == 111 &&
               H::nameWidthBudget(true, true, 20) == 131,
           "name width");
    expect(H::glowStep(126) == 128 &&
               H::glowSprite(12, 8, 0).tint == 0x08676767,
           "glow");
    std::array<int, 8> timers{5, 0, 1, 0, 100, 0, 0, 0}, alpha{},
        counts{-1, -1, -1, -1, 3, -1, -1, -1};
    auto nf = H::noticeFrame(0, timers, alpha, counts, 0);
    expect(nf.submissions.size() == 4 && nf.timers[0] == 4 &&
               nf.timers[2] == 0 && nf.timers[4] == 99 &&
               nf.cleared == std::vector<int>{2},
           "notice frame counts");
    expect(std::get<H::Sprite>(nf.submissions[0]).x == 88 &&
               std::get<H::Text>(nf.submissions[2]).colour == 0x044080e4,
           "notice positions and tint");
    expect(H::noticeBaseX(2, true) == 37 && H::noticeBaseX(2, false) == 199 &&
               H::noticeAlphaStep(37, 2) == 0,
           "notice network/fade");
    expect(H::countActiveNotices({1, 1, 0, 0, 1, 0, 0, 0}, -1) == 2,
           "notice suppression");
    auto mf = H::messageFrame(40, 10, false, false, 211);
    expect(mf.alpha == 13 && std::get<H::Frame>(mf.submissions[1]).x == 150 &&
               std::get<H::Frame>(mf.submissions[1]).tint == 0x06000000,
           "message frame");
    expect(H::messageFrame(40, 10, false, true, 211).submissions.empty(),
           "message gate");
    auto bf = H::bossMeterFrame({50, 0, 0, 0, 0});
    expect(std::get<H::Rect>(bf.submissions[1]).x1 == 412 &&
               bf.state.maximum == 50 && bf.state.alpha == 78,
           "boss first frame");
    bf = H::bossMeterFrame({25, 50, 78, 1, 50});
    expect(std::get<H::Rect>(bf.submissions[1]).x1 == 315 &&
               std::get<H::Rect>(bf.submissions[1]).colour == 0x4e808080,
           "boss damage flash");
    auto gf = H::goldFloaterStep(
        {{F::from_int(10), F::from_int(20), 0}, {F::ONE, 0, 0}}, 0);
    expect(gf.position[0] == F::from_int(11) &&
               gf.position[1] == F::from_int(20) &&
               F::lt(F::ONE, gf.velocity[1]),
           "gold spring");
    expect(H::goldFloaterAlpha(110, 100) == 98 &&
               H::goldCounterStep(60, 124) == std::pair<int, int>{59, 128},
           "gold fades");
    expect(std::get<H::Text>(H::goldCounter(3, 8, 30)[0]).x == 560 &&
               H::goldCounter(0, 0, 30).empty(),
           "gold counter");
    expect(H::selectorCentre(1) == std::pair<int, int>{538, 145} &&
               H::selectorCentre(3, true) == std::pair<int, int>{67, 145} &&
               H::selectorCentre(2, false, true) ==
                   std::pair<int, int>{141, 249},
           "selector placement");
    expect(H::selectorDirection(0, -1) == 0 &&
               H::selectorDirection(1, 1) == 3 &&
               H::selectorDirection(0, 0) == -1,
           "selector direction");
    H::SelectorFade sf{0, 0, -1};
    for (int i = 0; i < 8; ++i)
      sf = H::selectorFadeStep(sf, true);
    expect(sf.alpha == 128, "selector open fade");
    int released = 0;
    while (sf.alpha == 128 && released < 40) {
      sf = H::selectorFadeStep(sf, false);
      ++released;
    }
    expect(released == 30 && sf.alpha == 124, "selector release fade");
    expect(H::selectorRadii(128) == std::pair<int, int>{60, 32},
           "selector radii");
    auto ring = H::selectorRing(100, 100, 128);
    expect(ring[0].x == 100 && ring[0].y == 40 && ring[2].x == 160 &&
               ring[2].y == 100 && ring[1].x == 142 && ring[1].y == 58,
           "selector ring");
    expect(H::selectorHighlight(16, 0x40) ==
               std::pair<std::uint32_t, std::uint32_t>{0x90808080, 0x40ffffff},
           "selector highlight");
    H::StageRecords stages;
    stages[0].emplace_back(H::Sprite{"gold", 0, 0, 6, 0});
    stages[1].emplace_back(H::Sprite{"status", 0, 0, 6, 0});
    auto composed = H::composeHud(stages);
    expect(std::get<H::Sprite>(composed[0]).resource == "gold" &&
               std::get<H::Sprite>(composed[1]).resource == "status" &&
               H::painterIndices(composed) == std::vector<std::size_t>({1, 0}),
           "HUD stage and painter order");
    C::Textures tex{
        {"a", solid(4, 2, {200, 100, 50, 128})},
        {"b", solid(4, 2, {10, 20, 30, 128})},
        {"ramp", {3, 1, {1, 0, 0, 128, 2, 0, 0, 128, 3, 0, 0, 128}}}};
    auto a = std::get<C::Blit>(
        C::normalize(ui_layout::Draw{"a", 5, 7, false, {}}, tex));
    expect(a.y == 8 && a.src.width == 4, "plain sprite y offset");
    auto b = std::get<C::Blit>(C::normalize(
        ui_layout::HudLayer{{"b", 5, 7, true, {0, 0, 640, 447}}, 0x80808080, 6},
        tex));
    expect(b.y == 7, "clipped sprite no offset");
    auto fb = C::composite({C::Blit{"a", 10, 10, {0, 0, 4, 2}},
                            C::Blit{"b", 10, 10, {0, 0, 4, 2}}},
                           tex);
    expect(pixel(fb, 10, 10) == C::Pixel{200, 100, 50, 128},
           "reverse submission painter");
    C::Textures hud_tex{{"bar_red00.tex", solid(100, 8, {255, 0, 0, 128})},
                        {"hud_multi.tex", solid(158, 46, {0, 0, 255, 128})}};
    std::vector<C::Unit> hud_units;
    for (const auto &layer :
         ui_layout::hudBaseLayers(12, 0, 2, 4, 1, 1, 0, 0, 1, 1, false))
      hud_units.push_back(C::normalize(layer, hud_tex));
    fb = C::composite(hud_units, hud_tex);
    expect(pixel(fb, 27, 46)[0] == 255 && pixel(fb, 77, 46)[2] == 255,
           "base HUD frame behind fill");
    fb = C::composite(
        {C::Blit{
            "ramp", 0, 0, {0, 0, 3, 1}, std::nullopt, 0x80808080, 6, true}},
        tex);
    expect(pixel(fb, 0, 0)[0] == 3 && pixel(fb, 2, 0)[0] == 1, "blit flip");
    fb = C::composite({C::Rect{3, 4, 6, 6, 6, 0x80000060}}, tex);
    expect(pixel(fb, 3, 4)[0] == 0x60 && pixel(fb, 6, 5)[0] == 0,
           "rect corners");
    fb = C::composite(
        {C::Blit{"a", 0, 0, {0, 0, 4, 2}, std::nullopt, 0x80404040}}, tex);
    expect(pixel(fb, 0, 0)[0] == 100, "tint modulation");
    fb = C::composite(
        {C::Blit{"a", 0, 0, {0, 0, 4, 2}, std::nullopt, 0x80ffffff}}, tex);
    expect(pixel(fb, 0, 0)[0] == 255 && pixel(fb, 0, 0)[1] == 199 &&
               pixel(fb, 0, 0)[2] == 99,
           "bright tint clamp");
    tex["half"] = solid(2, 2, {255, 255, 255, 64});
    fb = C::composite({C::Blit{"half", 0, 0, {0, 0, 2, 2}}}, tex,
                      {100, 0, 200, 128});
    expect(pixel(fb, 0, 0) == C::Pixel{177, 127, 227, 64}, "half alpha blend");
    tex["clear"] = solid(2, 2, {255, 0, 0, 1});
    fb = C::composite({C::Blit{"clear", 0, 0, {0, 0, 2, 2}}}, tex,
                      {9, 9, 9, 128});
    expect(pixel(fb, 0, 0) == C::Pixel{9, 9, 9, 128}, "alpha test");
    fb =
        C::composite({C::Rect{0, 0, 1, 1, 6, 0x01000000}}, tex, {1, 0, 0, 128});
    expect(pixel(fb, 0, 0)[0] == 0, "negative blend floors");
    fb = C::composite(
        {C::Blit{"ramp", 20, 5, {0, 0, 3, 1}, ui_layout::Clip{20, 0, 22, 447}}},
        tex);
    expect(pixel(fb, 20, 5)[0] == 1 && pixel(fb, 21, 5)[0] == 2 &&
               pixel(fb, 22, 5)[0] == 0,
           "clip cuts without stretch");
    expect(C::clampClip(ui_layout::Clip{-5, -1, 1000, 1000}).bottom == 447,
           "clip clamp");
    fb = C::composite(
        {C::Blit{"a", 0, 446, {0, 0, 4, 2}, ui_layout::Clip{0, 0, 1000, 1000}}},
        tex);
    expect(pixel(fb, 0, 446)[0] == 200 && pixel(fb, 0, 447)[0] == 0,
           "bottom clip exclusive");
    fb = C::composite({C::TextBatch{{C::Blit{"a", 0, 0, {0, 0, 4, 2}},
                                     C::Blit{"b", 2, 0, {0, 0, 4, 2}}},
                                    6}},
                      tex);
    expect(pixel(fb, 2, 0)[0] == 10, "text batch forward glyphs");
    fb = C::composite(
        {C::Blit{"a", 10, 10, {0, 0, 4, 2}, std::nullopt, 0x80808080, 7},
         C::Blit{"b", 10, 10, {0, 0, 4, 2}, std::nullopt, 0x80808080, 6}},
        tex);
    expect(pixel(fb, 10, 10)[0] == 200, "higher layer paints above lower");
    reject([&] { C::painterOrder({C::Rect{0, 0, 1, 1, 8, 0}}); },
           "invalid sort layer");
    std::vector<std::uint8_t> aa(8 * 4 * 4, 0), bb = aa;
    bb[4 * (1 * 8 + 2)] = 5;
    bb[4 * (2 * 8 + 6) + 2] = 40;
    auto report = C::compare(aa, bb, 8, 4, 8);
    expect(report.max_delta == 40 && report.over == 1 &&
               *report.bbox == std::array<int, 4>{6, 2, 7, 3},
           "masked comparator threshold");
    expect(C::compare(aa, bb, 8, 4, 0, {{2, 1, 3, 2}}).over == 1,
           "comparator mask");
    auto shifted = aa;
    for (int x = 0; x < 8; ++x) {
      aa[4 * (1 * 8 + x)] = 200;
      shifted[4 * (2 * 8 + x)] = 200;
    }
    expect(C::compare(aa, shifted, 8, 4).over == 16 &&
               C::compare(aa, shifted, 8, 4, 0, {}, 1).over == 0,
           "vertical slack");
    reject([&] { C::compare(aa, std::vector<std::uint8_t>(4), 8, 4); },
           "comparison size guard");
    expect(C::diffRecords({a, b}, {a, b}).empty(), "record parity");
    auto changed = b;
    changed.y = 8;
    auto diff = C::diffRecords({a, b}, {a, changed});
    expect(diff.size() == 1 && diff[0].op == "replace" &&
               std::get<C::Blit>(diff[0].original[0]).y == 7,
           "record difference region");
    constexpr int n = 2, go = 0x214, ko = go + n * 16, to = ko;
    std::vector<std::uint8_t> hud_font(to + 0x40);
    put16(hud_font, 0, n);
    put16(hud_font, 2, 8);
    put32(hud_font, 8, go);
    put32(hud_font, 12, ko);
    put32(hud_font, 16, to);
    for (int i = 0; i < 256; ++i)
      put16(hud_font, 0x14 + 2 * i, -1);
    put16(hud_font, 0x14 + 2 * 'A', 1);
    const int glyphs[2][8] = {{0, 0, 0, 0, 0, 0, 3, -1}, {65, 0, 1, 0, 1, 0, 4, -1}};
    for (int i = 0; i < 2; ++i)
      for (int k = 0; k < 8; ++k)
        put16(hud_font, go + i * 16 + k * 2, glyphs[i][k]);
    Bytes view{hud_font.data(), hud_font.size()};
    // The tint is a required argument (audit FONT-1). 0x80323c3c is the status
    // text base colour (research-ui 3e39392 docs/hud-dynamic.md section 3).
    auto drawn = T::hudTextGlyphs(view, {'A'}, 73, 17, 0x80323c3c);
    expect(drawn.size() == 1 && drawn[0].x_half == 142 && drawn[0].y == 17 &&
               drawn[0].source.x == 0 && drawn[0].source.y == 0 &&
               drawn[0].source.width == 1 && drawn[0].source.height == 1 &&
               drawn[0].tint == 0x80323c3c,
           "HUD name glyph is centered in half pixels");
    auto tinted = T::hudTextGlyphs(view, {'A'}, 134, 44, 0x80ffffff);
    expect(tinted.size() == 1 && tinted[0].y == 44 && tinted[0].tint == 0x80ffffff,
           "caller tint is kept");
    expect(T::hudTextGlyphs(view, {}, 73, 17, 0x80323c3c).empty() &&
               T::hudTextGlyphs(view, {0}, 73, 17, 0x80323c3c).empty(),
           "empty HUD text draws nothing");
    hud_font[2] = 99;
    expect(T::hudTextGlyphs(view, {'A'}, 73, 17, 0x80323c3c)[0].x_half == 142,
           "header +2 does not move the glyph");
    if (argc < 2)
      throw std::runtime_error("hud_r3 vectors directory required");
    checkHudRound3(std::string(argv[1]) + "/hud_r3.tsv");
    checkRound3Extras();
    std::cout << "ui dynamic/text/compositor exact synthetic checks: " << checks
              << " passed\n";
  } catch (const std::exception &e) {
    std::cerr << "check " << checks + 1 << ": " << e.what() << '\n';
    return 1;
  }
}
