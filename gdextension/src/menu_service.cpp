#include "champions/menus.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace {
using namespace godot;
namespace M = champions::menus;

Dictionary failure(const std::exception &error) {
  Dictionary out;
  out["ok"] = false;
  out["error"] = String::utf8(error.what());
  return out;
}
Dictionary success() {
  Dictionary out;
  out["ok"] = true;
  return out;
}
std::string text_of(const String &value) {
  auto utf = value.utf8();
  return utf.get_data() ? std::string(utf.get_data()) : std::string();
}

class ChampionsMenuService : public RefCounted {
  GDCLASS(ChampionsMenuService, RefCounted)
  std::unique_ptr<M::Ledger> ledger_;
  std::string scheme_ = "cross";

  Dictionary fill_state(Dictionary out) {
    out["gold"] = ledger_->gold();
    out["skill_points"] = ledger_->skill_points();
    out["stat_points"] = ledger_->stat_points();
    out["level"] = ledger_->level();
    out["acknowledged"] = ledger_->acknowledged();
    out["hp"] = int64_t(ledger_->hit_points());
    out["max_hp"] = int64_t(ledger_->max_hit_points());
    out["bag_count"] = ledger_->bag_count(0);
    out["equipment_index"] = ledger_->equipment_index(0);
    out["page"] = ledger_->page();
    out["dragging"] = ledger_->dragging();
    out["scheme"] = String::utf8(scheme_.c_str());
    const auto masks = ledger_->scheme();
    out["shop_mask"] = int64_t(masks.shop);
    out["equip_mask"] = int64_t(masks.equip);
    out["drop_mask"] = int64_t(masks.drop);
    out["passive"] = ledger_->passive_level_at(1);
    out["rank_cost"] = M::rank_cost(5, ledger_->passive_level_at(1));
    Array stats;
    for (int i = 0; i < 4; ++i)
      stats.push_back(ledger_->base_stat(i));
    out["base_stats"] = stats;
    return out;
  }

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("state"), &ChampionsMenuService::state);
    ClassDB::bind_method(D_METHOD("frame", "screen"), &ChampionsMenuService::frame);
    ClassDB::bind_method(D_METHOD("press", "mode", "edge"), &ChampionsMenuService::press);
    ClassDB::bind_method(D_METHOD("set_cursor", "x", "y"), &ChampionsMenuService::set_cursor);
    ClassDB::bind_method(D_METHOD("set_page", "page"), &ChampionsMenuService::set_page);
    ClassDB::bind_method(D_METHOD("set_scheme", "name"), &ChampionsMenuService::set_scheme);
    ClassDB::bind_method(D_METHOD("spend_stat", "stat"), &ChampionsMenuService::spend_stat);
    ClassDB::bind_method(D_METHOD("spend_skill", "is_skill", "value"), &ChampionsMenuService::spend_skill);
    ClassDB::bind_method(D_METHOD("grant_level"), &ChampionsMenuService::grant_level);
    ClassDB::bind_method(D_METHOD("open_level_up"), &ChampionsMenuService::open_level_up);
    ClassDB::bind_method(D_METHOD("save_bytes"), &ChampionsMenuService::save_bytes);
    ClassDB::bind_method(D_METHOD("load_bytes", "bytes"), &ChampionsMenuService::load_bytes);
    ClassDB::bind_method(D_METHOD("reset"), &ChampionsMenuService::reset);
    ClassDB::bind_method(D_METHOD("adopt", "character"), &ChampionsMenuService::adopt);
    ClassDB::bind_method(D_METHOD("gaps"), &ChampionsMenuService::gaps);
    ClassDB::bind_method(D_METHOD("layout"), &ChampionsMenuService::layout);
  }

public:
  ChampionsMenuService() : ledger_(std::make_unique<M::Ledger>()) {}

  Dictionary state() {
    try {
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary frame(const String &screen) {
    try {
      const auto drawn = ledger_->frame(text_of(screen));
      auto out = success();
      out["screen"] = String::utf8(drawn.screen.c_str());
      Array texts;
      for (const auto &text : drawn.texts) {
        Dictionary row;
        row["x"] = text.x;
        row["y"] = text.y;
        row["colour"] = int64_t(text.colour);
        row["centred"] = text.centred;
        row["text"] = String::utf8(text.text.c_str());
        texts.push_back(row);
      }
      Array rects;
      for (const auto &rect : drawn.rects) {
        Dictionary row;
        row["x0"] = rect.x0;
        row["y0"] = rect.y0;
        row["x1"] = rect.x1;
        row["y1"] = rect.y1;
        row["colour"] = int64_t(rect.colour);
        row["layer"] = rect.layer;
        rects.push_back(row);
      }
      Array gaps;
      for (const auto &gap : drawn.gaps) {
        Dictionary row;
        row["id"] = String::utf8(gap.id);
        row["reason"] = String::utf8(gap.reason);
        gaps.push_back(row);
      }
      out["texts"] = texts;
      out["rects"] = rects;
      out["gaps"] = gaps;
      return out;
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary press(int mode, int64_t edge) {
    try {
      auto out = success();
      out["action"] = String::utf8(M::action_name(ledger_->press(mode, static_cast<std::uint32_t>(edge))));
      return fill_state(out);
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary set_cursor(int x, int y) {
    ledger_->set_cursor(x, y);
    return success();
  }
  Dictionary set_page(int page) {
    try {
      ledger_->set_page(page);
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary set_scheme(const String &name) {
    try {
      const auto key = text_of(name);
      if (key == "cross")
        ledger_->set_scheme(M::CROSS_CONFIRM);
      else if (key == "circle")
        ledger_->set_scheme(M::CIRCLE_CONFIRM);
      else
        throw M::Refusal("button scheme is cross or circle");
      scheme_ = key;
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary spend_stat(int stat) {
    try {
      ledger_->spend_stat(stat);
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary spend_skill(int is_skill, int64_t value) {
    try {
      ledger_->spend_skill(is_skill, static_cast<std::uint32_t>(value));
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary grant_level() {
    try {
      ledger_->grant_level();
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary open_level_up() {
    try {
      ledger_->open_level_up();
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary save_bytes() const {
    try {
      const auto bytes = ledger_->save();
      PackedByteArray packed;
      packed.resize(static_cast<int64_t>(bytes.size()));
      if (!bytes.empty())
        std::memcpy(packed.ptrw(), bytes.data(), bytes.size());
      auto out = success();
      out["bytes"] = packed;
      return out;
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary load_bytes(const PackedByteArray &bytes) {
    try {
      std::vector<std::uint8_t> raw;
      if (bytes.size())
        raw.assign(bytes.ptr(), bytes.ptr() + bytes.size());
      ledger_->load(raw);
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary reset() {
    try {
      ledger_ = std::make_unique<M::Ledger>();
      scheme_ = "cross";
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  // A front-end character (frontend view "character") or a PS2 import result.
  Dictionary adopt(const Dictionary &c) {
    try {
      M::Ledger::Adopted a;
      a.gold = int(c.get("gold", 0));
      a.level = int(c.get("level", 1));
      a.acknowledged = int(c.get("acknowledged", 1));
      a.experience = int(c.get("experience", 0));
      a.skill_points = int(c.get("skill_points", 0));
      a.stat_points = int(c.get("stat_points", 0));
      Array stats = c.get("stats", c.get("base_stats", Array()));
      if (stats.size() != 4)
        throw M::Refusal("adopt needs four base stats");
      for (int i = 0; i < 4; ++i)
        a.stats[i] = int(stats[i]);
      ledger_->adopt(a);
      return fill_state(success());
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary gaps() const {
    try {
      auto out = success();
      Array rows;
      for (const auto &gap : M::gaps()) {
        Dictionary row;
        row["id"] = String::utf8(gap.id);
        row["reason"] = String::utf8(gap.reason);
        rows.push_back(row);
      }
      out["gaps"] = rows;
      return out;
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary layout() const {
    try {
      auto out = success();
      const auto rows = M::shop_choice_rows();
      out["shop_buy_y"] = rows.first;
      out["shop_sell_y"] = rows.second;
      const auto header = M::stats_header();
      out["stats_x"] = header.x;
      out["stats_y"] = header.y;
      out["stats_font"] = int64_t(header.font);
      const auto skill = M::skill_layout(0);
      out["skill_header_x"] = skill.header_x;
      out["skill_header_y"] = skill.header_y;
      out["skill_clip_x"] = skill.clip_x;
      out["skill_clip_y"] = skill.clip_y;
      const auto pause = M::pause_style();
      out["pause_font"] = int64_t(pause.font);
      out["pause_layer"] = pause.layer;
      out["pause_colour"] = int64_t(pause.colour);
      const auto shop = M::shop_mark();
      out["shop_sprite_x"] = shop.sprite_x;
      out["shop_sprite_y"] = shop.sprite_y;
      out["scheme_note"] = String("Cross-confirm is the inferred playable scheme. Circle-confirm is the other executed mask set. The writer of 0x41b69c was not found.");
      return out;
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
};
} // namespace

void register_frontend_service();
void register_menu_service() {
  GDREGISTER_CLASS(ChampionsMenuService);
  register_frontend_service();
}
