#include "champions/hud_extras.hpp"
#include "champions/ui_layout.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <cstring>
#include <array>
#include <exception>
#include <optional>
#include <string>

namespace {
using namespace godot;
namespace X = champions::hud_extras;

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
X::Raw raw_float(double value) {
  const float f = static_cast<float>(value);
  X::Raw r;
  std::memcpy(&r, &f, sizeof r);
  return r;
}
int64_t geti(const Dictionary &d, const char *key, int64_t fallback) {
  return d.has(key) ? int64_t(d[key]) : fallback;
}
double getf(const Dictionary &d, const char *key, double fallback) {
  return d.has(key) ? double(d[key]) : fallback;
}

// HUD extras for the play scene (docs/HUD_EXTRAS.md). One instance owns one
// gold state. Inputs are display values; native code does the arithmetic.
class ChampionsHudExtrasService : public RefCounted {
  GDCLASS(ChampionsHudExtrasService, RefCounted)
  X::GoldState gold_;
  std::array<std::optional<uint32_t>, 4> last_observed_gold_{};

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("status_draws", "input"), &ChampionsHudExtrasService::status_draws);
    ClassDB::bind_method(D_METHOD("gold_reset", "gold"), &ChampionsHudExtrasService::gold_reset);
    ClassDB::bind_method(D_METHOD("gold_sync", "slot", "value"), &ChampionsHudExtrasService::gold_sync);
    ClassDB::bind_method(D_METHOD("gold_add", "slot", "amount", "screen_x", "screen_y"), &ChampionsHudExtrasService::gold_add);
    ClassDB::bind_method(D_METHOD("gold_tick", "frame"), &ChampionsHudExtrasService::gold_tick);
    ClassDB::bind_method(D_METHOD("gold_flush"), &ChampionsHudExtrasService::gold_flush);
    ClassDB::bind_method(D_METHOD("gold_bonus_life", "slot"), &ChampionsHudExtrasService::gold_bonus_life);
    ClassDB::bind_method(D_METHOD("gold_draw", "local_mask", "language", "font"), &ChampionsHudExtrasService::gold_draw);
    ClassDB::bind_method(D_METHOD("gold_state"), &ChampionsHudExtrasService::gold_state);
    ClassDB::bind_method(D_METHOD("format_gold", "value", "language"), &ChampionsHudExtrasService::format_gold);
    ClassDB::bind_method(D_METHOD("potion_use", "input"), &ChampionsHudExtrasService::potion_use);
    ClassDB::bind_method(D_METHOD("selector_input", "input"), &ChampionsHudExtrasService::selector_input);
    ClassDB::bind_method(D_METHOD("minimap_view", "input"), &ChampionsHudExtrasService::minimap_view);
    ClassDB::bind_method(D_METHOD("refusal", "name"), &ChampionsHudExtrasService::refusal);
  }

public:
  // Keys (doubles unless stated): slot, x (promoted HUD origin), local, dead
  // (object bit 0x8), health, max_health, pending_health, mana, max_mana,
  // pending_mana, tick, cooldown, ack_level, level, exp, exp_base, exp_next,
  // glow_alpha, flash. Returns {ok, draws, flash, glow_alpha}.
  Dictionary status_draws(const Dictionary &in) const {
    try {
      X::StatusInput s;
      s.slot = int(geti(in, "slot", 0));
      s.x = int(geti(in, "x", 12));
      s.local = bool(in.get("local", true));
      s.obj_flags = in.get("dead", false) ? 8u : 0u;
      s.health = raw_float(getf(in, "health", 0.0));
      s.health_max = raw_float(getf(in, "max_health", 1.0));
      s.health_pending = raw_float(getf(in, "pending_health", 0.0));
      s.mana = raw_float(getf(in, "mana", 0.0));
      s.mana_max = raw_float(getf(in, "max_mana", 1.0));
      s.mana_pending = raw_float(getf(in, "pending_mana", 0.0));
      s.tick = int32_t(geti(in, "tick", 0));
      s.flash = uint32_t(geti(in, "flash", 0));
      s.cooldown = int32_t(geti(in, "cooldown", 0));
      s.ack_level = int32_t(geti(in, "ack_level", 0));
      s.level = int32_t(geti(in, "level", 0));
      s.exp = int32_t(geti(in, "exp", 0));
      s.exp_base = int32_t(geti(in, "exp_base", 0));
      s.exp_next = int32_t(geti(in, "exp_next", 1));
      s.glow_alpha = int32_t(geti(in, "glow_alpha", 0));
      const auto r = X::statusDraws(s);
      Array draws;
      for (const auto &d : r.draws) {
        Dictionary e;
        e["layer"] = d.layer;
        if (d.rect) {
          e["kind"] = "rect";
          e["left"] = d.x0; e["top"] = d.y0; e["right"] = d.x1; e["bottom"] = d.y1;
          e["colour"] = int64_t(d.tint);
        } else {
          e["kind"] = "sprite";
          e["resource"] = String::utf8(d.resource.c_str());
          e["x"] = d.x0; e["y"] = d.y0;
          e["clipped"] = true;
          Dictionary clip;
          clip["left"] = d.clip_left; clip["top"] = d.clip_top; clip["right"] = d.clip_right; clip["bottom"] = d.clip_bottom;
          e["clip"] = clip;
          e["tint"] = int64_t(d.tint);
        }
        draws.push_back(e);
      }
      Dictionary out = success();
      out["draws"] = draws;
      out["flash"] = int64_t(r.flash);
      out["glow_alpha"] = r.glow_alpha;
      return out;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_reset(const PackedInt64Array &gold) {
    gold_ = X::GoldState{};
    last_observed_gold_ = {};
    for (int i = 0; i < 4 && i < gold.size(); ++i) {
      gold_.gold[std::size_t(i)] = uint32_t(gold[i]);
      last_observed_gold_[std::size_t(i)] = uint32_t(gold[i]);
    }
    return success();
  }
  // A busy floater owns local payouts. A changed observed record may include
  // those payouts, a direct writer, or both; without their ordering it cannot
  // be reconciled. Refuse the composite instead of displaying stale gold.
  Dictionary gold_sync(int slot, int64_t value) {
    try {
      if (slot < 0 || slot > 3) throw X::Refusal("gold slot outside 0..3");
      if (value < 0 || value > X::GOLD_CAP) throw X::Refusal("record gold outside the documented counter range");
      bool busy = gold_.pending[std::size_t(slot)] != 0;
      for (const auto &f : gold_.floaters)
        if (f.amount && f.slot == slot) busy = true;
      if (busy && last_observed_gold_[std::size_t(slot)] &&
          uint32_t(value) != *last_observed_gold_[std::size_t(slot)])
        throw X::Refusal("record gold changed during a floater or pending credit; composite order is unobserved");
      if (!busy) gold_.gold[std::size_t(slot)] = uint32_t(value);
      if (!busy) last_observed_gold_[std::size_t(slot)] = uint32_t(value);
      Dictionary out = success(); out["synced"] = !busy; return out;
    } catch (const std::exception &e) { return failure(e); }
  }
  // The floater's screen start is approximated by the given point with no drift:
  // it changes no shared state (hud-gold.md section 5). The owed cosmetic seed
  // draw is counted in gold_state().cosmetic_draws, not consumed here.
  Dictionary gold_add(int slot, int64_t amount, double screen_x, double screen_y) {
    try {
      X::FloaterStart fs;
      fs.pos = {raw_float(screen_x), raw_float(screen_y), 0};
      fs.vel = {0, 0, 0};
      Dictionary d = success();
      d["index"] = X::addGoldFloater(gold_, slot, uint32_t(amount), fs);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_tick(int64_t frame) {
    try {
      gold_.frame = uint32_t(frame);
      X::tickGold(gold_);
      return success();
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_flush() {
    try {
      X::flushGold(gold_);
      return success();
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_bonus_life(int slot) {
    try {
      X::arenaBonusLife(gold_, slot);
      return success();
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // One displayed frame. font is the FONT.LMP bytes; widths are only needed for
  // slots 1 and 3 and are refused (-1) when no font is given.
  Dictionary gold_draw(int local_mask, int language, const PackedByteArray &font) {
    try {
      std::optional<champions::ui_layout::FontMetrics> metrics;
      const auto width = [&](const std::string &text) {
        if (!font.size()) throw X::Refusal("right-aligned gold text needs the font metrics (FONT.LMP)");
        if (!metrics) metrics = champions::ui_layout::fontMetrics({font.ptr(), std::size_t(font.size())});
        return metrics->widthAscii(std::vector<std::uint8_t>(text.begin(), text.end()));
      };
      const auto r = X::drawGold(gold_, {bool(local_mask & 1), bool(local_mask & 2), bool(local_mask & 4), bool(local_mask & 8)},
                                 width, language);
      Dictionary d = success();
      Array floaters, counters, coins;
      for (const auto &f : r.floaters) {
        Dictionary e; e["x"] = f.x; e["y"] = f.y; e["text"] = String::utf8(f.text.c_str()); e["colour"] = int64_t(f.colour);
        floaters.push_back(e);
      }
      for (const auto &c : r.counters) {
        Dictionary e; e["slot"] = c.slot; e["x"] = c.x; e["y"] = c.y; e["text"] = String::utf8(c.text.c_str()); e["colour"] = int64_t(c.colour);
        counters.push_back(e);
      }
      for (const auto &c : r.coins) {
        Dictionary e; e["slot"] = c.slot; e["x"] = c.x; e["y"] = c.y; e["tint"] = int64_t(c.tint);
        coins.push_back(e);
      }
      d["floaters"] = floaters; d["counters"] = counters; d["coins"] = coins;
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary gold_state() const {
    Dictionary d = success();
    PackedInt64Array gold, pending, hold, alpha;
    for (int i = 0; i < 4; ++i) {
      gold.push_back(int32_t(gold_.gold[std::size_t(i)]));
      pending.push_back(gold_.pending[std::size_t(i)]);
      hold.push_back(gold_.hold[std::size_t(i)]);
      alpha.push_back(gold_.alpha[std::size_t(i)]);
    }
    int busy = 0;
    for (const auto &f : gold_.floaters) busy += f.amount ? 1 : 0;
    d["gold"] = gold; d["pending"] = pending; d["hold"] = hold; d["alpha"] = alpha;
    d["floaters_busy"] = busy;
    d["cosmetic_draws"] = gold_.cosmetic_draws;
    return d;
  }
  String format_gold(int64_t value, int language) const {
    return String::utf8(X::formatGold(int32_t(value), language).c_str());
  }
  // An explicit bag and live current values are required. The native play
  // session currently has neither a bag-2 mirror nor Player_usePotion input;
  // callers must pass observed data, never an inferred inventory.
  Dictionary potion_use(const Dictionary &in) const {
    try {
      for (const char *key : {"hp", "max_hp", "pending_hp", "mana", "max_mana", "pending_mana", "bag", "kind"})
        if (!in.has(key)) throw X::Refusal("potion input is incomplete");
      X::PotionInput p;
      p.hp = raw_float(getf(in, "hp", 0)); p.max_hp = raw_float(getf(in, "max_hp", 0));
      p.hp_pending = raw_float(getf(in, "pending_hp", 0));
      p.mana = raw_float(getf(in, "mana", 0)); p.max_mana = raw_float(getf(in, "max_mana", 0));
      p.mana_pending = raw_float(getf(in, "pending_mana", 0));
      const int64_t kind = geti(in, "kind", -1);
      if (kind < 0 || kind > 1) throw X::Refusal("potion kind outside 0..1");
      p.kind = int(kind);
      p.copy = bool(in.get("copy", false)); p.local = bool(in.get("local", true));
      p.menu_open = bool(in.get("menu_open", false)); p.grid = bool(in.get("grid", false));
      const Array bag = in["bag"];
      for (const Variant &cell : bag) {
        if (cell.get_type() != Variant::DICTIONARY) throw X::Refusal("bag-2 cell is not a record");
        const Dictionary d = cell;
        if (!d.has("kind") || !d.has("size") || !d.has("quantity"))
          throw X::Refusal("bag-2 cell is incomplete");
        p.bag.push_back({int(geti(d, "kind", -1)), int(geti(d, "size", -1)), int32_t(geti(d, "quantity", 0))});
      }
      const auto r = X::usePotion(p);
      Dictionary out = success(); Array notices, next_bag;
      for (const auto &n : r.notices) notices.push_back(Array::make(n.first, n.second));
      for (const auto &c : r.bag) {
        Dictionary cell; cell["kind"] = c.kind; cell["size"] = c.size; cell["quantity"] = c.quantity;
        next_bag.push_back(cell);
      }
      out["returned_early"] = r.returned_early; out["notices"] = notices;
      out["size"] = r.size; out["count"] = r.count;
      out["removed_cell"] = r.removed_cell ? Variant(*r.removed_cell) : Variant();
      out["sent_drop"] = r.sent_drop; out["bag"] = next_bag;
      if (r.regen) out["regen_raw"] = Array::make(int64_t((*r.regen)[0]), int64_t((*r.regen)[1]));
      out["pending_raw"] = int64_t(r.pending);
      return out;
    } catch (const std::exception &e) { return failure(e); }
  }
  // The nine gates and button masks are read from one explicit raw-pad frame.
  // This computes the documented mask/message; it does not claim a handler ran.
  Dictionary selector_input(const Dictionary &in) const {
    try {
      for (const char *key : {"local", "hp", "gates", "held", "pressed", "direction", "a", "b", "use", "accept"})
        if (!in.has(key)) throw X::Refusal("selector raw-pad input is incomplete");
      const Array supplied = in["gates"];
      if (supplied.size() != X::GateCount) throw X::Refusal("selector needs all nine gates");
      std::array<bool, X::GateCount> gates{};
      for (int i = 0; i < X::GateCount; ++i) gates[std::size_t(i)] = bool(supplied[i]);
      const bool menu = bool(in.get("local_menu_open", false));
      // hud_spellSelect returns before reading the pad for a nonlocal slot
      // or a player whose record current HP is not positive.
      const bool eligible = bool(in["local"]) && getf(in, "hp", 0) > 0.0;
      const bool readable = eligible && X::padReadable(gates, menu);
      const auto axes = X::padAxes(readable ? uint32_t(geti(in, "held", 0)) : 0u);
      X::SelectorButtons buttons{uint32_t(geti(in, "a", 0)), uint32_t(geti(in, "b", 0)),
                                  uint32_t(geti(in, "use", 0)), uint32_t(geti(in, "accept", 0))};
      const auto mask = axes.any ? X::maskLocalButtons({}, buttons) : X::MaskEntry{};
      const int64_t raw_direction = geti(in, "direction", -1);
      const int64_t raw_slot = geti(in, "slot", 0);
      if (raw_slot < 0 || raw_slot > 3) throw X::Refusal("selector slot outside 0..3");
      if (raw_direction < -1 || raw_direction > 7) throw X::Refusal("selector direction outside -1..7");
      const int slot = int(raw_slot), direction = int(raw_direction);
      const auto message = readable ? X::selectorMessage(slot, direction,
          bool(in.get("sheet", false)), uint32_t(geti(in, "pressed", 0)), buttons,
          int(geti(in, "kind", 0)), int(geti(in, "value", 0))) : X::SelectorMessage{};
      Dictionary out = success();
      out["eligible"] = eligible; out["readable"] = readable; out["dx"] = axes.dx; out["dy"] = axes.dy;
      out["held"] = axes.any; out["mask_frames"] = mask.frames;
      out["mask_bits"] = int64_t(mask.mask); out["message_id"] = message.id;
      Array args; for (int i = 0; i < message.count; ++i) args.push_back(message.args[std::size_t(i)]);
      out["message_args"] = args;
      return out;
    } catch (const std::exception &e) { return failure(e); }
  }
  // Keys: world_texture, free_units, yaw (s16), centre_x/y, origin_x/y, offx,
  // offy, zoom, half_w, half_h, width, height. Blips and tiles are separate.
  Dictionary minimap_view(const Dictionary &in) const {
    try {
      X::MinimapInput m;
      m.world_texture = bool(in.get("world_texture", false));
      m.free_units = int32_t(geti(in, "free_units", 0));
      m.yaw = int32_t(geti(in, "yaw", 0));
      m.centre = {raw_float(getf(in, "centre_x", 0)), raw_float(getf(in, "centre_y", 0))};
      m.origin = {raw_float(getf(in, "origin_x", 0)), raw_float(getf(in, "origin_y", 0))};
      m.offx = raw_float(getf(in, "offx", 0)); m.offy = raw_float(getf(in, "offy", 0));
      m.zoom = raw_float(getf(in, "zoom", 1)); m.half_w = raw_float(getf(in, "half_w", 0));
      m.half_h = raw_float(getf(in, "half_h", 0));
      m.width = int(geti(in, "width", 216)); m.height = int(geti(in, "height", 216));
      const auto v = X::minimapView(m);
      Dictionary d = success();
      d["gate"] = v.gate; d["drawn"] = v.drawn;
      d["scissor"] = Array::make(v.sx0, v.sy0, v.sx1, v.sy1);
      d["cells"] = Array::make(v.col_min, v.row_min, v.col_end, v.row_end);
      return d;
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  // "minimap_tiles" and "minimap_icons" always fail with the research reason.
  Dictionary refusal(const String &name) const {
    try {
      if (name == "minimap_tiles") X::minimapTiles();
      if (name == "minimap_icons") X::minimapIcons();
      throw std::invalid_argument("unknown refusal");
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
};
} // namespace

void register_hud_extras_service() { GDREGISTER_CLASS(ChampionsHudExtrasService); }
