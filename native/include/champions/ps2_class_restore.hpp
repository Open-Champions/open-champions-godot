#pragma once
#include "champions/ps2_save.hpp"
#include "champions/level_flow.hpp"
#include <optional>

namespace champions::simulation::rta::ps2_save {
// Bounded slot-8 class loaders for ALREADY CONSTRUCTED objects, not a world
// factory or native companion-format decoder. All references below are pinned
// to research-death-reload 0e3afdb8. Missing services throw at their call site.
// docs/death-reload.md:75-89 define these fields/call arguments; :91-97
// preserves Unknown for full Creature motion/corpse/registry and portal world
// effects. A callback must implement the supported service or throw Unknown
// BEFORE its unsupported mutation. Binding a no-op is only a synthetic recorder,
// never evidence of completion. Missing callbacks refuse at the exact call site;
// already applied supported prefixes are not rolled back. Callers must propagate
// refusal and must not revive, register or publish a partially restored object.
// A returned byte count proves only these class fields/services, never World
// membership, a completed level load or continuous playable death reload.
struct ClassLoadServices {
  // docs/death-reload.md:76-82; tools/difftest_death_reload.py:138-285.

  std::function<void(std::uint32_t)> variant, stop, reset_animation, end_animation,
      update, light, unlink;
  std::function<void(std::uint32_t, std::uint32_t, std::uint32_t)> start_animation;
  std::function<void(std::uint32_t, std::uint32_t)> motion, set_class;
  std::function<std::uint32_t(std::uint8_t)> class_definition;
  // Resolve the dead handle against the constructed class's clips. nullopt
  // means an established missing clip, not an unbound resolver.
  std::function<std::optional<std::uint32_t>(std::uint32_t, std::int32_t)> dead_clip;
  // Establish the tested type/optional-word domain; false refuses unknown
  // eligibility branches. No guessed type names or constructor assumptions.
  std::function<bool(std::uint32_t)> eligible_creature;
  std::function<std::uint32_t(std::uint32_t, std::uint32_t)> gate_asset;
  std::function<std::string(std::uint32_t)> string;
  std::function<std::uint32_t(const std::string &, const std::string &)> asset;
  // docs/death-reload.md:79-81,85-89: reuse native collision/height/pose,
  // not an approximate door transform or a replacement world factory.
  level_flow::Pieces *pieces = nullptr;
};
std::size_t restore_class_fields(builtins::Memory &, std::uint32_t object,
    const std::string &cls, const Bytes &, const ClassLoadServices &);
} // namespace champions::simulation::rta::ps2_save
