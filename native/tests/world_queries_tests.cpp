#include "champions/world_queries.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace R = champions::simulation::rta;
namespace W = R::world_queries;
namespace {
unsigned checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <class E = W::Error, class F> void rejects(F f, const char *why) {
  bool refused = false;
  try {
    f();
  } catch (const E &) {
    refused = true;
  }
  check(refused, why);
}
R::Raw f(std::int32_t n) { return R::from_int(static_cast<std::uint32_t>(n)); }
W::LevelData flat(std::int16_t sample = 0) {
  W::LevelData d;
  d.tg_w = d.tg_h = 1;
  d.grid = {{0}};
  W::HeightField h;
  h.w = h.h = 1;
  h.lo = h.hi =
      sample >= 0 ? sample / 16 : -((-std::int32_t(sample) + 15) / 16);
  h.samples = {sample};
  h.offset = 123;
  d.heightfields.push_back(h);
  d.elements.push_back(
      W::make_element(77, -100, -100, 100, 100, 0, 0, 0, 0, 0, 32767, 0));
  return d;
}
void collection() {
  auto d = flat();
  d.stamp = 9;
  d.tg_w = d.tg_h = 2;
  d.grid = {{2, 0, 2}, {1}, {0}, {3}};
  for (unsigned i = 1; i < 4; ++i)
    d.elements.push_back(d.elements[0]);
  for (auto &e : d.elements) {
    e.min_x = e.min_y = -1000;
    e.max_x = e.max_y = 1000;
    e.f_min_x = e.f_min_y = f(-1000);
    e.f_max_x = e.f_max_y = f(1000);
  }
  d.elements[1].flags = 2;
  W::Level level(d);
  check(W::collect_elements(level, f(-20), f(-20), f(320), f(320), 3) ==
            std::vector<std::size_t>{2, 0},
        "collection uses rows/cell order, deduplicates and stops at limit "
        "minus one");
  check(level.data().stamp == 9 && level.data().elements[2].stamp == 9 &&
            level.data().elements[0].stamp == 9 &&
            level.data().elements[3].stamp == 0,
        "collection does not advance caller stamp or stamp unvisited elements");
  check(W::collect_elements(level, f(-20), f(-20), f(320), f(320), 64) ==
            std::vector<std::size_t>{3},
        "repeat same stamp skips prior elements and ignored flag2");
  check(level.data().elements[0].index == 77 &&
            level.data().elements[2].index == 77,
        "duplicate index labels do not replace storage identity");
  const auto prior = level.snapshot();
  check(
      W::collect_elements(level, f(-20), f(-20), f(320), f(320), 64).empty(),
      "complete repeated collection remains empty until caller advances stamp");
  level.next_stamp();
  check(
      level.data().stamp == 10 &&
          W::collect_elements(level, f(-20), f(-20), f(320), f(320), 64) ==
              std::vector<std::size_t>{2, 0, 3},
      "caller-owned stamp advance makes original ordered identities available");
  check(prior.data.stamp == 9 && prior.data.elements[0].stamp == 9 &&
            prior.data.elements[3].stamp == 9,
        "caller stamp advance leaves the earlier owned snapshot unchanged");
  auto cached = flat();
  cached.stamp = 1;
  cached.elements[0].f_min_x = f(10);
  W::Level cache(cached);
  check(W::collect_elements(cache, 0, 0, 0, 0, 64).empty(),
        "cached rectangle is observed separately from integer metadata");
  auto negative = flat();
  negative.stamp = 1;
  negative.grid = {{-1}};
  negative.elements[0].hf = -1;
  W::Level back(negative);
  check(W::collect_elements(back, 0, 0, 0, 0, 64) ==
            std::vector<std::size_t>{0},
        "negative supplied references retain checked Python indexing");
  auto wrap = flat();
  wrap.stamp = UINT32_MAX;
  W::Level direct_roll(wrap);
  const auto before_roll = direct_roll.snapshot();
  direct_roll.next_stamp();
  check(
      direct_roll.data().stamp == 0 &&
          W::collect_elements(direct_roll, 0, 0, 0, 0, 64).empty() &&
          direct_roll.data().elements[0].stamp == 0,
      "explicit rollover preserves old zero stamps instead of clearing visits");
  direct_roll.next_stamp();
  check(W::collect_elements(direct_roll, 0, 0, 0, 0, 64) ==
                std::vector<std::size_t>{0} &&
            before_roll.data.stamp == UINT32_MAX,
        "explicit post-rollover advance restores collection without changing "
        "snapshot");
  W::Level roll(wrap);
  check(W::terrain_height(roll, f(4), {0, 0, 0}) == W::NO_FLOOR &&
            roll.data().stamp == 0,
        "stamp rollover preserves equal-zero element suppression");
  check(W::terrain_height(roll, f(4), {0, 0, 0}) == 0 && roll.data().stamp == 1,
        "next stamp makes suppressed element visible again");
}
void heights_and_pieces() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    R::ScopedFpu context(
        R::Fpu(static_cast<R::Fpu::Model>(cfg / 4), cfg & 2, cfg & 1));
    W::Level empty;
    check(W::terrain_height(empty, f(4), {0, 0, 0}) == W::NO_FLOOR &&
              W::terrain_height_at_point(empty, {0, 0, 0}) == 0,
          "height and point-query absent-floor sentinels are distinct");
    W::Level zero(flat());
    check(W::terrain_height(zero, 0, {0, 0, 0}) == W::NO_FLOOR,
          "strict zero-radius sample disc is empty");
    W::Level ground(flat());
    check(W::terrain_height(ground, f(4), {0, 0, 0}) == 0,
          "authored flat sample returns literalzero in every FPU");
    auto bridge = flat(13 * 16);
    W::Level high(bridge);
    check(W::terrain_height_below(high, f(4), {0, 0, 0}) == W::NO_FLOOR &&
              W::terrain_height_below(high, f(4), {0, 0, f(1)}) == f(13),
          "heightBelow rejects above12 and accepts exact12 boundary");
    auto mixed = flat();
    mixed.heightfields[0].w = 2;
    mixed.heightfields[0].samples = {16, 160};
    mixed.heightfields[0].hi = 0;
    W::Level lowmax(mixed);
    check(W::terrain_height(lowmax, f(8), {0, 0, 0}) == R::ONE,
          "stored inconsistent maximum preserves first-sample early exit");
    auto flagged = flat(1);
    flagged.elements.push_back(flagged.elements[0]);
    flagged.grid = {{0, 1}};
    flagged.pieces = {0, 1};
    flagged.pg_x0 = -3;
    flagged.pg_y0 = 7;
    flagged.pg_w = 2;
    flagged.pg_h = 1;
    flagged.path_cells = {0xabcdef01, 0xffffffff};
    W::Level pieces(flagged);
    check(pieces.data().heightfields[0].samples[0] == 1 &&
              pieces.data().heightfields[0].hi == 0,
          "post-fixup input is never silently rewritten for pieces");
    auto initial = pieces.snapshot();
    pieces.set_piece_collision(0, false);
    pieces.set_piece_height(1, 0x40700000);
    check(pieces.data().elements[0].flags == 2 &&
              pieces.data().elements[1].flags == 0 &&
              pieces.get_piece_height(1) == 3,
          "piece collision and truncated height affect selected element only");
    check(pieces.data().elements[0].hf == pieces.data().elements[1].hf &&
              pieces.data().heightfields.size() == 1,
          "shared field alias remains one explicit registry identity");
    check(W::terrain_test_flag1(pieces, f(4), {0, 0, 0}) == R::ONE,
          "second aliased element still exposes its sample flags");
    pieces.set_piece_collision(0, true);
    pieces.set_piece_height(-1, R::FMAX);
    pieces.set_piece_collision(-1, false);
    check(pieces.get_piece_height(-1) == 0 &&
              pieces.data().elements[0].flags == 0,
          "negative piece sentinel is a no-op with zero get result");
    check(initial.data.elements[0].flags == 0 &&
              initial.data.elements[1].z == 0 &&
              initial.data.path_cells == flagged.path_cells,
          "earlier snapshot owns all elements and uninterpreted path state");
    initial.data.heightfields[0].samples[0] = 999;
    check(pieces.data().heightfields[0].samples[0] == 1,
          "snapshot sample mutation cannot alter live shared field");
    auto copied = pieces;
    copied.set_piece_height(1, f(8));
    check(pieces.get_piece_height(1) == 3 && copied.get_piece_height(1) == 8,
          "Level copy owns independent mutable state");
  }
}
void movement() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    R::ScopedFpu context(
        R::Fpu(static_cast<R::Fpu::Model>(cfg / 4), cfg & 2, cfg & 1));
    W::Level level(flat());
    auto clear =
        W::terrain_sweep_circle(level, f(2), {0, 0, f(100)}, {f(4), 0, 0}, 3);
    check(clear.time == R::ONE && !clear.normal,
          "clear sweep returns one without fabricated normal");
    auto slide =
        W::world_slide_move(level, f(2), {0, 0, f(100)}, {f(4), 0, 0}, 3);
    check(slide.hit == 0 && slide.position == W::Vec3{f(4), 0, f(100)} &&
              !slide.normal,
          "unblocked slide advances exact step and retains absent normal");
    auto grounded = W::ground_step(level, f(4), {0, 0, 0}, 0);
    check(
        grounded.position == W::Vec3{0, 0, 0} && grounded.vz == 0 &&
            grounded.landed == 1,
        "literal gravity below flat ground snaps and clears falling velocity");
    auto rising = W::ground_step(level, f(4), {0, 0, f(-1)}, f(20));
    check(rising.position[2] == 0 && rising.vz != 0 && rising.landed == 0,
          "ground snap while rising does not mark landing or zero upward "
          "velocity");
    auto stand = W::ground_step(level, f(4), {0, 0, 0}, 0, f(5));
    check(stand.position[2] == f(5) && stand.landed == 1,
          "explicit standing height wins over terrain height");
    auto moved =
        W::object_move_isolated(level, f(4), {0, 0, 0}, {0, 0, 0}, 0, 0x20);
    check(moved.position == W::Vec3{0, 0, 0} && moved.vz == 0 &&
              moved.bits == 4,
          "stationary isolated mover still applies gravity and landing");
    auto blocking = flat(1);
    blocking.elements[0].flags = 4;
    W::Level blocked(blocking);
    auto overlap =
        W::terrain_sweep_circle(blocked, f(2), {0, 0, 0}, {f(4), 0, 0}, 1);
    check(R::lt(overlap.time, 0) && overlap.normal.has_value(),
          "starting in blocking sample keeps negative contact time rather than "
          "clamping");
    auto without = W::terrain_sweep_circle(blocked, f(2), {0, 0, 0},
                                           {f(4), 0, 0}, 1, false);
    check(without.time == overlap.time && !without.normal,
          "normal request changes optional write only, not contact time");
  }
  check(W::mover_mask(0) == 1 && W::mover_mask(0x20) == 3 &&
            W::mover_mask(0x40) == 5 && W::mover_mask(0x60) == 5,
        "player mask takes precedence over creature flag; unrelated bits "
        "ignored");
}
void raw_rotation() {
  auto d = flat();
  d.heightfields[0].w = 2;
  d.heightfields[0].samples = {16, 32};
  d.heightfields[0].hi = 2;
  {
    R::ScopedFpu scope(R::Fpu(R::Fpu::Model::Ps2, false, false));
    W::Level truncated(d);
    check(W::terrain_height_at_point(truncated, {0x40800000, 0, 0}) == R::ONE,
          "truncated signed16 rotation shifts exactworld4 to prior sample");
  }
  {
    R::ScopedFpu scope(R::Fpu(R::Fpu::Model::Nearest, false, false));
    W::Level nearest(d);
    check(W::terrain_height_at_point(nearest, {0x40800000, 0, 0}) == R::TWO,
          "nearest rotation reaches next sample, proving selected FPU affects "
          "geometry");
  }
}
void empty_ranges_and_signed_masks() {
  W::LevelData d;
  d.tg_w = 0;
  d.tg_h = INT32_MAX;
  W::Level empty(d);
  check(W::terrain_height(empty, R::ONE, {0, 0, 0}) == W::NO_FLOOR &&
            empty.data().stamp == 1,
        "zero-width tall grid completes empty query and advances stamp once");
  auto field = flat();
  field.heightfields[0].w = 0;
  field.heightfields[0].h = INT32_MAX;
  field.heightfields[0].samples.clear();
  W::Level no_samples(field);
  check(W::terrain_height(no_samples, R::ONE, {0, 0, 0}) == W::NO_FLOOR,
        "empty clipped sample range skips huge outer rows");
  auto negative = flat(-16);
  negative.elements[0].flags = 4;
  W::Level high_mask(negative);
  auto hit = W::terrain_sweep_circle(high_mask, f(2), {0, 0, 0}, {f(4), 0, 0},
                                     0x80000000);
  auto clear =
      W::terrain_sweep_circle(high_mask, f(2), {0, 0, 0}, {f(4), 0, 0}, 0);
  check(R::lt(hit.time, 0) && clear.time == R::ONE,
        "signed packed negative sample retains high mask bits before blocking "
        "test");
}
void piece_mask_and_nested_budget() {
  R::ScopedFpu context(R::Fpu(R::Fpu::Model::Ps2, false, false));
  auto flags = flat();
  flags.elements[0].flags = 0xffff0006;
  flags.pieces = {0};
  W::Level piece(flags);
  const auto before = piece.snapshot();
  // The published piece setter uses 0xfffd, not an unbounded complement of 2.
  piece.set_piece_collision(0, true);
  check(piece.data().elements[0].flags == 4,
        "enabling piece collision clears high bits with exact 0xfffd mask");
  piece.set_piece_collision(0, false);
  check(piece.data().elements[0].flags == 6,
        "disabling piece collision sets flag 2 after exact mask");
  check(before.data.elements[0].flags == 0xffff0006,
        "piece flag snapshot retains the earlier full unsigned32 value");

  W::Limits cap;
  cap.query_work = 3;
  W::Level slide_level(flat(), cap), ground_level(flat(), cap),
      combined(flat(), cap);
  // A clear sweep visits one cell and reference. Grounding visits one cell,
  // reference and sample. Each fits alone; their composition must share its
  // cap.
  const auto slide =
      W::world_slide_move(slide_level, f(4), {0, 0, 0}, {R::ONE, 0, 0}, 1);
  check(slide.hit == 0 && slide.position == W::Vec3{R::ONE, 0, 0} &&
            !slide.normal,
        "standalone clear slide fits the three-visit budget");
  const auto ground = W::ground_step(ground_level, f(4), {0, 0, 0}, 0);
  check(ground.landed == 1 && ground.position == W::Vec3{0, 0, 0} &&
            ground.vz == 0,
        "standalone grounding fits the three-visit budget");
  rejects<W::ResourceLimit>(
      [&] {
        W::object_move_isolated(combined, f(4), {0, 0, 0}, {f(60), 0, 0}, 0, 0);
      },
      "composed movement cannot reset the budget between slide and ground");
  check(combined.data().stamp == 2 && combined.data().elements[0].stamp == 1,
        "nested exhaustion retains ground stamp but refuses before its "
        "reference");
}
void bounds() {
  for (unsigned which = 0; which < 8; ++which) {
    auto d = flat();
    if (which == 0)
      d.tg_w = -1;
    if (which == 1)
      d.grid.clear();
    if (which == 2)
      d.heightfields[0].samples.clear();
    if (which == 3)
      d.elements[0].hf = 1;
    if (which == 4)
      d.grid[0] = {1};
    if (which == 5)
      d.pieces = {2};
    if (which == 6)
      d.tg_x0 = INT64_MAX;
    if (which == 7) {
      d.pg_w = 2;
      d.pg_h = 2;
      d.path_cells = {1};
    }
    rejects([&] { W::Level bad(d); },
            "malformed topology/metadata refuses at owned boundary");
  }
  auto d = flat();
  W::Limits small;
  small.samples = 0;
  rejects<W::ResourceLimit>([&] { W::Level bad(d, small); },
                            "sample storage cap refuses construction");
  d.stamp = 5;
  d.elements.push_back(d.elements[0]);
  d.grid = {{0, 1}};
  small = W::Limits{};
  small.query_work = 2;
  W::Level limited(d, small);
  rejects<W::ResourceLimit>(
      [&] { W::collect_elements(limited, 0, 0, 0, 0, 64); },
      "query work refuses before unbounded reference visits");
  check(limited.data().elements[0].stamp == 5 &&
            limited.data().elements[1].stamp == 0 && limited.data().stamp == 5,
        "budget refusal preserves exact previously visited stamp prefix");
  auto snap = limited.snapshot();
  check(snap.limits.query_work == 2 && snap.data.grid == d.grid,
        "failure snapshot preserves limits and complete topology");
  W::Level piece(flat());
  rejects([&] { piece.set_piece_height(0, R::ONE); },
          "out-of-range piece refuses without mutation");
  check(piece.data().elements[0].z == 0,
        "bad piece leaves unrelated element unchanged");
}
void sight_rays() {
  // Authored analogue of ref_world.py's room: the heightfield has a wall
  // from sample x=10. No disc data or arena level geometry is embedded.
  W::LevelData d;
  d.tg_x0 = d.tg_y0 = -1;
  d.tg_w = d.tg_h = 2;
  d.grid = {{0}, {0}, {0}, {0}};
  W::HeightField h;
  h.x0 = h.y0 = -20;
  h.w = h.h = 40;
  h.lo = 0;
  h.hi = 40;
  h.samples.reserve(1600);
  for (int y = -20; y < 20; ++y)
    for (int x = -20; x < 20; ++x)
      h.samples.push_back(x >= 10 ? 640 : (y == -10 ? 2 : 0));
  d.heightfields.push_back(std::move(h));
  d.elements.push_back(W::make_element(0, -88, -88, 84, 84, 0, 4,
                                       0, 0, 0, 32767, 0));
  W::Level level(d);
  check(W::terrain_raycast(level, R::ONE, {0, 0, f(5)}, {f(60), 0, 0}, 0) ==
            0x3f3bbbbbu,
        "disc-style sight ray hits the synthetic wall at the reference parameter");
  check(W::terrain_raycast(level, R::ONE, {0, 0, f(5)}, {f(30), 0, 0}, 0) ==
            0xbf800000u,
        "sight ray with insufficient reach returns the clear-line answer");
  check(W::terrain_raycast(level, R::ONE, {f(2), f(2), f(30)},
                           {0, 0, f(-1)}, 0) == f(30),
        "vertical downward ray uses the documented distance branch");
  check(W::terrain_raycast(level, R::ONE, {0, 0, f(5)},
                           {0, f(-60), 0}, 0) == 0xbf800000u &&
            W::terrain_raycast(level, R::ONE, {0, 0, f(5)},
                               {0, f(-60), 0}, 3) != 0xbf800000u,
        "ray flags promote the marked sample only when requested");
  // Independent ref_world.py results on this authored room, away from its
  // unused negative-X ledge. These include angled and sloped sight lines.
  struct RayCase { W::Vec3 origin, direction; R::Raw flags, expected; };
  const RayCase cases[] = {
      {{0, 0, f(5)}, {f(80), 0, 0}, 0, 0x3f0cccccu},
      {{f(10), 0, f(5)}, {f(50), 0, 0}, 0, 0x3f23d70au},
      {{0, 0, f(20)}, {f(100), 0, 0}, 0, 0x3ee147adu},
      {{0, 0, f(50)}, {f(100), 0, 0}, 0, 0xbf800000u},
      {{0, 0, f(5)}, {f(60), f(10), 0}, 0, 0x3f392dd3u},
      {{0, 0, f(5)}, {f(60), f(-10), 0}, 0, 0x3f392dd3u},
      {{0, 0, f(5)}, {f(60), 0, f(20)}, 0, 0x3f3bbbbbu},
      {{0, 0, f(5)}, {f(60), 0, f(-20)}, 0, 0x3e888888u},
      {{0, 0, f(5)}, {0, f(-60), 0}, 3, 0x3f3bbbbbu},
  };
  for (const auto &ray : cases)
    check(W::terrain_raycast(level, R::ONE, ray.origin, ray.direction,
                             ray.flags) == ray.expected,
          "terrain ray matches the independent synthetic reference bitwise");
}
} // namespace
int main() {
  try {
    collection();
    heights_and_pieces();
    movement();
    raw_rotation();
    empty_ranges_and_signed_masks();
    piece_mask_and_nested_budget();
    bounds();
    sight_rays();
    std::cout << checks << " world query checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "world query check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
