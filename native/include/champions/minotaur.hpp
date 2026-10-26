#pragma once
#include "champions/arena_monsters.hpp"

// RTA US v2.00, research-minotaur 27eb6ae, docs/spec/minotaur.md.
// These are class bodies over the caller's shared Creature memory/services.
// Resource words are runtime identities, never embedded asset identifiers.
namespace champions::simulation::rta::minotaur {
namespace A = arena_monsters;
constexpr Raw ALLOCATION = 0x470, CONSTRUCTOR = 0x288fdc,
              DEFINITION = 0x501200, VTABLE = 0x5165f0,
              RUN = 0x2894b8, DRAW = 0x289400, SET_STATE = 0x28a16c;
struct EventParams {
  Raw entry;
  std::array<Raw, 4> masks, times;
};
extern const std::array<EventParams, 8> EVENTS;
// GameObject_ctor and Creature_ctor must already have completed. This does
// not allocate, roll HP again, or replace the caller's properties/identity.
void finish_constructor(A::Model &, A::Address, const std::string &type_name);
// Original ABI: this, type name, position, signed16 angle, properties.
void constructor(A::Model &, A::Address, Raw type_name, const A::Vec3 &,
                 Raw angle, Raw properties);
void set_state(A::Model &, A::Address, Raw requested);
void run(A::Model &, A::Address);
void draw(A::Model &, A::Address);
// model_buildHideMask metadata arithmetic; bounds are checked before reads.
std::uint64_t hide_mask(const std::uint8_t *, std::size_t, Raw selector);
} // namespace champions::simulation::rta::minotaur
