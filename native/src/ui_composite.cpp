#include "champions/ui_composite.hpp"
#include "champions/core_arithmetic.hpp"
#include <algorithm>
#include <stdexcept>

namespace champions::ui_composite {
namespace {
int layerOf(const Unit &u) {
  return std::visit([](const auto &v) { return v.layer; }, u);
}
bool equalClip(const std::optional<ui_layout::Clip> &a,
               const std::optional<ui_layout::Clip> &b) {
  if (a.has_value() != b.has_value())
    return false;
  return !a || (a->left == b->left && a->top == b->top &&
                a->right == b->right && a->bottom == b->bottom);
}
bool equalBlit(const Blit &a, const Blit &b) {
  return a.resource == b.resource && a.x == b.x && a.y == b.y &&
         a.src.x == b.src.x && a.src.y == b.src.y &&
         a.src.width == b.src.width && a.src.height == b.src.height &&
         equalClip(a.clip, b.clip) && a.tint == b.tint && a.layer == b.layer &&
         a.flip_x == b.flip_x && a.flip_y == b.flip_y;
}
bool equalUnit(const Unit &a, const Unit &b) {
  if (a.index() != b.index())
    return false;
  if (auto x = std::get_if<Blit>(&a))
    return equalBlit(*x, std::get<Blit>(b));
  if (auto x = std::get_if<Rect>(&a)) {
    const auto &y = std::get<Rect>(b);
    return x->x0 == y.x0 && x->y0 == y.y0 && x->x1 == y.x1 && x->y1 == y.y1 &&
           x->layer == y.layer && x->colour == y.colour;
  }
  const auto &x = std::get<TextBatch>(a), &y = std::get<TextBatch>(b);
  if (x.layer != y.layer || x.glyphs.size() != y.glyphs.size())
    return false;
  for (std::size_t i = 0; i < x.glyphs.size(); ++i)
    if (!equalBlit(x.glyphs[i], y.glyphs[i]))
      return false;
  return true;
}
std::array<int, 4> unpack(std::uint32_t c) {
  return {int(c & 255), int(c >> 8 & 255), int(c >> 16 & 255),
          int(c >> 24 & 255)};
}
void blend(std::vector<std::uint8_t> &fb, std::size_t at, int r, int g, int b,
           int a) {
  const int c[3] = {r, g, b};
  for (int k = 0; k < 3; ++k) {
    const int cd = fb[at + k], d = (c[k] - cd) * a;
    const int shifted = d >= 0 ? d / 128 : -((-d + 127) / 128);
    fb[at + k] = std::clamp(shifted + cd, 0, 255);
  }
  fb[at + 3] = std::min(a, 255);
}
void drawBlit(std::vector<std::uint8_t> &fb, const Blit &u,
              const Textures &textures) {
  const auto it = textures.find(u.resource);
  if (it == textures.end())
    throw std::out_of_range("missing compositor texture");
  const auto &t = it->second;
  if (t.width < 0 || t.height < 0 ||
      t.rgba.size() != std::size_t(t.width) * t.height * 4)
    throw std::invalid_argument("invalid compositor texture");
  const int sx = u.src.x, sy = u.src.y, sw = u.src.width, sh = u.src.height;
  if (sx < 0 || sy < 0 || sw < 0 || sh < 0 || sx > t.width - sw ||
      sy > t.height - sh)
    throw std::out_of_range("source outside texture");
  const auto c = clampClip(u.clip);
  const auto tint = unpack(u.tint);
  for (int dy = 0; dy < sh; ++dy) {
    const long long y = static_cast<long long>(u.y) + dy;
    if (y < c.top || y >= c.bottom || y < 0 || y >= HEIGHT)
      continue;
    const int ty = sy + (u.flip_y ? sh - 1 - dy : dy);
    for (int dx = 0; dx < sw; ++dx) {
      const long long x = static_cast<long long>(u.x) + dx;
      if (x < c.left || x >= c.right || x < 0 || x >= WIDTH)
        continue;
      const int tx = sx + (u.flip_x ? sw - 1 - dx : dx);
      const auto i = 4 * (std::size_t(ty) * t.width + tx);
      const auto j = 4 * (std::size_t(y) * WIDTH + x);
      const int a = std::min(255, int(t.rgba[i + 3]) * tint[3] >> 7);
      if (a <= 1)
        continue;
      blend(fb, j, std::min(255, int(t.rgba[i]) * tint[0] >> 7),
            std::min(255, int(t.rgba[i + 1]) * tint[1] >> 7),
            std::min(255, int(t.rgba[i + 2]) * tint[2] >> 7), a);
    }
  }
}
void drawRect(std::vector<std::uint8_t> &fb, const Rect &u) {
  const auto colour = unpack(u.colour);
  for (int y = std::max(u.y0, 0); y < std::min(u.y1, HEIGHT); ++y)
    for (int x = std::max(u.x0, 0); x < std::min(u.x1, WIDTH); ++x)
      blend(fb, 4 * (std::size_t(y) * WIDTH + x), colour[0], colour[1],
            colour[2], colour[3]);
}
} // namespace
ui_layout::Clip clampClip(std::optional<ui_layout::Clip> c) {
  if (!c)
    return {0, 0, WIDTH, HEIGHT};
  return {std::max(c->left, 0), std::max(c->top, 0), std::min(c->right, WIDTH),
          std::min(c->bottom, 447)};
}
Unit normalize(const ui_layout::Draw &d, const Textures &tex, int layer) {
  const auto it = tex.find(d.resource);
  if (it == tex.end())
    throw std::out_of_range("missing compositor texture");
  return Blit{d.resource,
              d.x,
              d.y + (d.clipped ? 0 : 1),
              {0, 0, it->second.width, it->second.height},
              d.clipped ? std::optional<ui_layout::Clip>(d.clip) : std::nullopt,
              0x80808080,
              layer};
}
Unit normalize(const ui_layout::HudLayer &h, const Textures &tex) {
  auto b = std::get<Blit>(normalize(h.draw, tex, h.layer));
  b.y = h.draw.y;
  b.tint = h.tint;
  return b;
}
std::vector<Unit> painterOrder(const std::vector<Unit> &in) {
  std::vector<Unit> out;
  out.reserve(in.size());
  for (const auto &u : in)
    if (layerOf(u) < 0 || layerOf(u) > 7)
      throw std::out_of_range("sort layer outside 0..7");
  for (int l = 0; l < 8; ++l)
    for (auto it = in.rbegin(); it != in.rend(); ++it)
      if (layerOf(*it) == l)
        out.push_back(*it);
  return out;
}
std::vector<std::uint8_t> composite(const std::vector<Unit> &records,
                                    const Textures &textures, Pixel bg) {
  std::vector<std::uint8_t> fb(4 * WIDTH * HEIGHT);
  for (std::size_t i = 0; i < fb.size(); i += 4)
    std::copy(bg.begin(), bg.end(), fb.begin() + i);
  for (const auto &u : painterOrder(records)) {
    if (auto b = std::get_if<Blit>(&u))
      drawBlit(fb, *b, textures);
    else if (auto r = std::get_if<Rect>(&u))
      drawRect(fb, *r);
    else
      for (const auto &g : std::get<TextBatch>(u).glyphs)
        drawBlit(fb, g, textures);
  }
  return fb;
}
Report compare(const std::vector<std::uint8_t> &a,
               const std::vector<std::uint8_t> &b, int width, int height,
               int tolerance, const std::vector<std::array<int, 4>> &mask,
               int slack) {
  if (width <= 0 || height <= 0 || tolerance < 0 || slack < 0 ||
      a.size() != b.size() || a.size() != std::size_t(width) * height * 4)
    throw std::invalid_argument("invalid compositor compare buffers");
  Report out;
  auto delta = [&](std::size_t i, std::size_t j) {
    return std::max({std::abs(int(a[i]) - int(b[j])),
                     std::abs(int(a[i + 1]) - int(b[j + 1])),
                     std::abs(int(a[i + 2]) - int(b[j + 2]))});
  };
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      bool hidden = false;
      for (auto m : mask)
        if (m[0] <= x && x < m[2] && m[1] <= y && y < m[3]) {
          hidden = true;
          break;
        }
      if (hidden)
        continue;
      ++out.total;
      const auto i = 4 * (std::size_t(y) * width + x);
      int d = delta(i, i);
      if (d > tolerance && slack)
        for (int s = 1; s <= slack; ++s) {
          bool match = false;
          for (int yy : {y - s, y + s})
            if (yy >= 0 && yy < height &&
                delta(i, 4 * (std::size_t(yy) * width + x)) <= tolerance)
              match = true;
          if (match) {
            d = tolerance;
            break;
          }
        }
      out.max_delta = std::max(out.max_delta, d);
      if (d > tolerance) {
        ++out.over;
        if (!out.first)
          out.first = std::array<int, 2>{x, y};
        if (!out.bbox)
          out.bbox = std::array<int, 4>{x, y, x + 1, y + 1};
        else {
          auto &box = *out.bbox;
          box[0] = std::min(box[0], x);
          box[1] = std::min(box[1], y);
          box[2] = std::max(box[2], x + 1);
          box[3] = std::max(box[3], y + 1);
        }
      }
    }
  return out;
}
std::vector<RecordDifference> diffRecords(const std::vector<Unit> &original,
                                          const std::vector<Unit> &native) {
  const auto a = painterOrder(original), b = painterOrder(native);
  if (a.size() * b.size() > 4000000)
    throw std::length_error("record diff input too large");
  struct Match {
    std::size_t i, j, n;
  };
  struct Range {
    std::size_t alo, ahi, blo, bhi;
  };
  std::vector<Range> queue{{0, a.size(), 0, b.size()}};
  std::vector<Match> found;
  while (!queue.empty()) {
    const auto r = queue.back();
    queue.pop_back();
    std::size_t best_i = r.alo, best_j = r.blo, best_n = 0;
    std::vector<std::size_t> prev(b.size() + 1), curr(b.size() + 1);
    for (std::size_t i = r.alo; i < r.ahi; ++i) {
      std::fill(curr.begin(), curr.end(), 0);
      for (std::size_t j = r.blo; j < r.bhi; ++j)
        if (equalUnit(a[i], b[j])) {
          curr[j + 1] = prev[j] + 1;
          if (curr[j + 1] > best_n) {
            best_i = i + 1 - curr[j + 1];
            best_j = j + 1 - curr[j + 1];
            best_n = curr[j + 1];
          }
        }
      prev.swap(curr);
    }
    if (!best_n)
      continue;
    found.push_back({best_i, best_j, best_n});
    if (r.alo < best_i && r.blo < best_j)
      queue.push_back({r.alo, best_i, r.blo, best_j});
    if (best_i + best_n < r.ahi && best_j + best_n < r.bhi)
      queue.push_back({best_i + best_n, r.ahi, best_j + best_n, r.bhi});
  }
  std::sort(found.begin(), found.end(), [](const Match &x, const Match &y) {
    return x.i < y.i || (x.i == y.i && x.j < y.j);
  });
  std::vector<Match> blocks;
  for (const auto &m : found) {
    if (!blocks.empty() && blocks.back().i + blocks.back().n == m.i &&
        blocks.back().j + blocks.back().n == m.j)
      blocks.back().n += m.n;
    else
      blocks.push_back(m);
  }
  blocks.push_back({a.size(), b.size(), 0});
  std::vector<RecordDifference> out;
  std::size_t ai = 0, bj = 0;
  for (const auto &m : blocks) {
    if (ai < m.i || bj < m.j) {
      std::string op = ai == m.i ? "insert" : bj == m.j ? "delete" : "replace";
      out.push_back({op,
                     {a.begin() + ai, a.begin() + m.i},
                     {b.begin() + bj, b.begin() + m.j}});
    }
    ai = m.i + m.n;
    bj = m.j + m.n;
  }
  return out;
}

std::array<std::array<int, 4>, 4> dimxMatrix() {
  std::array<std::array<int, 4>, 4> m{};
  for (int i = 0; i < 16; ++i) {
    const unsigned nibble =
        static_cast<unsigned>((DIMX_WORD >> (i * 4)) & 7u);
    const int value = (nibble & 4u) ? static_cast<int>(nibble) - 8
                                     : static_cast<int>(nibble);
    m[static_cast<std::size_t>(i / 4)][static_cast<std::size_t>(i % 4)] = value;
  }
  return m;
}
int dither5(int channel, int x, int y) {
  const auto m = dimxMatrix();
  int v = channel + m[static_cast<std::size_t>(y & 3)][static_cast<std::size_t>(x & 3)];
  if (v < 0)
    v = 0;
  if (v > 255)
    v = 255;
  return v >> 3;
}
std::uint32_t scaleRgb(std::uint32_t colour, simulation::rta::Raw factor) {
  auto channel = [&](int shift) {
    const int c = static_cast<int>((colour >> shift) & 0xffu);
    int v = simulation::rta::to_int(
        simulation::rta::mul(simulation::rta::from_int(static_cast<std::uint32_t>(c)), factor));
    if (v < 0)
      v = 0;
    if (v > 255)
      v = 255;
    return static_cast<std::uint32_t>(v);
  };
  return channel(0) | (channel(8) << 8) | (channel(16) << 16) | (colour & 0xff000000u);
}
simulation::rta::Raw presentGain(simulation::rta::Raw brightness) {
  return simulation::rta::add(simulation::rta::mul(brightness, 0x3f4ccccc), 0x3e4ccccc);
}
int presentChannel(int value, simulation::rta::Raw brightness) {
  const auto scaled = simulation::rta::mul(
      simulation::rta::from_int(static_cast<std::uint32_t>(value)), presentGain(brightness));
  int v = simulation::rta::to_int(simulation::rta::add(scaled, scaled));
  if (v < 0)
    v = 0;
  if (v > 255)
    v = 255;
  return v;
}
std::uint32_t presentColour(int r, int g, int b, simulation::rta::Raw brightness) {
  return static_cast<std::uint32_t>(presentChannel(r, brightness)) |
         (static_cast<std::uint32_t>(presentChannel(g, brightness)) << 8) |
         (static_cast<std::uint32_t>(presentChannel(b, brightness)) << 16);
}
int presentChannelStep(int value) {
  if (value >= 0x85)
    return value - 4;
  if (value >= 0x7c)
    return 0x80;
  return value + 4;
}
} // namespace champions::ui_composite
