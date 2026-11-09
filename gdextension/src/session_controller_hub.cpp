#include "session_controller.hpp"
#include "champions/script_commands.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;
namespace rta = champions::simulation::rta;

namespace {
// research-hub-npcs@fbbfe65 docs/hub-services.md (HS). Both hubs follow the
// same researched rules; only the Unknowns below stay refused.
const char *service_refusal(const std::string &kind, bool) {
  if (kind == "Savepoint") return "";
  if (kind == "NPC")
    return "Use E near the NPC: its conversation runs the researched hub/hub2 table (HS:160-245); this selector does not open dialogs.";
  if (kind == "PushTrigger")
    return "Use the level-action control: key and action run the original entries (HS:361-391). Physical use at radius 79/78 is refused: this research asserts the radius values only, not the use query that applies them.";
  if (kind == "Trigger")
    return "The walk-in rectangle runs from the 15-list walk path and dispatches trigger(ring, 0) once (HS:393-401); it is not a use-button service.";
  if (kind == "Portal" || kind == "JumpGate")
    return "Gate discovery, party-distance checks, travel and arrival are not integrated in this host.";
  if (kind == "apprings" || kind == "fpprings")
    return "The ring is visual only (HS:403-409); its state requests are counted, and the rendered effect is Inferred.";
  return "This object has no audited hub use service.";
}
// Unknowns this research leaves open, refused with the evidence gap (HS:467-472,
// HSF-3). None is assigned a value.
const char *const UNKNOWNS[][2] = {
    {"inn_gold", "L-INN imports checkPlayerGold, givePlayerGold and takePlayerGold; their use is Unknown (not traced, HS:351-353, HSF-3). Whether L-INN charges for the stay is Unknown; no fee or refund is applied."},
    {"inn_route", "How a player reaches L-INN is Unknown: no hub object or hub script requests it (HS:354)."},
    {"hub_restore_charge", "The hub-side mana restore charges nothing (Executed, HS:325). That is not a claim about L-INN."},
    {"cutscene_caller", "The caller of hub public 0x34a4 is Unknown and the line list's link to that cutscene is Inferred (HS:66-69); the line list is never offered as a dialog."},
    {"help_box_pause", "Whether the warning help box pauses the game is Inferred, not executed (HS:381-382). A K.poll run whose gamePaused call comes while a help box is up refuses and stops the level script; it is never counted."},
    {"poll_copies", "What several queued K.poll copies do together is Unknown (HS:382-383). An addDeferedCall of K.poll while a copy is already pending (a second warning) refuses and stops the level script; K.poll re-queuing itself is admitted."},
    {"store_stack_index", "The stack-read activateStore index is fixture-dependent (HS:152-153); only backup-dialog writers use it and they are not wired."},
    {"dedupe_bag_cell", "Which bag cell removePlayerItem deletes is Unknown (HS:470)."},
};
}

void ChampionsSessionController::bind_hub_methods() {
  ClassDB::bind_method(D_METHOD("hub_service_targets"), &ChampionsSessionController::hub_service_targets);
  ClassDB::bind_method(D_METHOD("hub_service_use", "index", "card_busy"), &ChampionsSessionController::hub_service_use);
  ClassDB::bind_method(D_METHOD("hub_service_research"), &ChampionsSessionController::hub_service_research);
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
    if (record.cls == "PushTrigger") {
      // HS:363-366, Observed and asserted: nethostonly=1, radius 79 / 78.
      const auto hub = fight_level_ == "hub" ? rta::hub_dialog::Hub::Hub : rta::hub_dialog::Hub::Hub2;
      const auto radius = record.prop("radius"), host = record.prop("nethostonly");
      item["radius"] = String::utf8(radius.value_or("").c_str());
      item["nethostonly"] = String::utf8(host.value_or("").c_str());
      item["record_matches_research"] = radius == std::to_string(rta::hub_dialog::push_radius(hub)) &&
          host == std::to_string(rta::hub_dialog::NET_HOST_ONLY);
    }
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

Dictionary ChampionsSessionController::hub_service_research() const {
  Dictionary out;
  out["ok"] = false;
  if (!scripts_ || (fight_level_ != "hub" && fight_level_ != "hub2")) {
    out["error"] = "Hub research facts need an attached hub or hub2 script.";
    return out;
  }
  const auto facts = scripts_->hub_research();
  out["ok"] = true;
  out["hub"] = String::utf8(fight_level_.c_str());
  // HS:329-341: tables of 10 and 13 cells; the loops visit 9 and 12.
  out["dedupe_tables_match"] = facts.dedupe.has_value();
  out["shard_table_cells"] = int64_t(facts.dedupe ? facts.dedupe->shard_cells : 0);
  out["blood_table_cells"] = int64_t(facts.dedupe ? facts.dedupe->blood_cells : 0);
  out["shard_cells_visited"] = int64_t(rta::hub_dialog::SHARD_LIST);
  out["blood_cells_visited"] = int64_t(rta::hub_dialog::BLOOD_LIST);
  out["dialog_files"] = int64_t(facts.dialog_files);
  out["backup_dialogs"] = int64_t(facts.backups);
  out["cutscene_line_lists"] = int64_t(facts.cutscene_line_lists);
  out["cutscene_stem_requested"] = facts.cutscene_stem_requested;
  out["cutscene_playable"] = false;
  const auto *bound = scripts_->hub_services();
  out["dialog_table_bound"] = bound != nullptr;
  out["dialog_table"] = bound ? String(bound->hub == rta::hub_dialog::Hub::Hub ? "hub" : "hub2") : String();
  out["dialog_table_mismatch"] = String::utf8(scripts_->hub_services_mismatch().c_str());
  out["dialog_table_start"] = int64_t(scripts_->hub_services_start());
  const auto &counts = scripts_->dialog_counts();
  out["dialog_table_opens"] = int64_t(counts.hub_service_opens);
  out["dialog_table_actions"] = int64_t(counts.hub_service_actions);
  Dictionary fenced;
  for (const auto &[why, n] : counts.hub_service_refused) fenced[String::utf8(why.c_str())] = int64_t(n);
  out["dialog_action_refusals"] = fenced;
  // HS:32-34, 381-383: K.use / K.help / K.poll resolved at the researched
  // offsets, and the mission-map warning Unknowns refused in code.
  out["warn_entries_resolved"] = facts.warn_entries;
  Dictionary warned;
  for (const auto &[why, n] : counts.hub_warn_refused) warned[String::utf8(why.c_str())] = int64_t(n);
  out["warn_refusals"] = warned;
  out["warn_poll_runs"] = int64_t(rta::hub_dialog::WARN_POLL_RUNS);
  out["warn_poll_counts"] = "Script-task runs (on_ready_tick), not frames (HSF-2).";
  Dictionary unknowns;
  for (const auto &row : UNKNOWNS) unknowns[row[0]] = row[1];
  out["unknowns"] = unknowns;
  return out;
}
