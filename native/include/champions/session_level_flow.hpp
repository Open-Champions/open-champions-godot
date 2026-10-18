#pragma once
#include "champions/level_flow_loader.hpp"
namespace champions::simulation::rta::level_flow {
// Opt-in composition adapter. No mutation of Session/Controller/save glue.
// Owner must call advance_ready once per simulation tick and change at the next
// main-loop boundary. All callbacks are mandatory: missing integration refuses.
struct SessionHooks {
  FindSpot find_free_spot;
  Draw shared_draw;
  Height terrain_height;
  std::function<void(const std::string &)> event;
  std::function<void(const std::string &, const Blob &)> script;
  std::function<void(const Record &)> construct;
  // Decode saved object records without constructing them. Restore per-object
  // state in construct; restore decals/map on restore_saved_decals_and_map.
  // Returned records MUST preserve their restored grid order.
  std::function<std::vector<Record>(const Blob &)> restore_level_state;
  std::function<void(unsigned, const Vec3 &, std::uint16_t)> construct_player;
  std::function<void()> remove_flag39_items;
  std::function<void()> disable_unclaimed_pieces;
};
class SessionAdapter {
  State &state_;
  Loader &loader_;
  SessionHooks hooks_;
  bool ready_ = false, failed_ = false, clear_travel_next_tick_ = false;

public:
  SessionAdapter(State &, Loader &, SessionHooks);
  bool ready() const { return ready_; }
  bool failed() const { return failed_; }
  bool request_script(const std::string &,
                      const std::vector<std::string> &props, bool net,
                      int active, bool post = false, bool simple = false,
                      bool parameter = false);
  GatePrompt prompt_gate(bool &found, bool script_ok,
                         const std::vector<Raw> &other_alive_d2,
                         const std::string &hub);
  // Only invokes gate travel: owner routes triggers/recall through the same
  // ready clock.
  bool advance_ready();
  // level_flow::recall_tick on the owned state, once per simulation tick in
  // gameMiscSync's priority-1 slot, with the cutscene flag and alive count as
  // the previous tick left them. Runs during cutscenes and pause.
  RecallEvent recall_tick(unsigned players, bool cutscene_active,
                          std::uint32_t alive_count,
                          const std::array<Vec3, 4> &resolved_positions,
                          const std::string &hub);
  LoadedLevel change(const ChangeOptions &, const std::vector<LevelRow> &,
                     Placement);
};
} // namespace champions::simulation::rta::level_flow
