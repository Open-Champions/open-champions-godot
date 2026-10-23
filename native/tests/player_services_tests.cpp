#include "champions/archive.hpp"
#include "champions/player_services.hpp"
#include "champions/terrain_file.hpp"
#include "champions/world_geometry.hpp"
#include "champions/real_world.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
namespace R = champions::simulation::rta;
namespace W = R::world_queries;
namespace P = R::player_services;
namespace C = R::combat_runtime;
namespace {
unsigned checks = 0;
void check(bool ok, const char *reason) {
  ++checks;
  if (!ok) throw std::runtime_error(reason);
}
template <class F> void refuses(F call, const char *reason) {
  bool found = false;
  try { call(); } catch (const C::Unsupported &) { found = true; }
  check(found, reason);
}
template <class F> void refuses_with(F call, const char *label, const char *reason) {
  bool found = false;
  try { call(); } catch (const C::Unsupported &e) {
    found = std::string(e.what()).find(label) != std::string::npos;
  }
  check(found, reason);
}
R::Raw f(int n) { return R::from_int(std::uint32_t(n)); }
W::LevelData authored_ground() {
  W::LevelData d;
  d.tg_w = d.tg_h = 1;
  d.grid = {{0}};
  W::HeightField h;
  h.w = h.h = 1;
  h.lo = h.hi = 13;
  h.samples = {13 * 16};
  d.heightfields.push_back(h);
  d.elements.push_back(W::make_element(0, -100, -100, 100, 100,
                                      0, 0, 0, 0, 0, 32767, 0));
  return d;
}
void synthetic() {
  W::Level level(authored_ground());
  P::Bundle bundle(level);
  auto s = bundle.services();
  const R::Vec3 at{0, 0, f(13)};
  check(s.height_below(at, f(4)) == f(13), "height below from loaded level");
  check(s.terrain_height(at, f(4)) == f(13), "terrain height from loaded level");
  check(s.height_at_point(at) == f(13), "height at point from loaded level");
  check(s.terrain_blocked(at, f(4)) == 0, "authored ground has no blocking flag");
  auto slide = s.slide({0, 0, f(100)}, {f(4), 0, 0}, 3, f(2));
  check(slide.hit == 0 && slide.position == R::Vec3{f(4), 0, f(100)},
        "authored clear slide");
  refuses([&] { (void)s.water_test(at, f(4)); }, "unbound water refuses");
  refuses([&] { (void)s.water_disturb(at, f(1), f(4)); }, "unbound ripple refuses");
  bundle.bind_water_chunks({{{f(100), f(100), f(10)},
                             {f(120), f(120), f(20)}}});
  check(s.water_test(at, f(4)) == 0 && s.water_disturb(at, f(1), f(4)) == 0,
        "loaded dry point remains dry");
  check(s.water_test({f(110), f(110), f(12)}, f(4)) == 1,
        "loaded water chunk overlaps documented query box");
  check(s.water_test({f(95), f(110), f(12)}, f(4)) == 1 &&
            s.water_disturb({f(95), f(110), f(12)}, f(1), f(4)) == 0,
        "dry disturb uses its unpadded query despite a padded water-test hit");
  refuses([&] { (void)s.water_height({f(110), f(110), f(12)}, f(4)); },
          "ordered water height refuses without runtime tree");
  refuses([&] { (void)s.water_disturb({f(110), f(110), f(12)}, f(1), f(4)); },
          "wet ripple without runtime grid refuses");
  R::creature_water::Memory memory;
  memory.poke(0x1004, 4, 0x2000); // world root
  memory.poke(0x1024, 4, 0x3000); // chunk table
  memory.poke(0x2000, 2, 1); // one authored leaf
  memory.poke(0x2002, 2, 0);
  memory.poke(0x3000, 4, 0x5000); // payload
  memory.poke(0x3024, 4, 0x80); // water flag
  for (unsigned axis = 0; axis < 3; ++axis) {
    memory.poke(0x3008 + 4 * axis, 4, f(axis < 2 ? -192 : 8));
    memory.poke(0x3014 + 4 * axis, 4, f(axis < 2 ? 192 : 8));
  }
  memory.poke(0x5008, 4, 0); // real-level image has no runtime ripple grid
  R::creature_water::Service water_service(memory);
  bundle.bind_water(water_service, 0x1000);
  check(s.water_test({0, 0, 0}, f(12)) == 1,
        "bound water service queries authored tree");
  check(s.water_height({0, 0, 0}, f(12)) == f(8),
        "bound water height reads first queried chunk");
  check(s.water_disturb({f(400), f(400), 0}, f(100), f(12)) == 0,
        "bound service keeps a dry disturbance dry without a grid owner");
  refuses_with([&] { (void)s.water_disturb({0, 0, 0}, f(100), f(12)); },
               "wet runtime ripple grid is not bound",
               "wet service refuses when runtime grid ownership is unverified");
  check(water_service.writes().empty(), "wet refusal occurs before ripple writes");
  bundle.bind_water(water_service, 0x1000, true);
  refuses_with([&] { (void)s.water_disturb({0, 0, 0}, f(100), f(12)); },
               "wet runtime ripple grid is missing",
               "declared grid ownership still refuses a null grid pointer");
  memory.poke(0x5008, 4, 0x6000); // authored runtime grid supplied by this test
  check(s.water_disturb({0, 0, 0}, f(100), f(12)) == 1,
        "explicit runtime grid owner permits the authored disturbance");
  // terrain_raycast 0x18a5c0 now backs the service: maxT 0 is the documented
  // empty ray (-1.0), and a downward vertical ray returns O.z - ground.
  check(s.raycast(at, at, 0, 0) == R::MINUS_ONE, "empty ray reports no hit");
  const R::Vec3 above{at[0], at[1], f(30)};
  check(s.raycast(above, {0, 0, R::MINUS_ONE}, 0, R::ONE) == R::sub(f(30), s.height_at_point(above)),
        "vertical ray returns the distance to the loaded ground (world-queries 9.1)");
  refuses([&] { (void)s.camera_yaw(0); }, "unbound camera refuses");
  P::Camera known;
  known.yaw = 0x4000;
  known.position = {0, 0, f(20)};
  known.side_normals = {{{R::ONE, 0, 0}, {R::MINUS_ONE, 0, 0},
                         {0, R::ONE, 0}, {0, R::MINUS_ONE, 0}}};
  bundle.set_camera(2, known);
  bundle.set_local_slot(2);
  const auto planes = s.screen_planes(at);
  check(s.camera_yaw(2) == 0x4000 && planes[0].point == R::Vec3{f(60), 0, f(13)} &&
            planes[1].point == R::Vec3{f(-60), 0, f(13)} &&
            planes[2].point == R::Vec3{0, f(60), f(13)} &&
            planes[3].point == R::Vec3{0, f(-60), f(13)},
        "known host camera flattened at player height with documented inset");
  refuses_with([&] { (void)s.camera_flag(2); }, "live mode is not bound",
               "host yaw and frustum do not imply a live follow mode");
  known.flag = 1;
  bundle.set_camera(2, known);
  check(s.camera_flag(2) == 1, "observed non-following mode is supplied");
  known.flag = 0;
  bundle.set_camera(2, known);
  check(s.camera_flag(2) == 0, "observed following mode is supplied");
  refuses([&] { (void)s.camera_yaw(1); }, "other machine has no implicit camera");

  R::creature_anim::Runtime runtime;
  auto state = runtime.add_state();
  R::creature_anim::Clip clip;
  clip.length = 60;
  const auto id = runtime.add_clip(clip);
  bundle.bind_animation(runtime, [id](P::Address, R::Raw word) {
    return word == 0x1234 ? id : R::creature_anim::ClipId(0);
  });
  bundle.bind_actor(0x1000, state);
  refuses([&] { (void)bundle.animation_fields(0x1000); }, "unbound shared player fields refuse");
  R::Raw weapon_class = 0xfe, action_state = 2;
  bundle.bind_player_reader([&](P::Address address, unsigned size) -> std::uint64_t {
    const P::Address record = 0x4d8af0 + 0x4c30;
    if (address == 0x1000 + 0x3c8 && size == 4) return 1;
    if (address == record + 0x43d8 && size == 4) return 0x1000;
    if (address == record + 0x4a86 && size == 1) return weapon_class;
    if (address == 0x1000 + 0x58c && size == 4) return action_state;
    throw std::runtime_error("unexpected authored player field read");
  });
  auto fields = bundle.animation_fields(0x1000);
  check(fields.weapon_class == -2 && fields.action_state == 2,
        "signed weapon class and action state read from audited owners");
  weapon_class = 4; action_state = 8;
  fields = bundle.animation_fields(0x1000);
  check(fields.weapon_class == 4 && fields.action_state == 8,
        "equip and action transitions are observed live");
  refuses([&] { (void)s.animation_root(0x1000); }, "root requires an advance");
  refuses([&] { s.animation_start(0x1000, 0x5678, 1, 0, 0); }, "unknown clip refuses");
  s.animation_start(0x1000, 0x1234, 1, 0, 0);
  check(runtime.state(state).count == 1, "start reaches ported animation runtime");
  (void)s.animation_advance(0x1000, R::ONE);
  const auto track = runtime.state(state).tracks[0];
  const auto step = R::mul(R::ONE, 0x3f7fffff);
  check(runtime.track(track).time == step,
        "first tick advances by the ported animation step");
  (void)s.animation_advance(0x1000, R::ONE);
  check(runtime.track(track).time == R::add(step, step),
        "second tick advances the same owned track");
  check(s.animation_root(0x1000) == std::array<R::Raw, 2>{0, 0},
        "authored no-root clip gives measured zero displacement");
  const auto replacement = runtime.add_state();
  bundle.bind_actor(0x1000, replacement);
  refuses_with([&] { (void)s.animation_root(0x1000); }, "actor has not advanced",
               "rebound actor cannot reuse the old state's root displacement");
  s.animation_start(0x1000, 0x1234, 1, 0, 0);
  (void)s.animation_advance(0x1000, R::ONE);
  check(s.animation_root(0x1000) == std::array<R::Raw, 2>{0, 0},
        "newly bound actor exposes only its own new advance");
}
void real_probe(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("real-level probe input unavailable");
  const std::vector<std::uint8_t> container((std::istreambuf_iterator<char>(file)), {});
  std::vector<std::uint8_t> bytes;
  const champions::Bytes input{container.data(), container.size()};
  for (const auto &entry : champions::readLmp(input)) {
    if (!champions::hasExtension(entry.name, ".world")) continue;
    const auto payload = champions::payload(input, entry);
    bytes.assign(payload.data, payload.data + payload.size);
    break;
  }
  if (bytes.empty()) throw std::runtime_error("real-level probe has no WORLD payload");
  W::Level level(R::terrain_file::decode(bytes));
  P::Bundle bundle(level);
  const auto geometry = champions::presentation::rta::world_geometry::decode(bytes);
  champions::real_world::WaterImage water(bytes, geometry);
  const auto water_count = geometry.water.size();
  bundle.bind_water(water.service(), water.world());
  auto s = bundle.services();
  unsigned queried = 0, floors = 0, refusals = 0;
  for (const auto &e : level.data().elements) {
    if (queried == 16) break;
    if (e.x < std::numeric_limits<std::int32_t>::min() ||
        e.x > std::numeric_limits<std::int32_t>::max() ||
        e.y < std::numeric_limits<std::int32_t>::min() ||
        e.y > std::numeric_limits<std::int32_t>::max() ||
        e.z < std::numeric_limits<std::int32_t>::min() ||
        e.z > std::numeric_limits<std::int32_t>::max()) continue;
    const R::Vec3 p{f(int(e.x)), f(int(e.y)), f(int(e.z))};
    auto height = s.terrain_height(p, f(12));
    ++queried;
    if (height != W::NO_FLOOR) ++floors;
  }
  try { (void)s.water_test({}, f(12)); } catch (const C::Unsupported &) { ++refusals; }
  std::cout << "real-level: loaded=1 queried=" << queried << " floors=" << floors
            << " water_chunks=" << water_count
            << " callback_refusals=" << refusals << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    R::ScopedFpu fpu(R::Fpu{R::Fpu::Model::Ps2, false, false});
    if (argc == 2) real_probe(argv[1]);
    else synthetic();
    std::cout << "player-services: checks=" << checks << " failures=0\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "player-services: failure: " << e.what() << '\n';
    return 1;
  }
}
