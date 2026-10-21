#include "champions/creature_anim/dispatch.hpp"
#include "creature_dispatch_fixture.hpp"
#include <iostream>
#include <map>
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
      if (op == "agony") {
        Raw flags;
        in >> flags;
        std::cout << agony_hit_entry(flags) << '\n';
        continue;
      }
      if (op == "clockwork") {
        Raw form;
        in >> form;
        auto e = clockwork_idle_entry(form);
        std::cout << (e ? std::to_string(*e) : std::string("none")) << '\n';
        continue;
      }
      if (op == "combined") {
        std::string name;
        Raw s, f0, v0, f1, v1;
        in >> name >> s >> f0 >> v0 >> f1 >> v1;
        auto d = combined_dispatch(name, s, {{f0, v0}, {f1, v1}});
        std::cout << d.stored << ' ' << d.ops.size() << ' ';
        for (const auto &o : d.ops) {
          std::cout << o.kind << ' ' << o.args.size() << ' ';
          for (auto a : o.args)
            std::cout << a << ' ';
        }
        std::cout << '\n';
        continue;
      }
      if (op == "skill") {
        // Skill-driven states (ref_skill_anim): each looked-up name gets its
        // own clip; a named op prints the name of the clip it starts. The
        // looked-up names print sorted (their order is not compared).
        std::string a;
        PlayerInput p;
        in >> a >> p.weapon_class >> p.object_flags;
        Runtime rr(8, {}, 3);
        std::map<std::string, ClipId> named;
        auto sel = player_action(a, p, [&](const std::string &n) {
          auto it = named.find(n);
          if (it == named.end())
            it = named.emplace(n, rr.add_clip(clip_bytes(120))).first;
          return it->second;
        });
        std::cout << sel.stored << ' '
                  << (sel.object_flags ? std::to_string(*sel.object_flags) : std::string("-"))
                  << ' ' << sel.ops.size() << ' ';
        for (const auto &o : sel.ops) {
          std::cout << o.kind << ' ' << o.args.size() << ' ';
          for (std::size_t k = 0; k < o.args.size(); ++k) {
            std::string clip;
            if (k == 0 && o.kind.rfind("named_", 0) == 0)
              for (const auto &[n, id] : named)
                if (std::int64_t(id) == o.args[0])
                  clip = n;
            if (!clip.empty())
              std::cout << clip << ' ';
            else
              std::cout << o.args[k] << ' ';
          }
        }
        std::cout << named.size() << ' ';
        for (const auto &[n, id] : named)
          std::cout << n << ' ';
        std::cout << '\n';
        continue;
      }
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
