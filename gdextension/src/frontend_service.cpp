#include "champions/arena_executable.hpp"
#include "champions/frontend.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Godot bridge for champions::frontend (docs/FRONTEND.md). The flow, its
// timings and every state change are native; Godot draws a host layout and
// performs the requests (file choosers, session start, audio).
namespace {
using namespace godot;
namespace F = champions::frontend;

Dictionary failure(const std::exception &error) {
  Dictionary out;
  out["ok"] = false;
  out["error"] = String::utf8(error.what());
  return out;
}
std::string text_of(const String &value) {
  auto utf = value.utf8();
  return utf.get_data() ? std::string(utf.get_data()) : std::string();
}
std::vector<std::uint8_t> bytes_of(const PackedByteArray &data) {
  return std::vector<std::uint8_t>(data.ptr(), data.ptr() + data.size());
}
PackedByteArray packed(const std::vector<std::uint8_t> &data) {
  PackedByteArray out;
  out.resize(static_cast<int64_t>(data.size()));
  for (std::size_t i = 0; i < data.size(); ++i)
    out.set(static_cast<int64_t>(i), data[i]);
  return out;
}

class ChampionsFrontendService : public RefCounted {
  GDCLASS(ChampionsFrontendService, RefCounted)
  std::unique_ptr<F::Flow> flow_;
  Array pending_; // requests drained from the flow, handed to Godot once

  void drain() {
    for (const auto &r : flow_->take_requests()) {
      Dictionary d;
      d["kind"] = String::utf8(r.kind.c_str());
      d["arg"] = String::utf8(r.arg.c_str());
      d["value"] = int64_t(r.value);
      pending_.push_back(d);
    }
  }

  static String kind_name(F::Draw::Kind k) {
    switch (k) {
    case F::Draw::Kind::Sprite: return "sprite";
    case F::Draw::Kind::Frame: return "frame";
    case F::Draw::Kind::Rect: return "rect";
    case F::Draw::Kind::Text: return "text";
    }
    return "text";
  }
  static String align_name(F::Align a) {
    switch (a) {
    case F::Align::Left: return "left";
    case F::Align::Centred: return "centred";
    case F::Align::Wrapped: return "wrapped";
    case F::Align::WrappedCentred: return "wrapped_centred";
    }
    return "left";
  }

  Dictionary scene_of() {
    const auto scene = flow_->layout();
    Dictionary out;
    out["ok"] = true;
    out["screen"] = String::utf8(scene.screen.c_str());
    out["host_layout"] = scene.host_layout;
    out["host_label"] = String::utf8(scene.host_label.c_str());
    Array unknown;
    for (const auto &u : scene.unknown)
      unknown.push_back(String::utf8(u.c_str()));
    out["unknown"] = unknown;
    Array draws;
    int order = 0;
    for (const auto &d : scene.draws) {
      Dictionary item;
      item["order"] = order++;
      item["kind"] = kind_name(d.kind);
      item["evidence"] = String::utf8(F::evidence_name(d.evidence));
      item["layer"] = d.layer;
      item["x"] = d.x;
      item["y"] = d.y;
      item["w"] = d.w;
      item["h"] = d.h;
      item["colour"] = int64_t(d.colour);
      item["settled"] = d.settled;
      item["style"] = d.style;
      item["min_w"] = d.min_w;
      item["lump"] = String::utf8(d.lump.c_str());
      item["resource"] = String::utf8(d.resource.c_str());
      item["font"] = d.font == F::Font::Medium ? "medium" : "small";
      item["align"] = align_name(d.align);
      item["wrap_width"] = d.wrap_width;
      item["line"] = d.line;
      item["text"] = String::utf8(d.text.c_str());
      item["role"] = String::utf8(d.role.c_str());
      item["site"] = String::utf8(d.site);
      draws.push_back(item);
    }
    out["draws"] = draws;
    return out;
  }

  Dictionary view_of() {
    Dictionary out;
    out["ok"] = true;
    out["screen"] = String::utf8(F::screen_name(flow_->screen()));
    out["state"] = F::screen_state(flow_->screen());
    out["cursor"] = flow_->cursor();
    Array entries;
    for (const auto &e : flow_->entries()) {
      Dictionary d;
      d["id"] = String::utf8(e.id.c_str());
      d["label"] = String::utf8(e.label.c_str());
      d["evidence"] = String::utf8(e.evidence.c_str());
      d["enabled"] = e.enabled;
      d["refusal"] = String::utf8(e.refusal.c_str());
      d["type"] = e.type;
      d["value"] = int64_t(e.value);
      entries.push_back(d);
    }
    out["entries"] = entries;
    Array notes;
    for (const auto &n : flow_->notes())
      notes.push_back(String::utf8(n.c_str()));
    out["notes"] = notes;
    out["frames"] = flow_->frames_on_screen();
    out["paused"] = flow_->paused();
    out["lockout"] = flow_->lockout();
    out["skip_splash"] = flow_->skip_splash();
    out["in_frontend"] = flow_->in_frontend();
    out["options_file"] = flow_->options_file_present();
    out["music_live"] = int64_t(flow_->music().live);
    out["music_image"] = int64_t(flow_->music().image);
    out["slider_timer"] = int64_t(flow_->slider_timer());
    out["num_players"] = flow_->num_players();
    out["difficulty"] = flow_->difficulty_index();
    out["save_latch"] = flow_->save_latch();
    out["exit_code"] = flow_->last_exit_code();
    out["gated_music"] = flow_->gated_music();
    out["ability_remaining"] = flow_->ability_remaining();
    if (flow_->screen() == F::Screen::CharClass || flow_->screen() == F::Screen::CharAppearance) {
      const int preview_model = flow_->screen() == F::Screen::CharClass
                                    ? F::carousel_model(flow_->cursor()) : flow_->character().model;
      out["preview_model"] = preview_model;
      out["preview_body_code"] = String::utf8(F::body_code_of_model(preview_model));
    }
    out["error"] = String::utf8(flow_->last_error().c_str());
    const auto &c = flow_->character();
    Dictionary ch;
    ch["ready"] = flow_->character_ready();
    ch["name"] = String::utf8(c.name.c_str());
    ch["model"] = c.model;
    ch["class_index"] = c.class_index;
    ch["gold"] = c.gold;
    ch["level"] = c.level;
    ch["acknowledged"] = c.acknowledged;
    ch["stat_points"] = c.stat_points;
    ch["skill_points"] = c.skill_points;
    ch["skill_points_known"] = c.skill_points_known;
    ch["imported"] = c.imported;
    ch["base_known"] = c.base_known;
    ch["appearance_known"] = c.appearance_known;
    Array base, spent;
    for (auto v : c.base) base.push_back(v);
    for (auto v : c.spent) spent.push_back(v);
    ch["base"] = base;
    ch["spent"] = spent;
    Array stats;
    for (auto v : c.stats())
      stats.push_back(v);
    ch["stats"] = stats;
    out["character"] = ch;
    out["requests"] = pending_;
    pending_ = Array();
    return out;
  }

  template <class Fn> Dictionary guarded(Fn fn) {
    if (!flow_) {
      Dictionary out;
      out["ok"] = false;
      out["error"] = "call boot() first";
      return out;
    }
    try {
      fn();
      drain();
      return view_of();
    } catch (const std::exception &e) {
      drain();
      return failure(e);
    }
  }

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("boot", "options_file"), &ChampionsFrontendService::boot);
    ClassDB::bind_method(D_METHOD("view"), &ChampionsFrontendService::view);
    ClassDB::bind_method(D_METHOD("layout"), &ChampionsFrontendService::layout);
    ClassDB::bind_method(D_METHOD("set_card_cache", "metadata", "headers", "count", "scroll_base"),
                         &ChampionsFrontendService::set_card_cache);
    ClassDB::bind_method(D_METHOD("set_card_preview", "header_index", "debug_time"),
                         &ChampionsFrontendService::set_card_preview);
    ClassDB::bind_method(D_METHOD("frame", "buttons", "stick_x", "stick_y"), &ChampionsFrontendService::frame, DEFVAL(128), DEFVAL(128));
    ClassDB::bind_method(D_METHOD("set_name", "name"), &ChampionsFrontendService::set_name);
    ClassDB::bind_method(D_METHOD("load_class_start_stats", "executable_path"), &ChampionsFrontendService::load_class_start_stats);
    ClassDB::bind_method(D_METHOD("import_result", "result"), &ChampionsFrontendService::import_result);
    ClassDB::bind_method(D_METHOD("load_result", "ok", "error"), &ChampionsFrontendService::load_result, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("options_loaded", "ok", "file"), &ChampionsFrontendService::options_loaded);
    ClassDB::bind_method(D_METHOD("save_result", "ok", "error"), &ChampionsFrontendService::save_result, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("recall_executed"), &ChampionsFrontendService::recall_executed);
    ClassDB::bind_method(D_METHOD("set_dialog_open", "open"), &ChampionsFrontendService::set_dialog_open);
    ClassDB::bind_method(D_METHOD("return_to_frontend"), &ChampionsFrontendService::return_to_frontend);
    ClassDB::bind_method(D_METHOD("session_exit", "code"), &ChampionsFrontendService::session_exit);
    ClassDB::bind_method(D_METHOD("request_save"), &ChampionsFrontendService::request_save);
    ClassDB::bind_method(D_METHOD("attach_play"), &ChampionsFrontendService::attach_play);
    ClassDB::bind_method(D_METHOD("set_stream2_playing", "playing"), &ChampionsFrontendService::set_stream2_playing);
    ClassDB::bind_method(D_METHOD("options_image"), &ChampionsFrontendService::options_image);
    ClassDB::bind_static_method("ChampionsFrontendService", D_METHOD("options_size"), &ChampionsFrontendService::options_size);
  }

public:
  static int64_t options_size() { return int64_t(F::OPTIONS_BYTES); }

  // options_file: the stored 760-byte image, or empty for a card without one.
  Dictionary boot(const PackedByteArray &options_file) {
    try {
      std::optional<std::vector<std::uint8_t>> file;
      if (options_file.size() > 0)
        file = bytes_of(options_file);
      flow_ = std::make_unique<F::Flow>(file);
      pending_ = Array();
      drain();
      return view_of();
    } catch (const std::exception &e) {
      flow_.reset();
      return failure(e);
    }
  }
  Dictionary view() { return guarded([] {}); }
  // The documented draw calls of the current screen.
  Dictionary layout() {
    if (!flow_) {
      Dictionary out;
      out["ok"] = false;
      out["error"] = "call boot() first";
      return out;
    }
    try {
      return scene_of();
    } catch (const std::exception &e) {
      return failure(e);
    }
  }
  Dictionary frame(int64_t buttons, int64_t stick_x, int64_t stick_y) {
    return guarded([&] {
      if (buttons < 0 || buttons > 0xffff || stick_x < 0 || stick_x > 255 || stick_y < 0 || stick_y > 255)
        throw F::Refusal("pad word or stick byte out of range");
      flow_->frame(static_cast<std::uint16_t>(buttons), static_cast<std::uint8_t>(stick_x),
                   static_cast<std::uint8_t>(stick_y));
    });
  }
  Dictionary set_card_cache(const PackedByteArray &metadata, const PackedByteArray &headers,
                            int64_t count, int64_t scroll_base) {
    return guarded([&] {
      if (count < 0 || count > std::numeric_limits<int>::max() || scroll_base < 0 ||
          scroll_base > std::numeric_limits<int>::max())
        throw F::Refusal("card count or scroll is outside int range");
      flow_->set_card_cache(bytes_of(metadata), bytes_of(headers), int(count), int(scroll_base));
    });
  }
  Dictionary set_card_preview(int64_t header_index, bool debug_time) {
    return guarded([&] {
      if (header_index < 0 || header_index > std::numeric_limits<int>::max())
        throw F::Refusal("card preview index is outside int range");
      flow_->set_card_preview(int(header_index), debug_time);
    });
  }
  Dictionary set_name(const String &name) { return guarded([&] { flow_->set_name(text_of(name)); }); }

  // g_classStartStats 0x4ef400, seven rows of four s32, read from the user's
  // executable at run time. Nothing read here is stored outside this process.
  Dictionary load_class_start_stats(const String &path) {
    return guarded([&] {
      auto file = FileAccess::open(path, FileAccess::READ);
      if (file.is_null())
        throw F::Refusal("the executable cannot be opened");
      auto data = file->get_buffer(static_cast<int64_t>(file->get_length()));
      champions::simulation::rta::arena_modes::Executable exe;
      if (!exe.load(bytes_of(data)))
        throw F::Refusal("the executable is not an ELF32 image");
      std::array<std::array<std::int32_t, 4>, F::CLASSES> rows{};
      for (int c = 0; c < F::CLASSES; ++c)
        for (int s = 0; s < 4; ++s)
          rows[static_cast<std::size_t>(c)][static_cast<std::size_t>(s)] = static_cast<std::int32_t>(
              exe.word(F::CLASS_START_STATS + static_cast<std::uint32_t>(c * 16 + s * 4)));
      flow_->set_class_start_stats(rows);
    });
  }

  // result: the ChampionsPs2SaveImport dictionary (ok, name, class_index, ...).
  Dictionary import_result(const Dictionary &result) {
    return guarded([&] {
      const bool ok = bool(result.get("ok", false));
      F::Character c;
      if (ok) {
        c.name = text_of(String(result.get("name", "")));
        c.model = int(result.get("model_index", 0));
        c.class_index = int(result.get("class_index", 0));
        c.level = int(result.get("level", 1));
        c.acknowledged = int(result.get("acknowledged", c.level));
        c.gold = int(result.get("gold", 0));
        c.stat_points = int(result.get("stat_points", 0));
        c.skill_points = int(result.get("skill_points", 0));
        c.skill_points_known = result.has("skill_points");
        Array stats = result.get("base_stats", Array());
        for (int i = 0; i < 4 && i < stats.size(); ++i)
          c.base[static_cast<std::size_t>(i)] = int(stats[i]);
        c.base_known = stats.size() == 4;
        const auto level = text_of(String(result.get("level_name", "")));
        const auto sub = text_of(String(result.get("subname", "")));
        c.level_name = sub.empty() ? level : level + "_" + sub;
      }
      flow_->import_result(ok, c, text_of(String(result.get("error", ""))));
    });
  }
  Dictionary load_result(bool ok, const String &error) {
    return guarded([&] { flow_->load_result(ok, text_of(error)); });
  }
  Dictionary options_loaded(bool ok, const PackedByteArray &file) {
    return guarded([&] {
      flow_->options_loaded(ok ? std::optional<std::vector<std::uint8_t>>(bytes_of(file)) : std::nullopt);
    });
  }
  Dictionary save_result(bool ok, const String &error) {
    return guarded([&] { flow_->save_result(ok, text_of(error)); });
  }
  Dictionary recall_executed() { return guarded([&] { flow_->recall_executed(); }); }
  // dialog_isOpen: offline the start button cannot open the pause menu and its recall and exit
  // entries are not selectable (docs/spec/dialog-gaps.md 3).
  Dictionary set_dialog_open(bool open) { return guarded([&] { flow_->set_dialog_open(open); }); }
  Dictionary return_to_frontend() { return guarded([&] { flow_->return_to_frontend(); }); }
  Dictionary session_exit(int64_t code) { return guarded([&] { flow_->session_exit(int(code)); }); }
  Dictionary request_save() { return guarded([&] { flow_->request_save(); }); }
  Dictionary attach_play() { return guarded([&] { flow_->attach_play(); }); }
  Dictionary set_stream2_playing(bool playing) { return guarded([&] { flow_->set_stream2_playing(playing); }); }
  PackedByteArray options_image() { return flow_ ? packed(flow_->options_image()) : PackedByteArray(); }
};
} // namespace

void register_frontend_service() { GDREGISTER_CLASS(ChampionsFrontendService); }
