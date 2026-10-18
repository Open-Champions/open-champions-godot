#include "champions/creature_anim/dispatch.hpp"
#include "creature_dispatch_fixture.hpp"
#include <iostream>
#include <sstream>
using dispatch_fixture::clip_bytes;
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::creature_anim;
namespace {

void selection(const Selection &s) {
  std::cout << s.stored << ' ' << s.ops.size() << ' ';
  for (const auto &o : s.ops) {
    std::cout << o.kind << ' ' << o.args.size() << ' ';
    for (auto a : o.args)
      std::cout << a << ' ';
  }
}
void state(Runtime &r, StateId id) {
  const auto &s = r.state(id);
  std::cout << s.count << ' ' << s.flags << ' ' << r.cosmetic_seed() << ' ';
  for (auto id : s.tracks) {
    const auto &t = r.track(id);
    for (auto v :
         {std::uint64_t(t.clip), std::uint64_t(t.weight), std::uint64_t(t.rate),
          std::uint64_t(t.life), std::uint64_t(t.life_rate),
          std::uint64_t(t.time), std::uint64_t(t.flags)})
      std::cout << v << ' ';
  }
}
} // namespace
int main() {
  std::string line;
  while (std::getline(std::cin, line))
    try {
      std::istringstream in(line);
      std::string op;
      in >> op;
      if (op == "combo") {
        Raw m, c, v, f;
        in >> m >> c >> v >> f;
        std::cout << player_combo_variant(m, c, v, f) << '\n';
        continue;
      }
      Runtime r(8, {}, 3);
      auto st = r.add_state();
      std::vector<ClipId> clips;
      unsigned length = op == "events" ? 180 : 120;
      for (unsigned i = 0; i < 112; ++i)
        clips.push_back(r.add_clip(clip_bytes(length)));
      DispatchContext ctx{r, st, [&](Raw i) { return clips.at(i); },
                          [](const AnimOp &) {}};
      r.set_sim_frame(17);
      if (op == "select") {
        std::string name;
        Raw s, rand, k, field, value;
        in >> name >> s >> rand >> k >> field >> value;
        Choice c;
        c.random = rand;
        if (k)
          c.condition = Condition{k == 2, field, value};
        auto selected = select_dispatch(name, s, c);
        r.anim_push(st, clips[0], 1);
        apply_dispatch(ctx, selected);
        selection(selected);
        state(r, st);
      } else if (op == "events") {
        std::string name;
        Raw entry;
        in >> name >> entry;
        install_dispatch_events(ctx, name);
        r.anim_push(st, clips.at(entry), 1);
        for (unsigned i = 0; i < 185; ++i)
          std::cout << r.advance(ONE, st, false).events << ' ';
      } else if (op == "player") {
        std::string a;
        PlayerInput p;
        in >> a >> p.weapon_class >> p.model >> p.variant >> p.relative_hit >>
            p.draw >> p.no_combat_level;
        std::string name;
        auto s = player_action(a, p, [&](const std::string &n) {
          name = n;
          return clips[100];
        });
        selection(s);
        std::cout << (name.empty() ? "-" : name) << ' ';
        r.anim_push(st, clips[0], 1);
        apply_dispatch(ctx, s);
        state(r, st);
      } else if (op == "move") {
        unsigned mode, bias, divn, count;
        in >> mode >> bias >> divn >> count;
        set_fpu(static_cast<Fpu::Model>(mode), bias, divn);
        reset_player_idle(ctx, {}, {});
        Raw b = 0, prev = 0;
        for (unsigned i = 0; i < count; ++i) {
          Raw speed;
          unsigned movement;
          in >> speed >> movement;
          auto v = player_locomotion(ctx, speed, b, prev, movement);
          b = v.blend;
          prev = v.previous;
          std::cout << v.speed << ' ' << b << ' ' << prev << ' '
                    << v.motion.events << ' ';
          state(r, st);
        }
        set_fpu();
      } else
        throw ValueError("unknown trace operation");
      std::cout << '\n';
    } catch (const std::exception &e) {
      std::cout << "REFUSED " << e.what() << '\n';
    }
}
