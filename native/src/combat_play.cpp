#include "champions/combat_play.hpp"
#include "champions/objects.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace champions::simulation::rta::combat_play {
namespace C = combat_runtime;
struct Session::State {
  std::shared_ptr<WorldAuthority> authority = std::make_shared<WorldAuthority>();
  world_queries::Level terrain;
  std::map<Raw, std::pair<Raw, Raw>> animations;
  std::unique_ptr<C::World> world;
  std::unique_ptr<C::SessionAdapter> adapter;
  netwire::Machine network{1, 0};
  objects::TaskList tasks;
  // Slot 3 is the optional one-per-session creature. Slots 4, 5 and 6 are
  // the Full fixture's AntQueen, Skeleton and Soul.
  // Slots 7 and 8 are an optional mounted rider and Nightmare pair.
  std::array<Raw, 9> handles{};
  std::string own_class;
  std::array<Raw, 3> pending{};
  session_control::Observation status;
  std::vector<VisualEvent> visual_events;
  std::unique_ptr<play_animation::Actors> poses;
  Fixture fixture = Fixture::Combat;
  // Full fixture: the research composed runtime's PlayerAnim test fixture
  // (four base cells; creature swing clips 30 ticks, hit mask 0x1 on tick
  // 10; every other clip 20 ticks). Authored, labelled synthetic timing.
  // One addition: a started player track is at full weight at once (cell +8 =
  // 1.0, no blend; research-anim player-animation-dispatch.md 1). PlayerAnim
  // never writes weights, so Player_run state 8 could never release a block
  // (it waits for top weight >= 1.0). Only block release and the state-5 cast
  // phases read that word; this fixture casts through World::queue_cast.
  struct Clip { unsigned length = 20; std::map<unsigned, Raw> events; };
  std::map<Raw, Clip> clips;
  std::map<C::Address, std::pair<Raw, unsigned>> clip_state;
  std::set<C::Address> players;
  C::SessionAdapter::PadInput pad{};
  bool attack_pressed = false, attack_held = false, block_held = false, block_was = false;
  bool ability_pressed = false, ability_held = false;
  bool disease_selected = false;
  bool cast_pending = false;
  TickObserver observer;
  void full_start(C::Address actor, Raw clip, Raw flags) {
    auto anim = actor + 0x114;
    clip_state[anim] = {clip, 0};
    if (players.count(actor)) {
      world->write(anim + 4, 1, 5);
      world->write(anim + 8 + 0x80, 4, clip);
      world->write(anim + 8 + 0x80 + 8, 4, ONE);
      world->write(anim + 8 + 0x80 + 0x1c, 2, flags & 0xffff);
    } else {
      world->write(anim + 4, 1, 1);
    }
    world->write(anim + 6, 2, 8);
  }
  Raw full_advance(C::Address actor) {
    auto anim = actor + 0x114;
    auto i = clip_state.find(anim);
    if (i == clip_state.end()) return 0;
    auto c = clips.find(i->second.first);
    Clip clip = c == clips.end() ? Clip{} : c->second;
    auto tick = ++i->second.second;
    auto e = clip.events.find(tick);
    Raw bits = e == clip.events.end() ? 0 : e->second;
    if (tick >= clip.length) {
      auto ended = (Raw(world->read(anim + 6, 2)) & ~8u & 0xffffu) | 0x80u;
      if (players.count(actor)) {
        clip_state.erase(i);
        world->write(anim + 4, 1, 4);
        world->write(anim + 8 + 0x80, 4, 0);
        world->write(anim + 8 + 0x80 + 8, 4, 0);
        world->write(anim + 8 + 0x80 + 0x1c, 2, 0);
      }
      world->write(anim + 6, 2, ended);
    }
    return bits;
  }
  C::Services full_services() {
    // Explicitly synthetic services of the composed research runtime's host
    // fixture: yaw 0, follow camera, no root motion, attach point at the
    // object, no sample points, resource 0, no loot class. Never production.
    auto out = C::flat_fixture_services();
    out.dialog_is_open = [this] { return status.paused; };
    out.height_below = [this](const Vec3 &p, Raw r) {
      return world_queries::terrain_height_below(terrain, r, p);
    };
    // world_findFreeSpot's two terrain tests on the same ground.
    out.terrain_blocked = [this](const Vec3 &p, Raw r) {
      return world_queries::terrain_test_flag1(terrain, r, p);
    };
    out.terrain_height = [this](const Vec3 &p, Raw r) {
      return world_queries::terrain_height(terrain, r, p);
    };
    out.animation_start = [this](Raw a, Raw clip, Raw flags, Raw, Raw) { full_start(a, clip, flags); };
    out.animation_advance = [this](Raw a, Raw) { return full_advance(a); };
    out.in_knockdown_anim = [](C::Address) { return false; }; // no knockdown clip in this fixture
    out.is_class = [](C::Address, const std::string &) { return false; };
    out.loot_generate = [](const C::LootDrop &) {};
    out.spawn_named_item = [](const Vec3 &, Raw) {};
    return out;
  }
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
  explicit State(const world_queries::Snapshot &source, Fixture which)
      : terrain(source.data, source.limits), fixture(which) {
    const bool animated = which == Fixture::Animated;
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
    if (which == Fixture::Full) { full_setup(); return; }
    world = std::make_unique<C::World>(authority, services(), 0);
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
        collect_visual_events();
        if (observer) observer(*owner);
        net.rand_seed = world->read(C::SHARED_SEED);
        net.positions[0] = position(world->target(handles[0]));
      });
    };
    status.ready_tick_limit = animated ? 90 : 4096; // Explicit fixture budget; no save/replay.
  }
  void full_setup() {
    world = std::make_unique<C::World>(authority, full_services(), 0, C::Composition::Full);
    // Authored set-up from the composed research runtime's walk-and-combo
    // scenario (player strength 40, weapon 30..40 / attack 50 / type 8, a
    // 120-HP Skelight at 80 facing the player), plus a second 120-HP
    // Skelight behind the player for blocking and Holy Strike.
    C::Spawn spawn;
    spawn.team = 0;
    auto player = world->spawn_player(0, spawn, 40);
    players.insert(player);
    handles[0] = world->retain(player);
    C::Weapon weapon;
    weapon.minimum = 30; weapon.maximum = 40; weapon.attack = 50; weapon.damage_type = 8;
    world->give_weapon(0, weapon);
    C::SkillSetup holy;
    holy.stats[0] = {1, 30, 0x3f800000};
    holy.stats[1] = {1, 30, 0};
    holy.stats[2] = {1, 10, 0};
    world->give_skill(HOLY_STRIKE, holy);
    world->arm_fixture_holy_shield(player);
    for (unsigned k = 1; k < 3; ++k) {
      C::Spawn s;
      s.position[0] = from_int(k == 1 ? 80 : -40);
      s.facing = k == 1 ? 0x8000 : 0;
      s.hp = 0x42f00000; // 120.0
      if (world_queries::terrain_height_below(terrain, from_int(4), s.position) != 0)
        throw C::Unsupported("combat fixture actor has no flat ground");
      auto a = world->spawn_skelight(s);
      handles[k] = world->retain(a);
      auto definition = Raw(world->read(a + 0x24));
      for (Raw off : {0x1cu, 0x24u})
        clips[Raw(world->read(definition + off))] = Clip{30, {{10u, Raw(1)}}};
    }
    // Slot 4: an AntQueen at (-110, 110), facing the player, 120 HP, built by
    // its executed constructor (World::spawn_class). AntQueen_run runs in
    // every state (research-combat-ctors-2 5b92837, antqueen-run.md); the
    // research probe ran her 3,000 idle ticks at (0, 140) without reaching an
    // unexecuted body (full-placement.md 3.1). The placement is authored: the
    // round-5 (0, 100) puts her in the walk-and-combo lane, where the player's
    // swings hit her and her blood stops the session at GutChunk_ctor (its
    // PhProp_ctor is not executed). In this corner of the flat ground (+-128)
    // she still sees the player at ready tick 33 and runs states 3, 8, 9, 0x17
    // and 0x18, and the authored combat scenarios end before she arrives.
    // Slot 6: a Soul on the placement the round-5 fixture used (-60, 100),
    // facing the player, 120 HP. Creature_setCharm puts it on team 0, and it
    // wanders in states 0 and 1 for the whole fight.
    // Slot 5: a Skeleton at (70, 100). FIXTURE DECISION, not a research
    // value, like the AntQueen's (-110, 110). The research's round-5 probe
    // placement is (70, 140) (research-combat-ctors-4 6baf4cd, full-placement.md
    // 6.4); y = 140 is off this synthetic level's flat ground (+-128), which
    // the placement check below requires. (70, 100) keeps that x, puts the
    // Skeleton on the flat ground, and leaves it on the far side of the
    // player from the AntQueen and the Soul. In this fixture it first enters
    // state 3 at frame 282, after the authored 300-tick combat scenarios have
    // done their work; the research's own timing (frame 123 on (70, 140)) is a
    // different placement. The constructor draws and equipped holder draws of
    // this actor shift the shared seed, so it changes FULL_TRACE and the
    // other actors' seed-driven rolls; that is a consequence of choosing to
    // place it, not a traced event. The run and ally-in-line-of-fire bodies
    // are from research-combat-ctors-3 9ff6ae0.
    auto place = [&](unsigned slot, const char *cls, int x, int y) {
      C::Spawn s;
      s.position[0] = from_int(Raw(x));
      s.position[1] = from_int(Raw(y));
      s.facing = 0xc000;
      s.hp = 0x42f00000;
      if (world_queries::terrain_height_below(terrain, from_int(4), s.position) != 0)
        throw C::Unsupported("combat fixture actor has no flat ground");
      handles[slot] = world->retain(world->spawn_class(cls, s));
    };
    place(4, "AntQueen", -110, 110);
    place(5, "Skeleton", 70, 100);
    place(6, "Soul", -60, 100);
    adapter = std::make_unique<C::SessionAdapter>(*world);
    adapter->bind_button(0, C::World::Button::Attack, ATTACK_MASK);
    adapter->bind_button(0, C::World::Button::Block, BLOCK_MASK);
    adapter->bind_button(0, C::World::Button::AbilityA, ABILITY_A_MASK);
    tasks.add(1, 10, "combatRunObjects", 0);
    network.num_players = 1;
    network.run_tasks = [this](netwire::Machine &net) {
      tasks.call(1, -1, [this, &net](Raw) {
        world->clear_history();
        visual_events.clear();
        auto in = pad;
        in.held = (attack_held ? ATTACK_MASK : 0) | (block_held ? BLOCK_MASK : 0) |
                  (ability_held ? ABILITY_A_MASK : 0);
        in.pressed = (attack_pressed ? ATTACK_MASK : 0) |
                     (block_held && !block_was ? BLOCK_MASK : 0) |
                     (ability_pressed ? ABILITY_A_MASK : 0);
        adapter->pad_input(0, in);
        attack_pressed = false;
        ability_pressed = false;
        block_was = block_held;
        if (cast_pending) adapter->cast(handles[0], HOLY_STRIKE, 1);
        cast_pending = false;
        adapter->ready_tick(Raw(net.net_frame - 1));
        net.sim_frame = world->read(C::FRAME);
        ++status.ready_ticks;
        collect_visual_events();
        if (observer) observer(*owner);
        net.rand_seed = world->read(C::SHARED_SEED);
        net.positions[0] = position(world->target(handles[0]));
      });
    };
    status.ready_tick_limit = 4096; // Explicit fixture budget; no save/replay.
  }
  Session *owner = nullptr;
  void collect_visual_events() {
    for (const auto &event : world->events())
      if (event.name == "damage_apply") ++status.damage_events;
    for (const auto &event : world->events()) {
      bool death = event.name == "obj_delete";
      bool hit = event.name == "damage_result" && event.args.size() == 2 && event.args[1] != 0;
      if ((!death && !hit) || event.args.empty()) continue;
      for (unsigned slot=0;slot<handles.size();++slot) {
        if (!handles[slot]) continue;
        auto address=world->target(handles[slot]);
        if(address && address==event.args[0]) visual_events.push_back({death,slot,position(address)});
      }
    }
  }
  Vec3 position(Raw a) const {
    return {Raw(world->read(a + 16)), Raw(world->read(a + 20)), Raw(world->read(a + 24))};
  }
};
Session::Session(const world_queries::Snapshot &s, bool animated)
    : Session(s, animated ? Fixture::Animated : Fixture::Combat) {}
Session::Session(const world_queries::Snapshot &s, Fixture which) : s_(std::make_unique<State>(s, which)) {
  s_->owner = this;
}
bool Session::animated() const { return bool(s_->poses); }
Fixture Session::fixture() const { return s_->fixture; }
bool Session::full() const { return s_->fixture == Fixture::Full; }
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
  auto &s = *s_;
  if (s.fixture != Fixture::Full && command)
    throw C::Unsupported("combat fixture refuses movement and diagnostic damage commands");
  if (command & ~(session_control::POS_X | session_control::NEG_X | session_control::POS_Y | session_control::NEG_Y))
    throw C::Unsupported("Full combat fixture refuses diagnostic damage commands");
  if (s.fixture == Fixture::Full) {
    // Client stick mapping: one axis is +-1.0; a diagonal is +-0.70710677 per
    // axis so the stick stays on the unit circle. Authored, not original.
    auto axis = [&](std::uint32_t pos, std::uint32_t neg) -> int {
      return ((command & pos) ? 1 : 0) - ((command & neg) ? 1 : 0);
    };
    const int x = axis(session_control::POS_X, session_control::NEG_X);
    const int y = axis(session_control::POS_Y, session_control::NEG_Y);
    const Raw unit = (x && y) ? Raw(0x3f3504f3) : ONE;
    auto value = [&](int v) { return v > 0 ? unit : v < 0 ? (unit | 0x80000000u) : Raw(0); };
    s.pad.stick_x = value(x);
    s.pad.stick_y = value(y);
  }
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
    if (k >= 3 && !s_->handles[k]) continue; // empty optional or non-Full slot
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
  if (s_->fixture == Fixture::Full) throw C::Unsupported("Full fixture: hits come from the real player tick, not an event button");
  if (s_->poses) throw C::Unsupported("animated fixture refuses player hit timing; action release is uncontracted");
  if (slot >= 3 || (mask & ~Raw(1))) throw C::Unsupported("combat fixture accepts only animation mask 0x1");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  auto a = s_->world->target(s_->handles[slot]);
  if (!a) throw C::Error("hit event handle expired");
  if (slot != 0) throw C::Unsupported("creature events already supplied by animation fixture");
  if (s_->world->read(a + 0x58c) != 1) throw C::Unsupported("player hit requires action state 1; action switch is unavailable");
  s_->pending[slot] |= mask;
}
void Session::release_input() {
  s_->pending.fill(0);
  s_->pad = {};
  s_->attack_pressed = s_->attack_held = s_->block_held = false;
  s_->ability_pressed = s_->ability_held = false;
  s_->cast_pending = false;
}
void Session::set_paused(bool value) { release_input(); s_->status.paused = value; }
void Session::stop() { release_input(); s_->status.stopped = true; }
Raw Session::shared_seed() const { return Raw(s_->world->read(C::SHARED_SEED)); }
}

namespace champions::simulation::rta::combat_play {
void Session::press_attack(bool held) {
  if (!full()) throw C::Unsupported("attack input needs the Full combat fixture");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  s_->attack_pressed = true;
  s_->attack_held = held;
}
void Session::release_attack() { s_->attack_held = false; }
void Session::set_block(bool held) {
  if (!full()) throw C::Unsupported("block input needs the Full combat fixture");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  s_->block_held = held;
}
void Session::cast_holy_strike() {
  if (!full()) throw C::Unsupported("Holy Strike needs the Full combat fixture");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  if (!s_->world->target(s_->handles[0])) throw C::Error("player handle expired");
  s_->cast_pending = true;
}
void Session::select_disease_bolt(unsigned rank) {
  if (!full()) throw C::Unsupported("Disease Bolt selection needs the Full combat fixture");
  if (s_->status.stopped || s_->disease_selected)
    throw C::Unsupported("Disease Bolt selection is unavailable or already made");
  if (!s_->world->target(s_->handles[0])) throw C::Error("player handle expired");
  // Authored synthetic fixture resource, like the Full fixture's HP. This is
  // not a new-character current mana claim and is never used on a campaign.
  s_->world->select_disease_bolt(0, from_int(100), from_int(100), rank);
  s_->disease_selected = true;
}
void Session::press_disease_bolt(bool held) {
  if (!full() || !s_->disease_selected)
    throw C::Unsupported("Disease Bolt input needs a selected Full-fixture spell");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  s_->ability_pressed = true;
  s_->ability_held = held;
}
void Session::release_disease_bolt() {
  s_->ability_held = false;
}
void Session::spawn_creature(const std::string &own_class) {
  if (!full()) throw C::Unsupported("own-machine creatures need the Full combat fixture");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  if (s_->handles[3]) throw C::Unsupported("one own-machine creature per session");
  C::Spawn spawn;
  spawn.position[1] = from_int(80);
  spawn.facing = 0xc000; // toward the player at the origin
  spawn.hp = 0x42f00000;  // 120.0, the fixture's Skelight value
  if (world_queries::terrain_height_below(s_->terrain, from_int(4), spawn.position) != 0)
    throw C::Unsupported("combat fixture actor has no flat ground");
  auto a = s_->world->spawn_class(own_class, spawn);
  s_->handles[3] = s_->world->retain(a);
  s_->own_class = own_class;
  auto definition = Raw(s_->world->read(a + 0x24));
  // The session's authored clip timeline on the two swing words, and for
  // Gnaw on its third bite clip too (Gnaw_ctor marks entries 3, 4 and 5).
  std::vector<Raw> offsets{0x1c, 0x24};
  if (own_class == "Gnaw") offsets.push_back(0x2c);
  if (definition)
    for (Raw off : offsets) {
      const auto clip = Raw(s_->world->read(definition + off));
      if (clip && !s_->clips.count(clip)) s_->clips[clip] = State::Clip{30, {{10u, Raw(1)}}};
    }
}
std::array<C::Address, 2> Session::spawn_mounted_horseman(
    unsigned variant, const C::Spawn &rider, const C::Spawn &mount) {
  if (!full()) throw C::Unsupported("mounted Horsemen need the Full combat fixture");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  if (s_->handles[7] || s_->handles[8])
    throw C::Unsupported("one mounted pair per combat fixture");
  auto pair = s_->world->spawn_mounted_horseman(variant, rider, mount);
  s_->handles[7] = s_->world->retain(pair[0]);
  s_->handles[8] = s_->world->retain(pair[1]);
  // Synthetic event timeline for the four documented mounted swing entries.
  auto def = Raw(s_->world->read(pair[0] + 0x24));
  for (Raw off : {0x8cu, 0x94u, 0x9cu, 0xc4u}) {
    auto clip = Raw(s_->world->read(def + off));
    s_->clips[clip] = State::Clip{30, {{10u, Raw(1)}}};
  }
  return pair;
}
const std::string &Session::own_creature_class() const { return s_->own_class; }
void Session::drop_potion(unsigned kind) {
  if (!full()) throw C::Unsupported("potions need the Full combat fixture");
  if (s_->status.stopped) throw C::Error("combat fixture stopped");
  if (kind < 1 || kind > 3) throw C::Unsupported("potion kinds 1..3 only");
  auto player = s_->world->target(s_->handles[0]);
  if (!player) throw C::Error("player handle expired");
  C::Thrown thrown;
  thrown.kind = kind;
  thrown.position = s_->position(player);
  thrown.facing = Raw(s_->world->read(player + 0x38, 2));
  s_->world->spawn_thrown(player, thrown);
}
const C::World &Session::world() const { return *s_->world; }
C::World &Session::world() { return *s_->world; }
const std::vector<C::Event> &Session::tick_events() const { return s_->world->events(); }
void Session::set_tick_observer(TickObserver f) { s_->observer = std::move(f); }
}
