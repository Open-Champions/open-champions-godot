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
  // Set when the skill constructor writes the object flag word. Unset leaves
  // the caller's flags alone.
  std::optional<std::uint32_t> object_flags;
  Selection() = default;
  Selection(std::vector<AnimOp> operations, std::uint32_t stored_state,
            std::optional<std::uint32_t> flags = std::nullopt)
      : ops(std::move(operations)), stored(stored_state),
        object_flags(flags) {}
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
const DispatchTable &dispatch_table(const std::string &name);
Selection select_dispatch(const std::string &, Raw filtered_state, Choice = {});
// Agony state 0x12, word +0x2dc. Any bit of 0x380000 selects entry 0xd and
// wins over 0x40000; otherwise 0x40000 selects entry 9; otherwise entry 0xa.
Raw agony_hit_entry(Raw flags);
// Clockwork idle form index at +0x3c8. Equality, not a mask. Forms outside
// 0..5 have no clip.
std::optional<Raw> clockwork_idle_entry(Raw form);
struct FieldValue {
  Raw field = 0, value = 0;
};
struct CombinedDispatch {
  std::vector<AnimOp> ops;
  std::uint32_t stored = 0;
};
// Executed ops when every listed field is set. Unknown pairs refuse.
CombinedDispatch combined_dispatch(const std::string &cls, Raw state,
                                   const std::vector<FieldValue> &fields);
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
// action after the documented gameplay predicate is true. State 7 stays an
// explicit refusal. States 6, 9 and 0xe are the recorded skill constructors.
void admit_player_state(Raw state);
Raw player_entry(Raw weapon_class, Raw offset);
struct PlayerInput {
  Raw weapon_class = 0, model = 0, variant = 1;
  std::int32_t relative_hit = 0;
  Raw draw = 0;
  Raw object_flags = 0;
  bool no_combat_level = false, display_only = false;
};
using NamedClip = std::function<ClipId(const std::string &)>;
Selection player_action(const std::string &action, const PlayerInput &,
                        const NamedClip &);
// Named-resource ops use 'named_start'/'named_blend'/'named_push'; their first
// argument is an owned ClipId. apply_dispatch preserves their original order
// like entry ops.
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
