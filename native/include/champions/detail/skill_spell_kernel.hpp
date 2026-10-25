// MIT; Copyright (c) 2026 TrustlessMike and contributors.
#pragma once
#include "champions/detail/skill_kernel.hpp"

// RTA publication order only, from the skills contracts pinned at 356add27.
// Adapters own addresses, validation, history sinks and shared draw accounting.
namespace champions::simulation::rta::detail::skill_spell_kernel {
template<class Read, class Publish>
Raw rng_publication(Read read, Publish publish) {
  const Raw stepped = skill_kernel::lcg_step(read());
  publish(stepped);
  return skill_kernel::lcg_result(stepped);
}
// Distinct constructor prefixes stay outside this tail. Address checks belong
// to the callbacks at their original boundaries; none are eagerly batched here.
// acquire returns the shared draw's upper16, not its full stepped state.
template<class Acquire, class PublishSeed, class PublishTable>
void constructor_tail(Acquire acquire, PublishSeed publish_seed,
                      PublishTable publish_table) {
  const Raw seed = acquire();
  publish_seed(seed);
  publish_table();
}
}
