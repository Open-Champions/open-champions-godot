// The level-script text commands on the session controller (docs/STRINGS.md):
// string tables and the help chime from the user's game folder, the HUD text
// state for drawing, and help_task's offline pause.
#include "session_controller.hpp"
#include "champions/script_commands.hpp"
#include "champions/strings.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <cstring>

using namespace godot;
namespace rta = champions::simulation::rta;
namespace st = champions::simulation::rta::strings;

namespace {
Dictionary text_failure(const std::exception &error) {
  Dictionary d;
  d["ok"] = false;
  d["error"] = String::utf8(error.what());
  return d;
}
PackedInt32Array units(const st::Units &text) {
  PackedInt32Array out;
  for (const auto u : text)
    out.push_back(u);
  return out;
}
std::vector<std::vector<std::uint8_t>> tables_of(const Array &tables) {
  std::vector<std::vector<std::uint8_t>> out;
  for (int64_t i = 0; i < tables.size(); ++i) {
    if (tables[i].get_type() != Variant::PACKED_BYTE_ARRAY)
      throw std::runtime_error("A text table is not a byte array.");
    const PackedByteArray bytes = tables[i];
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(bytes.size()));
    if (!raw.empty())
      std::memcpy(raw.data(), bytes.ptr(), raw.size());
    // Validates the stream before anything is registered.
    st::uni_entries(raw.data(), raw.size());
    out.push_back(std::move(raw));
  }
  return out;
}
} // namespace

void ChampionsSessionController::bind_text_methods() {
  ClassDB::bind_method(D_METHOD("set_text_sources", "tables", "help_chime_group"),
                       &ChampionsSessionController::set_text_sources);
  ClassDB::bind_method(D_METHOD("set_level_text", "tables"), &ChampionsSessionController::set_level_text);
  ClassDB::bind_method(D_METHOD("text_hud"), &ChampionsSessionController::text_hud);
  ClassDB::bind_method(D_METHOD("ui_frame"), &ChampionsSessionController::ui_frame);
  ClassDB::bind_method(D_METHOD("set_ui_frame_driver", "driven"), &ChampionsSessionController::set_ui_frame_driver);
  ClassDB::bind_method(D_METHOD("text_key_units", "key"), &ChampionsSessionController::text_key_units);
  ClassDB::bind_method(D_METHOD("text_lookup", "key"), &ChampionsSessionController::text_lookup);
  ClassDB::bind_method(D_METHOD("help_frame", "confirm"), &ChampionsSessionController::help_frame);
}

// Before preStart: GTEXT.LMP's tables, then the level lump's (docs/strings.md
// 1.1), the chime, and the camera. Camera_setMode writes 0 only while this
// machine's inventory, shop, dialog camera or a cutscene is up (research-ui
// docs/monsters-2.md 7.1 M6); the play session has none of them, so its local
// camera is in mode 1.
void ChampionsSessionController::apply_text(rta::script_commands::Host &host) {
  for (const auto &table : text_tables_)
    host.register_text(table);
  for (const auto &table : level_text_)
    host.register_text(table);
  if (help_chime_)
    host.set_help_chime(*help_chime_);
  host.hud().inputs.camera_mode = 1;
}

Dictionary ChampionsSessionController::set_text_sources(const Array &tables, int64_t chime) {
  try {
    if (chime < -1 || chime > 255)
      throw std::runtime_error("The help chime group is not a byte.");
    auto next = tables_of(tables);
    std::size_t entries = 0;
    for (const auto &t : next)
      entries += st::uni_entries(t.data(), t.size()).size();
    text_tables_ = std::move(next);
    help_chime_ = chime < 0 ? std::nullopt : std::optional<std::uint8_t>(static_cast<std::uint8_t>(chime));
    if (scripts_) {
      for (const auto &table : text_tables_)
        scripts_->register_text(table);
      if (help_chime_)
        scripts_->set_help_chime(*help_chime_);
    }
    Dictionary d;
    d["ok"] = true;
    d["tables"] = int64_t(text_tables_.size());
    d["entries"] = int64_t(entries);
    d["help_chime"] = help_chime_.has_value();
    return d;
  } catch (const std::exception &e) {
    return text_failure(e);
  }
}

Dictionary ChampionsSessionController::set_level_text(const Array &tables) {
  try {
    level_text_ = tables_of(tables);
    Dictionary d;
    d["ok"] = true;
    d["tables"] = int64_t(level_text_.size());
    return d;
  } catch (const std::exception &e) {
    return text_failure(e);
  }
}

Dictionary ChampionsSessionController::text_hud() const {
  Dictionary d;
  d["ok"] = false;
  if (!scripts_)
    return d;
  try {
    const auto &hud = scripts_->hud();
    const auto resolve = [&](std::uint32_t address) -> st::Units {
      if (address == 0)
        return {};
      if (hud.holds(address))
        return hud.text_at(address);
      return scripts_->text().text_at(address);
    };
    d["ok"] = true;
    d["has_text"] = scripts_->has_text();
    d["message"] = units(hud.message);
    d["message_frames"] = int64_t(static_cast<std::int16_t>(hud.msg_frames));
    d["message_force"] = int64_t(static_cast<std::int8_t>(hud.msg_force));
    d["suppressed"] = hud.inputs.suppressed;
    d["help_open"] = hud.help_open;
    d["help_has_title"] = hud.help_title != 0;
    d["help_title"] = units(resolve(hud.help_title));
    d["help_body"] = units(resolve(hud.help_body));
    d["help_close_delay"] = int64_t(hud.help_close_delay);
    d["menu_input_lockout"] = int64_t(hud.menu_input_lockout);
    d["help_paused"] = help_paused_;
    d["bust_shown"] = int64_t(hud.bust_shown);
    d["bust_texture"] = String::utf8(hud.bust_texture.c_str());
    d["bust_text"] = units(hud.bust_text);
    d["bust_value"] = int64_t(static_cast<std::int32_t>(hud.bust_value));
    d["bust_max"] = int64_t(static_cast<std::int32_t>(hud.bust_max));
    d["compass_shown"] = int64_t(hud.compass_shown);
    // g_simFrame (0x44fdcc) is not supplied by this host. The scene omits the
    // animated bar until a real simulation-frame observation is available.
    d["dialog_open"] = scripts_->dialog_attached() && scripts_->dialog().is_open();
    d["compass_label"] = units(hud.compass_label);
    d["latch"] = int64_t(hud.latch);
    d["pulse_starts"] = int64_t(scripts_->ui_pulse_starts());
    Dictionary reasons;
    for (const auto &[reason, n] : scripts_->counts().text_refusals)
      reasons[String::utf8(reason.c_str())] = int64_t(n);
    d["text_refusals"] = reasons;
    return d;
  } catch (const std::exception &e) {
    return text_failure(e);
  }
}

Dictionary ChampionsSessionController::text_lookup(const String &key) const {
  Dictionary out;
  out["ok"] = false;
  if (!scripts_ || !scripts_->has_text()) return out;
  try {
    const auto request = std::string(key.utf8().get_data());
    const auto address = scripts_->text().find(request);
    if (!address) return out;
    out["ok"] = true;
    out["units"] = units(scripts_->text().text_at(address));
  } catch (const std::exception &e) { return text_failure(e); }
  return out;
}

// An offline help box pauses the game until it is confirmed (docs/strings.md
// 3.3). help_task does this on the frame the box is drawn; the controller
// stops further ready ticks at once, so a catch-up submission does not run
// ticks the original would not.
void ChampionsSessionController::note_help_box() {
  if (!scripts_)
    return;
  const auto &hud = scripts_->hud();
  if (!hud.help_open || hud.inputs.network || current_.paused || current_.stopped || current_.failed)
    return;
  try {
    if (combat_) combat_->set_paused(true); else if (level_) level_->set_paused(true); else control_->set_paused(true);
    snap();
    help_paused_ = true;
  } catch (const std::exception &) {
    // A session that cannot pause keeps running; help_frame reports it.
  }
}

void ChampionsSessionController::release_help_pause() {
  if (!help_paused_)
    return;
  help_paused_ = false;
  try {
    if (combat_) combat_->set_paused(false); else if (level_) level_->set_paused(false); else control_->set_paused(false);
    snap();
  } catch (const std::exception &) {
  }
}

Dictionary ChampionsSessionController::help_frame(bool confirm) {
  try {
    Dictionary d;
    d["ok"] = false;
    if (!scripts_)
      return d;
    const auto frame = scripts_->help_task(confirm);
    if (frame.closed && !frame.paused)
      release_help_pause();
    else
      note_help_box();
    d["ok"] = true;
    d["closed"] = frame.closed;
    d["post_action"] = frame.run_post_action;
    d["paused"] = frame.paused;
    d["session_paused"] = bool(current_.paused);
    d["open"] = scripts_->hud().help_open;
    d["close_delay"] = int64_t(scripts_->hud().help_close_delay);
    d["menu_input_lockout"] = int64_t(scripts_->hud().menu_input_lockout);
    return d;
  } catch (const std::exception &e) {
    return text_failure(e);
  }
}

Dictionary ChampionsSessionController::ui_frame() {
  Dictionary d;
  d["ok"] = false;
  if (!scripts_)
    return d;
  const auto &r = scripts_->ui_ramps();
  PackedInt64Array pulse, fade;
  for (std::size_t i = 0; i < 4; ++i) {
    pulse.push_back(int64_t(r.pulse[i]));
    fade.push_back(int64_t(r.fade[i]));
  }
  d["pulse"] = pulse;
  d["fade"] = fade;
  scripts_->ui_frame_step();
  d["ok"] = true;
  return d;
}

Dictionary ChampionsSessionController::set_ui_frame_driver(bool driven) {
  ui_frame_driven_ = driven;
  Dictionary d;
  d["ok"] = true;
  d["driven"] = ui_frame_driven_;
  return d;
}

PackedInt32Array ChampionsSessionController::text_key_units(const String &key) const {
  PackedInt32Array out;
  if (!scripts_ || !scripts_->has_text())
    return out;
  for (const auto u : scripts_->text_peek(key.utf8().get_data()))
    out.push_back(int32_t(u));
  return out;
}
