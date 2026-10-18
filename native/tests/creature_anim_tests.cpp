#include "champions/creature_anim/creature.hpp"
#include <iostream>
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::creature_anim;
namespace {
unsigned checks = 0;
void check(bool b, const char *why) {
  ++checks;
  if (!b)
    throw std::runtime_error(why);
}
template <class E, class F> void refuses(F f, const char *why) {
  bool caught = false;
  try {
    f();
  } catch (const E &) {
    caught = true;
  }
  check(caught, why);
}
// Authored one-joint clip, identity rotation, x=1; no game bytes.
std::vector<std::uint8_t> clip_bytes() {
  std::vector<std::uint8_t> out(0x80, 0);
  auto word = [&](std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
      out[at + i] = std::uint8_t(v >> (8 * i));
  };
  word(0, 1);
  word(4, 20);
  word(8, 0x80);
  word(12, 0x78);
  word(16, 0x70);
  std::vector<unsigned> bits;
  auto put = [&](std::uint32_t v, unsigned n) {
    for (unsigned i = n; i > 0; --i)
      bits.push_back((v >> (i - 1)) & 1);
  };
  put(7, 4);
  put(64, 8);
  put(0, 8);
  put(0, 8);
  put(13, 4);
  put(4096, 14);
  put(0, 14);
  put(0, 14);
  put(0, 14);
  put(255, 8);
  while (bits.size() % 32)
    bits.push_back(0);
  for (std::size_t i = 0; i < bits.size(); i += 16) {
    std::uint16_t v = 0;
    for (unsigned j = 0; j < 16; ++j)
      v = std::uint16_t((v << 1) | bits[i + j]);
    out.push_back(v & 255);
    out.push_back(v >> 8);
  }
  return out;
}
void resources() {
  Memory m;
  m.poke(0x1024, 4, 0x2000);
  m.poke(0x200c, 4, 0x3000);
  m.poke(0x2014, 4, 0x4000);
  m.poke(0x1048, 4, 0x5000);
  m.poke(0x5000 + 27 * 8, 2, 0xfff0);
  m.poke(0x5004 + 27 * 8, 4, 0x6000);
  m.poke(0x2000, 4, 0x7000);
  check(class_resource(m, 0x1000, 1) == 0x3000 &&
            class_resource(m, 0x1000, 2) == 0x4000,
        "class resources");
  check(virtual_target(m, 0x1000, 27) == std::make_pair(0x6000u, 0xff0u),
        "signed this adjustment");
  check(role_lump(m, 0x1000, true) == 0x13c8 &&
            role_lump(m, 0x1000, false) == 0x7000,
        "embedded NPC lump");
  std::vector<std::string> log;
  auto prop = [&](const std::string &k) -> std::optional<std::string> {
    log.push_back(k);
    return k == "idlename" ? "missing" : "actor";
  };
  auto find = [&](Raw, const std::string &s) {
    log.push_back(s);
    return 0u;
  };
  check(find_role_animation(7, "idle", prop, find) == 0 &&
            log ==
                std::vector<std::string>{"idlename", "idlename", "missing.anm"},
        "override null has no fallback");
  log.clear();
  find_role_animation(
      7, "idle", [](auto) -> std::optional<std::string> { return ""; }, find);
  check(log == std::vector<std::string>{".anm"}, "empty override present");
  refuses<ValueError>(
      [&] { find_role_animation(0, std::string(156, 'a'), prop, find); },
      "scratch bound");
  m.poke(0x201c, 4, 0x3000);
  set_class_events(m, 0x1000, 3, {0x3e800000, HALF, 0x3f400000, ONE},
                   {1, 2, 4, 0x108});
  check(m.log().size() == 8 && m.log()[0].address == 0x3018 &&
            m.log()[1].address == 0x3014 && m.log()[6].value == 8,
        "ordered shared event writes");
  for (auto off : {0x10, 0x14, 0x18, 0x3c, 0xdc, 0xe0})
    m.poke(0x1000 + off, 4,
           from_int(off == 0x10   ? 10
                    : off == 0x14 ? 20
                    : off == 0x18 ? 999
                    : off == 0x3c ? 30
                    : off == 0xdc ? 2
                                  : 3));
  auto mat = draw_matrix(m, 0x1000);
  check(mat[9] == from_int(13) && mat[10] == from_int(20) &&
            mat[11] == from_int(30),
        "render height and facing offset");
  check(mat34_transform_point(mat, {ONE, 0, 0}) ==
            Vec3{0x414fffff, 0x41b00000, 0x41f00000},
        "raw32 draw transform reference vector");
}
void facing_width() {
  Runtime runtime(2, {}, 2);
  auto state = runtime.add_state();
  auto a = runtime.creature_step(ONE, state, INT32_MIN, ONE, 0, false,
                                 {0, ONE, 0, 0});
  auto b = runtime.creature_step(ONE, state, 0, ONE, 0, false, {0, ONE, 0, 0});
  check(a.x == b.x && a.y == b.y,
        "creature facing narrows to original signed16 before arithmetic");
}
void skin() {
  Pose p;
  p.joint_count = 4;
  for (unsigned j = 0; j < 4; ++j)
    for (unsigned r = 0; r < 3; ++r) {
      p.mats[j][r][r] = 4096;
      p.mats[j][r][3] = r ? 0 : j * 64;
    }
  Vertex v{{16, 0, 0}, {0, 0, 127}, {{0, 64}, {1, 64}, {2, 64}, {3, 63}}};
  check(skin_point(p, v)[0] == 636 * (POSITION_DENOMINATOR / 255),
        "four-weight exact rational vector");
  check(skin_normal(p, v)[2] == NORMAL_DENOMINATOR,
        "normal rotation no translation");
  p.joint_count = 65;
  refuses<ValueError>([&] { skin_point(p, v); }, "invalid pose count");
  p.joint_count = 4;
  v.influences = {{0, 254}};
  refuses<ValueError>([&] { skin_point(p, v); }, "invalid weight sum");
  v.influences = {{4, 255}};
  refuses<ValueError>([&] { skin_point(p, v); }, "joint out of range");
}
void shared_resources() {
  auto env = std::make_shared<Environment>(8, 4);
  auto id = env->register_clip(0x6000, clip_bytes());
  auto again = env->register_clip(0x6000, clip_bytes());
  check(id == again && env->runtime.snapshot().decoder.clips.size() == 1,
        "one shared resource identity");
  env->memory.poke(0x6018, 1, 7);
  check(env->register_clip(0x6000, clip_bytes()) == id &&
            env->memory.load(0x6018, 1) == 7,
        "registration does not reset shared events");
  auto bad = clip_bytes();
  bad[4] = 21;
  refuses<Unsupported>([&] { env->register_clip(0x6000, bad); },
                       "same resource cannot change bytes");
  Creature a(env, 0x1000), b(env, 0x2000);
  check(&a.runtime == &b.runtime && &a.memory == &b.memory,
        "actors share world memory and clip registry");
  refuses<ValueError>([&] { Creature duplicate(env, 0x1000); },
                      "duplicate live object identity");
  Pairing p{0x5000, 0x9000, 0xa000, id, {{0x6000, id}}};
  for (auto c : {0x1000u, 0x2000u}) {
    env->memory.poke(c + 0x24, 4, 0x5000);
    env->memory.poke(0x500c, 4, 0x9000);
    env->memory.poke(0x5014, 4, 0xa000);
    env->memory.poke(0x503c, 4, 0x6000);
  }
  a.bind(p, {});
  b.bind(p, {});
  check(env->memory.load(0x6018, 1) == 7,
        "second actor binding retains class events");
  a.model.callbacks["handle_clear"] = b.model.callbacks["handle_clear"] =
      [](auto &, const auto &) { return 0u; };
  a.change_state(0);
  b.change_state(0);
  check(a.runtime.track(a.runtime.state(a.state_id()).tracks[0]).clip ==
            b.runtime.track(b.runtime.state(b.state_id()).tracks[0]).clip,
        "class dispatch uses same clip across actors");
}
void dispatch() {
  Creature c(0x1000, 8, 4);
  auto &m = c.memory;
  constexpr Raw D = 0x502818, T = 0x50fd40;
  Pairing p;
  p.definition = D;
  p.model = 0x9000;
  p.texture = 0xa000;
  for (unsigned i = 0; i < 6; ++i) {
    auto id = c.runtime.add_clip(clip_bytes());
    p.clips[0x6000 + 0x100 * i] = id;
    if (!i)
      p.base = id;
  }
  m.poke(0x1024, 4, D);
  m.poke(D + 12, 4, p.model);
  m.poke(D + 20, 4, p.texture);
  m.poke(D + 0x3c, 4, 0x6000);
  m.poke(D + 0x4c, 4, 0x6100);
  for (unsigned i = 0; i < 4; ++i)
    m.poke(D + 0x1c + 8 * i, 4, 0x6200 + 0x100 * i);
  m.poke(T + 27 * 8 + 4, 4, 0x2ab704);
  m.poke(T + 28 * 8 + 4, 4, 0x10beb8);
  m.poke(0x1048, 4, T);
  m.poke(0x1300, 4, 100);
  c.bind(p, {{{{16, 0, 0}, {0, 0, 127}, {{0, 255}}}}});
  refuses<creature_hp::MissingHandler>([&] { c.change_state(0); },
                                       "missing game service refuses");
  c.model.callbacks["handle_clear"] = [](auto &, const auto &) { return 0u; };
  c.change_state(0);
  auto &state = c.runtime.state(c.state_id());
  check(state.count == 1 && c.runtime.track(state.tracks[0]).clip == p.base,
        "SmallSpider idle dispatch reaches runtime");
  check(m.load(0x10f4, 4) == 220 + ((0x3c6ef35f >> 16) % 300),
        "own RNG and idle timer exact vector");
  // Explicit reference-zero engine services only in this synthetic harness.
  c.model.policy = creature_hp::CallbackPolicy::ReferenceZero;
  c.change_state(1);
  check(m.load(0x1232, 2) == 1 &&
            c.runtime.track(state.tracks[state.count - 1]).clip ==
                p.clips[0x6100],
        "wander clip");
  for (auto ignored : {5u, 16u, 18u, 19u}) {
    auto count = state.count;
    c.change_state(ignored);
    check(m.load(0x1232, 2) == 1 && state.count == count,
          "ignored state preserves dispatch");
  }
  m.poke(0x100c, 4, 2);
  c.change_state(20);
  check((m.load(0x100c, 4) & 9) == 9 && m.load(0x1232, 2) == 20,
        "death flags/state");
  const auto &death = c.runtime.track(state.tracks[state.count - 1]);
  check(death.flags == 6 && death.time == from_int(20),
        "hidden death jumps selected clip end");
  auto frame = c.draw(10);
  check(frame.model == 0x9000 && frame.positions.size() == 1,
        "paired pose skins mesh");
  m.poke(0x1024, 4, 0);
  refuses<Unsupported>([&] { c.draw(11); }, "changed provenance refused");
  m.poke(0x1024, 4, D);
  auto mismatch = p;
  c.runtime.clip(mismatch.base).depth[0] = 1;
  refuses<Unsupported>([&] { c.bind(mismatch, {}); },
                       "incompatible skeleton refused");
  c.runtime.clip(mismatch.base).depth[0] = 0;
  // Constructor uses cosmetic seed, not per-object random state.
  Creature init(0x1000, 8, 4);
  for (unsigned i = 0; i < 6; ++i)
    init.runtime.add_clip(clip_bytes());
  for (auto [a, b] : m.bytes())
    init.memory.poke(a, 1, b);
  init.bind(p, {{{{16, 0, 0}, {0, 0, 127}, {{0, 255}}}}});
  init.model.policy = creature_hp::CallbackPolicy::ReferenceZero;
  init.memory.poke(0x1044, 4, 0x12345678);
  init.memory.poke(0x404460, 4, 0);
  init.construct(0, {0, 0, 0}, 0, 0);
  check(init.memory.load(0x1044, 4) == 0x12345678 &&
            init.memory.load(0x404460, 4) == 0x3c6ef35f,
        "constructor cosmetic RNG authority");
  check(init.runtime.state(init.state_id()).count == 1,
        "constructor pushes idle once");
  init.memory.poke(0x13c4, 2, 600);
  init.memory.poke(0x403a08, 1, 1);
  init.memory.poke(0x1228, 4, from_int(100));
  init.memory.poke(0x1232, 2, 2);
  const auto time = init.runtime.track(1).time;
  auto out = init.tick(1, 10);
  check(init.runtime.track(1).time != time && out.positions.size() == 1,
        "active original run returns skinned tick");
  refuses<ValueError>([&] { init.tick(1, 11); },
                      "same simulation tick not advanced twice");
  auto after = init.runtime.track(1).time;
  init.draw(12);
  check(init.runtime.track(1).time == after, "render does not advance clock");
  init.memory.poke(0x403a08, 1, 0);
  init.tick(2, 13);
  check(init.runtime.track(1).time == after,
        "inactive run leaves clip time unchanged");
  init.memory.poke(0x800c, 4, 0x40);
  init.touch(0x8000);
  check(init.memory.load(0x1232, 2) == 20, "player touch dispatches death");
}
} // namespace
int main() {
  try {
    resources();
    facing_width();
    skin();
    shared_resources();
    dispatch();
    std::cout << checks << " creature animation API checks pass\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
