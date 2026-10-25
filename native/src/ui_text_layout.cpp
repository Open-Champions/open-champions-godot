#include "champions/ui_text_layout.hpp"
#include <algorithm>
#include <climits>
#include <limits>
#include <stdexcept>

namespace champions::ui_text_layout {
namespace {
int checked(long long n) {
  if (n < INT_MIN || n > INT_MAX)
    throw std::out_of_range("UI text coordinate overflow");
  return int(n);
}
int shift(int n, unsigned bits) {
  return n >= 0 ? n >> bits
                : -int(((-static_cast<long long>(n)) + ((1LL << bits) - 1)) >>
                       bits);
}
std::size_t length(const Codes &c, std::size_t count) {
  std::size_t n = 0;
  while (n < c.size() && n < count && c[n])
    ++n;
  return n;
}
std::uint16_t code(const Codes &c, std::size_t i) {
  return i < c.size() ? c[i] : 0;
}
int kern(const ui_layout::FontMetrics &f, int index, int previous) {
  for (int k = f.glyphs.at(index).kern_start;
       k >= 0 && k < int(f.kerns.size()) && f.kerns[k].current == index; ++k)
    if (f.kerns[k].previous == previous)
      return f.kerns[k].adjustment;
  return 0;
}
std::size_t wordEnd(const ui_layout::FontMetrics &f, const Codes &c,
                    std::size_t cur, int width, bool interlaced, int language,
                    bool split) {
  if (language == 5)
    return cur + (code(c, cur) ? 1 : 0);
  std::size_t end = cur;
  while (true) {
    const auto ch = code(c, end);
    if (ch == 0 || ch == 0x20 || ch == 0x0a)
      break;
    if (split) {
      Codes tail(c.begin() + cur, c.end());
      if (measureWide(f, tail, interlaced, end - cur) < width) {
        ++end;
        continue;
      }
      if (end == cur)
        return cur; // original loops forever; reject at caller
      --end;
      break;
    }
    ++end;
  }
  return code(c, end) == 0x20 ? end + 1 : end;
}
Wrapped wrap(const ui_layout::FontMetrics &f, int x, int y, const Codes &c,
             int max_width, int top, int bottom, int line_height,
             bool interlaced, int language, std::size_t budget, bool centered) {
  if (max_width <= 0)
    throw std::invalid_argument("nonpositive wrap width");
  const int y0 = y;
  int line_w = 0;
  std::size_t cur = 0, start = 0, used = 0, shown = 0;
  bool stop = false;
  std::vector<Run> runs;
  auto flush = [&]() {
    const auto count =
        centered ? cur - start
                 : std::min(cur - start,
                            budget > used ? budget - used : std::size_t(0));
    if (top < y && y < bottom && !stop) {
      Codes tail(c.begin() + start, c.end());
      runs.push_back(
          {centered ? x - shift(measureWide(f, tail, interlaced, count), 1) : x,
           y, start, count});
      if (!centered)
        shown = used + count;
    }
    if (!centered) {
      used += count;
      stop = stop || used >= budget;
    }
  };
  while (code(c, cur)) {
    auto end = wordEnd(f, c, cur, max_width, interlaced, language, !centered);
    if (end == cur && code(c, cur) != 0x0a)
      throw std::invalid_argument("unbreakable unit exceeds wrap width");
    Codes tail(c.begin() + cur, c.end());
    const int width = measureWide(f, tail, interlaced, end - cur);
    if (static_cast<long long>(line_w) + width < max_width)
      line_w += width;
    else {
      flush();
      start = cur;
      y = checked(static_cast<long long>(y) + line_height);
      line_w = width;
    }
    if (code(c, end) == 0x0a) {
      line_w = checked(static_cast<long long>(line_w) + 1000000);
      ++end;
    }
    cur = end;
  }
  if (start < cur) {
    flush();
    y = checked(static_cast<long long>(y) + line_height);
  }
  return {runs, checked(static_cast<long long>(y) - y0),
          centered ? std::nullopt : std::optional<std::size_t>(shown)};
}
} // namespace
void TextState::setColour(std::uint32_t value, int alpha) {
  colour = value;
  icon_alpha = stored_alpha = alpha;
}
std::uint32_t TextState::iconTint() const {
  return (std::uint32_t(icon_alpha == -1 ? colour >> 24 : icon_alpha & 255)
          << 24) |
         0x606060;
}
int glyphIndexWide(const ui_layout::FontMetrics &f, std::uint16_t ch) {
  if (ch == 0x0a)
    return 0x7ff;
  if (ch < 256) {
    int index = f.charmap[ch];
    return index >= 0 ? index : f.charmap[45] & 65535;
  }
  for (std::size_t i = 0; i < f.glyphs.size(); ++i)
    if (f.glyphs[i].code == ch)
      return checked(i);
  return 0;
}
int measureWide(const ui_layout::FontMetrics &f, const Codes &c,
                bool interlaced, std::size_t count) {
  if (f.glyphs.empty())
    throw std::invalid_argument("font has no glyph zero");
  long long pen = 0;
  int previous = -1;
  for (std::size_t i = 0, n = length(c, count); i < n; ++i) {
    int index = glyphIndexWide(f, c[i]);
    if (index == 0x7ff)
      continue;
    if (index <= 0)
      pen += static_cast<long long>(f.glyphs[0].advance) * 16;
    else {
      if (index >= int(f.glyphs.size()))
        throw std::out_of_range("wide glyph map index");
      pen += static_cast<long long>(kern(f, index, previous)) *
                 (interlaced ? 16 : 8) +
             static_cast<long long>(f.glyphs[index].advance) * 16;
    }
    previous = index;
  }
  return checked(pen >= 0 ? pen / 16 : -((-pen + 15) / 16));
}
GlyphRun glyphRun(const ui_layout::FontMetrics &f, int x, int y, const Codes &c,
                  std::size_t count, TextState *supplied) {
  TextState temporary;
  auto &state = supplied ? *supplied : temporary;
  int pen = checked(static_cast<long long>(x) * 2);
  std::vector<GlyphDraw> out;
  if (count == 0 || y < -100 || y > 800)
    return {out, pen};
  int previous = -1;
  bool icons = false;
  for (std::size_t i = 0, n = length(c, count); i < n; ++i) {
    const int index = glyphIndexWide(f, c[i]);
    if (index == 0x7ff) {
      previous = index;
      continue;
    }
    if (index <= 0) {
      pen = checked(static_cast<long long>(pen) + 2 * f.glyphs.at(0).advance);
      previous = index;
      continue;
    }
    if (index >= int(f.glyphs.size()))
      throw std::out_of_range("wide glyph map index");
    const auto &g = f.glyphs[index];
    pen = checked(static_cast<long long>(pen) + kern(f, index, previous));
    std::uint32_t tint;
    if (index >= 0x5b && index <= 0x68) {
      tint = state.iconTint();
      icons = true;
    } else {
      if (icons) {
        state.icon_alpha = state.stored_alpha;
        icons = false;
      }
      tint = state.colour;
    }
    out.push_back({index,
                   pen,
                   checked(static_cast<long long>(y) + g.y_offset),
                   {g.u0, g.v0, g.u1 - g.u0, g.v1 - g.v0},
                   tint});
    pen = checked(static_cast<long long>(pen) + 2 * g.advance);
    previous = index;
  }
  if (icons)
    state.icon_alpha = state.stored_alpha;
  return {out, pen};
}
Wrapped drawWrapped(const ui_layout::FontMetrics &f, int x, int y,
                    const Codes &c, int width, int top, int bottom, int height,
                    bool interlaced, int language, std::size_t budget) {
  return wrap(f, x, y, c, width, top, bottom, height, interlaced, language,
              budget, false);
}
Wrapped drawWrappedCentered(const ui_layout::FontMetrics &f, int x, int y,
                            const Codes &c, int width, int top, int bottom,
                            int height, bool interlaced, int language) {
  return wrap(f, x, y, c, width, top, bottom, height, interlaced, language,
              100000, true);
}
int centeredXWide(const ui_layout::FontMetrics &f, int x, const Codes &c,
                  bool interlaced, std::size_t count) {
  return checked(static_cast<long long>(x) -
                 shift(measureWide(f, c, interlaced, count), 1));
}
int centeredXAscii(const ui_layout::FontMetrics &f, int x,
                   const std::vector<std::uint8_t> &c, bool interlaced,
                   std::size_t count) {
  return checked(static_cast<long long>(x) -
                 shift(f.widthAscii(c, interlaced, count), 1));
}
std::vector<GlyphDraw> hudTextGlyphs(Bytes fnt, const std::vector<std::uint8_t> &ascii,
                                     int anchor_x, int text_y, std::uint32_t tint) {
  const auto font = ui_layout::fontMetrics(fnt);
  if (ascii.empty() || ascii.front() == 0)
    return {};
  const int left = centeredXAscii(font, anchor_x, ascii, true);
  Codes units;
  units.reserve(ascii.size());
  for (auto byte : ascii) {
    if (!byte)
      break;
    units.push_back(byte);
  }
  TextState state;
  state.colour = tint;
  return glyphRun(font, left, text_y, units, static_cast<std::size_t>(-1), &state).glyphs;
}
std::vector<std::uint8_t> fitAscii(const ui_layout::FontMetrics *f,
                                   const std::vector<std::uint8_t> &text,
                                   int width, bool interlaced) {
  if (width < 0)
    throw std::invalid_argument("negative fit width");
  auto it = std::find(text.begin(), text.end(), std::uint8_t(0));
  std::vector<std::uint8_t> out(text.begin(), it);
  const std::size_t original = out.size();
  if (!f) {
    if (width < int(out.size()))
      while (!out.empty() && width < int(out.size()) + 3)
        out.pop_back();
  } else
    while (!out.empty() && width < f->widthAscii(out, interlaced))
      out.pop_back();
  if (out.size() != original)
    out.insert(out.end(), {'.', '.', '.'});
  return out;
}
Scissor textScissor(int l, int t, int r, int b) {
  l = std::max(l, 0);
  t = std::max(t, 0);
  r = std::min(r, 640);
  b = std::min(b, 447);
  return {checked(2LL * l),
          checked(2LL * r - 1),
          t,
          b,
          {l, t, r, checked(static_cast<long long>(b) + 1)}};
}
} // namespace champions::ui_text_layout
