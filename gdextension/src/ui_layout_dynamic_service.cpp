#include "champions/hud_dynamic.hpp"
#include "champions/ui_text_layout.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using namespace godot;
namespace H = champions::hud_dynamic;
namespace T = champions::ui_text_layout;
Dictionary failure(const std::exception &e) {
  Dictionary d;
  d["ok"] = false;
  d["error"] = String::utf8(e.what());
  return d;
}
Dictionary success() {
  Dictionary d;
  d["ok"] = true;
  return d;
}
Dictionary record(const H::Record &v) {
  Dictionary d;
  std::visit(
      [&](const auto &r) {
        using R = std::decay_t<decltype(r)>;
        if constexpr (std::is_same_v<R, H::Sprite>) {
          d["kind"] = "sprite";
          d["resource"] = String::utf8(r.resource.c_str());
          d["x"] = r.x;
          d["y"] = r.y;
          d["tint"] = int64_t(r.tint);
        } else if constexpr (std::is_same_v<R, H::Rect>) {
          d["kind"] = "rect";
          d["x0"] = r.x0;
          d["y0"] = r.y0;
          d["x1"] = r.x1;
          d["y1"] = r.y1;
          d["colour"] = int64_t(r.colour);
        } else if constexpr (std::is_same_v<R, H::Text>) {
          d["kind"] = "text";
          d["text_kind"] = String::utf8(r.kind.c_str());
          d["x"] = r.x;
          d["y"] = r.y;
          d["colour"] = int64_t(r.colour);
          d["text"] = String::utf8(r.text.c_str());
        } else {
          d["kind"] = "frame";
          d["x"] = r.x;
          d["y"] = r.y;
          d["width"] = r.width;
          d["height"] = r.height;
          d["tint"] = int64_t(r.tint);
          d["style"] = r.style;
        }
        d["layer"] = r.layer;
      },
      v);
  return d;
}
Array records(const H::Records &src) {
  Array out;
  for (const auto &r : src)
    out.push_back(record(r));
  return out;
}
T::Codes codes(const PackedInt32Array &source) {
  T::Codes out;
  out.reserve(source.size());
  for (int x : source) {
    if (x < 0 || x > 65535)
      throw std::out_of_range("wide text unit outside u16");
    out.push_back(std::uint16_t(x));
  }
  return out;
}
std::vector<std::uint8_t> bytes(const PackedByteArray &source) {
  std::vector<std::uint8_t> out;
  out.reserve(source.size());
  for (int i = 0; i < source.size(); ++i)
    out.push_back(source[i]);
  return out;
}
PackedByteArray packed(const std::vector<std::uint8_t> &source) {
  PackedByteArray out;
  out.resize(source.size());
  for (std::size_t i = 0; i < source.size(); ++i)
    out[i] = source[i];
  return out;
}
template <std::size_t N>
std::array<int, N> ints(const PackedInt32Array &source) {
  if (source.size() != N)
    throw std::invalid_argument("incorrect HUD array length");
  std::array<int, N> out;
  for (int i = 0; i < int(N); ++i)
    out[i] = source[i];
  return out;
}
PackedInt32Array packedInts(const std::array<int, 8> &source) {
  PackedInt32Array out;
  for (int x : source)
    out.push_back(x);
  return out;
}
Array wrappedRuns(const T::Wrapped &r) {
  Array out;
  for (const auto &v : r.runs) {
    Dictionary d;
    d["x"] = v.x;
    d["y"] = v.y;
    d["start"] = int64_t(v.start);
    d["count"] = int64_t(v.count);
    out.push_back(d);
  }
  return out;
}
class ChampionsUiDynamicLayoutService : public RefCounted {
  GDCLASS(ChampionsUiDynamicLayoutService, RefCounted)
protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("draw_sequence"),
                         &ChampionsUiDynamicLayoutService::draw_sequence);
    ClassDB::bind_method(D_METHOD("status_sequence"),
                         &ChampionsUiDynamicLayoutService::status_sequence);
    ClassDB::bind_method(D_METHOD("painter_indices", "layers"),
                         &ChampionsUiDynamicLayoutService::painter_indices);
    ClassDB::bind_method(D_METHOD("pulse", "tick"),
                         &ChampionsUiDynamicLayoutService::pulse);
    ClassDB::bind_method(
        D_METHOD("status_adjuncts", "slot", "tick", "x", "glow_alpha",
                 "arena_colour", "has_player_colour", "player_colour",
                 "blink_mask", "pvp_a", "pvp_b", "counter_width"),
        &ChampionsUiDynamicLayoutService::status_adjuncts);
    ClassDB::bind_method(
        D_METHOD("count_active_notices", "timers", "except_index"),
        &ChampionsUiDynamicLayoutService::count_active_notices);
    ClassDB::bind_method(D_METHOD("notice_frame", "slot", "timers", "alphas",
                                  "counts", "tick", "network", "hide_counts"),
                         &ChampionsUiDynamicLayoutService::notice_frame);
    ClassDB::bind_method(D_METHOD("message_frame", "timer", "alpha", "force",
                                  "suppressed", "text_width"),
                         &ChampionsUiDynamicLayoutService::message_frame);
    ClassDB::bind_method(D_METHOD("boss_meter_frame", "hp", "maximum", "alpha",
                                  "visible", "previous", "language"),
                         &ChampionsUiDynamicLayoutService::boss_meter_frame);
    ClassDB::bind_method(
        D_METHOD("gold_counter", "slot", "alpha", "text_width"),
        &ChampionsUiDynamicLayoutService::gold_counter);
    ClassDB::bind_method(
        D_METHOD("gold_floater_step", "slot", "position_raw", "velocity_raw"),
        &ChampionsUiDynamicLayoutService::gold_floater_step);
    ClassDB::bind_method(
        D_METHOD("gold_fade", "tick", "start", "hold", "alpha"),
        &ChampionsUiDynamicLayoutService::gold_fade);
    ClassDB::bind_method(D_METHOD("selector_direction", "dx", "dy"),
                         &ChampionsUiDynamicLayoutService::selector_direction);
    ClassDB::bind_method(D_METHOD("selector_frame", "slot", "tick", "alpha",
                                  "timer_raw", "speed", "active", "network",
                                  "menu_mode"),
                         &ChampionsUiDynamicLayoutService::selector_frame);
    ClassDB::bind_method(D_METHOD("text_layout", "fnt", "units", "x", "y",
                                  "count", "interlaced", "wrap_width",
                                  "clip_top", "clip_bottom", "line_height",
                                  "centered", "language", "budget", "colour",
                                  "icon_alpha", "stored_alpha"),
                         &ChampionsUiDynamicLayoutService::text_layout);
    ClassDB::bind_method(
        D_METHOD("text_scissor", "left", "top", "right", "bottom"),
        &ChampionsUiDynamicLayoutService::text_scissor);
    ClassDB::bind_method(
        D_METHOD("fit_ascii", "fnt", "ascii", "width", "interlaced"),
        &ChampionsUiDynamicLayoutService::fit_ascii);
  }

public:
  Array draw_sequence() const {
    Array out;
    for (auto stage : H::drawSequence())
      out.push_back(String::utf8(H::stageName(stage)));
    return out;
  }
  Array status_sequence() const {
    Array out;
    for (auto name : H::statusSequence())
      out.push_back(String::utf8(name));
    return out;
  }
  Dictionary painter_indices(const PackedInt32Array &layers) const {
    try {
      H::Records records;
      records.reserve(layers.size());
      for (int layer : layers)
        records.emplace_back(H::Rect{0, 0, 0, 0, layer, 0});
      auto order = H::painterIndices(records);
      PackedInt32Array indices;
      for (std::size_t i : order)
        indices.push_back(int(i));
      auto d = success();
      d["indices"] = indices;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  int pulse(int tick) const { return H::pulse(std::uint32_t(tick)); }
  Dictionary status_adjuncts(int slot, int tick, int x, int glow_alpha,
                             int64_t arena_colour, bool has_player_colour,
                             int64_t player_colour, int64_t blink_mask,
                             bool pvp_a, bool pvp_b, int counter_width) const {
    try {
      const int p = H::pulse(std::uint32_t(tick));
      auto d = success();
      d["pulse"] = p;
      d["pending_level_colour"] = int64_t(H::pendingLevelColour(p));
      d["name_colour"] = int64_t(H::nameColour(
          slot, tick, std::uint32_t(arena_colour), has_player_colour,
          std::uint32_t(player_colour), std::uint32_t(blink_mask)));
      d["name_width_budget"] = H::nameWidthBudget(pvp_a, pvp_b, counter_width);
      const int next_glow_alpha = H::glowStep(glow_alpha);
      d["glow_alpha"] = next_glow_alpha;
      d["glow_sprite"] = record(H::glowSprite(x, next_glow_alpha, p));
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary count_active_notices(const PackedInt32Array &timers,
                                  int except_index) const {
    try {
      auto d = success();
      d["count"] = H::countActiveNotices(ints<8>(timers), except_index);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary notice_frame(int slot, const PackedInt32Array &timers,
                          const PackedInt32Array &alphas,
                          const PackedInt32Array &counts, int tick,
                          bool network, bool hide_counts) const {
    try {
      auto r = H::noticeFrame(slot, ints<8>(timers), ints<8>(alphas),
                              ints<8>(counts), std::uint32_t(tick), network,
                              hide_counts);
      auto d = success();
      d["submissions"] = records(r.submissions);
      d["timers"] = packedInts(r.timers);
      d["alphas"] = packedInts(r.alphas);
      PackedInt32Array cleared;
      for (int i : r.cleared)
        cleared.push_back(i);
      d["cleared"] = cleared;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary message_frame(int timer, int alpha, bool force, bool suppressed,
                           int text_width) const {
    try {
      auto r = H::messageFrame(timer, alpha, force, suppressed, text_width);
      auto d = success();
      d["submissions"] = records(r.submissions);
      d["alpha"] = r.alpha;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary boss_meter_frame(int hp, int maximum, int alpha, int visible,
                              int previous, int language) const {
    try {
      auto r =
          H::bossMeterFrame({hp, maximum, alpha, visible, previous}, language);
      auto d = success();
      d["submissions"] = records(r.submissions);
      d["hp"] = r.state.hp;
      d["maximum"] = r.state.maximum;
      d["alpha"] = r.state.alpha;
      d["visible"] = r.state.visible;
      d["previous"] = r.state.previous;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_counter(int slot, int alpha, int width) const {
    try {
      auto d = success();
      d["submissions"] = records(H::goldCounter(slot, alpha, width));
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_floater_step(int slot, const PackedInt32Array &position,
                               const PackedInt32Array &velocity) const {
    try {
      if (position.size() != 3 || velocity.size() != 3)
        throw std::invalid_argument("gold vectors must have three raw lanes");
      H::GoldFloater s;
      for (int i = 0; i < 3; ++i) {
        s.position[i] = std::uint32_t(position[i]);
        s.velocity[i] = std::uint32_t(velocity[i]);
      }
      auto r = H::goldFloaterStep(s, slot);
      PackedInt32Array p, v;
      for (int i = 0; i < 3; ++i) {
        p.push_back(std::int32_t(r.position[i]));
        v.push_back(std::int32_t(r.velocity[i]));
      }
      auto d = success();
      d["position_raw"] = p;
      d["velocity_raw"] = v;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_fade(int tick, int start, int hold, int alpha) const {
    try {
      const auto next = H::goldCounterStep(hold, alpha);
      auto d = success();
      d["floater_alpha"] = H::goldFloaterAlpha(tick, start);
      d["hold"] = next.first;
      d["alpha"] = next.second;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  int selector_direction(int dx, int dy) const {
    return H::selectorDirection(dx, dy);
  }
  Dictionary selector_frame(int slot, int tick, int alpha, int timer_raw,
                            int speed, bool active, bool network,
                            bool menu_mode) const {
    try {
      auto state =
          H::selectorFadeStep({alpha, std::uint32_t(timer_raw), speed}, active);
      auto centre = H::selectorCentre(slot, network, menu_mode);
      auto ring = H::selectorRing(centre.first, centre.second, state.alpha);
      auto highlight = H::selectorHighlight(std::uint32_t(tick), state.alpha);
      auto d = success();
      d["alpha"] = state.alpha;
      d["timer_raw"] = int64_t(state.timer);
      d["speed"] = state.speed;
      d["x"] = centre.first;
      d["y"] = centre.second;
      Array icons;
      for (auto r : ring) {
        Dictionary item;
        item["index"] = r.index;
        item["x"] = r.x;
        item["y"] = r.y;
        icons.push_back(item);
      }
      d["ring"] = icons;
      d["corona_tint"] = int64_t(highlight.first);
      d["icon_tint"] = int64_t(highlight.second);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary text_layout(const PackedByteArray &fnt,
                         const PackedInt32Array &units, int x, int y, int count,
                         bool interlaced, int width, int top, int bottom,
                         int height, bool centered, int language, int budget,
                         int64_t colour, int icon_alpha,
                         int stored_alpha) const {
    try {
      if (count < 0 || budget < 0)
        throw std::invalid_argument("negative text count or budget");
      auto font = champions::ui_layout::fontMetrics(
          {fnt.ptr(), std::size_t(fnt.size())});
      auto input = codes(units);
      T::TextState state{std::uint32_t(colour), icon_alpha, stored_alpha};
      auto r = T::glyphRun(font, x, y, input, std::size_t(count), &state);
      auto d = success();
      d["width"] = T::measureWide(font, input, interlaced, std::size_t(count));
      d["centered_x"] =
          T::centeredXWide(font, x, input, interlaced, std::size_t(count));
      d["end_x_half"] = r.end_x_half;
      d["icon_alpha_after"] = state.icon_alpha;
      Array glyphs;
      for (const auto &g : r.glyphs) {
        Dictionary item;
        item["index"] = g.index;
        item["x_half"] = g.x_half;
        item["y"] = g.y;
        item["source_x"] = g.source.x;
        item["source_y"] = g.source.y;
        item["source_width"] = g.source.width;
        item["source_height"] = g.source.height;
        item["tint"] = int64_t(g.tint);
        glyphs.push_back(item);
      }
      d["glyphs"] = glyphs;
      if (width > 0) {
        auto w =
            centered
                ? T::drawWrappedCentered(font, x, y, input, width, top, bottom,
                                         height, interlaced, language)
                : T::drawWrapped(font, x, y, input, width, top, bottom, height,
                                 interlaced, language, std::size_t(budget));
        d["runs"] = wrappedRuns(w);
        d["height"] = w.height;
        if (w.shown)
          d["shown"] = int64_t(*w.shown);
      }
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary text_scissor(int l, int t, int r, int b) const {
    try {
      auto s = T::textScissor(l, t, r, b);
      auto d = success();
      d["gs_left"] = s.gs_left;
      d["gs_right"] = s.gs_right;
      d["gs_top"] = s.gs_top;
      d["gs_bottom"] = s.gs_bottom;
      d["left"] = s.logical.left;
      d["top"] = s.logical.top;
      d["right"] = s.logical.right;
      d["bottom"] = s.logical.bottom;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary fit_ascii(const PackedByteArray &fnt, const PackedByteArray &ascii,
                       int width, bool interlaced) const {
    try {
      auto data = bytes(ascii);
      auto d = success();
      if (fnt.size()) {
        auto font = champions::ui_layout::fontMetrics(
            {fnt.ptr(), std::size_t(fnt.size())});
        d["ascii"] = packed(T::fitAscii(&font, data, width, interlaced));
      } else
        d["ascii"] = packed(T::fitAscii(nullptr, data, width, interlaced));
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
};
} // namespace
void register_ui_hud_round3_service();
void register_hud_extras_service();
void register_menu_service();
void register_ui_layout_dynamic_service() {
  GDREGISTER_CLASS(ChampionsUiDynamicLayoutService);
  register_ui_hud_round3_service();
  register_hud_extras_service();
  register_menu_service();
}
