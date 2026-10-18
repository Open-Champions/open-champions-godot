#include "champions/combat_play.hpp"
#include "champions/objects.hpp"
#include <algorithm>
#include <map>
namespace champions::simulation::rta::combat_play {
namespace C = combat_runtime;
struct Session::State {
  world_queries::Level terrain;
  std::map<Raw, std::pair<Raw, Raw>> animations;
  std::unique_ptr<C::World> world;
  std::unique_ptr<C::SessionAdapter> adapter;
  netwire::Machine network{1, 0};
  objects::TaskList tasks;
  std::array<Raw, 3> handles{};
  std::array<Raw, 3> pending{};
  session_control::Observation status;
  std::vector<VisualEvent> visual_events;
  std::unique_ptr<play_animation::Actors> poses;
  C::Services services() {
    // This policy deliberately selects the documented empty-resource fixture.
    // It is never a fallback for a production level/animation/skill service.
    auto out = C::flat_fixture_services();
    out.dialog_is_open = [this] { return status.paused; };
    out.height_below = [this](const Vec3 &p, Raw r) {
      return world_queries::terrain_height_below(terrain, r, p);
    };
    out.slide = [this](const Vec3 &p, const Vec3 &d, Raw r, Raw mask) {
      auto v = world_queries::world_slide_move(terrain, r, p, d, mask);
      return C::Slide{v.position, Raw(v.hit), v.normal.value_or(Vec3{})};
    };
    out.animation_start = [this](Raw a, Raw clip, Raw, Raw, Raw) {
      if (poses) throw C::Unsupported("animated fixture refuses uncontracted action transitions");
      animations[a] = {clip, 0};
      world->write(a + 0x118, 1, 1);
      world->write(a + 0x11a, 2, 8);
    };
    // Same explicitly synthetic 10/20/30-tick fixture as combat runtime tests.
    out.animation_advance = [this](Raw a, Raw) {
      if (poses) {
        unsigned slot=1;
        while (slot<3 && world->target(handles[slot])!=a) ++slot;
        if(slot==3) throw C::Unsupported("unknown animated actor");
        poses->advance(slot, unsigned(status.ready_ticks+1));
        const auto &frame=poses->frame(slot);
        world->write(a+0x118,1,1);
        world->write(a+0x11a,2,frame.flags);
        for(const auto &event:frame.events) {
          if(event.kind=="hit_test") {
            if(event.mask!=1) throw C::Unsupported("combat adapter supports only hit mask 0x1");
            // Queue synchronously inside step_anim; World consumes this queue
            // immediately after this callback, in the SAME ready tick, once.
            adapter->hit_test_event(handles[slot],1);
          }
        }
        return Raw(0);
      }
      auto it = animations.find(a);
      if (it == animations.end()) return Raw(0);
      auto &v = it->second;
      bool swing = v.first == 0xc1100018 || v.first == 0xc1100020;
      auto tick = ++v.second;
      if (tick >= (swing ? 30u : 20u))
        world->write(a + 0x11a, 2, (world->read(a + 0x11a, 2) & ~8u) | 0x80);
      return Raw(swing && tick == 10 ? 1 : 0);
    };
    return out;
  }
  explicit State(const world_queries::Snapshot &source, bool animated)
      : terrain(source.data, source.limits) {
    ScopedFpu fixed{Fpu{Fpu::Model::Ps2, false, false}};
    // Empty sight/water fixtures are valid only under this explicitly flat,
    // dry synthetic policy. No arbitrary selected WORLD is silently accepted.
    if (source.data.heightfields.empty() || source.data.elements.empty() ||
        !source.data.pieces.empty()) throw C::Unsupported("combat fixture requires flat dry ground");
    for (const auto &h : source.data.heightfields)
      if (std::any_of(h.samples.begin(), h.samples.end(), [](auto v) { return v != 0; }))
        throw C::Unsupported("combat fixture requires zero-height ground");
    for (const auto &e : source.data.elements)
      if (e.flags || e.z || e.cos != 32767 || e.sin)
        throw C::Unsupported("combat fixture requires unmodified dry ground");
    world = std::make_unique<C::World>(services(), 0);
    C::Spawn spawn;
    spawn.team = 0;
    auto player = world->spawn_player(0, spawn);
    handles[0] = world->retain(player);
    world->write(player + 0x58c, 4, 1); // Authored action-state-one fixture.
    spawn.team = -1;
    spawn.hp = ONE;
    spawn.attack = 100000;
    for (unsigned k = 1; k < 3; ++k) {
      spawn.position[0] = from_int(k == 1 ? 30 : -30);
      spawn.facing = k == 1 ? 0x8000 : 0;
      if (world_queries::terrain_height_below(terrain, from_int(4), spawn.position) != 0)
        throw C::Unsupported("combat fixture actor has no flat ground");
      handles[k] = world->retain(world->spawn_skelight(spawn));
    }
    world->relink(player + C::TARGET, world->target(handles[1]));
    if(animated) {
      poses=std::make_unique<play_animation::Actors>();
      for(unsigned k=1;k<3;++k) {
        auto a=world->target(handles[k]);
        // Explicit synthetic carrier: Skelight's contracted shared hit-arc
        // body, already in attack state six. NOT original Agony AI/resources.
        world->write(a+0x232,2,6);
        world->write(a+0xf4,4,1000);
        world->write(a+0x118,1,1);
        world->write(a+0x11a,2,poses->frame(k).flags);
        world->relink(a+C::TARGET,player);
      }
    }
    adapter = std::make_unique<C::SessionAdapter>(*world);
    tasks.add(1, 10, "combatRunObjects", 0);
    network.num_players = 1;
    network.run_tasks = [this](netwire::Machine &net) {
      tasks.call(1, -1, [this, &net](Raw) {
        world->clear_history();
        visual_events.clear();
        if(poses) poses->advance(0, unsigned(status.ready_ticks+1));
        for (unsigned k = 0; k < pending.size(); ++k) {
          if (pending[k]) adapter->hit_test_event(handles[k], pending[k]);
          pending[k] = 0;
        }
        // Wire input starts at one; the combat World starts at zero.
        adapter->ready_tick(Raw(net.net_frame - 1));
        net.sim_frame = world->read(C::FRAME);
        ++status.ready_ticks;
        for (const auto &event : world->events())
          if (event.name == "damage_apply") ++status.damage_events;
        for (const auto &event : world->events()) {
          bool death = event.name == "obj_delete";
          bool hit = event.name == "damage_result" && event.args.size() == 2 && event.args[1] != 0;
          if ((!death && !hit) || event.args.empty()) continue;
          for (unsigned slot=0;slot<handles.size();++slot) {
            auto address=world->target(handles[slot]);
            if(address && address==event.args[0]) visual_events.push_back({death,slot,position(address)});
          }
        }
        net.rand_seed = world->read(C::SHARED_SEED);
        net.positions[0] = position(world->target(handles[0]));
      });
    };
    status.ready_tick_limit = animated ? 90 : 4096; // Explicit fixture budget; no save/replay.
  }
  Vec3 position(Raw a) const {
    return {Raw(world->read(a + 16)), Raw(world->read(a + 20)), Raw(world->read(a + 24))};
  }
};
Session::Session(const world_queries::Snapshot &s, bool animated) : s_(std::make_unique<State>(s, animated)) {}
bool Session::animated() const { return bool(s_->poses); }
const play_animation::Frame &Session::animation_frame(unsigned slot) const {
  if(!s_->poses) throw C::Unsupported("animated fixture is disabled");
  return s_->poses->frame(slot);
}
Session::~Session() = default;
session_control::Observation Session::observation() const {
  auto o = s_->status;
  auto p = s_->world->target(s_->handles[0]);
  o.position = s_->position(p);
  o.hp = Raw(s_->world->read(C::PLAYERS + 0xac));
  o.max_health = Raw(s_->world->read(C::PLAYERS + 0xb0));
  o.radius = Raw(s_->world->read(p + 0x1c));
  o.facing = Raw(s_->world->read(p + 0x38, 2));
  o.frame = s_->world->read(C::FRAME);
  return o;
}
session_control::Observation Session::advance(std::uint32_t command) {
  if (command) throw C::Unsupported("combat fixture refuses movement and diagnostic damage commands");
  auto &s = *s_;
  if (s.status.paused || s.status.stopped) return observation();
  ScopedFpu fixed{Fpu{Fpu::Model::Ps2, false, false}};
  try {
    ++s.status.submissions;
    s.network.net_main({0, 0, 128, 128, 128, 128, 0, 0});
    s.network.log.clear();
    s.network.outbox.clear();
    if (s.status.ready_ticks >= s.status.ready_tick_limit) {
      s.status.stopped = true;
      s.status.error = "combat fixture tick budget";
    }
  } catch (const std::exception &e) {
    s.status.failed = s.status.stopped = true;
    s.status.error = e.what();
  }
  return observation();
}
std::vector<Actor> Session::actors() const {
  std::vector<Actor> out;
  for (unsigned k = 0; k < s_->handles.size(); ++k) {
    auto a = s_->world->target(s_->handles[k]);
    out.push_back({k, s_->handles[k], a != 0,
      a && bool(s_->world->read(a + 12) & 8), a ? s_->position(a) : Vec3{},
      a ? Raw(s_->world->read(k ? a + 0x228 : C::PLAYERS + 0xac)) : 0,
      a ? Raw(s_->world->read(a + (k ? 0x232 : 0x58c), k ? 2 : 4)) : 0});
  }
  return out;
}
const std::vector<VisualEvent> &Session::visual_events() const { return s_->visual_events; }
void Session::hit_event(unsigned slot, Raw mask) {
  if (s_->poses) throw C::Unsupported("animated fixture refuses player hit timing; action release is uncontracted");
  if (slot >= 3 || (mask & ~Raw(1))) throw C::Unsupported("combat fixture accepts only animation mask 0x1");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  auto a = s_->world->target(s_->handles[slot]);
  if (!a) throw C::Error("hit event handle expired");
  if (slot != 0) throw C::Unsupported("creature events already supplied by animation fixture");
  if (s_->world->read(a + 0x58c) != 1) throw C::Unsupported("player hit requires action state 1; action switch is unavailable");
  s_->pending[slot] |= mask;
}
void Session::release_input() { s_->pending.fill(0); }
void Session::set_paused(bool value) { release_input(); s_->status.paused = value; }
void Session::stop() { release_input(); s_->status.stopped = true; }
Raw Session::shared_seed() const { return Raw(s_->world->read(C::SHARED_SEED)); }
}
