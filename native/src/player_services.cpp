#include "champions/player_services.hpp"

namespace champions::simulation::rta::player_services {
namespace {
[[noreturn]] void missing(const char *what) {
  throw combat_runtime::Unsupported(what);
}
} // namespace
void Bundle::set_camera(Raw slot, Camera camera) {
  cameras_[slot] = std::move(camera);
}
void Bundle::set_camera_flag(Raw slot, Raw flag) {
  const auto it = cameras_.find(slot);
  if (it == cameras_.end()) missing("player camera flag: machine camera is not bound");
  it->second.flag = flag;
}
void Bundle::bind_animation(creature_anim::Runtime &runtime,
                            std::function<creature_anim::ClipId(Address, Raw)> clips) {
  actors_.clear();
  motion_.clear();
  animation_ = &runtime;
  clips_ = std::move(clips);
}
void Bundle::bind_actor(Address actor, creature_anim::StateId state) {
  if (!animation_)
    missing("player animation actor: runtime is not bound");
  (void)animation_->state(state); // reject stale state at the binding boundary
  motion_.erase(actor); // a new binding has no advance for this actor
  actors_[actor] = state;
}
void Bundle::bind_player_reader(std::function<std::uint64_t(Address, unsigned)> read) {
  player_read_ = std::move(read);
}
AnimationFields Bundle::animation_fields(Address actor) const {
  if (!player_read_) missing("player animation fields: shared World reader is not bound");
  // cda95dc: P+0x3c8 selects R; R+0x43d8 points back to P.
  const auto slot = player_read_(actor + 0x3c8, 4);
  if (slot >= 4) missing("player animation fields: invalid player slot");
  const Address record = Address(0x4d8af0 + slot * 0x4c30);
  if (player_read_(record + 0x43d8, 4) != actor)
    missing("player animation fields: record does not own actor");
  return {static_cast<std::int8_t>(player_read_(record + 0x4a86, 1)),
          static_cast<std::int32_t>(player_read_(actor + 0x58c, 4))};
}
void Bundle::bind_water(creature_water::Service &service,
                        creature_water::Address world, bool runtime_grids_bound) {
  water_ = &service;
  water_world_ = world;
  water_grids_bound_ = runtime_grids_bound;
}
void Bundle::bind_water_chunks(std::vector<WaterChunk> chunks) {
  water_chunks_ = std::move(chunks);
  water_chunks_bound_ = true;
}
bool Bundle::touches_water(const Vec3 &p, Raw radius, bool padded) const {
  const Raw pad = padded ? ONE : 0;
  const Vec3 lo{sub(sub(p[0], radius), pad), sub(sub(p[1], radius), pad),
                sub(p[2], ONE)};
  const Vec3 hi{add(add(p[0], radius), pad), add(add(p[1], radius), pad),
                add(p[2], from_int(60))};
  for (const auto &chunk : water_chunks_) {
    bool hit = true;
    for (unsigned axis = 0; axis < 3; ++axis)
      if (lt(hi[axis], chunk.minimum[axis]) || lt(chunk.maximum[axis], lo[axis])) {
        hit = false;
        break;
      }
    if (hit) return true;
  }
  return false;
}
combat_runtime::Services Bundle::services(combat_runtime::Services s) {
  s.slide = [this](const Vec3 &p, const Vec3 &d, Raw mask, Raw radius) {
    const auto r = world_queries::world_slide_move(level_, radius, p, d, mask);
    combat_runtime::Slide out;
    out.position = r.position;
    out.hit = Raw(r.hit);
    if (r.normal) out.normal = *r.normal;
    return out;
  };
  s.height_below = [this](const Vec3 &p, Raw radius) {
    return world_queries::terrain_height_below(level_, radius, p);
  };
  s.terrain_height = [this](const Vec3 &p, Raw radius) {
    return world_queries::terrain_height(level_, radius, p);
  };
  s.height_at_point = [this](const Vec3 &p) {
    return world_queries::terrain_height_at_point(level_, p);
  };
  s.terrain_blocked = [this](const Vec3 &p, Raw radius) {
    return world_queries::terrain_test_flag1(level_, radius, p);
  };
  // (origin, direction, terrain flag mask, maxT): terrain_raycast 0x18a5c0
  // (world-queries.md 9.1). world_raycast's object-circle part is not ported,
  // so objects do not block these rays.
  s.raycast = [this](const Vec3 &origin, const Vec3 &direction, Raw mask, Raw max_t) -> Raw {
    return world_queries::terrain_raycast(level_, max_t, origin, direction, mask);
  };
  s.water_test = [this](const Vec3 &p, Raw radius) {
    if (water_) return Raw(water_->water_test(radius, water_world_, p));
    if (water_chunks_bound_) return Raw(touches_water(p, radius, true));
    missing("player water test: loaded water chunks are not bound");
  };
  s.water_disturb = [this](const Vec3 &p, Raw strength, Raw radius) {
    if (water_) {
      // Query before any service write. A wet disturbance needs an owned grid
      // for every touched chunk; the real-level WORLD image has none.
      if (water_->memory().load(0x44fcec, 1)) return Raw(0);
      if (!lt(0, radius)) missing("player water disturb: radius must be positive");
      const Vec3 lo{sub(p[0], radius), sub(p[1], radius), sub(p[2], ONE)};
      const Vec3 hi{add(p[0], radius), add(p[1], radius), add(p[2], from_int(60))};
      const auto chunks = water_->query(water_world_, lo, hi, 16);
      if (chunks.empty()) return Raw(0);
      if (!water_grids_bound_)
        missing("player water disturb: wet runtime ripple grid is not bound");
      for (const auto chunk : chunks) {
        const auto payload = creature_water::Address(water_->memory().load(chunk, 4));
        if (!payload || !water_->memory().load(payload + 8, 4))
          missing("player water disturb: wet runtime ripple grid is missing");
      }
      return Raw(water_->water_disturb(strength, radius, water_world_, p));
    }
    if (!water_chunks_bound_) missing("player water disturb: loaded water chunks are not bound");
    if (!lt(0, radius)) missing("player water disturb: radius must be positive");
    if (!touches_water(p, radius, false)) return Raw(0);
    missing("player water disturb: wet runtime ripple grid is not bound");
  };
  s.water_height = [this](const Vec3 &p, Raw radius) -> Raw {
    if (!water_) missing("player water height: ordered first-chunk water service is not bound");
    const Vec3 lo{sub(sub(p[0], radius), ONE), sub(sub(p[1], radius), ONE),
                  sub(p[2], ONE)};
    const Vec3 hi{add(add(p[0], radius), ONE), add(add(p[1], radius), ONE),
                  add(p[2], from_int(60))};
    const auto chunks = water_->query(water_world_, lo, hi, 2);
    if (chunks.empty()) missing("player water height: query has no first chunk");
    return Raw(water_->memory().load(chunks.front() + 0x10, 4));
  };
  s.camera_yaw = [this](Raw slot) {
    const auto it = cameras_.find(slot);
    if (it == cameras_.end()) missing("player camera yaw: machine camera is not bound");
    return it->second.yaw;
  };
  s.camera_flag = [this](Raw slot) {
    const auto it = cameras_.find(slot);
    if (it == cameras_.end()) missing("player camera flag: machine camera is not bound");
    if (!it->second.flag) missing("player camera flag: live mode is not bound");
    return *it->second.flag;
  };
  s.screen_planes = [this](const Vec3 &player) {
    // Only the local machine has a drawable screen; a remote clamp must use
    // the caller's explicitly selected host camera, not a fixture plane.
    const auto it = cameras_.find(local_slot_);
    if (it == cameras_.end()) missing("player screen frustum: local camera is not bound");
    std::array<Plane, 4> planes{};
    const auto &camera = it->second;
    for (unsigned i = 0; i < 4; ++i) {
      const auto &n = camera.side_normals[i];
      const Raw flat = sqrt(sub(ONE, mul(n[2], n[2])));
      if (!lt(0, flat)) missing("player screen frustum: degenerate side normal");
      const Vec3 horizontal{div(n[0], flat), div(n[1], flat), 0};
      const Raw distance = add(div(mul(sub(camera.position[2], player[2]), n[2]), flat),
                               from_int(60));
      planes[i].normal = horizontal;
      planes[i].point = {add(camera.position[0], mul(horizontal[0], distance)),
                         add(camera.position[1], mul(horizontal[1], distance)), player[2]};
    }
    return planes;
  };
  s.animation_start = [this](Address actor, Raw clip, Raw flags, Raw blend, Raw out) {
    if (!animation_ || !clips_) missing("player animation start: runtime or resource resolver is not bound");
    const auto it = actors_.find(actor);
    if (it == actors_.end()) missing("player animation start: actor state is not bound");
    const auto id = clips_(actor, clip);
    if (!id) missing("player animation start: clip resource is unresolved");
    if (out)
      animation_->anim_start_blend_in_out(blend, out, it->second, id, flags);
    else
      animation_->anim_start(blend, it->second, id, flags);
  };
  s.animation_advance = [this](Address actor, Raw speed) {
    if (!animation_) missing("player animation advance: runtime is not bound");
    const auto it = actors_.find(actor);
    if (it == actors_.end()) missing("player animation advance: actor state is not bound");
    const auto motion = animation_->advance(speed, it->second, false);
    motion_[actor] = motion;
    return Raw(motion.events);
  };
  s.animation_root = [this](Address actor) -> std::array<Raw, 2> {
    const auto it = motion_.find(actor);
    if (it == motion_.end()) missing("player animation root motion: actor has not advanced");
    return {it->second.x, it->second.y};
  };
  return s;
}
} // namespace champions::simulation::rta::player_services
