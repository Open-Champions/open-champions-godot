#include "champions/player.hpp"
#include "combat_runtime_internal.hpp"
// The real player tick as slot 4 (research ref_combat_full.FullRuntime /
// ref_combat_ptick.TickRuntime): Player_run 0x1a5030 is the existing exact
// player::Tick port on the shared memory; every call-out is answered by the
// composed world, a labelled fixture or a refusal.
namespace champions::simulation::rta::combat_runtime {
namespace P = player;
namespace {
Value to_value(const P::Argument &a) {
  if (auto x = std::get_if<std::uint32_t>(&a))
    return iv(*x);
  if (auto x = std::get_if<Vec3>(&a))
    return *x;
  if (std::holds_alternative<P::StackHandle>(a))
    return iv(STACK_HEAD);
  if (std::holds_alternative<P::NullArgument>(a))
    return std::monostate{};
  if (auto x = std::get_if<std::int64_t>(&a))
    return iv(std::int64_t(std::uint32_t(*x)));
  if (auto x = std::get_if<std::uint64_t>(&a))
    return iv(std::int64_t(std::uint32_t(*x)));
  throw Error("unsupported player call argument");
}
P::Result to_result(const Value &v) {
  if (auto x = std::get_if<std::int64_t>(&v))
    return std::uint32_t(*x);
  if (auto x = std::get_if<Vec3>(&v))
    return *x;
  if (auto x = std::get_if<A::Objects>(&v))
    return x->values;
  if (std::holds_alternative<std::monostate>(v))
    return std::uint32_t(0);
  if (auto x = std::get_if<A::CallbackResult>(&v)) {
    if (x->kind == A::CallbackResult::Kind::integer)
      return std::uint32_t(x->magnitude);
    if (x->kind == A::CallbackResult::Kind::none)
      return std::uint32_t(0);
  }
  throw Error("unsupported player call result");
}
} // namespace
// TickRuntime / FullRuntime services the tick reaches that no other model
// calls; the rest go through service().
Value tick_service(World::State &s, const std::string &n,
                   std::vector<Value> a) {
  auto ar = [&](unsigned k) { return number(a.at(k)); };
  auto slot_of = [&](const std::string &prefix) -> int {
    if (n.compare(0, prefix.size(), prefix) != 0)
      return -1;
    return std::stoi(n.substr(prefix.size()));
  };
  int slot;
  if ((slot = slot_of("Player.v")) >= 0 || (slot = slot_of("Creature.v")) >= 0) {
    if (n == "Creature.v0")
      return s.service(n, a);
    std::vector<Value> rest(a.begin() + 1, a.end());
    return s.dispatch_virtual(ar(0), unsigned(slot), std::move(rest));
  }
  if ((slot = slot_of("Spell.v")) >= 0) {
    std::vector<Value> rest(a.begin() + 1, a.end());
    return s.spell_virtual(ar(0), unsigned(slot), std::move(rest));
  }
  if ((slot = slot_of("Skill.v")) >= 0) {
    std::vector<Value> rest(a.begin() + 1, a.end());
    return s.skill_virtual(ar(0), unsigned(slot), std::move(rest));
  }
  if (n == "Creature_run")
    return iv(s.model.creature_run(ar(0), ar(1)) ? 1 : 0);
  return s.service(n, a);
}
void World::State::player_tick(Address p) {
  if (!tick_callbacks) {
    tick_callbacks = std::make_shared<P::Tick::NamedCallbacks>();
    for (unsigned k = 0; k < unsigned(P::Service::Named); ++k) {
      std::string name = P::service_name(P::Service(k));
      (*tick_callbacks)[name] = [this, name](P::Tick &,
                                             const std::vector<P::Argument> &a) {
        std::vector<Value> args;
        for (auto &x : a)
          args.push_back(to_value(x));
        if (name == "model_getAttachPoint") {
          if (!services.attach_point)
            throw Unsupported("attach point service missing");
          auto fn = services.attach_point;
          auto pos = fn(number(args.at(3)) - 0x114, number(args.at(0)),
                        std::get<Vec3>(args.at(1)), number(args.at(2)),
                        number(args.at(4)));
          if (args.size() > 5 && number(args.at(5)))
            return P::Result(P::AttachResult{pos, Vec3{}});
          return P::Result(pos);
        }
        // Player_run state 7 (take-off in water) and state 0xb (block-walk in
        // water) ripple the water; the original ignores the result
        // (docs/PLAYER_WADING.md, research player-wading.md section 6).
        if (name == "water_disturb") {
          if (!services.water_disturb)
            throw Unsupported("water_disturb service missing");
          services.water_disturb(std::get<Vec3>(args.at(1)), number(args.at(2)),
                                 number(args.at(3)));
        }
        return to_result(tick_service(*this, name, std::move(args)));
      };
    }
  }
  auto before = seed();
  P::Tick t(mem, p, P::Tick::Policy::RequireHandlers, tick_callbacks);
  std::string returned = "end";
  try {
    t.top();
    t.dispatch_state();
    run_queued_casts(p);
    t.tail();
  } catch (const P::Leave &leave) {
    returned = leave.what();
  }
  auto after = seed();
  std::uint64_t draws = 0;
  for (auto x = before; x != after; x = x * 0x19660dU + 0x3c6ef35fU)
    if (++draws > 100000)
      throw Error("the shared seed moved by more than 100000 draws");
  note("player_tick", {p, draws});
  trace.back().text = returned;
}
} // namespace champions::simulation::rta::combat_runtime
