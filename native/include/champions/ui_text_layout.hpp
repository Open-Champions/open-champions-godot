#pragma once
#include "champions/ui_layout.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace champions::ui_text_layout {
using Codes = std::vector<std::uint16_t>;
struct GlyphDraw {
  int index{}, x_half{}, y{};
  ui_layout::Rect source{};
  std::uint32_t tint{};
};
struct TextState {
  std::uint32_t colour{0x80808080};
  int icon_alpha{-1}, stored_alpha{-1};
  void setColour(std::uint32_t value, int alpha = -1);
  std::uint32_t iconTint() const;
};
struct GlyphRun {
  std::vector<GlyphDraw> glyphs;
  int end_x_half{};
};
struct Run {
  int x{}, y{};
  std::size_t start{}, count{};
};
struct Wrapped {
  std::vector<Run> runs;
  int height{};
  std::optional<std::size_t> shown;
};
struct Scissor {
  int gs_left{}, gs_right{}, gs_top{}, gs_bottom{};
  ui_layout::Clip logical{};
};
int glyphIndexWide(const ui_layout::FontMetrics &, std::uint16_t code);
int measureWide(const ui_layout::FontMetrics &, const Codes &,
                bool interlaced = true,
                std::size_t count = static_cast<std::size_t>(-1));
GlyphRun glyphRun(const ui_layout::FontMetrics &, int x, int y, const Codes &,
                  std::size_t count = static_cast<std::size_t>(-1),
                  TextState *state = nullptr);
Wrapped drawWrapped(const ui_layout::FontMetrics &, int x, int y, const Codes &,
                    int max_width, int clip_top, int clip_bottom,
                    int line_height, bool interlaced = true, int language = 0,
                    std::size_t budget = 100000);
Wrapped drawWrappedCentered(const ui_layout::FontMetrics &, int x, int y,
                            const Codes &, int max_width, int clip_top,
                            int clip_bottom, int line_height,
                            bool interlaced = true, int language = 0);
int centeredXWide(const ui_layout::FontMetrics &, int x, const Codes &,
                  bool interlaced = true,
                  std::size_t count = static_cast<std::size_t>(-1));
int centeredXAscii(const ui_layout::FontMetrics &, int x,
                   const std::vector<std::uint8_t> &, bool interlaced = true,
                   std::size_t count = static_cast<std::size_t>(-1));
// Centered HUD string. anchor_x is the documented center. The pen is the
// existing half-pixel glyph run. Empty or NUL-only text draws nothing.
// Header +2 is not a line height; the caller supplies y. The tint has no
// default: each HUD call site computes its own colour (audit FONT-1).
std::vector<GlyphDraw> hudTextGlyphs(Bytes fnt, const std::vector<std::uint8_t> &ascii,
                                     int anchor_x, int text_y, std::uint32_t tint);
std::vector<std::uint8_t> fitAscii(const ui_layout::FontMetrics *,
                                   const std::vector<std::uint8_t> &, int width,
                                   bool interlaced = true);
Scissor textScissor(int left, int top, int right, int bottom);
} // namespace champions::ui_text_layout
