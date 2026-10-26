#pragma once
#include "champions/objects.hpp"
#include <memory>
#include <utility>

// Shared host registry reconciliation, extracted from the audited campaign
// helpers. Pool addresses are reusable; World membership, pointer and saved
// serial identify an occupant. Never dereference a retained stale pointer.
namespace champions::simulation::rta::actor_lifetime {
inline bool owns_live_object(const objects::World &world,
                             const objects::GameObject &object) {
  const auto found = world.objects.find(object.addr);
  return found != world.objects.end() && found->second == &object && object.alive;
}
template<class Registry, class Accept>
void retire_stale(const objects::World &world, Registry &actors, Accept accept) {
  for (auto entry = actors.begin(); entry != actors.end();) {
    const auto live = world.objects.find(entry->first);
    if (live == world.objects.end() || live->second != entry->second->object ||
        live->second->serial != entry->second->object_serial || !live->second->alive ||
        !accept(*live->second))
      entry = actors.erase(entry);
    else
      ++entry;
  }
}
template<class Error, class Registry, class Actor, class Accept>
Actor &register_actor(const objects::World &world, Registry &actors,
                      std::unique_ptr<Actor> actor, Accept accept) {
  retire_stale(world, actors, accept);
  if (!owns_live_object(world, *actor->object) ||
      actor->object->serial != actor->object_serial || !accept(*actor->object))
    throw Error("actor object is not live in this World");
  const auto address = actor->object->addr;
  const auto inserted = actors.emplace(address, std::move(actor));
  if (!inserted.second) throw Error("address already has an actor");
  return *inserted.first->second;
}
} // namespace champions::simulation::rta::actor_lifetime
