#pragma once
#include "champions/creature_anim/creature.hpp"

namespace champions::simulation::rta::creature_anim {
// Numeric call contract, not an AI scheduler. Inputs have already passed the
// original state filter. Choice is the clip-choosing draw, not a new RNG draw.
struct AnimOp {
  std::string kind;
  std::vector<std::int64_t> args;
};
struct Selection {
  std::vector<AnimOp> ops;
  std::uint32_t stored = 0;
};
struct Condition {
  bool returned = false;
  Raw field = 0, value = 0;
};
struct Choice {
  Raw random = 0;
  std::optional<Condition> condition;
};
struct ConditionalSelection {
  Condition condition;
  Selection selection;
};
struct DispatchCase {
  Selection baseline;
  std::vector<Selection> residues;
  std::map<Raw, Selection> points;
  std::vector<ConditionalSelection> conditions;
};
using EventSlots = std::array<std::pair<Raw, std::uint8_t>, 4>;
struct DispatchTable {
  std::string group;
  std::map<Raw, DispatchCase> states;
  std::map<Raw, EventSlots> events;
  std::map<std::string, std::map<Raw, EventSlots>> event_variants;
  std::vector<AnimOp> constructor;
  std::map<Raw, std::map<int, std::vector<std::string>>> consumers;
  // Executed probes: flags == 0 and flags == 0xffff, respectively. These
  // observations do not establish predicates for arbitrary flag combinations.
  std::map<Raw, std::pair<std::vector<Raw>, std::vector<Raw>>> transitions;
};
const std::map<std::string, DispatchTable> &dispatch_tables();
const DispatchTable &
dispatch_table(const std::string &name); // root/steered only
Selection select_dispatch(const std::string &, Raw filtered_state, Choice = {});
struct DispatchContext {
  Runtime &runtime;
  StateId state;
  std::function<ClipId(Raw entry)> entry;
  // Required if the selected ops contain a virtual/holder call. Delivers the
  // ordered call, including nested slot27; never silently executes baseline AI.
  std::function<void(const AnimOp &)> external;
};
void apply_dispatch(DispatchContext &, const Selection &);
void install_dispatch_events(DispatchContext &, const std::string &,
                             const std::string &variant = {});
struct DispatchEvent {
  std::string kind, consumer;
  Raw state = 0, word = 0;
  int mask = 0;
};
// Observations for downstream services; never performs damage, RNG or stores.
// Each row retains its source mask and the complete sign-extended event word.
std::vector<DispatchEvent> dispatch_events(const std::string &, Raw state,
                                           std::int32_t word);
std::vector<Raw> dispatch_transition_probe(const std::string &, Raw state,
                                           Raw flags);

// Player helpers implement animation dispatch only. The host calls the named
// action after the documented gameplay predicate is true; no invented AI/input
// filtering, combat scheduling or skill-driven state 6/7/9/14 dispatch.
void admit_player_state(Raw state);
Raw player_entry(Raw weapon_class, Raw offset);
struct PlayerInput {
  Raw weapon_class = 0, model = 0, variant = 1;
  std::int32_t relative_hit = 0;
  Raw draw = 0;
  bool no_combat_level = false, display_only = false;
};
using NamedClip = std::function<ClipId(const std::string &)>;
Selection player_action(const std::string &action, const PlayerInput &,
                        const NamedClip &);
// Named-resource ops use 'named_start'/'named_blend'; their first argument is
// an owned ClipId. apply_dispatch preserves their original order like entry
// ops.
void reset_player_idle(DispatchContext &, const PlayerInput &,
                       const NamedClip &);
void install_player_events(DispatchContext &);
void install_ground_pound_events(Runtime &, ClipId);
Raw player_combo_variant(Raw model, Raw weapon_class, Raw old, Raw sim_frame);
bool player_fidget(DispatchContext &, Raw stick_length, bool &active,
                   Raw weapon_class, const NamedClip &,
                   const std::function<Raw()> &shared_draw);
struct PlayerMove {
  Motion motion;
  Raw speed = 0, blend = 0, previous = 0;
};
// s is the already scaled/capped/slow-adjusted stick speed from Player_move.
PlayerMove player_locomotion(DispatchContext &, Raw s, Raw blend, Raw previous,
                             unsigned mode = 0);
std::vector<DispatchEvent> player_events(Raw state, std::int32_t word,
                                         bool copy = false,
                                         bool scripted_walk = false);
std::optional<Raw> player_marker_exit(Raw state, Raw flags);
} // namespace champions::simulation::rta::creature_anim
