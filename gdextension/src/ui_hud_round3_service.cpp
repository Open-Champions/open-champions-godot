#include "champions/hud_dynamic.hpp"
#include "champions/ui_composite.hpp"
#include "champions/ui_text_gaps.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace godot;
namespace H = champions::hud_dynamic;
namespace C = champions::ui_composite;
namespace G = champions::ui_text_gaps;
namespace F = champions::simulation::rta;

Dictionary failure(const std::exception &e) {
  Dictionary d;
  d["ok"] = false;
  d["error"] = String::utf8(e.what());
  return d;
}
// A call of the research model as {name, args}: words are unsigned 64-bit ints,
// names are strings.
Array calls_array(const std::vector<G::Call> &calls) {
  Array out;
  for (const auto &c : calls) {
    Dictionary d;
    d["name"] = String::utf8(c.name.c_str());
    Array args;
    for (const auto &a : c.args) {
      if (const auto *w = std::get_if<std::uint32_t>(&a))
        args.push_back(int64_t(*w));
      else
        args.push_back(String::utf8(std::get<std::string>(a).c_str()));
    }
    d["args"] = args;
    out.push_back(d);
  }
  return out;
}
Dictionary success() {
  Dictionary d;
  d["ok"] = true;
  return d;
}
F::Raw raw_of(int64_t bits) { return static_cast<F::Raw>(static_cast<uint32_t>(bits)); }
Dictionary submission(const H::Record &v) {
  Dictionary d;
  std::visit(
      [&](const auto &r) {
        using R = std::decay_t<decltype(r)>;
        if constexpr (std::is_same_v<R, H::Sprite>) {
          d["kind"] = "sprite";
          d["resource"] = String::utf8(r.resource.c_str());
          d["x"] = r.x;
          d["y"] = r.y;
          // Unknown RGB is not exported as 0: only the documented alpha.
          if (r.tint_rgb_known)
            d["tint"] = int64_t(r.tint);
          else
            d["alpha"] = int64_t(r.tint >> 24);
          d["clipped"] = r.clipped;
          if (r.clipped)
            d["clip_right"] = r.clip_right;
        } else if constexpr (std::is_same_v<R, H::Text>) {
          d["kind"] = "text";
          d["text_kind"] = String::utf8(r.kind.c_str());
          d["x"] = r.x;
          d["y"] = r.y;
          d["colour"] = int64_t(r.colour);
          d["text"] = String::utf8(r.text.c_str());
          d["batch"] = r.batch;
        } else if constexpr (std::is_same_v<R, H::Rect>) {
          d["kind"] = "rect";
          d["x0"] = r.x0;
          d["y0"] = r.y0;
          d["x1"] = r.x1;
          d["y1"] = r.y1;
          d["colour"] = int64_t(r.colour);
        } else {
          d["kind"] = "frame";
          d["x"] = r.x;
          d["y"] = r.y;
          d["width"] = r.width;
          d["height"] = r.height;
        }
        d["layer"] = r.layer;
      },
      v);
  return d;
}

class ChampionsUiHudRound3Service : public RefCounted {
  GDCLASS(ChampionsUiHudRound3Service, RefCounted)
protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("minimap_params", "mode", "players"),
                         &ChampionsUiHudRound3Service::minimap_params);
    ClassDB::bind_method(D_METHOD("minimap_approach", "current", "target"),
                         &ChampionsUiHudRound3Service::minimap_approach);
    ClassDB::bind_method(D_METHOD("minimap_player_blip", "kind"),
                         &ChampionsUiHudRound3Service::minimap_player_blip);
    ClassDB::bind_method(D_METHOD("minimap_object_blip", "flags_c", "flags_hi", "flags_lo"),
                         &ChampionsUiHudRound3Service::minimap_object_blip);
    ClassDB::bind_method(D_METHOD("ui_ramps_step", "pulse", "fade"), &ChampionsUiHudRound3Service::ui_ramps_step);
    ClassDB::bind_method(D_METHOD("help_plan", "delay", "pulse", "glyph_word", "wrap_height", "label_width", "has_title", "has_body"),
                         &ChampionsUiHudRound3Service::help_plan);
    ClassDB::bind_method(D_METHOD("bust_plan", "latch", "fade", "value", "maximum", "sim_frame", "bar_frame_available", "sizes"),
                         &ChampionsUiHudRound3Service::bust_plan);
    ClassDB::bind_method(D_METHOD("compass_plan", "dialog_open", "shown"), &ChampionsUiHudRound3Service::compass_plan);
    ClassDB::bind_method(D_METHOD("confirm_glyph", "layout_byte"), &ChampionsUiHudRound3Service::confirm_glyph);
    ClassDB::bind_method(D_METHOD("flying_step", "life", "alpha"),
                         &ChampionsUiHudRound3Service::flying_step);
    ClassDB::bind_method(D_METHOD("flying_draw", "kind", "crit", "alpha", "x_off", "y_off",
                                  "world_z", "clip_x", "clip_y", "w"),
                         &ChampionsUiHudRound3Service::flying_draw);
    ClassDB::bind_method(D_METHOD("flying_drift", "rand_word"),
                         &ChampionsUiHudRound3Service::flying_drift);
    ClassDB::bind_method(D_METHOD("compass_label"), &ChampionsUiHudRound3Service::compass_label);
    ClassDB::bind_method(D_METHOD("help_box", "wrap_height", "label_width"),
                         &ChampionsUiHudRound3Service::help_box);
    ClassDB::bind_method(D_METHOD("notice_box", "word", "wrap_height"),
                         &ChampionsUiHudRound3Service::notice_box);
    ClassDB::bind_method(D_METHOD("medal_prompt", "pause_menu", "armed", "mission_menu", "network",
                                  "first_active"),
                         &ChampionsUiHudRound3Service::medal_prompt);
    ClassDB::bind_method(D_METHOD("net_cam", "network", "other_slot", "health_zero", "has_object",
                                  "countdown", "alive"),
                         &ChampionsUiHudRound3Service::net_cam);
    ClassDB::bind_method(D_METHOD("arena_step", "timer", "step"),
                         &ChampionsUiHudRound3Service::arena_step);
    ClassDB::bind_method(D_METHOD("present_colour", "r", "g", "b", "brightness"),
                         &ChampionsUiHudRound3Service::present_colour);
    ClassDB::bind_method(D_METHOD("dither5", "channel", "x", "y"),
                         &ChampionsUiHudRound3Service::dither5);
    ClassDB::bind_method(D_METHOD("refused", "which"), &ChampionsUiHudRound3Service::refused);
  }

public:
  Dictionary minimap_params(int mode, int players) const {
    try {
      auto p = H::minimapParams(mode, players);
      PackedInt64Array words;
      for (auto w : p.words)
        words.push_back(int64_t(w));
      auto d = success();
      d["words"] = words;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary minimap_approach(int64_t current, int64_t target) const {
    try {
      auto d = success();
      d["value"] = int64_t(H::minimapApproach(raw_of(current), raw_of(target)));
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary minimap_player_blip(int kind) const {
    try {
      auto d = success();
      auto rgb = H::minimapPlayerBlip(kind);
      d["drawn"] = rgb.has_value();
      if (rgb) {
        PackedInt32Array out;
        for (int c : *rgb)
          out.push_back(c);
        d["rgb"] = out;
      }
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary minimap_object_blip(int64_t flags_c, int64_t flags_hi, int64_t flags_lo) const {
    try {
      auto flags = (static_cast<uint64_t>(static_cast<uint32_t>(flags_hi)) << 32) |
                   static_cast<uint32_t>(flags_lo);
      auto b = H::minimapObjectBlip(static_cast<uint32_t>(flags_c), flags);
      PackedInt32Array rgb;
      for (int c : b.rgb)
        rgb.push_back(c);
      auto d = success();
      d["rgb"] = rgb;
      if (b.scale)
        d["scale"] = int64_t(*b.scale);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // Session step 13 (docs/ui-text-gaps.md 1) on four slots of raw floats.
  Dictionary ui_ramps_step(const PackedInt64Array &pulse, const PackedInt64Array &fade) const {
    try {
      if (pulse.size() != 4 || fade.size() != 4)
        throw std::invalid_argument("four slots");
      G::Ramps r;
      for (int i = 0; i < 4; ++i) {
        r.pulse[size_t(i)] = raw_of(pulse[i]);
        r.fade[size_t(i)] = raw_of(fade[i]);
      }
      r = G::rampsStep(r);
      PackedInt64Array p, f;
      for (int i = 0; i < 4; ++i) {
        p.push_back(int64_t(r.pulse[size_t(i)]));
        f.push_back(int64_t(r.fade[size_t(i)]));
      }
      auto d = success();
      d["pulse"] = p;
      d["fade"] = f;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // The draw calls of help_task for an open box (docs/ui-text-gaps.md 2). The
  // pointers are stand-ins the caller maps back to its own units: 1 title, 2 body.
  Dictionary help_plan(int delay, int64_t pulse, int64_t glyph_word, int wrap_height, int label_width,
                       bool has_title, bool has_body) const {
    try {
      G::HelpState st;
      st.delay = delay;
      st.pulse = raw_of(pulse);
      st.glyph_word = static_cast<uint32_t>(glyph_word);
      st.wrap_height = wrap_height;
      st.label_width = label_width;
      st.title = has_title ? 1 : 0;
      st.body = has_body ? 2 : 0;
      st.font_med = 0x4f7100;
      st.num_players = 0; // the confirm loop is the host's (strings.cpp help_task)
      auto r = G::helpTask(st);
      auto d = success();
      d["calls"] = calls_array(r.calls);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // hud_drawBust (docs/ui-text-gaps.md 3). sizes maps a resource name to
  // [width, height]; a name the draw needs and the map lacks refuses.
  Dictionary bust_plan(int latch, int64_t fade, int value, int maximum, int sim_frame, bool bar_frame_available,
                       const Dictionary &sizes) const {
    try {
      G::BustState st;
      st.latch = static_cast<int8_t>(latch);
      st.fade = raw_of(fade);
      st.value = value;
      st.maximum = maximum;
      st.sim_frame = sim_frame;
      st.bar_frame_available = bar_frame_available;
      st.font_small = 0x4f7104;
      st.tex = [&](const std::string &name) {
        const Variant key = String::utf8(name.c_str());
        if (!sizes.has(key))
          throw std::invalid_argument("no size for " + name);
        const Array wh = sizes[key];
        return G::TexSize{int(wh[0]), int(wh[1])};
      };
      auto r = G::hudDrawBust(st);
      auto d = success();
      d["calls"] = calls_array(r.calls);
      d["fade"] = int64_t(r.fade);
      d["latch"] = int(r.latch);
      d["value"] = int(r.value);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // The compass label (docs/ui-text-gaps.md 5). The world matrix and the mesh
  // need the game camera's view matrix: the label alone is planned here, and the
  // caller checks the researched gate words before requesting this label plan.
  Dictionary compass_plan(bool dialog_open, bool shown) const {
    try {
      G::CompassState st;
      st.dialog_open = dialog_open ? 1 : 0;
      st.shown = shown ? 1 : 0;
      st.font_small = 0x4f7104;
      st.view = {0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000};
      auto r = G::hudDrawCompass(st);
      Array label;
      for (const auto &c : r.calls)
        if (c.name != "model_draw")
          label.push_back(calls_array({c})[0]);
      auto d = success();
      d["calls"] = label;
      d["passes"] = int(r.calls.size()) - int(label.size());
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // The drawn confirm glyph for the observed layout byte only (docs 7).
  Dictionary confirm_glyph(int layout_byte) const {
    const auto g = G::confirmGlyph(static_cast<int8_t>(layout_byte));
    if (!g)
      return failure(std::invalid_argument("layout byte is not observed; its glyph is inferred or unwritten"));
    auto d = success();
    d["glyph"] = int64_t(*g);
    return d;
  }
  Dictionary flying_step(int life, int alpha) const {
    try {
      auto s = H::flyingTextStep(static_cast<uint16_t>(life), static_cast<uint16_t>(alpha));
      auto d = success();
      d["life"] = int(s.life);
      d["alpha"] = int(s.alpha);
      d["erase"] = s.erase;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary flying_draw(int kind, bool crit, int alpha, int64_t x_off, int64_t y_off,
                         int64_t world_z, int64_t clip_x, int64_t clip_y, int64_t w) const {
    try {
      H::FlyingNumber n;
      n.kind = kind;
      n.crit = crit;
      n.alpha = static_cast<uint16_t>(alpha);
      n.x_off = static_cast<uint32_t>(x_off);
      n.y_off = static_cast<uint32_t>(y_off);
      n.world_z = raw_of(world_z);
      auto before = n.x_off;
      auto drawn = n.draw(raw_of(clip_x), raw_of(clip_y), raw_of(w));
      auto d = success();
      d["x"] = int64_t(drawn.x);
      d["y"] = int64_t(drawn.y);
      d["colour"] = int64_t(drawn.colour);
      d["layer"] = drawn.layer;
      d["anchor_z"] = int64_t(drawn.anchor_z);
      d["x_off"] = int64_t(drawn.x_off);
      d["stored_x_off"] = int64_t(n.x_off);
      d["unchanged"] = n.x_off == before;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary flying_drift(int64_t rand_word) const {
    try {
      auto d = success();
      d["drift"] = H::flyingTextDrift(static_cast<uint32_t>(rand_word));
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // No hp_drop_text: damage numbers are created by damage_apply for
  // non-player victims (research-ui 1ed740a docs/gameplay.md step 30), never
  // from the player's own HP drop (audit HUD-1).
  Dictionary compass_label() const {
    auto l = H::compassLabel();
    auto d = success();
    d["x"] = l.x;
    d["y"] = l.y;
    d["width"] = l.width;
    d["top"] = l.top;
    d["bottom"] = l.bottom;
    d["line"] = l.line;
    d["colour"] = int64_t(l.colour);
    return d;
  }
  Dictionary help_box(int wrap_height, int label_width) const {
    try {
      auto b = H::helpBox(wrap_height, label_width);
      auto d = success();
      d["button_x"] = b.button_x;
      d["button_y"] = b.button_y;
      d["label_x"] = b.label_x;
      d["label_y"] = b.label_y;
      d["frame_x"] = b.frame_x;
      d["frame_y"] = b.frame_y;
      d["frame_width"] = b.frame_width;
      d["frame_height"] = b.frame_height;
      d["inner_x"] = b.inner_x;
      d["inner_y"] = b.inner_y;
      d["inner_width"] = b.inner_width;
      d["inner_height"] = b.inner_height;
      d["title_x"] = b.title_x;
      d["title_y"] = b.title_y;
      d["body_x"] = b.body_x;
      d["body_y"] = b.body_y;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary notice_box(int word, int wrap_height) const {
    try {
      auto b = H::noticeBox(H::noticeColumn(word), wrap_height);
      auto d = success();
      d["title_x"] = b.title_x;
      d["title_y"] = b.title_y;
      d["body_x"] = b.body_x;
      d["body_y"] = b.body_y;
      d["button_x"] = b.button_x;
      d["button_y"] = b.button_y;
      d["frame_x"] = b.frame_x;
      d["frame_y"] = b.frame_y;
      d["frame_width"] = b.frame_width;
      d["frame_height"] = b.frame_height;
      d["rect_x0"] = b.rect_x0;
      d["rect_y0"] = b.rect_y0;
      d["rect_x1"] = b.rect_x1;
      d["rect_y1"] = b.rect_y1;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary medal_prompt(bool pause_menu, bool armed, bool mission_menu, bool network,
                          bool first_active) const {
    try {
      auto p = H::medalPrompt(pause_menu, armed, mission_menu, network, first_active);
      auto d = success();
      d["shown"] = p.has_value();
      if (p) {
        d["title_key"] = String::utf8(p->title_key.c_str());
        d["body_key"] = String::utf8(p->body_key.c_str());
        d["box_x"] = int64_t(p->box.x);
        d["box_y"] = int64_t(p->box.y);
        d["box_width"] = int64_t(p->box.width);
        d["layer"] = int64_t(p->box.layer);
        d["list_x"] = p->list_x;
        d["list_y"] = p->list_y;
      }
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary net_cam(bool network, bool other_slot, bool health_zero, bool has_object,
                     int countdown, int alive) const {
    try {
      auto t = H::netCamText();
      auto d = success();
      d["show"] = H::netCamShow(network, other_slot, health_zero, has_object, countdown, alive);
      d["prompt"] = alive >= 2;
      d["name_x"] = t.name_x;
      d["name_y"] = t.name_y;
      d["prompt_x"] = t.prompt_x;
      d["prompt_y"] = t.prompt_y;
      d["prompt_colour"] = int64_t(t.prompt_colour);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary arena_step(int64_t timer, int step) const {
    try {
      auto pulse = H::arenaPulseArgs(step);
      auto d = success();
      d["active"] = H::arenaPlayerActive(raw_of(timer));
      d["shown"] = H::arenaStepShown(raw_of(timer), step);
      d["pulse_a"] = pulse.first;
      d["pulse_b"] = pulse.second;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary present_colour(int r, int g, int b, int64_t brightness) const {
    try {
      auto d = success();
      d["colour"] = int64_t(C::presentColour(r, g, b, raw_of(brightness)));
      d["gain"] = int64_t(C::presentGain(raw_of(brightness)));
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary dither5(int channel, int x, int y) const {
    try {
      auto d = success();
      d["value"] = C::dither5(channel, x, y);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary refused(String which) const {
    try {
      auto name = std::string(which.utf8().get_data());
      if (name == "minimap_projector")
        H::minimapProjector();
      else if (name == "compass_mesh")
        H::compassMesh();
      else if (name == "arena_point")
        H::arenaHermitePoint();
      else if (name == "tex_flag")
        H::textureFlag0x1000();
      else if (name == "font_header")
        H::fontHeaderPlus2();
      else
        throw std::invalid_argument("unknown refusal");
      return success();
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
};
} // namespace

void register_ui_hud_round3_service() { GDREGISTER_CLASS(ChampionsUiHudRound3Service); }
