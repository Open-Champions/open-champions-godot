#include "champions/creature_anim/creature.hpp"
#include <utility>
namespace champions::simulation::rta::creature_anim {
Raw class_resource(const Memory &m, Address c, std::uint32_t i) {
  const auto def = m.load(c + 0x24, 4);
  return static_cast<Raw>(m.load(def + std::uint64_t(i) * 8 + 4, 4));
}
std::pair<Raw, Raw> virtual_target(const Memory &m, Address c,
                                   std::uint32_t slot) {
  const auto entry = m.load(c + 0x48, 4) + std::uint64_t(slot) * 8;
  return {static_cast<Raw>(m.load(entry + 4, 4)),
          static_cast<Raw>(c + s16(static_cast<Raw>(m.load(entry, 2))))};
}
Raw role_lump(const Memory &m, Address c, bool npc) {
  return npc ? static_cast<Raw>(c + 0x3c8)
             : static_cast<Raw>(m.load(m.load(c + 0x24, 4), 4));
}
Raw find_role_animation(Raw lump, const std::string &role, const Property &get,
                        const FindResource &find) {
  auto bounded = [](const std::string &s) {
    if (s.size() >= 160)
      throw ValueError("role exceeds original scratch buffer");
    for (unsigned char c : s)
      if (!c || c >= 128)
        throw ValueError("role requires ASCII");
    return s;
  };
  auto key = bounded(role + "name");
  if (get(key)) {
    auto again = get(key);
    if (!again)
      throw HostFault("property disappeared on repeated read");
    return find(lump, bounded(*again + ".anm"));
  }
  if (get("name")) {
    auto again = get("name");
    if (!again)
      throw HostFault("name disappeared on repeated read");
    auto result = find(lump, bounded(*again + "_" + role + ".anm"));
    if (result)
      return result;
  }
  return find(lump, bounded(role + ".anm"));
}
void set_class_events(Memory &m, Address c, std::uint32_t i,
                      const std::array<Raw, 4> &times,
                      const std::array<Raw, 4> &masks) {
  for (unsigned j = 0; j < 4; ++j) {
    auto clip = class_resource(m, c, i);
    m.store(std::uint64_t(clip) + 0x18 + 8 * j, 1, masks[j] & 255);
    m.store(std::uint64_t(clip) + 0x14 + 8 * j, 4, times[j]);
  }
}
Mat34 draw_matrix(const Memory &m, Address c) {
  auto u = [&](unsigned off) { return static_cast<Raw>(m.load(c + off, 4)); };
  auto facing = s16(static_cast<Raw>(m.load(c + 0x38, 2)));
  Vec3 p{u(0x10), u(0x14), u(0x3c)};
  auto off = u(0xe0);
  if (!eq(off, ZERO)) {
    p[0] = add(p[0], mul(cos16(facing), off));
    p[1] = add(p[1], mul(sin16(facing), off));
  }
  return mat34_set_pos_rot_z_scale(u(0xdc), p, s16(facing + 0x4000));
}
namespace {
void validate_vertex(const Vertex &v, int joints) {
  if (joints < 1 || joints > 64)
    throw ValueError("pose joint count must be 1..64");
  if (v.influences.empty() || v.influences.size() > 4)
    throw ValueError("one to four influences required");
  unsigned sum = 0;
  for (auto i : v.influences) {
    if (i.joint >= joints)
      throw ValueError("joint outside paired skeleton");
    sum += i.weight;
  }
  if (sum != 255)
    throw ValueError("weights must sum to 255; never renormalized");
}
Raw arg(const std::vector<monsters_1::Argument> &a, std::size_t i) {
  return std::get<std::uint32_t>(a.at(i));
}
} // namespace
std::array<std::int64_t, 3> skin_point(const Pose &p, const Vertex &v) {
  validate_vertex(v, p.joint_count);
  std::array<std::int64_t, 3> out{};
  for (auto i : v.influences)
    for (unsigned r = 0; r < 3; ++r) {
      const auto &m = p.mats[i.joint][r];
      std::int64_t n = std::int64_t(m[3]) * 1024;
      for (unsigned c = 0; c < 3; ++c)
        n += std::int64_t(m[c]) * v.position[c];
      out[r] += n * i.weight;
    }
  return out;
}
std::array<std::int64_t, 3> skin_normal(const Pose &p, const Vertex &v) {
  validate_vertex(v, p.joint_count);
  std::array<std::int64_t, 3> out{};
  for (auto i : v.influences)
    for (unsigned r = 0; r < 3; ++r)
      for (unsigned c = 0; c < 3; ++c)
        out[r] += std::int64_t(p.mats[i.joint][r][c]) * v.normal[c] * i.weight;
  return out;
}
Dispatch small_spider_dispatch() {
  return {[](auto &m, Address c, Raw name, Vec3 p, Raw f, Raw props) {
            m.smallspider_ctor(c, name, p, f, props);
          },
          [](auto &m, Address c) { m.smallspider_run(c); },
          [](auto &m, Address c, Raw s) { m.smallspider_set_state(c, s); },
          [](auto &m, Address c, Raw o) { m.smallspider_on_touch(c, o); }};
}
ClipId Environment::register_clip(Raw resource,
                                  const std::vector<std::uint8_t> &bytes) {
  if (!resource || resource > UINT32_MAX - 0x34)
    throw ValueError("class clip resource span outside raw32");
  auto found = resources.find(resource);
  if (found != resources.end()) {
    if (runtime.clip(found->second).data != bytes)
      throw Unsupported("shared resource bytes changed");
    return found->second;
  }
  auto id = runtime.add_clip(bytes);
  resources.emplace(resource, id);
  for (unsigned j = 0; j < 4; ++j) {
    const auto &e = runtime.clip(id).events[j];
    memory.poke(std::uint64_t(resource) + 0x18 + 8 * j, 1, e.mask);
    memory.poke(std::uint64_t(resource) + 0x14 + 8 * j, 4, e.seconds);
  }
  return id;
}
Creature::Creature(Address c, std::size_t slots, std::size_t poses)
    : Creature(std::make_shared<Environment>(slots, poses), c) {}
Creature::Creature(std::shared_ptr<Environment> environment, Address c)
    : environment_(std::move(environment)), address_(c), state_(0),
      runtime(environment_ ? environment_->runtime
                           : throw ValueError("null creature environment")),
      memory(environment_->memory), model(memory) {
  if (!c || c > UINT32_MAX - 0x400)
    throw ValueError("creature address outside raw32 span");
  if (environment_->actors.count(c))
    throw ValueError("creature address already bound in environment");
  state_ = runtime.add_state();
  environment_->actors.emplace(c, state_);
  auto state_arg = [this](const auto &a) {
    if (arg(a, 0) != address_ + 0x114)
      throw Unsupported("foreign animation callback state");
  };
  model.callbacks["anim_push"] = [this, state_arg](auto &, const auto &a) {
    state_arg(a);
    auto t = runtime.anim_push(state_, resolve(arg(a, 1)), arg(a, 2));
    publish_flags();
    return t;
  };
  model.callbacks["anim_start"] = [this, state_arg](auto &, const auto &a) {
    state_arg(a);
    auto t =
        runtime.anim_start(arg(a, 3), state_, resolve(arg(a, 1)), arg(a, 2));
    publish_flags();
    return t;
  };
  model.callbacks["anim_randomizeTopTime"] = [this, state_arg](auto &,
                                                               const auto &a) {
    state_arg(a);
    runtime.set_cosmetic_seed(static_cast<Raw>(memory.load(0x404460, 4)));
    runtime.anim_randomize_top_time(state_);
    memory.store(0x404460, 4, runtime.cosmetic_seed());
    return 0u;
  };
  model.callbacks["anim_jumpTopToEnd"] = [this, state_arg](auto &,
                                                           const auto &a) {
    state_arg(a);
    runtime.anim_jump_top_to_end(state_);
    return 0u;
  };
  model.callbacks["anim_releaseCaches"] = [this, state_arg](auto &,
                                                            const auto &a) {
    state_arg(a);
    runtime.anim_release_caches(state_);
    return 0u;
  };
  model.callbacks["anim_snapTopToEnd"] = [this, state_arg](auto &,
                                                           const auto &a) {
    state_arg(a);
    runtime.anim_snap_top_to_end(state_);
    return 0u;
  };
  model.callbacks["anim_syncToSimFrame"] = [this, state_arg](auto &,
                                                             const auto &a) {
    state_arg(a);
    runtime.anim_sync_to_sim_frame(state_);
    return 0u;
  };
  model.callbacks["anim_clear"] = [this, state_arg](auto &, const auto &a) {
    state_arg(a);
    runtime.anim_clear(state_);
    return 0u;
  };
  model.callbacks["anim_startBlendInOut"] = [this, state_arg](auto &,
                                                              const auto &a) {
    state_arg(a);
    auto t = runtime.anim_start_blend_in_out(arg(a, 3), arg(a, 4), state_,
                                             resolve(arg(a, 1)), arg(a, 2));
    publish_flags();
    return t;
  };
  model.callbacks["anim_advance"] = [this, state_arg](auto &, const auto &a) {
    state_arg(a);
    // Mutable event entries share resource identity with constructor writes.
    for (auto [resource, id] : pairing_.clips)
      for (unsigned j = 0; j < 4; ++j)
        runtime.clip(id).events[j] = {
            static_cast<Raw>(
                memory.load(std::uint64_t(resource) + 0x14 + 8 * j, 4)),
            static_cast<std::uint8_t>(
                memory.load(std::uint64_t(resource) + 0x18 + 8 * j, 1))};
    auto m = runtime.advance(arg(a, 4), state_, arg(a, 1) != 0);
    last_events_ = m.events;
    memory.store(arg(a, 2), 4, m.x);
    memory.store(std::uint64_t(arg(a, 2)) + 4, 4, m.y);
    memory.store(arg(a, 3), 2, static_cast<Raw>(m.yaw) & 65535);
    publish_flags();
    return static_cast<std::uint64_t>(static_cast<Raw>(m.events));
  };
}
Creature::~Creature() {
  // Destruction releases owned pose/decoder references, never shared resources.
  try {
    runtime.anim_state_dtor(state_);
  } catch (...) {
  }
  environment_->actors.erase(address_);
}
ClipId Creature::resolve(Raw p) const {
  if (!p)
    return 0;
  auto it = pairing_.clips.find(p);
  if (it == pairing_.clips.end())
    throw Unsupported("class clip resource not registered");
  return it->second;
}
void Creature::publish_flags() {
  memory.store(address_ + 0x11a, 2, runtime.state(state_).flags);
}
void Creature::check_pairing() const {
  if (!pairing_.definition ||
      memory.load(address_ + 0x24, 4) != pairing_.definition ||
      class_resource(memory, address_, 1) != pairing_.model ||
      class_resource(memory, address_, 2) != pairing_.texture)
    throw Unsupported("creature class/model/texture provenance changed");
}
void Creature::bind(Pairing p, Mesh mesh, Dispatch dispatch) {
  if (!p.definition || !p.model || !p.base || p.clips.empty())
    throw ValueError("explicit class pairing required");
  if (!dispatch.construct || !dispatch.run || !dispatch.change_state ||
      !dispatch.touch)
    throw ValueError("incomplete class dispatch");
  const auto &base = runtime.clip(p.base);
  if (!base.joint_count)
    throw ValueError("empty creature skeleton");
  std::map<ClipId, Raw> identities;
  bool has_base = false;
  for (const auto &[resource, id] : p.clips) {
    if (!resource || resource > UINT32_MAX - 0x34)
      throw ValueError("class clip resource span outside raw32");
    if (!identities.emplace(id, resource).second)
      throw Unsupported("distinct resources cannot share one clip identity");
    has_base |= id == p.base;
    auto existing = environment_->resources.find(resource);
    if (existing != environment_->resources.end() && existing->second != id)
      throw Unsupported("resource must retain shared clip identity");
    for (auto [other, shared] : environment_->resources)
      if (shared == id && other != resource)
        throw Unsupported(
            "one clip identity cannot alias distinct resource addresses");
    const auto &clip = runtime.clip(id);
    if (clip.joint_count != base.joint_count || clip.depth != base.depth ||
        clip.bind != base.bind)
      throw Unsupported("incompatible ordered class skeleton");
  }
  if (!has_base)
    throw ValueError("base clip must belong to class resources");
  if (mesh.vertices.size() > 600000)
    throw ResourceLimit("mesh vertex budget");
  for (const auto &v : mesh.vertices)
    validate_vertex(v, base.joint_count);
  // Admit preorder hierarchies only at the host boundary; low-level Runtime
  // retains the reference's stale-depth behavior for exact synthetic tests.
  int previous = -1;
  for (auto d : base.depth) {
    if (d < 0 || d > previous + 1)
      throw Unsupported("non-preorder creature skeleton");
    previous = d;
  }
  pairing_ = std::move(p);
  mesh_ = std::move(mesh);
  dispatch_ = std::move(dispatch);
  for (auto [resource, id] : pairing_.clips) {
    if (!environment_->resources.emplace(resource, id).second)
      continue;
    for (unsigned j = 0; j < 4; ++j) {
      const auto &e = runtime.clip(id).events[j];
      memory.poke(std::uint64_t(resource) + 0x18 + 8 * j, 1, e.mask);
      memory.poke(std::uint64_t(resource) + 0x14 + 8 * j, 4, e.seconds);
    }
  }
}
void Creature::construct(Raw name, Vec3 pos, Raw facing, Raw props) {
  if (!dispatch_.construct)
    throw Unsupported("unbound creature");
  dispatch_.construct(model, address_, name, pos, facing, props);
  check_pairing();
}
void Creature::change_state(Raw s) {
  check_pairing();
  dispatch_.change_state(model, address_, s);
}
void Creature::touch(Raw o) {
  check_pairing();
  dispatch_.touch(model, address_, o);
}
Frame Creature::tick(std::uint32_t sim, std::uint32_t draw_frame) {
  check_pairing();
  if (last_tick_ &&
      (sim - *last_tick_ == 0 || sim - *last_tick_ >= 0x80000000u))
    throw ValueError("duplicate or stale simulation tick");
  // Consume the tick before external calls: failure prefixes cannot be
  // replayed.
  last_tick_ = sim;
  last_events_ = 0;
  runtime.set_sim_frame(sim);
  runtime.set_draw_frame(draw_frame);
  memory.poke(creature_hp::G_SIM_FRAME, 4, sim);
  dispatch_.run(model, address_);
  return draw(draw_frame);
}
Frame Creature::draw(std::uint32_t frame) {
  check_pairing();
  runtime.set_draw_frame(frame);
  Frame out;
  out.pose = runtime.draw_pose(state_);
  out.object = draw_matrix(memory, address_);
  out.model = pairing_.model;
  out.texture = pairing_.texture;
  out.motion.events = last_events_;
  out.motion.x = static_cast<Raw>(memory.load(address_ + 0xbc, 4));
  out.motion.y = static_cast<Raw>(memory.load(address_ + 0xc0, 4));
  out.motion.yaw = s16(static_cast<Raw>(memory.load(address_ + 0xc4, 2)));
  out.positions.reserve(mesh_.vertices.size());
  out.normals.reserve(mesh_.vertices.size());
  for (const auto &v : mesh_.vertices) {
    out.positions.push_back(skin_point(out.pose, v));
    out.normals.push_back(skin_normal(out.pose, v));
  }
  return out;
}
} // namespace champions::simulation::rta::creature_anim
