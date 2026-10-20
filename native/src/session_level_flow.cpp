#include "champions/session_level_flow.hpp"
namespace champions::simulation::rta::level_flow {
SessionAdapter::SessionAdapter(State &s, Loader &l, SessionHooks h)
    : state_(s), loader_(l), hooks_(std::move(h)) {
  if (!hooks_.find_free_spot || !hooks_.shared_draw || !hooks_.terrain_height ||
      !hooks_.event || !hooks_.script || !hooks_.construct ||
      !hooks_.restore_level_state || !hooks_.construct_player ||
      !hooks_.remove_flag39_items || !hooks_.disable_unclaimed_pieces)
    throw Error("Session level-flow hooks must all be supplied");
}
bool SessionAdapter::request_script(const std::string &name,
                                    const std::vector<std::string> &props,
                                    bool net, int active, bool post,
                                    bool simple, bool parameter) {
  if (active < -1 || active > 3)
    throw Error("invalid active player");
  int alive = 0;
  for (auto &p : state_.players)
    alive += p.hp > 0;
  if (!parameter) {
    auto gate = new_world_gate(net, active,
                               active < 0 ? 0 : state_.players[active].cooldown,
                               alive, post);
    if (gate.first)
      return false;
    if (active >= 0)
      state_.players[active].cooldown = gate.second;
  }
  state_.pending.request(name,
                         simple ? ORIENT_NONE : orientation_from_props(props));
  return true;
}
GatePrompt SessionAdapter::prompt_gate(bool &found, bool script_ok,
                                       const std::vector<Raw> &distances,
                                       const std::string &hub) {
  if (!found && !state_.travel.menu_open && script_ok) {
    state_.gates.add_current(qualified_name(state_.level, state_.sub), hub);
    found = true;
  }
  return gate_prompt(found, state_.travel.menu_open, script_ok,
                     state_.gates.names.size(), distances);
}
bool SessionAdapter::advance_ready() {
  if (failed_)
    throw Error("failed load requires owner recovery");
  if (clear_travel_next_tick_) {
    state_.travel.arrived();
    clear_travel_next_tick_ = false;
  }
  return state_.travel.tick(state_.pending);
}
RecallEvent SessionAdapter::recall_tick(unsigned players, bool cutscene,
                                        std::uint32_t alive,
                                        const std::array<Vec3, 4> &positions,
                                        const std::string &hub) {
  if (failed_)
    throw Error("failed load requires owner recovery");
  return level_flow::recall_tick(state_, players, cutscene, alive, positions,
                                 hub);
}
LoadedLevel SessionAdapter::change(const ChangeOptions &opts,
                                   const std::vector<LevelRow> &rows,
                                   Placement p) {
  if (failed_)
    throw Error("failed load requires owner recovery");
  if (p.players > 4)
    throw Error("player count exceeds four");
  ready_ = false;
  try {
    hooks_.event("before_load_network_flush");
    auto change_opts = opts;
    change_opts.defer_recall_clear = true;
    auto result =
        begin_change(state_, change_opts, [&](const std::string &event) {
          if (event == "act_reset")
            hooks_.remove_flag39_items();
          hooks_.event(event);
        });
    auto level = loader_.load(
        state_.level, state_.sub, rows, state_.difficulty,
        state_.arrived_by_recall || bool(state_.restore_xy), p.pvp);
    state_.level_flags = level.row.flags;
    auto records = parse_objects({level.objects.data(), level.objects.size()});
    state_.spawn.rank = state_.loaded_from_save ? 3 : 0;
    state_.loaded_from_save = false;
    hooks_.script("preStart", level.script);
    auto build_records = [&](const std::vector<Record> &pass) {
      for (auto &r : pass) {
        hooks_.construct(r);
        auto &working = state_.spawn;
        auto cls = r.cls;
        for (auto &c : cls)
          if (c >= 'A' && c <= 'Z')
            c = char(c + 'a' - 'A');
        if (cls == "player1") {
          auto d = marker_decision(working.rank, r, state_.prev,
                                   state_.entrance, state_.arrived_by_recall);
          working.rank = d.second;
          if (d.first)
            for (unsigned i = 0; i < 4; ++i) {
              working.pos[i] = hooks_.find_free_spot(r.position, 0x42100000,
                                                     0x41c00000, false);
              working.facing[i] =
                  std::uint16_t(r.angle + hooks_.shared_draw() % 4000 - 2000);
            }
        } else if ((cls == "portal" || cls == "portalhurt") &&
                   state_.travel.ticks && !state_.travel.fixed) {
          for (unsigned i = 0; i < p.players; ++i) {
            working.pos[i] =
                hooks_.find_free_spot(r.position, 0x42100000, 0x41c00000, true);
            working.facing[i] = hooks_.shared_draw();
          }
          working.rank = 3;
        }
      }
      hooks_.event("flush_constructor_deletions");
    };
    if (result.had_saved_state) {
      build_records(hooks_.restore_level_state(
          state_.states.blobs.at(qualified_name(state_.level, state_.sub))));
      hooks_.event("restore_saved_decals_and_map");
      std::vector<Record> filtered;
      for (auto &r : records)
        if (save_filter_admits(r.cls))
          filtered.push_back(r);
      build_records(filtered);
    } else
      build_records(records);
    hooks_.script("start", level.script);
    if (p.pvp)
      hooks_.event("arena_begin");
    hooks_.disable_unclaimed_pieces();
    hooks_.event("paths_join_segments");
    if (!state_.post_action.empty()) {
      auto action = state_.post_action;
      state_.post_action.clear();
      hooks_.script(action, level.script);
    }
    hooks_.event("loading_end_light_autosave_dialog_immunity30");
    p.level_loaded = state_.level != "intro";
    p.restore_xy = state_.restore_xy;
    for (unsigned i = 0; i < 4; ++i) {
      p.alive[i] = state_.players[i].hp > 0;
      p.active[i] = state_.players[i].active;
    }
    for (auto slot : place_players(state_.spawn, p, hooks_.terrain_height,
                                   hooks_.find_free_spot)) {
      auto pos = state_.spawn.pos[slot];
      pos[2] = player_ctor_z(pos, PLACEMENT_RADIUS, hooks_.terrain_height);
      hooks_.construct_player(slot, pos, state_.spawn.facing[slot]);
    }
    apply_act_entry(state_.players, result.act_changed, opts.card_state,
                    opts.death_task);
    state_.restore_xy.reset();
    state_.arrived_by_recall = false;
    hooks_.event("warmup");
    hooks_.event("deferred_calls");
    hooks_.event("network_barrier");
    hooks_.event("shop_stock");
    hooks_.event("loading_screen_off");
    hooks_.event("environment_upload_if_resident");
    clear_travel_next_tick_ = state_.travel.ticks != 0;
    ready_ = true;
    return level;
  } catch (...) {
    failed_ = true;
    throw;
  }
}
} // namespace champions::simulation::rta::level_flow
