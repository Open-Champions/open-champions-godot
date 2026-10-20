#include "champions/hud_dynamic.hpp"
#include "champions/ui_composite.hpp"
#include "champions/ui_text_layout.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
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
int main() {
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
    auto drawn = T::hudTextGlyphs(view, {'A'}, 73, 17);
    expect(drawn.size() == 1 && drawn[0].x_half == 142 && drawn[0].y == 17 &&
               drawn[0].source.x == 0 && drawn[0].source.y == 0 &&
               drawn[0].source.width == 1 && drawn[0].source.height == 1 &&
               drawn[0].tint == 0x80808080,
           "HUD name glyph is centered in half pixels");
    auto tinted = T::hudTextGlyphs(view, {'A'}, 134, 44, 0x80ffffff);
    expect(tinted.size() == 1 && tinted[0].y == 44 && tinted[0].tint == 0x80ffffff,
           "caller tint is kept");
    expect(T::hudTextGlyphs(view, {}, 73, 17).empty() &&
               T::hudTextGlyphs(view, {0}, 73, 17).empty(),
           "empty HUD text draws nothing");
    hud_font[2] = 99;
    expect(T::hudTextGlyphs(view, {'A'}, 73, 17)[0].x_half == 142,
           "header +2 does not move the glyph");
    std::cout << "ui dynamic/text/compositor exact synthetic checks: " << checks
              << " passed\n";
  } catch (const std::exception &e) {
    std::cerr << "check " << checks + 1 << ": " << e.what() << '\n';
    return 1;
  }
}
