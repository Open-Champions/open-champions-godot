#include "session_controller.hpp"
#include "champions/script_commands.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;
namespace rta = champions::simulation::rta;

namespace {
const char *service_refusal(const std::string &kind, bool alternate) {
  if (kind == "Savepoint") return "";
  if (kind == "NPC")
    return alternate ? "Alternate-hub dialog choice effects are not audited; no conversation was queued."
                     : "Use E near the NPC for the existing Player conversation and quest route.";
  if (kind == "PushTrigger")
    return alternate ? "Alternate-hub trigger entry effects are not audited; no action was run."
                     : "The level-action control opens native mission state; mission-map presentation and completion are not integrated.";
  if (kind == "Trigger")
    return alternate ? "Alternate-hub walk-in effects are not audited; no action was run."
                     : "The portal visual signal requires the separate walk-in path; this is not a use-button service.";
  if (kind == "Portal" || kind == "JumpGate")
    return "Gate discovery, party-distance checks, travel and arrival are not integrated in this host.";
  if (kind == "apprings" || kind == "fpprings")
    return "Effect-ring actions are not audited; no action was run.";
  return "This object has no audited hub use service.";
}
}

void ChampionsSessionController::bind_hub_methods() {
  ClassDB::bind_method(D_METHOD("hub_service_targets"), &ChampionsSessionController::hub_service_targets);
  ClassDB::bind_method(D_METHOD("hub_service_use", "index", "card_busy"), &ChampionsSessionController::hub_service_use);
}

Array ChampionsSessionController::hub_service_targets() const {
  Array targets;
  if (!scripts_ || (fight_level_ != "hub" && fight_level_ != "hub2"))
    return targets;
  for (std::size_t index = 0; index < script_level_objects_.size(); ++index) {
    const auto &record = script_level_objects_[index];
    if (record.cls != "Savepoint" && record.cls != "NPC" && record.cls != "PushTrigger" &&
        record.cls != "Portal" && record.cls != "JumpGate" && record.cls != "Trigger" &&
        record.cls != "apprings" && record.cls != "fpprings")
      continue;
    Dictionary item;
    item["index"] = int64_t(index);
    item["kind"] = String::utf8(record.cls.c_str());
    item["direct_use"] = record.cls == "Savepoint";
    item["engagement"] = "Host direct-use adapter; original proximity dispatch is not implemented.";
    item["refusal"] = service_refusal(record.cls, fight_level_ == "hub2");
    targets.push_back(item);
  }
  return targets;
}

Dictionary ChampionsSessionController::hub_service_use(int64_t index, bool card_busy) {
  Dictionary result;
  result["ok"] = false;
  result["request_save"] = false;
  try {
    if (!scripts_ || !player_ || !control_ ||
        (fight_level_ != "hub" && fight_level_ != "hub2"))
      throw std::runtime_error("Hub service requires an attached hub and a session Player.");
    if (index < 0 || std::size_t(index) >= script_level_objects_.size())
      throw std::runtime_error("No hub service at that object index.");
    const auto &record = script_level_objects_[std::size_t(index)];
    if (record.cls != "Savepoint")
      throw std::runtime_error(service_refusal(record.cls, fight_level_ == "hub2"));
    if (record.prop("action"))
      throw std::runtime_error("Savepoint action dispatch is not established for this record.");
    if (!scripts_->dialog_attached())
      throw std::runtime_error("Savepoint menu-mutex state is unavailable.");
    const bool held = scripts_->dialog().mutex != 0 || scripts_->dialog().is_open() ||
        scripts_->hud().help_open || scripts_->campaign().menu_open();
    auto &point = hub_savepoints_[std::size_t(index)];
    const auto use = rta::hub_services::use_savepoint(point, card_busy, held);
    if (use == rta::hub_services::Use::CardBusy)
      throw std::runtime_error("Savepoint refused: memory-card/save host is busy.");
    if (use == rta::hub_services::Use::MenuHeld)
      throw std::runtime_error("Savepoint refused: menu mutex is held.");
    result["ok"] = true;
    result["request_save"] = true;
    result["activated"] = int64_t(point.activated);
    result["effect_pending"] = int64_t(point.effect_pending);
    result["player_record_changed"] = false;
    result["persistence"] = "Native continuation save, not original memory-card persistence; savepoint effect completion is unimplemented.";
  } catch (const std::exception &error) {
    result["error"] = String::utf8(error.what());
  }
  return result;
}
