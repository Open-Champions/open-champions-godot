#pragma once
#include "champions/creature_hp.hpp"
#include "champions/objects.hpp"
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace champions::simulation::rta {
// One play-session address domain. The object World owns the pool and handle
// nodes; Memory owns the bytes. The registry is an address index, not a second
// object owner. Hosts and combat keep their typed views of registered objects.
class WorldAuthority {
public:
  std::shared_ptr<objects::World> objects = std::make_shared<objects::World>();
  creature_hp::Memory memory;

  void register_object(Object &object) {
    if (!object.id || (live_.count(object.id) && live_.at(object.id) != &object))
      throw std::logic_error("World address already registered");
    live_[object.id] = &object;
  }
  void unregister_object(const Object &object) {
    auto it = live_.find(object.id);
    if (it != live_.end() && it->second == &object) {
      factory_.erase(object.id);
      live_.erase(it);
    }
  }
  void register_factory(std::uint32_t address) {
    if (!find(address)) throw std::logic_error("factory object is not registered");
    factory_.insert(address);
  }
  bool is_factory(std::uint32_t address) const {
    return factory_.count(address) != 0 && find(address);
  }
  Object *find(std::uint32_t address) const {
    auto it = live_.find(address);
    return it == live_.end() ? nullptr : it->second;
  }

private:
  std::map<std::uint32_t, Object *> live_;
  std::set<std::uint32_t> factory_;
};
} // namespace champions::simulation::rta
