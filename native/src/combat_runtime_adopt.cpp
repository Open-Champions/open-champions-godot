#include "combat_runtime_internal.hpp"
// Adoption of script-host factory creatures into the Full composition. The
// factory (script_commands::Host with a class port) built the object over the
// shared authority memory; these entry points only give the executed combat
// bodies a record to act on. No game value is added here: the slot/function
// addresses and the base steps are the executed ones the arena model owns.
namespace champions::simulation::rta::combat_runtime {
arena_monsters::Model &World::creature_services() {
  s_->install_ctor_services();
  auto &m = s_->model;
  auto bind = [&](const char *name, A::Model::Callback fn) { m.callbacks[name]=std::move(fn); };
  bind("Creature_processDamage",[](auto &model,const auto &a)->A::Value {
    model.process_damage(number(a.at(0)),number(a.at(1))); return std::int64_t(0);
  });
  bind("Creature_run",[](auto &model,const auto &a)->A::Value {
    return std::int64_t(model.creature_run(number(a.at(0)),number(a.at(1))));
  });
  bind("Creature_updateActive",[](auto &model,const auto &a)->A::Value {
    return std::int64_t(model.update_active(number(a.at(0))));
  });
  bind("Creature_stepAnim",[](auto &model,const auto &a)->A::Value {
    return std::int64_t(model.step_anim(number(a.at(0)),number(a.at(1))));
  });
  bind("Creature_canSeeTargetIn",[](auto &model,const auto &a)->A::Value {
    return model.can_see_target_in(number(a.at(0)),number(a.at(1)),number(a.at(2)),
        number(a.at(3)),number(a.at(4)),number(a.at(5)),number(a.at(6)));
  });
  bind("Creature_turnToTarget",[](auto &model,const auto &a)->A::Value {
    return model.turn_to_target(number(a.at(0)));
  });
  bind("Creature_settle",[](auto &model,const auto &a)->A::Value {
    model.settle(number(a.at(0))); return std::int64_t(0);
  });
  bind("Creature_moveBy",[](auto &model,const auto &a)->A::Value {
    return std::int64_t(model.move_by(number(a.at(0)),std::get<Vec3>(a.at(1))));
  });
  bind("Creature_moveToTarget",[](auto &model,const auto &a)->A::Value {
    model.move_to_target(number(a.at(0))); return std::int64_t(0);
  });
  bind("Creature_moveToDest",[](auto &model,const auto &a)->A::Value {
    return std::int64_t(model.move_to_dest(number(a.at(0))));
  });
  bind("Creature_fleeFromTarget",[](auto &model,const auto &a)->A::Value {
    model.flee_from_target(number(a.at(0)),number(a.at(1))); return std::int64_t(0);
  });
  bind("Creature_isAwayFromHome",[](auto &model,const auto &a)->A::Value {
    return std::int64_t(model.is_away_from_home(number(a.at(0))));
  });
  bind("Creature_isTargetCharging",[](auto &model,const auto &a)->A::Value {
    return model.is_target_charging(number(a.at(0)));
  });
  bind("Creature_isNotRooted",[](auto &model,const auto &a)->A::Value {
    return model.is_not_rooted(number(a.at(0)));
  });
  bind("Creature_isAttackReady",[](auto &model,const auto &a)->A::Value {
    return model.is_attack_ready(number(a.at(0)));
  });
  bind("Creature_isNotStunned",[](auto &model,const auto &a)->A::Value {
    return model.is_not_stunned(number(a.at(0)));
  });
  bind("Creature_isTargetAttackingMe",[](auto &,const auto &)->A::Value {
    throw Unsupported("Creature_isTargetAttackingMe: body untraced by the pinned Minotaur contract");
  });
  return m;
}
void World::unadopt_creature(Address a) {
  if (!s_->authority->find(a) || !s_->authority->objects->objects.count(a))
    throw Error("not a registered host combat view");
  s_->objects.erase(a);
}
void World::adopt_creature(Address a, bool during_constructor) {
  if (!s_->full())
    throw Unsupported("adopting a factory creature needs the Full composition");
  if (!s_->authority->is_factory(a) &&
      !(during_constructor && s_->authority->find(a) && s_->authority->objects->objects.count(a)))
    throw Error("not a ported factory object");
  if (s_->objects.count(a))
    throw Error("factory creature already has a combat record");
  // GameObject_ctor already linked these three heads in the factory World.
  // Bind the combat view to those exact heads. Allocating an empty head at
  // +8 loses the existing grid backlink as soon as root motion moves it.
  auto factory = s_->authority->objects->objects.find(a);
  if (factory == s_->authority->objects->objects.end())
    throw Error("factory creature has no owning GameObject");
  for (unsigned k = 0; k < 3; ++k) {
    auto cached = s_->heads.find(a + 4 * k);
    if (cached != s_->heads.end() && cached->second != &factory->second->heads[k] && cached->second->next)
      throw Error("adopting creature over a live combat head");
  }
  auto record = std::make_unique<State::Actor>();
  record->id = a;
  record->kind = State::Kind::Creature;
  record->player = false;
  s_->objects[a] = std::move(record);
  for (unsigned k = 0; k < 3; ++k) {
    const auto address = a + 4 * k;
    auto *cell = &factory->second->heads[k];
    auto cached = s_->heads.find(address);
    if (cached != s_->heads.end() && cached->second != cell) {
      s_->links.erase(cached->second);
      s_->owned_heads.erase(address);
    }
    s_->heads[address] = cell;
    s_->links[cell] = address;
  }
  // The authority already indexes this address to the host's object; the
  // combat record is a view and deliberately does not register itself.
}
void World::define_creature_table(Address table,
                                  const std::map<unsigned, Raw> &overrides) {
  s_->write_table(table, &overrides);
}
void World::adopt_prop(Address a, std::function<void()> retire) {
  auto owner = s_->authority->objects->objects.find(a);
  if (owner == s_->authority->objects->objects.end() || !owner->second->alive ||
      s_->authority->find(a) != owner->second || !retire || s_->adopted_props.count(a))
    throw Error("adopted prop requires its live registered owner and retirement callback");
  adopt_creature(a);
  s_->objects.at(a)->kind = State::Kind::Effect;
  if (!s_->adopted_props.emplace(a, std::make_pair(owner->second, std::move(retire))).second)
    throw Error("adopted prop identity already indexed");
}
void World::set_class_body(
    Raw function,
    std::function<void(Address, const std::vector<std::uint64_t> &)> body) {
  s_->class_hooks[function] = body;
  s_->model.class_bodies[function] =
      [body](A::Model &, Address self, const std::vector<A::Value> &args) -> A::Value {
    std::vector<std::uint64_t> numbers;
    for (const auto &value : args)
      if (auto n = std::get_if<std::int64_t>(&value))
        numbers.push_back(std::uint64_t(*n));
    body(self, numbers);
    return std::monostate{};
  };
}
bool World::creature_run(Address a) {
  s_->actor(a);
  return s_->model.creature_run(a, s_->model.class_model(a)) != 0;
}
bool World::creature_update_active(Address a) {
  s_->actor(a);
  return s_->model.update_active(a);
}
Raw World::creature_step_anim(Address a, Raw speed) {
  s_->actor(a);
  return s_->model.step_anim(a, speed);
}
bool World::creature_away_from_home(Address a) {
  s_->actor(a);
  return s_->model.is_away_from_home(a);
}
bool World::creature_filter_state(Address a, std::uint32_t requested) {
  return creature_filter_state_result(a, requested) == requested;
}
std::optional<std::uint32_t> World::creature_filter_state_result(Address a, std::uint32_t requested) {
  s_->actor(a);
  const auto result = s_->model.filter_state(a, requested);
  if (result.refused) return std::nullopt;
  return std::uint32_t(result.state);
}
bool World::creature_can_see_target(Address a, Raw force) {
  s_->actor(a);
  return s_->model.slot25_can_see_target(a, force) != 0;
}
void World::creature_clear_path(Address a) { s_->actor(a); s_->model.clear_path(a); }
void World::creature_pick_wander_dest(Address a) { s_->actor(a); s_->model.pick_wander_dest(a); }
void World::creature_move_to_dest(Address a) { s_->actor(a); s_->model.move_to_dest(a); }
void World::creature_move_to_target(Address a) { s_->actor(a); s_->model.move_to_target(a); }
void World::creature_turn_to_target(Address a) { s_->actor(a); s_->model.turn_to_target(a); }
void World::creature_flee_from_target(Address a, Raw kind) {
  s_->actor(a); s_->model.flee_from_target(a, kind);
}
std::size_t World::stub_box_skips() const { return s_->stub_box_skips; }
} // namespace champions::simulation::rta::combat_runtime
