#include "champions/combat_play_effects.hpp"
#include "combat_runtime_internal.hpp"
#include <utility>

namespace champions::simulation::rta::combat_play {
namespace C = combat_runtime;
using fxskills::Callout;
using fxskills::Value;

namespace {
constexpr Raw G_SIM_FRAME = 0x44fdcc; // the effect model's g_simFrame (= combat FRAME)
Value vec(const std::vector<std::uint64_t> &a, std::size_t at) {
  return Value::vec({Raw(a.at(at)), Raw(a.at(at + 1)), Raw(a.at(at + 2))});
}
Value integer(std::uint64_t v) { return Value::integer(v & 0xffffffffu); }
std::int64_t s16(std::uint64_t v) {
  return static_cast<std::int64_t>(static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)));
}
const Value &arg(const Callout &c, std::size_t k) {
  if (k >= c.args.size()) throw C::Unsupported(c.name + ": missing argument");
  return c.args[k];
}
std::uint64_t int_arg(const Callout &c, std::size_t k) {
  const Value &v = arg(c, k);
  if (!v.is_int()) throw C::Unsupported(c.name + ": argument is not an integer");
  return v.i;
}
Vec3 vec_arg(const Callout &c, std::size_t k) {
  const Value &v = arg(c, k);
  if (!v.is_sequence() || v.items.size() != 3) throw C::Unsupported(c.name + ": argument is not a vector");
  Vec3 out{};
  for (unsigned i = 0; i < 3; ++i) {
    if (!v.items[i].is_int()) throw C::Unsupported(c.name + ": vector lane");
    out[i] = Raw(v.items[i].i);
  }
  return out;
}
// The canonical descriptor value of the effect models (skills_fx.cpp desc()):
// ('desc', <header>, (type, a, b, c), (0xffff), (0xffff), (0xffff)).
std::vector<std::array<std::int64_t, 4>> descriptor(const Value &d) {
  static const std::string HEADER = "00000000000000000000000000000000ffff";
  if (d.kind != Value::Kind::Tuple || d.items.size() != 6 || d.items[0].s != "desc" || d.items[1].s != HEADER)
    throw C::Unsupported("damage_apply descriptor outside the documented effect-model shape");
  std::vector<std::array<std::int64_t, 4>> rows;
  for (std::size_t r = 2; r < 6; ++r) {
    const Value &row = d.items[r];
    if (!row.is_sequence() || row.items.empty() || row.items.size() > 4)
      throw C::Unsupported("damage_apply descriptor row");
    std::array<std::int64_t, 4> out{0, 0, 0, 0};
    for (std::size_t k = 0; k < row.items.size(); ++k) {
      if (!row.items[k].is_int()) throw C::Unsupported("damage_apply descriptor entry");
      out[k] = s16(row.items[k].i);
    }
    rows.push_back(out);
  }
  return rows;
}
} // namespace

Effects::Effects(Session &session, std::shared_ptr<const fxskills::RecipeConstants> constants)
    : session_(session), shared_(0), sim_(shared_, std::move(constants)) {
  sim_.hooks.callout = [this](const Callout &c) { return answer_game(c); };
  // The objects these recipes construct are refused by the combat runtime
  // (docs/COMBAT_RUNTIME.md, Effects that are not cosmetic): never allocated here.
  sim_.hooks.pool_new = [this](std::uint32_t) -> std::uint32_t {
    session_.world().refuse("fx_spawn id 5 Fire / fx_shatterToLooseIce / GutChunk objects");
  };
  sim_.hooks.construct = [this](std::uint32_t, const champions::rta::fx::recipes::Call &) {
    session_.world().refuse("Fire_ctor / LooseIceChunk / GutChunk_ctorFromProp");
  };
  session_.set_tick_observer([this](Session &s) { tick(s); });
}

Effects::~Effects() { session_.set_tick_observer({}); }

void Effects::attach(fxsim::EffectWorld *world) { sim_.attach(world); }

template <class F> void Effects::with_shared(F &&f) {
  auto &world = session_.world();
  auto &model = sim_.classes();
  const auto seed = std::uint32_t(world.read(C::SHARED_SEED));
  shared_.state = seed;
  model.mem.poke(C::SHARED_SEED, 4, seed);
  model.mem.poke(G_SIM_FRAME, 4, world.read(C::FRAME));
  auto publish = [&] {
    // One generator: whichever half drew last holds the newest state.
    std::uint32_t next = shared_.state;
    const auto model_seed = std::uint32_t(model.mem.load(C::SHARED_SEED, 4));
    if (model_seed != seed) next = model_seed;
    if (next != seed) world.write(C::SHARED_SEED, 4, next);
  };
  try {
    f();
  } catch (...) {
    publish();
    throw;
  }
  publish();
}

std::optional<Value> Effects::answer_game(const Callout &c) {
  auto &world = session_.world();
  if (c.name == "world_collectObjectsInBox") {
    ++counts_.game_queries;
    std::vector<Value> found;
    for (auto a : world.collect_box(vec_arg(c, 0), vec_arg(c, 1), static_cast<int>(static_cast<std::int32_t>(int_arg(c, 2)))))
      found.push_back(integer(a));
    return Value::list(std::move(found));
  }
  if (c.name == "damage_apply") {
    // (victim, attacker, attack, -, -, flags, descriptor, no_crit, minimum, maximum)
    if (c.args.size() != 10) throw C::Unsupported("damage_apply call-out shape");
    if (int_arg(c, 3) || int_arg(c, 4) || int_arg(c, 7))
      throw C::Unsupported("damage_apply arguments the combat glue does not pass");
    const auto victim = C::Address(int_arg(c, 0)), attacker = C::Address(int_arg(c, 1));
    if (!world.contains(victim) || (attacker && !world.contains(attacker)))
      throw C::Unsupported("effect damage to an object the combat World does not own");
    ++counts_.game_damage;
    const int result = world.damage_descriptor(victim, attacker, Raw(int_arg(c, 2)), Raw(int_arg(c, 5)),
                                               descriptor(arg(c, 6)), Raw(int_arg(c, 8)), Raw(int_arg(c, 9)));
    return integer(std::uint64_t(std::uint32_t(result)));
  }
  return std::nullopt; // EffectSimulation refuses any other gameplay call-out
}

void Effects::tick(Session &s) {
  ++counts_.ticks;
  for (const auto &e : s.tick_events()) {
    const std::string &n = e.name;
    if (n == "presentation:particle_spawn" && e.args.size() == 7) {
      ++counts_.particle_spawns;
      with_shared([&] { sim_.answer({"particle_spawn", {integer(e.args[0]), vec(e.args, 1), vec(e.args, 4), integer(0)}}); });
    } else if (n == "presentation:fx_spawn" && e.args.size() == 4) {
      ++counts_.fx_spawns;
      with_shared([&] { sim_.answer({"fx_spawn", {integer(e.args[0]), vec(e.args, 1)}}); });
    } else if (n == "decal_add" && e.args.size() == 7) {
      // (pos, angle, type, grow, size) as Creature_deadUpdate records it.
      ++counts_.decals;
      with_shared([&] {
        sim_.answer({"decal_add", {vec(e.args, 0), integer(e.args[3]), integer(e.args[4]), integer(e.args[5]),
                                   integer(e.args[6])}});
      });
    } else if (n.rfind("presentation:", 0) == 0) {
      ++counts_.unrouted[n.substr(13) + "/" + std::to_string(e.args.size())];
    } else if (n == "light") {
      ++counts_.unrouted["LightEffect light part (no colour arguments in the combat trace)"];
    }
  }
  for (const auto &run : runs_) {
    ++counts_.class_runs;
    with_shared([&] {
      sim_.classes().invoke(run.first, {Value::integer(std::uint64_t(run.second))});
      sim_.sync_lights();
    });
  }
  // updateParticles (priority 15) after runObjects (priority 10), per machine.
  if (auto *w = sim_.world()) w->tick(std::int32_t(s.world().read(C::FRAME)));
}

void Effects::draw_slots() {
  if (!sim_.world()) return;
  auto &world = session_.world();
  auto &model = sim_.classes();
  model.mem.poke(G_SIM_FRAME, 4, world.read(C::FRAME));
  for (auto a : world.actors()) {
    if (world.read(a + 0x48) != C::VT_HOLYSTRIKE) continue;
    for (Raw off : {0x10u, 0x14u, 0x18u, 0x6cu, 0x70u})
      model.mem.poke(std::int64_t(a + off), 4, world.read(a + off));
    model.holystrike_draw(Raw(a));
    ++counts_.glows;
  }
}

void Effects::run_each_tick(const std::string &method, std::int64_t obj) {
  runs_.emplace_back(method, obj);
}

} // namespace champions::simulation::rta::combat_play
