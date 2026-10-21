#include "champions/netplay.hpp"

#include "champions/core_arithmetic.hpp"
#include "champions/level_flow.hpp"
#include "champions/session_combat_runtime_adapter.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace champions::simulation::rta::netplay {
namespace {
namespace C = combat_runtime;
namespace W = world_queries;
using Vec3 = W::Vec3;
using Integer = netwire::Integer;

constexpr std::size_t MAX_STEPS = 1u << 20;
// Radii World::spawn writes at object +0x1c.
constexpr Raw PLAYER_RADIUS = 0x41800000u, CREATURE_RADIUS = 0x41400000u;
// g_killPlaneZ. Player_tick kills a player below it when it is above -90000.
constexpr C::Address KILL_PLANE = 0x4f7188;
constexpr char MAGIC[8] = {'O', 'C', 'N', 'P', 'L', 'A', 'Y', '1'};
constexpr std::uint32_t SAVE_VERSION = 2;
// One journal sample: buttons u16, four sticks, two pressure bytes, yaw u16,
// camera flag u8.
constexpr std::size_t SAMPLE_BYTES = 11;

Raw pad_word(const netwire::Pad &pad, unsigned offset) {
  return Raw(pad[offset]) | (Raw(pad[offset + 1]) << 8) |
         (Raw(pad[offset + 2]) << 16) | (Raw(pad[offset + 3]) << 24);
}

netwire::ByteVector sample_bytes(const Sample &sample) {
  netwire::ByteVector out(8);
  out[0] = std::uint8_t(sample.buttons);
  out[1] = std::uint8_t(sample.buttons >> 8);
  out[2] = sample.sticks[0];
  out[3] = sample.sticks[1];
  out[4] = sample.sticks[2];
  out[5] = sample.sticks[3];
  out[6] = sample.pressure[0];
  out[7] = sample.pressure[1];
  return out;
}

std::uint32_t packet_word(const netwire::ByteVector &data) {
  return std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8) |
         (std::uint32_t(data[2]) << 16) | (std::uint32_t(data[3]) << 24);
}

bool has_floor(W::Level &level, Raw radius, const std::array<Raw, 3> &at) {
  return W::terrain_height_below(level, radius, at) != W::NO_FLOOR;
}

struct Clip {
  unsigned length = 20;
  std::map<unsigned, Raw> events;
};

// One machine and the Full combat world it ticks. Addresses stay put: the
// session holds these in unique_ptr, and the service callbacks capture this.
struct Host {
  netwire::Machine machine;
  W::Level level;
  std::unique_ptr<C::World> world;
  std::unique_ptr<C::SessionAdapter> adapter;
  std::array<C::Address, 4> players{};
  C::Address creature = 0;
  std::set<C::Address> player_actors;
  std::map<Raw, Clip> clips;
  std::map<C::Address, std::pair<Raw, unsigned>> clip_state;
  std::vector<TracePoint> trace;
  std::uint64_t free_spot_searches = 0;
  int peer_count = 0;

  Host(int peers, int slot, const W::Snapshot &snapshot)
      : machine(peers, slot), level(snapshot.data, snapshot.limits),
        peer_count(peers) {}

  void full_start(C::Address actor, Raw clip, Raw flags) {
    auto anim = actor + 0x114;
    clip_state[anim] = {clip, 0};
    if (player_actors.count(actor)) {
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
    auto found = clip_state.find(anim);
    if (found == clip_state.end())
      return 0;
    auto known = clips.find(found->second.first);
    Clip clip = known == clips.end() ? Clip{} : known->second;
    auto tick = ++found->second.second;
    auto event = clip.events.find(tick);
    Raw bits = event == clip.events.end() ? 0 : event->second;
    if (tick >= clip.length) {
      auto ended =
          (Raw(world->read(anim + 6, 2)) & ~8u & 0xffffu) | 0x80u;
      if (player_actors.count(actor)) {
        clip_state.erase(found);
        world->write(anim + 4, 1, 4);
        world->write(anim + 8 + 0x80, 4, 0);
        world->write(anim + 8 + 0x80 + 8, 4, 0);
        world->write(anim + 8 + 0x80 + 0x1c, 2, 0);
      }
      world->write(anim + 6, 2, ended);
    }
    return bits;
  }

  C::Services services() {
    // Flat fixture for the calls this session does not contract (water stays
    // dry: a wet result refuses; sight stays the clear ray because the
    // callback has no radius). Height and slide use this peer's level copy.
    auto out = C::flat_fixture_services();
    out.height_below = [this](const Vec3 &p, Raw radius) {
      return W::terrain_height_below(level, radius, p);
    };
    out.slide = [this](const Vec3 &p, const Vec3 &delta, Raw radius,
                       Raw mask) {
      auto moved = W::world_slide_move(level, radius, p, delta,
                                       std::uint32_t(mask));
      C::Slide slide;
      slide.position = moved.position;
      slide.hit = Raw(moved.hit);
      slide.normal = moved.normal.value_or(Vec3{});
      return slide;
    };
    // world_findFreeSpot's two terrain tests (terrain_testFlag1 0x183b48,
    // terrain_height 0x183820) on this peer's level copy. A walking player
    // pinning the Skelight reaches the search through Creature_moveTo's
    // stuck recovery (research t3/research-combat-walk, ref_combat_walk).
    out.terrain_blocked = [this](const Vec3 &p, Raw radius) {
      return W::terrain_test_flag1(level, radius, p);
    };
    out.terrain_height = [this](const Vec3 &p, Raw radius) {
      return W::terrain_height(level, radius, p);
    };
    out.camera_yaw = [this](Raw slot) {
      if (slot > 3)
        return Raw(0);
      return Raw(std::uint32_t(machine.cam_yaw[slot]));
    };
    out.camera_flag = [this](Raw slot) {
      if (slot > 3)
        return Raw(0);
      return Raw(std::uint32_t(machine.cam_yaw[slot]) & 1u);
    };
    // AUTHORED: everyone inside. The research Full fixture's screen planes,
    // verbatim (test_ref_combat_full.py PLANES: four planes through
    // (0, 0, -1000) with normal (0, 0, 1)). Not a camera frustum: the
    // original per-machine frustum is not contracted.
    out.screen_planes = [](const Vec3 &) {
      std::array<C::Plane, 4> planes{};
      for (auto &plane : planes) {
        plane.point = {0, 0, 0xc47a0000u};
        plane.normal = {0, 0, ONE};
      }
      return planes;
    };
    // Same synthetic clip timing as the Full combat fixture: creature swings
    // are 30 ticks with hit mask 0x1 on tick 10; other clips last 20.
    out.animation_start = [this](Raw actor, Raw clip, Raw flags, Raw, Raw) {
      full_start(actor, clip, flags);
    };
    out.animation_advance = [this](Raw actor, Raw) {
      return full_advance(actor);
    };
    out.is_class = [](C::Address, const std::string &) { return false; };
    out.loot_generate = [](const C::LootDrop &) {};
    out.spawn_named_item = [](const Vec3 &, Raw) {};
    return out;
  }

  void spawn(const Placement &placement) {
    // 0x402791 stays 0. World::tick's domain check refuses a nonzero
    // network-mode byte ("network, cutscene, PvP, damage numbers or
    // decals"), and a set byte also takes the unported GutChunk path. So
    // this is the offline co-op simulation (offline screen clamp against the
    // authored planes above, single-player speed base 0x3f5ca86d) carried by
    // the lockstep transport, not the original net-game simulation.
    world = std::make_unique<C::World>(services(), 0, C::Composition::Full);
    // game_loadLevel is the only writer of g_killPlaneZ, and every level-table
    // row, like the default row, holds -100000, so the plane never fires
    // (research level-flow.md section 2). The Full world starts it at 0,
    // which would kill a player standing on any floor below z = 0.
    world->write(KILL_PLANE, 4, level_flow::LevelRow{}.kill_plane);
    C::Weapon weapon;
    weapon.minimum = 30;
    weapon.maximum = 40;
    weapon.attack = 50;
    weapon.damage_type = 8;
    for (int slot = 0; slot < peer_count; ++slot) {
      if (!placement.player_present[std::size_t(slot)])
        throw Error("every peer slot needs a player");
      C::Spawn spawn;
      spawn.position = placement.players[std::size_t(slot)];
      spawn.team = 0;
      auto actor = world->spawn_player(static_cast<unsigned>(slot), spawn, 40);
      player_actors.insert(actor);
      players[std::size_t(slot)] = world->retain(actor);
      world->give_weapon(static_cast<unsigned>(slot), weapon);
    }
    if (placement.creature_present) {
      C::Spawn spawn;
      spawn.position = placement.creature;
      spawn.facing = 0x8000;
      spawn.hp = 0x42f00000;
      spawn.team = -1;
      auto actor = world->spawn_skelight(spawn);
      creature = world->retain(actor);
      auto definition = Raw(world->read(actor + 0x24));
      for (Raw offset : {Raw(0x1c), Raw(0x24)})
        clips[Raw(world->read(definition + offset))] =
            Clip{30, {{10u, Raw(1)}}};
    }
    adapter = std::make_unique<C::SessionAdapter>(*world);
    for (int slot = 0; slot < peer_count; ++slot)
      adapter->bind_button(static_cast<unsigned>(slot), C::World::Button::Attack,
                           ATTACK_BIT);
    machine.num_players = peer_count;
    machine.run_tasks = [this](netwire::Machine &net) { simulate(net); };
  }

  void simulate(netwire::Machine &net) {
    world->clear_history();
    for (int slot = 0; slot < peer_count; ++slot) {
      const auto &pad = net.pads[std::size_t(slot)];
      adapter->pad_input(static_cast<unsigned>(slot),
                         {pad_word(pad, 0x14), pad_word(pad, 0x18),
                          pad_word(pad, 8), pad_word(pad, 0x0c)});
    }
    adapter->ready_tick(Raw(std::uint32_t(net.net_frame - 1)));
    for (const auto &event : world->events())
      if (event.name == "findFreeSpot")
        ++free_spot_searches;
    net.sim_frame = Integer(world->read(C::FRAME));
    net.rand_seed = Integer(world->read(C::SHARED_SEED));
    TracePoint point;
    for (int slot = 0; slot < peer_count; ++slot) {
      auto actor = world->target(players[std::size_t(slot)]);
      if (!actor) {
        net.positions[std::size_t(slot)] = std::nullopt;
        continue;
      }
      net.positions[std::size_t(slot)] =
          netwire::Position{Raw(world->read(actor + 16)),
                            Raw(world->read(actor + 20)),
                            Raw(world->read(actor + 24))};
      for (unsigned k = 0; k < 3; ++k)
        point.player_position[std::size_t(slot)][k] =
            std::uint32_t(world->read(actor + 16 + 4 * k));
    }
    point.seed = std::uint32_t(world->read(C::SHARED_SEED));
    for (int slot = 0; slot < peer_count; ++slot) {
      point.player_hp[std::size_t(slot)] = std::uint32_t(
          world->read(C::PLAYERS + C::Address(slot) * C::RECORD_SIZE + 0xac));
    }
    if (creature) {
      auto actor = world->target(creature);
      if (actor)
        point.creature_hp = std::uint32_t(world->read(actor + 0x228));
    }
    trace.push_back(point);
  }
};

struct Identity {
  std::uint32_t heightfields = 0, elements = 0, samples = 0;
  std::int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  std::int32_t field_x = 0, field_y = 0, field_w = 0, field_h = 0;
};

Identity identity_of(const W::Snapshot &snapshot) {
  Identity id;
  id.heightfields = std::uint32_t(snapshot.data.heightfields.size());
  id.elements = std::uint32_t(snapshot.data.elements.size());
  for (const auto &field : snapshot.data.heightfields)
    id.samples += std::uint32_t(field.samples.size());
  if (!snapshot.data.elements.empty()) {
    const auto &element = snapshot.data.elements[0];
    id.min_x = std::int32_t(element.min_x);
    id.min_y = std::int32_t(element.min_y);
    id.max_x = std::int32_t(element.max_x);
    id.max_y = std::int32_t(element.max_y);
  }
  if (!snapshot.data.heightfields.empty()) {
    const auto &field = snapshot.data.heightfields[0];
    id.field_x = std::int32_t(field.x0);
    id.field_y = std::int32_t(field.y0);
    id.field_w = std::int32_t(field.w);
    id.field_h = std::int32_t(field.h);
  }
  return id;
}

void require_floor(const W::Snapshot &snapshot, const Placement &placement,
                   int peers) {
  W::Level level(snapshot.data, snapshot.limits);
  for (int slot = 0; slot < peers; ++slot) {
    if (!placement.player_present[std::size_t(slot)])
      throw Error("player slots occupy 0 .. peer count - 1");
    if (!has_floor(level, PLAYER_RADIUS, placement.players[std::size_t(slot)]))
      throw Error("player spawn has no floor");
  }
  for (int slot = peers; slot < 4; ++slot)
    if (placement.player_present[std::size_t(slot)])
      throw Error("player slot is past the peer count");
  if (placement.creature_present &&
      !has_floor(level, CREATURE_RADIUS, placement.creature))
    throw Error("creature spawn has no floor");
}

struct Writer {
  std::vector<std::uint8_t> bytes;
  void u8(std::uint8_t value) { bytes.push_back(value); }
  void u16(std::uint16_t value) {
    u8(std::uint8_t(value));
    u8(std::uint8_t(value >> 8));
  }
  void u32(std::uint32_t value) {
    u16(std::uint16_t(value));
    u16(std::uint16_t(value >> 16));
  }
  void i32(std::int32_t value) { u32(std::uint32_t(value)); }
};

void write_point(Writer &writer, const TracePoint &point) {
  writer.u32(point.seed);
  for (auto hp : point.player_hp)
    writer.u32(hp);
  writer.u32(point.creature_hp);
  for (const auto &position : point.player_position)
    for (auto word : position)
      writer.u32(word);
}

struct Reader {
  const std::uint8_t *data;
  std::size_t size, at = 0;
  explicit Reader(const std::vector<std::uint8_t> &bytes)
      : data(bytes.data()), size(bytes.size()) {}
  void need(std::size_t n) const {
    if (n > size - at)
      throw Error("truncated lockstep save");
  }
  std::uint8_t u8() {
    need(1);
    return data[at++];
  }
  std::uint16_t u16() {
    auto lo = this->u8();
    return std::uint16_t(lo | (std::uint16_t(this->u8()) << 8));
  }
  std::uint32_t u32() {
    auto lo = this->u16();
    return std::uint32_t(lo) | (std::uint32_t(this->u16()) << 16);
  }
  std::int32_t i32() { return std::int32_t(u32()); }
};

} // namespace

struct Session::State {
  int peers = 0;
  Placement placement;
  W::Snapshot terrain;
  LoopbackTransport transport;
  std::vector<std::unique_ptr<Host>> hosts;
  std::vector<Sample> journal;
  std::size_t render_steps = 0;
  bool failed = false;
  bool synced = true;
  std::uint64_t input_packets = 0;

  State(int count, W::Snapshot snapshot, Placement where)
      : peers(count), placement(std::move(where)), terrain(std::move(snapshot)),
        transport(count) {}
};

LoopbackTransport::LoopbackTransport(int peers)
    : peers_(peers), inbox_(std::size_t(peers)), pending_(std::size_t(peers)) {
  if (peers < 2 || peers > 4)
    throw Error("lockstep peers must be 2, 3 or 4");
}

void LoopbackTransport::send(int from, const netwire::Sent &packet) {
  if (from < 0 || from >= peers_ || packet.peer < 0 || packet.peer >= peers_)
    throw Error("loopback slot is outside the session");
  if (packet.data.size() > netwire::MAX_BYTES)
    throw Error("loopback packet exceeds the wire budget");
  pending_[std::size_t(packet.peer)].push_back(packet.data);
}

std::vector<netwire::ByteVector> LoopbackTransport::take(int slot) {
  if (slot < 0 || slot >= peers_)
    throw Error("loopback slot is outside the session");
  std::vector<netwire::ByteVector> out;
  out.swap(inbox_[std::size_t(slot)]);
  return out;
}

void LoopbackTransport::deliver() {
  for (int slot = 0; slot < peers_; ++slot) {
    auto &dest = inbox_[std::size_t(slot)];
    auto &src = pending_[std::size_t(slot)];
    dest.insert(dest.end(), src.begin(), src.end());
    src.clear();
  }
}

Session::Session(int peers, const W::Snapshot &terrain,
                 const Placement &placement)
    : state_(std::make_unique<State>(peers, terrain, placement)) {
  if (peers < 2 || peers > 4)
    throw Error("lockstep peers must be 2, 3 or 4");
  ScopedFpu fixed{Fpu{Fpu::Model::Ps2, false, false}};
  require_floor(terrain, placement, peers);
  state_->hosts.reserve(std::size_t(peers));
  for (int slot = 0; slot < peers; ++slot) {
    auto host = std::make_unique<Host>(peers, slot, terrain);
    host->spawn(placement);
    state_->hosts.push_back(std::move(host));
  }
}

Session::~Session() = default;
Session::Session(Session &&) noexcept = default;
Session &Session::operator=(Session &&) noexcept = default;

int Session::peers() const { return state_->peers; }

std::size_t Session::sim_ticks() const {
  if (state_->hosts.empty())
    return 0;
  auto n = state_->hosts[0]->trace.size();
  for (int slot = 1; slot < state_->peers; ++slot)
    n = std::min(n, state_->hosts[std::size_t(slot)]->trace.size());
  return n;
}

bool Session::aligned() const {
  if (state_->hosts.empty())
    return true;
  const auto n = state_->hosts[0]->trace.size();
  for (int slot = 1; slot < state_->peers; ++slot)
    if (state_->hosts[std::size_t(slot)]->trace.size() != n)
      return false;
  return true;
}

std::size_t Session::render_steps() const { return state_->render_steps; }

bool Session::synced() const { return state_->synced && !state_->failed; }

std::uint64_t Session::input_packets() const { return state_->input_packets; }

std::vector<std::uint8_t> Session::trace_bytes(int peer) const {
  if (peer < 0 || peer >= state_->peers)
    throw Error("trace peer is outside the session");
  Writer writer;
  writer.bytes.reserve(state_->hosts[std::size_t(peer)]->trace.size() *
                       TRACE_RECORD);
  for (const auto &point : state_->hosts[std::size_t(peer)]->trace)
    write_point(writer, point);
  return writer.bytes;
}

std::uint64_t Session::free_spot_searches(int peer) const {
  if (peer < 0 || peer >= state_->peers)
    throw Error("search peer is outside the session");
  return state_->hosts[std::size_t(peer)]->free_spot_searches;
}

int Session::step(const std::vector<Sample> &samples) {
  auto &state = *state_;
  if (state.failed)
    throw Error("lockstep session has failed");
  if (samples.size() != std::size_t(state.peers))
    throw Error("one sample is required per peer");
  if (state.render_steps >= MAX_STEPS)
    throw Error("lockstep render budget");
  ScopedFpu fixed{Fpu{Fpu::Model::Ps2, false, false}};
  try {
    int steps = -1;
    for (int slot = 0; slot < state.peers; ++slot) {
      auto &host = *state.hosts[std::size_t(slot)];
      auto inbox = state.transport.take(slot);
      auto result = host.machine.net_main(
          sample_bytes(samples[std::size_t(slot)]), samples[std::size_t(slot)].yaw,
          samples[std::size_t(slot)].camera_flag ? 1 : 0, 1, false, false,
          netwire::Integer(state.render_steps), inbox);
      // Send rotation does not deliver every peer's sample to every other
      // peer on the same rendered frame. One machine can tick while another
      // is still waiting. The shared trace is compared by sim tick, after
      // the slower peers have caught up.
      if (steps < 0)
        steps = int(result.steps);
      else
        steps += int(result.steps);
      for (const auto &sent : result.sent) {
        if (sent.data.size() >= 4 &&
            packet_word(sent.data) == netwire::PKT_INPUT)
          ++state.input_packets;
        state.transport.send(slot, sent);
      }
      for (const auto &line : host.machine.log)
        if (line.find("out of sync") != std::string::npos)
          state.synced = false;
      for (int other = 0; other < state.peers; ++other)
        if (host.machine.peers[std::size_t(other)].oos)
          state.synced = false;
      host.machine.log.clear();
    }
    state.transport.deliver();
    state.journal.insert(state.journal.end(), samples.begin(), samples.end());
    ++state.render_steps;
    return steps;
  } catch (...) {
    state.failed = true;
    throw;
  }
}

std::vector<std::uint8_t> Session::save() const {
  auto &state = *state_;
  if (state.failed)
    throw Error("a failed lockstep session is not saved");
  if (state.journal.size() != state.render_steps * std::size_t(state.peers))
    throw Error("lockstep journal is incomplete");
  const auto ticks = state.hosts[0]->trace.size();
  for (int slot = 1; slot < state.peers; ++slot)
    if (state.hosts[std::size_t(slot)]->trace.size() != ticks)
      throw Error("lockstep peers are not caught up");
  const auto id = identity_of(state.terrain);
  Writer writer;
  for (char byte : MAGIC)
    writer.u8(std::uint8_t(byte));
  writer.u32(SAVE_VERSION);
  writer.u32(std::uint32_t(state.peers));
  writer.u32(std::uint32_t(state.render_steps));
  writer.u32(std::uint32_t(ticks));
  writer.u32(id.heightfields);
  writer.u32(id.elements);
  writer.u32(id.samples);
  writer.i32(id.min_x);
  writer.i32(id.min_y);
  writer.i32(id.max_x);
  writer.i32(id.max_y);
  writer.i32(id.field_x);
  writer.i32(id.field_y);
  writer.i32(id.field_w);
  writer.i32(id.field_h);
  for (int slot = 0; slot < 4; ++slot) {
    writer.u8(state.placement.player_present[std::size_t(slot)] ? 1 : 0);
    for (auto word : state.placement.players[std::size_t(slot)])
      writer.u32(word);
  }
  writer.u8(state.placement.creature_present ? 1 : 0);
  for (auto word : state.placement.creature)
    writer.u32(word);
  for (const auto &sample : state.journal) {
    writer.u16(sample.buttons);
    for (auto stick : sample.sticks)
      writer.u8(stick);
    for (auto pressure : sample.pressure)
      writer.u8(pressure);
    writer.u16(std::uint16_t(sample.yaw));
    writer.u8(sample.camera_flag ? 1 : 0);
  }
  Writer witness;
  for (const auto &point : state.hosts[0]->trace)
    write_point(witness, point);
  writer.bytes.insert(writer.bytes.end(), witness.bytes.begin(),
                      witness.bytes.end());
  return writer.bytes;
}

Session Session::load(const std::vector<std::uint8_t> &bytes,
                      const W::Snapshot &terrain) {
  Reader reader(bytes);
  for (char byte : MAGIC)
    if (reader.u8() != std::uint8_t(byte))
      throw Error("lockstep save magic");
  if (reader.u32() != SAVE_VERSION)
    throw Error("lockstep save version");
  const int peers = int(reader.u32());
  const auto render_steps = reader.u32();
  const auto ticks = reader.u32();
  if (peers < 2 || peers > 4 || render_steps > MAX_STEPS || ticks > MAX_STEPS)
    throw Error("lockstep save is outside its budget");
  Identity id;
  id.heightfields = reader.u32();
  id.elements = reader.u32();
  id.samples = reader.u32();
  id.min_x = reader.i32();
  id.min_y = reader.i32();
  id.max_x = reader.i32();
  id.max_y = reader.i32();
  id.field_x = reader.i32();
  id.field_y = reader.i32();
  id.field_w = reader.i32();
  id.field_h = reader.i32();
  const auto have = identity_of(terrain);
  if (id.heightfields != have.heightfields || id.elements != have.elements ||
      id.samples != have.samples || id.min_x != have.min_x ||
      id.min_y != have.min_y || id.max_x != have.max_x ||
      id.max_y != have.max_y || id.field_x != have.field_x ||
      id.field_y != have.field_y || id.field_w != have.field_w ||
      id.field_h != have.field_h)
    throw Error("lockstep save terrain identity");
  Placement placement;
  for (int slot = 0; slot < 4; ++slot) {
    placement.player_present[std::size_t(slot)] = reader.u8() != 0;
    for (auto &word : placement.players[std::size_t(slot)])
      word = reader.u32();
  }
  placement.creature_present = reader.u8() != 0;
  for (auto &word : placement.creature)
    word = reader.u32();
  // The journal and witness must fit in what is left before either is
  // allocated: the header's counts are bounded, not trusted.
  reader.need(std::size_t(render_steps) * std::size_t(peers) * SAMPLE_BYTES +
              std::size_t(ticks) * TRACE_RECORD);
  std::vector<Sample> journal(std::size_t(render_steps) * std::size_t(peers));
  for (auto &sample : journal) {
    sample.buttons = reader.u16();
    for (auto &stick : sample.sticks)
      stick = reader.u8();
    for (auto &pressure : sample.pressure)
      pressure = reader.u8();
    sample.yaw = std::int16_t(reader.u16());
    sample.camera_flag = reader.u8() != 0;
  }
  const auto witness_bytes = std::size_t(ticks) * TRACE_RECORD;
  reader.need(witness_bytes);
  const auto witness_at = reader.at;
  reader.at += witness_bytes;
  if (reader.at != reader.size)
    throw Error("lockstep save has trailing bytes");

  Session session(peers, terrain, placement);
  for (std::uint32_t frame = 0; frame < render_steps; ++frame) {
    std::vector<Sample> samples(
        journal.begin() + std::ptrdiff_t(frame) * peers,
        journal.begin() + std::ptrdiff_t(frame + 1) * peers);
    session.step(samples);
  }
  if (session.sim_ticks() != ticks)
    throw Error("lockstep save tick count");
  auto restored = session.trace_bytes(0);
  if (restored.size() != witness_bytes ||
      !std::equal(restored.begin(), restored.end(), bytes.begin() +
                                                        std::ptrdiff_t(witness_at)))
    throw Error("lockstep save witness");
  for (int slot = 1; slot < peers; ++slot)
    if (session.trace_bytes(slot) != restored)
      throw Error("lockstep save peers differ");
  return session;
}

} // namespace champions::simulation::rta::netplay
