#pragma once
#include "champions/core_arithmetic.hpp"
#include "champions/ui_layout.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace champions::ui_composite {
constexpr int WIDTH = 640, HEIGHT = 448;
using Pixel = std::array<std::uint8_t, 4>;
struct Texture {
  int width{}, height{};
  std::vector<std::uint8_t> rgba;
};
struct Blit {
  std::string resource;
  int x{}, y{};
  ui_layout::Rect src{};
  std::optional<ui_layout::Clip> clip;
  std::uint32_t tint{0x80808080};
  int layer{6};
  bool flip_x{}, flip_y{};
};
struct Rect {
  int x0{}, y0{}, x1{}, y1{}, layer{6};
  std::uint32_t colour{};
};
struct TextBatch {
  std::vector<Blit> glyphs;
  int layer{6};
};
using Unit = std::variant<Blit, Rect, TextBatch>;
using Textures = std::map<std::string, Texture>;
ui_layout::Clip clampClip(std::optional<ui_layout::Clip>);
Unit normalize(const ui_layout::Draw &, const Textures &, int layer = 6);
Unit normalize(const ui_layout::HudLayer &, const Textures &);
std::vector<Unit> painterOrder(const std::vector<Unit> &);
std::vector<std::uint8_t> composite(const std::vector<Unit> &, const Textures &,
                                    Pixel background = {0, 0, 0, 128});
struct Report {
  int max_delta{}, over{}, total{};
  std::optional<std::array<int, 4>> bbox;
  std::optional<std::array<int, 2>> first;
};
Report compare(const std::vector<std::uint8_t> &a,
               const std::vector<std::uint8_t> &b, int width = WIDTH,
               int height = HEIGHT, int tolerance = 0,
               const std::vector<std::array<int, 4>> &mask = {}, int slack = 0);
struct RecordDifference {
  std::string op;
  std::vector<Unit> original, native;
};
std::vector<RecordDifference> diffRecords(const std::vector<Unit> &original,
                                          const std::vector<Unit> &native);

// GS DIMX / present gain (docs/ui-parity.md). composite() does not apply these.
constexpr std::uint64_t DIMX_WORD = 0x3131757531317575ull;
constexpr int DIMX_REGISTER = 0x44;
constexpr int DTHE_REGISTER = 0x45;
constexpr std::uint32_t BRIGHTNESS_DEFAULT = 0x3f000000u;
std::array<std::array<int, 4>, 4> dimxMatrix();
int dither5(int channel, int x, int y);
std::uint32_t scaleRgb(std::uint32_t colour, simulation::rta::Raw factor);
simulation::rta::Raw presentGain(simulation::rta::Raw brightness);
int presentChannel(int value, simulation::rta::Raw brightness);
std::uint32_t presentColour(int r, int g, int b, simulation::rta::Raw brightness);
int presentChannelStep(int value);
} // namespace champions::ui_composite
