#include "session_controller.hpp"
#include <limits>

namespace rta = champions::simulation::rta;
using namespace godot;

Dictionary ChampionsSessionController::script_mission1_status() const {
  Dictionary out;
  const auto *c = scripts_ ? scripts_->mission1_contract() : nullptr;
  out["ok"] = bool(c);
  if (!c) return out;
  const bool strife = c->level == rta::mission1_scripts::Level::Strife;
  out["branch"] = strife ? 0 : 1; // good destination / evil destination
  out["addressed_only"] = true;
  out["continuous_execution"] = false;
  out["objective_reachability"] = false;
  out["combat"] = false;
  out["cutscene_playback"] = false;
  out["transitions"] = false;
  out["objects_validated"] = c->objects_validated;
  out["records"] = int64_t(c->census.records);
  out["factory_selected"] = int64_t(c->census.selected);
  out["factory_not_created"] = int64_t(c->census.not_created);
  out["hostile_placements"] = int64_t(c->census.hostiles);
  out["stage1_callbacks"] = int64_t(c->census.stage1);
  out["stage2_callbacks"] = int64_t(c->census.stage2);
  out["catapult_callbacks"] = int64_t(c->census.catapults);
  out["switch_pairs"] = int64_t(c->census.switches);
  int64_t travel = 0, demo = 0;
  for (const auto &t : c->travel) (t.demo_branch ? demo : travel) += 1;
  out["travel_sites"] = travel;     // admitted newWorld sites (docs/mission1-levels.md:410-417)
  out["travel_demo_refused"] = demo; // demoRunning branch, not executed (425-428)
  out["local_count"] = int64_t(scripts_->mission1_local_count());
  out["count_predicate"] = scripts_->mission1_local_count() == (strife ? 2 : 8);
  Array flags;
  if (strife) {
    for (std::size_t i=0; i<c->stage_keys.size(); ++i) {
      const bool flag = scripts_->variables().quest_get(c->stage_keys[i]) != 0;
      flags.push_back(i == 0 ? !flag : flag);
    }
  } else {
    for (const auto &s : c->switches) flags.push_back(scripts_->variables().quest_get(s.persistent) == 0);
    out["door_persistent"] = scripts_->variables().quest_get(c->door_flag) != 0;
  }
  out["predicates"] = flags;
  out["save_inhibit"] = scripts_->variables().memory.word(0x50605c) != 0;
  out["deferred_queued"] = int64_t(scripts_->queue().queued);
  out["deferred_count"] = int64_t(scripts_->queue().count);
  Dictionary sites;
  for (const auto &site : scripts_->mission1_sites_reached()) sites[int64_t(site.first)] = int64_t(site.second);
  out["sites_reached"] = sites;
  Array entries;
  for (const auto &entry : c->entries) entries.push_back(int64_t(entry.first));
  out["addressed_entries"] = entries;
  out["refusal_stops"] = int64_t(scripts_->counts().refusal_stops);
  out["unbuilt_objects"] = int64_t(scripts_->counts().objects_unbuilt);
  return out;
}
Dictionary ChampionsSessionController::script_mission1_action(const String &action, int64_t index) {
  Dictionary out;
  out["ok"] = false;
  out["diagnostic"] = true;
  if (!scripts_ || !scripts_->mission1_contract() || index < 0 ||
      uint64_t(index) > std::numeric_limits<uint32_t>::max()) {
    out["error"] = "No validated Mission 1 addressed action";
    return out;
  }
  using Stop = rta::script_commands::Stop;
  Stop stop = Stop::Refused;
  if (action == "entry") stop = scripts_->run_mission1_entry(uint32_t(index));
  else if (action == "restore") stop = scripts_->restore_mission1_addressed();
  else if (action == "count_gate") stop = scripts_->mission1_count_gate();
  else if (action == "bridge") stop = scripts_->mission1_bridge_request(unsigned(index));
  else if (action == "travel") stop = scripts_->mission1_travel(unsigned(index));
  out["ok"] = stop == Stop::Complete;
  out["refused"] = stop == Stop::Refused;
  out["fault"] = stop == Stop::Fault;
  out["value"] = int64_t(scripts_->last_value());
  if (stop == Stop::Refused) out["error"] = "Untraced entry, effect body, constructor or predicate";
  if (stop == Stop::Fault) out["error"] = "Addressed entry VM fault";
  return out;
}
