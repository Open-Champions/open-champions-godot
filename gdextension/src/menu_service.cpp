#include "champions/menus.hpp"
#include "champions/arena_executable.hpp"
#include "champions/script_items.hpp"

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
    out["live_record"] = ledger_->live_record();
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
    ClassDB::bind_method(D_METHOD("record_word", "offset"), &ChampionsMenuService::record_word);
    ClassDB::bind_method(D_METHOD("adopt_record", "record", "executable", "picture_index"), &ChampionsMenuService::adopt_record);
    ClassDB::bind_method(D_METHOD("record_bytes"), &ChampionsMenuService::record_bytes);
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
        row["reason"] = String::utf8(gap.reason.c_str());
        row["column"] = gap.column;
        row["row"] = gap.row;
        gaps.push_back(row);
      }
      out["texts"] = texts;
      Array selected_texts;
      for (const auto &text : drawn.selected_texts) {
        Dictionary row;
        row["x"] = text.x;
        row["y"] = text.y;
        row["colour"] = int64_t(text.colour);
        row["centred"] = text.centred;
        row["text"] = String::utf8(text.text.c_str());
        selected_texts.push_back(row);
      }
      out["selected_texts"] = selected_texts;
      out["rects"] = rects;
      Array sprites;
      for (const auto &sprite : drawn.sprites) {
        Dictionary row;
        row["x"] = sprite.x;
        row["y"] = sprite.y;
        row["layer"] = sprite.layer;
        row["tint"] = int64_t(sprite.tint);
        row["lump"] = String::utf8(sprite.lump.c_str());
        row["resource"] = String::utf8(sprite.resource.c_str());
        sprites.push_back(row);
      }
      out["sprites"] = sprites;
      Array icons;
      for (const auto &icon : drawn.icons) {
        Dictionary row;
        row["column"] = icon.column;
        row["row"] = icon.row;
        row["category"] = icon.category;
        row["slot"] = icon.slot;
        row["tint"] = int64_t(icon.tint);
        row["picture"] = String::utf8(icon.picture.c_str());
        icons.push_back(row);
      }
      out["icons"] = icons;
      Array held_icons;
      for (const auto &icon : drawn.held_icons) {
        Dictionary row;
        row["x"] = icon.x;
        row["y"] = icon.y;
        row["layer"] = icon.layer;
        row["tint"] = int64_t(icon.tint);
        row["picture"] = String::utf8(icon.picture.c_str());
        held_icons.push_back(row);
      }
      out["held_icons"] = held_icons;
      Array frames;
      for (const auto &frame : drawn.frames) {
        Dictionary row;
        row["x"] = frame.x;
        row["y"] = frame.y;
        row["width"] = frame.width;
        row["height"] = frame.height;
        row["style"] = frame.style;
        row["tint"] = int64_t(frame.tint);
        frames.push_back(row);
      }
      out["frames"] = frames;
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
  Dictionary record_word(int64_t offset) {
    try {
      if (offset < 0 || offset > 0x4c30 - 4)
        throw M::Refusal("player record offset is outside the record");
      auto out = success();
      out["value"] = int64_t(ledger_->record_word(static_cast<std::uint32_t>(offset)));
      return out;
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  Dictionary adopt_record(const PackedByteArray &record, const PackedByteArray &executable,
                          const PackedByteArray &picture_index) {
    try {
      if (record.size() != 0x4c30 || executable.is_empty())
        throw M::Refusal("player record or selected executable is absent");
      champions::simulation::rta::arena_modes::Executable image;
      if (!image.load(std::vector<std::uint8_t>(executable.ptr(), executable.ptr() + executable.size())))
        throw M::Refusal("selected executable is not a file-backed ELF32 image");
      auto reader = [&image](std::uint32_t at, std::size_t count) {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
          bytes.push_back(image.byte(at + static_cast<std::uint32_t>(i)));
        return bytes;
      };
      auto tables = champions::simulation::rta::script_items::image_tables(reader);
      if (!tables) throw M::Refusal("selected executable has no item tables");
      champions::inventory_icons::PictureTables pictures;
      for (std::uint32_t row = 0; row < 134; ++row) {
        const auto ptr = image.word(0x3e9ce0u + row * 0x10u + 4u);
        pictures.model_textures.push_back(ptr ? image.string(ptr) : std::string());
      }
      for (std::uint32_t row = 0; row < 73; ++row) {
        const auto at = 0x400e50u + row * 0x28u + 6u;
        pictures.prefix_colours.push_back({static_cast<std::int8_t>(image.byte(at)),
                                           static_cast<std::int8_t>(image.byte(at + 1)),
                                           static_cast<std::int8_t>(image.byte(at + 2))});
      }
      for (std::uint32_t row = 0; row < 12; ++row) {
        const auto ptr = image.word(0x4026d8u + row * 4u);
        if (!ptr) throw M::Refusal("body code is absent from the selected executable");
        pictures.body_codes[row] = image.string(ptr);
      }
      const auto candidate_pictures = pictures;
      auto next = std::make_unique<M::Ledger>();
      next->adopt_record(std::vector<std::uint8_t>(record.ptr(), record.ptr() + record.size()),
                         *tables, std::move(pictures),
                         [&image](std::uint32_t at) { return image.byte(at); });
      std::uint32_t unverified_definition = 0;
      if (!picture_index.is_empty()) {
        const auto index = champions::inventory_icons::parse_index(
            picture_index.ptr(), static_cast<std::size_t>(picture_index.size()));
        std::array<std::vector<M::IconSize>, M::BAGS> sizes;
        const auto names = next->bag_picture_names();
        for (int bag = 0; bag < M::BAGS; ++bag) {
          for (const auto &name : names[bag]) {
            const auto *entry = champions::inventory_icons::find_entry(index, name);
            // Index dimensions are runtime observations, not evidence that
            // the original builder rounds this footprint to one row.
            sizes[bag].push_back(entry ? M::IconSize{entry->width, entry->height}
                                       : M::IconSize{0, 0});
          }
        }
        next->build_display_grid(sizes);
        // Select a bounded-looking runtime footprint for the refusal probe
        // without recording a private item name or table address.
        const auto body = static_cast<std::int32_t>(next->record_word(0x54));
        const auto flags = std::uint64_t(next->record_word(0x138)) |
                           (std::uint64_t(next->record_word(0x13c)) << 32);
        const auto prefix = static_cast<std::uint8_t>(next->record_word(0x150) & 0xff);
        for (const auto &definition : tables->defs) {
          if (!definition || definition->category != 2) continue;
          champions::inventory_icons::PictureItem candidate;
          candidate.definition.name = definition->name;
          candidate.definition.category = definition->category;
          candidate.definition.slot = definition->slot;
          candidate.definition.subtype = definition->subtype;
          candidate.definition.damage_min = definition->damage_min;
          candidate.definition.damage_max = definition->damage_max;
          candidate.definition.picture = definition->picture;
          candidate.definition.model_row = static_cast<std::size_t>(definition->model_row);
          candidate.flags = flags;
          candidate.prefix = prefix;
          try {
            const auto name = champions::inventory_icons::picture_filename(candidate, body, candidate_pictures, {});
            const auto *entry = champions::inventory_icons::find_entry(index, name);
            if (entry && entry->width >= 0 && entry->width <= 64 &&
                entry->height > 0 && entry->height <= M::CURSOR_ROW) {
              unverified_definition = definition->addr;
              break;
            }
          } catch (const std::exception &) {
          }
        }
      }
      ledger_ = std::move(next);
      auto result = fill_state(success());
      result["unverified_definition"] = int64_t(unverified_definition);
      return result;
    } catch (const std::exception &error) {
      return failure(error);
    }
  }
  PackedByteArray record_bytes() const {
    const auto bytes = ledger_->record_bytes();
    PackedByteArray out;
    out.resize(static_cast<int64_t>(bytes.size()));
    std::memcpy(out.ptrw(), bytes.data(), bytes.size());
    return out;
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
        row["reason"] = String::utf8(gap.reason.c_str());
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
