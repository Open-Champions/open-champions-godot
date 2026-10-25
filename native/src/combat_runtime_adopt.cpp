#include "combat_runtime_internal.hpp"
// Adoption of script-host factory creatures into the Full composition. The
// factory (script_commands::Host with a class port) built the object over the
// shared authority memory; these entry points only give the executed combat
// bodies a record to act on. No game value is added here: the slot/function
// addresses and the base steps are the executed ones the arena model owns.
namespace champions::simulation::rta::combat_runtime {
void World::adopt_creature(Address a) {
  if (!s_->full())
    throw Unsupported("adopting a factory creature needs the Full composition");
  if (!s_->authority->is_factory(a))
    throw Error("not a ported factory object");
  if (s_->objects.count(a))
    throw Error("factory creature already has a combat record");
  auto record = std::make_unique<State::Actor>();
  record->id = a;
  record->kind = State::Kind::Creature;
  record->player = false;
  s_->objects[a] = std::move(record);
  // The authority already indexes this address to the host's object; the
  // combat record is a view and deliberately does not register itself.
}
void World::define_creature_table(Address table,
                                  const std::map<unsigned, Raw> &overrides) {
  s_->write_table(table, &overrides);
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
  s_->actor(a);
  const auto result = s_->model.filter_state(a, requested);
  return !result.refused && result.state == std::int32_t(requested);
}
void World::creature_on_death(Address a, Raw quiet) {
  s_->actor(a);
  s_->model.slot28_on_death(a, quiet);
}
std::size_t World::stub_box_skips() const { return s_->stub_box_skips; }
} // namespace champions::simulation::rta::combat_runtime
