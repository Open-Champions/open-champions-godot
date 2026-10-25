#include "champions/fx_simulation.hpp"
#include "champions/fx.hpp"
#include <algorithm>
#include <cstring>

// Wiring of the native-fx-2 parts.  The exactness lives in the parts (fx_exact, fx_render,
// fx_recipes, fx_skills, lighting), each checked against its pinned reference; this file
// only routes calls between them as docs/effects.md 13.3, 14, 15 and 17 describe, and turns
// the GS packets into draw runs for a presentation layer.
namespace champions::rta::fx::sim {
namespace {
using exact::Vec3d;

double to_double(Raw raw) {
  float f;
  std::memcpy(&f, &raw, sizeof f);
  return static_cast<double>(f);
}
Vec3d to_double(const Vec3 &v) { return {to_double(v[0]), to_double(v[1]), to_double(v[2])}; }
lighting::Vec3 to_light(const Vec3 &v) { return {to_double(v[0]), to_double(v[1]), to_double(v[2])}; }
Raw to_raw(double d) { return rounding::pattern(d); }

std::array<std::uint8_t, 4> bytes_of(std::uint32_t word) {
  // How beam_create splits its t0 colour word into the trails' four colour bytes is not
  // documented; the GS RGBAQ order (red in the low byte) is used: 0x5e808080 (docs 14.6) is
  // grey 0x80 with alpha 0x5e.  I.
  return {static_cast<std::uint8_t>(word), static_cast<std::uint8_t>(word >> 8),
          static_cast<std::uint8_t>(word >> 16), static_cast<std::uint8_t>(word >> 24)};
}

const skills::Field &need(const skills::FxCall &c, const char *name) {
  const skills::Field *f = c.field(name);
  if (!f) throw Refusal(c.primitive + ": missing field " + name);
  return *f;
}
Raw raw_of(const skills::FxCall &c, const char *name) { return need(c, name).raw; }
std::uint32_t int_of(const skills::FxCall &c, const char *name) { return need(c, name).raw; }
Vec3 vec_of(const skills::FxCall &c, const char *name) {
  const skills::Field &f = need(c, name);
  if (f.kind != skills::Field::Kind::Vector || f.lanes.size() != 3)
    throw Refusal(c.primitive + ": field " + name + " is not a vector");
  return {f.lanes[0], f.lanes[1], f.lanes[2]};
}
} // namespace

// ---- glow textures ------------------------------------------------------------------------------
const std::vector<GlowTexture> &glow_textures() {
  static const std::vector<GlowTexture> rows = {
      {0x44FE04, ENGINE_LUMP, "halo1.tex"}, {0x4F848C, FX_LUMP, "halo2.tex"},
      {0x4F8490, FX_LUMP, "halo3.tex"},     {0x4F8494, FX_LUMP, "halo4.tex"},
      {0x4F8498, FX_LUMP, "wisphalo.tex"},  {0x4F849C, FX_LUMP, "halo5.tex"},
      {0x4F84A0, FX_LUMP, "halo7.tex"},     {0x4F84A4, FX_LUMP, "halo8.tex"}};
  return rows;
}
const GlowTexture *glow_texture(std::uint32_t word) {
  for (const GlowTexture &t : glow_textures())
    if (t.global == word) return &t;
  return nullptr;
}

// ---- EffectWorld --------------------------------------------------------------------------------
EffectWorld::EffectWorld() : decals(2), level(lighting::Level::boot()) {}

bool EffectWorld::load_definitions(const std::string &json, std::string &error) {
  for (const auto &kv : definitions_)
    if (kv.second.live > 0) {
      error = "definitions are resident in the particle pools";
      return false;
    }
  std::map<std::uint32_t, Definition> parsed;
  if (!load_definition_json(json, parsed, error)) return false;
  definitions_.clear();
  for (const auto &kv : parsed) definitions_.emplace(kv.first, kv.second.exact);
  return true;
}
const exact::ParticleDef *EffectWorld::definition(std::uint32_t address) const {
  const auto it = definitions_.find(address);
  return it == definitions_.end() ? nullptr : &it->second;
}

bool EffectWorld::add_definition(std::uint32_t address, const exact::ParticleDef &d) {
  const auto it = definitions_.find(address);
  if (it != definitions_.end() && it->second.live > 0) return false;
  definitions_.insert_or_assign(address, d);
  return true;
}

void EffectWorld::particle_spawn(std::uint32_t definition, const Vec3 &pos, const Vec3 &vel,
                                 Raw backdate) {
  const auto it = definitions_.find(definition);
  if (it == definitions_.end()) {
    ++missing_definitions;
    ++missing_by_definition[definition];
    return;
  }
  particles.spawn(it->second, to_double(pos), to_double(vel), to_double(backdate));
}
void EffectWorld::particle_spawn_unrounded(std::uint32_t definition, const exact::Vec3d &pos,
                                           const exact::Vec3d &vel, double backdate) {
  const auto it = definitions_.find(definition);
  if (it == definitions_.end()) {
    ++missing_definitions;
    ++missing_by_definition[definition];
    return;
  }
  particles.spawn(it->second, pos, vel, backdate);
}
bool EffectWorld::glow_add(const render::Glow &glow) { return glows.add(glow); }
std::uint32_t EffectWorld::group_create() {
  return static_cast<std::uint32_t>(particles.group_create());
}
void EffectWorld::group_begin(std::uint32_t h) { particles.group_begin(h); }
void EffectWorld::group_end(std::uint32_t h) { particles.group_end(h); }
void EffectWorld::group_free(std::uint32_t h) {
  if (particles.group(h)) unplaced_groups.erase(h & 0xff);
  particles.group_free(h);
}
void EffectWorld::group_set_matrix(std::uint32_t h, const Mat34 &m) {
  if (!particles.group(h)) return;
  exact::Mat34d d{};
  for (std::size_t i = 0; i < 12; ++i) d[i] = to_double(m[i]);
  particles.group_set_matrix(h, d);
  unplaced_groups.erase(h & 0xff);
}
void EffectWorld::group_set_unknown_matrix(std::uint32_t h) {
  if (particles.group(h)) unplaced_groups.insert(h & 0xff);
}

void EffectWorld::decal_add(Raw size, const Vec3 &pos, std::int32_t angle, std::int32_t type,
                            std::int32_t grow) {
  const lighting::Rgb tint = lighting::decal_tint(to_light(pos), light_nodes(), level);
  decals.add(to_double(size), to_double(pos), angle, type, grow, {tint[0], tint[1], tint[2]});
}

void EffectWorld::light_effect(const Vec3 &pos, std::uint32_t follow, bool signed_colour,
                               const std::array<Raw, 4> &c0, const std::array<Raw, 4> &c1,
                               Raw radius, Raw radius_end, Raw floor, Raw seconds) {
  std::array<double, 4> a{}, b{};
  for (std::size_t i = 0; i < 4; ++i) {
    a[i] = to_double(c0[i]);
    b[i] = to_double(c1[i]);
  }
  Effect e{lighting::LightEffect::from_floats(a, b, to_double(radius), to_double(radius_end),
                                              to_double(seconds), sim_frame_, signed_colour,
                                              to_double(floor), to_light(pos)),
           follow, lighting::Node(to_light(pos), {0, 0, 0}, {0, 0, 0}, 0.0)};
  effects_.push_back(std::move(e));
}
void EffectWorld::link_light(std::uint64_t key, const lighting::Node &node) {
  linked_.insert_or_assign(key, node);
}
void EffectWorld::unlink_light(std::uint64_t key) { linked_.erase(key); }

std::vector<lighting::Node> EffectWorld::light_nodes() const {
  std::vector<lighting::Node> out;
  for (const auto &kv : linked_) out.push_back(kv.second);
  for (const Effect &e : effects_) out.push_back(e.node);
  return out;
}

std::uint32_t EffectWorld::apply(const recipes::Call &c) {
  using K = recipes::Kind;
  switch (c.kind) {
  case K::Spawn:
    particle_spawn(static_cast<std::uint32_t>(c.i[0]), c.v[0], c.v[1], c.f[0]);
    return 0;
  case K::Glow: {
    render::Glow g;
    g.pos = c.v[0];
    g.bias = c.f[0];
    g.size = c.f[1];
    for (std::size_t k = 0; k < 4; ++k) g.rgba[k] = static_cast<std::uint8_t>(c.rgba[k]);
    g.tex = static_cast<std::uint32_t>(c.i[0]);
    g.angle = static_cast<std::uint16_t>(c.i[1]);
    g.flags = static_cast<std::int16_t>(c.i[2]);
    glow_add(g);
    return 0;
  }
  case K::TrailCreate:
    return trails.create(c.f[0], static_cast<std::int32_t>(c.i[0]),
                         static_cast<std::int32_t>(c.i[1]), static_cast<std::uint32_t>(c.i[2]),
                         static_cast<std::uint8_t>(c.i[3]), static_cast<std::uint8_t>(c.i[4]),
                         static_cast<std::uint8_t>(c.i[5]), static_cast<std::uint8_t>(c.i[6]));
  case K::TrailPair:
    trails.add_pair(c.f[0], static_cast<std::uint32_t>(c.i[0]), c.v[0], c.v[1]);
    return 0;
  case K::BeamCreate:
    return beams.create(c.f[0], c.f[1], c.v[0], c.v[1], static_cast<std::int32_t>(c.i[0]),
                        static_cast<std::int32_t>(c.i[1]),
                        bytes_of(static_cast<std::uint32_t>(c.i[2])), sim_frame_, in_sim_tick,
                        cosmetic, libc);
  case K::BeamUpdate:
    beams.update(static_cast<std::uint32_t>(c.i[0]), c.v[0], c.v[1],
                 static_cast<std::int32_t>(c.i[1]), static_cast<std::int32_t>(c.i[2]), sim_frame_);
    return 0;
  case K::FxSpawn: {
    // A recipe's own fx_spawn call (weapon status: id 0x11).  Id 5 draws the shared seed and
    // belongs to the simulation, never to a world.
    if (c.i[0] == 5) throw Refusal("fx_spawn id 5 reached the per-machine world");
    core::Lcg unused;
    const bool near = near_camera_focus ? near_camera_focus(c.v[0]) : true;
    apply(recipes::fx_spawn(static_cast<std::int32_t>(c.i[0]), c.v[0], cosmetic, unused, near,
                            static_cast<std::int32_t>(decals.detail), &made));
    return 0;
  }
  default:
    // Queries, sounds and the gameplay records (pool, Fire, PhProp, GutChunk ...): the
    // simulation has acted on them.
    return 0;
  }
}
void EffectWorld::apply(const recipes::Calls &calls) {
  for (const recipes::Call &c : calls) apply(c);
}

std::uint32_t EffectWorld::apply(const skills::FxCall &c) {
  const std::string &p = c.primitive;
  if (p == "particle") {
    particle_spawn(int_of(c, "definition"), vec_of(c, "pos"), vec_of(c, "vel"), raw_of(c, "backdate"));
  } else if (p == "glow") {
    render::Glow g;
    g.pos = vec_of(c, "pos");
    g.rgba = {static_cast<std::uint8_t>(int_of(c, "r")), static_cast<std::uint8_t>(int_of(c, "g")),
              static_cast<std::uint8_t>(int_of(c, "b")), static_cast<std::uint8_t>(int_of(c, "a"))};
    g.tex = int_of(c, "texture");
    g.angle = static_cast<std::uint16_t>(int_of(c, "angle"));
    g.flags = static_cast<std::int16_t>(int_of(c, "flags"));
    g.bias = raw_of(c, "bias");
    g.size = raw_of(c, "size");
    glow_add(g);
  } else if (p == "light_effect") {
    light_effect(vec_of(c, "pos"), int_of(c, "follow"), int_of(c, "flag") != 0,
                 {raw_of(c, "c0"), raw_of(c, "c1"), raw_of(c, "c2"), raw_of(c, "c3")},
                 {raw_of(c, "d0"), raw_of(c, "d1"), raw_of(c, "d2"), raw_of(c, "d3")},
                 raw_of(c, "radius"), raw_of(c, "radius_end"), raw_of(c, "floor"), raw_of(c, "seconds"));
  } else if (p == "trail") {
    return trails.create(raw_of(c, "lifetime"), static_cast<std::int32_t>(int_of(c, "row")),
                         static_cast<std::int32_t>(int_of(c, "width")), int_of(c, "flags"),
                         static_cast<std::uint8_t>(int_of(c, "r")), static_cast<std::uint8_t>(int_of(c, "g")),
                         static_cast<std::uint8_t>(int_of(c, "b")), static_cast<std::uint8_t>(int_of(c, "a")));
  } else if (p == "trail_point") {
    trails.add_point(int_of(c, "handle"), vec_of(c, "pos"));
  } else if (p == "trail_release") {
    trails.release(int_of(c, "handle"));
  } else if (p == "trail_color") {
    trails.set_color(int_of(c, "handle"), static_cast<std::uint8_t>(int_of(c, "r")),
                     static_cast<std::uint8_t>(int_of(c, "g")), static_cast<std::uint8_t>(int_of(c, "b")),
                     static_cast<std::uint8_t>(int_of(c, "a")));
  } else if (p == "trail_width") {
    trails.set_length(int_of(c, "handle"), static_cast<std::int32_t>(int_of(c, "value")));
  } else if (p == "beam") {
    return beams.create(raw_of(c, "jitter"), raw_of(c, "scale"), vec_of(c, "from"), vec_of(c, "to"),
                        static_cast<std::int32_t>(int_of(c, "width")),
                        static_cast<std::int32_t>(int_of(c, "depth")), bytes_of(int_of(c, "rgba")),
                        sim_frame_, in_sim_tick, cosmetic, libc);
  } else if (p == "beam_update") {
    beams.update(int_of(c, "handle"), vec_of(c, "from"), vec_of(c, "to"),
                 static_cast<std::int32_t>(int_of(c, "alpha")),
                 static_cast<std::int32_t>(int_of(c, "width")), sim_frame_);
  } else if (p == "decal") {
    decal_add(raw_of(c, "size"), vec_of(c, "pos"), static_cast<std::int32_t>(int_of(c, "angle")),
              static_cast<std::int32_t>(int_of(c, "type")), static_cast<std::int32_t>(int_of(c, "grow")));
  } else if (p == "group_create") {
    return group_create();
  } else if (p == "group_begin") {
    group_begin(int_of(c, "handle"));
  } else if (p == "group_end") {
    group_end(int_of(c, "handle"));
  } else if (p == "group_free") {
    group_free(int_of(c, "handle"));
  } else if (p == "group_matrix") {
    const skills::Field &m = need(c, "matrix");
    if (m.kind == skills::Field::Kind::Vector && m.lanes.size() == 12) {
      Mat34 mm{};
      std::copy(m.lanes.begin(), m.lanes.end(), mm.begin());
      group_set_matrix(int_of(c, "handle"), mm);
    } else {
      group_set_unknown_matrix(int_of(c, "handle"));
    }
  }
  // fx_spawn, light links and the opaque rows are the simulation's (EffectSimulation).
  return 0;
}

void EffectWorld::tick(std::int32_t sim_frame, bool cutscene, bool hold_trails,
                       const std::map<std::uint32_t, Vec3> &positions) {
  sim_frame_ = sim_frame;
  particles.update(cutscene);
  trails.update(hold_trails);
  // LightEffect_run 0x1ee050, every simulated tick while it exists.
  for (auto it = effects_.begin(); it != effects_.end();) {
    lighting::Vec3 owner{};
    const lighting::Vec3 *at = nullptr;
    if (it->follow) {
      const auto p = positions.find(it->follow);
      if (p != positions.end()) {
        owner = to_light(p->second);
        at = &owner;
      }
    }
    const bool alive = it->light.run(sim_frame, it->node, at);
    it = alive ? std::next(it) : effects_.erase(it);
  }
}

render::ParticleFrame EffectWorld::particle_frame() const {
  render::ParticleFrame f;
  f.clock = particles.clock();
  f.pages.resize(exact::PAGE_COUNT);
  for (int pg = 0; pg < exact::PAGE_COUNT; ++pg)
    for (int c = 0; c < 256; ++c) f.pages[static_cast<std::size_t>(pg)][static_cast<std::size_t>(c)] = particles.cell(pg, c);
  for (int s = 0; s < 32; ++s) f.types[static_cast<std::size_t>(s)] = particles.slot_record(s).words;
  auto lists = [](const exact::ListSet &in, render::ParticleListSet &out) {
    for (std::size_t k = 0; k < 12; ++k) {
      for (int pg : in[k].pages) out.lists[k].pages.push_back(static_cast<std::uint32_t>(pg));
      out.lists[k].head = static_cast<std::uint32_t>(in[k].head);
      out.lists[k].tail = static_cast<std::uint32_t>(in[k].tail);
    }
  };
  lists(particles.global_set(), f.global);
  for (int slot = 0; slot < exact::GROUP_COUNT; ++slot) {
    const exact::GroupSlotView g = particles.group_slot(slot);
    if (!g.live || unplaced_groups.count(static_cast<std::uint32_t>(slot))) continue;
    render::ParticleListSet set;
    for (std::size_t i = 0; i < 12; ++i) set.matrix[i] = to_raw(g.matrix[i]);
    lists(*g.lists, set);
    f.groups.push_back(std::move(set));
  }
  return f;
}

std::vector<render::Packet> EffectWorld::draw(const Camera &camera, std::int32_t sim_frame,
                                              std::int32_t net_frame, bool frozen) {
  // particle_draw 0x194888 first steps the flipbook of the resident definitions.
  for (int s = 1; s < exact::SLOT_COUNT; ++s)
    if (const exact::ParticleDef *d = particles.slot_definition(s))
      exact::flipbook_step(*const_cast<exact::ParticleDef *>(d), sim_frame);
  render::FrameInput in;
  in.view = camera.view;
  in.proj_scale = camera.proj_scale;
  in.camera = camera.camera;
  in.viewproj = camera.viewproj;
  in.stale_zw = stale_zw;
  in.waitq = waitq;
  in.sim_frame = sim_frame;
  in.net_frame = net_frame;
  in.draw_frame = draw_frame;
  in.frozen = frozen;
  const render::ParticleFrame frame = particle_frame();
  std::vector<render::Packet> out =
      render::draw_frame(in, frame, trails, &beams, glows, glow_dims, cosmetic);
  decals.grow_step(frozen);   // decal_drawTask 0x19f4e8
  ++draw_frame;
  return out;
}

// ---- EffectSimulation ---------------------------------------------------------------------------
bool EffectSimulation::is_gameplay(std::string_view name) {
  // Presentation-only call-outs; every other non-effect call-out changes or reads game state
  // (damage, objects, path nodes, world and model queries, isNearView which gates
  // StaticFire's damage: the D6/D7 hazard of docs/skills.md) and must be answered by the game.
  return !(name == "sound_playAtPos" || name == "sound_playOnObject" || name == "res_find" ||
           name == "libc_sprintf");
}

EffectSimulation::EffectSimulation(core::Lcg &shared,
                                   std::shared_ptr<const skills::RecipeConstants> constants)
    : shared_(&shared), constants_(std::move(constants)) {
  if (!constants_) throw Refusal("skill recipe constants are required (fx_skill_recipes.py --json)");
  classes_ = std::make_unique<skills::Model>(*constants_);
  // The glow texture globals hold their own address (the native token convention).
  for (const GlowTexture &t : glow_textures()) classes_->mem.poke(t.global, 4, t.global);
  classes_->fallback = [this](skills::Model &m, const skills::Callout &c) -> std::optional<skills::Value> {
    if (c.name == "light_link" || c.name == "light_relink") {
      if (!c.args.empty() && c.args[0].is_int()) model_lights_.insert(c.args[0].i);
      if (world_ && !c.args.empty() && c.args[0].is_int())
        world_->link_light(c.args[0].i, light_node_from_record(
            [&m](std::int64_t a, unsigned n) { return m.mem.load(a, n); },
            static_cast<std::int64_t>(c.args[0].i)));
      return skills::Value::integer(0);
    }
    if (c.name == "light_unlink") {
      if (!c.args.empty() && c.args[0].is_int()) {
        model_lights_.erase(c.args[0].i);
        if (world_) world_->unlink_light(c.args[0].i);
      }
      return skills::Value::integer(0);
    }
    if (std::optional<skills::Value> v = answer(c)) return v;
    if (hooks.callout)
      if (std::optional<skills::Value> v = hooks.callout(c)) return v;
    if (is_gameplay(c.name))
      throw Refusal("no game answer for the call-out " + c.name + " (GameHooks::callout)");
    return std::nullopt;
  };
}

void EffectSimulation::attach(EffectWorld *world) { world_ = world; }

recipes::PoolNew EffectSimulation::pool(std::vector<std::uint32_t> &blocks, const char *who) {
  return [this, &blocks, who](std::uint32_t size) {
    if (!hooks.pool_new) throw Refusal(std::string(who) + " needs GameHooks::pool_new");
    const std::uint32_t b = hooks.pool_new(size);
    blocks.push_back(b);
    return b;
  };
}

// The recipes construct their object only when pool_new gave a block (docs 14.1, 14.4,
// 14.5): the n-th constructor record goes with the n-th non-zero block.
void EffectSimulation::construct(const recipes::Calls &calls, recipes::Kind kind,
                                 const std::vector<std::uint32_t> &blocks) {
  if (!hooks.construct) return;
  std::vector<std::uint32_t> given;
  for (std::uint32_t b : blocks)
    if (b) given.push_back(b);
  std::size_t next = 0;
  for (const recipes::Call &c : calls)
    if (c.kind == kind) {
      if (next >= given.size()) throw Refusal("a constructor record without a pool block");
      hooks.construct(given[next++], c);
    }
}

recipes::Calls EffectSimulation::fx_spawn(std::int32_t id, const Vec3 &pos, bool near,
                                          std::int32_t gore) {
  if (id != 5 && !world_) return {};   // cosmetic only: nothing other machines see
  std::vector<std::uint32_t> blocks;
  core::Lcg &cosmetic = world_ ? world_->cosmetic : headless_cosmetic_;
  recipes::MadeFlags &made = world_ ? world_->made : headless_made_;
  recipes::Calls calls = recipes::fx_spawn(id, pos, cosmetic, *shared_, near, gore, &made,
                                           pool(blocks, "fx_spawn id 5"));
  construct(calls, recipes::Kind::Fire, blocks);
  if (world_) world_->apply(calls);
  return calls;
}

recipes::Calls EffectSimulation::shatter_to_loose_ice(recipes::SampleModel &model, const Vec3 &pos,
                                                      std::uint32_t yaw16, Raw count, Raw scale,
                                                      Raw offset, std::uint32_t anim) {
  std::vector<std::uint32_t> blocks;
  recipes::Calls calls = recipes::shatter_to_loose_ice(model, pos, yaw16, count, scale, offset, anim,
                                                       *shared_, pool(blocks, "the ice shatter"));
  construct(calls, recipes::Kind::PhProp, blocks);
  return calls;
}

recipes::Calls EffectSimulation::lose_body_part(recipes::PartClass cls, recipes::Creature &creature,
                                                std::uint32_t part,
                                                const recipes::ShowMask &show_mask) {
  std::vector<std::uint32_t> blocks;
  recipes::Calls calls =
      recipes::lose_body_part(cls, creature, part, show_mask, pool(blocks, "losing a body part"));
  construct(calls, recipes::Kind::Gut, blocks);
  return calls;
}

recipes::Calls EffectSimulation::disintegrate_model(recipes::SampleModel &model, const Vec3 &pos,
                                                    std::uint32_t yaw16, Raw scale, Raw offset,
                                                    bool alternate, std::uint32_t anim) {
  if (!world_) return {};
  recipes::Calls calls = recipes::disintegrate_model(model, pos, yaw16, scale, offset, alternate,
                                                     anim, world_->cosmetic);
  world_->apply(calls);
  return calls;
}

// Placeholders handed to a recipe in place of a per-machine handle it has not got yet.
namespace {
constexpr std::uint32_t PLACEHOLDER = 0xFFFF0000u;
bool placeholder(std::uint32_t h) { return (h & 0xFFFF0000u) == PLACEHOLDER; }
}

std::map<std::uint32_t, std::uint32_t> EffectSimulation::apply_with_handles(const recipes::Calls &calls) {
  std::map<std::uint32_t, std::uint32_t> real;
  std::uint32_t next_trail = PLACEHOLDER | 0x100, next_beam = PLACEHOLDER | 0x200;
  for (recipes::Call c : calls) {
    if (c.kind == recipes::Kind::TrailCreate) {
      real[next_trail++] = world_->apply(c);
      continue;
    }
    if (c.kind == recipes::Kind::BeamCreate) {
      real[next_beam++] = world_->apply(c);
      continue;
    }
    if ((c.kind == recipes::Kind::TrailPair || c.kind == recipes::Kind::BeamUpdate) &&
        placeholder(static_cast<std::uint32_t>(c.i[0]))) {
      const auto it = real.find(static_cast<std::uint32_t>(c.i[0]));
      c.i[0] = it == real.end() ? 0 : it->second;
    }
    world_->apply(c);
  }
  return real;
}

recipes::Calls EffectSimulation::model_status_effects(Raw amount, Raw scale, recipes::WeaponItem &item,
                                                      recipes::WeaponModel *model,
                                                      recipes::WeaponFxState &state, const Vec3 &pos,
                                                      std::uint32_t yaw16, recipes::HolderModel *holder,
                                                      std::uint32_t anim, bool swinging,
                                                      std::int32_t joint_add, const Mat34 *override_matrix,
                                                      std::uint32_t level_flags, bool dialog_open) {
  if (!world_) return {};
  recipes::WeaponContext ctx;
  ctx.level_flags = level_flags;
  ctx.gore = static_cast<std::int32_t>(world_->decals.detail);
  ctx.sim_frame = static_cast<std::uint32_t>(world_->sim_frame());
  ctx.dialog_open = dialog_open;
  ctx.answers.trail_create = PLACEHOLDER | 0x100;
  ctx.answers.beam_create = PLACEHOLDER | 0x200;
  ctx.answers.beam_alive = world_->beams.is_alive(state.beam) ? 1 : 0;
  recipes::Calls calls = recipes::model_status_effects(amount, scale, item, model, state, pos, yaw16, holder,
                                                       anim, swinging, joint_add, override_matrix,
                                                       world_->cosmetic, ctx);
  const auto real = apply_with_handles(calls);
  for (std::uint32_t *h : {&state.trail, &state.beam})
    if (placeholder(*h)) {
      const auto it = real.find(*h);
      *h = it == real.end() ? 0 : it->second;
    }
  return calls;
}

recipes::Calls EffectSimulation::model_arcs(recipes::SampleModel *model, const Vec3 &pos, std::uint32_t yaw16,
                                            std::uint32_t anim, Raw alpha, Raw scale, Raw offset,
                                            recipes::ArcState &state, std::uint32_t rgba, bool near) {
  if (!world_) return {};
  std::uint32_t next = PLACEHOLDER | 0x200;
  recipes::Calls calls = recipes::model_arcs(model, pos, yaw16, anim, alpha, scale, offset, state, rgba,
                                             world_->cosmetic, near,
                                             [&next](const Vec3 &, const Vec3 &) { return next++; });
  const auto real = apply_with_handles(calls);
  for (recipes::Arc &arc : state.arcs)
    if (placeholder(arc.beam)) {
      const auto it = real.find(arc.beam);
      arc.beam = it == real.end() ? 0 : it->second;
    }
  return calls;
}

recipes::Calls EffectSimulation::model_sparkle(recipes::SampleModel *model, const Vec3 &pos, std::uint32_t yaw16,
                                               std::uint32_t anim, Raw amount, Raw offset, bool near) {
  if (!world_) return {};
  recipes::Calls calls = recipes::model_sparkle(model, pos, yaw16, anim, amount, offset, world_->cosmetic, near);
  world_->apply(calls);
  return calls;
}

recipes::Calls EffectSimulation::model_glow(recipes::SampleModel *model, const std::uint32_t *obj_flags,
                                            recipes::GlowRamp &ramp, const Vec3 &pos, std::uint32_t yaw16,
                                            std::uint32_t anim, Raw scale, Raw offset, Raw fade,
                                            const std::array<std::uint32_t, 4> &target_a,
                                            const std::array<std::uint32_t, 4> &target_b, std::int32_t mode,
                                            std::int32_t size_add, bool near) {
  if (!world_) return {};
  recipes::Calls calls = recipes::model_glow(model, obj_flags, ramp, pos, yaw16, anim, scale, offset, fade,
                                             target_a, target_b, mode, size_add,
                                             static_cast<std::uint32_t>(world_->sim_frame()), near);
  world_->apply(calls);
  return calls;
}

recipes::Calls EffectSimulation::emit_from_model_verts(recipes::SampleModel &model, const Vec3 &pos,
                                                       std::uint32_t yaw16, std::uint32_t anim,
                                                       std::int32_t count, std::uint32_t definition,
                                                       const Vec3 *direction, const Vec3 *offset, Raw speed) {
  if (!world_) return {};
  recipes::Calls calls = recipes::emit_from_model_verts(model, pos, yaw16, anim, count, definition, direction,
                                                        offset, speed, world_->cosmetic);
  world_->apply(calls);
  return calls;
}

std::optional<skills::Value> EffectSimulation::answer(const skills::Callout &call) {
  const std::optional<skills::FxCall> fx = skills::to_primitive(call);
  if (!fx) return std::nullopt;
  if (fx->primitive == "fx_spawn") {
    const skills::Field *id = fx->field("id");
    const skills::Field *pos = fx->field("pos");
    if (!id || !pos || pos->lanes.size() != 3) throw Refusal("fx_spawn call-out without id and position");
    const Vec3 p{pos->lanes[0], pos->lanes[1], pos->lanes[2]};
    const bool near = world_ && world_->near_camera_focus ? world_->near_camera_focus(p) : true;
    fx_spawn(static_cast<std::int32_t>(static_cast<std::int32_t>(id->raw)), p, near,
             world_ ? static_cast<std::int32_t>(world_->decals.detail) : 2);
    return skills::Value::integer(0);
  }
  if (!world_) return skills::Value::integer(0);   // per-machine handles: 0 headless
  return skills::Value::integer(world_->apply(*fx));
}

void EffectSimulation::replay(const std::vector<skills::Callout> &calls) {
  for (const skills::Callout &c : calls) answer(c);
}

void EffectSimulation::sync_lights() {
  if (!world_) return;
  for (std::uint64_t addr : model_lights_)
    world_->link_light(addr, light_node_from_record(
        [this](std::int64_t a, unsigned n) { return classes_->mem.load(a, n); },
        static_cast<std::int64_t>(addr)));
}

lighting::Node light_node_from_record(const std::function<std::uint64_t(std::int64_t, unsigned)> &load,
                                      std::int64_t a) {
  auto s16 = [&](std::int64_t at) {
    const auto v = static_cast<std::uint16_t>(load(at, 2));
    return static_cast<int>(static_cast<std::int16_t>(v));
  };
  auto f = [&](std::int64_t at) { return to_double(static_cast<Raw>(load(at, 4))); };
  lighting::Node n(lighting::Vec3{f(a + 0x30), f(a + 0x34), f(a + 0x38)},
                   {s16(a + 0xa), s16(a + 0xc), s16(a + 0xe)},
                   {s16(a + 0x12), s16(a + 0x14), s16(a + 0x16)}, f(a + 0x1c), f(a + 0x24),
                   f(a + 0x2c), static_cast<std::int8_t>(load(a + 8, 1)), load(a + 9, 1) != 0);
  return n;
}

// ---- presentation ---------------------------------------------------------------------------------
namespace {
std::array<float, 3> ndc(std::uint32_t x, std::uint32_t y, Raw w) {
  return {static_cast<float>((static_cast<double>(x & 0xffff) / 16.0 - 2048.0) / 640.0),
          static_cast<float>(-(static_cast<double>(y & 0xffff) / 16.0 - 2048.0) / 224.0),
          static_cast<float>(to_double(w))};
}
Run &run_for(Presentation &p, const std::string &texture, std::uint32_t alpha1, bool linear, bool lines) {
  if (!p.runs.empty()) {
    Run &last = p.runs.back();
    if (last.texture == texture && last.alpha1 == alpha1 && last.linear == linear && last.lines == lines)
      return last;
  }
  p.runs.push_back(Run{texture, alpha1, linear, lines, {}, {}, {}});
  return p.runs.back();
}
void put(Run &r, const std::array<float, 3> &v, const std::array<std::uint8_t, 4> &c,
         std::array<float, 2> uv) {
  r.vertices.push_back(v);
  r.colors.push_back(c);
  r.uvs.push_back(uv);
}
void refuse(Presentation &p, const std::string &why) {
  if (std::find(p.refusals.begin(), p.refusals.end(), why) == p.refusals.end()) p.refusals.push_back(why);
}
} // namespace

Presentation present(const std::vector<render::Packet> &packets, const render::TextureDims &glow_dims) {
  Presentation out;
  for (const render::Packet &pk : packets) {
    if (pk.kind == render::PacketKind::Particles) {
      for (const render::ParticleDraw &d : pk.particles.draws) {
        if (d.slot == 0) continue;   // slot 0 / zero-filled cells: the invisible held definition
        if (d.prim == render::PRIM_LINE) {
          if (d.v[1].adc) continue;  // the head's clip test sets ADC on the tail
          Run &r = run_for(out, "", d.alpha1, false, true);
          put(r, ndc(d.v[0].x, d.v[0].y, d.v[0].clip_w), d.v[0].rgba, {0, 0});
          put(r, ndc(d.v[1].x, d.v[1].y, d.v[1].clip_w), d.v[1].rgba, {0, 0});
          continue;
        }
        if (d.v[1].adc) continue;
        if (d.v[0].v >= static_cast<std::uint32_t>(SHEET_HEIGHT * 16) ||
            d.v[1].v > static_cast<std::uint32_t>(SHEET_HEIGHT * 16)) {
          refuse(out, "particle tile below the 160 lines of particletex.tex (GS memory, U)");
          continue;
        }
        Run &r = run_for(out, PARTICLE_SHEET, d.alpha1, false, false);
        const auto a = ndc(d.v[0].x, d.v[0].y, d.v[1].clip_w), b = ndc(d.v[1].x, d.v[1].y, d.v[1].clip_w);
        const float u0 = d.v[0].u / 16.0f / SHEET_WIDTH, v0 = d.v[0].v / 16.0f / SHEET_HEIGHT;
        const float u1 = d.v[1].u / 16.0f / SHEET_WIDTH, v1 = d.v[1].v / 16.0f / SHEET_HEIGHT;
        const std::array<float, 3> tl = a, br = b, tr{b[0], a[1], b[2]}, bl{a[0], b[1], b[2]};
        put(r, tl, d.v[1].rgba, {u0, v0}); put(r, tr, d.v[1].rgba, {u1, v0}); put(r, br, d.v[1].rgba, {u1, v1});
        put(r, tl, d.v[1].rgba, {u0, v0}); put(r, br, d.v[1].rgba, {u1, v1}); put(r, bl, d.v[1].rgba, {u0, v1});
      }
    } else if (pk.kind == render::PacketKind::Trails) {
      for (const render::TrailDraw &t : pk.trails.trails) {
        Run &r = run_for(out, TRAIL_TEXTURE, t.alpha1, true, false);
        const auto &vs = t.vertices;
        for (std::size_t i = 2; i < vs.size(); ++i) {
          if (vs[i].adc) continue;   // a strip vertex with ADC does not draw its triangle
          for (std::size_t k = i - 2; k <= i; ++k)
            put(r, ndc(vs[k].x, vs[k].y, vs[k].clip_w), vs[k].rgba,
                {vs[k].u / 16.0f / TRAIL_WIDTH, vs[k].v / 16.0f / TRAIL_HEIGHT});
        }
      }
    } else {
      const render::GlowPacket &g = pk.glow;
      const GlowTexture *tex = glow_texture(g.texture);
      const auto dims = glow_dims.find(g.texture);
      if (!tex || dims == glow_dims.end()) {
        if (!g.sprites.empty()) refuse(out, "glow texture word without a loaded texture");
        continue;
      }
      Run &r = run_for(out, tex->name, g.alpha1, true, false);
      const float tw = static_cast<float>(dims->second.first), th = static_cast<float>(dims->second.second);
      for (const render::GlowSprite &s : g.sprites) {
        std::array<std::uint8_t, 4> rgba{};
        std::array<float, 2> uv{};
        std::vector<std::pair<std::array<float, 3>, std::array<float, 2>>> corners;
        std::vector<bool> kick;
        for (std::size_t e = s.first_event; e < s.first_event + s.event_count && e < g.events.size(); ++e) {
          const render::AdWrite &w = g.events[e];
          if (w.reg == render::REG_RGBAQ) {
            for (std::size_t k = 0; k < 4; ++k) rgba[k] = static_cast<std::uint8_t>(w.value >> (8 * k));
          } else if (w.reg == render::REG_UV) {
            uv = {static_cast<float>(w.value & 0x3fff) / 16.0f / tw,
                  static_cast<float>((w.value >> 16) & 0x3fff) / 16.0f / th};
          } else if (w.reg == render::REG_XYZ2 || w.reg == render::REG_XYZ3) {
            corners.push_back({ndc(static_cast<std::uint32_t>(w.value), static_cast<std::uint32_t>(w.value >> 16),
                                   s.screen.depth), uv});
            kick.push_back(w.reg == render::REG_XYZ2);
          }
        }
        if (g.prim == render::PRIM_GLOW && corners.size() == 2) {
          const auto &a = corners[0], &b = corners[1];
          const std::pair<std::array<float, 3>, std::array<float, 2>> tr{{b.first[0], a.first[1], b.first[2]}, {b.second[0], a.second[1]}},
              bl{{a.first[0], b.first[1], b.first[2]}, {a.second[0], b.second[1]}};
          for (const auto *v : {&a, &tr, &b, &a, &b, &bl}) put(r, v->first, rgba, v->second);
        } else {
          for (std::size_t i = 2; i < corners.size(); ++i)
            if (kick[i])
              for (std::size_t k = i - 2; k <= i; ++k) put(r, corners[k].first, rgba, corners[k].second);
        }
      }
    }
  }
  return out;
}

Presentation present_decals(const decals::DecalStore &store, const Camera &camera) {
  Presentation out;
  const Mat4 m = render::particle_matrix(camera.view, camera.proj_scale);
  Run run{FLOOR_TEXTURE, 0x44, true, false, {}, {}, {}};
  auto project = [&](double x, double y, double z, std::array<float, 3> &v) {
    const core::TransformPoint q = core::mat4_transform_point(m, {to_raw(x), to_raw(y), to_raw(z)});
    const double w = to_double(q.w);
    if (!(w > 1.0)) return false;
    v = {static_cast<float>(to_double(q.point[0]) * 800.0 / w / 640.0),
         static_cast<float>(-to_double(q.point[1]) * 400.0 / w / 224.0), static_cast<float>(w)};
    return true;
  };
  for (const auto &cd : store.decals()) {
    const decals::Decal &d = *cd.second;
    const double x = d.x4 / 4.0, y = d.y4 / 4.0, z = d.z4 / 4.0, c = d.c4 / 4.0, s = d.s4 / 4.0;
    const std::array<std::array<double, 2>, 4> off{{{c, s}, {s, -c}, {-c, -s}, {-s, c}}};
    const auto type = static_cast<std::size_t>(d.kind);
    if (type >= decal_rects.size()) continue;
    const TextureRect r = decal_rects[type];
    const std::array<std::array<float, 2>, 4> uv{{{(r.x0 + 0.5f) / FLOOR_WIDTH, (r.y0 + 0.5f) / FLOOR_HEIGHT},
                                                  {(r.x1 - 0.5f) / FLOOR_WIDTH, (r.y0 + 0.5f) / FLOOR_HEIGHT},
                                                  {(r.x1 - 0.5f) / FLOOR_WIDTH, (r.y1 - 0.5f) / FLOOR_HEIGHT},
                                                  {(r.x0 + 0.5f) / FLOOR_WIDTH, (r.y1 - 0.5f) / FLOOR_HEIGHT}}};
    std::array<std::array<float, 3>, 4> v{};
    bool ok = true;
    for (std::size_t k = 0; k < 4; ++k) ok = project(x + off[k][0], y + off[k][1], z, v[k]) && ok;
    if (!ok) continue;
    const std::array<std::uint8_t, 4> rgba{static_cast<std::uint8_t>(d.rgb[0]), static_cast<std::uint8_t>(d.rgb[1]),
                                           static_cast<std::uint8_t>(d.rgb[2]), 0x80};
    for (std::size_t k : {0u, 1u, 2u, 0u, 2u, 3u}) put(run, v[k], rgba, uv[k]);
  }
  if (!run.vertices.empty()) out.runs.push_back(std::move(run));
  return out;
}

} // namespace champions::rta::fx::sim
