#include "champions/combat_runtime.hpp"

namespace champions::simulation::rta::combat_runtime {
Services with_terrain(Services base, world_queries::Level &level) {
  base.raycast = [&level](const Vec3 &origin, const Vec3 &direction,
                          Raw flags, Raw max_t) {
    return world_queries::terrain_raycast(level, max_t, origin, direction,
                                          flags);
  };
  base.slide = [&level](const Vec3 &position, const Vec3 &delta,
                        Raw radius, Raw mask) {
    const auto result = world_queries::world_slide_move(
        level, radius, position, delta, mask);
    return Slide{result.position, Raw(result.hit),
                 result.normal.value_or(Vec3{})};
  };
  base.height_below = [&level](const Vec3 &point, Raw radius) {
    return world_queries::terrain_height_below(level, radius, point);
  };
  base.terrain_height = [&level](const Vec3 &point, Raw radius) {
    return world_queries::terrain_height(level, radius, point);
  };
  base.terrain_blocked = [&level](const Vec3 &point, Raw radius) {
    return world_queries::terrain_test_flag1(level, radius, point);
  };
  base.height_at_point = [&level](const Vec3 &point) {
    return world_queries::terrain_height_at_point(level, point);
  };
  return base;
}
} // namespace champions::simulation::rta::combat_runtime
